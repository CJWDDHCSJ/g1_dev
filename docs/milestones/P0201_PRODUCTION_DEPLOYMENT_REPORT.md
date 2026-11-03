# P0-20.1 生产 Voice Router 切换与单动作真机验收 · 交付报告

- 日期：2026-11-02 ~ 2026-11-03
- 结论：**P0-20 新版本已在生产 Domain 0 运行；非动作语音回归通过（动作请求计数 0）；单次 wave 真机验收通过（action=wave/id=26，请求 1 次，`success:wave:26`，TTS 反馈正确，现场确认真实挥手）；动作后业务恢复正常；未发生回滚。**
- 证据：`evidence_p0201/`（录制序列、Router/Executor/TTS 尾巴日志、systemd 快照）

## 1. 部署前 PID 快照（2026-11-02 14:02 之前，实测）

| 服务 | MainPID | ActiveState | NRestarts |
|---|---:|---|---:|
| g1-robot-state-provider | 1365 | active | 0 |
| g1-tts | 1366 | active | 0 |
| g1-voice-router | 1367 | active | 0 |
| g1-asr-bridge | 1368 | active | 0 |
| g1-action-executor | 1369 | active | 0 |

`g1-voice-stack.target`：active。
旧 Router `/proc/1367/exe` 指向 `build/.../voice_router (deleted)`——磁盘二进制已被新编译覆盖，运行中的是旧 inode，**已用 `cp /proc/1367/exe` 保存真实旧二进制**。

## 2. 新旧二进制与版本确认

| 对象 | SHA256 |
|---|---|
| 旧生产二进制（`/proc/1367/exe` 副本） | `67681e51596739df9bd8fddcaaa3f78ab9e39e98c196050833bdaf51d349d423` |
| 新 P0-20 二进制（build/install 指向同一文件） | `e4eda4bbd83feb57e6695f1a3e5a4157e70c1e9a9431e387d18c66f1f4c4ac57` |

- 新二进制包含 `ACTION_SENT` / `ACTION_NOT_EXECUTED` 字符串，旧二进制不含 → 版本判定有据，非仅看文件时间。
- 源码哈希与 P0-20 报告一致（`voice_router.cpp=dfbe656…`、`action_command_mapper.cpp=1dac270…`），切换前又因 `drain_notices` 修复重新编译，最终二进制为 `e4eda4bb…`。

## 3. Action Result 关联风险审计

审计源码 `arm_action_executor.cpp` 与 Router `wait_action_result()`，实测补充发现：

1. **执行器实际会先发 `accepted:<name>:<id>`**（P0-20 报告未记录该类型）。Router 解析器只认 `success|failed|timeout|rejected`，`accepted` 被忽略并继续等待——实测行为正确（录制序列可见 `accepted:wave:26` 后 13s 才收到 `success:wave:26`）。
2. **无请求 ID**：现有关联为「串行 worker + 动作名 + 发送前序号快照」。风险：
   - DDS 延迟的旧 Result：被序号快照过滤（早于发送的结果不会参与匹配）。
   - 同名动作连续执行：若上一条的 `success:wave:26` 在本条发送后才到达，会被误认为本条结果。**当前靠"每条必须收到结果才继续下一条 + 无自动重试"降低概率，但不等于强关联。**
   - 名字不匹配的结果：被忽略并继续等待至超时，不会提前结束当前请求。
   - 重复 Result：只有第一条匹配成功的结果生效（后续因请求已结束不参与）。
   - REJECTED/TIMEOUT/FAILED：分别映射到拒绝/超时/失败文案，不重试。
3. **结论（必须写入后续设计约束）**：现有机制是"单请求串行关联"，**不是带请求 ID 的强关联**。生产使用前提：`/g1/action_request` 只有 1 个发布者（实测 1 = g1_voice_router）、同时最多 1 个动作在途（worker 串行）、禁止外部节点并发发布动作请求、结果不确定时不得重复执行。

## 4. QUEUE_FULL 顺序：复测前先修了一处真实缺陷

- 复查发现 `drain_notices()` 原本**每条请求处理完就无条件执行**，会在已接受请求尚未答复时插入忙碌提示，违反要求。
- 修复：仅当 `interaction_manager_.pending_count() == 0`（无任何已接受请求待处理）时才发布通知；新增 `pending_count()` 接口，通知队列仍为容量 2 的有界队列，溢出丢最旧并告警。
- 复测（DOMAIN_ID=77，delay8 慢响应 + 6 条快速提交）：`QUEUE_FULL` 2 次，TTS 序列为 **4 条已接受回复 → 2 条忙碌提示**（时间戳 086/094/102/110 回复，110.854 起提示），顺序正确。
- 复测中出现"每条回复重复两次"，经查为**两个录制器同时订阅**造成（测试环境残留），非重复发布；已在报告注明。

## 5. P0-20 请求总数证据差异调查（如实纠正）

P0-20 报告称"动作请求 5 次"，但原始 `p020_action.log` 实为 **7 条**：

| 记录 | 名称 | 来源 |
|---|---|---|
| REQ 1 | wave | 场景 1「挥个手」 |
| REQ 2 | clap | 场景 2「拍个手」(fail) |
| REQ 3 | hands_up | 场景 3「举手」(no response → timeout) |
| REQ 4 | heart | 场景 5「比个心 ×2」去重后仅 1 次 |
| REQ 5 | wave | 场景 6 混合顺序中的「挥个手」 |
| REQ 6/7 | hands_up ×2（同一毫秒） | **SIGINT 测试（T3）时同域存在两个测试 Router** |

- 原因：`pgrep` 匹配到 `ros2 run` 包装进程，T2 的节点未被杀死，T3 启动后同域两个 Router 同时订阅 `/g1/asr_text`，同一命令产生两次动作请求。
- 性质：**测试环境缺陷**，不是生产代码缺陷；但它真实演示了"同域双 Router → 重复动作请求"的风险，因此本阶段切换与验收全程校验 Router 进程数 = 1。
- 场景 1/2/3/4 与 REQ 1-4 一一对应，无"凭空补齐"。P0-20 报告中"5 次"应更正为"7 条记录，其中 5 条为既定场景，2 条为测试环境重复订阅产物"。

## 6. 生产切换记录

| 项目 | 值 |
|---|---|
| 备份目录 | `~/g1_backups/g1_p0201_20261102_140228_before_router_switch/` |
| 备份内容 | 旧二进制、新二进制、`g1-voice-router.service`、`start_voice_router.sh`、`tts_config.json`、`lab_knowledge.json`、旧日志尾巴、PID 快照、SHA256 |
| 操作 | `systemctl --user restart g1-voice-router.service`（**未**重启 target，未动其他服务） |
| 切换后 PID | 15185（旧 1367 退出） |
| `/proc/15185/exe` | `build/g1_voice_router/voice_router`，哈希 `e4eda4bb…` = 新版 |
| Router 进程数 | 1 |
| 其他四节点 | 1365 / 1366 / 1368 / 1369 **未变** |
| 启动日志 | KnowledgeBase FAQ=21 加载、Ollama URL 正确、无错误 |

### 6.1 期间发生一次整机重启（非本阶段操作）

2026-11-03 06:03 G1 重启（`uptime` 显示 boot 06:03），systemd 用户服务 06:03:48-49 自动拉起：
`1379(provider) / 1380(tts) / 1381(router) / 1382(asr) / 1383(executor)`，NRestarts 均为 0。
- `/proc/1381/exe` 哈希仍为 `e4eda4bb…`（新版）→ **P0-18 的开机恢复把 P0-20 新版本拉起，新版本经受住了重启验证**。
- 因此本阶段验收（回归 + wave）全部发生在**新版本**上，且是在重启后自动恢复的生产环境中完成的。

## 7. 生产非动作语音回归（真实麦克风，动作请求必须为 0）

| # | 用户说话 | ASR 文本 | Intent | 处理路径 | 结果 |
|---|---|---|---|---|---|
| 1 | 脑机接口是什么 | （见 §9，ASR 波动） | GENERAL_QA | Ollama | 194 字节回答，正常播报 |
| 2 | 你现在有多少电量 | 走本地 | ROBOT_STATUS | RobotStateProvider（state_ready=true） | 41 字节本地回答 |
| 3 | 你现在的 IMU 数据接收情况怎么样 | `你现在的IMU数据接收情况怎么样？` | GENERAL_QA | **FactRiskGuard 拦截**（REALTIME_STATUS） | 114 字节本地兜底，未调用 Ollama |
| 4 | 你正在执行什么动作 | 走本地 | ROBOT_STATUS | RobotStateProvider | 60 字节本地回答（当前无动作） |
| 5 | 你现在运行状态怎么样 | 走本地 | ROBOT_STATUS | RobotStateProvider 聚合 | 131 字节、4 句播报：“我的核心状态数据正在持续接收。**电池电量大约百分之93**。IMU数据接收正常。当前没有在执行动作。” |

- **`grep -c ACTION_SENT` = 0，Executor 日志动作请求计数 = 0**：整段回归未触发任何动作。
- TTS 全部 `ret=0`，逐句播报正常，无重复播报。
- 说明：第 5 项的"电池电量大约百分之93"来自真实 BMS（`/lf/bmsstate` soc），不是模型生成。

## 8. 单次 wave 真机验收

现场确认：机器人稳定站立、周围清空、有人可随时停机、无其他动作程序。

| 项目 | 实测 |
|---|---|
| ASR 文本 | `挥个手`（text_len=12 字节） |
| Router request_id | 7 |
| Intent | DEMO_ACTION |
| Mapper 结果 | `wave` / ID 26 |
| `/g1/action_request` 实际消息 | `wave`（录制时间 1793687411.470） |
| `/g1/action_result` 序列 | `accepted:wave:26`（411.471）→ `success:wave:26`（424.461） |
| Router 关联 | `ACTION_FINISHED outcome=SUCCESS action=wave` |
| `/g1/tts` | `好的，挥手动作完成了。`（424.461，33 字节） |
| 真实动作 | 现场确认挥手完成 |
| 动作请求次数 | **1**（录制器 `ACTION_REQ` 计数 = 1；Router `ACTION_SENT` 计数 = 1） |
| 重复请求 | **0** |
| 执行器内部 | 动作开始 → `[Arm State] id=99 release_arm` → idle → 3s 稳定 → 发布 success |

## 9. 动作后业务恢复（真实麦克风）

| 项目 | 实测 |
|---|---|
| 请求 | request_id=8，`intent=GENERAL_QA`，`WAITING_OLLAMA` |
| 结论 | **确实走了 Ollama**（`total_ms=5500`，228 字节回答，Mock/网络无关） |
| 异常 | ASR 把“脑机接口”识别为 **“脑急接口”**，模型据此回答“‘脑急接口’不是标准术语……” |
| 业务恢复 | Worker 正常处理下一请求，无 `ACTION_WAITING` 残留，Executor `is_executing=false`（robot_state age 4ms） |

**用户最初怀疑"没走 Ollama"，日志证明相反**：走了 Ollama，但输入被 ASR 识别错词，导致答案看似"不准确"。这是 ASR 层问题，不是路由或模型调用问题。

## 10. 五个生产节点对比

| 服务 | 切换前 | 切换后 | 重启后（现状） | 说明 |
|---|---:|---:|---:|---|
| g1-robot-state-provider | 1365 | 1365 | 1379 | 整机重启导致（非本阶段操作） |
| g1-tts | 1366 | 1366 | 1380 | 同上 |
| g1-voice-router | 1367 | **15185** | 1381 | 本阶段切换；随后整机重启 |
| g1-asr-bridge | 1368 | 1368 | 1382 | 未由本阶段重启 |
| g1-action-executor | 1369 | 1369 | 1383 | 未由本阶段重启 |

- 本阶段**只重启过 `g1-voice-router.service`**；其余四服务在切换前后 PID 完全一致。
- 06:03 的整机重启使全部 PID 变化，属外部事件，已如实记录（未修改 systemd/target/logrotate）。
- g1-voice-stack.target：active。

## 11. 结果汇总

| 项目 | 结果 |
|---|---|
| 生产 Router 版本 | P0-20 新版（`e4eda4bb…`，重启后仍为新版） |
| 非动作回归 | 通过（5 项；动作请求 0） |
| wave 真机 | 通过（1 次请求，`success:wave:26`，ID 26） |
| 真实动作次数 | **1** |
| 重复动作 | **0** |
| 回滚 | **未触发** |
| 回滚可用性 | 旧二进制 `67681e…` 在备份目录，替换 + `systemctl --user restart g1-voice-router.service` 即可回滚 |
| systemd / logrotate / 其他模块 | 未修改 |

回滚命令（如需）：

```bash
B=~/g1_backups/g1_p0201_20261102_140228_before_router_switch
systemctl --user stop g1-voice-router.service
cp "$B/voice_router_old_production" ~/unitree_ros2/example/build/g1_voice_router/voice_router
sha256sum ~/unitree_ros2/example/build/g1_voice_router/voice_router   # 应为 67681e51...
systemctl --user start g1-voice-router.service
```

## 12. 尚未解决的问题

1. **IMU 类问法被 Guard 拦截而非走本地状态**：“你现在的 IMU 数据接收情况怎么样”被判为 GENERAL_QA → FactRiskGuard REALTIME_STATUS → 通用兜底。回答安全（不编造），但比实际可提供的信息保守。建议后续：Guard 判定为 REALTIME_STATUS 且 RobotStateProvider `state_ready=true` 时，改走本地状态应答（需单独评估并测试）。
2. **ASR 短词误识**：“脑机” → “脑急”、“宇树G1” → “语数之一”等。当前无同义词纠错，会直接影响 Ollama 答案质量。建议后续在文本层做有限别名纠正（只对高风险专有名词，且保留原文日志）。
3. Action Result **无请求 ID**，同名动作连续执行存在理论上的延迟结果误关联（当前靠串行 + 不重试缓解）。
4. 执行器 `accepted:<name>:<id>` 结果类型此前未文档化，现已知并确认被正确忽略。
5. 生产日志按日轮转，跨日测试需同时查 `voice_router.log-YYYYMMDD`（本阶段证据已包含轮转文件内容）。
6. 本次仅验证 `wave`；其余五个白名单动作仍未真机验收。
