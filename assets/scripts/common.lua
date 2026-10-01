-- The activities every NPC machine can use: idle, patrol, chase, bite, shoot, search,
-- retreat and watch. Machines decide when each runs; the engine plans routes and moves.

-- Looking about turns the aim every LookTurnSeconds.
local LookTurnSeconds = 0.5

-- An aim to one side that turns every LookTurnSeconds, first towards where the target was
-- last known to be.
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
        -- Stands still.
        idle = {
            update = function()
                return nil
            end,
        },

        -- Walks the patrol from end to end. Arriving at one end turns it round, and it sets
        -- off for the other at once.
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

        -- Goes to where the target was last known to be, facing it.
        chase = {
            update = function(self, snapshot)
                local goal = snapshot.lastKnownTargetFeet
                return { routeTo = goal, aimAt = goal }
            end,
        },

        -- Bites once, on entering, and faces the target while the bite plays out.
        bite = {
            update = function(self, snapshot)
                local command = { aimAt = snapshot.lastKnownTargetFeet }
                if not self.bitten then
                    command.primaryAttackPressed = true
                    self.bitten = true
                end
                return command
            end,
        },

        -- Shoots at the target's body, from its own. A machine leaves this activity when the
        -- target is out of sight or lost.
        shoot = {
            update = function(self, snapshot)
                local target = snapshot.targetCenter
                if target == nil then
                    return nil
                end
                return { aimDirection = target - snapshot.center, primaryAttackPressed = true }
            end,
        },

        -- Finishes the walk to where the target was last known to be, and looks about once
        -- there, or once it finds no route there.
        search = {
            update = function(self, snapshot)
                local goal = snapshot.lastKnownTargetFeet
                local command = { routeTo = goal }
                if self.routed and (snapshot.routeComplete or not snapshot.hasRoute) then
                    command.aimDirection = lookAbout(snapshot)
                end
                self.routed = true
                return command
            end,
        },

        -- Backs away from where the target was last known to be while shooting at it. A
        -- walker moves straight left or right, and stops at a ledge rather than step off.
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
                return { direction = away, aimAt = threat, primaryAttackPressed = true }
            end,
        },

        -- Stands and looks about.
        watch = {
            update = function(self, snapshot)
                return { aimDirection = lookAbout(snapshot) }
            end,
        },
    },
}
