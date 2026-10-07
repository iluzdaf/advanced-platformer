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

local function routeWithReplan(self, snapshot, step, destination)
    local short = snapshot.routeComplete and snapshot.feet:distance(destination) > ArrivalDistance
    if short or stillFor(self, snapshot, step) >= StillSecondsBeforeReplan then
        self.stillSeconds = 0
        return { clearRoute = true, routeTo = destination }
    end
    return { routeTo = destination }
end

local skills = {}

skills.goToExit = {
    description = "Route to the level exit, asking for a fresh route when the route ends short of it or the bot has stood still for a second.",
    run = function(self, snapshot, step)
        local exit = snapshot.exitFeet
        if exit == nil then
            return { clearRoute = true }
        end
        return routeWithReplan(self, snapshot, step, exit)
    end,
}

skills.fightNearest = {
    description = "Aim at the visible opponent and fire the primary attack whenever it is ready, on top of the command of whatever skill is moving the bot.",
    run = function(self, snapshot, step, command)
        local target = snapshot.targetCenter
        if target ~= nil and snapshot.facts.targetVisible then
            command.aimAt = target
            command.primaryAttackPressed = snapshot.facts.primaryReady
        end
        return command
    end,
}

skills.collect = {
    description = "Route to the nearest pickup of the named item, or return nil when the level has none left.",
    run = function(self, snapshot, step, item)
        local nearest, nearestDistance
        for _, pickup in ipairs(snapshot.pickups) do
            if pickup.item == item then
                local distance = snapshot.feet:distance(pickup.feet)
                if nearestDistance == nil or distance < nearestDistance then
                    nearest, nearestDistance = pickup.feet, distance
                end
            end
        end
        if nearest == nil then
            return nil
        end
        return routeWithReplan(self, snapshot, step, nearest)
    end,
}

local function play(self, snapshot, step)
    local command = skills.goToExit.run(self, snapshot, step)
    return skills.fightNearest.run(self, snapshot, step, command)
end

return {
    activities = {
        play = { update = play },
    },
}
