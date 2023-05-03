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

#if GAIA_ARCHETYPE_GRAPH
		// Simulate the hot path. This happens when the component was
		// added at least once and thus the graph edges are already created.
		state.stop_timer();
		AddEntities(w, 1);
		state.start_timer();
#endif

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

constexpr size_t EntitiesForArchetype = 1'000;

void Create_Archetypes_1(ecs::World& w) {
	AddComponents<float, 3, 1>(w, EntitiesForArchetype);
}

void Create_Archetypes_100(ecs::World& w) {
	utils::for_each<4>([&](auto i) {
		AddComponents<float, i, 25>(w, EntitiesForArchetype);
	});
}

template <size_t N, size_t ComponentCount>
void Create_Archetypes_N(ecs::World& w) {
	utils::for_each<N>([&](auto i) {
		AddComponents<bool, i, ComponentCount>(w, EntitiesForArchetype);
	});
	//-----------------------------------------
	utils::for_each<N>([&](auto i) {
		AddComponents<int8_t, i, ComponentCount>(w, EntitiesForArchetype);
	});
	//-----------------------------------------
	utils::for_each<N>([&](auto i) {
		AddComponents<uint8_t, i, ComponentCount>(w, EntitiesForArchetype);
	});
	//-----------------------------------------
	utils::for_each<N>([&](auto i) {
		AddComponents<int16_t, i, ComponentCount>(w, EntitiesForArchetype);
	});
	//-----------------------------------------
	utils::for_each<N>([&](auto i) {
		AddComponents<uint16_t, i, ComponentCount>(w, EntitiesForArchetype);
	});
	//-----------------------------------------
	utils::for_each<N>([&](auto i) {
		AddComponents<int32_t, i, ComponentCount>(w, EntitiesForArchetype);
	});
	//-----------------------------------------
	utils::for_each<N>([&](auto i) {
		AddComponents<uint32_t, i, ComponentCount>(w, EntitiesForArchetype);
	});
	//-----------------------------------------
	utils::for_each<N>([&](auto i) {
		AddComponents<int64_t, i, ComponentCount>(w, EntitiesForArchetype);
	});
	//-----------------------------------------
	utils::for_each<N>([&](auto i) {
		AddComponents<uint64_t, i, ComponentCount>(w, EntitiesForArchetype);
	});
	//-----------------------------------------
	utils::for_each<N>([&](auto i) {
		AddComponents<float, i, ComponentCount>(w, EntitiesForArchetype);
	});
}

void Create_Archetypes_1000(ecs::World& w) {
	Create_Archetypes_N<4, 25>(w);
}

void Create_Archetypes_2500(ecs::World& w) {
	Create_Archetypes_N<10, 25>(w);
}

void Create_Archetypes_5000(ecs::World& w) {
	Create_Archetypes_N<20, 25>(w);
}

#define DEFINE_FOREACH_INTERNALQUERY(ArchetypeCount)                                                                   \
	void BM_ForEach_Internal_##ArchetypeCount(picobench::state& state) {                                                 \
		ecs::World w;                                                                                                      \
		Create_Archetypes_##ArchetypeCount(w);                                                                             \
                                                                                                                       \
		using c1 = Component<0, float, 3>;                                                                                 \
                                                                                                                       \
		float f = 0.f;                                                                                                     \
		for (auto _: state) {                                                                                              \
			(void)_;                                                                                                         \
			w.ForEach([&](const c1& p) {                                                                                     \
				f += p.value[0];                                                                                               \
			});                                                                                                              \
		}                                                                                                                  \
		picobench::DoNotOptimize(f);                                                                                       \
	}

DEFINE_FOREACH_INTERNALQUERY(1)
DEFINE_FOREACH_INTERNALQUERY(100)
DEFINE_FOREACH_INTERNALQUERY(1000)
DEFINE_FOREACH_INTERNALQUERY(2500)
DEFINE_FOREACH_INTERNALQUERY(5000)

#define DEFINE_FOREACH_EXTERNALQUERY(ArchetypeCount)                                                                   \
	void BM_ForEach_External_##ArchetypeCount(picobench::state& state) {                                                 \
		ecs::World w;                                                                                                      \
		Create_Archetypes_##ArchetypeCount(w);                                                                             \
                                                                                                                       \
		using c1 = Component<0, float, 3>;                                                                                 \
		auto query = w.CreateQuery().All<const c1>();                                                                      \
                                                                                                                       \
		for (auto _: state) {                                                                                              \
			(void)_;                                                                                                         \
			float f = 0.f;                                                                                                   \
			query.ForEach([&](const c1& p) {                                                                                 \
				f += p.value[0];                                                                                               \
			});                                                                                                              \
			picobench::DoNotOptimize(f);                                                                                     \
		}                                                                                                                  \
	}

DEFINE_FOREACH_EXTERNALQUERY(1)
DEFINE_FOREACH_EXTERNALQUERY(100)
DEFINE_FOREACH_EXTERNALQUERY(1000)
DEFINE_FOREACH_EXTERNALQUERY(2500)
DEFINE_FOREACH_EXTERNALQUERY(5000)

#define DEFINE_FOREACHCHUNK_EXTERNALQUERY(ArchetypeCount)                                                              \
	void BM_ForEachChunk_External_##ArchetypeCount(picobench::state& state) {                                            \
		ecs::World w;                                                                                                      \
		Create_Archetypes_##ArchetypeCount(w);                                                                             \
                                                                                                                       \
		using c1 = Component<0, float, 3>;                                                                                 \
		auto query = w.CreateQuery().All<const c1>();                                                                      \
                                                                                                                       \
		for (auto _: state) {                                                                                              \
			(void)_;                                                                                                         \
			float f = 0.f;                                                                                                   \
			query.ForEach([&](const ecs::Chunk& chunk) {                                                                     \
				auto c1View = chunk.View<c1>();                                                                                \
				for (size_t i = 0; i < chunk.GetItemCount(); ++i)                                                              \
					f += c1View[i].value[0];                                                                                     \
			});                                                                                                              \
			picobench::DoNotOptimize(f);                                                                                     \
		}                                                                                                                  \
	}

DEFINE_FOREACHCHUNK_EXTERNALQUERY(1)
DEFINE_FOREACHCHUNK_EXTERNALQUERY(100)
DEFINE_FOREACHCHUNK_EXTERNALQUERY(1000)
DEFINE_FOREACHCHUNK_EXTERNALQUERY(2500)
DEFINE_FOREACHCHUNK_EXTERNALQUERY(5000)

#define PICO_SETTINGS_CREATEENTITY() iterations({2048}).samples(3)
#define PICO_SETTINGS_FOREACH() iterations({8192}).samples(3)

PICOBENCH_SUITE("Entity and component creation");
PICOBENCH(BM_CreateEntity).PICO_SETTINGS_CREATEENTITY().label("0 components");
PICOBENCH(BM_CreateEntity_With_Component<1>).PICO_SETTINGS_CREATEENTITY().label("1 component");
PICOBENCH(BM_CreateEntity_With_Component<2>).PICO_SETTINGS_CREATEENTITY().label("2 components");
PICOBENCH(BM_CreateEntity_With_Component<4>).PICO_SETTINGS_CREATEENTITY().label("4 components");
PICOBENCH(BM_CreateEntity_With_Component<8>).PICO_SETTINGS_CREATEENTITY().label("8 components");
PICOBENCH(BM_CreateEntity_With_Component<16>).PICO_SETTINGS_CREATEENTITY().label("16 components");
PICOBENCH(BM_CreateEntity_With_Component<32>).PICO_SETTINGS_CREATEENTITY().label("32 components");

PICOBENCH_SUITE("ForEach - internal query");
PICOBENCH(BM_ForEach_Internal_1).PICO_SETTINGS_FOREACH().label("1 archetype");
PICOBENCH(BM_ForEach_Internal_100).PICO_SETTINGS_FOREACH().label("100 archetypes");
PICOBENCH(BM_ForEach_Internal_1000).PICO_SETTINGS_FOREACH().label("1000 archetypes");
PICOBENCH(BM_ForEach_Internal_2500).PICO_SETTINGS_FOREACH().label("2500 archetypes");
PICOBENCH(BM_ForEach_Internal_5000).PICO_SETTINGS_FOREACH().label("5000 archetypes");

PICOBENCH_SUITE("ForEach - external query");
PICOBENCH(BM_ForEach_External_1).PICO_SETTINGS_FOREACH().label("1 archetype");
PICOBENCH(BM_ForEach_External_100).PICO_SETTINGS_FOREACH().label("100 archetypes");
PICOBENCH(BM_ForEach_External_1000).PICO_SETTINGS_FOREACH().label("1000 archetypes");
PICOBENCH(BM_ForEach_External_2500).PICO_SETTINGS_FOREACH().label("2500 archetypes");
PICOBENCH(BM_ForEach_External_5000).PICO_SETTINGS_FOREACH().label("5000 archetypes");

PICOBENCH_SUITE("ForEach - external query, chunk");
PICOBENCH(BM_ForEachChunk_External_1).PICO_SETTINGS_FOREACH().label("1 archetype");
PICOBENCH(BM_ForEachChunk_External_100).PICO_SETTINGS_FOREACH().label("100 archetypes");
PICOBENCH(BM_ForEachChunk_External_1000).PICO_SETTINGS_FOREACH().label("1000 archetypes");
PICOBENCH(BM_ForEachChunk_External_2500).PICO_SETTINGS_FOREACH().label("2500 archetypes");
PICOBENCH(BM_ForEachChunk_External_5000).PICO_SETTINGS_FOREACH().label("5000 archetypes");
