# P0-18 隔离域测试用状态发布器（持久化保存，支持按话题选择性发布）
# 用法: python3 mock_state_selective.py <duration_sec> <bms,low,sm,arm>
import sys, time
import rclpy
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from unitree_hg.msg import BmsState, LowState, SportModeState
from std_msgs.msg import String

duration = float(sys.argv[1])
topics = set(sys.argv[2].split(','))

rclpy.init()
node = Node('p018_mock_state')
pubs = {}
if 'bms' in topics:
    pubs['bms'] = node.create_publisher(BmsState, '/lf/bmsstate', qos_profile_sensor_data)
if 'low' in topics:
    pubs['low'] = node.create_publisher(LowState, '/lowstate', qos_profile_sensor_data)
if 'sm' in topics:
    pubs['sm'] = node.create_publisher(SportModeState, '/sportmodestate', qos_profile_sensor_data)
if 'arm' in topics:
    pubs['arm'] = node.create_publisher(String, '/arm/action/state', qos_profile_sensor_data)

def bms_msg():
    b = BmsState(); b.soc = 66; b.soh = 94
    b.bmsvoltage = [50173, 50190, 0]; b.current = -2257
    b.cell_vol = [3860]*13 + [0]*27
    return b

def low_msg():
    l = LowState()
    l.imu_state.quaternion = [0.86, 0.0, 0.02, 0.51]
    l.imu_state.accelerometer = [-0.4, 0.0, 9.8]
    l.imu_state.gyroscope = [0.0, 0.0, 0.0]
    l.imu_state.rpy = [0.0, 0.05, 1.07]
    l.imu_state.temperature = 79
    for i in range(29):
        l.motor_state[i].q = 0.1
        l.motor_state[i].temperature[0] = 40
        l.motor_state[i].temperature[1] = 40
    return l

start = time.time(); n = 0
while time.time() - start < duration:
    if 'bms' in pubs: pubs['bms'].publish(bms_msg())
    if 'low' in pubs: pubs['low'].publish(low_msg())
    if 'sm' in pubs:
        s = SportModeState(); s.fsm_id = 501; s.fsm_mode = 0; s.task_id = 4
        pubs['sm'].publish(s)
    if 'arm' in pubs:
        a = String(); a.data = '{"holding": false, "id": 0, "name": ""}'
        pubs['arm'].publish(a)
    n += 1; time.sleep(0.1)
print('PUBLISHED', n, 'topics', sorted(topics), flush=True)
