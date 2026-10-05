#include "advanced_platformer/movement/pounce.hpp"

#include <algorithm>
#include <stdexcept>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "advanced_platformer/input/input_state.hpp"
#include "advanced_platformer/math/validation.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"
#include "advanced_platformer/physics/body.hpp"
#include "advanced_platformer/physics/collision.hpp"

namespace advanced_platformer
{
    namespace
    {
        bool launches(
            const PlatformerMovement& movement,
            const SurfaceClimb* climb,
            const Pounce& pounce,
            const InputIntentions& intentions)
        {
            const bool resting =
                movement.grounded || (climb != nullptr && climb->surface != ClimbSurface::None);
            return pounce.phase == PouncePhase::Ready && intentions.pouncePressed && resting &&
                   intentions.aimDirection != glm::vec2{0.0F, 0.0F};
        }

        void launch(
            Body& body,
            PlatformerMovement& movement,
            SurfaceClimb* climb,
            Pounce& pounce,
            const InputIntentions& intentions)
        {
            const bool holding = climb != nullptr && climb->surface != ClimbSurface::None;
            body.velocity = glm::normalize(intentions.aimDirection) * pounce.config.speed;
            if (!holding)
            {
                body.velocity.y = std::min(body.velocity.y, -pounce.config.lift);
            }
            pounce.launchedFrom = holding ? climb->surface : ClimbSurface::None;
            if (climb != nullptr)
            {
                climb->surface = ClimbSurface::None;
                climb->wallHeading = WallHeading::Up;
            }
            movement.grounded = false;
            movement.coyoteRemaining = 0.0F;
            movement.jumpBufferRemaining = 0.0F;
            pounce.phase = PouncePhase::Airborne;
        }

        InputIntentions flightIntentions(
            const TileMap& map,
            const Body& body,
            const Pounce& pounce,
            const InputIntentions& intentions,
            bool launchedThisUpdate)
        {
            InputIntentions applied = intentions;
            applied.direction = {0.0F, 0.0F};
            applied.jumpPressed = false;
            applied.jumpHeld = true;
            const bool leavingSurface = pounce.launchedFrom != ClimbSurface::None &&
                                        touchesClimbable(map, body.bounds, pounce.launchedFrom);
            if (launchedThisUpdate || leavingSurface)
            {
                applied.climbGrip = ClimbGrip::Release;
            }
            return applied;
        }
    }

    void validatePounceConfig(const PounceConfig& config)
    {
        if (!isFinitePositive(config.speed) || !isFiniteNonNegative(config.lift) ||
            !isFinitePositive(config.range) || !isFiniteNonNegative(config.recoveryDuration))
        {
            throw std::invalid_argument(
                "A pounce needs a finite positive speed and range, and finite non-negative lift "
                "and recovery");
        }
    }

    CollisionContacts updatePounceMovement(
        const TileMap& map,
        Body& body,
        PlatformerMovement& movement,
        SurfaceClimb* climb,
        Pounce& pounce,
        const InputIntentions& intentions,
        float deltaTime)
    {
        requireSeconds(deltaTime, "Pounce time step");
        validatePounceConfig(pounce.config);
        requireFinite(intentions.aimDirection, "Input intentions");

        const bool launchedThisUpdate = launches(movement, climb, pounce, intentions);
        if (launchedThisUpdate)
        {
            launch(body, movement, climb, pounce, intentions);
        }
        const InputIntentions applied =
            pounce.phase == PouncePhase::Airborne
                ? flightIntentions(map, body, pounce, intentions, launchedThisUpdate)
                : intentions;

        const CollisionContacts contacts =
            climb != nullptr
                ? updateSurfaceClimbMovement(map, body, movement, *climb, applied, deltaTime)
                : updatePlatformerMovement(map, body, movement, applied, deltaTime);

        if (pounce.phase == PouncePhase::Airborne && !launchedThisUpdate)
        {
            const bool holding = climb != nullptr && climb->surface != ClimbSurface::None;
            if (movement.grounded || holding)
            {
                pounce.phase = PouncePhase::Recovery;
                pounce.phaseTimeRemaining = pounce.config.recoveryDuration;
                pounce.launchedFrom = ClimbSurface::None;
            }
        }
        else if (pounce.phase == PouncePhase::Recovery)
        {
            pounce.phaseTimeRemaining = std::max(0.0F, pounce.phaseTimeRemaining - deltaTime);
            if (pounce.phaseTimeRemaining == 0.0F)
            {
                pounce.phase = PouncePhase::Ready;
            }
        }
        return contacts;
    }
}
