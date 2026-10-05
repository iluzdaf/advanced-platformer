#include "advanced_platformer/render/presentation.hpp"

#include <optional>

#include "advanced_platformer/render/animation_system.hpp"
#include "advanced_platformer/render/camera.hpp"
#include "advanced_platformer/render/cover_fade.hpp"
#include "advanced_platformer/render/presentation_scripts.hpp"
#include "advanced_platformer/world/level_exit.hpp"
#include "advanced_platformer/world/world.hpp"

namespace advanced_platformer
{
    void updateWorldPresentation(
        const TileMap& map,
        World& world,
        float deltaTime,
        PresentationScripts& scripts,
        CameraShake& shake)
    {
        for (const WorldEvent& event : world.takeEvents())
        {
            const PresentationEffects effects =
                scripts.onEvent(event, event.actor == world.playerId());
            if (effects.shake.has_value())
            {
                shake.start(effects.shake->duration, effects.shake->magnitude);
            }
        }
        shake.update(deltaTime);
        if (exitOpening(world))
        {
            return;
        }
        updateWorldAnimations(world, deltaTime);
        updateCoverFades(map, world, deltaTime);
    }
}
