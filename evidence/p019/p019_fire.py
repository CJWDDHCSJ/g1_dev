# P0-19 高频发布器：按给定间隔连续发布多条 ASR 文本
# 用法: python3 p019_fire.py <interval_ms> <text1> <text2> ...
import sys, time
import rclpy
from rclpy.node import Node
from std_msgs.msg import String

interval_ms = float(sys.argv[1])
texts = sys.argv[2:]

rclpy.init()
node = Node('p019_fire')
pub = node.create_publisher(String, '/g1/asr_text', 10)

# 等待订阅连接建立
time.sleep(1.0)

for text in texts:
    msg = String()
    msg.data = text
    pub.publish(msg)
    print('PUB %.3f %s' % (time.time(), text), flush=True)
    time.sleep(interval_ms / 1000.0)

time.sleep(0.5)
