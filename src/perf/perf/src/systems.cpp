#include "common.h"
#include "registry.h"

template <uint32_t Systems>
void BM_SystemFrame(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_linear_entities<true, true, true, true, false>(w, entities, n);

	for (uint32_t i = 0U; i < entities.size(); ++i) {
		if ((i % 5U) == 0U)
			w.add<Frozen>(entities[i], {true});
		if ((i % 7U) == 0U)
			w.add<Dirty>(entities[i], {false});
	}

	// We intentionally keep this single-threaded in the matrix suite.
	init_systems(w, Systems, ecs::QueryExecType::Default);

	// Warm query caches and system scheduling path
	for (uint32_t i = 0U; i < 4U; ++i)
		w.update();

	for (auto _: state) {
		(void)_;
		w.update();
	}
}

void BM_SystemFrame_Serial_2(picobench::state& state) {
	BM_SystemFrame<2>(state);
}

void BM_SystemFrame_Serial_5(picobench::state& state) {
	BM_SystemFrame<5>(state);
}

////////////////////////////////////////////////////////////////////////////////

void register_systems(PerfRunMode mode) {
	switch (mode) {
		case PerfRunMode::Profiling:
			PICOBENCH_SUITE_REG("Profile picks");
			PICOBENCH_REG(BM_SystemFrame_Serial_5)
					.PICO_SETTINGS_HEAVY()
					.user_data(NEntitiesMedium)
					.label("systems serial, 5");
			return;
		case PerfRunMode::Sanitizer:
			PICOBENCH_SUITE_REG("Sanitizer picks");
			PICOBENCH_REG(BM_SystemFrame_Serial_2).PICO_SETTINGS_SANI().user_data(NEntitiesFew).label("systems serial");
			return;
		case PerfRunMode::Normal:
			PICOBENCH_SUITE_REG("Systems (single-thread)");
			PICOBENCH_REG(BM_SystemFrame_Serial_2).PICO_SETTINGS().user_data(NEntitiesMedium).label("serial, 2 systems");
			PICOBENCH_REG(BM_SystemFrame_Serial_5).PICO_SETTINGS().user_data(NEntitiesMedium).label("serial, 5 systems");
			return;
		default:
			return;
	}
}
