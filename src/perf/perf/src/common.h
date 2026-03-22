#pragma once

#include <picobench/picobench.hpp>
#include <string_view>

#define GAIA_ENABLE_HOOKS 1
#define GAIA_ENABLE_SET_HOOKS 1
#define GAIA_ENABLE_ADD_DEL_HOOKS 1
#define GAIA_SYSTEMS_ENABLED 1
#define GAIA_OBSERVERS_ENABLED 1
#define GAIA_USE_SAFE_ENTITY 1
#include <gaia.h>

using namespace gaia;

struct Position {
	float x;
	float y;
	float z;
};

struct PositionSparse {
	GAIA_STORAGE(Sparse);
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

struct ObsA {};
struct ObsB {};
struct ObsC {};
struct ObsD {};
struct ObsE {};
struct ObsF {};
struct ObsG {};
struct ObsH {};

static constexpr uint32_t NEntitiesFew = 10'000;
static constexpr uint32_t NEntitiesMedium = 100'000;
static constexpr uint32_t NEntitiesMany = 1'000'000;
static constexpr uint32_t NObserverEntities = 10'000;

static constexpr float DeltaTime = 0.016f;

////////////////////////////////////////////////////////////////////////////////
// Data setup helpers
////////////////////////////////////////////////////////////////////////////////

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

inline void create_fragmented_entities(ecs::World& w, uint32_t archetypes, uint32_t entitiesPerArchetype) {
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

inline void create_sorted_exact_entities(
		ecs::World& w, cnt::darray<ecs::Entity>& entities, uint32_t archetypes, uint32_t totalCount) {
	GAIA_ASSERT(archetypes > 0U);

	entities.clear();
	entities.reserve(totalCount);

	const uint32_t entitiesPerArchetype = totalCount / archetypes;
	const uint32_t remainder = totalCount % archetypes;

	GAIA_FOR(archetypes) {
		const uint32_t mask = i;
		const uint32_t count = entitiesPerArchetype + (i < remainder ? 1U : 0U);
		if (count == 0U)
			continue;

		auto e = w.add();
		auto b = w.build(e);
		b.add<Position>().add<Health>();

		if ((mask & 1U) != 0U)
			b.add<Velocity>();
		if ((mask & 2U) != 0U)
			b.add<Acceleration>();
		if ((mask & 4U) != 0U)
			b.add<Damage>();
		if ((mask & 8U) != 0U)
			b.add<Team>();
		if ((mask & 16U) != 0U)
			b.add<Frozen>();
		if ((mask & 32U) != 0U)
			b.add<Dirty>();

		b.commit();

		w.set<Position>(e) = {(float)(totalCount - entities.size()), (float)(mask % 41U), 0.0f};
		w.set<Health>(e) = {100, 100};

		if ((mask & 1U) != 0U)
			w.set<Velocity>(e) = {1.0f, 0.1f, 0.05f};
		if ((mask & 2U) != 0U)
			w.set<Acceleration>(e) = {0.0f, -0.01f, 0.0f};
		if ((mask & 4U) != 0U)
			w.set<Damage>(e) = {(int32_t)(mask % 9U)};
		if ((mask & 8U) != 0U)
			w.set<Team>(e) = {mask % 4U};
		if ((mask & 16U) != 0U)
			w.set<Frozen>(e) = {(mask % 3U) == 0U};
		if ((mask & 32U) != 0U)
			w.set<Dirty>(e) = {(mask % 2U) == 0U};

		entities.push_back(e);
		if (count > 1U) {
			w.copy_n(e, count - 1U, [&](ecs::Entity entity) {
				entities.push_back(entity);
				auto& pos = w.set<Position>(entity);
				pos.x = (float)(totalCount - entities.size() + 1U);
				pos.y = (float)(mask % 41U);
			});
		}
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

inline void init_systems(ecs::World& w, uint32_t systems, ecs::QueryExecType mode) {
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
