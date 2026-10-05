#include "advanced_platformer/render/animation.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <stdexcept>

#include <glm/vec2.hpp>

#include "advanced_platformer/math/validation.hpp"
#include "advanced_platformer/render/sprite.hpp"

namespace advanced_platformer
{
    const AnimationClip* findClip(const AnimationSet& animationSet, AnimationName name)
    {
        const auto clip = std::ranges::find_if(
            animationSet.clips,
            [name](const AnimationClip& candidate) { return candidate.name == name; });
        return clip == animationSet.clips.end() ? nullptr : &*clip;
    }

    const AnimationClip& clipFor(const AnimationSet& animationSet, AnimationName name)
    {
        const AnimationClip* clip = findClip(animationSet, name);
        if (clip == nullptr)
        {
            throw std::invalid_argument(
                "The animation set does not contain the selected animation");
        }
        return *clip;
    }

    const SpriteRegion& frameAt(const AnimationClip& clip, float elapsedSeconds)
    {
        if (clip.frames.empty())
        {
            throw std::invalid_argument("Animation clips require at least one frame");
        }
        if (!isFinitePositive(clip.frameDuration) || !isFiniteNonNegative(elapsedSeconds))
        {
            throw std::invalid_argument("Animation timing must be positive and finite");
        }

        std::size_t frame = static_cast<std::size_t>(elapsedSeconds / clip.frameDuration);
        if (clip.looping)
        {
            frame %= clip.frames.size();
        }
        else
        {
            frame = std::min(frame, clip.frames.size() - 1);
        }

        return clip.frames[frame];
    }

    void updateAnimation(
        Animator& animator,
        Sprite& sprite,
        AnimationName selected,
        float deltaTime)
    {
        requireSeconds(deltaTime, "Animations time step");
        if (animator.current != selected)
        {
            animator.current = selected;
            animator.elapsed = 0.0F;
        }
        else
        {
            animator.elapsed += deltaTime;
        }

        sprite.region = frameAt(clipFor(animator.animationSet, selected), animator.elapsed);
    }

    namespace
    {
        constexpr std::array<AnimationName, 8> AnimationPriority{
            AnimationName::Death,
            AnimationName::Pounce,
            AnimationName::Bite,
            AnimationName::Shoot,
            AnimationName::Jump,
            AnimationName::Fall,
            AnimationName::Move,
            AnimationName::Idle};

        bool animationActive(AnimationName name, const AnimationState& state)
        {
            switch (name)
            {
            case AnimationName::Death:
                return state.dying;
            case AnimationName::Pounce:
                return state.pouncing;
            case AnimationName::Bite:
                return state.biting;
            case AnimationName::Shoot:
                return state.shooting;
            case AnimationName::Jump:
                return !state.grounded && state.velocity.y < 0.0F;
            case AnimationName::Fall:
                return !state.grounded;
            case AnimationName::Move:
                return state.grounded && state.velocity != glm::vec2{0.0F, 0.0F};
            case AnimationName::Idle:
                return true;
            }
            return false;
        }
    }

    AnimationName selectAnimation(const AnimationSet& animationSet, const AnimationState& state)
    {
        for (const AnimationName name : AnimationPriority)
        {
            if (animationActive(name, state) && findClip(animationSet, name) != nullptr)
            {
                return name;
            }
        }
        return AnimationName::Idle;
    }
}
