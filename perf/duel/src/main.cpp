#define _ITERATOR_DEBUG_LEVEL 0
#include <gaia.h>

#include <immintrin.h>

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

	#if _MSV_VER <= 1916
// warning C4100: 'XYZ': unreferenced formal parameter
GAIA_MSVC_WARNING_DISABLE(4100)
// warning C4307: 'XYZ': integral constant overflow
GAIA_MSVC_WARNING_DISABLE(4307)
	#endif
#endif

using namespace gaia;

float dt;

struct Position {
	float x, y, z;
};
struct PositionSoA {
	float x, y, z;
	static constexpr auto Layout = utils::DataLayout::SoA;
};
struct Velocity {
	float x, y, z;
};
struct VelocitySoA {
	float x, y, z;
	static constexpr auto Layout = utils::DataLayout::SoA;
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

void BM_Game_ECS_WithSystems_ForEachChunk(benchmark::State& state) {
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
					.ForEachChunk(
							m_q,
							[&](ecs::Chunk& ch) {
								auto p = ch.ViewRW<Position>();
								auto v = ch.View<Velocity>();
								for (auto i = 0U; i < ch.GetItemCount(); ++i) {
									p[i].x += v[i].x * dt;
									p[i].y += v[i].y * dt;
									p[i].z += v[i].z * dt;
								}
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
					.ForEachChunk(
							m_q,
							[&](ecs::Chunk& ch) {
								auto p = ch.ViewRW<Position>();
								auto v = ch.ViewRW<Velocity>();
								for (auto i = 0U; i < ch.GetItemCount(); ++i) {
									if (p[i].y < 0.0f) {
										p[i].y = 0.0f;
										v[i].y = 0.0f;
									}
								}
							})
					.Run();
		}
	};
	class GravitySystem final: public ecs::System {
		ecs::EntityQuery m_q;

	public:
		void OnCreated() override {
			m_q.All<Velocity>();
		}

		void OnUpdate() override {
			GetWorld()
					.ForEachChunk(
							m_q,
							[&](ecs::Chunk& ch) {
								auto v = ch.ViewRW<Velocity>();
								for (auto i = 0U; i < ch.GetItemCount(); ++i) {
									v[i].y += 9.81f * dt;
								}
							})
					.Run();
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
BENCHMARK(BM_Game_ECS_WithSystems_ForEachChunk);

void BM_Game_ECS_WithSystems_ForEachChunkSoA(benchmark::State& state) {
	ecs::World w;

	// Create static entities
	{
		auto e = w.CreateEntity();
		w.AddComponent<PositionSoA>(e, {});
		w.AddComponent<Rotation>(e, {1, 2, 3, 4});
		w.AddComponent<Scale>(e, {1, 1, 1});
		for (uint32_t i = 1U; i < N; i++) {
			[[maybe_unused]] auto newentity = w.CreateEntity(e);
		}
	}
	// Create dynamic entities
	{
		auto e = w.CreateEntity();
		w.AddComponent<PositionSoA>(e, {0, 100, 0});
		w.AddComponent<VelocitySoA>(e, {0, 0, 1});
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
			m_q.All<PositionSoA, VelocitySoA>();
		}
		void OnUpdate() override {
			GetWorld()
					.ForEachChunk(
							m_q,
							[&](ecs::Chunk& ch) {
								auto p = ch.ViewRW<PositionSoA>();
								auto v = ch.View<VelocitySoA>();

								using pp = utils::auto_view_policy<PositionSoA>;
								using vv = utils::auto_view_policy<VelocitySoA>;

								auto ppx = pp::get<0>(p);
								auto ppy = pp::get<1>(p);
								auto ppz = pp::get<2>(p);

								auto vvx = vv::get<0>(v);
								auto vvy = vv::get<1>(v);
								auto vvz = vv::get<2>(v);

								const auto dtVec = _mm_set_ps1(dt);

								for (auto i = 0; i < ch.GetItemCount(); i += 8) {
									// We're memory starved so let's do two ticks in one
									// iteration to hide the latency
									{
										const auto pxVec = _mm_load_ps(ppx.data());
										const auto pyVec = _mm_load_ps(ppy.data());
										const auto pzVec = _mm_load_ps(ppz.data());

										const auto vxVec = _mm_load_ps(vvx.data());
										const auto vyVec = _mm_load_ps(vvy.data());
										const auto vzVec = _mm_load_ps(vvz.data());

										const auto vx_tVec = _mm_mul_ps(vxVec, dtVec);
										const auto vy_tVec = _mm_mul_ps(vyVec, dtVec);
										const auto vz_tVec = _mm_mul_ps(vzVec, dtVec);

										const auto resxVec = _mm_add_ps(vx_tVec, pxVec);
										const auto resyVec = _mm_add_ps(vy_tVec, pyVec);
										const auto reszVec = _mm_add_ps(vz_tVec, pzVec);

										_mm_store_ps((float*)ppx.data(), resxVec);
										_mm_store_ps((float*)ppy.data(), resyVec);
										_mm_store_ps((float*)ppz.data(), reszVec);
									}
									{
										const auto pxVec = _mm_load_ps(ppx.data() + 4);
										const auto pyVec = _mm_load_ps(ppy.data() + 4);
										const auto pzVec = _mm_load_ps(ppz.data() + 4);

										const auto vxVec = _mm_load_ps(vvx.data());
										const auto vyVec = _mm_load_ps(vvy.data());
										const auto vzVec = _mm_load_ps(vvz.data());

										const auto vx_tVec = _mm_mul_ps(vxVec, dtVec);
										const auto vy_tVec = _mm_mul_ps(vyVec, dtVec);
										const auto vz_tVec = _mm_mul_ps(vzVec, dtVec);

										const auto resxVec = _mm_add_ps(vx_tVec, pxVec);
										const auto resyVec = _mm_add_ps(vy_tVec, pyVec);
										const auto reszVec = _mm_add_ps(vz_tVec, pzVec);

										_mm_store_ps((float*)(ppx.data() + 4), resxVec);
										_mm_store_ps((float*)(ppy.data() + 4), resyVec);
										_mm_store_ps((float*)(ppz.data() + 4), reszVec);
									}
								}
							})
					.Run();
		}
	};
	class CollisionSystem final: public ecs::System {
		ecs::EntityQuery m_q;

	public:
		void OnCreated() override {
			m_q.All<PositionSoA, VelocitySoA>();
		}
		void OnUpdate() override {
			GetWorld()
					.ForEachChunk(
							m_q,
							[&](ecs::Chunk& ch) {
								auto p = ch.ViewRW<PositionSoA>();
								auto v = ch.ViewRW<VelocitySoA>();

								using pp = utils::auto_view_policy<PositionSoA>;
								using vv = utils::auto_view_policy<VelocitySoA>;

								auto ppy = pp::get<1>(p);
								auto vvy = vv::get<1>(v);

								for (auto i = 0; i < ch.GetItemCount(); i += 8) {
									// We're memory starved so let's do two ticks in one
									// iteration to hide the latency
									{
										const auto vyVec = _mm_load_ps(vvy.data());
										const auto pyVec = _mm_load_ps(ppy.data());

										const auto condVec = _mm_cmplt_ps(vyVec, _mm_setzero_ps());

										const auto res_vyVec =
												_mm_blendv_ps(vyVec, _mm_setzero_ps(), condVec);
										const auto res_pyVec =
												_mm_blendv_ps(pyVec, _mm_setzero_ps(), condVec);

										_mm_store_ps((float*)vvy.data(), res_vyVec);
										_mm_store_ps((float*)ppy.data(), res_pyVec);
									}
									{
										const auto vyVec = _mm_load_ps(vvy.data() + 4);
										const auto pyVec = _mm_load_ps(ppy.data() + 4);

										const auto condVec = _mm_cmplt_ps(vyVec, _mm_setzero_ps());

										const auto res_vyVec =
												_mm_blendv_ps(vyVec, _mm_setzero_ps(), condVec);
										const auto res_pyVec =
												_mm_blendv_ps(pyVec, _mm_setzero_ps(), condVec);

										_mm_store_ps((float*)(vvy.data() + 4), res_vyVec);
										_mm_store_ps((float*)(ppy.data() + 4), res_pyVec);
									}
								}
							})
					.Run();
		}
	};
	class GravitySystem final: public ecs::System {
		ecs::EntityQuery m_q;

	public:
		void OnCreated() override {
			m_q.All<VelocitySoA>();
		}

		void OnUpdate() override {
			GetWorld()
					.ForEachChunk(
							m_q,
							[&](ecs::Chunk& ch) {
								auto v = ch.ViewRW<VelocitySoA>();
								using vv = utils::auto_view_policy<VelocitySoA>;
								auto vvy = vv::get<1>(v);

								const auto gg_dtVec = _mm_set_ps1(9.81f * dt);

								for (auto i = 0; i < ch.GetItemCount(); i += 8) {
									// We're memory starved so let's do two ticks in one
									// iteration to hide the latency
									{
										const auto vyVec = _mm_load_ps(vvy.data());
										_mm_store_ps(
												(float*)vvy.data(), _mm_mul_ps(vyVec, gg_dtVec));
									}
									{
										const auto vyVec = _mm_load_ps(vvy.data() + 4);
										_mm_store_ps(
												(float*)(vvy.data() + 4), _mm_mul_ps(vyVec, gg_dtVec));
									}
								}
							})
					.Run();
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
BENCHMARK(BM_Game_ECS_WithSystems_ForEachChunkSoA);

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
