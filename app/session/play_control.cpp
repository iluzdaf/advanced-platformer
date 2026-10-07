#include "session/play_control.hpp"

#include <optional>

#include "game.hpp"
#include "advanced_platformer/input/input_state.hpp"
#include "advanced_platformer/timing/fixed_step.hpp"
#include "advanced_platformer/timing/frame_profile.hpp"
#include "advanced_platformer/timing/stopwatch.hpp"

namespace advanced_platformer
{
    void PlayControl::setButton(InputButton button, bool down)
    {
        inputState.setButton(button, down);
    }

    void PlayControl::clearAttackButtons()
    {
        inputState.clearButton(InputButton::PrimaryAttack);
        inputState.clearButton(InputButton::SecondaryAttack);
    }

    void PlayControl::toggleInventory()
    {
        inventoryShown = !inventoryShown;
        interrupted = true;
    }

    void PlayControl::togglePause()
    {
        pausedByUser = !pausedByUser;
        interrupted = true;
    }

    void PlayControl::setPaused(bool paused)
    {
        if (pausedByUser == paused)
        {
            return;
        }

        pausedByUser = paused;
        interrupted = true;
        inputState = {};
    }

    void PlayControl::requestStep()
    {
        if (pausedByUser)
        {
            stepPending = true;
        }
    }

    void PlayControl::interrupt()
    {
        interrupted = true;
    }

    InputIntentions PlayControl::playerIntentions(const Game& game, const PlayFrame& frame)
    {
        InputIntentions intentions = inputState.consumeIntentions();
        const std::optional<glm::vec2> gameCursor = gameplayCursor(frame);
        if (gameCursor.has_value())
        {
            const glm::vec2 aimDirection = game.playerAimDirection(*gameCursor);
            if (aimDirection != glm::vec2{0.0F, 0.0F})
            {
                aim = aimDirection;
            }
        }
        else
        {
            intentions.primaryAttackPressed = false;
            intentions.secondaryAttackPressed = false;
        }
        intentions.aimDirection = aim;
        return intentions;
    }

    void PlayControl::advance(
        Game& game,
        FixedStep& fixedStep,
        const PlayFrame& frame,
        FrameProfile& profile)
    {
        const bool blocked = paused();
        clearBlockedInput(blocked, frame);
        const auto step = [&](float deltaTime)
        { game.update(playerIntentions(game, frame), deltaTime, &profile); };

        if (!blocked && !interrupted)
        {
            const Stopwatch simulationWatch;
            const FixedStepResult stepped = fixedStep.advance(profile.frameSeconds, step);
            profile.simulationTicks = static_cast<int>(stepped.updates);
            profile.simulationSeconds = simulationWatch.elapsedSeconds();
            stepPending = false;
            return;
        }

        fixedStep.reset();
        interrupted = false;
        if (stepPending && !inventoryShown)
        {
            const Stopwatch simulationWatch;
            step(static_cast<float>(fixedStep.stepSeconds()));
            profile.simulationTicks = 1;
            profile.simulationSeconds = simulationWatch.elapsedSeconds();
        }
        stepPending = false;
    }

    bool PlayControl::inventoryOpen() const
    {
        return inventoryShown;
    }

    bool PlayControl::simulationPaused() const
    {
        return pausedByUser;
    }

    bool PlayControl::paused() const
    {
        return inventoryShown || pausedByUser;
    }

    const InputState& PlayControl::input() const
    {
        return inputState;
    }

    std::optional<glm::vec2> PlayControl::gameplayCursor(const PlayFrame& frame) const
    {
        if (frame.uiCapturesMouse || inventoryShown)
        {
            return std::nullopt;
        }
        return frame.internalCursor;
    }

    void PlayControl::clearBlockedInput(bool paused, const PlayFrame& frame)
    {
        if (paused || interrupted || frame.uiCapturesKeyboard)
        {
            inputState = {};
        }
        else if (!gameplayCursor(frame).has_value())
        {
            clearAttackButtons();
        }
    }
}
