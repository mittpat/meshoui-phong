add_library(imgui
    imconfig.h      imgui.h                 imgui_impl_sdl.h  stb_truetype.h
    imgui.cpp       imgui_impl_opengl3.cpp  imgui_internal.h
    imgui_demo.cpp  imgui_impl_opengl3.h    stb_rect_pack.h
    imgui_draw.cpp  imgui_impl_sdl.cpp      stb_textedit.h
)

target_link_libraries(imgui)

install(TARGETS imgui
                RUNTIME DESTINATION bin COMPONENT libraries
                LIBRARY DESTINATION lib COMPONENT libraries
                ARCHIVE DESTINATION lib/static COMPONENT libraries)
