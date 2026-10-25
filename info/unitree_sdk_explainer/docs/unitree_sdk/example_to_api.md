# 示例逐行 → API / 话题映射

> 本文把 `example/g1/` 下 15 个 G1 示例拆成“代码块 → API / 话题 / 消息字段”的映射，用于回答三个问题：
> 1. 示例里的每一行到底调用了 SDK 的什么？
> 2. 哪些行是**安全关键**的（控制权、权重、CRC、线程频率）？
> 3. 抄到生产代码前，哪些行必须替换？
>
> **本文没有编译或运行任何示例**（只读约束）；所有“行为”均为源码阅读结果，运行效果【未验证】。行号基于当前 checkout。
> API 完整语义见 [api_index.md](api_index.md)；风险分级见 [13_examples_index.md](13_examples_index.md)。

---

## 1. 图例与阅读方法

| 标记 | 含义 |
|---|---|
| `API` | 示例调用的 SDK 符号 |
| `Topic` | DDS 话题名 |
| `Msg` | 消息类型 / 字段 |
| ⚠️ | 安全关键或不建议照搬 |
| 🐞 | 示例/头文件中的疑似缺陷 |

示例代码块的“行号”是**起止范围**；同一范围内可能还有参数解析、打印等与 API 无关的行，表中只列关键行。

---

## 2. 所有示例共用的启动骨架

无论高层还是低层，G1 示例都遵循同一顺序：

| 步骤 | 典型代码 | 对应 API | 说明 |
|---|---|---|---|
| 1 | `ChannelFactory::Instance()->Init(0, argv[1])` | `ChannelFactory::Init(int32_t, const std::string&)` | 所有示例都用 **domain 0**；第二个参数是网卡名 |
| 2 | `client.Init()` | `LocoClient::Init` / `ArmActionClient::Init` / ... | 注册该客户端用到的 API ID |
| 3 | `client.SetTimeout(3~10.f)` | `ClientBase::SetTimeout(float)` | 浮点按秒【示例推断】；默认 1 s |
| 4 | `new ChannelPublisher<...>(topic)` + `InitChannel()` | `ChannelPublisher` | 低层/音频订阅才会用到 |
| 5 | `new ChannelSubscriber<...>(topic)` + `InitChannel(handler, 1)` | `ChannelSubscriber` | 绝大多数示例 `queuelen=1`；`terminations.cpp` 用默认 0 |
| 6 | `CreateRecurrentThreadEx(..., 2000, ...)` 或 `while(true)` | `RecurrentThread` | 低层示例用 500 Hz 周期线程；高层示例多为事件驱动 |

**第一个差异点**：`ChannelSubscriber::InitChannel(handler, 1)` 会把回调放到 `rlsnr` 线程；`queuelen=0` 则直接在 DDS 线程回调。示例并不统一，抄代码时要明确选哪种。详见 [08](08_concurrency_and_lifecycle.md) §2。

---

## 3. 高层示例

### 3.1 `g1_loco_client_example.cpp`

文件：`example/g1/high_level/g1_loco_client_example.cpp`（272 行）
风险：**R3**（同时包含只读查询与运动命令）

#### 3.1.1 初始化

| 行号 | 代码 | API / 参数 | 说明 |
|---|---|---|---|
| `:21` | `args = {{"network_interface", "lo"}}` | — | ⚠️ **默认 `"lo"` 回环网卡**，不接机器人时也会“初始化成功” |
| `:48-49` | `ChannelFactory::Instance()->Init(0, args["network_interface"])` | `ChannelFactory::Init` | domain 0；网卡来自 `--network_interface` |
| `:51-54` | `LocoClient client; client.Init(); client.SetTimeout(10.f);` | `g1::LocoClient` 构造 / `Init` / `SetTimeout` | 超时 10 s，比默认 1 s 宽松 |

#### 3.1.2 只读查询（可以安全执行）

| 行号 | 命令参数 | API | 输出 |
|---|---|---|---|
| `:64-69` | `get_fsm_id` | `GetFsmId(int&)` | 当前 FSM ID |
| `:70-75` | `get_fsm_mode` | `GetFsmMode(int&)` | 当前 FSM mode |
| `:76-81` | `get_balance_mode` | `GetBalanceMode(int&)` | 平衡模式 |
| `:82-87` | `get_swing_height` | `GetSwingHeight(float&)` | 摆腿高度 |
| `:88-93` | `get_stand_height` | `GetStandHeight(float&)` | 站立高度 |
| `:94-103` | `get_phase` | `GetPhase(std::vector<float>&)` | 已废弃的步态相位 |
| `:263-268` | `get_mimic_motion` | `GetMimicMotion(std::string&)` | 查询模仿动作列表 |

这些调用都只发 RPC、不改状态；但示例**不检查返回值**，失败时输出的是未初始化变量。生产代码必须检查 `ret`。

#### 3.1.3 写命令（R3，需授权）

| 行号 | 命令参数 | API | 真实语义 / 风险 |
|---|---|---|---|
| `:105` | `set_fsm_id` | `SetFsmId(int)` | 直接切 FSM；参数来自命令行，可能进入未知状态 |
| `:111` | `set_balance_mode` | `SetBalanceMode(int)` | 0/1 之外的参数行为未验证 |
| `:117` | `set_swing_height` | `SetSwingHeight(float)` | 摆腿高度 |
| `:123` | `set_stand_height` | `SetStandHeight(float)` | 站立高度 |
| `:135-147` | `set_velocity` | `SetVelocity(vx,vy,omega,duration)` | 四参数版本可指定持续时间 |
| `:152` | `damp` | `Damp()` → `SetFsmId(1)` | 阻尼模式，可能瞬间卸力 |
| `:156` | `start` | `Start()` → `SetFsmId(500)` | 启动运控 |
| `:160` | `squat` | `Squat()` → `SetFsmId(2)` | 蹲下 |
| `:164` | `sit` | `Sit()` → `SetFsmId(3)` | 坐下 |
| `:168` | `stand_up` | `StandUp()` → `SetFsmId(4)` | 站起 |
| `:172` | `zero_torque` | `ZeroTorque()` → `SetFsmId(0)` | ⚠️ 零力矩，可能直接倒下 |
| `:176` | `stop_move` | `StopMove()` → `SetVelocity(0,0,0)` | 只发 1 s 零速，不是急停 |
| `:180` | `high_stand` | `HighStand()` | 🐞 头文件把 `uint32_t` 最大值传给 `float`（`g1_loco_client.hpp:228`） |
| `:184` | `low_stand` | `LowStand()` | 🐞 头文件传 0（`:230`） |
| `:188` | `balance_stand` | `BalanceStand()` → `SetBalanceMode(0)` | 平衡站 |
| `:201` | `continous_gait` | `ContinuousGait(bool)` → `SetBalanceMode(1/0)` | 连续步态 |
| `:214` | `switch_move_mode` | `SwitchMoveMode(bool)` | 🐞 **只改客户端本地变量，不发 RPC**（`g1_loco_client.hpp:242-245`） |
| `:230` | `move` | `Move(vx,vy,omega)` | 受本地 `continous_move_` 影响，默认 false |
| `:235` | `set_task_id` | `SetTaskId(int)` | 任务 ID 直接来自命令行 |
| `:240` / `:245` | `shake_hand 0/1` | `ShakeHand(0/1)` | 交替发送 task 2/3 |
| `:249` / `:254` | `wave_hand [turn]` | `WaveHand(bool)` | `SetTaskId(0/1)` |
| `:259` | `set_speed_mode` | `SetSpeedMode(int)` | 速度档位 |
| `:265` | `get_mimic_motion` | `GetMimicMotion` | 只读，但要求稳定站立 |

#### 3.1.4 抄到生产代码前必须改

1. 默认网卡 `"lo"` 改为实际网卡，并在 Init 后做“是否收到状态”的检查。
2. 所有 `Get*` 检查返回值；失败时不要使用输出变量。
3. 删除 `HighStand/LowStand/SwitchMoveMode` 的调用，或改用明确的 `SetStandHeight`。
4. `Move(..., true)` 的 864000 s 必须配看门狗；生产代码不应使用无限持续命令。
5. 写命令前先 `GetFsmId` 确认状态，并确认 `motion_switcher`/控制权归属。

---

### 3.2 `g1_arm_action_example.cpp`

文件：`example/g1/high_level/g1_arm_action_example.cpp`
风险：R2（执行动作）/ R0（`--list`）

| 行号 | 代码 | API | 说明 |
|---|---|---|---|
| `:37` | `ChannelFactory::Instance()->Init(0, vm["network"].as<std::string>())` | `ChannelFactory::Init` | 命令行 `--network` |
| `:39-40` | `make_shared<G1ArmActionClient>(); client->Init();` | `G1ArmActionClient` 构造 / `Init` | 服务名 `arm` |
| `:43` | `client->SetTimeout(10.f)` | `SetTimeout` | 自定义动作可能超过 10 s，注释说明 |
| `:47` | `client->GetActionList(action_list_data)` | `GetActionList` | 只读；返回 JSON 文本 |
| `:53` | `client->ExecuteAction(vm["id"].as<int>())` | `ExecuteAction(int32_t)` | 预设动作；99=释放 |
| `:55-73` | `switch(ret)` 错误码分支 | `UT_ROBOT_ARM_ACTION_ERR_*` | 7400/7401/7402/7404 的处理示例 |
| `:78` | `client->ExecuteAction(vm["name"].as<std::string>())` | `ExecuteAction(const std::string&)` | 自定义动作；名称直接拼 JSON |
| `:81` | `client->StopCustomAction()` | `StopCustomAction` | 停止自定义动作 |

注意：

- `ExecuteAction(string)` 内部用字符串拼接构造 JSON（`g1_arm_action_client.hpp:69`），**动作名含引号/反斜杠会破坏 JSON**。生产代码应自己做转义或改用 ID。
- 7401 的含义是“手臂正在 holding”，不是通信失败；先发 99 或同一动作释放。
- 动作是否成功要看 `rt/arm/action/state`，不是看 `ret==0`。

---

### 3.3 `g1_arm5_sdk_dds_example.cpp`

文件：`example/g1/high_level/g1_arm5_sdk_dds_example.cpp`
风险：R2；控制 13 个上肢关节（5-DOF 手臂 + 腰）

| 行号 | 代码 | API / Topic / Msg | 说明 |
|---|---|---|---|
| `:11-12` | `kTopicArmSDK="rt/arm_sdk"`、`kTopicState="rt/lowstate"` | Topic | 手臂接管 + 状态 |
| `:65` | `ChannelFactory::Instance()->Init(0, argv[1])` | `ChannelFactory::Init` | 必须传网卡 |
| `:67-74` | `ChannelPublisher<LowCmd_>(kTopicArmSDK); InitChannel()` | `ChannelPublisher<LowCmd_>` | 发布 `unitree_hg::LowCmd_` |
| `:76-87` | `ChannelSubscriber<LowState_>(kTopicState); InitChannel(lambda, 1)` | `ChannelSubscriber<LowState_>` | 回调里 `memcpy` 整个状态 |
| `:89-98` | `std::array<JointIndex,13> arm_joints` | `JointIndex`（`dds_wrapper/robots/g1/defines.h`） | 10 个手臂 + 3 个腰 |
| `:104-107` | `weight=0; weight_rate=0.2;` | — | 声明了渐变速率 |
| `:109-121` | `kp=60; kd=1.5; dq=0; tau_ff=0; control_dt=0.02; max_joint_velocity=0.5` | — | 控制参数；`kp/kd` 是否适合你的负载需实测 |
| `:130-143` | 读取 `state_msg.motor_state().at(joint).q()` | `LowState_::motor_state()[i].q()` | 当前关节角 |
| `:144-145` | `weight=1.0; msg.motor_cmd().at(kNotUsedJoint).q(weight);` | `LowCmd_` 索引 29 | ⚠️ **权重瞬间从 0 跳到 1**，没有按 `weight_rate` 渐变 |
| `:151-155` | `q/dq/kp/kd/tau` 赋值 | `MotorCmd_` 字段 | 13 个关节 |
| `:159` | `arm_sdk_publisher->Write(msg)` | `ChannelPublisher::Write` | 每 20 ms 一次 |
| `:189-193` / `:214-218` | 目标位置插值 | `MotorCmd_::q()` | 梯形/线性插值由示例自己实现 |
| `:239` | `msg.motor_cmd().at(kNotUsedJoint).q(weight)` | 权重 | 释放阶段递减 |
| `:249-251` | `weight=0; Write(msg)` | 权重 0 | 交还控制权 |

#### 关键问题

1. **权重跳变**：示例声明 `weight_rate`、`delta_weight`，但初始化循环里直接 `weight=1.0`。正确做法是从 0 按速率递增，否则手臂会瞬间被接管。
2. **未校验 `mode_machine`**：示例不检查机器人的 `mode_machine` 与消息里的 `mode_machine` 是否一致；`dds_wrapper` 的 `publisher::LowCmd::check_mode_machine` 提供了这个检查，但本示例没用。
3. **无超时释放**：如果进程崩溃，`rt/arm_sdk` 停止发送后机器人行为取决于内部服务，示例没有兜底。
4. **`q` 插值基于旧状态**：`state_msg` 由回调持续更新，但初始化循环读取时没有加锁；数据竞争风险。

---

### 3.4 `g1_arm7_sdk_dds_example.cpp`

文件：`example/g1/high_level/g1_arm7_sdk_dds_example.cpp`
风险：R2；控制 17 个关节（7-DOF 手臂 + 腰）

结构与 3.3 相同，差异：

| 行号 | 差异 | 说明 |
|---|---|---|
| `:68` | `Init(0, argv[1])` | 同 |
| `:70-77` | `rt/arm_sdk` publisher | 同 |
| `:79-90` | `rt/lowstate` subscriber，`queuelen=1` | 同 |
| `:92-100` | `std::array<JointIndex,17>` | 左右各 7 + 腰 3 |
| `:103-116` | 控制参数 | 同 |
| `:149-150` | `weight=1.0` 直接置位 | ⚠️ 同样存在权重跳变 |
| `:164` / `:202` / `:227` / `:247` / `:257` | `Write(msg)` | 初始化、保持、目标、释放各阶段 |

结论与 3.3 相同：**不要直接照搬权重置 1 的写法**。

---

### 3.5 `g1_hand_sdk_example.cpp`

文件：`example/g1/high_level/g1_hand_sdk_example.cpp`
风险：R2；4 个手部电机

| 行号 | 代码 | API / Topic / Msg | 说明 |
|---|---|---|---|
| `:27-31` | `ChannelPublisher<MotorCmds_>("rt/hand_sdk"); InitChannel(); cmds().resize(4)` | `ChannelPublisher<unitree_go::msg::dds_::MotorCmds_>` | 注意是 **GO2 命名空间**的消息 |
| `:34-41` | `weight()` / `set_weight(float)` | `MotorCmds_::cmds()[0].mode()` | 权重编码为 `uint8(weight*100)`，范围 0–100 |
| `:43-48` | `set_tau(float)` | `cmds()[i].tau()` | 正力矩闭合，负力矩张开 |
| `:50` | `write()` | `ChannelPublisher::Write` | 无 CRC |
| `:59-63` | `Init(0)` 或 `Init(0, argv[1])` | `ChannelFactory::Init` | 无参数时使用默认网卡【未验证】 |
| `:65-66` | `HandSdk hand_sdk; set_weight(1.0f)` | — | ⚠️ **瞬间 100% 接管**，没有渐变 |
| `:68-74` | 每秒翻转 `tau=±0.3` | — | 无限循环；没有超时/退出路径 |

#### 关键问题

1. **权重编码**：`cmds[0].mode` 是 `uint8`，权重 1.0 → 100；不要写成 `mode=1`（那是 1% 权重）。
2. **无渐变**：手部会瞬间从内部控制切到 SDK 控制。
3. **无退出条件**：`while(true)` + 1 s 周期；生产代码必须响应 SIGINT 并把权重降回 0。
4. **力矩方向/幅值**：`±0.3` 只是示例值，不同手型/负载需要实测；不要当安全上限。

---

### 3.6 `g1_dex3_example.cpp`

文件：`example/g1/dex3/g1_dex3_example.cpp`
风险：R2；7 个手指电机

| 行号 | 代码 | API / Topic / Msg | 说明 |
|---|---|---|---|
| `:14` | `#include <eigen3/Eigen/Dense>` | Eigen | 编译需要 Eigen3 头文件（CMake 未显式查找） |
| `:32-33` | `dds_namespace="rt/dex3/left"; sub_namespace="rt/dex3/left/state"` | Topic | 默认左手 |
| `:34-37` | 全局 publisher/subscriber/msg/state | — | 全局变量，非线程安全 |
| `:41-42` | `MOTOR_MAX 7`、`SENSOR_MAX 9` | — | 7 个电机、9 个触觉传感器 |
| `:105-135` | `rotateMotors()`：构造 `mode`、`q/dq/kp/kd/tau`、正弦目标 | `HandCmd_::motor_cmd()` | `mode` 位域 `id/status/timeout` 由示例自行拼接 |
| `:135` | `handcmd_publisher->Write(msg)` | `ChannelPublisher::Write` | 无 CRC |
| `:184-205` | `stopMotors()`：`kp=0, kd=0, q=0` | `HandCmd_` | 停止方式；`timeout=1` |
| `:250-257` | 选择 L/R，设置 `dds_namespace` / `sub_namespace` | Topic | ⚠️ 状态话题用 `rt/lf/dex3/...`，与 `g1_sub.h` 的 `rt/dex3/...` 不一致 |
| `:268` | `Init(0, argv[1])` | `ChannelFactory::Init` | 必须传网卡 |
| `:269-273` | `ChannelPublisher<HandCmd_>(dds_namespace+"/cmd")`、`ChannelSubscriber<HandState_>(sub_namespace)`、`InitChannel(handler,1)` | 通道 API | 命令与状态使用不同命名空间 |
| `:274-276` | `state.motor_state().resize(7); state.press_sensor_state().resize(9); msg.motor_cmd().resize(7)` | 消息字段 | 变长数组需手动 resize |
| `:281` | `std::thread inputThread(userInputThread)` | 标准库线程 | 没有 join/detach 管理 |
| `:283` | `while(true)` 主循环 | — | 无退出路径 |

#### 关键问题

1. **话题不一致**：`g1_sub.h` 用 `rt/dex3/left/state`，示例运行时用 `rt/lf/dex3/left/state`。哪个是实际固件话题【未验证】；部署前必须用 `dds` 工具确认。
2. **模式位域是手写的**：`RIS_Mode_t` 的位布局（`:45-49`）来自示例，不是 SDK 公共 API；不同固件版本可能变化。
3. **无 CRC/无超时**：`HandCmd_` 没有 CRC 字段；`timeout` 位是否被固件执行【未验证】。
4. **Eigen 依赖**：只用于打印状态（`:209`），却让整个示例依赖 Eigen。

---

### 3.7 `g1_userctrl_dds_example.cpp`

文件：`example/g1/high_level/g1_userctrl_dds_example.cpp`
风险：**R3**；全身 29 关节用户控制

| 行号 | 代码 | API / Topic / Msg | 说明 |
|---|---|---|---|
| `:13-14` | `kTopicUserCtrl="rt/user_lowcmd"`、`kTopicState="rt/lowstate"` | Topic | 用户控制通道 |
| `:68` | `Init(0, argv[1])` | `ChannelFactory::Init` | 必须传网卡 |
| `:70-77` | `ChannelPublisher<LowCmd_>(kTopicUserCtrl); InitChannel()` | `ChannelPublisher<LowCmd_>` | 发布全身命令 |
| `:79-90` | `ChannelSubscriber<LowState_>(kTopicState); InitChannel(lambda,1)` | `ChannelSubscriber` | `memcpy` 状态 |
| `:92-94` | `LocoClient client; Init(); SetTimeout(5.f)` | `LocoClient` | 用于查询/切换控制权 |
| `:95-100` | `GetFsmId(current_fsm_id); if != 1 exit` | `GetFsmId` | **安全检查**：必须处于 PASSIVE（FSM 1） |
| `:129-136` | 29 个关节 `q=0,dq=0,kp=0,kd=1.5,tau=0`，然后 `Write` | `LowCmd_` | ⚠️ 先发零增益命令，避免瞬间出力 |
| `:138` | `client.SwitchToUserCtrl()` | `SwitchToUserCtrl` | 请求用户控制（发 `{"data":false}`） |
| `:140-147` | 读取当前关节角 | `LowState_` | 从状态消息取初值 |
| `:154-173` | 2 s 插值到 `init_pos` | `LowCmd_` | `phase` 线性插值 |
| `:235` | `Write(msg)` | `ChannelPublisher::Write` | 20 ms 周期 |
| `:243` | `SwitchToInternalCtrl(InternalFsmMode::LAST)` | `SwitchToInternalCtrl` | 结束前交还控制权 |

#### 关键问题

1. **`SwitchToUserCtrl` 的参数语义不明**：头文件发的是 `{"data":false}`（`g1_loco_client.hpp:186-192`），但示例不检查返回值，也不验证是否真的获得控制权。
2. **只检查 FSM=1，不检查模式机/控制权冲突**：如果其他进程仍在发 `rt/lowcmd`，行为未定义。
3. **无看门狗**：主循环结束后直接 `SwitchToInternalCtrl`；如果进程崩溃，29 个关节的命令会停在最后一帧，机器人内部是否超时接管【未验证】。
4. **`state_msg` 数据竞争**：回调线程写、主线程读，没有锁或原子保护。

---

### 3.8 `g1_ankle_swing_example.cpp`

文件：`example/g1/low_level/g1_ankle_swing_example.cpp`
风险：**R3**；全身低层 + 踝关节摆动

| 行号 | 代码 | API / Topic / Msg | 说明 |
|---|---|---|---|
| `:18-20` | `rt/lowcmd`、`rt/secondary_imu`、`rt/lowstate` | Topic | 三个通道 |
| `:165-170` | publisher/subscriber/`MotionSwitcherClient` 成员 | — | 类封装 |
| `:180` | `Init(0, networkInterface)` | `ChannelFactory::Init` | 必须传网卡 |
| `:183-185` | `msc_=make_shared<MotionSwitcherClient>(); SetTimeout(5); Init()` | `b2::MotionSwitcherClient` | 释放内置运控 |
| `:186-191` | `while (CheckMode(form,name), !name.empty()) { ReleaseMode(); sleep(5); }` | `CheckMode` / `ReleaseMode` | ⚠️ **死循环直到模式名为空**；失败会永久卡住 |
| `:194-195` | `ChannelPublisher<LowCmd_>(rt/lowcmd); InitChannel()` | `ChannelPublisher` | 全身低层命令 |
| `:197-198` | `ChannelSubscriber<LowState_>(rt/lowstate); InitChannel(handler,1)` | `ChannelSubscriber` | 状态 |
| `:199-200` | `ChannelSubscriber<IMUState_>(rt/secondary_imu); InitChannel(handler,1)` | `ChannelSubscriber` | 副 IMU |
| `:202-203` | `CreateRecurrentThreadEx("command_writer",...,2000,...)`、`("control",...,2000,...)` | `CreateRecurrentThreadEx` | **500 Hz**（2000 µs） |
| `:213-215` | `LowStateHandler` 校验 CRC | `LowState_::crc()` | 示例自己做 CRC 校验 |
| `:284-302` | `LowCommandWriter`：填 `mode_pr`、`mode_machine`、29 个电机字段，算 CRC，`Write` | `LowCmd_` / `crc32_core` | 每个周期重算 CRC |
| `:300-301` | `crc() = Crc32Core((uint32_t*)&cmd, (sizeof>>2)-1)` | `Crc32Core`（示例自己的函数） | 与 `dds_wrapper::crc32_core` 算法一致【推断】 |
| `:305+` | `Control()`：读状态、写 `motor_command_buffer_` | — | 双缓冲由示例自己实现 |

#### 关键问题

1. **释放模式死循环**：`ReleaseMode()` 失败时只打印，不退出；`sleep(5)` 是秒级，现场会表现为“启动卡住”。
2. **500 Hz 控制**：`CreateRecurrentThreadEx` 只保证周期，不保证实时性；Linux 非 RT 内核下抖动可能很大。
3. **`mode_machine` 来源**：示例从 `LowState_` 更新；若第一帧还没到就发命令，`mode_machine_` 为 0，可能被固件拒绝。
4. **无急停**：示例没有监听手柄/键盘的紧急退出，Ctrl+C 后命令停止，但机器人是否保持最后姿态取决于固件。
5. **CRC 函数**：示例用自己的 `Crc32Core`；生产代码应统一用 `dds_wrapper/common/crc.h` 的 `crc32_core`，避免算法漂移。

---

### 3.9 `g1_dual_arm_example.cpp`

文件：`example/g1/low_level/g1_dual_arm_example.cpp`
风险：**R3**；全身低层 + 预编排动作

| 行号 | 代码 | API / Topic / Msg | 说明 |
|---|---|---|---|
| `:1` | `#include <yaml-cpp/yaml.h>` | yaml-cpp | 构建需要 yaml-cpp ≥ 0.6 |
| `:16-17` | `b2::MotionSwitcherClient` | — | 释放内置运控 |
| `:19-25` | `rt/lowcmd`、`rt/lowstate`、`G1_NUM_MOTOR=29` | Topic / 常量 | — |
| `:28-60` | `DataBuffer<T>` 模板 | — | 示例自建双缓冲，不是 SDK API |
| `:170-176` | 构造：`Init(0, networkInterface)` | `ChannelFactory::Init` | — |
| `:178-180` | `msc->SetTimeout(5); msc->Init()` | `MotionSwitcherClient` | — |
| `:182-193` | `while(queryMotionStatus()) { ReleaseMode(); sleep(5); }` | `ReleaseMode` | ⚠️ 同样是死循环 |
| `:195` | `loadBehaviorLibrary("motion")` | yaml-cpp | 读取 `BLIB_DIR/motion.seq`，不是 SDK API |
| `:198-200` | `ChannelPublisher<LowCmd_>(rt/lowcmd); InitChannel()` | `ChannelPublisher` | — |
| `:203-207` | `ChannelSubscriber<LowState_>(rt/lowstate); InitChannel(handler,1)` | `ChannelSubscriber` | — |
| `:210-214` | 两个 2000 µs 周期线程 | `CreateRecurrentThreadEx` | 500 Hz |
| `:217-225` | `YAML::LoadFile(...)` 解析 `components[1]` | — | 依赖外部行为库文件 |
| `:284-304` | `LowCommandWriter`：填 29 关节 + CRC + `Write` | `LowCmd_` / `crc32_core` | — |
| `:395-404` | `main` 构造后 `while(true) sleep(10);` | — | 无退出路径 |

#### 关键问题

1. **外部依赖**：没有 `motion.seq` 时构造会抛异常/崩溃；该文件不在 SDK 内。
2. **死循环释放模式**：与 3.8 相同。
3. **`components[1]` 硬编码**：YAML 结构变化会直接越界/抛异常。
4. **无安全退出**：`while(true)` + 析构不会执行，`RealTimePublisher` 的清理逻辑（如果用了）也不会跑。

---

### 3.10 `g1_audio_client_example.cpp`

文件：`example/g1/audio/g1_audio_client_example.cpp`
风险：R1（声音/LED）；但包含原始 socket 组播录音

| 行号 | 代码 | API / Topic / Msg | 说明 |
|---|---|---|---|
| `:11-19` | WAV 路径、`rt/audio_msg`、组播 `239.168.123.161:5555`、chunk 96000 | 常量 | 音频文件路径是相对路径 |
| `:22-25` | `asr_handler` 打印 `String_::data()` | `std_msgs::msg::dds_::String_` | 订阅 ASR 文本 |
| `:47-79` | 原始 UDP 组播录音，写 `record.wav` | POSIX socket | **不是 SDK API**；绑定 `192.168.123.*` |
| `:91` | `Init(0, argv[1])` | `ChannelFactory::Init` | 必须传网卡 |
| `:92-94` | `AudioClient client; Init(); SetTimeout(10)` | `g1::AudioClient` | 服务名 `voice` |
| `:97-99` | `ChannelSubscriber<String_>("rt/audio_msg"); InitChannel(asr_handler)` | `ChannelSubscriber` | 默认 `queuelen=0`，回调在 DDS 线程 |
| `:103-107` | `GetVolume` / `SetVolume(100)` | `AudioClient` | — |
| `:110-120` | `TtsMaker(中文,0)`、`TtsMaker(英文,1)` | `TtsMaker` | speaker_id 0=自动播放？示例注释如此写 |
| `:126-147` | 读 WAV、按 96000 字节分块 `PlayStream("example", stream_id, chunk)` | `PlayStream` | 要求 16 kHz/单声道 |
| `:153` | `PlayStop(stream_id)` | `PlayStop` | 🐞 **把 `stream_id` 当 `app_name` 传入**；且 `PlayStop` 忽略 RPC 返回值，永远返回 0 |
| `:160-164` | `LedControl(0,255,0)` 等 | `LedControl` | — |
| `:168-170` | `std::thread mic_t(thread_mic); while(1)` | 标准库线程 | 无退出路径 |

#### 关键问题

1. **`PlayStop` 参数错误**：`PlayStream` 用 `app_name="example"`，停止时却传 `stream_id`（时间戳）。服务端是否按 `app_name` 匹配【未验证】；即使匹配，示例也传错了。
2. **`PlayStop` 吞错误**：`g1_audio_client.hpp:80-88` 丢弃 `Call` 返回值。
3. **原始组播端口 5555**：与 DDS 无关，可能与现场其他服务冲突。
4. **相对路径**：`../example/g1/audio/test.wav` 只在特定工作目录下有效。

---

## 4. G1D 示例

### 4.1 `g1_agv_client_example.cpp`

文件：`example/g1/g1d/g1_agv_client_example.cpp`
风险：**R3**；底盘运动 + 升降

| 行号 | 代码 | API | 说明 |
|---|---|---|---|
| `:15` | `Init(0)` | `ChannelFactory::Init` | ⚠️ 未指定网卡，使用默认 |
| `:16-19` | `AgvClient ac; SetTimeout(3); Init()` | `g1::AgvClient` | 服务名 `agv` |
| `:22-26` | 周期 40 步、`vx/vyaw` 幅值 0.3、高度幅值 1.0 | — | 参数写死 |
| `:37` | `ac.Move(vx, 0.0f, vyaw)` | `AgvClient::Move` | 50 ms 一次 |
| `:43-45` | `ac.HeightAdjust(height)` | `AgvClient::HeightAdjust` | 50 ms 一次 |
| `:28-50` | `while(true)` | — | 无退出、无看门狗 |

#### 关键问题

1. **没有停止命令**：循环结束后不会执行；Ctrl+C 后底盘是否停【未验证】。
2. **`Move` 会覆盖上一条**：没有加速度限制，正弦幅值 0.3 m/s 已经不小。
3. **`Init(0)` 无网卡**：多网卡主机上可能选错接口。

---

### 4.2 `g1d_height_control.cpp`

文件：`example/g1/g1d/g1d_height_control.cpp`
风险：R3（升降柱）

| 行号 | 代码 | API / Topic | 说明 |
|---|---|---|---|
| `:18` | `rt/hispeed_state` | Topic | G1D 高度状态 |
| `:71-76` | `StopHeightAdjust`：`HeightAdjust(0.0F)` | `AgvClient::HeightAdjust` | 停止升降 |
| `:79-150` | `height_control`：闭环/超时/失败停止 | `HeightAdjust` | 示例自己实现控制逻辑 |
| `:125` | `HeightAdjust(vz)` | `AgvClient::HeightAdjust` | 归一化速度 |
| `:167` | `Init(0, network_interface)` | `ChannelFactory::Init` | 必须传网卡 |
| `:169-171` | `AgvClient; SetTimeout(3); Init()` | `AgvClient` | — |
| `:173-175` | `ChannelSubscriber<Point32_>(rt/hispeed_state); InitChannel(handler,1)` | `ChannelSubscriber` | 用 `Point32_::y()` 表示高度【示例注释】 |

#### 关键问题

1. **`Point32_` 的哪个分量是高度**：示例用 `y()`；这是约定，不是类型保证。换固件要复核。
2. **依赖状态新鲜度**：示例有 `StopHeightAdjust`，但没看到对 `GetLastDataAvailableTime` 的检查；掉线时可能继续发速度。
3. **目标高度范围**：`HeightAdjust` 输入是归一化速度，不是位置；示例自己做了位置闭环。

---

### 4.3 `g1d_waist_example.cpp`

文件：`example/g1/g1d/g1d_waist_example.cpp`
风险：R2（腰部）

| 行号 | 代码 | API / Topic / Msg | 说明 |
|---|---|---|---|
| `:12-13` | `rt/waistpitchcmd`、`rt/waistpitchstate` | Topic | G1D 腰部 |
| `:33-36` | `ChannelPublisher<MotorCmd_>(cmd); InitChannel()` | `ChannelPublisher<unitree_hg::msg::dds_::MotorCmd_>` | 单电机命令 |
| `:38-39` | `ChannelSubscriber<MotorState_>(state); InitChannel(handler,1)` | `ChannelSubscriber<MotorState_>` | 单电机状态 |
| `:41-43` | 等待 `get_current_q` | — | 忙等 10 ms |
| `:45-51` | `mode=1; q=0; dq=0; tau=0; kp=500; kd=12` | `MotorCmd_` | ⚠️ `kp=500` 很大 |
| `:53` | `Write(command)` | `ChannelPublisher::Write` | 单次 |
| `:55-58` | 等待到位（±0.05） | — | 无超时 |
| `:60-74` | `q=0.8` / `q=0` 无限往返 | `MotorCmd_::q()` | 无退出路径 |

#### 关键问题

1. **`kp=500/kd=12`**：示例值，不代表安全上限；腰部负载变化时需要重新整定。
2. **无超时**：等待到位循环没有超时，堵转时会永久卡住。
3. **无 CRC**：`MotorCmd_` 单电机命令没有 CRC 字段；是否被固件接受取决于该通道实现【未验证】。

---

### 4.4 `g1d_arm_example.cpp`

文件：`example/g1/g1d/g1d_arm_example.cpp`
风险：**R3**；直接发 `rt/lowcmd`

| 行号 | 代码 | API / Topic / Msg | 说明 |
|---|---|---|---|
| `:20-22` | `rt/lowcmd`、`rt/secondary_imu`、`rt/lowstate` | Topic | 全身低层 |
| `:217` | `Init(0, networkInterface)` | `ChannelFactory::Init` | — |
| `:220-221` | `ChannelPublisher<LowCmd_>(rt/lowcmd); InitChannel()` | `ChannelPublisher` | — |
| `:224-227` | `ChannelSubscriber<LowState_>(...); InitChannel(handler,1)`、`IMUState_` 同 | `ChannelSubscriber` | — |
| `:230-232` | 等待第一帧 IMU | — | 忙等 10 ms |
| `:241-250` | 等待手柄 A 键 | `LowState_.wireless_remote`（由 `gamepad.hpp` 解析） | 不是 SDK 手柄 API |
| `:253-263` | 构造 29 关节 `LowCmd_`，`mode_pr=0`，`mode_machine=mode_machine_` | `LowCmd_` | `q` 取当前状态 |
| `:270-281` | 正弦摆动肩关节，算 CRC，`Write` | `LowCmd_` / `Crc32Core` | 2 ms 周期 |
| `:283-285` | A 键退出 | — | 退出后不再发命令 |

#### 关键问题

1. **没有释放内置运控**：示例不调用 `MotionSwitcherClient`；如果机器人还在高层控制下，`rt/lowcmd` 可能被忽略或冲突。这与其他低层示例不一致。
2. **`mode_pr=0`**：PR 模式；G1D 是否支持该模式【未验证】。
3. **无权重/无渐变**：直接以 `kp` 接管全部 29 关节，风险高。

---

### 4.5 `terminations.cpp`

文件：`example/g1/low_level/terminations.cpp`
风险：R0（只读）

| 行号 | 代码 | API | 说明 |
|---|---|---|---|
| `:23` | `Init(0, vm["network"].as<std::string>())` | `ChannelFactory::Init` | 命令行 `--network` |
| `:25-29` | `ChannelSubscriber<LowState_>("rt/lowstate"); InitChannel(lambda)` | `ChannelSubscriber` | **默认 `queuelen=0`**，回调在 DDS 线程 |
| `:35` | `g1::bad_orientation(lowstate, 1.0f)` | `terminations.hpp:18` | 姿态判据 |
| `:38` | `g1::lost_connection(lowstate_subscriber, 1000)` | `terminations.hpp:81` | 掉线判据 |
| `:42` | `sleep_for(100ms)` | 标准库 | 10 Hz 检查 |

#### 关键问题

1. **`lowstate` 数据竞争**：回调线程写、主线程读，没有锁；`LowState_` 很大，撕裂读风险真实存在。
2. **只演示两个判据**：`joint_vel_out_of_limit`、温度、电池等没有示例。
3. **`lost_connection` 依赖 `GetLastDataAvailableTime`**：从未收到数据时返回 `-1`，计算结果必然超时——这符合预期，但不要在初始化阶段误判为“掉线”。

---

## 5. 不应照搬到 G1 的示例

| 示例 | 原因 | 详见 |
|---|---|---|
| `example/state_machine/robot_controller.hpp` | 使用 `unitree_go` 消息与 GO2 的 `rt/lowcmd` 布局，G1 类型不匹配 | [13](13_examples_index.md) §6 |
| `example/go2/go2_video_client.cpp` | `VideoClient` 只存在于 GO2/B2 服务，G1 没有视频服务 | [07](07_sensors_and_camera.md) §3 |
| `example/wireless_controller/main.cpp` | 订阅 GO2 风格 `rt/wirelesscontroller`；G1 手柄在 `LowState_.wireless_remote` | [01](01_channel_communication.md) §6 |
| `example/b2/*`、`example/h1/*`、`example/r1/*` | 其他机型 API，服务名/消息可能不同 | [00](00_sdk_overview.md) §5 |

---

## 6. 跨示例横向对比

### 6.1 通道与线程参数

| 示例 | 网卡参数 | `queuelen` | 控制周期 | 退出方式 |
|---|---|---|---|---|
| `g1_loco_client` | `--network_interface`，默认 `"lo"` | 无订阅 | 事件驱动 | 命令执行完退出 |
| `g1_arm_action` | `--network` | 无订阅 | 事件驱动 | 执行完退出 |
| `g1_arm5/7_sdk` | 必填 `argv[1]` | 1 | 20 ms | 脚本结束 |
| `g1_hand_sdk` | 可选 `argv[1]` | 无订阅 | 1 s | 无（`while(true)`） |
| `g1_dex3` | 必填 `argv[1]` | 1 | 事件/状态机 | 输入 `q` |
| `g1_userctrl` | 必填 `argv[1]` | 1 | 20 ms | 脚本结束 |
| `g1_ankle_swing` | 必填参数 | 1 | 2 ms（500 Hz） | 无（周期线程） |
| `g1_dual_arm` | 必填参数 | 1 | 2 ms（500 Hz） | 无（`while(true)`） |
| `g1_audio` | 必填 `argv[1]` | 0（默认） | 事件驱动 | 无（`while(1)`） |
| `g1_agv` | 无（`Init(0)`） | 无订阅 | 50 ms | 无（`while(true)`） |
| `g1d_height` | 必填参数 | 1 | 事件驱动 | 到达目标 |
| `g1d_waist` | 必填 `argv[1]` | 1 | 事件驱动 | 无（`for(;;)`） |
| `g1d_arm` | 必填参数 | 1 | 2 ms | 手柄 A 键 |
| `terminations` | `--network` | 0（默认） | 100 ms | 无（`while(true)`） |

### 6.2 控制权处理对比

| 示例 | 是否释放内置运控 | 是否有 FSM 检查 | 是否渐变权重 |
|---|---|---|---|
| `g1_ankle_swing` | ✅ `MotionSwitcherClient::ReleaseMode` | ❌ | — |
| `g1_dual_arm` | ✅ | ❌ | — |
| `g1_userctrl` | ❌（走用户控制通道） | ✅ FSM==1 | — |
| `g1d_arm` | ❌ | ❌ | — |
| `g1_arm5/7_sdk` | ❌ | ❌ | ❌（直接 1.0） |
| `g1_hand_sdk` | ❌ | ❌ | ❌（直接 1.0） |

这张表说明：**示例之间的安全实践并不一致**。不要因为“官方示例这么写”就认为它适合生产。

---

## 7. 示例中“缺失”的 API

以下 API 在 `example/g1/` 中**没有出现**，但对 G1-EDU 部署可能重要：

| API / 能力 | 为什么重要 | 详见 |
|---|---|---|
| `Client::WaitLeaseApplied` / `GetLeaseId` | 多客户端竞争控制权 | [02](02_dds_rpc_client_server.md) §4.2 |
| `Server` / `ServerBase` | 在边缘侧提供自定义 RPC | [02](02_dds_rpc_client_server.md) §4.4 |
| `SubscriptionBase` / `RealTimePublisher` | 更安全的订阅/发布封装 | [08](08_concurrency_and_lifecycle.md) §2 |
| `publisher::LowCmd::check_mode_machine` | 防止 `mode_machine` 不匹配 | [06](06_g1_low_level_control.md) §2 |
| `terminations.hpp` 的温度/电池/关节速度判据 | 安全监控 | [04](04_robot_state.md) §6 |
| `lost_connection` 之外的新鲜度检查 | 推理输入门控 | [10](10_model_deployment_integration.md) §4 |
| G1 相机/图像 | 视觉输入 | [07](07_sensors_and_camera.md) |
| ROS 2 桥接 | 与 ROS 2 生态集成 | [09](09_ros2_integration.md) |

---

## 8. 从示例到生产代码的替换清单

| 示例写法 | 生产代码应替换为 | 原因 |
|---|---|---|
| `while(true)` / `for(;;)` | 可响应 SIGINT/SIGTERM 的循环 + 状态机 | 优雅退出、降级 |
| `sleep()` / `usleep()` | `std::this_thread::sleep_for` + 周期线程/时间戳 | 可读性、可测试性 |
| 主线程直接发命令 | 独立控制线程 + 看门狗 | 避免阻塞 |
| 回调里 `memcpy` 到全局变量 | `SubscriptionBase` 或带锁双缓冲 | 数据竞争 |
| `weight=1.0` | 按 `weight_rate` 渐变 + 超时释放 | 防瞬间接管 |
| `SwitchToUserCtrl()` 后直接发命令 | 检查返回值 + 确认 FSM/控制权 | 参数语义不明 |
| 动作名直接拼 JSON | 转义或使用动作 ID | JSON 注入/解析失败 |
| 相对路径读 WAV/YAML | 配置项 + 存在性检查 | 工作目录变化 |
| 无超时等待到位 | 超时 + 失败降级 | 堵转卡死 |
| 只检查 `ret==0` | 同时检查状态话题/新鲜度 | RPC 成功 ≠ 动作完成 |

---

## 9. 复核状态

| 项 | 状态 |
|---|---|
| 示例源码阅读 | 已完成（15 个 G1 `.cpp`） |
| 行号 | 基于当前 checkout，SDK 升级后需复核 |
| 示例编译 | 11 个 G1 目标 g++ 编译通过；4 个因缺依赖未编译（见 [13](13_examples_index.md) §9） |
| 示例运行 | **未执行** |
| 机器人行为 | **未验证** |
| 消息字段偏移 | 见 [03](03_message_types.md)，未做二进制级验证 |

相关文档：[13 示例索引](13_examples_index.md)、[api_index](api_index.md)、[05 高层控制](05_g1_high_level_control.md)、[06 低层控制](06_g1_low_level_control.md)、[08 并发与生命周期](08_concurrency_and_lifecycle.md)。
