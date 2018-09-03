add_definitions(-DASCIILINESZ=4096)
add_library(iniparser
    dictionary.h
    dictionary.c
    iniparser.h
    iniparser.c
)

target_link_libraries(iniparser)

install(TARGETS iniparser
                RUNTIME DESTINATION bin COMPONENT libraries
                LIBRARY DESTINATION lib COMPONENT libraries
                ARCHIVE DESTINATION lib/static COMPONENT libraries)
