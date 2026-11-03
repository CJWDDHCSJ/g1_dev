# P0-19 连续对话与交互任务管理模块 · 交付报告

- 日期：2026-11-02
- 结论：**C++17 InteractionManager 已实现并在 G1 ARM64 通过全部测试；噪声过滤、去重、有界 FIFO 队列、统一回复顺序、超时/失败恢复、停机清理均已用 DOMAIN_ID=77 + Mock 实测验证。生产节点未切换、未重启。**
- 证据：`evidence_p019/`（router 日志、TTS 录制、Mock 日志与脚本）

## A. 修改前审计（真实源码确认）

### A.1 ASR Bridge（`g1_asr_bridge/src/asr_json_bridge.cpp`）

- 仅从 `/audio_msg` 的 JSON 中提取 `text` 字段并发布到 `/g1/asr_text`（`std_msgs/String`）。
- **没有 confidence 字段，没有 final/partial 标记**，因此本阶段只能做文本层过滤；未改造 Bridge（也无需改造）。
- 同一句话可能被重复发布（P0-18.3 真机日志已出现重复/噪声文本）。

### A.2 Voice Router（修改前）

| 审计问题 | 真实情况 |
|---|---|
| Ollama worker 数量 | 1 个（`worker_loop`） |
| ASR 回调并发 | 回调线程直接执行业务，未排队 |
| 本地分支是否绕过 worker | **是**——GREETING / LAB_INTRO / ROBOT_INTRO / GOODBYE / ROBOT_STATUS / FactRiskGuard 命中的回复都在 ROS 回调线程直接发布 `/g1/tts` |
| 多请求 TTS 顺序 | **不确定**：排队的 Ollama 回答可被后来的本地回答插队 |
| 重复文本 | 无去重（注释明确写"不做文本去重"） |
| 队列长度限制 | **无**（`std::deque` 无上限） |
| 超时 | 仅 HTTP 超时（`request_timeout_seconds`，默认 60s） |
| 关闭行为 | 清空队列 + join worker（已正确） |

### A.3 TTS Node（只读）

- 订阅 `/g1/tts`，消息进内部队列，按中文分句逐句 `TtsMaker`，句间按估算时长等待。
- **没有播放完成回调**；`TtsMaker ret=0` 只代表提交成功，不代表整段播放结束。本阶段未修改 TTS，也未声称实现播放完成同步。

## B. 新模块设计

```text
ASR /g1/asr_text
   ↓ （ROS 回调，只做准入）
InteractionManager::submit
   ├─ classify_text → FILTERED（空/纯标点/明确语气词）
   ├─ 去重（归一化文本，1200ms 窗口）→ DUPLICATE
   ├─ 有界队列（max_pending=3，不含正在处理的一条）→ QUEUED / REJECTED(QUEUE_FULL)
   ↓ （唯一 worker 线程）
process_request：IntentRecognizer → 本地分支 / KnowledgeBase / ROBOT_STATUS / FactRiskGuard → Ollama
   ↓
publish_reply（所有回复都在同一线程发布 → 顺序严格等于接收顺序）
```

- **核心改动**：把原先"回调线程跑本地分支、worker 只跑 Ollama"的结构，改为 **全部业务在单一 worker 串行执行**，从根上消除本地回答插队。
- 队列满策略：**拒绝最新请求 + 播报忙碌提示**（策略 A）。不丢弃正在处理/已排队请求，不中断进行中的 Ollama 请求，不静默丢弃。
- 状态机：RECEIVED→QUEUED→PROCESSING→(WAITING_OLLAMA)→REPLY_READY→COMPLETED，失败走 FAILED，过滤/去重/拒绝各有独立终态；非法转移被拒绝并可通过单测验证。
- 过滤器：只过滤**空/纯标点**与**完整独立的语气词**（嗯/啊/呃/唔/哦/额，含重复如"嗯嗯嗯"）；"嗯，介绍一下实验室""IMU""G1""停止""取消""嗨"等一律保留。
- 停机：`stop()` 清空队列、唤醒 worker，worker 退出后 join；日志可见 `Worker loop exited`。
- 结构化日志：`request_id=… event=… queue_depth=… text_len=…`（不打印完整语音文本）。

## C. 文件修改清单

| 文件 | 性质 | SHA256 |
|---|---|---|
| `include/g1_voice_router/interaction_types.hpp` | 新增 | `24a41da91893a7936a9ef66d3323cea8a75b080cd372c2f93f3890c691beccc6` |
| `include/g1_voice_router/interaction_manager.hpp` | 新增 | `0b31c56bda3c204f436c7c2a3ce68352c0b87f164e0f28cc52f0596dea8db678` |
| `src/interaction_manager.cpp` | 新增 | `18f90d333d0fd36487ffd4c3d8f8da1dceef55b1fee32bf5a332d70e027388c1` |
| `src/interaction_filter.cpp` | 新增 | `d06792307e8ddec743f4343e297db28d0946b4ebfe45a6af598698521f5e4c9d` |
| `test/interaction_manager_test.cpp` | 新增 | `a92e463accb49cd74ef9de14b45ce67c19bc96ca1e2b2c375f1031cd47f080dd` |
| `src/voice_router.cpp` | 修改 | `3ef37c6b245eaf88b4fbc571fcac10314cff1d95ca635e16879175e363047356` |
| `CMakeLists.txt` | 修改（新增库与测试） | `37c31666310563080325158cef341d7137fba4aec719197f8e49b5772bf48871` |

未修改：`intent_recognizer.*`、`fact_risk_guard.*`、`knowledge_base.*`、`robot_state_*`、ASR Bridge、TTS Node、Action Executor、systemd unit、logrotate 配置。

## D. 单元测试

| 套件 | 本地 x86 | G1 ARM64 |
|---|---|---|
| InteractionManager（新增） | **71/71** | **71/71** |
| IntentRecognizer | 75/75 | **75/75** |
| FactRiskGuard | 119/119 | **119/119** |
| KnowledgeBase | 57/57 | **57/57** |
| RobotStateEvaluator | 44/44 | **44/44** |
| colcon test-result | — | **5 tests, 0 errors, 0 failures, 0 skipped** |

- 新增覆盖：过滤 25 例、归一化 5 例、去重 6 例、有界队列 8 例、状态机 10 例、停机 5 例、并发（4 线程 × 25 提交）5 例、FIFO 顺序 2 例。
- 开发中两轮真实失败均为**测试自身逻辑问题**（队列容量不足干扰去重用例；把 PROCESSING→COMPLETED 误判为非法转移），已修正测试假设，未删除任何断言。

## E. 隔离域运行结果（DOMAIN_ID=77 + Mock，不启动 TTS/ASR/Action）

| 场景 | 输入 | 结果 | 证据 |
|---|---|---|---|
| A 单条 | 脑机接口是什么 | 1 次 QUEUED→COMPLETED，Mock 1 次，TTS 1 条 | router.log request_id=1 |
| B 去重 | 脑机接口是什么 ×3（200ms 间隔） | 1 次处理 + **2 次 DUPLICATE**，Mock 1 次，TTS 1 条 | `event=DUPLICATE reason=DUPLICATE_WINDOW` ×2 |
| C 噪声 | 嗯 / 啊 / 呃 | **3 次 FILTERED(NOISE_PHRASE)**，Mock 0 次，TTS 0 条 | `event=FILTERED` ×3 |
| D FIFO | 3 条不同问题（300ms 间隔） | 3 条依次处理，Mock 3 次，TTS 顺序与提交一致 | request_id 6/7/8 + tts.tsv 时间戳递增 |
| E 混合 | 电量 + 脑机接口 + 实验室 + 如何判断（250ms 间隔） | TTS 顺序严格等于提交顺序：本地电量兜底 → Ollama → KB 兜底 → Ollama；Mock 仅 2 次 | tts.tsv 时间戳 909.322 / 909.576 / 909.831 / 910.078 |
| F 队列满 | 慢响应下 6 条并发 | **2 次 REJECTED(QUEUE_FULL)** 且各播报 1 条忙碌提示；队列深度未超 3 | `QUEUE_FULL` ×2，`稍等一下` ×2 |
| G 失败恢复 | HTTP 500 / 非法 JSON / 超时 | 500→FAILED+兜底1条；非法JSON→FAILED+兜底1条；超时→FAILED+兜底；**后续请求均恢复正常**，无重复回复 | router2.log `FAILED reason=OLLAMA_ERROR` ×2；router.log 超时 4 次 |
| H 节点退出 | SIGINT | 路由器正常退出，日志 `Worker loop exited`；域 77 节点清空，无残留线程/进程 | router2.log 末尾 |

- Mock HTTP 计数逐场景核对（A=1、B=1、C=0、D=3、E=2、F=4、G=4），全部与实际处理条数一致。
- 说明：早期 B 用例用 `ros2 topic pub --once` 每条间隔约 1.9s，超出 1200ms 窗口而未判重——改用 200ms 高频发布后正确判重；这是测试工具差异，不是实现缺陷。

## F. 资源与性能（观测值）

| 指标 | 实测 |
|---|---|
| 队列上限 | 3（等待中），加 1 条处理中 |
| 峰值队列深度 | 3（统计 `max_queue_depth`） |
| 单请求处理耗时（Mock 正常） | 3–4 ms |
| 超时路径耗时 | 约 5 s（测试配置 `request_timeout_seconds:=5`），失败后立即继续下一条 |
| 过滤命中 / 去重命中 / 队列满拒绝 | 3 / 2 / 2（本轮日志统计） |
| 死锁/数据竞争 | 未出现；并发用例 100 次提交无异常 |
| 内存 | 未做精确测量（进程常驻变化不可观测到 MB 级增长），队列有界保证不会无限增长 |

## G. 生产环境状态

```text
五个生产节点 PID：1365 / 1366 / 1367 / 1368 / 1369（与 P0-18.3 一致，未重启）
g1-voice-stack.target：active
是否重启生产节点：否
是否修改 systemd / target / Restart 策略：否
是否修改 logrotate：否
是否触发机器人动作：否
是否修改 ASR / TTS / Action Executor / Provider 源码：否
生产 Voice Router 是否使用新版本：否（仍是旧版本进程，需下阶段单独切换）
```

## H. 遗留事项

1. **TTS 无播放完成反馈**：只能保证 `/g1/tts` 的提交顺序，不能声称多段播报之间"严格不重叠"。TTS Node 内部有队列与句间等待，实际听感连续，但无完成事件。
2. **未实现语音打断**：AudioClient 是否支持安全中断未验证，按要求未做。
3. **ASR 无置信度**：无法用置信度过滤；"嗯嗯"这类非语气词噪声（如"被告在"）仍会走 GENERAL_QA 调模型。
4. **有意重复提问的取舍**：同一文本 1.2s 内的真实重复（如用户连说两次"你好"）会被判重丢弃，这是文本通道的固有限制，已在设计上接受。
5. DEMO_ACTION 仍走原行为（进 Ollama 队列），本阶段未接入动作调度。
6. 生产部署未进行：源码与 install 已更新、隔离域验证通过，但生产进程仍是旧版本。

## 验收对照（任务书十七）

1 现有语音链路未被破坏 ✅（生产未动，隔离域全链路正常）；2 噪声不调 Ollama ✅；3 有效短句不过滤 ✅（25 例过滤测试含 12 个保留用例）；4 去重 ✅；5 有界 FIFO ✅；6 队列不无限增长 ✅；7 超时/失败处理 ✅；8 本地与模型回复顺序可控 ✅；9 同一请求不重复 TTS ✅；10 worker 正确退出 ✅；11 并发安全 ✅；12 75/119/57/44 全回归 ✅；13 ARM64 编译 ✅；14 域 77 场景 A–H 通过 ✅；15 生产未重启 ✅；16 systemd/logrotate 未改 ✅；17 报告与证据已保存 ✅。
