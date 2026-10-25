# 04 · 机器人状态订阅与安全前置判断

> 目标：让上层推理/控制程序在“什么时候可以动作”这个问题上有可验证的依据，而不是依赖固定延时。
>
> 本文只做**只读**分析。证据标记见 [README.md](README.md)。

---

## 1. 能读到哪些状态

| 状态源 | Topic | 类型 | 方向 | 已确认程度 |
|---|---|---|---|---|
| 全身状态 | `rt/lowstate` | `unitree_hg::msg::dds_::LowState_` | 机器人 → 上位机 | 源码 + 示例 |
| 躯干副 IMU | `rt/secondary_imu` | `unitree_hg::msg::dds_::IMUState_` | 机器人 → 上位机 | 示例 |
| 高层 FSM 状态 | `rt/sportmodestate` | `unitree_hg::msg::dds_::SportModeState_` | 机器人 → 上位机 | 头文件 + 注释；**无 G1 订阅示例** |
| 手臂动作状态 | `rt/arm/action/state` | JSON 字符串（推断 `std_msgs::String_`） | 机器人 → 上位机 | 仅注释 |
| 手柄（原始） | `rt/lowstate.wireless_remote[40]` | 40 字节 | 机器人 → 上位机 | 源码 + 示例 |
| 手柄（解析后） | `rt/wirelesscontroller` | `unitree_go::msg::dds_::WirelessController_` | 机器人 → 上位机 | 通用示例；G1 是否发布未验证 |
| Dex3 左手 | `rt/dex3/left/state` 或 `rt/lf/dex3/left/state` | `unitree_hg::msg::dds_::HandState_` | 机器人 → 上位机 | 示例初始值与分支覆盖不一致 |
| Dex3 右手 | `rt/dex3/right/state` 或 `rt/lf/dex3/right/state` | `HandState_` | 机器人 → 上位机 | 同上 |
| Inspire 手 | `rt/inspire/state` | `unitree_go::msg::dds_::MotorStates_` | 机器人 → 上位机 | `g1_sub.h:84-90` |
| G1D 高度 | `rt/hispeed_state` | `geometry_msgs::msg::dds_::Point32_` | 机器人 → 上位机 | `g1d_height_control.cpp:18,173-175` |
| G1D 腰部俯仰 | `rt/waistpitchstate` | `unitree_hg::msg::dds_::MotorState_` | 机器人 → 上位机 | `g1d_waist_example.cpp:13,38-39` |
| BMS | **未确认** | `unitree_hg::msg::dds_::BmsState_` | — | 类型存在，无 Topic 映射 |

【源码】Topic 字符串与类型对照见 `include/unitree/dds_wrapper/robots/g1/g1_sub.h:32-105`、`example/g1/low_level/g1_ankle_swing_example.cpp:18-20`、`example/g1/g1d/g1d_height_control.cpp:18`。

> 不要仅凭“SDK 里有 `BmsState_`”就假设 G1 会发布电池 Topic。`terminations.hpp` 的 `low_battery()` 需要调用者自己提供 `BmsState_`，而 SDK 未给出 G1 的 BMS Topic。

---

## 2. `rt/lowstate`：主状态源

### 2.1 内容

见 [03_message_types.md](03_message_types.md) 第 2.1 节。对状态判断最有用的字段：

| 字段 | 用途 | 注意 |
|---|---|---|
| `tick` | 判断是否收到新帧 | 回绕行为未验证 |
| `mode_machine` | 实机/仿真、机型匹配 | 0 = 仿真 |
| `imu_state.quaternion` | 姿态 | 分量顺序需实测 |
| `imu_state.gyroscope` | 角速度 | rad/s |
| `imu_state.accelerometer` | 重力方向/冲击 | m/s² |
| `motor_state[0..28].q/dq/tau_est` | 关节状态 | 29 DOF |
| `motor_state[*].temperature[0/1]` | 温度 | 外壳/绕组 |
| `wireless_remote` | 手柄 | 原始 40 字节 |

### 2.2 新鲜度

`ChannelSubscriber::GetLastDataAvailableTime()` 返回最后一次收到数据的单调时间（纳秒）；若通道未建立返回 `-1`（`channel_subscriber.hpp:47-55`）。

**重要**：在收到第一帧之前，该时间可能是 0 或初始值，因此基于它的超时判断会立即为真。**不要把“超时”当作“曾经连接过且现在断了”**，要结合“是否收到过第一帧”。

### 2.3 频率

SDK 头文件没有承诺 `rt/lowstate` 的发布频率。示例的控制循环是 500 Hz（`CreateRecurrentThreadEx(..., 2000, ...)`，`g1_ankle_swing_example.cpp:202-203`），但这只是示例周期，不是状态频率。

**结论：频率、抖动、丢帧率必须现场测量**，不能从文档推断。

---

## 3. `rt/sportmodestate`：只有 4 个字段

```text
fsm_id, fsm_mode, task_id, task_time
```

【源码】`hg/SportModeState_.hpp:20-26`。它适合做**模式门控**，不适合做导航/定位：

- `fsm_id`：当前高层状态机编号。
- `fsm_mode`：状态机子模式。
- `task_id` / `task_time`：当前任务与已运行时间。

已知的 FSM 语义（来自 G1 头文件与示例，非完整枚举）：

| `fsm_id` | 含义 | 证据 |
|---|---|---|
| 0 | 零力矩 | `g1_loco_client.hpp:224` |
| 1 | Damp | `g1_loco_client.hpp:214` |
| 2 | Squat | `g1_loco_client.hpp:218` |
| 3 | Sit | `g1_loco_client.hpp:220` |
| 4 | StandUp | `g1_loco_client.hpp:222` |
| 500 | Start（主运控） | `g1_loco_client.hpp:216` |
| 501 | 手臂动作允许的 FSM 之一 | `g1_arm_action_error.hpp` 注释 |
| 801 | 手臂动作允许的 FSM 之一；`fsm_mode ∈ {0,3}` | 同上 |

**`fsm_id` 只说明高层状态机处于哪个状态，不说明机器人是否稳定、是否有控制权、是否即将执行动作。**

读取方式有两种：

1. RPC：`LocoClient::GetFsmId()` / `GetFsmMode()`（见 [02](02_dds_rpc_client_server.md)、[05](05_g1_high_level_control.md)）。
2. Topic：订阅 `rt/sportmodestate`（更低延迟、无 RPC 超时；但 G1 的订阅示例缺失，需自行验证消息是否真的发布）。

【未验证】G1 实机是否持续发布 `rt/sportmodestate`。头文件与注释支持它存在，但没有 G1 示例订阅它。部署前必须先用只读程序确认。

---

## 4. `rt/secondary_imu`

【示例】`g1_ankle_swing_example.cpp:199-207` 订阅 `rt/secondary_imu`，类型 `unitree_hg::msg::dds_::IMUState_`，在回调中整条复制。

用途：

- 躯干 IMU 与 `rt/lowstate.imu_state` 可能来源不同/安装位置不同；不要假设二者一致。
- 适合做姿态冗余校验。

【未验证】G1-EDU 是否一定发布该 Topic；不同子型号可能没有副 IMU。

---

## 5. 手臂动作状态 `rt/arm/action/state`

【注释】`g1_arm_action_client.hpp:17-22` 给出 JSON 结构：

```json
{
  "holding": false,
  "id": 99,
  "name": "release_arm"
}
```

- `holding=true`：动作结束后保持最后关键帧，最长 20 秒（注释）。
- `id=99`/`name=release_arm`：释放手臂。
- 自定义示教动作 `id` 恒为 100，由 `name` 区分（`g1_arm_action_client.hpp:60-66`）。

**没有任何 G1 示例订阅该 Topic**。消息的具体 DDS 类型（很可能是 `std_msgs::msg::dds_::String_`）**未验证**。如果要用，先以只读方式尝试订阅并打印原始类型/内容。

---

## 6. 安全判断函数（`terminations.hpp`）

【源码】`include/unitree/robot/g1/common/terminations.hpp` 提供 6 个运行时保护谓词。文件头注释明确：**返回 true 时，建议在低层控制中把电机设为 passive**。

| 函数 | 行号 | 默认阈值 | 判断内容 | 注意事项 |
|---|---|---|---|---|
| `bad_orientation(lowstate, limit_angle=1.0)` | `:18-29` | 1.0 rad ≈ 57° | 用四元数把重力投影到机体系，与竖直方向夹角 | 未归一化四元数可能导致 `acos` 输入越界；`fabs(NaN)>limit` 为 false，**可能漏报** |
| `joint_vel_out_of_limit(lowstate, limit_vel=10.0)` | `:32-38` | 10 rad/s | 任一 `motor.dq()` 超限 | 遍历全部 35 槽，未使用槽位若有噪声可能误报 |
| `ang_vel_out_of_limit(lowstate, limit_vel=6.0)` | `:41-47` | 6 rad/s | 任一陀螺轴超限 | 只看角速度，不看持续时间 |
| `motor_winding_overheat(lowstate, limit_temp=120.0)` | `:50-56` | 120 °C | `temperature()[1]` | 绕组温度 |
| `motor_casing_overheat(lowstate, limit_temp=85.0)` | `:59-65` | 85 °C | `temperature()[0]` | 外壳温度 |
| `low_battery(bms_state, limit_soc=20.0)` | `:68-71` | 20 % | `soc()` | **需要 BmsState_；G1 Topic 未确认** |
| `lost_connection(subscriber, timeout_ms=1000)` | `:81-86` | 1000 ms | 距最后数据的时间 | 第一帧到达前就会返回 true；订阅者必须非空 |

### 6.1 这些函数不是什么

- 不是“急停”：它们只返回布尔值，不改变机器人状态。
- 不是完整安全认证：阈值是 SDK 给出的示例值，不是 G1-EDU 的安全标准。
- 不覆盖：控制权冲突、命令超时、网络分区后的恢复、模型输出异常、关节限位。

### 6.2 推荐组合

【推断】一个只读监控循环可以这样组织：

```text
每帧 lowstate:
  if lost_connection(sub, 1000)        -> 标记“状态不可信”，禁止新动作
  if bad_orientation(ls, 0.6)          -> 标记“姿态异常”
  if joint_vel_out_of_limit(ls, 8.0)   -> 标记“速度异常”
  if motor_winding_overheat(ls, 100)   -> 标记“过热”
  if ang_vel_out_of_limit(ls, 4.0)     -> 标记“角速度异常”
```

阈值应比默认值更保守还是更宽松，取决于机型、负载与场地；**必须由现场安全负责人确认**，本文不给出“安全阈值”结论。

---

## 7. 只读状态监控示例

【环境不满足，无法验证】本机缺 Eigen3 头文件，编译在 `terminations.hpp` 处停止；未运行。展示“首帧检测 + 新鲜度 + 安全谓词”的组合方式；不发送任何命令。

```cpp
// state_monitor.cpp — 只读
#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>

#include <unitree/robot/channel/channel_factory.hpp>
#include <unitree/robot/channel/channel_subscriber.hpp>
#include <unitree/robot/g1/common/terminations.hpp>
#include <unitree/idl/hg/LowState_.hpp>

using LowState = unitree_hg::msg::dds_::LowState_;

int main(int argc, char** argv)
{
    const std::string iface = (argc > 1) ? argv[1] : "lo";
    unitree::robot::ChannelFactory::Instance()->Init(0, iface);

    auto sub = std::make_shared<
        unitree::robot::ChannelSubscriber<LowState>>("rt/lowstate");

    std::mutex mtx;
    LowState shared;
    std::atomic<bool> first{false};
    std::atomic<uint64_t> frames{0};

    sub->InitChannel([&](const void* msg) {
        std::lock_guard<std::mutex> lock(mtx);
        shared = *static_cast<const LowState*>(msg);
        first = true;
        frames.fetch_add(1, std::memory_order_relaxed);
    }, 1);

    const auto t0 = std::chrono::steady_clock::now();
    while (!first && std::chrono::steady_clock::now() - t0 < std::chrono::seconds(5)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!first) {
        std::cerr << "rt/lowstate: no first frame within 5 s\n";
        return 1;
    }

    for (int i = 0; i < 100; ++i) {
        LowState snap;
        {
            std::lock_guard<std::mutex> lock(mtx);
            snap = shared;
        }

        const bool lost = unitree::robot::g1::lost_connection(sub, 1000);
        const bool bad_ori = unitree::robot::g1::bad_orientation(snap, 1.0f);
        const bool fast = unitree::robot::g1::joint_vel_out_of_limit(snap, 10.0f);
        const bool hot = unitree::robot::g1::motor_winding_overheat(snap, 120.0f);

        std::cout << "tick=" << snap.tick()
                  << " lost=" << lost
                  << " bad_ori=" << bad_ori
                  << " joint_fast=" << fast
                  << " hot=" << hot
                  << " frames=" << frames.load() << std::endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    sub->CloseChannel();
    unitree::robot::ChannelFactory::Instance()->Release();
    return 0;
}
```

注意：

- `lost_connection` 接受的是 `ChannelSubscriberPtr<LowState_>&`，所以 `sub` 必须保持存活。
- 该示例只打印，不执行任何保护动作；真正接入控制时，保护动作的设计必须经过安全评审。
- 多线程读取 `shared` 用互斥锁；`first/frames` 用原子量。

---

## 8. 状态如何用于模型部署（门控，不控制）

【推断】推荐的依赖方向：

```text
状态监控线程（只读，10–50 Hz）
        │ 产生 “ready / not-ready + 原因”
        ▼
推理线程（相机 + 模型）
        │ 只在 ready 时接受新帧/输出动作
        ▼
动作仲裁层（是否允许下发、是否被更高优先级逻辑占用）
        ▼
控制线程（低层或高层 API）
```

状态门控应至少包含：

| 条件 | 数据来源 | 失败时 |
|---|---|---|
| 收到首帧且持续更新 | `rt/lowstate` + `lost_connection` | 禁止新动作 |
| `mode_machine` 与目标环境一致 | `rt/lowstate` | 禁止低层命令 |
| FSM 在允许集合 | `rt/sportmodestate` 或 `GetFsmId` | 禁止运动 |
| 姿态/速度/温度未越界 | `terminations.hpp` | 停止/退出，由上层策略决定 |
| 控制权在自己手里 | 见 [05](05_g1_high_level_control.md) | 不得假设 |

**不要把“状态读取成功”等同于“可以安全运动”。** 状态只提供事实，授权与策略是另一层。

---

## 9. 常见问题

| 现象 | 可能原因 | 排查 |
|---|---|---|
| 首帧一直不来 | 网卡/domain 错；类型用了 GO2 | 检查 `Init` 参数与头文件命名空间 |
| `lost_connection` 一启动就是 true | 还没收到第一帧 | 先等首帧，再启用超时判断 |
| `bad_orientation` 一直 true | 四元数顺序/归一化问题；机器人确实倾斜 | 打印四元数与 RPY 对照 |
| `joint_vel_out_of_limit` 偶发 true | 未使用槽位噪声/瞬时冲击 | 只检查 0–28，或加持续时间窗口 |
| 温度判断不触发 | 用错索引（`[0]`/`[1]`） | 外壳 85、绕组 120 |
| 订阅 `rt/sportmodestate` 无数据 | G1 可能不发布/名称不同 | 先用只读程序枚举；不要假设 |
| 手部状态长度变化 | 手型/固件不同 | 按 `vector.size()` 处理 |
| 电池状态读不到 | 无 G1 BMS Topic 证据 | 不要伪造 Topic；查官方最新文档或实测 |

---

## 10. G1-EDU 能力划分

| 能力 | 归属 | 说明 |
|---|---|---|
| 读关节/IMU/手柄 | SDK 原生 | `rt/lowstate` |
| 读 FSM | SDK 原生（RPC + 可能 Topic） | Topic 发布需实测 |
| 读电池 | 未知 | 类型存在，Topic 未确认 |
| 读相机 | 非 SDK | 见 [07](07_sensors_and_camera.md) |
| 安全门控 | 开发者适配 | SDK 只给谓词，不给策略 |
| 状态时间同步 | 开发者适配 | 无统一时间戳字段；`tick` 非时间 |

---

## 11. 相关文档

- 消息字段与关节索引：[03_message_types.md](03_message_types.md)
- 高层控制与 FSM：[05_g1_high_level_control.md](05_g1_high_level_control.md)
- 低层控制与 CRC：[06_g1_low_level_control.md](06_g1_low_level_control.md)
- 并发与生命周期：[08_concurrency_and_lifecycle.md](08_concurrency_and_lifecycle.md)
