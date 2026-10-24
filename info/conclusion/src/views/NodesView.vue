<script setup>
import { RouterLink } from 'vue-router'
import SectionHeading from '../components/SectionHeading.vue'
import NodeDossier from '../components/NodeDossier.vue'
import {
  apiKindLegend,
  apiMap,
  nodeGroups,
  nodeMeta,
  nodes,
  routeComparison,
  sources,
  statusLegend,
} from '../data/nodes'

function scrollToNode(id) {
  document.getElementById(`node-${id}`)?.scrollIntoView({ behavior: 'smooth', block: 'start' })
}
</script>

<template>
  <div class="view">
    <section class="page-hero">
      <p class="eyebrow">{{ nodeMeta.eyebrow }}</p>
      <h1>{{ nodeMeta.title }}</h1>
      <p class="page-hero__lead">{{ nodeMeta.lead }}</p>

      <div class="page-hero__stats">
        <div v-for="stat in nodeMeta.stats" :key="stat.label">
          <strong>{{ stat.value }}</strong>
          <span>{{ stat.label }}</span>
        </div>
      </div>
    </section>

    <nav class="node-jump" aria-label="节点索引">
      <span class="mono-label node-jump__label">JUMP</span>
      <button
        v-for="node in nodes"
        :key="node.id"
        type="button"
        class="node-jump__item"
        @click="scrollToNode(node.id)"
      >
        <span class="node-jump__no">{{ node.no }}</span>
        <span>{{ node.jumpLabel }}</span>
        <i class="node-jump__dot" :class="`node-jump__dot--${node.tone}`" aria-hidden="true"></i>
      </button>
    </nav>

    <section class="section">
      <SectionHeading
        eyebrow="How to read"
        title="先懂一句话，再看契约与 API"
        lead="每个节点都先用一句大白话说明它在做什么，再进入输入/输出、API 与现场笔记。术语不是不能用，而是要在你明白它在干什么之后出现；这样遇到陌生缩写时，至少知道它在这条链路里负责哪一段。"
      />

      <div class="source-strip">
        <article v-for="item in apiKindLegend" :key="item.kind" class="source-chip">
          <span class="api-kind" :class="`api-kind--${item.tone}`">{{ item.kind }}</span>
          <p>{{ item.detail }}</p>
        </article>
      </div>
    </section>

    <section class="section">
      <SectionHeading
        eyebrow="API map"
        title="八个节点，一张调用地图"
        lead="从原始 PCM 到扬声器波形，每一行都是一个可替换边界。点击节点序号跳到详细卡片；状态列说明它在 G1 上验证到了哪一步。"
      />

      <p class="table-hint">表格可横向滑动 →</p>
      <div class="api-map" role="table" aria-label="节点与 API 地图">
        <div class="api-map__row api-map__row--head" role="row">
          <span role="columnheader">#</span>
          <span role="columnheader">节点</span>
          <span role="columnheader">核心 API / 运行时</span>
          <span role="columnheader">数据契约</span>
          <span role="columnheader">状态</span>
        </div>
        <div v-for="row in apiMap" :key="row.id" class="api-map__row" role="row">
          <span role="cell" class="api-map__no">{{ row.no }}</span>
          <span role="cell" class="api-map__node">
            <button type="button" class="api-map__link" @click="scrollToNode(row.id)">{{ row.node }}</button>
          </span>
          <span role="cell" class="api-map__api">{{ row.api }}</span>
          <span role="cell">{{ row.contract }}</span>
          <span role="cell">
            <span class="status-pill" :class="`status-pill--${row.tone}`">{{ row.status }}</span>
          </span>
        </div>
      </div>
    </section>

    <section class="section">
      <SectionHeading
        :eyebrow="routeComparison.eyebrow"
        :title="routeComparison.title"
        :lead="routeComparison.lead"
      />

      <p class="table-hint">表格可横向滑动 →</p>
      <div class="route-compare" role="table" aria-label="官方语音服务与项目自建链路对照">
        <div class="route-compare__row route-compare__row--head" role="row">
          <span role="columnheader">环节</span>
          <span role="columnheader">官方原生路径</span>
          <span role="columnheader">项目选择</span>
          <span role="columnheader">代价 / 收益</span>
        </div>
        <div v-for="row in routeComparison.rows" :key="row.stage" class="route-compare__row" role="row">
          <span role="cell" class="route-compare__stage">{{ row.stage }}</span>
          <span role="cell" class="route-compare__official">{{ row.official }}</span>
          <span role="cell" class="route-compare__project">{{ row.project }}</span>
          <span role="cell">{{ row.tradeoff }}</span>
        </div>
      </div>

      <aside class="callout callout--mint">
        <span class="callout__icon" aria-hidden="true">✓</span>
        <div>
          <strong>复用播放面，替换识别与合成。</strong>
          <p>
            项目仍然使用 Unitree AudioClient 驱动 G1 扬声器，但没有调用内置 ASR 与 TtsMaker。
            这是一次有边界的替换：设备控制交给官方 SDK，模型与音频数据留在自己手里。
          </p>
        </div>
      </aside>
    </section>

    <section
      v-for="group in nodeGroups"
      :id="'group-' + group.id"
      :key="group.id"
      class="section node-group"
    >
      <SectionHeading :eyebrow="group.eyebrow" :title="group.title" :lead="group.lead" />

      <div class="node-stack">
        <NodeDossier v-for="node in group.nodes" :key="node.id" :node="node" />
      </div>
    </section>

    <section class="section">
      <SectionHeading
        eyebrow="Reference"
        title="证据分级与资料来源"
        lead="页面把「官方文档确认过的方法签名」「项目源码里的实现」「下一步的设计建议」分开写。状态标签只描述证据强度，不把“能跑”升级成“已验收”。"
      />

      <div class="legend-grid">
        <div class="legend-panel">
          <p class="mono-label">API SOURCE / 来源</p>
          <ul class="legend-list">
            <li v-for="item in apiKindLegend" :key="item.kind">
              <span class="api-kind" :class="`api-kind--${item.tone}`">{{ item.kind }}</span>
              <p>{{ item.detail }}</p>
            </li>
          </ul>
        </div>

        <div class="legend-panel">
          <p class="mono-label">EVIDENCE / 证据状态</p>
          <ul class="legend-list">
            <li v-for="item in statusLegend" :key="item.label">
              <span class="status-pill" :class="`status-pill--${item.tone}`">{{ item.label }}</span>
              <p>{{ item.detail }}</p>
            </li>
          </ul>
        </div>
      </div>

      <ul class="source-list">
        <li v-for="source in sources" :key="source.url">
          <div class="source-list__head">
            <a :href="source.url" target="_blank" rel="noopener noreferrer">{{ source.label }}</a>
            <span class="mono-label">{{ source.kind }}</span>
          </div>
          <p>{{ source.note }}</p>
          <span class="source-list__url">{{ source.url }}</span>
        </li>
      </ul>
    </section>

    <section class="section">
      <div class="split-cta">
        <div>
          <p class="eyebrow">Next move</p>
          <h2>能说清楚每个节点，才配谈“闭环”。</h2>
          <p>
            节点详解补上了概念与 API 的缺口，但对话脑、GPU 验证与视觉验收仍然没有完成。
            下一步先把对话脑的接口落地，再用同一段音频决定 GPU 是否值得切换。
          </p>
        </div>
        <div class="split-cta__actions">
          <RouterLink class="button button--primary" to="/roadmap">查看下一步</RouterLink>
          <RouterLink class="button button--ghost" to="/architecture">回到系统架构</RouterLink>
        </div>
      </div>
    </section>
  </div>
</template>
