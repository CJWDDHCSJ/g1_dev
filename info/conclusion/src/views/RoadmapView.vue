<script setup>
import { RouterLink } from 'vue-router'
import SectionHeading from '../components/SectionHeading.vue'
import { gpuChecklist, roadmap, risks, sprintGoals, finalVision } from '../data/content'
</script>

<template>
  <div class="view">
    <section class="page-hero">
      <p class="eyebrow">Roadmap · 下一步</p>
      <h1>先让 GPU 证明自己，再谈加速</h1>
      <p class="page-hero__lead">
        当前 CPU 基线已经能持续收音、切段和识别。下一阶段最重要的不是继续堆模型，
        而是完成 GPU 运行库的诚实验证、接上对话脑，并逐步把视觉与运动纳入闭环。
      </p>

      <div class="roadmap-status">
        <div class="roadmap-status__item is-done">
          <span class="status-dot status-dot--done"></span>
          <strong>CPU 基线</strong>
          <small>已封存 · RTF 0.51–0.60</small>
        </div>
        <div class="roadmap-status__item is-active">
          <span class="status-dot status-dot--active"></span>
          <strong>GPU 运行库</strong>
          <small>编译中 · 尚未验证</small>
        </div>
        <div class="roadmap-status__item">
          <span class="status-dot"></span>
          <strong>对话脑</strong>
          <small>未接入</small>
        </div>
        <div class="roadmap-status__item">
          <span class="status-dot"></span>
          <strong>视觉 / 运动</strong>
          <small>搁置 / 未开始</small>
        </div>
      </div>
    </section>

    <section class="section">
      <SectionHeading
        eyebrow="GPU gate"
        title="GPU 不是打开一个开关，而是一组验收关卡"
        lead="每一关都必须有证据。任何一关没有数据，就不把 GPU 版本接进正式语音链路。"
      />

      <div class="checklist">
        <article
          v-for="(item, index) in gpuChecklist"
          :key="item.title"
          class="checklist-item"
          :class="`checklist-item--${item.tone}`"
        >
          <span class="checklist-item__box" aria-hidden="true">{{ item.status === '进行中' ? '…' : index + 1 }}</span>
          <div>
            <div class="checklist-item__head">
              <h3>{{ item.title }}</h3>
              <span class="status-pill" :class="`status-pill--${item.tone}`">{{ item.status }}</span>
            </div>
            <p>{{ item.detail }}</p>
          </div>
        </article>
      </div>

      <aside class="callout callout--mint">
        <span class="callout__icon" aria-hidden="true">✓</span>
        <div>
          <strong>验收标准：同一段音频、同一套参数。</strong>
          <p>
            记录识别文本、decode 时间、RTF、内存、功耗与稳定性；如果 GPU 没有显著收益，保留 CPU 才是正确决策。
          </p>
        </div>
      </aside>
    </section>

    <section class="section">
      <SectionHeading
        eyebrow="Sequence"
        title="六步路线：从运行库到完整闭环"
        lead="先验证底层，再改上层。不要在同一轮里同时改队列、VAD 和推理后端。"
      />

      <ol class="roadmap-list">
        <li v-for="item in roadmap" :key="item.step" class="roadmap-step" :class="`roadmap-step--${item.tone}`">
          <div class="roadmap-step__step">{{ item.step }}</div>
          <div class="roadmap-step__body">
            <div class="roadmap-step__head">
              <h3>{{ item.title }}</h3>
              <span class="status-pill" :class="`status-pill--${item.tone}`">{{ item.status }}</span>
            </div>
            <p>{{ item.detail }}</p>
          </div>
        </li>
      </ol>
    </section>

    <section class="section">
      <SectionHeading
        eyebrow="30-day sprint"
        title="原计划还剩什么"
        lead="视觉、ROS 2、语音与软著材料是这次 30 天冲刺的四块交付。语音推进最快，但还没有形成完整闭环。"
      />

      <div class="sprint-grid">
        <article v-for="goal in sprintGoals" :key="goal.title" class="sprint-card" :class="`sprint-card--${goal.tone}`">
          <div class="sprint-card__head">
            <h3>{{ goal.title }}</h3>
            <span class="status-pill" :class="`status-pill--${goal.tone}`">{{ goal.status }}</span>
          </div>
          <p>{{ goal.note }}</p>
        </article>
      </div>
    </section>

    <section class="section">
      <SectionHeading
        eyebrow="Risk register"
        title="风险清单：哪些地方最容易翻车"
        lead="风险不是用来吓人的，是用来决定下一步先验证什么的。"
      />

      <div class="risk-table" role="table" aria-label="风险清单">
        <div class="risk-table__row risk-table__row--head" role="row">
          <span role="columnheader">风险</span>
          <span role="columnheader">影响</span>
          <span role="columnheader">缓解方式</span>
        </div>
        <div v-for="item in risks" :key="item.risk" class="risk-table__row" role="row">
          <span role="cell" class="risk-table__risk">{{ item.risk }}</span>
          <span role="cell">
            <span class="risk-level" :class="`risk-level--${item.impact === '高' ? 'high' : 'mid'}`">{{ item.impact }}</span>
          </span>
          <span role="cell">{{ item.mitigation }}</span>
        </div>
      </div>
    </section>

    <section class="section">
      <div class="final-vision">
        <p class="eyebrow">North star</p>
        <h2>{{ finalVision.quote }}</h2>
        <p class="final-vision__note">{{ finalVision.note }}</p>
        <div class="final-vision__steps">
          <span v-for="(step, index) in finalVision.steps" :key="step" :class="{ done: index < 2 }">
            {{ step }}
          </span>
        </div>
        <RouterLink class="button button--primary" to="/">回到概览</RouterLink>
      </div>
    </section>
  </div>
</template>
