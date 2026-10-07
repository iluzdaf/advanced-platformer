#pragma once

#include <array>
#include <cstddef>

#include <glm/vec2.hpp>

namespace advanced_platformer
{
    enum class InputButton
    {
        Left,
        Right,
        Up,
        Down,
        Jump,
        PrimaryAttack,
        SecondaryAttack,
        Count
    };

    enum class ClimbGrip
    {
        Keep,
        Hold,
        Release
    };

    struct InputIntentions
    {
        glm::vec2 direction = {0.0F, 0.0F};
        glm::vec2 aimDirection = {0.0F, 0.0F};
        bool jumpPressed = false;
        bool jumpHeld = false;
        bool primaryAttackPressed = false;
        bool secondaryAttackPressed = false;
        ClimbGrip climbGrip = ClimbGrip::Keep;
        bool avoidLedges = false;

        bool operator==(const InputIntentions&) const = default;
    };

    class InputState
    {
    public:
        void setButton(InputButton button, bool down);
        void clearButton(InputButton button);

        bool isHeld(InputButton button) const;
        bool wasPressed(InputButton button) const;
        bool wasReleased(InputButton button) const;

        InputIntentions consumeIntentions();

    private:
        static constexpr std::size_t ButtonCount = static_cast<std::size_t>(InputButton::Count);

        std::array<bool, ButtonCount> held{};
        std::array<bool, ButtonCount> pressed{};
        std::array<bool, ButtonCount> released{};
    };
}
