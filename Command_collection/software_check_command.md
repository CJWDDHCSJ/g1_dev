# 🛠️ 宇树 G1 开发环境版本校验清单 (Ubuntu 20.04 + Foxy)

> 💡 **核心原则**：Ubuntu 20.04 官方源中的 Python、CMake、GCC 等基础工具版本，是 ROS 2 Foxy 整个生态二进制兼容性的基准。**不建议随意升级**这些系统级工具，否则极易引发 ABI 不兼容的链接错误。优先使用 `apt` 安装库，而非 `pip`，能最大程度避免依赖冲突。

---

## 🐍 1. Python 版本

| 项目 | 说明 |
| :--- | :--- |
| **检查命令** | `python3 --version` |
| **Ubuntu 20.04 默认** | **Python 3.8.x** |
| **宇树 SDK 要求** | `unitree_sdk2py` 要求 **Python >= 3.8** |
| **绑定强度** | ⭐⭐ 中：满足最低版本即可，但需注意 `pip` 安装的包与系统 Python 3.8 的兼容性 |
| **建议** | 优先用 `apt install python3-xxx` 安装系统包；若需 `pip`，在 `venv` 虚拟环境中进行 |

---

## 🔨 2. CMake 版本

| 项目 | 说明 |
| :--- | :--- |
| **检查命令** | `cmake --version` |
| **Ubuntu 20.04 默认** | **CMake 3.16.3** |
| **ROS 2 Foxy 要求** | 通常只要求 `cmake_minimum_required(VERSION 3.5)` 及以上 |
| **绑定强度** | ⭐ 弱：无强版本绑定，默认版本完全满足 |
| **建议** | **不要升级** CMake，以免与 ROS 2 Foxy 的 `ament_cmake` 构建系统产生兼容性问题 |

---

## ⚙️ 3. GNU (GCC/G++) 版本

| 项目 | 说明 |
| :--- | :--- |
| **检查命令** | `gcc --version` / `g++ --version` |
| **Ubuntu 20.04 默认** | **GCC 9.4.0** |
| **ROS 2 Foxy 要求** | GCC 9 对 C++14/17 支持完整，满足 Foxy 与宇树代码需求 |
| **绑定强度** | ⭐⭐ 中：整个 ROS 2 Foxy 生态的二进制包基于此版本编译 |
| **建议** | **坚决不要升级** GCC，否则会导致 `libstdc++` ABI 不兼容，ROS 2 节点可能无法运行 |

---

## 👁️ 4. OpenCV 版本

| 项目 | 说明 |
| :--- | :--- |
| **检查命令** | `python3 -c "import cv2; print(cv2.__version__)"` |
| **Ubuntu 20.04 默认 (apt)** | **OpenCV 4.2.0** |
| **宇树/社区要求** | 通常接受 **4.x** 系列 |
| **绑定强度** | ⭐ 弱：版本号无严格锁定，但需与 Python 3.8 兼容 |
| **建议** | 使用 `sudo apt install python3-opencv` 安装系统包，与 Ubuntu 20.04 兼容性最佳 |

---

## 🔗 5. 宇树 G1 强绑定/关键依赖

以下依赖的版本约束比通用工具**更严格**，需特别留意。

### 5.1 CycloneDDS

| 项目 | 说明 |
| :--- | :--- |
| **检查命令** | `dpkg -l \| grep cyclonedds` 或查看编译时使用的版本 |
| **宇树要求** | **CycloneDDS 0.10.x**（宇树 G1 通信协议层强绑定） |
| **绑定强度** | ⭐⭐⭐ **强**：版本不匹配可能导致 DDS 通信失败或行为异常 |
| **验证方式** | 确认 `cyclonedds_ws` 编译时拉取的是 `releases/0.10.x` 分支，且已成功 `source install/setup.bash` |

### 5.2 rmw_cyclonedds_cpp

| 项目 | 说明 |
| :--- | :--- |
| **检查命令** | `ros2 pkg list \| grep rmw_cyclonedds` |
| **要求** | 需与 CycloneDDS 0.10.x 配套的 Foxy 分支版本 |
| **绑定强度** | ⭐⭐⭐ **强**：与 CycloneDDS 版本协同，不可单独升级 |

### 5.3 unitree_sdk2py (Python SDK)

| 项目 | 说明 |
| :--- | :--- |
| **检查命令** | `pip3 show unitree_sdk2py` 或 `python3 -c "import unitree_sdk2py"` |
| **Python 要求** | **>= 3.8** |
| **绑定强度** | ⭐⭐ 中：对 Python 版本有下限要求，对 SDK 自身版本需参考宇树官方发布的配套版本 |
| **建议** | 使用宇树官方提供的 `unitree_sdk2py` 版本，不要自行升级到不兼容的版本 |

### 5.4 宇树消息包 (unitree_go / unitree_hg)

| 项目 | 说明 |
| :--- | :--- |
| **检查命令** | `ros2 interface list \| grep unitree` |
| **验证标准** | 应能看到 `unitree_go/msg/LowCmd`、`unitree_hg/msg/LowState` 等消息类型 |
| **绑定强度** | ⭐⭐⭐ **强**：消息定义与机器人固件版本强关联，须使用匹配的分支 |

---

## 📋 6. 一键校验脚本

将以下内容保存为 `check_env.sh`，在容器内执行 `bash check_env.sh` 即可快速核对所有关键版本：

```bash
#!/bin/bash
echo "===== 宇树 G1 开发环境版本校验 ====="
echo ""
echo "--- Python ---"
python3 --version
echo ""
echo "--- CMake ---"
cmake --version | head -1
echo ""
echo "--- GCC / G++ ---"
gcc --version | head -1
g++ --version | head -1
echo ""
echo "--- OpenCV (Python) ---"
python3 -c "import cv2; print('OpenCV:', cv2.__version__)" 2>/dev/null || echo "OpenCV (Python) 未安装"
echo ""
echo "--- CycloneDDS 相关包 ---"
dpkg -l 2>/dev/null | grep -i cyclonedds || echo "未通过 apt 安装 CycloneDDS"
echo ""
echo "--- rmw_cyclonedds_cpp ---"
ros2 pkg list 2>/dev/null | grep rmw_cyclonedds || echo "ROS 2 环境未 source 或未安装"
echo ""
echo "--- 宇树消息接口 ---"
ros2 interface list 2>/dev/null | grep unitree || echo "宇树消息包未编译或未 source"
echo ""
echo "--- unitree_sdk2py ---"
python3 -c "import unitree_sdk2py; print('unitree_sdk2py 已安装')" 2>/dev/null || echo "unitree_sdk2py 未安装"
```

---

## 💎 总结

| 工具/依赖 | 检查命令 | 绑定强度 | 关键动作 |
| :--- | :--- | :--- | :--- |
| Python | `python3 --version` | ⭐⭐ | 保持 3.8，`pip` 优先用 `venv` |
| CMake | `cmake --version` | ⭐ | **不要升级** |
| GCC/G++ | `gcc --version` | ⭐⭐ | **坚决不要升级** |
| OpenCV | `python3 -c "import cv2"` | ⭐ | 用 `apt` 安装 |
| CycloneDDS | `dpkg -l \| grep cyclonedds` | ⭐⭐⭐ | 确认 0.10.x 分支 |
| rmw_cyclonedds_cpp | `ros2 pkg list \| grep rmw` | ⭐⭐⭐ | 与 CycloneDDS 配套 |
| unitree_sdk2py | `pip3 show unitree_sdk2py` | ⭐⭐ | 用宇树官方版本 |
| 宇树消息包 | `ros2 interface list \| grep unitree` | ⭐⭐⭐ | 编译后需 `source` |
