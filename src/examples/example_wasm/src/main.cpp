#include <emscripten.h>
#include <gaia.h>

using namespace gaia;

// How to test:
// 1) emcmake cmake -S . -B build-wasm
// 2) cmake --build build-wasm/src/examples/example_wasm -j
// 3) emrun --no_browser --port 8080 --serve_root build-wasm/src/examples/example_wasm
// or emrun --browser chrome --port 8080 --serve_root build-wasm/src/examples/example_wasm gaia_example_wasm.html
// 4) http://localhost:8080/gaia_example_wasm.html

struct Position {
	float x, y, z;
};
struct Acceleration {
	float x, y, z;
};

void MoveSystem(ecs::World& w, float dt) {
	auto q = w.query().all<Position&>().all<Acceleration>();
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
	w.add<Position>(e, {10,15,20});
	w.add<Acceleration>(e, {0, 0, 1});
	GAIA_FOR(N) {
		[[maybe_unused]] auto newEntity = w.copy(e);
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
