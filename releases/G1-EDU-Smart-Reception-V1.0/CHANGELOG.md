# 变更记录

## 文档重构 — 2026-11-03（V1.0 内容不变）

- README 重写为项目门户：架构图、三段时序、模块设计、接口表、测试证据、故障排查、限制与路线图
- docs 重组为 architecture / development / deployment / testing / milestones / assets 六组
- 新增 `docs/milestones/README.md` 阶段索引；阶段报告正文保持原样
- 修正：Intent 枚举中不存在 `PRICE`（价格属 FactRiskGuard 风险类型）；V1.0 Provider 不消费 `/dog_odom`
- 同步更新清单与文件台账（交付版 143 个文件，公开 131 + 私有 12）

## V1.0 — 2026-11-03（冻结）

首个可演示稳定版本。里程碑按内部阶段编号记录，详细报告见 `docs/milestones/`。

| 阶段 | 内容 | 关键结果 |
|---|---|---|
| P0-16 | 真实语音闭环（ASR → Router → Ollama/知识库 → TTS） | 真机通过；GREETING 触发挥手 4/4 |
| P0-17 | 机器人状态数据源审计 | 确认 `/lowstate`、`/sportmodestate`、`/arm/action/state`、`/dog_odom`、`/lf/bmsstate` 可用 |
| P0-17.1 | 实时状态问答漏放修复 + ROBOT_STATUS 本地兜底 | FactRiskGuard 119 项 |
| P0-17.2 | 意图语义边界修复（技术原理 vs 实时状态 vs 实验室） | IntentRecognizer 86 项 |
| P0-17.3 | 生产 Voice Router 切换（单服务） | 其余四节点 PID 不变 |
| P0-18 | RobotStateProvider、systemd 五节点、开机恢复、日志轮转、磁盘监测 | 重启 45s 内自恢复 |
| P0-19 | InteractionManager：过滤、去重、有界 FIFO、统一回复调度、SIGINT 处理 | 76 项；停机 <1s |
| P0-20 | 语音动作命令识别、白名单映射、Action Result 处理 | 39 项；Mock 全场景通过 |
| P0-20.1 | 生产切换 + 真实 wave 验收 + 忙碌提示顺序修复 | `success:wave:26` |
| P0-21 | ASR 归一化、IMU 状态路由、全量回归、V1.0 冻结 | 453 项全过 |
| P0-21 补充 | ASR001 别名专项复核 | 静态校验通过；真机未触发该分支（如实记录） |

### V1.0 测试基线

ASRTextNormalizer 22 / ActionCommandMapper 39 / InteractionManager 76 / IntentRecognizer 96 / FactRiskGuard 119 / KnowledgeBase 57 / RobotStateEvaluator 44 = **453 项**，colcon `7 tests, 0 errors, 0 failures`。

### V1.0 生产标识

Router SHA256 `6ab99de8ceb287d1fdb58b66399874c60b41734775a74a0c2745bf64223d6bb7`
