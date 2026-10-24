我回顾了一下之前在 Unitree G1-EDU 机器人本体上核查的环境。你去查文档时，重点盯住 TensorRT 8.5.2、CUDA 11.4 和 JetPack 5.1.1 这一套，别一不小心拿最新版文档往旧环境里套，版本兼容这东西可不会因为文档写得漂亮就自动解决。

## 1. G1 本体端已确认的版本

| 组件                 | G1 当前环境                  | 状态      |
| ------------------ | ------------------------ | ------- |
| 硬件                 | NVIDIA Jetson Orin NX    | 已确认     |
| 架构                 | ARM64 / `aarch64`        | 已确认     |
| 操作系统               | Ubuntu 20.04.6 LTS       | 已确认     |
| JetPack            | 5.1.1                    | 已确认     |
| Jetson Linux / L4T | R35.3.1                  | 已确认     |
| CUDA               | 11.4.315                 | 已确认     |
| TensorRT           | 8.5.2                    | 已确认     |
| cuDNN              | 8.x，系统存在 `libcudnn.so.8` | 主版本已确认  |
| Python             | 3.8.10                   | 已确认     |
| GCC                | 9.4.0                    | 已确认     |
| CMake              | 3.16.3                   | 已确认     |
| ROS 2              | Foxy                     | 已确认     |
| OpenCV             | 4.2.0                    | 之前核查已确认 |
| ONNX Runtime       | 未安装 / 尚无可用运行时            | 已核查     |
| pip                | 20.0.2                   | 已确认     |
| NumPy              | 1.24.4                   | 已确认     |

补充的 TensorRT 信息：

- `trtexec`：`/usr/src/tensorrt/bin/trtexec`
- ONNX 解析库：`libnvonnxparser.so.8` 存在
- TensorRT 库：`libnvinfer.so.8` 存在
- `nvidia-smi` 不可用，这是 Jetson 环境，不代表 GPU 不存在。

以上是此前检查记录，不代表我刚刚重新连接机器人核验过当前状态。

## 2. 你查官方文档时，重点看这些

TensorRT

当前版本：8.5.2

重点查 ONNX Parser、支持的算子、动态输入形状、FP16、Engine 序列化与反序列化，以及目标硬件上的 Engine 兼容性。

[TensorRT 官方归档文档](https://docs.nvidia.com/deeplearning/tensorrt/archives/?utm_source=chatgpt.com)

JetPack / CUDA / cuDNN

JetPack 5.1.1 · CUDA 11.4 · cuDNN 8.x

查 JetPack 5.1.1 对应的组件版本和安装包兼容性。不要直接安装为 CUDA 12/13 或新版 JetPack 设计的包。

[JetPack 官方归档](https://developer.nvidia.com/embedded/jetpack-archive?utm_source=chatgpt.com)

ONNX Runtime

当前未安装

重点看 ARM64 Linux 的 C++ API、CUDA Execution Provider 与 TensorRT Execution Provider 的依赖要求，以及对应 CUDA/cuDNN 版本。最新版预编译 GPU 包不一定适用于你的系统。

[ONNX Runtime 官方文档](https://onnxruntime.ai/docs/?utm_source=chatgpt.com) · [Execution Providers 文档](https://onnxruntime.ai/docs/execution-providers/?utm_source=chatgpt.com)

ROS 2

机器人本体：Foxy

查 Foxy 下 C++ 节点、消息接口和 SDK 集成方式。机器人端不是 Humble；开发机和机器人本体的 ROS 版本不能混为一谈。

[ROS 2 Foxy 官方文档](https://docs.ros.org/en/foxy/?utm_source=chatgpt.com)

## 3. 结合你现在的部署项目，怎么理解这几套技术

- 目标检测主链路： ONNX → TensorRT 8.5.2 → C++ 推理 → ROS 2 Foxy。
- ONNX Runtime： 是另一套推理运行时。目前机器人上还没有确认可用的 ONNX Runtime，所以不要把它当作已经安装好的依赖。
- ASR： 之前部署过 sherpa-onnx / ONNX Runtime 相关方案，但在 G1 上的 CUDA/TensorRT 路径遇到过内存与性能问题；不能据此认定 ONNX Runtime 已全局安装。
- 原生 TTS： 现在已通过 `AudioClient::TtsMaker()` 成功调用，返回码为 `0`，但机器人实际没有听到播报。这个问题与 TensorRT、ONNX Runtime 版本无关，不要把两条链路混在一起排查。

最重要的版本基准：JetPack 5.1.1 + L4T R35.3.1 + CUDA 11.4 + TensorRT 8.5.2 + ARM64。 你找到文档后，按这套版本核对，而不是只看“最新版支持什么”。