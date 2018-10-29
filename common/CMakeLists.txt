add_library(commonlib
    hashid.h
    loose.h
    collada.cpp
    collada.h)

include_directories(../3rdparty/linalg)
include_directories(../3rdparty/pugixml)

target_link_libraries(commonlib stdc++fs pugixml)
