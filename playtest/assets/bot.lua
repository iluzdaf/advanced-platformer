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
local KeepAwaySecondsLimit = 2.0
local DoorReplansLimit = 4
local PushDistance = 48.0
local LowHealth = 1
local CellSize = 16
local NoProgressSeconds = 4.0
local UnstickSecondsLimit = 2.0
local UnstickJumpSeconds = 0.5
local ShotgunItem = "Shotgun"
local ShellsItem = "Shells"
local SniperItem = "Sniper"
local RoundsItem = "Rounds"
local ShotgunDistance = 64.0
local SniperDistance = 96.0

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

local function loaded(snapshot, gun, ammo)
    return (snapshot.carried[gun] or 0) > 0 and (snapshot.carried[ammo] or 0) > 0
end

local function weaponFor(snapshot)
    local distance = targetDistance(snapshot)
    if not snapshot.facts.targetVisible or distance == nil then
        return nil
    end
    if distance <= ShotgunDistance and loaded(snapshot, ShotgunItem, ShellsItem) then
        return ShotgunItem
    end
    if distance >= SniperDistance and loaded(snapshot, SniperItem, RoundsItem) then
        return SniperItem
    end
    return nil
end

local function stillPlaced(snapshot, item, feet, skipped)
    if feet == nil or (skipped or {})[tostring(feet)] then
        return false
    end
    for _, pickup in ipairs(snapshot.pickups) do
        if pickup.item == item and pickup.feet == feet then
            return true
        end
    end
    return false
end

local function nearestPickupOf(snapshot, item, skipped)
    local nearest, nearestDistance
    for _, pickup in ipairs(snapshot.pickups) do
        if pickup.item == item and not (skipped or {})[tostring(pickup.feet)] then
            local distance = snapshot.feet:distance(pickup.feet)
            if nearestDistance == nil or distance < nearestDistance then
                nearest, nearestDistance = pickup.feet, distance
            end
        end
    end
    return nearest
end

local function nearestOf(feet, points)
    local nearest, nearestDistance
    for _, point in ipairs(points) do
        local distance = feet:distance(point)
        if nearestDistance == nil or distance < nearestDistance then
            nearest, nearestDistance = point, distance
        end
    end
    return nearest
end

local function stillListed(point, points)
    for _, listed in ipairs(points) do
        if listed == point then
            return true
        end
    end
    return false
end

local function chooseDoor(memory, snapshot)
    local doors = snapshot.unexploredDoors
    local skipped = memory.skippedDoors or {}
    memory.skippedDoors = skipped
    local door = memory.door
    if door ~= nil and stillListed(door, doors) and not skipped[tostring(door)] then
        return door
    end
    local open = {}
    for _, listed in ipairs(doors) do
        if not skipped[tostring(listed)] then
            open[#open + 1] = listed
        end
    end
    if #open == 0 then
        memory.skippedDoors = {}
        open = doors
    end
    memory.door = nearestOf(snapshot.feet, open)
    return memory.door
end

local function aimAtDoor(memory, door)
    memory.routedDoor = door
    memory.doorGoal = door
    memory.doorTries = 0
    memory.doorReplans = 0
end

local function pushedPast(snapshot, door, tries)
    local away = door - snapshot.feet
    local length = away:length()
    local pushes = {
        length > 0 and away * (PushDistance / length) or vec2(0, 0),
        vec2(0, -PushDistance),
        vec2(-PushDistance, 0),
        vec2(PushDistance, 0),
        vec2(0, PushDistance),
    }
    local push = pushes[tries]
    return push ~= nil and door + push or nil
end

local function gaveUpOnGoal(memory, snapshot)
    local goal = memory.doorGoal
    local endedShort = snapshot.routeStatus == "unreachable"
        and snapshot.routeComplete
        and snapshot.feet:distance(goal) > ArrivalDistance
    return endedShort or memory.doorReplans >= DoorReplansLimit
end

return {
    facts = {
        exitKnown = function(_, snapshot)
            return snapshot.exitFeet ~= nil
        end,

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

        potionNeeded = function(memory, snapshot)
            local health = snapshot.health
            if health == nil or health.current > LowHealth then
                return false
            end
            memory.unreachablePotions = memory.unreachablePotions or {}
            return nearestPickupOf(snapshot, PotionItem, memory.unreachablePotions) ~= nil
        end,

        weaponBetter = function(_, snapshot)
            return weaponFor(snapshot) ~= snapshot.equipped
        end,

        noProgress = function(memory, snapshot, step)
            local level = tostring(snapshot.exitFeet)
            if memory.progressLevel ~= level then
                memory.progressLevel = level
                memory.seenCells = {}
                memory.noProgressSeconds = 0
            end
            local feet = snapshot.feet
            local cell = math.floor(feet.x / CellSize) .. "," .. math.floor(feet.y / CellSize)
            if memory.seenCells[cell] then
                memory.noProgressSeconds = memory.noProgressSeconds + step
            else
                memory.seenCells[cell] = true
                memory.noProgressSeconds = 0
            end
            if memory.noProgressSeconds < NoProgressSeconds then
                memory.unstickSeconds = 0
                return false
            end
            if withinBudget(memory.unstickSeconds, step, UnstickSecondsLimit) then
                return true
            end
            memory.noProgressSeconds = 0
            memory.unstickSeconds = 0
            return false
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
        explore = {
            description = "Entered while exitKnown is false: route to the nearest door into a "
                .. "room the bot has not entered, keeping that door while it stays "
                .. "unexplored and asking for a fresh route as travel does. When the "
                .. "route ends short as unreachable or has needed DoorReplansLimit "
                .. "fresh routes, aim PushDistance past the door, away from the bot, "
                .. "then skip the door if that fails too, until every door is "
                .. "skipped. Stand still when no door is left, and do the same on "
                .. "the way as travel.",
            update = function(_, snapshot, step, memory)
                local door = chooseDoor(memory, snapshot)
                if door ~= memory.routedDoor then
                    aimAtDoor(memory, door)
                elseif door ~= nil and gaveUpOnGoal(memory, snapshot) then
                    memory.doorTries = memory.doorTries + 1
                    memory.doorReplans = 0
                    memory.doorGoal = pushedPast(snapshot, door, memory.doorTries)
                    if memory.doorGoal == nil then
                        memory.skippedDoors[tostring(door)] = true
                        door = chooseDoor(memory, snapshot)
                        aimAtDoor(memory, door)
                    end
                end
                if door == nil then
                    return onTheWay(memory, snapshot, { clearRoute = true })
                end
                local command = routeWithReplan(memory, snapshot, step, memory.doorGoal)
                if command.clearRoute then
                    memory.doorReplans = memory.doorReplans + 1
                end
                return onTheWay(memory, snapshot, command)
            end,
        },

        travel = {
            description = "Entered while exitKnown: route to the level exit, asking for a fresh "
                .. "route when the route ends short of it or the bot has stood still "
                .. "for a second; on the way, shoot the breakable tile under the "
                .. "nearest pickup within LootShootDistance, shoot the opponent in "
                .. "sight and drink a potion at 1 health.",
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

        heal = {
            description = "Entered while potionNeeded: at LowHealth health, route to the "
                .. "nearest health potion in the level the route has not found "
                .. "unreachable, keeping that potion as the goal while it is still "
                .. "there, doing the same on the way as travel, so the potion is drunk "
                .. "as soon as it is in the bag.",
            update = function(_, snapshot, step, memory)
                local potion = memory.healTarget
                if not stillPlaced(snapshot, PotionItem, potion, memory.unreachablePotions) then
                    potion = nearestPickupOf(snapshot, PotionItem, memory.unreachablePotions)
                end
                if snapshot.routeStatus == "unreachable" and memory.healTarget == potion then
                    memory.unreachablePotions[tostring(potion)] = true
                end
                memory.healTarget = potion
                local command = routeWithReplan(memory, snapshot, step, potion)
                return onTheWay(memory, snapshot, command)
            end,
        },

        unstick = {
            description = "Entered while noProgress: the bot has reached no new cell for "
                .. "NoProgressSeconds. Give up the potion heal was heading for, if "
                .. "any, and walk the other way from last time, jumping every "
                .. "UnstickJumpSeconds, for at most UnstickSecondsLimit, then try "
                .. "the goal again.",
            enter = function(_, snapshot, memory)
                local health = snapshot.health
                if
                    memory.healTarget ~= nil
                    and memory.unreachablePotions ~= nil
                    and health ~= nil
                    and health.current <= LowHealth
                then
                    memory.unreachablePotions[tostring(memory.healTarget)] = true
                end
                memory.unstickDirection = -(memory.unstickDirection or -1)
            end,
            update = function(_, snapshot, step, memory)
                memory.unstickSeconds = (memory.unstickSeconds or 0) + step
                local jump = memory.unstickSeconds % UnstickJumpSeconds < step
                return onTheWay(memory, snapshot, {
                    clearRoute = true,
                    direction = { x = memory.unstickDirection, y = 0 },
                    jumpPressed = jump,
                    jumpHeld = true,
                })
            end,
        },

        switchWeapon = {
            description = "Entered while weaponBetter: equip the shotgun for an opponent in "
                .. "sight within ShotgunDistance, the sniper for one at SniperDistance or "
                .. "further, when the bag holds that gun and its ammo, and the pistol "
                .. "otherwise, so no ammo is spent on tiles or out of sight; keep "
                .. "shooting the opponent in sight.",
            update = function(_, snapshot)
                local command = fightNearest(snapshot, { clearRoute = true })
                command.useItem = weaponFor(snapshot) or snapshot.equipped
                return command
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
