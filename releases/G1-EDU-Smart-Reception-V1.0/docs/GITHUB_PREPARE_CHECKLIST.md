# GitHub 上传前检查清单

> 本清单由归档时自动扫描 + 人工判断生成。**生成 .gitignore 不等于完成安全审计**，公开前请逐项确认。

## 1. 可以直接提交

| 目录/文件 | 说明 |
|---|---|
| `ros2_ws/src/g1_voice_router/{src,include,test,CMakeLists.txt,package.xml}` | 自有代码与测试 |
| `ros2_ws/src/g1_asr_bridge/` | 自有代码 |
| `action_ws/src/g1_action_executor/` | 自有代码 |
| `integrations/unitree_audio/g1_tts_node.cpp` | 基于 Unitree 官方示例修改，已在文件头/文档注明来源 |
| `deployment/scripts`、`deployment/systemd`、`deployment/logrotate`、`deployment/monitoring` | 部署资产，含本机绝对路径，见 §3 |
| `deployment/config_examples/` | 占位符模板，安全 |
| `docs/architecture`、`docs/deployment`、`docs/testing` | 技术文档 |
| `software_copyright/` | 软著材料 |
| `manifests/*.md`、`manifests/*.sha256` | 清单 |
| `README.md`、`CHANGELOG.md`、`CONTRIBUTING.md`、`.gitignore` | 项目说明 |

## 2. 禁止提交

| 目录 | 原因 |
|---|---|
| `private_archive/` | 生产二进制、真实配置、systemd 快照、内部清单 |
| 仓库根 `evidence/` | 含运行日志与内网信息（默认不上传，或脱敏后单独上传） |
| `build/`、`install/`、`log/` | 构建产物 |
| `ros2_ws/src/g1_voice_router/config/lab_knowledge.json`（真实版） | 实验室内部资料；仓库只提供 `lab_knowledge.example.json` |

## 3. 需要人工确认（含内部信息）

| 文件/目录 | 具体内容 | 建议 |
|---|---|---|
| `docs/milestones/*.md`、`docs/development/handover.md`、`PROJECT_ARCHIVE_REPORT.md` | 报告正文原含内网 IP、主机名、用户名与 `/home/<user>` 路径 | **本次重构已对 markdown 正文做地址脱敏**（IP → `<G1_HOST>` / `<LLM_HOST>`，开发者私有路径 → `/home/<user>`）；`/home/unitree` 属机器人账号路径，按部署必需保留 |
| `deployment/systemd/*.service`、`scripts/*.sh` | 硬编码 `/home/unitree/...` 绝对路径 | 保留（部署必需）或改为变量 + 说明 |
| `deployment/logrotate/g1-voice-stack.conf` | 日志路径含用户名 | 同上 |
| `integrations/unitree_audio/config/tts_config.json` | 音量/语速等运行参数（非敏感） | 可提交 |
| `ros2_ws/src/g1_voice_router/config/lab_knowledge.json` | 实验室公开信息来源整理，但仍属内部资料 | 建议移入 `private_archive`，公开仓库用示例 |
| `software_copyright/08_软件测试报告.md` | 提及真机测试细节与型号 | 可提交 |

## 4. 已排除的敏感内容

- SSH 私钥 / 公钥：**未复制**（仅使用现有 SSH 连接）
- API Key / Token / 密码：仓库内未发现
- 真实语音录音（`*.wav`）、对话全文：**未复制**；日志仅保留结构化字段
- 生产二进制：仅存 `private_archive/`
- `.env` 类文件：未复制，仅提供 `.env.example`

## 5. 许可证提醒

**项目当前未指定开源许可证。** 请勿自行添加 MIT/Apache/GPL 等许可证文件，除非作者明确决定。

Unitree SDK / 官方 `unitree_ros2` 代码不在本仓库中，使用时需遵守其原始授权。

## 6. 上传前自检命令

```bash
cd G1-EDU-Smart-Reception-V1.0

# 1) 内网 IP
grep -rInE '192\.168\.[0-9]+\.[0-9]+' --exclude-dir=private_archive . | head

# 2) 绝对路径与用户名
grep -rInE '/home/(unitree|mick)' --exclude-dir=private_archive . | head

# 3) 密钥/令牌
grep -rInE 'BEGIN (RSA|OPENSSH|PRIVATE) KEY|api[_-]?key|token|password' --exclude-dir=private_archive . | head

# 4) 确认私有目录未被跟踪
git status --porcelain | grep -E 'private_archive|build/|install/' || echo "no private artifacts staged"
```

## 7. 结论

- 代码与文档结构已具备上传条件。
- **`docs/milestones/` 与部署资产含内网信息，公开前必须由作者决定"脱敏"或"私有仓库"**。
- 未经作者确认，不要执行 `git push`。

## 8. 归档时扫描的实际结果（2026-11-03）

| 检查项 | 结果 | 说明 |
|---|---|---|
| 含内网 IP `192.168.x.x` 的文件 | 交付版 markdown **0 个**（已脱敏）；`voice_router.cpp` 默认值 **1 处**；仓库整体仍有多处（`evidence/*.log`） | 源码默认值为与冻结二进制保持同源而保留，公开前由作者决定是否改为占位符并重编；`evidence/` 默认不上传 |
| 含 `/home/unitree` 或 `/home/<user>` 路径 | 交付版 **37 个** / 仓库整体 **149 个** | 文档、部署脚本、systemd unit、证据日志 |
| 私钥 / Token / 密码 | 0（仅本文件自身出现了关键字样） | 未发现真实凭证 |
| `build/`、`install/`、`log/` 目录 | 0 | 已排除 |
| 生产二进制 | 0（仅 `private_archive/`） | 已隔离 |
| 真实录音 `*.wav` / 完整语音转写 | 0 | 日志仅含意图、长度、归一化结果 |

### 关于源码中的默认地址

`voice_router.cpp` 的 ROS 参数 `ollama_url` 默认值为生产使用的内网地址。归档**保留了与生产二进制一致的原始字节**（便于溯源与哈希校验），因此该默认值仍在源码中。

公开前有两种处理方式，请作者选择：

1. **推荐**：仓库公开但保持源码不动，在 README 说明"默认值为示例内网地址，请用参数覆盖"（当前 README 已如此处理）；
2. 若需彻底移除：修改默认值为占位符并重新编译验证——这会与冻结二进制产生差异，必须在 `CHANGELOG` 记录。

### 关于 `evidence/`

仓库根 `evidence/` 共 127 个文件（约 832 KB），以运行日志为主，含内网 IP 与主机名。**默认建议不上传**；如需上传，请只挑选脱敏后的关键片段。
