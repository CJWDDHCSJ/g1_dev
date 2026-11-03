# P0-18.1 RobotStateProvider 生产部署与真实状态感知验收 · 交付报告

- 日期：2026-11-02
- 结论：**RobotStateProvider 已接入 G1 生产域（DOMAIN_ID 0）并持续输出真实状态；Voice Router 依据真实 BMS/IMU/动作数据回答；七项真实麦克风测试全部通过。**
- 证据目录：`/home/mick/Mick_Grey/g1_dev/teach/g1_kb_work/evidence_p0181/`

## A. 部署前状态

- G1：Ubuntu 22.04.5 / aarch64 / ROS humble / rmw_cyclonedds_cpp / eth0 绑定 / Domain 0 / ROS_LOCALHOST_ONLY=0；部署时 uptime 1:09（当日重启后未再重启）。
- 接待节点：**全部不存在**（重启后未启动），无残留测试进程，无 Domain 77 残留。
- Unitree 原生数据源：`/lf/bmsstate`、`/lowstate`、`/sportmodestate`、`/arm/action/state` 全部 `DATA_OK`；`/audio_msg` pub=1/sub=2（原生 ASR 活跃）；`/ros_bridge` 正常。
- 部署前实测 SOC=50（机器人已充电），动作状态 `{"holding":false,"id":0,"name":""}`。
- Ollama：`http://192.168.123.99:11434` 可达，`qwen3-vl:4b-instruct` 在位。

## B. 部署版本

| 文件 | SHA256（与 P0-18 报告一致） |
|---|---|
| src/robot_state_evaluator.cpp | `799d43e4c01220b74feb0a5b031c0aac20f6167a777061f12d35c4fc1daf69d5` |
| src/voice_router.cpp | `fc13bdd605fc0006576d79e46eaa0c3e835c1df160f12fd96ccd52d09f82a2b1` |
| CMakeLists.txt | `ca73b2aabad43f1bcc40e0e81b3a360ae955594112f158daec0802522600e99f` |
| package.xml | `27819e7d4230cafadaae6ac89679d71277a953e4532519d8706c2c7c80cd8ebe` |
| src/robot_state_provider.cpp | `4e8e9e28dd7b676bf4699417d4d4720d5b7fc03b4c9e67212f569365cdc0a4cc` |

- 重新 `colcon build --symlink-install`（0.45s 增量）+ `colcon test`：**4 tests, 0 errors, 0 failures, 0 skipped**；直跑 RS 44/44、Intent 75/75、Guard 119/119、KB 57/57。
- 可执行文件：`install/g1_voice_router/lib/g1_voice_router/{voice_router,robot_state_provider}`（symlink → build）。

## C. 生产节点状态

| 节点 | PID | 启动命令 | 日志 | 状态 |
|---|---:|---|---|---|
| RobotStateProvider | **11528** | `ros2 run g1_voice_router robot_state_provider`（nohup） | `~/g1_logs_p0181/robot_state_provider.log` | 运行，2Hz 发布 |
| TTS Node | **11653** | `ros2 run unitree_ros2_example g1_tts_node` | `tts.log` | 运行，配置 speaker_id=0/vol=80/pause=1500ms（未改） |
| Voice Router | **11673** | `ros2 run g1_voice_router voice_router`（无 Mock 参数） | `voice_router.log` | 运行，KB=21，Ollama URL 正确 |
| ASR Bridge | **11696** | `ros2 run g1_asr_bridge asr_json_bridge` | `asr_bridge.log` | 运行 |
| Action Executor | **11882** | `ros2 run g1_action_executor g1_action_executor_node`（需 source `~/lidar_ws/install/setup.bash`） | `action_executor.log` | 运行，白名单 6 动作，Arm State id=0 空闲 |
| Unitree ros_bridge | 原生 bare-DDS（非 Linux 进程） | — | — | `/ros_bridge` 节点在位 |

- 首启 Executor 失败原因：启动 shell 只 source 了 example 工作区，`Package 'g1_action_executor' not found`；补 source `~/lidar_ws/install/setup.bash` 后正常。**这是本阶段唯一故障，已解决，未改任何代码。**

## D. 真实状态数据（Domain 0，实测）

```json
{"schema_version":1,
 "battery":{"status":"VALID","soc_percent":49,"soh_percent":94,"voltage_mv":47353,"received_age_ms":24},
 "imu":{"status":"VALID","data_available":true,"received_age_ms":0},
 "action":{"status":"VALID","is_executing":false,"action_id":0,"received_age_ms":77},
 "runtime":{"status":"VALID","lowstate_available":true,"sportmode_available":true},
 "joints":{"status":"VALID","available_motor_count":29,"temperature_max_raw":45,"unit_confirmed":false}}
```

- SOC 连续观测 50 → 49 → 48，真实变化，非固定值。
- 全部字段 VALID，无 STALE / INVALID / SOURCE_MISSING。
- 话题接线：`/g1/robot_state` pub=1(Provider)/sub=1(Router)；`/g1/asr_text`、`/g1/tts`、`/g1/action_request` 均 1:1；`/g1/action_result` pub=1。

## E. 真实麦克风验收（7/7 通过，用户现场确认）

| # | 用户语音 | ASR 实际 | Intent | 状态/Guard | 最终路径 | Ollama | 播报 |
|---|---|---|---|---|---|---|---|
| 1 | 你现在有多少电量 | 准确 | ROBOT_STATUS | state_ready=true, soc=48 | 本地真实数据 | ❌ | "我的电池电量大约是百分之48。"（41B） |
| 2 | 你的 IMU 数据正常吗 | 准确（你的imu数据正常吗） | ROBOT_STATUS | state_ready=true | 本地，仅描述接收 | ❌ | 93B，"持续接收…未完成健康诊断" |
| 3 | 你现在运行状态怎么样 | 准确 | ROBOT_STATUS | state_ready=true | 本地汇总 | ❌ | 131B，电量+IMU+动作空闲 |
| 4 | 你现在正在执行什么动作 | 准确 | ROBOT_STATUS | action VALID idle | 本地 | ❌ | 60B，"手臂处于空闲状态" |
| 5 | 脑机接口是什么 | **"机接口是什么"**（ASR 截断"脑"） | GENERAL_QA | NORMAL（knowledge form） | Ollama | ✅ 261B | 正常知识回答 |
| 6 | 实验室当前状态怎么样 | 准确 | LAB_INTRO | KB found=false | knowledge base 兜底 | ❌ | 162B 资料不足话术 |
| 7 | 宇树 G1 多少钱 | **"铝数机多少钱"** | GENERAL_QA | **PRICE 拦截** | 价格兜底 | ❌ | 87B 报价兜底 |

- 所有播报均 `TtsMaker returned ret=0`（真实扬声器输出，用户确认听感正常）。
- ASR 误识如实记录（第 5、7 句），均为原生 ASR 偏差，路由对误识文本反应正确（知识→Ollama、价格→拦截）。
- 测试前后有 ASR 噪声输入（"唤醒模式""你可以帮我导管吗""也す""は"），走 GENERAL_QA→Ollama，回答无害，不影响验收项。

## F. 安全检查

```text
是否执行真实机器人动作：否（仅启动 Executor，未发动作请求）
是否改动 Action Executor：否
是否更改 DDS / 网络 / JetPack / CUDA：否
是否修改 Ollama 模型或服务：否
是否存在重复进程：否（精确路径匹配：Router 11673/Provider 11528/Executor 11882 各 1）
是否残留 Domain 77 测试节点：否（0 个）
是否覆盖历史备份：否
```

- 回滚方案：`kill -INT <PID>` 精确停止本次启动的节点；Provider 可直接停止而不影响语音链路（Router 安全兜底）。备份位于 `~/g1_backups/g1_robot_state_production_20261102_193307_before_p0181/`（源码/二进制/配置/SHA256SUMS）。

## G. 遗留事项

1. 电机温度、BMS 温度、电流单位仍未确认；motor 索引→关节名映射未知 → 相应字段一律不播报。
2. IMU 仅验证"数据在接收"，未做传感器健康诊断（零偏/校准）。
3. `/dog_odom` 未接入 Provider；无位姿回答。
4. 动作"执行中"播报仅经 Mock（P0-18 隔离域）验证，真机运动中的 Provider 播报未测（本阶段禁止动作）。
5. 真实麦克风对专有名词（脑机接口、宇树G1）识别仍不稳定，属 ASR 层问题。
6. 生产节点当前为 nohup 后台方式，无开机自启；G1 重启后需按本报告第 C 节命令手工恢复（含 lidar_ws 环境）。

## 验收对照（任务书十三）

1 Provider 生产运行 ✅；2 真实 BMS 接收 ✅；3 SOC 仅 VALID 播报 ✅（本次 VALID 48%）；4 真实 LowState ✅；5 IMU 不夸大 ✅；6 Action 真实读取 ✅；7 Router 订阅 `/g1/robot_state` ✅；8 ROBOT_STATUS 基于真实数据 ✅；9 缺失/过期兜底 ✅（P0-18 已证 STALE 路径）；10 不调 Ollama ✅；11 GENERAL_QA 正常调 Ollama ✅；12 LAB_INTRO 用 KB ✅；13 PRICE 拦截 ✅；14 真实 ASR 正常 ✅；15 真实 TTS 正常 ✅；16 Executor 在线且未执行动作 ✅；17 无重复实例 ✅；18 报告与证据已保存 ✅。
