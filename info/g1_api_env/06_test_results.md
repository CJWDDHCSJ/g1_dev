# 06 已完成测试与结果

本文只记录共享对话中有命令、日志或明确测试摘要支持的结果。“程序跑完”“推理执行成功”“检测结果正确”分开判断。

## 1. 测试一览

| 编号 | 测试 | 执行设备 | 结果 | 能得出的结论 | 不能得出的结论 |
|---|---|---|---|---|---|
| T01 | ROS 2 Topic/Node 环境检查 | G1 | 可列出话题；`/frontvideostream` 可发现 | ROS 2 图和 DDS 端点存在 | 本机拥有可解析消息包或新节点一定可创建 |
| T02 | CUDA/TensorRT 工具检查 | G1 | `trtexec` 打印 `v8502`，随后因无模型退出 | TensorRT 工具可启动 | 当时已成功推理 |
| T03 | 自定义 `.pt → .onnx` 导出 | 开发机 | `PASSED`，11.7 MB，Opset 18 | ONNX 文件已生成 | ONNX 推理和 TensorRT 结果正确 |
| T04 | TensorRT FP32 Engine 构建 | G1 | `PASSED`，约 171 s，12 MiB | Engine 可构建和反序列化 | 真实图像检测准确 |
| T05 | Python TensorRT 随机输入 | G1 | 显存、H2D、推理、D2H 全成功 | Engine 执行链路可用 | 模型准确率或真实图片输出正确 |
| T06 | Python 官方 YOLO 图片检测 | 开发机 RTX 4070 上下文 | 检测到 person，置信度 0.881 | 开发机基础 PyTorch/Ultralytics 链路可用 | 自定义模型或 G1 Engine 正确 |
| T07 | Python 自定义模型图片检测 | 开发机 | 检测到 `bingren`，约 0.9780 | 自定义 `.pt` 对固定图片可产生合理检测 | C++/TensorRT 输出与之一致 |
| T08 | Unitree SDK `GetImageSample` | G1 | JPEG 字节返回，`imdecode` 成功 | 实际相机图像获取链路可用 | `/frontvideostream` DDS 可订阅 |
| T09 | 原生 Cyclone DDS Reader | G1 | Reader 创建成功，0 帧 | 程序能建 DDS 实体 | 话题映射/QoS/类型已经匹配 |
| T10 | C++ 相机 + TensorRT | G1 | 全流程走完，3 次最终框数均为 0 | 单帧程序、CUDA 和 Engine 可执行 | 相机画面中有可检测目标或后处理正确 |
| T11 | `trtexec` Engine 推理性能 | G1 | 报告性能测试通过 | Engine 能执行性能循环 | 具体吞吐/延迟数字未保存 |
| T12 | 锁频 + Spin Wait | G1 | 用户报告“更好了” | 做过优化对照 | 精确提升幅度和完整测试条件未知 |
| T13 | C++ 与 Python 同图对照 | G1/开发机 | 未看到执行输出 | 无 | 不能判断 C++/Python 是否一致 |
| T14 | C++ TensorRT 随机输入专项 | G1 | 交接清单提到 `trt_cpp_test`，但本次分享页无输出 | 可能做过或仅准备过测试目录，不能确定 | 不能把它列为已验证 |

## 2. 环境与通信

### 2.1 T01：ROS 2 图和视频话题

执行位置：G1。

```bash
ros2 node list
ros2 topic list
ros2 topic info -v /frontvideostream
```

关键输出：

```text
/frontvideostream
Type: unitree_go/msg/Go2FrontVideoData
Publisher count: 1
Subscription count: 1
Node name: _CREATED_BY_BARE_DDS_APP_
Reliability: RELIABLE
Durability: VOLATILE
```

结论：

- **已验证成功**：ROS 2 CLI 能查询已有图，说明基础命令与发现链路至少部分可用。
- **已观察到**：视频端点来自 bare DDS 应用。
- **待验证**：当前 shell 是否能创建并完成普通 `rclcpp` Pub/Sub。

### 2.2 普通 DDS 通信

早期 `listener` 没有输出。它本来会等待 `chatter`，所以“无输出”不能证明节点创建失败。随后建议执行 `talker` 作为双向测试，但共享页没有看到最终 `talker/listener` 对照结果。

结论：

- `listener` 无输出不等于崩溃。
- 普通节点收发是否完全正常：**待验证**。

## 3. ONNX 与 TensorRT

### 3.1 T03：自定义 ONNX 导出

执行设备：开发机 Ubuntu 24.04。

```bash
cd /home/mick/robot-vision
yolo export model=models/custom/best.pt format=onnx imgsz=640
```

环境行：

```text
Ultralytics 8.4.183
Python-3.12.3
torch-2.14.1+cu130
CPU (13th Gen Intel Core i7-13650HX)
```

关键输出：

```text
73 layers, 3,006,428 parameters, 8.1 GFLOPs
input (1,3,640,640)
output (1,8,8400)
opset 18
best.onnx (11.7 MB)
```

Status：**导出成功**。  
不能得出：ONNX 检测准确或 G1 可执行。

### 3.2 T04：G1 TensorRT Engine 构建

执行位置：G1。

```bash
/usr/src/tensorrt/bin/trtexec \
  --onnx="$HOME/robot-vision/models/custom/best.onnx" \
  --saveEngine="$HOME/robot-vision/engines/best_fp32.engine" \
  --workspace=1024 \
  --buildOnly
```

关键输出：

```text
Selected Device: Orin
Compute Capability: 8.7
SMs: 8
Device Global Memory: 15388 MiB
TensorRT version: 8.5.2
Opset version: 18
Engine built in 170.956 sec.
Loaded engine size: 12 MiB
Engine deserialized in 0.0422227 sec.
&&&& PASSED
```

Status：**构建成功**。

警告：

- INT64 weights cast down to INT32；
- 1024 MiB workspace 下部分 tactics 不可用。

两者都未阻止最终 `PASSED`。

### 3.3 T11：`trtexec` 推理性能

命令：

```bash
/usr/src/tensorrt/bin/trtexec \
  --loadEngine="$HOME/robot-vision/engines/best_fp32.engine" \
  --warmUp=500 \
  --duration=5
```

历史记录：

```text
用户：运行结果很好……后面锁频+spin wait又测了一遍，更好了
```

Status：**性能测试执行过**；精确指标缺失。  
不能得出：G1 在实际相机、预处理、后处理和系统负载下达到多少 FPS。

## 4. Python 推理

### 4.1 T05：随机输入 Engine 烟雾测试

执行位置：G1。

```bash
python3 /home/unitree/robot-vision/scripts/trt_smoke_test.py
```

输入：`numpy.random.default_rng(42)` 生成的 `float32` 随机张量。  
形状：`(1,3,640,640)`。

关键输出：

```text
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

Status：**推理执行成功**。  
不能得出：模型输出有语义意义。随机输入下的类别分数只用于检查数值链路。

### 4.2 T06：官方模型真实图片

开发机实际日志：

```text
image 1/1 ... 384x640 1 person, 5.2ms
Speed: 31.9ms preprocess, 5.2ms inference,
       10.1ms postprocess per image at shape (1, 3, 384, 640)
类别=person, 置信度=0.881,
边界框=(317.2, 164.5, 1357.9, 1013.7)
```

Status：**真实图片检测成功**，设备上下文为 RTX 4070 开发机。  
不能得出：G1 的 TensorRT 输出达到同一精度或延迟。

### 4.3 T07：自定义模型真实图片

对话记录的自定义模型结果：

```text
类别: bingren
置信度: 0.9780
边界框: (33, 236, 1210, 1298)
```

共享页没有完整保留该次运行的命令和全部前后日志。  
Status：**已观察到检测结果**，不是本次重新执行的原始日志。  
不能得出：C++ 或 TensorRT Engine 对同一图片结果一致。

## 5. 相机与 DDS

### 5.1 T08：Unitree SDK 取图

最小程序构建通过后，`robot_detector_cpp` 的真实相机日志为：

```text
[Camera] JPEG bytes: 144126
[Camera] Image size: 1920x1080
```

后续不同时刻：

```text
127575 bytes
151467 bytes
```

均成功解码为 `1920x1080`。  
Status：**Unitree SDK 相机取图已验证成功**。

### 5.2 T09：原生 Cyclone DDS Reader

```bash
CYCLONEDDS_URI='file:///home/unitree/cyclonedds_ws/cyclonedds.xml' \
  ./build/camera_dds_test
```

输出：

```text
[DDS] Creating participant...
[DDS] Subscriber created.
[DDS] Waiting for camera frames...
^C
```

Status：**Reader 创建成功，0 帧**。  
根因：**尚未确认**。  
不能得出：相机未发布、DDS 整体故障或 `rt/frontvideostream` 一定错误。

## 6. C++ 视觉链路

### 6.1 T10：真实相机单帧运行

执行位置：G1。  
输入：实时相机 JPEG。

```bash
cd /home/unitree/robot-vision/cpp/robot_detector_cpp
./build/robot_detector_cpp
```

第一次日志摘要：

```text
[Camera] JPEG bytes: 144126
[Camera] Image size: 1920x1080
[Preprocess] Input: 1x3x640x640, scale=0.333333, padding=(0,140)
[TensorRT] Running inference...
[TensorRT] Inference completed
[Detection] Final boxes: 0
[Output] Saved: .../cpp_detection_result.jpg
[Result] C++ real-image detection PASSED
```

后续诊断：

| 输入 | JPEG bytes | 原始 min/max | finite anchors | 最高类别分数 | >0.25 | 最终框 |
|---|---:|---:|---:|---:|---:|---:|
| 实时帧 A | 144126 | 未记录 | 未记录 | 未记录 | 未记录 | 0 |
| 实时帧 B | 127575 | `6.91414e-06 / 635.071` | `8400/8400` | `0.078074` | 0 | 0 |
| 实时帧 C | 151467 | `4.85778e-06 / 634.856` | `8400/8400` | `0.00366318` | 0 | 0 |

Status：

- **程序正常结束**；
- **TensorRT 推理执行成功**；
- **实时检测没有通过阈值的候选框**；
- **检测正确性未通过验收**。

`[Result] ... PASSED` 是程序自己的完成标记，不能代替人工检查检测框。

### 6.2 T13：同图对照

下一步要求：

```bash
./build/robot_detector_cpp /path/to/test_image.jpeg
```

该图片应为 Python 已检测出 `bingren` 的原始图片，不是已经画框的 `*result.jpg`。  
Status：**待验证**；共享对话中没有结果。

## 7. 锁频与 Spin Wait

已知：

- 用户明确表示执行过锁频与 Spin Wait；
- 用户表示结果更好；
- 后续讨论将其视为“初步性能观察”，没有把它当最终部署性能。

缺失：

- `jetson_clocks`、`nvpmodel` 或其他锁频命令；
- `--useSpinWait` 或等价 TensorRT 参数的完整命令；
- 优化前/后的 Throughput、平均/P90/P95/P99 Latency；
- 运行时长、电源模式、温度和并发负载。

因此不能把“更好”量化。下一步应在相同 Engine、相同输入形状、相同电源状态和相同 warm-up 条件下保存两份完整 `trtexec` 日志。

## 8. 测试结论分级

| 目标 | 状态 |
|---|---|
| Engine 文件存在且可构建 | **已验证成功** |
| TensorRT 可分配显存、H2D、执行、D2H | **已验证成功** |
| Unitree SDK 可获取相机 JPEG | **已验证成功** |
| OpenCV 可解码真实 JPEG | **已验证成功** |
| C++ 能执行完整单帧流程 | **已验证成功** |
| ROS 2 能解析 `/frontvideostream` | **失败/待验证** |
| 原生 DDS Reader 能收帧 | **失败** |
| C++ 实时检测框正确 | **未通过验收** |
| C++ 和 Python 同图一致 | **未执行/无证据** |
| 精确 FPS/延迟 | **无证据** |

