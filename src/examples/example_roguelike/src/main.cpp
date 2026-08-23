//! \file
//! \brief Terminal loop for the Gaia-ECS roguelike example.

#ifdef _WIN32
	#include <conio.h>
	#include <cstdlib>
#else
	#include <fcntl.h>
	#include <termios.h>
	#include <unistd.h>
#endif
#include <cstdio>
#include <cstring>

#include "roguelike_game.h"

using namespace gaia;

#ifndef _WIN32
//! Enables one-character terminal input and saves the original terminal state.
//! \param old Destination for the original terminal settings.
//! \return True when raw mode was enabled.
bool enable_raw_mode(termios* old) {
	if (tcgetattr(0, old) < 0) {
		perror("tcgetattr");
		return false;
	}

	termios raw = *old;
	raw.c_lflag &= ~(ICANON | ECHO);
	raw.c_cc[VMIN] = 1;
	raw.c_cc[VTIME] = 0;

	if (tcsetattr(0, TCSANOW, &raw) < 0) {
		perror("tcsetattr raw");
		return false;
	}
	return true;
}

//! Restores terminal settings saved by `enable_raw_mode`.
//! \param old Original terminal settings.
void disable_raw_mode(termios* old) {
	if (tcsetattr(0, TCSADRAIN, old) < 0)
		perror("tcsetattr restore");
}

//! Reads one character without waiting for a newline.
//! \return Character read from standard input, or zero after a read failure.
char get_char() {
	char buf[2] = {};
	termios old;
	const bool rawEnabled = enable_raw_mode(&old);

	const auto n = read(0, buf, sizeof(buf) - 1);
	if (n < 0)
		perror("read");

	if (rawEnabled)
		disable_raw_mode(&old);
	return n == 1 ? buf[0] : KEY_QUIT;
}

//! Clears the ANSI terminal display.
void clear_screen_main() {
	printf("\033[H\033[J");
	fflush(stdout);
}
#else
//! Reads one character from the Windows console.
//! \return Character read from the console.
char get_char() {
	return (char)_getch();
}
//! Clears the Windows console display.
void clear_screen_main() {
	system("cls");
}
#endif

//! Prints the control sheet before the first turn.
void print_welcome() {
	printf(
			"A Gaia-ECS dungeon.\n"
			"\nControls:\n"
			"  %c%c%c%c  move     space wait     %c shoot     %c quit\n"
			"  walk into a monster to melee\n"
			"  walk onto %c to descend\n"
			"\nGlyphs:\n"
			"  %c you   %c goblin   %c orc   %c potion   %c poison   %c gold   %c stairs\n"
			"\nPress any key...\n",
			KEY_UP, KEY_LEFT, KEY_DOWN, KEY_RIGHT, KEY_SHOOT, KEY_QUIT, TILE_STAIRS, TILE_PLAYER, TILE_ENEMY_GOBLIN,
			TILE_ENEMY_ORC, TILE_POTION, TILE_POISON, TILE_GOLD, TILE_STAIRS);
}

//! Checks wall stop plus a planted-foe melee hit after `--smoke`.
//! \param game World that just finished the smoke key sequence.
//! \param foe Goblin planted east of the player.
//! \return 0 on success. 1 when an invariant failed.
int run_smoke(Game& game, ecs::Entity foe) {
	if (!game.playerAlive || !game.world.valid(game.player)) {
		printf("smoke: player is gone\n");
		return 1;
	}
	if (game.floor != 1) {
		printf("smoke: expected floor 1, got %u\n", game.floor);
		return 1;
	}

	const auto p = game.world.get<Position>(game.player);
	if (p.x != game.dungeon.westFloorX || p.y != game.dungeon.spawn.y) {
		printf("smoke: expected player at %d,%d got %d,%d\n", game.dungeon.westFloorX, game.dungeon.spawn.y, p.x, p.y);
		return 1;
	}
	if (game.enemyCount < 3) {
		printf("smoke: expected at least 3 enemies, got %u\n", game.enemyCount);
		return 1;
	}
	if (!game.world.valid(foe) || !game.world.has<Health>(foe) || game.world.get<Health>(foe).value >= 20) {
		printf("smoke: expected a melee hit against the planted foe\n");
		return 1;
	}
	if (game.dungeon.At(game.dungeon.stairs.x, game.dungeon.stairs.y) != TILE_STAIRS) {
		printf("smoke: dungeon has no stairs\n");
		return 1;
	}
	if (game.world.get("game.monsters.Goblin") != game.prefabGoblin) {
		printf("smoke: module path game.monsters.Goblin did not resolve\n");
		return 1;
	}
	if (game.world.get("game.items.Potion") != game.prefabPotion) {
		printf("smoke: module path game.items.Potion did not resolve\n");
		return 1;
	}
	if (!game.world.is(game.prefabGoblin, game.prefabEnemy) || !game.world.is(game.prefabOrc, game.prefabEnemy)) {
		printf("smoke: goblin/orc prefabs are not as(Enemy)\n");
		return 1;
	}
	if (!game.world.is(foe, game.prefabGoblin)) {
		printf("smoke: planted foe is not a goblin instance\n");
		return 1;
	}
	if (!game.world.is(foe, game.prefabEnemy)) {
		printf("smoke: planted foe is not in the enemy family\n");
		return 1;
	}
	if (game.world.has_direct(foe, ecs::Pair(ecs::Is, game.prefabEnemy))) {
		printf("smoke: planted foe should inherit Enemy, not store Pair(Is, Enemy) directly\n");
		return 1;
	}
	if (!game.world.has(foe, ecs::Pair(ecs::ChildOf, game.floorRoot))) {
		printf("smoke: planted foe is not ChildOf the floor\n");
		return 1;
	}
	if (game.world.has(game.player, ecs::Pair(ecs::ChildOf, game.floorRoot))) {
		printf("smoke: player should not be ChildOf the floor\n");
		return 1;
	}
	if (game.world.find_prefab_instance(foe, game.prefabPack) == ecs::EntityBad) {
		printf("smoke: planted goblin has no pack instance\n");
		return 1;
	}

	printf(
			"smoke: ok (player at %d,%d, floor %u, foes %u, foe hp %d, floor bodies %u)\n", p.x, p.y, game.floor,
			game.enemyCount, game.world.get<Health>(foe).value, game.floorBodies);
	return 0;
}

//! Builds prefabs, registers systems, and runs one idle turn.
//! \param world Empty Gaia world.
//! \param game Game bound to `world`.
void start_scripted(ecs::World& world, Game& game) {
	game.quiet = true;
	game.color = false;
	game.CreatePrefabs();
	auto turn = world.add();
	world.add<Turn>(turn);
	register_systems(world, game);
	game.StartRun();
	game.pendingKey = 0;
	world.update();
}

//! Applies one recognized input and advances the world once.
//! Projectile instantiation happens here, before the locked system update.
//! \param world World that owns the systems.
//! \param game Game receiving `pendingKey`.
void update_game(ecs::World& world, Game& game) {
	const char key = game.pendingKey;
	if (key != 0 && key != KEY_UP && key != KEY_DOWN && key != KEY_LEFT && key != KEY_RIGHT && key != KEY_SHOOT &&
			key != KEY_WAIT && key != KEY_QUIT)
		return;

	if (key == KEY_SHOOT && game.world.valid(game.player) && game.world.has<Position>(game.player) &&
			game.world.has<Orientation>(game.player)) {
		const auto orientation = game.world.get<Orientation>(game.player);
		game.QueueArrow(game.world.get<Position>(game.player), {orientation.x, orientation.y});
		(void)game.SpawnQueuedArrow();
	}

	world.update();
}

//! Feeds one key and advances the game.
//! \param world World that owns the systems.
//! \param game Game receiving `pendingKey`.
//! \param key Input character.
void tap(ecs::World& world, Game& game, char key) {
	game.pendingKey = key;
	update_game(world, game);
}

//! True when any combat-log line contains `needle`.
//! \param game Game whose log is scanned.
//! \param needle Substring to find.
//! \return True if a line matches.
bool log_has(const Game& game, const char* needle) {
	GAIA_EACH(game.log) {
		if (strstr(game.log[i].text, needle) != nullptr)
			return true;
	}
	return false;
}

//! Shoots, walks after the shot, and point-blanks an adjacent goblin.
//! \return 0 on success. 1 when an invariant failed.
int run_shot_smoke() {
	{
		ecs::World world;
		Game game(world);
		start_scripted(world, game);
		tap(world, game, 'q');

		ecs::Entity firstArrow = ecs::EntityBad;
		world.uquery().all<Position>().all<Velocity>().each([&](ecs::Iter& it) {
			auto ve = it.view<ecs::Entity>();
			GAIA_EACH(it) {
				if (firstArrow == ecs::EntityBad && world.is(ve[i], game.prefabArrow))
					firstArrow = ve[i];
			}
		});
		if (firstArrow == ecs::EntityBad) {
			printf("smoke-shot: no arrow spawned\n");
			return 1;
		}
		const auto firstPosition = world.get<Position>(firstArrow);
		tap(world, game, 'x');
		if (world.get<Position>(firstArrow).x != firstPosition.x || world.get<Position>(firstArrow).y != firstPosition.y) {
			printf("smoke-shot: invalid input advanced the arrow\n");
			return 1;
		}
		tap(world, game, 'q');
		if (!world.valid(firstArrow) || world.get<Position>(firstArrow).x != firstPosition.x + 1 ||
				world.get<Position>(firstArrow).y != firstPosition.y) {
			printf("smoke-shot: repeated shooting advanced or consumed the first arrow incorrectly\n");
			return 1;
		}

		if (!game.world.valid(game.player) || !game.world.has<Health>(game.player)) {
			printf("smoke-shot: player is gone\n");
			return 1;
		}
		const int hp = game.world.get<Health>(game.player).value;
		if (hp != PlayerStartHealth) {
			printf("smoke-shot: expected hp %d, got %d\n", PlayerStartHealth, hp);
			return 1;
		}
		if (log_has(game, "potion")) {
			printf("smoke-shot: arrow was treated as a potion\n");
			return 1;
		}

		uint32_t arrows = 0;
		world.uquery().all<Position>().all<Velocity>().each([&](ecs::Iter& it) {
			auto ve = it.view<ecs::Entity>();
			GAIA_EACH(it) {
				if (world.is(ve[i], game.prefabArrow))
					++arrows;
			}
		});
		if (arrows == 0) {
			printf("smoke-shot: the arrow was consumed\n");
			return 1;
		}
	}

	{
		ecs::World world;
		Game game(world);
		start_scripted(world, game);
		const Position p{game.dungeon.spawn.x + 1, game.dungeon.spawn.y};
		if (!game.dungeon.IsWalkable(p.x, p.y)) {
			printf("smoke-shot: no walkable cell east of spawn\n");
			return 1;
		}
		const auto foe = game.SpawnFrom(game.prefabGoblin, p);
		tap(world, game, 'q');
		if (!game.world.valid(foe) || !game.world.has<Health>(foe) || game.world.get<Health>(foe).value >= 20) {
			printf("smoke-shot: adjacent shot missed the planted foe\n");
			return 1;
		}
		if (game.world.valid(foe) && game.world.has<Health>(foe) && game.world.get<Health>(foe).value != 8) {
			printf("smoke-shot: expected foe hp 8, got %d\n", game.world.get<Health>(foe).value);
			return 1;
		}
		tap(world, game, 'q');
		if (game.world.valid(foe)) {
			printf("smoke-shot: second adjacent shot did not delete the foe\n");
			return 1;
		}
	}

	{
		ecs::World world;
		Game game(world);
		start_scripted(world, game);
		const Position p{game.dungeon.spawn.x + 3, game.dungeon.spawn.y};
		if (!game.dungeon.IsWalkable(p.x, p.y)) {
			printf("smoke-shot: no ranged-shot corridor east of spawn\n");
			return 1;
		}
		const auto foe = game.SpawnFrom(game.prefabGoblin, p);
		tap(world, game, 'q');
		tap(world, game, ' ');
		if (!game.world.valid(foe) || game.world.get<Health>(foe).value != 8) {
			printf("smoke-shot: crossing arrow hit the foe more than once\n");
			return 1;
		}
	}

	{
		ecs::World world;
		Game game(world);
		start_scripted(world, game);
		ecs::Entity sample = ecs::EntityBad;
		world.uquery().all<Position>().all<Health>().each([&](ecs::Iter& it) {
			auto ve = it.view<ecs::Entity>();
			GAIA_EACH(it) {
				if (sample == ecs::EntityBad && world.is(ve[i], game.prefabGoblin))
					sample = ve[i];
			}
		});
		const auto oldFloor = game.floorRoot;
		if (sample == ecs::EntityBad || oldFloor == ecs::EntityBad) {
			printf("smoke-shot: no floor contents to wipe\n");
			return 1;
		}
		game.GenerateFloor(2);
		if (game.world.valid(oldFloor) || game.world.valid(sample)) {
			printf("smoke-shot: floor wipe left old entities\n");
			return 1;
		}
		if (!game.world.valid(game.player) || game.world.has(game.player, ecs::Pair(ecs::ChildOf, game.floorRoot))) {
			printf("smoke-shot: player did not survive the descent\n");
			return 1;
		}
	}

	{
		ecs::World world;
		Game game(world);
		start_scripted(world, game);
		const Position enemyPos{game.dungeon.spawn.x + 1, game.dungeon.spawn.y};
		const Position poisonPos{game.dungeon.spawn.x + 2, game.dungeon.spawn.y};
		const Position playerPos{game.dungeon.spawn.x + 3, game.dungeon.spawn.y};
		if (!game.dungeon.IsWalkable(enemyPos.x, enemyPos.y) || !game.dungeon.IsWalkable(poisonPos.x, poisonPos.y) ||
				!game.dungeon.IsWalkable(playerPos.x, playerPos.y)) {
			printf("smoke-shot: no walkable cells for enemy-item ownership check\n");
			return 1;
		}

		game.world.set<Position>(game.player) = playerPos;
		const auto foe = game.SpawnFrom(game.prefabGoblin, enemyPos);
		const auto poison = game.SpawnFrom(game.prefabPoison, poisonPos);
		tap(world, game, KEY_WAIT);
		if (!game.world.valid(poison) || game.world.get<Health>(foe).value != 20) {
			printf("smoke-shot: enemy consumed a player pickup\n");
			return 1;
		}
	}

	printf("smoke-shot: ok\n");
	return 0;
}

//! Runs the dungeon. `--smoke`, `--keys`, and `--no-color` are optional.
//! \param argc Argument count.
//! \param argv Argument vector.
//! \return 0 on a normal exit. 1 when `--smoke` fails.
int main(int argc, char** argv) {
	bool smoke = false;
	const char* keys = nullptr;
	bool noColor = false;
	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--smoke") == 0)
			smoke = true;
		else if (strcmp(argv[i], "--keys") == 0 && i + 1 < argc)
			keys = argv[++i];
		else if (strcmp(argv[i], "--no-color") == 0)
			noColor = true;
	}

	const bool scripted = smoke || keys != nullptr;
	if (smoke && keys == nullptr)
		keys = "daaaaaaaaaaaaaaaaaaaa";

	if (!scripted) {
		clear_screen_main();
		print_welcome();
		(void)get_char();
	}

	ecs::World world;
	Game game(world);
	game.quiet = scripted;
	game.color = !scripted && !noColor;

	// Prefabs first so systems can use them in semantic query terms.
	game.CreatePrefabs();

	auto turn = world.add();
	world.add<Turn>(turn);
	world.name(turn, "GameTurn");

	register_systems(world, game);
	game.StartRun();

	ecs::Entity smokeFoe = ecs::EntityBad;
	if (smoke) {
		const Position p{game.dungeon.spawn.x + 1, game.dungeon.spawn.y};
		if (game.dungeon.IsWalkable(p.x, p.y)) {
			smokeFoe = game.SpawnFrom(game.prefabGoblin, p);
		}
	}

	game.pendingKey = 0;
	update_game(world, game);

	uint32_t keyIndex = 0;
	while (!game.terminate) {
		if (scripted) {
			if (keys == nullptr || keys[keyIndex] == '\0')
				game.pendingKey = KEY_QUIT;
			else
				game.pendingKey = keys[keyIndex++];
		} else {
			game.pendingKey = get_char();
		}
		update_game(world, game);
	}

	if (smoke) {
		const int melee = run_smoke(game, smokeFoe);
		if (melee != 0)
			return melee;
		return run_shot_smoke();
	}

	return 0;
}
