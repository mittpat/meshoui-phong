add_library(lodepng
    lodepng.h
    lodepng.cpp
)

target_link_libraries(lodepng)

install(TARGETS lodepng
                RUNTIME DESTINATION bin COMPONENT libraries
                LIBRARY DESTINATION lib COMPONENT libraries
                ARCHIVE DESTINATION lib/static COMPONENT libraries)
