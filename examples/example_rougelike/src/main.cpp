#ifdef WIN32
	#include <conio.h>
#else
	#include <termios.h>
	#include <unistd.h>
#endif
#include <cstdio>
#define _ITERATOR_DEBUG_LEVEL 0
#include <gaia.h>

GAIA_INIT

using namespace gaia;

#pragma region System

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
};
struct Item {};
struct Player {};

#pragma endregion

constexpr int ScreenX = 40;
constexpr int ScreenY = 20;
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

void RenderScreen() {
	for (int y = 0; y < ScreenY; y++) {
		for (int x = 0; x < ScreenX; x++) {
			putchar(map[y][x]);
		}
		printf("\n");
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
							if (map[p.y + v.y][p.x + v.x] == TILE_FREE)
								return;

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
		GetWorld()
				.ForEach(
						m_q,
						[&](Position& p, const Velocity& v) {
							p.x += v.x;
							p.y += v.y;
						})
				.Run();
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

class RenderSystem final: public ecs::System {
	ecs::EntityQuery m_q;

public:
	void OnCreated() override {
		m_q.All<Position, Sprite>();
	}
	void OnUpdate() override {
		InitWorldMap();
		GetWorld()
				.ForEach(
						m_q, [&](const Position& p,
										 const Sprite& s) { map[p.y][p.x] = s.value; })
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
	sm.CreateSystem<RenderSystem>("render");
	sm.CreateSystem<CollisionSystem>("collision");
	sm.CreateSystem<MoveSystem>("move");
	sm.CreateSystem<InputSystem>("input");

	auto player = w.CreateEntity();
	w.AddComponent<Position>(player, {5, 10});
	w.AddComponent<Velocity>(player, {0, 0});
	w.AddComponent<Sprite>(player, {TILE_PLAYER});
	w.AddComponent<Player>(player);

	auto enemy = w.CreateEntity();
	w.AddComponent<Position>(enemy, {10, 10});
	w.AddComponent<Velocity>(enemy, {0, 0});
	w.AddComponent<Sprite>(enemy, {TILE_ENEMY});

	auto potion = w.CreateEntity();
	w.AddComponent<Position>(potion, {5, 5});
	w.AddComponent<Sprite>(potion, {TILE_POTION});
	w.AddComponent<Item>(potion);

	auto poison = w.CreateEntity();
	w.AddComponent<Position>(poison, {15, 10});
	w.AddComponent<Sprite>(poison, {TILE_POISON});
	w.AddComponent<Item>(poison);

	InitWorldMap();

	for (;;) {
		sm.Update();

		RenderScreen();
	};

	return 0;
}