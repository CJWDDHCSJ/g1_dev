# 单元测试

## 1. 基线（G1 ARM64 实测，2026-11-03 复跑）

| 套件 | 断言数 | 覆盖重点 |
|---|---:|---|
| `asr_text_normalizer_test` | 22 | 别名纠正、上下文约束、控制词保护、数字不变、原文保留 |
| `action_command_mapper_test` | 39 | 6 动作多表达、否定、疑问、能力询问、永久禁用 ID 33、出口白名单校验 |
| `interaction_manager_test` | 76 | 过滤、去重窗口、有界 FIFO、状态转移、去重缓存有界 |
| `intent_recognizer_test` | 96 | 7 类意图边界、IMU 状态 vs 知识、动作命令 vs 能力询问 |
| `fact_risk_guard_test` | 119 | PRICE / REALTIME_STATUS / LAB_FACT / NORMAL、防过度拦截 |
| `knowledge_base_test` | 57 | 实体检索、FAQ 同分、诊断字段、兜底 |
| `robot_state_evaluator_test` | 44 | 四态判定、回答生成、不夸大健康 |
| **合计** | **453** | — |

## 2. 运行方式

```bash
source /opt/ros/humble/setup.bash
source ~/unitree_ros2/setup.sh

cd ~/unitree_ros2/example
colcon test --packages-select g1_voice_router
colcon test-result --verbose
# Summary: 7 tests, 0 errors, 0 failures, 0 skipped
```

单套件直跑（便于看断言明细）：

```bash
./build/g1_voice_router/intent_recognizer_test
./build/g1_voice_router/knowledge_base_test g1_voice_router/config/lab_knowledge.json
./build/g1_voice_router/fact_risk_guard_test g1_voice_router/config/lab_knowledge.json
```

## 3. 设计特点

- 所有被测模块都是**不依赖 ROS 的纯 C++ 静态库**，可在 x86 开发机上直接编译运行（`g++ -std=c++17 test/xxx_test.cpp src/xxx.cpp`）。
- 测试不连接机器人、不播放语音、不触发动作。
- 语义边界用正负用例成对锁定（例如“IMU 是什么”必须 GENERAL_QA，“IMU 数据还在更新吗”必须 ROBOT_STATUS）。

## 4. 约束

- 不得通过删除断言或放宽预期让测试变绿。
- 语义变更必须同步更新测试并在报告中说明。
- 基线不得低于 453。

## 5. 证据

各阶段测试记录见 `docs/milestones/`（P0-15 ~ P0-21）与仓库根 `evidence/`。
