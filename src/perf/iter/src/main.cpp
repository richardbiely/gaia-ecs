#define PICOBENCH_IMPLEMENT_WITH_MAIN
#include "gaia/external/picobench.hpp"
#include <gaia.h>

using namespace gaia;

constexpr size_t EntitiesPerArchetype = 1'000;

template <size_t Version, typename T, size_t TCount>
struct Component {
	T value[TCount];
};

template <size_t Version, typename T>
struct Component<Version, T, 0> {}; // empty component

namespace detail {
	template <typename T, size_t ValuesCount, size_t ComponentCount>
	constexpr void AddComponents(ecs::World& w, ecs::Entity e) {
		(void)w;
		(void)e;
		// utils::for_each<ComponentCount>([&](auto i) {
		// 	w.AddComponent<Component<i, T, ValuesCount>>(e);
		// });
	}
} // namespace detail

template <typename T, size_t ValuesCount, size_t ComponentCount>
constexpr void AddComponents(ecs::World& w, uint32_t n) {
	for (size_t i = 0; i < n; ++i) {
		[[maybe_unused]] auto e = w.CreateEntity();
		detail::AddComponents<T, ValuesCount, ComponentCount>(w, e);
	}
}

void Create_Archetypes_1(ecs::World& w) {
	AddComponents<float, 3, 1>(w, EntitiesPerArchetype);
}

void Create_Archetypes_100(ecs::World& w) {
	utils::for_each<4>([&](auto i) {
		AddComponents<float, i, 25>(w, EntitiesPerArchetype);
	});
}

template <size_t N, size_t ComponentCount>
void Create_Archetypes_N(ecs::World& w) {
	utils::for_each<N>([&](auto i) {
		AddComponents<bool, i, ComponentCount>(w, EntitiesPerArchetype);
	});
	//-----------------------------------------
	utils::for_each<N>([&](auto i) {
		AddComponents<int8_t, i, ComponentCount>(w, EntitiesPerArchetype);
	});
	//-----------------------------------------
	utils::for_each<N>([&](auto i) {
		AddComponents<uint8_t, i, ComponentCount>(w, EntitiesPerArchetype);
	});
	//-----------------------------------------
	utils::for_each<N>([&](auto i) {
		AddComponents<int16_t, i, ComponentCount>(w, EntitiesPerArchetype);
	});
	//-----------------------------------------
	utils::for_each<N>([&](auto i) {
		AddComponents<uint16_t, i, ComponentCount>(w, EntitiesPerArchetype);
	});
	//-----------------------------------------
	utils::for_each<N>([&](auto i) {
		AddComponents<int32_t, i, ComponentCount>(w, EntitiesPerArchetype);
	});
	//-----------------------------------------
	utils::for_each<N>([&](auto i) {
		AddComponents<uint32_t, i, ComponentCount>(w, EntitiesPerArchetype);
	});
	//-----------------------------------------
	utils::for_each<N>([&](auto i) {
		AddComponents<int64_t, i, ComponentCount>(w, EntitiesPerArchetype);
	});
	//-----------------------------------------
	utils::for_each<N>([&](auto i) {
		AddComponents<uint64_t, i, ComponentCount>(w, EntitiesPerArchetype);
	});
	//-----------------------------------------
	utils::for_each<N>([&](auto i) {
		AddComponents<float, i, ComponentCount>(w, EntitiesPerArchetype);
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
		for (auto _: state) {                                                                                              \
			(void)_;                                                                                                         \
			float f = 0.f;                                                                                                   \
			w.ForEach([&](const c1& p) {                                                                                     \
				f += p.value[0];                                                                                               \
			});                                                                                                              \
			picobench::DoNotOptimize(f);                                                                                     \
		}                                                                                                                  \
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
		/* We want to benchmark the hot-path. In real-world scenarios queries are cached so cache them now */              \
		picobench::DoNotOptimize(query.HasEntities());                                                                     \
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

#define DEFINE_FOREACHCHUNK_EXTERNALQUERY_ITER(ArchetypeCount)                                                         \
	void BM_ForEachChunk_External_Iter_##ArchetypeCount(picobench::state& state) {                                       \
		ecs::World w;                                                                                                      \
		Create_Archetypes_##ArchetypeCount(w);                                                                             \
                                                                                                                       \
		using c1 = Component<0, float, 3>;                                                                                 \
		auto query = w.CreateQuery().All<const c1>();                                                                      \
                                                                                                                       \
		/* We want to benchmark the hot-path. In real-world scenarios queries are cached so cache them now */              \
		picobench::DoNotOptimize(query.HasEntities());                                                                     \
                                                                                                                       \
		for (auto _: state) {                                                                                              \
			(void)_;                                                                                                         \
			float f = 0.f;                                                                                                   \
			query.ForEach([&](ecs::Iterator iter) {                                                                          \
				auto c1View = iter.View<c1>();                                                                                 \
				for (const auto i: iter)                                                                                       \
					f += c1View[i].value[0];                                                                                     \
			});                                                                                                              \
			picobench::DoNotOptimize(f);                                                                                     \
		}                                                                                                                  \
	}

DEFINE_FOREACHCHUNK_EXTERNALQUERY_ITER(1)
DEFINE_FOREACHCHUNK_EXTERNALQUERY_ITER(100)
DEFINE_FOREACHCHUNK_EXTERNALQUERY_ITER(1000)
DEFINE_FOREACHCHUNK_EXTERNALQUERY_ITER(2500)
DEFINE_FOREACHCHUNK_EXTERNALQUERY_ITER(5000)

#define DEFINE_FOREACHCHUNK_EXTERNALQUERY_INDEX(ArchetypeCount)                                                        \
	void BM_ForEachChunk_External_Index_##ArchetypeCount(picobench::state& state) {                                      \
		ecs::World w;                                                                                                      \
		Create_Archetypes_##ArchetypeCount(w);                                                                             \
                                                                                                                       \
		using c1 = Component<0, float, 3>;                                                                                 \
		auto query = w.CreateQuery().All<const c1>();                                                                      \
                                                                                                                       \
		/* We want to benchmark the hot-path. In real-world scenarios queries are cached so cache them now */              \
		picobench::DoNotOptimize(query.HasEntities());                                                                     \
                                                                                                                       \
		for (auto _: state) {                                                                                              \
			(void)_;                                                                                                         \
			float f = 0.f;                                                                                                   \
			query.ForEach([&](ecs::Iterator iter) {                                                                          \
				auto c1View = iter.View<c1>();                                                                                 \
				for (uint32_t i = 0; i < iter.size(); ++i)                                                                     \
					f += c1View[i].value[0];                                                                                     \
			});                                                                                                              \
			picobench::DoNotOptimize(f);                                                                                     \
		}                                                                                                                  \
	}

DEFINE_FOREACHCHUNK_EXTERNALQUERY_INDEX(5000);

#define DEFINE_FOREACHCHUNK_EXTERNALQUERY_ENABLED(ArchetypeCount)                                                      \
	void BM_ForEachChunk_External_Enabled_##ArchetypeCount(picobench::state& state) {                                    \
		ecs::World w;                                                                                                      \
		Create_Archetypes_##ArchetypeCount(w);                                                                             \
                                                                                                                       \
		using c1 = Component<0, float, 3>;                                                                                 \
		auto query = w.CreateQuery().All<const c1>();                                                                      \
                                                                                                                       \
		/* We want to benchmark the hot-path. In real-world scenarios queries are cached so cache them now */              \
		picobench::DoNotOptimize(query.HasEntities());                                                                     \
                                                                                                                       \
		for (auto _: state) {                                                                                              \
			(void)_;                                                                                                         \
			float f = 0.f;                                                                                                   \
			query.ForEach([&](ecs::IteratorEnabled iter) {                                                                   \
				auto c1View = iter.View<c1>();                                                                                 \
				for (auto i: iter)                                                                                             \
					f += c1View[i].value[0];                                                                                     \
			});                                                                                                              \
			picobench::DoNotOptimize(f);                                                                                     \
		}                                                                                                                  \
	}

DEFINE_FOREACHCHUNK_EXTERNALQUERY_ENABLED(5000);

#define PICO_SETTINGS() iterations({8192}).samples(3)

PICOBENCH_SUITE("ForEach - internal query");
PICOBENCH(BM_ForEach_Internal_1).PICO_SETTINGS().label("1 archetype");
PICOBENCH(BM_ForEach_Internal_100).PICO_SETTINGS().label("100 archetypes");
PICOBENCH(BM_ForEach_Internal_1000).PICO_SETTINGS().label("1000 archetypes");
PICOBENCH(BM_ForEach_Internal_2500).PICO_SETTINGS().label("2500 archetypes");
PICOBENCH(BM_ForEach_Internal_5000).PICO_SETTINGS().label("5000 archetypes");

PICOBENCH_SUITE("ForEach - external query");
PICOBENCH(BM_ForEach_External_1).PICO_SETTINGS().label("1 archetype");
PICOBENCH(BM_ForEach_External_100).PICO_SETTINGS().label("100 archetypes");
PICOBENCH(BM_ForEach_External_1000).PICO_SETTINGS().label("1000 archetypes");
PICOBENCH(BM_ForEach_External_2500).PICO_SETTINGS().label("2500 archetypes");
PICOBENCH(BM_ForEach_External_5000).PICO_SETTINGS().label("5000 archetypes");

PICOBENCH_SUITE("ForEach - external query, iterator");
PICOBENCH(BM_ForEachChunk_External_Iter_1).PICO_SETTINGS().label("1 archetype");
PICOBENCH(BM_ForEachChunk_External_Iter_100).PICO_SETTINGS().label("100 archetypes");
PICOBENCH(BM_ForEachChunk_External_Iter_1000).PICO_SETTINGS().label("1000 archetypes");
PICOBENCH(BM_ForEachChunk_External_Iter_2500).PICO_SETTINGS().label("2500 archetypes");
PICOBENCH(BM_ForEachChunk_External_Iter_5000).PICO_SETTINGS().label("5000 archetypes (Itr)");
PICOBENCH(BM_ForEachChunk_External_Index_5000).PICO_SETTINGS().label("5000 archetypes (Idx)");
PICOBENCH(BM_ForEachChunk_External_Enabled_5000).PICO_SETTINGS().label("5000 archetypes (Enb)");
