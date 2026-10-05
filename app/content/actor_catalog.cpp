#include "actor_catalog.hpp"

#include "actor_definition.hpp"
#include "animation_catalog.hpp"
#include "content_diagnostics.hpp"
#include "content_glaze.hpp"
#include "content_validation.hpp"
#include "machine_catalog.hpp"

#include <algorithm>
#include <array>
#include <vector>
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

namespace advanced_platformer
{
    enum class AttackKind;
}

template <>
struct glz::from<glz::JSON, advanced_platformer::AttackKind>
    : advanced_platformer::NamedEnumReader<advanced_platformer::AttackKind>
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

    enum class AttackKind
    {
        Bite,
        Ranged,
        Contact,
        Pounce
    };

    template <> struct ContentNames<AttackKind>
    {
        static constexpr std::array Names{
            std::pair{std::string_view{"bite"}, AttackKind::Bite},
            std::pair{std::string_view{"ranged"}, AttackKind::Ranged},
            std::pair{std::string_view{"contact"}, AttackKind::Contact},
            std::pair{std::string_view{"pounce"}, AttackKind::Pounce}};
    };

    struct AttackJson
    {
        AttackKind kind = AttackKind::Bite;
        std::optional<int> damage;
        std::optional<WithDefaults<Knockback>> knockback;
        std::optional<glm::vec2> hitboxSize;
        std::optional<float> reach;
        std::optional<float> windupDuration;
        std::optional<float> activeDuration;
        std::optional<float> recoveryDuration;
        std::optional<glm::vec2> projectileSize;
        std::optional<float> projectileSpeed;
        std::optional<float> projectileLifetime;
        std::optional<float> shootDuration;
        std::optional<bool> breaksTiles;
        std::optional<SpriteJson> sprite;
        std::optional<float> speed;
        std::optional<float> lift;
        std::optional<float> range;
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

        std::string_view nameOf(AttackKind kind)
        {
            for (const auto& [name, value] : ContentNames<AttackKind>::Names)
            {
                if (value == kind)
                {
                    return name;
                }
            }
            return "attack";
        }

        std::vector<std::string_view> fieldsOf(AttackKind kind)
        {
            switch (kind)
            {
            case AttackKind::Bite:
                return {
                    "damage",
                    "hitboxSize",
                    "reach",
                    "windupDuration",
                    "activeDuration",
                    "recoveryDuration"};
            case AttackKind::Ranged:
                return {
                    "damage",
                    "projectileSize",
                    "projectileSpeed",
                    "projectileLifetime",
                    "shootDuration",
                    "recoveryDuration",
                    "breaksTiles",
                    "sprite"};
            case AttackKind::Contact:
                return {"damage", "knockback"};
            case AttackKind::Pounce:
                return {"damage", "knockback", "speed", "lift", "range", "recoveryDuration"};
            }
            return {};
        }

        void rejectForeignFields(const AttackJson& json, std::string_view slot)
        {
            const std::array<std::pair<std::string_view, bool>, 16> given{{
                {"damage", json.damage.has_value()},
                {"knockback", json.knockback.has_value()},
                {"hitboxSize", json.hitboxSize.has_value()},
                {"reach", json.reach.has_value()},
                {"windupDuration", json.windupDuration.has_value()},
                {"activeDuration", json.activeDuration.has_value()},
                {"recoveryDuration", json.recoveryDuration.has_value()},
                {"projectileSize", json.projectileSize.has_value()},
                {"projectileSpeed", json.projectileSpeed.has_value()},
                {"projectileLifetime", json.projectileLifetime.has_value()},
                {"shootDuration", json.shootDuration.has_value()},
                {"breaksTiles", json.breaksTiles.has_value()},
                {"sprite", json.sprite.has_value()},
                {"speed", json.speed.has_value()},
                {"lift", json.lift.has_value()},
                {"range", json.range.has_value()},
            }};
            const std::vector<std::string_view> allowed = fieldsOf(json.kind);
            for (const auto& [key, isGiven] : given)
            {
                if (isGiven && !std::ranges::contains(allowed, key))
                {
                    throw std::invalid_argument(
                        std::format(
                            "{}: a {} attack has no field '{}'", slot, nameOf(json.kind), key));
                }
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

        Attack attackFrom(const AttackJson& json, std::string_view slot)
        {
            rejectForeignFields(json, slot);
            switch (json.kind)
            {
            case AttackKind::Bite: {
                BiteAttack bite;
                setIfGiven(bite.damage, json.damage);
                setIfGiven(bite.hitboxSize, json.hitboxSize);
                setIfGiven(bite.reach, json.reach);
                setIfGiven(bite.windupDuration, json.windupDuration);
                setIfGiven(bite.activeDuration, json.activeDuration);
                setIfGiven(bite.recoveryDuration, json.recoveryDuration);
                return bite;
            }
            case AttackKind::Ranged: {
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
            case AttackKind::Contact: {
                ContactDamage contact;
                setIfGiven(contact.damage, json.damage);
                contact.knockback = knockbackFrom(json.knockback);
                return contact;
            }
            case AttackKind::Pounce: {
                Pounce pounce;
                setIfGiven(pounce.config.damage, json.damage);
                pounce.config.knockback = knockbackFrom(json.knockback);
                setIfGiven(pounce.config.speed, json.speed);
                setIfGiven(pounce.config.lift, json.lift);
                setIfGiven(pounce.config.range, json.range);
                setIfGiven(pounce.config.recoveryDuration, json.recoveryDuration);
                return pounce;
            }
            }
            throw std::invalid_argument(std::format("{}: unknown attack kind", slot));
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
            setIfGiven(result.machine, json.machine);
            if (json.primaryAttack.has_value())
            {
                result.primaryAttack = attackFrom(*json.primaryAttack, "primaryAttack");
            }
            if (json.secondaryAttack.has_value())
            {
                result.secondaryAttack = attackFrom(*json.secondaryAttack, "secondaryAttack");
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
