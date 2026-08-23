//! \file
//! \brief Room carving and walkability graph for the dungeon side store.

#include "roguelike_dungeon.h"

int Dungeon::RoomCenterX(const Room& r) {
	return r.x + r.w / 2;
}

int Dungeon::RoomCenterY(const Room& r) {
	return r.y + r.h / 2;
}

bool Dungeon::InBounds(int x, int y) const {
	return x >= 0 && y >= 0 && x < (int)ScreenX && y < (int)ScreenY;
}

bool Dungeon::IsWall(int x, int y) const {
	if (!InBounds(x, y))
		return true;
	return tiles[y][x] == TILE_WALL;
}

bool Dungeon::IsWalkable(int x, int y) const {
	return !IsWall(x, y);
}

char Dungeon::At(int x, int y) const {
	return tiles[y][x];
}

void Dungeon::CarveRoom(const Room& r) {
	GAIA_FOR2_(r.y + 1, r.y + r.h - 1, y) {
		GAIA_FOR2_(r.x + 1, r.x + r.w - 1, x) {
			if (InBounds(x, y))
				tiles[y][x] = TILE_FREE;
		}
	}
}

void Dungeon::CarveH(int x0, int x1, int y) {
	if (x0 > x1)
		gaia::core::swap(x0, x1);
	GAIA_FOR2_(x0, x1 + 1, x) {
		if (InBounds(x, y))
			tiles[y][x] = TILE_FREE;
	}
}

void Dungeon::CarveV(int y0, int y1, int x) {
	if (y0 > y1)
		gaia::core::swap(y0, y1);
	GAIA_FOR2_(y0, y1 + 1, y) {
		if (InBounds(x, y))
			tiles[y][x] = TILE_FREE;
	}
}

void Dungeon::ConnectRooms(const Room& a, const Room& b) {
	const int ax = RoomCenterX(a);
	const int ay = RoomCenterY(a);
	const int bx = RoomCenterX(b);
	const int by = RoomCenterY(b);
	CarveH(ax, bx, ay);
	CarveV(ay, by, bx);
}

void Dungeon::RebuildGraph() {
	graph.clear();
	graph.reserve(ScreenX * ScreenY);
	uint32_t index = 0;
	GAIA_FOR_(ScreenY, y) {
		GAIA_FOR_(ScreenX, x) {
			AStar::Node node{index++};
			if (IsWalkable(x, y)) {
				if (y > 0)
					node.InitIndex(0, IsWalkable(x, y - 1) ? 1 : 0);
				if (x < (int)ScreenX - 1)
					node.InitIndex(1, IsWalkable(x + 1, y) ? 1 : 0);
				if (y < (int)ScreenY - 1)
					node.InitIndex(2, IsWalkable(x, y + 1) ? 1 : 0);
				if (x > 0)
					node.InitIndex(3, IsWalkable(x - 1, y) ? 1 : 0);
			}
			graph.push_back(GAIA_MOV(node));
		}
	}
}

void Dungeon::Generate() {
	GAIA_FOR_(ScreenY, y) {
		GAIA_FOR_(ScreenX, x) {
			tiles[y][x] = TILE_WALL;
		}
	}

	GAIA_FOR(kRoomCount) {
		CarveRoom(kRooms[i]);
	}
	ConnectRooms(kRooms[0], kRooms[1]);
	ConnectRooms(kRooms[1], kRooms[2]);
	ConnectRooms(kRooms[0], kRooms[3]);
	ConnectRooms(kRooms[3], kRooms[4]);
	ConnectRooms(kRooms[2], kRooms[4]);

	const Room& start = kRooms[0];
	const Room& last = kRooms[4];
	spawn = {RoomCenterX(start), RoomCenterY(start)};
	stairs = {RoomCenterX(last), RoomCenterY(last)};
	tiles[stairs.y][stairs.x] = TILE_STAIRS;
	westFloorX = start.x + 1;

	RebuildGraph();
}
