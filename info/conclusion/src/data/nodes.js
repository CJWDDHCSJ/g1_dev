export const nodeMeta = {
  eyebrow: 'Node manual · 概念与 API',
  title: '每个节点，都有它的数据契约和 API',
  lead:
    '把「听见 → 听懂 → 回答」拆成八个可替换的节点。每张卡片先用一句大白话说明它在链路里的角色，再展开输入输出、API 与现场笔记；官方 SDK、推理库与项目自研代码会明确分开标注。',
  stats: [
    { value: '8', label: '关键节点' },
    { value: '5', label: '类 API 来源' },
    { value: '2', label: '条已独立打通的语音链路' },
    { value: '1', label: '个未接入的对话脑' },
  ],
}

export const apiKindLegend = [
  {
    kind: '官方 SDK',
    tone: 'official',
    detail: '来自 Unitree 文档、头文件或官方示例，方法签名与参数语义可在 G1 上直接使用。',
  },
  {
    kind: '推理库',
    tone: 'library',
    detail: '来自 sherpa-onnx、ONNX Runtime、TensorRT 等运行时。版本差异会改变字段名或调用方式。',
  },
  {
    kind: '系统 API',
    tone: 'system',
    detail: 'Linux Socket、OpenCV 等基础接口，负责把机器人数据接进程序。',
  },
  {
    kind: '项目实现',
    tone: 'project',
    detail: '本项目的封装、状态机或约定。函数名与参数以源码为准，页面中的签名是示意。',
  },
  {
    kind: '建议接口',
    tone: 'proposal',
    detail: '为下一步设计的调用边界，尚未实现，也不是当前可运行的 API。',
  },
]

export const statusLegend = [
  { label: '已实测', tone: 'done', detail: '在 G1 上有量化数据或完整端到端结果。' },
  { label: '已跑通', tone: 'done', detail: '链路能完整工作，但还没有系统级基准。' },
  { label: '进行中', tone: 'active', detail: '构建或配置已经开始，缺少最终运行结果。' },
  { label: '未验证', tone: 'pending', detail: '代码或产物存在，但没有可复现的运行证据。' },
  { label: '搁置', tone: 'paused', detail: '链路存在，因关键问题暂停推进。' },
  { label: '未接入', tone: 'pending', detail: '设计存在，尚未实现并接入主链路。' },
]

export const routeComparison = {
  eyebrow: 'Build or reuse',
  title: '同一件事，官方有现成路径，项目为什么自己搭？',
  lead:
    'Unitree 已经提供内置语音服务和 AudioClient。项目只复用「播放与设备控制」，把识别与合成换成可替换的模型；两条路线解决的不是同一个问题。',
  rows: [
    {
      stage: '收音',
      official: '开启麦克风后，内置语音服务完成采集与识别，结果通过 rt/audio_msg 以 JSON 文本形式发布。',
      project: '加入 UDP 多播组，直接拿 16 kHz PCM16 原始采样，自己保存、切段与识别。',
      tradeoff: '多一层工程，但拿到原始音频，能换 ASR、调 VAD、做回放与评测。',
    },
    {
      stage: '识别',
      official: '内置离线 ASR，带 angle、sense、speaker_id 等元信息，接入成本低。',
      project: 'Qwen3-ASR-0.6B INT8，经 sherpa-onnx C++ API 推理，可替换模型并测量 RTF。',
      tradeoff: '模型与运行库都要自己维护；换来模型选择权与可比较的性能数据。',
    },
    {
      stage: '合成',
      official: 'TtsMaker(text, speaker_id)，中文 / 英文角色固定，由机器人内部服务直接发声。',
      project: 'Kokoro-82M v1.1-zh ONNX，中文 G2P 在 Python 侧完成，C++ 只做推理。',
      tradeoff: '多一套模型与前端，但音色、文本前端和输出波形都可控。',
    },
    {
      stage: '播放',
      official: 'TtsMaker 内部完成合成与播放，不暴露中间波形。',
      project: 'Kokoro 输出 float32 24 kHz，转 PCM16、重采样到 16 kHz，再分块 PlayStream。',
      tradeoff: '必须自己处理格式、分块与播放完成时机；换来对波形和播放状态的完全控制。',
    },
    {
      stage: '适用',
      official: '快速做语音 demo、固定中文/英文播报，或只需要一个能响的交互原型。',
      project: '需要自定义音色、替换模型、记录指标，并计划接入对话脑与 ROS 2 的工程链路。',
      tradeoff: '两条路线可以共存：项目复用了 AudioClient 的播放面，而没有使用内置 ASR/TTS。',
    },
  ],
}

export const nodes = [
  {
    id: 'mic',
    no: '01',
    jumpLabel: '麦克风',
    group: 'listen',
    stage: 'INPUT · 采集',
    title: '麦克风 / UDP 多播',
    short: '绕开 ALSA，从机器人身体里拿到原始 PCM',
    status: '已实测',
    tone: 'done',
    tags: ['UDP Multicast', 'PCM16', 'Linux Socket', 'C++'],
    plain:
      '可以把这一环理解成「绕过机器人的内置翻译，直接把最原始的录音拿到手」：G1 内部服务持续把麦克风阵列的声音打包发出，我们的程序加入多播组接收原始 PCM，后续怎么切、怎么识别都由自己决定。',
    concept:
      'G1 的麦克风阵列由内部音频服务采集，服务把 16 kHz、单声道、16-bit PCM 通过 UDP 多播持续发出。应用不直接打开 ALSA 设备，而是加入组播组，把每个 datagram 当作一块原始采样。这样得到的是未经过机器人语音封装的音频，后续可以自己决定 VAD、ASR、保存与评测策略。',
    contract: {
      input: [
        { label: '物理输入', value: 'G1 四麦克风阵列；需音频服务处于可收音状态' },
        { label: '网络入口', value: 'UDP 多播 239.168.123.161:5555' },
        { label: '本机接口', value: '192.168.123.x 网卡；官方示例用 getifaddrs 查找' },
      ],
      output: [
        { label: '采样格式', value: 'PCM16 little-endian · 16 kHz · mono' },
        { label: '包大小', value: '项目实测 5120 B / 包 ≈ 0.16 s' },
        { label: '帧边界', value: 'UDP 包只是传输单位，不等同于一句话' },
      ],
    },
    apis: [
      {
        name: 'socket / bind / recvfrom',
        kind: 'Linux Socket',
        kindTone: 'system',
        signature: `int fd = socket(AF_INET, SOCK_DGRAM, 0);

sockaddr_in local{};
local.sin_family = AF_INET;
local.sin_port = htons(5555);
local.sin_addr.s_addr = INADDR_ANY;
bind(fd, reinterpret_cast<sockaddr *>(&local), sizeof(local));

ssize_t n = recvfrom(fd, buf, sizeof(buf), 0, nullptr, nullptr);`,
        note: '创建 UDP 套接字、绑定 5555 端口并阻塞收包。recvfrom 返回的 n 是本包真实字节数，必须按 n 累积，不能假设固定包长。',
      },
      {
        name: 'IP_ADD_MEMBERSHIP',
        kind: 'Linux Socket',
        kindTone: 'system',
        signature: `ip_mreq mreq{};
inet_pton(AF_INET, "239.168.123.161", &mreq.imr_multiaddr);
mreq.imr_interface.s_addr = inet_addr(local_ip.c_str());
setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));`,
        note: '加入多播组。imr_interface 必须指向 G1 局域网地址，而不是随意一个网卡；接口选错时程序能运行，但收不到音频。',
      },
      {
        name: 'getifaddrs / freeifaddrs',
        kind: '系统 API',
        kindTone: 'system',
        signature: `getifaddrs(&ifaddr);
// 遍历接口，找到 192.168.123.x 的本地地址
freeifaddrs(ifaddr);`,
        note: '官方示例用它在多网卡环境中找到正确的组播接口。项目也应打印最终选中的本地 IP，否则“没有声音”很难定位。',
      },
      {
        name: 'WriteWave',
        kind: '官方示例工具',
        kindTone: 'official',
        signature: 'WriteWave("record.wav", 16000, pcm.data(), pcm.size(), 1);',
        note: '把累积的 PCM 封装成 RIFF/WAVE，仅用于验证采集链路。第三个参数是采样点数，不是字节数；采样率、声道数和位宽必须与真实数据一致。',
      },
    ],
    code: {
      title: '最小收包序列（错误处理略）',
      body: `int fd = socket(AF_INET, SOCK_DGRAM, 0);

sockaddr_in local{};
local.sin_family = AF_INET;
local.sin_port = htons(5555);
local.sin_addr.s_addr = INADDR_ANY;
bind(fd, reinterpret_cast<sockaddr *>(&local), sizeof(local));

ip_mreq mreq{};
inet_pton(AF_INET, "239.168.123.161", &mreq.imr_multiaddr);
mreq.imr_interface.s_addr = inet_addr(local_ip.c_str()); // 192.168.123.x
setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));

std::array<uint8_t, 8192> buf{};
std::vector<uint8_t> pcm;
for (;;) {
  ssize_t n = recvfrom(fd, buf.data(), buf.size(), 0, nullptr, nullptr);
  if (n <= 0) continue;
  pcm.insert(pcm.end(), buf.begin(), buf.begin() + n);
}`,
    },
    notes: [
      {
        title: '2048 B 不是协议常量',
        detail:
          '官方示例的 recvfrom 缓冲区是 2048 B，但项目在 G1 上实际收到 5120 B 的包。若接收缓冲区小于真实 datagram，recvfrom 会截断，音频会悄悄缺样。缓冲区要覆盖观测到的最大包长，并记录每次 n。',
      },
      {
        title: '多播接口必须选对',
        detail:
          '在有多块网卡的 Linux 上，只绑定 INADDR_ANY 并不保证从 G1 内网收流。官方示例扫描 192.168.123.*，项目也应显式设置 imr_interface，并把选中的接口写入日志。',
      },
      {
        title: '包边界不是语义边界',
        detail:
          '5120 B 约等于 0.16 秒音频，但一句话通常跨越几十个包。不要把单包直接送进 ASR；先累积、切段，并处理丢包造成的音频不连续。',
      },
      {
        title: '丢包要能看见',
        detail:
          'UDP 不保证顺序与到达。项目用有界队列缓冲，队列溢出时计数并重置当前 VAD 段，避免把断开前后的音频拼成一句话。实测 390 包、0 非法包、0 溢出的记录比“程序没报错”更有意义。',
      },
    ],
    evidence:
      '项目实测收到 390 个 UDP 包，0 个非法包、0 次队列溢出；保存的 5.12 秒 WAV 格式正确，回放确认是现场声音。5120 B/包与约 0.16 秒的换算来自本次实测，不是官方协议保证。',
  },
  {
    id: 'vad',
    no: '02',
    jumpLabel: 'VAD',
    group: 'listen',
    stage: 'INPUT · 切段',
    title: 'VAD 能量切段',
    short: '把连续音频流切成可以送进离线 ASR 的一句话',
    status: '已实测',
    tone: 'done',
    tags: ['Energy VAD', '状态机', '预录', '有界队列'],
    plain:
      'VAD 是机器人的「听音开关」：听到有人说话就打开录音，安静下来就认为一句结束。它不负责听懂内容，只负责把连续声音切成一句一句。',
    concept:
      '离线 ASR 需要一段完整音频，不能每 0.16 秒调用一次。VAD（Voice Activity Detection）负责判断语音何时开始、何时结束，把连续 PCM 切成句子。项目没有引入神经网络 VAD，而是用能量阈值、静音包计数和起始预录组成状态机；它简单、可解释，但阈值高度依赖机器人本体噪声。',
    contract: {
      input: [
        { label: '音频块', value: 'PCM16 · 16 kHz · mono，按 UDP 包到达' },
        { label: '判定量', value: '每包能量 / RMS 与静音包计数' },
        { label: '控制参数', value: '起始阈值、结束阈值、静音包数、预录长度' },
      ],
      output: [
        { label: '语音段', value: 'float32 [-1, 1] 或 PCM16，仍为 16 kHz mono' },
        { label: '长度上限', value: '10 s 自动切段，保留 0.32 s 重叠' },
        { label: '队列预算', value: '64 包 ≈ 10 s 音频' },
      ],
    },
    apis: [
      {
        name: '项目能量状态机',
        kind: '项目实现',
        kindTone: 'project',
        signature: `PushPacket(const int16_t *pcm, size_t n);
bool SegmentReady() const;
void TakeSegment(std::vector<float> &out);`,
        note: '函数名与参数是示意，真实实现以项目源码为准。状态机维护当前段、静音计数、预录缓冲，并在达到结束条件时把整段交给识别线程。',
      },
      {
        name: 'PCM16 → float32',
        kind: '项目实现',
        kindTone: 'project',
        signature: 'float sample = pcm[i] / 32768.0f;',
        note: 'sherpa-onnx 的 AcceptWaveform 接收 float32 采样。转换必须在切段前后保持一致，不能一段用 int16、一段用 float32。',
      },
      {
        name: '有界队列 + mutex',
        kind: '项目实现',
        kindTone: 'project',
        signature: `std::lock_guard<std::mutex> lock(queue_mutex);
if (queue.size() >= kMaxPackets) {
  ++overflow_count;
  ResetSegment();
}`,
        note: '收包线程与识别线程共享队列，所有状态检查与统计都必须加锁。队列满时不能静默丢弃后继续拼接，否则会把断开的音频当成一句话。',
      },
      {
        name: 'VoiceActivityDetector',
        kind: '推理库（未采用）',
        kindTone: 'library',
        signature: `auto vad = VoiceActivityDetector::Create(config, 30.0f);
vad.AcceptWaveform(samples.data(), samples.size());
while (!vad.IsEmpty()) {
  SpeechSegment seg = vad.Pop();
}`,
        note: 'sherpa-onnx 自带 VAD 封装，支持 Silero / TEN VAD 等模型。本项目没有使用它，而是保留自己的能量状态机；如果环境噪声导致误切，这是下一步可替换的接口。',
      },
    ],
    code: {
      title: '能量 VAD 的核心分支（伪代码）',
      body: `// 函数名与阈值仅作示意，真实实现以源码为准
if (!active && energy > start_threshold) {
  active = true;
  segment = pre_roll;                 // 保留起始预录，避免吞掉声母
}

if (active) {
  segment.insert(segment.end(), packet.begin(), packet.end());
}

if (active && energy < end_threshold) {
  if (++silent_packets >= end_packets) {
    active = false;
    silent_packets = 0;
    if (segment.size() >= min_samples) {
      queue.push(std::move(segment));
    }
  }
} else {
  silent_packets = 0;
}

if (segment.size() >= 10 * 16000) {   // 10 s 上限
  queue.push(segment);                // 保留 0.32 s 重叠后继续
}`,
    },
    notes: [
      {
        title: '能量 VAD 不是“通用语音检测”',
        detail:
          '风扇、关节电机、空调和桌面碰撞都会抬高能量。阈值必须在机器人本体上实录校准；同一套参数换一台机器或换一个房间就可能失效。',
      },
      {
        title: '预录与重叠是两回事',
        detail:
          '预录把语音起点之前的几十毫秒一并保留，避免吞掉声母；0.32 秒重叠则用于长段切分时保住跨切点的词。前者增加段首静音，后者可能造成重复文本，需要在上层去重。',
      },
      {
        title: '10 秒上限是延迟预算',
        detail:
          '离线 ASR 的 decode 时间随音频长度增长。项目把单段限制在 10 秒，并在超长时切分；这与 max_new_tokens=128 共同决定单次识别的时延上限。',
      },
      {
        title: '丢包必须打断当前段',
        detail:
          'UDP 丢包后，前后音频在时间上不连续。项目在队列溢出时重置 VAD 段，而不是继续拼接；代价是可能丢掉半句话，收益是不产生“听起来合理但内容错误”的文本。',
      },
    ],
    evidence:
      '集成链路在一次稳定性测试中完成 6 段完整识别；队列从 500 包降到 64 包，390 个 UDP 包、0 非法包、0 溢出、结束时 0 积压。切段准确率尚未用标注集量化。',
  },
  {
    id: 'asr',
    no: '03',
    jumpLabel: 'ASR',
    group: 'listen',
    stage: 'COGNITION · 识别',
    title: 'Qwen3-ASR 离线识别',
    short: '模型常驻，每段语音只新建一个流',
    status: 'CPU 已实测',
    tone: 'done',
    tags: ['Qwen3-ASR 0.6B', 'INT8', 'sherpa-onnx', 'ARM64'],
    plain:
      'ASR 是机器人的「耳朵 + 听写员」：耳朵（AuT 编码器）负责听清发音，听写员（Qwen3 解码器）负责把发音写成文字。它先把声音压成很短的「声音拼图」，再逐字写出结果。',
    concept:
      '技术上，Qwen3-ASR-0.6B 由 AuT 编码器（180M 参数）与 Qwen3-0.6B 解码器组成。官方把 128 维 Fbank 做 8 倍下采样，得到 12.5 Hz 的音频 token；sherpa-onnx 把 conv frontend、encoder、decoder 与 BPE tokenizer 串成 OfflineRecognizer。本项目使用离线路径：VAD 切段后整段解码，没有增量文本。模型加载一次常驻，每段只新建 OfflineStream；int8 三件套约 42 + 174 + 721 MB，避免反复读取近 1 GB 权重。',
    contract: {
      input: [
        { label: '音频', value: 'float32 · 16 kHz · mono；sample_rate 传数据真实值，sherpa 自动重采样' },
        { label: '模型文件', value: 'conv_frontend + encoder.int8 + decoder.int8 + tokenizer 目录' },
        { label: '运行参数', value: 'CPU · 2 threads · max_new_tokens=128 · max_total_len=512' },
      ],
      output: [
        { label: '主结果', value: 'result.text / result.tokens（Qwen3 路径的可靠字段）' },
        { label: '附加字段', value: 'lang / timestamps / durations 当前为空；时间戳需独立 ForcedAligner' },
        { label: '停止条件', value: 'EOS、max_new_tokens 或 max_total_len 截断' },
        { label: '时延指标', value: 'decode RTF 0.506–0.556（8–14 s 片段）' },
      ],
    },
    apis: [
      {
        name: 'OfflineRecognizerConfig',
        kind: '推理库',
        kindTone: 'library',
        signature: `sherpa_onnx::cxx::OfflineRecognizerConfig config;
config.model_config.num_threads = 2;
config.model_config.provider = "cpu";
config.decoding_method = "greedy_search";`,
        note: '顶层配置。model_config 管模型与线程，feat_config 管特征，decoding_method 决定解码策略；项目使用 CPU 与 greedy_search。',
      },
      {
        name: 'OfflineQwen3ASRModelConfig',
        kind: '推理库',
        kindTone: 'library',
        signature: `config.model_config.qwen3_asr.conv_frontend = "conv_frontend.onnx";
config.model_config.qwen3_asr.encoder = "encoder.int8.onnx";
config.model_config.qwen3_asr.decoder = "decoder.int8.onnx";
config.model_config.qwen3_asr.tokenizer = "tokenizer"; // 目录
config.model_config.qwen3_asr.hotwords = "宇树,G1";   // UTF-8，ASCII 逗号
config.model_config.qwen3_asr.max_total_len = 512;
config.model_config.qwen3_asr.max_new_tokens = 128;`,
        note: 'Qwen3 专用字段。tokenizer 目录需包含 vocab.json、merges.txt、tokenizer_config.json；max_total_len 管 prompt + 音频 + 生成 token 的总 KV，max_new_tokens 只管新生成部分。hotwords 会注入 system prompt，不是识别后的字符串替换。',
      },
      {
        name: 'OfflineRecognizer::Create',
        kind: '推理库',
        kindTone: 'library',
        signature: `static OfflineRecognizer Create(const OfflineRecognizerConfig &config);
bool ok = static_cast<bool>(recognizer.Get());`,
        note: '加载模型并构造识别器。应放在进程启动阶段只调用一次；用继承自 MoveOnly 的 Get() 检查底层句柄是否为空。',
      },
      {
        name: 'OfflineRecognizer::CreateStream',
        kind: '推理库',
        kindTone: 'library',
        signature: 'OfflineStream CreateStream() const;',
        note: '为一段语音创建独立状态。每段都必须用新流；复用旧流会把上一段的音频与解码状态带进来。',
      },
      {
        name: 'OfflineStream::SetOption',
        kind: '推理库',
        kindTone: 'library',
        signature: `void SetOption(const char *key, const char *value) const;
// v1.13.8 的 Qwen3 实现读取：
// hotwords / language / max_new_tokens / max_total_len / temperature / top_p`,
        note: '按句覆盖配置，适合「短指令用小预算、长听写用大预算」，避免为长句重载模型。值都以字符串传入；热词列表保持短小，否则会挤压音频 KV 空间。',
      },
      {
        name: 'OfflineStream::AcceptWaveform',
        kind: '推理库',
        kindTone: 'library',
        signature: 'void AcceptWaveform(int32_t sample_rate, const float *samples, int32_t n) const;',
        note: 'sample_rate 必须是数据的真实采样率；sherpa 会内部重采样到 16 kHz。把 48 kHz 音频错报成 16 kHz 才会变调、语速异常。n 是采样点数。',
      },
      {
        name: 'OfflineRecognizer::Decode / GetResult',
        kind: '推理库',
        kindTone: 'library',
        signature: `recognizer.Decode(&stream);
OfflineRecognizerResult result = recognizer.GetResult(&stream);
std::cout << result.text << std::endl;`,
        note: 'Decode 是同步阻塞调用，应在识别线程执行。Qwen3 路径当前可靠返回 text 与 tokens；官方示例中 lang、timestamps、durations 为空，时间戳需另接 Qwen3-ForcedAligner。',
      },
    ],
    code: {
      title: '常驻识别器 + 按句覆盖参数',
      body: `using namespace sherpa_onnx::cxx;

OfflineRecognizerConfig config;
config.model_config.qwen3_asr.conv_frontend = "conv_frontend.onnx";
config.model_config.qwen3_asr.encoder = "encoder.int8.onnx";
config.model_config.qwen3_asr.decoder = "decoder.int8.onnx";
config.model_config.qwen3_asr.tokenizer = "tokenizer";
config.model_config.qwen3_asr.max_total_len = 512;
config.model_config.qwen3_asr.max_new_tokens = 128;
config.model_config.num_threads = 2;
config.model_config.provider = "cpu";

// 进程启动时加载一次；短静音预热一次 decode
auto recognizer = OfflineRecognizer::Create(config);
if (!recognizer.Get()) {
  std::cerr << "failed to load Qwen3-ASR" << std::endl;
  return 1;
}

// 每段语音一个全新 stream
auto stream = recognizer.CreateStream();
stream.SetOption("hotwords", "宇树,G1,导航点");
stream.SetOption("max_new_tokens", "192");

// 传数据真实采样率，44.1k/48k 由 sherpa 内部重采样
stream.AcceptWaveform(static_cast<int32_t>(actual_sample_rate),
                      samples.data(), samples.size());
recognizer.Decode(&stream);
auto result = recognizer.GetResult(&stream);
std::cout << result.text << std::endl;`,
    },
    notes: [
      {
        title: '常驻模型，流按句创建',
        detail:
          '初版每识别一句就重新加载模型，0.64–0.80 秒音频的 RTF 达到 1.25–1.90。模型常驻后，同一批短音频不再承担加载成本；长片段 RTF 降到 0.506–0.556。',
      },
      {
        title: '离线不等于实时',
        detail:
          '模型要等 VAD 判定语音结束才解码，因此用户说完到看到文本之间有一段静音等待。Qwen3-ASR 模型本身支持 1–8 秒窗口的流式推理，但 sherpa-onnx v1.13.8 的 C++ 路径只提供离线识别；要真流式需换运行时或自行维护增量状态。',
      },
      {
        title: '热词和长度上限共享同一份上下文',
        detail:
          '热词被注入 system prompt，会占用 max_total_len 的 token 预算。热词列表过长或长音频接近上限时，音频 token 可能先被挤掉，表现为「后半句没识别」而不是报错。',
      },
      {
        title: 'INT8 是模型格式，不是 GPU 保证',
        detail:
          'encoder.int8.onnx 与 decoder.int8.onnx 的体积适合 Orin NX，但量化算子不一定都有 CUDA 实现。GPU 版必须检查算子分配，不能因为 Provider 加载成功就认为整网在 GPU 上跑。',
      },
      {
        title: '识别错误要分层归因',
        detail:
          '同音字与词语混淆可能来自麦克风、VAD 切断、模型能力、max_new_tokens 截断或 max_total_len 截断。排查时应保留每段原始 WAV，用同一段音频分别跑模型与人工听写，而不是只看最终文本。',
      },
    ],
    evidence:
      'G1 三段长音频实测：8.16 s → decode 4.534 s，RTF 0.556；13.76 s → 7.604 s，RTF 0.553；10.24 s → 5.184 s，RTF 0.506（Orin NX CPU，2 threads，max_new_tokens=128）。初版短音频 RTF 1.246–1.898，主要来自重复加载模型。官方桌面 CPU、2 threads、512 token 预算下 RTF 约 0.077–0.168，机器与预算均不同，不能直接横比。',
  },
  {
    id: 'dialog',
    no: '04',
    jumpLabel: '对话脑',
    group: 'respond',
    stage: 'COGNITION · 决策',
    title: '对话脑 / 意图理解',
    short: '整条语音闭环里唯一还没有接上的节点',
    status: '未接入',
    tone: 'pending',
    tags: ['LLM 未选型', '意图 / 任务', '建议接口', '安全边界'],
    plain:
      '对话脑是机器人的「大脑」：判断你是在聊天、下命令还是提问，再决定回一句话、调用技能，还是先追问清楚。',
    concept:
      '对话脑负责把 ASR 文本变成下一步：是一句回答、一次工具调用，还是先追问澄清。它需要维护会话状态、理解意图、决定是否调用技能，并把结果交给 TTS 或运动层。本项目没有接入任何 LLM 或对话管理框架，因此这里只能定义调用边界，不能给出已实现的 API。',
    contract: {
      input: [
        { label: '文本', value: 'ASR result.text，可能附带 lang / confidence' },
        { label: '上下文', value: 'session_id、最近若干轮对话、机器人状态' },
        { label: '约束', value: '超时预算、可用工具白名单、安全策略' },
      ],
      output: [
        { label: '回答', value: 'reply_text，送 TTS 合成' },
        { label: '意图', value: 'intent 标签，用于日志与路由' },
        { label: '动作', value: 'tool_calls，必须经过安全门后才执行' },
      ],
    },
    apis: [
      {
        name: 'DialogEngine::Respond',
        kind: '建议接口',
        kindTone: 'proposal',
        signature: `struct DialogRequest {
  std::string session_id;
  std::string text;
  std::vector<Message> history;
};

struct DialogResult {
  std::string reply_text;
  std::string intent;
  std::vector<ToolCall> tool_calls;
};

DialogResult DialogEngine::Respond(const DialogRequest &request);`,
        note: '建议的同步 C++ 边界。真实实现可以是规则引擎、端侧小模型或远程 LLM；接口只约定输入输出，不绑定模型。',
      },
      {
        name: 'LLM Chat API（未选型）',
        kind: '建议接口',
        kindTone: 'proposal',
        signature: `messages: [
  { role: "system", content: "..." },
  { role: "user", content: asr_text }
] -> assistant_message`,
        note: '如果使用 LLM，最常见的输入是消息数组。模型、供应商、端侧/云端尚未选择；必须定义超时、重试和降级，否则 ASR 之后的链路会被网络或显存拖死。',
      },
      {
        name: 'ToolRegistry::Invoke',
        kind: '建议接口',
        kindTone: 'proposal',
        signature: 'ToolResult ToolRegistry::Invoke(const std::string &name, const json &args);',
        note: '把「看看桌上有什么」拆成视觉、导航、播报等技能。工具必须有白名单、参数校验和超时；对话模型不应直接拼接关节控制指令。',
      },
      {
        name: 'SafetyGate::Allow',
        kind: '建议接口',
        kindTone: 'proposal',
        signature: 'bool SafetyGate::Allow(const ToolCall &call, std::string &reason);',
        note: '所有运动相关调用先经过安全层：速度、范围、急停状态、人工确认。语音模型判断错一句话，不能直接变成机器人关节动作。',
      },
    ],
    code: {
      title: '建议的 C++ 边界（尚未实现）',
      body: `struct Message {
  std::string role;
  std::string content;
};

struct ToolCall {
  std::string name;
  std::string arguments_json;
};

class DialogEngine {
 public:
  virtual ~DialogEngine() = default;
  virtual DialogResult Respond(const DialogRequest &request) = 0;
};

// 语音主链路未来的接法：
// ASR text -> DialogEngine::Respond -> reply_text -> Kokoro TTS
//                                -> tool_calls -> SafetyGate -> skills`,
    },
    notes: [
      {
        title: '不要把“未接入”写成“已完成”',
        detail:
          '当前仓库没有对话模型、会话存储或工具调用层。页面保留这一节，是为了明确闭环缺口，而不是暗示已经有一个可用的聊天机器人。',
      },
      {
        title: '先做单轮，再做多轮',
        detail:
          '第一版只需要处理「一句话 → 一个回答或一个技能」；多轮上下文、指代消解、打断与澄清会显著增加状态管理复杂度，应分开验收。',
      },
      {
        title: '延迟预算要前置',
        detail:
          'ASR 的 RTF 已经接近 0.55，TTS 还有推理与播放时间。对话脑必须知道自己最多能花多少毫秒；如果接云端 LLM，网络抖动会直接变成机器人“发呆”。',
      },
      {
        title: '安全层不是可选项',
        detail:
          '涉及行走、转向、抓取的工具调用必须经过独立安全门。对话模型可以提议动作，但不能绕过技能层的范围检查、急停状态与人工确认。',
      },
    ],
    evidence:
      '无运行证据。状态为「未接入」：没有模型、没有接口实现、没有对话数据。页面中的类型与函数名是设计建议，不是现有代码。',
  },
  {
    id: 'tts',
    no: '05',
    jumpLabel: 'TTS',
    group: 'respond',
    stage: 'OUTPUT · 合成',
    title: 'Kokoro 中文 TTS',
    short: '文本 → 音素 → 波形，模型在 C++ 里跑',
    status: '已跑通',
    tone: 'done',
    tags: ['Kokoro-82M v1.1-zh', 'ONNX Runtime 1.22', 'ZHG2P', '311 MB'],
    plain:
      'TTS 是机器人的「嘴巴」：先把文字翻译成一串发音单位，再用声学模型把发音变成波形，最后交给扬声器。',
    concept:
      'Kokoro-82M v1.1-zh 负责把文本变成中文语音。中文 G2P（字 → 音素）没有放进 C++，而是留在 Python 侧用 misaki 的 ZHG2P 完成；C++ 拿到 token id、256 维音色向量和语速，调用 ONNX Runtime 推理，得到 24 kHz float32 波形。随后再转 PCM16、重采样到 16 kHz，交给 G1 扬声器。',
    contract: {
      input: [
        { label: 'input_ids', value: 'int64 [1, N]，由中文音素 tokenizer 生成' },
        { label: 'style', value: 'float32 [1, 256]，音色向量' },
        { label: 'speed', value: 'float32 标量，语速' },
      ],
      output: [
        { label: 'audio', value: 'float32 [1, T]，24 kHz mono' },
        { label: '播放格式', value: 'PCM16 · 16 kHz · mono（重采样后）' },
        { label: '模型体积', value: '约 311 MB ONNX' },
      ],
    },
    apis: [
      {
        name: 'Ort::Session',
        kind: '推理库',
        kindTone: 'library',
        signature: `Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "kokoro");
Ort::SessionOptions options;
Ort::Session session(env, "kokoro-v1.1-zh.onnx", options);`,
        note: '加载 ONNX 模型。311 MB 的模型应在启动时加载一次并复用；每次说话都重建 Session 会把交互延迟拉长。',
      },
      {
        name: 'GetInputNameAllocated',
        kind: '推理库',
        kindTone: 'library',
        signature: `for (size_t i = 0; i < session.GetInputCount(); ++i) {
  auto name = session.GetInputNameAllocated(i, allocator);
  std::cout << name.get() << std::endl;
}`,
        note: '输入名与顺序由导出文件决定。当前导出使用 input_ids / style / speed，但应在运行时枚举确认，不要仅凭文档硬编码。',
      },
      {
        name: 'Ort::Value::CreateTensor',
        kind: '推理库',
        kindTone: 'library',
        signature: `auto memory = Ort::MemoryInfo::CreateCpu(
    OrtArenaAllocator, OrtMemTypeDefault);

std::array<int64_t, 2> ids_shape{1, static_cast<int64_t>(ids.size())};
auto ids_tensor = Ort::Value::CreateTensor<int64_t>(
    memory, ids.data(), ids.size(), ids_shape.data(), ids_shape.size());`,
        note: '为 input_ids、style、speed 分别建张量。dtype、shape 和内存生命周期必须与模型一致；输入名、dtype 或维度错一个，推理就会失败或输出噪声。',
      },
      {
        name: 'Ort::Session::Run',
        kind: '推理库',
        kindTone: 'library',
        signature: `const char *input_names[] = {"input_ids", "style", "speed"};
const char *output_names[] = {"audio"};
auto outputs = session.Run(
    Ort::RunOptions{nullptr},
    input_names, inputs.data(), inputs.size(),
    output_names, 1);`,
        note: '同步执行推理，返回输出张量。输出名同样应由导出文件确认；当前项目按 audio 读取 float32 波形。',
      },
      {
        name: 'misaki ZHG2P',
        kind: '项目实现 / 外部前端',
        kindTone: 'project',
        signature: 'text -> 中文音素 -> token ids',
        note: '中文 G2P 在 Python 侧完成，C++ 不做分词或拼音转换。模型与 tokenizer、G2P 版本必须配套，否则音素表对不上，输出会是含糊或错误的发音。',
      },
      {
        name: 'PCM16 + 重采样',
        kind: '项目实现',
        kindTone: 'project',
        signature: 'float32 [-1, 1] -> int16; 24000 Hz -> 16000 Hz',
        note: 'Kokoro 输出 24 kHz，G1 PlayStream 要求 16 kHz PCM16。转换时要防削波（clamp）、保持单声道，并保证重采样后的时长与播放等待时间一致。',
      },
    ],
    code: {
      title: 'Kokoro ONNX 推理骨架',
      body: `Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "kokoro");
Ort::SessionOptions options;
Ort::Session session(env, "kokoro-v1.1-zh.onnx", options);

std::array<int64_t, 2> ids_shape{1, static_cast<int64_t>(ids.size())};
std::array<int64_t, 2> style_shape{1, 256};
std::array<int64_t, 1> speed_shape{1};

auto memory = Ort::MemoryInfo::CreateCpu(
    OrtArenaAllocator, OrtMemTypeDefault);

std::vector<Ort::Value> inputs;
inputs.push_back(Ort::Value::CreateTensor<int64_t>(
    memory, ids.data(), ids.size(), ids_shape.data(), 2));
inputs.push_back(Ort::Value::CreateTensor<float>(
    memory, style.data(), style.size(), style_shape.data(), 2));
inputs.push_back(Ort::Value::CreateTensor<float>(
    memory, &speed, 1, speed_shape.data(), 1));

const char *input_names[] = {"input_ids", "style", "speed"};
const char *output_names[] = {"audio"};
auto outputs = session.Run(
    Ort::RunOptions{nullptr},
    input_names, inputs.data(), inputs.size(),
    output_names, 1);

const float *audio = outputs[0].GetTensorData<float>();`,
    },
    notes: [
      {
        title: 'C++ 不负责中文 G2P',
        detail:
          '项目把文本前端留在 Python：misaki ZHG2P 生成音素与 token，C++ 只做数值推理。这样 C++ 侧更薄，但部署时必须保证 tokenizer、音色包与模型是同一套版本。',
      },
      {
        title: '输入名是导出属性，不是模型常识',
        detail:
          '不同导出脚本可能把输入命名为 input_ids / style / speed，也可能改变顺序或维度。先用 GetInputNameAllocated 枚举，再用 GetTensorTypeAndShapeInfo 核对 dtype 与 shape。',
      },
      {
        title: '波形要防削波',
        detail:
          'float32 输出可能略微超出 [-1, 1]。直接乘 32767 再转 int16 会溢出回绕，产生爆音；应先 clamp 到 [-1, 1]，再量化。',
      },
      {
        title: '重采样会改变播放时长',
        detail:
          '24 kHz 到 16 kHz 后采样点数变少，但物理时长不变。播放等待时间应按重采样后的时长计算，否则容易提前 PlayStop，截断尾音。',
      },
    ],
    evidence:
      '5 条纯中文语句全部通过，连续两次播放验证成功；模型约 311 MB，C++ 推理链路可接收动态中文输入。首次播放因提前 PlayStop 被截断，修正等待逻辑后恢复完整。',
  },
  {
    id: 'speaker',
    no: '06',
    jumpLabel: '扬声器',
    group: 'respond',
    stage: 'OUTPUT · 播放',
    title: 'G1 扬声器 / AudioClient',
    short: '把内存里的 PCM 分块送进机器人自己的嗓子',
    status: '已跑通',
    tone: 'done',
    tags: ['Unitree SDK2', 'AudioClient', 'PlayStream', '16 kHz PCM16'],
    plain:
      '扬声器这一环，是让数字波形真正变成空气振动。AudioClient 不直接操作喇叭，而是把 PCM 交给 G1 内部的音频服务，由它负责播放。',
    concept:
      'Unitree AudioClient 是 G1 官方的音频控制面：应用把 PCM 数据交给机器人内部音频服务，由服务驱动实体扬声器。PlayStream 接收 app_name、stream_id 和一块 PCM 字节；相同 stream_id 视为连续播放，换一个 stream_id 会打断当前播放。项目把 Kokoro 生成的波形转成 16 kHz PCM16，分块送入，并等到真实播放时长结束后再 PlayStop。',
    contract: {
      input: [
        { label: '数据格式', value: 'PCM16 · 16 kHz · mono' },
        { label: '调用参数', value: 'app_name、stream_id、pcm_data' },
        { label: '设备前置', value: 'ChannelFactory 初始化 + AudioClient.Init' },
      ],
      output: [
        { label: '物理输出', value: 'G1 扬声器（8Ω 3W，峰值 5W）' },
        { label: '返回值', value: '0 表示调用成功，不表示播放完成' },
        { label: '停止', value: 'PlayStop(app_name)' },
      ],
    },
    apis: [
      {
        name: 'ChannelFactory::Init',
        kind: '官方 SDK',
        kindTone: 'official',
        signature: 'unitree::robot::ChannelFactory::Instance()->Init(0, "eth0");',
        note: '初始化 Unitree DDS 通信与网卡。官方示例从命令行接收网络接口名；接口名错误时，AudioClient 调用会失败或超时。',
      },
      {
        name: 'AudioClient::Init / SetTimeout',
        kind: '官方 SDK',
        kindTone: 'official',
        signature: `unitree::robot::g1::AudioClient client;
client.Init();
client.SetTimeout(10.0f);`,
        note: '创建音频客户端并设置 RPC 超时。官方示例在 Init 后设置 10 秒超时；项目应检查每个接口的返回值，而不是只看程序是否继续运行。',
      },
      {
        name: 'AudioClient::PlayStream',
        kind: '官方 SDK',
        kindTone: 'official',
        signature:
          'int32_t PlayStream(std::string app_name, std::string stream_id, std::vector<uint8_t> pcm_data);',
        note: '音频流播放。app_name 标识应用，stream_id 相同表示连续播放、不同表示打断当前播放；pcm_data 必须是 16 kHz、单声道、16-bit PCM。',
      },
      {
        name: 'AudioClient::PlayStop',
        kind: '官方 SDK',
        kindTone: 'official',
        signature: 'int32_t PlayStop(std::string app_name);',
        note: '停止该应用的播放。调用过早会截断缓冲区里尚未播完的音频；必须按实际播放时长等待，而不是数据发送完就停。',
      },
      {
        name: 'SetVolume / GetVolume / LedControl',
        kind: '官方 SDK',
        kindTone: 'official',
        signature: `int32_t SetVolume(uint8_t volume);       // 0 - 100
int32_t GetVolume(uint8_t &volume);
int32_t LedControl(uint8_t R, uint8_t G, uint8_t B);`,
        note: '音量与灯带控制。LedControl 的调用间隔需大于 200 ms；音量是系统级状态，调试时不要默认它一定是 100。',
      },
      {
        name: 'AudioClient::TtsMaker',
        kind: '官方 SDK（未采用）',
        kindTone: 'official',
        signature: 'int32_t TtsMaker(const std::string &text, int32_t speaker_id);',
        note: '官方内置 TTS：speaker_id 0 为中文、1 为英文，不支持中英混合。项目没有使用它，而是用 Kokoro 生成自定义波形后再 PlayStream。',
      },
    ],
    code: {
      title: '播放一段 Kokoro PCM（分块为示意）',
      body: `unitree::robot::ChannelFactory::Instance()->Init(0, "eth0");
unitree::robot::g1::AudioClient client;
client.Init();
client.SetTimeout(10.0f);

const std::string app_name = "g1_kokoro";
const std::string stream_id = "utt-001";
const size_t chunk_bytes = 3200; // 0.1 s @ 16 kHz mono PCM16

for (size_t offset = 0; offset < pcm.size(); offset += chunk_bytes) {
  size_t end = std::min(offset + chunk_bytes, pcm.size());
  std::vector<uint8_t> chunk(pcm.begin() + offset, pcm.begin() + end);
  client.PlayStream(app_name, stream_id, chunk);
}

// 等待真实播放结束，再停止；不要数据发完立刻 PlayStop
std::this_thread::sleep_for(
    std::chrono::milliseconds(duration_ms + 300));
client.PlayStop(app_name);`,
    },
    notes: [
      {
        title: 'API 返回 0 不等于播放完成',
        detail:
          'PlayStream 只表示数据被接受。项目曾把 4.8 秒音频发送完后只等 0.5 秒就 PlayStop，结果只播了一部分。修正方式是按重采样后的真实时长等待，再留一点缓冲。',
      },
      {
        title: 'stream_id 决定“接续”还是“打断”',
        detail:
          '同一句话的分块必须使用相同 stream_id；新一句话应换新的 stream_id，否则可能被当成上一段的续播。反过来，若要主动打断上一句，也应换 ID 或先 PlayStop。',
      },
      {
        title: '格式不对，声音就不对',
        detail:
          'PlayStream 要求 16 kHz 单声道 16-bit PCM。送 24 kHz、双声道或 float32 数据，可能表现为变调、变速、噪声或直接无声；格式转换要在进入播放器之前完成。',
      },
      {
        title: '官方 TTS 与自建 TTS 是两条路',
        detail:
          'TtsMaker 接入最快，但音色与文本前端固定；Kokoro 可换音色、可记录波形与推理耗时，代价是要自己处理 G2P、ONNX 推理、重采样和播放时序。',
      },
    ],
    evidence:
      'Kokoro 输出经 AudioClient 播放，5 条中文语句全部通过，连续两次播放成功。4.8 秒音频在等待 0.5 秒后 PlayStop 会被截断；按真实时长等待后恢复正常。',
  },
  {
    id: 'vision',
    no: '07',
    jumpLabel: '视觉',
    group: 'side',
    stage: 'PERCEPTION · 检测',
    title: 'YOLO / TensorRT 视觉支线',
    short: '链路能跑，但 Python 与 C++ 的结果对不上',
    status: '搁置',
    tone: 'paused',
    tags: ['YOLO', 'ONNX', 'TensorRT FP32', 'OpenCV', '域偏差'],
    plain:
      '视觉支线是机器人的「眼睛」：拍一张图，找出目标，再告诉我们目标在哪。当前模型在 Python 里能检测，换到机器人相机和 C++ 后分数骤降，所以还在排查。',
    concept:
      '视觉支线把自训练 YOLO 模型从 .pt 导出为 ONNX，再构建 TensorRT FP32 engine；G1 相机取 JPEG，OpenCV 解码后做 letterbox、RGB、CHW 与归一化，送入 engine，输出 (1, 8, 8400) 的张量，再做 NMS。Python 侧对同一张测试图能检测成功，C++ 侧最高类别分数却只有约 0.0037，怀疑训练数据与机器人相机存在域偏差。',
    contract: {
      input: [
        { label: '图像', value: 'G1 相机 JPEG → OpenCV BGR frame' },
        { label: '前处理', value: 'letterbox、BGR→RGB、HWC→CHW、归一化' },
        { label: '模型', value: 'best.pt → best.onnx → TensorRT FP32 engine' },
      ],
      output: [
        { label: '原始张量', value: '(1, 8, 8400)；4 个框坐标 + 4 个类别分数' },
        { label: '后处理', value: '置信度过滤 + NMS → boxes / classes / scores' },
        { label: '当前结果', value: 'Python 0.978；C++ 约 0.00366318' },
      ],
    },
    apis: [
      {
        name: 'cv::imdecode',
        kind: '系统 API',
        kindTone: 'system',
        signature: `std::vector<uchar> jpeg = ...;
cv::Mat bgr = cv::imdecode(jpeg, cv::IMREAD_COLOR);`,
        note: '把相机 JPEG 解码为 BGR。空 Mat 表示解码失败；不要在没有检查 empty() 的情况下继续做 resize。',
      },
      {
        name: 'letterbox / RGB / CHW',
        kind: '项目实现',
        kindTone: 'project',
        signature: `cv::resize / cv::copyMakeBorder
cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB)
HWC -> CHW, /255.0f`,
        note: '训练与部署必须使用完全一致的前处理：缩放比例、padding 颜色、通道顺序、归一化方式。任何一项不同都会让置信度塌陷。',
      },
      {
        name: 'TensorRT runtime / engine',
        kind: '推理库',
        kindTone: 'library',
        signature: `auto runtime = nvinfer1::createInferRuntime(logger);
auto engine = runtime->deserializeCudaEngine(data, size);
auto context = engine->createExecutionContext();`,
        note: '反序列化 engine 并创建执行上下文。engine 与 GPU 架构、TensorRT 版本绑定；在开发机构建的 engine 不能假设在 Orin NX 上可直接加载。',
      },
      {
        name: 'enqueueV2 / cudaMemcpyAsync',
        kind: '推理库',
        kindTone: 'library',
        signature: `cudaMemcpyAsync(d_input, input, bytes,
    cudaMemcpyHostToDevice, stream);
context->enqueueV2(bindings, stream, nullptr);
cudaMemcpyAsync(output, d_output, out_bytes,
    cudaMemcpyDeviceToHost, stream);
cudaStreamSynchronize(stream);`,
        note: '异步拷贝与推理必须使用同一个 CUDA stream，并在读取输出前同步。TensorRT 8.5 仍可用 enqueueV2；更高版本 API 有变化。',
      },
      {
        name: '输出解码 + NMS',
        kind: '项目实现',
        kindTone: 'project',
        signature: `(1, 8, 8400)
4 box + 4 class scores
confidence threshold -> IoU NMS`,
        note: '输出布局与类别数必须从模型导出确认。若把 (8, 8400) 的行列或 box/class 分组读错，NMS 会得到完全错误的框。',
      },
    ],
    code: {
      title: 'TensorRT 推理骨架（错误处理略）',
      body: `auto runtime = nvinfer1::createInferRuntime(logger);
auto engine = runtime->deserializeCudaEngine(engine_data, engine_size);
auto context = engine->createExecutionContext();

cudaMemcpyAsync(d_input, input.data(), input_bytes,
    cudaMemcpyHostToDevice, cuda_stream);
context->enqueueV2(bindings, cuda_stream, nullptr);
cudaMemcpyAsync(output.data(), d_output, output_bytes,
    cudaMemcpyDeviceToHost, cuda_stream);
cudaStreamSynchronize(cuda_stream);

// output: (1, 8, 8400) -> 置信度过滤 -> NMS`,
    },
    notes: [
      {
        title: '先做同图对照',
        detail:
          'Python 0.978 与 C++ 0.00366318 的差异，首先要排除前处理不一致：用同一张图、同一组归一化参数、同一份类别表，把中间张量逐层比对，再谈模型泛化。',
      },
      {
        title: '输出形状不是类别定义的证明',
        detail:
          '(1, 8, 8400) 只说明 4 个框坐标 + 4 个类别分数（按当前导出推断）。真实类别数、是否含 objectness、框格式是 xywh 还是 xyxy，都必须从导出脚本与训练配置确认。',
      },
      {
        title: '域偏差是部署问题，不只是模型问题',
        detail:
          '训练集以手机高分辨率图片为主，G1 相机的视角、光照、压缩与噪声分布不同。即使 C++ 预处理完全正确，模型也可能没见过机器人看到的世界。',
      },
      {
        title: '搁置是工程决策',
        detail:
          '视觉链路已串通但结果未验收。主线先转向语音闭环，避免在未验证的检测结果上继续叠加对话与运动逻辑；恢复时需要机器人视角数据与更严格的同图对照。',
      },
    ],
    evidence:
      'Python TensorRT 对测试图检测成功，置信度约 0.978；C++ 实时链路可推理，但最高类别分数约 0.00366318。输入尺寸、通道顺序、归一化、输出形状与 NMS 已逐项核对，域偏差仍是主要怀疑。状态：搁置。',
  },
  {
    id: 'runtime',
    no: '08',
    jumpLabel: '运行时',
    group: 'side',
    stage: 'INFRA · 运行时',
    title: '运行库 / GPU 验证',
    short: '编译成功只是入场券，不是加速证明',
    status: '进行中',
    tone: 'active',
    tags: ['sherpa-onnx', 'ONNX Runtime 1.16', 'CUDA 11.4', 'TensorRT 8.5.2'],
    plain:
      '运行时这一层决定模型「在什么设备上跑」。机器上有 CUDA，不代表当前程序就在用 GPU；CPU 版和 GPU 版是两套运行库，要分别证明。',
    concept:
      'G1 上装有 CUDA、cuDNN、TensorRT 与设备节点，但当前安装的 sherpa-onnx 是 CPU 运行库。项目单独构建 GPU 版：sherpa-onnx 保持 v1.13.8，ONNX Runtime 使用与 Orin NX / CUDA 11.4 匹配的 1.16.0；CMake 打开 GPU、关闭 TTS 与说话人分离。即使产物生成、Provider 加载成功，也必须证明关键算子确实在 GPU 上执行。',
    contract: {
      input: [
        { label: '硬件', value: 'Jetson Orin NX · 8 核 ARM64 · 15 GiB' },
        { label: '软件', value: 'Ubuntu 20.04 · CUDA 11.4 · cuDNN 8.6 · TensorRT 8.5.2' },
        { label: '构建', value: 'sherpa-onnx v1.13.8 + ONNX Runtime 1.16.0 GPU' },
      ],
      output: [
        { label: '产物', value: 'GPU 版 libsherpa-onnx + CUDA / TensorRT Provider' },
        { label: '验证项', value: 'Provider 注册、算子分配、同音频 CPU/GPU 对照' },
        { label: '当前状态', value: '配置关通过；编译与运行验证未完成' },
      ],
    },
    apis: [
      {
        name: 'CMake GPU 构建开关',
        kind: '项目实现',
        kindTone: 'project',
        signature: `-DSHERPA_ONNX_ENABLE_GPU=ON
-DSHERPA_ONNX_ENABLE_TTS=OFF
-DSHERPA_ONNX_ENABLE_SPEAKER_DIARIZATION=OFF
-DCMAKE_BUILD_TYPE=Release`,
        note: '第一次配置仍开着 TTS 与说话人分离，编译到约 42% 停止；重新配置时关闭无关模块，并用 -j2 控制内存占用。',
      },
      {
        name: 'Ort::GetAvailableProviders',
        kind: '推理库',
        kindTone: 'library',
        signature: `for (const auto &provider : Ort::GetAvailableProviders()) {
  std::cout << provider << std::endl;
}`,
        note: '列出运行库实际注册的 Provider。出现 CUDAExecutionProvider / TensorrtExecutionProvider 才说明构建产物带上了对应后端；仅有文件名不算。',
      },
      {
        name: 'AppendExecutionProvider_CUDA / TensorRT',
        kind: '推理库',
        kindTone: 'library',
        signature: `Ort::SessionOptions options;
options.AppendExecutionProvider_CUDA(device_id);
options.AppendExecutionProvider_TensorRT(device_id);`,
        note: '直接使用 ONNX Runtime 时的 Provider 注册方式。sherpa-onnx 内部会按配置选择 Provider；具体可用字符串与构建选项有关，必须看运行日志。',
      },
      {
        name: 'sherpa-onnx provider 配置',
        kind: '推理库',
        kindTone: 'library',
        signature: `config.model_config.provider = "cuda";
config.model_config.num_threads = 2;`,
        note: '把识别器指向 GPU 后端。provider 字符串与构建时启用的 EP 必须匹配；配置写 cuda 但运行库没有 CUDA EP 时，可能回退或直接报错。',
      },
      {
        name: 'ldd / 运行日志',
        kind: '系统 API',
        kindTone: 'system',
        signature: `ldd libsherpa-onnx-cxx-api.so
ldd libonnxruntime.so
# 查看 Provider 注册与算子分配日志`,
        note: '依赖检查能证明动态库找得到，不能证明 GPU 可用。还需要看 Provider 注册日志、算子回退警告与显存占用。',
      },
      {
        name: '同音频对照基准',
        kind: '项目实现',
        kindTone: 'project',
        signature: `同一 WAV + 同一参数
记录：识别文本 / decode / RTF / RSS / 功耗 / 稳定性`,
        note: '只有同输入、同参数的对照才能回答“GPU 是否真的更快”。若 RTF 没有显著改善，保留 CPU 基线是正确决策。',
      },
    ],
    code: {
      title: 'GPU 版运行库构建（示意）',
      body: `cmake -S . -B build-gpu -DCMAKE_BUILD_TYPE=Release -DSHERPA_ONNX_ENABLE_GPU=ON -DSHERPA_ONNX_ENABLE_TTS=OFF -DSHERPA_ONNX_ENABLE_SPEAKER_DIARIZATION=OFF
cmake --build build-gpu -j2

# 构建后先检查产物与依赖
ldd build-gpu/lib/libsherpa-onnx-cxx-api.so
# 再运行识别程序，查看 Provider 与算子分配日志`,
    },
    notes: [
      {
        title: 'Provider 文件存在 ≠ Provider 已注册',
        detail:
          '目录里出现 libonnxruntime_providers_cuda.so，只能说明文件被生成或拷贝。程序必须通过 GetAvailableProviders 或运行日志证明它被加载；否则推理仍在 CPU 上跑。',
      },
      {
        title: 'Provider 注册 ≠ 所有算子都在 GPU',
        detail:
          'INT8 encoder / decoder 的部分算子可能没有 CUDA 实现，会回退到 CPU；跨设备拷贝也会增加开销。必须检查算子分配，而不是只看 Provider 列表。',
      },
      {
        title: '15 GiB 内存要省着用',
        detail:
          '模型约 954 MB，ONNX Runtime、TensorRT workspace、编译器和系统服务都要内存。项目用 2 个并行编译任务，避免编译器把内存吃光导致整机卡死。',
      },
      {
        title: '保留 CPU 回退',
        detail:
          'GPU 版单独构建、单独目录，不覆盖已经稳定的 CPU 运行库。集成时保留运行时开关；实验失败时能一键切回，而不是把可用语音链路一起拖下水。',
      },
    ],
    evidence:
      'CMake 配置关已通过，能识别并下载 CUDA / TensorRT Provider；第一次构建在约 42% 停止，随后关闭 TTS 与说话人分离重新配置。编译完成、Provider 注册、算子分配与加速效果均未验证；状态：进行中。',
  },
]

export const nodeGroups = [
  {
    id: 'listen',
    eyebrow: '01 / Listen',
    title: '听见 → 听懂',
    lead:
      '这一段把物理声音变成文本：UDP 多播拿到原始 PCM，能量 VAD 切出一句话，Qwen3-ASR 在 CPU 上完成离线识别。三个节点都有 G1 上的实测记录。',
    nodes: nodes.filter((node) => node.group === 'listen'),
  },
  {
    id: 'respond',
    eyebrow: '02 / Respond',
    title: '对话 → 回答',
    lead:
      'ASR 之后本应接对话脑，但这一环还没有实现。TTS 与扬声器是另一条已经独立打通的支线：Kokoro 生成波形，AudioClient 驱动实体扬声器。',
    nodes: nodes.filter((node) => node.group === 'respond'),
  },
  {
    id: 'side',
    eyebrow: '03 / Side quests',
    title: '旁支与底座',
    lead:
      '视觉链路已经串通但结果未验收；GPU 运行库仍在构建。它们不是当前语音闭环的一部分，却决定了下一步能走多远。',
    nodes: nodes.filter((node) => node.group === 'side'),
  },
]

export const apiMap = [
  {
    id: 'mic',
    no: '01',
    node: '麦克风 / UDP',
    api: 'socket / bind / IP_ADD_MEMBERSHIP / recvfrom',
    contract: 'PCM16 · 16 kHz · mono · 5120 B/包',
    status: '已实测',
    tone: 'done',
  },
  {
    id: 'vad',
    no: '02',
    node: 'VAD 切段',
    api: '项目能量状态机（未用第三方 VAD）',
    contract: 'PCM16 包 → float32 语音段 · 10 s + 0.32 s overlap',
    status: '已实测',
    tone: 'done',
  },
  {
    id: 'asr',
    no: '03',
    node: 'Qwen3-ASR',
    api: 'OfflineRecognizer + OfflineQwen3ASRModelConfig',
    contract: 'float32 + 真实采样率 → text / tokens（lang、时间戳为空）',
    status: 'CPU 已实测',
    tone: 'done',
  },
  {
    id: 'dialog',
    no: '04',
    node: '对话脑',
    api: '尚未选型（建议 DialogEngine::Respond）',
    contract: 'ASR 文本 + 会话状态 → 回答 / 意图 / tool_calls',
    status: '未接入',
    tone: 'pending',
  },
  {
    id: 'tts',
    no: '05',
    node: 'Kokoro TTS',
    api: 'ONNX Runtime C++ + Kokoro ONNX',
    contract: 'input_ids + style + speed → float32 24 kHz',
    status: '已跑通',
    tone: 'done',
  },
  {
    id: 'speaker',
    no: '06',
    node: 'G1 扬声器',
    api: 'unitree::robot::g1::AudioClient::PlayStream',
    contract: 'PCM16 16 kHz mono → 分块 bytes',
    status: '已跑通',
    tone: 'done',
  },
  {
    id: 'vision',
    no: '07',
    node: 'YOLO / TensorRT',
    api: 'TensorRT + OpenCV',
    contract: 'JPEG → (1, 8, 8400) → NMS boxes',
    status: '搁置',
    tone: 'paused',
  },
  {
    id: 'runtime',
    no: '08',
    node: '运行库 / GPU',
    api: 'sherpa-onnx GPU build + ONNX Runtime 1.16',
    contract: '同一 WAV → CPU / GPU 对照指标',
    status: '进行中',
    tone: 'active',
  },
]

export const sources = [
  {
    kind: 'Unitree 官方',
    label: 'G1 VuiClient / Audio Service',
    url: 'https://support.unitree.com/home/en/G1_developer/VuiClient_Service',
    note: 'AudioClient 方法签名、PlayStream / PlayStop 语义、TtsMaker、rt/audio_msg 字段与音频硬件参数。',
  },
  {
    kind: 'Unitree 官方',
    label: 'G1 音频开发文档（中文）',
    url: 'https://support.unitree.com/home/zh/G1_developer/audio',
    note: '中文音频服务说明与接口列表，用于交叉核对英文文档。',
  },
  {
    kind: 'Unitree SDK2',
    label: 'g1_audio_client.hpp',
    url: 'https://github.com/unitreerobotics/unitree_sdk2/blob/main/include/unitree/robot/g1/audio/g1_audio_client.hpp',
    note: 'AudioClient 头文件；具体方法以机器人上安装的 SDK 版本为准。',
  },
  {
    kind: 'Unitree SDK2',
    label: 'g1_audio_client_example.cpp',
    url: 'https://github.com/unitreerobotics/unitree_sdk2/blob/main/example/g1/audio/g1_audio_client_example.cpp',
    note: 'ChannelFactory 初始化、多播收包、WAV 保存与 AudioClient 调用的官方示例。',
  },
  {
    kind: 'sherpa-onnx',
    label: 'v1.13.8 cxx-api.h',
    url: 'https://github.com/k2-fsa/sherpa-onnx/blob/v1.13.8/sherpa-onnx/c-api/cxx-api.h',
    note: 'OfflineRecognizer、OfflineStream、OfflineQwen3ASRModelConfig 与可选 VAD API 的准确签名。',
  },
  {
    kind: 'sherpa-onnx',
    label: '官方文档',
    url: 'https://k2-fsa.github.io/sherpa/onnx/',
    note: '模型导出、C++ 示例与不同后端的构建方式。',
  },
  {
    kind: 'sherpa-onnx',
    label: 'Qwen3-ASR 使用说明',
    url: 'https://k2-fsa.github.io/sherpa/onnx/qwen3-asr/index.html',
    note: 'Qwen3-ASR 的模型下载、Tokenizer 目录、C++/Python 离线识别示例与热词用法。',
  },
  {
    kind: 'sherpa-onnx',
    label: 'Qwen3-ASR 模型与 RTF',
    url: 'https://k2-fsa.github.io/sherpa/onnx/qwen3-asr/pretrained.html',
    note: 'conv frontend / encoder / decoder 各文件体积，桌面 CPU 2 threads 下的实测 RTF 与配置。',
  },
  {
    kind: '模型',
    label: 'Qwen3-ASR Technical Report',
    url: 'https://arxiv.org/html/2601.21337v1',
    note: 'AuT 编码器 + Qwen3 解码器架构、128 维 Fbank、8 倍下采样与流式/离线能力说明。',
  },
  {
    kind: '模型',
    label: 'Qwen3-ASR-0.6B',
    url: 'https://huggingface.co/Qwen/Qwen3-ASR-0.6B',
    note: '官方权重、模型卡、语言覆盖与推理依赖；本项目使用 sherpa-onnx 转换后的 ONNX 包。',
  },
  {
    kind: '模型',
    label: 'Qwen3-ASR',
    url: 'https://github.com/QwenLM/Qwen3-ASR',
    note: 'Qwen3-ASR 模型家族与推理说明；端侧使用 sherpa-onnx 预转换包。',
  },
  {
    kind: '模型',
    label: 'Kokoro-82M-v1.1-zh',
    url: 'https://huggingface.co/hexgrad/Kokoro-82M-v1.1-zh',
    note: '中文 Kokoro 模型与音色资源；输入输出契约以本项目导出文件为准。',
  },
  {
    kind: '模型',
    label: 'kokoro-onnx',
    url: 'https://github.com/thewh1teagle/kokoro-onnx',
    note: 'Kokoro 的 ONNX 推理参考实现，用于核对 token、style 与采样率。',
  },
  {
    kind: '运行时',
    label: 'ONNX Runtime C/C++ API',
    url: 'https://onnxruntime.ai/docs/api/c/',
    note: 'Ort::Env、Session、Value、Run 与 Provider 相关接口。',
  },
  {
    kind: '运行时',
    label: 'TensorRT Developer Guide',
    url: 'https://docs.nvidia.com/deeplearning/tensorrt/developer-guide/',
    note: 'runtime、engine、execution context、enqueueV2 与 CUDA 内存拷贝。',
  },
]
