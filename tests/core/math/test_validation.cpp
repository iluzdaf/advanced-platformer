#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <limits>
#include <stdexcept>

#include <glm/vec2.hpp>

#include "advanced_platformer/math/validation.hpp"

TEST_CASE("Vector finiteness checks both components", "[math][validation]")
{
    const float infinity = std::numeric_limits<float>::infinity();

    REQUIRE(advanced_platformer::isFinite({1.0F, -2.0F}));
    REQUIRE_FALSE(advanced_platformer::isFinite({infinity, 0.0F}));
    REQUIRE_FALSE(advanced_platformer::isFinite({0.0F, infinity}));
}

TEST_CASE("A time step must be finite and not negative", "[math][validation]")
{
    REQUIRE_NOTHROW(advanced_platformer::requireSeconds(0.0F, "Attacks time step"));
    REQUIRE_NOTHROW(advanced_platformer::requireSeconds(0.25F, "Attacks time step"));
    REQUIRE_THROWS_WITH(
        advanced_platformer::requireSeconds(-0.1F, "Attacks time step"),
        "Attacks time step must be a finite, non-negative number of seconds");
    REQUIRE_THROWS_AS(
        advanced_platformer::requireSeconds(
            std::numeric_limits<float>::infinity(), "Attacks time step"),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        advanced_platformer::requireSeconds(
            std::numeric_limits<float>::quiet_NaN(), "Attacks time step"),
        std::invalid_argument);
}

TEST_CASE("A finite positive number is above zero and not infinite", "[math][validation]")
{
    REQUIRE(advanced_platformer::isFinitePositive(0.5F));
    REQUIRE_FALSE(advanced_platformer::isFinitePositive(0.0F));
    REQUIRE_FALSE(advanced_platformer::isFinitePositive(-0.5F));
    REQUIRE_FALSE(advanced_platformer::isFinitePositive(std::numeric_limits<float>::infinity()));
    REQUIRE_FALSE(advanced_platformer::isFinitePositive(std::numeric_limits<float>::quiet_NaN()));
}

TEST_CASE("A finite non-negative number may be zero but not below it", "[math][validation]")
{
    REQUIRE(advanced_platformer::isFiniteNonNegative(0.0F));
    REQUIRE(advanced_platformer::isFiniteNonNegative(0.5F));
    REQUIRE_FALSE(advanced_platformer::isFiniteNonNegative(-0.5F));
    REQUIRE_FALSE(advanced_platformer::isFiniteNonNegative(std::numeric_limits<float>::infinity()));
    REQUIRE_FALSE(
        advanced_platformer::isFiniteNonNegative(std::numeric_limits<float>::quiet_NaN()));
}

TEST_CASE("A finite non-negative vector may have zero components", "[math][validation]")
{
    REQUIRE(advanced_platformer::isFiniteNonNegative(glm::vec2{0.0F, 0.0F}));
    REQUIRE(advanced_platformer::isFiniteNonNegative(glm::vec2{32.0F, 0.0F}));
    REQUIRE_FALSE(advanced_platformer::isFiniteNonNegative(glm::vec2{-1.0F, 0.0F}));
    REQUIRE_FALSE(
        advanced_platformer::isFiniteNonNegative(
            glm::vec2{0.0F, std::numeric_limits<float>::infinity()}));
}

TEST_CASE("A finite positive vector has both components above zero", "[math][validation]")
{
    const float infinity = std::numeric_limits<float>::infinity();

    REQUIRE(advanced_platformer::isFinitePositive(glm::vec2{12.0F, 20.0F}));
    REQUIRE_FALSE(advanced_platformer::isFinitePositive(glm::vec2{0.0F, 20.0F}));
    REQUIRE_FALSE(advanced_platformer::isFinitePositive(glm::vec2{12.0F, -1.0F}));
    REQUIRE_FALSE(advanced_platformer::isFinitePositive(glm::vec2{infinity, 20.0F}));
}

TEST_CASE("Requiring a finite vector names what it is", "[math][validation]")
{
    REQUIRE_NOTHROW(advanced_platformer::requireFinite({1.0F, -2.0F}, "Camera position"));
    REQUIRE_THROWS_WITH(
        advanced_platformer::requireFinite(
            {std::numeric_limits<float>::quiet_NaN(), 0.0F}, "Camera position"),
        "Camera position must be finite");
}

TEST_CASE("A positive time step rejects zero and non-finite values", "[math][validation]")
{
    REQUIRE_NOTHROW(
        advanced_platformer::requirePositiveSeconds(0.25F, "Navigation simulation step"));
    REQUIRE_THROWS_WITH(
        advanced_platformer::requirePositiveSeconds(0.0F, "Navigation simulation step"),
        "Navigation simulation step must be finite and positive");
    REQUIRE_THROWS_AS(
        advanced_platformer::requirePositiveSeconds(-0.1F, "Navigation simulation step"),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        advanced_platformer::requirePositiveSeconds(
            std::numeric_limits<float>::infinity(), "Navigation simulation step"),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        advanced_platformer::requirePositiveSeconds(
            std::numeric_limits<float>::quiet_NaN(), "Navigation simulation step"),
        std::invalid_argument);
}

TEST_CASE("A length of time must be finite and not negative", "[math][validation]")
{
    REQUIRE_NOTHROW(advanced_platformer::requireSeconds(0.0F, "Frame time"));
    REQUIRE_THROWS_WITH(
        advanced_platformer::requireSeconds(-1.0F, "Frame time"),
        "Frame time must be a finite, non-negative number of seconds");
    REQUIRE_THROWS_AS(
        advanced_platformer::requireSeconds(std::numeric_limits<float>::infinity(), "Frame time"),
        std::invalid_argument);
}
