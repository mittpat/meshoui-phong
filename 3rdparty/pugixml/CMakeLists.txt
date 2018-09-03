add_library(pugixml
    pugiconfig.hpp
    pugixml.hpp
    pugixml.cpp
)

target_link_libraries(pugixml)

install(TARGETS pugixml
                RUNTIME DESTINATION bin COMPONENT libraries
                LIBRARY DESTINATION lib COMPONENT libraries
                ARCHIVE DESTINATION lib/static COMPONENT libraries)
