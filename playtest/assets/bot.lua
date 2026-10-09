local StillDistance = 0.5
local StillSecondsBeforeReplan = 1.0
local ArrivalDistance = 8.0
local LootShootDistance = 128.0
local LootChaseDistance = 160.0
local KiteDistance = 48.0
local KiteSecondsLimit = 1.0
local PotionItem = "Health potion"
local HoldSecondsLimit = 2.0
local NoHoldKind = "rat"
local FlyerKind = "bat"
local DodgeDistance = 64.0
local DodgeSecondsLimit = 0.75
local PouncerKind = "spider"
local PounceKeepAway = 80.0
local KeepAwaySecondsLimit = 1.0

local function stillFor(memory, snapshot, step)
    local feet = snapshot.feet
    if memory.lastFeet ~= nil and feet:distance(memory.lastFeet) < StillDistance then
        memory.stillSeconds = (memory.stillSeconds or 0) + step
    else
        memory.stillSeconds = 0
    end
    memory.lastFeet = feet
    return memory.stillSeconds
end

local function routeWithReplan(memory, snapshot, step, destination)
    local short = snapshot.routeComplete and snapshot.feet:distance(destination) > ArrivalDistance
    if short or stillFor(memory, snapshot, step) >= StillSecondsBeforeReplan then
        memory.stillSeconds = 0
        return { clearRoute = true, routeTo = destination }
    end
    return { routeTo = destination }
end

local function withinBudget(spent, step, limit)
    return (spent or 0) + step <= limit
end

local function targetDistance(snapshot)
    local target = snapshot.targetCenter
    return target ~= nil and snapshot.center:distance(target) or nil
end

local function backAway(snapshot)
    local target = snapshot.targetCenter
    local away = target.x < snapshot.center.x and 1 or -1
    return {
        clearRoute = true,
        direction = { x = away, y = 0 },
        avoidLedges = true,
        aimAt = target,
        primaryAttackPressed = snapshot.facts.primaryReady,
    }
end

local function drinkWhenLow(snapshot, command)
    local health = snapshot.health
    if health ~= nil and health.current <= 1 and health.current < health.maximum then
        command.useItem = PotionItem
    end
    return command
end

local function fightNearest(snapshot, command)
    local target = snapshot.targetCenter
    if target ~= nil and snapshot.facts.targetVisible then
        command.aimAt = target
        command.primaryAttackPressed = snapshot.facts.primaryReady
    end
    return command
end

local function freeLoot(memory, snapshot, command)
    local nearest, nearestDistance
    for _, pickup in ipairs(snapshot.pickups) do
        if pickup.breakableBelow ~= nil then
            local distance = snapshot.feet:distance(pickup.breakableBelow)
            if
                distance <= LootShootDistance
                and (nearestDistance == nil or distance < nearestDistance)
            then
                nearest, nearestDistance = pickup, distance
            end
        end
    end
    if nearest ~= nil then
        command.aimAt = nearest.breakableBelow
        command.primaryAttackPressed = snapshot.facts.primaryReady
        memory.freedItem = nearest.item
    end
    return command
end

local function onTheWay(memory, snapshot, command)
    command = freeLoot(memory, snapshot, command)
    command = drinkWhenLow(snapshot, command)
    return fightNearest(snapshot, command)
end

local function nearestPickupOf(snapshot, item)
    local nearest, nearestDistance
    for _, pickup in ipairs(snapshot.pickups) do
        if pickup.item == item then
            local distance = snapshot.feet:distance(pickup.feet)
            if nearestDistance == nil or distance < nearestDistance then
                nearest, nearestDistance = pickup.feet, distance
            end
        end
    end
    return nearest
end

return {
    facts = {
        targetClosingIn = function(memory, snapshot, step)
            local facts = snapshot.facts
            local distance = targetDistance(snapshot)
            local closing = distance ~= nil
                and memory.lastTargetDistance ~= nil
                and distance < memory.lastTargetDistance
            memory.lastTargetDistance = distance
            if
                not (
                    facts.targetVisible
                    and facts.targetOnSameSurface
                    and closing
                    and distance <= KiteDistance
                )
            then
                memory.kiteSeconds = 0
                return false
            end
            return withinBudget(memory.kiteSeconds, step, KiteSecondsLimit)
        end,

        nonRatInRange = function(memory, snapshot, step)
            local facts = snapshot.facts
            if
                not (
                    facts.targetVisible
                    and facts.targetOnSameSurface
                    and facts.targetInPrimaryRange
                    and snapshot.targetKind ~= nil
                    and snapshot.targetKind ~= NoHoldKind
                )
            then
                memory.holdSeconds = 0
                return false
            end
            return withinBudget(memory.holdSeconds, step, HoldSecondsLimit)
        end,

        batDiving = function(memory, snapshot, step)
            local distance = targetDistance(snapshot)
            local closing = distance ~= nil
                and memory.lastDiveDistance ~= nil
                and distance < memory.lastDiveDistance
            memory.lastDiveDistance = distance
            if
                not (
                    snapshot.facts.targetVisible
                    and snapshot.targetKind == FlyerKind
                    and closing
                    and distance <= DodgeDistance
                )
            then
                memory.dodgeSeconds = 0
                return false
            end
            return withinBudget(memory.dodgeSeconds, step, DodgeSecondsLimit)
        end,

        spiderNear = function(memory, snapshot, step)
            local distance = targetDistance(snapshot)
            if
                not (
                    snapshot.facts.targetVisible
                    and snapshot.targetKind == PouncerKind
                    and distance ~= nil
                    and distance <= PounceKeepAway
                )
            then
                memory.keepAwaySeconds = 0
                return false
            end
            return withinBudget(memory.keepAwaySeconds, step, KeepAwaySecondsLimit)
        end,

        freedLootNearby = function(memory, snapshot)
            if memory.freedItem == nil then
                return false
            end
            for _, pickup in ipairs(snapshot.pickups) do
                if
                    pickup.item == memory.freedItem
                    and pickup.breakableBelow == nil
                    and snapshot.feet:distance(pickup.feet) <= LootChaseDistance
                then
                    return true
                end
            end
            memory.freedItem = nil
            return false
        end,
    },

    activities = {
        travel = {
            description = "Route to the level exit, asking for a fresh route when the route "
                .. "ends short of it or the bot has stood still for a second; on the "
                .. "way, shoot the breakable tile under the nearest pickup within "
                .. "LootShootDistance, shoot the opponent in sight and drink a potion "
                .. "at 1 health.",
            update = function(_, snapshot, step, memory)
                local exit = snapshot.exitFeet
                local command = exit == nil and { clearRoute = true }
                    or routeWithReplan(memory, snapshot, step, exit)
                return onTheWay(memory, snapshot, command)
            end,
        },

        loot = {
            description = "Entered while freedLootNearby: route to the pickup the bot shot "
                .. "free while it stays within LootChaseDistance, doing the same on "
                .. "the way as travel.",
            update = function(_, snapshot, step, memory)
                local pickup = nearestPickupOf(snapshot, memory.freedItem)
                local command = routeWithReplan(memory, snapshot, step, pickup)
                return onTheWay(memory, snapshot, command)
            end,
        },

        kite = {
            description = "Entered while targetClosingIn: back away from an opponent in sight"
                .. " on the same surface that is within KiteDistance and closing in, "
                .. "aiming and firing at it, for at most KiteSecondsLimit per "
                .. "approach.",
            update = function(_, snapshot, step, memory)
                memory.kiteSeconds = (memory.kiteSeconds or 0) + step
                return drinkWhenLow(snapshot, backAway(snapshot))
            end,
        },

        hold = {
            description = "Entered while nonRatInRange: stand and shoot at an opponent in "
                .. "sight that is not a rat while it is in primary range on the same "
                .. "surface, for at most HoldSecondsLimit per opponent.",
            update = function(_, snapshot, step, memory)
                memory.holdSeconds = (memory.holdSeconds or 0) + step
                return drinkWhenLow(snapshot, {
                    clearRoute = true,
                    aimAt = snapshot.targetCenter,
                    primaryAttackPressed = snapshot.facts.primaryReady,
                })
            end,
        },

        dodge = {
            description = "Entered while batDiving: run from a bat in sight that is within "
                .. "DodgeDistance and closing in, aiming and firing at it, for at most"
                .. " DodgeSecondsLimit per dive.",
            update = function(_, snapshot, step, memory)
                memory.dodgeSeconds = (memory.dodgeSeconds or 0) + step
                return drinkWhenLow(snapshot, backAway(snapshot))
            end,
        },

        keepAway = {
            description = "Entered while spiderNear: back away from a spider in sight within "
                .. "PounceKeepAway, aiming and firing at it, for at most "
                .. "KeepAwaySecondsLimit per sighting.",
            update = function(_, snapshot, step, memory)
                memory.keepAwaySeconds = (memory.keepAwaySeconds or 0) + step
                return drinkWhenLow(snapshot, backAway(snapshot))
            end,
        },
    },
}
