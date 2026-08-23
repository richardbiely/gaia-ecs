#pragma once

#include "roguelike_path.h"
#include "roguelike_types.h"

//! \file
//! \brief Dense terrain side store. Walls and stairs are not ECS components.

//! Fixed dungeon map plus the walkability graph rebuilt when the floor changes.
struct Dungeon {
	//! Durable tiles. Collision and FOV read this grid.
	char tiles[ScreenY][ScreenX]{};
	//! Four-neighbor graph used by A*. One node per cell.
	gaia::cnt::darray<AStar::Node> graph;
	//! Player spawn cell for the current floor.
	Position spawn{};
	//! Stairs cell. Walk here to descend.
	Position stairs{};
	//! West-most walkable column of the start room.
	int westFloorX = 1;

	//! Fills the map with rooms, corridors, and stairs.
	void Generate();

	//! True when the cell is inside the map.
	//! \param x Column.
	//! \param y Row.
	//! \return True when both axes are on the grid.
	bool InBounds(int x, int y) const;

	//! True when the cell blocks walking.
	//! \param x Column.
	//! \param y Row.
	//! \return True for out-of-bounds cells and walls.
	bool IsWall(int x, int y) const;

	//! True when a body may enter the cell.
	//! \param x Column.
	//! \param y Row.
	//! \return True for floors and stairs.
	bool IsWalkable(int x, int y) const;

	//! Tile stored at `x,y`. The cell must be in bounds.
	//! \param x Column.
	//! \param y Row.
	//! \return Terrain glyph.
	char At(int x, int y) const;

	//! Center column of a room.
	//! \param r Room.
	//! \return Center X.
	static int RoomCenterX(const Room& r);

	//! Center row of a room.
	//! \param r Room.
	//! \return Center Y.
	static int RoomCenterY(const Room& r);

private:
	//! Carves every cell inside a room.
	//! \param r Room bounds.
	void CarveRoom(const Room& r);
	//! Carves a horizontal corridor, including both endpoints.
	//! \param x0 First column.
	//! \param x1 Last column.
	//! \param y Corridor row.
	void CarveH(int x0, int x1, int y);
	//! Carves a vertical corridor, including both endpoints.
	//! \param y0 First row.
	//! \param y1 Last row.
	//! \param x Corridor column.
	void CarveV(int y0, int y1, int x);
	//! Connects two room centers with an L-shaped corridor.
	//! \param a Source room.
	//! \param b Destination room.
	void ConnectRooms(const Room& a, const Room& b);
	//! Rebuilds the four-neighbor A* graph from `tiles`.
	void RebuildGraph();
};
