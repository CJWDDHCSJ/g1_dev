# 04 TensorRT 与 GPU 推理

本文记录从 ONNX 到 TensorRT Engine、Python/C++ 执行推理的实际路径和已确认的 Tensor 信息。

## 1. 设备和制品

| 项目 | 值 | 状态 |
|---|---|---|
| GPU | NVIDIA Orin，Compute Capability 8.7，8 SM | **已验证成功**：`trtexec` |
| GPU 全局显存 | 15388 MiB | **已验证成功** |
| CUDA | 11.4.315 | **已验证成功** |
| TensorRT | 8.5.2，`trtexec` 打印 `v8502` | **已验证成功** |
| ONNX | `/home/unitree/robot-vision/models/custom/best.onnx` | **已验证成功** |
| Engine | `/home/unitree/robot-vision/engines/best_fp32.engine` | **已验证成功** |
| Engine 精度 | FP32（构建日志明确） | **已验证成功** |
| ONNX Opset | 18 | **已验证成功** |
| ONNX producer | PyTorch 2.14.1 | **已验证成功** |
| Engine 文件大小 | 约 12 MiB | **已验证成功** |

## 2. ONNX 生成

开发机上使用 Ultralytics：

```bash
cd /home/mick/robot-vision
yolo export model=models/custom/best.pt format=onnx imgsz=640
```

导出结果：

```text
Model summary (fused): 73 layers, 3,006,428 parameters, 8.1 GFLOPs
PyTorch: input (1, 3, 640, 640) BCHW
Outputs: (1, 8, 8400)
ONNX: opset 18
ONNX size: 11.7 MB
```

导出过程的设备显示为 CPU；这是导出进程的运行标识，不代表 ONNX 或 TensorRT 只能在 CPU 上运行。

## 3. Engine 构建

实际执行位置：G1 机身。

```bash
/usr/src/tensorrt/bin/trtexec \
  --onnx="$HOME/robot-vision/models/custom/best.onnx" \
  --saveEngine="$HOME/robot-vision/engines/best_fp32.engine" \
  --workspace=1024 \
  --buildOnly
```

| 参数 | 含义 | 备注 |
|---|---|---|
| `--onnx` | 输入 ONNX 路径 | 实际为 11.7 MB 文件 |
| `--saveEngine` | 输出序列化 Engine | 实际生成约 12 MiB 文件 |
| `--workspace=1024` | 构建器内存池 1024 MiB | 该版本提示已弃用，建议 `--memPoolSize`；此次仍然生效 |
| `--buildOnly` | 只构建，不跑 Inference 阶段 | 成功输出 `PASSED` |

构建结果：

```text
Selected Device: Orin
Compute Capability: 8.7
SMs: 8
Device Global Memory: 15388 MiB
TensorRT version: 8.5.2
ONNX Opset version: 18
Engine built in 170.956 sec.
Loaded engine size: 12 MiB
Engine deserialized in 0.0422227 sec.
&&&& PASSED
```

本阶段确认的是“ONNX 能被 TensorRT 8.5.2 解析，Engine 能生成并反序列化”，不等于检测结果正确。

### 3.1 构建警告

| 警告 | 实际含义 | 是否阻止构建 |
|---|---|---|
| ONNX 含 INT64 weights，TensorRT 不原生支持 INT64，尝试转为 INT32 | 部分索引/常量被降为 INT32 | **不影响**：随后成功生成 Engine |
| Some tactics do not have sufficient workspace memory | 1024 MiB workspace 下部分 tactic 不适用，仍有其他 tactic 可用 | **不影响**：最终 `PASSED` |
| `--workspace` deprecated | 版本建议新参数，旧参数仍工作 | **不影响** |

如果要调整 workspace，应先保留当前 Engine 和日志，再单独做构建对照，不能为了消除警告覆盖已验证制品。

## 4. Tensor 规格

| Tensor | 名称 | 类型 | 形状 | 元素数 | 字节数（FP32） |
|---|---|---|---:|---:|---:|
| 输入 | `images` | `float32` | `(1, 3, 640, 640)` | 1,228,800 | 4,915,200 |
| 输出 | `output0` | `float32` | `(1, 8, 8400)` | 67,200 | 268,800 |

元素数和字节数是由确认形状按 `N × C × H × W` 或 `N × C × A` 计算得到。Python 烟雾测试实际打印了 `float32`。

输出布局：

```text
index 0: cx
index 1: cy
index 2: width
index 3: height
index 4: class 0
index 5: class 1
index 6: class 2
index 7: class 3
```

共有 4 个类别分数，没有额外 objectness。模型类别名称为：

```cpp
{"bingren", "wangqiu", "zhentou", "shubiao"}
```

这个解释由后续 C++ 实现和对话中的类别表明确给出。早期仅凭 `(1,8,8400)` 推断“7 类”的说法已被后续证据修正。

## 5. C++ TensorRT API 流程

历史中已验证的 `robot_detector_cpp` 使用 TensorRT 8.5 的地址式 API：

```cpp
std::vector<char> data = loadEngineFile(ENGINE_PATH);

nvinfer1::IRuntime* runtime =
    nvinfer1::createInferRuntime(logger);

nvinfer1::ICudaEngine* engine =
    runtime->deserializeCudaEngine(data.data(), data.size());

nvinfer1::IExecutionContext* context =
    engine->createExecutionContext();

context->setTensorAddress("images", device_input);
context->setTensorAddress("output0", device_output);
cudaMemcpy(device_input, host_input.data(), input_bytes,
           cudaMemcpyHostToDevice);

bool ok = context->enqueueV3(nullptr);
cudaDeviceSynchronize();

cudaMemcpy(host_output.data(), device_output, output_bytes,
           cudaMemcpyDeviceToHost);
```

项目实际使用 RAII 包装 `IRuntime`、`ICudaEngine`、`IExecutionContext`，并在异常路径上释放 CUDA 内存。真实程序中 `enqueueV3(nullptr)` 返回成功，C++ 推理日志出现 `Inference completed`。

| API | 头文件/库 | 用途 | 关键参数/返回值 | 项目中使用位置 |
|---|---|---|---|---|
| `nvinfer1::createInferRuntime` | `NvInfer.h` / `nvinfer` | 创建反序列化运行时 | 传 `ILogger&`；返回 `IRuntime*` | `robot_detector_cpp/main.cpp` |
| `IRuntime::deserializeCudaEngine` | `NvInfer.h` | 从文件字节加载 Engine | 字节指针和长度；返回 `ICudaEngine*` | 同上 |
| `ICudaEngine::createExecutionContext` | `NvInfer.h` | 创建执行上下文 | 返回 `IExecutionContext*` | 同上 |
| `ICudaEngine::getTensorIOMode` | `NvInfer.h` | 检查 `images` 是输入、`output0` 是输出 | 返回 `TensorIOMode` | 同上 |
| `IExecutionContext::setTensorAddress` | `NvInfer.h` | 绑定输入/输出 GPU 地址 | tensor 名、`void*` | 同上 |
| `IExecutionContext::enqueueV3` | `NvInfer.h` | 异步排队推理 | CUDA stream，可为 `nullptr` | 同上 |
| `cudaMalloc` | `cuda_runtime_api.h` / `cudart` | 分配 GPU 输入/输出缓冲 | `void**`、字节数 | 同上 |
| `cudaMemcpy` | CUDA Runtime | H2D/D2H 数据复制 | 目标、源、字节数、方向 | 同上 |
| `cudaDeviceSynchronize` | CUDA Runtime | 等待异步推理完成 | 无参数；返回 `cudaError_t` | 同上 |
| `cudaFree` | CUDA Runtime | 释放 GPU 缓冲 | 设备指针 | 成功/异常路径 |

## 6. Python TensorRT 烟雾测试

脚本路径：

```text
/home/unitree/robot-vision/scripts/trt_smoke_test.py
```

该脚本用 `ctypes.util.find_library("cudart")` 加载 CUDA Runtime，并用 NumPy 生成随机输入：

```python
rng = np.random.default_rng(42)
host_input = rng.random(input_shape).astype(np.float32)
```

关键调用：

```python
engine = trt.Runtime(logger).deserialize_cuda_engine(engine_data)
context = engine.create_execution_context()

input_index = engine.get_binding_index("images")
output_index = engine.get_binding_index("output0")

bindings = [0] * engine.num_bindings
bindings[input_index] = device_input.value
bindings[output_index] = device_output.value

success = context.execute_v2(bindings)
cuda.cudaDeviceSynchronize()
```

实际输出：

```text
输入形状: (1, 3, 640, 640)
输出形状: (1, 8, 8400)
GPU 显存分配成功
输入数据复制成功
TensorRT 推理执行成功
输出数据读取成功
输出数据类型: float32
输出全部为有限数值: True
输出最小值: 1.2546777725219727e-05
输出最大值: 658.754150390625
各类别最大候选分数:
[0.007582247257232666, 0.0017976760864257812,
 0.21617966890335083, 0.0018575787544250488]
TensorRT 随机输入推理测试通过
```

Status：

- **已验证成功**：Engine 加载、显存分配、H2D、推理、D2H、输出可读。
- **不能据此判断检测正确**：输入是随机张量，没有图像语义。

日志里的 `DeprecationWarning: Use get_tensor_name instead` 和 `get_tensor_shape` 属于旧 binding API 提示，不阻止执行。

## 7. `trtexec` 性能测试

曾执行：

```bash
/usr/src/tensorrt/bin/trtexec \
  --loadEngine="$HOME/robot-vision/engines/best_fp32.engine" \
  --warmUp=500 \
  --duration=5
```

对话明确写到这次 Engine 推理性能测试通过；后续用户又报告锁频和 Spin Wait 后测试结果更好。

但是共享页面没有保留以下具体数字：

- Throughput（queries/s 或 fps）；
- GPU Compute Time 的中位数、P90/P95/P99；
- End-to-End Latency；
- 锁频方式、频率值、开启 Spin Wait 的完整命令；
- 两轮测试的原始日志。

因此：

- **已验证成功**：`trtexec` 加载 Engine 并执行了推理；锁频和 Spin Wait 测试确实尝试过。
- **已观察到**：用户称锁频+Spin Wait 后结果更好。
- **待验证**：改进百分比和最终性能数字。

不能把 `5.2 ms` 的开发机 PyTorch/Ultralytics 推理时间当成 G1 的 Engine 延迟。

## 8. 已验证/未验证边界

| 命题 | 状态 |
|---|---|
| ONNX 导出成功 | **已验证成功** |
| G1 TensorRT 能解析 Opset 18 模型 | **已验证成功** |
| FP32 Engine 构建成功 | **已验证成功** |
| Engine 可反序列化 | **已验证成功** |
| Python 随机输入完整推理 | **已验证成功** |
| C++ 真实相机输入可执行 TensorRT 推理 | **已验证成功** |
| Engine 对真实图像输出的检测结果正确 | **待验证**：实时无框，同图对照未完成 |
| 锁频+Spin Wait 的具体性能数字 | **待验证** |
| Engine 可跨 CUDA/TensorRT/设备直接复用 | **不成立为前提**：应按目标设备构建 |

