#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec2.hpp>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/input/input_state.hpp"
#include "advanced_platformer/navigation/navigation_path.hpp"
#include "advanced_platformer/npc/npc_activity.hpp"
#include "advanced_platformer/npc/npc.hpp"
#include "advanced_platformer/npc/npc_facts.hpp"

namespace advanced_platformer
{
    struct NpcFooting
    {
        bool left = false;
        bool right = false;
    };

    struct NpcPickupSnapshot
    {
        glm::vec2 feet{0.0F, 0.0F};
        std::string item;
        int quantity = 1;
        std::optional<glm::vec2> breakableBelow;

        bool operator==(const NpcPickupSnapshot&) const = default;
    };

    struct NpcActivitySnapshot
    {
        glm::vec2 feet = {0.0F, 0.0F};
        glm::vec2 center = {0.0F, 0.0F};
        std::optional<Health> health;
        std::optional<glm::vec2> targetFeet;
        glm::vec2 lastKnownTargetFeet = {0.0F, 0.0F};
        std::optional<glm::vec2> targetCenter;
        std::optional<std::string> targetKind;
        std::optional<Patrol> patrol;
        std::optional<NpcFooting> footing;
        NpcFacts facts;
        std::optional<NavigationPathStatus> routeStatus;
        bool routeComplete = false;
        std::optional<glm::vec2> exitFeet;
        std::vector<NpcPickupSnapshot> pickups;
    };

    struct NpcActivityCommand
    {
        InputIntentions intentions;
        std::optional<glm::vec2> routeTo;
        std::optional<glm::vec2> aimAt;
        bool clearRoute = false;
        bool turnPatrol = false;
        std::optional<std::string> useItem;
    };

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
        virtual bool hasFacts(const std::string& script) const = 0;
        virtual std::map<std::string, bool> facts(
            ActorId actor,
            const std::string& script,
            const NpcActivitySnapshot& snapshot,
            float deltaTime) = 0;
        virtual void forget(ActorId actor) = 0;
    };
}
