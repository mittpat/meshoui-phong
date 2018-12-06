file(GLOB resources resources/*)
set_source_files_properties(${resources} PROPERTIES HEADER_FILE_ONLY TRUE)

include_directories(vk)
add_library(meshouilib
    vk/DeviceVk.cpp
    vk/DeviceVk.h
    vk/InstanceVk.cpp
    vk/InstanceVk.h
    vk/SwapChainVk.cpp
    vk/SwapChainVk.h
    enums.h
    Assets.cpp
    Assets.h
    Camera.cpp
    Camera.h
    IKeyboard.cpp
    IKeyboard.h
    IMouse.cpp
    IMouse.h
    InputCallbacks.cpp
    InputCallbacks.h
    Manipulators.h
    Mesh.cpp
    Mesh.h
    MeshLoader.cpp
    MeshLoader.h
    MeshLoaderPrivate.cpp
    MeshLoaderPrivate.h
    Program.cpp
    Program.h
    Renderer.cpp
    Renderer.h
    RendererPrivate.cpp
    RendererPrivate.h
    TextureLoader.cpp
    TextureLoader.h
    Widget.h)

set(shaders resources/shaders/Phong.frag resources/shaders/Phong.vert
            resources/shaders/Skydome.frag resources/shaders/Skydome.vert)
set(glslang_output_dir ${CMAKE_BINARY_DIR}/meshoui/resources/shaders)
compile_glsl(spirv ${shaders})

add_custom_target(meshouiresources
    DEPENDS ${resources}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/resources ${CMAKE_BINARY_DIR}/meshoui/resources)
add_dependencies(meshouilib meshouiresources)
add_custom_target(meshouilib_data SOURCES ${Resources})

find_package(Vulkan REQUIRED)

use_linalg()
use_pugixml()
use_qu3e()
include_directories(${Vulkan_INCLUDE_DIR})
include_directories(../3rdparty/glfw/include)
include_directories(../3rdparty/imgui)
include_directories(../3rdparty/lodepng)
include_directories(../3rdparty/nv_dds)
include_directories(../common)

target_link_libraries(meshouilib commonlib ${Vulkan_LIBRARIES} glfw imgui lodepng pugixml nv_dds qu3e)
if(NOT MSVC)
    target_link_libraries(meshouilib stdc++fs)
endif()

ADD_CUSTOM_TARGET(meshouilib_spirv DEPENDS ${spirv})
ADD_DEPENDENCIES(meshouilib meshouilib_spirv)
