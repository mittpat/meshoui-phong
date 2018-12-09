add_library(commonlib
    hashid.h
    loose.h
    octree.h
    collada.cpp
    collada.h)

use_pugixml()
use_linalg()

target_link_libraries(commonlib pugixml)
if(NOT MSVC)
    target_link_libraries(commonlib stdc++fs)
endif()
