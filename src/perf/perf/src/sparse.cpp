#include "common.h"
#include "registry.h"

template <bool DontFragment>
void setup_sparse_component_entities(ecs::World& w, cnt::darray<ecs::Entity>& entities, uint32_t count) {
	entities.clear();
	entities.reserve(count);

	const auto& comp = w.add<PositionSparse>();
	if constexpr (DontFragment)
		w.add(comp.entity, ecs::DontFragment);

	GAIA_FOR(count) {
		const auto e = w.add();
		entities.push_back(e);
	}
}

template <bool DontFragment>
void BM_SparseComponent_Add(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;

		ecs::World w;
		cnt::darray<ecs::Entity> entities;
		setup_sparse_component_entities<DontFragment>(w, entities, n);

		state.start_timer();
		for (auto e: entities)
			w.add<PositionSparse>(e);
		state.stop_timer();
	}
}

template <bool DontFragment>
void BM_SparseComponent_Set(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	setup_sparse_component_entities<DontFragment>(w, entities, n);

	for (auto e: entities)
		w.add<PositionSparse>(e);

	uint32_t cursor = 0;
	for (auto _: state) {
		(void)_;

		auto& pos = w.set<PositionSparse>(entities[cursor % entities.size()]);
		pos = {(float)cursor, (float)(cursor + 1U), (float)(cursor + 2U)};
		++cursor;
	}
}

template <bool DontFragment>
void BM_SparseComponent_Del(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;

		ecs::World w;
		cnt::darray<ecs::Entity> entities;
		setup_sparse_component_entities<DontFragment>(w, entities, n);

		for (auto e: entities)
			w.add<PositionSparse>(e);

		state.start_timer();
		for (auto e: entities)
			w.del<PositionSparse>(e);
		state.stop_timer();
	}
}

template <bool DontFragment>
void BM_SparseComponent_DeleteEntity(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;

		ecs::World w;
		cnt::darray<ecs::Entity> entities;
		setup_sparse_component_entities<DontFragment>(w, entities, n);

		for (auto e: entities)
			w.add<PositionSparse>(e);

		state.start_timer();
		for (auto e: entities)
			w.del(e);
		w.update();
		state.stop_timer();
	}
}

template <bool DontFragment>
void BM_Query_DirectSparse_All(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	setup_sparse_component_entities<DontFragment>(w, entities, n);

	GAIA_FOR(n) {
		const auto e = entities[i];
		w.add<Position>(e);
		if ((i & 1U) == 0)
			w.add<PositionSparse>(e);
	}

	auto q = w.query().all<Position>().all<PositionSparse>();
	uint32_t total = 0;

	for (auto _: state) {
		(void)_;
		total += q.count();
	}

	dont_optimize(total);
}

template <bool DontFragment>
void BM_Query_DirectSparse_Each(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	setup_sparse_component_entities<DontFragment>(w, entities, n);

	GAIA_FOR(n) {
		const auto e = entities[i];
		w.add<Position>(e, {(float)i, (float)(i + 1U), (float)(i + 2U)});
		if ((i & 1U) == 0)
			w.add<PositionSparse>(e);
	}

	auto q = w.query().all<Position>().all<PositionSparse>();
	uint64_t total = 0;

	for (auto _: state) {
		(void)_;
		uint64_t sum = 0;
		q.each([&](const Position& p) {
			sum += (uint64_t)(p.x + p.y + p.z);
		});
		total += sum;
	}

	dont_optimize(total);
}

template <bool DontFragment>
void BM_Query_DirectSparse_Or(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	setup_sparse_component_entities<DontFragment>(w, entities, n);

	GAIA_FOR(n) {
		const auto e = entities[i];
		if ((i & 1U) == 0)
			w.add<PositionSparse>(e);
		if ((i % 4U) == 1U)
			w.add<Acceleration>(e);
	}

	auto q = w.query().or_<PositionSparse>().or_<Acceleration>();
	uint32_t total = 0;

	for (auto _: state) {
		(void)_;
		total += q.count();
	}

	dont_optimize(total);
}

template <bool DontFragment>
void BM_Query_DirectSparse_Or_Each(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	setup_sparse_component_entities<DontFragment>(w, entities, n);

	GAIA_FOR(n) {
		const auto e = entities[i];
		w.add<Position>(e, {(float)i, (float)(i + 1U), (float)(i + 2U)});
		if ((i & 1U) == 0)
			w.add<PositionSparse>(e);
		if ((i % 4U) == 1U)
			w.add<Acceleration>(e);
	}

	auto q = w.query().all<Position>().or_<PositionSparse>().or_<Acceleration>();
	uint64_t total = 0;

	for (auto _: state) {
		(void)_;
		uint64_t sum = 0;
		q.each([&](const Position& p) {
			sum += (uint64_t)(p.x + p.y + p.z);
		});
		total += sum;
	}

	dont_optimize(total);
}

template <bool DontFragment>
void setup_runtime_sparse_component_entities(
		ecs::World& w, cnt::darray<ecs::Entity>& entities, uint32_t count, ecs::Entity& component) {
	entities.clear();
	entities.reserve(count);

	const auto& comp = w.add(
			"Runtime_Sparse_Position", (uint32_t)sizeof(Position), ecs::DataStorageType::Sparse, (uint32_t)alignof(Position));
	component = comp.entity;
	if constexpr (DontFragment)
		w.add(component, ecs::DontFragment);

	GAIA_FOR(count) {
		const auto e = w.add();
		entities.push_back(e);
	}
}

template <bool DontFragment>
void BM_RuntimeSparseComponent_Add(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;

		ecs::World w;
		cnt::darray<ecs::Entity> entities;
		ecs::Entity component = ecs::EntityBad;
		setup_runtime_sparse_component_entities<DontFragment>(w, entities, n, component);

		state.start_timer();
		for (auto e: entities)
			w.add(e, component, Position{1.0f, 2.0f, 3.0f});
		state.stop_timer();
	}
}

template <bool DontFragment>
void BM_RuntimeSparseComponent_Set(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	ecs::Entity component = ecs::EntityBad;
	setup_runtime_sparse_component_entities<DontFragment>(w, entities, n, component);

	for (auto e: entities)
		w.add(e, component, Position{1.0f, 2.0f, 3.0f});

	uint32_t cursor = 0;
	for (auto _: state) {
		(void)_;

		auto& pos = w.set<Position>(entities[cursor % entities.size()], component);
		pos = {(float)cursor, (float)(cursor + 1U), (float)(cursor + 2U)};
		++cursor;
	}
}

template <bool DontFragment>
void BM_RuntimeSparseComponent_Del(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;

		ecs::World w;
		cnt::darray<ecs::Entity> entities;
		ecs::Entity component = ecs::EntityBad;
		setup_runtime_sparse_component_entities<DontFragment>(w, entities, n, component);

		for (auto e: entities)
			w.add(e, component, Position{1.0f, 2.0f, 3.0f});

		state.start_timer();
		for (auto e: entities)
			w.del(e, component);
		state.stop_timer();
	}
}

template <bool DontFragment>
void BM_RuntimeSparseComponent_DeleteEntity(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;

		ecs::World w;
		cnt::darray<ecs::Entity> entities;
		ecs::Entity component = ecs::EntityBad;
		setup_runtime_sparse_component_entities<DontFragment>(w, entities, n, component);

		for (auto e: entities)
			w.add(e, component, Position{1.0f, 2.0f, 3.0f});

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

void register_sparse(PerfRunMode mode) {
	if (mode != PerfRunMode::Normal)
		return;

	PICOBENCH_SUITE_REG("Structural changes");
	PICOBENCH_REG(BM_SparseComponent_Add<false>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("sparse frag add 10K");
	PICOBENCH_REG(BM_SparseComponent_Add<true>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("sparse dontfrag add 10K");
	PICOBENCH_REG(BM_SparseComponent_Set<false>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("sparse frag set 10K");
	PICOBENCH_REG(BM_SparseComponent_Set<true>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("sparse dontfrag set 10K");
	PICOBENCH_REG(BM_SparseComponent_Del<false>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("sparse frag del 10K");
	PICOBENCH_REG(BM_SparseComponent_Del<true>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("sparse dontfrag del 10K");
	PICOBENCH_REG(BM_SparseComponent_DeleteEntity<false>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("sparse frag del entity 10K");
	PICOBENCH_REG(BM_SparseComponent_DeleteEntity<true>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("sparse dontfrag del entity 10K");
	PICOBENCH_REG(BM_Query_DirectSparse_All<false>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("sparse frag query all 10K");
	PICOBENCH_REG(BM_Query_DirectSparse_All<true>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("sparse dontfrag query all 10K");
	PICOBENCH_REG(BM_Query_DirectSparse_Each<false>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("sparse frag query each 10K");
	PICOBENCH_REG(BM_Query_DirectSparse_Each<true>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("sparse dontfrag query each 10K");
	PICOBENCH_REG(BM_Query_DirectSparse_Or<false>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("sparse frag query or 10K");
	PICOBENCH_REG(BM_Query_DirectSparse_Or<true>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("sparse dontfrag query or 10K");
	PICOBENCH_REG(BM_Query_DirectSparse_Or_Each<false>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("sparse frag query or each 10K");
	PICOBENCH_REG(BM_Query_DirectSparse_Or_Each<true>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("sparse dontfrag query or each 10K");
	PICOBENCH_REG(BM_RuntimeSparseComponent_Add<true>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("runtime sparse dontfrag add 10K");
	PICOBENCH_REG(BM_RuntimeSparseComponent_Set<true>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("runtime sparse dontfrag set 10K");
	PICOBENCH_REG(BM_RuntimeSparseComponent_Del<true>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("runtime sparse dontfrag del 10K");
	PICOBENCH_REG(BM_RuntimeSparseComponent_DeleteEntity<true>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("runtime sparse dontfrag del entity 10K");
}
