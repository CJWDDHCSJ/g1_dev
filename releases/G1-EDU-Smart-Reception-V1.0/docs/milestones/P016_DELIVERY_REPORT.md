# P0-16 真实语音闭环验收报告

- 日期：2026-11-01
- 项目：《基于 ROS 2 的 G1-EDU 校园实验室智能接待与讲解系统 V1.0》
- 结论：**真实麦克风 ASR → ROS 2 → Voice Router（意图+FactRiskGuard）→ 本地知识库/Ollama → TTS → G1 扬声器 → 动作联动的完整闭环验收通过。**

## 1. 实际启动命令与终端分配（DOMAIN_ID 0，生产域）

全部通过 SSH 在 G1 上以 nohup 启动，日志位于 `~/g1_logs_p016/`：

```bash
source ~/unitree_ros2/setup.sh
source ~/unitree_ros2/example/install/setup.bash
nohup ros2 run g1_asr_bridge asr_json_bridge      > ~/g1_logs_p016/asr.log     2>&1 &
nohup ros2 run g1_voice_router voice_router       > ~/g1_logs_p016/router.log  2>&1 &
nohup ros2 run unitree_ros2_example g1_tts_node   > ~/g1_logs_p016/tts.log     2>&1 &

# 动作联动阶段（第四阶段）另行启动：
source ~/lidar_ws/install/setup.bash
nohup ros2 run g1_action_executor g1_action_executor_node > ~/g1_logs_p016/executor.log 2>&1 &
```

## 2. 节点 PID 与运行状态（验收时实测）

| 节点 | PID | 状态 |
|---|---|---|
| asr_json_bridge | 8247 | 运行中，`/audio_msg → /g1/asr_text` |
| voice_router | 8270 | 运行中，KB=21 条 FAQ，Ollama URL 正确 |
| g1_tts_node | 8248 | 运行中，speaker_id=0, volume=80, pause=1500ms |
| g1_action_executor_node | 8880 | 运行中，白名单 6 动作，Arm State 空闲 |

验收结束时四个节点保持运行（生产语音链路状态）。停止方式：`kill -INT <节点PID>`（注意 `ros2 run` 包装进程与实际节点进程是两个 PID）。

## 3. 六项语音测试结果（真实麦克风）

| # | 用户输入 | ASR 实际识别 | 意图 | FactRiskGuard | Ollama | TTS 播放 | 结果 |
|---|---|---|---|---|---|---|---|
| 1 | 你好 | 首次误识为"唤醒模式/介绍/介绍一下"；复测正确识别"你好" | 复测 GREETING ✅ | — | 首次走了 Ollama；复测走固定问候 | ✅ | 复测通过（见问题 1） |
| 2 | 介绍一下你自己 | 介绍一下你自己 | ROBOT_INTRO ✅ | —（本地） | ❌ | ✅ 245B/3句 | ✅ |
| 3 | 介绍一下我们实验室 | 介绍一下我们实验室 | LAB_INTRO ✅ | —（本地） | ❌ | ✅ 412B/3句 | ✅ KB 命中 faq_intro(score=114)，两次重复一致 |
| 4 | 脑机接口是什么 | 脑机接口是什么？ | GENERAL_QA ✅ | NORMAL 放行 ✅ | ✅ 6s | ✅ 177B | ✅ |
| 5 | 宇树 G1 多少钱 | **"语数之一多少钱？"**（ASR 误识） | GENERAL_QA | **PRICE 拦截** ✅ | ❌ 未调用 | ✅ 87B 兜底 | ✅ 保护对 ASR 噪声鲁棒 |
| 6 | 你现在有多少电量 | 你现在还有多少电量？ | ROBOT_STATUS | **REALTIME_STATUS 拦截** ✅ | ❌ 未调用 | ✅ 114B 兜底 | ✅ 双层生效 |

用户听感确认：六项回答全部正常，5、6 正确给出"无法回答"类兜底，无编造数据。

## 4. 动作联动测试结果（第四阶段）

| 触发语音 | ASR | 意图 | 动作 | 执行器日志 | /g1/action_result | 机械状态轨迹 |
|---|---|---|---|---|---|---|
| 你好 | "你好" | GREETING | wave (ID 26) | `Accepted action: wave, ID=26, FSM=501, mode_machine=5` | `accepted:wave:26` → `success:wave:26` | id=26 wave_above_head → id=99 release_arm → id=0 idle → 3s settle |
| 你好（复测） | "你好" | GREETING | wave (26) | 同上 | success | 同上 |
| 嗨 | "hello" | GREETING | wave (26) | 同上 | success | 同上 |
| 嗨（复测） | "hello" | GREETING | wave (26) | 同上 | success | 同上 |

- 共 **4 次挥手全部成功**，机器人真实做出挥手动作（用户确认）。
- 每次执行前执行器完成 FSM=501、mode_machine=5、Arm State holding=false 检查；未触碰 ID33 hand_on_heart，未使用任何白名单外动作。
- GREETING 同时发布固定问候语 TTS 与 wave 请求，两条链路并行正常。

## 5. 发现的问题和处置记录

1. **短问候语 ASR 识别不稳定（未修码，属原生 ASR 行为）**：首次测试"你好"被原生 ASR 转写为"唤醒模式/介绍/介绍一下"，走了 Ollama 普通对话（回答内容正常）；复测中"你好"识别正常并触发 GREETING+wave。处置：记录现象，建议演示时用清晰、完整的"你好"/"哈喽"；如需增强可后续扩充 `match_greeting_reply()` 表达（需先审 UTF-8 风险，本轮未改）。
2. **`Failed to parse response JSON` 警告（AudioClient）**：TTS/Action RPC 返回空响应体导致，均为历史已知现象，`ret=0`/`RPC returned: 0` 表示成功，不影响功能。未修改。
3. **ASR 噪声输入**（"系统旦病"、"はい"、"模式预备模式"）：走 GENERAL_QA→Ollama，回答无害；后续可在交互任务管理阶段考虑噪声过滤，本轮未改。
4. 本阶段**零代码修改**，未产生新备份需求。

## 6. 闭环验收结论

**达到完整闭环验收标准**：真实麦克风 → Unitree 原生 ASR → /audio_msg → asr_json_bridge → /g1/asr_text → IntentRecognizer + FactRiskGuard → 本地 KB / Ollama → /g1/tts → g1_tts_node → G1 扬声器真实发声，以及 GREETING → /g1/action_request → g1_action_executor → 真实挥手 → /g1/action_result，全链路真机验证通过。

证据文件（本地）：`/home/<user>/Mick_Grey/g1_dev/teach/g1_kb_work/evidence_p016/`（asr.log、router.log、tts.log、executor.log）。

遗留事项（下一阶段）：交互任务管理（噪声/连续提问队列）、ROBOT_STATUS 真实数据接入、DEMO_ACTION 语音触发扩展、软著材料。
