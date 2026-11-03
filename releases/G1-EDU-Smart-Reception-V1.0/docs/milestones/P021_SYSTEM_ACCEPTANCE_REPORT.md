# P0-21 系统整体验收与 V1.0 版本冻结报告

- 日期：2026-11-03
- 结论：**ASR 归一化与 IMU 状态路由两项边界修复完成；453 项单元测试全通过；G1 ARM64 编译通过；DOMAIN_ID=77 隔离验证通过；生产真机演示 6 项通过（1 项因 ASR 变体暴露缺陷，已补规则并再上线）；V1.0 已冻结并生成软著材料。**
- 证据：`evidence_p021/`（Router/TTS 日志、systemd 快照、动作计数）

## 1. 阶段 A：生产与源码审计（改动前）

| 检查项 | 实测结果 |
|---|---|
| 五服务 | 1379/1380/1381/1382/1383，均 active/running，NRestarts=0 |
| `g1-voice-stack.target` | active |
| 生产 Router 版本 | `e4eda4bb…`（P0-20 版本），`/proc/1381/exe` 与 build 一致 |
| Router 进程数 | 1 |
| `/g1/action_request` | 发布者 1（g1_voice_router），订阅者 1（g1_action_executor） |
| `/g1/action_result` | 发布者 1（executor），订阅者 1（router） |
| 源码 vs 生产 | `voice_router.cpp=3f6e59a5`、`interaction_manager.cpp=9cc4f139` 等本地与 G1 完全一致 |

## 2. 阶段 B：ASR 专有名词归一化（新增模块）

文件：`include/g1_voice_router/asr_text_normalizer.hpp`、`src/asr_text_normalizer.cpp`、`test/asr_text_normalizer_test.cpp`

| 规则 | 输入 | 输出 | 约束 |
|---|---|---|---|
| ASR001 | 脑急接口 / 脑筋接口 | 脑机接口 | 无上下文要求 |
| ASR002 | 语数之一 / 语数G1 | 宇树G1 | **仅当**文本含机器人/宇树/动作/价格/型号等上下文 |

安全边界（均有测试）：
- 含 `不要/别/停止/取消/不准/不许` → 整体跳过（`CONTROL_TOKEN_PROTECTED`），否定句不会被改成命令；
- 不修改任何数字与状态数值；
- 无机器人语境时 `语数之一` 保留原文（`UNITREE_ALIAS_WITHOUT_CONTEXT`）；
- 原始文本始终保留在 `AsrNormalization.raw_text`，日志只输出长度 + FNV 摘要 + 归一化结果；
- 未使用模型、未做模糊匹配/编辑距离。

集成位置：`enqueue_user_text()` 中，`/g1/asr_text` 之后、过滤/去重/入队之前。

## 3. 阶段 C：IMU 实时状态问法路由修复

问题：`你现在的IMU数据接收情况怎么样` 原为 GENERAL_QA → FactRiskGuard REALTIME_STATUS → 通用兜底，浪费了已具备的 Provider 能力。

修复：IntentRecognizer 新增 `imu_status_query` 判定（`imu/惯导/陀螺仪/惯性测量` 且含 `接收/更新/能收到/正常/健康/状态/数据`，并排除知识问法）→ ROBOT_STATUS → 本地 Provider。

语义边界（96 项测试锁定）：

| 输入 | 路径 |
|---|---|
| 你现在的IMU数据接收情况怎么样 / 能收到IMU数据吗 / IMU数据还在更新吗 | ROBOT_STATUS → 本地 |
| 你的IMU是否完全健康 | ROBOT_STATUS → 本地，回答明确"未完成完整传感器健康诊断" |
| IMU是什么 / IMU怎么工作的 / IMU数据接收原理是什么 | GENERAL_QA → Ollama |

FactRiskGuard 未删除任何规则；仍作为 GENERAL_QA 误分类时的第二道防线。

额外补充：`你会做哪些动作` → DEMO_ACTION → 本地白名单能力介绍（不让模型自由列举，也不执行动作）。

## 4. 修改文件清单

| 文件 | 性质 | SHA256 |
|---|---|---|
| `include/g1_voice_router/asr_text_normalizer.hpp` | 新增 | `83fb29e1193230969c9237fd8e6c5d62623235bf1d35609326444b472466cd4a` |
| `src/asr_text_normalizer.cpp` | 新增 | `b8ae1126ae7aec8de9067c605bbfd7d58ebf4a3f23dc90fcdd510ee80b6569f1` |
| `test/asr_text_normalizer_test.cpp` | 新增 | `d53ccd7b74ad14b32d40d8c07159c8b649293cfcf3b9a92af0ef9a4ac94bb412` |
| `src/voice_router.cpp` | 修改（归一化集成 + 证据日志） | `c4046431816bfac0b51a999faa9dd701e810799cf10a445dc3c2072b44585b32` |
| `src/intent_recognizer.cpp` | 修改（IMU 状态问法、能力询问） | `73b136cfc26756cff381f335dff51bf071296454b762038c9d87a9ff0e5f81d8` |
| `src/action_command_mapper.cpp` | 修改（能力询问识别为 QUESTION） | `ee46556a0cd4439e410e09aad2df7a9941b91f0d9b3798ce0867d59f35eec476` |
| `test/intent_recognizer_test.cpp` | 修改（+12 用例） | `77e1d626b9658c5b22341bdef7b5edafd94b52172c63a3cc5e6b800bed22b818` |
| `CMakeLists.txt` | 修改（新库源 + 新测试目标） | `d392c68029f86ccdd82e2c8c3fe1f670e4bd7f56551b4082221277ae98b094aa` |

未修改：FactRiskGuard、KnowledgeBase、RobotStateProvider/Evaluator、ASR Bridge、TTS、Action Executor、systemd unit、logrotate、Ollama、DDS。

## 5. 阶段 D：回归测试（G1 ARM64 实测）

| 套件 | 用例数 | 结果 |
|---|---:|---|
| asr_text_normalizer_test | **22** | 通过 |
| action_command_mapper_test | 39 | 通过 |
| interaction_manager_test | 76 | 通过 |
| intent_recognizer_test | **96** | 通过 |
| fact_risk_guard_test | 119 | 通过 |
| knowledge_base_test | 57 | 通过 |
| robot_state_evaluator_test | 44 | 通过 |
| **合计** | **453** | colcon `7 tests, 0 errors, 0 failures, 0 skipped` |

编译：`colcon build --packages-select g1_voice_router --symlink-install` 通过（ARM64）。

### 5.1 隔离域验证（DOMAIN_ID=77，Mock）

| # | 输入 | 归一化/意图 | 路径 | 结果 |
|---|---|---|---|---|
| 1 | 脑急接口是什么 | ASR001 → 脑机接口 / GENERAL_QA | Mock Ollama（收到"脑机接口是什么"） | 通过 |
| 2 | 你现在的IMU数据接收情况怎么样 | ROBOT_STATUS | 本地 Provider | 通过 |
| 3 | IMU是什么 | GENERAL_QA | Mock Ollama | 通过 |
| 4 | IMU数据接收原理是什么 | GENERAL_QA | Mock Ollama | 通过 |
| 5 | 你的IMU是否完全健康 | ROBOT_STATUS | 本地（不宣称健康） | 通过 |
| 6 | 语数之一多少钱 | ASR002 → 宇树G1 / PRICE | 拦截，0 次 Ollama | 通过 |
| 7 | 语数之一这个说法是什么意思 | 跳过（无语境）/ GENERAL_QA | Mock Ollama | 通过 |
| 8 | 不要挥手 | 控制词保护 / DEMO_ACTION | 本地拒绝，0 次动作 | 通过 |

Mock Ollama 收到 4 条请求，文本与预期完全一致；动作请求 0 条；测试后按 PID 精确清理，Domain 77 复查为空。

## 6. 阶段 E：生产真机演示（2026-11-03）

| # | 目标语句 | ASR 实际文本 | 意图 | 路径 | TTS | 结果 |
|---|---|---|---|---|---:|---|
| 1 | 脑机接口是什么 | **脑筋接口是什么？** | GENERAL_QA | Ollama（7.0s） | 207B | 链路通，但术语被 ASR 误识 → 事后补 ASR001 覆盖 |
| 2 | 介绍一下我们的实验室 | — | LAB_INTRO | 本地知识库 | 412B | 正确 |
| 3 | 你现在还有多少电量 | — | ROBOT_STATUS | 本地 BMS | 41B | 正确 |
| 4 | 你现在的 IMU 数据接收情况怎么样 | — | ROBOT_STATUS | 本地 Provider | 93B | 正确（已不再走 Guard 兜底） |
| 5 | 你现在整体运行状态怎么样 | — | ROBOT_STATUS | 本地聚合 | 131B | 正确 |
| 6 | 你会做哪些动作 | — | DEMO_ACTION | 本地能力介绍 | 105B | 正确，未执行动作 |

- 本轮演示会话 `ACTION_SENT` 计数 **0**；无 DUPLICATE/FILTERED；无重复播报。
- 全日志仅 1 条 `ACTION_SENT`，为当日 P0-20.1 的 wave 验收（`request_id=7 action=wave id=26`），非本轮。

### 6.1 用户反馈「部分对话需要说第二遍」的根因

日志显示 6 句均被接收（无漏识别），问题集中在第 1 句：ASR 把"脑机接口"识别为 **"脑筋接口"**（此前还出现过"脑急接口"）。归一化层当时只覆盖"脑急接口"，因此该句被当作陌生术语交给模型，模型据"脑筋接口"作答，用户只能重说。

处置：新增 `脑筋接口 → 脑机接口`（同一 `ASR001` 规则，属同一真实证据族），单测 22/22，G1 重建通过，已单服务重启上线（PID 11283，哈希 `6ab99de8…`）。

## 7. 阶段 F：V1.0 版本冻结

| 项目 | 内容 |
|---|---|
| 冻结说明 | `V1.0_RELEASE_NOTES.md` |
| 安装启动 | `V1.0/INSTALL_AND_START.md` |
| 源码+二进制+unit 全量清单 | `V1.0/SOURCE_AND_BINARY_MANIFEST.sha256`（61 行） |
| 生产 Router | PID 11283，`6ab99de8ceb287d1fdb58b66399874c60b41734775a74a0c2745bf64223d6bb7` |
| Provider / TTS / ASR / Executor | `b8195e26…` / `816fe70e…` / `0f098df2…` / `2b040188…` |
| 备份 | `~/g1_backups/g1_voice_router_20261103_064115_before_p021/`（P0-20 源码 + 二进制 `e4eda4bb…`） |

回滚（只影响 Router）：停 `g1-voice-router.service` → 覆盖二进制 → 校验 SHA256 → 启动单服务。

## 8. 阶段 G：软著材料

`software_copyright/`：01 软件基本信息、02 设计说明书、03 系统架构说明、04 模块功能说明、05 数据库与配置说明、06 安装部署手册、07 用户操作手册、08 软件测试报告、09 版本说明、10 源码结构清单、`diagrams/architecture.md`（Mermaid：总体架构、状态通路、交互状态机、部署拓扑）。

所有功能描述均对应真实源码；未写入导航、人脸识别、视觉识别等未实现能力。

## 9. 生产状态与安全

| 服务 | PID | 状态 |
|---|---:|---|
| g1-robot-state-provider | 1379 | active |
| g1-tts | 1380 | active |
| g1-voice-router | **11283** | active（P0-21 最终版） |
| g1-asr-bridge | 1382 | active |
| g1-action-executor | 1383 | active |

- 本阶段只重启过 `g1-voice-router.service`；其他四节点 PID 全程未变。
- systemd unit、logrotate、DDS、Ollama、ASR/TTS/Executor 源码均未修改。
- 本阶段真实动作次数：**0**（wave 于 P0-20.1 已完成验收，本轮未重复）。
- 回滚：未触发。

## 10. 遗留问题

1. **ASR 专有名词误识仍可能新变体**：已覆盖"脑急接口""脑筋接口"，但 ASR 变体不封闭。建议演示时用完整说法（"介绍一下脑机接口"），或持续按证据扩充词典（禁止模糊匹配）。
2. 动作结果无请求 ID，同名动作不可并发（当前靠串行 + 不重试缓解）。
3. IMU 类问题只回答"数据接收/时效"，不做传感器健康结论（设计约束，非缺陷）。
4. 其余 5 个白名单动作（wave_low/hands_up/clap/heart/victory）尚未真机逐一验收。
5. `src/intent_recognizer_test.cpp` 为历史遗留副本，不参与构建。
6. 生产 Router 每次改动都需单服务重启；重启期间约 1–2 秒无响应。
