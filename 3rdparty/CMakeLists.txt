# assimp
set(BUILD_SHARED_LIBS OFF)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF)
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF)
set(ASSIMP_BUILD_COLLADA_IMPORTER TRUE)
set(ASSIMP_BUILD_COLLADA_EXPORTER TRUE)
set(BUILD_FRAMEWORK OFF)
set(ASSIMP_DOUBLE_PRECISION OFF)
set(ASSIMP_OPT_BUILD_PACKAGES OFF)
set(ASSIMP_ANDROID_JNIIOSYSTEM OFF)
set(ASSIMP_NO_EXPORT OFF)
set(ASSIMP_BUILD_ZLIB ON)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "If the supplementary tools for Assimp are built in addition to the library.")
set(ASSIMP_BUILD_SAMPLES OFF)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "If the test suite for Assimp is built in addition to the library.")
set(ASSIMP_COVERALLS OFF)
set(ASSIMP_WERROR OFF)
set(ASSIMP_ASAN OFF)
set(ASSIMP_UBSAN OFF)
set(SYSTEM_IRRXML OFF)
set(BUILD_DOCS OFF)
set(INJECT_DEBUG_POSTFIX OFF)
set(IGNORE_GIT_HASH ON)
add_subdirectory(assimp)
target_include_directories(assimp PUBLIC assimp/contrib)

# glfw
set(FOO_BUILD_SHARED OFF)
set(BUILD_SHARED_LIBS OFF)
set(GLFW_BUILD_EXAMPLES OFF)
set(GLFW_BUILD_TESTS OFF)
set(GLFW_BUILD_DOCS OFF)
set(GLFW_INSTALL OFF)
set(GLFW_VULKAN_STATIC)
add_subdirectory(glfw)

# glslang
function(compile_glsl binaries)
  if(NOT TARGET glslangValidator)
    message(STATUS "Info: glslangValidator not found. Will use cached spir-v files")
  endif()

  if(NOT ARGN)
    message(SEND_ERROR "Error: compile_glsl() called without any glsl files")
    return()
  endif()

  # clear output list
  set(${binaries})

  # generate from each input .frag/.vert file
  foreach(FIL ${ARGN})
    get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
    get_filename_component(FIL_NAME ${FIL} NAME_WE)
    get_filename_component(FIL_DIR ${FIL} DIRECTORY)

    if (NOT DEFINED glslang_output_dir)
      set(glslang_output_dir "${FIL_DIR}")
    endif()
    add_custom_target(glslang_make_output_dir_${FIL_NAME} COMMAND ${CMAKE_COMMAND} -E make_directory ${glslang_output_dir})

    #vertex
    set(binary "${glslang_output_dir}/${FIL_NAME}.vert.spv")
    list(APPEND ${binaries} "${binary}")

    if(TARGET glslangValidator)
      add_custom_command(
        OUTPUT "${binary}"
        COMMAND glslangValidator
        ARGS -V
             -e main
             -S vert
             -DCOMPILING_VERTEX
             -o ${binary}
             ${ABS_FIL}
        DEPENDS ${ABS_FIL} glslangValidator glslang_make_output_dir_${FIL_NAME}
        COMMENT "Running glslangValidator on ${FIL_NAME}"
        VERBATIM)
    endif()

    #fragment
    set(binary "${glslang_output_dir}/${FIL_NAME}.frag.spv")
    list(APPEND ${binaries} "${binary}")

    if(TARGET glslangValidator)
      add_custom_command(
        OUTPUT "${binary}"
        COMMAND glslangValidator
        ARGS -V
             -e main
             -S frag
             -DCOMPILING_FRAGMENT
             -o ${binary}
             ${ABS_FIL}
        DEPENDS ${ABS_FIL} glslangValidator glslang_make_output_dir_${FIL_NAME}
        COMMENT "Running glslangValidator on ${FIL_NAME}"
        VERBATIM)
    endif()
  endforeach()

  set_source_files_properties(${${binaries}} PROPERTIES GENERATED TRUE)
  set(${binaries} ${${binaries}} PARENT_SCOPE)
endfunction()

set(ENABLE_HLSL OFF CACHE BOOL "Enables HLSL input support")
add_subdirectory(glslang)

# linalg
add_library(linalg INTERFACE)
target_include_directories(linalg INTERFACE linalg)

# meshoui-ray
set(MESHOUIRAY_ASSIMP_ENABLED false)
set(MESHOUIRAY_LINALG_ENABLED false)
add_subdirectory(meshouiray)
