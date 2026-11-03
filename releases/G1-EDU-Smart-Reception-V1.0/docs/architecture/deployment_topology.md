# 部署拓扑

```mermaid
flowchart TB
    subgraph G1[G1-EDU · Jetson Orin NX · Ubuntu 22.04.5 · ROS 2 Humble]
        direction TB
        RAW[Unitree 原生语音/状态服务] --> N1[g1-asr-bridge]
        RAW --> N2[g1-robot-state-provider]
        N1 --> N3[g1-voice-router]
        N2 --> N3
        N3 --> N4[g1-tts]
        N3 --> N5[g1-action-executor]
        N5 --> N3
        N4 --> SPK[G1 扬声器]
    end
    subgraph HOST[外部模型服务主机]
        OL[Ollama · /api/chat]
    end
    N3 <-->|HTTP 局域网| OL
```

## systemd 结构

```mermaid
flowchart TB
    T[g1-voice-stack.target]
    T --> S1[g1-asr-bridge.service]
    T --> S2[g1-robot-state-provider.service]
    T --> S3[g1-voice-router.service]
    T --> S4[g1-tts.service]
    T --> S5[g1-action-executor.service]
    T --> S6[g1-log-rotate.timer]
    T --> S7[g1-disk-check.timer]
```

| 属性 | 值 |
|---|---|
| `Restart` | `always` |
| `RestartSec` | `3` |
| `KillSignal` | `SIGINT` |
| `TimeoutStopSec` | `15`（实测 SIGINT 退出 < 1s） |
| 启动顺序 | `voice-router` 在 provider 与 tts 之后 |

## 位置说明

- 模型服务运行在**与机器人直连的外部主机**上，不在 Jetson 本地；文档中一律使用 `<LLM_HOST>` 占位。
- `g1_voice_router` 与 `g1_asr_bridge` 属 `unitree_ros2/example` 工作区；`g1_action_executor` 属独立 `lidar_ws` 工作区，需分别构建与 source。
