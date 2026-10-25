# 01 · 通道与 DDS 通信基础

> 本文回答：如何初始化通信、创建发布/订阅、理解回调线程与消息生命周期、选择 `queuelen`，以及 G1 上实际存在哪些话题。
>
> 适用源码：`include/unitree/robot/channel/`、`include/unitree/common/dds/`、`include/unitree/dds_wrapper/common/`。

---

## 1. 模块定位

`ChannelFactory` + `ChannelPublisher` + `ChannelSubscriber` 是 SDK 最底层的**无 RPC 语义**通信封装：

- 一个话题 = 一个 DDS Topic；
- 发布 = `DdsWriter::Write`；
- 订阅 = `DdsReaderListener` 接收样本并回调；
- 消息类型由 C++ 模板参数决定，DDS 类型名由 IDL 生成代码的 `TopicTraits` 决定。

高层 `Client/Server` 也建立在同一套通道之上（见 [02_dds_rpc_client_server.md](02_dds_rpc_client_server.md)）。

### 1.1 适用场景

| 场景 | 建议 |
|---|---|
| 读机器人高频状态（`rt/lowstate`） | `ChannelSubscriber`，`queuelen=1`，回调只做拷贝 |
| 发布低层关节命令（`rt/lowcmd`） | `ChannelPublisher`，单独控制线程，周期写 |
| 发布手臂命令（`rt/arm_sdk`） | 同上；注意权重通道 |
| 自定义进程间传输 | 自定义 topic + 自己的 IDL 类型；或直接用 RPC |
| 高层运动/语音 | 不要用裸通道，用 `Client`（见 02、05） |

### 1.2 前置依赖

- 必须先调用 `ChannelFactory::Init(...)`；
- 必须知道机器人使用的 DDS domain 与网卡；
- 必须使用**与机器人固件一致的消息类型**；
- 低层命令还依赖 FSM/运动服务状态（见 06）。

---

## 2. 源码位置

| 文件 | 关键内容 |
|---|---|
| `unitree_sdk2/include/unitree/robot/channel/channel_factory.hpp` | 单例、Init/Release、通道创建 |
| `.../channel_publisher.hpp` | 发布封装 |
| `.../channel_subscriber.hpp` | 订阅封装、最后数据时间 |
| `.../channel_namer.hpp` | RPC 通道命名规则 |
| `unitree_sdk2/include/unitree/common/dds/dds_topic_channel.hpp` | Topic/Writer/Reader 组装；100 ms 等待 |
| `.../dds_entity.hpp` | Writer 写入、Reader 监听器、队列线程、回调 |
| `.../dds_factory_model.hpp` | Participant/Topic/QoS 持有与通道创建 |
| `.../dds_parameter.hpp` | JSON 配置键 |
| `unitree_sdk2/include/unitree/dds_wrapper/common/Subscription.h` | 带互斥锁的消息快照封装 |
| `.../Publisher.h` | 后台发布线程 `RealTimePublisher` |
| `.../crc.h` | G1 LowCmd/LowState CRC32 |

### 2.1 关键符号与行号

| 符号 | 位置 |
|---|---|
| `ChannelFactory::Instance()` | `channel_factory.hpp:19-23` |
| `Init(int32_t, const std::string&)` | `channel_factory.hpp:25` |
| `Init(const std::string&)` / `Init(const JsonMap&)` | `channel_factory.hpp:26-27` |
| `Release()` | `channel_factory.hpp:29` |
| `CreateSendChannel` | `channel_factory.hpp:32-37` |
| `CreateRecvChannel` | `channel_factory.hpp:40-45` |
| `ChannelPublisher::Write` | `channel_publisher.hpp:23-31` |
| `ChannelSubscriber::InitChannel(handler, queuelen)` | `channel_subscriber.hpp:22-28` |
| 空 handler 抛异常 | `channel_subscriber.hpp:36-39` |
| `GetLastDataAvailableTime` | `channel_subscriber.hpp:47-55` |
| 发布者创建后固定睡眠 100 ms | `dds_topic_channel.hpp:22,42-46` |
| `DdsWriter::Write` | `dds_entity.hpp:172-187` |
| 等待 reader 匹配 | `dds_entity.hpp:190-208` |
| `DdsReaderListener::on_data_available` | `dds_entity.hpp:307-338` |
| 队列线程 `rlsnr` | `dds_entity.hpp:252-289` |
| 监听器析构停线程 | `dds_entity.hpp:232-240` |
| RPC 通道前缀/后缀 | `channel_namer.hpp:10-12` |

---

## 3. 核心 API 表

### 3.1 ChannelFactory

| API | 声明位置 | 功能 | 参数 | 返回 | 副作用 | 生命周期 | 线程安全 | 异常/错误 | 典型场景 |
|---|---|---|---|---|---|---|---|---|---|
| `static ChannelFactory* Instance()` | `channel_factory.hpp:19` | 获取进程内单例 | 无 | 非空指针 | 首次调用构造单例 | 进程级 | C++11 静态局部变量初始化线程安全【源码】 | 无 | 所有通信入口 |
| `void Init(int32_t domainId, const std::string& networkInterface = "")` | `:25` | 初始化 DDS participant | domain（示例均为 0）；网卡名（空=默认） | void | 创建 DDS 实体、加载配置 | Init 后长期有效 | 未验证 | 失败可能抛 DDS 异常【推断】 | 指定机器人网卡 |
| `void Init(const std::string& configFileName = "")` | `:26` | 从配置文件初始化 | 配置文件路径，默认空 | void | 读取 JSON/XML 配置 | 未验证 | 未验证 | 未验证 | 需要自定义 QoS/域时 |
| `void Init(const common::JsonMap& jsonMap)` | `:27` | 从内存 JSON 初始化 | JSON map | void | 同上 | 未验证 | 未验证 | 未验证 | 程序内配置 |
| `void Release()` | `:29` | 释放 DDS 资源 | 无 | void | 销毁 participant/factory | 未验证 | 未验证 | 未验证 | 进程退出前；不建议在通道仍存活时调用 |
| `template<MSG> ChannelPtr<MSG> CreateSendChannel(name)` | `:32-37` | 创建发送通道 | topic 名 | 通道指针 | 创建 Writer；**固定睡眠 100 ms** | 与返回 shared_ptr 同寿命 | 未验证；无空指针检查 | Init 前调用会解引用空 `mDdsFactoryPtr` | 发布命令 |
| `template<MSG> ChannelPtr<MSG> CreateRecvChannel(name, callback, queuelen=0)` | `:40-45` | 创建接收通道 | topic、回调、队列长度 | 通道指针 | 创建 Reader/Listener；可能创建线程 | 同上 | 未验证 | 同上 | 订阅状态 |

> **Init 之前创建通道是硬错误。** `CreateSendChannel`/`CreateRecvChannel` 直接使用 `mDdsFactoryPtr->...`，头文件里没有判空（`channel_factory.hpp:32-45`）。`ChannelPublisher::InitChannel` 也不会检查工厂是否初始化。

### 3.2 ChannelPublisher

| API | 声明位置 | 功能 | 参数 | 返回 | 副作用 | 生命周期 | 线程安全 | 异常/错误 | 典型场景 |
|---|---|---|---|---|---|---|---|---|---|
| `explicit ChannelPublisher(name)` | `channel_publisher.hpp:14-16` | 记录 topic 名 | topic | — | 无 | — | — | — | 构造成员 |
| `void InitChannel()` | `:18-21` | 真正创建 DDS writer | 无 | void | **睡眠 100 ms** | 通道存活期 | 未验证 | Init 前调用崩溃【推断】 | 初始化阶段 |
| `bool Write(const MSG&, int64_t waitMicrosec = 0)` | `:23-31` | 发送一条消息 | 消息；等待 reader 的微秒数 | true=写入调用成功 | DDS 网络发送 | 可反复调用 | DDS Writer 可能线程安全，但封装未声明；建议单线程写 | 通道为空返回 false；DDS 异常被捕获并记录，返回 false | 周期命令 |
| `void CloseChannel()` | `:33-36` | 释放通道 | 无 | void | 销毁 writer | 可重新 InitChannel | 未验证 | 在回调内关闭同一通道可能死锁【推断】 | 退出/切换 |
| `GetChannelName()` | `:38-41` | 返回 topic 名 | 无 | const string& | 无 | — | 只读 | — | 日志 |

### 3.3 ChannelSubscriber

| API | 声明位置 | 功能 | 参数 | 返回 | 副作用 | 生命周期 | 线程安全 | 异常/错误 | 典型场景 |
|---|---|---|---|---|---|---|---|---|---|
| `ChannelSubscriber(name)` | `channel_subscriber.hpp:14-16` | 仅记录名字 | topic | — | 无 | — | — | — | 先构造后 Init |
| `ChannelSubscriber(name, handler, queuelen=0)` | `:18-20` | 记录 handler/队列 | topic、handler、队列 | — | 无 | — | — | 构造不会创建通道 | — |
| `void InitChannel(handler, queuelen=0)` | `:22-28` | 创建订阅并注册回调 | `std::function<void(const void*)>`、队列长度 | void | 创建 reader、可能创建线程 | 通道存活期 | 未验证 | handler 为空抛 `CommonException("subscribe handler is invalid")`【源码】 | 订阅状态 |
| `void InitChannel()` | `:30-40` | 使用已保存的 handler | 无 | void | 同上 | 同上 | 未验证 | 同上 | 构造时已给 handler |
| `void CloseChannel()` | `:42-45` | 释放通道 | 无 | void | 销毁 reader/listener；队列线程等待退出 | — | 未验证 | 回调内调用可能死锁【推断】 | 退出 |
| `int64_t GetLastDataAvailableTime() const` | `:47-55` | 最后收到有效样本的单调时间（ns） | 无 | ns；无通道返回 -1 | 无 | — | 读竞态：回调线程写、调用线程读；未加锁【源码】 | — | 连接健康检查 |
| `GetChannelName()` | `:57-60` | topic 名 | 无 | const string& | 无 | — | 只读 | — | 日志 |

### 3.4 底层写入语义（`DdsWriter`）

```cpp
// unitree_sdk2/include/unitree/common/dds/dds_entity.hpp:172-208
bool Write(const MSG& message, int64_t waitMicrosec)
{
    if (waitMicrosec > 0) WaitReader(waitMicrosec);
    UT_DDS_EXCEPTION_TRY { mNative.write(message); return true; }
    UT_DDS_EXCEPTION_CATCH(mLogger, false)
    return false;
}
```

- `waitMicrosec == 0`（默认）：**不等待任何订阅者**，直接调用 DDS `write`。没有订阅者时也可能返回 true，消息直接丢失。
- `waitMicrosec > 0`：先等待 `publication_matched_status().current_count() > 0`，最多等待 `waitMicrosec/2`，且被截断到 **1,000,000 µs = 1 s**；每 10 ms 检查一次（`dds_entity.hpp:190-208`、`dds_entity.hpp:19-20`）。它只保证“至少有一个 reader 匹配”，**不保证对方处理成功**。
- 即使 `waitMicrosec > 0`，`waitMicrosec < 10000` 会直接跳过等待（`:192-195`）。
- `Write` 捕获所有 DDS 异常并记录日志，然后返回 `false`；不会向调用者抛异常（`UT_DDS_EXCEPTION_CATCH(mLogger,false)`）。
- `ChannelPublisher::Write` 在通道未创建时直接返回 `false`（`channel_publisher.hpp:25-30`）。

---

## 4. 初始化与生命周期

### 4.1 推荐顺序

```text
1. 解析命令行（网卡、domain、模式）
2. ChannelFactory::Instance()->Init(domain, iface)
3. 构造所有 ChannelPublisher / ChannelSubscriber
4. InitChannel()  —— 每个 publisher 约 100 ms；放在启动阶段
5. 启动控制/推理线程
6. 主循环/等待 SIGINT
7. 停止线程 → 发送安全停止命令 → 销毁通道 → 进程退出
```

### 4.2 为什么 publisher 初始化要 100 ms

`DdsTopicChannel::SetWriter` 在创建 DDS writer 后无条件调用：

```cpp
// dds_topic_channel.hpp:42-46
void SetWriter(const DdsPublisherPtr& publisher, const DdsWriterQos& qos)
{
    mWriter = DdsWriterPtr<MSG>(new DdsWriter<MSG>(publisher, mTopic, qos));
    MicroSleep(UT_DDS_WAIT_MATCHED_TIME_MICRO_SEC); // 100000 us
}
```

【源码】这意味着：

- 每创建一个 publisher，启动阶段至少多 100 ms；
- 10 个 publisher 就是约 1 s；
- 这是固定睡眠，不是“等到匹配成功”，不能当作可靠性保证。

### 4.3 析构与关闭

- `ChannelSubscriber::CloseChannel()` 只是 `mChannelPtr.reset()`（`channel_subscriber.hpp:42-45`）；真正的 reader/listener 在 shared_ptr 归零时销毁。
- 带队列的 `DdsReaderListener` 析构会：`mQuit = true; mDataQueuePtr->Interrupt(false); mDataQueueThreadPtr->Wait();`（`dds_entity.hpp:232-240`）。
- `ChannelPublisher` 没有显式析构函数；writer 在通道销毁时释放。
- `ChannelFactory::Release()` 的行为在预编译库中，**未验证**。如果仍有通道持有 DDS 实体，提前 Release 可能造成悬空；建议在所有通道销毁后再调用，或干脆让进程退出自动回收。

---

## 5. 回调、线程与消息生命周期

### 5.1 两种模式

| 模式 | 触发条件 | 回调执行线程 | 消息来源 | 优点 | 风险 |
|---|---|---|---|---|---|
| 直接模式 | `queuelen <= 0` | CycloneDDS 监听/分发线程 | DDS loaned sample 的栈上引用 | 少一次拷贝、延迟低 | 回调阻塞会拖住 DDS 分发；不能做重活 |
| 队列模式 | `queuelen > 0` | SDK 创建的 `rlsnr` 线程（每个订阅者一个） | `BlockQueue` 中的 `shared_ptr<MSG>` 拷贝 | 解耦 DDS 接收与业务处理 | 多一次拷贝；队列满丢最旧；仍只有一个消费线程 |

源码证据：

```cpp
// dds_entity.hpp:325-335
if (mHasQueue) {
    if (!mDataQueuePtr->Put(MSG_PTR(new MSG(m)), true))
        LOG_WARNING(mLogger, "earliest mesage was evicted. type:", DdsGetTypeName(MSG));
} else {
    mCallbackPtr->OnDataAvailable((const void*)&m);
}
```

```cpp
// dds_entity.hpp:259-288
mHasQueue = true;
mDataQueuePtr.reset(new BlockQueue<MSG_PTR>(len));
...
mDataQueueThreadPtr = CreateThreadEx("rlsnr", UT_CPU_ID_NONE, queueThreadFunc);
```

### 5.2 回调指针只在回调期间有效

`on_data_available` 中 `const MSG& m = iter->data();` 指向 DDS 借出的样本；回调返回后样本可能被回收。**禁止保存 `const void*` 指针**。示例统一做值拷贝：

```cpp
// example/g1/high_level/g1_arm7_sdk_dds_example.cpp:88-89
auto s = (const unitree_hg::msg::dds_::LowState_*)msg;
memcpy(&state_msg, s, sizeof(unitree_hg::msg::dds_::LowState_));
```

`LowState_` 是 POD 风格生成类型，可以整体赋值/`memcpy`；含 `std::vector` 的 `HandState_`/`HandCmd_` 应使用拷贝构造，不要 `memcpy`。

### 5.3 队列行为

`BlockQueue::Put(t, replace=true)` 在满时 `pop_front()` 再 `emplace_back()`，并返回 `false`；调用方打印 “earliest mesage was evicted”（原文拼写）警告。见 `common/block_queue.hpp:24-58`。

因此：

- `queuelen=1` 对状态流是合理的：总是处理最新样本；
- 队列模式不保证“每帧都处理”，只保证顺序和最新性；
- 处理慢时会持续丢旧帧，日志可能刷屏；
- 队列线程对每个订阅者是串行的，多订阅者可并行。

### 5.4 回调里不能做什么

| 禁止 | 原因 |
|---|---|
| 保存消息指针 | 回调返回后失效 |
| 长时间推理/解码 | 阻塞 DDS 分发或该订阅者的 `rlsnr` 线程，导致队列溢出 |
| 阻塞式 RPC（`Client::Call`） | 可能长时间占用回调线程；与 DDS 线程交互复杂，延迟不可控 |
| `std::cout` 大量输出 | 可能阻塞；示例中也有此问题 |
| 在回调内 `CloseChannel()` 当前订阅 | 析构会等待回调线程/队列线程，可能自锁【推断】 |
| 抛出未捕获异常 | 异常会进入 DDS/SDK 线程，行为未定义【推断】 |

### 5.5 异步语义总结

| 问题 | 答案 |
|---|---|
| 回调何时执行？ | 有数据到达时；没有周期性保证 |
| 成功/失败如何知道？ | 直接模式下无返回值；队列模式下无返回值；只能通过数据是否更新判断 |
| 超时如何处理？ | SDK 不提供回调超时；用 `GetLastDataAvailableTime()` 或自己的时间戳 |
| 顺序如何？ | 单订阅者队列内 FIFO；DDS 层按 writer/QoS 决定跨源顺序；不保证跨话题顺序 |
| 会丢消息吗？ | 会；默认 QoS 未知，队列满必丢旧；网络/UDP 也可能丢 |
| 回调会并发吗？ | 同一订阅者的队列消费者串行；不同订阅者可并发；直接模式的 DDS 分发并发度未验证 |

---

## 6. G1 话题表

> 只列出**在 G1 示例或 G1 封装中有直接证据**的话题；`【推断】` 表示类型映射合理但未在示例中显式订阅。

| 话题 | 方向（应用视角） | 消息类型 | 证据 | 说明 |
|---|---|---|---|---|
| `rt/lowstate` | 订阅 | `unitree_hg::msg::dds_::LowState_` | `example/g1/low_level/g1_ankle_swing_example.cpp:20,197-198` | 关节、IMU、手柄、CRC |
| `rt/lowcmd` | 发布 | `unitree_hg::msg::dds_::LowCmd_` | 同上 `:18,194-195` | 全身低层命令，高风险 |
| `rt/arm_sdk` | 发布 | `unitree_hg::msg::dds_::LowCmd_` | `g1_arm7_sdk_dds_example.cpp:11` | 手臂接管；索引 29 为权重 |
| `rt/user_lowcmd` | 发布 | `unitree_hg::msg::dds_::LowCmd_` | `g1_userctrl_dds_example.cpp:13` | 用户全身控制模式 |
| `rt/secondary_imu` | 订阅 | `unitree_hg::msg::dds_::IMUState_` | `g1_ankle_swing_example.cpp:19,200` | 躯干副 IMU（示例注释） |
| `rt/hand_sdk` | 发布 | `unitree_go::msg::dds_::MotorCmds_` | `g1_hand_sdk_example.cpp:17,27` | 手部力矩；权重复用 `cmds[0].mode` |
| `rt/dex3/left/cmd` | 发布 | `unitree_hg::msg::dds_::HandCmd_` | `g1_dex3_example.cpp:32,269` | 左手 Dex3 |
| `rt/dex3/right/cmd` | 发布 | 同上 | `g1_dex3_example.cpp:256,269` | 右手 Dex3 |
| `rt/dex3/left/state` | 订阅 | `unitree_hg::msg::dds_::HandState_` | `g1_sub.h:97`、`g1_dex3_example.cpp:33` | 左手状态（基础名） |
| `rt/dex3/right/state` | 订阅 | 同上 | `g1_sub.h:105` | 右手状态（基础名） |
| `rt/lf/dex3/left/state` | 订阅 | 同上 | `g1_dex3_example.cpp:253` | 示例运行 L 时实际使用；与基础名冲突 |
| `rt/lf/dex3/right/state` | 订阅 | 同上 | `g1_dex3_example.cpp:257` | 示例运行 R 时实际使用 |
| `rt/inspire/state` | 订阅 | `unitree_go::msg::dds_::MotorStates_` | `g1_sub.h:89` | Inspire 手状态；无 G1 示例验证 |
| `rt/audio_msg` | 订阅 | `std_msgs::msg::dds_::String_` | `g1_audio_client_example.cpp:5,12,24` | ASR/事件文本（示例直接打印 data） |
| `rt/sportmodestate` | 订阅 | `unitree_hg::msg::dds_::SportModeState_`【推断】 | 注释 `g1_arm_action_error.hpp:12-14` | G1 只有 4 字段；类型未在示例中显式使用 |
| `rt/arm/action/state` | 订阅 | JSON 文本（具体类型未声明） | 注释 `g1_arm_action_client.hpp:17-22` | 动作状态 `holding/id/name` |
| `rt/hispeed_state` | 订阅 | `geometry_msgs::msg::dds_::Point32_` | `g1d_height_control.cpp:14,18,35-40` | G1D 高度；使用 `y()` |
| `rt/waistpitchcmd` | 发布 | `unitree_hg::msg::dds_::MotorCmd_` | `g1d_waist_example.cpp:12,35-42` | G1D 腰部俯仰 |
| `rt/waistpitchstate` | 订阅 | `unitree_hg::msg::dds_::MotorState_` | `g1d_waist_example.cpp:13,27-32` | G1D 腰部状态 |
| `rt/wirelesscontroller` | 订阅 | `unitree_go::msg::dds_::WirelessController_` | `example/wireless_controller/main.cpp:10` | GO2 风格独立手柄话题；G1 用手柄数据在 `LowState_.wireless_remote` |
| `rt/api/<service>/request` | 发布/订阅 | `unitree_api::msg::dds_::Request_` | `channel_namer.hpp:10-12` | RPC 请求 |
| `rt/api/<service>/response` | 订阅/发布 | `unitree_api::msg::dds_::Response_` | 同上 | RPC 响应 |

### 6.1 同名不同型警告

| 话题 | G1 | GO2 |
|---|---|---|
| `rt/lowstate` | `unitree_hg::LowState_`（35 槽、无 BMS） | `unitree_go::LowState_`（四足结构） |
| `rt/sportmodestate` | `unitree_hg::SportModeState_`（仅 4 字段） | `unitree_go::SportModeState_`（位置/速度/步态/足端力） |

在同一个 DDS domain 内，**topic 名相同但 type name 不同不会匹配**。跨机型调试时先在代码里确认类型，不要只看 `ros2 topic list`。

---

## 7. 可运行性最小的只读示例（已编译通过，未运行）

目标：初始化 DDS，订阅 `rt/lowstate`，每 100 ms 打印一次姿态和前 3 个关节位置；Ctrl-C 干净退出。**不发送任何命令。**

```cpp
// lowstate_reader.cpp — 文档示例，已编译通过、未运行
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <iostream>
#include <mutex>
#include <thread>

#include <unitree/idl/hg/LowState_.hpp>
#include <unitree/robot/channel/channel_factory.hpp>
#include <unitree/robot/channel/channel_subscriber.hpp>

using namespace unitree::robot;
using namespace unitree_hg::msg::dds_;

static std::atomic<bool> g_running{true};
static void OnSignal(int) { g_running.store(false); }

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <network_interface>\n";
        return 2;
    }

    std::signal(SIGINT, OnSignal);
    std::signal(SIGTERM, OnSignal);

    ChannelFactory::Instance()->Init(0, argv[1]);

    LowState_ snapshot{};
    std::mutex snapshot_mutex;
    std::atomic<int64_t> last_rx_ns{0};

    ChannelSubscriber<LowState_> subscriber("rt/lowstate");
    subscriber.InitChannel([&](const void* message) {
        const auto* state = static_cast<const LowState_*>(message);
        {
            std::lock_guard<std::mutex> lock(snapshot_mutex);
            snapshot = *state;              // 值拷贝，不能保存 message 指针
        }
        last_rx_ns.store(std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }, 1);                                   // queuelen=1：只要最新状态

    const auto start = std::chrono::steady_clock::now();
    while (g_running.load() && last_rx_ns.load() == 0) {
        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(5)) {
            std::cerr << "no rt/lowstate data in 5s: check iface/domain/type\n";
            return 1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    while (g_running.load()) {
        LowState_ local;
        {
            std::lock_guard<std::mutex> lock(snapshot_mutex);
            local = snapshot;
        }
        const auto& rpy = local.imu_state().rpy();
        std::cout << "rpy(rad)=" << rpy[0] << "," << rpy[1] << "," << rpy[2]
                  << " q[0..2]=" << local.motor_state()[0].q() << ","
                  << local.motor_state()[1].q() << ","
                  << local.motor_state()[2].q() << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    subscriber.CloseChannel();
    return 0;
}
```

编译（假设 SDK 已安装到 `/opt/unitree_robotics`）：

```cmake
find_package(unitree_sdk2 2.0.0 EXACT REQUIRED)
add_executable(lowstate_reader lowstate_reader.cpp)
target_link_libraries(lowstate_reader PRIVATE unitree_sdk2)
```

> 本示例仍未验证实机；如果 5 秒内无数据，按第 9 节排查，不要直接改成发命令。

---

## 8. 与 `dds_wrapper` 的关系

`dds_wrapper` 在 Channel 之上又包了一层：

- `SubscriptionBase<MSG>`：内部创建 `ChannelSubscriber`，回调中加 `std::mutex` 拷贝到 `msg_`，提供 `wait_for_connection()`、`isTimeout()`（`Subscription.h:22-79`）。
- `RealTimePublisher<MSG>`：后台线程 + 双缓冲式 `trylock/unlockAndPublish`，适合控制循环（`Publisher.h:38-174`）。
- G1 专用类型与 CRC 在 `g1_pub.h` / `g1_sub.h`。

**不要混用两种风格**：用 `SubscriptionBase` 时不要再手动 `InitChannel`；用 `RealTimePublisher` 时不要直接写 `msg_` 而不加锁。

详见 [06_g1_low_level_control.md](06_g1_low_level_control.md) 与 [08_concurrency_and_lifecycle.md](08_concurrency_and_lifecycle.md)。

---

## 9. 常见错误与排查

| 现象 | 可能原因 | 排查 |
|---|---|---|
| 程序启动即崩溃（Init 前建通道） | 忘记 `ChannelFactory::Init` | 确保 Init 在最前；检查是否在全局对象构造期建通道 |
| `subscribe handler is invalid` | `InitChannel()` 时 handler 为空 | 使用带 handler 的重载，或先赋值成员 |
| 订阅一直无数据 | 网卡/domain 错；类型不匹配；机器人未发布 | 用 `GetLastDataAvailableTime()`；换正确网卡；确认消息类型 |
| 第一次 `Write` 丢失 | 默认 `waitMicrosec=0`，reader 未匹配 | 启动后等待状态；首帧前用 `Write(msg, 100000)` 或延迟启动 |
| 启动特别慢 | 每个 publisher 固定 100 ms | 统计 publisher 数量；合并/延迟创建 |
| 队列警告刷屏 | 消费太慢，队列满丢旧帧 | 减小回调工作量；增大队列；只保留最新帧 |
| 回调里死锁 | 回调内关通道/等锁/调 RPC | 回调只拷贝，业务放到独立线程 |
| 退出卡住 | `rlsnr` 线程/`RealTimePublisher` 析构等待 | 先停业务线程与发布，再销毁通道；不要 `_exit` |
| `Write` 返回 false | 通道未初始化，或 DDS 异常 | 检查 `InitChannel`；开启 SDK 日志 |
| 两个进程都发 `rt/lowcmd` | DDS 层无独占 | 设计单一命令源；用模式/租约/进程监督 |

### 9.1 连接健康检查

```cpp
auto last_ns = subscriber.GetLastDataAvailableTime(); // -1 表示通道未建立
// 自己记录接收时间更可靠：GetLastDataAvailableTime 在回调前更新，
// 且读取无锁，只适合粗粒度判断。
```

G1 安全判据 `lost_connection` 使用 1000 ms 默认超时（`terminations.hpp:81-86`），但初始值为 0，意味着**还没收到第一帧时就会被判为断连**。使用前先等待首帧，或自己处理初始状态。

---

## 10. G1-EDU 部署场景划分

| 能力 | SDK 原生 | 需要开发者适配 | ROS 2/第三方 | 未知/未验证 |
|---|---|---|---|---|
| DDS 初始化/域/网卡 | ✅ `ChannelFactory::Init` | 配置管理、故障重试 | — | 默认 QoS、Release 语义 |
| 状态订阅 | ✅ `ChannelSubscriber` | 快照、时间戳、丢帧策略、看门狗 | ROS 2 桥接 | 实际频率/延迟 |
| 命令发布 | ✅ `ChannelPublisher` | 单写者、限幅、权重渐变、失联停止 | — | DDS 匹配/丢包行为 |
| 回调线程 | ✅ 直接/队列两种 | 线程池、优先级、CPU 绑核 | — | DDS 分发线程细节 |
| 图像传输 | ❌ | 外接相机/自定义 DDS 类型 | ROS 2 相机驱动 | G1 是否有厂商图像服务 |
| 高频实时性 | ⚠️ 普通 Linux 线程 | 实时优先级、无锁队列、绑核 | — | 内核/调度未配置 |

---

## 11. 要点回顾

1. `Init` 是一切的前提；Init 前建通道没有保护。
2. 每个 publisher 固定 100 ms 启动成本。
3. `Write` 默认不等待订阅者；等待参数最多 1 s，且只等“匹配”，不等“送达”。
4. 回调里的消息指针只活一次；必须拷贝。
5. `queuelen=1` 是状态订阅的常用选择；满了丢最旧。
6. G1 的 `rt/lowstate` 是 `unitree_hg::LowState_`，不是 GO2 的类型。
7. 任何“实时”承诺都要自己用时间戳、看门狗和线程模型补上。
