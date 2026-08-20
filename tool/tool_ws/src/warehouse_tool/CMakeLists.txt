cmake_minimum_required(VERSION 3.8)
project(warehouse_tool)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(visualization_msgs REQUIRED)
find_package(yaml-cpp REQUIRED)

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


add_library(
warehouse_tool_common
src/clicked_point_receiver.cpp
src/yaml_zone_manager.cpp
src/yaml_station_manager.cpp
src/zone_marker_publisher.cpp
src/station_marker_publisher.cpp
src/yaml_wall_manager.cpp
src/wall_marker_publisher.cpp
)

target_include_directories(
  warehouse_tool_common
  PUBLIC
  include
)

ament_target_dependencies(
  warehouse_tool_common
  rclcpp
  geometry_msgs
  visualization_msgs
)

target_link_libraries(
  warehouse_tool_common
  yaml-cpp
)

add_executable(
  zone_editor
  src/zone_editor_node.cpp
)


target_link_libraries(
  zone_editor
  warehouse_tool_common
)

ament_target_dependencies(
  zone_editor
  rclcpp
  geometry_msgs
  visualization_msgs
)

add_executable(
  station_editor
  src/station_editor_node.cpp
)


target_link_libraries(
station_editor
warehouse_tool_common
)

ament_target_dependencies(
station_editor
rclcpp
geometry_msgs
visualization_msgs
)

add_executable(
  wall_editor
  src/wall_editor_node.cpp
)


target_link_libraries(
  wall_editor
  warehouse_tool_common
)

ament_target_dependencies(
  wall_editor
  rclcpp
  geometry_msgs
  visualization_msgs
)

install(
  TARGETS
  warehouse_tool_common
  zone_editor
  station_editor
  wall_editor
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY launch config rviz DESTINATION share/${PROJECT_NAME})

install(
    PROGRAMS
        scripts/warehouse_tool.py
    DESTINATION lib/${PROJECT_NAME}
)



ament_package()
