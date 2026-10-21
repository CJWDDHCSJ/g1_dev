# 03 DDS、ROS 2 与相机数据通信

本文只记录当前 G1-EDU 上已经观察到的 ROS 2/DDS 状态。核心结论是：**ROS 2 能发现话题，不等于本地具备消息接口包；原生 DDS 能建立 Reader，不等于已经收到相机帧。**

## 1. G1 的 Cyclone DDS 配置

G1 当前文件中看到的配置为：

```xml
<?xml version="1.0" encoding="UTF-8" ?>
<CycloneDDS xmlns="https://cdds.io/config"
            xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
            xsi:schemaLocation="https://cdds.io/config
              https://raw.githubusercontent.com/eclipse-cyclonedds/cyclonedds/master/etc/cyclonedds.xsd">
    <Domain Id="any">
        <General>
            <Interfaces>
                <NetworkInterface name="eth0"
                                  priority="default"
                                  multicast="default" />
            </Interfaces>
            <AllowMulticast>spdp</AllowMulticast>
        </General>
        <Internal>
            <!--MinimumSocketReceiveBufferSize>50MB</MinimumSocketReceiveBufferSize-->
        </Internal>
    </Domain>
</CycloneDDS>
```

| 配置项 | 含义 | 已知事实 |
|---|---|---|
| `<Domain Id="any">` | 配置适用于任意 DDS Domain，具体运行时仍可指定 Domain ID | 原生 Reader 实际创建了 `DomainParticipant(0)` |
| `NetworkInterface name="eth0"` | DDS 只绑定 `eth0` | G1 的 `eth0` 为 `192.168.123.164/24`，状态 UP |
| `priority="default"` | 使用默认接口优先级 | 未做更深入性能验证 |
| `multicast="default"` | 接口使用默认组播策略 | 实际发现结果仍需单独判断 |
| `AllowMulticast=spdp` | 允许 SPDP 发现使用组播 | 与“ROS 2 CLI 可看到话题”一致，但不能证明用户 Reader 与发布者完成匹配 |

`/home/unitree/cyclonedds_ws/cyclonedds.xml` 是配置文件路径。原生 Reader 运行时曾显式指定：

```bash
CYCLONEDDS_URI='file:///home/unitree/cyclonedds_ws/cyclonedds.xml' \
  ./build/camera_dds_test
```

## 2. `enp3s0` 与 `eth0` 的冲突

历史中同时出现过两个网卡名，必须保留各自上下文：

| 出现位置 | 值 | 观察结果 |
|---|---|---|
| G1 早期 shell 环境的 `CYCLONEDDS_URI` | `enp3s0` | `ros2 topic hz` 报 `enp3s0: does not match an available interface` 和 `rmw_create_node: failed to create domain` |
| G1 的 `cyclonedds.xml` | `eth0` | 与 `ip -br addr` 中的实际接口一致 |

因此当前文件使用 `eth0` 是合理的，但 `enp3s0` 原本属于哪个环境、是否是开发机跨机配置，**尚未确认**。在获得配置负责人或 `video_api`/启动脚本证据之前，不要直接删改 XML。

## 3. ROS 2 Topic 清单

### 3.1 视频相关

| Topic | 类型 | 发布者/订阅者 | QoS（发现信息） | 已知用途 | 真实数据接收 |
|---|---|---:|---|---|---|
| `/frontvideostream` | `unitree_go/msg/Go2FrontVideoData` | 1 / 1 | RELIABLE，VOLATILE，AUTOMATIC Liveliness；Deadline/Lifespan 为最大 int64 | 前置视频流 | **未通过 ROS CLI/原生 Reader 成功取得帧**；Unitree SDK `VideoClient` 可取得 JPEG |
| `/api/videohub/request` | `unitree_api/msg/Request` | 2 / 1 | 未取得详细 QoS | videohub 服务请求通道 | **未观察到应用数据解析结果** |
| `/api/videohub/response` | `unitree_api/msg/Response` | 1 / 2 | 未取得详细 QoS | videohub 服务响应通道 | **未观察到应用数据解析结果** |
| `/videohub/inner` | `std_msgs/msg/String` | 1 / 1 | 未取得详细 QoS | videohub 内部字符串通道；具体语义待定 | **未观察到真实消息内容** |

`/frontvideostream` 的详细端点输出为：

```text
Type: unitree_go/msg/Go2FrontVideoData
Publisher count: 1
Node name: _CREATED_BY_BARE_DDS_APP_
Topic type: unitree_go/msg/Go2FrontVideoData
Endpoint type: PUBLISHER
Reliability: RELIABLE
Durability: VOLATILE

Subscription count: 1
Node name: _CREATED_BY_BARE_DDS_APP_
Topic type: unitree_go/msg/Go2FrontVideoData
Endpoint type: SUBSCRIPTION
Reliability: RELIABLE
Durability: VOLATILE
```

`_CREATED_BY_BARE_DDS_APP_` 表示端点不是普通 ROS 2 节点暴露出来的名称，不能拿它证明一个普通 `rclcpp` 节点已经完成创建和收发。

### 3.2 传感器与状态 Topic

| Topic | 类型 | 发布/订阅 | 结论 |
|---|---|---:|---|
| `/utlidar/cloud_livox_mid360` | `sensor_msgs/msg/PointCloud2` | 1 / 1 | 话题存在；频率未成功测量 |
| `/utlidar/imu_livox_mid360` | `sensor_msgs/msg/Imu` | 1 / 0 | 话题存在；频率未成功测量 |
| `/lowstate` | 未在此次页面展开 | 未记录 | 只能确认出现在话题列表 |
| `/sportmodestate` | 未在此次页面展开 | 未记录 | 只能确认出现在话题列表 |
| `/api/.../request` / `/api/.../response` | Unitree API 风格 | 多处存在 | 仅作为接口线索，不能据此编写服务调用 |

## 4. `Go2FrontVideoData` 消息定义

实际 C++ 头文件：

```text
/home/unitree/unitree/unitree_sdk2/include/unitree/idl/go2/Go2FrontVideoData_.hpp
```

生成工具和版本：

```text
Eclipse Cyclone DDS IDL to CXX Translator
Cyclone DDS: v0.10.2
File name: Go2FrontVideoData_.idl
```

字段：

| IDL/ROS 字段 | C++ 类型 | 访问器 | 含义 |
|---|---|---|---|
| `time_frame` | `uint64_t` | `time_frame()` / `time_frame(value)` | 帧时间标识；时间单位未确认 |
| `video720p` | `std::vector<uint8_t>` | `video720p()` / `video720p(value)` | 720p 视频数据缓冲区；编码未从该类型名称单独证明 |
| `video360p` | `std::vector<uint8_t>` | `video360p()` | 360p 数据缓冲区 |
| `video180p` | `std::vector<uint8_t>` | `video180p()` | 180p 数据缓冲区 |

C++ 完整类型名：

```cpp
using VideoMessage =
    unitree_go::msg::dds_::Go2FrontVideoData_;
```

DDS 类型名为：

```text
unitree_go::msg::dds_::Go2FrontVideoData_
```

生成头文件还标明：

- `isSelfContained() == false`
- `isKeyless() == true`
- 注册了 `REGISTER_TOPIC_TYPE`
- type map blob 大小 386 字节，type info blob 大小 100 字节

这些字段结构与上游 ROS 2 定义一致，但本机 `ros2 interface show` 无法导入它，原因是本地没有可用的 `unitree_go` ROS 2 消息包/生成类型，不是字段结构不存在。

## 5. ROS 2 CLI 发现话题但无法解析

已观察到：

```bash
ros2 topic info /frontvideostream
# Type: unitree_go/msg/Go2FrontVideoData

ros2 interface show unitree_go/msg/Go2FrontVideoData
# Unknown package 'unitree_go'
```

后续 `ros2 topic hz` 也因 Python 无法导入消息类型而失败，出现 `No module named 'unitree_go'` 一类错误。另一次终端记录的 `ros2 pkg list | grep unitree` 没有输出。

这说明：

1. ROS 2 图发现和接口包加载是两个环节。
2. 发现话题只需要端点/topic 信息；解析消息还需要本地类型支持。
3. 当前 shell 的 `AMENT_PREFIX_PATH` 只列出了 Cyclone DDS RMW 工作空间和 `/opt/ros/foxy`，没有明显包含 `unitree_go` 的 ROS 2 包。

搜索可见目录后找到了 SDK Python IDL 路径和 C++ 头文件，但没有找到可直接安装使用的 ROS 2 `unitree_go` 包。根因尚未完全确认，可能是 overlay 未 source、消息包未编译/安装，或服务端与客户端使用了不同部署布局。

## 6. 原生 DDS Reader 测试

### 6.1 实测结果

测试程序结构：

```cpp
dds::domain::DomainParticipant participant(0);

dds::topic::Topic<VideoMessage> topic(
    participant,
    "rt/frontvideostream");  // 当时尝试的映射

dds::sub::Subscriber subscriber(participant);
dds::sub::DataReader<VideoMessage> reader(
    subscriber,
    topic);

while (true) {
    auto samples = reader.take();
    for (const auto& sample : samples) {
        if (!sample.info().valid()) {
            continue;
        }

        const auto& frame = sample.data();
        std::cout
            << frame.time_frame() << " "
            << frame.video720p().size() << " "
            << frame.video360p().size() << " "
            << frame.video180p().size()
            << std::endl;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}
```

终端输出：

```text
[DDS] Creating participant...
[DDS] Subscriber created.
[DDS] Waiting for camera frames...
^C
没反应
```

Status：

| 环节 | 结果 | 状态 |
|---|---|---|
| DomainParticipant 创建 | 成功 | **已验证成功** |
| Topic/Subscriber/DataReader 创建 | 成功 | **已验证成功** |
| 收到有效样本 | 未发生 | **失败** |
| 根因 | 未确定 | **尚未确认** |

### 6.2 已排除的方向

`ldd` 显示：

```text
libddscxx.so.0 => /usr/local/lib/libddscxx.so.0
libddsc.so.0   => /usr/local/lib/libddsc.so.0
```

ROS 2 也使用 `rmw_cyclonedds_cpp`。因此不能简单归因于“ROS 2 用 Fast DDS，程序用 Cyclone DDS”的明显混用。

仍需核对的可能方向：

- `/frontvideostream` 的底层 Topic 名称是否确实是 `rt/frontvideostream`；
- Reader 的默认类型描述是否与发布端兼容；
- 发布端和 Reader 的 Domain ID 是否相同；
- QoS 关键策略是否匹配；
- `_CREATED_BY_BARE_DDS_APP_` 端点是否属于另一个进程/领域配置；
- Topic 本身的视频字段是否真由当前进程发布。

以上均为**推测/待验证**，不能写成已确认根因。

### 6.3 编译依赖

最小 CMake 需要：

```cmake
target_include_directories(camera_dds_test PRIVATE
    /usr/local/include/ddscxx
    /home/unitree/unitree/unitree_sdk2/include
)

target_link_directories(camera_dds_test PRIVATE
    /usr/local/lib
)

target_link_libraries(camera_dds_test PRIVATE
    ddscxx
    ddsc
    pthread
)
```

这只能说明 Reader 程序能构建和创建对象；接收数据没有得到验证。

## 7. 三条相机路线对照

| 方案 | 数据源 | 已验证状态 | 适用场景 | 当前建议 |
|---|---|---|---|---|
| ROS 2 Topic `/frontvideostream` | DDS 发布端点 | 话题可发现；本地 CLI 无法解析类型；没有成功订阅真实帧 | 需要把图像接入现有 ROS 2 图、发布检测结果、与其他节点共享坐标/TF | **暂不作为主线**，先补消息包、类型和 QoS 证据 |
| 原生 Cyclone DDS Reader | `rt/frontvideostream` 尝试 | Reader 建立，0 帧 | 无 ROS 2 依赖的轻量取流 | **暂不作为主线**；下一次先做 endpoint/type 匹配诊断 |
| Unitree SDK `VideoClient` | `videohub` 服务，API ID 1001 | 已取到 JPEG，OpenCV 解码为 1920×1080 | 直接获取相机图像、快速验证检测 | **当前已验证可用路线** |

注意：SDK 取到的 JPEG 和 `/frontvideostream` 的真实 DDS Topic 是两条路线。不能因为 SDK 成功就说 `/frontvideostream` 已打通，也不能因为 DDS Reader 没收到帧就否定 SDK 取图。

## 8. 视频服务观察

历史终端曾看到两个进程：

```text
/unitree/module/video_hub_pc4/videohub_pc4_chest /dev/video10
/unitree/module/video_hub_pc4/videohub_pc4 /dev/video4
```

这说明：

- videohub/相关服务在测试时运行。
- `/dev/video4` 和 `/dev/video10` 至少分别被两个服务使用。
- 不应在未确认影响前停止、重启这些服务。

## 9. 下次连接 G1 的诊断顺序

1. `ros2 topic info -v /frontvideostream` 记录 QoS、GID、节点名。
2. 确认当前 `RMW_IMPLEMENTATION`、`CYCLONEDDS_URI`、`ROS_DOMAIN_ID` 与发布端设置。
3. 从 SDK 头文件或示例确认 `videohub` API ID 1001 的请求字段、返回码和图像编码。
4. 先用 `VideoClient` 保存一帧并确认 `imdecode`，再考虑 DDS 或 ROS 2 封装。
5. 对原生 Reader 打印 discovery/匹配状态和 Topic 实际名称；不要反复运行同一个 `take()` 等待循环。

