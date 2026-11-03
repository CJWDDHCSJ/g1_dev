# 01 — 系统与 Jetson 软件栈

> 采集时间：2026-10-27（本机 `ssh unitree@192.168.123.164`，全部为只读命令；采集过程未修改机器人上任何文件）

## 主机概况

| 项目 | 值 |
|---|---|
| 主机名 | `ubuntu` |
| 登录用户 | `unitree` |
| 系统 | Ubuntu 22.04.5 LTS (Jammy) |
| 架构 | aarch64 |
| 内核 | 5.15.185-tegra（SMP PREEMPT，2026-01-15 编译） |
| CPU | 8 核 NVIDIA Cortex-A78AE（Jetson Orin 系列） |
| 内存 | 16 GB（15 Gi），Swap 7.6 GB |
| 磁盘 | /dev/nvme0n1p1 — 1.9 TB NVMe，已用 20 GB（2%） |
| 开机时长 | 采集时约 1 小时 40 分 |

## NVIDIA Jetson 软件栈

| 组件 | 版本 |
|---|---|
| JetPack | 6.2.3（含 dev / runtime，dpkg 包 `nvidia-jetpack 6.2.3+b81`） |
| L4T（Jetson Linux） | R36 (release), REVISION 5.0 |
| CUDA | 12.6（`/usr/local/cuda-12.6`，nvcc build `r12.6.r12.6`） |

## Python 与应用软件

| 项目 | 值 |
|---|---|
| Python | 3.10.12（`/usr/bin/python3`，系统自带） |
| pip 深度学习库 | 未检测到 torch / tensorflow / onnx / tensorrt / opencv 等 pip 包 |
| Docker | 未安装 |

## 观察结论

- 硬件为 Jetson Orin 级别（Cortex-A78AE + 16GB + 大容量 NVMe），是宇树机器人机载计算单元的典型配置。
- 软件栈以 **JetPack 6.2.3 + CUDA 12.6 + ROS 2** 为主，未见 pip 安装的高层 AI 框架，也无常驻容器，推理/控制应直接基于 CUDA / ROS 2 原生栈。
