#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <utility>
#include <string>
#include <vector>

#include "content/game_content.hpp"
#include "game.hpp"
#include "advanced_platformer/combat/combat.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "playtest.hpp"
#include "support/fixed_step.hpp"
#include "support/fixture_game.hpp"

TEST_CASE("A playtest bot follows the route to the exit")
{
    advanced_platformer::Game game = tests::fixtureGame(
        advanced_platformer::playtestContent(
            tests::fixtureContent(), ADVANCED_PLATFORMER_SOURCE_PLAYTEST_ASSETS));

    const std::vector<advanced_platformer::LevelPlaytest> results =
        advanced_platformer::playtestRun(game, 1, 60.0F, tests::FixedStepSeconds);

    REQUIRE(results.size() == 1);
    CHECK(results.front().outcome == advanced_platformer::PlaytestOutcome::Exit);
    CHECK(results.front().level == 1);
    CHECK(game.levelNumber() == 2);
}

TEST_CASE("A playtest samples the level's pacing every half second")
{
    advanced_platformer::Game game = tests::fixtureGame(
        advanced_platformer::playtestContent(
            tests::fixtureContent(), ADVANCED_PLATFORMER_SOURCE_PLAYTEST_ASSETS));

    const std::vector<advanced_platformer::LevelPlaytest> results =
        advanced_platformer::playtestRun(game, 1, 60.0F, tests::FixedStepSeconds);

    REQUIRE(results.size() == 1);
    const std::vector<advanced_platformer::PacingSample>& pacing = results.front().pacing;
    REQUIRE(pacing.size() > 2);
    CHECK(pacing.front().seconds == 0.0F);
    CHECK(pacing.front().health > 0);
    CHECK(pacing.back().seconds == Catch::Approx(results.front().seconds).margin(0.001));
    for (std::size_t sample = 1; sample < pacing.size(); ++sample)
    {
        INFO("sample " << sample);
        CHECK(pacing[sample].seconds > pacing[sample - 1].seconds);
        CHECK(pacing[sample].seconds - pacing[sample - 1].seconds <= Catch::Approx(0.5F));
        CHECK_FALSE(pacing[sample].piece.empty());
    }
}

TEST_CASE("A playtest records the level's tiles as it starts")
{
    advanced_platformer::Game game = tests::fixtureGame(
        advanced_platformer::playtestContent(
            tests::fixtureContent(), ADVANCED_PLATFORMER_SOURCE_PLAYTEST_ASSETS));
    const advanced_platformer::TileMap& map = game.currentLevel().map;
    const auto width = static_cast<std::size_t>(map.width());
    const auto height = static_cast<std::size_t>(map.height());

    const std::vector<advanced_platformer::LevelPlaytest> results =
        advanced_platformer::playtestRun(game, 1, 60.0F, tests::FixedStepSeconds);

    REQUIRE(results.size() == 1);
    const std::vector<std::string>& rows = results.front().map;
    REQUIRE(rows.size() == height);
    for (const std::string& row : rows)
    {
        CHECK(row.size() == width);
        CHECK(row.find_first_not_of("#XG.") == std::string::npos);
    }
    CHECK(rows.front().find('#') != std::string::npos);
    CHECK(std::ranges::any_of(rows, [](const std::string& row) { return row.contains('.'); }));
}

TEST_CASE("A playtest names each pickup the bot collects")
{
    advanced_platformer::Game game = tests::fixtureGame(
        advanced_platformer::playtestContent(
            tests::fixtureContent("tests/fixtures/rooms/pickup_placement/pieces.json"),
            ADVANCED_PLATFORMER_SOURCE_PLAYTEST_ASSETS));

    const std::vector<advanced_platformer::LevelPlaytest> results =
        advanced_platformer::playtestRun(game, 1, 60.0F, tests::FixedStepSeconds);

    REQUIRE(results.size() == 1);
    REQUIRE(results.front().pickupsCollected > 0);
    int named = 0;
    for (const advanced_platformer::PacingSample& sample : results.front().pacing)
    {
        for (const std::string& item : sample.collected)
        {
            ++named;
            CHECK_FALSE(item.empty());
        }
    }
    CHECK(named == results.front().pickupsCollected);
}

TEST_CASE("A playtest records each NPC that starts targeting the bot")
{
    advanced_platformer::GameContent content = tests::fixtureContent();
    content.gameCatalogs.actors.definitions.at("test_guard").team =
        advanced_platformer::Team::Enemy;
    advanced_platformer::Game game = tests::fixtureGame(
        advanced_platformer::playtestContent(
            std::move(content), ADVANCED_PLATFORMER_SOURCE_PLAYTEST_ASSETS));

    const std::vector<advanced_platformer::LevelPlaytest> results =
        advanced_platformer::playtestRun(game, 1, 60.0F, tests::FixedStepSeconds);

    REQUIRE(results.size() == 1);
    int notices = 0;
    for (const advanced_platformer::PacingSample& sample : results.front().pacing)
    {
        for (const advanced_platformer::NpcNotice& notice : sample.noticed)
        {
            ++notices;
            CHECK_FALSE(notice.npc.empty());
            CHECK(notice.cells > 0.0F);
            CHECK(sample.npcsTargeting > 0);
        }
    }
    CHECK(notices > 0);
}

TEST_CASE("A playtest lists each NPC near the bot with its state and offset")
{
    advanced_platformer::GameContent content = tests::fixtureContent();
    content.gameCatalogs.actors.definitions.at("test_guard").team =
        advanced_platformer::Team::Enemy;
    advanced_platformer::Game game = tests::fixtureGame(
        advanced_platformer::playtestContent(
            std::move(content), ADVANCED_PLATFORMER_SOURCE_PLAYTEST_ASSETS));

    const std::vector<advanced_platformer::LevelPlaytest> results =
        advanced_platformer::playtestRun(game, 1, 60.0F, tests::FixedStepSeconds);

    REQUIRE(results.size() == 1);
    int listed = 0;
    for (const advanced_platformer::PacingSample& sample : results.front().pacing)
    {
        CHECK(static_cast<int>(sample.npcs.size()) == sample.npcsNear);
        for (const advanced_platformer::NpcNearby& npc : sample.npcs)
        {
            ++listed;
            CHECK(npc.npc == "test_guard");
            CHECK_FALSE(npc.state.empty());
        }
    }
    CHECK(listed > 0);
}

TEST_CASE("A playtest moves on to the next level after a timeout")
{
    advanced_platformer::Game game = tests::fixtureGame(
        advanced_platformer::playtestContent(
            tests::fixtureContent(), ADVANCED_PLATFORMER_SOURCE_PLAYTEST_ASSETS));

    const std::vector<advanced_platformer::LevelPlaytest> results =
        advanced_platformer::playtestRun(game, 2, tests::FixedStepSeconds, tests::FixedStepSeconds);

    REQUIRE(results.size() == 2);
    CHECK(results[0].outcome == advanced_platformer::PlaytestOutcome::Timeout);
    CHECK(results[0].level == 1);
    CHECK(results[1].level == 2);
    CHECK(results[1].runSeed == results[0].runSeed);
}

TEST_CASE("A playtest calls a level stuck after 15 seconds in no new cell")
{
    advanced_platformer::Game game = tests::fixtureGame(
        advanced_platformer::playtestContent(
            tests::fixtureContent(), "tests/fixtures/standing_bot"));

    const std::vector<advanced_platformer::LevelPlaytest> results =
        advanced_platformer::playtestRun(game, 1, 60.0F, tests::FixedStepSeconds);

    REQUIRE(results.size() == 1);
    CHECK(results.front().outcome == advanced_platformer::PlaytestOutcome::Stuck);
    CHECK(results.front().seconds == Catch::Approx(15.0F).margin(0.1));
}

TEST_CASE("A level playtest is written as one JSON line")
{
    const advanced_platformer::LevelPlaytest level{
        .runSeed = 7,
        .level = 2,
        .levelSeed = 11,
        .outcome = advanced_platformer::PlaytestOutcome::Stuck,
        .damageByNearestNpc = {{"rat", 2}},
        .endCell = {3, 4},
        .map = {"#.", "XG"},
        .pacing = {
            {.seconds = 0.5F,
             .health = 3,
             .damage = 1,
             .npcsNear = 2,
             .npcsTargeting = 1,
             .npcs = {{.npc = "rat", .state = "chase", .right = -1.5F, .above = 2.0F}},
             .noticed = {{.npc = "rat", .cells = 2.5F}},
             .collected = {"Health potion"},
             .piece = "den",
             .cell = {5, 6}}}};

    const std::string text = advanced_platformer::formatLevelPlaytest(level);

    CHECK(text.find('\n') == std::string::npos);
    CHECK(text.find(R"("outcome":"stuck")") != std::string::npos);
    CHECK(text.find(R"("damageByNearestNpc":{"rat":2})") != std::string::npos);
    CHECK(text.find(R"("endCell":[3,4])") != std::string::npos);
    CHECK(text.find(R"("map":["#.","XG"])") != std::string::npos);
    CHECK(
        text.find(
            R"("pacing":[{"seconds":0.5,"health":3,"damage":1,"npcsNear":2,"npcsTargeting":1,)"
            R"("npcs":[{"npc":"rat","state":"chase","right":-1.5,"above":2}],)"
            R"("noticed":[{"npc":"rat","cells":2.5}],"collected":["Health potion"],"piece":"den",)"
            R"("cell":[5,6]}])") != std::string::npos);
}
