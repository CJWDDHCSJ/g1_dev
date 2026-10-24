<script setup>
import { ref, watch } from 'vue'
import { RouterLink, useRoute } from 'vue-router'
import { nav, site } from '../data/content'

const route = useRoute()
const menuOpen = ref(false)

watch(
  () => route.path,
  () => {
    menuOpen.value = false
  },
)
</script>

<template>
  <header class="site-header">
    <a class="skip-link" href="#main">跳到主要内容</a>

    <div class="site-header__inner">
      <RouterLink class="brand" to="/" aria-label="返回概览">
        <svg class="brand__mark" viewBox="0 0 40 44" aria-hidden="true">
          <path d="M20 1.5 37 11v22L20 42.5 3 33V11Z" fill="none" stroke="currentColor" stroke-width="1.5" />
          <path d="M20 9.5 29.5 15v14L20 34.5 10.5 29V15Z" fill="none" stroke="currentColor" stroke-width="1" opacity=".55" />
          <circle cx="20" cy="22" r="4" fill="currentColor" />
          <path d="M20 9.5v8M20 26.5v8M10.5 15l6 4M29.5 15l-6 4M10.5 29l6-4M29.5 29l-6-4" stroke="currentColor" stroke-width="1" opacity=".6" />
        </svg>
        <span class="brand__text">
          <strong>{{ site.name }}</strong>
          <small>{{ site.tagline }}</small>
        </span>
      </RouterLink>

      <button
        class="nav-toggle"
        type="button"
        :aria-expanded="menuOpen"
        aria-controls="primary-nav"
        @click="menuOpen = !menuOpen"
      >
        <span></span><span></span><span></span>
        <span class="sr-only">切换导航</span>
      </button>

      <nav id="primary-nav" class="primary-nav" :class="{ 'is-open': menuOpen }" aria-label="主导航">
        <RouterLink v-for="item in nav" :key="item.to" class="nav-link" :to="item.to">
          <span class="nav-link__index">{{ item.index }}</span>
          <span>{{ item.label }}</span>
        </RouterLink>
      </nav>

      <div class="header-status" aria-label="当前状态">
        <span class="status-dot" aria-hidden="true"></span>
        <span>{{ site.status }}</span>
      </div>
    </div>
  </header>
</template>
