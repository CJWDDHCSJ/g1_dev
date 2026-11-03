# 附录 B：知识库完整数据与结构说明

> 本附录逐字复制 `config/lab_knowledge.json`，未使用 schema 示例、未省略任何条目。

> 远端源码路径：`/home/unitree/unitree_ros2/example/g1_voice_router/config/lab_knowledge.json`

> 远端安装路径：`/home/unitree/unitree_ros2/example/install/g1_voice_router/share/g1_voice_router/config/lab_knowledge.json`（符号链接到源码路径）

> SHA256：`cbe711e3b5c19115dc9d85fe37c701829ee3ad37203a3a4337efa5263d5acea7`；行数：798；字节数：27206


## B.1 运行时会加载哪一个文件

1. 节点启动时先调用 `ament_index_cpp::get_package_share_directory("g1_voice_router")`，拼接 `/config/lab_knowledge.json` 作为默认路径（`src/voice_router.cpp:48-60`）。
2. 若 ROS 2 参数 `knowledge_file` 显式提供，则使用参数值覆盖默认路径（`:62-64`）。
3. 当前运行实例的参数值（`ros2 param get` 实测）为：
   `/home/unitree/unitree_ros2/example/install/g1_voice_router/share/g1_voice_router/config/lab_knowledge.json`
4. 该安装路径当前是符号链接，指向源码树：
   `/home/unitree/unitree_ros2/example/g1_voice_router/config/lab_knowledge.json`
5. 启动日志实测：
   `KnowledgeBase loaded: .../config/lab_knowledge.json (FAQ=20, directions=3, projects=1, equipment=1)`
6. 加载成功后数据常驻内存，`query()` 不再次读文件；修改 JSON 后必须重启节点才会生效。


## B.2 顶层结构与统计

| 顶层字段 | 类型 | 当前值/数量 | 是否被 `query()` 使用 | 说明 |
|---|---|---|---|---|
| `schema_version` | 整数 | `1` | 加载时校验 | 仅支持 1；不等于 1 时整库拒绝加载 |
| `knowledge_version` | 字符串 | `1.0.0` | 否 | 仅元数据；未参与校验或日志 |
| `updated_at` | 字符串 | `2026-10-31` | 否 | 仅元数据；运行时不会检查新鲜度 |
| `school_name` | 字符串 | 重庆理工大学 | 否 | 仅加载时校验类型，可空 |
| `college_name` | 字符串 | 重庆理工大学两江人工智能学院 | 否 | 仅加载时校验类型，可空 |
| `lab_name` | 字符串 | 两个机构名 | 否 | 仅加载时校验类型，可空 |
| `location` | 字符串 | 81 字节 | **否** | 顶层地址；实际回答使用 `faq_location` / `categories.location` |
| `introduction` | 字符串 | 412 字节 | **否** | 顶层简介；实际回答使用 `faq_intro` / `categories.introduction` |
| `labs` | 数组 | 3 | 否 | 机构元数据；仅校验 ID/name/source_ids |
| `research_directions` | 数组 | 3 | 是 | 关键词查询的第 3 层 |
| `projects` | 数组 | 1 | 是 | 关键词查询的第 3 层 |
| `equipment` | 数组 | 1 | 是 | 关键词查询的第 3 层 |
| `categories` | 数组 | 5 | 是 | 关键词查询的第 4 层 |
| `faq` | 数组 | 20 | 是 | 精确/包含/关键词查询的第 1-2 层 |
| `fallback_answer` | 字符串 | 162 字节 | 是 | 加载成功但查询无命中时播报 |
| `disclaimer` | 字符串 | 69 字节 | **否** | 仅校验类型和 URL；当前没有任何播报路径引用 |
| `sources` | 数组 | 15（A=13, B=1, C=1） | 仅校验 | 答案不播报来源；`source_ids` 只用于完整性校验 |

> 重要：顶层 `introduction`、`location`、`disclaimer` 虽然被加载和校验，但 `KnowledgeBase::query()` 从未读取它们。修改这三个字段不会改变机器人回答；必须同步修改 `faq` 或 `categories` 中对应答案。


### B.2.1 FAQ 条目统计

| 序号 | FAQ ID | category | answer 字节数 | questions | keywords | source_ids |
|---|---|---|---|---|---|---|
| 1 | `faq_intro` | `introduction` | 412 | 6 | 5 | 5 |
| 2 | `faq_bci_what` | `labs` | 263 | 3 | 3 | 2 |
| 3 | `faq_bci_directions` | `research_directions` | 168 | 3 | 8 | 1 |
| 4 | `faq_bci_leaders` | `people` | 168 | 3 | 7 | 2 |
| 5 | `faq_bci_date` | `labs` | 104 | 2 | 3 | 2 |
| 6 | `faq_embodied_name` | `labs` | 216 | 3 | 4 | 2 |
| 7 | `faq_embodied_research` | `research_directions` | 306 | 3 | 9 | 2 |
| 8 | `faq_embodied_approval` | `labs` | 176 | 2 | 3 | 2 |
| 9 | `faq_embodied_seres` | `labs` | 168 | 2 | 3 | 2 |
| 10 | `faq_platform` | `labs` | 216 | 2 | 5 | 1 |
| 11 | `faq_difference` | `labs` | 261 | 2 | 4 | 3 |
| 12 | `faq_equipment` | `equipment` | 180 | 4 | 7 | 1 |
| 13 | `faq_location` | `location` | 162 | 4 | 6 | 2 |
| 14 | `faq_teams` | `college` | 192 | 2 | 8 | 1 |
| 15 | `faq_awards` | `college` | 181 | 2 | 5 | 1 |
| 16 | `faq_xiao` | `people` | 303 | 2 | 4 | 2 |
| 17 | `faq_liu` | `people` | 315 | 2 | 4 | 2 |
| 18 | `faq_project` | `projects` | 319 | 3 | 5 | 1 |
| 19 | `faq_college_focus` | `college` | 198 | 2 | 4 | 4 |
| 20 | `faq_college_established` | `college` | 99 | 2 | 4 | 1 |

### B.2.2 其他可检索条目

| 数组 | ID | answer 字节数 | keywords | source_ids |
|---|---|---|---|---|
| `research_directions` | `rd_bci` | 168 | 7 | 1 |
| `research_directions` | `rd_embodied` | 306 | 8 | 2 |
| `research_directions` | `rd_platform` | 219 | 7 | 1 |
| `projects` | `proj_embodied_robot` | 319 | 5 | 1 |
| `equipment` | `eq_platform_carriers` | 180 | 6 | 1 |
| `categories` | `introduction` | 412 | 5 | 5 |
| `categories` | `research_directions` | 348 | 5 | 2 |
| `categories` | `projects` | 319 | 6 | 1 |
| `categories` | `equipment` | 180 | 9 | 1 |
| `categories` | `location` | 162 | 7 | 2 |

### B.2.3 `labs` 条目

| ID | 名称 | 类型 | 关系字段 | source_ids |
|---|---|---|---|---|
| `bci_center` | 脑机交互智能研究中心 | 研究中心 | parent=重庆理工大学两江人工智能学院; established=2025-12-30 | S04, S05 |
| `embodied_lab` | 人形机器人具身智能感知与自主学习重庆市重点实验室 | 重庆市重点实验室 | parent=重庆理工大学; co_builder=赛力斯汽车有限公司; approved=2025-07 | S01, S02 |
| `embodied_platform` | 具身智能开放平台 | 学院开放平台 | parent=重庆理工大学两江人工智能学院 | S09 |

## B.3 条目结构定义

### B.3.1 FAQ 条目

```text
{
  "id":          string,   必填，全库唯一
  "questions":   string[], 必填，非空；每个元素归一化后不能为空
  "keywords":    string[], 必填，非空；每个元素归一化后不能为空
  "answer":      string,   必填，非空；禁止包含 http://、https://、www.
  "source_ids":  string[], 必填，非空；每个 ID 必须存在于顶层 sources
  "category":    string,   可选；缺省时查询结果 category 为 "faq"
}
```

### B.3.2 research_directions / projects / equipment 条目

```text
{
  "id":          string,   必填，全库唯一
  "name":        string,   可选；当前查询不读取
  "summary":     string,   可选；当前查询不读取
  "lab":         string,   可选；当前查询不读取
  "status":      string,   可选；当前查询不读取
  "keywords":    string[], 必填，非空
  "answer":      string,   必填，非空；禁止 URL
  "source_ids":  string[], 必填，非空
}
```

### B.3.3 categories 条目

```text
{
  "id":          string,   必填，全库唯一
  "keywords":    string[], 必填，非空
  "answer":      string,   必填，非空；禁止 URL
  "source_ids":  string[], 必填，非空
}
```

### B.3.4 sources 条目

```text
{
  "id":           string,  必填，全库唯一
  "title":        string,  必填，非空
  "publisher":    string,  必填，非空
  "url":          string,  必填，非空（不校验 URL 格式）
  "date":         string,  可选；可为空字符串
  "level":        string,  必填，非空（不校验取值）
  "access_date":  string,  必填，非空
  "verification": string,  可选；不校验取值
}
```

### B.3.5 labs 条目

```text
{
  "id":          string,   必填，非空
  "name":        string,   必填，非空
  "type":        string,   可选
  "parent":      string,   可选
  "co_builder":  string,   可选
  "established": string,   可选
  "approved":    string,   可选
  "source_ids":  string[], 必填，非空；每个 ID 必须存在
}
```

> `labs[].id` 未纳入全库条目 ID 唯一性检查；当前没有任何查询按 lab ID 检索，因此重复 lab ID 不会影响回答，但属于 schema 缺口。

## B.4 加载校验（失败即整库拒绝）

`KnowledgeBase::load()`（`src/knowledge_base.cpp:270-566`）按以下顺序校验：

1. 路径非空；文件可打开、可读。
2. 容忍 UTF-8 BOM；`nlohmann::json::parse` 失败则返回解析错误。
3. 根节点必须是对象；`schema_version` 必须是整数且等于 1。
4. 六个顶层字符串字段必须是字符串：`school_name`、`college_name`、`lab_name`、`location`、`introduction`、`fallback_answer`；其中 `fallback_answer` 非空。
5. `fallback_answer`、非空 `introduction`、非空 `location` 禁止 URL；`disclaimer` 若存在，必须是字符串且禁止 URL。
6. 六个必需数组必须存在且为数组：`research_directions`、`projects`、`equipment`、`categories`、`faq`、`sources`；`sources` 非空。
7. 每个 source：`id/title/publisher/url/level/access_date` 为非空字符串；`date`、`verification` 若存在必须是字符串；source ID 唯一。
8. `labs` 若存在必须是数组；每个 lab 的 `id/name/source_ids` 必填且 source 引用有效。
9. `research_directions`、`projects`、`equipment`、`categories`、`faq` 逐条校验：`id`、`answer`、`keywords`、`source_ids` 必填；FAQ 额外要求 `questions`；答案禁止 URL；source 引用必须存在。
10. 五类条目 ID 全库唯一（`labs` 除外）。
11. 任何一步失败：清空 `data_`、`loaded_ = false`，返回 false；节点记录 `ERROR` 但继续运行。

## B.5 空字段处理

| 情况 | 加载行为 | 查询行为 |
|---|---|---|
| 顶层 `school_name`/`college_name`/`lab_name` 为空字符串 | 允许 | 不影响查询（查询不读取） |
| 顶层 `location`/`introduction` 为空字符串 | 允许 | 不影响查询（查询不读取）；对应 FAQ 仍可回答 |
| `fallback_answer` 为空 | 拒绝加载 | 节点启动后所有 LAB_INTRO 使用硬编码 57 字节兜底 |
| 条目 `answer` 为空 | 拒绝加载 | 整库不可用 |
| 条目 `keywords` 为空数组 | 拒绝加载 | 整库不可用 |
| 条目 `questions` 为空数组（FAQ） | 拒绝加载 | 整库不可用 |
| 条目 `source_ids` 为空数组 | 拒绝加载 | 整库不可用 |
| `sources` 为空数组 | 拒绝加载 | 整库不可用 |
| `date` 为空字符串 | 允许 | 不影响回答 |
| `disclaimer` 缺失 | 允许 | 当前不播报 |

## B.6 查询顺序与兜底

`KnowledgeBase::query()`（`:568-708`）的固定顺序：

1. **FAQ 问题精确匹配**：归一化后与任一 `questions[]` 完全相等。
2. **FAQ 问题包含匹配**：某个 `questions[]` 是输入的连续子串，且该问题归一化长度 ≥ 5 个 Unicode 码点；取最长匹配。
3. **FAQ 关键词匹配**：`match_score()` 对每条 FAQ 的 `keywords[]` 计分；至少命中 2 个关键词，或单个关键词长度 ≥ 4 个码点才计分；分数 = 命中关键词长度之和 + 命中个数。
4. **研究方向/项目/设备关键词匹配**：同样的 `match_score()`，三类条目放在一起取最高分。
5. **类别关键词匹配**：同样的 `match_score()`，五类取最高分。
6. **兜底**：返回 `fallback_answer`；若加载失败或该字段为空，返回硬编码 `这个信息目前没有公开资料，我不能确认。`（57 字节）。

平局规则：所有阶段都使用 `score > best_score`，**分数相同保留先遍历到的条目**；FAQ 按 JSON 数组顺序，条目按 `research_directions` → `projects` → `equipment` 顺序。该规则会造成“同分时答错条目”，见主报告第 10 节。

## B.7 当前 JSON 全文（逐字）

> 以下内容与远端文件逐字节一致；仅代码围栏由本报告添加。文件共 798 行。


```json
{
  "schema_version": 1,
  "knowledge_version": "1.0.0",
  "updated_at": "2026-10-31",
  "school_name": "重庆理工大学",
  "college_name": "重庆理工大学两江人工智能学院",
  "lab_name": "脑机交互智能研究中心；人形机器人具身智能感知与自主学习重庆市重点实验室",
  "location": "重庆市渝北区龙兴镇普福大道459号（重庆理工大学两江校区）",
  "introduction": "重庆理工大学两江人工智能学院设有脑机交互智能研究中心和人形机器人具身智能感知与自主学习重庆市重点实验室，并建设具身智能开放平台。脑机交互智能研究中心于2025年12月30日成立，定位是高起点、大交叉、国际化、产业化。市级重点实验室于2025年7月获批，由重庆理工大学与赛力斯汽车有限公司共建。",
  "labs": [
    {
      "id": "bci_center",
      "name": "脑机交互智能研究中心",
      "type": "研究中心",
      "parent": "重庆理工大学两江人工智能学院",
      "established": "2025-12-30",
      "source_ids": [
        "S04",
        "S05"
      ]
    },
    {
      "id": "embodied_lab",
      "name": "人形机器人具身智能感知与自主学习重庆市重点实验室",
      "type": "重庆市重点实验室",
      "parent": "重庆理工大学",
      "co_builder": "赛力斯汽车有限公司",
      "approved": "2025-07",
      "source_ids": [
        "S01",
        "S02"
      ]
    },
    {
      "id": "embodied_platform",
      "name": "具身智能开放平台",
      "type": "学院开放平台",
      "parent": "重庆理工大学两江人工智能学院",
      "source_ids": [
        "S09"
      ]
    }
  ],
  "research_directions": [
    {
      "id": "rd_bci",
      "lab": "脑机交互智能研究中心",
      "name": "脑机交互智能研究中心的五个研究方向",
      "summary": "神经信号感知、脑网络组应用、脑病智能诊疗、脑机自然交互、类脑智能系统",
      "keywords": [
        "脑机交互智能研究中心",
        "脑机中心",
        "神经信号感知",
        "脑网络组应用",
        "脑病智能诊疗",
        "脑机自然交互",
        "类脑智能系统"
      ],
      "answer": "脑机交互智能研究中心公开的五个研究方向是：神经信号感知、脑网络组应用、脑病智能诊疗、脑机自然交互、类脑智能系统。",
      "source_ids": [
        "S04"
      ]
    },
    {
      "id": "rd_embodied",
      "lab": "人形机器人具身智能感知与自主学习重庆市重点实验室",
      "name": "市级重点实验室的三项研究内容",
      "summary": "抗干扰多模态融合学习、域泛化和抗知识遗忘持续学习、技能可迁移自主学习",
      "keywords": [
        "重点实验室",
        "具身智能感知",
        "多模态融合学习",
        "域泛化",
        "抗知识遗忘",
        "持续学习",
        "技能可迁移",
        "自主学习"
      ],
      "answer": "这个市级重点实验室主要研究三块：复杂场景下具身智能感知的抗干扰多模态融合学习；开放场景下人机环境交互的域泛化和抗知识遗忘持续学习；动态场景下技能可迁移的自主学习。应用方向主要是汽车智能制造和电子柔性装配。",
      "source_ids": [
        "S02",
        "S03"
      ]
    },
    {
      "id": "rd_platform",
      "lab": "具身智能开放平台",
      "name": "具身智能开放平台的三个方向",
      "summary": "复杂场景高鲁棒感知、高精度定位与建图、精细操作与协同",
      "keywords": [
        "具身智能开放平台",
        "开放平台",
        "高鲁棒感知",
        "高精度定位",
        "定位与建图",
        "精细操作",
        "协同"
      ],
      "answer": "具身智能开放平台围绕三个方向：复杂场景高鲁棒感知、高精度定位与建图、精细操作与协同。公开资料提到，它依托智能机器人、特种无人车、智能机器狗等载体。",
      "source_ids": [
        "S09"
      ]
    }
  ],
  "projects": [
    {
      "id": "proj_embodied_robot",
      "name": "面向汽车智能制造应用场景的具身智能机器人开发",
      "summary": "重庆市工信领域揭榜挂帅项目，具身智能机器人方向第一批；2025.04至2028.03；公开经费500万元；肖汉光主持",
      "status": "公开周期内，按周期推断在研",
      "keywords": [
        "揭榜挂帅",
        "汽车智能制造",
        "具身智能机器人开发",
        "500万",
        "在研项目"
      ],
      "answer": "公开的教师主页显示，肖汉光教授主持重庆市工信领域揭榜挂帅项目，题目是面向汽车智能制造应用场景的具身智能机器人开发，周期从2025年4月到2028年3月，公开经费是500万元。按公开周期推断，目前应在研，具体进展以学院正式介绍为准。",
      "source_ids": [
        "S13"
      ]
    }
  ],
  "equipment": [
    {
      "id": "eq_platform_carriers",
      "name": "具身智能开放平台载体",
      "summary": "智能机器人、特种无人车、智能机器狗等",
      "keywords": [
        "具身智能开放平台",
        "智能机器人",
        "特种无人车",
        "智能机器狗",
        "设备清单",
        "载体"
      ],
      "answer": "公开资料只披露，具身智能开放平台使用智能机器人、特种无人车、智能机器狗等载体。具体品牌、型号和数量没有公开，我不能确认。",
      "source_ids": [
        "S09"
      ]
    }
  ],
  "categories": [
    {
      "id": "introduction",
      "keywords": [
        "实验室介绍",
        "介绍一下实验室",
        "实验室概况",
        "实验室是做什么",
        "主要做什么"
      ],
      "answer": "重庆理工大学两江人工智能学院设有脑机交互智能研究中心和人形机器人具身智能感知与自主学习重庆市重点实验室，并建设具身智能开放平台。脑机交互智能研究中心于2025年12月30日成立，定位是高起点、大交叉、国际化、产业化。市级重点实验室于2025年7月获批，由重庆理工大学与赛力斯汽车有限公司共建。",
      "source_ids": [
        "S04",
        "S05",
        "S01",
        "S02",
        "S09"
      ]
    },
    {
      "id": "research_directions",
      "keywords": [
        "研究方向",
        "研究什么",
        "主要研究",
        "科研方向",
        "做什么研究"
      ],
      "answer": "公开资料中，脑机交互智能研究中心有五个方向：神经信号感知、脑网络组应用、脑病智能诊疗、脑机自然交互、类脑智能系统。市级重点实验室研究复杂场景下的抗干扰多模态融合学习、人机环境交互的域泛化和抗知识遗忘持续学习，以及技能可迁移自主学习。",
      "source_ids": [
        "S04",
        "S02"
      ]
    },
    {
      "id": "projects",
      "keywords": [
        "科研项目",
        "在研项目",
        "揭榜挂帅",
        "项目进展",
        "做了什么项目",
        "有什么项目"
      ],
      "answer": "公开的教师主页显示，肖汉光教授主持重庆市工信领域揭榜挂帅项目，题目是面向汽车智能制造应用场景的具身智能机器人开发，周期从2025年4月到2028年3月，公开经费是500万元。按公开周期推断，目前应在研，具体进展以学院正式介绍为准。",
      "source_ids": [
        "S13"
      ]
    },
    {
      "id": "equipment",
      "keywords": [
        "实验设备",
        "设备清单",
        "有什么设备",
        "有哪些设备",
        "有什么机器人",
        "有哪些机器人",
        "机器人型号",
        "实验室设备",
        "实验室机器人"
      ],
      "answer": "公开资料只披露，具身智能开放平台使用智能机器人、特种无人车、智能机器狗等载体。具体品牌、型号和数量没有公开，我不能确认。",
      "source_ids": [
        "S09"
      ]
    },
    {
      "id": "location",
      "keywords": [
        "实验室在哪里",
        "实验室地址",
        "学院地址",
        "具体位置",
        "怎么走",
        "在哪个楼",
        "实验室位置"
      ],
      "answer": "学院位于重庆市渝北区龙兴镇普福大道459号，重庆理工大学两江校区。实验室的具体房间和参观路线，公开资料没有说明。",
      "source_ids": [
        "S06",
        "S21"
      ]
    }
  ],
  "faq": [
    {
      "id": "faq_intro",
      "questions": [
        "介绍一下实验室",
        "介绍一下你们实验室",
        "你们实验室是做什么的",
        "实验室是做什么的",
        "你们实验室主要做什么",
        "实验室概况"
      ],
      "keywords": [
        "实验室介绍",
        "实验室概况",
        "实验室是做什么",
        "主要做什么",
        "介绍一下实验室"
      ],
      "answer": "重庆理工大学两江人工智能学院设有脑机交互智能研究中心和人形机器人具身智能感知与自主学习重庆市重点实验室，并建设具身智能开放平台。脑机交互智能研究中心于2025年12月30日成立，定位是高起点、大交叉、国际化、产业化。市级重点实验室于2025年7月获批，由重庆理工大学与赛力斯汽车有限公司共建。",
      "source_ids": [
        "S04",
        "S05",
        "S01",
        "S02",
        "S09"
      ],
      "category": "introduction"
    },
    {
      "id": "faq_bci_what",
      "questions": [
        "脑机交互智能研究中心是什么",
        "脑机中心是做什么的",
        "脑机交互智能实验室是什么"
      ],
      "keywords": [
        "脑机交互智能研究中心",
        "脑机中心",
        "脑机交互智能实验室"
      ],
      "answer": "官方名称是脑机交互智能研究中心，2025年12月30日成立，定位是高起点、大交叉、国际化、产业化。它公开的研究方向有神经信号感知、脑网络组应用、脑病智能诊疗、脑机自然交互和类脑智能系统。",
      "source_ids": [
        "S04",
        "S05"
      ],
      "category": "labs"
    },
    {
      "id": "faq_bci_directions",
      "questions": [
        "脑机中心有哪些研究方向",
        "脑机中心研究什么",
        "脑机交互有哪些方向"
      ],
      "keywords": [
        "脑机",
        "神经信号感知",
        "脑网络组",
        "脑病智能诊疗",
        "脑机自然交互",
        "类脑智能",
        "脑机中心方向",
        "脑机研究方向"
      ],
      "answer": "脑机交互智能研究中心公开的五个研究方向是：神经信号感知、脑网络组应用、脑病智能诊疗、脑机自然交互、类脑智能系统。",
      "source_ids": [
        "S04"
      ],
      "category": "research_directions"
    },
    {
      "id": "faq_bci_leaders",
      "questions": [
        "脑机中心负责人是谁",
        "脑机中心主任是谁",
        "谁负责脑机中心"
      ],
      "keywords": [
        "脑机",
        "主任",
        "负责人",
        "蒋田仔",
        "刘宏",
        "肖汉光",
        "朱秘"
      ],
      "answer": "脑机交互智能研究中心的学术委员会主任是蒋田仔研究员，中心主任是刘宏教授，执行主任是肖汉光教授，副主任是朱秘老师。",
      "source_ids": [
        "S04",
        "S11"
      ],
      "category": "people"
    },
    {
      "id": "faq_bci_date",
      "questions": [
        "脑机中心什么时候成立",
        "脑机中心成立时间"
      ],
      "keywords": [
        "脑机",
        "成立时间",
        "2025年12月30日"
      ],
      "answer": "脑机交互智能研究中心于2025年12月30日在重庆理工大学两江人工智能学院成立。",
      "source_ids": [
        "S04",
        "S05"
      ],
      "category": "labs"
    },
    {
      "id": "faq_embodied_name",
      "questions": [
        "具身智能实验室的正式名称是什么",
        "具身智能实验室叫什么",
        "你们有具身智能实验室吗"
      ],
      "keywords": [
        "具身智能实验室",
        "正式名称",
        "重点实验室",
        "开放平台"
      ],
      "answer": "公开可查的有两个机构：一个是人形机器人具身智能感知与自主学习重庆市重点实验室，另一个是学院的具身智能开放平台。没有只叫具身智能实验室的官方机构。",
      "source_ids": [
        "S01",
        "S09"
      ],
      "category": "labs"
    },
    {
      "id": "faq_embodied_research",
      "questions": [
        "具身智能实验室研究什么",
        "重点实验室研究什么",
        "人形机器人实验室研究什么"
      ],
      "keywords": [
        "具身智能",
        "重点实验室",
        "多模态融合",
        "域泛化",
        "知识遗忘",
        "技能迁移",
        "自主学习",
        "重点实验室方向",
        "具身智能研究方向"
      ],
      "answer": "这个市级重点实验室主要研究三块：复杂场景下具身智能感知的抗干扰多模态融合学习；开放场景下人机环境交互的域泛化和抗知识遗忘持续学习；动态场景下技能可迁移的自主学习。应用方向主要是汽车智能制造和电子柔性装配。",
      "source_ids": [
        "S02",
        "S03"
      ],
      "category": "research_directions"
    },
    {
      "id": "faq_embodied_approval",
      "questions": [
        "重点实验室什么时候获批",
        "具身智能实验室什么时候成立"
      ],
      "keywords": [
        "重点实验室",
        "获批",
        "2025年7月"
      ],
      "answer": "人形机器人具身智能感知与自主学习重庆市重点实验室在2025年7月获批。依托单位是重庆理工大学，共建单位是赛力斯汽车有限公司。",
      "source_ids": [
        "S01",
        "S02"
      ],
      "category": "labs"
    },
    {
      "id": "faq_embodied_seres",
      "questions": [
        "重点实验室和赛力斯是什么关系",
        "赛力斯和实验室有什么关系"
      ],
      "keywords": [
        "赛力斯",
        "共建",
        "重点实验室"
      ],
      "answer": "赛力斯汽车有限公司是这个重庆市重点实验室的共建单位。除了共建实验室，双方还有哪些具体合作项目，公开资料没有说明。",
      "source_ids": [
        "S01",
        "S02"
      ],
      "category": "labs"
    },
    {
      "id": "faq_platform",
      "questions": [
        "具身智能开放平台是什么",
        "开放平台研究什么"
      ],
      "keywords": [
        "具身智能开放平台",
        "开放平台",
        "高鲁棒感知",
        "定位与建图",
        "精细操作"
      ],
      "answer": "具身智能开放平台围绕复杂场景高鲁棒感知、高精度定位与建图、精细操作与协同三个方向。公开资料提到，它依托智能机器人、特种无人车、智能机器狗等载体。",
      "source_ids": [
        "S09"
      ],
      "category": "labs"
    },
    {
      "id": "faq_difference",
      "questions": [
        "脑机中心和重点实验室是一回事吗",
        "两个实验室是什么关系"
      ],
      "keywords": [
        "脑机中心",
        "重点实验室",
        "一回事",
        "关系"
      ],
      "answer": "不是同一个名称。脑机方向的官方机构是脑机交互智能研究中心；具身智能方向的官方机构是人形机器人具身智能感知与自主学习重庆市重点实验室。两者在组织上是什么关系，公开资料没有说明。",
      "source_ids": [
        "S04",
        "S01",
        "S09"
      ],
      "category": "labs"
    },
    {
      "id": "faq_equipment",
      "questions": [
        "实验室有哪些设备",
        "实验室有哪些机器人",
        "有机器狗吗",
        "实验室有几台机器人"
      ],
      "keywords": [
        "设备",
        "机器人",
        "机器狗",
        "无人车",
        "载体",
        "实验室设备",
        "实验室机器人"
      ],
      "answer": "公开资料只披露，具身智能开放平台使用智能机器人、特种无人车、智能机器狗等载体。具体品牌、型号和数量没有公开，我不能确认。",
      "source_ids": [
        "S09"
      ],
      "category": "equipment"
    },
    {
      "id": "faq_location",
      "questions": [
        "实验室在哪里",
        "实验室地址",
        "学院地址",
        "怎么走"
      ],
      "keywords": [
        "地址",
        "在哪里",
        "位置",
        "普福大道",
        "两江校区",
        "实验室位置"
      ],
      "answer": "学院位于重庆市渝北区龙兴镇普福大道459号，重庆理工大学两江校区。实验室的具体房间和参观路线，公开资料没有说明。",
      "source_ids": [
        "S06",
        "S21"
      ],
      "category": "location"
    },
    {
      "id": "faq_teams",
      "questions": [
        "学院有哪些科研团队",
        "学院有哪些团队"
      ],
      "keywords": [
        "科研团队",
        "团队",
        "机器视觉",
        "大数据",
        "机器博弈",
        "智能控制",
        "智能机器人",
        "学院团队"
      ],
      "answer": "公开资料里，学院建有五个科研团队：图像处理与机器视觉、大数据与智能信息处理、机器学习与机器博弈、智能硬件与智能控制、智能机器人。",
      "source_ids": [
        "S07"
      ],
      "category": "college"
    },
    {
      "id": "faq_awards",
      "questions": [
        "学院学生拿过什么奖",
        "学院竞赛成绩"
      ],
      "keywords": [
        "竞赛",
        "获奖",
        "机器人及人工智能大赛",
        "一等奖",
        "拿过什么奖"
      ],
      "answer": "2026年8月，学院学生团队在第28届中国机器人及人工智能大赛全国总决赛中，获得国家级奖项15项，其中一等奖2项。这是学生团队的成绩。",
      "source_ids": [
        "S16"
      ],
      "category": "college"
    },
    {
      "id": "faq_xiao",
      "questions": [
        "肖汉光教授是谁",
        "肖汉光是做什么的"
      ],
      "keywords": [
        "肖汉光",
        "副院长",
        "实验室主任",
        "执行主任"
      ],
      "answer": "肖汉光教授是学院副院长、博士生导师，也是人形机器人具身智能感知与自主学习重庆市重点实验室主任、脑机交互智能研究中心执行主任。他的公开研究方向包括医学影像与生理信号智能分析、图像处理与机器视觉、具身智能。",
      "source_ids": [
        "S10",
        "S14"
      ],
      "category": "people"
    },
    {
      "id": "faq_liu",
      "questions": [
        "刘宏教授是谁",
        "刘宏院长是谁"
      ],
      "keywords": [
        "刘宏",
        "院长",
        "北京大学",
        "脑机中心主任"
      ],
      "answer": "刘宏教授是北京大学教授、博士生导师，北京大学人工智能研究院具身智能与机器人中心主任，同时担任重庆理工大学两江人工智能学院院长和脑机交互智能研究中心主任。他在北京大学的研究工作，不能算作重庆理工大学实验室的成果。",
      "source_ids": [
        "S12",
        "S04"
      ],
      "category": "people"
    },
    {
      "id": "faq_project",
      "questions": [
        "学院有什么在研项目",
        "有什么科研项目",
        "揭榜挂帅项目是什么"
      ],
      "keywords": [
        "项目",
        "揭榜挂帅",
        "汽车智能制造",
        "500万",
        "有什么项目"
      ],
      "answer": "公开的教师主页显示，肖汉光教授主持重庆市工信领域揭榜挂帅项目，题目是面向汽车智能制造应用场景的具身智能机器人开发，周期从2025年4月到2028年3月，公开经费是500万元。按公开周期推断，目前应在研，具体进展以学院正式介绍为准。",
      "source_ids": [
        "S13"
      ],
      "category": "projects"
    },
    {
      "id": "faq_college_focus",
      "questions": [
        "学院主要研究什么",
        "学院的研究方向是什么"
      ],
      "keywords": [
        "学院",
        "研究",
        "智能感知",
        "先进装备"
      ],
      "answer": "学院聚焦面向先进装备的智能感知技术，建设了脑机交互智能研究中心、人形机器人具身智能感知与自主学习重庆市重点实验室和具身智能开放平台。",
      "source_ids": [
        "S06",
        "S04",
        "S01",
        "S09"
      ],
      "category": "college"
    },
    {
      "id": "faq_college_established",
      "questions": [
        "学院什么时候成立",
        "学院成立时间"
      ],
      "keywords": [
        "学院",
        "成立时间",
        "2018年6月9日",
        "什么时候成立"
      ],
      "answer": "重庆理工大学两江人工智能学院成立于2018年6月9日，办学地点在两江校区。",
      "source_ids": [
        "S06"
      ],
      "category": "college"
    }
  ],
  "fallback_answer": "这个信息目前没有公开资料，我不能确认。您可以问我实验室的研究方向、重点实验室、具身智能开放平台或者学院地址。",
  "disclaimer": "以下内容来自公开资料，具体以学院正式介绍为准。",
  "sources": [
    {
      "id": "S01",
      "title": "2025年拟认定重庆市重点实验室名单",
      "publisher": "重庆市科学技术局",
      "url": "http://kjj.cq.gov.cn/zwxx_176/tzgg/202507/P020250704593476930297.pdf",
      "date": "2025-07-04",
      "level": "A",
      "access_date": "2026-10-31",
      "verification": "direct_pdf"
    },
    {
      "id": "S02",
      "title": "【喜报】市级重点实验室获批！我院在人形机器人方向实现关键突破",
      "publisher": "重庆理工大学两江人工智能学院",
      "url": "https://ai.cqut.edu.cn/info/1041/5044.htm",
      "date": "2025-07-16",
      "level": "A",
      "access_date": "2026-10-31",
      "verification": "indirect_waf"
    },
    {
      "id": "S03",
      "title": "[喜讯]学校新增3个市级科研平台",
      "publisher": "重庆理工大学",
      "url": "https://www.cqut.edu.cn/info/1264/67150.htm",
      "date": "2025-10-19",
      "level": "A",
      "access_date": "2026-10-31",
      "verification": "indirect_waf"
    },
    {
      "id": "S04",
      "title": "重庆理工大学举行脑机交互智能研究中心成立仪式暨2025脑机交互前沿学术报告",
      "publisher": "第1眼TV-华龙网",
      "url": "https://www.cqnews.net/app/content_1455983106463735808.html",
      "date": "2025-12-31",
      "level": "C",
      "access_date": "2026-10-31",
      "verification": "direct"
    },
    {
      "id": "S05",
      "title": "脑机交互智能研究中心在两江人工智能学院成立",
      "publisher": "重庆理工大学两江人工智能学院",
      "url": "https://ai.cqut.edu.cn/info/1041/5393.htm",
      "date": "2026-01-05",
      "level": "A",
      "access_date": "2026-10-31",
      "verification": "indirect_waf"
    },
    {
      "id": "S06",
      "title": "学院简介",
      "publisher": "重庆理工大学两江人工智能学院",
      "url": "https://ai.cqut.edu.cn/xygk/xyjj.htm",
      "date": "",
      "level": "A",
      "access_date": "2026-10-31",
      "verification": "indirect_waf"
    },
    {
      "id": "S07",
      "title": "两江人工智能学院2025年本科生招生简章",
      "publisher": "重庆理工大学两江人工智能学院",
      "url": "https://ai.cqut.edu.cn/info/1024/4889.htm",
      "date": "2025",
      "level": "A",
      "access_date": "2026-10-31",
      "verification": "indirect_waf"
    },
    {
      "id": "S09",
      "title": "两江人工智能学院举行建院8周年产教融合开放日活动",
      "publisher": "重庆理工大学两江人工智能学院",
      "url": "https://ai.cqut.edu.cn/info/1041/5803.htm",
      "date": "2026-06-10",
      "level": "A",
      "access_date": "2026-10-31",
      "verification": "indirect_waf"
    },
    {
      "id": "S10",
      "title": "肖汉光",
      "publisher": "重庆理工大学两江人工智能学院",
      "url": "https://ai.cqut.edu.cn/info/1052/1679.htm",
      "date": "2026-01-14",
      "level": "A",
      "access_date": "2026-10-31",
      "verification": "indirect_waf"
    },
    {
      "id": "S11",
      "title": "朱秘",
      "publisher": "重庆理工大学两江人工智能学院",
      "url": "https://ai.cqut.edu.cn/info/1054/3154.htm",
      "date": "2026-01-14",
      "level": "A",
      "access_date": "2026-10-31",
      "verification": "indirect_waf"
    },
    {
      "id": "S12",
      "title": "刘宏（院长）",
      "publisher": "重庆理工大学两江人工智能学院",
      "url": "https://ai.cqut.edu.cn/info/1005/1025.htm",
      "date": "",
      "level": "A",
      "access_date": "2026-10-31",
      "verification": "indirect_waf"
    },
    {
      "id": "S13",
      "title": "肖汉光（机械工程学院教师页）",
      "publisher": "重庆理工大学机械工程学院",
      "url": "https://jxgc.cqut.edu.cn/info/1117/7003.htm",
      "date": "2025-09-26",
      "level": "A",
      "access_date": "2026-10-31",
      "verification": "indirect_waf"
    },
    {
      "id": "S14",
      "title": "特色论坛 - 中国生物特征识别大会 CCBR 2025",
      "publisher": "中国生物特征识别大会 CCBR 2025",
      "url": "https://ccbr99.cn/2025/forums.html",
      "date": "2025",
      "level": "B",
      "access_date": "2026-10-31",
      "verification": "direct"
    },
    {
      "id": "S16",
      "title": "[喜讯]学校参赛团队在第28届中国机器人及人工智能大赛全国总决赛中斩获一等奖等15项荣誉",
      "publisher": "重庆理工大学",
      "url": "https://www.cqut.edu.cn/info/1049/71663.htm",
      "date": "2026-08-24",
      "level": "A",
      "access_date": "2026-10-31",
      "verification": "indirect_waf"
    },
    {
      "id": "S21",
      "title": "两江人工智能学院首页",
      "publisher": "重庆理工大学两江人工智能学院",
      "url": "https://ai.cqut.edu.cn/",
      "date": "",
      "level": "A",
      "access_date": "2026-10-31",
      "verification": "indirect_waf"
    }
  ]
}
```
