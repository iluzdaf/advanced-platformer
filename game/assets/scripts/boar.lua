return {
    activities = {
        sleep = {
            description = "Stay still until something wakes it.",
            update = function()
                return { clearRoute = true }
            end,
        },
        charge = {
            description = "Run straight toward the side the target was on when the charge began, "
                .. "attacking, without stepping off a ledge.",
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
            description = "Stand still, facing the target while it is on the same floor within notice "
                .. "distance.",
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
