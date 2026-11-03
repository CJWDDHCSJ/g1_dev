# P0-21 最终验收补充报告（ASR001 别名规则专项）

- 日期：2026-11-03
- 范围：仅验证 `脑筋接口 → 脑机接口`（ASR001）在生产环境是否生效
- 约束遵守：**未修改源码、未重新编译、未重启任何生产服务、未新增别名、未引入模糊匹配**
- 证据：`evidence_p021/supplement/`（`router_request.log`、`tts_sentences.log`、`action_sent_hist.log`、`systemd_final.txt`）

## 1. 生产状态确认（验证前）

| 服务 | PID | 状态 | NRestarts |
|---|---:|---|---:|
| g1-robot-state-provider | 1379 | active/running | 0 |
| g1-tts | 1380 | active/running | 0 |
| g1-voice-router | 11283 | active/running | 0 |
| g1-asr-bridge | 1382 | active/running | 0 |
| g1-action-executor | 1383 | active/running | 0 |

- `g1-voice-stack.target`：active
- Router 进程数：**1**（`ps` 仅 11283，无第二实例、无 `--ros-args` 测试进程）
- `/proc/11283/exe` SHA256：`6ab99de8ceb287d1fdb58b66399874c60b41734775a74a0c2745bf64223d6bb7`
- 与冻结版本比对：**完全一致**；磁盘 `build/g1_voice_router/voice_router` 同一哈希；`install/.../voice_router` 为指向 build 的符号链接

## 2. ASR001 规则存在性确认

源码（`~/unitree_ros2/example/g1_voice_router/src/asr_text_normalizer.cpp:97-102`）：

```cpp
// 2. 脑急接口 / 脑筋接口 -> 脑机接口（真实 ASR 日志证据：
//    P0-20.1 回归出现"脑急接口"，P0-21 真机演示出现"脑筋接口"）。
if (replace_all(text, "脑急接口", "脑机接口") ||
    replace_all(text, "脑筋接口", "脑机接口"))
{
    matched_rule = "ASR001_BRAIN_COMPUTER_INTERFACE";
}
```

| 检查 | 结果 |
|---|---|
| 源码 SHA256 | `b8ae1126ae7aec8de9067c605bbfd7d58ebf4a3f23dc90fcdd510ee80b6569f1`（与冻结清单一致） |
| 运行二进制含 `ASR001_BRAIN_COMPUTER_INTERFACE` | 是（`strings` 命中 1 次） |
| 运行二进制含“脑筋接口”字面量 | 是（`grep -ac` 命中 1 次） |
| 运行二进制含“脑机接口”字面量 | 是（命中 2 次） |

结论：规则确实存在于当前生产二进制，且运行实例加载的就是该二进制。

## 3. 真实麦克风验证记录（本次唯一一次输入）

目标语句：**“脑机接口是什么？”**

| 记录项 | 实测值 |
|---|---|
| 原始 ASR 文本 | **`手机接口是什么？`**（归一化日志中的 text 字段） |
| AsrTextNormalizer 原始文本 | `手机接口是什么？`（长度 24 字节） |
| 是否发生归一化 | **否** — `event=ASR_NORMALIZATION_SKIPPED reason=NO_RULE_MATCHED` |
| 匹配规则 ID | 无（未命中） |
| 归一化后文本 | 与原始相同：`手机接口是什么？` |
| IntentRecognizer | `GENERAL_QA` |
| FactRiskGuard | `risk=NORMAL blocked=false`（`reason=knowledge question form; not a realtime status query`） |
| 实际发送给 Ollama 的文本 | `手机接口是什么？` |
| Ollama 返回 | 183 字节，3 句；首句为“手机接口是用于连接手机与其他设备的物理或数据传输端口。”，随后“常见的有USB、Lightning、Micro USB、Type-C等。”“不同手机型号支持的接口可能不同。” |
| `/g1/tts` 播报 | 3 句全部 `TtsMaker ret=0`，逐句播报（Message split into 3 sentence(s)） |
| 总耗时 | `COMPLETED total_ms=5902` |
| Action Request | **0 次**（本轮无 `ACTION_SENT`；全日志仅 1 条为当日 P0-20.1 的 wave 记录） |
| 本轮 request_id | `1`（该 Router 进程自 07:25 启动后首条请求，无历史混淆） |

## 4. 验收结论（如实）

| 验收重点 | 结论 |
|---|---|
| ASR 输出“脑筋接口”时必须归一化为“脑机接口” | **本轮未触发**——ASR 输出的是第三种变体“手机接口”，不属于 ASR001 覆盖范围，无法证明该条别名在真机生效 |
| Ollama 必须收到“脑机接口” | **本轮未达成**——Ollama 收到的是“手机接口是什么？”，因此回答的是手机硬件接口 |
| 最终回答与脑机接口概念相关 | **本轮未达成**（回答为手机接口） |
| 不得触发 Action Request | **达成**（0 次） |
| 不得修改源码/重编译/重启服务 | **达成**（PID 11283 未变、NRestarts=0、二进制哈希未变） |

**因此：ASR001 的“脑筋接口→脑机接口”目前只有单元测试证据（`asr_text_normalizer_test` 22/22，含该用例），本轮真机未验证到该分支，不能声称“已在真机验证别名纠错”。**

## 5. 关于“手机接口”的处理判断（重要，未实施）

不新增别名的原因：

1. **“手机接口是什么”本身是完全合法的独立问题**（问手机硬件接口），把“手机接口”映射为“脑机接口”会污染真实提问，属于禁止的过度纠正。
2. 该变体与已覆盖的“脑急/脑筋”性质不同：后两者不是合法中文词组，前者是常见名词组合。
3. 本阶段任务明确禁止“为通过验收增加模糊匹配或未经验证的别名”。

若后续要处理，只能做**上下文门控**规则（例如同时出现“机器人/意念/信号/大脑”等语境词才纠正），且必须先有更多真实日志证据并单独评估——不属于 V1.0。

## 6. ASR 变体统计（真实日志，截至本报告）

| 观测到的 ASR 输出 | 性质 | 当前处理 |
|---|---|---|
| `脑机接口` | 正确 | 不修改 |
| `脑急接口` | 非词组合，明显误识 | ASR001 纠正 |
| `脑筋接口` | 非词组合，明显误识 | ASR001 纠正（单测覆盖，真机未触发） |
| `手机接口` | **合法词组** | 不纠正（避免破坏正常提问） |

演示建议：说“介绍一下脑机接口技术”或稍慢、清晰地分字说出“脑—机—接口”，可降低短词被替换的概率；如仍误识，属 ASR 层限制，系统保持"不编造"的安全行为（会按听到的词作答，不会凭空假设是脑机接口）。

## 7. 变更检查

```text
源码修改：0
重新编译：0
生产服务重启：0（PID 11283 未变，NRestarts=0）
新增别名：0
模糊匹配引入：0
真实动作触发：0
冻结版本哈希变化：0（仍为 6ab99de8…）
```

## 8. 最终状态

V1.0 生产版本保持冻结不变：Router `6ab99de8ceb287d1fdb58b66399874c60b41734775a74a0c2745bf64223d6bb7`，五节点运行正常，本任务完成后不再修改生产版本。
