find_package(Vulkan REQUIRED)

include_directories(${Vulkan_INCLUDE_DIR})

add_definitions(-DIMGUI_IMPL_API=)
add_library(imgui
    imconfig.h      imgui.h               imgui_impl_vulkan.h imstb_textedit.h
    imgui.cpp       imgui_impl_glfw.cpp   imgui_internal.h    imstb_truetype.h
    imgui_demo.cpp  imgui_impl_glfw.h     imgui_widgets.cpp
    imgui_draw.cpp  imgui_impl_vulkan.cpp imstb_rectpack.h)

target_link_libraries(imgui)

install(TARGETS imgui
                RUNTIME DESTINATION bin COMPONENT libraries
                LIBRARY DESTINATION lib COMPONENT libraries
                ARCHIVE DESTINATION lib/static COMPONENT libraries)
