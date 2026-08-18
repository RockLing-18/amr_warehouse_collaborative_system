cmake_minimum_required(VERSION 3.8)
project(amr_manager)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(amr_interfaces REQUIRED)


# ============================
# Manager Node
# ============================
add_executable(
    amr_manager_node
    src/amr_manager_node.cpp
    src/navigation_client.cpp
)

target_include_directories(
    amr_manager_node
    PUBLIC
    include
)

ament_target_dependencies(
    amr_manager_node
    rclcpp
    rclcpp_action
    geometry_msgs
    amr_interfaces
)

# ============================
# install
# ============================
install(
    TARGETS
    amr_manager_node
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
