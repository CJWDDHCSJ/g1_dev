<script setup>
import { computed, ref } from 'vue'
import { layers } from '../data/content'

const selectedId = ref('asr')

const flatNodes = computed(() =>
  layers.flatMap((layer) =>
    layer.nodes.map((node) => ({
      ...node,
      layerId: layer.id,
      layerLabel: layer.label,
      layerCaption: layer.caption,
      layerDescription: layer.description,
    })),
  ),
)

const selected = computed(() => flatNodes.value.find((node) => node.id === selectedId.value) ?? flatNodes.value[0])

function select(id) {
  selectedId.value = id
}
</script>

<template>
  <div class="layer-diagram">
    <div class="layer-diagram__layers">
      <div v-for="layer in layers" :key="layer.id" class="layer-row">
        <div class="layer-row__label">
          <span class="mono-label">{{ layer.caption }}</span>
          <strong>{{ layer.label }}</strong>
        </div>

        <div class="layer-row__nodes">
          <button
            v-for="node in layer.nodes"
            :key="node.id"
            type="button"
            class="layer-node"
            :class="[`layer-node--${node.tone}`, { 'is-selected': selectedId === node.id }]"
            :aria-pressed="selectedId === node.id"
            @click="select(node.id)"
          >
            <span class="layer-node__status">
              <span class="status-dot" :class="`status-dot--${node.tone}`"></span>
              {{ node.status }}
            </span>
            <strong>{{ node.label }}</strong>
          </button>
        </div>
      </div>
    </div>

    <aside class="layer-detail" aria-live="polite">
      <p class="mono-label">{{ selected.layerCaption }} · {{ selected.layerLabel }}</p>
      <h4>{{ selected.label }}</h4>
      <p class="layer-detail__desc">{{ selected.detail }}</p>
      <ul class="layer-detail__meta">
        <li v-for="item in selected.meta" :key="item">{{ item }}</li>
      </ul>
      <p class="layer-detail__layer">{{ selected.layerDescription }}</p>
    </aside>
  </div>
</template>
