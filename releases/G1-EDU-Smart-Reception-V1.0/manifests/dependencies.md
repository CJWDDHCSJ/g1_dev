# 依赖清单

## 1. 运行时环境（G1 端）

| 项目 | 版本/说明 |
|---|---|
| 硬件 | Unitree G1-EDU，Jetson Orin NX，ARM64 / aarch64 |
| 操作系统 | Ubuntu 22.04.5 LTS |
| 中间件 | ROS 2 Humble，RMW = `rmw_cyclonedds_cpp`（CycloneDDS） |
| 加速环境 | JetPack 6.2.3 / CUDA 12.6 / TensorRT 10.3（本项目未直接使用 CUDA/TensorRT） |
| 内核 | 5.15.185-tegra |

## 2. 系统库与第三方头文件

| 依赖 | 用途 | 获取方式 |
|---|---|---|
| `rclcpp`、`std_msgs`、`sensor_msgs` | ROS 2 节点与消息 | ROS 2 Humble apt 包 |
| `nlohmann/json`（header-only） | 知识库、状态 JSON、动作请求解析 | 随工作区提供：`~/unitree_ros2/example/src/include/nlohmann/json.hpp`（上游：https://github.com/nlohmann/json ，MIT） |
| `libcurl` | 调用 Ollama HTTP 接口 | 系统包 `libcurl4-openssl-dev` |
| `pthread` | worker 线程、条件变量 | 系统库 |
| `ament_index_cpp` | 定位安装后的 `lab_knowledge.json` | ROS 2 包 |

## 3. Unitree 官方组件

| 组件 | 用途 | 说明 |
|---|---|---|
| `unitree_hg` / `unitree_go` 消息包 | `/lowstate`、`/sportmodestate` 等消息类型 | 位于 `~/unitree_ros2/cyclonedds_ws`；**不随本仓库分发**，需按 Unitree 官方 `unitree_ros2` 仓库说明安装 |
| `unitree_sdk2` / `AudioClient` | TTS 调用 `TtsMaker` | 位于 `~/unitree_sdk2_transfer/unitree_sdk2-main`；**不随本仓库分发** |
| `g1_tts_node.cpp` | TTS 节点源码 | 基于 Unitree 官方 `audio_client` 示例（`unitree_ros2/example/src/src/g1/audio_client`），本项目仅调整配置读取与分句播报逻辑，未改动 `AudioClient` 核心 |

## 4. 外部模型服务（非 G1 本地）

| 项目 | 说明 |
|---|---|
| Ollama | 部署在开发笔记本（与 G1 通过以太网直连），提供 `/api/chat` |
| 模型 | `qwen3-vl:4b-instruct`（Q4_K_M） |
| 地址 | 由 ROS 参数 `ollama_url` 配置，默认值见源码；仓库内一律使用占位符 |

> 注意：G1 本机**不运行** Ollama。README 与文档中出现的地址均为占位符。

## 5. 构建依赖

```bash
sudo apt install -y build-essential cmake libcurl4-openssl-dev python3-colcon-common-extensions
```

ROS 2 环境：

```bash
source /opt/ros/humble/setup.bash
source ~/unitree_ros2/setup.sh          # Unitree 提供
source ~/unitree_ros2/cyclonedds_ws/install/setup.bash
```

## 6. 不在本仓库中的内容

| 内容 | 原因 |
|---|---|
| Unitree SDK2 源码与预编译库 | 第三方授权，需从官方渠道获取 |
| `unitree_ros2` 工作区整体 | 官方仓库，非本项目产出 |
| `build/`、`install/`、`log/` | 构建产物，不应入库 |
| 生产二进制 | 仅保存在本地 `private_archive/` |
| 真实实验室知识库 | 涉及内部资料，公开仓库仅提供 `lab_knowledge.example.json` |
