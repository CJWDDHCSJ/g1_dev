# 附录 C：代码变更审查（备份 → 当前部署）

> 备份来源：`/home/unitree/g1_backups/g1_voice_router_20261031_132307_before_kb/`（G1 上的知识库改造前备份）

> 当前来源：`/home/unitree/unitree_ros2/example/g1_voice_router/`（G1 当前部署目录）

> 本地副本：`backup_snapshot/` 与 `remote_snapshot/g1_voice_router/`

> 生成方式：`diff -u`，仅做路径前缀替换，未改动 hunk 内容。


## C.1 变更文件总览

| 文件 | 备份中是否存在 | 当前 SHA256 | 备份 SHA256 | 结论 |
|---|---|---|---|---|
| `CMakeLists.txt` | 是 | `a7166376f6335ae346250d5b4af23e9fe9a6d624bf117420906ec46cee2feb8c` | `b9ee317568bfc11eb09b720fc857866d165ecd626784d9f4f1e57b4c232c3225` | 修改 |
| `package.xml` | 是 | `22d0f4a6341c455bc83122d7983c15490a0bd96730136dab247ae5d2a42dcb39` | `d23ae7506c073529cc16ce8b25b85823bfac27a764c0ff5d29c712c8fba8d290` | 修改 |
| `src/voice_router.cpp` | 是 | `fd6d29c02757eb82dba517d53d2f3f72c82d324cc56741ae917e8e42988e0148` | `491fc3b9efafebd3e85278981a0ec3383d9a6f726b95c06392a8c61c69b14f2a` | 修改 |
| `src/intent_recognizer.cpp` | 是 | `80d50eaaa8a75733f55bda629b5be19cf8fd9b0a921d8974e2441fda80640569` | `7b4bae1c3f7509a630497077e2307ceb1007794087700b44d2b25ae4cc737b31` | 修改 |
| `test/intent_recognizer_test.cpp` | 是 | `cd80b15f4542412051133e30a9a7faefc55eca7dbb20fe130778efd9c3abb3d7` | `36923d106dd59a9659b71c8d6c284a70f89ccdce1bbfa99b38f85d312fba3e62` | 修改 |
| `include/g1_voice_router/intent_recognizer.hpp` | 是 | `a208e31fdf4970177438b98fb2def583b6ae459f22d9cec22387fc706ae709c5` | `a208e31fdf4970177438b98fb2def583b6ae459f22d9cec22387fc706ae709c5` | 未修改 |
| `include/g1_voice_router/knowledge_base.hpp` | 否 | `1dc19db4db18d5f3503488fe7486d5e794b5c645fc6259ee878bebb53d57a2af` | — | 新增 |
| `src/knowledge_base.cpp` | 否 | `295df377a43e0eb9675bb6f6cc8cdad6f26a3fed4b74ecfbeb4e5bb62f043477` | — | 新增 |
| `test/knowledge_base_test.cpp` | 否 | `dab2aef7156ff2cf3762f98a9d1681f38200f140ebb865482d4749c3c9c742fa` | — | 新增 |
| `config/lab_knowledge.json` | 否 | `cbe711e3b5c19115dc9d85fe37c701829ee3ad37203a3a4337efa5263d5acea7` | — | 新增 |

### 未修改文件（备份与当前逐字节一致）

- `include/g1_voice_router/intent_recognizer.hpp`：SHA256 两边均为 `a208e31fdf4970177438b98fb2def583b6ae459f22d9cec22387fc706ae709c5`。

- 未修改的运行时链路文件：ASR 桥接、TTS 节点、Action Executor、`unitree_ros2_example`、DDS 配置、systemd/启动脚本、Ollama 服务配置。


## C.2 逐文件 Unified Diff


### C.2.1 `CMakeLists.txt`

```diff
--- backup/CMakeLists.txt
+++ current/CMakeLists.txt
@@ -4,9 +4,22 @@
 find_package(ament_cmake REQUIRED)
 find_package(rclcpp REQUIRED)
 find_package(std_msgs REQUIRED)
+find_package(ament_index_cpp REQUIRED)
 find_package(CURL REQUIRED)
 find_package(Threads REQUIRED)
 
+add_library(knowledge_base STATIC
+  src/knowledge_base.cpp
+)
+
+target_compile_features(knowledge_base PUBLIC cxx_std_17)
+
+# 复用工作区现有的 nlohmann/json.hpp。
+target_include_directories(knowledge_base PUBLIC
+  "${CMAKE_CURRENT_SOURCE_DIR}/include"
+  "${CMAKE_CURRENT_SOURCE_DIR}/../src/include"
+)
+
 add_executable(voice_router
   src/voice_router.cpp
   src/intent_recognizer.cpp
@@ -14,7 +27,6 @@
 
 target_compile_features(voice_router PUBLIC cxx_std_17)
 
-# 复用工作区现有的 nlohmann/json.hpp。
 target_include_directories(voice_router PRIVATE
   "${CMAKE_CURRENT_SOURCE_DIR}/include"
   "${CMAKE_CURRENT_SOURCE_DIR}/../src/include"
@@ -23,9 +35,11 @@
 ament_target_dependencies(voice_router
   rclcpp
   std_msgs
+  ament_index_cpp
 )
 
 target_link_libraries(voice_router
+  knowledge_base
   CURL::libcurl
   Threads::Threads
 )
@@ -35,4 +49,46 @@
   DESTINATION lib/${PROJECT_NAME}
 )
 
+install(
+  FILES config/lab_knowledge.json
+  DESTINATION share/${PROJECT_NAME}/config
+)
+
+include(CTest)
+
+if(BUILD_TESTING)
+  add_executable(knowledge_base_test
+    test/knowledge_base_test.cpp
+  )
+
+  target_link_libraries(knowledge_base_test
+    knowledge_base
+  )
+
+  target_include_directories(knowledge_base_test PRIVATE
+    "${CMAKE_CURRENT_SOURCE_DIR}/include"
+    "${CMAKE_CURRENT_SOURCE_DIR}/../src/include"
+  )
+
+  add_test(
+    NAME knowledge_base_test
+    COMMAND knowledge_base_test
+      "${CMAKE_CURRENT_SOURCE_DIR}/config/lab_knowledge.json"
+  )
+
+  add_executable(intent_recognizer_test
+    test/intent_recognizer_test.cpp
+    src/intent_recognizer.cpp
+  )
+
+  target_include_directories(intent_recognizer_test PRIVATE
+    "${CMAKE_CURRENT_SOURCE_DIR}/include"
+  )
+
+  add_test(
+    NAME intent_recognizer_test
+    COMMAND intent_recognizer_test
+  )
+endif()
+
 ament_package()
```


### C.2.2 `package.xml`

```diff
--- backup/package.xml
+++ current/package.xml
@@ -10,6 +10,7 @@
   <buildtool_depend>ament_cmake</buildtool_depend>
   <depend>rclcpp</depend>
   <depend>std_msgs</depend>
+  <depend>ament_index_cpp</depend>
   <depend>libcurl-dev</depend>
 
   <export>
```


### C.2.3 `voice_router`

```diff
--- backup/voice_router.cpp
+++ current/voice_router.cpp
@@ -1,5 +1,6 @@
 #include <curl/curl.h>
 #include <algorithm>
+#include <ament_index_cpp/get_package_share_directory.hpp>
 #include <nlohmann/json.hpp>
 
 #include <cctype>
@@ -14,6 +15,7 @@
 #include <utility>
 
 #include "g1_voice_router/intent_recognizer.hpp"
+#include "g1_voice_router/knowledge_base.hpp"
 #include "rclcpp/rclcpp.hpp"
 #include "std_msgs/msg/string.hpp"
 
@@ -43,6 +45,43 @@
       timeout_seconds_ = 60;
     }
 
+    std::string default_knowledge_file;
+
+    try {
+      default_knowledge_file =
+        ament_index_cpp::get_package_share_directory(
+          "g1_voice_router") +
+        "/config/lab_knowledge.json";
+    } catch (const std::exception & error) {
+      RCLCPP_ERROR(
+        get_logger(),
+        "Failed to locate g1_voice_router share directory: %s",
+        error.what());
+    }
+
+    knowledge_file_ = declare_parameter<std::string>(
+      "knowledge_file",
+      default_knowledge_file);
+
+    std::string knowledge_error;
+
+    if (!knowledge_base_.load(knowledge_file_, knowledge_error)) {
+      RCLCPP_ERROR(
+        get_logger(),
+        "KnowledgeBase load failed: %s",
+        knowledge_error.c_str());
+    } else {
+      RCLCPP_INFO(
+        get_logger(),
+        "KnowledgeBase loaded: %s "
+        "(FAQ=%zu, directions=%zu, projects=%zu, equipment=%zu)",
+        knowledge_file_.c_str(),
+        knowledge_base_.faq_count(),
+        knowledge_base_.direction_count(),
+        knowledge_base_.project_count(),
+        knowledge_base_.equipment_count());
+    }
+
     system_prompt_ = declare_parameter<std::string>(
       "system_prompt",
       "你是运行在宇树G1人形机器人中的中文语音助手。"
@@ -265,6 +304,31 @@
     
 
     // --------------------------------------------------------
+    // LAB_INTRO：本地知识库
+    // 有资料时直接回答；资料缺失时返回安全兜底。
+    // 不进入 Ollama 消息队列。
+    // --------------------------------------------------------
+
+    if (intent_result.intent ==
+        g1_voice_router::Intent::LAB_INTRO)
+    {
+      const auto result = knowledge_base_.query(text);
+
+      publish_reply(result.answer);
+
+      RCLCPP_INFO(
+        get_logger(),
+        "LAB_INTRO local knowledge base completed: "
+        "found=%s category=%s matched_id=%s",
+        result.found ? "true" : "false",
+        result.category.c_str(),
+        result.matched_id.c_str());
+
+      return;
+    }
+
+
+    // --------------------------------------------------------
     // ROBOT_INTRO：机器人自我介绍
     // 本地固定回答，不调用 Ollama
     // --------------------------------------------------------
@@ -563,6 +627,10 @@
 
   std::deque<Json> history_;
 
+  std::string knowledge_file_;
+
+  g1_voice_router::KnowledgeBase knowledge_base_;
+
   g1_voice_router::IntentRecognizer intent_recognizer_;
 
   rclcpp::Subscription<std_msgs::msg::String>::SharedPtr
```


### C.2.4 `intent_recognizer.cpp`

```diff
--- backup/intent_recognizer.cpp
+++ current/intent_recognizer.cpp
@@ -153,7 +153,26 @@
         "研究方向",
         "科研项目",
         "实验设备",
-        "实验成果"
+        "实验成果",
+        "脑机",
+        "具身智能",
+        "重点实验室",
+        "开放平台",
+        "赛力斯",
+        "在研项目",
+        "揭榜挂帅",
+        "科研团队",
+        "学院有哪些团队",
+        "学院学生拿过什么奖",
+        "学院竞赛",
+        "学院什么时候成立",
+        "学院成立",
+        "学院地址",
+        "学院在哪里",
+        "学院主要研究",
+        "肖汉光",
+        "刘宏",
+        "有机器狗吗"
     }))
     {
         result.intent = Intent::LAB_INTRO;
```


### C.2.5 `intent_recognizer_test.cpp`

```diff
--- backup/intent_recognizer_test.cpp
+++ current/intent_recognizer_test.cpp
@@ -30,6 +30,17 @@
         {"当前运行模式是什么？", Intent::ROBOT_STATUS},
         {"谢谢", Intent::GOODBYE},
         {"再见！", Intent::GOODBYE},
+        {"学院有什么在研项目", Intent::LAB_INTRO},
+        {"脑机中心负责人是谁", Intent::LAB_INTRO},
+        {"学院有哪些科研团队", Intent::LAB_INTRO},
+        {"学院学生拿过什么奖", Intent::LAB_INTRO},
+        {"学院什么时候成立", Intent::LAB_INTRO},
+        {"学院地址在哪里", Intent::LAB_INTRO},
+        {"具身智能开放平台是什么", Intent::LAB_INTRO},
+        {"肖汉光教授是谁", Intent::LAB_INTRO},
+        {"有机器狗吗", Intent::LAB_INTRO},
+        {"学院有哪些专业", Intent::GENERAL_QA},
+        {"机器狗多少钱", Intent::GENERAL_QA},
         {"宇树机器人多少钱？", Intent::GENERAL_QA}
     };
 
```


## C.3 关键变更目的与行为影响

### C.3.1 `src/voice_router.cpp`

| 变更点 | 目的 | 对原行为的影响 |
|---|---|---|
| 增加 `ament_index_cpp` include 与 `knowledge_base.hpp` include | 通过 ROS 2 安装索引定位知识库，并引入本地查询类 | 编译依赖增加；运行逻辑不改 |
| 构造函数中解析 `knowledge_file` 参数并 `load()` | 知识库只加载一次，常驻内存 | 加载失败时只记录 `ERROR`，节点继续启动；所有 `LAB_INTRO` 走兜底 |
| 在 `enqueue_user_text()` 中新增 `LAB_INTRO` 分支 | 实验室问题本地回答，禁止进入 Ollama | `LAB_INTRO` 不再排队、不再调用 Ollama；其他意图不变 |
| 新增 `knowledge_file_`、`knowledge_base_` 成员 | 保存路径与内存数据 | 对象生命周期与节点一致 |

`enqueue_user_text()` 的实际分发顺序（当前版本）：

1. `trim`，空文本直接忽略（`voice_router.cpp:263-272`）。
2. `IntentRecognizer::recognize()`（`:274-282`）。
3. `GREETING` 且 `match_greeting_reply()` 成功：发布 `wave` 到 `/g1/action_request`，本地回复，return（`:286-303`）。
4. `LAB_INTRO`：`knowledge_base_.query(text)`，发布 `/g1/tts`，return（`:312-328`）。
5. `ROBOT_INTRO`：固定文本，return（`:336-353`）。
6. `GOODBYE`：固定文本，return（`:361-375`）。
7. 其余（含 `DEMO_ACTION`、`ROBOT_STATUS`、`GENERAL_QA`）：入队，由单工作线程调用 Ollama（`:378-397`）。

> 与备份相比，唯一的分发顺序变化是第 4 步插入在 `ROBOT_INTRO` 之前。`DEMO_ACTION` 与 `ROBOT_STATUS` 在备份和当前版本中都没有本地分支，仍会进入 Ollama；这不是本次知识库改造引入的回归。

### C.3.2 `src/intent_recognizer.cpp`

- 只扩展 `LAB_INTRO` 的 `contains_any` 关键词列表，新增 21 个词/短语。
- 其余意图的关键词、判断顺序、`normalize()` 逻辑未改。
- 影响：更多学院/实验室/人物/项目/设备类问题进入本地知识库；同时因为 `LAB_INTRO` 检查在 `ROBOT_STATUS`、`DEMO_ACTION` 之前，包含“展示一下”且包含“具身智能/实验室”的句子会被判为 `LAB_INTRO`，不会触发动作。详见主报告第 10 节。

### C.3.3 `test/intent_recognizer_test.cpp`

- 从 14 条用例扩展到 25 条；新增 9 条 `LAB_INTRO`、2 条 `GENERAL_QA` 回归用例。
- 未修改测试框架，仍为独立可执行文件，返回非零表示失败。

### C.3.4 `CMakeLists.txt`

- 增加 `find_package(ament_index_cpp REQUIRED)`。
- 新增静态库 `knowledge_base`，只编译 `src/knowledge_base.cpp`，复用工作区 `../src/include` 下的 `nlohmann/json.hpp`。
- `voice_router` 链接 `knowledge_base` 与 `ament_index_cpp`。
- 安装 `config/lab_knowledge.json` 到 `share/${PROJECT_NAME}/config`。
- 增加 `knowledge_base_test` 与 `intent_recognizer_test` 两个 CTest 目标；KB 测试把源码树中的 JSON 路径作为参数传入。
- 未改变 `voice_router` 可执行文件安装位置、C++ 标准（仍为 C++17）、CURL/Threads 依赖。

### C.3.5 `package.xml`

- 增加 `<depend>ament_index_cpp</depend>`；版本仍为 `0.2.0`。

### C.3.6 未改变的部分（明确核对）

| 项目 | 结论 | 证据 |
|---|---|---|
| `/g1/asr_text` 订阅 | 未变 | `voice_router.cpp:111-117`，类型/QoS 深度 10 |
| `/g1/tts` 发布 | 未变 | `voice_router.cpp:100-102`，类型/QoS 深度 10 |
| `/g1/action_request` 发布 | 未变 | `voice_router.cpp:105-107`，仅 GREETING 发布 `wave` |
| Ollama URL / 模型 / 请求体 / 超时 | 未变 | `voice_router.cpp:30-39, 400-560` |
| 工作线程、队列、条件变量 | 未变 | `voice_router.cpp:579-621, 645-649`；LAB_INTRO 在入队前 return |
| 对话历史裁剪（最多 6 轮） | 未变 | `voice_router.cpp:535-550` |
| ASR/TTS/Action Executor 源码 | 未改 | 不在本次 diff 范围；远端文件哈希未变（见主报告） |
| DDS / 网络 / systemd / Ollama 配置 | 未改 | 本轮只读核查，未执行写操作 |

## C.4 变更性质结论

- 本次变更属于**增量式接入**：新增知识库模块与安装/测试配置，在路由器中插入一个本地分支，扩展一个意图的关键词。
- 未重构原有队列、线程、HTTP、历史管理逻辑。
- 未修改 ROS 2 话题名称、消息类型、QoS。
- 未新增除 `ament_index_cpp` 外的第三方依赖；`nlohmann/json.hpp` 复用工作区既有头文件。
- 主要行为变化是设计目标本身：`LAB_INTRO` 从“进入 Ollama 队列”变为“本地知识库同步回答”。
