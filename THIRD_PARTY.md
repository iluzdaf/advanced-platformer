# Third-party source

Third-party libraries are git submodules under `external/`, each pinned to an upstream
commit. Clone with `git clone --recurse-submodules`, or run `git submodule update --init`
in an existing checkout. The submodules are shallow, so only the pinned commit is
fetched.

| Directory                    | Version                    | Upstream                              | Revision                                                |
| ---------------------------- | -------------------------- | ------------------------------------- | ------------------------------------------------------- |
| `external/catch2`            | Catch2 3.8.1               | `github.com/catchorg/Catch2`          | tag `v3.8.1`                                            |
| `external/glfw`              | GLFW 3.4                   | `github.com/glfw/glfw`                | tag `3.4`                                               |
| `external/glaze`             | Glaze 9.0.0                | `github.com/stephenberry/glaze`       | tag `v9.0.0`                                            |
| `external/glm`               | GLM 1.0.1                  | `github.com/g-truc/glm`               | tag `1.0.1`                                             |
| `external/imgui`             | Dear ImGui 1.91.8          | `github.com/ocornut/imgui`            | tag `v1.91.8`                                           |
| `external/implot`            | ImPlot 1.1 WIP             | `github.com/epezent/implot`           | `master` at `7eeb9168d2e5e6b14e266d8782ecf7e649dfc3a4`  |
| `external/imgui-node-editor` | imgui-node-editor 0.9.4    | `github.com/thedmd/imgui-node-editor` | `master` at `021aa0ea4da13fed864bafb2a92d4c5205076866`  |
| `external/lua`               | Lua 5.4.9                  | `github.com/lua/lua`                  | tag `v5.4.9`                                            |
| `external/nlohmann`          | JSON for Modern C++ 3.12.0 | `github.com/nlohmann/json`            | tag `v3.12.0`                                           |
| `external/sol2`              | sol2 3.5.0 and later fixes | `github.com/ThePhD/sol2`              | `develop` at `c1f95a773c6f8f4fde8ca3efe872e7286afe4444` |
| `external/stb`               | stb_image 2.30             | `github.com/nothings/stb`             | `master` at `013ac3beddff3dbffafd5177e7972067cd2b5083`  |

Three revisions are newer than their library's last tagged release:

- ImPlot and imgui-node-editor are pinned to untagged commits on `master` because their
  tagged releases predate the ImGui 1.91 API and do not compile against it.
- sol2's `develop` adds fixes made after 3.5.0 to `optional` and variadic results.

`external/glad` is the one library that is not a submodule. It is a GLAD 0.1.36 OpenGL
4.6 core loader generated on 2025-05-20: generated code with no upstream repository, so
it stays in this repository.

`external/stb_image.cpp` is the project's own translation unit that compiles the
stb_image implementation. The Lua repository has no CMake build, so
`cmake/Dependencies.cmake` lists its library sources directly.

Each dependency retains its upstream licence or licensing notice in its source tree.
The core uses GLM for vector mathematics, and the tests use Catch2.
The application uses GLFW for its window and input, GLAD to load OpenGL functions,
stb_image for texture loading, ImGui for UI, and JSON for Modern C++ and Glaze to read
level files and content catalogs. Lua and sol2 provide the protected NPC activity scripting
boundary. JSON parsing stays in `app/game`, outside the core.
