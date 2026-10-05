#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <glm/vec2.hpp>

#include "advanced_platformer/render/camera.hpp"

namespace
{
    constexpr std::uint32_t Seed = 20260828;

    std::vector<glm::vec2> shakeFor(
        advanced_platformer::CameraShake& shake,
        int steps,
        float deltaTime = 0.01F)
    {
        std::vector<glm::vec2> offsets;
        offsets.reserve(static_cast<std::size_t>(steps));
        for (int step = 0; step < steps; ++step)
        {
            shake.update(deltaTime);
            offsets.push_back(shake.offset());
        }
        return offsets;
    }

    float furthestOf(const std::vector<glm::vec2>& offsets)
    {
        float furthest = 0.0F;
        for (const glm::vec2 offset : offsets)
        {
            furthest = std::max({furthest, std::abs(offset.x), std::abs(offset.y)});
        }
        return furthest;
    }
}

TEST_CASE("A camera shake is still until it is started", "[render][camera][shake]")
{
    advanced_platformer::CameraShake shake(Seed);

    REQUIRE_FALSE(shake.active());
    shake.update(0.01F);
    REQUIRE(shake.offset() == glm::vec2{0.0F, 0.0F});
}

TEST_CASE("A camera shake lasts as long as it was asked to", "[render][camera][shake]")
{
    advanced_platformer::CameraShake shake(Seed);
    shake.start(0.1F, 4.0F);
    REQUIRE(shake.active());

    shakeFor(shake, 9);
    REQUIRE(shake.active());

    shakeFor(shake, 2);
    REQUIRE_FALSE(shake.active());
    shake.update(0.01F);
    REQUIRE(shake.offset() == glm::vec2{0.0F, 0.0F});
}

TEST_CASE("A camera shake stays within its magnitude and fades", "[render][camera][shake]")
{
    advanced_platformer::CameraShake shake(Seed);
    shake.start(1.0F, 4.0F);

    const std::vector<glm::vec2> offsets = shakeFor(shake, 99);
    const float furthest = furthestOf(offsets);
    REQUIRE(furthest <= 4.0F);
    REQUIRE(furthest > 3.0F);

    const std::vector<glm::vec2> firstHalf(offsets.begin(), offsets.begin() + 20);
    const std::vector<glm::vec2> lastSteps(offsets.end() - 10, offsets.end());
    REQUIRE(furthestOf(lastSteps) < furthestOf(firstHalf));
}

TEST_CASE("A camera shake moves both ways on both axes", "[render][camera][shake]")
{
    advanced_platformer::CameraShake shake(Seed);
    shake.start(1.0F, 4.0F);

    bool left = false;
    bool right = false;
    bool up = false;
    bool down = false;
    for (const glm::vec2 offset : shakeFor(shake, 99))
    {
        left = left || offset.x < -1.0F;
        right = right || offset.x > 1.0F;
        up = up || offset.y < -1.0F;
        down = down || offset.y > 1.0F;
    }

    REQUIRE((left && right && up && down));
}

TEST_CASE("Starting a camera shake replaces the one in progress", "[render][camera][shake]")
{
    advanced_platformer::CameraShake shake(Seed);
    shake.start(1.0F, 4.0F);
    shakeFor(shake, 50);

    shake.start(1.0F, 1.0F);

    REQUIRE(furthestOf(shakeFor(shake, 99)) <= 1.0F);
}

TEST_CASE(
    "A camera shake rejects a non-positive duration or magnitude and negative time",
    "[render][camera][shake]")
{
    advanced_platformer::CameraShake shake(Seed);

    REQUIRE_THROWS_AS(shake.start(0.0F, 4.0F), std::invalid_argument);
    REQUIRE_THROWS_AS(shake.start(0.1F, -1.0F), std::invalid_argument);
    REQUIRE_THROWS_AS(shake.update(-0.01F), std::invalid_argument);
    REQUIRE_FALSE(shake.active());
}
