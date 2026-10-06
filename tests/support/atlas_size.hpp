#pragma once

#include <glm/vec2.hpp>

namespace tests
{
    // The atlas the test catalogs are loaded against, in pixels. Every fixture region fits.
    constexpr glm::ivec2 AtlasSize{256, 256};
    // The shipped atlas, assets/textures/sprites.png, for tests that load the shipped
    // catalogs.
    constexpr glm::ivec2 ShippedAtlasSize{256, 280};
}
