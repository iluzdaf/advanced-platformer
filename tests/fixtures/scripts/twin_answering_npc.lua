return {
    facts = {
        nearTarget = function(_, snapshot)
            return snapshot.facts.targetKnown
        end,
    },
    activities = {
        idle = {
            description = "Do nothing.",
            update = function()
                return nil
            end,
        },
    },
}
