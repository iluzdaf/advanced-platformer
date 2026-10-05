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
        pounce = {
            update = function(self, snapshot)
                if snapshot.facts.pounceReady then
                    return {
                        clearRoute = true,
                        aimAt = snapshot.targetCenter or snapshot.lastKnownTargetFeet,
                        pounce = true,
                        contactDamage = true,
                    }
                end

                return { contactDamage = snapshot.facts.pouncing, climbGrip = "hold" }
            end,
        },
    },
}
