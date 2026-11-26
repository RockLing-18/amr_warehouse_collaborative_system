cmake_minimum_required(VERSION 3.8)
project(amr_power_manager)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(amr_interfaces REQUIRED)
find_package(amr_common REQUIRED)

add_executable(
  amr_power_manager_node
  src/amr_power_manager_node.cpp
)

target_include_directories(
  amr_power_manager_node
  PUBLIC
  include
)

ament_target_dependencies(
  amr_power_manager_node
  rclcpp
  amr_interfaces
  amr_common
)

install(
  TARGETS
  amr_power_manager_node
  DESTINATION lib/${PROJECT_NAME}
)

install(
  DIRECTORY
  config
  DESTINATION share/${PROJECT_NAME}
)

ament_package()
