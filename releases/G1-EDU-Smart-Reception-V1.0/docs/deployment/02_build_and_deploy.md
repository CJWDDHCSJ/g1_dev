# 编译与部署

## 1. 源码放置

```bash
cp -r ros2_ws/src/g1_voice_router   ~/unitree_ros2/example/
cp -r ros2_ws/src/g1_asr_bridge     ~/unitree_ros2/example/
cp -r action_ws/src/g1_action_executor ~/lidar_ws/src/
cp integrations/unitree_audio/g1_tts_node.cpp ~/unitree_ros2/example/src/src/g1/audio_client/
cp integrations/unitree_audio/config/*.json    ~/unitree_ros2/example/config/
cp deployment/scripts/*.sh ~/g1_systemd/scripts/ && chmod +x ~/g1_systemd/scripts/*.sh
cp deployment/systemd/g1-* ~/.config/systemd/user/
mkdir -p ~/.config/logrotate && cp deployment/logrotate/g1-voice-stack.conf ~/.config/logrotate/
```

## 2. 编译（两个工作区分别构建）

```bash
source /opt/ros/humble/setup.bash && source ~/unitree_ros2/setup.sh

cd ~/unitree_ros2/example
colcon build --packages-select g1_voice_router g1_asr_bridge --symlink-install

cd ~/lidar_ws
colcon build --packages-select g1_action_executor --symlink-install
```

## 3. 测试

```bash
cd ~/unitree_ros2/example
colcon test --packages-select g1_voice_router
colcon test-result --verbose
# 期望：Summary: 7 tests, 0 errors, 0 failures, 0 skipped
# 断言总数 453
```

## 3.1 启用定时器（日志轮转、磁盘检查）

```bash
systemctl --user daemon-reload
systemctl --user enable --now g1-log-rotate.timer g1-disk-check.timer
systemctl --user list-timers --all | grep g1-
```

> `timers.target.wants/` 中的符号链接是部署机本地状态，不入版本库；请用 `enable` 在目标机生成。

## 4. 部署校验

```bash
systemctl --user daemon-reload
systemctl --user start g1-voice-stack.target
sleep 45

systemctl --user is-active g1-voice-stack.target
ps -eo pid,args | grep '[g]1_voice_router/voice_router'   # 必须只有 1 个

ros2 topic info /g1/action_request    # Pub 1 / Sub 1
ros2 topic echo /g1/robot_state --once | head -3
```

## 5. 更新生产（只重启单节点）

```bash
B=~/g1_backups/g1_voice_router_$(date +%Y%m%d_%H%M%S)_before_update
mkdir -p "$B" && cp -a ~/unitree_ros2/example/g1_voice_router "$B/"
PID=$(systemctl --user show -p MainPID --value g1-voice-router.service)
cp "/proc/$PID/exe" "$B/voice_router_old"

colcon build --packages-select g1_voice_router --symlink-install
systemctl --user restart g1-voice-router.service     # 仅此一个
sha256sum "/proc/$(systemctl --user show -p MainPID --value g1-voice-router.service)/exe"
```
