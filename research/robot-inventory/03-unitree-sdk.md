# 03 — Unitree SDK（unitree_ros2 v0.3.0）

> 采集时间：2026-10-27（`ssh unitree@192.168.123.164`，全部为只读命令）

## 仓库概况：`~/unitree_ros2`（v0.3.0）

官方说明：Unitree SDK2 基于 CycloneDDS 实现机器人通信与控制；由于 ROS 2 同样以 DDS 为通信层，ROS 2 消息可以**直接**与宇树机器人通信，无需再包一层 SDK 接口。README 标注支持 Go2 / B2 / H1，但本仓库示例已扩展到 G1 / H1-2 / H2。

```
~/unitree_ros2/
├── cyclonedds_ws/          # 消息包工作区（已编译，install/ 与 build/ 均在）
│   └── src/unitree/
│       ├── unitree_api/    # 请求/响应 API 消息（所有高层控制走这里）
│       ├── unitree_go/     # 四足（Go2 系）消息：SportModeState、LowCmd/LowState、雷达、音频等
│       └── unitree_hg/     # 人形（G1 系）消息：LowCmd/LowState、MotorCmd、HandCmd、IMU 等
├── example/                # 示例工程
│   └── src/
│       ├── include/common/ # 公共客户端与工具
│       └── src/            # b2/ b2w/ common/ g1/ go2/ h1-2/ h2/ + 独立 demo cpp
├── setup.sh / setup_default.sh / setup_local.sh
└── version.txt             # 0.3.0
```

## 消息包

### unitree_api — 统一请求/响应协议

所有高层功能以 `/api/<服务名>/request` 与 `/api/<服务名>/response` 话题对的形式暴露：

| 消息 | 结构 |
|---|---|
| `Request.msg` | `RequestHeader header` + `string parameter`（JSON 参数）+ `uint8[] binary` |
| `RequestHeader.msg` | `RequestIdentity identity`（api_id、entity_id）+ `RequestLease lease` + `RequestPolicy policy` |
| `Response.msg` | `ResponseHeader header` + `string data` + `int8[] binary` |
| `ResponseStatus.msg` | `int32 code`（0=成功，其余为错误码，见 `ut_errror.hpp`） |

### unitree_hg（G1 人形低层）

`LowCmd` / `LowState`、`MotorCmd` / `MotorState`、`HandCmd` / `HandState`、`IMUState`、`BmsCmd` / `BmsState`、`MainBoardState`、`PressSensorState`、`AgvBmsState`

### unitree_go（四足）

`SportModeState` / `SportModeCmd`、`LowCmd` / `LowState`、`MotorCmd(s)` / `MotorState(s)`、`LidarState`、`Go2FrontVideoData`、`AudioData`、`WirelessController`、`UwbState` / `UwbSwitch`、`HeightMap`、`PathPoint`、`InterfaceConfig`、`ConfigChangeStatus`、`SymState`、`Error`

## 公共客户端（example/src/include/common）

| 头文件 | 作用 |
|---|---|
| `base_client.hpp` | API 客户端基类：持有 `rclcpp::Node*`、request/response 话题名、`Publisher<Request>`/`Subscription<Response>`，负责拼装 `Request` 与调用 id |
| `b2_base_client.hpp` / `ros2_b2_sport_client.h` | B2 运动客户端 |
| `ros2_sport_client.h` | 通用运动指令客户端（_balance、_move 等 API 封装） |
| `ros2_robot_state_client.h` | 机器人状态查询客户端 |
| `motor_crc.h` / `motor_crc_hg.cpp` | 底层 `LowCmd` 电机帧的 CRC 校验（四足 / 人形两版） |
| `patch.hpp`、`time_tools.hpp`、`ut_errror.hpp` | 补丁、时间工具、错误码 |

## 示例程序（example/src/src）

| 目录/文件 | 内容 |
|---|---|
| `g1/` | **audio_client**、**dex3**（三指灵巧手）、**high_level**（高层运动控制）、**lowlevel**（低层关节控制） |
| `go2/`、`b2/`、`b2w/`、`h1-2/`、`h2/` | 其他机型示例 |
| `common/` | `motor_crc.cpp`、`ros2_sport_client.cpp` 等实现 |
| 独立 demo | `low_level_ctrl.cpp`、`read_low_state.cpp`、`read_low_state_hg.cpp`、`read_motion_state.cpp`、`read_wireless_controller.cpp`、`record_bag.cpp` |

## 未安装项

- `unitree_sdk2`（C++ 原生 SDK）：`~/unitree_sdk2` 与 `/usr/local/include`、`/usr/local/lib` 下均未发现——本机只用 **ROS 2 消息直连**路线。
