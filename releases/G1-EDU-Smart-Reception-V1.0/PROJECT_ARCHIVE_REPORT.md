# G1-EDU V1.0 项目归档报告

- 归档日期：2026-11-03
- 执行方式：从 G1（192.168.123.164）主动拉取，全程**只读**；未修改 G1 任何文件、未重启服务、未触发动作、未编译
- 未执行：`git init` / `git commit` / `git remote add` / `git push`

## 1. 最终本地路径

```text
/home/mick/Mick_Grey/g1_dev/releases/G1-EDU-Smart-Reception-V1.0/
```

> 目标目录此前不存在，未覆盖任何既有内容；`/home/mick/Mick_Grey/g1_dev/teach/` 下的原有资料**全部保留未动**。

## 2. 归档规模

| 项目 | 数量 |
|---|---:|
| 交付版本目录文件数（不含私有快照） | **120** |
| 私有快照文件数 | 12（另：仓库根 `evidence/` 128 个证据文件，不在本目录内） |
| 总目录数 | 67 |
| 总体积 | 30 MB |
| C++ 源文件 | 23 |
| C++ 头文件 | 11 |
| 测试源文件 | 8（其中 1 个为历史遗留副本，不参与构建） |
| Markdown 文档 | 48 |
| 历史交付报告 | 16 |
| 软著材料 | 11（10 md + 1 Mermaid 图） |
| 证据文件（仓库根 `evidence/`） | 127（不计入交付版本目录） |
| 部署脚本 / unit | 7 / 10 |

## 3. 关键校验结果

| 校验项 | 结果 |
|---|---|
| `g1_voice_router` 源码双端逐文件哈希 | **完全一致** |
| `g1_asr_bridge` 源码 | **完全一致** |
| `g1_action_executor` 源码 | **完全一致** |
| `g1_tts_node.cpp` / `tts_config.json` | 哈希一致 |
| `start_voice_router.sh` / `check_disk.sh` / `g1-voice-router.service` | 哈希一致 |
| 生产 Router 二进制 | `6ab99de8…` **MATCH_FROZEN** |
| 其余 4 个生产二进制 | 与 V1.0 清单一致 |
| 是否复制 build/install/log | 否 |

详见 `manifests/transfer_verification.md`。

## 4. 私有备份（本机，禁止上传）

```text
private_archive/
├── production_binaries/    # 5 个 ARM64 可执行文件（取自 /proc/PID/exe）
├── production_configs/     # 真实 lab_knowledge.json、tts_config*.json
├── systemd_snapshot/       # units_and_status.txt（unit 内容 + 运行状态，158 行）
├── release_manifests/      # V1.0 冻结清单（源码+二进制+unit 全量 SHA256）
└── README.md
```

## 5. 排除清单

| 排除项 | 原因 |
|---|---|
| `*.bak` / `*.before_action` / `*.before_local_reception` | 临时备份，非构建文件 |
| `g1_action_executor/backup_*` | 历史备份目录 |
| `build/`、`install/`、`log/` | 构建产物 |
| Unitree SDK / `unitree_ros2` 官方工作区 | 第三方代码，不随仓库分发 |
| `teach/g1_kb_work/backups/` | 多版本源码副本，与当前源码重复 |
| `teach/lab_research/` | 调研原始草稿，未整理（可后续单独评审） |
| 完整语音录音 / 对话全文 | 隐私；日志仅保留结构化字段 |

## 6. 敏感信息审计（GitHub 上传前）

| 检查项 | 结果 |
|---|---|
| 内网 IP `192.168.x.x` | 交付版内 **13** 个文件（含 `voice_router.cpp` 参数默认值、部分软著与部署文档）；仓库整体（含 `evidence/`）**58** 个文件 |
| `/home/unitree`、`/home/mick` 路径 | 交付版内 **37** 个文件；仓库整体 **149** 个文件（文档、脚本、unit、日志） |
| 私钥 / Token / 密码 | 未发现真实凭证 |
| 生产二进制混入公共目录 | 无 |
| `build/install/log` 混入 | 无 |
| `.env` / 密钥文件 | 无（仅提供 `.example` 模板） |
| 许可证 | **未指定**，未添加任何 LICENSE 文件 |

处置建议见 `docs/GITHUB_PREPARE_CHECKLIST.md`：
- `private_archive/` **不要上传**；证据见仓库根 `evidence/`，同样不应上传；
- `docs/milestones/` 含内网信息，公开前需脱敏或改为私有仓库；
- 真实 `lab_knowledge.json` 建议以示例替换（已提供 `lab_knowledge.example.json`）。

## 7. README 与文档编写情况

| 文件 | 内容要点 |
|---|---|
| `README.md` | 项目背景、核心功能、Mermaid 架构图、软硬件环境、目录结构、编译部署、话题接口、动作白名单（含真机验收状态）、测试数据 453 项、已知限制、后续规划、许可证声明 |
| `CHANGELOG.md` | P0-16 → P0-21 里程碑表 + 测试基线 + 生产标识 |
| `CONTRIBUTING.md` | 编码规范、架构强制约束（不得绕过 Guard/Manager、不得新增未验证动作）、测试要求、变更流程、安全红线 |
| `docs/architecture/` | 总体架构与节点通信、交互状态机、状态与动作数据通路、systemd 部署拓扑 |
| `docs/deployment/` | 环境准备、编译部署、运维与回滚、故障排查 |
| `docs/testing/` | 453 项测试摘要、隔离验证、真机验证、未验证项 |
| `docs/GITHUB_PREPARE_CHECKLIST.md` | 可提交/禁止提交/需人工确认三类清单 + 实际扫描结果 |
| `software_copyright/` | 10 份材料 + 架构图（未写入导航/视觉等未实现能力） |

## 8. 尚未找到 / 需要人工确认

**已找到**：P0-15、P0-16、P0-17、P0-17.1、P0-17.2、P0-17.3、P0-18、P0-18.1~18.4、P0-19、P0-20、P0-20.1、P0-21、P0-21 补充报告，共 16 份。

**未找到**：无（P0-16 起全部在位；更早的 P0-15 也在 `docs/milestones/`）。

**需要人工确认**：

1. 是否公开 `docs/milestones/`（含内网 IP、主机名、路径）。
2. 是否公开真实 `lab_knowledge.json`（当前随源码保留，建议替换为示例）。
3. 是否删除 `voice_router.cpp` 中的默认内网地址（会与冻结二进制产生差异）。
4. 是否为项目添加开源许可证（当前**未指定**）。
5. 是否上传仓库根 `evidence/`（127 文件、约 832KB 日志、含内网信息）。
6. 是否清理 `src/intent_recognizer_test.cpp` 历史遗留副本。

## 9. 执行合规确认

```text
修改 G1 生产文件：否
重启 G1 生产服务：否
触发机器人动作：否
重新编译：否
覆盖笔记本既有备份：否
虚构历史报告：否（未找到即记录，未创建任何伪造报告）
git commit：否
git push：否
创建远程仓库：否
```
