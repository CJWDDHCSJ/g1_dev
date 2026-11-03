# P0-18 证据索引（DOMAIN_ID=77 隔离域验证）

采集日期：2026-11-02（G1 重启后重新执行）。全部数据来自隔离域 77，未使用生产域 0。

| 文件 | 内容 |
|---|---|
| `test_result.txt` | G1 colcon test 与四个测试套件直跑计数（RS 44/44、Intent 75/75、Guard 119/119、KB 57/57） |
| `state_stale.json` | 全部数据源停止发布后的 `/g1/robot_state`（MESSAGE_TIMEOUT） |
| `state_A.json` | 场景 A：BMS 断流、LowState+Action 正常 → battery STALE、imu/action VALID |
| `state_B.json` | 场景 B：LowState 断流、BMS+SportMode 正常 → imu/action STALE、battery VALID |
| `state_C.json` | 场景 C：Action 断流、BMS+LowState 正常 → action STALE、battery/imu VALID |
| `p018_provider.log` | Provider 启动日志 |
| `p018_router.log` | 测试 Voice Router 完整交互日志（意图/Guard/分支） |
| `p018_mock_ollama.log` | Ollama Mock HTTP 请求全文（总计数 3） |
| `mock_state_selective.py` | 支持按话题选择性发布的状态 Mock 脚本（G1 端持久化副本在 `~/g1_logs_p018/`） |

## 关键实测值

- 超时配置（代码常量）：/lowstate 1000ms、/sportmodestate 1000ms、/arm/action/state 1000ms、/lf/bmsstate 2000ms；Provider 发布 2Hz；Router 缓存有效期 5000ms。
- 场景 A：`battery=STALE(age 2960837ms)`，`imu/action/joints=VALID(35ms)`。
- 场景 B：`battery=VALID(56ms)`，`imu/action=STALE(9340ms)`，`runtime=VALID`（sportmode 仍在发布）。
- 场景 C：`battery=VALID(11ms)`，`imu=VALID(9ms)`，`action=STALE(15341ms)`。
- Mock Ollama：仅 3 次请求（IMU是什么 / 机器人如何判断自己的运行状态 / 脑机接口是什么），ROBOT_STATUS、LAB_INTRO、PRICE 均 0 次。
