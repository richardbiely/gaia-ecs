#ifdef _WIN32
	#include <conio.h>
#else
	#include <termios.h>
	#include <unistd.h>
	#include <term.h>
#endif
#include <cstdio>
#include <cstdlib>

#define _ITERATOR_DEBUG_LEVEL 0
#include <gaia.h>

GAIA_INIT

#if GAIA_COMPILER_MSVC
	#if _MSV_VER <= 1916
// warning C4100: 'XYZ': unreferenced formal parameter
GAIA_MSVC_WARNING_DISABLE(4100)
// warning C4307: 'XYZ': integral constant overflow
GAIA_MSVC_WARNING_DISABLE(4307)
	#endif
#endif

using namespace gaia;

//----------------------------------------------------------------------
// Platform-specific helpers
//----------------------------------------------------------------------

#ifndef _WIN32
char get_char() {
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
char get_char() {
	return (char)_getch();
}
void ClearScreen() {
	system("cls");
}
#endif

//----------------------------------------------------------------------
// Components
//----------------------------------------------------------------------

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

struct Orientation {
	int x, y;
};
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
enum ItemType { Poison, Potion, Arrow };
struct Item {
	ItemType type;
};
struct Player {};

//----------------------------------------------------------------------

constexpr int ScreenX = 40;
constexpr int ScreenY = 15;
constexpr char TILE_WALL = '#';
constexpr char TILE_FREE = ' ';
constexpr char TILE_PLAYER = 'P';
constexpr char TILE_ENEMY_GOBLIN = 'G';
constexpr char TILE_ENEMY_ORC = 'O';
constexpr char TILE_ARROW = '.';
constexpr char TILE_POTION = 'i';
constexpr char TILE_POISON = 'p';

struct World {
	ecs::World& w;
	//! map tiles
	char map[ScreenY][ScreenX];
	//! blocked tiles
	bool blocked[ScreenY][ScreenX];
	//! tile content
	containers::map<Position, containers::darray<ecs::Entity>> content;

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
		w.AddComponent<Orientation>(player, {1, 0});
		w.AddComponent<Sprite>(player, {TILE_PLAYER});
		w.AddComponent<Health>(player, {100, 100});
		w.AddComponent<BattleStats>(player, {9, 5});
		w.AddComponent<Player>(player);
	}

	void CreateEnemies() {
		containers::sarray<ecs::Entity, 3> enemies;
		for (size_t i = 0U; i < enemies.size(); ++i) {
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
		w.AddComponent<Item>(potion, {Potion});
		w.AddComponent<BattleStats>(potion, {10, 0});

		auto poison = w.CreateEntity();
		w.AddComponent<Position>(poison, {15, 10});
		w.AddComponent<Sprite>(poison, {TILE_POISON});
		w.AddComponent<Item>(poison, {Poison});
		w.AddComponent<BattleStats>(poison, {-10, 0});
	}

	void CreateArrow(Position p, Velocity v) {
		auto e = w.CreateEntity();
		w.AddComponent<Position>(e, std::move(p));
		w.AddComponent<Velocity>(e, std::move(v));
		w.AddComponent<Sprite>(e, {TILE_ARROW});
		w.AddComponent<Item>(e, {Arrow});
		w.AddComponent<BattleStats>(e, {10, 0});
		w.AddComponent<Health>(e, {1, 1});
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
		// Wall blocks
		for (int y = 0; y < ScreenY; y++)
			for (int x = 0; x < ScreenX; x++)
				s_world.blocked[y][x] = s_world.map[y][x] == TILE_WALL;

		// Everything with postion && velocity blocks
		GetWorld()
				.ForEach(
						m_q,
						[&](const Position& p) {
							s_world.blocked[p.y][p.x] = true;
						})
				.Run();

		// Everything with position is content
		s_world.content.clear();
		GetWorld()
				.ForEach([&](ecs::Entity e, const Position& p) {
					s_world.content[p].push_back(e);
				})
				.Run();
	}
};

struct WorldCollisionData {
	//! Entity colliding
	ecs::Entity e;
	//! Position of collision
	Position p;
	//! Velocity at the time of collision
	Velocity v;
};

struct CollisionData {
	//! Entity colliding
	ecs::Entity e1;
	//! Entity being collided with
	ecs::Entity e2;
	//! Position of collision
	Position p;
	//! Velocity at the time of collision
	Velocity v;
};

class CollisionSystem final: public ecs::System {
	ecs::EntityQuery m_q;
	containers::darray<CollisionData> m_colliding;

public:
	void OnCreated() override {
		m_q.All<Position, Velocity>();
	}

	void OnUpdate() override {
		// Drop all previous collision records
		m_colliding.clear();

		GetWorld()
				.ForEachChunk(
						m_q,
						[&](ecs::Chunk& chunk) {
							auto ent = chunk.View<ecs::Entity>();
							auto vel = chunk.View<Velocity>();
							auto pos = chunk.View<Position>();

							for (uint32_t i = 0; i < chunk.GetItemCount(); ++i) {
								auto& e = ent[i];
								auto& v = vel[i];
								auto& p = pos[i];

								// Skip stationary objects
								if (v.x == 0 && v.y == 0)
									return;

								const int nx = p.x + v.x;
								const int ny = p.y + v.y;

								if (!s_world.blocked[ny][nx])
									return;

								// Collect all collisions
								auto it = s_world.content.find({nx, ny});
								if (it == s_world.content.end()) {
									m_colliding.push_back(CollisionData{e, ecs::EntityNull, {nx, ny}, v});
								} else {
									for (auto e2: it->second)
										m_colliding.push_back(CollisionData{e, e2, {nx, ny}, v});
								}

								// No velocity after collision
								auto vel_mut = chunk.ViewRW<Velocity>();
								vel_mut[i] = {0, 0};
							}
						})
				.Run();
	}

	const containers::darray<CollisionData>& GetCollisions() const {
		return m_colliding;
	}
};

class MoveSystem final: public ecs::System {
	ecs::EntityQuery m_q1;
	ecs::EntityQuery m_q2;

public:
	void OnCreated() override {
		m_q1.All<Position, Velocity>();
		m_q2.All<Orientation, Velocity>().WithChanged<Velocity>();
	}

	void OnUpdate() override {
		// Update position based on current velocity
		GetWorld()
				.ForEach(
						m_q1,
						[&](Position& p, const Velocity& v) {
							p.x += v.x;
							p.y += v.y;
						})
				.Run();

		// Update orientation based on current velocity
		GetWorld()
				.ForEach(
						m_q2,
						[&](Orientation& o, const Velocity& v) {
							if (v.x != 0) {
								o.x = v.x > 0 ? 1 : -1;
								o.y = 0;
							}
							if (v.y != 0) {
								o.x = 0;
								o.y = v.y > 0 ? 1 : -1;
							}
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
			// Skip world collisions
			if (coll.e2 == ecs::EntityNull)
				continue;

			uint32_t idx1, idx2;
			auto* pChunk1 = GetWorld().GetEntityChunk(coll.e1, idx1);
			auto* pChunk2 = GetWorld().GetEntityChunk(coll.e2, idx2);

			// Skip non-damagable things
			if (!pChunk2->HasComponent<Health>())
				continue;
			if (!pChunk1->HasComponent<BattleStats>() || !pChunk2->HasComponent<BattleStats>())
				continue;

			// Verify if damage can be applied (e.g. power > armor)
			const auto stats1 = pChunk1->View<BattleStats>();
			const auto stats2 = pChunk2->View<BattleStats>();

			const int damage = stats1[idx1].power - stats2[idx2].armor;
			if (damage < 0)
				continue;

			// Apply damage
			auto health2 = pChunk2->ViewRW<Health>();
			health2[idx2].value -= damage;
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
			// World collision
			if (coll.e2 == ecs::EntityNull) {
				uint32_t idx1;
				auto* pChunk1 = GetWorld().GetEntityChunk(coll.e1, idx1);

				// An arrow colliding with something. Bring its health to 0 (destroyed).
				if (pChunk1->HasComponent<Item>() && pChunk1->HasComponent<Health>()) {
					auto item1 = pChunk1->View<Item>();
					if (item1[idx1].type == Arrow) {
						auto health1 = pChunk1->ViewRW<Health>();
						health1[idx1].value = 0;
					}
				}
			}
			// Entity-entity collision
			else {
				uint32_t idx1, idx2;
				auto* pChunk1 = GetWorld().GetEntityChunk(coll.e1, idx1);
				auto* pChunk2 = GetWorld().GetEntityChunk(coll.e2, idx2);

				// TODO: Add ability to get a list of components based on query

				// E.g. a player colliding with an item
				if (pChunk1->HasComponent<Health>() && pChunk2->HasComponent<Item>() && pChunk2->HasComponent<BattleStats>()) {
					auto health1 = pChunk1->ViewRW<Health>();
					auto stats2 = pChunk2->View<BattleStats>();

					// Apply the item's effect
					health1[idx1].value += stats2[idx2].power;
				}

				// An arrow colliding with something. Bring its health to 0 (destroyed).
				if (pChunk1->HasComponent<Item>() && pChunk1->HasComponent<Health>()) {
					auto item1 = pChunk1->View<Item>();
					if (item1[idx1].type == Arrow) {
						auto health1 = pChunk1->ViewRW<Health>();
						health1[idx1].value = 0;
					}
				}
			}
		}
	}

	bool DependsOn([[maybe_unused]] const BaseSystem* system) const override {
		return system == s_sm.FindSystem<CollisionSystem>();
	}
};

class HandleHealthSystem final: public ecs::System {
	ecs::EntityQuery m_q;

public:
	void OnCreated() override {
		m_q.All<Health>().WithChanged<Health>();
	}
	void OnUpdate() override {
		GetWorld()
				.ForEach(
						m_q,
						[&](Health& h) {
							if (h.value > h.valueMax)
								h.value = h.valueMax;
						})
				.Run();
	}
};

class HandleDeathSystem final: public ecs::System {
	ecs::EntityQuery m_q;

public:
	void OnCreated() override {
		m_q.All<Health, Position>().WithChanged<Health>();
	}
	void OnUpdate() override {
		GetWorld()
				.ForEach(
						m_q,
						[&](ecs::Entity e, const Health& h, const Position& p) {
							if (h.value > 0)
								return;

							s_world.map[p.y][p.x] = TILE_FREE;
							s_sm.AfterUpdateCmdBufer().DeleteEntity(e);
						})
				.Run();
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
						m_q,
						[&](const Position& p, const Sprite& s) {
							s_world.map[p.y][p.x] = s.value;
						})
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
		qe.All<Health>().None<Player, Item>();
		GetWorld()
				.ForEach(
						qe,
						[](ecs::Entity e, const Health& h) {
							printf("Enemy %d:%d health: %d/%d\n", e.id(), e.gen(), h.value, h.valueMax);
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
		m_key = get_char();

		// Player movement
		GetWorld()
				.ForEach(
						m_q,
						[&](Velocity& v, const Position& p, const Orientation& o) {
							v = {0, 0};
							if (m_key == 'w') {
								v = {0, -1};
							} else if (m_key == 's') {
								v = {0, 1};
							} else if (m_key == 'a') {
								v = {-1, 0};
							} else if (m_key == 'd') {
								v = {1, 0};
							} else if (m_key == 'q') {
								s_world.CreateArrow(p, {o.x, o.y});
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
	s_sm.CreateSystem<HandleHealthSystem>("handlehealth");
	s_sm.CreateSystem<HandleDeathSystem>("handledeath");
	s_sm.CreateSystem<MoveSystem>("move");
	s_sm.CreateSystem<WriteSpritesToMapSystem>("spritestomap");
	s_sm.CreateSystem<RenderSystem>("render");
	s_sm.CreateSystem<UISystem>("ui");
	s_sm.CreateSystem<InputSystem>("input");

	s_world.Init();
	for (;;)
		s_sm.Update();

	return 0;
}