# 贡献指南

## 1. 语言与编码规范

- **C++17**，遵循仓库现有风格：4 空格缩进、`namespace g1_voice_router`、头文件用 `G1_VOICE_ROUTER_XXX_HPP_` 保护宏、成员变量后缀 `_`。
- 文件命名：`snake_case.hpp` / `snake_case.cpp`；测试统一 `*_test.cpp`。
- 注释写"为什么"，不写"是什么"；不提交被注释掉的旧代码。
- 机器人端默认 C++；Python 仅用于离线脚本、Mock 与验证工具，不进入生产链路。

## 2. 架构强制约束

以下为**硬性要求**，评审不通过即拒绝合并：

1. **不得绕过 `InteractionManager`**：任何 `/g1/tts` 发布都必须来自唯一 worker 线程，不允许在 ROS 回调里直接发布回复。
2. **不得绕过 `FactRiskGuard`**：不得删除或弱化 PRICE / REALTIME_STATUS / LAB_FACT 规则；如需调整，必须补充正向与负向测试。
3. **不得新增未经真机验证的动作 ID**；`hand_on_heart` / ID 33 **永久禁用**，不得以别名重新映射。
4. **不得让大模型决定动作**：动作必须来自 `ActionCommandMapper` 的固定白名单。
5. **不得引入无界队列**；新增异步等待必须有明确超时与失败路径。
6. **不得让实时状态问题进入 Ollama**：状态类回答必须来自 `RobotStateProvider` 或安全兜底。
7. **不得使用模糊匹配/编辑距离纠正 ASR 文本**：只允许有真实日志证据的明确别名，且必须做控制词保护。

## 3. 测试要求

- 每个新模块必须有独立单元测试，且不依赖 ROS 运行时。
- 提交前必须本地（x86）跑通对应套件；生产变更必须在 G1 ARM64 上 `colcon test` 通过。
- 基线不得下降：ASRTextNormalizer 22 / ActionCommandMapper 39 / InteractionManager 76 / IntentRecognizer 96 / FactRiskGuard 119 / KnowledgeBase 57 / RobotStateEvaluator 44（合计 453）。
- **不得通过删除断言、放宽预期来让测试通过**。若语义确需变更，必须在报告里说明原因并同步更新测试。
- Mock 测试与真机测试必须分开报告，不得把 Mock 结果写成真机验证。

## 4. 变更流程

1. 先读真实源码，不凭历史报告猜接口。
2. 修改前备份（G1：`~/g1_backups/g1_voice_router_<时间戳>_before_<阶段>/`）。
3. 本地编译测试 → 同步 G1 → ARM64 编译测试 → `ROS_DOMAIN_ID=77` 隔离验证。
4. 生产部署**只允许重启单个** `g1-voice-router.service`；禁止重启 `g1-voice-stack.target`。
5. 记录修改文件、原因、SHA256、测试计数与证据路径。

## 5. 安全红线

- 不得修改 Unitree SDK、DDS 配置、JetPack/CUDA/TensorRT、Ollama 服务。
- 不得在未确认现场安全（周围净空、有人可停机）的情况下触发真实动作。
- 不得把生产日志、真实录音、内网地址、密钥提交入库。
- 不得为项目擅自添加开源许可证。

## 6. 提交信息格式（建议）

```text
<scope>: <简要说明>

- 动机：
- 变更：
- 测试：<套件名与计数>
- 风险：
```

`scope` 取值：`router` / `intent` / `guard` / `kb` / `state` / `action` / `asr` / `deploy` / `docs`。
