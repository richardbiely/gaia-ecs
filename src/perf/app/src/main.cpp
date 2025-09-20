// Benchmark inspired by https://github.com/abeimler/ecs_benchmark.

#define PICOBENCH_IMPLEMENT
#include <gaia.h>
#include <picobench/picobench.hpp>
#include <string_view>

using namespace gaia;

float dt;

inline static constexpr uint32_t FrameBufferWidth = 320;
inline static constexpr uint32_t FrameBufferHeight = 240;
inline static constexpr uint32_t SpawAreaMaxX = 320;
inline static constexpr uint32_t SpawAreaMaxY = 240;
inline static constexpr uint32_t SpawAreaMargin = 100;

namespace rnd {
	struct pseudo_random {
		uint32_t state = 0;

		constexpr pseudo_random() {}
		constexpr pseudo_random(uint32_t seed): state(seed) {}

		constexpr uint32_t next() {
			state = state * 1664525 + 1013904223;
			return state;
		}

		constexpr uint32_t operator()() noexcept {
			return next();
		}

		constexpr uint32_t range(uint32_t low, uint32_t high) noexcept {
			const uint32_t r = high - low + 1;
			return (operator()() % r) + low;
		}
	};
} // namespace rnd

namespace components {
	inline static constexpr char PlayerSprite = '@';
	inline static constexpr char MonsterSprite = 'k';
	inline static constexpr char NPCSprite = 'h';
	inline static constexpr char GraveSprite = '|';
	inline static constexpr char SpawnSprite = '_';
	inline static constexpr char NoneSprite = ' ';

	enum class PlayerType { NPC, Monster, Hero };

	struct PlayerComponent {
		rnd::pseudo_random rng{};
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
		static constexpr uint32_t DefaultSeed = 340383L;

		int thingy{0};
		double dingy{0.0};
		bool mingy{false};

		uint32_t seed{DefaultSeed};
		rnd::pseudo_random rng;
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
		GAIA_LAYOUT(SoA);
		float x{0.0f};
		float y{0.0f};
	};

	struct VelocityComponent {
		float x{1.0f};
		float y{1.0f};
	};

	struct SoAVelocityComponent {
		GAIA_LAYOUT(SoA);
		float x{1.0f};
		float y{1.0f};
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

	void init_DamageSystem(ecs::World& world) {
		world
				.system() //
				.all<HealthComponent>()
				.all<DamageComponent>()
				.on_each([](ecs::Iter& it) {
					auto health = it.view_mut<HealthComponent>(0);
					auto damage = it.view<DamageComponent>(1);
					auto update = [](HealthComponent& health, const DamageComponent& damage) {
						const int totalDamage = core::get_max(damage.atk - damage.def, 0);
						health.hp -= totalDamage;
					};

					const auto cnt = it.size();
					GAIA_FOR(cnt) update(health[i], damage[i]);
				});
	}

	void init_DataSystem(ecs::World& world) {
		world.system()
				.all<DataComponent>()
				.on_each([](ecs::Iter& it) {
					auto data = it.view_mut<DataComponent>(0);
					auto update = [](DataComponent& data) {
						data.thingy = (data.thingy + 1) % 1'000'000;
						data.dingy += 0.0001f * dt;
						data.mingy = !data.mingy;
						data.numgy = data.rng();
					};

					const auto cnt = it.size();
					GAIA_FOR(cnt) update(data[i]);
				})
				.mode(ecs::QueryExecType::Parallel);
	}

	void init_HealthSystem(ecs::World& world) {
		world.system()
				.all<HealthComponent>()
				.on_each([](ecs::Iter& it) {
					auto health = it.view_mut<HealthComponent>(0);
					const auto cnt = it.size();
					GAIA_FOR(cnt) updateHealth(health[i]);
				})
				.mode(ecs::QueryExecType::Parallel);
	}

	void init_MoreComplexSystem(ecs::World& world) {
		world.system()
				.all<PositionComponent>()
				.all<VelocityComponent&>()
				.all<DataComponent&>()
				.on_each([](ecs::Iter& it) {
					auto position = it.view<PositionComponent>(0);
					auto direction = it.view_mut<VelocityComponent>(1);
					auto data = it.view_mut<DataComponent>(2);

					const auto cnt = it.size();
					GAIA_FOR(cnt) {
						if ((data[i].thingy % 10) == 0) {
							if (position[i].x > position[i].y) {
								direction[i].x = (float)data[i].rng.range(3, 19) - 10.0f;
								direction[i].y = (float)data[i].rng.range(0, 5);
							} else {
								direction[i].x = (float)data[i].rng.range(0, 5);
								direction[i].y = (float)data[i].rng.range(3, 19) - 10.0f;
							}
						}
					}
				})
				.mode(ecs::QueryExecType::Parallel);
	}

	void init_MoreComplexSystemSoA(ecs::World& world) {
		world.system()
				.all<SoAPositionComponent>()
				.all<SoAVelocityComponent&>()
				.all<DataComponent&>()
				.on_each([](ecs::Iter& it) {
					auto vp = it.view<SoAPositionComponent>(0); // read-only access to PositionSoA
					auto px = vp.get<0>(); // continuous block of "x" from PositionSoA
					auto py = vp.get<1>(); // continuous block of "y" from PositionSoA

					auto vv = it.view_mut<SoAVelocityComponent>(1); // read-write access to VelocitySoA
					auto vx = vv.set<0>(); // continuous block of "x" from VelocitySoA
					auto vy = vv.set<1>(); // continuous block of "y" from VelocitySoA

					auto vd = it.view_mut<DataComponent>(2);

					const auto cnt = it.size();
					GAIA_FOR(cnt) {
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
				})
				.mode(ecs::QueryExecType::Parallel);
	}

	void init_MovementSystem(ecs::World& world) {
		world.system()
				.all<PositionComponent>()
				.all<VelocityComponent>()
				.on_each([](ecs::Iter& it) {
					auto position = it.view_mut<PositionComponent>(0);
					auto direction = it.view<VelocityComponent>(1);

					const auto cnt = it.size();
					GAIA_FOR(cnt) {
						position[i].x += direction[i].x * dt;
						position[i].y += direction[i].y * dt;
					}
				})
				.mode(ecs::QueryExecType::Parallel);
	}

	void init_MovementSystemSoA(ecs::World& world) {
		world.system()
				.all<SoAPositionComponent>()
				.all<SoAVelocityComponent>()
				.on_each([](ecs::Iter& it) {
					auto vp = it.view_mut<SoAPositionComponent>(0); // read-write access to PositionSoA
					auto px = vp.set<0>(); // continuous block of "x" from PositionSoA
					auto py = vp.set<1>(); // continuous block of "y" from PositionSoA

					auto vv = it.view<SoAVelocityComponent>(1); // read-only access to VelocitySoA
					auto vx = vv.get<0>(); // continuous block of "x" from VelocitySoA
					auto vy = vv.get<1>(); // continuous block of "y" from VelocitySoA

					const auto cnt = it.size();
					GAIA_FOR(cnt) {
						px[i] += vx[i] * dt;
						py[i] += vy[i] * dt;
					}
				})
				.mode(ecs::QueryExecType::Parallel);
	}

	void init_RenderSystem(ecs::World& world, FrameBuffer& fb) {
		world.system()
				.all<PositionComponent>()
				.all<SpriteComponent>()
				.on_each([&](ecs::Iter& it) {
					auto position = it.view<PositionComponent>(0);
					auto sprite = it.view<SpriteComponent>(1);

					const auto cnt = it.size();
					GAIA_FOR(cnt) {
						renderSprite(fb, position[i].x, position[i].y, sprite[i]);
					}
				})
				.mode(ecs::QueryExecType::Parallel);
	}

	void init_RenderSystemSoA(ecs::World& world, FrameBuffer& fb) {
		world.system()
				.all<SoAPositionComponent>()
				.all<SpriteComponent>()
				.on_each([&](ecs::Iter& it) {
					auto vp = it.view<SoAPositionComponent>(0); // read-only access to PositionSoA
					auto px = vp.get<0>(); // continuous block of "x" from PositionSoA
					auto py = vp.get<1>(); // continuous block of "y" from PositionSoA

					auto vs = it.view<SpriteComponent>(1);

					const auto cnt = it.size();
					GAIA_FOR(cnt) {
						renderSprite(fb, px[i], py[i], vs[i]);
					}
				})
				.mode(ecs::QueryExecType::Parallel);
	}

	void init_SpriteSystem(ecs::World& world) {
		world.system()
				.all<SpriteComponent>()
				.all<PlayerComponent>()
				.all<HealthComponent>()
				.on_each([](ecs::Iter& it) {
					auto sprite = it.view_mut<SpriteComponent>(0);
					auto player = it.view<PlayerComponent>(1);
					auto health = it.view<HealthComponent>(2);

					const auto cnt = it.size();
					GAIA_FOR(cnt) {
						updateSprite(sprite[i], player[i], health[i]);
					}
				})
				.mode(ecs::QueryExecType::Parallel);
	}
} // namespace sys

template <bool SoA>
void init_systems(ecs::World& w, FrameBuffer& fb) {
	if constexpr (SoA) {
		sys::init_MovementSystemSoA(w);
	} else {
		sys::init_MovementSystem(w);
	}

	sys::init_DataSystem(w);

	if constexpr (SoA) {
		sys::init_MoreComplexSystemSoA(w);
	} else {
		sys::init_MoreComplexSystem(w);
	}

	sys::init_HealthSystem(w);
	sys::init_DamageSystem(w);
	sys::init_SpriteSystem(w);

	if constexpr (SoA) {
		sys::init_RenderSystemSoA(w, fb);
	} else {
		sys::init_RenderSystem(w, fb);
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
	FrameBuffer fb(FrameBufferWidth, FrameBufferHeight);

	{
		GAIA_PROF_SCOPE(setup);
		init_systems<SoA>(w, fb);
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
			w.update();
			state.stop_timer();
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

		constexpr uint32_t NFew = 10'000;
		constexpr uint32_t NMany = 100'000;

		if (profilingMode) {
			PICOBENCH_REG(BM_Run<false>).PICO_SETTINGS().user_data(NMany).label("1M");
			r.run_benchmarks();
			return 0;
		}

		if (sanitizerMode) {
			PICOBENCH_REG(BM_Run<false>).PICO_SETTINGS().user_data(NFew).label("1K");
			PICOBENCH_REG(BM_Run<true>).PICO_SETTINGS().user_data(NFew).label("1K");
			r.run_benchmarks();
			return 0;
		}

		{
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
