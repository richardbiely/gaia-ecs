#include "perf_matrix_common.h"
#include "perf_registry.h"

void BM_MixedFrame_Churn(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_linear_entities<true, true, true, true, false>(w, entities, n);

	auto q = w.query().all<Position&>().all<Velocity&>().no<Frozen>();
	dont_optimize(q.empty());

	uint32_t cursor = 0U;

	for (auto _: state) {
		(void)_;

		q.each([](Position& p, Velocity& v) {
			v.y += -0.1f * DeltaTime;
			p.x += v.x * DeltaTime;
			p.y += v.y * DeltaTime;
		});

		const uint32_t toggleCount = core::get_max(64U, n / 64U);
		GAIA_FOR(toggleCount) {
			const uint32_t idx = (cursor + i) % n;
			auto e = entities[idx];
			if (w.has<Frozen>(e))
				w.del<Frozen>(e);
			else
				w.add<Frozen>(e, {true});
		}

		const uint32_t churnCount = core::get_max(32U, n / 128U);
		GAIA_FOR(churnCount) {
			const uint32_t idx = cursor % n;
			auto oldE = entities[idx];

			auto newE = w.add();
			w.add<Position>(newE, {(float)idx, 0.0f, 0.0f});
			w.add<Velocity>(newE, {1.0f, 0.0f, 0.0f});
			w.add<Acceleration>(newE, {0.0f, -0.02f, 0.0f});
			w.add<Health>(newE, {100, 100});
			w.add<Damage>(newE, {(int32_t)(idx % 7U)});

			w.del(oldE);
			entities[idx] = newE;
			++cursor;
		}

		w.update();
	}
}

#define PICO_SETTINGS() iterations({256}).samples(3)
#define PICO_SETTINGS_HEAVY() iterations({64}).samples(3)
#define PICO_SETTINGS_FOCUS() iterations({256}).samples(7)
#define PICO_SETTINGS_OBS() iterations({64}).samples(3)
#define PICO_SETTINGS_SANI() iterations({8}).samples(1)
#define PICOBENCH_SUITE_REG(name) (void)picobench::global_registry::set_bench_suite(name)
#define PICOBENCH_REG(func) picobench::global_registry::new_benchmark(#func, func)

void register_perf_matrix_mixed(PerfRunMode mode) {
	switch (mode) {
		case PerfRunMode::Profiling:
			PICOBENCH_SUITE_REG("Profile picks");
			PICOBENCH_REG(BM_MixedFrame_Churn).PICO_SETTINGS_HEAVY().user_data(NEntitiesMedium).label("mixed churn");
			return;
		case PerfRunMode::Normal:
			PICOBENCH_SUITE_REG("Mixed frame");
			PICOBENCH_REG(BM_MixedFrame_Churn).PICO_SETTINGS().user_data(NEntitiesFew).label("10K");
			PICOBENCH_REG(BM_MixedFrame_Churn).PICO_SETTINGS_HEAVY().user_data(NEntitiesMedium).label("100K");
			return;
		case PerfRunMode::Sanitizer:
		default:
			return;
	}
}
