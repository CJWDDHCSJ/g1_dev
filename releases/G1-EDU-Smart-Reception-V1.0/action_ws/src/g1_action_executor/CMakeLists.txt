cmake_minimum_required(VERSION 3.8)

project(g1_action_executor)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(unitree_api REQUIRED)
find_package(unitree_hg REQUIRED)

# 复用本地 Unitree ROS 2 官方动作客户端头文件。
set(UNITREE_ROS2_EXAMPLE_INCLUDE
    "$ENV{HOME}/unitree_ros2/example/src/include")

if(NOT EXISTS "${UNITREE_ROS2_EXAMPLE_INCLUDE}/g1/g1_arm_action_client.hpp")
  message(FATAL_ERROR
    "Cannot find Unitree G1 arm action client header: "
    "${UNITREE_ROS2_EXAMPLE_INCLUDE}/g1/g1_arm_action_client.hpp")
endif()

add_executable(g1_action_executor_node
  src/main.cpp
  src/arm_action_executor.cpp
)

target_compile_features(g1_action_executor_node PRIVATE cxx_std_20)

target_include_directories(g1_action_executor_node PRIVATE
  include
  "${UNITREE_ROS2_EXAMPLE_INCLUDE}"
  "${UNITREE_ROS2_EXAMPLE_INCLUDE}/common"
)

ament_target_dependencies(g1_action_executor_node
  rclcpp
  std_msgs
  unitree_api
  unitree_hg
)

install(
  TARGETS g1_action_executor_node
  DESTINATION lib/${PROJECT_NAME}
)

install(
  DIRECTORY include/
  DESTINATION include
)

ament_package()
