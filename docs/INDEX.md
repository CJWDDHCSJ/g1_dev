# 文档导航

## 交付物（推荐入口）

| 文档 | 说明 |
|---|---|
| [`releases/G1-EDU-Smart-Reception-V1.0/README.md`](../releases/G1-EDU-Smart-Reception-V1.0/README.md) | 交付版本首页：架构图、编译部署、话题接口、动作白名单、限制 |
| [`../releases/.../docs/architecture/`](../releases/G1-EDU-Smart-Reception-V1.0/docs/architecture/) | 系统架构、交互状态机、数据通路、systemd 拓扑 |
| [`../releases/.../docs/deployment/`](../releases/G1-EDU-Smart-Reception-V1.0/docs/deployment/) | 环境准备、编译部署、运维回滚 |
| [`../releases/.../docs/testing/`](../releases/G1-EDU-Smart-Reception-V1.0/docs/testing/) | 453 项测试摘要与真机验证记录 |
| [`../releases/.../software_copyright/`](../releases/G1-EDU-Smart-Reception-V1.0/software_copyright/) | 软著材料 01–10 + Mermaid 图 |
| [`../releases/.../docs/GITHUB_PREPARE_CHECKLIST.md`](../releases/G1-EDU-Smart-Reception-V1.0/docs/GITHUB_PREPARE_CHECKLIST.md) | 上传 GitHub 前的敏感信息与目录检查 |

## 开发期文档

| 文档 | 说明 |
|---|---|
| [`handover/G1_AGENT_HANDOVER_20261101.md`](handover/G1_AGENT_HANDOVER_20261101.md) | 项目交接文档（环境、模块、备份、排错经验） |
| [`design/G1_EDU_ROS2_Node_File_Index.md`](design/G1_EDU_ROS2_Node_File_Index.md) | 节点与文件索引 |
| [`../releases/.../software_copyright/`](../releases/G1-EDU-Smart-Reception-V1.0/software_copyright/) | 软著材料（唯一副本） |

## 阶段报告（按时间顺序）

| 阶段 | 文档 |
|---|---|
| P0-15 | `milestones/P015_DELIVERY_REPORT.md`（事实风险保护 FactRiskGuard） |
| P0-16 | `milestones/P016_DELIVERY_REPORT.md`（真实语音闭环） |
| P0-17 | `milestones/P017_ROBOT_STATE_AUDIT_REPORT.md`（状态数据源审计） |
| P0-17.1 | `milestones/P0171_FACT_RISK_GUARD_FIX_REPORT.md`（实时状态漏放修复） |
| P0-17.2 | `milestones/P0172_INTENT_RECOGNIZER_BOUNDARY_FIX_REPORT.md`（意图语义边界） |
| P0-17.3 | `milestones/P0173_PRODUCTION_VOICE_ROUTER_CUTOVER_REPORT.md`（生产切换） |
| P0-18 | `milestones/P018_ROBOT_STATE_PROVIDER_REPORT.md` |
| P0-18.1~18.4 | `milestones/P0181…P0184_*.md`（状态部署、systemd、重启恢复、日志轮转） |
| P0-19 | `milestones/P019_INTERACTION_MANAGER_REPORT.md`（交互任务管理） |
| P0-20 | `milestones/P020_ACTION_INTERACTION_REPORT.md`（动作语音闭环） |
| P0-20.1 | `milestones/P0201_PRODUCTION_DEPLOYMENT_REPORT.md`（生产切换 + wave 真机） |
| P0-21 | `milestones/P021_SYSTEM_ACCEPTANCE_REPORT.md`（系统验收 + V1.0 冻结） |
| P0-21 补充 | `milestones/P021_FINAL_ACCEPTANCE_SUPPLEMENT.md`（ASR 别名专项，含"真机未触发"的如实记录） |

## 证据

`evidence/p015 … evidence/p021` 存放各阶段日志与录制证据；含内网 IP，公开前需脱敏。
