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
	return x >= 0 && y >= 0 && x < ScreenX && y < ScreenY;
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
	for (int y = r.y + 1; y < r.y + r.h - 1; ++y) {
		for (int x = r.x + 1; x < r.x + r.w - 1; ++x) {
			if (InBounds(x, y))
				tiles[y][x] = TILE_FREE;
		}
	}
}

void Dungeon::CarveH(int x0, int x1, int y) {
	if (x0 > x1)
		gaia::core::swap(x0, x1);
	for (int x = x0; x <= x1; ++x) {
		if (InBounds(x, y))
			tiles[y][x] = TILE_FREE;
	}
}

void Dungeon::CarveV(int y0, int y1, int x) {
	if (y0 > y1)
		gaia::core::swap(y0, y1);
	for (int y = y0; y <= y1; ++y) {
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
	graph.reserve((uint32_t)(ScreenX * ScreenY));
	uint32_t index = 0;
	for (int y = 0; y < ScreenY; ++y) {
		for (int x = 0; x < ScreenX; ++x) {
			AStar::Node node{index++};
			if (IsWalkable(x, y)) {
				if (y > 0)
					node.InitIndex(0, IsWalkable(x, y - 1) ? 1 : 0);
				if (x < ScreenX - 1)
					node.InitIndex(1, IsWalkable(x + 1, y) ? 1 : 0);
				if (y < ScreenY - 1)
					node.InitIndex(2, IsWalkable(x, y + 1) ? 1 : 0);
				if (x > 0)
					node.InitIndex(3, IsWalkable(x - 1, y) ? 1 : 0);
			}
			graph.push_back(GAIA_MOV(node));
		}
	}
}

void Dungeon::Generate() {
	for (int y = 0; y < ScreenY; ++y) {
		for (int x = 0; x < ScreenX; ++x) {
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
