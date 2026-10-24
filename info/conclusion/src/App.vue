<script setup>
import AppHeader from './components/AppHeader.vue'
import AppFooter from './components/AppFooter.vue'
</script>

<template>
  <div class="app-shell">
    <div class="bg-grid" aria-hidden="true"></div>
    <div class="bg-glow bg-glow-a" aria-hidden="true"></div>
    <div class="bg-glow bg-glow-b" aria-hidden="true"></div>

    <AppHeader />

    <main id="main" class="app-main">
      <RouterView v-slot="{ Component, route }">
        <Transition name="page" mode="out-in">
          <component :is="Component" :key="route.path" />
        </Transition>
      </RouterView>
    </main>

    <AppFooter />
  </div>
</template>
