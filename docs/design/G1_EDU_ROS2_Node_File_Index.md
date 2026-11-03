# Unitree G1-EDU ROS 2 节点与源码文件索引

> **整理日期：** 2026-10-31  
> **用途：** 记录 G1-EDU 上已经编写、编译或实际跑通过的 ROS 2 节点，明确每个节点对应的 ROS 包、可执行程序、源码文件、订阅/发布话题、用途和启动命令。后续开发优先更新本文，不再依赖聊天记录翻找路径。
>
> **信息可信度说明：** 本文根据此前保存的项目文档、终端输出和已完成测试整理，并非本次直接登录机器人逐个读取文件所得。明确标注“已验证”的内容有历史运行结果支撑；标注“待核实”的内容不要当作当前文件系统的最终事实。`~` 默认指 G1 上的用户目录 `/home/unitree`。

---

## 1. 当前机器人环境与工作区

### 1.1 G1 端环境

| 项目 | 当前记录 |
|---|---|
| 机器人 | Unitree G1-EDU |
| 主控 | Jetson Orin NX，ARM64 / aarch64 |
| OS | Ubuntu 22.04.5 LTS (Jammy) |
| ROS 2 | Humble |
| JetPack | 6.2.3 |
| CUDA | 12.6 |
| TensorRT | 10.3 |
| DDS | CycloneDDS，`rmw_cyclonedds_cpp` |
| 机器人网口 | `eth0`，`192.168.123.164` |
| ROS Domain | `0` |

### 1.2 主要工作区

| 路径 | 用途 | 备注 |
|---|---|---|
| `~/unitree_ros2/` | Unitree ROS 2 主仓库 | 官方接口、例程及项目工作区所在目录 |
| `~/unitree_ros2/cyclonedds_ws/` | Unitree ROS 2 消息工作区 | `unitree_api`、`unitree_go`、`unitree_hg` 等消息包 |
| `~/unitree_ros2/example/` | Unitree 示例包及当前语音项目节点 | `unitree_ros2_example`、`g1_asr_bridge`、`g1_voice_router` |
| `~/lidar_ws/` | 自己创建的 ROS 2 工作区 | 当前包含 `g1_action_executor`，另有历史 `lidar_check` 工具包 |
| `~/unitree_sdk2_transfer/unitree_sdk2-main/` | 转移到 G1 的 Unitree SDK2 源码 | 官方 SDK 头文件、库和示例 |

### 1.3 每个新终端的环境初始化

G1 上新开一个 ROS 2 终端，先执行：

```bash
source ~/unitree_ros2/setup.sh
```

这个脚本是目前项目使用过的一键环境入口，会加载 ROS 2 Humble、Unitree 消息工作区以及 CycloneDDS 相关环境。运行自定义包之前，再加载对应工作区：

```bash
# 位于 ~/unitree_ros2/example 的包
source ~/unitree_ros2/example/install/setup.bash

# 位于 ~/lidar_ws 的包
source ~/lidar_ws/install/setup.bash
```

不要在同一个终端混用不同 ROS 发行版的 setup 文件。若 `ros2 run` 提示找不到包，先确认该终端已经 source 对应工作区，再检查包是否成功构建。

---

## 2. 当前主线：语音输入、问候判断、动作与语音播报

### 2.1 当前已经跑通的语音问候链路

```text
用户对 G1 说“你好”
       │
       ▼
G1 自带语音识别服务 / ASR
       │  Unitree API 语音请求与响应
       ▼
/api/voice/request  <---->  /api/voice/response
       │
       ▼
[g1_asr_bridge]
源码：g1_asr_bridge/src/asr_json_bridge.cpp
       │ 发布识别文本
       ▼
/g1/asr_text  (std_msgs/msg/String)
       │
       ├─────────────── 终端监听：ros2 topic echo /g1/asr_text
       │
       ▼
[g1_voice_router]
源码：g1_voice_router/src/voice_router.cpp
       │
       ├── 问候语精确匹配
       │      ├── 发布 "wave" 到 /g1/action_request
       │      └── 发布问候文本到 /g1/tts
       │
       └── 普通对话（非本次问候捷径）
              └── HTTP 请求 Windows 上的 Ollama
                     └── 模型回复再发布到 /g1/tts
       │
       ├─────────────────────────────┐
       ▼                             ▼
[g1_action_executor]             [g1_tts_node]
执行预设挥手动作                 文本转语音播报
       │                             │
       ▼                             ▼
G1 手臂挥手                       G1 扬声器
```

**当前“你好 → 挥手 + 播报”不依赖 Windows Ollama。** `voice_router.cpp` 对常见问候语有本地精确匹配分支，匹配成功后直接发动作请求和 TTS 文本；只有普通自由对话才需要访问 Windows 上的 Ollama 服务。

### 2.2 节点总览

| ROS 包 | 可执行程序 | 源码路径（G1） | 作用 | 当前状态 |
|---|---|---|---|---|
| `g1_asr_bridge` | `asr_json_bridge` | `~/unitree_ros2/example/g1_asr_bridge/src/asr_json_bridge.cpp` | 将 G1 语音识别服务返回的数据解析并转成 ROS 2 文本消息，发布 `/g1/asr_text` | **已验证**：2026-10-31 确认启动该节点后 ASR 文本链路恢复 |
| `g1_voice_router` | `voice_router` | `~/unitree_ros2/example/g1_voice_router/src/voice_router.cpp` | 订阅 ASR 文本，判断问候意图；问候时触发挥手和固定 TTS 回复；普通对话走 Ollama | **已编译；问候分支已集成**，整条链路由用户确认调通 |
| `unitree_ros2_example` | `g1_tts_node` | `~/unitree_ros2/example/src/src/g1/audio_client/g1_tts_node.cpp` | 订阅 `/g1/tts`，调用 Unitree `AudioClient::TtsMaker()` 进行语音合成和播报 | **已验证**：真机播报可用 |
| `g1_action_executor` | `g1_action_executor_node` | `~/lidar_ws/src/g1_action_executor/src/main.cpp`、`src/arm_action_executor.cpp`、`include/g1_action_executor/arm_action_executor.hpp` | 接收动作字符串命令，检查 FSM / 手臂状态 / 忙碌和冷却状态后调用 G1 手臂预设动作 | **已验证**：`wave` 对应 ID 26，真机挥手成功 |

---

## 3. 每个语音节点的详细说明

### 3.1 ASR 桥接节点：`g1_asr_bridge`

**包目录：**

```text
~/unitree_ros2/example/g1_asr_bridge/
├── CMakeLists.txt
├── package.xml
└── src/
    └── asr_json_bridge.cpp
```

**启动命令：**

```bash
source ~/unitree_ros2/setup.sh
source ~/unitree_ros2/example/install/setup.bash
ros2 run g1_asr_bridge asr_json_bridge
```

**输入 / 输出：**

- 上游：G1 自带语音服务相关的 Unitree API 请求/响应通信。
- 相关 ROS 2 服务话题：`/api/voice/request`、`/api/voice/response`。
- 下游发布：`/g1/asr_text`，类型 `std_msgs/msg/String`。

**作用：**

机器人自带 ASR 服务负责语音识别；这个桥接节点把返回的识别结果变成普通 ROS 2 String 消息，供其他 ROS 2 节点消费。它不是语音识别模型本身，也不是 TTS 节点。

**调试命令：**

```bash
# 查包内真实的可执行程序
ros2 pkg executables g1_asr_bridge

# 查看桥接节点是否在发布
ros2 topic info /g1/asr_text --verbose

# 实时打印识别文本
ros2 topic echo /g1/asr_text
```

历史故障：曾出现 `/g1/asr_text` 的 `Publisher count: 0`，原因是 ASR Bridge 没有启动，而不是先验地证明机器人 ASR 服务坏了。启动 `asr_json_bridge` 后，用户确认恢复正常。以后遇到“没有识别文本”，先检查该节点是否存在、发布者是否为 1，再决定是否分析 JSON 解析代码。

> **注意：** 目前历史记录确认了它的源码和可执行程序名；其内部订阅回调、JSON 字段名等实现细节，需以当前 `asr_json_bridge.cpp` 内容为准，不在此猜测。

### 3.2 语音路由节点：`g1_voice_router`

**包目录：**

```text
~/unitree_ros2/example/g1_voice_router/
├── CMakeLists.txt
├── package.xml
└── src/
    └── voice_router.cpp
```

**启动命令：**

```bash
source ~/unitree_ros2/setup.sh
source ~/unitree_ros2/example/install/setup.bash
ros2 run g1_voice_router voice_router
```

**主要话题：**

| 方向 | 话题 | 类型 | 用途 |
|---|---|---|---|
| 订阅 | `/g1/asr_text` | `std_msgs/msg/String` | 接收 ASR 识别文本 |
| 发布 | `/g1/tts` | `std_msgs/msg/String` | 把应答文本交给 TTS 节点 |
| 发布 | `/g1/action_request` | `std_msgs/msg/String` | 向动作执行器发送动作命令，例如 `wave` |

**问候分支：**

目前在 `voice_router.cpp` 中增加了文本归一化和固定问候语匹配。历史代码记录的匹配例子包括：

| 识别文本 | TTS 回复 |
|---|---|
| `你好`、`您好`、`你好啊`、`嗨`、`嗨嗨`、`哈喽`、`hello`、`hi` | `你好！很高兴见到你。` |
| `早上好`、`早安`、`上午好` | `早上好！今天也一起加油。` |
| `下午好` | `下午好！很高兴见到你。` |
| `晚上好` | `晚上好！今天辛苦了。` |

问候命中后，路由节点向 `/g1/action_request` 发布 `wave`，同时向 `/g1/tts` 发布问候回复，不再调用 Ollama。识别文本如果不是支持的精确问候内容，则走普通对话分支。

**普通对话模型配置（仅普通对话需要）：**

- 模型服务：Windows 上的 Ollama。
- 历史 IP / URL：`http://192.168.123.99:11434/api/chat`。
- 历史模型名：`qwen3-vl:4b-instruct`。
- 如果 Windows 主机 IP、模型名或端口变化，应以当前 `voice_router.cpp` 中的参数为准。

> 用户当前在 Ubuntu 环境工作。不要为了测试固定问候分支，要求启动 Windows Ollama。Ollama 不可用时，普通对话可能失败，但本地问候分支仍应可工作。

### 3.3 TTS 播报节点：`g1_tts_node`

**包：** `unitree_ros2_example`  
**工程目录：** `~/unitree_ros2/example/src/`

**源码与相关头文件：**

```text
~/unitree_ros2/example/src/
├── include/
│   ├── g1/g1_audio_client.hpp
│   └── common/base_client.hpp
└── src/g1/audio_client/
    ├── g1_tts_node.cpp                 # 当前主源码
    ├── g1_tts_node.before_pause.cpp    # 历史版本/备份，不要当主节点启动
    ├── g1_tts_node.working.cpp        # 历史工作版本，不要与主节点重复启动
    ├── g1_audio_client_example.cpp    # SDK/音频客户端示例
    └── g1_tts_test.cpp                # 测试代码
```

**启动命令：**

```bash
source ~/unitree_ros2/setup.sh
source ~/unitree_ros2/example/install/setup.bash
ros2 run unitree_ros2_example g1_tts_node
```

**输入话题：** `/g1/tts`，类型 `std_msgs/msg/String`。  
**底层接口：** Unitree `AudioClient::TtsMaker(sentence, speaker_id)`。

**已实现的防截断处理：**

- 将长文本按中英文句末标点拆分。
- 分句调用 `TtsMaker()`。
- 根据 UTF-8 字符数量估算每句等待时长，历史测试使用约 250 ms/字符、最短 1000 ms。
- 句间停顿通过配置控制，历史配置测试使用 `pause_ms = 1500`。

**配置文件：**

```text
~/unitree_ros2/example/config/tts_config.json
~/unitree_ros2/example/config/tts_test_config.json
```

历史配置中使用过 `speaker_id: 0`、`volume: 80`、`pause_ms: 1500`。以当前 JSON 文件为准，不要把历史值当作当前一定未变的值。

查看配置：

```bash
cat ~/unitree_ros2/example/config/tts_config.json
cat ~/unitree_ros2/example/config/tts_test_config.json
```

**常见问题：** 如果 TTS 节点一启动就播报一段旧演示文本，检查 `tts_config.json` 的 `text` 字段和源码中的启动播报逻辑；这不一定是 `/g1/tts` 收到的新消息。不要同时启动主 TTS 节点和旧测试程序，否则可能有音频请求冲突。

### 3.4 手臂动作执行节点：`g1_action_executor`

**工作区：** `~/lidar_ws/`  
**包目录：** `~/lidar_ws/src/g1_action_executor/`

**目录结构：**

```text
~/lidar_ws/src/g1_action_executor/
├── CMakeLists.txt
├── package.xml
├── include/
│   └── g1_action_executor/
│       └── arm_action_executor.hpp
└── src/
    ├── arm_action_executor.cpp
    └── main.cpp
```

**启动命令：**

```bash
source ~/unitree_ros2/setup.sh
source ~/lidar_ws/install/setup.bash
ros2 run g1_action_executor g1_action_executor_node
```

**话题接口：**

| 方向 | 话题 | 类型 | 用途 |
|---|---|---|---|
| 订阅 | `/g1/action_request` | `std_msgs/msg/String` | 接收动作字符串，例如 `wave` |
| 发布 | `/g1/action_result` | `std_msgs/msg/String` | 返回 accepted / success / failure 状态 |
| 订阅 | `/arm/action/state` | `std_msgs/msg/String` | 读取手臂当前动作、ID 和 holding 状态 |
| 订阅 | `/sportmodestate` | `unitree_hg/msg/SportModeState` | 获取 FSM 状态，判断动作是否允许执行 |

**目前明确支持的命令：**

| 命令 | Unitree 预设动作 ID | 说明 |
|---|---:|---|
| `wave` | 26 | `wave_above_head`，头顶上方挥手，已在真机验证 |
| `wave_low` | 25 | `wave_under_head`，较低位置挥手，已加入白名单；具体使用前仍应确认当前动作状态 |

**节点内已经记录的安全检查：**

- FSM 只允许已知支持状态：500、501、801；FSM 801 时还要求 `fsm_mode` 为 0 或 3。
- 检查是否收到手臂状态。
- `holding` 为 true 时不接受新的动作请求。
- 动作执行中不接受新的动作任务。
- 设有约 3 秒动作冷却时间。
- 在工作线程中调用同步动作请求，避免阻塞 ROS 2 回调线程处理响应。

**成功测试记录：**

向话题发送：

```bash
ros2 topic pub --once /g1/action_request std_msgs/msg/String "{data: 'wave'}"
```

动作执行器曾返回：

```text
accepted:wave:26
success:wave:26
```

并在 `/arm/action/state` 中观察到动作 ID 26，之后进入 ID 99 `release_arm`，再回到 ID 0 空闲状态。该节点已在真机完成挥手测试。

---

## 4. 官方接口与我们自己写的节点：不要混为一谈

### 4.1 官方 ROS 2 / Unitree 节点

历史节点列表中出现过：

```text
/audio_client
/ros_bridge
```

这两个节点属于机器人现有音频/ROS 桥接通信栈中的节点，不应当作本项目自己创建的 `g1_*` 节点。它们的具体启动管理方式和源码映射尚未在本索引中确认。当前语音节点会借助它们提供的机器人侧能力或底层接口工作。

### 4.2 手臂动作官方客户端封装

**官方 SDK2 头文件：**

```text
~/unitree_sdk2_transfer/unitree_sdk2-main/include/unitree/robot/g1/arm/g1_arm_action_client.hpp
```

**官方 ROS 2 示例头文件：**

```text
~/unitree_ros2/example/src/include/g1/g1_arm_action_client.hpp
```

**被本项目引用/修改过的 ROS 2 基础请求响应封装：**

```text
~/unitree_ros2/example/src/include/common/base_client.hpp
```

历史排错时，`BaseClient` 曾出现 5 秒等待不足、JSON 空响应解析报错的问题。之后已调整等待时间到约 20 秒，并增加空 JSON 响应保护及请求/响应诊断日志。相关备份文件曾有：

```text
base_client.hpp.before_debug
base_client.hpp.before_empty_json_fix
```

不要直接用备份覆盖当前 `base_client.hpp`。如果后续动作 RPC 又出现超时或 JSON 解析错误，先检查当前文件和它的实际构建来源。

### 4.3 官方 SDK2 独立动作示例

历史编译产物：

```text
~/unitree_sdk2_transfer/unitree_sdk2-main/build/bin/g1_arm_action_example
```

曾用以下参数查询和调用：

```bash
~/unitree_sdk2_transfer/unitree_sdk2-main/build/bin/g1_arm_action_example --network eth0 --list
~/unitree_sdk2_transfer/unitree_sdk2-main/build/bin/g1_arm_action_example --network eth0 --id 26
```

注意：独立 SDK2 示例曾发生 RPC 返回 3104 超时，但当时真机动作实际已经触发。当前语音项目应优先使用已经验证的 ROS 2 `g1_action_executor`，不要因为看见这个旧示例就同时启动两套动作客户端。

---

## 5. 当前问候功能：推荐 5 个终端布局

**每个终端都在 G1 的 Ubuntu shell 中执行。** 一个节点对应一个长期运行的终端，不要重复启动同一节点。

### 终端 1：TTS 播报

```bash
source ~/unitree_ros2/setup.sh
source ~/unitree_ros2/example/install/setup.bash
ros2 run unitree_ros2_example g1_tts_node
```

### 终端 2：手臂动作执行器

```bash
source ~/unitree_ros2/setup.sh
source ~/lidar_ws/install/setup.bash
ros2 run g1_action_executor g1_action_executor_node
```

### 终端 3：语音路由

```bash
source ~/unitree_ros2/setup.sh
source ~/unitree_ros2/example/install/setup.bash
ros2 run g1_voice_router voice_router
```

### 终端 4：ASR 桥接

```bash
source ~/unitree_ros2/setup.sh
source ~/unitree_ros2/example/install/setup.bash
ros2 run g1_asr_bridge asr_json_bridge
```

### 终端 5：实时查看 ASR 识别文本

```bash
source ~/unitree_ros2/setup.sh
ros2 topic echo /g1/asr_text
```

对机器人说“你好”时，终端 5 应显示类似：

```yaml
data: 你好
---
```

随后终端 3 的路由逻辑应触发动作请求和 TTS 回复，终端 2 应显示动作执行结果，G1 完成挥手和播报。

### 建议启动顺序

1. TTS 节点。
2. 动作执行器。
3. 语音路由。
4. ASR Bridge。
5. 最后打开文本监听终端并开始测试。

先启动下游，再启动上游，避免识别文本到来时接收节点尚未就绪。

---

## 6. 常用话题与排错命令速查

### 6.1 关键话题

| 话题 | 消息类型 | 生产者 / 消费者 | 用途 |
|---|---|---|---|
| `/api/voice/request` | `unitree_api/msg/Request` | G1 语音服务相关 DDS 端点 | ASR/语音服务请求侧通信 |
| `/api/voice/response` | `unitree_api/msg/Response` | G1 语音服务相关 DDS 端点 | ASR/语音服务响应侧通信 |
| `/g1/asr_text` | `std_msgs/msg/String` | `g1_asr_bridge` → `g1_voice_router` | ASR 识别文本 |
| `/g1/action_request` | `std_msgs/msg/String` | `g1_voice_router` → `g1_action_executor` | 动作指令 |
| `/g1/action_result` | `std_msgs/msg/String` | `g1_action_executor` | 动作执行结果 |
| `/arm/action/state` | `std_msgs/msg/String` | G1 机器人侧 | 手臂动作状态 |
| `/sportmodestate` | `unitree_hg/msg/SportModeState` | G1 机器人侧 | FSM / 运动模式状态 |
| `/g1/tts` | `std_msgs/msg/String` | `g1_voice_router` → `g1_tts_node` | 待播报文本 |

### 6.2 一次性检查节点和话题

```bash
source ~/unitree_ros2/setup.sh
source ~/unitree_ros2/example/install/setup.bash
source ~/lidar_ws/install/setup.bash

ros2 node list
ros2 topic list | grep -E '(^/g1/|/api/voice|/arm/action/state|sportmodestate)'

ros2 pkg executables g1_asr_bridge
ros2 pkg executables g1_voice_router
ros2 pkg executables unitree_ros2_example
ros2 pkg executables g1_action_executor
```

### 6.3 分段监听数据

```bash
# 第一段：ASR 有没有产出文本
ros2 topic echo /g1/asr_text

# 第二段：路由有没有发出挥手指令
ros2 topic echo /g1/action_request

# 第三段：动作执行器是否接受并完成动作
ros2 topic echo /g1/action_result

# 第四段：路由有没有把回复发给 TTS
ros2 topic echo /g1/tts

# 观察真实手臂动作状态
ros2 topic echo /arm/action/state
```

每条 `echo` 命令都在独立终端执行，或者每次检查一条。不要在同一终端同时运行多个长期阻塞的 `echo`。

### 6.4 故障定位顺序

| 现象 | 第一检查项 | 后续方向 |
|---|---|---|
| `/g1/asr_text` 无文本，Publisher count 为 0 | `ros2 node list` 是否有 ASR Bridge；`ros2 pkg executables g1_asr_bridge` | 启动 `asr_json_bridge`；再检查 `/api/voice/request`、`/api/voice/response` 和源码解析逻辑 |
| ASR Bridge 在运行，但 `/g1/asr_text` 仍无输出 | `ros2 topic info /g1/asr_text --verbose` | 检查上游服务是否产生响应、Bridge 日志、DDS 和 QoS |
| `/g1/asr_text` 有字，但没有挥手请求 | `voice_router` 日志和 `/g1/action_request` | 检查识别文本是否符合精确问候词规则，以及路由节点是否为重新编译后的版本 |
| `/g1/action_request` 有 `wave`，但手臂不动 | `g1_action_executor` 日志、`/g1/action_result`、`/arm/action/state`、`/sportmodestate` | 检查 FSM、holding、动作执行超时及是否有其他程序占用手臂 |
| 有 `success:wave:26`，但动作没表现 | 对比 `/arm/action/state` | 核实动作状态和机械执行情况，不要连续重复发动作命令 |
| `/g1/tts` 有文本但没有声音 | `g1_tts_node` 是否运行、日志、音频服务状态 | 检查 TTS 配置、AudioClient 响应和扬声器状态 |
| 普通聊天失败但“你好”仍可挥手 | Windows Ollama 服务和网络 | 固定问候分支本地运行；普通对话依赖 Windows Ollama |
| `ros2 run` 找不到包 | 是否 source 对应 install/setup.bash | 用 `ros2 pkg executables <包名>` 查询实际可执行程序并按需重新构建 |

---

## 7. 构建与修改代码时应该去哪儿

### 7.1 构建语音相关包

```bash
cd ~/unitree_ros2/example
source ~/unitree_ros2/setup.sh

colcon build \
  --packages-select g1_asr_bridge g1_voice_router unitree_ros2_example \
  --symlink-install \
  --event-handlers console_direct+
```

构建后运行新终端时：

```bash
source ~/unitree_ros2/setup.sh
source ~/unitree_ros2/example/install/setup.bash
```

### 7.2 构建动作执行器

```bash
cd ~/lidar_ws
source ~/unitree_ros2/setup.sh

colcon build \
  --packages-select g1_action_executor \
  --symlink-install \
  --event-handlers console_direct+
```

构建后运行新终端时：

```bash
source ~/unitree_ros2/setup.sh
source ~/lidar_ws/install/setup.bash
```

### 7.3 修改代码前的备份习惯

修改某个 C++ 文件前先备份，例如：

```bash
cp -a path/to/file.cpp path/to/file.cpp.backup_$(date +%Y%m%d_%H%M%S)
```

创建/修改源码优先使用 `cat <<'EOF'`，并且只重编译发生变化的包。不要为了修改 `g1_voice_router` 而重编译整个 ROS 2 工作区。

---

## 8. 历史工具包：`lidar_check`（与本次语音问候无关）

工作区：`~/lidar_ws/`。此前的可执行文件清单显示有以下程序：

| ROS 包 | 可执行程序 | 从名称可判断的用途 | 当前项目是否需要 |
|---|---|---|---|
| `lidar_check` | `lidar_check` | LiDAR 相关检查/验证程序 | 否，语音问候不需要 |
| `lidar_check` | `scan_publisher` | Scan 数据发布辅助程序 | 否 |
| `lidar_check` | `avoidance` | 避障实验节点 | 否；可能涉及运动逻辑，不要随意启动 |
| `lidar_check` | `cmd_vel_bridge` | `cmd_vel` 桥接程序 | 否；可能涉及运动控制，不要随意启动 |
| `lidar_check` | `g1_ai_sport_checker` | G1 AI Sport 状态/接口检查程序 | 否 |
| `lidar_check` | `g1_fsm_checker` | FSM 状态检查程序 | 否，动作执行器本身会订阅运动状态 |
| `lidar_check` | `g1_fsm_switcher` | FSM 切换实验程序 | 否；会改变机器人 FSM，不要为语音测试启动 |

已确认过的构建产物路径形式：

```text
~/lidar_ws/install/lidar_check/lib/lidar_check/<executable>
~/lidar_ws/build/lidar_check/<executable>
```

**重要：** 目前保存的记录有包名与可执行程序名，但没有完整保存 `lidar_check` 每个可执行程序对应的 `.cpp` 文件名。因此这里不猜测源码路径。可在 G1 上执行以下只读命令获取当前真实映射：

```bash
cd ~/lidar_ws

colcon list
find src/lidar_check -type f \
  \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' -o -name 'CMakeLists.txt' \) \
  -print | sort

grep -RInE 'add_executable|rclcpp::Node|create_(publisher|subscription)' \
  src/lidar_check
```

这套旧工具不属于当前“说你好 → 挥手 → 播报”的最小链路。当前测试不要启动 `avoidance`、`cmd_vel_bridge` 或 `g1_fsm_switcher`。

---

## 9. 备份时需要保存哪些源码

如果要备份当前语音问候 MVP，优先保存：

```text
~/unitree_ros2/example/g1_asr_bridge/
~/unitree_ros2/example/g1_voice_router/
~/unitree_ros2/example/src/src/g1/audio_client/g1_tts_node.cpp
~/unitree_ros2/example/src/include/g1/g1_audio_client.hpp
~/unitree_ros2/example/src/include/common/base_client.hpp
~/unitree_ros2/example/src/CMakeLists.txt
~/unitree_ros2/example/src/package.xml
~/unitree_ros2/example/config/tts_config.json
~/unitree_ros2/example/config/tts_test_config.json
~/lidar_ws/src/g1_action_executor/
```

还应保留工作区构建配置和环境信息，例如 `colcon list` 输出、ROS 发行版、DDS 配置、构建日志。一般不把 `build/`、`install/`、`log/` 当作源码备份的核心内容；它们可重新生成，但安装依赖和运行环境必须记录清楚。

分享压缩包前先检查配置中的 IP、设备信息等本地参数。

---

## 10. 在 G1 上刷新这份索引的快速盘点命令

以后新增或改名节点时，在 G1 上运行下面这组命令，把输出补到本文对应章节：

```bash
source ~/unitree_ros2/setup.sh
source ~/unitree_ros2/example/install/setup.bash
source ~/lidar_ws/install/setup.bash

echo '========== ROS 2 packages =========='
colcon list --base-paths ~/unitree_ros2/example ~/lidar_ws/src 2>/dev/null

echo
echo '========== executables =========='
ros2 pkg executables g1_asr_bridge
ros2 pkg executables g1_voice_router
ros2 pkg executables unitree_ros2_example
ros2 pkg executables g1_action_executor
ros2 pkg executables lidar_check

echo
echo '========== voice source files =========='
find ~/unitree_ros2/example/g1_asr_bridge \
     ~/unitree_ros2/example/g1_voice_router \
     ~/unitree_ros2/example/src/src/g1/audio_client \
     ~/unitree_ros2/example/src/include/g1 \
     ~/unitree_ros2/example/src/include/common \
     ~/lidar_ws/src/g1_action_executor \
     -type f 2>/dev/null | sort

echo
echo '========== running nodes =========='
ros2 node list

echo
echo '========== relevant topics =========='
ros2 topic list | grep -E '(^/g1/|/api/voice|/arm/action/state|sportmodestate)'
```

如果包结构变化，先以这组命令的实时输出修订索引，不要沿用过时路径。

---

## 11. 当前状态摘要

截至 2026-10-31，已完成的真机里程碑：

- G1 ASR 识别结果能够通过 `g1_asr_bridge` 输出到 `/g1/asr_text`；本次排错确认只因 Bridge 未启动而没有 Publisher，启动 `asr_json_bridge` 后用户确认调通。
- `g1_voice_router` 已集成问候语识别分支：匹配问候语时不访问 Ollama，发送 `wave` 动作请求并发布固定问候 TTS 文本。
- `g1_tts_node` 负责从 `/g1/tts` 接收文本并调用 Unitree AudioClient 播报。
- `g1_action_executor_node` 已通过真机测试，`wave` 对应 ID 26，能够完成挥手。
- 当前最小演示目标已经实现：**对 G1 说“你好”，ASR 输出文本，路由触发挥手并让机器人播报问候语。**

下一阶段再逐步扩展其他意图和机器人状态查询。不要为了扩展功能破坏已经跑通的 MVP 主链路。
