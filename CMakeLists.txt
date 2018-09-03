cmake_minimum_required(VERSION 3.0.0)
project(meshoui VERSION 0.1.0)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -std=c++17")

add_subdirectory(3rdparty/imgui)
add_subdirectory(3rdparty/iniparser)
add_subdirectory(3rdparty/linalg)
add_subdirectory(3rdparty/nv_dds)
add_subdirectory(3rdparty/octree)
add_subdirectory(3rdparty/pugixml)
add_subdirectory(meshoui)

include_directories(3rdparty/linalg)
include_directories(meshoui)

add_executable(meshouiview main.cpp)
target_link_libraries(meshouiview meshouilib)
