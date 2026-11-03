# P0-17.1 FactRiskGuard 实时状态问答漏放修复 · 交付报告

- 日期：2026-11-01
- 项目：《基于 ROS 2 的 G1-EDU 校园实验室智能接待与讲解系统 V1.0》
- 范围：修复状态问法漏放 + ROBOT_STATUS 本地兜底分支。不开发 RobotStateProvider，不读取新硬件话题。
- 证据目录：`/home/mick/Mick_Grey/g1_dev/teach/g1_kb_work/evidence_p0171/`

## A. 修改前问题分析（基于 P0-17 审计实测）

四个漏放问法在修改前的真实行为（探针+源码确认）：

| 问法 | IntentRecognizer | FactRiskGuard（旧） | Voice Router 实际路径 | 风险 |
|---|---|---|---|---|
| 你现在运行状态怎么样 | ROBOT_STATUS | **NORMAL**（语境词表缺"你"） | 进 Ollama 队列 | **模型编造运行状态** |
| 你的 IMU 数据正常吗 | ROBOT_STATUS | **NORMAL**（无 IMU 关键词） | 进 Ollama 队列 | **模型编造 IMU 状态** |
| 你现在正在执行什么动作 | GENERAL_QA | **NORMAL**（无动作状态规则） | 进 Ollama 队列 | **模型编造动作状态** |
| 动作做完了吗 | GENERAL_QA | **NORMAL** | 进 Ollama 队列 | **模型编造完成状态** |

真实执行顺序（修改前后一致，以源码为准）：ASR → IntentRecognizer →（GREETING 固定匹配 → LAB_INTRO → ROBOT_INTRO → GOODBYE 本地分支）→ **FactRiskGuard** →（拦截则兜底 / NORMAL 则排队）→ worker_loop → query_ollama → /g1/tts。ROBOT_STATUS 此前**无独立分支**，全部落入 Guard→Ollama 路径。

## B. 修改方案

### B.1 FactRiskGuard 规则完善（`src/fact_risk_guard.cpp`）

未新增风险枚举（REALTIME_STATUS 足以表达），公共接口不变。判定顺序调整为：

```text
1. PRICE 关键词（不变）
2. 动作执行状态：kActionStatusMarkers ∩ kActionContextTokens
   （做完了吗/完成了吗/执行完/正在执行/在做什么/执行的是什么/执行什么/什么动作
    + 动作/执行/挥手/比心/举手/拍手/机器人/手臂/腿/你/您/g1/宇树）→ REALTIME_STATUS
   ※ 置于知识形式判断之前，因为"你当前执行的是什么动作"含"是什么"
3. 知识形式豁免：kKnowledgeMarkers（是什么/什么是/什么意思/如何/
   怎么用/怎么实现/怎么估计/怎么判断/怎么计算/为什么/原理/的定义/指什么/属于什么）
   → NORMAL（普通概念/原理问题），两个豁免：
   a) 含实验室实体词 → 继续走 LAB_FACT（"实验室设备是什么型号"）
   b) 含"什么状态" → 继续走状态规则（"机器人目前是什么状态"）
4. 传感器词（原表 + imu/惯导/陀螺仪/姿态数据）→ REALTIME_STATUS
5. 泛化状态词 + 机器人语境（语境词表补充"你/您"；状态词补充"什么状态"）→ REALTIME_STATUS
6. LAB_FACT（原逻辑不变）
7. NORMAL
```

**为什么不用简单全局关键词拦截**："机器人如何判断自己的运行状态"含"机器人+运行状态"，"动作规划是什么"含"动作"，裸关键词会把普通技术问题错误拦截。本方案用"知识问题形式豁免（如何/是什么等）+ 标志词与语境词共现"的组合判定，并以 8.2 负向测试矩阵锁死。
（开发中实际修过一次过度匹配：初版知识标志词含裸"怎么"，误伤"状态怎么样"，已收紧为"怎么用/怎么实现"等具体搭配，由负向测试捕获。）

### B.2 Voice Router 新增 ROBOT_STATUS 本地分支（`src/voice_router.cpp`）

位置：`enqueue_user_text()` 内、GOODBYE 分支之后、FactRiskGuard 之前。命中 ROBOT_STATUS 意图时：

- 直接 `publish_reply("我目前还没有接入完整的实时机器人状态数据，暂时无法准确判断这个状态，请以实际设备状态为准。")`；
- 不进 Ollama 队列、不发 HTTP、不等超时、不触发动作；
- Guard 的 REALTIME_STATUS 规则**完整保留**，作为 GENERAL_QA 误分类状态问题的第二道防线。

### B.3 修改文件清单与 SHA256（双端一致）

| 文件 | 修改 | SHA256 |
|---|---|---|
| `src/fact_risk_guard.cpp` | 规则完善 | `39b567b45b6240c3ba9919589f5d88078b50c4ca9896243951a2553aea859d83` |
| `src/voice_router.cpp` | ROBOT_STATUS 分支 | `c3d15dfdf96725b8122659be03d378c07ab61880c076e14ad4b0cc894dcc2f04` |
| `test/fact_risk_guard_test.cpp` | +48 断言 | `742c5c0d226b7f3bf3fdf5927b94f65cd31bc4dc8822fdbb211ef4a5e17dcdd4` |
| `test/intent_recognizer_test.cpp` | +3 用例 | `0b8950ae26120369bb246d279c04e16883349ce6eba5148bb4bdda3e39472e12` |
| `include/`、`CMakeLists.txt`、`package.xml`、`knowledge_base.*`、`intent_recognizer.cpp` | **未修改** | 与 P0-15 版本一致 |

## C. 测试结果（实际计数）

| 测试 | 本地 x86 | G1 ARM64 直跑 | colcon test-result |
|---|---|---|---|
| FactRiskGuard（原 71 + 新 48 断言） | **119/119** | **119/119** | 3 tests, 0 errors, 0 failures |
| KnowledgeBase 回归 | 57/57 | **57/57** | 同上 |
| IntentRecognizer（原 44 + 新 3 用例） | 47/47 | **47/47** | 同上 |

- G1 ARM64 `colcon build --packages-select g1_voice_router --symlink-install`：通过（31.1s）。
- 新增正向覆盖任务书 8.1 全部 10 项（Guard 独立判定均为 REALTIME_STATUS/blocked）。
- 新增负向覆盖任务书 8.2 全部 8 项（含"机器人如何判断自己的运行状态"、"动作规划是什么"、"IMU 是什么"等，Guard 均为 NORMAL）。
- 原有 PRICE/LAB_FACT/NORMAL 用例与 8.3 回归清单无回归。

## D. Ollama 调用证据（DOMAIN_ID=77 + 本地 HTTP Mock 实测）

Mock：G1 本机 127.0.0.1:11435，每收到一次 `/api/chat` 请求计数 +1 并记录最后一条用户消息。测试路由器以 `-p ollama_url:=http://127.0.0.1:11435/api/chat` 启动。

| 输入 | IntentRecognizer | Guard | Mock 计数 | /g1/tts 输出 |
|---|---|---|---:|---|
| 你现在运行状态怎么样 | ROBOT_STATUS | 未经过（本地分支拦截） | 0 | ROBOT_STATUS 本地兜底话术 ✅ |
| 你的 IMU 数据正常吗 | ROBOT_STATUS | 未经过（本地分支拦截） | 0 | 本地兜底话术 ✅ |
| 你现在正在执行什么动作 | GENERAL_QA | REALTIME_STATUS 拦截 | 0 | Guard 兜底话术 ✅ |
| 动作做完了吗 | GENERAL_QA | REALTIME_STATUS 拦截 | 0 | Guard 兜底话术 ✅ |
| 你现在有多少电量 | ROBOT_STATUS | 未经过（本地分支拦截） | 0 | 本地兜底话术 ✅ |
| IMU 是什么 | GENERAL_QA | NORMAL | **1** | MOCK_OLLAMA_REPLY_1 ✅ |
| 机器人如何判断自己的运行状态 | **ROBOT_STATUS** | 未经过（本地分支拦截） | 0 | 本地兜底话术 ⚠ 见说明 |
| 脑机接口是什么 | GENERAL_QA | NORMAL | **2** | MOCK_OLLAMA_REPLY_2 ✅ |
| 宇树 G1 多少钱 | GENERAL_QA | PRICE 拦截 | 0 | 价格兜底话术 ✅ |

**Mock 最终计数 = 2**，与预期完全一致（仅两条纯知识问题）；无任何意外 Ollama 调用；未触发动作。

⚠ 说明（如实记录）："机器人如何判断自己的运行状态"在 **Guard 层为 NORMAL**（单元测试 119 项中验证通过，负向矩阵达标），但 IntentRecognizer 将其归为 ROBOT_STATUS，因此 Router 层走本地兜底而非 Ollama。这是双层设计的保守取向：宁可少答一次知识，不放行任何状态问题。该具体问法无法获得知识型回答属已知取舍，记录在遗留事项。

## E. 安全检查

- 生产域 DOMAIN_ID=0 未启动任何测试节点（测试路由器与 Mock 均在 DOMAIN_ID=77）。
- 未终止/重启任何生产节点；测试结束按精确 PID `kill -INT 12602`、`kill 12600` 停止隔离域进程，Domain 77 复查无残留节点。
- 未触发机器人动作；未修改 Action Executor 与白名单。
- Mock 用 `pkill -f p0171_mock.py` 结束（模式唯一指向本阶段临时脚本，未波及其他进程），G1 /tmp 临时文件已清理。
- 历史备份（P0-15/P0-17.1 之前各版本）未被覆盖。
- 生产节点验证：asr 8247 / tts 8248 / router 8270 / executor 8880 全部仍在运行。

## F. 部署状态（严格区分）

| 项目 | 状态 |
|---|---|
| 源码已修改并同步 G1 | ✅（SHA256 双端一致） |
| install 产物已更新 | ✅（symlink-install，build 二进制 2026-11-01 重新编译） |
| 新版本完成隔离域运行验证 | ✅（DOMAIN_ID=77 + Mock，见 D 节） |
| **当前生产 Voice Router（PID 8270）仍为旧版本** | ✅ **是——旧进程未重启，新逻辑未生效于生产** |
| 生产版本切换 | ❌ 待单独安排（建议低峰期 `kill -INT 8270` 后重启，新话题接口无变化，切换风险低） |

## G. 备份与证据

- G1 备份：`/home/unitree/g1_backups/g1_voice_router_20261101_074728_before_p0171/`（完整包副本）
- 本地备份：`/home/mick/Mick_Grey/g1_dev/teach/g1_kb_work/backups/g1_voice_router_20261101_before_p0171_local/`
- 运行证据：`evidence_p0171/p0171_router.log`（9 条输入的意图/Guard/路由日志）、`evidence_p0171/p0171_mock.log`（Mock 请求全文，仅 2 条知识请求）
- 本地工作副本：`/home/mick/Mick_Grey/g1_dev/teach/g1_kb_work/g1_voice_router/`（与 G1 同步）

## H. 遗留事项

1. RobotStateProvider 未开发（P0-18 范围）：电量、温度、IMU、动作状态目前一律返回"暂不可用"兜底，无任何编造。
2. "机器人如何判断自己的运行状态"被 IntentRecognizer 归为 ROBOT_STATUS，走本地兜底（保守取舍）；如需恢复其知识回答，需在 IntentRecognizer 层细化（涉及已验证模块，须单独评估）。
3. "实验室当前状态怎么样"此前即被 IntentRecognizer 误归 ROBOT_STATUS，现在会得到机器人状态兜底话术而非 Ollama 回答（更安全，体验略变，如实记录）。
4. 生产 Voice Router（PID 8270）仍运行 P0-15 版本，需安排安全切换。
5. 后续还需：SOC 读数稳定性验证、温度/电流单位核验（P0-17 报告 F 节）。

## 变更检查

```text
源码修改：4 个文件（fact_risk_guard.cpp / voice_router.cpp / 两个测试文件，均在授权范围内）
配置修改：0
生产节点重启：0
生产节点终止：0
机器人动作触发：0
历史备份覆盖：0
```
