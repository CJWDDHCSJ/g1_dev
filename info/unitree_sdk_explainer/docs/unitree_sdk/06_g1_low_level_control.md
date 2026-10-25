# 06 · G1 低层控制（关节命令 / arm_sdk / user_lowcmd）

> **高风险章节**：低层接口直接给关节下发位置/速度/力矩，绕过内置平衡策略。任何错误都可能导致机器人跌倒、关节过载或硬件损坏。本文**不提供可直接运行的运动程序**，只分析协议、前置条件与失败模式。
>
> 阅读前请确认：[README.md](README.md) 的安全边界仍然有效。

---

## 1. 四条低层通路

G1 的低层控制不是单一 Topic，而是四条互相独立的注入点：

| 通路 | Topic | 消息类型 | 接管范围 | 前置条件 | 风险 |
|---|---|---|---|---|---|
| 全身低层 | `rt/lowcmd` | `unitree_hg::LowCmd_` | 29 关节 | 释放高层运控 | 最高 |
| 用户控制 | `rt/user_lowcmd` | `unitree_hg::LowCmd_` | 29 关节 | FSM=PASSIVE + `SwitchToUserCtrl` | 最高 |
| 手臂 SDK | `rt/arm_sdk` | `unitree_hg::LowCmd_` | 17 手臂+腰关节 | 无/部分场景需 FSM 允许 | 高 |
| 手部 SDK | `rt/hand_sdk` | `unitree_go::MotorCmds_` | 4 个手部电机 | 无 | 中 |
| Dex3 手 | `rt/dex3/{left,right}/cmd` | `unitree_hg::HandCmd_` | 7 个手指电机 | 无 | 中 |
| G1D 腰 | `rt/waistpitchcmd` | `unitree_hg::MotorCmd_` | 单个腰部电机 | G1D 配置 | 中 |

【源码/示例】Topic 与类型来自：
- `g1_ankle_swing_example.cpp:18-20`（`rt/lowcmd`、`rt/secondary_imu`、`rt/lowstate`）
- `g1_userctrl_dds_example.cpp:13-14`（`rt/user_lowcmd`、`rt/lowstate`）
- `g1_arm7_sdk_dds_example.cpp:11-12`（`rt/arm_sdk`、`rt/lowstate`）
- `g1_hand_sdk_example.cpp:14,27`（`rt/hand_sdk`）
- `g1_dex3_example.cpp:32-33,252-257`（`rt/dex3/...`，存在 `rt/lf/...` 变体）
- `g1d_waist_example.cpp:13-14`（`rt/waistpitchcmd/state`）

> 这些 Topic 的**服务端实现全部在机器人内部**，不在 SDK 源码中。SDK 只负责把消息发到 DDS。任何“必须满足的条件”如果没有示例/注释支持，都标为未验证。

---

## 2. `rt/lowcmd`：全身接管

### 2.1 示例采用的安全顺序

【示例】`g1_ankle_swing_example.cpp`：

1. `ChannelFactory::Init(0, networkInterface)`（`:178`）。
2. 创建并 `Init()` `MotionSwitcherClient`，`CheckMode` 循环直到 `name` 为空，期间调用 `ReleaseMode()`（`:181-190`）。
3. 创建 `rt/lowcmd` 发布者、`rt/lowstate`/`rt/secondary_imu` 订阅者（`:194-200`）。
4. 创建两个 500 Hz 周期线程：`LowCommandWriter` 与 `Control`（`:202-203`）。
5. `LowStateHandler` 先校验 CRC（`:215`），再复制前 29 个关节状态到缓冲区。
6. `LowCommandWriter` 只在缓冲区有数据时发送；`mode_pr`、`mode_machine` 来自状态，`motor_cmd[i].mode=1`，填充 q/dq/kp/kd/tau，计算 CRC（`:284-302`）。

**关键点**：

- 先释放高层，再发低层。释放后机器人不再由内置策略保持，必须由你的控制器立即接管。
- `mode_machine_` 初始为 0，只有收到 `LowState` 后才会更新（`:240-244`）。如果过早发送，`mode_machine=0`（仿真值）可能导致命令被拒或行为异常。示例通过“状态缓冲区非空才发”间接避免了过早发送。
- `mode_pr` 选择 PR（串联踝）或 AB（并联踝），示例枚举为 `PR=0, AB=1`（`:84-87`）。
- 只填充 0–28，29–34 保持默认 0；CRC 覆盖整条 35 槽消息。

### 2.2 控制律

示例使用位置 + 速度阻尼：

```text
tau_cmd = kp * (q_des - q) + kd * (dq_des - dq) + tau_ff
```

【推断】由字段组合与示例用法推断；电机固件内部实现未公开。

### 2.3 CRC

```cpp
// g1_ankle_swing_example.cpp:127-147 定义；:300-301 使用
dds_low_command.crc() = Crc32Core(
    (uint32_t*)&dds_low_command,
    (sizeof(dds_low_command) >> 2) - 1);
```

- 多项式 `0x04c11db7`，初值 `0xFFFFFFFF`，按 32 位字、MSB-first 处理。
- 长度为“总字数 − 1”，即排除最后一个 `crc` 字段。
- 依赖消息在内存中连续且 4 字节对齐。生成的消息类型满足该条件；不要自己在中间插入非 4 字节字段。
- 接收 `rt/lowstate` 时示例也校验 CRC（`:215`）。SDK 的订阅通道**不替你校验**。

### 2.4 失败模式

| 失败 | 后果 |
|---|---|
| 未释放高层就发 `rt/lowcmd` | 命令可能被忽略，或与内置策略冲突（未验证） |
| `mode_machine` 不匹配 | 命令可能被拒；仿真值 0 与实机不同 |
| CRC 错误 | 整帧可能被丢弃 |
| 频率过低/丢帧 | 电机命令保持上一帧或进入保护，行为未验证 |
| 只发部分关节 | 未填槽位为默认 0，可能被解释为 0 目标 |
| 程序崩溃/断网 | 最后一帧命令可能持续生效；必须设计看门狗 |

**没有“自动停止”保证**。`rt/lowcmd` 是持续命令流，停止发布不等于机器人停止。

---

## 3. `rt/user_lowcmd`：官方用户控制通道

### 3.1 流程

【示例】`g1_userctrl_dds_example.cpp`：

1. 初始化 DDS，创建 `rt/user_lowcmd` 发布者与 `rt/lowstate` 订阅者（`:68-89`）。
2. `LocoClient::Init()`，读取 `GetFsmId`；**要求 `fsm_id == 1`（PASSIVE）**，否则退出（`:94-100`）。
3. 发送一帧所有关节 `kp=0, kd=1.5, q=0, dq=0, tau=0` 的消息（`:129-136`），用于“软接管”。
4. 调用 `client.SwitchToUserCtrl()`（`:138`）。
5. 读取当前关节位置，按 2 秒、50 Hz 的轨迹把关节平滑移动到初始位置（`:149-175`）。
6. 进入 5 秒、50 Hz 的手臂抬起/放下循环，每步限制关节增量 `max_joint_velocity * control_dt`（`:183-239`）。
7. 结束时调用 `client.SwitchToInternalCtrl(InternalFsmMode::LAST)`（`:243`）。

### 3.2 与 `rt/lowcmd` 的差异

| 项目 | `rt/lowcmd` | `rt/user_lowcmd` |
|---|---|---|
| 进入方式 | 释放 motion switcher | `SwitchToUserCtrl()` |
| FSM 要求 | 示例未显式检查 | 示例要求 `fsm_id==1` |
| 初始软接管 | 自行设计 | 示例先发零 kp 帧 |
| 退出 | 自行设计 | `SwitchToInternalCtrl(LAST)` |
| CRC | 示例计算 | 示例**未计算** |
| `mode` 字段 | 示例设 1 | 示例**未设置** |
| 控制频率 | 示例 500 Hz | 示例 50 Hz |

【源码】`g1_userctrl_dds_example.cpp` 未调用 `Crc32Core`，也未设置 `motor_cmd[i].mode()`。这说明 `rt/user_lowcmd` 的校验要求与 `rt/lowcmd` **可能不同**；**未验证**服务端是否强制 CRC/mode。不要据此推断 `rt/lowcmd` 可以省略 CRC。

### 3.3 `SwitchToInternalCtrl` 的三种模式

```cpp
enum class InternalFsmMode { LAST, PASSIVE, WALKRUN };
```

映射为 `0/1/2`（`g1_loco_client.hpp:194-213`）。`LAST` 表示回到切换前的内部状态；具体恢复行为未验证。

---

## 4. `rt/arm_sdk`：只接管手臂

### 4.1 原理

- 消息类型仍是 `LowCmd_`。
- **权重放在 `motor_cmd[29].q()`**，范围 `[0,1]`（`g1_pub.h:77-85`）。
- 最终关节命令由用户手臂命令与内置命令按权重混合：

```text
q_cmd = weight * q_user + (1 - weight) * q_internal
```

【推断】来自 `g1_pub.h` 的 `weight()` 设计与示例注释；混合公式未在 SDK 中明文给出。

### 4.2 正确接管顺序

【示例】`g1_arm7_sdk_dds_example.cpp` 的意图：

1. 读取当前 17 个手臂/腰关节位置。
2. 发第一帧：`motor_cmd[29].q(weight)`，`weight` 从 0 开始。
3. 用 2 秒把手臂从当前位置插值到初始位置。
4. 之后每 20 ms 更新目标，限制每步增量。

**但示例实现有缺陷**：`weight = 1.0;` 在循环内被**直接赋值为 1**（`g1_arm7_sdk_dds_example.cpp:149`），虽然声明了 `weight_rate`，却没有使用。也就是说示例**瞬间完成接管**，而不是平滑升权。不要照抄这一行。

正确做法（【推断】，需实测）：

```text
weight = 0
第一帧发送当前关节位置 + weight=0
每周期 weight += rate * dt，直到 1
同时把 q_des 从当前值平滑过渡到目标值
```

### 4.3 关节集合

示例使用 17 个索引：左臂 15–21、右臂 22–28、腰 12–14（`g1_arm7_sdk_dds_example.cpp:92-103`）。与 `defines.h` 的 `ArmJoints` 一致。

**型号差异**：

- `g1_ankle_swing_example.cpp:102-116` 注释：`WaistRoll(13)`、`WaistPitch(14)` 在“23/29 DOF with waist locked”上无效；`LeftWristPitch/Yaw(20/21)`、`RightWristPitch/Yaw(27/28)` 在 23 DOF 上无效。
- `g1_arm5_sdk_dds_example.cpp` 是 13 关节变体（无腰/无腕）。
- `g1d_arm_example.cpp:118-130` 注释说明 G1D 上右腿/腰部索引无效。

**必须先确认机型与关节表，再决定发送哪些索引。**

### 4.4 示例省略的字段

`g1_arm7_sdk_dds_example.cpp` 的 `msg` **没有设置** `mode_pr`、`mode_machine`、`crc`。而 `g1_pub.h` 的 `ArmSdk` 包装器在 `pre_communication()` 中计算 CRC。两种用法并存，说明 `rt/arm_sdk` 的校验要求可能与 `rt/lowcmd` 不同；**未验证**。稳妥做法是：按 `g1_pub.h` 的方式设置 `mode_machine` 并计算 CRC。

### 4.5 与 `arm` 服务的关系

`G1ArmActionClient` 的注释明确：手臂动作服务基于 `rt/arm_sdk`（`g1_arm_action_client.hpp:13-16`）。因此：

- 你自己的 `rt/arm_sdk` 发布者会与内置手臂动作服务**争用同一通道**。
- 手臂动作返回 `7400` 表示 `rt/armsdk` 被占用。
- 不要同时运行手臂动作与自定义 `arm_sdk` 控制。

---

## 5. `rt/hand_sdk` 与 Dex3

### 5.1 `rt/hand_sdk`

【示例】`g1_hand_sdk_example.cpp`：

- 类型：`unitree_go::msg::dds_::MotorCmds_`（**不是** `unitree_hg`）。
- 4 个电机（`kMotorNum = 4`，`:25`）。
- 权重编码：`cmds[0].mode = uint8(weight * 100)`，范围 0–100（`:34-41`）。
- 示例直接 `set_weight(1.0f)`，然后以 1 Hz 在 `tau=+0.3/-0.3` 之间切换（`:65-74`）。
- `mode` 字段被复用为权重，不是电机模式；`set_tau` 只写 `tau`，未写 `q/dq/kp/kd`。

【示例】注释给出的混合公式：`Motor_real = weight * Hand_SDK + (1 - weight) * G1_Cmd`（文件头 `:3`）。

风险：

- 示例**瞬间 weight=1**，没有渐变。
- 4 个手指电机持续以 ±0.3 N·m 往复，可能夹伤或堵转；只在空载/安全状态下验证。
- 手部型号不同，电机数/方向可能不同。

### 5.2 Dex3

【示例】`g1_dex3_example.cpp`：

- `MOTOR_MAX = 7`（`:41`）。
- 发布 `rt/dex3/{left,right}/cmd`（命名空间 `:32`，拼接 `/cmd` 在 `:269`），订阅 `rt/dex3/{left,right}/state`（`:33`）。
- 但切换到“lf”分支时，订阅变为 `rt/lf/dex3/{left,right}/state`（`:252-257`），发布命名空间却仍是 `rt/dex3/...`。
- `RIS_Mode` 是位域（`:45-49`），不是普通枚举。
- 示例会 `resize(MOTOR_MAX)`（`:274-276`），说明消息长度由调用者决定。

**Topic 不一致是明确风险**：部署前必须实测哪个 Topic 有数据、哪个能被手接受。不要同时向两个 Topic 发布。

---

## 6. G1D 专用通道

| Topic | 类型 | 示例参数 | 说明 |
|---|---|---|---|
| `rt/waistpitchcmd` | `hg::MotorCmd_` | `mode=1, kp=500, kd=12` | 单关节腰部俯仰 |
| `rt/waistpitchstate` | `hg::MotorState_` | — | 反馈 |
| `rt/hispeed_state` | `geometry_msgs::Point32_` | `y()` = 高度 | 升降高度反馈 |
| `rt/audio_msg` | 原始音频流 | — | 音频，不是关节 |

【示例】`g1d_waist_example.cpp:45-65` 直接构造 `MotorCmd_` 并发布，`kp=500`、`kd=12`，在 0 与 0.8 rad 之间往复。这是**位置伺服**，不是 MIT 全身控制。

`g1d_height_control.cpp` 订阅 `rt/hispeed_state`，用 `Point32_.y()` 作为高度（`:35-40`），并通过 `AgvClient` 控制升降。这是 G1D/AGV 配置，不适用于普通 G1。

---

## 7. 线程、频率与看门狗

### 7.1 示例的线程模型

| 示例 | 线程 | 周期 | 说明 |
|---|---|---|---|
| `g1_ankle_swing` | `command_writer` | 2000 µs = 500 Hz | 发 `rt/lowcmd` |
| 同上 | `control` | 2000 µs = 500 Hz | 计算目标 |
| `g1_userctrl` | 主线程 | 20 ms = 50 Hz | 发 `rt/user_lowcmd` |
| `g1_arm7` | 主线程 | 20 ms = 50 Hz | 发 `rt/arm_sdk` |
| `g1_hand_sdk` | 主线程 | 1 s | 发 `rt/hand_sdk` |

【源码】`CreateRecurrentThreadEx("command_writer", UT_CPU_ID_NONE, 2000, ...)`（`g1_ankle_swing_example.cpp:202-203`）；该函数的周期单位是**微秒**（`CreateRecurrentThreadEx` 声明在 `include/unitree/common/thread/recurrent_thread.hpp:74-75`，形参名为 `intervalMicrosec`）。

### 7.2 控制频率不是越高越好

- 频率必须稳定；抖动会直接反映到力矩。
- DDS 发布本身有匹配等待/队列行为（见 [01](01_channel_communication.md)）。
- 示例用双缓冲（`DataBuffer` + `shared_mutex`）解耦“状态回调”和“控制计算”，避免在回调里做控制。
- `LowCommandWriter` 每周期新建 `LowCmd_`，填充后 `Write`，不保留跨周期状态；这避免了消息被回调线程同时修改。

### 7.3 必须自己实现的保护（SDK 不提供）

| 保护 | 建议 |
|---|---|
| 状态超时 | 超过 N ms 没收到 `rt/lowstate` → 停止发布/切 passive |
| 命令超时 | 上层决策超过 N ms 未更新 → 保持最后安全姿态或降权 |
| 关节限位 | 对 `q_des` 做机型相关的硬限幅 |
| 速率限制 | 对 `q_des` 每周期增量限幅（示例用 `max_joint_velocity * dt`） |
| 力矩限幅 | 对 `tau_ff` 限幅 |
| 姿态保护 | `bad_orientation` 等谓词（见 [04](04_robot_state.md)） |
| 通信中断 | 断网/进程崩溃时，电机端的超时策略未知，必须外部验证 |
| 控制权 | 只允许一个发布者；切换要有确认 |

**不要依赖“停止发布就会停”**。电机/服务端的失联保护行为未在 SDK 中说明。

---

## 8. 安全的“只构造、不发布”示例

以下代码只演示 `LowCmd_` 的字段布局与 CRC 计算，**不创建发布者、不发送任何消息**。【已验证：编译通过】未运行。

```cpp
// lowcmd_layout_check.cpp — 不发布，仅本地构造与校验
#include <cstdint>
#include <iostream>
#include <unitree/idl/hg/LowCmd_.hpp>

using LowCmd = unitree_hg::msg::dds_::LowCmd_;

static uint32_t Crc32Core(const uint32_t* ptr, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    const uint32_t poly = 0x04c11db7;
    for (uint32_t i = 0; i < len; ++i) {
        uint32_t data = ptr[i];
        uint32_t xbit = 1u << 31;
        for (int b = 0; b < 32; ++b) {
            if (crc & 0x80000000) { crc <<= 1; crc ^= poly; }
            else                  { crc <<= 1; }
            if (data & xbit)      { crc ^= poly; }
            xbit >>= 1;
        }
    }
    return crc;
}

int main()
{
    LowCmd cmd;   // 默认全零
    cmd.mode_pr() = 0;
    cmd.mode_machine() = 0;   // 仅演示；实机必须填实际值

    // 只填 0..28，且全部为“零力矩/零增益”布局
    for (int i = 0; i < 29; ++i) {
        auto& m = cmd.motor_cmd()[i];
        m.mode() = 0;   // 0 = Disable（示例注释）
        m.q() = 0.f;
        m.dq() = 0.f;
        m.kp() = 0.f;
        m.kd() = 0.f;
        m.tau() = 0.f;
    }

    cmd.crc() = Crc32Core(
        reinterpret_cast<uint32_t*>(&cmd),
        (sizeof(cmd) >> 2) - 1);

    std::cout << "sizeof(LowCmd)=" << sizeof(cmd)
              << " slots=" << cmd.motor_cmd().size()
              << " crc=0x" << std::hex << cmd.crc() << std::endl;
    return 0;
}
```

用途：

- 检查本机编译环境的类型大小与字段布局。
- 验证 CRC 实现与示例一致。
- **不要**把这段加上 `ChannelPublisher` 后直接跑在实机上；`mode_machine`、`mode_pr`、关节目标都必须来自真实状态与安全策略。

---

## 9. 常见问题

| 现象 | 可能原因 | 处理 |
|---|---|---|
| 发 `rt/lowcmd` 机器人无反应 | 高层未释放、`mode_machine` 错、CRC 错 | 按 2.1 顺序；先只读验证状态 |
| 一接管就软倒 | 释放高层后没有立即给保持力矩 | 先读当前位置，发同位置的保持命令，再渐变 |
| 手臂突然跳到目标 | `rt/arm_sdk` weight 瞬间为 1 | 从 0 缓升；不要照抄示例的 `weight=1.0` |
| 手臂动作返回 7400 | `rt/arm_sdk` 被自己的发布者占用 | 停止自定义发布或改用动作服务 |
| 手部不动 | `rt/hand_sdk` 用了 `hg` 类型 | 改用 `unitree_go::MotorCmds_` |
| 手部方向反了 | 电机方向/手型不同 | 小力矩、空载验证 |
| Dex3 无反馈 | 订阅了错误的 Topic 变体 | 实测 `rt/dex3` vs `rt/lf/dex3` |
| 腰部关节报错 | 机型不支持/腰部锁定 | 查 `defines.h` 与机型注释 |
| 高频下 CPU 占用高 | 每周期复制大消息/锁竞争 | 双缓冲、避免在回调中分配 |
| 退出后机器人仍动 | 最后一帧命令持续/服务端保持 | 设计退出序列：降权→保持→切回内部→释放 |

---

## 10. G1-EDU 部署能力划分

| 能力 | 归属 | 建议 |
|---|---|---|
| 读关节状态 | SDK 原生 | `rt/lowstate`，只读 |
| 手臂自定义控制 | SDK 原生 + 开发者适配 | `rt/arm_sdk`，权重渐变 |
| 手部控制 | SDK 原生 + 开发者适配 | `rt/hand_sdk`/Dex3，小力矩验证 |
| 全身自定义控制 | 开发者适配 | `rt/lowcmd` 或 `rt/user_lowcmd`，需独立安全层 |
| 内置平衡下的行走 | SDK 原生 | 用高层 `LocoClient`，不要用低层重写 |
| 模型输出 → 关节 | 开发者适配 | 必须限幅、限速、看门狗、可撤销 |
| 失联保护 | 未知/开发者适配 | 服务端行为未验证，不能假设 |

**推荐路线**：先用高层 API 完成“感知→决策→速度指令”的闭环；只有在高层无法满足控制频率/自由度需求时，才引入低层，并且把低层控制器当作独立的安全关键模块评审。

---

## 11. 相关文档

- 消息字段与关节索引：[03_message_types.md](03_message_types.md)
- 状态与安全谓词：[04_robot_state.md](04_robot_state.md)
- 高层控制与 FSM：[05_g1_high_level_control.md](05_g1_high_level_control.md)
- 并发与生命周期：[08_concurrency_and_lifecycle.md](08_concurrency_and_lifecycle.md)
- 已知限制汇总：[known_limitations.md](known_limitations.md)
