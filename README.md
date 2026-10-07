# Advanced Platformer

Advanced Platformer is a C++26 engine and example game built from independently
testable systems. The current implementation includes platformer movement, tile collision,
scrolling, composed actors, NPC state machines with Lua activities, flying and platformer
pathfinding, projectiles, animation, inventory, automatic pickups, an endless run of
generated levels, and ImGui debugging tools.

## Documentation

| Document                                | What it covers                                                                    |
| --------------------------------------- | --------------------------------------------------------------------------------- |
| [ARCHITECTURE.md](docs/ARCHITECTURE.md) | Design, ownership rules, runtime flow, and the reasons behind the main decisions. |
| [CONTENT.md](docs/CONTENT.md)           | How to author levels, definitions, machines, and NPC scripts under `game/assets`. |
| [GLOSSARY.md](docs/GLOSSARY.md)         | The words the code and documents use, each with one meaning.                      |

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

Debug builds read `game/assets/` from the source tree and reload it while the game runs when a
file there changes. See [Hot reload](docs/CONTENT.md#hot-reload).

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

## Playtest runner

The playtest runner plays a run without a window and prints one JSON line per level.
Give it a run seed, the number of levels, and optionally a time limit per level in
seconds (120 by default):

```sh
cmake --build --preset mac-debug --target advanced_platformer_playtest
build/mac-debug/advanced_platformer_playtest 1 5
```

[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md#playtest) describes the bot and each field.

## Playing the example game

| Action                                              | Controls                                  |
| --------------------------------------------------- | ----------------------------------------- |
| Move                                                | A and D, or the left and right arrow keys |
| Jump                                                | W, Up, or Space                           |
| Aim                                                 | Mouse                                     |
| Primary attack                                      | Left mouse button                         |
| Secondary attack                                    | Right mouse button                        |
| Collect an item                                     | Walk over it                              |
| Open or close the inventory (pauses the game)       | Q, or click the bag at the bottom-left    |
| Drink a health potion                               | Click it in the open inventory            |
| Restart the current level, keeping health and items | F5                                        |
| Generate the current level again from the next seed | F6                                        |
| Pause or resume the simulation                      | P                                         |
| Run one simulation step while paused                | . (full stop)                             |
| Toggle the debug overlay                            | F1                                        |
| Close the window                                    | Escape                                    |

The hearts at the top-left show the player's current and maximum health.

Reach the bunker door to move on to the next level. Dying starts a new run at level 1.

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
| Show or hide the console                          | 6                                     |
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
| Coverage                     | `ubuntu-24.04` | Builds with coverage instrumentation, runs the tests, and summarises which of the project's lines they ran. Report only.                              | pushes to `main` and pull requests |

The jobs other than build and test, and coverage, are skipped on pushes because branch protection
already ran them on the pull request.

Every job runs on Linux and installs the same toolchain through
[`install-toolchain`](.github/actions/install-toolchain/action.yml): CMake 4.4.3 from
PyPI and LLVM 23 from apt.llvm.org, with its libc++, clang-format and clang-tidy, rather
than the runner's own. The jobs configure with the `linux-debug` preset, which builds
GLFW without a display backend, so nothing needs a window.

Every job restores the submodules through
[`checkout-submodules`](.github/actions/checkout-submodules/action.yml), from a cache keyed
on the tree of `external/`, which changes whenever a submodule moves. Only a miss fetches
them, shallowly.

The build and test job uses a pinned `sccache` release backed by GitHub Actions'
cache service. Only compiler outputs are cached; generated build directories are not.
None of this affects local builds.

## Coverage

Coverage uses Clang's source-based coverage and the `llvm-profdata` and `llvm-cov` beside
the compiler. Configure the coverage preset, build, and run the `coverage` target:

```sh
cmake --preset mac-coverage
cmake --build --preset mac-coverage
cmake --build --preset mac-coverage --target coverage
```

It runs the tests under instrumentation and prints line, function, region and branch
coverage for `app/`, `core/`, `game/`, `playtest/` and `scripting/`, leaving out `external/` and
the tests themselves. The line-by-line report is at
`build/mac-coverage/coverage/html/index.html`. Only code built into the test executable is
counted, so the window, renderer and ImGui code does not appear. The table and the HTML report are `llvm-cov`'s own output; Clang's [Interpreting reports](https://clang.llvm.org/docs/SourceBasedCodeCoverage.html#interpreting-reports) explains their regions, functions, lines and branches.

## Formatting

Format every first-party file, check them as CI does, or turn on the pre-commit hook:

```sh
python3 tools/format.py
python3 tools/format.py --check
git config core.hooksPath .githooks
```

| Files                | Formatter       | Config          | Install on macOS                  | VS Code on save    |
| -------------------- | --------------- | --------------- | --------------------------------- | ------------------ |
| C and C++            | clang-format 23 | `.clang-format` | `brew install llvm`               | clangd             |
| CMake                | gersemi 0.29.2  | `.gersemirc`    | `uv tool install gersemi==0.29.2` | gersemi extension  |
| JSON, YAML, Markdown | Prettier 3.9.8  | `.prettierrc`   | `brew install prettier`           | Prettier extension |
| Python               | Ruff 0.16.8     | Ruff defaults   | `uv tool install ruff==0.16.8`    | Ruff extension     |
| Lua                  | StyLua 2.5.2    | `.stylua.toml`  | `brew install stylua`             | StyLua extension   |

The versions are the ones CI runs. clang-format has to match, so `tools/format.py` treats
any other version as missing. The other tools may format differently at another version,
and CI catches that. `uv tool` installs into `~/.local/bin`; `uv tool update-shell` puts
it on your `PATH`. `.editorconfig` supplies shared whitespace rules.

[`tools/format.py`](tools/format.py) decides which files each tool covers, skipping
`external/` and anything git does not track. A missing tool fails the run.

The hook runs `tools/format.py --staged`, which formats the staged files and stages them.

- A file with unstaged edits is left as staged and named, so the rest of your edits stay
  out of the commit.
- A missing tool is skipped and named.
- A formatter that fails, for example on a file that does not parse, stops the commit.

`git commit --no-verify` skips the hook.

## Linting

| Files  | Linter         | Config        | Install on macOS        |
| ------ | -------------- | ------------- | ----------------------- |
| Python | Ruff 0.16.8    | Ruff defaults | as above                |
| Lua    | Luacheck 1.2.0 | `.luacheckrc` | `brew install luacheck` |

```sh
cmake --build --preset mac-debug --target lint-python lint-lua
```

CMake leaves out the target of a linter it cannot find. `-DRUFF_EXECUTABLE=` or
`-DLUACHECK_EXECUTABLE=` picks a specific one.

Luacheck only allows what scripts can use in the game. [`.luacheckrc`](.luacheckrc)
lists the allowed globals, and `openSandbox` in
[`scripting/lua_sandbox.cpp`](scripting/lua_sandbox.cpp) opens the same libraries at run
time. Keep the two in step. `.luarc.json` configures LuaLS, which VS Code recommends for
Lua diagnostics.

Run the tests for the scripts in `tools/`, as CI does:

```sh
python3 -m unittest discover -s tools -p 'test_*.py'
```

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
whole tree. CMake configuration fails with a focused error if an `app/`, `core/`, `game/`,
`playtest/`, or enabled `tests/` source is missing from its target's manifest.

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
app/           application shell, content session, graphics, UI, and debug tools
cmake/         dependencies, quality rules, and explicit target source manifests
core/          the simulation and level generation library: include/ holds its public headers, src/ its implementations
game/          the headless game library: content loaders and catalogs, level composition, Game, diagnostics
  assets/      runtime game content: catalogs (room pieces in catalogs/pieces), Lua scripts, the atlas
playtest/      headless playtest runner, with the bot's own content in assets/
scripting/     Lua scripting target: the NPC activity runtime and its sol2 bindings
tests/         Catch2 tests, grouped like the code they test (core, game, app, playtest, scripting)
  fixtures/    example content mirroring game/assets catalogs and scripts, with room piece sets in rooms/
  support/     test-only builders and simulation helpers
tools/         repository quality and maintenance scripts
docs/          architecture, content format, and glossary
external/      third-party libraries, as pinned git submodules
.github/       continuous-integration workflow
.githooks/     opt-in git hooks
```

## License

Advanced Platformer is available under the [MIT License](LICENSE). Third-party
dependencies retain their own licenses as documented in
[THIRD_PARTY.md](THIRD_PARTY.md).
