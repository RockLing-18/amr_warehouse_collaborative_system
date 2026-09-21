cmake_minimum_required(VERSION 3.8)
project(amr_agent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
# find_package(std_msgs REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(PahoMqttCpp REQUIRED)
find_package(fmt REQUIRED)

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



# ===============================
# source files
# ===============================
file(GLOB_RECURSE SOURCES
    CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/src/*.cpp
)


# ===============================
# executable
# ===============================
add_executable(
    amr_agent
    ${SOURCES}
)

# ===============================
# include
# ===============================
target_include_directories(
    amr_agent
    PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/third_party
    ${PROJECT_SOURCE_DIR}/../../../common
)


ament_target_dependencies(
    amr_agent
    rclcpp
    # std_msgs
)


target_link_libraries(
    amr_agent
    yaml-cpp
    PahoMqttCpp::paho-mqttpp3
    fmt::fmt
)

install(TARGETS
  amr_agent
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
