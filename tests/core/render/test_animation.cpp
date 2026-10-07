#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include "advanced_platformer/render/animation.hpp"
#include "advanced_platformer/render/sprite.hpp"
#include "support/animator.hpp"
#include "support/require_near.hpp"

namespace
{
    using advanced_platformer::AnimationClip;
    using advanced_platformer::AnimationName;
    using advanced_platformer::AnimationSet;
    using advanced_platformer::Animator;
    using advanced_platformer::Sprite;
    using advanced_platformer::SpriteRegion;
}

TEST_CASE("Looping animation clips wrap around", "[render][animation]")
{
    const AnimationClip clip{
        AnimationName::Move,
        {{{1.0F, 0.0F}, {1.0F, 1.0F}}, {{2.0F, 0.0F}, {1.0F, 1.0F}}},
        0.1F,
        true};

    REQUIRE(advanced_platformer::frameAt(clip, 0.0F).position.x == 1.0F);
    REQUIRE(advanced_platformer::frameAt(clip, 0.1F).position.x == 2.0F);
    REQUIRE(advanced_platformer::frameAt(clip, 0.2F).position.x == 1.0F);
}

TEST_CASE("Non-looping animation clips hold their last frame", "[render][animation]")
{
    const AnimationClip clip{
        AnimationName::Death,
        {{{1.0F, 0.0F}, {1.0F, 1.0F}}, {{2.0F, 0.0F}, {1.0F, 1.0F}}},
        0.1F,
        false};

    REQUIRE(advanced_platformer::frameAt(clip, 10.0F).position.x == 2.0F);
}

TEST_CASE("Each animator keeps independent playback state", "[render][animation]")
{
    const AnimationClip move{
        AnimationName::Move,
        {{{1.0F, 0.0F}, {1.0F, 1.0F}}, {{2.0F, 0.0F}, {1.0F, 1.0F}}},
        0.1F,
        true};
    Animator first;
    Animator second;
    first.animationSet = AnimationSet{{move}};
    second.animationSet = AnimationSet{{move}};
    Sprite firstSprite;
    Sprite secondSprite;

    advanced_platformer::updateAnimation(first, firstSprite, AnimationName::Move, 0.1F);
    advanced_platformer::updateAnimation(first, firstSprite, AnimationName::Move, 0.1F);
    advanced_platformer::updateAnimation(second, secondSprite, AnimationName::Move, 0.1F);

    REQUIRE_NEAR(first.elapsed, 0.1F);
    REQUIRE(firstSprite.region.position.x == 2.0F);
    REQUIRE_NEAR(second.elapsed, 0.0F);
    REQUIRE(secondSprite.region.position.x == 1.0F);
}

TEST_CASE("Changing animation resets its playback time", "[render][animation]")
{
    const AnimationClip death{AnimationName::Death, {{{5.0F, 0.0F}, {1.0F, 1.0F}}}, 0.4F, false};
    Animator animator;
    animator.current = AnimationName::Move;
    animator.elapsed = 0.3F;
    animator.animationSet = AnimationSet{{death}};
    Sprite sprite;

    advanced_platformer::updateAnimation(animator, sprite, AnimationName::Death, 0.1F);

    REQUIRE(animator.current == AnimationName::Death);
    REQUIRE_NEAR(animator.elapsed, 0.0F);
    REQUIRE(sprite.region.position == glm::vec2{5.0F, 0.0F});
}

TEST_CASE("Animation sets find clips by name", "[render][animation]")
{
    const AnimationClip idle{
        AnimationName::Idle, {SpriteRegion{{1.0F, 2.0F}, {3.0F, 4.0F}}}, 0.1F, true};
    const AnimationSet animations{{idle}};

    REQUIRE(
        advanced_platformer::clipFor(animations, AnimationName::Idle).frames.front().position.x ==
        1.0F);
    REQUIRE_THROWS_AS(
        advanced_platformer::clipFor(animations, AnimationName::Death), std::invalid_argument);
}

TEST_CASE("Animation selection walks the priority list", "[render][animation]")
{
    const AnimationSet full = tests::fullAnimator().animationSet;
    advanced_platformer::AnimationState state;
    REQUIRE(advanced_platformer::selectAnimation(full, state) == AnimationName::Idle);
    state.velocity = {1.0F, 0.0F};
    REQUIRE(advanced_platformer::selectAnimation(full, state) == AnimationName::Move);
    state.grounded = false;
    state.velocity = {0.0F, -1.0F};
    REQUIRE(advanced_platformer::selectAnimation(full, state) == AnimationName::Jump);
    state.velocity = {0.0F, 1.0F};
    REQUIRE(advanced_platformer::selectAnimation(full, state) == AnimationName::Fall);
    state.shooting = true;
    REQUIRE(advanced_platformer::selectAnimation(full, state) == AnimationName::Shoot);
    state.biting = true;
    REQUIRE(advanced_platformer::selectAnimation(full, state) == AnimationName::Bite);
    state.pouncing = true;
    REQUIRE(advanced_platformer::selectAnimation(full, state) == AnimationName::Pounce);
    state.dying = true;
    REQUIRE(advanced_platformer::selectAnimation(full, state) == AnimationName::Death);
}

TEST_CASE("A state without a clip falls through to the next active one", "[render][animation]")
{
    advanced_platformer::AnimationState state;
    state.dying = true;
    state.biting = true;
    state.grounded = false;
    state.velocity = {1.0F, -1.0F};

    const AnimationSet idleOnly{{tests::clip(AnimationName::Idle, 0.0F)}};
    REQUIRE(advanced_platformer::selectAnimation(idleOnly, state) == AnimationName::Idle);

    const AnimationSet withFall{
        {tests::clip(AnimationName::Idle, 0.0F), tests::clip(AnimationName::Fall, 3.0F)}};
    REQUIRE(advanced_platformer::selectAnimation(withFall, state) == AnimationName::Fall);

    const AnimationSet withBite{
        {tests::clip(AnimationName::Idle, 0.0F), tests::clip(AnimationName::Bite, 5.0F)}};
    REQUIRE(advanced_platformer::selectAnimation(withBite, state) == AnimationName::Bite);

    state.grounded = true;
    const AnimationSet withMove{
        {tests::clip(AnimationName::Idle, 0.0F), tests::clip(AnimationName::Move, 1.0F)}};
    REQUIRE(advanced_platformer::selectAnimation(withMove, state) == AnimationName::Move);
}

TEST_CASE("Animation clips reject missing frames and invalid timing", "[render][animation]")
{
    const AnimationClip empty;
    REQUIRE_THROWS_AS(advanced_platformer::frameAt(empty, 0.0F), std::invalid_argument);

    const AnimationClip invalidDuration{
        AnimationName::Idle, {SpriteRegion{{0.0F, 0.0F}, {1.0F, 1.0F}}}, 0.0F, true};
    REQUIRE_THROWS_AS(advanced_platformer::frameAt(invalidDuration, 0.0F), std::invalid_argument);

    Animator animator;
    Sprite sprite;
    const AnimationClip idle{
        AnimationName::Idle, {SpriteRegion{{0.0F, 0.0F}, {1.0F, 1.0F}}}, 0.1F, true};
    animator.animationSet = AnimationSet{{idle}};
    REQUIRE_THROWS_AS(
        advanced_platformer::updateAnimation(animator, sprite, AnimationName::Move, 0.1F),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        advanced_platformer::updateAnimation(animator, sprite, AnimationName::Idle, -0.1F),
        std::invalid_argument);
}
