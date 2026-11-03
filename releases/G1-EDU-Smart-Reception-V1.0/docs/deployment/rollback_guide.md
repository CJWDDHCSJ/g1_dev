# 回滚指南

## 1. 回滚前必须确认

1. 已定位问题（不是网络/外部模型故障）；
2. 旧二进制备份存在且哈希正确；
3. 当前没有正在执行的动作；
4. 现场安全（手臂净空、有人可停机）。

## 2. 备份规范

每次更新前：

```bash
B=~/g1_backups/g1_voice_router_$(date +%Y%m%d_%H%M%S)_before_update
mkdir -p "$B"
cp -a ~/unitree_ros2/example/g1_voice_router "$B/"
PID=$(systemctl --user show -p MainPID --value g1-voice-router.service)
cp "/proc/$PID/exe" "$B/voice_router_old"     # 保存"正在运行的映像"，不是磁盘文件
sha256sum "$B/voice_router_old" | tee "$B/sha256_old.txt"
```

> 磁盘上的 `build/` 二进制可能已被新编译覆盖；必须从 `/proc/PID/exe` 取运行映像。

## 3. 回滚步骤（只影响 Router）

```bash
B=~/g1_backups/g1_voice_router_20261103_064115_before_p021

systemctl --user stop g1-voice-router.service
ps -p "$(systemctl --user show -p MainPID --value g1-voice-router.service)" || true

cp "$B/voice_router_production_1381" ~/unitree_ros2/example/build/g1_voice_router/voice_router
sha256sum ~/unitree_ros2/example/build/g1_voice_router/voice_router
# 期望：e4eda4bbd83feb57e6695f1a3e5a4157e70c1e9a9431e387d18c66f1f4c4ac57（P0-20 版本）

systemctl --user start g1-voice-router.service
```

## 4. 回滚后验证

```bash
P=$(systemctl --user show -p MainPID --value g1-voice-router.service)
readlink -f /proc/$P/exe
sha256sum /proc/$P/exe                       # 应与备份一致

ps -eo pid,args | grep '[g]1_voice_router/voice_router'   # 仅 1 个
ros2 topic info /g1/action_result            # 订阅数 1
ros2 topic echo /g1/robot_state --once | head -3
```

## 5. 已知可回滚基线

| 版本 | Router SHA256 | 说明 |
|---|---|---|
| V1.0（当前） | `6ab99de8…` | P0-21 冻结版（含 ASR 归一化、IMU 路由、动作闭环） |
| P0-20 | `e4eda4bb…` | 无 ASR001 别名、无 IMU 状态路由 |
| P0-19 时期 | `67681e51…` | 动作闭环之前 |

## 6. 禁止事项

- 不得为回滚重启 `g1-voice-stack.target`；
- 不得覆盖历史备份目录；
- 回滚后不要立即再次尝试新版本，先定位失败原因；
- 回滚不涉及 ASR/TTS/Executor/DDS/Ollama 的任何改动。
