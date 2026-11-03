# Mock 隔离测试

> 目的：在不触碰真机动作、不依赖真实模型的前提下，验证**端到端消息流**与**拦截逻辑**。
> 环境：`ROS_DOMAIN_ID=77`，与生产（0）完全隔离。

## 1. 为什么要单独做一层 Mock

单元测试证明了函数行为，但证明不了：

- Router 真的把 `/g1/asr_text` 收下并走了正确分支；
- 价格问题真的**没有**发 HTTP；
- 动作分支真的**没有**发 `/g1/action_request`；
- 队列满时提示真的排在已接受请求之后。

这些只能靠“起一套节点 + 假下游”验证，因此引入 Mock Ollama（本地 HTTP 桩，记录收到的请求体）与 Mock 执行器（订阅 `/g1/action_request`，按脚本回结果）。

## 2. 隔离做法

```bash
export ROS_DOMAIN_ID=77
export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
# 单独终端启动 Mock Ollama（记录请求体到文件），Router 用参数指向它
ros2 run g1_voice_router voice_router --ros-args \
  -p ollama_url:=http://127.0.0.1:11434/api/chat
```

要点：

- **Domain 隔离**：77 与生产 0 不互通，生产节点不受影响；
- **停生产 Router 后再测**：同一 Domain 下不可有两个 Router（否则一句话两次处理）；
- 测试结束按 PID 精确清理，并用 `ros2 node list` 复查域内为空。

## 3. 端到端用例（P0-21 §5.1，8 条）

| # | 输入 | 归一化 / 意图 | 期望路径 | 结果 |
|---|---|---|---|---|
| 1 | 脑急接口是什么 | ASR001 → 脑机接口 / `GENERAL_QA` | Mock Ollama 收到“脑机接口是什么” | 通过 |
| 2 | 你现在的IMU数据接收情况怎么样 | `ROBOT_STATUS` | 本地 Provider，0 次 HTTP | 通过 |
| 3 | IMU是什么 | `GENERAL_QA` | Mock Ollama | 通过 |
| 4 | IMU数据接收原理是什么 | `GENERAL_QA` | Mock Ollama（不被状态规则抢走） | 通过 |
| 5 | 你的IMU是否完全健康 | `ROBOT_STATUS` | 本地，明确“未完成健康诊断” | 通过 |
| 6 | 语数之一多少钱 | ASR002 → 宇树G1 / `PRICE` | 拦截，0 次 HTTP | 通过 |
| 7 | 语数之一这个说法是什么意思 | 跳过归一化（无语境）/ `GENERAL_QA` | Mock Ollama | 通过 |
| 8 | 不要挥手 | 控制词保护 / `DEMO_ACTION` | 本地拒绝，0 次动作请求 | 通过 |

统计：Mock Ollama 收到 4 条请求，文本与预期完全一致；`/g1/action_request` 计数 0。

## 4. 动作分支的 Mock 用例（P0-20）

| 场景 | Mock 执行器行为 | Router 期望 |
|---|---|---|
| 成功 | 回 `accepted:wave:26` → `success:wave:26` | 播报“挥手动作完成了” |
| 失败 | 回 `failed:wave:26:rpc_error=1` | 播报“没有执行成功” |
| 拒绝 | 回 `rejected: arm is holding...` | 播报“当前状态不允许” |
| 超时 | 只回 `accepted:`，不回终态 | 45 s 后播报“未收到结果，不会自动重复执行” |
| 禁用 | 输入“把手放在心口” | `DISABLED`，0 次请求 |
| 去重 | 600 ms 内重复同一句 | 第二次 `DUPLICATE`，只播报一次 |
| 队列满 | 连发 5 条 | 第 5 条 `REJECTED (QUEUE_FULL)`，忙碌提示最后播报 |

> 这些都是 Mock 结论，**不能**当作真机动作验收。真机只验证过 `wave`。

## 5. 停机与中断

| 场景 | 期望 | 实测 |
|---|---|---|
| 空闲时 SIGINT | 立即退出 | 0.29 s |
| 等 Ollama 时 SIGINT | 中断 HTTP 并退出 | 0.79 s |
| 等动作结果时 SIGINT | 唤醒等待并退出 | 0.29 s |

## 6. 与真机测试的边界

| 能力 | Mock 能证明 | Mock 不能证明 |
|---|---|---|
| 话题连通与分支正确 | 是 | — |
| 拦截生效（0 次 HTTP / 0 次动作） | 是 | — |
| 顺序与去重 | 是 | — |
| 模型真实时延 | 否（Mock 立即返回） | 需真机 |
| 动作真实执行 | 否 | 需真机 + 现场确认 |
| ASR 真实识别 | 否 | 需真机 |

> 特别提示：Mock 的“毫秒级返回”不代表真实模型延迟；真机实测 5.5–7.0 s，不要混用。
