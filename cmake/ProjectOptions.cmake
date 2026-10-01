# Upstream Clang on macOS ships its own libc++ beside it. Link that one, so the library
# matches the headers the code compiled against; Apple Clang uses the system's.
if(APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    cmake_path(GET CMAKE_CXX_COMPILER PARENT_PATH llvm_bin)
    cmake_path(GET llvm_bin PARENT_PATH llvm_root)
    if(EXISTS "${llvm_root}/lib/c++/libc++.dylib")
        add_link_options(
            "-L${llvm_root}/lib/c++"
            "-L${llvm_root}/lib/unwind"
            -lunwind
            "-Wl,-rpath,${llvm_root}/lib/c++"
            "-Wl,-rpath,${llvm_root}/lib/unwind"
        )
    endif()
endif()

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
            # A designated initializer names the fields it sets and leaves the rest at
            # their defaults, which this warning would reject.
            -Wno-missing-designated-field-initializers
        )
    endif()
endfunction()
