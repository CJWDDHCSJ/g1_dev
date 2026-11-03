cmake_minimum_required(VERSION 3.8)
project(g1_voice_router)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

add_executable(voice_router
  src/voice_router.cpp
)

target_compile_features(voice_router PUBLIC cxx_std_17)

# 复用工作区现有的 nlohmann/json.hpp。
target_include_directories(voice_router PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/include"
)

ament_target_dependencies(voice_router
  rclcpp
  std_msgs
)

target_link_libraries(voice_router
  CURL::libcurl
  Threads::Threads
)

install(
  TARGETS voice_router
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
