return {
    facts = {
        nearTarget = function(_, snapshot)
            return snapshot.facts.targetKnown
        end,
    },
    activities = {
        idle = {
            update = function()
                return nil
            end,
        },
    },
}
