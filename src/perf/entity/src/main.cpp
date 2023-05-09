#define PICOBENCH_IMPLEMENT_WITH_MAIN
#include "gaia/external/picobench.hpp"
#include <gaia.h>

using namespace gaia;

constexpr size_t NEntities = 1'000;

void AddEntities(ecs::World& w, uint32_t n) {
	for (size_t i = 0; i < n; ++i) {
		[[maybe_unused]] auto e = w.CreateEntity();
		picobench::DoNotOptimize(e);
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

template <size_t Version, typename T, size_t TCount>
struct Component {
	T value[TCount];
};

template <size_t Version, typename T>
struct Component<Version, T, 0> {}; // empty component

namespace detail {
	template <typename T, size_t ValuesCount, size_t ComponentCount>
	constexpr void AddComponents(ecs::World& w, ecs::Entity e) {
		utils::for_each<ComponentCount>([&](auto i) {
			w.AddComponent<Component<i, T, ValuesCount>>(e);
		});
	}
} // namespace detail

template <typename T, size_t ValuesCount, size_t ComponentCount>
constexpr void AddComponents(ecs::World& w, uint32_t n) {
	for (size_t i = 0; i < n; ++i) {
		[[maybe_unused]] auto e = w.CreateEntity();
		detail::AddComponents<T, ValuesCount, ComponentCount>(w, e);
	}
}

template <size_t Iterations>
void BM_CreateEntity_With_Component(picobench::state& state) {
	for (auto s: state) {
		(void)s;
		ecs::World w;

		// Simulate the hot path. This happens when the component was
		// added at least once and thus the graph edges are already created.
		state.stop_timer();
		AddComponents<float, 0, Iterations>(w, 1);
		state.start_timer();

		AddComponents<float, 0, Iterations>(w, NEntities);
	}
}

#define PICO_SETTINGS() iterations({2048}).samples(3)

PICOBENCH_SUITE("Entity and component creation");
PICOBENCH(BM_CreateEntity).PICO_SETTINGS().label("0 components");
PICOBENCH(BM_CreateEntity_With_Component<1>).PICO_SETTINGS().label("1 component");
PICOBENCH(BM_CreateEntity_With_Component<2>).PICO_SETTINGS().label("2 components");
PICOBENCH(BM_CreateEntity_With_Component<4>).PICO_SETTINGS().label("4 components");
PICOBENCH(BM_CreateEntity_With_Component<8>).PICO_SETTINGS().label("8 components");
PICOBENCH(BM_CreateEntity_With_Component<16>).PICO_SETTINGS().label("16 components");
PICOBENCH(BM_CreateEntity_With_Component<32>).PICO_SETTINGS().label("32 components");
