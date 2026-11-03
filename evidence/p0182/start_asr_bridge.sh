#!/usr/bin/env bash
set -eo pipefail
source /home/unitree/unitree_ros2/setup.sh
source /home/unitree/unitree_ros2/example/install/setup.bash
set -u
exec /home/unitree/unitree_ros2/example/install/g1_asr_bridge/lib/g1_asr_bridge/asr_json_bridge
