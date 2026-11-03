# 附录 E：完整测试证据（单元测试 / 隔离 ROS 测试 / 运行时测试）

> 本附录只整理本轮已经产生的原始输出，**没有重新编译、没有重新运行测试、没有重启任何节点、没有向机器人发送语音或动作请求**。

> 证据来源：G1 上 `/home/unitree/unitree_ros2/example/log/` 下的 colcon/CTest 日志、临时启动的 Voice Router 日志、`ros2 topic echo` 捕获，以及本会话中执行过的隔离域测试输出。所有文件均已复制到本目录的 `evidence/` 下；下文引用路径时同时给出本地副本路径和机器人上的原始路径（若已知）。

## E.0 证据分级与阅读说明

| 级别 | 含义 | 本轮是否执行 | 环境 |
|---|---|---|---|
| 单元测试 | 直接运行 C++ 测试可执行文件，由 CTest 采集 | 是 | G1 本机 aarch64，不启动 ROS 节点 |
| 隔离 ROS 测试 | 单独设置 `ROS_DOMAIN_ID`，启动临时 Voice Router，注入 `/g1/asr_text`，捕获 `/g1/tts` | 是（域 42、43） | G1 本机，独立 DDS 域，不影响生产域 |
| 默认域运行时测试 | 在默认域启动临时 Voice Router，注入 `/g1/asr_text`，`ros2 topic echo` 捕获 `/g1/tts` | 是（runtime1/2/3、最终生产进程） | G1 本机，未启动 TTS/动作/ASR 节点 |
| 真机端到端测试 | 真实麦克风 → ASR → Voice Router → 真实 TTS 节点 → 扬声器；或真实动作执行器 | **否** | 本轮明确未执行 |

三个必须提前说明的限制：

1. **单元测试程序成功时不打印实际答案原文。** `knowledge_base_test.cpp` 的 `check()` 只在失败时输出 `detail`；成功时只打印 `[PASS] 名称`。因此下表中“实际结果”列记录的是**断言结果**，不是测试程序另行输出的答案字符串。涉及答案内容的断言，已在“期望结果”中写明必须出现/不得出现的文本。
2. **默认域运行时测试没有启动 TTS 节点。** `/g1/tts` 上只有一个遗留的 `ros2 topic echo` 订阅者（PID 4216），所以本轮捕获到的是消息内容，**没有产生机器人扬声器播放**。这符合本轮“不播放语音”的约束，但不能据此声称“真机语音播报已验证”。
3. **隔离域 42/43 的原始 `/tmp` 日志已被此前的测试命令删除。** 本附录只能引用会话中已经捕获的输出，并明确标注“会话记录，无法在机器人上复核”。其余 CTest/构建/运行时日志均有文件副本。

---

## E.1 构建与 CTest 原始输出

### E.1.1 构建命令与退出码

机器人上的包级构建由 colcon 调起，内部实际命令为（来自 `evidence/build_test/build_1332/events.log`）：

```text
/usr/bin/cmake --build /home/unitree/unitree_ros2/example/build/g1_voice_router -- -j4 -l4
```

构建完成后执行安装：

```text
/usr/bin/cmake --install /home/unitree/unitree_ros2/example/build/g1_voice_router
```

`evidence/build_test/latest_build/command.log` 全文：

```text
Invoking command in '/home/unitree/unitree_ros2/example/build/g1_voice_router': CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}:/opt/ros/humble /usr/bin/cmake --build /home/unitree/unitree_ros2/example/build/g1_voice_router -- -j4 -l4
Invoked command in '/home/unitree/unitree_ros2/example/build/g1_voice_router' returned '0': CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}:/opt/ros/humble /usr/bin/cmake --build /home/unitree/unitree_ros2/example/build/g1_voice_router -- -j4 -l4
Invoking command in '/home/unitree/unitree_ros2/example/build/g1_voice_router': CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}:/opt/ros/humble /usr/bin/cmake --install /home/unitree/unitree_ros2/example/build/g1_voice_router
Invoked command in '/home/unitree/unitree_ros2/example/build/g1_voice_router' returned '0': CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}:/opt/ros/humble /usr/bin/cmake --install /home/unitree/unitree_ros2/example/build/g1_voice_router
```

构建与安装退出码：**0 / 0**。包级 `JobEnded` 记录为 `rc: 0`（`evidence/build_test/build_1332/events.log`）。

外层命令也已从本会话记录中恢复。会话记录中可辨认出两次构建：

**（1）13:24 测试之前的一次构建（会话记录）：**

```text
colcon build --packages-select g1_voice_router
Starting >>> g1_voice_router
[Processing: g1_voice_router]
Finished <<< g1_voice_router [33.0s]
Summary: 1 package finished [33.6s]
=== build exit: 0 ===
```

**（2）13:32 最终构建，对应本附录的 `build_1332/` 日志（会话记录，仅保留捕获到的行）：**

```text
colcon build --packages-select g1_voice_router 2>&1
Finished <<< g1_voice_router [32.1s]
Summary: 1 package finished [32.7s]
```

第二次构建与 `build_1332/` 的内部时间吻合：内部 `cmake --build` 用时 31.939 秒，`cmake --install` 结束于 32.030 秒，外层 colcon 记录 32.1 秒 / 32.7 秒。

> 证据边界：上述外层 colcon 输出是会话记录，没有对应的 colcon 顶层日志副本；包级 `command.log`、`streams.log`、`events.log` 是文件证据。两次构建都只编译 `g1_voice_router` 一个包。

### E.1.2 完整构建输出

文件：`evidence/build_test/build_1332/g1_voice_router/streams.log`（4,209 字节），全文如下：

```text
[0.023s] Invoking command in '/home/unitree/unitree_ros2/example/build/g1_voice_router': CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}:/opt/ros/humble /usr/bin/cmake --build /home/unitree/unitree_ros2/example/build/g1_voice_router -- -j4 -l4
[0.103s] Consolidate compiler generated dependencies of target intent_recognizer_test
[0.120s] [ 10%] Building CXX object CMakeFiles/knowledge_base.dir/src/knowledge_base.cpp.o
[0.122s] [ 40%] Built target intent_recognizer_test
[5.656s] [ 50%] Linking CXX static library libknowledge_base.a
[5.760s] [ 50%] Built target knowledge_base
[5.794s] Consolidate compiler generated dependencies of target voice_router
[5.805s] [ 60%] Building CXX object CMakeFiles/knowledge_base_test.dir/test/knowledge_base_test.cpp.o
[5.821s] [ 70%] Building CXX object CMakeFiles/voice_router.dir/src/voice_router.cpp.o
[9.449s] [ 80%] Linking CXX executable knowledge_base_test
[9.713s] [ 80%] Built target knowledge_base_test
[30.746s] [ 90%] Linking CXX executable voice_router
[31.939s] [100%] Built target voice_router
[31.965s] Invoked command in '/home/unitree/unitree_ros2/example/build/g1_voice_router' returned '0': CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}:/opt/ros/humble /usr/bin/cmake --build /home/unitree/unitree_ros2/example/build/g1_voice_router -- -j4 -l4
[31.985s] Invoking command in '/home/unitree/unitree_ros2/example/build/g1_voice_router': CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}:/opt/ros/humble /usr/bin/cmake --install /home/unitree/unitree_ros2/example/build/g1_voice_router
[32.003s] -- Install configuration: ""
[32.003s] -- Execute custom install script
[32.004s] -- Up-to-date symlink: /home/unitree/unitree_ros2/example/install/g1_voice_router/lib/g1_voice_router/voice_router
[32.004s] -- Up-to-date symlink: /home/unitree/unitree_ros2/example/install/g1_voice_router/share/g1_voice_router/config/lab_knowledge.json
[32.005s] -- Up-to-date symlink: /home/unitree/unitree_ros2/example/install/g1_voice_router/share/ament_index/resource_index/package_run_dependencies/g1_voice_router
[32.005s] -- Up-to-date symlink: /home/unitree/unitree_ros2/example/install/g1_voice_router/share/ament_index/resource_index/parent_prefix_path/g1_voice_router
[32.006s] -- Up-to-date symlink: /home/unitree/unitree_ros2/example/install/g1_voice_router/share/g1_voice_router/environment/ament_prefix_path.sh
[32.006s] -- Up-to-date symlink: /home/unitree/unitree_ros2/example/install/g1_voice_router/share/g1_voice_router/environment/ament_prefix_path.dsv
[32.006s] -- Up-to-date symlink: /home/unitree/unitree_ros2/example/install/g1_voice_router/share/g1_voice_router/environment/path.sh
[32.007s] -- Up-to-date symlink: /home/unitree/unitree_ros2/example/install/g1_voice_router/share/g1_voice_router/environment/path.dsv
[32.007s] -- Up-to-date symlink: /home/unitree/unitree_ros2/example/install/g1_voice_router/share/g1_voice_router/local_setup.bash
[32.007s] -- Up-to-date symlink: /home/unitree/unitree_ros2/example/install/g1_voice_router/share/g1_voice_router/local_setup.sh
[32.008s] -- Up-to-date symlink: /home/unitree/unitree_ros2/example/install/g1_voice_router/share/g1_voice_router/local_setup.zsh
[32.008s] -- Up-to-date symlink: /home/unitree/unitree_ros2/example/install/g1_voice_router/share/g1_voice_router/local_setup.dsv
[32.008s] -- Symlinking: /home/unitree/unitree_ros2/example/install/g1_voice_router/share/g1_voice_router/package.dsv
[32.025s] -- Up-to-date symlink: /home/unitree/unitree_ros2/example/install/g1_voice_router/share/ament_index/resource_index/packages/g1_voice_router
[32.025s] -- Up-to-date symlink: /home/unitree/unitree_ros2/example/install/g1_voice_router/share/g1_voice_router/cmake/g1_voice_routerConfig.cmake
[32.026s] -- Up-to-date symlink: /home/unitree/unitree_ros2/example/install/g1_voice_router/share/g1_voice_router/cmake/g1_voice_routerConfig-version.cmake
[32.026s] -- Up-to-date symlink: /home/unitree/unitree_ros2/example/install/g1_voice_router/share/g1_voice_router/package.xml
[32.030s] Invoked command in '/home/unitree/unitree_ros2/example/build/g1_voice_router' returned '0': CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}:/opt/ros/humble /usr/bin/cmake --install /home/unitree/unitree_ros2/example/build/g1_voice_router
```

> 注意：安装日志显示 `voice_router` 与 `lab_knowledge.json` 都是 **symlink install**。安装后的路径指向 `build/` 和源码树，详见主报告第 9 节。

### E.1.3 `colcon test` 的 CTest 命令与退出码

`evidence/build_test/test_1333/g1_voice_router/command.log` 全文：

```text
Invoking command in '/home/unitree/unitree_ros2/example/build/g1_voice_router': CMAKE_PREFIX_PATH=/home/unitree/unitree_ros2/example/install/g1_voice_router:/home/unitree/unitree_ros2/example/install/unitree_ros2_example:/home/unitree/unitree_ros2/example/install/g1_asr_bridge:/home/unitree/lidar_ws/install/lidar_check:/home/unitree/lidar_ws/install/g1_sports_show:/home/unitree/lidar_ws/install/g1_action_executor:/home/unitree/unitree_ros2/cyclonedds_ws/install/unitree_hg:/home/unitree/unitree_ros2/cyclonedds_ws/install/unitree_go:/home/unitree/unitree_ros2/cyclonedds_ws/install/unitree_api /usr/bin/ctest -C Release -D ExperimentalTest --no-compress-output -V --force-new-ctest-process
Invoked command in '/home/unitree/unitree_ros2/example/build/g1_voice_router' returned '0': CMAKE_PREFIX_PATH=/home/unitree/unitree_ros2/example/install/g1_voice_router:/home/unitree/unitree_ros2/example/install/unitree_ros2_example:/home/unitree/unitree_ros2/example/install/g1_asr_bridge:/home/unitree/lidar_ws/install/lidar_check:/home/unitree/lidar_ws/install/g1_sports_show:/home/unitree/lidar_ws/install/g1_action_executor:/home/unitree/unitree_ros2/cyclonedds_ws/install/unitree_hg:/home/unitree/unitree_ros2/cyclonedds_ws/install/unitree_go:/home/unitree/unitree_ros2/cyclonedds_ws/install/unitree_api /usr/bin/ctest -C Release -D ExperimentalTest --no-compress-output -V --force-new-ctest-process
```

`evidence/build_test/test_1333/events.log` 末尾：

```text
[0.096055] (g1_voice_router) CommandEnded: {'returncode': 0}
[0.096830] (g1_voice_router) JobEnded: {'identifier': 'g1_voice_router', 'rc': 0}
```

退出码：**0**。

外层 `colcon test` 在会话记录中也可辨认出两次，命令与输出如下：

**（1）13:24 的完整 `console_direct` 测试（会话记录）：**

```text
colcon test --packages-select g1_voice_router --event-handlers console_direct+
```

会话记录中的结果：KnowledgeBase 35 条 PASS、IntentRecognizer 14 条 PASS；随后汇总为：

```text
100% tests passed, 0 tests failed out of 2
Total Test time (real) = 0.02 sec
Finished <<< g1_voice_router [0.11s]
Summary: 1 package finished [0.74s]
--- test result ---
Summary: 2 tests, 0 errors, 0 failures, 0 skipped
```

这次运行的 CTest 目录是 `Testing/20261031-1324/`；当时测试源码尚未扩展到最终的 37 + 25 条。该次输出是**未经过滤的完整控制台输出**。

**（2）13:33 的最终测试（会话记录，输出经过 `grep` 过滤）：**

```text
colcon test --packages-select g1_voice_router --event-handlers console_direct+ 2>&1 | grep -E "Passed:|Failed:|tests passed|Test #"
```

捕获到的输出：

```text
1: Passed: 37
1: Failed: 0
1/2 Test #1: knowledge_base_test ... Passed 0.02 sec
2: Passed: 25
2: Failed: 0
2/2 Test #2: intent_recognizer_test ... Passed 0.00 sec
100% tests passed, 0 tests failed out of 2
```

这次运行的 CTest 目录是 `Testing/20261031-1333/`；由于命令行带 `grep`，外层控制台只保留了上述行，但该次的完整 CTest 原始输出已保存在 `evidence/build_test/LastTest_20261031-1333.log` 与 `test_1333/` 下的 `streams.log`、`stdout.log`、`events.log`。

> 证据边界：两次外层 `colcon test` 的命令与汇总行来自会话记录；包级 CTest 日志是文件证据。13:24 的完整控制台输出为 35+14 条断言，13:33 的过滤输出为 37+25 条断言，两者属于不同时间点，不能混用。

### E.1.4 完整 CTest 输出

文件：`evidence/build_test/LastTest_20261031-1333.log`（3,978 字节），全文如下：

```text
Start testing: Oct 31 13:33 UTC
----------------------------------------------------------
1/2 Testing: knowledge_base_test
1/2 Test: knowledge_base_test
Command: "/home/unitree/unitree_ros2/example/build/g1_voice_router/knowledge_base_test" "/home/unitree/unitree_ros2/example/g1_voice_router/config/lab_knowledge.json"
Directory: /home/unitree/unitree_ros2/example/build/g1_voice_router
"knowledge_base_test" start time: Oct 31 13:33 UTC
Output:
----------------------------------------------------------
========== KnowledgeBase Test ==========
Knowledge file: /home/unitree/unitree_ros2/example/g1_voice_router/config/lab_knowledge.json
[PASS] normal JSON load
[PASS] loaded flag is true
[PASS] FAQ count is 20
[PASS] research direction count is 3
[PASS] project count is 1
[PASS] equipment count is 1
[PASS] introduction query
[PASS] introduction query with greeting prefix
[PASS] FAQ question embedded in a longer sentence
[PASS] research directions query
[PASS] BCI directions FAQ
[PASS] embodied lab research FAQ
[PASS] project query
[PASS] equipment query
[PASS] FAQ exact leader hit
[PASS] Chinese synonym expression
[PASS] SERES co-building FAQ
[PASS] two labs are distinguished
[PASS] location query
[PASS] college teams FAQ
[PASS] college awards FAQ
[PASS] unverified equipment count is not invented
[PASS] unknown team size falls back
[PASS] no-match question falls back
[PASS] empty input falls back
[PASS] answers contain no source URL
[PASS] missing file is rejected
[PASS] query after failed load uses fallback
[PASS] malformed fixture written
[PASS] malformed JSON is rejected
[PASS] unsupported fixture written
[PASS] unsupported schema is rejected
[PASS] empty-fields fixture written
[PASS] empty fields load
[PASS] empty introduction is not spoken
[PASS] empty location is not spoken
[PASS] empty path is rejected
=========================================
Passed: 37
Failed: 0
<end of output>
Test time =   0.02 sec
----------------------------------------------------------
Test Passed.
"knowledge_base_test" end time: Oct 31 13:33 UTC
"knowledge_base_test" time elapsed: 00:00:00
----------------------------------------------------------

2/2 Testing: intent_recognizer_test
2/2 Test: intent_recognizer_test
Command: "/home/unitree/unitree_ros2/example/build/g1_voice_router/intent_recognizer_test"
Directory: /home/unitree/unitree_ros2/example/build/g1_voice_router
"intent_recognizer_test" start time: Oct 31 13:33 UTC
Output:
----------------------------------------------------------
========== Intent Recognizer Test ==========
[PASS] 你好 -> GREETING
[PASS] 您好！ -> GREETING
[PASS] GOOD MORNING -> GENERAL_QA
[PASS] 你好，介绍一下你们实验室 -> LAB_INTRO
[PASS] 你们实验室研究什么？ -> LAB_INTRO
[PASS] 介绍一下你自己 -> ROBOT_INTRO
[PASS] 你能做什么 -> ROBOT_INTRO
[PASS] 给我们展示一下 -> DEMO_ACTION
[PASS] 挥挥手 -> DEMO_ACTION
[PASS] 你现在状态怎么样 -> ROBOT_STATUS
[PASS] 当前运行模式是什么？ -> ROBOT_STATUS
[PASS] 谢谢 -> GOODBYE
[PASS] 再见！ -> GOODBYE
[PASS] 学院有什么在研项目 -> LAB_INTRO
[PASS] 脑机中心负责人是谁 -> LAB_INTRO
[PASS] 学院有哪些科研团队 -> LAB_INTRO
[PASS] 学院学生拿过什么奖 -> LAB_INTRO
[PASS] 学院什么时候成立 -> LAB_INTRO
[PASS] 学院地址在哪里 -> LAB_INTRO
[PASS] 具身智能开放平台是什么 -> LAB_INTRO
[PASS] 肖汉光教授是谁 -> LAB_INTRO
[PASS] 有机器狗吗 -> LAB_INTRO
[PASS] 学院有哪些专业 -> GENERAL_QA
[PASS] 机器狗多少钱 -> GENERAL_QA
[PASS] 宇树机器人多少钱？ -> GENERAL_QA
============================================
Passed: 25
Failed: 0
<end of output>
Test time =   0.00 sec
----------------------------------------------------------
Test Passed.
"intent_recognizer_test" end time: Oct 31 13:33 UTC
"intent_recognizer_test" time elapsed: 00:00:00
----------------------------------------------------------

End testing: Oct 31 13:33 UTC
```

汇总：**2/2 测试通过，37 + 25 条断言全部 PASS，0 FAIL**；CTest 总耗时 0.03 秒。

> CTest 的 `100% tests passed, 0 tests failed out of 2` 与 `Total Test time (real) = 0.03 sec` 汇总行由 CTest 写到 stdout，**不在** `LastTest.log` 内。原文见 `evidence/build_test/test_1333/g1_voice_router/streams.log:101-103` 和 `.../stdout.log:100-102`。

### E.1.5 `colcon test-result --verbose` 输出

实际执行命令（会话记录）：

```text
colcon test-result --verbose; echo "EXIT=$?"
```

会话中捕获的 stdout：

```text
Summary: 2 tests, 0 errors, 0 failures, 0 skipped
EXIT=0
```

退出码：**0**。

> 为什么没有逐条测试列表：`evidence/test_result_1343/logger_all.log` 显示，colcon 的 xunit 解析器跳过了所有 `Test.xml`，原因是根标签既不是 `testsuite` 也不是 `testsuites`：
>
> ```text
> DEBUG:colcon.colcon_test_result.test_result.xunit:Skipping 'build/g1_voice_router/Testing/20261031-1324/Test.xml': the root tag is neither 'testsuite' nor 'testsuites'
> DEBUG:colcon.colcon_test_result.test_result.xunit:Skipping 'build/g1_voice_router/Testing/20261031-1326/Test.xml': the root tag is neither 'testsuite' nor 'testsuites'
> DEBUG:colcon.colcon_test_result.test_result.xunit:Skipping 'build/g1_voice_router/Testing/20261031-1330/Test.xml': the root tag is neither 'testsuite' nor 'testsuites'
> DEBUG:colcon.colcon_test_result.test_result.xunit:Skipping 'build/g1_voice_router/Testing/20261031-1333/Test.xml': the root tag is neither 'testsuite' nor 'testsuites'
> ```
>
> 因此逐条 PASS 证据以 `LastTest_20261031-1333.log` 和 `test_1333/events.log` 为准；`test-result` 的汇总数字与 CTest 输出一致（2 tests, 0 failures）。

---

## E.2 KnowledgeBase 单元测试：37 条逐项结果

测试文件：`test/knowledge_base_test.cpp`（SHA256 `dab2aef7156ff2cf3762f98a9d1681f38200f140ebb865482d4749c3c9c742fa`）。

测试可执行文件：

```text
/home/unitree/unitree_ros2/example/build/g1_voice_router/knowledge_base_test
```

CTest 传入的参数是**源码树中的 JSON**：

```text
/home/unitree/unitree_ros2/example/g1_voice_router/config/lab_knowledge.json
```

该文件与运行时安装路径指向同一文件（symlink），SHA256 为 `cbe711e3b5c19115dc9d85fe37c701829ee3ad37203a3a4337efa5263d5acea7`。

| # | 输入 | 期望结果（测试断言） | 实际结果 | 判定 |
|---:|---|---|---|---|
| 1 | JSON 路径 `.../config/lab_knowledge.json` | `load()` 返回 `true`，无错误 | PASS，加载成功 | PASS |
| 2 | 加载完成后查询 `loaded()` | `loaded()` 为 `true` | PASS，断言成立 | PASS |
| 3 | `faq_count()` | 等于 20 | PASS，计数为 20 | PASS |
| 4 | `direction_count()` | 等于 3 | PASS，计数为 3 | PASS |
| 5 | `project_count()` | 等于 1 | PASS，计数为 1 | PASS |
| 6 | `equipment_count()` | 等于 1 | PASS，计数为 1 | PASS |
| 7 | `介绍一下实验室` | `found=true`，`category=introduction`，答案含“脑机交互智能研究中心” | PASS，断言成立 | PASS |
| 8 | `你好，介绍一下你们实验室` | `found=true`，`category=introduction`，答案含“脑机交互智能研究中心” | PASS，断言成立（带问候前缀仍命中） | PASS |
| 9 | `请问重点实验室研究什么` | `found=true`，答案含“多模态融合学习” | PASS，断言成立（FAQ 问题被包含在长句中） | PASS |
| 10 | `你们实验室主要研究什么` | `found=true`，`category=research_directions`，答案含“神经信号感知”“多模态融合学习” | PASS，断言成立 | PASS |
| 11 | `脑机中心有哪些研究方向` | `found=true`，答案含“类脑智能系统” | PASS，断言成立 | PASS |
| 12 | `重点实验室研究什么` | `found=true`，答案含“多模态融合学习”“域泛化” | PASS，断言成立 | PASS |
| 13 | `学院有什么在研项目` | `found=true`，`category=projects`，答案含“500万” | PASS，断言成立 | PASS |
| 14 | `实验室有哪些设备` | `found=true`，`category=equipment`，答案含“智能机器狗” | PASS，断言成立 | PASS |
| 15 | `脑机中心负责人是谁` | `found=true`，答案含“蒋田仔” | PASS，断言成立（FAQ 精确命中） | PASS |
| 16 | `脑机中心是干什么的` | `found=true`，答案含“脑机交互智能研究中心” | PASS，断言成立（中文同义表达） | PASS |
| 17 | `重点实验室和赛力斯是什么关系` | `found=true`，答案含“共建单位” | PASS，断言成立 | PASS |
| 18 | `脑机中心和重点实验室是一回事吗` | `found=true`，答案含“不是同一个名称” | PASS，断言成立 | PASS |
| 19 | `实验室在哪里` | `found=true`，答案含“普福大道” | PASS，断言成立 | PASS |
| 20 | `学院有哪些科研团队` | `found=true`，答案含“机器博弈” | PASS，断言成立 | PASS |
| 21 | `学院学生拿过什么奖` | `found=true`，答案含“15项” | PASS，断言成立 | PASS |
| 22 | `实验室有几台机器人` | `found=true`，答案中**不得**出现“阿拉伯数字 + 台”，且不得出现“G1” | PASS，断言成立（未编造数量/型号） | PASS |
| 23 | `实验室有多少人` | `found=false`，`category=fallback`，答案非空 | PASS，断言成立 | PASS |
| 24 | `今天天气怎么样` | `found=false`，`category=fallback`，答案非空 | PASS，断言成立 | PASS |
| 25 | 空字符串 `""` | `found=false`，`category=fallback`，答案非空 | PASS，断言成立 | PASS |
| 26 | 依次查询 6 条：`介绍一下实验室`、`你们实验室主要研究什么`、`脑机中心负责人是谁`、`学院有什么在研项目`、`实验室有哪些设备`、`实验室在哪里` | 所有答案均不含 `http://`、`https://`、`www.` | PASS，6 条均无 URL | PASS |
| 27 | 不存在的临时路径 `/tmp/g1_kb_test_<pid>_missing.json` | `load()` 返回 `false`，`loaded()` 为 `false` | PASS，断言成立 | PASS |
| 28 | 加载失败后再查 `介绍一下实验室` | `found=false` | PASS，断言成立 | PASS |
| 29 | 写入畸形文件 `{ this is not json` | `write_file()` 返回 `true` | PASS，临时文件写入成功 | PASS |
| 30 | 加载畸形 JSON | `load()` 返回 `false` | PASS，断言成立 | PASS |
| 31 | 写入 `{"schema_version": 2}` | `write_file()` 返回 `true` | PASS，临时文件写入成功 | PASS |
| 32 | 加载不支持的 schema 版本 | `load()` 返回 `false` | PASS，断言成立 | PASS |
| 33 | 写入“空字段”最小 JSON（空 `location`、空 `introduction`、空数组、`fallback_answer="测试兜底回答。"`） | `write_file()` 返回 `true` | PASS，临时文件写入成功 | PASS |
| 34 | 加载“空字段”最小 JSON | `load()` 返回 `true` | PASS，断言成立 | PASS |
| 35 | 空字段库中查询 `介绍一下实验室` | `found=false`，答案严格等于“测试兜底回答。” | PASS，断言成立（空介绍不播报） | PASS |
| 36 | 空字段库中查询 `实验室在哪里` | `found=false`，答案严格等于“测试兜底回答。” | PASS，断言成立（空地址不播报） | PASS |
| 37 | 空路径 `load("", error)` | `load()` 返回 `false` | PASS，断言成立 | PASS |

**统计：37 PASS / 0 FAIL。**

> 关于第 7–21 条：CTest 只记录 `[PASS]`，没有记录答案原文。断言要求答案包含指定文本，PASS 表示包含关系成立；答案全文见附录 B（`KNOWLEDGE_BASE_APPENDIX.md`）。
>
> 关于第 22 条：测试检测“阿拉伯数字 + 台”以及“G1”，PASS 说明当前设备答案为“公开资料只披露载体类型，具体品牌、型号和数量没有公开”，没有编造“3 台”“G1”等未核实信息。

---

## E.3 IntentRecognizer 单元测试：25 条逐项结果

测试文件：`test/intent_recognizer_test.cpp`（SHA256 `cd80b15f4542412051133e30a9a7faefc55eca7dbb20fe130778efd9c3abb3d7`）。

测试可执行文件：

```text
/home/unitree/unitree_ros2/example/build/g1_voice_router/intent_recognizer_test
```

| # | 输入 | 期望意图 | 实际意图（CTest 输出） | 判定 |
|---:|---|---|---|---|
| 1 | `你好` | GREETING | GREETING | PASS |
| 2 | `您好！` | GREETING | GREETING | PASS |
| 3 | `GOOD MORNING` | GENERAL_QA | GENERAL_QA | PASS |
| 4 | `你好，介绍一下你们实验室` | LAB_INTRO | LAB_INTRO | PASS |
| 5 | `你们实验室研究什么？` | LAB_INTRO | LAB_INTRO | PASS |
| 6 | `介绍一下你自己` | ROBOT_INTRO | ROBOT_INTRO | PASS |
| 7 | `你能做什么` | ROBOT_INTRO | ROBOT_INTRO | PASS |
| 8 | `给我们展示一下` | DEMO_ACTION | DEMO_ACTION | PASS |
| 9 | `挥挥手` | DEMO_ACTION | DEMO_ACTION | PASS |
| 10 | `你现在状态怎么样` | ROBOT_STATUS | ROBOT_STATUS | PASS |
| 11 | `当前运行模式是什么？` | ROBOT_STATUS | ROBOT_STATUS | PASS |
| 12 | `谢谢` | GOODBYE | GOODBYE | PASS |
| 13 | `再见！` | GOODBYE | GOODBYE | PASS |
| 14 | `学院有什么在研项目` | LAB_INTRO | LAB_INTRO | PASS |
| 15 | `脑机中心负责人是谁` | LAB_INTRO | LAB_INTRO | PASS |
| 16 | `学院有哪些科研团队` | LAB_INTRO | LAB_INTRO | PASS |
| 17 | `学院学生拿过什么奖` | LAB_INTRO | LAB_INTRO | PASS |
| 18 | `学院什么时候成立` | LAB_INTRO | LAB_INTRO | PASS |
| 19 | `学院地址在哪里` | LAB_INTRO | LAB_INTRO | PASS |
| 20 | `具身智能开放平台是什么` | LAB_INTRO | LAB_INTRO | PASS |
| 21 | `肖汉光教授是谁` | LAB_INTRO | LAB_INTRO | PASS |
| 22 | `有机器狗吗` | LAB_INTRO | LAB_INTRO | PASS |
| 23 | `学院有哪些专业` | GENERAL_QA | GENERAL_QA | PASS |
| 24 | `机器狗多少钱` | GENERAL_QA | GENERAL_QA | PASS |
| 25 | `宇树机器人多少钱？` | GENERAL_QA | GENERAL_QA | PASS |

**统计：25 PASS / 0 FAIL。**

> 这 25 条验证的是“意图分类”，不是“知识库回答正确”。第 14–22 条只是说明会进入 `LAB_INTRO` 本地分支；是否命中正确 FAQ、答案是否合理，由 E.2 的知识库测试和 E.4 的运行时测试分别验证。
>
> 第 8、9 条只验证 `DEMO_ACTION` 分类；当前 Voice Router **没有** `DEMO_ACTION` 本地处理分支，这些输入在真实链路中仍会进入 Ollama 队列。详见主报告第 10 节。

---

## E.4 默认域运行时测试

### E.4.1 测试方法与边界

- 在 G1 默认域（`ROS_DOMAIN_ID` 未设置，默认 0）启动**临时** Voice Router 实例；测试结束后发送 `SIGINT` 停止。生产进程 `PID 10751/10753` 的启动时间是 13:33:06/07，最终生产日志与临时测试日志分开保存。
- 测试方式：向 `/g1/asr_text` 发布 `std_msgs/msg/String`；用 `ros2 topic echo /g1/tts` 捕获输出；从 Voice Router 日志读取意图、知识库命中、发布字节数和是否入 Ollama 队列。
- **没有启动** `g1_tts_node`、`g1_action_executor_node`、`g1_asr_bridge`。因此：
  - 捕获到的是 `/g1/tts` 消息，不是扬声器实际播放；
  - `/g1/action_request` 没有订阅者，即使发出 `wave` 也不会执行；
  - 没有真实麦克风/ASR 输入。
- 默认域运行时测试分 4 组：runtime1（修复前）、runtime2（初次修复后）、runtime3（意图关键词扩展后）、最终生产进程。
- 各组之间临时 Router 均被 `SIGINT` 停止并重新启动，日志首行均重新打印 `KnowledgeBase loaded`。

### E.4.2 runtime1：修复前的兜底行为

日志：`evidence/runtime/g1_voice_router_kb_runtime.log`（984 字节）。

启动：`KnowledgeBase loaded: .../install/g1_voice_router/share/g1_voice_router/config/lab_knowledge.json (FAQ=20, directions=3, projects=1, equipment=1)`。

| # | 输入 | 识别意图 | 知识库结果 | `/g1/tts` 字节 | 实际捕获内容 | 进入 Ollama 队列 | 日志证据 |
|---:|---|---|---|---:|---|---|---|
| 1 | `你好介绍一下你们实验室` | LAB_INTRO | `found=false`，`category=fallback`，`matched_id=` | 162 | `这个信息目前没有公开资料，我不能确认。您可以问我实验室的研究方向、重点实验室、具身智能开放平台或者学院地址。`（`tts_lab.txt`，173 字节含 `data: ` 与 `---`） | 否 | `1793453122.707` 识别；`1793453122.707969` 发布 162 字节；`1793453122.708045` `LAB_INTRO local knowledge base completed: found=false category=fallback` |

**结论：** 修复前，`你好介绍一下你们实验室` 没有命中 FAQ，返回 162 字节兜底。修复后同一输入在 runtime2 命中 `faq_intro`（412 字节）。从当前 JSON 可见 `faq_intro.questions` 已包含 `介绍一下你们实验室`；具体编辑过程不属于本轮只读审查范围。**当前最终生产版本没有重新测试这一条原始输入。**

### E.4.3 runtime2：初次修复后的 8 条测试

日志：`evidence/runtime/g1_voice_router_kb_runtime2.log`（3,347 字节）。

| # | 输入 | 识别意图 | 知识库结果 | `/g1/tts` 字节 | 实际捕获内容 | 进入 Ollama 队列 | 日志证据（UTC 时间戳） |
|---:|---|---|---|---:|---|---|---|
| 1 | `你好介绍一下你们实验室` | LAB_INTRO | `found=true`，`category=introduction`，`matched_id=faq_intro` | 412 | `tts_lab1.txt` 仅捕获前 372 字节，末尾截断在“由重”；完整 412 字节答案见 JSON `faq_intro`。**不得把截断文件当作完整播报证据。** | 否 | `1793453212.632` 识别；`.633819` 发布 412 字节；`.633906` `found=true category=introduction matched_id=faq_intro` |
| 2 | `你们实验室主要研究什么` | LAB_INTRO | `found=true`，`category=research_directions`，`matched_id=research_directions` | 348 | `tts_lab2.txt` 全文：`公开资料中，脑机交互智能研究中心有五个方向：神经信号感知、脑网络组应用、脑病智能诊疗、脑机自然交互、类脑智能系统。市级重点实验室研究复杂场景下的抗干扰多模态融合学习、人机环境交互的域泛化和抗知识遗忘持续学习，以及技能可迁移自主学习。` | 否 | `1793453223.798` 识别；`.799418` 发布 348 字节；`.799495` `found=true category=research_directions` |
| 3 | `实验室有多少人` | LAB_INTRO | `found=false`，`category=fallback` | 162 | `tts_lab3.txt` 全文为 162 字节兜底（同 runtime1 第 1 条） | 否 | `1793453235.351` 识别；`.352582` 发布 162 字节；`.352677` `found=false category=fallback` |
| 4 | `介绍一下你自己` | ROBOT_INTRO | 不查询知识库 | 245 | `tts_robot.txt` 全文：`我是宇树 G1 EDU 人形机器人。我可以通过语音识别接收你的问题，连接外部大语言模型进行中文对话，还可以执行挥手、举手和比心等预设手臂动作。很高兴为你介绍我们的机器人系统。` | 否 | `1793453247.008` 识别；`.008708` 发布 245 字节；`.008785` `ROBOT_INTRO local handling completed` |
| 5 | `谢谢` | GOODBYE | 不查询知识库 | 75 | `tts_bye.txt` 全文：`感谢你的参观！很高兴和你交流，祝你今天愉快，再见！` | 否 | `1793453251.758` 识别；`.759142` 发布 75 字节；`.759215` `GOODBYE local handling completed` |
| 6 | `宇树机器人多少钱` | GENERAL_QA | 不查询知识库 | 166 | `tts_general.txt` 全文：`宇树G1人形机器人目前官方售价为19999元人民币。具体价格可能因地区或促销活动略有不同，建议访问官方渠道获取最新信息。` | **是** | `1793453264.173` 识别；`.173893` `ASR text queued for Ollama; pending=1`；`.174011` `Querying Ollama model`；`1793453267.885` 发布 166 字节（约 3.71 秒） |
| 7 | `学院有什么在研项目` | GENERAL_QA（此版本意图关键词尚未扩展） | 不查询知识库 | 159 | `tts_proj.txt` 全文：`我目前没有接入学院项目数据，无法提供在研项目信息。建议您联系学院科研办公室或访问学校官网获取最新项目列表。` | **是** | `1793453333.184` 识别；`.184929` 入队 `pending=1`；`.185016` 查询 Ollama；`1793453333.649` 发布 159 字节（约 0.46 秒） |
| 8 | `实验室有哪些设备` | LAB_INTRO | `found=true`，`category=equipment`，`matched_id=faq_equipment` | 180 | `tts_equip.txt` 全文：`公开资料只披露，具身智能开放平台使用智能机器人、特种无人车、智能机器狗等载体。具体品牌、型号和数量没有公开，我不能确认。` | 否 | `1793453338.380` 识别；`.381392` 发布 180 字节；`.381486` `found=true category=equipment matched_id=faq_equipment` |

**关键观察：**

- 第 1 条修复后命中 `faq_intro`；第 3 条“实验室有多少人”仍按设计返回兜底，没有编造人数。
- 第 6 条是 Ollama 回答，出现 **“官方售价 19999 元”**；该数字与调研资料中宇树官方“¥9.9 万元起”的公开信息冲突，属于 **Ollama 未接地（ungrounded）风险**，不是知识库答案。详见主报告第 10 节。
- 第 7 条证明：在扩展 `LAB_INTRO` 关键词之前，“学院有什么在研项目”会进入 Ollama，并且 Ollama 回答“没有接入学院项目数据”。runtime3 中同一输入已改为本地知识库回答。

### E.4.4 runtime3：扩展意图关键词后的 8 条测试

日志：`evidence/runtime/g1_voice_router_kb_runtime3.log`（3,422 字节）。

| # | 输入 | 识别意图 | 知识库结果 | `/g1/tts` 字节 | 实际捕获内容 | 进入 Ollama 队列 | 日志证据（UTC 时间戳） |
|---:|---|---|---|---:|---|---|---|
| 1 | `学院有什么在研项目` | LAB_INTRO | `found=true`，`category=projects`，`matched_id=faq_project` | 319 | `tts_final.txt` 内容为 319 字节项目答案（330 字节含 `data: ` 与 `---`）；但该文件无时间戳，runtime3 第 1 条与最终生产第 1 条都可能产生同样内容，**不能唯一归属**。答案全文见 JSON `faq_project`。 | 否 | `1793453424.888` 识别；`.888818` 发布 319 字节；`.888903` `found=true category=projects matched_id=faq_project` |
| 2 | `脑机中心负责人是谁` | LAB_INTRO | `found=true`，`category=people`，`matched_id=faq_bci_leaders` | 168 | 未单独留存逐字捕获；日志字节数 168 与 JSON `faq_bci_leaders` 答案长度一致。答案全文见附录 B。 | 否 | `1793453429.651` 识别；`.651565` 发布 168 字节；`.651657` `found=true category=people matched_id=faq_bci_leaders` |
| 3 | `学院有哪些科研团队` | LAB_INTRO | `found=true`，`category=college`，`matched_id=faq_teams` | 192 | 未单独留存逐字捕获；日志字节数 192 与 JSON `faq_teams` 长度一致。 | 否 | `1793453434.399` 识别；`.400357` 发布 192 字节；`.400448` `found=true category=college matched_id=faq_teams` |
| 4 | `学院学生拿过什么奖` | LAB_INTRO | `found=true`，`category=college`，`matched_id=faq_awards` | 181 | 未单独留存逐字捕获；日志字节数 181 与 JSON `faq_awards` 长度一致。 | 否 | `1793453439.159` 识别；`.159885` 发布 181 字节；`.159970` `found=true category=college matched_id=faq_awards` |
| 5 | `学院地址在哪里` | LAB_INTRO | `found=true`，`category=location`，`matched_id=faq_location` | 162 | 未单独留存逐字捕获；日志字节数 162 与 JSON `faq_location` 长度一致。 | 否 | `1793453443.915` 识别；`.916773` 发布 162 字节；`.916851` `found=true category=location matched_id=faq_location` |
| 6 | `具身智能开放平台是什么` | LAB_INTRO | `found=true`，`category=labs`，`matched_id=faq_platform` | 216 | 未单独留存逐字捕获；日志字节数 216 与 JSON `faq_platform` 长度一致。 | 否 | `1793453448.668` 识别；`.669260` 发布 216 字节；`.669427` `found=true category=labs matched_id=faq_platform` |
| 7 | `肖汉光教授是谁` | LAB_INTRO | `found=true`，`category=people`，`matched_id=faq_xiao` | 303 | 未单独留存逐字捕获；日志字节数 303 与 JSON `faq_xiao` 长度一致。 | 否 | `1793453453.496` 识别；`.497189` 发布 303 字节；`.497275` `found=true category=people matched_id=faq_xiao` |
| 8 | `宇树机器人多少钱` | GENERAL_QA | 不查询知识库 | 166 | `tts_one.txt` 全文：`宇树G1目前官方售价为19999元人民币。具体价格可能因地区或促销活动略有不同，建议访问官方商城或线下门店获取最新信息。` | **是** | `1793453458.328` 识别；`.328415` 入队 `pending=1`；`.328529` 查询 Ollama；`1793453458.991` 发布 166 字节（约 0.66 秒） |

**关键观察：**

- 第 1 条与 runtime2 第 7 条是同一输入，修复后从“Ollama 说没有项目数据”变为“本地知识库 319 字节项目答案”，这是本轮意图扩展的直接效果。
- 第 2–7 条均未入 Ollama 队列，日志只有 `LAB_INTRO local knowledge base completed`，没有 `ASR text queued for Ollama`。
- 第 8 条仍是 Ollama 回答，仍出现 19999 元，与公开来源冲突；说明 **GENERAL_QA 的未接地问题没有因知识库接入而消失**。

### E.4.5 最终生产进程测试

日志：`evidence/runtime/g1_voice_router_kb_final.log`（917 字节）。

生产进程：

```text
10751  1  Sat Oct 31 13:33:06 2026  /usr/bin/python3 /opt/ros/humble/bin/ros2 run g1_voice_router voice_router
10753  10751  Sat Oct 31 13:33:07 2026  /home/unitree/unitree_ros2/example/install/g1_voice_router/lib/g1_voice_router/voice_router
```

| # | 输入 | 识别意图 | 知识库结果 | `/g1/tts` 字节 | 实际捕获内容 | 进入 Ollama 队列 | 日志证据（UTC 时间戳） |
|---:|---|---|---|---:|---|---|---|
| 1 | `学院有什么在研项目` | LAB_INTRO | `found=true`，`category=projects`，`matched_id=faq_project` | 319 | `tts_final.txt` 与 319 字节项目答案一致；文件无时间戳，不能唯一确认属于本次测试。 | 否 | `1793453604.932` 识别；`.932590` 发布 319 字节；`.932664` `found=true category=projects matched_id=faq_project` |

**结论：** 最终生产进程加载知识库成功（`FAQ=20, directions=3, projects=1, equipment=1`），并对项目问题走本地知识库。**最终生产进程只做了这 1 条注入测试**；没有在生产进程上重新跑完整 8 条，也没有做真实 ASR/TTS 端到端测试。

### E.4.6 `/g1/tts` 捕获文件清单

| 文件 | 字节 | 内容摘要 | 对应测试 | 完整性 |
|---|---:|---|---|---|
| `tts_lab.txt` | 173 | 162 字节兜底回答 | runtime1 #1 | 完整（含 `data: ` 与 `---`） |
| `tts_lab1.txt` | 372 | `faq_intro` 开头，截断在“由重” | runtime2 #1 | **截断**；日志记录完整为 412 字节 |
| `tts_lab2.txt` | 359 | 348 字节研究方向回答 | runtime2 #2 | 完整（348 + 11） |
| `tts_lab3.txt` | 173 | 162 字节兜底回答 | runtime2 #3 | 完整 |
| `tts_robot.txt` | 256 | 245 字节机器人自我介绍 | runtime2 #4 | 完整 |
| `tts_bye.txt` | 86 | 75 字节告别语 | runtime2 #5 | 完整 |
| `tts_general.txt` | 177 | 166 字节 Ollama 价格回答（19999 元） | runtime2 #6 | 完整 |
| `tts_proj.txt` | 170 | 159 字节 Ollama“没有项目数据”回答 | runtime2 #7 | 完整 |
| `tts_equip.txt` | 191 | 180 字节设备回答 | runtime2 #8 | 完整 |
| `tts_final.txt` | 330 | 319 字节项目回答 | runtime3 #1 或最终生产 #1 | 完整（319 + 11），归属不唯一 |
| `tts_one.txt` | 177 | 166 字节 Ollama 价格回答（19999 元） | runtime3 #8 | 完整 |

> `data: ` 前缀和 `---` 分隔符是 `ros2 topic echo` 的输出格式，不计入消息字节数。

---

## E.5 隔离 ROS 域测试（域 42 / 43）

### E.5.1 目的与方法

- 使用独立 `ROS_DOMAIN_ID`，避免影响默认域生产进程。
- 启动临时 Voice Router，分别验证：
  - 域 42：知识库文件不存在时，节点是否仍启动、`LAB_INTRO` 是否安全兜底；
  - 域 43：`knowledge_file` 参数覆盖是否生效，是否能从另一路径加载知识库。
- 两个域都未启动 TTS/动作/ASR 节点；`/g1/tts` 由 `ros2 topic echo` 捕获。
- **证据来源仅为会话记录。** 测试结束后 `/tmp` 下的临时日志已被删除，本轮无法在机器人上重新读取原始文件。

### E.5.2 域 42：缺失知识库文件

会话记录的关键输出（原文照录）：

```text
[ERROR] [g1_voice_router]: KnowledgeBase load failed: cannot open knowledge file: /tmp/definitely_missing_kb.json
[INFO] [g1_voice_router]: G1 voice router started
...
[INFO] [g1_voice_router]: Intent recognized: LAB_INTRO | text: 实验室有多少人
[INFO] [g1_voice_router]: Reply published to /g1/tts (57 bytes)
[INFO] [g1_voice_router]: LAB_INTRO local knowledge base completed: found=false category=fallback matched_id=
```

`/g1/tts` 捕获：

```text
data: 这个信息目前没有公开资料，我不能确认。
---
```

| 项目 | 结果 |
|---|---|
| 启动参数 | `knowledge_file:=/tmp/definitely_missing_kb.json`，`ROS_DOMAIN_ID=42` |
| 知识库加载 | 失败；日志为 `cannot open knowledge file` |
| 节点是否继续运行 | 是；仍打印 `G1 voice router started` |
| 输入 | `实验室有多少人` |
| 识别意图 | LAB_INTRO |
| 知识库结果 | `found=false`，`category=fallback`，`matched_id=` |
| 发布字节 | 57 |
| 实际内容 | `这个信息目前没有公开资料，我不能确认。` |
| 是否入 Ollama | 否 |
| 证据级别 | 会话记录；原 `/tmp` 日志已删除，无法复核 |

**结论：** 知识库加载失败不会阻止节点启动；`LAB_INTRO` 会退化为 **57 字节硬编码兜底**，而不是配置中的 162 字节 `fallback_answer`，因为加载失败时 `data_` 被清空。这个行为已在主报告第 10 节列为风险。

### E.5.3 域 43：`knowledge_file` 参数覆盖

会话记录的关键输出（原文照录）：

```text
[INFO] [g1_voice_router]: KnowledgeBase loaded: /tmp/lab_knowledge_override.json (FAQ=20, directions=3, projects=1, equipment=1)
[INFO] [g1_voice_router]: G1 voice router started
...
[INFO] [g1_voice_router]: Intent recognized: LAB_INTRO | text: 实验室在哪里
[INFO] [g1_voice_router]: Reply published to /g1/tts (162 bytes)
[INFO] [g1_voice_router]: LAB_INTRO local knowledge base completed: found=true category=location matched_id=faq_location
```

| 项目 | 结果 |
|---|---|
| 启动参数 | `knowledge_file:=/tmp/lab_knowledge_override.json`，`ROS_DOMAIN_ID=43` |
| 知识库加载 | 成功；`FAQ=20, directions=3, projects=1, equipment=1` |
| 输入 | `实验室在哪里` |
| 识别意图 | LAB_INTRO |
| 知识库结果 | `found=true`，`category=location`，`matched_id=faq_location` |
| 发布字节 | 162 |
| 实际内容 | 162 字节地址回答；与 JSON `faq_location` 长度一致（未逐字捕获） |
| 是否入 Ollama | 否 |
| 证据级别 | 会话记录；原 `/tmp` 日志已删除，无法复核 |

**结论：** `knowledge_file` 参数覆盖生效；节点可以从非默认路径加载知识库。该测试验证的是参数机制，不是生产路径的永久配置。

---

## E.6 未测试 / 不能声称已验证的项目

以下项目本轮**没有执行**，不得在审查结论中写成“已验证”：

| 项目 | 状态 | 原因 / 证据 |
|---|---|---|
| 真实麦克风 → ASR 桥接 → `/g1/asr_text` | **未测试** | `/g1/asr_text` 的 publisher count 为 0；`g1_asr_bridge` 未运行 |
| 真实 `g1_tts_node` → 扬声器播放 | **未测试** | TTS 节点未运行；`/g1/tts` 只有遗留的 `ros2 topic echo` 订阅者（PID 4216） |
| 动作执行器接收 `wave` 并执行 | **未测试** | `g1_action_executor_node` 未运行；`/g1/action_request` 订阅者为 0；本轮未发送动作请求 |
| GREETING 触发 `wave` 的真实动作链路 | **未测试** | 未发送问候语到生产链路；也没有动作执行器订阅 |
| `DEMO_ACTION` 本地动作处理 | **未实现/未测试** | Voice Router 没有 `DEMO_ACTION` 分支；该意图会进入 Ollama。单测只验证分类 |
| `ROBOT_STATUS` 本地状态回答 | **未实现/未测试** | Voice Router 没有 `ROBOT_STATUS` 分支；该意图会进入 Ollama。单测只验证分类 |
| 20 条 FAQ 的逐条运行时覆盖 | **未完全覆盖** | 运行时命中 9 个 FAQ ID：`faq_intro`、`faq_equipment`、`faq_project`、`faq_bci_leaders`、`faq_teams`、`faq_awards`、`faq_location`、`faq_platform`、`faq_xiao`。其余 11 个 FAQ ID 只在单元测试或静态检查中覆盖，未逐条走 ROS 运行时 |
| 多轮对话上下文 | **未测试** | 没有连续多轮 GENERAL_QA 会话测试 |
| 队列积压 / 连续多条 ASR | **未测试** | 没有做并发或积压压力测试 |
| Ollama 请求进行中按 Ctrl-C 退出 | **未测试** | 日志中的 SIGINT 都发生在空闲时；没有在 60 秒请求中测试 |
| 生产进程上的知识库加载失败 | **未测试** | 只在域 42 隔离环境测试；没有在生产进程上制造加载失败 |
| 生产进程上的 `knowledge_file` 覆盖 | **未测试** | 只在域 43 隔离环境测试 |
| 中文 ASR 错字 / 同音词鲁棒性 | **未测试** | 输入均为文本注入，不是真实 ASR 输出 |
| 真实机器人移动/安全联锁 | **未测试** | 本轮不执行动作，不涉及运动控制 |

---

## E.7 证据来源与可复核性

| 证据 | 本地副本 | 机器人原始路径（若已知） | 可复核性 |
|---|---|---|---|
| 构建输出 | `evidence/build_test/build_1332/g1_voice_router/streams.log` | `~/unitree_ros2/example/log/build_2026-10-31_13-32-*/g1_voice_router/streams.log`（按 colcon 日志布局推断） | 本地副本可读；原始日志未在本轮重新访问 |
| 构建命令/退出码 | `evidence/build_test/latest_build/command.log` | 同上 | 本地副本可读 |
| CTest 完整输出 | `evidence/build_test/LastTest_20261031-1333.log` | `~/unitree_ros2/example/build/g1_voice_router/Testing/20261031-1333/LastTest.log` | 本地副本可读 |
| CTest 事件/退出码 | `evidence/build_test/test_1333/events.log` | `~/unitree_ros2/example/log/test_2026-10-31_13-33-*/events.log`（按布局推断） | 本地副本可读 |
| CTest 命令日志 | `evidence/build_test/test_1333/g1_voice_router/command.log` | 同上 | 本地副本可读 |
| `colcon test-result` logger | `evidence/test_result_1343/logger_all.log` | `~/unitree_ros2/example/log/latest_test_result/logger_all.log`（按布局推断） | 本地副本可读；命令 stdout 仅有会话记录 |
| runtime1 日志 | `evidence/runtime/g1_voice_router_kb_runtime.log` | 临时启动日志，原路径未保留 | 本地副本可读 |
| runtime2 日志 | `evidence/runtime/g1_voice_router_kb_runtime2.log` | 临时启动日志，原路径未保留 | 本地副本可读 |
| runtime3 日志 | `evidence/runtime/g1_voice_router_kb_runtime3.log` | 临时启动日志，原路径未保留 | 本地副本可读 |
| 最终生产日志 | `evidence/runtime/g1_voice_router_kb_final.log` | 生产进程 stdout 捕获，原路径未保留 | 本地副本可读 |
| `/g1/tts` 捕获 | `evidence/runtime/tts_*.txt` | `ros2 topic echo` 输出重定向 | 本地副本可读；部分截断/归属不唯一 |
| 部署环境 | `evidence/deploy/host_env.txt` 等 | 本轮 SSH 只读命令输出 | 本地副本可读；快照时间 2026-10-31 13:43–13:44 UTC |
| 域 42/43 输出 | 无独立文件 | 原 `/tmp` 日志已删除 | **仅会话记录，不可复核** |

---

## E.8 测试覆盖结论

1. **单元层面证据充分。** 37 条知识库断言 + 25 条意图断言全部通过，覆盖加载校验、查询顺序、兜底、空字段、无 URL、缺失/畸形/不支持 schema、意图关键词扩展。
2. **ROS 运行时层面证据有限但真实。** 默认域共 18 条注入（runtime1 1 条、runtime2 8 条、runtime3 8 条、最终生产 1 条），另有域 42/43 各 1 条。捕获的是 `/g1/tts` 消息，不是扬声器播放。
3. **端到端未验证。** 没有真实 ASR、TTS 音频、动作执行器；因此不能对外声称“机器人已经能听懂并说出知识库答案”。可以声称的是：**Voice Router 在收到指定文本后，能够正确路由并发布知识库答案消息**。
4. **知识库答案的“正确性”只经过单元断言与人工调研核验，没有经过运行时逐条人工听测。** 20 条 FAQ 中只有 9 条在运行时命中过。
5. **已发现的运行时问题：**
   - `你好介绍一下你们实验室` 在修复前兜底，修复后命中；但最终生产版本未重测该原始输入。
   - `实验室有多少人` 按设计兜底，未编造人数。
   - GENERAL_QA 的 Ollama 价格回答出现 19999 元，与公开来源“¥9.9 万元起”冲突；这是未接地风险，不是知识库缺陷。
   - `学院有什么在研项目` 在意图扩展前进入 Ollama 并回答“没有项目数据”，扩展后本地命中；这是修复有效的直接证据。

---

*本附录与 `SOURCE_APPENDIX.md`、`KNOWLEDGE_BASE_APPENDIX.md`、`CODE_DIFF_APPENDIX.md`、`RESEARCH_APPENDIX.md` 共同构成审查包。所有测试结论以本附录列出的原始输出为准；没有重新编造任何测试结果。*
