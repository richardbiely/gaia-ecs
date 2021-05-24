#define _ITERATOR_DEBUG_LEVEL 0
#include <gaia.h>

GAIA_INIT

#include <benchmark/benchmark.h>

using namespace gaia;

void BM_CreateEntity(benchmark::State& state) {
	constexpr uint32_t N = 1'000'000;
		for (auto _: state) {
			ecs::World w;
				for (uint32_t i = 0; i < N; ++i) {
					[[maybe_unused]] auto e = w.CreateEntity();
				}
		}
}
BENCHMARK(BM_CreateEntity);

template <uint32_t version, typename T, uint32_t ComponentItems>
struct Component {
	T value[ComponentItems];
};

template <typename T, uint32_t ComponentItems, uint32_t Components>
void BM_CreateEntity_With_Component______(benchmark::State& state) {
	constexpr uint32_t N = 1'000'000;
		for (auto _: state) {
			ecs::World w;
				for (uint32_t i = 0; i < N; ++i) {
					[[maybe_unused]] auto e = w.CreateEntity();
					utils::for_each<Components>([&](auto i) {
						w.AddComponent<Component<i, T, ComponentItems>>(e);
					});
				}
		}
}

template <typename T, uint32_t ComponentItems, auto... Is>
constexpr void
AddComponents(ecs::World& w, ecs::Entity e, std::index_sequence<Is...>) {
	w.AddComponent(
			e,
			Component<
					std::integral_constant<decltype(Is), Is>{}, T, ComponentItems>{}...);
}

template <typename T, uint32_t ComponentItems, uint32_t Components>
void BM_CreateEntity_With_Component_Batch(benchmark::State& state) {
	constexpr uint32_t N = 1'000'000;
		for (auto _: state) {
			ecs::World w;
				for (uint32_t i = 0; i < N; ++i) {
					[[maybe_unused]] auto e = w.CreateEntity();
					AddComponents<T, ComponentItems>(
							w, e, std::make_index_sequence<uint32_t(Components)>());
				}
		}
}

// 1 component, size increases with each benchmark
BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component______, float, 1, 1);
BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component______, float, 2, 1);
BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component______, float, 4, 1);

// 2 components, size increases with each benchmark
BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component______, float, 1, 2);
BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component_Batch, float, 1, 2);
BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component______, float, 2, 2);
BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component_Batch, float, 2, 2);
BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component______, float, 4, 2);
BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component_Batch, float, 4, 2);
BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component______, float, 8, 2);
BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component_Batch, float, 8, 2);

// 3 components, size increases with each benchmark
BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component______, float, 1, 3);
BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component_Batch, float, 1, 3);
BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component______, float, 2, 3);
BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component_Batch, float, 2, 3);
BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component______, float, 4, 3);
BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component_Batch, float, 4, 3);
BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component______, float, 8, 3);
BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component_Batch, float, 8, 3);

// 4 components, size increases with each benchmark
BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component______, float, 1, 4);
BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component_Batch, float, 1, 4);
BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component______, float, 2, 4);
BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component_Batch, float, 2, 4);
BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component______, float, 4, 4);
BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component_Batch, float, 4, 4);
BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component______, float, 8, 4);
BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component_Batch, float, 8, 4);

// 8 components, size increases with each benchmark
BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component______, float, 1, 8);
BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component_Batch, float, 1, 8);
BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component______, float, 2, 8);
BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component_Batch, float, 2, 8);
BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component______, float, 4, 8);
BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component_Batch, float, 4, 8);
BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component______, float, 8, 8);
BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component_Batch, float, 8, 8);

// Run the benchmark
BENCHMARK_MAIN();
