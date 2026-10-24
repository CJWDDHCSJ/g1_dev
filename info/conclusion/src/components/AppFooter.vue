<script setup>
import { RouterLink } from 'vue-router'
import { nav, site } from '../data/content'
</script>

<template>
  <footer class="site-footer">
    <div class="site-footer__inner">
      <div class="site-footer__brand">
        <span class="mono-label">{{ site.name }}</span>
        <p>{{ site.tagline }}</p>
      </div>

      <div class="site-footer__meta">
        <p>{{ site.source }}</p>
        <p class="muted">{{ site.sourceNote }}</p>
      </div>

      <nav class="site-footer__nav" aria-label="页脚导航">
        <RouterLink v-for="item in nav" :key="item.to" :to="item.to">{{ item.label }}</RouterLink>
        <span class="footer-updated">更新于 {{ site.updated }}</span>
      </nav>
    </div>
  </footer>
</template>
