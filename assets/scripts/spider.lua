local function patrolGoal(patrol, headingToSecond)
    return headingToSecond and patrol.secondFeet or patrol.firstFeet
end

return {
    activities = {
        patrol = {
            update = function(self, snapshot)
                local patrol = snapshot.patrol
                if patrol == nil then
                    return { clearRoute = true }
                end

                if not self.resumed then
                    self.resumed = true
                    local nearerIsSecond = snapshot.feet:distanceSquared(patrol.secondFeet)
                        < snapshot.feet:distanceSquared(patrol.firstFeet)
                    if nearerIsSecond ~= patrol.headingToSecond then
                        return { turnPatrol = true, routeTo = patrolGoal(patrol, nearerIsSecond) }
                    end
                end

                if snapshot.routeComplete then
                    return { turnPatrol = true, clearRoute = true }
                end

                return { routeTo = patrolGoal(patrol, patrol.headingToSecond) }
            end,
        },
        pursue = {
            update = function(self, snapshot)
                local target = snapshot.targetFeet
                if target == nil then
                    return { clearRoute = true }
                end

                return { routeTo = target, aimAt = target }
            end,
        },
        attack = {
            update = function(self, snapshot)
                if snapshot.facts.primaryReady then
                    return {
                        clearRoute = true,
                        aimAt = snapshot.targetCenter or snapshot.lastKnownTargetFeet,
                        primaryAttackPressed = true,
                    }
                end

                return { climbGrip = "hold" }
            end,
        },
    },
}
