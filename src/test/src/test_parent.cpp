#include "test_common.h"

TEST_CASE("Parent - non-fragmenting relation and structural archetype cache") {
	TestWorld twld;

	const auto rootA = wld.add();
	const auto rootB = wld.add();
	const auto e1 = wld.add();
	const auto e2 = wld.add();

	wld.add<Position>(e1);
	wld.add<Position>(e2);

	auto q = wld.query().all<Position>();
	(void)q.count();
	auto& info = q.fetch();
	CHECK(info.cache_archetype_view().size() == 1);

	wld.parent(e1, rootA);
	wld.parent(e2, rootB);

	CHECK(wld.has(e1, ecs::Pair(ecs::Parent, rootA)));
	CHECK(wld.has(e2, ecs::Pair(ecs::Parent, rootB)));
	CHECK(q.count() == 2);
	CHECK(info.cache_archetype_view().size() == 1);
}

TEST_CASE("Parent - targets and sources use non-fragmenting relation storage") {
	TestWorld twld;

	const auto root = wld.add();
	const auto child = wld.add();

	wld.parent(child, root);

	CHECK(wld.has(ecs::Pair(ecs::Parent, root)));
	CHECK(wld.has(child, ecs::Pair(ecs::Parent, root)));
	CHECK(wld.has(child, ecs::Pair(ecs::Parent, ecs::All)));
	CHECK(wld.has(child, ecs::Pair(ecs::All, root)));
	CHECK(wld.has(child, ecs::Pair(ecs::All, ecs::All)));
	CHECK(wld.target(child, ecs::Parent) == root);
	CHECK(wld.relation(child, root) == ecs::Parent);

	cnt::darray<ecs::Entity> targets;
	wld.targets(child, ecs::Parent, [&](ecs::Entity target) {
		targets.push_back(target);
	});
	CHECK(targets.size() == 1);
	CHECK(targets[0] == root);

	cnt::darray<ecs::Entity> sources;
	wld.sources(ecs::Parent, root, [&](ecs::Entity source) {
		sources.push_back(source);
	});
	CHECK(sources.size() == 1);
	CHECK(sources[0] == child);
}

TEST_CASE("Non-fragmenting relation - exact pair sources have independent storage") {
	TestWorld twld;

	const auto relation = wld.add();
	const auto target = wld.add();
	const auto owner = wld.add();
	const auto rootA = wld.add();
	const auto rootB = wld.add();
	const auto pair = ecs::Pair(relation, target);
	wld.add(owner, pair);

	wld.parent(relation, rootA);
	wld.parent(pair, rootB);

	CHECK(wld.target(relation, ecs::Parent) == rootA);
	CHECK(wld.target(pair, ecs::Parent) == rootB);
	CHECK(wld.has(relation, ecs::Pair(ecs::Parent, rootA)));
	CHECK(wld.has(pair, ecs::Pair(ecs::Parent, rootB)));

	cnt::darray<ecs::Entity> rootASources;
	wld.sources(ecs::Parent, rootA, [&](ecs::Entity source) {
		rootASources.push_back(source);
	});
	CHECK(rootASources.size() == 1);
	if (rootASources.size() == 1)
		CHECK(rootASources[0] == relation);

	cnt::darray<ecs::Entity> rootBSources;
	wld.sources(ecs::Parent, rootB, [&](ecs::Entity source) {
		rootBSources.push_back(source);
	});
	CHECK(rootBSources.size() == 1);
	if (rootBSources.size() == 1)
		CHECK(rootBSources[0] == (ecs::Entity)pair);

	CHECK(wld.query().all(ecs::Pair(ecs::Parent, ecs::All)).count() == 2);
	CHECK(wld.query().all(ecs::Pair(ecs::Parent, rootA)).count() == 1);
	CHECK(wld.query().all(ecs::Pair(ecs::Parent, rootB)).count() == 1);

	const auto ordinarySource = wld.add();
	wld.parent(ordinarySource, rootB);
	wld.del(pair, ecs::Pair(ecs::Parent, rootB));
	CHECK(wld.target(ordinarySource, ecs::Parent) == rootB);
	wld.parent(pair, rootB);
	wld.del(ordinarySource, ecs::Pair(ecs::Parent, rootB));
	CHECK(wld.target(pair, ecs::Parent) == rootB);

	wld.del(pair);
	wld.update();
	CHECK_FALSE(wld.has(pair));
	CHECK(wld.target(relation, ecs::Parent) == rootA);
	CHECK(wld.query().all(ecs::Pair(ecs::Parent, ecs::All)).count() == 1);
	CHECK(wld.query().all(ecs::Pair(ecs::Parent, rootB)).count() == 0);
}

TEST_CASE("Non-fragmenting relation - wildcard observer sees add and delete") {
	TestWorld twld;

	const auto relation = wld.add();
	const auto target = wld.add();
	const auto targetB = wld.add();
	const auto source = wld.add();
	wld.add(relation, ecs::Exclusive);
	wld.add(relation, ecs::DontFragment);

	uint32_t addHits = 0;
	uint32_t delHits = 0;
	(void)wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.all(ecs::Pair(relation, ecs::All))
			.on_each([&](ecs::Iter&) {
				++addHits;
			})
			.entity();
	(void)wld.observer()
			.event(ecs::ObserverEvent::OnDel)
			.all(ecs::Pair(relation, ecs::All))
			.on_each([&](ecs::Iter&) {
				++delHits;
			})
			.entity();

	wld.add(source, ecs::Pair(relation, target));
	CHECK(addHits == 1);
	CHECK(delHits == 0);

	wld.del(source, ecs::Pair(relation, target));
	CHECK(addHits == 1);
	CHECK(delHits == 1);

	wld.add(source, ecs::Pair(relation, target));
	wld.add(source, ecs::Pair(relation, targetB));
	CHECK(addHits == 3);
	CHECK(delHits == 2);
}

TEST_CASE("Non-fragmenting relation - wildcard observer sees final builder state") {
	TestWorld twld;

	const auto relation = wld.add();
	const auto target = wld.add();
	const auto source = wld.add();
	wld.add(relation, ecs::Exclusive);
	wld.add(relation, ecs::DontFragment);

	uint32_t hits = 0;
	(void)wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.all<Position>()
			.all(ecs::Pair(relation, ecs::All))
			.on_each([&](ecs::Iter&) {
				++hits;
			})
			.entity();

	{
		auto builder = wld.build(source);
		builder.add(ecs::Pair(relation, target));
		builder.add<Position>();
		builder.commit();
	}

	CHECK(hits == 1);
}

TEST_CASE("Non-fragmenting relation - entity copies preserve direct pair storage") {
	TestWorld twld;

	const auto relation = wld.add();
	const auto target = wld.add();
	const auto source = wld.add();
	const auto pair = ecs::Pair(relation, target);
	wld.add(relation, ecs::Exclusive);
	wld.add(relation, ecs::DontFragment);
	wld.add(source, pair);

	uint32_t addHits = 0;
	(void)wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.all(pair)
			.on_each([&](ecs::Iter& it) {
				addHits += it.size();
			})
			.entity();

	const auto silentCopy = wld.copy(source);
	CHECK(wld.has_direct(silentCopy, pair));
	CHECK(wld.target(silentCopy, relation) == target);
	CHECK(addHits == 0);

	const auto observedCopy = wld.copy_ext(source);
	CHECK(wld.has_direct(observedCopy, pair));
	CHECK(wld.target(observedCopy, relation) == target);
	CHECK(addHits == 1);

	cnt::darray<ecs::Entity> silentCopies;
	wld.copy_n(source, 3, [&](ecs::Entity entity) {
		silentCopies.push_back(entity);
	});
	CHECK(silentCopies.size() == 3);
	for (const auto entity: silentCopies) {
		CHECK(wld.has_direct(entity, pair));
		CHECK(wld.target(entity, relation) == target);
	}
	CHECK(addHits == 1);

	cnt::darray<ecs::Entity> observedCopies;
	wld.copy_ext_n(source, 3, [&](ecs::Entity entity) {
		observedCopies.push_back(entity);
	});
	CHECK(observedCopies.size() == 3);
	for (const auto entity: observedCopies) {
		CHECK(wld.has_direct(entity, pair));
		CHECK(wld.target(entity, relation) == target);
	}
	CHECK(addHits == 4);
}

TEST_CASE("Parent - deleting target deletes children through non-fragmenting relation") {
	TestWorld twld;

	const auto root = wld.add();
	const auto child = wld.add();

	wld.add<Position>(child);
	wld.parent(child, root);

	wld.del(root);
	wld.update();

	CHECK(!wld.has(root));
	CHECK(!wld.has(child));
}

TEST_CASE("Parent - cached query rematches after parent target recycling") {
	TestWorld twld;

	const auto root = wld.add();
	const auto child = wld.add();
	wld.add<Position>(child);
	wld.parent(child, root);

	auto query = wld.query().all<Position>().all(ecs::Pair(ecs::Parent, root));
	CHECK(query.count() == 1);

	wld.del(root);
	wld.update();
	CHECK_FALSE(wld.has(root));
	CHECK_FALSE(wld.has(child));
	CHECK(query.count() == 0);

	ecs::Entity recycledRoot = ecs::EntityBad;
	for (uint32_t i = 0; i < 64 && recycledRoot == ecs::EntityBad; ++i) {
		const auto candidate = wld.add();
		if (candidate.id() == root.id())
			recycledRoot = candidate;
	}
	CHECK(recycledRoot != ecs::EntityBad);
	if (recycledRoot == ecs::EntityBad)
		return;
	CHECK(recycledRoot.gen() != root.gen());

	const auto recycledChild = wld.add();
	wld.add<Position>(recycledChild);
	wld.parent(recycledChild, recycledRoot);

	CHECK(query.count() == 1);
	expect_exact_entities(query, {recycledChild});
}

TEST_CASE("Non-fragmenting relation - deleting target removes source pair when policy is Remove") {
	TestWorld twld;

	const auto relation = wld.add();
	const auto target = wld.add();
	const auto source = wld.add();
	wld.add<Position>(source);

	wld.add(relation, ecs::Exclusive);
	wld.add(relation, ecs::DontFragment);
	wld.add(relation, ecs::Pair(ecs::OnDeleteTarget, ecs::Remove));
	wld.add(source, ecs::Pair(relation, target));

	uint32_t removed = 0;
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnDel)
														.all(ecs::Pair(relation, target))
														.on_each([&](ecs::Iter& it) {
															removed += it.size();
														})
														.entity();
	(void)observer;

	CHECK(wld.has(source, ecs::Pair(relation, target)));
	CHECK(wld.target(source, relation) == target);

	wld.del(target);
	wld.update();

	CHECK(!wld.has(target));
	CHECK(wld.has(source));
	CHECK(wld.has<Position>(source));
	CHECK(!wld.has(source, ecs::Pair(relation, target)));
	CHECK(wld.target(source, relation) == ecs::EntityBad);
	CHECK(removed == 1);
}

TEST_CASE("Non-fragmenting relation - wildcard removal clears source binding") {
	TestWorld twld;

	const auto relation = wld.add();
	const auto target = wld.add();
	const auto source = wld.add();
	wld.add(relation, ecs::Exclusive);
	wld.add(relation, ecs::DontFragment);
	wld.add(source, ecs::Pair(relation, target));
	uint32_t removed = 0;
	(void)wld.observer()
			.event(ecs::ObserverEvent::OnDel)
			.all(ecs::Pair(relation, target))
			.on_each([&](ecs::Iter& it) {
				removed += it.size();
			})
			.entity();

	CHECK(wld.has(source, ecs::Pair(relation, ecs::All)));
	wld.del(source, ecs::Pair(relation, ecs::All));
	CHECK_FALSE(wld.has(source, ecs::Pair(relation, ecs::All)));
	CHECK(removed == 1);
}

TEST_CASE("Non-fragmenting relation - all-relation wildcard removes mixed pair storage") {
	TestWorld twld;

	const auto relationA = wld.add();
	const auto relationB = wld.add();
	const auto fragmentingRelation = wld.add();
	const auto targetA = wld.add();
	const auto targetB = wld.add();
	const auto source = wld.add();
	wld.add(relationA, ecs::Exclusive);
	wld.add(relationA, ecs::DontFragment);
	wld.add(relationB, ecs::Exclusive);
	wld.add(relationB, ecs::DontFragment);
	wld.add(source, ecs::Pair(relationA, targetA));
	wld.add(source, ecs::Pair(relationB, targetB));
	wld.add(source, ecs::Pair(fragmentingRelation, targetA));

	wld.del(source, ecs::Pair(ecs::All, targetA));
	CHECK_FALSE(wld.has(source, ecs::Pair(relationA, targetA)));
	CHECK_FALSE(wld.has(source, ecs::Pair(fragmentingRelation, targetA)));
	CHECK(wld.has(source, ecs::Pair(relationB, targetB)));

	wld.del(source, ecs::Pair(ecs::All, ecs::All));
	CHECK_FALSE(wld.has(source, ecs::Pair(ecs::All, ecs::All)));
}

TEST_CASE("Non-fragmenting relation - wildcard removal observes more pairs than archetype component capacity") {
	TestWorld twld;

	constexpr uint32_t Count = ecs::ChunkHeader::MAX_COMPONENTS + 1;
	const auto source = wld.add();
	uint32_t removed = 0;
	(void)wld.observer()
			.event(ecs::ObserverEvent::OnDel)
			.all(ecs::Pair(ecs::All, ecs::All))
			.on_each([&](ecs::Iter& it) {
				removed += it.size();
			})
			.entity();

	GAIA_FOR(Count) {
		const auto relation = wld.add();
		const auto target = wld.add();
		wld.add(relation, ecs::Exclusive);
		wld.add(relation, ecs::DontFragment);
		wld.add(source, ecs::Pair(relation, target));
	}

	wld.del(source, ecs::Pair(ecs::All, ecs::All));
	CHECK_FALSE(wld.has(source, ecs::Pair(ecs::All, ecs::All)));
	CHECK(removed == 1);
}

TEST_CASE("Non-fragmenting relation - builder adds more pairs than archetype component capacity") {
	TestWorld twld;

	constexpr uint32_t Count = ecs::ChunkHeader::MAX_COMPONENTS + 1;
	cnt::darray_ext<ecs::Entity, Count> relations;
	cnt::darray_ext<ecs::Entity, Count> targets;
	GAIA_FOR(Count) {
		const auto relation = wld.add();
		relations.push_back(relation);
		targets.push_back(wld.add());
		wld.add(relation, ecs::Exclusive);
		wld.add(relation, ecs::DontFragment);
	}

	const auto source = wld.add();
	{
		auto builder = wld.build(source);
		GAIA_FOR(Count) builder.add(ecs::Pair(relations[i], targets[i]));
		builder.commit();
	}

	CHECK(wld.has(source, ecs::Pair(ecs::All, ecs::All)));
	GAIA_FOR(Count) CHECK(wld.has(source, ecs::Pair(relations[i], targets[i])));
}

TEST_CASE("Non-fragmenting relation - deleting target deletes source and emits source pair OnDel") {
	TestWorld twld;

	const auto relation = wld.add();
	const auto target = wld.add();
	const auto source = wld.add();
	wld.add<Position>(source);

	wld.add(relation, ecs::Exclusive);
	wld.add(relation, ecs::DontFragment);
	wld.add(relation, ecs::Pair(ecs::OnDeleteTarget, ecs::Delete));
	wld.add(source, ecs::Pair(relation, target));

	uint32_t removed = 0;
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnDel)
														.all(ecs::Pair(relation, target))
														.on_each([&](ecs::Iter& it) {
															removed += it.size();
														})
														.entity();
	(void)observer;

	CHECK(wld.has(source, ecs::Pair(relation, target)));
	wld.del(target);
	wld.update();

	CHECK_FALSE(wld.has(target));
	CHECK_FALSE(wld.has(source));
	CHECK(removed == 1);
}

TEST_CASE("Non-fragmenting relation - deleting relation entity emits source pair OnDel") {
	TestWorld twld;

	const auto relation = wld.add();
	const auto targetA = wld.add();
	const auto targetB = wld.add();
	const auto sourceA = wld.add();
	const auto sourceB = wld.add();
	wld.add<Position>(sourceA);
	wld.add<Position>(sourceB);

	wld.add(relation, ecs::Exclusive);
	wld.add(relation, ecs::DontFragment);
	wld.add(sourceA, ecs::Pair(relation, targetA));
	wld.add(sourceB, ecs::Pair(relation, targetB));

	uint32_t removedA = 0;
	uint32_t removedB = 0;
	const auto observerA = wld.observer()
														 .event(ecs::ObserverEvent::OnDel)
														 .all(ecs::Pair(relation, targetA))
														 .on_each([&](ecs::Iter& it) {
															 removedA += it.size();
														 })
														 .entity();
	const auto observerB = wld.observer()
														 .event(ecs::ObserverEvent::OnDel)
														 .all(ecs::Pair(relation, targetB))
														 .on_each([&](ecs::Iter& it) {
															 removedB += it.size();
														 })
														 .entity();
	(void)observerA;
	(void)observerB;

	wld.del(relation);
	wld.update();

	CHECK_FALSE(wld.has(relation));
	CHECK(wld.has(sourceA));
	CHECK(wld.has(sourceB));
	CHECK_FALSE(wld.has(sourceA, ecs::Pair(relation, targetA)));
	CHECK_FALSE(wld.has(sourceB, ecs::Pair(relation, targetB)));
	CHECK(removedA == 1);
	CHECK(removedB == 1);
}

TEST_CASE("Non-fragmenting relation - recycled self-pair does not retain stale binding") {
	TestWorld twld;

	const auto relation = wld.add();
	wld.add(relation, ecs::Exclusive);
	wld.add(relation, ecs::DontFragment);
	wld.add(relation, ecs::Pair(relation, relation));

	CHECK(wld.has(relation, ecs::Pair(relation, relation)));
	wld.del(relation);
	wld.update();
	CHECK_FALSE(wld.has(relation));

	const auto recycled = wld.add();
	CHECK(recycled.id() == relation.id());
	CHECK(recycled.gen() != relation.gen());
	CHECK_FALSE(wld.has(recycled, ecs::Pair(recycled, recycled)));
}

TEST_CASE("Non-fragmenting relation - reentrant relation deletion keeps pair removal stable") {
	TestWorld twld;

	const auto relation = wld.add();
	const auto targetA = wld.add();
	const auto targetB = wld.add();
	const auto sourceA = wld.add();
	const auto sourceB = wld.add();
	const auto pairA = ecs::Pair(relation, targetA);
	const auto pairB = ecs::Pair(relation, targetB);

	wld.add(relation, ecs::Exclusive);
	wld.add(relation, ecs::DontFragment);
	wld.add(sourceA, pairA);
	wld.add(sourceB, pairB);

	bool reentered = false;
	uint32_t removedA = 0;
	uint32_t removedB = 0;
	const auto observerA = wld.observer()
														 .event(ecs::ObserverEvent::OnDel)
														 .all(pairA)
														 .on_each([&](ecs::Iter& it) {
															 removedA += it.size();
															 if (reentered)
																 return;

															 reentered = true;
															 wld.del(relation);
														 })
														 .entity();
	const auto observerB = wld.observer()
														 .event(ecs::ObserverEvent::OnDel)
														 .all(pairB)
														 .on_each([&](ecs::Iter& it) {
															 removedB += it.size();
														 })
														 .entity();
	(void)observerA;
	(void)observerB;

	wld.del(relation);
	wld.update();

	CHECK(reentered);
	CHECK_FALSE(wld.has(relation));
	CHECK(wld.has(sourceA));
	CHECK(wld.has(sourceB));
	CHECK_FALSE(wld.has(sourceA, pairA));
	CHECK_FALSE(wld.has(sourceB, pairB));
	CHECK_FALSE(wld.has(pairA));
	CHECK_FALSE(wld.has(pairB));
	CHECK(removedA == 1);
	CHECK(removedB == 1);
}

TEST_CASE("Non-fragmenting relation - repeated deletion cleans self and external pairs once") {
	TestWorld twld;

	const auto relation = wld.add();
	const auto incomingRelation = wld.add();
	const auto target = wld.add();
	const auto outgoingSource = wld.add();
	const auto incomingSource = wld.add();
	const auto selfPair = ecs::Pair(relation, relation);
	const auto outgoingPair = ecs::Pair(relation, target);
	const auto incomingPair = ecs::Pair(incomingRelation, relation);

	wld.add(relation, ecs::Exclusive);
	wld.add(relation, ecs::DontFragment);
	wld.add(incomingRelation, ecs::Exclusive);
	wld.add(incomingRelation, ecs::DontFragment);
	wld.add(relation, selfPair);
	wld.add(outgoingSource, outgoingPair);
	wld.add(incomingSource, incomingPair);

	uint32_t selfRemoved = 0;
	uint32_t outgoingRemoved = 0;
	uint32_t incomingRemoved = 0;
	const auto selfObserver = wld.observer()
																.event(ecs::ObserverEvent::OnDel)
																.all(selfPair)
																.on_each([&](ecs::Iter& it) {
																	selfRemoved += it.size();
																})
																.entity();
	const auto outgoingObserver = wld.observer()
																		.event(ecs::ObserverEvent::OnDel)
																		.all(outgoingPair)
																		.on_each([&](ecs::Iter& it) {
																			outgoingRemoved += it.size();
																		})
																		.entity();
	const auto incomingObserver = wld.observer()
																		.event(ecs::ObserverEvent::OnDel)
																		.all(incomingPair)
																		.on_each([&](ecs::Iter& it) {
																			incomingRemoved += it.size();
																		})
																		.entity();
	(void)selfObserver;
	(void)outgoingObserver;
	(void)incomingObserver;

	wld.del(relation);
	wld.del(relation);
	wld.update();

	CHECK_FALSE(wld.has(relation));
	CHECK(wld.has(outgoingSource));
	CHECK(wld.has(incomingSource));
	CHECK_FALSE(wld.has(selfPair));
	CHECK_FALSE(wld.has(outgoingPair));
	CHECK_FALSE(wld.has(incomingPair));
	CHECK(selfRemoved == 1);
	CHECK(outgoingRemoved == 1);
	CHECK(incomingRemoved == 1);

	const auto replacement = wld.add();
	const auto replacementSelfPair = ecs::Pair(replacement, replacement);
	const auto replacementOutgoingPair = ecs::Pair(replacement, target);
	const auto replacementIncomingPair = ecs::Pair(incomingRelation, replacement);
	CHECK(replacement.id() == relation.id());
	CHECK(replacement.gen() != relation.gen());
	CHECK_FALSE(wld.has(replacementSelfPair));
	CHECK_FALSE(wld.has(replacementOutgoingPair));
	CHECK_FALSE(wld.has(replacementIncomingPair));
	CHECK_FALSE(wld.has(outgoingSource, replacementOutgoingPair));
	CHECK_FALSE(wld.has(incomingSource, replacementIncomingPair));
}

TEST_CASE("Non-fragmenting relation - deleting relation entity cascades sources and emits source pair OnDel") {
	TestWorld twld;

	const auto relation = wld.add();
	const auto targetA = wld.add();
	const auto targetB = wld.add();
	const auto sourceA = wld.add();
	const auto sourceB = wld.add();
	const auto pairA = ecs::Pair(relation, targetA);
	const auto pairB = ecs::Pair(relation, targetB);
	wld.add<Position>(sourceA);
	wld.add<Position>(sourceB);

	wld.add(relation, ecs::Exclusive);
	wld.add(relation, ecs::DontFragment);
	wld.add(relation, ecs::Pair(ecs::OnDelete, ecs::Delete));
	wld.add(sourceA, pairA);
	wld.add(sourceB, pairB);

	uint32_t removedA = 0;
	uint32_t removedB = 0;
	bool repeatedSourceDelete = false;
	const auto observerA = wld.observer()
														 .event(ecs::ObserverEvent::OnDel)
														 .all(pairA)
														 .on_each([&](ecs::Iter& it) {
															 removedA += it.size();
															 if (repeatedSourceDelete)
																 return;

															 repeatedSourceDelete = true;
															 wld.del(sourceA);
														 })
														 .entity();
	const auto observerB = wld.observer()
														 .event(ecs::ObserverEvent::OnDel)
														 .all(pairB)
														 .on_each([&](ecs::Iter& it) {
															 removedB += it.size();
														 })
														 .entity();
	(void)observerA;
	(void)observerB;

	CHECK(wld.has(sourceA, pairA));
	CHECK(wld.has(sourceB, pairB));
	CHECK(wld.target(sourceA, relation) == targetA);
	CHECK(wld.target(sourceB, relation) == targetB);

	wld.del(relation);
	wld.del(relation);
	wld.update();

	CHECK_FALSE(wld.has(relation));
	CHECK_FALSE(wld.has(sourceA));
	CHECK_FALSE(wld.has(sourceB));
	CHECK(wld.has(targetA));
	CHECK(wld.has(targetB));
	CHECK_FALSE(wld.has(pairA));
	CHECK_FALSE(wld.has(pairB));
	CHECK(removedA == 1);
	CHECK(removedB == 1);
}

#if !GAIA_ASSERT_ENABLED
TEST_CASE("Non-fragmenting relation - OnDelete Error preserves sources") {
	TestWorld twld;

	const auto relation = wld.add();
	const auto target = wld.add();
	const auto source = wld.add();
	const auto pair = ecs::Pair(relation, target);

	wld.add(relation, ecs::Exclusive);
	wld.add(relation, ecs::DontFragment);
	wld.add(relation, ecs::Pair(ecs::OnDelete, ecs::Error));
	wld.add(source, pair);

	uint32_t removed = 0;
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnDel)
														.all(pair)
														.on_each([&](ecs::Iter& it) {
															removed += it.size();
														})
														.entity();
	(void)observer;

	for (uint32_t attempt = 0; attempt < 2; ++attempt) {
		wld.del(relation);
		wld.update();

		CHECK(wld.has(relation));
		CHECK(wld.has(target));
		CHECK(wld.has(source));
		CHECK(wld.has(source, pair));
		CHECK(wld.has(pair));
		CHECK(removed == 0);
	}
}

TEST_CASE("Non-fragmenting relation - protected relation deletion preserves sources") {
	TestWorld twld;

	const auto relation = wld.add();
	const auto target = wld.add();
	const auto source = wld.add();
	const auto guardRelation = wld.add();
	const auto guardSource = wld.add();
	const auto pair = ecs::Pair(relation, target);
	const auto guardPair = ecs::Pair(guardRelation, relation);

	wld.add(relation, ecs::Exclusive);
	wld.add(relation, ecs::DontFragment);
	wld.add(relation, ecs::Pair(ecs::OnDelete, ecs::Delete));
	wld.add(source, pair);

	wld.add(guardRelation, ecs::Pair(ecs::OnDeleteTarget, ecs::Error));
	wld.add(guardSource, guardPair);

	uint32_t removed = 0;
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnDel)
														.all(pair)
														.on_each([&](ecs::Iter& it) {
															removed += it.size();
														})
														.entity();
	(void)observer;

	for (uint32_t attempt = 0; attempt < 2; ++attempt) {
		wld.del(relation);
		wld.update();

		CHECK(wld.has(relation));
		CHECK(wld.has(target));
		CHECK(wld.has(source));
		CHECK(wld.has(source, pair));
		CHECK(wld.has(pair));
		CHECK(wld.has(guardSource));
		CHECK(wld.has(guardSource, guardPair));
		CHECK(wld.has(guardPair));
		CHECK(removed == 0);
	}
}
#endif

#if GAIA_USE_SAFE_ENTITY
TEST_CASE("Non-fragmenting relation - retained relation deletion preserves sources") {
	TestWorld twld;

	const auto relation = wld.add();
	const auto target = wld.add();
	const auto source = wld.add();
	const auto pair = ecs::Pair(relation, target);

	wld.add(relation, ecs::Exclusive);
	wld.add(relation, ecs::DontFragment);
	wld.add(relation, ecs::Pair(ecs::OnDelete, ecs::Delete));
	wld.add(source, pair);

	uint32_t removed = 0;
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnDel)
														.all(pair)
														.on_each([&](ecs::Iter& it) {
															removed += it.size();
														})
														.entity();
	(void)observer;

	{
		auto safeRelation = ecs::SafeEntity(wld, relation);
		(void)safeRelation;
		wld.del(relation);
		wld.del(relation);
		wld.update();

		CHECK(wld.has(relation));
		CHECK(wld.has(source));
		CHECK(wld.has(source, pair));
		CHECK(wld.has(pair));
		CHECK(removed == 0);
	}
	wld.update();

	CHECK_FALSE(wld.has(relation));
	CHECK_FALSE(wld.has(source));
	CHECK(wld.has(target));
	CHECK_FALSE(wld.has(pair));
	CHECK(removed == 1);
}
#endif

TEST_CASE("Parent - breadth-first traversal on non-fragmenting relation") {
	TestWorld twld;

	const auto root = wld.add();
	const auto child = wld.add();
	const auto leaf = wld.add();

	wld.parent(child, root);
	wld.parent(leaf, child);

	cnt::darray<ecs::Entity> traversed;
	wld.sources_bfs(ecs::Parent, root, [&](ecs::Entity entity) {
		traversed.push_back(entity);
	});

	CHECK(traversed.size() == 2);
	CHECK(traversed[0] == child);
	CHECK(traversed[1] == leaf);
}

TEST_CASE("ChildOf and Parent - breadth-first sources skip disabled subtrees") {
	TestWorld twld;

	const auto childOfRoot = wld.add();
	const auto childOfChild = wld.add();
	const auto childOfLeaf = wld.add();
	const auto parentRoot = wld.add();
	const auto parentChild = wld.add();
	const auto parentLeaf = wld.add();

	wld.child(childOfChild, childOfRoot);
	wld.child(childOfLeaf, childOfChild);
	wld.parent(parentChild, parentRoot);
	wld.parent(parentLeaf, parentChild);

	wld.enable(childOfChild, false);
	wld.enable(parentChild, false);

	cnt::darray<ecs::Entity> childOfDisabled;
	wld.sources_bfs(ecs::ChildOf, childOfRoot, [&](ecs::Entity source) {
		childOfDisabled.push_back(source);
	});
	CHECK(childOfDisabled.size() == 0);

	cnt::darray<ecs::Entity> parentDisabled;
	wld.sources_bfs(ecs::Parent, parentRoot, [&](ecs::Entity source) {
		parentDisabled.push_back(source);
	});
	CHECK(parentDisabled.size() == 0);

	wld.enable(childOfChild, true);
	wld.enable(parentChild, true);

	cnt::darray<ecs::Entity> childOfEnabled;
	wld.sources_bfs(ecs::ChildOf, childOfRoot, [&](ecs::Entity source) {
		childOfEnabled.push_back(source);
	});
	CHECK(childOfEnabled.size() == 2);
	if (childOfEnabled.size() == 2) {
		CHECK(childOfEnabled[0] == childOfChild);
		CHECK(childOfEnabled[1] == childOfLeaf);
	}

	cnt::darray<ecs::Entity> parentEnabled;
	wld.sources_bfs(ecs::Parent, parentRoot, [&](ecs::Entity source) {
		parentEnabled.push_back(source);
	});
	CHECK(parentEnabled.size() == 2);
	if (parentEnabled.size() == 2) {
		CHECK(parentEnabled[0] == parentChild);
		CHECK(parentEnabled[1] == parentLeaf);
	}
}

TEST_CASE("ChildOf and Parent - breadth-first source search skips disabled subtrees") {
	TestWorld twld;

	const auto childOfRoot = wld.add();
	const auto childOfChild = wld.add();
	const auto childOfLeaf = wld.add();
	const auto parentRoot = wld.add();
	const auto parentChild = wld.add();
	const auto parentLeaf = wld.add();

	wld.child(childOfChild, childOfRoot);
	wld.child(childOfLeaf, childOfChild);
	wld.parent(parentChild, parentRoot);
	wld.parent(parentLeaf, parentChild);

	wld.enable(childOfChild, false);
	wld.enable(parentChild, false);

	const bool childOfFoundDisabled = wld.sources_bfs_if(ecs::ChildOf, childOfRoot, [&](ecs::Entity source) {
		return source == childOfLeaf;
	});
	CHECK_FALSE(childOfFoundDisabled);

	const bool parentFoundDisabled = wld.sources_bfs_if(ecs::Parent, parentRoot, [&](ecs::Entity source) {
		return source == parentLeaf;
	});
	CHECK_FALSE(parentFoundDisabled);

	wld.enable(childOfChild, true);
	wld.enable(parentChild, true);

	const bool childOfFoundEnabled = wld.sources_bfs_if(ecs::ChildOf, childOfRoot, [&](ecs::Entity source) {
		return source == childOfLeaf;
	});
	CHECK(childOfFoundEnabled);

	const bool parentFoundEnabled = wld.sources_bfs_if(ecs::Parent, parentRoot, [&](ecs::Entity source) {
		return source == parentLeaf;
	});
	CHECK(parentFoundEnabled);
}

TEST_CASE("ChildOf and Parent - breadth-first source cache updates after reparent") {
	TestWorld twld;

	const auto childOfRootA = wld.add();
	const auto childOfRootB = wld.add();
	const auto childOfChild = wld.add();
	const auto childOfLeaf = wld.add();
	const auto parentRootA = wld.add();
	const auto parentRootB = wld.add();
	const auto parentChild = wld.add();
	const auto parentLeaf = wld.add();

	wld.child(childOfChild, childOfRootA);
	wld.child(childOfLeaf, childOfChild);
	wld.parent(parentChild, parentRootA);
	wld.parent(parentLeaf, parentChild);

	cnt::darray<ecs::Entity> childOfBefore;
	wld.sources_bfs(ecs::ChildOf, childOfRootA, [&](ecs::Entity source) {
		childOfBefore.push_back(source);
	});
	CHECK(childOfBefore.size() == 2);
	if (childOfBefore.size() == 2) {
		CHECK(childOfBefore[0] == childOfChild);
		CHECK(childOfBefore[1] == childOfLeaf);
	}

	cnt::darray<ecs::Entity> parentBefore;
	wld.sources_bfs(ecs::Parent, parentRootA, [&](ecs::Entity source) {
		parentBefore.push_back(source);
	});
	CHECK(parentBefore.size() == 2);
	if (parentBefore.size() == 2) {
		CHECK(parentBefore[0] == parentChild);
		CHECK(parentBefore[1] == parentLeaf);
	}

	wld.child(childOfChild, childOfRootB);
	wld.parent(parentChild, parentRootB);

	cnt::darray<ecs::Entity> childOfOldRoot;
	wld.sources_bfs(ecs::ChildOf, childOfRootA, [&](ecs::Entity source) {
		childOfOldRoot.push_back(source);
	});
	CHECK(childOfOldRoot.size() == 0);

	cnt::darray<ecs::Entity> parentOldRoot;
	wld.sources_bfs(ecs::Parent, parentRootA, [&](ecs::Entity source) {
		parentOldRoot.push_back(source);
	});
	CHECK(parentOldRoot.size() == 0);

	cnt::darray<ecs::Entity> childOfNewRoot;
	wld.sources_bfs(ecs::ChildOf, childOfRootB, [&](ecs::Entity source) {
		childOfNewRoot.push_back(source);
	});
	CHECK(childOfNewRoot.size() == 2);
	if (childOfNewRoot.size() == 2) {
		CHECK(childOfNewRoot[0] == childOfChild);
		CHECK(childOfNewRoot[1] == childOfLeaf);
	}

	cnt::darray<ecs::Entity> parentNewRoot;
	wld.sources_bfs(ecs::Parent, parentRootB, [&](ecs::Entity source) {
		parentNewRoot.push_back(source);
	});
	CHECK(parentNewRoot.size() == 2);
	if (parentNewRoot.size() == 2) {
		CHECK(parentNewRoot[0] == parentChild);
		CHECK(parentNewRoot[1] == parentLeaf);
	}

	CHECK_FALSE(wld.sources_bfs_if(ecs::ChildOf, childOfRootA, [&](ecs::Entity source) {
		return source == childOfLeaf;
	}));
	CHECK_FALSE(wld.sources_bfs_if(ecs::Parent, parentRootA, [&](ecs::Entity source) {
		return source == parentLeaf;
	}));
	CHECK(wld.sources_bfs_if(ecs::ChildOf, childOfRootB, [&](ecs::Entity source) {
		return source == childOfLeaf;
	}));
	CHECK(wld.sources_bfs_if(ecs::Parent, parentRootB, [&](ecs::Entity source) {
		return source == parentLeaf;
	}));
}

TEST_CASE("ChildOf and Parent - breadth-first source cache updates after subtree delete") {
	TestWorld twld;

	const auto childOfRoot = wld.add();
	const auto childOfChild = wld.add();
	const auto childOfLeaf = wld.add();
	const auto parentRoot = wld.add();
	const auto parentChild = wld.add();
	const auto parentLeaf = wld.add();

	wld.child(childOfChild, childOfRoot);
	wld.child(childOfLeaf, childOfChild);
	wld.parent(parentChild, parentRoot);
	wld.parent(parentLeaf, parentChild);

	cnt::darray<ecs::Entity> childOfBefore;
	wld.sources_bfs(ecs::ChildOf, childOfRoot, [&](ecs::Entity source) {
		childOfBefore.push_back(source);
	});
	CHECK(childOfBefore.size() == 2);

	cnt::darray<ecs::Entity> parentBefore;
	wld.sources_bfs(ecs::Parent, parentRoot, [&](ecs::Entity source) {
		parentBefore.push_back(source);
	});
	CHECK(parentBefore.size() == 2);

	wld.del(childOfChild);
	wld.del(parentChild);
	wld.update();

	cnt::darray<ecs::Entity> childOfAfter;
	wld.sources_bfs(ecs::ChildOf, childOfRoot, [&](ecs::Entity source) {
		childOfAfter.push_back(source);
	});
	CHECK(childOfAfter.size() == 0);

	cnt::darray<ecs::Entity> parentAfter;
	wld.sources_bfs(ecs::Parent, parentRoot, [&](ecs::Entity source) {
		parentAfter.push_back(source);
	});
	CHECK(parentAfter.size() == 0);

	CHECK_FALSE(wld.sources_bfs_if(ecs::ChildOf, childOfRoot, [&](ecs::Entity source) {
		return source == childOfLeaf;
	}));
	CHECK_FALSE(wld.sources_bfs_if(ecs::Parent, parentRoot, [&](ecs::Entity source) {
		return source == parentLeaf;
	}));
	CHECK_FALSE(wld.has(childOfChild));
	CHECK_FALSE(wld.has(childOfLeaf));
	CHECK_FALSE(wld.has(parentChild));
	CHECK_FALSE(wld.has(parentLeaf));
}

TEST_CASE("ChildOf and Parent - breadth-first sources use deterministic sibling order") {
	TestWorld twld;

	const auto childOfRoot = wld.add();
	const auto childOfA = wld.add();
	const auto childOfB = wld.add();
	const auto childOfC = wld.add();
	const auto childOfALeaf = wld.add();
	const auto childOfCLeaf = wld.add();
	const auto parentRoot = wld.add();
	const auto parentA = wld.add();
	const auto parentB = wld.add();
	const auto parentC = wld.add();
	const auto parentALeaf = wld.add();
	const auto parentCLeaf = wld.add();

	wld.child(childOfC, childOfRoot);
	wld.child(childOfA, childOfRoot);
	wld.child(childOfB, childOfRoot);
	wld.child(childOfCLeaf, childOfC);
	wld.child(childOfALeaf, childOfA);
	wld.parent(parentC, parentRoot);
	wld.parent(parentA, parentRoot);
	wld.parent(parentB, parentRoot);
	wld.parent(parentCLeaf, parentC);
	wld.parent(parentALeaf, parentA);

	cnt::darray<ecs::Entity> childOfOrder;
	wld.sources_bfs(ecs::ChildOf, childOfRoot, [&](ecs::Entity source) {
		childOfOrder.push_back(source);
	});
	CHECK(childOfOrder.size() == 5);
	if (childOfOrder.size() == 5) {
		CHECK(childOfOrder[0] == childOfA);
		CHECK(childOfOrder[1] == childOfB);
		CHECK(childOfOrder[2] == childOfC);
		CHECK(childOfOrder[3] == childOfALeaf);
		CHECK(childOfOrder[4] == childOfCLeaf);
	}

	cnt::darray<ecs::Entity> parentOrder;
	wld.sources_bfs(ecs::Parent, parentRoot, [&](ecs::Entity source) {
		parentOrder.push_back(source);
	});
	CHECK(parentOrder.size() == 5);
	if (parentOrder.size() == 5) {
		CHECK(parentOrder[0] == parentA);
		CHECK(parentOrder[1] == parentB);
		CHECK(parentOrder[2] == parentC);
		CHECK(parentOrder[3] == parentALeaf);
		CHECK(parentOrder[4] == parentCLeaf);
	}
}

TEST_CASE("Parent - direct query terms are evaluated as entity filters") {
	TestWorld twld;

	const auto rootA = wld.add();
	const auto rootB = wld.add();
	const auto eA = wld.add();
	const auto eB = wld.add();
	const auto eC = wld.add();

	wld.add<Position>(eA);
	wld.add<Position>(eB);
	wld.add<Position>(eC);
	wld.add<Scale>(eC);

	wld.parent(eA, rootA);
	wld.parent(eB, rootB);

	auto qAll = wld.query().all<Position>().all(ecs::Pair(ecs::Parent, rootA));
	CHECK(qAll.count() == 1);
	expect_exact_entities(qAll, {eA});
	CHECK(!qAll.empty());

	auto qNo = wld.query().all<Position>().no(ecs::Pair(ecs::Parent, rootA));
	CHECK(qNo.count() == 2);
	expect_exact_entities(qNo, {eB, eC});

	auto qOr = wld.query().or_(ecs::Pair(ecs::Parent, rootA)).or_<Scale>();
	CHECK(qOr.count() == 2);
	expect_exact_entities(qOr, {eA, eC});
}

TEST_CASE("Parent - clear and recycled child slots do not retain non-fragmenting relations") {
	TestWorld twld;

	const auto root = wld.add();
	const auto child = wld.add();
	wld.add<Position>(child);
	wld.parent(child, root);

	auto exact = wld.query().all<Position>().all(ecs::Pair(ecs::Parent, root));
	auto wildcard = wld.query().all<Position>().all(ecs::Pair(ecs::Parent, ecs::All));
	CHECK(exact.count() == 1);
	CHECK(wildcard.count() == 1);

	wld.clear(child);
	CHECK(wld.has(child));
	CHECK(wld.target(child, ecs::Parent) == ecs::EntityBad);
	CHECK_FALSE(wld.has(child, ecs::Pair(ecs::Parent, root)));
	CHECK(exact.count() == 0);
	CHECK(wildcard.count() == 0);

	wld.add<Position>(child);
	wld.parent(child, root);
	CHECK(exact.count() == 1);
	CHECK(wildcard.count() == 1);

	wld.del(child);
	wld.update();
	CHECK(exact.count() == 0);
	CHECK(wildcard.count() == 0);

	const auto recycled = wld.add();
	CHECK(recycled.id() == child.id());
	CHECK(recycled.gen() != child.gen());
	wld.add<Position>(recycled);
	CHECK(wld.target(recycled, ecs::Parent) == ecs::EntityBad);
	CHECK(exact.count() == 0);
	CHECK(wildcard.count() == 0);

	wld.parent(recycled, root);
	CHECK(exact.count() == 1);
	CHECK(wildcard.count() == 1);
}

TEST_CASE("Parent - duplicate direct set does not dispatch OnAdd again") {
	TestWorld twld;

	const auto root = wld.add();
	const auto child = wld.add();

	uint32_t hits = 0;
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnAdd)
														.all(ecs::Pair(ecs::Parent, root))
														.on_each([&](ecs::Iter& it) {
															hits += it.size();
														})
														.entity();
	(void)observer;

	wld.parent(child, root);
	wld.parent(child, root);

	const auto& cwld = wld;
	CHECK(wld.has(ecs::Pair(ecs::Parent, root)));
	CHECK(wld.is_parent(child, root));
	CHECK_FALSE(wld.is_parent(root, child));
	CHECK(cwld.is_parent(child, root));
	CHECK(hits == 1);
}

TEST_CASE("Parent - direct rebind replaces target without archetype move") {
	TestWorld twld;

	const auto rootA = wld.add();
	const auto rootB = wld.add();
	const auto child = wld.add();
	wld.add<Position>(child);

	wld.parent(child, rootA);
	const auto* pArchetype = wld.fetch(child).pArchetype;
	CHECK(pArchetype != nullptr);
	CHECK(wld.target(child, ecs::Parent) == rootA);

	wld.parent(child, rootB);

	CHECK(wld.has(ecs::Pair(ecs::Parent, rootA)));
	CHECK(wld.has(ecs::Pair(ecs::Parent, rootB)));
	CHECK_FALSE(wld.has(child, ecs::Pair(ecs::Parent, rootA)));
	CHECK(wld.has(child, ecs::Pair(ecs::Parent, rootB)));
	CHECK(wld.target(child, ecs::Parent) == rootB);
	CHECK(wld.relation(child, rootB) == ecs::Parent);
	CHECK(wld.fetch(child).pArchetype == pArchetype);

	uint32_t oldRootSources = 0;
	wld.sources(ecs::Parent, rootA, [&](ecs::Entity source) {
		(void)source;
		++oldRootSources;
	});
	CHECK(oldRootSources == 0);

	cnt::darray<ecs::Entity> newRootSources;
	wld.sources(ecs::Parent, rootB, [&](ecs::Entity source) {
		newRootSources.push_back(source);
	});
	CHECK(newRootSources.size() == 1);
	CHECK(newRootSources[0] == child);
}

TEST_CASE("Parent - duplicate builder add keeps target and archetype") {
	TestWorld twld;

	const auto root = wld.add();
	const auto child = wld.add();
	wld.add<Position>(child);

	{
		auto builder = wld.build(child);
		builder.add(ecs::Pair(ecs::Parent, root));
		builder.commit();
	}

	const auto* pArchetype = wld.fetch(child).pArchetype;
	CHECK(pArchetype != nullptr);
	CHECK(wld.target(child, ecs::Parent) == root);

	{
		auto builder = wld.build(child);
		builder.add(ecs::Pair(ecs::Parent, root));
		builder.add(ecs::Pair(ecs::Parent, root));
		builder.commit();
	}

	CHECK(wld.has(ecs::Pair(ecs::Parent, root)));
	CHECK(wld.has(child, ecs::Pair(ecs::Parent, root)));
	CHECK(wld.target(child, ecs::Parent) == root);
	CHECK(wld.fetch(child).pArchetype == pArchetype);
}

TEST_CASE("Parent - builder rebind replaces target without archetype move") {
	TestWorld twld;

	const auto rootA = wld.add();
	const auto rootB = wld.add();
	const auto child = wld.add();
	wld.add<Position>(child);

	{
		auto builder = wld.build(child);
		builder.add(ecs::Pair(ecs::Parent, rootA));
		builder.commit();
	}

	const auto* pArchetype = wld.fetch(child).pArchetype;
	CHECK(pArchetype != nullptr);
	CHECK(wld.target(child, ecs::Parent) == rootA);

	{
		auto builder = wld.build(child);
		builder.add(ecs::Pair(ecs::Parent, rootB));
		builder.commit();
	}

	CHECK(wld.has(ecs::Pair(ecs::Parent, rootA)));
	CHECK(wld.has(ecs::Pair(ecs::Parent, rootB)));
	CHECK_FALSE(wld.has(child, ecs::Pair(ecs::Parent, rootA)));
	CHECK(wld.has(child, ecs::Pair(ecs::Parent, rootB)));
	CHECK(wld.target(child, ecs::Parent) == rootB);
	CHECK(wld.relation(child, rootB) == ecs::Parent);
	CHECK(wld.fetch(child).pArchetype == pArchetype);

	uint32_t oldRootSources = 0;
	wld.sources(ecs::Parent, rootA, [&](ecs::Entity source) {
		(void)source;
		++oldRootSources;
	});
	CHECK(oldRootSources == 0);

	cnt::darray<ecs::Entity> newRootSources;
	wld.sources(ecs::Parent, rootB, [&](ecs::Entity source) {
		newRootSources.push_back(source);
	});
	CHECK(newRootSources.size() == 1);
	CHECK(newRootSources[0] == child);
}

TEST_CASE("Non-fragmenting relation - builder rebind emits observer replacement events") {
	TestWorld twld;

	const auto relation = wld.add();
	wld.add(relation, ecs::Exclusive);
	wld.add(relation, ecs::DontFragment);

	const auto targetA = wld.add();
	const auto targetB = wld.add();
	const auto source = wld.add();
	wld.add<Position>(source);

	uint32_t addNew = 0;
	uint32_t delOld = 0;
	const auto addObserver = wld.observer()
															 .event(ecs::ObserverEvent::OnAdd)
															 .all(ecs::Pair(relation, targetB))
															 .on_each([&](ecs::Iter& it) {
																 addNew += it.size();
															 })
															 .entity();
	const auto delObserver = wld.observer()
															 .event(ecs::ObserverEvent::OnDel)
															 .all(ecs::Pair(relation, targetA))
															 .on_each([&](ecs::Iter& it) {
																 delOld += it.size();
															 })
															 .entity();
	(void)addObserver;
	(void)delObserver;

	{
		auto builder = wld.build(source);
		builder.add(ecs::Pair(relation, targetA));
		builder.commit();
	}

	CHECK(addNew == 0);
	CHECK(delOld == 0);

	{
		auto builder = wld.build(source);
		builder.add(ecs::Pair(relation, targetB));
		builder.commit();
	}

	CHECK(addNew == 1);
	CHECK(delOld == 1);
	CHECK_FALSE(wld.has(source, ecs::Pair(relation, targetA)));
	CHECK(wld.has(source, ecs::Pair(relation, targetB)));
}

TEST_CASE("ChildOf and Parent - direct child query returns equivalent entity sets") {
	TestWorld twld;

	const auto childOfRootA = wld.add();
	const auto childOfRootB = wld.add();
	const auto parentRootA = wld.add();
	const auto parentRootB = wld.add();
	const auto childOfA0 = wld.add();
	const auto childOfA1 = wld.add();
	const auto childOfB = wld.add();
	const auto parentA0 = wld.add();
	const auto parentA1 = wld.add();
	const auto parentB = wld.add();

	wld.add<Position>(childOfA0);
	wld.add<Position>(childOfA1);
	wld.add<Position>(childOfB);
	wld.add<Position>(parentA0);
	wld.add<Position>(parentA1);
	wld.add<Position>(parentB);

	wld.child(childOfA0, childOfRootA);
	wld.child(childOfA1, childOfRootA);
	wld.child(childOfB, childOfRootB);
	wld.parent(parentA0, parentRootA);
	wld.parent(parentA1, parentRootA);
	wld.parent(parentB, parentRootB);

	auto childOfQuery = wld.query().all<Position>().all(ecs::Pair(ecs::ChildOf, childOfRootA));
	CHECK(childOfQuery.count() == 2);
	expect_exact_entities(childOfQuery, {childOfA0, childOfA1});

	auto parentQuery = wld.query().all<Position>().all(ecs::Pair(ecs::Parent, parentRootA));
	CHECK(parentQuery.count() == 2);
	expect_exact_entities(parentQuery, {parentA0, parentA1});
}

TEST_CASE("ChildOf and Parent - recursive delete removes nested subtree") {
	TestWorld twld;

	const auto childOfRoot = wld.add();
	const auto childOfChild = wld.add();
	const auto childOfLeaf = wld.add();
	const auto parentRoot = wld.add();
	const auto parentChild = wld.add();
	const auto parentLeaf = wld.add();

	wld.child(childOfChild, childOfRoot);
	wld.child(childOfLeaf, childOfChild);
	wld.parent(parentChild, parentRoot);
	wld.parent(parentLeaf, parentChild);

	wld.del(childOfRoot);
	wld.del(parentRoot);
	wld.update();

	CHECK_FALSE(wld.has(childOfRoot));
	CHECK_FALSE(wld.has(childOfChild));
	CHECK_FALSE(wld.has(childOfLeaf));
	CHECK_FALSE(wld.has(parentRoot));
	CHECK_FALSE(wld.has(parentChild));
	CHECK_FALSE(wld.has(parentLeaf));
}

TEST_CASE("Parent - deep non-fragmenting delete cascade") {
	TestWorld twld;

	constexpr uint32_t Depth = 256;
	ecs::Entity entities[Depth + 1]{};
	entities[0] = wld.add();
	GAIA_FOR(Depth) {
		entities[i + 1] = wld.add();
		wld.parent(entities[i + 1], entities[i]);
	}

	wld.del(entities[0]);
	wld.update();

	for (const auto entity: entities)
		CHECK_FALSE(wld.has(entity));
}

TEST_CASE("Non-fragmenting relation - cyclic delete cascade terminates") {
	TestWorld twld;

	const auto relation = wld.add();
	const auto entityA = wld.add();
	const auto entityB = wld.add();
	wld.add(relation, ecs::Exclusive);
	wld.add(relation, ecs::DontFragment);
	wld.add(relation, ecs::Pair(ecs::OnDeleteTarget, ecs::Delete));
	wld.add(entityA, ecs::Pair(relation, entityB));
	wld.add(entityB, ecs::Pair(relation, entityA));

	wld.del(entityA);
	wld.update();

	CHECK_FALSE(wld.has(entityA));
	CHECK_FALSE(wld.has(entityB));
	CHECK(wld.has(relation));
}

TEST_CASE("ChildOf and Parent - recursive delete removes semantic prefab query seeds") {
	TestWorld twld;

	const auto basePrefab = wld.prefab();
	const auto derivedPrefab = wld.prefab();
	wld.as(derivedPrefab, basePrefab);
	wld.add<Position>(derivedPrefab, {1.0f, 2.0f, 3.0f});

	const auto root = wld.add();
	const auto instance = wld.instantiate(derivedPrefab);
	wld.child(instance, root);

	auto inQuery = wld.query().in(basePrefab);
	auto isQuery = wld.query().is(basePrefab);
	CHECK(inQuery.count() == 1);
	CHECK(isQuery.count() == 1);

	wld.del(root);
	CHECK(inQuery.count() == 0);
	CHECK(isQuery.count() == 0);

	wld.frame_cleanup();

	CHECK_FALSE(wld.valid(root));
	CHECK_FALSE(wld.valid(instance));
	CHECK(inQuery.count() == 0);
	CHECK(isQuery.count() == 0);

	const auto replacement = wld.instantiate(derivedPrefab);
	CHECK(inQuery.count() == 1);
	CHECK(isQuery.count() == 1);
	CHECK(wld.valid(replacement));
}

TEST_CASE("ChildOf and Parent - mixed recursive delete removes branch and leaf siblings") {
	TestWorld twld;

	const auto root = wld.add();
	const auto childOfLeaf = wld.add();
	const auto childOfBranch = wld.add();
	const auto parentGrandchild = wld.add();
	const auto parentLeaf = wld.add();
	const auto parentBranch = wld.add();
	const auto childOfGrandchild = wld.add();

	wld.child(childOfLeaf, root);
	wld.child(childOfBranch, root);
	wld.parent(parentGrandchild, childOfBranch);
	wld.parent(parentLeaf, root);
	wld.parent(parentBranch, root);
	wld.child(childOfGrandchild, parentBranch);

	wld.del(root);
	wld.update();

	CHECK_FALSE(wld.has(root));
	CHECK_FALSE(wld.has(childOfLeaf));
	CHECK_FALSE(wld.has(childOfBranch));
	CHECK_FALSE(wld.has(parentGrandchild));
	CHECK_FALSE(wld.has(parentLeaf));
	CHECK_FALSE(wld.has(parentBranch));
	CHECK_FALSE(wld.has(childOfGrandchild));
}

TEST_CASE("ChildOf and Parent - observer OnAdd and OnDel fire for relation replacement") {
	TestWorld twld;

	const auto childOfRootA = wld.add();
	const auto childOfRootB = wld.add();
	const auto childOfChild = wld.add();
	const auto parentRootA = wld.add();
	const auto parentRootB = wld.add();
	const auto parentChild = wld.add();

	uint32_t childOfAddNew = 0;
	uint32_t childOfDelOld = 0;
	uint32_t parentAddNew = 0;
	uint32_t parentDelOld = 0;

	const auto childOfAddObserver = wld.observer()
																			.event(ecs::ObserverEvent::OnAdd)
																			.all(ecs::Pair(ecs::ChildOf, childOfRootB))
																			.on_each([&](ecs::Iter& it) {
																				childOfAddNew += it.size();
																			})
																			.entity();
	const auto childOfDelObserver = wld.observer()
																			.event(ecs::ObserverEvent::OnDel)
																			.all(ecs::Pair(ecs::ChildOf, childOfRootA))
																			.on_each([&](ecs::Iter& it) {
																				childOfDelOld += it.size();
																			})
																			.entity();
	const auto parentAddObserver = wld.observer()
																		 .event(ecs::ObserverEvent::OnAdd)
																		 .all(ecs::Pair(ecs::Parent, parentRootB))
																		 .on_each([&](ecs::Iter& it) {
																			 parentAddNew += it.size();
																		 })
																		 .entity();
	const auto parentDelObserver = wld.observer()
																		 .event(ecs::ObserverEvent::OnDel)
																		 .all(ecs::Pair(ecs::Parent, parentRootA))
																		 .on_each([&](ecs::Iter& it) {
																			 parentDelOld += it.size();
																		 })
																		 .entity();
	(void)childOfAddObserver;
	(void)childOfDelObserver;
	(void)parentAddObserver;
	(void)parentDelObserver;

	wld.child(childOfChild, childOfRootA);
	wld.parent(parentChild, parentRootA);

	CHECK(childOfAddNew == 0);
	CHECK(childOfDelOld == 0);
	CHECK(parentAddNew == 0);
	CHECK(parentDelOld == 0);

	wld.child(childOfChild, childOfRootB);
	wld.parent(parentChild, parentRootB);

	CHECK(childOfAddNew == 1);
	CHECK(childOfDelOld == 1);
	CHECK(parentAddNew == 1);
	CHECK(parentDelOld == 1);
	CHECK(wld.has(childOfChild, ecs::Pair(ecs::ChildOf, childOfRootB)));
	CHECK(wld.has(parentChild, ecs::Pair(ecs::Parent, parentRootB)));
}

TEST_CASE("ChildOf and Parent - hierarchical name lookup resolves nested children") {
	TestWorld twld;

	const auto childOfRoot = wld.add();
	const auto childOfChild = wld.add();
	const auto childOfLeaf = wld.add();
	const auto parentRoot = wld.add();
	const auto parentChild = wld.add();
	const auto parentLeaf = wld.add();

	wld.name(childOfRoot, "childof_root");
	wld.name(childOfChild, "childof_child");
	wld.name(childOfLeaf, "childof_leaf");
	wld.name(parentRoot, "parent_root");
	wld.name(parentChild, "parent_child");
	wld.name(parentLeaf, "parent_leaf");

	wld.child(childOfChild, childOfRoot);
	wld.child(childOfLeaf, childOfChild);
	wld.parent(parentChild, parentRoot);
	wld.parent(parentLeaf, parentChild);

	CHECK(wld.get("childof_root.childof_child") == childOfChild);
	CHECK(wld.get("childof_root.childof_child.childof_leaf") == childOfLeaf);
	CHECK(wld.get("parent_root.parent_child") == parentChild);
	CHECK(wld.get("parent_root.parent_child.parent_leaf") == parentLeaf);
	CHECK(wld.get("childof_root.parent_child") == ecs::EntityBad);
	CHECK(wld.get("parent_root.childof_child") == ecs::EntityBad);
}
