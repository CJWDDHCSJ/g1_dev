<script setup>
import { ref } from 'vue'
import { pitfalls } from '../data/content'

const expanded = ref([0])

function toggle(index) {
  expanded.value = expanded.value.includes(index)
    ? expanded.value.filter((item) => item !== index)
    : [...expanded.value, index]
}
</script>

<template>
  <div class="pitfall-grid">
    <article
      v-for="(item, index) in pitfalls"
      :key="item.title"
      class="pitfall-card"
      :class="[`pitfall-card--${item.tone}`, { 'is-open': expanded.includes(index) }]"
    >
      <button
        class="pitfall-card__head"
        type="button"
        :aria-expanded="expanded.includes(index)"
        :aria-controls="`pitfall-body-${index}`"
        @click="toggle(index)"
      >
        <span class="pitfall-card__index">{{ String(index + 1).padStart(2, '0') }}</span>
        <span class="pitfall-card__title">
          <strong>{{ item.title }}</strong>
          <small>{{ item.symptom }}</small>
        </span>
        <span class="pitfall-card__toggle" aria-hidden="true"></span>
      </button>

      <div v-show="expanded.includes(index)" :id="`pitfall-body-${index}`" class="pitfall-card__body">
        <dl>
          <div>
            <dt>原因</dt>
            <dd>{{ item.cause }}</dd>
          </div>
          <div>
            <dt>处理</dt>
            <dd>{{ item.fix }}</dd>
          </div>
          <div class="pitfall-card__lesson">
            <dt>经验</dt>
            <dd>{{ item.lesson }}</dd>
          </div>
        </dl>
      </div>
    </article>
  </div>
</template>
