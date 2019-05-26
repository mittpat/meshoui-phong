# glfw
set(FOO_BUILD_SHARED OFF CACHE BOOL "Build libfoo shared library")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries")
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build the GLFW example programs")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build the GLFW test programs")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build the GLFW documentation")
set(GLFW_INSTALL OFF CACHE BOOL "Generate installation target")
set(GLFW_VULKAN_STATIC OFF CACHE BOOL "Use the Vulkan loader statically linked into application")
add_subdirectory(glfw)

# imgui
add_library(imgui
    imgui/imconfig.h      imgui/imgui.h           imgui/examples/imgui_impl_glfw.cpp   imgui/imstb_textedit.h
    imgui/imgui.cpp       imgui/imgui_internal.h  imgui/examples/imgui_impl_glfw.h     imgui/imstb_truetype.h
    imgui/imgui_demo.cpp  imgui/imgui_widgets.cpp imgui/examples/imgui_impl_vulkan.cpp
    imgui/imgui_draw.cpp  imgui/imstb_rectpack.h  imgui/examples/imgui_impl_vulkan.h)
target_include_directories(imgui PUBLIC ${Vulkan_INCLUDE_DIR} imgui)
target_compile_definitions(imgui PUBLIC -DIMGUI_IMPL_API=)
install(TARGETS imgui
                RUNTIME DESTINATION bin COMPONENT libraries
                LIBRARY DESTINATION lib COMPONENT libraries
                ARCHIVE DESTINATION lib/static COMPONENT libraries)

# linalg
add_library(linalg INTERFACE)
target_include_directories(linalg INTERFACE linalg)

# lodepng
add_library(lodepng lodepng/lodepng.h lodepng/lodepng.cpp)
target_include_directories(lodepng PUBLIC lodepng)
install(TARGETS lodepng
                RUNTIME DESTINATION bin COMPONENT libraries
                LIBRARY DESTINATION lib COMPONENT libraries
                ARCHIVE DESTINATION lib/static COMPONENT libraries)

# nv_dds
add_library(nv_dds nv_dds/nv_dds.h nv_dds/nv_dds.cpp)
target_include_directories(nv_dds PUBLIC nv_dds)
install(TARGETS nv_dds
                RUNTIME DESTINATION bin COMPONENT libraries
                LIBRARY DESTINATION lib COMPONENT libraries
                ARCHIVE DESTINATION lib/static COMPONENT libraries)

# pugixml
add_library(pugixml pugixml/src/pugiconfig.hpp pugixml/src/pugixml.hpp pugixml/src/pugixml.cpp)
target_include_directories(pugixml PUBLIC pugixml/src)
install(TARGETS pugixml
                RUNTIME DESTINATION bin COMPONENT libraries
                LIBRARY DESTINATION lib COMPONENT libraries
                ARCHIVE DESTINATION lib/static COMPONENT libraries)
