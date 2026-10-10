local function patrolGoal(patrol, headingToSecond)
    return headingToSecond and patrol.secondFeet or patrol.firstFeet
end

return {
    activities = {
        patrol = {
            description = "Walk or climb between the patrol ends, first toward the nearer one; without "
                .. "a patrol, stay put.",
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
            description = "Route to the target, aiming at it.",
            update = function(self, snapshot)
                local target = snapshot.targetFeet
                if target == nil then
                    return { clearRoute = true }
                end

                return { routeTo = target, aimAt = target }
            end,
        },
        attack = {
            description = "Pounce at the target when the pounce is ready; otherwise keep hold of the "
                .. "wall or ceiling.",
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
