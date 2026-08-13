cmake_minimum_required(VERSION 3.8)

project(amr_navigation)


if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()


# ============================
# dependencies
# ============================
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav2_msgs REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(builtin_interfaces REQUIRED)
find_package(amr_interfaces REQUIRED)

# ============================
# interfaces
# ============================
rosidl_generate_interfaces(${PROJECT_NAME}
  "action/AmrNavigateToPose.action"
  "srv/GetRobotPose.srv"
  "msg/RobotPose.msg"
  DEPENDENCIES geometry_msgs builtin_interfaces amr_interfaces
)

# ============================
# TF Helper Library
# ============================
add_library(
    tf_helper
    src/tf_helper.cpp
)

target_include_directories(
    tf_helper
    PUBLIC
    include
)


ament_target_dependencies(
    tf_helper
    rclcpp
    tf2
    tf2_ros
    tf2_geometry_msgs
)

# ============================
# Navigation Manager Library
# ============================
add_library(
    navigation_manager
    src/navigation_manager.cpp
)

target_include_directories(
    navigation_manager
    PUBLIC
    include
)

ament_target_dependencies(
    navigation_manager
    rclcpp
    rclcpp_action
    nav2_msgs
    geometry_msgs
)

# ============================
# waypoint navigator Library
# ============================
add_library(
    waypoint_navigator
    src/waypoint_navigator.cpp
)

target_include_directories(
    waypoint_navigator
    PUBLIC
    include
)

ament_target_dependencies(
    waypoint_navigator
    rclcpp
    rclcpp_action
    nav2_msgs
    geometry_msgs
)

# ============================
# Navigation Node
# ============================
add_executable(
    navigation_node
    src/navigation_node.cpp
)

target_link_libraries(
    navigation_node
    tf_helper
    navigation_manager
    waypoint_navigator
)

ament_target_dependencies(
    navigation_node
    rclcpp
    geometry_msgs
)

# ============================
# Navigation Server
# ============================
add_executable(
    navigation_server
    src/navigation_server.cpp
)

target_link_libraries(
    navigation_server
    tf_helper
    navigation_manager
)

ament_target_dependencies(
    navigation_server
    rclcpp
    rclcpp_action
    geometry_msgs
    nav2_msgs
    amr_interfaces
)

rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp")
target_link_libraries(navigation_server ${cpp_typesupport_target})

# ============================
# install
# ============================
install(
    TARGETS
    navigation_node
    navigation_server
    tf_helper
    navigation_manager
    waypoint_navigator
    DESTINATION
    lib/${PROJECT_NAME}
)

install(
    DIRECTORY
    include/
    DESTINATION
    include
)

# ============================
# test
# ============================

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
