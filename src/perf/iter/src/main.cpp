#define PICOBENCH_IMPLEMENT_WITH_MAIN
#include "gaia/external/picobench.hpp"
#include <gaia.h>

using namespace gaia;

constexpr uint32_t EntitiesPerArchetype = 1'000;

template <uint32_t Version, typename T, uint32_t TCount>
struct Component {
	T value[TCount];
};

template <uint32_t Version, typename T>
struct Component<Version, T, 0> {}; // empty component

namespace detail {
	template <typename T, uint32_t ValuesCount, uint32_t ComponentCount>
	constexpr void Adds(ecs::World& w, ecs::Entity e) {
#if 0
		(void)w;
		(void)e;
#else
		core::each<ComponentCount>([&](auto i) {
			w.Add<Component<i, T, ValuesCount>>(e);
		});
#endif
	}
} // namespace detail

template <typename T, uint32_t ValuesCount, uint32_t ComponentCount>
constexpr void Adds(ecs::World& w, uint32_t n) {
	for (uint32_t i = 0; i < n; ++i) {
		[[maybe_unused]] auto e = w.Add();
		::detail::Adds<T, ValuesCount, ComponentCount>(w, e);
	}
}

void Create_Archetypes_1(ecs::World& w) {
	Adds<float, 3, 1>(w, EntitiesPerArchetype);
}

void Create_Archetypes_100(ecs::World& w) {
	core::each<4>([&](auto i) {
		Adds<float, i, 25>(w, EntitiesPerArchetype);
	});
}

template <uint32_t N, uint32_t ComponentCount>
void Create_Archetypes_N(ecs::World& w) {
	core::each<N>([&](auto i) {
		Adds<bool, i, ComponentCount>(w, EntitiesPerArchetype);
	});
	//-----------------------------------------
	core::each<N>([&](auto i) {
		Adds<int8_t, i, ComponentCount>(w, EntitiesPerArchetype);
	});
	//-----------------------------------------
	core::each<N>([&](auto i) {
		Adds<uint8_t, i, ComponentCount>(w, EntitiesPerArchetype);
	});
	//-----------------------------------------
	core::each<N>([&](auto i) {
		Adds<int16_t, i, ComponentCount>(w, EntitiesPerArchetype);
	});
	//-----------------------------------------
	core::each<N>([&](auto i) {
		Adds<uint16_t, i, ComponentCount>(w, EntitiesPerArchetype);
	});
	//-----------------------------------------
	core::each<N>([&](auto i) {
		Adds<int32_t, i, ComponentCount>(w, EntitiesPerArchetype);
	});
	//-----------------------------------------
	core::each<N>([&](auto i) {
		Adds<uint32_t, i, ComponentCount>(w, EntitiesPerArchetype);
	});
	//-----------------------------------------
	core::each<N>([&](auto i) {
		Adds<int64_t, i, ComponentCount>(w, EntitiesPerArchetype);
	});
	//-----------------------------------------
	core::each<N>([&](auto i) {
		Adds<uint64_t, i, ComponentCount>(w, EntitiesPerArchetype);
	});
	//-----------------------------------------
	core::each<N>([&](auto i) {
		Adds<float, i, ComponentCount>(w, EntitiesPerArchetype);
	});
}

void Create_Archetypes_1000(ecs::World& w) {
	Create_Archetypes_N<4, 25>(w);
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
			gaia::dont_optimize(f);                                                                                          \
		}                                                                                                                  \
	}

DEFINE_FOREACH_INTERNALQUERY(1)
DEFINE_FOREACH_INTERNALQUERY(100)
DEFINE_FOREACH_INTERNALQUERY(1000)

#define DEFINE_FOREACH_EXTERNALQUERY(ArchetypeCount)                                                                   \
	void BM_ForEach_External_##ArchetypeCount(picobench::state& state) {                                                 \
		ecs::World w;                                                                                                      \
		Create_Archetypes_##ArchetypeCount(w);                                                                             \
                                                                                                                       \
		using c1 = Component<0, float, 3>;                                                                                 \
		auto query = w.CreateQuery().All<const c1>();                                                                      \
                                                                                                                       \
		/* We want to benchmark the hot-path. In real-world scenarios queries are cached so cache them now */              \
		gaia::dont_optimize(query.HasEntities());                                                                          \
                                                                                                                       \
		for (auto _: state) {                                                                                              \
			(void)_;                                                                                                         \
			float f = 0.f;                                                                                                   \
			query.ForEach([&](const c1& p) {                                                                                 \
				f += p.value[0];                                                                                               \
			});                                                                                                              \
			gaia::dont_optimize(f);                                                                                          \
		}                                                                                                                  \
	}

DEFINE_FOREACH_EXTERNALQUERY(1)
DEFINE_FOREACH_EXTERNALQUERY(100)
DEFINE_FOREACH_EXTERNALQUERY(1000)

#define DEFINE_FOREACHCHUNK_EXTERNALQUERY_ITER(ArchetypeCount)                                                         \
	void BM_ForEachChunk_External_Iter_##ArchetypeCount(picobench::state& state) {                                       \
		ecs::World w;                                                                                                      \
		Create_Archetypes_##ArchetypeCount(w);                                                                             \
                                                                                                                       \
		using c1 = Component<0, float, 3>;                                                                                 \
		auto query = w.CreateQuery().All<const c1>();                                                                      \
                                                                                                                       \
		/* We want to benchmark the hot-path. In real-world scenarios queries are cached so cache them now */              \
		gaia::dont_optimize(query.HasEntities());                                                                          \
                                                                                                                       \
		for (auto _: state) {                                                                                              \
			(void)_;                                                                                                         \
			float f = 0.f;                                                                                                   \
			query.ForEach([&](ecs::Iterator iter) {                                                                          \
				auto c1View = iter.View<c1>();                                                                                 \
				iter.each([&](uint32_t i) {                                                                                    \
					f += c1View[i].value[0];                                                                                     \
				});                                                                                                            \
			});                                                                                                              \
			gaia::dont_optimize(f);                                                                                          \
		}                                                                                                                  \
	}

DEFINE_FOREACHCHUNK_EXTERNALQUERY_ITER(1)
DEFINE_FOREACHCHUNK_EXTERNALQUERY_ITER(100)
DEFINE_FOREACHCHUNK_EXTERNALQUERY_ITER(1000)

#define DEFINE_FOREACHCHUNK_EXTERNALQUERY_INDEX(ArchetypeCount)                                                        \
	void BM_ForEachChunk_External_Index_##ArchetypeCount(picobench::state& state) {                                      \
		ecs::World w;                                                                                                      \
		Create_Archetypes_##ArchetypeCount(w);                                                                             \
                                                                                                                       \
		using c1 = Component<0, float, 3>;                                                                                 \
		auto query = w.CreateQuery().All<const c1>();                                                                      \
                                                                                                                       \
		/* We want to benchmark the hot-path. In real-world scenarios queries are cached so cache them now */              \
		gaia::dont_optimize(query.HasEntities());                                                                          \
                                                                                                                       \
		for (auto _: state) {                                                                                              \
			(void)_;                                                                                                         \
			float f = 0.f;                                                                                                   \
			query.ForEach([&](ecs::Iterator iter) {                                                                          \
				auto c1View = iter.View<c1>();                                                                                 \
				for (uint32_t i = 0; i < iter.size(); ++i)                                                                     \
					f += c1View[i].value[0];                                                                                     \
			});                                                                                                              \
			gaia::dont_optimize(f);                                                                                          \
		}                                                                                                                  \
	}

DEFINE_FOREACHCHUNK_EXTERNALQUERY_INDEX(1000);

#define DEFINE_FOREACHCHUNK_EXTERNALQUERY_ALL(ArchetypeCount)                                                          \
	void BM_ForEachChunk_External_All_##ArchetypeCount(picobench::state& state) {                                        \
		ecs::World w;                                                                                                      \
		Create_Archetypes_##ArchetypeCount(w);                                                                             \
                                                                                                                       \
		using c1 = Component<0, float, 3>;                                                                                 \
		auto query = w.CreateQuery().All<const c1>();                                                                      \
                                                                                                                       \
		/* We want to benchmark the hot-path. In real-world scenarios queries are cached so cache them now */              \
		gaia::dont_optimize(query.HasEntities());                                                                          \
                                                                                                                       \
		for (auto _: state) {                                                                                              \
			(void)_;                                                                                                         \
			float f = 0.f;                                                                                                   \
			query.ForEach([&](ecs::IteratorAll iter) {                                                                       \
				auto c1View = iter.View<c1>();                                                                                 \
				iter.each([&](uint32_t i) {                                                                                    \
					f += c1View[i].value[0];                                                                                     \
				});                                                                                                            \
			});                                                                                                              \
			gaia::dont_optimize(f);                                                                                          \
		}                                                                                                                  \
	}

DEFINE_FOREACHCHUNK_EXTERNALQUERY_ALL(1000);

#define PICO_SETTINGS() iterations({8192}).samples(3)

PICOBENCH_SUITE("ForEach - internal query");
PICOBENCH(BM_ForEach_Internal_1).PICO_SETTINGS().label("1 archetype");
PICOBENCH(BM_ForEach_Internal_100).PICO_SETTINGS().label("100 archetypes");
PICOBENCH(BM_ForEach_Internal_1000).PICO_SETTINGS().label("1000 archetypes");

PICOBENCH_SUITE("ForEach - external query");
PICOBENCH(BM_ForEach_External_1).PICO_SETTINGS().label("1 archetype");
PICOBENCH(BM_ForEach_External_100).PICO_SETTINGS().label("100 archetypes");
PICOBENCH(BM_ForEach_External_1000).PICO_SETTINGS().label("1000 archetypes");

PICOBENCH_SUITE("ForEach - external query, iterator");
PICOBENCH(BM_ForEachChunk_External_Iter_1).PICO_SETTINGS().label("1 archetype");
PICOBENCH(BM_ForEachChunk_External_Iter_100).PICO_SETTINGS().label("100 archetypes");
PICOBENCH(BM_ForEachChunk_External_Iter_1000).PICO_SETTINGS().label("1000 archetypes");
PICOBENCH(BM_ForEachChunk_External_Index_1000).PICO_SETTINGS().label("1000 archetypes (Idx)");
PICOBENCH(BM_ForEachChunk_External_All_1000).PICO_SETTINGS().label("1000 archetypes (All)");
