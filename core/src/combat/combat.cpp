#include "advanced_platformer/combat/combat.hpp"

namespace advanced_platformer
{
    bool areOpponents(Team first, Team second)
    {
        return (first == Team::Player && second == Team::Enemy) ||
               (first == Team::Enemy && second == Team::Player);
    }
}
