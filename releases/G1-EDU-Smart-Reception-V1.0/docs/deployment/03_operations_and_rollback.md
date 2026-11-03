# 运行维护、日志与回滚

## 1. 日常检查

```bash
systemctl --user is-active g1-voice-stack.target
for s in g1-robot-state-provider g1-tts g1-voice-router g1-asr-bridge g1-action-executor; do
  printf '%s pid=%s %s\n' "$s" "$(systemctl --user show -p MainPID --value $s.service)" \
    "$(systemctl --user show -p ActiveState --value $s.service)"
done
ps -eo pid,args | grep '[g]1_voice_router/voice_router'
```

## 2. 日志

| 日志 | 路径 |
|---|---|
| Router | `~/g1_logs_systemd/voice_router.log` |
| ASR | `~/g1_logs_systemd/asr_bridge.log` |
| TTS | `~/g1_logs_systemd/tts.log` |
| Action Executor | `~/g1_logs_systemd/action_executor.log` |

- 按日轮转（`logrotate` + `g1-log-rotate.timer`，每小时检查）；跨日排查需同时看 `*.log-YYYYMMDD`。
- 磁盘监测：`g1-disk-check.timer`（每 6 小时）。

## 3. 典型日志字段

```text
request_id=1 event=QUEUED queue_depth=1 text_len=24
request_id=1 event=ASR_NORMALIZED rule=ASR001_... raw_len=21 raw_digest=08deee1c normalized=脑机接口是什么
request_id=1 event=PROCESSING intent=GENERAL_QA text_len=24
request_id=1 event=ACTION_SENT action=wave id=26
request_id=1 event=ACTION_FINISHED outcome=SUCCESS action=wave
request_id=1 event=COMPLETED total_ms=5902
```

## 4. 回滚

```bash
B=~/g1_backups/g1_voice_router_20261103_064115_before_p021
systemctl --user stop g1-voice-router.service
cp "$B/voice_router_production_1381" ~/unitree_ros2/example/build/g1_voice_router/voice_router
sha256sum ~/unitree_ros2/example/build/g1_voice_router/voice_router   # 期望 e4eda4bb…
systemctl --user start g1-voice-router.service
```

回滚只影响 Router；**不得重启 target**。

## 5. 常见故障处置

| 现象 | 处置 |
|---|---|
| 无回复 | 检查五服务；检查是否出现两个 Router |
| "无法连接对话模型" | 检查笔记本 Ollama 与网络 |
| 状态全为兜底 | `/g1/robot_state` 是否存在且 `status=VALID` |
| 动作不执行 | 看 Executor 日志 FSM/Arm State；确认手臂净空 |
| 重复播报 | 立即停 Router，排查双实例/双 TTS 订阅 |
| 停止慢 | 确认 `TimeoutStopSec=15`；正常应在 1s 内退出 |
