# 依赖说明

> 与 `manifests/dependencies.md` 内容一致，本文补充“为什么需要它”和“缺失时会怎样”。

## 1. 运行时环境（G1 端）

| 项目 | 版本 / 说明 | 验证方式 |
|---|---|---|
| 硬件 | Unitree G1-EDU，Jetson Orin NX 16 GB | 真机 |
| 系统 | Ubuntu 22.04.5 LTS | `lsb_release -a` |
| ROS 2 | Humble | `ros2 --version` |
| RMW | `rmw_cyclonedds_cpp`（CycloneDDS） | `echo $RMW_IMPLEMENTATION` |
| 加速环境 | JetPack 6.2.3 / CUDA 12.6 / TensorRT 10.3 | 本项目**未直接使用** CUDA/TensorRT |
| 内核 | 5.15.185-tegra | `uname -r`（P0-17 记录） |
| 生产 Domain | `ROS_DOMAIN_ID=0` | P0-18 起固定 |

## 2. 编译期与运行期依赖

| 依赖 | 用途 | 获取方式 | 缺失后果 | 许可证 |
|---|---|---|---|---|
| `rclcpp` | 节点、话题、参数、定时器 | ROS 2 Humble apt | 无法构建 | Apache-2.0 |
| `std_msgs` | 业务话题消息类型 | ROS 2 Humble apt | 无法构建 | Apache-2.0 |
| `ament_cmake` / `ament_index_cpp` | 构建系统、定位安装后的 `lab_knowledge.json` | ROS 2 Humble apt | 无法构建 / 找不到知识库 | Apache-2.0 |
| `unitree_hg` | `/lowstate`、`/sportmodestate`、`/lf/bmsstate` 消息 | Unitree `unitree_ros2` 工作区 | Provider 与 Executor 无法构建 | 随 Unitree 官方仓库 |
| `nlohmann/json`（header-only） | 知识库解析、状态 JSON、动作状态解析 | 随工作区提供（`src/include/nlohmann/json.hpp`）或 `nlohmann-json3-dev` | 编译失败 | MIT |
| `libcurl` | 调用 Ollama HTTP 接口 | `libcurl4-openssl-dev` | 链接失败 | curl 许可证 |
| `Threads`（pthread） | worker 线程、条件变量 | 系统库 | 链接失败 | 系统 |
| `logrotate` | 日志轮转 | `sudo apt-get install -y logrotate`（3.19.0 起可用） | 日志只增不减 | GPL-2.0（系统工具，非本项目分发） |

> `logrotate` 是**运行时可选**依赖：P0-18.4 审计时 G1 未安装，由用户手工安装后启用；未安装时 systemd timer 会失败但不影响语音业务。

## 3. Unitree 官方组件

| 组件 | 用途 | 来源 | 说明 |
|---|---|---|---|
| `unitree_ros2` 工作区 | 消息定义、`setup.sh`、示例包 `unitree_ros2_example` | https://github.com/unitreerobotics/unitree_ros2 | **不随本仓库分发**，需按官方说明安装 |
| `unitree_sdk2` / SDK2 客户端 | `AudioClient::TtsMaker`、`G1ArmActionClient` | https://github.com/unitreerobotics/unitree_sdk2 | **不随本仓库分发** |
| `g1_tts_node.cpp` 基线 | TTS 节点原型 | `unitree_ros2/example/src/src/g1/audio_client` | 本项目在其上增加配置读取、分句、节奏控制；核心 `AudioClient` 未改 |

## 4. 外部模型服务

| 项目 | 说明 |
|---|---|
| 服务 | Ollama，`POST /api/chat`，`stream=false` |
| 位置 | **机器人外部主机**（开发笔记本），不在 Jetson 上 |
| 模型 | `qwen3-vl:4b-instruct`（Q4_K_M，约 3.34 GB） |
| 参数 | `temperature=0.2`、`num_predict=160` |
| 地址配置 | ROS 参数 `ollama_url`；源码保留一个内网默认值（与冻结二进制一致），公开文档统一用 `http://<LLM_HOST>:11434/api/chat` |
| 缺失后果 | 通用问答降级为“暂时无法连接到对话模型”；本地能力不受影响 |

## 5. 环境加载与构建

```bash
sudo apt update
sudo apt install -y build-essential cmake libcurl4-openssl-dev python3-colcon-common-extensions

source /opt/ros/humble/setup.bash
source ~/unitree_ros2/setup.sh
source ~/unitree_ros2/cyclonedds_ws/install/setup.bash
```

构建：

```bash
cd ~/unitree_ros2/example
colcon build --packages-select g1_voice_router g1_asr_bridge --symlink-install

cd ~/lidar_ws
colcon build --packages-select g1_action_executor --symlink-install
```

## 6. 不在本仓库中的内容

| 内容 | 原因 |
|---|---|
| `unitree_ros2` / `unitree_sdk2` 源码与预编译库 | 第三方授权，需从官方渠道获取 |
| `build/`、`install/`、`log/` | 构建产物，不入库（`.gitignore`） |
| 生产二进制与真实配置 | 仅保存在 `private_archive/`，已从公开仓库排除 |
| 真实实验室知识库 | 涉及内部资料，公开仓库仅提供脱敏示例 `deployment/config_examples/lab_knowledge.example.json` |

## 7. 许可证状态

- 本项目**未指定开源许可证**，未添加 `LICENSE` 文件；代码与文档版权归作者所有。
- `package.xml` 中的 `<license>Apache-2.0</license>` 为模板遗留占位，**不代表**本项目已选择该许可证。
- 第三方组件遵循各自许可证；本项目不复制其源码，仅在 G1 上与其链接或调用。
