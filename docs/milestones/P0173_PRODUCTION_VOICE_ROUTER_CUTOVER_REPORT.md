# P0-17.3 G1 生产环境 Voice Router 安全切换 · 交付报告

- 日期：2026-11-01
- 结论：**P0-17.1 + P0-17.2 新版本已成功切换到 G1 生产环境（DOMAIN_ID 0），真实语音验收通过，其他生产节点零影响，旧版本可回滚。**

## A. 切换前环境状态

- 主机 `ubuntu` / aarch64 / Ubuntu 22.04.5；ROS humble；RMW=rmw_cyclonedds_cpp；CYCLONEDDS_URI 绑定 eth0；ROS_DOMAIN_ID 未显式设置（默认 0）。
- 生产节点（切换前实测，非历史记录）：

| 节点 | 包装进程 | 节点 PID | 运行时长 |
|---|---|---|---|
| ASR Bridge | 8243 | 8247 | 1:19:26 |
| Voice Router | 8244 | **8270** | 1:19:26 |
| TTS Node | 8245 | 8248 | 1:19:26 |
| Action Executor | 8878 | 8880 | 1:11:31 |
| Unitree ros_bridge | —（原生 bare-DDS 应用，非 Linux 进程） | /ros_bridge 节点在位 | — |

- 旧 Voice Router 启动方式：`nohup ros2 run g1_voice_router voice_router`（父进程 8244 = `ros2 run` 包装；**无 systemd / launch / respawn**），工作目录 `/home/unitree`，可独立停止。
- **旧运行版本身份**：`/proc/8270/exe → build/g1_voice_router/voice_router (deleted)`，提取哈希 `e21c5079fff6a33e49737891b22cccc5610e47dac13d420884c956d6ec2d53f9`——运行中确为旧映像（磁盘文件已被 P0-17.1/17.2 编译覆盖），环境白名单：ROS_DISTRO=humble、RMW=cyclonedds、ROS_LOCALHOST_ONLY=0。
- 业务空闲：最后一轮交互（Ollama 请求+回复）已完成，无队列堆积。

## B. 新版本完整性

- 5 个关键源码 SHA256 与 P0-17.1/P0-17.2 交付报告**逐一致对通过**（fact_risk_guard.cpp `39b567b4…`、voice_router.cpp `c3d15dfdf…`、intent_recognizer.cpp `b05a3cc3…`、两个测试文件 `742c5c0d…`/`15596eb0…`）。
- 切换前重新 `colcon build --symlink-install`（增量 0.37s）+ colcon test：**Intent 75/75、Guard 119/119、KB 57/57；colcon 3 tests, 0 errors, 0 failures**。
- 新二进制：`build/g1_voice_router/voice_router`，SHA256 `e8a7e549d19892b0b2d4a91453d0c29fd3cfb90321cf1f61ba4f5af0945be531`（2026-11-01 08:11 编译）。
- 功能确认：ROBOT_STATUS 本地兜底（P0-17.1）、Guard 状态/知识边界（P0-17.1/17.2）、Intent 语义边界（P0-17.2）、PRICE/LAB_FACT 保留。

## C. 备份与回滚方案

- 备份目录：`/home/unitree/g1_backups/g1_voice_router_20261101_081200_before_p0173/`
  - `voice_router_old_production`（4.55MB ELF，**取自 /proc/8270/exe 的真实运行映像**，SHA256 `e21c5079…`）
  - `old_launch_info.txt`（cmdline/exe/cwd/父进程/启动方式）、`old_environment.txt`（5 个白名单变量，无敏感信息）、`old_process_info.txt`（四节点进程快照）
  - `lab_knowledge.json.installed` / `.source`（配置双份）
- **可执行回滚命令**（旧二进制在同一 ament 环境下运行，节点名相同）：

```bash
NEWPID=$(pgrep -f 'install/g1_voice_router/lib/g1_voice_router/voice_router')
kill -INT "$NEWPID"; sleep 4
bash -lc 'source ~/unitree_ros2/setup.sh; source ~/unitree_ros2/example/install/setup.bash; \
  nohup ~/g1_backups/g1_voice_router_20261101_081200_before_p0173/voice_router_old_production \
  > ~/g1_logs_p0173/rollback.log 2>&1 & echo ROLLBACK_PID=$!'
# 验证：ros2 node list 仅一个 /g1_voice_router；其余节点 PID 不变
```

## D. 生产切换记录

| 项 | 值 |
|---|---|
| 旧 PID / 退出方式 | 8270，`kill -INT` 正常退出（OLD_EXITED，包装进程 8244 同步退出） |
| 新包装进程 / 新节点 PID | 15783 / **15789** |
| 新进程实际加载 | `/proc/15789/exe → build/g1_voice_router/voice_router`，SHA256 **`e8a7e549…` = 新版本** ✅ |
| 启动命令 | `nohup ros2 run g1_voice_router voice_router`（与原参数一致，无 Mock 地址残留） |
| 启动时间 | 2026-11-01 08:24 UTC（epoch 1793521473） |
| ROS 注册 | Domain 0 仅一个 `/g1_voice_router`；`/g1/asr_text` pub1/sub1、`/g1/tts`、`/g1/action_request`、`/g1/action_result` 均正常 |
| 日志 | `~/g1_logs_p0173/router.log`（KB 加载 FAQ=21，Ollama URL 正确） |

## E. 其他生产节点状态

| 节点 | 切换前 PID | 切换后 PID | 保持 |
|---|---:|---:|---|
| ASR Bridge | 8247 | 8247（运行时长连续 1:24:30） | ✅ |
| Voice Router | 8270 | **15789** | 本次切换 ✅ |
| TTS Node | 8248 | 8248（1:32:12 连续） | ✅ |
| Action Executor | 8880 | 8880（1:16:35 连续） | ✅ |
| ros_bridge（/ros_bridge） | 原生应用 | 在位 | ✅ |

无重复 Voice Router；无其他 PID 意外变化。

## F. 真实语音验收结果（真实麦克风，DOMAIN_ID 0）

| # | 用户语音 | ASR 识别 | 意图 | Guard | 最终路径 | Ollama | 播报 |
|---|---|---|---|---|---|---|---|
| 1 | 你现在运行状态怎么样 | **"你现在运行状态怎"**（ASR 截断"么样"） | **ROBOT_STATUS** ✅ | 未经过（本地分支） | 本地兜底 | ❌ 未调用 | ✅ 135B，"尚未接入完整实时状态…" |
| 2 | 机器人如何判断自己的运行状态 | 完整正确 | **GENERAL_QA** ✅（核心修复①） | **NORMAL**（knowledge question form）✅ | Ollama | ✅ 调用（4.8s，276B） | ✅ 播报技术原理 |
| 3 | 实验室当前状态怎么样 | 完整正确 | **LAB_INTRO** ✅（核心修复②） | LAB_FACT 路径 | KnowledgeBase found=false → 本地兜底 | ❌ 未调用 | ✅ 162B，"没有公开资料…可问我研究方向/重点实验室…" |
| 4 | 脑机接口是什么 | **误识为"手机接口是什么"** | GENERAL_QA（对误识文本路由正确） | NORMAL | Ollama | ✅ 调用（244B） | ✅ 播报（Ollama 回答了手机接口知识） |
| 5 | 宇树 G1 多少钱 | **误识为"语速机多少钱"** | GENERAL_QA | **PRICE 拦截** ✅ | 本地兜底 | ❌ 未调用 | ✅ 87B 价格兜底话术 |

- 每条回复均经 TTS 节点确认实际播报：`Speaking sentence … TtsMaker returned ret=0`（证据见 tts 日志摘录）。
- **ASR 误识记录（9.6 要求）**：第 4 句"脑机"→"手机"、第 5 句"宇树G1"→"语速机"，均为原生 ASR 识别偏差，非 Voice Router 错误；两条误识文本的路由行为均正确（知识进 Ollama、价格被拦截）。核心验收点"技术原理问题正常调用 Ollama"已由第 2 句完整正确文本直接证明。
- ASR 截断鲁棒性：第 1 句缺尾字仍正确进入 ROBOT_STATUS 本地兜底，0 次 Ollama 调用。
- 未触发任何机器人动作（本阶段不测动作；Action Executor PID 未变，P0-16 已完成挥手闭环）。

## G. 安全与回滚情况

- 旧 Voice Router：已精确停止（SIGINT 正常退出）✅；新 Voice Router：已启动并验证 ✅。
- **未发生回滚**；无节点重复；无其他生产 PID 意外变化；无机器人动作；未修改任何其他模块。
- 旧版本可恢复：备份二进制 + 启动参数 + 环境齐备（见 C 节回滚命令）。

## H. 最终状态（严格区分）

| 层面 | 状态 |
|---|---|
| 源码 | P0-17.1/17.2 全部在 G1（哈希核对一致），本轮零代码修改 |
| install 产物 | 已包含新版本（symlink → build，e8a7e549…） |
| **新进程实际加载状态** | **已确认 PID 15789 加载新二进制**（/proc/exe 哈希验证） |
| ROS 节点运行状态 | Domain 0 单实例 /g1_voice_router 正常运行 |
| 真实麦克风验收 | ✅ 通过（5 项，含 2 项 ASR 误识如实记录） |
| 真实扬声器验收 | ✅ 通过（TtsMaker ret=0 全部确认，用户现场听取） |

**生产切换判定：通过**（验收标准 1-15 全部满足，其中第 9 项以第 2 句完整识别文本直接证明、第 4 句间接佐证）。

## 变更检查

```text
源码修改：0
配置修改：0
生产节点重启：仅 Voice Router（本阶段任务目标），其余 3 节点 + ros_bridge 未动
机器人动作触发：0
历史备份覆盖：0
```

## 遗留事项

1. RobotStateProvider 未开发：实时状态问题继续返回"尚未接入"兜底（本轮真机已验证该兜底生效）。
2. 原生 ASR 对短句/专有名词识别波动（"脑机→手机"、"宇树G1→语速机"）属 ASR 层，不在本阶段修复范围；演示时建议使用清晰完整语句。
3. "实验室当前状态怎么样"当前知识库无实时运行状态资料，回答资料不足兜底——符合事实红线设计；如需更好体验，可在知识库补充固定说明类 FAQ（需走事实核验流程）。
4. 后续待办不变：RobotStateProvider 开发、SOC 稳定性与单位核验、软著材料。
