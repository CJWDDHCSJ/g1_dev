# Mock 隔离测试（DOMAIN_ID=77）

## 1. 为什么要隔离验证

真机验证成本高且涉及物理安全：不能为了测试“价格拦截”就真的让机器人说话，也不能为了测试“动作超时”就真的发动作。隔离域用 Mock 替身验证**业务分支**，真机只验证**物理链路**。

隔离条件：

- `ROS_DOMAIN_ID=77`（生产为 0）；
- 不启动 TTS Node / Action Executor / ASR Bridge；
- Mock Ollama（HTTP）、Mock Action Executor、必要时 Mock RobotStateProvider；
- 录制 `/g1/tts`、`/g1/action_request`、`/g1/action_result`。

## 2. 已验证场景

| 场景 | 输入/条件 | 期望 | 实测结果 |
|---|---|---|---|
| 别名归一化 | “脑急接口是什么” | 归一化为“脑机接口”，Mock 收到正确文本 | 通过（Mock 日志可见 `脑机接口是什么`） |
| 状态路由 | “你现在的IMU数据接收情况怎么样” | ROBOT_STATUS → 本地 Provider | 通过 |
| 知识问法 | “IMU 是什么 / 接收原理是什么” | GENERAL_QA → Mock Ollama | 通过（Mock 计数增加） |
| 价格拦截 | “宇树 G1 多少钱” | PRICE 拦截，Mock 计数不变 | 通过 |
| 动作成功 | “挥个手” + Mock success | 播报“好的，挥手动作完成了。” | 通过 |
| 动作失败 | “拍个手” + Mock failed | 播报“这个动作暂时没有执行成功。” | 通过 |
| 动作超时 | “举手” + Mock 不回复 | 超时后播报“未收到结果，不会自动重复执行。” | 通过（6s 测试超时） |
| 永久禁用 | “把手放在心口” | 不发送动作请求 | 通过 |
| 去重 | 200ms 内两次“比个心” | 只发送 1 次动作请求 | 通过 |
| 队列满顺序 | 慢 Ollama + 6 条并发 | 4 条已接受回复后才播报忙碌提示 | 通过 |
| 停机 | 空闲 / HTTP 中 / 动作等待中 SIGINT | 快速退出，无死锁 | 0.29s / 0.79s / 0.29s |

## 3. 常用 Mock 结构（历史脚本形态）

```python
# Mock Ollama：记录请求文本与次数，可模拟 normal / delay8 / error / badjson
# Mock Action Executor：按模式返回 success / failed / 不回复；记录收到的动作名
# Mock RobotStateProvider：周期性发布一份 VALID 的 aggregate JSON
```

脚本未随仓库分发（含本机路径）；需要时按上表行为重写即可，接口只有三个字符串话题 + 一个 HTTP 端点。

## 4. 纪律

- **Mock 通过 ≠ 真机通过**。报告中必须分别标注。
- Mock 只能证明业务分支与顺序，不能证明麦克风、扬声器、机械臂是否真实工作。
- 隔离测试结束后按 PID 精确清理，不用 `pkill -f`，避免误杀生产节点。
