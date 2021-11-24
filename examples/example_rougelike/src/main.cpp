#include "gaia/ecs/command_buffer.h"
#include "gaia/ecs/entity.h"
#include "gaia/ecs/fwd.h"
#include "gaia/utils/vector.h"
#ifdef WIN32
	#include <conio.h>
#else
	#include <termios.h>
	#include <unistd.h>
	#include <term.h>
#endif
#include <cstdio>
#define _ITERATOR_DEBUG_LEVEL 0
#include <gaia.h>

GAIA_INIT

using namespace gaia;

#pragma region Plaform specific helper functions

#ifndef WIN32
char _getch() {
	char buf = 0;
	struct termios old = {};
	if (tcgetattr(0, &old) < 0)
		perror("tcsetattr()");
	old.c_lflag &= ~ICANON;
	old.c_lflag &= ~ECHO;
	old.c_cc[VMIN] = 1;
	old.c_cc[VTIME] = 0;
	if (tcsetattr(0, TCSANOW, &old) < 0)
		perror("tcsetattr ICANON");
	if (read(0, &buf, 1) < 0)
		perror("read()");
	old.c_lflag |= ICANON;
	old.c_lflag |= ECHO;
	if (tcsetattr(0, TCSADRAIN, &old) < 0)
		perror("tcsetattr ~ICANON");
	return (buf);
}

void ClearScreen() {
	system("clear");
}
#else
void ClearStream() {
	system("cls");
}
#endif

#pragma endregion

#pragma region Components

struct Position {
	int x, y;

	bool operator==(const Position& other) const {
		return x == other.x && y == other.y;
	}
};

namespace std {
	template <>
	struct hash<Position> {
		std::size_t operator()(const Position& p) const {
			return gaia::utils::detail::hash_combine2((uint64_t)p.x, (uint64_t)p.y);
		}
	};
} // namespace std

struct Velocity {
	int x, y;
};
struct Sprite {
	char value;
};
struct Health {
	int value;
	int valueMax;
};
struct BattleStats {
	int power;
	int armor;
};
struct Item {};
struct Player {};

#pragma endregion

constexpr int ScreenX = 40;
constexpr int ScreenY = 15;
constexpr char TILE_WALL = '#';
constexpr char TILE_FREE = ' ';
constexpr char TILE_PLAYER = 'P';
constexpr char TILE_ENEMY_GOBLIN = 'G';
constexpr char TILE_ENEMY_ORC = 'O';
constexpr char TILE_POTION = 'i';
constexpr char TILE_POISON = 'p';

struct World {
	ecs::World& w;
	//! map tiles
	char map[ScreenY][ScreenX];
	//! blocked tiles
	bool blocked[ScreenY][ScreenX];
	//! tile content
	utils::map<Position, utils::vector<ecs::Entity>> content;

	World(ecs::World& world): w(world) {}

	void Init() {
		InitWorldMap();
		CreatePlayer();
		CreateEnemies();
		CreateItems();
	}

	void InitWorldMap() {
		// map
		for (int y = 0; y < ScreenY; y++) {
			for (int x = 0; x < ScreenX; x++) {
				map[y][x] = TILE_FREE;
			}
		}

		// edges
		for (int y = 1; y < ScreenY - 1; y++) {
			map[y][0] = TILE_WALL;
			map[y][ScreenX - 1] = TILE_WALL;
		}
		for (int x = 0; x < ScreenX; x++) {
			map[0][x] = TILE_WALL;
			map[ScreenY - 1][x] = TILE_WALL;
		}
	}

	void CreatePlayer() {
		auto player = w.CreateEntity();
		w.AddComponent<Position>(player, {5, 10});
		w.AddComponent<Velocity>(player, {0, 0});
		w.AddComponent<Sprite>(player, {TILE_PLAYER});
		w.AddComponent<Health>(player, {100, 100});
		w.AddComponent<BattleStats>(player, {9, 5});
		w.AddComponent<Player>(player);
	}

	void CreateEnemies() {
		utils::array<ecs::Entity, 3> enemies;
		for (size_t i = 0; i < enemies.size(); ++i) {
			auto& e = enemies[i];
			e = w.CreateEntity();
			w.AddComponent<Position>(e, {});
			w.AddComponent<Velocity>(e, {0, 0});
			const bool isOrc = i % 2;
			if (isOrc) {
				w.AddComponent<Sprite>(e, {TILE_ENEMY_ORC});
				w.AddComponent<Health>(e, {120, 120});
				w.AddComponent<BattleStats>(e, {12, 7});
			} else {
				w.AddComponent<Sprite>(e, {TILE_ENEMY_GOBLIN});
				w.AddComponent<Health>(e, {80, 80});
				w.AddComponent<BattleStats>(e, {10, 5});
			}
		}
		w.SetComponent<Position>(enemies[0], {8, 8});
		w.SetComponent<Position>(enemies[1], {10, 10});
		w.SetComponent<Position>(enemies[2], {12, 12});
	}

	void CreateItems() {
		auto potion = w.CreateEntity();
		w.AddComponent<Position>(potion, {5, 5});
		w.AddComponent<Sprite>(potion, {TILE_POTION});
		w.AddComponent<Item>(potion);

		auto poison = w.CreateEntity();
		w.AddComponent<Position>(poison, {15, 10});
		w.AddComponent<Sprite>(poison, {TILE_POISON});
		w.AddComponent<Item>(poison);
	}
};

ecs::World s_ecs;
ecs::SystemManager s_sm(s_ecs);
World s_world(s_ecs);

class UpdateMapSystem final: public ecs::System {
	ecs::EntityQuery m_q;

public:
	void OnCreated() override {
		m_q.All<Position, Velocity>();
	}
	void OnUpdate() override {
		// wall blocks a tile
		for (int y = 0; y < ScreenY; y++)
			for (int x = 0; x < ScreenX; x++)
				s_world.blocked[y][x] = s_world.map[y][x] == TILE_WALL;

		// everything with velocity blocks
		GetWorld()
				.ForEach(
						m_q, [&](const Position& p) { s_world.blocked[p.y][p.x] = true; })
				.Run();

		// everything with position is content
		s_world.content.clear();
		GetWorld()
				.ForEach([&](ecs::Entity e, const Position& p) {
					s_world.content[p].push_back(e);
				})
				.Run();
	}
};

struct CollisionData {
	//! Entity colliding
	ecs::Entity e;
	//! Entity being collided with
	ecs::Entity e2;
	//! Position of collision
	Position p;
	//! Velocity at the time of collision
	Velocity v;
};

class CollisionSystem final: public ecs::System {
	ecs::EntityQuery m_q;
	utils::vector<CollisionData> m_colliding;

public:
	void OnCreated() override {
		m_q.All<Position, Velocity>();
	}

	void OnUpdate() override {
		// Drop all previous collision records
		m_colliding.clear();

		GetWorld()
				.ForEach(
						m_q,
						[&](ecs::Entity e, const Position& p, Velocity& v) {
							// Skip stationary objects
							if (v.x == 0 && v.y == 0)
								return;

							const int nx = p.x + v.x;
							const int ny = p.y + v.y;

							if (!s_world.blocked[ny][nx])
								return;

							if (s_world.content.find({nx, ny}) == s_world.content.end())
								return;

							// Collect all collisions
							for (auto e2: s_world.content[{nx, ny}])
								m_colliding.push_back(CollisionData{e, e2, {nx, ny}, v});

							v.x = 0;
							v.y = 0;
						})
				.Run();
	}

	const utils::vector<CollisionData>& GetCollisions() const {
		return m_colliding;
	}
};

class MoveSystem final: public ecs::System {
	ecs::EntityQuery m_q;

public:
	void OnCreated() override {
		m_q.All<Position, Velocity>();
	}

	void OnUpdate() override {
		// Update position based on current velocity
		GetWorld()
				.ForEach(
						m_q,
						[&](Position& p, const Velocity& v) {
							p.x += v.x;
							p.y += v.y;
						})
				.Run();

		// Consume the velocity
		GetWorld()
				.ForEach(
						m_q,
						[&](Velocity& v) {
							v.x = 0;
							v.y = 0;
						})
				.Run();
	}
};

class HandleDamageSystem final: public ecs::System {
	CollisionSystem* m_collisionSystem;

public:
	void OnCreated() override {
		m_collisionSystem = s_sm.FindSystem<CollisionSystem>();
	}

	void OnUpdate() override {
		const auto& colls = m_collisionSystem->GetCollisions();
		for (const auto& coll: colls) {
			auto e = coll.e;
			auto e2 = coll.e2;

			// run into something damagable
			if (!GetWorld().HasComponents<Health>(e2))
				continue;

			BattleStats stats = {0, 0};
			BattleStats stats2 = {0, 0};

			if (!GetWorld().HasComponents<BattleStats>(e))
				return;
			if (!GetWorld().HasComponents<BattleStats>(e2))
				return;

			GetWorld().GetComponent<BattleStats>(e, stats);
			GetWorld().GetComponent<BattleStats>(e2, stats2);

			int damage = stats.power - stats2.armor;
			if (damage < 0)
				continue;

			Health h2;
			GetWorld().GetComponent<Health>(e2, h2);
			GetWorld().SetComponent<Health>(e2, {h2.value - damage, h2.valueMax});
		}
	}

	bool DependsOn([[maybe_unused]] const BaseSystem* system) const override {
		return system == s_sm.FindSystem<CollisionSystem>();
	}
};

class HandleItemHitSystem final: public ecs::System {
	CollisionSystem* m_collisionSystem;

public:
	void OnCreated() override {
		m_collisionSystem = s_sm.FindSystem<CollisionSystem>();
	}

	void OnUpdate() override {
		const auto& colls = m_collisionSystem->GetCollisions();
		for (const auto& coll: colls) {
			if (!GetWorld().HasComponents<Item>(coll.e2))
				continue;

			if (s_world.map[coll.p.y][coll.p.x] == TILE_POISON) {
				GetWorld()
						.ForEach([&]([[maybe_unused]] const Player& p, Health& h) {
							h.value -= 10;
						})
						.Run();
			}

			if (s_world.map[coll.p.y][coll.p.x] == TILE_POTION) {
				GetWorld()
						.ForEach([&]([[maybe_unused]] const Player& p, Health& h) {
							h.value += 10;
							if (h.value > h.valueMax)
								h.value = h.valueMax;
						})
						.Run();
			}
		}
	}

	bool DependsOn([[maybe_unused]] const BaseSystem* system) const override {
		return system == s_sm.FindSystem<CollisionSystem>();
	}
};

class HandleDeadSystem final: public ecs::System {
	ecs::EntityQuery m_q;
	ecs::CommandBuffer m_cb;

public:
	void OnCreated() override {
		m_q.All<Health, Position>();
	}
	void OnUpdate() override {
		GetWorld()
				.ForEach(
						m_q,
						[&](ecs::Entity e, const Health& h, const Position& p) {
							if (h.value > 0)
								return;

							if (GetWorld().HasComponents<Player>(e))
								printf("Player killed!\n");
							else
								printf("Enemy killed %d:%d!\n", e.gen(), e.id());

							s_world.map[p.y][p.x] = TILE_FREE;
							m_cb.DeleteEntity(e);
						})
				.Run();

		m_cb.Commit(&GetWorld());
	}
};

class WriteSpritesToMapSystem final: public ecs::System {
	ecs::EntityQuery m_q;

public:
	void OnCreated() override {
		m_q.All<Position, Sprite>();
	}
	void OnUpdate() override {
		ClearScreen();
		s_world.InitWorldMap();
		GetWorld()
				.ForEach(
						m_q, [&](const Position& p,
										 const Sprite& s) { s_world.map[p.y][p.x] = s.value; })
				.Run();
	}
};

class RenderSystem final: public ecs::System {
public:
	void OnUpdate() override {
		for (int y = 0; y < ScreenY; y++) {
			for (int x = 0; x < ScreenX; x++) {
				putchar(s_world.map[y][x]);
			}
			printf("\n");
		}
	}
};

class UISystem final: public ecs::System {
public:
	void OnUpdate() override {
		ecs::EntityQuery qp;
		qp.All<Health, Player>();
		GetWorld()
				.ForEach(
						qp,
						[](const Health& h) {
							printf("Player health: %d/%d\n", h.value, h.valueMax);
						})
				.Run();

		ecs::EntityQuery qe;
		qe.All<Health>().None<Player>();
		GetWorld()
				.ForEach(
						qe,
						[](ecs::Entity e, const Health& h) {
							printf(
									"Enemy %d:%d health: %d/%d\n", e.id(), e.gen(), h.value,
									h.valueMax);
						})
				.Run();
	}
};

class InputSystem final: public ecs::System {
	ecs::EntityQuery m_q;
	char m_key;

public:
	void OnCreated() override {
		m_q.All<Player>();
	}
	void OnUpdate() override {
		m_key = _getch();

		GetWorld()
				.ForEach(
						m_q,
						[&](const ecs::Entity e) {
							if (GetWorld().HasComponents<Velocity>(e)) {
								if (m_key == 'w') {
									GetWorld().SetComponent<Velocity>(e, {0, -1});
								} else if (m_key == 's') {
									GetWorld().SetComponent<Velocity>(e, {0, 1});
								} else if (m_key == 'a') {
									GetWorld().SetComponent<Velocity>(e, {-1, 0});
								} else if (m_key == 'd') {
									GetWorld().SetComponent<Velocity>(e, {1, 0});
								}
							}
						})
				.Run();
	}
};

int main() {
	s_sm.CreateSystem<UpdateMapSystem>("updateblocked");
	s_sm.CreateSystem<CollisionSystem>("collision");
	s_sm.CreateSystem<HandleDamageSystem>("damagesystem");
	s_sm.CreateSystem<HandleItemHitSystem>("itemhitsystem");
	s_sm.CreateSystem<MoveSystem>("move");
	s_sm.CreateSystem<WriteSpritesToMapSystem>("spritestomap");
	s_sm.CreateSystem<RenderSystem>("render");
	s_sm.CreateSystem<UISystem>("ui");
	s_sm.CreateSystem<HandleDeadSystem>("handledead");
	s_sm.CreateSystem<InputSystem>("input");

	s_world.Init();
	for (;;)
		s_sm.Update();

	return 0;
}