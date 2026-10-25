# 13 · 示例索引与风险分级

> 本文把 `example/` 下的示例按“能否在 G1-EDU 上用、会不会动机器人、依赖什么”重新整理。它是索引，不替代各模块文档中的 API 语义分析。
>
> **重要**：本次工作没有编译或运行任何示例（只读约束，且分析主机未安装 cmake）。表中“可编译/可运行”均指源码层面的判断，标为【未验证】。

---

## 1. 总览

| 项 | 数量 | 说明 |
|---|---|---|
| 示例分组 | 14 个目录 | `example/CMakeLists.txt:1-14` |
| `.cpp` 文件 | 59 个 | 覆盖 G1/GO2/B2/B2W/H1/H2/R1/A2/AS2/GO2W |
| G1 专用 `.cpp` | 15 个 | 本文重点 |
| G1 辅助头文件 | 5 个 | `gamepad.hpp` ×2、`wav.hpp`、`HelloWorldData` 等不在 G1 |
| 构建条件 | 大多无条件；2 个要 Boost，1 个要 yaml-cpp | `example/g1/CMakeLists.txt:22-41` |
| 会直接动机器人的示例 | 至少 10 个 | 见风险表 |
| 本次实际运行 | **0 个** | 【未验证】 |

---

## 2. 风险分级

| 级别 | 含义 | 示例 |
|---|---|---|
| **R0 只读/无机器人** | 不连接机器人，或只读状态/查询，不发运动指令 | `helloworld`、`jsonize`、`g1_termination`、`wireless_controller`、`g1_arm_action --list` |
| **R1 非运动输出** | 声音/LED，不驱动关节 | `g1_audio_client_example` |
| **R2 肢体动作** | 手/臂/腰，可能夹伤、碰撞 | `g1_arm_action_example`（执行动作时）、`g1_arm5/7_sdk_dds_example`、`g1_hand_sdk_example`、`g1_dex3_example`、`g1d_waist_example` |
| **R3 运动/低层/整机** | 腿、底盘、全身低层命令，可能摔倒/撞人 | `g1_loco_client_example`（运动命令）、`g1_ankle_swing_example`、`g1_userctrl_dds_example`、`g1_dual_arm_example`、`g1_agv_client_example`、`g1d_height_control`、`g1d_arm_example`、`state_machine`（且类型不匹配） |

> R2/R3 示例必须在**支撑/吊装、急停可用、场地隔离**的条件下，由授权人员执行。本文不提供运行步骤。

---

## 3. G1 构建目标与依赖

【源码】`example/g1/CMakeLists.txt`：

| 目标 | 源文件 | 构建条件 | 额外依赖 |
|---|---|---|---|
| `g1_loco_client` | `high_level/g1_loco_client_example.cpp` | 总是 | 无 |
| `g1_arm5_sdk_dds_example` | `high_level/g1_arm5_sdk_dds_example.cpp` | 总是 | 无 |
| `g1_arm7_sdk_dds_example` | `high_level/g1_arm7_sdk_dds_example.cpp` | 总是 | 无 |
| `g1_ankle_swing_example` | `low_level/g1_ankle_swing_example.cpp` | 总是 | 无 |
| `g1_audio_client_example` | `audio/g1_audio_client_example.cpp` | 总是 | 无 |
| `g1_dex3_example` | `dex3/g1_dex3_example.cpp` | 总是 | **Eigen3 头文件**（源码 `:14`；CMake 未显式查找） |
| `g1_userctrl_dds_example` | `high_level/g1_userctrl_dds_example.cpp` | 总是 | 无 |
| `g1_termination` | `low_level/terminations.cpp` | `Boost_FOUND`（`:22-25`） | Boost.ProgramOptions |
| `g1_arm_action_example` | `high_level/g1_arm_action_example.cpp` | `Boost_FOUND`（`:27-28`） | Boost.ProgramOptions |
| `g1_dual_arm_example` | `low_level/g1_dual_arm_example.cpp` | `yaml-cpp_FOUND` 且 ≥0.6（`:31-41`） | yaml-cpp；运行时需 `behavior_lib/motion.seq`（`BLIB_DIR`） |
| `g1_agv_client_example` | `g1d/g1_agv_client_example.cpp` | 总是 | 无 |
| `g1d_arm_example` | `g1d/g1d_arm_example.cpp` | 总是 | 无 |
| `g1d_height_control` | `g1d/g1d_height_control.cpp` | 总是 | 无 |
| `g1d_waist_example` | `g1d/g1d_waist_example.cpp` | 总是 | 无 |
| `g1_hand_sdk_example` | `high_level/g1_hand_sdk_example.cpp` | 总是 | 无 |

结论：

- 想构建**全部** G1 示例：需要 Boost、yaml-cpp、Eigen3 头文件。
- 只构建核心通信/高层客户端示例：不需要 Boost/yaml，但仍可能需要 Eigen（只要包含 `g1_pub.h`/`g1_sub.h`/`terminations.hpp`）。
- `g1_dex3_example` 的 Eigen 依赖没有在 CMake 中声明；缺 Eigen 时会在编译期报错，而不是配置期跳过。

---

## 4. G1 示例明细

### 4.1 高层运动客户端：`g1_loco_client_example`

| 项 | 内容 |
|---|---|
| 源文件 | `example/g1/high_level/g1_loco_client_example.cpp`（272 行） |
| 主要 API | `LocoClient`：`GetFsmId`、`GetFsmMode`、`GetBalanceMode`、`GetSwingHeight`、`GetStandHeight`、`GetPhase`、`SetFsmId`、`SetBalanceMode`、`SetSwingHeight`、`SetStandHeight`、`SetVelocity`、`Damp`、`Start`、`Squat`、`Sit`、`StandUp`、`ZeroTorque`、`StopMove`、`HighStand`、`LowStand`、`BalanceStand`、`ContinuousGait`、`SwitchMoveMode`、`Move`、`SetTaskId`、`ShakeHand`、`WaveHand`、`SetSpeedMode`、`GetMimicMotion` |
| 读写 | 同时包含只读查询与运动会话（`:65-95` 查询，`:105-265` 写入） |
| 网络 | 默认 `"lo"`（`:21`），可用 `--network_interface=eth0` 覆盖（`:24-49`） |
| 超时 | `SetTimeout(10.f)`（`:54`） |
| 风险 | **R3**（含 `Move`、`SetVelocity`、`Damp` 等） |
| 安全用法 | 只传只读参数，例如 `--get_fsm_id`、`--get_stand_height`；不要传 `--move`、`--set_velocity`、`--damp` 等 |
| 已知问题 | `HighStand()`/`LowStand()` 把 `uint32_t` 极值传给 `float` 参数（见 `05_g1_high_level_control.md`）；`SwitchMoveMode` 只改本地变量不 RPC |
| 证据 | 【示例】调用方式；API 语义见 `05` 文档【源码】 |

> 该示例的 CLI 会按传入参数顺序执行多个动作；不要一次传多个运动参数。

### 4.2 手臂动作服务：`g1_arm_action_example`

| 项 | 内容 |
|---|---|
| 源文件 | `example/g1/high_level/g1_arm_action_example.cpp`（85 行） |
| 主要 API | `G1ArmActionClient::GetActionList`（`:47`）、`ExecuteAction(int)`（`:53`）、`ExecuteAction(string)`（`:78`）、`StopCustomAction`（`:81`） |
| 读写 | `--list` 只读；按 id/name 执行会动臂 |
| 参数 | Boost：`--list`、`--id`、`--name`、`--network`（见 `:13-37`） |
| 依赖 | Boost.ProgramOptions |
| 风险 | **R2**（执行动作）/ **R0**（仅 `--list`） |
| 安全用法 | 只用 `--list` 获取动作表；动作 id/name 见 `arm_action_map`（`g1_arm_action_client.hpp:85-102`） |
| 注意 | `StopCustomAction` 只对自定义动作有效；内置动作是否可中断未验证 |

### 4.3 5-DOF 手臂 SDK：`g1_arm5_sdk_dds_example`

| 项 | 内容 |
|---|---|
| 源文件 | `example/g1/high_level/g1_arm5_sdk_dds_example.cpp`（256 行） |
| 话题 | `rt/arm_sdk`（`:11`）、`rt/lowstate`（`:12`） |
| 控制对象 | 左右各 5 个手臂关节 + 3 个腰部关节，共 13 个（`:89-98`） |
| 关键写法 | 在 `rt/arm_sdk` 的 `motor_cmd[29].q` 写权重（`:145`）；权重直接设为 `1.0`（`:144`），并没有按 `weight_rate` 渐变 |
| 风险 | **R2**（手臂/腰），权重瞬间接管可能造成抖动 |
| 注意 | 未设置 `mode_machine`/CRC（示例层面）；索引 29 在 23-DOF 等构型上可能无效 |
| 证据 | 【示例】`05`/`06` 文档有详细分析 |

### 4.4 7-DOF 手臂 SDK：`g1_arm7_sdk_dds_example`

| 项 | 内容 |
|---|---|
| 源文件 | `example/g1/high_level/g1_arm7_sdk_dds_example.cpp`（262 行） |
| 话题 | `rt/arm_sdk`、`rt/lowstate`（`:11-12`） |
| 控制对象 | 左右各 7 个手臂关节 + 3 个腰部关节，共 17 个（`:92-100`） |
| 关键写法 | 同样在 `motor_cmd[29].q` 写权重（`:150`），第一帧就设为 `1.0`（`:149`） |
| 风险 | **R2**（手臂/腰） |
| 注意 | 与 arm5 相同：权重未渐变、未处理 `mode_machine`/CRC、构型相关索引需确认 |

### 4.5 手部 SDK：`g1_hand_sdk_example`

| 项 | 内容 |
|---|---|
| 源文件 | `example/g1/high_level/g1_hand_sdk_example.cpp`（76 行） |
| 话题 | `rt/hand_sdk`（`:27`） |
| 消息 | `unitree_go::msg::dds_::MotorCmds_`（`:17,28-29`） |
| 控制对象 | 4 个手部电机（`:25`） |
| 关键写法 | 权重编码在 `cmds[0].mode = weight*100`（`:34-41`）；力矩 ±0.3 交替（`:68-73`） |
| 风险 | **R2**（夹手/堵转） |
| 注意 | 示例直接 `set_weight(1.0)`（`:66`），无渐变；正/负力矩方向依赖手型与安装 |

### 4.6 灵巧手 Dex3：`g1_dex3_example`

| 项 | 内容 |
|---|---|
| 源文件 | `example/g1/dex3/g1_dex3_example.cpp`（325 行） |
| 话题 | `rt/dex3/left`、`rt/dex3/left/state`（`:32-33`） |
| 消息 | `unitree_hg::msg::dds_::HandCmd_`、`HandState_`（`:34-36`） |
| 控制对象 | 7 个手指电机（`MOTOR_MAX 7`，`:41`） |
| 交互 | 终端非阻塞按键（`:64-82`），状态机 INIT/ROTATE/GRIP/STOP/PRINT（`:17-23`） |
| 依赖 | Eigen3 头文件（`:14`）；CMake 未显式查找 |
| 风险 | **R2**（手指） |
| 注意 | 示例只控制左手；另有 `rt/lf/dex3/...` 命名不一致问题（见 `06` 文档） |

### 4.7 用户低层控制：`g1_userctrl_dds_example`

| 项 | 内容 |
|---|---|
| 源文件 | `example/g1/high_level/g1_userctrl_dds_example.cpp`（248 行） |
| 话题 | `rt/user_lowcmd`、`rt/lowstate`（见 `06` 文档） |
| 关键条件 | 必须 `fsm_id == 1`（PASSIVE）才继续（`:95-99`） |
| 关键写法 | 先发零 kp/kd 帧（`:136-144`）→ `SwitchToUserCtrl`（`:146`）→ 控制循环 → `SwitchToInternalCtrl(LAST)`（`:243`） |
| 风险 | **R3**（全身 29 关节低层） |
| 注意 | 示例未设置 CRC/mode；`rt/user_lowcmd` 的完整契约未验证；不要把它当作生产模板 |

### 4.8 踝关节摆动：`g1_ankle_swing_example`

| 项 | 内容 |
|---|---|
| 源文件 | `example/g1/low_level/g1_ankle_swing_example.cpp`（371 行） |
| 话题 | `rt/lowcmd`、`rt/lowstate`、`rt/secondary_imu`（`:18-20`） |
| 服务 | `MotionSwitcherClient`（`:185`）释放运动服务 |
| 关键写法 | `mode_machine` 取自 lowstate（`:84-87`）；500 Hz 线程（`:202-203`）；CRC（`:300-301`） |
| 风险 | **R3**（全身低层，可能摔倒） |
| 注意 | 需要先释放 motion switcher；没有急停/看门狗；仅用于理解低层链路 |

### 4.9 双臂轨迹：`g1_dual_arm_example`

| 项 | 内容 |
|---|---|
| 源文件 | `example/g1/low_level/g1_dual_arm_example.cpp`（407 行） |
| 依赖 | yaml-cpp；运行时 `BLIB_DIR/motion.seq`（`:217-226`；CMake `:37`） |
| 话题/服务 | `rt/lowcmd`、`rt/lowstate`；`MotionSwitcherClient`（`:176-193`） |
| 关键写法 | 2 ms 周期双线程（`:210-214`）；29 电机 kp/kd（`:284-304`）；CRC（`:301-302`）；无限循环（`:404`） |
| 风险 | **R3**（全身，含腿） |
| 注意 | 轨迹文件格式与关节顺序强耦合；`motion.seq` 缺失会抛异常；无安全退出 |

### 4.10 音频：`g1_audio_client_example`

| 项 | 内容 |
|---|---|
| 源文件 | `example/g1/audio/g1_audio_client_example.cpp`（175 行） |
| API | `GetVolume`（`:103`）、`SetVolume`（`:106`）、`TtsMaker`（`:110,115`）、`PlayStream`（`:147`）、`PlayStop`（`:153`）、`LedControl`（`:160-164`） |
| 依赖 | 无额外库；读取 `test.wav` |
| 风险 | **R1**（声音/LED） |
| 注意 | `PlayStop` 忽略返回值，总是返回 0（见 `05` 文档）；音量被设为 100 |

### 4.11 AGV 底盘：`g1_agv_client_example`

| 项 | 内容 |
|---|---|
| 源文件 | `example/g1/g1d/g1_agv_client_example.cpp`（53 行） |
| API | `AgvClient::Move`（`:37`）、`HeightAdjust`（`:45`） |
| 网络 | `Init(0)` 不指定网卡（`:15`） |
| 行为 | 无限正弦周期运动，无停止条件（`:28-50`） |
| 风险 | **R3（最高）**：底盘会持续移动 |
| 注意 | 仅适用于带 AGV 底盘的 G1D；不要在任何未隔离场地运行 |

### 4.12 G1D 高度控制：`g1d_height_control`

| 项 | 内容 |
|---|---|
| 源文件 | `example/g1/g1d/g1d_height_control.cpp`（184 行） |
| 话题 | `rt/hispeed_state`（`:18`） |
| API | `AgvClient::HeightAdjust`（`:72,125`） |
| 交互 | 手柄/信号处理（`:34`、`gamepad.hpp`） |
| 风险 | **R3**（底盘升降） |
| 注意 | 依赖 G1D 的 hispeed 状态话题；普通 G1 不适用 |

### 4.13 G1D 腰部：`g1d_waist_example`

| 项 | 内容 |
|---|---|
| 源文件 | `example/g1/g1d/g1d_waist_example.cpp`（76 行） |
| 话题 | `rt/waistpitchcmd`、`rt/waistpitchstate`（`:12-13`） |
| 消息 | `unitree_hg::msg::dds_::MotorCmd_` / `MotorState_` |
| 关键写法 | `kp=500`、`kd=12`（`:50-51`）；在 0 与 0.8 rad 之间无限往返（`:60-73`） |
| 风险 | **R3**（腰部大刚度、无斜率限制） |
| 注意 | 仅 G1D；普通 G1 是否有该话题未验证 |

### 4.14 G1D 手臂：`g1d_arm_example`

| 项 | 内容 |
|---|---|
| 源文件 | `example/g1/g1d/g1d_arm_example.cpp`（292 行） |
| 话题 | `rt/lowcmd`、`rt/lowstate`、`rt/secondary_imu`（`:20-22`） |
| 服务 | `MotionSwitcherClient`（`:18`） |
| 关键写法 | 29 电机、手柄控制、双线程；`Init` 在 `:217` |
| 风险 | **R3**（全身低层） |
| 注意 | G1D 专用；普通 G1 的关节定义/构型不同 |

### 4.15 安全判据：`g1_termination`

| 项 | 内容 |
|---|---|
| 源文件 | `example/g1/low_level/terminations.cpp`（46 行） |
| 话题 | `rt/lowstate`（`:25`） |
| API | `g1::bad_orientation`（`:35`）、`g1::lost_connection`（`:38`） |
| 依赖 | Boost.ProgramOptions；`terminations.hpp` 需要 Eigen3 |
| 风险 | **R0**（只读，不运动） |
| 注意 | 只演示两个判据；没有覆盖过温/超速/低电量；`lost_connection` 的 1000 ms 是示例参数 |

---

## 5. 安全的示例学习顺序

> 前提：机器人处于支撑/吊装、急停可用、场地隔离；所有运行均需授权。

1. **R0 通信**：`helloworld` 的 subscriber（不连机器人）→ 确认 DDS 编译/运行链路。
2. **R0 只读**：`g1_termination`（只订阅 `rt/lowstate`）→ 确认网卡/域正确。
3. **R0 查询**：`g1_arm_action_example --list`、`g1_loco_client_example --get_fsm_id` → 确认 RPC 通。
4. **R1 非运动**：`g1_audio_client_example`（音量/TTS/LED）→ 确认服务调用。
5. **R2 单肢体**：手/臂示例必须在空载、低速、有人监护下逐项验证。
6. **R3 运动/低层**：仅在完成前四步、并有专门安全方案后进行；本文不提供运行指导。

---

## 6. 非 G1 示例的参考价值

| 示例 | 能否用于 G1 | 价值 | 说明 |
|---|---|---|---|
| `helloworld` | 可（不连机器人） | **最小 pub/sub 模板** | `publisher.cpp`/`subscriber.cpp`，展示 `ChannelFactory`、`ChannelPublisher`、`ChannelSubscriber` 的最简用法 |
| `wireless_controller` | 可（只读，需验证） | 手柄输入解析 | 订阅 `rt/wirelesscontroller`，`unitree_go::WirelessController_`；G1 是否发布该话题【未验证】，G1 示例使用的是 `LowState_.wireless_remote` |
| `jsonize` | 可（不连机器人） | JSON 序列化工具 | `unitree/common/json/jsonize.hpp`，可用于 RPC 参数构造 |
| `state_machine` | **不可直接用于 G1** | 状态机架构参考 | 使用 `unitree_go::LowCmd_/LowState_`（`robot_controller.hpp:33-58`），与 G1 的 `unitree_hg` 类型不兼容 |
| `go2_video_client` | **不可用于 G1** | 视频 API 参考 | 仅 GO2/B2 的 `videohub` 类服务；G1 无相机 API（见 `07` 文档） |
| `go2_low_level` / `go2_stand_example` | 不可用于 G1 | 低层架构参考 | `unitree_go` 类型与关节布局不同 |
| `h1` / `h2` / `r1` / `b2` / `a2` / `as2` / `go2w` / `b2w` | 不可直接用于 G1 | 各自机器人专用 | 不要混用消息类型与话题契约 |

---

## 7. 示例 → API/话题速查

| 需求 | 参考示例 | 关键 API/话题 |
|---|---|---|
| 最小发布/订阅 | `helloworld` | `ChannelFactory::Init`、`ChannelPublisher::InitChannel/Write`、`ChannelSubscriber::InitChannel` |
| 读关节/IMU | `g1_termination`、`g1_ankle_swing_example` | `rt/lowstate`、`rt/secondary_imu` |
| 读高层状态 | `g1_loco_client_example` | `GetFsmId`、`GetFsmMode`、`GetStandHeight` |
| 高层移动 | `g1_loco_client_example` | `SetVelocity`、`Move`、`StopMove` |
| 手臂动作 | `g1_arm_action_example` | `GetActionList`、`ExecuteAction` |
| 手臂 SDK | `g1_arm5/7_sdk_dds_example` | `rt/arm_sdk` |
| 手部 | `g1_hand_sdk_example` | `rt/hand_sdk` |
| 灵巧手 | `g1_dex3_example` | `rt/dex3/left`、`rt/dex3/left/state` |
| 全身低层 | `g1_ankle_swing_example`、`g1_dual_arm_example` | `rt/lowcmd`、`MotionSwitcherClient` |
| 用户低层 | `g1_userctrl_dds_example` | `rt/user_lowcmd` |
| 音频/LED | `g1_audio_client_example` | `TtsMaker`、`PlayStream`、`LedControl` |
| AGV | `g1_agv_client_example`、`g1d_height_control` | `AgvClient::Move/HeightAdjust` |
| G1D 腰部 | `g1d_waist_example` | `rt/waistpitchcmd` |

---

## 8. 示例中的共性陷阱

| 陷阱 | 涉及示例 | 说明 |
|---|---|---|
| 默认网卡 `"lo"` | `g1_loco_client_example` | 上机器人必须显式覆盖 |
| 不传网卡 | `g1_hand_sdk_example`、`g1_agv_client_example`、`g1_arm_action_example`、`g1_termination` | 多网卡主机可能选错 |
| 权重瞬间置 1 | arm5/arm7/hand | 没有按 `weight_rate` 渐变，可能抖动 |
| 未设 `mode_machine`/CRC | arm5/arm7/userctrl/hand | 与低层 `rt/lowcmd` 契约不同；不要混用 |
| 无退出/停止路径 | AGV、dual arm、waist、G1D arm | `while(true)`，Ctrl-C 后不一定回到安全状态 |
| 无限连续运动 | AGV、loco `Move(..., true)` | 断网/进程崩溃不会自动停 |
| 话题命名不一致 | Dex3 `rt/dex3/*` vs `rt/lf/dex3/*` | 以实机为准 |
| 消息类型不匹配 | `state_machine`（GO2） | G1 必须用 `unitree_hg` |
| 动作列表含同名/重复 id | arm action | 左右 kiss 都是 12（见 `05` 文档） |
| `PlayStop` 忽略返回值 | audio | 总是返回 0，不能据此判断停止成功 |
| 高/低站姿 bug | loco | `HighStand`/`LowStand` 参数类型不匹配 |
| 依赖隐式缺失 | dex3（Eigen）、dual arm（yaml/seq） | 配置阶段不一定报错，编译/运行才暴露 |

---

## 9. 编译/运行验证状态

2026-10-24 在本机用 `g++ 13.3`、`-std=c++17`、SDK `lib/x86_64` 直接编译（未使用 CMake；本机无 `cmake`），仅编译/链接，**未运行任何示例**。

| 项 | 状态 |
|---|---|
| 11 个 G1 目标 | **编译+链接通过**：`g1_loco_client`、`g1_arm5_sdk_dds_example`、`g1_arm7_sdk_dds_example`、`g1_ankle_swing_example`、`g1_audio_client_example`、`g1_userctrl_dds_example`、`g1_hand_sdk_example`、`g1_agv_client_example`、`g1d_arm_example`、`g1d_height_control`、`g1d_waist_example` |
| `g1_dex3_example` | **环境不满足，编译失败**：缺 Eigen3（`eigen3/Eigen/Dense`） |
| `g1_termination` | **环境不满足，编译失败**：缺 Eigen3；本机也未装 Boost.ProgramOptions |
| `g1_arm_action_example` | **环境不满足，编译失败**：缺 `boost/program_options.hpp` |
| `g1_dual_arm_example` | **环境不满足，编译失败**：缺 `yaml-cpp/yaml.h` |
| 文档内完整 C++ 示例 | 8/11 编译通过（01/02/03/05/06/08/10、部署骨架）；04 缺 Eigen3、07 缺 OpenCV、09 缺 ROS 2/rclcpp，均属环境不满足 |
| 任何示例的运行 | 【未验证】未执行（只读约束；示例会连接机器人服务） |
| 示例参数/API 调用 | 【示例】阅读源码确认 |
| API 内部实现 | 【源码】头文件/内联；预编译库内部【未验证】 |
| 实机话题/频率 | 【未验证】需现场测量 |

---

## 10. 未深入分析的示例及原因

| 目录 | 原因 |
|---|---|
| `b2/`、`b2w/`、`a2/`、`as2/`、`go2w/`、`go2/` | 非 G1 机器人；消息类型/服务契约不同；仅在需要对照时引用 |
| `h1/`、`h2/` | 非 G1；H1 README 仅作 PR 模式概念参考 |
| `r1/` | 非 G1；`r1_arm_sdk_dds_example` 直接链接 `fmt`，与 G1 无关 |
| `state_machine/` | 使用 `unitree_go` 类型，不能直接用于 G1；仅作架构参考 |
| `wireless_controller/` | 已作为只读输入参考，未逐行分析手柄映射 |
| `jsonize/` | 工具示例，与机器人控制无关 |

---

## 11. 相关文档

- 高层 API 语义：`05_g1_high_level_control.md`
- 低层话题与 CRC：`06_g1_low_level_control.md`
- 状态与安全判据：`04_robot_state.md`
- 构建依赖：`11_build_and_link.md`
- 示例逐行映射：`example_to_api.md`
