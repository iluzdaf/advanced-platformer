local CaughtDistance = 16

return {
    activities = {
        flee = {
            update = function(self, snapshot)
                local threat = snapshot.targetFeet
                if threat == nil then
                    return { clearRoute = true }
                end

                if math.abs(threat.x - snapshot.feet.x) <= CaughtDistance then
                    return { clearRoute = true, aimAt = threat }
                end

                local side = snapshot.feet.x < threat.x and -1 or 1
                local footing = snapshot.footing
                local canStep = footing ~= nil
                    and ((side < 0 and footing.left) or (side > 0 and footing.right))
                if not canStep then
                    return { clearRoute = true, aimAt = threat }
                end

                return { clearRoute = true, direction = { x = side, y = 0 }, avoidLedges = true }
            end,
        },
    },
}
