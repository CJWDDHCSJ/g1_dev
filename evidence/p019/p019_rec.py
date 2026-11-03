# P0-19 隔离测试用 /g1/tts 录制器：按到达顺序记录时间与文本
import sys, time
import rclpy
from rclpy.node import Node
from std_msgs.msg import String

OUT = sys.argv[1] if len(sys.argv) > 1 else '/tmp/p019_tts.tsv'

rclpy.init()
node = Node('p019_tts_recorder')


def cb(msg):
    ts = time.time()
    text = msg.data.replace('\n', ' ').replace('\t', ' ')
    with open(OUT, 'a') as f:
        f.write('%.3f\t%s\n' % (ts, text))


node.create_subscription(String, '/g1/tts', cb, 10)
print('REC_READY', flush=True)
rclpy.spin(node)
