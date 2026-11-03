# 编译指南

## 1. 放置源码

```bash
cd G1-EDU-Smart-Reception-V1.0

cp -r ros2_ws/src/g1_voice_router    ~/unitree_ros2/example/
cp -r ros2_ws/src/g1_asr_bridge      ~/unitree_ros2/example/
cp -r action_ws/src/g1_action_executor ~/lidar_ws/src/
cp integrations/unitree_audio/g1_tts_node.cpp \
   ~/unitree_ros2/example/src/src/g1/audio_client/
cp integrations/unitree_audio/config/*.json ~/unitree_ros2/example/config/
```

## 2. 构建（两个工作区分别执行）

```bash
source /opt/ros/humble/setup.bash
source ~/unitree_ros2/setup.sh

# 主业务包 + ASR 桥
cd ~/unitree_ros2/example
colcon build --packages-select g1_voice_router g1_asr_bridge --symlink-install

# 动作执行器（独立工作区）
cd ~/lidar_ws
colcon build --packages-select g1_action_executor --symlink-install
```

> `--symlink-install` 是当前生产使用的方式：`install/` 下是指向 `build/` 的符号链接，改代码后重新 build 即生效。

## 3. 测试（必须全绿）

```bash
cd ~/unitree_ros2/example
colcon test --packages-select g1_voice_router
colcon test-result --verbose
```

期望输出：

```text
Summary: 7 tests, 0 errors, 0 failures, 0 skipped
```

各套件断言数（G1 ARM64 实测）：22 + 39 + 76 + 96 + 119 + 57 + 44 = **453**。

## 4. 构建产物位置

| 产物 | 路径 |
|---|---|
| `voice_router` | `~/unitree_ros2/example/build/g1_voice_router/voice_router` |
| `robot_state_provider` | 同上目录 |
| `asr_json_bridge` | `~/unitree_ros2/example/build/g1_asr_bridge/asr_json_bridge` |
| `g1_tts_node` | `~/unitree_ros2/example/build/unitree_ros2_example/g1_tts_node` |
| `g1_action_executor_node` | `~/lidar_ws/build/g1_action_executor/g1_action_executor_node` |

## 5. 编译排错

| 现象 | 原因 | 处理 |
|---|---|---|
| 找不到 `nlohmann/json.hpp` | 缺少头文件依赖 | 把 `workspace/local_include/` 指给编译器，或安装 `nlohmann-json3-dev` |
| `unitree_hg/msg/*.hpp` 找不到 | 未 source `cyclonedds_ws` | 先 source 再 build |
| 链接 `libcurl` 失败 | 未装 `libcurl4-openssl-dev` | apt 安装后重新 build |
| 改了代码但行为没变 | 没重新 build 或进程未重启 | 重新 `colcon build` 后重启对应 service |
