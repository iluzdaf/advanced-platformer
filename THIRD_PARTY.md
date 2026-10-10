# Third-party source

Third-party libraries are git submodules under `external/`, each pinned to an upstream
commit. Clone with `git clone --recurse-submodules`, or run `git submodule update --init`
in an existing checkout. The submodules are shallow, so only the pinned commit is
fetched.

| Directory                    | Version                    | Upstream                              | Revision                                                   |
| ---------------------------- | -------------------------- | ------------------------------------- | ---------------------------------------------------------- |
| `external/catch2`            | Catch2 3.8.1               | `github.com/catchorg/Catch2`          | tag `v3.8.1`                                               |
| `external/glfw`              | GLFW 3.4                   | `github.com/glfw/glfw`                | tag `3.4`                                                  |
| `external/glaze`             | Glaze 9.0.0                | `github.com/stephenberry/glaze`       | tag `v9.0.0`                                               |
| `external/glm`               | GLM 1.0.1                  | `github.com/g-truc/glm`               | tag `1.0.1`                                                |
| `external/imgui`             | Dear ImGui 1.91.8          | `github.com/ocornut/imgui`            | tag `v1.91.8`                                              |
| `external/implot`            | ImPlot 1.1 WIP             | `github.com/epezent/implot`           | `master` at `7eeb9168d2e5e6b14e266d8782ecf7e649dfc3a4`     |
| `external/imgui-node-editor` | imgui-node-editor 0.9.4    | `github.com/thedmd/imgui-node-editor` | `master` at `021aa0ea4da13fed864bafb2a92d4c5205076866`     |
| `external/lua`               | Lua 5.4.9                  | `github.com/lua/lua`                  | tag `v5.4.9`                                               |
| `external/sol2`              | sol2 3.5.0 and later fixes | `github.com/ThePhD/sol2`              | `develop` at `c1f95a773c6f8f4fde8ca3efe872e7286afe4444`    |
| `external/miniaudio`         | miniaudio 0.11.25          | `github.com/mackron/miniaudio`        | tag `0.11.25` (`9634bedb5b5a2ca38c1ee7108a9358a4e233f14d`) |
| `external/stb`               | stb_image 2.30             | `github.com/nothings/stb`             | `master` at `013ac3beddff3dbffafd5177e7972067cd2b5083`     |

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
stb_image for texture loading, ImGui for UI, and Glaze to read level files and content
catalogs. Lua and sol2 provide the protected NPC activity scripting boundary. JSON parsing
stays in `game/content`, outside the core.

miniaudio supplies the application's audio device only; decoding, encoding, the
resource manager, engine, node graph, and generators are disabled. On macOS the
only enabled device backend is CoreAudio. The upstream source offers public-domain
or MIT-0 licensing in `external/miniaudio/LICENSE`.

The project's procedural synth is adapted from the algorithm and parameter model
of [jsfxr](https://github.com/chr15m/jsfxr), revision
`b7b6aa27d62f8f356db267276bf54b451555c49d`, released under the Unlicense. jsfxr is
not a runtime dependency. `tests/fixtures/audio/jsfxr-*.txt` contains reference
float samples rendered from a small test patch, using a seeded generator for
noise; the parameter values are in `tests/core/audio/test_sound_patch.cpp`.
