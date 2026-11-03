# g1_dev — G1-EDU 校园实验室智能接待系统 · 项目工作仓库

本仓库是《基于 ROS 2 的 G1-EDU 校园实验室智能接待与讲解系统 V1.0》的**开发工作仓库**，包含：

- 开发期的源码工作副本与调研资料
- 各阶段（P0-15 ~ P0-21）的交付报告与运行证据
- 软件著作权申请材料
- **V1.0 冻结版本（可独立交付/上传 GitHub 的子目录）**

> 如果你要找"可以直接发布/交付的那一份"，请直接看
> [`releases/G1-EDU-Smart-Reception-V1.0/`](releases/G1-EDU-Smart-Reception-V1.0/README.md)。
> 它是自包含的（源码 + 部署 + 文档 + 软著 + 清单），本仓库其余目录是开发过程资料。

---

## 1. 项目状态（V1.0）

| 项目 | 状态 |
|---|---|
| 版本 | **V1.0，2026-11-03 冻结** |
| 生产 Router 二进制 SHA256 | `6ab99de8ceb287d1fdb58b66399874c60b41734775a74a0c2745bf64223d6bb7` |
| 单元测试 | **453 项**（7 个套件，G1 ARM64 `colcon` 0 failures） |
| 真机验证 | 语音闭环、状态问答、单次 `wave`（ID 26）已通过 |
| 生产部署 | 5 个用户级 systemd 服务，开机自恢复 |
| 未实现 | 自主导航、人脸识别、视觉感知、G1 本地大模型 |

---

## 2. 目录结构

```text
g1_dev/
├── README.md                 ← 仓库总览与导航（本文件）
├── .gitignore                ← 忽略构建产物、node_modules、私有快照
│
├── releases/                 ← 【交付物】V1.0 冻结版本，自包含
│   └── G1-EDU-Smart-Reception-V1.0/
│       ├── README.md         ← 交付版首页（架构图/编译/接口/限制）
│       ├── ros2_ws/          ← g1_voice_router、g1_asr_bridge 源码
│       ├── action_ws/        ← g1_action_executor 源码
│       ├── integrations/     ← TTS 节点源码与配置
│       ├── deployment/       ← 7 service + 2 timer + target、启动脚本、脱敏模板
│       ├── docs/             ← 架构、部署、测试、里程碑报告
│       ├── software_copyright/  ← 软著材料 01–10 + Mermaid 图
│       ├── manifests/        ← 文件台账、SHA256 清单、依赖说明
│       └── private_archive/  ← 生产二进制与真实配置（禁止上传）
│
├── docs/                     ← 开发期文档
│   ├── handover/             ← 项目交接文档
│   ├── milestones/           ← 16 份阶段报告 + V1.0 版本说明（共 17 份）
│   ├── design/               ← 节点与文件索引
│   └── INDEX.md              ← 文档导航
│
├── workspace/                ← 开发工作区（非交付物）
│   ├── g1_voice_router/      ← 本地开发副本（含测试与知识库配置）
│   ├── lab_research/         ← 知识库事实来源的调研资料
│   └── local_include/        ← 本地编译用头文件（nlohmann/json）
│
├── research/                 ← 设备与 SDK 参考
│   ├── robot-inventory/      ← 机器人系统 / ROS 2 / SDK / 接口清单
│   └── unitree_sdk_explainer/← SDK 讲解材料
│
└── evidence/                 ← 运行证据（按阶段，唯一副本）
    ├── p015/ p016/ p017/ p0171/ p0172/
    ├── p018/ p0181/ p0182/ p0183/ p0184/
    └── p019/ p020/ p0201/ p021/ p0fix/
```

---

## 3. 快速开始

### 3.1 只想看交付内容

```bash
cd releases/G1-EDU-Smart-Reception-V1.0
less README.md
```

### 3.2 想编译/部署到 G1

见 [`releases/G1-EDU-Smart-Reception-V1.0/docs/deployment/`](releases/G1-EDU-Smart-Reception-V1.0/docs/deployment/)：

1. `01_environment_preparation.md` — 环境与依赖
2. `02_build_and_deploy.md` — 两个工作区分别构建、单服务重启
3. `03_operations_and_rollback.md` — 日志、回滚、故障处置

### 3.3 想了解设计

- 架构与数据通路：`releases/.../docs/architecture/`
- 模块职责与接口：`releases/G1-EDU-Smart-Reception-V1.0/software_copyright/02_软件设计说明书.md`、`04_模块功能说明.md`
- 话题接口与动作白名单：`releases/.../README.md` 第 7、8 节

### 3.4 想继续开发

- 编码与架构红线（不得绕过 FactRiskGuard / InteractionManager、不得新增未验证动作 ID）：`releases/.../CONTRIBUTING.md`
- 开发副本：`workspace/g1_voice_router/`
- 测试基线：453 项，改完必须全绿

---

## 4. 测试基线（V1.0）

| 套件 | 用例数 |
|---|---:|
| asr_text_normalizer_test | 22 |
| action_command_mapper_test | 39 |
| interaction_manager_test | 76 |
| intent_recognizer_test | 96 |
| fact_risk_guard_test | 119 |
| knowledge_base_test | 57 |
| robot_state_evaluator_test | 44 |
| **合计** | **453** |

```bash
# G1 上执行
cd ~/unitree_ros2/example
colcon test --packages-select g1_voice_router && colcon test-result --verbose
```

---

## 5. 关于历史路径

仓库在 2026-11-03 做过一次结构重整，并删除了不再需要的临时目录（`teach/`、`archive/`、旧源码快照、过期调研站点等）。

因此**历史报告里出现的旧路径（如 `g1_dev/teach/...`）已不存在**——那是当时的真实记录，为保证交付证据可信，报告正文未做改写。当前有效结构以上面的目录树为准。

---

## 6. 仓库与发布

- 本仓库有远端 `origin`（GitHub），但**本次重整未执行任何 `git commit` / `git push`**。
- 要让 Git 记录本次目录移动，可自行执行：

```bash
cd /home/mick/Mick_Grey/g1_dev
git add -A
git status          # 检查后自行决定是否提交
```

- 提交前请先看 `releases/G1-EDU-Smart-Reception-V1.0/docs/GITHUB_PREPARE_CHECKLIST.md`：
  `evidence/`（127 个文件）与 `private_archive/` 不应进入公开仓库。
- **项目未指定开源许可证**，未添加任何 LICENSE 文件，请勿默认视为可再分发。

---

## 7. 安全与隐私提醒

| 内容 | 说明 |
|---|---|
| `evidence/**` | 含内网 IP（`192.168.x.x`）、主机名、`/home/unitree` 路径 |
| `docs/milestones/**` | 同上，为历史报告原样保留 |
| `releases/.../private_archive/**` | 生产二进制与真实配置，**禁止上传** |
| 源码中的 `ollama_url` 默认值 | 仍是内网地址（为与冻结二进制保持一致，未改动） |
