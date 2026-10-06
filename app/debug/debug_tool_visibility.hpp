#pragma once

namespace advanced_platformer
{
    struct DebugToolVisibility
    {
        bool frameProfileDetails = false;
        bool worldAndCameraOverlay = false;
        bool actorText = false;
        bool navigationCacheText = false;
        bool stateMachine = false;
        bool console = false;
    };
}
