# README 重构与文档完善报告

- **日期**：2026-11-03
- **范围**：`releases/G1-EDU-Smart-Reception-V1.0/`（V1.0 交付版）+ 仓库根 `README.md`
- **未执行**：`git commit` / `git push` / GitHub 上传；未修改 G1 生产代码、配置或正在运行的服务
- **依据**：本轮逐文件核对源码（`voice_router.cpp`、`interaction_manager.cpp`、`intent_recognizer.cpp`、`fact_risk_guard.cpp`、`knowledge_base.cpp`、`robot_state_*`、`asr_text_normalizer.cpp`、`action_command_mapper.cpp`、`arm_action_executor.cpp`、`g1_tts_node.cpp`）、`CMakeLists.txt`、`package.xml`、`deployment/systemd/*`、`deployment/logrotate/*`、`config/lab_knowledge.json` 实际计数、`docs/milestones/*`、`evidence/p0183` 与 `evidence/p021` 原始日志

---

## 1. 原 README 存在的问题

重构前的交付版 README 已经具备 18 章骨架，但存在以下实质问题：

| # | 问题 | 具体表现 |
|---|---|---|
| 1 | 事实性错误（数字归属） | 把“播报 93%”记为 P0-21；该数值实际出自 P0-20.1 报告 |
| 2 | 事实性错误（时间） | “开机恢复约 45s”在 P0-18.3 报告中无秒级证据，报告只有 `boot 12:53` 与 `ActiveEnter 12:54:07` 的分钟级时间戳 |
| 3 | 遗漏关键机制 | 未提及执行器 3 s 动作冷却、FSM 白名单（500/501/801）、`victory`/`heart` 需 `mode_machine ∈ {5,6}`、失败后保持锁定 |
| 4 | 遗漏协议缺口 | 执行器会输出 `completion_unknown:` / `completed_with_rpc_error:`，Router 不识别，只会退化成 45 s 超时——原 README 未披露 |
| 5 | 配置描述误导 | TTS 写“配置 volume=80”，但节点**只读取未应用**（启动日志明确警告 `JSON volume is loaded but not applied by this node`） |
| 6 | 深度不足 | 模块章节多为“问题—方案—收益”三行，缺“工作机制/输入输出/依赖/测试映射”；架构文档偏索引，缺 QoS 选择理由、线程模型、数据所有权 |
| 7 | 术语未解释 | ASR/TTS/DDS/QoS/FSM/RPC/SOC 等直接使用，未在首次出现时给出中英文全称 |
| 8 | 材料不完整 | 缺术语表；缺“四种验证级别”说明表；故障排查未覆盖 `completion_unknown` 与执行器锁定 |
| 9 | 敏感信息 | `V1.0_RELEASE_NOTES.md` 含真实内网地址与开发者私有路径；`PROJECT_ARCHIVE_REPORT.md`、`handover.md`、部分里程碑报告含内网 IP 与 `/home/<用户>` 路径 |
| 10 | 发布信息缺失 | `V1.0_RELEASE_NOTES.md` 写“意图 13 类”（实际 7 类）、状态聚合含“里程计”（Provider 未订阅 `/dog_odom`） |

> 仓库根 `README.md` 的问题不同：它只是“开发工作仓库导览”，正文以目录树与流程指引为主，无法作为项目门户（缺架构、功能矩阵、测试结果、限制说明）。

---

## 2. 新 README 的章节结构

交付版 `README.md`（18 章 + 2 个附录）：

| 章节 | 内容要点 |
|---|---|
| 标题区 | 正式中文名 + 英文副标题 + 状态表（含生产 SHA256、测试规模）+ 许可证声明 |
| 目录 | 18 章 + 附录锚点 |
| 1 实机展示 | 素材清单表（6 项，含拍摄要求与状态），不放置任何替代图 |
| 2 背景与设计目标 | 4 个真实痛点（含“模型给错报价”实例）、6 条目标-手段对应、**7 个“为什么”**、明确边界 |
| 3 核心功能 | 13 行功能矩阵（用户能做什么 / 系统如何实现 / 验证级别）+ 四种验证级别区别表 |
| 4 系统总体架构 | 主链路、动作子链路、状态子链路、部署拓扑 **4 张 Mermaid** |
| 5 关键业务流程 | 知识问答 / 状态查询 / 动作执行 **3 张时序图** + 5 条动作硬约束 |
| 6 核心模块设计 | 10 个模块，统一表头（输入/输出/依赖/源码）+ 问题→方案→机制→收益→限制 |
| 7 技术选型与决策 | 7 行选型对照（含代价列）+ 7 条设计原则 |
| 8 硬件与软件环境 | 机器人端 / 开发机 / 模型服务端三段 |
| 9 目录与源码导航 | 真实文件树 + 关键代码定位表 + 10 步阅读顺序 |
| 10 编译部署运行 | 环境、两工作区、构建、测试、systemd 启动、日志、单节点重启、手工调试、注意事项 |
| 11 ROS 2 接口 | 5 个业务话题 + 5 个原生话题 + 三份负载示例 + 六类动作结果前缀对照 + 检查命令 |
| 12 动作安全 | 白名单表（含固件附加检查与验收状态）+ 四道闸门 + 结果关联风险 |
| 13 测试与验收 | 453 项明细（含覆盖重点）+ Mock + 真机 7 行 + 证据目录导航 |
| 14 性能与资源 | 20 项带来源的数值，明确标注“未测量”项 |
| 15 故障排查 | 12 个真实场景，统一模板（现象/原因/检查/正常/判断/处理/安全影响） |
| 16 已知限制 | 13 条，逐条可追溯到源码或报告 |
| 17 路线图 | 9 项，全部标注未实现 |
| 18 参考与依赖 | 7 个第三方项目（链接/用途/版本/许可证）+ 来源声明 + 许可证状态 |
| 附录 A | 术语表（13 条中英全称与项目内含义） |
| 附录 B | 文档索引（12 个入口） |

仓库根 `README.md` 同步升级为“仓库导览 + 项目速览”：一句话简介、能力表、架构图、5 条工程亮点、目录、快速开始、测试基线、限制摘要、隐私说明，并明确指向交付版门户。

---

## 3. 新增/重写的技术文档

```text
README.md                              重写（项目门户）
README_REVIEW_REPORT.md                重写（本文件）
CHANGELOG.md                           新增“文档二轮重构”条目
V1.0_RELEASE_NOTES.md                  修正 4 处（意图数量、里程计、内网地址、TTS 说明）

docs/architecture/
├── system_overview.md                 重写：分层依据、进程/线程模型、数据所有权、失败模式
├── communication_graph.md             重写：QoS 选择理由、负载格式、单实例约束、时序边界
├── interaction_sequence.md            重写：13 状态机、3 张时序、结果协议匹配规则、超时预算、停机时序
├── robot_state_pipeline.md            重写：数据源边界、四态与阈值、SOC 三道校验、JSON 约定、话术映射
└── deployment_topology.md             重写：物理拓扑、目录布局、启动顺序、服务策略、端口与资源

docs/development/
├── code_structure.md                  重写：构建目标与依赖链、逐文件行数与职责、测试映射
├── module_design.md                   重写：数据结构、算法顺序、边界用例、每模块测试映射
├── design_decisions.md                重写：14 条 ADR（背景/决策/备选/后果）
└── dependencies.md                    重写：补齐“为什么需要/缺失后果/许可证”

docs/deployment/
├── environment_setup.md               重写：目录布局、加载顺序与理由、连通性检查
├── build_guide.md                     重写：构建目标表、产物路径、哈希校验、排错（含 1 处“待确认”）
├── startup_guide.md                   重写：首次部署、自检、手工调试、参数表、注意事项
├── service_management.md              重写：单元清单、配置含义、状态判定、日志体系与 copytruncate 理由
└── rollback_guide.md                  重写：回滚决策树、备份、单服务回滚、回归清单、禁止事项

docs/testing/
├── unit_tests.md                      重写：7 套件覆盖重点、直跑方式、历史数字差异说明
├── mock_tests.md                      重写：8 条端到端用例、动作 7 场景、停机、与真机边界
├── hardware_validation.md             重写：6 轮真机记录（含字节数、时长、SOC）+ 未验证清单
└── known_issues.md                    重写：15 条（现象/影响/缓解/根治）

docs/milestones/
├── README.md                          重写：阶段编号—报告文件—导航页三列对照 + 演进图
├── P016.md / P017.md / P018.md        新增：阶段摘要（不复制原文）
├── P019.md / P020.md / P021.md        新增：同上
└── （P015 与 17 份原始报告）            保持原样，仅脱敏地址

docs/assets/README.md                  重写：命名约定、引用写法、禁止内容、体积限制、留空理由
docs/GITHUB_PREPARE_CHECKLIST.md       更新：脱敏状态与扫描结果
PROJECT_ARCHIVE_REPORT.md              更新：脱敏说明
```

---

## 4. 每份文档的依据

| 文档 | 主要依据 |
|---|---|
| README 全部数字 | 源码常量：`kDefaultMaxPending=3`、`kDefaultDedupeWindowMs=1200`、`kRobotStateCacheTimeoutMs=5000`、`StateTimeouts`（1000/1000/1000/2000）、`action_timeout_seconds=45`、`request_timeout_seconds=60`、执行器 `start 5s / completion 30s / settle 3s / cooldown 3s`、TTS `250 ms/char + 1500 ms` |
| 主链路与分支 | `voice_router.cpp` 的 `process_request()` 分支顺序与 `publish_reply()` 单一出口 |
| 状态机 | `interaction_types.hpp` 枚举 + `interaction_manager.cpp` 的 `can_transition()` |
| 话题与 QoS | 各节点 `create_publisher/create_subscription` 实参；频率取自 P0-18 实测 |
| 七类意图 | `intent_recognizer.hpp` 枚举（确认 `PRICE` 不在其中） |
| 四类风险 | `fact_risk_guard.hpp` 枚举 + `classify()` 判定顺序 |
| 知识库规模 | 解析 `config/lab_knowledge.json` 实测：FAQ 21、方向 3、项目 1、设备 1、分类 5、实验室 3、来源 15 |
| 状态四态与 SOC | `robot_state_types.hpp`、`robot_state_evaluator.hpp`（阈值与注释中的 1229 样本依据）、`robot_state_provider.cpp` |
| 动作白名单与检查 | `action_command_mapper.cpp`（短语表、禁用词）与 `arm_action_executor.cpp`（FSM 500/501/801、mode_machine 5/6、holding/id、冷却、锁定） |
| 动作结果协议 | `arm_action_executor.cpp` 的 `publish_result(...)` 六类前缀 + `voice_router.cpp` 的 `parse_action_result()` |
| TTS 行为 | `g1_tts_node.cpp`（分句、估算、队列 10、`volume` 未应用警告、`ret=0` 语义） |
| 运维参数 | `deployment/systemd/*.service|timer|target`、`deployment/logrotate/g1-voice-stack.conf`、`check_disk.sh` |
| 453 项与真机结论 | `docs/milestones/P021_SYSTEM_ACCEPTANCE_REPORT.md` §5/§6、`P0201`、`P016`、`P0183`、`P0184`；`evidence/p0183/post_reboot_service_status.txt`、`evidence/p021/router_demo.log` |
| 5.5 s / 7.0 s | `evidence/p021/router_demo.log` 的 `event=COMPLETED total_ms=5500` 与 `total_ms=7021` |

---

## 5. 修正的不准确内容

| 位置 | 原内容 | 修正后 | 依据 |
|---|---|---|---|
| README §3 功能矩阵 | 电量“真机（P0-21 播报 93%）” | 改为 P0-20.1（93% 出自该报告） | `P0201_PRODUCTION_DEPLOYMENT_REPORT.md` |
| README §6.10 / 旧 startup_guide | “开机恢复约 45s” | “boot 12:53 → 五服务 12:54:07（分钟级时间戳）” | `evidence/p0183/post_reboot_service_status.txt` |
| README §6.9 TTS | “配置 volume=80” | 明确“只读取未应用，需设备侧设置” | `g1_tts_node.cpp` 的 WARN 日志 |
| README §11 / §16 | 未提结果前缀不对齐 | 新增六类前缀对照表 + 超时退化说明 | `arm_action_executor.cpp` / `parse_action_result()` |
| README §12 | 未提冷却/FSM/模式检查 | 新增“固件附加检查”列与四道闸门 | `arm_action_executor.cpp` |
| README §9 / §6.10 | “5 service + 2 timer + 2 oneshot”含混 | “7 service（5 常驻 + 2 oneshot）+ target + 2 timer” | `deployment/systemd/` 实际文件数 |
| `V1.0_RELEASE_NOTES.md` §3-3 | 意图“13 类” | **7 类**，并说明 `PRICE` 属风控类型 | `intent_recognizer.hpp` |
| `V1.0_RELEASE_NOTES.md` §3-7 | 状态含“里程计” | 四路订阅，明确不消费 `/dog_odom` | `robot_state_provider.cpp` |
| `V1.0_RELEASE_NOTES.md` §3-10 | 含真实内网地址 | `<LLM_HOST>` 占位 | 隐私要求 |
| `V1.0_RELEASE_NOTES.md` §1 | 含开发者私有路径 | 改为“开发机上的同源副本” | 隐私要求 |
| `software_copyright/01` | “6 个经真机验证的动作 ID” | “6 个固定白名单动作，其中 wave 已完成真机验收” | 仅 `wave` 有真机记录 |
| `software_copyright/03` | 架构图/接口表含 `/dog_odom`；重启写“45s 内（06:03:48）” | 四路订阅；改为 boot 12:53 → 12:54:07 | `robot_state_provider.cpp`、`evidence/p0183` |
| `software_copyright/04` | 订阅含 `/dog_odom`；发布频率“1–2Hz” | 四路订阅、明确未接入 odom；2 Hz | 同上 |
| `software_copyright/07` | “等待 45 秒左右” | “等待约 1 分钟” | 同上 |
| `software_copyright/08` | “13 类意图边界” | “7 类意图边界” | `intent_recognizer.hpp` |
| 全仓库 markdown | 内网 IP 与 `/home/<用户>` | 统一脱敏（见 §8） | 隐私要求 |

> 未改动任何 `.cpp/.hpp/.json/.service/.sh`：代码与部署资产的字节保持不变，冻结哈希仍然有效。

---

## 6. Mermaid 图数量

| 文件 | 图数 | 类型 |
|---|---:|---|
| `README.md` | 7 | 4 flowchart + 3 sequenceDiagram |
| `docs/architecture/system_overview.md` | 1 | flowchart |
| `docs/architecture/communication_graph.md` | 1 | graph |
| `docs/architecture/interaction_sequence.md` | 4 | 1 stateDiagram-v2 + 3 sequenceDiagram |
| `docs/architecture/robot_state_pipeline.md` | 1 | flowchart |
| `docs/architecture/deployment_topology.md` | 1 | flowchart |
| `docs/deployment/rollback_guide.md` | 1 | flowchart |
| `docs/development/handover.md` | 1 | flowchart（原有） |
| `docs/milestones/README.md` | 1 | flowchart |
| `software_copyright/diagrams/architecture.md` | 4 | 原有，未改 |
| **合计** | **22** | — |

- 全部使用 GitHub 支持的语法（`flowchart` / `graph` / `sequenceDiagram` / `stateDiagram-v2`）；
- 未使用 `%%{init}%%` 主题指令、未使用 HTML 标签以外的插件语法；
- 代码围栏成对检查：87 个 markdown 文件**无未闭合围栏**。

---

## 7. 内部链接检查结果

| 检查项 | 结果 |
|---|---|
| 扫描范围 | 仓库根 `README.md` + 交付版全部 `.md`（共 87 个文件） |
| 相对链接总数 | **95**（不含代码块与行内代码中的示例） |
| 断链 | **0** |
| 锚点链接 | **0** 失效（目录锚点与 `#`/`##`/`###` 标题自动锚点逐项比对） |
| 代码围栏 | 87 个文件全部成对闭合 |
| 图片引用 | 尚无图片文件；README 与 `docs/assets/README.md` 中的 5 处素材路径写在代码块内作为“放入后取消注释”的示例，不产生 GitHub 断链 |
| 目录链接 | `docs/` 下所有交叉引用（architecture ↔ development ↔ testing ↔ deployment ↔ milestones）均已核对 |

检查方式：脚本先剥离代码围栏与行内代码，再正则提取 `[text](target)` 与 `![alt](target)`，跳过 `http(s)`/`mailto`，对相对路径做 `normpath` + `os.path.exists` 判定；锚点按 GitHub 规则（小写、去标点、空格转 `-`）比对文件内标题。

---

## 8. 敏感信息审查结果

| 检查项 | 交付版 markdown | 其他位置 | 处置 |
|---|---|---|---|
| 内网 IP（`192.168.x.x`） | **0** | `ros2_ws/.../voice_router.cpp` 默认值 1 处；仓库根 `evidence/*.log` 多处 | markdown 已替换为 `<G1_HOST>` / `<LLM_HOST>`；源码默认值保留（与冻结二进制同源，改则哈希失效），已在 README 与检查清单标注；`evidence/` 默认不上传 |
| 开发者私有路径（`/home/<用户>`） | **0** | `evidence/` 日志 | markdown 已替换为 `/home/<user>`；`/home/unitree` 属机器人账号路径，部署脚本必需，保留 |
| 私钥 / Token / 密码 | 0 | 0 | 未发现 |
| 真实语音录音 | 0 | 0 | 仓库不含 `*.wav` |
| 生产二进制 | — | 仅 `private_archive/`（已 `.gitignore`） | 禁止上传 |
| 第三方 SDK 源码 | 0 | 0 | 未复制，仅链接官方仓库 |
| 许可证 | 未添加 `LICENSE` | `package.xml` 存在模板遗留 `<license>Apache-2.0</license>` | README 与 `dependencies.md` 均声明“未指定许可证”，并说明该字段不代表授权 |

---

## 9. 尚缺少的实机照片与视频

| 素材 | 用途 | 状态 |
|---|---|---|
| `docs/assets/g1_edu_full.jpg` | 整机形象 | 缺 |
| `docs/assets/g1_wave.jpg` | 对应 `success:wave:26` 的现场 | 缺 |
| `docs/assets/reception_scene.jpg` | 接待场景 | 缺（需肖像授权） |
| `docs/assets/terminal_nodes.png` | 节点/服务状态 | 缺（需遮挡内网信息） |
| `docs/assets/terminal_topics.png` | 话题连接数 | 缺 |
| `docs/assets/terminal_log.png` | `event=COMPLETED total_ms=...` | 缺 |
| `docs/assets/demo.mp4` | 30–90 秒演示 | 缺 |

所有位置均以表格 + 文件名占位，未使用任何网络图片或生成图。

---

## 10. 需要人工补充/确认的信息

1. **GitHub 仓库地址与主页链接**：README 未写入任何仓库 URL（避免编造），需要时补在标题区。
2. **许可证**：需作者决定后才添加 `LICENSE`；同时决定是否清理 `package.xml` 中的 `Apache-2.0` 遗留字段（会改变源码字节，需重新构建并更新哈希）。
3. **源码内网默认值**：`ollama_url` 是否改为 `<LLM_HOST>` 占位符。若改，必须重新编译、更新 `V1.0_RELEASE_NOTES.md` 的哈希，并在 `CHANGELOG` 记录。
4. **其余 5 个动作的真机验收**：完成 `wave_low / hands_up / clap / heart / victory` 后更新 README §12 表与 `docs/testing/hardware_validation.md`。
5. **ASR001 真机证据**：若后续真机出现“脑急接口/脑筋接口”，补一条真机记录，把“仅单测证据”改为“真机已验证”。
6. **TTS 播放完成反馈**：若底层提供事件，可移除“ret=0 ≠ 播放完成”的限制并更新 §6.9/§16。
7. **`completion_unknown` 结果的前缀统一**：需修改执行器与 Router 两侧协议，属代码变更，本轮未做。
8. **长时间稳定性数据**：>1 小时连续接待的 CPU/内存/日志增速曲线，建议专用长跑采集后补入 §14。
9. **`g1_tts_node` 的构建方式**：本归档未包含 Unitree 官方 `unitree_ros2_example` 的 `CMakeLists.txt`，构建章节中该步骤已标注“待确认”，需按实际文件补两行声明。
10. **素材**：见 §9。

---

## 结论

- README 已从“章节齐全但深度不足”升级为可独立阅读的项目门户：每个核心设计都写清了**工程问题 → 设计方案 → 工作机制 → 实际收益 → 当前限制**，所有数字可追溯到源码常量或阶段报告。
- 本次共修正 6 处 README 事实性/表述问题、4 处 `V1.0_RELEASE_NOTES.md` 问题、5 处软著材料问题（含“6 个动作均经真机验证”的夸大表述）；新发现并如实披露了“动作结果前缀不对齐导致超时退化”这一真实协议缺口。
- 文档体系：架构 5 篇、开发 4 篇、部署 5 篇、测试 4 篇全部重写加深，新增 6 个阶段导航页与素材规范页，未复制原始阶段报告。
- 未执行任何 git 操作，未触碰 G1 代码、配置与服务。
