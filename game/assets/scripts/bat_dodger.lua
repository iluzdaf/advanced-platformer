local WeaveSeconds = 0.4
local WeaveSpeed = 0.5
local ClimbRise = 1.5
local DodgeMargin = 10

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

local function dodgeFrom(snapshot)
    for _, shot in ipairs(snapshot.shots) do
        local speed = shot.velocity:length()
        if speed > 0.001 then
            local heading = shot.velocity / speed
            local toBat = snapshot.center - shot.center
            local side = vec2(-heading.y, heading.x)
            local offset = toBat:dot(side)
            if toBat:dot(heading) > 0 and math.abs(offset) < DodgeMargin then
                return offset > 0 and side or side * -1
            end
        end
    end
    return nil
end

return {
    activities = {
        wait = {
            description = "Hover in place; while the target is known, weave left and right and "
                .. "aim at it, and fly out of the path of any shot it sees coming.",
            update = function(self, snapshot)
                local dodge = dodgeFrom(snapshot)
                if dodge then
                    return { clearRoute = true, direction = dodge, aimAt = targetOf(snapshot) }
                end
                if not snapshot.facts.targetKnown then
                    return { clearRoute = true }
                end
                local side = math.floor(snapshot.stateElapsed / WeaveSeconds) % 2 == 0 and 1 or -1
                return {
                    clearRoute = true,
                    direction = { x = side * WeaveSpeed, y = 0 },
                    aimAt = targetOf(snapshot),
                }
            end,
        },
        dive = {
            description = "Fly straight at the target, biting.",
            update = function(self, snapshot)
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
            description = "Fly up and away from the side the target was on when the dive ended.",
            enter = function(self, snapshot)
                self.side = targetOf(snapshot).x < snapshot.center.x and 1 or -1
            end,
            update = function(self, snapshot)
                return {
                    clearRoute = true,
                    direction = unit(vec2(self.side, -ClimbRise)),
                    aimAt = targetOf(snapshot),
                }
            end,
        },
    },
}
