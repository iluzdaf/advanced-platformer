if(NOT EXISTS ${PROJECT_SOURCE_DIR}/external/glm/glm/glm.hpp)
    message(
        FATAL_ERROR
        "The libraries in external/ are git submodules and have not been checked out. "
        "Run: git submodule update --init"
    )
endif()

add_library(advanced_platformer_glm INTERFACE)
target_include_directories(
    advanced_platformer_glm
    SYSTEM INTERFACE
    ${PROJECT_SOURCE_DIR}/external/glm
)

add_library(advanced_platformer_json INTERFACE)
target_include_directories(
    advanced_platformer_json
    SYSTEM INTERFACE
    ${PROJECT_SOURCE_DIR}/external/nlohmann/single_include
)

# The Lua repository has no build of its own for CMake, so the library is listed here:
# every source except the interpreter, the single-file build, and the test harness.
set(LUA_SOURCE_DIR ${PROJECT_SOURCE_DIR}/external/lua)
add_library(
    advanced_platformer_lua
    STATIC
    ${LUA_SOURCE_DIR}/lapi.c
    ${LUA_SOURCE_DIR}/lauxlib.c
    ${LUA_SOURCE_DIR}/lbaselib.c
    ${LUA_SOURCE_DIR}/lcode.c
    ${LUA_SOURCE_DIR}/lcorolib.c
    ${LUA_SOURCE_DIR}/lctype.c
    ${LUA_SOURCE_DIR}/ldblib.c
    ${LUA_SOURCE_DIR}/ldebug.c
    ${LUA_SOURCE_DIR}/ldo.c
    ${LUA_SOURCE_DIR}/ldump.c
    ${LUA_SOURCE_DIR}/lfunc.c
    ${LUA_SOURCE_DIR}/lgc.c
    ${LUA_SOURCE_DIR}/linit.c
    ${LUA_SOURCE_DIR}/liolib.c
    ${LUA_SOURCE_DIR}/llex.c
    ${LUA_SOURCE_DIR}/lmathlib.c
    ${LUA_SOURCE_DIR}/lmem.c
    ${LUA_SOURCE_DIR}/loadlib.c
    ${LUA_SOURCE_DIR}/lobject.c
    ${LUA_SOURCE_DIR}/lopcodes.c
    ${LUA_SOURCE_DIR}/loslib.c
    ${LUA_SOURCE_DIR}/lparser.c
    ${LUA_SOURCE_DIR}/lstate.c
    ${LUA_SOURCE_DIR}/lstring.c
    ${LUA_SOURCE_DIR}/lstrlib.c
    ${LUA_SOURCE_DIR}/ltable.c
    ${LUA_SOURCE_DIR}/ltablib.c
    ${LUA_SOURCE_DIR}/ltm.c
    ${LUA_SOURCE_DIR}/lundump.c
    ${LUA_SOURCE_DIR}/lutf8lib.c
    ${LUA_SOURCE_DIR}/lvm.c
    ${LUA_SOURCE_DIR}/lzio.c
)
target_include_directories(advanced_platformer_lua SYSTEM PUBLIC ${LUA_SOURCE_DIR})
target_compile_definitions(advanced_platformer_lua PRIVATE LUA_USE_POSIX)
target_link_libraries(advanced_platformer_lua PRIVATE m)

add_library(advanced_platformer_sol2 INTERFACE)
target_include_directories(
    advanced_platformer_sol2
    SYSTEM INTERFACE
    ${PROJECT_SOURCE_DIR}/external/sol2/include
)
target_link_libraries(advanced_platformer_sol2 INTERFACE advanced_platformer_lua)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(${PROJECT_SOURCE_DIR}/external/glfw external/glfw EXCLUDE_FROM_ALL)

add_library(advanced_platformer_glad ${PROJECT_SOURCE_DIR}/external/glad/src/glad.c)
target_include_directories(
    advanced_platformer_glad
    SYSTEM PUBLIC
    ${PROJECT_SOURCE_DIR}/external/glad/include
)

add_library(advanced_platformer_stb ${PROJECT_SOURCE_DIR}/external/stb_image.cpp)
target_include_directories(
    advanced_platformer_stb
    SYSTEM PUBLIC
    ${PROJECT_SOURCE_DIR}/external/stb
)

add_library(
    advanced_platformer_imgui
    ${PROJECT_SOURCE_DIR}/external/imgui/imgui.cpp
    ${PROJECT_SOURCE_DIR}/external/imgui/imgui_draw.cpp
    ${PROJECT_SOURCE_DIR}/external/imgui/imgui_tables.cpp
    ${PROJECT_SOURCE_DIR}/external/imgui/imgui_widgets.cpp
    ${PROJECT_SOURCE_DIR}/external/imgui/backends/imgui_impl_glfw.cpp
    ${PROJECT_SOURCE_DIR}/external/imgui/backends/imgui_impl_opengl3.cpp
    ${PROJECT_SOURCE_DIR}/external/implot/implot.cpp
    ${PROJECT_SOURCE_DIR}/external/implot/implot_items.cpp
    ${PROJECT_SOURCE_DIR}/external/imgui-node-editor/crude_json.cpp
    ${PROJECT_SOURCE_DIR}/external/imgui-node-editor/imgui_canvas.cpp
    ${PROJECT_SOURCE_DIR}/external/imgui-node-editor/imgui_node_editor.cpp
    ${PROJECT_SOURCE_DIR}/external/imgui-node-editor/imgui_node_editor_api.cpp
)
target_compile_features(advanced_platformer_imgui PUBLIC cxx_std_17)
target_include_directories(
    advanced_platformer_imgui
    SYSTEM PUBLIC
    ${PROJECT_SOURCE_DIR}/external/imgui
    ${PROJECT_SOURCE_DIR}/external/imgui/backends
    ${PROJECT_SOURCE_DIR}/external/implot
    ${PROJECT_SOURCE_DIR}/external/imgui-node-editor
)
target_link_libraries(advanced_platformer_imgui PUBLIC glfw)
