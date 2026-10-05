#pragma once

#include <functional>
#include <utility>

#include "advanced_platformer/render/presentation_scripts.hpp"
#include "advanced_platformer/world/world.hpp"

namespace tests
{
    class NoEffects final : public advanced_platformer::PresentationScripts
    {
    public:
        advanced_platformer::PresentationEffects onEvent(
            const advanced_platformer::WorldEvent&,
            bool) override
        {
            return {};
        }
    };

    class EffectsFrom final : public advanced_platformer::PresentationScripts
    {
    public:
        using Rule = std::function<
            advanced_platformer::PresentationEffects(const advanced_platformer::WorldEvent&, bool)>;

        explicit EffectsFrom(Rule rule)
            : rule(std::move(rule))
        {
        }

        advanced_platformer::PresentationEffects onEvent(
            const advanced_platformer::WorldEvent& event,
            bool player) override
        {
            ++calls;
            return rule(event, player);
        }

        int calls = 0;

    private:
        Rule rule;
    };
}
