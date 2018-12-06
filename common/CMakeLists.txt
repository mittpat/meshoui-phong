add_library(commonlib
    hashid.h
    loose.h
    octree.h
    collada.cpp
    collada.h)

use_pugixml()
use_linalg()

if(MSVC)
    target_link_libraries(         commonlib pugixml)
else()
    target_link_libraries(stdc++fs commonlib pugixml)
endif()