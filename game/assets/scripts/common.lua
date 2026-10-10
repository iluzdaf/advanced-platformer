local LookTurnSeconds = 0.5

local function followingRoute(snapshot)
    return snapshot.routeStatus == "found" or snapshot.routeStatus == "unreachable"
end

local function lookAbout(snapshot)
    local toward = snapshot.lastKnownTargetFeet.x - snapshot.feet.x
    local side = toward < 0 and -1 or 1
    if math.floor(snapshot.stateElapsed / LookTurnSeconds) % 2 == 1 then
        side = -side
    end
    return { x = side, y = 0 }
end

local function patrolGoal(patrol, headingToSecond)
    return headingToSecond and patrol.secondFeet or patrol.firstFeet
end

return {
    activities = {
        idle = {
            description = "Stand still and do nothing.",
            update = function()
                return nil
            end,
        },

        patrol = {
            description = "Walk to one patrol end, then turn and walk to the other; without a patrol, "
                .. "stand still.",
            update = function(self, snapshot)
                local patrol = snapshot.patrol
                if patrol == nil then
                    return nil
                end
                if snapshot.routeComplete then
                    return {
                        turnPatrol = true,
                        clearRoute = true,
                        routeTo = patrolGoal(patrol, not patrol.headingToSecond),
                    }
                end
                return { routeTo = patrolGoal(patrol, patrol.headingToSecond) }
            end,
        },

        chase = {
            description = "Route to where the target was last seen or heard, aiming at that spot.",
            update = function(self, snapshot)
                local goal = snapshot.lastKnownTargetFeet
                return { routeTo = goal, aimAt = goal }
            end,
        },

        attack = {
            description = "Aim at the target, attacking while it is in primary range.",
            update = function(self, snapshot)
                local target = snapshot.targetCenter
                local command = { aimAt = target or snapshot.lastKnownTargetFeet }
                if snapshot.facts.targetInPrimaryRange then
                    command.primaryAttackPressed = true
                end
                return command
            end,
        },

        search = {
            description = "Route to where the target was last known, then look left and right in turn."
                .. "Route to where the target was last known, then look left and right in turn.",
            update = function(self, snapshot)
                local goal = snapshot.lastKnownTargetFeet
                local command = { routeTo = goal }
                if self.routed and (snapshot.routeComplete or not followingRoute(snapshot)) then
                    command.aimDirection = lookAbout(snapshot)
                end
                self.routed = true
                return command
            end,
        },

        retreat = {
            description = "Step away from where the target was last known without leaving the floor, "
                .. "firing at it.",
            update = function(self, snapshot)
                local threat = snapshot.lastKnownTargetFeet
                local away = snapshot.feet - threat
                local footing = snapshot.footing
                if footing ~= nil then
                    local side = away.x < 0 and -1 or 1
                    local canStep = (side < 0 and footing.left) or (side > 0 and footing.right)
                    away = canStep and { x = side, y = 0 } or { x = 0, y = 0 }
                end
                return {
                    direction = away,
                    aimAt = snapshot.targetCenter or threat,
                    primaryAttackPressed = true,
                }
            end,
        },

        watch = {
            description = "Stand still and look left and right in turn.",
            update = function(self, snapshot)
                return { aimDirection = lookAbout(snapshot) }
            end,
        },
    },
}
