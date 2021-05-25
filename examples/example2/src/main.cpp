#define _ITERATOR_DEBUG_LEVEL 0
#include <gaia.h>

GAIA_INIT

using namespace gaia;

struct Position {
	float x, y, z;
};
struct Acceleration {
	float x, y, z;
};

void MoveSystem(ecs::World& w, float dt) {
	w.ForEach([&](Position& p, const Acceleration& a) {
		 p.x += a.x * dt;
		 p.y += a.y * dt;
		 p.z += a.z * dt;
	 }).Run(0);
}

int main() {
	ecs::World w;

	constexpr uint32_t N = 10'000;

	// Create entities with position and acceleration
	auto e = w.CreateEntity();
	w.AddComponent<Position>(e, {});
	w.AddComponent<Acceleration>(e, {0, 0, 1});
	for (uint32_t i = 1U; i < N; i++) {
		[[maybe_unused]] auto newentity = w.CreateEntity(e);
	}

	// Record the orignal position
	Position p0;
	w.GetComponent<Position>(e, p0);

	// Move until a key is hit
	constexpr uint32_t GameLoops = 10'000;
	for (uint32_t i = 1U; i < GameLoops; i++) {
		float dt = 0.01f; // simulate 100 FPS
		MoveSystem(w, dt);
	}

	Position p1;
	w.GetComponent<Position>(e, p1);
	LOG_N(
			"Entity 0 moved from [%.2f,%.2f,%.2f] to [%.2f,%.2f,%.2f]", p0.x, p0.y,
			p0.z, p1.x, p1.y, p1.z);

	return 0;
}