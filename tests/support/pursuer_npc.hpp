#pragma once

#include "advanced_platformer/npc/npc_state_machine.hpp"
#include "lua_npc_scripts.hpp"
#include "support/npc_machine_builder.hpp"

namespace tests
{
    // A fixed fixture for world tests, not the shipped enemies' tunable policy: an NPC that
    // patrols, chases what it knows of and shoots what it sees.
    inline void loadPursuerScript(advanced_platformer::LuaNpcScripts& scripts)
    {
        scripts.loadScriptText("pursuer", R"(
                local function patrolGoal(patrol, headingToSecond)
                    return headingToSecond and patrol.secondFeet or patrol.firstFeet
                end
                return {activities = {
                    idle = {update = function() return nil end},
                    patrol = {update = function(self, snapshot)
                        local patrol = snapshot.patrol
                        if snapshot.routeComplete then
                            return {turnPatrol = true, clearRoute = true,
                                    routeTo = patrolGoal(patrol, not patrol.headingToSecond)}
                        end
                        return {routeTo = patrolGoal(patrol, patrol.headingToSecond)}
                    end},
                    chase = {update = function(self, snapshot)
                        local goal = snapshot.lastKnownTargetFeet
                        return {routeTo = goal, aimAt = goal}
                    end},
                    shoot = {update = function(self, snapshot)
                        return {aimDirection = snapshot.targetCenter - snapshot.center,
                                primaryAttackPressed = true}
                    end}
                }}
            )");
    }

    // The machine that runs the activities loadPursuerScript loads.
    inline advanced_platformer::NpcStateMachine pursuerMachine()
    {
        return NpcMachineBuilder::named("pursuer")
            .state("idle", {"pursuer", "idle"})
            .state("patrol", {"pursuer", "patrol"})
            .state("chase", {"pursuer", "chase"})
            .state("shoot", {"pursuer", "shoot"})
            .transition("idle", "shoot")
            .when("targetInSights", true)
            .transition("patrol", "shoot")
            .when("targetInSights", true)
            .transition("idle", "chase")
            .when("targetKnown", true)
            .transition("patrol", "chase")
            .when("targetKnown", true)
            .transition("idle", "patrol")
            .when("hasPatrol", true)
            .transition("chase", "shoot")
            .when("targetInSights", true)
            .transition("shoot", "chase")
            .when("targetInSights", false)
            .transition("chase", "patrol")
            .when("targetKnown", false)
            .when("hasPatrol", true)
            .transition("chase", "idle")
            .when("targetKnown", false);
    }
}
