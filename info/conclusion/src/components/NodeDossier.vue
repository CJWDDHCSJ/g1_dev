<script setup>
import AsrDeepDive from './AsrDeepDive.vue'

defineProps({
  node: { type: Object, required: true },
})
</script>

<template>
  <article
    :id="`node-${node.id}`"
    class="node-dossier"
    :class="`node-dossier--${node.tone}`"
    :aria-labelledby="`node-${node.id}-title`"
  >
    <header class="node-dossier__head">
      <div class="node-dossier__no" aria-hidden="true">{{ node.no }}</div>

      <div class="node-dossier__title">
        <div class="node-dossier__meta">
          <span class="mono-label">{{ node.stage }}</span>
          <span class="status-pill" :class="`status-pill--${node.tone}`">{{ node.status }}</span>
        </div>
        <h3 :id="`node-${node.id}-title`">{{ node.title }}</h3>
        <p class="node-dossier__short">{{ node.short }}</p>
      </div>

      <div class="node-dossier__tags">
        <span v-for="tag in node.tags" :key="tag" class="tag">{{ tag }}</span>
      </div>
    </header>

    <div class="node-dossier__explain">
      <p class="mono-label">PLAIN FIRST / 一句话理解</p>
      <p class="node-dossier__plain">{{ node.plain }}</p>
      <p class="node-dossier__concept">{{ node.concept }}</p>
    </div>

    <div class="node-contract">
      <div class="node-contract__col">
        <p class="mono-label">INPUT / 输入契约</p>
        <dl>
          <div v-for="item in node.contract.input" :key="item.label">
            <dt>{{ item.label }}</dt>
            <dd>{{ item.value }}</dd>
          </div>
        </dl>
      </div>

      <div class="node-contract__arrow" aria-hidden="true">→</div>

      <div class="node-contract__col">
        <p class="mono-label">OUTPUT / 输出契约</p>
        <dl>
          <div v-for="item in node.contract.output" :key="item.label">
            <dt>{{ item.label }}</dt>
            <dd>{{ item.value }}</dd>
          </div>
        </dl>
      </div>
    </div>

    <div class="node-api">
      <div class="node-api__head">
        <p class="mono-label">API / 调用面</p>
        <span class="mono-label">{{ node.apis.length }} entries</span>
      </div>

      <ul class="api-list">
        <li v-for="api in node.apis" :key="api.name" class="api-item">
          <div class="api-item__head">
            <code>{{ api.name }}</code>
            <span class="api-kind" :class="`api-kind--${api.kindTone}`">{{ api.kind }}</span>
          </div>
          <pre v-if="api.signature" class="api-signature"><code>{{ api.signature }}</code></pre>
          <p>{{ api.note }}</p>
        </li>
      </ul>
    </div>

    <details class="node-code">
      <summary>
        <span>{{ node.code.title }}</span>
        <span class="node-code__hint">展开 / 收起</span>
      </summary>
      <pre><code>{{ node.code.body }}</code></pre>
    </details>

    <div class="node-notes">
      <p class="mono-label">FIELD NOTES / 现场笔记</p>
      <ul>
        <li v-for="note in node.notes" :key="note.title">
          <strong>{{ note.title }}</strong>
          <p>{{ note.detail }}</p>
        </li>
      </ul>
    </div>

    <footer class="node-dossier__foot">
      <span class="mono-label">EVIDENCE</span>
      <p>{{ node.evidence }}</p>
    </footer>

    <AsrDeepDive v-if="node.id === 'asr'" />
  </article>
</template>
