#include "test_common.h"

namespace {
	template <uint32_t Idx>
	struct SparseTouchProbe {
		GAIA_STORAGE(Sparse);
		uint32_t value = 0;
	};

	template <uint32_t Idx, uint32_t Count>
	void add_sparse_touch_probes(ecs::World& world, ecs::Entity entity) {
		using Probe = SparseTouchProbe<Idx>;
		const auto& item = world.add<Probe>();
		world.add(item.entity, ecs::DontFragment);
		world.add<Probe>(entity);
		if constexpr (Idx + 1 < Count)
			add_sparse_touch_probes<Idx + 1, Count>(world, entity);
	}

	struct DontFragmentEmptyTag {
		GAIA_STORAGE(DontFragment);
	};

	struct DontFragmentEmptyTagB {
		GAIA_STORAGE(DontFragment);
	};

	template <uint32_t Idx>
	struct DontFragmentEmptyTagN {
		GAIA_STORAGE(DontFragment);
	};

	template <uint32_t Idx, uint32_t Count>
	void add_empty_dontfrag_tags(ecs::World& world, ecs::Entity entity) {
		world.add<DontFragmentEmptyTagN<Idx>>(entity);
		if constexpr (Idx + 1 < Count)
			add_empty_dontfrag_tags<Idx + 1, Count>(world, entity);
	}

	template <uint32_t Idx, uint32_t Count>
	void expect_empty_dontfrag_tags(ecs::World& world, ecs::Entity entity) {
		CHECK(world.has<DontFragmentEmptyTagN<Idx>>(entity));
		CHECK_FALSE(world.fetch(entity).pArchetype->has(world.add<DontFragmentEmptyTagN<Idx>>().entity));
		if constexpr (Idx + 1 < Count)
			expect_empty_dontfrag_tags<Idx + 1, Count>(world, entity);
	}

	struct DontFragmentPayload {
		GAIA_STORAGE(DontFragment);
		float x = 0.0f;
	};

	struct DontFragmentRuntimeLatchTag {};

	struct DontFragmentLatchAfterInstancesTag {};

#if GAIA_ENABLE_ADD_DEL_HOOKS
	thread_local uint32_t g_emptyDontFragAddHooks = 0;
	thread_local uint32_t g_emptyDontFragDelHooks = 0;
#endif

	template <uint32_t Idx, uint32_t Count>
	void write_sparse_touch_probes(ecs::Iter& it) {
		using Probe = SparseTouchProbe<Idx>;
		auto view = it.view_any_mut<Probe>();
		if constexpr (Idx < ecs::ChunkHeader::MAX_COMPONENTS) {
			CHECK(view.size() == 1);
			view[0].value = Idx + 1;
		} else {
			CHECK(view.size() == 0);
			if (view.size() != 0)
				view[0].value = Idx + 1;
		}

		if constexpr (Idx + 1 < Count)
			write_sparse_touch_probes<Idx + 1, Count>(it);
	}

	void collect_iter_entities(ecs::Query& query, cnt::darray<ecs::Entity>& out, uint32_t& callbackCnt) {
		callbackCnt = 0;
		out.clear();
		query.each([&](ecs::Iter& it) {
			++callbackCnt;
			CHECK(it.size() > 0);
			const auto entities = it.entity_rows();
			GAIA_EACH(it) {
				out.push_back(entities[i]);
			}
		});
	}

	void expect_iter_entities(ecs::Query& query, std::initializer_list<ecs::Entity> expected) {
		cnt::darray<ecs::Entity> actual;
		uint32_t callbacks = 0;
		collect_iter_entities(query, actual, callbacks);
		CHECK(query.count() == (uint32_t)expected.size());
		CHECK(actual.size() == expected.size());
		for (const auto exp: expected) {
			bool found = false;
			for (const auto got: actual) {
				if (got != exp)
					continue;
				found = true;
				break;
			}
			CHECK(found);
		}
	}

	void collect_ids(
			const ecs::World& world, ecs::Entity entity, cnt::darray<ecs::Entity>& out, ecs::EntityIdOptions options = {}) {
		out.clear();
		world.ids(
				entity,
				[&](ecs::Entity id) {
					out.push_back(id);
				},
				options);
	}

	GAIA_NODISCARD bool contains_id(const cnt::darray<ecs::Entity>& ids, ecs::Entity id) {
		return core::get_index(ids, id) != gaia::BadIndex;
	}

	void expect_unique_ids(const cnt::darray<ecs::Entity>& ids) {
		GAIA_FOR_((uint32_t)ids.size(), idx) {
			GAIA_FOR2_(idx + 1, (uint32_t)ids.size(), other) {
				CHECK(ids[idx] != ids[other]);
			}
		}
	}

	void expect_unique_direct_ids(const ecs::World& world, ecs::Entity entity, const cnt::darray<ecs::Entity>& ids) {
		expect_unique_ids(ids);
		GAIA_FOR_((uint32_t)ids.size(), idx) {
			CHECK(world.has_direct(entity, ids[idx]));
		}
	}
} // namespace

TEST_CASE("Bare Requires prevents direct trait removal") {
	TestWorld twld;

	const auto requiredTrait = wld.add();
	const auto removableTrait = wld.add();
	const auto entity = wld.add();
	wld.add(requiredTrait, ecs::Requires);
	wld.add(entity, requiredTrait);
	wld.add(entity, removableTrait);

	wld.del(entity, requiredTrait);
	wld.del(entity, removableTrait);
	CHECK(wld.has_direct(entity, requiredTrait));
	CHECK_FALSE(wld.has_direct(entity, removableTrait));
}

TEST_CASE("Bare Requires deletion check ignores unassigned pairs") {
	TestWorld twld;

	const auto relation = wld.add();
	const auto target = wld.add();
	const auto entity = wld.add();
	wld.del(entity, ecs::Pair(relation, target));

	CHECK(wld.has(entity));
}

TEST_CASE("Sparse storage traits carry Requires") {
	SparseTestWorld twld;

	CHECK(wld.has_direct(ecs::Sparse, ecs::Requires));
	CHECK(wld.has_direct(ecs::DontFragment, ecs::Requires));
}

TEST_CASE("Required DontFragment cannot be removed from a relation") {
	SparseTestWorld twld;

	const auto relation = wld.add();
	wld.add(relation, ecs::Exclusive);
	wld.add(relation, ecs::DontFragment);
	wld.del(relation, ecs::DontFragment);
	CHECK(wld.has_direct(relation, ecs::DontFragment));
}

TEST_CASE("Sparse DontFragment component and non-fragmenting relation storage") {
	SparseTestWorld twld;

	const auto& compItem = wld.add<PositionSparse>();
	wld.add(compItem.entity, ecs::DontFragment);
	wld.del(compItem.entity, ecs::DontFragment);
	CHECK(wld.has(compItem.entity, ecs::DontFragment));

	const auto e = wld.add();
	const auto* pArchetypeBefore = wld.fetch(e).pArchetype;

	wld.add<PositionSparse>(e);
	CHECK(wld.has<PositionSparse>(e));
	CHECK(wld.has(e, compItem.entity));
	CHECK(wld.fetch(e).pArchetype == pArchetypeBefore);

	{
		auto pos = wld.set<PositionSparse>(e);
		pos = {1.0f, 2.0f, 3.0f};
	}

	const auto& posConst = wld.get<PositionSparse>(e);
	CHECK(posConst.x == doctest::Approx(1.0f));
	CHECK(posConst.y == doctest::Approx(2.0f));
	CHECK(posConst.z == doctest::Approx(3.0f));

	wld.del<PositionSparse>(e);
	CHECK_FALSE(wld.has<PositionSparse>(e));
	CHECK_FALSE(wld.has(e, compItem.entity));
	CHECK(wld.fetch(e).pArchetype == pArchetypeBefore);
}

TEST_CASE("Sparse DontFragment relationship payload stays outside archetype storage") {
	SparseTestWorld twld;

	const auto& relationItem = wld.add<PositionSparse>();
	wld.add(relationItem.entity, ecs::Exclusive);
	wld.add(relationItem.entity, ecs::DontFragment);

	const auto target = wld.add();
	const auto otherTarget = wld.add();
	const auto pair = ecs::Pair(relationItem.entity, target);
	const auto otherPair = ecs::Pair(relationItem.entity, otherTarget);
	const auto entity = wld.add();
	const auto* pArchetypeBefore = wld.fetch(entity).pArchetype;

	wld.add<PositionSparse>(entity, pair, {1.0f, 2.0f, 3.0f});
	const auto* pPayloadBefore = wld.get_raw(entity, pair).data;
	CHECK(wld.fetch(entity).pArchetype == pArchetypeBefore);
	CHECK(wld.has(entity, pair));
	CHECK(wld.get<PositionSparse>(entity, pair).y == doctest::Approx(2.0f));
	CHECK(wld.query().all(pair).count() == 1);

	const auto raw = wld.get_raw(entity, pair);
	CHECK(raw.valid());
	if (raw.valid()) {
		CHECK(raw.size == sizeof(PositionSparse));
		CHECK(((const PositionSparse*)raw.data)->z == doctest::Approx(3.0f));
	}

	// The relationship payload is not a chunk column, so unrelated archetype moves do not relocate it.
	wld.add<Rotation>(entity, {4.0f, 5.0f, 6.0f});
	CHECK(wld.fetch(entity).pArchetype != pArchetypeBefore);
	CHECK(wld.get_raw(entity, pair).data == pPayloadBefore);

	// Replacing an exclusive target removes the old exact-pair payload before constructing the new one.
	wld.add<PositionSparse>(entity, otherPair, {7.0f, 8.0f, 9.0f});
	CHECK_FALSE(wld.has(entity, pair));
	CHECK_FALSE(wld.get_raw(entity, pair).valid());
	CHECK(wld.has(entity, otherPair));
	CHECK(wld.get<PositionSparse>(entity, otherPair).x == doctest::Approx(7.0f));

	const auto copy = wld.copy(entity);
	CHECK(wld.has(copy, otherPair));
	CHECK(wld.get<PositionSparse>(copy, otherPair).z == doctest::Approx(9.0f));

	wld.del(entity, otherPair);
	CHECK_FALSE(wld.has(entity, otherPair));
	CHECK_FALSE(wld.get_raw(entity, otherPair).valid());
	CHECK(wld.has(copy, otherPair));
}

TEST_CASE("Sparse relationship payload keeps exact pair stores independent") {
	SparseTestWorld twld;

	const auto& relationItem = wld.add<PositionSparse>();
	const auto targetA = wld.add();
	const auto targetB = wld.add();
	const auto pairA = ecs::Pair(relationItem.entity, targetA);
	const auto pairB = ecs::Pair(relationItem.entity, targetB);
	const auto entity = wld.add();

	wld.add<PositionSparse>(entity, pairA, {1.0f, 2.0f, 3.0f});
	wld.add<PositionSparse>(entity, pairB, {4.0f, 5.0f, 6.0f});

	CHECK(wld.has(entity, pairA));
	CHECK(wld.has(entity, pairB));
	CHECK(wld.get<PositionSparse>(entity, pairA).x == doctest::Approx(1.0f));
	CHECK(wld.get<PositionSparse>(entity, pairB).x == doctest::Approx(4.0f));
	CHECK(wld.get_raw(entity, pairA).data != wld.get_raw(entity, pairB).data);
	CHECK(wld.query().all(pairA).count() == 1);
	CHECK(wld.query().all(pairB).count() == 1);

	wld.del(entity, pairA);
	CHECK_FALSE(wld.has(entity, pairA));
	CHECK_FALSE(wld.get_raw(entity, pairA).valid());
	CHECK(wld.has(entity, pairB));
	CHECK(wld.get<PositionSparse>(entity, pairB).z == doctest::Approx(6.0f));
	CHECK(wld.sparse_component_store<PositionSparse>((ecs::Entity)pairB) != nullptr);
	wld.add<PositionSparse>(entity, pairA, {10.0f, 11.0f, 12.0f});

	wld.del(targetB);
	wld.update();
	CHECK_FALSE(wld.has(entity, pairB));
	const auto* pPairStore = wld.sparse_component_store<PositionSparse>((ecs::Entity)pairB);
	const bool payloadRemoved = pPairStore == nullptr || !pPairStore->has(entity);
	CHECK(payloadRemoved);
	CHECK(wld.has(entity, pairA));
	CHECK(wld.get<PositionSparse>(entity, pairA).x == doctest::Approx(10.0f));
}

TEST_CASE("Typed sparse mutable views reject touched-term capacity exhaustion") {
	TestWorld twld;

	constexpr uint32_t ProbeCount = ecs::ChunkHeader::MAX_COMPONENTS + 1;
	const auto entity = wld.add();
	wld.add<Position>(entity);
	add_sparse_touch_probes<0, ProbeCount>(wld, entity);

	wld.query().all<Position>().each([&](ecs::Iter& it) {
		write_sparse_touch_probes<0, ProbeCount>(it);
	});

	CHECK(
			wld.get<SparseTouchProbe<ecs::ChunkHeader::MAX_COMPONENTS - 1>>(entity).value ==
			ecs::ChunkHeader::MAX_COMPONENTS);
	CHECK(wld.get<SparseTouchProbe<ecs::ChunkHeader::MAX_COMPONENTS>>(entity).value == 0);
}

TEST_CASE("Builder handles sticky traits and non-fragmenting relation ids") {
	SparseTestWorld twld;

	const auto& compItem = wld.add<PositionSparse>();
	wld.build(compItem.entity).add(ecs::DontFragment).del(ecs::DontFragment).add(ecs::Sparse).del(ecs::Sparse);
	CHECK(wld.has(compItem.entity, ecs::DontFragment));
	CHECK(wld.has(compItem.entity, ecs::Sparse));
	CHECK(compItem.comp.storage_type() == ecs::DataStorageType::Sparse);

	const auto parent = wld.add();
	const auto child = wld.add();
	const auto* pArchetypeBefore = wld.fetch(child).pArchetype;

	wld.build(child).add(ecs::Pair(ecs::Parent, parent));
	CHECK(wld.has(ecs::Pair(ecs::Parent, parent)));
	CHECK(wld.target(child, ecs::Parent) == parent);
	CHECK(wld.fetch(child).pArchetype == pArchetypeBefore);

	wld.build(child).add(ecs::Pair(ecs::Parent, parent));
	CHECK(wld.has(ecs::Pair(ecs::Parent, parent)));
	CHECK(wld.target(child, ecs::Parent) == parent);
	CHECK(wld.fetch(child).pArchetype == pArchetypeBefore);

	wld.build(child).del(ecs::Pair(ecs::Parent, parent));
	CHECK_FALSE(wld.has(child, ecs::Pair(ecs::Parent, parent)));
	CHECK(wld.fetch(child).pArchetype == pArchetypeBefore);
}

TEST_CASE("Sparse DontFragment component runtime object add with value") {
	SparseTestWorld twld;

	const auto& compItem = wld.add<PositionSparse>();
	wld.add(compItem.entity, ecs::DontFragment);

	const auto e = wld.add();
	wld.add(e, compItem.entity, PositionSparse{4.0f, 5.0f, 6.0f});

	CHECK(wld.has<PositionSparse>(e));
	CHECK(wld.has(e, compItem.entity));

	const auto& pos = wld.get<PositionSparse>(e);
	CHECK(pos.x == doctest::Approx(4.0f));
	CHECK(pos.y == doctest::Approx(5.0f));
	CHECK(pos.z == doctest::Approx(6.0f));
}

TEST_CASE("Sparse DontFragment component direct query terms are evaluated as entity filters") {
	SparseTestWorld twld;

	const auto& compItem = wld.add<PositionSparse>();
	wld.add(compItem.entity, ecs::DontFragment);

	const auto eA = wld.add();
	const auto eB = wld.add();
	const auto eC = wld.add();

	wld.add<Position>(eA);
	wld.add<Position>(eB);
	wld.add<Position>(eC);
	wld.add<Scale>(eC);

	wld.add<PositionSparse>(eA);

	auto qAll = wld.query().all<Position>().all<PositionSparse>();
	CHECK(qAll.test_iter_plan().mode == ecs::detail::QueryImpl::QueryPlanMode::EntitySeed);
	CHECK(qAll.count() == 1);
	expect_exact_entities(qAll, {eA});
	expect_iter_entities(qAll, {eA});

	auto qNo = wld.query().all<Position>().no<PositionSparse>();
	CHECK(qNo.test_iter_plan().mode == ecs::detail::QueryImpl::QueryPlanMode::General);
	CHECK((qNo.test_iter_plan().flags & ecs::detail::QueryImpl::QueryPlanFlag_EntityFilter) != 0);
	CHECK(qNo.count() == 2);
	expect_exact_entities(qNo, {eB, eC});
	expect_iter_entities(qNo, {eB, eC});

	auto qOr = wld.query().or_<PositionSparse>().or_<Scale>();
	CHECK(qOr.count() == 2);
	expect_exact_entities(qOr, {eA, eC});
	expect_iter_entities(qOr, {eA, eC});
}

TEST_CASE("Iter each splits DontFragment entity-filter ranges and keeps views aligned") {
	SparseTestWorld twld;

	const auto& compItem = wld.add<PositionSparse>();
	wld.add(compItem.entity, ecs::DontFragment);

	constexpr uint32_t N = 80;
	cnt::darray<ecs::Entity> entities;
	entities.reserve(N);
	GAIA_FOR(N) {
		const auto e = wld.add();
		wld.add<Position>(e, {(float)i, (float)i, 0.0f});
		if ((i & 1U) == 0U)
			wld.add<PositionSparse>(e);
		entities.push_back(e);
	}

	auto qNo = wld.query().all<Position>().no<PositionSparse>();
	CHECK(qNo.test_iter_plan().mode == ecs::detail::QueryImpl::QueryPlanMode::General);
	CHECK(qNo.count() == N / 2);

	uint32_t callbacks = 0;
	uint32_t hits = 0;
	qNo.each([&](ecs::Iter& it) {
		++callbacks;
		CHECK(it.size() > 0);
		const auto rows = it.entity_rows();
		const auto positions = it.view<Position>();
		GAIA_EACH(it) {
			++hits;
			uint32_t entIdx = N;
			for (uint32_t e = 0; e < N; ++e) {
				if (entities[e] != rows[i])
					continue;
				entIdx = e;
				break;
			}
			CHECK(entIdx < N);
			CHECK((entIdx & 1U) == 1U);
			CHECK(positions[i].x == doctest::Approx((float)entIdx));
			CHECK_FALSE(wld.has<PositionSparse>(rows[i]));
		}
	});
	CHECK(hits == N / 2);
	CHECK(callbacks == N / 2);

	qNo.each([&](ecs::Iter& it) {
		auto positions = it.view_mut<Position>();
		GAIA_EACH(it) {
			positions[i].x += 100.0f;
		}
	});
	GAIA_FOR(N) {
		const auto x = wld.get<Position>(entities[i]).x;
		if ((i & 1U) == 0U)
			CHECK(x == doctest::Approx((float)i));
		else
			CHECK(x == doctest::Approx((float)i + 100.0f));
	}

	wld.enable(entities[1], false);
	CHECK(qNo.count() == N / 2 - 1);
	uint32_t enabledHits = 0;
	qNo.each([&](ecs::Iter& it) {
		const auto rows = it.entity_rows();
		GAIA_EACH(it) {
			++enabledHits;
			CHECK(rows[i] != entities[1]);
			CHECK_FALSE(wld.has<PositionSparse>(rows[i]));
		}
	});
	CHECK(enabledHits == N / 2 - 1);

	auto qAllMarked = wld.query().all<Position>().all<PositionSparse>();
	CHECK(qAllMarked.count() == N / 2);
	cnt::darray<ecs::Entity> marked;
	uint32_t markedCallbacks = 0;
	collect_iter_entities(qAllMarked, marked, markedCallbacks);
	CHECK(marked.size() == N / 2);
	CHECK(markedCallbacks == N / 2);
	for (const auto e: marked)
		CHECK(wld.has<PositionSparse>(e));

	auto qEmpty = wld.query().all<Position>().no<PositionSparse>().all<Scale>();
	uint32_t emptyCallbacks = 0;
	qEmpty.each([&](ecs::Iter& it) {
		++emptyCallbacks;
		(void)it;
	});
	CHECK(qEmpty.count() == 0);
	CHECK(emptyCallbacks == 0);
}

TEST_CASE("System Iter each respects DontFragment entity filters") {
	SparseTestWorld twld;

	const auto& compItem = wld.add<PositionSparse>();
	wld.add(compItem.entity, ecs::DontFragment);

	const auto eA = wld.add();
	const auto eB = wld.add();
	wld.add<Position>(eA, {1.0f, 0.0f, 0.0f});
	wld.add<Position>(eB, {2.0f, 0.0f, 0.0f});
	wld.add<PositionSparse>(eA);

	uint32_t hits = 0;
	float sum = 0.0f;
	wld.system().all<Position>().no<PositionSparse>().on_each([&](ecs::Iter& it) {
		const auto rows = it.entity_rows();
		const auto positions = it.view<Position>();
		GAIA_EACH(it) {
			++hits;
			sum += positions[i].x;
			CHECK(rows[i] == eB);
			CHECK_FALSE(wld.has<PositionSparse>(rows[i]));
		}
	});
	wld.update();
	CHECK(hits == 1);
	CHECK(sum == doctest::Approx(2.0f));
}

TEST_CASE("Sparse DontFragment typed callbacks bind direct payloads per entity") {
	SparseTestWorld twld;

	const auto& compItem = wld.add<PositionSparse>();
	wld.add(compItem.entity, ecs::DontFragment);

	const auto eA = wld.add();
	const auto eB = wld.add();
	const auto eC = wld.add();
	for (const auto entity: {eA, eB, eC})
		wld.add<Position>(entity, {1.0f, 2.0f, 3.0f});

	auto q = wld.query().all<Position&>().all<PositionSparse&>();
	uint32_t hits = 0;
	q.each([&](Position& pos, PositionSparse& sparse) {
		pos.x += sparse.x;
		sparse.y += 10.0f;
		++hits;
	});
	CHECK(hits == 0);

	wld.add<PositionSparse>(eA, {4.0f, 5.0f, 6.0f});
	wld.add<PositionSparse>(eC, {7.0f, 8.0f, 9.0f});

	q.each([&](Position& pos, PositionSparse& sparse) {
		pos.x += sparse.x;
		sparse.y += 10.0f;
		++hits;
	});
	CHECK(hits == 2);
	CHECK(wld.get<Position>(eA).x == doctest::Approx(5.0f));
	CHECK(wld.get<Position>(eB).x == doctest::Approx(1.0f));
	CHECK(wld.get<Position>(eC).x == doctest::Approx(8.0f));
	CHECK(wld.get<PositionSparse>(eA).y == doctest::Approx(15.0f));
	CHECK(wld.get<PositionSparse>(eC).y == doctest::Approx(18.0f));

	float sum = 0.0f;
	wld.query().all<PositionSparse>().each([&](const PositionSparse& sparse) {
		sum += sparse.x;
	});
	CHECK(sum == doctest::Approx(11.0f));
}

TEST_CASE("Typed systems bind sparse callback payloads per run") {
	SUBCASE("Fragmenting") {
		SparseTestWorld twld;
		uint32_t hits = 0;
		wld.system().all<PositionSparse&>().on_each([&](PositionSparse& sparse) {
			sparse.x += 4.0f;
			++hits;
		});
		wld.systems_run();
		CHECK(hits == 0);

		const auto entity = wld.add();
		wld.add<PositionSparse>(entity, {1.0f, 2.0f, 3.0f});
		wld.systems_run();
		CHECK(hits == 1);
		CHECK(wld.get<PositionSparse>(entity).x == doctest::Approx(5.0f));

		wld.del<PositionSparse>(entity);
		wld.systems_run();
		CHECK(hits == 1);

		wld.add<PositionSparse>(entity, {2.0f, 3.0f, 4.0f});
		wld.systems_run();
		CHECK(hits == 2);
		CHECK(wld.get<PositionSparse>(entity).x == doctest::Approx(6.0f));
	}

	SUBCASE("DontFragment") {
		SparseTestWorld twld;
		const auto& compItem = wld.add<PositionSparse>();
		wld.add(compItem.entity, ecs::DontFragment);
		uint32_t hits = 0;
		wld.system().all<PositionSparse&>().on_each([&](PositionSparse& sparse) {
			sparse.x += 4.0f;
			++hits;
		});
		wld.systems_run();
		CHECK(hits == 0);

		const auto entity = wld.add();
		wld.add<PositionSparse>(entity, {1.0f, 2.0f, 3.0f});
		wld.systems_run();
		CHECK(hits == 1);
		CHECK(wld.get<PositionSparse>(entity).x == doctest::Approx(5.0f));

		wld.del<PositionSparse>(entity);
		wld.systems_run();
		CHECK(hits == 1);

		wld.add<PositionSparse>(entity, {2.0f, 3.0f, 4.0f});
		wld.systems_run();
		CHECK(hits == 2);
		CHECK(wld.get<PositionSparse>(entity).x == doctest::Approx(6.0f));
	}
}

TEST_CASE("Mutable sparse iterator views keep inherited rows stable and notify after writes") {
	SparseTestWorld twld;

	const auto parent = wld.add();
	wld.add<PositionSparse>(parent, {100.0f, 0.0f, 0.0f});
	const auto sparseId = wld.add<PositionSparse>().entity;
	wld.add(sparseId, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto direct = wld.add();
	wld.add<Position>(direct);
	wld.add<PositionSparse>(direct, {1.0f, 0.0f, 0.0f});

	constexpr uint32_t ChildCount = 3;
	ecs::Entity children[ChildCount];
	GAIA_FOR(ChildCount) {
		children[i] = wld.add();
		wld.add<Position>(children[i]);
		wld.as(children[i], parent);
		CHECK_FALSE(wld.has_direct(children[i], sparseId));
	}

	uint32_t setHits = 0;
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnSet)
														.all<PositionSparse>()
														.on_each([&](ecs::Entity entity) {
															CHECK(entity != parent);
															CHECK(wld.get<PositionSparse>(entity).x >= 10.0f);
															++setHits;
														})
														.entity();
	(void)observer;

	wld.query().all<Position>().all<PositionSparse&>().each([&](ecs::Iter& it) {
		const auto entities = it.view<ecs::Entity>();
		auto sparse = it.view_any_mut<PositionSparse>();
		GAIA_EACH(it) {
			if (entities[i] == direct)
				sparse[i].x = 11.0f;
			else
				sparse[i].x = 200.0f + (float)i;
		}
	});

	CHECK(setHits == ChildCount + 1);
	CHECK(wld.get<PositionSparse>(direct).x == doctest::Approx(11.0f));
	CHECK(wld.get<PositionSparse>(parent).x == doctest::Approx(100.0f));
	GAIA_FOR(ChildCount) {
		CHECK(wld.has_direct(children[i], sparseId));
		CHECK(wld.get<PositionSparse>(children[i]).x >= 200.0f);
	}
}

TEST_CASE("Sparse DontFragment component can change directly during serial query iteration") {
	SparseTestWorld twld;

	const auto& compItem = wld.add<PositionSparse>();
	wld.add(compItem.entity, ecs::DontFragment);

	const auto eA = wld.add();
	const auto eB = wld.add();
	const auto eC = wld.add();
	wld.add<Position>(eA);
	wld.add<Position>(eB);
	wld.add<Position>(eC);
	wld.add<PositionSparse>(eC, {9.0f, 10.0f, 11.0f});

	const auto* pArchetypeA = wld.fetch(eA).pArchetype;
	const auto* pArchetypeB = wld.fetch(eB).pArchetype;
	const auto* pArchetypeC = wld.fetch(eC).pArchetype;

	uint32_t hits = 0;
	wld.query().all<Position>().each([&](ecs::Iter& it) {
		const auto entities = it.view<ecs::Entity>();
		GAIA_EACH(it) {
			const auto entity = entities[i];
			if (entity == eA) {
				wld.add<PositionSparse>(entity);
			} else if (entity == eB) {
				wld.add<PositionSparse>(entity, {4.0f, 5.0f, 6.0f});
			} else if (entity == eC) {
				wld.del<PositionSparse>(entity);
			}
			++hits;
		}
	});

	CHECK(hits == 3);
	CHECK(wld.has<PositionSparse>(eA));
	CHECK(wld.has<PositionSparse>(eB));
	CHECK_FALSE(wld.has<PositionSparse>(eC));
	CHECK(wld.fetch(eA).pArchetype == pArchetypeA);
	CHECK(wld.fetch(eB).pArchetype == pArchetypeB);
	CHECK(wld.fetch(eC).pArchetype == pArchetypeC);

	const auto& posB = wld.get<PositionSparse>(eB);
	CHECK(posB.x == doctest::Approx(4.0f));
	CHECK(posB.y == doctest::Approx(5.0f));
	CHECK(posB.z == doctest::Approx(6.0f));
}

TEST_CASE("Entity-seed each commits iterator command buffers") {
	SparseTestWorld twld;

	const auto& compItem = wld.add<PositionSparse>();
	wld.add(compItem.entity, ecs::DontFragment);
	wld.add<Scale>();

	const auto eKeep = wld.add();
	const auto eDrop = wld.add();
	const auto eAdd = wld.add();
	wld.add<PositionSparse>(eKeep, {1.0f, 2.0f, 3.0f});
	wld.add<PositionSparse>(eDrop, {4.0f, 5.0f, 6.0f});
	wld.add<PositionSparse>(eAdd, {7.0f, 8.0f, 9.0f});

	auto q = wld.query().all<PositionSparse>();
	CHECK(q.test_iter_plan().mode == ecs::detail::QueryImpl::QueryPlanMode::EntitySeed);

	q.each([&](ecs::Iter& it) {
		const auto entities = it.view<ecs::Entity>();
		auto& cb = it.cmd_buffer_st();
		GAIA_EACH(it) {
			const auto entity = entities[i];
			if (entity == eDrop)
				cb.del<PositionSparse>(entity);
			else if (entity == eAdd)
				cb.add<Scale>(entity, {1.0f, 2.0f, 3.0f});
		}
	});

	CHECK(wld.has<PositionSparse>(eKeep));
	CHECK_FALSE(wld.has<PositionSparse>(eDrop));
	CHECK(wld.has<PositionSparse>(eAdd));
	CHECK(wld.has<Scale>(eAdd));
	CHECK_FALSE(wld.has<Scale>(eKeep));
	CHECK_FALSE(wld.has<Scale>(eDrop));

	uint32_t n = 0;
	wld.query().all<PositionSparse>().each([&](ecs::Entity) {
		++n;
	});
	CHECK(n == 2);

	const auto eSet = wld.add();
	wld.add<PositionSparse>(eSet, {1.0f, 2.0f, 3.0f});

	auto qWrite = wld.query().all<PositionSparse&>();
	CHECK(qWrite.test_iter_plan().mode == ecs::detail::QueryImpl::QueryPlanMode::EntitySeed);
	qWrite.each([&](ecs::Iter& it) {
		const auto entities = it.view<ecs::Entity>();
		auto& cb = it.cmd_buffer_st();
		GAIA_EACH(it) {
			if (entities[i] == eSet)
				cb.set<PositionSparse>(entities[i], {10.0f, 11.0f, 12.0f});
		}
	});

	const auto& pos = wld.get<PositionSparse>(eSet);
	CHECK(pos.x == doctest::Approx(10.0f));
	CHECK(pos.y == doctest::Approx(11.0f));
	CHECK(pos.z == doctest::Approx(12.0f));
}

TEST_CASE("Compile-time sparse component uses sparse storage and still fragments") {
	SparseTestWorld twld;

	const auto& compItem = wld.add<PositionSparse>();
	CHECK(wld.has(compItem.entity, ecs::Sparse));
	const auto e = wld.add();
	const auto* pArchetypeBefore = wld.fetch(e).pArchetype;

	wld.add<PositionSparse>(e, {1.0f, 2.0f, 3.0f});
	CHECK(wld.has<PositionSparse>(e));
	CHECK(wld.has(e, compItem.entity));
	CHECK(wld.fetch(e).pArchetype != pArchetypeBefore);
	CHECK(wld.fetch(e).pArchetype->has(compItem.entity));

	auto q = wld.query().all<PositionSparse&>();
	uint32_t hits = 0;
	q.each([&](PositionSparse& pos) {
		pos.x += 1.0f;
		++hits;
	});
	CHECK(hits == 1);
	CHECK(wld.get<PositionSparse>(e).x == doctest::Approx(2.0f));

	wld.del<PositionSparse>(e);
	CHECK_FALSE(wld.has<PositionSparse>(e));
	CHECK(wld.fetch(e).pArchetype == pArchetypeBefore);
}

TEST_CASE("Sparse component default add still makes direct storage") {
	SparseTestWorld twld;

	const auto& compItem = wld.add<PositionSparse>();
	const auto e = wld.add();
	const auto* pArchetypeBefore = wld.fetch(e).pArchetype;

	wld.add<PositionSparse>(e);
	CHECK(wld.has<PositionSparse>(e));
	CHECK(wld.has(e, compItem.entity));
	CHECK(wld.fetch(e).pArchetype != pArchetypeBefore);
	CHECK(wld.fetch(e).pArchetype->has(compItem.entity));

	{
		auto pos = wld.set<PositionSparse>(e);
		pos = {2.0f, 3.0f, 4.0f};
	}

	const auto& pos = wld.get<PositionSparse>(e);
	CHECK(pos.x == doctest::Approx(2.0f));
	CHECK(pos.y == doctest::Approx(3.0f));
	CHECK(pos.z == doctest::Approx(4.0f));
}

TEST_CASE("Compile-time table component storage is authoritative") {
	TestWorld twld;

	const auto& compItem = wld.add<Position>();
	CHECK(compItem.comp.storage_type() == ecs::DataStorageType::Table);
	CHECK_FALSE(wld.has(compItem.entity, ecs::Sparse));

	wld.add(compItem.entity, ecs::Sparse);
	CHECK(compItem.comp.storage_type() == ecs::DataStorageType::Table);
	CHECK_FALSE(wld.has(compItem.entity, ecs::Sparse));
	CHECK_FALSE(wld.component_uses_sparse_storage(compItem.entity));
	wld.add(compItem.entity, ecs::DontFragment);
	CHECK_FALSE(wld.has(compItem.entity, ecs::DontFragment));

	const auto e = wld.add();
	const auto* pArchetypeBefore = wld.fetch(e).pArchetype;

	wld.add<Position>(e, {1.0f, 2.0f, 3.0f});
	CHECK(wld.has<Position>(e));
	CHECK(wld.fetch(e).pArchetype != pArchetypeBefore);
	CHECK(wld.fetch(e).pArchetype->has(compItem.entity));

	const auto& pos = wld.get<Position>(e);
	CHECK(pos.x == doctest::Approx(1.0f));
	CHECK(pos.y == doctest::Approx(2.0f));
	CHECK(pos.z == doctest::Approx(3.0f));
}

TEST_CASE("Compile-time DontFragment empty tag stays outside archetype identity") {
	TestWorld twld;

	const auto& compItem = wld.add<DontFragmentEmptyTag>();
	CHECK(compItem.comp.storage_type() == ecs::DataStorageType::Table);
	CHECK(compItem.comp.size() == 0);
	CHECK(wld.has(compItem.entity, ecs::DontFragment));
	CHECK_FALSE(wld.has(compItem.entity, ecs::Sparse));
	CHECK_FALSE(wld.component_uses_sparse_storage(compItem.entity));

	const auto e = wld.add();
	const auto* pArchetypeBefore = wld.fetch(e).pArchetype;

	wld.add<DontFragmentEmptyTag>(e);
	CHECK(wld.has<DontFragmentEmptyTag>(e));
	CHECK(wld.has(e, compItem.entity));
	CHECK(wld.fetch(e).pArchetype == pArchetypeBefore);
	CHECK_FALSE(wld.fetch(e).pArchetype->has(compItem.entity));

	auto q = wld.query().all<DontFragmentEmptyTag>();
	CHECK(q.test_iter_plan().mode == ecs::detail::QueryImpl::QueryPlanMode::EntitySeed);
	CHECK(q.count() == 1);
	expect_exact_entities(q, {e});

	wld.del<DontFragmentEmptyTag>(e);
	CHECK_FALSE(wld.has<DontFragmentEmptyTag>(e));
	CHECK_FALSE(wld.has(e, compItem.entity));
	CHECK(wld.fetch(e).pArchetype == pArchetypeBefore);
	CHECK(q.count() == 0);

	wld.add<DontFragmentEmptyTag>(e);
	wld.build(e).del<DontFragmentEmptyTag>();
	CHECK_FALSE(wld.has<DontFragmentEmptyTag>(e));
	CHECK(wld.fetch(e).pArchetype == pArchetypeBefore);
}

TEST_CASE("Compile-time DontFragment AoS payload uses sparse non-fragmenting storage") {
	TestWorld twld;

	const auto& compItem = wld.add<DontFragmentPayload>();
	CHECK(compItem.comp.storage_type() == ecs::DataStorageType::Sparse);
	CHECK(wld.has(compItem.entity, ecs::DontFragment));
	CHECK(wld.has(compItem.entity, ecs::Sparse));
	CHECK(wld.component_uses_sparse_storage(compItem.entity));

	const auto e = wld.add();
	const auto* pArchetypeBefore = wld.fetch(e).pArchetype;

	wld.add<DontFragmentPayload>(e, {4.0f});
	CHECK(wld.has<DontFragmentPayload>(e));
	CHECK(wld.fetch(e).pArchetype == pArchetypeBefore);
	CHECK_FALSE(wld.fetch(e).pArchetype->has(compItem.entity));
	CHECK(wld.get<DontFragmentPayload>(e).x == doctest::Approx(4.0f));

	{
		auto payload = wld.set<DontFragmentPayload>(e);
		payload.x = 8.0f;
	}
	CHECK(wld.get<DontFragmentPayload>(e).x == doctest::Approx(8.0f));

	wld.del<DontFragmentPayload>(e);
	CHECK_FALSE(wld.has<DontFragmentPayload>(e));
	CHECK(wld.fetch(e).pArchetype == pArchetypeBefore);
}

TEST_CASE("Runtime DontFragment latches on a fresh empty typed tag") {
	TestWorld twld;

	const auto& compItem = wld.add<DontFragmentRuntimeLatchTag>();
	CHECK(compItem.comp.storage_type() == ecs::DataStorageType::Table);
	CHECK_FALSE(wld.has(compItem.entity, ecs::DontFragment));
	CHECK_FALSE(wld.has(compItem.entity, ecs::Sparse));

	wld.add(compItem.entity, ecs::DontFragment);
	CHECK(wld.has(compItem.entity, ecs::DontFragment));
	CHECK(compItem.comp.storage_type() == ecs::DataStorageType::Table);
	CHECK_FALSE(wld.component_uses_sparse_storage(compItem.entity));

	const auto e = wld.add();
	const auto* pArchetypeBefore = wld.fetch(e).pArchetype;

	wld.add(e, compItem.entity);
	CHECK(wld.has<DontFragmentRuntimeLatchTag>(e));
	CHECK(wld.has(e, compItem.entity));
	CHECK(wld.fetch(e).pArchetype == pArchetypeBefore);
	CHECK_FALSE(wld.fetch(e).pArchetype->has(compItem.entity));

	auto q = wld.query().all<DontFragmentRuntimeLatchTag>();
	CHECK(q.test_iter_plan().mode == ecs::detail::QueryImpl::QueryPlanMode::EntitySeed);
	CHECK(q.count() == 1);
	expect_exact_entities(q, {e});

	wld.del(e, compItem.entity);
	CHECK_FALSE(wld.has<DontFragmentRuntimeLatchTag>(e));
	CHECK(wld.fetch(e).pArchetype == pArchetypeBefore);
}

TEST_CASE("Runtime DontFragment latches on a fresh empty runtime tag") {
	TestWorld twld;

	const auto& runtimeComp =
			add_runtime_component(wld, "Runtime_Empty_DontFragment_Tag", 0, ecs::DataStorageType::Table, 1);
	CHECK(runtimeComp.comp.storage_type() == ecs::DataStorageType::Table);
	CHECK_FALSE(wld.has(runtimeComp.entity, ecs::DontFragment));

	wld.add(runtimeComp.entity, ecs::DontFragment);
	CHECK(wld.has(runtimeComp.entity, ecs::DontFragment));
	CHECK(runtimeComp.comp.storage_type() == ecs::DataStorageType::Table);
	CHECK_FALSE(wld.component_uses_sparse_storage(runtimeComp.entity));

	const auto e = wld.add();
	const auto* pArchetypeBefore = wld.fetch(e).pArchetype;

	wld.add(e, runtimeComp.entity);
	CHECK(wld.has(e, runtimeComp.entity));
	CHECK(wld.fetch(e).pArchetype == pArchetypeBefore);
	CHECK_FALSE(wld.fetch(e).pArchetype->has(runtimeComp.entity));

	auto q = wld.query().all(runtimeComp.entity);
	CHECK(q.test_iter_plan().mode == ecs::detail::QueryImpl::QueryPlanMode::EntitySeed);
	CHECK(q.count() == 1);
	expect_exact_entities(q, {e});

	wld.del(e, runtimeComp.entity);
	CHECK_FALSE(wld.has(e, runtimeComp.entity));
	CHECK(wld.fetch(e).pArchetype == pArchetypeBefore);
}

TEST_CASE("Runtime DontFragment does not latch on an empty tag that already has instances") {
	TestWorld twld;

	const auto& compItem = wld.add<DontFragmentLatchAfterInstancesTag>();
	const auto e = wld.add();
	const auto* pArchetypeBefore = wld.fetch(e).pArchetype;

	wld.add<DontFragmentLatchAfterInstancesTag>(e);
	CHECK(wld.has<DontFragmentLatchAfterInstancesTag>(e));
	CHECK(wld.fetch(e).pArchetype != pArchetypeBefore);
	CHECK(wld.fetch(e).pArchetype->has(compItem.entity));

	wld.add(compItem.entity, ecs::DontFragment);
	CHECK_FALSE(wld.has(compItem.entity, ecs::DontFragment));
	CHECK(wld.fetch(e).pArchetype->has(compItem.entity));
}

TEST_CASE("Copy keeps empty DontFragment tag membership outside the archetype") {
	TestWorld twld;

	const auto& compItem = wld.add<DontFragmentEmptyTag>();
	const auto src = wld.add();
	const auto* pArchetypeBefore = wld.fetch(src).pArchetype;
	wld.add<DontFragmentEmptyTag>(src);

	const auto dst = wld.copy(src);
	CHECK(wld.has<DontFragmentEmptyTag>(src));
	CHECK(wld.has<DontFragmentEmptyTag>(dst));
	CHECK(wld.fetch(src).pArchetype == pArchetypeBefore);
	CHECK(wld.fetch(dst).pArchetype == pArchetypeBefore);
	CHECK_FALSE(wld.fetch(dst).pArchetype->has(compItem.entity));

	wld.del<DontFragmentEmptyTag>(src);
	CHECK_FALSE(wld.has<DontFragmentEmptyTag>(src));
	CHECK(wld.has<DontFragmentEmptyTag>(dst));
}

TEST_CASE("Empty DontFragment tag query all, no, and or with a table neighbor") {
	TestWorld twld;

	const auto eA = wld.add();
	const auto eB = wld.add();
	const auto eC = wld.add();
	wld.add<Position>(eA);
	wld.add<Position>(eB);
	wld.add<Position>(eC);
	wld.add<Scale>(eC);
	wld.add<DontFragmentEmptyTag>(eA);

	auto qAll = wld.query().all<Position>().all<DontFragmentEmptyTag>();
	CHECK(qAll.test_iter_plan().mode == ecs::detail::QueryImpl::QueryPlanMode::EntitySeed);
	CHECK(qAll.count() == 1);
	expect_exact_entities(qAll, {eA});
	expect_iter_entities(qAll, {eA});

	auto qNo = wld.query().all<Position>().no<DontFragmentEmptyTag>();
	CHECK(qNo.count() == 2);
	expect_exact_entities(qNo, {eB, eC});
	expect_iter_entities(qNo, {eB, eC});

	auto qOr = wld.query().or_<DontFragmentEmptyTag>().or_<Scale>();
	CHECK(qOr.count() == 2);
	expect_exact_entities(qOr, {eA, eC});
	expect_iter_entities(qOr, {eA, eC});
}

#if GAIA_OBSERVERS_ENABLED
TEST_CASE("Empty DontFragment tag observers fire on add and del") {
	TestWorld twld;

	uint32_t addHits = 0;
	uint32_t delHits = 0;
	(void)wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.all<DontFragmentEmptyTag>()
			.on_each([&]() {
				++addHits;
			})
			.entity();
	(void)wld.observer()
			.event(ecs::ObserverEvent::OnDel)
			.all<DontFragmentEmptyTag>()
			.on_each([&]() {
				++delHits;
			})
			.entity();

	const auto e = wld.add();
	const auto* pArchetypeBefore = wld.fetch(e).pArchetype;
	wld.add<DontFragmentEmptyTag>(e);
	CHECK(addHits == 1);
	wld.del<DontFragmentEmptyTag>(e);
	CHECK(delHits == 1);
	CHECK(wld.fetch(e).pArchetype == pArchetypeBefore);
}
#endif

TEST_CASE("Empty DontFragment tag command buffers, instantiate, clear, and entity delete") {
	TestWorld twld;

	const auto& compItem = wld.add<DontFragmentEmptyTag>();
	const auto e = wld.add();
	const auto* pArchetypeBefore = wld.fetch(e).pArchetype;

	ecs::CommandBufferST commandBuffer(wld);
	commandBuffer.add<DontFragmentEmptyTag>(e);
	commandBuffer.commit();
	CHECK(wld.has<DontFragmentEmptyTag>(e));
	CHECK(wld.fetch(e).pArchetype == pArchetypeBefore);

	commandBuffer.del<DontFragmentEmptyTag>(e);
	commandBuffer.commit();
	CHECK_FALSE(wld.has<DontFragmentEmptyTag>(e));
	CHECK(wld.fetch(e).pArchetype == pArchetypeBefore);

	const auto prefab = wld.prefab();
	wld.add<DontFragmentEmptyTag>(prefab);
	const auto instance = wld.instantiate(prefab);
	CHECK(wld.has<DontFragmentEmptyTag>(instance));
	CHECK_FALSE(wld.fetch(instance).pArchetype->has(compItem.entity));

	wld.add<DontFragmentEmptyTag>(e);
	wld.add<Position>(e, {1.0f, 2.0f, 3.0f});
	const auto* pArchetypeWithPosition = wld.fetch(e).pArchetype;
	wld.clear(e);
	CHECK(wld.has(e));
	CHECK_FALSE(wld.has<DontFragmentEmptyTag>(e));
	CHECK_FALSE(wld.has<Position>(e));
	CHECK(wld.fetch(e).pArchetype != pArchetypeWithPosition);

	wld.add<DontFragmentEmptyTag>(e);
	CHECK(wld.query().all<DontFragmentEmptyTag>().count() == 2);
	wld.del(e);
	CHECK_FALSE(wld.has(e));
	CHECK(wld.query().all<DontFragmentEmptyTag>().count() == 1);
	expect_exact_entities(wld.query().all<DontFragmentEmptyTag>(), {instance});
}

TEST_CASE("Empty DontFragment tag query each reads a table neighbor") {
	TestWorld twld;

	const auto eA = wld.add();
	const auto eB = wld.add();
	wld.add<Position>(eA, {1.0f, 2.0f, 3.0f});
	wld.add<Position>(eB, {4.0f, 5.0f, 6.0f});
	wld.add<DontFragmentEmptyTag>(eA);

	auto q = wld.query().all<Position>().all<DontFragmentEmptyTag>();
	CHECK(q.test_iter_plan().mode == ecs::detail::QueryImpl::QueryPlanMode::EntitySeed);

	float sum = 0.0f;
	uint32_t hits = 0;
	q.each([&](ecs::Iter& it) {
		auto pos = it.view<Position>();
		GAIA_EACH(it) {
			sum += pos[i].x;
			++hits;
		}
	});
	CHECK(hits == 1);
	CHECK(sum == doctest::Approx(1.0f));

	auto qWrite = wld.query().all<Position&>().all<DontFragmentEmptyTag>();
	CHECK(qWrite.test_iter_plan().mode == ecs::detail::QueryImpl::QueryPlanMode::EntitySeed);
	hits = 0;
	qWrite.each([&](Position& p) {
		p.x += 10.0f;
		++hits;
	});
	CHECK(hits == 1);
	CHECK(wld.get<Position>(eA).x == doctest::Approx(11.0f));
	CHECK(wld.get<Position>(eB).x == doctest::Approx(4.0f));
}

TEST_CASE("Empty DontFragment tag uncached query") {
	TestWorld twld;

	const auto e = wld.add();
	wld.add<Position>(e, {1.0f, 2.0f, 3.0f});
	wld.add<DontFragmentEmptyTag>(e);

	auto q = wld.uquery().all<Position>().all<DontFragmentEmptyTag>();
	CHECK(q.test_iter_plan().mode == ecs::detail::QueryImpl::QueryPlanMode::EntitySeed);
	CHECK(q.count() == 1);
	expect_exact_entities(q, {e});

	float sum = 0.0f;
	q.each([&](ecs::Iter& it) {
		auto pos = it.view<Position>();
		GAIA_EACH(it) sum += pos[i].x;
	});
	CHECK(sum == doctest::Approx(1.0f));
}

TEST_CASE("Empty DontFragment tag disabled entities") {
	TestWorld twld;

	const auto eEnabled = wld.add();
	const auto eDisabled = wld.add();
	wld.add<DontFragmentEmptyTag>(eEnabled);
	wld.add<DontFragmentEmptyTag>(eDisabled);
	wld.enable(eDisabled, false);

	auto q = wld.query().all<DontFragmentEmptyTag>();
	CHECK(q.count() == 1);
	expect_exact_entities(q, {eEnabled});
	CHECK(q.count(ecs::Constraints::DisabledOnly) == 1);
	CHECK(q.count(ecs::Constraints::AcceptAll) == 2);

	uint32_t disabledHits = 0;
	q.each(
			[&](ecs::Iter& it) {
				auto ents = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					CHECK(ents[i] == eDisabled);
					++disabledHits;
				}
			},
			ecs::Constraints::DisabledOnly);
	CHECK(disabledHits == 1);

	uint32_t acceptHits = 0;
	q.each(
			[&](ecs::Iter& it) {
				acceptHits += it.size();
			},
			ecs::Constraints::AcceptAll);
	CHECK(acceptHits == 2);
}

TEST_CASE("Empty DontFragment tag recycled entity slots") {
	TestWorld twld;

	const auto entity = wld.add();
	wld.add<DontFragmentEmptyTag>(entity);
	CHECK(wld.has<DontFragmentEmptyTag>(entity));

	wld.del(entity);
	wld.update();

	const auto recycled = wld.add();
	CHECK(recycled.id() == entity.id());
	CHECK(recycled.gen() != entity.gen());
	CHECK_FALSE(wld.has<DontFragmentEmptyTag>(recycled));
	CHECK(wld.query().all<DontFragmentEmptyTag>().count() == 0);

	ecs::CommandBufferST commandBuffer(wld);
	commandBuffer.add<DontFragmentEmptyTag>(recycled);
	commandBuffer.commit();
	CHECK(wld.has<DontFragmentEmptyTag>(recycled));
	CHECK(wld.query().all<DontFragmentEmptyTag>().count() == 1);
	expect_exact_entities(wld.query().all<DontFragmentEmptyTag>(), {recycled});
}

TEST_CASE("Empty DontFragment tag CommandBufferMT and iterator command buffers") {
	TestWorld twld;

	const auto e = wld.add();
	const auto* pArchetypeBefore = wld.fetch(e).pArchetype;

	ecs::CommandBufferMT commandBuffer(wld);
	commandBuffer.add<DontFragmentEmptyTag>(e);
	commandBuffer.commit();
	CHECK(wld.has<DontFragmentEmptyTag>(e));
	CHECK(wld.fetch(e).pArchetype == pArchetypeBefore);

	commandBuffer.del<DontFragmentEmptyTag>(e);
	commandBuffer.commit();
	CHECK_FALSE(wld.has<DontFragmentEmptyTag>(e));
	CHECK(wld.fetch(e).pArchetype == pArchetypeBefore);

	const auto eKeep = wld.add();
	const auto eDrop = wld.add();
	wld.add<DontFragmentEmptyTag>(eKeep);
	wld.add<DontFragmentEmptyTag>(eDrop);

	auto q = wld.query().all<DontFragmentEmptyTag>();
	CHECK(q.test_iter_plan().mode == ecs::detail::QueryImpl::QueryPlanMode::EntitySeed);
	q.each([&](ecs::Iter& it) {
		const auto entities = it.view<ecs::Entity>();
		auto& cb = it.cmd_buffer_mt();
		GAIA_EACH(it) {
			if (entities[i] == eDrop)
				cb.del<DontFragmentEmptyTag>(entities[i]);
		}
	});

	CHECK(wld.has<DontFragmentEmptyTag>(eKeep));
	CHECK_FALSE(wld.has<DontFragmentEmptyTag>(eDrop));
	expect_exact_entities(wld.query().all<DontFragmentEmptyTag>(), {eKeep});
}

#if GAIA_OBSERVERS_ENABLED
TEST_CASE("Empty DontFragment tag monitor observers") {
	TestWorld twld;

	uint32_t addHits = 0;
	uint32_t delHits = 0;
	ecs::Entity callbackEntity = ecs::EntityBad;
	const auto observer = wld.observer()
														.monitor()
														.all<DontFragmentEmptyTag>()
														.on_each([&](ecs::Iter& it) {
															callbackEntity = it.view<ecs::Entity>()[0];
															if (it.event() == ecs::ObserverEvent::OnAdd)
																++addHits;
															else if (it.event() == ecs::ObserverEvent::OnDel)
																++delHits;
														})
														.entity();

	const auto entity = wld.add();
	wld.add<Position>(entity);
	CHECK(addHits == 0);
	CHECK(delHits == 0);

	wld.add<DontFragmentEmptyTag>(entity);
	CHECK(addHits == 1);
	CHECK(delHits == 0);
	CHECK(callbackEntity == entity);

	wld.add<Scale>(entity);
	CHECK(addHits == 1);
	CHECK(delHits == 0);

	wld.del<DontFragmentEmptyTag>(entity);
	CHECK(addHits == 1);
	CHECK(delHits == 1);
	CHECK(callbackEntity == entity);
	(void)observer;
}
#endif

#if GAIA_ENABLE_ADD_DEL_HOOKS
TEST_CASE("Empty DontFragment tag add and del hooks") {
	TestWorld twld;

	const auto& item = wld.add<DontFragmentEmptyTag>();
	g_emptyDontFragAddHooks = 0;
	g_emptyDontFragDelHooks = 0;
	ecs::ComponentCache::hooks(item).func_add = [](const ecs::World&, const ecs::ComponentCacheItem&, ecs::Entity) {
		++g_emptyDontFragAddHooks;
	};
	ecs::ComponentCache::hooks(item).func_del = [](const ecs::World&, const ecs::ComponentCacheItem&, ecs::Entity) {
		++g_emptyDontFragDelHooks;
	};

	const auto entity = wld.add();
	wld.add<DontFragmentEmptyTag>(entity);
	CHECK(g_emptyDontFragAddHooks == 1);
	CHECK(g_emptyDontFragDelHooks == 0);

	wld.del<DontFragmentEmptyTag>(entity);
	CHECK(g_emptyDontFragAddHooks == 1);
	CHECK(g_emptyDontFragDelHooks == 1);

	wld.add<DontFragmentEmptyTag>(entity);
	const auto copied = wld.copy_ext(entity);
	CHECK(g_emptyDontFragAddHooks == 3);
	CHECK(wld.has<DontFragmentEmptyTag>(copied));
}
#endif

#if GAIA_SYSTEMS_ENABLED
TEST_CASE("Empty DontFragment tag systems") {
	TestWorld twld;

	uint32_t hits = 0;
	wld.system().all<DontFragmentEmptyTag>().all<Position&>().on_each([&](Position& p) {
		p.x += 1.0f;
		++hits;
	});

	const auto eTagged = wld.add();
	const auto ePlain = wld.add();
	wld.add<Position>(eTagged, {1.0f, 0.0f, 0.0f});
	wld.add<Position>(ePlain, {4.0f, 0.0f, 0.0f});
	wld.add<DontFragmentEmptyTag>(eTagged);

	wld.systems_run();
	CHECK(hits == 1);
	CHECK(wld.get<Position>(eTagged).x == doctest::Approx(2.0f));
	CHECK(wld.get<Position>(ePlain).x == doctest::Approx(4.0f));

	wld.del<DontFragmentEmptyTag>(eTagged);
	wld.systems_run();
	CHECK(hits == 1);
	CHECK(wld.get<Position>(eTagged).x == doctest::Approx(2.0f));
}
#endif

TEST_CASE("Empty DontFragment tag copy_ext and copy_n") {
	TestWorld twld;

	const auto& compItem = wld.add<DontFragmentEmptyTag>();
	const auto src = wld.add();
	const auto* pArchetypeBefore = wld.fetch(src).pArchetype;
	wld.add<DontFragmentEmptyTag>(src);
	wld.add<Position>(src, {1.0f, 2.0f, 3.0f});

	const auto dstExt = wld.copy_ext(src);
	CHECK(wld.has<DontFragmentEmptyTag>(src));
	CHECK(wld.has<DontFragmentEmptyTag>(dstExt));
	CHECK(wld.has<Position>(dstExt));
	CHECK_FALSE(wld.fetch(dstExt).pArchetype->has(compItem.entity));

	cnt::darray<ecs::Entity> copies;
	wld.copy_n(src, 3, [&](ecs::Entity entity) {
		copies.push_back(entity);
	});
	CHECK(copies.size() == 3);
	for (const auto entity: copies) {
		CHECK(wld.has<DontFragmentEmptyTag>(entity));
		CHECK(wld.has<Position>(entity));
		CHECK_FALSE(wld.fetch(entity).pArchetype->has(compItem.entity));
	}

	cnt::darray<ecs::Entity> observedCopies;
	wld.copy_ext_n(src, 2, [&](ecs::Entity entity) {
		observedCopies.push_back(entity);
	});
	CHECK(observedCopies.size() == 2);
	for (const auto entity: observedCopies) {
		CHECK(wld.has<DontFragmentEmptyTag>(entity));
		CHECK_FALSE(wld.fetch(entity).pArchetype->has(compItem.entity));
	}

	CHECK(wld.fetch(src).pArchetype != pArchetypeBefore);
}

TEST_CASE("add_n copies non-table type without table values") {
	TestWorld twld;

	const auto parent = wld.add();
	const auto src = wld.add();
	wld.name(src, "NamedAddNSource");
	wld.add<Position>(src, {1.0f, 2.0f, 3.0f});
	wld.add<PositionSparse>(src, {4.0f, 5.0f, 6.0f});
	wld.add<DontFragmentEmptyTag>(src);
	wld.add<DontFragmentPayload>(src, {7.0f});
	wld.child(src, parent);

	uint32_t created = 0;
	wld.add_n(src, 2, [&](ecs::Entity entity) {
		CHECK_FALSE(entity.pair());
		CHECK(wld.has<Position>(entity));
		CHECK(wld.has<PositionSparse>(entity));
		CHECK(wld.get<PositionSparse>(entity).x == doctest::Approx(0.0f));
		CHECK(wld.has<DontFragmentEmptyTag>(entity));
		CHECK(wld.has<DontFragmentPayload>(entity));
		CHECK(wld.get<DontFragmentPayload>(entity).x == doctest::Approx(7.0f));
		CHECK(wld.is_child(entity, parent));
		CHECK(wld.name(entity).empty());
		++created;
	});
	CHECK(created == 2);
	CHECK(wld.get<Position>(src).x == doctest::Approx(1.0f));
	CHECK(wld.get<PositionSparse>(src).x == doctest::Approx(4.0f));
	CHECK(wld.name(src) == "NamedAddNSource");
}

TEST_CASE("Empty DontFragment tag multiple tags on one entity") {
	TestWorld twld;

	const auto& tagA = wld.add<DontFragmentEmptyTag>();
	const auto& tagB = wld.add<DontFragmentEmptyTagB>();
	const auto e = wld.add();
	const auto* pArchetypeBefore = wld.fetch(e).pArchetype;

	wld.add<DontFragmentEmptyTag>(e);
	wld.add<DontFragmentEmptyTagB>(e);
	CHECK(wld.has<DontFragmentEmptyTag>(e));
	CHECK(wld.has<DontFragmentEmptyTagB>(e));
	CHECK(wld.fetch(e).pArchetype == pArchetypeBefore);
	CHECK_FALSE(wld.fetch(e).pArchetype->has(tagA.entity));
	CHECK_FALSE(wld.fetch(e).pArchetype->has(tagB.entity));

	auto qBoth = wld.query().all<DontFragmentEmptyTag>().all<DontFragmentEmptyTagB>();
	CHECK(qBoth.count() == 1);
	expect_exact_entities(qBoth, {e});

	wld.del<DontFragmentEmptyTag>(e);
	CHECK_FALSE(wld.has<DontFragmentEmptyTag>(e));
	CHECK(wld.has<DontFragmentEmptyTagB>(e));
	CHECK(qBoth.count() == 0);
	expect_exact_entities(wld.query().all<DontFragmentEmptyTagB>(), {e});
}

TEST_CASE("Empty DontFragment tag does not consume archetype component capacity") {
	TestWorld twld;

	ecs::Entity ids[ecs::ChunkHeader::MAX_COMPONENTS]{};
	const auto entity = wld.add();
	GAIA_FOR(ecs::ChunkHeader::MAX_COMPONENTS) {
		ids[i] = wld.add();
		wld.add(entity, ids[i]);
	}

	const auto archetypeSize = wld.fetch(entity).pArchetype->ids_view().size();
	CHECK(archetypeSize == ecs::ChunkHeader::MAX_COMPONENTS);

	wld.add<DontFragmentEmptyTag>(entity);
	CHECK(wld.has<DontFragmentEmptyTag>(entity));
	CHECK(wld.fetch(entity).pArchetype->ids_view().size() == archetypeSize);
	GAIA_FOR(ecs::ChunkHeader::MAX_COMPONENTS) CHECK(wld.has(entity, ids[i]));

	const auto many = wld.add();
	const auto sizeBefore = wld.fetch(many).pArchetype->ids_view().size();
	add_empty_dontfrag_tags<0, ecs::ChunkHeader::MAX_COMPONENTS + 1>(wld, many);
	expect_empty_dontfrag_tags<0, ecs::ChunkHeader::MAX_COMPONENTS + 1>(wld, many);
	CHECK(wld.fetch(many).pArchetype->ids_view().size() == sizeBefore);
}

TEST_CASE("Empty DontFragment tag query any, changed, sort, and group") {
	TestWorld twld;

	const auto eats = wld.add();
	const auto carrot = wld.add();
	const auto salad = wld.add();

	const auto eA = wld.add();
	const auto eB = wld.add();
	const auto eC = wld.add();
	wld.add<Position>(eA, {3.0f, 0.0f, 0.0f});
	wld.add<Position>(eB, {1.0f, 0.0f, 0.0f});
	wld.add<Position>(eC, {2.0f, 0.0f, 0.0f});
	wld.add(eA, ecs::Pair(eats, carrot));
	wld.add(eB, ecs::Pair(eats, carrot));
	wld.add(eC, ecs::Pair(eats, salad));
	wld.add<DontFragmentEmptyTag>(eA);
	wld.add<DontFragmentEmptyTag>(eC);

	auto qAny = wld.query().all<Position>().any<DontFragmentEmptyTag>();
	CHECK(qAny.count() == 3);
	expect_exact_entities(qAny, {eA, eB, eC});

	auto qChanged = wld.query().all<DontFragmentEmptyTag>().changed<DontFragmentEmptyTag>();
	expect_changed_consume_exact(qChanged, {eA, eC});
	expect_changed_consume_exact(qChanged, {});

	wld.del<DontFragmentEmptyTag>(eC);
	expect_changed_consume_exact(qChanged, {});
	wld.add<DontFragmentEmptyTag>(eC);
	expect_changed_consume_exact(qChanged, {eC});

	const auto sortByX = []([[maybe_unused]] const ecs::World& world, const void* pData0, const void* pData1) {
		const auto& p0 = *static_cast<const Position*>(pData0);
		const auto& p1 = *static_cast<const Position*>(pData1);
		return (p0.x < p1.x) ? -1 : ((p0.x > p1.x) ? 1 : 0);
	};
	auto qSorted = wld.query().all<Position>().all<DontFragmentEmptyTag>().sort_by<Position>(sortByX);
	cnt::darray<ecs::Entity> ordered;
	qSorted.each([&](ecs::Entity entity) {
		ordered.push_back(entity);
	});
	CHECK(ordered.size() == 2);
	CHECK(ordered[0] == eC);
	CHECK(ordered[1] == eA);

	auto qGrouped = wld.query().all<Position>().all<DontFragmentEmptyTag>().group_by(eats);
	CHECK(qGrouped.count() == 2);
	qGrouped.group_id(carrot);
	CHECK(qGrouped.count() == 1);
	expect_exact_entities(qGrouped, {eA});
	qGrouped.group_id(salad);
	CHECK(qGrouped.count() == 1);
	expect_exact_entities(qGrouped, {eC});
}

TEST_CASE("Fragmenting sparse neighbor keeps table view_mut") {
	TestWorld twld;

	const auto& sparseItem = wld.add<PositionSparse>();
	CHECK_FALSE(wld.has(sparseItem.entity, ecs::DontFragment));

	const auto e = wld.add();
	wld.add<Position>(e, {1.0f, 2.0f, 3.0f});
	wld.add<PositionSparse>(e, {4.0f, 5.0f, 6.0f});
	CHECK(wld.fetch(e).pArchetype->has(sparseItem.entity));
	CHECK(wld.fetch(e).pArchetype->has(wld.add<Position>().entity));

	auto q = wld.query().all<Position&>();
	uint32_t hits = 0;
	q.each([&](ecs::Iter& it) {
		auto posView = it.view_mut<Position>();
		auto posSView = it.sview_mut<Position>();
		CHECK(posView.data() != nullptr);
		CHECK(posSView.data() != nullptr);
		CHECK(posView.size() == 1);
		CHECK(posSView.size() == 1);
		posView[0].x += 10.0f;
		posSView[0].y += 20.0f;
		++hits;
	});

	CHECK(hits == 1);
	CHECK(wld.get<Position>(e).x == doctest::Approx(11.0f));
	CHECK(wld.get<Position>(e).y == doctest::Approx(22.0f));
	CHECK(wld.get<PositionSparse>(e).x == doctest::Approx(4.0f));
}

TEST_CASE("Sparse prefab instantiate copies sparse payload") {
	SparseTestWorld twld;

	const auto& compItem = wld.add<PositionSparse>();
	const auto prefab = wld.prefab();
	wld.add<PositionSparse>(prefab, {4.0f, 5.0f, 6.0f});

	const auto instance = wld.instantiate(prefab);

	CHECK(wld.has<PositionSparse>(instance));
	CHECK(wld.fetch(instance).pArchetype->has(compItem.entity));

	const auto& pos = wld.get<PositionSparse>(instance);
	CHECK(pos.x == doctest::Approx(4.0f));
	CHECK(pos.y == doctest::Approx(5.0f));
	CHECK(pos.z == doctest::Approx(6.0f));
}

TEST_CASE("Sparse prefab sync keeps copied sparse payload after prefab delete") {
	SparseTestWorld twld;

	const auto prefab = wld.prefab();
	wld.add<PositionSparse>(prefab, {4.0f, 5.0f, 6.0f});
	const auto instance = wld.instantiate(prefab);

	wld.del<PositionSparse>(prefab);
	CHECK(wld.sync(prefab) == 0);

	CHECK(wld.has<PositionSparse>(instance));
	const auto& pos = wld.get<PositionSparse>(instance);
	CHECK(pos.x == doctest::Approx(4.0f));
	CHECK(pos.y == doctest::Approx(5.0f));
	CHECK(pos.z == doctest::Approx(6.0f));
}

TEST_CASE("Chunk-backed query view keeps contiguous data access") {
	TestWorld twld;

	const auto e0 = wld.add();
	const auto e1 = wld.add();
	wld.add<Position>(e0, {1.0f, 2.0f, 3.0f});
	wld.add<Position>(e1, {4.0f, 5.0f, 6.0f});

	auto q = wld.query().all<Position>();
	uint32_t hits = 0;
	q.each([&](ecs::Iter& it) {
		auto posView = it.view<Position>();
		CHECK(posView.data() != nullptr);
		CHECK(posView.size() == 2);
		CHECK(posView.data()[0].x == doctest::Approx(1.0f));
		CHECK(posView.data()[1].x == doctest::Approx(4.0f));
		++hits;
	});
	CHECK(hits == 1);
}

TEST_CASE("Sparse query view does not expose contiguous data") {
	SparseTestWorld twld;

	const auto e = wld.add();
	wld.add<PositionSparse>(e, {1.0f, 2.0f, 3.0f});

	auto q = wld.query().all<PositionSparse>();
	uint32_t hits = 0;
	q.each([&](ecs::Iter& it) {
		auto posView = it.view_any<PositionSparse>();
		CHECK(posView.data() == nullptr);
		CHECK(posView.size() == 1);
		CHECK(posView[0].x == doctest::Approx(1.0f));
		CHECK(posView[0].y == doctest::Approx(2.0f));
		CHECK(posView[0].z == doctest::Approx(3.0f));
		++hits;
	});
	CHECK(hits == 1);
}

TEST_CASE("Chunk-backed sview_mut keeps contiguous data access") {
	TestWorld twld;

	const auto e0 = wld.add();
	const auto e1 = wld.add();
	wld.add<Position>(e0, {1.0f, 2.0f, 3.0f});
	wld.add<Position>(e1, {4.0f, 5.0f, 6.0f});

	auto q = wld.query().all<Position&>();
	uint32_t hits = 0;
	q.each([&](ecs::Iter& it) {
		auto posView = it.sview_mut<Position>();
		auto posTermView = it.sview_mut<Position>(0);
		CHECK(posView.data() != nullptr);
		CHECK(posTermView.data() != nullptr);
		CHECK(posView.data() == posTermView.data());
		CHECK(posView.size() == 2);
		CHECK(posTermView.size() == 2);

		posView[0].x += 10.0f;
		posTermView[1].x += 20.0f;
		++hits;
	});

	CHECK(hits == 1);
	CHECK(wld.get<Position>(e0).x == doctest::Approx(11.0f));
	CHECK(wld.get<Position>(e1).x == doctest::Approx(24.0f));
}

TEST_CASE("Sparse DontFragment runtime-registered component typed object access") {
	TestWorld twld;

	const auto& runtimeComp = add_runtime_component(
			wld, "Runtime_Sparse_Position", (uint32_t)sizeof(Position), ecs::DataStorageType::Sparse,
			(uint32_t)alignof(Position));
	wld.add(runtimeComp.entity, ecs::DontFragment);

	const auto e = wld.add();
	const auto* pArchetypeBefore = wld.fetch(e).pArchetype;

	wld.add(e, runtimeComp.entity, Position{7.0f, 8.0f, 9.0f});
	CHECK(wld.has(e, runtimeComp.entity));
	CHECK(wld.fetch(e).pArchetype == pArchetypeBefore);

	{
		auto posMut = wld.set<Position>(e, runtimeComp.entity);
		posMut = {10.0f, 11.0f, 12.0f};
	}

	const auto& pos = wld.get<Position>(e, runtimeComp.entity);
	CHECK(pos.x == doctest::Approx(10.0f));
	CHECK(pos.y == doctest::Approx(11.0f));
	CHECK(pos.z == doctest::Approx(12.0f));

	wld.del(e, runtimeComp.entity);
	CHECK_FALSE(wld.has(e, runtimeComp.entity));
	CHECK(wld.fetch(e).pArchetype == pArchetypeBefore);
}

TEST_CASE("Sparse runtime-registered component uses sparse storage and still fragments") {
	TestWorld twld;

	const auto& runtimeComp = add_runtime_component(
			wld, "Runtime_Sparse_Fragmenting_Position", (uint32_t)sizeof(Position), ecs::DataStorageType::Sparse,
			(uint32_t)alignof(Position));
	CHECK(wld.has(runtimeComp.entity, ecs::Sparse));

	const auto e = wld.add();
	const auto* pArchetypeBefore = wld.fetch(e).pArchetype;

	wld.add(e, runtimeComp.entity, Position{7.0f, 8.0f, 9.0f});
	CHECK(wld.has(e, runtimeComp.entity));
	CHECK(wld.fetch(e).pArchetype != pArchetypeBefore);
	CHECK(wld.fetch(e).pArchetype->has(runtimeComp.entity));

	{
		auto posMut = wld.set<Position>(e, runtimeComp.entity);
		posMut = {10.0f, 11.0f, 12.0f};
	}

	const auto& pos = wld.get<Position>(e, runtimeComp.entity);
	CHECK(pos.x == doctest::Approx(10.0f));
	CHECK(pos.y == doctest::Approx(11.0f));
	CHECK(pos.z == doctest::Approx(12.0f));

	wld.del(e, runtimeComp.entity);
	CHECK_FALSE(wld.has(e, runtimeComp.entity));
	CHECK(wld.fetch(e).pArchetype == pArchetypeBefore);
}

TEST_CASE("Runtime sparse mode drives add/has/get behavior") {
	TestWorld twld;

	const auto& fragComp = add_runtime_component(
			wld, "Runtime_Sparse_Mode_Frag", (uint32_t)sizeof(Position), ecs::DataStorageType::Sparse,
			(uint32_t)alignof(Position));
	const auto& nonFragComp = add_runtime_component(
			wld, "Runtime_Sparse_Mode_NonFrag", (uint32_t)sizeof(Position), ecs::DataStorageType::Sparse,
			(uint32_t)alignof(Position));
	wld.add(nonFragComp.entity, ecs::DontFragment);

	const auto e = wld.add();
	const auto* pArchetypeBefore = wld.fetch(e).pArchetype;

	wld.add(e, fragComp.entity, Position{1.0f, 2.0f, 3.0f});
	wld.add(e, nonFragComp.entity, Position{4.0f, 5.0f, 6.0f});

	CHECK(wld.has(e, fragComp.entity));
	CHECK(wld.has(e, nonFragComp.entity));
	CHECK(wld.fetch(e).pArchetype != pArchetypeBefore);
	CHECK(wld.fetch(e).pArchetype->has(fragComp.entity));
	CHECK_FALSE(wld.fetch(e).pArchetype->has(nonFragComp.entity));

	const auto& fragPos = wld.get<Position>(e, fragComp.entity);
	CHECK(fragPos.x == doctest::Approx(1.0f));
	CHECK(fragPos.y == doctest::Approx(2.0f));
	CHECK(fragPos.z == doctest::Approx(3.0f));

	const auto& nonFragPos = wld.get<Position>(e, nonFragComp.entity);
	CHECK(nonFragPos.x == doctest::Approx(4.0f));
	CHECK(nonFragPos.y == doctest::Approx(5.0f));
	CHECK(nonFragPos.z == doctest::Approx(6.0f));
}

TEST_CASE("Sparse copy keeps frag and non-frag payloads") {
	TestWorld twld;

	const auto& fragComp = add_runtime_component(
			wld, "Runtime_Sparse_Copy_Frag", (uint32_t)sizeof(Position), ecs::DataStorageType::Sparse,
			(uint32_t)alignof(Position));
	const auto& nonFragComp = add_runtime_component(
			wld, "Runtime_Sparse_Copy_NonFrag", (uint32_t)sizeof(Position), ecs::DataStorageType::Sparse,
			(uint32_t)alignof(Position));
	wld.add(nonFragComp.entity, ecs::DontFragment);

	const auto src = wld.add();
	wld.add(src, fragComp.entity, Position{1.0f, 2.0f, 3.0f});
	wld.add(src, nonFragComp.entity, Position{4.0f, 5.0f, 6.0f});

	const auto dst = wld.copy(src);
	CHECK(wld.has(dst, fragComp.entity));
	CHECK(wld.has(dst, nonFragComp.entity));
	CHECK(wld.fetch(dst).pArchetype->has(fragComp.entity));
	CHECK_FALSE(wld.fetch(dst).pArchetype->has(nonFragComp.entity));

	{
		auto pos = wld.set<Position>(src, fragComp.entity);
		pos = {10.0f, 11.0f, 12.0f};
	}
	{
		auto pos = wld.set<Position>(src, nonFragComp.entity);
		pos = {13.0f, 14.0f, 15.0f};
	}

	const auto& fragPos = wld.get<Position>(dst, fragComp.entity);
	CHECK(fragPos.x == doctest::Approx(1.0f));
	CHECK(fragPos.y == doctest::Approx(2.0f));
	CHECK(fragPos.z == doctest::Approx(3.0f));

	const auto& nonFragPos = wld.get<Position>(dst, nonFragComp.entity);
	CHECK(nonFragPos.x == doctest::Approx(4.0f));
	CHECK(nonFragPos.y == doctest::Approx(5.0f));
	CHECK(nonFragPos.z == doctest::Approx(6.0f));
}

TEST_CASE("Runtime-registered table component can opt into sparse storage via trait") {
	TestWorld twld;

	const auto& runtimeComp = add_runtime_component(
			wld, "Runtime_Table_Position_Becomes_Sparse", (uint32_t)sizeof(Position), ecs::DataStorageType::Table,
			(uint32_t)alignof(Position));
	CHECK(runtimeComp.comp.storage_type() == ecs::DataStorageType::Table);
	CHECK_FALSE(wld.has(runtimeComp.entity, ecs::Sparse));

	wld.add(runtimeComp.entity, ecs::Sparse);
	CHECK(runtimeComp.comp.storage_type() == ecs::DataStorageType::Sparse);
	CHECK(wld.has(runtimeComp.entity, ecs::Sparse));
	CHECK(wld.component_uses_sparse_storage(runtimeComp.entity));

	const auto e = wld.add();
	const auto* pArchetypeBefore = wld.fetch(e).pArchetype;

	wld.add(e, runtimeComp.entity, Position{7.0f, 8.0f, 9.0f});
	CHECK(wld.has(e, runtimeComp.entity));
	CHECK(wld.fetch(e).pArchetype != pArchetypeBefore);
	CHECK(wld.fetch(e).pArchetype->has(runtimeComp.entity));

	const auto& pos = wld.get<Position>(e, runtimeComp.entity);
	CHECK(pos.x == doctest::Approx(7.0f));
	CHECK(pos.y == doctest::Approx(8.0f));
	CHECK(pos.z == doctest::Approx(9.0f));
}

TEST_CASE("DontFragment runtime-registered table component typed object access") {
	TestWorld twld;

	const auto& runtimeComp = add_runtime_component(
			wld, "Runtime_Table_Position", (uint32_t)sizeof(Position), ecs::DataStorageType::Table,
			(uint32_t)alignof(Position));
	wld.add(runtimeComp.entity, ecs::DontFragment);
	CHECK(runtimeComp.comp.storage_type() == ecs::DataStorageType::Sparse);
	CHECK(wld.component_uses_sparse_storage(runtimeComp.entity));

	const auto e = wld.add();
	const auto* pArchetypeBefore = wld.fetch(e).pArchetype;

	wld.add(e, runtimeComp.entity, Position{7.0f, 8.0f, 9.0f});
	CHECK(wld.has(e, runtimeComp.entity));
	CHECK(wld.fetch(e).pArchetype == pArchetypeBefore);
	CHECK_FALSE(wld.fetch(e).pArchetype->has(runtimeComp.entity));

	{
		auto posMut = wld.set<Position>(e, runtimeComp.entity);
		posMut = {10.0f, 11.0f, 12.0f};
	}

	const auto& pos = wld.get<Position>(e, runtimeComp.entity);
	CHECK(pos.x == doctest::Approx(10.0f));
	CHECK(pos.y == doctest::Approx(11.0f));
	CHECK(pos.z == doctest::Approx(12.0f));

	wld.del(e, runtimeComp.entity);
	CHECK_FALSE(wld.has(e, runtimeComp.entity));
	CHECK(wld.fetch(e).pArchetype == pArchetypeBefore);
}

TEST_CASE("Runtime-registered component accessor object setter paths") {
	TestWorld twld;

	const auto& runtimeTableComp = add_runtime_component(
			wld, "Runtime_Table_Position_Setter", (uint32_t)sizeof(Position), ecs::DataStorageType::Table,
			(uint32_t)alignof(Position));
	const auto& runtimeSparseComp = add_runtime_component(
			wld, "Runtime_Sparse_Position_Setter", (uint32_t)sizeof(Position), ecs::DataStorageType::Sparse,
			(uint32_t)alignof(Position));
	wld.add(runtimeSparseComp.entity, ecs::DontFragment);

	const auto e = wld.add();
	wld.add(e, runtimeTableComp.entity, Position{1.0f, 2.0f, 3.0f});
	wld.add(e, runtimeSparseComp.entity, Position{4.0f, 5.0f, 6.0f});

	auto setter = wld.acc_mut(e);
	setter.set<Position>(runtimeTableComp.entity, Position{7.0f, 8.0f, 9.0f});
	setter.sset<Position>(runtimeTableComp.entity, Position{10.0f, 11.0f, 12.0f});
	setter.set<Position>(runtimeSparseComp.entity, Position{13.0f, 14.0f, 15.0f});
	setter.sset<Position>(runtimeSparseComp.entity, Position{16.0f, 17.0f, 18.0f});

	const auto getter = wld.acc(e);
	const auto& tablePos = getter.get<Position>(runtimeTableComp.entity);
	CHECK(tablePos.x == doctest::Approx(10.0f));
	CHECK(tablePos.y == doctest::Approx(11.0f));
	CHECK(tablePos.z == doctest::Approx(12.0f));

	const auto& sparsePos = getter.get<Position>(runtimeSparseComp.entity);
	CHECK(sparsePos.x == doctest::Approx(16.0f));
	CHECK(sparsePos.y == doctest::Approx(17.0f));
	CHECK(sparsePos.z == doctest::Approx(18.0f));
}

TEST_CASE("Sparse DontFragment runtime-registered component is removed on entity delete") {
	TestWorld twld;

	const auto& runtimeComp = add_runtime_component(
			wld, "Runtime_Sparse_Position_Delete", (uint32_t)sizeof(Position), ecs::DataStorageType::Sparse,
			(uint32_t)alignof(Position));
	wld.add(runtimeComp.entity, ecs::DontFragment);

	const auto e = wld.add();
	wld.add(e, runtimeComp.entity, Position{1.0f, 2.0f, 3.0f});
	CHECK(wld.has(e, runtimeComp.entity));

	wld.del(e);
	wld.update();

	CHECK_FALSE(wld.has(e));
}

TEST_CASE("Clear removes table and sparse component state") {
	SparseTestWorld twld;

	const auto e = wld.add();
	wld.add<PositionSparse>(e, {4.0f, 5.0f, 6.0f});
	wld.add<Rotation>(e, {7.0f, 8.0f, 9.0f, 10.0f});

	CHECK(wld.has<PositionSparse>(e));
	CHECK(wld.has<Rotation>(e));

	wld.clear(e);

	CHECK(wld.has(e));
	CHECK_FALSE(wld.has<PositionSparse>(e));
	CHECK_FALSE(wld.has<Rotation>(e));

	wld.add<PositionSparse>(e);

	{
		auto posSparse = wld.set<PositionSparse>(e);
		posSparse = {14.0f, 15.0f, 16.0f};
	}

	CHECK(wld.get<PositionSparse>(e).x == doctest::Approx(14.0f));
}

TEST_CASE("Clear removes non-fragmenting sparse component state") {
	SparseTestWorld twld;
	const auto& compItem = wld.add<PositionSparse>();
	wld.add(compItem.entity, ecs::DontFragment);

	const auto entity = wld.add();
	const auto* pArchetype = wld.fetch(entity).pArchetype;
	wld.add<PositionSparse>(entity, {1.0f, 2.0f, 3.0f});
	CHECK(wld.has<PositionSparse>(entity));
	CHECK(wld.fetch(entity).pArchetype == pArchetype);

	wld.clear(entity);
	CHECK(wld.has(entity));
	CHECK_FALSE(wld.has<PositionSparse>(entity));
	CHECK(wld.fetch(entity).pArchetype == pArchetype);

	wld.add<PositionSparse>(entity, {4.0f, 5.0f, 6.0f});
	CHECK(wld.has<PositionSparse>(entity));
	CHECK(wld.get<PositionSparse>(entity).x == doctest::Approx(4.0f));
}

TEST_CASE("Command buffer sparse lifecycle does not leak across recycled entity slots") {
	SparseTestWorld twld;

	const auto entity = wld.add();
	ecs::CommandBufferST commandBuffer(wld);
	commandBuffer.add<PositionSparse>(entity, {1.0f, 2.0f, 3.0f});
	commandBuffer.commit();

	CHECK(wld.has<PositionSparse>(entity));
	CHECK(wld.get<PositionSparse>(entity).x == doctest::Approx(1.0f));

	commandBuffer.set<PositionSparse>(entity, {10.0f, 11.0f, 12.0f});
	commandBuffer.commit();
	CHECK(wld.get<PositionSparse>(entity).x == doctest::Approx(10.0f));

	commandBuffer.del<PositionSparse>(entity);
	commandBuffer.commit();
	CHECK_FALSE(wld.has<PositionSparse>(entity));

	wld.add<PositionSparse>(entity, {4.0f, 5.0f, 6.0f});
	wld.del(entity);
	wld.update();

	const auto recycled = wld.add();
	CHECK(recycled.id() == entity.id());
	CHECK(recycled.gen() != entity.gen());
	CHECK_FALSE(wld.has<PositionSparse>(recycled));

	commandBuffer.add<PositionSparse>(recycled, {7.0f, 8.0f, 9.0f});
	commandBuffer.commit();
	CHECK(wld.has<PositionSparse>(recycled));
	CHECK(wld.get<PositionSparse>(recycled).x == doctest::Approx(7.0f));
}

TEST_CASE("Command buffer table payload replay addresses exact pair records") {
	TestWorld twld;
	(void)wld.add<Position>();
	const auto relation = wld.add();
	const auto target = wld.add();
	const auto owner = wld.add();
	const auto pair = ecs::Pair(relation, target);
	wld.add(owner, pair);

	ecs::CommandBufferST commandBuffer(wld);
	commandBuffer.add<Position>(pair, Position{1.0f, 2.0f, 3.0f});
	commandBuffer.commit();
	CHECK(wld.has<Position>(pair));
	CHECK_FALSE(wld.has<Position>(relation));
	CHECK(wld.get<Position>(pair).x == doctest::Approx(1.0f));

	commandBuffer.set<Position>(pair, Position{4.0f, 5.0f, 6.0f});
	commandBuffer.commit();
	CHECK(wld.get<Position>(pair).x == doctest::Approx(4.0f));
	CHECK(wld.get<Position>(pair).y == doctest::Approx(5.0f));
	CHECK(wld.get<Position>(pair).z == doctest::Approx(6.0f));

	commandBuffer.del<Position>(pair);
	commandBuffer.commit();
	CHECK_FALSE(wld.has<Position>(pair));
	CHECK_FALSE(wld.has<Position>(relation));
}

TEST_CASE("Command buffer sparse payload replay addresses exact pair records") {
	SparseTestWorld twld;
	const auto relation = wld.add();
	const auto target = wld.add();
	const auto owner = wld.add();
	const auto pair = ecs::Pair(relation, target);
	wld.add(owner, pair);

	ecs::CommandBufferST commandBuffer(wld);
	commandBuffer.add<PositionSparse>(pair, PositionSparse{1.0f, 2.0f, 3.0f});
	commandBuffer.commit();
	CHECK(wld.has<PositionSparse>(pair));
	CHECK_FALSE(wld.has<PositionSparse>(relation));
	CHECK(wld.get<PositionSparse>(pair).x == doctest::Approx(1.0f));

	commandBuffer.set<PositionSparse>(pair, PositionSparse{4.0f, 5.0f, 6.0f});
	commandBuffer.commit();
	CHECK(wld.get<PositionSparse>(pair).x == doctest::Approx(4.0f));
	CHECK(wld.get<PositionSparse>(pair).y == doctest::Approx(5.0f));
	CHECK(wld.get<PositionSparse>(pair).z == doctest::Approx(6.0f));
	CHECK_FALSE(wld.has<PositionSparse>(relation));

	commandBuffer.del<PositionSparse>(pair);
	commandBuffer.commit();
	CHECK_FALSE(wld.has<PositionSparse>(pair));
	CHECK_FALSE(wld.has<PositionSparse>(relation));
}

TEST_CASE("Command buffer delays entity deletion until relationship replay finishes") {
	auto run = [](bool targetBeforeSource) {
		TestWorld twld;
		const auto relation = wld.add();
		ecs::Entity source;
		ecs::Entity target;
		if (targetBeforeSource) {
			target = wld.add();
			source = wld.add();
		} else {
			source = wld.add();
			target = wld.add();
		}
		const auto pair = ecs::Pair(relation, target);

		ecs::CommandBufferST commandBuffer(wld);
		commandBuffer.del(target);
		commandBuffer.add(source, pair);
		commandBuffer.commit();

		CHECK_FALSE(wld.has(target));
		CHECK(wld.has(source));
		CHECK_FALSE(wld.has(source, pair));
		CHECK(wld.target(source, relation) == ecs::EntityBad);
	};

	SUBCASE("target id sorts before source id") {
		run(true);
	}
	SUBCASE("source id sorts before target id") {
		run(false);
	}
}

TEST_CASE("Changed filters track direct and buffered sparse writes") {
	SUBCASE("fragmenting sparse storage") {
		SparseTestWorld twld;
		const auto entity = wld.add();
		wld.add<PositionSparse>(entity, {1.0f, 2.0f, 3.0f});

		auto query = wld.query().all<PositionSparse>().changed<PositionSparse>();
		expect_changed_consume_exact(query, {entity});

		wld.set<PositionSparse>(entity) = {4.0f, 5.0f, 6.0f};
		expect_changed_consume_exact(query, {entity});

		ecs::CommandBufferST commandBuffer(wld);
		commandBuffer.set<PositionSparse>(entity, {7.0f, 8.0f, 9.0f});
		commandBuffer.commit();
		expect_changed_consume_exact(query, {entity});
	}

	SUBCASE("non-fragmenting sparse storage") {
		SparseTestWorld twld;
		const auto& compItem = wld.add<PositionSparse>();
		wld.add(compItem.entity, ecs::DontFragment);
		const auto entity = wld.add();
		wld.add<PositionSparse>(entity, {1.0f, 2.0f, 3.0f});

		auto query = wld.query().all<PositionSparse>().changed<PositionSparse>();
		expect_changed_consume_exact(query, {entity});

		wld.set<PositionSparse>(entity) = {4.0f, 5.0f, 6.0f};
		expect_changed_consume_exact(query, {entity});

		ecs::CommandBufferST commandBuffer(wld);
		commandBuffer.set<PositionSparse>(entity, {7.0f, 8.0f, 9.0f});
		commandBuffer.commit();
		expect_changed_consume_exact(query, {entity});
	}
}

TEST_CASE("EntityContainer cached entity slot across row swap and archetype move") {
	TestWorld twld;

	const auto targetA = wld.add();
	const auto targetB = wld.add();
	const auto sourceA = wld.add();
	const auto sourceB = wld.add();

	wld.child(sourceA, targetA);
	wld.child(sourceB, targetB);

	const auto* pTargetBBefore = wld.fetch(targetB).pEntity;
	CHECK(pTargetBBefore != nullptr);
	if (pTargetBBefore != nullptr)
		CHECK(*pTargetBBefore == targetB);
	CHECK(wld.target(sourceB, ecs::ChildOf) == targetB);

	wld.del(targetA);

	const auto& targetBAfterDelete = wld.fetch(targetB);
	CHECK(targetBAfterDelete.pEntity != nullptr);
	if (targetBAfterDelete.pEntity != nullptr)
		CHECK(*targetBAfterDelete.pEntity == targetB);
	CHECK(wld.target(sourceB, ecs::ChildOf) == targetB);

	wld.add<Position>(targetB, {1.0f, 2.0f, 3.0f});

	const auto& targetBAfterMove = wld.fetch(targetB);
	CHECK(targetBAfterMove.pEntity != nullptr);
	if (targetBAfterMove.pEntity != nullptr)
		CHECK(*targetBAfterMove.pEntity == targetB);
	CHECK(wld.target(sourceB, ecs::ChildOf) == targetB);
}

TEST_CASE("World set writes back when the proxy finishes") {
	SparseTestWorld twld;

	SUBCASE("chunk-backed component") {
		const auto e = wld.add();
		wld.add<Position>(e, {1.0f, 2.0f, 3.0f});

		{
			auto pos = wld.set<Position>(e);
			pos.x = 10.0f;
			pos.y = 11.0f;
			pos.z = 12.0f;

			const auto& current = wld.get<Position>(e);
			CHECK(current.x == doctest::Approx(1.0f));
			CHECK(current.y == doctest::Approx(2.0f));
			CHECK(current.z == doctest::Approx(3.0f));
		}

		const auto& updated = wld.get<Position>(e);
		CHECK(updated.x == doctest::Approx(10.0f));
		CHECK(updated.y == doctest::Approx(11.0f));
		CHECK(updated.z == doctest::Approx(12.0f));
	}

	SUBCASE("sparse component") {
		const auto e = wld.add();
		wld.add<PositionSparse>(e, {4.0f, 5.0f, 6.0f});

		{
			auto pos = wld.set<PositionSparse>(e);
			pos.x = 14.0f;
			pos.y = 15.0f;
			pos.z = 16.0f;

			const auto& current = wld.get<PositionSparse>(e);
			CHECK(current.x == doctest::Approx(4.0f));
			CHECK(current.y == doctest::Approx(5.0f));
			CHECK(current.z == doctest::Approx(6.0f));
		}

		const auto& updated = wld.get<PositionSparse>(e);
		CHECK(updated.x == doctest::Approx(14.0f));
		CHECK(updated.y == doctest::Approx(15.0f));
		CHECK(updated.z == doctest::Approx(16.0f));
	}
}

TEST_CASE("World ids enumeration") {
	SparseTestWorld twld;

	const auto position = wld.add<Position>().entity;
	const auto sparse = wld.add<PositionSparse>().entity;
	const auto emptyTag = wld.add<DontFragmentEmptyTag>().entity;
	const auto payload = wld.add<DontFragmentPayload>().entity;
	const auto relation = wld.add();
	const auto target = wld.add();
	const auto fragmentingPair = ecs::Pair(relation, target);
	const auto parent = wld.add();
	const auto childParent = wld.add();

	SUBCASE("empty entity has no direct ids") {
		const auto e = wld.add();
		cnt::darray<ecs::Entity> ids;
		collect_ids(wld, e, ids);
		CHECK(ids.empty());
		expect_unique_direct_ids(wld, e, ids);
	}

	SUBCASE("complete direct type across storage kinds") {
		const auto e = wld.add();
		wld.add<Position>(e, {1.0f, 2.0f, 3.0f});
		wld.add<PositionSparse>(e, {4.0f, 5.0f, 6.0f});
		wld.add<DontFragmentEmptyTag>(e);
		wld.add<DontFragmentPayload>(e, {7.0f});
		wld.add(e, fragmentingPair);
		wld.parent(e, parent);
		wld.child(e, childParent);
		wld.name(e, "ids_inspector");

		cnt::darray<ecs::Entity> ids;
		collect_ids(wld, e, ids);
		expect_unique_direct_ids(wld, e, ids);

		const auto archIds = wld.fetch(e).pArchetype->ids_view();
		CHECK(ids.size() == archIds.size() + 3);
		GAIA_FOR((uint32_t)archIds.size()) {
			CHECK(ids[i] == archIds[i]);
		}

		CHECK(contains_id(ids, position));
		CHECK(contains_id(ids, sparse));
		CHECK(contains_id(ids, emptyTag));
		CHECK(contains_id(ids, payload));
		CHECK(contains_id(ids, (ecs::Entity)fragmentingPair));
		CHECK(contains_id(ids, (ecs::Entity)ecs::Pair(ecs::Parent, parent)));
		CHECK(contains_id(ids, (ecs::Entity)ecs::Pair(ecs::ChildOf, childParent)));
		CHECK(contains_id(ids, ecs::GAIA_ID(EntityDesc)));

		CHECK(wld.fetch(e).pArchetype->has(position));
		CHECK(wld.fetch(e).pArchetype->has(sparse));
		CHECK(wld.fetch(e).pArchetype->has((ecs::Entity)fragmentingPair));
		CHECK(wld.fetch(e).pArchetype->has((ecs::Entity)ecs::Pair(ecs::ChildOf, childParent)));
		CHECK_FALSE(wld.fetch(e).pArchetype->has(emptyTag));
		CHECK_FALSE(wld.fetch(e).pArchetype->has(payload));
		CHECK_FALSE(wld.fetch(e).pArchetype->has((ecs::Entity)ecs::Pair(ecs::Parent, parent)));

		uint32_t visited = 0;
		wld.ids_if(e, [&](ecs::Entity id) {
			++visited;
			(void)id;
			return visited < 2;
		});
		CHECK(visited == 2);

		wld.del<DontFragmentEmptyTag>(e);
		wld.del(e, ecs::Pair(ecs::Parent, parent));
		collect_ids(wld, e, ids);
		expect_unique_direct_ids(wld, e, ids);
		CHECK_FALSE(contains_id(ids, emptyTag));
		CHECK_FALSE(contains_id(ids, (ecs::Entity)ecs::Pair(ecs::Parent, parent)));
		CHECK(contains_id(ids, payload));
	}

	SUBCASE("EntityIdOptions restrict storage and kind") {
		const auto e = wld.add();
		wld.add<Position>(e, {1.0f, 2.0f, 3.0f});
		wld.add<PositionSparse>(e, {4.0f, 5.0f, 6.0f});
		wld.add<DontFragmentEmptyTag>(e);
		wld.add<DontFragmentPayload>(e, {7.0f});
		wld.add(e, fragmentingPair);
		wld.parent(e, parent);
		wld.child(e, childParent);
		wld.name(e, "ids_options");

		const auto archIds = wld.fetch(e).pArchetype->ids_view();
		cnt::darray<ecs::Entity> ids;

		collect_ids(wld, e, ids, ecs::EntityIdOptions{}.archetype());
		expect_unique_direct_ids(wld, e, ids);
		CHECK(ids.size() == archIds.size());
		GAIA_FOR((uint32_t)archIds.size()) {
			CHECK(ids[i] == archIds[i]);
		}
		CHECK_FALSE(contains_id(ids, emptyTag));
		CHECK_FALSE(contains_id(ids, payload));
		CHECK_FALSE(contains_id(ids, (ecs::Entity)ecs::Pair(ecs::Parent, parent)));

		collect_ids(wld, e, ids, ecs::EntityIdOptions{}.dont_fragment());
		expect_unique_direct_ids(wld, e, ids);
		CHECK(ids.size() == 2);
		CHECK(contains_id(ids, emptyTag));
		CHECK(contains_id(ids, payload));

		collect_ids(wld, e, ids, ecs::EntityIdOptions{}.relation());
		expect_unique_direct_ids(wld, e, ids);
		CHECK(ids.size() == 1);
		CHECK(contains_id(ids, (ecs::Entity)ecs::Pair(ecs::Parent, parent)));

		collect_ids(wld, e, ids, ecs::EntityIdOptions{}.pairs());
		expect_unique_direct_ids(wld, e, ids);
		CHECK(contains_id(ids, (ecs::Entity)fragmentingPair));
		CHECK(contains_id(ids, (ecs::Entity)ecs::Pair(ecs::ChildOf, childParent)));
		CHECK(contains_id(ids, (ecs::Entity)ecs::Pair(ecs::Parent, parent)));
		CHECK_FALSE(contains_id(ids, position));
		CHECK_FALSE(contains_id(ids, emptyTag));

		collect_ids(wld, e, ids, ecs::EntityIdOptions{}.components());
		expect_unique_direct_ids(wld, e, ids);
		CHECK(contains_id(ids, position));
		CHECK(contains_id(ids, sparse));
		CHECK(contains_id(ids, emptyTag));
		CHECK(contains_id(ids, payload));
		CHECK(contains_id(ids, ecs::GAIA_ID(EntityDesc)));
		CHECK_FALSE(contains_id(ids, (ecs::Entity)fragmentingPair));
		CHECK_FALSE(contains_id(ids, (ecs::Entity)ecs::Pair(ecs::Parent, parent)));
		CHECK_FALSE(contains_id(ids, (ecs::Entity)ecs::Pair(ecs::ChildOf, childParent)));

		collect_ids(wld, e, ids, ecs::EntityIdOptions{}.dont_fragment().relation());
		expect_unique_direct_ids(wld, e, ids);
		CHECK(ids.size() == 3);
		CHECK(contains_id(ids, emptyTag));
		CHECK(contains_id(ids, payload));
		CHECK(contains_id(ids, (ecs::Entity)ecs::Pair(ecs::Parent, parent)));

		cnt::darray<ecs::Entity> defaultIds;
		collect_ids(wld, e, defaultIds);
		collect_ids(wld, e, ids, ecs::EntityIdOptions{}.components().pairs());
		CHECK(ids.size() == defaultIds.size());
		GAIA_FOR((uint32_t)ids.size()) {
			CHECK(ids[i] == defaultIds[i]);
		}
	}

	SUBCASE("in() includes inherited ids") {
		wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
		const auto prefab = wld.prefab();
		wld.add<Position>(prefab, {9.0f, 0.0f, 0.0f});
		const auto instance = wld.instantiate(prefab);

		CHECK(wld.has<Position>(instance));
		CHECK_FALSE(wld.has_direct(instance, position));

		cnt::darray<ecs::Entity> ids;
		collect_ids(wld, instance, ids);
		expect_unique_direct_ids(wld, instance, ids);
		CHECK_FALSE(contains_id(ids, position));
		CHECK(contains_id(ids, (ecs::Entity)ecs::Pair(ecs::Is, prefab)));

		collect_ids(wld, instance, ids, ecs::EntityIdOptions{}.components().pairs().in());
		expect_unique_ids(ids);
		CHECK(contains_id(ids, position));
		CHECK(contains_id(ids, (ecs::Entity)ecs::Pair(ecs::Is, prefab)));
		CHECK(wld.has(instance, position));
		CHECK_FALSE(wld.has_direct(instance, position));
	}
}
