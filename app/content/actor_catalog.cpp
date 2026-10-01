#include "actor_catalog.hpp"

#include "actor_definition.hpp"
#include "animation_catalog.hpp"
#include "content_diagnostics.hpp"
#include "content_glaze.hpp"
#include "content_validation.hpp"
#include "machine_catalog.hpp"

#include <array>
#include <filesystem>
#include <format>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <glaze/glaze.hpp>
#include <glm/vec2.hpp>

#include "advanced_platformer/combat/combat.hpp"
#include "advanced_platformer/movement/flying_movement.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"
#include "advanced_platformer/npc/npc.hpp"
#include "advanced_platformer/render/sprite.hpp"

template <>
struct glz::from<glz::JSON, advanced_platformer::Team>
    : advanced_platformer::NamedEnumReader<advanced_platformer::Team>
{
};

template <>
struct glz::from<glz::JSON, advanced_platformer::Facing>
    : advanced_platformer::NamedEnumReader<advanced_platformer::Facing>
{
};

template <>
struct glz::from<glz::JSON, advanced_platformer::NpcTactic>
    : advanced_platformer::NamedEnumReader<advanced_platformer::NpcTactic>
{
};

namespace advanced_platformer
{
    template <> struct ContentNames<Team>
    {
        static constexpr std::array Names{
            std::pair{std::string_view{"player"}, Team::Player},
            std::pair{std::string_view{"enemy"}, Team::Enemy},
            std::pair{std::string_view{"neutral"}, Team::Neutral}};
    };

    template <> struct ContentNames<Facing>
    {
        static constexpr std::array Names{
            std::pair{std::string_view{"left"}, Facing::Left},
            std::pair{std::string_view{"right"}, Facing::Right}};
    };

    template <> struct ContentNames<NpcTactic>
    {
        static constexpr std::array Names{
            std::pair{std::string_view{"pursuer"}, NpcTactic::Pursuer},
            std::pair{std::string_view{"keepDistance"}, NpcTactic::KeepDistance}};
    };

    // actors.json as written: its member names are the file's keys. Glaze reflects only types
    // with linkage, so these cannot go in an anonymous namespace. The movement and senses
    // configs are read as they are, keeping C++ defaults for what a file leaves out; the attack
    // components also hold runtime state, which a file must not set, so they have their own
    // structs of optional settings.
    struct BiteJson
    {
        std::optional<int> damage;
        std::optional<glm::vec2> hitboxSize;
        std::optional<float> reach;
        std::optional<float> windupDuration;
        std::optional<float> activeDuration;
        std::optional<float> recoveryDuration;
    };

    struct ContactDamageJson
    {
        std::optional<int> damage;
    };

    struct RangedJson
    {
        std::optional<int> damage;
        std::optional<glm::vec2> projectileSize;
        std::optional<float> projectileSpeed;
        std::optional<float> projectileLifetime;
        std::optional<float> shootDuration;
        std::optional<float> recoveryDuration;
        std::optional<bool> breaksTiles;
        std::optional<SpriteJson> sprite;
    };

    struct ActorJson
    {
        glm::vec2 bodySize{};
        std::optional<Team> team;
        std::optional<Facing> facing;
        std::optional<SpriteAnchor> spriteAnchor;
        std::optional<std::string> animations;
        std::optional<int> health;
        std::optional<int> inventorySlots;
        std::optional<WithDefaults<PlatformerMovementConfig>> platformer;
        std::optional<WithDefaults<FlyingMovement>> flying;
        std::optional<WithDefaults<SurfaceClimbConfig>> surfaceClimb;
        std::optional<WithDefaults<NpcSenses>> senses;
        std::optional<NpcTactic> tactic;
        std::optional<std::string> machine;
        std::optional<BiteJson> bite;
        std::optional<ContactDamageJson> contactDamage;
        std::optional<RangedJson> ranged;
    };

    struct ActorsJson
    {
        std::string player;
        std::map<std::string, ActorJson> actors;
    };

    namespace
    {
        // Sets the field when the file gives a value, and otherwise keeps its C++ default.
        template <class T> void setIfGiven(T& field, const std::optional<T>& given)
        {
            if (given.has_value())
            {
                field = *given;
            }
        }

        // Each component starts from the C++ defaults and takes only the fields the file
        // names, so an empty object means "this component, as configured in code".
        BiteAttack biteFrom(const BiteJson& json)
        {
            BiteAttack bite;
            setIfGiven(bite.damage, json.damage);
            setIfGiven(bite.hitboxSize, json.hitboxSize);
            setIfGiven(bite.reach, json.reach);
            setIfGiven(bite.windupDuration, json.windupDuration);
            setIfGiven(bite.activeDuration, json.activeDuration);
            setIfGiven(bite.recoveryDuration, json.recoveryDuration);
            return bite;
        }

        ContactDamage contactDamageFrom(const ContactDamageJson& json)
        {
            ContactDamage contact;
            setIfGiven(contact.damage, json.damage);
            return contact;
        }

        RangedWeapon rangedFrom(const RangedJson& json)
        {
            RangedWeapon ranged;
            setIfGiven(ranged.damage, json.damage);
            setIfGiven(ranged.projectileSize, json.projectileSize);
            setIfGiven(ranged.projectileSpeed, json.projectileSpeed);
            setIfGiven(ranged.projectileLifetime, json.projectileLifetime);
            setIfGiven(ranged.shootDuration, json.shootDuration);
            setIfGiven(ranged.recoveryDuration, json.recoveryDuration);
            setIfGiven(ranged.breaksTiles, json.breaksTiles);
            if (json.sprite.has_value())
            {
                ranged.projectileSprite = spriteFrom(*json.sprite);
            }
            return ranged;
        }

        template <class T> std::optional<T> configFrom(const std::optional<WithDefaults<T>>& json)
        {
            if (!json.has_value())
            {
                return std::nullopt;
            }
            return json->get();
        }

        ActorDefinition definitionFrom(const ActorJson& json)
        {
            ActorDefinition result;
            result.bodySize = json.bodySize;
            setIfGiven(result.team, json.team);
            setIfGiven(result.facing, json.facing);
            setIfGiven(result.spriteAnchor, json.spriteAnchor);
            setIfGiven(result.animations, json.animations);
            result.health = json.health;
            result.inventorySlots = json.inventorySlots;
            result.platformer = configFrom(json.platformer);
            result.flying = configFrom(json.flying);
            result.surfaceClimb = configFrom(json.surfaceClimb);
            result.senses = configFrom(json.senses);
            setIfGiven(result.tactic, json.tactic);
            setIfGiven(result.machine, json.machine);
            if (json.bite.has_value())
            {
                result.bite = biteFrom(*json.bite);
            }
            if (json.contactDamage.has_value())
            {
                result.contactDamage = contactDamageFrom(*json.contactDamage);
            }
            if (json.ranged.has_value())
            {
                result.ranged = rangedFrom(*json.ranged);
            }
            return result;
        }
    }

    ActorCatalog parseActorCatalog(
        std::string_view text,
        std::string_view sourceName,
        const AnimationCatalog& animations,
        const MachineCatalog& machines)
    {
        const auto file = readContent<ActorsJson>(text, sourceName);
        ActorCatalog result;
        result.player = file.player;
        if (file.actors.empty())
        {
            failJson(sourceName, "actors", "expected a nonempty object");
        }
        for (const auto& [name, json] : file.actors)
        {
            if (name.empty())
            {
                failJson(sourceName, "actors", "actor name cannot be empty");
            }
            result.definitions.emplace(name, definitionFrom(json));
        }
        validateInFile(sourceName, [&] { validateActorCatalog(result, animations, machines); });
        return result;
    }

    void validateActorAtlasRegions(
        const ActorCatalog& catalog,
        glm::ivec2 atlasSize,
        std::string_view sourceName)
    {
        for (const auto& [name, definition] : catalog.definitions)
        {
            if (definition.ranged.has_value())
            {
                requireInAtlas(
                    definition.ranged->projectileSprite.region,
                    atlasSize,
                    sourceName,
                    fieldPath(fieldPath(fieldPath("actors", name), "ranged"), "sprite"));
            }
        }
    }

    ActorCatalog loadActorCatalog(
        const std::filesystem::path& path,
        const AnimationCatalog& animations,
        const MachineCatalog& machines)
    {
        return parseActorCatalog(loadContentText(path), path.string(), animations, machines);
    }

    void validateActorCatalog(
        const ActorCatalog& catalog,
        const AnimationCatalog& animations,
        const MachineCatalog& machines)
    {
        for (const auto& entry : catalog.definitions)
        {
            if (entry.first.empty())
            {
                throw std::invalid_argument("actor name cannot be empty");
            }
            try
            {
                validateActorDefinition(entry.second, animations, machines);
            }
            catch (const std::invalid_argument& error)
            {
                failJson({}, fieldPath("actors", entry.first), error.what());
            }
        }
        const auto& player = actorDefinition(catalog, catalog.player);
        if (player.senses)
        {
            throw std::invalid_argument("player definition must not enable NPC sensing");
        }
        if (!player.health || !player.inventorySlots)
        {
            throw std::invalid_argument(
                "player definition requires health and inventorySlots for the game HUD");
        }
    }

    const ActorDefinition& actorDefinition(const ActorCatalog& catalog, const std::string& name)
    {
        const auto found = catalog.definitions.find(name);
        if (found == catalog.definitions.end())
        {
            throw std::invalid_argument(std::format("unknown actor definition '{}'", name));
        }
        return found->second;
    }
}
