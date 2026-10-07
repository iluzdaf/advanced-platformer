local StillDistance = 0.5
local StillSecondsBeforeReplan = 1.0
local ArrivalDistance = 8.0

local function stillFor(self, snapshot, step)
    local feet = snapshot.feet
    if self.lastFeet ~= nil and feet:distance(self.lastFeet) < StillDistance then
        self.stillSeconds = (self.stillSeconds or 0) + step
    else
        self.stillSeconds = 0
    end
    self.lastFeet = feet
    return self.stillSeconds
end

local function towardExit(self, snapshot, step)
    local exit = snapshot.exitFeet
    if exit == nil then
        return { clearRoute = true }
    end
    local arrived = snapshot.routeComplete and snapshot.feet:distance(exit) > ArrivalDistance
    if arrived or stillFor(self, snapshot, step) >= StillSecondsBeforeReplan then
        self.stillSeconds = 0
        return { clearRoute = true, routeTo = exit }
    end
    return { routeTo = exit }
end

return {
    activities = {
        play = {
            update = function(self, snapshot, step)
                local command = towardExit(self, snapshot, step)
                local target = snapshot.targetCenter
                if target ~= nil and snapshot.facts.targetVisible then
                    command.aimAt = target
                    command.primaryAttackPressed = snapshot.facts.primaryReady
                end
                return command
            end,
        },
    },
}
