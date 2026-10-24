<script setup>
import { computed, ref } from 'vue'
import { benchmarks, initialBenchmarks } from '../data/content'

const chartMode = ref('rtf')
const maxRtf = 2
const maxLatency = Math.max(...benchmarks.map((item) => item.audio)) * 1.08

const rtfRows = computed(() => [
  ...initialBenchmarks.map((item) => ({ ...item, group: '初版 · 每段重载', tone: 'initial' })),
  ...benchmarks.map((item) => ({ ...item, group: '优化后 · 常驻模型', tone: 'optimized' })),
])
</script>

<template>
  <div class="latency-chart panel">
    <div class="panel__head panel__head--tight">
      <div>
        <p class="eyebrow">Measured · 实测数据</p>
        <h3>解码耗时与 RTF</h3>
        <p class="panel__lead">
          RTF = 解码耗时 ÷ 音频时长。大于 1 表示处理一段音频比播放它更慢；小于 1 才有实时余量。
        </p>
      </div>

      <div class="segmented" role="group" aria-label="图表类型">
        <button type="button" :class="{ active: chartMode === 'rtf' }" @click="chartMode = 'rtf'">RTF</button>
        <button type="button" :class="{ active: chartMode === 'latency' }" @click="chartMode = 'latency'">耗时</button>
      </div>
    </div>

    <div v-if="chartMode === 'rtf'" class="rtf-chart">
      <div v-for="group in ['初版 · 每段重载', '优化后 · 常驻模型']" :key="group" class="rtf-group">
        <div class="rtf-group__head">
          <h4>{{ group }}</h4>
          <span class="mono-badge">{{ group.startsWith('初版') ? '模型重复加载' : 'CPU 基线' }}</span>
        </div>

        <div class="rtf-bars">
          <div
            v-for="row in rtfRows.filter((item) => item.group === group)"
            :key="row.label"
            class="rtf-row"
          >
            <span class="rtf-row__label">{{ row.label }}<small>{{ row.audio.toFixed(2) }} s 音频</small></span>
            <div class="rtf-row__track">
              <span
                class="rtf-row__bar"
                :class="`rtf-row__bar--${row.tone}`"
                :style="{ width: `${Math.min((row.rtf / maxRtf) * 100, 100)}%` }"
              ></span>
              <span class="rtf-row__value">{{ row.rtf.toFixed(3) }}</span>
            </div>
          </div>
        </div>
      </div>

      <div class="chart-note">
        <span class="chart-note__mark"></span>
        初版数据含每段重新加载模型的开销，因此 RTF 偏高；优化版为模型常驻 + 有界队列后的 decode 基线。
      </div>
    </div>

    <div v-else class="latency-bars">
      <div class="latency-legend">
        <span><i class="legend-dot legend-dot--audio"></i>音频时长</span>
        <span><i class="legend-dot legend-dot--decode"></i>ASR decode</span>
      </div>

      <div v-for="item in benchmarks" :key="item.label" class="latency-row">
        <div class="latency-row__label">
          <strong>{{ item.label }}</strong>
          <small>RTF {{ item.rtf.toFixed(3) }}</small>
        </div>
        <div class="latency-row__bars">
          <div class="latency-bar">
            <span class="latency-bar__fill latency-bar__fill--audio" :style="{ width: `${(item.audio / maxLatency) * 100}%` }"></span>
            <span class="latency-bar__num">{{ item.audio.toFixed(2) }} s</span>
          </div>
          <div class="latency-bar">
            <span class="latency-bar__fill latency-bar__fill--decode" :style="{ width: `${(item.decode / maxLatency) * 100}%` }"></span>
            <span class="latency-bar__num">{{ item.decode.toFixed(3) }} s</span>
          </div>
        </div>
      </div>

      <div class="chart-note">
        <span class="chart-note__mark"></span>
        这三组是优化后集成测试的 decode 数据，不包含收包排队、VAD 与 TTS 的端到端时间。
      </div>
    </div>
  </div>
</template>
