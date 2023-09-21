#include <gaia.h>

using namespace gaia;

struct Position {
	float x, y, z;
};
struct Velocity {
	float x, y, z;
};

class PositionSystem final: public ecs::System {
	ecs::Query m_q;

public:
	void OnCreated() override {
		m_q = GetWorld().CreateQuery().All<Position, const Velocity>();
	}

	void OnUpdate() override {
		m_q.ForEach([](Position& p, const Velocity& v) {
			const float dt = 0.01f;
			p.x += v.x * dt;
			p.y += v.y * dt;
			p.z += v.z * dt;
		});
	}
};

class PositionSystem_All final: public ecs::System {
	ecs::Query m_q;

public:
	void OnCreated() override {
		m_q = GetWorld().CreateQuery().All<Position, const Velocity>();
	}

	void OnUpdate() override {
		m_q.ForEach([](ecs::Iterator iter) {
			auto p = iter.ViewRW<Position>();
			auto v = iter.View<Velocity>();
			const float dt = 0.01f;
			for (auto i: iter) {
				p[i].x += v[i].x * dt;
				p[i].y += v[i].y * dt;
				p[i].z += v[i].z * dt;
			}

			if (iter.IsEntityEnabled(0))
				p[0].x += 1.f;
		});
	}
};

class PositionSystem_All2 final: public ecs::System {
	ecs::Query m_q;

public:
	void OnCreated() override {
		m_q = GetWorld().CreateQuery().All<Position, const Velocity>();
	}

	void OnUpdate() override {
		m_q.ForEach([](ecs::Iterator iter) {
			auto p = iter.ViewRW<Position>();
			auto v = iter.View<Velocity>();
			const float dt = 0.01f;
			for (uint32_t i = 0; i < iter.size(); ++i) {
				p[i].x += v[i].x * dt;
				p[i].y += v[i].y * dt;
				p[i].z += v[i].z * dt;
			}

			if (iter.IsEntityEnabled(0))
				p[0].x += 1.f;
		});
	}
};

class PositionSystem_EnabledOnly final: public ecs::System {
	ecs::Query m_q;

public:
	void OnCreated() override {
		m_q = GetWorld().CreateQuery().All<Position, const Velocity>();
	}

	void OnUpdate() override {
		m_q.ForEach([](ecs::IteratorEnabled iter) {
			auto p = iter.ViewRW<Position>();
			auto v = iter.View<Velocity>();
			const float dt = 0.01f;
			for (auto i: iter) {
				p[i].x += v[i].x * dt;
				p[i].y += v[i].y * dt;
				p[i].z += v[i].z * dt;
			}
		});
	}
};

void CreateEntities(ecs::World& w) {
	{
		auto e = w.CreateEntity();
		w.AddComponent<Position>(e, {0, 100, 0});
		w.AddComponent<Velocity>(e, {1, 0, 0});

		constexpr uint32_t N = 1000;
		for (size_t i = 0; i < N; i++) {
			[[maybe_unused]] auto newentity = w.CreateEntity(e);
		}
	}
}

int main() {
	ecs::World w;
	CreateEntities(w);

	ecs::SystemManager sm(w);
	sm.CreateSystem<PositionSystem>();
	sm.CreateSystem<PositionSystem_All>();
	sm.CreateSystem<PositionSystem_All2>();
	sm.CreateSystem<PositionSystem_EnabledOnly>();
	for (uint32_t i = 0; i < 1000; ++i) {
		sm.Update();
		w.Update();
	}

	return 0;
}