# 宇树 G1 机器人软件清单（robot-inventory）

> 采集方式：2026-10-27 通过 `ssh unitree@192.168.123.164` 执行**只读**命令（cat / ls / dpkg / systemctl list 等），全程未修改机器人上的任何文件。接口部分复用了机器人上 `~/ros2_audit/` 里 2026-10-26 的 ROS 图快照。

## 文档索引

| 文件 | 内容 |
|---|---|
| [01-system.md](01-system.md) | 系统：Ubuntu 22.04.5、JetPack 6.2.3（L4T R36.5）、CUDA 12.6、硬件概况 |
| [02-ros2.md](02-ros2.md) | ROS 2 Humble + CycloneDDS、机载工作区（unitree_ros2 / g1_sensor_test / g1_slam_trial / ros2_audit）、开机服务、网络接口 |
| [03-unitree-sdk.md](03-unitree-sdk.md) | unitree_ros2 v0.3.0：unitree_api / unitree_go / unitree_hg 消息包、公共客户端、示例工程 |
| [04-interfaces.md](04-interfaces.md) | 接口：/api/* 高层控制 API、底层 LowCmd/LowState、雷达/SLAM/灵巧手/音视频话题全表、网络入口、QoS 观察 |

## 一页速览

- **机型推断**：宇树 **G1 人形机器人**（Cortex-A78AE Jetson Orin 级机载计算 + Dex3 灵巧手话题 + Livox Mid360 雷达 + G1 SLAM/传感器工作区）
- **软件栈**：Ubuntu 22.04.5 → JetPack 6.2.3 / CUDA 12.6 → ROS 2 Humble（RMW=CycloneDDS）→ unitree_ros2 v0.3.0（ROS 消息直连，未装 unitree_sdk2 原生库）
- **控制入口**：高层走 `/api/<service>/request|response`（unitree_api Request/Response + JSON 参数）；低层走 `/lowcmd`/`/lowstate`（unitree_hg，需 CRC）
- **常驻服务**：`master_service.service`（/etc/init.d，开机自启托管内部功能服务）
- **网络**：内网 eth0 `192.168.123.164/24`；外网 wlan0；本机已配 SSH 免密

## 采集约束

- 全部命令为只读查看；未安装/修改/删除机器人上任何文件
- 未启动任何机器人上的服务或节点（接口数据取自既有审计快照）
