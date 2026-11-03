# 测试报告摘要（V1.0）

> 详细分层记录见 `software_copyright/08_软件测试报告.md`；各阶段原始记录见 `docs/milestones/` 与**仓库根** `evidence/`（交付版目录内不再重复保存）。

## 1. 单元测试（G1 ARM64，colcon）

| 套件 | 用例数 | 结果 |
|---|---:|---|
| asr_text_normalizer_test | 22 | 通过 |
| action_command_mapper_test | 39 | 通过 |
| interaction_manager_test | 76 | 通过 |
| intent_recognizer_test | 96 | 通过 |
| fact_risk_guard_test | 119 | 通过 |
| knowledge_base_test | 57 | 通过 |
| robot_state_evaluator_test | 44 | 通过 |
| **合计** | **453** | `7 tests, 0 errors, 0 failures, 0 skipped` |

## 2. 隔离验证（ROS_DOMAIN_ID=77 + Mock）

| 类别 | 覆盖 | 结果 |
|---|---|---|
| ASR 归一化 | 脑急/脑筋→脑机、无语境不纠正、控制词保护 | 通过 |
| 意图边界 | 状态/知识/实验室/动作口令/能力询问 | 通过 |
| 事实保护 | PRICE 拦截、无资料兜底、普通知识放行 | 通过 |
| 动作闭环 | success / failed / 无响应超时 / 禁用 / 去重 | 通过 |
| 调度 | 队列满时忙碌提示在已接受请求之后 | 通过 |
| 停机 | 空闲 / HTTP 中 / 动作等待中 SIGINT | 0.29s / 0.79s / 0.29s |

## 3. 真机验证

| 项目 | 结果 |
|---|---|
| 语音闭环（P0-16） | 通过；GREETING 挥手 4/4 |
| 非动作回归（P0-20.1） | 5 项通过，动作请求 0 |
| wave（P0-20.1） | `success:wave:26`，现场确认真实挥手，请求 1 次 |
| 生产 6 项（P0-21） | 实验室介绍/电量/IMU/整体状态/动作能力 正确；脑机接口因 ASR 变体答错，已补别名规则 |
| ASR001 专项（P0-21 补充） | 静态校验通过；真机本轮 ASR 输出"手机接口"，**未触发该分支**，如实记录 |
| 整机重启恢复 | 5 服务 45s 内自动恢复 |

## 4. 未验证项（不得视为通过）

- `wave_low` / `hands_up` / `clap` / `heart` / `victory` 仅白名单登记，**未逐项真机验收**。
- TTS 真实播放完成时刻（无可靠事件）。
- 并发动作（协议不支持）。
