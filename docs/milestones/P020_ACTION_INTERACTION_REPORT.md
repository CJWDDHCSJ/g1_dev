# P0-20 语音动作触发与业务闭环 · 交付报告

- 日期：2026-11-02
- 结论：**动作命令识别、固定白名单映射、Action 请求发送、结果关联与反馈已实现并在 G1 ARM64 编译、DOMAIN_ID=77 用 Mock Action Executor 实测通过（成功/失败/超时/禁用/去重/队列满顺序全部符合预期）；同时修复了 P0-19 的忙碌提示插队与去重缓存无界两个问题，并验证了 SIGINT 停机行为。生产五节点 PID 未变、未重启、未触发任何真实动作。**
- 证据：`evidence_p020/`（动作请求日志、路由日志、TTS 时间序列、停机日志）

## 1. 修改前架构审计（真实源码）

### 1.1 Action Executor 接口（`~/lidar_ws/src/g1_action_executor`）

| 项目 | 实测结果 |
|---|---|
| 节点名 | `/g1_action_executor` |
| 请求话题/类型 | `/g1/action_request`，`std_msgs/msg/String` |
| 请求负载 | **规范动作名**（`wave`/`wave_low`/`hands_up`/`clap`/`victory`/`heart`），`normalize_command` 内部做小写与空白处理 |
| 结果话题/类型 | `/g1/action_result`，`std_msgs/msg/String` |
| 结果格式 | `success:<name>:<id>`、`failed:<name>:<id>`、`timeout:<name>:timeout:state_id=<n>`、`rejected: <reason>` |
| 请求 ID | **没有**（需按动作名 + 时间窗关联） |
| 白名单 | wave 26 / wave_low 25 / hands_up 15 / clap 17 / victory 30 / heart 20（`arm_action_executor.cpp:209-236`） |
| 内部超时 | 启动等待 5s、完成等待 30s、动作后稳定 3s（最坏约 38s） |
| 并发 | 单 worker 串行，执行中请求排队 |
| 阻塞 | 请求回调仅入队，不阻塞 ROS 执行器 |

### 1.2 结论与设计约束

- 因为**没有请求 ID**，关联策略：Voice Router 侧 worker 天然串行（同时最多一个动作在途），用「动作名 + 发送后序号快照」匹配；发送前的旧结果被丢弃；名字不匹配的结果忽略并继续等待直到超时。不重试。
- 动作总超时设为参数 `action_timeout_seconds`（默认 45s > 执行器最坏 38s）。
- 一切映射在程序内完成，**不接受任意字符串作为动作名**。

## 2. P0-19 边界复查（三项）

### 2.1 QUEUE_FULL 忙碌提示曾绕过统一调度 —— 已修复

- 原实现：`enqueue_user_text()` 在队列满时**直接从 ROS 回调线程** `publish_reply()`，会插队到已接受请求的回复之前。
- 修复：新增有界的 `notices_`（容量 2，溢出丢最旧并告警）；worker 每处理完一条请求后 `drain_notices()` 按序发布。
- 实测（慢响应 + 6 条并发）：`QUEUE_FULL` 2 次，TTS 序列为「已接受请求的回复 → 忙碌提示 ×2」，时间戳 399.729 → 399.730 → 399.730，**不再插队**。

### 2.2 Worker 在 HTTP 请求中收到 SIGINT —— 已修复并实测

- 原实现：`curl_easy_perform` 不可中断，最坏拖到 60s，超过 systemd `TimeoutStopSec=15`（会被 SIGKILL）。
- 修复：`CURLOPT_NOPROGRESS=0` + `CURLOPT_XFERINFOFUNCTION` 检查 `shutdown_requested_`，停机时返回 1 中止传输；`stop_worker()` 先置标志并 `notify_all`，同时唤醒动作结果等待。
- 实测（DOMAIN_ID=77，精确节点 PID）：

| 场景 | 退出耗时 |
|---|---:|
| 空闲时 SIGINT | **0.29 s** |
| HTTP 等待中 SIGINT（60s 超时 + 8s 慢 Mock） | **0.79 s**（日志 `Operation was aborted by an application callback`） |
| 动作等待中 SIGINT（60s 动作超时，Mock 不回复） | **0.29 s**（`ACTION_FINISHED TIMEOUT` → `Worker loop exited`） |

三者均远小于 15s，无死锁、无线程残留。

### 2.3 去重历史缓存无界 —— 已修复

- 原实现：`last_accepted_ms_` 只增不减。长期运行会随不同文本无限增长。
- 修复：提交时若条目 ≥64 或每 32 次提交，清理 `now - 窗口×2` 之前的记录。
- 实测（单测）：连续提交 400 条不同文本后缓存规模 <64；早期文本在过期后可再次被接受。

## 3. 新增模块

### 3.1 `ActionCommandMapper`（独立、可单测）

- 白名单常量：26/25/15/17/20/30；`hand_on_heart`/33 记为永久禁用常量并在映射出口二次校验（任何路径都不可能输出 33）。
- 判定顺序：**永久禁用 → 否定/停止 → 疑问/技术讨论 → 最长短语匹配（26 条短语）**。
- 关键取舍：命令含糊（如"给我们表演一个动作"）不猜动作，返回 `UNKNOWN` 并播报能力澄清提示。

### 3.2 动作执行闭环（Voice Router worker 内）

```text
DEMO_ACTION
  → ActionCommandMapper（白名单）
  → /g1/action_request（规范名）
  → 等待 /g1/action_result（按名 + 序号快照匹配，超时 action_timeout_seconds=45s）
  → SUCCESS / FAILED / TIMEOUT / REJECTED
  → 统一 publish_reply（顺序保证）
```

- 新增状态 `ACTION_WAITING`（转移：PROCESSING→ACTION_WAITING→REPLY_READY/COMPLETED/FAILED/TIMEOUT）。
- 反馈文案：成功「好的，X动作完成了。」；失败「这个动作暂时没有执行成功。」；拒绝「动作执行器暂时不能执行这个动作…」；超时「动作执行结果暂时没有收到，我不会自动重复执行。」。
- 未收到结果绝不重发；不宣称未确认的成功。

### 3.3 IntentRecognizer 最小扩展（仅动作命令识别，未改任何状态/实验室逻辑）

- 原因：实测发现"挥个手""拍个手""举手"未进入 DEMO_ACTION（只认"表演+动作宾语"），会流入 Ollama。
- 修改：新增**直接命令式短语**、**否定式动作**（"不要挥手"→DEMO_ACTION→NEGATED 本地回复）、**永久禁用手势**（"把手放在心口"→DISABLED 本地拒绝），并给兜底动作词加**疑问/原理豁免**（"挥手的原理是什么"仍为 GENERAL_QA）。
- 负向保护：`挥手的原理是什么`、`挥手是怎么实现的` 均保持 GENERAL_QA（单测锁定）。

## 4. 文件修改清单

| 文件 | 性质 | SHA256 |
|---|---|---|
| `include/g1_voice_router/action_command_mapper.hpp` | 新增 | `d8adabc990c945e2b2228a87e352c944a34318eaad52402bb510fab330079cf1` |
| `src/action_command_mapper.cpp` | 新增 | `1dac27094e1169717e418d86c122bff54f1e5994c11c54377007ab7f91cea812` |
| `test/action_command_mapper_test.cpp` | 新增 | `e739a9f156b3df2e8df389253efd95f7b423961e70a30da823b626ddefc80ff7` |
| `src/voice_router.cpp` | 修改（动作分支/结果等待/忙碌提示/停机中断） | `dfbe656dffd3c14cc02814fae32e2d5d844b786b6c0891cf615abdd9bcb5ee81` |
| `src/intent_recognizer.cpp` | 修改（动作命令识别扩展） | `7bfc93a5a5549ba21ac1e1b566769437e324158c0f69acd28bcb8e98299108b6` |
| `src/interaction_manager.cpp` | 修改（ACTION_WAITING、去重有界） | `f6e81bb8a624ca0e371f71851848a605bf148337483885e243d5245b2d7e0e04` |
| `src/interaction_filter.cpp` | 修改（新增状态字符串） | `7e65026b6434894ceddff9eec03ea5357ddd0d1218d9318ddf64d54cd50dd061` |
| `include/g1_voice_router/interaction_manager.hpp` | 修改 | `71eb8a385eea12d547e5ec0aa80a1935012185cf6eaa2623d1f506e96a3c59f7` |
| `include/g1_voice_router/interaction_types.hpp` | 修改 | `ff4850ad86166bbd7d67901f318df9f691f78565033b678bce9af77bd10c825f` |
| `test/interaction_manager_test.cpp` | 修改（+5 用例） | `6bef52250f603d4ac1623683cf2854230c6d3dcf6fc271685eda258344e69d8e` |
| `test/intent_recognizer_test.cpp` | 修改（+11 用例） | `92e4e17f3e02548b6c876c008c63f30ec87f85de43169239734a11c3fc398eb7` |
| `CMakeLists.txt` | 修改（新增库与测试） | `861f0268619d9e8c15dfb799830fe5d6a5331a51b2cce51b5cbf7b19f023c2f5` |

未修改：FactRiskGuard、KnowledgeBase、RobotStateProvider、ASR Bridge、TTS Node、Action Executor、systemd unit、logrotate、Ollama 配置。

## 5. 测试结果

| 套件 | 本地 x86 | G1 ARM64 |
|---|---|---|
| ActionCommandMapper（新增） | **39/39** | **39/39** |
| InteractionManager | **76/76**（原 71 + 5） | **76/76** |
| IntentRecognizer | **86/86**（原 75 + 11） | **86/86** |
| FactRiskGuard | 119/119 | **119/119** |
| KnowledgeBase | 57/57 | **57/57** |
| RobotStateEvaluator | 44/44 | **44/44** |
| colcon test-result | — | **6 tests, 0 errors, 0 failures, 0 skipped** |

编译：`colcon build --packages-select g1_voice_router --symlink-install` 首次 28.2s 通过（修复一处遗漏的 `progress_callback` 定义后）。

## 6. DOMAIN_ID=77 隔离实测

Mock：`Mock Ollama`（normal/fail 模式）+ `Mock Action Executor`（success/fail/none/bad 模式）+ `/g1/tts` 录制器。**未启动 TTS/ASR/Action Executor 生产节点，未触发真实动作。**

### 6.1 动作链路（TTS 时间序列 + 动作请求日志）

| 输入 | Intent | Mapper | 动作请求 | Mock 结果 | TTS 反馈 |
|---|---|---|---|---|---|
| 挥个手 | DEMO_ACTION | wave/26 | `REQ 1 name=wave` | success | 「好的，挥手动作完成了。」 |
| 拍个手 | DEMO_ACTION | clap/17 | `REQ 2 name=clap` | fail | 「这个动作暂时没有执行成功。」 |
| 举手 | DEMO_ACTION | hands_up/15 | `REQ 3 name=hands_up` | 无响应 | 「动作执行结果暂时没有收到，我不会自动重复执行。」（6s 超时） |
| 把手放在心口 | DEMO_ACTION | **DISABLED** | **无请求** | — | 「这个动作不在我的可执行列表里，我不能执行。」 |
| 比个心 ×2（200ms） | DEMO_ACTION | heart/20 | **仅 `REQ 4` 一次** | success | 「好的，比心动作完成了。」 |
| 不要挥手 | DEMO_ACTION | NEGATED | 无请求 | — | 「好的，那我不做动作。」 |
| 挥手的原理是什么 | GENERAL_QA | — | 无请求 | Mock Ollama | 正常知识回答 |
| 给我们表演一个动作 | DEMO_ACTION | UNKNOWN | 无请求 | — | 能力澄清提示 |

**动作请求总计 5 次，动作名分别为 wave/clap/hands_up/heart/wave，全部在白名单内，`hand_on_heart`/33 出现 0 次。**

### 6.2 动作与知识混合顺序

提交顺序「比个心 → 脑机接口是什么 → 你现在有多少电量」，TTS 时间戳 998.581（动作反馈）→ 998.584（Ollama）→ 998.679（电量兜底），**顺序与提交一致**，动作执行期间后续请求在队列等待，无插队。

### 6.3 队列满顺序（慢 Ollama）

6 条快速提交：`QUEUE_FULL` 2 次、忙碌提示 2 条，且均出现在已接受请求的回复之后（见 2.1 时间戳证据）。

### 6.4 停机

见 2.2 表格（0.29s / 0.79s / 0.29s）。

### 6.5 本阶段机器人真实动作次数：**0**

## 7. 生产环境保护

```text
五个生产节点 PID：1365 / 1366 / 1367 / 1368 / 1369（与 P0-18.3 一致，未重启）
g1-voice-stack.target：active
是否修改 systemd / target / Restart：否
是否修改 logrotate：否
Domain 0 是否被测试节点污染：否（全部测试在 77，测试节点按 PID 精确清理，Domain 77 复查为空）
生产 Voice Router 是否切换：否（仍为旧版本进程，新版本仅编译进 install）
```

## 8. 遗留问题

1. **未做真机动作验证**：本阶段按要求只做 Mock 隔离测试；真实挥手链路仍以 P0-16 记录为准。真实动作验收需单独安排（有人现场监督）。
2. Action Result **无请求 ID**：当前靠"串行 + 动作名 + 序号快照"关联，若将来支持并发动作必须改为带 ID 的协议（不改执行器则不可并发）。
3. 执行器自身最坏 38s，Router 默认 45s；若执行器异常挂死，用户最多等 45s 才得到超时反馈。
4. 若 Result 完全丢失但动作实际已执行，系统会播报"未收到结果、不重复执行"——这是安全取向，但可能与实际物理状态不一致（如实说明）。
5. 生产部署未进行：源码与 install 已更新、隔离验证通过，生产仍需单独切换。
6. ASR 误识（如"挥手"被识别为其他词）仍可能导致命令未触发，属 ASR 层限制。
