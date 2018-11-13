# glfw
SET(FOO_BUILD_SHARED OFF CACHE BOOL "Build libfoo shared library")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries")
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build the GLFW example programs")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build the GLFW test programs")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build the GLFW documentation")
set(GLFW_INSTALL OFF CACHE BOOL "Generate installation target")
set(GLFW_VULKAN_STATIC OFF CACHE BOOL "Use the Vulkan loader statically linked into application")
add_subdirectory(glfw)

# glslang
set(ENABLE_HLSL OFF CACHE BOOL "Enables HLSL input support")
add_subdirectory(glslang)

# imgui
find_package(Vulkan REQUIRED)
include_directories(${Vulkan_INCLUDE_DIR} imgui)
add_definitions(-DIMGUI_IMPL_API=)
add_library(imgui
    imgui/imconfig.h      imgui/imgui.h           imgui/examples/imgui_impl_glfw.cpp   imgui/imstb_textedit.h
    imgui/imgui.cpp       imgui/imgui_internal.h  imgui/examples/imgui_impl_glfw.h     imgui/imstb_truetype.h
    imgui/imgui_demo.cpp  imgui/imgui_widgets.cpp imgui/examples/imgui_impl_vulkan.cpp
    imgui/imgui_draw.cpp  imgui/imstb_rectpack.h  imgui/examples/imgui_impl_vulkan.h)
target_link_libraries(imgui)
install(TARGETS imgui
                RUNTIME DESTINATION bin COMPONENT libraries
                LIBRARY DESTINATION lib COMPONENT libraries
                ARCHIVE DESTINATION lib/static COMPONENT libraries)

# iniparser
add_definitions(-DASCIILINESZ=4096)
add_library(iniparser
    iniparser/src/dictionary.h iniparser/src/dictionary.c
    iniparser/src/iniparser.h  iniparser/src/iniparser.c)
target_link_libraries(iniparser)
install(TARGETS iniparser
                RUNTIME DESTINATION bin COMPONENT libraries
                LIBRARY DESTINATION lib COMPONENT libraries
                ARCHIVE DESTINATION lib/static COMPONENT libraries)

# linalg
add_custom_target(linalg SOURCES linalg/linalg.h)

# lodepng
add_library(lodepng lodepng/lodepng.h lodepng/lodepng.cpp)
target_link_libraries(lodepng)
install(TARGETS lodepng
                RUNTIME DESTINATION bin COMPONENT libraries
                LIBRARY DESTINATION lib COMPONENT libraries
                ARCHIVE DESTINATION lib/static COMPONENT libraries)

# nv_dds
add_library(nv_dds nv_dds/nv_dds.h nv_dds/nv_dds.cpp)
target_link_libraries(nv_dds)
install(TARGETS nv_dds
                RUNTIME DESTINATION bin COMPONENT libraries
                LIBRARY DESTINATION lib COMPONENT libraries
                ARCHIVE DESTINATION lib/static COMPONENT libraries)

# pugixml
add_library(pugixml pugixml/src/pugiconfig.hpp pugixml/src/pugixml.hpp pugixml/src/pugixml.cpp)
target_link_libraries(pugixml)
install(TARGETS pugixml
                RUNTIME DESTINATION bin COMPONENT libraries
                LIBRARY DESTINATION lib COMPONENT libraries
                ARCHIVE DESTINATION lib/static COMPONENT libraries)
