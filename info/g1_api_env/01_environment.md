# 01 设备与软件环境

本文记录 G1-EDU 机载环境与开发机环境的已知基线。时间以分享链接中的日期和已有环境记录为准。

## 1. 两台设备先分清

| 环境 | 已确认信息 | 主要用途 |
|---|---|---|
| G1-EDU 机身计算平台 | ARM64 Jetson Orin 系列，Ubuntu 20.04.6，CUDA 11.4，TensorRT 8.5.2 | 相机服务、DDS、ROS 2、TensorRT Engine 构建与运行 |
| 开发机 | Ubuntu 24.04，Python 3.12.3，PyTorch 2.14.1+cu130，Ultralytics 8.4.183 | 训练模型验证、导出 ONNX、图片检测与结果对比 |

不能用 x86_64 开发机上的二进制、TensorRT Engine 或 Python 扩展直接替代 G1 的 ARM64 制品。

## 2. G1-EDU 硬件与系统

### 2.1 计算平台

| 配置项 | 实际值 | 作用/说明 | 证据状态与验证方式 |
|---|---|---|---|
| 机器人型号 | Unitree G1-EDU | 当前目标硬件 | **已验证成功**：用户明确确认；对话反复引用 |
| 计算模组 | NVIDIA Orin NX Developer Kit | 机载计算平台 | **已观察到**：已有 `hardware_info.md` 的系统记录；TensorRT 同时报告 `Selected Device: Orin` |
| CPU 架构 | `aarch64` / ARM64 | 决定二进制和 Engine 兼容性 | **已验证成功**：CUDA/OS 环境记录与 `trtexec` 设备信息 |
| CPU | 8 核 ARMv8，最高约 1.984 GHz | 通用处理 | **已观察到**：已有 `lscpu` 整理，非本次分享正文原样输出 |
| GPU | Orin，Compute Capability 8.7，8 SM，显存报告 15388 MiB | TensorRT 推理 | **已验证成功**：2026-10-21 Engine 构建日志 |
| GPU 时钟（构建设备信息） | 0.918 GHz | 当时报告值 | **已观察到**：`trtexec` 日志；不是锁频后性能数字 |
| 内存 | 约 15 GiB | 相机、TensorRT 和系统服务共用 | **已观察到**：已有 `free -h`/`lshw` 记录 |
| 主存储 | NVMe 约 1.9 TiB，根文件系统 ext4 | 系统和项目数据 | **已观察到**：已有硬件报告 |
| 内核 | `5.10.104-tegra` | Jetson 内核 | **已观察到**：已有系统报告 |
| L4T | R35.3.1 | Jetson Linux 基线 | **已观察到**：已有 `/etc/nv_tegra_release` 记录 |

### 2.2 网络

| 接口 | 观察值 | 说明 |
|---|---|---|
| `eth0` | UP，`192.168.123.164/24` | G1 机身有线接口；Cyclone DDS XML 绑定此接口 |
| `wlan0` | UP，曾观察 `10.25.125.22/24` | 动态地址，可能随网络变化 |
| `lo` | `127.0.0.1/8` | 本机回环 |
| `enp3s0` | 在 G1 当前接口列表中没有出现 | 早期 `CYCLONEDDS_URI` 使用了此名称，位置/用途存在冲突 |

`eth0` 的地址可能因现场网络配置变化。连接前应以 `ip -br addr` 重新确认。

## 3. 软件版本对照

### 3.1 G1 机身

| 软件 | 实际值 | 作用 | 状态/验证方式 |
|---|---|---|---|
| 操作系统 | Ubuntu 20.04.6 LTS | 系统基线 | **已观察到**：环境记录与对话引用 |
| ROS 2 | Foxy | 机器人通信 | **已验证成功**：`ros2 topic list/info` 可运行，路径为 `/opt/ros/foxy` |
| RMW | `rmw_cyclonedds_cpp` | ROS 2 使用 Cyclone DDS | **已验证成功**：`echo $RMW_IMPLEMENTATION` |
| Cyclone DDS | `0.10.2` | DDS/IDL C++ 基础库 | **已验证成功**：生成头文件标记 `Cyclone DDS: v0.10.2`；源码目录为 `cyclonedds-0.10.2` |
| CUDA Toolkit | 11.4，`V11.4.315` | CUDA 编译/运行 | **已验证成功**：已有 `nvcc --version` 记录 |
| TensorRT | 8.5.2，`trtexec` 报告 `v8502` | Engine 构建与运行 | **已验证成功**：Engine 构建日志 |
| TensorRT Python 运行库 | 观察到 `8.5.2.2` 的进度摘要 | Python Engine 测试 | **已观察到**：属于对话总结；C++ 构建日志明确为 8.5.2 |
| OpenCV C++ | 4.2.0 | JPEG 解码、缩放、NMS、画框 | **已验证成功**：CMake `Found OpenCV ... 4.2.0` |
| GCC/G++ | 9.4.0 | C++17 编译 | **已验证成功**：CMake 编译器识别输出 |
| CMake | 构建配置要求至少 3.16 | 项目构建 | **待验证**：对话只展示 `cmake_minimum_required(VERSION 3.16)`，未显示实际 `cmake --version` |
| Python | `python3` 可运行 | TensorRT Python 烟雾测试 | **待验证**：版本号未在共享页中显示 |
| `nvidia-smi` | `command not found` | 不是 Orin 上判断 GPU 可用性的必要条件 | **已验证成功**：命令不可用；不影响 CUDA/TensorRT 实测成功 |
| `trtexec` | `/usr/src/tensorrt/bin/trtexec`，不在默认 PATH | Engine 工具 | **已验证成功**：绝对路径可启动 |

### 3.2 开发机

| 软件/硬件 | 实际值 | 作用 | 状态 |
|---|---|---|---|
| 操作系统 | Ubuntu 24.04 | 模型验证与 ONNX 导出 | **已观察到** |
| CPU | 13th Gen Intel Core i7-13650HX | 开发机 CPU | **已观察到**：Ultralytics 启动行 |
| GPU | 文中上下文为 RTX 4070 | PyTorch/Ultralytics 推理 | **已观察到**：没有附完整 `nvidia-smi` 输出 |
| Python | 3.12.3 | 模型工具链 | **已验证成功**：Ultralytics 导出/检测输出 |
| PyTorch | 2.14.1+cu130 | Ultralytics 后端 | **已验证成功**：命令行输出 |
| Ultralytics | 8.4.183 | 自定义 YOLO 导出和检测 | **已验证成功**：命令行输出 |
| CUDA Toolkit | 未记录 | 与 `cu130` PyTorch wheel 不同概念 | **待验证** |
| TensorRT | 未记录 | 开发机是否安装未知 | **待验证** |
| OpenCV Python | 未记录 | 开发机图像处理 | **待验证** |

## 4. 实际路径

### 4.1 G1 机身

| 路径 | 内容/用途 | 状态 |
|---|---|---|
| `/home/unitree/unitree/unitree_sdk2` | Unitree C++ SDK 根目录 | **已观察到** |
| `/home/unitree/unitree/unitree_sdk2/lib/aarch64/libunitree_sdk2.a` | SDK 静态库 | **已观察到** |
| `/home/unitree/unitree/unitree_sdk2/include` | SDK 头文件 | **已观察到** |
| `/home/unitree/unitree/unitree_sdk2/example/go2/go2_video_client.cpp` | 视频客户端示例路径 | **已观察到**：命令引用；源码正文未在本次页面中完整返回 |
| `/home/unitree/unitree/unitree_sdk2/include/unitree/robot/go2/video/video_client.hpp` | `VideoClient` 头文件 | **已观察到** |
| `/home/unitree/unitree/unitree_sdk2/include/unitree/robot/go2/video/video_api.hpp` | 视频 API ID 头文件路径 | **已观察到**：对话检查清单提到 |
| `/home/unitree/unitree/unitree_sdk2/include/unitree/idl/go2/Go2FrontVideoData_.hpp` | 生成的 DDS C++ 类型 | **已验证成功**：类型内容被终端打印 |
| `/home/unitree/unitree/unitree_sdk2_python/unitree_sdk2py/idl/unitree_go` | Python IDL 目录 | **已观察到** |
| `/home/unitree/cyclonedds_ws` | 自定义 Cyclone DDS/RMW 工作空间 | **已验证成功** |
| `/home/unitree/cyclonedds_ws/cyclonedds.xml` | DDS 网卡绑定配置 | **已验证成功**：文件内容被打印 |
| `/usr/local/lib/libddsc.so` | Cyclone DDS C 库 | **已验证成功**：文件存在并被程序链接 |
| `/usr/local/lib/libddscxx.so` | Cyclone DDS C++ 库 | **已验证成功**：文件存在并被程序链接 |
| `/usr/local/include/ddscxx` | `dds/dds.hpp` 等头文件根 | **已验证成功**：加入 include 后编译通过 |
| `/home/unitree/robot-vision` | 视觉项目根目录 | **已验证成功** |
| `/home/unitree/robot-vision/models/custom/best.onnx` | 自定义 ONNX | **已观察到** |
| `/home/unitree/robot-vision/engines/best_fp32.engine` | TensorRT FP32 Engine | **已验证成功**：成功加载和执行 |
| `/home/unitree/robot-vision/cpp/robot_detector_cpp` | C++ 相机到 TensorRT 单帧工程 | **已验证成功**：构建并运行 |
| `/home/unitree/robot-vision/cpp/video_client_test` | VideoClient 最小单帧测试 | **已验证成功**：构建并运行 |
| `/home/unitree/robot-vision/cpp/camera_dds_test` | 原生 Cyclone DDS Reader 测试 | **已验证成功**：程序可运行；未收到帧 |
| `/home/unitree/robot-vision/cpp/trt_cpp_test` | 交接清单提到的 C++ TensorRT 测试目录 | **待验证**：本次页面无该测试输出 |
| `/home/unitree/robot-vision/outputs/cpp_detection_result.jpg` | 相机单帧检测结果图 | **已观察到**：程序报告保存成功 |
| `/home/unitree/robot-vision/outputs/video_client_test.jpg` | VideoClient 单帧保存路径 | **已观察到**：测试程序目标路径 |

### 4.2 开发机

| 路径 | 内容 | 状态 |
|---|---|---|
| `/home/mick/robot-vision` | 开发机视觉项目 | **已验证成功** |
| `models/custom/best.pt` | 自定义 PyTorch 权重 | **已验证成功** |
| `models/custom/best.onnx` | 导出后的 ONNX | **已验证成功** |
| `images/` | 本地测试图片 | **已观察到** |
| `outputs/result_0.jpg` | 官方模型结果图片 | **已验证成功** |
| `outputs/model_comparison/` | 草案对比脚本的输出目录 | **待验证**：未看到实际对比结果 |

ONNX 从开发机通过 `scp` 传到 G1；目标路径为 `/home/unitree/robot-vision/models/custom/best.onnx`。

## 5. 环境变量与 DDS 配置

| 变量/配置 | 观察值 | 作用 | 发生位置/上下文 |
|---|---|---|---|
| `RMW_IMPLEMENTATION` | `rmw_cyclonedds_cpp` | 让 ROS 2 使用 Cyclone DDS RMW | G1 机身终端 |
| 早期 `CYCLONEDDS_URI` | 绑定 `enp3s0` | 限定 Cyclone DDS 网络接口 | G1 早期会话；G1 无同名接口，导致 `does not match an available interface` |
| 后期 XML | `/home/unitree/cyclonedds_ws/cyclonedds.xml` 中绑定 `eth0` | G1 当前有线通信接口 | G1；与 `ip -br addr` 中 `eth0=192.168.123.164` 一致 |
| 显式运行环境 | `CYCLONEDDS_URI='file:///home/unitree/cyclonedds_ws/cyclonedds.xml'` | 让原生 DDS Reader 使用该 XML | G1；Reader 建成了，但未收到帧 |
| `AllowMulticast` | `spdp` | 允许 SPDP 发现组播 | G1 XML |

关于 `enp3s0` 与 `eth0` 的差异，已经确认的是：早期 shell 配置使用 `enp3s0`，而 G1 实际接口为 `eth0`；**尚未确认** `enp3s0` 原本是不是为开发机跨机器通信准备的。不要把两个值合并成“完整配置”，也不要未经负责人确认覆盖现有 XML。

## 6. 已验证的环境命令

```bash
# G1：ROS 2 / DDS
printenv | grep -Ei '^(ROS|RMW|CYCLONE|AMENT|COLCON|CMAKE_PREFIX_PATH|LD_LIBRARY_PATH|PATH)'
ros2 node list
ros2 topic list
ros2 topic info -v /frontvideostream

# G1：网络
ip -br addr
ip route

# G1：CUDA / TensorRT
command -v nvcc
nvcc --version
/usr/src/tensorrt/bin/trtexec --version

# G1：OpenCV / 编译器
pkg-config --modversion opencv4
gcc --version
g++ --version
cmake --version
```

`trtexec --version` 在该版本打印了 `TensorRT v8502`，随后因缺少模型最终报错；应结合前面的版本行判断工具可启动，不能只看末尾的 `FAILED`。

