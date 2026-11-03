#!/usr/bin/env bash
set -eo pipefail
source /home/unitree/unitree_ros2/setup.sh
source /home/unitree/unitree_ros2/example/install/setup.bash
set -u
exec /home/unitree/unitree_ros2/example/install/unitree_ros2_example/lib/unitree_ros2_example/g1_tts_node
