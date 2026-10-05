#include "animation_catalog.hpp"

#include "content_diagnostics.hpp"
#include "content_glaze.hpp"
#include "content_validation.hpp"

#include <array>
#include <cstddef>
#include <filesystem>
#include <format>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <glaze/glaze.hpp>
#include <glm/vec2.hpp>

#include "advanced_platformer/math/validation.hpp"
#include "advanced_platformer/render/animation.hpp"
#include "advanced_platformer/render/sprite.hpp"

// NOLINTBEGIN(readability-identifier-naming)
template <> struct glz::meta<advanced_platformer::AnimationName>
{
    static constexpr std::array
        keys{"idle", "move", "jump", "fall", "shoot", "bite", "pounce", "death"};
    static constexpr std::array value{
        advanced_platformer::AnimationName::Idle,
        advanced_platformer::AnimationName::Move,
        advanced_platformer::AnimationName::Jump,
        advanced_platformer::AnimationName::Fall,
        advanced_platformer::AnimationName::Shoot,
        advanced_platformer::AnimationName::Bite,
        advanced_platformer::AnimationName::Pounce,
        advanced_platformer::AnimationName::Death};
};

// NOLINTEND(readability-identifier-naming)

namespace advanced_platformer
{
    struct FrameJson
    {
        glm::vec2 position{};
        glm::vec2 size{};
    };

    struct ClipJson
    {
        std::vector<FrameJson> frames;
        float frameDuration = 0.0F;
        bool looping = false;
    };

    using AnimationSetJson = std::map<AnimationName, ClipJson>;

    struct AnimationsJson
    {
        std::map<std::string, AnimationSetJson> animations;
    };

    const char* clipName(AnimationName name)
    {
        constexpr auto& Keys = glz::meta<AnimationName>::keys;
        constexpr auto& Values = glz::meta<AnimationName>::value;
        for (std::size_t index = 0; index < Values.size(); ++index)
        {
            if (Values[index] == name)
            {
                return Keys[index];
            }
        }
        throw std::logic_error("An animation clip has no catalog name");
    }

    void validateAnimationSet(const AnimationSet& set)
    {
        if (findClip(set, AnimationName::Idle) == nullptr)
        {
            failJson({}, "idle", "required clip is missing");
        }
        std::set<AnimationName> names;
        for (const auto& clip : set.clips)
        {
            const std::string_view name = clipName(clip.name);
            if (!names.insert(clip.name).second)
            {
                failJson({}, name, "duplicate animation clip");
            }
            if (clip.frames.empty())
            {
                failJson({}, fieldPath(name, "frames"), "expected at least one frame");
            }
            if (!isFinitePositive(clip.frameDuration))
            {
                failJson({}, fieldPath(name, "frameDuration"), "expected a positive finite number");
            }
            for (std::size_t index = 0; index < clip.frames.size(); ++index)
            {
                try
                {
                    const auto& frame = clip.frames[index];
                    Sprite sprite;
                    sprite.region = frame;
                    validateContentSprite(sprite);
                    if (frame.size != set.clips.front().frames.front().size)
                    {
                        throw std::invalid_argument("all frames in a set must use the same size");
                    }
                }
                catch (const std::invalid_argument& error)
                {
                    failJson({}, indexPath(fieldPath(name, "frames"), index), error.what());
                }
            }
        }
    }

    void validateAnimationCatalog(const AnimationCatalog& catalog)
    {
        for (const auto& entry : catalog)
        {
            try
            {
                if (entry.first.empty())
                {
                    throw std::invalid_argument("animation set name cannot be empty");
                }
                validateAnimationSet(entry.second);
            }
            catch (const std::invalid_argument& error)
            {
                failJson({}, fieldPath("animations", entry.first), error.what());
            }
        }
    }

    namespace
    {
        AnimationClip clipFrom(AnimationName name, const ClipJson& json)
        {
            AnimationClip clip;
            clip.name = name;
            clip.frameDuration = json.frameDuration;
            clip.looping = json.looping;
            for (const FrameJson& frame : json.frames)
            {
                clip.frames.push_back({frame.position, frame.size});
            }
            return clip;
        }
    }

    AnimationCatalog parseAnimationCatalog(std::string_view text, std::string_view sourceName)
    {
        const auto file = readContent<AnimationsJson>(text, sourceName);
        AnimationCatalog catalog;
        for (const auto& [name, json] : file.animations)
        {
            AnimationSet set;
            for (const auto& [type, clipJson] : json)
            {
                set.clips.push_back(clipFrom(type, clipJson));
            }
            catalog.emplace(name, set);
        }
        validateInFile(sourceName, [&] { validateAnimationCatalog(catalog); });
        return catalog;
    }

    AnimationCatalog loadAnimationCatalog(const std::filesystem::path& path)
    {
        return parseAnimationCatalog(loadContentText(path), path.string());
    }

    void validateAnimationAtlasRegions(
        const AnimationCatalog& catalog,
        glm::ivec2 atlasSize,
        std::string_view sourceName)
    {
        for (const auto& [name, set] : catalog)
        {
            for (const AnimationClip& clip : set.clips)
            {
                const std::string frames = fieldPath(
                    fieldPath(fieldPath("animations", name), clipName(clip.name)), "frames");
                for (std::size_t index = 0; index < clip.frames.size(); ++index)
                {
                    requireInAtlas(
                        clip.frames[index], atlasSize, sourceName, indexPath(frames, index));
                }
            }
        }
    }

    const AnimationSet& animationSet(const AnimationCatalog& catalog, const std::string& name)
    {
        const auto found = catalog.find(name);
        if (found == catalog.end())
        {
            throw std::invalid_argument(std::format("unknown animation set '{}'", name));
        }
        return found->second;
    }
}
