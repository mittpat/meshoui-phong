# glfw
set(FOO_BUILD_SHARED OFF CACHE BOOL "Build libfoo shared library")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries")
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build the GLFW example programs")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build the GLFW test programs")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build the GLFW documentation")
set(GLFW_INSTALL OFF CACHE BOOL "Generate installation target")
set(GLFW_VULKAN_STATIC OFF CACHE BOOL "Use the Vulkan loader statically linked into application")
add_subdirectory(glfw)

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
