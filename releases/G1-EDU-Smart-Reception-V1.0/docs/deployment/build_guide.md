# 编译指南

> 命令基于 `deployment/scripts/*.sh` 中的真实二进制路径与 P0-18/P0-21 的构建记录。标注“待确认”的步骤需要在你的环境中核对后执行。

## 1. 放置源码

```bash
cd G1-EDU-Smart-Reception-V1.0

cp -r ros2_ws/src/g1_voice_router      ~/unitree_ros2/example/
cp -r ros2_ws/src/g1_asr_bridge        ~/unitree_ros2/example/
cp -r action_ws/src/g1_action_executor ~/lidar_ws/src/
cp integrations/unitree_audio/g1_tts_node.cpp ~/unitree_ros2/example/src/src/g1/audio_client/
cp integrations/unitree_audio/config/*.json   ~/unitree_ros2/example/config/
```

## 2. 构建主业务包与 ASR 桥

```bash
source /opt/ros/humble/setup.bash
source ~/unitree_ros2/setup.sh
source ~/unitree_ros2/cyclonedds_ws/install/setup.bash

cd ~/unitree_ros2/example
colcon build --packages-select g1_voice_router g1_asr_bridge --symlink-install
```

`--symlink-install` 是当前生产使用的方式：`install/` 下是指向 `build/` 的符号链接，改源码重新 build 即生效（启动脚本直接执行 `install/.../voice_router`）。

构建目标（见 `ros2_ws/src/g1_voice_router/CMakeLists.txt`）：

| 目标 | 类型 | 来源 |
|---|---|---|
| `voice_router` | 可执行 | `voice_router.cpp` + `intent_recognizer.cpp` |
| `robot_state_provider` | 可执行 | `robot_state_provider.cpp` + `_node.cpp` |
| `robot_state_evaluator` | 静态库 | 评估器 |
| `interaction_manager` | 静态库 | 队列 + 过滤 + 动作映射 + ASR 归一化 |
| `knowledge_base` | 静态库 | 知识库 |
| `fact_risk_guard` | 静态库 | 风控 |
| 7 个测试目标 | 可执行 | `test/*.cpp`（`BUILD_TESTING=ON` 时） |

同时会安装知识库：

```cmake
install(FILES config/lab_knowledge.json DESTINATION share/${PROJECT_NAME}/config)
```

Router 通过 `ament_index_cpp::get_package_share_directory("g1_voice_router")` 定位它，因此**不要手工移动**该文件；如需外部路径，用参数 `knowledge_file` 覆盖。

## 3. 构建动作执行器（独立工作区）

```bash
cd ~/lidar_ws
colcon build --packages-select g1_action_executor --symlink-install
```

## 4. 构建 TTS 节点

`g1_tts_node.cpp` 放在 Unitree 示例包 `unitree_ros2_example` 内编译（生产 `ExecStart` 指向 `install/unitree_ros2_example/lib/unitree_ros2_example/g1_tts_node`）：

```bash
cd ~/unitree_ros2/example
colcon build --packages-select unitree_ros2_example --symlink-install
```

> **待确认**：把 `g1_tts_node.cpp` 加入 `unitree_ros2_example` 需要在官方包的 `CMakeLists.txt` 中新增 `add_executable` 与 `install(TARGETS ...)`。该 CMakeLists 属于 Unitree 官方仓库、不在本归档内，请按你机器上的实际内容补两行声明后再编译。

## 5. 测试（必须全绿）

```bash
cd ~/unitree_ros2/example
colcon test --packages-select g1_voice_router
colcon test-result --verbose
```

期望：

```text
Summary: 7 tests, 0 errors, 0 failures, 0 skipped
```

各套件断言数（G1 ARM64 实测）：22 + 39 + 76 + 96 + 119 + 57 + 44 = **453**。

## 6. 构建产物位置

| 产物 | 路径 |
|---|---|
| `voice_router` | `~/unitree_ros2/example/build/g1_voice_router/voice_router`（`install/` 下为符号链接） |
| `robot_state_provider` | `~/unitree_ros2/example/build/g1_voice_router/robot_state_provider` |
| `asr_json_bridge` | `~/unitree_ros2/example/build/g1_asr_bridge/asr_json_bridge` |
| `g1_tts_node` | `~/unitree_ros2/example/build/unitree_ros2_example/g1_tts_node` |
| `g1_action_executor_node` | `~/lidar_ws/build/g1_action_executor/g1_action_executor_node` |

## 7. 产物的可校验性

```bash
sha256sum ~/unitree_ros2/example/build/g1_voice_router/voice_router
# V1.0 冻结值：6ab99de8ceb287d1fdb58b66399874c60b41734775a74a0c2745bf64223d6bb7
```

其他四个组件的冻结哈希见 `V1.0_RELEASE_NOTES.md`。注意：源码改动后哈希必然变化，**不要用旧哈希校验新构建**。

## 8. 编译排错

| 现象 | 原因 | 处理 |
|---|---|---|
| 找不到 `nlohmann/json.hpp` | 头文件不在 include 路径 | 核对 `~/unitree_ros2/example/src/include/nlohmann/json.hpp` 是否存在 |
| `unitree_hg/msg/*.hpp` 找不到 | 未 source `cyclonedds_ws` | 先 source 再 build |
| 链接 `libcurl` 失败 | 未装 `libcurl4-openssl-dev` | apt 安装后重新 build |
| 改了代码行为没变 | 未重新 build，或进程未重启 | `colcon build` 后 `systemctl --user restart <单服务>` |
| 测试数量少于 453 | `BUILD_TESTING` 未开 | 默认由 `include(CTest)` 开启，确认 `CMAKE_BUILD_TYPE` 未被覆盖 |
| 交叉编译产物无法运行 | 在 x86 上构建 ARM 目标 | 所有构建、测试必须在 G1（aarch64）上执行 |
