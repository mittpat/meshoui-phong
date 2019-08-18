cmake_minimum_required(VERSION 3.10.0)
project(meshoui VERSION 0.2.0)

find_package(Vulkan REQUIRED)

add_subdirectory(3rdparty)

set(shaders dome.glsl phong.glsl)
set_source_files_properties(${shaders} PROPERTIES HEADER_FILE_ONLY TRUE)
add_executable(meshouiview
    main.cpp
    phong.h phong.cpp
    ${shaders})

if(NOT MSVC)
    target_link_libraries(meshouiview meshouiray ${Vulkan_LIBRARIES} assimp glfw linalg stdc++fs)
else()
    target_link_libraries(meshouiview meshouiray ${Vulkan_LIBRARIES} assimp glfw linalg)
endif()

add_custom_command(TARGET meshouiview POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
                       ${CMAKE_CURRENT_SOURCE_DIR}/teapot.dae
                       $<TARGET_FILE_DIR:meshouiview>/teapot.dae)

set(glslang_output_dir ${CMAKE_CURRENT_SOURCE_DIR}/cache)
compile_glsl(spirv ${shaders})
add_custom_target(meshouiview_spirv DEPENDS ${spirv})
add_custom_command(TARGET meshouiview_spirv POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
                       ${spirv}
                       ${CMAKE_BINARY_DIR})
add_dependencies(meshouiview meshouiview_spirv)
