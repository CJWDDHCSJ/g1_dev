# REVIEW_REPORT · 第二轮技术审计报告

> 审计对象：本目录（`docs/unitree_sdk/`）19 个 Markdown 文档。
> 审计日期：2026-10-24。SDK checkout：`unitree_sdk2` git `2.0.2-75-gd1abf00`（HEAD `d1abf00`，CMake project 版本 2.0.0）。
> 本报告只记录**第二轮审计**发现与修正；第一轮产出的文档结构不在本报告评价范围内。
> 本轮遵守只读约束：未修改 SDK 源码、未安装软件、未执行任何机器人操作。

---

## 1. 审计范围与方法

### 1.1 覆盖文件

19 个 Markdown，共 7688 行：`README`、`00`–`13`、`api_index`、`example_to_api`、`known_limitations`、`g1_deployment_priority`。

### 1.2 核对方法

| 方法 | 说明 |
|---|---|
| 逐头文件比对 | G1 相关头文件（`g1_loco_client.hpp`、`g1_arm_action_client.hpp`、`g1_audio_client.hpp`、`g1_agv_client.hpp`、`motion_switcher_client.hpp`、channel/dds 核心、`dds_wrapper`）与文档签名/行号逐一核对 |
| 示例源码比对 | 15 个 G1 `.cpp` 与 `g1_sub.h`/`g1_pub.h` 的调用点、话题名、字段偏移逐一核对 |
| 构建文件核对 | 顶层 `CMakeLists.txt`、`thirdparty/CMakeLists.txt`、`example/CMakeLists.txt`、安装导出 `unitree_sdk2Targets.cmake` |
| 自动扫描 | ① 全部相对 Markdown 链接存在性；② 文档引用的 SDK 路径存在性；③ 行号引用越界检查（basename 唯一文件） |
| 真实编译 | `g++ 13.3.0` 直接编译（见 §4），未使用 cmake（本机未安装） |
| 交叉一致性 | 版本号、类名命名空间、话题名、错误码、编译状态标签在全部文档间互查 |

### 1.3 编译环境

g++ 13.3.0；**未安装** cmake、Eigen3、Boost、yaml-cpp、OpenCV、ROS 2。命令模板：

```bash
g++ -std=c++17 -O2 -I<sdk>/include -I<sdk>/thirdparty/include -I<sdk>/thirdparty/include/ddscxx \
    <src> -L<sdk>/lib/x86_64 -lunitree_sdk2 \
    -L<sdk>/thirdparty/lib/x86_64 -lddscxx -lddsc -pthread -o <out>
```

---

## 2. 问题统计

按问题类别去重统计（同一问题影响多个文件只计一次）：

| 类别 | 数量 | 严重程度分布 |
|---|---|---|
| 话题/消息事实错误（示例发布话题缺 `/cmd`） | 1 | P1 ×1 |
| 行为/生命周期结论错误（`RealTimePublisher` 析构等待时间） | 1 | P1 ×1 |
| 验证状态标签过期（全库"未编译/未执行"与真实编译结果不符） | 1 | P1 ×1 |
| 虚构类名（`G1LocoClient` 不存在） | 1 | P2 ×1 |
| 错误头文件/源码引用（`thread.hpp`、QoS 类行号） | 2 | P2 ×2 |
| 依赖/构建事实错误（SDK 依赖范围、示例目录计数） | 2 | P2 ×2 |
| 关键信息缺失（腰部类型标"未确认"、lf 分支差异未说明） | 2 | P2 ×2 |
| 描述不准确/证据等级标注缺失 | 3 | P2 ×3 |
| 错误码范围错误（7403 不存在） | 1 | P3 ×1 |
| 行号/引用漂移 | 12 | P3 ×12 |
| **合计** | **26** | P1 ×3、P2 ×10、P3 ×13 |

P0（会直接诱导危险操作或使安全机制失效）级问题：**0 个**。

### 严重程度定义

| 级别 | 定义 |
|---|---|
| **P0** | 错误信息会直接导致危险操作（如把急停/失力/权重通道写错），或使安全判据失效 |
| **P1** | 事实性错误：API/话题/行为/生命周期结论错误，照做会得到错误结果；验证状态标签与事实不符 |
| **P2** | 误导性内容：虚构名称、错误引用、依赖/计数错误、关键信息缺失；不致危险但会浪费排查时间 |
| **P3** | 轻微偏差：行号漂移、措辞不精确、推断未标注，不影响主要结论 |

---

## 3. 问题明细

| 编号 | 严重程度 | 文档位置 | 问题描述 | 源码证据 | 修正方式 | 验证状态 |
|---|---|---|---|---|---|---|
| 01 | P1 | 06 §5.2 | Dex3 发布话题写成 `rt/dex3/{left,right}`，缺 `/cmd` 后缀 | `g1_dex3_example.cpp:32`（命名空间）、`:269`（拼接 `"/cmd"`） | 改为 `rt/dex3/{left,right}/cmd`，补充订阅 `:33` | 源码复核确认 |
| 02 | P1 | g1_deployment_priority.md P0-6 | `RealTimePublisher` 析构写成"最多等待约 100 ms + 1 ms 轮询"，结论错误 | `Publisher.h:56-63`（析构 while 每 100 ms 轮询 `is_running_`）、`:133-162`（publishingLoop） | 改为"每 100 ms 轮询，若发布线程不退出可能无限等待" | 源码复核确认 |
| 03 | P1 | README/01/02/03/04/05/06/07/08/09/10/11/13/api_index/example_to_api/known_limitations/g1_deployment_priority | 全库多处"未编译/未执行"标签与真实编译结果不符；13 §9 原为占位 | §4 编译记录（11 个 G1 目标通过、4 个缺依赖失败；文档内 8/11 通过） | 统一改写为真实编译状态，并明确"编译通过 ≠ 运行验证" | g++ 编译实证 |
| 04 | P2 | 11 §1 结论表 | 出现不存在的类名 `G1LocoClient` | `g1_loco_client.hpp:12` 类名为 `unitree::robot::g1::LocoClient`；`nm` 无 `LocoClient` 独立符号（内联） | 改为 `LocoClient` 并注明命名空间 | 源码复核 + `nm` 确认 |
| 05 | P2 | 08 §4 | `CreateRecurrentThreadEx` 证据引用了不存在的 `thread.hpp:71-74` | 实际声明在 `recurrent_thread.hpp:74-75` | 改为正确文件与行号 | 源码复核确认 |
| 06 | P2 | 09 §2/§11 | 把 Eigen/Boost/yaml-cpp/fmt 列为 SDK 依赖 | 顶层 `CMakeLists.txt:54`：SDK 目标只链接 `ddsc ddscxx Threads::Threads` | 拆分说明：Eigen 仅 `dds_wrapper` 头文件所需，Boost/yaml-cpp/fmt 仅部分示例可选 | 源码 + 编译实证 |
| 07 | P2 | 09 §6 | QoS 引用错误：`dds_qos.hpp:22-42` 实为宏，被当成 Reader/Writer 类 | 类声明在 `:61,66`，`InitPolicyDefault` 声明 `:33`，实现在预编译库 | 改为精确引用并注明实现不可见 | 源码复核确认 |
| 08 | P2 | 11/13 | 示例目录计数 15 错误（把 `CMakeLists.txt` 计入） | `ls example` 15 项 = 14 目录 + 1 文件；`example/CMakeLists.txt:1-14` 共 14 个 `add_subdirectory` | 改为"14 个示例目录（含 4 个通用示例）" | 目录取证确认 |
| 09 | P2 | 04 §2 | G1D 腰部订阅类型写成"未确认类型" | `g1d_waist_example.cpp:13`（话题）`:38-39`（订阅 `MotorState_`） | 确认为 `unitree_hg::msg::dds_::MotorState_` 并补行号 | 源码复核确认 |
| 10 | P2 | 06 §5.2 | 未说明 lf 分支订阅名差异（`rt/lf/dex3/...`） | `g1_dex3_example.cpp:252-257`（L/R 分支覆盖 `sub_namespace`），发布命名空间仍 `rt/dex3/...` | 补充"lf 分支订阅 `rt/lf/dex3/{left,right}/state`"及实机未验证标注 | 源码复核确认 |
| 11 | P2 | 12 §4 | 把示例 500 Hz 控制周期写成带宽表中的 Topic 频率 | 示例为发送端周期，非 Topic 实测频率 | 注明"示例控制周期 500 Hz（非 Topic 实测频率）" | 源码复核确认 |
| 12 | P2 | 04/06 | Dex3 描述"示例自相矛盾"措辞不当（实为初始值与分支覆盖不一致） | `:33` 初始 `rt/dex3/left/state`，`:252-257` 分支覆盖为 `rt/lf/dex3/...` | 措辞改为"初始值与 L/R 分支覆盖不一致"，保留"实机正确值未验证" | 源码复核确认 |
| 13 | P2 | g1_deployment_priority.md §5 | `"lo"` 默认值陷阱的推断未标证据等级 | 该值为【推断】，非源码确认 | bullets 前补【推断】标签 | 一致性检查 |
| 14 | P3 | 06 §5 | `RIS_Mode` 位域行号 `:41-46` 错误 | `g1_dex3_example.cpp:45-49` | 修正行号 | 源码复核确认 |
| 15 | P3 | 04 §3 | Inspire 证据行号 `:85-89` 偏差 | `g1_sub.h:84-90` | 修正行号 | 源码复核确认 |
| 16 | P3 | 04 §4 | G1D 高度订阅证据 `:33` 非订阅处 | `g1d_height_control.cpp:173-175` | 改为 `:18,173-175` | 源码复核确认 |
| 17 | P3 | 08 §3.2 | `lock()` 行号 `:107-112` 偏差 | `Publisher.h:108-112` | 修正行号 | 源码复核确认 |
| 18 | P3 | 08 §3.3 | `CreateSendChannel/CreateRecvChannel` 范围 `:32-46` 偏差 | `channel_factory.hpp:32-45` | 修正范围 | 源码复核确认 |
| 19 | P3 | 09 §4 | `ros2/Imu_.hpp` 命名空间行号 `:19` 错误 | `:20` | 修正行号 | 源码复核确认 |
| 20 | P3 | 11 §5/§8 | CMake 安装规则行号 `:63-82` 偏差（两处） | `CMakeLists.txt:64-82` | 修正行号 | 源码复核确认 |
| 21 | P3 | 11 §7 | `thirdparty/CMakeLists.txt` 范围 `:29-35` 偏差 | `:28-35` | 修正范围 | 源码复核确认 |
| 22 | P3 | 11 §8 | `ExactVersion` 行号 `:74-77` 偏差 | `:74-78` | 修正行号 | 源码复核确认 |
| 23 | P3 | 12 §6 | `g1_ankle_swing_example` 必传参数行号 `:362-368` 偏差 | `:363-366` | 修正行号 | 源码复核确认 |
| 24 | P3 | 12 §5 | `IMUState_` 字段范围 `:24-28` 偏窄 | `:24-30` | 修正范围 | 源码复核确认 |
| 25 | P3 | 12 §7 | userctrl PASSIVE 检查行号 `:95-99` 偏差 | `:97-101` | 修正行号 | 源码复核确认 |
| 26 | P3 | 05 §2/api_index §3.5/g1_deployment_priority §10 | arm 错误码范围写 `7400–7404`，隐含 7403 存在 | `g1_arm_action_error.hpp:9-16` 只有 7400/7401/7402/7404 | 改为 `7400–7402、7404` 并注明无 7403 | 源码复核确认 |

> 编号 03 的"全库标签"作为**一个系统性问题**计数，不按文件重复计；各文件的具体修改见 §5 修改清单。

---

## 4. 编译验证记录

### 4.1 SDK 自带示例（15 个 G1 `.cpp` → 15 个 CMake 目标）

**编译 + 链接通过（11 个）**：

`g1_loco_client`、`g1_arm5_sdk_dds_example`、`g1_arm7_sdk_dds_example`、`g1_ankle_swing_example`、`g1_audio_client_example`、`g1_userctrl_dds_example`、`g1_hand_sdk_example`、`g1_agv_client_example`、`g1d_arm_example`、`g1d_height_control`、`g1d_waist_example`

**失败（4 个，均为环境不满足，非代码缺陷）**：

| 目标 | 失败原因 |
|---|---|
| `g1_dex3_example` | 缺 Eigen3（`eigen3/Eigen/Dense`） |
| `g1_termination` | 缺 Eigen3（本机亦未装 Boost.ProgramOptions） |
| `g1_arm_action_example` | 缺 `boost/program_options.hpp` |
| `g1_dual_arm_example` | 缺 `yaml-cpp/yaml.h` |

### 4.2 文档内完整 C++ 示例

| 文档 | 结果 |
|---|---|
| 01、02、03、05、06、08、10（7 篇 8 个程序） | 编译通过，未运行 |
| 04 | 编译失败：缺 Eigen3（`terminations.hpp`），环境不满足 |
| 07 | 编译失败：缺 OpenCV，环境不满足 |
| 09 | 编译失败：缺 `rclcpp/rclcpp.hpp`，环境不满足 |

### 4.3 边界声明

- 全部二进制**仅编译，未运行**；未连接机器人，无任何机器人副作用。
- 编译失败均区分了"代码问题 / 缺依赖 / 构建环境 / 硬件条件"：本轮**没有发现任何示例代码自身编译错误**；4+3 个失败全部为缺第三方依赖。
- CMake 配置/安装路径未验证（本机无 cmake），仅静态核对 `CMakeLists.txt` 与安装导出文件。

---

## 5. 修改清单

| 文件 | 修改内容 |
|---|---|
| `06_g1_low_level_control.md` | §5.2 Dex3 发布/订阅话题（含 lf 分支）、`RIS_Mode` 行号、§8 编译状态 |
| `04_robot_state.md` | Dex3 措辞、Inspire/G1D 证据行号、G1D 腰部类型确认、§7 编译状态 |
| `08_concurrency_and_lifecycle.md` | `lock()` 行号、工厂方法范围、`CreateRecurrentThreadEx` 头文件引用、§6 编译状态 |
| `09_ros2_integration.md` | 依赖范围拆分、`Imu_.hpp` 行号、QoS 引用、开头/§5 编译状态说明 |
| `11_build_and_link.md` | `G1LocoClient`→`LocoClient`、示例目录计数、CMake/thirdparty/ExactVersion 行号、开头与 §12 编译说明 |
| `12_network_and_deployment.md` | ankle 行号、`"lo"` 推断标注、带宽表措辞、`IMUState_` 范围、PASSIVE 行号 |
| `13_examples_index.md` | 目录计数、§9 重写为真实编译结果 |
| `g1_deployment_priority.md` | `RealTimePublisher` 析构风险、P0-1 骨架状态、错误码范围、§9 复核状态 |
| `05_g1_high_level_control.md` | arm 错误头文件计数行、§11.1 编译状态 |
| `api_index.md` | arm 错误码范围、§10 复核状态 |
| `03_message_types.md` | Dex3 措辞、§6 编译状态 |
| `01_channel_communication.md` / `02_dds_rpc_client_server.md` / `10_model_deployment_integration.md` | §7 / 开头+§7 / §10 编译状态 |
| `07_sensors_and_camera.md` | §5 OpenCV 环境不满足说明 |
| `README.md` | 标签说明、安全边界编译状态、复核状态表 |
| `known_limitations.md` | ABI 段（GCC 13.3 编译通过、运行期未验证）、§10 复核状态 |
| `example_to_api.md` | §9 复核状态 |

未修改任何无问题章节；未删除任何目录结构。

---

## 6. 剩余风险（本轮未解决、文档中已标注）

| 风险 | 状态 |
|---|---|
| 静态库内部行为（`ClientBase::Call` 匹配/超时/重试、QoS 默认值、`Release` 时序） | **未验证**，实现不可见 |
| 任何二进制运行行为、DDS 实际收发、抓包 | **未验证**（只读约束 + 无机器人） |
| Dex3 实机话题 `rt/dex3/...` vs `rt/lf/dex3/...` | 未定论，文档标注"部署前必须实测" |
| `rt/sportmodestate` 消息类型 | 仅注释推断（`g1_arm_action_error.hpp:12-14`） |
| CMake 配置/安装/`ldconfig` 运行时路径 | 未执行（无 cmake），静态分析结论 |
| 4 个示例 + 3 个文档程序因缺 Eigen3/Boost/yaml-cpp/OpenCV/ROS 2 未编译 | 环境不满足，非代码缺陷 |
| 运行期 ABI（SDK 以 GCC 9.4 预编译 vs 本机 GCC 13.3 链接） | 编译/链接层面通过；运行期未验证 |
| SDK 已知缺陷（`g1_sub.h` `ArmSdk::SharedPtr` 类型错误、`HighStand/LowStand` 数值、`PlayStop` 吞返回值、`ExecuteAction` JSON 注入） | **未修改 SDK**（只读约束），已在 known_limitations / api_index 标注规避方式 |
| 行号引用 | 基于 checkout `d1abf00`；SDK 升级后需整体复核 |

---

## 7. 最终可信度

按证据强度分级，**不给无依据的整体准确率**：

| 等级 | 覆盖内容 | 依据 |
|---|---|---|
| **源码确认** | 通信 API 签名与线程模型、消息 IDL 字段、G1 五个客户端全部方法签名/行号/内联实现、错误码、CRC 常量、构建规则与依赖、SDK 已知缺陷、`dds_wrapper` 行为（`RealTimePublisher`/`Subscription`/`BlockQueue`） | 头文件 + 示例 + CMake 逐行阅读，全部带 `file:line` 引用 |
| **编译验证** | 11 个 G1 示例目标、8 个文档内完整程序可编译可链接 | g++ 13.3.0 实测（2026-10-24）；头文件/静态库/链接参数正确 |
| **示例验证（静态）** | 15 个 G1 示例的调用链、话题名、字段偏移、参数约定 | 已读源码并与 API 文档交叉核对；**未运行** |
| **仅文档/注释依据** | `duration` 语义、`SwitchToUserCtrl` 参数语义、`SetSilent` 行为、`rt/sportmodestate` 类型、部分字段单位 | 来自注释或命名约定，已标【推断】/【未验证】 |
| **尚未验证** | 一切运行期行为：DDS 收发、QoS 实效、回调时序、机器人响应、CMake 安装、运行期 ABI、多机网络 | 需真机/实机环境；文档中对应结论均已标注 |

修正后状态：上述 26 个问题已全部落入文档修正（P0 级 0 个）；第 6 节风险为本轮**有意保留**的未验证项，均已在对应文档中以标签标明，不属于"已验证"内容。
