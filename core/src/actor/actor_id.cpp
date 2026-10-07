#include "advanced_platformer/actor/actor_id.hpp"

namespace advanced_platformer
{
    bool isValid(ActorId id)
    {
        return id.value != 0;
    }
}
