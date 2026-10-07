#pragma once

#include <optional>

#include <glm/vec2.hpp>

#include "advanced_platformer/actor/actor_id.hpp"

namespace advanced_platformer
{
    class Game;
    class PlayControl;

    struct DebugCursor
    {
        bool breakTileRequested = false;
        std::optional<ActorId> machineActor;
    };

    struct DebugClick
    {
        std::optional<glm::vec2> internalCursor;
        bool mouseClicked = false;
        bool uiCapturesMouse = false;
    };

    bool breakRequestedTile(
        DebugCursor& cursor,
        Game& game,
        const std::optional<glm::vec2>& internalCursor);

    void selectClickedMachineActor(
        DebugCursor& cursor,
        const Game& game,
        PlayControl& play,
        const DebugClick& click);
}
