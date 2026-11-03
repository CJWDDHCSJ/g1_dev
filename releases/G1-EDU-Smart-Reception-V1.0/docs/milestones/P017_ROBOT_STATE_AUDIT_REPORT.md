# P0-17 G1-EDU 机器人真实状态数据源审计报告

- 日期：2026-11-01
- 性质：**只读审计**，无任何代码/配置修改，无节点重启，无动作触发。
- 证据目录：`/home/<user>/Mick_Grey/g1_dev/teach/g1_kb_work/evidence_p017/`
- 报告位置：`/home/<user>/Mick_Grey/g1_dev/teach/g1_kb_work/P017_ROBOT_STATE_AUDIT_REPORT.md`
- 实测路径核实：本地工作根目录确为 `/home/<user>/Mick_Grey/g1_dev/teach/`，`g1_kb_work/` 与 `evidence_p016/` 均存在。

证据分级标注：
- 【接口确认】= 已由 `ros2 interface show` 确认
- 【采样确认】= 已由 `ros2 topic echo --once` 真实采样确认
- 【频率确认】= 已由 `ros2 topic hz` 持续采样确认
- 【源码确认】= 已读真实源码
- 【未验证】= 尚未确认，禁止当成事实

---

## A. 当前系统运行状态

| 项目 | 实测值 |
|---|---|
| 主机名 / 架构 | `ubuntu` / aarch64 |
| 系统 | Ubuntu 22.04.5 LTS |
| ROS_DISTRO | humble；RMW=rmw_cyclonedds_cpp；ROS_DOMAIN_ID 未导出（默认 0） |

当前相关进程（【已验证】ps -eo pid,ppid,stat,etime,cmd）：

| 节点 | 包装进程 PID | 节点进程 PID | 已运行 |
|---|---|---|---|
| asr_json_bridge | 8243 | 8247 | 16h43m |
| voice_router | 8244 | 8270 | 16h43m |
| g1_tts_node | 8245 | 8248 | 16h43m |
| g1_action_executor_node | 8878 | 8880 | 8h48m |

当前节点：`/audio_client`（TTS 的 Unitree 客户端节点名）、`/g1_action_executor`、`/g1_asr_bridge`、`/g1_tts_node`、`/g1_voice_router`、`/ros_bridge`（原生）。

### 状态相关话题总览（ros2 topic list -t 实测）

| 话题 | 消息类型 | 备注 |
|---|---|---|
| `/lowstate` | unitree_hg/msg/LowState | **主数据源**（G1 用 hg 版） |
| `/lf/lowstate` | unitree_go + unitree_hg LowState 双类型共存 | 双类型话题，订阅需指定类型 |
| `/sportmodestate` | unitree_hg/msg/SportModeState | **运动/FSM 状态源** |
| `/lf/sportmodestate` | unitree_go + unitree_hg 双类型 | 同上 |
| `/odommodestate`、`/lf/odommodestate` | unitree_go/msg/SportModeState | go 版字段（位置/速度等） |
| `/arm/action/state` | std_msgs/msg/String | **原生手臂动作状态（JSON 字符串）** |
| `/dog_odom` | nav_msgs/msg/Odometry | 里程计 |
| `/dog_imu_raw`、`/utlidar/imu_livox_mid360` | sensor_msgs/msg/Imu | 辅助 IMU |
| `/secondary_imu` | unitree_hg/msg/IMUState | 副 IMU |
| `/lowstate_doubleimu` | unitree_hg_doubleimu/msg/doubleIMUState | 双 IMU 聚合（未深查） |
| `/lf/bmsstate` | unitree_hg/msg/BmsState | **电池核心数据源** |
| `/lf/agvbmsstate` | unitree_hg/msg/AgvBmsState | **Publisher count: 0 → UNAVAILABLE** |
| `/lf/battery_alarm` | std_msgs/msg/String | 电池告警 JSON（事件型） |
| `/g1/action_result` | std_msgs/msg/String | 本项目 Action Executor 发布（pub=1, sub=0） |

---

## B. 状态话题审计表

| 话题 | 消息类型 | Publisher（实测） | 频率（实测 hz） | 关键字段（实测采样） | 可用性 |
|---|---|---|---:|---|---|
| `/lowstate` | unitree_hg/msg/LowState | 1（原生 bare-DDS，RELIABLE；订阅 5 个，含 g1_action_executor BEST_EFFORT） | **1053 Hz 连续稳定** | mode_pr=0, mode_machine=5, tick 递增，imu_state，motor_state[35] | ✅ 可用 |
| `/sportmodestate` | unitree_hg/msg/SportModeState | 1（RELIABLE；订阅 2，含 executor） | **100.0 Hz 连续稳定** | fsm_id=501, fsm_mode=0, task_id=4, task_time=0 | ✅ 可用 |
| `/arm/action/state` | std_msgs/msg/String | 1（RELIABLE；订阅 3，含 executor） | **9.9 Hz 连续**（非事件型！） | `{"holding": false, "id": 0, "name": ""}` | ✅ 可用 |
| `/dog_odom` | nav_msgs/msg/Odometry | 1（RELIABLE） | **1047 Hz 连续稳定** | header.stamp 当前时刻，frame odom / robot_center，position z=0.709（站立），四元数有效 | ✅ 可用 |
| `/lf/bmsstate` | unitree_hg/msg/BmsState | 1（RELIABLE；订阅 6） | **20.2 Hz 连续稳定** | soc=66/73, soh=94, current=-2197, bmsvoltage=[50173,50190,0], cell_vol 13 串有效, temperature=[29,27,26,31], cycle=20 | ✅ 可用 |
| `/secondary_imu` | unitree_hg/msg/IMUState | 1 | **1052.8 Hz 连续** | 四元数/角速度/加速度/rpy/temperature=80 | ✅ 可用 |
| `/lf/battery_alarm` | std_msgs/msg/String | 1 | 未测（事件型，含 timestamp 字段） | `{"alarm_status":0,"timestamp":"1793…","cell_voltages":[…]}` | ⚠ 事件型，仅告警用 |
| `/lf/agvbmsstate` | unitree_hg/msg/AgvBmsState | **0** | 无 | — | ❌ UNAVAILABLE |

QoS 要点【采样确认】：原生发布者 RELIABLE/VOLATILE，现有 executor 以 BEST_EFFORT 订阅且工作正常；未来 Provider 订阅可用 BEST_EFFORT（兼容且不增加原生端负担）。

---

## C. 机器人状态能力矩阵

| 状态 | 真实数据源 | 字段路径 | 单位 | 频率 | 可用于回答？ |
|---|---|---|---|---|---|
| **电池电量** | ✅ `/lf/bmsstate` | `soc`（uint8） | **%（采样两次 73→66，确认在更新）** | 20Hz | ✅ 可回答"电量百分比" |
| 电池总电压 | ✅ 同上 | `bmsvoltage[0]`（uint32） | **mV（50173 ≈ 13×3860mV，交叉验证一致）** | 20Hz | ✅ |
| 单体电芯电压 | ✅ 同上 | `cell_vol[40]`（前 13 有效） | mV | 20Hz | ✅（汇总口径） |
| 电池电流 | ⚠ 同上 | `current`（int32，实测 -2197） | **未确认**（疑似 mA，符号含义未知） | 20Hz | ⚠ 不建议对外回答 |
| 电池健康度 | ✅ 同上 | `soh`（uint8=94） | %（推断） | 20Hz | ✅ |
| 电池温度 | ✅ 同上 | `temperature[12]`（前 4 有效：29/27/26/31） | **°C（推断，未确认）** | 20Hz | ⚠ 需注明"约" |
| 电池循环次数 | ✅ 同上 | `cycle`=20 | 次 | 20Hz | ✅ |
| **电机温度** | ✅ `/lowstate` | `motor_state[i].temperature`（int16[2]，实测 32–49） | **°C（推断，未确认）** | 1053Hz | ✅（用最大值/范围口径；**索引→关节名映射未知**，不得报具体关节名） |
| 关节位置 | ✅ `/lowstate` | `motor_state[i].q` | rad（推断，未确认） | 1053Hz | ⚠ 仅做定性判断（如"手臂在动"） |
| 关节速度 | ✅ `/lowstate` | `motor_state[i].dq` | rad/s（推断，未确认） | 1053Hz | ⚠ 仅定性 |
| 关节力矩 | ✅ `/lowstate` | `motor_state[i].tau_est` | N·m（推断，未确认） | 1053Hz | ⚠ 仅定性 |
| **IMU 姿态** | ✅ `/lowstate.imu_state` 与 `/secondary_imu` | `quaternion[4]`、`rpy[3]` | rad（rpy，采样合理） | ~1053Hz | ✅（可回答朝向/是否直立，定性） |
| IMU 加速度 | ✅ 同上 | `accelerometer[3]`（z≈9.79–9.80） | **m/s²（由 z≈9.8 交叉验证）** | ~1053Hz | ⚠ 定性 |
| IMU 角速度 | ✅ 同上 | `gyroscope[3]` | rad/s（推断） | ~1053Hz | ⚠ 定性 |
| IMU 温度 | ✅ 同上 | `temperature`（int16，实测 79/80） | **未确认**（79 既可能是 °C 也可能带系数） | ~1053Hz | ❌ 不对外回答 |
| **动作状态** | ✅ `/arm/action/state` | JSON `id/holding/name`（id=0 空闲；executor 日志中执行 wave 时 name=wave_above_head, id=26） | —（String JSON） | 10Hz | ✅ |
| 动作结果 | ✅ `/g1/action_result`（本项目 executor） | `accepted:<name>:<id>` / `success:…` 字符串 | — | 事件型 | ✅ |
| 运动 FSM | ✅ `/sportmodestate` | `fsm_id`=501（与 executor 日志 FSM=501 一致）、`fsm_mode`、`task_id`、`task_time` | 枚举值含义未确认 | 100Hz | ⚠ 定性（"运动模式正常"） |
| 里程计位置 | ✅ `/dog_odom` | `pose.pose.position/orientation` | m / 四元数 | 1047Hz | ⚠ 仅定性（原点位姿） |
| AGV BMS | ❌ `/lf/agvbmsstate` | — | — | — | ❌ UNAVAILABLE |

**关键交叉验证**：`bmsvoltage≈50173mV` 与 13 串 `cell_vol≈3860mV` 之和吻合 → 电压单位 mV 置信度高；`accelerometer.z≈9.8` → m/s² 置信度高。其余单位均为推断，**对外播报时必须规避或模糊化**。

---

## D. ROBOT_STATUS 当前实现审计

### D.1 意图识别【源码确认 + 离线探针实测】

`ROBOT_STATUS` 意图存在（`intent_recognizer.cpp`）。用真实源码编译的临时探针（/tmp，已删除）实测 15 种问法：

| 问法 | 意图 |
|---|---|
| 你现在运行状态怎么样 | ROBOT_STATUS |
| 机器人现在正常吗 | ROBOT_STATUS |
| 你现在有多少电量 / 你现在的电池电量是多少 / 你现在还有多少电量 / 机器人电量还剩多少 | ROBOT_STATUS |
| 你的IMU数据正常吗 | ROBOT_STATUS |
| 实验室当前状态怎么样 | ROBOT_STATUS（错位：实验室问题进入机器人状态意图，现无实际影响） |
| **你的关节温度是多少** | **GENERAL_QA**（意图漏判） |
| **你现在正在执行什么动作** | **GENERAL_QA**（意图漏判） |
| 你关节温度高不高 / 动作做完了吗 / 你累不累 / 你现在蹲着还是站着 | GENERAL_QA |

与 DEMO_ACTION 无冲突（"给我们表演一个动作"→DEMO_ACTION 正确）。

### D.2 Voice Router 分流逻辑【源码确认：voice_router.cpp `enqueue_user_text()`】

ROBOT_STATUS **没有独立处理分支**。实际路径（`~/unitree_ros2/example/g1_voice_router/src/voice_router.cpp`）：

1. GREETING 固定匹配（`match_greeting_reply`）→ 不适用。
2. LAB_INTRO / ROBOT_INTRO / GOODBYE 本地分支 → 不适用。
3. **P0-15 FactRiskGuard 检查（`fact_risk_guard_.evaluate`）**：
   - 电量类（"电量""电池"）→ `REALTIME_STATUS` 拦截 → 兜底"暂时无法确认"，**不调 Ollama**（P0-16 真机已验证）。
   - 关节温度类（"温度"+"关节/机器人语境"）→ `REALTIME_STATUS` 拦截。
4. 其余 → 进入 Ollama 队列（`worker_loop` → `query_ollama`）。

**当前无任何真实状态数据读取逻辑**——机器人状态问题要么被 Guard 兜底，要么由大模型自由发挥。

### D.3 FactRiskGuard 保护边界【源码确认 + 探针实测】(`fact_risk_guard.cpp`)

| 问法 | Guard 判定 | 后果 |
|---|---|---|
| 机器人现在正常吗 | REALTIME_STATUS 拦截 ✅ | 兜底，安全 |
| 你现在有多少电量 / 报一下你的电池电量 / 你现在还有多少电量 | REALTIME_STATUS 拦截 ✅ | 兜底，安全 |
| 你的关节温度是多少 / 你关节温度高不高 | REALTIME_STATUS 拦截 ✅（"关节"语境词） | 兜底，安全 |
| **你现在运行状态怎么样** | **NORMAL 放行** ❌（"状态怎么样"需机器人语境词，但语境表只有 机器人/g1/宇树/unitree/关节/自身/本体，**不含"你"**） | **进入 Ollama，可能编造** |
| **你的IMU数据正常吗** | **NORMAL 放行** ❌（IMU 不在关键词表） | **进入 Ollama，可能编造** |
| **你现在正在执行什么动作 / 动作做完了吗** | **NORMAL 放行** ❌ | **进入 Ollama，可能编造** |
| 你现在蹲着还是站着 / 你累不累 | NORMAL 放行 ❌ | 进入 Ollama（低风险闲聊） |

**结论：Guard 目前只可靠拦截"电量"和"温度（带语境）"两类；状态类问题存在 4 个漏放通道。**

### D.4 未来接入 RobotStateProvider 的集成方式（不删除保护）

推荐：在 `voice_router.cpp` 中、**FactRiskGuard 之前**新增 ROBOT_STATUS 本地分支：
- 意图==ROBOT_STATUS 时先尝试 `state_provider` 查询；有 VALID 数据 → 本地生成回答（C++ 模板拼句），**不进 Guard、不进 Ollama**。
- 数据 STALE/UNAVAILABLE → 沿用现有 Guard 兜底话术。
- 保留 Guard 的 REALTIME_STATUS 规则作为兜底防线：覆盖 GENERAL_QA 误分类（如"你的关节温度是多少"被判 GENERAL_QA）时仍然拦截。**规则不删，只加分支。**
- 顺带修复建议（下一阶段）：Guard 语境词表增加"你/您"、关键词表增加"IMU/动作/电量百分比"，堵住漏放通道（需先补单测再改）。

---

## E. RobotStateProvider 设计方案（基于实测，仅设计不实现）

### E.1 架构选择

推荐**独立 ROS 2 节点 `g1_state_provider`**（放 `~/unitree_ros2/example/` 新包，C++17），对外发布聚合状态 JSON 到 `std_msgs/String` 话题 `/g1/robot_state`（1–2Hz），voice_router 订阅并缓存最近一条。

理由：与现有 `/arm/action/state`（String JSON）风格一致；router 不直接接触 unitree_hg 原生消息（满足"不把 ROS 原始结构暴露给语音层"）；单测可对 JSON/缓存逻辑离线进行；router 侧改动最小。

### E.2 订阅关系（实测类型）

```text
/lf/bmsstate      (unitree_hg/msg/BmsState, 20Hz, BEST_EFFORT)   → BatteryState
/lowstate         (unitree_hg/msg/LowState, ~1kHz, BEST_EFFORT)  → ImuState + JointStateSummary
/sportmodestate   (unitree_hg/msg/SportModeState, 100Hz)         → MotionState(fsm_id/fsm_mode)
/arm/action/state (std_msgs/String JSON, 10Hz)                   → ActionState(id/name/holding)
/g1/action_result (std_msgs/String, 事件)                        → LastActionResult(带接收时刻)
/dog_odom         (nav_msgs/msg/Odometry, ~1kHz, 可选)           → OdomSummary
```

注意：**必须订阅 unitree_hg 版本**（/lf/ 下存在 go+hg 双类型共存，显式指定类型避免歧义）。低频话题（bms 20Hz）之外，/lowstate 1kHz 建议订阅 QoS 用 BEST_EFFORT + 只取需要的字段，避免 CPU 压力（Orin NX 上 1kHz 回调仅做浅拷贝缓存）。

### E.3 数据结构（封装，不暴露原生消息）

```cpp
enum class DataValidity { VALID, STALE, UNAVAILABLE, INVALID };

struct BatteryState {          // 来自 /lf/bmsstate
  DataValidity validity;
  rclcpp::Time recv_time;      // ROS 本地接收时刻
  int soc_percent{-1};         // soc, 0-100
  int soh_percent{-1};
  long pack_voltage_mv{-1};    // bmsvoltage[0]
  long current_raw{-1};        // current, 单位未确认，原样透传
  int max_cell_voltage_mv{-1}; // cell_vol 最大值
  int cycle_count{-1};
};

struct JointStateSummary {     // 来自 /lowstate.motor_state
  DataValidity validity;
  rclcpp::Time recv_time;
  int active_motor_count{-1};  // 实测 29
  int max_temperature{-1};     // 全部温度通道最大值（°C 推断）
  int temperature_slot{-1};    // 该值来自哪个索引（内部诊断用）
  bool any_motion{false};      // 任一 |dq| 超阈值（定性）
};

struct ImuSummary {            // 来自 /lowstate.imu_state
  DataValidity validity;
  rclcpp::Time recv_time;
  float rpy_rad[3]{};          // 朝向
  float accel_z{-1};           // ≈9.8 为直立参考
};

struct ActionState {           // 来自 /arm/action/state + /g1/action_result
  DataValidity validity;
  rclcpp::Time recv_time;
  int action_id{-1};           // 0=空闲
  std::string action_name;     // "" / "wave_above_head"…
  bool holding{false};
  std::string last_result;     // 最近一次 success/accepted（带时间）
};

struct MotionState {           // 来自 /sportmodestate
  DataValidity validity;
  rclcpp::Time recv_time;
  uint32_t fsm_id{0}, fsm_mode{0}, task_id{0};
};
```

### E.4 数据超时与有效性策略（分话题差异化，基于实测频率）

| 话题 | 名义周期 | 有效窗口 VALID→STALE | 说明 |
|---|---:|---:|---|
| /lowstate | ~1ms | 1.0 s | tick 仅用于递增校验，**不作绝对时钟**（1kHz 下 uint32 约 71.6 分钟回绕，已实测递增） |
| /sportmodestate | 10ms | 1.0 s | |
| /arm/action/state | 100ms | 2.0 s | 实测持续 10Hz，不是事件型 |
| /lf/bmsstate | 50ms | 2.0 s | soc 变化慢，数值陈旧可接受，话题陈旧不可接受 |
| /g1/action_result | 事件型 | 不设超时 | 保留最近一条 + 接收时刻，播报时给出"距今 X 秒" |
| 未收到过 | — | UNAVAILABLE | 如 /lf/agvbmsstate |

- 无消息内时间戳（hg 消息只有 tick）→ **统一用 ROS 节点本地接收时刻 `now()` 判陈旧**；/dog_odom 有 header.stamp 可双重校验。
- 字段级有效性：soc=-1/温度=0 的无效通道（实测 cell_vol 后 27 位、temperature 后 8 位为 0）在汇总时跳过，不算 INVALID。
- 异常值：温度 > 90 或 soc > 100 视为 INVALID 并告警日志。
- 播报策略：只对 VALID 状态给出数值；STALE → "这个数据暂时没有更新，我无法确认"；UNAVAILABLE → 现有兜底话术。

### E.5 ROBOT_STATUS 集成位置

`voice_router.cpp` 新增分支（Guard 之前）：

```text
Intent recognized: ROBOT_STATUS
  → 解析最近一条 /g1/robot_state JSON
  → 按 IntentRecognizer 细分类（电量/温度/动作/姿态/总体）选择回答模板
  → publish_reply(本地回答)   // 不进 Guard、不进 Ollama
  → 数据非 VALID 时回落到 FactRiskGuard 现有兜底
```

### E.6 需要新增的单元测试

1. 缓存与超时：注入递增接收时刻，验证 VALID→STALE→（长时间）UNAVAILABLE 迁移。
2. 字段汇总：构造含 0/异常值的 BMS/MotorState，验证无效通道跳过、max 计算正确。
3. JSON 序列化/解析往返（/g1/robot_state 生成 + router 解析）。
4. 播报模板：soc=66 → "我的电量大约是 66%"；soc=-1 → 兜底句。
5. Guard 回归：新增语境词后原 71 项测试不回归 + 新增漏放通道用例（运行状态怎么样/IMU/执行什么动作）。
6. 隔离域运行时：DOMAIN_ID=77 发布模拟 /arm/action/state 与 /g1/action_result 验证聚合（不碰真机原生话题）。

### E.7 预计修改/新增文件（下一阶段，本轮未创建）

```text
~/unitree_ros2/example/g1_state_provider/          # 新包（CMakeLists/package.xml/src/include/test）
~/unitree_ros2/example/g1_voice_router/src/voice_router.cpp   # ROBOT_STATUS 分支
~/unitree_ros2/example/g1_voice_router/src/fact_risk_guard.cpp # 语境词补充（带回归测试）
~/unitree_ros2/example/g1_voice_router/test/…                  # 新测试
```

---

## F. 风险与限制

1. **单位未确认项**（对外播报前必须解决或规避）：电机/IMU 温度（°C 推断）、q/dq/tau（rad、rad/s、N·m 推断）、BMS current（-2197，单位与符号未知）。
2. **motor_state 索引→关节名映射未知**：只能回答"最高电机温度约 X 度"，不能回答"左髋关节温度"。
3. **soc 读数波动**：两次采样间隔约 40 分钟 73→66，可能是负载相关估计跳变；建议 Provider 做变化率 sanity check（如单次跳变 >10% 时标注 INVALID 并告警）。
4. mode_pr/mode_machine/fsm_mode 的枚举语义未确认（fsm_id=501 与 executor 日志一致是唯一已验证锚点）。
5. `/lf/*` 双类型话题陷阱：订阅时必须显式 `unitree_hg` 类型。
6. `/lf/agvbmsstate` 无发布者：UNAVAILABLE。
7. 深度相机异常与状态感知无关，不在此范围。

## G. 证据

命令与输出已存档：`/home/<user>/Mick_Grey/g1_dev/teach/g1_kb_work/evidence_p017/audit_evidence.md`
（含 topic list -t、info -v、interface show、echo 采样、hz 实测、意图/Guard 探针输出；P0-15/P0-16 证据未触碰。）
探针程序使用 /tmp 独立编译，运行后已删除；G1 端临时采样文件已删除。

## H. 变更检查

```text
源码修改：0
配置修改：0
生产节点重启：0
生产节点终止：0
机器人动作触发：0
历史备份覆盖：0
```

本阶段仅执行 SSH 只读命令、`ros2 topic type/info/interface show/echo --once/hz` 只读采样、本地 /tmp 临时探针（已清理），并新增了本报告与证据文件。
