# V1.0 安装、启动与回滚手册

## 1. 环境前提

```bash
# G1 端（ARM64）
source ~/unitree_ros2/setup.sh
source ~/unitree_ros2/example/install/setup.bash
# Action Executor 所在工作区
source ~/lidar_ws/install/setup.bash
```

- ROS 2 Humble，`RMW_IMPLEMENTATION=rmw_cyclonedds_cpp`，生产 `ROS_DOMAIN_ID=0`
- 隔离测试使用 `ROS_DOMAIN_ID=77`
- 外部模型：`http://192.168.123.99:11434/api/chat`（`qwen3-vl:4b-instruct`）

## 2. 编译

```bash
cd ~/unitree_ros2/example
colcon build --packages-select g1_voice_router --symlink-install
colcon test  --packages-select g1_voice_router
colcon test-result --verbose      # 期望：7 tests, 0 errors, 0 failures
```

## 3. 启动（systemd，推荐）

```bash
systemctl --user start g1-voice-stack.target
systemctl --user status g1-voice-stack.target
for s in g1-robot-state-provider g1-tts g1-voice-router g1-asr-bridge g1-action-executor; do
  printf '%s pid=%s %s\n' "$s" \
    "$(systemctl --user show -p MainPID --value $s.service)" \
    "$(systemctl --user show -p ActiveState --value $s.service)"
done
```

单节点操作（只允许对需要更新的服务执行）：

```bash
systemctl --user restart g1-voice-router.service
```

## 4. 启动（手工，仅排障用）

终端 1：`ros2 run g1_asr_bridge asr_json_bridge`
终端 2：`ros2 run g1_voice_router voice_router --ros-args -p ollama_url:=http://192.168.123.99:11434/api/chat`
终端 3：`ros2 run unitree_ros2_example g1_tts_node`
终端 4：`ros2 run g1_voice_router robot_state_provider`
终端 5：`ros2 run g1_action_executor g1_action_executor_node`

> 手工启动前先 `systemctl --user stop g1-voice-stack.target`，避免同一 Domain 出现双 Router（会导致重复动作请求）。

## 5. 运行检查

```bash
rs2 topic info /g1/asr_text      # Pub 1 / Sub 1
ros2 topic info /g1/tts          # Pub 1 / Sub 1
ros2 topic info /g1/action_request   # 只有 g1_voice_router 发布
ros2 topic info /g1/action_result    # 只有 g1_action_executor 发布
ros2 topic echo /g1/robot_state --once   # schema_version=1 的聚合状态
```

## 6. 日志

| 日志 | 路径 |
|---|---|
| Router | `~/g1_logs_systemd/voice_router.log`（按日轮转） |
| ASR | `~/g1_logs_systemd/asr_bridge.log` |
| TTS | `~/g1_logs_systemd/tts.log` |
| Action Executor | `~/g1_logs_systemd/action_executor.log` |

跨日排查需同时查看 `*.log-YYYYMMDD` 文件。

## 7. 回滚

```bash
B=~/g1_backups/g1_voice_router_20261103_064115_before_p021
systemctl --user stop g1-voice-router.service
cp "$B/voice_router_production_1381" ~/unitree_ros2/example/build/g1_voice_router/voice_router
sha256sum ~/unitree_ros2/example/build/g1_voice_router/voice_router   # e4eda4bb…
systemctl --user start g1-voice-router.service
```

回滚只影响 Router，不得重启 `g1-voice-stack.target`。

## 8. 常见故障

| 现象 | 排查 |
|---|---|
| 无回复 | `ps` 是否双 Router；`ros2 topic info /g1/asr_text` 订阅数 |
| 回答"暂时无法连接对话模型" | `curl -s http://192.168.123.99:11434/api/tags` |
| 状态回答为兜底 | `ros2 topic echo /g1/robot_state --once` 的 `status` 是否为 VALID |
| 动作不执行 | Executor 日志 `FSM`/`Arm State`；`/g1/action_result` 内容 |
| 播报重复 | 是否两个 TTS 订阅者或两个 Router |
| 停止慢 | `systemctl --user show -p TimeoutStopSec --value g1-voice-router.service` 应为 15s；实测 SIGINT 退出 < 1s |
