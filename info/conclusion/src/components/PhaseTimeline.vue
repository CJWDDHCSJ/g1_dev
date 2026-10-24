<script setup>
defineProps({
  phases: { type: Array, required: true },
  detailed: { type: Boolean, default: false },
})
</script>

<template>
  <ol class="timeline" :class="{ 'timeline--detailed': detailed }">
    <li v-for="phase in phases" :key="phase.id" class="timeline__item" :class="`timeline__item--${phase.tone}`">
      <div class="timeline__rail" aria-hidden="true">
        <span class="timeline__node">{{ phase.no }}</span>
      </div>

      <article class="timeline__card">
        <header class="timeline__head">
          <div>
            <p class="timeline__subtitle">{{ phase.subtitle }}</p>
            <h3>{{ phase.title }}</h3>
          </div>
          <span class="status-pill" :class="`status-pill--${phase.tone}`">{{ phase.status }}</span>
        </header>

        <p class="timeline__summary">{{ phase.summary }}</p>

        <template v-if="detailed">
          <ul class="detail-list">
            <li v-for="bullet in phase.bullets" :key="bullet">{{ bullet }}</li>
          </ul>

          <div class="timeline__result">
            <span class="mono-label">阶段结论</span>
            <p>{{ phase.result }}</p>
          </div>
        </template>

        <div class="tag-row">
          <span v-for="tag in phase.tags" :key="tag" class="tag">{{ tag }}</span>
        </div>
      </article>
    </li>
  </ol>
</template>
