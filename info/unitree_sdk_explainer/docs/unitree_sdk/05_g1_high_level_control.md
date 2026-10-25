# 05 · G1 高层控制 API

> **安全边界**：本章记录的是**会真实驱动 G1 的接口**。本文只做接口分析，未执行任何运动；示例以“读取/列出/查询”为主。任何运动调用都必须在有人值守、场地清空、急停可用的条件下进行。
>
> 证据标记见 [README.md](README.md)。

---

## 1. 模块定位

G1 高层 API 是“告诉机器人做什么”，由机器人内置的运动服务执行，SDK 只负责 RPC 调用：

| 客户端 | 服务名 | 控制对象 | 典型风险 |
|---|---|---|---|
| `g1::LocoClient` | `sport` | 全身运动、FSM、速度、站姿、手臂任务 | 机器人跌倒、与 App/手柄争夺控制权 |
| `g1::G1ArmActionClient` | `arm` | 上半身预设/示教动作 | 手臂撞击、动作期间无法响应 |
| `g1::AudioClient` | `voice` | TTS、PCM 播放、音量、RGB | 低风险；播放阻塞/队列问题 |
| `g1::AgvClient` | `agv` | G1D 底盘/升降柱 | 底盘移动、夹伤 |
| `b2::MotionSwitcherClient` | `motion_switcher` | 释放/查询高层运控模式 | 释放后机器人可能失去平衡 |

所有客户端都继承 `Client`，调用语义、超时、错误码见 [02_dds_rpc_client_server.md](02_dds_rpc_client_server.md)。本文只讲**方法级语义**。

---

## 2. 源码位置

| 文件 | 内容 |
|---|---|
| `include/unitree/robot/g1/loco/g1_loco_api.hpp` | 服务名、版本、API ID、JSON 结构 |
| `include/unitree/robot/g1/loco/g1_loco_client.hpp` | `LocoClient` 全部方法 |
| `include/unitree/robot/g1/loco/g1_loco_error.hpp` | 7301–7303 |
| `include/unitree/robot/g1/arm/g1_arm_action_api.hpp` | 服务名、版本、API ID |
| `include/unitree/robot/g1/arm/g1_arm_action_client.hpp` | 动作执行/列表/停止 |
| `include/unitree/robot/g1/arm/g1_arm_action_error.hpp` | 7400–7402、7404（无 7403）、FSM 约束 |
| `include/unitree/robot/g1/audio/g1_audio_api.hpp` | 服务名、版本、API ID、JSON |
| `include/unitree/robot/g1/audio/g1_audio_client.hpp` | TTS/播放/音量/LED |
| `include/unitree/robot/g1/agv/g1_agv_api.hpp` | AGV 服务、API ID |
| `include/unitree/robot/g1/agv/g1_agv_client.hpp` | 底盘速度、升降柱 |
| `include/unitree/robot/b2/motion_switcher/motion_switcher_client.hpp` | 模式查询/选择/释放 |
| `example/g1/high_level/g1_loco_client_example.cpp` | 只读查询与命令示例 |
| `example/g1/high_level/g1_arm_action_example.cpp` | 动作列表/执行 |
| `example/g1/high_level/g1_arm7_sdk_dds_example.cpp` | 7 DOF 手臂低层接管 |
| `example/g1/high_level/g1_userctrl_dds_example.cpp` | 用户控制切换流程 |
| `example/g1/audio/g1_audio_client_example.cpp` | TTS 与 PCM 播放 |
| `example/g1/g1d/g1_agv_client_example.cpp` | AGV 示例 |

---

## 3. 服务与 API ID

### 3.1 `sport`（`LocoClient`）

【源码】`g1_loco_api.hpp:11`（服务名）、`:14`（版本 `1.0.0.0`）、`:17-33`（API ID）。

| API ID | 常量 | 方向 | 说明 |
|---|---|---|---|
| 7001 | `GET_FSM_ID` | 读 | 当前 FSM |
| 7002 | `GET_FSM_MODE` | 读 | 当前子模式 |
| 7003 | `GET_BALANCE_MODE` | 读 | 平衡模式 |
| 7004 | `GET_SWING_HEIGHT` | 读 | 摆腿高度 |
| 7005 | `GET_STAND_HEIGHT` | 读 | 站立高度 |
| 7006 | `GET_PHASE` | 读 | 步态相位，**已标记 deprecated** |
| 7100 | `FSM_API` | 写 | 通用 FSM 扩展入口 |
| 7101 | `SET_FSM_ID` | 写 | 切换 FSM |
| 7102 | `SET_BALANCE_MODE` | 写 | 设置平衡/连续步态 |
| 7103 | `SET_SWING_HEIGHT` | 写 | 设置摆腿高度 |
| 7104 | `SET_STAND_HEIGHT` | 写 | 设置站立高度 |
| 7105 | `SET_VELOCITY` | 写 | 速度命令 + 持续时间 |
| 7106 | `SET_ARM_TASK` | 写 | 手臂任务（挥手/握手） |
| 7107 | `SET_SPEED_MODE` | 写 | 速度档位 |
| 7110 | `SWITCH_TO_USER_CTRL` | 写 | 切到用户低层控制 |
| 7111 | `SWITCH_TO_INTERNAL_CTRL` | 写 | 切回内部运控 |

> 注意：`arm` 服务的 API ID 也是 7106/7107/7108/7113，但**服务名不同**（`arm` vs `sport`），ID 空间互不影响。不要按 ID 猜服务。

### 3.2 `arm`（`G1ArmActionClient`）

【源码】`g1_arm_action_api.hpp:10`（`arm`）、`:13`（`1.0.0.14`）、`:16-19`：

| API ID | 方法 |
|---|---|
| 7106 | `ExecuteAction(int32_t)` |
| 7107 | `GetActionList` |
| 7108 | `ExecuteAction(const std::string&)`（示教动作） |
| 7113 | `StopCustomAction` |

### 3.3 `voice`（`AudioClient`）

【源码】`g1_audio_api.hpp:11`（`voice`）、`:14`（`1.0.0.0`）、`:17-23`：

| API ID | 方法 | 说明 |
|---|---|---|
| 1001 | `TtsMaker` | 文本转语音 |
| 1002 | `Asr` | 语音识别（客户端未封装公开方法） |
| 1003 | `PlayStream` | 播放 PCM 流 |
| 1004 | `PlayStop` | 停止播放 |
| 1005 | `GetVolume` | 读音量 |
| 1006 | `SetVolume` | 设音量 |
| 1010 | `LedControl` | RGB 灯 |

### 3.4 `agv`（`AgvClient`，G1D）

【源码】`g1_agv_api.hpp:15`（`agv`）、`:18`（`1.0.0.1`）、`:21-22`：`1001 Move`、`1002 HeightAdjust`。

### 3.5 `motion_switcher`

【源码】`motion_switcher_api.hpp:15`（`motion_switcher`）、`:20`（`1.0.0.1`）、`:25-29`：`CheckMode 1001`、`SelectMode 1002`、`ReleaseMode 1003`、`SetSilent 1004`、`GetSilent 1005`。

---

## 4. `LocoClient` 只读 API（P0）

这些方法**不改变机器人运动状态**，适合部署前的连通性/状态检查。

| 方法 | 行号 | 参数 | 返回 | 副作用 | 线程安全 | 典型场景 |
|---|---|---|---|---|---|---|
| `GetFsmId(int&)` | `g1_loco_client.hpp:40` | 输出 `fsm_id` | RPC 码 | 无 | 【未验证】建议串行 | 判断是否可执行高层动作 |
| `GetFsmMode(int&)` | `:54` | 输出 `fsm_mode` | RPC 码 | 无 | 同上 | 801 模式下判断 0/3 |
| `GetBalanceMode(int&)` | `:68` | 输出 `balance_mode` | RPC 码 | 无 | 同上 | 判断平衡/连续步态 |
| `GetSwingHeight(float&)` | `:82` | 输出高度 | RPC 码 | 无 | 同上 | 读取步态参数 |
| `GetStandHeight(float&)` | `:96` | 输出高度 | RPC 码 | 无 | 同上 | 读取站姿 |
| `GetPhase(vector<float>&)` | `:110` | 输出相位 | RPC 码 | 无 | 同上 | **deprecated**，不建议新代码使用 |

返回值处理：只有 `ret == 0` 时才读取输出参数。客户端代码没有检查 `FromJsonString` 的返回值；解析失败时 RPC 层是否返回 `3106`（`Response data error`）**未验证**，不要把 `3106` 当作已确认的解析失败专用码。

`GetPhase` 在 `Init()` 中仍被注册（`g1_loco_client.hpp:25`），但 API 头注释明确 deprecated（`g1_loco_api.hpp:22`）。

---

## 5. `LocoClient` 写 API（P2，需授权）

> 以下调用会改变机器人状态。表中“副作用”仅描述**接口行为**，不代表已实测。

| 方法 | 行号 | 参数 | 底层 API | 副作用 | 典型场景 |
|---|---|---|---|---|---|
| `SetFsmId(int)` | `:124` | FSM 编号 | 7101 | 切换运控状态机 | `Start/Damp/Squat/...` 的底层 |
| `SetBalanceMode(int)` | `:134` | 0/1 | 7102 | 平衡/连续步态 | `BalanceStand/ContinuousGait` |
| `SetSwingHeight(float)` | `:144` | 高度 | 7103 | 改变摆腿高度 | 步态调参 |
| `SetStandHeight(float)` | `:154` | 高度 | 7104 | 改变站姿高度 | 站高调整 |
| `SetVelocity(vx,vy,omega,duration=1.f)` | `:164` | 速度 + 持续时间（秒） | 7105 | 发送速度指令 | 遥控/导航输出 |
| `SetTaskId(int)` | `:176` | 任务 ID | 7106 | 触发手臂任务 | 挥手/握手 |
| `SwitchToUserCtrl()` | `:186` | 无（实际发送 `{"data":false}`） | 7110 | 交出低层控制权 | 自定义控制器接管前 |
| `SwitchToInternalCtrl(mode)` | `:194` | `LAST/PASSIVE/WALKRUN` | 7111 | 收回控制权 | 退出自定义控制 |
| `SetSpeedMode(int)` | `:265` | 档位 | 7107 | 改变速度档 | 场地限速 |
| `_fsm_api(string,data)` | `:303` | 原始 JSON | 7100 | 通用扩展 | 实验性功能 |

### 5.1 便捷封装的真实语义

| 方法 | 行号 | 实际发送 | 注意 |
|---|---|---|---|
| `Damp()` | `:214` | `SetFsmId(1)` | 阻尼模式 |
| `Start()` | `:216` | `SetFsmId(500)` | 进入主运控 |
| `Squat()` | `:218` | `SetFsmId(2)` | 下蹲 |
| `Sit()` | `:220` | `SetFsmId(3)` | 坐下 |
| `StandUp()` | `:222` | `SetFsmId(4)` | 站起 |
| `ZeroTorque()` | `:224` | `SetFsmId(0)` | **零力矩，机器人可能直接倒下** |
| `StopMove()` | `:226` | `SetVelocity(0,0,0)`，duration 默认 1 s | 只停 1 秒有效 |
| `HighStand()` | `:228` | `SetStandHeight(numeric_limits<uint32_t>::max())` | **类型不匹配的疑似 bug，见 9.1** |
| `LowStand()` | `:230` | `SetStandHeight(numeric_limits<uint32_t>::min())` = 0 | 同上 |
| `Move(vx,vy,vyaw,bool)` | `:232-234` | duration = `864000.f` 或 `1.f` | 见 5.2 |
| `Move(vx,vy,vyaw)` | `:236` | 使用成员 `continous_move_` | 默认 `false` |
| `BalanceStand()` | `:238` | `SetBalanceMode(0)` | — |
| `ContinuousGait(bool)` | `:240` | `SetBalanceMode(flag?1:0)` | — |
| `SwitchMoveMode(bool)` | `:242-245` | **仅改本地成员，不发 RPC** | 返回 0 不代表服务端变化 |
| `WaveHand(bool)` | `:247` | `SetTaskId(turn?1:0)` | 任务 ID 语义未文档化 |
| `ShakeHand(int stage=-1)` | `:249-263` | 在 task 2/3 间切换 | 本地状态机 |
| `GetMimicMotion(string&)` | `:298` | `fsm_id=550, api_id=1` | 查询模仿动作列表，只读 |

### 5.2 `SetVelocity` 的 `duration`

```cpp
// g1_loco_client.hpp:232-234
int32_t Move(float vx, float vy, float vyaw, bool continous_move) {
    return SetVelocity(vx, vy, vyaw, continous_move ? 864000.f : 1.f);
}
```

- `duration` 单位是**秒**（字段名与示例用法支持；IDL 注释未给出）。
- 连续移动：`864000 s`（10 天），即“直到下一条命令”。
- 单次移动：`1 s`，之后速度指令可能自动失效。
- `SwitchMoveMode(true)` 只设置本地 `continous_move_`，不改变机器人当前状态；下一条 `Move(vx,vy,vyaw)` 才生效。

【推断】`duration` 是服务端保持该速度指令的时间窗口，不是“运动 1 秒后停止”的精确时长。真实行为需实测。

### 5.3 `SwitchToUserCtrl` 的参数问题

【源码】`g1_loco_client.hpp:186-192`：

```cpp
go2::JsonizeDataBool json;   // 默认 data=false
parameter = common::ToJsonString(json);
return Call(ROBOT_API_ID_LOCO_SWITCH_TO_USER_CTRL, parameter, data);
```

- 方法没有参数，发送的 JSON 固定为 `{"data":false}`（默认构造见 `jsonize_type.hpp:41-45`）。
- 服务端是否忽略该字段、`false` 是否表示“切换”而非“使能”，**未验证**。
- 用户控制示例在调用前会把关节 `kp=0` 发一帧（`g1_userctrl_dds_example.cpp:129-136`，`Write` 在 `:136`），再切换（`:138`），再逐步给 `kp`。这是示例作者的安全做法，不是 SDK 强制。

---

## 6. FSM、控制权与切换顺序

### 6.1 已知 FSM 约束

| 约束 | 证据 |
|---|---|
| 手臂动作只在 FSM `{500, 501, 801}` 可用 | `g1_arm_action_error.hpp` 注释 |
| FSM 801 下，手臂动作只在 `fsm_mode ∈ {0,3}` 可用 | 同上 |
| 用户低层控制要求当前 FSM 为 `1`（PASSIVE） | `g1_userctrl_dds_example.cpp:95-99` |
| 低层接管前需释放高层模式 | `g1_ankle_swing_example.cpp:183-190` |

### 6.2 `motion_switcher` 的作用

【示例】`g1_ankle_swing_example.cpp:181-190`：

```cpp
msc_ = std::make_shared<unitree::robot::b2::MotionSwitcherClient>();
msc_->SetTimeout(5.0f);
msc_->Init();
std::string form, name;
while (msc_->CheckMode(form, name), !name.empty()) {
  if (msc_->ReleaseMode())
    std::cout << "Failed to switch to Release Mode\n";
  sleep(5);
}
```

语义（【推断】自示例）：

- `CheckMode(form, name)`：查询当前高层模式；`name` 为空表示已无高层模式占用。
- `ReleaseMode()`：请求释放；示例把**非 0 返回值**打印为失败，并每 5 秒重试，直到 `name` 为空。
- 释放后机器人不再由内置运控保持姿态，**可能跌倒**；示例随后创建 `rt/lowcmd` 发布者接管。

### 6.3 控制权冲突

| 场景 | 结果 | 证据 |
|---|---|---|
| App/手柄正在控制，程序发 `SetFsmId` | 可能被拒绝或覆盖，行为未验证 | — |
| 手臂正在 holding，再发动作 | 返回 7401，需 id=99 或相同 id 释放 | `g1_arm_action_error.hpp` |
| `rt/arm_sdk` 被占用 | 手臂动作返回 7400 | 同上 |
| 低层 `rt/lowcmd` 与高层 `sport` 同时发 | 结果取决于服务端仲裁，**未验证** | — |

**部署原则**：同一时刻只允许一个控制源。切换必须有明确的状态确认，而不是“发完就假设成功”。

---

## 7. `G1ArmActionClient`

### 7.1 方法

| 方法 | 行号 | 参数 | 返回 | 副作用 | 备注 |
|---|---|---|---|---|---|
| `ExecuteAction(int32_t)` | `g1_arm_action_client.hpp:51` | 动作 ID | RPC 码 | 执行预设动作 | 99 = 释放 |
| `ExecuteAction(const std::string&)` | `:67` | 示教动作名 | RPC 码 | 执行示教动作 | 无 holding |
| `StopCustomAction()` | `:74` | 无 | RPC 码 | 停止示教动作 | — |
| `GetActionList(std::string&)` | `:79` | 输出 JSON | RPC 码 | **只读** | 可安全调用 |

`ExecuteAction(name)` 直接把用户字符串拼进 JSON（`:69`）：

```cpp
parameter = R"({"action_name":")" + action_name + R"("})";
```

如果 `action_name` 含引号/反斜杠，会破坏 JSON。**不要传入外部不可信输入**；必要时先做转义。

### 7.2 预设动作映射

【源码】`g1_arm_action_client.hpp:85-102`：

| 名称 | ID | 名称 | ID |
|---|---|---|---|
| `release arm` | 99 | `two-hand kiss` | 11 |
| `left kiss` | 12 | `right kiss` | 12 |
| `hands up` | 15 | `clap` | 17 |
| `high five` | 18 | `hug` | 19 |
| `heart` | 20 | `right heart` | 21 |
| `reject` | 22 | `right hand up` | 23 |
| `x-ray` | 24 | `face wave` | 25 |
| `high wave` | 26 | `shake hand` | 27 |

注意：

- `left kiss` 与 `right kiss` **都是 12**，映射表无法区分左右。是同一动作还是表错误，**未验证**。
- 头文件注释警告：部分动作不在 App 中显示，但可由程序执行，**可能导致机器人跌倒**（`g1_arm_action_client.hpp:41-49`）。
- `holding=true` 时最长保持 20 秒（注释），之后自动释放。

### 7.3 状态查询

`rt/arm/action/state` 发布 JSON（`holding/id/name`）。SDK 未提供订阅封装，见 [04](04_robot_state.md) 第 5 节。

---

## 8. `AudioClient` 与 `AgvClient`

### 8.1 `AudioClient`

| 方法 | 行号 | 参数 | 副作用 | 备注 |
|---|---|---|---|---|
| `TtsMaker(text, speaker_id)` | `:31` | 文本、音色 | 机器人播报 | 内部自增 `tts_index` |
| `GetVolume(uint8_t&)` | `:43` | 输出音量 | 只读 | — |
| `SetVolume(uint8_t)` | `:57` | 音量 | 改变音量 | 范围未校验 |
| `PlayStream(app_name, stream_id, pcm)` | `:68` | 应用名、流 ID、PCM | 播放音频 | JSON + binary |
| `PlayStop(app_name)` | `:80` | 应用名 | 停止 | **忽略 RPC 返回值，恒返回 0** |
| `LedControl(R,G,B)` | `:91` | RGB | 改变灯色 | — |

`PlayStop` 的实现（`:80-89`）调用 `Call(...)` 后丢弃返回值并 `return 0`，因此**无法通过返回值判断停止是否成功**。示例 `g1_audio_client_example.cpp:153` 传入的是 `stream_id` 而不是 `app_name`，语义也不匹配。使用前需自行核对服务端字段。

### 8.2 `AgvClient`（G1D）

| 方法 | 行号 | 参数范围 | 副作用 |
|---|---|---|---|
| `Move(vx,vy,vyaw)` | `:56` | vx ∈ [-1.5,1.5] m/s；vyaw ∈ [-0.6,0.6] rad/s；vy 忽略 | 底盘移动 |
| `HeightAdjust(vz)` | `:93` | vz ∈ [-1,1] → 0.0765 m/s 伸缩 | 升降柱运动 |

【源码】范围来自注释 `g1_agv_client.hpp:28-50`、`:66-92`。注释同时声明 `Move` 非阻塞，发送后立即返回。

`HeightAdjust` 只使用 z 分量（注释），输入被归一化映射到最大柱速 76.5 mm/s。**这是 G1D/AGV 配置，不适用于普通 G1。**

---

## 9. 已知实现问题与限制

### 9.1 `HighStand` / `LowStand` 数值类型不匹配

```cpp
int32_t HighStand() { return SetStandHeight(std::numeric_limits<uint32_t>::max()); }
int32_t LowStand()  { return SetStandHeight(std::numeric_limits<uint32_t>::min()); }
```

`SetStandHeight` 接受 `float`（`g1_loco_client.hpp:154`）。`uint32_t::max()` 被隐式转换为约 `4.29e9`，`min()` 为 `0`。这几乎肯定不是“最高/最低站姿”的预期值。

**结论：不要使用这两个便捷方法。** 如需设定站高，调用 `SetStandHeight` 并传入经现场验证的物理量；单位与有效范围 SDK 未注明。

### 9.2 `SwitchMoveMode` 只改本地状态

`SwitchMoveMode(bool)` 返回 0 但不发 RPC（`:242-245`）。它只影响后续 `Move(vx,vy,vyaw)` 选择 1 s 还是 864000 s。不要把它当作“切换机器人运动模式”。

### 9.3 `GetPhase` 已废弃

`g1_loco_api.hpp:22` 注释 `deprecated`，但客户端仍注册。新代码不要依赖。

### 9.4 返回 0 ≠ 动作完成

所有方法返回的是 RPC 状态码。动作是否执行、是否完成、是否被安全层打断，需要：
- 订阅 `rt/sportmodestate` 看 `fsm_id/fsm_mode`；
- 订阅 `rt/arm/action/state` 看动作状态；
- 观察 `rt/lowstate` 的实际关节运动。

### 9.5 没有取消/急停 API

`LocoClient` 没有“立即停止并保持”的专用方法。`StopMove()` 只发 1 秒零速度；`Damp()` 切 FSM；`ZeroTorque()` 会失去支撑。真正的急停应使用物理急停或经安全评审的策略。

---

## 10. 错误码

### 10.1 `sport`

| 码 | 含义 | 处理 |
|---|---|---|
| 7301 | LocoState 不可用 | 等待状态/检查服务 |
| 7302 | 无效 FSM ID | 检查允许的状态转换 |
| 7303 | 无效 task ID | 检查任务 ID |

【源码】`g1_loco_error.hpp:9-12`。

### 10.2 `arm`

| 码 | 含义 | 处理 |
|---|---|---|
| 7400 | `rt/armsdk` 被占用 | 停止低层手臂发布 |
| 7401 | 手臂正在 holding | 发 99 或相同动作 ID 释放 |
| 7402 | 无效动作 ID | 查 `GetActionList` |
| 7404 | 无效 FSM | 切到 500/501/801；801 下 mode 0/3 |

【源码】`g1_arm_action_error.hpp`。

### 10.3 `agv` / `audio`

| 码 | 含义 |
|---|---|
| 9101 | AGV 模块未初始化 |
| 9102 | Move 执行失败 |
| 9103 | HeightAdjust 执行失败 |
| 100 | Audio 参数无效（`g1_audio_error.hpp`） |

通用 RPC 错误（3102–3107、3201–3207）见 [02](02_dds_rpc_client_server.md)。

---

## 11. 只读示例

### 11.1 查询 FSM 与动作列表

【已验证：编译通过】未运行。只读，不触发运动。

```cpp
// g1_highlevel_readonly.cpp
#include <iostream>
#include <unitree/robot/channel/channel_factory.hpp>
#include <unitree/robot/g1/loco/g1_loco_client.hpp>
#include <unitree/robot/g1/arm/g1_arm_action_client.hpp>

int main(int argc, char** argv)
{
    const std::string iface = (argc > 1) ? argv[1] : "lo";
    unitree::robot::ChannelFactory::Instance()->Init(0, iface);

    unitree::robot::g1::LocoClient loco;
    loco.Init();
    loco.SetTimeout(3.f);

    int fsm_id = -1, fsm_mode = -1, balance = -1;
    if (loco.GetFsmId(fsm_id) != 0 ||
        loco.GetFsmMode(fsm_mode) != 0 ||
        loco.GetBalanceMode(balance) != 0) {
        std::cerr << "sport service not reachable\n";
        return 1;
    }
    std::cout << "fsm_id=" << fsm_id
              << " fsm_mode=" << fsm_mode
              << " balance_mode=" << balance << std::endl;

    unitree::robot::g1::G1ArmActionClient arm;
    arm.Init();
    arm.SetTimeout(3.f);

    std::string actions;
    int32_t ret = arm.GetActionList(actions);
    if (ret == 0) {
        std::cout << "actions: " << actions << std::endl;
    } else {
        std::cerr << "GetActionList failed: " << ret << std::endl;
    }

    unitree::robot::ChannelFactory::Instance()->Release();
    return 0;
}
```

### 11.2 只读查询 motion switcher 当前模式

【未验证】`CheckMode` 是否为纯读操作，需以服务端实现为准；示例把它用作查询。

```cpp
unitree::robot::b2::MotionSwitcherClient msc;
msc.Init();
msc.SetTimeout(3.f);
std::string form, name;
int32_t ret = msc.CheckMode(form, name);
if (ret == 0) {
    std::cout << "form=" << form << " name=" << name << std::endl;
}
```

`name` 非空意味着有高层模式占用；此时**不要**贸然发低层命令。

---

## 12. 生命周期、线程与异步语义

| 关注点 | 结论 | 证据 |
|---|---|---|
| 构造 | 只保存服务名，不建通道 | `LocoClient()` 构造函数 `:15` |
| `Init()` | 注册 API ID、建立 RPC 通道 | `:18-36` |
| 调用阻塞 | 是，直到响应/超时 | [02](02_dds_rpc_client_server.md) |
| 析构 | 默认析构，未显式关闭通道 | `~LocoClient() {}` `:16` |
| 线程安全 | 【未验证】不建议并发调用同一实例 | 无锁成员 |
| 动作完成通知 | 无回调；需轮询状态 | 见 9.4 |
| `Move` 是否阻塞 | 否，RPC 返回即命令已受理 | AGV 注释明确；loco 推断相同 |

**推论**：高层 API 适合“低频决策 + 状态轮询”，不适合 500 Hz 闭环。高频控制必须走低层 `rt/lowcmd`（见 [06](06_g1_low_level_control.md)）。

---

## 13. 故障排查

| 现象 | 可能原因 | 处理 |
|---|---|---|
| `Start()` 返回 0 但不站起 | 当前 FSM 不允许；急停/保护触发 | 读 `GetFsmId/GetFsmMode`，查官方状态机 |
| 手臂动作 7404 | FSM 不在 500/501/801 | 先 `Start()` 或切到允许状态 |
| 手臂动作 7401 | 正在 holding | 发 99 或上次动作 ID |
| 手臂动作 7400 | `rt/arm_sdk` 被占用 | 停止低层手臂发布者 |
| `Move` 后只动 1 秒 | 未开启连续模式 | 用 `Move(vx,vy,vyaw,true)` |
| `HighStand` 行为异常 | 数值类型 bug | 不用；改用 `SetStandHeight` |
| `PlayStop` 总是返回 0 | 实现丢弃返回值 | 不要依赖返回值 |
| 低层接管后机器人软倒 | 未先释放高层/未保持姿态 | 参考 `g1_ankle_swing`/`g1_userctrl` 的顺序 |
| 与 App 抢控制 | 多控制源 | 只保留一个控制源 |

---

## 14. G1-EDU 部署能力划分

| 能力 | 归属 | 建议 |
|---|---|---|
| 读 FSM/平衡模式/站高 | SDK 原生（P0） | 部署前必做 |
| 列出/执行预设手臂动作 | SDK 原生（P1/P2） | 先在吊装/保护下验证 |
| 速度控制 | SDK 原生（P2） | 仅在上层安全仲裁后使用 |
| 用户低层控制切换 | SDK 原生 + 开发者适配 | 顺序敏感，需状态确认 |
| 自定义全身控制器 | 开发者适配 | 走 `rt/lowcmd`，见 06 |
| 模型输出 → 高层速度 | 开发者适配 | 必须限幅、限时、可撤销 |
| 模型输出 → 低层关节 | 开发者适配 | 风险最高，需独立安全层 |
| 急停/安全认证 | 非 SDK | 物理急停 + 独立监控 |

---

## 15. 相关文档

- RPC 机制与超时：[02_dds_rpc_client_server.md](02_dds_rpc_client_server.md)
- 状态与安全谓词：[04_robot_state.md](04_robot_state.md)
- 低层关节控制：[06_g1_low_level_control.md](06_g1_low_level_control.md)
- 并发与生命周期：[08_concurrency_and_lifecycle.md](08_concurrency_and_lifecycle.md)
- 部署优先级：[g1_deployment_priority.md](g1_deployment_priority.md)
