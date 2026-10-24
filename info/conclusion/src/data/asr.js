export const asrDeepDive = {
  eyebrow: 'ASR DEEP DIVE',
  title: '机器人怎么听懂一句话？',
  lead:
    '先做个思想实验：你对着机器人说「向前走」，它收到的其实只是一串忽高忽低的数字。ASR 要做的，是先把这串数字压成模型看得懂的「声音拼图」，再让语言模型一块一块拼出文字。下面先用大白话走一遍，再补上 Fbank、AuT、KV cache 这些术语。',
  summary: '整条链路：声音 → 频谱照片 → 压缩声音块 → 耳朵编码 → 大脑写字 → 查表还原汉字。',
  facts: [
    { label: '模型 / 主角', value: 'Qwen3-ASR-0.6B' },
    { label: '耳朵 / 编码器', value: 'AuT · 180M · 896 hidden' },
    { label: '声音压缩', value: '128 维 Fbank → 8× → 12.5 Hz' },
    { label: '大脑 / 解码器', value: 'Qwen3-0.6B · KV cache' },
    { label: '语言覆盖', value: '30 语言 + 22 中文方言' },
    { label: '本项目路径', value: 'sherpa-onnx 离线 C++ · CPU' },
  ],
  glossary: [
    {
      term: '音频 token',
      plain: '声音的「拼图块」',
      body: '模型不会逐点读波形，而是把约 80 毫秒的声音压成一块；1 秒约 12.5 块。',
    },
    {
      term: 'RTF',
      plain: '解码耗时 ÷ 音频时长',
      body: '0.55 表示 10 秒语音约花 5.5 秒解码；小于 1 才跟得上录音速度。',
    },
    {
      term: 'KV cache',
      plain: '生成文字时的「草稿本」',
      body: '缓存已经算过的上下文，每写一个字不必把前文重算一遍；草稿本越长越占内存。',
    },
  ],
  pipeline: [
    {
      no: '01',
      title: '波形输入',
      subtitle: '空气振动 → 一串数字',
      plain:
        '麦克风不认字，它只记录空气的振动。你每说一个字，振动的强弱就变成一串数字，这就是波形。',
      body:
        '采样率决定每秒记多少点：本项目统一到 16 kHz、单声道、float32。AcceptWaveform 的第一个参数必须是数据真实采样率，sherpa 会内部重采样；把 48 kHz 错报成 16 kHz，声音就会变调。',
      tech: 'OfflineStream::AcceptWaveform',
    },
    {
      no: '02',
      title: '128 维 Fbank',
      subtitle: '把声音切成「频谱照片」',
      plain:
        '模型不直接看波形，而是先给声音拍「频谱照片」：每 10 毫秒一张，每张记录 128 个频段的能量。',
      body:
        'Qwen3-ASR 使用 Whisper 风格的 128 维 log-mel/Fbank，而不是传统 Kaldi 的 80 维；随后做特征归一化。前端参数必须与导出的 ONNX 对齐，否则模型听到的世界就变了。',
      tech: 'WhisperTag · kQwen3MelDim = 128',
    },
    {
      no: '03',
      title: 'Conv frontend',
      subtitle: '8 张照片并成 1 块',
      plain:
        '如果每秒 100 张照片都丢给大模型，太占地方。conv frontend 像拼图，把相邻照片合并，约 8 张并成 1 块。',
      body:
        '卷积前端把声学帧下采样约 8 倍，100 帧压成十几个音频 token，帧率从 100 Hz 降到 12.5 Hz。这个 42 MB 的 frontend 输出低帧率 token 序列，交给 AuT 编码器继续处理。',
      tech: 'conv_frontend.onnx · 42 MB',
    },
    {
      no: '04',
      title: 'AuT encoder',
      subtitle: '模型的「耳朵」',
      plain:
        'AuT 是耳朵：它把压缩后的声音块听成「声学表示」，保留发音、语调等信息，再交给大脑。',
      body:
        'AuT 是独立预训练的注意力编码器，0.6B 模型里约 180M 参数、896 维隐藏层。10 秒语音只剩约 125 个位置，远短于 1,000 帧原始声学序列，这是长音频仍能推理的关键。',
      tech: 'encoder.int8.onnx · 174 MB',
    },
    {
      no: '05',
      title: 'KV-cache decoder',
      subtitle: '模型的「大脑」',
      plain:
        'Qwen3 是大脑：它一边看耳朵送来的声音表示，一边一个 token 一个 token 地猜下一个字，直到写出句号。',
      body:
        '解码器把音频表示、system prompt（含热词 / 语言提示）作为上下文，自回归生成文本。KV cache 像草稿本，缓存已经算过的键值，避免每写一个字都重算前文；草稿本越长越占内存，所以还需要 max_total_len。',
      tech: 'decoder.int8.onnx · 721 MB',
    },
    {
      no: '06',
      title: '文本落地',
      subtitle: '查表，把 token 变回汉字',
      plain:
        '模型吐出的是 token，不是汉字。最后用词表把 token 查回文字，再去掉多余前缀，你才看到一句正常的话。',
      body:
        'BPE detokenize 依赖 tokenizer 目录中的 vocab/merges；实现会去掉 language 前缀与非法 UTF-8，可选同音替换器再做词级修正。C++ 侧真正稳定可用的结果仍是 result.text 与 result.tokens。',
      tech: 'tokenizer/ · result.text / tokens',
    },
  ],
  concepts: [
    {
      no: '01',
      title: '离线识别 ≠ 流式识别',
      plain:
        '我们的做法更像「录音笔」：等你说完、停顿够久，才把整段录音送去转写，而不是边说边出字幕。',
      body:
        'Qwen3-ASR 模型本身支持 1–8 秒动态窗口的流式推理，单次最长可处理 1200 秒音频；但 sherpa-onnx v1.13.8 提供的 Qwen3 路径是 OfflineRecognizer：要等整段音频收完才 Decode，期间没有增量文本。文档里的「simulated streaming」只是 VAD 切段后逐段离线识别，不是真正的流式解码。',
    },
    {
      no: '02',
      title: '12.5 Hz 是压缩，不是采样率',
      plain:
        '别被单位吓到：它不是每秒只录 12.5 次音，而是模型每秒只保留约 12.5 张「声音缩略图」。',
      body:
        '12.5 Hz 指编码器每秒输出约 12.5 个音频 token，不是音频采样率。128 维 Fbank 每秒 100 帧，经 8 倍下采样后进入低帧率空间；10 秒话从 1,000 帧变成约 125 个位置，注意力和 KV cache 才可控。sherpa 以 100 个 mel 帧为一个 chunk 做映射，和官方 AuT 的 8 倍下采样是同一个量级。',
    },
    {
      no: '03',
      title: 'AuT 管「听清」，Qwen3 管「写对」',
      plain:
        '可以把 AuT 当成耳朵，把 Qwen3 当成大脑：耳朵负责听清声音，大脑负责把意思写成通顺的字。',
      body:
        'AuT encoder 把声音编码为声学表示，Qwen3-0.6B decoder 利用语言模型能力输出文字和标点。它不是 CTC，因此不天然产生帧级对齐；官方时间戳来自独立的 Qwen3-ForcedAligner，本项目并未导出或接入，所以不要期待 result.timestamps 有值。',
    },
    {
      no: '04',
      title: '两个长度上限，管的是两件事',
      plain:
        '模型的工作台只有固定大小：max_total_len 是整张桌子的面积，max_new_tokens 是「最多写多少字」的限额。',
      body:
        'max_total_len 限制 prompt + 音频占位 + 已生成 token 的总 KV 空间，默认 512；max_new_tokens 只限制新生成 token 数，默认 128。前者不够会截断音频 token（识别内容变少），后者不够会把句子尾部截掉。调参前先分清是哪一种截断。',
    },
    {
      no: '05',
      title: 'greedy 让识别可复现',
      plain:
        '每次选字时，模型都挑当前最有把握的那个，像考试只填最有把握的答案，而不是掷骰子。',
      body:
        '配置里的 temperature 默认 1e-6，实现等价于取 argmax；top_p 和 seed 只有在主动提高 temperature 时才有意义。ASR 通常应保持贪心解码：同一段音频结果稳定，才方便用 CER 做回归。采样解码会让每次结果略有不同，不适合作为默认值。',
    },
    {
      no: '06',
      title: '热词是 prompt 偏置，不是纠错',
      plain:
        '热词像考前给模型划重点，而不是考完再改答案；它只能提高模型「想到这个词」的概率。',
      body:
        '热词被拼进 system prompt，改变解码器对下一个 token 的概率，而不是在识别完成后替换字符串。官方示例给绕口令加入「骨质疏松症患者、紫色柿子树、年年恋牛娘」等词后，部分低频词被纠正，但仍有错误；热词能提高召回，也可能带来误触发，必须用领域音频 A/B。同一套 prompt 机制还支持 language 选项，它只影响解码条件，不会让 result.lang 变成可靠字段。',
    },
    {
      no: '07',
      title: 'INT8 省的是内存带宽',
      plain:
        'INT8 就像把模型文件从「无损音乐」压成「高码率 MP3」：体积小、读得快，但可能丢一点细节。',
      body:
        'encoder.int8 174 MB、decoder.int8 721 MB，相比 FP32 显著缩小；在 Orin NX 这类带宽有限的 ARM CPU 上，收益往往比「算力更多」更直接。但量化会损失少量精度，而且并非所有 INT8 算子都有 CUDA 实现；切 GPU 时要检查算子分配，而不是只看 Provider 名称。',
    },
    {
      no: '08',
      title: '静音与循环输出有护栏',
      plain:
        '还有两个安全阀：全是静音时不硬猜；模型如果像卡带一样重复，就自动剪掉重复并停下。',
      body:
        'sherpa 会先裁掉尾部的全静音音频；若整段都是静音，直接返回空文本，避免热词把静音「脑补」成一句话。解码若在 64 token 窗口内退化成几个 token 的循环，实现会截断重复部分并结束，防止把 max_new_tokens 跑满。护栏降低异常输出，但不能替代 VAD 和音频质量。',
    },
  ],
  optimizations: [
    {
      priority: 'P0',
      name: '模型常驻 + 预热',
      plain: '别每说一句就把近 1 GB 的模型从硬盘搬一次。',
      state: '已做',
      gain: '短音频 RTF 从 1.246–1.898 降到长音频 0.506–0.556，不再为每句话支付加载成本。',
      cost: '约 1 GB 模型常驻内存；首次推理仍需预热。',
      how: '启动时 Create 一次，用一段静音或短 WAV 跑一次 Decode；每段语音只 CreateStream。',
    },
    {
      priority: 'P0',
      name: '按交互目标切段',
      plain: '把长录音切成短句；切太碎会丢上下文，切太晚又显得反应慢。',
      state: '已做 · 可继续调',
      gain: '10 秒上限控制最坏解码时间；更早判定句尾可减少「说完还发呆」。',
      cost: '切得太短会损失上下文、放大编码器固定开销；0.32 秒重叠可能产生重复词。',
      how: '记录说完到出字的 p50/p95；VAD 结束静音在 0.3–0.6 秒间扫；重叠区在文本层做去重。',
    },
    {
      priority: 'P1',
      name: '线程数扫描',
      plain: '多派几个「帮手」不一定更快，因为机器人还要同时听、说、看。',
      state: '当前 2 threads',
      gain: '更多 intra-op 线程可能缩短 encoder/decoder 时间；官方示例使用 2–3 线程。',
      cost: '8 核 Orin 还要养麦克风、VAD、TTS；线程过多会互相抢 CPU、抬高尾延迟。',
      how: '固定同一组 WAV，分别跑 2/3/4 线程，记录 RTF、p95 与整机负载，而不是只看平均值。',
    },
    {
      priority: 'P1',
      name: 'max_new_tokens 按场景设置',
      plain: '短命令少给点「写字额度」，长听写多给点，避免句尾被截掉。',
      state: '当前 128',
      gain: '短指令下限制最坏循环；长听写时给足空间，避免尾句被截断。',
      cost: '数值越大，最坏解码时间越长；仍需 EOS 和循环护栏。',
      how: '短指令 64–128；长听写 256–512。检查输出是否以异常截断结尾，按 P95 调。',
    },
    {
      priority: 'P1',
      name: '让 max_total_len 匹配音频',
      plain: '工作台就那么大，热词、音频、生成的字都要抢位置。',
      state: '当前 512',
      gain: '保证 system prompt、音频 token 与生成 token 都有 KV 空间，避免音频被静默截断。',
      cost: '上限越大，KV cache 与内存访问越多；超过模型导出上限需要重导 decoder。',
      how: '打开 debug，观察 audio_token_len 与截断日志；长音频 + 热词时尤其要复核。',
    },
    {
      priority: 'P2',
      name: '领域热词 A/B',
      plain: '先把机器人名、地点、专有名词划成重点，再看识别有没有变准。',
      state: '尚未启用',
      gain: '官方示例中「骨质疏松症患者」「紫色柿子树」等低频词被部分纠正（但仍有错误）；适合机器人名、地点、专有名词。',
      cost: '热词会占 prompt token，过多会挤压音频空间；也可能让本来正确的同音词被带偏。',
      how: '用 SetOption("hotwords", "...") 按句传入；只保留高价值词，比较热词前后的 CER 与误触发。',
    },
    {
      priority: 'P2',
      name: 'INT8 / GPU Provider 验证',
      plain: '压缩模型适合小机器；想上 GPU，要确认算子真的跑在 GPU 上。',
      state: 'CPU INT8 已跑 · GPU 待验',
      gain: 'INT8 适合 Orin CPU 的带宽与内存；CUDA Provider 若真生效，可显著缩短大矩阵计算。',
      cost: '量化算子可能回退 CPU；GPU 需要 FP32/FP16 版本与独立的精度回归。',
      how: '用 Ort::GetAvailableProviders、session 日志和算子分配确认；同一 WAV 比 CPU/GPU 的 RTF 与 CER。',
    },
    {
      priority: 'P3',
      name: '音频卫生与半双工',
      plain: '有时问题不在模型，而在麦克风、回声和机器人自己的喇叭。',
      state: '建议 · 未验证',
      gain: '降低 TTS 自听、混响、削顶导致的识别错误，通常比换模型更划算。',
      cost: 'AEC 实现复杂；播报时暂停识别会牺牲自然打断。',
      how: '先录原始 WAV 复盘；尝试播报期间暂停 ASR，再评估回声消除或波束成形。',
    },
  ],
  benchmarks: [
    {
      env: 'sherpa-onnx 文档环境 · 桌面 CPU · 2 threads',
      sample: '272.4 s / 334.2 s 长音频',
      config: 'max_new_tokens=512',
      rtf: '0.077 / 0.103',
    },
    {
      env: 'sherpa-onnx 文档环境 · 桌面 CPU · 2 threads',
      sample: '5.08 s / 11.38 s / 20.76 s',
      config: 'max_new_tokens=512',
      rtf: '0.119 / 0.168 / 0.146',
    },
    {
      env: 'G1 · Orin NX CPU · 2 threads',
      sample: '8.16 s / 10.24 s / 13.76 s',
      config: 'max_new_tokens=128',
      rtf: '0.556 / 0.506 / 0.553',
    },
    {
      env: 'G1 初版 · 每句重载模型',
      sample: '0.64–0.80 s 短音频',
      config: '加载成本占主导',
      rtf: '1.246–1.898',
    },
  ],
  benchmarkNote:
    '看这张表先记住：RTF 越小越快，但不能跨机器、跨音频长度直接横比。官方文档是桌面 CPU，且使用 512 个新 token 的预算；G1 是 ARM CPU、10 秒级片段、128 token 预算。这组数字只用于确认量级和定位瓶颈，不作为「谁更快」的结论。官方 Qwen3-ASR 服务端还报告了 92 ms TTFT、128 并发下 RTF 0.064——那是 vLLM + bfloat16 + CUDA Graph 的服务器吞吐，不是 G1 的单机离线路径。',
  pitfalls: [
    {
      title: 'RTF 0.55 不等于「实时」',
      body:
        'RTF 只描述解码耗时与音频时长的比例。10 秒语音在 RTF 0.55 下约需 5.5 秒解码，之后还要加上 VAD 等待句尾的静音时间与 TTS。用户感知的是「说完到听见回答」的端到端时间，不是 RTF。',
    },
    {
      title: 'lang 和 timestamps 不要想当然',
      body:
        '会识别 52 种语言/方言，不等于每个字段都会填好。sherpa-onnx 的 Qwen3-ASR 路径在官方示例中返回的 lang、timestamps、durations 为空；时间戳需要独立的 ForcedAligner。业务逻辑不要依赖这些字段。',
    },
    {
      title: '采样率要传真实值',
      body:
        '录音的采样率要如实上报，不能替模型「猜」。AcceptWaveform 的第一个参数是数据的实际采样率；传入 48 kHz 时 sherpa 会自动重采样，把 48 kHz 错报成 16 kHz 才会变调。',
    },
    {
      title: '先判断截断，再怪模型',
      body:
        '输出缺斤少两时，先查预算，再怪模型。尾句丢失先看 max_new_tokens 是否到顶；长音频缺内容先看 max_total_len 是否截断了音频 token；热词加入后变差则先看 prompt 是否挤压了音频空间。',
    },
  ],
  code: {
    title: '按句注入热词与生成长度（v1.13.8 per-stream option）',
    body: `auto stream = recognizer.CreateStream();

// 只影响当前这句话；逗号分隔，UTF-8
stream.SetOption("hotwords", "宇树,G1,导航点");
stream.SetOption("max_new_tokens", "192");
stream.SetOption("max_total_len", "512");

stream.AcceptWaveform(16000, samples.data(), samples.size());
recognizer.Decode(&stream);
auto result = recognizer.GetResult(&stream);
// 以 result.text / result.tokens 为准；lang/timestamps 当前为空`,
  },
  references: [
    {
      label: 'sherpa-onnx · Qwen3-ASR 使用说明',
      url: 'https://k2-fsa.github.io/sherpa/onnx/qwen3-asr/index.html',
    },
    {
      label: 'sherpa-onnx · 预训练模型与 RTF 示例',
      url: 'https://k2-fsa.github.io/sherpa/onnx/qwen3-asr/pretrained.html',
    },
    {
      label: 'sherpa-onnx v1.13.8 · C++ API',
      url: 'https://github.com/k2-fsa/sherpa-onnx/blob/v1.13.8/sherpa-onnx/c-api/cxx-api.h',
    },
    {
      label: 'Qwen3-ASR Technical Report',
      url: 'https://arxiv.org/html/2601.21337v1',
    },
    {
      label: 'Qwen3-ASR-0.6B · Hugging Face',
      url: 'https://huggingface.co/Qwen/Qwen3-ASR-0.6B',
    },
  ],
}
