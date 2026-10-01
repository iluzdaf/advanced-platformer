function(enable_project_warnings target)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(
            ${target}
            PRIVATE
            -Wshadow
            -Wfloat-conversion
            -Wimplicit-int-conversion
            -Wshorten-64-to-32
        )
    endif()
endfunction()
