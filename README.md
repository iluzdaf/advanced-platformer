# Advanced Platformer

Advanced Platformer is a C++26 engine and example game built from independently
testable systems. The current implementation includes platformer movement, tile collision,
scrolling, composed actors, NPC state machines with Lua activities, flying and platformer
pathfinding, projectiles, animation, inventory, automatic pickups, a three-level game
loop, and ImGui debugging tools.

## Documentation

| Document                                | What it covers                                                                    |
| --------------------------------------- | --------------------------------------------------------------------------------- |
| [ARCHITECTURE.md](docs/ARCHITECTURE.md) | Design, ownership rules, runtime flow, and the reasons behind the main decisions. |
| [CONTENT.md](docs/CONTENT.md)           | How to author levels, definitions, machines, and NPC scripts under `assets`.      |
| [GLOSSARY.md](docs/GLOSSARY.md)         | The words the code and documents use, each with one meaning.                      |
| [FUTURE_WORK.md](docs/FUTURE_WORK.md)   | Proposed features that are not implemented yet.                                   |

## Requirements

- CMake 4.4 or newer (`brew install cmake`)
- LLVM 23 or newer from Homebrew (`brew install llvm`). It is the only supported
  toolchain: its Clang compiles the project as C++26 against its own libc++, and its
  clang-format and clang-tidy check it. CMake stops with an error for any other compiler.
- The Xcode Command Line Tools (`xcode-select --install`), whose macOS SDK Homebrew's
  Clang builds against

Third-party libraries are git submodules under `external/` (see
[THIRD_PARTY.md](THIRD_PARTY.md)). Clone with them:

```sh
git clone --recurse-submodules https://github.com/iluzdaf/advanced-platformer.git
```

In a checkout made without them, run `git submodule update --init`. Run it again after
pulling a change that moves a submodule.

macOS is the only supported platform for development and graphical testing. CI runs on
Linux, headless, with the same LLVM major version.

## macOS: configure, build, and test

The shared macOS presets use Homebrew's LLVM at `/opt/homebrew/opt/llvm`.

```sh
cmake --preset mac-debug
cmake --build --preset mac-debug
ctest --preset mac-debug
```

Run the example game:

```sh
cd build/mac-debug
./advanced_platformer
```

Press F1 in the game to open the [debug overlay](#debug-overlay), which shows frame
timings among other things. For performance numbers, build and run the release preset
instead. The debug build has no optimisation, so its timings may not be an accurate
representation of what players will experience.

```sh
cmake --preset mac-release
cmake --build --preset mac-release
build/mac-release/advanced_platformer
```

`CMakePresets.json` contains the shared macOS configurations.
`CMakeUserPresets.json` is ignored and is available for personal configuration that
should not be shared with version control.

## Running focused tests

Build before running CTest so the test executable includes your changes. On macOS,
list test names or run only tests whose names contain `Pickup` with:

```sh
ctest --preset mac-debug -N
# Omit -R "Pickup" to run the complete suite.
ctest --preset mac-debug -R "Pickup" --output-on-failure
```

## Playing the example game

| Action                                        | Controls                                  |
| --------------------------------------------- | ----------------------------------------- |
| Move                                          | A and D, or the left and right arrow keys |
| Jump                                          | W, Up, or Space                           |
| Aim                                           | Mouse                                     |
| Fire                                          | Left mouse button                         |
| Collect an item                               | Walk over it                              |
| Open or close the inventory (pauses the game) | Q, or click the bag at the bottom-left    |
| Drink a health potion                         | Click it in the open inventory            |
| Restart from the starting level               | R, at the completion message              |
| Pause or resume the simulation                | P                                         |
| Run one simulation step while paused          | . (full stop)                             |
| Toggle the debug overlay                      | F1                                        |
| Close the window                              | Escape                                    |

The hearts at the top-left show the player's current and maximum health.

Find each level's key and reach its bunker door to unlock the exit. Each door consumes
one key; the third exit completes the example campaign.

## Debug overlay

F1 opens the debug tools with only the frame plot visible. It shows the last two seconds
against the 60 Hz budget, with simulation costs stacked by category.

| Action                                            | Controls                              |
| ------------------------------------------------- | ------------------------------------- |
| Show or hide the frame details and legend         | 1                                     |
| Show or hide the world-space and camera overlay   | 2                                     |
| Show or hide actor text                           | 3                                     |
| Show or hide navigation-cache totals              | 4                                     |
| Show or hide the state-machine window             | 5                                     |
| Lock or unlock the machine window to an NPC       | Click the NPC                         |
| Pause and inspect a frame, or scrub across frames | Press or drag on the plot             |
| Deselect and resume                               | Click the picked frame again          |
| Show or hide a plotted series                     | Click it in the plot's legend         |
| Show the next navigation-cache profile            | N                                     |
| Break a labelled tile under the cursor            | B                                     |
| Move a state in the machine window                | Drag it                               |
| Pan or zoom the machine window                    | Drag with the right button, or scroll |
| Fit the machine window to its graph               | F, with the cursor over it            |

Timings are only meaningful from a release build.

## Continuous integration

GitHub Actions runs the jobs below. The names are the ones shown on a pull request.

| Job                          | Runner         | What it does                                                                                                                                          | Runs on                            |
| ---------------------------- | -------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------- |
| Build and test               | `ubuntu-24.04` | Configures, builds, and runs the whole test suite.                                                                                                    | pushes to `main` and pull requests |
| Formatting                   | `ubuntu-24.04` | Checks the formatting of C++, CMake, JSON, YAML, Markdown, Python, and Lua, lints the Python and Lua, and runs the tests for the repository's tools.  | pull requests only                 |
| Headers stand alone          | `ubuntu-24.04` | Compiles every public header on its own.                                                                                                              | pull requests only                 |
| Static analysis (1/3 to 3/3) | `ubuntu-24.04` | Runs clang-tidy, with warnings as errors, on the files the pull request affects (see [Static analysis](#static-analysis)), split across three shards. | pull requests only                 |
| Static analysis              | `ubuntu-24.04` | Passes only if every static analysis shard passed. This is the check branch protection requires.                                                      | pull requests only                 |

The jobs other than build and test are skipped on pushes because branch protection
already ran them on the pull request.

Every job runs on Linux and installs the same toolchain through
[`install-toolchain`](.github/actions/install-toolchain/action.yml): CMake 4.4.3 from
PyPI and LLVM 23 from apt.llvm.org, with its libc++, clang-format and clang-tidy, rather
than the runner's own. The jobs configure with the `linux-debug` preset, which builds
GLFW without a display backend, so nothing needs a window.

The build and test job uses a pinned `sccache` release backed by GitHub Actions'
cache service. Only compiler outputs are cached; generated build directories are not.
None of this affects local builds.

## Formatting

`.clang-format` defines the C and C++ style; `.gersemirc` defines the CMake style;
`.prettierrc` covers JSON, YAML, and Markdown. Ruff formats and checks first-party Python. `.stylua.toml` formats Lua 5.4,
while `.luacheckrc` limits linted globals to the libraries exposed by the protected
runtime. `.luarc.json` configures LuaLS for Lua 5.4 and leaves formatting to StyLua.
`.editorconfig` supplies shared whitespace rules.

|           | Config          | Tool                            | VS Code                                 |
| --------- | --------------- | ------------------------------- | --------------------------------------- |
| C and C++ | `.clang-format` | clang-format 23                 | on save, through clangd                 |
| CMake     | `.gersemirc`    | gersemi 0.29.2                  | not set up                              |
| JSON      | `.prettierrc`   | Prettier 3.9.8                  | on save, through the Prettier extension |
| YAML      | `.prettierrc`   | Prettier 3.9.8                  | on save, through the Prettier extension |
| Markdown  | `.prettierrc`   | Prettier 3.9.8                  | on save, through the Prettier extension |
| Python    | Ruff defaults   | Ruff 0.16.8                     | on save, through the Ruff extension     |
| Lua       | `.stylua.toml`  | StyLua 2.5.2 and Luacheck 1.2.0 | on save, through the StyLua extension   |

VS Code reads `.clang-format` and `.editorconfig` without an extension. It also
recommends LuaLS for Lua diagnostics.

On macOS, install the Lua command-line tools and the pinned gersemi with:

```sh
brew install stylua luacheck
uv tool install gersemi==0.29.2
```

`pipx install gersemi==0.29.2` works as well. Configure again afterwards so CMake finds it,
or pass `-DGERSEMI_EXECUTABLE=`.

Format first-party CMake, or check it without changing files:

```sh
cmake --build --preset mac-debug --target format-cmake
cmake --build --preset mac-debug --target format-cmake-check
```

Format first-party C++, or check it without changing files:

```sh
cmake --build --preset mac-debug --target format
cmake --build --preset mac-debug --target format-check
```

Format first-party JSON, or check it without changing files:

```sh
cmake --build --preset mac-debug --target format-json
cmake --build --preset mac-debug --target format-json-check
```

Format first-party YAML, or check it without changing files:

```sh
cmake --build --preset mac-debug --target format-yaml
cmake --build --preset mac-debug --target format-yaml-check
```

Format first-party Markdown, or check it without changing files:

```sh
cmake --build --preset mac-debug --target format-markdown
cmake --build --preset mac-debug --target format-markdown-check
```

Format first-party Python, or check its formatting and lint findings:

```sh
cmake --build --preset mac-debug --target format-python
cmake --build --preset mac-debug --target format-python-check lint-python
```

Run the tests for the scripts in `tools/`, as CI does:

```sh
python3 -m unittest discover -s tools -p 'test_*.py'
```

Format first-party Lua, or check its formatting and lint findings:

```sh
cmake --build --preset mac-debug --target format-lua
cmake --build --preset mac-debug --target format-lua-check lint-lua
```

Luacheck only allows what scripts can use in the game. [`.luacheckrc`](.luacheckrc)
lists the allowed globals, and `openSandbox` in
[`scripting/lua_sandbox.cpp`](scripting/lua_sandbox.cpp) opens the same libraries at run
time. Keep the two in step.

The C++ targets skip `external/`; the CMake targets cover `CMakeLists.txt` and `cmake/`;
the JSON targets cover `assets/`, `tests/fixtures/`, `.vscode/`, `CMakePresets.json`,
`.luarc.json` and `.prettierrc`; the YAML targets cover `.github/`, `.clang-format`,
`.clang-tidy`, `.clangd` and `.gersemirc`; the Markdown targets cover the
root documentation and `docs/`; the Python targets cover `tools/`; and the Lua targets
cover `assets/` and `tests/fixtures/`. CMake reports any unavailable tool while
configuring and omits only its targets. Use `-DCLANG_FORMAT_EXECUTABLE=`,
`-DPRETTIER_EXECUTABLE=`, `-DRUFF_EXECUTABLE=`, `-DSTYLUA_EXECUTABLE=`,
`-DLUACHECK_EXECUTABLE=`, or `-DGERSEMI_EXECUTABLE=` to choose a specific one.

CI runs clang-format from LLVM 23, gersemi 0.29.2, Prettier 3.9.8, Ruff 0.16.8, StyLua
2.5.2, and Luacheck 1.2.0, and a pull request cannot merge until their checks pass. clang-format
comes from your LLVM, so it matches CI when your LLVM does; the other tools need not.
If yours format differently, CI fails and you reformat with the commands above.

## Static analysis

The checked-in `.clang-tidy` checks naming, unused and missing includes, common bugs,
and performance mistakes. VS Code's recommended clangd extension reports unused and
missing includes while editing. Treat include-cleaner suggestions as findings to
review; do not automatically remove headers without rebuilding and running the tests.

Static analysis is enforced by CI, and runs locally with the clang-tidy from the same
LLVM as the compiler:

```sh
cmake --build --preset mac-debug --target tidy
```

Pull-request CI checks each changed C++ file, and every first-party file that includes
a changed header, directly or through other headers. Adding a new `.cpp` file and
listing it in its manifest under `cmake/sources/` checks only the new code, but
changing only a manifest checks the whole tree. So do changes to the analysis rules,
the CI workflow, the global build configuration, the third-party libraries under
`external/` (including a submodule moving to another commit), or
`tools/tidy_targets.py`, which picks the files. Local `tidy` builds always check the
whole tree. CMake configuration fails with a focused error if an `app/`, `src/`, or
enabled `tests/` source is missing from its target's manifest.

To see which files CI will check for your branch, run the same script:

```sh
python3 tools/tidy_targets.py --since origin/main
```

CMake finds clang-format and clang-tidy beside the compiler, so they always match its
LLVM version, locally as in CI.

The `header_self_containment` target verifies that public headers include everything
they need themselves:

```sh
cmake --build --preset mac-debug --target header_self_containment
```

## Repository layout

```text
app/           application shell, graphics, UI, and debug tools
  game/        game flow, level transitions, and level composition
  content/     JSON loaders, catalogs, and content validators
assets/        runtime game content
  catalogs/    shared JSON definitions
  levels/      level catalog and maps
  scripts/     Lua NPC activities
  textures/    runtime sprite atlas
cmake/         dependencies, quality rules, and explicit target source manifests
include/       public core headers
scripting/     Lua scripting target: the NPC activity runtime and its sol2 bindings
src/           core implementations
tests/         Catch2 tests for core systems and testable application code
  app/         application tests grouped like app/ (content, debug, game, graphics, UI)
  fixtures/    example content mirroring assets/levels, catalogs, and scripts
  support/     test-only builders and simulation helpers
tools/         repository quality and maintenance scripts
docs/          reading route, architecture, content format, and future work
external/      third-party libraries, as pinned git submodules
.github/       continuous-integration workflow
```

## License

Advanced Platformer is available under the [MIT License](LICENSE). Third-party
dependencies retain their own licenses as documented in
[THIRD_PARTY.md](THIRD_PARTY.md).
