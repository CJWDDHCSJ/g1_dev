# 环境准备

## 1. 系统基线（实测）

| 项目 | 值 |
|---|---|
| 机器人 | Unitree G1-EDU，Jetson Orin NX（16GB） |
| 系统 | Ubuntu 22.04.5 LTS |
| ROS 2 | Humble |
| JetPack / CUDA / TensorRT | 6.2.3 / 12.6 / 10.3 |
| 架构 | aarch64 |
| DDS | CycloneDDS（`rmw_cyclonedds_cpp`） |
| 生产 Domain | `ROS_DOMAIN_ID=0` |

## 2. 工作区

```bash
# Unitree 官方（需自行安装，不随本仓库分发）
~/unitree_ros2/                  # setup.sh、example/、cyclonedds_ws/
~/unitree_sdk2_transfer/unitree_sdk2-main/   # AudioClient（TTS 依赖）

# 本项目（两个独立工作区）
~/unitree_ros2/example/g1_voice_router
~/unitree_ros2/example/g1_asr_bridge
~/unitree_ros2/example/src/src/g1/audio_client/g1_tts_node.cpp
~/lidar_ws/src/g1_action_executor
```

## 3. 环境加载顺序

```bash
source /opt/ros/humble/setup.bash
source ~/unitree_ros2/setup.sh
source ~/unitree_ros2/cyclonedds_ws/install/setup.bash
export ROS_DOMAIN_ID=0
export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
```

> `setup.sh` 由 Unitree 提供，**不要全局修改**。若遇到 SDK2 动态库冲突，用 `env -u LD_LIBRARY_PATH -u LD_PRELOAD <cmd>` 做单次隔离，而不是改环境。

## 4. 依赖安装

```bash
sudo apt update
sudo apt install -y build-essential cmake libcurl4-openssl-dev python3-colcon-common-extensions
```

## 5. 连通性检查

```bash
# 外部模型服务（替换为实际主机）
curl -s http://<LLM_HOST>:11434/api/tags | head

# ROS 图
ros2 topic list | grep '/g1/'
ros2 node list
```

## 6. 常见环境问题

| 现象 | 原因 | 处理 |
|---|---|---|
| `ros2: command not found` | 未 source `/opt/ros/humble/setup.bash` | 按 §3 顺序 source |
| 找不到 `unitree_hg/msg/LowState` | 未 source `cyclonedds_ws` | 补 source，或重新编译该工作区 |
| 节点互相看不见 | `ROS_DOMAIN_ID` 不一致 / 网卡未绑定 | 统一 Domain；检查 `CYCLONEDDS_URI` |
| SDK2 示例 `free(): invalid pointer` | ROS 的 `libddsc` 与 SDK2 的 `libddscxx` 混用 | 单次命令 `env -u LD_LIBRARY_PATH` |
