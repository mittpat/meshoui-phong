cmake_minimum_required(VERSION 3.0.0)
project(meshoui VERSION 0.1.0)

set(meshoui_root ${CMAKE_CURRENT_SOURCE_DIR})
add_definitions(-DNV_DDS_NO_GL_SUPPORT)

find_package(Vulkan REQUIRED)

#include_directories(3rdparty/glfw/include)
#include_directories(3rdparty/lodepng)
#include_directories(3rdparty/nv_dds)

add_subdirectory(3rdparty)

#include_directories(${meshoui_root}/3rdparty/linalg)
#include_directories(${meshoui_root}/3rdparty/pugixml/src)

add_executable(meshouiview main.cpp 
    collada.h collada.cpp
    control.h control.cpp
    dome.h
    phong.h phong.cpp
    vertexformat.h vertexformat.cpp)

if(NOT MSVC)
    target_link_libraries(meshouiview ${Vulkan_LIBRARIES} glfw pugixml lodepng nv_dds stdc++fs linalg)
else()
    target_link_libraries(meshouiview ${Vulkan_LIBRARIES} glfw pugixml lodepng nv_dds linalg)
endif()
