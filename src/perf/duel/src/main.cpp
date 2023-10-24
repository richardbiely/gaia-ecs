#define PICOBENCH_IMPLEMENT
#include "gaia/external/picobench.hpp"
#include <gaia.h>
#include <string_view>

using namespace gaia;

float dt;

struct Position {
	float x, y, z;
};
struct PositionSoA {
	float x, y, z;
	static constexpr auto Layout = mem::DataLayout::SoA;
};
struct Velocity {
	float x, y, z;
};
struct VelocitySoA {
	float x, y, z;
	static constexpr auto Layout = mem::DataLayout::SoA;
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

constexpr uint32_t N = 32'000; // kept a multiple of 32 to keep it simple even for SIMD code
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
		auto e = w.Add();
		if constexpr (SoA)
			w.Add<PositionSoA>(e, {0, 100, 0});
		else
			w.Add<Position>(e, {0, 100, 0});
		w.Add<Rotation>(e, {1, 2, 3, 4});
		w.Add<Scale>(e, {1, 1, 1});
		for (uint32_t i = 0; i < N; ++i) {
			[[maybe_unused]] auto newentity = w.Add(e);
		}
	}
}

template <bool SoA>
void CreateECSEntities_Dynamic(ecs::World& w) {
	{
		auto e = w.Add();
		if constexpr (SoA)
			w.Add<PositionSoA>(e, {0, 100, 0});
		else
			w.Add<Position>(e, {0, 100, 0});
		w.Add<Rotation>(e, {1, 2, 3, 4});
		w.Add<Scale>(e, {1, 1, 1});
		if constexpr (SoA)
			w.Add<VelocitySoA>(e, {0, 0, 1});
		else
			w.Add<Velocity>(e, {0, 0, 1});
		for (uint32_t i = 0; i < N / 4; ++i) {
			[[maybe_unused]] auto newentity = w.Add(e);
		}
	}
	{
		auto e = w.Add();
		if constexpr (SoA)
			w.Add<PositionSoA>(e, {0, 100, 0});
		else
			w.Add<Position>(e, {0, 100, 0});
		w.Add<Rotation>(e, {1, 2, 3, 4});
		w.Add<Scale>(e, {1, 1, 1});
		if constexpr (SoA)
			w.Add<VelocitySoA>(e, {0, 0, 1});
		else
			w.Add<Velocity>(e, {0, 0, 1});
		w.Add<Direction>(e, {0, 0, 1});
		for (uint32_t i = 0; i < N / 4; ++i) {
			[[maybe_unused]] auto newentity = w.Add(e);
		}
	}
	{
		auto e = w.Add();
		if constexpr (SoA)
			w.Add<PositionSoA>(e, {0, 100, 0});
		else
			w.Add<Position>(e, {0, 100, 0});
		w.Add<Rotation>(e, {1, 2, 3, 4});
		w.Add<Scale>(e, {1, 1, 1});
		if constexpr (SoA)
			w.Add<VelocitySoA>(e, {0, 0, 1});
		else
			w.Add<Velocity>(e, {0, 0, 1});
		w.Add<Direction>(e, {0, 0, 1});
		w.Add<Health>(e, {100, 100});
		for (uint32_t i = 0; i < N / 4; ++i) {
			[[maybe_unused]] auto newentity = w.Add(e);
		}
	}
	{
		auto e = w.Add();
		if constexpr (SoA)
			w.Add<PositionSoA>(e, {0, 100, 0});
		else
			w.Add<Position>(e, {0, 100, 0});
		w.Add<Rotation>(e, {1, 2, 3, 4});
		w.Add<Scale>(e, {1, 1, 1});
		if constexpr (SoA)
			w.Add<VelocitySoA>(e, {0, 0, 1});
		else
			w.Add<Velocity>(e, {0, 0, 1});
		w.Add<Direction>(e, {0, 0, 1});
		w.Add<Health>(e, {100, 100});
		w.Add<IsEnemy>(e, {false});
		for (uint32_t i = 0; i < N / 4; ++i) {
			[[maybe_unused]] auto newentity = w.Add(e);
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
		gaia::dont_optimize(aliveUnits);

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
			gaia::dont_optimize(aliveUnits);
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

				iter.each([&](uint32_t i) {
					p[i].x += v[i].x * dt;
					p[i].y += v[i].y * dt;
					p[i].z += v[i].z * dt;
				});
			});
		}
	};

	class CollisionSystem final: public TestSystem {
	public:
		void OnUpdate() override {
			m_q->ForEach([](ecs::Iterator iter) {
				auto p = iter.ViewRW<Position>();
				auto v = iter.ViewRW<Velocity>();

				iter.each([&](uint32_t i) {
					if (p[i].y < 0.0f) {
						p[i].y = 0.0f;
						v[i].y = 0.0f;
					}
				});
			});
		}
	};
	class GravitySystem final: public TestSystem {
	public:
		void OnUpdate() override {
			m_q->ForEach([](ecs::Iterator iter) {
				auto v = iter.ViewRW<Velocity>();

				iter.each([&](uint32_t i) {
					v[i].y += 9.81f * dt;
				});
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
				iter.each([&](uint32_t i) {
					if (h[i].value > 0)
						++a;
				});
				aliveUnits += a;
			});
			gaia::dont_optimize(aliveUnits);
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

				iter.each([&](uint32_t i) {
					ppx[i] += vvx[i] * dt;
				});
				iter.each([&](uint32_t i) {
					ppy[i] += vvy[i] * dt;
				});
				iter.each([&](uint32_t i) {
					ppz[i] += vvz[i] * dt;
				});
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

				iter.each([&](uint32_t i) {
					if (ppy[i] < 0.0f) {
						ppy[i] = 0.0f;
						vvy[i] = 0.0f;
					}
				});
			});
		}
	};
	class GravitySystem final: public TestSystem {
	public:
		void OnUpdate() override {
			m_q->ForEach([](ecs::Iterator iter) {
				auto v = iter.ViewRW<VelocitySoA>();
				auto vvy = v.set<1>();

				iter.each([&](uint32_t i) {
					vvy[i] += dt * 9.81f;
				});
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
				iter.each([&](uint32_t i) {
					if (h[i].value > 0)
						++a;
				});
				aliveUnits += a;
			});
			gaia::dont_optimize(aliveUnits);
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

		IUnit() noexcept = default;
		virtual ~IUnit() = default;

		IUnit(const IUnit&) = default;
		IUnit(IUnit&&) noexcept = default;
		IUnit& operator=(const IUnit&) = default;
		IUnit& operator=(IUnit&&) noexcept = default;

		virtual void updatePosition(float deltaTime) = 0;
		virtual void updatePosition_verify() {}

		virtual void handleGroundCollision(float deltaTime) = 0;
		virtual void handleGroundCollision_verify() {}

		virtual void applyGravity(float deltaTime) = 0;
		virtual void applyGravity_verify() {}

		virtual bool isAlive() const = 0;
		virtual void isAlive_verify() {}
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
		void updatePosition_verify() override {
			gaia::dont_optimize(p.x);
		}

		void handleGroundCollision([[maybe_unused]] float deltaTime) override {
			if (p.y < 0.0f) {
				p.y = 0.0f;
				v.y = 0.0f;
			}
		}
		void handleGroundCollision_verify() override {
			gaia::dont_optimize(v.y);
		}

		void applyGravity(float deltaTime) override {
			v.y += 9.81f * deltaTime;
		}
		void applyGravity_verify() override {
			gaia::dont_optimize(v.y);
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
		void updatePosition_verify() override {
			gaia::dont_optimize(p.x);
		}

		void handleGroundCollision([[maybe_unused]] float deltaTime) override {
			if (p.y < 0.0f) {
				p.y = 0.0f;
				v.y = 0.0f;
			}
		}
		void handleGroundCollision_verify() override {
			gaia::dont_optimize(v.y);
		}

		void applyGravity(float deltaTime) override {
			v.y += 9.81f * deltaTime;
		}
		void applyGravity_verify() override {
			gaia::dont_optimize(v.y);
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
		void updatePosition_verify() override {
			gaia::dont_optimize(p.x);
		}

		void handleGroundCollision([[maybe_unused]] float deltaTime) override {
			if (p.y < 0.0f) {
				p.y = 0.0f;
				v.y = 0.0f;
			}
		}
		void handleGroundCollision_verify() override {
			gaia::dont_optimize(v.x);
		}

		void applyGravity(float deltaTime) override {
			v.y += 9.81f * deltaTime;
		}
		void applyGravity_verify() override {
			gaia::dont_optimize(v.y);
		}

		bool isAlive() const override {
			return h.value > 0;
		}
		void isAlive_verify() override {
			gaia::dont_optimize(h.value);
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
		void updatePosition_verify() override {
			gaia::dont_optimize(p.x);
		}

		void handleGroundCollision([[maybe_unused]] float deltaTime) override {
			if (p.y < 0.0f) {
				p.y = 0.0f;
				v.y = 0.0f;
			}
		}
		void handleGroundCollision_verify() override {
			gaia::dont_optimize(v.x);
		}

		void applyGravity(float deltaTime) override {
			v.y += 9.81f * deltaTime;
		}
		void applyGravity_verify() override {
			gaia::dont_optimize(v.y);
		}

		bool isAlive() const override {
			return h.value > 0;
		}
		void isAlive_verify() override {
			gaia::dont_optimize(h.value);
		}
	};
} // namespace NonECS

template <bool AlternativeExecOrder>
void BM_NonECS(picobench::state& state) {
	using namespace NonECS;

	// Create entities.
	// We allocate via new to simulate the usual kind of behavior in games
	cnt::darray<IUnit*> units(N * 2);
	{
		for (uint32_t i = 0; i < N; ++i) {
			auto* u = new UnitStatic();
			u->p = {0, 100, 0};
			u->r = {1, 2, 3, 4};
			u->s = {1, 1, 1};
			units[i] = u;
		}
		uint32_t j = N;
		for (uint32_t i = 0; i < N / 4; ++i) {
			auto* u = new UnitDynamic1();
			u->p = {0, 100, 0};
			u->r = {1, 2, 3, 4};
			u->s = {1, 1, 1};
			u->v = {0, 0, 1};
			units[j + i] = u;
		}
		j += N / 4;
		for (uint32_t i = 0; i < N / 4; ++i) {
			auto* u = new UnitDynamic2();
			u->p = {0, 100, 0};
			u->r = {1, 2, 3, 4};
			u->s = {1, 1, 1};
			u->v = {0, 0, 1};
			units[j + i] = u;
		}
		j += N / 4;
		for (uint32_t i = 0; i < N / 4; ++i) {
			auto* u = new UnitDynamic3();
			u->p = {0, 100, 0};
			u->r = {1, 2, 3, 4};
			u->s = {1, 1, 1};
			u->v = {0, 0, 1};
			units[j + i] = u;
		}
		j += N / 4;
		for (uint32_t i = 0; i < N / 4; ++i) {
			auto* u = new UnitDynamic4();
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
			units[0]->updatePosition_verify();
			for (auto& u: units)
				u->handleGroundCollision(dt);
			units[0]->handleGroundCollision_verify();
			for (auto& u: units)
				u->applyGravity(dt);
			units[0]->applyGravity_verify();
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
			units[0]->updatePosition_verify();
			units[0]->handleGroundCollision_verify();
			units[0]->applyGravity_verify();
		}
		gaia::dont_optimize(aliveUnits);

		GAIA_PROF_FRAME();
	}

	for (const auto& u: units) {
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
		void updatePosition_verify() {}

		void handleGroundCollision([[maybe_unused]] float deltaTime) {}
		void handleGroundCollision_verify() {}

		void applyGravity([[maybe_unused]] float deltaTime) {}
		void applyGravity_verify() {}

		bool isAlive() const {
			return true;
		}
		void isAlive_verify() {}
	};

	struct UnitDynamic1: UnitData {
		Velocity v;

		void updatePosition(float deltaTime) {
			p.x += v.x * deltaTime;
			p.y += v.y * deltaTime;
			p.z += v.z * deltaTime;
		}
		void updatePosition_verify() {
			gaia::dont_optimize(p.x);
		}

		void handleGroundCollision([[maybe_unused]] float deltaTime) {
			if (p.y < 0.0f) {
				p.y = 0.0f;
				v.y = 0.0f;
			}
		}
		void handleGroundCollision_verify() {
			gaia::dont_optimize(v.y);
		}

		void applyGravity(float deltaTime) {
			v.y += 9.81f * deltaTime;
		}
		void applyGravity_verify() {
			gaia::dont_optimize(v.y);
		}

		bool isAlive() const {
			return true;
		}
		void isAlive_verify() {}
	};

	struct UnitDynamic2: public UnitDynamic1 {
		Direction d;
	};

	struct UnitDynamic3: public UnitDynamic2 {
		Health h;

		using UnitDynamic2::isAlive;
		using UnitDynamic2 ::isAlive_verify;
		bool isAlive() const {
			return h.value > 0;
		}
		void isAlive_verify() {
			gaia::dont_optimize(h.value);
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
	cnt::darray<UnitStatic> units_static(N);
	for (uint32_t i = 0; i < N; ++i) {
		UnitStatic u;
		u.p = {0, 100, 0};
		u.r = {1, 2, 3, 4};
		u.s = {1, 1, 1};
		units_static[i] = std::move(u);
	}

	cnt::darray<UnitDynamic1> units_dynamic1(N / 4);
	cnt::darray<UnitDynamic2> units_dynamic2(N / 4);
	cnt::darray<UnitDynamic3> units_dynamic3(N / 4);
	cnt::darray<UnitDynamic4> units_dynamic4(N / 4);

	for (uint32_t i = 0; i < N / 4; ++i) {
		UnitDynamic1 u;
		u.p = {0, 100, 0};
		u.r = {1, 2, 3, 4};
		u.s = {1, 1, 1};
		u.v = {0, 0, 1};
		units_dynamic1[i] = std::move(u);
	}
	for (uint32_t i = 0; i < N / 4; ++i) {
		UnitDynamic2 u;
		u.p = {0, 100, 0};
		u.r = {1, 2, 3, 4};
		u.s = {1, 1, 1};
		u.v = {0, 0, 1};
		u.d = {0, 0, 1};
		units_dynamic2[i] = std::move(u);
	}
	for (uint32_t i = 0; i < N / 4; ++i) {
		UnitDynamic3 u;
		u.p = {0, 100, 0};
		u.r = {1, 2, 3, 4};
		u.v = {0, 0, 1};
		u.s = {1, 1, 1};
		u.d = {0, 0, 1};
		u.h = {100, 100};
		units_dynamic3[i] = std::move(u);
	}
	for (uint32_t i = 0; i < N / 4; ++i) {
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
			arr[0].updatePosition_verify();

			for (auto& u: arr)
				u.handleGroundCollision(dt);
			arr[0].handleGroundCollision_verify();

			for (auto& u: arr)
				u.applyGravity(dt);
			arr[0].applyGravity_verify();
		} else {
			for (auto& u: arr) {
				u.updatePosition(dt);
				u.handleGroundCollision(dt);
				u.applyGravity(dt);
			}
			arr[0].updatePosition_verify();
			arr[0].handleGroundCollision_verify();
			arr[0].applyGravity_verify();
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
		gaia::dont_optimize(aliveUnits);

		GAIA_PROF_FRAME();
	}
} // namespace BM_NonECS_BetterMemoryLayout(picobench::state

template <uint32_t Groups>
void BM_NonECS_DOD(picobench::state& state) {
	struct UnitDynamic {
		static void updatePosition(cnt::darray<Position>& p, const cnt::darray<Velocity>& v, float deltaTime) {
			for (uint32_t i = 0; i < p.size(); ++i) {
				p[i].x += v[i].x * deltaTime;
				p[i].y += v[i].y * deltaTime;
				p[i].z += v[i].z * deltaTime;
			}

			gaia::dont_optimize(p[0].x);
			gaia::dont_optimize(p[0].y);
			gaia::dont_optimize(p[0].z);
		}
		static void handleGroundCollision(cnt::darray<Position>& p, cnt::darray<Velocity>& v) {
			for (uint32_t i = 0; i < p.size(); ++i) {
				if (p[i].y < 0.0f) {
					p[i].y = 0.0f;
					v[i].y = 0.0f;
				}
			}

			gaia::dont_optimize(p[0].y);
			gaia::dont_optimize(v[0].y);
		}

		static void applyGravity(cnt::darray<Velocity>& v, float deltaTime) {
			for (uint32_t i = 0; i < v.size(); ++i)
				v[i].y += 9.81f * deltaTime;

			gaia::dont_optimize(v[0].y);
		}

		static uint32_t calculateAliveUnits(const cnt::darray<Health>& h) {
			uint32_t aliveUnits = 0;
			for (uint32_t i = 0; i < h.size(); ++i) {
				if (h[i].value > 0)
					++aliveUnits;
			}
			return aliveUnits;
		}
	};

	constexpr uint32_t NGroup = N / Groups;

	// Create static entities.
	struct static_units_group {
		cnt::darray<Position> units_p{NGroup};
		cnt::darray<Rotation> units_r{NGroup};
		cnt::darray<Scale> units_s{NGroup};
	} static_groups[Groups];
	for (auto& g: static_groups) {
		for (uint32_t i = 0; i < NGroup; ++i) {
			g.units_p[i] = {0, 100, 0};
			g.units_r[i] = {1, 2, 3, 4};
			g.units_s[i] = {1, 1, 1};
		}
	}

	// Create dynamic entities.
	struct dynamic_units_group {
		cnt::darray<Position> units_p{NGroup};
		cnt::darray<Rotation> units_r{NGroup};
		cnt::darray<Scale> units_s{NGroup};
		cnt::darray<Velocity> units_v{NGroup};
		cnt::darray<Direction> units_d{NGroup};
		cnt::darray<Health> units_h{NGroup};
		cnt::darray<IsEnemy> units_e{NGroup};
	} dynamic_groups[Groups];
	for (auto& g: dynamic_groups) {
		for (uint32_t i = 0; i < NGroup; ++i) {
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
		gaia::dont_optimize(aliveUnits);

		GAIA_PROF_FRAME();
	}
}

template <uint32_t Groups>
void BM_NonECS_DOD_SoA(picobench::state& state) {
	struct UnitDynamic {
		static void updatePosition(cnt::darray<PositionSoA>& p, const cnt::darray<VelocitySoA>& v) {
			GAIA_PROF_SCOPE(updatePosition);

			gaia::mem::auto_view_policy_set<PositionSoA> pv{p};
			gaia::mem::auto_view_policy_get<VelocitySoA> vv{v};

			auto ppx = pv.set<0>();
			auto ppy = pv.set<1>();
			auto ppz = pv.set<2>();

			auto vvx = vv.get<0>();
			auto vvy = vv.get<1>();
			auto vvz = vv.get<2>();

			for (uint32_t i = 0; i < ppx.size(); ++i)
				ppx[i] += vvx[i] * dt;
			for (uint32_t i = 0; i < ppy.size(); ++i)
				ppy[i] += vvy[i] * dt;
			for (uint32_t i = 0; i < ppz.size(); ++i)
				ppz[i] += vvz[i] * dt;

			gaia::dont_optimize(ppx[0]);
			gaia::dont_optimize(ppy[0]);
			gaia::dont_optimize(ppz[0]);
		}

		static void handleGroundCollision(cnt::darray<PositionSoA>& p, cnt::darray<VelocitySoA>& v) {
			GAIA_PROF_SCOPE(handleGroundCollision);

			gaia::mem::auto_view_policy_set<PositionSoA> pv{p};
			gaia::mem::auto_view_policy_set<VelocitySoA> vv{v};

			auto ppy = pv.set<1>();
			auto vvy = vv.set<1>();

			for (uint32_t i = 0; i < ppy.size(); ++i) {
				if (ppy[i] < 0.0f) {
					ppy[i] = 0.0f;
					vvy[i] = 0.0f;
				}
			}

			gaia::dont_optimize(ppy[0]);
			gaia::dont_optimize(vvy[0]);
		}

		static void applyGravity(cnt::darray<VelocitySoA>& v) {
			GAIA_PROF_SCOPE(applyGravity);

			gaia::mem::auto_view_policy_set<VelocitySoA> vv{v};

			auto vvy = vv.set<1>();

			for (uint32_t i = 0; i < vvy.size(); ++i)
				vvy[i] += 9.81f * dt;

			gaia::dont_optimize(vvy[0]);
		}

		static uint32_t calculateAliveUnits(const cnt::darray<Health>& h) {
			GAIA_PROF_SCOPE(calculateAliveUnits);

			uint32_t aliveUnits = 0;
			for (uint32_t i = 0; i < h.size(); ++i) {
				if (h[i].value > 0)
					++aliveUnits;
			}
			return aliveUnits;
		}
	};

	constexpr uint32_t NGroup = N / Groups;

	// Create static entities.
	struct static_units_group {
		cnt::darray<PositionSoA> units_p{NGroup};
		cnt::darray<Rotation> units_r{NGroup};
		cnt::darray<Scale> units_s{NGroup};
	} static_groups[Groups];
	for (auto& g: static_groups) {
		for (uint32_t i = 0; i < NGroup; ++i) {
			g.units_p[i] = {0, 100, 0};
			g.units_r[i] = {1, 2, 3, 4};
			g.units_s[i] = {1, 1, 1};
		}
	}

	// Create dynamic entities.
	struct dynamic_units_group {
		cnt::darray<PositionSoA> units_p{NGroup};
		cnt::darray<Rotation> units_r{NGroup};
		cnt::darray<Scale> units_s{NGroup};
		cnt::darray<VelocitySoA> units_v{NGroup};
		cnt::darray<Direction> units_d{NGroup};
		cnt::darray<Health> units_h{NGroup};
		cnt::darray<IsEnemy> units_e{NGroup};
	} dynamic_groups[Groups];
	for (auto& g: dynamic_groups) {
		for (uint32_t i = 0; i < NGroup; ++i) {
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
		gaia::dont_optimize(aliveUnits);

		GAIA_PROF_FRAME();
	}
}

#define PICO_SETTINGS() iterations({1024}).samples(3)
#define PICO_SETTINGS_1() iterations({1024}).samples(1)
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

		const gaia::cnt::darray<std::string_view> args(argv + 1, argv + argc);
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

		GAIA_LOG_N("CacheGrind = %d", cachegrind);
		GAIA_LOG_N("DOD mode   = %d", test_dod);

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

			// Best possible performance with no manual SIMD optimization.
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

			// GaiaECS performance.
			PICOBENCH_SUITE_REG("ECS");
			PICOBENCH_REG(BM_ECS).PICO_SETTINGS().baseline().label("Default");
			PICOBENCH_REG(BM_ECS_WithSystems).PICO_SETTINGS().label("Systems");
			PICOBENCH_REG(BM_ECS_WithSystems_Iter).PICO_SETTINGS().label("Systems_Iter");
			PICOBENCH_REG(BM_ECS_WithSystems_Iter_SoA).PICO_SETTINGS().label("Systems_Iter_SoA");
		}
	}

	return r.run(0);
}
