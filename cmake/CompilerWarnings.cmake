function(cyan_set_project_warnings target)
  if(MSVC)
    target_compile_options(
      ${target}
      PRIVATE
        /W4
        /permissive-
        /Zc:__cplusplus
        /EHsc
        /utf-8
        /sdl
    )
    if(CYAN_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(
      ${target}
      PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Wshadow
    )
    if(CYAN_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()

