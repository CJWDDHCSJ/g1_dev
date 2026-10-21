# 05 C++ 视觉检测完整流程

本文描述历史中已经构建和运行的单帧 C++ 检测程序 `robot_detector_cpp`。它在 G1 上完成过一次“相机取图 → TensorRT 推理 → 输出保存”，但实时画面判为 0 个检测框，且同图 Python/C++ 对照尚未完成。

## 1. 源码与构建位置

| 项目 | 路径/值 |
|---|---|
| 工程 | `/home/unitree/robot-vision/cpp/robot_detector_cpp` |
| 入口 | `main.cpp` |
| 构建文件 | `CMakeLists.txt` |
| 可执行文件 | `build/robot_detector_cpp` |
| Engine | `/home/unitree/robot-vision/engines/best_fp32.engine` |
| 输出图 | `/home/unitree/robot-vision/outputs/cpp_detection_result.jpg` |
| C++ 标准 | C++17 |
| OpenCV | 4.2.0；使用 `core`、`imgproc`、`imgcodecs`、`dnn` |

当前工作区没有该 G1 工程源码，本文依据历史中展示的代码副本整理。G1 上实际文件可能还包含后来的调试插入，不能保证与本文片段逐字相同。

## 2. 构建配置

```cmake
cmake_minimum_required(VERSION 3.16)
project(robot_detector_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(UNITREE_SDK_DIR "/home/unitree/unitree/unitree_sdk2")

find_package(OpenCV 4 REQUIRED COMPONENTS
    core
    imgproc
    imgcodecs
    dnn
)

add_executable(robot_detector_cpp main.cpp)

target_include_directories(robot_detector_cpp PRIVATE
    "${UNITREE_SDK_DIR}/include"
    /usr/include/aarch64-linux-gnu
    /usr/local/include/ddscxx
    /usr/local/cuda/targets/aarch64-linux/include
    ${OpenCV_INCLUDE_DIRS}
)

target_link_directories(robot_detector_cpp PRIVATE
    /usr/local/lib
    /lib/aarch64-linux-gnu
    /usr/local/cuda/targets/aarch64-linux/lib
)

target_link_libraries(robot_detector_cpp PRIVATE
    "${UNITREE_SDK_DIR}/lib/aarch64/libunitree_sdk2.a"
    nvinfer
    cudart
    ${OpenCV_LIBS}
    ddscxx
    ddsc
    pthread
)

set_target_properties(robot_detector_cpp PROPERTIES
    BUILD_RPATH "/usr/local/lib"
)
```

编译：

```bash
cd /home/unitree/robot-vision/cpp/robot_detector_cpp
cmake -S . -B build
cmake --build build -j2
```

`opencv_dnn` 只用于 `cv::dnn::NMSBoxes()`，TensorRT 仍负责模型推理。

## 3. 完整数据流

```text
VideoClient::GetImageSample
        ↓  JPEG bytes
cv::Mat CV_8UC1 → cv::imdecode
        ↓  BGR 1920×1080
Letterbox 到 640×640（114 灰色填充）
        ↓
BGR → RGB → float32 / 255 → HWC → CHW
        ↓  3×640×640 contiguous vector
cudaMemcpyHostToDevice
        ↓
TensorRT enqueueV3(nullptr) + cudaDeviceSynchronize
        ↓  output0: 8×8400 float32
按 anchor 选择最高类别分数
        ↓  conf ≥ 0.25
cx,cy,w,h → 原图 xyxy → clamp
        ↓
按类别 NMS (IoU=0.45)
        ↓
画框 + 保存 JPEG
```

### 3.1 相机取图

```cpp
unitree::robot::ChannelFactory::Instance()->Init(0);

unitree::robot::go2::VideoClient video_client;
video_client.SetTimeout(3.0F);
video_client.Init();

std::vector<uint8_t> jpeg_data;
const int camera_ret =
    video_client.GetImageSample(jpeg_data);

if (camera_ret != 0 || jpeg_data.empty()) {
    throw std::runtime_error("GetImageSample failed");
}

cv::Mat encoded(
    1,
    static_cast<int>(jpeg_data.size()),
    CV_8UC1,
    jpeg_data.data());

cv::Mat image = cv::imdecode(encoded, cv::IMREAD_COLOR);
```

实测取到的 JPEG 为 `127575`、`144126`、`151467` 字节，解码尺寸 `1920x1080`。

### 3.2 Letterbox

实际实现：

```cpp
const float scale = std::min(
    640.0F / image.cols,
    640.0F / image.rows);

const int new_w = std::round(image.cols * scale);
const int new_h = std::round(image.rows * scale);

cv::resize(image, resized, cv::Size(new_w, new_h),
           0.0, 0.0, cv::INTER_LINEAR);

const int left  = std::round((640 - new_w) / 2.0F - 0.1F);
const int top   = std::round((640 - new_h) / 2.0F - 0.1F);

cv::copyMakeBorder(
    resized, padded,
    top, 640 - new_h - top,
    left, 640 - new_w - left,
    cv::BORDER_CONSTANT,
    cv::Scalar(114, 114, 114));
```

对 `1920x1080` 输入，实际日志为：

```text
scale=0.333333
padding=(0,140)
```

即缩放为 `640x360`，上下各补 140 像素，得到 `640x640`。

### 3.3 RGB / CHW / 归一化

```cpp
cv::cvtColor(padded, rgb, cv::COLOR_BGR2RGB);

std::vector<float> input(3 * 640 * 640);

for (int y = 0; y < 640; ++y) {
    const cv::Vec3b* row = rgb.ptr<cv::Vec3b>(y);
    for (int x = 0; x < 640; ++x) {
        const cv::Vec3b pixel = row[x];
        const std::size_t offset = y * 640 + x;

        input[offset]             = pixel[0] / 255.0F;
        input[640 * 640 + offset] = pixel[1] / 255.0F;
        input[2 * 640 * 640 + offset] = pixel[2] / 255.0F;
    }
}
```

实际使用的归一化是简单的 `[0,255] -> [0,1]`。代码没有展示 ImageNet mean/std 处理。

## 4. TensorRT 加载和推理

实际流程：

```cpp
auto data = loadEngineFile(ENGINE_PATH);

std::unique_ptr<nvinfer1::IRuntime> runtime(
    nvinfer1::createInferRuntime(logger));

std::unique_ptr<nvinfer1::ICudaEngine> engine(
    runtime->deserializeCudaEngine(data.data(), data.size()));

std::unique_ptr<nvinfer1::IExecutionContext> context(
    engine->createExecutionContext());

const char* input_name = "images";
const char* output_name = "output0";

context->setTensorAddress(input_name, device_input);
context->setTensorAddress(output_name, device_output);

cudaMemcpy(device_input, host_input.data(), input_bytes,
           cudaMemcpyHostToDevice);

if (!context->enqueueV3(nullptr)) {
    throw std::runtime_error("enqueueV3 failed");
}

cudaDeviceSynchronize();

cudaMemcpy(host_output.data(), device_output, output_bytes,
           cudaMemcpyDeviceToHost);
```

关键规则：

- `setTensorAddress()` 必须在 `enqueueV3()` 前完成。
- CUDA stream 传 `nullptr` 使用默认流；同步必须用 `cudaDeviceSynchronize()` 或等价的 stream 同步。
- `host_output` 大小是 `8 * 8400` 个 `float`。
- `cudaMalloc/cudaFree` 和 TensorRT 生命周期都应覆盖异常路径。

## 5. 输出解析与坐标还原

输入输出：

```text
input  images   float32  (1,3,640,640)
output output0  float32  (1,8,8400)
```

每个候选位置 `i` 的布局：

```text
0: cx
1: cy
2: width
3: height
4: class 0 score ("bingren")
5: class 1 score ("wangqiu")
6: class 2 score ("zhentou")
7: class 3 score ("shubiao")
```

代码对每个 anchor 选取四个类别分数中的最大值，然后做阈值筛选：

```cpp
constexpr float CONF_THRESHOLD = 0.25F;
constexpr float IOU_THRESHOLD = 0.45F;

if (!finite || confidence < CONF_THRESHOLD) {
    continue;
}

const float x1f = (cx - bw / 2 - pad_x) / scale;
const float y1f = (cy - bh / 2 - pad_y) / scale;
const float x2f = (cx + bw / 2 - pad_x) / scale;
const float y2f = (cy + bh / 2 - pad_y) / scale;

// 再 clamp 到原图 [0,width-1] / [0,height-1]
```

这里默认输出已经是 `xywh`（中心点+宽高）baked 后处理，不是三个独立特征图。`(1,8,8400)` 与代码一致；最终类别数 4，若把类别数解释为 7 会与代码和类别表冲突。

### 5.1 NMS

按类别分别收集：

```cpp
std::vector<std::vector<cv::Rect>> boxes_by_class(4);
std::vector<std::vector<float>> scores_by_class(4);

for (int class_id = 0; class_id < 4; ++class_id) {
    std::vector<int> selected;
    cv::dnn::NMSBoxes(
        boxes_by_class[class_id],
        scores_by_class[class_id],
        CONF_THRESHOLD,
        IOU_THRESHOLD,
        selected);
}
```

按类别 NMS 可以避免不同类别因高 IoU 被互相抑制。

### 5.2 绘制和保存

```cpp
cv::rectangle(image, box, cv::Scalar(0, 255, 0), 2);
cv::putText(
    image,
    label,
    cv::Point(box.x, std::max(20, box.y - 8)),
    cv::FONT_HERSHEY_SIMPLEX,
    0.6,
    cv::Scalar(0, 255, 0),
    2);

cv::imwrite(
    "/home/unitree/robot-vision/outputs/cpp_detection_result.jpg",
    image);
```

程序最后打印：

```text
[Result] C++ real-image detection PASSED
```

这行只表示程序走完了当前 `try` 分支，不表示检测结果一定有意义；真实输出为 `Final boxes: 0`。

## 6. 第一次 C++ 相机运行

```text
[Camera] JPEG bytes: 144126
[Camera] Image size: 1920x1080
[Preprocess] Input: 1x3x640x640, scale=0.333333, padding=(0,140)
[TensorRT] Running inference...
[TensorRT] Inference completed
[Detection] Final boxes: 0
[Output] Saved: /home/unitree/robot-vision/outputs/cpp_detection_result.jpg
```

后续两次诊断：

| 运行 | 原始输出 min/max | 最高类别分数 | 超过 0.25 | 最终框数 |
|---|---:|---:|---:|---:|
| 1 | `6.91414e-06 / 635.071` | `0.078074` | 0 | 0 |
| 2 | `4.85778e-06 / 634.856` | `0.00366318` | 0 | 0 |

这些数值表明 Engine 输出了有限数值，且当前实时画面中最高候选分数低于阈值；但**不能据此确定**是相机画面不匹配、预处理差异还是输出解释问题。

## 7. Python 与 C++ 的已知差异

### 7.1 已验证共同点

- 模型输入尺寸为 `640`。
- 自定义模型输出形状 `(1,8,8400)`。
- 置信度阈值使用 `0.25`。
- 类别数量为 4，类别顺序为 `bingren, wangqiu, zhentou, shubiao`。
- ONNX 已成功构建 Engine；Python 和 C++ 均可实际调用 Engine。

### 7.2 尚未完全对齐

| 项目 | Python/Ultralytics | C++ | 状态 |
|---|---|---|---|
| 图像缩放 | Ultralytics 内部 `imgsz=640` 预处理细节未逐行复核 | 明确使用居中 letterbox、114 填充、round 规则 | **待对照同一输入张量** |
| 颜色/归一化 | Ultralytics 内部实现 | BGR→RGB，`/255`，CHW | **待对照张量** |
| 输出布局 | `results.boxes` 已由 Ultralytics 解码 | 手工按 8 通道、8400 anchor 解析 | **待对照原始输出** |
| 候选阈值 | `conf=0.25` | `0.25` | **已对齐代码参数** |
| NMS | Ultralytics 默认/内部 | 按类别 `NMSBoxes`，IoU 0.45 | **待确认实现差异** |
| 输入来源 | 固定图片 | 第一次程序使用相机实时帧 | **不可直接对比** |

Python 中检测成功的那张原图曾被要求用于 C++ 对照，输出预期为 `bingren`、置信度约 `0.9780`、框 `(33, 236, 1210, 1298)`；但历史中没有看到 C++ 对同一张原图的运行结果，因此**不能声称同图对照完成**。

## 8. 后续正确修法

不要先改阈值。建议按以下顺序：

1. 找到 Python 已验证成功的原始图片的明确路径，确认不是已经画框的输出图。
2. 给 C++ 增加图片路径输入模式，或直接复制同一预处理函数对文件运行。
3. 对同一图片比较：
   - letterbox 后尺寸、scale、pad；
   - 输入向量前若干元素或哈希；
   - Engine 原始输出的最大值和最大值索引；
   - 超过阈值的候选数量；
   - 最终框坐标。
4. 如果 C++ 对同一图仍为 0，优先检查预处理和输出索引；如果同一图正常，再看实时相机画面、曝光、角度和模型覆盖范围。

