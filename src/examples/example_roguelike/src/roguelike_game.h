#pragma once

#include "roguelike_dungeon.h"

//! \file
//! \brief Application state: one Gaia world plus dense side stores.

//! Start-of-turn occupants packed by cell. Not an ECS component.
struct Occupancy {
	//! Bodies standing on each cell this turn.
	gaia::cnt::sarray_ext<gaia::ecs::Entity, MaxBodiesPerCell> cells[ScreenY][ScreenX];

	//! Clears every cell.
	void Clear();

	//! Records `e` on `x,y`. Extra bodies on a full cell are dropped. Out of bounds is ignored.
	//! \param x Column.
	//! \param y Row.
	//! \param e Occupant.
	void Add(int x, int y, gaia::ecs::Entity e);

	//! Occupants of `x,y`.
	//! \param x Column.
	//! \param y Row.
	//! \return Cell list. Empty when the cell is free or out of bounds.
	const gaia::cnt::sarray_ext<gaia::ecs::Entity, MaxBodiesPerCell>& At(int x, int y) const;
};

//! Owns the Gaia world, dungeon, occupancy, and session flags.
struct Game {
	//! Gaia world that stores every gameplay entity.
	gaia::ecs::World& world;
	//! Terrain side store.
	Dungeon dungeon;
	//! Rigid-body occupancy rebuilt at the start of each turn.
	Occupancy occupancy;
	//! Per-turn glyph buffer plus FOV memory.
	char glyphs[ScreenY][ScreenX]{};
	//! Cells in the current field of view.
	bool visible[ScreenY][ScreenX]{};
	//! Cells the player has seen on this floor.
	bool seen[ScreenY][ScreenX]{};
	//! Contacts produced by `ResolveMovement`.
	gaia::cnt::darray<CollisionData> colliding;
	//! Rolling combat log.
	gaia::cnt::sarray_ext<LogLine, LogLines> log;

	//! Unique player entity, or `EntityBad` before the first spawn.
	gaia::ecs::Entity player = gaia::ecs::EntityBad;
	//! Prefab used by `World::instantiate` for goblins.
	gaia::ecs::Entity prefabGoblin = gaia::ecs::EntityBad;
	//! Prefab used by `World::instantiate` for orcs.
	gaia::ecs::Entity prefabOrc = gaia::ecs::EntityBad;
	//! Prefab used by `World::instantiate` for potions.
	gaia::ecs::Entity prefabPotion = gaia::ecs::EntityBad;
	//! Prefab used by `World::instantiate` for poison flasks.
	gaia::ecs::Entity prefabPoison = gaia::ecs::EntityBad;
	//! Prefab used by `World::instantiate` for gold.
	gaia::ecs::Entity prefabGold = gaia::ecs::EntityBad;
	//! Prefab used by `World::instantiate` for arrows.
	gaia::ecs::Entity prefabArrow = gaia::ecs::EntityBad;
	//! Shared monster prefab. Goblin and orc inherit from it with `World::as`.
	gaia::ecs::Entity prefabEnemy = gaia::ecs::EntityBad;
	//! Prefab child of the goblin. Instantiated as `ChildOf` the goblin instance.
	gaia::ecs::Entity prefabPack = gaia::ecs::EntityBad;
	//! Module entity `game.monsters`.
	gaia::ecs::Entity modMonsters = gaia::ecs::EntityBad;
	//! Module entity `game.items`.
	gaia::ecs::Entity modItems = gaia::ecs::EntityBad;
	//! Module entity `game.phases`.
	gaia::ecs::Entity modPhases = gaia::ecs::EntityBad;
	//! Current floor root. Contents are `ChildOf` this entity.
	gaia::ecs::Entity floorRoot = gaia::ecs::EntityBad;

	//! Living enemies on the current floor. Recounted with `World::is`.
	uint32_t enemyCount = 0;
	//! Rigid bodies parented to `floorRoot` this turn.
	uint32_t floorBodies = 0;
	//! Current dungeon depth. 1 is the entrance.
	uint32_t floor = 1;
	//! Gold plus kill bounty.
	uint32_t score = 0;
	//! False after the player entity is deleted.
	bool playerAlive = false;
	//! True after the player leaves floor `MaxFloors`.
	bool escaped = false;
	//! True after the last enemy on this floor dies.
	bool floorCleared = false;
	//! Key consumed by `InputSystem` this turn. Zero performs the initial draw without acting.
	char pendingKey = 0;
	//! True when the player spent the turn moving, waiting, or shooting.
	bool playerActed = false;
	//! Arrow request assembled by the application before the system update.
	bool pendingShot = false;
	//! Spawn cell for the pending arrow.
	Position pendingShotPos{};
	//! Flight direction for the pending arrow.
	Velocity pendingShotVel{};
	//! When false, glyphs are printed without ANSI color.
	bool color = true;
	//! When true, the renderer skips terminal clears.
	bool quiet = false;
	//! When true, the main loop exits after the current frame.
	bool terminate = false;

	//! Binds this helper to an existing Gaia world.
	//! \param w World that will own every gameplay entity.
	explicit Game(gaia::ecs::World& w): world(w) {}

	//! Appends a printf-style line to the combat log.
	//! \param fmt printf format string.
	void log_msg(const char* fmt, ...);

	//! Creates monster and item prefabs. Call once before systems are registered.
	void CreatePrefabs();

	//! Carves floor 1, spawns the player, and writes the opening log line.
	void StartRun();

	//! Stores an arrow request before the world update begins.
	//! \param p Cell the shot starts in. Usually the player cell.
	//! \param v Flight direction taken from `Orientation`.
	void QueueArrow(Position p, Velocity v);

	//! Instantiates the queued arrow before `World::update`, if any.
	//! \return True when an arrow was spawned.
	bool SpawnQueuedArrow();

	//! Stops movers that hit walls, occupants, or a tile already claimed this turn.
	void ResolveMovement();

	//! Sets enemy velocity toward the player when the player acted.
	//! \param p Enemy cell.
	//! \param v Velocity written by the AI system.
	void ChasePlayer(const Position& p, Velocity& v);

	//! Walks onto stairs to the next floor, or marks escape after `MaxFloors`.
	void TryDescend();

	//! Counts living monsters via `World::is` and floor bodies via `ChildOf`.
	void RecountCensus();

	//! Instantiates `prefab` and places it on `p`.
	//! \param prefab Prefab created with `World::prefab`.
	//! \param p Cell for the new instance.
	//! \return Instance entity. It does not carry `ecs::Prefab`.
	gaia::ecs::Entity SpawnFrom(gaia::ecs::Entity prefab, Position p);

	//! Deletes every positioned entity except the player and rebuilds the floor.
	//! \param nextFloor Depth to generate. Values above `MaxFloors` escape.
	void GenerateFloor(uint32_t nextFloor);

private:
	//! Creates the player if needed and moves it to the current spawn cell.
	void PlacePlayer();
	//! Instantiates the monsters and items for the current floor.
	void SpawnFloorPopulation();
	//! Deletes the floor root and its `ChildOf` contents.
	void WipeFloorEntities();
	//! Tries one cardinal fallback step for enemy movement.
	//! \param x Current column.
	//! \param y Current row.
	//! \param v Velocity written when the destination is walkable.
	//! \param vx Horizontal step.
	//! \param vy Vertical step.
	//! \return True when the step is walkable.
	bool TryStep(int x, int y, Velocity& v, int vx, int vy) const;
	//! Tests projectile overlap exceptions used during movement resolution.
	//! \param mover Moving entity.
	//! \param occupant Entity already occupying the destination.
	//! \return True when this pair may share or cross the cell.
	bool ShotPasses(gaia::ecs::Entity mover, gaia::ecs::Entity occupant) const;
};

//! Recovers the `Game` stored with `SystemBuilder::ctx`.
//! \param it Iterator of the system currently running.
//! \return The game passed to `.ctx(...)`.
Game& game_of(gaia::ecs::Iter& it);

//! Registers every gameplay system on `world`.
//! Prefabs must already exist so systems can classify instances with `World::is`.
//! \param world Gaia world that will own the systems.
//! \param game Application state passed to every system through `.ctx(&game)`.
void register_systems(gaia::ecs::World& world, Game& game);
