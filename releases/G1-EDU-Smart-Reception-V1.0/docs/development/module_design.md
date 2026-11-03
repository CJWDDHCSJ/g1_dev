# 模块设计（内部机制）

> 本文补充 README §6 的“模块契约”，聚焦**内部数据结构、算法顺序与边界用例**，并按模块给出测试映射。
> 依据：各模块 `.cpp/.hpp` 与 `test/` 下的用例。

## 1. AsrTextNormalizer

**数据结构**

```cpp
struct AsrNormalization {
  std::string raw_text;        // 原文，永久保留
  std::string normalized_text; // 供业务使用
  bool changed{false};
  std::string rule_id;         // ASR001 / ASR002 / 组合
  std::string reason;          // CONTROL_TOKEN_PROTECTED / UNITREE_ALIAS_WITHOUT_CONTEXT / NO_RULE_MATCHED / EMPTY_TEXT
};
```

**算法顺序**

1. 空文本 → `EMPTY_TEXT`；
2. 控制词命中（8 个）→ 立即返回，**不做任何替换**（防止“不要挥手”被改成“不要挥手啦”之类的语义漂移）；
3. `脑急接口` / `脑筋接口` → `脑机接口`，记 `ASR001`；
4. `语数之一 / 语数g1 / 语数G1`：若同时含 11 个机器人语境词之一 → 替换为 `宇树G1`，记 `ASR002`（可与 ASR001 组合成 `ASR001+ASR002`）；否则返回 `UNITREE_ALIAS_WITHOUT_CONTEXT` 并保留原文；
5. `text != raw_text` 才算 `changed`。

**为什么不做模糊匹配**：`手机接口` 是合法提问，映射成“脑机接口”会破坏正常语义；这条边界在 P0-21 专项真机上被实际验证过（ASR 输出“手机接口”，系统正确地**没有**替换）。

**日志设计**：`digest()` 用 FNV-1a 32 位，只用于关联“同一条原始文本的多次尝试”，不参与任何业务判断；日志输出 `raw_len + raw_digest + normalized`，避免把可能含隐私的长句直接写日志。

**测试映射（22 项）**：两条规则的正向/反向、控制词 8 个、无机器人语境、数字不被修改、组合规则、空文本、`digest` 稳定性。

## 2. IntentRecognizer

**数据结构**：`IntentResult { Intent intent; std::string normalized_text; }`；归一化删除中英文标点与空白，仅对 ASCII 做小写（避免破坏 UTF-8 字节）。

**判定顺序（源码实际顺序）**

1. `GREETING`：整句等于 14 个问候短语之一（`你好/您好/你好啊/您好啊/嗨/嗨嗨/哈喽/hello/hi/早上好/早安/上午好/下午好/晚上好`）——**必须整句相等**，避免“你好，脑机接口是什么”被当成纯问候；
2. `GOODBYE`：整句等值匹配（再见/拜拜等）；
3. `DEMO_ACTION` 的强口令形式（“挥个手”等）；
4. 知识问法优先判定（含“是什么/如何/原理”等且不含“现在/当前/目前”）→ `GENERAL_QA`；
5. `ROBOT_STATUS`：电量/状态类词；IMU 类要求“传感器词 + 接收/更新/正常/状态”同时出现；
6. `LAB_INTRO`：实验室实体词；
7. 又一次 `ROBOT_STATUS` / `DEMO_ACTION` 的宽松匹配（能力询问）；
8. `ROBOT_INTRO`；
9. 兜底 `GENERAL_QA`。

顺序即优先级，这就是“知识问题不能被状态规则抢走”的实现方式。

**已知边界**：`PRICE` 不在意图层；“实验室当前状态怎么样”判 `LAB_INTRO`；"IMU 是什么"判 `GENERAL_QA`。

**测试映射（96 项）**：七类意图各若干正例/反例、复合问法、知识 vs 状态对冲、IMU 三种问法、能力询问。

## 3. InteractionManager

**数据结构**

```cpp
std::deque<InteractionRequest> queue_;                 // 有界 FIFO
std::unordered_map<std::string, long> last_accepted_ms_; // 去重：归一化文本 → 接受时刻
std::map<std::uint64_t, InteractionStatus> status_map_;  // 在途请求状态
InteractionStats stats_;                                 // 只增不减计数
```

**submit() 顺序**

1. `normalize_text` → 去首尾空白/标点、压缩空白、删除中英文标点、ASCII 小写；
2. `classify_text`：空 → `FILTERED_EMPTY`；纯噪声（语气词 ≤4 字 / 纯标点）→ `FILTERED_NOISE`；
3. 去重：同一归一化文本在 1200 ms 内再次出现 → `DUPLICATE`；历史表 ≥ 64 条或每 32 次提交清理一次 `cutoff = now - 2 × window` 的过期项（**保证内存有界**）；
4. 容量：`queue_.size() >= max_pending` → `REJECTED (QUEUE_FULL)`；
5. 通过 → `QUEUED`，`request_id` 自增，记录 `max_queue_depth`。

**状态转移白名单**（`can_transition`）：见 [interaction_sequence.md](../architecture/interaction_sequence.md)。终态拒绝任何出边。

**stop()**：置 `stopping_`、清空队列、唤醒 `wait_pop`。排队请求在清空前从 `status_map_` 移除（终态由统计承担），不在停机路径上做多余的锁竞争。

**测试映射（76 项）**：过滤边界（“嗯”“嗯嗯嗯嗯嗯”“你好”“IMU”“停止”）、去重窗口内/外、去重表有界性（`dedupe_history_size()`）、队列满顺序、状态机非法转移、停机唤醒、统计计数。

## 4. FactRiskGuard

**分类顺序（决定行为的关键）**

1. 价格词 → `PRICE`（无条件，不管是否提到具体产品）；
2. 动作执行状态问法（`做完了吗/正在执行/什么动作` **且** 含动作语境词）→ `REALTIME_STATUS`；**放在知识形式判断之前**，否则“你当前执行的是什么动作”会被“是什么”放行；
3. 知识形式（且不含实验室实体、不含“什么状态”）→ `NORMAL`；
4. 传感器词（电量/电池/电压/电流/续航/关节温度/IMU/惯导/摔倒没有…）→ `REALTIME_STATUS`（**无条件**）；
5. 泛化状态词（正常吗/运行状态/什么状态/温度…）**且** 含机器人语境（机器人/g1/宇树/关节/自身/本体/你/您）→ `REALTIME_STATUS`；
6. 实验室实体词 → `LAB_FACT`；
7. 其余 → `NORMAL`。

**LAB_FACT 的处理**：`KnowledgeBase` 未加载 → 直接兜底；命中但 `match_type` 以 `fallback` 开头 → 视为无可靠答案并兜底；只有 `found && answer 非空 && !fallback*` 才 `use_local_answer`。

**代价（明确接受）**：白名单式规则会误拦个别本可回答的问题，因此专门配了 8 项“防过度拦截”用例（如“IMU 是什么”“动作规划是什么”“多传感器如何进行状态融合”）。

**测试映射（119 项）**：价格 13 个关键词、传感器词、语境依赖、知识豁免、`什么状态` 豁免、实验室实体、兜底话术一致性。

## 5. KnowledgeBase

**加载**：`load(path, error)` 解析 JSON，校验 `schema_version`，统计各段条目数并打日志（`FAQ=21, directions=3, projects=1, equipment=1`），任一必需段缺失即加载失败（Router 会打 ERROR 但继续运行，LAB_FACT 全部兜底）。

**查询**：`query(text)` 依次尝试

1. 结构化字段匹配（实验室名、学院、方向、项目、设备、分类）；
2. FAQ 相似度打分（关键词覆盖率 + 长度归一）；
3. 实体掩码：把已识别实体替换为占位符后再比对，避免“同一个实体不同说法”拉低分数；
4. 意图相容判断：问答意图明显不匹配时降权；
5. 同分且无法区分 → `fallback_no_confident_match`。

**输出**：`{found, answer, category, matched_id, match_type, match_score}`；`matched_id` 可回溯到 `sources` 中的来源条目。

**测试映射（57 项）**：各类命中、未命中兜底、同分、来源字段、FAQ 与结构化冲突、空文本。

## 6. RobotStateProvider / Evaluator

**Provider 侧**（ROS 依赖）

- 四个订阅全部 BEST_EFFORT / KeepLast(1)；
- `/lowstate` 回调统计 `available_motor_count`（`|q|>1e-4` 或温度非 0）与 `temp_max_raw`，并用 `accelerometer.size()==3 && quaternion.size()==4` 判 IMU 可用；
- 每 500 ms `evaluate()` → `to_json()` → 发布；
- 所有 evaluator 访问都在 `evaluator_mutex_` 下。

**Evaluator 侧**（纯逻辑，可单测）

- 每个字段独立记录 `last_ms`、最近有效值、无效原因；
- `evaluate(now)` 按时效窗口给四态；
- `to_json` 把非 `VALID` 的数值写成 `null`；
- `parse_robot_state_json` 校验 `schema_version == 1` 与必需段；
- `classify_status_question` + `generate_status_answer` 生成话术（见 [robot_state_pipeline.md](../architecture/robot_state_pipeline.md)）。

**测试映射（44 项）**：各字段四态、边界时效（刚好 1000/2000 ms）、SOC 越界与跳变、JSON 往返、schema 不匹配、话术分支。

## 7. ActionCommandMapper

**判定顺序**：禁用表达 → 否定/停止 → 疑问/能力询问 → 最长短语匹配 → 白名单二次校验。

**最长匹配的理由**：“低位挥手”同时命中“挥手”与“低位挥手”，若按表顺序取第一个会误判成 `wave`；代码比较 `phrase.size()` 取最长。

**出口校验**：`whitelist_id(name)` 返回 0（不在白名单）或 33（禁用）一律 `DISABLED`，即使短语表被改错也不会输出非法 ID。

**测试映射（39 项）**：6 个动作的多说法、最长匹配、8 个否定词、17 个疑问词、禁用变体、空文本、歧义提示。

## 8. Action Executor

**状态变量**：`fsm_id_`、`fsm_mode_`、`mode_machine_`、`arm_action_id_`、`arm_holding_`、`action_busy_`、`last_accepted_time_`、`expected_action_id_`（均为原子量或锁保护）。

**请求处理顺序**：空 → 白名单 → FSM 已收到 → FSM ∈ {500,501,801}（801 需 mode ∈ {0,3}）→ `requires_mode_machine` 时 mode ∈ {5,6} → 手臂状态已收到 → `!holding` → `id == 0` → 非 stopping → 非 busy/队列非空 → 冷却 ≥3 s → 入队并 `publish_result("accepted:...")`。

**执行线程**：`ExecuteAction(id)` 同步 RPC → 等开始 5 s → 等空闲 30 s → 稳定 3 s → 依据 `ret` 与状态发 `success` 或 `completed_with_rpc_error`；任何“未确认”分支都**不释放 busy**。

**为什么把 RPC 放在独立线程**：`ExecuteAction` 是同步阻塞调用，若在 ROS 回调线程执行，`/arm/action/state` 与 `/sportmodestate` 会停止更新，超时判定就失去依据。

## 9. TTS 节点

**结构**：`enqueue_text()`（加锁、容量 10、满则丢新）→ worker 循环取出 → `split_sentences`（`。！？` 3 字节、`.!?` 1 字节）→ 逐句 `TtsMaker` → `estimate_speech_time`（250 ms/字符，最少 1000 ms）→ 句间 `pause_ms`。

**可中断性**：所有等待都走 `interruptible_wait()`（`condition_variable::wait_for` + `stopping_` 谓词），因此 SIGINT 能在毫秒级退出。

**诚实性**：`ret == 0` 只记“调用成功”；日志明确区分 “TtsMaker returned ret=0” 与“已播完”，系统不伪造播放完成。
