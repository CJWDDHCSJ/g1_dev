# 宇树 G1-EDU 全硬件 × 全算法 端侧部署参考清单

> **硬件约束**：Jetson Orin NX 16GB（100 TOPS），与CPU共享统一内存
> **核心策略**：分层解耦 · 异步流水线 · 故障降级 · C++硬实时隔离


## 一、硬件清单（不可遗漏项）

G1-EDU的感知与控制硬件决定了算法栈的边界，以下为必须纳入架构设计的全部关键传感器与执行器：

| 硬件 | 型号/规格 | 算法角色 | 数据接口/频率 |
| :--- | :--- | :--- | :--- |
| **激光雷达** | Livox MID-360（3D，水平360°，垂直-7°~52°） | SLAM定位与建图、远距几何避障 | Ethernet / 点云~10-20Hz |
| **深度相机** | Intel RealSense D435i（RGB + 深度，内置IMU） | 近距物体检测、视觉SLAM辅助、VLM图像输入 | USB 3.0 / 30-60Hz |
| **麦克风阵列** | 4麦克风阵列 | 语音唤醒、ASR输入 | USB/I2S / 持续流 |
| **扬声器** | 5W | TTS音频输出 | I2S/模拟 / 按需 |
| **关节编码器** | **双编码器**（全部关节） | WBC本体感受反馈、VLA动作空间约束 | CAN FD（5 Mbps）或CAN（1 Mbps） |
| **IMU** | 内置（部分集成于RealSense/LiDAR） | 姿态估计、SLAM预积分、WBC平衡 | 串口/SPI / 100-1000Hz |
| **底层通信** | CAN FD / CAN 总线 | 关节扭矩/位置指令下发 | 1-5 Mbps，周期1ms级 |

> **关键认知**：SLAM不是“又一个AI模型”，而是**几何引擎**；WBC不是“VLA的一部分”，而是**独立的安全底线**。两者都必须与GPU上的AI模型做进程级隔离。


## 二、算法/模型选型矩阵（按控制频率分层）

| 层级 | 模块 | 推荐起点 | 备选/升级 | 运行频率 | 硬件位置 | 资源策略 |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **L0 底层控制** | **WBC全身控制** | **NVIDIA SONIC `gear_sonic_deploy`（C++/TensorRT）** | 宇树底层SDK低层模式 | **50 Hz**（关节指令） | CPU硬实时线程 | 独立进程，`SCHED_FIFO`，绑定实时核 |
| **L1 定位建图** | **SLAM** | **FAST-LIO2**（MID-360 + Orin NX已验证） | GLIO2（含GNSS，当前场景非必需） | **~10 Hz** | CPU/GPU混合 | 常驻，几何优化，与AI进程隔离 |
| **L2 高频感知** | **目标检测** | **RF-DETR Nano** 或 **YOLO26n**（均NMS-Free） | 视实测端到端延迟择优 | **30-60 Hz** | GPU（TensorRT） | 常驻，显存池复用 |
| **L2 高频感知** | **本体感受** | 关节编码器 + IMU原始数据 | — | **100-1000 Hz** | CPU/MCU | 直接供WBC消费，不经过AI层 |
| **L3 认知决策** | **VLM** | **Qwen2.5-VL-3B**（INT4/INT8） | 7B级别仅限极低频规划 | **< 1 Hz**（事件触发） | GPU | 严格限制上下文≤2K，按需加载 |
| **L4 动作生成** | **VLA** | **Octo（93M）** 或 **SmolVLA（450M）** | WVLA2.0（宣称Orin NX全onboard）/ PredActor（代码未发布） | **3-10 Hz** | GPU | 与VLM错峰，输出动作意图→L0 |
| **L5 语音输入** | **ASR** | **faster-whisper（small）** | GLM-ASR-Nano（1.5B，中文/方言更强） | 事件触发 | CPU优先 | 语音活动检测唤醒 |
| **L5 语音输出** | **TTS** | **Kokoro-82M**（ONNX流式） | Qwen3-TTS-0.6B（GPU，更高音质） | 事件触发 | CPU（GPU可选） | 环形缓冲区→ALSA流式播放 |


## 三、各模块部署详解

### 3.1 SLAM：FAST-LIO2（MID-360专用）

FAST-LIO2在Orin NX + Livox MID-360组合上已有充分的边缘部署验证，提供六自由度位姿和增量点云地图。GLIO2在MID-360上的Orin NX单帧耗时**23.28 ms**，FAST-LIO2更快，两者均满足实时预算。

**C++部署要点**：
- **ARM64编译陷阱**：Livox SDK2和ROS驱动需显式启用**NEON指令集**（`-DCMAKE_CXX_FLAGS="-march=armv8-a+simd"`），否则运行效率显著下降。
- **输出接口**：向C++调度器提供`/odom`（位姿）和`/cloud_registered`（配准点云），**只读共享**，VLA和路径规划器按需读取。
- **降级策略**：SLAM丢失定位时，系统自动切入“视觉避障 + 原地待命”模式，禁止盲目移动。

### 3.2 WBC：NVIDIA SONIC（让G1安全“站住”并“动起来”）

SONIC是NVIDIA为Unitree G1提供的全身控制栈，包含**三个已发布的控制器checkpoint**，部署使用**C++ + TensorRT**的`gear_sonic_deploy`进程。它以**50 Hz**频率通过DDS低层指令（`rt/lowcmd`）控制G1的29个关节。

**与VLA的集成链路**：
> VLA输出**motion_token + 手部关节** → `gear_sonic_deploy` → 50Hz关节指令 → CAN总线 → 电机

**WBC的硬性约束**：
- **独立进程**，与VLM/VLA的Python/C++推理进程完全隔离。
- **禁止**在WBC线程内做任何动态内存分配（`new/malloc`）或阻塞调用。
- 底层SDK的Python版本（`unitree_sdk2_python`）在G1 Orin NX上存在实时性缺陷，**C++ WBC栈是唯一可靠选择**。

### 3.3 VLA：Octo / SmolVLA 作为意图层

VLA**不直接输出关节扭矩**。它的职责是：接收当前图像 + VLM的语义指令 + SLAM位姿，输出**动作意图**（离散token或连续动作表征），频率3-10 Hz。

- **Octo（93M）** ：资源最省，适合作为“先跑通”的基线。
- **SmolVLA（450M）** ：能力更强，Orin NX可部署。
- **WVLA2.0**：宣称将边缘算力压缩至**100 TOPS以下**，完整运行于G1 EDU的Orin NX，无需云端依赖。可作为中后期评估项。
- **PredActor**：在G1 Orin NX上实现**50Hz关节状态-动作扩散**，p50延迟**16.79ms**，但代码截至2026-09尚未发布，仅作技术跟踪。

### 3.4 VLM：Qwen2.5-VL-3B（事件驱动“慢思考”）

Qwen2.5-VL-3B是Orin NX 16GB上**务实甜点**，已有专门的边缘部署项目采用**TensorRT（ViT）+ llama.cpp（LLM主干，Q4_K_M）** 混合链路并板上实测交付。在8GB Orin上实测图文识别速度可达**23 tok/s**。

**C++集成关键**：
- Qwen2.5-VL使用**M-RoPE**（多模态旋转位置编码），位置ID算错**不会报错，只会输出乱码**，必须使用经过验证的导出工具链。
- 严格限制**上下文≤2048 token**，图像分辨率按需降低，防止KV Cache膨胀挤占YOLO/FAST-LIO2的显存。
- 输出强制约束为**JSON**（如`{"action":"pick","target":"red_cup","reply":"好的，去拿红杯子"}`），由C++调度器解析后分发给VLA和Kokoro。

### 3.5 视觉检测：RF-DETR Nano / YOLO26n

两者均为**NMS-Free**，简化C++后处理。RF-DETR Nano精度更高（COCO 48.4 AP），有Orin NX + DeepStream部署指南；YOLO26n生态最成熟。建议在真实场景视频流上**实测P95端到端延迟**后择优，不只看COCO指标。

### 3.6 ASR 与 TTS

**ASR（faster-whisper small）** ：CPU推理，语音活动检测触发，仅在有人说话时唤醒，不常驻GPU。

**TTS（Kokoro-82M）** ：ONNX Runtime流式推理，在Orin上合成**3.64秒中文音频仅需264ms**，合成速度远超实时。部署要点：
- 输出`float32` → `int16`转换（×32767 + Clamp）。
- 通过**无锁环形缓冲区**将音频块推送给ALSA，实现“边生成边播放”。
- CPU侧运行，与GPU上的VLM/VLA负载完全错开。


## 四、C++ 工程 Checklist（硬实时与故障安全）

### 4.1 进程隔离（不可妥协）

| 进程 | 包含模块 | 调度策略 | 故障后果 |
| :--- | :--- | :--- | :--- |
| **P0（硬实时）** | WBC（SONIC `gear_sonic_deploy`） + CAN通信 | `SCHED_FIFO`，独立核 | 崩溃 = 机器人摔倒，**最高优先级** |
| **P1（高吞吐）** | FAST-LIO2 + YOLO/RF-DETR | 普通实时调度 | 崩溃 = 降级为原地待命 |
| **P2（认知）** | VLM + VLA + ASR + TTS | 普通调度，最低优先级 | 崩溃 = 切断复杂交互，底盘/避障继续 |

**P0必须永远假设P2会死。** P2的段错误（Segfault）或OOM绝不能波及P0。

### 4.2 队列策略

- **相机 → YOLO**：SPSC无锁环形队列，**Drop Oldest**（满时丢弃最老帧，只处理最新画面）。
- **VLA意图 → WBC**：带**超时看门狗**的指令队列。VLA超过**200ms**未推送新意图，WBC自动执行“减速刹车/原地站立”。
- **VLM文本 → Kokoro**：无锁队列，音频块即时消费，**永不积压**。

### 4.3 内存与显存

- **全局显存池**：启动时预分配，YOLO/VLM/VLA的TensorRT引擎和KV Cache全部从池中切分，杜绝运行时`cudaMalloc`。
- **SLAM点云缓冲区**：预分配固定大小，FAST-LIO2输出直接写入，避免高频`new/delete`。
- **WBC零分配**：`gear_sonic_deploy`的控制循环内**禁止任何动态内存操作**。

### 4.4 故障降级矩阵

| 故障 | 检测方式 | 降级动作 |
| :--- | :--- | :--- |
| SLAM丢失定位 | `/odom`超时或协方差超阈值 | 切入“视觉避障 + 原地待命”，禁止移动 |
| VLA无输出 | 意图队列超时（>200ms） | WBC执行“站立保持”，等待恢复 |
| VLM OOM/崩溃 | 进程心跳丢失 | 切断VLM，保留YOLO避障 + Kokoro基础播报 |
| 相机断流 | 帧队列持续为空 | YOLO挂起，SLAM降级为纯LiDAR模式 |


## 五、实施演进路线

| 阶段 | 目标 | 关键验证项 |
| :--- | :--- | :--- |
| **阶段一：稳定站立与移动** | FAST-LIO2 + SONIC WBC 跑通 | 建图→定位→路径规划→WBC执行，P95端到端延迟；SLAM丢失时的安全降级 |
| **阶段二：高频视觉感知** | RF-DETR/YOLO26n TensorRT C++推理 | 真实场景P95延迟、与SLAM的CPU/GPU共存开销 |
| **阶段三：语音闭环** | ASR + VLM + Kokoro | 语音唤醒→VLM JSON→Kokoro流式播报，全链路延迟 |
| **阶段四：动作生成** | Octo/SmolVLA → SONIC | VLA意图→WBC 50Hz执行，超时看门狗触发测试 |
| **阶段五：极限调优** | 显存池、无锁队列、线程绑核 | 多模型并发下的长时稳定性（数小时），热降频监控 |


## 六、与上一版清单的核心修正说明

| 修正项 | 上一版缺失 | 本版补充 |
| :--- | :--- | :--- |
| **SLAM** | 完全遗漏 | FAST-LIO2（MID-360专用），SLAM是移动的几何前提 |
| **WBC** | 提及SONIC但未作为独立硬实时层 | SONIC `gear_sonic_deploy` 作为P0进程，50Hz，与AI进程隔离 |
| **关节编码器/IMU** | 未纳入 | 作为WBC本体感受输入，不经过AI层直接消费 |
| **CAN通信** | 未提及 | CAN FD/CAN作为P0进程的输出通道，周期1ms级 |
| **故障降级** | 有原则但未成表 | 新增故障降级矩阵，每类故障对应具体动作 |
| **VLM部署工具链** | 未说明M-RoPE陷阱 | 明确Qwen2.5-VL的M-RoPE兼容性要求与已验证的混合引擎方案 |


**一句话总结**：G1-EDU的“自由活动”不是某个大模型单独完成的，而是 **SONIC WBC（50Hz保命） + FAST-LIO2（10Hz定位） + YOLO/RF-DETR（60Hz看路） + VLA（5Hz意图） + VLM（1Hz思考） + Kokoro（事件播报）** 在C++进程隔离与故障降级框架下的协同结果。**P0（WBC）的确定性，优先于一切AI模型的性能。**