# 启动指南

## 1. 推荐方式：systemd 整套启动

```bash
# 首次部署：安装 unit 与脚本
cp deployment/scripts/*.sh ~/g1_systemd/scripts/ && chmod +x ~/g1_systemd/scripts/*.sh
mkdir -p ~/.config/systemd/user
cp deployment/systemd/g1-* ~/.config/systemd/user/

systemctl --user daemon-reload
systemctl --user enable --now g1-voice-stack.target
```

## 2. 状态检查

```bash
systemctl --user is-active g1-voice-stack.target

for s in g1-robot-state-provider g1-tts g1-voice-router g1-asr-bridge g1-action-executor; do
  printf '%s pid=%s %s\n' "$s" \
    "$(systemctl --user show -p MainPID --value $s.service)" \
    "$(systemctl --user show -p ActiveState --value $s.service)"
done

# 必须只有 1 个 Router
ps -eo pid,args | grep '[g]1_voice_router/voice_router'
```

正常情况：5 个 PID 互不相同、均 active；Router 只有一行。

## 3. 手工调试启动（排障用）

```bash
# 先停整套，避免同域出现两个 Router
systemctl --user stop g1-voice-stack.target

source /opt/ros/humble/setup.bash
source ~/unitree_ros2/setup.sh
source ~/unitree_ros2/example/install/setup.bash
source ~/lidar_ws/install/setup.bash

ros2 run g1_asr_bridge asr_json_bridge
ros2 run g1_voice_router voice_router --ros-args -p ollama_url:=http://<LLM_HOST>:11434/api/chat
ros2 run unitree_ros2_example g1_tts_node
ros2 run g1_voice_router robot_state_provider
ros2 run g1_action_executor g1_action_executor_node
```

## 4. 启动后的自检

```bash
ros2 topic info /g1/asr_text       # 1 pub / 1 sub
ros2 topic info /g1/tts            # 1 pub / 1 sub
ros2 topic info /g1/action_request # 发布者只有 g1_voice_router
ros2 topic echo /g1/robot_state --once | head -3
```

## 5. 停止

```bash
systemctl --user stop g1-voice-stack.target
# 或只停业务节点
systemctl --user stop g1-voice-router.service
```

## 6. 开机自启与 linger

```bash
systemctl --user enable g1-voice-stack.target
loginctl enable-linger "$USER"     # 未登录也能启动用户服务（需一次 sudo/管理员操作）
```

实测：整机重启后约 45s 五节点全部恢复（PID 全新，`NRestarts=0`）。

## 7. 注意事项

- **不要**同时启动两份 Voice Router：同域双订阅会导致一句话被处理两次、动作请求翻倍。
- **不要**随意改 `ROS_DOMAIN_ID`：生产为 0，测试用 77。
- 手工启动前务必先停 systemd 整套，避免端口/话题冲突。
- 启动动作执行器本身不会触发动作；动作只在收到白名单请求时执行。
