#define _ITERATOR_DEBUG_LEVEL 0
#include <gaia.h>

GAIA_INIT

#include <benchmark/benchmark.h>
#if GAIA_COMPILER_MSVC
	#ifdef _WIN32
		#pragma comment(lib, "Shlwapi.lib")
		#ifdef _DEBUG
			#pragma comment(lib, "benchmarkd.lib")
		#else
			#pragma comment(lib, "benchmark.lib")
		#endif
	#endif
#endif

using namespace gaia;

struct Position {
	float x, y, z;
};
struct Acceleration {
	float x, y, z;
};
struct Rotation {
	float x, y, z, w;
};
struct Scale {
	float x, y, z;
};

constexpr uint32_t N = 1'000;
constexpr float MinDelta = 0.01f;
constexpr float MaxDelta = 0.033f;

void BM_SoA(benchmark::State& state) {
	ecs::World w;

	// Create static entities
	{
		auto e = w.CreateEntity();
		w.AddComponent<Position>(e, {});
		w.AddComponent<Rotation>(e, {1, 2, 3, 4});
		w.AddComponent<Scale>(e, {1, 1, 1});
		for (uint32_t i = 1U; i < N; i++) {
			[[maybe_unused]] auto newentity = w.CreateEntity(e);
		}
	}
	// Create dynamic entities
	{
		auto e = w.CreateEntity();
		w.AddComponent<Position>(e, {});
		w.AddComponent<Acceleration>(e, {0, 0, 1});
		w.AddComponent<Rotation>(e, {1, 2, 3, 4});
		w.AddComponent<Scale>(e, {1, 1, 1});
		for (uint32_t i = 1U; i < N; i++) {
			[[maybe_unused]] auto newentity = w.CreateEntity(e);
		}
	}

	// auto queryDynamic = ecs::EntityQuery().All<Position, Acceleration>();

	srand(0);
	for ([[maybe_unused]] auto _: state) {
		// Calculate delta
		state.PauseTiming();
		const float dt =
				MinDelta + static_cast<float>(rand()) /
											 (static_cast<float>(RAND_MAX / (MaxDelta - MinDelta)));
		state.ResumeTiming();

		// Process entities
		// w.ForEach(queryDynamic, [&](Position& p, const Acceleration& a) {
		w.ForEach([&](Position& p, const Acceleration& a) {
			 p.x += a.x * dt;
			 p.y += a.y * dt;
			 p.z += a.z * dt;
		 }).Run(0);
	}

	{
		const auto i = rand() % N;
		auto e = w.GetEntity(i);
		Position p;
		w.GetComponent<Position>(e, p);
		benchmark::DoNotOptimize(p.x);
	}
}
BENCHMARK(BM_SoA);

void BM_AoS_NoECS(benchmark::State& state) {
	struct IUnit {
		Position p;
		Rotation r;
		Scale s;

		IUnit() = default;
		virtual ~IUnit() = default;

		virtual void move(float dt) = 0;
	};

	struct UnitStatic: public IUnit {
		void move([[maybe_unused]] float dt) override {}
	};

	struct UnitDynamic: public IUnit {
		Acceleration a;
		void move(float dt) override {
			p.x += a.x * dt;
			p.y += a.y * dt;
			p.z += a.z * dt;
		}
	};

	// Create entities.
	// We allocate via new to simulate the usual kind of behavior in games
	std::vector<IUnit*> units(N * 2);
	for (uint32_t i = 0; i < N; i++) {
		auto u = new UnitStatic();
		u->p = {};
		u->r = {1, 2, 3, 4};
		u->s = {1, 1, 1};
		units[i] = u;
	}
	for (uint32_t i = 0; i < N; i++) {
		auto u = new UnitDynamic();
		u->p = {};
		u->a = {0, 0, 1};
		u->r = {1, 2, 3, 4};
		u->s = {1, 1, 1};
		units[N + i] = u;
	}

	srand(0);
	for ([[maybe_unused]] auto _: state) {
		// Calculate delta
		state.PauseTiming();
		const float dt =
				MinDelta + static_cast<float>(rand()) /
											 (static_cast<float>(RAND_MAX / (MaxDelta - MinDelta)));
		state.ResumeTiming();

		// Process entities
		for (auto& u: units) {
			u->move(dt);
		}
	}

	{
		const auto i = rand() % N;
		benchmark::DoNotOptimize(units[i]->p.x);
	}

	for (auto& u: units) {
		delete u;
	}
}
BENCHMARK(BM_AoS_NoECS);

// Run the benchmark
BENCHMARK_MAIN();
