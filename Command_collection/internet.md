### 🔌 第一步：通过网线“带外”登录机器人

G1默认关闭了Wi-Fi硬件，需要先通过有线网络SSH登录进去开启。

**1. 配置电脑有线网卡（静态IP）**
用网线连接电脑和G1的网口（通常是Port 4或5）。在**宿主机**终端找到有线网卡名（如 `enp3s0`），设置与机器人同网段的IP：
```bash
sudo ip addr flush dev enp3s0
sudo ip addr add 192.168.123.99/24 dev enp3s0
sudo ip link set enp3s0 up
```
> **G1默认有线IP**：`192.168.123.164`（开发板）或 `192.168.123.161`（主控）。

**2. SSH登录**
```bash
ssh unitree@192.168.123.164
# 默认密码：123
```
看到 `unitree@` 提示符，说明已成功登入机器人内部。

---

### 📶 第二步：在机器人内部启用Wi-Fi并配网

登录机器人后，依次执行（**在机器人的终端里**，不是你的电脑）：

**1. 解锁并启用Wi-Fi硬件**
```bash
sudo rfkill unblock all
sudo ip link set wlan0 up
sudo nmcli radio wifi on
sudo nmcli device set wlan0 managed yes
sudo systemctl restart NetworkManager
```
*G1出厂默认可能用软件锁禁用了Wi-Fi，`rfkill` 就是用来解除这个锁的。*

**2. 扫描并连接Wi-Fi**
```bash
# 列出可用网络，确认能看到你的Wi-Fi
nmcli device wifi list

# 连接你的Wi-Fi（替换SSID和密码）
sudo nmcli device wifi connect "你的WiFi名称" password "你的WiFi密码"
```
如果Wi-Fi隐藏，用完整命令：
```bash
sudo nmcli connection add type wifi ifname wlan0 con-name "MyWiFi" ssid "你的WiFi名称"
sudo nmcli connection modify "MyWiFi" wifi-sec.key-mgmt wpa-psk
sudo nmcli connection modify "MyWiFi" wifi-sec.psk "你的WiFi密码"
sudo nmcli connection modify "MyWiFi" connection.autoconnect yes
sudo nmcli connection up "MyWiFi"
```

**3. 获取无线IP并测试**
```bash
ip a show wlan0
```
记下 `inet` 后面的地址（如 `192.168.1.xxx`）。然后测试外网：
```bash
ping -c 3 8.8.8.8
```
通的话，说明机器人已独立联网。

---

### 🔄 第三步（可选但推荐）：让机器人通过你电脑共享网络

如果G1连接Wi-Fi后无法直接上网（比如校园网需要网页认证），可以让G1通过你电脑的Wi-Fi共享网络。

**在电脑上（宿主机）**：
```bash
# 开启IP转发
sudo sysctl -w net.ipv4.ip_forward=1
# 设置NAT（把 wlp132s0f0 换成你电脑的Wi-Fi网卡名）
sudo iptables -t nat -A POSTROUTING -o wlp132s0f0 -s 192.168.123.0/24 -j MASQUERADE
sudo iptables -A FORWARD -i wlp132s0f0 -o enp3s0 -m state --state RELATED,ESTABLISHED -j ACCEPT
sudo iptables -A FORWARD -i enp3s0 -o wlp132s0f0 -j ACCEPT
```
**在机器人上**：
```bash
# 把默认网关指向电脑的有线IP
sudo ip route del default 2>/dev/null || true
sudo ip route add default via 192.168.123.99 dev eth0
echo "nameserver 8.8.8.8" | sudo tee /etc/resolv.conf
ping -c 3 8.8.8.8
```
这样即使G1连的Wi-Fi本身不能上网，它也能借你电脑的网出去。

---

### 💻 第四步：断开网线，通过无线SSH登录

拔掉网线，在**你的电脑**上尝试无线SSH（用第二步记下的IP）：
```bash
ssh unitree@192.168.1.xxx   # 替换为机器人的无线IP
```
如果能登录，说明无线链路已通。之后所有开发操作（包括在容器里）都通过网络进行，不再需要网线。

---

### 🎯 第五步：配置ROS 2的DDS（无线环境关键一步）

无线环境下，`CYCLONEDDS_URI` 要指向你电脑的**无线网卡**（如 `wlp3s0`），而不是有线网卡。

**在电脑（容器/宿主机）的 `setup.sh` 中**：
```bash
export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
export CYCLONEDDS_URI='<CycloneDDS><Domain><General><Interfaces><NetworkInterface name="wlp3s0" priority="default" multicast="default" /></Interfaces></General></Domain></CycloneDDS>'
```
> 用 `ip a` 确认无线网卡名。**不要**再写 `enp3s0`，否则DDS会从错误接口发广播，看不到话题。

机器人端通常无需额外配置，确保其DDS绑定在 `wlan0` 上即可。

---

### ⚠️ 常见问题

- **`nmcli device wifi list` 看不到网络**：确认 `rfkill list` 中Wi-Fi没有被“Soft blocked”，并已执行 `ip link set wlan0 up`。
- **能连Wi-Fi但无法ping通外网**：检查是否需要网页认证，或尝试第三步的网络共享。
- **无线下 `ros2 topic list` 看不到G1**：99%是 `CYCLONEDDS_URI` 网卡名写成了有线网卡，改成无线网卡名后重新 `source`。
- **SSH连接不稳定/延迟高**：无线适合移动调试，底层高频控制（如电机使能）仍建议用**有线**，延迟和稳定性更有保障。

**安全提醒**：G1默认SSH密码是 `123`，首次登录后务必用 `passwd` 修改。