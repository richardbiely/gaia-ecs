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

template <bool BoundVar0>
void BM_QueryMatch_Variable_GenericOr(picobench::state& state) {
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
	// Single valid source target forces unbound OR backtracking in the generic solver.
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

void BM_QueryMatch_Variable_GenericOr_Bound(picobench::state& state) {
	BM_QueryMatch_Variable_GenericOr<true>(state);
}

void BM_QueryMatch_Variable_GenericOr_Unbound(picobench::state& state) {
	BM_QueryMatch_Variable_GenericOr<false>(state);
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

		PICOBENCH_SUITE_REG("Fragmented archetypes");
		PICOBENCH_REG(BM_Fragmented_Read).PICO_SETTINGS().label("read");
		PICOBENCH_REG(BM_Fragmented_Write).PICO_SETTINGS().label("write");

		PICOBENCH_SUITE_REG("Query build");
		PICOBENCH_REG(BM_QueryBuild_Cached_2).PICO_SETTINGS_HEAVY().label("cached, 2 comp");
		PICOBENCH_REG(BM_QueryBuild_Uncached_2).PICO_SETTINGS_HEAVY().label("uncached, 2 comp");
		PICOBENCH_REG(BM_QueryBuild_Cached_4).PICO_SETTINGS_HEAVY().label("cached, 4 comp");
		PICOBENCH_REG(BM_QueryBuild_Uncached_4).PICO_SETTINGS_HEAVY().label("uncached, 4 comp");

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
		PICOBENCH_REG(BM_QueryMatch_Variable_AllOnly_Bound)
				.PICO_SETTINGS_HEAVY()
				.user_data(128)
				.label("match all-only 2var (bound)");
		PICOBENCH_REG(BM_QueryMatch_Variable_AllOnly_Unbound)
				.PICO_SETTINGS_HEAVY()
				.user_data(128)
				.label("match all-only 2var (unbound)");
		PICOBENCH_REG(BM_QueryMatch_Variable_GenericOr_Bound)
				.PICO_SETTINGS_HEAVY()
				.user_data(128)
				.label("match generic or (bound)");
		PICOBENCH_REG(BM_QueryMatch_Variable_GenericOr_Unbound)
				.PICO_SETTINGS_HEAVY()
				.user_data(128)
				.label("match generic or (unbound)");

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
