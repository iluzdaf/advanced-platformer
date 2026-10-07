#include "debug/debug_cursor.hpp"

#include <optional>

#include "game.hpp"
#include "session/play_control.hpp"
#include "advanced_platformer/actor/actor_id.hpp"

namespace advanced_platformer
{
    bool breakRequestedTile(
        DebugCursor& cursor,
        Game& game,
        const std::optional<glm::vec2>& internalCursor)
    {
        if (!cursor.breakTileRequested)
        {
            return false;
        }

        cursor.breakTileRequested = false;
        return internalCursor.has_value() && game.breakTileAt(*internalCursor);
    }

    void selectClickedMachineActor(
        DebugCursor& cursor,
        const Game& game,
        PlayControl& play,
        const DebugClick& click)
    {
        if (!click.internalCursor.has_value() || !click.mouseClicked || click.uiCapturesMouse)
        {
            return;
        }

        const std::optional<ActorId> clicked = game.machineActorAt(*click.internalCursor);
        if (!clicked.has_value())
        {
            return;
        }

        if (cursor.machineActor == clicked)
        {
            cursor.machineActor.reset();
        }
        else
        {
            cursor.machineActor = clicked;
        }
        play.clearAttackButtons();
    }
}
