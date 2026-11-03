# 单元测试

> 测试代码：`ros2_ws/src/g1_voice_router/test/`（7 个目标，453 项断言）。
> 运行环境：G1 ARM64 真机（2026-11-03 全量回归）。

## 1. 总览

| 套件 | 断言数 | 被测模块 | 测试性质 |
|---|---:|---|---|
| `asr_text_normalizer_test` | 22 | AsrTextNormalizer | 纯函数 |
| `action_command_mapper_test` | 39 | ActionCommandMapper | 纯函数 |
| `interaction_manager_test` | 76 | InteractionManager + filter | 多线程（真实线程与条件变量） |
| `intent_recognizer_test` | 96 | IntentRecognizer | 纯函数 |
| `fact_risk_guard_test` | 119 | FactRiskGuard（依赖真实知识库 JSON） | 纯函数 + 文件 |
| `knowledge_base_test` | 57 | KnowledgeBase（加载真实 JSON） | 纯函数 + 文件 |
| `robot_state_evaluator_test` | 44 | RobotStateEvaluator | 纯逻辑（注入时钟） |
| **合计** | **453** | — | — |

## 2. 运行方式

统一方式（推荐的回归入口）：

```bash
cd ~/unitree_ros2/example
colcon test --packages-select g1_voice_router
colcon test-result --verbose
```

期望输出：

```text
Summary: 7 tests, 0 errors, 0 failures, 0 skipped
```

单套件直跑（排障更快，能看到具体断言）：

```bash
cd ~/unitree_ros2/example/build/g1_voice_router
./asr_text_normalizer_test
./action_command_mapper_test
./interaction_manager_test
./intent_recognizer_test
./fact_risk_guard_test   ../../src/g1_voice_router/config/lab_knowledge.json
./knowledge_base_test    ../../src/g1_voice_router/config/lab_knowledge.json
./robot_state_evaluator_test
```

> `fact_risk_guard_test` 与 `knowledge_base_test` 需要传入知识库路径（CMake 中已用 `${CMAKE_CURRENT_SOURCE_DIR}/config/lab_knowledge.json` 配置），直跑时要自己给。

## 3. 各套件覆盖重点

### 3.1 ASRTextNormalizer（22）

- `ASR001`：`脑急接口` / `脑筋接口` → `脑机接口`；
- `ASR002`：`语数之一` / `语数g1` → `宇树G1`，必须带机器人语境；
- 8 个控制词（不要/不用/别/停止/取消/不准/不许/别动）触发 `CONTROL_TOKEN_PROTECTED`；
- 无机器人语境时保留原文（`UNITREE_ALIAS_WITHOUT_CONTEXT`）；
- 数字不被修改；`raw_text` 始终保留；`digest()` 稳定可复现。

### 3.2 ActionCommandMapper（39）

- 6 个动作的全部登记说法；
- **最长匹配**：`低位挥手` 必须判 `wave_low/25` 而不是 `wave/26`；
- 8 个否定词 → `NEGATED`；17 个疑问/能力词 → `QUESTION`；
- 禁用表达（含 `hand_on_heart` 口语变体）→ `DISABLED`；
- 空文本、无匹配 → `UNKNOWN` + 能力提示；
- 出口校验：不会输出白名单外 ID 或 33。

### 3.3 InteractionManager（76）

- 噪声过滤：`嗯`/`嗯嗯`/`嗯嗯嗯嗯嗯`/纯标点被滤，`你好`/`IMU`/`G1`/`停止` 保留；
- 去重：窗口内第二次 → `DUPLICATE`；窗口外正常接受；
- 去重表有界性：`dedupe_history_size()` 不会无限增长；
- 有界队列：满时拒绝**最新**请求并带 `QUEUE_FULL`；
- 状态机：合法转移成功、非法转移返回 `false`；
- 停机：`stop()` 唤醒 `wait_pop` 并清空队列；
- 统计：`received/filtered/duplicates/rejected_full/completed/failed/timed_out` 计数正确。

### 3.4 IntentRecognizer（96）

- 七类意图的正例与反例；
- 知识问法 vs 实时状态的冲突用例（“IMU 是什么” vs “你的 IMU 正常吗”）；
- 复合问法（“你好，介绍一下实验室”）不误判为纯问候；
- 实验室状态 vs 机器人状态（“实验室当前状态怎么样” → `LAB_INTRO`）；
- 能力询问（“你会做哪些动作”）→ `DEMO_ACTION`。

### 3.5 FactRiskGuard（119）

- 13 个价格关键词全部拦截；
- 传感器词无条件拦截；泛化状态词需机器人语境；
- 知识形式豁免（“IMU 是什么”“动作规划是什么”）；
- “什么状态”豁免（否则状态询问会被知识规则放行）；
- 实验室实体 → `LAB_FACT`，知识库有/无可靠答案两条分支；
- 防过度拦截用例（8 项）。

### 3.6 KnowledgeBase（57）

- 各类结构化字段命中；
- FAQ 相似度命中与同分兜底；
- 未收录问题 → `fallback_*`；
- `matched_id` 与 `sources` 可回溯。

### 3.7 RobotStateEvaluator（44）

- 四态判定：`VALID` / `STALE`（刚好超时）/ `UNAVAILABLE` / `INVALID`；
- SOC 越界、SOC 跳变（<60 s 内 ≥15%）；
- JSON 往返（`to_json` → `parse_robot_state_json`）；
- `schema_version` 不匹配 → 解析失败；
- 话术分支：电池/IMU/动作/温度/整体。

## 4. 测试设计约定

- 纯逻辑模块不依赖 ROS，时钟以毫秒参数注入，因此可在 x86 上跑（但**冻结值以 G1 ARM64 为准**）；
- 话术断言只校验关键片段（如“无法准确确认”），不锁死整句，避免文案微调导致大面积失败；
- 每个边界修复都要求“先加用例再改代码”，P0-17.2 / P0-21 的修复均按此执行。

## 5. 与历史记录的数字差异

- P0-17.1 时期 `IntentRecognizer` 为 84 项、P0-17.2 修复后 86 项，P0-21 增加 IMU 与能力询问用例后为 **96 项**；
- `src/intent_recognizer_test.cpp`（154 行）是早期放在 `src/` 下的历史副本，**不参与构建**，不代表测试总量。

因此引用测试规模时，一律以 `colcon test-result` 与本文表格（453）为准。
