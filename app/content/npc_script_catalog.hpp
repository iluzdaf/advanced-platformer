#pragma once

#include <filesystem>

#include "machine_catalog.hpp"

namespace advanced_platformer
{
    class LuaNpcScripts;

    // Loads each script referenced by a machine from <directory>/<script>.lua, once, then
    // rejects any state whose named activity was not returned by that script.
    void loadNpcActivityScripts(
        LuaNpcScripts& scripts,
        const MachineCatalog& machines,
        const std::filesystem::path& directory);
}
