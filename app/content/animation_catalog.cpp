#include "animation_catalog.hpp"
#include "content_diagnostics.hpp"
#include "content_glaze.hpp"
#include "content_json.hpp"
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
#include <utility>
#include <vector>
#include <glaze/glaze.hpp>
#include <glm/vec2.hpp>
#include "advanced_platformer/math/validation.hpp"
#include "advanced_platformer/render/animation.hpp"
#include "advanced_platformer/render/sprite.hpp"

namespace advanced_platformer
{
    // animations.json as written: its member names are the file's keys. Glaze reflects only
    // types with linkage, so these cannot go in an anonymous namespace.
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

    // Every set has all six clips.
    struct AnimationSetJson
    {
        ClipJson idle;
        ClipJson move;
        ClipJson jump;
        ClipJson fall;
        ClipJson attack;
        ClipJson death;
    };

    struct AnimationsJson
    {
        std::map<std::string, AnimationSetJson> animations;
    };

    namespace
    {
        // Each clip's member in an AnimationSetJson, in the order sets list their clips.
        constexpr std::array<std::pair<AnimationName, ClipJson AnimationSetJson::*>, 6>
            ClipMembers = {
                {{AnimationName::Idle, &AnimationSetJson::idle},
                 {AnimationName::Move, &AnimationSetJson::move},
                 {AnimationName::Jump, &AnimationSetJson::jump},
                 {AnimationName::Fall, &AnimationSetJson::fall},
                 {AnimationName::Attack, &AnimationSetJson::attack},
                 {AnimationName::Death, &AnimationSetJson::death}}};

        struct ClipEntry
        {
            std::string_view name;
            AnimationName type;
        };

        constexpr std::array<ClipEntry, 6> Clips = {
            {{"idle", AnimationName::Idle},
             {"move", AnimationName::Move},
             {"jump", AnimationName::Jump},
             {"fall", AnimationName::Fall},
             {"attack", AnimationName::Attack},
             {"death", AnimationName::Death}}};

        // The clip's field name in animations.json, as in "idle".
        std::string_view clipName(AnimationName name)
        {
            for (const ClipEntry& entry : Clips)
            {
                if (entry.type == name)
                {
                    return entry.name;
                }
            }
            throw std::logic_error("An animation clip has no catalog name");
        }

    }

    void validateAnimationSet(const AnimationSet& set)
    {
        std::set<AnimationName> names;
        for (const auto& clip : set.clips)
        {
            std::string name;
            for (const ClipEntry& entry : Clips)
            {
                if (clip.name == entry.type)
                {
                    name = entry.name;
                    break;
                }
            }
            if (name.empty())
            {
                throw std::invalid_argument("unknown animation clip");
            }
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
                    sprite.size = frame.size;
                    validateContentSprite(sprite);
                    // Playback changes the source rectangle, not the sprite's display size.
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
        for (const ClipEntry& entry : Clips)
        {
            if (names.count(entry.type) == 0)
            {
                throw std::invalid_argument(
                    std::format("{}: required clip is missing", entry.name));
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

    AnimationCatalog parseAnimationCatalog(std::string_view text, std::string_view sourceName)
    {
        const auto file = readContent<AnimationsJson>(text, sourceName);
        AnimationCatalog catalog;
        for (const auto& [name, json] : file.animations)
        {
            AnimationSet set;
            for (const auto& [type, member] : ClipMembers)
            {
                const ClipJson& clipJson = json.*member;
                AnimationClip clip;
                clip.name = type;
                clip.frameDuration = clipJson.frameDuration;
                clip.looping = clipJson.looping;
                for (const FrameJson& frame : clipJson.frames)
                {
                    clip.frames.push_back({frame.position, frame.size});
                }
                set.clips.push_back(clip);
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

    const AnimationSet& animationSet(const AnimationCatalog& catalog, const std::string& name)
    {
        const auto found = catalog.find(name);
        if (found == catalog.end())
        {
            throw std::invalid_argument(std::format("unknown animation set '{}'", name));
        }
        return found->second;
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
}
