# Roguelike example

A playable five-floor terminal roguelike built as a small Gaia-ECS application rather than a single-file API sample.

The example demonstrates where an ECS adds value: entity identity, component storage, prefab families, hierarchy lifetime, queries, ordered systems, change detection, and deferred deletion. It also shows where ordinary application data is the better fit. Terrain, pathfinding, occupancy, field of view, and terminal rendering remain compact purpose-built structures.

## Build and run

```bash
cmake -S . -B build/RelWithDebInfo \
  -DGAIA_BUILD_EXAMPLES=ON \
  -DGAIA_BUILD_UNITTEST=ON
cmake --build build/RelWithDebInfo --target gaia_example_roguelike
./build/RelWithDebInfo/src/examples/example_roguelike/gaia_example_roguelike
```

Controls:

| Key | Action |
| --- | --- |
| `W` `A` `S` `D` | Move one cell and change facing |
| space | Wait one turn |
| `Q` | Shoot in the facing direction |
| bump a monster | Melee attack |
| walk onto `>` | Descend |
| `P` | Quit |

Glyphs: `@` player, `g` goblin, `O` orc, `!` potion, `x` poison, `$` gold, `*` arrow, `>` stairs.

Unseen cells are black. Explored cells remain dim, while visible cells show their current terrain and entities. Descend through five floors to escape. Monsters on later floors deal more damage.

Optional command-line modes:

```bash
# Disable ANSI colors
./build/RelWithDebInfo/src/examples/example_roguelike/gaia_example_roguelike --no-color

# Feed deterministic input without opening an interactive terminal
./build/RelWithDebInfo/src/examples/example_roguelike/gaia_example_roguelike --keys 'ddq  aas'

# Run the built-in gameplay and architecture checks
./build/RelWithDebInfo/src/examples/example_roguelike/gaia_example_roguelike --smoke
ctest --test-dir build/RelWithDebInfo -R gaia_example_roguelike_smoke
```

## Architecture at a glance

The program has one `gaia::ecs::World` and one application object:

```text
terminal input
     |
     v
application boundary ---- instantiate projectiles before update
     |
     v
World::update()
  Input phase      player intent -> enemy AI -> occupancy
  Simulation phase facing -> collision -> movement -> combat -> death
  View phase       terrain -> sprites -> FOV -> terminal HUD
     |
     v
terminal output
```

`Game` owns the world reference and the non-ECS data needed by the application. Systems that need this state receive it through `SystemBuilder::ctx(&game)`. `Game` is not a singleton component and no global game pointer is required.

This separation keeps the ECS focused on gameplay objects and behavior while dense grid algorithms retain direct indexing and predictable memory access.

## What Gaia-ECS owns

Gameplay entities are composed from small components:

- `Position`, `Velocity`, and `Orientation`
- `Sprite`
- `Health` and `BattleStats`
- `Item`
- tags such as `Player`, `RigidBody`, and `Turn`

Gaia-ECS also owns the relationships that give those entities meaning and lifetime:

- named modules for monsters, items, and phases
- monster and item prefabs
- `Is` inheritance between enemy prefabs
- instantiated prefab children
- `ChildOf` ownership for floor contents
- `DependsOn` ordering between phases and systems

The result is not merely data placed in a world. Gameplay consumes the model directly: AI and combat classify prefab families with `World::is`, goblins find their instantiated pack bonus with `find_prefab_instance`, and deleting a floor root removes everything owned by that floor.

## What stays application-owned

The following data is dense, bounded, and naturally indexed by cell, so it is intentionally not represented as entities or components:

- `Dungeon::tiles` for walls, floors, and stairs
- `Dungeon::graph` for A* pathfinding
- `Occupancy` for start-of-turn rigid bodies
- `visible`, `seen`, and `glyphs` for the terminal view

A map cell is an array index, not an entity lookup. Occupancy uses a grid of `cnt::sarray_ext`, pathfinding uses Gaia containers, and the code does not need a hash map of positions.

This is an important architectural choice: Gaia-ECS coordinates dynamic gameplay state without forcing static terrain or rendering scratch data into the world.

## Modules, prefabs, and relationships

### Named application modules

`World::module("game.monsters")`, `World::module("game.items")`, and `World::module("game.phases")` create discoverable scopes. `World::scope(...)` registers entities inside a module, and `World::child(...)` expresses ownership by that module.

The smoke test resolves names such as `game.monsters.Goblin` and `game.items.Potion`, so the module structure is exercised rather than merely constructed.

### Prefab families

`EnemyBase` contains data shared by hostile creatures. Goblin and orc prefabs are fully configured and then related to that base with `World::as`.

Spawning uses `World::instantiate`. Each monster keeps its leaf-prefab identity while remaining part of the broader enemy family:

```cpp
world.is(goblin, prefabGoblin); // concrete kind
world.is(goblin, prefabEnemy);  // shared enemy family
```

Systems put prefab semantics directly into query plans when identity defines membership: enemy AI uses `.is(prefabEnemy)`, species HUD systems use `.is(prefabGoblin)` and `.is(prefabOrc)`, and the census uses the runtime `.in(prefabEnemy)` descendant query. `World::is` remains useful for pairwise combat decisions where the entities come from collision records rather than a query. The same relationship therefore drives enemy AI, combat rules, the census, and species-specific HUD lines.

### Instantiated prefab children

The goblin prefab owns a `PackBonus` child. Instantiating a goblin creates the corresponding child under that goblin instance. Combat and the HUD locate it with `find_prefab_instance` and consume its `BattleStats` bonus.

This demonstrates prefab hierarchy as gameplay data rather than setup-only metadata.

### Floor lifetime

Every monster, pickup, and arrow is `ChildOf` the current floor root. The player is deliberately not a child of the floor.

Descending deletes the old floor root. Gaia-ECS recursively removes its owned contents, while the player survives and is repositioned on the newly generated floor. No separate list of floor allocations is needed for cleanup.

## Systems and turn ordering

Three phase entities define the frame:

1. **Input** — interpret player intent, run enemy AI, rebuild occupancy.
2. **Simulation** — update facing, resolve movement, apply collisions, descend stairs, process combat and pickups, clamp health, and delete dead entities.
3. **View** — copy terrain, stamp sprites, compute FOV, draw the map, and print the HUD and game state.

Gaia uses the same relationship model for ordering:

```cpp
world.add(first, {gaia::ecs::DependsOn, second});
```

In this schedule, `first` runs before `second`. Dependencies order both phase entities and individual systems, making the pipeline explicit without a manually maintained call sequence.

Systems use two callback styles:

- Typed `on_each` callbacks for local component transformations, such as movement and facing.
- `Iter` callbacks with `Iter::ctx()` when a system also needs dungeon, occupancy, prefab, or rendering state from `Game`.

Health clamp and death systems use `.changed<Health>()`, so they react to writes instead of scanning unchanged health rows as game-state logic.

## Safe structural changes

Gaia locks the world while systems iterate. The example respects that boundary in two ways:

- Death and pickup systems use `Iter::cmd_buffer_st().del(...)`. Deletion is applied safely after the active system iteration.
- Shooting instantiates the arrow at the application boundary before `World::update()`. The projectile then participates in the same occupancy, collision, movement, damage, and rendering pipeline as every other body.

Each accepted command performs exactly one world update. Enemies act once, existing projectiles advance once, and invalid keys do not advance the simulation.

## Deterministic movement and collision

Movement is resolved from a start-of-turn occupancy grid:

1. Collect moving `RigidBody` entities.
2. Process movers in entity-id order.
3. Stop at walls or occupied cells and record a contact.
4. Claim each free destination cell for the first accepted mover.
5. Stop later movers that request an already claimed cell.
6. Apply the truncated velocity in `MoveSystem`.

This prevents bodies from stacking or passing through one another and makes contested movement deterministic.

Contacts are consumed by combat and pickup systems. A projectile can damage a target only once, is consumed by a blocking contact, and cannot damage the player. Co-directional arrows can follow one another without colliding with stale start-of-turn occupancy.

## Source layout

All Gaia includes pass through one application wrapper. CMake rejects a direct `#include <gaia.h>` anywhere else in the example.

| File | Responsibility |
| --- | --- |
| `src/gaia_ecs.h` | Application-wide Gaia include boundary |
| `src/roguelike_types.h` | Components, tags, input keys, and map constants |
| `src/roguelike_dungeon.h` / `.cpp` | Dense terrain and walkability graph |
| `src/roguelike_path.h` / `.cpp` | Grid A* used by enemy chase |
| `src/roguelike_game.h` / `.cpp` | Application state, prefabs, floor population, occupancy, and movement resolution |
| `src/roguelike_render.h` / `.cpp` | Field of view and terminal rendering |
| `src/roguelike_systems.cpp` | Systems, phases, queries, and ordering relationships |
| `src/main.cpp` | Terminal input, application update boundary, and smoke checks |

The split keeps platform code, ECS systems, dense algorithms, and rendering independently readable. It also centralizes the Gaia include behind one application header.

## Verification coverage

`--smoke` runs deterministic scenarios that cover both gameplay and architecture:

- wall blocking and melee
- module path lookup
- prefab inheritance through `World::as` and `World::is`
- instantiated prefab children
- floor hierarchy cleanup and player survival
- pickup ownership
- point-blank and ranged projectile damage
- projectile collision, repeated shooting, and invalid-input turn behavior
- floor replacement, hierarchy cleanup, and player survival

The CMake test registers this mode as `gaia_example_roguelike_smoke`, making the example executable as documentation and continuously testable as application code.
