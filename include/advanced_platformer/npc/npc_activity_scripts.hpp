#pragma once

#include <map>
#include <optional>
#include <string>

#include <glm/vec2.hpp>

#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/input/input_state.hpp"
#include "advanced_platformer/npc/npc_activity.hpp"
#include "advanced_platformer/npc/npc.hpp"
#include "advanced_platformer/npc/npc_facts.hpp"

namespace advanced_platformer
{
    // Whether a walker could stand one body width to each side of where it stands.
    struct NpcFooting
    {
        bool left = false;
        bool right = false;
    };

    // The engine-owned knowledge copied into one script update. Lua can change its copy,
    // but none of those changes reach the simulation.
    struct NpcActivitySnapshot
    {
        glm::vec2 feet = {0.0F, 0.0F};
        // The centre of the NPC's body, where a shot leaves from.
        glm::vec2 center = {0.0F, 0.0F};
        // Where the target is known to be, only while it is known.
        std::optional<glm::vec2> targetFeet;
        // Where the target was last seen or heard, kept after it is forgotten, so a search
        // can go there. It is {0, 0} until the NPC has had a target.
        glm::vec2 lastKnownTargetFeet = {0.0F, 0.0F};
        // The centre of the living target's body, where it is now, seen or not.
        std::optional<glm::vec2> targetCenter;
        // The authored ends of this NPC's run, when it has one, and which it is heading for.
        // Scripts may choose a goal between them, but pathfinding and movement remain
        // engine work.
        std::optional<Patrol> patrol;
        // Only a walker has footing; a flyer has none.
        std::optional<NpcFooting> footing;
        NpcFacts facts;
        // Whether the engine holds a route: one being followed, or followed to its end.
        bool hasRoute = false;
        // Whether the route last asked for with routeTo has been followed to its end.
        bool routeComplete = false;
        std::map<std::string, float> tuning;
    };

    // What a script asks the engine to attempt. Applying the command remains C++ work.
    struct NpcActivityCommand
    {
        InputIntentions intentions;
        std::optional<glm::vec2> routeTo;
        std::optional<glm::vec2> aimAt;
        bool clearRoute = false;
        // Turns the NPC's patrol round, to head for its other end.
        bool turnPatrol = false;
    };

    // The core-facing boundary. Tests can provide a fake without loading Lua.
    class NpcActivityScripts
    {
    public:
        virtual ~NpcActivityScripts() = default;

        virtual void enter(
            ActorId actor,
            const NpcActivity& activity,
            const NpcActivitySnapshot& snapshot) = 0;
        virtual NpcActivityCommand update(
            ActorId actor,
            const NpcActivity& activity,
            const NpcActivitySnapshot& snapshot,
            float deltaTime) = 0;
        virtual void exit(
            ActorId actor,
            const NpcActivity& activity,
            const NpcActivitySnapshot& snapshot) = 0;
        virtual void forget(ActorId actor) = 0;
    };
}
