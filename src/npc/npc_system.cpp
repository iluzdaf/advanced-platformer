#include "advanced_platformer/npc/npc_system.hpp"

#include <optional>
#include <stdexcept>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/combat/attack_system.hpp"
#include "advanced_platformer/input/input_state.hpp"
#include "advanced_platformer/math/validation.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/navigation/actor_navigation.hpp"
#include "advanced_platformer/navigation/path_follower.hpp"
#include "advanced_platformer/navigation/platformer_cells.hpp"
#include "advanced_platformer/npc/npc.hpp"
#include "advanced_platformer/npc/npc_activity.hpp"
#include "advanced_platformer/npc/npc_activity_scripts.hpp"
#include "advanced_platformer/npc/npc_facts.hpp"
#include "advanced_platformer/npc/npc_navigation.hpp"
#include "advanced_platformer/npc/npc_scripted_activity.hpp"
#include "advanced_platformer/npc/npc_senses.hpp"
#include "advanced_platformer/npc/npc_state_machine.hpp"
#include "advanced_platformer/npc/npc_update.hpp"
#include "advanced_platformer/timing/frame_profile.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "advanced_platformer/world/world.hpp"

namespace advanced_platformer
{
    namespace
    {
        // Which state comes next is decided once, from the facts, before the state acts. A
        // transition exits the old state's activity, then the new state's activity is
        // entered and updated on the same tick, with facts gathered again for it.
        void updateNpcState(const NpcUpdate& update, Actor& actor)
        {
            if (!actor.brain.has_value() || !actor.perception.has_value() ||
                !actor.pathFollower.has_value() || !actor.machine.has_value())
            {
                throw std::logic_error("An NPC is missing behaviour components");
            }
            NpcBrain& brain = *actor.brain;
            const NpcPerception& perception = *actor.perception;
            PathFollower& follower = *actor.pathFollower;
            NpcMachine& machine = *actor.machine;
            const Actor* target = livingTarget(update.world, brain);

            NpcFacts facts =
                gatherNpcFacts(update.map, actor, brain, perception, target, machine.stateElapsed);
            const LuaNpcActivity previous = activeNpcMachineState(machine).does;
            if (advanceNpcMachine(machine, facts, update.deltaTime).has_value())
            {
                if (machine.activityEntered)
                {
                    exitScriptedActivity(update, actor, brain, follower, target, previous, facts);
                    machine.activityEntered = false;
                }
                facts = gatherNpcFacts(
                    update.map, actor, brain, perception, target, machine.stateElapsed);
            }

            const LuaNpcActivity& activity = activeNpcMachineState(machine).does;
            if (!machine.activityEntered)
            {
                enterScriptedActivity(update, actor, brain, follower, target, activity, facts);
                machine.activityEntered = true;
            }
            updateScriptedActivity(update, actor, brain, follower, target, activity, facts);
            machine.stateElapsed += update.deltaTime;
        }
    }

    void updateNpcBehaviour(
        const TileMap& map,
        World& world,
        float deltaTime,
        NpcActivityScripts* scripts,
        FrameProfile* profile)
    {
        requireSeconds(deltaTime, "NPC behaviour time step");
        const NpcUpdate update{map, world, deltaTime, scripts, profile};

        for (Actor& actor : world.actors())
        {
            if (!actor.brain.has_value())
            {
                continue;
            }

            actor.intentions = {};
            if (actor.life == LifeState::Alive)
            {
                updateNpcState(update, actor);
            }
        }
    }
}
