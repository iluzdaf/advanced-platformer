import unittest

from format import formatter_for


class FormatterSelectionTests(unittest.TestCase):
    def assertFormatter(self, expected, paths):
        for path in paths:
            with self.subTest(path=path):
                self.assertEqual(formatter_for(path), expected)

    def test_first_party_cpp(self):
        self.assertFormatter(
            "clang-format",
            [
                "app/application.cpp",
                "scripting/lua_npc_scripts.hpp",
                "core/src/npc/npc_system.cpp",
                "core/include/advanced_platformer/render/sprite.hpp",
                "tests/support/cell_connections.hpp",
            ],
        )

    def test_cmake(self):
        self.assertFormatter("gersemi", ["CMakeLists.txt", "cmake/Coverage.cmake"])

    def test_json_yaml_and_markdown(self):
        self.assertFormatter(
            "prettier",
            [
                "assets/catalogs/pickups.json",
                "tests/fixtures/rooms/locked_door/pieces/exit.json",
                ".vscode/settings.json",
                "CMakePresets.json",
                ".luarc.json",
                ".prettierrc",
                ".github/workflows/ci.yml",
                ".github/actions/checkout-submodules/action.yml",
                ".clang-format",
                ".clang-tidy",
                ".clangd",
                ".gersemirc",
                "README.md",
                "docs/ARCHITECTURE.md",
            ],
        )

    def test_python(self):
        self.assertFormatter("ruff", ["tools/format.py"])

    def test_lua(self):
        self.assertFormatter(
            "stylua",
            ["assets/scripts/rat.lua", "tests/fixtures/scripts/example_npc.lua"],
        )

    def test_files_the_format_targets_skip(self):
        self.assertFormatter(
            None,
            [
                "external/glm/glm/glm.hpp",
                "docs/vendored.cpp",
                "core/src/CMakeLists.txt",
                "tests/CMakeLists.txt",
                "build/mac-debug/compile_commands.json",
                "external/catch2/README.md",
                "tests/README.md",
                "assets/textures/sprites.png",
                "tools/hooks/notes.md",
                "LICENSE",
            ],
        )


if __name__ == "__main__":
    unittest.main()
