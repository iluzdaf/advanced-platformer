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

    void addNpcScriptFacts(
        const NpcUpdate& update,
        const Actor& actor,
        const NpcBrain& brain,
        const PathFollower& follower,
        const Actor* target,
        NpcFacts& facts);

    void enterNpcActivity(
        const NpcUpdate& update,
        const Actor& actor,
        const NpcBrain& brain,
        PathFollower& follower,
        const Actor* target,
        const NpcActivity& activity,
        const NpcFacts& facts);

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

    void forgetNpcActivities(const std::vector<ActorId>& actors, NpcActivityScripts& scripts);
}
