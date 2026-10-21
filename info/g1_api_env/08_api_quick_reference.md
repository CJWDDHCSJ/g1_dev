# 08 API 与命令速查表

本页只放高频、可直接定位的接口、路径和命令。详细证据与限制见对应专题文档。

## 1. 路径速查

| 名称 | 路径 |
|---|---|
| G1 项目根 | `/home/unitree/robot-vision` |
| SDK | `/home/unitree/unitree/unitree_sdk2` |
| SDK library | `/home/unitree/unitree/unitree_sdk2/lib/aarch64/libunitree_sdk2.a` |
| VideoClient header | `/home/unitree/unitree/unitree_sdk2/include/unitree/robot/go2/video/video_client.hpp` |
| Video API header | `/home/unitree/unitree/unitree_sdk2/include/unitree/robot/go2/video/video_api.hpp` |
| Video sample | `/home/unitree/unitree/unitree_sdk2/example/go2/go2_video_client.cpp` |
| Video DDS header | `/home/unitree/unitree/unitree_sdk2/include/unitree/idl/go2/Go2FrontVideoData_.hpp` |
| Cyclone DDS XML | `/home/unitree/cyclonedds_ws/cyclonedds.xml` |
| ONNX | `/home/unitree/robot-vision/models/custom/best.onnx` |
| Engine | `/home/unitree/robot-vision/engines/best_fp32.engine` |
| C++ detector | `/home/unitree/robot-vision/cpp/robot_detector_cpp` |
| C++ output | `/home/unitree/robot-vision/outputs/cpp_detection_result.jpg` |

## 2. Unitree SDK 视频 API

| API | 用途 | 关键参数/返回值 | 已验证状态 |
|---|---|---|---|
| `ChannelFactory::Instance()` | 取得 SDK 通信工厂单例 | 返回工厂对象/指针 | 成功 |
| `ChannelFactory::Init(0)` | 初始化 Domain 0 通道 | 参数为 domain id；返回类型待从头文件复核 | 成功 |
| `unitree::robot::go2::VideoClient` | 视频请求客户端 | 当前中文档中的实际类名 | 成功 |
| `VideoClient::SetTimeout(3.0F)` | 设置请求等待时间 | `float`；代码说明约 3 秒 | 成功 |
| `VideoClient::Init()` | 客户端初始化 | 无参 | 成功 |
| `VideoClient::GetImageSample(std::vector<uint8_t>&)` | 取 JPEG 字节 | 返回 `int32_t`，0 成功；输出参数填充数据 | 成功 |

服务：`videohub`，API ID `1001`（对话观察，完整声明需查 `video_api.hpp`）。

调用顺序：

```cpp
unitree::robot::ChannelFactory::Instance()->Init(0);

unitree::robot::go2::VideoClient client;
client.SetTimeout(3.0F);
client.Init();

std::vector<uint8_t> jpeg;
int32_t ret = client.GetImageSample(jpeg);
```

JPEG 解码：

```cpp
cv::Mat encoded(
    1,
    static_cast<int>(jpeg.size()),
    CV_8UC1,
    jpeg.data());

cv::Mat image = cv::imdecode(encoded, cv::IMREAD_COLOR);
```

## 3. Topic 与消息

| Topic/Service | 类型 | 说明 | 当前状态 |
|---|---|---|---|
| `/frontvideostream` | `unitree_go/msg/Go2FrontVideoData` | 前置视频 DDS 端点 | 可发现；本地消息包缺失；Reader 未收帧 |
| `/api/videohub/request` | `unitree_api/msg/Request` | videohub 请求 | 类型可见；未解析 |
| `/api/videohub/response` | `unitree_api/msg/Response` | videohub 响应 | 类型可见；未解析 |
| `/videohub/inner` | `std_msgs/msg/String` | videohub 内部字符串 | 类型可见；内容未知 |

`Go2FrontVideoData_` 字段：

| 字段 | 类型 |
|---|---|
| `time_frame` | `uint64_t` |
| `video720p` | `std::vector<uint8_t>` |
| `video360p` | `std::vector<uint8_t>` |
| `video180p` | `std::vector<uint8_t>` |

QoS 观察：

- `/frontvideostream`：`RELIABLE`；
- Durability：`VOLATILE`；
- Liveliness：`AUTOMATIC`。

## 4. DDS 配置

```xml
<CycloneDDS>
  <Domain Id="any">
    <General>
      <Interfaces>
        <NetworkInterface name="eth0"
                          priority="default"
                          multicast="default" />
      </Interfaces>
      <AllowMulticast>spdp</AllowMulticast>
    </General>
  </Domain>
</CycloneDDS>
```

```bash
echo "$RMW_IMPLEMENTATION"
# rmw_cyclonedds_cpp

ip -br addr
# G1 当前有线接口 eth0: 192.168.123.164/24

ros2 topic info -v /frontvideostream
```

原生 Reader 使用配置：

```bash
CYCLONEDDS_URI='file:///home/unitree/cyclonedds_ws/cyclonedds.xml' \
  ./build/camera_dds_test
```

`rt/frontvideostream` 是当时的尝试名，不是已证实名称。

## 5. TensorRT API

| API | 头/库 | 用途 | 关键参数 |
|---|---|---|---|
| `nvinfer1::createInferRuntime` | `NvInfer.h`, `nvinfer` | 创建运行时 | `ILogger&` |
| `IRuntime::deserializeCudaEngine` | TensorRT | 加载 Engine | 字节指针、大小 |
| `ICudaEngine::createExecutionContext` | TensorRT | 创建上下文 | 无参数 |
| `ICudaEngine::getTensorIOMode` | TensorRT 8.5 | 检查输入/输出角色 | tensor name |
| `IExecutionContext::setTensorAddress` | TensorRT 8.5 | 绑定设备地址 | tensor name、`void*` |
| `IExecutionContext::enqueueV3` | TensorRT 8.5 | 异步推理 | CUDA stream，可为 `nullptr` |
| `cudaMalloc` | `cuda_runtime_api.h`, `cudart` | 分配 GPU 缓冲 | `void**`, bytes |
| `cudaMemcpy` | CUDA Runtime | H2D/D2H | dst, src, bytes, direction |
| `cudaDeviceSynchronize` | CUDA Runtime | 等待 GPU | 无参数 |
| `cudaFree` | CUDA Runtime | 释放 GPU | 指针 |

模型规格：

```text
images  float32  (1,3,640,640)   1,228,800 elements
output0 float32  (1,8,8400)         67,200 elements
```

输出：

```text
0: cx
1: cy
2: width
3: height
4: class bingren
5: class wangqiu
6: class zhentou
7: class shubiao
```

阈值：

```text
confidence >= 0.25
class-wise NMS IoU = 0.45
```

## 6. CMake 依赖

最小视频工程：

```cmake
find_package(OpenCV 4 REQUIRED COMPONENTS core imgcodecs)

target_include_directories(video_client_test PRIVATE
    /home/unitree/unitree/unitree_sdk2/include
    /usr/local/include/ddscxx
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(video_client_test PRIVATE
    /home/unitree/unitree/unitree_sdk2/lib/aarch64/libunitree_sdk2.a
    ${OpenCV_LIBS}
    ddscxx
    ddsc
    pthread
)
```

检测工程额外需要：

```cmake
find_package(OpenCV 4 REQUIRED COMPONENTS
    core imgproc imgcodecs dnn)

target_include_directories(robot_detector_cpp PRIVATE
    /usr/local/cuda/targets/aarch64-linux/include
)

target_link_libraries(robot_detector_cpp PRIVATE
    nvinfer
    cudart
)
```

必须把 `/usr/local/include/ddscxx` 加入 include，否则 `<dds/dds.hpp>` 找不到。

## 7. 常用命令

### 环境检查

```bash
uname -a
cat /etc/os-release
cat /etc/nv_tegra_release 2>/dev/null
lscpu
free -h
ip -br addr
ip route
nvcc --version
pkg-config --modversion opencv4
gcc --version
g++ --version
cmake --version
```

### ROS 2/DDS

```bash
echo "$RMW_IMPLEMENTATION"
echo "$ROS_DOMAIN_ID"
ros2 node list
ros2 topic list
ros2 topic info -v /frontvideostream
ros2 interface show unitree_go/msg/Go2FrontVideoData
ros2 pkg list | grep -E 'unitree|rmw_cyclonedds'
```

### Engine 构建

```bash
/usr/src/tensorrt/bin/trtexec \
  --onnx="$HOME/robot-vision/models/custom/best.onnx" \
  --saveEngine="$HOME/robot-vision/engines/best_fp32.engine" \
  --workspace=1024 \
  --buildOnly
```

### Engine 推理

```bash
/usr/src/tensorrt/bin/trtexec \
  --loadEngine="$HOME/robot-vision/engines/best_fp32.engine" \
  --warmUp=500 \
  --duration=5
```

### Python TensorRT 烟雾测试

```bash
python3 /home/unitree/robot-vision/scripts/trt_smoke_test.py
```

### C++ 视频/C++ 检测

```bash
cd /home/unitree/robot-vision/cpp/video_client_test
cmake -S . -B build
cmake --build build -j2
./build/video_client_test

cd /home/unitree/robot-vision/cpp/robot_detector_cpp
cmake -S . -B build
cmake --build build -j2
./build/robot_detector_cpp
```

### 原生 DDS Reader

```bash
cd /home/unitree/robot-vision/cpp/camera_dds_test
cmake -S . -B build
cmake --build build -j2

CYCLONEDDS_URI='file:///home/unitree/cyclonedds_ws/cyclonedds.xml' \
  ./build/camera_dds_test
```

## 8. 修改前的检查

| 问题 | 先查 |
|---|---|
| 当前路径是否正确 | `pwd`, `ls -lh` |
| 当前 shell 是否加载了 SDK/ROS | `env | grep -E 'AMENT|RMW|CYCLONE'` |
| 链接是否正确 | `ldd ./build/程序` |
| 相机是否真能取图 | `GetImageSample` + `imdecode` |
| Engine 是否可加载 | 先用已验证脚本运行，不直接改业务代码 |
| 是否有目标框 | 先打印分数分布，再检查同图对照 |
| 性能是否变化 | 固定输入、Engine、温度、电源和命令 |

