# 文件台账（V1.0 归档）

- 归档时间：2026-11-03
- 归档根目录：`/home/mick/Mick_Grey/g1_dev/releases/G1-EDU-Smart-Reception-V1.0`
- 交付版本目录文件数：**132**（含 `private_archive/` 12 个）；不含私有快照 **120**
- 运行证据：**127 个文件（约 832KB）**，统一保存在**仓库根 `evidence/`**，不重复存放于本目录
- 总目录数：**67**（不含 `private_archive/` 子目录）
- 交付版本目录体积：**29 MB**（其中 `private_archive/` 生产二进制约 28 MB）

## 1. 分类统计

| 类别 | 数量 | 位置 |
|---|---:|---|
| C++ 源文件 `*.cpp` | 23 | `ros2_ws/`、`action_ws/`、`integrations/` |
| C++ 头文件 `*.hpp` | 11 | `ros2_ws/src/g1_voice_router/include/`、`action_ws/` |
| 测试源文件 `*_test.cpp` | 8 | `ros2_ws/src/g1_voice_router/test/`（含 1 个历史遗留副本） |
| Markdown 文档 | 48 | `README`、`docs/`、`software_copyright/`、`manifests/` |
| 历史交付报告 | 16 | `docs/milestones/` |
| 软著材料 | 11 | `software_copyright/`（10 份 md + 1 架构图） |
| 证据文件（仓库根） | 127 | `../../evidence/p015 … p021` |
| 部署脚本 `*.sh` | 7 | `deployment/scripts/`、`monitoring/`、`config_examples/` |
| systemd unit / timer | 10 | `deployment/systemd/` |
| JSON 配置 | 13 | 源码 config、示例、私有真实配置 |
| 生产二进制 | 5 | `private_archive/production_binaries/` |

## 2. 来源映射

| 归档位置 | 来源（G1 或本机） | 处理方式 |
|---|---|---|
| `ros2_ws/src/g1_voice_router/` | `~/unitree_ros2/example/g1_voice_router/` | rsync，排除 `*.bak`、`*.before_*`、build/install/log |
| `ros2_ws/src/g1_asr_bridge/` | `~/unitree_ros2/example/g1_asr_bridge/` | rsync 全量 |
| `action_ws/src/g1_action_executor/` | `~/lidar_ws/src/g1_action_executor/` | rsync，排除 `backup*/` |
| `integrations/unitree_audio/` | `~/unitree_ros2/example/src/src/g1/audio_client/g1_tts_node.cpp`、`config/*.json` | scp |
| `deployment/scripts/` | `~/g1_systemd/scripts/` | rsync |
| `deployment/monitoring/` | `~/g1_systemd/monitor/` | rsync |
| `deployment/logrotate/` | `~/g1_systemd/logrotate_staged/`、`~/.config/logrotate/g1-voice-stack.conf` | rsync + scp |
| `deployment/systemd/` | `~/.config/systemd/user/g1-*` | rsync（仅 `g1-*`） |
| `docs/milestones/` | 本机 `teach/g1_kb_work/P0*.md`、`teach/P021_*.md` | 原样复制，**未改写任何日期/结论** |
| `docs/development/` | `teach/G1_AGENT_HANDOVER_20261101.md` | 原样复制 |
| `software_copyright/` | `teach/software_copyright/` | 原样复制 |
| `../../evidence/p0xx/` | `teach/g1_kb_work/evidence_p0xx/`、`teach/evidence_p021/` | 原样移动，本目录不再保存副本 |
| `private_archive/production_binaries/` | G1 `/proc/<PID>/exe`（运行映像） | scp 后校验 SHA256 |
| `private_archive/production_configs/` | G1 `config/lab_knowledge.json`、`config/tts_*.json` | scp |

## 3. 排除清单（及原因）

| 排除项 | 原因 |
|---|---|
| `g1_voice_router/src/voice_router.cpp.bak` | 历史临时备份，非构建文件 |
| `g1_voice_router/src/voice_router.cpp.before_action` | 同上 |
| `g1_voice_router/src/voice_router.cpp.before_local_reception` | 同上 |
| `lidar_ws/src/g1_action_executor/backup_20261031_061557/` | 动作执行器历史备份 |
| `lidar_ws/src/g1_action_executor/backup_before_settle_20261031_062701/` | 同上 |
| `build/`、`install/`、`log/` | 构建产物，可由源码重建 |
| Unitree SDK / `unitree_ros2` 官方工作区 | 第三方代码，不随仓库分发 |
| `teach/g1_kb_work/backups/`（本地源码备份） | 与源码重复，且含多版本噪音 |
| `teach/lab_research/`（调研原始资料） | 内部调研草稿，未整理；如需可后续单独评审 |

## 4. 保留但需注意的文件

| 文件 | 说明 |
|---|---|
| `ros2_ws/src/g1_voice_router/src/intent_recognizer_test.cpp` | 历史遗留副本，**不在 CMake 构建目标中**，不参与编译；保留以便追溯，清理需单独评估 |
| `docs/milestones/P021_FINAL_ACCEPTANCE_SUPPLEMENT.md` | 明确记录"真机未触发 ASR001 分支"，不得当作已真机验证 |
| `evidence/p021/supplement/*` | 含真实日志片段（无完整语音转写） |
| `ros2_ws/src/g1_voice_router/config/lab_knowledge.json` | 真实知识库（公开来源整理，但属实验室资料）——公开前建议替换为示例 |
