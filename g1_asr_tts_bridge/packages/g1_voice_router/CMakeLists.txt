cmake_minimum_required(VERSION 3.8)
project(g1_voice_router)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)

add_executable(voice_router src/voice_router.cpp)

target_compile_features(voice_router PUBLIC cxx_std_17)

ament_target_dependencies(voice_router
  rclcpp
  std_msgs
)

install(
  TARGETS voice_router
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
