# 🤖 宇树 G1 联网与 ROS 2 常用指令大全

## 一、🔌 物理连接与 IP 配置（宿主机 / 开发电脑端）

G1 的有线通信依赖固定的 IP 网段，**必须手动设置静态 IP，不能依赖 DHCP**。

### 1. 有线直连（推荐开发调试，稳定性最高）

| 配置项 | 值 | 说明 |
| :--- | :--- | :--- |
| **电脑 IP** | `192.168.123.99` | **手动设置**，不能自动获取 |
| **子网掩码** | `255.255.255.0` | /24 子网 |
| **机器人默认 IP** | `192.168.123.10` | 有线连接时的默认地址 |
| **网关** | 通常留空或 `192.168.123.1` | 直连场景下非必需 |

**设置命令（Ubuntu 终端）**：
```bash
# 先插上网线，用 ip a 找到你连接机器人的网卡名（如 enp3s0、eth0）
ip a

# 假设网卡名为 enp3s0，设置静态 IP
sudo ip addr add 192.168.123.99/24 dev enp3s0
sudo ip link set enp3s0 up
```

### 2. 无线连接（移动场景备用）
连接 G1 发出的 Wi-Fi 热点（SSID 通常为 `Unitree-G1-xxxx`），电脑会自动获取 `192.168.1.x` 网段 IP，机器人默认无线 IP 为 `192.168.1.10`。

### 3. 连通性验证（必做）
```bash
# 有线
ping 192.168.123.10 -c 4

# 无线
ping 192.168.1.10 -c 4
```
出现 `4 packets transmitted, 4 received` 才说明物理链路正常。如果丢包，先查网线、IP 是否配错，再查防火墙。

---

## 二、⚙️ DDS 环境配置（容器 / 开发电脑端）

G1 底层使用 **CycloneDDS 0.10.2**，必须通过环境变量告诉 DDS “走哪张网卡”。

### 1. 核心环境变量

```bash
export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
export CYCLONEDDS_URI='<CycloneDDS><Domain><General><Interfaces><NetworkInterface name="enp3s0" priority="default" multicast="default" /></Interfaces></General></Domain></CycloneDDS>'
```

**⚠️ 关键点**：`name="enp3s0"` 必须替换成**你的电脑上实际连接机器人的那块网卡名**（用 `ip a` 或 `ifconfig` 查看）。**绝对不能在机器人上配你电脑的网卡名**。

### 2. 一键 Source 脚本（推荐）

在你的工作空间根目录（如 `~/unitree_ros2/`）创建 `setup.sh`：

```bash
#!/bin/bash
source /opt/ros/foxy/setup.bash
source $HOME/unitree_ros2/cyclonedds_ws/install/setup.bash
export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
export CYCLONEDDS_URI='<CycloneDDS><Domain><General><Interfaces><NetworkInterface name="enp3s0" priority="default" multicast="default" /></Interfaces></General></Domain></CycloneDDS>'
```

每次新开终端后执行：
```bash
source ~/unitree_ros2/setup.sh
```
如果不想每次都手动 source，可以把这行写入 `~/.bashrc`。

### 3. 仿真 / 本机测试（无真机时）

不连机器人、只在本机测试节点逻辑时，把网卡指定为回环 `lo`，DDS 就不会往物理网卡上发广播：
```bash
export CYCLONEDDS_URI='<CycloneDDS><Domain><General><Interfaces><NetworkInterface name="lo" priority="default" multicast="default" /></Interfaces></General></Domain></CycloneDDS>'
```

---

## 三、📡 通信验证与 G1 核心接口

### 1. 验证 DDS 是否打通（最关键的一步）

```bash
# 列出当前网络里所有话题
ros2 topic list

# 过滤 G1 相关话题
ros2 topic list | grep unitree
```

如果**看不到任何 `unitree` 话题**，99% 是 `CYCLONEDDS_URI` 里的网卡名写错了，或者没执行 `source setup.sh`。

### 2. G1 核心话题（Topic）

| 话题名 | 消息类型 | 方向 | 用途 |
| :--- | :--- | :--- | :--- |
| `/lowstate` | `unitree_go/msg/LowState` | 机器人 → 电脑 | **底层状态**：IMU、所有关节电机数据，约 **500Hz** |
| `/sportmodestate` | `unitree_go/msg/SportModeState` | 机器人 → 电脑 | **高层运动状态**：位置、速度、步态、足端位置 |
| `/unitree_g1/cmd_vel` | `geometry_msgs/msg/Twist` | 电脑 → 机器人 | **速度控制**：线速度、角速度 |
| `/unitree_g1/joint_cmd` | 自定义 | 电脑 → 机器人 | **关节控制**：单关节/多关节角度指令 |
| `/unitree_g1/state` | `unitree_g1_msgs/msg/G1State` | 机器人 → 电脑 | **综合状态**：电池、位姿、关节、里程计 |

**实时查看数据**：
```bash
# 看底层电机状态（数据量很大，建议配合 -n 限制条数）
ros2 topic echo /lowstate -n 3

# 看高层运动状态
ros2 topic echo /sportmodestate -n 3

# 看话题发布频率（确认是否掉帧）
ros2 topic hz /lowstate
```

### 3. G1 核心服务（Service）

| 服务名（常见命名） | 用途 | 安全说明 |
| :--- | :--- | :--- |
| `~/standup` | 从蹲姿/坐姿站起 | 有限状态机控制，确保安全序列 |
| `~/damp` | **阻尼模式**：电机有轻微阻力，安全中间态 | 紧急停止时优先用这个，不要直接切零力矩 |
| `~/zero_torque` | **零力矩**：所有电机完全松弛，最省电 | 仅用于完全断电前的最终状态 |

```bash
# 查看当前可用的服务
ros2 service list | grep -E "standup|damp|zero_torque"

# 调用服务（具体接口类型以实际为准）
ros2 service call /g1_bridge/standup std_srvs/srv/Trigger
```

---

## 四、🛠️ 日常调试高频指令

### 1. 话题调试

```bash
# 手动发布速度指令（测试机器人是否会动）
ros2 topic pub /unitree_g1/cmd_vel geometry_msgs/msg/Twist "{linear: {x: 0.2}, angular: {z: 0.0}}" -r 10

# 只看消息结构，不打印数据
ros2 topic info /lowstate
```

### 2. 接口查看（宇树专属消息必用）

```bash
# 列出所有 unitree 消息/服务接口
ros2 interface list | grep unitree

# 查看 LowState 到底包含哪些字段
ros2 interface show unitree_go/msg/LowState

# 直接生成一条 LowCmd 的填空模板，复制去改
ros2 interface proto unitree_go/msg/LowCmd
```

### 3. 可视化排查

```bash
# 图形化显示节点与话题的拓扑关系
ros2 run rqt_graph rqt_graph

# 生成 TF 坐标树 PDF（排查坐标系问题）
ros2 run tf2_tools view_frames
```

### 4. 系统“体检”

```bash
ros2 doctor --report
```
检查网络配置、环境变量、DDS 实现是否都正常。

---

## 五、🚨 故障排查速查

| 现象 | 最可能原因 | 排查动作 |
| :--- | :--- | :--- |
| `ros2 topic list` 看不到任何 `unitree` 话题 | `CYCLONEDDS_URI` 网卡名错误，或没 `source` | 用 `ip a` 确认电脑连机器人的网卡名，重写 `setup.sh` 后 `source` |
| 话题能看到，但 `echo` 没数据 | 网卡名配错（DDS 从错误网卡发广播） | 检查 `CYCLONEDDS_URI` 是否指向了连接机器人的物理网卡 |
| `ros2` 命令找不到 | 终端没 `source /opt/ros/foxy/setup.bash` | 新开终端先 `source /opt/ros/foxy/setup.bash` |
| 编译 DDS 时报 `undefined symbol` | 终端里残留了 ROS 环境的 `LD_LIBRARY_PATH` | `export LD_LIBRARY_PATH=""` 后重编 |
| `ping 192.168.123.10` 不通 | 电脑 IP 不是 `192.168.123.x` 网段，或网线没插好 | 设静态 IP `192.168.123.99/24`，重插网线 |

