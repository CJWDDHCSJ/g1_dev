<script setup>
import { computed, onBeforeUnmount, ref, watch } from 'vue'
import { pipelineStages } from '../data/content'

const presets = {
  a: { label: '8.16 s 片段', audio: 8.16, decode: 4.534, rtf: 0.556 },
  b: { label: '13.76 s 片段', audio: 13.76, decode: 7.604, rtf: 0.553 },
}

const mode = ref('current')
const preset = ref('a')
const isRunning = ref(false)
const activeId = ref('')
const completed = ref([])
const logs = ref([])
const result = ref(null)

let runToken = 0
const timers = new Set()

const stages = computed(() => {
  if (mode.value === 'current') return pipelineStages.slice(0, 3)
  return pipelineStages
})

const currentPreset = computed(() => presets[preset.value])

const progress = computed(() => {
  if (!stages.value.length) return 0
  return Math.round((completed.value.length / stages.value.length) * 100)
})

const stageDurations = {
  mic: 850,
  vad: 650,
  asr: 1600,
  brain: 700,
  tts: 900,
  speaker: 750,
}

function stageLog(stage) {
  if (stage.id === 'mic') return `收到 ${Math.round(currentPreset.value.audio / 0.16)} 个 UDP 包 · PCM 16 kHz / mono`
  if (stage.id === 'vad') return `检测到语音段 · ${currentPreset.value.audio.toFixed(2)} s · 长段自动切分`
  if (stage.id === 'asr') return `decode ${currentPreset.value.decode.toFixed(3)} s · RTF ${currentPreset.value.rtf.toFixed(3)}`
  if (stage.id === 'brain') return mode.value === 'target' ? '对话脑未接入：此处仅为闭环示意' : '跳过'
  if (stage.id === 'tts') return 'Kokoro ONNX 生成波形 · 24k→16k · PCM16'
  if (stage.id === 'speaker') return 'AudioClient.PlayStream · 独立链路已验证'
  return ''
}

function sleep(ms) {
  return new Promise((resolve) => {
    const timer = setTimeout(() => {
      timers.delete(timer)
      resolve()
    }, ms)
    timers.add(timer)
  })
}

function cancel() {
  runToken += 1
  timers.forEach((timer) => clearTimeout(timer))
  timers.clear()
  isRunning.value = false
}

function reset() {
  cancel()
  activeId.value = ''
  completed.value = []
  logs.value = []
  result.value = null
}

async function run() {
  if (isRunning.value) return
  reset()
  isRunning.value = true
  const token = ++runToken

  for (const stage of stages.value) {
    if (token !== runToken) return
    activeId.value = stage.id
    logs.value.push({ id: stage.id, text: stageLog(stage) })
    await sleep(stageDurations[stage.id] ?? 800)
    if (token !== runToken) return
    completed.value.push(stage.id)
  }

  activeId.value = ''
  isRunning.value = false
  result.value = {
    audio: currentPreset.value.audio,
    decode: currentPreset.value.decode,
    rtf: currentPreset.value.rtf,
    mode: mode.value,
  }
}

function stateOf(id) {
  if (activeId.value === id) return 'active'
  if (completed.value.includes(id)) return 'done'
  return 'idle'
}

watch([mode, preset], reset)
onBeforeUnmount(cancel)
</script>

<template>
  <section class="panel pipeline-demo" aria-labelledby="pipeline-demo-title">
    <div class="panel__head">
      <div>
        <p class="eyebrow">Interactive · 链路演示</p>
        <h3 id="pipeline-demo-title">跑一轮语音，看看信号走到哪里</h3>
        <p class="panel__lead">
          动画经过压缩，不是真实耗时。选择「当前链路」只演示已实测的收音、VAD、ASR；选择「目标闭环」会把对话与 TTS 作为示意补上。
        </p>
      </div>
      <span class="mono-badge">DEMO</span>
    </div>

    <div class="demo-controls">
      <div class="segmented" role="group" aria-label="演示模式">
        <button type="button" :class="{ active: mode === 'current' }" @click="mode = 'current'">当前链路</button>
        <button type="button" :class="{ active: mode === 'target' }" @click="mode = 'target'">目标闭环</button>
      </div>

      <div class="segmented" role="group" aria-label="音频片段">
        <button
          v-for="(item, key) in presets"
          :key="key"
          type="button"
          :class="{ active: preset === key }"
          @click="preset = key"
        >
          {{ item.label }}
        </button>
      </div>

      <button class="button button--primary" type="button" :disabled="isRunning" @click="run">
        {{ isRunning ? '运行中…' : '运行演示' }}
      </button>
      <button class="button button--ghost" type="button" :disabled="isRunning && !logs.length" @click="reset">
        重置
      </button>
    </div>

    <div class="demo-stage-wrap">
      <div class="demo-progress" aria-hidden="true">
        <span :style="{ width: `${progress}%` }"></span>
      </div>

      <ol class="demo-stages">
        <li
          v-for="(stage, index) in stages"
          :key="stage.id"
          class="demo-stage"
          :class="[`demo-stage--${stage.tone}`, `is-${stateOf(stage.id)}`]"
        >
          <div class="demo-stage__top">
            <span class="demo-stage__index">{{ String(index + 1).padStart(2, '0') }}</span>
            <span class="demo-stage__status">{{ stage.status }}</span>
          </div>
          <h4>{{ stage.label }}</h4>
          <p>{{ stage.detail }}</p>
          <div class="demo-stage__signal" aria-hidden="true">
            <span></span><span></span><span></span><span></span><span></span>
          </div>
        </li>
      </ol>
    </div>

    <div class="demo-output">
      <div class="demo-log" aria-live="polite">
        <p class="mono-label">Pipeline log</p>
        <p v-if="!logs.length" class="demo-log__empty">等待运行。真实数据来自优化后的 CPU 集成测试。</p>
        <p v-for="log in logs" :key="log.id" class="demo-log__line">
          <span>›</span>{{ log.text }}
        </p>
      </div>

      <div class="demo-result">
        <template v-if="result">
          <p class="mono-label">本轮结果</p>
          <dl class="result-grid">
            <div>
              <dt>音频时长</dt>
              <dd>{{ result.audio.toFixed(2) }} s</dd>
            </div>
            <div>
              <dt>ASR decode</dt>
              <dd>{{ result.decode.toFixed(3) }} s</dd>
            </div>
            <div>
              <dt>RTF</dt>
              <dd>{{ result.rtf.toFixed(3) }}</dd>
            </div>
          </dl>
          <p class="demo-result__note">
            {{
              result.mode === 'current'
                ? '已到达 ASR 输出。对话脑、TTS 与扬声器尚未接入这条当前链路。'
                : '闭环为示意：对话脑尚未接入，TTS 与扬声器只验证过独立链路；端到端延迟未测量。'
            }}
          </p>
        </template>
        <template v-else>
          <p class="mono-label">结果面板</p>
          <p class="demo-result__placeholder">运行后显示实测片段的 decode 时间与 RTF。</p>
        </template>
      </div>
    </div>
  </section>
</template>
