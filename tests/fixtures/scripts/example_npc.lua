local speed = 4

return {
    activities = {
        idle = {
            update = function(self, snapshot, dt)
                return {
                    direction = { x = speed * dt, y = 0 },
                    primaryAttackPressed = snapshot.facts.targetKnown,
                }
            end,
        },
    },
}
