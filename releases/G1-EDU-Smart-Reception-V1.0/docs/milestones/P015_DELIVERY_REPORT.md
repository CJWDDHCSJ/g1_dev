# P0-15 大模型事实可靠性保护模块 · 交付报告

- 日期：2026-11-01
- 项目：《基于 ROS 2 的 G1-EDU 校园实验室智能接待与讲解系统 V1.0》
- 范围：仅 P0-15（GENERAL_QA 事实风险保护）。未动 TTS、ASR、Action Executor、DDS、Ollama、系统配置。
- 本轮到此结束，等待下一阶段安排。

## 验证状态总览

| 级别 | 结论 |
|---|---|
| 实际编译通过 | G1 ARM64 原生 `colcon build` 通过（32.3s，无警告输出） |
| 独立测试通过 | FactRiskGuard 71/71、KnowledgeBase 57/57、IntentRecognizer 44/44（G1 直跑 + colcon 双确认；本地 x86 复现一致） |
| ROS 运行时测试通过 | `ROS_DOMAIN_ID=77` 隔离域 6 个用例全部符合预期（PRICE/REALTIME 拦截、LAB 命中/兜底、两条 NORMAL 实际调通 Ollama 并返回 /g1/tts） |
| 真实机器人尚未验证 | 真实麦克风 ASR、真实扬声器播放、与 Action Executor 联动均未验证（P0-16 范围） |

## 1. 修改文件清单

| 文件 | 性质 | G1 SHA256（改后） |
|---|---|---|
| `include/g1_voice_router/fact_risk_guard.hpp` | 新增 | `4289f4a346932200948725961c3c4683a5ca50766348bf3107f951153b4a585d` |
| `src/fact_risk_guard.cpp` | 新增 | `399762a86a3a94722d795bede0139c2d0c59a18a3faedd676b32b21257dec146` |
| `test/fact_risk_guard_test.cpp` | 新增 | `eb2546a3acc5235ad4d9743d70fd81bbe083c1c78ee9dd040b868aebb4ea0c55` |
| `src/voice_router.cpp` | 修改 | `197773acc0c9b3234dd6484a8c04bfaed84f3f696656773a4a0b7d029d74a016` |
| `CMakeLists.txt` | 修改 | `19703d2cb1cfe281859893ee96371ff71d1b4f7977382cee0fd354ac108c6aed` |
| `package.xml` | 未修改（无新依赖） | — |
| `intent_recognizer.*`、`knowledge_base.*`、`lab_knowledge.json` | 未修改（哈希与交接文档一致） | — |

本地同步副本：`/home/mick/Mick_Grey/g1_dev/teach/g1_kb_work/g1_voice_router/`（与 G1 逐字节一致）。

## 2. 备份位置

- G1：`/home/unitree/g1_backups/g1_voice_router_20261101_064239_before_p015/`（含 src、include、test、config、CMakeLists.txt、package.xml；未覆盖任何历史备份）。
- 本地证据：`/home/mick/Mick_Grey/g1_dev/teach/g1_kb_work/evidence_p015/voice_router_p015_runtime.log`（隔离域运行日志全文）。

## 3. FactRiskGuard 设计说明

- 纯 C++17 静态库 `fact_risk_guard`，仅依赖标准库和现有 `knowledge_base.hpp`，不新增任何外部依赖。
- 接口：`FactRiskGuard::evaluate(user_text, const KnowledgeBase*)` 返回 `FactRiskResult{risk, blocked, use_local_answer, reason, fallback_answer, local_answer, kb_match_type, kb_matched_id, kb_match_score}`。
- 在 `enqueue_user_text()` 中、文本进入 Ollama 队列**之前**调用；拦截/本地回答直接发布 `/g1/tts` 并 return，不进队列。
- 判定为 LAB_FACT 时才调用 `knowledge_base_.query()`（复用现有实现，未重复造轮子）；普通知识放行时零开销、不查库。
- 可靠性判定：`result.found == true` 且 `match_type` 不以 `fallback` 开头（含 `clarification`）才视为可靠本地答案；`fallback*`（含 `fallback_quantity_unverified`、`fallback_ambiguous_*` 等）一律兜底。
- 降级：知识库指针为空或未加载时，LAB_FACT 一律安全兜底。
- 归一化自带实现：只对 ASCII A–Z 小写，不触碰 UTF-8 多字节序列（规避已知 `std::tolower` 全字节转换风险）。

## 4. 风险分类规则（按判定顺序）

1. **PRICE**：出现 价格类词（多少钱/价格/报价/售价/价位/贵不贵/多少元/多少块/多少万等）→ 拦截，固定话术“抱歉，目前没有经过核实的实时价格信息，我不能随意给你报价。”
2. **REALTIME_STATUS**：传感器词（关节温度/电量/电池/电压/电流/转速/续航等）无条件命中；泛化状态词（正常吗/状态怎么样/温度等）需同时出现机器人语境词（机器人/G1/宇树/关节等）→ 拦截，“……暂时无法确认我的实时运行状态。”（防过度拦截用例：`今天天气温度怎么样` 仍为 NORMAL）
3. **LAB_FACT**：命中实验室实体词（实验室/研究中心/脑机中心/脑机交互/重点实验室/学院/两江人工智能/开放平台/重庆理工等）→ 查知识库；可靠命中用本地答案，否则“抱歉，这部分信息目前没有经过核实的公开资料，我暂时无法准确回答。”（注意：实体词不含裸“脑机”，故 `脑机接口是什么` 不受影响）
4. **NORMAL**：其余全部放行进入 Ollama。

## 5. System Prompt 修改前后对比

修改前（尾部约束）：
> 不要编造机器人实时状态；没有传感器数据时要诚实说明。

修改后（尾部约束）：
> 你绝对不能编造以下内容：产品价格、人员数量、科研成果、项目进展、设备品牌型号规格、机器人实时状态。如果不确定或没有可靠依据，必须诚实说明你无法确认，不要给出任何猜测的具体数字、名称或型号。

说明：System Prompt 仅是辅助约束；硬保证由 FactRiskGuard 在程序侧完成（本轮隔离域验证中，价格/实时状态问题根本未到达 Ollama）。

## 6. 新增测试清单（fact_risk_guard_test，71 项断言）

- PRICE ×4 输入（宇树机器人多少钱 / G1多少钱 / 宇树机器狗价格是多少 / G1 EDU现在报价多少）：risk、blocked、无本地答案、兜底话术，共 16 项。
- REALTIME ×4 输入（G1当前温度是多少 / 机器人现在正常吗 / 当前关节温度是多少 / 机器人现在电量多少）：risk、blocked、兜底话术，共 12 项；另有非机器人语境“今天天气温度怎么样”保持 NORMAL 的防过度拦截断言。
- NORMAL ×5 输入（脑机接口是什么 / 具身智能是什么 / 多模态融合是什么 / Transformer是什么 / 给我们表演一个动作）：risk、不拦截、不产生本地答案，共 15 项。
- LAB_FACT ×7 输入（带真实 lab_knowledge.json）：设备命中（faq_equipment）、负责人命中（faq_bci_leaders）断言 risk/use_local_answer/matched_id/答案内容；人数、科研成果、研究成果、设备型号、正在开展项目断言 blocked + 兜底话术。
- 降级 ×4：kb 为 nullptr / KB 未加载时 LAB_FACT 必须兜底。
- 每个用例同时验证风险类型、是否拦截、是否使用知识库（kb_matched_id/match_type）、最终回答文本。

## 7. 原有测试回归结果（G1 ARM64 直跑）

- KnowledgeBase：Passed 57 / Failed 0。
- IntentRecognizer：Passed 44 / Failed 0。
- `colcon test-result`：3 tests, 0 errors, 0 failures, 0 skipped。

## 8. G1 ARM64 编译输出（摘要）

```text
Starting >>> g1_voice_router
Finished <<< g1_voice_router [32.3s]
Summary: 1 package finished [33.0s]
Summary: 3 tests, 0 errors, 0 failures, 0 skipped
```

## 9. 隔离域（ROS_DOMAIN_ID=77）运行时结果

| 输入 | 路径 | /g1/tts 实际输出 |
|---|---|---|
| 宇树机器人多少钱 | Guard 拦截，**未查询 Ollama**（日志 WARN `blocked before Ollama queue`） | 抱歉，目前没有经过核实的实时价格信息，我不能随意给你报价。 |
| 机器人现在电量多少 | Guard 拦截，未查询 Ollama | 抱歉，我目前还没有接入可靠的自身状态读取功能，暂时无法确认我的实时运行状态。 |
| 实验室有哪些设备 | LAB_INTRO→KB 命中（exact_question, faq_equipment） | 设备介绍（含“具体品牌、型号和数量没有公开”） |
| 实验室有多少名研究人员 | LAB_INTRO→KB 兜底（fallback_quantity_unverified） | 安全兜底话术 |
| 脑机接口是什么 | Guard=NORMAL → 队列 → **实际调通 Ollama**（日志 `Querying Ollama model`，6s 返回） | 正确的科普回答 |
| 具身智能是什么 | 同上，实际调通 Ollama | 正确的科普回答 |

日志证据：`evidence_p015/voice_router_p015_runtime.log`。测试全程未启动 TTS/Action Executor，未发布动作请求，未触碰 DOMAIN_ID 0；测试节点已停止，G1 上无残留进程（已复查）。

补充说明：设备/人数两条在运行时被 IntentRecognizer 判为 LAB_INTRO、走原有本地分支；Guard 的 LAB_FACT 分支由单元测试直接覆盖（覆盖 IntentRecognizer 误判为 GENERAL_QA 时的保护场景）。

## 10. 仍然无法回答的问题（设计使然，安全兜底）

- 一切产品价格（无实时价格来源）。
- 机器人实时温度/电量/状态（状态读取业务未实现，P0-15 明确禁止本轮顺手实现）。
- 实验室人数、科研成果/研究成果、设备品牌型号数量、正在开展的具体项目（知识库无已核实资料，兜底）。
- 知识库可回答但 Guard 语义下走兜底的边缘问法（如“实验室设备是什么型号”命中歧义兜底）——这是安全行为而非缺陷。

已知待改进（下一阶段可做，不在本轮范围）：
1. `projects` 条目的 questions/keywords 未覆盖“实验室正在开展哪些项目”这类问法（当前安全兜底而非本地命中）。
2. “您好在问候匹配”与 normalize 的 UTF-8 问题沿用交接文档第 17.3 节，未修改。

## 11. 完整启动命令（生产，DOMAIN_ID 0）

```bash
ssh unitree@192.168.123.164
source ~/unitree_ros2/setup.sh
source ~/unitree_ros2/example/install/setup.bash
ros2 run g1_voice_router voice_router
```

构建与测试命令（G1）：

```bash
source ~/unitree_ros2/setup.sh
cd ~/unitree_ros2/example
colcon build --packages-select g1_voice_router
colcon test --packages-select g1_voice_router
colcon test-result --verbose
```

隔离域验证（G1，另开终端）：

```bash
export ROS_DOMAIN_ID=77
ros2 topic pub --once /g1/asr_text std_msgs/msg/String "{data: '宇树机器人多少钱'}"
ros2 topic echo /g1/tts --once
```
