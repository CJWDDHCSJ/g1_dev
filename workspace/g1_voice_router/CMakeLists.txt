cmake_minimum_required(VERSION 3.8)
project(g1_voice_router)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(ament_index_cpp REQUIRED)
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)
find_package(unitree_hg REQUIRED)

add_library(robot_state_evaluator STATIC
  src/robot_state_evaluator.cpp
)

add_library(interaction_manager STATIC
  src/interaction_manager.cpp
  src/interaction_filter.cpp
  src/action_command_mapper.cpp
  src/asr_text_normalizer.cpp
)

target_compile_features(interaction_manager PUBLIC cxx_std_17)

target_include_directories(interaction_manager PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/include"
)

target_compile_features(robot_state_evaluator PUBLIC cxx_std_17)

target_include_directories(robot_state_evaluator PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/include"
)

add_executable(robot_state_provider
  src/robot_state_provider.cpp
  src/robot_state_provider_node.cpp
)

target_compile_features(robot_state_provider PUBLIC cxx_std_17)

target_include_directories(robot_state_provider PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/include"
)

ament_target_dependencies(robot_state_provider
  rclcpp
  std_msgs
  unitree_hg
)

target_link_libraries(robot_state_provider
  robot_state_evaluator
  Threads::Threads
)

add_library(knowledge_base STATIC
  src/knowledge_base.cpp
)

target_compile_features(knowledge_base PUBLIC cxx_std_17)

# 复用工作区现有的 nlohmann/json.hpp。
target_include_directories(knowledge_base PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/include"
)

add_library(fact_risk_guard STATIC
  src/fact_risk_guard.cpp
)

target_compile_features(fact_risk_guard PUBLIC cxx_std_17)

target_link_libraries(fact_risk_guard
  knowledge_base
)

add_executable(voice_router
  src/voice_router.cpp
  src/intent_recognizer.cpp
)

target_compile_features(voice_router PUBLIC cxx_std_17)

target_include_directories(voice_router PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/include"
)

ament_target_dependencies(voice_router
  rclcpp
  std_msgs
  ament_index_cpp
)

target_link_libraries(voice_router
  fact_risk_guard
  robot_state_evaluator
  interaction_manager
  knowledge_base
  CURL::libcurl
  Threads::Threads
)

install(
  TARGETS voice_router robot_state_provider
  DESTINATION lib/${PROJECT_NAME}
)

install(
  FILES config/lab_knowledge.json
  DESTINATION share/${PROJECT_NAME}/config
)

include(CTest)

if(BUILD_TESTING)
  add_executable(knowledge_base_test
    test/knowledge_base_test.cpp
  )

  target_link_libraries(knowledge_base_test
    knowledge_base
  )

  target_include_directories(knowledge_base_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/include"
  )

  add_test(
    NAME knowledge_base_test
    COMMAND knowledge_base_test
      "${CMAKE_CURRENT_SOURCE_DIR}/config/lab_knowledge.json"
  )

  add_executable(intent_recognizer_test
    test/intent_recognizer_test.cpp
    src/intent_recognizer.cpp
  )

  target_include_directories(intent_recognizer_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )

  add_test(
    NAME intent_recognizer_test
    COMMAND intent_recognizer_test
  )

  add_executable(fact_risk_guard_test
    test/fact_risk_guard_test.cpp
  )

  target_link_libraries(fact_risk_guard_test
    fact_risk_guard
    knowledge_base
  )

  target_include_directories(fact_risk_guard_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/include"
  )

  add_test(
    NAME fact_risk_guard_test
    COMMAND fact_risk_guard_test
      "${CMAKE_CURRENT_SOURCE_DIR}/config/lab_knowledge.json"
  )

  add_executable(robot_state_evaluator_test
    test/robot_state_evaluator_test.cpp
  )

  target_link_libraries(robot_state_evaluator_test
    robot_state_evaluator
  )

  target_include_directories(robot_state_evaluator_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/include"
  )

  add_test(
    NAME robot_state_evaluator_test
    COMMAND robot_state_evaluator_test
  )

  add_executable(interaction_manager_test
    test/interaction_manager_test.cpp
  )

  target_link_libraries(interaction_manager_test
    interaction_manager
    Threads::Threads
  )

  target_include_directories(interaction_manager_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/include"
  )

  add_test(
    NAME interaction_manager_test
    COMMAND interaction_manager_test
  )

  add_executable(action_command_mapper_test
    test/action_command_mapper_test.cpp
  )

  target_link_libraries(action_command_mapper_test
    interaction_manager
  )

  target_include_directories(action_command_mapper_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/include"
  )

  add_test(
    NAME action_command_mapper_test
    COMMAND action_command_mapper_test
  )

  add_executable(asr_text_normalizer_test
    test/asr_text_normalizer_test.cpp
  )

  target_link_libraries(asr_text_normalizer_test
    interaction_manager
  )

  target_include_directories(asr_text_normalizer_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/include"
  )

  add_test(
    NAME asr_text_normalizer_test
    COMMAND asr_text_normalizer_test
  )
endif()

ament_package()
