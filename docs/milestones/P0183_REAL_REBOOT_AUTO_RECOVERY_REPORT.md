# P0-18.3 G1 真实重启后生产服务自动恢复验收 · 交付报告

- 日期：2026-11-02
- 结论：**G1 真实系统重启后，五个接待节点全部由 systemd 自动恢复（boot 12:53 → 服务 12:54:07 全部拉起），真实 BMS/IMU/动作数据重新接入，四项真实语音验收通过。开机自启动已获真机验证。**
- 证据：G1 `~/g1_logs_p0183/`，本地 `evidence_p0183/`

## A. 重启前配置检查（已通过）

| 项目 | 实测 |
|---|---|
| Linger | `Linger=yes`、`State=active` |
| target | `enabled` + `active`，enable 符号链接位于 `default.target.wants/` |
| 五个服务 | 全部 active (running)，NRestarts 全为 0，无 nohup 残留 |
| 重启前 MainPID | Provider 16583 / TTS 16584 / Router 16585 / ASR 16589 / Executor 16592 |
| 依赖配置 | `PartOf=g1-voice-stack.target`；`After=` Provider→TTS→Router→ASR→Executor |
| 恢复策略（effective） | `Restart=always`、`RestartUSec=3s`、`StartLimitIntervalUSec=1min`、`StartLimitBurst=5`、`KillSignal=SIGINT`、`TimeoutStopSec=15` |
| 启动脚本 | 5 个，可执行、绝对路径、`exec` 真实二进制；Executor 额外 source `lidar_ws`；**无 Mock 地址** |
| 机器人安全状态 | 动作 `id=0` 空闲；无业务队列；重启前 SOC=31%（已提示偏低风险） |

## B. 重启授权记录（如实记录）

- Agent **未执行任何重启命令**（`systemctl reboot` / `reboot` / `shutdown` 均未调用）。
- 重启前检查报告与重启命令建议已输出，并在该处停止等待授权。
- 用户以“已经重启过了”回复，即**用户本人手动执行了重启**；系统侧证据为 boot 时间 12:53（`who -b`）与 uptime 归零。

## C. 重启事实

| 项目 | 值 |
|---|---|
| boot 时间 | 2026-11-02 **12:53**（`who -b`） |
| 验收时 uptime | 2 分钟（12:56） |
| 重启前 uptime | 1 小时 42 分（12:05）→ 已归零，确认为真实重启 |

## D. 重启后自动恢复（核心验收）

| 服务 | ActiveEnterTimestamp | MainPID | NRestarts | 状态 |
|---|---|---:|---:|---|
| g1-robot-state-provider | **12:54:07** | 1365 | 0 | active (running) |
| g1-tts | **12:54:07** | 1366 | 0 | active (running) |
| g1-voice-router | **12:54:07** | 1367 | 0 | active (running) |
| g1-asr-bridge | **12:54:07** | 1368 | 0 | active (running) |
| g1-action-executor | **12:54:07** | 1369 | 0 | active (running) |

- boot 12:53 → 服务 12:54:07 自动拉起（boot 后约 1 分钟，**无任何人工干预**，Agent 在 12:56 才接入检查）。
- `Linger=yes` 重启后仍保持；`target` 仍 `enabled` 且已 `active`。
- 五个 MainPID 均为真实节点二进制（`/proc/<pid>/exe` 路径为 install 下的 `voice_router`、`robot_state_provider`、`g1_tts_node`、`asr_json_bridge`、`g1_action_executor_node`）。
- 实例唯一：精确路径匹配各 1 个，无重复、无旧进程残留。

## E. ros_bridge 兼容性与话题恢复

- `/ros_bridge` 重启后正常在位（未触碰其生命周期）。
- 原生话题连接（pub/sub）：`/lowstate` 1/6、`/lf/bmsstate` 1/7、`/arm/action/state` 1/4、`/sportmodestate` 1/3、`/audio_msg` 1/3。
- 业务话题：`/g1/robot_state` 1/1、`/g1/asr_text` 1/1、`/g1/tts` 1/1、`/g1/action_request` 1/1。
- Router 在 Provider 数据到达前的兜底逻辑未触发异常；重启后自动重新订阅并收到新数据（无需手工干预）。

## F. 真实数据恢复（重启后）

```json
{"action":{"status":"VALID","is_executing":false,"action_id":0,"received_age_ms":33},
 "battery":{"status":"VALID","soc_percent":92,"voltage_mv":51870,"received_age_ms":12},
 "imu":{"status":"VALID","data_available":true,"received_age_ms":1},
 "runtime":{"status":"VALID","lowstate_available":true,"sportmode_available":true}}
```

- **SOC=92%、51.87V**：重启前为 31%/45.48V（期间用户充电）→ 数值显著变化，**证明是重启后的全新 BMS 数据，不是旧缓存**。
- IMU age=1ms、action age=33ms → 持续真实更新。
- Action Executor 重启后 `Arm State id=0`，**未触发任何动作**。

## G. 真实语音验收（重启后，4/4 通过，用户现场确认）

| # | 用户语音 | ASR | Intent | 状态/Guard | 路径 | Ollama | 播报 |
|---|---|---|---|---|---|---|---|
| 1 | 你现在有多少电量 | 准确 | ROBOT_STATUS | state_ready=true, SOC 92→91 | 本地真实数据 | ❌ | "我的电池电量大约是百分之91。" |
| 2 | 你的 IMU 数据正常吗 | 准确 | ROBOT_STATUS | state_ready=true | 本地，仅述接收 | ❌ | 93B，"持续接收…未完成健康诊断" |
| 3 | 你现在运行状态怎么样 | 准确 | ROBOT_STATUS | state_ready=true | 本地汇总 | ❌ | 4 句：核心数据接收 / 电量约 91% / IMU 接收正常 / 未执行动作 |
| 4 | 脑机接口是什么 | "机接口是什么"（截断） | GENERAL_QA | NORMAL（knowledge form） | Ollama 162B | ✅ | 正常知识回答 |

- 所有句子 `TtsMaker returned ret=0`，真实扬声器播报。
- 第 4 句 ASR 截断属原生 ASR 行为，路由对实际识别文本处理正确（知识型放行）。
- 重启后另有若干 ASR 噪声输入（"被告在""嗯"等）走 GENERAL_QA→Ollama，无害。

## H. 安全情况

```text
Agent 是否执行 G1 重启：否（用户手动执行）
是否触发机器人动作：否
是否修改 DDS/网络：否
是否修改 ros_bridge 生命周期：否
是否修改 C++ 业务代码：否
是否出现重复节点：否（五节点各 1 实例）
是否出现异常重启循环：否（NRestarts 全 0，未触及 StartLimit）
是否覆盖历史备份/日志：否
```

## I. 证据清单

G1 `~/g1_logs_p0183/` 与本地 `evidence_p0183/`：

```text
pre_reboot_uptime.txt / pre_reboot_systemd_status.txt / pre_reboot_process_snapshot.txt
pre_reboot_robot_state.json / pre_reboot_ros_nodes.txt / pre_reboot_topics.txt
pre_reboot_unit_hashes.txt
post_reboot_uptime.txt / post_reboot_linger_status.txt / post_reboot_target_status.txt
post_reboot_service_status.txt / post_reboot_process_snapshot.txt
post_reboot_ros_nodes.txt / post_reboot_robot_state.json / post_reboot_voice_test.md
robot_state_provider.log / tts.log / voice_router.log / asr_bridge.log / action_executor.log
```

## J. 未验证项 / 说明

1. `journalctl --user -b` 在本机未返回有效条目（用户级 journal 未持久化且本次启动无记录），因此**自启动证据以 `ActiveEnterTimestamp`、boot 时间、`/proc` 启动时间与文件日志追加为准**，已足够证明由开机触发。
2. 本次为**系统重启**验证；断电冷启动（掉电重启）未单独验证（行为路径相同，但未实测）。
3. 日志文件为 append 模式，未配置轮转（长期运行需加 logrotate）。
4. 电池：重启前 31% 偏低（已预警）；重启后 92%，说明用户已充电。此前观测待机耗电约 12%/h 甚至更快，演示前务必充满。
5. 业务遗留项同前：温度/电流单位、关节索引映射、IMU 健康诊断、Odom 接入。

## 验收对照（任务书十五）

1-3（Linger 持久、target enabled、重启后自动启动）✅；4-5（五服务自动启动、MainPID 为真实节点）✅；6（唯一实例）✅；7-8（ros_bridge 正常、DDS 话题重连）✅；9-10（Provider 新数据、SOC 来自重启后 BMS：92%）✅；11（Router 收到新状态）✅；12（ASR/TTS 恢复）✅；13（Executor 在线空闲）✅；14（真实语音验收 4/4）✅；15（未触发动作）✅；16（回滚与日志完整）✅。
