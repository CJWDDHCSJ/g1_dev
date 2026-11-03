# 依赖与第三方组件

## 1. 运行时环境

| 项目 | 版本/说明 |
|---|---|
| 硬件 | Unitree G1-EDU，Jetson Orin NX，ARM64 |
| 系统 | Ubuntu 22.04.5 LTS |
| ROS 2 | Humble（`rmw_cyclonedds_cpp`） |
| 加速环境 | JetPack 6.2.3 / CUDA 12.6 / TensorRT 10.3（本项目未直接调用） |

## 2. 代码依赖

| 依赖 | 用途 | 获取方式 | 许可/来源 |
|---|---|---|---|
| `rclcpp`、`std_msgs` | 节点与消息 | ROS 2 Humble apt | Apache-2.0 |
| `unitree_hg` / `unitree_go` 消息 | `/lowstate`、`/sportmodestate`、`/lf/bmsstate` | `unitree_ros2` 官方工作区 | Unitree 官方仓库，**不随本仓库分发** |
| `unitree_sdk2` / `AudioClient` | TTS 调用 `TtsMaker` | Unitree SDK2 | Unitree 官方，**不随本仓库分发** |
| `nlohmann/json`（header-only） | 知识库/状态/动作 JSON | `workspace/local_include/` 或系统包 | MIT |
| `libcurl` | HTTP 调用 Ollama | `libcurl4-openssl-dev` | curl 许可证 |
| `pthread` | worker 线程与条件变量 | 系统库 | — |
| `ament_index_cpp` | 定位安装后的知识库 | ROS 2 包 | Apache-2.0 |

## 3. 外部服务

| 服务 | 位置 | 用途 | 说明 |
|---|---|---|---|
| Ollama | **机器人外部主机**（局域网 HTTP） | `/api/chat` 通用问答 | 模型 `qwen3-vl:4b-instruct`；G1 本机不运行 |

地址通过 ROS 参数 `ollama_url` 配置；仓库示例与文档统一使用 `<LLM_HOST>` 占位，**不写入真实内网地址**。

## 4. 上游项目链接

| 项目 | 链接 | 说明 |
|---|---|---|
| ROS 2 Humble | https://docs.ros.org/en/humble/ | 中间件与构建系统 |
| Unitree `unitree_ros2` | https://github.com/unitreerobotics/unitree_ros2 | 消息定义与 `setup.sh` |
| Unitree SDK2 | https://github.com/unitreerobotics/unitree_sdk2 | 语音/动作底层客户端 |
| CycloneDDS | https://github.com/eclipse-cyclonedds/cyclonedds | DDS 实现 |
| Ollama | https://github.com/ollama/ollama | 本地模型服务 |
| nlohmann/json | https://github.com/nlohmann/json | JSON 库 |

## 5. 不在本仓库中的内容

- Unitree SDK / `unitree_ros2` 全量源码与预编译库（第三方授权）
- 构建产物 `build/`、`install/`、`log/`
- 生产二进制与真实配置（见 `private_archive/`，本地保留）
- 真实实验室知识库（公开仓库仅提供 `lab_knowledge.example.json`）

## 6. 许可证状态

本项目**尚未指定开源许可证**。第三方组件遵循各自许可证；使用/分发前请分别确认。
