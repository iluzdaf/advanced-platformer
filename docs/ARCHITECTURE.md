# Architecture

The main boundaries, ownership rules, and update order in Advanced Platformer. Use
[CONTENT.md](CONTENT.md) for the files under `assets`, including Lua activities and hot
reload, [GLOSSARY.md](GLOSSARY.md) for the words the code uses, and
[README.md](../README.md) for building, running, and the quality checks.

## Project shape

- C++26, built with Homebrew LLVM. Gameplay rules run without a window.
- Structs hold state; namespace functions update it. Classes own collections and
  resources where lifetime and invariants matter: `World`, `Inventory`,
  `CameraController`, `SpriteRenderer`.
- Third-party source is git submodules under `external/`, each pinned to one commit.
  Versions and licences are in [THIRD_PARTY.md](../THIRD_PARTY.md).
- Out of scope: slopes, one-way or moving platforms, rigid-body physics, actors pushing
  one another, multiplayer, scripting beyond NPC activities and presentation effects,
  save games, an editor, an animation graph, a general ECS, and homing or piercing
  projectiles.

| Target                          | What it owns                                                                                                            | Dependencies                                                              |
| ------------------------------- | ----------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------- |
| `advanced_platformer_core`      | Simulation, navigation, and plain render-scene data                                                                     | GLM; no window, graphics API, JSON, or Lua                                |
| `advanced_platformer_scripting` | The Lua runtime behind the NPC activity boundary; `lua_npc_scripts.hpp` is its interface, its other headers are private | Core, Lua, sol2                                                           |
| `advanced_platformer`           | Application, content loading, graphics, UI, and debug tools                                                             | Core, scripting, GLFW, glad, ImGui, ImPlot, imgui-node-editor, stb, Glaze |
| `advanced_platformer_tests`     | Catch2 tests for the core, scripting, and the application code that needs no window                                     | Core, scripting, Glaze, Catch2                                            |

### Application folders

| Location              | Responsibility                                                                                           |
| --------------------- | -------------------------------------------------------------------------------------------------------- |
| `app/application.cpp` | Window events, input, fixed steps, pause and single step, UI requests, hot reload, and rendering         |
| `app/game`            | Session flow: `Game` owns the current level, catalogs, scripts, and camera; level composition and reload |
| `app/content`         | Content definitions, Glaze loaders, catalogs, validators, and the asset watcher                          |
| `app/graphics`        | Window and OpenGL context, ImGui session, viewport conversion, and sprite submission                     |
| `app/ui`              | HUD, inventory, exit hint, and completion UI                                                             |
| `app/debug`           | Debug snapshots and their ImGui presentation, the console, and the frame profile UI                      |
| `scripting`           | Lua VM, sandbox, `vec2` binding, and the activity adapter                                                |
| `assets`              | Levels, catalogs, Lua scripts, and the atlas                                                             |

- `GameLevel` keeps the level number, map, world, player spawn, actor definition names,
  and the placement id of each actor and pickup together. Replacing it starts a fresh
  world. A generated level also keeps its seed, so a restart or reload builds the same
  level.

### Headers, not modules

- Modules would parse the standard library once, enforce what each part exports, and
  stop one file's includes or macros changing how another reads a type.
- The tools the project relies on do not support them well enough yet. clangd supports
  modules experimentally. clang-tidy and `misc-include-cleaner` are built around
  `#include`, as is `tools/tidy_targets.py`. CMake's module support is best on Ninja,
  not the Makefiles generator the presets use. sccache caches module builds poorly.
- Revisit when clangd and clang-tidy support modules fully. If build time becomes the
  problem first, measure with `-ftime-trace` and try precompiled headers for the
  standard library and Glaze before modules.

## Runtime flow

- `application.cpp` gathers player input and applies UI requests before simulation.
- `FixedStep` runs at 60 Hz (`FixedDeltaSeconds`) and clamps a frame to 0.25 s before it
  enters the accumulator, so a stall does not cause a burst of catch-up steps.
- The application owns pause and single step. The fixed step resets across a pause, as
  across the inventory; the game only sees which steps it is asked to run.
- Input is cleared while the inventory is open, the game is complete, play was
  interrupted, or ImGui captures the keyboard. Without a gameplay cursor, only the
  attack is cleared.
- `Game::update` writes player intentions, runs the simulation, handles completion,
  updates the camera, then runs `updateWorldPresentation`: world events through the
  presentation scripts, the camera shake, and animation and cover fades, which pause
  while the exit opens.
- Rendering reads the resulting state at the available frame rate and never advances
  time.

[`updateWorldSimulation`](../src/world/world_simulation.cpp) owns this order:

| Order | Work                                                                                              |
| ----- | ------------------------------------------------------------------------------------------------- |
| 1     | Return if the level is complete; otherwise advance the world clock                                |
| 2     | While the exit is opening, update only the exit and return                                        |
| 3     | Build queued navigation connections within the step's budget                                      |
| 4     | Update NPC senses and target memory                                                               |
| 5     | Advance NPC machines and run their Lua activities into intentions                                 |
| 6     | Move actors, then pickups, resolving tile collision                                               |
| 7     | Update attacks, projectiles, which may break tiles, and projectile bursts                         |
| 8     | Apply damage and knockback, advance death timers, respawn the player                              |
| 9     | Detect pickups                                                                                    |
| 10    | Forget the Lua state of actors about to be removed, then apply queued requests                    |
| 11    | Open the exit when the player enters it with what it needs; complete the level once it has opened |

- Systems update existing objects while iterating. Spawns, removals, damage, item use,
  and collection go into `WorldRequests` and are applied after iteration.
- Combat visits both attack slots of every actor each step, pressing each with its own
  intention; facts are gathered per slot the same way, so scripts and machines never name a
  kind.
- `updateLifeState` alone applies damage. It stamps the hit on the world clock, sets a
  knockback's velocity, and owns death timers and respawning.
- For inventory clicks, `drawInterface` returns a slot request; the application passes
  it to `Game::useInventoryItem`, which applies that request alone.
- Each phase measures itself into the optional `FrameProfile`. Parent phases exclude
  child time.

## Coordinates and time

| Value           | Meaning                                                                                                   |
| --------------- | --------------------------------------------------------------------------------------------------------- |
| World axes      | X points right; Y points down                                                                             |
| `Aabb::topLeft` | The corner physics moves and measures collision from                                                      |
| Feet            | Middle of a body's bottom edge; spawns, placements, patrols, goals, and waypoints                         |
| Cell            | A tile position. The tile catalog declares the tile size (16) and every cell calculation takes it         |
| Internal image  | 320 × 180 pixels, integer-scaled and letterboxed in the window                                            |
| `deltaTime`     | Seconds for one update, a `float`; `requireSeconds` allows zero and rejects negative or non-finite values |
| Timer           | A `float` on the component its window belongs to, ticked by the one system that owns it                   |
| Stamp           | An `std::optional<double>` of the world clock when something happened, empty until it does                |

- Name the point being moved: `topLeft`, `feetOf`, `boxStandingOn`, or `moveFeetTo`.
  There is no general `setPosition`.
- Use a timer when one system owns a window's start and end: `coyoteRemaining`,
  `phaseTimeRemaining`, `targetMemoryRemaining`, `deathTimeRemaining`, `stateElapsed`.
  Use a stamp when several readers ask how long ago something happened:
  `lastDamageTimeSeconds`, `lastFiredTimeSeconds`, `lastLockedTouchTimeSeconds`,
  `openedTimeSeconds`.
- `World` advances the clock once at the start of each active step. `secondsSince`
  returns a stamp's age, or nothing when unset; a stamp from the future is rejected.
  The clock and stamps are `double`, so precision holds however long a session runs;
  an age is a `float`.
- Rendering reads stamps, the clock, and timers; it never ticks them. The hit flash's
  length is a rendering constant.
- A length content tunes is a duration field in seconds, checked like every other time.
- Timers and stamps belong to their world. Respawn clears the damage stamp, and nothing
  carries a stamp into another world.

## World ownership and identity

- `World` owns actors, projectiles, bursts, pickups, item definitions, the exit, noise
  events, the simulation clock, and the level's platformer connection cache.
- `TileMap` belongs to `GameLevel`, beside `World`.
- `World::addActor` validates the actor and assigns an `ActorId`. Zero is invalid; IDs
  are not reused within a world.
- `findActor` is a linear search. Keep IDs across updates and look actors up when needed.
- Pointers and references into the world are temporary views. Adding or removing
  objects, a reload, a restart, or a level change can invalidate them.
- A reload keeps the live `World`, so IDs, the clock, and projectiles survive it. A
  level change does not.

## Actor composition

Players and NPCs are configurations of one
[`Actor`](../include/advanced_platformer/actor/actor.hpp): a body, intentions, facing,
team, and life state, plus optional components.

| Capability           | Components                                                                                               | Rule                                                  |
| -------------------- | -------------------------------------------------------------------------------------------------------- | ----------------------------------------------------- |
| Movement             | `PlatformerMovement` or `FlyingMovement`                                                                 | Exactly one                                           |
| Climbing             | `SurfaceClimb`                                                                                           | Requires platformer movement                          |
| NPC control          | `NpcBrain`, `NpcPerception`, `NpcSenses`, `PathFollower`, `NpcMachine`                                   | Required together; `Patrol` is optional               |
| Attacks              | `primaryAttack` and `secondaryAttack`, each a `BiteAttack`, `RangedWeapon`, `ContactDamage`, or `Pounce` | Optional; at most one pounce, since it moves the body |
| Presentation         | `Sprite` and `Animator`                                                                                  | An animator needs a sprite and a set with `idle`      |
| Health and inventory | `Health`, `Inventory`                                                                                    | Optional                                              |

- Attack components need a non-neutral team for opponent filtering.
- `World::addActor` checks component combinations. Level validation checks placement
  against the map.
- Systems read the components they need. There is no actor subclass or virtual dispatch.
- Composing a definition creates fresh runtime state; the same definition placed again
  is another actor.

## Input and movement

- Player input and Lua activities produce the same `InputIntentions`. The left and right
  mouse buttons, and the `primaryAttackPressed` and `secondaryAttackPressed` commands,
  press the attack in that slot; what happens depends on the kind it holds.
- `InputState` keeps button edges until a fixed update consumes them.
- `InputProgram` is a timed sequence of intentions that navigation records and replays.
  The sequence and its replay belong to input, not to pathfinding.
- The mouse passes through the display viewport and camera into a world aim. Clicks
  outside the viewport are ignored, and gameplay input is cleared while ImGui captures
  it.
- Facing follows horizontal aim, then intended movement, then keeps its value. It flips
  the sprite and places a bite.

| Movement         | Rule                                                                                                                                                                                                               |
| ---------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Platformer       | Acceleration and braking, variable-height jumps, coyote time, jump buffering, normal and jump-release gravity, and a fall-speed limit                                                                              |
| Flying           | Normalised two-axis input at a configured speed; no gravity                                                                                                                                                        |
| Surface climbing | Holding a climbable wall or ceiling and moving along it; letting go or losing contact returns to platformer movement                                                                                               |
| Pounce           | A leap along the aim at `speed` from a floor, with at least `lift` upward, or from a wall or ceiling; in flight steering is ignored and gravity bends the arc, and landing or grabbing a surface starts a recovery |

- Movement produces velocity; `moveBody` moves the body and stops it on the axis that
  hit a tile. Platformer movement, flying movement, and pickups share it. Gravity's
  default rates live with `Body`.
- `avoidLedges` stops grounded walking before unsupported floor; a deliberate jump still
  works.
- `climbGrip` is `Hold`, `Release`, or `Keep`. `Keep` leaves the grip alone, so code
  that ignores climbing never knocks a climber off. While a route is followed, the
  follower sets it.
- Movement records whether a wall or the ledge guard blocked the last update; NPC policy
  reads it as `movementBlocked`. `contactDamage` separately asks combat for overlap
  damage.
- Further abilities follow `SurfaceClimb` and `Pounce`: an optional component with its
  own configuration and state, between intentions and collision, falling back to
  platformer movement.

## Tile map, collision, and validation

- `TileMap` is a rectangular, row-major array of tile IDs; zero is empty. A definition
  chooses `blocksMovement`, `blocksSight`, `climbable`, and what the tile `breaksInto`.
- One layer serves rendering, collision, sensing, and climbing.
- Actors, pickups, and exits are level data, not tile IDs. Tests build maps from text
  with `tests/support/tile_map_builder.hpp`, which supplies its own definitions.

| Tile  | Movement and projectiles | Sight | Hides what stands in it |
| ----- | ------------------------ | ----- | ----------------------- |
| Empty | Pass                     | Pass  | No                      |
| Stone | Block                    | Block | Nothing can stand in it |
| Glass | Block                    | Pass  | Nothing can stand in it |
| Grass | Pass                     | Block | Yes                     |

- Collision sweeps an arbitrary-sized AABB along X against nearby tile boxes, then along
  Y, and reports left, right, ground, and ceiling contacts. Actors do not collide with or
  push one another. The left, right, and bottom map edges block; the top is open.
- `segmentCast` finds where a line first touches an AABB. `segmentCastMovementBlockingTiles`
  serves projectiles and `segmentCastSightBlockingTiles` serves sensing; they share the
  geometry.
- A projectile breaks a tile only when its weapon `breaksTiles` and the tile names what it
  breaks into. The swap changes the cell's ID and the map logs the break.
  `updateProjectiles` alone takes a mutable map.
- `validateLevelActors` checks every spawn, the player's respawn, and every patrol
  endpoint for body clearance. Platformers also need ground support, except a climber's
  patrol endpoints, which may be on a wall or ceiling. Errors name the level, actor, and
  location.
- `playerCanReachExit` finds a path for the player from its respawn to the exit, in a
  cache of its own. Each time the search defers, the fill builds just the cell it asked
  for, so only the cells the search reaches are simulated. It ignores the exit's
  requirement and breakable tiles.
- Starting a generated level runs it on each seed in turn until one passes, so a restart,
  reload or F6 dump rebuilds a level the player can finish.

## NPC behaviour

| Part                | What it holds or does                                                                                   |
| ------------------- | ------------------------------------------------------------------------------------------------------- |
| `NpcSenses`         | Notice distance, standoff distance, memory duration, and search duration                                |
| `NpcPerception`     | `targetVisible` and `heardLanding`, replaced each sensing update                                        |
| `NpcBrain`          | The remembered target's ID, last known feet, and memory remaining                                       |
| `NpcFacts`          | A snapshot gathered each update from perception, memory, movement, and attacks                          |
| `NpcMachine`        | The machine from `machines.json`: active state, its elapsed time, and how long each transition has held |
| `npc_fact_rows.cpp` | The names a machine's `when` may use, each answered from the facts                                      |
| Lua activities      | What a state does; see [CONTENT.md](CONTENT.md#lua-activities)                                          |

- Sight detects the living opponent player within notice distance with clear line of
  sight, and stores the player's ID and feet.
- Shots and landings record `WorldEvent` values with the actor's feet at the time. The
  next sensing update offers the batch to every NPC, then discards it. Shots are heard
  through walls; a landing needs a grounded observer on the same ground run. Both use
  notice distance.
- Fresh sight wins over a heard position; otherwise the last eligible noise refreshes
  memory. Without either, memory counts down and the target is forgotten.
- Firing stamps `lastFiredTimeSeconds`. Cover fading reads its age against
  `ShotRevealSeconds`; combat keeps no reveal countdown.
- Sensing records observations only. Decisions belong to the machine and its activities.
- NPCs hand navigation the last known feet as a goal. Navigation never reads the hidden
  player's position.

### State machine

Each NPC update has three steps:

1. `gatherNpcFacts` collects sensing, memory, movement, attacks, and state time into
   `NpcFacts`.
2. `advanceNpcMachine` chooses the state. A transition fires once every fact in its
   `when` has held for `after` seconds; at most one fires per update.
3. A transition exits the old activity, resets the state's time, clears the route, and
   enters the new activity. The active activity updates, and its command requests a
   route, aim, or attack through `InputIntentions`. Movement and combat execute them
   later in the same step.

- Loading rejects a machine with no states, a repeated state name, a transition from or
  to an unknown state, a condition no fact row answers, or a hold that is not finite and
  non-negative, and names the transition.
- The engine supplies facts, routes, movement, and combat. Machines and scripts hold
  every policy.

### Lua activity boundary

- The core sees no Lua types. `NpcActivitySnapshot` is a copied view of what the NPC
  knows; `NpcActivityCommand` carries intentions and requests. Applying them, including
  pathfinding, is engine work.
- `LuaNpcScripts` loads each script into its own sandboxed environment: the base, math,
  string, and table libraries, no `require`, no files. Positions are `glm::vec2` bound
  as `vec2`, copied in and out; its constructor is a read-only global.
- Returned commands reject unknown fields, wrong types, and non-finite vectors.
- Each visit has a `self` table keyed by `ActorId`, script, and activity. Calls are
  protected and have an instruction budget. An error or invalid command records its
  source, script, activity, hook, and actor and yields no command. `print` is recorded
  the same way.
- A failed script replacement leaves the previous script in place. Removing an actor,
  replacing a level, or reloading content discards script-owned state.
- Scripts cannot emit noise or apply damage directly.
- `LuaPresentationScript` implements the core's `PresentationScripts` interface, as
  `LuaNpcScripts` implements `NpcActivityScripts`, running `presentation.lua` in the
  same sandbox with the same protected calls, budget, and diagnostics. It returns
  effects, never intentions; see [Effects](#effects).

## Navigation

- `findActorPath` is the one entry: an actor, a goal point, the map, the step, and the
  connection cache. It looks at the body and the moves it has, never at what the actor
  is doing.
- A flyer starts in the cell at its feet. A platformer starts where its bounds rest
  within a pixel of a floor, wall, or ceiling, the nearest along the surface. Airborne,
  it has no start and gets no result.
- A route location is a cell and a surface. A cell's floor, walls, and ceiling are
  separate places; only climbing searches use walls and ceilings.
- `route_search` is A\* over locations. The caller supplies the connections leaving a
  location as a `std::span` view, or nothing when they are not ready yet, which pauses
  the search there; a goal cell; a heuristic that never exceeds the real cost; and
  optionally a cost function, which the platformer search uses for its jump-start
  penalty.
- The result is a `NavigationPath` of waypoints: the feet at the end of each step, its
  traversal, and its recorded inputs. Cells and surfaces stay inside navigation.
- A goal is a point that need not be somewhere the actor can be. The search heads for
  the cell holding it and stops at the cheapest place in that cell.

| Status        | When                                                              | The path                                                                                            |
| ------------- | ----------------------------------------------------------------- | --------------------------------------------------------------------------------------------------- |
| `Found`       | The route reaches the goal's cell                                 | Ends at the cheapest location in that cell, with the distance from its end to the goal point        |
| `Unreachable` | Everything reachable from the start was tried                     | Ends in the reached cell nearest the goal, with that distance; it has no waypoints if already there |
| `Deferred`    | The cache lacks a cell the search needs; flying paths never defer | None. The caller asks again once the fill has built the cell                                        |

| Movement   | Connections                                                                                                            | Cost                                                                                                                  |
| ---------- | ---------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------- |
| Flying     | Four neighbouring open cells                                                                                           | One per cell; Manhattan heuristic                                                                                     |
| Platformer | Walks, falls, jumps and, for a climber, climbs and release falls, found by running the real movement at the NPC's step | Simulation ticks, plus a fixed penalty per jump; the heuristic is the ticks to cross the columns between at top speed |

### Traversals

A traversal that needs an optional capability is tried only for a profile with it. A new
capability adds a row here, its connections in `platformer_connections`, and its config
to the traversal profile. The search itself does not change.

| Traversal | Requires       | Tried from → to                                                                                    | Accepted when                                                   | Replayed by the follower                                                                                          |
| --------- | -------------- | -------------------------------------------------------------------------------------------------- | --------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------- |
| Walk      | —              | Floor → each standable floor cell along the row, in both directions                                | The body stops within a pixel of the cell                       | Walking and braking into the cell; no inputs are recorded                                                         |
| Fall      | —              | Floor → off the edge beside it                                                                     | It lands on another standable cell and stops there              | Stopping at the takeoff, then the recorded inputs                                                                 |
| Fall      | `SurfaceClimb` | Wall or ceiling → letting go, straight down                                                        | It lands on a standable cell other than its own and stops there | Travelling the surface to the start, then the recorded inputs                                                     |
| Jump      | —              | Floor → a short and a full-height jump each way                                                    | It lands on another standable cell and stops there              | Stopping at the takeoff, then the recorded inputs                                                                 |
| Climb     | `SurfaceClimb` | Floor → wall beside it; wall or ceiling → next cell along it, round a corner, or down to the floor | The body settles at the destination's resting bounds            | Holding a surface, travelling it to the start; standing, stopping there; in the air, grabbing on; then the inputs |

### The connection cache

- A cell's connections depend only on the map, the cell, and a
  `PlatformerTraversalProfile` (body size, movement configuration, step, and optional
  climb), so actors with equal profiles share them.
- `PlatformerConnectionCache`, owned by `World`, stores per profile each cell's
  connections from every surface, sorted by the surface they leave so a search gets one
  surface as a single view, with the footprint their simulation swept; and walk results
  by length, reusable from any floor.
- The search only reads the cache. The fill alone builds connections:
  `queueNavigationFill` queues every cell for each distinct NPC profile when a level
  starts, and `advanceNavigationFill` builds queued cells each step within a budget of
  simulated ticks plus a charge per cell, shared among profiles. A search that reaches a
  missing cell queues it at the front and returns `Deferred`.
- A tile break drops only the cells whose footprint contains it, grown a tile for what
  collision reads beside the body. Walks stay. Searches and fills apply recorded breaks
  before using the cache, dropped cells rejoin the fill queue, and an NPC plans again
  after any break.
- Optional frame profiling counts searches, expanded cells, and deferred searches.

### Following a path

| Traversal           | What the follower asks for                                                |
| ------------------- | ------------------------------------------------------------------------- |
| Fly                 | Steer at each waypoint, shortening the last move so it does not overshoot |
| Walk                | Walk and brake into the waypoint                                          |
| Fall or Jump        | Stop at the takeoff, then replay the recorded inputs                      |
| Fall from a surface | Travel the surface to the start, then replay the recorded release         |
| Climb               | Reach the start on the surface, hold it, then replay the recorded inputs  |

- The follower emits intentions; movement moves the body. It never teleports it or
  writes velocity. Between steps `climbGrip` stays `Keep`.
- Arrival is the feet reaching the waypoint. A jump or fall is done once the body lands
  and stops on the waypoint's row. A step that ends elsewhere drops the path, and the
  NPC plans again.
- The follower remembers the goal its path was planned for and how the search ended:
  found, unreachable, with a path to the nearest reachable cell, or deferred, with no
  path until the fill catches up. The snapshot passes that status to scripts; following
  a partial path is their choice. The NPC plans again when the goal moves more than 8
  pixels.
- End-to-end tests replay generated programs through the real simulation, so planning
  and movement cannot drift apart.

## Combat, projectiles, and life cycle

| Subject        | Rule                                                                                                                                                    |
| -------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Bite           | Ready → Windup → Active → Recovery; a forward hitbox by facing damages each opponent once per bite, and a committed bite completes                      |
| Ranged weapon  | Ready → Shoot → Recovery; entering Shoot queues one projectile along the aim, in any direction                                                          |
| Contact damage | While its slot is held by a living actor, body overlap damages each opponent once; releasing or dying clears the hit history                            |
| Pounce         | While airborne from a pounce, body overlap damages each opponent once per leap, with the pounce's knockback                                             |
| Knockback      | Optional on contact damage and pounces: the victim is thrown away from the attacker at `speed` and up at `lift`, by body centres, facing breaking a tie |
| Projectile     | A swept cast picks the earliest blocking tile or eligible actor; any hit ends it and may break the tile; owner and team exclude shooter and allies      |
| Burst          | A short visual queued where a projectile ends, recording impact or expiry; no collision or damage                                                       |
| Damage         | Queued by combat and projectiles, applied by `updateLifeState`, and stamped on the world clock; a knockback sets velocity and clears grounded           |
| Death          | Health at zero enters Dying with a timer; dying actors take no intentions or damage but still move and collide                                          |

- The death timer removes an NPC or respawns the player at the stored spawn feet with
  restored health and movement state. Inventory persists.
- Death timing is independent of animation length; sprites fade in its last part.

## Inventory, pickups, and levels

- `item.hpp` holds item data; `inventory.hpp` owns slots and stacking; `item_use.hpp`
  applies effects through an explicit switch; `pickup.hpp` detects collection;
  `level_exit.hpp` evaluates completion.
- Inventory fills compatible stacks, then empty slots, and reports what did not fit.
- Pickups have bodies, fall at the default gravity, and rest on tiles, so a key on glass
  drops when the glass breaks. The living player collects them on body overlap; excess
  quantity remains. Their bob is computed at draw time.
- An exit may require and consume an item, and opening is latched. Standing in a locked
  exit stamps the touch for the HUD hint. Entering with the requirement consumes it and
  stamps the opening; for `ExitOpenSeconds` only the clock runs and the screen fades the
  player into the door, then the level completes.
- `Game` replaces `GameLevel` at a transition, carrying only the player's health and
  inventory, and resets the camera. Velocities, projectiles, NPC and Lua state, and old
  IDs do not cross. The final exit completes the game; a restart loads the start level.

### Data-driven level boundary

| Step                 | Owner                                | Result                                                                             |
| -------------------- | ------------------------------------ | ---------------------------------------------------------------------------------- |
| Load shared catalogs | `app/content/game_catalogs.cpp`      | Definitions checked against the atlas size and kept for the session                |
| Load scripts         | `app/content/npc_script_catalog.cpp` | Each script a machine names, with every named activity present                     |
| Load a level         | `app/content/level_data.cpp`         | Plain `LevelData` of explicit placements, each actor and pickup with an id         |
| Compose the level    | `app/game/level_composition.cpp`     | Names resolved into a map, world, and placed objects                               |
| Start the level      | `composeStartedLevel`, then `Game`   | Player inserted, placements validated, camera made, and the navigation fill queued |

- JSON stays in `app/content`; the core receives C++ values. Each file is read with
  Glaze through `content_glaze` into structs that mirror it, so unknown keys, missing
  required members, and wrong types fail with a line and column. `WithDefaults<T>` keeps
  C++ defaults for fields left out; enums and one-of fields are Glaze metadata.
  `content_diagnostics` builds field paths without a JSON dependency.

| Verb          | Example                              | Meaning                                                                   |
| ------------- | ------------------------------------ | ------------------------------------------------------------------------- |
| `parse...`    | `parseItemCatalog(text, sourceName)` | Text to typed data. Never opens a file, so tests pass a string.           |
| `load...`     | `loadItemCatalog(path)`              | Reads the file, then calls the matching `parse...`.                       |
| `validate...` | `validateItemCatalog(catalog)`       | Authoring rules on typed data, whether it came from JSON or C++.          |
| `compose...`  | `composeActor(definition, ...)`      | Authoring data and runtime context, such as a spawn position, to a value. |
| a noun        | `itemDefinition(catalog, name)`      | A lookup that throws when the name is unknown.                            |

### Hot reload

- Builds other than Release define `ADVANCED_PLATFORMER_SOURCE_ASSETS`. The application
  reads content from there and polls an `AssetWatcher`, which reports a change once two
  polls in a row see the same files.
- `Game::reload` is all or nothing. It loads and composes everything, merges the result
  into a copy of the live level with `reloadLevel`, then swaps in the level, catalogs,
  and scripts; the atlas is uploaded after. An error changes nothing and is reported to
  the console.
- The merge keeps the live `World`. What it keeps and replaces is in
  [CONTENT.md](CONTENT.md#hot-reload).

## Presentation

- `Game::buildScene` returns ordered `SpriteDrawCommand` values; `SpriteRenderer`
  submits them with one shader. Gameplay state flows one way into render-scene data and
  then into OpenGL. There is no scene graph, material system, lighting, or render graph.
- `updateWorldPresentation` advances animation and cover fades after simulation and
  pauses while the exit opens.
- Scene construction computes pickup bobbing, hit flashes, death fading, and bursts from
  state and timers.
- `CameraController` starts centred on the player, moves only to return the player to a
  dead zone sized by the level catalog, clamps to the map, and rounds to internal pixels.
- `CameraShake` offsets only the drawn view. A shake has a duration and a magnitude,
  fades linearly, and a new one replaces it. Follow, aim, and the debug overlay use the
  steady camera.
- `DisplayViewport` is shared by rendering, aiming, HUD, and debug UI, so they agree
  about letterboxing and high-DPI coordinates.
- Core tests cover scene construction, camera transforms, visible tiles, placement,
  flips, rotation, and draw order. The graphics driver is checked by running the game.

| Size           | Meaning                                                                                         |
| -------------- | ----------------------------------------------------------------------------------------------- |
| Texture        | The atlas, 256 × 256 source pixels                                                              |
| `SpriteRegion` | A source rectangle, drawn at one world pixel per source pixel; a tile's region is the tile size |
| Body bounds    | Collision size, independent of the sprite                                                       |
| Sprite anchor  | Art placed at the body's feet (default) or centre; it does not change collision                 |

### Effects

- A simulation step records `WorldEvent` values: the actor, its feet, the kind, and for
  a knockback the velocity. Senses take the audible kinds; `Game` takes them all after
  the step.
- `updateWorldPresentation` offers each event to `PresentationScripts`, an interface
  the core owns and the Lua script implements, and applies the effects it returns. The
  one effect is `shake`, with a duration and a magnitude.
- Continuous presentation derived from world state, such as animation and cover fades,
  is engine code. Discrete effects answering an event are the script's decision.

| Hook          | Event                     |
| ------------- | ------------------------- |
| `onLanding`   | A platformer actor landed |
| `onShot`      | A ranged weapon fired     |
| `onKnockback` | A hit threw its target    |

### Animation

- Clips come from `animations.json`. `selectAnimation` walks `AnimationPriority`,
  death, pounce, bite, shoot, jump, fall, move, idle, and takes the first state that holds
  and has a clip in the set; a missing clip falls through to the next state, and `idle`
  always ends the walk. There is no animation state machine.
- Each actor owns an `Animator` and its `AnimationSet`. Frames in one set share a size,
  and collision bodies are independent of frame size.
- A climber holding a surface counts as grounded. `placeActorSprite` turns its art onto
  the surface, a quarter turn on a wall and a half turn on a ceiling, the head leading by
  `wallHeading` or facing. The collider does not turn.

### Cover

- NPC sight is line of sight. Separately, NPCs and pickups fade on screen by how much of
  their body is in a sight-blocking tile they can stand in: fully visible at half or less,
  not drawn from three quarters, unless the player has line of sight to them. The fade
  eases over `CoverFadeSeconds`.
- The player's sprite darkens by `PlayerConcealedShade` in cover rather than fading, and
  is fully exposed while any NPC's `targetVisible` saw them this update or for
  `ShotRevealSeconds` after firing.

### HUD, debug tools, and logging

- `drawInterface` is the ordered list of what the player sees over the scene. Built
  before the simulation, it returns `InterfaceRequests` the loop applies, so building the
  interface never changes the game and a click on the bag pauses the same frame.
- The inventory UI derives its rows from the slot count, uses at most three columns,
  pauses the simulation while open, and emits item-use requests.
- `Game::debugOverlay` builds a `DebugOverlay` snapshot without ImGui, limited to the
  camera and a margin. `drawDebugTools` projects it through `DisplayViewport` and owns
  the profiling, selection, and graph-editor state; `machine_graph_ui` shows the
  selected NPC's machine. The keys are in [README.md](../README.md#debug-overlay).
- One `ConsoleLog` takes script errors and prints, GLFW errors, and reload reports,
  echoes each line to standard error, and keeps the latest 500 for the console window.
- Frame profiling is optional. The application passes a `FrameProfile` into the
  simulation, phases time themselves with nested scopes and add named statistics, and
  `FrameHistory` keeps completed records for the plot.

## Error handling and validation

| Boundary                           | What it checks                                                                 |
| ---------------------------------- | ------------------------------------------------------------------------------ |
| Glaze loaders                      | Types, required and unknown fields, and rules a struct cannot state, by path   |
| Content validators and composition | Authoring rules and cross-file references, including unused catalog entries    |
| Core validators                    | Runtime values and component combinations, regardless of how they were created |
| Level validation                   | Body clearance and support against the composed map, and a route to the exit   |

- Domain checks run without parsing JSON; loaders add the source name and field path to
  their errors.
- Missing optional fields keep defaults; present invalid values fail.
- Invalid content or programmer input throws at the boundary that can explain it.
  Required assets fail startup with a message rather than falling back to placeholders.
- A failed hot reload reports to the console and keeps the running content.

## Testing and quality checks

- Tests mirror the source subjects and check behaviour, not private implementation.
  Within a file, cases are grouped by sub-subject with the most important first.
- Tests build their own maps, actors, and content, with helpers in `tests/support` and
  files in `tests/fixtures`. Checks on the shipped content test only that it is valid.
- Generated input programs are replayed through the real simulation.
- OpenGL and ImGui are checked by running the game. [README.md](../README.md) lists the
  formatting, static analysis, coverage, and CI checks.
