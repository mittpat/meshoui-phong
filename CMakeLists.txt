cmake_minimum_required(VERSION 3.0.0)
project(meshoui VERSION 0.1.0)

set(meshoui_root ${CMAKE_CURRENT_SOURCE_DIR})
if(NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
endif()
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")
add_definitions(-DNV_DDS_NO_GL_SUPPORT)
add_definitions(-DMESHOUI_USE_IMGUI)

include_directories(3rdparty/glfw/include)

add_subdirectory(3rdparty)

include(CMake/modules/3p_glslang.cmake)
include(CMake/modules/3p_linalg.cmake)
include(CMake/modules/3p_pugixml.cmake)
include(CMake/modules/3p_qu3e.cmake)

add_subdirectory(common)
add_subdirectory(meshoui)

include_directories(common)
include_directories(meshoui)

use_linalg()
use_qu3e()

add_executable(meshouiview main.cpp assets.h assets.cpp manipulators.h)
target_link_libraries(meshouiview meshouilib qu3e)
