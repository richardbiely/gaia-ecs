#include "common.h"
#include "registry.h"

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

void BM_Query_SelectiveAll_BroadFirst(picobench::state& state) {
	const uint32_t archetypeCnt = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> tags;
	tags.resize(archetypeCnt);
	GAIA_FOR(archetypeCnt) {
		tags[i] = w.add();
	}

	GAIA_FOR(archetypeCnt) {
		auto e = w.add();
		w.add(e, tags[i]);
		w.add<Position>(e, {(float)i, (float)(i % 97U), 0.0f});
		if ((i & 31U) == 0U)
			w.add<Health>(e, {(int32_t)i});
	}

	auto q = w.query().all<Position>().all<Health>();
	dont_optimize(q.count());

	for (auto _: state) {
		(void)_;
		dont_optimize(q.count());
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

//! Builds many distinct immediate cached structural queries, all of which should match the same new archetype.
template <uint32_t TermsPerQuery = 1>
void create_structural_cache_queries(
		ecs::World& w, cnt::darray<ecs::Entity>& tags, cnt::darray<ecs::Query>& queries, uint32_t queryCnt) {
	static_assert(TermsPerQuery > 0);

	tags.clear();
	queries.clear();
	tags.reserve(queryCnt * TermsPerQuery);
	queries.reserve(queryCnt);

	GAIA_FOR(queryCnt * TermsPerQuery) {
		tags.push_back(w.add());
	}

	GAIA_FOR(queryCnt) {
		auto q = w.query();
		GAIA_FOR_(TermsPerQuery, j) {
			const auto tag = tags[i * TermsPerQuery + j];
			q.all(tag);
		}
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

template <uint32_t TermsPerQuery, uint32_t QueryCnt>
void BM_QueryCache_Create_Fanout_Multi(picobench::state& state) {
	const uint32_t archetypeCnt = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;
		state.stop_timer();

		ecs::World w;
		cnt::darray<ecs::Entity> tags;
		cnt::darray<ecs::Query> queries;
		create_structural_cache_queries<TermsPerQuery>(w, tags, queries, QueryCnt);
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

void BM_QueryCache_Create_Fanout_15q_2t(picobench::state& state) {
	BM_QueryCache_Create_Fanout_Multi<2, 15>(state);
}

void BM_QueryCache_Create_Fanout_7q_4t(picobench::state& state) {
	BM_QueryCache_Create_Fanout_Multi<4, 7>(state);
}

void BM_QueryCache_Create_Fanout_3q_8t(picobench::state& state) {
	BM_QueryCache_Create_Fanout_Multi<8, 3>(state);
}

//! Benchmarks batched builder archetype resolution where only the final archetype matters.
void BM_EntityBuilder_BatchAdd_4(picobench::state& state) {
	for (auto _: state) {
		(void)_;
		state.stop_timer();

		ecs::World w;
		const auto tagA = w.add();
		const auto tagB = w.add();
		const auto tagC = w.add();
		const auto tagD = w.add();

		state.start_timer();

		auto e = w.add();
		auto builder = w.build(e);
		builder.add(tagA).add(tagB).add(tagC).add(tagD);
		builder.commit();

		state.stop_timer();
		dont_optimize(w.has(e, tagD));
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

//! Benchmarks repeated probe reads across many identical cached changed() queries kept alive at once.
//! count() is intentionally non-consuming for changed() queries.
void BM_QueryCache_ChangedProbe_IdenticalCached(picobench::state& state) {
	const uint32_t queryCnt = (uint32_t)state.user_data();

	ecs::World w;
	const auto e = w.add();
	w.add<Position>(e, {1, 2, 3});

	cnt::darray<ecs::Query> queries;
	queries.reserve(queryCnt);
	GAIA_FOR(queryCnt) queries.push_back(w.query().all<Position>().changed<Position>());

	uint64_t total = 0;
	for (auto _: state) {
		(void)_;
		uint64_t sum = 0;
		for (auto& q: queries)
			sum += q.count();
		total += sum;
	}

	dont_optimize(total);
}

//! Benchmarks consuming iteration across many identical cached changed() queries kept alive at once.
//! each() is consuming for changed() queries, so the setup is rebuilt each sample.
void BM_QueryCache_ChangedConsume_IdenticalCached(picobench::state& state) {
	const uint32_t queryCnt = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;
		state.stop_timer();

		ecs::World w;
		const auto e = w.add();
		w.add<Position>(e, {1, 2, 3});

		cnt::darray<ecs::Query> queries;
		queries.reserve(queryCnt);
		GAIA_FOR(queryCnt) queries.push_back(w.query().all<Position>().changed<Position>());

		uint64_t total = 0;
		state.start_timer();

		for (auto& q: queries) {
			q.each([&](const Position& p) {
				total += (uint64_t)(p.x + p.y + p.z);
			});
		}

		state.stop_timer();
		dont_optimize(total);
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

//! Benchmarks steady-state warm reads for a cached sorted query spanning many matching archetypes.
//! This isolates the exact sortBy remap path that now uses the component index for exact sort terms.
void BM_QueryCache_Sorted_ExactMergeWarmRead(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_sorted_exact_entities(w, entities, 32U, n);

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

//! Benchmarks steady-state warm reads for a cached sorted query whose exact sort term is not part of the query terms.
//! This isolates the cached sortBy column remap path without relying on normal query-term remapping.
void BM_QueryCache_Sorted_ExactExternalMergeWarmRead(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_sorted_exact_entities(w, entities, 32U, n);

	auto q = w.query().all<Health>().sort_by(
			w.get<Position>(), []([[maybe_unused]] const ecs::World& world, const void* pData0, const void* pData1) {
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

//! Benchmarks uncached matching of queries that depend on transitive `Is` traversal.
//! This isolates the inheritance walk used by the query matcher.
template <uint32_t ChainDepth>
void BM_QueryMatch_IsChain(picobench::state& state) {
	const uint32_t archetypeCnt = (uint32_t)state.user_data();

	ecs::World w;

	cnt::sarray<ecs::Entity, ChainDepth> matchingBases{};
	cnt::sarray<ecs::Entity, ChainDepth> nonMatchingBases{};
	GAIA_FOR(ChainDepth) {
		matchingBases[i] = w.add();
		nonMatchingBases[i] = w.add();
		if (i == 0)
			continue;

		w.add(matchingBases[i], ecs::Pair(ecs::Is, matchingBases[i - 1]));
		w.add(nonMatchingBases[i], ecs::Pair(ecs::Is, nonMatchingBases[i - 1]));
	}

	cnt::darray<ecs::Entity> tags;
	tags.resize(archetypeCnt);
	GAIA_FOR(archetypeCnt) {
		tags[i] = w.add();
	}

	GAIA_FOR(archetypeCnt) {
		auto e = w.add();
		w.add<Position>(e, {(float)i, (float)(i % 97U), 0.0f});
		w.add(e, tags[i]);
		w.add(e, ecs::Pair(ecs::Is, (i & 1U) == 0U ? matchingBases[ChainDepth - 1] : nonMatchingBases[ChainDepth - 1]));
	}

	auto q = w.query<false>().all<Position>().all(ecs::Pair(ecs::Is, matchingBases[0]));
	auto& qi = q.fetch();
	q.match_all(qi);
	dont_optimize(qi.cache_archetype_view().size());

	for (auto _: state) {
		(void)_;
		qi.reset();
		q.match_all(qi);
		dont_optimize(qi.cache_archetype_view().size());
	}
}

void BM_QueryMatch_IsChain_2(picobench::state& state) {
	BM_QueryMatch_IsChain<2>(state);
}

void BM_QueryMatch_IsChain_4(picobench::state& state) {
	BM_QueryMatch_IsChain<4>(state);
}

void BM_QueryMatch_IsChain_8(picobench::state& state) {
	BM_QueryMatch_IsChain<8>(state);
}

void BM_QueryMatch_IsChain_32(picobench::state& state) {
	BM_QueryMatch_IsChain<32>(state);
}

//! Benchmarks uncached matching of an exact owned term across many archetypes.
//! This exercises the component-index exact-term lookup path in the matcher.
void BM_QueryMatch_ExactTerm(picobench::state& state) {
	const uint32_t archetypeCnt = (uint32_t)state.user_data();

	ecs::World w;
	const auto tag = w.add();

	cnt::darray<ecs::Entity> tags;
	tags.resize(archetypeCnt);
	GAIA_FOR(archetypeCnt) {
		tags[i] = w.add();
	}

	GAIA_FOR(archetypeCnt) {
		auto e = w.add();
		w.add<Position>(e, {(float)i, (float)(i % 97U), 0.0f});
		w.add(e, tags[i]);
		if ((i & 1U) == 0U)
			w.add(e, tag);
	}

	auto q = w.query<false>().all<Position>().all(tag);
	auto& qi = q.fetch();
	q.match_all(qi);
	dont_optimize(qi.cache_archetype_view().size());

	for (auto _: state) {
		(void)_;
		qi.reset();
		q.match_all(qi);
		dont_optimize(qi.cache_archetype_view().size());
	}
}

//! Benchmarks immediate cached-query refresh as new exact/wildcard-matching archetypes are created.
//! This isolates QueryInfo::register_archetype for the direct create-time path backed by the component index.
void BM_QueryCache_CreateArchetype_ExactWildcard(picobench::state& state) {
	const uint32_t archetypeCnt = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;

		ecs::World w;
		const auto rel = w.add();
		const auto tgt = w.add();

		cnt::darray<ecs::Entity> tags;
		tags.resize(archetypeCnt);
		GAIA_FOR(archetypeCnt) {
			tags[i] = w.add();
		}

		auto q = w.query().all<Position>().all(ecs::Pair(rel, ecs::All));
		dont_optimize(q.count());

		GAIA_FOR(archetypeCnt) {
			auto e = w.add();
			w.add(e, tags[i]);
			w.add(e, ecs::Pair(rel, tgt));
			w.add<Position>(e, {(float)i, (float)(i % 97U), 0.0f});
		}

		dont_optimize(q.count());
	}
}

//! Benchmarks create-time routing misses for a mixed exact + wildcard ALL query compiled on an empty world.
//! Selector tie-breaking should prefer the wildcard pair selector over the broad exact term in this case.
void BM_QueryCache_CreateArchetype_ExactWildcard_Miss(picobench::state& state) {
	const uint32_t archetypeCnt = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;

		ecs::World w;
		const auto rel = w.add();

		cnt::darray<ecs::Entity> tags;
		tags.resize(archetypeCnt);
		GAIA_FOR(archetypeCnt) {
			tags[i] = w.add();
		}

		auto q = w.query().all<Position>().all(ecs::Pair(rel, ecs::All));
		dont_optimize(q.count());

		GAIA_FOR(archetypeCnt) {
			auto e = w.add();
			w.add(e, tags[i]);
			w.add<Position>(e, {(float)i, (float)(i % 97U), 0.0f});
		}

		dont_optimize(q.count());
	}
}

//! Benchmarks immediate cached-query refresh for mixed exact ALL+NOT queries as matching archetypes are created.
//! This isolates the direct create-time path without going through the one-archetype VM matcher.
void BM_QueryCache_CreateArchetype_ExactNot(picobench::state& state) {
	const uint32_t archetypeCnt = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;

		ecs::World w;
		cnt::darray<ecs::Entity> tags;
		tags.resize(archetypeCnt);
		GAIA_FOR(archetypeCnt) {
			tags[i] = w.add();
		}

		auto q = w.query().all<Position>().no<Acceleration>();
		dont_optimize(q.count());

		GAIA_FOR(archetypeCnt) {
			auto e = w.add();
			w.add(e, tags[i]);
			w.add<Position>(e, {(float)i, (float)(i % 97U), 0.0f});
		}

		dont_optimize(q.count());
	}
}

//! Benchmarks immediate cached-query refresh for mixed exact ALL+OR queries as matching archetypes are created.
//! This isolates the direct create-time structural matcher on a common positive-selector shape.
void BM_QueryCache_CreateArchetype_ExactOr(picobench::state& state) {
	const uint32_t archetypeCnt = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;

		ecs::World w;
		cnt::darray<ecs::Entity> tags;
		tags.resize(archetypeCnt);
		GAIA_FOR(archetypeCnt) {
			tags[i] = w.add();
		}

		auto q = w.query().all<Position>().or_<Acceleration>().or_<Health>();
		dont_optimize(q.count());

		GAIA_FOR(archetypeCnt) {
			auto e = w.add();
			w.add(e, tags[i]);
			w.add<Position>(e, {(float)i, (float)(i % 97U), 0.0f});
			w.add<Acceleration>(e, {(float)(i % 13U), 1.0f, 0.0f});
		}

		dont_optimize(q.count());
	}
}

//! Benchmarks immediate cached-query refresh for mixed exact ALL+ANY queries as archetypes are created.
//! ANY terms are not hard requirements, so this measures the direct structural path skipping them.
void BM_QueryCache_CreateArchetype_ExactAny(picobench::state& state) {
	const uint32_t archetypeCnt = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;

		ecs::World w;
		cnt::darray<ecs::Entity> tags;
		tags.resize(archetypeCnt);
		GAIA_FOR(archetypeCnt) {
			tags[i] = w.add();
		}

		auto q = w.query().all<Position>().any<Acceleration>();
		dont_optimize(q.count());

		GAIA_FOR(archetypeCnt) {
			auto e = w.add();
			w.add(e, tags[i]);
			w.add<Position>(e, {(float)i, (float)(i % 97U), 0.0f});
		}

		dont_optimize(q.count());
	}
}

//! Benchmarks immediate cached-query refresh for a broad-first exact ALL query.
//! The create-selector planner should route this through the required selective term instead of every positive term.
void BM_QueryCache_CreateArchetype_BroadFirstAll(picobench::state& state) {
	const uint32_t archetypeCnt = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;

		ecs::World w;
		cnt::darray<ecs::Entity> tags;
		tags.resize(archetypeCnt);
		GAIA_FOR(archetypeCnt) {
			tags[i] = w.add();
		}

		// Pre-seed a broad Position distribution before the query is built so selector planning can
		// choose the rarer required Health term for future archetype-create routing.
		GAIA_FOR(archetypeCnt) {
			auto e = w.add();
			w.add(e, tags[i]);
			w.add<Position>(e, {(float)i, (float)(i % 97U), 0.0f});
		}

		auto q = w.query().all<Position>().all<Health>();
		dont_optimize(q.count());

		GAIA_FOR(archetypeCnt) {
			auto e = w.add();
			w.add(e, w.add());
			w.add<Position>(e, {(float)i, (float)(i % 97U), 0.0f});
			w.add<Health>(e, {(int32_t)i, 100});
		}

		dont_optimize(q.count());
	}
}

//! Benchmarks create-time routing for non-matching broad archetypes under a cached broad-first ALL query.
//! With narrowest-selector routing, Position-only archetype creation should not wake the query at all.
void BM_QueryCache_CreateArchetype_BroadMissAll(picobench::state& state) {
	const uint32_t archetypeCnt = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;

		ecs::World w;
		cnt::darray<ecs::Entity> seedTags;
		cnt::darray<ecs::Entity> newTags;
		seedTags.resize(archetypeCnt);
		newTags.resize(archetypeCnt);
		GAIA_FOR(archetypeCnt) {
			seedTags[i] = w.add();
			newTags[i] = w.add();
		}

		GAIA_FOR(archetypeCnt) {
			auto e = w.add();
			w.add(e, seedTags[i]);
			w.add<Position>(e, {(float)i, (float)(i % 97U), 0.0f});
		}

		auto q = w.query().all<Position>().all<Acceleration>();
		dont_optimize(q.count());

		GAIA_FOR(archetypeCnt) {
			auto e = w.add();
			w.add(e, newTags[i]);
			w.add<Position>(e, {(float)i, (float)(i % 97U), 0.0f});
		}

		dont_optimize(q.count());
	}
}

//! Benchmarks create-time routing for pair-heavy archetypes under a cached relation-wildcard query.
//! This isolates duplicate `(rel, All)` lookup suppression in QueryCache::register_archetype_with_queries().
void BM_QueryCache_CreateArchetype_PairHeavyRelWildcard(picobench::state& state) {
	const uint32_t archetypeCnt = (uint32_t)state.user_data();
	static constexpr uint32_t PairCount = 30;

	for (auto _: state) {
		(void)_;

		ecs::World w;
		const auto rel = w.add();

		cnt::darray<ecs::Entity> targets;
		targets.resize(PairCount);
		GAIA_FOR(PairCount) {
			targets[i] = w.add();
		}

		auto q = w.query().all(ecs::Pair(rel, ecs::All));
		dont_optimize(q.count());

		GAIA_FOR(archetypeCnt) {
			auto e = w.add();
			{
				auto b = w.build(e);
				b.add(w.add());
				GAIA_FOR2_(0, PairCount, pairIdx) {
					b.add(ecs::Pair(rel, targets[pairIdx]));
				}
			}
		}

		dont_optimize(q.count());
	}
}

template <uint32_t ChainDepth>
ecs::Entity create_is_fanout_fixture(ecs::World& w, uint32_t branches, bool attachPositionToLeavesOnly) {
	const auto root = w.add();
	GAIA_FOR(branches) {
		auto curr = root;
		for (uint32_t j = 0; j < ChainDepth; ++j) {
			const auto next = w.add();
			w.add(next, ecs::Pair(ecs::Is, curr));
			if (!attachPositionToLeavesOnly || j + 1U == ChainDepth)
				w.add<Position>(next, {(float)i, (float)j, (float)(i + j)});
			curr = next;
		}
	}
	return root;
}

template <uint32_t ChainDepth>
ecs::Entity create_is_fanout_fixture(
		ecs::World& w, uint32_t branches, bool attachPositionToLeavesOnly, cnt::darray<ecs::Entity>& leaves) {
	leaves.clear();
	leaves.reserve(branches);

	const auto root = w.add();
	GAIA_FOR(branches) {
		auto curr = root;
		for (uint32_t j = 0; j < ChainDepth; ++j) {
			const auto next = w.add();
			w.add(next, ecs::Pair(ecs::Is, curr));
			if (!attachPositionToLeavesOnly || j + 1U == ChainDepth)
				w.add<Position>(next, {(float)i, (float)j, (float)(i + j)});
			curr = next;
		}
		leaves.push_back(curr);
	}
	return root;
}

template <uint32_t ChainDepth, bool Direct>
void BM_Query_IsEach(picobench::state& state) {
	const uint32_t branches = (uint32_t)state.user_data();

	ecs::World w;
	const auto root = create_is_fanout_fixture<ChainDepth>(w, branches, false);
	auto q = Direct ? w.query().all<Position>().is(root, ecs::QueryTermOptions{}.direct())
									: w.query().all<Position>().is(root);
	dont_optimize(q.empty());

	for (auto _: state) {
		(void)_;
		uint64_t sum = 0;
		q.each([&](const Position& p) {
			sum += (uint64_t)(p.x + p.y + p.z);
		});
		dont_optimize(sum);
	}
}

ecs::Entity create_prefab_inherit_fixture(ecs::World& w, uint32_t count) {
	const auto position = w.add<Position>().entity;
	w.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto prefab = w.prefab();
	w.add<Position>(prefab, {1.0f, 2.0f, 3.0f});
	w.instantiate_n(prefab, count);
	return prefab;
}

void BM_Query_PrefabInherited_Read_Each(picobench::state& state) {
	const uint32_t count = (uint32_t)state.user_data();

	ecs::World w;
	const auto prefab = create_prefab_inherit_fixture(w, count);
	auto q = w.query().all<Position>().is(prefab);
	dont_optimize(q.empty());

	for (auto _: state) {
		(void)_;
		uint64_t sum = 0;
		q.each([&](const Position& p) {
			sum += (uint64_t)(p.x + p.y + p.z);
		});
		dont_optimize(sum);
	}
}

void BM_Query_PrefabInherited_Read_Iter(picobench::state& state) {
	const uint32_t count = (uint32_t)state.user_data();

	ecs::World w;
	const auto prefab = create_prefab_inherit_fixture(w, count);
	auto q = w.query().all<Position>().is(prefab);
	dont_optimize(q.empty());

	for (auto _: state) {
		(void)_;
		uint64_t sum = 0;
		q.each([&](ecs::Iter& it) {
			auto posView = it.view<Position>(1);
			GAIA_EACH(it) {
				sum += (uint64_t)(posView[i].x + posView[i].y + posView[i].z);
			}
		});
		dont_optimize(sum);
	}
}

void BM_Query_PrefabInherited_Write_Each(picobench::state& state) {
	const uint32_t count = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;
		state.stop_timer();
		ecs::World w;
		const auto prefab = create_prefab_inherit_fixture(w, count);
		auto q = w.query().all<Position&>().is(prefab);
		state.start_timer();

		float sum = 0.0f;
		q.each([&](Position& p) {
			p.x += 1.0f;
			sum += p.x;
		});
		dont_optimize(sum);

		state.stop_timer();
	}
}

template <uint32_t ChainDepth, bool Direct>
void BM_Query_IsEachIter(picobench::state& state) {
	const uint32_t branches = (uint32_t)state.user_data();

	ecs::World w;
	const auto root = create_is_fanout_fixture<ChainDepth>(w, branches, false);
	auto q = Direct ? w.query().all<Position>().is(root, ecs::QueryTermOptions{}.direct())
									: w.query().all<Position>().is(root);
	dont_optimize(q.empty());

	for (auto _: state) {
		(void)_;
		uint64_t sum = 0;
		q.each([&](ecs::Iter& it) {
			auto posView = it.view<Position>();
			GAIA_EACH(it) {
				sum += (uint64_t)(posView[i].x + posView[i].y + posView[i].z);
			}
		});
		dont_optimize(sum);
	}
}

template <uint32_t ChainDepth, bool Direct>
void BM_System_Is(picobench::state& state) {
	const uint32_t branches = (uint32_t)state.user_data();

	ecs::World w;
	const auto root = create_is_fanout_fixture<ChainDepth>(w, branches, false);
	uint64_t sink = 0;

	if constexpr (Direct) {
		w.system()
				.name("is_direct")
				.all<Position>()
				.is(root, ecs::QueryTermOptions{}.direct())
				.mode(ecs::QueryExecType::Default)
				.on_each([&sink](const Position& p) {
					sink += (uint64_t)(p.x + p.y + p.z);
				});
	} else {
		w.system()
				.name("is_semantic")
				.all<Position>()
				.is(root)
				.mode(ecs::QueryExecType::Default)
				.on_each([&sink](const Position& p) {
					sink += (uint64_t)(p.x + p.y + p.z);
				});
	}

	for (uint32_t i = 0; i < 4; ++i)
		w.update();

	for (auto _: state) {
		(void)_;
		w.update();
	}

	dont_optimize(sink);
}

template <uint32_t ChainDepth, bool Direct>
void BM_System_IsIter(picobench::state& state) {
	const uint32_t branches = (uint32_t)state.user_data();

	ecs::World w;
	const auto root = create_is_fanout_fixture<ChainDepth>(w, branches, false);
	uint64_t sink = 0;

	if constexpr (Direct) {
		w.system()
				.name("is_direct_iter")
				.all<Position>()
				.is(root, ecs::QueryTermOptions{}.direct())
				.mode(ecs::QueryExecType::Default)
				.on_each([&sink](ecs::Iter& it) {
					auto posView = it.view<Position>();
					GAIA_EACH(it) {
						sink += (uint64_t)(posView[i].x + posView[i].y + posView[i].z);
					}
				});
	} else {
		w.system()
				.name("is_semantic_iter")
				.all<Position>()
				.is(root)
				.mode(ecs::QueryExecType::Default)
				.on_each([&sink](ecs::Iter& it) {
					auto posView = it.view<Position>();
					GAIA_EACH(it) {
						sink += (uint64_t)(posView[i].x + posView[i].y + posView[i].z);
					}
				});
	}

	for (uint32_t i = 0; i < 4; ++i)
		w.update();

	for (auto _: state) {
		(void)_;
		w.update();
	}

	dont_optimize(sink);
}

void BM_Query_IsEach_Semantic_D2(picobench::state& state) {
	BM_Query_IsEach<2, false>(state);
}

void BM_Query_IsEach_Direct_D2(picobench::state& state) {
	BM_Query_IsEach<2, true>(state);
}

void BM_Query_IsEach_Semantic_D8(picobench::state& state) {
	BM_Query_IsEach<8, false>(state);
}

void BM_Query_IsEach_Direct_D8(picobench::state& state) {
	BM_Query_IsEach<8, true>(state);
}

void BM_Query_IsEachIter_Semantic_D8(picobench::state& state) {
	BM_Query_IsEachIter<8, false>(state);
}

void BM_Query_IsEachIter_Direct_D8(picobench::state& state) {
	BM_Query_IsEachIter<8, true>(state);
}

void BM_System_Is_Semantic_D2(picobench::state& state) {
	BM_System_Is<2, false>(state);
}

void BM_System_Is_Direct_D2(picobench::state& state) {
	BM_System_Is<2, true>(state);
}

void BM_System_Is_Semantic_D8(picobench::state& state) {
	BM_System_Is<8, false>(state);
}

void BM_System_Is_Direct_D8(picobench::state& state) {
	BM_System_Is<8, true>(state);
}

void BM_System_IsIter_Semantic_D8(picobench::state& state) {
	BM_System_IsIter<8, false>(state);
}

void BM_System_IsIter_Direct_D8(picobench::state& state) {
	BM_System_IsIter<8, true>(state);
}

template <uint32_t ChainDepth, bool Direct>
void BM_Observer_IsMatchesAny(picobench::state& state) {
	const uint32_t branches = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> leaves;
	const auto root = create_is_fanout_fixture<ChainDepth>(w, branches, true, leaves);
	const auto observerEntity = Direct ? w.observer() //
																					 .event(ecs::ObserverEvent::OnAdd)
																					 .is(root, ecs::QueryTermOptions{}.direct())
																					 .on_each([](ecs::Iter&) {})
																					 .entity()
																		 : w.observer() //
																					 .event(ecs::ObserverEvent::OnAdd)
																					 .is(root)
																					 .on_each([](ecs::Iter&) {})
																					 .entity();

	auto& observerData = w.observers().data(observerEntity);
	auto& observerQueryInfo = observerData.query.fetch();
	dont_optimize(observerEntity);

	for (auto _: state) {
		(void)_;

		uint32_t hits = 0;
		for (const auto leaf: leaves) {
			const auto& ec = w.fetch(leaf);
			hits += (uint32_t)observerData.query.matches_any(observerQueryInfo, *ec.pArchetype, ecs::EntitySpan{&leaf, 1});
		}
		dont_optimize(hits);
	}
}

void BM_Observer_IsMatchesAny_Semantic_D2(picobench::state& state) {
	BM_Observer_IsMatchesAny<2, false>(state);
}

void BM_Observer_IsMatchesAny_Direct_D2(picobench::state& state) {
	BM_Observer_IsMatchesAny<2, true>(state);
}

void BM_Observer_IsMatchesAny_Semantic_D8(picobench::state& state) {
	BM_Observer_IsMatchesAny<8, false>(state);
}

void BM_Observer_IsMatchesAny_Direct_D8(picobench::state& state) {
	BM_Observer_IsMatchesAny<8, true>(state);
}

//! Benchmarks transitive target traversal over `Is` targets.
template <uint32_t ChainDepth>
void BM_World_AsTargetsTrav(picobench::state& state) {
	ecs::World w;

	cnt::sarray<ecs::Entity, ChainDepth> chain{};
	GAIA_FOR(ChainDepth) {
		chain[i] = w.add();
		if (i == 0)
			continue;
		w.add(chain[i], ecs::Pair(ecs::Is, chain[i - 1]));
	}

	for (auto _: state) {
		(void)_;
		uint64_t sum = 0;
		w.as_targets_trav(chain[ChainDepth - 1], [&](ecs::Entity target) {
			sum += target.id();
		});
		dont_optimize(sum);
	}
}

void BM_World_AsTargetsTrav_2(picobench::state& state) {
	BM_World_AsTargetsTrav<2>(state);
}

void BM_World_AsTargetsTrav_4(picobench::state& state) {
	BM_World_AsTargetsTrav<4>(state);
}

void BM_World_AsTargetsTrav_8(picobench::state& state) {
	BM_World_AsTargetsTrav<8>(state);
}

void BM_World_AsTargetsTrav_32(picobench::state& state) {
	BM_World_AsTargetsTrav<32>(state);
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

//! Benchmarks cached wildcard-pair query maintenance while building a pair-heavy archetype.
//! This exercises incremental query registration against archetypes that repeat the same relation
//! across many pair ids, which would otherwise create duplicate wildcard lookup keys.
void BM_QueryCache_RegisterPairHeavy_RelWildcard(picobench::state& state) {
	const uint32_t pairCnt = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;

		state.stop_timer();
		ecs::World w;
		const auto rel = w.add();
		cnt::darray<ecs::Entity> targets;
		targets.reserve(pairCnt);
		GAIA_FOR(pairCnt) {
			targets.push_back(w.add());
		}

		auto q = w.query().all(ecs::Pair(rel, ecs::All));
		dont_optimize(q.count());

		const auto e = w.add();
		state.start_timer();

		GAIA_FOR(pairCnt) {
			w.add(e, ecs::Pair(rel, targets[i]));
		}

		dont_optimize(e);
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

#define PICO_SETTINGS() iterations({256}).samples(3)
#define PICO_SETTINGS_HEAVY() iterations({64}).samples(3)
#define PICO_SETTINGS_FOCUS() iterations({256}).samples(7)
#define PICO_SETTINGS_OBS() iterations({64}).samples(3)
#define PICO_SETTINGS_SANI() iterations({8}).samples(1)
#define PICOBENCH_SUITE_REG(name) (void)picobench::global_registry::set_bench_suite(name)
#define PICOBENCH_REG(func) picobench::global_registry::new_benchmark(#func, func)

void register_query_hot_path(PerfRunMode mode) {
	switch (mode) {
		case PerfRunMode::Profiling:
			PICOBENCH_SUITE_REG("Profile picks");
			PICOBENCH_REG(BM_QueryCache_SourceTraversal_WarmRead_Snapshotted)
					.PICO_SETTINGS_HEAVY()
					.user_data(NEntitiesMedium)
					.label("query cache, traversed source snap");
			return;
		case PerfRunMode::Sanitizer:
			PICOBENCH_SUITE_REG("Sanitizer picks");
			PICOBENCH_REG(BM_Query_ReadWrite_2Comp).PICO_SETTINGS_SANI().user_data(NEntitiesFew).label("query rw");
			PICOBENCH_REG(BM_QueryCache_SourceTraversal_WarmRead_Snapshotted)
					.PICO_SETTINGS_SANI()
					.user_data(NEntitiesFew)
					.label("query cache, traversed source snap");
			return;
		case PerfRunMode::Normal:
			PICOBENCH_SUITE_REG("Query hot path");
			PICOBENCH_REG(BM_Query_ReadOnly_1Comp).PICO_SETTINGS().user_data(NEntitiesMedium).label("ro 1 comp");
			PICOBENCH_REG(BM_Query_SelectiveAll_BroadFirst)
					.PICO_SETTINGS_HEAVY()
					.user_data(1024)
					.label("query selective all broad-first 1K arch");
			PICOBENCH_REG(BM_Query_ReadWrite_2Comp).PICO_SETTINGS().user_data(NEntitiesMedium).label("rw 2 comp");
			PICOBENCH_REG(BM_Query_ReadWrite_4Comp).PICO_SETTINGS().user_data(NEntitiesMedium).label("rw 4 comp");
			PICOBENCH_REG(BM_Query_Filter_NoFrozen).PICO_SETTINGS().user_data(NEntitiesMedium).label("rw + no<Frozen>");
			PICOBENCH_REG(BM_QueryMatch_IsChain_2).PICO_SETTINGS_FOCUS().user_data(256).label("match is chain d2");
			PICOBENCH_REG(BM_QueryMatch_IsChain_4).PICO_SETTINGS_FOCUS().user_data(256).label("match is chain d4");
			PICOBENCH_REG(BM_QueryMatch_IsChain_8).PICO_SETTINGS_FOCUS().user_data(256).label("match is chain d8");
			PICOBENCH_REG(BM_QueryMatch_IsChain_32).PICO_SETTINGS_FOCUS().user_data(256).label("match is chain d32");
			PICOBENCH_REG(BM_Query_IsEach_Semantic_D2)
					.PICO_SETTINGS_FOCUS()
					.user_data(1024)
					.label("query is each semantic d2");
			PICOBENCH_REG(BM_Query_IsEach_Direct_D2).PICO_SETTINGS_FOCUS().user_data(1024).label("query is each direct d2");
			PICOBENCH_REG(BM_Query_IsEach_Semantic_D8)
					.PICO_SETTINGS_FOCUS()
					.user_data(1024)
					.label("query is each semantic d8");
			PICOBENCH_REG(BM_Query_IsEach_Direct_D8).PICO_SETTINGS_FOCUS().user_data(1024).label("query is each direct d8");
			PICOBENCH_REG(BM_Query_IsEachIter_Semantic_D8)
					.PICO_SETTINGS_FOCUS()
					.user_data(1024)
					.label("query is iter semantic d8");
			PICOBENCH_REG(BM_Query_IsEachIter_Direct_D8)
					.PICO_SETTINGS_FOCUS()
					.user_data(1024)
					.label("query is iter direct d8");
			PICOBENCH_REG(BM_Query_PrefabInherited_Read_Each)
					.PICO_SETTINGS_FOCUS()
					.user_data(NEntitiesMedium)
					.label("query prefab inherit read each 100K");
			PICOBENCH_REG(BM_Query_PrefabInherited_Read_Iter)
					.PICO_SETTINGS_FOCUS()
					.user_data(NEntitiesMedium)
					.label("query prefab inherit read iter 100K");
			PICOBENCH_REG(BM_Query_PrefabInherited_Write_Each)
					.PICO_SETTINGS_FOCUS()
					.user_data(NEntitiesMedium)
					.label("query prefab inherit write each 100K");
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
			PICOBENCH_REG(BM_QueryCache_Create_Fanout_15q_2t)
					.PICO_SETTINGS_FOCUS()
					.user_data(1024)
					.label("create fanout 15q 2t 1K arch");
			PICOBENCH_REG(BM_QueryCache_Create_Fanout_15q_2t)
					.PICO_SETTINGS_FOCUS()
					.user_data(4096)
					.label("create fanout 15q 2t 4K arch");
			PICOBENCH_REG(BM_QueryCache_Create_Fanout_7q_4t)
					.PICO_SETTINGS_FOCUS()
					.user_data(1024)
					.label("create fanout 7q 4t 1K arch");
			PICOBENCH_REG(BM_QueryCache_Create_Fanout_7q_4t)
					.PICO_SETTINGS_FOCUS()
					.user_data(4096)
					.label("create fanout 7q 4t 4K arch");
			PICOBENCH_REG(BM_QueryCache_Create_Fanout_3q_8t)
					.PICO_SETTINGS_FOCUS()
					.user_data(1024)
					.label("create fanout 3q 8t 1K arch");
			PICOBENCH_REG(BM_QueryCache_Create_Fanout_3q_8t)
					.PICO_SETTINGS_FOCUS()
					.user_data(4096)
					.label("create fanout 3q 8t 4K arch");
			PICOBENCH_REG(BM_EntityBuilder_BatchAdd_4).PICO_SETTINGS_FOCUS().label("builder batch add 4");
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
			PICOBENCH_REG(BM_QueryCache_ChangedProbe_IdenticalCached)
					.PICO_SETTINGS_FOCUS()
					.user_data(128)
					.label("changed probe identical cached 128q");
			PICOBENCH_REG(BM_QueryCache_ChangedConsume_IdenticalCached)
					.PICO_SETTINGS_FOCUS()
					.user_data(128)
					.label("changed consume identical cached 128q");
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
			PICOBENCH_REG(BM_QueryCache_Sorted_ExactMergeWarmRead)
					.PICO_SETTINGS_FOCUS()
					.user_data(NEntitiesFew)
					.label("sorted exact merge warm 10K");
			PICOBENCH_REG(BM_QueryCache_Sorted_ExactExternalMergeWarmRead)
					.PICO_SETTINGS_FOCUS()
					.user_data(NEntitiesFew)
					.label("sorted exact external merge warm 10K");
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
			PICOBENCH_REG(BM_QueryCache_RegisterPairHeavy_RelWildcard)
					.PICO_SETTINGS_FOCUS()
					.user_data(30)
					.label("register pair-heavy rel wildcard 30");

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
			PICOBENCH_REG(BM_QueryMatch_ExactTerm).PICO_SETTINGS_HEAVY().user_data(128).label("match exact term 1K arch");
			PICOBENCH_REG(BM_QueryCache_CreateArchetype_ExactWildcard)
					.PICO_SETTINGS_HEAVY()
					.user_data(128)
					.label("register cached exact+wildcard 1K arch");
			PICOBENCH_REG(BM_QueryCache_CreateArchetype_ExactWildcard_Miss)
					.PICO_SETTINGS_HEAVY()
					.user_data(128)
					.label("register cached exact+wildcard miss 1K arch");
			PICOBENCH_REG(BM_QueryCache_CreateArchetype_ExactNot)
					.PICO_SETTINGS_HEAVY()
					.user_data(128)
					.label("register cached exact+not 1K arch");
			PICOBENCH_REG(BM_QueryCache_CreateArchetype_ExactOr)
					.PICO_SETTINGS_HEAVY()
					.user_data(128)
					.label("register cached exact+or 1K arch");
			PICOBENCH_REG(BM_QueryCache_CreateArchetype_ExactAny)
					.PICO_SETTINGS_HEAVY()
					.user_data(128)
					.label("register cached exact+any 1K arch");
			PICOBENCH_REG(BM_QueryCache_CreateArchetype_BroadFirstAll)
					.PICO_SETTINGS_HEAVY()
					.user_data(128)
					.label("register cached broad-first all 1K arch");
			PICOBENCH_REG(BM_QueryCache_CreateArchetype_BroadMissAll)
					.PICO_SETTINGS_HEAVY()
					.user_data(128)
					.label("register cached broad-miss all 1K arch");
			PICOBENCH_REG(BM_QueryCache_CreateArchetype_PairHeavyRelWildcard)
					.PICO_SETTINGS_HEAVY()
					.user_data(128)
					.label("register cached pair-heavy rel wildcard 30");
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
			PICOBENCH_REG(BM_QueryMatch_Variable_AllOnly_Unbound)
					.PICO_SETTINGS_FOCUS()
					.user_data(1)
					.label("match all-only 2var src-gated (unbound 1 arch)");
			PICOBENCH_REG(BM_QueryMatch_Variable_AllOnly_Unbound)
					.PICO_SETTINGS_FOCUS()
					.user_data(8)
					.label("match all-only 2var src-gated (unbound 8 arch)");
			PICOBENCH_REG(BM_QueryMatch_Variable_AllOnlyCoupled_Bound)
					.PICO_SETTINGS_HEAVY()
					.user_data(128)
					.label("match all-only coupled (bound)");
			PICOBENCH_REG(BM_QueryMatch_Variable_AllOnlyCoupled_Unbound)
					.PICO_SETTINGS_HEAVY()
					.user_data(128)
					.label("match all-only coupled (unbound)");
			PICOBENCH_REG(BM_QueryMatch_Variable_AllOnlyCoupled_Unbound)
					.PICO_SETTINGS_FOCUS()
					.user_data(1)
					.label("match all-only coupled (unbound 1 arch)");
			PICOBENCH_REG(BM_QueryMatch_Variable_AllOnlyCoupled_Unbound)
					.PICO_SETTINGS_FOCUS()
					.user_data(8)
					.label("match all-only coupled (unbound 8 arch)");
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

			PICOBENCH_SUITE_REG("Observers");
			PICOBENCH_REG(BM_Observer_IsMatchesAny_Semantic_D2)
					.PICO_SETTINGS_FOCUS()
					.user_data(1024)
					.label("observer is matches_any semantic d2");
			PICOBENCH_REG(BM_Observer_IsMatchesAny_Direct_D2)
					.PICO_SETTINGS_FOCUS()
					.user_data(1024)
					.label("observer is matches_any direct d2");
			PICOBENCH_REG(BM_Observer_IsMatchesAny_Semantic_D8)
					.PICO_SETTINGS_FOCUS()
					.user_data(1024)
					.label("observer is matches_any semantic d8");
			PICOBENCH_REG(BM_Observer_IsMatchesAny_Direct_D8)
					.PICO_SETTINGS_FOCUS()
					.user_data(1024)
					.label("observer is matches_any direct d8");

			PICOBENCH_SUITE_REG("Systems (single-thread)");
			PICOBENCH_REG(BM_System_Is_Semantic_D2).PICO_SETTINGS_FOCUS().user_data(1024).label("is semantic d2");
			PICOBENCH_REG(BM_System_Is_Direct_D2).PICO_SETTINGS_FOCUS().user_data(1024).label("is direct d2");
			PICOBENCH_REG(BM_System_Is_Semantic_D8).PICO_SETTINGS_FOCUS().user_data(1024).label("is semantic d8");
			PICOBENCH_REG(BM_System_Is_Direct_D8).PICO_SETTINGS_FOCUS().user_data(1024).label("is direct d8");
			PICOBENCH_REG(BM_System_IsIter_Semantic_D8).PICO_SETTINGS_FOCUS().user_data(1024).label("is iter semantic d8");
			PICOBENCH_REG(BM_System_IsIter_Direct_D8).PICO_SETTINGS_FOCUS().user_data(1024).label("is iter direct d8");

			PICOBENCH_SUITE_REG("Traversal helpers");
			PICOBENCH_REG(BM_World_AsTargetsTrav_2).PICO_SETTINGS_FOCUS().label("as_targets_trav d2");
			PICOBENCH_REG(BM_World_AsTargetsTrav_4).PICO_SETTINGS_FOCUS().label("as_targets_trav d4");
			PICOBENCH_REG(BM_World_AsTargetsTrav_8).PICO_SETTINGS_FOCUS().label("as_targets_trav d8");
			PICOBENCH_REG(BM_World_AsTargetsTrav_32).PICO_SETTINGS_FOCUS().label("as_targets_trav d32");
			return;
		default:
			return;
	}
}
