#include "perf_matrix_common.h"
#include "perf_registry.h"

template <bool UseParent>
void add_hierarchy_edge(ecs::World& w, ecs::Entity entity, ecs::Entity parent) {
	if constexpr (UseParent)
		w.parent(entity, parent);
	else
		w.add(entity, ecs::Pair(ecs::ChildOf, parent));
}

template <bool UseParent>
void create_hierarchy_tree(ecs::World& w, cnt::darray<ecs::Entity>& entities, uint32_t count) {
	entities.clear();
	entities.reserve(count);

	GAIA_FOR(count) {
		const auto e = w.add();
		entities.push_back(e);

		if (i == 0)
			continue;

		const auto parentIdx = (i - 1U) / 4U;
		add_hierarchy_edge<UseParent>(w, e, entities[parentIdx]);
	}
}

template <bool UseParent>
void create_hierarchy_tree_with_position(ecs::World& w, cnt::darray<ecs::Entity>& entities, uint32_t count) {
	create_hierarchy_tree<UseParent>(w, entities, count);
	for (auto e: entities)
		w.add<Position>(e, {1.0f, 2.0f, 3.0f});
}

inline void disable_hierarchy_barrier(ecs::World& w, const cnt::darray<ecs::Entity>& entities) {
	if (entities.size() > 1)
		w.enable(entities[1], false);
}

template <bool UseParent>
void BM_Hierarchy_Set(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;

		ecs::World w;
		cnt::darray<ecs::Entity> entities;
		entities.reserve(n);

		const auto root = w.add();
		GAIA_FOR(n) {
			const auto e = w.add();
			entities.push_back(e);
		}

		state.start_timer();
		for (auto e: entities)
			add_hierarchy_edge<UseParent>(w, e, root);
		state.stop_timer();
	}
}

template <bool UseParent, bool WithDisabledBarrier = false>
void BM_Hierarchy_Bfs(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_hierarchy_tree<UseParent>(w, entities, n);
	if constexpr (WithDisabledBarrier)
		disable_hierarchy_barrier(w, entities);

	uint64_t visited = 0;
	auto relation = UseParent ? ecs::Parent : ecs::ChildOf;

	w.sources_bfs(relation, entities[0], [&](ecs::Entity) {
		++visited;
	});
	visited = 0;

	for (auto _: state) {
		(void)_;

		w.sources_bfs(relation, entities[0], [&](ecs::Entity) {
			++visited;
		});
	}

	dont_optimize(visited);
}

template <bool UseParent, bool WithDisabledBarrier = false>
void BM_Query_Bfs(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_hierarchy_tree_with_position<UseParent>(w, entities, n);
	if constexpr (WithDisabledBarrier)
		disable_hierarchy_barrier(w, entities);

	const auto relation = UseParent ? ecs::Parent : ecs::ChildOf;
	auto q = w.query().all<Position>();
	uint64_t visited = 0;

	q.bfs(relation).each([&](ecs::Entity) {
		++visited;
	});
	visited = 0;

	for (auto _: state) {
		(void)_;

		q.bfs(relation).each([&](ecs::Entity) {
			++visited;
		});
	}

	dont_optimize(visited);
}

void BM_Query_Plain_ChildOf(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_hierarchy_tree_with_position<false>(w, entities, n);

	auto q = w.query().all<Position>();
	uint64_t visited = 0;

	q.each([&](ecs::Entity) {
		++visited;
	});
	visited = 0;

	for (auto _: state) {
		(void)_;

		q.each([&](ecs::Entity) {
			++visited;
		});
	}

	dont_optimize(visited);
}

void BM_Query_Cascade_ChildOf(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_hierarchy_tree_with_position<false>(w, entities, n);

	auto q = w.query().all<Position>().cascade(ecs::ChildOf);
	uint64_t visited = 0;

	q.each([&](ecs::Entity) {
		++visited;
	});
	visited = 0;

	for (auto _: state) {
		(void)_;

		q.each([&](ecs::Entity) {
			++visited;
		});
	}

	dont_optimize(visited);
}

void BM_Query_Plain_ChildOf_Component(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_hierarchy_tree_with_position<false>(w, entities, n);

	auto q = w.query().all<Position>();
	float sum = 0.0f;

	q.each([&](ecs::Entity entity) {
		sum += w.get<Position>(entity).x;
	});
	sum = 0.0f;

	for (auto _: state) {
		(void)_;

		q.each([&](ecs::Entity entity) {
			sum += w.get<Position>(entity).x;
		});
	}

	dont_optimize(sum);
}

void BM_Query_Bfs_ChildOf_Component(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_hierarchy_tree_with_position<false>(w, entities, n);

	auto q = w.query().all<Position>();
	float sum = 0.0f;

	q.bfs(ecs::ChildOf).each([&](ecs::Entity entity) {
		sum += w.get<Position>(entity).x;
	});
	sum = 0.0f;

	for (auto _: state) {
		(void)_;

		q.bfs(ecs::ChildOf).each([&](ecs::Entity entity) {
			sum += w.get<Position>(entity).x;
		});
	}

	dont_optimize(sum);
}

void BM_Query_Cascade_ChildOf_Component(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_hierarchy_tree_with_position<false>(w, entities, n);

	auto q = w.query().all<Position>().cascade(ecs::ChildOf);
	float sum = 0.0f;

	q.each([&](ecs::Entity entity) {
		sum += w.get<Position>(entity).x;
	});
	sum = 0.0f;

	for (auto _: state) {
		(void)_;

		q.each([&](ecs::Entity entity) {
			sum += w.get<Position>(entity).x;
		});
	}

	dont_optimize(sum);
}

void BM_Query_Plain_ChildOf_EachComponent(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_hierarchy_tree_with_position<false>(w, entities, n);

	auto q = w.query().all<Position>();
	float sum = 0.0f;

	q.each([&](ecs::Entity, const Position& pos) {
		sum += pos.x;
	});
	sum = 0.0f;

	for (auto _: state) {
		(void)_;

		q.each([&](ecs::Entity, const Position& pos) {
			sum += pos.x;
		});
	}

	dont_optimize(sum);
}

void BM_Query_Bfs_ChildOf_EachComponent(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_hierarchy_tree_with_position<false>(w, entities, n);

	auto q = w.query().all<Position>();
	float sum = 0.0f;

	q.bfs(ecs::ChildOf).each([&](ecs::Entity, const Position& pos) {
		sum += pos.x;
	});
	sum = 0.0f;

	for (auto _: state) {
		(void)_;

		q.bfs(ecs::ChildOf).each([&](ecs::Entity, const Position& pos) {
			sum += pos.x;
		});
	}

	dont_optimize(sum);
}

void BM_Query_Cascade_ChildOf_EachComponent(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_hierarchy_tree_with_position<false>(w, entities, n);

	auto q = w.query().all<Position>().cascade(ecs::ChildOf);
	float sum = 0.0f;

	q.each([&](ecs::Entity, const Position& pos) {
		sum += pos.x;
	});
	sum = 0.0f;

	for (auto _: state) {
		(void)_;

		q.each([&](ecs::Entity, const Position& pos) {
			sum += pos.x;
		});
	}

	dont_optimize(sum);
}

void BM_Query_Plain_ChildOf_Iter(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_hierarchy_tree_with_position<false>(w, entities, n);

	auto q = w.query().all<Position>();
	float sum = 0.0f;

	q.each([&](ecs::Iter& it) {
		auto posView = it.view<Position>();
		GAIA_EACH(it) {
			sum += posView[i].x;
		}
	});
	sum = 0.0f;

	for (auto _: state) {
		(void)_;

		q.each([&](ecs::Iter& it) {
			auto posView = it.view<Position>();
			GAIA_EACH(it) {
				sum += posView[i].x;
			}
		});
	}

	dont_optimize(sum);
}

void BM_Query_Bfs_ChildOf_Iter(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_hierarchy_tree_with_position<false>(w, entities, n);

	auto q = w.query().all<Position>();
	float sum = 0.0f;

	q.bfs(ecs::ChildOf).each([&](ecs::Iter& it) {
		auto posView = it.view<Position>();
		GAIA_EACH(it) {
			sum += posView[i].x;
		}
	});
	sum = 0.0f;

	for (auto _: state) {
		(void)_;

		q.bfs(ecs::ChildOf).each([&](ecs::Iter& it) {
			auto posView = it.view<Position>();
			GAIA_EACH(it) {
				sum += posView[i].x;
			}
		});
	}

	dont_optimize(sum);
}

void BM_Query_Cascade_ChildOf_Iter(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_hierarchy_tree_with_position<false>(w, entities, n);

	auto q = w.query().all<Position>().cascade(ecs::ChildOf);
	float sum = 0.0f;

	q.each([&](ecs::Iter& it) {
		auto posView = it.view<Position>();
		GAIA_EACH(it) {
			sum += posView[i].x;
		}
	});
	sum = 0.0f;

	for (auto _: state) {
		(void)_;

		q.each([&](ecs::Iter& it) {
			auto posView = it.view<Position>();
			GAIA_EACH(it) {
				sum += posView[i].x;
			}
		});
	}

	dont_optimize(sum);
}

void BM_Hierarchy_Bfs_ChildOf_Disabled(picobench::state& state) {
	BM_Hierarchy_Bfs<false, true>(state);
}

void BM_Hierarchy_Bfs_Parent_Disabled(picobench::state& state) {
	BM_Hierarchy_Bfs<true, true>(state);
}

void BM_Query_Bfs_ChildOf_Disabled(picobench::state& state) {
	BM_Query_Bfs<false, true>(state);
}

void BM_Query_Bfs_Parent_Disabled(picobench::state& state) {
	BM_Query_Bfs<true, true>(state);
}

template <bool UseParent>
void BM_Hierarchy_TargetWalk(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_hierarchy_tree<UseParent>(w, entities, n);

	const auto relation = UseParent ? ecs::Parent : ecs::ChildOf;
	const auto leaf = entities.back();
	uint64_t steps = 0;

	for (auto _: state) {
		(void)_;

		auto curr = leaf;
		while (true) {
			const auto parent = w.target(curr, relation);
			if (parent == ecs::EntityBad)
				break;

			curr = parent;
			++steps;
		}
	}

	dont_optimize(steps);
}

template <bool UseParent>
void BM_Hierarchy_Sources(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_hierarchy_tree<UseParent>(w, entities, n);

	const auto relation = UseParent ? ecs::Parent : ecs::ChildOf;
	uint64_t visited = 0;

	for (auto _: state) {
		(void)_;

		w.sources(relation, entities[0], [&](ecs::Entity) {
			++visited;
		});
	}

	dont_optimize(visited);
}

void BM_Relationship_SourcesWildcard(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_linear_entities<false, false, false, false, false>(w, entities, n);

	const auto root = w.add();
	const auto rel0 = w.add();
	const auto rel1 = w.add();
	const auto rel2 = w.add();
	const ecs::Entity rels[] = {rel0, rel1, rel2};

	const auto cnt = (uint32_t)entities.size();
	GAIA_FOR(cnt) {
		w.add(entities[i], ecs::Pair(rels[i % 3U], root));
	}

	uint64_t visited = 0;
	for (auto _: state) {
		(void)_;

		w.sources(ecs::All, root, [&](ecs::Entity) {
			++visited;
		});
	}

	dont_optimize(visited);
}

void BM_Relationship_TargetsWildcard(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	const auto source = w.add();

	cnt::darray<ecs::Entity> targets;
	targets.reserve(n);
	GAIA_FOR(n) {
		// A single entity can't legally fragment into 10K pair ids because archetypes cap the
		// number of stored terms. Use Exclusive+DontFragment relations so the wildcard helper still
		// traverses 10K direct targets without inflating the source archetype.
		const auto rel = w.add();
		w.add(rel, ecs::Exclusive);
		w.add(rel, ecs::DontFragment);
		const auto target = w.add();
		targets.push_back(target);
		w.add(source, ecs::Pair(rel, target));
	}

	uint64_t visited = 0;
	for (auto _: state) {
		(void)_;

		w.targets(source, ecs::All, [&](ecs::Entity) {
			++visited;
		});
	}

	dont_optimize(visited);
}

template <bool UseParent>
void BM_Hierarchy_DeleteTarget(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;

		ecs::World w;
		cnt::darray<ecs::Entity> entities;
		entities.reserve(n);

		const auto root = w.add();
		GAIA_FOR(n) {
			const auto e = w.add();
			entities.push_back(e);
			add_hierarchy_edge<UseParent>(w, e, root);
		}

		state.start_timer();
		w.del(root);
		w.update();
		state.stop_timer();
	}
}

template <bool UseParent>
void BM_Query_DirectHierarchy_All(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	const auto rootA = w.add();
	const auto rootB = w.add();

	cnt::darray<ecs::Entity> entities;
	entities.reserve(n);

	GAIA_FOR(n) {
		const auto e = w.add();
		entities.push_back(e);
		w.add<Position>(e);
		add_hierarchy_edge<UseParent>(w, e, (i & 1U) == 0 ? rootA : rootB);
	}

	auto q = w.query().all<Position>().all(ecs::Pair(UseParent ? ecs::Parent : ecs::ChildOf, rootA));
	uint32_t total = 0;

	for (auto _: state) {
		(void)_;
		total += q.count();
	}

	dont_optimize(total);
}

template <bool UseParent>
void BM_Query_DirectHierarchy_Each(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	const auto rootA = w.add();
	const auto rootB = w.add();

	cnt::darray<ecs::Entity> entities;
	entities.reserve(n);

	GAIA_FOR(n) {
		const auto e = w.add();
		entities.push_back(e);
		w.add<Position>(e, {(float)i, (float)(i + 1U), (float)(i + 2U)});
		add_hierarchy_edge<UseParent>(w, e, (i & 1U) == 0 ? rootA : rootB);
	}

	auto q = w.query().all<Position>().all(ecs::Pair(UseParent ? ecs::Parent : ecs::ChildOf, rootA));
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

template <bool UseParent>
void BM_Query_DirectHierarchy_Or(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	const auto rootA = w.add();
	const auto rootB = w.add();

	cnt::darray<ecs::Entity> entities;
	entities.reserve(n);

	GAIA_FOR(n) {
		const auto e = w.add();
		entities.push_back(e);
		if ((i & 1U) == 0)
			add_hierarchy_edge<UseParent>(w, e, rootA);
		else
			add_hierarchy_edge<UseParent>(w, e, rootB);

		if ((i % 4U) == 1U)
			w.add<Acceleration>(e);
	}

	auto q = w.query().or_(ecs::Pair(UseParent ? ecs::Parent : ecs::ChildOf, rootA)).or_<Acceleration>();
	uint32_t total = 0;

	for (auto _: state) {
		(void)_;
		total += q.count();
	}

	dont_optimize(total);
}

template <bool UseParent>
void BM_Query_DirectHierarchy_Or_Each(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	const auto rootA = w.add();
	const auto rootB = w.add();

	cnt::darray<ecs::Entity> entities;
	entities.reserve(n);

	GAIA_FOR(n) {
		const auto e = w.add();
		entities.push_back(e);
		w.add<Position>(e, {(float)i, (float)(i + 1U), (float)(i + 2U)});
		if ((i & 1U) == 0)
			add_hierarchy_edge<UseParent>(w, e, rootA);
		else
			add_hierarchy_edge<UseParent>(w, e, rootB);

		if ((i % 4U) == 1U)
			w.add<Acceleration>(e);
	}

	auto q = w.query().all<Position>().or_(ecs::Pair(UseParent ? ecs::Parent : ecs::ChildOf, rootA)).or_<Acceleration>();
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

#define PICO_SETTINGS() iterations({256}).samples(3)
#define PICO_SETTINGS_HEAVY() iterations({64}).samples(3)
#define PICO_SETTINGS_FOCUS() iterations({256}).samples(7)
#define PICO_SETTINGS_OBS() iterations({64}).samples(3)
#define PICO_SETTINGS_SANI() iterations({8}).samples(1)
#define PICOBENCH_SUITE_REG(name) (void)picobench::global_registry::set_bench_suite(name)
#define PICOBENCH_REG(func) picobench::global_registry::new_benchmark(#func, func)

void register_perf_matrix_parent(PerfRunMode mode) {
	if (mode != PerfRunMode::Normal)
		return;

	PICOBENCH_SUITE_REG("Structural changes");
	PICOBENCH_REG(BM_Hierarchy_Set<false>).PICO_SETTINGS_FOCUS().user_data(NEntitiesFew).label("childof set 10K");
	PICOBENCH_REG(BM_Hierarchy_Set<true>).PICO_SETTINGS_FOCUS().user_data(NEntitiesFew).label("parent set 10K");
	PICOBENCH_REG(BM_Query_DirectHierarchy_All<false>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("childof query all 10K");
	PICOBENCH_REG(BM_Query_DirectHierarchy_All<true>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("parent query all 10K");
	PICOBENCH_REG(BM_Query_DirectHierarchy_Each<false>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("childof query each 10K");
	PICOBENCH_REG(BM_Query_DirectHierarchy_Each<true>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("parent query each 10K");
	PICOBENCH_REG(BM_Query_DirectHierarchy_Or<false>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("childof query or 10K");
	PICOBENCH_REG(BM_Query_DirectHierarchy_Or<true>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("parent query or 10K");
	PICOBENCH_REG(BM_Query_DirectHierarchy_Or_Each<false>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("childof query or each 10K");
	PICOBENCH_REG(BM_Query_DirectHierarchy_Or_Each<true>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("parent query or each 10K");

	PICOBENCH_SUITE_REG("Traversal helpers");
	PICOBENCH_REG(BM_Hierarchy_TargetWalk<false>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("childof up-walk 10K");
	PICOBENCH_REG(BM_Hierarchy_TargetWalk<true>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("parent up-walk 10K");
	PICOBENCH_REG(BM_Hierarchy_Sources<false>).PICO_SETTINGS_FOCUS().user_data(NEntitiesFew).label("childof sources 10K");
	PICOBENCH_REG(BM_Hierarchy_Sources<true>).PICO_SETTINGS_FOCUS().user_data(NEntitiesFew).label("parent sources 10K");
	PICOBENCH_REG(BM_Relationship_SourcesWildcard)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("wildcard sources 10K");
	PICOBENCH_REG(BM_Relationship_TargetsWildcard)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("wildcard targets 10K");
	PICOBENCH_REG(BM_Hierarchy_Bfs<false>).PICO_SETTINGS_FOCUS().user_data(NEntitiesFew).label("childof bfs 10K");
	PICOBENCH_REG(BM_Hierarchy_Bfs<true>).PICO_SETTINGS_FOCUS().user_data(NEntitiesFew).label("parent bfs 10K");
	PICOBENCH_REG(BM_Hierarchy_Bfs_ChildOf_Disabled)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("childof bfs disabled barrier 10K");
	PICOBENCH_REG(BM_Hierarchy_Bfs_Parent_Disabled)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("parent bfs disabled barrier 10K");
	PICOBENCH_REG(BM_Query_Plain_ChildOf).PICO_SETTINGS_FOCUS().user_data(NEntitiesFew).label("query childof plain 10K");
	PICOBENCH_REG(BM_Query_Plain_ChildOf_Component)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("query childof plain get 10K");
	PICOBENCH_REG(BM_Query_Bfs<false>).PICO_SETTINGS_FOCUS().user_data(NEntitiesFew).label("query childof bfs 10K");
	PICOBENCH_REG(BM_Query_Bfs_ChildOf_Component)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("query childof bfs get 10K");
	PICOBENCH_REG(BM_Query_Bfs<true>).PICO_SETTINGS_FOCUS().user_data(NEntitiesFew).label("query parent bfs 10K");
	PICOBENCH_REG(BM_Query_Cascade_ChildOf)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("query childof cascade 10K");
	PICOBENCH_REG(BM_Query_Cascade_ChildOf_Component)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("query childof cascade get 10K");
	PICOBENCH_REG(BM_Query_Plain_ChildOf_EachComponent)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("query childof plain each pos 10K");
	PICOBENCH_REG(BM_Query_Bfs_ChildOf_EachComponent)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("query childof bfs each pos 10K");
	PICOBENCH_REG(BM_Query_Cascade_ChildOf_EachComponent)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("query childof cascade each pos 10K");
	PICOBENCH_REG(BM_Query_Plain_ChildOf_Iter)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("query childof plain iter pos 10K");
	PICOBENCH_REG(BM_Query_Bfs_ChildOf_Iter)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("query childof bfs iter pos 10K");
	PICOBENCH_REG(BM_Query_Cascade_ChildOf_Iter)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("query childof cascade iter pos 10K");
	PICOBENCH_REG(BM_Query_Bfs_ChildOf_Disabled)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("query childof bfs disabled barrier 10K");
	PICOBENCH_REG(BM_Query_Bfs_Parent_Disabled)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("query parent bfs disabled barrier 10K");
	PICOBENCH_SUITE_REG("Query cache maintenance");
	PICOBENCH_REG(BM_Hierarchy_DeleteTarget<false>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("childof delete target 10K");
	PICOBENCH_REG(BM_Hierarchy_DeleteTarget<true>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("parent delete target 10K");
}
