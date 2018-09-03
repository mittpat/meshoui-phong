add_library(nv_dds
    nv_dds.h
    nv_dds.cpp
)

target_link_libraries(nv_dds)

install(TARGETS nv_dds
                RUNTIME DESTINATION bin COMPONENT libraries
                LIBRARY DESTINATION lib COMPONENT libraries
                ARCHIVE DESTINATION lib/static COMPONENT libraries)
