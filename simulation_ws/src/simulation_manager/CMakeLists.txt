cmake_minimum_required(VERSION 3.8)
project(simulation_manager)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ============================================================
# Dependencies
# ============================================================

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(gazebo_msgs REQUIRED)
find_package(controller_manager_msgs REQUIRED)
find_package(Threads REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(
    LIBWEBSOCKETS
    REQUIRED
    libwebsockets
)

# ============================================================
# Executable
# ============================================================

add_executable(
  simulation_manager
  src/main.cpp
  src/simulation_manager_node.cpp
  src/edge_client.cpp
  src/gazebo_client.cpp
  src/amr_process_manager.cpp
  src/robot_sync_manager.cpp
  src/controller_checker.cpp
  src/common_func.cpp
  src/robot_lifecycle_manager.cpp
  src/websocket_client.cpp
)

# ============================================================
# Include directories
# ============================================================

target_include_directories(
  simulation_manager
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${LIBWEBSOCKETS_INCLUDE_DIRS}
)

# ============================================================
# ROS2 dependencies
# ============================================================

ament_target_dependencies(
  simulation_manager
  rclcpp
  gazebo_msgs
  geometry_msgs
  controller_manager_msgs
)

target_link_libraries(
  simulation_manager
  Threads::Threads
  ${LIBWEBSOCKETS_LIBRARIES}
)

# ============================================================
# Install executable
# ============================================================

install(
  TARGETS
    simulation_manager
  DESTINATION
    lib/${PROJECT_NAME}
)

# ============================================================
# Install headers
# ============================================================

install(
  DIRECTORY include/
  DESTINATION include
)

# ============================================================
# Testing
# ============================================================

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)

  # Skip copyright check until copyright headers are added.
  set(ament_cmake_copyright_FOUND TRUE)

  # Skip cpplint until project is fully configured.
  set(ament_cmake_cpplint_FOUND TRUE)

  ament_lint_auto_find_test_dependencies()
endif()

# ============================================================
# Package
# ============================================================

ament_package()