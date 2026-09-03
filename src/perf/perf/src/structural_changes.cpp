#include "common.h"
#include "registry.h"

void BM_ComponentAdd_Velocity(picobench::state& state) {
	const uint32_t n = (uint32_t)state.input_data();
	cnt::darray<ecs::Entity> entities;

	for (auto _: state) {
		(void)_;
		state.stop_timer();
		ecs::World w;
		create_linear_entities<false, false, false, false, false>(w, entities, n);

		// warm graph edge
		{
			auto warm = w.add();
			w.add<Position>(warm, {0.0f, 0.0f, 0.0f});
			w.add<Velocity>(warm, {0.0f, 0.0f, 0.0f});
			w.del<Velocity>(warm);
		}

		state.start_timer();

		for (auto e: entities)
			w.add<Velocity>(e, {1.0f, 0.0f, 0.0f});

		state.stop_timer();
	}
}

void BM_ComponentRemove_Velocity(picobench::state& state) {
	const uint32_t n = (uint32_t)state.input_data();
	cnt::darray<ecs::Entity> entities;

	for (auto _: state) {
		(void)_;
		state.stop_timer();
		ecs::World w;
		create_linear_entities<true, false, false, false, false>(w, entities, n);
		state.start_timer();

		for (auto e: entities)
			w.del<Velocity>(e);

		state.stop_timer();
	}
}

void BM_ComponentToggle_Frozen(picobench::state& state) {
	const uint32_t n = (uint32_t)state.input_data();
	cnt::darray<ecs::Entity> entities;
	ecs::World w;
	create_linear_entities<true, true, false, false, false>(w, entities, n);

	bool addPhase = true;
	for (auto _: state) {
		(void)_;
		if (addPhase) {
			for (uint32_t idx = 0U; idx < entities.size(); idx += 2U)
				w.add<Frozen>(entities[idx], {true});
		} else {
			for (uint32_t idx = 0U; idx < entities.size(); idx += 2U)
				w.del<Frozen>(entities[idx]);
		}
		addPhase = !addPhase;
	}
}

//! Benchmarks repeated creation, emptying, and GC of chunk-heavy archetypes.
//! This exercises World's deferred chunk-delete queue maintenance.
void BM_World_ChunkDeleteQueue_GC(picobench::state& state) {
	const uint32_t n = (uint32_t)state.input_data();

	struct ChunkQueueBenchTag {};

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	entities.reserve(n);

	for (auto _: state) {
		(void)_;

		entities.clear();
		for (uint32_t i = 0; i < n; ++i) {
			auto e = w.add();
			w.add<ChunkQueueBenchTag>(e);
			entities.push_back(e);
		}

		for (auto e: entities)
			w.del(e);

		w.update();
		dont_optimize(entities.size());
	}
}

////////////////////////////////////////////////////////////////////////////////

void register_structural_changes(PerfRunMode mode) {
	switch (mode) {
		case PerfRunMode::Sanitizer:
			PICOBENCH_SUITE_REG("Sanitizer picks");
			PICOBENCH_REG(BM_ComponentAdd_Velocity).PICO_SETTINGS_SANI().user_data(NEntitiesFew).label("add velocity");
			return;
		case PerfRunMode::Normal:
			PICOBENCH_SUITE_REG("Structural changes");
			PICOBENCH_REG(BM_ComponentAdd_Velocity).PICO_SETTINGS().user_data(NEntitiesMedium).label("add velocity");
			PICOBENCH_REG(BM_ComponentRemove_Velocity).PICO_SETTINGS().user_data(NEntitiesMedium).label("remove velocity");
			PICOBENCH_REG(BM_ComponentToggle_Frozen).PICO_SETTINGS().user_data(NEntitiesMedium).label("toggle frozen");
			PICOBENCH_REG(BM_World_ChunkDeleteQueue_GC)
					.PICO_SETTINGS_FOCUS()
					.user_data(NEntitiesFew)
					.label("chunk delete queue gc 10K");
			return;
		case PerfRunMode::Profiling:
		default:
			return;
	}
}
