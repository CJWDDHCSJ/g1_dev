# G1 环境准备

## 1. 系统基线

| 项目 | 值 |
|---|---|
| 机器 | Unitree G1-EDU，Jetson Orin NX |
| 系统 | Ubuntu 22.04.5 LTS |
| ROS 2 | Humble |
| JetPack / CUDA / TensorRT | 6.2.3 / 12.6 / 10.3 |
| 架构 | aarch64 |
| DDS | CycloneDDS（`rmw_cyclonedds_cpp`） |
| 生产 Domain | 0（测试用 77） |

## 2. 工作区

```bash
# Unitree 官方（需自行安装，不入库）
~/unitree_ros2/                 # 含 setup.sh、example/、cyclonedds_ws/
~/unitree_sdk2_transfer/unitree_sdk2-main/   # AudioClient（TTS 依赖）

# 本项目
~/unitree_ros2/example/g1_voice_router
~/unitree_ros2/example/g1_asr_bridge
~/unitree_ros2/example/src/src/g1/audio_client/g1_tts_node.cpp
~/lidar_ws/src/g1_action_executor
```

## 3. 环境变量

```bash
source /opt/ros/humble/setup.bash
source ~/unitree_ros2/setup.sh
source ~/unitree_ros2/cyclonedds_ws/install/setup.bash
export ROS_DOMAIN_ID=0
export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
```

> `setup.sh` 由 Unitree 提供，**不要全局修改**；必要时用 `env -u LD_LIBRARY_PATH` 做单次命令隔离，避免 ROS 的 `libddsc` 与 SDK2 的 `libddscxx` 冲突。

## 4. 依赖检查

```bash
ros2 --version
python3 -c "import rclpy; print('rclpy ok')"
pkg-config --modversion libcurl
ls ~/unitree_ros2/cyclonedds_ws/install/unitree_hg/share/unitree_hg/msg/LowState.msg
```

## 5. 网络与外部模型

- G1 与笔记本通过以太网直连；Router 通过 ROS 参数 `ollama_url` 指定模型地址。
- 连通性检查（把 `<LLM_HOST>` 换成实际地址）：

```bash
curl -s http://<LLM_HOST>:11434/api/tags | head
```

- 生产环境**不在 G1 上运行 Ollama**。
