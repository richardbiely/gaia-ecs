#define PICOBENCH_IMPLEMENT
#include <gaia.h>
#include <picobench/picobench.hpp>

using namespace gaia;

constexpr uint32_t NEntities = 100; // 1'000;
constexpr uint32_t NEntitiesMany = 1000; // 1'000'000;

template <uint32_t Version, typename T, uint32_t TCount>
struct Component {
	T value[TCount];
};

template <uint32_t Version, typename T>
struct Component<Version, T, 0> {}; // empty component

namespace detail {

	template <typename T, uint32_t ValuesCount, uint32_t ComponentCount, bool BulkCreate>
	void Adds(ecs::World& w, ecs::Entity e) {
		if constexpr (BulkCreate) {
			auto builder = w.build(e);
			core::each<ComponentCount>([&builder](auto i) {
				builder.add<Component<i, T, ValuesCount>>();
			});
		} else {
			core::each<ComponentCount>([&](auto i) {
				w.add<Component<i, T, ValuesCount>>(e);
			});
		}
	}
} // namespace detail

template <typename T, uint32_t ValuesCount, uint32_t ComponentCount, bool BulkCreate>
constexpr ecs::Entity AddsOne(ecs::World& w) {
	auto e = w.add();
	::detail::Adds<T, ValuesCount, ComponentCount, BulkCreate>(w, e);
	return e;
}

template <typename T, uint32_t ValuesCount, uint32_t ComponentCount, bool BulkCreate>
constexpr void Adds(ecs::World& w, uint32_t n) {
	GAIA_FOR(n) {
		(void)AddsOne<T, ValuesCount, ComponentCount, BulkCreate>(w);
	}
}

template <uint32_t NumberOfEntities>
void BM_CreateEntity(picobench::state& state) {
	for (auto _: state) {
		(void)_;
		state.stop_timer();
		ecs::World w;

		// Simulate the hot path. This happens when the component was
		// added at least once and thus the graph edges are already created.
		(void)w.add();
		state.start_timer();

		GAIA_FOR(NumberOfEntities)(void) w.add();

		state.stop_timer();
	}
}

template <uint32_t NumberOfEntities>
void BM_CreateEntity_Many(picobench::state& state) {
	for (auto _: state) {
		(void)_;
		state.stop_timer();
		ecs::World w;

		// Simulate the hot path. This happens when the component was
		// added at least once and thus the graph edges are already created.
		(void)w.add();
		state.start_timer();

		w.add_n(NumberOfEntities);

		state.stop_timer();
	}
}

template <uint32_t Iterations>
void BM_CreateEntity_Many_With_Component(picobench::state& state) {
	for (auto s: state) {
		(void)s;
		state.stop_timer();
		ecs::World w;

		// Simulate the hot path. This happens when the component was
		// added at least once and thus the graph edges are already created.
		state.start_timer();
		auto e = AddsOne<float, 0, Iterations, false>(w);
		dont_optimize(e);

		state.stop_timer();
	}
}

template <uint32_t NumberOfEntities>
void BM_DeleteEntity(picobench::state& state) {
	cnt::darray<ecs::Entity> ents;
	ents.reserve(NumberOfEntities);

	for (auto _: state) {
		(void)_;
		state.stop_timer();
		ecs::World w;

		// Simulate the hot path. This happens when the component was
		// added at least once and thus the graph edges are already created.
		(void)w.add();
		GAIA_FOR(NumberOfEntities) ents.push_back(w.add());
		state.start_timer();

		for (auto e: ents)
			w.del(e);
		w.update();

		state.stop_timer();
		ents.clear();
	}
}

template <uint32_t Iterations>
void BM_CreateEntity_CopyMany(picobench::state& state) {
	for (auto s: state) {
		(void)s;
		state.stop_timer();
		ecs::World w;

		// Simulate the hot path. This happens when the component was
		// added at least once and thus the graph edges are already created.
		auto e = w.add();
		state.start_timer();

		w.copy_n(e, NEntities);

		state.stop_timer();
	}
}

template <uint32_t Iterations>
void BM_CreateEntity_CopyMany_With_Component(picobench::state& state) {
	for (auto s: state) {
		(void)s;
		state.stop_timer();
		ecs::World w;

		// Simulate the hot path. This happens when the component was
		// added at least once and thus the graph edges are already created.
		auto e = AddsOne<float, 0, Iterations, false>(w);
		state.start_timer();

		w.copy_n(e, NEntities);

		state.stop_timer();
	}
}

template <uint32_t Iterations>
void BM_CreateEntity_With_Component(picobench::state& state) {
	for (auto s: state) {
		(void)s;
		state.stop_timer();
		ecs::World w;

		// Simulate the hot path. This happens when the component was
		// added at least once and thus the graph edges are already created.
		Adds<float, 0, Iterations, false>(w, 1);
		state.start_timer();

		Adds<float, 0, Iterations, false>(w, NEntities);

		state.stop_timer();
	}
}

template <uint32_t Iterations>
void BM_BulkCreateEntity_With_Component(picobench::state& state) {
	for (auto s: state) {
		(void)s;
		state.stop_timer();
		ecs::World w;

		// Simulate the hot path. This happens when the component was
		// added at least once and thus the graph edges are already created.
		state.stop_timer();
		Adds<float, 0, Iterations, true>(w, 1);
		state.start_timer();

		Adds<float, 0, Iterations, true>(w, NEntities);

		state.stop_timer();
	}
}

#define PICO_SETTINGS() iterations({1024}).samples(3)
#define PICO_SETTINGS_1() iterations({16}).samples(1)
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
			PICOBENCH_REG(BM_CreateEntity_With_Component<30>).PICO_SETTINGS().label("30 components");
			r.run_benchmarks();
			return 0;
		} else if (sanitizerMode) {
			PICOBENCH_REG(BM_CreateEntity<NEntities>).PICO_SETTINGS_SANI().label("0 components");
			PICOBENCH_REG(BM_CreateEntity_Many_With_Component<30>).PICO_SETTINGS_SANI().label("30 components");
			PICOBENCH_REG(BM_CreateEntity_CopyMany_With_Component<30>).PICO_SETTINGS_SANI().label("30 components");
			PICOBENCH_REG(BM_CreateEntity_With_Component<30>).PICO_SETTINGS_SANI().label("30 components");
			PICOBENCH_REG(BM_BulkCreateEntity_With_Component<30>).PICO_SETTINGS_SANI().label("30 components");
			r.run_benchmarks();
			return 0;
		} else {
			PICOBENCH_SUITE_REG("Entity creation");
			PICOBENCH_REG(BM_CreateEntity<NEntities>).PICO_SETTINGS().label("0 components");
			PICOBENCH_REG(BM_CreateEntity<NEntitiesMany>).PICO_SETTINGS_1().label("0 components, 1M entities");

			PICOBENCH_SUITE_REG("Entity deletion");
			PICOBENCH_REG(BM_DeleteEntity<NEntities>).PICO_SETTINGS().label("0 components");
			PICOBENCH_REG(BM_DeleteEntity<NEntitiesMany>).PICO_SETTINGS_1().label("0 components, 1M entities");

			PICOBENCH_SUITE_REG("Entity + N components - add_n");
			PICOBENCH_REG(BM_CreateEntity_Many<NEntities>).PICO_SETTINGS().label("0 component");
			PICOBENCH_REG(BM_CreateEntity_Many<NEntitiesMany>).PICO_SETTINGS_1().label("0 component, 1M entities");
			PICOBENCH_REG(BM_CreateEntity_Many_With_Component<1>).PICO_SETTINGS().label("1 component");
			PICOBENCH_REG(BM_CreateEntity_Many_With_Component<2>).PICO_SETTINGS().label("2 components");
			PICOBENCH_REG(BM_CreateEntity_Many_With_Component<4>).PICO_SETTINGS().label("4 components");
			PICOBENCH_REG(BM_CreateEntity_Many_With_Component<8>).PICO_SETTINGS().label("8 components");
			PICOBENCH_REG(BM_CreateEntity_Many_With_Component<16>).PICO_SETTINGS().label("16 components");
			PICOBENCH_REG(BM_CreateEntity_Many_With_Component<30>).PICO_SETTINGS().label("30 components");

			PICOBENCH_SUITE_REG("Entity + N components - copy_n");
			PICOBENCH_REG(BM_CreateEntity_CopyMany_With_Component<1>).PICO_SETTINGS().label("1 component");
			PICOBENCH_REG(BM_CreateEntity_CopyMany_With_Component<2>).PICO_SETTINGS().label("2 components");
			PICOBENCH_REG(BM_CreateEntity_CopyMany_With_Component<4>).PICO_SETTINGS().label("4 components");
			PICOBENCH_REG(BM_CreateEntity_CopyMany_With_Component<8>).PICO_SETTINGS().label("8 components");
			PICOBENCH_REG(BM_CreateEntity_CopyMany_With_Component<16>).PICO_SETTINGS().label("16 components");
			PICOBENCH_REG(BM_CreateEntity_CopyMany_With_Component<30>).PICO_SETTINGS().label("30 components");

			PICOBENCH_SUITE_REG("Entity + N component - add() + one-by-one components");
			PICOBENCH_REG(BM_CreateEntity_With_Component<1>).PICO_SETTINGS().label("1 component");
			PICOBENCH_REG(BM_CreateEntity_With_Component<2>).PICO_SETTINGS().label("2 components");
			PICOBENCH_REG(BM_CreateEntity_With_Component<4>).PICO_SETTINGS().label("4 components");
			PICOBENCH_REG(BM_CreateEntity_With_Component<8>).PICO_SETTINGS().label("8 components");
			PICOBENCH_REG(BM_CreateEntity_With_Component<16>).PICO_SETTINGS().label("16 components");
			PICOBENCH_REG(BM_CreateEntity_With_Component<30>).PICO_SETTINGS().label("30 components");

			PICOBENCH_SUITE_REG("Entity + N component - add() + bulk components");
			PICOBENCH_REG(BM_BulkCreateEntity_With_Component<1>).PICO_SETTINGS().label("1 component");
			PICOBENCH_REG(BM_BulkCreateEntity_With_Component<2>).PICO_SETTINGS().label("2 components");
			PICOBENCH_REG(BM_BulkCreateEntity_With_Component<4>).PICO_SETTINGS().label("4 components");
			PICOBENCH_REG(BM_BulkCreateEntity_With_Component<8>).PICO_SETTINGS().label("8 components");
			PICOBENCH_REG(BM_BulkCreateEntity_With_Component<16>).PICO_SETTINGS().label("16 components");
			PICOBENCH_REG(BM_BulkCreateEntity_With_Component<30>).PICO_SETTINGS().label("30 components");
		}
	}

	return r.run(0);
}
