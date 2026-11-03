# 传输与校验记录

- 传输时间：2026-11-03
- 方式：`rsync -a`（源码/目录）+ `scp`（单文件/二进制），全部由笔记本主动从 G1 拉取
- 校验方式：**源端与目标端分别计算 SHA256，逐文件 `diff` 比对**（不仅比对压缩包）

## 1. 源码包校验结果

| 包 | 文件数 | 校验方法 | 结果 |
|---|---:|---|---|
| `g1_voice_router` | 32（src/include/test/config + CMakeLists + package.xml） | 双端 `sha256sum` 排序后 `diff` | **完全一致（无差异行）** |
| `g1_asr_bridge` | 3 | 同上 | **完全一致** |
| `g1_action_executor` | 5 | 同上 | **完全一致** |

> `g1_voice_router` 的来源端哈希排除了 `*.bak` / `*.before_*`（这些文件未纳入仓库，见 `file_inventory.md` §3）。

## 2. 集成与部署资产校验

| 文件 | 源（G1）SHA256 | 本地 SHA256 | 结果 |
|---|---|---|---|
| `g1_tts_node.cpp` | `63c8afed2cc4c8c195822fafe2d05f0cf4dca98c8ba80f78c55d31fe7891340e` | 同左 | 一致 |
| `tts_config.json` | `c652401ea23583c3187e8df58c5c817110d81f0f5669799c976e208a96739c7a` | 同左 | 一致 |
| `start_voice_router.sh` | `aad4b779eb1db9097ba2948e62f878924d55153dcce951bb9b8b7c56d28c7d2f` | 同左 | 一致 |
| `check_disk.sh` | `b3a4416f2af3b9fd9e7a31568fa02d7490cd04a95d2a9b56d5e039b36bb47466` | 同左 | 一致 |
| `g1-voice-router.service` | `d7a64e311e57bef70d856a7ad542345b583f08aebace4809670bed47def9d499` | 同左 | 一致 |

## 3. 生产二进制校验（从 `/proc/PID/exe` 读取运行映像）

| 组件 | PID | SHA256 | 冻结清单比对 |
|---|---:|---|---|
| `g1_voice_router` | 11283 | `6ab99de8ceb287d1fdb58b66399874c60b41734775a74a0c2745bf64223d6bb7` | **MATCH_FROZEN** |
| `g1_robot_state_provider` | 1379 | `b8195e26a6b7106776f7ec4875ba9150e3964f97d076ff66775a5d00ada943f1` | 一致 |
| `g1_asr_bridge` | 1382 | `0f098df28d6b266b4f13e8d0e609761b314deb2828ab5cc5dfff7cf78297f810` | 一致 |
| `g1_tts_node` | 1380 | `816fe70e2c2e0031c0b689d06146b48a61e32645694113e21f4240a6409d9aac` | 一致 |
| `g1_action_executor` | 1383 | `2b040188f7979a5f0552e672c7c415d3c96182b019875976fe7c5f725f7148a5` | 一致 |

说明：Router 的磁盘文件与运行映像哈希相同（未出现 `(deleted)`），仍按 `/proc/PID/exe` 取源以确保是"正在运行的映像"。

## 4. 文档与证据传输

| 类别 | 数量 | 校验 |
|---|---:|---|
| 历史报告 `docs/milestones/` | 16 | 逐文件 `cp -a`，本地与源目录行数/大小一致（未做内容改写） |
| 软著材料 | 11 | 同上 |
| 证据 `evidence/` | 127 | 同上 |

> 文档与日志不参与业务正确性判断，采用"原样复制 + 清单记录"，未逐文件计算 SHA256 比对（源为本地目录，复制过程无网络传输）。

## 5. 校验命令留存

```bash
# 源码双端比对（示例）
ssh unitree@<G1_HOST> 'cd ~/unitree_ros2/example/g1_voice_router && sha256sum $(find src include test config -type f ! -name "*.bak" ! -name "*.before_*" | sort) CMakeLists.txt package.xml' > /tmp/g1_vr.sha
cd ros2_ws/src/g1_voice_router && sha256sum $(find src include test config -type f ! -name '*.bak' ! -name '*.before_*' | sort) CMakeLists.txt package.xml > /tmp/loc_vr.sha
diff /tmp/g1_vr.sha /tmp/loc_vr.sha     # 无输出 = 一致

# 生产二进制
ssh unitree@<G1_HOST> 'sha256sum /proc/11283/exe'
sha256sum private_archive/production_binaries/g1_voice_router
```

## 6. 结论

- 三个 ROS 包源码**逐文件一致**，无截断、无换行差异。
- TTS 源码、配置、部署脚本、systemd unit **哈希一致**。
- 五个生产二进制与 V1.0 冻结清单**一致**，Router 命中 `MATCH_FROZEN`。
- 传输过程**未修改 G1 任何文件**（仅读取）。
