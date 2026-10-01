cmake_minimum_required(VERSION 3.25)

project(
    AdvancedPlatformer
    VERSION 0.1.0
    LANGUAGES C CXX
)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/ProjectOptions.cmake)
include(cmake/Dependencies.cmake)

add_library(advanced_platformer_core)
include(cmake/sources/Core.cmake)
target_include_directories(
    advanced_platformer_core
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(advanced_platformer_core PUBLIC cxx_std_26)
target_link_libraries(advanced_platformer_core PUBLIC advanced_platformer_glm)
enable_project_warnings(advanced_platformer_core)

add_library(advanced_platformer_scripting)
include(cmake/sources/Scripting.cmake)
target_include_directories(
    advanced_platformer_scripting
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/scripting
)
target_compile_features(advanced_platformer_scripting PUBLIC cxx_std_26)
target_link_libraries(
    advanced_platformer_scripting
    PUBLIC advanced_platformer_core
    PRIVATE advanced_platformer_sol2
)
enable_project_warnings(advanced_platformer_scripting)

add_executable(advanced_platformer)
include(cmake/sources/Application.cmake)
target_compile_features(advanced_platformer PRIVATE cxx_std_26)
target_include_directories(advanced_platformer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/app)
target_link_libraries(
    advanced_platformer
    PRIVATE
    advanced_platformer_scripting
    advanced_platformer_glad
    advanced_platformer_imgui
    advanced_platformer_json
    advanced_platformer_stb
    glfw
)
enable_project_warnings(advanced_platformer)

add_custom_target(
    game_assets
    COMMAND
        ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:advanced_platformer>/assets
    COMMENT "Copying game assets beside the executables"
    VERBATIM
)
add_dependencies(advanced_platformer game_assets)

include(cmake/Quality.cmake)

include(CTest)

if(BUILD_TESTING)
    set(CATCH_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(CATCH_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(CATCH_BUILD_EXTRA_TESTS OFF CACHE BOOL "" FORCE)
    set(CATCH_ENABLE_REPRODUCIBLE_BUILD ON CACHE BOOL "" FORCE)
    set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
    set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)
    add_subdirectory(external/catch2 EXCLUDE_FROM_ALL)

    add_executable(advanced_platformer_tests)
    include(cmake/sources/Tests.cmake)
    target_compile_features(advanced_platformer_tests PRIVATE cxx_std_26)
    target_include_directories(
        advanced_platformer_tests
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/app
        ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_link_libraries(
        advanced_platformer_tests
        PRIVATE
        advanced_platformer_scripting
        advanced_platformer_json
        Catch2::Catch2WithMain
    )
    enable_project_warnings(advanced_platformer_tests)
    add_custom_target(
        test_fixtures
        COMMAND
            ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures
            $<TARGET_FILE_DIR:advanced_platformer_tests>/tests/fixtures
        COMMENT "Copying test fixtures beside the test executable"
        VERBATIM
    )
    add_dependencies(advanced_platformer_tests game_assets test_fixtures)

    if(
        CMAKE_CXX_COMPILER_ID MATCHES "Clang"
        AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 22
    )
        target_compile_options(advanced_platformer_tests PRIVATE -Wno-c2y-extensions)
    endif()

    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/external/catch2/extras)
    include(Catch)
    catch_discover_tests(
        advanced_platformer_tests
        WORKING_DIRECTORY $<TARGET_FILE_DIR:advanced_platformer_tests>
    )
endif()

include(cmake/SourceRegistration.cmake)
verify_project_source_registration()
