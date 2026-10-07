#pragma once

#include <filesystem>

#include "advanced_platformer/level/room_pieces.hpp"

namespace advanced_platformer
{
    RoomPieces loadRoomPieceCatalog(const std::filesystem::path& path);
}
