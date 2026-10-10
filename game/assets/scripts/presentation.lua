return {
    onShot = function()
        return { sound = { name = "shot" } }
    end,
    onKnockback = function(event)
        if event.actor ~= "player" then
            return nil
        end
        return { shake = { duration = 0.15, magnitude = 2 } }
    end,
}
