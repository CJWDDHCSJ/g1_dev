# G1-EDU 语音交互节点备份

本备份保存 2026-10-27 验收通过的 G1 语音交互相关源码与配置。

## 1. 包与节点

- g1_asr_bridge / asr_json_bridge
  - 订阅 /audio_msg
  - 从宇树 ASR JSON 中提取 text
  - 发布纯文本到 /g1/asr_text

- g1_voice_router / voice_router
  - 订阅 /g1/asr_text
  - 根据关键词选择预设回复
  - 发布回复到 /g1/tts
  - 当前版本不做重复消息过滤

- unitree_ros2_example / g1_tts_node
  - 订阅 /g1/tts
  - 按标点切分句子
  - 调用宇树 AudioClient::TtsMaker() 播报
  - 使用后台工作线程串行处理播报队列

## 2. 数据流

G1 内置 ASR
  -> /audio_msg
  -> g1_asr_bridge
  -> /g1/asr_text
  -> g1_voice_router
  -> /g1/tts
  -> g1_tts_node
  -> Unitree TTS API

## 3. TTS 配置

- config/tts_config.json：原始主持词配置，保留原件。
- config/tts_test_config.json：text 为空，适合测试实时语音交互而不自动播放主持词。
- 可使用 G1_TTS_CONFIG 环境变量指定配置文件。
- 当前代码读取 volume 字段，但未通过 SetVolume() 实际设置设备音量。

## 4. 构建参考

在原 G1 工作区中执行：

source /opt/ros/humble/setup.bash
source ~/unitree_ros2/cyclonedds_ws/install/setup.bash

cd ~/unitree_ros2/example
colcon build --packages-select \
  g1_asr_bridge g1_voice_router unitree_ros2_example \
  --symlink-install

本备份是相关源码与配置快照，不是完整的 ROS 2 工作区。
恢复编译仍需要原工作区、ROS 2 Humble 和宇树相关依赖。

## 5. 真机验收

2026-10-27：用户确认 ASR、文本路由、/g1/tts 消息流和 TTS 播报链路通过。
