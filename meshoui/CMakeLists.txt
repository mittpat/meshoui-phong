file(GLOB_RECURSE Resources ${CMAKE_SOURCE_DIR}/meshoui/resources/*)
set_source_files_properties(${Resources} PROPERTIES HEADER_FILE_ONLY TRUE)

add_library(meshouilib
    enums.h
    gltypes.h
    hashid.h
    loose.h
    Camera.cpp
    Camera.h
    GraphicsModule.cpp
    GraphicsModule.h
    GraphicsPrivate.cpp
    GraphicsPrivate.h
    GraphicsProgram.cpp
    GraphicsProgram.h
    GraphicsUniform.cpp
    GraphicsUniform.h
    IGraphics.cpp
    IGraphics.h
    IWhatever.h
    Mesh.cpp
    Mesh.h
    MeshLoader.cpp
    MeshLoader.h
    MeshLoaderPrivate.cpp
    MeshLoaderPrivate.h
    TextureLoader.cpp
    TextureLoader.h)

add_custom_target(meshouiresources
    DEPENDS ${Resources}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/meshoui/resources ${CMAKE_BINARY_DIR}/meshoui/resources)
add_dependencies(meshouilib meshouiresources)
add_custom_target(meshouilib_data SOURCES ${Resources})

find_package(GLEW REQUIRED)
find_package(OpenGL REQUIRED)
find_package(SDL2)
find_package(SDL2_image)

if(NOT DEFINED SDL2_LIBRARIES)
    include(FindPkgConfig)
    pkg_search_module(SDL2 REQUIRED sdl2)
endif()

if(NOT DEFINED SDL2IMAGE_LIBRARIES)
    include(FindPkgConfig)
    pkg_search_module(SDL2IMAGE REQUIRED SDL2_image)
endif()

include_directories(../3rdparty/iniparser)
include_directories(../3rdparty/imgui)
include_directories(../3rdparty/linalg)
include_directories(../3rdparty/nv_dds)
include_directories(../3rdparty/octree)
include_directories(../3rdparty/pugixml)
include_directories(../3rdparty/sdl2_image)

target_link_libraries(meshouilib stdc++fs ${SDL2_LIBRARIES} ${SDL2IMAGE_LIBRARIES} ${OPENGL_gl_LIBRARY} GLEW::GLEW iniparser imgui pugixml nv_dds)
