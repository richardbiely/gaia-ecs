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
			w.add<Component<i, T, ValuesCount>>(e);
		});
#endif
	}
} // namespace detail

template <typename T, uint32_t ValuesCount, uint32_t ComponentCount>
constexpr void Adds(ecs::World& w, uint32_t n) {
	for (uint32_t i = 0; i < n; ++i) {
		[[maybe_unused]] auto e = w.add();
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

#define DEFINE_EACH(ArchetypeCount)                                                                                    \
	void BM_Each_##ArchetypeCount(picobench::state& state) {                                                             \
		ecs::World w;                                                                                                      \
		Create_Archetypes_##ArchetypeCount(w);                                                                             \
                                                                                                                       \
		using c1 = Component<0, float, 3>;                                                                                 \
		auto query = w.query().all<const c1>();                                                                            \
                                                                                                                       \
		/* We want to benchmark the hot-path. In real-world scenarios queries are cached so cache them now */              \
		gaia::dont_optimize(query.empty());                                                                                \
                                                                                                                       \
		for (auto _: state) {                                                                                              \
			(void)_;                                                                                                         \
			float f = 0.f;                                                                                                   \
			query.each([&](const c1& p) {                                                                                    \
				f += p.value[0];                                                                                               \
			});                                                                                                              \
			gaia::dont_optimize(f);                                                                                          \
		}                                                                                                                  \
	}

DEFINE_EACH(1)
DEFINE_EACH(100)
DEFINE_EACH(1000)

#define DEFINE_EACH_ITER(ArchetypeCount)                                                                               \
	void BM_Each_Iter_##ArchetypeCount(picobench::state& state) {                                                        \
		ecs::World w;                                                                                                      \
		Create_Archetypes_##ArchetypeCount(w);                                                                             \
                                                                                                                       \
		using c1 = Component<0, float, 3>;                                                                                 \
		auto query = w.query().all<const c1>();                                                                            \
                                                                                                                       \
		/* We want to benchmark the hot-path. In real-world scenarios queries are cached so cache them now */              \
		gaia::dont_optimize(query.empty());                                                                                \
                                                                                                                       \
		for (auto _: state) {                                                                                              \
			(void)_;                                                                                                         \
			float f = 0.f;                                                                                                   \
			query.each([&](ecs::Iterator iter) {                                                                             \
				auto c1View = iter.view<c1>();                                                                                 \
				GAIA_EACH(iter) f += c1View[i].value[0];                                                                       \
			});                                                                                                              \
			gaia::dont_optimize(f);                                                                                          \
		}                                                                                                                  \
	}

DEFINE_EACH_ITER(1)
DEFINE_EACH_ITER(100)
DEFINE_EACH_ITER(1000)

#define DEFINE_EACH_ITER_INDEX(ArchetypeCount)                                                                         \
	void BM_Each_Iter_Index_##ArchetypeCount(picobench::state& state) {                                                  \
		ecs::World w;                                                                                                      \
		Create_Archetypes_##ArchetypeCount(w);                                                                             \
                                                                                                                       \
		using c1 = Component<0, float, 3>;                                                                                 \
		auto query = w.query().all<const c1>();                                                                            \
                                                                                                                       \
		/* We want to benchmark the hot-path. In real-world scenarios queries are cached so cache them now */              \
		gaia::dont_optimize(query.empty());                                                                                \
                                                                                                                       \
		for (auto _: state) {                                                                                              \
			(void)_;                                                                                                         \
			float f = 0.f;                                                                                                   \
			query.each([&](ecs::Iterator iter) {                                                                             \
				auto c1View = iter.view<c1>();                                                                                 \
				GAIA_EACH(iter) f += c1View[i].value[0];                                                                       \
			});                                                                                                              \
			gaia::dont_optimize(f);                                                                                          \
		}                                                                                                                  \
	}

DEFINE_EACH_ITER_INDEX(1000);

#define DEFINE_EACH_ITER_ALL(ArchetypeCount)                                                                           \
	void BM_Each_Iter_All_##ArchetypeCount(picobench::state& state) {                                                    \
		ecs::World w;                                                                                                      \
		Create_Archetypes_##ArchetypeCount(w);                                                                             \
                                                                                                                       \
		using c1 = Component<0, float, 3>;                                                                                 \
		auto query = w.query().all<const c1>();                                                                            \
                                                                                                                       \
		/* We want to benchmark the hot-path. In real-world scenarios queries are cached so cache them now */              \
		gaia::dont_optimize(query.empty());                                                                                \
                                                                                                                       \
		for (auto _: state) {                                                                                              \
			(void)_;                                                                                                         \
			float f = 0.f;                                                                                                   \
			query.each([&](ecs::IteratorAll iter) {                                                                          \
				auto c1View = iter.view<c1>();                                                                                 \
				GAIA_EACH(iter) f += c1View[i].value[0];                                                                       \
			});                                                                                                              \
			gaia::dont_optimize(f);                                                                                          \
		}                                                                                                                  \
	}

DEFINE_EACH_ITER_ALL(1000);

#define PICO_SETTINGS() iterations({8192}).samples(3)

PICOBENCH_SUITE("each");
PICOBENCH(BM_Each_1).PICO_SETTINGS().label("1 archetype");
PICOBENCH(BM_Each_100).PICO_SETTINGS().label("100 archetypes");
PICOBENCH(BM_Each_1000).PICO_SETTINGS().label("1000 archetypes");

PICOBENCH_SUITE("each - iterator");
PICOBENCH(BM_Each_Iter_1).PICO_SETTINGS().label("1 archetype");
PICOBENCH(BM_Each_Iter_100).PICO_SETTINGS().label("100 archetypes");
PICOBENCH(BM_Each_Iter_1000).PICO_SETTINGS().label("1000 archetypes");
PICOBENCH(BM_Each_Iter_Index_1000).PICO_SETTINGS().label("1000 archetypes (Idx)");
PICOBENCH(BM_Each_Iter_All_1000).PICO_SETTINGS().label("1000 archetypes (All)");
