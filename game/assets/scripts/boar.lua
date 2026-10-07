return {
    activities = {
        sleep = {
            update = function()
                return { clearRoute = true }
            end,
        },
        charge = {
            enter = function(self, snapshot)
                local target = snapshot.targetFeet
                self.direction = target ~= nil and target.x < snapshot.feet.x and -1 or 1
            end,
            update = function(self)
                return {
                    clearRoute = true,
                    direction = { x = self.direction, y = 0 },
                    avoidLedges = true,
                    primaryAttackPressed = true,
                }
            end,
        },
        stunned = {
            update = function(self, snapshot)
                if
                    snapshot.facts.targetOnSameSurface
                    and snapshot.facts.targetWithinNoticeDistance
                    and snapshot.targetFeet ~= nil
                then
                    return { clearRoute = true, aimAt = snapshot.targetFeet }
                end
                return { clearRoute = true }
            end,
        },
    },
}
