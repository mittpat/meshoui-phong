cmake_minimum_required(VERSION 3.0.0)
project(meshoui VERSION 0.1.0)

set(meshoui_root ${CMAKE_CURRENT_SOURCE_DIR})
if(NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
endif()
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")
add_definitions(-DNV_DDS_NO_GL_SUPPORT)

find_package(Vulkan REQUIRED)

include_directories(3rdparty/glfw/include)
include_directories(3rdparty/lodepng)
include_directories(3rdparty/nv_dds)

add_subdirectory(3rdparty)

include(CMake/modules/3p_linalg.cmake)
include(CMake/modules/3p_pugixml.cmake)

use_linalg()
use_pugixml()

add_executable(meshouiview main.cpp 
    collada.h collada.cpp
    control.h control.cpp
    phong.h phong.cpp
    vertexformat.h vertexformat.cpp)

target_link_libraries(meshouiview ${Vulkan_LIBRARIES} glfw pugixml lodepng nv_dds)
if(NOT MSVC)
    target_link_libraries(meshouiview stdc++fs)
endif()
