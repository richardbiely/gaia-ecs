#pragma once

#include "roguelike_game.h"

//! \file
//! \brief Terminal view. Reads the dungeon and sprites. Writes no gameplay state.

//! Rebuilds `visible` and `seen` from the player cell.
//! \param game Application whose view fields are updated.
void view_recompute_fov(Game& game);

//! Copies dungeon tiles into the glyph buffer.
//! \param game Application whose glyph buffer is replaced.
void view_copy_terrain(Game& game);

//! Stamps one sprite onto the glyph buffer.
//! \param game Application whose glyph buffer is written.
//! \param x Column.
//! \param y Row.
//! \param tile Glyph to draw.
void view_stamp(Game& game, int x, int y, char tile);

//! Prints the map, HUD is printed by systems after this.
//! \param game Application to draw.
void view_draw_map(const Game& game);

//! Clears the terminal when the game is not in quiet mode.
//! \param game Application whose `quiet` flag is read.
void view_begin_frame(const Game& game);
