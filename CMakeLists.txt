cmake_minimum_required(VERSION 3.10.0)
project(meshoui VERSION 0.2.0)

find_package(Vulkan REQUIRED)

add_subdirectory(3rdparty)

add_executable(meshouiview
    main.cpp
    dome.h
    phong.h phong.cpp)

if(NOT MSVC)
    target_link_libraries(meshouiview ${Vulkan_LIBRARIES} assimp glfw linalg stdc++fs)
else()
    target_link_libraries(meshouiview ${Vulkan_LIBRARIES} assimp glfw linalg)
endif()

add_custom_command(TARGET meshouiview POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
                       ${CMAKE_CURRENT_SOURCE_DIR}/teapot.dae
                       $<TARGET_FILE_DIR:meshouiview>/teapot.dae
                   )
