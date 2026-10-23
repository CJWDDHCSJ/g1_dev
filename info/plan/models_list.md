# 宇树 G1-EDU (Orin NX 16GB) 具身智能端侧部署架构清单

> **核心设计理念**：面对 16GB 统一内存与 100 TOPS 算力约束，放弃“全能大模型”幻想。
> 采用 **“分层解耦 + 异步流水线 + 极致量化”** 策略，用 C++ 榨干硬件性能。

---

## 一、 核心模型选型矩阵

| 模块定位 | 推荐起点 (最务实) | 备选 / 升级项 | 运行频率 | 资源策略 |
| :--- | :--- | :--- | :--- | :--- |
| **👂 语音输入 (ASR)** | `faster-whisper (small)` | `GLM-ASR-Nano (1.5B)` | 事件触发 | CPU 优先，脉冲式 GPU 唤醒 |
| **👁️ 视觉感知 (检测)** | `RF-DETR Nano` 或 `YOLO26n` | 视实测端到端延迟与精度择优 | **30 - 60 Hz** | 常驻 GPU，转 TensorRT，**原生无 NMS** |
| **🧠 认知决策 (VLM)** | `Qwen2.5-VL-3B` (INT4/INT8) | 7B 级别 (仅限极低频规划，风险高) | 低频触发 (< 1 Hz) | GPU，严格限制上下文与分辨率 |
| **🤖 动作规划 (VLA)** | `Octo (93M)` / `SmolVLA (450M)` | `EdgeVLA-Small (228M)` | 3 - 10 Hz | GPU，与 VLM 错峰调度，输出动作意图 |
| **🦵 底层控制 (WBC)** | **`NVIDIA SONIC` (C++ 栈)** | 宇树官方底层控制器 | **50 - 1000 Hz** | **独立 CPU 硬实时线程**，保障动态平衡 |
| **🗣️ 语音输出 (TTS)** | **`Kokoro-82M`** | `Qwen3-TTS-0.6B` (GPU流式) | 事件触发 | CPU 侧运行，零 GPU 占用，流式推流 |

---

## 二、 各模块详细部署指南

### 1. 👁️ 视觉感知：高频肌肉记忆
*   **模型本质**：DETR 范式（RF-DETR）或 原生 NMS-Free CNN（YOLO26n）。
*   **部署路径**：
    *   在 x86 机器上执行 `PT -> ONNX` 导出（固定输入尺寸，如 384x384 或 640x640）。
    *   在 G1 端使用 `trtexec` 或 C++ 代码生成 **TensorRT Engine (FP16)**。
*   **C++ 后处理**：取消传统 NMS 逻辑，直接解析模型输出的类别概率和坐标框。
*   **工程库参考**：`infracv/rf-detr-cpp`（提供生产级 TensorRT C++ 实现）。

### 2. 🧠 认知决策：低频大脑皮层
*   **选型红线**：死守 **3B 参数量** 红线。7B 模型会吃光内存，导致其他模块 OOM。
*   **部署路径**：
    *   使用 INT4 或 INT8 量化（AWQ/GPTQ）。
    *   利用 TensorRT-LLM 或 ONNX Runtime 加速推理。
*   **调度策略**：平时休眠。当 YOLO 发现未知物体，或用户发出语音指令时，唤醒 VLM 分析当前帧并生成 JSON 格式的决策（如：`{"action": "pick", "target": "red_cup"}`）。

### 3. 🤖 动作生成与底层控制：意图与反射的分离
*   **VLA (意图层)**：接收图像和 VLM 指令，输出“动作表征”（Latent Tokens 或连续动作向量），频率约 3-10 Hz。
*   **WBC (执行层)**：**绝对不能用大模型做底层控制！** 必须使用 **NVIDIA SONIC C++ 栈**，以 50Hz+ 的频率，接收 VLA 的意图，结合电机反馈，计算出各关节扭矩，保障 G1 站立不摔倒。
*   **通信链路**：VLA (Python/C++) `->` ZMQ/共享内存 `->` SONIC (C++)。

### 4. 🗣️ 语音交互：极速响应
*   **ASR 输入**：`faster-whisper` 走 CPU 即可。检测到人声时启动，转成文本推给 VLM。
*   **TTS 输出**：
    *   将 VLM 输出的文本通过无锁环形队列（Ring Buffer）传给 Kokoro。
    *   **流式输出**：Kokoro 生成一块音频数据，立刻转换为 `int16`，推入 ALSA 音频回调，实现“边生成边播报”。

---

## 三、 C++ 榨干硬件性能 Checklist

在 G1 上写 C++ 调度代码时，必须落实以下优化：

- [ ] **内存池 (Memory Pool)**：程序启动时预分配大块统一内存/显存，后续所有模型的输入输出张量全部复用，杜绝高频 `cudaMalloc` 和 `malloc`。
- [ ] **零拷贝 (Zero-Copy)**：使用 ONNX Runtime/TensorRT 的 API 时，通过 `Ort::MemoryInfo` 或指针，直接读写指定内存池地址，避免跨库隐式拷贝。
- [ ] **无锁环形队列 (Lock-Free Ring Buffer)**：用于多线程数据传递（如：相机线程 -> YOLO 线程，VLM 线程 -> TTS 线程）。杜绝 `std::mutex` 带来的线程挂起。
- [ ] **线程亲和性 (Thread Affinity)**：使用 `pthread_setaffinity_np`，将 WBC 控制线程绑定到独立 CPU 核心，给 YOLO 线程设置实时优先级（`SCHED_FIFO`）。
- [ ] **ONNX -> Engine 跨机转换**：绝不在 Orin NX 上做模型导出！在带 N 卡的 x86 机器上完成转换，只把 `.engine` 二进制文件拷贝到 G1 上。
- [ ] **音频格式硬转换**：模型输出的 `float32` 必须乘以 32767 并 Clamp 限幅，再转为 `int16` 送给 ALSA，防止爆音。

---

## 四、 实施演进路线图

1.  **阶段一：单点突破 (Vision & Audio)**
    *   跑通 RF-DETR/YOLO26n 的 TensorRT C++ 推理。
    *   跑通 Kokoro 的 C++ ONNX 流式音频生成与播放。
2.  **阶段二：大脑接入 (VLM + 总线调度)**
    *   部署 Qwen2.5-VL-3B，实现“看图说话”。
    *   编写 C++ 调度器，打通 `VLM 文本 -> JSON 解析 -> 唤醒 Kokoro 播报` 的链路。
3.  **阶段三：闭环动作 (VLA + WBC)**
    *   引入 Octo/SmolVLA 生成动作意图。
    *   接入 NVIDIA SONIC 栈，实现 VLA 指令驱动机器人行走与抓取。
4.  **阶段四：极限调优 (Performance Tuning)**
    *   部署内存池、零拷贝、线程绑核。
    *   压测多模型并发时的 P95 端到端延迟。