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
struct Direction {
	float x, y, z;
};
struct Health {
	int value;
};
struct IsEnemy {
	bool value;
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

template <bool SoA>
void CreateECSEntities_Static(ecs::World& w) {
	{
		auto e = w.CreateEntity();
		if constexpr (SoA)
			w.AddComponent<PositionSoA>(e, {0, 100, 0});
		else
			w.AddComponent<Position>(e, {0, 100, 0});
		w.AddComponent<Rotation>(e, {1, 2, 3, 4});
		w.AddComponent<Scale>(e, {1, 1, 1});
		for (uint32_t i = 0; i < N; i++) {
			[[maybe_unused]] auto newentity = w.CreateEntity(e);
		}
	}
}

template <bool SoA>
void CreateECSEntities_Dynamic(ecs::World& w) {
	{
		auto e = w.CreateEntity();
		if constexpr (SoA)
			w.AddComponent<PositionSoA>(e, {0, 100, 0});
		else
			w.AddComponent<Position>(e, {0, 100, 0});
		w.AddComponent<Rotation>(e, {1, 2, 3, 4});
		w.AddComponent<Scale>(e, {1, 1, 1});
		if constexpr (SoA)
			w.AddComponent<VelocitySoA>(e, {0, 0, 1});
		else
			w.AddComponent<Velocity>(e, {0, 0, 1});
		for (uint32_t i = 0; i < N / 4; i++) {
			[[maybe_unused]] auto newentity = w.CreateEntity(e);
		}
	}
	{
		auto e = w.CreateEntity();
		if constexpr (SoA)
			w.AddComponent<PositionSoA>(e, {0, 100, 0});
		else
			w.AddComponent<Position>(e, {0, 100, 0});
		w.AddComponent<Rotation>(e, {1, 2, 3, 4});
		w.AddComponent<Scale>(e, {1, 1, 1});
		if constexpr (SoA)
			w.AddComponent<VelocitySoA>(e, {0, 0, 1});
		else
			w.AddComponent<Velocity>(e, {0, 0, 1});
		w.AddComponent<Direction>(e, {0, 0, 1});
		for (uint32_t i = 0; i < N / 4; i++) {
			[[maybe_unused]] auto newentity = w.CreateEntity(e);
		}
	}
	{
		auto e = w.CreateEntity();
		if constexpr (SoA)
			w.AddComponent<PositionSoA>(e, {0, 100, 0});
		else
			w.AddComponent<Position>(e, {0, 100, 0});
		w.AddComponent<Rotation>(e, {1, 2, 3, 4});
		w.AddComponent<Scale>(e, {1, 1, 1});
		if constexpr (SoA)
			w.AddComponent<VelocitySoA>(e, {0, 0, 1});
		else
			w.AddComponent<Velocity>(e, {0, 0, 1});
		w.AddComponent<Direction>(e, {0, 0, 1});
		w.AddComponent<Health>(e, {100});
		for (uint32_t i = 0; i < N / 4; i++) {
			[[maybe_unused]] auto newentity = w.CreateEntity(e);
		}
	}
	{
		auto e = w.CreateEntity();
		if constexpr (SoA)
			w.AddComponent<PositionSoA>(e, {0, 100, 0});
		else
			w.AddComponent<Position>(e, {0, 100, 0});
		w.AddComponent<Rotation>(e, {1, 2, 3, 4});
		w.AddComponent<Scale>(e, {1, 1, 1});
		if constexpr (SoA)
			w.AddComponent<VelocitySoA>(e, {0, 0, 1});
		else
			w.AddComponent<Velocity>(e, {0, 0, 1});
		w.AddComponent<Direction>(e, {0, 0, 1});
		w.AddComponent<Health>(e, {100});
		w.AddComponent<IsEnemy>(e, {false});
		for (uint32_t i = 0; i < N / 4; i++) {
			[[maybe_unused]] auto newentity = w.CreateEntity(e);
		}
	}
}

void BM_Game_ECS(benchmark::State& state) {
	ecs::World w;
	CreateECSEntities_Static<false>(w);
	CreateECSEntities_Dynamic<false>(w);

	auto queryDynamic = ecs::EntityQuery().All<Position, Velocity>();

	srand(0);
	for ([[maybe_unused]] auto _: state) {
		dt = CalculateDelta(state);

		// Update position
		w.ForEach(queryDynamic, [&](Position& p, const Velocity& v) {
			p.x += v.x * dt;
			p.y += v.y * dt;
			p.z += v.z * dt;
		});
		// Handle ground collision
		w.ForEach(queryDynamic, [&](Position& p, Velocity& v) {
			if (p.y < 0.0f) {
				p.y = 0.0f;
				v.y = 0.0f;
			}
		});
		// Apply gravity
		w.ForEach(queryDynamic, [&](Velocity& v) {
			v.y += 9.81f * dt;
		});
	}
}

void BM_Game_ECS_WithSystems(benchmark::State& state) {
	ecs::World w;
	CreateECSEntities_Static<false>(w);
	CreateECSEntities_Dynamic<false>(w);

	class PositionSystem final: public ecs::System {
		ecs::EntityQuery m_q;

	public:
		void OnCreated() override {
			m_q.All<Position, Velocity>();
		}
		void OnUpdate() override {
			GetWorld().ForEach(m_q, [](Position& p, const Velocity& v) {
				p.x += v.x * dt;
				p.y += v.y * dt;
				p.z += v.z * dt;
			});
		}
	};
	class CollisionSystem final: public ecs::System {
		ecs::EntityQuery m_q;

	public:
		void OnCreated() override {
			m_q.All<Position, Velocity>();
		}
		void OnUpdate() override {
			GetWorld().ForEach(m_q, [](Position& p, Velocity& v) {
				if (p.y < 0.0f) {
					p.y = 0.0f;
					v.y = 0.0f;
				}
			});
		}
	};
	class GravitySystem final: public ecs::System {
	public:
		void OnUpdate() override {
			GetWorld().ForEach([](Velocity& v) {
				v.y += 9.81f * dt;
			});
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
	CreateECSEntities_Static<false>(w);
	CreateECSEntities_Dynamic<false>(w);

	class PositionSystem final: public ecs::System {
		ecs::EntityQuery m_q;

	public:
		void OnCreated() override {
			m_q.All<Position, Velocity>();
		}
		void OnUpdate() override {
			GetWorld().ForEachChunk(m_q, [](ecs::Chunk& ch) {
				auto p = ch.ViewRW<Position>();
				auto v = ch.View<Velocity>();

				[&](Position* GAIA_RESTRICT p, const Velocity* GAIA_RESTRICT v, const uint32_t size) {
					for (auto i = 0U; i < size; ++i) {
						p[i].x += v[i].x * dt;
						p[i].y += v[i].y * dt;
						p[i].z += v[i].z * dt;
					}
				}(p.data(), v.data(), ch.GetItemCount());
			});
		}
	};

	class CollisionSystem final: public ecs::System {
		ecs::EntityQuery m_q;

	public:
		void OnCreated() override {
			m_q.All<Position, Velocity>();
		}
		void OnUpdate() override {
			GetWorld().ForEachChunk(m_q, [](ecs::Chunk& ch) {
				auto p = ch.ViewRW<Position>();
				auto v = ch.ViewRW<Velocity>();

				[&](Position* GAIA_RESTRICT p, Velocity* GAIA_RESTRICT v, const uint32_t size) {
					for (auto i = 0U; i < size; ++i) {
						if (p[i].y < 0.0f) {
							p[i].y = 0.0f;
							v[i].y = 0.0f;
						}
					}
				}(p.data(), v.data(), ch.GetItemCount());
			});
		}
	};
	class GravitySystem final: public ecs::System {
		ecs::EntityQuery m_q;

	public:
		void OnCreated() override {
			m_q.All<Velocity>();
		}

		void OnUpdate() override {
			GetWorld().ForEachChunk(m_q, [](ecs::Chunk& ch) {
				auto v = ch.ViewRW<Velocity>();

				[&](Velocity* GAIA_RESTRICT v, const uint32_t size) {
					for (auto i = 0U; i < size; ++i)
						v[i].y += 9.81f * dt;
				}(v.data(), ch.GetItemCount());
			});
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
	CreateECSEntities_Static<true>(w);
	CreateECSEntities_Dynamic<true>(w);

	class PositionSystem final: public ecs::System {
		ecs::EntityQuery m_q;

	public:
		void OnCreated() override {
			m_q.All<PositionSoA, VelocitySoA>();
		}
		void OnUpdate() override {
			GetWorld().ForEachChunk(m_q, [](ecs::Chunk& ch) {
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

				const auto size = ch.GetItemCount();
				exec(ppx.data(), vvx.data(), size);
				exec(ppy.data(), vvy.data(), size);
				exec(ppz.data(), vvz.data(), size);
			});
		}
	};
	class CollisionSystem final: public ecs::System {
		ecs::EntityQuery m_q;

	public:
		void OnCreated() override {
			m_q.All<PositionSoA, VelocitySoA>();
		}
		void OnUpdate() override {
			GetWorld().ForEachChunk(m_q, [](ecs::Chunk& ch) {
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

				const auto size = ch.GetItemCount();
				exec(ppy.data(), vvy.data(), size);
			});
		}
	};
	class GravitySystem final: public ecs::System {
		ecs::EntityQuery m_q;

	public:
		void OnCreated() override {
			m_q.All<VelocitySoA>();
		}

		void OnUpdate() override {
			GetWorld().ForEachChunk(m_q, [](ecs::Chunk& ch) {
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

				auto exec = [](float* GAIA_RESTRICT v, const size_t sz) {
					for (size_t i = 0U; i < sz; ++i)
						v[i] *= dt * 9.81f;
				};

				const auto size = ch.GetItemCount();
				exec(vvy.data(), size);
			});
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
	CreateECSEntities_Static<true>(w);
	CreateECSEntities_Dynamic<true>(w);

	class PositionSystem final: public ecs::System {
		ecs::EntityQuery m_q;

	public:
		void OnCreated() override {
			m_q.All<PositionSoA, VelocitySoA>();
		}
		void OnUpdate() override {
			GetWorld().ForEachChunk(m_q, [](ecs::Chunk& ch) {
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
			});
		}
	};

	class CollisionSystem final: public ecs::System {
		ecs::EntityQuery m_q;

	public:
		void OnCreated() override {
			m_q.All<PositionSoA, VelocitySoA>();
		}
		void OnUpdate() override {
			GetWorld().ForEachChunk(m_q, [](ecs::Chunk& ch) {
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
			});
		}
	};

	class GravitySystem final: public ecs::System {
		ecs::EntityQuery m_q;

	public:
		void OnCreated() override {
			m_q.All<VelocitySoA>();
		}

		void OnUpdate() override {
			GetWorld().ForEachChunk(m_q, [](ecs::Chunk& ch) {
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
			});
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

	struct UnitDynamic1: public IUnit {
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
	struct UnitDynamic2: public IUnit {
		Velocity v;
		Direction d;

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
	struct UnitDynamic3: public IUnit {
		Velocity v;
		Direction d;
		Health h;

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
	struct UnitDynamic4: public IUnit {
		Velocity v;
		Direction d;
		Health h;
		IsEnemy e;

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
	{
		for (uint32_t i = 0U; i < N; i++) {
			auto u = new UnitStatic();
			u->p = {0, 100, 0};
			u->r = {1, 2, 3, 4};
			u->s = {1, 1, 1};
			units[i] = u;
		}
		uint32_t j = N;
		for (uint32_t i = 0U; i < N / 4; i++) {
			auto u = new UnitDynamic1();
			u->p = {0, 100, 0};
			u->r = {1, 2, 3, 4};
			u->s = {1, 1, 1};
			u->v = {0, 0, 1};
			units[j + i] = u;
		}
		j += N / 4;
		for (uint32_t i = 0U; i < N / 4; i++) {
			auto u = new UnitDynamic2();
			u->p = {0, 100, 0};
			u->r = {1, 2, 3, 4};
			u->s = {1, 1, 1};
			u->v = {0, 0, 1};
			units[j + i] = u;
		}
		j += N / 4;
		for (uint32_t i = 0U; i < N / 4; i++) {
			auto u = new UnitDynamic3();
			u->p = {0, 100, 0};
			u->r = {1, 2, 3, 4};
			u->s = {1, 1, 1};
			u->v = {0, 0, 1};
			units[j + i] = u;
		}
		j += N / 4;
		for (uint32_t i = 0U; i < N / 4; i++) {
			auto u = new UnitDynamic4();
			u->p = {0, 100, 0};
			u->r = {1, 2, 3, 4};
			u->s = {1, 1, 1};
			u->v = {0, 0, 1};
			units[j + i] = u;
		}
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

	struct UnitDynamic1 {
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

	struct UnitDynamic2: public UnitDynamic1 {
		Direction d;
	};

	struct UnitDynamic3: public UnitDynamic2 {
		Health h;
	};

	struct UnitDynamic4: public UnitDynamic3 {
		IsEnemy e;
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

	containers::darray<UnitDynamic1> units_dynamic1(N / 4);
	containers::darray<UnitDynamic2> units_dynamic2(N / 4);
	containers::darray<UnitDynamic3> units_dynamic3(N / 4);
	containers::darray<UnitDynamic4> units_dynamic4(N / 4);

	for (uint32_t i = 0U; i < N / 4; i++) {
		UnitDynamic1 u;
		u.p = {0, 100, 0};
		u.r = {1, 2, 3, 4};
		u.s = {1, 1, 1};
		u.v = {0, 0, 1};
		units_dynamic1[i] = std::move(u);
	}
	for (uint32_t i = 0U; i < N / 4; i++) {
		UnitDynamic2 u;
		u.p = {0, 100, 0};
		u.r = {1, 2, 3, 4};
		u.s = {1, 1, 1};
		u.v = {0, 0, 1};
		u.d = {0, 0, 1};
		units_dynamic2[i] = std::move(u);
	}
	for (uint32_t i = 0U; i < N / 4; i++) {
		UnitDynamic3 u;
		u.p = {0, 100, 0};
		u.r = {1, 2, 3, 4};
		u.v = {0, 0, 1};
		u.s = {1, 1, 1};
		u.d = {0, 0, 1};
		u.h = {100};
		units_dynamic3[i] = std::move(u);
	}
	for (uint32_t i = 0U; i < N / 4; i++) {
		UnitDynamic4 u;
		u.p = {0, 100, 0};
		u.r = {1, 2, 3, 4};
		u.s = {1, 1, 1};
		u.v = {0, 0, 1};
		u.d = {0, 0, 1};
		u.h = {100};
		u.e = {false};
		units_dynamic4[i] = std::move(u);
	}

	auto exec = [](auto& arr) {
		for (auto& u: arr) {
			u.updatePosition(dt);
			u.handleGroundCollision(dt);
			u.applyGravity(dt);
		}
	};

	srand(0);
	for ([[maybe_unused]] auto _: state) {
		dt = CalculateDelta(state);

		exec(units_static);
		exec(units_dynamic1);
		exec(units_dynamic2);
		exec(units_dynamic3);
		exec(units_dynamic4);
	}
}

template <uint32_t Groups>
void BM_Game_NonECS_DOD(benchmark::State& state) {
	struct UnitDynamic {
		static void
		updatePosition(containers::darray<Position>& p, const containers::darray<Velocity>& v, float deltaTime) {
			[&](Position* GAIA_RESTRICT p, const Velocity* GAIA_RESTRICT v, const size_t size) {
				for (uint32_t i = 0U; i < size; i++) {
					p[i].x += v[i].x * deltaTime;
					p[i].y += v[i].y * deltaTime;
					p[i].z += v[i].z * deltaTime;
				}
			}(p.data(), v.data(), v.size());
		}
		static void handleGroundCollision(containers::darray<Position>& p, containers::darray<Velocity>& v) {
			[&](Position* GAIA_RESTRICT p, Velocity* GAIA_RESTRICT v, const size_t size) {
				for (uint32_t i = 0U; i < size; i++) {
					if (p[i].y < 0.0f) {
						p[i].y = 0.0f;
						v[i].y = 0.0f;
					}
				}
			}(p.data(), v.data(), v.size());
		}

		static void applyGravity(containers::darray<Velocity>& v, float deltaTime) {
			[&](Velocity* GAIA_RESTRICT v, const size_t size) {
				for (uint32_t i = 0U; i < size; i++)
					v[i].y += 9.81f * deltaTime;
			}(v.data(), v.size());
		}
	};

	constexpr uint32_t NGroup = N / Groups;

	// Create static entities.
	struct static_units_group {
		containers::darray<Position> units_p{NGroup};
		containers::darray<Rotation> units_r{NGroup};
		containers::darray<Scale> units_s{NGroup};
	} static_groups[Groups];
	for (auto& g: static_groups) {
		for (uint32_t i = 0U; i < NGroup; i++) {
			g.units_p[i] = {0, 100, 0};
			g.units_r[i] = {1, 2, 3, 4};
			g.units_s[i] = {1, 1, 1};
		}
	}

	// Create dynamic entities.
	struct dynamic_units_group {
		containers::darray<Position> units_p{NGroup};
		containers::darray<Rotation> units_r{NGroup};
		containers::darray<Scale> units_s{NGroup};
		containers::darray<Velocity> units_v{NGroup};
		containers::darray<Direction> units_d{NGroup};
		containers::darray<Health> units_h{NGroup};
		containers::darray<IsEnemy> units_e{NGroup};
	} dynamic_groups[Groups];
	for (auto& g: dynamic_groups) {
		for (uint32_t i = 0U; i < NGroup; i++) {
			g.units_p[i] = {0, 100, 0};
			g.units_r[i] = {1, 2, 3, 4};
			g.units_s[i] = {1, 1, 1};
			g.units_v[i] = {0, 0, 1};
			g.units_d[i] = {0, 0, 1};
			g.units_h[i] = {100};
			g.units_e[i] = {false};
		}
	}

	srand(0);
	for ([[maybe_unused]] auto _: state) {
		dt = CalculateDelta(state);

		// Process static entities
		for (auto& g: dynamic_groups)
			UnitDynamic::updatePosition(g.units_p, g.units_v, dt);
		for (auto& g: dynamic_groups)
			UnitDynamic::handleGroundCollision(g.units_p, g.units_v);
		for (auto& g: dynamic_groups)
			UnitDynamic::applyGravity(g.units_v, dt);
	}
}

template <uint32_t Groups>
void BM_Game_NonECS_DOD_SoA(benchmark::State& state) {
	struct UnitDynamic {
		static void updatePosition(containers::darray<PositionSoA>& p, const containers::darray<VelocitySoA>& v) {
			gaia::utils::auto_view_policy_set<PositionSoA> pv{std::span(p.data(), p.size())};
			gaia::utils::auto_view_policy_get<VelocitySoA> vv{std::span(v.data(), v.size())};

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

			const auto size = p.size();
			exec(ppx.data(), vvx.data(), size);
			exec(ppy.data(), vvy.data(), size);
			exec(ppz.data(), vvz.data(), size);
		}

		static void handleGroundCollision(containers::darray<PositionSoA>& p, containers::darray<VelocitySoA>& v) {
			gaia::utils::auto_view_policy_set<PositionSoA> pv{std::span(p.data(), p.size())};
			gaia::utils::auto_view_policy_set<VelocitySoA> vv{std::span(v.data(), v.size())};

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

			const auto size = p.size();
			exec(ppy.data(), vvy.data(), size);
		}

		static void applyGravity(containers::darray<VelocitySoA>& v) {
			gaia::utils::auto_view_policy_set<VelocitySoA> vv{std::span(v.data(), v.size())};

			auto vvy = vv.set<1>();

			auto exec = [](float* GAIA_RESTRICT v, const size_t sz) {
				for (size_t i = 0U; i < sz; ++i)
					v[i] *= 9.81f * dt;
			};

			const auto size = v.size();
			exec(vvy.data(), size);
		}
	};

	constexpr uint32_t NGroup = N / Groups;

	// Create static entities.
	struct static_units_group {
		containers::darray<PositionSoA> units_p{NGroup};
		containers::darray<Rotation> units_r{NGroup};
		containers::darray<Scale> units_s{NGroup};
	} static_groups[Groups];
	for (auto& g: static_groups) {
		for (uint32_t i = 0U; i < NGroup; i++) {
			g.units_p[i] = {0, 100, 0};
			g.units_r[i] = {1, 2, 3, 4};
			g.units_s[i] = {1, 1, 1};
		}
	}

	// Create dynamic entities.
	struct dynamic_units_group {
		containers::darray<PositionSoA> units_p{NGroup};
		containers::darray<Rotation> units_r{NGroup};
		containers::darray<Scale> units_s{NGroup};
		containers::darray<VelocitySoA> units_v{NGroup};
		containers::darray<Direction> units_d{NGroup};
		containers::darray<Health> units_h{NGroup};
		containers::darray<IsEnemy> units_e{NGroup};
	} dynamic_groups[Groups];
	for (auto& g: dynamic_groups) {
		for (uint32_t i = 0U; i < NGroup; i++) {
			g.units_p[i] = {0, 100, 0};
			g.units_r[i] = {1, 2, 3, 4};
			g.units_s[i] = {1, 1, 1};
			g.units_v[i] = {0, 0, 1};
			g.units_d[i] = {0, 0, 1};
			g.units_h[i] = {100};
			g.units_e[i] = {false};
		}
	}

	srand(0);
	for ([[maybe_unused]] auto _: state) {
		dt = CalculateDelta(state);

		// Process static entities
		for (auto& g: dynamic_groups)
			UnitDynamic::updatePosition(g.units_p, g.units_v);
		for (auto& g: dynamic_groups)
			UnitDynamic::handleGroundCollision(g.units_p, g.units_v);
		for (auto& g: dynamic_groups)
			UnitDynamic::applyGravity(g.units_v);
	}
}

template <uint32_t Groups>
void BM_Game_NonECS_DOD_SoA_ManualSIMD(benchmark::State& state) {

	struct UnitDynamic {
		static void updatePosition(containers::darray<PositionSoA>& p, const containers::darray<VelocitySoA>& v) {
			gaia::utils::auto_view_policy_set<PositionSoA> pv{std::span(p.data(), p.size())};
			gaia::utils::auto_view_policy_get<VelocitySoA> vv{std::span(v.data(), v.size())};

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
			gaia::utils::auto_view_policy_set<PositionSoA> pv{std::span(p.data(), p.size())};
			gaia::utils::auto_view_policy_set<VelocitySoA> vv{std::span(v.data(), v.size())};

			auto ppy = pv.set<1>();
			auto vvy = vv.set<1>();
			const auto size = p.size();

			auto exec = [](float* GAIA_RESTRICT p, float* GAIA_RESTRICT v, const size_t offset) {
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
			gaia::utils::auto_view_policy_set<VelocitySoA> vv{std::span(v.data(), v.size())};

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

	constexpr uint32_t NGroup = N / Groups;

	// Create static entities.
	struct static_units_group {
		containers::darray<PositionSoA> units_p{NGroup};
		containers::darray<Rotation> units_r{NGroup};
		containers::darray<Scale> units_s{NGroup};
	} static_groups[Groups];
	for (auto& g: static_groups) {
		for (uint32_t i = 0U; i < NGroup; i++) {
			g.units_p[i] = {0, 100, 0};
			g.units_r[i] = {1, 2, 3, 4};
			g.units_s[i] = {1, 1, 1};
		}
	}

	// Create dynamic entities.
	struct dynamic_units_group {
		containers::darray<PositionSoA> units_p{NGroup};
		containers::darray<Rotation> units_r{NGroup};
		containers::darray<Scale> units_s{NGroup};
		containers::darray<VelocitySoA> units_v{NGroup};
		containers::darray<Direction> units_d{NGroup};
		containers::darray<Health> units_h{NGroup};
		containers::darray<IsEnemy> units_e{NGroup};
	} dynamic_groups[Groups];
	for (auto& g: dynamic_groups) {
		for (uint32_t i = 0U; i < NGroup; i++) {
			g.units_p[i] = {0, 100, 0};
			g.units_r[i] = {1, 2, 3, 4};
			g.units_s[i] = {1, 1, 1};
			g.units_v[i] = {0, 0, 1};
			g.units_d[i] = {0, 0, 1};
			g.units_h[i] = {100};
			g.units_e[i] = {false};
		}
	}

	srand(0);
	for ([[maybe_unused]] auto _: state) {
		dt = CalculateDelta(state);

		// Process static entities
		for (auto& g: dynamic_groups)
			UnitDynamic::updatePosition(g.units_p, g.units_v);
		for (auto& g: dynamic_groups)
			UnitDynamic::handleGroundCollision(g.units_p, g.units_v);
		for (auto& g: dynamic_groups)
			UnitDynamic::applyGravity(g.units_v);
	}
}

// Ordinary coding style.
BENCHMARK(BM_Game_NonECS);
// Ordinary coding style with optimized memory layout (imagine using custom allocators
// to keep things close and tidy in memory).
BENCHMARK(BM_Game_NonECS_BetterMemoryLayout);
// Memory organized in DoD style.
// Performance target BM_Game_ECS_WithSystems_ForEachChunk.
// "Groups" is there to simulate having items split into separate chunks similar to what ECS does.
BENCHMARK_TEMPLATE(BM_Game_NonECS_DOD, 1);
BENCHMARK_TEMPLATE(BM_Game_NonECS_DOD, 20);
BENCHMARK_TEMPLATE(BM_Game_NonECS_DOD, 40);
BENCHMARK_TEMPLATE(BM_Game_NonECS_DOD, 80);
BENCHMARK_TEMPLATE(BM_Game_NonECS_DOD, 160);
// Best possible performance with no manual optimization.
// Performance target for BM_Game_ECS_WithSystems_ForEachChunk_SoA.
// "Groups" is there to simulate having items split into separate chunks similar to what ECS does.
BENCHMARK_TEMPLATE(BM_Game_NonECS_DOD_SoA, 1);
BENCHMARK_TEMPLATE(BM_Game_NonECS_DOD_SoA, 20);
BENCHMARK_TEMPLATE(BM_Game_NonECS_DOD_SoA, 40);
BENCHMARK_TEMPLATE(BM_Game_NonECS_DOD_SoA, 80);
BENCHMARK_TEMPLATE(BM_Game_NonECS_DOD_SoA, 160);
// Best possible performance.
// Performance target for BM_Game_ECS_WithSystems_ForEachChunk_SoA_ManualSIMD.
// "Groups" is there to simulate having items split into separate chunks similar to what ECS does.
BENCHMARK_TEMPLATE(BM_Game_NonECS_DOD_SoA_ManualSIMD, 1);
BENCHMARK_TEMPLATE(BM_Game_NonECS_DOD_SoA_ManualSIMD, 20);
BENCHMARK_TEMPLATE(BM_Game_NonECS_DOD_SoA_ManualSIMD, 40);
BENCHMARK_TEMPLATE(BM_Game_NonECS_DOD_SoA_ManualSIMD, 80);
BENCHMARK_TEMPLATE(BM_Game_NonECS_DOD_SoA_ManualSIMD, 160);

BENCHMARK(BM_Game_ECS);
BENCHMARK(BM_Game_ECS_WithSystems);
BENCHMARK(BM_Game_ECS_WithSystems_ForEachChunk);
BENCHMARK(BM_Game_ECS_WithSystems_ForEachChunk_SoA);
BENCHMARK(BM_Game_ECS_WithSystems_ForEachChunk_SoA_ManualSIMD);

// Run the benchmark
BENCHMARK_MAIN();
