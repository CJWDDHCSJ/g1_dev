# 代码结构

> 依据：实际文件树与 `CMakeLists.txt` 构建目标。行数为归档时的实际统计。

## 1. 包与构建目标

| 工作区 | 包 | 构建目标 | 说明 |
|---|---|---|---|
| `~/unitree_ros2/example` | `g1_voice_router` | `voice_router`（可执行） | 业务主节点 |
| 同上 | 同上 | `robot_state_provider`（可执行） | 状态 Provider 节点 |
| 同上 | 同上 | `robot_state_evaluator`（静态库） | 纯逻辑，供 Router 与测试共用 |
| 同上 | 同上 | `interaction_manager`（静态库，含 4 个源文件） | 队列/过滤/动作映射/归一化 |
| 同上 | 同上 | `knowledge_base`（静态库） | 知识库 |
| 同上 | 同上 | `fact_risk_guard`（静态库） | 风控（依赖 knowledge_base） |
| 同上 | 同上 | 7 个测试可执行 | `BUILD_TESTING` 下注册到 CTest |
| `~/unitree_ros2/example` | `g1_asr_bridge` | `asr_json_bridge` | ASR JSON → 文本 |
| `~/lidar_ws` | `g1_action_executor` | `g1_action_executor_node` | 独立工作区 |
| `~/unitree_ros2/example` | `unitree_ros2_example` | `g1_tts_node` | 放在官方示例包内编译 |

依赖链（静态库）：

```text
voice_router
 ├── fact_risk_guard ── knowledge_base
 ├── interaction_manager（interaction_manager.cpp / interaction_filter.cpp /
 │                        action_command_mapper.cpp / asr_text_normalizer.cpp）
 ├── robot_state_evaluator
 └── CURL::libcurl + Threads::Threads

robot_state_provider ── robot_state_evaluator + Threads
```

因此 `voice_router` 链接时已经包含了所有业务逻辑库，路由器源文件只做“编排”。

## 2. `g1_voice_router` 文件清单

### 2.1 头文件（`include/g1_voice_router/`，10 个）

| 文件 | 行数 | 职责 |
|---|---:|---|
| `interaction_types.hpp` | 80 | 状态枚举、请求记录、统计、准入结果 |
| `interaction_manager.hpp` | 109 | 队列接口、状态转移声明、去重参数 |
| `intent_recognizer.hpp` | 47 | 七类意图枚举与识别接口 |
| `fact_risk_guard.hpp` | 71 | 四类风险枚举与评估结果结构 |
| `knowledge_base.hpp` | 54 | 知识库接口与结果结构 |
| `robot_state_types.hpp` | 104 | 四态、字段状态、电池/IMU/运行/动作/关节结构 |
| `robot_state_evaluator.hpp` | 136 | 时效常量、SOC 阈值、评估器与话术接口 |
| `robot_state_provider.hpp` | 62 | Provider 节点与四个回调 |
| `action_command_mapper.hpp` | 52 | 动作映射接口与白名单 ID 常量 |
| `asr_text_normalizer.hpp` | 41 | 归一化结果结构与 `digest()` |

### 2.2 源文件（`src/`，11 个 + 1 个历史副本）

| 文件 | 行数 | 职责 | 是否参与构建 |
|---|---:|---|---|
| `voice_router.cpp` | 1273 | 节点组装、分支编排、HTTP、动作等待、worker | 是 |
| `knowledge_base.cpp` | 1569 | 结构化检索 + FAQ 相似度 + 实体掩码 | 是 |
| `robot_state_evaluator.cpp` | 656 | 四态判定、聚合、JSON 序列化/解析、话术 | 是 |
| `intent_recognizer.cpp` | 510 | 意图规则 | 是 |
| `interaction_manager.cpp` | 307 | 有界队列、去重、状态机、统计 | 是 |
| `fact_risk_guard.cpp` | 307 | 风险分类与兜底 | 是 |
| `interaction_filter.cpp` | 234 | 文本归一化与噪声判定 | 是 |
| `action_command_mapper.cpp` | 203 | 白名单短语匹配与拒绝 | 是 |
| `robot_state_provider.cpp` | 157 | 四路订阅与 2 Hz 发布 | 是 |
| `asr_text_normalizer.cpp` | 145 | 两条别名规则 + 控制词保护 | 是 |
| `robot_state_provider_node.cpp` | 23 | Provider `main()` | 是 |
| `intent_recognizer_test.cpp` | 154 | 历史遗留副本 | **否**（保持原样，清理需单独评估） |

> `src/intent_recognizer_test.cpp` 是早期把测试放在 `src/` 下的遗留文件，`CMakeLists.txt` 未引用它；真正的测试在 `test/intent_recognizer_test.cpp`。归档时保留原样以确保与冻结二进制同源。

### 2.3 测试（`test/`，7 个目标 / 453 项断言）

| 文件 | 断言数 | 覆盖 |
|---|---:|---|
| `asr_text_normalizer_test.cpp` | 22 | 别名、控制词、无语境保留、原文留存 |
| `action_command_mapper_test.cpp` | 39 | 短语、最长匹配、否定/疑问、禁用、歧义 |
| `interaction_manager_test.cpp` | 76 | 过滤/去重/队列/状态机/停机/提示顺序 |
| `intent_recognizer_test.cpp` | 96 | 七类意图与语义边界 |
| `fact_risk_guard_test.cpp` | 119 | 价格/实时状态/实验室事实/豁免 |
| `knowledge_base_test.cpp` | 57 | 命中、未命中、同分兜底 |
| `robot_state_evaluator_test.cpp` | 44 | 四态、时效、SOC 跳变、JSON |

测试运行需要传入知识库路径：

```cmake
add_test(NAME knowledge_base_test  COMMAND knowledge_base_test  "${CMAKE_CURRENT_SOURCE_DIR}/config/lab_knowledge.json")
add_test(NAME fact_risk_guard_test COMMAND fact_risk_guard_test "${CMAKE_CURRENT_SOURCE_DIR}/config/lab_knowledge.json")
```

## 3. 其他包

| 文件 | 行数 | 说明 |
|---|---:|---|
| `g1_asr_bridge/src/asr_json_bridge.cpp` | — | 自带小型 JSON 字符串提取器（含 `\uXXXX`、UTF-16 代理对），不引入 JSON 库；BEST_EFFORT 订阅 |
| `g1_action_executor/src/arm_action_executor.cpp` | 736 | 白名单映射、FSM/模式/手臂检查、RPC、超时、失败锁定 |
| `g1_action_executor/src/main.cpp` | 23 | 节点 `main()` |
| `integrations/unitree_audio/g1_tts_node.cpp` | 453 | 配置加载、分句、节奏估算、有界队列、优雅停机 |

## 4. 编码约定（读代码前应知道）

- 中文注释，注释解释“为什么”，不重复“是什么”；
- 纯逻辑模块（归一化、意图、风控、评估器、映射器）**不依赖 ROS**，因此可以在 x86 上直接单测；
- 时间一律用 `std::chrono::steady_clock` 毫秒注入，便于测试构造；
- 所有对外字符串（话术）集中在各模块内，便于统一改口径；
- 错误路径优先返回“明确兜底”，不抛异常到业务层；`worker_loop` 另有一层 `try/catch` 兜底并标 `FAILED(EXCEPTION)`。
