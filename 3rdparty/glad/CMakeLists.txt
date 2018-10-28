add_library(glad
    include/glad/glad.h
    src/glad.c
)

target_link_libraries(glad)

install(TARGETS glad
                RUNTIME DESTINATION bin COMPONENT libraries
                LIBRARY DESTINATION lib COMPONENT libraries
                ARCHIVE DESTINATION lib/static COMPONENT libraries)
