# 02 Unitree SDK 与相机接口

当前可工作的实时取图路线是 Unitree SDK 的 `unitree::robot::go2::VideoClient`。它已经返回真实 JPEG，并经 OpenCV 解码为 `1920x1080` 彩色图。

## 1. SDK 安装与组成

| 项目 | 实际路径/值 | 用途 |
|---|---|---|
| SDK 根目录 | `/home/unitree/unitree/unitree_sdk2` | C++ 接口和示例 |
| 静态库 | `/home/unitree/unitree/unitree_sdk2/lib/aarch64/libunitree_sdk2.a` | 链接 Unitree SDK |
| 通信初始化头 | `include/unitree/robot/channel/channel_factory.hpp` | DDS Channel 初始化 |
| 视频客户端头 | `include/unitree/robot/go2/video/video_client.hpp` | `VideoClient` 声明 |
| 视频 API 头 | `include/unitree/robot/go2/video/video_api.hpp` | 视频 API ID/服务名定义路径；完整声明待下次连接核对 |
| 视频示例 | `example/go2/go2_video_client.cpp` | 官方示例路径 |
| 视频消息 C++ 头 | `include/unitree/idl/go2/Go2FrontVideoData_.hpp` | DDS 消息类型 |
| Python IDL | `unitree_sdk2_python/unitree_sdk2py/idl/unitree_go/msg/dds_/` | Python DDS 类型 |
| Cyclone DDS C++ 头 | `/usr/local/include/ddscxx` | `#include <dds/dds.hpp>` |
| DDS 库 | `/usr/local/lib/libddscxx.so`、`/usr/local/lib/libddsc.so` | `ddscxx`、`ddsc` 链接依赖 |

## 2. 相机客户端 API

以下接口来自历史中实际编译和运行的程序。未从 `video_api.hpp` 原样取得的细节会明确标注。

| API | 声明/所属 | 作用 | 关键参数/返回值 | 实测状态 |
|---|---|---|---|---|
| `ChannelFactory::Instance()` | `unitree::robot::ChannelFactory` | 取全局单例 | 返回工厂指针 | **已验证成功** |
| `ChannelFactory::Init(int)` | `unitree/robot/channel/channel_factory.hpp` | 初始化 SDK 通信通道 | 实际调用 `Init(0)`；参数被用作 DDS domain 0；返回类型未在输出中展示 | **已验证成功** |
| `VideoClient()` | `unitree::robot::go2::VideoClient` | 构造视频客户端 | 无参构造 | **已验证成功** |
| `SetTimeout(float)` | `VideoClient` | 设置请求超时 | 实际传 `3.0f`；程序说明按约 3 秒等待，推断单位为秒 | **已验证成功** |
| `Init()` | `VideoClient` | 初始化客户端/服务请求 | 无参；返回类型未在代码中保存 | **已验证成功** |
| `GetImageSample(std::vector<uint8_t>&)` | `VideoClient` | 请求一帧并填入 JPEG 字节 | 返回 `int32_t`；实测 `0` 表示成功；非 0 视为失败；输出参数保存压缩图像 | **已验证成功** |

服务/协议信息：

| 项目 | 已知值 | 状态 |
|---|---|---|
| 视频服务 | `videohub` | **已观察到**：对话交接摘要 |
| API ID | `1001` | **已观察到**：对话交接摘要；应从 `video_api.hpp` 再核对 |
| 数据内容 | JPEG 压缩字节 | **已验证成功**：`cv::imdecode()` 可解码 |
| 相机图像尺寸 | `1920x1080` | **已验证成功** |
| 单帧大小 | `127575`、`144126`、`151467` 字节 | **已验证成功**：不同运行/场景有变化 |

调用顺序不能省：先初始化通道，再构造并初始化客户端，最后请求图像。

## 3. 最小可运行取图程序

下面代码是历史中编译通过并按相同结构用于实测的版本。错误处理和输出保存已保留；它不是完整部署程序。

```cpp
#include <cstdint>
#include <exception>
#include <iostream>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include <unitree/robot/channel/channel_factory.hpp>
#include <unitree/robot/go2/video/video_client.hpp>

int main()
{
    try
    {
        unitree::robot::ChannelFactory::Instance()->Init(0);

        unitree::robot::go2::VideoClient client;
        client.SetTimeout(3.0f);
        client.Init();

        std::vector<uint8_t> image_bytes;
        const int32_t ret = client.GetImageSample(image_bytes);

        std::cout << "return=" << ret
                  << " bytes=" << image_bytes.size() << std::endl;

        if (ret != 0 || image_bytes.empty())
        {
            return 1;
        }

        cv::Mat encoded(
            1,
            static_cast<int>(image_bytes.size()),
            CV_8UC1,
            image_bytes.data());

        cv::Mat image = cv::imdecode(encoded, cv::IMREAD_COLOR);
        if (image.empty())
        {
            return 2;
        }

        std::cout << image.cols << "x" << image.rows << std::endl;
        cv::imwrite(
            "/home/unitree/robot-vision/outputs/video_client_test.jpg",
            image);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 10;
    }

    return 0;
}
```

这段曾成功编译；实际相机取图成功则进一步由 `/home/unitree/robot-vision/cpp/robot_detector_cpp` 的日志验证。

## 4. CMake 与编译

该最小工程第一次编译报：

```text
fatal error: dds/dds.hpp: No such file or directory
```

原因是遗漏 Cyclone DDS C++ 头目录，不是 `GetImageSample()` 或 OpenCV 故障。加入 `/usr/local/include/ddscxx` 后构建通过。

```cmake
cmake_minimum_required(VERSION 3.16)
project(video_client_test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV 4 REQUIRED COMPONENTS core imgcodecs)

set(UNITREE_SDK_DIR "/home/unitree/unitree/unitree_sdk2")

add_executable(video_client_test main.cpp)

target_include_directories(video_client_test PRIVATE
    "${UNITREE_SDK_DIR}/include"
    /usr/local/include/ddscxx
    ${OpenCV_INCLUDE_DIRS}
)

target_link_directories(video_client_test PRIVATE
    /usr/local/lib
)

target_link_libraries(video_client_test PRIVATE
    "${UNITREE_SDK_DIR}/lib/aarch64/libunitree_sdk2.a"
    ${OpenCV_LIBS}
    ddscxx
    ddsc
    pthread
)

set_target_properties(video_client_test PROPERTIES
    BUILD_RPATH "/usr/local/lib"
)
```

```bash
cd /home/unitree/robot-vision/cpp/video_client_test
cmake -S . -B build
cmake --build build -j2
./build/video_client_test
```

## 5. JPEG 到 OpenCV 图像

`GetImageSample()` 填入的是压缩字节，不是 `cv::Mat`。完整过程是：

1. 用 `std::vector<uint8_t>` 接收。
2. 包装为单行 `CV_8UC1` 连续字节视图：

   ```cpp
   cv::Mat encoded(1, image_bytes.size(), CV_8UC1, image_bytes.data());
   ```

3. 调用 `cv::imdecode(encoded, cv::IMREAD_COLOR)`；结果通常是 OpenCV 的 BGR 三通道图像。
4. 检查 `image.empty()`，再读 `image.cols/image.rows`。
5. 可选 `cv::imwrite()` 保存，或直接送入 letterbox/推理流程。

`image_bytes` 必须在 `cv::imdecode()` 执行期间保持有效。实际观察到 JPEG 大小约 120–150 KiB，解码后为 `1920x1080x3`。

## 6. `Go2` 命名与 G1-EDU 的关系

已确认的事实：

- 当前硬件身份是 **G1-EDU**。
- SDK 的实际视频客户端类名是 `unitree::robot::go2::VideoClient`。
- ROS 2 报告的消息类型是 `unitree_go/msg/Go2FrontVideoData`。
- C++ DDS 生成类型是 `unitree_go::msg::dds_::Go2FrontVideoData_`。
- 该 C++ 类型在 G1 上可通过 SDK 获取到真实前置图像。

不能从这些名称推出当前硬件是 Go2。更合理的描述是：当前 G1 软件栈复用了 `unitree_go`/`go2` 命名空间和 `videohub` 视频接口。至于前置相机实际对应 `videohub_pc4`、胸前 `videohub_pc4_chest` 还是两者之一，尚未确认。

## 7. 与原生 DDS 的边界

SDK `VideoClient` 已验证能获得 JPEG；原生 DDS Reader 对 `/frontvideostream` 的 `rt/frontvideostream` 尝试没有收到帧。两者不能混写为“DDS 相机接口已打通”。详见 [03_dds_ros2_camera.md](03_dds_ros2_camera.md)。

