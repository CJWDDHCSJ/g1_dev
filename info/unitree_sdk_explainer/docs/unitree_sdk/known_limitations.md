# 已知限制与风险清单

> 本文只收录**有源码、IDL 或示例证据**的限制；每条都标注它是“SDK 没有这个能力”“实现有缺陷”“语义未验证”还是“部署环境风险”。
> 没有编译、运行或连接机器人；凡涉及运行时行为的结论均为【未验证】。
> 相关文档：[README](README.md)（证据等级）、[api_index](api_index.md)、[05](05_g1_high_level_control.md)、[06](06_g1_low_level_control.md)、[07](07_sensors_and_camera.md)、[12](12_network_and_deployment.md)。

---

## 0. 先看这十条

| # | 限制 | 严重度 | 证据 |
|---|---|---|---|
| 1 | **G1 没有相机/图像 API**，视觉输入必须外接 | 阻断（视觉项目） | `include/unitree/robot/g1/` 无 video/camera；`VideoClient` 仅在 GO2/B2 |
| 2 | **SDK 不是 ROS 2 包**，也没有 `sensor_msgs/Image` | 高 | 全仓库无 `package.xml`；`idl/ros2/` 只有少量类型 |
| 3 | **低层/运动控制没有急停、看门狗、碰撞检测** | 安全关键 | 所有保护都在示例/应用层，SDK 不提供 |
| 4 | **`HighStand()` / `LowStand()` 有数值类型缺陷** | 高 | `g1_loco_client.hpp:228-230` |
| 5 | **`SwitchMoveMode()` 只改本地变量，不发 RPC** | 中高 | `g1_loco_client.hpp:242-245` |
| 6 | **`PlayStop()` 吞掉返回值，示例还传错参数** | 中 | `g1_audio_client.hpp:80-89`；`g1_audio_client_example.cpp:153` |
| 7 | **G1 `SportModeState_` 只有 4 个字段**，没有位姿/速度/步态 | 高（导航/定位） | `idl/hg/SportModeState_.hpp:23-26` |
| 8 | **QoS 与 DDS Security 没有公开 API** | 高（集成/安全） | `channel_factory.hpp` 无 setter；无 security API |
| 9 | **同名话题在 G1/GO2 上类型不同**，不会自动互通 | 高（多机型） | `rt/lowstate` 的两种 IDL |
| 10 | **静态库无源码**，线程回收、默认 QoS、`Release()` 语义不可确认 | 中高 | `lib/*/libunitree_sdk2.a` 预编译 |

---

## 1. 严重度定义

| 级别 | 含义 | 处理原则 |
|---|---|---|
| **S1 安全关键** | 可能导致人员伤害、机器人摔倒、失控 | 必须有独立安全措施；不得仅依赖 SDK |
| **S2 高** | 功能不可用、集成失败、数据错误 | 部署前必须验证并设计替代方案 |
| **S3 中** | 行为不符合预期、需要额外适配 | 记录并在适配层规避 |
| **S4 低** | 文档/构建/可维护性问题 | 修正文档或工程配置 |

---

## 2. 能力边界：SDK 不提供什么

### 2.1 G1 相机与视觉

**结论**：SDK 没有 G1 的相机采集、图像消息、解码 API。

证据：

- `include/unitree/robot/g1/` 只有 `loco/`、`arm/`、`audio/`、`agv/`、`common/` 五个子目录。
- G1 15 个示例中没有图像话题；`g1_audio_client_example.cpp` 的组播 socket 是**麦克风音频**，不是视频。
- `VideoClient` 只在 `go2/video/video_client.hpp` 和 B2 的 `front_video/back_video` 中存在。
- `include/unitree/idl/ros2/` 没有 `sensor_msgs/Image`、`CompressedImage`、`CameraInfo`。

影响：G1-EDU 的视觉链路必须自备相机 SDK（USB/GMSL/RTSP 等），并自己定义图像消息、时间戳、编码、分辨率元数据。

详见 [07](07_sensors_and_camera.md)。

### 2.2 G1 雷达/点云

**结论**：SDK 没有 G1 激光雷达或点云订阅 API。

证据：G1 目录与示例中没有 `PointCloud2`、`LaserScan` 或雷达话题；`idl/ros2/` 也不包含这些类型。

影响：需要外接雷达并使用厂商 SDK；如果要与 DDS 集成，必须自己定义 IDL 或走 ROS 2 桥。

### 2.3 定位/里程计

**结论**：G1 的 `SportModeState_` 不提供机身位置、速度、步态、足端力。

证据：`unitree_sdk2/include/unitree/idl/hg/SportModeState_.hpp:23-26` 只有 `fsm_id/fsm_mode/task_id/task_time`。

影响：不能把 G1 当 GO2 用；导航/里程计需要外部定位（动捕、SLAM、轮式编码器等）。

### 2.4 推理运行时

**结论**：SDK 不包含 TensorRT/ONNX Runtime/PyTorch 等推理 API，也没有图像预处理/后处理。

影响：模型部署需要自建 `InferenceEngine` 抽象、批处理、GPU 上下文和资源释放；参考 [10](10_model_deployment_integration.md)。

### 2.5 ROS 2 包

**结论**：仓库不是 ROS 2 包，没有 `package.xml`、`CMakeLists.txt` 的 `ament` 配置或 ROS 2 节点。

证据：全仓库没有 `package.xml`；`include/unitree/idl/ros2/` 只是 DDS 类型，不是 `rclcpp` 接口。

影响：ROS 2 集成必须自己做桥接，并处理 domain/QoS/类型映射。详见 [09](09_ros2_integration.md)。

### 2.6 音频 ASR 客户端方法

**结论**：`AudioClient` 没有 ASR 方法，虽然 `ROBOT_API_ID_AUDIO_ASR=1002` 存在。

证据：`g1_audio_api.hpp:18` 定义 ID；`g1_audio_client.hpp` 的公共方法只有 TTS/音量/播放/LED。

影响：语音识别只能订阅 `rt/audio_msg`（示例做法）或自己发 1002 RPC；后者参数/返回格式未验证。

### 2.7 取消/急停 API

**结论**：没有“立即停止并保持”的专用 API。

证据：

- `LocoClient::StopMove()` 只是 `SetVelocity(0,0,0)`（`g1_loco_client.hpp:226`），默认 1 s。
- `Damp()` 切 FSM 1；`ZeroTorque()` 切 FSM 0（可能失力）。
- 没有 `EmergencyStop`、`CancelAction`、`Abort` 之类的方法。

影响：急停必须依赖物理急停、安全 PLC 或应用层策略；不能把 `StopMove()` 当急停。

### 2.8 动作完成反馈

**结论**：RPC 返回 `0` 只表示调用成功，不表示动作完成；`LocoClient` 没有完成回调/状态查询（除 `GetFsmId` 外）。

证据：`ClientBase::Call` 返回 `int32_t` 错误码；`LocoClient` 没有事件订阅 API。`G1ArmActionClient` 的 `rt/arm/action/state` 是独立话题，不是 RPC 返回值。

影响：生产代码必须订阅状态话题并自行判定完成/超时。

### 2.9 DDS QoS 与安全

**结论**：`ChannelFactory` 没有 QoS setter，也没有 DDS Security API。

证据：

- `channel_factory.hpp:16-57` 只有 `Init/Create/Release`。
- `DdsFactoryModel` 内部持有 QoS，但没有通过 `ChannelFactory` 暴露。
- 在 `include/unitree` 搜索 security/encrypt/authentication 无结果。

影响：

- 与 ROS 2/其他 DDS 互通时，QoS 不兼容会“发现得到、收不到数据”。
- 同一网段内任何进程都能发布/订阅；没有认证、访问控制、加密。

详见 [12](12_network_and_deployment.md) §6、§9。

### 2.10 BMS 话题未确认

**结论**：G1 的 `BmsState_` 类型存在，但**发布话题未在 SDK 中确认**；`rt/lowstate` 不包含 BMS。

证据：`LowState_` 字段表无 BMS；`terminations.hpp:68-71` 的 `low_battery()` 需要独立 `BmsState_`；G1 示例没有订阅 BMS。

影响：`low_battery()` 目前只能用于你自行拿到 `BmsState_` 的场景；不要假设它能直接工作。

### 2.11 `dds_wrapper` 的 `ArmSdk` 类型别名错误

**结论**：`g1_sub.h` 中 `subscription::ArmSdk::SharedPtr` 被定义为 `std::shared_ptr<LowCmd>`，而不是 `std::shared_ptr<ArmSdk>`。

证据：`include/unitree/dds_wrapper/robots/g1/g1_sub.h:36-43`：

```cpp
class ArmSdk : public SubscriptionBase<unitree_hg::msg::dds_::LowCmd_>
{
public:
    using SharedPtr = std::shared_ptr<LowCmd>;   // 应为 ArmSdk
    ...
};
```

影响：按 `ArmSdk::SharedPtr` 使用时会得到错误的静态类型，可能编译失败或掩盖订阅对象混用。该头文件是辅助封装（`dds_wrapper`），不属于 RPC 客户端；本次审计**未修改 SDK 源码**。若使用该封装，请自行在该版本中规避或用 `std::shared_ptr<ArmSdk>` 显式声明。

---

## 3. 安全关键限制

### 3.1 控制权互斥但没有仲裁 API

G1 的高层 `sport`、低层 `rt/lowcmd`、`rt/user_lowcmd`、`rt/arm_sdk` 之间存在控制权竞争：

- 低层示例通过 `MotionSwitcherClient::ReleaseMode()` 释放内置运控（`g1_ankle_swing_example.cpp:183-191`、`g1_dual_arm_example.cpp:178-193`）。
- `g1d_arm_example.cpp` 直接发 `rt/lowcmd`，**没有释放**（`:217-227`）。
- `g1_userctrl_dds_example.cpp` 走用户控制通道，只检查 FSM==1（`:95-100`）。

SDK 没有提供“当前谁拥有控制权”的查询/仲裁 API；`CheckMode` 只能查 `motion_switcher` 的模式名。多进程同时控制时行为未定义。

### 3.2 没有命令超时/看门狗

SDK 不会因为你停止发送 `LowCmd_` 就自动让机器人进入安全状态；是否超时接管取决于机器人内部服务【未验证】。

示例中的保护全是应用层：

- `g1_ankle_swing_example` 没有看门狗；
- `g1_dual_arm_example` 没有看门狗；
- `g1_hand_sdk_example` 无限循环；
- `g1_agv_client_example` 无限循环；
- `g1d_waist_example` 无限往返。

生产代码必须自己实现：命令超时 → 降级/释放/停止。

### 3.3 低层命令没有参数校验

`LowCmd_` / `MotorCmd_` 的 `q/dq/kp/kd/tau` 没有 SDK 侧限幅；示例直接填值：

- `g1_userctrl_dds_example.cpp:129-135`：`kp=0, kd=1.5`；
- `g1_arm5_sdk_dds_example.cpp:109-121`：`kp=60, kd=1.5`；
- `g1d_waist_example.cpp:50-51`：`kp=500, kd=12`；
- `g1_dex3_example.cpp:174-175`：`kp=1.5, kd=0.1`。

这些只是示例值，不是安全上限。应用层必须做关节限位、速度限幅、力矩限幅、温度/电流保护。

### 3.4 权重瞬变

`rt/arm_sdk` 用索引 29 的 `q` 作为权重（`dds_wrapper/robots/g1/g1_pub.h:77-80`）。两个手臂示例都声明了 `weight_rate`，却在初始化时直接：

```cpp
weight = 1.0;
msg.motor_cmd().at(JointIndex::kNotUsedJoint).q(weight);
```

（`g1_arm5_sdk_dds_example.cpp:144-145`、`g1_arm7_sdk_dds_example.cpp:149-150`）

手部示例同样直接 `set_weight(1.0f)`（`g1_hand_sdk_example.cpp:66`）。

影响：手臂/手会瞬间从内部控制切到 SDK 控制，可能产生冲击。正确做法是从 0 按速率渐变。

### 3.5 持续运动命令

`LocoClient::Move(..., true)` 使用 `duration=864000.f` 秒（10 天）（`g1_loco_client.hpp:232-234`）。如果进程崩溃/网络断开，服务端是否在 duration 到期前一直保持速度【未验证】。

`AgvClient::Move` 没有持续时间参数，示例每 50 ms 重发一次（`g1_agv_client_example.cpp:37-49`）；停止发送后的行为未定义。

### 3.6 网络中断

`lost_connection()` 只是判据，不会自动停止（`terminations.hpp:81-86`）。控制循环必须自己检查并在超时后进入安全状态。

### 3.7 默认网卡陷阱

`g1_loco_client_example.cpp:21` 默认 `network_interface="lo"`。在回环网卡上 DDS 可以初始化成功，但收不到机器人数据；示例的只读查询会打印未初始化变量。

`g1_hand_sdk_example.cpp:59-63` 和 `g1_agv_client_example.cpp:15` 使用 `Init(0)`，默认网卡由 DDS/系统路由决定，SDK 没有文档化规则。

### 3.8 多机器人串扰

G1 话题名没有机器人序列号（`rt/lowstate`、`rt/lowcmd` 等），同一 DDS domain 内所有机器人共享同名话题。多台 G1 在同一交换机/域上会导致：

- 状态混叠；
- 控制命令被多台执行；
- 无法通过话题前缀区分。

`ChannelNamer` 也没有分区参数。详见 [12](12_network_and_deployment.md) §5。

---

## 4. 实现缺陷与可疑行为

### 4.1 `HighStand()` / `LowStand()` 数值类型错误

```cpp
int32_t HighStand() { return SetStandHeight(std::numeric_limits<uint32_t>::max()); }
int32_t LowStand()  { return SetStandHeight(std::numeric_limits<uint32_t>::min()); }
```

位置：`g1_loco_client.hpp:228-230`。

`SetStandHeight` 参数是 `float`；`uint32_t::max()` 转成 `float` 是约 `4.29e9`，`min()` 是 `0`。这不是“最高/最低身高”的合理值。示例仍暴露了 `high_stand`/`low_stand` 命令（`g1_loco_client_example.cpp:179-185`）。

**建议**：不要调用；用 `SetStandHeight` 传明确数值，并先确认单位/范围。

### 4.2 `SwitchMoveMode()` 是空操作

```cpp
int32_t SwitchMoveMode(bool flag) {
  continous_move_ = flag;
  return 0;
}
```

位置：`g1_loco_client.hpp:242-245`。它只改本地 `continous_move_`，不发 RPC；示例却把它当命令（`g1_loco_client_example.cpp:214`）。

### 4.3 `SwitchToUserCtrl()` 参数语义不明

```cpp
go2::JsonizeDataBool json;
parameter = common::ToJsonString(json);
return Call(ROBOT_API_ID_LOCO_SWITCH_TO_USER_CTRL, parameter, data);
```

`json` 默认构造，发送 `{"data":false}`（`g1_loco_client.hpp:186-192`）。服务端是否接受、`false` 代表什么【未验证】。示例不检查返回值（`g1_userctrl_dds_example.cpp:138`）。

### 4.4 `PlayStop()` 吞错误 + 示例传错参数

- `PlayStop` 调用 `Call(...)` 后丢弃返回值，固定 `return 0`（`g1_audio_client.hpp:80-88`）。
- 示例 `PlayStream("example", stream_id, chunk)` 用 `app_name="example"`，但 `PlayStop(stream_id)` 传的是时间戳（`g1_audio_client_example.cpp:147,153`）。

影响：无法判断停止是否成功；参数是否匹配服务端字段未验证。

### 4.5 手臂动作映射重复

`g1_arm_action_client.hpp:85-102`：

```cpp
{"left kiss", 12},
{"right kiss", 12},
```

左右动作 ID 相同，映射表无法区分。是同一动作还是表错误【未验证】。以 `GetActionList` 的服务端返回为准。

### 4.6 自定义动作名直接拼 JSON

`ExecuteAction(const std::string& action_name)` 内部：

```cpp
parameter = R"({"action_name":")" + action_name + R"("})";
```

位置：`g1_arm_action_client.hpp:67-71`。动作名包含 `"`、`\` 或控制字符会破坏 JSON；这不是远程注入（本地 API），但会导致请求失败或字段被篡改。

### 4.7 Dex3 状态话题不一致

- `g1_sub.h:97,105` 使用 `rt/dex3/left/state`、`rt/dex3/right/state`。
- `g1_dex3_example.cpp:253,257` 运行时使用 `rt/lf/dex3/left/state`、`rt/lf/dex3/right/state`。

两者必有一个与当前固件不匹配；部署前必须用 DDS 工具确认。

### 4.8 `g1d_arm_example` 未释放内置运控

`g1d_arm_example.cpp:217-227` 直接创建 `rt/lowcmd` 发布者并发送命令，没有调用 `MotionSwitcherClient::ReleaseMode()`，也没有检查 FSM。与 `g1_ankle_swing_example` / `g1_dual_arm_example` 的做法不一致。

### 4.9 `GetPhase()` 已废弃

`g1_loco_api.hpp:22` 注释 `// deprecated`；`LocoClient::GetPhase` 仍可用（`g1_loco_client.hpp:110-122`），但不应在新代码中使用。

### 4.10 `SubscriptionBase::wait_for_connection()` 无超时

```cpp
while(isTimeout()) {
  sleep(100ms);
  ...
}
```

位置：`common/Subscription.h:53-67`。没有超时参数，收不到数据会永久阻塞；2 s 后只打印提示。

### 4.11 `SubscriptionBase` 默认 `queuelen=0`

构造函数调用 `sub_->InitChannel(handler)`（`Subscription.h:33`），没有传 `queuelen`，所以默认回调在 DDS listener 线程执行。封装看起来“高级”，但阻塞风险仍在。

### 4.12 `lost_connection()` 在首帧前返回 true

`lost_connection` 用 `now - GetLastDataAvailableTime()`；未收到数据时 `GetLastDataAvailableTime()` 返回 `-1`（`channel_subscriber.hpp:47-55`），因此必然超时。这是合理行为，但初始化阶段不要把“尚未收到”误判为“运行中掉线”。

### 4.13 示例中的数据竞争

多个示例在回调线程写、主线程读，没有锁：

- `g1_arm5/7_sdk_dds_example.cpp:84-87` 回调用 `memcpy` 写 `state_msg`，主线程直接读；
- `g1_userctrl_dds_example.cpp:87-90` 同样；
- `terminations.cpp:27-29` 回调用 `lowstate = *(...)`，主线程读；
- `g1_dex3_example.cpp` 的全局 `state` 由回调写、主循环读（`:272-273`）。

`LowState_` 很大，撕裂读的风险真实存在。生产代码应使用 `SubscriptionBase`（带锁）或自己的双缓冲。

---

## 5. 协议与语义不确定性

### 5.1 预编译静态库

`lib/x86_64/libunitree_sdk2.a`、`lib/aarch64/libunitree_sdk2.a` 是二进制；以下行为无法从源码确认：

- `ChannelFactory::Init` 失败是否抛异常、重复 Init 的行为；
- `Release()` 是否等待回调结束、是否可重复调用；
- `ClientBase::Call` 的超时精度、并发安全性；
- `LeaseClient` 的续约周期与失联行为；
- `Server` handler 的线程模型；
- `ChannelSubscriber::CloseChannel` 后回调是否保证不再触发；
- 默认 QoS（Reliability/History/Durability）。

这些在 [api_index](api_index.md) §8 与各模块文档中统一标为【未验证】。

### 5.2 服务可用性与版本

- 服务名/API ID 来自头文件，但**机器人固件是否实现全部 API**未验证。
- `Client::GetServerApiVersion` 可以查询服务端版本，但版本不匹配时的行为未定义。
- API ID 只在服务内唯一；`sport` 与 `arm` 都有 7106/7107，不能跨服务复用。

### 5.3 消息单位与范围

IDL 只给字段类型，不给单位：

- `MotorCmd_::q` 是弧度还是米？取决于关节类型；
- `SetStandHeight(float)` 的单位/范围未在头文件说明；
- `AgvClient::HeightAdjust(float)` 头文件注释明确是归一化 `[-1,1]` → `±76.5 mm/s`，这是少数有文档的；
- `LowState_::wireless_remote` 是 40 字节原始数据，不是解析后的按键。

不要从字段名推断单位；用示例/实测确认。

### 5.4 `mode_machine` 与仿真

`dds_wrapper/robots/g1/g1_pub.h:52-61` 注释：`mode_machine == 0` 表示仿真；实机非 0。`LowCmd_` 的 `mode_machine` 必须与 `LowState_` 匹配，否则命令可能被拒绝。

示例大多从状态更新 `mode_machine`，但 `g1d_arm_example` 在收到状态前就构造命令（`:253-255`），存在首帧不匹配风险。

### 5.5 `rt/sportmodestate` 类型是推断

`g1_arm_action_error.hpp:12-14` 注释说可以订阅 `rt/sportmodestate` 查 FSM，但没有 G1 示例显式使用该话题。类型标为 `unitree_hg::SportModeState_` 是【推断】。

### 5.6 话题名与固件版本

`rt/dex3/...` vs `rt/lf/dex3/...` 已证明示例内部不一致。其他话题也可能随固件变化；部署前应用 DDS 发现工具核对实际话题/类型。

---

## 6. 构建、依赖与兼容性

### 6.1 静态 SDK + 共享 DDS

- `libunitree_sdk2.a` 是静态库；
- CycloneDDS 以共享库形式随附（`libddsc.so.0`、`libddscxx.so.0`，SONAME 带 `.0`）；
- 部署时必须把 DDS 共享库带到目标机，或设置 `LD_LIBRARY_PATH`。

只拷贝 `libunitree_sdk2.a` 不够。详见 [11](11_build_and_link.md)。

### 6.2 架构选择

CMake 用 `${CMAKE_SYSTEM_PROCESSOR}` 选 `lib/x86_64` 或 `lib/aarch64`（`CMakeLists.txt:38-45`）。交叉编译时必须确保 `CMAKE_SYSTEM_PROCESSOR` 与目标架构一致，否则会链接错库。

### 6.3 Eigen 依赖未在 CMake 中声明

`g1_pub.h:6`、`g1_sub.h:6`、`terminations.hpp:8`、`g1_dex3_example.cpp:14` 都包含 Eigen，但 SDK 的 CMake 没有 `find_package(Eigen3)`；`g1_dex3_example` 也没有在 `example/g1/CMakeLists.txt` 中声明 Eigen 依赖。

结果：缺 Eigen 时在**编译期**报错，而不是配置期提示。详见 [11](11_build_and_link.md) §4。

### 6.4 编译器/ABI

SDK 预编译环境是 Ubuntu 20.04 + GCC 9.4（`README.md:5-7`）。用 GCC 13 或更新 libstdc++ 的主机链接时：

- 通常可以链接；
- 但 `std::string`/`std::function` ABI、异常类型、DDS C++ 绑定可能存在版本差异。

本次分析主机是 GCC 13.3；已用该编译器成功编译并链接 11 个 G1 示例，说明编译/链接层面可用（见 [13](13_examples_index.md) §9）。但未运行，运行期 ABI 兼容性仍【未验证】。

### 6.5 构建全部示例的依赖

`BUILD_EXAMPLES` 默认 `ON`（`CMakeLists.txt:5`），会构建所有机型示例。G1 示例中：

- `g1_termination`、`g1_arm_action_example` 需要 Boost.ProgramOptions；
- `g1_dual_arm_example` 需要 yaml-cpp ≥ 0.6 和运行时 `motion.seq`；
- `g1_dex3_example` 需要 Eigen 头文件。

只做核心集成时，可以关闭示例或只构建需要的目标。

### 6.6 G1 与 GO2 同名不同型

| 话题 | G1 | GO2 |
|---|---|---|
| `rt/lowstate` | `unitree_hg::LowState_` | `unitree_go::LowState_` |
| `rt/sportmodestate` | `unitree_hg::SportModeState_`（4 字段） | `unitree_go::SportModeState_`（完整） |

DDS 匹配要求类型兼容；同名不同类型不会匹配，表现为“能发现、收不到”。详见 [03](03_message_types.md) §3。

---

## 7. 仓库/文档层面

| 问题 | 证据 | 影响 |
|---|---|---|
| README 引用了不存在的 `example/cmake_sample` | `README.md:55`；`example/` 下无该目录 | 新手无法按文档找到 CMake 示例 |
| CMake 最低版本 3.5，README 要求 3.10 | `CMakeLists.txt:1` vs `README.md:13` | 低版本 CMake 可能通过配置但构建失败 |
| 项目版本 2.0.0，git describe 是 `2.0.2-75` | `CMakeLists.txt:2` vs git | 版本标识不一致，问题复现时需同时记录两者 |
| README 安装 `libfmt-dev`，示例中 fmt 使用极少 | `README.md:21` | 多装一个依赖，不影响功能 |
| 示例内部安全实践不一致 | 见 §3、§4 | 不能把“官方示例”当最佳实践 |

---

## 8. 风险登记表

| ID | 风险 | 严重度 | 触发条件 | 影响 | 缓解 |
|---|---|---|---|---|---|
| R-01 | G1 无相机 API | S2（视觉项目 S1） | 直接找 SDK 取图 | 视觉链路无法启动 | 外接相机 + 自建图像契约 |
| R-02 | 控制权冲突 | S1 | 多进程/未释放运控 | 命令被忽略或机器人异常 | 单一控制进程 + 模式确认 |
| R-03 | 无看门狗 | S1 | 进程崩溃/网络断 | 机器人保持最后命令 | 应用层超时降级 + 物理急停 |
| R-04 | 权重瞬变 | S1 | 直接 `weight=1` | 手臂/手冲击 | 权重渐变 + 限速 |
| R-05 | `HighStand/LowStand` 缺陷 | S2 | 调用这两个方法 | 发送异常身高 | 禁用，改用明确数值 |
| R-06 | `SwitchMoveMode` 空操作 | S3 | 依赖该调用改模式 | 以为已切换，实际没有 | 改用 `SetVelocity`/状态确认 |
| R-07 | `PlayStop` 吞错/传错参 | S3 | 停止音频 | 无法判断是否停止 | 自行实现停止 RPC + 校验 |
| R-08 | Dex3 话题不一致 | S2 | 用示例状态话题 | 收不到手状态 | DDS 工具核对实际话题 |
| R-09 | QoS 不可配 | S2 | 与 ROS 2 桥接 | 能发现、收不到 | 实测默认 QoS + 桥接侧适配 |
| R-10 | 无 DDS 安全 | S1（多租户/公网） | 不可信网络 | 任意进程可发命令 | 网络隔离/VPN/物理隔离 |
| R-11 | 多机器人串扰 | S1 | 同域多机 | 命令串机 | 一机一域/一网 |
| R-12 | 默认 `"lo"` | S2 | 直接运行 loco 示例 | 无数据、误判 | 显式传网卡 |
| R-13 | 静态库行为未知 | S3 | 依赖析构/Release | 退出卡住/回调在途 | 实测 + 保守关闭顺序 |
| R-14 | ABI/编译器不匹配 | S3 | 非 Ubuntu 20.04/GCC 9.4 | 链接失败/运行异常 | 用目标环境构建 |
| R-15 | 示例数据竞争 | S2 | 照抄回调写法 | 状态撕裂/崩溃 | 加锁或双缓冲 |

---

## 9. 不能从当前工作区确认的结论

以下问题**没有答案**，不要把它们写成确定事实：

1. 机器人固件是否实现了头文件中的全部 API ID。
2. `ChannelFactory::Init` 失败时是否抛异常。
3. `Release()` 是否等待回调线程结束、是否可重复调用。
4. `ClientBase::Call` 是否线程安全、超时是否包含重试。
5. `LeaseClient` 的续约周期、失联后的控制权行为。
6. `Server` handler 运行在哪个线程、队列多深。
7. SDK 默认 QoS 的具体取值。
8. `rt/sportmodestate` 在 G1 上的实际类型与频率。
9. G1 的 BMS 话题名与频率。
10. `Dex3` 的实际状态话题是 `rt/dex3/...` 还是 `rt/lf/dex3/...`。
11. `AgvClient::Move` 停止发送后底盘是否自动停止。
12. `LowCmd_` 停止发送后机器人是否自动进入安全状态。
13. `SwitchToUserCtrl` 的 `{"data":false}` 语义。
14. `SetStandHeight` 的单位与允许范围。
15. `G1ArmActionClient` 的 holding 最长 20 s 是否由固件强制执行（头文件注释如此说）。

验证方法：在**支撑/吊装、急停可用、场地隔离**的条件下，用只读订阅和单变量实验逐项确认；任何运动相关验证必须由授权人员执行。

---

## 10. 复核状态

| 项 | 状态 |
|---|---|
| 头文件/IDL 证据 | 已核对当前 checkout |
| 示例行为 | 已读源码；11 个 G1 目标编译通过，**全部未运行**（见 [13](13_examples_index.md) §9） |
| 静态库内部行为 | **未验证** |
| 机器人固件行为 | **未验证** |
| 多机/网络实验 | **未执行** |
| 行号 | SDK 升级后需复核 |

如果实测结果与本文冲突，以实测为准，并回到对应模块文档修正证据标签。
