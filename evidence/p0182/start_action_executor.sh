#!/usr/bin/env bash
set -eo pipefail
source /home/unitree/unitree_ros2/setup.sh
source /home/unitree/unitree_ros2/example/install/setup.bash
source /home/unitree/lidar_ws/install/setup.bash
set -u
exec /home/unitree/lidar_ws/install/g1_action_executor/lib/g1_action_executor/g1_action_executor_node
