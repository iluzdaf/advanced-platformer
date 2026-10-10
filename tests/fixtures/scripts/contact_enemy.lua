return {
    activities = {
        attack = {
            description = "Keep pressing the primary attack.",
            update = function()
                return { primaryAttackPressed = true }
            end,
        },
    },
}
