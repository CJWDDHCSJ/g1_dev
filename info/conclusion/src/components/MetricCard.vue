<script setup>
defineProps({
  metric: { type: Object, required: true },
  index: { type: Number, default: 0 },
})
</script>

<template>
  <article class="metric-card" :class="`metric-card--${metric.tone}`" :style="{ '--delay': `${index * 70}ms` }">
    <p class="metric-card__label">{{ metric.label }}</p>
    <p class="metric-card__value">{{ metric.value }}</p>
    <p class="metric-card__note">{{ metric.note }}</p>
  </article>
</template>
