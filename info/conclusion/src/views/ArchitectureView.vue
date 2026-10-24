<script setup>
import { RouterLink } from 'vue-router'
import SectionHeading from '../components/SectionHeading.vue'
import LayerDiagram from '../components/LayerDiagram.vue'
import PipelineDemo from '../components/PipelineDemo.vue'
import { runtimeFacts, stack } from '../data/content'
</script>

<template>
  <div class="view">
    <section class="page-hero">
      <p class="eyebrow">System architecture · 端侧链路</p>
      <h1>一条语音，穿过六层系统</h1>
      <p class="page-hero__lead">
        从机器人内部音频流开始，到扬声器发出声音结束。当前真正跑通的是语音输入与语音输出两条支线，
        它们之间还缺一个「对话大脑」，脚下还缺视觉与运动技能。
      </p>
      <div class="page-hero__stats">
        <div><strong>6</strong><span>系统分层</span></div>
        <div><strong>0.55</strong><span>CPU decode RTF</span></div>
        <div><strong>1</strong><span>尚未接入的对话脑</span></div>
        <div><strong>4</strong><span>已确认的硬件约束</span></div>
      </div>
    </section>

    <section class="section">
      <SectionHeading
        eyebrow="Layer map"
        title="点击任意节点，查看它在这套系统里的位置"
        lead="分层不是为了画得好看，而是为了在故障发生时能快速判断：问题在采集、识别、对话、表达，还是在机器人硬件与运行库。"
      />
      <LayerDiagram />
    </section>

    <section class="section">
      <SectionHeading
        eyebrow="Voice round-trip"
        title="把一轮语音交互拆成可验证的节点"
        lead="当前链路在 ASR 结束；TTS 与扬声器是另一条已经独立验证的支线。目标闭环需要把两者接上，而这一步还没有完成。"
      >
        <template #aside>
          <RouterLink class="text-link" to="/nodes">逐节点看 API →</RouterLink>
        </template>
      </SectionHeading>
      <PipelineDemo />
    </section>

    <section class="section">
      <div class="two-col">
        <div class="panel runtime-panel">
          <div class="panel__head panel__head--tight">
            <div>
              <p class="eyebrow">Runtime facts</p>
              <h3>这套系统实际跑在什么上面</h3>
            </div>
            <span class="mono-badge">G1-EDU</span>
          </div>

          <dl class="runtime-grid">
            <div v-for="fact in runtimeFacts" :key="fact.label">
              <dt>{{ fact.label }}</dt>
              <dd>{{ fact.value }}</dd>
            </div>
          </dl>
        </div>

        <div class="panel stack-panel">
          <div class="panel__head panel__head--tight">
            <div>
              <p class="eyebrow">Stack</p>
              <h3>技术栈不是重点，边界才是</h3>
            </div>
          </div>

          <div class="stack-groups">
            <div v-for="group in stack" :key="group.group" class="stack-group">
              <p class="mono-label">{{ group.group }}</p>
              <div class="tag-row">
                <span v-for="item in group.items" :key="item" class="tag">{{ item }}</span>
              </div>
            </div>
          </div>
        </div>
      </div>
    </section>

    <section class="section">
      <SectionHeading
        eyebrow="CPU vs GPU"
        title="两套运行库，别把它们混成一锅"
        lead="这是整条链路里最容易误判的一处：机器上有 CUDA，不等于当前程序会用 GPU；当前 ASR 的 CPU 基线仍然是唯一被实测确认的版本。"
      />

      <div class="compare-table" role="table" aria-label="CPU 与 GPU 运行库对比">
        <div class="compare-table__row compare-table__row--head" role="row">
          <span role="columnheader">检查项</span>
          <span role="columnheader">当前 CPU 基线</span>
          <span role="columnheader">GPU 实验版本</span>
        </div>
        <div class="compare-table__row" role="row">
          <span role="cell">运行库</span>
          <span role="cell">sherpa-onnx v1.13.8 shared-cpu</span>
          <span role="cell">sherpa-onnx v1.13.8 GPU build</span>
        </div>
        <div class="compare-table__row" role="row">
          <span role="cell">ONNX Runtime</span>
          <span role="cell">CPU 包内置</span>
          <span role="cell">1.16.0 · CUDA / TensorRT Provider</span>
        </div>
        <div class="compare-table__row" role="row">
          <span role="cell">实测状态</span>
          <span role="cell"><em class="ok">已跑通 · RTF 0.51–0.60</em></span>
          <span role="cell"><em class="warn">编译中 · 未验证</em></span>
        </div>
        <div class="compare-table__row" role="row">
          <span role="cell">使用建议</span>
          <span role="cell">保留为回退基线，不覆盖</span>
          <span role="cell">单独目录构建，数据说话后再切换</span>
        </div>
      </div>

      <aside class="callout callout--warn">
        <span class="callout__icon" aria-hidden="true">!</span>
        <div>
          <strong>INT8 不是 GPU 加速的保证。</strong>
          <p>
            Qwen3-ASR 使用 encoder.int8.onnx 与 decoder.int8.onnx。即使 CUDA Provider 成功加载，
            部分量化算子仍可能回退到 CPU，跨设备拷贝也会增加开销。必须检查算子分配，并用同一段音频对照，
            才能判断是否真的变快。
          </p>
        </div>
      </aside>
    </section>

    <section class="section">
      <div class="split-cta">
        <div>
          <p class="eyebrow">Retrospective</p>
          <h2>架构之外，真正花时间的是踩坑。</h2>
          <p>音频截断、0 字节 WAV、CPU/GPU 运行库混淆、队列积压与 INT8 算子回退——这些才是工程现场。</p>
        </div>
        <div class="split-cta__actions">
          <RouterLink class="button button--primary" to="/retrospective">进入实战复盘</RouterLink>
          <RouterLink class="button button--ghost" to="/roadmap">看下一步计划</RouterLink>
        </div>
      </div>
    </section>
  </div>
</template>
