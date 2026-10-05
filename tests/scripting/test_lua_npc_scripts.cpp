#include <initializer_list>

#include <catch2/catch_test_macros.hpp>
#include <glm/vec2.hpp>

#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/input/input_state.hpp"
#include "advanced_platformer/npc/npc.hpp"
#include "advanced_platformer/npc/npc_activity.hpp"
#include "advanced_platformer/npc/npc_activity_scripts.hpp"
#include "lua_npc_scripts.hpp"

namespace
{
    using advanced_platformer::ActorId;
    using advanced_platformer::LuaNpcScripts;
    using advanced_platformer::NpcActivity;
    using advanced_platformer::NpcActivitySnapshot;

    constexpr ActorId FirstActor{1};
    constexpr ActorId SecondActor{2};
    const NpcActivity Activity{"example", "decide"};

    NpcActivitySnapshot commandSnapshot()
    {
        NpcActivitySnapshot snapshot;
        snapshot.feet = {12.0F, 34.0F};
        snapshot.targetFeet = {{56.0F, 78.0F}};
        snapshot.patrol = advanced_platformer::Patrol{{8.0F, 34.0F}, {80.0F, 34.0F}, true};
        snapshot.facts.targetKnown = true;
        snapshot.facts.heardLanding = true;
        snapshot.facts.targetOnSameSurface = true;
        snapshot.facts.targetWithinNoticeDistance = true;
        snapshot.facts.targetWithinStandoffDistance = true;
        snapshot.facts.movementBlocked = true;
        snapshot.facts.stateElapsed = 0.25F;
        snapshot.routeComplete = true;
        return snapshot;
    }
}

TEST_CASE("A Lua activity reads a copied snapshot and returns a command", "[lua][npc]")
{
    LuaNpcScripts scripts;
    scripts.loadScriptText(
        "example",
        R"(
            return {
                activities = {
                    decide = {
                        update = function(self, snapshot, dt)
                            snapshot.feet.x = 999
                            return {
                                direction = {x = 3 * dt, y = 0},
                                aimAt = snapshot.targetFeet,
                                routeTo = snapshot.patrol.secondFeet,
                                primaryAttackPressed = snapshot.facts.targetKnown,
                                climbGrip = snapshot.facts.targetKnown and "hold" or "release",
                                jumpHeld = snapshot.facts.heardLanding and snapshot.facts.targetOnSameSurface,
                                jumpPressed = snapshot.facts.movementBlocked,
                                avoidLedges = snapshot.facts.targetWithinStandoffDistance,
                                contactDamage = snapshot.facts.targetWithinNoticeDistance,
                                clearRoute = snapshot.routeComplete
                            }
                        end
                    }
                }
            }
        )",
        "command.lua");
    NpcActivitySnapshot snapshot = commandSnapshot();
    scripts.enter(FirstActor, Activity, snapshot);

    const advanced_platformer::NpcActivityCommand command =
        scripts.update(FirstActor, Activity, snapshot, 0.5F);

    REQUIRE(command.intentions.direction.x == 1.5F);
    REQUIRE(command.intentions.direction.y == 0.0F);
    REQUIRE(command.intentions.primaryAttackPressed);
    REQUIRE(command.intentions.climbGrip == advanced_platformer::ClimbGrip::Hold);
    REQUIRE(command.intentions.jumpHeld);
    REQUIRE(command.intentions.jumpPressed);
    REQUIRE(command.intentions.avoidLedges);
    REQUIRE(command.intentions.contactDamage);
    REQUIRE(command.aimAt == snapshot.targetFeet);
    REQUIRE(command.routeTo == glm::vec2{80.0F, 34.0F});
    REQUIRE(command.clearRoute);
    REQUIRE(snapshot.feet.x == 12.0F);
    REQUIRE(scripts.diagnostics().empty());
}

TEST_CASE(
    "A snapshot's centres, footing, route and patrol heading reach Lua as fields",
    "[lua][npc]")
{
    LuaNpcScripts scripts;
    scripts.loadScriptText(
        "example",
        R"(
            return {
                activities = {
                    decide = {
                        update = function(self, snapshot)
                            if snapshot.footing == nil then
                                return { jumpPressed = snapshot.targetCenter == nil }
                            end
                            return {
                                direction = snapshot.center,
                                aimAt = snapshot.targetCenter,
                                routeTo = snapshot.lastKnownTargetFeet,
                                clearRoute = snapshot.hasRoute,
                                turnPatrol = snapshot.footing.left and not snapshot.footing.right
                                    and snapshot.patrol.headingToSecond
                            }
                        end
                    }
                }
            }
        )",
        "command.lua");

    NpcActivitySnapshot walker = commandSnapshot();
    walker.center = {12.0F, 28.0F};
    walker.targetCenter = {{56.0F, 72.0F}};
    walker.lastKnownTargetFeet = {50.0F, 78.0F};
    walker.footing = advanced_platformer::NpcFooting{true, false};
    walker.hasRoute = true;
    scripts.enter(FirstActor, Activity, walker);
    const advanced_platformer::NpcActivityCommand command =
        scripts.update(FirstActor, Activity, walker, 0.5F);
    REQUIRE(command.intentions.direction == glm::vec2{12.0F, 28.0F});
    REQUIRE(command.aimAt == glm::vec2{56.0F, 72.0F});
    REQUIRE(command.routeTo == glm::vec2{50.0F, 78.0F});
    REQUIRE(command.clearRoute);
    REQUIRE(command.turnPatrol);

    NpcActivitySnapshot flyer = commandSnapshot();
    scripts.enter(SecondActor, Activity, flyer);
    REQUIRE(scripts.update(SecondActor, Activity, flyer, 0.5F).intentions.jumpPressed);
    REQUIRE(scripts.diagnostics().empty());
}

TEST_CASE("Lua receives independent surface and range facts", "[lua][npc]")
{
    LuaNpcScripts scripts;
    scripts.loadScriptText("example", R"(
        return {activities={decide={update=function(self, snapshot)
            return {jumpHeld=snapshot.facts.targetOnSameSurface,
                    contactDamage=snapshot.facts.targetWithinNoticeDistance}
        end}}}
    )");
    NpcActivitySnapshot snapshot;
    scripts.enter(FirstActor, Activity, snapshot);
    for (const bool sameSurface : {false, true})
    {
        for (const bool withinRange : {false, true})
        {
            snapshot.facts.targetOnSameSurface = sameSurface;
            snapshot.facts.targetWithinNoticeDistance = withinRange;
            const auto command = scripts.update(FirstActor, Activity, snapshot, 0.1F);
            REQUIRE(command.intentions.jumpHeld == sameSurface);
            REQUIRE(command.intentions.contactDamage == withinRange);
        }
    }
    REQUIRE(scripts.diagnostics().empty());
}

TEST_CASE("Each actor and each visit has its own Lua activity memory", "[lua][npc]")
{
    LuaNpcScripts scripts;
    scripts.loadScriptText(
        "example",
        R"(
            return {
                activities = {
                    decide = {
                        enter = function(self) self.updates = 10 end,
                        update = function(self)
                            self.updates = self.updates + 1
                            return {direction = {x = self.updates, y = 0}}
                        end
                    }
                }
            }
        )");
    const NpcActivitySnapshot snapshot;
    scripts.enter(FirstActor, Activity, snapshot);
    scripts.enter(SecondActor, Activity, snapshot);

    REQUIRE(scripts.update(FirstActor, Activity, snapshot, 0.1F).intentions.direction.x == 11.0F);
    REQUIRE(scripts.update(FirstActor, Activity, snapshot, 0.1F).intentions.direction.x == 12.0F);
    REQUIRE(scripts.update(SecondActor, Activity, snapshot, 0.1F).intentions.direction.x == 11.0F);

    scripts.exit(FirstActor, Activity, snapshot);
    scripts.enter(FirstActor, Activity, snapshot);
    REQUIRE(scripts.update(FirstActor, Activity, snapshot, 0.1F).intentions.direction.x == 11.0F);

    scripts.forget(SecondActor);
    REQUIRE(scripts.update(SecondActor, Activity, snapshot, 0.1F).intentions.direction.x == 0.0F);
    REQUIRE(scripts.diagnostics().back().message == "activity was not entered");
}
