# G1-EDU 实验室知识库接入与部署报告

- 日期：2026-10-31
- 项目：基于 ROS 2 的 G1-EDU 校园实验室智能接待与讲解系统 V1.0
- 结论：已完成 C++17 本地知识库、G1 原生 ARM64 编译、单元测试和隔离 ROS 2 运行时验证。`LAB_INTRO` 已完全本地处理；`GENERAL_QA` 仍走 Ollama。未执行真实 TTS 播放、动作执行和真实 ASR 链路。

## 验证状态说明

- **实际执行**：G1 原生 `colcon build`、G1 原生 `colcon test`、模拟 `/g1/asr_text` 并监听 `/g1/tts`、缺失知识库启动、`knowledge_file` 参数覆盖、进程/话题审计。
- **仅静态或单元测试**：JSON 结构校验、损坏 JSON、错误 schema、空字段、关键词匹配、无匹配兜底、答案不含 URL。
- **未执行**：真实麦克风 ASR、真实 TTS 发声、Action Executor、机器人动作、GREETING 运行时挥手测试。

---

## 1. SSH 主机与架构

- 使用已有 SSH 配置别名 `192.168.123.164`，未生成新密钥、未修改 SSH 配置、未输出或复制私钥。
- 连接检查结果：
  - `hostname`：`ubuntu`
  - `uname -m`：`aarch64`
  - 系统：Ubuntu 22.04.5
  - ROS 2：Humble
  - 机器人：Unitree G1-EDU，Jetson Orin NX
- 远程工作区：`/home/unitree/unitree_ros2/example`
- 远程包：`/home/unitree/unitree_ros2/example/g1_voice_router`
- 当前运行的 Voice Router（截至报告时间）：
  - 包装进程 PID `10751`
  - 节点进程 PID `10753`
- 远程包在 Git 中为未跟踪目录（`?? g1_voice_router/`），因此不能依赖 `git restore`，已做物理备份。

## 2. 原始调研资料路径

本地调研资料位于 `/home/mick/Mick_Grey/teach/lab_research/`：

- `LAB_RESEARCH_REPORT.md`
- `LAB_PROFILE.md`
- `SOURCES.md`
- `FACT_CHECK.md`
- `FAQ_DRAFT.md`
- `UNVERIFIED.md`
- `TALK_SCRIPTS.md`
- `README.md`

原始远程包已复制到本地只读参考目录：

- `/home/mick/Mick_Grey/teach/g1_kb_work/original/g1_voice_router/`

未将上述调研文档、`UNVERIFIED.md`、私人信息或整个目录复制到 G1。只向 G1 传输了运行所需的 `config/lab_knowledge.json` 和项目源码文件。

## 3. 知识库来源摘要

最终 JSON 使用 15 个来源：

- 13 个 A 级来源
- 1 个 B 级来源：S14（CCBR 2025 会议公开简介）
- 1 个 C 级来源：S04（华龙网报道，用于中心成立信息，与 A 级来源互证）

主要来源包括：

- S01 重庆市科技局 2025 年拟认定重庆市重点实验室名单
- S02 学院市级重点实验室获批新闻
- S04 脑机交互智能研究中心成立报道
- S05 学院脑机中心成立新闻
- S06 学院简介
- S07 2025 年本科生招生简章
- S09 建院 8 周年产教融合开放日活动
- S10、S11、S12、S13 教师公开页面
- S14 CCBR 2025 公开会议简介
- S16 第 28 届中国机器人及人工智能大赛获奖新闻
- S21 学院首页

内容处理原则：

- 只写入已核验、可公开介绍、无未解决冲突的事实。
- 未写入“智慧医疗/大健康”三领域的冲突表述。
- 播报答案中未写入“首个”等宣传性表述；项目元数据 `summary` 保留了来源中的“具身智能机器人方向第一批”分类，该字段不参与播报。
- 未写入具体设备品牌、型号、数量。
- 未写入中心成果、伦理审批、参观预约、G1 归属等未核实内容。
- 项目状态明确写为“按公开周期推断”，并提示以学院正式介绍为准。
- 所有未知信息均使用明确兜底，不编造。

最终条数：

- `faq`：20
- `research_directions`：3
- `projects`：1
- `equipment`：1
- `categories`：5
- `labs`：3
- `sources`：15

全部 15 个来源均被引用，无缺失引用、无未使用来源。所有播报 `answer` 均不含 URL；URL 只保留在 `sources` 元数据中。

## 4. JSON 数据结构

文件：`/home/mick/Mick_Grey/teach/g1_kb_work/g1_voice_router/config/lab_knowledge.json`

顶层字段：

```text
schema_version        1
knowledge_version     1.0.0
updated_at            2026-10-31
school_name
college_name
lab_name
location
introduction
labs[]                3
research_directions[] 3
projects[]            1
equipment[]           1
categories[]          5
faq[]                 20
fallback_answer
disclaimer
sources[]             15
```

FAQ 条目字段：

```text
id
questions[]
keywords[]
answer
source_ids[]
category
```

关键约束：

- `schema_version` 必须为整数且等于 1。
- 来源 ID 在整个文件中唯一；条目中的 `source_ids` 必须引用已存在的来源。
- 条目 ID 唯一。
- `answer`、`introduction`、`location`、`fallback_answer` 中禁止出现 URL。
- 未知字段留空；空字段加载后在查询阶段直接走兜底，不会被播报。

## 5. 新增文件

本地：

- `/home/mick/Mick_Grey/teach/g1_kb_work/g1_voice_router/include/g1_voice_router/knowledge_base.hpp`
- `/home/mick/Mick_Grey/teach/g1_kb_work/g1_voice_router/src/knowledge_base.cpp`
- `/home/mick/Mick_Grey/teach/g1_kb_work/g1_voice_router/config/lab_knowledge.json`
- `/home/mick/Mick_Grey/teach/g1_kb_work/g1_voice_router/test/knowledge_base_test.cpp`

G1 对应路径：

- `/home/unitree/unitree_ros2/example/g1_voice_router/include/g1_voice_router/knowledge_base.hpp`
- `/home/unitree/unitree_ros2/example/g1_voice_router/src/knowledge_base.cpp`
- `/home/unitree/unitree_ros2/example/g1_voice_router/config/lab_knowledge.json`
- `/home/unitree/unitree_ros2/example/g1_voice_router/test/knowledge_base_test.cpp`

## 6. 修改文件

- `src/voice_router.cpp`
  - 引入 `ament_index_cpp` 和 `knowledge_base.hpp`。
  - 构造时解析 `knowledge_file` 参数并加载知识库。
  - 增加 `LAB_INTRO` 本地知识库分支。
  - 增加 `knowledge_file_` 和 `knowledge_base_` 成员。
- `src/intent_recognizer.cpp`
  - 扩展 `LAB_INTRO` 的领域关键词，使项目、团队、获奖、地址、人物、具身智能平台等问题能进入本地知识库。
  - 其余六类意图逻辑未改动。
- `test/intent_recognizer_test.cpp`
  - 从 14 条扩展到 25 条，增加学院项目、团队、获奖、成立时间、地址、开放平台、人物、设备等路由用例。
- `CMakeLists.txt`
  - 增加 `ament_index_cpp`。
  - 增加 `knowledge_base` 静态库。
  - 安装 JSON 到 `share/g1_voice_router/config`。
  - 增加 CTest 单元测试。
- `package.xml`
  - 增加 `<depend>ament_index_cpp</depend>`。

未修改：

- `include/g1_voice_router/intent_recognizer.hpp`
- ASR、TTS、Action Executor 节点
- Ollama URL、模型名和请求参数
- 系统级环境变量、DDS、JetPack、CUDA、防火墙、启动脚本

## 7. 关键代码说明

### 7.1 知识库加载与校验

`src/knowledge_base.cpp:270-566`

- 二进制读取 JSON，容忍 UTF-8 BOM。
- `nlohmann::json::parse` 捕获异常并返回明确错误。
- 校验根对象、`schema_version`、全部必需数组和字符串字段。
- 校验来源 ID 唯一、条目来源引用有效、条目 ID 唯一。
- 校验 FAQ 的问题、关键词、答案、来源非空。
- 拒绝答案中的 URL。
- 加载成功后数据常驻内存，后续查询不再读文件。

### 7.2 文本归一化

`src/knowledge_base.cpp:28-59`

- 去除中英文标点、空格、换行、制表符。
- 只对 ASCII 字母做小写转换，不破坏 UTF-8 中文字节。
- FAQ 问题、关键词和用户输入使用同一套归一化。

### 7.3 查询顺序

`src/knowledge_base.cpp:568-705`

1. FAQ 问题精确匹配。
2. 输入中包含的 FAQ 问题匹配（容忍“你好，介绍一下你们实验室”这类前缀）。
3. FAQ 关键词匹配。
4. 研究方向、项目、设备条目关键词匹配。
5. 类别关键词匹配。
6. 全部失败返回兜底。

关键词匹配规则 `src/knowledge_base.cpp:75-110`：

- 至少命中两个不同关键词，或命中一个长度不少于 4 个汉字的关键词。
- 分数按关键词长度和命中数量累加，避免单个宽泛关键词误触发。

### 7.4 启动容错

`src/voice_router.cpp:48-83`

- 通过 `ament_index_cpp::get_package_share_directory("g1_voice_router")` 获取 share 路径。
- 默认路径为 `<share>/config/lab_knowledge.json`。
- 支持 ROS 参数 `knowledge_file` 覆盖。
- 加载失败只记录 `RCLCPP_ERROR`，节点继续启动。
- 知识库未加载时，`LAB_INTRO` 返回硬编码安全兜底。

### 7.5 LAB_INTRO 本地分支

`src/voice_router.cpp:312-328`

```cpp
if (intent_result.intent == g1_voice_router::Intent::LAB_INTRO) {
  const auto result = knowledge_base_.query(text);
  publish_reply(result.answer);
  RCLCPP_INFO(..., "LAB_INTRO local knowledge base completed: "
                   "found=%s category=%s matched_id=%s", ...);
  return;
}
```

该分支在 `publish_reply` 后直接 `return`，不会进入 Ollama 队列。

### 7.6 意图关键词扩展

`src/intent_recognizer.cpp:150-176`

新增 LAB_INTRO 关键词包括：

```text
脑机、具身智能、重点实验室、开放平台、赛力斯、在研项目、揭榜挂帅、
科研团队、学院有哪些团队、学院学生拿过什么奖、学院竞赛、
学院什么时候成立、学院成立、学院地址、学院在哪里、学院主要研究、
肖汉光、刘宏、有机器狗吗
```

目的：让知识库中已有的项目、团队、获奖、地址、人物、开放平台等问题实际进入 `LAB_INTRO`，而不是被误送到 Ollama。未加入单独的“学院”“项目”等过宽词，避免把“学院有哪些专业”“机器狗多少钱”等普通问题误吞。

## 8. CMake / package.xml 变更

`CMakeLists.txt`：

- `find_package(ament_index_cpp REQUIRED)`
- `add_library(knowledge_base STATIC src/knowledge_base.cpp)`
- 知识库库目标包含：
  - `include`
  - `../src/include`（复用工作区已有 `nlohmann/json.hpp`）
- `voice_router` 链接 `knowledge_base`
- `ament_target_dependencies(voice_router ... ament_index_cpp)`
- 安装 JSON：

```cmake
install(
  FILES config/lab_knowledge.json
  DESTINATION share/${PROJECT_NAME}/config
)
```

- `include(CTest)` + `BUILD_TESTING`：
  - `knowledge_base_test`
  - `intent_recognizer_test`

`package.xml`：

```xml
<depend>ament_index_cpp</depend>
```

构建命令：

```bash
source ~/unitree_ros2/setup.sh
source ~/unitree_ros2/example/install/setup.bash
cd ~/unitree_ros2/example
colcon build --packages-select g1_voice_router
```

## 9. 备份路径

唯一备份目录：

```text
/home/unitree/g1_backups/g1_voice_router_20261031_132307_before_kb/
```

备份内容：

- `voice_router.cpp`
- `CMakeLists.txt`
- `package.xml`
- `intent_recognizer.hpp`
- `intent_recognizer.cpp`
- `intent_recognizer_test.cpp`

备份使用 `cp -a --no-clobber`，未覆盖旧备份；包内原有 `.bak` 文件未改动。备份目录创建时间为 2026-10-31 13:23:07 UTC。

示例原始哈希：

```text
voice_router.cpp  491fc3b9efafebd3e85278981a0ec3383d9a6f726b95c06392a8c61c69b14f2a
CMakeLists.txt    b9ee317568bfc11eb09b720fc857866d165ecd626784d9f4f1e57b4c232c3225
```

## 10. 独立测试结果

### 10.1 本地 x86_64 语法与逻辑测试

使用本地 g++ 13.3.0，链接 G1 上已有的 `nlohmann` 头文件目录，仅做语法和逻辑验证：

- `knowledge_base_test`：37/37 PASS
- `intent_recognizer_test`：25/25 PASS

本地 x86 结果不作为部署证明，仅用于提前发现编译和逻辑错误。

### 10.2 G1 原生 aarch64 测试

最终一次 `colcon test --packages-select g1_voice_router`：

```text
1/2 Test #1: knowledge_base_test .............. Passed
2/2 Test #2: intent_recognizer_test ........... Passed
100% tests passed, 0 tests failed out of 2
```

详细计数：

- KnowledgeBase：Passed 37，Failed 0
- IntentRecognizer：Passed 25，Failed 0

覆盖用例包括：

- 正常加载、缺失文件、损坏 JSON、错误 schema
- 空字段最小 JSON
- 实验室介绍、研究方向、项目、设备、地点、团队、获奖
- FAQ 精确命中、中文同义说法、长句包含问题
- 未核实设备数量不编造
- 未知团队人数兜底
- 无匹配和空输入兜底
- 播报答案不含 URL

## 11. G1 原生编译结果

实际执行：

```bash
colcon build --packages-select g1_voice_router
```

结果：

- 首次构建：`Finished <<< g1_voice_router [33.0s]`，退出码 0
- 最终构建：`Finished <<< g1_voice_router [32.1s]`，退出码 0
- `colcon test-result --verbose`：2 tests, 0 errors, 0 failures, 0 skipped

安装产物：

```text
/home/unitree/unitree_ros2/example/install/g1_voice_router/lib/g1_voice_router/voice_router
/home/unitree/unitree_ros2/example/install/g1_voice_router/share/g1_voice_router/config/lab_knowledge.json
```

说明：当前工作区的 install 使用了符号链接布局，JSON 安装路径是指向源码的符号链接，`sha256sum` 与源文件一致：

```text
cbe711e3b5c19115dc9d85fe37c701829ee3ad37203a3a4337efa5263d5acea7
```

`ament_index_cpp::get_package_share_directory` 能正常解析该路径，运行时加载成功。

## 12. ROS 2 运行时隔离测试

测试环境：

- 未启动 TTS 节点。
- 未启动 Action Executor。
- 未启动 ASR bridge。
- 未发送 GREETING，避免触发挥手动作。
- 只模拟发布 `/g1/asr_text`，监听 `/g1/tts`。
- `/g1/action_request` 订阅者为 0。
- `/g1/tts` 的唯一订阅者是历史遗留的 `ros2 topic echo` 进程 PID 4216，不是 TTS 播放节点。

运行时结果：

| 模拟输入 | 识别意图 | 知识库结果 | `/g1/tts` 字节数 | Ollama 队列 |
|---|---|---|---:|---|
| 你好，介绍一下你们实验室 | LAB_INTRO | found=true, introduction, faq_intro | 412 | 无 |
| 你们实验室主要研究什么 | LAB_INTRO | found=true, research_directions | 348 | 无 |
| 实验室有多少人 | LAB_INTRO | found=false, fallback | 162 | 无 |
| 实验室有哪些设备 | LAB_INTRO | found=true, equipment, faq_equipment | 180 | 无 |
| 学院有什么在研项目 | LAB_INTRO | found=true, projects, faq_project | 319 | 无 |
| 脑机中心负责人是谁 | LAB_INTRO | found=true, people, faq_bci_leaders | 168 | 无 |
| 学院有哪些科研团队 | LAB_INTRO | found=true, college, faq_teams | 192 | 无 |
| 学院学生拿过什么奖 | LAB_INTRO | found=true, college, faq_awards | 181 | 无 |
| 学院地址在哪里 | LAB_INTRO | found=true, location, faq_location | 162 | 无 |
| 具身智能开放平台是什么 | LAB_INTRO | found=true, labs, faq_platform | 216 | 无 |
| 肖汉光教授是谁 | LAB_INTRO | found=true, people, faq_xiao | 303 | 无 |
| 宇树机器人多少钱？ | GENERAL_QA | 不适用 | 166 | 有，pending=1，Ollama 约 3.7 秒 |

代表性实际播报文本：

```text
你好，介绍一下你们实验室
→ 重庆理工大学两江人工智能学院设有脑机交互智能研究中心和人形机器人具身智能感知与自主学习重庆市重点实验室，并建设具身智能开放平台。……

你们实验室主要研究什么
→ 公开资料中，脑机交互智能研究中心有五个方向：神经信号感知、脑网络组应用、脑病智能诊疗、脑机自然交互、类脑智能系统。市级重点实验室研究复杂场景下的抗干扰多模态融合学习、人机环境交互的域泛化和抗知识遗忘持续学习，以及技能可迁移自主学习。

实验室有多少人
→ 这个信息目前没有公开资料，我不能确认。您可以问我实验室的研究方向、重点实验室、具身智能开放平台或者学院地址。
```

隔离补充测试：

- **缺失知识库**：在 `ROS_DOMAIN_ID=42` 下启动临时节点，`knowledge_file:=/tmp/definitely_missing_kb.json`。
  - 启动日志：`KnowledgeBase load failed: cannot open knowledge file: ...`
  - 节点仍正常启动。
  - `LAB_INTRO` 返回硬编码兜底，`/g1/tts` 57 字节。
  - 测试节点已停止，生产节点未受影响。
- **参数覆盖**：在 `ROS_DOMAIN_ID=43` 下启动临时节点，`knowledge_file:=/tmp/lab_knowledge_override.json`。
  - 启动日志：`KnowledgeBase loaded: /tmp/lab_knowledge_override.json (FAQ=20, ...)`
  - `实验室在哪里` 返回 location 答案，162 字节。
  - 临时 JSON 和日志已删除，测试节点已停止。

`GENERAL_QA` 的 Ollama 路径日志：

```text
Intent recognized: GENERAL_QA | text: 宇树机器人多少钱
ASR text queued for Ollama; pending=1
Querying Ollama model: qwen3-vl:4b-instruct
Reply published to /g1/tts (166 bytes)
```

注意：该价格回答由 Ollama 模型生成，不属于知识库事实，未经实验室资料核验。

## 13. 未完成 / 未执行事项

以下项目**未执行**，不能声称已验证：

- 真实麦克风输入（`/audio_msg` → `/g1/asr_text`）未测试，因为 `g1_asr_bridge` 未运行，且测试期间未启动它。
- 真实 TTS 节点未启动，实际扬声器发声未验证；只验证了 `/g1/tts` 话题消息。
- Action Executor 未启动，机器人动作未执行；未发送任何动作请求。
- GREETING 的运行时挥手路径未测试，以避免机器人意外挥手；仅通过单元测试验证意图分类，静态代码保持原样。
- 损坏 JSON、错误 schema 在节点启动时的行为未单独做运行时测试；已由 `knowledge_base_test` 覆盖，且与缺失文件共用同一加载失败分支。
- 未修改 JetPack、CUDA、DDS、系统启动脚本、防火墙、Ollama 服务、系统级环境变量；未执行 `apt upgrade`；未重启机器人。
- 未在机器人上保留调研文档、私人资料、密钥或无关配置。

当前生产状态：

- 新的 `g1_voice_router` 正在运行（PID 10751/10753）。
- 无 TTS、Action Executor、ASR bridge 进程。
- `/g1/action_request` 订阅者为 0。
- `/g1/tts` 的订阅者为历史 `ros2 topic echo` 进程，不是播放节点。

## 14. 完整启动命令

```bash
source ~/unitree_ros2/setup.sh
source ~/unitree_ros2/example/install/setup.bash
ros2 run g1_voice_router voice_router
```

可选：显式指定知识库路径

```bash
ros2 run g1_voice_router voice_router --ros-args \
  -p knowledge_file:=$HOME/unitree_ros2/example/install/g1_voice_router/share/g1_voice_router/config/lab_knowledge.json
```

完整构建命令：

```bash
source ~/unitree_ros2/setup.sh
source ~/unitree_ros2/example/install/setup.bash
cd ~/unitree_ros2/example
colcon build --packages-select g1_voice_router
```

停止当前节点：

```bash
kill -INT 10753
```

（节点已在运行；如需重新启动，先停止旧进程，再执行上面的启动命令。）
