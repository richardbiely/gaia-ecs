#define PICOBENCH_IMPLEMENT
#include <gaia.h>
#include <picobench/picobench.hpp>

using namespace gaia;

constexpr uint32_t NEntities = 1'000;

void AddEntities(ecs::World& w, uint32_t n) {
	GAIA_FOR(n) {
		[[maybe_unused]] auto e = w.add();
		gaia::dont_optimize(e);
	}
}

void BM_CreateEntity(picobench::state& state) {
	for (auto _: state) {
		(void)_;
		ecs::World w;

		// Simulate the hot path. This happens when the component was
		// added at least once and thus the graph edges are already created.
		state.stop_timer();
		AddEntities(w, 1);
		state.start_timer();

		AddEntities(w, NEntities);
	}
}

template <uint32_t Version, typename T, uint32_t TCount>
struct Component {
	T value[TCount];
};

template <uint32_t Version, typename T>
struct Component<Version, T, 0> {}; // empty component

namespace detail {
	template <typename T, uint32_t ValuesCount, uint32_t ComponentCount>
	constexpr void Adds(ecs::World& w, ecs::Entity e) {
		core::each<ComponentCount>([&](auto i) {
			w.add<Component<i, T, ValuesCount>>(e);
		});
	}
} // namespace detail

template <typename T, uint32_t ValuesCount, uint32_t ComponentCount>
constexpr void Adds(ecs::World& w, uint32_t n) {
	GAIA_FOR(n) {
		[[maybe_unused]] auto e = w.add();
		::detail::Adds<T, ValuesCount, ComponentCount>(w, e);
	}
}

template <uint32_t Iterations>
void BM_CreateEntity_With_Component(picobench::state& state) {
	for (auto s: state) {
		(void)s;
		ecs::World w;

		// Simulate the hot path. This happens when the component was
		// added at least once and thus the graph edges are already created.
		state.stop_timer();
		Adds<float, 0, Iterations>(w, 1);
		state.start_timer();

		Adds<float, 0, Iterations>(w, NEntities);
	}
}

#define PICO_SETTINGS() iterations({1024}).samples(3)
#define PICO_SETTINGS_1() iterations({1024}).samples(1)
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
			PICOBENCH_REG(BM_CreateEntity_With_Component<32>).PICO_SETTINGS().label("32 components");
		} else {
			PICOBENCH_SUITE_REG("Entity and component creation");
			PICOBENCH_REG(BM_CreateEntity).PICO_SETTINGS().label("0 components");
			PICOBENCH_REG(BM_CreateEntity_With_Component<1>).PICO_SETTINGS().label("1 component");
			PICOBENCH_REG(BM_CreateEntity_With_Component<2>).PICO_SETTINGS().label("2 components");
			PICOBENCH_REG(BM_CreateEntity_With_Component<4>).PICO_SETTINGS().label("4 components");
			PICOBENCH_REG(BM_CreateEntity_With_Component<8>).PICO_SETTINGS().label("8 components");
			PICOBENCH_REG(BM_CreateEntity_With_Component<16>).PICO_SETTINGS().label("16 components");
			PICOBENCH_REG(BM_CreateEntity_With_Component<32>).PICO_SETTINGS().label("32 components");
		}
	}

	return r.run(0);
}
