file(GLOB_RECURSE Resources ${CMAKE_CURRENT_SOURCE_DIR}/resources/*)
set_source_files_properties(${Resources} PROPERTIES HEADER_FILE_ONLY TRUE)

add_library(meshouilib
    enums.h
    gltypes.h
    Camera.cpp
    Camera.h
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
    Uniform.cpp
    Uniform.h
    Widget.h)

add_custom_target(meshouiresources
    DEPENDS ${Resources}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/resources ${CMAKE_BINARY_DIR}/meshoui/resources)
add_dependencies(meshouilib meshouiresources)
add_custom_target(meshouilib_data SOURCES ${Resources})

find_package(Vulkan REQUIRED)

include_directories(${Vulkan_INCLUDE_DIR})
include_directories(../3rdparty/glfw/include)
include_directories(../3rdparty/iniparser)
include_directories(../3rdparty/imgui)
include_directories(../3rdparty/linalg)
include_directories(../3rdparty/lodepng)
include_directories(../3rdparty/nv_dds)
include_directories(../3rdparty/octree)
include_directories(../3rdparty/pugixml)
include_directories(../common)

target_link_libraries(meshouilib commonlib stdc++fs ${Vulkan_LIBRARIES} glfw iniparser imgui lodepng pugixml nv_dds)
