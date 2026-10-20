# 宇树 G1 机载计算单元硬件参数报告

## 一、 系统与主板概述
*   **设备型号**：NVIDIA Orin NX Developer Kit
*   **操作系统**：Ubuntu 20.04.6 LTS (Focal Fossa)
*   **系统内核**：5.10.104-tegra
*   **显示服务器**：X.org 1.20.13 (tty 控制台模式)

## 二、 处理器 (CPU) 参数
*   **架构**：aarch64 (ARMv8 Processor rev 1 v8l)
*   **核心数量**：8 核心 (单插槽，每核心 1 线程)
*   **主频范围**：115.2 MHz - 1984.0 MHz (当前状态：部分核心 1984 MHz，部分 730 MHz)
*   **缓存容量**：
    *   L1d 缓存：512 KiB (每核心 64KiB)
    *   L1i 缓存：512 KiB (每核心 64KiB)
    *   L2 缓存：2 MiB
    *   L3 缓存：4 MiB
*   **指令集特性 (Flags)**：fp, asimd, evtstrm, aes, pmull, sha1, sha2, crc32, atomics, fphp, asimdhp, cpuid, asimdrdm, lrcpc, dcpop, asimddp, uscat, ilrcpc, flagm

## 三、 内存 (RAM) 与交换空间 (Swap)
*   **物理内存**：总计 15 GiB (约 15.03 GiB，通常为 16GB 规格)
    *   已用：2.17 GiB
    *   可用：12 GiB
*   **交换空间 (Swap)**：总计 7.5 GiB
    *   实现方式：由 8 个 ZRAM 压缩块设备组成，每个容量为 961.8M (`zram0` - `zram7`)

## 四、 存储设备 (Storage)
*   **物理硬盘**：NVMe 固态硬盘 (`/dev/nvme0n1`)
    *   型号：NX-2TB 2280
    *   物理容量：1.9 TiB (标称 2TB)
    *   控制器接口：PCI (Device 1e4b:1202)
*   **分区挂载表**：
    *   `nvme0n1p1`：1.83 TiB，文件系统 ext4，挂载点 `/` (根目录)
    *   `nvme0n1p2`：64M
    *   `nvme0n1p3`：448K
    *   `nvme0n1p4`：32M
    *   `nvme0n1p5`：64M
    *   `nvme0n1p6`：448K
    *   `nvme0n1p7`：32M
    *   `nvme0n1p8`：80M
    *   `nvme0n1p9`：512K
    *   `nvme0n1p10`：300M
    *   `nvme0n1p11`：64M
    *   `nvme0n1p12`：80M
    *   `nvme0n1p13`：512K
    *   `nvme0n1p14`：64M

## 五、 网络适配器 (Network)
*   **有线网卡 (`eth0`)**：
    *   硬件型号：Realtek RTL8111/8168/8411 PCI Express Gigabit Ethernet Controller
    *   驱动：r8169
    *   MAC 地址：3c:6d:66:f3:1d:a6
    *   物理连接状态：UP，速率 1000 Mbps (全双工)
    *   IP 地址：192.168.123.164/24
*   **无线网卡 (`wlan0`)**：
    *   硬件类型：Wireless interface
    *   MAC 地址：94:ba:06:f3:36:99
    *   物理连接状态：UP
    *   IP 地址：10.25.125.22/24 (动态分配)
*   **虚拟网络接口**：
    *   `docker0`：网桥，MAC 02:42:90:46:29:f4，IP 172.17.0.1/16，状态 DOWN
    *   `dummy0`：以太网，MAC 82:15:b5:1c:21:82，状态 DOWN
    *   `l4tbr0`：网桥，MAC 1a:da:05:a7:b3:ed，状态 DOWN
    *   `rndis0`：以太网，MAC 1a:da:05:a7:b3:ed，状态 DOWN
    *   `usb0`：以太网，MAC 1a:da:05:a7:b3:ef，状态 DOWN

## 六、 总线与外设 (Bus & Peripherals)
### 1. PCI 设备
*   `0004:00:00.0`：NVIDIA Corporation PCI bridge (Device 229c, rev a1)
*   `0004:01:00.0`：Non-Volatile memory controller (Device 1e4b:1202, rev 01) [对应 NVMe 固态硬盘]
*   `0008:00:00.0`：NVIDIA Corporation PCI bridge (Device 229c, rev a1)
*   `0008:01:00.0`：Realtek Semiconductor Co., Ltd. RTL8111/8168/8411 Gigabit Ethernet Controller (rev 15)

### 2. USB 设备
*   **Bus 002 (USB 3.0)**：
    *   Device 003: Intel Corp. (ID: 8086:0b3a)
    *   Device 002: Realtek Semiconductor Corp. 4-Port USB 3.0 Hub (ID: 0bda:0411)
    *   Device 001: Linux Foundation 3.0 root hub
*   **Bus 001 (USB 2.0)**：
    *   Device 003: Realtek Semiconductor Corp. (ID: 0bda:a85b)
    *   Device 002: Realtek Semiconductor Corp. 4-Port USB 2.0 Hub (ID: 0bda:5411)
    *   Device 001: Linux Foundation 2.0 root hub

### 3. 串口/通讯接口状态
*   `/dev/ttyACM*`：未检测到设备
*   `/dev/ttyUSB*`：未检测到设备

## 七、 图形与显示控制
*   **图形控制器**：tegra234-display
*   **驱动模块**：nv_platform
*   **NVIDIA 专有驱动状态**：FAILED (未加载/控制台模式下不可用)

## 八、 编译工具链
*   **GCC 版本**：9.4.0
*   **Shell 环境**：bash 5.0.17