# 服务与日志管理

## 1. 服务清单

| 服务 | 作用 | 启动脚本 |
|---|---|---|
| `g1-asr-bridge.service` | ASR JSON → 文本 | `start_asr_bridge.sh` |
| `g1-robot-state-provider.service` | 状态聚合 | `start_robot_state_provider.sh` |
| `g1-voice-router.service` | 意图/风控/调度/回复 | `start_voice_router.sh` |
| `g1-tts.service` | 文本播报 | `start_tts.sh` |
| `g1-action-executor.service` | 白名单动作执行 | `start_action_executor.sh` |
| `g1-log-rotate.timer` | 每小时检查日志轮转 | — |
| `g1-disk-check.timer` | 每 6 小时磁盘检查 | `check_disk.sh` |

由 `g1-voice-stack.target` 组合（`Wants` + `After`）。

## 2. 关键 unit 属性

```ini
Restart=always
RestartSec=3
KillSignal=SIGINT
TimeoutStopSec=15
StandardOutput=append:/home/unitree/g1_logs_systemd/<name>.log
```

实测停机：空闲 0.29s、HTTP 请求中 0.79s、动作等待中 0.29s，均远小于 15s。

## 3. 常用操作

```bash
# 只更新 Router（生产变更唯一允许的单节点操作）
systemctl --user restart g1-voice-router.service

# 查看最近日志
journalctl --user -u g1-voice-router.service -n 100 --no-pager

# 查看重启次数（异常恢复是否发生）
systemctl --user show -p NRestarts --value g1-voice-router.service

# 定时器
systemctl --user list-timers --all | grep g1-
```

## 4. 日志

| 日志 | 路径 |
|---|---|
| Router | `~/g1_logs_systemd/voice_router.log` |
| ASR | `~/g1_logs_systemd/asr_bridge.log` |
| TTS | `~/g1_logs_systemd/tts.log` |
| Executor | `~/g1_logs_systemd/action_executor.log` |

轮转策略（`deployment/logrotate/g1-voice-stack.conf`）：`daily` + `maxsize 20M` + `rotate 7` + `maxage 14` + `compress` + `delaycompress` + `copytruncate` + `dateext`。

跨日排查需同时查看 `*.log-YYYYMMDD`。

## 5. 日志字段速查

```text
request_id=1 event=QUEUED queue_depth=1 text_len=24
request_id=1 event=ASR_NORMALIZED rule=ASR001_... raw_len=21 raw_digest=08deee1c normalized=脑机接口是什么
request_id=1 event=PROCESSING intent=GENERAL_QA text_len=24
request_id=1 event=WAITING_OLLAMA
request_id=1 event=ACTION_SENT action=wave id=26
request_id=1 event=ACTION_FINISHED outcome=SUCCESS action=wave
request_id=1 event=COMPLETED total_ms=5902
```

日志不打印完整用户语音，只保留长度、意图、归一化结果与摘要。

## 6. 磁盘监测

`g1-disk-check.service` 调用 `deployment/monitoring/check_disk.sh`，由 timer 每 6 小时触发；用于长期运行场景下的空间预警。

## 7. 运维红线

- 生产更新只允许重启**单个** `g1-voice-router.service`。
- 禁止 `systemctl --user restart g1-voice-stack.target`（会同时打断 ASR/TTS/Executor）。
- 禁止改 `Restart` 策略来"避免重启"——重启是保护机制。
- 日志文件不要手工删除，交给 logrotate。
