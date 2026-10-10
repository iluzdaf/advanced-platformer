local speed = 4

return {
    activities = {
        idle = {
            description = "Drift right, attacking while the target is known.",
            update = function(self, snapshot, dt)
                return {
                    direction = { x = speed * dt, y = 0 },
                    primaryAttackPressed = snapshot.facts.targetKnown,
                }
            end,
        },
    },
}
