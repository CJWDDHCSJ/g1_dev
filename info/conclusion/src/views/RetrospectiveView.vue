<script setup>
import { RouterLink } from 'vue-router'
import SectionHeading from '../components/SectionHeading.vue'
import PhaseTimeline from '../components/PhaseTimeline.vue'
import LatencyChart from '../components/LatencyChart.vue'
import PitfallGrid from '../components/PitfallGrid.vue'
import { phases, optimizations, principles } from '../data/content'
</script>

<template>
  <div class="view">
    <section class="page-hero">
      <p class="eyebrow">Field notes · 工程复盘</p>
      <h1>从「能跑」到「可用」，中间隔着很多次验证</h1>
      <p class="page-hero__lead">
        这场 30 天冲刺的第 11 次会话，覆盖了视觉、TTS、麦克风采集、ASR、系统集成与 GPU 尝试。
        真正有价值的不是命令清单，而是每一次「以为成功」之后如何重新确认。
      </p>
      <div class="page-hero__stats">
        <div><strong>333</strong><span>用户输入</span></div>
        <div><strong>148 万</strong><span>对话字符</span></div>
        <div><strong>6</strong><span>阶段产物</span></div>
        <div><strong>1</strong><span>仍在编译的 GPU</span></div>
      </div>
    </section>

    <section class="section">
      <SectionHeading
        eyebrow="Timeline"
        title="六个阶段，逐段验证"
        lead="每一段都尽量独立验证：先让单段程序跑通，再接入下一条链路。失败时保留现场、备份源码、不覆盖能工作的版本。"
      />
      <PhaseTimeline :phases="phases" detailed />
    </section>

    <section class="section">
      <SectionHeading
        eyebrow="Performance"
        title="优化前后，数据怎么变"
        lead="初版的 RTF 大于 1，不是模型单次推理慢，而是每段都重新加载模型；模型常驻并修好队列后，长片段 decode RTF 降到约 0.51–0.60。"
      />
      <LatencyChart />
    </section>

    <section class="section">
      <SectionHeading
        eyebrow="Engineering ledger"
        title="改了什么，为什么改"
        lead="优化不是盲目调参，而是把「延迟、连续性、并发安全」逐项落到代码结构上。"
      />

      <div class="opt-table" role="table" aria-label="优化项对照">
        <div class="opt-table__row opt-table__row--head" role="row">
          <span role="columnheader">优化项</span>
          <span role="columnheader">之前</span>
          <span role="columnheader">之后</span>
          <span role="columnheader">目的</span>
        </div>
        <div v-for="row in optimizations" :key="row.item" class="opt-table__row" role="row">
          <span role="cell" class="opt-table__item">{{ row.item }}</span>
          <span role="cell">{{ row.before }}</span>
          <span role="cell" class="opt-table__after">{{ row.after }}</span>
          <span role="cell" class="opt-table__why">{{ row.why }}</span>
        </div>
      </div>
    </section>

    <section class="section">
      <SectionHeading
        eyebrow="Pitfalls"
        title="六个值得记住的坑"
        lead="点击卡片展开：现象、原因、处理与一句话经验。它们比顺利时的日志更有复用价值。"
      />
      <PitfallGrid />
    </section>

    <section class="section">
      <SectionHeading
        eyebrow="Working principles"
        title="这套工程方法，可以迁移到下一个模型"
        lead="换一个视觉模型、换一台机器人，这些原则仍然成立。"
      />

      <div class="principle-grid">
        <article v-for="(item, index) in principles" :key="item.title" class="principle-card">
          <span class="principle-card__no">{{ String(index + 1).padStart(2, '0') }}</span>
          <h3>{{ item.title }}</h3>
          <p>{{ item.text }}</p>
        </article>
      </div>
    </section>

    <section class="section">
      <div class="split-cta">
        <div>
          <p class="eyebrow">Roadmap</p>
          <h2>下一个里程碑，不是再加一个模型。</h2>
          <p>先把 GPU 验证做诚实，再把对话脑接进来，形成真正的语音闭环；视觉和运动排在后面。</p>
        </div>
        <div class="split-cta__actions">
          <RouterLink class="button button--primary" to="/roadmap">查看路线与风险</RouterLink>
          <RouterLink class="button button--ghost" to="/architecture">回到架构图</RouterLink>
        </div>
      </div>
    </section>
  </div>
</template>
