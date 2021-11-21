#include "gaia/ecs/command_buffer.h"
#include "gaia/ecs/entity.h"
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
struct Item {};
struct Player {};

#pragma endregion

constexpr int ScreenX = 40;
constexpr int ScreenY = 15;
char map[ScreenY][ScreenX] = {};

constexpr char TILE_WALL = 'O';
constexpr char TILE_FREE = ' ';
constexpr char TILE_PLAYER = 'P';
constexpr char TILE_ENEMY = 'E';
constexpr char TILE_POTION = 'i';
constexpr char TILE_POISON = 'p';

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

class CollisionSystem final: public ecs::System {
	ecs::EntityQuery m_q;

public:
	void OnCreated() override {
		m_q.All<Position, Velocity>();
	}
	void OnUpdate() override {
		GetWorld()
				.ForEach(
						m_q,
						[&](const Position& p, Velocity& v) {
							const int nx = p.x + v.x;
							const int ny = p.y + v.y;

							if (map[ny][nx] == TILE_FREE)
								return;

							if (v.x != 0 || v.y != 0) {
								// TODO: event system is necessary that would inform of
								// collisions
								if (map[ny][nx] == TILE_PLAYER) {
									// enemy run into player
									ecs::EntityQuery q;
									q.All<Health, Player>();
									GetWorld()
											.ForEach(
													q,
													[&](const Position& p, Health& h) {
														if (p.x != nx || p.y != ny)
															return;
														h.value -= 10;
													})
											.Run();
								}
								// TODO: event system is necessary that would inform of
								// collisions
								if (map[ny][nx] == TILE_ENEMY) {
									// player run into enemy
									ecs::EntityQuery q;
									q.All<Position, Health>().None<Player>();
									GetWorld()
											.ForEach(
													q,
													[&](const Position& p, Health& h) {
														if (p.x != nx || p.y != ny)
															return;
														h.value -= 10;
													})
											.Run();
								}
							}

							v.x = 0;
							v.y = 0;
						})
				.Run();
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

							map[p.y][p.x] = TILE_FREE;
							m_cb.DeleteEntity(e);
						})
				.Run();

		m_cb.Commit(&GetWorld());
	}
};

class UpdateMapSystem final: public ecs::System {
	ecs::EntityQuery m_q;

public:
	void OnCreated() override {
		m_q.All<Position, Sprite>();
	}
	void OnUpdate() override {
		ClearScreen();
		InitWorldMap();
		GetWorld()
				.ForEach(
						m_q, [&](const Position& p,
										 const Sprite& s) { map[p.y][p.x] = s.value; })
				.Run();
	}
};

class RenderSystem final: public ecs::System {
public:
	void OnUpdate() override {
		for (int y = 0; y < ScreenY; y++) {
			for (int x = 0; x < ScreenX; x++) {
				putchar(map[y][x]);
			}
			printf("\n");
		};

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
	ecs::World w;

	ecs::SystemManager sm(w);
	sm.CreateSystem<CollisionSystem>("collision");
	sm.CreateSystem<MoveSystem>("move");
	sm.CreateSystem<UpdateMapSystem>("updatemap");
	sm.CreateSystem<RenderSystem>("render");
	sm.CreateSystem<HandleDeadSystem>("handledead");
	sm.CreateSystem<InputSystem>("input");

	auto player = w.CreateEntity();
	w.AddComponent<Position>(player, {5, 10});
	w.AddComponent<Velocity>(player, {0, 0});
	w.AddComponent<Sprite>(player, {TILE_PLAYER});
	w.AddComponent<Health>(player, {100, 100});
	w.AddComponent<Player>(player);

	utils::array<ecs::Entity, 3> enemies;
	for (auto& e: enemies) {
		e = w.CreateEntity();
		w.AddComponent<Position>(e, {});
		w.AddComponent<Velocity>(e, {0, 0});
		w.AddComponent<Sprite>(e, {TILE_ENEMY});
		w.AddComponent<Health>(e, {100, 100});
	}
	w.SetComponent<Position>(enemies[0], {8, 8});
	w.SetComponent<Position>(enemies[1], {10, 10});
	w.SetComponent<Position>(enemies[2], {12, 12});

	auto potion = w.CreateEntity();
	w.AddComponent<Position>(potion, {5, 5});
	w.AddComponent<Sprite>(potion, {TILE_POTION});
	w.AddComponent<Item>(potion);

	auto poison = w.CreateEntity();
	w.AddComponent<Position>(poison, {15, 10});
	w.AddComponent<Sprite>(poison, {TILE_POISON});
	w.AddComponent<Item>(poison);

	for (;;) {
		sm.Update();
	};

	return 0;
}