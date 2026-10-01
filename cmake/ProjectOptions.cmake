# The project builds only with upstream LLVM: Clang 23 or newer, its libc++, and its
# clang-format and clang-tidy. The presets select Homebrew's.
foreach(language C CXX)
    if(NOT CMAKE_${language}_COMPILER_ID STREQUAL "Clang"
       OR CMAKE_${language}_COMPILER_VERSION VERSION_LESS 23)
        message(
            FATAL_ERROR
            "Advanced Platformer builds only with LLVM Clang 23 or newer, but the ${language} "
            "compiler is ${CMAKE_${language}_COMPILER_ID} ${CMAKE_${language}_COMPILER_VERSION}. "
            "Install LLVM with `brew install llvm` and configure with a preset."
        )
    endif()
endforeach()

# The LLVM installation the compiler belongs to, whose libc++ and tools the build uses.
cmake_path(GET CMAKE_CXX_COMPILER PARENT_PATH LLVM_BIN_DIR)
cmake_path(GET LLVM_BIN_DIR PARENT_PATH LLVM_ROOT_DIR)
if(NOT EXISTS "${LLVM_ROOT_DIR}/lib/c++/libc++.dylib")
    message(FATAL_ERROR "No libc++ beside the compiler, in ${LLVM_ROOT_DIR}/lib/c++")
endif()

# Link the libc++ shipped beside the compiler, so the library matches the headers the code
# compiled against, rather than the one macOS provides. Its headers otherwise still limit
# the library to what that older system libc++ supports.
add_compile_definitions(_LIBCPP_DISABLE_AVAILABILITY)
add_link_options(
    "-L${LLVM_ROOT_DIR}/lib/c++"
    "-L${LLVM_ROOT_DIR}/lib/unwind"
    -lunwind
    "-Wl,-rpath,${LLVM_ROOT_DIR}/lib/c++"
    "-Wl,-rpath,${LLVM_ROOT_DIR}/lib/unwind"
)

function(enable_project_warnings target)
    target_compile_options(
        ${target}
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -Wshadow
        -Wfloat-conversion
        -Wimplicit-int-conversion
        -Wshorten-64-to-32
        # A designated initializer names the fields it sets and leaves the rest at their
        # defaults, which this warning would reject.
        -Wno-missing-designated-field-initializers
    )
endfunction()
