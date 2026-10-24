<script setup>
defineProps({
  eyebrow: { type: String, default: '' },
  title: { type: String, required: true },
  lead: { type: String, default: '' },
  compact: { type: Boolean, default: false },
})
</script>

<template>
  <header class="section-heading" :class="{ 'section-heading--compact': compact }">
    <p v-if="eyebrow" class="eyebrow">{{ eyebrow }}</p>
    <div class="section-heading__row">
      <h2>{{ title }}</h2>
      <div v-if="$slots.aside" class="section-heading__aside">
        <slot name="aside" />
      </div>
    </div>
    <p v-if="lead" class="section-lead">{{ lead }}</p>
  </header>
</template>
