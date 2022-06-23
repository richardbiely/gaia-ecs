#include <benchmark/benchmark.h>
#include <gaia.h>

using namespace gaia;

constexpr size_t NEntities = 1'000;

void BM_CreateEntity(benchmark::State& state) {
	for ([[maybe_unused]] auto _: state) {
		ecs::World w;
		for (size_t i = 0U; i < NEntities; ++i) {
			[[maybe_unused]] auto e = w.CreateEntity();
			benchmark::DoNotOptimize(e);
		}
	}
}

template <size_t Version, typename T, size_t TCount>
struct Component {
	T value[TCount];
};

template <size_t Version, typename T>
struct Component<Version, T, 0U> {}; // empty component

namespace detail {
	template <typename T, size_t TCount, size_t Iterations>
	constexpr void AddComponents(ecs::World& w, ecs::Entity e) {
		utils::for_each<Iterations>([&](auto i) {
			w.AddComponent<Component<i, T, TCount>>(e);
		});
	}
} // namespace detail

template <typename T, size_t TCount, size_t Iterations>
constexpr void AddComponents(ecs::World& w, uint32_t N) {
	for (uint32_t i = 0U; i < N; ++i) {
		[[maybe_unused]] auto e = w.CreateEntity();
		detail::AddComponents<T, TCount, Iterations>(w, e);
	}
}

template <typename T, size_t TCount, size_t Iterations>
void BM_CreateEntity_With_Component(benchmark::State& state) {
	for ([[maybe_unused]] auto _: state) {
		ecs::World w;
		AddComponents<T, TCount, Iterations>(w, NEntities);
	}
}

constexpr uint32_t ForEachN = 1'000;

void BM_ForEach_1_Archetype(benchmark::State& state) {
	ecs::World w;
	//-----------------------------------------
	AddComponents<float, 3, 1>(w, ForEachN);
	//-----------------------------------------

	using c1 = Component<0, float, 3>;
	auto query = ecs::EntityQuery().All<c1>();

	float f = 0.f;
	for ([[maybe_unused]] auto _: state) {
		w.ForEach(query, [&](const c1& p) {
			f += p.value[0];
		});
	}
	benchmark::DoNotOptimize(f);
}

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

	float f = 0.f;
	for ([[maybe_unused]] auto _: state) {
		w.ForEach(query, [&](const c1& p) {
			f += p.value[0];
		});
	}
	benchmark::DoNotOptimize(f);
}

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
	AddComponents<int64_t, 0, 25>(w, ForEachN);
	AddComponents<int64_t, 1, 25>(w, ForEachN);
	AddComponents<int64_t, 2, 25>(w, ForEachN);
	AddComponents<int64_t, 3, 25>(w, ForEachN);
	//-----------------------------------------
	AddComponents<uint64_t, 0, 25>(w, ForEachN);
	AddComponents<uint64_t, 1, 25>(w, ForEachN);
	AddComponents<uint64_t, 2, 25>(w, ForEachN);
	AddComponents<uint64_t, 3, 25>(w, ForEachN);
	//-----------------------------------------
	AddComponents<float, 0, 25>(w, ForEachN);
	AddComponents<float, 1, 25>(w, ForEachN);
	AddComponents<float, 2, 25>(w, ForEachN);
	AddComponents<float, 3, 25>(w, ForEachN);
	//-----------------------------------------

	using c1 = Component<0, float, 3>;
	auto query = ecs::EntityQuery().All<c1>();

	float f = 0.f;
	for ([[maybe_unused]] auto _: state) {
		w.ForEach(query, [&](const c1& p) {
			f += p.value[0];
		});
	}
	benchmark::DoNotOptimize(f);
}

void BM_ForEach_Internal_1_Archetype(benchmark::State& state) {
	ecs::World w;
	//-----------------------------------------
	AddComponents<float, 3, 1>(w, ForEachN);
	//-----------------------------------------

	using c1 = Component<0, float, 3>;

	float f = 0.f;
	for ([[maybe_unused]] auto _: state) {
		w.ForEach([&](const c1& p) {
			f += p.value[0];
		});
	}
	benchmark::DoNotOptimize(f);
}

void BM_ForEach_Internal_100_Archetypes(benchmark::State& state) {
	ecs::World w;
	//-----------------------------------------
	AddComponents<float, 0, 25>(w, ForEachN);
	AddComponents<float, 1, 25>(w, ForEachN);
	AddComponents<float, 2, 25>(w, ForEachN);
	AddComponents<float, 3, 25>(w, ForEachN);
	//-----------------------------------------

	using c1 = Component<0, float, 3>;

	float f = 0.f;
	for ([[maybe_unused]] auto _: state) {
		w.ForEach([&](const c1& p) {
			f += p.value[0];
		});
	}
	benchmark::DoNotOptimize(f);
}

void BM_ForEach_Internal_1000_Archetypes(benchmark::State& state) {
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
	AddComponents<int64_t, 0, 25>(w, ForEachN);
	AddComponents<int64_t, 1, 25>(w, ForEachN);
	AddComponents<int64_t, 2, 25>(w, ForEachN);
	AddComponents<int64_t, 3, 25>(w, ForEachN);
	//-----------------------------------------
	AddComponents<uint64_t, 0, 25>(w, ForEachN);
	AddComponents<uint64_t, 1, 25>(w, ForEachN);
	AddComponents<uint64_t, 2, 25>(w, ForEachN);
	AddComponents<uint64_t, 3, 25>(w, ForEachN);
	//-----------------------------------------
	AddComponents<float, 0, 25>(w, ForEachN);
	AddComponents<float, 1, 25>(w, ForEachN);
	AddComponents<float, 2, 25>(w, ForEachN);
	AddComponents<float, 3, 25>(w, ForEachN);
	//-----------------------------------------

	using c1 = Component<0, float, 3>;

	float f = 0.f;
	for ([[maybe_unused]] auto _: state) {
		w.ForEach([&](const c1& p) {
			f += p.value[0];
		});
	}
	benchmark::DoNotOptimize(f);
}

void BM_ForEach_Chunk_1_Archetype(benchmark::State& state) {
	ecs::World w;
	//-----------------------------------------
	AddComponents<float, 3, 1>(w, ForEachN);
	//-----------------------------------------

	using c1 = Component<0, float, 3>;
	auto query = ecs::EntityQuery().All<c1>();

	for ([[maybe_unused]] auto _: state) {
		float f = 0.f;
		w.ForEach(query, [&](const ecs::Chunk& chunk) {
			auto c1View = chunk.View<c1>();
			for (size_t i = 0; i < chunk.GetItemCount(); ++i)
				f += c1View[i].value[0];
		});
		benchmark::DoNotOptimize(f);
	}
}

void BM_ForEach_Chunk_100_Archetypes(benchmark::State& state) {
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
		float f = 0.f;
		w.ForEach(query, [&](const ecs::Chunk& chunk) {
			auto c1View = chunk.View<c1>();
			for (size_t i = 0; i < chunk.GetItemCount(); ++i)
				f += c1View[i].value[0];
		});
		benchmark::DoNotOptimize(f);
	}
}

void BM_ForEach_Chunk_1000_Archetypes(benchmark::State& state) {
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
	AddComponents<int64_t, 0, 25>(w, ForEachN);
	AddComponents<int64_t, 1, 25>(w, ForEachN);
	AddComponents<int64_t, 2, 25>(w, ForEachN);
	AddComponents<int64_t, 3, 25>(w, ForEachN);
	//-----------------------------------------
	AddComponents<uint64_t, 0, 25>(w, ForEachN);
	AddComponents<uint64_t, 1, 25>(w, ForEachN);
	AddComponents<uint64_t, 2, 25>(w, ForEachN);
	AddComponents<uint64_t, 3, 25>(w, ForEachN);
	//-----------------------------------------
	AddComponents<float, 0, 25>(w, ForEachN);
	AddComponents<float, 1, 25>(w, ForEachN);
	AddComponents<float, 2, 25>(w, ForEachN);
	AddComponents<float, 3, 25>(w, ForEachN);
	//-----------------------------------------

	using c1 = Component<0, float, 3>;
	auto query = ecs::EntityQuery().All<c1>();

	for ([[maybe_unused]] auto _: state) {
		float f = 0.f;
		w.ForEach(query, [&](const ecs::Chunk& chunk) {
			auto c1View = chunk.View<c1>();
			for (size_t i = 0; i < chunk.GetItemCount(); ++i)
				f += c1View[i].value[0];
		});
		benchmark::DoNotOptimize(f);
	}
}

void BM_ForEach_Chunk_Internal_1_Archetype(benchmark::State& state) {
	ecs::World w;
	//-----------------------------------------
	AddComponents<float, 3, 1>(w, ForEachN);
	//-----------------------------------------

	using c1 = Component<0, float, 3>;

	for ([[maybe_unused]] auto _: state) {
		float f = 0.f;
		w.ForEach(ecs::EntityQuery().All<c1>(), [&](const ecs::Chunk& chunk) {
			auto c1View = chunk.View<c1>();
			for (size_t i = 0; i < chunk.GetItemCount(); ++i)
				f += c1View[i].value[0];
		});
	}
}

void BM_ForEach_Chunk_Internal_100_Archetypes(benchmark::State& state) {
	ecs::World w;
	//-----------------------------------------
	AddComponents<float, 0, 25>(w, ForEachN);
	AddComponents<float, 1, 25>(w, ForEachN);
	AddComponents<float, 2, 25>(w, ForEachN);
	AddComponents<float, 3, 25>(w, ForEachN);
	//-----------------------------------------

	using c1 = Component<0, float, 3>;

	for ([[maybe_unused]] auto _: state) {
		float f = 0.f;
		w.ForEach(ecs::EntityQuery().All<c1>(), [&](const ecs::Chunk& chunk) {
			auto c1View = chunk.View<c1>();
			for (size_t i = 0; i < chunk.GetItemCount(); ++i)
				f += c1View[i].value[0];
		});
		benchmark::DoNotOptimize(f);
	}
}

void BM_ForEach_Chunk_Internal_1000_Archetypes(benchmark::State& state) {
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
	AddComponents<int64_t, 0, 25>(w, ForEachN);
	AddComponents<int64_t, 1, 25>(w, ForEachN);
	AddComponents<int64_t, 2, 25>(w, ForEachN);
	AddComponents<int64_t, 3, 25>(w, ForEachN);
	//-----------------------------------------
	AddComponents<uint64_t, 0, 25>(w, ForEachN);
	AddComponents<uint64_t, 1, 25>(w, ForEachN);
	AddComponents<uint64_t, 2, 25>(w, ForEachN);
	AddComponents<uint64_t, 3, 25>(w, ForEachN);
	//-----------------------------------------
	AddComponents<float, 0, 25>(w, ForEachN);
	AddComponents<float, 1, 25>(w, ForEachN);
	AddComponents<float, 2, 25>(w, ForEachN);
	AddComponents<float, 3, 25>(w, ForEachN);
	//-----------------------------------------

	using c1 = Component<0, float, 3>;

	for ([[maybe_unused]] auto _: state) {
		float f = 0.f;
		w.ForEach(ecs::EntityQuery().All<c1>(), [&](const ecs::Chunk& chunk) {
			auto c1View = chunk.View<c1>();
			for (size_t i = 0; i < chunk.GetItemCount(); ++i)
				f += c1View[i].value[0];
		});
		benchmark::DoNotOptimize(f);
	}
}

#define BENCHMARK_CREATEENTITY_WITH_COMPONENT______(component_count)                                                   \
	BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component, float, 0, component_count);                                       \
	BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component, float, 1, component_count);                                       \
	BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component, float, 2, component_count);                                       \
	BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component, float, 4, component_count);                                       \
	BENCHMARK_TEMPLATE(BM_CreateEntity_With_Component, float, 8, component_count)

// Empty entites
BENCHMARK(BM_CreateEntity);
// 1 component, size increases with each benchmark
BENCHMARK_CREATEENTITY_WITH_COMPONENT______(1);
// 2 components, size increases with each benchmark
BENCHMARK_CREATEENTITY_WITH_COMPONENT______(2);
// 3 components, size increases with each benchmark
BENCHMARK_CREATEENTITY_WITH_COMPONENT______(3);
// 4 components, size increases with each benchmark
BENCHMARK_CREATEENTITY_WITH_COMPONENT______(4);
// 8 components, size increases with each benchmark
BENCHMARK_CREATEENTITY_WITH_COMPONENT______(8);

// Iteration performance
BENCHMARK(BM_ForEach_1_Archetype);
BENCHMARK(BM_ForEach_100_Archetypes);
BENCHMARK(BM_ForEach_1000_Archetypes);
BENCHMARK(BM_ForEach_Internal_1_Archetype);
BENCHMARK(BM_ForEach_Internal_100_Archetypes);
BENCHMARK(BM_ForEach_Internal_1000_Archetypes);
BENCHMARK(BM_ForEach_Chunk_1_Archetype);
BENCHMARK(BM_ForEach_Chunk_100_Archetypes);
BENCHMARK(BM_ForEach_Chunk_1000_Archetypes);
BENCHMARK(BM_ForEach_Chunk_Internal_1_Archetype);
BENCHMARK(BM_ForEach_Chunk_Internal_100_Archetypes);
BENCHMARK(BM_ForEach_Chunk_Internal_1000_Archetypes);

// Run the benchmark
BENCHMARK_MAIN();
