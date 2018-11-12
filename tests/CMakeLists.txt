function(my_add_test testFile)
    get_filename_component(testName ${testFile} NAME_WE)
    add_executable(${testName} ${testFile} test.h)
    target_link_libraries(${testName} meshouilib)
    add_test(NAME ${testName} COMMAND ${testName})
endfunction()

include_directories(../3rdparty/linalg)
include_directories(../common)
include_directories(../meshoui)

my_add_test(t_Program.cpp)
