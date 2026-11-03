# 交互时序

## 1. 普通知识问答（示例：脑机接口是什么）

```mermaid
sequenceDiagram
    participant U as 用户
    participant A as Unitree ASR
    participant B as asr_bridge
    participant N as AsrTextNormalizer
    participant M as InteractionManager
    participant W as Worker
    participant I as IntentRecognizer
    participant G as FactRiskGuard
    participant O as Ollama
    participant T as TTS
    U->>A: 语音
    A->>B: /audio_msg (JSON)
    B->>M: /g1/asr_text
    M->>N: 归一化（保留原文）
    N->>M: 入队（过滤/去重/有界）
    M->>W: 出队（串行）
    W->>I: 意图识别
    I-->>W: GENERAL_QA
    W->>G: 事实风险检查
    G-->>W: NORMAL（放行）
    W->>O: HTTP /api/chat
    O-->>W: 回答文本
    W->>T: /g1/tts
    T-->>U: 分句播报
```

## 2. 机器人状态查询（示例：你现在还有多少电量）

```mermaid
sequenceDiagram
    participant U as 用户
    participant W as Worker
    participant I as IntentRecognizer
    participant R as Voice Router
    participant P as RobotStateProvider
    participant T as TTS
    U->>W: ASR 文本
    W->>I: 意图识别
    I-->>W: ROBOT_STATUS
    W->>R: 读取 /g1/robot_state 缓存
    R->>P: 最近一次聚合 JSON
    P-->>R: battery.status=VALID, soc_percent
    alt VALID 且缓存 ≤5s
        R-->>W: 播报电量
    else STALE / UNAVAILABLE / INVALID
        R-->>W: 安全兜底（不给旧值）
    end
    W->>T: /g1/tts
```

**不允许**把过期、缺失或格式错误的数据当作真实数值回答。

## 3. 动作执行（示例：挥个手）

```mermaid
sequenceDiagram
    participant U as 用户
    participant W as Worker
    participant M as ActionCommandMapper
    participant X as Action Executor
    participant T as TTS
    U->>W: “挥个手”
    W->>W: IntentRecognizer → DEMO_ACTION
    W->>M: 白名单映射
    M-->>W: wave / 26
    W->>X: /g1/action_request "wave"
    X-->>W: accepted:wave:26
    Note over X: FSM / 机械臂检查 → 执行
    X-->>W: success:wave:26
    W->>T: “好的，挥手动作完成了。”
```

约束：

1. 动作名只能来自白名单，模型不参与决定 ID；
2. 结果按“动作名 + 发送前序号快照”匹配，**协议没有请求 ID**；
3. 所有动作在单个 Worker 中串行，不支持并发；
4. 超时或结果缺失时**不重试**，只播报“未收到结果，不会自动重复执行”。
