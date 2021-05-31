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

float dt;

struct Position {
	float x, y, z;
};
struct Velocity {
	float x, y, z;
};
struct Rotation {
	float x, y, z, w;
};
struct Scale {
	float x, y, z;
};

constexpr uint32_t N = 10'000;
constexpr float MinDelta = 0.01f;
constexpr float MaxDelta = 0.033f;

void BM_Game_ECS(benchmark::State& state) {
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
		w.AddComponent<Position>(e, {0, 100, 0});
		w.AddComponent<Velocity>(e, {0, 0, 1});
		w.AddComponent<Rotation>(e, {1, 2, 3, 4});
		w.AddComponent<Scale>(e, {1, 1, 1});
		for (uint32_t i = 1U; i < N; i++) {
			[[maybe_unused]] auto newentity = w.CreateEntity(e);
		}
	}

	auto queryDynamic = ecs::EntityQuery().All<Position, Velocity>();

	srand(0);
	for ([[maybe_unused]] auto _: state) {
		// Calculate delta
		state.PauseTiming();
		dt = MinDelta + static_cast<float>(rand()) /
												(static_cast<float>(RAND_MAX / (MaxDelta - MinDelta)));
		state.ResumeTiming();

		// Update position
		w.ForEach(queryDynamic, [&](Position& p, const Velocity& v) {
			 p.x += v.x * dt;
			 p.y += v.y * dt;
			 p.z += v.z * dt;
		 }).Run();
		// Handle ground collision
		w.ForEach(queryDynamic, [&](Position& p, Velocity& v) {
			 if (p.y < 0.0f) {
				 p.y = 0.0f;
				 v.y = 0.0f;
			 }
		 }).Run();
		// Apply gravity
		w.ForEach(queryDynamic, [&](Velocity& v) { v.y += 9.81f * dt; }).Run();
	}
}
BENCHMARK(BM_Game_ECS);

void BM_Game_ECS_WithSystems(benchmark::State& state) {
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
		w.AddComponent<Position>(e, {0, 100, 0});
		w.AddComponent<Velocity>(e, {0, 0, 1});
		w.AddComponent<Rotation>(e, {1, 2, 3, 4});
		w.AddComponent<Scale>(e, {1, 1, 1});
		for (uint32_t i = 1U; i < N; i++) {
			[[maybe_unused]] auto newentity = w.CreateEntity(e);
		}
	}

	class PositionSystem final: public ecs::System {
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
								p.x += v.x * dt;
								p.y += v.y * dt;
								p.z += v.z * dt;
							})
					.Run();
		}
	};
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
							[&](Position& p, Velocity& v) {
								if (p.y < 0.0f) {
									p.y = 0.0f;
									v.y = 0.0f;
								}
							})
					.Run();
		}
	};
	class GravitySystem final: public ecs::System {
	public:
		void OnUpdate() override {
			GetWorld().ForEach([&](Velocity& v) { v.y += 9.81f * dt; }).Run();
		}
	};

	ecs::SystemManager sm(w);
	sm.CreateSystem<PositionSystem>("position");
	sm.CreateSystem<CollisionSystem>("collision");
	sm.CreateSystem<GravitySystem>("gravity");

	srand(0);
	for ([[maybe_unused]] auto _: state) {
		// Calculate delta
		state.PauseTiming();
		dt = MinDelta + static_cast<float>(rand()) /
												(static_cast<float>(RAND_MAX / (MaxDelta - MinDelta)));
		state.ResumeTiming();

		sm.Update();
	}
}
BENCHMARK(BM_Game_ECS_WithSystems);

void BM_Game_NonECS(benchmark::State& state) {
	struct IUnit {
		Position p;
		Rotation r;
		Scale s;

		IUnit() = default;
		virtual ~IUnit() = default;

		virtual void updatePosition(float deltaTime) = 0;
		virtual void handleGroundCollision(float deltaTime) = 0;
		virtual void applyGravity(float deltaTime) = 0;
	};

	struct UnitStatic: public IUnit {
		void updatePosition([[maybe_unused]] float deltaTime) override {}
		void handleGroundCollision([[maybe_unused]] float deltaTime) override {}
		void applyGravity([[maybe_unused]] float deltaTime) override {}
	};

	struct UnitDynamic: public IUnit {
		Velocity v;
		void updatePosition(float deltaTime) override {
			p.x += v.x * deltaTime;
			p.y += v.y * deltaTime;
			p.z += v.z * deltaTime;
		}
		void handleGroundCollision([[maybe_unused]] float deltaTime) override {
			if (p.y < 0.0f) {
				p.y = 0.0f;
				v.y = 0.0f;
			}
		}
		void applyGravity(float deltaTime) override {
			v.y += 9.81f * deltaTime;
		}
	};

	// Create entities.
	// We allocate via new to simulate the usual kind of behavior in games
	std::vector<IUnit*> units(N * 2);
	for (uint32_t i = 0U; i < N; i++) {
		auto u = new UnitStatic();
		u->p = {0, 100, 0};
		u->r = {1, 2, 3, 4};
		u->s = {1, 1, 1};
		units[i] = u;
	}
	for (uint32_t i = 0U; i < N; i++) {
		auto u = new UnitDynamic();
		u->p = {0, 100, 0};
		u->v = {0, 0, 1};
		u->r = {1, 2, 3, 4};
		u->s = {1, 1, 1};
		units[N + i] = u;
	}

	srand(0);
	for ([[maybe_unused]] auto _: state) {
		// Calculate delta
		state.PauseTiming();
		dt = MinDelta + static_cast<float>(rand()) /
												(static_cast<float>(RAND_MAX / (MaxDelta - MinDelta)));
		state.ResumeTiming();

		// Process entities
		for (auto& u: units) {
			u->updatePosition(dt);
			u->handleGroundCollision(dt);
			u->applyGravity(dt);
		}
	}

	for (auto& u: units) {
		delete u;
	}
}
BENCHMARK(BM_Game_NonECS);

// Run the benchmark
BENCHMARK_MAIN();
