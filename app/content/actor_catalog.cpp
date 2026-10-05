#include "actor_catalog.hpp"

#include "actor_definition.hpp"
#include "animation_catalog.hpp"
#include "content_diagnostics.hpp"
#include "content_glaze.hpp"
#include "content_validation.hpp"
#include "machine_catalog.hpp"

#include <array>
#include <variant>
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
#include "advanced_platformer/combat/attack.hpp"
#include "advanced_platformer/movement/flying_movement.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/movement/pounce.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"
#include "advanced_platformer/npc/npc.hpp"
#include "advanced_platformer/render/sprite.hpp"

template <> struct glz::meta<advanced_platformer::Team>
{
    // NOLINTNEXTLINE(readability-identifier-naming)
    static constexpr std::array keys{"player", "enemy", "neutral"};
    // NOLINTNEXTLINE(readability-identifier-naming)
    static constexpr std::array value{
        advanced_platformer::Team::Player,
        advanced_platformer::Team::Enemy,
        advanced_platformer::Team::Neutral};
};

template <> struct glz::meta<advanced_platformer::Facing>
{
    // NOLINTNEXTLINE(readability-identifier-naming)
    static constexpr std::array keys{"left", "right"};
    // NOLINTNEXTLINE(readability-identifier-naming)
    static constexpr std::array value{
        advanced_platformer::Facing::Left,
        advanced_platformer::Facing::Right};
};

namespace advanced_platformer
{
    struct BiteFields
    {
        std::optional<int> damage;
        std::optional<glm::vec2> hitboxSize;
        std::optional<float> reach;
        std::optional<float> windupDuration;
        std::optional<float> activeDuration;
        std::optional<float> recoveryDuration;
    };

    struct RangedFields
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

    struct ContactFields
    {
        std::optional<int> damage;
        std::optional<WithDefaults<Knockback>> knockback;
    };

    struct PounceFields
    {
        std::optional<int> damage;
        std::optional<WithDefaults<Knockback>> knockback;
        std::optional<float> speed;
        std::optional<float> lift;
        std::optional<float> range;
        std::optional<float> recoveryDuration;
    };

    struct BiteJson
    {
        BiteFields bite;
    };

    struct RangedJson
    {
        RangedFields ranged;
    };

    struct ContactJson
    {
        ContactFields contact;
    };

    struct PounceJson
    {
        PounceFields pounce;
    };

    using AttackJson = std::variant<BiteJson, RangedJson, ContactJson, PounceJson>;

    struct PlatformerJson
    {
        WithDefaults<PlatformerMovementConfig> platformer;
    };

    struct FlyingJson
    {
        WithDefaults<FlyingMovementConfig> flying;
    };

    using MovementJson = std::variant<PlatformerJson, FlyingJson>;

    struct ActorJson
    {
        glm::vec2 bodySize{};
        std::optional<Team> team;
        std::optional<Facing> facing;
        std::optional<SpriteAnchor> spriteAnchor;
        std::optional<std::string> animations;
        std::optional<int> health;
        std::optional<int> inventorySlots;
        MovementJson movement;
        std::optional<WithDefaults<SurfaceClimbConfig>> surfaceClimb;
        std::optional<WithDefaults<NpcSenses>> senses;
        std::optional<std::string> machine;
        std::optional<AttackJson> primaryAttack;
        std::optional<AttackJson> secondaryAttack;
    };

    struct ActorsJson
    {
        std::string player;
        std::map<std::string, ActorJson> actors;
    };

    namespace
    {
        template <class T> void setIfGiven(T& field, const std::optional<T>& given)
        {
            if (given.has_value())
            {
                field = *given;
            }
        }

        std::optional<Knockback> knockbackFrom(const std::optional<WithDefaults<Knockback>>& json)
        {
            if (!json.has_value())
            {
                return std::nullopt;
            }
            return json->get();
        }

        Attack attackFrom(const BiteJson& json)
        {
            const BiteFields& fields = json.bite;
            BiteAttack bite;
            setIfGiven(bite.damage, fields.damage);
            setIfGiven(bite.hitboxSize, fields.hitboxSize);
            setIfGiven(bite.reach, fields.reach);
            setIfGiven(bite.windupDuration, fields.windupDuration);
            setIfGiven(bite.activeDuration, fields.activeDuration);
            setIfGiven(bite.recoveryDuration, fields.recoveryDuration);
            return bite;
        }

        Attack attackFrom(const RangedJson& json)
        {
            const RangedFields& fields = json.ranged;
            RangedWeapon ranged;
            setIfGiven(ranged.damage, fields.damage);
            setIfGiven(ranged.projectileSize, fields.projectileSize);
            setIfGiven(ranged.projectileSpeed, fields.projectileSpeed);
            setIfGiven(ranged.projectileLifetime, fields.projectileLifetime);
            setIfGiven(ranged.shootDuration, fields.shootDuration);
            setIfGiven(ranged.recoveryDuration, fields.recoveryDuration);
            setIfGiven(ranged.breaksTiles, fields.breaksTiles);
            if (fields.sprite.has_value())
            {
                ranged.projectileSprite = spriteFrom(*fields.sprite);
            }
            return ranged;
        }

        Attack attackFrom(const ContactJson& json)
        {
            ContactDamage contact;
            setIfGiven(contact.damage, json.contact.damage);
            contact.knockback = knockbackFrom(json.contact.knockback);
            return contact;
        }

        Attack attackFrom(const PounceJson& json)
        {
            const PounceFields& fields = json.pounce;
            Pounce pounce;
            setIfGiven(pounce.config.damage, fields.damage);
            pounce.config.knockback = knockbackFrom(fields.knockback);
            setIfGiven(pounce.config.speed, fields.speed);
            setIfGiven(pounce.config.lift, fields.lift);
            setIfGiven(pounce.config.range, fields.range);
            setIfGiven(pounce.config.recoveryDuration, fields.recoveryDuration);
            return pounce;
        }

        Attack attackFrom(const AttackJson& json)
        {
            return std::visit([](const auto& kind) { return attackFrom(kind); }, json);
        }

        MovementConfig movementFrom(const PlatformerJson& json)
        {
            return json.platformer.get();
        }

        MovementConfig movementFrom(const FlyingJson& json)
        {
            return json.flying.get();
        }

        MovementConfig movementFrom(const MovementJson& json)
        {
            return std::visit([](const auto& kind) { return movementFrom(kind); }, json);
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
            result.movement = movementFrom(json.movement);
            result.surfaceClimb = configFrom(json.surfaceClimb);
            result.senses = configFrom(json.senses);
            setIfGiven(result.machine, json.machine);
            if (json.primaryAttack.has_value())
            {
                result.primaryAttack = attackFrom(*json.primaryAttack);
            }
            if (json.secondaryAttack.has_value())
            {
                result.secondaryAttack = attackFrom(*json.secondaryAttack);
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
            try
            {
                result.definitions.emplace(name, definitionFrom(json));
            }
            catch (const std::invalid_argument& error)
            {
                failJson(sourceName, fieldPath("actors", name), error.what());
            }
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
            for (const AttackSlot slot : AttackSlots)
            {
                const std::optional<Attack>& attack = slot == AttackSlot::Primary
                                                          ? definition.primaryAttack
                                                          : definition.secondaryAttack;
                if (!attack.has_value())
                {
                    continue;
                }
                if (const auto* weapon = std::get_if<RangedWeapon>(&*attack))
                {
                    requireInAtlas(
                        weapon->projectileSprite.region,
                        atlasSize,
                        sourceName,
                        fieldPath(
                            fieldPath(
                                fieldPath("actors", name),
                                slot == AttackSlot::Primary ? "primaryAttack" : "secondaryAttack"),
                            "sprite"));
                }
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
