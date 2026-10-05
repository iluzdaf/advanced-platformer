#pragma once

#include "advanced_platformer/render/animation.hpp"

namespace tests
{
    inline advanced_platformer::AnimationClip clip(
        advanced_platformer::AnimationName name,
        float left)
    {
        return {name, {{{left, 0.0F}, {1.0F, 1.0F}}}, 0.1F, true};
    }

    inline advanced_platformer::Animator fullAnimator()
    {
        using advanced_platformer::AnimationName;

        advanced_platformer::Animator animator;
        animator.animationSet = {{
            clip(AnimationName::Idle, 0.0F),
            clip(AnimationName::Move, 1.0F),
            clip(AnimationName::Jump, 2.0F),
            clip(AnimationName::Fall, 3.0F),
            clip(AnimationName::Shoot, 4.0F),
            clip(AnimationName::Bite, 5.0F),
            clip(AnimationName::Pounce, 6.0F),
            clip(AnimationName::Death, 7.0F),
        }};
        return animator;
    }
}
