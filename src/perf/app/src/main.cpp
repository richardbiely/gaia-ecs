// Benchmark inspired by https://github.com/abeimler/ecs_benchmark.

#define PICOBENCH_IMPLEMENT
#include <gaia.h>
#include <picobench/picobench.hpp>
#include <string_view>

#include "random.h"

using namespace gaia;

float dt;

inline static constexpr uint32_t FrameBufferWidth = 320;
inline static constexpr uint32_t FrameBufferHeight = 240;
inline static constexpr uint32_t SpawAreaMaxX = 320;
inline static constexpr uint32_t SpawAreaMaxY = 240;
inline static constexpr uint32_t SpawAreaMargin = 100;

namespace components {
	inline static constexpr char PlayerSprite = '@';
	inline static constexpr char MonsterSprite = 'k';
	inline static constexpr char NPCSprite = 'h';
	inline static constexpr char GraveSprite = '|';
	inline static constexpr char SpawnSprite = '_';
	inline static constexpr char NoneSprite = ' ';

	enum class PlayerType { NPC, Monster, Hero };

	struct PlayerComponent {
		rnd::random_xoshiro128 rng{};
		PlayerType type{PlayerType::NPC};
	};

	enum class StatusEffect { Spawn, Dead, Alive };
	struct HealthComponent {
		int32_t hp{0};
		int32_t maxhp{0};
		StatusEffect status{StatusEffect::Spawn};
	};

	struct DamageComponent {
		int32_t atk{0};
		int32_t def{0};
	};

	struct DataComponent {
		inline static constexpr uint32_t DefaultSeed = 340383L;

		int thingy{0};
		double dingy{0.0};
		bool mingy{false};

		uint32_t seed{DefaultSeed};
		rnd::random_xoshiro128 rng;
		uint32_t numgy;

		DataComponent(): rng(seed), numgy(rng()) {}
	};

	struct SpriteComponent {
		char character{NoneSprite};
	};

	struct PositionComponent {
		float x{0.0f};
		float y{0.0f};
	};

	struct SoAPositionComponent {
		float x{0.0f};
		float y{0.0f};

		static constexpr auto Layout = ::gaia::mem::DataLayout::SoA;
	};

	struct VelocityComponent {
		float x{1.0f};
		float y{1.0f};
	};

	struct SoAVelocityComponent {
		float x{1.0f};
		float y{1.0f};

		static constexpr auto Layout = ::gaia::mem::DataLayout::SoA;
	};
} // namespace components

constexpr float MinDelta = 0.01f;
constexpr float MaxDelta = 0.033f;

float CalculateDelta(picobench::state& state) {
	state.stop_timer();
	const float d = static_cast<float>((double)RAND_MAX / (MaxDelta - MinDelta));
	float delta = MinDelta + (static_cast<float>(rand()) / d);
	state.start_timer();
	return delta;
}

class FrameBuffer {
public:
	FrameBuffer(uint32_t w, uint32_t h): m_width{w}, m_height{h}, m_buffer(m_width * m_height) {}

	[[nodiscard]] auto width() const noexcept {
		return m_width;
	}
	[[nodiscard]] auto height() const noexcept {
		return m_height;
	}

	void draw(int x, int y, char c) {
		if (y >= 0 && y < (int)m_height) {
			if (x >= 0 && x < (int)m_width) {
				const auto xx = (uint32_t)x;
				const auto yy = (uint32_t)y;
				m_buffer[xx + yy * m_width] = c;
			}
		}
	}

private:
	uint32_t m_width;
	uint32_t m_height;
	cnt::darray<char> m_buffer;
};

namespace sysbase {
	using namespace ::components;

	void updateHealth(HealthComponent& health) {
		if (health.hp <= 0 && health.status != StatusEffect::Dead) {
			health.hp = 0;
			health.status = StatusEffect::Dead;
		} else if (health.status == StatusEffect::Dead && health.hp == 0) {
			health.hp = health.maxhp;
			health.status = StatusEffect::Spawn;
		} else if (health.hp >= health.maxhp && health.status != StatusEffect::Alive) {
			health.hp = health.maxhp;
			health.status = StatusEffect::Alive;
		} else {
			health.status = StatusEffect::Alive;
		}
	}

	void renderSprite(FrameBuffer& fb, float x, float y, const SpriteComponent& spr) {
		fb.draw((int)x, (int)y, spr.character);
	}

	void updateSprite(::components::SpriteComponent& spr, const PlayerComponent& player, const HealthComponent& health) {
		spr.character = [&]() {
			switch (health.status) {
				case StatusEffect::Alive:
					switch (player.type) {
						case PlayerType::Hero:
							return PlayerSprite;
						case PlayerType::Monster:
							return MonsterSprite;
						case PlayerType::NPC:
							return NPCSprite;
					}
					break;
				case StatusEffect::Dead:
					return GraveSprite;
				case StatusEffect::Spawn:
					return SpawnSprite;
			}
			return NoneSprite;
		}();
	}
} // namespace sysbase

namespace sys {
	using namespace ::sysbase;
	using namespace ::gaia;
	using namespace ::components;

	class DamageSystem final: public ecs::System {
	public:
		void OnCreated() override {
			m_q = world().query().all<HealthComponent&, DamageComponent>();
#if GAIA_DEBUG
			GAIA_LOG_N("DamageSystem: %u", m_q.count());
#endif
		}
		void OnUpdate() override {
			m_q.each([&](ecs::Iter& it) {
				auto health = it.view_mut<HealthComponent>(0);
				auto damage = it.view<DamageComponent>(1);
				auto update = [](HealthComponent& health, const DamageComponent& damage) {
					const int totalDamage = core::get_max(damage.atk - damage.def, 0);
					health.hp -= totalDamage;
				};
				GAIA_EACH(it) update(health[i], damage[i]);
			});
		}

	private:
		ecs::Query m_q;
	};

	class DataSystem final: public ecs::System {
	public:
		void OnCreated() override {
			m_q = world().query().all<DataComponent&>();
#if GAIA_DEBUG
			GAIA_LOG_N("DataComponent: %u", m_q.count());
#endif
		}

		void OnUpdate() override {
			m_q.each([&](ecs::Iter& it) {
				auto data = it.view_mut<DataComponent>(0);
				auto update = [](DataComponent& data) {
					data.thingy = (data.thingy + 1) % 1'000'000;
					data.dingy += 0.0001f * dt;
					data.mingy = !data.mingy;
					data.numgy = data.rng();
				};
				GAIA_EACH(it) update(data[i]);
			});
		}

	private:
		ecs::Query m_q;
	};

	class HealthSystem final: public ecs::System {
	public:
		void OnCreated() override {
			m_q = world().query().all<HealthComponent&>();
#if GAIA_DEBUG
			GAIA_LOG_N("HealthSystem: %u", m_q.count());
#endif
		}

		void OnUpdate() override {
			m_q.each([&](ecs::Iter& it) {
				auto health = it.view_mut<HealthComponent>(0);
				GAIA_EACH(it)::sysbase::updateHealth(health[i]);
			});
		}

	private:
		ecs::Query m_q;
	};

	class MoreComplexSystem final: public ecs::System {
	public:
		void OnCreated() override {
			m_q = world().query().all<PositionComponent, VelocityComponent&, DataComponent&>();
#if GAIA_DEBUG
			GAIA_LOG_N("MoreComplexSystem: %u", m_q.count());
#endif
		}

		void OnUpdate() override {
			m_q.each([&](ecs::Iter& it) {
				auto position = it.view<PositionComponent>(0);
				auto direction = it.view_mut<VelocityComponent>(1);
				auto data = it.view_mut<DataComponent>(2);

				auto update = [](const PositionComponent& position, VelocityComponent& direction, DataComponent& data) {
					if ((data.thingy % 10) == 0) {
						if (position.x > position.y) {
							direction.x = (float)data.rng.range(3, 19) - 10.0f;
							direction.y = (float)data.rng.range(0, 5);
						} else {
							direction.x = (float)data.rng.range(0, 5);
							direction.y = (float)data.rng.range(3, 19) - 10.0f;
						}
					}
				};
				GAIA_EACH(it) update(position[i], direction[i], data[i]);
			});
		}

	private:
		ecs::Query m_q;
	};

	class MoreComplexSystemSoA final: public ecs::System {
	public:
		void OnCreated() override {
			m_q = world().query().all<SoAPositionComponent, SoAVelocityComponent&, DataComponent&>();
#if GAIA_DEBUG
			GAIA_LOG_N("MoreComplexSystemSoA: %u", m_q.count());
#endif
		}

		void OnUpdate() override {
			m_q.each([&](ecs::Iter& iter) {
				// Position
				auto vp = iter.view<SoAPositionComponent>(0); // read-only access to PositionSoA
				auto px = vp.get<0>(); // continuous block of "x" from PositionSoA
				auto py = vp.get<1>(); // continuous block of "y" from PositionSoA

				// Velocity
				auto vv = iter.view_mut<SoAVelocityComponent>(1); // read-write access to VelocitySoA
				auto vx = vv.set<0>(); // continuous block of "x" from VelocitySoA
				auto vy = vv.set<1>(); // continuous block of "y" from VelocitySoA

				// Data
				auto vd = iter.view_mut<DataComponent>(2);

				GAIA_EACH(iter) {
					if ((vd[i].thingy % 10) == 0) {
						if (px[i] > py[i]) {
							vx[i] = (float)vd[i].rng.range(3, 19) - 10.0f;
							vy[i] = (float)vd[i].rng.range(0, 5);
						} else {
							vx[i] = (float)vd[i].rng.range(0, 5);
							vy[i] = (float)vd[i].rng.range(3, 19) - 10.0f;
						}
					}
				}
			});
		}

	private:
		ecs::Query m_q;
	};

	class MovementSystem final: public ecs::System {
	public:
		void OnCreated() override {
			m_q = world().query().all<PositionComponent&, VelocityComponent>();
#if GAIA_DEBUG
			GAIA_LOG_N("MovementSystem: %u", m_q.count());
#endif
		}

		void OnUpdate() override {
			m_q.each([&](ecs::Iter& it) {
				auto position = it.view_mut<PositionComponent>(0);
				auto direction = it.view<VelocityComponent>(1);
				GAIA_EACH(it) {
					position[i].x += direction[i].x * dt;
					position[i].y += direction[i].y * dt;
				}
			});
		}

	private:
		ecs::Query m_q;
	};

	class MovementSystemSoA final: public ecs::System {
	public:
		void OnCreated() override {
			m_q = world().query().all<SoAPositionComponent&, SoAVelocityComponent>();
#if GAIA_DEBUG
			GAIA_LOG_N("MovementSystemSoA: %u", m_q.count());
#endif
		}

		void OnUpdate() override {
			m_q.each([&](ecs::Iter& iter) {
				// Position
				auto vp = iter.view_mut<SoAPositionComponent>(0); // read-write access to PositionSoA
				auto px = vp.set<0>(); // continuous block of "x" from PositionSoA
				auto py = vp.set<1>(); // continuous block of "y" from PositionSoA

				// Velocity
				auto vv = iter.view<SoAVelocityComponent>(1); // read-only access to VelocitySoA
				auto vx = vv.get<0>(); // continuous block of "x" from VelocitySoA
				auto vy = vv.get<1>(); // continuous block of "y" from VelocitySoA

				// Handle x coordinates
				GAIA_EACH(iter) px[i] += vx[i] * dt;
				// Handle y coordinates
				GAIA_EACH(iter) py[i] += vy[i] * dt;
			});
		}

	private:
		ecs::Query m_q;
	};

	class RenderSystem final: public ecs::System {
	public:
		RenderSystem() = default;

		void setFrameBuffer(FrameBuffer& frameBuffer) {
			m_frameBuffer = &frameBuffer;
		}

		void OnCreated() override {
			m_q = world().query().all<PositionComponent, SpriteComponent>();
#if GAIA_DEBUG
			GAIA_LOG_N("RenderSystem: %u", m_q.count());
#endif
		}

		void OnUpdate() override {
			m_q.each([&](ecs::Iter& it) {
				auto position = it.view<PositionComponent>(0);
				auto sprite = it.view_mut<SpriteComponent>(1);
				GAIA_EACH(it)::sysbase::renderSprite(*m_frameBuffer, position[i].x, position[i].y, sprite[i]);
			});
		}

	private:
		ecs::Query m_q;
		FrameBuffer* m_frameBuffer;
	};

	class RenderSystemSoA final: public ecs::System {
	public:
		void setFrameBuffer(FrameBuffer& frameBuffer) {
			m_frameBuffer = &frameBuffer;
		}

		void OnCreated() override {
			m_q = world().query().all<SoAPositionComponent, SpriteComponent>();
#if GAIA_DEBUG
			GAIA_LOG_N("RenderSystemSoA: %u", m_q.count());
#endif
		}

		void OnUpdate() override {
			m_q.each([&](ecs::Iter& it) {
				// Position
				auto vp = it.view<SoAPositionComponent>(0); // read-only access to PositionSoA
				auto px = vp.get<0>(); // continuous block of "x" from PositionSoA
				auto py = vp.get<1>(); // continuous block of "y" from PositionSoA

				// Sprite
				auto vs = it.view<SpriteComponent>(1);

				GAIA_EACH(it)::sysbase::renderSprite(*m_frameBuffer, px[i], py[i], vs[i]);
			});
		}

	private:
		ecs::Query m_q;
		FrameBuffer* m_frameBuffer;
	};

	class SpriteSystem final: public ::gaia::ecs::System {
	public:
		void OnCreated() override {
			m_q = world().query().all<SpriteComponent&, PlayerComponent, HealthComponent>();
#if GAIA_DEBUG
			GAIA_LOG_N("SpriteSystem: %u", m_q.count());
#endif
		}

		void OnUpdate() override {
			m_q.each([&](::gaia::ecs::Iter& it) {
				auto sprite = it.view_mut<SpriteComponent>(0);
				auto player = it.view<PlayerComponent>(1);
				auto health = it.view<HealthComponent>(2);
				GAIA_EACH(it) sysbase::updateSprite(sprite[i], player[i], health[i]);
			});
		}

	private:
		::gaia::ecs::Query m_q;
	};
} // namespace sys

template <bool SoA>
void init_systems(ecs::SystemManager& sm, FrameBuffer& fb) {
	if constexpr (SoA)
		sm.add<sys::MovementSystemSoA>();
	else
		sm.add<sys::MovementSystem>();

	sm.add<sys::DataSystem>();

	if constexpr (SoA)
		sm.add<sys::MoreComplexSystemSoA>();
	else
		sm.add<sys::MoreComplexSystem>();

	sm.add<sys::HealthSystem>();
	sm.add<sys::DamageSystem>();
	sm.add<sys::SpriteSystem>();

	if constexpr (SoA) {
		auto* renderSystem = sm.add<sys::RenderSystemSoA>();
		renderSystem->setFrameBuffer(fb);
	} else {
		auto* renderSystem = sm.add<sys::RenderSystem>();
		renderSystem->setFrameBuffer(fb);
	}
}

template <bool SoA>
void createHero(ecs::World& w, ecs::Entity entity) {
	auto builder = w.build(entity);

	if constexpr (SoA)
		builder.add<components::SoAPositionComponent>().add<components::SoAVelocityComponent>();
	else
		builder.add<components::PositionComponent>().add<components::VelocityComponent>();

	builder //
			.add<components::PlayerComponent>()
			.add<components::SpriteComponent>()
			.add<components::HealthComponent>()
			.add<components::DamageComponent>();
	builder.commit();

	auto& player = w.set<components::PlayerComponent>(entity);
	auto& health = w.set<components::HealthComponent>(entity);
	auto& damage = w.set<components::DamageComponent>(entity);
	auto& sprite = w.set<components::SpriteComponent>(entity);

	player.type = components::PlayerType::Hero;
	health.maxhp = (int)player.rng.range(5, 15);
	damage.def = (int)player.rng.range(2, 6);
	damage.atk = (int)player.rng.range(4, 10);
	sprite.character = '_';

	const auto x = (float)player.rng.range(0, SpawAreaMaxX + SpawAreaMargin) - SpawAreaMargin;
	const auto y = (float)player.rng.range(0, SpawAreaMaxY + SpawAreaMargin) - SpawAreaMargin;
	if constexpr (SoA)
		w.set<components::SoAPositionComponent>(entity) = {x, y};
	else
		w.set<components::PositionComponent>(entity) = {x, y};
}

template <bool SoA>
void createEnemy(ecs::World& w, ecs::Entity entity) {
	auto builder = w.build(entity);

	if constexpr (SoA)
		builder.add<components::SoAPositionComponent>().add<components::SoAVelocityComponent>();
	else
		builder.add<components::PositionComponent>().add<components::VelocityComponent>();

	builder //
			.add<components::PlayerComponent>()
			.add<components::SpriteComponent>()
			.add<components::HealthComponent>()
			.add<components::DamageComponent>();
	builder.commit();

	auto& player = w.set<components::PlayerComponent>(entity);
	auto& health = w.set<components::HealthComponent>(entity);
	auto& damage = w.set<components::DamageComponent>(entity);
	auto& sprite = w.set<components::SpriteComponent>(entity);

	player.type = components::PlayerType::Monster;
	health.maxhp = (int)(player.rng.range(4, 12));
	damage.def = (int)(player.rng.range(2, 8));
	damage.atk = (int)(player.rng.range(3, 9));
	sprite.character = '_';

	const auto x = (float)player.rng.range(0, SpawAreaMaxX + SpawAreaMargin) - SpawAreaMargin;
	const auto y = (float)player.rng.range(0, SpawAreaMaxY + SpawAreaMargin) - SpawAreaMargin;
	if constexpr (SoA)
		w.set<components::SoAPositionComponent>(entity) = {x, y};
	else
		w.set<components::PositionComponent>(entity) = {x, y};
}

template <bool SoA>
void createNPC(ecs::World& w, ecs::Entity entity) {
	auto builder = w.build(entity);

	if constexpr (SoA)
		builder.add<components::SoAPositionComponent>().add<components::SoAVelocityComponent>();
	else
		builder.add<components::PositionComponent>().add<components::VelocityComponent>();

	builder //
			.add<components::PlayerComponent>()
			.add<components::SpriteComponent>();
	builder.commit();

	auto& player = w.set<components::PlayerComponent>(entity);
	auto& sprite = w.set<components::SpriteComponent>(entity);

	player.type = components::PlayerType::NPC;
	sprite.character = '_';

	const auto x = (float)player.rng.range(0, SpawAreaMaxX + SpawAreaMargin) - SpawAreaMargin;
	const auto y = (float)player.rng.range(0, SpawAreaMaxY + SpawAreaMargin) - SpawAreaMargin;
	if constexpr (SoA)
		w.set<components::SoAPositionComponent>(entity) = {x, y};
	else
		w.set<components::PositionComponent>(entity) = {x, y};
}

template <bool SoA>
void createRandom(ecs::World& w, ecs::Entity entity) {
	const auto rnd = rand() % 100;
	if (rnd <= 3)
		createNPC<SoA>(w, entity);
	else if (rnd <= 30)
		createHero<SoA>(w, entity);
	else
		createEnemy<SoA>(w, entity);
}

template <bool SoA>
void createEntitiesWithMixedComponents(ecs::World& w, uint32_t nentities, cnt::darray<ecs::Entity>& out) {
	out.clear();
	out.reserve(nentities);
	// Inspired by EnTT benchmark "pathological",
	// https://github.com/skypjack/entt/blob/de0e5862dd02fa669325a0a06b7936af4d2a841d/test/benchmark/benchmark.cpp#L44
	for (uint32_t i = 0, j = 0; i < nentities; i++) {
		auto e = w.add();
		{
			auto builder = w.build(e);

			if constexpr (SoA)
				builder.add<components::SoAPositionComponent>()
						.add<components::SoAVelocityComponent>()
						.add<components::DataComponent>();
			else
				builder.add<components::PositionComponent>()
						.add<components::VelocityComponent>()
						.add<components::DataComponent>();
			builder.commit();
		}

		{
			auto builder = w.build(e);

			if (nentities < 100 || (i >= 2 * nentities / 4 && i <= 3 * nentities / 4)) {
				if (nentities < 100 || (j % 10) == 0U) {
					if ((i % 7) == 0U) {
						if constexpr (SoA)
							builder.del<components::SoAPositionComponent>();
						else
							builder.del<components::PositionComponent>();
					}

					if ((i % 11) == 0U) {
						if constexpr (SoA)
							builder.del<components::SoAVelocityComponent>();
						else
							builder.del<components::VelocityComponent>();
					}

					if ((i % 13) == 0U)
						builder.del<components::DataComponent>();
				}
				j++;
			}

			builder.commit();
		}

		out.emplace_back(e);
	}
}

template <bool SoA>
void init_entities(ecs::World& w, uint32_t nentities) {
	cnt::darray<ecs::Entity> entities;
	createEntitiesWithMixedComponents<SoA>(w, nentities, entities);

	{
		auto entity = entities[0];
		createHero<SoA>(w, entity);
	}

	for (uint32_t i = 1, j = 0; i < entities.size(); i++) {
		auto entity = entities[i];
		if ((nentities < 100 && i == 0) || nentities >= 100 || i >= nentities / 8) {
			if ((nentities < 100 && i == 0) || nentities >= 100 || (j % 2) == 0U) {
				if ((i % 6) == 0U)
					createRandom<SoA>(w, entity);
				else if ((i % 4) == 0U)
					createNPC<SoA>(w, entity);
				else if ((i % 2) == 0U)
					createEnemy<SoA>(w, entity);
			}
			j++;
		}
	}
}

template <bool SoA>
void BM_Run(picobench::state& state) {
	ecs::World w;
	ecs::SystemManager sm(w);
	FrameBuffer fb(FrameBufferWidth, FrameBufferHeight);

	{
		GAIA_PROF_SCOPE(setup);
		init_systems<SoA>(sm, fb);
		init_entities<SoA>(w, (uint32_t)state.user_data());
	}

	srand(0);

	{
		GAIA_PROF_SCOPE(update);

		state.stop_timer();
		for (auto _: state) {
			state.stop_timer();
			(void)_;
			dt = CalculateDelta(state);

			state.start_timer();
			sm.update();
			state.stop_timer();

			w.update();
		}
	}
}

#define PICO_SETTINGS() iterations({1024}).samples(3)
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
		}

		GAIA_LOG_N("Profiling mode = %s", profilingMode ? "ON" : "OFF");
		GAIA_LOG_N("Sanitizer mode = %s", sanitizerMode ? "ON" : "OFF");

		constexpr uint32_t NFew = 1'000;
		constexpr uint32_t NMany = 10'000;

		if (profilingMode) {
			PICOBENCH_REG(BM_Run<false>).PICO_SETTINGS().user_data(NMany).label("1M");
			r.run_benchmarks();
			return 0;
		} else if (sanitizerMode) {
			PICOBENCH_REG(BM_Run<false>).PICO_SETTINGS().user_data(NFew).label("1K");
			PICOBENCH_REG(BM_Run<true>).PICO_SETTINGS().user_data(NFew).label("1K");
			r.run_benchmarks();
			return 0;
		} else {
			PICOBENCH_SUITE_REG("AoS");
			PICOBENCH_REG(BM_Run<false>).PICO_SETTINGS().user_data(NFew).label("1K");
			PICOBENCH_REG(BM_Run<false>).PICO_SETTINGS().user_data(NMany).label("1M");
			PICOBENCH_SUITE_REG("SoA");
			PICOBENCH_REG(BM_Run<true>).PICO_SETTINGS().user_data(NFew).label("1K");
			PICOBENCH_REG(BM_Run<true>).PICO_SETTINGS().user_data(NMany).label("1M");
		}
	}

	return r.run(0);
}
