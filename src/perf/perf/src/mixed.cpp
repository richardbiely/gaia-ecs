#include "common.h"
#include "registry.h"

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

////////////////////////////////////////////////////////////////////////////////

void register_mixed(PerfRunMode mode) {
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
