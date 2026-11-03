# 目录重整对照表（2026-11-03）

本次操作**只移动、未删除**：重整前后文件总数均为 **1572**（不含 `.git`）。

## 顶层变化

| 重整前 | 重整后 | 说明 |
|---|---|---|
| `teach/` | 已拆分到 `docs/`、`workspace/`、`evidence/`、`archive/` | 原"教学/试错"目录，语义不清 |
| `info/` | `research/` | 设备/SDK 调研资料 |
| `G1_voice_Project/` | `workspace/legacy_source/` | 早期源码快照 |
| `G1_EDU_ROS2_Node_File_Index.md` | `docs/design/` | 索引类文档归入 docs |
| `README.md` | 重写；旧版存 `archive/notes/README_before_v1.0_20261028.md` | 旧版为 P0-16 时期说明 |
| `releases/` | 保持不动 | V1.0 冻结交付物 |

## 明细映射

| 旧路径 | 新路径 |
|---|---|
| `teach/G1_AGENT_HANDOVER_20261101.md` | `docs/handover/G1_AGENT_HANDOVER_20261101.md` |
| `teach/P021_SYSTEM_ACCEPTANCE_REPORT.md` | `docs/milestones/P021_SYSTEM_ACCEPTANCE_REPORT.md` |
| `teach/P021_FINAL_ACCEPTANCE_SUPPLEMENT.md` | `docs/milestones/P021_FINAL_ACCEPTANCE_SUPPLEMENT.md` |
| `teach/V1.0_RELEASE_NOTES.md` | `docs/milestones/V1.0_RELEASE_NOTES.md` |
| `teach/g1_kb_work/P0*.md`（14 份） | `docs/milestones/` |
| `teach/software_copyright/` | `releases/G1-EDU-Smart-Reception-V1.0/software_copyright/`（2026-11-03 去重后合并到交付版，`docs/copyright/` 已删除） |
| `teach/g1_kb_work/g1_voice_router/` | `workspace/g1_voice_router/` |
| `teach/g1_kb_work/.local_include/` | `workspace/local_include/` |
| `teach/lab_research/` | `workspace/lab_research/` |
| `teach/g1_kb_review/` | `workspace/g1_kb_review/` |
| `teach/g1_kb_work/evidence_pXXX/` | `evidence/pXXX/` |
| `teach/evidence_p021/` | `evidence/p021/` |
| `teach/g1_kb_work/backups/` | `archive/backups/g1_voice_router_versions/` |
| `teach/g1_kb_work/original/` | `archive/source_snapshots/g1_voice_router_original/` |
| `teach/g1_kb_work/DELIVERY_REPORT.md` | `archive/notes/DELIVERY_REPORT.md` |
| `teach/V1.0/` | `archive/release_templates/V1.0/` |
| `teach/.vscode/` | `archive/misc/teach_vscode/` |

## 影响说明

1. **历史报告中的旧路径不再存在**（例如 `…/g1_dev/teach/g1_kb_work/...`）。报告本身未修改，属历史记录；按上表换算即可。
2. `releases/G1-EDU-Smart-Reception-V1.0/` 内部路径**未变**，其 README 与清单仍然有效。
3. Git 尚未提交；执行 `git add -A` 后，Git 会以重命名方式记录（历史可追溯）。

## 2026-11-03 二次清理（去重与减重）

| 删除项 | 原大小 | 证据 | 理由 |
|---|---:|---|---|
| `research/conclusion/node_modules/` | 54 MB（831 个文件） | 目录内无 `package.json`，且 `dist/` 已是构建产物 | 第三方依赖，可重建，且已被 Git 跟踪（应由 `.gitignore` 排除） |
| `releases/.../evidence/` | 832 KB（126 个文件） | 与仓库根 `evidence/` 内容一致（p015/p016/p019/p021/p0fix 哈希完全相同；p017/p018/p020 仅目录层级不同） | 同一证据保留两份属冗余，统一以仓库根 `evidence/` 为准 |
| `docs/copyright/` | 约 200 KB（11 个文件） | `diff -rq` 与 `releases/.../software_copyright/` 无差异 | 软著材料只保留交付版一份 |

清理后：仓库文件数 **1577 → 612**，体积 **约 90 MB → 60 MB**（其中 `releases/private_archive` 12 个二进制约 28 MB）。

保留未删（有意保留的重复）：`releases/.../docs/milestones/` 与仓库根 `docs/milestones/` 内容一致（16 份报告），前者为交付版自包含需要，后者为开发版导航；两者已核对无差异，`V1.0_RELEASE_NOTES.md` 在交付版中放于其根目录。

保留未删：`archive/backups/`（多版本源码，1.3 MB）、`archive/source_snapshots/`（真实历史快照，含当时的 `.bak`）、`workspace/local_include/`（本地编译所需头文件 1.1 MB）。
