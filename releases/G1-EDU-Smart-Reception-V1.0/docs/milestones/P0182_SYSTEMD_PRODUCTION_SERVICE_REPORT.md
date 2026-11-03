# P0-18.2 G1 生产节点 systemd 服务化与启动恢复管理 · 交付报告

- 日期：2026-11-02
- 结论：**五个接待节点已全部迁移到 systemd 管理（用户级 + linger），统一 target 启停、单节点 restart、异常退出自动恢复、业务回归全部通过；开机自启已配置但未经真实重启验证。**
- 证据目录：`/home/<user>/Mick_Grey/g1_dev/teach/g1_kb_work/evidence_p0182/`

## A. 原启动架构（迁移前实测）

| 节点 | 旧 PID | 父进程 | 启动方式 |
|---|---:|---|---|
| RobotStateProvider | 11528 | 11524（`ros2 run` 包装，PPID=1） | `nohup ros2 run ...` |
| TTS Node | 11653 | 11651 | 同上 |
| Voice Router | 11673 | 11671 | 同上 |
| ASR Bridge | 11696 | 11694 | 同上 |
| Action Executor | 11882 | 11879 | 同上（需 lidar_ws） |

- unitree 账户：uid=1000，HOME=/home/unitree；systemd 249.11-0ubuntu3.17。
- Unitree 原生：`/ros_bridge` 为 bare-DDS 应用（master_service/videohub 进程），**不是 systemd 服务**（系统内仅 `unitree-ota-uploader.service`），本阶段未触碰其生命周期。
- 环境（进程实测白名单）：`ROS_DISTRO=humble`、`RMW_IMPLEMENTATION=rmw_cyclonedds_cpp`、`ROS_LOCALHOST_ONLY=0`、`CYCLONEDDS_URI`（eth0 绑定，来自 setup.sh）、`ROS_DOMAIN_ID` 未设置（默认 0）。

## B. 新服务结构

**部署层级选择说明（重要偏差）**：`/etc/systemd/system` 不可写且 `sudo` 需要密码（`sudo -n` 失败）。为避免向用户索取密码，采用**用户级 systemd**：`~/.config/systemd/user/` + `loginctl enable-linger unitree`（该命令免密执行成功，`Linger=yes`）。用户级 unit 同样具备依赖、重启策略、独立日志和开机自启能力，且不涉及任何系统级配置。

| 文件 | 作用 |
|---|---|
| `~/.config/systemd/user/g1-robot-state-provider.service` | Provider |
| `~/.config/systemd/user/g1-tts.service` | TTS |
| `~/.config/systemd/user/g1-voice-router.service` | Voice Router |
| `~/.config/systemd/user/g1-asr-bridge.service` | ASR Bridge |
| `~/.config/systemd/user/g1-action-executor.service` | Action Executor |
| `~/.config/systemd/user/g1-voice-stack.target` | 统一 target（`Wants=` 五个服务） |
| `~/g1_systemd/scripts/start_*.sh` | 五个启动脚本（`exec` 直接运行二进制） |

依赖与顺序（`After=`）：Provider → TTS → Router → ASR → Executor；`PartOf=g1-voice-stack.target` 保证 `stop target` 能统一停止且视为显式停止（不触发自愈）。

恢复策略：`Restart=always`、`RestartSec=3`、`StartLimitIntervalSec=60`、`StartLimitBurst=5`、`KillSignal=SIGINT`、`TimeoutStopSec=15`。
- 选 `always` 的依据：五个节点均为长期常驻服务，不存在"正常退出"语义；人为 `systemctl stop` 不会触发重启（systemd 显式停止不执行 Restart）。
- 限速 5 次/60 秒，避免反复崩溃时进入无限重启循环。

**迁移中发现并修复的问题**：初始 target 仅用 `Wants=`，`stop target` 后服务因 `Restart=always` 被自愈拉起（观测到 stop 后服务仍 active）。加入 `PartOf=` 后复测：stop target → 五个服务全部 inactive、进程数 0，且 5 秒后仍保持停止。

## C. 环境配置

- 脚本统一：`set -eo pipefail` → `source /home/unitree/unitree_ros2/setup.sh` → `source .../example/install/setup.bash` → `set -u` → `exec <绝对路径二进制>`。
- Action Executor 额外 `source /home/unitree/lidar_ws/install/setup.bash`（P0-18.1 曾因缺失此步报 `Package 'g1_action_executor' not found`）。
- 直接 `exec` 二进制而非 `ros2 run`，因此 `MainPID` 就是真实 ROS 节点进程（不是包装 shell）。
- Domain 0、RMW、CYCLONEDDS_URI 全部继承既有 `setup.sh`，**未新增/修改任何 DDS 配置**。
- 日志：本机 journald 为易失模式（无 `/var/log/journal`，且无 sudo 无法改 Storage），因此每个服务使用 `StandardOutput=append:/home/unitree/g1_logs_systemd/<name>.log`，重启后日志保留；`journalctl --user -u <unit>` 仍可查启动/停止生命周期事件。

## D. 迁移结果（最终态）

| 节点 | 原 PID | 新 MainPID | systemd 状态 | 唯一实例 |
|---|---:|---:|---|---|
| RobotStateProvider | 11528 | **16583** | active (running) | ✅ |
| TTS Node | 11653 | **16584** | active (running) | ✅ |
| Voice Router | 11673 | **16585** | active (running) | ✅ |
| ASR Bridge | 11696 | **16589** | active (running) | ✅ |
| Action Executor | 11882 | **16592** | active (running) | ✅ |

- 迁移方式：逐个 `kill -INT` 旧节点 → 确认旧节点与其 `ros2 run` 包装进程退出 → `systemctl --user start` 对应服务 → 校验 MainPID 指向真实二进制 → 再迁移下一个。**无重复实例，无宽泛 kill。**
- 迁移过程中旧包装进程（11524/11651/11671/11694/11879）随子进程退出而自动结束。
- 迁移后 ROS 节点 5 个 + `/ros_bridge`；话题 `/g1/asr_text`、`/g1/tts`、`/g1/robot_state`、`/g1/action_request` 均 1:1；`/g1/robot_state` 持续发布真实数据（action age=1~59ms、battery VALID）。

## E. 服务恢复验证

| 测试 | 操作 | 结果 |
|---|---|---|
| 单节点 restart | `systemctl --user restart g1-robot-state-provider` | MainPID 14146 → **14958**，active；`/g1/robot_state` 恢复发布 ✅ |
| 异常退出自愈 | `kill -9 14958` | 状态短时 `activating` → 6 秒后 MainPID **15020** active，单实例 ✅ |
| 统一停止 | `systemctl --user stop g1-voice-stack.target` | target inactive，五个服务全部 inactive，进程数 **0**，5 秒后仍停止（无自愈）✅ |
| 统一启动 | `systemctl --user start g1-voice-stack.target` | 五个服务全部 active（新 MainPID 16583/16584/16585/16589/16592）✅ |
| 日志独立可查 | `tail ~/g1_logs_systemd/*.log` / `journalctl --user -u` | 各节点日志独立、重启后继续追加（TTL 180 行等）✅ |

业务回归（systemd 迁移后，共 7 项，其中前 4 项为文本注入回归、后 3 项同上）：

| 输入 | 结果 | Ollama |
|---|---|---|
| 你现在有多少电量 | "我的电池电量大约是百分之36。"（真实 BMS，连续观测 40→39→36） | ❌ |
| 你的IMU数据正常吗 | "持续接收到IMU数据…未完成完整的传感器健康诊断。" | ❌ |
| 你现在运行状态怎么样 | 汇总真实字段："核心状态数据正在持续接收。电池电量大约百分之39。IMU数据接收正常。当前没有在执行动作。" | ❌ |
| 你现在正在执行什么动作 | "我目前没有在执行动作，手臂处于空闲状态。" | ❌ |
| 脑机接口是什么 | GENERAL_QA → NORMAL → Ollama 返回 198B（7.4s）并播报 | ✅ |
| 实验室当前状态怎么样 | LAB_INTRO → KB 未命中 → 本地兜底 | ❌ |
| 宇树G1多少钱 | PRICE 拦截 → 价格兜底 | ❌ |

（说明：本阶段业务回归以 ROS 文本注入完成，用于验证 systemd 迁移未影响业务路由；真实麦克风全链路已于 P0-18.1 同日验证，迁移只改变进程管理方式。）

## F. 开机自启动

- `systemctl --user enable g1-voice-stack.target` → 已创建 `default.target.wants/g1-voice-stack.target` 符号链接，`is-enabled = enabled`。
- `Linger=yes`（`loginctl enable-linger unitree` 免密成功）→ 用户管理器在开机后即启动，无需登录。
- 开机依赖：target → `Wants=` 五个服务；服务间 `After=` 顺序启动。
- ros_bridge 晚启动的兼容性：Provider 在无话题时输出 `SOURCE_MISSING`，Router 有缓存过期（5s）与兜底逻辑，TTS/ASR/Executor 均容忍话题暂缺；因此无需强制等待 ros_bridge，也不会产生多实例。
- **未执行真实 G1 重启验证**（任务书禁止）：`开机自启动配置已启用，尚未通过真实重启验证`。

## G. 安全情况

```text
是否触发机器人动作：否（Executor 启动后 Arm State id=0 空闲，全程未发动作请求）
是否修改 DDS/CYCLONEDDS_URI：否
是否修改 ros_bridge 生命周期管理：否
是否重启 G1：否
是否出现重复节点：否（每节点唯一 MainPID，ps 精确匹配计数为 1/1/1/1/1）
是否异常重启循环：否（StartLimit 5/60s 未触发）
是否覆盖历史备份：否
是否修改任何 C++ 业务代码：否（本轮零代码修改）
```

**观察项**：迁移期间电池从 50% 降至 36%（约 1.5 小时），与 P0-17 观测的"待机约 12%/h"一致，属真实放电；演示前需充满电。

## H. 回滚方案（每个节点独立）

```bash
# 1) 停止并禁用 systemd 管理（不影响其他节点）
systemctl --user stop g1-voice-router.service
systemctl --user disable g1-voice-router.service   # 若曾单独 enable

# 2) 恢复原有 nohup 方式
bash -lc 'source ~/unitree_ros2/setup.sh; source ~/unitree_ros2/example/install/setup.bash; \
  nohup ros2 run g1_voice_router voice_router > ~/g1_logs_p0181/voice_router_rollback.log 2>&1 &'

# 3) 整体回滚
systemctl --user stop g1-voice-stack.target
systemctl --user disable g1-voice-stack.target
# 再按各自 nohup 命令恢复（Executor 需额外 source ~/lidar_ws/install/setup.bash）
```

- 备份：`/home/unitree/g1_backups/g1_systemd_20261102_before_p0182/`（进程树、cmdline、环境白名单、二进制 SHA256、P0-18.1 运行日志）
- 本地备份：`/home/<user>/Mick_Grey/g1_dev/teach/g1_kb_work/backups/g1_robot_state_production_20261102_before_p0181_local/`（P0-18.1 源码）；本轮未改源码，无新增源码备份需求
- 证据：`evidence_p0182/`（unit 文件、启动脚本、服务状态快照、五节点日志）

## 验收对照（任务书十五）

1-3（五节点 systemd、独立 unit、统一 target）✅；4-6（环境、Executor overlay、Domain/CycloneDDS 不变）✅；7（唯一实例）✅；8-11（Provider 真实状态、Router 订阅、ASR/TTS、Executor 空闲）✅；12（单节点 restart）✅；13（异常退出自愈实测）✅；14（日志独立可查）✅；15（target 已 enable）✅；16（回滚可执行）✅；17（未重启 G1）✅；18（未触发动作）✅；19（报告与证据）✅。

## 待办 / 未验证

1. **真实 G1 重启后的自动恢复未验证**（需用户批准后单独安排）。
2. 若需要系统级服务（`/etc/systemd/system`），需用户提供 sudo 或手工执行一次拷贝安装（unit 文件与脚本已就绪，可直接复制）。
3. 日志文件未配置轮转（单文件 append），长期运行建议加 logrotate 或定期清理。
4. 温度/电流单位、关节映射、IMU 健康诊断、Odom 接入等业务遗留项同 P0-18.1。
