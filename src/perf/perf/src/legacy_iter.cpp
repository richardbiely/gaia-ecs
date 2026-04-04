#include "common.h"
#include "registry.h"

constexpr uint32_t EntitiesPerArchetype = 1'000;

using TBenchmarkTypes = cnt::darray<ecs::Entity>;
struct Rotation {
	float x, y, z, w;
};
struct Scale {
	float x, y, z;
};
struct Direction {
	float x, y, z;
};
struct IsEnemy {
	bool value;
};
struct Dummy {
	int value[24];
};

template <bool FillWithManyEntities>
auto create_archetypes(ecs::World& w, uint32_t archetypes, uint32_t maxIdsPerArchetype, ecs::Entity* specialEntity) {
	GAIA_PROF_SCOPE(create_archetypes);

	GAIA_ASSERT(archetypes > 0);
	GAIA_ASSERT(maxIdsPerArchetype > 0);

	// One entity that is going to be present everywhere.
	if (specialEntity != nullptr)
		*specialEntity = w.add();

	TBenchmarkTypes types;
	types.resize(archetypes);
	GAIA_FOR(archetypes) types[i] = w.add();

	// Each archetype can contain only so many components.
	// Therefore, we will be creating archetypes with up to "maxIdsPerArchetype" ones.

	auto group = BadIndex;
	ecs::Entity e;
	GAIA_FOR(archetypes) {
		if (i % maxIdsPerArchetype == 0) {
			if (i != 0) {
				if constexpr (FillWithManyEntities) {
					GAIA_FOR_(EntitiesPerArchetype - 1U, j)(void) w.copy(e);
				} else {
					(void)w.copy(e);
				}
			}

			++group;
			e = w.add();
			if (specialEntity != nullptr)
				w.add(e, *specialEntity);
		}

		w.add(e, types[(i % maxIdsPerArchetype) + (group * maxIdsPerArchetype)]);
	}

	return types;
}

void prepare_query_types(ecs::EntitySpan in, ecs::EntitySpanMut out) {
	const auto size = (uint32_t)out.size();
	GAIA_FOR2(0, size) {
		out[i] = (const ecs::Entity)in[i];
	}
}

void prepare_query_types(ecs::Entity specialEntity, ecs::EntitySpan in, ecs::EntitySpanMut out) {
	const auto size = (uint32_t)out.size();
	out[0] = specialEntity;
	GAIA_FOR2(1, size) {
		out[i] = (const ecs::Entity)in[i];
	}
}

template <bool UseCachedQuery, ecs::QueryCacheScope CacheScope = ecs::QueryCacheScope::Local>
auto create_query(ecs::World& w, ecs::EntitySpan queryTypes) {
	GAIA_PROF_SCOPE(create_query);

	GAIA_ASSERT(!queryTypes.empty());

	auto query = make_query<UseCachedQuery>(w);
	if constexpr (UseCachedQuery) {
		query.scope(CacheScope);
	}
	for (auto e: queryTypes)
		query.all(e);

	return query;
}

void bench_query_each(picobench::state& state, ecs::Query& query) {
	GAIA_PROF_SCOPE(bench_query_each);

	/* We want to benchmark the hot-path. In real-world scenarios queries are cached so cache them now */
	gaia::dont_optimize(query.empty());

	state.stop_timer();
	for (auto _: state) {
		GAIA_PROF_SCOPE(update);
		(void)_;

		state.start_timer();

		uint32_t cnt = 0;
		query.each([&]() {
			++cnt;
		});
		gaia::dont_optimize(cnt);

		state.stop_timer();
	}
}

template <ecs::Constraints Constraints, typename TQuery>
void bench_query_each_iter(picobench::state& state, TQuery& query) {
	GAIA_PROF_SCOPE(bench_query_each_iter);

	/* We want to benchmark the hot-path. In real-world scenarios queries are cached so cache them now */
	[[maybe_unused]] bool isEmpty = query.empty();
	gaia::dont_optimize(isEmpty);

	state.stop_timer();
	for (auto _: state) {
		GAIA_PROF_SCOPE(update);
		(void)_;

		state.start_timer();

		uint32_t iters = 0;
		query.each([&](ecs::Iter& it) {
			iters += it.size();
		}, Constraints);
		gaia::dont_optimize(iters);

		state.stop_timer();
	}
}

template <typename T>
void acc_view(ecs::Iter& it, uint32_t idx) {
	auto v = it.template view<T>(idx);
	const auto* d = v.data();
	gaia::dont_optimize(d);
}

template <typename T>
void acc_view(ecs::Iter& it) {
	auto v = it.template view<T>();
	const auto* d = v.data();
	gaia::dont_optimize(d);
}

template <uint32_t NViews, bool ViewWithIndex>
void bench_query_each_view(picobench::state& state, ecs::World& w) {
	GAIA_PROF_SCOPE(bench_query_each_view);

	/* We want to benchmark the hot-path. In real-world scenarios queries are cached so cache them now */
	auto q = w.query().all<Position>();
	if constexpr (NViews > 1)
		q.all<Velocity>();
	if constexpr (NViews > 2)
		q.all<Scale>();
	if constexpr (NViews > 3)
		q.all<Direction>();
	if constexpr (NViews > 4)
		q.all<Health>();

	[[maybe_unused]] bool isEmpty = q.empty();
	gaia::dont_optimize(isEmpty);

	state.stop_timer();
	for (auto _: state) {
		GAIA_PROF_SCOPE(update);
		(void)_;

		state.stop_timer();
		q.each([&](ecs::Iter& it) {
			const auto cnt = it.size();
			GAIA_FOR(cnt) {
				state.start_timer();

				if constexpr (ViewWithIndex) {
					acc_view<Position>(it, 0);
					if constexpr (NViews > 1)
						acc_view<Velocity>(it, 1);
					if constexpr (NViews > 2)
						acc_view<Scale>(it, 2);
					if constexpr (NViews > 3)
						acc_view<Direction>(it, 3);
					if constexpr (NViews > 4)
						acc_view<Health>(it, 4);
				} else {
					acc_view<Position>(it);
					if constexpr (NViews > 1)
						acc_view<Velocity>(it);
					if constexpr (NViews > 2)
						acc_view<Scale>(it);
					if constexpr (NViews > 3)
						acc_view<Direction>(it);
					if constexpr (NViews > 4)
						acc_view<Health>(it);
				}

				state.stop_timer();
			}
		});
	}
}

template <bool UseCachedQuery, uint32_t QueryComponents>
void bench_build_query(picobench::state& state, ecs::World& w, ecs::EntitySpan types, ecs::Entity specialEntity) {
	GAIA_PROF_SCOPE(bench_build_query);

	state.stop_timer();

	ecs::Entity tmp[QueryComponents + 1];
	prepare_query_types(specialEntity, {types.data(), types.size()}, tmp);

	for (auto _: state) {
		GAIA_PROF_SCOPE(update);
		(void)_;

		auto query = create_query<UseCachedQuery>(w, tmp);

		// Measure the time to build the query
		state.start_timer();
		auto& queryInfo = query.fetch();
		query.match_all(queryInfo);
		state.stop_timer();
	}
}

template <bool UseCachedQuery, uint32_t QueryComponents>
void BM_BuildQuery(picobench::state& state) {
	ecs::World w;
	ecs::Entity specialEntity;
	// Create some archetypes for a good measure
	auto types = create_archetypes<false>(w, 10000, 10, &specialEntity);
	bench_build_query<UseCachedQuery, QueryComponents>(state, w, types, specialEntity);
}

template <uint32_t QueryComponents>
void BM_BuildQuery_Shared(picobench::state& state) {
	ecs::World w;
	ecs::Entity specialEntity;
	auto types = create_archetypes<false>(w, 10000, 10, &specialEntity);

	GAIA_PROF_SCOPE(bench_build_query);

	state.stop_timer();

	ecs::Entity tmp[QueryComponents + 1];
	prepare_query_types(specialEntity, {types.data(), types.size()}, tmp);

	for (auto _: state) {
		GAIA_PROF_SCOPE(update);
		(void)_;

		auto query = create_query<true, ecs::QueryCacheScope::Shared>(w, tmp);

		state.start_timer();
		auto& queryInfo = query.fetch();
		query.match_all(queryInfo);
		state.stop_timer();
	}
}

#define DEFINE_BUILD_QUERY(QueryComponents)                                                                            \
	void BM_BuildQuery_##QueryComponents(picobench::state& state) {                                                      \
		GAIA_PROF_SCOPE(BM_BuildQuery_##QueryComponents);                                                                  \
		BM_BuildQuery<true, QueryComponents>(state);                                                                       \
	}

#define DEFINE_BUILD_QUERY_U(QueryComponents)                                                                          \
	void BM_BuildQuery_U_##QueryComponents(picobench::state& state) {                                                    \
		GAIA_PROF_SCOPE(BM_BuildQuery_U_##QueryComponents);                                                                \
		BM_BuildQuery<false, QueryComponents>(state);                                                                      \
	}

#define DEFINE_BUILD_QUERY_S(QueryComponents)                                                                          \
	void BM_BuildQuery_S_##QueryComponents(picobench::state& state) {                                                    \
		GAIA_PROF_SCOPE(BM_BuildQuery_S_##QueryComponents);                                                                \
		BM_BuildQuery_Shared<QueryComponents>(state);                                                                      \
	}

DEFINE_BUILD_QUERY(1)
DEFINE_BUILD_QUERY_U(1)
DEFINE_BUILD_QUERY_S(1)
DEFINE_BUILD_QUERY(3)
DEFINE_BUILD_QUERY_U(3)
DEFINE_BUILD_QUERY_S(3)
DEFINE_BUILD_QUERY(5)
DEFINE_BUILD_QUERY_U(5)
DEFINE_BUILD_QUERY_S(5)
DEFINE_BUILD_QUERY(7)
DEFINE_BUILD_QUERY_U(7)
DEFINE_BUILD_QUERY_S(7)

#define DEFINE_EACH(ArchetypeCount, MaxIdsPerArchetype, QueryComponents)                                               \
	void BM_Each_##ArchetypeCount##_##QueryComponents(picobench::state& state) {                                         \
		GAIA_PROF_SCOPE(BM_Each_##ArchetypeCount##_##QueryComponents);                                                     \
		ecs::World w;                                                                                                      \
		auto types = create_archetypes<true>(w, ArchetypeCount, MaxIdsPerArchetype, nullptr);                              \
		ecs::Entity tmp[(QueryComponents)];                                                                                \
		prepare_query_types({types.data(), types.size()}, tmp);                                                            \
		auto query = create_query<true>(w, tmp);                                                                           \
		bench_query_each(state, query);                                                                                    \
	}

DEFINE_EACH(1, 1, 1)
DEFINE_EACH(100, 10, 1)
DEFINE_EACH(1000, 10, 1)

template <bool UseCachedQuery, uint32_t QueryComponents, ecs::Constraints Constraints>
void BM_Each_Iter(picobench::state& state, uint32_t ArchetypeCount, uint32_t MaxIdsPerArchetype) {
	ecs::World w;
	auto types = create_archetypes<true>(w, ArchetypeCount, MaxIdsPerArchetype, nullptr);
	ecs::Entity tmp[QueryComponents];
	prepare_query_types({types.data(), types.size()}, tmp);
	auto query = create_query<UseCachedQuery>(w, tmp);
	bench_query_each_iter<Constraints>(state, query);
}

template <uint32_t NViews, bool ViewWithIndex>
void BM_Each_View(picobench::state& state) {
	ecs::World w;

	// Create some archetypes for a good measure
	create_archetypes<true>(w, 1000, 10, nullptr);

	// Register our components. The order is random so it does not match
	// the order in queries.
	{
		(void)w.add<Scale>();
		(void)w.add<Position>();
		(void)w.add<Direction>();
		(void)w.add<Health>();
		(void)w.add<IsEnemy>();
		(void)w.add<Velocity>();
		(void)w.add<Rotation>();
	}

	// Create our native component archetypes
	constexpr uint32_t NEntities = 1000;
	{
		auto e = w.add();
		w.build(e) //
				.add<Position>()
				.add<Velocity>();
		w.copy_n(e, NEntities - 1);
	}
	{
		auto e = w.add();
		w.build(e) //
				.add<Position>()
				.add<Velocity>()
				.add<Rotation>();
		w.copy_n(e, NEntities - 1);
	}
	{
		auto e = w.add();
		w.build(e) //
				.add<Position>()
				.add<Velocity>()
				.add<Rotation>()
				.add<Scale>();
		w.copy_n(e, NEntities - 1);
	}
	{
		auto e = w.add();
		w.build(e) //
				.add<Position>()
				.add<Velocity>()
				.add<Rotation>()
				.add<Scale>()
				.add<Direction>();
		w.copy_n(e, NEntities - 1);
	}
	{
		auto e = w.add();
		w.build(e) //
				.add<Position>()
				.add<Velocity>()
				.add<Rotation>()
				.add<Scale>()
				.add<Direction>()
				.add<Health>();
		w.copy_n(e, NEntities - 1);
	}
	{
		auto e = w.add();
		w.build(e) //
				.add<Position>()
				.add<Velocity>()
				.add<Rotation>()
				.add<Scale>()
				.add<Direction>()
				.add<Health>()
				.add<IsEnemy>();
		w.copy_n(e, NEntities - 1);
	}

	bench_query_each_view<NViews, ViewWithIndex>(state, w);
}

#define DEFINE_EACH_ITER(Name, Constraints, ArchetypeCount, MaxIdsPerArchetype, QueryComponents)                      \
	void BM_Each_##Name##_##ArchetypeCount##_##QueryComponents(picobench::state& state) {                                \
		GAIA_PROF_SCOPE(BM_Each_##Name##_##ArchetypeCount##_##QueryComponents);                                            \
		BM_Each_Iter<true, QueryComponents, Constraints>(state, ArchetypeCount, MaxIdsPerArchetype);                      \
	}
#define DEFINE_EACH_U_ITER(Name, Constraints, ArchetypeCount, MaxIdsPerArchetype, QueryComponents)                    \
	void BM_Each_U_##Name##_##ArchetypeCount##_##QueryComponents(picobench::state& state) {                              \
		GAIA_PROF_SCOPE(BM_Each_U_##Name##_##ArchetypeCount##_##QueryComponents);                                          \
		BM_Each_Iter<false, QueryComponents, Constraints>(state, ArchetypeCount, MaxIdsPerArchetype);                     \
	}

#define DEFINE_EACH_VIEW(NViews, ViewWithIndex)                                                                        \
	void BM_Each_View_##NViews##_##ViewWithIndex(picobench::state& state) {                                              \
		GAIA_PROF_SCOPE(BM_Each_View_##NViews##_##ViewWithIndex);                                                          \
		BM_Each_View<NViews, ViewWithIndex>(state);                                                                        \
	}

DEFINE_EACH_ITER(IterAcceptAll, ecs::Constraints::AcceptAll, 1, 1, 1)
DEFINE_EACH_ITER(Iter, ecs::Constraints::EnabledOnly, 1, 1, 1)
DEFINE_EACH_U_ITER(Iter, ecs::Constraints::EnabledOnly, 1, 1, 1)

DEFINE_EACH_ITER(IterAcceptAll, ecs::Constraints::AcceptAll, 100, 10, 1)
DEFINE_EACH_ITER(Iter, ecs::Constraints::EnabledOnly, 100, 10, 1)
DEFINE_EACH_U_ITER(Iter, ecs::Constraints::EnabledOnly, 100, 10, 1)

DEFINE_EACH_ITER(IterAcceptAll, ecs::Constraints::AcceptAll, 1000, 10, 1)
DEFINE_EACH_ITER(Iter, ecs::Constraints::EnabledOnly, 1000, 10, 1)
DEFINE_EACH_U_ITER(Iter, ecs::Constraints::EnabledOnly, 1000, 10, 1)

DEFINE_EACH_ITER(Iter, ecs::Constraints::EnabledOnly, 1000, 10, 3)
DEFINE_EACH_ITER(Iter, ecs::Constraints::EnabledOnly, 1000, 10, 5)
DEFINE_EACH_ITER(Iter, ecs::Constraints::EnabledOnly, 1000, 10, 7)

DEFINE_EACH_U_ITER(Iter, ecs::Constraints::EnabledOnly, 1000, 10, 3)
DEFINE_EACH_U_ITER(Iter, ecs::Constraints::EnabledOnly, 1000, 10, 5)
DEFINE_EACH_U_ITER(Iter, ecs::Constraints::EnabledOnly, 1000, 10, 7)

DEFINE_EACH_VIEW(1, false)
DEFINE_EACH_VIEW(1, true)
DEFINE_EACH_VIEW(2, false)
DEFINE_EACH_VIEW(2, true)
DEFINE_EACH_VIEW(3, false)
DEFINE_EACH_VIEW(3, true)
DEFINE_EACH_VIEW(4, false)
DEFINE_EACH_VIEW(4, true)
DEFINE_EACH_VIEW(5, false)
DEFINE_EACH_VIEW(5, true)

#undef PICO_SETTINGS
#undef PICO_SETTINGS_SANI
#define PICO_SETTINGS() iterations({8192}).samples(3)
#define PICO_SETTINGS_1() iterations({8192}).samples(1)
#define PICO_SETTINGS_2() iterations({32768}).samples(3)
#define PICO_SETTINGS_SANI() iterations({8}).samples(1)

void register_legacy_iter(PerfRunMode mode) {
	switch (mode) {
		case PerfRunMode::Profiling:
			PICOBENCH_SUITE_REG("1000 archetypes");
			PICOBENCH_REG(BM_Each_Iter_1000_7).PICO_SETTINGS().label("Iter, 7 comps");
			return;
		case PerfRunMode::Sanitizer:
			PICOBENCH_REG(BM_Each_Iter_1000_7).PICO_SETTINGS_SANI().label("Iter, 7 comps");
			PICOBENCH_REG(BM_Each_U_Iter_1000_7).PICO_SETTINGS_SANI().label("(u) 7 comps");
			return;
		case PerfRunMode::Normal:
			PICOBENCH_SUITE_REG("1 archetype");
			PICOBENCH_REG(BM_Each_1_1).PICO_SETTINGS().label("each, 1 comp");
			PICOBENCH_REG(BM_Each_IterAcceptAll_1_1).PICO_SETTINGS().label("Iter AcceptAll, 1 comp");
			PICOBENCH_REG(BM_Each_Iter_1_1).PICO_SETTINGS().label("Iter, 1 comp");
			PICOBENCH_REG(BM_Each_U_Iter_1_1).PICO_SETTINGS().label("(u) Iter, 1 comp");
			PICOBENCH_SUITE_REG("100 archetypes");
			PICOBENCH_REG(BM_Each_100_1).PICO_SETTINGS().label("each, 1 comp");
			PICOBENCH_REG(BM_Each_IterAcceptAll_100_1).PICO_SETTINGS().label("Iter AcceptAll, 1 comp");
			PICOBENCH_REG(BM_Each_Iter_100_1).PICO_SETTINGS().label("Iter, 1 comp");
			PICOBENCH_REG(BM_Each_U_Iter_100_1).PICO_SETTINGS().label("(u) Iter, 1 comp");
			PICOBENCH_SUITE_REG("1000 archetypes");
			PICOBENCH_REG(BM_Each_1000_1).PICO_SETTINGS().label("each, 1 comp");
			PICOBENCH_REG(BM_Each_IterAcceptAll_1000_1).PICO_SETTINGS().label("Iter AcceptAll, 1 comp");
			PICOBENCH_REG(BM_Each_Iter_1000_1).PICO_SETTINGS().label("Iter, 1 comp");
			PICOBENCH_REG(BM_Each_U_Iter_1000_1).PICO_SETTINGS().label("(u) Iter, 1 comp");
			PICOBENCH_SUITE_REG("1000 archetypes, Iter");
			PICOBENCH_REG(BM_Each_Iter_1000_3).PICO_SETTINGS().label("3 comps");
			PICOBENCH_REG(BM_Each_U_Iter_1000_3).PICO_SETTINGS().label("(u) 3 comps");
			PICOBENCH_REG(BM_Each_Iter_1000_5).PICO_SETTINGS().label("5 comps");
			PICOBENCH_REG(BM_Each_U_Iter_1000_5).PICO_SETTINGS().label("(u) 5 comps");
			PICOBENCH_REG(BM_Each_Iter_1000_7).PICO_SETTINGS().label("7 comps");
			PICOBENCH_REG(BM_Each_U_Iter_1000_7).PICO_SETTINGS().label("(u) 7 comps");
			PICOBENCH_SUITE_REG("build query");
			PICOBENCH_REG(BM_BuildQuery_1).PICO_SETTINGS_2().label("1 comp ");
			PICOBENCH_REG(BM_BuildQuery_U_1).PICO_SETTINGS_2().label("(u) 1 comp ");
			PICOBENCH_REG(BM_BuildQuery_S_1).PICO_SETTINGS_2().label("(s) 1 comp ");
			PICOBENCH_REG(BM_BuildQuery_3).PICO_SETTINGS_2().label(" 3 comps");
			PICOBENCH_REG(BM_BuildQuery_U_3).PICO_SETTINGS_2().label("(u) 3 comps");
			PICOBENCH_REG(BM_BuildQuery_S_3).PICO_SETTINGS_2().label("(s) 3 comps");
			PICOBENCH_REG(BM_BuildQuery_5).PICO_SETTINGS_2().label("5 comps");
			PICOBENCH_REG(BM_BuildQuery_U_5).PICO_SETTINGS_2().label("(u) 5 comps");
			PICOBENCH_REG(BM_BuildQuery_S_5).PICO_SETTINGS_2().label("(s) 5 comps");
			PICOBENCH_REG(BM_BuildQuery_7).PICO_SETTINGS_2().label("7 comps");
			PICOBENCH_REG(BM_BuildQuery_U_7).PICO_SETTINGS_2().label("(u) 7 comps");
			PICOBENCH_REG(BM_BuildQuery_S_7).PICO_SETTINGS_2().label("(s) 7 comps");
			return;
	}
}
