# Unitree G1-EDU 技术资料归档

本目录保存截至 **2026-10-22** 从 11 份历史对话中整理出的 G1-EDU 技术资料。内容覆盖机载环境、Unitree SDK 视频接口、DDS/ROS 2 通信、TensorRT、C++ 视觉检测和测试记录。

## 先看什么

| 场景 | 优先文档 |
|---|---|
| 登录 G1、确认版本与路径 | [01_environment.md](01_environment.md) |
| 获取相机 JPEG、查 VideoClient API | [02_unitree_sdk_api.md](02_unitree_sdk_api.md) |
| 查 ROS 2 Topic、DDS、QoS 和通信现状 | [03_dds_ros2_camera.md](03_dds_ros2_camera.md) |
| 构建 Engine、理解 TensorRT/CUDA 调用 | [04_tensorrt_deployment.md](04_tensorrt_deployment.md) |
| 修改或复核 C++ 检测程序 | [05_cpp_vision_pipeline.md](05_cpp_vision_pipeline.md) |
| 确认哪些测试真的做过 | [06_test_results.md](06_test_results.md) |
| 遇到无话题、无帧、无检测框 | [07_troubleshooting.md](07_troubleshooting.md) |
| 查类名、函数、命令和链接库 | [08_api_quick_reference.md](08_api_quick_reference.md) |

## 当前主线结论

- 当前硬件身份是 **Unitree G1-EDU**。SDK 命名空间和消息类型中的 `go2`/`Go2` 不能作为机器人型号判断依据。
- G1 机身侧已观察到 ARM64、Ubuntu 20.04.6、ROS 2 Foxy、CUDA 11.4、TensorRT 8.5.2、OpenCV 4.2.0。
- Unitree SDK `VideoClient::GetImageSample()` 已成功取得真实 JPEG；解码尺寸为 `1920x1080`。
- `/frontvideostream` 可由 ROS 2 发现，类型报告为 `unitree_go/msg/Go2FrontVideoData`，但当前 CLI 找不到 `unitree_go` ROS 2 接口包。
- 原生 Cyclone DDS C++ Reader 能建立 Participant/Subscriber/Reader，但一次实测没有收到相机帧；根因尚未确认。
- `best.onnx` 已转换为 `best_fp32.engine`，输入 `images` 为 `(1,3,640,640)`，输出 `output0` 为 `(1,8,8400)`。
- Engine 构建、Python 随机输入推理和 C++ 相机单帧推理均已实际执行。C++ 多次实时取帧的最终检测框数均为 0。
- **尚未完成 C++ 与 Python 对同一张已验证原图的对照**，因此现在不能判定零框来自相机画面、预处理还是后处理。
- 锁频和 Spin Wait 的性能测试被报告为“更好”，但共享对话中没有保留吞吐量、延迟分布或完整命令，不能填写精确性能数字。

## 证据标记

| 标记 | 含义 |
|---|---|
| **已验证成功** | 有实际程序输出、构建成功信息或文件读取结果 |
| **已观察到** | 在终端输出或对话中确实出现过，但用途、完整接口或根因未完全确认 |
| **推测** | 根据现象提出的解释，不能当作结论 |
| **待验证** | 共享对话中没有足够证据 |

## 证据范围与限制

本次读取的 11 个分享链接都可以访问。主要证据是分享页正文中保留的终端输出、代码和聊天摘要。

### 历史来源索引

按用户给定顺序读取：

| 编号 | 对话主题 | 链接 |
|---|---|---|
| S01 | 早期环境、DDS 网卡、视频话题与 TensorRT 基线 | <https://chatgpt.com/share/6ad8e740-5880-83ec-9f52-7bef84aefd9e> |
| S02 | 开发机 YOLO 初始检测、自定义 `.pt` 与 ONNX 导出 | <https://chatgpt.com/share/6ad8e78a-9bb0-83ec-a5d5-1f5a83b9f01d> |
| S03 | G1 ONNX 上传、TensorRT Engine 构建与性能初测 | <https://chatgpt.com/share/6ad8e79d-3834-83ec-b8f5-c6dd640c2119> |
| S04 | CUDA Runtime `ctypes` 与 Python TensorRT 随机输入测试 | <https://chatgpt.com/share/6ad8e7ac-a850-83ec-8ae6-e3246d1b6122> |
| S05 | ROS 2 工作空间、`unitree_go` 缺失、videohub 端点 | <https://chatgpt.com/share/6ad8e7c5-856c-83ec-ba88-11f5a5ef74a6> |
| S06 | `Go2FrontVideoData_` 定义、DDS XML 与最小 Reader 代码 | <https://chatgpt.com/share/6ad8e7d5-1a2c-83ec-8fc1-22c1e5a87929> |
| S07 | 原生 DDS Reader 未收帧、运行时 RMW/链接检查 | <https://chatgpt.com/share/6ad8e811-665c-83ec-8b2d-f32e0fc23597> |
| S08 | Unitree SDK `VideoClient` 最小取图工程与编译修复 | <https://chatgpt.com/share/6ad8e828-7414-83ec-9eef-f1b1670b837f> |
| S09 | C++ 相机取图、预处理、TensorRT、后处理和 0 框首测 | <https://chatgpt.com/share/6ad8e83c-0198-83ec-b516-7220d6e69f60> |
| S10 | 多次实时画面无框诊断与固定图片对照方案 | <https://chatgpt.com/share/6ad8e853-91d0-83ec-84ed-f9e144527bcd> |
| S11 | 整晚技术交接、路径/API 汇总与下一步优先级 | <https://chatgpt.com/share/6ad8e862-0de0-83ec-8472-7570d7c7bda0> |

当前开发电脑工作区中**没有 G1 的 `~/robot-vision` 源码、SDK 头文件或 Engine 文件**，所以无法在本机重新做源码行号级复核。文中的 C++ 内容来自历史对话展示的代码副本；它不自动等于 G1 上最后一次修改后的文件。下次连接 G1 后，应同步并核对：

- `/home/unitree/robot-vision/cpp/robot_detector_cpp/main.cpp`
- `/home/unitree/robot-vision/cpp/robot_detector_cpp/CMakeLists.txt`
- `/home/unitree/robot-vision/cpp/video_client_test/`
- `/home/unitree/robot-vision/cpp/trt_cpp_test/`
- `/home/unitree/robot-vision/cpp/camera_dds_test/`
- SDK 的 `video_client.hpp`、`video_api.hpp` 和 `Go2FrontVideoData_.hpp`

本目录还参考了工作区已有的 `../../info/unitree_info/hardware_info.md` 与 `soft_env.md` 来补齐 CPU、内存、内核和 L4T 信息；这些硬件输出没有原样出现在本次 11 个分享页正文中，文档中已与“11 个链接可见输出”区分。

## 下一步最高优先级

1. 将 Python 已检测成功的原始图片路径固定下来，让 C++ 读取同一文件。
2. 保存 Python 和 C++ 的预处理张量摘要、原始 Engine 输出、筛选后的检测框，做逐项比较。
3. 再查 `/frontvideostream` 的真实 DDS Topic 名称、类型描述和 Reader QoS；在没有证据前，不修改现有 `eth0` 绑定和视频服务。

