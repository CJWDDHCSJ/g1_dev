# 环境准备

> 命令均来自 `deployment/scripts/`（真实 `ExecStart` 的 source 顺序）与 P0-18 系列的构建/运行记录。

## 1. 系统基线（实测）

| 项目 | 值 |
|---|---|
| 机器人 | Unitree G1-EDU，Jetson Orin NX（16 GB） |
| 系统 | Ubuntu 22.04.5 LTS |
| ROS 2 | Humble |
| JetPack / CUDA / TensorRT | 6.2.3 / 12.6 / 10.3（CUDA 与 TensorRT 本项目未使用） |
| 架构 | aarch64 |
| DDS | CycloneDDS（`rmw_cyclonedds_cpp`） |
| 生产 Domain | `ROS_DOMAIN_ID=0`（测试用 77） |

## 2. 目录与工作区布局

```text
~/unitree_ros2/                              # Unitree 官方工作区（需自行安装）
├── setup.sh                                 # 提供 ROS_DOMAIN_ID / RMW / DDS 配置
├── cyclonedds_ws/install/setup.bash         # unitree_hg 等消息包
└── example/
    ├── src/g1_voice_router/                 # ← 本仓库 ros2_ws/src/g1_voice_router
    ├── src/g1_asr_bridge/                   # ← 本仓库 ros2_ws/src/g1_asr_bridge
    ├── src/src/g1/audio_client/g1_tts_node.cpp   # ← 本仓库 integrations/unitree_audio/
    └── config/tts_config.json               # ← 本仓库 integrations/unitree_audio/config/

~/lidar_ws/src/g1_action_executor/           # ← 本仓库 action_ws/src/g1_action_executor
~/g1_systemd/{scripts,monitor}/              # ← 本仓库 deployment/{scripts,monitoring}
~/.config/systemd/user/                      # ← 本仓库 deployment/systemd
~/.config/logrotate/g1-voice-stack.conf      # ← 本仓库 deployment/logrotate
~/g1_logs_systemd/                           # 运行期日志目录（需存在）
```

## 3. 环境加载顺序（必须按此顺序）

```bash
source /opt/ros/humble/setup.bash
source ~/unitree_ros2/setup.sh
source ~/unitree_ros2/cyclonedds_ws/install/setup.bash

export ROS_DOMAIN_ID=0
export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
```

说明：

- `setup.sh` 由 Unitree 提供，包含网卡绑定与 DDS 相关环境，**不要全局修改**；
- 顺序不能颠倒：先 ROS 基础环境，再 Unitree 覆盖项，最后消息包；
- 若遇 SDK2 动态库冲突（典型现象 `free(): invalid pointer`），用单次命令隔离，不要改全局环境：
  `env -u LD_LIBRARY_PATH -u LD_PRELOAD <command>`。

## 4. 依赖安装

```bash
sudo apt update
sudo apt install -y build-essential cmake libcurl4-openssl-dev python3-colcon-common-extensions

# 日志轮转（P0-18.4 时为未安装状态，由用户手工安装）
sudo apt-get install -y logrotate
```

`nlohmann/json` 从工作区获取（`~/unitree_ros2/example/src/include/nlohmann/json.hpp`），或安装 `nlohmann-json3-dev`。

## 5. 连通性检查

```bash
# 外部模型服务（替换为实际主机；不要把真实地址写进公开文档）
curl -s http://<LLM_HOST>:11434/api/tags | head

# ROS 图
ros2 node list
ros2 topic list | grep '/g1/'

# 原生数据是否在
ros2 topic hz /lowstate        # 期望约 1000 Hz 量级
ros2 topic hz /lf/bmsstate     # 期望约 20 Hz
```

## 6. 日志目录与 linger

```bash
mkdir -p ~/g1_logs_systemd
loginctl enable-linger "$USER"     # 未登录也能启动用户级服务
loginctl show-user "$USER" -p Linger   # 期望 Linger=yes
```

## 7. 常见环境问题

| 现象 | 原因 | 处理 |
|---|---|---|
| `ros2: command not found` | 未 source `/opt/ros/humble/setup.bash` | 按 §3 顺序 source |
| 找不到 `unitree_hg/msg/LowState` | 未 source `cyclonedds_ws` | 补 source 后重新 `colcon build` |
| 节点互相看不见 | `ROS_DOMAIN_ID` 不一致 / `CYCLONEDDS_URI` 网卡不对 | 统一 Domain；核对网卡 |
| SDK2 示例 `free(): invalid pointer` | ROS 的 `libddsc` 与 SDK2 的 `libddscxx` 混用 | 单次 `env -u LD_LIBRARY_PATH` 隔离 |
| `logrotate: command not found` | 系统未安装 | `sudo apt-get install -y logrotate` |
| 用户服务重启后不启动 | 未启用 linger | `loginctl enable-linger $USER` |
