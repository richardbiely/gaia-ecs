#include <gaia.h>

using namespace gaia;

struct Position {
	float x, y, z;
};
struct Acceleration {
	float x, y, z;
};

void MoveSystem(ecs::World& w, float dt) {
	auto q = w.query().all<Position&, Acceleration>();
	q.each([&](Position& p, const Acceleration& a) {
		p.x += a.x * dt;
		p.y += a.y * dt;
		p.z += a.z * dt;
	});
}

int main() {
	ecs::World w;

	constexpr uint32_t N = 1'500;

	// Create entities with position and acceleration
	auto e = w.add();
	w.add<Position>(e, {});
	w.add<Acceleration>(e, {0, 0, 1});
	GAIA_FOR(N) {
		[[maybe_unused]] auto newentity = w.copy(e);
	}

	// Record the original position
	auto p0 = w.get<Position>(e);

	// Move until a key is hit
	constexpr uint32_t GameLoops = 1'000;
	GAIA_FOR(GameLoops) {
		float dt = 0.01f; // simulate 100 FPS
		MoveSystem(w, dt);
	}

	auto p1 = w.get<Position>(e);
	GAIA_LOG_N("Entity 0 moved from [%.2f,%.2f,%.2f] to [%.2f,%.2f,%.2f]", p0.x, p0.y, p0.z, p1.x, p1.y, p1.z);

	return 0;
}
