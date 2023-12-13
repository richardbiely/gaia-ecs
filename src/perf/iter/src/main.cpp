#define PICOBENCH_IMPLEMENT
#include <gaia.h>
#include <picobench/picobench.hpp>

using namespace gaia;

constexpr uint32_t EntitiesPerArchetype = 1'000;

using TBenchmarkTypes = cnt::darray<ecs::Entity>;

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

auto create_query(ecs::World& w, ecs::EntitySpan queryTypes) {
	GAIA_ASSERT(!queryTypes.empty());

	auto query = w.query();
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

template <typename Iterator>
void bench_query_each_iter(picobench::state& state, ecs::Query& query) {
	/* We want to benchmark the hot-path. In real-world scenarios queries are cached so cache them now */
	gaia::dont_optimize(query.empty());

	for (auto _: state) {
		(void)_;
		uint32_t cnt = 0;
		query.each([&](Iterator iter) {
			GAIA_EACH(iter) {
				++cnt;
			}
		});
		gaia::dont_optimize(cnt);
	}
}

#define DEFINE_EACH(ArchetypeCount, MaxIdsPerArchetype, QueryComponents)                                               \
	void BM_Each_##ArchetypeCount##_##QueryComponents(picobench::state& state) {                                         \
		ecs::World w;                                                                                                      \
		auto types = create_archetypes(w, ArchetypeCount, MaxIdsPerArchetype);                                             \
		ecs::Entity tmp[QueryComponents];                                                                                  \
		prepare_query_types({types.data(), types.size()}, tmp);                                                            \
		auto query = create_query(w, tmp);                                                                                 \
		bench_query_each(state, query);                                                                                    \
	}

DEFINE_EACH(1, 1, 1)
DEFINE_EACH(100, 10, 1)
DEFINE_EACH(1000, 10, 1)

#define DEFINE_EACH_ITER(IterKind, ArchetypeCount, MaxIdsPerArchetype, QueryComponents)                                \
	void BM_Each_##IterKind##_##ArchetypeCount##_##QueryComponents(picobench::state& state) {                            \
		srand(0U);                                                                                                         \
		ecs::World w;                                                                                                      \
		auto types = create_archetypes(w, ArchetypeCount, MaxIdsPerArchetype);                                             \
		ecs::Entity tmp[QueryComponents];                                                                                  \
		prepare_query_types({types.data(), types.size()}, tmp);                                                            \
		auto query = create_query(w, tmp);                                                                                 \
		bench_query_each_iter<ecs::IterKind>(state, query);                                                                \
	}

DEFINE_EACH_ITER(Iterator, 1, 1, 1)
DEFINE_EACH_ITER(Iterator, 100, 10, 1)
DEFINE_EACH_ITER(Iterator, 1000, 10, 1)
DEFINE_EACH_ITER(IteratorAll, 1000, 10, 1);

DEFINE_EACH_ITER(Iterator, 1000, 10, 3);
DEFINE_EACH_ITER(Iterator, 1000, 10, 5);
DEFINE_EACH_ITER(Iterator, 1000, 10, 7);

#define PICO_SETTINGS() iterations({8192}).samples(3)
#define PICO_SETTINGS_1() iterations({8192}).samples(1)
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

		const gaia::cnt::darray<std::string_view> args(argv + 1, argv + argc);
		for (const auto& arg: args) {
			if (arg == "-p") {
				profilingMode = true;
				continue;
			}
		}

		GAIA_LOG_N("Profiling mode = %s", profilingMode ? "ON" : "OFF");

		if (profilingMode) {
			PICOBENCH_REG(BM_Each_IteratorAll_1000_1).PICO_SETTINGS_1().label("1000 archetypes (All)");
		} else {
			PICOBENCH_SUITE_REG("each");
			PICOBENCH_REG(BM_Each_1_1).PICO_SETTINGS().label("1 archetype");
			PICOBENCH_REG(BM_Each_100_1).PICO_SETTINGS().label("100 archetypes");
			PICOBENCH_REG(BM_Each_1000_1).PICO_SETTINGS().label("1000 archetypes");

			PICOBENCH_SUITE_REG("each - iterator");
			PICOBENCH_REG(BM_Each_Iterator_1_1).PICO_SETTINGS().label("1 archetype");
			PICOBENCH_REG(BM_Each_Iterator_100_1).PICO_SETTINGS().label("100 archetypes");
			PICOBENCH_REG(BM_Each_Iterator_1000_1).PICO_SETTINGS().label("1000 archetypes");
			PICOBENCH_REG(BM_Each_IteratorAll_1000_1).PICO_SETTINGS().label("1000 archetypes (All)");

			PICOBENCH_SUITE_REG("1000 archetypes, query all");
			PICOBENCH_REG(BM_Each_Iterator_1000_3).PICO_SETTINGS().label("1000 archetypes, 3 comps");
			PICOBENCH_REG(BM_Each_Iterator_1000_5).PICO_SETTINGS().label("1000 archetypes, 5 comps");
			PICOBENCH_REG(BM_Each_Iterator_1000_7).PICO_SETTINGS().label("1000 archetypes, 7 comps");
		}
	}

	return r.run(0);
}
