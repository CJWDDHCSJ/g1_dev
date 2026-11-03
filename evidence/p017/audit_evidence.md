# P0-17 审计证据存档（只读采样原始记录）

采集时间：2026-11-01。采集方式：SSH + `ros2 topic type/info -v/interface show/echo --once/hz` 只读命令；本地 /tmp 临时探针（已删除）。

## 1. 系统与进程

```text
hostname=ubuntu  uname -m=aarch64  Ubuntu 22.04.5 LTS
ROS_DISTRO=humble  RMW=rmw_cyclonedds_cpp  ROS_DOMAIN_ID=（未导出，默认0）

ps -eo pid,ppid,stat,etime,cmd（grep 接待相关）：
  8243(1)   ros2 run g1_asr_bridge asr_json_bridge      [16:43]
  8247(8243)  …/g1_asr_bridge/lib/g1_asr_bridge/asr_json_bridge
  8244(1)   ros2 run g1_voice_router voice_router        [16:43]
  8270(8244)  …/g1_voice_router/lib/g1_voice_router/voice_router
  8245(1)   ros2 run unitree_ros2_example g1_tts_node    [16:43]
  8248(8245)  …/unitree_ros2_example/lib/unitree_ros2_example/g1_tts_node
  8878(1)   ros2 run g1_action_executor g1_action_executor_node [08:48]
  8880(8878)  /home/unitree/lidar_ws/install/g1_action_executor/lib/…/g1_action_executor_node

ros2 node list: /audio_client /g1_action_executor /g1_asr_bridge /g1_tts_node /g1_voice_router /ros_bridge
```

## 2. 状态话题清单（ros2 topic list -t 摘录）

```text
/arm/action/state [std_msgs/msg/String]
/dog_imu_raw [sensor_msgs/msg/Imu]        /dog_odom [nav_msgs/msg/Odometry]
/lf/agvbmsstate [unitree_hg/msg/AgvBmsState]      (pub=0)
/lf/battery_alarm [std_msgs/msg/String]
/lf/bmsstate [unitree_hg/msg/BmsState]
/lf/lowstate [unitree_go/msg/LowState, unitree_hg/msg/LowState]   (双类型)
/lf/odommodestate [/lf/sportmodestate] [unitree_go/SportModeState, unitree_hg/SportModeState]
/lf/secondary_imu [/secondary_imu] [unitree_hg/msg/IMUState]
/lowstate [unitree_hg/msg/LowState]
/lowstate_doubleimu [unitree_hg_doubleimu/msg/doubleIMUState]
/odommodestate [unitree_go/msg/SportModeState]
/sportmodestate [unitree_hg/msg/SportModeState]
/utlidar/imu_livox_mid360 [sensor_msgs/msg/Imu]
```

## 3. topic info -v 要点

```text
/lowstate:          pub=1 (RELIABLE/VOLATILE, _CREATED_BY_BARE_DDS_APP_); sub=5 (g1_action_executor=BEST_EFFORT)
/sportmodestate:    pub=1 (RELIABLE); sub=2 (g1_action_executor=RELIABLE)
/arm/action/state:  pub=1 (RELIABLE); sub=3 (g1_action_executor=RELIABLE)
/dog_odom:          pub=1 (RELIABLE); sub=2 (BEST_EFFORT)
/secondary_imu:     pub=1; sub=0
/lf/bmsstate:       pub=1 (RELIABLE); sub=6
/lf/agvbmsstate:    pub=0  → UNAVAILABLE
/lf/battery_alarm:  pub=1; sub=2
```

## 4. 接口定义（ros2 interface show）

```text
unitree_hg/msg/LowState:
  uint32[2] version; uint8 mode_pr; uint8 mode_machine; uint32 tick
  IMUState imu_state { float32[4] quaternion; float32[3] gyroscope;
                       float32[3] accelerometer; float32[3] rpy; int16 temperature }
  MotorState[35] motor_state { uint8 mode; float32 q; float32 dq; float32 ddq;
                               float32 tau_est; int16[2] temperature; float32 vol;
                               uint32[2] sensor; uint32 motorstate; uint32[4] reserve }
  uint8[40] wireless_remote; uint32[4] reserve; uint32 crc

unitree_hg/msg/BmsState:
  uint8 version_high/low; uint8 fn; uint16[40] cell_vol; uint32[3] bmsvoltage;
  int32 current; uint8 soc; uint8 soh; int16[12] temperature; uint16 cycle;
  uint16 manufacturer_date; uint32[5] bmsstate; uint32[3] reserve

unitree_hg/msg/IMUState: quaternion[4], gyroscope[3], accelerometer[3], rpy[3], int16 temperature

unitree_hg/msg/SportModeState: uint32 fsm_id; uint32 fsm_mode; uint32 task_id; float32 task_time
```

## 5. 实际采样（echo --once）

```text
/arm/action/state:  data: '{"holding": false, "id": 0, "name": ""}'
/sportmodestate:    fsm_id=501 fsm_mode=0 task_id=4 task_time=0.0
/lowstate:          mode_pr=0 mode_machine=5 tick=4113050; imu_state.temperature=79
  motor_state 35 路，索引 0-28 有效（29 路），温度范围 32-49；
  代表值 motor[3]: q=0.5941 tau=-11.514 temp=(35,39)；motor[9]: tau=-18.721 temp=(37,49)
  索引 29-34 q 全 0
/lowstate.imu_state: quaternion=[0.861,-0.0088,0.0220,0.508] rpy=[0.0072,0.0468,1.0667]
  accelerometer z=9.798  gyroscope ~1e-2 量级
/secondary_imu:     quaternion=[0.9366,0.0025,0.0133,0.3501] accelerometer z=9.700 rpy z=0.7155 temperature=80
/lf/bmsstate:       soc=66（此前另一次采样 73） soh=94 current=-2197 cycle=20
  cell_vol 40 路，前 13 有效 [3855..3868] mV，其余 0
  bmsvoltage=[50173,50190,0]（mV，与 13×~3860 交叉验证一致）
  temperature=[29,27,26,31,0,0,0,0,0,0,0,0]
/lf/battery_alarm:  data: '{"alarm_status":0,"timestamp":"1793517456838","cell_voltages":[3937,...]}'
/dog_odom:          header.stamp=1793517489.605 frame_id=odom child=robot_center
  position=(-0.034,0.162,0.709) orientation 四元数有效
```

## 6. 发布频率（ros2 topic hz）

```text
/lowstate:          average rate: 1053.077  (std 0.00011s) 连续稳定
/sportmodestate:    average rate: 100.018   (std 0.00023s) 连续稳定
/arm/action/state:  average rate: 9.923     (std 0.00025s) 连续（非事件型）
/dog_odom:          average rate: 1047.338  (std 0.00030s) 连续稳定
/lf/bmsstate:       average rate: 20.249    (std 0.00040s) 连续稳定
/secondary_imu:     average rate: 1052.818  连续稳定
```

## 7. tick 递增验证（间隔约 2 秒两次采样）

```text
4313931 → 4317602  （Δ≈3671 / ~2s，符合 ~1kHz；uint32 @1kHz 约 71.6 分钟回绕）
```

## 8. 离线意图/Guard 探针（本地 /tmp 编译真实源码，运行后已删除）

```text
[IntentRecognizer]
ROBOT_STATUS    | 你现在运行状态怎么样
ROBOT_STATUS    | 机器人现在正常吗
ROBOT_STATUS    | 你现在有多少电量
ROBOT_STATUS    | 你现在的电池电量是多少
GENERAL_QA      | 你的关节温度是多少
GENERAL_QA      | 你现在正在执行什么动作
ROBOT_STATUS    | 你的IMU数据正常吗
DEMO_ACTION     | 给我们表演一个动作
ROBOT_STATUS    | 实验室当前状态怎么样
ROBOT_STATUS    | 你现在还有多少电量
ROBOT_STATUS    | 机器人电量还剩多少
GENERAL_QA      | 你累不累
GENERAL_QA      | 你现在蹲着还是站着
GENERAL_QA      | 你关节温度高不高
GENERAL_QA      | 动作做完了吗

[FactRiskGuard]
NORMAL           | 你现在运行状态怎么样     ← 漏放
REALTIME_STATUS  | 机器人现在正常吗
REALTIME_STATUS  | 你现在有多少电量
REALTIME_STATUS  | 你现在的电池电量是多少
REALTIME_STATUS  | 你的关节温度是多少
NORMAL           | 你现在正在执行什么动作   ← 漏放
NORMAL           | 你的IMU数据正常吗       ← 漏放
REALTIME_STATUS  | 你现在还有多少电量
REALTIME_STATUS  | 你关节温度高不高
NORMAL           | 动作做完了吗             ← 漏放
NORMAL           | 你现在蹲着还是站着       ← 漏放
REALTIME_STATUS  | 报一下你的电池电量
```
