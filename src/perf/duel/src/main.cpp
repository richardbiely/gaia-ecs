#define _ITERATOR_DEBUG_LEVEL 0
#include <gaia.h>

GAIA_INIT

#include <benchmark/benchmark.h>
#if GAIA_COMPILER_MSVC
	#if _MSV_VER <= 1916
// warning C4100: 'XYZ': unreferenced formal parameter
GAIA_MSVC_WARNING_DISABLE(4100)
// warning C4307: 'XYZ': integral constant overflow
GAIA_MSVC_WARNING_DISABLE(4307)
	#endif
#endif

#if GAIA_ARCH != GAIA_ARCH_ARM
	#include <immintrin.h>
#else
	#include <arm_neon.h>
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

constexpr uint32_t N = 32'000; // kept a multiple of 32 to keep it simple even for SIMD code
constexpr float MinDelta = 0.01f;
constexpr float MaxDelta = 0.033f;

float CalculateDelta(benchmark::State& state) {
	state.PauseTiming();
	const float d = static_cast<float>((double)RAND_MAX / (MaxDelta - MinDelta));
	float delta = MinDelta + (static_cast<float>(rand()) / d);
	state.ResumeTiming();
	return delta;
}

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
		dt = CalculateDelta(state);

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
		w.ForEach(queryDynamic, [&](Velocity& v) {
			 v.y += 9.81f * dt;
		 }).Run();
	}
}

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
			GetWorld()
					.ForEach([&](Velocity& v) {
						v.y += 9.81f * dt;
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
		dt = CalculateDelta(state);

		sm.Update();
	}
}

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
		dt = CalculateDelta(state);

		sm.Update();
	}
}

void BM_Game_ECS_WithSystems_ForEachChunk_SoA(benchmark::State& state) {
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

								auto ppx = p.set<0>();
								auto ppy = p.set<1>();
								auto ppz = p.set<2>();

								auto vvx = v.get<0>();
								auto vvy = v.get<1>();
								auto vvz = v.get<2>();

								////////////////////////////////////////////////////////////////////
								// This is the code we'd like to run. However, not all compilers are
								// as smart as Clang so they wouldn't be able to vectorize even though
								// the oportunity is screaming.
								////////////////////////////////////////////////////////////////////
								// for (auto i = 0U; i < ch.GetItemCount(); ++i)
								// 	ppx[i] += vvx[i] * dt;
								// for (auto i = 0U; i < ch.GetItemCount(); ++i)
								// 	ppy[i] += vvy[i] * dt;
								// for (auto i = 0U; i < ch.GetItemCount(); ++i)
								// 	ppz[i] += vvz[i] * dt;
								////////////////////////////////////////////////////////////////////

								auto exec = [](float* GAIA_RESTRICT p, const float* GAIA_RESTRICT v, const size_t sz) {
									for (size_t i = 0U; i < sz; ++i)
										p[i] += v[i] * dt;
								};

								exec(ppx.data(), vvx.data(), ch.GetItemCount());
								exec(ppy.data(), vvy.data(), ch.GetItemCount());
								exec(ppz.data(), vvz.data(), ch.GetItemCount());
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

								auto ppy = p.set<1>();
								auto vvy = v.set<1>();

								////////////////////////////////////////////////////////////////////
								// This is the code we'd like to run. However, not all compilers are
								// as smart as Clang so they wouldn't be able to vectorize even though
								// the oportunity is screaming.
								////////////////////////////////////////////////////////////////////
								// for (auto i = 0U; i < ch.GetItemCount(); ++i) {
								// 	 if (ppy[i] < 0.0f) {
								// 		 ppy[i] = 0.0f;
								// 		 vvy[i] = 0.0f;
								//   }
								// }
								////////////////////////////////////////////////////////////////////

								auto exec = [](float* GAIA_RESTRICT p, float* GAIA_RESTRICT v, const size_t sz) {
									for (auto i = 0U; i < sz; ++i) {
										if (p[i] < 0.0f) {
											p[i] = 0.0f;
											v[i] = 0.0f;
										}
									}
								};

								exec(ppy.data(), vvy.data(), ch.GetItemCount());
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
								auto vvy = v.set<1>();

								////////////////////////////////////////////////////////////////////
								// This is the code we'd like to run. However, not all compilers are
								// as smart as Clang so they wouldn't be able to vectorize even though
								// the oportunity is screaming.
								////////////////////////////////////////////////////////////////////
								// for (auto i = 0U; i < ch.GetItemCount(); ++i)
								// 	vvy[i] = vvy[i] * dt * 9.81f;
								////////////////////////////////////////////////////////////////////

								auto exec = [&](float* GAIA_RESTRICT v, const size_t sz) {
									for (size_t i = 0U; i < sz; ++i)
										v[i] *= dt * 9.81f;
								};

								exec(vvy.data(), ch.GetItemCount());
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
		dt = CalculateDelta(state);

		sm.Update();
	}
}

#if GAIA_ARCH == GAIA_ARCH_ARM
	#define vreinterpretq_m128_f32(x) (x)
	#define vreinterpretq_f32_m128(x) (x)
	#define vreinterpretq_s32_m128(x) vreinterpretq_s32_f32(x)
	#define vreinterpretq_m128_u32(x) vreinterpretq_f32_u32(x)

using __m128 = float32x4_t;

__m128 _mm_load_ps(const float* p) {
	return vreinterpretq_m128_f32(vld1q_f32(p));
}

void _mm_store_ps(float* p, __m128 a) {
	vst1q_f32(p, vreinterpretq_f32_m128(a));
}

__m128 _mm_set_ps1(float a) {
	return vdupq_n_f32(a);
}

__m128 _mm_setzero_ps() {
	return vdupq_n_f32(0.0f);
}

__m128 _mm_fmadd_ps(__m128 a, __m128 b, __m128 c) {
	return vfmaq_f32(a, b, c);
}

__m128 _mm_cmplt_ps(__m128 a, __m128 b) {
	return vreinterpretq_m128_u32(vcltq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b)));
}

__m128 _mm_mul_ps(__m128 a, __m128 b) {
	return vreinterpretq_m128_f32(vmulq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b)));
}

__m128 _mm_blendv_ps(__m128 a, __m128 b, __m128 mask) {
	// Use a signed shift right to create a mask with the sign bit
	const auto res_mask = vreinterpretq_u32_s32(vshrq_n_s32(vreinterpretq_s32_m128(mask), 31));
	const auto res_a = vreinterpretq_f32_m128(a);
	const auto res_b = vreinterpretq_f32_m128(b);
	return vreinterpretq_m128_f32(vbslq_f32(res_mask, res_b, res_a));
}
#endif

void BM_Game_ECS_WithSystems_ForEachChunk_SoA_ManualSIMD(benchmark::State& state) {
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

								auto ppx = p.set<0>();
								auto ppy = p.set<1>();
								auto ppz = p.set<2>();

								auto vvx = v.get<0>();
								auto vvy = v.get<1>();
								auto vvz = v.get<2>();

								const auto size = ch.GetItemCount();
								const auto dtVec = _mm_set_ps1(dt);

								auto exec = [&](float* GAIA_RESTRICT p, const float* GAIA_RESTRICT v, const size_t offset) {
									const auto pVec = _mm_load_ps(p + offset);
									const auto vVec = _mm_load_ps(v + offset);
									const auto respVec = _mm_fmadd_ps(vVec, dtVec, pVec);
									_mm_store_ps(p + offset, respVec);
								};

								size_t i;
								// Optimize via "double-read" trick to hide latencies.
								// Item count is always a multiple of 4 for chunks with SoA components.
								for (i = 0; i < size; i += 8) {
									exec(ppx.data(), vvx.data(), i);
									exec(ppx.data(), vvx.data(), i + 4);
								}
								for (; i < size; i += 4)
									exec(ppx.data(), vvx.data(), i);

								for (i = 0; i < size; i += 8) {
									exec(ppy.data(), vvy.data(), i);
									exec(ppy.data(), vvy.data(), i + 4);
								}
								for (; i < size; i += 4)
									exec(ppy.data(), vvy.data(), i);

								for (i = 0; i < size; i += 8) {
									exec(ppz.data(), vvz.data(), i);
									exec(ppz.data(), vvz.data(), i + 4);
								}
								for (; i < size; i += 4)
									exec(ppz.data(), vvz.data(), i);
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

								auto ppy = p.set<1>();
								auto vvy = v.set<1>();
								const auto size = ch.GetItemCount();

								auto exec = [&](float* GAIA_RESTRICT p, float* GAIA_RESTRICT v, const size_t offset) {
									const auto vyVec = _mm_load_ps(v + offset);
									const auto pyVec = _mm_load_ps(p + offset);

									const auto condVec = _mm_cmplt_ps(vyVec, _mm_setzero_ps());
									const auto res_vyVec = _mm_blendv_ps(vyVec, _mm_setzero_ps(), condVec);
									const auto res_pyVec = _mm_blendv_ps(pyVec, _mm_setzero_ps(), condVec);

									_mm_store_ps(v + offset, res_vyVec);
									_mm_store_ps(p + offset, res_pyVec);
								};

								size_t i;
								// Optimize via "double-read" trick to hide latencies.
								// Item count is always a multiple of 4 for chunks with SoA components.
								for (i = 0; i < size; i += 8) {
									exec(ppy.data(), vvy.data(), i);
									exec(ppy.data(), vvy.data(), i + 4);
								}
								for (; i < size; i += 4)
									exec(ppy.data(), vvy.data(), i);
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

								auto vvy = v.set<1>();
								const auto size = ch.GetItemCount();

								const auto gg_dtVec = _mm_set_ps1(9.81f * dt);

								auto exec = [&](float* GAIA_RESTRICT v, const size_t offset) {
									const auto vyVec = _mm_load_ps(vvy.data() + offset);
									const auto mulVec = _mm_mul_ps(vyVec, gg_dtVec);
									_mm_store_ps(v + offset, mulVec);
								};

								size_t i;
								// Optimize via "double-read" trick to hide latencies.
								// Item count is always a multiple of 4 for chunks with SoA components.
								for (i = 0; i < size; i += 8) {
									exec(vvy.data(), i);
									exec(vvy.data(), i + 4);
								}
								for (; i < size; i += 4)
									exec(vvy.data(), i);
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
		dt = CalculateDelta(state);

		sm.Update();
	}
}

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
	containers::darray<IUnit*> units(N * 2);
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
		dt = CalculateDelta(state);

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

void BM_Game_NonECS_BetterMemoryLayout(benchmark::State& state) {
	struct UnitStatic {
		Position p;
		Rotation r;
		Scale s;

		void updatePosition([[maybe_unused]] float deltaTime) {}
		void handleGroundCollision([[maybe_unused]] float deltaTime) {}
		void applyGravity([[maybe_unused]] float deltaTime) {}
	};

	struct UnitDynamic {
		Position p;
		Rotation r;
		Scale s;
		Velocity v;

		void updatePosition(float deltaTime) {
			p.x += v.x * deltaTime;
			p.y += v.y * deltaTime;
			p.z += v.z * deltaTime;
		}
		void handleGroundCollision([[maybe_unused]] float deltaTime) {
			if (p.y < 0.0f) {
				p.y = 0.0f;
				v.y = 0.0f;
			}
		}
		void applyGravity(float deltaTime) {
			v.y += 9.81f * deltaTime;
		}
	};

	// Create entities.
	containers::darray<UnitStatic> units_static(N);
	for (uint32_t i = 0U; i < N; i++) {
		UnitStatic u;
		u.p = {0, 100, 0};
		u.r = {1, 2, 3, 4};
		u.s = {1, 1, 1};
		units_static[i] = std::move(u);
	}

	containers::darray<UnitDynamic> units_dynamic(N);
	for (uint32_t i = 0U; i < N; i++) {
		UnitDynamic u;
		u.p = {0, 100, 0};
		u.v = {0, 0, 1};
		u.r = {1, 2, 3, 4};
		u.s = {1, 1, 1};
		units_dynamic[i] = std::move(u);
	}

	srand(0);
	for ([[maybe_unused]] auto _: state) {
		dt = CalculateDelta(state);

		// Process entities
		for (auto& u: units_static) {
			u.updatePosition(dt);
			u.handleGroundCollision(dt);
			u.applyGravity(dt);
		}
		for (auto& u: units_dynamic) {
			u.updatePosition(dt);
			u.handleGroundCollision(dt);
			u.applyGravity(dt);
		}
	}
}

void BM_Game_NonECS_DOD(benchmark::State& state) {

	struct UnitDynamic {
		static void
		updatePosition(containers::darray<Position>& p, const containers::darray<Velocity>& v, float deltaTime) {
			const uint32_t size = p.size();
			for (uint32_t i = 0U; i < size; i++) {
				p[i].x += v[i].x * deltaTime;
				p[i].y += v[i].y * deltaTime;
				p[i].z += v[i].z * deltaTime;
			}
		}
		static void handleGroundCollision(containers::darray<Position>& p, containers::darray<Velocity>& v) {
			const uint32_t size = p.size();
			for (uint32_t i = 0U; i < size; i++) {
				if (p[i].y < 0.0f) {
					p[i].y = 0.0f;
					v[i].y = 0.0f;
				}
			}
		}
		static void applyGravity(containers::darray<Velocity>& v, float deltaTime) {
			const uint32_t size = v.size();
			for (uint32_t i = 0U; i < size; i++) {
				v[i].y += 9.81f * deltaTime;
			}
		}
	};

	// Create static entities.
	containers::darray<Position> units_static_p(N);
	containers::darray<Rotation> units_static_r(N);
	containers::darray<Scale> units_static_s(N);
	for (uint32_t i = 0U; i < N; i++) {
		units_static_p[i] = {0, 100, 0};
		units_static_r[i] = {1, 2, 3, 4};
		units_static_s[i] = {1, 1, 1};
	}

	// Create dynamic entities.
	containers::darray<Position> units_dynamic_p(N);
	containers::darray<Rotation> units_dynamic_r(N);
	containers::darray<Scale> units_dynamic_s(N);
	containers::darray<Velocity> units_dynamic_v(N);
	for (uint32_t i = 0U; i < N; i++) {
		units_dynamic_p[i] = {0, 100, 0};
		units_dynamic_r[i] = {1, 2, 3, 4};
		units_dynamic_s[i] = {1, 1, 1};
		units_dynamic_v[i] = {0, 0, 1};
	}

	srand(0);
	for ([[maybe_unused]] auto _: state) {
		dt = CalculateDelta(state);

		// Process static entities
		UnitDynamic::updatePosition(units_dynamic_p, units_dynamic_v, dt);
		UnitDynamic::handleGroundCollision(units_dynamic_p, units_dynamic_v);
		UnitDynamic::applyGravity(units_dynamic_v, dt);
	}
}

void BM_Game_NonECS_DOD_SoA(benchmark::State& state) {

	struct UnitDynamic {
		static void updatePosition(containers::darray<PositionSoA>& p, const containers::darray<VelocitySoA>& v) {
			gaia::utils::auto_view_policy_set<PositionSoA> pv(std::span(p.data(), p.size()));
			gaia::utils::auto_view_policy_get<VelocitySoA> vv(std::span(v.data(), v.size()));

			auto ppx = pv.set<0>();
			auto ppy = pv.set<1>();
			auto ppz = pv.set<2>();

			auto vvx = vv.get<0>();
			auto vvy = vv.get<1>();
			auto vvz = vv.get<2>();

			////////////////////////////////////////////////////////////////////
			// This is the code we'd like to run. However, not all compilers are
			// as smart as Clang so they wouldn't be able to vectorize even though
			// the oportunity is screaming.
			////////////////////////////////////////////////////////////////////
			// for (auto i = 0U; i < ch.GetItemCount(); ++i)
			// 	ppx[i] += vvx[i] * dt;
			// for (auto i = 0U; i < ch.GetItemCount(); ++i)
			// 	ppy[i] += vvy[i] * dt;
			// for (auto i = 0U; i < ch.GetItemCount(); ++i)
			// 	ppz[i] += vvz[i] * dt;
			////////////////////////////////////////////////////////////////////

			auto exec = [](float* GAIA_RESTRICT p, const float* GAIA_RESTRICT v, size_t sz) {
				for (size_t i = 0U; i < sz; ++i)
					p[i] += v[i] * dt;
			};

			exec(ppx.data(), vvx.data(), p.size());
			exec(ppy.data(), vvy.data(), p.size());
			exec(ppz.data(), vvz.data(), p.size());
		}

		static void handleGroundCollision(containers::darray<PositionSoA>& p, containers::darray<VelocitySoA>& v) {
			gaia::utils::auto_view_policy_set<PositionSoA> pv(std::span(p.data(), p.size()));
			gaia::utils::auto_view_policy_set<VelocitySoA> vv(std::span(v.data(), v.size()));

			auto ppy = pv.set<1>();
			auto vvy = vv.set<1>();

			auto exec = [](float* GAIA_RESTRICT p, float* GAIA_RESTRICT v, size_t sz) {
				for (auto i = 0U; i < sz; ++i) {
					if (p[i] < 0.0f) {
						p[i] = 0.0f;
						v[i] = 0.0f;
					}
				}
			};

			exec(ppy.data(), vvy.data(), p.size());
		}

		static void applyGravity(containers::darray<VelocitySoA>& v) {
			gaia::utils::auto_view_policy_set<VelocitySoA> vv(std::span(v.data(), v.size()));

			auto vvy = vv.set<1>();

			auto exec = [&](float* GAIA_RESTRICT v, const size_t sz) {
				for (size_t i = 0U; i < sz; ++i)
					v[i] *= 9.81f * dt;
			};

			exec(vvy.data(), v.size());
		}
	};

	// Create static entities.
	containers::darray<PositionSoA> units_static_p(N);
	containers::darray<Rotation> units_static_r(N);
	containers::darray<Scale> units_static_s(N);
	for (uint32_t i = 0U; i < N; i++) {
		units_static_p[i] = {0, 100, 0};
		units_static_r[i] = {1, 2, 3, 4};
		units_static_s[i] = {1, 1, 1};
	}

	// Create dynamic entities.
	containers::darray<PositionSoA> units_dynamic_p(N);
	containers::darray<Rotation> units_dynamic_r(N);
	containers::darray<Scale> units_dynamic_s(N);
	containers::darray<VelocitySoA> units_dynamic_v(N);
	for (uint32_t i = 0U; i < N; i++) {
		units_dynamic_p[i] = {0, 100, 0};
		units_dynamic_r[i] = {1, 2, 3, 4};
		units_dynamic_s[i] = {1, 1, 1};
		units_dynamic_v[i] = {0, 0, 1};
	}

	srand(0);
	for ([[maybe_unused]] auto _: state) {
		dt = CalculateDelta(state);

		// Process static entities
		UnitDynamic::updatePosition(units_dynamic_p, units_dynamic_v);
		UnitDynamic::handleGroundCollision(units_dynamic_p, units_dynamic_v);
		UnitDynamic::applyGravity(units_dynamic_v);
	}
}

void BM_Game_NonECS_DOD_SoA_ManualSIMD(benchmark::State& state) {

	struct UnitDynamic {
		static void updatePosition(containers::darray<PositionSoA>& p, const containers::darray<VelocitySoA>& v) {
			gaia::utils::auto_view_policy_set<PositionSoA> pv(std::span(p.data(), p.size()));
			gaia::utils::auto_view_policy_get<VelocitySoA> vv(std::span(v.data(), v.size()));

			auto ppx = pv.set<0>();
			auto ppy = pv.set<1>();
			auto ppz = pv.set<2>();

			auto vvx = vv.get<0>();
			auto vvy = vv.get<1>();
			auto vvz = vv.get<2>();

			const auto size = p.size();
			const auto dtVec = _mm_set_ps1(dt);

			auto exec = [&](float* GAIA_RESTRICT p, const float* GAIA_RESTRICT v, const size_t offset) {
				const auto pVec = _mm_load_ps(p + offset);
				const auto vVec = _mm_load_ps(v + offset);
				const auto respVec = _mm_fmadd_ps(vVec, dtVec, pVec);
				_mm_store_ps(p + offset, respVec);
			};

			size_t i;
			// Optimize via "double-read" trick to hide latencies.
			// Item count is always a multiple of 4 for chunks with SoA components.
			for (i = 0; i < size; i += 8) {
				exec(ppx.data(), vvx.data(), i);
				exec(ppx.data(), vvx.data(), i + 4);
			}
			for (; i < size; i += 4)
				exec(ppx.data(), vvx.data(), i);

			for (i = 0; i < size; i += 8) {
				exec(ppy.data(), vvy.data(), i);
				exec(ppy.data(), vvy.data(), i + 4);
			}
			for (; i < size; i += 4)
				exec(ppy.data(), vvy.data(), i);

			for (i = 0; i < size; i += 8) {
				exec(ppz.data(), vvz.data(), i);
				exec(ppz.data(), vvz.data(), i + 4);
			}
			for (; i < size; i += 4)
				exec(ppz.data(), vvz.data(), i);
		}

		static void handleGroundCollision(containers::darray<PositionSoA>& p, containers::darray<VelocitySoA>& v) {
			gaia::utils::auto_view_policy_set<PositionSoA> pv(std::span(p.data(), p.size()));
			gaia::utils::auto_view_policy_set<VelocitySoA> vv(std::span(v.data(), v.size()));

			auto ppy = pv.set<1>();
			auto vvy = vv.set<1>();
			const auto size = p.size();

			auto exec = [&](float* GAIA_RESTRICT p, float* GAIA_RESTRICT v, const size_t offset) {
				const auto vyVec = _mm_load_ps(v + offset);
				const auto pyVec = _mm_load_ps(p + offset);

				const auto condVec = _mm_cmplt_ps(vyVec, _mm_setzero_ps());
				const auto res_vyVec = _mm_blendv_ps(vyVec, _mm_setzero_ps(), condVec);
				const auto res_pyVec = _mm_blendv_ps(pyVec, _mm_setzero_ps(), condVec);

				_mm_store_ps(v + offset, res_vyVec);
				_mm_store_ps(p + offset, res_pyVec);
			};

			size_t i;
			// Optimize via "double-read" trick to hide latencies.
			// Item count is always a multiple of 4 for chunks with SoA components.
			for (i = 0; i < size; i += 8) {
				exec(ppy.data(), vvy.data(), i);
				exec(ppy.data(), vvy.data(), i + 4);
			}
			for (; i < size; i += 4)
				exec(ppy.data(), vvy.data(), i);
		}

		static void applyGravity(containers::darray<VelocitySoA>& v) {
			gaia::utils::auto_view_policy_set<VelocitySoA> vv(std::span(v.data(), v.size()));

			auto vvy = vv.set<1>();
			const auto size = v.size();

			const auto gg_dtVec = _mm_set_ps1(9.81f * dt);

			auto exec = [&](float* GAIA_RESTRICT v, const size_t offset) {
				const auto vyVec = _mm_load_ps(vvy.data() + offset);
				const auto mulVec = _mm_mul_ps(vyVec, gg_dtVec);
				_mm_store_ps(v + offset, mulVec);
			};

			size_t i;
			// Optimize via "double-read" trick to hide latencies.
			// Item count is always a multiple of 4 for chunks with SoA components.
			for (i = 0; i < size; i += 8) {
				exec(vvy.data(), i);
				exec(vvy.data(), i + 4);
			}
			for (; i < size; i += 4)
				exec(vvy.data(), i);
		}
	};

	// Create static entities.
	containers::darray<PositionSoA> units_static_p(N);
	containers::darray<Rotation> units_static_r(N);
	containers::darray<Scale> units_static_s(N);
	for (uint32_t i = 0U; i < N; i++) {
		units_static_p[i] = {0, 100, 0};
		units_static_r[i] = {1, 2, 3, 4};
		units_static_s[i] = {1, 1, 1};
	}

	// Create dynamic entities.
	containers::darray<PositionSoA> units_dynamic_p(N);
	containers::darray<Rotation> units_dynamic_r(N);
	containers::darray<Scale> units_dynamic_s(N);
	containers::darray<VelocitySoA> units_dynamic_v(N);
	for (uint32_t i = 0U; i < N; i++) {
		units_dynamic_p[i] = {0, 100, 0};
		units_dynamic_r[i] = {1, 2, 3, 4};
		units_dynamic_s[i] = {1, 1, 1};
		units_dynamic_v[i] = {0, 0, 1};
	}

	srand(0);
	for ([[maybe_unused]] auto _: state) {
		dt = CalculateDelta(state);

		// Process static entities
		UnitDynamic::updatePosition(units_dynamic_p, units_dynamic_v);
		UnitDynamic::handleGroundCollision(units_dynamic_p, units_dynamic_v);
		UnitDynamic::applyGravity(units_dynamic_v);
	}
}

// Ordinary coding style.
BENCHMARK(BM_Game_NonECS);
// Ordinary coding style with optimized memory layout (imagine using custom allocators
// to keep things close and tidy in memory).
BENCHMARK(BM_Game_NonECS_BetterMemoryLayout);
// Memory organized in DoD style.
// Performance target BM_Game_ECS_WithSystems_ForEachChunk.
BENCHMARK(BM_Game_NonECS_DOD);
// Best possible performance with no manual optimization.
// Performance target for BM_Game_ECS_WithSystems_ForEachChunk_SoA.
BENCHMARK(BM_Game_NonECS_DOD_SoA);
// Best possible performance.
// Performance target for BM_Game_ECS_WithSystems_ForEachChunk_SoA_ManualSIMD.
BENCHMARK(BM_Game_NonECS_DOD_SoA_ManualSIMD);

BENCHMARK(BM_Game_ECS);
BENCHMARK(BM_Game_ECS_WithSystems);
BENCHMARK(BM_Game_ECS_WithSystems_ForEachChunk);
BENCHMARK(BM_Game_ECS_WithSystems_ForEachChunk_SoA);
BENCHMARK(BM_Game_ECS_WithSystems_ForEachChunk_SoA_ManualSIMD);

// Run the benchmark
BENCHMARK_MAIN();
