#define PICOBENCH_IMPLEMENT
#include "gaia.h"
#include "gaia/external/picobench.hpp"
#include <string_view>

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
	int max;
};
struct IsEnemy {
	bool value;
};
struct Dummy {
	int value[24];
};

constexpr size_t N = 32'000; // kept a multiple of 32 to keep it simple even for SIMD code
constexpr float MinDelta = 0.01f;
constexpr float MaxDelta = 0.033f;

float CalculateDelta(picobench::state& state) {
	state.stop_timer();
	const float d = static_cast<float>((double)RAND_MAX / (MaxDelta - MinDelta));
	float delta = MinDelta + (static_cast<float>(rand()) / d);
	state.start_timer();
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
		for (size_t i = 0; i < N; i++) {
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
		for (size_t i = 0; i < N / 4; i++) {
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
		for (size_t i = 0; i < N / 4; i++) {
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
		w.AddComponent<Health>(e, {100, 100});
		for (size_t i = 0; i < N / 4; i++) {
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
		w.AddComponent<Health>(e, {100, 100});
		w.AddComponent<IsEnemy>(e, {false});
		for (size_t i = 0; i < N / 4; i++) {
			[[maybe_unused]] auto newentity = w.CreateEntity(e);
		}
	}
}

void BM_ECS(picobench::state& state) {
	ecs::World w;
	CreateECSEntities_Static<false>(w);
	CreateECSEntities_Dynamic<false>(w);

	auto queryPosCVel = w.CreateQuery().All<Position, const Velocity>();
	auto queryPosVel = w.CreateQuery().All<Position, Velocity>();
	auto queryVel = w.CreateQuery().All<Velocity>();
	auto queryCHealth = w.CreateQuery().All<const Health>();

	/* We want to benchmark the hot-path. In real-world scenarios queries are cached so cache them now */
	(void)queryPosCVel.HasEntities();
	(void)queryPosVel.HasEntities();
	(void)queryVel.HasEntities();
	(void)queryCHealth.HasEntities();

	srand(0);
	for (auto _: state) {
		(void)_;
		dt = CalculateDelta(state);

		// Update position
		queryPosCVel.ForEach([&](Position& p, const Velocity& v) {
			p.x += v.x * dt;
			p.y += v.y * dt;
			p.z += v.z * dt;
		});
		// Handle ground collision
		queryPosVel.ForEach([&](Position& p, Velocity& v) {
			if (p.y < 0.0f) {
				p.y = 0.0f;
				v.y = 0.0f;
			}
		});
		// Apply gravity
		queryVel.ForEach([&](Velocity& v) {
			v.y += 9.81f * dt;
		});
		// Calculate the number of units alive
		uint32_t aliveUnits = 0;
		queryCHealth.ForEach([&](const Health& h) {
			if (h.value > 0)
				++aliveUnits;
		});
		(void)aliveUnits;

		GAIA_PROF_FRAME();
	}
}

class TestSystem: public ecs::System {
protected:
	ecs::Query* m_q;

public:
	void Init(ecs::Query* q) {
		m_q = q;
	}
};

void BM_ECS_WithSystems(picobench::state& state) {
	ecs::World w;
	CreateECSEntities_Static<false>(w);
	CreateECSEntities_Dynamic<false>(w);

	auto queryPosCVel = w.CreateQuery().All<Position, const Velocity>();
	auto queryPosVel = w.CreateQuery().All<Position, Velocity>();
	auto queryVel = w.CreateQuery().All<Velocity>();
	auto queryCHealth = w.CreateQuery().All<const Health>();

	/* We want to benchmark the hot-path. In real-world scenarios queries are cached so cache them now */
	(void)queryPosCVel.HasEntities();
	(void)queryPosCVel.HasEntities();
	(void)queryPosCVel.HasEntities();
	(void)queryPosCVel.HasEntities();

	class PositionSystem final: public TestSystem {
	public:
		void OnUpdate() override {
			m_q->ForEach([](Position& p, const Velocity& v) {
				p.x += v.x * dt;
				p.y += v.y * dt;
				p.z += v.z * dt;
			});
		}
	};
	class CollisionSystem final: public TestSystem {
	public:
		void OnUpdate() override {
			m_q->ForEach([](Position& p, Velocity& v) {
				if (p.y < 0.0f) {
					p.y = 0.0f;
					v.y = 0.0f;
				}
			});
		}
	};
	class GravitySystem final: public TestSystem {
	public:
		void OnUpdate() override {
			m_q->ForEach([](Velocity& v) {
				v.y += 9.81f * dt;
			});
		}
	};
	class CalculateAliveUnitsSystem final: public TestSystem {
	public:
		void OnUpdate() override {
			uint32_t aliveUnits = 0;
			m_q->ForEach([&](const Health& h) {
				if (h.value > 0)
					++aliveUnits;
			});
			(void)aliveUnits;
		}
	};

	ecs::SystemManager sm(w);
	sm.CreateSystem<PositionSystem>()->Init(&queryPosCVel);
	sm.CreateSystem<CollisionSystem>()->Init(&queryPosVel);
	sm.CreateSystem<GravitySystem>()->Init(&queryVel);
	sm.CreateSystem<CalculateAliveUnitsSystem>()->Init(&queryCHealth);

	srand(0);
	for (auto _: state) {
		(void)_;
		dt = CalculateDelta(state);

		sm.Update();
	}
}

void BM_ECS_WithSystems_Iter(picobench::state& state) {
	ecs::World w;
	CreateECSEntities_Static<false>(w);
	CreateECSEntities_Dynamic<false>(w);

	auto queryPosCVel = w.CreateQuery().All<Position, const Velocity>();
	auto queryPosVel = w.CreateQuery().All<Position, Velocity>();
	auto queryVel = w.CreateQuery().All<Velocity>();
	auto queryCHealth = w.CreateQuery().All<const Health>();

	/* We want to benchmark the hot-path. In real-world scenarios queries are cached so cache them now */
	(void)queryPosCVel.HasEntities();
	(void)queryPosVel.HasEntities();
	(void)queryVel.HasEntities();
	(void)queryCHealth.HasEntities();

	class PositionSystem final: public TestSystem {
	public:
		void OnUpdate() override {
			m_q->ForEach([](ecs::Iterator iter) {
				auto p = iter.ViewRW<Position>();
				auto v = iter.View<Velocity>();

				for (size_t i: iter) {
					p[i].x += v[i].x * dt;
					p[i].y += v[i].y * dt;
					p[i].z += v[i].z * dt;
				}
			});
		}
	};

	class CollisionSystem final: public TestSystem {
	public:
		void OnUpdate() override {
			m_q->ForEach([](ecs::Iterator iter) {
				auto p = iter.ViewRW<Position>();
				auto v = iter.ViewRW<Velocity>();

				for (size_t i: iter) {
					if (p[i].y < 0.0f) {
						p[i].y = 0.0f;
						v[i].y = 0.0f;
					}
				}
			});
		}
	};
	class GravitySystem final: public TestSystem {
	public:
		void OnUpdate() override {
			m_q->ForEach([](ecs::Iterator iter) {
				auto v = iter.ViewRW<Velocity>();

				for (size_t i: iter)
					v[i].y += 9.81f * dt;
			});
		}
	};
	class CalculateAliveUnitsSystem final: public TestSystem {
	public:
		void OnUpdate() override {
			uint32_t aliveUnits = 0;
			m_q->ForEach([&](ecs::Iterator iter) {
				auto h = iter.View<Health>();

				uint32_t a = 0;
				for (size_t i: iter) {
					if (h[i].value > 0)
						++a;
				}
				aliveUnits += a;
			});
			(void)aliveUnits;
		}
	};

	ecs::SystemManager sm(w);
	sm.CreateSystem<PositionSystem>()->Init(&queryPosCVel);
	sm.CreateSystem<CollisionSystem>()->Init(&queryPosVel);
	sm.CreateSystem<GravitySystem>()->Init(&queryVel);
	sm.CreateSystem<CalculateAliveUnitsSystem>()->Init(&queryCHealth);

	srand(0);
	for (auto _: state) {
		(void)_;
		dt = CalculateDelta(state);

		sm.Update();
	}
}

void BM_ECS_WithSystems_Iter_SoA(picobench::state& state) {
	ecs::World w;
	CreateECSEntities_Static<true>(w);
	CreateECSEntities_Dynamic<true>(w);

	auto queryPosCVel = w.CreateQuery().All<PositionSoA, const VelocitySoA>();
	auto queryPosVel = w.CreateQuery().All<PositionSoA, VelocitySoA>();
	auto queryVel = w.CreateQuery().All<VelocitySoA>();
	auto queryCHealth = w.CreateQuery().All<const Health>();

	/* We want to benchmark the hot-path. In real-world scenarios queries are cached so cache them now */
	(void)queryPosCVel.HasEntities();
	(void)queryPosVel.HasEntities();
	(void)queryVel.HasEntities();
	(void)queryCHealth.HasEntities();

	class PositionSystem final: public TestSystem {
	public:
		void OnUpdate() override {
			m_q->ForEach([](ecs::Iterator iter) {
				auto p = iter.ViewRW<PositionSoA>();
				auto v = iter.View<VelocitySoA>();

				auto ppx = p.set<0>();
				auto ppy = p.set<1>();
				auto ppz = p.set<2>();

				auto vvx = v.get<0>();
				auto vvy = v.get<1>();
				auto vvz = v.get<2>();

				for (size_t i: iter)
					ppx[i] += vvx[i] * dt;
				for (size_t i: iter)
					ppy[i] += vvy[i] * dt;
				for (size_t i: iter)
					ppz[i] += vvz[i] * dt;
			});
		}
	};
	class CollisionSystem final: public TestSystem {
	public:
		void OnUpdate() override {
			m_q->ForEach([](ecs::Iterator iter) {
				auto p = iter.ViewRW<PositionSoA>();
				auto v = iter.ViewRW<VelocitySoA>();

				auto ppy = p.set<1>();
				auto vvy = v.set<1>();

				for (size_t i: iter) {
					if (ppy[i] < 0.0f) {
						ppy[i] = 0.0f;
						vvy[i] = 0.0f;
					}
				}
			});
		}
	};
	class GravitySystem final: public TestSystem {
	public:
		void OnUpdate() override {
			m_q->ForEach([](ecs::Iterator iter) {
				auto v = iter.ViewRW<VelocitySoA>();
				auto vvy = v.set<1>();

				for (size_t i: iter)
					vvy[i] += dt * 9.81f;
			});
		}
	};
	class CalculateAliveUnitsSystem final: public TestSystem {
	public:
		void OnUpdate() override {
			uint32_t aliveUnits = 0;
			m_q->ForEach([&](ecs::Iterator iter) {
				auto h = iter.View<Health>();

				uint32_t a = 0;
				for (size_t i: iter) {
					if (h[i].value > 0)
						++a;
				}
				aliveUnits += a;
			});
			(void)aliveUnits;
		}
	};

	ecs::SystemManager sm(w);
	sm.CreateSystem<PositionSystem>()->Init(&queryPosCVel);
	sm.CreateSystem<CollisionSystem>()->Init(&queryPosVel);
	sm.CreateSystem<GravitySystem>()->Init(&queryVel);
	sm.CreateSystem<CalculateAliveUnitsSystem>()->Init(&queryCHealth);

	srand(0);
	for (auto _: state) {
		(void)_;
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

void BM_ECS_WithSystems_Iter_SoA_SIMD(picobench::state& state) {
	ecs::World w;
	CreateECSEntities_Static<true>(w);
	CreateECSEntities_Dynamic<true>(w);

	auto queryPosCVel = w.CreateQuery().All<PositionSoA, const VelocitySoA>();
	auto queryPosVel = w.CreateQuery().All<PositionSoA, VelocitySoA>();
	auto queryVel = w.CreateQuery().All<VelocitySoA>();
	auto queryCHealth = w.CreateQuery().All<const Health>();

	/* We want to benchmark the hot-path. In real-world scenarios queries are cached so cache them now */
	(void)queryPosCVel.HasEntities();
	(void)queryPosVel.HasEntities();
	(void)queryVel.HasEntities();
	(void)queryCHealth.HasEntities();

	class PositionSystem final: public TestSystem {
	public:
		void OnUpdate() override {
			m_q->ForEach([](ecs::Iterator iter) {
				auto p = iter.ViewRW<PositionSoA>();
				auto v = iter.View<VelocitySoA>();

				auto ppx = p.set<0>();
				auto ppy = p.set<1>();
				auto ppz = p.set<2>();

				auto vvx = v.get<0>();
				auto vvy = v.get<1>();
				auto vvz = v.get<2>();

				const auto size = iter.size();
				const auto dtVec = _mm_set_ps1(dt);

				auto exec = [&](float* GAIA_RESTRICT p, const float* GAIA_RESTRICT v, const size_t offset) {
					const auto pVec = _mm_load_ps(p + offset);
					const auto vVec = _mm_load_ps(v + offset);
					const auto respVec = _mm_fmadd_ps(vVec, dtVec, pVec);
					_mm_store_ps(p + offset, respVec);
				};
				auto exec2 = [](float* GAIA_RESTRICT p, const float* GAIA_RESTRICT v, const size_t offset) {
					p[offset] += v[offset] * dt;
				};

				size_t i = 0;
				// Optimize via "double-read" trick to hide latencies.
				// Item count is always a multiple of 4 for chunks with SoA components.
				for (; i < size; i += 8) {
					exec(ppx.data(), vvx.data(), i);
					exec(ppx.data(), vvx.data(), i + 4);
				}
				for (; i < size; i++)
					exec2(ppx.data(), vvx.data(), i);

				i = 0;
				for (; i < size; i += 8) {
					exec(ppy.data(), vvy.data(), i);
					exec(ppy.data(), vvy.data(), i + 4);
				}
				for (; i < size; i++)
					exec2(ppy.data(), vvy.data(), i);

				i = 0;
				for (; i < size; i += 8) {
					exec(ppz.data(), vvz.data(), i);
					exec(ppz.data(), vvz.data(), i + 4);
				}
				for (; i < size; i++)
					exec2(ppz.data(), vvz.data(), i);
			});
		}
	};

	class CollisionSystem final: public TestSystem {
	public:
		void OnUpdate() override {
			m_q->ForEach([](ecs::Iterator iter) {
				auto p = iter.ViewRW<PositionSoA>();
				auto v = iter.ViewRW<VelocitySoA>();

				auto ppy = p.set<1>();
				auto vvy = v.set<1>();
				const auto size = iter.size();

				auto exec = [&](float* GAIA_RESTRICT p, float* GAIA_RESTRICT v, const size_t offset) {
					const auto vyVec = _mm_load_ps(v + offset);
					const auto pyVec = _mm_load_ps(p + offset);

					const auto condVec = _mm_cmplt_ps(vyVec, _mm_setzero_ps());
					const auto res_vyVec = _mm_blendv_ps(vyVec, _mm_setzero_ps(), condVec);
					const auto res_pyVec = _mm_blendv_ps(pyVec, _mm_setzero_ps(), condVec);

					_mm_store_ps(v + offset, res_vyVec);
					_mm_store_ps(p + offset, res_pyVec);
				};
				auto exec2 = [](float* GAIA_RESTRICT p, float* GAIA_RESTRICT v, const size_t offset) {
					if (p[offset] < 0.0f) {
						p[offset] = 0.0f;
						v[offset] = 0.0f;
					}
				};

				size_t i = 0;
				// Optimize via "double-read" trick to hide latencies.
				// Item count is always a multiple of 4 for chunks with SoA components.
				for (; i < size; i += 8) {
					exec(ppy.data(), vvy.data(), i);
					exec(ppy.data(), vvy.data(), i + 4);
				}
				for (; i < size; i++)
					exec2(ppy.data(), vvy.data(), i);
			});
		}
	};

	class GravitySystem final: public TestSystem {
	public:
		void OnUpdate() override {
			m_q->ForEach([](ecs::Iterator iter) {
				auto v = iter.ViewRW<VelocitySoA>();

				auto vvy = v.set<1>();
				const auto size = iter.size();

				const auto gg_gVec = _mm_set_ps1(9.81f);
				const auto gg_dtVec = _mm_set_ps1(dt);

				auto exec = [&](float* GAIA_RESTRICT v, const size_t offset) {
					const auto vyVec = _mm_load_ps(v + offset);
					const auto mulVec = _mm_fmadd_ps(gg_dtVec, gg_dtVec, vyVec);
					_mm_store_ps(v + offset, mulVec);
				};
				auto exec2 = [](float* GAIA_RESTRICT v, const size_t offset) {
					v[offset] += dt * 9.81f;
				};

				size_t i = 0;
				// Optimize via "double-read" trick to hide latencies.
				// Item count is always a multiple of 4 for chunks with SoA components.
				for (; i < size; i += 8) {
					exec(vvy.data(), i);
					exec(vvy.data(), i + 4);
				}
				for (; i < size; i++)
					exec2(vvy.data(), i);
			});
		}
	};

	class CalculateAliveUnitsSystem final: public TestSystem {
	public:
		void OnUpdate() override {
			uint32_t aliveUnits = 0;
			m_q->ForEach([&](ecs::Iterator iter) {
				auto h = iter.View<Health>();

				uint32_t a = 0;
				for (size_t i = 0; i < h.size(); ++i) {
					if (h[i].value > 0)
						++a;
				}
				aliveUnits += a;
			});
			(void)aliveUnits;
		}
	};

	ecs::SystemManager sm(w);
	sm.CreateSystem<PositionSystem>()->Init(&queryPosCVel);
	sm.CreateSystem<CollisionSystem>()->Init(&queryPosVel);
	sm.CreateSystem<GravitySystem>()->Init(&queryVel);
	sm.CreateSystem<CalculateAliveUnitsSystem>()->Init(&queryCHealth);

	srand(0);
	for (auto _: state) {
		(void)_;
		dt = CalculateDelta(state);

		sm.Update();
	}
}

namespace NonECS {
	struct IUnit {
		Position p;
		Rotation r;
		Scale s;
		//! This is a bunch of generic data that keeps getting added
		//! to the base class over its lifetime and is rarely used ever.
		Dummy dummy;

		IUnit() = default;
		virtual ~IUnit() = default;

		virtual void updatePosition(float deltaTime) = 0;
		virtual void handleGroundCollision(float deltaTime) = 0;
		virtual void applyGravity(float deltaTime) = 0;
		virtual bool isAlive() const = 0;
	};

	struct UnitStatic: public IUnit {
		void updatePosition([[maybe_unused]] float deltaTime) override {}
		void handleGroundCollision([[maybe_unused]] float deltaTime) override {}
		void applyGravity([[maybe_unused]] float deltaTime) override {}
		bool isAlive() const override {
			return true;
		}
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

		bool isAlive() const override {
			return true;
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
		bool isAlive() const override {
			return true;
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
		bool isAlive() const override {
			return h.value > 0;
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
		bool isAlive() const override {
			return h.value > 0;
		}
	};
} // namespace NonECS

template <bool AlternativeExecOrder>
void BM_NonECS(picobench::state& state) {
	using namespace NonECS;

	// Create entities.
	// We allocate via new to simulate the usual kind of behavior in games
	containers::darray<IUnit*> units(N * 2);
	{
		for (size_t i = 0; i < N; i++) {
			auto u = new UnitStatic();
			u->p = {0, 100, 0};
			u->r = {1, 2, 3, 4};
			u->s = {1, 1, 1};
			units[i] = u;
		}
		uint32_t j = N;
		for (size_t i = 0; i < N / 4; i++) {
			auto u = new UnitDynamic1();
			u->p = {0, 100, 0};
			u->r = {1, 2, 3, 4};
			u->s = {1, 1, 1};
			u->v = {0, 0, 1};
			units[j + i] = u;
		}
		j += N / 4;
		for (size_t i = 0; i < N / 4; i++) {
			auto u = new UnitDynamic2();
			u->p = {0, 100, 0};
			u->r = {1, 2, 3, 4};
			u->s = {1, 1, 1};
			u->v = {0, 0, 1};
			units[j + i] = u;
		}
		j += N / 4;
		for (size_t i = 0; i < N / 4; i++) {
			auto u = new UnitDynamic3();
			u->p = {0, 100, 0};
			u->r = {1, 2, 3, 4};
			u->s = {1, 1, 1};
			u->v = {0, 0, 1};
			units[j + i] = u;
		}
		j += N / 4;
		for (size_t i = 0; i < N / 4; i++) {
			auto u = new UnitDynamic4();
			u->p = {0, 100, 0};
			u->r = {1, 2, 3, 4};
			u->s = {1, 1, 1};
			u->v = {0, 0, 1};
			units[j + i] = u;
		}
	}

	srand(0);
	for (auto _: state) {
		(void)_;
		dt = CalculateDelta(state);

		uint32_t aliveUnits = 0;

		// Process entities
		if constexpr (AlternativeExecOrder) {
			for (auto& u: units)
				u->updatePosition(dt);
			for (auto& u: units)
				u->handleGroundCollision(dt);
			for (auto& u: units)
				u->applyGravity(dt);
			for (auto& u: units) {
				if (u->isAlive())
					++aliveUnits;
			}
		} else {
			for (auto& u: units) {
				u->updatePosition(dt);
				u->handleGroundCollision(dt);
				u->applyGravity(dt);
				if (u->isAlive())
					++aliveUnits;
			}
		}
		(void)aliveUnits;

		GAIA_PROF_FRAME();
	}

	for (auto& u: units) {
		delete u;
	}
}

namespace NonECS_BetterMemoryLayout {
	struct UnitData {
		Position p;
		Rotation r;
		Scale s;
		//! This is a bunch of generic data that keeps getting added
		//! to the base class over its lifetime and is rarely used ever.
		Dummy dummy;
	};

	struct UnitStatic: UnitData {
		void updatePosition([[maybe_unused]] float deltaTime) {}
		void handleGroundCollision([[maybe_unused]] float deltaTime) {}
		void applyGravity([[maybe_unused]] float deltaTime) {}
		bool isAlive() const {
			return true;
		}
	};

	struct UnitDynamic1: UnitData {
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
		bool isAlive() const {
			return true;
		}
	};

	struct UnitDynamic2: public UnitDynamic1 {
		Direction d;
	};

	struct UnitDynamic3: public UnitDynamic2 {
		Health h;

		using UnitDynamic2::isAlive;
		bool isAlive() const {
			return h.value > 0;
		}
	};

	struct UnitDynamic4: public UnitDynamic3 {
		IsEnemy e;
	};
} // namespace NonECS_BetterMemoryLayout

template <bool AlternativeExecOrder>
void BM_NonECS_BetterMemoryLayout(picobench::state& state) {
	using namespace NonECS_BetterMemoryLayout;

	// Create entities.
	containers::darray<UnitStatic> units_static(N);
	for (size_t i = 0; i < N; i++) {
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

	for (size_t i = 0; i < N / 4; i++) {
		UnitDynamic1 u;
		u.p = {0, 100, 0};
		u.r = {1, 2, 3, 4};
		u.s = {1, 1, 1};
		u.v = {0, 0, 1};
		units_dynamic1[i] = std::move(u);
	}
	for (size_t i = 0; i < N / 4; i++) {
		UnitDynamic2 u;
		u.p = {0, 100, 0};
		u.r = {1, 2, 3, 4};
		u.s = {1, 1, 1};
		u.v = {0, 0, 1};
		u.d = {0, 0, 1};
		units_dynamic2[i] = std::move(u);
	}
	for (size_t i = 0; i < N / 4; i++) {
		UnitDynamic3 u;
		u.p = {0, 100, 0};
		u.r = {1, 2, 3, 4};
		u.v = {0, 0, 1};
		u.s = {1, 1, 1};
		u.d = {0, 0, 1};
		u.h = {100, 100};
		units_dynamic3[i] = std::move(u);
	}
	for (size_t i = 0; i < N / 4; i++) {
		UnitDynamic4 u;
		u.p = {0, 100, 0};
		u.r = {1, 2, 3, 4};
		u.s = {1, 1, 1};
		u.v = {0, 0, 1};
		u.d = {0, 0, 1};
		u.h = {100, 100};
		u.e = {false};
		units_dynamic4[i] = std::move(u);
	}

	auto exec = [](auto& arr) {
		if constexpr (AlternativeExecOrder) {
			for (auto& u: arr)
				u.updatePosition(dt);
			for (auto& u: arr)
				u.handleGroundCollision(dt);
			for (auto& u: arr)
				u.applyGravity(dt);
		} else {
			for (auto& u: arr) {
				u.updatePosition(dt);
				u.handleGroundCollision(dt);
				u.applyGravity(dt);
			}
		}
	};

	srand(0);
	for (auto _: state) {
		(void)_;
		dt = CalculateDelta(state);

		exec(units_static);
		exec(units_dynamic1);
		exec(units_dynamic2);
		exec(units_dynamic3);
		exec(units_dynamic4);

		uint32_t aliveUnits = 0;
		for (auto& u: units_dynamic3) {
			if (u.isAlive())
				++aliveUnits;
		}
		for (auto& u: units_dynamic4) {
			if (u.isAlive())
				++aliveUnits;
		}
		(void)aliveUnits;

		GAIA_PROF_FRAME();
	}
} // namespace BM_NonECS_BetterMemoryLayout(picobench::state

template <uint32_t Groups>
void BM_NonECS_DOD(picobench::state& state) {
	struct UnitDynamic {
		static void
		updatePosition(containers::darray<Position>& p, const containers::darray<Velocity>& v, float deltaTime) {
			for (size_t i = 0; i < p.size(); i++) {
				p[i].x += v[i].x * deltaTime;
				p[i].y += v[i].y * deltaTime;
				p[i].z += v[i].z * deltaTime;
			}
		}
		static void handleGroundCollision(containers::darray<Position>& p, containers::darray<Velocity>& v) {
			for (size_t i = 0; i < p.size(); i++) {
				if (p[i].y < 0.0f) {
					p[i].y = 0.0f;
					v[i].y = 0.0f;
				}
			}
		}

		static void applyGravity(containers::darray<Velocity>& v, float deltaTime) {
			for (size_t i = 0; i < v.size(); i++)
				v[i].y += 9.81f * deltaTime;
		}

		static uint32_t calculateAliveUnits(const containers::darray<Health>& h) {
			uint32_t aliveUnits = 0;
			for (size_t i = 0; i < h.size(); i++) {
				if (h[i].value > 0)
					++aliveUnits;
			}
			return aliveUnits;
		}
	};

	constexpr size_t NGroup = N / Groups;

	// Create static entities.
	struct static_units_group {
		containers::darray<Position> units_p{NGroup};
		containers::darray<Rotation> units_r{NGroup};
		containers::darray<Scale> units_s{NGroup};
	} static_groups[Groups];
	for (auto& g: static_groups) {
		for (size_t i = 0; i < NGroup; i++) {
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
		for (size_t i = 0; i < NGroup; i++) {
			g.units_p[i] = {0, 100, 0};
			g.units_r[i] = {1, 2, 3, 4};
			g.units_s[i] = {1, 1, 1};
			g.units_v[i] = {0, 0, 1};
			g.units_d[i] = {0, 0, 1};
			g.units_h[i] = {100, 100};
			g.units_e[i] = {false};
		}
	}

	srand(0);
	for (auto _: state) {
		(void)_;
		dt = CalculateDelta(state);

		{
			GAIA_PROF_SCOPE(updatePosition);

			for (auto& g: dynamic_groups)
				UnitDynamic::updatePosition(g.units_p, g.units_v, dt);
		}

		{
			GAIA_PROF_SCOPE(handleGroundPosition);

			for (auto& g: dynamic_groups)
				UnitDynamic::handleGroundCollision(g.units_p, g.units_v);
		}

		{
			GAIA_PROF_SCOPE(applyGravity);

			for (auto& g: dynamic_groups)
				UnitDynamic::applyGravity(g.units_v, dt);
		}

		uint32_t aliveUnits = 0;
		for (auto& g: dynamic_groups)
			aliveUnits += UnitDynamic::calculateAliveUnits(g.units_h);
		(void)aliveUnits;

		GAIA_PROF_FRAME();
	}
}

template <size_t Groups>
void BM_NonECS_DOD_SoA(picobench::state& state) {
	struct UnitDynamic {
		static void updatePosition(containers::darray<PositionSoA>& p, const containers::darray<VelocitySoA>& v) {
			GAIA_PROF_SCOPE(updatePosition);

			gaia::utils::auto_view_policy_set<PositionSoA> pv{{std::span(p.data(), p.size())}};
			gaia::utils::auto_view_policy_get<VelocitySoA> vv{{std::span(v.data(), v.size())}};

			auto ppx = pv.set<0>();
			auto ppy = pv.set<1>();
			auto ppz = pv.set<2>();

			auto vvx = vv.get<0>();
			auto vvy = vv.get<1>();
			auto vvz = vv.get<2>();

			for (size_t i = 0; i < ppx.size(); ++i)
				ppx[i] += vvx[i] * dt;
			for (size_t i = 0; i < ppy.size(); ++i)
				ppy[i] += vvy[i] * dt;
			for (size_t i = 0; i < ppz.size(); ++i)
				ppz[i] += vvz[i] * dt;
		}

		static void handleGroundCollision(containers::darray<PositionSoA>& p, containers::darray<VelocitySoA>& v) {
			GAIA_PROF_SCOPE(handleGroundCollision);

			gaia::utils::auto_view_policy_set<PositionSoA> pv{{std::span(p.data(), p.size())}};
			gaia::utils::auto_view_policy_set<VelocitySoA> vv{{std::span(v.data(), v.size())}};

			auto ppy = pv.set<1>();
			auto vvy = vv.set<1>();

			for (size_t i = 0; i < ppy.size(); ++i) {
				if (ppy[i] < 0.0f) {
					ppy[i] = 0.0f;
					vvy[i] = 0.0f;
				}
			}
		}

		static void applyGravity(containers::darray<VelocitySoA>& v) {
			GAIA_PROF_SCOPE(applyGravity);

			gaia::utils::auto_view_policy_set<VelocitySoA> vv{{std::span(v.data(), v.size())}};

			auto vvy = vv.set<1>();

			for (size_t i = 0; i < vvy.size(); ++i)
				vvy[i] += 9.81f * dt;
		}

		static uint32_t calculateAliveUnits(const containers::darray<Health>& h) {
			GAIA_PROF_SCOPE(calculateAliveUnits);

			uint32_t aliveUnits = 0;
			for (size_t i = 0; i < h.size(); i++) {
				if (h[i].value > 0)
					++aliveUnits;
			}
			return aliveUnits;
		}
	};

	constexpr size_t NGroup = N / Groups;

	// Create static entities.
	struct static_units_group {
		containers::darray<PositionSoA> units_p{NGroup};
		containers::darray<Rotation> units_r{NGroup};
		containers::darray<Scale> units_s{NGroup};
	} static_groups[Groups];
	for (auto& g: static_groups) {
		for (size_t i = 0; i < NGroup; i++) {
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
		for (size_t i = 0; i < NGroup; i++) {
			g.units_p[i] = {0, 100, 0};
			g.units_r[i] = {1, 2, 3, 4};
			g.units_s[i] = {1, 1, 1};
			g.units_v[i] = {0, 0, 1};
			g.units_d[i] = {0, 0, 1};
			g.units_h[i] = {100, 100};
			g.units_e[i] = {false};
		}
	}

	srand(0);
	for (auto _: state) {
		(void)_;
		dt = CalculateDelta(state);

		for (auto& g: dynamic_groups)
			UnitDynamic::updatePosition(g.units_p, g.units_v);

		for (auto& g: dynamic_groups)
			UnitDynamic::handleGroundCollision(g.units_p, g.units_v);

		for (auto& g: dynamic_groups)
			UnitDynamic::applyGravity(g.units_v);

		uint32_t aliveUnits = 0;
		for (auto& g: dynamic_groups)
			aliveUnits += UnitDynamic::calculateAliveUnits(g.units_h);
		(void)aliveUnits;

		GAIA_PROF_FRAME();
	}
}

template <uint32_t Groups>
void BM_NonECS_DOD_SoA_SIMD(picobench::state& state) {

	struct UnitDynamic {
		static void updatePosition(containers::darray<PositionSoA>& p, const containers::darray<VelocitySoA>& v) {
			GAIA_PROF_SCOPE(updatePosition);

			gaia::utils::auto_view_policy_set<PositionSoA> pv{{std::span(p.data(), p.size())}};
			gaia::utils::auto_view_policy_get<VelocitySoA> vv{{std::span(v.data(), v.size())}};

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
			auto exec2 = [](float* GAIA_RESTRICT p, const float* GAIA_RESTRICT v, size_t offset) {
				p[offset] += v[offset] * dt;
			};

			size_t i = 0;
			// Optimize via "double-read" trick to hide latencies.
			// Item count is always a multiple of 4 for chunks with SoA components.
			for (; i < size; i += 8) {
				exec(ppx.data(), vvx.data(), i);
				exec(ppx.data(), vvx.data(), i + 4);
			}
			for (; i < size; i++)
				exec2(ppx.data(), vvx.data(), i);

			i = 0;
			for (; i < size; i += 8) {
				exec(ppy.data(), vvy.data(), i);
				exec(ppy.data(), vvy.data(), i + 4);
			}
			for (; i < size; i++)
				exec2(ppy.data(), vvy.data(), i);

			i = 0;
			for (; i < size; i += 8) {
				exec(ppz.data(), vvz.data(), i);
				exec(ppz.data(), vvz.data(), i + 4);
			}
			for (; i < size; i++)
				exec2(ppz.data(), vvz.data(), i);
		}

		static void handleGroundCollision(containers::darray<PositionSoA>& p, containers::darray<VelocitySoA>& v) {
			GAIA_PROF_SCOPE(handleGroundCollision);

			gaia::utils::auto_view_policy_set<PositionSoA> pv{{std::span(p.data(), p.size())}};
			gaia::utils::auto_view_policy_set<VelocitySoA> vv{{std::span(v.data(), v.size())}};

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
			auto exec2 = [](float* GAIA_RESTRICT p, float* GAIA_RESTRICT v, size_t offset) {
				if (p[offset] < 0.0f) {
					p[offset] = 0.0f;
					v[offset] = 0.0f;
				}
			};

			size_t i = 0;
			// Optimize via "double-read" trick to hide latencies.
			// Item count is always a multiple of 4 for chunks with SoA components.
			for (; i < size; i += 8) {
				exec(ppy.data(), vvy.data(), i);
				exec(ppy.data(), vvy.data(), i + 4);
			}
			for (; i < size; i++)
				exec2(ppy.data(), vvy.data(), i);
		}

		static void applyGravity(containers::darray<VelocitySoA>& v) {
			GAIA_PROF_SCOPE(applyGravity);

			gaia::utils::auto_view_policy_set<VelocitySoA> vv{{std::span(v.data(), v.size())}};

			auto vvy = vv.set<1>();
			const auto size = v.size();

			const auto gg_gVec = _mm_set_ps1(9.81f);
			const auto gg_dtVec = _mm_set_ps1(dt);

			auto exec = [&](float* GAIA_RESTRICT v, const size_t offset) {
				const auto vyVec = _mm_load_ps(v + offset);
				const auto mulVec = _mm_fmadd_ps(gg_gVec, gg_dtVec, vyVec);
				_mm_store_ps(v + offset, mulVec);
			};
			auto exec2 = [](float* GAIA_RESTRICT v, const size_t offset) {
				v[offset] += 9.81f * dt;
			};

			size_t i = 0;
			// Optimize via "double-read" trick to hide latencies.
			// Item count is always a multiple of 4 for chunks with SoA components.
			for (; i < size; i += 8) {
				exec(vvy.data(), i);
				exec(vvy.data(), i + 4);
			}
			for (; i < size; i++)
				exec2(vvy.data(), i);
		}

		static uint32_t calculateAliveUnits(const containers::darray<Health>& h) {
			GAIA_PROF_SCOPE(calculateAliveUnits);

			uint32_t aliveUnits = 0;
			for (size_t i = 0; i < h.size(); i++) {
				if (h[i].value > 0)
					++aliveUnits;
			}
			return aliveUnits;
		}
	};

	constexpr size_t NGroup = N / Groups;

	// Create static entities.
	struct static_units_group {
		containers::darray<PositionSoA> units_p{NGroup};
		containers::darray<Rotation> units_r{NGroup};
		containers::darray<Scale> units_s{NGroup};
	} static_groups[Groups];
	for (auto& g: static_groups) {
		for (size_t i = 0; i < NGroup; i++) {
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
		for (size_t i = 0; i < NGroup; i++) {
			g.units_p[i] = {0, 100, 0};
			g.units_r[i] = {1, 2, 3, 4};
			g.units_s[i] = {1, 1, 1};
			g.units_v[i] = {0, 0, 1};
			g.units_d[i] = {0, 0, 1};
			g.units_h[i] = {100, 100};
			g.units_e[i] = {false};
		}
	}

	srand(0);
	for (auto _: state) {
		(void)_;
		dt = CalculateDelta(state);

		for (auto& g: dynamic_groups)
			UnitDynamic::updatePosition(g.units_p, g.units_v);

		for (auto& g: dynamic_groups)
			UnitDynamic::handleGroundCollision(g.units_p, g.units_v);

		for (auto& g: dynamic_groups)
			UnitDynamic::applyGravity(g.units_v);

		uint32_t aliveUnits = 0;
		for (auto& g: dynamic_groups)
			aliveUnits += UnitDynamic::calculateAliveUnits(g.units_h);
		(void)aliveUnits;

		GAIA_PROF_FRAME();
	}
}

#define PICO_SETTINGS() iterations({8192}).samples(3)
#define PICO_SETTINGS_1() iterations({8192}).samples(1)
#define PICOBENCH_SUITE_REG(name) r.current_suite_name() = name;
#define PICOBENCH_REG(func) (void)r.add_benchmark(#func, func)

int main(int argc, char* argv[]) {
	picobench::runner r(true);
	r.parse_cmd_line(argc, argv);

	// If picobench encounters an unknown command line argument it returns false and sets an error.
	// Ignore this behavior.
	// We only need to make sure to provide the custom arguments after the picobench ones.
	if (r.error() == picobench::error_unknown_cmd_line_argument)
		r.set_error(picobench::no_error);

	// With Cachegrind enabled we want to be able to pick what benchmark to run so it is easier
	// for us to isolate the results.
	{
		bool cachegrind = false;
		bool test_dod = false;

		const gaia::containers::darray<std::string_view> args(argv + 1, argv + argc);
		for (const auto& arg: args) {
			if (arg == "-cg") {
				cachegrind = true;
				continue;
			}
			if (arg == "-dod") {
				test_dod = true;
				continue;
			}
		}

		if (cachegrind) {
			if (test_dod) {
				// PICOBENCH_SUITE_REG("NonECS_DOD");
				PICOBENCH_REG(BM_NonECS_DOD<80>).PICO_SETTINGS_1().label("DOD_Chunks_80");
			} else {
				// PICOBENCH_SUITE_REG("ECS");
				PICOBENCH_REG(BM_ECS_WithSystems_Iter).PICO_SETTINGS_1().label("Systems_Iter");
			}
		} else {
			//  Ordinary coding style.
			PICOBENCH_REG(BM_NonECS<false>).PICO_SETTINGS().label("Default");
			PICOBENCH_REG(BM_NonECS<true>).PICO_SETTINGS().label("Default2");

			// Ordinary coding style with optimized memory layout (imagine using custom allocators
			// to keep things close and tidy in memory).
			PICOBENCH_REG(BM_NonECS_BetterMemoryLayout<false>).PICO_SETTINGS().label("OptimizedMemLayout");
			PICOBENCH_REG(BM_NonECS_BetterMemoryLayout<true>).PICO_SETTINGS().label("OptimizedMemLayout2");

			// Memory organized in DoD style.
			// Performance target BM_ECS_WithSystems_Iter.
			// "Groups" is there to simulate having items split into separate chunks similar to what ECS does.
			PICOBENCH_SUITE_REG("NonECS_DOD");
			PICOBENCH_REG(BM_NonECS_DOD<1>).PICO_SETTINGS().baseline().label("Default");
			PICOBENCH_REG(BM_NonECS_DOD<20>).PICO_SETTINGS().label("Chunks_20");
			PICOBENCH_REG(BM_NonECS_DOD<40>).PICO_SETTINGS().label("Chunks_40");
			PICOBENCH_REG(BM_NonECS_DOD<80>).PICO_SETTINGS().label("Chunks_80");
			PICOBENCH_REG(BM_NonECS_DOD<160>).PICO_SETTINGS().label("Chunks_160");
			PICOBENCH_REG(BM_NonECS_DOD<200>).PICO_SETTINGS().label("Chunks_200");
			PICOBENCH_REG(BM_NonECS_DOD<320>).PICO_SETTINGS().label("Chunks_320");

			// Best possible performance with no manual optimization.
			// Performance target for BM_ECS_WithSystems_Iter_SoA.
			// "Groups" is there to simulate having items split into separate chunks similar to what ECS does.
			PICOBENCH_SUITE_REG("NonECS_DOD_SoA");
			PICOBENCH_REG(BM_NonECS_DOD_SoA<1>).PICO_SETTINGS().baseline().label("Default");
			PICOBENCH_REG(BM_NonECS_DOD_SoA<20>).PICO_SETTINGS().label("Chunks_20");
			PICOBENCH_REG(BM_NonECS_DOD_SoA<40>).PICO_SETTINGS().label("Chunks_40");
			PICOBENCH_REG(BM_NonECS_DOD_SoA<80>).PICO_SETTINGS().label("Chunks_80");
			PICOBENCH_REG(BM_NonECS_DOD_SoA<160>).PICO_SETTINGS().label("Chunks_160");
			PICOBENCH_REG(BM_NonECS_DOD_SoA<200>).PICO_SETTINGS().label("Chunks_200");
			PICOBENCH_REG(BM_NonECS_DOD_SoA<320>).PICO_SETTINGS().label("Chunks_320");

			// Best possible performance.
			// Performance target for BM_ECS_WithSystems_Iter_SoA_SIMD.
			// "Groups" is there to simulate having items split into separate chunks similar to what ECS does.
			PICOBENCH_SUITE_REG("NonECS_DOD_SoA_SIMD");
			PICOBENCH_REG(BM_NonECS_DOD_SoA_SIMD<1>).PICO_SETTINGS().baseline().label("Default");
			PICOBENCH_REG(BM_NonECS_DOD_SoA_SIMD<20>).PICO_SETTINGS().label("Chunks_20");
			PICOBENCH_REG(BM_NonECS_DOD_SoA_SIMD<40>).PICO_SETTINGS().label("Chunks_40");
			PICOBENCH_REG(BM_NonECS_DOD_SoA_SIMD<80>).PICO_SETTINGS().label("Chunks_80");
			PICOBENCH_REG(BM_NonECS_DOD_SoA_SIMD<160>).PICO_SETTINGS().label("Chunks_160");
			PICOBENCH_REG(BM_NonECS_DOD_SoA_SIMD<200>).PICO_SETTINGS().label("Chunks_200");
			PICOBENCH_REG(BM_NonECS_DOD_SoA_SIMD<320>).PICO_SETTINGS().label("Chunks_320");

			// GaiaECS performance.
			PICOBENCH_SUITE_REG("ECS");
			PICOBENCH_REG(BM_ECS).PICO_SETTINGS().baseline().label("Default");
			PICOBENCH_REG(BM_ECS_WithSystems).PICO_SETTINGS().label("Systems");
			PICOBENCH_REG(BM_ECS_WithSystems_Iter).PICO_SETTINGS().label("Systems_Iter");
			PICOBENCH_REG(BM_ECS_WithSystems_Iter_SoA).PICO_SETTINGS().label("Systems_Iter_SoA");
			PICOBENCH_REG(BM_ECS_WithSystems_Iter_SoA_SIMD).PICO_SETTINGS().label("Systems_Iter_SoA_SIMD");
		}
	}

	return r.run(0);
}
