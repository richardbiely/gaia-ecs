//! \file
//! \brief Prefabs, floor population, occupancy, and movement resolution.

#include "roguelike_game.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

void Occupancy::Clear() {
	GAIA_FOR_(ScreenY, y) {
		GAIA_FOR_(ScreenX, x) {
			cells[y][x].clear();
		}
	}
}

void Occupancy::Add(int x, int y, gaia::ecs::Entity e) {
	if (x < 0 || y < 0 || x >= (int)ScreenX || y >= (int)ScreenY)
		return;
	if (cells[y][x].size() < MaxBodiesPerCell)
		cells[y][x].push_back(e);
}

const gaia::cnt::sarray_ext<gaia::ecs::Entity, MaxBodiesPerCell>& Occupancy::At(int x, int y) const {
	static const gaia::cnt::sarray_ext<gaia::ecs::Entity, MaxBodiesPerCell> kEmpty{};
	if (x < 0 || y < 0 || x >= (int)ScreenX || y >= (int)ScreenY)
		return kEmpty;
	return cells[y][x];
}

Game& game_of(gaia::ecs::Iter& it) {
	auto* p = static_cast<Game*>(it.ctx());
	GAIA_ASSERT(p != nullptr);
	return *p;
}

void Game::log_msg(const char* fmt, ...) {
	LogLine line{};
	va_list args;
	va_start(args, fmt);
	vsnprintf(line.text, LogWidth, fmt, args);
	va_end(args);

	if (log.size() == LogLines) {
		GAIA_FOR(LogLines - 1) {
			log[i] = log[i + 1];
		}
		log.pop_back();
	}
	log.push_back(line);
}

void Game::CreatePrefabs() {
	modMonsters = world.module("game.monsters");
	modItems = world.module("game.items");
	modPhases = world.module("game.phases");

	world.scope(modMonsters, [&] {
		prefabEnemy = world.prefab();
		world.child(prefabEnemy, modMonsters);
		world.build(prefabEnemy).add<Velocity>().add<RigidBody>().add<Health>().add<BattleStats>().add<Enemy>();
		world
				.acc_mut(prefabEnemy) //
				.set<Velocity>({0, 0})
				.set<Health>({20, 20})
				.set<BattleStats>({6, 3});
		world.name(prefabEnemy, "EnemyBase");

		prefabGoblin = world.prefab();
		world.child(prefabGoblin, modMonsters);
		world.build(prefabGoblin)
				.add<Sprite>()
				.add<Velocity>()
				.add<RigidBody>()
				.add<Health>()
				.add<BattleStats>()
				.add<Enemy>();
		world
				.acc_mut(prefabGoblin) //
				.set<Velocity>({0, 0})
				.set<Sprite>({TILE_ENEMY_GOBLIN})
				.set<Health>({20, 20})
				.set<BattleStats>({6, 3});
		world.name(prefabGoblin, "Goblin");

		prefabPack = world.prefab();
		world.build(prefabPack).add<BattleStats>();
		world.set<BattleStats>(prefabPack) = {2, 0};
		world.name(prefabPack, "Pack");
		world.child(prefabPack, prefabGoblin);

		prefabOrc = world.prefab();
		world.child(prefabOrc, modMonsters);
		world.build(prefabOrc).add<Sprite>().add<Velocity>().add<RigidBody>().add<Health>().add<BattleStats>().add<Enemy>();
		world
				.acc_mut(prefabOrc) //
				.set<Velocity>({0, 0})
				.set<Sprite>({TILE_ENEMY_ORC})
				.set<Health>({30, 30})
				.set<BattleStats>({8, 5});
		world.name(prefabOrc, "Orc");
	});

	world.scope(modItems, [&] {
		prefabPotion = world.prefab();
		world.child(prefabPotion, modItems);
		world.build(prefabPotion).add<Sprite>().add<RigidBody>().add<Item>().add<BattleStats>();
		world
				.acc_mut(prefabPotion) //
				.set<Sprite>({TILE_POTION})
				.set<Item>({ItemType::Potion})
				.set<BattleStats>({20, 0});
		world.name(prefabPotion, "Potion");

		prefabPoison = world.prefab();
		world.child(prefabPoison, modItems);
		world.build(prefabPoison).add<Sprite>().add<RigidBody>().add<Item>().add<BattleStats>();
		world
				.acc_mut(prefabPoison) //
				.set<Sprite>({TILE_POISON})
				.set<Item>({ItemType::Poison})
				.set<BattleStats>({-15, 0});
		world.name(prefabPoison, "Poison");

		prefabGold = world.prefab();
		world.child(prefabGold, modItems);
		world.build(prefabGold).add<Sprite>().add<RigidBody>().add<Item>().add<BattleStats>();
		world
				.acc_mut(prefabGold) //
				.set<Sprite>({TILE_GOLD})
				.set<Item>({ItemType::Gold})
				.set<BattleStats>({15, 0});
		world.name(prefabGold, "Gold");

		prefabArrow = world.prefab();
		world.child(prefabArrow, modItems);
		world
				.build(prefabArrow) //
				.add<Velocity>()
				.add<Sprite>()
				.add<RigidBody>()
				.add<BattleStats>()
				.add<Health>();
		world
				.acc_mut(prefabArrow) //
				.set<Sprite>({TILE_ARROW})
				.set<BattleStats>({15, 0})
				.set<Health>({1, 1});
		world.name(prefabArrow, "Arrow");
	});

	// Module paths are the lookup source after registration.
	const auto goblinPath = world.get("game.monsters.Goblin");
	if (goblinPath != gaia::ecs::EntityBad)
		prefabGoblin = goblinPath;
	const auto orcPath = world.get("game.monsters.Orc");
	if (orcPath != gaia::ecs::EntityBad)
		prefabOrc = orcPath;
	const auto enemyPath = world.get("game.monsters.EnemyBase");
	if (enemyPath != gaia::ecs::EntityBad)
		prefabEnemy = enemyPath;
	const auto potionPath = world.get("game.items.Potion");
	if (potionPath != gaia::ecs::EntityBad)
		prefabPotion = potionPath;
	const auto poisonPath = world.get("game.items.Poison");
	if (poisonPath != gaia::ecs::EntityBad)
		prefabPoison = poisonPath;
	const auto goldPath = world.get("game.items.Gold");
	if (goldPath != gaia::ecs::EntityBad)
		prefabGold = goldPath;
	const auto arrowPath = world.get("game.items.Arrow");
	if (arrowPath != gaia::ecs::EntityBad)
		prefabArrow = arrowPath;

	world.as(prefabGoblin, prefabEnemy);
	world.as(prefabOrc, prefabEnemy);
}

gaia::ecs::Entity Game::SpawnFrom(gaia::ecs::Entity prefab, Position p) {
	auto e = world.instantiate(prefab);
	world.add<Position>(e, p);
	if (world.valid(floorRoot))
		world.child(e, floorRoot);
	return e;
}

void Game::WipeFloorEntities() {
	if (world.valid(floorRoot))
		world.del(floorRoot);
	floorRoot = gaia::ecs::EntityBad;
	enemyCount = 0;
	floorBodies = 0;
	occupancy.Clear();
	colliding.clear();
}

void Game::RecountCensus() {
	enemyCount = 0;
	floorBodies = 0;

	world.uquery().in(prefabEnemy).each([&](gaia::ecs::Entity) {
		++enemyCount;
	});

	const bool hasFloor = world.valid(floorRoot);
	world.uquery().all<RigidBody>().each([&](gaia::ecs::Iter& it) {
		auto ve = it.view<gaia::ecs::Entity>();
		GAIA_EACH(it) {
			const auto e = ve[i];
			if (hasFloor && world.has(e, gaia::ecs::Pair(gaia::ecs::ChildOf, floorRoot)))
				++floorBodies;
		}
	});
}

void Game::PlacePlayer() {
	if (!world.valid(player)) {
		player = world.add();
		world
				.build(player) //
				.add<Position>()
				.add<Velocity>()
				.add<RigidBody>()
				.add<Orientation>()
				.add<Sprite>()
				.add<Health>()
				.add<BattleStats>()
				.add<Player>();
		world
				.acc_mut(player) //
				.set<Velocity>({0, 0})
				.set<Orientation>({1, 0})
				.set<Sprite>({TILE_PLAYER})
				.set<Health>({PlayerStartHealth, 100})
				.set<BattleStats>({12, 5});
		world.name(player, "Hero");
		playerAlive = true;
	}
	world.set<Position>(player) = dungeon.spawn;
	world.set<Velocity>(player) = {0, 0};
}

void Game::SpawnFloorPopulation() {
	const int bonus = (int)(floor - 1);

	auto g1 = SpawnFrom(prefabGoblin, {Dungeon::RoomCenterX(kRooms[1]), Dungeon::RoomCenterY(kRooms[1])});
	auto o1 = SpawnFrom(prefabOrc, {Dungeon::RoomCenterX(kRooms[2]), Dungeon::RoomCenterY(kRooms[2])});
	auto g2 = SpawnFrom(prefabGoblin, {Dungeon::RoomCenterX(kRooms[3]) - 2, Dungeon::RoomCenterY(kRooms[3])});
	auto o2 = SpawnFrom(prefabOrc, {Dungeon::RoomCenterX(kRooms[4]) - 4, Dungeon::RoomCenterY(kRooms[4])});

	if (bonus > 0) {
		auto os1 = world.get<BattleStats>(o1);
		os1.power += bonus;
		world.set<BattleStats>(o1) = os1;
		auto os2 = world.get<BattleStats>(o2);
		os2.power += bonus;
		world.set<BattleStats>(o2) = os2;
		auto hg = world.get<Health>(g1);
		hg.valueMax += 4 * bonus;
		hg.value += 4 * bonus;
		world.set<Health>(g1) = hg;
		(void)g2;
	}

	SpawnFrom(prefabPotion, {dungeon.spawn.x + 2, dungeon.spawn.y - 1});
	SpawnFrom(prefabPoison, {Dungeon::RoomCenterX(kRooms[2]) - 3, Dungeon::RoomCenterY(kRooms[2])});
	SpawnFrom(prefabGold, {Dungeon::RoomCenterX(kRooms[3]) + 3, Dungeon::RoomCenterY(kRooms[3])});

	if (floor >= 3)
		SpawnFrom(prefabGoblin, {Dungeon::RoomCenterX(kRooms[1]) - 3, Dungeon::RoomCenterY(kRooms[1])});
}

void Game::GenerateFloor(uint32_t nextFloor) {
	if (nextFloor > MaxFloors) {
		escaped = true;
		log_msg("You climb out of the dungeon. Score %u.", score);
		return;
	}

	WipeFloorEntities();
	floor = nextFloor;
	floorCleared = false;
	dungeon.Generate();
	memset(visible, 0, sizeof(visible));
	memset(seen, 0, sizeof(seen));
	PlacePlayer();

	floorRoot = world.add();
	char floorName[16];
	snprintf(floorName, sizeof(floorName), "Floor%u", floor);
	world.name(floorRoot, floorName);
	SpawnFloorPopulation();
	log_msg("Floor %u. The air grows heavier.", floor);
}

void Game::StartRun() {
	GenerateFloor(1);
	log.clear();
	log_msg("A Gaia-carved dungeon. Descend %c to escape.", TILE_STAIRS);
}

void Game::QueueArrow(Position p, Velocity v) {
	pendingShot = true;
	pendingShotPos = p;
	pendingShotVel = v;
}

bool Game::SpawnQueuedArrow() {
	if (!pendingShot)
		return false;
	pendingShot = false;

	const int x = pendingShotPos.x + pendingShotVel.x;
	const int y = pendingShotPos.y + pendingShotVel.y;
	if (!dungeon.InBounds(x, y) || dungeon.IsWall(x, y)) {
		log_msg("The shot hits stone.");
		return false;
	}

	auto e = SpawnFrom(prefabArrow, pendingShotPos);
	world.set<Velocity>(e) = pendingShotVel;
	return true;
}

void Game::ResolveMovement() {
	struct Mover {
		gaia::ecs::Entity e;
		Position p;
		Velocity v;
	};

	gaia::cnt::darray<Mover> movers;
	world.uquery().all<Position>().all<Velocity>().all<RigidBody>().each([&](gaia::ecs::Iter& it) {
		auto ve = it.view<gaia::ecs::Entity>();
		auto vp = it.view<Position>();
		auto vv = it.view<Velocity>();
		GAIA_EACH_(it, row) {
			if (vv[row].x == 0 && vv[row].y == 0)
				continue;
			movers.push_back({ve[row], vp[row], vv[row]});
		}
	});

	GAIA_FOR_(movers.size(), a) {
		GAIA_FOR2_(a + 1, movers.size(), b) {
			if (movers[b].e.id() < movers[a].e.id())
				gaia::core::swap(movers[a], movers[b]);
		}
	}

	gaia::ecs::Entity claimed[ScreenY][ScreenX];
	GAIA_FOR_(ScreenY, y) {
		GAIA_FOR_(ScreenX, x) {
			claimed[y][x] = gaia::ecs::EntityBad;
		}
	}

	GAIA_EACH_(movers, mi) {
		Mover& m = movers[mi];
		const int sx = m.v.x == 0 ? 0 : (m.v.x > 0 ? 1 : -1);
		const int sy = m.v.y == 0 ? 0 : (m.v.y > 0 ? 1 : -1);
		const int ii = m.v.y != 0;
		int pp[2] = {m.p.x + sx, m.p.y + sy};
		const int axisV = ii ? m.v.y : m.v.x;
		const int axisD = ii ? sy : sx;
		const int ppEnd = pp[ii] + axisV;
		int traveled[2] = {0, 0};
		bool hit = false;

		for (; pp[ii] != ppEnd; pp[ii] += axisD, traveled[ii] += axisD) {
			const int dx = pp[0];
			const int dy = pp[1];
			if (dungeon.IsWall(dx, dy)) {
				colliding.push_back({m.e, gaia::ecs::EntityBad, Position{dx, dy}, m.v});
				hit = true;
				break;
			}

			const auto& occ = occupancy.At(dx, dy);
			bool blocked = false;
			GAIA_EACH_(occ, oi) {
				const auto e2 = occ[oi];
				if (e2 == m.e)
					continue;
				if (ShotPasses(m.e, e2))
					continue;
				colliding.push_back({m.e, e2, Position{dx, dy}, m.v});
				blocked = true;
			}
			if (blocked) {
				hit = true;
				break;
			}

			if (claimed[dy][dx] != gaia::ecs::EntityBad && claimed[dy][dx] != m.e && !ShotPasses(m.e, claimed[dy][dx])) {
				hit = true;
				break;
			}

			claimed[dy][dx] = m.e;
		}

		if (!hit)
			continue;

		if (m.v.x != 0)
			world.set<Velocity>(m.e) = {traveled[0], 0};
		else
			world.set<Velocity>(m.e) = {0, traveled[1]};
	}
}

bool Game::ShotPasses(gaia::ecs::Entity mover, gaia::ecs::Entity occupant) const {
	const bool moverIsPlayer = world.has<Player>(mover);
	const bool occupantIsPlayer = world.has<Player>(occupant);
	const bool moverIsArrow = world.is(mover, prefabArrow);
	const bool occupantIsArrow = world.is(occupant, prefabArrow);
	if (moverIsArrow && occupantIsArrow) {
		const auto moverVelocity = world.get<Velocity>(mover);
		const auto occupantVelocity = world.get<Velocity>(occupant);
		return moverVelocity.x == occupantVelocity.x && moverVelocity.y == occupantVelocity.y;
	}
	return (moverIsPlayer && occupantIsArrow) || (moverIsArrow && occupantIsPlayer);
}

bool Game::TryStep(int x, int y, Velocity& v, int vx, int vy) const {
	const int nx = x + vx;
	const int ny = y + vy;
	if (!dungeon.IsWalkable(nx, ny))
		return false;
	v = {vx, vy};
	return true;
}

void Game::ChasePlayer(const Position& p, Velocity& v) {
	v = {0, 0};
	if (!playerActed || !playerAlive || !world.valid(player))
		return;

	const auto pp = world.get<Position>(player);
	const int dx = pp.x - p.x;
	const int dy = pp.y - p.y;
	const int adx = dx < 0 ? -dx : dx;
	const int ady = dy < 0 ? -dy : dy;
	if (adx + ady == 0)
		return;
	if (adx + ady > 10)
		return;

	if (adx + ady == 1) {
		v = {dx, dy};
		return;
	}

	AStar astar;
	auto path = astar.FindPath(
			dungeon.graph, AStar::NodeIdFromXY((uint32_t)p.x, (uint32_t)p.y),
			AStar::NodeIdFromXY((uint32_t)pp.x, (uint32_t)pp.y));
	if (path.size() >= 2) {
		const auto nx = (int)AStar::NodeIdToX(path[1]);
		const auto ny = (int)AStar::NodeIdToY(path[1]);
		v = {nx - p.x, ny - p.y};
		return;
	}

	const int sx = dx == 0 ? 0 : (dx > 0 ? 1 : -1);
	const int sy = dy == 0 ? 0 : (dy > 0 ? 1 : -1);
	if (adx >= ady) {
		if (!TryStep(p.x, p.y, v, sx, 0))
			(void)TryStep(p.x, p.y, v, 0, sy);
	} else {
		if (!TryStep(p.x, p.y, v, 0, sy))
			(void)TryStep(p.x, p.y, v, sx, 0);
	}
}

void Game::TryDescend() {
	if (!playerAlive || !world.valid(player))
		return;
	const auto p = world.get<Position>(player);
	if (!dungeon.InBounds(p.x, p.y) || dungeon.At(p.x, p.y) != TILE_STAIRS)
		return;
	GenerateFloor(floor + 1);
}
