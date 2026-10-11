import unittest
from pathlib import Path

from tidy_targets import selected_paths, sources


class TidyTargetSelectionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.paths = sources()
        cls.changed_source = Path("core/src/actor/actor_id.cpp")
        cls.manifest = Path("cmake/sources/Core.cmake")

    def test_audio_and_music_sources_are_discovered(self):
        self.assertIn(Path("audio/src/audio_device.cpp"), self.paths)
        self.assertIn(
            Path("audio/include/advanced_platformer/audio/audio_playback.hpp"),
            self.paths,
        )
        self.assertIn(Path("music/main.cpp"), self.paths)

    def test_audio_header_selects_both_executable_users(self):
        selected = selected_paths(
            self.paths,
            [Path("audio/include/advanced_platformer/audio/audio_device.hpp")],
        )
        self.assertIn(Path("app/application.cpp"), selected)
        self.assertIn(Path("music/main.cpp"), selected)

    def test_source_and_manifest_select_only_changed_code(self):
        self.assertEqual(
            selected_paths(self.paths, [self.changed_source, self.manifest]),
            [self.changed_source],
        )

    def test_manifest_only_change_selects_everything(self):
        self.assertEqual(selected_paths(self.paths, [self.manifest]), self.paths)

    def test_global_build_change_selects_everything(self):
        self.assertEqual(
            selected_paths(self.paths, [Path("CMakeLists.txt")]), self.paths
        )

    def test_submodule_update_selects_everything(self):
        self.assertEqual(selected_paths(self.paths, [Path("external/glm")]), self.paths)

    def test_vendored_source_change_selects_everything(self):
        self.assertEqual(
            selected_paths(self.paths, [Path("external/glad/src/glad.c")]), self.paths
        )

    def test_removed_source_selects_nothing(self):
        self.assertEqual(selected_paths(self.paths, [Path("core/src/removed.cpp")]), [])

    def test_removed_source_and_manifest_select_only_changed_code(self):
        self.assertEqual(
            selected_paths(
                self.paths,
                [Path("core/src/removed.cpp"), self.changed_source, self.manifest],
            ),
            [self.changed_source],
        )

    def test_removed_header_selects_everything(self):
        self.assertEqual(
            selected_paths(
                self.paths, [Path("core/include/advanced_platformer/removed.hpp")]
            ),
            self.paths,
        )

    def test_unrelated_change_selects_nothing(self):
        self.assertEqual(selected_paths(self.paths, [Path("README.md")]), [])


if __name__ == "__main__":
    unittest.main()
