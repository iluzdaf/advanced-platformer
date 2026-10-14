return {
    activities = {
        walk = {
            description = "Walk left.",
            update = function()
                return { direction = { x = -1, y = 0 } }
            end,
        },
    },
}
