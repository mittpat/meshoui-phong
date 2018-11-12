cmake_minimum_required(VERSION 3.0.0)
project(meshoui VERSION 0.1.0)

include(CTest)
enable_testing()

set(meshoui_root ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -std=c++17")
add_definitions(-DNV_DDS_NO_GL_SUPPORT)

include_directories(3rdparty/glfw/include)

add_subdirectory(3rdparty/glfw)
add_subdirectory(3rdparty/imgui)
add_subdirectory(3rdparty/iniparser)
add_subdirectory(3rdparty/linalg)
add_subdirectory(3rdparty/lodepng)
add_subdirectory(3rdparty/nv_dds)
add_subdirectory(3rdparty/octree)
add_subdirectory(3rdparty/pugixml)

include(CMake/modules/3p_glslang.cmake)

add_subdirectory(common)
add_subdirectory(meshoui)
add_subdirectory(tests)

include_directories(3rdparty/linalg)
include_directories(common)
include_directories(meshoui)

add_executable(meshouiview main.cpp)
target_link_libraries(meshouiview meshouilib)
