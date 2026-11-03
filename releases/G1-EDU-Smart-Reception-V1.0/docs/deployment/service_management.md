# 服务管理与日志运维

> 依据：`deployment/systemd/*`、`deployment/logrotate/*`、`deployment/monitoring/check_disk.sh`，以及 P0-18.2（服务化）、P0-18.3（重启恢复）、P0-18.4（日志轮转）验收报告。

## 1. 单元清单

| 单元 | 类型 | 作用 |
|---|---|---|
| `g1-voice-stack.target` | target | 聚合五个业务服务，`WantedBy=default.target` |
| `g1-robot-state-provider.service` | simple | 状态聚合 |
| `g1-tts.service` | simple | 语音合成 |
| `g1-voice-router.service` | simple | 业务主节点 |
| `g1-asr-bridge.service` | simple | ASR 桥 |
| `g1-action-executor.service` | simple | 动作执行 |
| `g1-log-rotate.service` | oneshot | 调用 `/usr/sbin/logrotate` 执行一次轮转 |
| `g1-log-rotate.timer` | timer | 每小时触发一次轮转检查 |
| `g1-disk-check.service` | oneshot | 运行 `check_disk.sh` |
| `g1-disk-check.timer` | timer | 每 6 小时检查磁盘 |

## 2. 常用操作

```bash
# 整体
systemctl --user start|stop|restart g1-voice-stack.target
systemctl --user is-active g1-voice-stack.target
systemctl --user list-units 'g1-*'

# 单节点
systemctl --user restart g1-voice-router.service
systemctl --user status g1-tts.service

# 定时任务
systemctl --user list-timers | grep g1-
systemctl --user start g1-log-rotate.service      # 手动轮转一次
systemctl --user list-dependencies g1-voice-stack.target
```

## 3. 关键配置与含义

| 配置 | 值 | 为什么 |
|---|---|---|
| `PartOf=g1-voice-stack.target` | 五个业务服务 | `stop target` 一次停全部，避免残留 |
| `Restart=always` | 五个业务服务 | 崩溃自愈 |
| `RestartSec=3` | 3 s | 给端口/话题/DDS 端点一点时间释放 |
| `StartLimitIntervalSec=60` / `StartLimitBurst=5` | 60 s / 5 次 | 崩溃循环时停机等人，避免打满 CPU |
| `KillSignal=SIGINT` | SIGINT | 触发 Router 的优雅停机路径 |
| `TimeoutStopSec=15` | 15 s | 远大于实测停机 0.29–0.79 s，留足余量 |
| `StandardOutput=append:~/g1_logs_systemd/<node>.log` | 五个日志 | 与 logrotate 的 `copytruncate` 配套 |
| `After=` 顺序 | Provider → TTS/Router → ASR → Executor | 保证依赖先起，但不产生动作 |

## 4. 状态检查与故障判定

```bash
systemctl --user show -p NRestarts --value g1-voice-router.service   # 期望 0 或很小
systemctl --user show -p ActiveState,SubState,MainPID -p --value g1-voice-router.service
journalctl --user -u g1-voice-router.service -n 100 --no-pager
```

| 现象 | 判定 | 处置 |
|---|---|---|
| `NRestarts` 持续增长 | 崩溃循环 | 看 journal 首个错误，修根因；必要时 `systemctl --user stop` 止损 |
| `Result=start-limit-hit` | 触及 5 次/60 s 限制 | 修好后 `systemctl --user reset-failed <unit>` 再启动 |
| `MainPID=0` 但 `ActiveState=active` | oneshot 已结束（正常） | 仅适用于 log-rotate / disk-check |
| 服务 active 但无数据 | 环境/DDS 问题 | 查 `ROS_DOMAIN_ID`、`CYCLONEDDS_URI` |

## 5. 日志体系

| 项 | 值 |
|---|---|
| 目录 | `~/g1_logs_systemd/` |
| 文件 | `voice_router.log`、`tts.log`、`asr_bridge.log`、`action_executor.log`、`robot_state_provider.log`、`disk_check.log` |
| 轮转触发 | `daily` 或 `maxsize 20M`（先到先触发） |
| 保留 | `rotate 7`、`maxage 14` |
| 压缩 | `compress` + `delaycompress`（活跃文件不压缩） |
| 活跃文件处理 | `copytruncate`（复制后截断） |
| 命名 | `dateext`、`dateformat -%Y%m%d` |
| 频率 | 每小时 timer 检查一次 |

### 5.1 为什么必须用 `copytruncate`

五个服务用 `StandardOutput=append:` 持有同一路径的 fd，且没有 reopen 机制。若用默认的 rename 轮转，进程会继续写已改名的旧 inode，新文件不再增长——看起来“日志停了”。`copytruncate` 复制后截断同一文件，进程无感。

代价：复制与截断之间有毫秒级窗口，可能丢失该瞬间的极少量日志。这是“不重启节点”前提下的务实取舍。

### 5.2 手动验证轮转

```bash
systemctl --user start g1-log-rotate.service
systemctl --user show -p Result -p ExecMainStatus --value g1-log-rotate.service
ls -lh ~/g1_logs_systemd/
```

预期：出现 `voice_router.log-YYYYMMDD`（或 `.gz`），活跃日志体积回落到接近 0 并继续增长。

## 6. 磁盘监测

`check_disk.sh` 只读、不删任何文件，每次输出一行并追加到 `disk_check.log`：

```text
2026-11-02T13:07:14Z state=OK root_used=2% used=22G avail=1.8T logs=74K backups=... largest_log=42K /home/unitree/g1_logs_systemd/tts.log
```

阈值：`<80%` → `OK`；`≥80%` → `WARNING`；`≥90%` → `CRITICAL`。脚本不自动清理，处置由人决定（避免自动化误删）。

## 7. 已知边界

- 日志写入量取决于演示强度，`20M` 阈值是按“一天几百次对话”估的，长时间高负载需重新评估；
- 轮转只覆盖 `~/g1_logs_systemd/*.log`，不动其他目录；
- `journalctl` 里也有 systemd 自身记录（不含节点 stdout，因为已重定向到文件）；
- 停 target 不会停 `g1-log-rotate.timer` / `g1-disk-check.timer`（它们不在 target 的 `Wants` 内），这是有意的：运维任务与业务解耦。
