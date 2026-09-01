cmake_minimum_required(VERSION 3.16)

project(edge_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()


find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(
    LIBWEBSOCKETS
    REQUIRED
    libwebsockets
)


# ===============================
# source files
# ===============================
file(GLOB_RECURSE SOURCES
    CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/src/*.cpp
)

message(STATUS "Sources:")
foreach(src ${SOURCES})
    message(STATUS ${src})
endforeach()


set(CMAKE_RUNTIME_OUTPUT_DIRECTORY
    ${PROJECT_SOURCE_DIR}/bin
)

# ===============================
# executable
# ===============================
add_executable(
    edge_server
    ${SOURCES}
)

# ===============================
# include
# ===============================
target_include_directories(
    edge_server
    PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/third_party
    ${LIBWEBSOCKETS_INCLUDE_DIRS}
)

# ===============================
# link
# ===============================
target_link_libraries(
    edge_server
    PRIVATE
    Threads::Threads
    ${LIBWEBSOCKETS_LIBRARIES}
)

file(COPY ${PROJECT_SOURCE_DIR}/config
     DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})