#define PICOBENCH_IMPLEMENT_WITH_MAIN
#include "gaia/external/picobench.hpp"
#include <gaia.h>

using namespace gaia;

constexpr size_t NEntities = 1'000;

void BM_CreateEntity(picobench::state& state) {
	for (auto _: state) {
		(void)_;
		ecs::World w;
		for (size_t i = 0; i < NEntities; ++i) {
			[[maybe_unused]] auto e = w.CreateEntity();
			picobench::DoNotOptimize(e);
		}
	}
}

template <size_t Version, typename T, size_t TCount>
struct Component {
	T value[TCount];
};

template <size_t Version, typename T>
struct Component<Version, T, 0> {}; // empty component

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
	for (size_t i = 0; i < N; ++i) {
		[[maybe_unused]] auto e = w.CreateEntity();
		detail::AddComponents<T, TCount, Iterations>(w, e);
	}
}

template <size_t Iterations>
void BM_CreateEntity_With_Component(picobench::state& state) {
	for (auto s: state) {
		(void)s;
		ecs::World w;

#if GAIA_ARCHETYPE_GRAPH
		// Simulate the hot path. This happens when the component was
		// added at least once and thus the graph edges are already created.
		state.stop_timer();
		AddComponents<float, 0, Iterations>(w, 1);
		state.start_timer();
#endif

		AddComponents<float, 0, Iterations>(w, NEntities);
	}
}

constexpr size_t ForEachN = 1'000;

void BM_ForEach_1_Archetype(picobench::state& state) {
	ecs::World w;
	//-----------------------------------------
	AddComponents<float, 3, 1>(w, ForEachN);
	//-----------------------------------------

	using c1 = Component<0, float, 3>;
	auto query = ecs::EntityQuery().All<const c1>();

	float f = 0.f;
	for (auto _: state) {
		(void)_;
		w.ForEach(query, [&](const c1& p) {
			f += p.value[0];
		});
	}
	picobench::DoNotOptimize(f);
}

void BM_ForEach_100_Archetypes(picobench::state& state) {
	ecs::World w;
	//-----------------------------------------
	AddComponents<float, 0, 25>(w, ForEachN);
	AddComponents<float, 1, 25>(w, ForEachN);
	AddComponents<float, 2, 25>(w, ForEachN);
	AddComponents<float, 3, 25>(w, ForEachN);
	//-----------------------------------------

	using c1 = Component<0, float, 3>;
	auto query = ecs::EntityQuery().All<const c1>();

	float f = 0.f;
	for (auto _: state) {
		(void)_;
		w.ForEach(query, [&](const c1& p) {
			f += p.value[0];
		});
	}
	picobench::DoNotOptimize(f);
}

void BM_ForEach_1000_Archetypes(picobench::state& state) {
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
	auto query = ecs::EntityQuery().All<const c1>();

	float f = 0.f;
	for (auto _: state) {
		(void)_;
		w.ForEach(query, [&](const c1& p) {
			f += p.value[0];
		});
	}
	picobench::DoNotOptimize(f);
}

void BM_ForEach_Internal_1_Archetype(picobench::state& state) {
	ecs::World w;
	//-----------------------------------------
	AddComponents<float, 3, 1>(w, ForEachN);
	//-----------------------------------------

	using c1 = Component<0, float, 3>;

	float f = 0.f;
	for (auto _: state) {
		(void)_;
		w.ForEach([&](const c1& p) {
			f += p.value[0];
		});
	}
	picobench::DoNotOptimize(f);
}

void BM_ForEach_Internal_100_Archetypes(picobench::state& state) {
	ecs::World w;
	//-----------------------------------------
	AddComponents<float, 0, 25>(w, ForEachN);
	AddComponents<float, 1, 25>(w, ForEachN);
	AddComponents<float, 2, 25>(w, ForEachN);
	AddComponents<float, 3, 25>(w, ForEachN);
	//-----------------------------------------

	using c1 = Component<0, float, 3>;

	float f = 0.f;
	for (auto _: state) {
		(void)_;
		w.ForEach([&](const c1& p) {
			f += p.value[0];
		});
	}
	picobench::DoNotOptimize(f);
}

void BM_ForEach_Internal_1000_Archetypes(picobench::state& state) {
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
	for (auto _: state) {
		(void)_;
		w.ForEach([&](const c1& p) {
			f += p.value[0];
		});
	}
	picobench::DoNotOptimize(f);
}

void BM_ForEach_Chunk_1_Archetype(picobench::state& state) {
	ecs::World w;
	//-----------------------------------------
	AddComponents<float, 3, 1>(w, ForEachN);
	//-----------------------------------------

	using c1 = Component<0, float, 3>;
	auto query = ecs::EntityQuery().All<const c1>();

	for (auto _: state) {
		(void)_;
		float f = 0.f;
		w.ForEach(query, [&](const ecs::Chunk& chunk) {
			auto c1View = chunk.View<c1>();
			for (size_t i = 0; i < chunk.GetItemCount(); ++i)
				f += c1View[i].value[0];
		});
		picobench::DoNotOptimize(f);
	}
}

void BM_ForEach_Chunk_100_Archetypes(picobench::state& state) {
	ecs::World w;
	//-----------------------------------------
	AddComponents<float, 0, 25>(w, ForEachN);
	AddComponents<float, 1, 25>(w, ForEachN);
	AddComponents<float, 2, 25>(w, ForEachN);
	AddComponents<float, 3, 25>(w, ForEachN);
	//-----------------------------------------

	using c1 = Component<0, float, 3>;
	auto query = ecs::EntityQuery().All<const c1>();

	for (auto _: state) {
		(void)_;
		float f = 0.f;
		w.ForEach(query, [&](const ecs::Chunk& chunk) {
			auto c1View = chunk.View<c1>();
			for (size_t i = 0; i < chunk.GetItemCount(); ++i)
				f += c1View[i].value[0];
		});
		picobench::DoNotOptimize(f);
	}
}

void BM_ForEach_Chunk_1000_Archetypes(picobench::state& state) {
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
	auto query = ecs::EntityQuery().All<const c1>();

	for (auto _: state) {
		(void)_;
		float f = 0.f;
		w.ForEach(query, [&](const ecs::Chunk& chunk) {
			auto c1View = chunk.View<c1>();
			for (size_t i = 0; i < chunk.GetItemCount(); ++i)
				f += c1View[i].value[0];
		});
		picobench::DoNotOptimize(f);
	}
}

void BM_ForEach_Chunk_Internal_1_Archetype(picobench::state& state) {
	ecs::World w;
	//-----------------------------------------
	AddComponents<float, 3, 1>(w, ForEachN);
	//-----------------------------------------

	using c1 = Component<0, float, 3>;

	for (auto _: state) {
		(void)_;
		float f = 0.f;
		w.ForEach(ecs::EntityQuery().All<const c1>(), [&](const ecs::Chunk& chunk) {
			auto c1View = chunk.View<c1>();
			for (size_t i = 0; i < chunk.GetItemCount(); ++i)
				f += c1View[i].value[0];
		});
	}
}

void BM_ForEach_Chunk_Internal_100_Archetypes(picobench::state& state) {
	ecs::World w;
	//-----------------------------------------
	AddComponents<float, 0, 25>(w, ForEachN);
	AddComponents<float, 1, 25>(w, ForEachN);
	AddComponents<float, 2, 25>(w, ForEachN);
	AddComponents<float, 3, 25>(w, ForEachN);
	//-----------------------------------------

	using c1 = Component<0, float, 3>;

	for (auto _: state) {
		(void)_;
		float f = 0.f;
		w.ForEach(ecs::EntityQuery().All<const c1>(), [&](const ecs::Chunk& chunk) {
			auto c1View = chunk.View<c1>();
			for (size_t i = 0; i < chunk.GetItemCount(); ++i)
				f += c1View[i].value[0];
		});
		picobench::DoNotOptimize(f);
	}
}

void BM_ForEach_Chunk_Internal_1000_Archetypes(picobench::state& state) {
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

	for (auto _: state) {
		(void)_;
		float f = 0.f;
		w.ForEach(ecs::EntityQuery().All<const c1>(), [&](const ecs::Chunk& chunk) {
			auto c1View = chunk.View<c1>();
			for (size_t i = 0; i < chunk.GetItemCount(); ++i)
				f += c1View[i].value[0];
		});
		picobench::DoNotOptimize(f);
	}
}

#define PICO_SETTINGS() iterations({8192}).samples(3)

// PICOBENCH_SUITE("Entity and component creation");
// PICOBENCH(BM_CreateEntity).PICO_SETTINGS().label("0 components");
// PICOBENCH(BM_CreateEntity_With_Component<1>).PICO_SETTINGS().label("1 component");
// PICOBENCH(BM_CreateEntity_With_Component<2>).PICO_SETTINGS().label("2 components");
// PICOBENCH(BM_CreateEntity_With_Component<4>).PICO_SETTINGS().label("4 components");
// PICOBENCH(BM_CreateEntity_With_Component<8>).PICO_SETTINGS().label("8 components");
// PICOBENCH(BM_CreateEntity_With_Component<16>).PICO_SETTINGS().label("16 components");
// PICOBENCH(BM_CreateEntity_With_Component<32>).PICO_SETTINGS().label("32 components");

PICOBENCH_SUITE("ForEach - external query");
PICOBENCH(BM_ForEach_1_Archetype).PICO_SETTINGS().label("1 archetype");
// PICOBENCH(BM_ForEach_100_Archetypes).PICO_SETTINGS().label("100 archetypes");
// PICOBENCH(BM_ForEach_1000_Archetypes).PICO_SETTINGS().label("1000 archetypes");

// PICOBENCH_SUITE("ForEach - external query, chunk");
// PICOBENCH(BM_ForEach_Chunk_1_Archetype).PICO_SETTINGS().label("1 archetype");
// PICOBENCH(BM_ForEach_Chunk_100_Archetypes).PICO_SETTINGS().label("100 archetypes");
// PICOBENCH(BM_ForEach_Chunk_1000_Archetypes).PICO_SETTINGS().label("1000 archetypes");

// PICOBENCH_SUITE("ForEach - internal query)");
// PICOBENCH(BM_ForEach_Internal_1_Archetype).PICO_SETTINGS().label("1 archetype");
// PICOBENCH(BM_ForEach_Internal_100_Archetypes).PICO_SETTINGS().label("100 archetypes");
// PICOBENCH(BM_ForEach_Internal_1000_Archetypes).PICO_SETTINGS().label("1000 archetypes");

// PICOBENCH_SUITE("ForEach - internal query, chunk");
// PICOBENCH(BM_ForEach_Chunk_Internal_1_Archetype).PICO_SETTINGS().label("1 archetype");
// PICOBENCH(BM_ForEach_Chunk_Internal_100_Archetypes).PICO_SETTINGS().label("100 archetypes");
// PICOBENCH(BM_ForEach_Chunk_Internal_1000_Archetypes).PICO_SETTINGS().label("1000 archetypes");
