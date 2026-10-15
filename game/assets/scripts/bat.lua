local HoverHeight = 40.0
local HoverSide = 16.0
local HoverArrival = 10.0

local function targetOf(snapshot)
    return snapshot.targetCenter or snapshot.lastKnownTargetFeet
end

local function unit(vector)
    local length = vector:length()
    if length < 0.001 then
        return { x = 0, y = 0 }
    end
    return { x = vector.x / length, y = vector.y / length }
end

local function perchOf(memory, snapshot)
    return targetOf(snapshot) + vec2(memory.side * HoverSide, -HoverHeight)
end

return {
    facts = {
        perched = function(memory, snapshot)
            return memory.side ~= nil
                and snapshot.center:distance(perchOf(memory, snapshot)) <= HoverArrival
                and snapshot.facts.targetVisible
        end,

        belowTarget = function(_, snapshot)
            return snapshot.center.y > targetOf(snapshot).y + 4
        end,
    },

    activities = {
        hover = {
            description = "Fly to a perch HoverHeight above the target and HoverSide to one "
                .. "side, the side it came from after a climb, until perched.",
            enter = function(_, snapshot, memory)
                memory.side = memory.climbedSide
                    or (targetOf(snapshot).x < snapshot.center.x and 1 or -1)
                memory.climbedSide = nil
            end,
            update = function(_, snapshot, _, memory)
                return {
                    routeTo = perchOf(memory, snapshot) + vec2(0, 4),
                    aimAt = targetOf(snapshot),
                }
            end,
        },

        dive = {
            description = "Entered once perched: dive straight at the target, attacking, until "
                .. "below it or the dive time is up.",
            update = function(_, snapshot)
                local target = targetOf(snapshot)
                return {
                    clearRoute = true,
                    direction = unit(target - snapshot.center),
                    aimAt = target,
                    primaryAttackPressed = true,
                }
            end,
        },

        climb = {
            description = "Entered after a dive: climb away to the other side, attacking, then "
                .. "hover again.",
            enter = function(_, _, memory)
                memory.climbedSide = -(memory.side or 1)
            end,
            update = function(_, snapshot, _, memory)
                return {
                    clearRoute = true,
                    direction = unit(vec2(memory.climbedSide, -1.5)),
                    aimAt = targetOf(snapshot),
                    primaryAttackPressed = true,
                }
            end,
        },
    },
}
