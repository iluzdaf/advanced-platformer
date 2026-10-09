local WeaveSeconds = 0.4
local WeaveSpeed = 0.5
local ClimbRise = 1.5

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

return {
    activities = {
        wait = {
            update = function(self, snapshot)
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
