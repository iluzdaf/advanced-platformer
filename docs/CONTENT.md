# Content Format

The authoring reference for the files under `game/assets`. Room pieces place named definitions;
definitions configure engine components; machines and Lua activities decide what NPCs
do. Movement, combat and pathfinding stay in C++.

- Unknown fields are rejected, so misspellings are reported.
- Errors name the file and either a field path (`items.herb.maximumStack`, `map[2][7]`)
  or a line and column (`items.json: line 5, column 7: unknown field 'maximimStack'`).
- Every shared definition is validated, even when no level uses it.
- Catalogs, room pieces and Lua scripts load at startup. Debug builds also reload them
  while the game runs; see [Hot reload](#hot-reload).
- Units are pixels, seconds and pixels per second. Sprite regions are atlas pixels.

## Hot reload

Builds other than Release read `game/assets/` from the source tree, not the copy beside the
executable, and check it four times a second. Once a change has settled, the game loads
every catalog, room piece, script and the atlas again, and builds the level again from
the same seed. If anything fails,
the console shows the error and the game keeps running what it had. Otherwise the game
applies the new content without restarting:

- Actors and pickups are matched to their placements by `id`, which names the room, the
  definition and a count, such as `room3_zombie_1`. A kept one stays where it
  is, keeps what it was doing, and takes its new definition, spawn and patrol. Health is
  kept but capped at the new maximum.
- A new `id` spawns. An `id` gone from the level removes its actor or pickup. An actor
  killed or a pickup collected stays gone while its `id` remains.
- The player keeps their position, health and items, matched to items by name.
- Tiles broken in play stay broken. Machines resume in the state with the same name, or
  the first state, and every activity starts again under the new scripts.

The console reports what was kept, spawned and removed. F5 restarts the current level
from the same seed, keeping the player's health and items. F6 builds the current level
again from the next seed, also keeping health and items.

## Files

| File                                                                  | Holds                                          | Loader                                                                                                                        |
| --------------------------------------------------------------------- | ---------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------- |
| [`catalogs/pieces.json`](../game/assets/catalogs/pieces.json)         | Room size, legend, and how a run's levels grow | [`room_pieces.cpp`](../game/content/room_pieces.cpp), [`level_generator.cpp`](../core/src/level/level_generator.cpp)          |
| [`catalogs/pieces/*.json`](../game/assets/catalogs/pieces)            | One room piece each                            | [`room_pieces.cpp`](../game/content/room_pieces.cpp)                                                                          |
| [`catalogs/tiles.json`](../game/assets/catalogs/tiles.json)           | Tile size and tiles                            | [`tile_catalog.cpp`](../game/content/tile_catalog.cpp)                                                                        |
| [`catalogs/actors.json`](../game/assets/catalogs/actors.json)         | The player and every actor definition          | [`actor_catalog.cpp`](../game/content/actor_catalog.cpp), [`actor_definition.cpp`](../game/content/actor_definition.cpp)      |
| [`catalogs/animations.json`](../game/assets/catalogs/animations.json) | Animation sets                                 | [`animation_catalog.cpp`](../game/content/animation_catalog.cpp)                                                              |
| [`catalogs/machines.json`](../game/assets/catalogs/machines.json)     | NPC state machines                             | [`machine_catalog.cpp`](../game/content/machine_catalog.cpp)                                                                  |
| [`scripts/*.lua`](../game/assets/scripts)                             | Lua activities                                 | [`npc_script_catalog.cpp`](../game/content/npc_script_catalog.cpp), [`lua_npc_scripts.cpp`](../scripting/lua_npc_scripts.cpp) |
| [`scripts/presentation.lua`](../game/assets/scripts/presentation.lua) | Effects that answer world events               | [`lua_presentation_script.cpp`](../scripting/lua_presentation_script.cpp)                                                     |
| [`catalogs/items.json`](../game/assets/catalogs/items.json)           | Inventory items                                | [`item_catalog.cpp`](../game/content/item_catalog.cpp)                                                                        |
| [`catalogs/pickups.json`](../game/assets/catalogs/pickups.json)       | World pickups                                  | [`pickup_catalog.cpp`](../game/content/pickup_catalog.cpp)                                                                    |
| [`catalogs/exits.json`](../game/assets/catalogs/exits.json)           | Exit bodies and sprites                        | [`exit_catalog.cpp`](../game/content/exit_catalog.cpp)                                                                        |
| [`catalogs/hud.json`](../game/assets/catalogs/hud.json)               | HUD icon regions                               | [`hud_catalog.cpp`](../game/content/hud_catalog.cpp)                                                                          |
| [`catalogs/sounds.json`](../game/assets/catalogs/sounds.json)         | Procedural sound patches                       | [`sound_catalog.cpp`](../game/content/sound_catalog.cpp)                                                                      |
| [`catalogs/camera.json`](../game/assets/catalogs/camera.json)         | Camera dead zone                               | [`camera_settings.cpp`](../game/content/camera_settings.cpp)                                                                  |

Every catalog is required, even when empty. Every sprite region, frame and icon must lie
inside the atlas.

## Run

A run is endless. It starts at level 1, and each exit leads to the next level. When the
player dies, the run starts again at level 1 with full health, no items and a new run
seed. Each level's seed follows from the run seed and the level number, so a run seed
always gives the same levels. The `run` block of [`pieces.json`](#room-pieces) says how
the levels grow:

```json
"run": { "firstRooms": 6, "roomsPerLevel": 3, "maxRooms": 18 }
```

| Field           | Required | Meaning                                                          |
| --------------- | -------- | ---------------------------------------------------------------- |
| `firstRooms`    | Yes      | How many rooms level 1 has, from 2 up to `maxRooms`.             |
| `roomsPerLevel` | Yes      | How many rooms each later level adds. Zero or more.              |
| `maxRooms`      | Yes      | The most rooms a level has, from 2 up to the number of slots.    |
| `grid`          | No       | The grid of room slots, `[columns, rows]`. Defaults to `[9, 7]`. |

The rooms grow from the centre slot of the grid, each new room opening off one room
already placed and touching no other, so they form branching corridors without loops.
The start is the centre room, and the exit is the room the most doors away from it. A
seed whose exit the player cannot reach is skipped for the next one, up to 100 seeds,
and the level keeps the seed it settled on. A hot reload keeps that seed, and an edit
that cuts off its exit is rejected like any other failed reload.
Each room takes a random piece of its role whose doors are exactly the room's, flipped
if the piece allows it. The pieces are copied onto one map with neighbours sharing the
wall between them, and empty slots are filled with the start piece's corner tile. Each piece's placements move with it,
so the level has the start piece's player spawn, the exit piece's exit, and every
piece's actors and pickups. Coordinates start at the top-left, with Y pointing down. A
pickup falls until it rests on a tile, and falls again if that tile breaks.

## Room pieces

`catalogs/pieces.json` describes every piece, and each piece is its own file in
`catalogs/pieces/`, named after the file: `pieces/hall.json` is the piece `hall`. The
loader reads the folder in file-name order.

```json
{
  "roomSize": [20, 12],
  "run": { "firstRooms": 6, "roomsPerLevel": 3, "maxRooms": 18 },
  "tileLegend": { ".": "empty", "#": "stone" }
}
```

| Field        | Meaning                                                                              |
| ------------ | ------------------------------------------------------------------------------------ |
| `roomSize`   | Every piece's size in cells: an even width of at least 8 and a height of at least 6. |
| `run`        | How many rooms each level of a [run](#run) has.                                      |
| `tileLegend` | One-character map symbols to tile names in `tiles.json`, with a symbol for `empty`.  |

```json
{
  "role": "corridor",
  "doors": ["left", "right"],
  "map": ["####################", "#..................#", "..."],
  "actors": [
    {
      "id": "zombie_1",
      "definition": "zombie",
      "spawn": [4, 10],
      "patrol": { "first": [2, 10], "second": [9, 10] }
    }
  ],
  "pickups": [{ "id": "coin_pile_1", "definition": "coin_pile", "spawn": [12, 10] }]
}
```

A piece file has `role`, `doors`, optional `mirror` (default `true`), `map`, the piece's
rows, and the placements below.

A piece's `role` is `start`, `exit`, `corridor`, `shaft` or `arena`. Its `doors` list
some of `left`, `right`, `up` and `down`. Side doors are three cells tall and stand on
the bottom row; doors above and below are four cells wide and centred. A piece's edges
are the `empty` symbol on its doors and solid everywhere else. With `mirror`, the
generator may also flip the piece left to right, placements included.

| Placement     | Fields                                                                                                                                                                               |
| ------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `playerSpawn` | A cell. Required on a `start` piece, allowed on no other.                                                                                                                            |
| `exit`        | `definition` from `exits.json`; `spawn`; optional `requirement` (`item`, positive `quantity`) and `consumeItem` (default `false`). Required on an `exit` piece, allowed on no other. |
| `actors`      | `id`; `definition` from `actors.json`; `spawn`; optional `patrol` with `first` and `second`.                                                                                         |
| `pickups`     | `id`; `definition` from `pickups.json`; `spawn`.                                                                                                                                     |

Every cell is `[column, row]` inside the piece, counted from its top-left. An `id` is
nonempty and unique among the piece's actors and pickups; the generator prefixes it with
the room, as in `room3_zombie_1`.

When content loads, every piece's placements are checked on the piece alone, and again
mirrored when `mirror` allows it. An actor's spawn and patrol ends, and a pickup's spawn,
must not overlap a blocked tile. A walker's spawn and patrol ends need ground under them.
A climber may instead spawn in a cell under a climbable ceiling or beside a climbable
wall, and starts holding it, the ceiling first; its patrol ends may be on a wall or ceiling. An actor with a `patrol` must be able
to reach both its ends from its spawn. A flyer must spawn in open air, with no blocked tile
right under it. Errors name the piece, `mirrored` when it is the flipped copy, and the placement's
`id`. Every pickup must be within the player's reach, or rest on a breakable tile that
drops it within reach; the content tests check this.

Each room needs a piece of its role with exactly its doors, so the shipped catalog
covers every door set a room can have; a level that has a room no piece fits fails to
build and names the doors. Tests check that the player can
walk and jump through every shipped piece from each of its doors to each other.

## Tiles

`tiles.json` has `tileSize`, the side of a tile in world pixels (16), and `tiles`, by name.

| Field            | Required | Meaning                                                                                |
| ---------------- | -------- | -------------------------------------------------------------------------------------- |
| `blocksMovement` | Yes      | Solid to bodies.                                                                       |
| `blocksSight`    | Yes      | Stops NPC sight.                                                                       |
| `sprite`         | \*       | `{ "position": [x, y] }`; the region is one tile square. `empty` has none.             |
| `climbable`      | No       | A climber can grip its walls and underside. Map edges never are.                       |
| `breaksInto`     | No       | The tile it becomes when a `breaksTiles` shot hits it. Chain tiles to break in stages. |

\* Required on every tile but `empty`, which must allow movement and sight. Speeds and
jump heights are tuned for 16-pixel tiles.

## Actors

```json
{
  "player": "hero",
  "actors": {
    "hero": {
      "bodySize": [12, 20],
      "team": "player",
      "health": 3,
      "inventorySlots": 6,
      "animations": "player",
      "movement": { "platformer": {} }
    },
    "bat": {
      "bodySize": [12, 8],
      "team": "enemy",
      "health": 1,
      "animations": "bat",
      "spriteAnchor": "center",
      "movement": { "flying": { "speed": 40 } },
      "senses": { "noticeDistance": 60 },
      "machine": "pursuer",
      "primaryAttack": { "bite": {} }
    }
  }
}
```

`player` names the player's definition, which needs `health` and `inventorySlots` and
no `senses`.

| Field             | Meaning                                                                                                                   |
| ----------------- | ------------------------------------------------------------------------------------------------------------------------- |
| `bodySize`        | Required. The collider.                                                                                                   |
| `team`            | `player`, `enemy` or `neutral` (default). Attacks need a non-neutral team.                                                |
| `facing`          | `left` or `right` (default).                                                                                              |
| `animations`      | A set in `animations.json`.                                                                                               |
| `spriteAnchor`    | `feet` (default) or `center`.                                                                                             |
| `health`          | Positive.                                                                                                                 |
| `inventorySlots`  | Positive.                                                                                                                 |
| `movement`        | Required. An object with one key: `platformer`, walking and jumping, or `flying`.                                         |
| `surfaceClimb`    | Climbing walls and ceilings. Needs platformer movement.                                                                   |
| `senses`          | Makes the actor an NPC. Needs `machine`.                                                                                  |
| `machine`         | A machine in `machines.json`. Needs `senses`.                                                                             |
| `primaryAttack`   | An attack: an object with one key, `bite`, `ranged`, `contact` or `pounce`. Pressed by the left mouse button or a script. |
| `secondaryAttack` | A second attack, the same way. Pressed by the right mouse button or a script. At most one of the two is a pounce.         |

A component object may leave out any field to keep its default, so `{}` is all defaults.
Where a field takes one of several forms, as `movement`, the attacks and positions do, the
key names the form and its value is that form's object.

| Component        | Fields (default)                                                                                                                                                                                                                                      |
| ---------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `platformer`     | `maximumSpeed` (100), `groundAcceleration` (800), `airAcceleration` (400), `groundDeceleration` (1000), `jumpSpeed` (240), `gravity` (800), `jumpReleaseGravity` (1600), `maximumFallSpeed` (600), `coyoteDuration` (0.1), `jumpBufferDuration` (0.1) |
| `flying`         | `speed` (60)                                                                                                                                                                                                                                          |
| `surfaceClimb`   | `speed` (60)                                                                                                                                                                                                                                          |
| `senses`         | `noticeDistance` (96), `targetMemoryDuration` (1.5), `searchDuration` (2), `standoffDistance` (48)                                                                                                                                                    |
| attack `bite`    | `damage` (1), `hitboxSize` ([10, 8]), `reach` (4), `windupDuration` (0.12), `activeDuration` (0.08), `recoveryDuration` (0.3)                                                                                                                         |
| attack `ranged`  | `damage` (1), `projectileSize` ([4, 2]), `projectileSpeed` (180), `projectileLifetime` (2), `shootDuration` (0.15), `recoveryDuration` (0.2), `breaksTiles` (false), `sprite`                                                                         |
| attack `contact` | `damage` (1), `knockback` (none): `speed` (150), `lift` (120), the push away from the attacker and the lift, in pixels per second. Damages while held.                                                                                                |
| attack `pounce`  | `damage` (1), `knockback` (none), `speed` (200), `lift` (120), `range` (64), `recoveryDuration` (0.5). Needs `platformer`; damages while in the air.                                                                                                  |

A `sprite`, here and for items, pickups and exits, has `position` and `size`, the atlas
region, and `anchor`, `feet` (default) or `center`. A sprite draws at its region's size:
one atlas pixel is one world pixel. To change how big something looks, change the art.

## State machines

`machines.json` holds machines by name. A machine's first state is the one an NPC starts in.

```json
"pursuer": {
  "states": [
    { "name": "patrol", "does": { "script": "common", "activity": "patrol" } },
    { "name": "chase", "does": { "script": "common", "activity": "chase" } }
  ],
  "transitions": [
    { "from": "patrol", "to": "chase", "when": { "targetKnown": true } },
    { "from": "chase", "to": "patrol", "when": { "targetKnown": false }, "after": 0.5 }
  ]
}
```

| Field   | Meaning                                                                                             |
| ------- | --------------------------------------------------------------------------------------------------- |
| `name`  | The state's name, unique in the machine.                                                            |
| `does`  | The Lua activity it runs: `script`, a file in `game/assets/scripts` without `.lua`, and `activity`. |
| `from`  | A state, or a list of states for one transition from each.                                          |
| `to`    | The state to enter.                                                                                 |
| `when`  | [Facts](#facts) or [script facts](#script-facts) and the value each must have. Empty always holds.  |
| `after` | Optional seconds every condition must hold before the transition fires.                             |

From one state, the first transition in the list whose conditions have held long enough
fires.

### Facts

| Fact                           | True when                                                          | Notes                                                                               |
| ------------------------------ | ------------------------------------------------------------------ | ----------------------------------------------------------------------------------- |
| `targetKnown`                  | The NPC remembers a living target.                                 | Sight or a heard noise refreshes it; it lasts `targetMemoryDuration`.               |
| `targetVisible`                | The NPC sees the target.                                           | Within `noticeDistance` with clear line of sight.                                   |
| `targetInPrimaryRange`         | The visible target is where the primary attack can hit it.         | Bite: in its hitbox. Ranged: visible. Contact: overlapping. Pounce: within `range`. |
| `primaryReady`                 | The primary attack can start now.                                  | Contact is always ready.                                                            |
| `primaryActive`                | The primary attack is doing its damage.                            | Bite active, shooting, contact held, or pouncing in the air.                        |
| `targetInSecondaryRange`       | As `targetInPrimaryRange`, for the secondary attack.               |                                                                                     |
| `secondaryReady`               | As `primaryReady`, for the secondary attack.                       |                                                                                     |
| `secondaryActive`              | As `primaryActive`, for the secondary attack.                      |                                                                                     |
| `targetWithinStandoffDistance` | The remembered target is nearer than `standoffDistance`.           | Measured to its last known feet.                                                    |
| `heardLanding`                 | The NPC heard an opponent land on its ground run.                  | For one update.                                                                     |
| `targetOnSameSurface`          | The NPC can reach its grounded target without leaving its surface. | A climber may use walls and ceilings. Ignores distance and sight.                   |
| `targetWithinNoticeDistance`   | The remembered target is within `noticeDistance`.                  | Ignores ground and sight.                                                           |
| `movementBlocked`              | The NPC hit a wall, or its ledge guard stopped it.                 | From the last movement update.                                                      |
| `hasPatrol`                    | The NPC has a patrol.                                              |                                                                                     |
| `searches`                     | `searchDuration` is positive.                                      |                                                                                     |
| `searchTimeUp`                 | The time in this state has reached `searchDuration`.               |                                                                                     |

### Lua activities

A script returns `{ activities = { name = { enter, update, exit } }, facts = { ... } }`.
`update` and `description`, nonempty text saying what the activity does and when it
runs, are required, and `enter` and `exit` are optional. Each hook gets `self`, a table
kept for the visit, and a snapshot. `update` also gets the step in seconds, and returns a
command or `nil`. Every hook gets `memory` last: one table per NPC and script, kept across
states, which the script's facts share. [`common.lua`](../game/assets/scripts/common.lua) has `idle`, `patrol`, `chase`,
`attack`, `search`, `retreat` and `watch`.

| Snapshot              | Meaning                                                                                                                                                                           |
| --------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `feet`, `center`      | The NPC's feet and body centre.                                                                                                                                                   |
| `health`              | `current` and `maximum`, when the NPC has health.                                                                                                                                 |
| `targetFeet`          | The target's known feet, while it is known.                                                                                                                                       |
| `lastKnownTargetFeet` | Where the target was last seen or heard; `{0, 0}` before any.                                                                                                                     |
| `targetCenter`        | The living target's body centre now, seen or not.                                                                                                                                 |
| `targetKind`          | The living target's name in `actors.json`, such as `bat` or `player`.                                                                                                             |
| `patrol`              | `firstFeet`, `secondFeet` and `headingToSecond`, when the NPC has a patrol.                                                                                                       |
| `footing`             | `left` and `right`: whether a walker could stand a body width that way.                                                                                                           |
| `facts`               | The [facts](#facts).                                                                                                                                                              |
| `stateElapsed`        | Seconds in this state.                                                                                                                                                            |
| `routeStatus`         | `found`, `unreachable` (the route ends as close to the goal as the NPC can get) or `deferred` (the engine is still working it out); `nil` before any route or after `clearRoute`. |
| `routeComplete`       | Whether the last route asked for has been followed to its end.                                                                                                                    |
| `exitFeet`            | The level exit's feet, when the level has one.                                                                                                                                    |
| `pickups`             | Every pickup in the level: `feet`, `item` (its name in `items.json`), `quantity`, and `breakableBelow`, the center of the breakable tile the pickup rests on (`nil` otherwise).   |

| Command                                          | Meaning                                                                          |
| ------------------------------------------------ | -------------------------------------------------------------------------------- |
| `direction`, `aimDirection`                      | Movement and aim, as vectors.                                                    |
| `jumpPressed`, `jumpHeld`                        | Jump input.                                                                      |
| `primaryAttackPressed`, `secondaryAttackPressed` | Use that attack: bite, shoot or pounce on the press, or hold for contact damage. |
| `climbGrip`                                      | `"hold"`, `"release"` or `"keep"` (default).                                     |
| `avoidLedges`                                    | Stop a walker at a ledge.                                                        |
| `routeTo`                                        | Follow a route to a point; the engine plans and moves.                           |
| `aimAt`                                          | Aim at a point from the body centre, where shots leave.                          |
| `clearRoute`                                     | Drop the current route.                                                          |
| `turnPatrol`                                     | Head for the patrol's other end.                                                 |
| `useItem`                                        | Use an item from the bag by its name in `items.json`, as the player would.       |

Positions are `vec2` values, made with `vec2(x, y)`. They have `x` and `y`, `+`, `-`,
`*` and `/` by a number, `==`, `tostring`, and the methods `length()`, `distance(v)`,
`distanceSquared(v)` and `dot(v)`. A command's vectors also accept `{x, y}` tables. Scripts
have the base, math, string and table libraries.

### Script facts

A script's optional `facts` table names yes-or-no questions its machine's transitions may
ask beside the [facts](#facts), such as the bot's `targetClosingIn`. Each is
`function(memory, snapshot, step)` returning `true` or `false`. Every update, before the
machine chooses a state, each fact of every script the machine's states use runs once, in
name order, and its answer also appears in `snapshot.facts`. A fact may keep what it needs
in `memory`, such as the last distance to the target or how long a state has run this
approach. A fact cannot share a name with an engine fact, and two scripts of one machine
cannot declare the same fact.

A behaviour moves from one actor to another as a state: copy its activity and the facts
its transitions ask into the other actor's script, then add the state and its transitions
to that actor's machine.

## Presentation script

`presentation.lua` returns a table of hooks, one per kind of world event. After each
simulation step the game calls the hook for each event that happened and applies the
effects it returns; `nil` asks for nothing. The script decides the feel; the engine owns
what each effect is.

```lua
return {
    onKnockback = function(event)
        if event.actor ~= "player" then return nil end
        return { shake = { duration = 0.15, magnitude = 2 } }
    end,
}
```

| Hook          | When                      |
| ------------- | ------------------------- |
| `onLanding`   | A platformer actor lands. |
| `onShot`      | A ranged weapon fires.    |
| `onKnockback` | A hit throws its target.  |

An `event` has `kind` (`landing`, `shot` or `knockback`), `actor` (`player` or `npc`),
`feet`, a `vec2`, and `velocity`, the knockback's throw and otherwise zero.

| Effect  | Fields                                                                                                                                                                |
| ------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `shake` | `duration` and `magnitude` in seconds and pixels, both positive; a new shake replaces the current one. Only the drawn view moves; aim and the camera's follow do not. |

An unknown hook or effect, a wrong type or a non-positive number is rejected and reported
like an activity error, and the event has no effect.

## Animation sets

`animations.json` holds sets by name. A set holds any of these clips; only `idle` is
required. Each frame the game goes down the table and shows the first clip whose state
holds and that the set has, so a missing clip falls through to the next row: a jump with no
`jump` clip shows `fall`, a bite with no `bite` clip shows whatever the body is doing.

| Priority | Clip     | Shown while                                 |
| -------- | -------- | ------------------------------------------- |
| 1        | `death`  | Dying.                                      |
| 2        | `pounce` | A pounce is in the air.                     |
| 3        | `bite`   | A bite is winding up, active or recovering. |
| 4        | `shoot`  | A ranged weapon is in its shoot phase.      |
| 5        | `jump`   | Airborne and rising.                        |
| 6        | `fall`   | Airborne.                                   |
| 7        | `move`   | On the ground and moving.                   |
| 8        | `idle`   | Always; required.                           |

```json
"move": {
  "frames": [{ "position": [64, 0], "size": [32, 24] }, { "position": [96, 0], "size": [32, 24] }],
  "frameDuration": 0.16,
  "looping": true
}
```

| Field           | Meaning                                                                       |
| --------------- | ----------------------------------------------------------------------------- |
| `frames`        | At least one atlas region, played in order. Every frame in a set is one size. |
| `frameDuration` | Seconds per frame. Positive.                                                  |
| `looping`       | Whether the clip repeats; otherwise it holds its last frame.                  |

## Items

```json
"health_potion": {
  "name": "Health potion",
  "icon": { "position": [16, 216], "size": [16, 16] },
  "maximumStack": 5,
  "effect": "heal",
  "effectAmount": 2
}
```

| Field          | Meaning                                      |
| -------------- | -------------------------------------------- |
| `name`         | The label shown in the HUD.                  |
| `icon`         | A [sprite](#actors), drawn in the inventory. |
| `maximumStack` | Positive.                                    |
| `effect`       | `none` (default) or `heal`.                  |
| `effectAmount` | Zero for `none`, positive for `heal`.        |

Saves, if added, should store item names: item IDs are assigned at load and can change.

## Pickups

```json
"medicine_box": { "item": "health_potion", "quantity": 2, "bodySize": [16, 16] }
```

| Field      | Meaning                                                  |
| ---------- | -------------------------------------------------------- |
| `item`     | An item in `items.json`.                                 |
| `quantity` | Positive.                                                |
| `bodySize` | The collider the player touches to collect it.           |
| `sprite`   | Optional; without one, the pickup draws its item's icon. |

## Exits

```json
"bunker_door": { "bodySize": [16, 32], "sprite": { "position": [48, 216], "size": [16, 32] } }
```

`bodySize` and `sprite` are required. The requirement and consumption belong to a room
piece file's [`exit`](#room-pieces). Every exit leads to the next level of the
[run](#run).

## HUD icons

`hud.json` has the regions `fullHeart`, `emptyHeart` and `bag`, each with `position` and
`size`. The HUD draws them all at one size.

## Camera

`camera.json` holds `deadZone`, the part of the 320 by 180 view the player moves in
before the camera follows.

```json
{ "deadZone": [80, 45] }
```

## Sound effects

`catalogs/sounds.json` is required, even if it contains only `{"sounds": {}}`.
Each name maps to a jsfxr parameter object; omitted parameters use jsfxr defaults.
Unknown fields are errors. Copy a JSON patch from [jsfxr](https://sfxr.me/) into a
named entry:

```json
{
  "sounds": {
    "shot": {
      "wave_type": 0,
      "p_env_attack": 0,
      "p_env_sustain": 0.08,
      "p_env_decay": 0.18,
      "p_base_freq": 0.65,
      "p_freq_ramp": -0.45,
      "sound_vol": 0.2
    }
  }
}
```

The waveform is 0 for square, 1 for the jsfxr saw, 2 for sine, or 3 for noise.
The parameter values are jsfxr slider values, not seconds or hertz.

| Parameters                                     | Range and meaning                                                                                                      |
| ---------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------- |
| `p_env_attack`, `p_env_sustain`, `p_env_decay` | [0, 1]; each stage lasts floor(value squared times 100,000) ticks at 44,100 Hz. At least one stage must last a sample. |
| `p_env_punch`                                  | [0, 1]; sustain punch.                                                                                                 |
| `p_base_freq`, `p_freq_limit`                  | [0, 1]; initial pitch and optional lower cutoff.                                                                       |
| `p_freq_ramp`, `p_freq_dramp`                  | [-1, 1]; frequency slide and its acceleration.                                                                         |
| `p_vib_strength`, `p_vib_speed`                | [0, 1]; vibrato depth and speed.                                                                                       |
| `p_arp_mod`, `p_arp_speed`                     | Amount [-1, 1], speed [0, 1]; one pitch change.                                                                        |
| `p_duty`, `p_duty_ramp`                        | Duty [0, 1], sweep [-1, 1].                                                                                            |
| `p_repeat_speed`                               | [0, 1]; retrigger pitch controls, with zero disabling repeat.                                                          |
| `p_pha_offset`, `p_pha_ramp`                   | [-1, 1]; phaser offset and sweep.                                                                                      |
| `p_lpf_freq`, `p_lpf_resonance`, `p_hpf_freq`  | [0, 1]; low-pass cutoff, resonance, and high-pass cutoff.                                                              |
| `p_lpf_ramp`, `p_hpf_ramp`                     | [-1, 1]; filter sweeps.                                                                                                |
| `sound_vol`                                    | [0, 1]; gain is exp(value) minus one. Keep it low enough for overlapping sounds.                                       |
| `sample_rate`                                  | 44,100 (default), 22,050, or 11,025 Hz.                                                                                |
| `sample_size`                                  | 8 (default) or 16; accepted export metadata. Playback uses unquantised floats.                                         |
| `oldParams`                                    | Must be true when supplied; identifies the jsfxr slider parameter format.                                              |

All numeric parameters must be finite. Noise renders with a fixed seed, so loading
or reloading a patch produces the same samples. Patches are prepared in memory
when content loads; they require no WAV files.

Presentation hooks request sounds by name:

```lua
return {
    onShot = function()
        return { sound = { name = "shot" } }
    end,
}
```

A sound can accompany `shake` in the same returned table. `sound` accepts only
`name`, a non-empty string in the loaded sound catalog. The name is checked when
the hook returns, including dynamically constructed names. An invalid name or
malformed effect is reported with the script source and hook, and the returned
effects are ignored. Content validation exercises the shipped hooks; loading a
Lua function alone cannot validate all names it might construct later.

Editing a patch or the presentation script triggers the normal debug hot reload.
A failed patch load keeps the running content. Old sounds finish using their old
samples while subsequent events use the new ones. Music sequencing and an in-game
patch editor are outside this first audio milestone.

## Music authoring

Music is authored as JSON under `game/assets/music/`. The first example,
[`first-loop.json`](../game/assets/music/first-loop.json), is an eight-bar loop at
120 beats per minute, with bass, melody, and percussion. Music currently belongs
to the standalone audition tool; it does not start automatically in the game.

A song has six required fields:

| Field         | Meaning                                                             |
| ------------- | ------------------------------------------------------------------- |
| `tempo`       | Beats per minute, from 20 to 400. A beat is a quarter note.         |
| `instruments` | Named procedural instruments.                                       |
| `tracks`      | Track names mapped to instrument names.                             |
| `phrases`     | Named sequences of notes and rests.                                 |
| `patterns`    | Pattern lengths in beats, and track names mapped to phrase names.   |
| `arrangement` | Pattern names in playback order. The whole arrangement is the loop. |

For example:

```json
{
  "tempo": 120,
  "instruments": {
    "soft": { "wave": "triangle", "gain": 0.2, "release": 0.1 }
  },
  "tracks": { "bass": "soft" },
  "phrases": {
    "root": {
      "notes": [
        { "pitch": "C2", "beats": 1, "gate": 0.75 },
        { "pitch": "rest", "beats": 1 }
      ]
    }
  },
  "patterns": { "opening": { "beats": 2, "tracks": { "bass": "root" } } },
  "arrangement": ["opening", "opening"]
}
```

A note's `beats` determines when the next entry starts. Its optional `gate`
determines how many beats the note is held before release; it defaults to
`beats` and must not exceed it. A long duration holds one note without
retriggering. `volume` defaults to 1 and ranges from 0 to 1. `rest` advances
musical time without triggering a note; previous release tails can still be
heard. Pitch names use uppercase letters, optional `#` or `b`, and an octave,
such as `C4`, `F#3`, and `Bb2`. A4 is 440 Hz. Pitches above the output frequency
limit are rejected. Beats and gates must be at least 1/64 beat.

Each track plays one phrase per pattern. Missing tracks and any unused time
at the end of a shorter phrase produce no new notes. A phrase cannot exceed its
pattern. A new note retriggers its oscillator and envelope; previous notes may
still be releasing. Shared phrases can appear on different tracks and in
multiple patterns. Every reference and definition is validated, including unused
ones. Songs support up to 32 tracks and at most 120 seconds per arrangement.

Instruments are independent of jsfxr sound-effect patches:

| Field     | Default | Meaning                                                                    |
| --------- | ------- | -------------------------------------------------------------------------- |
| `wave`    | `sine`  | `sine`, `triangle`, `square`, or deterministic `noise`.                    |
| `attack`  | 0.01    | Seconds rising from silence to full level.                                 |
| `decay`   | 0.08    | Seconds falling from full level to sustain.                                |
| `sustain` | 0.65    | Held level, from 0 to 1.                                                   |
| `release` | 0.08    | Seconds falling from the current envelope level to silence after the gate. |
| `gain`    | 0.2     | Instrument output level, from 0 to 1.                                      |

Attack, decay, and release each range from 0 to 5 seconds. Zero skips that stage.
Tune gains to leave room for track sums; the final output clamps to [-1, 1].

### Audition and preview

Build the `advanced_platformer_music` target, then run from the project root:

```sh
cmake --build --preset mac-debug --target advanced_platformer_music
build/mac-debug/advanced_platformer_music game/assets/music/first-loop.json
build/mac-debug/advanced_platformer_music game/assets/music/first-loop.json --watch
build/mac-debug/advanced_platformer_music game/assets/music/first-loop.json --solo bass --loop
build/mac-debug/advanced_platformer_music game/assets/music/first-loop.json --pattern opening --loop
build/mac-debug/advanced_platformer_music game/assets/music/first-loop.json --output build/first-loop.wav
```

Without options, the tool plays one arrangement and exits. `--loop` repeats
until Ctrl-C. `--watch` also repeats and prepares changed JSON while the current
loop plays. A valid change restarts playback from the beginning; an invalid
change reports a diagnostic and keeps the previous loop. `--solo` selects a
track and `--pattern` selects a section; they also work with `--watch` and
`--output`. Reload is intended for auditioning and can have a discontinuity.

`--output` exports one complete loop as a mono 44,100 Hz float WAV without
opening an audio device. It cannot be combined with `--loop` or `--watch`.
WAV is a disposable listening preview; JSON remains the authored source.
The renderer wraps release tails past the arrangement end into its beginning,
so the preview represents a repeating loop, including those tails on its first
play. It rounds absolute musical positions to samples, avoiding accumulated
rounding drift. Repetition runs on the audio callback's sample clock.
