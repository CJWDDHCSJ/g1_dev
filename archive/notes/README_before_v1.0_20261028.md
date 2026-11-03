# Unitree G1-EDU 语音交互项目说明

> 本文依据当前对话中确认的工程目录、源码文件名和已经跑通的数据链路整理。涉及实际参数时，以
> G1 上正在使用的源码和配置为准；不要把示例
> IP、可执行程序名或配置项当成未核实的事实。

## 1. 项目目标与总体数据流

当前语音交互链路：

``` text
G1 语音识别
    │
    ▼
g1_asr_bridge
    │ 发布识别文本
    ▼
ROS 2 话题：/g1/asr_text
    │
    ▼
g1_voice_router
    │ HTTP 请求
    ▼
Windows 主机上的 Ollama 模型服务
    │ 模型回复
    ▼
ROS 2 话题：/g1/tts
    │
    ▼
g1_tts_node
    │ Unitree AudioClient::TtsMaker(...)
    ▼
G1 扬声器播放
```

这个链路把语音识别、对话推理和语音播报分成独立节点。这样每一段都能单独检查，不需要把所有逻辑塞进一个程序里。

## 2. 工程目录与关键文件

G1 工作区的项目根目录：

``` text
~/unitree_ros2/example/
├── config/
│   ├── tts_config.json
│   ├── tts_test_config.json
│   └── 其他 TTS 配置备份/测试版本
├── g1_asr_bridge/
│   ├── CMakeLists.txt
│   ├── package.xml
│   └── src/asr_json_bridge.cpp
├── g1_voice_router/
│   ├── CMakeLists.txt
│   ├── package.xml
│   └── src/voice_router.cpp
└── src/
    ├── CMakeLists.txt
    ├── package.xml
    ├── include/g1/g1_audio_client.hpp
    ├── include/common/base_client.hpp
    └── src/g1/audio_client/
        ├── g1_tts_node.cpp
        ├── g1_tts_node.before_pause.cpp
        ├── g1_tts_node.working.cpp
        ├── g1_audio_client_example.cpp
        └── g1_tts_test.cpp
```

`build/`、`install/` 和 `log/`
是构建过程相关目录，源码备份通常不需要包含它们。

### 2.1 ASR 桥接节点

-   ROS 2 包目录：`g1_asr_bridge/`
-   源码：`g1_asr_bridge/src/asr_json_bridge.cpp`
-   构建配置：`g1_asr_bridge/CMakeLists.txt`
-   包依赖声明：`g1_asr_bridge/package.xml`
-   作用：将机器人侧语音识别结果桥接为 ROS 2 消息。当前链路中，下游使用
    `/g1/asr_text`。
-   实际订阅的数据源、消息类型、可执行程序名称应以源码和
    `ros2 pkg executables g1_asr_bridge` 的输出为准。

### 2.2 大模型语音路由节点

-   ROS 2 包目录：`g1_voice_router/`
-   源码：`g1_voice_router/src/voice_router.cpp`
-   历史备份：`g1_voice_router/src/voice_router.cpp.bak`
-   构建配置：`g1_voice_router/CMakeLists.txt`
-   包依赖声明：`g1_voice_router/package.xml`
-   作用：订阅 `/g1/asr_text`，把识别文本发送给
    Ollama，再把模型回复发布到 `/g1/tts`。
-   当前对话记录中使用过的 Ollama 地址为
    `http://192.168.123.99:11434/api/chat`，模型名为
    `qwen3-vl:4b-instruct`。如果源代码已更改，应以 `voice_router.cpp`
    中实际值为准。
-   Windows 主机必须运行 Ollama，并允许 G1
    访问对应服务端口；网络可达不等于模型请求一定成功。

### 2.3 TTS 节点

-   ROS 2 包：`unitree_ros2_example`，其工程目录为 `src/`
-   主源码：`src/src/g1/audio_client/g1_tts_node.cpp`
-   相关接口头文件：`src/include/g1/g1_audio_client.hpp`
-   基础客户端头文件：`src/include/common/base_client.hpp`
-   构建配置：`src/CMakeLists.txt`
-   包依赖声明：`src/package.xml`
-   作用：订阅 `/g1/tts`，将文本分句并调用 Unitree `AudioClient` 的
    `TtsMaker()` 接口进行语音合成与播报。
-   `g1_tts_node.before_pause.cpp` 和 `g1_tts_node.working.cpp`
    是保留的历史版本/工作版本，通常不要把它们当作独立节点同时启动。
-   TTS
    请求返回成功不一定代表扬声器已经播完整段音频；接口响应与实际播放完成是不同的状态。

## 3. 配置文件说明

### 3.1 `config/tts_config.json`

默认配置文件路径：

``` text
/home/unitree/unitree_ros2/example/config/tts_config.json
```

TTS 节点默认读取这个文件，除非启动环境通过 `G1_TTS_CONFIG`
指定其他路径。

注意事项：

1.  先阅读当前 JSON 内容，确认 `text` 等字段的实际含义。

2.  如果启动时播报固定的旧演示文本，检查该文件中的 `text`
    字段；它可能是启动/测试文本，不要误认为一定来自 `/g1/tts`。

3.  修改前先备份：

    ``` bash
    cp -a ~/unitree_ros2/example/config/tts_config.json \
      ~/unitree_ros2/example/config/tts_config.json.backup_$(date +%Y%m%d_%H%M%S)
    ```

4.  修改 JSON 后应确认语法正确。不要直接复制其他机器的配置覆盖当前文件。

### 3.2 `config/tts_test_config.json`

测试用途的 TTS
配置文件。是否会被使用取决于启动参数、环境变量或源码；文件存在并不意味着节点自动读取它。

### 3.3 `g1_voice_router` 的 Ollama 配置

在 `g1_voice_router/src/voice_router.cpp` 中检查： - Ollama HTTP 地址 -
模型名称 - 超时设置 - 请求 JSON 构造 - 错误处理和失败时的回复 -
发布话题名称

当前已知配置记录： - URL：`http://192.168.123.99:11434/api/chat` -
模型：`qwen3-vl:4b-instruct`

这两个值可能会随网络环境或模型调整变化，启动前以源文件为准。

### 3.4 ROS 2 与 CycloneDDS 环境

当前项目使用的环境设置如下。每个新终端运行 ROS 2
节点前，都要加载对应环境：

``` bash
cd ~/unitree_ros2/example

source /opt/ros/humble/setup.bash
source ~/unitree_ros2/cyclonedds_ws/install/setup.bash
source ~/unitree_ros2/example/install/setup.bash

export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
export ROS_DOMAIN_ID=0
export ROS_LOCALHOST_ONLY=0
export CYCLONEDDS_URI='<CycloneDDS><Domain Id="any"><General><Interfaces><NetworkInterface name="eth0" priority="default" multicast="default" /></Interfaces></General></Domain></CycloneDDS>'
```

注意： - 以上命令基于当前项目记录的 ROS 2 Humble 环境。 - `eth0`
必须是实际连接机器人网络的接口。如果网络接口名称不同，应先用
`ip -br addr` 核实，再调整配置。 - `ROS_DOMAIN_ID`、RMW 实现和 DDS
网卡配置必须与需要互通的节点相匹配。 - 不要在同一终端重复 source 不同
ROS 发行版的环境。

## 4. 构建命令

在 G1 的 Ubuntu 终端执行：

``` bash
cd ~/unitree_ros2/example

source /opt/ros/humble/setup.bash
source ~/unitree_ros2/cyclonedds_ws/install/setup.bash

colcon build \
  --packages-select g1_asr_bridge g1_voice_router unitree_ros2_example \
  --symlink-install
```

说明： - `--packages-select` 只选择列出的包。 - `--symlink-install`
便于开发时让安装空间引用工作区文件，具体行为取决于包的安装规则。 -
构建前需要确保这些包名真实存在且依赖已经安装。 -
如果其中一个包构建失败，先看报错包和首个编译错误，不要反复盲目重编译。

构建结束后，新开终端时加载工作区：

``` bash
source ~/unitree_ros2/example/install/setup.bash
```

## 5. 启动命令与建议顺序

每个启动终端都先执行第 3.4 节中的环境加载命令。建议先启动下游
TTS，再启动路由和 ASR，这样在识别消息到达时，下游通常已经就绪。

### 终端 A：TTS

``` bash
ros2 run unitree_ros2_example g1_tts_node
```

### 终端 B：大模型路由

``` bash
ros2 run g1_voice_router voice_router
```

### 终端 C：ASR 桥接

先查询实际可执行程序：

``` bash
ros2 pkg executables g1_asr_bridge
```

然后将查询到的可执行程序名称用于：

``` bash
ros2 run g1_asr_bridge <实际可执行程序名称>
```

尖括号内容是占位说明，执行前必须替换。不要凭包名猜可执行程序名。

### 查看路由节点可执行程序

``` bash
ros2 pkg executables g1_voice_router
```

当前记录的启动命令是：

``` bash
ros2 run g1_voice_router voice_router
```

### 查看 TTS 所在包的可执行程序

``` bash
ros2 pkg executables unitree_ros2_example
```

当前记录的 TTS 启动命令是：

``` bash
ros2 run unitree_ros2_example g1_tts_node
```

## 6. 运行检查与故障定位

### 6.1 检查话题

``` bash
ros2 topic list | grep -E '/g1/asr_text|/g1/tts'
```

查看 ASR 识别结果：

``` bash
ros2 topic echo /g1/asr_text
```

查看路由发给 TTS 的文本：

``` bash
ros2 topic echo /g1/tts
```

查看话题发布/订阅信息：

``` bash
ros2 topic info /g1/asr_text
ros2 topic info /g1/tts
```

查看 ROS 节点：

``` bash
ros2 node list
```

### 6.2 常见现象与排查方向

  ----------------------------------------------------------------------------------------
  现象                                优先检查
  ----------------------------------- ----------------------------------------------------
  `/g1/asr_text` 没有消息             ASR
                                      桥接进程是否运行、输入识别源是否有数据、话题名称及
                                      DDS 网络

  有 ASR 文本，但 `/g1/tts` 没消息    路由节点日志、Ollama 地址/模型、Windows 防火墙、HTTP
                                      超时

  `/g1/tts` 有文本但没声音            TTS 节点是否运行、TTS 配置、Unitree AudioClient
                                      响应、机器人音频服务

  启动 TTS 后先播固定旧文本           检查 `tts_config.json` 的 `text`
                                      字段及源码中的启动播报逻辑

  ROS 2 看不到其他节点/话题           检查 ROS_DOMAIN_ID、RMW_IMPLEMENTATION、CycloneDDS
                                      网卡名和网络连接

  `ros2 run` 提示找不到包/可执行程序  重新 source 工作区、检查 `colcon build` 结果，并用
                                      `ros2 pkg executables <包名>` 查询真实名称
  ----------------------------------------------------------------------------------------

## 7. Windows Ollama 服务检查

当前记录的 Ollama 地址为：

``` text
http://192.168.123.99:11434/api/chat
```

在运行 Ollama 的 Windows 主机上确认服务已经启动，并确认 G1
可以访问该主机地址和端口。不要只因为本机 `127.0.0.1:11434`
能访问，就认为 G1 一定能访问。

如果模型或 IP 改动，请同步检查 `voice_router.cpp` 中的配置。

## 8. 备份范围与安全提醒

建议备份以下内容： - `g1_asr_bridge/` - `g1_voice_router/` -
`src/src/g1/audio_client/` 中与 TTS 相关的源码 -
`src/CMakeLists.txt`、`src/package.xml` -
`src/include/g1/g1_audio_client.hpp` -
`src/include/common/base_client.hpp` - `config/tts_config*` 与
`config/tts_test_config.json` -
`SYSTEM_INFO.txt`、`FILE_MANIFEST.txt`、`SHA256SUMS.txt`（若已生成）

一般不需要将 `build/`、`install/`、`log/`
放进源码备份。完整迁移到另一台机器时，仍可能需要 Unitree SDK
库、其余依赖、正确的 ROS 发行版和 DDS 网络配置。

配置文件可能包含
IP、设备信息或其他本地参数；向他人分享压缩包前应检查并遮蔽不宜公开的内容。
