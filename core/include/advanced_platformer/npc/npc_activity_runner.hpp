#pragma once

#include <vector>

#include "advanced_platformer/actor/actor_id.hpp"

namespace advanced_platformer
{
    class NpcActivityScripts;
    struct Actor;
    struct NpcActivity;
    struct NpcBrain;
    struct NpcFacts;
    struct NpcUpdate;
    struct PathFollower;

    // A machine state's Lua activity, run through the update's scripts, which it requires.
    // Each hook hands the script a copied snapshot of the actor, its living target, if it has
    // one, and its facts.

    // Drops the old path, as every activity change does, then runs the script's enter.
    void enterNpcActivity(
        const NpcUpdate& update,
        const Actor& actor,
        const NpcBrain& brain,
        PathFollower& follower,
        const Actor* target,
        const NpcActivity& activity,
        const NpcFacts& facts);

    // Runs the script's update and applies its command as this tick's intentions: a route
    // to follow, a route to drop, an aim, and turning the patrol round.
    void updateNpcActivity(
        const NpcUpdate& update,
        Actor& actor,
        const NpcBrain& brain,
        PathFollower& follower,
        const Actor* target,
        const NpcActivity& activity,
        const NpcFacts& facts);

    void exitNpcActivity(
        const NpcUpdate& update,
        const Actor& actor,
        const NpcBrain& brain,
        const PathFollower& follower,
        const Actor* target,
        const NpcActivity& activity,
        const NpcFacts& facts);

    // Discards script-owned state before queued actor removals are applied to World.
    void forgetNpcActivities(const std::vector<ActorId>& actors, NpcActivityScripts& scripts);
}
