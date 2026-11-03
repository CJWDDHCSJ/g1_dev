# G1 KnowledgeBase 审查包 · 总索引

> 审查包根目录：`/home/mick/Mick_Grey/teach/g1_kb_review/`
> 生成日期：2026-10-31
> 用途：供 ChatGPT 做第二轮独立工程审查

---

## 1. 这是什么

这是一套**只读审查包**，包含：

- G1 当前部署源码与改造前备份的逐字副本；
- 完整知识库 JSON；
- 8 份调研资料全文与 35 行事实映射；
- 5 个修改文件的 unified diff；
- 构建、CTest、37 + 25 条单元测试、18 + 2 条 ROS 运行时测试的原始证据；
- 2026-10-31 14:01Z 的部署环境与运行态快照；
- 主报告中的 18 项潜在问题与 20 项未完成事项。

**没有**修改 G1 上的任何文件，**没有**重新编译，**没有**重启节点，**没有**播放语音或执行动作。

---

## 2. 推荐阅读顺序

| 顺序 | 文件 | 读什么 |
|---:|---|---|
| 1 | `G1_KB_REVIEW_BUNDLE.md` | 先读第 0 节结论、第 10 节问题、第 12 节汇总表 |
| 2 | `TEST_EVIDENCE_APPENDIX.md` | 核对测试结论是否被夸大；重点看 E.0、E.4、E.6 |
| 3 | `CODE_DIFF_APPENDIX.md` | 核对改造是否影响原业务行为 |
| 4 | `SOURCE_APPENDIX.md` | 逐字阅读 9 个源码/构建文件 |
| 5 | `KNOWLEDGE_BASE_APPENDIX.md` | 逐字阅读 798 行 JSON 与查询逻辑 |
| 6 | `RESEARCH_APPENDIX.md` | 核对 35 行事实映射与来源等级 |
| 7 | `INDEX.md`（本文件） | 查文件、哈希、证据来源与限制 |

如果只看一份：读主报告第 0、10、12 节。如果要独立复核：按 2 → 3 → 4 → 5 → 6 顺序。

---

## 3. 主交付物清单

| 文件 | 字节 | 行数 | SHA256 | 内容 |
|---|---:|---:|---|---|
| `G1_KB_REVIEW_BUNDLE.md` | 70,764 | 1,118 | `cfaf33ed01eb32de969a6a3c037a23e43cb28af0c60fdbf774f7a9599ee97f34` | 主报告：第 1–11 节 + 汇总表 + 未执行声明 |
| `SOURCE_APPENDIX.md` | 67,923 | 2,618 | `02be064833b536bddf6af978cc8b5e4671d5765153ed20a42fe4cb4b2d77a366` | 附录 A：9 个文件全文 |
| `KNOWLEDGE_BASE_APPENDIX.md` | 39,099 | 1,019 | `d761d524cfbc2ce4f1bfe3dc7e05ea5f4c44be29ec5244bd979e86ad99ecaf00` | 附录 B：JSON 全文与结构 |
| `CODE_DIFF_APPENDIX.md` | 14,517 | 389 | `24fea7d5dc0ccca3c2fb9a3b565c814196c1c72bc4e532c0b7dac9ee8b257cce` | 附录 C：diff 与影响分析 |
| `RESEARCH_APPENDIX.md` | 147,315 | 2,170 | `303f86f0d3d407542cf646ad91fe3c23c85d0282ee64e861e9911250ea24893a` | 附录 D：调研全文与事实映射 |
| `TEST_EVIDENCE_APPENDIX.md` | 50,228 | 674 | `748717e35c911dd9dde8f95ede16f1fdf95b26a6de420c3504b5c6b087c6f77a` | 附录 E：完整测试证据 |
| `INDEX.md` | 本文件 | — | — | 总索引 |

> 注意：主报告与附录的字节数是生成时实测值；后续若编辑任何文件，哈希会变化，请以 `SHA256SUMS` 重新计算为准。

---

## 4. 目录结构

```text
g1_kb_review/
├── G1_KB_REVIEW_BUNDLE.md              # 主报告
├── INDEX.md                            # 本文件
├── SHA256SUMS                          # 主交付物、快照、调研副本的校验清单
├── SOURCE_APPENDIX.md                  # 附录 A
├── KNOWLEDGE_BASE_APPENDIX.md          # 附录 B
├── CODE_DIFF_APPENDIX.md               # 附录 C
├── RESEARCH_APPENDIX.md                # 附录 D
├── TEST_EVIDENCE_APPENDIX.md           # 附录 E
├── remote_snapshot/                    # G1 当前部署的只读快照
│   └── g1_voice_router/
├── backup_snapshot/                    # 改造前备份的只读副本
├── diffs/                              # 5 个 unified diff 原始文件
├── research_materials/                 # 8 份调研文档副本
└── evidence/
    ├── build_test/                     # 构建/CTest 原始日志
    ├── deploy/                         # 环境/话题/进程/路径快照
    ├── runtime/                        # Voice Router 运行日志与 TTS 捕获
    ├── static/                         # 本地静态查询复现
    └── test_result_1343/               # colcon test-result logger
```

---

## 5. 文件逐项说明

### 5.1 主交付物

| 文件 | 说明 |
|---|---|
| `G1_KB_REVIEW_BUNDLE.md` | 主报告。第 1 节项目现状；第 2 节环境；第 3 节架构；第 4 节源码索引；第 5 节 JSON 索引；第 6 节调研/来源；第 7 节 diff；第 8 节测试；第 9 节部署运行态；第 10 节 18 项风险；第 11 节 20 项未完成；第 12 节汇总表；第 13 节未执行声明 |
| `SOURCE_APPENDIX.md` | 9 个源码/构建文件全文，每个文件独立标题、代码围栏、远端路径、状态、SHA256、行数；A.10 列出 3 个远端 `.bak` 文件 |
| `KNOWLEDGE_BASE_APPENDIX.md` | JSON 全文（B.7）；B.1 加载路径；B.2 顶层结构/统计；B.3 条目结构；B.4 加载校验；B.5 空字段；B.6 查询顺序与兜底 |
| `CODE_DIFF_APPENDIX.md` | C.1 变更总览；C.2 五个文件的 unified diff；C.3 关键变更目的与行为影响；C.4 变更性质结论 |
| `RESEARCH_APPENDIX.md` | D.1 状态图例；D.2 35 行事实映射；D.3 15 个来源元数据（含 URL/日期/摘录）；D.4 事实状态专项说明；D.5 八份调研文档全文 |
| `TEST_EVIDENCE_APPENDIX.md` | E.0 证据分级；E.1 构建/CTest 原始输出；E.2 37 条 KB 断言；E.3 25 条意图断言；E.4 18 条默认域运行；E.5 域 42/43；E.6 未测试清单；E.7 证据来源矩阵；E.8 覆盖结论 |

### 5.2 `remote_snapshot/g1_voice_router/` — G1 当前部署源码快照

| 文件 | 字节 | 说明 |
|---|---:|---|
| `CMakeLists.txt` | 1,967 | 当前构建配置（修改） |
| `package.xml` | 524 | 当前包清单（修改） |
| `config/lab_knowledge.json` | 27,206 | 当前知识库（新增，798 行） |
| `include/g1_voice_router/knowledge_base.hpp` | 1,132 | 知识库接口（新增） |
| `include/g1_voice_router/intent_recognizer.hpp` | 1,117 | 意图接口（未修改） |
| `src/knowledge_base.cpp` | 20,032 | 知识库实现（新增，772 行） |
| `src/intent_recognizer.cpp` | 7,550 | 意图实现（修改） |
| `src/voice_router.cpp` | 16,615 | 路由器（修改，688 行） |
| `test/knowledge_base_test.cpp` | 10,213 | KB 测试（新增，362 行） |
| `test/intent_recognizer_test.cpp` | 2,657 | 意图测试（修改，86 行） |
| `src/voice_router.cpp.bak` | 2,351 | 远端遗留历史备份，不编译 |
| `src/voice_router.cpp.before_action` | 10,471 | 远端遗留历史备份，不编译 |
| `src/voice_router.cpp.before_local_reception` | 13,304 | 远端遗留历史备份，不编译 |

### 5.3 `backup_snapshot/` — 改造前备份副本

| 文件 | 字节 | 说明 |
|---|---:|---|
| `CMakeLists.txt` | 764 | 改造前构建配置 |
| `package.xml` | 489 | 改造前包清单 |
| `intent_recognizer.hpp` | 1,117 | 与当前逐字节一致 |
| `intent_recognizer.cpp` | 7,061 | 改造前意图实现 |
| `intent_recognizer_test.cpp` | 2,031 | 改造前意图测试 |
| `voice_router.cpp` | 14,629 | 改造前路由器 |

> 备份原件位于 G1：`/home/unitree/g1_backups/g1_voice_router_20261031_132307_before_kb/`。

### 5.4 `diffs/` — 原始 unified diff

| 文件 | 字节 | 说明 |
|---|---:|---|
| `CMakeLists.txt.diff` | 2,127 | +45 / -1 |
| `package.xml.diff` | 255 | +1 / -0 |
| `voice_router.diff` | 3,014 | +55 / -0 |
| `intent_recognizer.cpp.diff` | 779 | +20 / -1 |
| `intent_recognizer_test.cpp.diff` | 950 | +11 / -0 |

> 由 `diff -u 备份 当前` 生成，仅替换路径前缀；hunk 内容未改。

### 5.5 `research_materials/` — 8 份调研文档副本

| 文件 | 字节 | 说明 |
|---|---:|---|
| `README.md` | 4,155 | 调研资料索引 |
| `LAB_PROFILE.md` | 15,726 | 实验室公开档案 |
| `LAB_RESEARCH_REPORT.md` | 24,197 | 完整调研报告 |
| `SOURCES.md` | 20,326 | 来源清单与摘录 |
| `FACT_CHECK.md` | 13,854 | 逐条事实核验 |
| `FAQ_DRAFT.md` | 16,604 | FAQ 草稿 |
| `UNVERIFIED.md` | 11,400 | 未确认项 |
| `TALK_SCRIPTS.md` | 13,291 | 讲解话术 |

> 与 `/home/mick/Mick_Grey/teach/lab_research/` 原件 SHA256 一致。

### 5.6 `evidence/build_test/` — 构建与 CTest 原始日志

| 文件/目录 | 字节 | 说明 |
|---|---:|---|
| `build_1332/` | — | 13:32 构建的 colcon 原始日志目录 |
| `build_1332/events.log` | 22,219 | colcon 事件流（含 `JobEnded rc=0`） |
| `build_1332/logger_all.log` | 21,943 | colcon 合并日志 |
| `build_1332/g1_voice_router/command.log` | 910 | cmake build/install 命令与退出码 |
| `build_1332/g1_voice_router/streams.log` | 4,209 | 完整构建输出（35 行，31.939 秒） |
| `build_1332/g1_voice_router/stdout.log` | 2,960 | 构建 stdout |
| `build_1332/g1_voice_router/stdout_stderr.log` | 2,960 | 合并输出 |
| `build_1332/g1_voice_router/stderr.log` | 0 | 空 |
| `latest_build/` | — | `build_1332` 的逐字节副本（已用 SHA256 核对） |
| `test_1333/` | — | 13:33 `colcon test` 的原始日志目录 |
| `test_1333/events.log` | 15,367 | CTest 事件流（含 `CommandEnded returncode=0`） |
| `test_1333/logger_all.log` | 14,324 | colcon 合并日志 |
| `test_1333/g1_voice_router/command.log` | 1,400 | CTest 命令与退出码 |
| `test_1333/g1_voice_router/streams.log` | 6,585 | CTest 完整输出 |
| `test_1333/g1_voice_router/stdout.log` | 4,249 | CTest stdout |
| `test_1333/g1_voice_router/stdout_stderr.log` | 4,249 | 合并输出 |
| `test_1333/g1_voice_router/stderr.log` | 0 | 空 |
| `LastTest_20261031-1333.log` | 3,978 | **最终 37/25 测试的完整 CTest 输出（权威）** |
| `LastTest_20261031-1330.log` | 3,978 | 13:30 测试输出 |
| `LastTest_20261031-1326.log` | 3,484 | 13:26 测试输出 |
| `LastTest_20261031-1324.log` | 3,387 | 13:24 测试输出 |
| `LastTest.log` | 121 | 一个空的 CTest 会话（只有 Start/End），不作为测试证据 |
| `CTestCostData.txt` | 73 | CTest 历史耗时 |

> 13:24/13:26/13:30 的日志保留了测试数量演进过程；最终结论以 13:33 的 `LastTest_20261031-1333.log` 为准。

### 5.7 `evidence/deploy/` — 部署环境快照

| 文件 | 字节 | 说明 |
|---|---:|---|
| `current_state_20261031T1401Z.txt` | 5,868 | **最完整的运行态快照（14:01Z）**：主机、OS、L4T、节点、话题、参数、安装路径、哈希、CMake 规则、TTS verbose |
| `host_env.txt` | 490 | 主机名、内核、架构、OS |
| `rmw_env.txt` | 1,690 | 运行进程的 RMW/CycloneDDS 环境 |
| `ros_live.txt` | 7,507 | 节点与话题列表（13:43Z） |
| `topic_endpoints.txt` | 3,015 | 关键话题的端点统计 |
| `process_paths.txt` | 5,665 | 进程、可执行文件、源码路径 |
| `packages.txt` | 838 | 已安装包列表 |
| `action_interface.txt` | 614 | 动作接口定义 |
| `action_tts_src.txt` | 4,577 | 动作/TTS 源码摘录 |

### 5.8 `evidence/runtime/` — 运行时日志与 `/g1/tts` 捕获

| 文件 | 字节 | 说明 |
|---|---:|---|
| `g1_voice_router_kb_runtime.log` | 984 | runtime1（修复前 1 条） |
| `g1_voice_router_kb_runtime2.log` | 3,347 | runtime2（8 条） |
| `g1_voice_router_kb_runtime3.log` | 3,422 | runtime3（8 条） |
| `g1_voice_router_kb_final.log` | 917 | 最终生产进程（1 条） |
| `tts_lab.txt` | 173 | runtime1 第 1 条：162 字节兜底 |
| `tts_lab1.txt` | 372 | runtime2 第 1 条：**截断**（日志记录 412 字节） |
| `tts_lab2.txt` | 359 | runtime2 第 2 条：研究方向 |
| `tts_lab3.txt` | 173 | runtime2 第 3 条：兜底 |
| `tts_robot.txt` | 256 | runtime2 第 4 条：ROBOT_INTRO |
| `tts_bye.txt` | 86 | runtime2 第 5 条：GOODBYE |
| `tts_general.txt` | 177 | runtime2 第 6 条：Ollama 价格回答（19999 元） |
| `tts_proj.txt` | 170 | runtime2 第 7 条：Ollama“没有项目数据” |
| `tts_equip.txt` | 191 | runtime2 第 8 条：设备 |
| `tts_final.txt` | 330 | 项目答案；runtime3 与最终生产**归属不唯一** |
| `tts_one.txt` | 177 | runtime3 第 8 条：Ollama 价格回答（19999 元） |

> `data: ` 与 `---` 是 `ros2 topic echo` 格式，不计入消息字节数。

### 5.9 `evidence/static/` — 本地静态复现

| 文件 | 字节 | 说明 |
|---|---:|---|
| `query_replication_20261031.txt` | 13,015 | 31 条输入的查询结果；按当前 C++ 逻辑在本地 Python 中重放，**不是 ROS 运行时测试** |

### 5.10 `evidence/test_result_1343/`

| 文件 | 字节 | 说明 |
|---|---:|---|
| `logger_all.log` | 1,559 | `colcon test-result --verbose` 的 logger；解释为何没有逐条测试列表 |

---

## 6. 证据来源与可信级别

| 级别 | 类型 | 本包中的例子 | 可复核性 |
|---|---|---|---|
| L1 | 机器人原文件/原日志的逐字节副本 | `remote_snapshot/`、`backup_snapshot/`、`evidence/build_test/*`、`evidence/runtime/*.log`、`evidence/deploy/*` | 高：本地可直接读取；与远端哈希核对过 |
| L2 | 会话中捕获的命令 stdout，无独立文件 | `colcon test-result` 的 `Summary: 2 tests...`、域 42/43 输出 | 中：本轮无法在机器人上重放；报告中已标注 |
| L3 | 本地静态分析/重放 | `evidence/static/query_replication_20261031.txt` | 中：逻辑可复核，但不等于运行时 |
| L4 | 审查者推断/判断 | 第 10 节的风险评级、优先级建议 | 低：明确标注为分析意见 |

**关键限制：**

1. 域 42/43 的原始 `/tmp` 日志已删除，只有会话记录；报告中未把它们写成“可复核原始日志”。
2. `tts_lab1.txt` 截断；完整 412 字节只能从 JSON 读取，不能当作播报证据。
3. `tts_final.txt` 无时间戳，归属不唯一。
4. `colcon test-result` 没有逐条列出测试；逐条证据来自 `LastTest_20261031-1333.log`。
5. 外层 `colcon build`/`colcon test` 的命令与汇总行来自会话记录，没有 colcon 顶层日志副本；13:24 运行为 35+14 条断言（完整控制台输出），13:33 运行为 37+25 条断言（外层经 `grep` 过滤），包级 cmake/ctest 日志完整。
6. 真实 ASR/TTS/动作链路未运行；所有运行时测试都是文本注入 + 话题捕获。

---

## 7. 任务要求 → 交付位置映射

| 任务要求 | 交付位置 |
|---|---|
| 项目名称、用途、硬件、软件、工作空间、包、话题、职责、已完成/未完成、当前进程 | 主报告第 1 节 |
| 历史信息与本轮实测区分 | 主报告 1.9、2.3、9.1 |
| 9 个核心文件全文、独立标题、路径、状态、SHA256 | 附录 A（A.1–A.9） |
| 知识库 JSON 全文、schema、字段、空值、兜底、安装路径 | 附录 B；主报告第 5 节 |
| 8 份调研文档全文 | 附录 D.5 |
| 35 行事实映射表 | 附录 D.2 |
| 每个重要来源的标题、机构、URL、日期、事实、摘录 | 附录 D.3 |
| 备份 vs 当前文件清单、diff、目的、行为影响 | 附录 C；主报告第 7 节 |
| `enqueue_user_text()` 顺序、KB 初始化、LAB_INTRO、Ollama 流、意图关键词、路径解析 | 主报告 7.4–7.5；附录 C.3 |
| 37 条 KB 测试逐项 | 附录 E.2 |
| 25 条意图测试逐项 | 附录 E.3 |
| `colcon build`/`test`/`test-result` 输出与退出码 | 附录 E.1 |
| 运行时测试（LAB_INTRO、研究、FAQ、设备、项目、人物、团队、地址、未知兜底、GENERAL_QA） | 附录 E.4 |
| 单测/隔离 ROS/真机测试区分 | 附录 E.0、E.4.1、E.6；主报告 8.3 |
| 未跑测试标注“未测试” | 附录 E.6；主报告 8.5、11 |
| 部署环境、路径、JSON 加载路径、CMake、参数、话题、启动命令 | 主报告第 9 节；`evidence/deploy/current_state_20261031T1401Z.txt` |
| 至少 12 个潜在问题领域 | 主报告第 10 节（18 项） |
| 问题标签（已确认/潜在/暂未发现/需进一步测试） | 主报告 10.1、10.2 |
| 第 1–11 节 + 汇总表 | 主报告；汇总表在第 12 节 |
| 拆分时提供 INDEX 说明所有文件 | 本文件 |
| 不修改、不重启、不编译、不播放、不发动作 | 主报告第 13 节；本文件第 8 节 |

---

## 8. 本轮未执行动作

- 未修改 G1 上任何源码、JSON、CMake、package.xml、systemd、DDS、防火墙、Ollama 配置。
- 未停止/重启 Voice Router（PID 10751/10753）。
- 未重新编译，未重新运行 `colcon build`/`colcon test`/`colcon test-result`。
- 未向 `/g1/asr_text`、`/g1/tts`、`/g1/action_request` 发布消息。
- 未启动 ASR 桥、TTS 节点、动作执行器。
- 未执行动作，未播放语音。
- 未修改网络、防火墙、DDS、Ollama。
- SSH 仅使用既有密钥；未生成新密钥、未修改 SSH 配置。
- 报告与本索引不含私钥、密码、Token、API Key、完整 SSH 配置文件。
- 新增文件仅为本地审查包，未写入 G1。

---

## 9. 哈希校验

```bash
cd /home/mick/Mick_Grey/teach/g1_kb_review
sha256sum -c SHA256SUMS
```

`SHA256SUMS` 覆盖：

- 6 个主交付物 + 本索引；
- `remote_snapshot/` 的 13 个文件（含 3 个 `.bak`）；
- `backup_snapshot/` 的 6 个文件；
- `diffs/` 的 5 个 diff 文件；
- `research_materials/` 的 8 个文件；
- 关键运行日志（runtime1/2/3/final）、`LastTest_20261031-1333.log`、`current_state_20261031T1401Z.txt`、`query_replication_20261031.txt`。

> 若重新生成或编辑任何报告，`SHA256SUMS` 需要同步更新；否则校验会失败。

---

## 10. 已知限制与使用建议

1. **不要把 37/25 条单测通过当作真机验收。** 单测只证明函数逻辑；运行时只覆盖了 9/20 FAQ。
2. **不要把 `/g1/tts` 捕获当作扬声器播放。** 本轮 TTS 节点未运行。
3. **不要把静态复现当作运行时结果。** 问题 1、2、5 是本地重放；报告已逐条标注。
4. **不要把域 42/43 当作可复核原始日志。** 原 `/tmp` 文件已删除，只有会话记录。
5. **不要忽略主报告第 10 节的既有问题。** `DEMO_ACTION`/`ROBOT_STATUS` 未本地处理、GREETING 表不一致、GENERAL_QA 未接地，都不是知识库改造引入，但会影响接待效果。
6. **审查建议优先看 P0：** 检索误命中（问题 1、5）、意图抢占（问题 2）、GENERAL_QA 事实冲突（问题 15）、真实链路未闭环（问题 16）。

---

*本索引与主报告、五个附录共同构成完整审查包。所有测试结论以附录 E 列出的原始输出为准；没有重新编造任何测试结果。*
