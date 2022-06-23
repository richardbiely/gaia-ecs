#include <gaia.h>

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
	});
}

int main() {
	ecs::World w;

	constexpr size_t N = 10'000;

	// Create entities with position and acceleration
	auto e = w.CreateEntity();
	w.AddComponent<Position>(e, {});
	w.AddComponent<Acceleration>(e, {0, 0, 1});
	for (size_t i = 1; i < N; i++) {
		[[maybe_unused]] auto newentity = w.CreateEntity(e);
	}

	// Record the original position
	auto p0 = w.GetComponent<Position>(e);

	// Move until a key is hit
	constexpr size_t GameLoops = 10'000;
	for (size_t i = 1U; i < GameLoops; i++) {
		float dt = 0.01f; // simulate 100 FPS
		MoveSystem(w, dt);
	}

	auto p1 = w.GetComponent<Position>(e);
	LOG_N("Entity 0 moved from [%.2f,%.2f,%.2f] to [%.2f,%.2f,%.2f]", p0.x, p0.y, p0.z, p1.x, p1.y, p1.z);

	return 0;
}
