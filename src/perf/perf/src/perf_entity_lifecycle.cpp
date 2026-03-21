#include "perf_matrix_common.h"
#include "perf_registry.h"

void BM_EntityCreate_Empty_Add(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;
		state.stop_timer();
		ecs::World w;
		(void)w.add(); // warm archetype edge
		state.start_timer();

		GAIA_FOR(n)(void) w.add();

		state.stop_timer();
	}
}

void BM_EntityCreate_Empty_AddN(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;
		state.stop_timer();
		ecs::World w;
		(void)w.add(); // warm archetype edge
		state.start_timer();

		w.add_n(n);

		state.stop_timer();
	}
}

void BM_EntityCreate_4Comp_OneByOne(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;
		state.stop_timer();
		ecs::World w;

		// warm the component graph
		{
			auto warm = w.add();
			w.add<Position>(warm, {0.0f, 0.0f, 0.0f});
			w.add<Velocity>(warm, {0.0f, 0.0f, 0.0f});
			w.add<Acceleration>(warm, {0.0f, 0.0f, 0.0f});
			w.add<Health>(warm, {100, 100});
		}

		state.start_timer();

		GAIA_FOR(n) {
			auto e = w.add();
			w.add<Position>(e, {(float)i, 1.0f, 2.0f});
			w.add<Velocity>(e, {1.0f, 0.5f, 0.25f});
			w.add<Acceleration>(e, {0.0f, -0.01f, 0.0f});
			w.add<Health>(e, {100, 100});
		}

		state.stop_timer();
	}
}

void BM_EntityCreate_4Comp_Builder(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;
		state.stop_timer();
		ecs::World w;

		// warm the component graph
		{
			auto warm = w.add();
			w.build(warm).add<Position>().add<Velocity>().add<Acceleration>().add<Health>().commit();
		}

		state.start_timer();

		GAIA_FOR(n) {
			auto e = w.add();
			w.build(e).add<Position>().add<Velocity>().add<Acceleration>().add<Health>().commit();
		}

		state.stop_timer();
	}
}

void BM_EntityCopyN_4Comp(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;
		state.stop_timer();
		ecs::World w;

		auto seed = w.add();
		w.add<Position>(seed, {1.0f, 2.0f, 3.0f});
		w.add<Velocity>(seed, {1.0f, 0.5f, 0.25f});
		w.add<Acceleration>(seed, {0.0f, -0.01f, 0.0f});
		w.add<Health>(seed, {100, 100});

		state.start_timer();

		w.copy_n(seed, n);

		state.stop_timer();
	}
}

void BM_EntityInstantiateN_ParentedFallback_4Comp(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;
		state.stop_timer();
		ecs::World w;

		const auto scene = w.add();
		auto seed = w.add();
		w.add<Position>(seed, {1.0f, 2.0f, 3.0f});
		w.add<Velocity>(seed, {1.0f, 0.5f, 0.25f});
		w.add<Acceleration>(seed, {0.0f, -0.01f, 0.0f});
		w.add<Health>(seed, {100, 100});

		state.start_timer();

		w.instantiate_n(seed, scene, n);

		state.stop_timer();
	}
}

void BM_EntityInstantiateN_Prefab_4Comp(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;
		state.stop_timer();
		ecs::World w;

		const auto prefab = w.prefab();
		w.add<Position>(prefab, {1.0f, 2.0f, 3.0f});
		w.add<Velocity>(prefab, {1.0f, 0.5f, 0.25f});
		w.add<Acceleration>(prefab, {0.0f, -0.01f, 0.0f});
		w.add<Health>(prefab, {100, 100});

		state.start_timer();

		w.instantiate_n(prefab, n);

		state.stop_timer();
	}
}

void BM_EntityInstantiateN_Prefab_1Comp(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;
		state.stop_timer();
		ecs::World w;

		const auto prefab = w.prefab();
		w.add<Position>(prefab, {1.0f, 2.0f, 3.0f});

		state.start_timer();

		w.instantiate_n(prefab, n);

		state.stop_timer();
	}
}

void BM_EntityInstantiateN_Prefab_8Comp(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;
		state.stop_timer();
		ecs::World w;

		const auto prefab = w.prefab();
		w.add<Position>(prefab, {1.0f, 2.0f, 3.0f});
		w.add<Velocity>(prefab, {1.0f, 0.5f, 0.25f});
		w.add<Acceleration>(prefab, {0.0f, -0.01f, 0.0f});
		w.add<Health>(prefab, {100, 100});
		w.add<Damage>(prefab, {5});
		w.add<Mass>(prefab, {1.5f});
		w.add<Team>(prefab, {2});
		w.add<AIState>(prefab, {3});

		state.start_timer();

		w.instantiate_n(prefab, n);

		state.stop_timer();
	}
}

void BM_EntityInstantiateN_Prefab_Sparse_1Comp(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;
		state.stop_timer();
		ecs::World w;

		const auto prefab = w.prefab();
		w.add<PositionSparse>(prefab, {1.0f, 2.0f, 3.0f});

		state.start_timer();

		w.instantiate_n(prefab, n);

		state.stop_timer();
	}
}

void BM_EntityInstantiateN_Prefab_Subtree_4Comp(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;
		state.stop_timer();
		ecs::World w;

		const auto root = w.prefab();
		const auto child = w.prefab();
		const auto leaf = w.prefab();

		w.add<Position>(root, {1.0f, 2.0f, 3.0f});
		w.add<Velocity>(root, {1.0f, 0.5f, 0.25f});
		w.add<Acceleration>(root, {0.0f, -0.01f, 0.0f});
		w.add<Health>(root, {100, 100});

		w.add<Position>(child, {4.0f, 5.0f, 6.0f});
		w.add<Velocity>(child, {1.0f, 0.5f, 0.25f});
		w.add<Acceleration>(child, {0.0f, -0.01f, 0.0f});
		w.add<Health>(child, {90, 100});

		w.add<Position>(leaf, {7.0f, 8.0f, 9.0f});
		w.add<Velocity>(leaf, {1.0f, 0.5f, 0.25f});
		w.add<Acceleration>(leaf, {0.0f, -0.01f, 0.0f});
		w.add<Health>(leaf, {80, 100});

		w.parent(child, root);
		w.parent(leaf, child);

		state.start_timer();

		w.instantiate_n(root, n);

		state.stop_timer();
	}
}

void BM_EntityDestroy_Empty(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();
	cnt::darray<ecs::Entity> entities;

	for (auto _: state) {
		(void)_;
		state.stop_timer();
		ecs::World w;
		entities.clear();
		entities.reserve(n);
		GAIA_FOR(n) entities.push_back(w.add());
		state.start_timer();

		for (auto e: entities)
			w.del(e);
		w.update();

		state.stop_timer();
	}
}

void BM_EntityDestroy_4Comp(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();
	cnt::darray<ecs::Entity> entities;

	for (auto _: state) {
		(void)_;
		state.stop_timer();
		ecs::World w;
		create_linear_entities<true, true, true, true, false>(w, entities, n);
		state.start_timer();

		for (auto e: entities)
			w.del(e);
		w.update();

		state.stop_timer();
	}
}

#define PICO_SETTINGS() iterations({256}).samples(3)
#define PICO_SETTINGS_HEAVY() iterations({64}).samples(3)
#define PICO_SETTINGS_FOCUS() iterations({256}).samples(7)
#define PICO_SETTINGS_OBS() iterations({64}).samples(3)
#define PICO_SETTINGS_SANI() iterations({8}).samples(1)
#define PICOBENCH_SUITE_REG(name) (void)picobench::global_registry::set_bench_suite(name)
#define PICOBENCH_REG(func) picobench::global_registry::new_benchmark(#func, func)

void register_perf_matrix_entity_lifecycle(PerfRunMode mode) {
	switch (mode) {
		case PerfRunMode::Sanitizer:
			PICOBENCH_SUITE_REG("Sanitizer picks");
			PICOBENCH_REG(BM_EntityCreate_Empty_Add).PICO_SETTINGS_SANI().user_data(NEntitiesFew).label("create add");
			return;
		case PerfRunMode::Normal:
			PICOBENCH_SUITE_REG("Entity lifecycle");
			PICOBENCH_REG(BM_EntityCreate_Empty_Add).PICO_SETTINGS().user_data(NEntitiesMedium).label("add, 100K");
			PICOBENCH_REG(BM_EntityCreate_Empty_Add).PICO_SETTINGS_HEAVY().user_data(NEntitiesMany).label("add, 1M");
			PICOBENCH_REG(BM_EntityCreate_Empty_AddN).PICO_SETTINGS().user_data(NEntitiesMedium).label("add_n, 100K");
			PICOBENCH_REG(BM_EntityCreate_Empty_AddN).PICO_SETTINGS_HEAVY().user_data(NEntitiesMany).label("add_n, 1M");
			PICOBENCH_REG(BM_EntityCreate_4Comp_OneByOne).PICO_SETTINGS().user_data(NEntitiesFew).label("4comp, one-by-one");
			PICOBENCH_REG(BM_EntityCreate_4Comp_Builder).PICO_SETTINGS().user_data(NEntitiesFew).label("4comp, builder");
			PICOBENCH_REG(BM_EntityCopyN_4Comp).PICO_SETTINGS().user_data(NEntitiesMedium).label("copy_n, 100K");
			PICOBENCH_REG(BM_EntityInstantiateN_ParentedFallback_4Comp)
					.PICO_SETTINGS()
					.user_data(NEntitiesMedium)
					.label("instantiate_n parented fallback, 100K");
			PICOBENCH_REG(BM_EntityInstantiateN_Prefab_1Comp)
					.PICO_SETTINGS()
					.user_data(NEntitiesMedium)
					.label("instantiate_n prefab 1comp, 100K");
			PICOBENCH_REG(BM_EntityInstantiateN_Prefab_4Comp)
					.PICO_SETTINGS()
					.user_data(NEntitiesMedium)
					.label("instantiate_n prefab, 100K");
			PICOBENCH_REG(BM_EntityInstantiateN_Prefab_8Comp)
					.PICO_SETTINGS()
					.user_data(NEntitiesMedium)
					.label("instantiate_n prefab 8comp, 100K");
			PICOBENCH_REG(BM_EntityInstantiateN_Prefab_Sparse_1Comp)
					.PICO_SETTINGS()
					.user_data(NEntitiesMedium)
					.label("instantiate_n prefab sparse 1comp, 100K");
			PICOBENCH_REG(BM_EntityInstantiateN_Prefab_Subtree_4Comp)
					.PICO_SETTINGS()
					.user_data(NEntitiesFew)
					.label("instantiate_n prefab subtree, 10K");
			PICOBENCH_REG(BM_EntityDestroy_Empty).PICO_SETTINGS().user_data(NEntitiesMedium).label("destroy empty");
			PICOBENCH_REG(BM_EntityDestroy_4Comp).PICO_SETTINGS().user_data(NEntitiesFew).label("destroy 4comp");
			return;
		case PerfRunMode::Profiling:
		default:
			return;
	}
}
