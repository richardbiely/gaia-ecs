//! \file
//! \brief Field of view and terminal drawing.

#include "roguelike_render.h"

#ifdef _WIN32
	#include <cstdlib>
#endif
#include <cstdio>
#include <cstring>

static bool has_los(const Dungeon& dungeon, int x0, int y0, int x1, int y1) {
	int dx = x1 - x0;
	int dy = y1 - y0;
	const int sx = dx < 0 ? -1 : 1;
	const int sy = dy < 0 ? -1 : 1;
	dx = dx < 0 ? -dx : dx;
	dy = dy < 0 ? -dy : dy;

	int x = x0;
	int y = y0;
	int err = dx - dy;
	for (;;) {
		if (x == x1 && y == y1)
			return true;
		if (!(x == x0 && y == y0) && dungeon.IsWall(x, y))
			return false;

		const int e2 = 2 * err;
		if (e2 > -dy) {
			err -= dy;
			x += sx;
		}
		if (e2 < dx) {
			err += dx;
			y += sy;
		}
	}
}

static const char* color_for(char tile, bool dim, bool color) {
	if (!color)
		return nullptr;
	if (dim)
		return "90";
	switch (tile) {
		case TILE_PLAYER:
			return "92";
		case TILE_ENEMY_GOBLIN:
			return "93";
		case TILE_ENEMY_ORC:
			return "91";
		case TILE_POTION:
			return "96";
		case TILE_POISON:
			return "95";
		case TILE_GOLD:
			return "33";
		case TILE_STAIRS:
			return "97";
		case TILE_ARROW:
			return "97";
		case TILE_WALL:
			return "37";
		case TILE_FREE:
			return "90";
		default:
			return "37";
	}
}

static void put_glyph(char tile, bool dim, bool color) {
	const char* col = color_for(tile, dim, color);
	if (col == nullptr) {
		putchar(tile);
		return;
	}
	printf("\033[%sm%c\033[0m", col, tile);
}

void view_begin_frame(const Game& game) {
	if (game.quiet)
		return;
#ifdef _WIN32
	system("cls");
#else
	printf("\033[H\033[J");
	fflush(stdout);
#endif
}

void view_copy_terrain(Game& game) {
	GAIA_FOR_(ScreenY, y) {
		GAIA_FOR_(ScreenX, x) {
			game.glyphs[y][x] = game.dungeon.tiles[y][x];
		}
	}
}

void view_stamp(Game& game, int x, int y, char tile) {
	if (!game.dungeon.InBounds(x, y))
		return;
	game.glyphs[y][x] = tile;
}

void view_recompute_fov(Game& game) {
	memset(game.visible, 0, sizeof(game.visible));
	if (!game.playerAlive || !game.world.valid(game.player))
		return;

	const auto p = game.world.get<Position>(game.player);
	const int r = (int)FovRadius;
	const int y0 = p.y - r < 0 ? 0 : p.y - r;
	const int y1 = p.y + r + 1 > (int)ScreenY ? (int)ScreenY : p.y + r + 1;
	const int x0 = p.x - r < 0 ? 0 : p.x - r;
	const int x1 = p.x + r + 1 > (int)ScreenX ? (int)ScreenX : p.x + r + 1;
	GAIA_FOR2_(y0, y1, y) {
		GAIA_FOR2_(x0, x1, x) {
			if (!game.dungeon.InBounds(x, y))
				continue;
			const int dx = x - p.x;
			const int dy = y - p.y;
			if (dx * dx + dy * dy > r * r)
				continue;
			if (!has_los(game.dungeon, p.x, p.y, x, y))
				continue;
			game.visible[y][x] = true;
			game.seen[y][x] = true;
		}
	}
}

void view_draw_map(const Game& game) {
	GAIA_FOR_(ScreenY, y) {
		GAIA_FOR_(ScreenX, x) {
			if (!game.visible[y][x] && !game.seen[y][x]) {
				putchar(' ');
				continue;
			}
			const bool dim = !game.visible[y][x];
			const char tile = dim ? game.dungeon.tiles[y][x] : game.glyphs[y][x];
			put_glyph(tile, dim, game.color);
		}
		printf("\n");
	}
}
