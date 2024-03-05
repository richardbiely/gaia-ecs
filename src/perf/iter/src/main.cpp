#define PICOBENCH_IMPLEMENT
#include <gaia.h>
#include <picobench/picobench.hpp>

using namespace gaia;

constexpr uint32_t EntitiesPerArchetype = 1'000;

using TBenchmarkTypes = cnt::darray<ecs::Entity>;

struct Position {
	float x, y, z;
};
struct Velocity {
	float x, y, z;
};
struct Rotation {
	float x, y, z, w;
};
struct Scale {
	float x, y, z;
};
struct Direction {
	float x, y, z;
};
struct Health {
	int value;
	int max;
};
struct IsEnemy {
	bool value;
};
struct Dummy {
	int value[24];
};

auto create_archetypes(ecs::World& w, uint32_t archetypes, uint32_t maxIdsPerArchetype) {
	GAIA_ASSERT(archetypes > 0);
	GAIA_ASSERT(maxIdsPerArchetype > 0);

	TBenchmarkTypes types;
	GAIA_FOR(archetypes) types.push_back(w.add());

	// Each archetype can contain only so many components.
	// Therefore, we will be creating archetypes with up to "maxIdsPerArchetype" ones.

	auto group = (uint32_t)-1;
	ecs::Entity e;
	GAIA_FOR(archetypes) {
		if (i % maxIdsPerArchetype == 0) {
			if (i != 0) {
				GAIA_FOR_(EntitiesPerArchetype - 1U, j)(void) w.copy(e);
			}

			++group;
			e = w.add();
		}

		w.add(e, types[(i % maxIdsPerArchetype) + group * maxIdsPerArchetype]);
	}

	return types;
}

void prepare_query_types(ecs::EntitySpan in, std::span<ecs::Entity> out) {
	GAIA_FOR((uint32_t)out.size()) {
		out[i] = in[i];
	}
}

template <bool UseCachedQuery>
auto create_query(ecs::World& w, ecs::EntitySpan queryTypes) {
	GAIA_ASSERT(!queryTypes.empty());

	auto query = w.query<UseCachedQuery>();
	for (auto e: queryTypes)
		query.all(e);

	return query;
}

void bench_query_each(picobench::state& state, ecs::Query& query) {
	/* We want to benchmark the hot-path. In real-world scenarios queries are cached so cache them now */
	gaia::dont_optimize(query.empty());

	for (auto _: state) {
		(void)_;
		uint32_t cnt = 0;
		query.each([&]() {
			++cnt;
		});
		gaia::dont_optimize(cnt);
	}
}

template <typename TIter, typename TQuery>
void bench_query_each_iter(picobench::state& state, TQuery& query) {
	/* We want to benchmark the hot-path. In real-world scenarios queries are cached so cache them now */
	[[maybe_unused]] bool isEmpty = query.empty();
	gaia::dont_optimize(isEmpty);

	for (auto _: state) {
		(void)_;
		uint32_t cnt = 0;
		query.each([&](TIter& it) {
			GAIA_EACH(it) {
				++cnt;
			}
		});
		gaia::dont_optimize(cnt);
	}
}

template <typename T>
void acc_view(ecs::Iter& it, uint32_t idx) {
	auto v = it.template view<T>(idx);
	const auto* d = v.data();
	gaia::dont_optimize(d);
};
template <typename T>
void acc_view(ecs::Iter& it) {
	auto v = it.template view<T>();
	const auto* d = v.data();
	gaia::dont_optimize(d);
};

template <uint32_t NViews, bool ViewWithIndex>
void bench_query_each_view(picobench::state& state, ecs::World& w) {
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
		(void)_;

		state.stop_timer();
		q.each([&](ecs::Iter& it) {
			GAIA_EACH(it) {
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
void BM_BuildQuery(picobench::state& state) {
	ecs::World w;
	// Create some archetypes for a good measure
	create_archetypes(w, 1000, 10);

	ecs::Entity tmp[QueryComponents];
	GAIA_FOR(QueryComponents)
	tmp[i] = w.add();

	state.stop_timer();
	for (auto _: state) {
		(void)_;
		auto query = create_query<UseCachedQuery>(w, tmp);

		// Measure the time to build the query
		state.start_timer();
		(void)query.fetch();
		state.stop_timer();
	}
}

#define DEFINE_BUILD_QUERY(QueryComponents)                                                                            \
	void BM_BuildQuery_##QueryComponents(picobench::state& state) {                                                      \
		BM_BuildQuery<true, QueryComponents>(state);                                                                       \
	}

#define DEFINE_BUILD_QUERY_U(QueryComponents)                                                                          \
	void BM_BuildQuery_U_##QueryComponents(picobench::state& state) {                                                    \
		BM_BuildQuery<false, QueryComponents>(state);                                                                      \
	}

DEFINE_BUILD_QUERY(1)
DEFINE_BUILD_QUERY_U(1)
DEFINE_BUILD_QUERY(3)
DEFINE_BUILD_QUERY_U(3)
DEFINE_BUILD_QUERY(5)
DEFINE_BUILD_QUERY_U(5)
DEFINE_BUILD_QUERY(7)
DEFINE_BUILD_QUERY_U(7)

#define DEFINE_EACH(ArchetypeCount, MaxIdsPerArchetype, QueryComponents)                                               \
	void BM_Each_##ArchetypeCount##_##QueryComponents(picobench::state& state) {                                         \
		ecs::World w;                                                                                                      \
		auto types = create_archetypes(w, ArchetypeCount, MaxIdsPerArchetype);                                             \
		ecs::Entity tmp[QueryComponents];                                                                                  \
		prepare_query_types({types.data(), types.size()}, tmp);                                                            \
		auto query = create_query<true>(w, tmp);                                                                           \
		bench_query_each(state, query);                                                                                    \
	}

DEFINE_EACH(1, 1, 1)
DEFINE_EACH(100, 10, 1)
DEFINE_EACH(1000, 10, 1)

template <bool UseCachedQuery, uint32_t QueryComponents, typename IterKind>
void BM_Each_Iter(picobench::state& state, uint32_t ArchetypeCount, uint32_t MaxIdsPerArchetype) {
	ecs::World w;
	auto types = create_archetypes(w, ArchetypeCount, MaxIdsPerArchetype);
	ecs::Entity tmp[QueryComponents];
	prepare_query_types({types.data(), types.size()}, tmp);
	auto query = create_query<UseCachedQuery>(w, tmp);
	bench_query_each_iter<IterKind>(state, query);
}

template <uint32_t NViews, bool ViewWithIndex>
void BM_Each_View(picobench::state& state) {
	ecs::World w;

	// Create some archetypes for a good measure
	create_archetypes(w, 1000, 10);

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

#define DEFINE_EACH_ITER(IterKind, ArchetypeCount, MaxIdsPerArchetype, QueryComponents)                                \
	void BM_Each_##IterKind##_##ArchetypeCount##_##QueryComponents(picobench::state& state) {                            \
		BM_Each_Iter<true, QueryComponents, ecs::IterKind>(state, ArchetypeCount, MaxIdsPerArchetype);                     \
	}
#define DEFINE_EACH_U_ITER(IterKind, ArchetypeCount, MaxIdsPerArchetype, QueryComponents)                              \
	void BM_Each_U_##IterKind##_##ArchetypeCount##_##QueryComponents(picobench::state& state) {                          \
		BM_Each_Iter<false, QueryComponents, ecs::IterKind>(state, ArchetypeCount, MaxIdsPerArchetype);                    \
	}

#define DEFINE_EACH_VIEW(NViews, ViewWithIndex)                                                                        \
	void BM_Each_View_##NViews##_##ViewWithIndex(picobench::state& state) {                                              \
		BM_Each_View<NViews, ViewWithIndex>(state);                                                                        \
	}

DEFINE_EACH_ITER(IterAll, 1, 1, 1)
DEFINE_EACH_ITER(Iter, 1, 1, 1)
DEFINE_EACH_U_ITER(Iter, 1, 1, 1)

DEFINE_EACH_ITER(IterAll, 100, 10, 1)
DEFINE_EACH_ITER(Iter, 100, 10, 1)
DEFINE_EACH_U_ITER(Iter, 100, 10, 1)

DEFINE_EACH_ITER(IterAll, 1000, 10, 1);
DEFINE_EACH_ITER(Iter, 1000, 10, 1)
DEFINE_EACH_U_ITER(Iter, 1000, 10, 1)

DEFINE_EACH_ITER(Iter, 1000, 10, 3);
DEFINE_EACH_ITER(Iter, 1000, 10, 5);
DEFINE_EACH_ITER(Iter, 1000, 10, 7);

DEFINE_EACH_U_ITER(Iter, 1000, 10, 3);
DEFINE_EACH_U_ITER(Iter, 1000, 10, 5);
DEFINE_EACH_U_ITER(Iter, 1000, 10, 7);

DEFINE_EACH_VIEW(1, false);
DEFINE_EACH_VIEW(1, true);
DEFINE_EACH_VIEW(2, false);
DEFINE_EACH_VIEW(2, true);
DEFINE_EACH_VIEW(3, false);
DEFINE_EACH_VIEW(3, true);
DEFINE_EACH_VIEW(4, false);
DEFINE_EACH_VIEW(4, true);
DEFINE_EACH_VIEW(5, false);
DEFINE_EACH_VIEW(5, true);

#define PICO_SETTINGS() iterations({8192}).samples(3)
#define PICO_SETTINGS_1() iterations({8192}).samples(1)
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

		if (profilingMode) {
			PICOBENCH_SUITE_REG("1000 archetypes");
			PICOBENCH_REG(BM_Each_Iter_1000_7).PICO_SETTINGS().label("Iter, 7 comps");
		} else if (sanitizerMode) {
			PICOBENCH_REG(BM_Each_Iter_1000_7).PICO_SETTINGS_SANI().label("Iter, 7 comps");
			PICOBENCH_REG(BM_Each_U_Iter_1000_7).PICO_SETTINGS_SANI().label("(u) 7 comps"); // uncached
			r.run_benchmarks();
			return 0;
		} else {
			PICOBENCH_SUITE_REG("1 archetype");
			PICOBENCH_REG(BM_Each_1_1).PICO_SETTINGS().label("each, 1 comp");
			PICOBENCH_REG(BM_Each_IterAll_1_1).PICO_SETTINGS().label("IterAll, 1 comp");
			PICOBENCH_REG(BM_Each_Iter_1_1).PICO_SETTINGS().label("Iter, 1 comp");
			PICOBENCH_REG(BM_Each_U_Iter_1_1).PICO_SETTINGS().label("(u) Iter, 1 comp"); // uncached

			PICOBENCH_SUITE_REG("100 archetypes");
			PICOBENCH_REG(BM_Each_100_1).PICO_SETTINGS().label("each, 1 comp");
			PICOBENCH_REG(BM_Each_IterAll_100_1).PICO_SETTINGS().label("IterAll, 1 comp");
			PICOBENCH_REG(BM_Each_Iter_100_1).PICO_SETTINGS().label("Iter, 1 comp");
			PICOBENCH_REG(BM_Each_U_Iter_100_1).PICO_SETTINGS().label("(u) Iter, 1 comp"); // uncached

			PICOBENCH_SUITE_REG("1000 archetypes");
			PICOBENCH_REG(BM_Each_1000_1).PICO_SETTINGS().label("each, 1 comp");
			PICOBENCH_REG(BM_Each_IterAll_1000_1).PICO_SETTINGS().label("IterAll, 1 comp");
			PICOBENCH_REG(BM_Each_Iter_1000_1).PICO_SETTINGS().label("Iter, 1 comp");
			PICOBENCH_REG(BM_Each_U_Iter_1000_1).PICO_SETTINGS().label("(u) Iter, 1 comp"); // uncached

			PICOBENCH_SUITE_REG("1000 archetypes, Iter");
			PICOBENCH_REG(BM_Each_Iter_1000_3).PICO_SETTINGS().label("3 comps");
			PICOBENCH_REG(BM_Each_U_Iter_1000_3).PICO_SETTINGS().label("(u) 3 comps"); // uncached
			PICOBENCH_REG(BM_Each_Iter_1000_5).PICO_SETTINGS().label("5 comps");
			PICOBENCH_REG(BM_Each_U_Iter_1000_5).PICO_SETTINGS().label("(u) 5 comps"); // uncached
			PICOBENCH_REG(BM_Each_Iter_1000_7).PICO_SETTINGS().label("7 comps");
			PICOBENCH_REG(BM_Each_U_Iter_1000_7).PICO_SETTINGS().label("(u) 7 comps"); // uncached

			PICOBENCH_SUITE_REG("build query");
			PICOBENCH_REG(BM_BuildQuery_1).PICO_SETTINGS().label("1 comp");
			PICOBENCH_REG(BM_BuildQuery_U_1).PICO_SETTINGS().label("(u) 1 comp"); // uncached
			PICOBENCH_REG(BM_BuildQuery_3).PICO_SETTINGS().label(" 3 comps");
			PICOBENCH_REG(BM_BuildQuery_U_3).PICO_SETTINGS().label("(u) 3 comps"); // uncached
			PICOBENCH_REG(BM_BuildQuery_5).PICO_SETTINGS().label("5 comps");
			PICOBENCH_REG(BM_BuildQuery_U_5).PICO_SETTINGS().label("(u) 5 comps"); // uncached
			PICOBENCH_REG(BM_BuildQuery_7).PICO_SETTINGS().label("7 comps");
			PICOBENCH_REG(BM_BuildQuery_U_7).PICO_SETTINGS().label("(u) 7 comps"); // uncached

			PICOBENCH_SUITE_REG("Iter view");
			PICOBENCH_REG(BM_Each_View_1_false).PICO_SETTINGS().label("view 1 comp");
			PICOBENCH_REG(BM_Each_View_1_true).PICO_SETTINGS().label("view 1 comp, idx");
			PICOBENCH_REG(BM_Each_View_2_false).PICO_SETTINGS().label("view 2 comp");
			PICOBENCH_REG(BM_Each_View_2_true).PICO_SETTINGS().label("view 2 comp, idx");
			PICOBENCH_REG(BM_Each_View_3_false).PICO_SETTINGS().label("view 3 comp");
			PICOBENCH_REG(BM_Each_View_3_true).PICO_SETTINGS().label("view 3 comp, idx");
			PICOBENCH_REG(BM_Each_View_4_false).PICO_SETTINGS().label("view 4 comp");
			PICOBENCH_REG(BM_Each_View_4_true).PICO_SETTINGS().label("view 4 comp, idx");
			PICOBENCH_REG(BM_Each_View_5_false).PICO_SETTINGS().label("view 5 comp");
			PICOBENCH_REG(BM_Each_View_5_true).PICO_SETTINGS().label("view 5 comp, idx");
		}
	}

	return r.run(0);
}
