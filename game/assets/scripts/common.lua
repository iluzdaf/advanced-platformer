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
            update = function()
                return nil
            end,
        },

        patrol = {
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
            update = function(self, snapshot)
                local goal = snapshot.lastKnownTargetFeet
                return { routeTo = goal, aimAt = goal }
            end,
        },

        attack = {
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
            update = function(self, snapshot)
                return { aimDirection = lookAbout(snapshot) }
            end,
        },
    },
}
