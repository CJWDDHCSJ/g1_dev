#!/usr/bin/env bash
# 复制为 g1_env.sh 并按实际环境填写（不要提交真实值）
export ROBOT_HOST=<G1_HOST>
export ROS_DOMAIN_ID=0
export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
export ROS_WS=$HOME/unitree_ros2/example
export ACTION_WS=$HOME/lidar_ws
source $HOME/unitree_ros2/setup.sh
source $ROS_WS/install/setup.bash
source $ACTION_WS/install/setup.bash
