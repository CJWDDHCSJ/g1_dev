# 模块设计

统一模板：解决什么问题 / 输入 / 输出 / 依赖 / 位置 / 限制。

## 1. AsrTextNormalizer

- **问题**：ASR 对专有名词有稳定的错识模式，直接把错词送进模型会答非所问。
- **输入**：原始 ASR 文本。**输出**：`AsrNormalization{raw_text, normalized_text, changed, rule_id, reason}`。
- **规则**：`ASR001` 脑急接口/脑筋接口 → 脑机接口；`ASR002` 语数之一 → 宇树G1（**仅当**出现机器人/宇树/价格/动作等上下文）。
- **保护**：含控制词（不要/别/停止/取消/不准/不许）整体跳过；不改数字；`raw_text` 永久保留。
- **源码**：`src/asr_text_normalizer.cpp`；测试 `test/asr_text_normalizer_test.cpp`（22 项）。
- **限制**：词典式，不做模糊匹配/编辑距离，覆盖不封闭。

## 2. IntentRecognizer

- **输入**：归一化文本。**输出**：`IntentResult{intent, matched_keywords}`。
- **枚举**（源码实际值，共 7 类）：`GREETING`、`LAB_INTRO`、`ROBOT_INTRO`、`DEMO_ACTION`、`ROBOT_STATUS`、`GOODBYE`、`GENERAL_QA`。
- **注意**：`PRICE` **不是意图**，而是 `FactRiskGuard` 的风险类型；价格问题在意图上属于 `GENERAL_QA`，随后被风控拦截。
- **语义边界**：知识问法（是什么/如何/原理，且不含“当前/现在/目前”）优先判为知识类；“实验室当前状态怎么样”归 `LAB_INTRO` 而非 `ROBOT_STATUS`。
- **源码**：`src/intent_recognizer.cpp`；测试 96 项。

## 3. InteractionManager

- **问题**：并发输入导致乱序、重复、过载。
- **机制**：`submit()` 做过滤/去重/入队；`wait_pop()` 供 worker 取任务；状态机校验转移合法性；`pending_count()` 用于判断“忙碌提示是否可以发布”。
- **参数**：等待上限 3（不含在处理的一条）；去重窗口 1200ms；去重历史 ≥64 条或每 32 次提交清理过期记录。
- **停机**：`stop()` 唤醒 worker；HTTP 由 curl 进度回调中断；动作等待由条件变量唤醒。
- **源码**：`src/interaction_manager.cpp`、`src/interaction_filter.cpp`；测试 76 项。

## 4. FactRiskGuard

- **输入**：文本 + `KnowledgeBase*`（可为空）。**输出**：`FactRiskResult{risk, blocked, use_local_answer, reason, fallback_answer, kb_*}`。
- **判定顺序**：价格 → 动作执行状态 → 知识问法豁免 → 传感器词 → 状态词+机器人语境 → 实验室实体。
- **风控类型**：`PRICE`、`REALTIME_STATUS`、`LAB_FACT`、`NORMAL`。
- **源码**：`src/fact_risk_guard.cpp`；测试 119 项（含 8 项负向）。

## 5. KnowledgeBase

- **输入**：文本。**输出**：`KnowledgeResult{found, answer, match_type, matched_id, match_score, clarification}`。
- **数据**：`config/lab_knowledge.json`（FAQ 21、方向 3、项目 1、设备 1、分类 5、实验室 3、来源 15）。
- **策略**：实体掩码 + 意图相容判断；同分且无法区分时**安全兜底**而不是猜。
- **限制**：事实必须来自已核实来源；未收录一律“暂无经过核实的公开资料”。

## 6. RobotStateProvider / RobotStateEvaluator

- **Provider**：订阅 `/lf/bmsstate`、`/lowstate`、`/sportmodestate`、`/arm/action/state`，2Hz 发布 `/g1/robot_state`。
- **Evaluator**：解析 JSON → `AggregatedRobotState`；按问题类型（电量/IMU/动作/综合）生成回答；四态判定。
- **源码**：`src/robot_state_provider.cpp`、`src/robot_state_evaluator.cpp`；测试 44 项。

## 7. ActionCommandMapper

- **白名单**：wave 26 / wave_low 25 / hands_up 15 / clap 17 / heart 20 / victory 30。
- **判定**：永久禁用 → 否定/停止 → 疑问/能力询问 → 最长短语匹配；出口二次校验。
- **源码**：`src/action_command_mapper.cpp`；测试 39 项。

## 8. Action Executor（独立包）

- **输入**：`/g1/action_request`（规范动作名）。**输出**：`/g1/action_result`。
- **前置检查**：FSM 状态、`mode_machine`、机械臂是否空闲（`/arm/action/state`）。
- **超时**：启动 5s、完成 30s、动作后稳定 3s（最坏约 38s）。
- **结果格式**：`accepted:<name>:<id>`、`success:<name>:<id>`、`failed:...`、`timeout:...`、`rejected: <原因>`。
- **限制**：协议无请求 ID；单 Worker 串行。

## 9. TTS 集成

- `/g1/tts` → `g1_tts_node` → `AudioClient::TtsMaker(sentence, speaker_id)`。
- 中文分句、逐句发送、按字数估算播放时长 + `pause_ms=1500`。
- 配置：`speaker_id=0`、`volume=80`。
- **限制**：`ret=0` 仅代表调用成功，不代表扬声器播放完成。

## 10. 运维模块

- 用户级 systemd：5 个 service + target + 2 个 timer；`Restart=always`、`KillSignal=SIGINT`、`TimeoutStopSec=15`。
- logrotate：按日 + 20M 上限 + 保留 7 份 + 压缩；磁盘检查每 6 小时。
- 开机恢复：整机重启后约 45s 五节点自动就绪（实测）。
