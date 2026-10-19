### 🔌 第一步：连接 G1 EDU（SSH）

G1 内部 PC 的默认有线网络信息是固定的：

| 项目 | 默认值 |
|---|---|
| G1 IP | `192.168.123.164` |
| 用户名 | `unitree` |
| 密码 | `123` |

**操作**：把自己的电脑以太网口设成同网段，例如 `192.168.123.222 / 255.255.255.0`，然后：

```bash
ssh unitree@192.168.123.164
```

登录后，G1 内部通常已经预置了环境选择菜单。**在终端里输入 `1` 并回车，可以进入 ROS Foxy 环境**（G1 EDU 默认的 ROS 2 版本）。

---

### 💻 第二步：查看系统硬件（CPU / 内存 / 磁盘 / 网络）

G1 EDU 的基础算力是 **8 核高性能 CPU**（通常为瑞芯微 RK3588），EDU 版本额外搭载 **NVIDIA Jetson Orin NX（16GB / 100 TOPS）** 作为高算力模组。

| 想看什么 | 指令 | 说明 |
|---|---|---|
| CPU 型号与核心 | `lscpu` | 确认是 RK3588 还是 Orin NX 在对外服务 |
| 内存 | `free -h` | 看总内存（Orin NX 版本通常 16GB） |
| 磁盘分区 | `lsblk` | 看 eMMC / NVMe 容量 |
| 所有硬件概要 | `lshw -short` | 需先 `sudo apt install lshw` |
| 全面系统报告 | `inxi -Fxz` | 需先 `sudo apt install inxi` |
| USB 设备（相机等） | `lsusb` | **确认 RealSense 是否被识别** |
| PCI 设备 | `lspci` | 看网卡、Wi-Fi 模块 |
| 网络接口与 IP | `ip addr` | 确认有线 / Wi-Fi 状态 |
| 串口设备 | `ls /dev/ttyACM* /dev/ttyUSB*` | 看是否有底层控制串口 |

---

### 📷 第三步：查看 Intel RealSense 深度相机（D435i）

G1 EDU 头部集成的深度相机通常是 **Intel RealSense D435i**。

**① 确认 USB 识别**

```bash
lsusb | grep -i Intel
```

如果看到 `Intel(R) RealSense(TM) Depth Camera`，说明系统已经认到相机。

**② 确认 RealSense 底层库**

```bash
rs-enumerate-devices
```

这个命令来自 `librealsense2`，可以列出相机型号、序列号、固件版本和所有支持的流配置。如果报“command not found”，说明驱动还没装。

**③ 通过 ROS 2 查看图像（如果 G1 上已经有 RealSense ROS 包）**

RealSense 的 ROS 2 包名是 `realsense2_camera`。可以先查包是否存在：

```bash
ros2 pkg list | grep realsense
```

如果存在，可以直接启动相机节点，然后在 RViz2 里看图像；或者用 `ros2 topic` 查看话题（话题名取决于 launch 配置，常见为 `/camera/camera/color/image_raw` 等）。

**④ 如果没有 ROS 包，想快速验证**

可以用 Python 直接测试（G1 上需要 `pyrealsense2`）：

```bash
python3 -c "
import pyrealsense2 as rs
ctx = rs.context()
for dev in ctx.query_devices():
    print(dev.get_info(rs.camera_info.name), dev.get_info(rs.camera_info.serial_number))
"
```

这会直接列出 RealSense 的型号和序列号，不依赖 ROS。

---

### 📡 第四步：查看 3D 激光雷达（Livox MID-360）

G1 EDU 头部搭载的是 **Livox MID-360 3D 激光雷达**，水平 360°、垂直最大 59° 视场。

Livox 雷达通常走 **以太网** 通信。G1 内部有独立的网段（如 `192.168.123.x`），雷达可能挂在这个网段或另一个专用网段上。

**① 查看网络接口**

```bash
ip addr
```

确认是否有多个网口 / 多个 IP 段，Livox 通常在一个独立网段。

**② 确认雷达是否在线（如果知道雷达 IP）**

Livox MID-360 的默认 IP 通常是 `192.168.1.1xx` 段，但 G1 上的具体配置可能不同。可以先看路由表：

```bash
ip route
```

或者用 Livox 官方工具 `livox_ros_driver2` 的状态提示（如果 G1 上装了驱动）。

**③ 通过 ROS 2 查看点云话题**

Livox 驱动的 ROS 2 话题通常是：

```text
/livox/lidar    （PointCloud2）
/livox/imu      （Imu，MID-360 自带 IMU）
```

查看话题是否存在、频率是否正常：

```bash
ros2 topic list | grep livox
ros2 topic hz /livox/lidar
ros2 topic echo /livox/lidar --once
```

如果话题不存在，说明 Livox 驱动没有启动，需要先启动 `livox_ros_driver2` 的 launch 文件。

---

### 🤖 第五步：通过 ROS 2 验证所有传感器数据流

无论传感器走什么底层协议，最终都可以在 ROS 2 层面统一检查。

| 目的 | 指令 | 说明 |
|---|---|---|
| 列出所有话题 | `ros2 topic list` | 先看整体 |
| 过滤传感器话题 | `ros2 topic list \| grep -E 'imu\|scan\|cam\|lidar\|livox'` | 快速定位 |
| 查看话题类型与 QoS | `ros2 topic info <话题> -v` | **重点看 Reliability 是否匹配** |
| 查看发布频率 | `ros2 topic hz <话题>` | 图像通常 30Hz，点云 10Hz，IMU 100Hz+ |
| 查看一帧数据 | `ros2 topic echo <话题> --once` | 检查 `header.stamp` 和 `frame_id` |
| 列出所有节点 | `ros2 node list` | 看驱动节点是否在跑 |

**G1 底层状态也可以通过 ROS 2 / DDS 直接读取**（如果用了 `unitree_ros2` 或 SDK2）：

```bash
ros2 topic echo /rt/lowstate --once
```

里面包含 `IMUState_`（四元数、陀螺仪、加速度、RPY、温度）和所有电机的状态。

---

### 🛠️ 第六步：查看开发套件（SDK / ROS 2 包）

G1 EDU 的二次开发主要围绕两个东西：

**① `unitree_sdk2`（C++ 底层 SDK）**

如果 G1 上已经克隆或安装了 SDK2，可以确认目录：

```bash
ls ~/unitree_sdk2
```

或者查找库文件：

```bash
find / -name "libunitree_sdk2*" 2>/dev/null
```

**② `unitree_ros2`（官方 ROS 2 功能包）**

`unitree_ros2` 让 ROS 2 直接使用 Unitree DDS 消息，推荐 **Ubuntu 22.04 + Humble**，底层走 **Cyclone DDS**。G1 EDU 自带的是 Ubuntu 20.04 / Foxy，所以官方推荐环境与 G1 板载环境存在版本差异；如果要在 G1 本机上使用，需要确认 Foxy 下的兼容性，或者通过外部开发机（Humble）对接。

检查是否已安装：

```bash
ros2 pkg list | grep unitree
```

如果看到 `unitree_hg`、`unitree_go`、`unitree_api` 等包，说明 ROS 2 接口已经就绪。

**③ 查看 DDS 实现**

Unitree 的底层通信默认使用 **Cyclone DDS**。查看当前 RMW：

```bash
echo $RMW_IMPLEMENTATION
```

如果输出 `rmw_cyclonedds_cpp`，说明 Cyclone DDS 已生效。

---

### ⚠️ 关键注意事项

| 坑 | 说明 |
|---|---|
| **G1 的 Ubuntu 是 20.04 / Foxy** | 官方 `unitree_ros2` 推荐 Humble（22.04），直接在 G1 本机编译可能遇到依赖版本问题；常见做法是在外部 Humble 开发机上对接 G1 的 DDS |
| **RealSense 和 LiDAR 的 `frame_id` 不同** | 相机通常在 `camera_link` 或 `camera_color_optical_frame`，雷达在 `livox_frame`。融合前必须用 tf2 统一坐标系 |
| **QoS 不匹配会导致“看得到话题、收不到数据”** | 传感器驱动常用 `BEST_EFFORT`，订阅时用 `SensorDataQoS()` 或显式设 `BEST_EFFORT` |
| **G1 的“头部相机”可能不在 `/dev/video*`** | 有讨论提到头部相机可能走独立网络（如 `192.168.123.161` 是运动控制单元，不是相机）；RealSense 一般走 USB，先用 `lsusb` 确认 |
| **Livox 雷达不走 USB，走以太网** | 不要用 `lsusb` 找雷达；用 `ip addr` 看网段，再通过 ROS 话题验证 |
