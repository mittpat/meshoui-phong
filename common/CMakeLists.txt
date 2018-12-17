add_library(commonlib collada.cpp collada.h)

use_pugixml()

target_link_libraries(commonlib pugixml)
if(NOT MSVC)
    target_link_libraries(commonlib stdc++fs)
endif()
