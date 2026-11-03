# 启动指南

> 命令来自 `deployment/systemd/*.service` 的 `ExecStart` 与 P0-18.2/P0-18.3 的验收记录。

## 1. 首次部署

```bash
mkdir -p ~/g1_systemd/scripts ~/g1_systemd/monitor ~/.config/systemd/user
cp deployment/scripts/*.sh          ~/g1_systemd/scripts/
cp deployment/monitoring/check_disk.sh ~/g1_systemd/monitor/
chmod +x ~/g1_systemd/scripts/*.sh ~/g1_systemd/monitor/check_disk.sh

cp deployment/systemd/g1-*          ~/.config/systemd/user/
mkdir -p ~/.config/logrotate ~/.local/state/logrotate ~/g1_logs_systemd
cp deployment/logrotate/g1-voice-stack.conf ~/.config/logrotate/

systemctl --user daemon-reload
systemd-analyze --user verify ~/.config/systemd/user/g1-voice-stack.target
```

## 2. 启动整套

```bash
systemctl --user enable --now g1-voice-stack.target
systemctl --user is-active g1-voice-stack.target        # 期望 active
```

`target` 会拉起五个服务：`g1-robot-state-provider`、`g1-tts`、`g1-voice-router`、`g1-asr-bridge`、`g1-action-executor`。

> 启动过程**不会触发任何动作**：动作只由 `/g1/action_request` 中的白名单命令触发。

## 3. 启动后自检

```bash
# 五个服务都要 running
for s in g1-robot-state-provider g1-tts g1-voice-router g1-asr-bridge g1-action-executor; do
  printf '%-26s pid=%s state=%s\n' "$s" \
    "$(systemctl --user show -p MainPID --value $s.service)" \
    "$(systemctl --user show -p ActiveState --value $s.service)"
done

# 只能有一个 Router
ps -eo pid,args | grep '[g]1_voice_router/voice_router'

# 话题连接数
ros2 topic info /g1/asr_text        # 1 pub / 1 sub
ros2 topic info /g1/tts             # 1 pub / 1 sub
ros2 topic info /g1/action_request  # 发布者只有 g1_voice_router
ros2 topic hz /g1/robot_state       # 约 2 Hz
```

日志应出现：

```text
[g1_robot_state_provider]: RobotStateProvider started: subscribing ...
[g1_voice_router]: KnowledgeBase loaded ... (FAQ=21, directions=3, projects=1, equipment=1)
[g1_voice_router]: Ollama URL: http://<LLM_HOST>:11434/api/chat
[g1_tts_node]: G1 TTS node initialized. Listening on /g1/tts
```

## 4. 手工调试启动（排障用）

```bash
systemctl --user stop g1-voice-stack.target   # 必须先停整套，避免同域两个 Router

source /opt/ros/humble/setup.bash
source ~/unitree_ros2/setup.sh
source ~/unitree_ros2/cyclonedds_ws/install/setup.bash
source ~/unitree_ros2/example/install/setup.bash
source ~/lidar_ws/install/setup.bash
export ROS_DOMAIN_ID=77          # 调试建议用 77，避免影响生产
export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp

ros2 run g1_voice_router robot_state_provider
ros2 run unitree_ros2_example g1_tts_node
ros2 run g1_voice_router voice_router --ros-args \
  -p ollama_url:=http://<LLM_HOST>:11434/api/chat \
  -p request_timeout_seconds:=60 \
  -p action_timeout_seconds:=45
ros2 run g1_asr_bridge asr_json_bridge
ros2 run g1_action_executor g1_action_executor_node
```

手工注入一条文本（不依赖麦克风）：

```bash
ros2 topic pub --once /g1/asr_text std_msgs/msg/String "{data: '脑机接口是什么'}"
ros2 topic echo /g1/tts --once
```

## 5. 主要 ROS 参数

| 参数 | 默认值 | 合法范围 | 说明 |
|---|---|---|---|
| `ollama_url` | 源码内网默认值（与冻结二进制一致） | — | 生产必须显式指定 |
| `model` | `qwen3-vl:4b-instruct` | — | 模型名 |
| `request_timeout_seconds` | 60 | 5–300 | 越界会被重置为 60 并告警 |
| `action_timeout_seconds` | 45 | — | 应大于执行器最坏 38 s |
| `knowledge_file` | 安装目录下的 `lab_knowledge.json` | — | 可用外部路径覆盖 |
| `system_prompt` | 内置中文提示词 | — | 禁止编造价格/人数/成果/型号/实时状态 |

## 6. 停止

```bash
systemctl --user stop g1-voice-stack.target     # 停整套
systemctl --user stop g1-voice-router.service   # 只停业务（其他节点保留）
```

停机行为：Router 收到 SIGINT → 置停机标志 → 中断进行中的 HTTP → 唤醒动作等待 → 清队列 → join worker；实测 0.29–0.79 s 完成。

## 7. 开机自启

```bash
systemctl --user enable g1-voice-stack.target
loginctl enable-linger "$USER"     # 关键：用户级服务在无人登录时也要能启动
```

P0-18.3 真机验收：整机重启后 boot 记录 12:53，五个服务 `ActiveEnterTimestamp` 均为 12:54:07，`NRestarts=0`，无人工干预。

## 8. 注意事项

- **不要**同时启动两份 Voice Router：会造成重复回答与双倍动作请求；
- **不要**在演示中改 `ROS_DOMAIN_ID`：生产 0，测试 77；
- 手工启动前必须停 systemd 整套，否则会出现两套同名节点；
- 改完源码后只重启受影响的**单个**服务；
- 真机动作需要现场安全确认，建议演示前先跑一条非动作问答确认链路正常。
