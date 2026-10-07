#pragma once

#include <optional>

#include <glm/vec2.hpp>

#include "advanced_platformer/input/input_state.hpp"

namespace advanced_platformer
{
    class FixedStep;
    class Game;
    struct FrameProfile;

    struct PlayFrame
    {
        std::optional<glm::vec2> internalCursor;
        bool uiCapturesMouse = false;
        bool uiCapturesKeyboard = false;
    };

    class PlayControl
    {
    public:
        void setButton(InputButton button, bool down);
        void clearAttackButtons();
        void toggleInventory();
        void togglePause();
        void setPaused(bool paused);
        void requestStep();
        void interrupt();
        InputIntentions playerIntentions(const Game& game, const PlayFrame& frame);
        void advance(
            Game& game,
            FixedStep& fixedStep,
            const PlayFrame& frame,
            FrameProfile& profile);

        bool inventoryOpen() const;
        bool simulationPaused() const;
        bool paused() const;
        const InputState& input() const;

    private:
        std::optional<glm::vec2> gameplayCursor(const PlayFrame& frame) const;
        void clearBlockedInput(bool paused, const PlayFrame& frame);

        InputState inputState;
        glm::vec2 aim = {1.0F, 0.0F};
        bool inventoryShown = false;
        bool pausedByUser = false;
        bool stepPending = false;
        bool interrupted = false;
    };
}
