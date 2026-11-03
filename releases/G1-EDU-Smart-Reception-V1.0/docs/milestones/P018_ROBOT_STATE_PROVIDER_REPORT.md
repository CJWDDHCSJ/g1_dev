# P0-18 RobotStateProvider 真实状态感知模块 · 交付报告

- 日期：2026-11-02（开发于 11-01，G1 重启后于 11-02 完成隔离域验证）
- 结论：**C++17 RobotStateProvider 已实现并在 G1 ARM64 编译通过；状态聚合、分字段有效性、STALE 超时、Voice Router 本地状态回答全部经 DOMAIN_ID=77 实测验证；生产环境未切换（且当前机器人上没有任何接待节点在运行）。**

## A. G1 重启情况（重要环境变化）

- 审计开始时 G1 已重启：`uptime` 8 分钟，`/ros_bridge` 在、其余接待节点（ASR 8247 / TTS 8248 / Router 15789 / Executor 8880）**全部不存在**。
- 重启清空 `/tmp`（tmpfs），导致 P0-18 第一轮部署的 Mock 脚本丢失 → 首轮 B 场景出现 `SOURCE_MISSING`，重建脚本后恢复正常。**该失败是环境原因，不是 Provider 逻辑缺陷。**
- 磁盘源码、`build/`、`install/` 未受影响：重启后直接 `colcon test` 仍 4 个测试全通过。
- 当前仍**没有**任何接待生产节点在运行；本阶段**没有启动生产系统**。

## B. BMS SOC 稳定性（本轮 60 秒连续采样）

| 指标 | 实测 |
|---|---|
| 采样时长 / 样本数 | 60 秒 / **1229 条**（≈20.5Hz，符合 /lf/bmsstate 实测频率） |
| SOC min / max / 均值 / 中位数 | 36 / 36 / 36.00 / 36 |
| 唯一值数量 / 变化次数 / 最大相邻跳变 | 1 / 0 / 0 |
| 断流次数 | 0 |
| SOH | 94 |
| 电池温度 | 原始数组 `[29,27,26,31]`（另有 8 个 0 空通道）；**单位未确认，不对外播报** |
| 电流 | `current=-2257`，**单位/符号未确认，不对外播报** |
| 电压 | `bmsvoltage[0]=50173mV`，13 串电芯均值 3860mV → 交叉验证一致 |

跨小时对照：07:00 soc=73/50.17V → 07:40 soc=66/50.17V → 11:00 soc=35/46.86V（电芯均值 3609.5mV，13×3.6095=46.92V 与总压吻合）。**结论：短窗口完全稳定、跨小时单调下降与电压一致，属真实待机放电（约 12%/h），SOC 可作为电量百分比对外使用。** 已内置跳变保护：60 秒内变化 ≥15 个百分点判 `INVALID(UNSTABLE_SOC_JUMP)`。

## C. RobotStateProvider 架构

```text
/lf/bmsstate ─┐
/lowstate ────┤→ RobotStateProvider(节点 /g1_robot_state_provider)
/sportmodestate┤    ├─ RobotStateEvaluator（纯逻辑，毫秒时钟注入，无 ROS 依赖）
/arm/action/state┘    ├─ 分字段缓存 + 分话题超时
                      └─ 2Hz 发布 /g1/robot_state (std_msgs/String, JSON schema_version=1)

/g1/robot_state → Voice Router ROBOT_STATUS 分支 → 本地模板回答 → /g1/tts
```

- 新增文件：`include/g1_voice_router/robot_state_types.hpp`、`robot_state_evaluator.hpp`、`robot_state_provider.hpp`；`src/robot_state_evaluator.cpp`、`robot_state_provider.cpp`、`robot_state_provider_node.cpp`；`test/robot_state_evaluator_test.cpp`。
- 修改：`src/voice_router.cpp`（订阅 `/g1/robot_state` + 状态感知 ROBOT_STATUS 分支）、`CMakeLists.txt`、`package.xml`（新增 `unitree_hg`）。
- 未订阅 `/dog_odom`（1kHz，且当前无对外位姿回答需求，按"最小必要订阅"原则排除）。
- 订阅 QoS：BEST_EFFORT（与原生 RELIABLE 发布端兼容，P0-17 实测确认）。
- JSON：数值仅在 VALID 时输出，其余为 `null`（禁止用 0 伪装测量值）；每字段独立 `status / received_age_ms / quality_reason / source_topic`。
- 超时（代码常量，非猜测）：/lowstate 1000ms、/sportmodestate 1000ms、/arm/action/state 1000ms、/lf/bmsstate 2000ms；Router 侧缓存有效期 5000ms（Provider 2Hz）。
- 时间基准：`std::chrono::steady_clock`（单调），不使用可能跳变的墙上时间。

## D. 字段有效性

| 字段 | 数据源 | 单位 | 状态 | 能否对外回答 |
|---|---|---|---|---|
| 电量 SOC | /lf/bmsstate.soc | % | VALID（60s 稳定 + 电压交叉验证） | ✅ 可播报百分比 |
| SOH | /lf/bmsstate.soh | %（推断） | VALID | ⚠ 存入 JSON，暂未单独播报 |
| 电池总电压 | bmsvoltage[0] | mV（交叉验证） | VALID | 可（暂未播报） |
| 电流 | current | **未确认** | 采集但不输出 | ❌ |
| 电池温度 | temperature[] | **未确认** | 采集但不输出 | ❌ |
| IMU 数据可用性 | /lowstate.imu_state | 状态指标 | VALID | ✅ 仅描述接收情况 |
| 电机温度 | motor_state[].temperature | **未确认** | 原始值内部诊断 | ❌ 一律兜底 |
| 动作状态 | /arm/action/state | JSON id/name/holding | VALID | ✅ id=0 空闲；id≠0 播报真实 name（不解析 FSM） |
| 里程计 | /dog_odom | — | 未接入 | ❌ |

## E. Voice Router 集成

- ROBOT_STATUS 分支先读缓存（>5s 视为 Provider 掉线）→ 解析 JSON（schema_version=1）→ 按问题子类型回答；任一环节失败都返回本地安全兜底，**任何路径都不进 Ollama**。
- 子类型回答：电量（VALID 播报"百分之N"；STALE/INVALID/UNAVAILABLE 兜底）；IMU（仅陈述接收情况 + 明确"未完成健康诊断"）；动作（idle/执行中/releasing/STALE/未知）；温度（恒定兜底，不播报数值）；运行状态（汇总真实可用字段，全不可用则通用兜底）。
- GENERAL_QA / LAB_INTRO / PRICE 路径未改动。

## F. 测试结果

### 单元测试（G1 ARM64 直跑 + colcon）

| 套件 | 结果 |
|---|---|
| RobotStateEvaluator（新增） | **44/44** |
| IntentRecognizer（回归） | **75/75** |
| FactRiskGuard（回归） | **119/119** |
| KnowledgeBase（回归） | **57/57** |
| colcon test-result | **4 tests, 0 errors, 0 failures, 0 skipped** |

- `colcon build --packages-select g1_voice_router --symlink-install`：ARM64 通过（首次全量 50.1s），`install/` 含 `voice_router` 与 `robot_state_provider`。
- 开发中两次真实失败均已定位（非测试规避）：① INVALID 被误判 UNAVAILABLE（`update_bms` 早退未标记 received）→ 修正为"收到即标记数据源存在"；② `state==nullptr` 时统一通用兜底 → 改为按问题子类型返回对应"无法确认"回答。

### DOMAIN_ID=77 运行时（Mock 状态发布 + Ollama Mock 计数）

| 场景 | 输入 | 结果 | Mock Ollama |
|---|---|---|---|
| A 无数据 | 你现在有多少电量 | "我目前无法准确确认电池电量，请以实际设备显示为准。" | 0 |
| B 有效数据(soc=66) | 你现在有多少电量 | "我的电池电量大约是百分之66。" | 0 |
| B | 你现在运行状态怎么样 | "我的核心状态数据正在持续接收。电池电量大约百分之66。IMU数据接收正常。当前没有在执行动作。" | 0 |
| B | 你的IMU数据正常吗 | "我目前能够持续接收到IMU数据，但还没有完成完整的传感器健康诊断。" | 0 |
| B | 你现在正在执行什么动作 | "我目前没有在执行动作，手臂处于空闲状态。" | 0 |
| C STALE | 你现在有多少电量 | "我的电池电量数据已经有一段时间没有更新了，暂时无法准确确认，请以实际设备显示为准。" | 0 |
| C STALE | 你的IMU数据正常吗 | "我的IMU数据已经有一段时间没有更新了，暂时无法确认当前状态。" | 0 |
| C STALE | 你现在正在执行什么动作 | "我的动作状态数据已经过期，暂时无法确认。" | 0 |
| C STALE | 你现在运行状态怎么样 | 通用安全兜底（不报告正常） | 0 |
| 回归 | IMU是什么 | MOCK_OLLAMA_REPLY_1 | 1 |
| 回归 | 机器人如何判断自己的运行状态 | MOCK_OLLAMA_REPLY_2 | 2 |
| 回归 | 脑机接口是什么 | MOCK_OLLAMA_REPLY_3 | 3 |
| 回归 | 宇树G1多少钱 | PRICE 兜底 | 0 |
| 回归 | 实验室当前状态怎么样 | LAB_INTRO → KB 未命中 → 本地兜底 | 0 |

**Mock 最终计数 = 3**，全部为知识问题；ROBOT_STATUS / LAB_INTRO / PRICE 零调用，无意外请求。

### D 部分数据源失效独立性（实测状态快照）

| 场景 | 发布内容 | battery | imu | action | runtime |
|---|---|---|---|---|---|
| A | 仅 low+arm（BMS 停） | **STALE(2960837ms)** | VALID(35ms) | VALID(35ms) | VALID |
| B | 仅 bms+sm（LowState 停） | VALID(56ms) | **STALE(9340ms)** | **STALE(9340ms)** | VALID（sm 仍在发布） |
| C | 仅 bms+low（Action 停） | VALID(11ms) | VALID(9ms) | **STALE(15341ms)** | VALID |

证据 JSON：`evidence_p018/state_A.json / state_B.json / state_C.json / state_stale.json`。

## G. 清理结果

- 精确停止（按 PID，未用宽泛匹配）：测试 Router 3429/3427、Provider 3430/3426、Ollama Mock 3866；Mock 状态发布器按设定时长自行退出。
- `ROS_DOMAIN_ID=77 ros2 node list`：**空**；`ps` 无任何 `robot_state_provider / voice_router / mock` 残留。
- 生产进程：重启后本就不存在，本轮**未启动**，也未触碰 `/ros_bridge`。

## H. 生产环境状态

```text
生产 ASR Bridge：不存在（G1 重启后未启动）
生产 Voice Router：不存在（重启后未启动）
生产 TTS Node：不存在（未启动）
生产 Action Executor：不存在（未启动）
生产 ros_bridge：运行（原生，未触碰）
是否发布真实机器人动作：否
DOMAIN_ID 0 是否启动新 Provider：否
是否修改生产启动配置：否
```

- 部署状态严格区分：**源码已同步 G1（SHA256 双端一致）→ install 已更新（含 robot_state_provider）→ 隔离域验证通过 → 生产未部署运行**。

## I. 变更清单与备份

| 文件 | SHA256（双端一致） |
|---|---|
| src/robot_state_evaluator.cpp | `799d43e4c01220b74feb0a5b031c0aac20f6167a777061f12d35c4fc1daf69d5` |
| src/voice_router.cpp | `fc13bdd605fc0006576d79e46eaa0c3e835c1df160f12fd96ccd52d09f82a2b1` |
| CMakeLists.txt | `ca73b2aabad43f1bcc40e0e81b3a360ae955594112f158daec0802522600e99f` |
| package.xml | `27819e7d4230cafadaae6ac89679d71277a953e4532519d8706c2c7c80cd8ebe` |
| 新增头文件 ×3、provider 源文件 ×2、evaluator 测试 ×1 | 见 G1 目录 |

- G1 备份：`/home/unitree/g1_backups/g1_voice_router_20261101_before_p018/`
- 本地备份：`/home/<user>/Mick_Grey/g1_dev/teach/g1_kb_work/backups/g1_voice_router_20261101_before_p018_local/`
- 证据：`evidence_p018/`（README.md、state_*.json、router/provider/mock 日志、test_result.txt、mock 脚本）
- G1 持久化：`~/g1_logs_p018/`

## J. 未完成事项 / 未验证项

1. **生产未切换**：Provider 与新版 Router 尚未在生产域运行（当前机器人上甚至没有接待节点）。恢复生产需按 P0-16 命令逐个启动 ASR/TTS/Router + 新增 Provider。
2. 电流单位、BMS 温度单位、电机温度单位、motor 索引→关节名映射仍未确认，相应字段一律不对外播报。
3. IMU 仅验证"数据在接收"，未做传感器健康诊断（六面校准/零偏等），回答已明确该限制。
4. 里程计（/dog_odom）未接入 Provider，未做位姿回答。
5. `soh_percent`、`voltage_mv` 已进入 JSON 但未用于语音回答。
6. 真实机器人动作状态（执行中/完成）本轮仅在 Mock 下验证 idle 与执行中映射；未在真机执行动作验证 Provider 的动作播报（P0-16 已有真机 wave 链路记录，但那是执行器侧）。

## 验收对照（任务书十七）

1-3（BMS 复核实、SOC 稳定性、单位未误转换）✅；4-7（Provider 实现、Schema、独立有效性、过期识别）✅；8-11（Router 订阅、真实回答、Provider 不可用兜底、绝不进 Ollama）✅；12-14（GENERAL_QA / LAB_INTRO / PRICE 回归）✅；15-16（新增测试 + 75/119/57 回归）✅；17-18（ARM64 编译、域 77 验证）✅；19（生产域未受影响）✅；20（报告与证据）✅。

唯一未做的是**生产切换**，按任务书要求本阶段不做。
