<script setup>
import { RouterLink } from 'vue-router'
import SectionHeading from '../components/SectionHeading.vue'
import MetricCard from '../components/MetricCard.vue'
import { phases, metrics, site, finalVision } from '../data/content'

const waveform = [18, 34, 52, 28, 64, 44, 76, 38, 58, 24, 48, 68, 32, 54, 40, 72, 26, 46, 60, 30, 50, 66, 36, 56]
</script>

<template>
  <div class="view">
    <section class="hero">
      <div class="hero__copy">
        <p class="eyebrow">UNITREE G1-EDU · 30 天冲刺 #11 · 工程复盘</p>
        <h1>
          让 G1 先学会
          <span class="gradient-text">听见、听懂、回答</span>
        </h1>
        <p class="hero__lead">
          这不是一次模型训练，而是一套在机器人本体上落地的端侧语音工程链路：从 G1 内部的 UDP 音频流取音，
          经过 VAD、Qwen3-ASR、对话脑与 Kokoro TTS，再由 Unitree AudioClient 驱动实体扬声器。
          视觉与运动被留在了下一阶段。
        </p>

        <div class="hero__actions">
          <RouterLink class="button button--primary" to="/architecture">查看系统链路</RouterLink>
          <RouterLink class="button button--ghost" to="/retrospective">阅读工程复盘</RouterLink>
        </div>

        <p class="hero__meta">
          {{ site.robot }} · Jetson Orin NX · Ubuntu 20.04 · 15 GiB RAM ·
          <span class="mono-inline">CPU baseline sealed</span>
        </p>
      </div>

      <aside class="hero-panel" aria-label="语音链路状态">
        <div class="hero-panel__head">
          <span class="mono-label">VOICE PATH / STATUS</span>
          <span class="status-pill status-pill--active">CPU 基线</span>
        </div>

        <div class="signal-visual">
          <div class="signal-visual__rings" aria-hidden="true">
            <span></span><span></span><span></span>
          </div>
          <div class="signal-visual__bars" aria-hidden="true">
            <i
              v-for="(height, index) in waveform"
              :key="index"
              :style="{ height: `${height}%`, animationDelay: `${index * 45}ms` }"
            ></i>
          </div>
          <div class="signal-visual__caption">
            <strong>16 kHz</strong>
            <span>PCM16 · mono</span>
          </div>
        </div>

        <ul class="hero-panel__list">
          <li>
            <span>MIC</span>
            <strong>UDP 239.168.123.161:5555</strong>
            <em class="ok">已跑通</em>
          </li>
          <li>
            <span>ASR</span>
            <strong>Qwen3-ASR 0.6B INT8</strong>
            <em class="ok">CPU · RTF 0.55</em>
          </li>
          <li>
            <span>TTS</span>
            <strong>Kokoro v1.1-zh ONNX</strong>
            <em class="ok">5 / 5</em>
          </li>
          <li>
            <span>GPU</span>
            <strong>Orin NX · CUDA 11.4</strong>
            <em class="warn">编译中 / 未验证</em>
          </li>
        </ul>
      </aside>
    </section>

    <section class="section">
      <div class="metrics-grid">
        <MetricCard v-for="(metric, index) in metrics" :key="metric.label" :metric="metric" :index="index" />
      </div>
    </section>

    <section class="section">
      <SectionHeading
        eyebrow="What is this workflow?"
        title="这套流程，本质上在做什么？"
        lead="一句话：把「人说话 → 机器人听懂 → 机器人回应」这条链路，从云端 demo 拆成可在这台 ARM64 机器人上运行、可测量、可回退的工程模块。"
      />

      <div class="explain-grid">
        <article class="explain-card">
          <span class="explain-card__index">01 / 输入</span>
          <h3>从机器人身体里拿到声音</h3>
          <p>
            不依赖系统 ALSA，直接加入 G1 内部音频服务的 UDP 多播流。每包 5120 字节，约 0.16 秒音频；
            C++ 拼接 PCM、VAD 切段，再交给 Qwen3-ASR 转写。
          </p>
          <div class="explain-card__flow">
            <span>UDP</span><i></i><span>VAD</span><i></i><span>ASR</span>
          </div>
        </article>

        <article class="explain-card">
          <span class="explain-card__index">02 / 输出</span>
          <h3>让机器人用自己的嗓子说话</h3>
          <p>
            Kokoro 中文模型导出为 311 MB ONNX；C++ 调用 ONNX Runtime 生成波形，
            转 PCM16、24 kHz 降到 16 kHz，再通过 AudioClient 分块送到 G1 扬声器。
          </p>
          <div class="explain-card__flow">
            <span>Text</span><i></i><span>Kokoro</span><i></i><span>Speaker</span>
          </div>
        </article>

        <article class="explain-card explain-card--accent">
          <span class="explain-card__index">03 / 缺口</span>
          <h3>闭环还差「大脑」和「身体」</h3>
          <p>
            目前只完成了听见、听懂与独立发声。对话理解/LLM、视觉感知、运动技能均未接入；
            GPU 也仅走到运行库构建，尚未证明能加速。
          </p>
          <div class="explain-card__flow">
            <span class="muted">Brain</span><i></i><span class="muted">Vision</span><i></i><span class="muted">Action</span>
          </div>
        </article>
      </div>

      <blockquote class="goal-quote">
        <p>{{ finalVision.quote }}</p>
        <cite>— 项目最终目标</cite>
      </blockquote>
    </section>

    <section class="section">
      <SectionHeading
        eyebrow="Phase map"
        title="六个阶段，一条从练习到系统的路径"
        lead="视觉部署被作为端侧工程练习，语音链路成为第一条真正闭环的候选主线。每个阶段都留下可验证的产物，而不是只留下几条命令。"
      >
        <template #aside>
          <RouterLink class="text-link" to="/retrospective">展开详细复盘 →</RouterLink>
        </template>
      </SectionHeading>

      <div class="phase-grid">
        <RouterLink
          v-for="phase in phases"
          :key="phase.id"
          class="phase-card"
          :class="`phase-card--${phase.tone}`"
          to="/retrospective"
        >
          <div class="phase-card__top">
            <span class="phase-card__no">{{ phase.no }}</span>
            <span class="status-pill" :class="`status-pill--${phase.tone}`">{{ phase.status }}</span>
          </div>
          <h3>{{ phase.title }}</h3>
          <p class="phase-card__sub">{{ phase.subtitle }}</p>
          <p class="phase-card__summary">{{ phase.summary }}</p>
          <span class="phase-card__arrow" aria-hidden="true">↗</span>
        </RouterLink>
      </div>
    </section>

    <section class="section">
      <div class="split-cta">
        <div>
          <p class="eyebrow">Next move</p>
          <h2>CPU 基线已封存，GPU 只差一次诚实的验证。</h2>
          <p>
            编译成功不等于 GPU 可用，Provider 文件存在也不等于算子真的落在 GPU 上。
            下一步是用同一段音频对照 CPU / GPU，再决定是否切换。
          </p>
        </div>
        <div class="split-cta__actions">
          <RouterLink class="button button--primary" to="/roadmap">查看下一步</RouterLink>
          <RouterLink class="button button--ghost" to="/architecture">先理解架构</RouterLink>
        </div>
      </div>
    </section>
  </div>
</template>
