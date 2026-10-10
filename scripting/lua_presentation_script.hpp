#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "advanced_platformer/render/presentation_scripts.hpp"
#include "advanced_platformer/world/world.hpp"
#include "lua_script_diagnostic.hpp"

namespace advanced_platformer
{
    class LuaPresentationScript final : public PresentationScripts
    {
    public:
        LuaPresentationScript();
        ~LuaPresentationScript() override;
        LuaPresentationScript(LuaPresentationScript&& other) noexcept;
        LuaPresentationScript& operator=(LuaPresentationScript&& other) noexcept;
        LuaPresentationScript(const LuaPresentationScript&) = delete;
        LuaPresentationScript& operator=(const LuaPresentationScript&) = delete;

        void loadScript(const std::filesystem::path& path);
        void loadScriptText(std::string_view source, std::string sourceName = "Lua script");
        bool loaded() const;
        void setSoundNames(const std::vector<std::string>& names);

        PresentationEffects onEvent(const WorldEvent& event, bool player) override;

        const std::vector<LuaScriptDiagnostic>& diagnostics() const;
        std::vector<LuaScriptDiagnostic> takeDiagnostics();

    private:
        struct Implementation;
        std::unique_ptr<Implementation> implementation;
    };
}
