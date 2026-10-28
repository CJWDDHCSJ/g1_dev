cmake_minimum_required(VERSION 3.8)
project(g1_asr_bridge)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)

add_executable(asr_json_bridge src/asr_json_bridge.cpp)

ament_target_dependencies(asr_json_bridge
  rclcpp
  std_msgs
)

install(
  TARGETS asr_json_bridge
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
