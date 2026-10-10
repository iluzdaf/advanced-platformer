#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>

#include "advanced_platformer/world/world.hpp"
#include "content/game_content.hpp"

TEST_CASE(
    "Shipped sound patches render finite samples and presentation hooks resolve their effects",
    "[audio][content][lua]")
{
    auto content = advanced_platformer::loadGameContent("assets");
    for (const auto& [name, sound] : content.gameCatalogs.sounds)
    {
        INFO(name);
        REQUIRE_FALSE(sound->samples.empty());
        REQUIRE(
            std::ranges::all_of(sound->samples, [](float value) { return std::isfinite(value); }));
    }
    for (auto kind :
         {advanced_platformer::WorldEventKind::Shot,
          advanced_platformer::WorldEventKind::Landing,
          advanced_platformer::WorldEventKind::Knockback})
    {
        for (bool player : {true, false})
        {
            const auto effects = content.presentation.onEvent({{}, {}, kind}, player);
            if (effects.sound.has_value())
            {
                REQUIRE(content.gameCatalogs.sounds.contains(effects.sound->name));
            }
            REQUIRE(content.presentation.diagnostics().empty());
        }
    }
}
