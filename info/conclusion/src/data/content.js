export const site = {
  name: 'G1 // VOICE STACK',
  tagline: '端侧具身智能语音链路复盘',
  updated: '2026.10',
  status: 'CPU 基线封存 · GPU 编译中',
  source: '整理自 ChatGPT 分享对话「宇树部署30天冲刺 (11)」',
  sourceNote:
    '这场工程对话累计 333 条用户输入、783 条助手回复与 176 条工具消息，约 148 万字符。以下内容只保留技术主线与已验证结论。',
  robot: 'Unitree G1-EDU',
}

export const nav = [
  { to: '/', label: '概览', index: '01' },
  { to: '/architecture', label: '系统架构', index: '02' },
  { to: '/nodes', label: '节点详解', index: '03' },
  { to: '/retrospective', label: '实战复盘', index: '04' },
  { to: '/roadmap', label: '下一步', index: '05' },
]

export const metrics = [
  {
    label: 'ASR 解码 RTF',
    value: '0.51–0.60',
    note: '优化后 CPU 长片段，越小越快',
    tone: 'mint',
  },
  {
    label: 'UDP 稳定性',
    value: '390 / 0 / 0',
    note: '收包 / 非法包 / 队列溢出',
    tone: 'blue',
  },
  {
    label: '中文 TTS',
    value: '5 / 5',
    note: 'Kokoro C++ 推理 + 实体播放',
    tone: 'amber',
  },
  {
    label: 'GPU 推理',
    value: '待验证',
    note: 'Orin NX 运行库编译进行中',
    tone: 'violet',
  },
]

export const phases = [
  {
    id: 'vision',
    no: '01',
    title: '视觉部署基线',
    subtitle: 'YOLO · ONNX · TensorRT · G1 相机',
    status: '搁置',
    tone: 'paused',
    summary:
      '把自训练模型从 .pt 导出为 ONNX，再构建 TensorRT FP32 engine，在 G1 上串起相机取图、C++ 前处理、推理与后处理。',
    bullets: [
      'Python TensorRT 对同一张测试图检测成功，置信度约 0.978。',
      'C++ 实时链路可以推理，但最高类别分数只有约 0.0037，无法复现 Python 结果。',
      '逐项核对输入尺寸、BGR/RGB、归一化、输出形状 (1, 8, 8400) 与 NMS，仍怀疑训练数据与机器人相机存在域偏差。',
      '视觉模型被定位为端侧部署练习；主线先转向语音闭环，避免在未验证的流水线上继续盖楼。',
    ],
    result: '链路已串通，结果未验收。下一步需要机器人视角数据与更严格的同图对照。',
    tags: ['PyTorch', 'ONNX', 'TensorRT', 'OpenCV', 'C++'],
  },
  {
    id: 'tts',
    no: '02',
    title: '中文 TTS：让 G1 说话',
    subtitle: 'Kokoro-82M v1.1-zh · ONNX Runtime C++ · AudioClient',
    status: '已跑通',
    tone: 'done',
    summary:
      '在开发机导出中文 Kokoro ONNX（约 311 MB），把模型与音色数据传到 G1；用 ARM64 ONNX Runtime 1.22.0 C++ API 推理，再直接播放内存中的波形。',
    bullets: [
      '中文前端留在 Python：misaki 的 ZHG2P 负责文本 → 音素，C++ 读取 Token 后执行 ONNX 推理。',
      '波形完成 float32 → PCM16、24 kHz → 16 kHz 转换，再通过 Unitree AudioClient 分块发送。',
      '首次播放到一半被截断：4.8 秒音频，程序只额外等待 0.5 秒就调用 PlayStop；改为等待完整播放后恢复正常。',
      '把临时修复封装成可复用的 G1AudioPlayer；5 条纯中文语句全部通过，连续两次播放验证成功。',
    ],
    result: 'Kokoro C++ 推理 + G1 扬声器链路打通，支持动态中文输入。',
    tags: ['Kokoro', 'ONNX Runtime', 'ZHG2P', 'PCM16', 'Unitree SDK'],
  },
  {
    id: 'mic',
    no: '03',
    title: '麦克风采集：绕开 ALSA',
    subtitle: 'UDP 多播 · Linux Socket · PCM16 → WAV',
    status: '已跑通',
    tone: 'done',
    summary:
      'G1 内部音频服务通过 UDP 多播持续发送麦克风数据，程序不直接调用 ALSA 录音接口，而是直接接收原始 PCM。',
    bullets: [
      '组播地址 239.168.123.161:5555；每包 5120 字节，16 kHz / 单声道 / 16-bit，约 0.16 秒音频。',
      'C++ 按 socket → bind → 加入多播组 → recvfrom 的顺序接收，拼接 PCM 字节流并封装为 RIFF/WAVE。',
      '实测 5.12 秒 WAV 格式正确，回传后确认是自己的声音。',
      '这条链路后来成为第三方 ASR 的输入源，不再受机器人自带录音接口限制。',
    ],
    result: '已获得可持续接收、可保存、可交给 ASR 的 16k 单声道音频流。',
    tags: ['UDP Multicast', 'Socket', 'PCM16', 'WAV', 'C++'],
  },
  {
    id: 'asr',
    no: '04',
    title: 'Qwen3-ASR：在机器人本体识别',
    subtitle: 'Qwen3-ASR-0.6B INT8 · sherpa-onnx v1.13.8 · C++',
    status: 'CPU 已跑通',
    tone: 'done',
    summary:
      '下载预转换模型包（解压约 954 MB：conv_frontend 43 MB、encoder.int8 175 MB、decoder.int8 721 MB，加 tokenizer），传到 G1，用 sherpa-onnx C++ API 完成离线识别。',
    bullets: [
      'G1 是 Ubuntu 20.04 / Python 3.8 / GCC 9.4 / ARM64，贸然升级会破坏机器人原有软件栈，因此选择预编译 CPU 运行库。',
      '先用单段 WAV 验证模型与 C++ API，再接 UDP 收音；把模型问题和音频问题分开排查。',
      '初版每识别一句话就重新加载模型，0.64–0.80 秒音频的 RTF 达到 1.25–1.90，不能算实时。',
      '识别文本存在同音字与词语混淆；需要结合原始音频、VAD 切段和模型参数继续评估，不能把责任直接推给麦克风。',
    ],
    result: 'Qwen3-ASR 能在 G1 本体 CPU 上运行，为后续集成提供可比较的基线。',
    tags: ['Qwen3-ASR', 'INT8', 'sherpa-onnx', 'ARM64', 'C++'],
  },
  {
    id: 'integration',
    no: '05',
    title: '集成与优化：从能跑到可持续',
    subtitle: 'UDP + VAD + Qwen3 ASR · 常驻模型 · 线程与队列',
    status: 'CPU 基线',
    tone: 'active',
    summary:
      '把收音、VAD 切段和 Qwen3 ASR 集成到 udp_vad_qwen3.cpp：模型只加载一次，UDP 收包放在独立线程，主线程负责 VAD 与识别。',
    bullets: [
      'VAD 使用能量/静音计数分段，保留起始预录；长语音超过 10 秒自动切段，并保留 0.32 秒重叠。',
      '队列从 500 包（约 80 秒）降到 64 包（约 10 秒）；所有队列状态检查加锁，修复数据竞争。',
      '队列溢出时记录断点并重置当前 VAD 段，避免把丢包前后的音频拼成一句话。',
      '稳定性测试：390 个 UDP 包、0 个非法包、0 次溢出、结束时 0 个积压；6 段完整识别；decode RTF 约 0.51–0.60。',
    ],
    result: 'CPU 语音链路达到可用基线；识别准确率、端到端延迟与长时间稳定性仍需继续打磨。',
    tags: ['VAD', '多线程', '有界队列', '并发安全', 'RTF'],
  },
  {
    id: 'gpu',
    no: '06',
    title: 'GPU 加速：正在编译',
    subtitle: 'Jetson Orin NX · CUDA 11.4 · cuDNN 8.6 · TensorRT 8.5.2',
    status: '进行中',
    tone: 'active',
    summary:
      '现有 sherpa-onnx 是 ARM64 CPU 包，系统装有 CUDA/TensorRT 并不代表运行库能用 GPU。因此单独构建 GPU 版运行库，不覆盖已经稳定的 CPU 基线。',
    bullets: [
      '按官方 Orin NX + CUDA 11.4 组合选择 ONNX Runtime 1.16.0；sherpa-onnx 保持 v1.13.8，减少 API 版本风险。',
      '第一次 CMake 配置仍开启 TTS 与说话人分离，编译到约 42% 时停止；重新配置为 GPU=ON、TTS=OFF、SPEAKER_DIARIZATION=OFF。',
      '配置已识别并下载 CUDA / TensorRT Provider 动态库；使用 2 个并行任务编译，避免 15 GiB 内存被编译器吃光。',
      '即使 Provider 成功加载，INT8 算子也不保证全部在 GPU 上执行；必须用同一段音频实测，而不是看到 CUDA 文件名就提前庆祝。',
    ],
    result: '构建链路已通过配置关；编译、Provider 加载、算子分配与加速效果全部尚未验证。',
    tags: ['Jetson Orin NX', 'CUDA', 'TensorRT', 'ONNX Runtime', 'INT8'],
  },
]

export const layers = [
  {
    id: 'input',
    label: '语音输入',
    caption: 'Audio In',
    description: '从机器人内部 UDP 多播流拿到 PCM，再完成分段与识别。',
    nodes: [
      {
        id: 'udp',
        label: '麦克风 / UDP',
        status: '已跑通',
        tone: 'done',
        detail:
          '不调用 ALSA。直接加入 G1 内部音频多播组，接收 16 kHz、单声道、16-bit PCM。每包 5120 字节，约 0.16 秒。',
        meta: ['239.168.123.161:5555', '5120 B / 包', '≈ 0.16 s / 包'],
      },
      {
        id: 'vad',
        label: 'VAD 切段',
        status: '已跑通',
        tone: 'done',
        detail:
          '按能量与静音包数判断语音起止，保留起始预录；长语音超过 10 秒自动切段，带 0.32 秒重叠，避免切断词尾。',
        meta: ['阈值 + 静音计数', '10 s 上限', '0.32 s overlap'],
      },
      {
        id: 'asr',
        label: 'Qwen3-ASR',
        status: 'CPU 已跑通',
        tone: 'done',
        detail:
          'Qwen3-ASR-0.6B INT8 由 sherpa-onnx C++ API 加载；模型常驻内存，每段只创建新的 OfflineStream，避免重复加载数百 MB 权重。',
        meta: ['0.6B INT8', 'sherpa-onnx v1.13.8', 'RTF 0.51–0.60'],
      },
    ],
  },
  {
    id: 'brain',
    label: '对话大脑',
    caption: 'Cognition',
    description: '把识别结果变成意图、回答或机器人任务。当前是闭环里缺失的一环。',
    nodes: [
      {
        id: 'dialog',
        label: '对话 / 意图理解',
        status: '待接入',
        tone: 'pending',
        detail:
          '本轮工程没有接入 LLM 或对话管理。它可以先用规则/意图分类，也可以接端侧小模型或网络侧大模型；需要结合延迟、内存与隐私做取舍。',
        meta: ['LLM 未接入', '意图 / 任务规划', '待定义验收指标'],
      },
      {
        id: 'skills',
        label: '任务与技能编排',
        status: '未开始',
        tone: 'pending',
        detail:
          '未来把「看看桌上有什么」拆成观察、移动、回答等技能调用。必须经过安全层，不能让对话模型直接控制关节。',
        meta: ['技能层', '安全约束', 'ROS 2 集成'],
      },
    ],
  },
  {
    id: 'output',
    label: '语音输出',
    caption: 'Audio Out',
    description: '文本生成中文语音，并由 G1 实体扬声器播放。',
    nodes: [
      {
        id: 'tts',
        label: 'Kokoro TTS',
        status: '已跑通',
        tone: 'done',
        detail:
          'Kokoro-82M v1.1-zh 导出为约 311 MB ONNX。中文 G2P 留在 Python 侧生成音素/Token，C++ 用 ONNX Runtime 1.22.0 推理并输出 24 kHz 波形。',
        meta: ['311 MB ONNX', 'ZHG2P 前端', '5 / 5 中文测试'],
      },
      {
        id: 'speaker',
        label: 'G1 扬声器',
        status: '已跑通',
        tone: 'done',
        detail:
          '波形转为 PCM16 并重采样到 16 kHz，通过 Unitree AudioClient 分块 PlayStream。关键教训：发送完数据不等于播放完成，PlayStop 必须等缓冲区真正播放结束。',
        meta: ['AudioClient.PlayStream', '24k → 16k', '直接播放内存波形'],
      },
    ],
  },
  {
    id: 'perception',
    label: '视觉感知',
    caption: 'Perception',
    description: '相机输入与目标检测。链路已搭好，但结果受数据域影响，暂未验收。',
    nodes: [
      {
        id: 'vision',
        label: 'YOLO / TensorRT',
        status: '搁置',
        tone: 'paused',
        detail:
          'best.pt → best.onnx → TensorRT FP32 engine，G1 相机 JPEG → OpenCV 解码 → C++ 前处理 → 推理 → NMS。Python 置信度 0.978，C++ 仅 0.0037，怀疑训练数据（手机高分辨率图）与机器人相机存在域偏差。',
        meta: ['输出 (1, 8, 8400)', 'FP32 engine', '域偏差待解决'],
      },
    ],
  },
  {
    id: 'body',
    label: '运动与安全',
    caption: 'Embodiment',
    description: '让机器人在真实环境中稳定、安全地执行技能。这是最终目标，但还没到这一步。',
    nodes: [
      {
        id: 'motion',
        label: '行走 / 动作技能',
        status: '未开始',
        tone: 'pending',
        detail:
          '目标包括站立、平衡、起步、停止、转向与基础操作。需要独立的技能层、安全边界与真实环境测试，而不是让高层模型直接控制关节。',
        meta: ['G1 运动能力', '技能调用', '安全优先'],
      },
    ],
  },
  {
    id: 'infra',
    label: '机器人基础',
    caption: 'Infrastructure',
    description: '所有模型与程序最终都要服从这台机器人的硬件和系统约束。',
    nodes: [
      {
        id: 'compute',
        label: 'Jetson Orin NX',
        status: '环境已确认',
        tone: 'info',
        detail:
          'G1-EDU 机载计算平台：8 核 ARM64（最高约 1.98 GHz）、15 GiB 内存、约 1.8 TiB 可用磁盘。',
        meta: ['aarch64', '15 GiB RAM', 'Ubuntu 20.04'],
      },
      {
        id: 'cuda',
        label: 'CUDA / TensorRT',
        status: 'GPU 待验证',
        tone: 'active',
        detail:
          '系统已有 CUDA 11.4、cuDNN 8.6、TensorRT 8.5.2 与 NVIDIA 设备节点。但当前 sherpa-onnx 是 CPU 运行库，GPU 版正在用 ONNX Runtime 1.16.0 独立构建。',
        meta: ['CUDA 11.4', 'TensorRT 8.5.2', 'ORT 1.16.0 GPU build'],
      },
      {
        id: 'middleware',
        label: 'DDS / ROS 2',
        status: '部分使用',
        tone: 'info',
        detail:
          'Unitree SDK 依赖 DDS 通信。语音采集直接走 UDP 多播；后续要把语音节点接入 ROS 2，让对话、视觉与运动模块解耦。',
        meta: ['Unitree SDK2', 'DDS', 'ROS 2 集成待做'],
      },
    ],
  },
]

export const pipelineStages = [
  {
    id: 'mic',
    label: '麦克风 / UDP',
    detail: '239.168.123.161:5555 · 5120 B/包',
    status: '已实测',
    tone: 'done',
  },
  {
    id: 'vad',
    label: 'VAD 切段',
    detail: '静音检测 · 10s 上限 + 0.32s 重叠',
    status: '已实测',
    tone: 'done',
  },
  {
    id: 'asr',
    label: 'Qwen3-ASR',
    detail: '0.6B INT8 · CPU',
    status: '已实测',
    tone: 'done',
  },
  {
    id: 'brain',
    label: '对话脑',
    detail: '意图 / LLM / 任务规划',
    status: '未接入',
    tone: 'pending',
  },
  {
    id: 'tts',
    label: 'Kokoro TTS',
    detail: '311MB ONNX · 24k→16k',
    status: '独立链路已通',
    tone: 'soft',
  },
  {
    id: 'speaker',
    label: 'G1 扬声器',
    detail: 'AudioClient.PlayStream',
    status: '独立链路已通',
    tone: 'soft',
  },
]

export const stack = [
  { group: '机器人', items: ['Unitree G1-EDU', 'Jetson Orin NX', 'Unitree SDK2', 'AudioClient', 'DDS'] },
  { group: '语音输入', items: ['UDP Multicast', 'Linux Socket', 'PCM16', 'WAV', 'VAD'] },
  { group: '语音识别', items: ['Qwen3-ASR 0.6B', 'INT8', 'sherpa-onnx v1.13.8', 'ONNX Runtime'] },
  { group: '语音合成', items: ['Kokoro-82M v1.1-zh', 'ONNX Runtime 1.22.0', 'misaki ZHG2P', 'AudioClient'] },
  { group: '视觉', items: ['YOLO', 'ONNX', 'TensorRT', 'OpenCV', 'C++'] },
  { group: '工程', items: ['C++17', 'CMake', '多线程', '有界队列', 'RTF 度量'] },
]

export const runtimeFacts = [
  { label: '音频入口', value: 'UDP 239.168.123.161:5555' },
  { label: '音频格式', value: '16 kHz · 单声道 · PCM16' },
  { label: '单包时长', value: '5120 B ≈ 0.16 s' },
  { label: 'ASR 模型', value: 'Qwen3-ASR-0.6B INT8 · 954 MB' },
  { label: 'TTS 模型', value: 'Kokoro v1.1-zh ONNX · 311 MB' },
  { label: 'CPU 运行库', value: 'sherpa-onnx v1.13.8 shared-cpu' },
  { label: 'GPU 目标', value: 'sherpa-onnx GPU · ORT 1.16.0' },
  { label: '计算平台', value: 'Orin NX · 8 核 ARM64 · 15 GiB' },
]

export const benchmarks = [
  { label: '片段 A', audio: 8.16, decode: 4.534, rtf: 0.556 },
  { label: '片段 B', audio: 13.76, decode: 7.604, rtf: 0.553 },
  { label: '片段 C', audio: 10.24, decode: 5.184, rtf: 0.506 },
]

export const initialBenchmarks = [
  { label: '初版 1', audio: 0.8, decode: 0.997, rtf: 1.246 },
  { label: '初版 2', audio: 0.64, decode: 0.936, rtf: 1.462 },
  { label: '初版 3', audio: 0.48, decode: 0.911, rtf: 1.898 },
]

export const pitfalls = [
  {
    title: 'API 返回 0 ≠ 播放完成',
    symptom: '4.8 秒的语音只播放了一部分，程序却正常退出。',
    cause:
      '音频数据分块发送完毕后只额外等待 0.5 秒就调用 PlayStop；机器人播放缓冲区里还有未播放的数据。',
    fix:
      '按实际播放时长等待，验证完整播放后再停止；之后把播放逻辑封装成可复用类，而不是把临时 sleep 留在主流程里。',
    lesson: '发送成功、接口返回 0、扬声器响了一声，都不等于“已经完整播放”。',
    tone: 'amber',
  },
  {
    title: '文件存在 ≠ 文件有效',
    symptom: 'C++ 播放程序读不到声音，但路径和文件名看起来完全正常。',
    cause: '测试用的 WAV 在某个环节变成了 0 字节，模型推理成功不代表输出文件一定有效。',
    fix: '检查文件大小、WAV 头、采样率、时长与振幅；从本地正常文件重新 scp 到 G1。',
    lesson: '先验证数据，再怀疑模型；先确认文件，再调试播放器。',
    tone: 'blue',
  },
  {
    title: '装了 CUDA ≠ 运行库能用 GPU',
    symptom: 'G1 上有 CUDA、TensorRT 和设备节点，但 ASR 始终跑在 CPU。',
    cause:
      '当前安装的是 sherpa-onnx 的 shared-cpu 包，目录里没有 CUDA / TensorRT Provider 动态库；libonnxruntime.so 也只依赖基础库。',
    fix:
      '用 ldd、文件列表和运行日志确认实际运行库；单独构建 GPU 版，不覆盖 CPU 基线。',
    lesson: 'strings 里出现 CUDAExecutionProvider，不等于 Provider 已注册、可用、能执行。',
    tone: 'violet',
  },
  {
    title: 'INT8 模型 ≠ 全部算子都在 GPU',
    symptom: 'GPU 版运行库加载成功，就以为推理一定变快。',
    cause:
      'encoder.int8.onnx / decoder.int8.onnx 中的部分量化算子可能没有 CUDA 实现，会回退到 CPU；跨设备拷贝也会增加开销。',
    fix:
      '用同一段音频跑 CPU / GPU 对照，记录模型加载、decode、RTF、内存与识别文本；查看 Provider 与算子分配日志。',
    lesson: '只有实际推理数据能证明加速，文件名和 Provider 列表都不能。',
    tone: 'mint',
  },
  {
    title: '能跑 ≠ 实时',
    symptom: '短音频能识别，但每句话都要等约 1 秒，RTF 大于 1。',
    cause: '初版每段语音都重新加载模型，且收包与识别没有正确解耦。',
    fix:
      '模型常驻、每段只建新 stream；UDP 收包独立线程；减小队列；分离排队、预处理、decode 的耗时统计。',
    lesson: 'RTF、队列长度、丢包和端到端延迟，比“程序没报错”更能说明系统是否可用。',
    tone: 'blue',
  },
  {
    title: '训练数据域偏差',
    symptom: 'Python 对手机图检测置信度 0.978，C++ 在机器人相机图上只有 0.0037。',
    cause: '训练集主要是手机高分辨率图片，和 G1 相机的视角、光照、压缩与噪声分布不一致。',
    fix: '先做同图对照排除预处理问题；再用机器人真实画面采集数据、微调或重新训练。',
    lesson: '模型部署失败不一定是推理代码错，也可能是模型没见过机器人看到的世界。',
    tone: 'amber',
  },
]

export const optimizations = [
  { item: '模型生命周期', before: '每段语音重新加载模型', after: '单进程常驻，每段只创建 OfflineStream', why: '减少数百 MB 权重重复加载' },
  { item: '收包与推理', before: '收包和识别互相阻塞', after: 'UDP 独立线程 + 有界队列', why: '每包仅 0.16 s，推理时不能漏包' },
  { item: '队列容量', before: '500 包 ≈ 80 s 音频', after: '64 包 ≈ 10 s 音频', why: '给实时交互设置延迟预算' },
  { item: '并发安全', before: '无锁读写队列状态', after: 'mutex 保护所有检查与统计', why: '消除数据竞争' },
  { item: '丢包处理', before: '直接丢包后继续拼接', after: '记录断点并重置 VAD 段', why: '避免把不连续音频拼成一句话' },
  { item: '长语音', before: '单段可能无限增长', after: '10 s 自动切段 + 0.32 s 重叠', why: '控制单次推理时长与切词风险' },
  { item: '性能度量', before: '只记录总耗时', after: '分离加载 / 排队 / decode / 端到端', why: '知道瓶颈到底在哪一段' },
  { item: '后端策略', before: '默认使用 CPU 运行库', after: 'CPU 基线封存，GPU 独立构建验证', why: '避免实验失败破坏可用系统' },
]

export const gpuChecklist = [
  {
    title: 'GPU 版运行库编译完成',
    detail: '确认 libsherpa-onnx-cxx-api.so、CUDA Provider、TensorRT Provider 均生成。',
    status: '进行中',
    tone: 'active',
  },
  {
    title: 'Provider 真正注册',
    detail: '查看运行日志与可用 Provider 列表，确认 CUDA / TensorRT 被加载，而不只是文件存在。',
    status: '待验证',
    tone: 'pending',
  },
  {
    title: '算子分配检查',
    detail: '确认 INT8 encoder / decoder 的关键算子是否回退到 CPU，记录实际执行设备。',
    status: '待验证',
    tone: 'pending',
  },
  {
    title: '同音频 CPU / GPU 对照',
    detail: '固定 WAV、固定参数，比较识别文本、decode 时间、RTF、内存与稳定性。',
    status: '待验证',
    tone: 'pending',
  },
  {
    title: '集成与回退开关',
    detail: '把 GPU 运行库接到 udp_vad_qwen3，同时保留 CPU fallback；异常时可一键切回。',
    status: '待做',
    tone: 'pending',
  },
]

export const roadmap = [
  {
    step: '01',
    title: '完成 GPU 编译',
    detail: '继续以 2 个并行任务编译，保存完整日志；先检查产物与依赖，不覆盖 CPU 运行库。',
    status: '进行中',
    tone: 'active',
  },
  {
    step: '02',
    title: '验证 Provider 与算子',
    detail: '确认 CUDA / TensorRT Provider 被注册，检查模型算子实际落在哪个设备上。',
    status: '下一步',
    tone: 'next',
  },
  {
    step: '03',
    title: '用数据决定是否切换',
    detail: '同音频对比 CPU / GPU 的识别结果、RTF、内存、功耗与稳定性；没有收益就保留 CPU。',
    status: '待做',
    tone: 'pending',
  },
  {
    step: '04',
    title: '接入对话脑，形成语音闭环',
    detail: '在 ASR 与 TTS 之间加入意图理解/LLM 与对话状态，先做单轮指令，再做多轮与打断。',
    status: '待做',
    tone: 'pending',
  },
  {
    step: '05',
    title: '接入视觉与技能层',
    detail: '用机器人视角数据解决检测域偏差，再把视觉结果提供给对话与任务规划，逐步接入安全技能。',
    status: '待做',
    tone: 'pending',
  },
  {
    step: '06',
    title: 'ROS 2 化与软著材料',
    detail: '把语音节点接入 ROS 2，补齐接口文档、测试记录与软件著作权材料。',
    status: '待做',
    tone: 'pending',
  },
]

export const risks = [
  {
    risk: 'GPU 算子回退，端到端没有变快',
    impact: '高',
    mitigation: '同音频对照 + 算子分配日志；没有数据就不切换；保留 CPU 回退。',
  },
  {
    risk: 'VAD 切段错误，识别文本断续',
    impact: '高',
    mitigation: '保留每段原始音频，按真实录音调阈值、预录与静音结束；长段切分加重叠。',
  },
  {
    risk: '延迟累积，连续对话卡顿',
    impact: '高',
    mitigation: '为队列与端到端延迟设预算；分离排队/解码耗时；按语音段边界丢弃而不是中途截断。',
  },
  {
    risk: '同音字与噪声导致识别不稳',
    impact: '中',
    mitigation: '用机器人麦克风实录做测试集；必要时加语言模型纠错或换模型，而不是只看单句 demo。',
  },
  {
    risk: '升级系统破坏机器人原有软件栈',
    impact: '高',
    mitigation: '不随意升级系统 Python/GCC；独立运行库与虚拟环境；先备份、再验证、最后切换。',
  },
  {
    risk: '让对话模型直接控制运动',
    impact: '高',
    mitigation: '增加技能层与安全约束；运动必须通过经过测试的接口，不能让 LLM 直接下发关节指令。',
  },
]

export const sprintGoals = [
  {
    title: '视觉模型部署',
    status: '搁置',
    tone: 'paused',
    note: '推理链路已串通；机器人视角数据与域偏差待解决。',
  },
  {
    title: 'ROS 2 集成',
    status: '待开始',
    tone: 'pending',
    note: '语音节点尚未接入 ROS 2，当前以独立 C++ 程序验证。',
  },
  {
    title: '语音部署',
    status: '进行中',
    tone: 'active',
    note: '收音 → VAD → ASR 的 CPU 基线已完成；GPU 验证与对话闭环待做。',
  },
  {
    title: '软著材料',
    status: '待整理',
    tone: 'pending',
    note: '等系统闭环与接口稳定后，再沉淀文档、测试记录与著作权材料。',
  },
]

export const principles = [
  {
    title: '先分段，再串联',
    text: '相机、播放、收音、识别分别验证，每一段都有独立的最小测试，避免多个未知故障叠在一起。',
  },
  {
    title: '保留能工作的基线',
    text: 'CPU 链路跑通后先封存；GPU 版本单独构建、单独验证。实验失败不能把可用系统拖下水。',
  },
  {
    title: '用指标说话',
    text: 'RTF、队列长度、丢包数、非法包数、端到端延迟和内存占用，比“看起来能跑”更接近工程结论。',
  },
  {
    title: '先查接口与数据',
    text: '官方 SDK 头文件、示例和真实音频/文件格式优先于猜测；API 返回成功不代表业务成功。',
  },
  {
    title: '为下一步留接口',
    text: '语音最终要接入对话、视觉与运动；当前每个模块都要为 ROS 2 与技能层留下清晰的调用边界。',
  },
]

export const finalVision = {
  quote: '让这台 G1 像人一样行走、对话，并根据环境与语音输入作出动作和语音反馈。',
  steps: ['听见', '听懂', '回答', '看见', '行动'],
  note: '当前完成的只是「听见 → 听懂」的 CPU 基线，以及独立的「回答」链路；感知与行动仍在后面。',
}
