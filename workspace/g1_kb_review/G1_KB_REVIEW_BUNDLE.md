# G1 KnowledgeBase 工程审查包（主报告）

> 审查对象：宇树 G1-EDU 校园实验室智能接待系统 —— `g1_voice_router` 的 KnowledgeBase 改造与部署
> 生成日期：2026-10-31
> 审查方式：读取本地开发副本、G1 当前部署源码、改造前备份、历史构建/测试/运行日志，并做只读静态比对
> 审查包根目录：`/home/mick/Mick_Grey/teach/g1_kb_review/`

---

## 0. 审查结论摘要

**一句话结论：知识库已真实接入 Voice Router 并通过 37 + 25 条单元测试，构建/测试退出码均为 0；但真实 ASR、TTS 播放、动作执行链路本轮都没有运行，当前只能证明“收到指定文本后能正确路由并发布答案消息”，不能声称“机器人已经能听懂并说出知识库答案”。**

| 维度 | 结论 | 关键证据 |
|---|---|---|
| 代码接入 | 已完成，新增 4 个文件、修改 5 个文件、1 个文件未变 | [附录 A](SOURCE_APPENDIX.md)、[附录 C](CODE_DIFF_APPENDIX.md) |
| 知识库数据 | 20 条 FAQ、3 个研究方向、1 个项目、1 个设备条目、5 个分类、15 个来源 | [附录 B](KNOWLEDGE_BASE_APPENDIX.md) |
| 构建 | 成功，`cmake --build` 与 `cmake --install` 退出码均为 0，耗时约 31.9 秒 | [附录 E.1](TEST_EVIDENCE_APPENDIX.md) |
| 单元测试 | KnowledgeBase 37/37 PASS；IntentRecognizer 25/25 PASS | [附录 E.2/E.3](TEST_EVIDENCE_APPENDIX.md) |
| CTest | `100% tests passed, 0 tests failed out of 2` | [附录 E.1.4](TEST_EVIDENCE_APPENDIX.md) |
| ROS 运行时 | 默认域 18 条注入 + 隔离域 42/43 各 1 条；捕获的是 `/g1/tts` 消息，不是扬声器播放 | [附录 E.4/E.5](TEST_EVIDENCE_APPENDIX.md) |
| 真实端到端 | **未验证**：ASR 桥、TTS 节点、动作执行器均未运行 | 本报告第 9 节 |
| 生产可用性 | **尚不具备**：需补真实链路、运行态覆盖和若干检索缺陷修复 | 本报告第 10、11 节 |

**必须避免的三种误读：**

1. 37/25 条单元测试通过，不等于 20 条 FAQ 都在真机上验证过；运行时只命中过 9 个 FAQ ID。
2. `/g1/tts` 捕获到字节，不等于扬声器播报；本轮 TTS 节点没有启动。
3. 本地静态复现发现的问题，是“按当前 C++ 逻辑重放”的结果，不是本轮在机器人上重新注入得到的运行时结果。

---

## 1. 项目现状

### 1.1 项目名称与用途

- **项目名称**：G1-EDU 校园实验室智能接待系统（ROS 2 包名 `g1_voice_router`）。
- **使用场景**：参观者用中文语音询问学院、脑机交互智能研究中心、人形机器人具身智能感知与自主学习重庆市重点实验室、具身智能开放平台等信息；机器人用本地知识库直接回答实验室相关问题，用外部 Ollama 回答通用问题，并对问候语发布挥手动作请求。
- **本轮范围**：只审查 KnowledgeBase 改造及部署状态，不评价上层产品设计、不执行动作、不播放语音。

### 1.2 机器人硬件与软件

| 项目 | 值 | 证据类型 |
|---|---|---|
| 机器人型号 | 宇树 G1-EDU 人形机器人 | 项目背景（历史信息） |
| 计算单元 | Jetson Orin NX，aarch64 | 历史信息；本轮实测架构 `aarch64` |
| 主机名 | `ubuntu` | 本轮 SSH 实测 |
| 内核 | `Linux 5.15.185-tegra ... aarch64` | 本轮 SSH 实测 |
| 操作系统 | Ubuntu 22.04.5 LTS | 本轮 SSH 实测（`/etc/os-release`） |
| L4T | R36.5.0 | 本轮 SSH 实测（`/etc/nv_tegra_release`） |
| ROS 2 | Humble | 本轮 SSH 实测 |
| RMW | `rmw_cyclonedds_cpp` | 运行进程环境实测 |
| DDS 网卡 | `eth0`（`CYCLONEDDS_URI` 指定） | 运行进程环境与 `setup.sh` 实测 |
| ROS 域 | 未设置 `ROS_DOMAIN_ID`，默认 0 | 本轮实测；隔离测试显式使用 42/43 |
| 机器人 IP | `192.168.123.164` | 历史配置信息；本轮 SSH 连通 |
| 工作空间 | `/home/unitree/unitree_ros2/example` | 本轮实测 |
| 笔记本电脑 Ollama | `http://192.168.123.99:11434/api/chat`，模型 `qwen3-vl:4b-instruct`，超时 60 秒 | 运行节点参数实测 |

> **历史信息与实测的区分：** JetPack 6.2.3 / CUDA 12.6 是上一轮部署时的记录；本轮没有重新读取 JetPack 版本，`nvidia-jetpack` 与 `nvcc` 也未在当前 PATH 中检测到。因此本报告不把 JetPack/CUDA 版本当作本轮实测结论。

### 1.3 ROS 2 工作空间与相关包

```text
/home/unitree/unitree_ros2/example/                 # colcon 工作空间
├── src/                                            # 共享头文件（含 nlohmann/json.hpp）
├── g1_voice_router/                                # 本次改造包（源码）
├── g1_asr_bridge/                                  # ASR JSON 桥接（源码）
├── src/                                            # unitree_ros2_example / g1_tts_node
├── build/
└── install/
    ├── g1_voice_router/
    ├── g1_asr_bridge/
    └── unitree_ros2_example/

/home/unitree/lidar_ws/                             # 另一工作空间
├── src/g1_action_executor/                         # 手臂动作执行器源码
└── install/
    ├── g1_action_executor/
    └── g1_sports_show/
```

| 包 | 可执行文件 | 源码位置 | 作用 | 当前是否运行 |
|---|---|---|---|---|
| `g1_asr_bridge` | ASR JSON 桥接节点 | `/home/unitree/unitree_ros2/example/g1_asr_bridge/src/asr_json_bridge.cpp` | 订阅 `/audio_msg`，发布 `/g1/asr_text` | **否** |
| `g1_voice_router` | `voice_router` | `/home/unitree/unitree_ros2/example/g1_voice_router/` | 意图识别、知识库、Ollama、发布 TTS/动作请求 | **是**（PID 10751/10753） |
| `unitree_ros2_example` | `g1_tts_node` | `.../example/src/` | 订阅 `/g1/tts`，驱动语音播放 | **否** |
| `g1_action_executor` | `g1_action_executor_node` | `.../lidar_ws/src/g1_action_executor/src/arm_action_executor.cpp` | 订阅 `/g1/action_request`，发布 `/g1/action_result` | **否** |
| `g1_sports_show` | `g1_sports_show_node` | `.../lidar_ws/src/g1_sports_show/` | 运动展示，不在语音链路 | **否** |
| `unitree_api` / `unitree_go` / `unitree_hg` | — | `.../cyclonedds_ws/` | 宇树消息定义 | 由 `/ros_bridge` 使用 |

### 1.4 节点输入/输出话题

| 话题 | 类型 | 方向（相对 Voice Router） | 当前端点（2026-10-31 14:01Z） |
|---|---|---|---|
| `/g1/asr_text` | `std_msgs/msg/String` | 输入 | 0 个发布者，1 个订阅者（Voice Router） |
| `/g1/tts` | `std_msgs/msg/String` | 输出 | 1 个发布者（Voice Router），1 个订阅者（遗留 `ros2 topic echo`，PID 4216） |
| `/g1/action_request` | `std_msgs/msg/String` | 输出 | 1 个发布者（Voice Router），0 个订阅者 |
| `/g1/action_result` | `std_msgs/msg/String` | 动作结果 | **Unknown topic**（当前无端点） |
| `/audio_msg` | `std_msgs/msg/String` | ASR 桥输入 | 当前发布者为 `_CREATED_BY_BARE_DDS_APP_`；ASR 桥未运行 |
| `/audio_msg/filter` | `std_msgs/msg/String` | ASR 过滤话题 | 存在于话题列表，未参与本轮测试 |

> 结论：当前默认域语音链路是“断头”的。没有 ASR 桥把 `/audio_msg` 转成 `/g1/asr_text`，没有 TTS 节点消费 `/g1/tts`，没有动作执行器消费 `/g1/action_request`。

### 1.5 模块职责

| 模块 | 职责 | 关键文件 |
|---|---|---|
| `IntentRecognizer` | 文本归一化、7 类意图识别 | `src/intent_recognizer.cpp`、`include/g1_voice_router/intent_recognizer.hpp` |
| `KnowledgeBase` | JSON 加载校验、FAQ/研究方向/项目/设备/分类检索、兜底 | `src/knowledge_base.cpp`、`include/g1_voice_router/knowledge_base.hpp` |
| `G1VoiceRouter` | 订阅 ASR 文本、分发本地回复或 Ollama 队列、发布 TTS/动作请求、维护对话历史 | `src/voice_router.cpp` |
| 单元测试 | 知识库 37 条、意图 25 条 | `test/knowledge_base_test.cpp`、`test/intent_recognizer_test.cpp` |
| 知识库数据 | 20 FAQ、3 研究方向、1 项目、1 设备、5 分类、15 来源 | `config/lab_knowledge.json` |

### 1.6 已完成的业务功能

1. 文本输入 → 意图识别（GREETING、LAB_INTRO、ROBOT_INTRO、DEMO_ACTION、ROBOT_STATUS、GOODBYE、GENERAL_QA）。
2. `LAB_INTRO` → 本地知识库同步回答，**不进入 Ollama 队列**。
3. `GREETING`（精确问候语）→ 发布 `wave` 动作请求 + 本地固定问候语。
4. `ROBOT_INTRO` → 本地固定自我介绍，不调用 Ollama。
5. `GOODBYE` → 本地固定告别语，不调用 Ollama。
6. `GENERAL_QA` → 进入单工作线程队列，调用 Ollama `/api/chat`，保留最近 6 轮对话；失败时播报固定提示。
7. 知识库：启动时加载一次，校验 schema/来源/ID/URL，支持 `knowledge_file` 参数覆盖，加载失败安全兜底。
8. 部署：G1 原生 ARM64 编译、安装、备份改造前文件、隔离域与默认域注入测试。
9. 测试：KnowledgeBase 37 条、IntentRecognizer 25 条断言全部通过。

### 1.7 未完成的功能

1. `DEMO_ACTION` 没有本地动作分支：识别为动作演示后仍会进入 Ollama 队列。
2. `ROBOT_STATUS` 没有本地状态分支：识别为状态查询后仍会进入 Ollama 队列。
3. 真实 ASR → Voice Router → TTS → 扬声器链路未验证。
4. 动作执行器未启动，`wave` 请求没有订阅者，真实挥手未验证。
5. 20 条 FAQ 只有 9 条在运行时命中过，剩余 11 条只经过单元测试或静态检查。
6. 多轮上下文、队列积压、连续语音、请求中退出等并发/边界场景未测试。
7. GENERAL_QA 的 Ollama 答案没有事实接地，已出现与公开资料冲突的价格数字。
8. 生产进程的开机自启方式未核实（本轮未检查 systemd）。
9. 远端包内仍留有 3 个 `.bak` 文件，未清理。

### 1.8 当前运行的节点与进程（本轮实测）

```text
PID    PPID   STARTED                  CMD
10751     1   Sat Oct 31 13:33:06 2026 /usr/bin/python3 /opt/ros/humble/bin/ros2 run g1_voice_router voice_router
10753 10751   Sat Oct 31 13:33:07 2026 /home/unitree/unitree_ros2/example/install/g1_voice_router/lib/g1_voice_router/voice_router
```

- 当前 ROS 节点只有 `/g1_voice_router` 和 `/ros_bridge`。
- 没有 `g1_tts_node`、`g1_action_executor_node`、`g1_asr_bridge` 进程。
- `/g1/tts` 的订阅者 `_ros2cli_4216` 是 **12:32:25 启动的遗留 `ros2 topic echo`**（PID 4216），不是 TTS 节点。
- 生产 Voice Router 启动于 13:33:06/07，早于本轮 14:01Z 快照；本轮没有停止、重启或修改它。

### 1.9 历史信息与本轮实测对照

| 项目 | 历史/背景信息 | 本轮是否实测 | 说明 |
|---|---|---|---|
| 机器人型号 G1-EDU | 是 | 否（未重新读取铭牌/序列号） | 作为项目背景使用 |
| JetPack 6.2.3 | 是（上一轮） | **否** | 本轮未找到 `nvidia-jetpack`/`nvcc`，不据此下结论 |
| CUDA 12.6 | 是（上一轮） | **否** | 同上 |
| 内核/Ubuntu/L4T | — | **是** | 见 1.2 |
| ROS 2 Humble / RMW | — | **是** | 见 1.2 |
| Ollama 模型/地址/超时 | — | **是**（读节点参数） | 笔记本侧服务配置未检查 |
| 构建/测试结果 | — | **历史原始输出** | 本轮未重新编译、未重新跑测试 |
| 当前进程/话题 | — | **是** | 14:01Z 只读快照 |

---

## 2. 部署环境与依赖

### 2.1 实际环境快照

采集时间：**2026-10-31T14:01:14Z**（补充证据采集于 13:43–13:44Z）。完整输出见 `evidence/deploy/current_state_20261031T1401Z.txt`。

```text
hostname: ubuntu
kernel:   Linux ubuntu 5.15.185-tegra #1 SMP PREEMPT Thu Jan 15 19:24:38 PST 2026 aarch64
OS:       Ubuntu 22.04.5 LTS (VERSION_ID=22.04)
L4T:      R36 (release), REVISION: 5.0
ROS:      ROS_DISTRO=humble
RMW:      rmw_cyclonedds_cpp（运行进程环境）
DOMAIN:   ROS_DOMAIN_ID 未设置（默认 0）
LOCAL:    ROS_LOCALHOST_ONLY=0
```

> 采集 shell 本身只 source 了 `/opt/ros/humble/setup.bash`，因此该 shell 的 `RMW_IMPLEMENTATION` 显示 `<unset>`；运行中的 Voice Router 进程环境实测为 `rmw_cyclonedds_cpp`，并加载了 `librmw_cyclonedds_cpp.so`。两者不矛盾。

### 2.2 编译与运行依赖

| 依赖 | 版本/来源 | 用途 | 证据 |
|---|---|---|---|
| C++ 标准 | C++17 | 知识库、路由器 | `CMakeLists.txt` |
| `rclcpp` | ROS 2 Humble | 节点、话题、参数 | `package.xml`、`CMakeLists.txt` |
| `std_msgs` | ROS 2 Humble | `/g1/*` 字符串消息 | 同上 |
| `ament_index_cpp` | ROS 2 Humble | 解析包 share 目录 | 新增依赖 |
| `libcurl` | 系统库 | Ollama HTTP 请求 | `find_package(CURL REQUIRED)` |
| `Threads` | 系统库 | 工作线程 | `find_package(Threads REQUIRED)` |
| `nlohmann/json.hpp` | 工作区既有头文件 `.../example/src/include/nlohmann/` | JSON 解析 | 未新增第三方依赖 |
| Ollama | 笔记本 `192.168.123.99:11434`，`qwen3-vl:4b-instruct` | GENERAL_QA | 节点参数实测 |
| DDS | CycloneDDS，`eth0` | 机器人通信 | `setup.sh`、进程环境 |

### 2.3 环境限制

- 本轮**没有**重新编译，也没有重新运行 `colcon test`；测试结论来自 13:24–13:43 的原始输出。
- 本轮**没有**检查笔记本 Ollama 服务的配置文件、模型清单或日志；只读取了机器人侧节点参数。
- 本轮**没有**检查 systemd 单元，无法确认生产进程是否配置开机自启。
- 本轮**没有**检查 JetPack/CUDA；历史版本号不作为本轮结论。

---

## 3. 系统架构与数据流

### 3.1 语音链路

```text
麦克风 / 上位机
      │
      ▼
  /audio_msg  (std_msgs/String)
      │
      ▼
 g1_asr_bridge  ──────────────►  /g1/asr_text  (std_msgs/String)
                                      │
                                      ▼
                             g1_voice_router
                              │     │      │
              ┌───────────────┘     │      └────────────────┐
              ▼                     ▼                       ▼
      本地固定回复            KnowledgeBase             Ollama 队列
      GREETING/ROBOT_INTRO    LAB_INTRO                 GENERAL_QA
      GOODBYE                 （同步、无网络）           （单工作线程、HTTP 60s）
              │                     │                       │
              └──────────┬──────────┴───────────┬───────────┘
                         ▼                      ▼
                  /g1/tts                 /g1/action_request
                  (TTS 节点)               (动作执行器)
```

当前实际运行状态：`g1_asr_bridge`、TTS 节点、动作执行器均未启动，因此上图只有中间 `g1_voice_router` 一段在运行。

### 3.2 `enqueue_user_text()` 分发顺序

源码位置：`src/voice_router.cpp:263-397`。

```text
1. trim 输入；空文本 → WARN 并 return
2. IntentRecognizer::recognize()
3. GREETING 且 match_greeting_reply() 命中
   → 发布 action_request "wave"
   → 发布固定问候语
   → return（不入 Ollama）
4. LAB_INTRO
   → KnowledgeBase::query()
   → 发布答案（命中或兜底）
   → return（不入 Ollama）
5. ROBOT_INTRO → 固定自我介绍 → return
6. GOODBYE → 固定告别语 → return
7. 其余全部（含 DEMO_ACTION、ROBOT_STATUS、GENERAL_QA）
   → 加锁 push_back 到 queue_
   → notify_one()
   → worker_loop() 串行调用 Ollama
```

**关键事实：** `DEMO_ACTION` 与 `ROBOT_STATUS` 在意图识别层存在，但在 `voice_router.cpp` 中既没有本地分支，也没有动作/状态处理；它们和 `GENERAL_QA` 一样进入 Ollama 队列。这一行为在改造前的备份中已经存在，不是本次知识库改造引入的回归。

### 3.3 知识库查询顺序

源码位置：`src/knowledge_base.cpp:568-708`。

```text
1. 精确 FAQ 问题匹配（归一化后完全相等）
2. FAQ 问题被包含在输入中（问题长度 ≥ 5 个 UTF-8 字符）
3. FAQ 关键词评分
4. research_directions / projects / equipment 关键词评分
5. categories 关键词评分
6. 兜底：fallback_answer（加载成功时 162 字节）
        或硬编码 "这个信息目前没有公开资料，我不能确认。"（57 字节）
```

关键词评分规则（`match_score`，`:75-110`）：

- 至少命中 **2 个不同关键词**，或 **单个关键词 ≥ 4 个 UTF-8 码点**，才算有效命中。
- 得分 = 所有命中关键词的 UTF-8 长度之和 + 命中个数。
- 并列时保留**先遍历到**的条目（`score > best_score`，不是 `>=`）。

### 3.4 线程、队列与历史

| 项目 | 实现 | 风险/说明 |
|---|---|---|
| 订阅回调 | 在 ROS executor 线程中执行 | `LAB_INTRO` 同步查询，无网络 I/O，通常很快 |
| 工作线程 | 单个 `std::thread` 串行处理 Ollama 队列 | 不会有两个 Ollama 请求同时修改 `history_` |
| 队列 | `std::deque<std::string>`，无上限 | 连续语音可能堆积；无去重 |
| 历史 | `std::deque<Json>`，系统提示 + 最近 6 轮 | 只在请求成功后追加 |
| 退出 | `stop_worker()` 清空队列并 `join()` | 正在进行的 HTTP 请求最长可阻塞 60 秒 |
| 背压 | 无 | 队列长度只打印日志，不做丢弃/限流 |

---

## 4. 核心源码全文索引

> **全文位置：** [附录 A：G1 当前部署源码全文](SOURCE_APPENDIX.md)。附录 A 逐字复制 9 个源码/构建文件，未使用省略号、未做格式化。本报告不重复粘贴数百行代码，避免主报告不可读；审查包按“主报告 + 附录”交付，`INDEX.md` 已说明全部文件。

### 4.1 文件清单

| 序号 | 文件（远端路径） | 状态 | SHA256 | 行数 | 全文位置 |
|---:|---|---|---|---:|---|
| 1 | `include/g1_voice_router/knowledge_base.hpp` | 新增 | `1dc19db4db18d5f3503488fe7486d5e794b5c645fc6259ee878bebb53d57a2af` | 48 | 附录 A.1 |
| 2 | `src/knowledge_base.cpp` | 新增 | `295df377a43e0eb9675bb6f6cc8cdad6f26a3fed4b74ecfbeb4e5bb62f043477` | 772 | 附录 A.2 |
| 3 | `test/knowledge_base_test.cpp` | 新增 | `dab2aef7156ff2cf3762f98a9d1681f38200f140ebb865482d4749c3c9c742fa` | 362 | 附录 A.3 |
| 4 | `include/g1_voice_router/intent_recognizer.hpp` | 未修改 | `a208e31fdf4970177438b98fb2def583b6ae459f22d9cec22387fc706ae709c5` | 47 | 附录 A.4 |
| 5 | `src/intent_recognizer.cpp` | 修改 | `80d50eaaa8a75733f55bda629b5be19cf8fd9b0a921d8974e2441fda80640569` | 310 | 附录 A.5 |
| 6 | `test/intent_recognizer_test.cpp` | 修改 | `cd80b15f4542412051133e30a9a7faefc55eca7dbb20fe130778efd9c3abb3d7` | 86 | 附录 A.6 |
| 7 | `src/voice_router.cpp` | 修改 | `fd6d29c02757eb82dba517d53d2f3f72c82d324cc56741ae917e8e42988e0148` | 688 | 附录 A.7 |
| 8 | `CMakeLists.txt` | 修改 | `a7166376f6335ae346250d5b4af23e9fe9a6d624bf117420906ec46cee2feb8c` | 94 | 附录 A.8 |
| 9 | `package.xml` | 修改 | `22d0f4a6341c455bc83122d7983c15490a0bd96730136dab247ae5d2a42dcb39` | 19 | 附录 A.9 |

知识库数据文件 `config/lab_knowledge.json`（新增，798 行，SHA256 `cbe711e3b5c19115dc9d85fe37c701829ee3ad37203a3a4337efa5263d5acea7`）全文见 [附录 B.7](KNOWLEDGE_BASE_APPENDIX.md)。

**哈希一致性：** 上述 10 个文件的 SHA256 与 G1 远端 `sha256sum` 实测值、本地开发副本三者一致（快照时间 2026-10-31 14:01Z）。远端包内另有 3 个遗留备份文件，见 4.3。

### 4.2 关键接口摘要

`knowledge_base.hpp`：

```cpp
struct KnowledgeResult {
    bool found{false};
    std::string category;
    std::string matched_id;
    std::string answer;
};

class KnowledgeBase {
public:
    bool load(const std::string &file_path, std::string &error);
    KnowledgeResult query(const std::string &user_text) const;
    bool loaded() const noexcept;
    std::size_t faq_count() const noexcept;
    std::size_t direction_count() const noexcept;
    std::size_t project_count() const noexcept;
    std::size_t equipment_count() const noexcept;
private:
    KnowledgeResult fallback_result() const;
    nlohmann::json data_;
    bool loaded_{false};
};
```

`intent_recognizer.hpp` 定义 7 个枚举值：`GREETING`、`LAB_INTRO`、`ROBOT_INTRO`、`DEMO_ACTION`、`ROBOT_STATUS`、`GOODBYE`、`GENERAL_QA`；`IntentResult` 包含 `intent` 与 `normalized_text`。

### 4.3 远端包内遗留文件

| 文件 | SHA256 | 说明 |
|---|---|---|
| `src/voice_router.cpp.bak` | `24eb907d05b048d21babbc533a4b23686757215bebd1edfd028bf8619d373dec` | 历史手工备份，不在 CMake 源文件列表中 |
| `src/voice_router.cpp.before_action` | `1f20d072af8516dcfa025ade403baf043fc99fce449edc6bbea4da0639724dd2` | 历史手工备份 |
| `src/voice_router.cpp.before_local_reception` | `87a95c5f3792c55eb4c905c60248a7321344d38c5112690be85152044d98bf8c` | 历史手工备份 |

> 这 3 个文件不会被编译，但会干扰后续维护和“当前版本”判断。是否删除由用户决定，本轮未删除。

---

## 5. 知识库完整数据索引

> **全文位置：** [附录 B：知识库完整数据与结构说明](KNOWLEDGE_BASE_APPENDIX.md)。B.7 逐字包含 798 行 JSON，未使用 schema 示例、未省略条目。

### 5.1 运行时实际加载路径

```text
默认路径：
  ament_index_cpp::get_package_share_directory("g1_voice_router")
  + "/config/lab_knowledge.json"

当前参数值（ros2 param get 实测）：
  /home/unitree/unitree_ros2/example/install/g1_voice_router/share/g1_voice_router/config/lab_knowledge.json

安装路径实际是符号链接：
  .../install/.../config/lab_knowledge.json
    -> /home/unitree/unitree_ros2/example/g1_voice_router/config/lab_knowledge.json
```

加载成功后数据常驻内存；修改 JSON 必须重启节点才会生效。

### 5.2 顶层结构与统计

| 顶层字段 | 当前值/数量 | 是否被 `query()` 使用 | 说明 |
|---|---|---|---|
| `schema_version` | `1` | 加载校验 | 仅支持 1 |
| `knowledge_version` | `1.0.0` | 否 | 元数据，不参与校验 |
| `updated_at` | `2026-10-31` | 否 | 元数据，运行时不做新鲜度检查 |
| `school_name` | 重庆理工大学 | 否 | 仅类型校验 |
| `college_name` | 重庆理工大学两江人工智能学院 | 否 | 仅类型校验 |
| `lab_name` | 两个机构名 | 否 | 仅类型校验 |
| `location` | 81 字节 | **否** | 顶层地址未参与回答 |
| `introduction` | 412 字节 | **否** | 顶层简介未参与回答 |
| `labs` | 3 条 | 否 | 机构元数据 |
| `research_directions` | 3 条 | 是 | 关键词查询第 3 层 |
| `projects` | 1 条 | 是 | 关键词查询第 3 层 |
| `equipment` | 1 条 | 是 | 关键词查询第 3 层 |
| `categories` | 5 条 | 是 | 关键词查询第 4 层 |
| `faq` | 20 条 | 是 | 精确/包含/关键词查询 |
| `fallback_answer` | 162 字节 | 是 | 加载成功且无命中时使用 |
| `disclaimer` | 69 字节 | **否** | 当前没有播报路径引用 |
| `sources` | 15 条（A=13，B=1，C=1） | 仅校验 | 答案不播报来源 |

> **维护陷阱：** 修改顶层 `introduction`、`location`、`disclaimer` 不会改变机器人回答；必须同步修改 `faq` 或 `categories` 中的对应答案。

### 5.3 FAQ 结构

```json
{
  "id": "faq_intro",
  "category": "introduction",
  "questions": ["介绍一下你们实验室", "..."],
  "keywords": ["实验室介绍", "..."],
  "answer": "……",
  "source_ids": ["S04", "S05", "S01", "S02", "S09"]
}
```

- `id` 全库唯一；`questions`、`keywords`、`answer`、`source_ids` 必填且非空。
- `answer` 禁止包含 `http://`、`https://`、`www.`。
- `source_ids` 必须全部存在于顶层 `sources`。
- `category` 可选，缺省为 `faq`。

### 5.4 其余检索条目结构

- `research_directions` / `projects` / `equipment`：`id`、`keywords`、`answer`、`source_ids` 必填；`name`、`summary`、`lab` 等字段当前查询不读取。
- `categories`：`id`、`keywords`、`answer`、`source_ids` 必填。
- `sources`：`id`、`title`、`publisher`、`url`、`level` 等；加载时校验 ID 唯一与 URL。
- `labs`：`id`、`name`、`type`、`source_ids` 等；**`labs[].id` 不在全库唯一 ID 检查范围内**。

### 5.5 空字段与兜底

- 空 `introduction`、空 `location` 等条目不会进入回答；测试 35/36 验证“不播报空字段”。
- 加载成功但查询无命中 → 播报 162 字节 `fallback_answer`。
- 加载失败（文件缺失、JSON 畸形、schema 不支持、校验失败）→ `data_` 被清空，`loaded_=false`；查询退化为 **57 字节硬编码兜底**，配置中的 162 字节兜底不可用。

### 5.6 数据规模

| 类型 | 数量 | 备注 |
|---|---:|---|
| FAQ | 20 | 覆盖学院、脑机中心、重点实验室、开放平台、人物、项目、设备、地址、团队、奖项 |
| 研究方向 | 3 | 脑机中心、市级重点实验室、开放平台 |
| 项目 | 1 | 揭榜挂帅项目；“在研”为按周期推断 |
| 设备 | 1 | 只披露载体类别，不披露品牌/型号/数量 |
| 分类 | 5 | introduction、research_directions、projects、equipment、location |
| 来源 | 15 | A=13、B=1（S14）、C=1（S04） |
| labs | 3 | 脑机中心、市级重点实验室、开放平台 |

---

## 6. 调研资料与来源核验索引

> **全文位置：** [附录 D：调研资料全文与事实映射](RESEARCH_APPENDIX.md)。
> D.2 为 **35 行**“知识库条目 → 事实 → 来源”映射表；D.3 为 15 个来源的标题、发布机构、原始 URL、发布日期、访问日期、支撑事实与摘录；D.5 逐字包含 8 份调研文档。

### 6.1 调研文档清单

| 文件 | SHA256 | 作用 |
|---|---|---|
| `README.md` | `483f372d135f980a1cf99ffe853810087a3868e9a3047ebdf082dbdd2cc87a26` | 调研资料索引与阅读顺序 |
| `LAB_PROFILE.md` | `13ce0c054d2156bd3894287ad2a83e78adacf5304b2e61143dcb406372de13f9` | 实验室公开档案 |
| `LAB_RESEARCH_REPORT.md` | `48fbd4fa7f4ccbd93f2aeb0fd5671c6490add4a290d4f436c50f2a868165be61` | 完整调研报告 |
| `SOURCES.md` | `d0a82fec2de246ccad60918d15459a5de60e07b90e650227eb653ead9981e47b` | 来源清单与摘录 |
| `FACT_CHECK.md` | `44806019ac0d7db011572b65ebf5a67bb0dfcac1e7dad65be218ec40ade13341` | 逐条事实核验 |
| `FAQ_DRAFT.md` | `53b728b38197f7f98bb918b71e6de20219d084404c5178070c954d5e53a9f751` | FAQ 草稿与措辞边界 |
| `UNVERIFIED.md` | `1644c78d4dd5e5b7ba1be4a267d612863c8973f49d4c2652b34156e03404e4d3` | 未确认项与禁止表述 |
| `TALK_SCRIPTS.md` | `8ef29f54a9d5d32f1d117d03ba4d4a71e56cb5fc0c73b0070e9ff15e437f8095` | 讲解话术 |

> 本地 `research_materials/` 副本与 `/home/mick/Mick_Grey/teach/lab_research/` 原件哈希一致。

### 6.2 来源等级与核验状态

| 等级 | 含义 | 数量 | 说明 |
|---|---|---:|---|
| A | 官方机构页面/PDF/官方索引 | 13 | 多数被 WAF 拦截，采用搜索索引摘录或 PDF 直接读取 |
| B | 会议/学术公开资料 | 1 | S14，CCBR 2025 论坛页，直接打开 |
| C | 媒体全文 | 1 | S04，第1眼TV-华龙网，直接打开全文 |

| 来源 ID | 标题 | 发布机构 | 发布日期 | 等级 | 获取/核验状态 |
|---|---|---|---|---|---|
| S01 | 2025年拟认定重庆市重点实验室名单 | 重庆市科学技术局 | 2025-07-04 | A | 直接下载 PDF，全文可读 |
| S02 | 【喜报】市级重点实验室获批！我院在人形机器人方向实现关键突破 | 重庆理工大学两江人工智能学院 | 2025-07-16 | A | WAF 拦截；索引摘录 + 转载交叉读取 |
| S03 | [喜讯]学校新增3个市级科研平台 | 重庆理工大学 | 2025-10-19 | A | WAF 拦截；仅索引摘录 |
| S04 | 重庆理工大学举行脑机交互智能研究中心成立仪式暨2025脑机交互前沿学术报告 | 第1眼TV-华龙网 | 2025-12-31 | C | 直接打开，全文可核验 |
| S05 | 脑机交互智能研究中心在两江人工智能学院成立 | 重庆理工大学两江人工智能学院 | 2026-01-05 | A | WAF 拦截；仅索引摘录 |
| S06 | 学院简介 | 重庆理工大学两江人工智能学院 | 页面未标注 | A | WAF 拦截；仅索引摘录 |
| S07 | 两江人工智能学院2025年本科生招生简章 | 重庆理工大学两江人工智能学院 | 2025 | A | WAF 拦截；仅索引摘录 |
| S09 | 两江人工智能学院举行建院8周年产教融合开放日活动 | 重庆理工大学两江人工智能学院 | 2026-06-10 | A | WAF 拦截；仅索引摘录 |
| S10 | 肖汉光 | 重庆理工大学两江人工智能学院 | 2026-01-14 | A | WAF 拦截；仅索引摘录 |
| S11 | 朱秘 | 重庆理工大学两江人工智能学院 | 2026-01-14 | A | WAF 拦截；仅索引摘录 |
| S12 | 刘宏（院长） | 重庆理工大学两江人工智能学院 | 页面未标注 | A | WAF 拦截；仅索引摘录 |
| S13 | 肖汉光（机械工程学院教师页） | 重庆理工大学机械工程学院 | 2025-09-26 | A | WAF 拦截；仅索引摘录 |
| S14 | 特色论坛 - 中国生物特征识别大会 CCBR 2025 | 中国生物特征识别大会 CCBR 2025 | 2025 | B | 直接打开，全文可核验 |
| S16 | [喜讯]学校参赛团队在第28届中国机器人及人工智能大赛全国总决赛中斩获一等奖等15项荣誉 | 重庆理工大学 | 2026-08-24 | A | WAF 拦截；仅索引摘录 |
| S21 | 两江人工智能学院首页 | 重庆理工大学两江人工智能学院 | 页面未标注 | A | WAF 拦截；地址取自索引摘录及页脚 |

> **原始 URL、访问日期、支撑事实与短摘录**见 [附录 D.3](RESEARCH_APPENDIX.md)。本表不重复 URL，避免主报告过长。

### 6.3 事实映射与核验状态

附录 D.2 共 35 行，覆盖 2 个顶层字段、3 个 labs、3 个研究方向、1 个项目、1 个设备、5 个分类、20 个 FAQ。核验状态分为：

- **官方材料直接确认：** 如 S01 PDF 中的实验室正式名称、依托单位、共建单位、主任。
- **论文/公开报告支撑：** 如 S14 对肖汉光研究方向的描述。
- **多来源交叉验证：** 如脑机中心成立时间（S04 + S05）、实验室获批（S01 + S02）。
- **由公开材料推断：** 如项目“在研”状态（按 2025.04–2028.03 周期推断）。
- **检索否定结论：** 如“没有只叫具身智能实验室的官方机构”“组织关系未公开”。
- **仍未确认、未写入知识库：** 见附录 D.4.5 与 `UNVERIFIED.md`。

### 6.4 必须标注的推断与表述边界

| 条目 | 问题 | 证据位置 |
|---|---|---|
| `faq_project` / `projects` | “在研”是按公开周期推断；答案已写“按公开周期推断……具体进展以学院正式介绍为准” | 附录 D.2、D.4.4 |
| `faq_embodied_name` | “没有只叫具身智能实验室的官方机构”是检索否定结论，不是官方原文；答案中以肯定句播报 | 附录 D.2 第 21 行 |
| `faq_liu` | “他在北京大学的研究工作，不能算作重庆理工大学实验室的成果”是项目归属规则，不是来源原文 | 附录 D.2 第 32 行 |
| `faq_difference` | “组织关系未公开”是检索否定结论 | 附录 D.2 第 26 行 |
| `faq_equipment` | 只披露载体类别，品牌/型号/数量未公开；答案没有编造 | 附录 D.2 第 27 行 |
| `faq_awards` | 是学生团队成绩，答案已明确“这是学生团队的成绩” | 附录 D.2 第 30 行 |
| `faq_teams` | 来自 2025 年招生简章，存在时效性 | 附录 D.2 第 29 行 |

> 原则仍然是：**宁可少写，也不能虚构。** 推断不得当作官方事实；否定结论不得当作官方声明。

### 6.5 调研资料与运行时 Ollama 的冲突

调研资料记录宇树官方公开价格为 **“¥9.9 万元起”**（`LAB_RESEARCH_REPORT.md:193`、`FACT_CHECK.md:87`、`SOURCES.md:288`，调研来源 S20；S20 未列入 JSON `sources[]`）。运行时 GENERAL_QA 的 Ollama 回答两次出现 **“19999 元”**（`tts_general.txt`、`tts_one.txt`）。两者冲突，且 Ollama 回答没有接地到调研资料。这是 GENERAL_QA 的未接地风险，不是知识库答案。

---

## 7. 代码变更审查索引

> **全文位置：** [附录 C：代码变更审查](CODE_DIFF_APPENDIX.md)。C.2 包含 5 个修改文件的完整 unified diff；C.3 逐项分析行为影响；C.4 给出变更性质结论。

### 7.1 变更文件总览

备份目录：`/home/unitree/g1_backups/g1_voice_router_20261031_132307_before_kb/`
当前目录：`/home/unitree/unitree_ros2/example/g1_voice_router/`

| 文件 | 备份 | 当前 | 备份 SHA256 | 当前 SHA256 | 结论 | 增/删行数 |
|---|---|---|---|---|---|---|
| `CMakeLists.txt` | 有 | 有 | `b9ee317568bfc11eb09b720fc857866d165ecd626784d9f4f1e57b4c232c3225` | `a7166376f6335ae346250d5b4af23e9fe9a6d624bf117420906ec46cee2feb8c` | 修改 | +45 / -1 |
| `package.xml` | 有 | 有 | `d23ae7506c073529cc16ce8b25b85823bfac27a764c0ff5d29c712c8fba8d290` | `22d0f4a6341c455bc83122d7983c15490a0bd96730136dab247ae5d2a42dcb39` | 修改 | +1 / -0 |
| `src/voice_router.cpp` | 有 | 有 | `491fc3b9efafebd3e85278981a0ec3383d9a6f726b95c06392a8c61c69b14f2a` | `fd6d29c02757eb82dba517d53d2f3f72c82d324cc56741ae917e8e42988e0148` | 修改 | +55 / -0 |
| `src/intent_recognizer.cpp` | 有 | 有 | `7b4bae1c3f7509a630497077e2307ceb1007794087700b44d2b25ae4cc737b31` | `80d50eaaa8a75733f55bda629b5be19cf8fd9b0a921d8974e2441fda80640569` | 修改 | +20 / -1 |
| `test/intent_recognizer_test.cpp` | 有 | 有 | `36923d106dd59a9659b71c8d6c284a70f89ccdce1bbfa99b38f85d312fba3e62` | `cd80b15f4542412051133e30a9a7faefc55eca7dbb20fe130778efd9c3abb3d7` | 修改 | +11 / -0 |
| `include/g1_voice_router/intent_recognizer.hpp` | 有 | 有 | `a208e31fdf4970177438b98fb2def583b6ae459f22d9cec22387fc706ae709c5` | `a208e31fdf4970177438b98fb2def583b6ae459f22d9cec22387fc706ae709c5` | **未修改** | 0 |
| `include/g1_voice_router/knowledge_base.hpp` | 无 | 有 | — | `1dc19db4db18d5f3503488fe7486d5e794b5c645fc6259ee878bebb53d57a2af` | **新增** | +48 |
| `src/knowledge_base.cpp` | 无 | 有 | — | `295df377a43e0eb9675bb6f6cc8cdad6f26a3fed4b74ecfbeb4e5bb62f043477` | **新增** | +772 |
| `test/knowledge_base_test.cpp` | 无 | 有 | — | `dab2aef7156ff2cf3762f98a9d1681f38200f140ebb865482d4749c3c9c742fa` | **新增** | +362 |
| `config/lab_knowledge.json` | 无 | 有 | — | `cbe711e3b5c19115dc9d85fe37c701829ee3ad37203a3a4337efa5263d5acea7` | **新增** | +798 |

### 7.2 每个重要变更的目的

| 变更 | 目的 | 是否改变原业务行为 |
|---|---|---|
| 新增 `KnowledgeBase` 类与静态库 | 加载/校验/查询本地知识库 | 新增能力，不改变原有分支 |
| `voice_router.cpp` 增加 `LAB_INTRO` 本地分支 | 实验室问题不再经过 Ollama | **是，设计目标本身**：LAB_INTRO 从“入 Ollama 队列”变为“本地同步回答” |
| `voice_router.cpp` 增加知识库初始化与参数 | 启动时加载一次，支持路径覆盖 | 新增启动日志与失败日志；失败不阻止节点启动 |
| `voice_router.cpp` 增加 `#include` 与成员 | 持有 `KnowledgeBase`、`knowledge_file_` | 不影响原有成员布局语义 |
| `intent_recognizer.cpp` 扩展 LAB_INTRO 关键词 | 让学院项目、团队、奖项、地址、人物、平台等问题进入本地知识库 | **是**：这些输入从 GENERAL_QA 变为 LAB_INTRO |
| `intent_recognizer_test.cpp` 增加 10 条断言 | 覆盖新增关键词 | 仅测试 |
| `CMakeLists.txt` 增加 `ament_index_cpp`、静态库、安装 JSON、CTest | 构建与部署知识库 | 构建配置变化 |
| `package.xml` 增加 `ament_index_cpp` | 声明新依赖 | 依赖清单变化 |

### 7.3 明确未改变的部分

- **队列逻辑：** `queue_`、`queue_mutex_`、`queue_cv_`、`worker_loop()` 未重构；仍是单工作线程、FIFO、无去重、无上限。
- **线程模型：** 仍是一个 ROS executor 线程 + 一个 Ollama 工作线程。
- **回调逻辑：** `/g1/asr_text` 订阅回调仍调用 `enqueue_user_text()`。
- **Ollama 请求：** URL、模型、system prompt、`/api/chat` 请求体、60 秒超时、历史裁剪逻辑未改变。
- **GENERAL_QA 原流程：** “入队 → 单工作线程 → 复制 `history_` → POST `/api/chat` → 解析 `message.content` → 发布 `/g1/tts` → 失败固定提示”整条路径没有 diff；`LAB_INTRO` 分支只是在入队之前提前 `return`，不会改变 GENERAL_QA 的处理。
- **ROS 2 话题接口：** `/g1/asr_text`、`/g1/tts`、`/g1/action_request` 的名称、类型、QoS 深度未改变。
- **固定回复：** `ROBOT_INTRO`、`GOODBYE`、问候语回复文本未改变。
- **`intent_recognizer.hpp`：** 与备份逐字节一致。
- **ASR 桥、TTS 节点、Action Executor、DDS、systemd、Ollama 服务配置：** 本轮未修改。

### 7.4 `enqueue_user_text()` 分发顺序核对

当前顺序（`voice_router.cpp:263-397`）：

```text
空文本 → return
GREETING（且 match_greeting_reply 命中）→ wave + 固定回复 → return
LAB_INTRO → 知识库 → return
ROBOT_INTRO → 固定回复 → return
GOODBYE → 固定回复 → return
其余 → Ollama 队列
```

与备份的差异：新增了 `LAB_INTRO` 分支；其余分支顺序未变。`LAB_INTRO` 在 `ROBOT_INTRO`、`GOODBYE` 之前，且远在 Ollama 入队之前。

**副作用：** 由于 `LAB_INTRO` 关键词包含“实验室”“具身智能”“开放平台”等宽泛词，且意图识别顺序是 `LAB_INTRO` 在 `ROBOT_STATUS`、`DEMO_ACTION` 之前，包含这些词的“展示/状态”请求会被本地知识库拦截，不会触发动作或状态查询。详见第 10 节问题 2。

### 7.5 知识库初始化与路径解析

源码：`voice_router.cpp:48-83`。

```cpp
std::string default_knowledge_file;
try {
  default_knowledge_file =
    ament_index_cpp::get_package_share_directory("g1_voice_router") +
    "/config/lab_knowledge.json";
} catch (const std::exception & error) {
  RCLCPP_ERROR(get_logger(),
    "Failed to locate g1_voice_router share directory: %s", error.what());
}

knowledge_file_ = declare_parameter<std::string>(
  "knowledge_file", default_knowledge_file);

std::string knowledge_error;
if (!knowledge_base_.load(knowledge_file_, knowledge_error)) {
  RCLCPP_ERROR(get_logger(), "KnowledgeBase load failed: %s",
    knowledge_error.c_str());
} else {
  RCLCPP_INFO(get_logger(),
    "KnowledgeBase loaded: %s (FAQ=%zu, directions=%zu, projects=%zu, equipment=%zu)",
    knowledge_file_.c_str(), knowledge_base_.faq_count(),
    knowledge_base_.direction_count(), knowledge_base_.project_count(),
    knowledge_base_.equipment_count());
}
```

- 路径解析失败（包未安装/ament 索引缺失）时，`default_knowledge_file` 为空字符串；`load("")` 会失败并记录错误，节点仍继续启动。
- `knowledge_file` 参数可覆盖默认路径；域 43 的会话记录显示覆盖生效（原始 `/tmp` 日志已删除，见附录 E.5.3）。
- 加载失败不会抛异常、不会阻止节点启动；`LAB_INTRO` 退化为 57 字节兜底。

### 7.6 备份与生产文件的区分

| 概念 | 路径 | 用途 |
|---|---|---|
| 改造前备份 | `/home/unitree/g1_backups/g1_voice_router_20261031_132307_before_kb/` | 仅用于 diff，不参与运行 |
| 当前源码 | `/home/unitree/unitree_ros2/example/g1_voice_router/` | 编译输入 |
| 当前安装 | `/home/unitree/unitree_ros2/example/install/g1_voice_router/` | 运行产物；可执行文件与 JSON 均为符号链接 |
| 本地快照 | `remote_snapshot/g1_voice_router/` | 审查副本，与远端逐字节一致 |
| 本地备份副本 | `backup_snapshot/` | 审查副本，与远端备份逐字节一致 |

---

## 8. 完整测试证据索引

> **全文位置：** [附录 E：完整测试证据](TEST_EVIDENCE_APPENDIX.md)。E.1 为构建/CTest 原始输出；E.2 为 37 条 KnowledgeBase 断言逐项表；E.3 为 25 条 IntentRecognizer 断言逐项表；E.4 为 18 条默认域运行时注入；E.5 为隔离域 42/43；E.6 为未测试清单。

### 8.1 构建与 CTest

| 项目 | 结果 | 证据 |
|---|---|---|
| `colcon build --packages-select g1_voice_router`（13:24 测试之前） | `Finished <<< g1_voice_router [33.0s]`，`Summary: 1 package finished [33.6s]`，exit 0 | 会话记录（无 colcon 顶层日志副本） |
| `colcon build --packages-select g1_voice_router 2>&1`（13:32 最终构建） | `Finished <<< g1_voice_router [32.1s]`，`Summary: 1 package finished [32.7s]` | 会话记录 + `evidence/build_test/build_1332/` |
| `cmake --build ... -- -j4 -l4` | 退出码 0，`[100%] Built target voice_router`，31.939 秒 | `evidence/build_test/build_1332/g1_voice_router/streams.log` |
| `cmake --install ...` | 退出码 0 | `evidence/build_test/latest_build/command.log` |
| `colcon test --packages-select g1_voice_router --event-handlers console_direct+`（13:24） | 35 + 14 条 PASS，`100% tests passed`，`Summary: 2 tests, 0 errors, 0 failures, 0 skipped` | 会话记录（未过滤的完整控制台输出） |
| `colcon test ... 2>&1 \| grep -E "Passed:\|Failed:\|tests passed\|Test #"`（13:33） | 37 + 25 条 PASS，`100% tests passed`，0 failed | 会话记录 + `evidence/build_test/test_1333/` |
| `ctest -C Release -D ExperimentalTest --no-compress-output -V --force-new-ctest-process` | 退出码 0 | `evidence/build_test/test_1333/g1_voice_router/command.log` |
| KnowledgeBase 测试 | 37 PASS / 0 FAIL，0.02 秒 | `LastTest_20261031-1333.log` |
| IntentRecognizer 测试 | 25 PASS / 0 FAIL，0.00 秒 | 同上 |
| CTest 汇总 | `100% tests passed, 0 tests failed out of 2`，`Total Test time (real) = 0.03 sec` | `evidence/build_test/test_1333/g1_voice_router/streams.log:101-103`、`stdout.log:100-102` |
| `colcon test-result --verbose; echo "EXIT=$?"` | `Summary: 2 tests, 0 errors, 0 failures, 0 skipped`，`EXIT=0` | 会话 stdout + `evidence/test_result_1343/logger_all.log` |

> **证据边界：** 外层 `colcon build` / `colcon test` 的命令与汇总行已从会话记录恢复；包级 cmake/ctest 日志是文件证据。13:24 的测试为 35 + 14 条断言（未过滤完整输出），13:33 的测试为 37 + 25 条断言（外层经 `grep` 过滤，完整 CTest 原文见 `LastTest_20261031-1333.log` 与 `test_1333/`）。两次运行时间点不同，不能混用。`colcon test-result` 没有逐条列出测试，因为 xunit 解析器跳过了根标签不符合要求的 `Test.xml`；逐条 PASS 以 `LastTest` 与 `events.log` 为准。

### 8.2 单元测试

| 测试 | 条数 | 通过 | 失败 | 覆盖内容 |
|---|---:|---:|---:|---|
| KnowledgeBase | 37 | 37 | 0 | 加载、计数、FAQ/研究方向/项目/设备/地址/团队/奖项查询、兜底、空字段、无 URL、缺失文件、畸形 JSON、不支持 schema、空路径 |
| IntentRecognizer | 25 | 25 | 0 | 7 类意图、新增 LAB_INTRO 关键词、边界输入 |

逐条编号、输入、期望、实际、PASS/FAIL 见附录 E.2 与 E.3。

### 8.3 ROS 运行时测试

| 组 | 时间 | 条数 | 结果 | 证据 |
|---|---|---:|---|---|
| runtime1（修复前） | 13:25Z | 1 | 兜底 162 字节 | `evidence/runtime/g1_voice_router_kb_runtime.log` |
| runtime2（初次修复后） | 13:26:36 起 | 8 | 6 条本地/固定回复（含 1 条兜底），2 条 Ollama（其中 1 条价格冲突） | `..._runtime2.log` |
| runtime3（扩展意图关键词后） | 13:30:15 起 | 8 | 7 条本地知识库，1 条 Ollama | `..._runtime3.log` |
| 最终生产进程 | 13:33:07 起 | 1 | 项目问题本地命中 319 字节 | `..._kb_final.log` |
| 域 42（缺文件） | 隔离域 | 1 | 加载失败，57 字节兜底 | 会话记录（原 `/tmp` 日志已删除） |
| 域 43（路径覆盖） | 隔离域 | 1 | 加载成功，地址命中 162 字节 | 会话记录（原 `/tmp` 日志已删除） |

**测试类型区分：**

- **单元测试：** 直接执行 C++ 测试二进制，不涉及 ROS 节点、不涉及网络。
- **隔离 ROS 测试：** 独立 `ROS_DOMAIN_ID` 启动临时 Voice Router，验证加载失败与参数覆盖；没有 TTS/动作/ASR 节点。
- **默认域运行时测试：** 向 `/g1/asr_text` 注入文本，用 `ros2 topic echo` 捕获 `/g1/tts`；不是真实麦克风/扬声器。
- **真机端到端测试：** **未执行**。

### 8.4 运行时命中情况

- 20 条 FAQ 中，运行时命中过 **9 条**：`faq_intro`、`faq_equipment`、`faq_project`、`faq_bci_leaders`、`faq_teams`、`faq_awards`、`faq_location`、`faq_platform`、`faq_xiao`。
- 其余 11 条只经过单元测试或静态检查。
- 运行时还命中过 `research_directions`、`projects` 等非 FAQ 条目。
- `tts_lab1.txt` 是截断捕获（372 字节，日志记录 412 字节），不能当作完整播报证据。
- `tts_final.txt` 无时间戳，runtime3 第 1 条与最终生产第 1 条都可能产生，**归属不唯一**。

### 8.5 未测试项

真实 ASR、TTS 扬声器播放、动作执行、GREETING 真实挥手、`DEMO_ACTION`/`ROBOT_STATUS` 本地处理、20 条 FAQ 逐条运行时覆盖、多轮上下文、队列积压、请求中退出、生产进程加载失败、生产进程路径覆盖、中文 ASR 错字、机器人运动安全联锁。完整表见附录 E.6。

---

## 9. 部署环境与运行态实测

### 9.1 快照信息

- 快照时间：**2026-10-31T14:01:14Z**
- 采集方式：SSH 只读命令（`hostname`、`uname`、`cat /etc/os-release`、`ros2 node list`、`ros2 topic info`、`ros2 param get`、`ps`、`readlink`、`sha256sum`）
- 未修改任何文件、未重启任何服务、未向任何话题发布消息。

### 9.2 运行中的生产 Voice Router

```text
PID    PPID   STARTED                  CMD
10751     1   Sat Oct 31 13:33:06 2026 /usr/bin/python3 /opt/ros/humble/bin/ros2 run g1_voice_router voice_router
10753 10751   Sat Oct 31 13:33:07 2026 /home/unitree/unitree_ros2/example/install/g1_voice_router/lib/g1_voice_router/voice_router
```

- 启动命令为 `ros2 run g1_voice_router voice_router`，没有 launch 文件、没有显式 `--ros-args` 参数。
- 父进程 PID 1 说明原 shell 已退出，进程被 init 收养；这不等于配置了 systemd。本轮未检查 systemd。
- 生产进程启动日志显示知识库加载成功：`FAQ=20, directions=3, projects=1, equipment=1`。

### 9.3 当前节点与话题

```text
节点：
  /g1_voice_router
  /ros_bridge

/g1/asr_text       0 publishers, 1 subscriber
/g1/tts            1 publisher,  1 subscriber（_ros2cli_4216）
/g1/action_request 1 publisher,  0 subscribers
/g1/action_result  Unknown topic
```

- `/g1/tts` 的订阅者 PID 4216 是 **12:32:25 启动的 `ros2 topic echo /g1/tts`**，遗留未退出；不是 TTS 节点。
- 没有 TTS、动作、ASR 进程。

### 9.4 当前参数

| 参数 | 实测值 |
|---|---|
| `knowledge_file` | `/home/unitree/unitree_ros2/example/install/g1_voice_router/share/g1_voice_router/config/lab_knowledge.json` |
| `model` | `qwen3-vl:4b-instruct` |
| `ollama_url` | `http://192.168.123.99:11434/api/chat` |
| `request_timeout_seconds` | `60` |

### 9.5 安装路径与符号链接

```text
.../install/g1_voice_router/lib/g1_voice_router/voice_router
  -> /home/unitree/unitree_ros2/example/build/g1_voice_router/voice_router

.../install/g1_voice_router/share/g1_voice_router/config/lab_knowledge.json
  -> /home/unitree/unitree_ros2/example/g1_voice_router/config/lab_knowledge.json
```

- 两个安装产物都是符号链接，说明构建时使用了 symlink install（未直接读取 CMake 缓存变量，仅从安装结果判断）。
- 删除 `build/` 或源码树，会破坏安装路径；后续重启可能失败。当前进程已把知识库加载到内存，所以现进程不受影响。

### 9.6 源码哈希

10 个文件的 SHA256 与本地开发副本、远端部署目录一致；安装路径 JSON 的 SHA256 也是 `cbe711e3b5c19115dc9d85fe37c701829ee3ad37203a3a4337efa5263d5acea7`。完整哈希表见第 4.1 节与 `evidence/deploy/current_state_20261031T1401Z.txt`。

### 9.7 CMake 安装与测试规则

```cmake
install(TARGETS voice_router DESTINATION lib/${PROJECT_NAME})
install(FILES config/lab_knowledge.json
        DESTINATION share/${PROJECT_NAME}/config)

if(BUILD_TESTING)
  add_executable(knowledge_base_test test/knowledge_base_test.cpp)
  target_link_libraries(knowledge_base_test knowledge_base)
  add_test(NAME knowledge_base_test
           COMMAND knowledge_base_test
                   "${CMAKE_CURRENT_SOURCE_DIR}/config/lab_knowledge.json")

  add_executable(intent_recognizer_test
                 test/intent_recognizer_test.cpp
                 src/intent_recognizer.cpp)
  add_test(NAME intent_recognizer_test COMMAND intent_recognizer_test)
endif()
```

### 9.8 安全与凭据

- 本轮 SSH 使用既有密钥，未生成新密钥、未修改 SSH 配置。
- 报告不含私钥、密码、Token、API Key，也不含完整 SSH 配置文件。
- 机器人内网地址、主机名、用户名仅用于说明部署位置。

---

## 10. 潜在问题与技术债

> 标注规则：
> - **已确认问题**：有源码、测试或运行态证据直接支持。
> - **潜在风险**：机制上存在，但当前未触发或未测试。
> - **暂未发现问题**：已检查，未发现证据支持问题。
> - **需要进一步测试**：现有证据不足，不能下结论。
>
> 本节不为了凑数虚构 Bug；静态复现结果均明确标注“非运行时”。

### 10.1 问题总表

| # | 领域 | 结论 | 等级 |
|---:|---|---|---|
| 1 | 中文关键词误分类与检索覆盖 | “重点实验室什么时候成立”“重点实验室主任是谁”“开放平台是什么”等会命中错误 FAQ 或兜底 | **已确认问题**（本地静态复现） |
| 2 | 意图优先级抢占 | 含“实验室/具身智能/开放平台”的“展示/状态”请求进入 LAB_INTRO，不触发动作/状态 | **已确认问题**（源码 + 静态复现） |
| 3 | GREETING 识别与回复表不一致 | “您好啊”被识别为 GREETING，但 `match_greeting_reply()` 不识别，最终进入 Ollama | **已确认问题**（源码对比，既有问题） |
| 4 | 文本归一化 | `voice_router.cpp:220` 对 UTF-8 所有字节做 `tolower`，依赖 locale；当前未触发异常 | **潜在风险** |
| 5 | FAQ 多关键词冲突/并列先到先得 | 同分时保留先遍历条目，导致“开放平台是什么”命中实验室命名条 | **已确认问题**（本地静态复现） |
| 6 | 知识库初始化失败影响 | 加载失败后退化为 57 字节硬编码兜底，丢失 162 字节引导语 | **已确认问题**（隔离域 42，会话记录） |
| 7 | 配置路径部署风险 | 安装产物为符号链接，删除 build/源码会破坏重启 | **潜在风险** |
| 8 | Schema 严格性与未使用字段 | 单条坏数据拒绝整库；顶层 introduction/location/disclaimer 不参与查询；labs ID 未查重 | **潜在风险** |
| 9 | 来源支撑与表述强度 | “没有只叫具身智能实验室的官方机构”等否定结论以肯定句播报 | **已确认问题**（表述强度） |
| 10 | 项目/人物/团队信息时效性 | 项目“在研”、团队名单、奖项均有时效性，JSON 无自动过期机制 | **潜在风险** |
| 11 | Voice Router 队列上限 | 队列无上限、无去重、无背压 | **潜在风险** |
| 12 | 会话历史并发 | 单工作线程串行修改 `history_`，当前无竞态 | **暂未发现问题** |
| 13 | 长 Ollama 请求阻塞退出 | `stop_worker()` 等待 `join()`，最长可等 60 秒 | **潜在风险**（未测试） |
| 14 | 连续多条语音导致回复错配 | 无请求 ID/关联，TTS 只收到文本，无法区分对应哪条 ASR | **潜在风险**（未测试） |
| 15 | GENERAL_QA 未接地 | Ollama 两次回答“19999 元”，与公开来源“¥9.9 万元起”冲突 | **已确认问题**（运行时观察） |
| 16 | 运行链路未闭环 | 无 ASR 桥、TTS 节点、动作执行器进程 | **已确认问题**（运行态快照） |
| 17 | 测试覆盖缺口 | 11 条 FAQ 未走运行时；并发、退出、真实 ASR 未测试 | **需要进一步测试** |
| 18 | 远端遗留 `.bak` 文件 | 3 个历史备份混在源码目录 | **潜在风险**（维护） |

### 10.2 关键问题说明

#### 问题 1：中文关键词误分类与检索覆盖

**证据：** `evidence/static/query_replication_20261031.txt`（本地按当前 C++ `query()` 逻辑重放，不涉及 ROS 节点）。

| 输入 | 实际命中 | 问题 |
|---|---|---|
| `重点实验室什么时候成立` | `faq_college_established`（99 B） | 回答学院 2018 年成立，不是重点实验室 |
| `重点实验室主任是谁` | `faq_embodied_name`（216 B） | 回答两个机构名称，没有主任信息 |
| `重点实验室的主任是谁` | `faq_embodied_name` | 同上 |
| `重点实验室是哪年成立的` | `faq_embodied_name` | 同上 |
| `介绍一下你们的实验室` | 兜底（162 B） | 归一化后与 FAQ 问题不完全匹配 |
| `学院在哪里` / `学院在哪` / `实验室在哪` / `学院的地址` | 兜底 | FAQ 只覆盖“实验室在哪里”“学院地址在哪里” |
| `脑机中心有多少人` | `faq_bci_what`（263 B） | 描述中心定位，没有人数 |
| `开放平台是什么` / `开放平台是做什么的` | `faq_embodied_name`（216 B） | 回答机构命名，不是开放平台介绍 |
| `肖汉光是谁` / `刘宏是谁` | 兜底 | 单个 3 字关键词未达阈值（需 ≥4 字或 ≥2 个关键词） |

**正向对照（同一静态复现）：** `学院什么时候成立` → `faq_college_established`；`重点实验室什么时候获批` → `faq_embodied_approval`；`实验室在哪里` / `学院地址在哪里` → `faq_location`；`实验室有几台机器人` → `faq_equipment`；`脑机中心负责人是谁` → `faq_bci_leaders`；`学院有什么在研项目` → `faq_project`；`实验室有哪些设备` → `faq_equipment`；`你好介绍一下你们实验室` → `faq_intro`；`重点实验室研究什么` → `faq_embodied_research`；`开放平台研究什么` / `具身智能开放平台是什么` → `faq_platform`；`学院学生拿过什么奖` → `faq_awards`。

**影响：** 这不是“完全不能用”，而是**问法稍微变化就会答错或兜底**。对接待场景，错误回答比兜底更伤信任。

**建议：** 增补 FAQ 问法；为“重点实验室 + 时间/主任”增加专属条目；调整 `match_score` 阈值或引入更细的实体匹配；对“开放平台”类问题优先匹配 `faq_platform`。本轮不修改代码/JSON。

#### 问题 2：意图优先级抢占动作/状态请求

**证据：** `intent_recognizer.cpp:147-178` 的 LAB_INTRO 关键词包含“实验室”“具身智能”“开放平台”等；LAB_INTRO 判断在 `:184` 的 ROBOT_STATUS 和 `:210` 的 DEMO_ACTION 之前。静态复现：

| 输入 | 识别意图 | 结果 |
|---|---|---|
| `展示一下具身智能平台` | LAB_INTRO | 命中 `faq_embodied_research`，不触发动作 |
| `给我们展示一下重点实验室` | LAB_INTRO | 命中 `faq_embodied_name`，不触发动作 |
| `实验室当前状态怎么样` | LAB_INTRO | 兜底，不走 ROBOT_STATUS |
| `实验室运行正常吗` | LAB_INTRO | 兜底，不走 ROBOT_STATUS |

**影响：** 用户说“展示一下”可能期待动作，但系统会播报知识库内容。即使 `DEMO_ACTION` 被识别，Voice Router 也没有动作分支，最终仍会进 Ollama。

**定性：** `DEMO_ACTION`/`ROBOT_STATUS` 未本地处理是**改造前既有缺口**；LAB_INTRO 关键词扩张放大了抢占概率，是本次改造的副作用。

#### 问题 3：GREETING 识别与回复表不一致

**证据：**

- `intent_recognizer.cpp:108` 将 `您好啊` 识别为 `GREETING`。
- `voice_router.cpp:230-260` 的 `match_greeting_reply()` 只匹配 `你好`、`您好`、`你好啊`、`嗨`、`嗨嗨`、`哈喽`、`hello`、`hi`、`早上好`、`早安`、`上午好`、`下午好`、`晚上好`，**没有 `您好啊`**。
- 因此 `您好啊` 会识别为 GREETING，但 `match_greeting_reply()` 返回 false，不发布 `wave`、不返回，最终落入 Ollama 队列。

**定性：** 备份中同样存在，属于既有问题，不是知识库改造引入。影响较小，但会造成“问候却回答通用问题、不挥手”。

#### 问题 4：文本归一化的 locale 依赖

**证据：** `voice_router.cpp:216-222`：

```cpp
std::transform(
  text.begin(), text.end(), text.begin(),
  [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
```

- `IntentRecognizer::normalize()` 与 `KnowledgeBase::normalize_text()` 都只对 `c < 0x80` 做小写，UTF-8 中文字节原样保留。
- `voice_router.cpp` 的 `normalize_intent_text()` 对所有字节做 `tolower`，包括 UTF-8 中文的多字节序列。当前进程 locale 下未观察到异常，中文问候仍能匹配；但 `tolower` 对 0x80–0xFF 的行为依赖 locale，属于可移植性风险。
- 另外两套标点表不一致：`knowledge_base.cpp` 去掉全角括号/书名号等，`intent_recognizer.cpp` 不去掉这些符号。当前输入未触发差异，但会造成同类文本在两个模块中归一化结果不同。

**定性：** 潜在风险，当前未确认故障。

#### 问题 5：FAQ 多关键词冲突与并列先到先得

**证据：** `knowledge_base.cpp` 的 `query()` 使用 `score > best_score`；并列时保留先遍历的条目。FAQ 顺序固定，导致：

- `开放平台是什么` → `faq_embodied_name`（score 5），而不是 `faq_platform`。
- `重点实验室主任是谁` → `faq_embodied_name`（score 6）。

**影响：** 同分冲突不可预测（取决于 JSON 顺序），维护时插入新条目可能改变既有回答。

#### 问题 6：知识库初始化失败的影响

**证据：** 域 42（`ROS_DOMAIN_ID=42`，`knowledge_file:=/tmp/definitely_missing_kb.json`）：

```text
[ERROR] KnowledgeBase load failed: cannot open knowledge file: /tmp/definitely_missing_kb.json
[INFO]  G1 voice router started
...
[INFO]  Reply published to /g1/tts (57 bytes)
[INFO]  LAB_INTRO local knowledge base completed: found=false category=fallback
```

- 节点仍启动，`LAB_INTRO` 不崩溃。
- 但 `load()` 失败时清空 `data_`，`fallback_result()` 读不到配置的 162 字节 `fallback_answer`，只能返回 57 字节硬编码兜底。
- 用户听到的引导语变短，且不会提示“知识库未加载”。

**证据级别：** 会话记录；原 `/tmp` 日志已删除，本轮无法复核。

#### 问题 7：配置路径部署风险

**证据：** 安装的可执行文件与 JSON 都是符号链接，分别指向 `build/` 和源码树。

- 当前进程已将 JSON 加载到内存，删除源文件不影响现进程。
- 但下次重启会重新解析 `knowledge_file`，若 `build/` 或源码树被清理，节点会加载失败并退化为 57 字节兜底。
- 这不是当前故障，是部署脆弱性。

#### 问题 8：Schema 严格性与未使用字段

**证据：** `knowledge_base.cpp:270-566`：

- 任意一条 FAQ 缺 `answer`/`keywords`/`source_ids`，或任一 `source_id` 不存在，整库拒绝加载。
- `labs[].id` 不参与全库唯一 ID 检查；如果未来把 lab ID 与 FAQ ID 混用，可能漏检。
- `knowledge_version`、`updated_at` 只要求是字符串，不校验格式/新鲜度。
- 顶层 `introduction`、`location`、`disclaimer` 被加载但不被 `query()` 使用。

**定性：** 严格校验是安全设计，但“单条坏数据拒绝整库”在运维上是高风险；未使用字段是维护陷阱。

#### 问题 9：来源支撑与表述强度

**证据：**

- `faq_embodied_name`：“没有只叫具身智能实验室的官方机构。”——附录 D.2 标注为“检索否定结论”。
- `faq_liu`：“他在北京大学的研究工作，不能算作重庆理工大学实验室的成果。”——附录 D.2 标注为“项目归属规则，不是来源原文”。
- `faq_difference`：“两者在组织上是什么关系，公开资料没有说明。”——检索否定结论。

**定性：** 不是事实错误，而是**表述强度超过来源**。对审查而言，应明确这些是编辑判断/检索结论，不是官方声明。机器人播报时不会附带这层限定。

#### 问题 10：项目/人物/团队信息时效性

- `faq_project` 的“在研”按周期推断，项目可能提前结题/延期。
- `faq_teams` 来自 2025 年招生简章，团队可能调整。
- `faq_awards` 是 2026-08 的奖项，后续会有新成绩。
- `updated_at` 不参与运行时校验，没有过期提醒。

**定性：** 潜在风险；建议建立定期复核机制，而不是依赖代码。

#### 问题 11：Voice Router 队列无上限

**证据：** `queue_` 是 `std::deque<std::string>`，`push_back` 前没有长度检查；`queue_size` 只用于日志。单工作线程串行调用 Ollama，60 秒超时。

**影响：** 连续多条 ASR 输入会无限堆积；在 Ollama 慢或离线时，用户会听到越来越滞后的回答。没有丢弃策略、没有“正在处理”提示。

#### 问题 12：会话历史并发

**证据：** `history_` 只在 `query_ollama()` 成功后被单工作线程修改；`enqueue_user_text()` 不直接读写历史。

**结论：** 当前没有数据竞争。若未来增加多工作线程或让本地分支写历史，需要重新评估。

#### 问题 13：长 Ollama 请求阻塞退出

**证据：** `stop_worker()` 设置 `stopping_=true`、清空队列、`join()`；正在执行的 `curl_easy_perform` 不会立即取消，最长等待 `request_timeout_seconds=60`。

**影响：** 在 60 秒请求进行中按 Ctrl-C，进程可能延迟最多 60 秒退出。本轮日志中的 SIGINT 都发生在空闲时，**未测试**请求中退出。

#### 问题 14：连续多条语音导致回复错配

**证据：** `/g1/tts` 只承载字符串，没有请求 ID、时间戳或会话 ID；TTS 节点（若运行）按到达顺序播放。若用户连续说两句，第二句可能在第一句回答之后才被处理，且无法从消息本身判断对应关系。

**影响：** 接待场景可能出现“答非所问”的观感。本轮**未测试**连续输入。

#### 问题 15：GENERAL_QA 未接地

**证据：** 运行时两次捕获：

```text
tts_general.txt: 宇树G1人形机器人目前官方售价为19999元人民币。……
tts_one.txt:     宇树G1目前官方售价为19999元人民币。……
```

调研资料记录宇树官方公开价格为“¥9.9 万元起”（S20，见 `LAB_RESEARCH_REPORT.md:193`、`FACT_CHECK.md:87`、`SOURCES.md:288`）。

**影响：** 知识库接入没有改变 GENERAL_QA 的未接地问题；机器人可能自信地说出错误价格。这不是 KnowledgeBase 的缺陷，而是整体方案的已知风险。

#### 问题 16：运行链路未闭环

**证据：** 14:01Z 快照：只有 `/g1_voice_router` 与 `/ros_bridge`；`/g1/asr_text` 0 发布者；`/g1/tts` 只有遗留 echo 订阅者；`/g1/action_request` 0 订阅者；无 TTS/动作/ASR 进程。

**影响：** 当前不能做真实语音接待。所有“运行时测试”都是文本注入 + 话题捕获，不是端到端。

#### 问题 17：测试覆盖缺口

- 11 条 FAQ 未走运行时。
- 未测试并发、队列积压、请求中退出、生产进程加载失败/路径覆盖。
- 未测试真实 ASR 错字/同音词。
- 未测试动作执行与安全联锁。
- 未测试多轮上下文。

**定性：** 需要进一步测试，不能以现有 37+25 条单测替代。

#### 问题 18：远端遗留 `.bak` 文件

3 个历史备份混在 `src/` 下，不被编译，但会干扰“当前源码”判断和后续 diff。建议移出源码目录或归档；本轮未操作。

---

## 11. 未完成事项

| # | 事项 | 原因/现状 | 建议优先级 |
|---:|---|---|---|
| 1 | 修复检索误命中与覆盖缺口（问题 1、5） | 静态复现确认 | P0（接待准确性） |
| 2 | 修复意图优先级抢占（问题 2） | 静态复现确认 | P0（动作/状态不可用） |
| 3 | 修复 GREETING 表不一致（问题 3） | 源码确认 | P1 |
| 4 | 统一归一化实现（问题 4） | 潜在风险 | P1 |
| 5 | 改善加载失败兜底与告警（问题 6） | 隔离域确认 | P1 |
| 6 | 消除符号链接部署脆弱性（问题 7） | 潜在风险 | P1 |
| 7 | 处理未使用顶层字段/严格 schema（问题 8） | 维护风险 | P2 |
| 8 | 复核否定结论与编辑判断的播报措辞（问题 9） | 表述强度 | P1 |
| 9 | 建立知识库定期复核机制（问题 10） | 时效性 | P2 |
| 10 | 增加队列上限/背压/提示（问题 11） | 潜在风险 | P1 |
| 11 | 测试并处理请求中退出（问题 13） | 未测试 | P2 |
| 12 | 设计多句输入关联/顺序策略（问题 14） | 未测试 | P2 |
| 13 | 为 GENERAL_QA 增加事实约束或敏感问题拦截（问题 15） | 运行时确认 | P0（对外演示前） |
| 14 | 启动真实 ASR/TTS/动作链路并做端到端验收（问题 16） | 未执行 | P0（上线前） |
| 15 | 补齐 11 条 FAQ 运行时覆盖与并发测试（问题 17） | 未测试 | P1 |
| 16 | 清理或归档 3 个 `.bak` 文件（问题 18） | 维护 | P3 |
| 17 | 核实生产进程开机自启方式 | 未检查 systemd | P1（上线前） |
| 18 | 在生产进程上重测“你好介绍一下你们实验室”原始输入 | 修复后未重测 | P1 |
| 19 | 决定是否保留 `DEMO_ACTION`/`ROBOT_STATUS` 意图及本地实现 | 既有缺口 | P1 |
| 20 | 明确知识库更新流程（JSON 改动 + 重启） | 部署流程 | P2 |

---

## 12. 汇总表

| 检查项目 | 结果 | 证据位置 | 是否需要处理 |
|---|---|---|---|
| 项目用途与范围 | 已明确：G1 校园实验室智能接待 | 本报告 1.1 | 否 |
| 机器人硬件/系统 | 实测 aarch64、Ubuntu 22.04.5、L4T R36.5.0 | `evidence/deploy/current_state_20261031T1401Z.txt` | 否 |
| ROS 2 环境 | Humble、CycloneDDS eth0、默认域 0 | 同上 | 否 |
| 工作空间/包结构 | 已列出 6 个相关包 | 本报告 1.3 | 否 |
| 节点输入/输出话题 | 已列出 6 个关键话题及当前端点 | 本报告 1.4、9.3 | 是（链路未闭环） |
| 模块职责 | 已列出 5 个模块 | 本报告 1.5 | 否 |
| 已完成功能 | 9 项 | 本报告 1.6 | 否 |
| 未完成功能 | 9 项 | 本报告 1.7 | 是 |
| 当前运行节点/进程 | 仅 `/g1_voice_router`、`/ros_bridge` | 本报告 1.8 | 视上线目标 |
| 历史信息 vs 实测 | 已区分 JetPack/CUDA 等 | 本报告 1.9 | 否 |
| 依赖清单 | C++17、rclcpp、std_msgs、ament_index_cpp、curl、threads、nlohmann | 本报告 2.2 | 否 |
| 架构与数据流 | 已画出，含分发/查询顺序 | 本报告 3 | 否 |
| 核心源码全文 | 9 个文件全文在附录 A | `SOURCE_APPENDIX.md` | 否 |
| 源码哈希一致性 | 10 个文件三方一致 | 本报告 4.1、9.6 | 否 |
| 知识库 JSON 全文 | 798 行全文在附录 B.7 | `KNOWLEDGE_BASE_APPENDIX.md` | 否 |
| JSON schema/兜底 | 已说明，含 57/162 字节差异 | 本报告 5、附录 B | 是（兜底策略） |
| 调研资料全文 | 8 份文档在附录 D.5 | `RESEARCH_APPENDIX.md` | 否 |
| 事实映射 | 35 行映射表 | 附录 D.2 | 是（表述强度） |
| 来源元数据 | 15 个来源，A=13/B=1/C=1 | 附录 D.3 | 否 |
| 代码 diff | 5 修改、4 新增、1 未变 | `CODE_DIFF_APPENDIX.md` | 否 |
| 原业务行为影响 | LAB_INTRO 本地化；其余分支/队列/接口未变 | 本报告 7、附录 C | 否 |
| 构建结果 | 退出码 0，31.939 秒 | 附录 E.1 | 否 |
| 单元测试 | 37/37 + 25/25 PASS | 附录 E.2/E.3 | 否 |
| CTest | 2/2 PASS，退出码 0 | 附录 E.1.4 | 否 |
| `colcon test-result` | 2 tests, 0 failures, 退出码 0 | 附录 E.1.5 | 否 |
| 运行时测试 | 默认域 18 条 + 隔离域 2 条 | 附录 E.4/E.5 | 是（覆盖不足） |
| 真实端到端 | **未测试** | 附录 E.6 | 是（上线前） |
| 部署路径 | 符号链接到 build/源码 | 本报告 9.5 | 是（脆弱性） |
| 当前参数 | `knowledge_file`、模型、URL、60 秒 | 本报告 9.4 | 否 |
| 潜在问题 | 18 项，含 8 项已确认 | 本报告 10 | 是 |
| 未完成事项 | 20 项 | 本报告 11 | 是 |
| 修改运行中系统 | **未修改** | 本报告 13 | 否 |
| 重新编译/重启 | **未执行** | 本报告 13 | 否 |
| 动作/语音播放 | **未执行** | 本报告 13 | 否 |
| 凭据泄露 | 未发现，报告不含凭据 | 本报告 9.8 | 否 |

---

## 13. 本轮未执行动作声明

本轮严格按“只收集、读取、比对、整理”执行：

- **未修改** G1 上任何源码、JSON、CMake、package.xml、systemd、DDS、防火墙、Ollama 配置。
- **未停止、未重启** 正在运行的 Voice Router（PID 10751/10753）。
- **未重新编译**，未重新运行 `colcon build` / `colcon test` / `colcon test-result`；测试结论全部来自既有原始日志。
- **未向** `/g1/asr_text`、`/g1/tts`、`/g1/action_request` 发布任何消息。
- **未启动** ASR 桥、TTS 节点、动作执行器。
- **未执行** 动作请求，**未播放** 语音。
- **未修改** 网络、防火墙、DDS、Ollama。
- SSH 仅使用既有密钥；未生成新密钥、未修改 SSH 服务配置。
- 报告不含私钥、密码、Token、API Key 或完整 SSH 配置文件。
- 本轮新增的本地文件仅为审查包（报告、附录、快照、证据副本），未写入 G1。
- 域 42/43 的临时日志已在历史测试中删除，本报告明确标注“仅会话记录，不可复核”，没有把它们伪装成可复核的原始文件。

---

## 附录索引

| 附录 | 内容 | 文件 |
|---|---|---|
| A | G1 当前部署源码全文（9 个文件，逐字） | [SOURCE_APPENDIX.md](SOURCE_APPENDIX.md) |
| B | 知识库完整 JSON（798 行）与结构说明 | [KNOWLEDGE_BASE_APPENDIX.md](KNOWLEDGE_BASE_APPENDIX.md) |
| C | 备份 → 当前部署的 unified diff 与影响分析 | [CODE_DIFF_APPENDIX.md](CODE_DIFF_APPENDIX.md) |
| D | 调研资料全文、35 行事实映射、15 个来源元数据 | [RESEARCH_APPENDIX.md](RESEARCH_APPENDIX.md) |
| E | 构建/CTest/单元/运行时时完整测试证据 | [TEST_EVIDENCE_APPENDIX.md](TEST_EVIDENCE_APPENDIX.md) |
| — | 审查包总索引、文件清单、哈希、阅读顺序 | [INDEX.md](INDEX.md) |

---

*本报告只报告实际检查结果。所有“已确认问题”均有源码、测试或运行态证据；所有“静态复现”均明确标注不是运行时；所有“未测试”均未写成“已验证”。*
