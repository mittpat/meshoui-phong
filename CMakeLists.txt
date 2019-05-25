cmake_minimum_required(VERSION 3.0.0)
project(meshoui VERSION 0.1.0)

find_package(Vulkan REQUIRED)

add_definitions(-DNV_DDS_NO_GL_SUPPORT)
add_subdirectory(3rdparty)

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
