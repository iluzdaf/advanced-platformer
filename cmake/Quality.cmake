find_program(RUFF_EXECUTABLE NAMES ruff)
find_program(LUACHECK_EXECUTABLE NAMES luacheck)
find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy HINTS "${LLVM_BIN_DIR}")

file(
    GLOB_RECURSE PROJECT_CPP_FILES
    CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/app/*.cpp
    ${PROJECT_SOURCE_DIR}/scripting/*.cpp
    ${PROJECT_SOURCE_DIR}/src/*.cpp
    ${PROJECT_SOURCE_DIR}/tests/*.cpp
)

file(
    GLOB_RECURSE PROJECT_HEADERS
    CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/app/*.hpp
    ${PROJECT_SOURCE_DIR}/include/*.hpp
    ${PROJECT_SOURCE_DIR}/scripting/*.hpp
    ${PROJECT_SOURCE_DIR}/tests/*.hpp
)

file(GLOB_RECURSE PROJECT_PUBLIC_HEADERS CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/include/*.hpp)
list(APPEND PROJECT_PUBLIC_HEADERS ${PROJECT_SOURCE_DIR}/scripting/lua_npc_scripts.hpp)

file(GLOB_RECURSE PROJECT_PYTHON_FILES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/tools/*.py)

file(
    GLOB_RECURSE PROJECT_LUA_FILES
    CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/assets/*.lua
    ${PROJECT_SOURCE_DIR}/tests/fixtures/*.lua
)

if(RUFF_EXECUTABLE)
    add_custom_target(
        lint-python
        COMMAND ${RUFF_EXECUTABLE} check ${PROJECT_PYTHON_FILES}
        COMMENT "Checking first-party Python source with Ruff"
        VERBATIM
    )
else()
    message(STATUS "ruff not found; Python lint target is unavailable")
endif()

if(LUACHECK_EXECUTABLE)
    add_custom_target(
        lint-lua
        COMMAND ${LUACHECK_EXECUTABLE} ${PROJECT_LUA_FILES}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMENT "Checking first-party Lua source with Luacheck"
        VERBATIM
    )
else()
    message(STATUS "luacheck not found; Lua lint target is unavailable")
endif()

if(CLANG_TIDY_EXECUTABLE)
    add_custom_target(
        tidy
        COMMAND
            ${CLANG_TIDY_EXECUTABLE} -p ${CMAKE_BINARY_DIR} --warnings-as-errors=* --quiet
            ${PROJECT_CPP_FILES} ${PROJECT_HEADERS}
        COMMENT "Checking first-party C++ source with clang-tidy"
        VERBATIM
    )
else()
    message(STATUS "clang-tidy not found; tidy target is unavailable")
endif()

set_source_files_properties(${PROJECT_PUBLIC_HEADERS} PROPERTIES LANGUAGE CXX)
add_library(header_self_containment OBJECT EXCLUDE_FROM_ALL ${PROJECT_PUBLIC_HEADERS})
target_compile_features(header_self_containment PRIVATE cxx_std_26)
target_link_libraries(header_self_containment PRIVATE advanced_platformer_core)
enable_project_warnings(header_self_containment)

target_compile_options(
    header_self_containment
    PRIVATE -Wno-pragma-once-outside-header -Wno-unused-const-variable
)
