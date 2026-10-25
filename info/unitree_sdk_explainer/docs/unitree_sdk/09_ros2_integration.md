# 09 · ROS 2 集成

> 结论先行：**这个 SDK 不是 ROS 2 包**。它直接使用 CycloneDDS C++ API，不依赖 `rclcpp`/`rmw`/`ament`；只附带了一小组 ROS 2 兼容 IDL 类型。想要 ROS 2 图，需要自己写桥接节点。
>
> 证据标记见 [README.md](README.md)。本文未安装 ROS 2，桥接节点因缺 `rclcpp` 未能编译，未做跨 RMW 实测。

---

## 1. 为什么不是 ROS 2 包

| 检查 | 结果 |
|---|---|
| `package.xml` | 仓库内 **0 个** |
| `COLCON_IGNORE` / `.repos` | 无 |
| `find_package(rclcpp/ament/...)` | CMake 与头文件中 **无** |
| 依赖 | SDK 目标只链接 `ddsc`、`ddscxx`、`Threads`；Eigen 为 `dds_wrapper` 头文件所需；Boost/yaml-cpp/fmt 仅部分示例可选使用 |
| 构建 | 普通 CMake，安装静态库 `libunitree_sdk2.a` |

【源码】`CMakeLists.txt:2,8-10,51-56`；`cmake/unitree_sdk2Targets.cmake` 中导入的目标只有 `ddsc`、`ddscxx`、`unitree_sdk2`。

因此：

- 不能 `colcon build` 这个 SDK。
- 不能在 launch 文件里直接把它当 ROS 2 节点。
- 需要把 SDK 链接进一个**普通 C++ 程序**，再由这个程序与 ROS 2 通信（或反过来）。

---

## 2. 附带的 ROS 2 兼容类型

`include/unitree/idl/ros2/` 提供以下生成类型（命名空间是**真正的 ROS 2 命名空间**）：

| 命名空间 | 类型 |
|---|---|
| `std_msgs::msg::dds_` | `Header_`、`String_` |
| `sensor_msgs::msg::dds_` | `Imu_`、`PointCloud2_`、`PointField_` |
| `geometry_msgs::msg::dds_` | `Point_`、`Point32_`、`PointStamped_`、`Quaternion_`、`QuaternionStamped_`、`Pose_`、`Pose2D_`、`PoseStamped_`、`PoseWithCovariance_`、`PoseWithCovarianceStamped_`、`Twist_`、`TwistStamped_`、`TwistWithCovariance_`、`TwistWithCovarianceStamped_`、`Vector3_` |
| `nav_msgs::msg::dds_` | `MapMetaData_`、`OccupancyGrid_`、`Odometry_` |
| `builtin_interfaces::msg::dds_` | `Time_` |

**没有**：

- `sensor_msgs/Image`
- `sensor_msgs/CameraInfo`
- `sensor_msgs/JointState`
- `sensor_msgs/LaserScan`
- `sensor_msgs/BatteryState`
- `tf2_msgs/TFMessage`
- `nav_msgs/Path`

【源码】目录清单与每个头文件的 `namespace` 声明；例如 `ros2/Imu_.hpp:20` 为 `namespace sensor_msgs`。

### 2.1 这些类型能直接给 `rclcpp` 用吗

不能直接当 ROS 2 消息用：

- 它们是 CycloneDDS 的 IDL-to-CXX 生成类（`...::msg::dds_::Imu_`），不是 `rosidl` 生成的 `sensor_msgs::msg::Imu`。
- `rclcpp::Publisher<T>` 需要 `rosidl_typesupport` 支持，而 SDK 没有提供对应的 type support。
- 可以在**纯 DDS 层**用它们发布/订阅；是否与 ROS 2 节点互通取决于类型名、CDR 布局、QoS、domain 是否完全一致。

【合理推断】由于类型名与 ROS 2 的 DDS 类型名一致（如 `sensor_msgs::msg::dds_::Imu_`），且 SDK 与 ROS 2 都可使用 CycloneDDS，**理论上**可在 DDS 层互通；但这不是 SDK 的承诺，**必须实测**。

---

## 3. Topic 命名与发现

### 3.1 命名

- Unitree 的状态/控制 Topic 是 `rt/lowstate`、`rt/lowcmd` 等。
- ROS 2 在 DDS 层也使用 `rt/<topic>` 前缀（由 RMW 添加）。
- 因此 ROS 2 工具可能**看到**名为 `rt/lowstate` 的 Topic，但没有对应类型支持时无法反序列化。

### 3.2 Domain 与网卡

| 项目 | Unitree SDK | ROS 2 |
|---|---|---|
| Domain | `ChannelFactory::Init(domainId, iface)`，示例多为 `0` | `ROS_DOMAIN_ID`，默认 0 |
| 网卡 | 显式传入（示例 `lo`/`eth0`） | `ROS_LOCALHOST_ONLY`、`CYCLONEDDS_URI` 等 |
| 中间件 | 直接 CycloneDDS | RMW（可能是 CycloneDDS/Fast DDS） |

要互通，三者必须一致：**domain、网卡/组播路由、DDS 实现或至少可互操作的 DDS**。

### 3.3 QoS

SDK 的 `DdsReaderQos`/`DdsWriterQos` 默认策略由库内 `InitPolicyDefault()` 设置（类声明见 `dds_qos.hpp:61,66`，该方法声明在 `:33`；实现位于预编译库，仓库无源码）。**默认 Reliability/History/Depth 未知**。

ROS 2 传感器 Topic 常用 `best_effort` + `keep_last(1)`；如果 SDK 端是 `reliable`，QoS 不兼容会导致**发现得到但收不到数据**。

桥接时必须：

1. 明确设置双方 QoS；
2. 先做最小 echo 测试；
3. 记录 QoS 不匹配的日志。

---

## 4. 三种桥接架构

### 4.1 A. 单进程：SDK + rclcpp

```text
┌─────────────────────────────────────────┐
│ 桥接进程                                 │
│  Unitree DDS 参与者 ── lowstate          │
│        │                                 │
│        ├─ 解析 hg::LowState_             │
│        └─ rclcpp::Publisher<Imu/JointState> │
│  ROS 2 参与者 ── /g1/imu, /g1/joint_states │
└─────────────────────────────────────────┘
```

优点：延迟低、部署简单。
缺点：一个进程内两套 DDS 参与者；需要同时链接 SDK 与 ROS 2；QoS/线程要自己协调。

### 4.2 B. 独立桥接进程 + 共享内存/管道

```text
SDK 采集进程 ──(共享内存/Unix socket)──> ROS 2 桥进程
```

优点：故障隔离；ROS 2 环境与 SDK 环境解耦。
缺点：多一次拷贝/序列化；需要定义进程间协议。

### 4.3 C. 纯 DDS 桥（不用 rclcpp）

直接用 CycloneDDS 发布 `sensor_msgs::msg::dds_::Imu_` 等类型，让 ROS 2 节点订阅。

优点：不依赖 ROS 2 编译环境。
缺点：需要精确匹配 ROS 2 的 DDS 类型名/QoS；调试困难；不是官方支持路径。

### 4.4 选型建议

| 场景 | 建议 |
|---|---|
| 只做数据记录/可视化 | A 或 B，优先 A |
| ROS 2 导航/感知栈 | A，桥接出标准 `Imu`、`JointState`、`Odometry`（需自算） |
| 安全关键控制 | 不要让 ROS 2 图成为唯一控制链；控制仍在 SDK 进程内 |
| 多机/分布式 | C 或 B，注意 domain 隔离 |

---

## 5. 一个最小桥接节点（示意，未验证）

以下代码需要 ROS 2 与 SDK 同时可用。本机未安装 ROS 2，编译尝试因缺 `rclcpp/rclcpp.hpp` 失败，属于环境不满足；未运行。只订阅 `rt/lowstate` 并发布 `/g1/imu` 与 `/g1/joint_states`，不发送任何控制。

```cpp
// g1_state_bridge.cpp — 需要 rclcpp + unitree_sdk2，未验证
#include <array>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <sensor_msgs/msg/joint_state.hpp>

#include <unitree/robot/channel/channel_factory.hpp>
#include <unitree/robot/channel/channel_subscriber.hpp>
#include <unitree/idl/hg/LowState_.hpp>

using LowState = unitree_hg::msg::dds_::LowState_;

class G1StateBridge : public rclcpp::Node {
public:
    G1StateBridge() : Node("g1_state_bridge") {
        imu_pub_ = create_publisher<sensor_msgs::msg::Imu>("/g1/imu", 10);
        joint_pub_ = create_publisher<sensor_msgs::msg::JointState>(
            "/g1/joint_states", 10);

        sub_ = std::make_shared<
            unitree::robot::ChannelSubscriber<LowState>>("rt/lowstate");
        sub_->InitChannel([this](const void* msg) {
            std::lock_guard<std::mutex> lock(mtx_);
            state_ = *static_cast<const LowState*>(msg);
            fresh_ = true;
        }, 1);

        timer_ = create_wall_timer(std::chrono::milliseconds(10),
                                   [this] { Publish(); });
    }

private:
    void Publish() {
        LowState s;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (!fresh_) return;
            s = state_;
            fresh_ = false;
        }

        const auto stamp = now();

        sensor_msgs::msg::Imu imu;
        imu.header.stamp = stamp;
        imu.header.frame_id = "pelvis";
        imu.orientation.w = s.imu_state().quaternion()[0];
        imu.orientation.x = s.imu_state().quaternion()[1];
        imu.orientation.y = s.imu_state().quaternion()[2];
        imu.orientation.z = s.imu_state().quaternion()[3];
        imu.angular_velocity.x = s.imu_state().gyroscope()[0];
        imu.angular_velocity.y = s.imu_state().gyroscope()[1];
        imu.angular_velocity.z = s.imu_state().gyroscope()[2];
        imu.linear_acceleration.x = s.imu_state().accelerometer()[0];
        imu.linear_acceleration.y = s.imu_state().accelerometer()[1];
        imu.linear_acceleration.z = s.imu_state().accelerometer()[2];
        imu_pub_->publish(imu);

        sensor_msgs::msg::JointState js;
        js.header.stamp = stamp;
        js.name.resize(29);
        js.position.resize(29);
        js.velocity.resize(29);
        js.effort.resize(29);
        for (int i = 0; i < 29; ++i) {
            js.name[i] = "joint_" + std::to_string(i);
            js.position[i] = s.motor_state()[i].q();
            js.velocity[i] = s.motor_state()[i].dq();
            js.effort[i] = s.motor_state()[i].tau_est();
        }
        joint_pub_->publish(js);
    }

    rclcpp::Publisher<sensor_msgs::msg::Imu>::SharedPtr imu_pub_;
    rclcpp::Publisher<sensor_msgs::msg::JointState>::SharedPtr joint_pub_;
    rclcpp::TimerBase::SharedPtr timer_;
    unitree::robot::ChannelSubscriberPtr<LowState> sub_;

    std::mutex mtx_;
    LowState state_;
    bool fresh_{false};
};

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    unitree::robot::ChannelFactory::Instance()->Init(0, ""); // 网卡按现场填
    auto node = std::make_shared<G1StateBridge>();
    rclcpp::spin(node);
    unitree::robot::ChannelFactory::Instance()->Release();
    rclcpp::shutdown();
    return 0;
}
```

注意：

- 关节名用 `joint_0..28` 只是占位；ROS 2 生态通常需要与 URDF 一致的真实关节名。
- 四元数顺序按 [03](03_message_types.md) 的推断（`w,x,y,z`）填入；ROS 2 `Imu` 也是 `w,x,y,z`。**必须实测确认**。
- 时间戳用 ROS 节点时钟，不是机器人时间；不要伪造硬件时间戳。
- 回调只复制，发布在 timer 线程；避免在 DDS 回调里调用 `rclcpp::publish`。
- 该节点没有发布 `Odometry`，因为 G1 的 `SportModeState_` 不含位姿（见 [03](03_message_types.md)）。
- 退出时先停止 timer/spin，再 `Release()`；示例的顺序仅为示意。

---

## 6. ROS 2 侧的常见缺口

| ROS 2 需要 | SDK 是否提供 | 替代方案 |
|---|---|---|
| `sensor_msgs/Image` | 否 | 外接相机 + 自己的图像发布节点 |
| `sensor_msgs/CameraInfo` | 否 | 相机标定参数自行发布 |
| `sensor_msgs/JointState` | 否（只有自定义 `LowState_`） | 桥接节点转换 |
| `sensor_msgs/LaserScan` | 否 | 外接雷达驱动 |
| `nav_msgs/Odometry` | 否（G1 无位姿状态） | VIO/定位系统 |
| `tf` | 否 | 自建 URDF/tf 树 |
| `sensor_msgs/BatteryState` | 否（BMS Topic 未确认） | 查官方接口或外接 |
| 标准 ROS 2 控制接口 | 否 | 自己封装 action/service |

---

## 7. 构建集成

SDK 提供 CMake 导入目标 `unitree_sdk2`（`cmake/unitree_sdk2Targets.cmake`）。桥接项目的 `CMakeLists.txt` 大致需要：

```cmake
find_package(unitree_sdk2 REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)

add_executable(g1_state_bridge src/g1_state_bridge.cpp)
target_link_libraries(g1_state_bridge
    unitree_sdk2
    rclcpp::rclcpp
    ${sensor_msgs_TARGETS})
```

【未验证】两套 DDS 的符号冲突、CMake 目标名、ROS 2 发行版差异都可能导致链接/运行问题；需要实际环境验证。

---

## 8. 故障排查

| 现象 | 可能原因 | 处理 |
|---|---|---|
| ROS 2 看不到 `rt/lowstate` | domain/网卡不同；ROS 2 只显示有类型支持的 Topic | 用 `ros2 topic list --include-hidden-topics`；检查 domain |
| 看得到但 `echo` 失败 | 没有自定义类型支持 | 写桥接节点，不要直接 echo |
| 桥接订阅无数据 | QoS 不匹配 | 显式设置 Reliability/History |
| 桥接发布无订阅者 | ROS 2 端 QoS/类型不匹配 | 用 `ros2 topic info -v` 查 QoS |
| 时间戳跳变 | 用了系统时间/机器人时间混用 | 统一时钟源 |
| 关节名对不上 URDF | 占位名 | 按 URDF 映射 |
| 两套 DDS 互相干扰 | 同 domain/同 Topic | 分 domain 或加命名空间 |
| 退出崩溃 | `Release()` 与 ROS 2 关闭顺序 | 先停 ROS spin，再关 SDK |

---

## 9. G1-EDU 部署能力划分

| 能力 | 归属 | 说明 |
|---|---|---|
| 用 ROS 2 记录/可视化 G1 状态 | 开发者适配 | 需桥接节点 |
| 用 ROS 2 导航栈控制 G1 | 开发者适配 | 无标准底盘接口/里程计 |
| 用 ROS 2 传图像 | 开发者适配 | SDK 无 Image 类型 |
| 用 SDK 原生 DDS 做实时控制 | SDK 原生 | 不经 ROS 2，延迟更可控 |
| 官方 `unitree_ros2` 包 | 未分析 | 不在本工作区，不能假设其 API |

---

## 10. 相关文档

- 消息类型：[03_message_types.md](03_message_types.md)
- 相机缺口：[07_sensors_and_camera.md](07_sensors_and_camera.md)
- 并发模型：[08_concurrency_and_lifecycle.md](08_concurrency_and_lifecycle.md)
- 构建：[11_build_and_link.md](11_build_and_link.md)
- 网络与 DDS：[12_network_and_deployment.md](12_network_and_deployment.md)
