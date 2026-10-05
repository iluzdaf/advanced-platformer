#pragma once

#include <vector>

#include <glm/vec2.hpp>

#include "advanced_platformer/render/sprite.hpp"

namespace advanced_platformer
{
    enum class AnimationName
    {
        Idle,
        Move,
        Jump,
        Fall,
        Shoot,
        Bite,
        Pounce,
        Death
    };

    struct AnimationState
    {
        bool dying = false;
        bool shooting = false;
        bool biting = false;
        bool pouncing = false;
        bool grounded = true;
        glm::vec2 velocity = {0.0F, 0.0F};
    };

    struct AnimationClip
    {
        AnimationName name = AnimationName::Idle;
        std::vector<SpriteRegion> frames;
        float frameDuration = 0.1F;
        bool looping = true;
    };

    struct AnimationSet
    {
        std::vector<AnimationClip> clips;
    };

    struct Animator
    {
        AnimationName current = AnimationName::Idle;
        float elapsed = 0.0F;
        AnimationSet animationSet;
    };

    const AnimationClip* findClip(const AnimationSet& animationSet, AnimationName name);
    const AnimationClip& clipFor(const AnimationSet& animationSet, AnimationName name);
    const SpriteRegion& frameAt(const AnimationClip& clip, float elapsedSeconds);
    void updateAnimation(
        Animator& animator,
        Sprite& sprite,
        AnimationName selected,
        float deltaTime);
    AnimationName selectAnimation(const AnimationSet& animationSet, const AnimationState& state);
}
