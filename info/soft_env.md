# Unitree G1-EDU 机载计算平台软件与运行环境盘点

> **文档用途：**
> 团队共享、部署前环境评估、后续视觉/语音模型部署的基线记录。\
> **盘点日期：** 2026-10-20（根据本次会话中提供的终端输出整理）\
> **设备身份：** 用户确认是 **Unitree G1-EDU**。不要因为某个 ROS 2
> 消息类型名称包含 `Go2` 就将机器人认定为 Go2。\
> **证据范围：**
> 本文仅记录本次已提供的命令输出。未实际验证的组件会明确标注"未确认"，不会把"命令不存在"直接等同于"功能不可用"。

------------------------------------------------------------------------

## 1. 执行摘要

当前 G1-EDU 机载计算平台可确认的主要环境如下：

  ---------------------------------------------------------------------------------------------------------------------------------------
  项目                    当前盘点结果                                                       证据/备注
  ----------------------- ------------------------------------------------------------------ --------------------------------------------
  机器人型号              Unitree G1-EDU                                                     用户确认

  机载计算平台            NVIDIA Orin NX Developer Kit                                       `lshw -short`、`inxi -Fxz`

  CPU 架构                ARM64 / `aarch64`                                                  `lscpu`

  CPU                     8 核 ARMv8，报告最大频率约 1.984 GHz                               `lscpu`

  内存                    约 15 GiB                                                          `free -h`、`lshw`

  根存储                  NVMe，约 1.9 TB（系统报告约 1.86 TiB）                             `lsblk`、`inxi`

  操作系统                Ubuntu 20.04.6 LTS (Focal Fossa)                                   `inxi -Fxz`

  Linux 内核              `5.10.104-tegra`                                                   `inxi -Fxz`

  NVIDIA L4T              R35.3.1                                                            `/etc/nv_tegra_release`

  ROS 2                   ROS 2 Foxy 路径存在，命令可用                                      `/opt/ros/foxy`、`ros2 topic list`

  ROS 2 RMW               `rmw_cyclonedds_cpp`                                               环境变量

  Cyclone DDS             `/home/unitree/cyclonedds_ws/install/...`                          环境变量路径
  自定义工作空间                                                                             

  CUDA Toolkit            CUDA 11.4.315，`nvcc` 可运行                                       `/usr/local/cuda-11.4/bin/nvcc`

  TensorRT                相关 Debian 包版本 8.5.2，动态库可被系统 linker 找到               `dpkg-query`、`ldconfig`

  TensorRT `trtexec`      文件存在于 `/usr/src/tensorrt/bin/trtexec`，不在当前 `PATH`        `find` 与 `command -v`

  `nvidia-smi`            当前 shell 提示 command not found                                  不能据此判断 GPU 不可用

  视频设备节点            `/dev/video0` 至 `/dev/video5` 存在                                `ls -l /dev/video*`

  视频设备识别            这些节点均报告为 Intel RealSense Depth Camera 435i；节点分属两个   `udevadm info`；不能据此认定为六台独立相机
                          `ID_PATH`                                                          

  RealSense SDK/ROS 包    `rs-enumerate-devices` 不存在，`ros2 pkg list \| grep realsense`   是否有其他方式/包未确认
                          无输出                                                             

  Livox MID-360 相关 ROS  点云和 IMU 话题存在                                                `ros2 topic list`、`ros2 topic info`
  话题                                                                                       

  网络接口                `eth0`、`wlan0`、`usb0`、`rndis0`、`dummy0`、`l4tbr0`、`docker0`   `ip addr`
                          等                                                                 

  当前 DDS 配置           `CYCLONEDDS_URI` 指定 `enp3s0`                                     当前 `ip addr` 输出中没有 `enp3s0`

  DDS 诊断问题            `ros2 topic hz` 创建节点时报                                       现象已复现；配置来源/用途尚未确认
                          `enp3s0: does not match an available interface`                    

  Unitree ROS 2           `ros2 pkg list \| grep unitree`                                    需进一步确认 overlay、包名和消息接口来源
  包可发现性              无输出；但系统正在发布多个机器人相关话题                           
  ---------------------------------------------------------------------------------------------------------------------------------------

### 当前最重要的结论

1.  这台设备是 **ARM64 的 Jetson Orin NX 机载平台**，不能直接套用 x86_64
    Ubuntu 电脑上的二进制包。
2.  设备上已经有 CUDA 11.4 和 TensorRT 8.5.2
    相关组件。部署前应先做兼容性和运行测试，**不要先重装或升级
    CUDA/TensorRT**。
3.  ROS 2 Foxy 与 Cyclone DDS
    已在当前环境中使用，机器人相关话题正在被发现。当前 `ros2 topic hz`
    报错与 `CYCLONEDDS_URI` 中指定的 `enp3s0` 不匹配有关。
4.  `enp3s0` 配置由团队中的 DS
    要求配置。**不要擅自删除或改写该配置**。应先确认它的设计目标、原本部署在哪台机器、是否面向跨机器通信，以及当前终端为何看不到该接口。
5.  摄像头、Livox
    话题和模型运行链路仍需分开验证。话题存在不代表已确认原始图像格式、帧率、模型推理可用性或实时性能。

------------------------------------------------------------------------

## 2. 硬件与系统

### 2.1 计算平台与 CPU

来自 `lshw -short` 的系统描述：

-   System：`NVIDIA Orin NX Developer Kit`
-   CPU：8 个 ARMv8 处理器核心
-   架构：`aarch64`
-   `lscpu` 报告 CPU 最大频率约 `1984.0000 MHz`，最小频率约
    `115.2000 MHz`
-   CPU 特性中包含 ARM NEON/ASIMD 相关指令集标记

`lshw` 输出有普通用户权限警告，部分硬件信息可能不完整。

### 2.2 内存与交换空间

`free -h` 输出：

``` text
              total        used        free      shared  buff/cache   available
Mem:           15Gi       2.0Gi        11Gi        34Mi       1.3Gi        12Gi
Swap:         7.5Gi          0B       7.5Gi
```

解读：

-   系统可见内存约 15 GiB。
-   盘点时可用内存约 12 GiB。
-   `free -h` 显示 Swap 总量 7.5 GiB，当前使用为 0。
-   `lsblk` 同时显示多个约 961.8 MiB 的 `zram`
    设备；这与交换空间配置有关。不要仅根据 `free` 与 `lsblk`
    的显示方式推断它们是一一对应的独立物理磁盘。

内存余量是某一时刻的快照，运行摄像头、点云、语音模型或多个 ROS
节点后会变化。

### 2.3 存储

`lsblk` 显示：

-   主设备：`/dev/nvme0n1`
-   设备容量：约 `1.9T`
-   根分区：`/dev/nvme0n1p1`，挂载在 `/`
-   `inxi` 报告总存储约 `1.86 TiB`，当时已使用约 `25.50 GiB`

输出还列出多个小容量分区。**不要在未了解宇树系统分区用途前删除、格式化或调整这些分区。**

### 2.4 操作系统、内核与 Jetson Linux

已确认：

``` text
Ubuntu 20.04.6 LTS (Focal Fossa)
Kernel: 5.10.104-tegra
Architecture: aarch64
```

`/etc/nv_tegra_release` 输出：

``` text
# R35 (release), REVISION: 3.1, ... BOARD: t186ref, EABI: aarch64
```

因此，本次输出对应的 L4T 版本标识为 **R35.3.1**。

执行：

``` bash
dpkg-query -W -f='${Package} ${Version}\n' nvidia-jetpack
```

返回没有找到 `nvidia-jetpack`
元包。该结果**只说明查询时没有匹配到这个元包**，不能单凭它认定 JetPack
组件没有安装。CUDA/TensorRT 的实际包和动态库应分别检查。

### 2.5 GPU 工具状态

执行 `nvidia-smi` 返回：

``` text
-bash: nvidia-smi: command not found
```

这是当前系统中找不到该命令的证据，不是 GPU 不可用的证据。Jetson 平台的
GPU 状态和负载应使用适合 Jetson/L4T 的工具进一步检查。

当前尚未记录 GPU 实际利用率、显存/共享内存占用、功耗模式或温度测试结果。

------------------------------------------------------------------------

## 3. CUDA 与 TensorRT

### 3.1 CUDA Toolkit

命令：

``` bash
command -v nvcc
nvcc --version
```

结果：

``` text
/usr/local/cuda-11.4/bin/nvcc
Cuda compilation tools, release 11.4, V11.4.315
```

同时存在以下路径：

``` text
/usr/local/cuda -> /etc/alternatives/cuda
/usr/local/cuda-11 -> /etc/alternatives/cuda-11
/usr/local/cuda-11.4
```

**已确认：** CUDA Toolkit 11.4.315 的 `nvcc`
可执行文件存在并能输出版本。\
**尚未确认：** CUDA 示例是否能成功编译运行、CUDA runtime
是否可在当前用户下正常调用、GPU 推理是否成功。

### 3.2 TensorRT 软件包

`dpkg-query` 输出的相关版本包括：

  软件包                    版本
  ------------------------- --------------------
  `libnvinfer-bin`          `8.5.2-1+cuda11.4`
  `libnvinfer-dev`          `8.5.2-1+cuda11.4`
  `libnvinfer-plugin8`      `8.5.2-1+cuda11.4`
  `libnvinfer8`             `8.5.2-1+cuda11.4`
  `libnvinfer-plugin-dev`   `8.5.2-1+cuda11.4`
  `libnvinfer-samples`      `8.5.2-1+cuda11.4`

`libnvinfer-dev-cross-aarch64`、`libnvinfer-plugin-dev-cross-aarch64` 和
`libnvinfer-doc`
的查询结果没有显示已安装版本，不能把它们列为已安装组件。

### 3.3 TensorRT 动态库

`ldconfig -p` 能找到以下库：

``` text
/lib/aarch64-linux-gnu/libnvonnxparser.so.8
/lib/aarch64-linux-gnu/libnvonnxparser.so
/lib/aarch64-linux-gnu/libnvinfer_plugin.so.8
/lib/aarch64-linux-gnu/libnvinfer_plugin.so
/lib/aarch64-linux-gnu/libnvinfer.so.8
/lib/aarch64-linux-gnu/libnvinfer.so
```

这说明系统动态链接器缓存中存在 TensorRT 与 ONNX Parser
相关动态库。它不等同于已经成功加载模型或执行推理。

### 3.4 `trtexec` 的位置

当前 `PATH` 中找不到 `trtexec`：

``` bash
command -v trtexec
# 无输出
```

但文件搜索找到了：

``` text
/usr/src/tensorrt/bin/trtexec
```

因此可以先使用绝对路径查询版本：

``` bash
/usr/src/tensorrt/bin/trtexec --version
```

本次已提供的输出**没有包含该命令的实际运行结果**，所以暂时只能确认文件路径存在，不能确认它能正常启动。

### 3.5 部署建议

-   不要因为 `nvidia-smi` 不存在就安装桌面版 NVIDIA 驱动。
-   不要在未验证宇树软件依赖前升级 CUDA、TensorRT 或 L4T。
-   后续模型需要明确 ONNX opset、输入形状、数据类型、算子支持和 TensorRT
    engine 的生成条件。
-   TensorRT engine 通常与目标设备的软件栈和硬件相关；团队应保留原始
    ONNX 模型、构建日志、精度设置、输入尺寸和性能测试记录。
-   应先用无控制风险的独立测试模型完成推理验证，再考虑 ROS 2 集成。

------------------------------------------------------------------------

## 4. ROS 2 与构建环境

### 4.1 ROS 2 版本与路径

当前环境变量和错误日志显示 ROS 2 Foxy：

``` text
/opt/ros/foxy
```

错误日志也包含 Foxy 的 `rcl` 路径与版本信息。当前盘点应按 **Ubuntu
20.04 + ROS 2 Foxy** 记录，而不是把开发机上的 ROS 版本混为一谈。

已执行：

``` bash
ros2 topic list
```

该命令返回大量机器人相关话题，说明当前终端能够发现 ROS 2
图中的节点/话题。

### 4.2 RMW 与 Cyclone DDS

环境变量：

``` bash
RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
```

当前 shell 的 `CYCLONEDDS_URI` 为：

``` xml
<CycloneDDS>
  <Domain>
    <General>
      <Interfaces>
        <NetworkInterface name="enp3s0"
                          priority="default"
                          multicast="default" />
      </Interfaces>
    </General>
  </Domain>
</CycloneDDS>
```

当前环境还包含自定义 Cyclone DDS 工作空间路径：

``` text
/home/unitree/cyclonedds_ws/install/cyclonedds
/home/unitree/cyclonedds_ws/install/rmw_cyclonedds_cpp
```

相关环境变量包括：

-   `PKG_CONFIG_PATH`
-   `AMENT_PREFIX_PATH`
-   `CMAKE_PREFIX_PATH`
-   `COLCON_PREFIX_PATH`
-   `LD_LIBRARY_PATH`
-   `PATH`

其中 `AMENT_PREFIX_PATH` 同时包含自定义 RMW 工作空间和
`/opt/ros/foxy`；`LD_LIBRARY_PATH` 包含 CUDA 11.4、自定义 Cyclone
DDS/RMW 目录以及 ROS 2 Foxy 的库目录。

这说明当前终端不仅使用
`/opt/ros/foxy`，还叠加了自定义工作空间环境。团队复现问题时应保留这点。

### 4.3 当前 DDS 网络接口问题

`ip addr` 显示的主要接口包括：

  接口        盘点时地址/状态
  ----------- ---------------------------------------------------
  `eth0`      UP，`192.168.123.164/24`；另有 altname `enP8p1s0`
  `wlan0`     UP，`10.25.125.22/24`
  `usb0`      存在，当前 `NO-CARRIER` / DOWN 状态
  `rndis0`    存在，当前 `NO-CARRIER` / DOWN 状态
  `dummy0`    存在，DOWN
  `l4tbr0`    存在，DOWN
  `docker0`   存在，地址 `172.17.0.1/16`，当前链路 DOWN
  `lo`        回环接口

在该输出中没有看到名为 `enp3s0` 的接口。

执行：

``` bash
ros2 topic hz /utlidar/cloud_livox_mid360
ros2 topic hz /utlidar/imu_livox_mid360
```

报错：

``` text
enp3s0: does not match an available interface.
rmw_create_node: failed to create domain
```

**当前判断：** 报错与 Cyclone DDS 被配置为绑定
`enp3s0`、而当前系统接口列表中没有同名接口这一事实一致。

**重要约束：** 团队成员 DS 曾要求配置该 DDS 网卡，因此不要直接删除或把
`enp3s0` 替换为 `eth0`。在修改前需要确认： 1. 该配置本来是给
G1、开发机，还是另一台主机使用的； 2.
它要解决的是机器人本机节点通信，还是跨机器 ROS 2 通信； 3.
该配置从哪里注入（shell 配置、启动脚本、服务环境或远程会话）； 4.
期望通信的网段、网卡、组播设置和 ROS Domain 是否有团队约定。

此前在 `~/.config`、`/etc`、`/opt/ros/foxy` 的有限范围内搜索
XML/CONF/YAML，没有找到包含 `enp3s0`
的文件。这个搜索范围有限，不能证明配置一定不来自其他脚本或服务。

在明确设计意图前，不要为了单独运行 `ros2 topic hz` 而改动机器人当前 DDS
配置。

### 4.4 ROS 2 话题现状

`ros2 topic list` 中观察到的部分话题：

#### 机器人状态与控制相关

``` text
/lowstate
/lowcmd
/lowstate_doubleimu
/secondary_imu
/dog_imu_raw
/dog_odom
/odommodestate
/sportmodestate
/arm_sdk
/armsdk
/loco_sdk
/multiplestate
```

#### 视觉/视频相关

``` text
/frontvideostream
/videohub/inner
```

#### LiDAR / 点云 / IMU 相关

``` text
/utlidar/cloud_livox_mid360
/utlidar/imu_livox_mid360
/utlidar/range_info
/collision_clouds
/ele_clouds
/grid_clouds
/global_map
```

#### Unitree API 风格的话题

``` text
/api/action_store/request
/api/action_store/response
/api/arm/request
/api/arm/response
/api/audiohub/request
/api/audiohub/response
/api/config/request
/api/config/response
/api/loco/request
/api/loco/response
/api/motion_switcher/request
/api/motion_switcher/response
/api/robot_state/request
/api/robot_state/response
/api/sport/request
/api/sport/response
/api/videohub/request
/api/videohub/response
/api/voice/request
/api/voice/response
```

以上是 `ros2 topic list`
中出现的例子，并非完整列表。完整列表应以目标机器当前 `ros2 topic list`
的实际输出为准。

### 4.5 已检查的消息类型

执行：

``` bash
ros2 topic info /utlidar/cloud_livox_mid360
```

结果：

``` text
Type: sensor_msgs/msg/PointCloud2
Publisher count: 1
Subscription count: 1
```

执行：

``` bash
ros2 topic info /utlidar/imu_livox_mid360
```

结果：

``` text
Type: sensor_msgs/msg/Imu
Publisher count: 1
Subscription count: 0
```

执行：

``` bash
ros2 topic info /frontvideostream
```

结果：

``` text
Type: unitree_go/msg/Go2FrontVideoData
Publisher count: 1
Subscription count: 1
```

这里的 `Go2FrontVideoData` 是消息类型名称。它本身**不能推翻用户确认的
G1-EDU 设备身份**，也不能单凭名字推断实际硬件型号。

尝试查看消息定义：

``` bash
ros2 interface show unitree_go/msg/Go2FrontVideoData
```

返回：

``` text
Unknown package 'unitree_go'
```

同时：

``` bash
ros2 pkg list | grep unitree
```

没有输出。

这说明当前命令环境无法通过 `ros2` CLI
发现该包/接口。由于系统话题仍然存在，需进一步调查消息接口的来源、ROS
overlay、发布端环境和安装方式。不要直接据此断言发布端不存在或话题不可用。

------------------------------------------------------------------------

## 5. 摄像头与 USB 设备

### 5.1 视频设备节点

执行：

``` bash
ls -l /dev/video*
```

观察到：

``` text
/dev/video0
/dev/video1
/dev/video2
/dev/video3
/dev/video4
/dev/video5
```

这些节点均存在，权限显示为 `crw-rw-rw-`，属组为 `plugdev`。

### 5.2 设备识别结果

对 `/dev/video0` 至 `/dev/video5` 运行 `udevadm info`，得到的
`ID_MODEL`/`ID_VENDOR` 都指向：

``` text
Intel(R) RealSense(TM) Depth Camera 435i
```

但 `ID_PATH` 分成两组：

-   `/dev/video0` 至
    `/dev/video3`：`platform-3610000.xhci-usb-0:2.1:1.0`
-   `/dev/video4` 至
    `/dev/video5`：`platform-3610000.xhci-usb-0:2.1:1.3`

因此，不能把六个 `/dev/videoX`
节点直接计为六台独立摄像头。它们可能是不同视频接口、流或设备节点，需要用设备枚举工具进一步核实。

### 5.3 USB 拓扑

`lsusb` 观察到：

``` text
Intel Corp. ID 8086:0b3a
Realtek Semiconductor Corp. 4-Port USB 3.0 Hub
Realtek Semiconductor Corp. 4-Port USB 2.0 Hub
Linux Foundation USB root hubs
```

`lsusb -t` 显示 USB 3 总线下有一个 Intel Video Class 设备，多个接口由
`uvcvideo` 驱动；同时还有 HID 接口。USB 2 总线上出现 Realtek
无线/蓝牙相关接口。

### 5.4 RealSense 工具状态

以下命令当前不可用：

``` bash
rs-enumerate-devices
# command not found

v4l2-ctl --list-devices
# command not found
```

并且：

``` bash
ros2 pkg list | grep realsense
```

没有输出。

这只能确认这些命令/包在当前 shell
的可发现状态，不足以判断摄像头硬件无法工作。当前还没有完成： - RealSense
SDK 设备枚举； - 彩色图、深度图的实际采集； -
图像分辨率、帧率、像素格式验证； - 相机标定参数与 ROS 2 图像话题检查。

在机器人上安装额外包之前，应先与团队确认是否允许改动系统镜像和包环境。

------------------------------------------------------------------------

## 6. LiDAR 与 IMU

已发现相关 ROS 2 话题：

``` text
/utlidar/cloud_livox_mid360
/utlidar/imu_livox_mid360
/utlidar/range_info
```

当前查询结果：

-   `/utlidar/cloud_livox_mid360`：`sensor_msgs/msg/PointCloud2`，发布者数量
    1，订阅者数量 1。
-   `/utlidar/imu_livox_mid360`：`sensor_msgs/msg/Imu`，发布者数量
    1，订阅者数量 0。

注意：本次尝试测量频率时，`ros2 topic hz` 在创建节点阶段就因 Cyclone DDS
网卡配置报错，**所以目前没有得到可信的点云/IMU
发布频率数据**。不能将该失败解释为传感器没有发布数据。

后续需在不破坏既有 DDS 设计的前提下，验证： - 点云与 IMU
消息能否被独立订阅； - 实际发布频率、时间戳和 frame_id； -
点云字段与坐标系； - 相机、LiDAR、机器人 base frame 的 TF 关系； -
机器人运动时数据是否持续稳定。

------------------------------------------------------------------------

## 7. 网络与路由

### 7.1 网络接口与 IP

本次 `ip addr` 输出中关键接口：

  接口        IPv4 地址              状态/备注
  ----------- ---------------------- ----------------------------------
  `eth0`      `192.168.123.164/24`   UP，链路可用；altname `enP8p1s0`
  `wlan0`     `10.25.125.22/24`      UP，动态地址
  `usb0`      未显示 IPv4 地址       当前 NO-CARRIER
  `rndis0`    未显示 IPv4 地址       当前 NO-CARRIER
  `docker0`   `172.17.0.1/16`        当前链路 DOWN
  `lo`        `127.0.0.1/8`          回环

`ip route` 显示：

``` text
default via 10.25.125.111 dev wlan0 proto dhcp metric 600
default via 192.168.123.1 dev eth0 proto static metric 20100
10.25.125.0/24 dev wlan0 proto kernel scope link src 10.25.125.22 metric 600
169.254.0.0/16 dev eth0 scope link metric 1000
172.17.0.0/16 dev docker0 proto kernel scope link src 172.17.0.1 linkdown
192.168.123.0/24 dev eth0 proto kernel scope link src 192.168.123.164 metric 100
```

IP
地址、路由和接口状态可能因连接网络变化而变化。对外共享文档时，团队应评估是否需要隐藏
IP、MAC 地址或其他网络标识。

### 7.2 网络配置注意事项

-   当前系统输出没有名为 `enp3s0` 的接口；`eth0` 的 altname 是
    `enP8p1s0`，不是 `enp3s0`。
-   不要根据常见 Linux 网卡命名习惯猜测接口映射关系。
-   DDS 绑定接口前，先确认机器人实际通信网段和团队要求。
-   修改 `CYCLONEDDS_URI`
    之前，先保存当前值、查明来源，并获得配置负责人确认。
-   不建议在正在运行的机器人上通过重启服务、修改系统网络或 DDS
    配置来试错。

------------------------------------------------------------------------

## 8. 其他工具与软件状态

  -------------------------------------------------------------------------------------------------------------------------
  组件/命令                                                观察结果                       当前结论
  -------------------------------------------------------- ------------------------------ ---------------------------------
  `lscpu`                                                  可运行                         CPU 信息已采集

  `free -h`                                                可运行                         内存信息已采集

  `lsblk`                                                  可运行                         存储设备信息已采集

  `lshw -short`                                            可运行，但有普通用户权限警告   部分硬件信息可能不完整

  `inxi -Fxz`                                              可运行                         系统概况已采集

  `lsusb` / `lsusb -t`                                     可运行                         USB 设备和拓扑已采集

  `lspci`                                                  可运行                         PCI 设备信息已采集

  `ip addr` / `ip route`                                   可运行                         网络状态已采集

  `ros2 topic list`                                        可运行                         可发现机器人相关话题

  `ros2 topic info`                                        可运行                         可查看部分话题类型/端点数量

  `ros2 interface show unitree_go/msg/Go2FrontVideoData`   返回 Unknown package           当前环境不能发现该消息包

  `ros2 pkg list \| grep unitree`                          无输出                         当前 CLI 包列表未发现匹配包

  `rs-enumerate-devices`                                   command not found              当前命令不可用

  `v4l2-ctl`                                               command not found              当前命令不可用

  `nvcc`                                                   可运行，CUDA 11.4.315          CUDA Toolkit 已确认

  `nvidia-smi`                                             command not found              不可用该命令判断 GPU 状态

  `trtexec`                                                不在 PATH                      文件在
                                                                                          `/usr/src/tensorrt/bin/trtexec`
  -------------------------------------------------------------------------------------------------------------------------

------------------------------------------------------------------------

## 9. 已知问题、风险与待确认事项

### 9.1 高优先级：DDS 接口绑定

**现象：** `ros2 topic hz` 报
`enp3s0: does not match an available interface`。\
**相关配置：** `CYCLONEDDS_URI` 绑定 `enp3s0`。\
**已知背景：** DS 要求配置该网卡。\
**下一步：**
由配置负责人确认目标主机、目标网络和通信意图；追踪环境变量来源；先在获得授权的诊断环境中验证。\
**不要做：** 擅自删除 DS 配置、直接改成 `eth0`、重启机器人通信服务。

### 9.2 ROS 包/接口无法由当前 CLI 发现

**现象：** 话题类型是 `unitree_go/msg/Go2FrontVideoData`，但
`ros2 interface show` 报 `Unknown package 'unitree_go'`，`ros2 pkg list`
也未列出 `unitree` 包。\
**可能方向（尚未证实）：** 发布端与当前 CLI 使用了不同的环境
overlay、消息定义来自自定义工作空间，或当前终端没有 source
对应工作空间。\
**下一步：**
查明发布节点启动方式、安装前缀、消息定义文件和环境加载脚本。\
**不要做：** 仅凭类型名把机器人型号改判为 Go2。

### 9.3 摄像头工具未确认

**现象：** `/dev/video0`--`/dev/video5` 存在，但
`rs-enumerate-devices`、`v4l2-ctl` 不可用。\
**下一步：**
与系统维护者确认是否允许安装工具，再验证设备枚举与实际采图。\
**不要做：** 把六个节点直接计作六台相机，或把工具缺失判为摄像头故障。

### 9.4 TensorRT 实际推理尚未验证

**现象：** 相关包与动态库存在，`trtexec`
文件存在，但未记录成功执行的版本输出和模型推理测试。\
**下一步：** 在不影响机器人控制的情况下，使用独立测试 ONNX
模型验证解析、构建 engine、执行推理和性能。\
**不要做：** 直接把未验证的模型输出接到运动控制链路。

### 9.5 系统版本与软件升级

当前基线为 Ubuntu 20.04.6、L4T R35.3.1、CUDA 11.4、TensorRT 8.5.2、ROS 2
Foxy。升级可能影响驱动、CUDA/TensorRT、ROS
2、宇树自带节点和通信。升级前应由团队评估并准备回滚方案，不要在项目盘点阶段直接升级。

------------------------------------------------------------------------

## 10. 建议的后续验证清单

以下是待办事项，不代表已完成：

-   [ ] 记录
    `uname -a`、`cat /etc/os-release`、`cat /etc/nv_tegra_release`
    的完整输出。
-   [ ] 由 DS/系统负责人说明 `enp3s0`
    配置的用途、适用主机和预期通信网段。
-   [ ] 查清 `CYCLONEDDS_URI` 的设置来源：登录 shell
    配置、启动脚本、systemd 服务、容器或远程会话。
-   [ ] 在不修改原配置的前提下，设计可回滚的 DDS 诊断方案。
-   [ ] 获取 `ros2 node list`，确认相关发布节点名称。
-   [ ] 找到 `unitree_go/msg/Go2FrontVideoData` 的 `.msg`/IDL
    定义和实际安装前缀。
-   [ ] 查明 `/frontvideostream` 的数据封装格式与解码方法。
-   [ ] 在允许的情况下使用 RealSense
    设备枚举工具，确认实际相机数量、流类型和参数。
-   [ ] 验证 `/utlidar/cloud_livox_mid360`、`/utlidar/imu_livox_mid360`
    的频率、时间戳和 `frame_id`。
-   [ ] 运行 `/usr/src/tensorrt/bin/trtexec --version` 并保存完整输出。
-   [ ] 在独立模型上验证 TensorRT 推理，不接入机器人运动控制。
-   [ ] 记录 GPU、CPU、内存、温度、功耗和推理延迟的测试方法与结果。
-   [ ] 对团队外发版本审查 IP、MAC、主机名、账号名、路径和其他敏感信息。

------------------------------------------------------------------------

## 11. 建议的环境快照命令

在获得团队许可后，可使用下列只读命令补齐信息。运行前注意输出可能包含
IP、MAC、主机名、用户名和内部路径，不要未经审查直接公开。

``` bash
# 系统与架构
date
hostname
uname -a
cat /etc/os-release
cat /etc/nv_tegra_release
lscpu
free -h
lsblk

# Jetson / CUDA / TensorRT
command -v nvcc
nvcc --version
dpkg-query -W -f='${Package} ${Version}\n' 'libnvinfer*' 2>/dev/null
ldconfig -p | grep -E 'libnvinfer|libnvonnxparser'
/usr/src/tensorrt/bin/trtexec --version

# ROS 2 / DDS
printenv | grep -Ei '^(ROS|RMW|CYCLONE|AMENT|COLCON|CMAKE_PREFIX_PATH|LD_LIBRARY_PATH|PATH)'
ros2 node list
ros2 topic list
ros2 topic info /frontvideostream
ros2 topic info /utlidar/cloud_livox_mid360
ros2 topic info /utlidar/imu_livox_mid360

# 网络
ip -br addr
ip route

# 视频设备
ls -l /dev/video*
for d in /dev/video*; do
  echo "===== $d ====="
  udevadm info --query=property --name="$d" 2>/dev/null \
    | grep -E 'ID_V4L_PRODUCT|ID_MODEL=|ID_VENDOR=|ID_PATH='
done
```

建议把输出保存为带时间戳的文件，并在提交团队前去除不需要共享的标识信息。

------------------------------------------------------------------------

## 12. 版本与环境兼容性说明

这份清单描述的是 **G1-EDU 机载端**，不是开发笔记本，也不是 RTX 5090D 的
Windows 机器。团队后续部署时至少需要区分：

1.  **机器人机载端：** ARM64、Ubuntu 20.04.6、L4T R35.3.1、CUDA
    11.4、TensorRT 8.5.2 相关组件、ROS 2 Foxy。
2.  **开发机：** 必须单独记录操作系统、CPU
    架构、GPU、驱动、CUDA、Python、PyTorch、ONNX 工具链和 ROS 2 版本。
3.  **模型制品：** 记录模型文件哈希、来源、许可证、输入输出规格、ONNX
    opset、精度、TensorRT engine 构建环境与性能数据。

不要把 x86_64 上编译的二进制直接拷贝到 ARM64
机载端运行，也不要假设在较新 CUDA/TensorRT 版本构建的 engine
可以直接在当前机载端使用。

------------------------------------------------------------------------

## 13. 变更记录

  ------------------------------------------------------------------------------------------------------------
  日期                                记录
  ----------------------------------- ------------------------------------------------------------------------
  2026-10-20                          根据当前对话中提供的三份终端输出整理初版；未对机器人系统进行任何更改。

  待补充                              DDS 配置来源、摄像头实际采集、TensorRT 版本命令和推理基准、ROS
                                      消息定义来源。
  ------------------------------------------------------------------------------------------------------------

------------------------------------------------------------------------

**免责声明：**
本文是当前环境的观测记录，不是宇树官方支持声明，也不是系统升级指南。所有"未确认"项目都应通过实际命令、配置文件或负责人确认补齐。
