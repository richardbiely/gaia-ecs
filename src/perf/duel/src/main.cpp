#define PICOBENCH_IMPLEMENT
#include <gaia.h>
#include <picobench/picobench.hpp>
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
		auto e = w.add();
		if constexpr (SoA)
			w.add<PositionSoA>(e, {0, 100, 0});
		else
			w.add<Position>(e, {0, 100, 0});
		w.add<Rotation>(e, {1, 2, 3, 4});
		w.add<Scale>(e, {1, 1, 1});
		GAIA_FOR(N) {
			[[maybe_unused]] auto newentity = w.copy(e);
		}
	}
}

template <bool SoA>
void CreateECSEntities_Dynamic(ecs::World& w) {
	{
		auto e = w.add();
		if constexpr (SoA)
			w.add<PositionSoA>(e, {0, 100, 0});
		else
			w.add<Position>(e, {0, 100, 0});
		w.add<Rotation>(e, {1, 2, 3, 4});
		w.add<Scale>(e, {1, 1, 1});
		if constexpr (SoA)
			w.add<VelocitySoA>(e, {0, 0, 1});
		else
			w.add<Velocity>(e, {0, 0, 1});
		GAIA_FOR(N / 4) {
			[[maybe_unused]] auto newentity = w.copy(e);
		}
	}
	{
		auto e = w.add();
		if constexpr (SoA)
			w.add<PositionSoA>(e, {0, 100, 0});
		else
			w.add<Position>(e, {0, 100, 0});
		w.add<Rotation>(e, {1, 2, 3, 4});
		w.add<Scale>(e, {1, 1, 1});
		if constexpr (SoA)
			w.add<VelocitySoA>(e, {0, 0, 1});
		else
			w.add<Velocity>(e, {0, 0, 1});
		w.add<Direction>(e, {0, 0, 1});
		GAIA_FOR(N / 4) {
			[[maybe_unused]] auto newentity = w.copy(e);
		}
	}
	{
		auto e = w.add();
		if constexpr (SoA)
			w.add<PositionSoA>(e, {0, 100, 0});
		else
			w.add<Position>(e, {0, 100, 0});
		w.add<Rotation>(e, {1, 2, 3, 4});
		w.add<Scale>(e, {1, 1, 1});
		if constexpr (SoA)
			w.add<VelocitySoA>(e, {0, 0, 1});
		else
			w.add<Velocity>(e, {0, 0, 1});
		w.add<Direction>(e, {0, 0, 1});
		w.add<Health>(e, {100, 100});
		GAIA_FOR(N / 4) {
			[[maybe_unused]] auto newentity = w.copy(e);
		}
	}
	{
		auto e = w.add();
		if constexpr (SoA)
			w.add<PositionSoA>(e, {0, 100, 0});
		else
			w.add<Position>(e, {0, 100, 0});
		w.add<Rotation>(e, {1, 2, 3, 4});
		w.add<Scale>(e, {1, 1, 1});
		if constexpr (SoA)
			w.add<VelocitySoA>(e, {0, 0, 1});
		else
			w.add<Velocity>(e, {0, 0, 1});
		w.add<Direction>(e, {0, 0, 1});
		w.add<Health>(e, {100, 100});
		w.add<IsEnemy>(e, {false});
		GAIA_FOR(N / 4) {
			[[maybe_unused]] auto newentity = w.copy(e);
		}
	}
}

void BM_ECS(picobench::state& state) {
	ecs::World w;
	CreateECSEntities_Static<false>(w);
	CreateECSEntities_Dynamic<false>(w);

	auto queryPosCVel = w.query().all<Position&, Velocity>();
	auto queryPosVel = w.query().all<Position&, Velocity&>();
	auto queryVel = w.query().all<Velocity&>();
	auto queryCHealth = w.query().all<Health>();

	/* We want to benchmark the hot-path. In real-world scenarios queries are cached so cache them now */
	gaia::dont_optimize(queryPosCVel.empty());
	gaia::dont_optimize(queryPosVel.empty());
	gaia::dont_optimize(queryVel.empty());
	gaia::dont_optimize(queryCHealth.empty());

	srand(0);
	for (auto _: state) {
		(void)_;
		dt = CalculateDelta(state);

		// Update position
		queryPosCVel.each([&](Position& p, const Velocity& v) {
			p.x += v.x * dt;
			p.y += v.y * dt;
			p.z += v.z * dt;
		});
		// Handle ground collision
		queryPosVel.each([&](Position& p, Velocity& v) {
			if (p.y < 0.0f) {
				p.y = 0.0f;
				v.y = 0.0f;
			}
		});
		// Apply gravity
		queryVel.each([&](Velocity& v) {
			v.y += 9.81f * dt;
		});
		// Calculate the number of units alive
		uint32_t aliveUnits = 0;
		queryCHealth.each([&](const Health& h) {
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
	void init(ecs::Query* q) {
		m_q = q;
	}
};

void BM_ECS_WithSystems(picobench::state& state) {
	ecs::World w;
	CreateECSEntities_Static<false>(w);
	CreateECSEntities_Dynamic<false>(w);

	auto queryPosCVel = w.query().all<Position&, Velocity>();
	auto queryPosVel = w.query().all<Position&, Velocity&>();
	auto queryVel = w.query().all<Velocity&>();
	auto queryCHealth = w.query().all<Health>();

	/* We want to benchmark the hot-path. In real-world scenarios queries are cached so cache them now */
	gaia::dont_optimize(queryPosCVel.empty());
	gaia::dont_optimize(queryPosVel.empty());
	gaia::dont_optimize(queryVel.empty());
	gaia::dont_optimize(queryCHealth.empty());

	class PositionSystem final: public TestSystem {
	public:
		void OnUpdate() override {
			m_q->each([](Position& p, const Velocity& v) {
				p.x += v.x * dt;
				p.y += v.y * dt;
				p.z += v.z * dt;
			});
		}
	};
	class CollisionSystem final: public TestSystem {
	public:
		void OnUpdate() override {
			m_q->each([](Position& p, Velocity& v) {
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
			m_q->each([](Velocity& v) {
				v.y += 9.81f * dt;
			});
		}
	};
	class CalculateAliveUnitsSystem final: public TestSystem {
	public:
		void OnUpdate() override {
			uint32_t aliveUnits = 0;
			m_q->each([&](const Health& h) {
				if (h.value > 0)
					++aliveUnits;
			});
			gaia::dont_optimize(aliveUnits);
		}
	};

	ecs::SystemManager sm(w);
	sm.add<PositionSystem>()->init(&queryPosCVel);
	sm.add<CollisionSystem>()->init(&queryPosVel);
	sm.add<GravitySystem>()->init(&queryVel);
	sm.add<CalculateAliveUnitsSystem>()->init(&queryCHealth);

	srand(0);
	for (auto _: state) {
		(void)_;
		dt = CalculateDelta(state);

		sm.update();
	}
}

void BM_ECS_WithSystems_Iter(picobench::state& state) {
	ecs::World w;
	CreateECSEntities_Static<false>(w);
	CreateECSEntities_Dynamic<false>(w);

	auto queryPosCVel = w.query().all<Position&, Velocity>();
	auto queryPosVel = w.query().all<Position&, Velocity&>();
	auto queryVel = w.query().all<Velocity&>();
	auto queryCHealth = w.query().all<Health>();

	/* We want to benchmark the hot-path. In real-world scenarios queries are cached so cache them now */
	gaia::dont_optimize(queryPosCVel.empty());
	gaia::dont_optimize(queryPosVel.empty());
	gaia::dont_optimize(queryVel.empty());
	gaia::dont_optimize(queryCHealth.empty());

	class PositionSystem final: public TestSystem {
	public:
		void OnUpdate() override {
			m_q->each([](ecs::Iter& it) {
				auto p = it.view_mut<Position>();
				auto v = it.view<Velocity>();

				GAIA_EACH(it) {
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
			m_q->each([](ecs::Iter& it) {
				auto p = it.view_mut<Position>();
				auto v = it.view_mut<Velocity>();

				GAIA_EACH(it) {
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
			m_q->each([](ecs::Iter& it) {
				auto v = it.view_mut<Velocity>();
				GAIA_EACH(it) v[i].y += 9.81f * dt;
			});
		}
	};
	class CalculateAliveUnitsSystem final: public TestSystem {
	public:
		void OnUpdate() override {
			uint32_t aliveUnits = 0;
			m_q->each([&](ecs::Iter& it) {
				auto h = it.view<Health>();

				uint32_t a = 0;
				GAIA_EACH(it) {
					if (h[i].value > 0)
						++a;
				}
				aliveUnits += a;
			});
			gaia::dont_optimize(aliveUnits);
		}
	};

	ecs::SystemManager sm(w);
	sm.add<PositionSystem>()->init(&queryPosCVel);
	sm.add<CollisionSystem>()->init(&queryPosVel);
	sm.add<GravitySystem>()->init(&queryVel);
	sm.add<CalculateAliveUnitsSystem>()->init(&queryCHealth);

	srand(0);
	for (auto _: state) {
		(void)_;
		dt = CalculateDelta(state);

		sm.update();
	}
}

void BM_ECS_WithSystems_Iter_SoA(picobench::state& state) {
	ecs::World w;
	CreateECSEntities_Static<true>(w);
	CreateECSEntities_Dynamic<true>(w);

	auto queryPosCVel = w.query().all<PositionSoA&, VelocitySoA>();
	auto queryPosVel = w.query().all<PositionSoA&, VelocitySoA&>();
	auto queryVel = w.query().all<VelocitySoA&>();
	auto queryCHealth = w.query().all<Health>();

	/* We want to benchmark the hot-path. In real-world scenarios queries are cached so cache them now */
	gaia::dont_optimize(queryPosCVel.empty());
	gaia::dont_optimize(queryPosVel.empty());
	gaia::dont_optimize(queryVel.empty());
	gaia::dont_optimize(queryCHealth.empty());

	class PositionSystem final: public TestSystem {
	public:
		void OnUpdate() override {
			m_q->each([](ecs::Iter& it) {
				auto p = it.view_mut<PositionSoA>();
				auto v = it.view<VelocitySoA>();

				auto ppx = p.set<0>();
				auto ppy = p.set<1>();
				auto ppz = p.set<2>();

				auto vvx = v.get<0>();
				auto vvy = v.get<1>();
				auto vvz = v.get<2>();

				GAIA_EACH(it) ppx[i] += vvx[i] * dt;
				GAIA_EACH(it) ppy[i] += vvy[i] * dt;
				GAIA_EACH(it) ppz[i] += vvz[i] * dt;
			});
		}
	};
	class CollisionSystem final: public TestSystem {
	public:
		void OnUpdate() override {
			m_q->each([](ecs::Iter& it) {
				auto p = it.view_mut<PositionSoA>();
				auto v = it.view_mut<VelocitySoA>();

				auto ppy = p.set<1>();
				auto vvy = v.set<1>();

				GAIA_EACH(it) {
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
			m_q->each([](ecs::Iter& it) {
				auto v = it.view_mut<VelocitySoA>();
				auto vvy = v.set<1>();

				GAIA_EACH(it) vvy[i] += dt * 9.81f;
			});
		}
	};
	class CalculateAliveUnitsSystem final: public TestSystem {
	public:
		void OnUpdate() override {
			uint32_t aliveUnits = 0;
			m_q->each([&](ecs::Iter& it) {
				auto h = it.view<Health>();

				uint32_t a = 0;
				GAIA_EACH(it) {
					if (h[i].value > 0)
						++a;
				}
				aliveUnits += a;
			});
			gaia::dont_optimize(aliveUnits);
		}
	};

	ecs::SystemManager sm(w);
	sm.add<PositionSystem>()->init(&queryPosCVel);
	sm.add<CollisionSystem>()->init(&queryPosVel);
	sm.add<GravitySystem>()->init(&queryVel);
	sm.add<CalculateAliveUnitsSystem>()->init(&queryCHealth);

	srand(0);
	for (auto _: state) {
		(void)_;
		dt = CalculateDelta(state);

		sm.update();
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

		virtual void update_pos(float deltaTime) = 0;
		virtual void updatePosition_verify() {}

		virtual void handle_collision(float deltaTime) = 0;
		virtual void handleGroundCollision_verify() {}

		virtual void apply_gravity(float deltaTime) = 0;
		virtual void applyGravity_verify() {}

		virtual bool isAlive() const = 0;
		virtual void isAlive_verify() {}
	};

	struct UnitStatic: public IUnit {
		void update_pos([[maybe_unused]] float deltaTime) override {}
		void handle_collision([[maybe_unused]] float deltaTime) override {}
		void apply_gravity([[maybe_unused]] float deltaTime) override {}
		bool isAlive() const override {
			return true;
		}
	};

	struct UnitDynamic1: public IUnit {
		Velocity v;

		void update_pos(float deltaTime) override {
			p.x += v.x * deltaTime;
			p.y += v.y * deltaTime;
			p.z += v.z * deltaTime;
		}
		void updatePosition_verify() override {
			gaia::dont_optimize(p.x);
		}

		void handle_collision([[maybe_unused]] float deltaTime) override {
			if (p.y < 0.0f) {
				p.y = 0.0f;
				v.y = 0.0f;
			}
		}
		void handleGroundCollision_verify() override {
			gaia::dont_optimize(v.y);
		}

		void apply_gravity(float deltaTime) override {
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

		void update_pos(float deltaTime) override {
			p.x += v.x * deltaTime;
			p.y += v.y * deltaTime;
			p.z += v.z * deltaTime;
		}
		void updatePosition_verify() override {
			gaia::dont_optimize(p.x);
		}

		void handle_collision([[maybe_unused]] float deltaTime) override {
			if (p.y < 0.0f) {
				p.y = 0.0f;
				v.y = 0.0f;
			}
		}
		void handleGroundCollision_verify() override {
			gaia::dont_optimize(v.y);
		}

		void apply_gravity(float deltaTime) override {
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

		void update_pos(float deltaTime) override {
			p.x += v.x * deltaTime;
			p.y += v.y * deltaTime;
			p.z += v.z * deltaTime;
		}
		void updatePosition_verify() override {
			gaia::dont_optimize(p.x);
		}

		void handle_collision([[maybe_unused]] float deltaTime) override {
			if (p.y < 0.0f) {
				p.y = 0.0f;
				v.y = 0.0f;
			}
		}
		void handleGroundCollision_verify() override {
			gaia::dont_optimize(v.x);
		}

		void apply_gravity(float deltaTime) override {
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

		void update_pos(float deltaTime) override {
			p.x += v.x * deltaTime;
			p.y += v.y * deltaTime;
			p.z += v.z * deltaTime;
		}
		void updatePosition_verify() override {
			gaia::dont_optimize(p.x);
		}

		void handle_collision([[maybe_unused]] float deltaTime) override {
			if (p.y < 0.0f) {
				p.y = 0.0f;
				v.y = 0.0f;
			}
		}
		void handleGroundCollision_verify() override {
			gaia::dont_optimize(v.x);
		}

		void apply_gravity(float deltaTime) override {
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
		GAIA_FOR(N) {
			auto* u = new UnitStatic();
			u->p = {0, 100, 0};
			u->r = {1, 2, 3, 4};
			u->s = {1, 1, 1};
			units[i] = u;
		}
		uint32_t j = N;
		GAIA_FOR(N / 4) {
			auto* u = new UnitDynamic1();
			u->p = {0, 100, 0};
			u->r = {1, 2, 3, 4};
			u->s = {1, 1, 1};
			u->v = {0, 0, 1};
			units[j + i] = u;
		}
		j += N / 4;
		GAIA_FOR(N / 4) {
			auto* u = new UnitDynamic2();
			u->p = {0, 100, 0};
			u->r = {1, 2, 3, 4};
			u->s = {1, 1, 1};
			u->v = {0, 0, 1};
			units[j + i] = u;
		}
		j += N / 4;
		GAIA_FOR(N / 4) {
			auto* u = new UnitDynamic3();
			u->p = {0, 100, 0};
			u->r = {1, 2, 3, 4};
			u->s = {1, 1, 1};
			u->v = {0, 0, 1};
			units[j + i] = u;
		}
		j += N / 4;
		GAIA_FOR(N / 4) {
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
				u->update_pos(dt);
			units[0]->updatePosition_verify();
			for (auto& u: units)
				u->handle_collision(dt);
			units[0]->handleGroundCollision_verify();
			for (auto& u: units)
				u->apply_gravity(dt);
			units[0]->applyGravity_verify();
			for (auto& u: units) {
				if (u->isAlive())
					++aliveUnits;
			}
		} else {
			for (auto& u: units) {
				u->update_pos(dt);
				u->handle_collision(dt);
				u->apply_gravity(dt);
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
		void update_pos([[maybe_unused]] float deltaTime) {}
		void updatePosition_verify() {}

		void handle_collision([[maybe_unused]] float deltaTime) {}
		void handleGroundCollision_verify() {}

		void apply_gravity([[maybe_unused]] float deltaTime) {}
		void applyGravity_verify() {}

		bool isAlive() const {
			return true;
		}
		void isAlive_verify() {}
	};

	struct UnitDynamic1: UnitData {
		Velocity v;

		void update_pos(float deltaTime) {
			p.x += v.x * deltaTime;
			p.y += v.y * deltaTime;
			p.z += v.z * deltaTime;
		}
		void updatePosition_verify() {
			gaia::dont_optimize(p.x);
		}

		void handle_collision([[maybe_unused]] float deltaTime) {
			if (p.y < 0.0f) {
				p.y = 0.0f;
				v.y = 0.0f;
			}
		}
		void handleGroundCollision_verify() {
			gaia::dont_optimize(v.y);
		}

		void apply_gravity(float deltaTime) {
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
	GAIA_FOR(N) {
		UnitStatic u;
		u.p = {0, 100, 0};
		u.r = {1, 2, 3, 4};
		u.s = {1, 1, 1};
		units_static[i] = GAIA_MOV(u);
	}

	cnt::darray<UnitDynamic1> units_dynamic1(N / 4);
	cnt::darray<UnitDynamic2> units_dynamic2(N / 4);
	cnt::darray<UnitDynamic3> units_dynamic3(N / 4);
	cnt::darray<UnitDynamic4> units_dynamic4(N / 4);

	GAIA_FOR(N / 4) {
		UnitDynamic1 u;
		u.p = {0, 100, 0};
		u.r = {1, 2, 3, 4};
		u.s = {1, 1, 1};
		u.v = {0, 0, 1};
		units_dynamic1[i] = GAIA_MOV(u);
	}
	GAIA_FOR(N / 4) {
		UnitDynamic2 u;
		u.p = {0, 100, 0};
		u.r = {1, 2, 3, 4};
		u.s = {1, 1, 1};
		u.v = {0, 0, 1};
		u.d = {0, 0, 1};
		units_dynamic2[i] = GAIA_MOV(u);
	}
	GAIA_FOR(N / 4) {
		UnitDynamic3 u;
		u.p = {0, 100, 0};
		u.r = {1, 2, 3, 4};
		u.v = {0, 0, 1};
		u.s = {1, 1, 1};
		u.d = {0, 0, 1};
		u.h = {100, 100};
		units_dynamic3[i] = GAIA_MOV(u);
	}
	GAIA_FOR(N / 4) {
		UnitDynamic4 u;
		u.p = {0, 100, 0};
		u.r = {1, 2, 3, 4};
		u.s = {1, 1, 1};
		u.v = {0, 0, 1};
		u.d = {0, 0, 1};
		u.h = {100, 100};
		u.e = {false};
		units_dynamic4[i] = GAIA_MOV(u);
	}

	auto exec = [](auto& arr) {
		if constexpr (AlternativeExecOrder) {
			for (auto& u: arr)
				u.update_pos(dt);
			arr[0].updatePosition_verify();

			for (auto& u: arr)
				u.handle_collision(dt);
			arr[0].handleGroundCollision_verify();

			for (auto& u: arr)
				u.apply_gravity(dt);
			arr[0].applyGravity_verify();
		} else {
			for (auto& u: arr) {
				u.update_pos(dt);
				u.handle_collision(dt);
				u.apply_gravity(dt);
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
		static void update_pos(cnt::darray<Position>& p, const cnt::darray<Velocity>& v, float deltaTime) {
			GAIA_EACH(p) {
				p[i].x += v[i].x * deltaTime;
				p[i].y += v[i].y * deltaTime;
				p[i].z += v[i].z * deltaTime;
			}

			gaia::dont_optimize(p[0].x);
			gaia::dont_optimize(p[0].y);
			gaia::dont_optimize(p[0].z);
		}
		static void handle_collision(cnt::darray<Position>& p, cnt::darray<Velocity>& v) {
			GAIA_EACH(p) {
				if (p[i].y < 0.0f) {
					p[i].y = 0.0f;
					v[i].y = 0.0f;
				}
			}

			gaia::dont_optimize(p[0].y);
			gaia::dont_optimize(v[0].y);
		}

		static void apply_gravity(cnt::darray<Velocity>& v, float deltaTime) {
			GAIA_EACH(v) v[i].y += 9.81f * deltaTime;

			gaia::dont_optimize(v[0].y);
		}

		static uint32_t calc_alive_units(const cnt::darray<Health>& h) {
			uint32_t aliveUnits = 0;
			GAIA_EACH(h) {
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
		GAIA_FOR(NGroup) {
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
		GAIA_FOR(NGroup) {
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
			GAIA_PROF_SCOPE(update_pos);

			for (auto& g: dynamic_groups)
				UnitDynamic::update_pos(g.units_p, g.units_v, dt);
		}

		{
			GAIA_PROF_SCOPE(handle_collision);

			for (auto& g: dynamic_groups)
				UnitDynamic::handle_collision(g.units_p, g.units_v);
		}

		{
			GAIA_PROF_SCOPE(apply_gravity);

			for (auto& g: dynamic_groups)
				UnitDynamic::apply_gravity(g.units_v, dt);
		}

		uint32_t aliveUnits = 0;
		for (auto& g: dynamic_groups)
			aliveUnits += UnitDynamic::calc_alive_units(g.units_h);
		gaia::dont_optimize(aliveUnits);

		GAIA_PROF_FRAME();
	}
}

template <uint32_t Groups>
void BM_NonECS_DOD_SoA(picobench::state& state) {
	struct UnitDynamic {
		static void update_pos(cnt::darray<PositionSoA>& p, const cnt::darray<VelocitySoA>& v) {
			GAIA_PROF_SCOPE(update_pos);

			gaia::mem::auto_view_policy_set<PositionSoA> pv{p};
			gaia::mem::auto_view_policy_get<VelocitySoA> vv{v};

			auto ppx = pv.set<0>();
			auto ppy = pv.set<1>();
			auto ppz = pv.set<2>();

			auto vvx = vv.get<0>();
			auto vvy = vv.get<1>();
			auto vvz = vv.get<2>();

			GAIA_EACH(ppx) ppx[i] += vvx[i] * dt;
			GAIA_EACH(ppy) ppy[i] += vvy[i] * dt;
			GAIA_EACH(ppz) ppz[i] += vvz[i] * dt;

			gaia::dont_optimize(ppx[0]);
			gaia::dont_optimize(ppy[0]);
			gaia::dont_optimize(ppz[0]);
		}

		static void handle_collision(cnt::darray<PositionSoA>& p, cnt::darray<VelocitySoA>& v) {
			GAIA_PROF_SCOPE(handle_collision);

			gaia::mem::auto_view_policy_set<PositionSoA> pv{p};
			gaia::mem::auto_view_policy_set<VelocitySoA> vv{v};

			auto ppy = pv.set<1>();
			auto vvy = vv.set<1>();

			GAIA_EACH(ppy) {
				if (ppy[i] < 0.0f) {
					ppy[i] = 0.0f;
					vvy[i] = 0.0f;
				}
			}

			gaia::dont_optimize(ppy[0]);
			gaia::dont_optimize(vvy[0]);
		}

		static void apply_gravity(cnt::darray<VelocitySoA>& v) {
			GAIA_PROF_SCOPE(apply_gravity);

			gaia::mem::auto_view_policy_set<VelocitySoA> vv{v};

			auto vvy = vv.set<1>();

			GAIA_EACH(vvy) vvy[i] += 9.81f * dt;

			gaia::dont_optimize(vvy[0]);
		}

		static uint32_t calc_alive_units(const cnt::darray<Health>& h) {
			GAIA_PROF_SCOPE(calc_alive_units);

			uint32_t aliveUnits = 0;
			GAIA_EACH(h) {
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
		GAIA_FOR(NGroup) {
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
		GAIA_FOR(NGroup) {
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
			UnitDynamic::update_pos(g.units_p, g.units_v);

		for (auto& g: dynamic_groups)
			UnitDynamic::handle_collision(g.units_p, g.units_v);

		for (auto& g: dynamic_groups)
			UnitDynamic::apply_gravity(g.units_v);

		uint32_t aliveUnits = 0;
		for (auto& g: dynamic_groups)
			aliveUnits += UnitDynamic::calc_alive_units(g.units_h);
		gaia::dont_optimize(aliveUnits);

		GAIA_PROF_FRAME();
	}
}

#define PICO_SETTINGS() iterations({1024}).samples(3)
#define PICO_SETTINGS_1() iterations({1024}).samples(1)
#define PICO_SETTINGS_SANI() iterations({8}).samples(1)
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

	// With profiling mode enabled we want to be able to pick what benchmark to run so it is easier
	// for us to isolate the results.
	{
		bool profilingMode = false;
		bool sanitizerMode = false;
		bool dodMode = false;

		const gaia::cnt::darray<std::string_view> args(argv + 1, argv + argc);
		for (const auto& arg: args) {
			if (arg == "-p") {
				profilingMode = true;
				continue;
			}
			if (arg == "-s") {
				sanitizerMode = true;
				continue;
			}
			if (arg == "-dod") {
				dodMode = true;
				continue;
			}
		}

		GAIA_LOG_N("Profiling mode = %s", profilingMode ? "ON" : "OFF");
		GAIA_LOG_N("Sanitizer mode = %s", sanitizerMode ? "ON" : "OFF");
		GAIA_LOG_N("DOD mode       = %s", dodMode ? "ON" : "OFF");

		if (profilingMode) {
			if (dodMode) {
				// PICOBENCH_SUITE_REG("NonECS_DOD");
				PICOBENCH_REG(BM_NonECS_DOD<80>).PICO_SETTINGS_1().label("DOD_Chunks_80");
			} else {
				// PICOBENCH_SUITE_REG("ECS");
				PICOBENCH_REG(BM_ECS_WithSystems_Iter).PICO_SETTINGS_1().label("Systems_Iter");
			}
		} else if (sanitizerMode) {
			PICOBENCH_REG(BM_ECS).PICO_SETTINGS().baseline().label("Default");
			PICOBENCH_REG(BM_ECS_WithSystems).PICO_SETTINGS().label("Systems");
			PICOBENCH_REG(BM_ECS_WithSystems_Iter).PICO_SETTINGS().label("Systems_Iter");
			PICOBENCH_REG(BM_ECS_WithSystems_Iter_SoA).PICO_SETTINGS().label("Systems_Iter_SoA");
		} else {
			PICOBENCH_SUITE_REG("OOP");
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
			PICOBENCH_SUITE_REG("DOD");
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
			PICOBENCH_SUITE_REG("DOD_SoA");
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
