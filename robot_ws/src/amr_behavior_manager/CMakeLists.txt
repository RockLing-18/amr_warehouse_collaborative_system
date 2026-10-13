cmake_minimum_required(VERSION 3.8)
project(amr_behavior_manager)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(amr_interfaces REQUIRED)
find_package(amr_common REQUIRED)
find_package(yaml-cpp REQUIRED)


# ============================
# Behavior Manager Node
# ============================
add_executable(
    amr_behavior_manager_node
    src/amr_behavior_manager_node.cpp
    src/navigation_client.cpp
    src/zone_station_loader.cpp
    src/goods_point_resolver.cpp
)

target_include_directories(
    amr_behavior_manager_node
    PUBLIC
    include
)

ament_target_dependencies(
    amr_behavior_manager_node
    rclcpp
    rclcpp_action
    geometry_msgs
    amr_interfaces
    amr_common
)

target_link_libraries(
    amr_behavior_manager_node
    yaml-cpp
)

# ============================
# install
# ============================
install(
    TARGETS
    amr_behavior_manager_node
    DESTINATION
    lib/${PROJECT_NAME}
)

install(
    DIRECTORY
    include/
    DESTINATION
    include
)

install(
    DIRECTORY
    config
    DESTINATION
    share/${PROJECT_NAME}
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
