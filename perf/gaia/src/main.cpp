#define _ITERATOR_DEBUG_LEVEL 0
#include <gaia.h>

GAIA_INIT

#include <benchmark/benchmark.h>
#if GAIA_COMPILER_MSVC
	#ifdef _WIN32
		#pragma comment(lib, "Shlwapi.lib")
		#ifdef _DEBUG
			#pragma comment(lib, "benchmarkd.lib")
		#else
			#pragma comment(lib, "benchmark.lib")
		#endif
	#endif
#endif

using namespace gaia;

void BM_CreateEntity(benchmark::State& state) {
	constexpr uint32_t N = 1'000'000;
	for ([[maybe_unused]] auto _: state) {
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

template <uint32_t version, typename T>
struct Component<version, T, 0U> {}; // empty component

template <typename T, uint32_t ComponentItems, uint32_t Components>
void BM_CreateEntity_With_Component______(benchmark::State& state) {
	constexpr uint32_t N = 1'000'000;
	for ([[maybe_unused]] auto _: state) {
		ecs::World w;
		for (uint32_t i = 0; i < N; ++i) {
			[[maybe_unused]] auto e = w.CreateEntity();
			utils::for_each<Components>(
					[&](auto i) { w.AddComponent<Component<i, T, ComponentItems>>(e); });
		}
	}
}

namespace detail {
	template <typename T, uint32_t ComponentItems, auto... Is>
	constexpr void
	AddComponents(ecs::World& w, ecs::Entity e, std::index_sequence<Is...>) {
		w.AddComponent(
				e, Component<
							 std::integral_constant<decltype(Is), Is>{}, T,
							 ComponentItems>{}...);
	}
} // namespace detail

template <typename T, uint32_t ComponentItems, uint32_t Components>
constexpr void AddComponents(ecs::World& w, uint32_t N) {
	for (uint32_t i = 0; i < N; ++i) {
		[[maybe_unused]] auto e = w.CreateEntity();
		detail::AddComponents<T, ComponentItems>(
				w, e, std::make_index_sequence<uint32_t(Components)>());
	}
}

template <typename T, uint32_t ComponentItems, uint32_t Components>
void BM_CreateEntity_With_Component_Batch(benchmark::State& state) {
	constexpr uint32_t N = 1'000'000;
	for (auto _: state) {
		ecs::World w;
		AddComponents<T, ComponentItems, Components>(w, N);
	}
}

#define BENCHMARK_CREATEENTITY_WITH_COMPONENT______(component_count)           \
	BENCHMARK_TEMPLATE(                                                          \
			BM_CreateEntity_With_Component______, float, 0, component_count);        \
	BENCHMARK_TEMPLATE(                                                          \
			BM_CreateEntity_With_Component______, float, 1, component_count);        \
	BENCHMARK_TEMPLATE(                                                          \
			BM_CreateEntity_With_Component______, float, 2, component_count);        \
	BENCHMARK_TEMPLATE(                                                          \
			BM_CreateEntity_With_Component______, float, 4, component_count);        \
	BENCHMARK_TEMPLATE(                                                          \
			BM_CreateEntity_With_Component______, float, 8, component_count);
#define BENCHMARK_CREATEENTITY_WITH_COMPONENT_BATCH(component_count)           \
	BENCHMARK_TEMPLATE(                                                          \
			BM_CreateEntity_With_Component_Batch, float, 0, component_count);        \
	BENCHMARK_TEMPLATE(                                                          \
			BM_CreateEntity_With_Component_Batch, float, 1, component_count);        \
	BENCHMARK_TEMPLATE(                                                          \
			BM_CreateEntity_With_Component_Batch, float, 2, component_count);        \
	BENCHMARK_TEMPLATE(                                                          \
			BM_CreateEntity_With_Component_Batch, float, 4, component_count);        \
	BENCHMARK_TEMPLATE(                                                          \
			BM_CreateEntity_With_Component_Batch, float, 8, component_count);

// // 1 component, size increases with each benchmark
// BENCHMARK_CREATEENTITY_WITH_COMPONENT______(1);
// // 2 components, size increases with each benchmark
// BENCHMARK_CREATEENTITY_WITH_COMPONENT______(2);
// BENCHMARK_CREATEENTITY_WITH_COMPONENT_BATCH(2);
// // 3 components, size increases with each benchmark
// BENCHMARK_CREATEENTITY_WITH_COMPONENT______(3);
// BENCHMARK_CREATEENTITY_WITH_COMPONENT_BATCH(3);
// // 4 components, size increases with each benchmark
// BENCHMARK_CREATEENTITY_WITH_COMPONENT______(4);
// BENCHMARK_CREATEENTITY_WITH_COMPONENT_BATCH(4);
// // 8 components, size increases with each benchmark
// BENCHMARK_CREATEENTITY_WITH_COMPONENT______(8);
// BENCHMARK_CREATEENTITY_WITH_COMPONENT_BATCH(8);

constexpr uint32_t ForEachN = 1'000;

void BM_ForEach_1_Archetype(benchmark::State& state) {
	ecs::World w;
	//-----------------------------------------
	AddComponents<float, 3, 1>(w, ForEachN);
	//-----------------------------------------

	using c1 = Component<0, float, 3>;
	auto query = ecs::EntityQuery().All<c1>();

	for ([[maybe_unused]] auto _: state) {
		w.ForEach(query, [&](const c1& p) { benchmark::DoNotOptimize(p); }).Run(0);
	}
}
BENCHMARK(BM_ForEach_1_Archetype);

void BM_ForEach_100_Archetypes(benchmark::State& state) {
	ecs::World w;
	//-----------------------------------------
	AddComponents<float, 0, 25>(w, ForEachN);
	AddComponents<float, 1, 25>(w, ForEachN);
	AddComponents<float, 2, 25>(w, ForEachN);
	AddComponents<float, 3, 25>(w, ForEachN);
	//-----------------------------------------

	using c1 = Component<0, float, 3>;
	auto query = ecs::EntityQuery().All<c1>();

	for ([[maybe_unused]] auto _: state) {
		w.ForEach(query, [&](const c1& p) { benchmark::DoNotOptimize(p); }).Run(0);
	}
}
BENCHMARK(BM_ForEach_100_Archetypes);

void BM_ForEach_1000_Archetypes(benchmark::State& state) {
	ecs::World w;
	//-----------------------------------------
	AddComponents<bool, 0, 25>(w, ForEachN);
	AddComponents<bool, 1, 25>(w, ForEachN);
	AddComponents<bool, 2, 25>(w, ForEachN);
	AddComponents<bool, 3, 25>(w, ForEachN);
	//-----------------------------------------
	AddComponents<int8_t, 0, 25>(w, ForEachN);
	AddComponents<int8_t, 1, 25>(w, ForEachN);
	AddComponents<int8_t, 2, 25>(w, ForEachN);
	AddComponents<int8_t, 3, 25>(w, ForEachN);
	//-----------------------------------------
	AddComponents<uint8_t, 0, 25>(w, ForEachN);
	AddComponents<uint8_t, 1, 25>(w, ForEachN);
	AddComponents<uint8_t, 2, 25>(w, ForEachN);
	AddComponents<uint8_t, 3, 25>(w, ForEachN);
	//-----------------------------------------
	AddComponents<int16_t, 0, 25>(w, ForEachN);
	AddComponents<int16_t, 1, 25>(w, ForEachN);
	AddComponents<int16_t, 2, 25>(w, ForEachN);
	AddComponents<int16_t, 3, 25>(w, ForEachN);
	//-----------------------------------------
	AddComponents<uint16_t, 0, 25>(w, ForEachN);
	AddComponents<uint16_t, 1, 25>(w, ForEachN);
	AddComponents<uint16_t, 2, 25>(w, ForEachN);
	AddComponents<uint16_t, 3, 25>(w, ForEachN);
	//-----------------------------------------
	AddComponents<int32_t, 0, 25>(w, ForEachN);
	AddComponents<int32_t, 1, 25>(w, ForEachN);
	AddComponents<int32_t, 2, 25>(w, ForEachN);
	AddComponents<int32_t, 3, 25>(w, ForEachN);
	//-----------------------------------------
	AddComponents<uint32_t, 0, 25>(w, ForEachN);
	AddComponents<uint32_t, 1, 25>(w, ForEachN);
	AddComponents<uint32_t, 2, 25>(w, ForEachN);
	AddComponents<uint32_t, 3, 25>(w, ForEachN);
	//-----------------------------------------
	AddComponents<float, 0, 25>(w, ForEachN);
	AddComponents<float, 1, 25>(w, ForEachN);
	AddComponents<float, 2, 25>(w, ForEachN);
	AddComponents<float, 3, 25>(w, ForEachN);
	//-----------------------------------------

	using c1 = Component<0, float, 3>;
	auto query = ecs::EntityQuery().All<c1>();

	for ([[maybe_unused]] auto _: state) {
		w.ForEach(query, [&](const c1& p) { benchmark::DoNotOptimize(p); }).Run(0);
	}
}
BENCHMARK(BM_ForEach_1000_Archetypes);

// Run the benchmark
BENCHMARK_MAIN();
