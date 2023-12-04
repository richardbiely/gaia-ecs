#define PICOBENCH_IMPLEMENT
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
#if GAIA_DEBUG_BUILD
		// Do not to anything in debug builds. It just slows down compilation.
		// This benchmark makes heavy use of templates because it generates
		// thousand of types and does not play well with fast iteration times.
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
	GAIA_FOR(n) {
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
		auto query = w.query().all<c1>();                                                                                  \
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
		auto query = w.query().all<c1>();                                                                                  \
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
		auto query = w.query().all<c1>();                                                                                  \
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
		auto query = w.query().all<c1>();                                                                                  \
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
#define PICO_SETTINGS_1() iterations({8192}).samples(1)
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
			PICOBENCH_REG(BM_Each_Iter_All_1000).PICO_SETTINGS_1().label("1000 archetypes (All)");
		} else {
			PICOBENCH_SUITE_REG("each");
			PICOBENCH_REG(BM_Each_1).PICO_SETTINGS().label("1 archetype");
			PICOBENCH_REG(BM_Each_100).PICO_SETTINGS().label("100 archetypes");
			PICOBENCH_REG(BM_Each_1000).PICO_SETTINGS().label("1000 archetypes");

			PICOBENCH_SUITE_REG("each - iterator");
			PICOBENCH_REG(BM_Each_Iter_1).PICO_SETTINGS().label("1 archetype");
			PICOBENCH_REG(BM_Each_Iter_100).PICO_SETTINGS().label("100 archetypes");
			PICOBENCH_REG(BM_Each_Iter_1000).PICO_SETTINGS().label("1000 archetypes");
			PICOBENCH_REG(BM_Each_Iter_Index_1000).PICO_SETTINGS().label("1000 archetypes (Idx)");
			PICOBENCH_REG(BM_Each_Iter_All_1000).PICO_SETTINGS().label("1000 archetypes (All)");
		}
	}

	return r.run(0);
}
