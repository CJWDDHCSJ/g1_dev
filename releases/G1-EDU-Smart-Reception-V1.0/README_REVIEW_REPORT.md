# README 重构与文档完善报告

- 日期：2026-11-03
- 范围：`releases/G1-EDU-Smart-Reception-V1.0/`（V1.0 交付版）
- 未执行：git commit / push / GitHub 上传；未改动 G1 生产代码、配置与服务

## 1. 原 README 存在的问题

| 问题 | 具体表现 |
|---|---|
| 篇幅与结构不足 | 约 370 行，偏"使用说明"，缺工程背景、设计动机、时序细节、故障排查 |
| 不满足门户要求 | 无目录导航、无功能矩阵、无三段时序图、无接口发布/订阅表、无性能数据 |
| 事实性小错 | ① 把 `PRICE` 当作意图（源码中它是 `FactRiskGuard` 的风险类型，意图只有 7 类）；② 文档曾把 `/dog_odom` 列为 Provider 数据源，源码实际只订阅 bms/lowstate/sportmode/arm 四路 |
| 缺失限制披露 | 未集中说明 ASR 误识范围、动作结果无请求 ID、`ret=0≠播放完成`、仅 wave 真机验收 |
| 缺素材位 | 无实拍/截图占位说明，读者无法判断哪些是真实演示 |

## 2. 新 README 的章节结构（18 章 + 附录）

1. 实机展示（含素材占位与要求）
2. 项目背景与设计目标（7 个"为什么" + 边界）
3. 核心功能（功能矩阵 + 四级验证标注）
4. 系统总体架构（主链路 + 动作子链路 + 状态子链路 + 部署拓扑，共 4 图）
5. 关键业务流程（知识问答 / 状态查询 / 动作执行，3 张时序图）
6. 核心模块设计（10 个模块，统一模板：问题→方案→机制→收益→限制）
7. 技术选型与设计决策（7 条对照表 + 7 条原则）
8. 硬件与软件环境（机器人 / 开发机 / 模型主机三段）
9. 项目目录与源码导航（真实文件树 + 阅读顺序）
10. 编译、部署与运行（含两个工作区、systemd、日志、注意事项）
11. ROS 2 接口文档（5 个业务话题 + 4 个底层话题，含发布者/订阅者）
12. 动作安全设计与白名单（6 动作 + 永久禁用 + 结果关联风险）
13. 测试与验收证据（453 项 + Mock + 真机，分层标注）
14. 性能与资源（12 项带来源的实测/配置值）
15. 常见问题与故障排查（12 个真实场景，统一模板）
16. 已知限制（11 条）
17. 路线图（8 项，明确未实现）
18. 参考资料与第三方依赖（含许可证状态）
附录：文档索引

## 3. 新增/重组的技术文档

```text
docs/
├── architecture/
│   ├── system_overview.md        新增
│   ├── communication_graph.md    新增
│   ├── interaction_sequence.md   新增
│   ├── robot_state_pipeline.md   新增
│   └── deployment_topology.md    新增
├── development/
│   ├── code_structure.md         新增
│   ├── module_design.md          新增
│   ├── design_decisions.md       新增
│   ├── dependencies.md           新增
│   └── handover.md               原交接文档（加归档说明，正文未改）
├── deployment/
│   ├── environment_setup.md      新增
│   ├── build_guide.md            新增
│   ├── startup_guide.md          新增
│   ├── service_management.md     新增
│   └── rollback_guide.md         新增
├── testing/
│   ├── unit_tests.md             新增
│   ├── mock_tests.md             新增
│   ├── hardware_validation.md    新增
│   └── known_issues.md           新增
├── milestones/README.md          新增（阶段索引，正文报告不复制）
└── assets/README.md              新增（实拍素材约定）
```

旧文件（`01_system_overview.md`、`02_build_and_deploy.md`、`INSTALL_AND_START.md`、`test_report.md` 等 8 个）已由新文件替代并删除，避免同一主题多版本。

## 4. 每份文档的依据

| 文档 | 依据 |
|---|---|
| README 全部数字 | 源码常量（`kDefaultMaxPending=3`、`kDefaultDedupeWindowMs=1200`、`kRobotStateCacheTimeoutMs=5000`、`action_timeout_seconds=45`、`request_timeout_seconds=60`、Provider 500ms 定时、执行器 5/30/3s） |
| 意图 7 类 | `intent_recognizer.hpp` 枚举 |
| 话题/消息类型 | `voice_router.cpp`、`robot_state_provider.hpp` 的 `create_subscription`/`create_publisher` |
| 状态 JSON 字段 | `robot_state_evaluator.cpp` 的序列化代码 |
| 动作白名单 | `arm_action_executor.cpp` 的 `action = {...}` 分支 |
| 453 项测试 | G1 上重新执行 7 个测试二进制后求和（22+39+76+96+119+57+44） |
| 真机结论 | `docs/milestones/P016/P0201/P021*`、`docs/testing/hardware_validation.md` |
| 运维参数 | `deployment/systemd/*.service`、`deployment/logrotate/g1-voice-stack.conf` |
| 依赖与许可 | `manifests/dependencies.md`、上游官方仓库 |

## 5. 修正的不准确内容

| 位置 | 原内容 | 修正后 |
|---|---|---|
| 模块章节 | `PRICE` 列为意图 | 明确：`PRICE` 是 `FactRiskGuard` 风险类型；意图仅 7 类，价格问题意图上属 `GENERAL_QA` |
| 状态章节 | Provider 订阅含 `/dog_odom` | V1.0 实际只订阅 bms/lowstate/sportmode/arm action；odom 未纳入回答 |
| 包规模 | "13 源文件 / 11 头文件" | "11 源文件 + 1 历史副本 / 10 头文件 / 7 测试" |
| systemd 数量 | "5 service + target + 2 timer" | "7 service + 2 timer + target"（含 disk-check 与 log-rotate service） |
| 文件计数 | 交付版 132 / 公开 120 | 交付版 144 / 公开 132 + 私有 12（文档重组后） |
| 敏感信息统计 | 单一数字 19/83 | 交付版 13/37、仓库整体 58/149（双口径） |

## 6. Mermaid 图数量

| 文件 | 图数 |
|---|---:|
| `README.md` | 7 |
| `docs/architecture/*.md` | 8 |
| **合计** | **15** |

类型：8 个 flowchart、6 个 sequenceDiagram、1 个 graph。全部围栏配对（代码围栏数量为偶数），未使用 GitHub 不支持的指令。

## 7. 内部链接检查结果

- 交付版内 Markdown 相对链接：**0 断链**（脚本扫描所有方括号链接，跳过 http 与页内锚点）。
- 图片：当前无图片文件，README 用注释与表格占位，**不产生断链**。
- 根目录 `README.md`、`docs/INDEX.md` 链接同样为 0 断链。
- 清单：`manifests/source_files.sha256` 重建后 127 项，`sha256sum -c` **0 失败**。

## 8. 敏感信息审查

| 检查项 | 结果 |
|---|---|
| README 内网 IP | **0**（统一 `<LLM_HOST>` 占位） |
| README 私人路径 | 无 `/home/<user>` 硬编码（仅出现 ROS 工作区 `~/`） |
| 密钥/Token | 未发现 |
| 真实录音 | 未包含 |
| 生产二进制 | 仅 `private_archive/`（已在 `.gitignore`） |
| 许可证 | 未擅自指定，README 明确"尚未指定" |

> 交付版其余文件（软著、部署脚本、部分里程碑）仍含内网地址与 `/home/...` 路径，已在 `docs/GITHUB_PREPARE_CHECKLIST.md` 标注，需公开前人工决定是否脱敏。

## 9. 尚缺少的实机素材

1. G1-EDU 整机照片
2. 真实挥手照片（对应 P0-20.1）
3. 接待现场照片（需肖像授权）
4. 节点/话题终端截图（需先遮挡内网信息）
5. 1–3 分钟演示视频

约定文件名见 `docs/assets/README.md`。

## 10. 需要人工补充/确认的信息

1. **GitHub 仓库地址**：README 未写入任何链接（避免编造），需要时在标题区补充。
2. **许可证**：需作者决定后才可添加 LICENSE。
3. **其余 5 个动作的真机验收**：完成后更新 §12 表格与 `docs/testing/hardware_validation.md`。
4. **ASR001 的真机验证**：当前只有单测证据，若后续真机触发，需在 §13.3 更新。
5. **长期稳定性数据**：>1 小时连续接待的压力/资源数据尚未采集。
6. **模型主机描述**：可按实际部署环境补充"独立工作站/笔记本"等具体形态（当前只写"外部主机"）。

---

**结论**：README 已从"使用说明"升级为可独立阅读的项目门户；全部数字与结论均可追溯到源码或阶段报告；已如实保留 11 条已知限制，未把 Mock 结果描述为真机验证。
