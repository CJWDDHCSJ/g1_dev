# P0-18.4 生产日志轮转与磁盘容量管理 · 交付报告（最终版）

- 日期：2026-11-02
- 结论：**用户级 logrotate（3.19.0）+ systemd hourly timer 已启用并实测通过：独立测试日志完成阈值轮转/截断/延迟压缩/保留清理；生产六个日志完成一次受控轮转，五个 ROS 节点 PID 全程不变、新日志继续写入；磁盘监测 timer 同步运行。**
- 证据：G1 `~/g1_logs_p0184/`、本地 `evidence_p0184/`

## A. 原始日志与磁盘审计

### A.1 磁盘

| 项目 | 实测 |
|---|---|
| 根分区 | `/dev/nvme0n1p1`，1.9T，已用 22G，可用 1.8T，使用率 **2%** |
| inode | 1%（293084 / 124936192） |
| 块设备 | nvme0n1 1.9T（另有 4×1.9G zram swap） |

### A.2 五个生产日志（轮转前实测）

| 文件 | 大小 | 属主/权限 |
|---|---:|---|
| `~/g1_logs_systemd/tts.log` | 41,997 B | unitree:unitree 664 |
| `~/g1_logs_systemd/voice_router.log` | 13,822 B | unitree:unitree 664 |
| `~/g1_logs_systemd/asr_bridge.log` | 13,255 B | unitree:unitree 664 |
| `~/g1_logs_systemd/action_executor.log` | 3,387 B | unitree:unitree 664 |
| `~/g1_logs_systemd/robot_state_provider.log` | 1,831 B | unitree:unitree 664 |
| `~/g1_logs_systemd/disk_check.log` | 138 B | unitree:unitree 664 |

路径由 `systemctl --user cat <unit>` 中 `StandardOutput=append:` 的实际值确认。

### A.3 logrotate

- 初次审计：**未安装**（`command not found`、`dpkg` 状态 `un`）。
- 按任务书要求未擅自安装；由用户执行 `sudo apt-get install -y logrotate` 后完成安装。
- 现版本：**logrotate 3.19.0**（`/usr/sbin/logrotate`，包 3.19.0-1ubuntu1.1 arm64），支持 `maxsize`、`copytruncate`、`dateext`、`dateformat`、`maxage`、`delaycompress`。

### A.4 现有定时任务（实施前）

`systemctl --user list-timers --all` → 0 个，无冲突。

## B. 轮转策略（已生效）

| 参数 | 值 |
|---|---|
| 触发 | `daily` + `maxsize 20M` |
| 保留 | `rotate 7`、`maxage 14` |
| 压缩 | `compress` + `delaycompress`（当前活跃文件不压缩，上一份归档压缩） |
| 活跃文件 | `copytruncate` |
| 命名 | `dateext` + `dateformat -%Y%m%d` |
| 范围 | 仅 `/home/unitree/g1_logs_systemd/*.log` |

**copytruncate 原因与边界**：五个服务以 `StandardOutput=append:` 持有同一 fd 且无 reopen 机制，rename 会导致新文件不再增长；因此使用 copytruncate（复制后截断，进程继续写同一路径）。边界：复制与截断之间存在毫秒级窗口，可能丢失该瞬间极少量日志；这是不重启节点前提下的务实方案。

## C. 文件清单与 SHA256

（`~/g1_systemd/logrotate_staged/` 中的暂存文件已按原样复制到激活位置，哈希一致。）

| 文件 | SHA256 |
|---|---|
| `~/.config/logrotate/g1-voice-stack.conf` | `6d105432e76a7ac82d1ec50eb0d799534a9ddb5784dc3d7d71fe2234ee5ec19b` |
| `~/.config/systemd/user/g1-log-rotate.service` | `1ad0eef9ec3e7aa08acf43f2ccff0aa524270aa446803b49722ae9403d22d9b4` |
| `~/.config/systemd/user/g1-log-rotate.timer` | `54f37592b2e6854507af6bd10288a722ada7f203efe11d064692833f132b8285` |
| `~/g1_systemd/monitor/check_disk.sh` | `b3a4416f2af3b9fd9e7a31568fa02d7490cd04a95d2a9b56d5e039b36bb47466` |
| `~/.config/systemd/user/g1-disk-check.service` | `75cfa75348de46edb37f2c895f51028645de0405b5c90b5854091692e50b1c43` |
| `~/.config/systemd/user/g1-disk-check.timer` | `6300f338fedb6166501db945c6840e12283c5043141a3f9af02dc5a8fab180fb` |

## D. 定时任务状态

```text
g1-log-rotate.timer   enabled  LAST=13:12:01  NEXT=14:12:01（每小时）
g1-disk-check.timer   enabled  LAST=13:07:14  NEXT=19:07:14（每 6 小时，OnBootSec=5min）
g1-log-rotate.service 手跑一次：Result=success, ExecMainStatus=0
g1-disk-check.service 运行：Result=success, ExecMainStatus=0
```

两个 timer 均为 `WantedBy=timers.target`，配合已启用的 Linger，在用户管理器启动后即可运行（与 P0-18.3 已验证的机制一致；本阶段未重启 G1，故"重启后自动运行"未再次实测）。

## E. 实际轮转测试

### E.1 独立测试日志（`~/g1_logs_systemd/test_rotation/`）

| 步骤 | 结果 |
|---|---|
| 生成 3MB 活跃日志 → 强制轮转 | ✅ 归档 `app.log-20261102-131106`（3,000,001 B），活跃文件截断为 **0 B** |
| 追加 2MB 再轮转（验证 delaycompress） | ✅ 上一份归档压缩为 `.gz`（2,944 B），新归档未压缩 |
| gzip 完整性 | ✅ `gzip -t app.log-20261102-131106.gz` 通过 |
| 连续 4 次轮转验证 `rotate 3` | ✅ 归档数量稳定为 **3**，最旧自动删除 |
| 截断后继续追加 | ✅ 新内容正常写入（`AFTER_ROTATE`） |

### E.2 生产日志受控轮转（业务空闲时执行一次）

| 项目 | 结果 |
|---|---|
| 轮转命令 | `logrotate -v -f -s ~/.local/state/logrotate/g1-voice-stack.status ~/.config/logrotate/g1-voice-stack.conf` |
| 归档 | 六个 `*.log-20261102`（含 5 个生产日志 + disk_check），内容为轮转前完整数据 |
| 活跃文件 | 全部截断为 0 B |
| **五个 ROS 节点 PID** | **1365 / 1366 / 1367 / 1368 / 1369 —— 轮转前后完全一致（未重启）** |
| 新日志继续写入 | ✅ 轮转后一次真实状态查询产生：`asr_bridge.log=92B`、`tts.log=888B`、`voice_router.log=306B` |
| 生产归档压缩 | ⏳ 按 `delaycompress` 设计，下一次轮转（次日）才会压缩今日归档；压缩路径已由独立测试目录验证 |

- 生产归档当前为未压缩 `.log-20261102`（符合 delaycompress 语义，非失败）。

## F. 磁盘监测（运行中）

| 项目 | 实测 |
|---|---|
| 根分区 | 1.9T / 已用 22G / 可用 1.8T（2%） |
| 日志目录 | 92K（轮转前） |
| 备份目录 | 20M |
| 最大单日志 | 44K（tts.log，轮转前） |
| 判定 | **OK**（<80% OK / 80–89% WARNING / ≥90% CRITICAL） |

监测只读、不清理任何文件；每 6 小时一条记录（约 120 B/条）。

## G. 安全检查

```text
五个 ROS 节点是否重启：否（PID 1365–1369 全程未变）
ROS MainPID 是否变化：否
是否修改 ROS 业务配置/启动命令/Restart 策略：否
是否修改 target 依赖：否
是否修改 DDS/RMW/CYCLONEDDS_URI：否
是否删除历史证据或备份：否（g1_logs_p016/p018/p0181/p0183、g1_backups 全部保留）
是否使用 sudo：否（安装 logrotate 由用户执行；本 Agent 未使用 sudo）
是否触发机器人动作：否
是否修改 C++ 源码：否
```

业务回归：轮转后 `你现在有多少电量` → "我的电池电量大约是百分之86。"（实时 SOC）；轮转前 `脑机接口是什么` → 正常调用 Ollama 并播报。`/g1/robot_state` 持续 VALID，`/g1/asr_text`、`/g1/tts` 1:1。

## H. 遗留事项

1. 生产归档的**压缩**要等下一次轮转触发（delaycompress 设计），本阶段未实测生产压缩（测试目录已验证）。
2. 两个 timer 的"重启后自动运行"依赖 Linger + timers.target，机制已在 P0-18.3 验证，但本阶段未重启 G1 复测。
3. copytruncate 毫秒窗口可能丢失极少量日志（架构性限制）。
4. 日志增长率仍需长期观察（当前低负载，tts.log 约 39K/小时；20M 阈值 ≈ 数百小时）。
5. 重启瞬间日志完整性无保证（append 无 fsync 语义）。
6. `test_rotation/` 目录与测试归档保留作证据，可后续手工清理（不影响生产）。

## 验收对照（任务书十四）

1 五个日志路径已审计 ✅；2 logrotate 版本/功能确认（3.19.0）✅；3 用户级配置已建立并激活 ✅；4 五个日志被正确匹配（6 个文件均被轮转）✅；5 轮转规则符合保留策略 ✅；6 压缩与归档验证 ✅（测试目录；生产待次日）；7 用户级 service+timer 正常 ✅；8 Linger 下 timer 可随用户管理器启动 ✅（机制已验证）；9 磁盘监测正常 ✅；10 五节点 MainPID 未变化 ✅（1365–1369）；11 状态话题正常 ✅；12 未触发动作 ✅；13 历史证据未删除 ✅；14 回滚备份已建 ✅；15 交付报告完成 ✅。
