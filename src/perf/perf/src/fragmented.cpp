#include "common.h"
#include "registry.h"

////////////////////////////////////////////////////////////////////////////////
// Fragmented archetypes
////////////////////////////////////////////////////////////////////////////////

void BM_Fragmented_Read(picobench::state& state) {
	ecs::World w;
	create_fragmented_entities(w, 256U, 128U);

	auto q = w.query().all<Position>();
	dont_optimize(q.empty());

	for (auto _: state) {
		(void)_;
		uint64_t sum = 0ULL;
		q.each([&](const Position& p) {
			sum += (uint64_t)(p.x + p.y + p.z);
		});
		dont_optimize(sum);
	}
}

void BM_Fragmented_Write(picobench::state& state) {
	ecs::World w;
	create_fragmented_entities(w, 256U, 128U);

	auto q = w.query().all<Position&>().all<Velocity>();
	dont_optimize(q.empty());

	for (auto _: state) {
		(void)_;
		q.each([](Position& p, const Velocity& v) {
			p.x += v.x * DeltaTime;
			p.y += v.y * DeltaTime;
			p.z += v.z * DeltaTime;
		});
	}
}

template <bool UseCachedQuery>
void BM_Fragmented_QueryEach_Read1(picobench::state& state) {
	ecs::World w;
	// One entity per archetype isolates query iteration overhead on many tiny chunks.
	create_fragmented_entities(w, 128U, 1U);

	auto q = w.query<UseCachedQuery>().template all<Position>();
	dont_optimize(q.empty());

	for (auto _: state) {
		(void)_;
		uint64_t sum = 0ULL;
		q.each([&](const Position& p) {
			sum += (uint64_t)(p.x + p.y + p.z);
		});
		dont_optimize(sum);
	}
}

template <bool UseCachedQuery>
void BM_Fragmented_QueryEach_Read2(picobench::state& state) {
	ecs::World w;
	// One entity per archetype isolates query iteration overhead on many tiny chunks.
	create_fragmented_entities(w, 128U, 1U);

	auto q = w.query<UseCachedQuery>().template all<Position>().template all<Velocity>();
	dont_optimize(q.empty());

	for (auto _: state) {
		(void)_;
		uint64_t sum = 0ULL;
		q.each([&](const Position& p, const Velocity& v) {
			sum += (uint64_t)(p.x + p.y + p.z + v.x + v.y + v.z);
		});
		dont_optimize(sum);
	}
}

template <bool UseCachedQuery>
void BM_Fragmented_QueryEach_Write2(picobench::state& state) {
	ecs::World w;
	// Many tiny chunks make batching/setup cost more visible than raw component math.
	create_fragmented_entities(w, 128U, 1U);

	auto q = w.query<UseCachedQuery>().template all<Position&>().template all<Velocity>();
	dont_optimize(q.empty());

	for (auto _: state) {
		(void)_;
		q.each([](Position& p, const Velocity& v) {
			p.x += v.x * DeltaTime;
			p.y += v.y * DeltaTime;
			p.z += v.z * DeltaTime;
		});
	}
}

void BM_Fragmented_QueryEach_Read1_Cached(picobench::state& state) {
	BM_Fragmented_QueryEach_Read1<true>(state);
}

void BM_Fragmented_QueryEach_Read1_Uncached(picobench::state& state) {
	BM_Fragmented_QueryEach_Read1<false>(state);
}

void BM_Fragmented_QueryEach_Read2_Cached(picobench::state& state) {
	BM_Fragmented_QueryEach_Read2<true>(state);
}

void BM_Fragmented_QueryEach_Read2_Uncached(picobench::state& state) {
	BM_Fragmented_QueryEach_Read2<false>(state);
}

void BM_Fragmented_QueryEach_Write2_Cached(picobench::state& state) {
	BM_Fragmented_QueryEach_Write2<true>(state);
}

void BM_Fragmented_QueryEach_Write2_Uncached(picobench::state& state) {
	BM_Fragmented_QueryEach_Write2<false>(state);
}

////////////////////////////////////////////////////////////////////////////////
// Query creation / matching costs
////////////////////////////////////////////////////////////////////////////////

template <bool UseCachedQuery, uint32_t QueryComponents>
void BM_QueryBuild(picobench::state& state) {
	ecs::World w;
	create_fragmented_entities(w, 512U, 64U);

	state.stop_timer();
	for (auto _: state) {
		(void)_;
		state.start_timer();

		auto q = create_query<UseCachedQuery, QueryComponents>(w);
		auto& qi = q.fetch();
		q.match_all(qi);

		state.stop_timer();
	}
}

void BM_QueryBuild_Cached_2(picobench::state& state) {
	BM_QueryBuild<true, 2>(state);
}

void BM_QueryBuild_Uncached_2(picobench::state& state) {
	BM_QueryBuild<false, 2>(state);
}

void BM_QueryBuild_Cached_4(picobench::state& state) {
	BM_QueryBuild<true, 4>(state);
}

void BM_QueryBuild_Uncached_4(picobench::state& state) {
	BM_QueryBuild<false, 4>(state);
}

////////////////////////////////////////////////////////////////////////////////

void register_fragmented(PerfRunMode mode) {
	switch (mode) {
		case PerfRunMode::Profiling:
			PICOBENCH_SUITE_REG("Profile picks");
			PICOBENCH_REG(BM_Fragmented_Write).PICO_SETTINGS_HEAVY().label("fragmented write");
			return;
		case PerfRunMode::Sanitizer:
			PICOBENCH_SUITE_REG("Sanitizer picks");
			PICOBENCH_REG(BM_QueryBuild_Cached_4).PICO_SETTINGS_SANI().label("build cached");
			return;
		case PerfRunMode::Normal:
			PICOBENCH_SUITE_REG("Fragmented archetypes");
			PICOBENCH_REG(BM_Fragmented_Read).PICO_SETTINGS().label("read");
			PICOBENCH_REG(BM_Fragmented_Write).PICO_SETTINGS().label("write");
			PICOBENCH_REG(BM_Fragmented_QueryEach_Read1_Cached).PICO_SETTINGS_FOCUS().label("each cached 1c tiny");
			PICOBENCH_REG(BM_Fragmented_QueryEach_Read1_Uncached).PICO_SETTINGS_FOCUS().label("each uncached 1c tiny");
			PICOBENCH_REG(BM_Fragmented_QueryEach_Read2_Cached).PICO_SETTINGS_FOCUS().label("each cached 2c tiny");
			PICOBENCH_REG(BM_Fragmented_QueryEach_Read2_Uncached).PICO_SETTINGS_FOCUS().label("each uncached 2c tiny");
			PICOBENCH_REG(BM_Fragmented_QueryEach_Write2_Cached).PICO_SETTINGS_FOCUS().label("each cached rw tiny");
			PICOBENCH_REG(BM_Fragmented_QueryEach_Write2_Uncached).PICO_SETTINGS_FOCUS().label("each uncached rw tiny");
			PICOBENCH_SUITE_REG("Query build");
			PICOBENCH_REG(BM_QueryBuild_Cached_2).PICO_SETTINGS_HEAVY().label("cached, 2 comp");
			PICOBENCH_REG(BM_QueryBuild_Uncached_2).PICO_SETTINGS_HEAVY().label("uncached, 2 comp");
			PICOBENCH_REG(BM_QueryBuild_Cached_4).PICO_SETTINGS_HEAVY().label("cached, 4 comp");
			PICOBENCH_REG(BM_QueryBuild_Uncached_4).PICO_SETTINGS_HEAVY().label("uncached, 4 comp");
			return;
		default:
			return;
	}
}
