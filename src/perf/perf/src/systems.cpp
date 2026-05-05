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

template <ecs::QueryCacheScope Scope, uint32_t Systems>
void BM_SystemFrame_Identical(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_linear_entities<true, false, false, false, false>(w, entities, n);

	GAIA_FOR(Systems) {
		w.system()
				.scope(Scope)
				.all<Position&>()
				.all<Velocity>()
				.mode(ecs::QueryExecType::Default)
				.on_each([](ecs::Iter& it) {
					auto p = it.view_mut<Position>(0);
					auto v = it.view<Velocity>(1);

					const auto cnt = it.size();
					GAIA_FOR(cnt) {
						p[i].x += v[i].x * DeltaTime;
						p[i].y += v[i].y * DeltaTime;
						p[i].z += v[i].z * DeltaTime;
					}
				});
	}

	for (uint32_t i = 0U; i < 4U; ++i)
		w.update();

	for (auto _: state) {
		(void)_;
		w.update();
	}
}

void BM_SystemFrame_Identical_Local_16(picobench::state& state) {
	BM_SystemFrame_Identical<ecs::QueryCacheScope::Local, 16>(state);
}

void BM_SystemFrame_Identical_Shared_16(picobench::state& state) {
	BM_SystemFrame_Identical<ecs::QueryCacheScope::Shared, 16>(state);
}

template <uint32_t ChainDepth>
ecs::Entity create_is_fanout_fixture(ecs::World& w, uint32_t branches, bool attachPositionToLeavesOnly) {
	const auto root = w.add();
	GAIA_FOR(branches) {
		auto curr = root;
		for (uint32_t j = 0; j < ChainDepth; ++j) {
			const auto next = w.add();
			w.add(next, ecs::Pair(ecs::Is, curr));
			if (!attachPositionToLeavesOnly || j + 1U == ChainDepth)
				w.add<Position>(next, {(float)i, (float)j, (float)(i + j)});
			curr = next;
		}
	}
	return root;
}

template <uint32_t ChainDepth>
ecs::Entity create_is_fanout_fixture(
		ecs::World& w, uint32_t branches, bool attachPositionToLeavesOnly, cnt::darray<ecs::Entity>& leaves) {
	leaves.clear();
	leaves.reserve(branches);

	const auto root = w.add();
	GAIA_FOR(branches) {
		auto curr = root;
		for (uint32_t j = 0; j < ChainDepth; ++j) {
			const auto next = w.add();
			w.add(next, ecs::Pair(ecs::Is, curr));
			if (!attachPositionToLeavesOnly || j + 1U == ChainDepth)
				w.add<Position>(next, {(float)i, (float)j, (float)(i + j)});
			curr = next;
		}
		leaves.push_back(curr);
	}
	return root;
}

template <uint32_t ChainDepth, bool Direct>
void BM_System_Is(picobench::state& state) {
	const uint32_t branches = (uint32_t)state.user_data();

	ecs::World w;
	const auto root = create_is_fanout_fixture<ChainDepth>(w, branches, false);
	uint64_t sink = 0;

	if constexpr (Direct) {
		w.system()
				.name("is_direct")
				.all<Position>()
				.is(root, ecs::QueryTermOptions{}.direct())
				.mode(ecs::QueryExecType::Default)
				.on_each([&sink](const Position& p) {
					sink += (uint64_t)(p.x + p.y + p.z);
				});
	} else {
		w.system()
				.name("is_semantic")
				.all<Position>()
				.is(root)
				.mode(ecs::QueryExecType::Default)
				.on_each([&sink](const Position& p) {
					sink += (uint64_t)(p.x + p.y + p.z);
				});
	}

	for (uint32_t i = 0; i < 4; ++i)
		w.update();

	for (auto _: state) {
		(void)_;
		w.update();
	}

	dont_optimize(sink);
}

template <uint32_t ChainDepth, bool Direct>
void BM_System_IsIter(picobench::state& state) {
	const uint32_t branches = (uint32_t)state.user_data();

	ecs::World w;
	const auto root = create_is_fanout_fixture<ChainDepth>(w, branches, false);
	uint64_t sink = 0;

	if constexpr (Direct) {
		w.system()
				.name("is_direct_iter")
				.all<Position>()
				.is(root, ecs::QueryTermOptions{}.direct())
				.mode(ecs::QueryExecType::Default)
				.on_each([&sink](ecs::Iter& it) {
					auto posView = it.view<Position>();
					GAIA_EACH(it) {
						sink += (uint64_t)(posView[i].x + posView[i].y + posView[i].z);
					}
				});
	} else {
		w.system()
				.name("is_semantic_iter")
				.all<Position>()
				.is(root)
				.mode(ecs::QueryExecType::Default)
				.on_each([&sink](ecs::Iter& it) {
					auto posView = it.view<Position>();
					GAIA_EACH(it) {
						sink += (uint64_t)(posView[i].x + posView[i].y + posView[i].z);
					}
				});
	}

	for (uint32_t i = 0; i < 4; ++i)
		w.update();

	for (auto _: state) {
		(void)_;
		w.update();
	}

	dont_optimize(sink);
}

void BM_System_Is_Semantic_D2(picobench::state& state) {
	BM_System_Is<2, false>(state);
}

void BM_System_Is_Direct_D2(picobench::state& state) {
	BM_System_Is<2, true>(state);
}

void BM_System_Is_Semantic_D8(picobench::state& state) {
	BM_System_Is<8, false>(state);
}

void BM_System_Is_Direct_D8(picobench::state& state) {
	BM_System_Is<8, true>(state);
}

void BM_System_IsIter_Semantic_D8(picobench::state& state) {
	BM_System_IsIter<8, false>(state);
}

void BM_System_IsIter_Direct_D8(picobench::state& state) {
	BM_System_IsIter<8, true>(state);
}

static constexpr uint32_t NScheduledSystems = 256;
static constexpr uint32_t NSchedulerPhases = 8;
static constexpr uint32_t NSchedulerEntities = 4'096;

void create_schedule_probe_entities(ecs::World& w, cnt::darray<ecs::Entity>& entities, uint32_t count) {
	entities.clear();
	entities.reserve(count);

	GAIA_FOR(count) {
		const auto e = w.add();
		entities.push_back(e);
		w.add<Position>(e, {(float)i, (float)(i % 31U), 0.0f});
		w.add<Acceleration>(e, {0.0f, 0.0f, 0.0f});
	}
}

void add_balanced_depends_on_edges(ecs::World& w, const cnt::darray<ecs::Entity>& systems, uint32_t stride) {
	GAIA_ASSERT(stride > 0U);
	GAIA_FOR2_(stride, systems.size(), i) {
		const auto lane = i % stride;
		const auto laneIdx = i / stride;
		const auto targetIdx = ((laneIdx - 1U) / 2U) * stride + lane;
		w.add(systems[i], {ecs::DependsOn, systems[targetIdx]});
	}
}

void BM_SystemSchedule_SerialOrder_256(picobench::state& state) {
	(void)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_schedule_probe_entities(w, entities, 1U);

	uint64_t sink = 0;
	cnt::darray<ecs::Entity> systems;
	systems.reserve(NScheduledSystems);
	GAIA_FOR(NScheduledSystems) {
		auto sys = w.system().all<const Position>().mode(ecs::QueryExecType::Default).on_each([&sink](ecs::Iter& it) {
			sink += it.entity_rows().size();
		});
		systems.push_back(sys.entity());
	}
	add_balanced_depends_on_edges(w, systems, 1U);

	GAIA_FOR(4U) {
		w.update();
	}

	for (auto _: state) {
		(void)_;
		w.update();
	}

	dont_optimize(sink);
}

void BM_SystemSchedule_PhasedOrder_256(picobench::state& state) {
	(void)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_schedule_probe_entities(w, entities, 1U);

	cnt::darray<ecs::Entity> phases;
	phases.reserve(NSchedulerPhases);
	GAIA_FOR(NSchedulerPhases) {
		const auto phase = w.add();
		phases.push_back(phase);
		if (i > 0U)
			w.add(phase, {ecs::DependsOn, phases[i - 1U]});
	}

	uint64_t sink = 0;
	cnt::darray<ecs::Entity> systems;
	systems.reserve(NScheduledSystems);
	GAIA_FOR(NScheduledSystems) {
		const auto phase = phases[i % phases.size()];
		auto sys =
				w.system().phase(phase).all<const Position>().mode(ecs::QueryExecType::Default).on_each([&sink](ecs::Iter& it) {
					sink += it.entity_rows().size();
				});
		systems.push_back(sys.entity());
	}
	add_balanced_depends_on_edges(w, systems, phases.size());

	GAIA_FOR(4U) {
		w.update();
	}

	for (auto _: state) {
		(void)_;
		w.update();
	}

	dont_optimize(sink);
}

void BM_SystemSchedule_ParallelJobsAccess_256(picobench::state& state) {
	(void)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_schedule_probe_entities(w, entities, NSchedulerEntities);

	uint64_t sink = 0;
	GAIA_FOR(NScheduledSystems) {
		if ((i % 4U) == 0U) {
			w.system().all<Position&>().mode(ecs::QueryExecType::Parallel).on_each([&sink](ecs::Iter& it) {
				auto p = it.view_mut<Position>();
				GAIA_EACH(it) {
					p[i].x += 1.0f;
				}
				sink += it.entity_rows().size();
			});
			continue;
		}

		w.system()
				.all<const Position>()
				.reads<Acceleration>()
				.mode(ecs::QueryExecType::Parallel)
				.on_each([&sink](ecs::Iter& it) {
					sink += it.entity_rows().size();
				});
	}

	GAIA_FOR(4U) {
		w.update();
	}

	for (auto _: state) {
		(void)_;
		w.update();
	}

	dont_optimize(sink);
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
			PICOBENCH_REG(BM_SystemFrame_Identical_Local_16)
					.PICO_SETTINGS_FOCUS()
					.user_data(NEntitiesMedium)
					.label("identical local 16 systems");
			PICOBENCH_REG(BM_SystemFrame_Identical_Shared_16)
					.PICO_SETTINGS_FOCUS()
					.user_data(NEntitiesMedium)
					.label("identical shared 16 systems");
			PICOBENCH_REG(BM_System_Is_Semantic_D2).PICO_SETTINGS_FOCUS().user_data(1024).label("is semantic d2");
			PICOBENCH_REG(BM_System_Is_Direct_D2).PICO_SETTINGS_FOCUS().user_data(1024).label("is direct d2");
			PICOBENCH_REG(BM_System_Is_Semantic_D8).PICO_SETTINGS_FOCUS().user_data(1024).label("is semantic d8");
			PICOBENCH_REG(BM_System_Is_Direct_D8).PICO_SETTINGS_FOCUS().user_data(1024).label("is direct d8");
			PICOBENCH_REG(BM_System_IsIter_Semantic_D8).PICO_SETTINGS_FOCUS().user_data(1024).label("is iter semantic d8");
			PICOBENCH_REG(BM_System_IsIter_Direct_D8).PICO_SETTINGS_FOCUS().user_data(1024).label("is iter direct d8");
			PICOBENCH_REG(BM_SystemSchedule_SerialOrder_256).PICO_SETTINGS_FOCUS().label("systems serial order 256");
			PICOBENCH_REG(BM_SystemSchedule_PhasedOrder_256).PICO_SETTINGS_FOCUS().label("systems phased order 256");
			PICOBENCH_REG(BM_SystemSchedule_ParallelJobsAccess_256)
					.PICO_SETTINGS_HEAVY()
					.label("systems parallel jobs access 256");
			return;
		default:
			return;
	}
}
