cmake_minimum_required(VERSION 3.8)
project(edge_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(
    LIBWEBSOCKETS
    REQUIRED
    libwebsockets
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

# ============================================================
# Executable
# ============================================================

add_executable(
    edge_server
    src/main.cpp
    src/edge_server_node.cpp
    src/websocket/websocket_server.cpp
    src/websocket/websocket_session.cpp
    src/robot/robot_manager.cpp
    src/robot/robot_list_publisher.cpp
)



# ============================================================
# Include directories
# ============================================================

target_include_directories(
  edge_server
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${LIBWEBSOCKETS_INCLUDE_DIRS}
)

# ============================================================
# ROS2 dependencies
# ============================================================
ament_target_dependencies(
  edge_server
  rclcpp
)

target_link_libraries(
  edge_server
  Threads::Threads
  ${LIBWEBSOCKETS_LIBRARIES}
)

# ============================================================
# Install executable
# ============================================================

install(
  TARGETS
    edge_server
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

install(
    DIRECTORY
    launch
    config
    DESTINATION
    share/${PROJECT_NAME}
)


ament_package()
