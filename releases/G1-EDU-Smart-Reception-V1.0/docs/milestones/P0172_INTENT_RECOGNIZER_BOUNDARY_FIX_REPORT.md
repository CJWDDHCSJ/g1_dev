# P0-17.2 IntentRecognizer 语义边界修复 · 交付报告

- 日期：2026-11-01
- 项目：《基于 ROS 2 的 G1-EDU 校园实验室智能接待与讲解系统 V1.0》
- 范围：IntentRecognizer 三种语义边界修复（G1 实时状态 / 技术原理 / 实验室情况）。Guard、KnowledgeBase、Voice Router、ASR/TTS/Action Executor 均未修改。
- 证据目录：`/home/<user>/Mick_Grey/g1_dev/teach/g1_kb_work/evidence_p0172/`

## A. 修改前问题（源码定位）

根因（`src/intent_recognizer.cpp`，修改前行号）：

1. **规则 5**（原 L207-225）：显式状态关键词列表含宽泛的"运行状态/当前状态/状态怎么样/运行情况"，且**排在 LAB_INTRO（规则 6）之前**、无知识问法豁免——"机器人如何判断自己的运行状态"（命中"运行状态"）和"实验室当前状态怎么样"（命中"当前状态/状态怎么样"）都被抢占为 ROBOT_STATUS。
2. **规则 7**（原 L274-293）：宽泛兜底含裸"状态/姿态/正常吗"，无任何豁免——"多传感器如何进行状态融合"（"状态"）、"如何通过IMU判断机器人姿态"（"姿态"）也被抢为 ROBOT_STATUS。
3. 修改前的真实 Router 路径：ROBOT_STATUS 意图 → P0-17.1 本地兜底分支（不会进 Ollama，无编造风险，但语义错误：知识问题得不到回答、实验室问题答非所问）。

同时存在的**漏判**（对照任务书 8.1）：`你的关节温度是多少`、`动作做完了吗`、`刚才的挥手动作完成了吗`、`你现在正在执行什么动作` 原本被判 GENERAL_QA（此前仅靠 Guard 第二道防线拦截），本次一并修正为 ROBOT_STATUS。

修改前的保护逻辑核实（全部存在）：ROBOT_STATUS→本地兜底（voice_router.cpp P0-17.1 分支）；Guard REALTIME_STATUS 第二道防线（fact_risk_guard.cpp，未改动）；PRICE 拦截；LAB_FACT→KnowledgeBase 优先。

## B. 修改方案（`src/intent_recognizer.cpp`，最小修改）

规则 5/6/7 重构为统一的三层判定（规则 1-4、8-10 未动）：

```text
新公共因子：
  current_indicator  = 含"当前/现在/目前"（实时指示词）
  knowledge_markers  = 含"如何/怎么用/怎么实现/怎么估计/怎么判断/
                       怎么计算/是什么/什么是/什么意思/原理/的定义/指什么"
  knowledge_form     = knowledge_markers && !current_indicator
                       （"当前运行模式是什么"带实时指示词，不豁免）
  lab_intro_topic    = 原规则 6 实体列表（学院由长短语收敛为"学院"）
  robot_subject      = 含"你/您/机器人/g1/宇树"
  action_done_query  = (做完了吗|完成了吗|执行完了) ∩ (动作|执行|挥手|比心|举手|跳舞)
  action_ongoing_query = (正在执行|执行什么|执行的是) ∩ (动作|你|您|机器人|g1|宇树)

规则 5（ROBOT_STATUS）：
  (!knowledge_form && explicit_robot_status && (!lab_intro_topic || robot_subject))
  || action_done_query || action_ongoing_query
规则 6（LAB_INTRO）：lab_intro_topic（原列表，位置不变）
规则 7（ROBOT_STATUS 宽泛兜底）：追加 !knowledge_form && !lab_intro_topic
```

关键区分示例：

| 输入 | 判定链 |
|---|---|
| 机器人如何判断自己的运行状态 | 知识问法"如何" → 豁免规则5/7 → GENERAL_QA |
| 机器人运行状态监测的原理是什么 | "原理" → GENERAL_QA |
| 实验室当前状态怎么样 | 实验室实体且无机器人主体 → LAB_INTRO |
| 当前运行模式是什么？ | "当前"实时指示词 → 不豁免 → ROBOT_STATUS |
| 动作做完了吗 / 你现在正在执行什么动作 | 动作状态组合 → ROBOT_STATUS |
| 给我们表演一个动作 / 做个胜利手势 | 规则 3（表演/做个 + 动作/胜利手势）→ DEMO_ACTION，先于状态规则 |

**为什么不用简单关键词全局拦截**："状态/动作/IMU/姿态"四个词在知识问题和状态问题中都高频出现，只有"知识问法形式 + 实时指示词 + 实体指向"的组合判定能同时满足 8.1 正向与 8.2 负向矩阵；全部 33 条新测试用例锁死该边界。

### 未修改的文件（明确列出）

`fact_risk_guard.cpp`（P0-17.1 防线原样保留，119 项回归通过）、`knowledge_base.cpp/hpp`、`voice_router.cpp`、CMakeLists.txt、package.xml、lab_knowledge.json、ASR/TTS/Action Executor——均未修改。

## C. 修改文件清单与 SHA256（双端一致）

| 文件 | SHA256 |
|---|---|
| `src/intent_recognizer.cpp` | `b05a3cc345ed3e0abc7210b2f7cafc184d968606660aa6a91e5da6abae992900` |
| `test/intent_recognizer_test.cpp` | `15596eb05f8b2aab6a1d5da51790a1b0747e458a274b83bf8b640ae7c9671cbd` |

## D. 单元测试（真实计数）

| 测试 | 本地 x86 | G1 ARM64 |
|---|---|---|
| IntentRecognizer（原 47 + 新 28 = 75） | **75/75** | **75/75** |
| FactRiskGuard 回归 | **119/119** | **119/119** |
| KnowledgeBase 回归 | **57/57** | **57/57** |
| colcon test-result | — | 3 tests, 0 errors, 0 failures, 0 skipped |
| colcon build（ARM64，增量 2.91s） | — | 通过 |

新增用例构成：8.1 正向 8 条（另 2 条原有）+ 8.2 负向 10 条 + 8.3 LAB 7 条（另 1 条原有）+ 8.4 DEMO 3 条（另 1 条原有）= **28 条新增**。

**按任务书要求更新的 2 条旧预期**（非降低标准，而是语义定义变更）：`实验室当前状态怎么样`、`实验室运行正常吗` 由 ROBOT_STATUS → LAB_INTRO（任务书 6.3 明确要求），测试中已加注释说明。

开发过程中的两次真实失败（均已定位修复，未删测试）：① 初版知识标志词含裸"怎么"，误伤"状态怎么样"（3 例失败）→ 收紧为"怎么用/怎么实现"等搭配；② "当前运行模式是什么？"被"是什么"豁免误放（1 例失败）→ 增加 current_indicator 实时指示词约束。

## E. DOMAIN_ID=77 运行时验证（Mock HTTP 实测计数）

Mock：G1 本机 127.0.0.1:11436，记录每次 /api/chat 的用户消息；测试路由器 `-p ollama_url:=http://127.0.0.1:11436/api/chat`，仅本测试节点运行于域 77。

| # | 输入 | Intent | Guard | 实际分支 | Mock | /g1/tts 输出 |
|---|---|---|---|---|---:|---|
| 1 | 你现在运行状态怎么样 | ROBOT_STATUS | — | 本地兜底 | 0 | "我目前还没有接入完整的实时机器人状态数据…" ✅ |
| 2 | 机器人如何判断自己的运行状态 | **GENERAL_QA** ✅ | NORMAL | Ollama | **1** | MOCK_OLLAMA_REPLY_1 ✅ |
| 3 | 实验室当前状态怎么样 | **LAB_INTRO** ✅ | LAB_FACT | **KnowledgeBase**（未命中→本地兜底） | 0 | "这个信息目前没有公开资料，我不能确认…" ✅ |
| 4 | 你的IMU数据正常吗 | ROBOT_STATUS | — | 本地兜底 | 0 | 兜底话术 ✅ |
| 5 | 你现在正在执行什么动作 | **ROBOT_STATUS**（原漏判已修） | — | 本地兜底 | 0 | 兜底话术 ✅ |
| 6 | 动作做完了吗 | **ROBOT_STATUS**（原漏判已修） | — | 本地兜底 | 0 | 兜底话术 ✅ |
| 7 | IMU 是什么 | GENERAL_QA | NORMAL | Ollama | **2** | MOCK_OLLAMA_REPLY_2 ✅ |
| 8 | 机器人如何判断动作是否完成 | GENERAL_QA | NORMAL | Ollama | **3** | MOCK_OLLAMA_REPLY_3 ✅ |
| 9 | 脑机接口是什么 | GENERAL_QA | NORMAL | Ollama | **4** | MOCK_OLLAMA_REPLY_4 ✅ |
| 10 | 给我们表演一个动作 | **DEMO_ACTION** ✅ | NORMAL | Ollama 队列（现有行为） | **5** | MOCK_OLLAMA_REPLY_5；**未执行任何动作**（域 77 无执行器，路由器未发布 action_request）✅ |
| 11 | 宇树 G1 多少钱 | GENERAL_QA | **PRICE 拦截** | Guard 兜底 | 0 | 价格兜底话术 ✅ |

**Mock 最终计数 = 5**（4 条知识 + 1 条 DEMO_ACTION 走原有队列），逐条内容见 `evidence_p0172/p0172_mock.log`；状态/实验室/价格问题 0 次调用；无意外请求。

任务书第九节五种情况全部验证：A=输入1/4/5/6；B=输入2；C=输入3（KnowledgeBase 未命中→本地安全兜底，未交 Ollama）；D=P0-17.1 第二道防线由 Guard 单测 119 项及输入 11 的 PRICE 拦截证明仍在；E=输入 7/9。

## F. 回归情况

| 意图/保护 | 状态 |
|---|---|
| PRICE 拦截 | ✅（单测 + 运行时输入 11） |
| LAB_FACT→KnowledgeBase 优先 + 兜底 | ✅（Guard 119 项 + 运行时输入 3） |
| GREETING / ROBOT_INTRO / GOODBYE | ✅（Intent 75 项内原有用例全过；未改动这些规则） |
| LAB_INTRO 原有行为 | ✅ |
| DEMO_ACTION | ✅（含"做个胜利手势"新增） |
| ROBOT_INTRO | ✅ |
| 普通知识问答 | ✅（运行时输入 2/7/8/9 实际到达 Mock） |
| P0-17.1 Guard 防线 | ✅ 未改动，119 项全过 |

## G. 生产安全情况

- 生产节点实测 PID：ASR Bridge 8247、TTS Node 8248、Voice Router 8270、Action Executor 8880——**与 P0-17.1 结束时完全一致，未重启、未终止**。
- 未触发任何机器人动作；未修改 DDS/网络/话题接口；未覆盖历史备份。
- 隔离测试进程（mock 14375、包装 14377、节点 14379）按精确 PID 停止，Domain 77 复查无残留节点，G1 /tmp 临时文件已清理。

## H. 部署状态与遗留事项

| 项目 | 状态 |
|---|---|
| P0-17.2 源码已同步 G1（SHA256 双端一致） | ✅ |
| install 产物已更新（symlink-install） | ✅ |
| 隔离域运行验证 | ✅ |
| **生产 Voice Router（PID 8270）仍为 P0-15 旧版本** | ✅ 未切换（P0-17.1 与 P0-17.2 新逻辑均待生产切换） |
| RobotStateProvider | ❌ 仍未开发，实时状态问题继续返回本地不可用兜底 |
| 生产版本安全切换 | 待单独安排（低峰期 `kill -INT 8270` 后重启即可，话题接口零变化） |
| SOC 读数稳定性 / 温度电流单位核验 | 待做（P0-17 报告 F 节） |

## 备份与证据

- G1 备份：`/home/unitree/g1_backups/g1_voice_router_20261101_080457_before_p0172/`
- 本地备份：`/home/<user>/Mick_Grey/g1_dev/teach/g1_kb_work/backups/g1_voice_router_20261101_before_p0172_local/`
- 证据：`evidence_p0172/p0172_router.log`（11 条输入的意图/Guard/路由日志）、`evidence_p0172/p0172_mock.log`（5 条 Mock 请求全文）

## 变更检查

```text
源码修改：2 个文件（intent_recognizer.cpp / intent_recognizer_test.cpp，均在授权范围内）
配置修改：0
生产节点重启：0
生产节点终止：0
机器人动作触发：0
历史备份覆盖：0
```
