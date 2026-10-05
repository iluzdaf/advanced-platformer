# Content and Level Format

The authoring reference for the files under `assets`. Levels place named definitions;
definitions configure engine components; machines and Lua activities decide what NPCs
do. Movement, combat and pathfinding stay in C++.

- Unknown fields are rejected, so misspellings are reported.
- Errors name the file and either a field path (`items.herb.maximumStack`, `map[2][7]`)
  or a line and column (`items.json: line 5, column 7: unknown field 'maximimStack'`).
- Every shared definition is validated, even when no level uses it.
- Shared catalogs and Lua scripts load at startup, and a level file when the level
  starts. Debug builds also reload them while the game runs; see [Hot reload](#hot-reload).
- Units are pixels, seconds and pixels per second. Sprite regions are atlas pixels.

## Hot reload

Builds other than Release read `assets/` from the source tree, not the copy beside the
executable, and check it four times a second. Once a change has settled, the game loads
every catalog, script, the atlas and the current level's file again. If anything fails,
the console shows the error and the game keeps running what it had. Otherwise the game
applies the new content without restarting:

- Actors and pickups are matched to their placements by `id`. A kept one stays where it
  is, keeps what it was doing, and takes its new definition, spawn and patrol. Health is
  kept but capped at the new maximum.
- A new `id` spawns. An `id` gone from the file removes its actor or pickup. An actor
  killed or a pickup collected stays gone while its `id` remains.
- The player keeps their position, health and items, matched to items by name.
- Tiles broken in play stay broken. Machines resume in the state with the same name, or
  the first state, and every activity starts again under the new scripts.

The console reports what was kept, spawned and removed. F5 restarts the current level
from its file, keeping the player's health and items.

## Files

| File                                                             | Holds                                                 | Loader                                                                                                                       |
| ---------------------------------------------------------------- | ----------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------- |
| [`levels/levels.json`](../assets/levels/levels.json)             | Start level, camera dead zone, level numbers to files | [`level_catalog.cpp`](../app/content/level_catalog.cpp)                                                                      |
| `levels/level_N.json`                                            | A level's map, legends and placements                 | [`level_data.cpp`](../app/content/level_data.cpp)                                                                            |
| [`catalogs/tiles.json`](../assets/catalogs/tiles.json)           | Tile size and tiles                                   | [`tile_catalog.cpp`](../app/content/tile_catalog.cpp)                                                                        |
| [`catalogs/actors.json`](../assets/catalogs/actors.json)         | The player and every actor definition                 | [`actor_catalog.cpp`](../app/content/actor_catalog.cpp), [`actor_definition.cpp`](../app/content/actor_definition.cpp)       |
| [`catalogs/animations.json`](../assets/catalogs/animations.json) | Animation sets                                        | [`animation_catalog.cpp`](../app/content/animation_catalog.cpp)                                                              |
| [`catalogs/machines.json`](../assets/catalogs/machines.json)     | NPC state machines                                    | [`machine_catalog.cpp`](../app/content/machine_catalog.cpp)                                                                  |
| [`scripts/*.lua`](../assets/scripts)                             | Lua activities                                        | [`npc_script_catalog.cpp`](../app/content/npc_script_catalog.cpp), [`lua_npc_scripts.cpp`](../scripting/lua_npc_scripts.cpp) |
| [`catalogs/items.json`](../assets/catalogs/items.json)           | Inventory items                                       | [`item_catalog.cpp`](../app/content/item_catalog.cpp)                                                                        |
| [`catalogs/pickups.json`](../assets/catalogs/pickups.json)       | World pickups                                         | [`pickup_catalog.cpp`](../app/content/pickup_catalog.cpp)                                                                    |
| [`catalogs/exits.json`](../assets/catalogs/exits.json)           | Exit bodies and sprites                               | [`exit_catalog.cpp`](../app/content/exit_catalog.cpp)                                                                        |
| [`catalogs/hud.json`](../assets/catalogs/hud.json)               | HUD icon regions                                      | [`hud_catalog.cpp`](../app/content/hud_catalog.cpp)                                                                          |

Every catalog is required, even when empty. Every sprite region, frame and icon must lie
inside the atlas.

## Level catalog

```json
{
  "startLevel": 1,
  "cameraDeadZone": [80, 45],
  "levels": [{ "number": 1, "file": "level_1.json" }]
}
```

| Field            | Meaning                                                                                |
| ---------------- | -------------------------------------------------------------------------------------- |
| `startLevel`     | The `number` of the first level.                                                       |
| `cameraDeadZone` | The part of the 320 by 180 view the player moves in before the camera follows.         |
| `levels`         | `number`, a positive unique ID that exits refer to, and `file`, relative to this file. |

## Level files

```json
{
  "tileLegend": { ".": "empty", "#": "stone" },
  "map": ["........", "########"],
  "playerSpawn": { "cell": [1, 0] },
  "actors": [{ "id": "zombie_1", "definition": "zombie", "spawn": { "cell": [2, 0] } }],
  "pickups": [{ "id": "medicine_1", "definition": "medicine_box", "spawn": { "cell": [4, 0] } }],
  "exit": { "definition": "bunker_door", "spawn": { "cell": [6, 0] }, "nextLevel": 2 }
}
```

| Field         | Required | Meaning                                                         |
| ------------- | -------- | --------------------------------------------------------------- |
| `tileLegend`  | Yes      | One-character map symbols to tile names in `tiles.json`.        |
| `map`         | Yes      | Rows of equal, nonzero length. Every symbol is in `tileLegend`. |
| `playerSpawn` | Yes      | Where the player starts.                                        |
| `actors`      | No       | Actor placements.                                               |
| `pickups`     | No       | Pickup placements.                                              |
| `exit`        | Yes      | The exit placement. Without `nextLevel`, it completes the game. |

### Positions

Coordinates start at the top-left, with Y pointing down. A cell is `[column, row]`.
A position is an object with one key: `cell`, which puts an object's feet at the bottom
centre of that cell, or `feet`, that point in world pixels.

### Placements

| Placement | Fields                                                                                                                                       |
| --------- | -------------------------------------------------------------------------------------------------------------------------------------------- |
| Actor     | `id`; `definition` from `actors.json`; `spawn`; optional `patrol` with `first` and `second`, absolute positions.                             |
| Pickup    | `id`; `definition` from `pickups.json`; `spawn`.                                                                                             |
| Exit      | `definition` from `exits.json`; `spawn`; optional `requirement` (`item`, positive `quantity`), `consumeItem` (default `false`), `nextLevel`. |

An `id` names one actor or pickup placement. It is nonempty and unique among the
level's actors and pickups.

Bodies come from definitions, never placements. A pickup falls until it rests on a
tile, and falls again if that tile breaks.

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

| Field   | Meaning                                                                                        |
| ------- | ---------------------------------------------------------------------------------------------- |
| `name`  | The state's name, unique in the machine.                                                       |
| `does`  | The Lua activity it runs: `script`, a file in `assets/scripts` without `.lua`, and `activity`. |
| `from`  | A state, or a list of states for one transition from each.                                     |
| `to`    | The state to enter.                                                                            |
| `when`  | [Facts](#facts) and the value each must have. Empty always holds.                              |
| `after` | Optional seconds every condition must hold before the transition fires.                        |

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
| `heardLanding`                 | The NPC heard the player land on its ground run.                   | For one update.                                                                     |
| `targetOnSameSurface`          | The NPC can reach its grounded target without leaving its surface. | A climber may use walls and ceilings. Ignores distance and sight.                   |
| `targetWithinNoticeDistance`   | The remembered target is within `noticeDistance`.                  | Ignores ground and sight.                                                           |
| `movementBlocked`              | The NPC hit a wall, or its ledge guard stopped it.                 | From the last movement update.                                                      |
| `hasPatrol`                    | The NPC has a patrol.                                              |                                                                                     |
| `searches`                     | `searchDuration` is positive.                                      |                                                                                     |
| `searchTimeUp`                 | The time in this state has reached `searchDuration`.               |                                                                                     |

### Lua activities

A script returns `{ activities = { name = { enter, update, exit } } }`. `update` is
required, and `enter` and `exit` are optional. Each hook gets `self`, a table kept for
the visit, and a snapshot. `update` also gets the step in seconds, and returns a command
or `nil`. [`common.lua`](../assets/scripts/common.lua) has `idle`, `patrol`, `chase`,
`attack`, `search`, `retreat` and `watch`.

| Snapshot              | Meaning                                                                     |
| --------------------- | --------------------------------------------------------------------------- |
| `feet`, `center`      | The NPC's feet and body centre.                                             |
| `targetFeet`          | The target's known feet, while it is known.                                 |
| `lastKnownTargetFeet` | Where the target was last seen or heard; `{0, 0}` before any.               |
| `targetCenter`        | The living target's body centre now, seen or not.                           |
| `patrol`              | `firstFeet`, `secondFeet` and `headingToSecond`, when the NPC has a patrol. |
| `footing`             | `left` and `right`: whether a walker could stand a body width that way.     |
| `facts`               | The [facts](#facts).                                                        |
| `stateElapsed`        | Seconds in this state.                                                      |
| `hasRoute`            | Whether the engine holds a route.                                           |
| `routeComplete`       | Whether the last route asked for has been followed to its end.              |

| Command                                          | Meaning                                                                          |
| ------------------------------------------------ | -------------------------------------------------------------------------------- |
| `direction`, `aimDirection`                      | Movement and aim, as vectors.                                                    |
| `jumpPressed`, `jumpHeld`                        | Jump input.                                                                      |
| `primaryAttackPressed`, `secondaryAttackPressed` | Use that attack: bite, shoot or pounce on the press, or hold for contact damage. |
| `climbGrip`                                      | `"hold"`, `"release"` or `"keep"` (default).                                     |
| `avoidLedges`                                    | Stop a walker at a ledge.                                                        |
| `routeTo`                                        | Follow a route to a point; the engine plans and moves.                           |
| `aimAt`                                          | Aim at a point.                                                                  |
| `clearRoute`                                     | Drop the current route.                                                          |
| `turnPatrol`                                     | Head for the patrol's other end.                                                 |

Positions are `vec2` values, made with `vec2(x, y)`. They have `x` and `y`, `+`, `-`,
`*` and `/` by a number, `==`, `tostring`, and the methods `length()`, `distance(v)`,
`distanceSquared(v)` and `dot(v)`. A command's vectors also accept `{x, y}` tables. Scripts
have the base, math, string and table libraries.

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

`bodySize` and `sprite` are required. The requirement, consumption and next level belong
to each [placement](#placements), so doors that look alike can lead to different levels.

## HUD icons

`hud.json` has the regions `fullHeart`, `emptyHeart` and `bag`, each with `position` and
`size`. The HUD draws them all at one size.
