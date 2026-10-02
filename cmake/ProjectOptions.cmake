option(ADVANCED_PLATFORMER_COVERAGE "Instrument the project's code for coverage" OFF)

foreach(language C CXX)
    if(
        NOT CMAKE_${language}_COMPILER_ID STREQUAL "Clang"
        OR CMAKE_${language}_COMPILER_VERSION VERSION_LESS 23
    )
        message(
            FATAL_ERROR
            "Advanced Platformer builds only with LLVM Clang 23 or newer, but the ${language} "
            "compiler is ${CMAKE_${language}_COMPILER_ID} ${CMAKE_${language}_COMPILER_VERSION}. "
            "Install LLVM with `brew install llvm` and configure with a preset."
        )
    endif()
endforeach()

cmake_path(GET CMAKE_CXX_COMPILER PARENT_PATH LLVM_BIN_DIR)
cmake_path(GET LLVM_BIN_DIR PARENT_PATH LLVM_ROOT_DIR)

if(APPLE)
    if(NOT EXISTS "${LLVM_ROOT_DIR}/lib/c++/libc++.dylib")
        message(FATAL_ERROR "No libc++ beside the compiler, in ${LLVM_ROOT_DIR}/lib/c++")
    endif()

    add_compile_definitions(_LIBCPP_DISABLE_AVAILABILITY)
    add_link_options(
        "-L${LLVM_ROOT_DIR}/lib/c++"
        "-L${LLVM_ROOT_DIR}/lib/unwind"
        -lunwind
        "-Wl,-rpath,${LLVM_ROOT_DIR}/lib/c++"
        "-Wl,-rpath,${LLVM_ROOT_DIR}/lib/unwind"
    )
else()
    if(NOT EXISTS "${LLVM_ROOT_DIR}/lib/libc++.so")
        message(FATAL_ERROR "No libc++ beside the compiler, in ${LLVM_ROOT_DIR}/lib")
    endif()

    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++>)
    add_link_options(
        $<$<LINK_LANGUAGE:CXX>:-stdlib=libc++>
        $<$<LINK_LANGUAGE:CXX>:-lc++abi>
        "-L${LLVM_ROOT_DIR}/lib"
        "-Wl,-rpath,${LLVM_ROOT_DIR}/lib"
    )
endif()

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
            -Wno-missing-designated-field-initializers
    )
endfunction()

function(enable_project_coverage target)
    if(ADVANCED_PLATFORMER_COVERAGE)
        target_compile_options(${target} PRIVATE -fprofile-instr-generate -fcoverage-mapping)
        target_link_options(${target} PRIVATE -fprofile-instr-generate)
    endif()
endfunction()
