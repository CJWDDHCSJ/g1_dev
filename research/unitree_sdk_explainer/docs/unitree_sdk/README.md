# Unitree SDK2 · G1-EDU 接口文档库

本目录是一套基于**工作区内实际 SDK 源码**整理的接口与工程分析文档，面向 G1-EDU 具身智能模型部署项目。目标不是复述厂商宣传，而是回答三个问题：

1. 这个 SDK 到底提供了什么、边界在哪里；
2. 在 G1 上做视觉/推理/控制集成时，哪些路径是 SDK 原生支持的，哪些必须自己写适配层；
3. 哪些结论有源码证据，哪些只是示例用法或推断，哪些完全未验证。

> **分析对象**：`/home/mick/Mick_Grey/unitree_sdk2`
> **文档目录**：`/home/mick/Mick_Grey/unitree_sdk_explainer/docs/unitree_sdk`
> **SDK 版本**：CMake `project(unitree_sdk2 VERSION 2.0.0)`（`CMakeLists.txt:2`）；git `2.0.2-75-gd1abf00`，HEAD `d1abf001220d3e5f59e00a0c4aaa595e00089a4a`（2026-10-15）
> **二进制**：`lib/x86_64/libunitree_sdk2.a`、`lib/aarch64/libunitree_sdk2.a`（预编译静态库）
> **DDS**：随附 Eclipse CycloneDDS / cyclonedds-cxx 0.10.2（`thirdparty/include/dds/version.h:15`）

---

## 阅读前必读：证据等级与安全边界

### 证据标签

本文库统一使用以下标签。2026-10-24 已用 `g++` 直接编译部分 G1 示例与文档示例，但**没有运行**它们；编译通过只证明代码可构建，不证明运行行为：

| 标签 | 含义 |
|---|---|
| `【源码】` | 直接阅读头文件声明、内联实现或 IDL 生成代码确认 |
| `【示例】` | 阅读了 SDK 自带示例源码；其中 11 个 G1 目标已编译通过，**全部未运行**；只证明“示例这样写”，不证明“API 契约就是这样” |
| `【注释】` | SDK 头文件注释、README 或错误码描述中的说法，未独立验证 |
| `【推断】` | 基于源码结构的合理推断，可能受预编译库内部实现影响 |
| `【未验证】` | 当前工作区无法确认；使用前必须实测 |

关键区别：

- 头文件里**有声明**，不等于静态库中**有实现**；本文库对只看到声明的接口会明确写“仅确认声明”。
- 示例里能跑通，不等于所有参数组合都安全；尤其运动控制示例，本文只做代码解读。
- 预编译静态库的内部行为（默认 QoS、线程回收、`Release()` 语义等）无法从源码确认，统一标为未验证。

### 安全边界（必须保持）

- 本次工作**没有修改 SDK 源码、没有升级 SDK、没有修改系统环境、没有安装软件**。
- 本文 C++ 代码片段均为**文档示例**：8/11 个完整程序已编译通过，04（缺 Eigen3）、07（缺 OpenCV）、09（缺 ROS 2）因环境不满足未编译；**全部未运行**，复制到工程前仍须自行验证。
- **不要在没有授权和安全措施的情况下运行任何运动示例**（`g1_ankle_swing_example`、`g1_userctrl_dds_example`、`g1_arm*_sdk_dds_example`、`g1_agv_client_example`、`g1d_*` 等）。这些示例会直接驱动关节或底盘。
- 需要验证通信时，优先使用**只读订阅**（`rt/lowstate`、`rt/secondary_imu`、`rt/hispeed_state`）和**只读 RPC**（`GetFsmId`、`GetStandHeight`、`GetActionList`）。
- 机器人急停、吊装/支撑、场地隔离、单人操作等安全流程不在 SDK 内，必须由部署方另行保证。

### 路径约定

文档中的源码位置统一写成：

```text
unitree_sdk2/<相对路径>:<行号>
```

其中 `unitree_sdk2/` 指 `/home/mick/Mick_Grey/unitree_sdk2/`。示例位置写成 `example/g1/...`。行号以当前 checkout 为准，SDK 升级后会漂移。

---

## 文档地图

| 文档 | 内容 | 建议读者 |
|---|---|---|
| [00_sdk_overview.md](00_sdk_overview.md) | SDK 身份、目录、分层架构、模块依赖图、能力边界 | 所有人，先读 |
| [01_channel_communication.md](01_channel_communication.md) | `ChannelFactory`、发布/订阅、回调线程、队列、G1 话题表 | 通信/集成工程师 |
| [02_dds_rpc_client_server.md](02_dds_rpc_client_server.md) | RPC-over-DDS、Client/Server/Lease、超时与错误码 | 服务集成工程师 |
| [03_message_types.md](03_message_types.md) | `unitree_hg`/`unitree_go`/ROS 2 IDL 类型、字段、关节索引 | 所有人 |
| [04_robot_state.md](04_robot_state.md) | 低层状态、IMU、手柄、SportModeState 限制、安全判据 | 感知/控制工程师 |
| [05_g1_high_level_control.md](05_g1_high_level_control.md) | Loco/Arm Action/Audio/AGV 高层 RPC | 应用开发 |
| [06_g1_low_level_control.md](06_g1_low_level_control.md) | `rt/lowcmd`、`rt/arm_sdk`、`rt/user_lowcmd`、手部 SDK、CRC、模式切换 | 控制工程师（高风险） |
| [07_sensors_and_camera.md](07_sensors_and_camera.md) | G1 相机缺口、视频/音频/IMU/雷达能力边界 | 视觉/感知工程师 |
| [08_concurrency_and_lifecycle.md](08_concurrency_and_lifecycle.md) | 后台线程、回调生命周期、锁、析构、退出清理 | 系统工程师 |
| [09_ros2_integration.md](09_ros2_integration.md) | ROS 2 兼容性、DDS 互通、消息映射、同名不同类型 | ROS 2 集成 |
| [10_model_deployment_integration.md](10_model_deployment_integration.md) | 传感器→推理→控制的完整链路与适配层设计 | 算法部署工程师 |
| [11_build_and_link.md](11_build_and_link.md) | CMake 导入、安装、依赖、架构、运行库路径 | 构建/运维 |
| [12_network_and_deployment.md](12_network_and_deployment.md) | DDS 域/网卡/组播/QoS/多进程/安全 | 现场部署 |
| [13_examples_index.md](13_examples_index.md) | 示例—API 对照、风险级别、编译依赖 | 所有人 |
| [api_index.md](api_index.md) | API 总索引（按模块/优先级） | 查阅 |
| [example_to_api.md](example_to_api.md) | 示例逐行到 API/话题的映射 | 查阅 |
| [known_limitations.md](known_limitations.md) | 已确认限制、疑似缺陷、风险清单 | 决策/评审 |
| [g1_deployment_priority.md](g1_deployment_priority.md) | P0/P1/P2 学习与落地优先级 | 项目负责人 |

### 面向“视觉模型部署”的最短阅读路径

```text
00 总览
 → 01 通信（Init/订阅/回调）
 → 03 消息（LowState / IMU / 关节）
 → 04 状态（怎么安全读机器人状态）
 → 07 传感器与相机（先接受“G1 SDK 无相机 API”这个事实）
 → 10 模型部署链路（适配层设计）
 → 11 构建 + 12 网络（把程序跑起来）
 → 08 并发/生命周期（长期稳定运行）
 → 05 高层控制（先做只读和低速安全动作）
 → 06 低层控制（最后，且必须有人监护）
```

---

## 十个最重要的结论

1. **G1 的相机能力不在此 SDK 中。** `include/unitree/robot/g1/` 只有 loco、arm、audio、agv、common 五类；视频客户端只存在于 GO2/B2（`go2/video/video_client.hpp:15`、`b2/front_video/...`）。G1-EDU 的视觉输入必须走外接相机或厂商另行提供的服务，不能假设本 SDK 能取图。详见 [07_sensors_and_camera.md](07_sensors_and_camera.md)。
2. **G1 原生通信就是 DDS 发布/订阅 + RPC-over-DDS。** 所有通道都必须先 `ChannelFactory::Instance()->Init(...)`；发送/接收通道创建函数直接解引用内部工厂指针，没有空指针保护（`channel_factory.hpp:32-45`），Init 之前创建通道属于未定义行为。
3. **回调线程模型是两套。** `queuelen == 0` 时回调直接跑在 CycloneDDS 监听线程；`queuelen > 0` 时每条消息拷贝进 `BlockQueue`，由名为 `rlsnr` 的线程串行回调，队列满时丢弃最旧消息（`dds_entity.hpp:307-338`）。回调里的 `message` 指针只在回调期间有效，必须拷贝。
4. **G1 的 `LowState_` 是 35 个电机槽，但整机是 29 DOF。** 关节索引 0–28 有明确的 `JointIndex` 定义（`dds_wrapper/robots/g1/defines.h:15-53`）；29–34 被用作特殊通道（例如 `rt/arm_sdk` 用索引 29 的 `q` 表示接管权重）。不要把 35 个槽都当关节。
5. **G1 的 `SportModeState_` 只有 4 个字段**：`fsm_id/fsm_mode/task_id/task_time`（`idl/hg/SportModeState_.hpp:23-26`）。它**不提供**机身位置、速度、步态、足端力。需要里程计/位姿时必须另行估计或接外部定位。
6. **同名话题在不同机器人上可能是不同类型。** `rt/lowstate` 在 G1 上是 `unitree_hg::LowState_`，在 GO2 上是 `unitree_go::LowState_`；`rt/sportmodestate` 同理。ROS 2 侧看到同名话题不代表能互通，详见 [09_ros2_integration.md](09_ros2_integration.md)。
7. **SDK 不是 ROS 2 包。** 全仓库没有 `package.xml`；`include/unitree/idl/ros2/` 只是生成的 ROS 2 风格 DDS 类型（String、Point32、PointCloud2 等），**没有** `sensor_msgs/Image`、`JointState`、`LaserScan`。ROS 2 集成必须自己做桥接。
8. **静态库只有二进制，没有实现源码。** QoS 默认值、Client 调用内部、`ChannelFactory::Release()`、线程析构是否 join 等都无法从工作区确认。本文对这些一律标注未验证，不把猜测写成结论。
9. **示例中存在若干会影响部署的缺陷/风险**：`HighStand()/LowStand()` 把整数极值当浮点身高传入（`g1_loco_client.hpp:228-230`）；`PlayStop()` 忽略返回码且示例传错参数（`g1_audio_client.hpp:80-88`、`example/g1/audio/...:153`）；Dex3 状态话题在示例内部不一致（`rt/dex3/...` 与 `rt/lf/dex3/...`）；Arm SDK 示例把权重瞬间设为 1.0。详见 [known_limitations.md](known_limitations.md)。
10. **控制权限是互斥的。** 高层 `sport` 服务、低层 `rt/lowcmd`、`rt/arm_sdk`、`rt/user_lowcmd` 之间存在模式/权限竞争；低层示例会先通过 `motion_switcher` 释放内置运动服务。不要在未释放/未确认 FSM 状态时抢发命令。

---

## 本 SDK 的适用边界（一句话版）

- **能用**：G1 状态读取、高层运动/手臂/音频 RPC、低层关节控制、DDS 自定义话题、C++17 静态链接。
- **要自己补**：相机采集、图像时间戳/分辨率元数据、视觉推理、ROS 2 桥接、里程计/定位、安全监控与急停逻辑。
- **不要假设**：G1 有视频服务、`SportModeState` 有速度、ROS 2 能直接订阅、示例参数就是安全参数、默认 QoS 能满足你的实时性。

---

## 复核状态

| 项目 | 状态 |
|---|---|
| 源码阅读 | 已完成（通信、消息、G1 API、示例、构建、网络配置路径） |
| 示例编译 | 11 个 G1 目标 g++ 编译通过；4 个因缺 Eigen/Boost/yaml-cpp 未编译（见 [13](13_examples_index.md) §9） |
| 示例运行 | **未执行**（只读约束） |
| 机器人连接 | **未执行** |
| 文档内代码 | 8/11 个完整程序编译通过；04/07/09 环境不满足；均未运行 |
| 行号 | 基于当前 checkout；SDK 更新后需复核 |

如发现某条结论与你的实测不一致，以实测为准，并回到对应文档的证据标签处修正。

第二轮审计的问题明细、编译记录与剩余风险见 [REVIEW_REPORT.md](REVIEW_REPORT.md)（2026-10-24）。
