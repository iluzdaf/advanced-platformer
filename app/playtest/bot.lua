local function towardExit(snapshot)
    if snapshot.exitFeet == nil then
        return { clearRoute = true }
    end
    if snapshot.routeComplete and snapshot.feet:distance(snapshot.exitFeet) > 8 then
        return { clearRoute = true, routeTo = snapshot.exitFeet }
    end
    return { routeTo = snapshot.exitFeet }
end

return {
    activities = {
        travel = {
            update = function(_, snapshot)
                return towardExit(snapshot)
            end,
        },

        fight = {
            update = function(_, snapshot)
                local command = towardExit(snapshot)
                local target = snapshot.targetCenter
                if target ~= nil then
                    command.aimAt = target
                    command.primaryAttackPressed = snapshot.facts.primaryReady
                end
                return command
            end,
        },
    },
}
