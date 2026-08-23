#pragma once

#include "gaia_ecs.h"

//! \file
//! \brief ECS components and map constants. No dungeon or render state lives here.

//! Grid position in dungeon cells.
struct Position {
	//! Horizontal cell. Origin is the top-left of the map.
	int x;
	//! Vertical cell. Origin is the top-left of the map.
	int y;
};

//! Facing used when the player shoots.
struct Orientation {
	//! Horizontal facing. -1, 0, or 1.
	int x;
	//! Vertical facing. -1, 0, or 1.
	int y;
};

//! Cells to move this turn. Zero means the body stays put.
struct Velocity {
	//! Horizontal delta in cells.
	int x;
	//! Vertical delta in cells.
	int y;
};

//! Glyph drawn for an entity that is currently visible.
struct Sprite {
	//! Terminal character for this body.
	char value;
};

//! Hit points. Death systems react when `value` drops to zero.
struct Health {
	//! Remaining hit points.
	int value;
	//! Inclusive upper clamp applied after healing.
	int valueMax;
};

//! Combat numbers used when one body bumps another.
struct BattleStats {
	//! Damage before the target armor is subtracted.
	int power;
	//! Flat reduction applied to incoming power.
	int armor;
};

//! Distinguishes pickups and projectiles that share the `Item` component.
enum class ItemType : uint8_t {
	//! Damages the walker.
	Poison,
	//! Restores hit points.
	Potion,
	//! Adds to the score and is consumed.
	Gold
};

//! Marker for a floor pickup.
struct Item {
	//! Kind of item.
	ItemType type;
};

//! Tag for the unique player entity.
struct Player {};

//! Tag for hostile creatures instantiated from monster prefabs.
struct Enemy {};

//! Tag for anything that occupies a cell and participates in movement.
struct RigidBody {};

//! Singleton tag so once-per-turn systems have a matching entity.
struct Turn {};

constexpr uint32_t ScreenX = 50;
constexpr uint32_t ScreenY = 18;
constexpr uint32_t FovRadius = 8;
constexpr uint32_t MaxFloors = 5;
constexpr uint32_t LogLines = 4;
constexpr uint32_t LogWidth = 72;
constexpr uint32_t MaxBodiesPerCell = 8;

constexpr char KEY_LEFT = 'a';
constexpr char KEY_RIGHT = 'd';
constexpr char KEY_UP = 'w';
constexpr char KEY_DOWN = 's';
constexpr char KEY_WAIT = ' ';
constexpr char KEY_SHOOT = 'q';
constexpr char KEY_QUIT = 'p';

constexpr char TILE_WALL = '#';
constexpr char TILE_FREE = '.';
constexpr char TILE_PLAYER = '@';
constexpr char TILE_ENEMY_GOBLIN = 'g';
constexpr char TILE_ENEMY_ORC = 'O';
constexpr char TILE_ARROW = '*';
constexpr char TILE_POTION = '!';
constexpr char TILE_POISON = 'x';
constexpr char TILE_GOLD = '$';
constexpr char TILE_STAIRS = '>';

constexpr int PlayerStartHealth = 80;

//! Axis-aligned room used by the deterministic dungeon layout.
struct Room {
	//! Left wall column.
	int x;
	//! Top wall row.
	int y;
	//! Width including walls.
	int w;
	//! Height including walls.
	int h;
};

constexpr Room kRooms[] = {
		{2, 2, 12, 7}, {18, 2, 13, 6}, {35, 2, 13, 7}, {4, 11, 16, 6}, {28, 11, 19, 6},
};

constexpr uint32_t kRoomCount = sizeof(kRooms) / sizeof(kRooms[0]);

//! One line of the on-screen combat log.
struct LogLine {
	//! NUL-terminated message text.
	char text[LogWidth];
};

//! Contact produced by movement resolution.
struct CollisionData {
	//! Mover that tried to enter the cell.
	gaia::ecs::Entity e1;
	//! Occupant or `EntityBad` for a wall.
	gaia::ecs::Entity e2;
	//! Cell where the move stopped.
	Position p;
	//! Velocity at the moment of contact.
	Velocity v;
};

//! Packed cell id `y * ScreenX + x`.
//! \param x Column.
//! \param y Row.
//! \return Cell index in the dense map stores.
constexpr uint32_t CellIndex(int x, int y) {
	return (uint32_t)y * ScreenX + (uint32_t)x;
}
