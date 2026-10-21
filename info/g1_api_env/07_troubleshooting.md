# 07 故障与排查记录

本文按“现象 → 已做排查 → 结果 → 当前结论 → 后续验证”整理。标注为“尚未确认”的问题不能按推测的根因直接修改系统。

## 1. ROS 2 找不到 `unitree_go` 消息包

### 现象

```bash
ros2 topic info /frontvideostream
# Type: unitree_go/msg/Go2FrontVideoData

ros2 interface show unitree_go/msg/Go2FrontVideoData
# Unknown package 'unitree_go'
```

随后 `ros2 topic hz` 因 Python 无法导入 `unitree_go` 失败。`ros2 pkg list | grep unitree` 没有结果。

### 已做排查

1. 检查 `AMENT_PREFIX_PATH`、`PKG_CONFIG_PATH`、`CMAKE_PREFIX_PATH`、`COLCON_PREFIX_PATH`。
2. 搜索 `/home/unitree` 下的 `unitree_go`、`unitree_ros2`、`unitree_hg` 目录。
3. 找到：

   ```text
   /home/unitree/unitree/unitree_sdk2_python/unitree_sdk2py/idl/unitree_go
   /home/unitree/unitree/unitree_sdk2_python/unitree_sdk2py/idl/unitree_hg
   ```

4. 找到 C++ DDS 生成头：

   ```text
   /home/unitree/unitree/unitree_sdk2/include/unitree/idl/go2/Go2FrontVideoData_.hpp
   ```

5. 检查 `ws_livox_foxy`、`ws_loc_foxy`、`unitree` 下的 `package.xml`，没有看到已安装的 `unitree_go` ROS 2 包。

### 实际结果

- 话题仍然可被 ROS 2 发现。
- DDS 类型在 SDK 中存在。
- 当前 shell 没有可用 ROS 2 接口包导入路径。
- SDK Python IDL 和 ROS 2 消息模块不是同一件事。

### 当前结论

**根因尚未确认。** 可以确认的是“消息包未在当前 Ros2 CLI 环境中可导入”，不能确认“机器人完全没有消息定义”。

### 后续验证

1. 检查发布端进程加载的工作空间和 `LD_LIBRARY_PATH`。
2. 找到 `unitree_go` 的 `.msg`/`.idl`、`package.xml` 和构建前缀。
3. 判断是否需要单独构建 ROS 2 接口包，还是在别的 overlay 中 source。
4. 先不改运行中的视频服务。

## 2. DDS Reader 没有收到相机帧

### 现象

原生 C++ Reader：

```text
[DDS] Creating participant...
[DDS] Subscriber created.
[DDS] Waiting for camera frames...
```

长时间无输出，手动 Ctrl+C。

### 已做排查

- 选用现有 `Go2FrontVideoData_` 生成类型。
- 尝试 Topic 名 `rt/frontvideostream`。
- 显式使用 `/home/unitree/cyclonedds_ws/cyclonedds.xml`。
- 检查 `RMW_IMPLEMENTATION=rmw_cyclonedds_cpp`。
- 检查 `ros2 pkg list | grep '^rmw_'`。
- 用 `ldd` 确认 Reader 链接 `libddscxx.so.0` 和 `libddsc.so.0`。
- 运行 Reader，没有出现异常，也没有收到数据。

### 实际结果

| 检查 | 结果 |
|---|---|
| Participant 构造 | 成功 |
| Subscriber 构造 | 成功 |
| DataReader 构造 | 成功 |
| 收到 sample | 0 |
| DDS 异常 | 未观察到 |
| Topic 实际底层名称 | 尚未确认：只尝试了 `rt/frontvideostream` |
| QoS/类型匹配状态 | 未取得诊断证据 |

### 当前结论

**尚未确认**根因。已经可以排除“简单的 ROS 2 Fast DDS 与程序 Cyclone DDS 混用”这一解释，但 Topic 名、类型描述、Domain、QoS、端点匹配和配置注入仍需分别核实。

### 后续验证

- 不重复运行同一个等待循环。
- 打开局部 Cyclone DDS 日志，观察 endpoint match。
- 从发布端/节点发现信息确认 Topic 实际名称。
- 打印 Reader/Publisher 的 type name、Domain、QoS。
- 保留现有 `eth0` 绑定；若需改变，先记录并准备回滚。

## 3. DDS 网卡配置错误

### 现象

```text
enp3s0: does not match an available interface.
rmw_create_node: failed to create domain
```

### 实际结果

- 早期 `CYCLONEDDS_URI` 明确指定 `enp3s0`。
- G1 的 `ip -br addr` 中没有 `enp3s0`。
- G1 有用接口是 `eth0=192.168.123.164/24`。
- 后来发现的 XML 指定 `eth0`。

### 当前结论

早期 `enp3s0` 配置与 G1 当前接口不匹配，这是错误产生的直接关联。  
但 `enp3s0` 原本服务于哪台机器、哪一个跨机链路，**尚未确认**。

### 后续验证

确认配置来源和设计目的后，再决定是否在 G1 上继续使用 `eth0` 的 XML。不要把开发机网卡名和机身网卡名混用。

## 4. VideoClient 编译找不到 `dds/dds.hpp`

### 现象

```text
fatal error: dds/dds.hpp: No such file or directory
```

来源路径：

```text
unitree_sdk2/include/unitree/common/dds/dds_entity.hpp:4
```

### 实际修复

在 CMake include 中加入：

```cmake
/usr/local/include/ddscxx
```

同时链接：

```cmake
ddscxx
ddsc
pthread
```

重新编译：

```text
[100%] Linking CXX executable video_client_test
[100%] Built target video_client_test
```

### 当前结论

**已确认并解决**：缺少 Cyclone DDS C++ 头文件 include 路径。

## 5. 相机 SDK 能取图，但实时检测没有框

### 现象

`GetImageSample()` 成功，JPEG 可解码为 `1920x1080`，TensorRT 推理完成，但：

```text
[Detection] Final boxes: 0
```

### 已做排查

- 检查 TensorRT 输出非空且全部有限。
- 诊断最高类别分数。
- 三次实时运行中最高分数分别为约 0.0781、0.0037（其他一次未打印诊断）。
- 没有为了让框出现而降低阈值。
- 决定改用 Python 已验证成功的同一张原始图片做对照。

### 当前结论

**根因尚未确认。** 至少可以确认：

- 相机和 JPEG 解码成功；
- Engine 执行成功；
- 8400 个候选输出中确实没有超过 0.25 的分数；
- 当前实时画面对模型的响应很低。

可能原因：

1. 相机画面没有四个训练类别的目标；
2. 角度、距离、光照、运动模糊导致分数低；
3. C++ 预处理与模型训练/Python 预处理不同；
4. 输出布局或类别分数解释不完全一致；
5. 模型本身对当前场景泛化不足。

这些是**推测方向**，不能直接判定根因。

### 后续验证

优先做同图对照，再检查输入张量、原始输出和 Python 结果。短时间内重复拍不同实时帧没有帮助。

## 6. Python 与 C++ 可能不一致

### 已经对齐的项目

- 模型输入 `640x640`；
- 输出 `(1,8,8400)`；
- 4 个类别 + 4 个框参数；
- 置信度阈值 `0.25`；
- C++ BGR→RGB、CHW、`/255`；
- C++ 按类别 NMS，IoU `0.45`。

### 尚未核对的项目

- Ultralytics 对同一原图的精确 letterbox 规则是否与 C++ 完全一致；
- Python 使用的模型预处理是否包含额外的均值/方差；
- C++ 通道排序 `4..7` 与 Python 类别顺序是否逐类对齐；
- 两者对原始输出的阈值筛选时机是否一致；
- NMS 的坐标取整、框宽高和类别抑制规则是否一致；
- 相机 JPEG 与 Python 固定图片的 JPEG 解码色彩管理是否一致。

### 当前结论

**待验证。** 只有在同一原始图片、同一 Engine 和相同阈值下比较，才能得出结论。

## 7. 不同相机帧的置信度大幅变化

### 观测

最高类别分数从 `0.078074` 变为 `0.00366318`。这不是同图对照，而是两次不同的实时画面。

### 解释

实时相机每一帧的内容、角度、光照、人物距离都可能不同，因此分数变化本身不证明程序有 bug。

### 当前结论

**已观察到现象，根因未确认。** 需要先让输入固定，再判断模型和实现是否稳定。

## 8. `trtexec --version` 最后显示 `FAILED`

### 现象

命令打印 `TensorRT v8502`，之后提示缺少模型或模型格式无法识别，最终出现 `FAILED`。

### 当前结论

**不是 TensorRT 安装失败。** 这个版本的 `trtexec` 仍把 `--version` 当成需要模型参数的普通运行路径；后续已有实际 Engine 构建成功的证据。

### 后续验证

读取完整日志的第一段版本行，或直接使用已验证的 `--onnx ... --buildOnly` 命令，不要反复尝试同一个 `--version` 失败路径。

## 9. 锁频和 Spin Wait 的数字缺失

### 现象

用户报告锁频和 Spin Wait 后“更好了”，但共享对话没有粘贴完整测试输出。

### 当前结论

只能确认尝试过优化，不能确认性能提升幅度。`trtexec` 构建日志里的 `Spin-wait: Disabled` 是构建阶段配置信息，不等于后来优化测试的命令。

### 后续验证

固定 Engine、输入、warm-up、时长、电源状态和温度采集，分别保留优化前/后日志；不要只记录“更好”。

## 10. 已知但是无效/低收益的重复做法

以下动作已经证明不足以解决问题，除非引入新的证据或参数：

- 重装/升级 Ubuntu、ROS 2、CUDA、TensorRT；
- 看到一个话题就改 DDS 网卡名；
- 直接在 Robot 上停止/重启 videohub 服务；
- 反复运行同一个原生 DDS Reader 等待循环；
- 为了“有框”不断降低置信度阈值；
- 在实时画面上反复试不同帧，而不固定输入；
- 把 `nvidia-smi` 不存在当成 Jetson GPU 不可用；
- 把 `Go2FrontVideoData` 名字当成 G1 硬件型号；
- 把 ROS 2 `topic list` 当成消息包已安装。

