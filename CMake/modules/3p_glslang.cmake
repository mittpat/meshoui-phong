#glslang: https://github.com/KhronosGroup/glslang

function(compile_glsl binaries)
  if(NOT ARGN)
    message(SEND_ERROR "Error: compile_glsl() called without any glsl files")
    return()
  endif()

  # clear output list
  set(${binaries})

  # generate from each input .frag/.vert file
  foreach(FIL ${ARGN})
    get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
    get_filename_component(FIL_NAME ${FIL} NAME)
    get_filename_component(FIL_DIR ${FIL} DIRECTORY)

    if (NOT DEFINED glslang_output_dir)
      set(glslang_output_dir "${FIL_DIR}")
    endif()
    add_custom_target(glslang_make_output_dir_${FIL_NAME} COMMAND ${CMAKE_COMMAND} -E make_directory ${glslang_output_dir})

    set(binary "${glslang_output_dir}/${FIL_NAME}.spv")
    set(reflection "${glslang_output_dir}/${FIL_NAME}.ref")
    list(APPEND ${binaries} "${binary}")

    add_custom_command(
      OUTPUT "${binary}"
      COMMAND glslangValidator > "${reflection}"
      ARGS -V
           -q
           ${ABS_FIL}
           -o ${binary}
      DEPENDS ${ABS_FIL} glslangValidator glslang_make_output_dir_${FIL_NAME}
      COMMENT "Running glslangValidator on ${FIL_NAME}"
      VERBATIM)
  endforeach()

  set_source_files_properties(${${binaries}} PROPERTIES GENERATED TRUE)
  set(${binaries} ${${binaries}} PARENT_SCOPE)
endfunction()
