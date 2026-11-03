# 04 — 接口清单（ROS 2 话题 / API / 网络）

> 来源：`~/ros2_audit/topics_with_types.txt`（2026-10-26 采集的 ROS 图快照，111 个话题）+ 本次只读复查。采集当时可见节点为 `/ros_bridge`（宇树内部服务通过它把 `/api/*` 暴露到 ROS 图）。

## 1. 高层控制 API —— `/api/<服务名>/request|response`

请求/响应成对出现，类型为 `unitree_api/msg/Request` / `Response`（协议见 [03-unitree-sdk.md](03-unitree-sdk.md)）。快照中存在的服务：

| 服务 | 领域 |
|---|---|
| `loco` | 运动控制（G1 主运动接口，平衡/步态/位姿指令） |
| `sport` | 运动指令（兼容四足风格） |
| `arm` / `arm_sdk` | 手臂控制 |
| `motion_switcher` | 模式切换器（高层/低层控制权切换） |
| `robot_state` / `robot_type_service` / `config` | 状态查询、机型、配置 |
| `slam_operate` | SLAM 建图/重定位操作 |
| `videohub` / `audiohub` | 视频流、音频路由 |
| `voice` / `vui` / `gpt` / `gesture` | 语音交互、VUI、GPT 指令、手势 |
| `dex3_msg_controller` | Dex3 灵巧手控制 |
| `action_store` / `basic_demarcate(lease)` | 动作库、标定/租约 |
| `bashrunner` / `rm_con` | 运维脚本执行、远程连接管理 |

## 2. 底层实时接口（unitree_hg 低层总线风格）

| 话题 | 类型 | 说明 |
|---|---|---|
| `/lowcmd` / `/lowstate` | `unitree_hg/msg/LowCmd` / `LowState` | 关节级命令/全状态（CRC 校验，见 `motor_crc_hg`） |
| `/lf/lowstate` | `unitree_go`+`unitree_hg` LowState 双类型 | 低频转发层（"lf"） |
| `/loco_sdk` | `unitree_hg/msg/LowState` | loco 对外的 SDK 状态 |
| `/arm_sdk` / `/armsdk` | `unitree_hg/msg/LowCmd` | 手臂 SDK 命令 |
| `/lowstate_doubleimu` | `unitree_hg_doubleimu/msg/doubleIMUState` | 双 IMU 状态 |
| `/secondary_imu`、`/lf/secondary_imu` | `unitree_hg/msg/IMUState` | 副 IMU |
| `/lf/*`（`bmsstate`、`agvbmsstate`、`battery_alarm`、`mainboardstate`、`sportmodestate`、`odommodestate`、`dex3/*/state`） | 见名 | 电池、主板、运动模式、灵巧手状态的低频镜像 |

## 3. 运动与里程计状态

| 话题 | 类型 |
|---|---|
| `/sportmodestate`、`/lf/sportmodestate` | `unitree_hg/msg/SportModeState`（`/lf/` 上兼有 `unitree_go` 版） |
| `/odommodestate`、`/lf/odommodestate` | `unitree_go/msg/SportModeState` |
| `/dog_odom` | `nav_msgs/msg/Odometry` |
| `/dog_imu_raw` | `sensor_msgs/msg/Imu` |

## 4. 雷达与感知 / SLAM

| 话题 | 类型 |
|---|---|
| `/utlidar/cloud_livox_mid360` | `sensor_msgs/msg/PointCloud2`（Livox Mid360 点云） |
| `/utlidar/imu_livox_mid360` | `sensor_msgs/msg/Imu` |
| `/utlidar/range_info` | `geometry_msgs/msg/PointStamped` |
| `/collision_clouds`、`/pre_collision_clouds`、`/safe_clouds`、`/pre_safe_clouds`、`/warning_clouds`、`/no_warning_clouds`、`/grid_clouds`、`/ele_clouds` | `sensor_msgs/msg/PointCloud2`（各类避障/安全点云） |
| `/global_map` | `nav_msgs/msg/OccupancyGrid` |
| `/gridmap`、`/planner_map` | `grid_map_msgs/msg/GridMap` |
| `/unitree/slam_mapping/odom`、`/unitree/slam_mapping/points`、`/unitree/slam_relocation/odom`、`/unitree/slam_relocation/points` | 建图/重定位输出 |
| `/unitree_slam/waypoints`、`/slam_info`、`/slam_key_info` | 航点与 SLAM 状态 |

## 5. 灵巧手（Dex3）

| 话题 | 类型 |
|---|---|
| `/dex3/left/cmd`、`/dex3/right/cmd` | `unitree_hg/msg/HandCmd` |
| `/dex3/left/state`、`/dex3/right/state` | `unitree_hg/msg/HandState` |

## 6. 视频 / 音频 / 语音链路

| 话题 | 类型 |
|---|---|
| `/frontvideostream` | `unitree_go/msg/Go2FrontVideoData` |
| `/videohub/inner` | `std_msgs/msg/String` |
| `/audiosender` | `unitree_go/msg/AudioData` |
| `/audio_msg`、`/audio_msg/filter` | `std_msgs/msg/String` |
| `/webrtcreq`、`/webrtcremoji…res`、`/rtc/state`、`/rtc_status` | `std_msgs/msg/String`（WebRTC 语音链路） |

## 7. 系统杂项

`/servicestate`、`/servicestateactivate`、`/multiplestate`、`/selftest`（`std_msgs/String` 状态类）；`/log_system_inbound`、`/log_system_outbound`；`/public_network_status`；`/wirelesscontroller`（`unitree_go/msg/WirelessController` 遥控器）；`/SymState`、`/config_change_status`；`/parameter_events`、`/rosout`。

## 8. 网络入口

| 通道 | 地址 |
|---|---|
| SSH（本机 → 机器人） | `ssh unitree@192.168.123.164`（eth0 内网，免密已配） |
| 机器人内网 | eth0 `192.168.123.164/24` |
| 外网 | wlan0 `10.44.6.22/24`（默认路由） |
| CAN | `can0` 存在但 DOWN |

## QoS 观察（快照抽样）

`/api/*` 与状态类话题均为 **RELIABLE + VOLATILE**；实时性要求高的低层话题如需最佳实践应改 BEST_EFFORT 订阅，接入前建议实测。
