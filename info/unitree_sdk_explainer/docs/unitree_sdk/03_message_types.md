# 03 · 消息类型与数据布局

> 目标：在不改 SDK、不连机器人的前提下，准确回答“G1 的 `rt/lowstate` 里到底有什么、哪些字段能信、字节怎么对齐、和 GO2 有什么不同”。
>
> 证据标记见 [README.md](README.md)。字段均来自 `include/unitree/idl/**` 的生成头文件；**未做 `sizeof` 实测、未抓包核对**。

---

## 1. 三套命名空间，先分清机型

Unitree SDK2 的 IDL 按机型/用途分成不同 C++ 命名空间。**名字相同不代表类型相同**。

| 命名空间 | 头文件目录 | 适用 | 说明 |
|---|---|---|---|
| `unitree_hg::msg::dds_` | `include/unitree/idl/hg/` | G1（以及 H1/H2 等人形） | 本项目的核心类型 |
| `unitree_go::msg::dds_` | `include/unitree/idl/go2/` | GO2 等四足 | `LowState_`/`SportModeState_` 字段与 G1 **完全不同** |
| `unitree_api::msg::dds_` | `include/unitree/robot/internal/internal_idl_decl/` | RPC | `Request_`/`Response_`，见 [02](02_dds_rpc_client_server.md) |
| `std_msgs` / `sensor_msgs` / `nav_msgs` / `geometry_msgs` / `builtin_interfaces` | `include/unitree/idl/ros2/` | ROS 2 消息兼容 | 仅部分消息；**没有** `Image`/`JointState`/`LaserScan` |

【源码】`unitree_hg` 命名空间见 `include/unitree/idl/hg/LowState_.hpp:19-24`；`unitree_go` 见 `include/unitree/idl/go2/LowState_.hpp`；ROS 2 命名空间直接使用 `sensor_msgs` 等（如 `include/unitree/idl/ros2/Imu_.hpp:20`）。

> 最常见的错误：把 `unitree_go::msg::dds_::LowState_` 订阅到 G1 的 `rt/lowstate`。DDS 类型不匹配时**通常静默收不到数据**，而不是编译错误。

---

## 2. G1 核心消息总表

| 消息 | 头文件 | 典型 Topic | 方向 | 关键特征 |
|---|---|---|---|---|
| `LowState_` | `hg/LowState_.hpp` | `rt/lowstate` | 机器人 → 上位机 | 35 个电机槽、IMU、40 字节手柄 |
| `LowCmd_` | `hg/LowCmd_.hpp` | `rt/lowcmd`、`rt/arm_sdk`、`rt/user_lowcmd` | 上位机 → 机器人 | 35 个电机命令槽、CRC |
| `MotorState_` | `hg/MotorState_.hpp` | 嵌入 `LowState_`/`HandState_` | — | 位置/速度/力矩/温度 |
| `MotorCmd_` | `hg/MotorCmd_.hpp` | 嵌入 `LowCmd_`/`HandCmd_` | — | 位置/速度/力矩/kp/kd |
| `IMUState_` | `hg/IMUState_.hpp` | 嵌入 `LowState_`、`rt/secondary_imu` | — | 四元数、角速度、加速度、RPY |
| `SportModeState_` | `hg/SportModeState_.hpp` | `rt/sportmodestate`【推断】 | 机器人 → 上位机 | **仅 4 个字段**；Topic 仅见注释，G1 示例未订阅 |
| `HandState_` | `hg/HandState_.hpp` | `rt/dex3/*/state` 等 | 机器人 → 上位机 | 变长电机数组 + 12 点压力 |
| `HandCmd_` | `hg/HandCmd_.hpp` | `rt/dex3/*/cmd` 等 | 上位机 → 机器人 | 变长电机命令 |
| `BmsState_` | `hg/BmsState_.hpp` | 未确认 G1 Topic | — | 电池电压/电流/SOC |
| `PressSensorState_` | `hg/PressSensorState_.hpp` | 嵌入 `HandState_` | — | 12 点压力/温度 |
| `MainBoardState_` | `hg/MainBoardState_.hpp` | 未确认 | — | 风扇/温度 |
| `AgvBmsState_` | `hg/AgvBmsState_.hpp` | AGV 相关 | — | 电池百分比/充电状态 |
| `doubleIMUState_` | `hg_doubleimu/doubleIMUState_.hpp` | 未确认 | — | 双 IMU |

### 2.1 `LowState_`（G1）

【源码】`include/unitree/idl/hg/LowState_.hpp:28-36`（字段块）：

| 字段 | 类型 | 含义 | 备注 |
|---|---|---|---|
| `version` | `array<uint32_t,2>` | 协议版本 | 具体编码未验证 |
| `mode_pr` | `uint8_t` | PR/AB 模式 | 示例把 PR 映射为 0、AB 为 1（`g1_ankle_swing_example.cpp:84-87`） |
| `mode_machine` | `uint8_t` | 机型/模式标识 | **0 表示仿真**；实机非 0，需与 `LowCmd_` 一致（`g1_pub.h:52-61`） |
| `tick` | `uint32_t` | 状态计数 | 可用于检测丢帧/更新 |
| `imu_state` | `IMUState_` | 躯干 IMU | 见 2.5 |
| `motor_state` | `array<MotorState_,35>` | 35 个电机槽 | **前 29 个对应 G1 关节，其余槽位含义不可假设** |
| `wireless_remote` | `array<uint8_t,40>` | 手柄原始数据 | 示例按 40 字节结构体 `memcpy`（`g1_sub.h:74-76`） |
| `reserve` | `array<uint32_t,4>` | 保留 | 不要依赖 |
| `crc` | `uint32_t` | 校验 | 上位机发送 `LowCmd_` 时需计算 |

关键结论：

1. **35 不是自由度**。G1 29 DOF 使用索引 0–28；`motor_state[29..34]` 不应被当作关节读取。
2. `rt/lowstate` **不包含 BMS 数据**。`terminations.hpp` 的 `low_battery()` 需要单独的 `BmsState_`，但 G1 上对应的 Topic **未在 SDK 中确认**。
3. `wireless_remote` 是原始字节，不是解析后的按键；解析逻辑在 `unitree_joystick.hpp`/示例 `gamepad.hpp`。
4. `mode_machine` 必须校验：仿真值为 0；实机不匹配时不要发低层命令。

### 2.2 `LowCmd_`（G1）

【源码】`hg/LowCmd_.hpp:23-30`：

| 字段 | 类型 | 含义 |
|---|---|---|
| `mode_pr` | `uint8_t` | PR/AB 模式 |
| `mode_machine` | `uint8_t` | 机型标识，需与状态一致 |
| `motor_cmd` | `array<MotorCmd_,35>` | 35 个命令槽 |
| `reserve` | `array<uint32_t,4>` | 保留 |
| `crc` | `uint32_t` | CRC32 |

`motor_cmd[29]` 在 `rt/arm_sdk` 中被复用为**手臂控制权重**，不是关节：

```cpp
// g1_pub.h:77-84
void weight(float coe) {
    msg_.motor_cmd()[29].q() = std::clamp(coe, 0.0f, 1.0f);
}
```

【源码】`g1_pub.h:77-84`。这解释了为什么手臂示例要先发一帧 `weight=0` 再逐渐升到 1：避免瞬时接管。

### 2.3 `MotorState_`（G1）

【源码】`hg/MotorState_.hpp:21-33`：

| 字段 | 类型 | 含义 | 工程注意 |
|---|---|---|---|
| `mode` | `uint8_t` | 电机模式/状态 | 取值含义未在头文件注释 |
| `q` | `float` | 关节位置（rad） | 29 关节顺序见第 4 节 |
| `dq` | `float` | 关节速度（rad/s） | 安全阈值示例 10 rad/s |
| `ddq` | `float` | 加速度（rad/s²） | 噪声较大，慎用 |
| `tau_est` | `float` | 估计力矩（N·m） | 不是实测力矩 |
| `temperature` | `array<int16_t,2>` | 温度 | `[0]` 外壳、`[1]` 绕组（依据 `terminations.hpp:50-65`） |
| `vol` | `float` | 电压 | 单位未注明，推断为 V |
| `sensor` | `array<uint32_t,2>` | 传感器原始值 | 编码未验证 |
| `motorstate` | `uint32_t` | 电机状态位 | 位定义未验证 |
| `reserve` | `array<uint32_t,4>` | 保留 | — |

### 2.4 `MotorCmd_`（G1）

【源码】`hg/MotorCmd_.hpp:20-29`：

| 字段 | 类型 | 含义 |
|---|---|---|
| `mode` | `uint8_t` | 控制模式；示例使用 `1`（`g1_ankle_swing_example.cpp:286`） |
| `q` | `float` | 目标位置（rad） |
| `dq` | `float` | 目标速度（rad/s） |
| `tau` | `float` | 前馈力矩（N·m） |
| `kp` | `float` | 位置增益 |
| `kd` | `float` | 速度增益 |
| `reserve` | `uint32_t` | 保留 |

【推断】底层执行的是 MIT 型阻抗控制 `tau_out = kp*(q_des-q) + kd*(dq_des-dq) + tau_ff`，因为字段组合与示例用法完全对应；**未读电机固件，不能作为合同**。

### 2.5 `IMUState_`（G1）

【源码】`hg/IMUState_.hpp:24-28`（字段块，class 声明在 :21）：

| 字段 | 类型 | 含义 |
|---|---|---|
| `quaternion` | `array<float,4>` | 姿态四元数 |
| `gyroscope` | `array<float,3>` | 角速度（rad/s） |
| `accelerometer` | `array<float,3>` | 加速度（m/s²） |
| `rpy` | `array<float,3>` | 欧拉角（rad） |
| `temperature` | `int16_t` | 温度 |

四元数分量顺序**头文件未注明**。`terminations.hpp:21-26` 用 `Eigen::Quaternionf(quat[0],quat[1],quat[2],quat[3])` 构造，而 Eigen 的 4 标量构造函数顺序为 `(w,x,y,z)`，故【推断】为 `w,x,y,z`。使用前建议用静止姿态验证：`w≈1, x≈y≈z≈0`。

### 2.6 `SportModeState_`（G1）

【源码】`hg/SportModeState_.hpp:20-26`，**只有 4 个字段**：

| 字段 | 类型 |
|---|---|
| `fsm_id` | `uint32_t` |
| `fsm_mode` | `uint32_t` |
| `task_id` | `uint32_t` |
| `task_time` | `float` |

这与 GO2 的 `SportModeState_` 形成鲜明对比：

| 能力 | G1 (`unitree_hg`) | GO2 (`unitree_go`) |
|---|---|---|
| FSM/task | 有 | 有 |
| IMU | **无**（在 `LowState_`） | 有 |
| 位置/速度/偏航角速度 | **无** | 有 |
| 足端力/足端位置 | **无** | 有 |
| 高度/步态/路径点 | **无** | 有 |

【源码】GO2 字段见 `include/unitree/idl/go2/SportModeState_.hpp:29-45`。

**工程后果**：G1 上不能靠 `rt/sportmodestate` 获取里程计。需要位姿时只能：
- 从 `rt/lowstate` 的 IMU 积分（会漂移，非 SDK 功能）；
- 外接定位系统/VIO；
- 使用机器人其他未在 SDK 中暴露的接口（未知）。

### 2.7 手部消息

`HandState_`（`hg/HandState_.hpp:28-41`）：

| 字段 | 类型 | 含义 |
|---|---|---|
| `motor_state` | `vector<MotorState_>` | 手部电机，长度随手型 |
| `press_sensor_state` | `vector<PressSensorState_>` | 压力传感器 |
| `imu_state` | `IMUState_` | 手部 IMU |
| `power_v/power_a` | `float` | 电压/电流 |
| `system_v/device_v` | `float` | 系统/设备电压 |
| `error` | `array<uint32_t,2>` | 错误码 |
| `reserve` | `array<uint32_t,2>` | 保留 |

`HandCmd_`（`hg/HandCmd_.hpp:24-29`）：`vector<MotorCmd_> motor_cmd` + `array<uint32_t,4> reserve`。**没有 CRC 字段**。

`PressSensorState_`（`hg/PressSensorState_.hpp`）：12 点 `pressure`/`temperature` + `lost`。

Dex3 示例把 `MOTOR_MAX` 定义为 7（`example/g1/dex3/g1_dex3_example.cpp:41`），但这是**示例常量**，不是 SDK 对所有手型的承诺。不同手型（Dex3、Inspire、因时等）数组长度不同，必须按实际设备 `resize` 并校验。

> Dex3 示例存在 Topic 不一致：初始使用 `rt/dex3/left/state`（`:33`），切换分支又使用 `rt/lf/dex3/left/state`（`:253`）。哪个是实机正确值**未验证**，部署前必须实测确认。

### 2.8 `BmsState_`（G1）

【源码】`hg/BmsState_.hpp:24-36`（字段块，class 声明在 :21）：`version_high/low`、`fn`、`cell_vol[40]`、`bmsvoltage[3]`、`current`、`soc`、`soh`、`temperature[12]`、`cycle`、`manufacturer_date`、`bmsstate[5]`、`reserve[3]`。

- `soc` 为 `uint8_t`，百分比。
- `terminations.hpp:68-71` 的 `low_battery()` 直接比较 `bms_state.soc()`。
- **G1 的 BMS Topic 未在 SDK 头文件/示例中确认**；不要假设它随 `rt/lowstate` 一起发布。

---

## 3. 与 GO2 同名消息的差异（必须避免混用）

| 项目 | G1 `unitree_hg` | GO2 `unitree_go` |
|---|---|---|
| `LowState_` 电机数 | 35 | 20 |
| `LowState_` 额外字段 | `mode_pr`、`mode_machine`、`tick`、`reserve[4]` | `head`、`sn`、`bandwidth`、`bms_state`、`foot_force`、`power_v/a` 等 |
| `MotorState_` 温度 | `int16_t[2]` | `uint8_t` 单值 |
| `MotorState_` 额外 | `vol`、`sensor[2]`、`motorstate` | `q_raw/dq_raw/ddq_raw`、`lost` |
| `MotorCmd_.reserve` | `uint32_t`（1 个） | `uint32_t[3]` |
| `SportModeState_` | 4 字段 | 20+ 字段 |
| `LowCmd_` | 35 槽、无 BMS 命令 | 20 槽、含 `bms_cmd`、LED、风扇、GPIO |

【源码】两侧头文件对照（均为字段块）：`hg/LowState_.hpp:28-36` vs `go2/LowState_.hpp:30-51`；`hg/MotorState_.hpp:24-33` vs `go2/MotorState_.hpp:24-34`。

**DDS 只按 Topic 名和类型匹配**。同样的 `rt/lowstate`，G1 与 GO2 的类型不同；跨机型复用代码时必须换头文件与数据类型。

---

## 4. G1 关节索引（29 DOF）

【源码】`include/unitree/dds_wrapper/robots/g1/defines.h:15-53`：

| 索引 | 枚举 | 部位 |
|---|---|---|
| 0 | `LeftHipPitch` | 左髋俯仰 |
| 1 | `LeftHipRoll` | 左髋横滚 |
| 2 | `LeftHipYaw` | 左髋偏航 |
| 3 | `LeftKnee` | 左膝 |
| 4 | `LeftAnklePitch` | 左踝俯仰 |
| 5 | `LeftAnkleRoll` | 左踝横滚 |
| 6 | `RightHipPitch` | 右髋俯仰 |
| 7 | `RightHipRoll` | 右髋横滚 |
| 8 | `RightHipYaw` | 右髋偏航 |
| 9 | `RightKnee` | 右膝 |
| 10 | `RightAnklePitch` | 右踝俯仰 |
| 11 | `RightAnkleRoll` | 右踝横滚 |
| 12 | `WaistYaw` | 腰偏航 |
| 13 | `WaistRoll` | 腰横滚 |
| 14 | `WaistPitch` | 腰俯仰 |
| 15 | `LeftShoulderPitch` | 左肩俯仰 |
| 16 | `LeftShoulderRoll` | 左肩横滚 |
| 17 | `LeftShoulderYaw` | 左肩偏航 |
| 18 | `LeftElbow` | 左肘 |
| 19 | `LeftWristRoll` | 左腕横滚 |
| 20 | `LeftWristPitch` | 左腕俯仰 |
| 21 | `LeftWristYaw` | 左腕偏航 |
| 22 | `RightShoulderPitch` | 右肩俯仰 |
| 23 | `RightShoulderRoll` | 右肩横滚 |
| 24 | `RightShoulderYaw` | 右肩偏航 |
| 25 | `RightElbow` | 右肘 |
| 26 | `RightWristRoll` | 右腕横滚 |
| 27 | `RightWristPitch` | 右腕俯仰 |
| 28 | `RightWristYaw` | 右腕偏航 |

`ArmJoints`（`defines.h:55-67`）是 17 个索引：14 个手臂关节 + 3 个腰部关节，顺序为左臂 15–21、右臂 22–28、腰 12–14。**不是按 0–16 排列**，做手臂向量映射时不要直接 `0..16`。

> 不同 G1 子型号（如 G1D）关节数量/含义可能不同。`g1d_arm_example.cpp:118-130` 的注释明确说明右腿/腰部索引在该型号上无效。必须按实际机型核对接线，不能仅凭枚举。

---

## 5. 序列化与字节布局

- 所有 IDL 由 CycloneDDS 0.10.2 的 IDL-to-CXX 生成（每个头文件顶部有注释，如 `hg/LowState_.hpp:1-7`）。
- 固定数组在 C++ 中是 `std::array<T,N>`；变长数组是 `std::vector<T>`。
- DDS 线上布局由 CycloneDDS 的 XTypes/CDR 实现决定，**不要手写 `memcpy` 构造消息**，除非像示例那样只填充 `wireless_remote` 这类字节数组。
- `LowCmd_` 的 CRC 计算方式是“整条消息按 32 位字数计算，排除最后一个 32 位 CRC 字段”：

```cpp
// g1_pub.h:66-68
msg_.crc() = crc32_core((uint32_t*)&msg_, (sizeof(MsgType)>>2)-1);
```

【源码】`g1_pub.h:36-41`（LowState）、`:66-68`（LowCmd）、`:81-83`（ArmSdk）。`crc32_core` 声明于 `include/unitree/dds_wrapper/common/crc.h`。该写法依赖消息内存连续且 4 字节对齐；**不要在消息中插入非 4 字节字段后照抄**。

- 接收方向是否需要校验 CRC：示例在订阅 `rt/lowstate` 后检查 `crc`（`g1_ankle_swing_example.cpp:215`）。SDK 的 `ChannelSubscriber` 本身**不做 CRC 校验**。

---

## 6. 只读解析示例

【已验证：编译通过】未运行。演示如何安全复制并解析 `LowState_`。

```cpp
// lowstate_fields.cpp — 只读示例
#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <mutex>
#include <thread>

#include <unitree/robot/channel/channel_factory.hpp>
#include <unitree/robot/channel/channel_subscriber.hpp>
#include <unitree/idl/hg/LowState_.hpp>

using LowState = unitree_hg::msg::dds_::LowState_;

int main(int argc, char** argv)
{
    const std::string iface = (argc > 1) ? argv[1] : "lo";
    unitree::robot::ChannelFactory::Instance()->Init(0, iface);

    std::mutex mtx;
    LowState shared;
    std::atomic<bool> got{false};

    auto sub = std::make_shared<
        unitree::robot::ChannelSubscriber<LowState>>("rt/lowstate");
    sub->InitChannel([&](const void* msg) {
        const auto* s = static_cast<const LowState*>(msg);
        std::lock_guard<std::mutex> lock(mtx);
        shared = *s;
        got = true;
    }, 1);   // 队列长度 1，只保留最新帧

    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::seconds(5);
    while (!got && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!got) {
        std::cerr << "no data on rt/lowstate" << std::endl;
        return 1;
    }

    LowState snap;
    {
        std::lock_guard<std::mutex> lock(mtx);
        snap = shared;
    }

    std::cout << "mode_machine=" << int(snap.mode_machine())
              << " tick=" << snap.tick() << std::endl;

    const auto& imu = snap.imu_state();
    std::cout << "quat(w,x,y,z)="
              << imu.quaternion()[0] << "," << imu.quaternion()[1] << ","
              << imu.quaternion()[2] << "," << imu.quaternion()[3] << "\n"
              << "rpy=" << imu.rpy()[0] << "," << imu.rpy()[1] << ","
              << imu.rpy()[2] << std::endl;

    // 只读前 29 个有效关节
    for (int i = 0; i < 29; ++i) {
        const auto& m = snap.motor_state()[i];
        if (!std::isfinite(m.q()) || !std::isfinite(m.dq())) {
            std::cerr << "joint " << i << " has non-finite state\n";
            continue;
        }
        std::cout << i << ": q=" << m.q() << " dq=" << m.dq()
                  << " tau=" << m.tau_est() << std::endl;
    }

    sub->CloseChannel();
    unitree::robot::ChannelFactory::Instance()->Release();
    return 0;
}
```

说明：

- `queuelen=1`：回调在独立线程执行，队列满时丢最旧帧（见 [01](01_channel_communication.md)）。这对状态显示足够，但**不能用于需要每一帧的控制算法**。
- 复制整条 `LowState_` 而非持有指针：回调返回后指针失效。
- `std::isfinite` 检查用于发现通信/驱动异常，不是 SDK 保证。

---

## 7. 字段可信度分级

| 字段/结论 | 级别 | 说明 |
|---|---|---|
| 数组长度 35、关节 0–28 | 源码已确认 | IDL + `defines.h` |
| `motor_cmd[29]` 为 `rt/arm_sdk` 权重 | 源码已确认 | `g1_pub.h:77-84` |
| 温度 `[0]` 外壳 / `[1]` 绕组 | 源码已确认（使用处） | `terminations.hpp:50-65` |
| 四元数 `w,x,y,z` | 合理推断 | Eigen 构造用法 |
| `q` 单位 rad | 合理推断 | 示例/安全阈值；IDL 无单位注释 |
| `tau_est` 为估计值 | 源码已确认（字段名） | 非实测 |
| G1 BMS Topic | 尚未验证 | 头文件有类型，无 Topic 映射 |
| `motor_state[29..34]` 含义 | 尚未验证 | 不同子型号可能不同 |
| `mode` 枚举取值 | 尚未验证 | 仅见示例用 1 |
| Dex3 Topic 名 | 尚未验证（示例初始值与 L/R 分支不一致） | `g1_dex3_example.cpp:33` vs `:252-257` |

---

## 8. 常见问题

| 现象 | 原因 | 处理 |
|---|---|---|
| 订阅 `rt/lowstate` 无数据 | 用了 `unitree_go` 类型 | 改用 `unitree_hg::msg::dds_::LowState_` |
| 数据乱码/字段错位 | 手写内存布局或跨版本结构体 | 只用生成头文件；不要自定义同名字节结构 |
| 手臂不动 | 只发了关节命令，没设 `weight` | `rt/arm_sdk` 需 `motor_cmd[29].q()` 从 0 缓升到 1 |
| 低层命令被拒/无响应 | `mode_machine` 不匹配 | 读取状态并比对；仿真为 0 |
| CRC 错误 | 未计算或字段改动后未重算 | 使用示例同款 `crc32_core`，保持 4 字节对齐 |
| 手部数组越界 | 假设固定 7 个电机 | 按 `motor_state().size()` 处理 |

---

## 9. 相关文档

- 通道与队列：[01_channel_communication.md](01_channel_communication.md)
- RPC 消息：[02_dds_rpc_client_server.md](02_dds_rpc_client_server.md)
- 状态订阅与安全判断：[04_robot_state.md](04_robot_state.md)
- 低层控制与 CRC：[06_g1_low_level_control.md](06_g1_low_level_control.md)
- ROS 2 类型映射：[09_ros2_integration.md](09_ros2_integration.md)
