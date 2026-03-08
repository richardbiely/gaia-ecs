// Broader ECS benchmark matrix inspired by https://github.com/SanderMertens/ecs_benchmark.

#define PICOBENCH_IMPLEMENT
#include <gaia.h>
#include <picobench/picobench.hpp>
#include <string_view>

using namespace gaia;

struct Position {
	float x;
	float y;
	float z;
};

struct Velocity {
	float x;
	float y;
	float z;
};

struct Acceleration {
	float x;
	float y;
	float z;
};

struct Mass {
	float value;
};

struct Health {
	int32_t value;
	int32_t max;
};

struct Damage {
	int32_t value;
};

struct Team {
	uint32_t value;
};

struct AIState {
	uint32_t value;
};

struct Frozen {
	bool value;
};

struct Dirty {
	bool value;
};

struct LinkedTo {};
struct VarTag0 {};
struct VarTag1 {};
struct VarTag2 {};
struct VarTag3 {};
struct VarTag4 {};
struct VarTag5 {};
struct VarTag6 {};
struct VarTag7 {};
struct VarTag8 {};
struct VarTag9 {};
struct SourceType0 {};
struct SourceType1 {};
struct SourceTypeOr {};

#if GAIA_OBSERVERS_ENABLED
struct ObsA {};
struct ObsB {};
struct ObsC {};
struct ObsD {};
struct ObsE {};
struct ObsF {};
struct ObsG {};
struct ObsH {};
#endif

static constexpr uint32_t NEntitiesFew = 10'000;
static constexpr uint32_t NEntitiesMedium = 100'000;
static constexpr uint32_t NEntitiesMany = 1'000'000;
#if GAIA_OBSERVERS_ENABLED
static constexpr uint32_t NObserverEntities = 10'000;
#endif

static constexpr float DeltaTime = 0.016f;

////////////////////////////////////////////////////////////////////////////////
// Data setup helpers
////////////////////////////////////////////////////////////////////////////////

#if GAIA_OBSERVERS_ENABLED
template <uint32_t TermCount>
void observer_query_all(ecs::ObserverBuilder& observer) {
	if constexpr (TermCount > 0)
		observer.all<ObsA>();
	if constexpr (TermCount > 1)
		observer.all<ObsB>();
	if constexpr (TermCount > 2)
		observer.all<ObsC>();
	if constexpr (TermCount > 3)
		observer.all<ObsD>();
	if constexpr (TermCount > 4)
		observer.all<ObsE>();
	if constexpr (TermCount > 5)
		observer.all<ObsF>();
	if constexpr (TermCount > 6)
		observer.all<ObsG>();
	if constexpr (TermCount > 7)
		observer.all<ObsH>();
}

template <uint32_t TermCount>
void observer_query_no(ecs::ObserverBuilder& observer) {
	if constexpr (TermCount > 0)
		observer.no<ObsA>();
	if constexpr (TermCount > 1)
		observer.no<ObsB>();
	if constexpr (TermCount > 2)
		observer.no<ObsC>();
	if constexpr (TermCount > 3)
		observer.no<ObsD>();
	if constexpr (TermCount > 4)
		observer.no<ObsE>();
	if constexpr (TermCount > 5)
		observer.no<ObsF>();
	if constexpr (TermCount > 6)
		observer.no<ObsG>();
	if constexpr (TermCount > 7)
		observer.no<ObsH>();
}

template <uint32_t TermCount>
void add_observer_terms_except_last(ecs::World& w, ecs::Entity e) {
	if constexpr (TermCount > 1)
		w.add<ObsA>(e);
	if constexpr (TermCount > 2)
		w.add<ObsB>(e);
	if constexpr (TermCount > 3)
		w.add<ObsC>(e);
	if constexpr (TermCount > 4)
		w.add<ObsD>(e);
	if constexpr (TermCount > 5)
		w.add<ObsE>(e);
	if constexpr (TermCount > 6)
		w.add<ObsF>(e);
	if constexpr (TermCount > 7)
		w.add<ObsG>(e);
}

template <uint32_t TermCount>
void add_observer_all_terms(ecs::World& w, ecs::Entity e) {
	if constexpr (TermCount > 0)
		w.add<ObsA>(e);
	if constexpr (TermCount > 1)
		w.add<ObsB>(e);
	if constexpr (TermCount > 2)
		w.add<ObsC>(e);
	if constexpr (TermCount > 3)
		w.add<ObsD>(e);
	if constexpr (TermCount > 4)
		w.add<ObsE>(e);
	if constexpr (TermCount > 5)
		w.add<ObsF>(e);
	if constexpr (TermCount > 6)
		w.add<ObsG>(e);
	if constexpr (TermCount > 7)
		w.add<ObsH>(e);
}

template <uint32_t TermCount>
void remove_observer_terms_before_last(ecs::World& w, ecs::Entity e) {
	if constexpr (TermCount > 1)
		w.del<ObsA>(e);
	if constexpr (TermCount > 2)
		w.del<ObsB>(e);
	if constexpr (TermCount > 3)
		w.del<ObsC>(e);
	if constexpr (TermCount > 4)
		w.del<ObsD>(e);
	if constexpr (TermCount > 5)
		w.del<ObsE>(e);
	if constexpr (TermCount > 6)
		w.del<ObsF>(e);
	if constexpr (TermCount > 7)
		w.del<ObsG>(e);
}

template <uint32_t TermCount>
void add_observer_last_term(ecs::World& w, ecs::Entity e) {
	if constexpr (TermCount == 1)
		w.add<ObsA>(e);
	if constexpr (TermCount == 2)
		w.add<ObsB>(e);
	if constexpr (TermCount == 3)
		w.add<ObsC>(e);
	if constexpr (TermCount == 4)
		w.add<ObsD>(e);
	if constexpr (TermCount == 5)
		w.add<ObsE>(e);
	if constexpr (TermCount == 6)
		w.add<ObsF>(e);
	if constexpr (TermCount == 7)
		w.add<ObsG>(e);
	if constexpr (TermCount == 8)
		w.add<ObsH>(e);
}

template <uint32_t TermCount>
void remove_observer_last_term(ecs::World& w, ecs::Entity e) {
	if constexpr (TermCount == 1)
		w.del<ObsA>(e);
	if constexpr (TermCount == 2)
		w.del<ObsB>(e);
	if constexpr (TermCount == 3)
		w.del<ObsC>(e);
	if constexpr (TermCount == 4)
		w.del<ObsD>(e);
	if constexpr (TermCount == 5)
		w.del<ObsE>(e);
	if constexpr (TermCount == 6)
		w.del<ObsF>(e);
	if constexpr (TermCount == 7)
		w.del<ObsG>(e);
	if constexpr (TermCount == 8)
		w.del<ObsH>(e);
}
#endif

template <bool WithVelocity, bool WithAcceleration, bool WithHealth, bool WithDamage, bool WithFrozen>
void create_linear_entities(ecs::World& w, cnt::darray<ecs::Entity>& entities, uint32_t count) {
	entities.clear();
	entities.reserve(count);

	GAIA_FOR(count) {
		auto e = w.add();
		entities.push_back(e);

		w.add<Position>(e, {(float)i, (float)(i % 97U), 0.0f});
		w.add<Mass>(e, {1.0f + (float)(i % 16U) * 0.05f});
		w.add<Team>(e, {i % 8U});
		w.add<AIState>(e, {i % 4U});

		if constexpr (WithVelocity)
			w.add<Velocity>(e, {1.0f, 0.5f, 0.25f});
		if constexpr (WithAcceleration)
			w.add<Acceleration>(e, {0.0f, -9.81f * 0.01f, 0.0f});
		if constexpr (WithHealth)
			w.add<Health>(e, {100, 100});
		if constexpr (WithDamage)
			w.add<Damage>(e, {(int32_t)(i % 6U)});
		if constexpr (WithFrozen)
			if ((i % 5U) == 0U)
				w.add<Frozen>(e, {true});
	}
}

void create_fragmented_entities(ecs::World& w, uint32_t archetypes, uint32_t entitiesPerArchetype) {
	GAIA_ASSERT(archetypes > 0U);
	GAIA_ASSERT(entitiesPerArchetype > 0U);

	GAIA_FOR(archetypes) {
		const uint32_t mask = i;

		auto e = w.add();
		auto b = w.build(e);
		b.add<Position>().add<Mass>().add<AIState>();

		if ((mask & 1U) != 0U)
			b.add<Velocity>();
		if ((mask & 2U) != 0U)
			b.add<Acceleration>();
		if ((mask & 4U) != 0U)
			b.add<Health>();
		if ((mask & 8U) != 0U)
			b.add<Damage>();
		if ((mask & 16U) != 0U)
			b.add<Team>();
		if ((mask & 32U) != 0U)
			b.add<Frozen>();
		if ((mask & 64U) != 0U)
			b.add<Dirty>();

		b.commit();

		w.set<Position>(e) = {(float)mask, (float)(mask % 41U), 0.0f};
		w.set<Mass>(e) = {1.0f + (float)(mask % 10U) * 0.1f};
		w.set<AIState>(e) = {mask % 6U};

		if ((mask & 1U) != 0U)
			w.set<Velocity>(e) = {1.0f, 0.1f, 0.05f};
		if ((mask & 2U) != 0U)
			w.set<Acceleration>(e) = {0.0f, -0.01f, 0.0f};
		if ((mask & 4U) != 0U)
			w.set<Health>(e) = {100, 100};
		if ((mask & 8U) != 0U)
			w.set<Damage>(e) = {(int32_t)(mask % 9U)};
		if ((mask & 16U) != 0U)
			w.set<Team>(e) = {mask % 4U};
		if ((mask & 32U) != 0U)
			w.set<Frozen>(e) = {(mask % 3U) == 0U};
		if ((mask & 64U) != 0U)
			w.set<Dirty>(e) = {(mask % 2U) == 0U};

		if (entitiesPerArchetype > 1U)
			w.copy_n(e, entitiesPerArchetype - 1U);
	}
}

template <bool UseCachedQuery, uint32_t QueryComponents>
auto create_query(ecs::World& w) {
	auto q = w.query<UseCachedQuery>().template all<Position>();

	if constexpr (QueryComponents > 1)
		q.template all<Velocity>();
	if constexpr (QueryComponents > 2)
		q.template all<Acceleration>();
	if constexpr (QueryComponents > 3)
		q.template all<Health>();
	if constexpr (QueryComponents > 4)
		q.template all<Damage>();
	if constexpr (QueryComponents > 5)
		q.template all<Mass>();

	return q;
}

void init_systems(ecs::World& w, uint32_t systems, ecs::QueryExecType mode) {
	w.system().name("integrate_pos").all<Position&>().all<Velocity>().mode(mode).on_each([](ecs::Iter& it) {
		auto p = it.view_mut<Position>(0);
		auto v = it.view<Velocity>(1);

		const auto cnt = it.size();
		GAIA_FOR(cnt) {
			p[i].x += v[i].x * DeltaTime;
			p[i].y += v[i].y * DeltaTime;
			p[i].z += v[i].z * DeltaTime;
		}
	});

	if (systems <= 1U)
		return;

	w.system().name("integrate_vel").all<Velocity&>().all<Acceleration>().mode(mode).on_each([](ecs::Iter& it) {
		auto v = it.view_mut<Velocity>(0);
		auto a = it.view<Acceleration>(1);

		const auto cnt = it.size();
		GAIA_FOR(cnt) {
			v[i].x += a[i].x * DeltaTime;
			v[i].y += a[i].y * DeltaTime;
			v[i].z += a[i].z * DeltaTime;
		}
	});

	if (systems <= 2U)
		return;

	w.system().name("apply_damage").all<Health&>().all<Damage>().mode(mode).on_each([](ecs::Iter& it) {
		auto h = it.view_mut<Health>(0);
		auto d = it.view<Damage>(1);

		const auto cnt = it.size();
		GAIA_FOR(cnt) {
			h[i].value -= d[i].value;
			if (h[i].value < 0)
				h[i].value = 0;
		}
	});

	if (systems <= 3U)
		return;

	w.system().name("heal_unfrozen").all<Health&>().no<Frozen>().mode(mode).on_each([](ecs::Iter& it) {
		auto h = it.view_mut<Health>(0);

		const auto cnt = it.size();
		GAIA_FOR(cnt) {
			if (h[i].value < h[i].max)
				h[i].value += 1;
		}
	});

	if (systems <= 4U)
		return;

	w.system().name("mark_dirty").all<Position>().all<Dirty&>().mode(mode).on_each([](ecs::Iter& it) {
		auto p = it.view<Position>(0);
		auto dirty = it.view_mut<Dirty>(1);

		const auto cnt = it.size();
		GAIA_FOR(cnt) {
			dirty[i].value = p[i].x > 50.0f;
		}
	});
}

////////////////////////////////////////////////////////////////////////////////
// Entity lifecycle benchmarks
////////////////////////////////////////////////////////////////////////////////

void BM_EntityCreate_Empty_Add(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;
		state.stop_timer();
		ecs::World w;
		(void)w.add(); // warm archetype edge
		state.start_timer();

		GAIA_FOR(n)(void) w.add();

		state.stop_timer();
	}
}

void BM_EntityCreate_Empty_AddN(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;
		state.stop_timer();
		ecs::World w;
		(void)w.add(); // warm archetype edge
		state.start_timer();

		w.add_n(n);

		state.stop_timer();
	}
}

void BM_EntityCreate_4Comp_OneByOne(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;
		state.stop_timer();
		ecs::World w;

		// warm the component graph
		{
			auto warm = w.add();
			w.add<Position>(warm, {0.0f, 0.0f, 0.0f});
			w.add<Velocity>(warm, {0.0f, 0.0f, 0.0f});
			w.add<Acceleration>(warm, {0.0f, 0.0f, 0.0f});
			w.add<Health>(warm, {100, 100});
		}

		state.start_timer();

		GAIA_FOR(n) {
			auto e = w.add();
			w.add<Position>(e, {(float)i, 1.0f, 2.0f});
			w.add<Velocity>(e, {1.0f, 0.5f, 0.25f});
			w.add<Acceleration>(e, {0.0f, -0.01f, 0.0f});
			w.add<Health>(e, {100, 100});
		}

		state.stop_timer();
	}
}

void BM_EntityCreate_4Comp_Builder(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;
		state.stop_timer();
		ecs::World w;

		// warm the component graph
		{
			auto warm = w.add();
			w.build(warm).add<Position>().add<Velocity>().add<Acceleration>().add<Health>().commit();
		}

		state.start_timer();

		GAIA_FOR(n) {
			auto e = w.add();
			w.build(e).add<Position>().add<Velocity>().add<Acceleration>().add<Health>().commit();
		}

		state.stop_timer();
	}
}

void BM_EntityCopyN_4Comp(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;
		state.stop_timer();
		ecs::World w;

		auto seed = w.add();
		w.add<Position>(seed, {1.0f, 2.0f, 3.0f});
		w.add<Velocity>(seed, {1.0f, 0.5f, 0.25f});
		w.add<Acceleration>(seed, {0.0f, -0.01f, 0.0f});
		w.add<Health>(seed, {100, 100});

		state.start_timer();

		w.copy_n(seed, n);

		state.stop_timer();
	}
}

void BM_EntityDestroy_Empty(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();
	cnt::darray<ecs::Entity> entities;

	for (auto _: state) {
		(void)_;
		state.stop_timer();
		ecs::World w;
		entities.clear();
		entities.reserve(n);
		GAIA_FOR(n) entities.push_back(w.add());
		state.start_timer();

		for (auto e: entities)
			w.del(e);
		w.update();

		state.stop_timer();
	}
}

void BM_EntityDestroy_4Comp(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();
	cnt::darray<ecs::Entity> entities;

	for (auto _: state) {
		(void)_;
		state.stop_timer();
		ecs::World w;
		create_linear_entities<true, true, true, true, false>(w, entities, n);
		state.start_timer();

		for (auto e: entities)
			w.del(e);
		w.update();

		state.stop_timer();
	}
}

////////////////////////////////////////////////////////////////////////////////
// Structural changes
////////////////////////////////////////////////////////////////////////////////

void BM_ComponentAdd_Velocity(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();
	cnt::darray<ecs::Entity> entities;

	for (auto _: state) {
		(void)_;
		state.stop_timer();
		ecs::World w;
		create_linear_entities<false, false, false, false, false>(w, entities, n);

		// warm graph edge
		{
			auto warm = w.add();
			w.add<Position>(warm, {0.0f, 0.0f, 0.0f});
			w.add<Velocity>(warm, {0.0f, 0.0f, 0.0f});
			w.del<Velocity>(warm);
		}

		state.start_timer();

		for (auto e: entities)
			w.add<Velocity>(e, {1.0f, 0.0f, 0.0f});

		state.stop_timer();
	}
}

void BM_ComponentRemove_Velocity(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();
	cnt::darray<ecs::Entity> entities;

	for (auto _: state) {
		(void)_;
		state.stop_timer();
		ecs::World w;
		create_linear_entities<true, false, false, false, false>(w, entities, n);
		state.start_timer();

		for (auto e: entities)
			w.del<Velocity>(e);

		state.stop_timer();
	}
}

void BM_ComponentToggle_Frozen(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();
	cnt::darray<ecs::Entity> entities;
	ecs::World w;
	create_linear_entities<true, true, false, false, false>(w, entities, n);

	bool addPhase = true;
	for (auto _: state) {
		(void)_;
		if (addPhase) {
			for (uint32_t idx = 0U; idx < entities.size(); idx += 2U)
				w.add<Frozen>(entities[idx], {true});
		} else {
			for (uint32_t idx = 0U; idx < entities.size(); idx += 2U)
				w.del<Frozen>(entities[idx]);
		}
		addPhase = !addPhase;
	}
}

void BM_ComponentSetGet_ByEntity(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();
	cnt::darray<ecs::Entity> entities;
	ecs::World w;
	create_linear_entities<false, false, true, false, false>(w, entities, n);

	for (auto _: state) {
		(void)_;
		uint64_t sum = 0ULL;

		for (auto e: entities) {
			auto& p = w.set<Position>(e);
			p.x += 0.125f;
			p.y += 0.250f;
			const auto& h = w.get<Health>(e);
			sum += (uint64_t)(uint32_t)h.value;
		}

		dont_optimize(sum);
	}
}

////////////////////////////////////////////////////////////////////////////////
// Query hot path
////////////////////////////////////////////////////////////////////////////////

void BM_Query_ReadOnly_1Comp(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();
	cnt::darray<ecs::Entity> entities;
	ecs::World w;
	create_linear_entities<false, false, false, false, false>(w, entities, n);

	auto q = w.query().all<Position>();
	dont_optimize(q.empty());

	for (auto _: state) {
		(void)_;
		uint64_t sum = 0ULL;
		q.each([&](const Position& p) {
			sum += (uint64_t)(p.x + p.y);
		});
		dont_optimize(sum);
	}
}

void BM_Query_ReadWrite_2Comp(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();
	cnt::darray<ecs::Entity> entities;
	ecs::World w;
	create_linear_entities<true, false, false, false, false>(w, entities, n);

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

void BM_Query_ReadWrite_4Comp(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();
	cnt::darray<ecs::Entity> entities;
	ecs::World w;
	create_linear_entities<true, true, false, false, false>(w, entities, n);

	auto q = w.query().all<Position&>().all<Velocity&>().all<Acceleration>().all<Mass>();
	dont_optimize(q.empty());

	for (auto _: state) {
		(void)_;
		q.each([](Position& p, Velocity& v, const Acceleration& a, const Mass& m) {
			v.x += a.x * DeltaTime;
			v.y += a.y * DeltaTime;
			v.z += a.z * DeltaTime;

			const float invMass = 1.0f / m.value;
			p.x += v.x * invMass * DeltaTime;
			p.y += v.y * invMass * DeltaTime;
			p.z += v.z * invMass * DeltaTime;
		});
	}
}

void BM_Query_Filter_NoFrozen(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();
	cnt::darray<ecs::Entity> entities;
	ecs::World w;
	create_linear_entities<true, false, false, false, true>(w, entities, n);

	auto q = w.query().all<Position&>().all<Velocity>().no<Frozen>();
	dont_optimize(q.empty());

	for (auto _: state) {
		(void)_;
		q.each([](Position& p, const Velocity& v) {
			p.x += v.x * DeltaTime;
			p.y += v.y * DeltaTime;
		});
	}
}

template <bool BoundVar0>
void BM_Query_Variable_Source(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();
	constexpr uint32_t SourceCnt = 8;
	constexpr uint32_t LinksPerEntity = 4;

	cnt::darray<ecs::Entity> sources;
	sources.reserve(SourceCnt);

	ecs::World w;
	const auto linkedTo = w.add<LinkedTo>().entity;

	GAIA_FOR(SourceCnt) {
		auto source = w.add();
		sources.push_back(source);
		w.add<Health>(source, {100, 100});
	}

	GAIA_FOR(n) {
		auto e = w.add();
		w.add<Position>(e, {(float)i, (float)(i % 97U), 0.0f});
		if ((i & 1U) != 0U)
			w.add<Velocity>(e, {1.0f, 0.5f, 0.25f});
		if ((i & 2U) != 0U)
			w.add<Mass>(e, {1.0f});

		GAIA_FOR_(LinksPerEntity, j) {
			w.add(e, ecs::Pair(linkedTo, sources[j]));
		}
	}

	auto q = w.query()
							 .all<Position>()
							 .all(ecs::Pair(linkedTo, ecs::Var0))
							 .template all<Health>(ecs::QueryTermOptions{}.src(ecs::Var0));
	if constexpr (BoundVar0)
		q.set_var(ecs::Var0, sources[0]);
	else
		q.clear_vars();

	dont_optimize(q.empty());

	for (auto _: state) {
		(void)_;
		uint64_t sum = 0ULL;
		q.each([&](const Position& p) {
			sum += (uint64_t)(p.x + p.y);
		});
		dont_optimize(sum);
	}
}

void BM_Query_Variable_Source_Bound(picobench::state& state) {
	BM_Query_Variable_Source<true>(state);
}

void BM_Query_Variable_Source_Unbound(picobench::state& state) {
	BM_Query_Variable_Source<false>(state);
}

//! Builds many distinct immediate cached queries, all of which should match the same new archetype.
void create_structural_cache_queries(
		ecs::World& w, cnt::darray<ecs::Entity>& tags, cnt::darray<ecs::Query>& queries, uint32_t queryCnt) {
	tags.clear();
	queries.clear();
	tags.reserve(queryCnt);
	queries.reserve(queryCnt);

	GAIA_FOR(queryCnt) {
		const auto tag = w.add();
		tags.push_back(tag);

		auto q = w.query().all(tag);
		dont_optimize(q.count());
		queries.push_back(GAIA_MOV(q));
	}
}

//! Seeds unrelated archetypes so cache maintenance runs against a world that already has many layouts.
void create_unrelated_archetypes(ecs::World& w, uint32_t archetypeCnt) {
	GAIA_FOR(archetypeCnt) {
		auto tag = w.add();
		auto e = w.add();
		auto eb = w.build(e);
		eb.add(tag);
		eb.commit();
	}
}

//! Benchmarks immediate structural cache maintenance when one new archetype fans out to many cached selectors.
void BM_QueryCache_Create_Fanout(picobench::state& state) {
	const uint32_t queryCnt = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;
		state.stop_timer();

		ecs::World w;
		cnt::darray<ecs::Entity> tags;
		cnt::darray<ecs::Query> queries;
		create_structural_cache_queries(w, tags, queries, queryCnt);

		state.start_timer();

		auto e = w.add();
		auto eb = w.build(e);
		for (const auto tag: tags)
			eb.add(tag);
		eb.commit();

		state.stop_timer();
		dont_optimize(queries.back().count());
	}
}

//! Benchmarks immediate structural cache maintenance in worlds that already contain many unrelated archetypes.
template <uint32_t QueryCnt>
void BM_QueryCache_Create_Fanout_Scaled(picobench::state& state) {
	const uint32_t archetypeCnt = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;
		state.stop_timer();

		ecs::World w;
		cnt::darray<ecs::Entity> tags;
		cnt::darray<ecs::Query> queries;
		create_structural_cache_queries(w, tags, queries, QueryCnt);
		create_unrelated_archetypes(w, archetypeCnt);

		state.start_timer();

		auto e = w.add();
		auto eb = w.build(e);
		for (const auto tag: tags)
			eb.add(tag);
		eb.commit();

		state.stop_timer();
		dont_optimize(queries.back().count());
	}
}

void BM_QueryCache_Create_Fanout_Scaled_31(picobench::state& state) {
	BM_QueryCache_Create_Fanout_Scaled<31>(state);
}

void BM_QueryCache_Create_Fanout_Scaled_31_4K(picobench::state& state) {
	BM_QueryCache_Create_Fanout_Scaled<31>(state);
}

struct SourceChain {
	ecs::Entity root = ecs::EntityBad;
	ecs::Entity leaf = ecs::EntityBad;
};

//! Creates a simple parent chain used by traversed source-query cache benchmarks.
SourceChain create_parent_chain(ecs::World& w, uint32_t depth) {
	SourceChain chain{};
	chain.root = w.add();
	chain.leaf = chain.root;

	GAIA_FOR(depth) {
		auto child = w.add();
		w.child(child, chain.leaf);
		chain.leaf = child;
	}

	return chain;
}

//! Benchmarks warm reads for a traversed source query, with traversed-source snapshot caching optional.
template <bool CacheSourceState, uint32_t SourceDepth>
void BM_QueryCache_SourceTraversal_WarmRead(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	cnt::darray<ecs::Entity> entities;
	entities.reserve(n);

	ecs::World w;
	const auto chain = create_parent_chain(w, SourceDepth);
	w.add<Acceleration>(chain.root, {1, 2, 3});

	GAIA_FOR(n) {
		auto e = w.add();
		entities.push_back(e);
		w.add<Position>(e, {(float)i, (float)(i % 97U), 0.0f});
	}

	auto q = CacheSourceState //
							 ? w.query()
										 .cache_src_trav(ecs::MaxCacheSrcTrav)
										 .all<Position>()
										 .all<Acceleration>(ecs::QueryTermOptions{}.src(chain.leaf).trav())
							 : w.query().all<Position>().all<Acceleration>(ecs::QueryTermOptions{}.src(chain.leaf).trav());
	dont_optimize(q.count());

	for (auto _: state) {
		(void)_;
		dont_optimize(q.count());
	}
}

//! Benchmarks warm reads for a direct concrete-source query, with explicit traversed-source snapshot opt-in optional.
//! Direct-source reuse is automatic, so the opt-in path measures redundant API usage overhead only.
template <bool CacheSourceState>
void BM_QueryCache_DirectSource_WarmRead(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	cnt::darray<ecs::Entity> entities;
	entities.reserve(n);

	ecs::World w;
	auto source = w.add();
	w.add<Acceleration>(source, {1, 2, 3});

	GAIA_FOR(n) {
		auto e = w.add();
		entities.push_back(e);
		w.add<Position>(e, {(float)i, (float)(i % 97U), 0.0f});
	}

	auto q = CacheSourceState //
							 ? w.query()
										 .cache_src_trav(ecs::MaxCacheSrcTrav)
										 .all<Position>()
										 .all<Acceleration>(ecs::QueryTermOptions{}.src(source))
							 : w.query().all<Position>().all<Acceleration>(ecs::QueryTermOptions{}.src(source));
	dont_optimize(q.count());

	for (auto _: state) {
		(void)_;
		dont_optimize(q.count());
	}
}

//! Benchmarks warm reads for a query with no source terms, with traversed-source snapshot caching toggled.
template <bool CacheSourceState>
void BM_QueryCache_NoSource_WarmRead(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	cnt::darray<ecs::Entity> entities;
	entities.reserve(n);

	ecs::World w;
	GAIA_FOR(n) {
		auto e = w.add();
		entities.push_back(e);
		w.add<Position>(e, {(float)i, (float)(i % 97U), 0.0f});
		w.add<Acceleration>(e, {1, 2, 3});
	}

	auto q = CacheSourceState //
							 ? w.query().cache_src_trav(ecs::MaxCacheSrcTrav).all<Position>().all<Acceleration>()
							 : w.query().all<Position>().all<Acceleration>();
	dont_optimize(q.count());

	for (auto _: state) {
		(void)_;
		dont_optimize(q.count());
	}
}

void BM_QueryCache_NoSource_WarmRead_Default(picobench::state& state) {
	BM_QueryCache_NoSource_WarmRead<false>(state);
}

void BM_QueryCache_NoSource_WarmRead_SourceState(picobench::state& state) {
	BM_QueryCache_NoSource_WarmRead<true>(state);
}

void BM_QueryCache_DirectSource_WarmRead_Default(picobench::state& state) {
	BM_QueryCache_DirectSource_WarmRead<false>(state);
}

void BM_QueryCache_DirectSource_WarmRead_SourceState(picobench::state& state) {
	BM_QueryCache_DirectSource_WarmRead<true>(state);
}

void BM_QueryCache_SourceTraversal_WarmRead_Lazy(picobench::state& state) {
	BM_QueryCache_SourceTraversal_WarmRead<false, 16>(state);
}

void BM_QueryCache_SourceTraversal_WarmRead_Snapshotted(picobench::state& state) {
	BM_QueryCache_SourceTraversal_WarmRead<true, 16>(state);
}

void BM_QueryCache_SourceTraversal_WarmRead_SmallClosure(picobench::state& state) {
	BM_QueryCache_SourceTraversal_WarmRead<true, 4>(state);
}

void BM_QueryCache_SourceTraversal_WarmRead_LargeClosure(picobench::state& state) {
	BM_QueryCache_SourceTraversal_WarmRead<true, 64>(state);
}

//! Benchmarks warm reads for a relation-versioned dynamic query with a bound variable.
void BM_QueryCache_DynamicRelation_WarmRead(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();
	constexpr uint32_t SourceCnt = 8;

	cnt::darray<ecs::Entity> sources;
	sources.reserve(SourceCnt);

	ecs::World w;
	const auto linkedTo = w.add<LinkedTo>().entity;

	GAIA_FOR(SourceCnt) {
		auto source = w.add();
		sources.push_back(source);
	}

	GAIA_FOR(n) {
		auto e = w.add();
		w.add<Position>(e, {(float)i, (float)(i % 97U), 0.0f});
		w.add(e, ecs::Pair(linkedTo, sources[i % SourceCnt]));
	}

	auto q = w.query().all<Position>().all(ecs::Pair(linkedTo, ecs::Var0));
	q.set_var(ecs::Var0, sources[0]);
	dont_optimize(q.count());

	for (auto _: state) {
		(void)_;
		dont_optimize(q.count());
	}
}

//! Benchmarks repeated structural invalidation and cache refresh while entities churn between existing archetypes.
void BM_QueryCache_Invalidate_Churn(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_linear_entities<true, true, false, false, false>(w, entities, n);

	for (uint32_t i = 0; i < entities.size(); ++i) {
		if ((i & 1U) == 0U)
			w.add<Frozen>(entities[i], {true});
	}

	auto qImmediate = w.query().all<Position&>().all<Velocity&>();
	auto qLazy = w.query().all<Position&>().all<Velocity&>().no<Frozen>();
	auto qBroadLazy = w.query().no<Frozen>();
	dont_optimize(qImmediate.count());
	dont_optimize(qLazy.count());
	dont_optimize(qBroadLazy.count());

	uint32_t cursor = 0U;
	for (auto _: state) {
		(void)_;

		const auto e = entities[cursor % n];
		if (w.has<Frozen>(e))
			w.del<Frozen>(e);
		else
			w.add<Frozen>(e, {true});

		dont_optimize(qImmediate.count());
		dont_optimize(qLazy.count());
		dont_optimize(qBroadLazy.count());
		++cursor;
	}
}

//! Benchmarks relation-driven query invalidation without forcing an immediate query repair.
//! This isolates the cost of routing relation changes through QueryCache.
void BM_QueryCache_Invalidate_Relation(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	entities.reserve(n);

	const auto rel = w.add<LinkedTo>().entity;
	auto rootA = w.add();
	auto rootB = w.add();
	auto source = w.add();

	w.add<Acceleration>(rootA, {1, 2, 3});
	w.add<Acceleration>(rootB, {4, 5, 6});
	w.add(source, ecs::Pair(rel, rootA));

	GAIA_FOR(n) {
		auto e = w.add();
		entities.push_back(e);
		w.add<Position>(e, {(float)i, (float)(i % 97U), 0.0f});
	}

	auto q = w.query().all<Position>().all<Acceleration>(ecs::QueryTermOptions{}.src(source).trav(rel));
	dont_optimize(q.count());

	bool useA = true;
	for (auto _: state) {
		(void)_;

		if (useA) {
			w.del(source, ecs::Pair(rel, rootA));
			w.add(source, ecs::Pair(rel, rootB));
		} else {
			w.del(source, ecs::Pair(rel, rootB));
			w.add(source, ecs::Pair(rel, rootA));
		}

		useA = !useA;
		dont_optimize(q.cache_policy());
	}
}

//! Benchmarks relation-driven invalidation followed by the next read that repairs the dynamic cache.
void BM_QueryCache_Invalidate_Relation_Read(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	entities.reserve(n);

	const auto rel = w.add<LinkedTo>().entity;
	auto rootA = w.add();
	auto rootB = w.add();
	auto source = w.add();

	w.add<Acceleration>(rootA, {1, 2, 3});
	w.add<Acceleration>(rootB, {4, 5, 6});
	w.add(source, ecs::Pair(rel, rootA));

	GAIA_FOR(n) {
		auto e = w.add();
		entities.push_back(e);
		w.add<Position>(e, {(float)i, (float)(i % 97U), 0.0f});
	}

	auto q = w.query().all<Position>().all<Acceleration>(ecs::QueryTermOptions{}.src(source).trav(rel));
	dont_optimize(q.count());

	bool useA = true;
	for (auto _: state) {
		(void)_;

		if (useA) {
			w.del(source, ecs::Pair(rel, rootA));
			w.add(source, ecs::Pair(rel, rootB));
		} else {
			w.del(source, ecs::Pair(rel, rootB));
			w.add(source, ecs::Pair(rel, rootA));
		}

		useA = !useA;
		dont_optimize(q.count());
	}
}

//! Benchmarks warm reads for a sorted query after writes to a component unrelated to the sort key.
//! A narrow sort dirty signal should avoid rebuilding the sorted slices in this case.
void BM_QueryCache_Sorted_UnrelatedWrite(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_linear_entities<true, false, true, false, false>(w, entities, n);

	auto q = w.query().all<Position>().all<Health>().sort_by<Position>(
			[]([[maybe_unused]] const ecs::World& world, const void* pData0, const void* pData1) {
				const auto& p0 = *static_cast<const Position*>(pData0);
				const auto& p1 = *static_cast<const Position*>(pData1);
				if (p0.x < p1.x)
					return -1;
				if (p0.x > p1.x)
					return 1;
				return 0;
			});
	dont_optimize(q.count());

	uint32_t cursor = 0;
	for (auto _: state) {
		(void)_;

		auto& health = w.set<Health>(entities[cursor % entities.size()]);
		health.value += 1;
		dont_optimize(q.count());
		++cursor;
	}
}

//! Benchmarks steady-state warm reads for a sorted query with no intervening world changes.
//! This isolates the read-time overhead of keeping the sorted slices valid.
void BM_QueryCache_Sorted_WarmRead(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_linear_entities<true, false, true, false, false>(w, entities, n);

	auto q = w.query().all<Position>().all<Health>().sort_by<Position>(
			[]([[maybe_unused]] const ecs::World& world, const void* pData0, const void* pData1) {
				const auto& p0 = *static_cast<const Position*>(pData0);
				const auto& p1 = *static_cast<const Position*>(pData1);
				if (p0.x < p1.x)
					return -1;
				if (p0.x > p1.x)
					return 1;
				return 0;
			});
	dont_optimize(q.count());

	for (auto _: state) {
		(void)_;
		dont_optimize(q.count());
	}
}

//! Benchmarks warm reads for a grouped query with a stable group_id selection.
//! The selected group range should be cached instead of scanning all group entries on every read.
void BM_QueryCache_Grouped_WarmRead(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_linear_entities<true, false, false, false, false>(w, entities, n);

	const auto likes = w.add();
	const auto apple = w.add();
	const auto carrot = w.add();
	const auto salad = w.add();

	const ecs::Entity targets[] = {apple, carrot, salad};
	static constexpr uint32_t TargetCount = 3;
	const auto cnt = entities.size();
	GAIA_FOR(cnt) {
		w.add(entities[i], ecs::Pair(likes, targets[i % TargetCount]));
	}

	auto q = w.query().all<Position>().group_by(likes).group_id(carrot);
	dont_optimize(q.count());

	for (auto _: state) {
		(void)_;
		dont_optimize(q.count());
	}
}

//! Benchmarks grouped warm reads while rotating the selected group id.
//! Cached group-id lookup should avoid rescanning all group ranges after each group switch.
void BM_QueryCache_Grouped_SwitchingRead(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_linear_entities<true, false, false, false, false>(w, entities, n);

	const auto likes = w.add();
	const auto apple = w.add();
	const auto carrot = w.add();
	const auto salad = w.add();
	const ecs::Entity targets[] = {apple, carrot, salad};
	static constexpr uint32_t TargetCount = 3;

	const auto cnt = entities.size();
	GAIA_FOR(cnt) {
		w.add(entities[i], ecs::Pair(likes, targets[i % TargetCount]));
	}

	auto q = w.query().all<Position>().group_by(likes);
	dont_optimize(q.count());

	uint32_t cursor = 0;
	for (auto _: state) {
		(void)_;
		q.group_id(targets[cursor % TargetCount]);
		dont_optimize(q.count());
		++cursor;
	}
}

//! Benchmarks repeated creation, emptying, and GC of chunk-heavy archetypes.
//! This exercises World's deferred chunk-delete queue maintenance.
void BM_World_ChunkDeleteQueue_GC(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	struct ChunkQueueBenchTag {};

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	entities.reserve(n);

	for (auto _: state) {
		(void)_;

		entities.clear();
		for (uint32_t i = 0; i < n; ++i) {
			auto e = w.add();
			w.add<ChunkQueueBenchTag>(e);
			entities.push_back(e);
		}

		for (auto e: entities)
			w.del(e);

		w.update();
		dont_optimize(entities.size());
	}
}

//! Benchmarks deleting wildcard pairs matching (*, target) across many relations.
//! The delete loop should avoid copying the full relation set before removing pairs.
void BM_World_Delete_Wildcard_Target(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;

		ecs::World w;
		const auto target = w.add();
		cnt::darray<ecs::Entity> rels;
		rels.reserve(n);

		for (uint32_t i = 0; i < n; ++i) {
			auto rel = w.add();
			rels.push_back(rel);
			auto src = w.add();
			w.add(src, ecs::Pair(rel, target));
		}

		w.del(ecs::Pair(ecs::All, target));
		w.update();
		dont_optimize(rels.size());
	}
}

static inline void add_var_match_tags(ecs::World& w, ecs::Entity e, uint32_t bits) {
	if ((bits & (1u << 0)) != 0u)
		w.add<VarTag0>(e);
	if ((bits & (1u << 1)) != 0u)
		w.add<VarTag1>(e);
	if ((bits & (1u << 2)) != 0u)
		w.add<VarTag2>(e);
	if ((bits & (1u << 3)) != 0u)
		w.add<VarTag3>(e);
	if ((bits & (1u << 4)) != 0u)
		w.add<VarTag4>(e);
	if ((bits & (1u << 5)) != 0u)
		w.add<VarTag5>(e);
	if ((bits & (1u << 6)) != 0u)
		w.add<VarTag6>(e);
	if ((bits & (1u << 7)) != 0u)
		w.add<VarTag7>(e);
	if ((bits & (1u << 8)) != 0u)
		w.add<VarTag8>(e);
}

template <bool BoundVar0>
void BM_QueryMatch_Variable_PairAll(picobench::state& state) {
	const uint32_t archetypeCnt = (uint32_t)state.user_data();
	constexpr uint32_t SourceCnt = 16;

	ecs::World w;

	const auto relA = w.add();
	const auto relB = w.add();
	const auto relC = w.add();
	const auto relD = w.add();

	cnt::sarray<ecs::Entity, SourceCnt> sources{};
	GAIA_FOR(SourceCnt) {
		sources[i] = w.add();
	}

	// Candidate sets with a single shared target (sources[15]) to force backtracking in unbound mode.
	// Keep set sizes small to stay under archetype id capacity limits.
	static constexpr uint8_t candA[] = {0, 1, 2, 3, 15};
	static constexpr uint8_t candB[] = {3, 4, 5, 6, 15};
	static constexpr uint8_t candC[] = {0, 2, 4, 6, 15};
	static constexpr uint8_t candD[] = {1, 3, 5, 7, 15};

	GAIA_FOR(archetypeCnt) {
		auto e = w.add();
		w.add<Position>(e, {(float)i, (float)(i % 97U), 0.0f});
		add_var_match_tags(w, e, i);
		for (const auto idx: candA)
			w.add(e, ecs::Pair(relA, sources[idx]));
		for (const auto idx: candB)
			w.add(e, ecs::Pair(relB, sources[idx]));
		for (const auto idx: candC)
			w.add(e, ecs::Pair(relC, sources[idx]));
		for (const auto idx: candD)
			w.add(e, ecs::Pair(relD, sources[idx]));
	}

	auto q = w.query()
							 .all(ecs::Pair(relA, ecs::Var0))
							 .all(ecs::Pair(relB, ecs::Var0))
							 .all(ecs::Pair(relC, ecs::Var0))
							 .all(ecs::Pair(relD, ecs::Var0));
	if constexpr (BoundVar0)
		q.set_var(ecs::Var0, sources[15]);
	else
		q.clear_vars();

	auto& qi = q.fetch();
	q.match_all(qi);
	dont_optimize(qi.cache_archetype_view().size());

	for (auto _: state) {
		(void)_;
		q.match_all(qi);
		dont_optimize(qi.cache_archetype_view().size());
	}
}

void BM_QueryMatch_Variable_PairAll_Bound(picobench::state& state) {
	BM_QueryMatch_Variable_PairAll<true>(state);
}

void BM_QueryMatch_Variable_PairAll_Unbound(picobench::state& state) {
	BM_QueryMatch_Variable_PairAll<false>(state);
}

template <bool BoundVar0>
void BM_QueryMatch_Variable_1Var(picobench::state& state) {
	const uint32_t archetypeCnt = (uint32_t)state.user_data();
	constexpr uint32_t SourceCnt = 16;

	ecs::World w;

	const auto relA = w.add();
	const auto relB = w.add();

	cnt::sarray<ecs::Entity, SourceCnt> sources{};
	GAIA_FOR(SourceCnt) {
		sources[i] = w.add();
	}
	w.add<SourceType0>(sources[15]);

	// Several pair candidates share a single valid source gate (sources[15]).
	static constexpr uint8_t candA[] = {0, 1, 2, 3, 15};
	static constexpr uint8_t candB[] = {3, 4, 5, 6, 15};

	GAIA_FOR(archetypeCnt) {
		auto e = w.add();
		w.add<Position>(e, {(float)i, (float)(i % 97U), 0.0f});
		add_var_match_tags(w, e, i);

		for (const auto idx: candA)
			w.add(e, ecs::Pair(relA, sources[idx]));
		for (const auto idx: candB)
			w.add(e, ecs::Pair(relB, sources[idx]));
	}

	auto q = w.query()
							 .all(ecs::Pair(relA, ecs::Var0))
							 .all(ecs::Pair(relB, ecs::Var0))
							 .template all<SourceType0>(ecs::QueryTermOptions{}.src(ecs::Var0));
	if constexpr (BoundVar0)
		q.set_var(ecs::Var0, sources[15]);
	else
		q.clear_vars();

	auto& qi = q.fetch();
	q.match_all(qi);
	dont_optimize(qi.cache_archetype_view().size());

	for (auto _: state) {
		(void)_;
		q.match_all(qi);
		dont_optimize(qi.cache_archetype_view().size());
	}
}

void BM_QueryMatch_Variable_1Var_Bound(picobench::state& state) {
	BM_QueryMatch_Variable_1Var<true>(state);
}

void BM_QueryMatch_Variable_1Var_Unbound(picobench::state& state) {
	BM_QueryMatch_Variable_1Var<false>(state);
}

template <bool BoundVars>
void BM_QueryMatch_Variable_2VarPairAll(picobench::state& state) {
	const uint32_t archetypeCnt = (uint32_t)state.user_data();
	constexpr uint32_t SourceCnt = 24;

	ecs::World w;

	const auto relConnected = w.add();
	const auto relMirror = w.add();
	const auto relPowered = w.add();
	const auto relRouted = w.add();

	cnt::sarray<ecs::Entity, SourceCnt> devices{};
	cnt::sarray<ecs::Entity, SourceCnt> powers{};
	GAIA_FOR(SourceCnt) {
		devices[i] = w.add();
		powers[i] = w.add();
	}

	static constexpr uint8_t candConnected[] = {0, 2, 4, 6, 8, 10, 12, 14, 18};
	static constexpr uint8_t candMirror[] = {12, 13, 14, 18, 19};
	static constexpr uint8_t candPowered[] = {3, 6, 9, 12, 15, 18};
	static constexpr uint8_t candRouted[] = {12, 15, 17, 18};

	GAIA_FOR(archetypeCnt) {
		auto e = w.add();
		w.add<Position>(e, {(float)i, (float)(i % 97U), 0.0f});
		add_var_match_tags(w, e, i);

		for (const auto idx: candConnected)
			w.add(e, ecs::Pair(relConnected, devices[idx]));
		for (const auto idx: candMirror)
			w.add(e, ecs::Pair(relMirror, devices[idx]));
		for (const auto idx: candPowered)
			w.add(e, ecs::Pair(relPowered, powers[idx]));
		for (const auto idx: candRouted)
			w.add(e, ecs::Pair(relRouted, powers[idx]));
	}

	auto q = w.query()
							 .all(ecs::Pair(relConnected, ecs::Var0))
							 .all(ecs::Pair(relMirror, ecs::Var0))
							 .all(ecs::Pair(relPowered, ecs::Var1))
							 .all(ecs::Pair(relRouted, ecs::Var1));
	if constexpr (BoundVars) {
		q.set_var(ecs::Var0, devices[12]);
		q.set_var(ecs::Var1, powers[12]);
	} else
		q.clear_vars();

	auto& qi = q.fetch();
	q.match_all(qi);
	dont_optimize(qi.cache_archetype_view().size());

	for (auto _: state) {
		(void)_;
		q.match_all(qi);
		dont_optimize(qi.cache_archetype_view().size());
	}
}

void BM_QueryMatch_Variable_2VarPairAll_Bound(picobench::state& state) {
	BM_QueryMatch_Variable_2VarPairAll<true>(state);
}

void BM_QueryMatch_Variable_2VarPairAll_Unbound(picobench::state& state) {
	BM_QueryMatch_Variable_2VarPairAll<false>(state);
}

template <bool BoundVars>
void BM_QueryMatch_Variable_AllOnly(picobench::state& state) {
	const uint32_t archetypeCnt = (uint32_t)state.user_data();
	constexpr uint32_t SourceCnt = 24;

	ecs::World w;

	const auto relConnected = w.add();
	const auto relMirror = w.add();
	const auto relPowered = w.add();

	cnt::sarray<ecs::Entity, SourceCnt> devices{};
	cnt::sarray<ecs::Entity, SourceCnt> powers{};
	GAIA_FOR(SourceCnt) {
		devices[i] = w.add();
		powers[i] = w.add();
		w.add<SourceType0>(devices[i]);
		w.add<SourceType1>(powers[i]);
	}

	// Keep intersections small to force branching in the unbound ALL-only solver.
	static constexpr uint8_t candConnected[] = {0, 2, 4, 6, 8, 10, 12, 14, 16, 18};
	static constexpr uint8_t candMirror[] = {12, 13, 14, 18, 19};
	static constexpr uint8_t candPowered[] = {3, 6, 9, 12, 15, 18};

	GAIA_FOR(archetypeCnt) {
		auto e = w.add();
		w.add<Position>(e, {(float)i, (float)(i % 97U), 0.0f});
		add_var_match_tags(w, e, i);

		for (const auto idx: candConnected)
			w.add(e, ecs::Pair(relConnected, devices[idx]));
		for (const auto idx: candMirror)
			w.add(e, ecs::Pair(relMirror, devices[idx]));
		for (const auto idx: candPowered)
			w.add(e, ecs::Pair(relPowered, powers[idx]));
	}

	auto q = w.query()
							 .all(ecs::Pair(relConnected, ecs::Var0))
							 .all(ecs::Pair(relMirror, ecs::Var0))
							 .all(ecs::Pair(relPowered, ecs::Var1))
							 .template all<SourceType0>(ecs::QueryTermOptions{}.src(ecs::Var0))
							 .template all<SourceType1>(ecs::QueryTermOptions{}.src(ecs::Var1));
	if constexpr (BoundVars) {
		q.set_var(ecs::Var0, devices[12]);
		q.set_var(ecs::Var1, powers[12]);
	} else
		q.clear_vars();

	auto& qi = q.fetch();
	q.match_all(qi);
	dont_optimize(qi.cache_archetype_view().size());

	for (auto _: state) {
		(void)_;
		q.match_all(qi);
		dont_optimize(qi.cache_archetype_view().size());
	}
}

void BM_QueryMatch_Variable_AllOnly_Bound(picobench::state& state) {
	BM_QueryMatch_Variable_AllOnly<true>(state);
}

void BM_QueryMatch_Variable_AllOnly_Unbound(picobench::state& state) {
	BM_QueryMatch_Variable_AllOnly<false>(state);
}

template <bool BoundVars>
void BM_QueryMatch_Variable_AllOnlyCoupled(picobench::state& state) {
	const uint32_t archetypeCnt = (uint32_t)state.user_data();
	constexpr uint32_t SourceCnt = 24;

	ecs::World w;

	const auto relConnected = w.add();
	const auto relPowered = w.add();

	cnt::sarray<ecs::Entity, SourceCnt> devices{};
	cnt::sarray<ecs::Entity, SourceCnt> powers{};
	GAIA_FOR(SourceCnt) {
		devices[i] = w.add();
		powers[i] = w.add();
	}

	static constexpr uint8_t candConnected[] = {0, 2, 4, 6, 12, 18};
	static constexpr uint8_t candPowered[] = {3, 6, 9, 12, 15, 18};
	struct CoupledPair {
		uint8_t devIdx;
		uint8_t pwrIdx;
	};
	static constexpr CoupledPair coupled[] = {{0, 3}, {2, 9}, {6, 12}, {12, 15}, {18, 18}};

	GAIA_FOR(archetypeCnt) {
		auto e = w.add();
		w.add<Position>(e, {(float)i, (float)(i % 97U), 0.0f});
		add_var_match_tags(w, e, i);

		for (const auto idx: candConnected)
			w.add(e, ecs::Pair(relConnected, devices[idx]));
		for (const auto idx: candPowered)
			w.add(e, ecs::Pair(relPowered, powers[idx]));
		for (const auto& pair: coupled)
			w.add(e, ecs::Pair(devices[pair.devIdx], powers[pair.pwrIdx]));
	}

	auto q = w.query()
							 .all(ecs::Pair(relConnected, ecs::Var0))
							 .all(ecs::Pair(relPowered, ecs::Var1))
							 .all(ecs::Pair(ecs::Var0, ecs::Var1));
	if constexpr (BoundVars) {
		q.set_var(ecs::Var0, devices[12]);
		q.set_var(ecs::Var1, powers[15]);
	} else
		q.clear_vars();

	auto& qi = q.fetch();
	q.match_all(qi);
	dont_optimize(qi.cache_archetype_view().size());

	for (auto _: state) {
		(void)_;
		q.match_all(qi);
		dont_optimize(qi.cache_archetype_view().size());
	}
}

void BM_QueryMatch_Variable_AllOnlyCoupled_Bound(picobench::state& state) {
	BM_QueryMatch_Variable_AllOnlyCoupled<true>(state);
}

void BM_QueryMatch_Variable_AllOnlyCoupled_Unbound(picobench::state& state) {
	BM_QueryMatch_Variable_AllOnlyCoupled<false>(state);
}

template <bool BoundVars>
void BM_QueryMatch_Variable_GenericSourceBacktrack(picobench::state& state) {
	const uint32_t archetypeCnt = (uint32_t)state.user_data();
	constexpr uint32_t SourceCnt = 24;

	ecs::World w;

	const auto relConnected = w.add();
	const auto relRoute = w.add();
	const auto relRouteAlt = w.add();

	cnt::sarray<ecs::Entity, SourceCnt> parents{};
	cnt::sarray<ecs::Entity, SourceCnt> devices{};
	cnt::sarray<ecs::Entity, SourceCnt> routes{};
	GAIA_FOR(SourceCnt) {
		parents[i] = w.add();
		devices[i] = w.add();
		routes[i] = w.add();
		w.add(devices[i], ecs::Pair(ecs::ChildOf, parents[i]));
		if ((i % 3U) == 0)
			w.add<SourceTypeOr>(parents[i]);
	}

	static constexpr uint8_t candConnected[] = {0, 6, 12, 21};
	static constexpr uint8_t candRoutes[] = {0, 12, 21};

	GAIA_FOR(archetypeCnt) {
		auto e = w.add();
		w.add<Position>(e, {(float)i, (float)(i % 97U), 0.0f});
		add_var_match_tags(w, e, i);

		for (const auto idx: candConnected)
			w.add(e, ecs::Pair(relConnected, devices[idx]));
		for (const auto idx: candRoutes) {
			w.add(e, ecs::Pair(relRoute, routes[idx]));
			w.add(e, ecs::Pair(relRouteAlt, routes[(idx + 3U) % SourceCnt]));
			if ((idx % 3U) == 0)
				w.add(e, ecs::Pair(routes[idx], devices[idx]));
		}
	}

	auto q = w.query()
							 .all(ecs::Pair(relConnected, ecs::Var0))
							 .all(ecs::Pair(ecs::Var1, ecs::Var0))
							 .template all<SourceTypeOr>(ecs::QueryTermOptions{}.src(ecs::Var0).trav_parent())
							 .or_(ecs::Pair(relRoute, ecs::Var1))
							 .or_(ecs::Pair(relRouteAlt, ecs::Var1));
	if constexpr (BoundVars) {
		q.set_var(ecs::Var0, devices[12]);
		q.set_var(ecs::Var1, routes[12]);
	} else
		q.clear_vars();

	auto& qi = q.fetch();
	q.match_all(qi);
	dont_optimize(qi.cache_archetype_view().size());

	for (auto _: state) {
		(void)_;
		q.match_all(qi);
		dont_optimize(qi.cache_archetype_view().size());
	}
}

void BM_QueryMatch_Variable_GenericSourceBacktrack_Bound(picobench::state& state) {
	BM_QueryMatch_Variable_GenericSourceBacktrack<true>(state);
}

void BM_QueryMatch_Variable_GenericSourceBacktrack_Unbound(picobench::state& state) {
	BM_QueryMatch_Variable_GenericSourceBacktrack<false>(state);
}

template <bool BoundVar0>
void BM_QueryMatch_Variable_1VarOr(picobench::state& state) {
	const uint32_t archetypeCnt = (uint32_t)state.user_data();
	constexpr uint32_t SourceCnt = 16;

	ecs::World w;

	const auto relA = w.add();
	const auto relB = w.add();
	const auto relC = w.add();

	cnt::sarray<ecs::Entity, SourceCnt> sources{};
	GAIA_FOR(SourceCnt) {
		sources[i] = w.add();
	}
	// Single valid source target forces the one-variable OR path to bind through the source gate.
	w.add<SourceTypeOr>(sources[15]);

	static constexpr uint8_t candA[] = {0, 1, 2, 3, 15};
	static constexpr uint8_t candB[] = {3, 4, 5, 6, 15};
	static constexpr uint8_t candC[] = {1, 7, 8, 9, 15};

	GAIA_FOR(archetypeCnt) {
		auto e = w.add();
		w.add<Position>(e, {(float)i, (float)(i % 97U), 0.0f});
		add_var_match_tags(w, e, i);

		for (const auto idx: candA)
			w.add(e, ecs::Pair(relA, sources[idx]));
		for (const auto idx: candB)
			w.add(e, ecs::Pair(relB, sources[idx]));
		for (const auto idx: candC)
			w.add(e, ecs::Pair(relC, sources[idx]));
	}

	auto q = w.query()
							 .template all<SourceTypeOr>(ecs::QueryTermOptions{}.src(ecs::Var0))
							 .or_(ecs::Pair(relA, ecs::Var0))
							 .or_(ecs::Pair(relB, ecs::Var0))
							 .or_(ecs::Pair(relC, ecs::Var0));
	if constexpr (BoundVar0)
		q.set_var(ecs::Var0, sources[15]);
	else
		q.clear_vars();

	auto& qi = q.fetch();
	q.match_all(qi);
	dont_optimize(qi.cache_archetype_view().size());

	for (auto _: state) {
		(void)_;
		q.match_all(qi);
		dont_optimize(qi.cache_archetype_view().size());
	}
}

void BM_QueryMatch_Variable_1VarOr_Bound(picobench::state& state) {
	BM_QueryMatch_Variable_1VarOr<true>(state);
}

void BM_QueryMatch_Variable_1VarOr_Unbound(picobench::state& state) {
	BM_QueryMatch_Variable_1VarOr<false>(state);
}

template <bool BoundVar0>
void BM_QueryMatch_Variable_1VarOrDown(picobench::state& state) {
	const uint32_t archetypeCnt = (uint32_t)state.user_data();
	constexpr uint32_t SourceCnt = 16;

	ecs::World w;

	const auto relA = w.add();
	const auto relB = w.add();
	const auto relC = w.add();

	cnt::sarray<ecs::Entity, SourceCnt> roots{};
	cnt::sarray<ecs::Entity, SourceCnt> mids{};
	cnt::sarray<ecs::Entity, SourceCnt> leaves{};
	GAIA_FOR(SourceCnt) {
		roots[i] = w.add();
		mids[i] = w.add();
		leaves[i] = w.add();
		w.child(mids[i], roots[i]);
		w.child(leaves[i], mids[i]);
	}
	w.add<SourceTypeOr>(mids[15]);

	static constexpr uint8_t candA[] = {0, 1, 2, 3, 15};
	static constexpr uint8_t candB[] = {3, 4, 5, 6, 15};
	static constexpr uint8_t candC[] = {1, 7, 8, 9, 15};

	GAIA_FOR(archetypeCnt) {
		auto e = w.add();
		w.add<Position>(e, {(float)i, (float)(i % 97U), 0.0f});
		add_var_match_tags(w, e, i);

		for (const auto idx: candA)
			w.add(e, ecs::Pair(relA, roots[idx]));
		for (const auto idx: candB)
			w.add(e, ecs::Pair(relB, roots[idx]));
		for (const auto idx: candC)
			w.add(e, ecs::Pair(relC, roots[idx]));
	}

	auto q = w.query()
							 .template all<SourceTypeOr>(ecs::QueryTermOptions{}.src(ecs::Var0).trav_down())
							 .or_(ecs::Pair(relA, ecs::Var0))
							 .or_(ecs::Pair(relB, ecs::Var0))
							 .or_(ecs::Pair(relC, ecs::Var0));
	if constexpr (BoundVar0)
		q.set_var(ecs::Var0, roots[15]);
	else
		q.clear_vars();

	auto& qi = q.fetch();
	q.match_all(qi);
	dont_optimize(qi.cache_archetype_view().size());

	for (auto _: state) {
		(void)_;
		q.match_all(qi);
		dont_optimize(qi.cache_archetype_view().size());
	}
}

void BM_QueryMatch_Variable_1VarOrDown_Bound(picobench::state& state) {
	BM_QueryMatch_Variable_1VarOrDown<true>(state);
}

void BM_QueryMatch_Variable_1VarOrDown_Unbound(picobench::state& state) {
	BM_QueryMatch_Variable_1VarOrDown<false>(state);
}

template <bool BoundVar0>
void BM_QueryMatch_Variable_1VarOrUpDown(picobench::state& state) {
	const uint32_t archetypeCnt = (uint32_t)state.user_data();
	constexpr uint32_t SourceCnt = 16;

	ecs::World w;

	const auto relA = w.add();
	const auto relB = w.add();
	const auto relC = w.add();

	cnt::sarray<ecs::Entity, SourceCnt> roots{};
	cnt::sarray<ecs::Entity, SourceCnt> mids{};
	cnt::sarray<ecs::Entity, SourceCnt> leaves{};
	GAIA_FOR(SourceCnt) {
		roots[i] = w.add();
		mids[i] = w.add();
		leaves[i] = w.add();
		w.child(mids[i], roots[i]);
		w.child(leaves[i], mids[i]);
	}
	w.add<SourceTypeOr>(mids[15]);

	static constexpr uint8_t candA[] = {0, 1, 2, 3, 15};
	static constexpr uint8_t candB[] = {3, 4, 5, 6, 15};
	static constexpr uint8_t candC[] = {1, 7, 8, 9, 15};

	GAIA_FOR(archetypeCnt) {
		auto e = w.add();
		w.add<Position>(e, {(float)i, (float)(i % 97U), 0.0f});
		add_var_match_tags(w, e, i);

		for (const auto idx: candA)
			w.add(e, ecs::Pair(relA, roots[idx]));
		for (const auto idx: candB)
			w.add(e, ecs::Pair(relB, leaves[idx]));
		for (const auto idx: candC)
			w.add(e, ecs::Pair(relC, roots[idx]));
	}

	auto q = w.query()
							 .template all<SourceTypeOr>(ecs::QueryTermOptions{}.src(ecs::Var0).trav_down().trav_kind(
									 ecs::QueryTravKind::Up | ecs::QueryTravKind::Down))
							 .or_(ecs::Pair(relA, ecs::Var0))
							 .or_(ecs::Pair(relB, ecs::Var0))
							 .or_(ecs::Pair(relC, ecs::Var0));
	if constexpr (BoundVar0)
		q.set_var(ecs::Var0, roots[15]);
	else
		q.clear_vars();

	auto& qi = q.fetch();
	q.match_all(qi);
	dont_optimize(qi.cache_archetype_view().size());

	for (auto _: state) {
		(void)_;
		q.match_all(qi);
		dont_optimize(qi.cache_archetype_view().size());
	}
}

void BM_QueryMatch_Variable_1VarOrUpDown_Bound(picobench::state& state) {
	BM_QueryMatch_Variable_1VarOrUpDown<true>(state);
}

void BM_QueryMatch_Variable_1VarOrUpDown_Unbound(picobench::state& state) {
	BM_QueryMatch_Variable_1VarOrUpDown<false>(state);
}

template <bool BoundVar0>
void BM_QueryMatch_Variable_1VarAny(picobench::state& state) {
	const uint32_t archetypeCnt = (uint32_t)state.user_data();
	constexpr uint32_t SourceCnt = 16;

	ecs::World w;

	const auto relA = w.add();
	const auto relB = w.add();
	const auto relC = w.add();

	cnt::sarray<ecs::Entity, SourceCnt> sources{};
	GAIA_FOR(SourceCnt) {
		sources[i] = w.add();
	}

	static constexpr uint8_t candA[] = {0, 1, 2, 3, 15};
	static constexpr uint8_t candB[] = {3, 4, 5, 6, 15};
	static constexpr uint8_t candC[] = {1, 7, 8, 9, 15};

	GAIA_FOR(archetypeCnt) {
		auto e = w.add();
		w.add<Position>(e, {(float)i, (float)(i % 97U), 0.0f});
		add_var_match_tags(w, e, i);

		for (const auto idx: candA)
			w.add(e, ecs::Pair(relA, sources[idx]));
		for (const auto idx: candB)
			w.add(e, ecs::Pair(relB, sources[idx]));
		for (const auto idx: candC)
			w.add(e, ecs::Pair(relC, sources[idx]));
	}

	auto q = w.query().any(ecs::Pair(relA, ecs::Var0)).any(ecs::Pair(relB, ecs::Var0)).any(ecs::Pair(relC, ecs::Var0));
	if constexpr (BoundVar0)
		q.set_var(ecs::Var0, sources[15]);
	else
		q.clear_vars();

	auto& qi = q.fetch();
	q.match_all(qi);
	dont_optimize(qi.cache_archetype_view().size());

	for (auto _: state) {
		(void)_;
		q.match_all(qi);
		dont_optimize(qi.cache_archetype_view().size());
	}
}

void BM_QueryMatch_Variable_1VarAny_Bound(picobench::state& state) {
	BM_QueryMatch_Variable_1VarAny<true>(state);
}

void BM_QueryMatch_Variable_1VarAny_Unbound(picobench::state& state) {
	BM_QueryMatch_Variable_1VarAny<false>(state);
}

template <bool BoundVar0>
void BM_QueryMatch_Variable_1VarMixed(picobench::state& state) {
	const uint32_t archetypeCnt = (uint32_t)state.user_data();
	constexpr uint32_t SourceCnt = 16;

	ecs::World w;

	const auto relA = w.add();
	const auto relB = w.add();
	const auto relC = w.add();
	const auto relBlocked = w.add();

	cnt::sarray<ecs::Entity, SourceCnt> sources{};
	GAIA_FOR(SourceCnt) {
		sources[i] = w.add();
	}

	static constexpr uint8_t candA[] = {0, 1, 2, 3, 15};
	static constexpr uint8_t candB[] = {3, 4, 5, 6, 15};
	static constexpr uint8_t candC[] = {1, 7, 8, 9, 15};
	static constexpr uint8_t blocked[] = {0, 1, 2, 3};

	GAIA_FOR(archetypeCnt) {
		auto e = w.add();
		w.add<Position>(e, {(float)i, (float)(i % 97U), 0.0f});
		add_var_match_tags(w, e, i);

		for (const auto idx: candA)
			w.add(e, ecs::Pair(relA, sources[idx]));
		for (const auto idx: candB)
			w.add(e, ecs::Pair(relB, sources[idx]));
		for (const auto idx: candC)
			w.add(e, ecs::Pair(relC, sources[idx]));
		for (const auto idx: blocked)
			w.add(e, ecs::Pair(relBlocked, sources[idx]));
	}

	auto q = w.query()
							 .all(ecs::Pair(relA, ecs::Var0))
							 .or_(ecs::Pair(relB, ecs::Var0))
							 .or_(ecs::Pair(relC, ecs::Var0))
							 .no(ecs::Pair(relBlocked, ecs::Var0));
	if constexpr (BoundVar0)
		q.set_var(ecs::Var0, sources[15]);
	else
		q.clear_vars();

	auto& qi = q.fetch();
	q.match_all(qi);
	dont_optimize(qi.cache_archetype_view().size());

	for (auto _: state) {
		(void)_;
		q.match_all(qi);
		dont_optimize(qi.cache_archetype_view().size());
	}
}

void BM_QueryMatch_Variable_1VarMixed_Bound(picobench::state& state) {
	BM_QueryMatch_Variable_1VarMixed<true>(state);
}

void BM_QueryMatch_Variable_1VarMixed_Unbound(picobench::state& state) {
	BM_QueryMatch_Variable_1VarMixed<false>(state);
}

static constexpr uint32_t VariableBuildArchetypeCnt = 128U;
static constexpr uint32_t VariableCompileBatch = 256U;

struct VariableBuildFixture_1VarSource {
	ecs::World w;
	ecs::Entity relA = ecs::EntityBad;
	ecs::Entity relB = ecs::EntityBad;
	cnt::sarray<ecs::Entity, 16> sources{};

	explicit VariableBuildFixture_1VarSource(uint32_t archetypeCnt) {
		relA = w.add();
		relB = w.add();

		GAIA_FOR((uint32_t)sources.size()) {
			sources[i] = w.add();
		}
		w.add<SourceType0>(sources[15]);

		static constexpr uint8_t candA[] = {0, 1, 2, 3, 15};
		static constexpr uint8_t candB[] = {3, 4, 5, 6, 15};

		GAIA_FOR(archetypeCnt) {
			auto e = w.add();
			w.add<Position>(e, {(float)i, (float)(i % 97U), 0.0f});
			add_var_match_tags(w, e, i);

			for (const auto idx: candA)
				w.add(e, ecs::Pair(relA, sources[idx]));
			for (const auto idx: candB)
				w.add(e, ecs::Pair(relB, sources[idx]));
		}
	}

	template <bool UseCachedQuery>
	auto query() {
		return w.query<UseCachedQuery>()
				.all(ecs::Pair(relA, ecs::Var0))
				.all(ecs::Pair(relB, ecs::Var0))
				.template all<SourceType0>(ecs::QueryTermOptions{}.src(ecs::Var0));
	}
};

struct VariableBuildFixture_1VarOrSource {
	ecs::World w;
	ecs::Entity relA = ecs::EntityBad;
	ecs::Entity relB = ecs::EntityBad;
	ecs::Entity relC = ecs::EntityBad;
	cnt::darray<ecs::Entity> sources;

	explicit VariableBuildFixture_1VarOrSource(uint32_t sourceCandidateCnt) {
		const uint32_t sourceTotalCnt = sourceCandidateCnt > 16U ? sourceCandidateCnt : 16U;

		relA = w.add();
		relB = w.add();
		relC = w.add();

		sources.resize(sourceTotalCnt);
		GAIA_FOR(sourceTotalCnt) {
			sources[i] = w.add();
			if (i < sourceCandidateCnt)
				w.add<SourceTypeOr>(sources[i]);
		}

		static constexpr uint8_t candA[] = {0, 1, 2, 3, 15};
		static constexpr uint8_t candB[] = {3, 4, 5, 6, 15};
		static constexpr uint8_t candC[] = {1, 7, 8, 9, 15};

		GAIA_FOR(VariableBuildArchetypeCnt) {
			auto e = w.add();
			w.add<Position>(e, {(float)i, (float)(i % 97U), 0.0f});
			add_var_match_tags(w, e, i);

			for (const auto idx: candA)
				w.add(e, ecs::Pair(relA, sources[idx]));
			for (const auto idx: candB)
				w.add(e, ecs::Pair(relB, sources[idx]));
			for (const auto idx: candC)
				w.add(e, ecs::Pair(relC, sources[idx]));
		}
	}

	template <bool UseCachedQuery>
	auto query() {
		return w.query<UseCachedQuery>()
				.template all<SourceTypeOr>(ecs::QueryTermOptions{}.src(ecs::Var0))
				.or_(ecs::Pair(relA, ecs::Var0))
				.or_(ecs::Pair(relB, ecs::Var0))
				.or_(ecs::Pair(relC, ecs::Var0));
	}
};

struct VariableBuildFixture_2VarPairAll {
	ecs::World w;
	ecs::Entity relConnected = ecs::EntityBad;
	ecs::Entity relMirror = ecs::EntityBad;
	ecs::Entity relPowered = ecs::EntityBad;
	ecs::Entity relRouted = ecs::EntityBad;
	cnt::sarray<ecs::Entity, 24> devices{};
	cnt::sarray<ecs::Entity, 24> powers{};

	explicit VariableBuildFixture_2VarPairAll(uint32_t archetypeCnt) {
		relConnected = w.add();
		relMirror = w.add();
		relPowered = w.add();
		relRouted = w.add();

		GAIA_FOR((uint32_t)devices.size()) {
			devices[i] = w.add();
			powers[i] = w.add();
		}

		static constexpr uint8_t candConnected[] = {0, 2, 4, 6, 8, 10, 12, 14, 18};
		static constexpr uint8_t candMirror[] = {12, 13, 14, 18, 19};
		static constexpr uint8_t candPowered[] = {3, 6, 9, 12, 15, 18};
		static constexpr uint8_t candRouted[] = {12, 15, 17, 18};

		GAIA_FOR(archetypeCnt) {
			auto e = w.add();
			w.add<Position>(e, {(float)i, (float)(i % 97U), 0.0f});
			add_var_match_tags(w, e, i);

			for (const auto idx: candConnected)
				w.add(e, ecs::Pair(relConnected, devices[idx]));
			for (const auto idx: candMirror)
				w.add(e, ecs::Pair(relMirror, devices[idx]));
			for (const auto idx: candPowered)
				w.add(e, ecs::Pair(relPowered, powers[idx]));
			for (const auto idx: candRouted)
				w.add(e, ecs::Pair(relRouted, powers[idx]));
		}
	}

	template <bool UseCachedQuery>
	auto query() {
		return w.query<UseCachedQuery>()
				.all(ecs::Pair(relConnected, ecs::Var0))
				.all(ecs::Pair(relMirror, ecs::Var0))
				.all(ecs::Pair(relPowered, ecs::Var1))
				.all(ecs::Pair(relRouted, ecs::Var1));
	}
};

struct VariableBuildFixture_GenericSourceBacktrack {
	ecs::World w;
	ecs::Entity relConnected = ecs::EntityBad;
	ecs::Entity relRoute = ecs::EntityBad;
	ecs::Entity relRouteAlt = ecs::EntityBad;
	cnt::darray<ecs::Entity> parents;
	cnt::darray<ecs::Entity> devices;
	cnt::darray<ecs::Entity> routes;

	explicit VariableBuildFixture_GenericSourceBacktrack(uint32_t sourceCnt) {
		const uint32_t sourceTotalCnt = sourceCnt > 24U ? sourceCnt : 24U;

		relConnected = w.add();
		relRoute = w.add();
		relRouteAlt = w.add();

		parents.resize(sourceTotalCnt);
		devices.resize(sourceTotalCnt);
		routes.resize(sourceTotalCnt);
		GAIA_FOR(sourceTotalCnt) {
			parents[i] = w.add();
			devices[i] = w.add();
			routes[i] = w.add();
			w.add(devices[i], ecs::Pair(ecs::ChildOf, parents[i]));
			if ((i % 3U) == 0U)
				w.add<SourceTypeOr>(parents[i]);
		}

		static constexpr uint8_t candConnected[] = {0, 6, 12, 21};
		static constexpr uint8_t candRoutes[] = {0, 12, 21};

		GAIA_FOR(VariableBuildArchetypeCnt) {
			auto e = w.add();
			w.add<Position>(e, {(float)i, (float)(i % 97U), 0.0f});
			add_var_match_tags(w, e, i);

			for (const auto idx: candConnected)
				w.add(e, ecs::Pair(relConnected, devices[idx]));
			for (const auto idx: candRoutes) {
				w.add(e, ecs::Pair(relRoute, routes[idx]));
				w.add(e, ecs::Pair(relRouteAlt, routes[(idx + 3U) % sourceTotalCnt]));
				if ((idx % 3U) == 0U)
					w.add(e, ecs::Pair(routes[idx], devices[idx]));
			}
		}
	}

	template <bool UseCachedQuery>
	auto query() {
		return w.query<UseCachedQuery>()
				.all(ecs::Pair(relConnected, ecs::Var0))
				.all(ecs::Pair(ecs::Var1, ecs::Var0))
				.template all<SourceTypeOr>(ecs::QueryTermOptions{}.src(ecs::Var0).trav_parent())
				.or_(ecs::Pair(relRoute, ecs::Var1))
				.or_(ecs::Pair(relRouteAlt, ecs::Var1));
	}
};

template <typename Fixture>
void BM_QueryBuild_Variable_Uncached(picobench::state& state) {
	Fixture fixture((uint32_t)state.user_data());

	state.stop_timer();
	for (auto _: state) {
		(void)_;
		state.start_timer();

		auto q = fixture.template query<false>();
		auto& qi = q.fetch();
		dont_optimize(qi.op_count());
		const auto bytecode = qi.bytecode();
		dont_optimize(bytecode.size());

		state.stop_timer();
	}
}

template <typename Fixture>
void BM_QueryBuild_Variable_Recompile(picobench::state& state) {
	Fixture fixture((uint32_t)state.user_data());

	auto q = fixture.template query<true>();
	auto& qi = q.fetch();
	dont_optimize(qi.op_count());
	const auto bytecodeWarm = qi.bytecode();
	dont_optimize(bytecodeWarm.size());

	state.stop_timer();
	for (auto _: state) {
		(void)_;
		state.start_timer();

		qi.recompile();
		dont_optimize(qi.op_count());
		const auto bytecode = qi.bytecode();
		dont_optimize(bytecode.size());

		state.stop_timer();
	}
}

template <typename Fixture>
void BM_QueryCompile_Variable_Uncached(picobench::state& state) {
	Fixture fixture((uint32_t)state.user_data());

	state.stop_timer();
	for (auto _: state) {
		(void)_;
		state.start_timer();

		uint64_t signatureSum = 0;
		GAIA_FOR(VariableCompileBatch) {
			auto q = fixture.template query<false>();
			dont_optimize(q);
			auto& qi = q.fetch();
			dont_optimize(qi);
			signatureSum ^= qi.op_signature() + (uint64_t)i;
			dont_optimize(qi);
		}
		dont_optimize(signatureSum);

		state.stop_timer();
	}
}

template <typename Fixture>
void BM_QueryCompile_Variable_Recompile(picobench::state& state) {
	Fixture fixture((uint32_t)state.user_data());

	auto q = fixture.template query<true>();
	dont_optimize(q);
	auto& qi = q.fetch();
	dont_optimize(qi);
	dont_optimize(qi.op_signature());

	state.stop_timer();
	for (auto _: state) {
		(void)_;
		state.start_timer();

		uint64_t signatureSum = 0;
		GAIA_FOR(VariableCompileBatch) {
			dont_optimize(qi);
			qi.recompile();
			dont_optimize(qi);
			signatureSum ^= qi.op_signature() + (uint64_t)i;
			dont_optimize(qi);
		}
		dont_optimize(signatureSum);

		state.stop_timer();
	}
}

void BM_QueryBuild_Variable_1VarSource_Uncached(picobench::state& state) {
	BM_QueryBuild_Variable_Uncached<VariableBuildFixture_1VarSource>(state);
}

void BM_QueryBuild_Variable_1VarSource_Recompile(picobench::state& state) {
	BM_QueryBuild_Variable_Recompile<VariableBuildFixture_1VarSource>(state);
}

void BM_QueryBuild_Variable_1VarOrSource_Uncached(picobench::state& state) {
	BM_QueryBuild_Variable_Uncached<VariableBuildFixture_1VarOrSource>(state);
}

void BM_QueryBuild_Variable_1VarOrSource_Recompile(picobench::state& state) {
	BM_QueryBuild_Variable_Recompile<VariableBuildFixture_1VarOrSource>(state);
}

void BM_QueryBuild_Variable_2VarPairAll_Uncached(picobench::state& state) {
	BM_QueryBuild_Variable_Uncached<VariableBuildFixture_2VarPairAll>(state);
}

void BM_QueryBuild_Variable_2VarPairAll_Recompile(picobench::state& state) {
	BM_QueryBuild_Variable_Recompile<VariableBuildFixture_2VarPairAll>(state);
}

void BM_QueryBuild_Variable_GenericSourceBacktrack_Uncached(picobench::state& state) {
	BM_QueryBuild_Variable_Uncached<VariableBuildFixture_GenericSourceBacktrack>(state);
}

void BM_QueryBuild_Variable_GenericSourceBacktrack_Recompile(picobench::state& state) {
	BM_QueryBuild_Variable_Recompile<VariableBuildFixture_GenericSourceBacktrack>(state);
}

void BM_QueryCompile_Variable_1VarSource_Uncached(picobench::state& state) {
	BM_QueryCompile_Variable_Uncached<VariableBuildFixture_1VarSource>(state);
}

void BM_QueryCompile_Variable_1VarSource_Recompile(picobench::state& state) {
	BM_QueryCompile_Variable_Recompile<VariableBuildFixture_1VarSource>(state);
}

void BM_QueryCompile_Variable_1VarOrSource_Uncached(picobench::state& state) {
	BM_QueryCompile_Variable_Uncached<VariableBuildFixture_1VarOrSource>(state);
}

void BM_QueryCompile_Variable_1VarOrSource_Recompile(picobench::state& state) {
	BM_QueryCompile_Variable_Recompile<VariableBuildFixture_1VarOrSource>(state);
}

void BM_QueryCompile_Variable_2VarPairAll_Uncached(picobench::state& state) {
	BM_QueryCompile_Variable_Uncached<VariableBuildFixture_2VarPairAll>(state);
}

void BM_QueryCompile_Variable_2VarPairAll_Recompile(picobench::state& state) {
	BM_QueryCompile_Variable_Recompile<VariableBuildFixture_2VarPairAll>(state);
}

void BM_QueryCompile_Variable_GenericSourceBacktrack_Uncached(picobench::state& state) {
	BM_QueryCompile_Variable_Uncached<VariableBuildFixture_GenericSourceBacktrack>(state);
}

void BM_QueryCompile_Variable_GenericSourceBacktrack_Recompile(picobench::state& state) {
	BM_QueryCompile_Variable_Recompile<VariableBuildFixture_GenericSourceBacktrack>(state);
}

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
// Observer benchmarks
////////////////////////////////////////////////////////////////////////////////

#if GAIA_OBSERVERS_ENABLED
template <uint32_t ObserverCount, uint32_t TermCount>
void BM_Observer_OnAdd(picobench::state& state) {
	static_assert(TermCount >= 1 && TermCount <= 8);

	const uint32_t n = (uint32_t)state.user_data();
	cnt::darray<ecs::Entity> entities;

	for (auto _: state) {
		(void)_;
		state.stop_timer();

		ecs::World w;
		entities.clear();
		entities.reserve(n);
		uint64_t hits = 0;

		GAIA_FOR(ObserverCount) {
			auto observer = w.observer().event(ecs::ObserverEvent::OnAdd);
			observer_query_all<TermCount>(observer);
			observer.on_each([&hits](ecs::Iter& it) {
				(void)it;
				++hits;
			});
		}

		GAIA_FOR(n) {
			auto e = w.add();
			entities.push_back(e);
			add_observer_terms_except_last<TermCount>(w, e);
		}

		hits = 0;
		state.start_timer();

		for (auto e: entities)
			add_observer_last_term<TermCount>(w, e);

		state.stop_timer();
		dont_optimize(hits);
	}
}

template <uint32_t ObserverCount, uint32_t TermCount>
void BM_Observer_OnDel(picobench::state& state) {
	static_assert(TermCount >= 1 && TermCount <= 8);

	const uint32_t n = (uint32_t)state.user_data();
	cnt::darray<ecs::Entity> entities;

	for (auto _: state) {
		(void)_;
		state.stop_timer();

		ecs::World w;
		entities.clear();
		entities.reserve(n);
		uint64_t hits = 0;

		GAIA_FOR(ObserverCount) {
			auto observer = w.observer().event(ecs::ObserverEvent::OnDel);
			observer_query_no<TermCount>(observer);
			observer.on_each([&hits](ecs::Iter& it) {
				(void)it;
				++hits;
			});
		}

		GAIA_FOR(n) {
			auto e = w.add();
			entities.push_back(e);
			add_observer_all_terms<TermCount>(w, e);
		}

		hits = 0;
		state.start_timer();

		for (auto e: entities) {
			remove_observer_terms_before_last<TermCount>(w, e);
			remove_observer_last_term<TermCount>(w, e);
		}

		state.stop_timer();
		dont_optimize(hits);
	}
}

void BM_Observer_OnAdd_0_1(picobench::state& state) {
	BM_Observer_OnAdd<0, 1>(state);
}
void BM_Observer_OnAdd_1_1(picobench::state& state) {
	BM_Observer_OnAdd<1, 1>(state);
}
void BM_Observer_OnAdd_10_1(picobench::state& state) {
	BM_Observer_OnAdd<10, 1>(state);
}
void BM_Observer_OnAdd_50_1(picobench::state& state) {
	BM_Observer_OnAdd<50, 1>(state);
}
void BM_Observer_OnAdd_50_2(picobench::state& state) {
	BM_Observer_OnAdd<50, 2>(state);
}
void BM_Observer_OnAdd_50_4(picobench::state& state) {
	BM_Observer_OnAdd<50, 4>(state);
}
void BM_Observer_OnAdd_50_8(picobench::state& state) {
	BM_Observer_OnAdd<50, 8>(state);
}

void BM_Observer_OnDel_0_1(picobench::state& state) {
	BM_Observer_OnDel<0, 1>(state);
}
void BM_Observer_OnDel_1_1(picobench::state& state) {
	BM_Observer_OnDel<1, 1>(state);
}
void BM_Observer_OnDel_10_1(picobench::state& state) {
	BM_Observer_OnDel<10, 1>(state);
}
void BM_Observer_OnDel_50_1(picobench::state& state) {
	BM_Observer_OnDel<50, 1>(state);
}
void BM_Observer_OnDel_50_2(picobench::state& state) {
	BM_Observer_OnDel<50, 2>(state);
}
void BM_Observer_OnDel_50_4(picobench::state& state) {
	BM_Observer_OnDel<50, 4>(state);
}
void BM_Observer_OnDel_50_8(picobench::state& state) {
	BM_Observer_OnDel<50, 8>(state);
}
#endif

////////////////////////////////////////////////////////////////////////////////
// System frame benchmarks
////////////////////////////////////////////////////////////////////////////////

template <uint32_t Systems>
void BM_SystemFrame(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_linear_entities<true, true, true, true, false>(w, entities, n);

	for (uint32_t i = 0U; i < entities.size(); ++i) {
		if ((i % 5U) == 0U)
			w.add<Frozen>(entities[i], {true});
		if ((i % 7U) == 0U)
			w.add<Dirty>(entities[i], {false});
	}

	// We intentionally keep this single-threaded in the matrix suite.
	init_systems(w, Systems, ecs::QueryExecType::Default);

	// Warm query caches and system scheduling path
	for (uint32_t i = 0U; i < 4U; ++i)
		w.update();

	for (auto _: state) {
		(void)_;
		w.update();
	}
}

void BM_SystemFrame_Serial_2(picobench::state& state) {
	BM_SystemFrame<2>(state);
}

void BM_SystemFrame_Serial_5(picobench::state& state) {
	BM_SystemFrame<5>(state);
}

////////////////////////////////////////////////////////////////////////////////
// Mixed churn benchmark
////////////////////////////////////////////////////////////////////////////////

void BM_MixedFrame_Churn(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_linear_entities<true, true, true, true, false>(w, entities, n);

	auto q = w.query().all<Position&>().all<Velocity&>().no<Frozen>();
	dont_optimize(q.empty());

	uint32_t cursor = 0U;

	for (auto _: state) {
		(void)_;

		q.each([](Position& p, Velocity& v) {
			v.y += -0.1f * DeltaTime;
			p.x += v.x * DeltaTime;
			p.y += v.y * DeltaTime;
		});

		const uint32_t toggleCount = core::get_max(64U, n / 64U);
		GAIA_FOR(toggleCount) {
			const uint32_t idx = (cursor + i) % n;
			auto e = entities[idx];
			if (w.has<Frozen>(e))
				w.del<Frozen>(e);
			else
				w.add<Frozen>(e, {true});
		}

		const uint32_t churnCount = core::get_max(32U, n / 128U);
		GAIA_FOR(churnCount) {
			const uint32_t idx = cursor % n;
			auto oldE = entities[idx];

			auto newE = w.add();
			w.add<Position>(newE, {(float)idx, 0.0f, 0.0f});
			w.add<Velocity>(newE, {1.0f, 0.0f, 0.0f});
			w.add<Acceleration>(newE, {0.0f, -0.02f, 0.0f});
			w.add<Health>(newE, {100, 100});
			w.add<Damage>(newE, {(int32_t)(idx % 7U)});

			w.del(oldE);
			entities[idx] = newE;
			++cursor;
		}

		w.update();
	}
}

////////////////////////////////////////////////////////////////////////////////
// Main
////////////////////////////////////////////////////////////////////////////////

#define PICO_SETTINGS() iterations({256}).samples(3)
#define PICO_SETTINGS_HEAVY() iterations({64}).samples(3)
#define PICO_SETTINGS_FOCUS() iterations({256}).samples(7)
#if GAIA_OBSERVERS_ENABLED
	#define PICO_SETTINGS_OBS() iterations({64}).samples(3)
#endif
#define PICO_SETTINGS_SANI() iterations({8}).samples(1)
#define PICOBENCH_SUITE_REG(name) r.current_suite_name() = name;
#define PICOBENCH_REG(func) (void)r.add_benchmark(#func, func)

int main(int argc, char* argv[]) {
	picobench::runner r(true);

	bool profilingMode = false;
	bool sanitizerMode = false;

	gaia::cnt::darray<const char*> picobenchArgs;
	picobenchArgs.reserve((uint32_t)argc);
	picobenchArgs.push_back(argv[0]);

	for (int i = 1; i < argc; ++i) {
		const std::string_view arg(argv[i]);
		if (arg == "-p") {
			profilingMode = true;
			continue;
		}

		if (arg == "-s") {
			sanitizerMode = true;
			continue;
		}

		picobenchArgs.push_back(argv[i]);
	}

	GAIA_LOG_N("Profiling mode = %s", profilingMode ? "ON" : "OFF");
	GAIA_LOG_N("Sanitizer mode = %s", sanitizerMode ? "ON" : "OFF");

	if (profilingMode) {
		PICOBENCH_SUITE_REG("Profile picks");
		PICOBENCH_REG(BM_SystemFrame_Serial_5).PICO_SETTINGS_HEAVY().user_data(NEntitiesMedium).label("systems serial, 5");
		PICOBENCH_REG(BM_MixedFrame_Churn).PICO_SETTINGS_HEAVY().user_data(NEntitiesMedium).label("mixed churn");
		PICOBENCH_REG(BM_Fragmented_Write).PICO_SETTINGS_HEAVY().label("fragmented write");
		PICOBENCH_REG(BM_QueryCache_SourceTraversal_WarmRead_Snapshotted)
				.PICO_SETTINGS_HEAVY()
				.user_data(NEntitiesMedium)
				.label("query cache, traversed source snap");
#if GAIA_OBSERVERS_ENABLED
		PICOBENCH_REG(BM_Observer_OnAdd_50_4)
				.PICO_SETTINGS_OBS()
				.user_data(NObserverEntities)
				.label("on_add, 50 obs, 4 terms");
#endif
	} else if (sanitizerMode) {
		PICOBENCH_SUITE_REG("Sanitizer picks");
		PICOBENCH_REG(BM_EntityCreate_Empty_Add).PICO_SETTINGS_SANI().user_data(NEntitiesFew).label("create add");
		PICOBENCH_REG(BM_ComponentAdd_Velocity).PICO_SETTINGS_SANI().user_data(NEntitiesFew).label("add velocity");
		PICOBENCH_REG(BM_Query_ReadWrite_2Comp).PICO_SETTINGS_SANI().user_data(NEntitiesFew).label("query rw");
		PICOBENCH_REG(BM_QueryCache_SourceTraversal_WarmRead_Snapshotted)
				.PICO_SETTINGS_SANI()
				.user_data(NEntitiesFew)
				.label("query cache, traversed source snap");
		PICOBENCH_REG(BM_QueryBuild_Cached_4).PICO_SETTINGS_SANI().label("build cached");
		PICOBENCH_REG(BM_SystemFrame_Serial_2).PICO_SETTINGS_SANI().user_data(NEntitiesFew).label("systems serial");
#if GAIA_OBSERVERS_ENABLED
		PICOBENCH_REG(BM_Observer_OnAdd_10_1).PICO_SETTINGS_SANI().user_data(1000).label("on_add, 10 obs");
#endif
	} else {
		PICOBENCH_SUITE_REG("Entity lifecycle");
		PICOBENCH_REG(BM_EntityCreate_Empty_Add).PICO_SETTINGS().user_data(NEntitiesMedium).label("add, 100K");
		PICOBENCH_REG(BM_EntityCreate_Empty_Add).PICO_SETTINGS_HEAVY().user_data(NEntitiesMany).label("add, 1M");
		PICOBENCH_REG(BM_EntityCreate_Empty_AddN).PICO_SETTINGS().user_data(NEntitiesMedium).label("add_n, 100K");
		PICOBENCH_REG(BM_EntityCreate_Empty_AddN).PICO_SETTINGS_HEAVY().user_data(NEntitiesMany).label("add_n, 1M");
		PICOBENCH_REG(BM_EntityCreate_4Comp_OneByOne).PICO_SETTINGS().user_data(NEntitiesFew).label("4comp, one-by-one");
		PICOBENCH_REG(BM_EntityCreate_4Comp_Builder).PICO_SETTINGS().user_data(NEntitiesFew).label("4comp, builder");
		PICOBENCH_REG(BM_EntityCopyN_4Comp).PICO_SETTINGS().user_data(NEntitiesMedium).label("copy_n, 100K");
		PICOBENCH_REG(BM_EntityDestroy_Empty).PICO_SETTINGS().user_data(NEntitiesMedium).label("destroy empty");
		PICOBENCH_REG(BM_EntityDestroy_4Comp).PICO_SETTINGS().user_data(NEntitiesFew).label("destroy 4comp");

		PICOBENCH_SUITE_REG("Structural changes");
		PICOBENCH_REG(BM_ComponentAdd_Velocity).PICO_SETTINGS().user_data(NEntitiesMedium).label("add velocity");
		PICOBENCH_REG(BM_ComponentRemove_Velocity).PICO_SETTINGS().user_data(NEntitiesMedium).label("remove velocity");
		PICOBENCH_REG(BM_ComponentToggle_Frozen).PICO_SETTINGS().user_data(NEntitiesMedium).label("toggle frozen");
		PICOBENCH_REG(BM_ComponentSetGet_ByEntity).PICO_SETTINGS().user_data(NEntitiesMedium).label("set/get by entity");

		PICOBENCH_SUITE_REG("Query hot path");
		PICOBENCH_REG(BM_Query_ReadOnly_1Comp).PICO_SETTINGS().user_data(NEntitiesMedium).label("ro 1 comp");
		PICOBENCH_REG(BM_Query_ReadWrite_2Comp).PICO_SETTINGS().user_data(NEntitiesMedium).label("rw 2 comp");
		PICOBENCH_REG(BM_Query_ReadWrite_4Comp).PICO_SETTINGS().user_data(NEntitiesMedium).label("rw 4 comp");
		PICOBENCH_REG(BM_Query_Filter_NoFrozen).PICO_SETTINGS().user_data(NEntitiesMedium).label("rw + no<Frozen>");
		PICOBENCH_REG(BM_Query_Variable_Source_Bound)
				.PICO_SETTINGS()
				.user_data(NEntitiesMedium)
				.label("var source (bound)");
		PICOBENCH_REG(BM_Query_Variable_Source_Unbound)
				.PICO_SETTINGS()
				.user_data(NEntitiesMedium)
				.label("var source (unbound)");

		PICOBENCH_SUITE_REG("Query cache maintenance");
		PICOBENCH_REG(BM_QueryCache_Create_Fanout).PICO_SETTINGS().user_data(16).label("create fanout 16");
		PICOBENCH_REG(BM_QueryCache_Create_Fanout).PICO_SETTINGS().user_data(24).label("create fanout 24");
		PICOBENCH_REG(BM_QueryCache_Create_Fanout_Scaled_31)
				.PICO_SETTINGS_FOCUS()
				.user_data(1024)
				.label("create fanout 31q 1K arch");
		PICOBENCH_REG(BM_QueryCache_Create_Fanout_Scaled_31_4K)
				.PICO_SETTINGS_FOCUS()
				.user_data(4096)
				.label("create fanout 31q 4K arch");
		PICOBENCH_REG(BM_QueryCache_NoSource_WarmRead_Default)
				.PICO_SETTINGS()
				.user_data(NEntitiesMedium)
				.label("no source default");
		PICOBENCH_REG(BM_QueryCache_NoSource_WarmRead_SourceState)
				.PICO_SETTINGS()
				.user_data(NEntitiesMedium)
				.label("no source src-trav");
		PICOBENCH_REG(BM_QueryCache_DirectSource_WarmRead_Default)
				.PICO_SETTINGS()
				.user_data(NEntitiesMedium)
				.label("direct source default");
		PICOBENCH_REG(BM_QueryCache_DirectSource_WarmRead_SourceState)
				.PICO_SETTINGS()
				.user_data(NEntitiesMedium)
				.label("direct source src-trav");
		PICOBENCH_REG(BM_QueryCache_SourceTraversal_WarmRead_Lazy)
				.PICO_SETTINGS()
				.user_data(NEntitiesMedium)
				.label("traversed source lazy");
		PICOBENCH_REG(BM_QueryCache_SourceTraversal_WarmRead_Snapshotted)
				.PICO_SETTINGS()
				.user_data(NEntitiesMedium)
				.label("traversed source snapshotted");
		PICOBENCH_REG(BM_QueryCache_SourceTraversal_WarmRead_SmallClosure)
				.PICO_SETTINGS_FOCUS()
				.user_data(NEntitiesMedium)
				.label("traversed source snap small");
		PICOBENCH_REG(BM_QueryCache_SourceTraversal_WarmRead_LargeClosure)
				.PICO_SETTINGS_FOCUS()
				.user_data(NEntitiesMedium)
				.label("traversed source snap large");
		PICOBENCH_REG(BM_QueryCache_DynamicRelation_WarmRead)
				.PICO_SETTINGS()
				.user_data(NEntitiesMedium)
				.label("dynamic relation warm");
		PICOBENCH_REG(BM_QueryCache_Invalidate_Churn)
				.PICO_SETTINGS_FOCUS()
				.user_data(NEntitiesMedium)
				.label("invalidate churn 100K");
		PICOBENCH_REG(BM_QueryCache_Invalidate_Relation)
				.PICO_SETTINGS_FOCUS()
				.user_data(NEntitiesMedium)
				.label("invalidate relation 100K");
		PICOBENCH_REG(BM_QueryCache_Invalidate_Relation_Read)
				.PICO_SETTINGS_FOCUS()
				.user_data(NEntitiesMedium)
				.label("invalidate relation+read 100K");
		PICOBENCH_REG(BM_QueryCache_Sorted_UnrelatedWrite)
				.PICO_SETTINGS_FOCUS()
				.user_data(NEntitiesFew)
				.label("sorted unrelated write 10K");
		PICOBENCH_REG(BM_QueryCache_Sorted_WarmRead)
				.PICO_SETTINGS_FOCUS()
				.user_data(NEntitiesFew)
				.label("sorted warm read 10K");
		PICOBENCH_REG(BM_QueryCache_Grouped_WarmRead)
				.PICO_SETTINGS_FOCUS()
				.user_data(NEntitiesFew)
				.label("grouped warm read 10K");
		PICOBENCH_REG(BM_QueryCache_Grouped_SwitchingRead)
				.PICO_SETTINGS_FOCUS()
				.user_data(NEntitiesFew)
				.label("grouped switching read 10K");
		PICOBENCH_REG(BM_World_ChunkDeleteQueue_GC)
				.PICO_SETTINGS_FOCUS()
				.user_data(NEntitiesFew)
				.label("chunk delete queue gc 10K");
		PICOBENCH_REG(BM_World_Delete_Wildcard_Target)
				.PICO_SETTINGS_FOCUS()
				.user_data(1000)
				.label("delete wildcard target 1K");

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

		PICOBENCH_SUITE_REG("Query variable build");
		PICOBENCH_REG(BM_QueryBuild_Variable_1VarSource_Uncached)
				.PICO_SETTINGS_FOCUS()
				.user_data(VariableBuildArchetypeCnt)
				.label("uncached, 1var source-gated");
		PICOBENCH_REG(BM_QueryBuild_Variable_1VarSource_Recompile)
				.PICO_SETTINGS_FOCUS()
				.user_data(VariableBuildArchetypeCnt)
				.label("recompile, 1var source-gated");
		PICOBENCH_REG(BM_QueryBuild_Variable_1VarOrSource_Uncached)
				.PICO_SETTINGS_FOCUS()
				.user_data(1)
				.label("uncached, 1var or-source, 1 src");
		PICOBENCH_REG(BM_QueryBuild_Variable_1VarOrSource_Uncached)
				.PICO_SETTINGS_FOCUS()
				.user_data(4096)
				.label("uncached, 1var or-source, 4K src");
		PICOBENCH_REG(BM_QueryBuild_Variable_1VarOrSource_Recompile)
				.PICO_SETTINGS_FOCUS()
				.user_data(1)
				.label("recompile, 1var or-source, 1 src");
		PICOBENCH_REG(BM_QueryBuild_Variable_1VarOrSource_Recompile)
				.PICO_SETTINGS_FOCUS()
				.user_data(4096)
				.label("recompile, 1var or-source, 4K src");
		PICOBENCH_REG(BM_QueryBuild_Variable_2VarPairAll_Uncached)
				.PICO_SETTINGS_FOCUS()
				.user_data(VariableBuildArchetypeCnt)
				.label("uncached, 2var pair-all");
		PICOBENCH_REG(BM_QueryBuild_Variable_2VarPairAll_Recompile)
				.PICO_SETTINGS_FOCUS()
				.user_data(VariableBuildArchetypeCnt)
				.label("recompile, 2var pair-all");
		PICOBENCH_REG(BM_QueryBuild_Variable_GenericSourceBacktrack_Uncached)
				.PICO_SETTINGS_FOCUS()
				.user_data(24)
				.label("uncached, generic source-backtrack, 24 src");
		PICOBENCH_REG(BM_QueryBuild_Variable_GenericSourceBacktrack_Uncached)
				.PICO_SETTINGS_FOCUS()
				.user_data(4096)
				.label("uncached, generic source-backtrack, 4K src");
		PICOBENCH_REG(BM_QueryBuild_Variable_GenericSourceBacktrack_Recompile)
				.PICO_SETTINGS_FOCUS()
				.user_data(24)
				.label("recompile, generic source-backtrack, 24 src");
		PICOBENCH_REG(BM_QueryBuild_Variable_GenericSourceBacktrack_Recompile)
				.PICO_SETTINGS_FOCUS()
				.user_data(4096)
				.label("recompile, generic source-backtrack, 4K src");

		PICOBENCH_SUITE_REG("Query variable compile");
		PICOBENCH_REG(BM_QueryCompile_Variable_1VarSource_Uncached)
				.PICO_SETTINGS_FOCUS()
				.user_data(VariableBuildArchetypeCnt)
				.label("uncached x256, 1var source-gated");
		PICOBENCH_REG(BM_QueryCompile_Variable_1VarOrSource_Uncached)
				.PICO_SETTINGS_FOCUS()
				.user_data(1)
				.label("uncached x256, 1var or-source, 1 src");
		PICOBENCH_REG(BM_QueryCompile_Variable_1VarOrSource_Uncached)
				.PICO_SETTINGS_FOCUS()
				.user_data(4096)
				.label("uncached x256, 1var or-source, 4K src");
		PICOBENCH_REG(BM_QueryCompile_Variable_2VarPairAll_Uncached)
				.PICO_SETTINGS_FOCUS()
				.user_data(VariableBuildArchetypeCnt)
				.label("uncached x256, 2var pair-all");
		PICOBENCH_REG(BM_QueryCompile_Variable_GenericSourceBacktrack_Uncached)
				.PICO_SETTINGS_FOCUS()
				.user_data(24)
				.label("uncached x256, generic source-backtrack, 24 src");
		PICOBENCH_REG(BM_QueryCompile_Variable_GenericSourceBacktrack_Uncached)
				.PICO_SETTINGS_FOCUS()
				.user_data(4096)
				.label("uncached x256, generic source-backtrack, 4K src");

		PICOBENCH_SUITE_REG("Query variable match");
		PICOBENCH_REG(BM_QueryMatch_Variable_1Var_Bound)
				.PICO_SETTINGS_HEAVY()
				.user_data(128)
				.label("match 1var source-gated (bound)");
		PICOBENCH_REG(BM_QueryMatch_Variable_1Var_Unbound)
				.PICO_SETTINGS_HEAVY()
				.user_data(128)
				.label("match 1var source-gated (unbound)");
		PICOBENCH_REG(BM_QueryMatch_Variable_PairAll_Bound)
				.PICO_SETTINGS_HEAVY()
				.user_data(128)
				.label("match 1var pair-all (bound)");
		PICOBENCH_REG(BM_QueryMatch_Variable_PairAll_Unbound)
				.PICO_SETTINGS_HEAVY()
				.user_data(128)
				.label("match 1var pair-all (unbound)");
		PICOBENCH_REG(BM_QueryMatch_Variable_2VarPairAll_Bound)
				.PICO_SETTINGS_HEAVY()
				.user_data(128)
				.label("match 2var pair-all (bound)");
		PICOBENCH_REG(BM_QueryMatch_Variable_2VarPairAll_Unbound)
				.PICO_SETTINGS_HEAVY()
				.user_data(128)
				.label("match 2var pair-all (unbound)");
		PICOBENCH_REG(BM_QueryMatch_Variable_AllOnly_Bound)
				.PICO_SETTINGS_HEAVY()
				.user_data(128)
				.label("match all-only 2var src-gated (bound)");
		PICOBENCH_REG(BM_QueryMatch_Variable_AllOnly_Unbound)
				.PICO_SETTINGS_HEAVY()
				.user_data(128)
				.label("match all-only 2var src-gated (unbound)");
		PICOBENCH_REG(BM_QueryMatch_Variable_AllOnlyCoupled_Bound)
				.PICO_SETTINGS_HEAVY()
				.user_data(128)
				.label("match all-only coupled (bound)");
		PICOBENCH_REG(BM_QueryMatch_Variable_AllOnlyCoupled_Unbound)
				.PICO_SETTINGS_HEAVY()
				.user_data(128)
				.label("match all-only coupled (unbound)");
		PICOBENCH_REG(BM_QueryMatch_Variable_GenericSourceBacktrack_Bound)
				.PICO_SETTINGS_HEAVY()
				.user_data(128)
				.label("match generic source-backtrack (bound)");
		PICOBENCH_REG(BM_QueryMatch_Variable_GenericSourceBacktrack_Unbound)
				.PICO_SETTINGS_HEAVY()
				.user_data(128)
				.label("match generic source-backtrack (unbound)");
		PICOBENCH_REG(BM_QueryMatch_Variable_1VarOr_Bound)
				.PICO_SETTINGS_HEAVY()
				.user_data(128)
				.label("match 1var or-source-gated (bound)");
		PICOBENCH_REG(BM_QueryMatch_Variable_1VarOr_Unbound)
				.PICO_SETTINGS_HEAVY()
				.user_data(128)
				.label("match 1var or-source-gated (unbound)");
		PICOBENCH_REG(BM_QueryMatch_Variable_1VarOrDown_Bound)
				.PICO_SETTINGS_HEAVY()
				.user_data(128)
				.label("match 1var or-down-source (bound)");
		PICOBENCH_REG(BM_QueryMatch_Variable_1VarOrDown_Unbound)
				.PICO_SETTINGS_HEAVY()
				.user_data(128)
				.label("match 1var or-down-source (unbound)");
		PICOBENCH_REG(BM_QueryMatch_Variable_1VarOrUpDown_Bound)
				.PICO_SETTINGS_HEAVY()
				.user_data(128)
				.label("match 1var or-updown-source (bound)");
		PICOBENCH_REG(BM_QueryMatch_Variable_1VarOrUpDown_Unbound)
				.PICO_SETTINGS_HEAVY()
				.user_data(128)
				.label("match 1var or-updown-source (unbound)");
		PICOBENCH_REG(BM_QueryMatch_Variable_1VarAny_Bound)
				.PICO_SETTINGS_HEAVY()
				.user_data(128)
				.label("match 1var any (bound)");
		PICOBENCH_REG(BM_QueryMatch_Variable_1VarAny_Unbound)
				.PICO_SETTINGS_HEAVY()
				.user_data(128)
				.label("match 1var any (unbound)");
		PICOBENCH_REG(BM_QueryMatch_Variable_1VarMixed_Bound)
				.PICO_SETTINGS_HEAVY()
				.user_data(128)
				.label("match 1var pair-mixed (bound)");
		PICOBENCH_REG(BM_QueryMatch_Variable_1VarMixed_Unbound)
				.PICO_SETTINGS_HEAVY()
				.user_data(128)
				.label("match 1var pair-mixed (unbound)");

		PICOBENCH_SUITE_REG("Query variable focus");
		PICOBENCH_REG(BM_QueryMatch_Variable_1VarMixed_Bound)
				.PICO_SETTINGS_FOCUS()
				.user_data(128)
				.label("1var pair-mixed (bound)");
		PICOBENCH_REG(BM_QueryMatch_Variable_1VarMixed_Unbound)
				.PICO_SETTINGS_FOCUS()
				.user_data(128)
				.label("1var pair-mixed (unbound)");

#if GAIA_OBSERVERS_ENABLED
		PICOBENCH_SUITE_REG("Observers");
		PICOBENCH_REG(BM_Observer_OnAdd_0_1).PICO_SETTINGS_OBS().user_data(NObserverEntities).label("on_add, 0 obs");
		PICOBENCH_REG(BM_Observer_OnAdd_1_1).PICO_SETTINGS_OBS().user_data(NObserverEntities).label("on_add, 1 obs");
		PICOBENCH_REG(BM_Observer_OnAdd_10_1).PICO_SETTINGS_OBS().user_data(NObserverEntities).label("on_add, 10 obs");
		PICOBENCH_REG(BM_Observer_OnAdd_50_1).PICO_SETTINGS_OBS().user_data(NObserverEntities).label("on_add, 50 obs");
		PICOBENCH_REG(BM_Observer_OnAdd_50_2)
				.PICO_SETTINGS_OBS()
				.user_data(NObserverEntities)
				.label("on_add, 50 obs, 2 terms");
		PICOBENCH_REG(BM_Observer_OnAdd_50_4)
				.PICO_SETTINGS_OBS()
				.user_data(NObserverEntities)
				.label("on_add, 50 obs, 4 terms");
		PICOBENCH_REG(BM_Observer_OnAdd_50_8)
				.PICO_SETTINGS_OBS()
				.user_data(NObserverEntities)
				.label("on_add, 50 obs, 8 terms");
		PICOBENCH_REG(BM_Observer_OnDel_0_1).PICO_SETTINGS_OBS().user_data(NObserverEntities).label("on_del, 0 obs");
		PICOBENCH_REG(BM_Observer_OnDel_1_1).PICO_SETTINGS_OBS().user_data(NObserverEntities).label("on_del, 1 obs");
		PICOBENCH_REG(BM_Observer_OnDel_10_1).PICO_SETTINGS_OBS().user_data(NObserverEntities).label("on_del, 10 obs");
		PICOBENCH_REG(BM_Observer_OnDel_50_1).PICO_SETTINGS_OBS().user_data(NObserverEntities).label("on_del, 50 obs");
		PICOBENCH_REG(BM_Observer_OnDel_50_2)
				.PICO_SETTINGS_OBS()
				.user_data(NObserverEntities)
				.label("on_del, 50 obs, 2 terms");
		PICOBENCH_REG(BM_Observer_OnDel_50_4)
				.PICO_SETTINGS_OBS()
				.user_data(NObserverEntities)
				.label("on_del, 50 obs, 4 terms");
		PICOBENCH_REG(BM_Observer_OnDel_50_8)
				.PICO_SETTINGS_OBS()
				.user_data(NObserverEntities)
				.label("on_del, 50 obs, 8 terms");
#endif

		PICOBENCH_SUITE_REG("Systems (single-thread)");
		PICOBENCH_REG(BM_SystemFrame_Serial_2).PICO_SETTINGS().user_data(NEntitiesMedium).label("serial, 2 systems");
		PICOBENCH_REG(BM_SystemFrame_Serial_5).PICO_SETTINGS().user_data(NEntitiesMedium).label("serial, 5 systems");

		PICOBENCH_SUITE_REG("Mixed frame");
		PICOBENCH_REG(BM_MixedFrame_Churn).PICO_SETTINGS().user_data(NEntitiesFew).label("10K");
		PICOBENCH_REG(BM_MixedFrame_Churn).PICO_SETTINGS_HEAVY().user_data(NEntitiesMedium).label("100K");
	}

	r.parse_cmd_line((int)picobenchArgs.size(), picobenchArgs.data());

	// If picobench encounters an unknown command line argument it returns false and sets an error.
	// Ignore this behavior.
	if (r.error() == picobench::error_unknown_cmd_line_argument)
		r.set_error(picobench::no_error);

	return r.run(0);
}
