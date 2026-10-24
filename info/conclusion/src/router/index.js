import { createRouter, createWebHashHistory } from 'vue-router'
import OverviewView from '../views/OverviewView.vue'
import ArchitectureView from '../views/ArchitectureView.vue'
import NodesView from '../views/NodesView.vue'
import RetrospectiveView from '../views/RetrospectiveView.vue'
import RoadmapView from '../views/RoadmapView.vue'

const routes = [
  {
    path: '/',
    name: 'overview',
    component: OverviewView,
    meta: { title: '概览' },
  },
  {
    path: '/architecture',
    name: 'architecture',
    component: ArchitectureView,
    meta: { title: '系统架构' },
  },
  {
    path: '/nodes',
    name: 'nodes',
    component: NodesView,
    meta: { title: '节点详解' },
  },
  {
    path: '/retrospective',
    name: 'retrospective',
    component: RetrospectiveView,
    meta: { title: '实战复盘' },
  },
  {
    path: '/roadmap',
    name: 'roadmap',
    component: RoadmapView,
    meta: { title: '下一步' },
  },
  {
    path: '/:pathMatch(.*)*',
    redirect: '/',
  },
]

const router = createRouter({
  history: createWebHashHistory(),
  routes,
  scrollBehavior() {
    return { top: 0 }
  },
})

router.afterEach((to) => {
  document.title = `${to.meta.title} · G1 语音链路复盘`
})

export default router
