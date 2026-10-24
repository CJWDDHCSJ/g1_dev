<script setup>
import { asrDeepDive } from '../data/asr'
</script>

<template>
  <section class="asr-deep" aria-labelledby="asr-deep-title">
    <header class="asr-deep__head">
      <div class="asr-deep__heading">
        <p class="eyebrow">{{ asrDeepDive.eyebrow }}</p>
        <h4 id="asr-deep-title">{{ asrDeepDive.title }}</h4>
      </div>
      <p class="asr-deep__lead">{{ asrDeepDive.lead }}</p>
    </header>

    <p class="asr-deep__summary">
      <span class="mono-label">一句话版</span>
      <span>{{ asrDeepDive.summary }}</span>
    </p>

    <dl class="asr-facts">
      <div v-for="fact in asrDeepDive.facts" :key="fact.label" class="asr-fact">
        <dt>{{ fact.label }}</dt>
        <dd>{{ fact.value }}</dd>
      </div>
    </dl>

    <div class="asr-glossary">
      <p class="mono-label">TERMS FIRST / 三个词，先混个脸熟</p>
      <dl>
        <div v-for="item in asrDeepDive.glossary" :key="item.term" class="asr-glossary__item">
          <dt>{{ item.term }}<span>{{ item.plain }}</span></dt>
          <dd>{{ item.body }}</dd>
        </div>
      </dl>
    </div>

    <section class="asr-block" aria-labelledby="asr-pipeline-title">
      <div class="asr-block__head">
        <h5 id="asr-pipeline-title">推理链 / 从波形到文本</h5>
        <span class="mono-label">SIGNAL → TOKENS → TEXT</span>
      </div>

      <ol class="asr-pipeline">
        <li v-for="step in asrDeepDive.pipeline" :key="step.no" class="asr-step">
          <div class="asr-step__no" aria-hidden="true">{{ step.no }}</div>
          <div class="asr-step__body">
            <div class="asr-step__top">
              <h6>{{ step.title }}</h6>
              <span class="asr-step__sub">{{ step.subtitle }}</span>
            </div>
            <p class="asr-step__plain">{{ step.plain }}</p>
            <p class="asr-step__detail">{{ step.body }}</p>
            <code class="asr-step__tech">{{ step.tech }}</code>
          </div>
        </li>
      </ol>
    </section>

    <section class="asr-block" aria-labelledby="asr-concepts-title">
      <div class="asr-block__head">
        <h5 id="asr-concepts-title">关键概念 / 八个容易含混的点</h5>
        <span class="mono-label">MENTAL MODEL</span>
      </div>

      <div class="asr-concepts">
        <article v-for="concept in asrDeepDive.concepts" :key="concept.no" class="asr-concept">
          <div class="asr-concept__head">
            <span class="asr-concept__no" aria-hidden="true">{{ concept.no }}</span>
            <h6>{{ concept.title }}</h6>
          </div>
          <p class="asr-concept__plain">{{ concept.plain }}</p>
          <p class="asr-concept__tech">{{ concept.body }}</p>
        </article>
      </div>
    </section>

    <section class="asr-block" aria-labelledby="asr-opt-title">
      <div class="asr-block__head">
        <h5 id="asr-opt-title">优化清单 / 按优先级执行</h5>
        <span class="mono-label">P0 → P3 · MEASURE FIRST</span>
      </div>

      <div class="asr-opts">
        <article
          v-for="opt in asrDeepDive.optimizations"
          :key="opt.name"
          class="asr-opt"
          :class="`asr-opt--${opt.priority.toLowerCase()}`"
        >
          <header class="asr-opt__head">
            <span class="asr-opt__pri">{{ opt.priority }}</span>
            <h6>{{ opt.name }}</h6>
            <span class="asr-opt__state">{{ opt.state }}</span>
          </header>
          <p class="asr-opt__plain">{{ opt.plain }}</p>
          <dl class="asr-opt__detail">
            <div>
              <dt>收益</dt>
              <dd>{{ opt.gain }}</dd>
            </div>
            <div>
              <dt>代价</dt>
              <dd>{{ opt.cost }}</dd>
            </div>
            <div>
              <dt>做法</dt>
              <dd>{{ opt.how }}</dd>
            </div>
          </dl>
        </article>
      </div>
    </section>

    <section class="asr-block" aria-labelledby="asr-bench-title">
      <div class="asr-block__head">
        <h5 id="asr-bench-title">时延数据 / RTF 对照</h5>
        <span class="mono-label">LOWER IS BETTER</span>
      </div>

      <p class="table-hint">表格可横向滑动 →</p>
      <div class="asr-bench-wrap">
        <table class="asr-bench">
          <caption class="sr-only">不同环境与配置下的 Qwen3-ASR 实时率</caption>
          <thead>
            <tr>
              <th scope="col">环境</th>
              <th scope="col">音频样本</th>
              <th scope="col">配置</th>
              <th scope="col">RTF</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="row in asrDeepDive.benchmarks" :key="`${row.env}-${row.sample}`">
              <td>{{ row.env }}</td>
              <td>{{ row.sample }}</td>
              <td>{{ row.config }}</td>
              <td class="asr-bench__rtf">{{ row.rtf }}</td>
            </tr>
          </tbody>
        </table>
      </div>

      <aside class="asr-bench-note">
        <span class="mono-label">READING NOTE</span>
        <p>{{ asrDeepDive.benchmarkNote }}</p>
      </aside>
    </section>

    <section class="asr-block" aria-labelledby="asr-pitfalls-title">
      <div class="asr-block__head">
        <h5 id="asr-pitfalls-title">常见误判 / 排查顺序</h5>
        <span class="mono-label">DEBUG ORDER</span>
      </div>

      <div class="asr-pitfalls">
        <article v-for="pitfall in asrDeepDive.pitfalls" :key="pitfall.title" class="asr-pitfall">
          <h6>{{ pitfall.title }}</h6>
          <p>{{ pitfall.body }}</p>
        </article>
      </div>
    </section>

    <section class="asr-block" aria-labelledby="asr-code-title">
      <div class="asr-block__head">
        <h5 id="asr-code-title">{{ asrDeepDive.code.title }}</h5>
        <span class="mono-label">C++ · SHERPA-ONNX 1.13.8</span>
      </div>
      <pre class="asr-code"><code>{{ asrDeepDive.code.body }}</code></pre>
    </section>

    <section class="asr-block" aria-labelledby="asr-refs-title">
      <div class="asr-block__head">
        <h5 id="asr-refs-title">官方文档 / 交叉核对</h5>
        <span class="mono-label">PRIMARY SOURCES</span>
      </div>
      <ul class="asr-refs">
        <li v-for="ref in asrDeepDive.references" :key="ref.url">
          <a :href="ref.url" target="_blank" rel="noopener noreferrer">
            <span>{{ ref.label }}</span>
            <span class="asr-refs__arrow" aria-hidden="true">↗</span>
          </a>
        </li>
      </ul>
    </section>
  </section>
</template>
