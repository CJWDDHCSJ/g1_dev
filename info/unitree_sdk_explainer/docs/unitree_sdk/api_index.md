# API 总索引（G1-EDU 优先）

> 本索引把 Unitree SDK2 中**有头文件证据的公开接口**按“部署时怎么用”重新排序，而不是按目录罗列。
> 行号基于当前 checkout（SDK 2.0.0，git `2.0.2-75-gd1abf00`）；SDK 升级后会漂移。
> 证据标签、风险分级与安全边界见 [README.md](README.md)；本文只做索引，完整语义以各模块文档为准。

---

## 0. 怎么用这份索引

### 0.1 三条使用路径

| 路径 | 入口 | 适用场景 | 首先读 |
|---|---|---|---|
| **A. 只读监控** | `ChannelFactory` + `ChannelSubscriber` | 读关节/IMU/状态，给推理和安全门控提供输入 | §2、§5、[04](04_robot_state.md) |
| **B. RPC 查询/控制** | `ChannelFactory` + `LocoClient` / `MotionSwitcherClient` | 查询 FSM、切换模式、发运动指令 | §3、§4、[05](05_g1_high_level_control.md) |
| **C. 低层注入** | `ChannelPublisher` + `LowCmd_` | 直接控制关节/手臂/手 | §5、[06](06_g1_low_level_control.md)（高风险） |

### 0.2 优先级与风险

| 标记 | 含义 |
|---|---|
| **P0** | G1-EDU 首次部署必须掌握：通信初始化、通道、构建、线程/退出、日志 |
| **P1** | 状态订阅、RPC 只读查询、多线程/异步、ROS 2/推理集成需要 |
| **P2** | 运动控制、低层控制、手部/音频等非核心功能，需额外授权与安全措施 |
| **R0** | 只读或无机器人副作用 |
| **R1** | 非运动输出（声音、LED） |
| **R2** | 肢体动作（手臂、手、腰），可能夹伤/碰撞 |
| **R3** | 运动/低层/整机命令，可能摔倒/撞人 |

> **P 与 R 是两回事**：`GetStandHeight` 是 P1、R0；`SetStandHeight` 是 P2、R3。不要因为“只是改身高”就当成低风险。

### 0.3 证据等级

本文所有签名均来自头文件。标记说明：

- **内联**：函数体在头文件中，可直接确认行为。
- **声明**：只有声明，实现位于 `lib/*/libunitree_sdk2.a`，行为【未验证】。
- **模板**：头文件模板，实例化行为可读，但底层 DDS 行为仍受库影响。

---

## 1. 按任务找 API

| 我要做什么 | 首选 API / 话题 | P / R | 详见 |
|---|---|---|---|
| 初始化 DDS、选择网卡/域 | `ChannelFactory::Init` | P0 / R0 | [01](01_channel_communication.md) §3.1、[12](12_network_and_deployment.md) §3 |
| 订阅关节/IMU 状态 | `ChannelSubscriber<unitree_hg::msg::dds_::LowState_>` + `rt/lowstate` | P1 / R0 | [01](01_channel_communication.md) §3.3、[04](04_robot_state.md) §2 |
| 发布自定义话题 | `ChannelPublisher<MSG>` | P0 / R0 | [01](01_channel_communication.md) §3.2 |
| 查询当前 FSM / 身高 / 平衡模式 | `LocoClient::GetFsmId` 等 | P1 / R0 | §4.2、[05](05_g1_high_level_control.md) §4 |
| 让机器人站起/行走/停止 | `LocoClient::StandUp` / `Move` / `StopMove` | P2 / R3 | §4.3、[05](05_g1_high_level_control.md) §5 |
| 释放内置运控、切模式 | `MotionSwitcherClient::CheckMode/SelectMode/ReleaseMode` | P2 / R3 | §4.8、[05](05_g1_high_level_control.md) §6 |
| 执行预设手臂动作 | `G1ArmActionClient::ExecuteAction(int)` | P2 / R2 | §4.5、[05](05_g1_high_level_control.md) §7 |
| 手臂连续控制 | `rt/arm_sdk` + 索引 29 权重 | P2 / R2 | §5.2、[06](06_g1_low_level_control.md) §4 |
| 全身低层控制 | `rt/lowcmd` + CRC | P2 / R3 | §5.2、[06](06_g1_low_level_control.md) §2 |
| 手部控制 | `rt/hand_sdk` / `rt/dex3/*/cmd` | P2 / R2 | §5.2、[06](06_g1_low_level_control.md) §5 |
| 播放语音 / 控制 LED | `AudioClient` | P2 / R1 | §4.6、[05](05_g1_high_level_control.md) §8.1 |
| 取相机图像 | **SDK 不提供 G1 相机 API** | — | [07](07_sensors_and_camera.md) §1 |
| 安全判断（姿态/超温/掉线） | `g1/common/terminations.hpp` | P1 / R0 | §5.4、[04](04_robot_state.md) §6 |
| 后台周期任务 | `CreateRecurrentThreadEx` | P0 / R0 | §6、[08](08_concurrency_and_lifecycle.md) §4 |
| 自定义 RPC 服务端 | `Server` / `ServerBase` | P2 / R0 | §3.4、[02](02_dds_rpc_client_server.md) §4.4 |

---

## 2. 初始化与通道 API

头文件：`unitree_sdk2/include/unitree/robot/channel/`

### 2.1 `ChannelFactory`（单例）

| API | 签名与位置 | 语义 | P / R |
|---|---|---|---|
| `Instance` | `static ChannelFactory* Instance()`，`channel_factory.hpp:19-23` | 函数内静态单例；首次调用构造 | P0 / R0 |
| `Init`（域+网卡） | `void Init(int32_t domainId, const std::string& networkInterface = "")`，`:25`（声明） | 指定 DDS domain 与网卡；**所有通道创建前必须调用** | P0 / R0 |
| `Init`（配置文件） | `void Init(const std::string& configFileName = "")`，`:26`（声明） | 从 JSON 配置初始化；空串行为【未验证】 | P0 / R0 |
| `Init`（JsonMap） | `void Init(const common::JsonMap& jsonMap)`，`:27`（声明） | 以内存 JSON 配置初始化 | P0 / R0 |
| `Release` | `void Release()`，`:29`（声明） | 释放 DDS 实体；调用顺序见 [08](08_concurrency_and_lifecycle.md) §3.3 | P0 / R0 |
| `CreateSendChannel` | `template<typename MSG> ChannelPtr<MSG> CreateSendChannel(const std::string& name)`，`:31-37`（模板） | 创建 topic + writer；内部直接解引用 `mDdsFactoryPtr`，**Init 前调用是未定义行为** | P0 / R0 |
| `CreateRecvChannel` | `template<typename MSG> ChannelPtr<MSG> CreateRecvChannel(const std::string& name, std::function<void(const void*)> callback, int32_t queuelen = 0)`，`:39-45`（模板） | 创建 topic + reader + listener；`queuelen` 决定回调线程模型 | P0 / R0 |

### 2.2 `ChannelPublisher<MSG>`

| API | 签名与位置 | 语义 | P / R |
|---|---|---|---|
| 构造 | `explicit ChannelPublisher(const std::string& channelName)`，`channel_publisher.hpp:14-16` | 只保存名字，不创建 DDS 实体 | P0 / R0 |
| `InitChannel` | `void InitChannel()`，`:18-21` | 调用 `CreateSendChannel`；writer 创建后库内等待约 100 ms | P0 / R0 |
| `Write` | `bool Write(const MSG& msg, int64_t waitMicrosec = 0)`，`:23-31` | 未初始化时返回 `false`；不抛异常；阻塞语义由底层 DDS 决定 | P0 / R0 |
| `CloseChannel` | `void CloseChannel()`，`:33-36` | `reset` 内部通道指针，不保证对端立即感知 | P0 / R0 |
| `GetChannelName` | `const std::string& GetChannelName() const`，`:38-41` | 返回构造时保存的名字 | P0 / R0 |

### 2.3 `ChannelSubscriber<MSG>`

| API | 签名与位置 | 语义 | P / R |
|---|---|---|---|
| 构造（延迟初始化） | `explicit ChannelSubscriber(const std::string& channelName)`，`channel_subscriber.hpp:14-16` | 只保存名字；之后必须显式 `InitChannel(handler)` | P0 / R0 |
| 构造（带回调） | `ChannelSubscriber(const std::string&, const std::function<void(const void*)>& handler, int64_t queuelen = 0)`，`:18-20` | 保存 handler，仍需调用 `InitChannel()` | P0 / R0 |
| `InitChannel(handler, queuelen)` | `void InitChannel(const std::function<void(const void*)>& handler, int64_t queuelen = 0)`，`:22-28` | 设置回调并创建 reader | P0 / R0 |
| `InitChannel()` | `void InitChannel()`，`:30-40` | 无 handler 时抛 `CommonException("subscribe handler is invalid")` | P0 / R0 |
| `CloseChannel` | `void CloseChannel()`，`:42-45` | `reset` 通道；回调是否仍在途【未验证】 | P0 / R0 |
| `GetLastDataAvailableTime` | `int64_t GetLastDataAvailableTime() const`，`:47-55` | 返回最后收到数据的时间（纳秒）；无通道返回 `-1` | P1 / R0 |
| `GetChannelName` | `const std::string& GetChannelName() const`，`:57-60` | 返回话题名 | P0 / R0 |

### 2.4 通道语义速记

| 事实 | 证据 | 工程含义 |
|---|---|---|
| `queuelen == 0`：回调直接在 CycloneDDS listener 线程执行 | `common/dds/dds_entity.hpp:307-338` | 回调阻塞会阻塞该 reader 后续数据；回调内不要做推理/IO |
| `queuelen > 0`：独立 `rlsnr` 线程 + 有界 `BlockQueue` | 同上 `:262-285` | 回调串行；队列满时替换最旧元素并打印 `earliest mesage was evicted` |
| 回调参数 `const void*` 只在回调期间有效 | [01](01_channel_communication.md) §5.2 | 必须深拷贝到自己的消息对象 |
| 订阅没有 QoS 公开设置接口 | `channel_factory.hpp` 全文；[12](12_network_and_deployment.md) §6 | 不要假设能配 RELIABLE/BEST_EFFORT/深度 |
| 发送通道创建后等待约 100 ms | `common/dds/dds_topic_channel.hpp:22,45` | 初始化阶段可接受；实时循环里不要反复建通道 |
| 话题名不做类型校验 | [01](01_channel_communication.md) §6.1 | 同名不同型不会匹配；G1/GO2 混用会“静默无数据” |

### 2.5 `ChannelNamer`（了解即可）

| 符号 | 位置 | 说明 |
|---|---|---|
| `ROBOT_SDK_CHANNEL_PREFIX = "rt/api/"` | `channel_namer.hpp:10` | RPC 话题前缀 |
| `..._SUFFIX_CLIENT = "/request"` / `..._SERVER = "/response"` | `:11-12` | 客户端发 request、服务端回 response |
| `ClientChannelNamer` / `ServerChannelNamer` | `:31-59` | 生成 RPC 通道名；普通 pub/sub 不需要 |

---

## 3. RPC 框架 API

头文件：`unitree_sdk2/include/unitree/robot/client/`、`server/`

### 3.1 `ClientBase`

| API | 签名与位置 | 语义 | P / R |
|---|---|---|---|
| 默认超时 | `const int64_t ROBOT_CLIENT_TIMEOUT = 1000000`，`client_base.hpp:14` | 1,000,000 µs = **1 s** | P0 / R0 |
| 构造 | `explicit ClientBase(const std::string& name)`，`:23`（声明） | `name` 是服务名（如 `"sport"`） | P0 / R0 |
| `Init` | `virtual void Init() = 0`，`:26` | 子类实现：注册 API、建立通道 | P0 / R0 |
| `SetTimeout` | `void SetTimeout(int64_t timeout)` / `void SetTimeout(float timeout)`，`:28-29`（声明） | 整数按 µs；浮点按秒【示例 `SetTimeout(10.f)`】 | P1 / R0 |
| `Call`（protected） | 多个重载，`:32-40`（声明） | 同步 RPC；字符串/二进制/混合参数；返回错误码 | P1 / R0 |
| `SetHeader` | `void SetHeader(RequestHeader&, int32_t apiId, int64_t leaseId, int32_t priority, bool noReply)`，`:42` | 子类构造请求头时使用 | P2 / R0 |

> `Call` 是 **protected**：应用层不直接调用，而是用 `LocoClient` 等封装。自己实现客户端时继承 `Client`。

### 3.2 `Client`

| API | 签名与位置 | 语义 | P / R |
|---|---|---|---|
| 构造 | `explicit Client(const std::string& name, bool enableLease = false)`，`client.hpp:24`（声明） | `enableLease=true` 时启用租约 | P1 / R0 |
| `WaitLeaseApplied` | `void WaitLeaseApplied()`，`:27`（声明） | 阻塞直到租约生效；失败/超时行为【未验证】 | P2 / R0 |
| `GetLeaseId` | `int64_t GetLeaseId()`，`:28`（声明） | 获取当前租约 ID | P2 / R0 |
| `GetApiVersion` | `const std::string& GetApiVersion() const`，`:30`（声明） | 本地声明的 API 版本 | P1 / R0 |
| `GetServerApiVersion` | `std::string GetServerApiVersion()`，`:31`（声明） | 向服务端查询版本；会阻塞 | P1 / R0 |
| `Call`（protected） | `:38-44`（声明） | 无 lease 参数的便捷重载 | P2 / R0 |
| `RegistApi` | `void RegistApi(int32_t apiId, int32_t priority = 0)`，`:46`（声明） | 注册本客户端会用的 API；未注册调用会失败 | P1 / R0 |
| `CheckApi` | `int32_t CheckApi(int32_t apiId, int32_t& priority, int64_t& leaseId)`，`:47`（声明） | 校验 API 是否可用 | P2 / R0 |

### 3.3 `LeaseClient`

| API | 签名与位置 | 语义 | P / R |
|---|---|---|---|
| 构造 | `explicit LeaseClient(const std::string& name)`，`lease_client.hpp:35`（声明） | 租约客户端 | P2 / R0 |
| `Init` | `void Init()`，`:38`（声明） | 建立租约通道并启动续约线程【未验证】 | P2 / R0 |
| `WaitApplied` | `void WaitApplied()`，`:40`（声明） | 等待租约被服务端批准 | P2 / R0 |
| `GetId` | `int64_t GetId()`，`:41`（声明） | 当前租约 ID | P2 / R0 |
| `Applied` | `bool Applied()`，`:42`（声明） | 租约是否有效 | P2 / R0 |

### 3.4 `Server` / `ServerBase`

> 只有需要**在机器人侧/边缘侧提供自定义 RPC 服务**时才用。G1-EDU 部署通常只做客户端。

| API | 签名与位置 | 语义 | P / R |
|---|---|---|---|
| `Server` 构造 | `explicit Server(const std::string& name)`，`server.hpp:29`（声明） | 服务名决定 `rt/api/<name>/...` | P2 / R0 |
| `Init` | `virtual void Init() = 0`，`:32` | 子类注册 handler | P2 / R0 |
| `StartLease` | `void StartLease(int64_t leaseTerm)` / `(float)`，`:34-35`（声明） | 启用租约，期限 µs / 秒【推断】 | P2 / R0 |
| `GetName` | `const std::string& GetName()`，`:37`（声明） | 服务名 | P2 / R0 |
| `GetApiVersion` | `const std::string& GetApiVersion() const`，`:38`（声明） | 本服务 API 版本 | P2 / R0 |
| `GetCurrentApiId` | `int32_t GetCurrentApiId() const`，`:40`（声明） | 当前线程正在处理的 API ID | P2 / R0 |
| `RegistHandler` | `void RegistHandler(int32_t apiId, const RequestHandler&, bool checkLease = false)`，`:47`（声明） | 注册字符串 handler | P2 / R0 |
| `RegistBinaryHandler` | `void RegistBinaryHandler(int32_t apiId, const BinaryRequestHandler&, bool checkLease = false)`，`:48`（声明） | 注册二进制 handler | P2 / R0 |
| `ServerBase::Start` | `virtual void Start(bool enableProiQueue = false)`，`server_base.hpp:17`（声明） | 启动请求处理 | P2 / R0 |
| `ServerBase::SendResponse` | `void SendResponse(const Response&)`，`:23`（声明） | 发送响应 | P2 / R0 |

### 3.5 RPC 错误码

| 范围 | 含义 | 来源 |
|---|---|---|
| `3102`–`3107` | 请求/响应通道、序列化、超时类错误 | `robot/internal/internal_error.hpp`，详见 [02](02_dds_rpc_client_server.md) §5.2 |
| `3201`–`3207` | 服务端处理/租约/API 类错误 | 同上 |
| `7301`–`7303` | G1 `sport`：状态不可用、非法 FSM ID、非法 task ID | `g1_loco_error.hpp:9-12` |
| `7400`–`7402`、`7404` | G1 `arm`：`rt/armsdk` 被占用、holding、非法动作、非法 FSM（头文件未定义 7403） | `g1_arm_action_error.hpp:9-16` |
| `9101`–`9103` | G1D `agv`：未初始化、Move 失败、HeightAdjust 失败 | `g1_agv_error.hpp:13-15` |
| `100` | G1 `voice`：参数非法（宏名却叫 `COMM`） | `g1_audio_error.hpp:9` |

> 返回值 `0` 表示 **RPC 调用成功**，不表示动作完成。运动类 API 的完成状态要另查状态话题。详见 [05](05_g1_high_level_control.md) §9.4。

---

## 4. G1 高层服务 API

### 4.1 服务与版本矩阵

| 服务名 | 客户端 | API 版本 | API ID | 证据 |
|---|---|---|---|---|
| `sport` | `unitree::robot::g1::LocoClient` | `1.0.0.0` | 7001–7006、7100–7107、7110–7111 | `g1_loco_api.hpp:11-33` |
| `arm` | `unitree::robot::g1::G1ArmActionClient` | `1.0.0.14` | 7106、7107、7108、7113 | `g1_arm_action_api.hpp:10-19` |
| `voice` | `unitree::robot::g1::AudioClient` | `1.0.0.0` | 1001–1006、1010 | `g1_audio_api.hpp:11-23` |
| `agv` | `unitree::robot::g1::AgvClient` | `1.0.0.1` | 1001、1002 | `g1_agv_api.hpp:15-22` |
| `motion_switcher` | `unitree::robot::b2::MotionSwitcherClient` | `1.0.0.1` | 1001–1005 | `motion_switcher_api.hpp:15-29` |

> API ID **只在各自服务内唯一**。`sport` 和 `arm` 都有 7106/7107，含义不同。

### 4.2 `LocoClient` 只读 API

头文件：`g1_loco_client.hpp`；方法均为**内联**。

| API | 签名与位置 | 返回/输出 | P / R |
|---|---|---|---|
| `GetFsmId` | `int32_t GetFsmId(int& fsm_id)`，`:40-52` | 0 成功；输出 FSM ID | P1 / R0 |
| `GetFsmMode` | `int32_t GetFsmMode(int& fsm_mode)`，`:54-66` | 输出 FSM mode | P1 / R0 |
| `GetBalanceMode` | `int32_t GetBalanceMode(int& balance_mode)`，`:68-80` | 输出平衡模式 | P1 / R0 |
| `GetSwingHeight` | `int32_t GetSwingHeight(float& swing_height)`，`:82-94` | 输出摆腿高度 | P1 / R0 |
| `GetStandHeight` | `int32_t GetStandHeight(float& stand_height)`，`:96-108` | 输出站立高度 | P1 / R0 |
| `GetPhase` | `int32_t GetPhase(std::vector<float>& phase)`，`:110-122` | **已废弃**；输出步态相位 | P2 / R0 |
| `GetMimicMotion` | `int32_t GetMimicMotion(std::string& data)`，`:298-301` | 查询可用模仿动作；要求机器人稳定站立 | P2 / R0 |

这些方法内部都用 `go2::JsonizeDataInt/Float` 解析响应；**G1 客户端复用了 GO2 的 JSON 包装类型**，不代表消息类型相同。

### 4.3 `LocoClient` 写 API

| API | 签名与位置 | 语义 | P / R |
|---|---|---|---|
| `SetFsmId` | `int32_t SetFsmId(int fsm_id)`，`:124-132` | 切 FSM；编号含义见 [05](05_g1_high_level_control.md) §3.1 | P2 / R3 |
| `SetBalanceMode` | `int32_t SetBalanceMode(int balance_mode)`，`:134-142` | 0=平衡站，1=连续步态（示例语义） | P2 / R3 |
| `SetSwingHeight` | `int32_t SetSwingHeight(float)`，`:144-152` | 摆腿高度 | P2 / R3 |
| `SetStandHeight` | `int32_t SetStandHeight(float)`，`:154-162` | 站立高度；单位/范围未在头文件声明 | P2 / R3 |
| `SetVelocity` | `int32_t SetVelocity(float vx, float vy, float omega, float duration = 1.f)`，`:164-174` | 速度命令；`duration` 秒；到期后是否自动停【未验证】 | P2 / R3 |
| `SetTaskId` | `int32_t SetTaskId(int task_id)`，`:176-184` | 触发任务（挥手/握手等） | P2 / R2 |
| `SwitchToUserCtrl` | `int32_t SwitchToUserCtrl()`，`:186-192` | 请求用户控制；**发送固定 `{"data":false}`**，参数语义存疑 | P2 / R3 |
| `SwitchToInternalCtrl` | `int32_t SwitchToInternalCtrl(InternalFsmMode mode)`，`:194-211` | 0=LAST、1=PASSIVE、2=WALKRUN | P2 / R3 |
| `Damp` | `Damp()`，`:214` | `SetFsmId(1)` | P2 / R3 |
| `Start` | `Start()`，`:216` | `SetFsmId(500)` | P2 / R3 |
| `Squat` | `Squat()`，`:218` | `SetFsmId(2)` | P2 / R3 |
| `Sit` | `Sit()`，`:220` | `SetFsmId(3)` | P2 / R3 |
| `StandUp` | `StandUp()`，`:222` | `SetFsmId(4)` | P2 / R3 |
| `ZeroTorque` | `ZeroTorque()`，`:224` | `SetFsmId(0)`；可能瞬间失力 | P2 / R3 |
| `StopMove` | `StopMove()`，`:226` | `SetVelocity(0,0,0)`（默认 1 s） | P2 / R3 |
| `HighStand` | `HighStand()`，`:228` | **缺陷**：把 `uint32_t` 最大值传给 `float` | P2 / R3 |
| `LowStand` | `LowStand()`，`:230` | **缺陷**：传 0（`uint32_t` 最小值） | P2 / R3 |
| `Move`（带 bool） | `Move(float vx, float vy, float vyaw, bool continous_move)`，`:232-234` | true 时 `duration=864000 s`（10 天） | P2 / R3 |
| `Move`（默认） | `Move(float, float, float)`，`:236` | 受本地 `continous_move_` 影响，默认 false | P2 / R3 |
| `BalanceStand` | `BalanceStand()`，`:238` | `SetBalanceMode(0)` | P2 / R3 |
| `ContinuousGait` | `ContinuousGait(bool flag)`，`:240` | `SetBalanceMode(flag?1:0)` | P2 / R3 |
| `SwitchMoveMode` | `SwitchMoveMode(bool flag)`，`:242-245` | **只改本地变量，不发 RPC，返回 0** | P2 / R3 |
| `WaveHand` | `WaveHand(bool turn_flag=false)`，`:247` | `SetTaskId(0/1)` | P2 / R2 |
| `ShakeHand` | `ShakeHand(int stage=-1)`，`:249-263` | 交替发送 task 2/3 | P2 / R2 |
| `SetSpeedMode` | `int32_t SetSpeedMode(int)`，`:265-273` | 速度档位 | P2 / R3 |
| `_fsm_api` | `int32_t _fsm_api(std::string parameter, std::string& data)`，`:303-305` | 直接访问 7100；下划线表示非稳定接口 | P2 / R3 |

### 4.4 `LocoClient` 已知缺陷（部署前必读）

| 问题 | 位置 | 影响 |
|---|---|---|
| `HighStand/LowStand` 数值类型不匹配 | `g1_loco_client.hpp:228-230` | 实际发送的 `float` 值不是“最高/最低”，可能是极大/0；不要调用 |
| `SwitchMoveMode` 纯本地 | `:242-245` | 误以为改变了机器人模式，实际只改客户端行为 |
| `SwitchToUserCtrl` 发 `{"data":false}` | `:186-192` | 服务端是否接受该参数【未验证】；不要把它当成“已获控制权” |
| `Move(..., true)` 持续 10 天 | `:232-234` | 没有看门狗就会一直保持速度命令 |
| `StopMove` 只发 1 s 零速 | `:226` | 1 s 后是否保持停止取决于服务端；不能替代急停 |

详细分析见 [05](05_g1_high_level_control.md) §9 与 [known_limitations.md](known_limitations.md)。

### 4.5 `G1ArmActionClient`

服务名 `arm`；底层基于 `rt/arm_sdk`。头文件：`g1_arm_action_client.hpp`。

| API | 签名与位置 | 语义 | P / R |
|---|---|---|---|
| `ExecuteAction`（预设） | `int32_t ExecuteAction(int32_t action_id)`，`:51-55` | 执行内置动作；99=释放 | P2 / R2 |
| `ExecuteAction`（自定义） | `int32_t ExecuteAction(const std::string& action_name)`，`:67-71` | 执行示教动作；**字符串直接拼接 JSON，未转义** | P2 / R2 |
| `StopCustomAction` | `int32_t StopCustomAction()`，`:74-77` | 停止自定义动作 | P2 / R2 |
| `GetActionList` | `int32_t GetActionList(std::string& data)`，`:79-82` | 查询动作列表（JSON 文本） | P1 / R0 |
| `action_map` | `std::map<std::string,int32_t>`，`:85-102` | 客户端侧预设名称→ID；不保证服务端支持 | P2 / R2 |

**动作 ID 映射中的明显问题**：`left kiss` 与 `right kiss` 都是 12（`:88-89`），动作名与 ID 不是一一对应。以 `GetActionList` 的服务端返回为准。

### 4.6 `AudioClient`

服务名 `voice`；头文件：`g1_audio_client.hpp`。

| API | 签名与位置 | 语义 | P / R |
|---|---|---|---|
| `TtsMaker` | `int32_t TtsMaker(const std::string& text, int32_t speaker_id)`，`:31-41` | 文本转语音；内部自增 `index`（非线程安全） | P2 / R1 |
| `GetVolume` | `int32_t GetVolume(uint8_t& volume)`，`:43-55` | 查询音量 | P2 / R1 |
| `SetVolume` | `int32_t SetVolume(uint8_t volume)`，`:57-66` | 设置音量 | P2 / R1 |
| `PlayStream` | `int32_t PlayStream(std::string app_name, std::string stream_id, std::vector<uint8_t> pcm_data)`，`:68-78` | 播放 PCM 流 | P2 / R1 |
| `PlayStop` | `int32_t PlayStop(std::string app_name)`，`:80-89` | **忽略 `Call` 返回值，永远返回 0** | P2 / R1 |
| `LedControl` | `int32_t LedControl(uint8_t R, uint8_t G, uint8_t B)`，`:91-101` | 控制 RGB LED | P2 / R1 |
| `ASR` | 常量 `ROBOT_API_ID_AUDIO_ASR=1002`，`g1_audio_api.hpp:18` | **没有对应客户端方法**；只能自己发 RPC 或订阅 `rt/audio_msg` | P2 / R1 |

### 4.7 `AgvClient`（G1D 底盘）

服务名 `agv`；头文件：`g1_agv_client.hpp`。

| API | 签名与位置 | 语义 | P / R |
|---|---|---|---|
| `Move` | `int32_t Move(float vx, float vy, float vyaw)`，`:56-67` | `vx∈[-1.5,1.5] m/s`；`vy` **被忽略**；`vyaw∈[-0.6,0.6] rad/s` | P2 / R3 |
| `HeightAdjust` | `int32_t HeightAdjust(float vz)`，`:93-102` | 归一化 `[-1,1]` → `±76.5 mm/s` | P2 / R3 |

> `Move` 注释说“非阻塞、发送后立即返回”，但没有停止保持/超时语义；连续调用需要自己控制节奏。

### 4.8 `MotionSwitcherClient`

命名空间是 `unitree::robot::b2`（不是 `g1`），但 G1 示例用它释放内置运控。头文件：`b2/motion_switcher/motion_switcher_client.hpp`；**全部为声明**。

| API | 签名与位置 | 语义 | P / R |
|---|---|---|---|
| `Init` | `void Init()`，`:21`（声明） | 初始化客户端 | P2 / R0 |
| `CheckMode` | `int32_t CheckMode(std::string& form, std::string& name)`，`:23`（声明） | 查询当前模式；【未验证】是否为纯读操作，以服务端实现为准 | P1 / R0 |
| `SelectMode` | `int32_t SelectMode(const std::string& nameOrAlias)`，`:24`（声明） | 切换模式（如 `"ai"`、`"normal"`）；会改变控制权 | P2 / R3 |
| `ReleaseMode` | `int32_t ReleaseMode()`，`:25`（声明） | 释放当前模式，让低层接管 | P2 / R3 |
| `SetSilent` | `int32_t SetSilent(bool silent)`，`:26`（声明） | 静默模式 | P2 / R3 |
| `GetSilent` | `int32_t GetSilent(bool& silent)`，`:27`（声明） | 查询静默状态 | P1 / R0 |

---

## 5. 订阅、低层通道与封装

### 5.1 G1 话题总表

> 类型名中的 `dds_` 命名空间不能省。方向是**应用视角**。

| 话题 | 方向 | 消息类型 | P / R | 证据 |
|---|---|---|---|---|
| `rt/lowstate` | 订阅 | `unitree_hg::msg::dds_::LowState_` | P0 / R0 | 常量 `g1_ankle_swing_example.cpp:20`；订阅 `:197-198` |
| `rt/sportmodestate` | 订阅 | `unitree_hg::msg::dds_::SportModeState_`【推断】 | P1 / R0 | `g1_arm_action_error.hpp:12-14` 注释 |
| `rt/secondary_imu` | 订阅 | `unitree_hg::msg::dds_::IMUState_` | P1 / R0 | 常量 `g1_ankle_swing_example.cpp:19`；订阅 `:199-200` |
| `rt/arm/action/state` | 订阅 | JSON 文本（类型未声明） | P1 / R0 | `g1_arm_action_client.hpp:17-22` |
| `rt/audio_msg` | 订阅 | `std_msgs::msg::dds_::String_` | P2 / R0 | `g1_audio_client_example.cpp:12` |
| `rt/lowcmd` | 发布 | `unitree_hg::msg::dds_::LowCmd_` | P2 / R3 | `g1_ankle_swing_example.cpp:18` |
| `rt/user_lowcmd` | 发布 | `unitree_hg::msg::dds_::LowCmd_` | P2 / R3 | `g1_userctrl_dds_example.cpp:13` |
| `rt/arm_sdk` | 发布 | `unitree_hg::msg::dds_::LowCmd_` | P2 / R2 | `g1_arm7_sdk_dds_example.cpp:11` |
| `rt/hand_sdk` | 发布 | `unitree_go::msg::dds_::MotorCmds_` | P2 / R2 | `g1_hand_sdk_example.cpp:27` |
| `rt/dex3/left/cmd` | 发布 | `unitree_hg::msg::dds_::HandCmd_` | P2 / R2 | `g1_dex3_example.cpp:32` |
| `rt/dex3/right/cmd` | 发布 | 同上 | P2 / R2 | `g1_dex3_example.cpp:269` |
| `rt/dex3/left/state` | 订阅 | `unitree_hg::msg::dds_::HandState_` | P2 / R0 | `g1_sub.h:97` |
| `rt/dex3/right/state` | 订阅 | 同上 | P2 / R0 | `g1_sub.h:105` |
| `rt/lf/dex3/left/state` | 订阅 | 同上 | P2 / R0 | `g1_dex3_example.cpp:253`（示例实际使用） |
| `rt/lf/dex3/right/state` | 订阅 | 同上 | P2 / R0 | `g1_dex3_example.cpp:257`（示例实际使用） |
| `rt/inspire/state` | 订阅 | `unitree_go::msg::dds_::MotorStates_` | P2 / R0 | `g1_sub.h:89`（无 G1 示例验证） |
| `rt/hispeed_state` | 订阅 | `geometry_msgs::msg::dds_::Point32_` | P2 / R0 | `g1d_height_control.cpp:18` |
| `rt/waistpitchcmd` | 发布 | `unitree_hg::msg::dds_::MotorCmd_` | P2 / R2 | `g1d_waist_example.cpp:12` |
| `rt/waistpitchstate` | 订阅 | `unitree_hg::msg::dds_::MotorState_` | P2 / R0 | `g1d_waist_example.cpp:13` |
| `rt/api/<service>/request` | 发布 | `unitree_api::msg::dds_::Request_` | P2 / R0 | `channel_namer.hpp:10-12` |
| `rt/api/<service>/response` | 订阅 | `unitree_api::msg::dds_::Response_` | P2 / R0 | 同上 |

**同名不同型**：`rt/lowstate` 在 G1 是 `unitree_hg::LowState_`，在 GO2 是 `unitree_go::LowState_`；`rt/sportmodestate` 同理。DDS 不会因同名而匹配不同类型。详见 [03](03_message_types.md) §3、[09](09_ros2_integration.md) §3。

### 5.2 低层发布前置条件（摘要）

| 通路 | 前置条件 | 必须自己实现的保护 | 详见 |
|---|---|---|---|
| `rt/lowcmd` | 释放内置运控（`motion_switcher`），FSM 允许 | 看门狗、CRC、关节限幅、掉线检测 | [06](06_g1_low_level_control.md) §2 |
| `rt/user_lowcmd` | FSM=PASSIVE + `SwitchToUserCtrl` | 同上 + 控制权确认 | [06](06_g1_low_level_control.md) §3 |
| `rt/arm_sdk` | 索引 29 的 `q` 作为权重 0→1 渐变 | 权重渐变、限幅、超时释放 | [06](06_g1_low_level_control.md) §4 |
| `rt/hand_sdk` | 无 | 力矩限幅、超时 | [06](06_g1_low_level_control.md) §5 |
| `rt/dex3/*/cmd` | 无 | 位置/速度限幅、超时 | [06](06_g1_low_level_control.md) §5.2 |
| `rt/waistpitchcmd` | G1D 配置 | 限幅、超时 | [06](06_g1_low_level_control.md) §6 |

> SDK **不提供**急停、碰撞检测、关节限位校验或命令超时。这些必须在应用层实现。

### 5.3 `dds_wrapper` 便捷封装

头文件：`unitree_sdk2/include/unitree/dds_wrapper/`。注意：这些封装会包含 Eigen（`g1_pub.h:6`、`g1_sub.h:6`），编译时需要 Eigen3 头文件。

#### 订阅封装

| 类 | 话题 / 消息 | 位置 |
|---|---|---|
| `subscription::LowState` | `rt/lowstate` / `LowState_` | `g1_sub.h:44-82` |
| `subscription::LowCmd` | `rt/lowcmd` / `LowCmd_` | `g1_sub.h:27-34` |
| `subscription::ArmSdk` | `rt/arm_sdk` / `LowCmd_` | `g1_sub.h:36-43` |
| `subscription::InspireHandState` | `rt/inspire/state` / `MotorStates_` | `g1_sub.h:84-90` |
| `subscription::Dex3LeftHandState` | `rt/dex3/left/state` / `HandState_` | `g1_sub.h:92-98` |
| `subscription::Dex3RightHandState` | `rt/dex3/right/state` / `HandState_` | `g1_sub.h:100-106` |

`SubscriptionBase` 公共接口（`common/Subscription.h`）：

| API | 位置 | 语义 |
|---|---|---|
| 构造 | `:28-43` | 创建 `ChannelSubscriber`；无 handler 时自动写入 `msg_` 并更新时间 |
| `set_timeout_ms` | `:45` | 默认 1000 ms |
| `isTimeout` | `:47-51` | 距最后更新是否超时 |
| `wait_for_connection` | `:53-67` | **死循环直到收到数据**；2 s 后开始打印等待提示；没有超时参数 |
| `msg_` / `mutex_` | `:69-70` | 公共成员；读取时必须自己加锁 |
| `pre/post_communication` | `:73-74` | 虚函数，在持锁状态下调用 |

`subscription::LowState` 额外提供 `update()`（`:51-73`）和 `isJoystickTimeout()`（`:75`，默认 3000 ms，`:79`）。`update()` 需要手动调用，不是自动回调。

#### 发布封装

| 类 | 话题 | 位置 | 说明 |
|---|---|---|---|
| `publisher::LowState` | `rt/lowstate` | `g1_pub.h:27-43` | 可注入摇杆数据并计算 CRC；通常不需要发布状态 |
| `publisher::LowCmd` | `rt/lowcmd` | `g1_pub.h:45-69` | `pre_communication` 自动算 CRC；`check_mode_machine` 对比 `mode_machine` |
| `publisher::ArmSdk` | `rt/arm_sdk` | `g1_pub.h:71-86` | `weight(float)` 写索引 29 的 `q`，自动 clamp 到 `[0,1]` |
| `PublisherBase` | 任意 | `common/Publisher.h:22-34` | 构造即 `InitChannel` |
| `RealTimePublisher` | 任意 | `common/Publisher.h:37-174` | 独立发布线程；`trylock/unlockAndPublish` 实时写，`lock/unlock` 非实时写 |

`RealTimePublisher` 接口：

| API | 位置 | 语义 |
|---|---|---|
| 构造（接收 publisher） | `:46-50` | 启动发布线程 |
| 构造（话题名） | `:52-54` | 内部创建 `PublisherBase` |
| 析构 | `:56-63` | `stop()` 后等待线程退出并 `join` |
| `stop` | `:65-68` | 置 `keep_running_=false` |
| `trylock` | `:77-90` | 实时侧尝试拿锁；只在 `turn_==REALTIME` 时成功 |
| `unlockAndPublish` | `:95-99` | 释放锁并允许发布线程发送 |
| `lock` / `unlock` | `:108-119` | 非实时侧拿锁/放锁，不发布 |
| `publishingLoop` | `:133-162` | 约 1 ms 轮询；复制 `msg_` 后 `Write(outgoing, 0)` |

### 5.4 工具与安全函数

| API | 位置 | 语义 | P |
|---|---|---|---|
| `crc32_core(uint32_t* ptr, uint32_t len)` | `dds_wrapper/common/crc.h:55-80` | Unitree CRC32（多项式 `0x04c11db7`），`len` 以 32-bit 字数计 | P0（低层） |
| `crc16_core(const uint8_t*, unsigned short)` | `crc.h:8-53` | CRC16，G1 低层命令不用 | P2 |
| `bad_orientation(lowstate, limit_angle=1.0)` | `g1/common/terminations.hpp:18-29` | 姿态偏离超过约 57.3° 返回 true | P1 |
| `joint_vel_out_of_limit(lowstate, limit_vel=10.0)` | `:32-38` | 任一关节速度绝对值超限 | P1 |
| `ang_vel_out_of_limit(lowstate, limit_vel=6.0)` | `:41-47` | 任一陀螺仪轴超限 | P1 |
| `motor_winding_overheat(lowstate, limit_temp=120.0)` | `:50-56` | 绕组温度超限 | P1 |
| `motor_casing_overheat(lowstate, limit_temp=85.0)` | `:59-65` | 外壳温度超限 | P1 |
| `low_battery(bms_state, limit_soc=20.0)` | `:68-71` | SOC 低于阈值 | P1 |
| `lost_connection(subscriber, timeout_ms=1000)` | `:81-86` | 距最后数据超过阈值；需要 `LowState_` 订阅者 | P1 |
| `CreateThread` / `CreateThreadEx` | `common/thread/thread.hpp:65-74` | 一次性线程 | P0 |
| `CreateRecurrentThread` / `CreateRecurrentThreadEx` | `common/thread/recurrent_thread.hpp:68-79` | 周期线程；`intervalMicrosec=0` 时忙循环 | P0 |
| `MicroSleep` / `MilliSleep` / `Sleep` | `common/time/sleep.hpp:10-12` | 睡眠工具 | P0 |
| `LOG_INFO/LOG_WARNING/LOG_ERROR` 等 | `common/log/log_decl.hpp:59-76` | 需要 `Logger` 实例，不是 printf 风格宏 | P0 |

> `terminations.hpp` 的函数只是**判据**，不会自动让机器人进入被动模式；返回 true 后必须由你的控制逻辑执行降级。`bad_orientation` 的默认 1.0 rad 约为 57°，是否适合你的场景需要自行标定。

---

## 6. 消息类型入口

| 类型 | 用途 | 详见 |
|---|---|---|
| `unitree_hg::msg::dds_::LowState_` | 关节、IMU、手柄、CRC | [03](03_message_types.md) §2.1 |
| `unitree_hg::msg::dds_::LowCmd_` | 低层命令 | [03](03_message_types.md) §2.2 |
| `unitree_hg::msg::dds_::MotorState_` / `MotorCmd_` | 单电机状态/命令 | [03](03_message_types.md) §2.3-2.4 |
| `unitree_hg::msg::dds_::IMUState_` | 副 IMU | [03](03_message_types.md) §2.5 |
| `unitree_hg::msg::dds_::SportModeState_` | 仅 4 字段 | [03](03_message_types.md) §2.6 |
| `unitree_hg::msg::dds_::HandState_` / `HandCmd_` | Dex3 手 | [03](03_message_types.md) §2.7 |
| `unitree_hg::msg::dds_::BmsState_` | 电池 | [03](03_message_types.md) §2.8 |
| `unitree_go::msg::dds_::MotorCmds_` / `MotorStates_` | Inspire 手 / 兼容 GO2 | [03](03_message_types.md) §3 |
| `unitree_api::msg::dds_::Request_` / `Response_` | RPC 载荷 | [02](02_dds_rpc_client_server.md) §3 |
| `std_msgs::msg::dds_::String_` | 音频事件文本 | [03](03_message_types.md) |
| `geometry_msgs::msg::dds_::Point32_` | G1D 高度状态 | [06](06_g1_low_level_control.md) §6 |

---

## 7. P0 / P1 / P2 汇总

### P0：首次部署必须掌握

| 类别 | API / 事实 | 文档 |
|---|---|---|
| 初始化 | `ChannelFactory::Instance()->Init(...)` | [01](01_channel_communication.md) §3.1、[12](12_network_and_deployment.md) §3 |
| 发送 | `ChannelPublisher<T>::InitChannel/Write/CloseChannel` | [01](01_channel_communication.md) §3.2 |
| 接收 | `ChannelSubscriber<T>::InitChannel/CloseChannel/GetLastDataAvailableTime` | [01](01_channel_communication.md) §3.3 |
| 线程模型 | `queuelen` 两套回调、`rlsnr`、队列丢最旧 | [08](08_concurrency_and_lifecycle.md) §2-3 |
| 生命周期 | `Release()` 顺序、回调在途、线程 join | [08](08_concurrency_and_lifecycle.md) §3.3、§6 |
| 周期线程 | `CreateRecurrentThreadEx` | [08](08_concurrency_and_lifecycle.md) §4 |
| 日志/退出 | `LOG_*`、`CommonException`、`UT_THROW` | [08](08_concurrency_and_lifecycle.md)、[README](README.md) |
| 构建 | CMake `find_package(unitree_sdk2)`、静态库 + DDS 共享库 | [11](11_build_and_link.md) |
| 网络 | domain、网卡、`"lo"` 默认陷阱、QoS 不可配 | [12](12_network_and_deployment.md) |

### P1：状态与集成

| 类别 | API / 话题 | 文档 |
|---|---|---|
| 状态订阅 | `rt/lowstate`、`rt/secondary_imu`、`rt/sportmodestate` | [04](04_robot_state.md) |
| 只读 RPC | `LocoClient::GetFsmId` 等、`MotionSwitcherClient::CheckMode` | §4.2、§4.8 |
| 安全判据 | `terminations.hpp` 全部函数 | [04](04_robot_state.md) §6 |
| IMU/手柄 | `IMUState_`、`LowState_.wireless_remote` | [04](04_robot_state.md) §2、[03](03_message_types.md) §2.1 |
| 异步/多线程 | `SubscriptionBase`、`RealTimePublisher` | §5.3、[08](08_concurrency_and_lifecycle.md) §2 |
| ROS 2 | 类型映射、桥接架构 | [09](09_ros2_integration.md) |
| 推理 I/O | 自建 `InferenceEngine` 接口、`LatestQueue` | [10](10_model_deployment_integration.md) |

### P2：运动/低层/非核心

| 类别 | API / 话题 | 风险 |
|---|---|---|
| 高层运动 | `LocoClient` 全部写方法 | R3 |
| 模式切换 | `MotionSwitcherClient::SelectMode/ReleaseMode` | R3 |
| 手臂 | `G1ArmActionClient`、`rt/arm_sdk` | R2 |
| 手部 | `rt/hand_sdk`、`rt/dex3/*` | R2 |
| 全身低层 | `rt/lowcmd`、`rt/user_lowcmd` | R3 |
| G1D | `AgvClient`、`rt/waistpitchcmd` | R3/R2 |
| 音频/LED | `AudioClient` | R1 |

---

## 8. 仅声明 / 未验证清单

以下接口只在头文件中看到声明，实现位于预编译静态库；**不要仅凭签名推断其行为**：

| API | 头文件 | 未确认点 |
|---|---|---|
| `ChannelFactory::Init` 三个重载 | `channel_factory.hpp:25-27` | 空配置默认值、失败是否抛异常、重复 Init 行为 |
| `ChannelFactory::Release` | `:29` | 是否等待回调结束、是否可重复调用 |
| `ClientBase::SetTimeout` / `Call` | `client_base.hpp:28-40` | 并发调用安全性、超时后迟到响应的处理；浮点单位已由反汇编确认（秒 × 1,000,000） |
| `Client` 全部方法 | `client.hpp:24-47` | 租约续期线程、版本不匹配处理 |
| `LeaseClient` | `lease_client.hpp:35-42` | 续约周期、失联行为 |
| `Server` / `ServerBase` | `server.hpp:26-70`、`server_base.hpp:17-23` | handler 线程模型、队列容量 |
| `MotionSwitcherClient` 全部方法 | `motion_switcher_client.hpp:18-27` | 模式名、失败语义、对控制权的即时影响 |
| `go2::VideoClient` / `b2::FrontVideoClient` / `b2::BackVideoClient` 的 `GetImageSample` | `go2/video/video_client.hpp:27`、`b2/front_video/front_video_client.hpp:27`、`b2/back_video/back_video_client.hpp:27` | **均不属于 G1**；图像编码/分辨率未知 |
| `ChannelSubscriber::CloseChannel` | `channel_subscriber.hpp:42-45` | 关闭后回调是否保证不再触发 |
| `RealTimePublisher` 析构 | `Publisher.h:56-63` | 线程退出延迟（最多约 100 ms + 1 ms 轮询） |

---

## 9. 错误码与返回值速查

| 返回值 | 含义 | 处理建议 |
|---|---|---|
| `0` | RPC 调用成功 | 不表示动作完成；继续查状态 |
| `-1` | 未初始化/通信失败（部分客户端注释如此写） | 检查 `ChannelFactory::Init`、网卡、服务名 |
| `3102`–`3107` | 请求/响应层错误 | 查服务是否存在、超时设置 |
| `3201`–`3207` | 服务端/租约错误 | 查租约、API 注册、服务端日志 |
| `7301` | `LocoState not available` | 状态未就绪；不要重试轰炸 |
| `7302` | `Invalid fsm id` | 查当前 FSM 与允许的切换 |
| `7303` | `Invalid task id` | 查动作 ID |
| `7400` | `rt/armsdk` 被占用 | 停止其他手臂控制源 |
| `7401` | 手臂 holding | 先发 99 或相同动作释放 |
| `7402` | 非法动作 ID | 用 `GetActionList` 校验 |
| `7404` | 非法 FSM | 动作只支持 FSM 500/501/801；801 下 mode 0/3 |
| `9101`–`9103` | AGV 未初始化/执行失败 | 检查 `AgvClient::Init` 与底盘状态 |
| `100` | 音频参数非法 | 检查音量/文本/流参数 |

> 错误码定义分散在各 `*_error.hpp`；`internal_error.hpp` 的具体映射见 [02](02_dds_rpc_client_server.md) §5.2。

---

## 10. 维护与复核

| 项 | 状态 |
|---|---|
| API 签名 | 已按当前 checkout 头文件核对 |
| 内联方法行为 | 已读源码确认 |
| 静态库实现 | **未验证** |
| 示例编译/运行 | 11 个 G1 目标编译通过，**未运行**（见 [13](13_examples_index.md) §9） |
| 机器人实测 | **未执行** |
| 行号漂移 | SDK 升级后必须重新核对 |

相关入口：[README](README.md)（证据等级/安全边界）、[01 通信](01_channel_communication.md)、[02 RPC](02_dds_rpc_client_server.md)、[03 消息](03_message_types.md)、[05 高层控制](05_g1_high_level_control.md)、[06 低层控制](06_g1_low_level_control.md)、[13 示例索引](13_examples_index.md)、[known_limitations](known_limitations.md)。
