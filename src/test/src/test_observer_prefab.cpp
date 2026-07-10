#include "test_common.h"

#define TestWorld SparseTestWorld
#include <iterator>

#if GAIA_OBSERVERS_ENABLED

TEST_CASE("Observer - Is pair semantic inheritance") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto mammal = wld.add();
	const auto wolf = wld.add();

	int hits = 0;
	cnt::darr<ecs::Entity> observed;
	const auto observerIs = wld.observer()
															.event(ecs::ObserverEvent::OnAdd)
															.all(ecs::Pair(ecs::Is, animal))
															.on_each([&](ecs::Iter& it) {
																auto entities = it.view<ecs::Entity>();
																GAIA_EACH(it) {
																	++hits;
																	observed.push_back(entities[i]);
																}
															})
															.entity();
	(void)observerIs;

	wld.as(mammal, animal);
	CHECK(hits == 1);
	CHECK(observed.size() == 1);
	if (!observed.empty())
		CHECK(observed[0] == mammal);

	wld.as(wolf, mammal);
	auto& observerIsData = wld.observers().data(observerIs);
	auto& observerQueryInfo = observerIsData.query.fetch();
	const auto& wolfEc = wld.fetch(wolf);
	CHECK(observerIsData.query.matches_any(observerQueryInfo, *wolfEc.pArchetype, ecs::EntitySpan{&wolf, 1}));
	CHECK(hits == 2);
	CHECK(observed.size() == 2);
	if (observed.size() >= 2)
		CHECK(observed[1] == wolf);
}

TEST_CASE("Observer - direct Is pair with direct stored edges") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto mammal = wld.add();
	const auto wolf = wld.add();

	ecs::QueryTermOptions directOpts;
	directOpts.direct();

	int hits = 0;
	cnt::darr<ecs::Entity> observed;
	const auto observerIs = wld.observer()
															.event(ecs::ObserverEvent::OnAdd)
															.all(ecs::Pair(ecs::Is, animal), directOpts)
															.on_each([&](ecs::Iter& it) {
																auto entities = it.view<ecs::Entity>();
																GAIA_EACH(it) {
																	++hits;
																	observed.push_back(entities[i]);
																}
															})
															.entity();
	(void)observerIs;

	wld.as(mammal, animal);
	CHECK(hits == 1);
	CHECK(observed.size() == 1);
	if (!observed.empty())
		CHECK(observed[0] == mammal);

	wld.as(wolf, mammal);
	CHECK(hits == 1);
	CHECK(observed.size() == 1);
}

TEST_CASE("Observer - direct Is pair via QueryInput with direct stored edges") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto mammal = wld.add();
	const auto wolf = wld.add();

	ecs::QueryInput item{};
	item.op = ecs::QueryOpKind::All;
	item.access = ecs::QueryAccess::None;
	item.id = ecs::Pair(ecs::Is, animal);
	item.matchKind = ecs::QueryMatchKind::Direct;

	int hits = 0;
	cnt::darr<ecs::Entity> observed;
	const auto observerIs = wld.observer()
															.event(ecs::ObserverEvent::OnAdd)
															.add(item)
															.on_each([&](ecs::Iter& it) {
																auto entities = it.view<ecs::Entity>();
																GAIA_EACH(it) {
																	++hits;
																	observed.push_back(entities[i]);
																}
															})
															.entity();
	(void)observerIs;

	wld.as(mammal, animal);
	CHECK(hits == 1);
	CHECK(observed.size() == 1);
	if (!observed.empty())
		CHECK(observed[0] == mammal);

	wld.as(wolf, mammal);
	CHECK(hits == 1);
	CHECK(observed.size() == 1);
}

TEST_CASE("Observer - is sugar matches semantic and direct Is terms") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto mammal = wld.add();
	const auto wolf = wld.add();

	int semanticHits = 0;
	const auto obsSemantic = wld.observer()
															 .event(ecs::ObserverEvent::OnAdd)
															 .is(animal)
															 .on_each([&](ecs::Iter&) {
																 ++semanticHits;
															 })
															 .entity();

	int directHits = 0;
	const auto obsDirect = wld.observer()
														 .event(ecs::ObserverEvent::OnAdd)
														 .is(animal, ecs::QueryTermOptions{}.direct())
														 .on_each([&](ecs::Iter&) {
															 ++directHits;
														 })
														 .entity();

	wld.as(mammal, animal);
	CHECK(semanticHits == 1);
	CHECK(directHits == 1);

	wld.as(wolf, mammal);
	CHECK(semanticHits == 2);
	CHECK(directHits == 1);

	(void)obsSemantic;
	(void)obsDirect;
}

TEST_CASE("Observer - in sugar matches descendants but excludes the base entity") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto mammal = wld.add();
	const auto wolf = wld.add();

	int hits = 0;
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnAdd)
														.in(animal)
														.on_each([&](ecs::Iter&) {
															++hits;
														})
														.entity();

	wld.as(mammal, animal);
	CHECK(hits == 1);

	wld.as(wolf, mammal);
	CHECK(hits == 2);

	(void)observer;
}

TEST_CASE("Observer - prefabs are excluded by default and can be matched explicitly") {
	TestWorld twld;

	uint32_t defaultHits = 0;
	const auto obsDefault = wld.observer()
															.event(ecs::ObserverEvent::OnAdd)
															.all<Position>()
															.on_each([&](ecs::Iter&) {
																++defaultHits;
															})
															.entity();

	uint32_t prefabHits = 0;
	const auto obsMatchPrefab = wld.observer()
																	.event(ecs::ObserverEvent::OnAdd)
																	.all<Position>()
																	.match_prefab()
																	.on_each([&](ecs::Iter&) {
																		++prefabHits;
																	})
																	.entity();

	const auto prefab = wld.prefab();
	const auto entity = wld.add();

	wld.add<Position>(prefab, {1, 0, 0});
	wld.add<Position>(entity, {2, 0, 0});

	CHECK(defaultHits == 1);
	CHECK(prefabHits == 2);

	(void)obsDefault;
	(void)obsMatchPrefab;
}

TEST_CASE("Observer - inherited prefab data matches on instantiate") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(prefab, {4, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	uint32_t hits = 0;
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnAdd)
														.all<Position>()
														.on_each([&](ecs::Iter& it) {
															hits += it.size();
														})
														.entity();

	const auto instance = wld.instantiate(prefab);

	CHECK(hits == 1);
	CHECK(wld.has(instance, position));
	CHECK_FALSE(wld.has_direct(instance, position));

	(void)observer;
}

TEST_CASE("Observer - inherited Is data matches when derived entity is linked to a base") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto rabbit = wld.add();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(animal, {4, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	uint32_t hits = 0;
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnAdd)
														.all<Position>()
														.on_each([&](ecs::Iter& it) {
															hits += it.size();
														})
														.entity();

	wld.as(rabbit, animal);

	CHECK(hits == 1);
	CHECK(wld.has(rabbit, position));
	CHECK_FALSE(wld.has_direct(rabbit, position));

	(void)observer;
}

TEST_CASE("Observer - inherited Is override and local ownership") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto rabbit = wld.add();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(animal, {4, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
	wld.as(rabbit, animal);

	uint32_t hits = 0;
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnAdd)
														.all<Position>()
														.on_each([&](ecs::Iter& it) {
															hits += it.size();
														})
														.entity();

	CHECK(wld.override<Position>(rabbit));
	CHECK(hits == 1);

	(void)observer;
}

TEST_CASE("Observer - inherited prefab data matches on instantiate_n") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(prefab, {4, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	uint32_t hits = 0;
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnAdd)
														.all<Position>()
														.on_each([&](ecs::Iter& it) {
															hits += it.size();
														})
														.entity();

	cnt::darray<ecs::Entity> instances;
	wld.instantiate_n(prefab, 5, [&](ecs::Entity instance) {
		instances.push_back(instance);
	});

	CHECK(hits == 5);
	CHECK(instances.size() == 5);
	for (const auto instance: instances) {
		CHECK(wld.has(instance, position));
		CHECK_FALSE(wld.has_direct(instance, position));
	}

	(void)observer;
}

TEST_CASE("Observer - prefab sync copied data matches existing instances") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto instance = wld.instantiate(prefab);

	uint32_t hits = 0;
	ecs::Entity observed = ecs::EntityBad;
	Position pos{};

	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnAdd)
														.all<Position>()
														.on_each([&](ecs::Iter& it) {
															++hits;
															auto entityView = it.view<ecs::Entity>();
															auto posView = it.view<Position>();
															observed = entityView[0];
															pos = posView[0];
														})
														.entity();

	auto prefabBuilder = wld.build(prefab);
	prefabBuilder.add<Position>();
	prefabBuilder.commit();
	wld.set<Position>(prefab) = {7.0f, 8.0f, 9.0f};

	hits = 0;
	observed = ecs::EntityBad;
	pos = {};

	CHECK(wld.sync(prefab) == 1);
	CHECK(hits == 1);
	CHECK(observed == instance);
	CHECK(pos.x == doctest::Approx(7.0f));
	CHECK(pos.y == doctest::Approx(8.0f));
	CHECK(pos.z == doctest::Approx(9.0f));

	(void)observer;
}

TEST_CASE("Observer - prefab sync sparse copied data matches existing instances") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto instance = wld.instantiate(prefab);

	uint32_t hits = 0;
	ecs::Entity observed = ecs::EntityBad;
	PositionSparse pos{};

	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnAdd)
														.all<PositionSparse>()
														.on_each([&](ecs::Iter& it) {
															++hits;
															auto entityView = it.view<ecs::Entity>();
															auto posView = it.view_any<PositionSparse>();
															observed = entityView[0];
															pos = posView[0];
														})
														.entity();

	wld.add<PositionSparse>(prefab, {2.0f, 3.0f, 4.0f});

	hits = 0;
	observed = ecs::EntityBad;
	pos = {};

	CHECK(wld.sync(prefab) == 1);
	CHECK(hits == 1);
	CHECK(observed == instance);
	CHECK(pos.x == doctest::Approx(2.0f));
	CHECK(pos.y == doctest::Approx(3.0f));
	CHECK(pos.z == doctest::Approx(4.0f));

	(void)observer;
}

TEST_CASE("Observer - inherited prefab data matches on delete from prefab") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
	wld.add<Position>(prefab, {4.0f, 5.0f, 6.0f});
	const auto instance = wld.instantiate(prefab);

	uint32_t hits = 0;
	ecs::Entity observed = ecs::EntityBad;
	Position pos{};

	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnDel)
														.all<Position>()
														.on_each([&](ecs::Iter& it) {
															++hits;
															auto entityView = it.view<ecs::Entity>();
															auto posView = it.view_any<Position>(0);
															observed = entityView[0];
															pos = posView[0];
														})
														.entity();

	wld.del<Position>(prefab);

	CHECK(hits == 1);
	CHECK(observed == instance);
	CHECK(pos.x == doctest::Approx(4.0f));
	CHECK(pos.y == doctest::Approx(5.0f));
	CHECK(pos.z == doctest::Approx(6.0f));
	CHECK_FALSE(wld.has<Position>(instance));
	CHECK(wld.sync(prefab) == 0);
	CHECK(hits == 1);

	(void)observer;
}

TEST_CASE("Observer - inherited sparse prefab data matches on delete from prefab") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto positionSparse = wld.add<PositionSparse>().entity;
	wld.add(positionSparse, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
	wld.add<PositionSparse>(prefab, {2.0f, 3.0f, 4.0f});
	const auto instance = wld.instantiate(prefab);

	uint32_t hits = 0;
	ecs::Entity observed = ecs::EntityBad;
	PositionSparse pos{};

	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnDel)
														.all<PositionSparse>()
														.on_each([&](ecs::Iter& it) {
															++hits;
															auto entityView = it.view<ecs::Entity>();
															auto posView = it.view_any<PositionSparse>(0);
															observed = entityView[0];
															pos = posView[0];
														})
														.entity();

	wld.del<PositionSparse>(prefab);

	CHECK(hits == 1);
	CHECK(observed == instance);
	CHECK(pos.x == doctest::Approx(2.0f));
	CHECK(pos.y == doctest::Approx(3.0f));
	CHECK(pos.z == doctest::Approx(4.0f));
	CHECK_FALSE(wld.has<PositionSparse>(instance));
	CHECK(wld.sync(prefab) == 0);
	CHECK(hits == 1);

	(void)observer;
}

TEST_CASE("Observer - inherited prefab removal does not fire when another source still provides the term") {
	TestWorld twld;

	const auto root = wld.prefab();
	const auto branchA = wld.prefab();
	const auto branchB = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	wld.as(branchA, root);
	wld.as(branchB, root);
	wld.add<Position>(branchA, {1.0f, 2.0f, 3.0f});
	wld.add<Position>(branchB, {4.0f, 5.0f, 6.0f});

	const auto instance = wld.instantiate(branchA);
	wld.as(instance, branchB);

	uint32_t hits = 0;
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnDel)
														.all<Position>()
														.on_each([&](ecs::Iter&) {
															++hits;
														})
														.entity();

	wld.del<Position>(branchA);

	CHECK(hits == 0);
	CHECK(wld.has<Position>(instance));
	const auto& pos = wld.get<Position>(instance);
	CHECK(pos.x == doctest::Approx(4.0f));
	CHECK(pos.y == doctest::Approx(5.0f));
	CHECK(pos.z == doctest::Approx(6.0f));

	(void)observer;
}

TEST_CASE("Observer - inherited prefab removal triggers no<T> OnDel") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
	wld.add<Position>(prefab, {9.0f, 8.0f, 7.0f});
	const auto instance = wld.instantiate(prefab);

	uint32_t hits = 0;
	ecs::Entity observed = ecs::EntityBad;

	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnDel)
														.no<Position>()
														.on_each([&](ecs::Iter& it) {
															++hits;
															auto entityView = it.view<ecs::Entity>();
															observed = entityView[0];
														})
														.entity();

	wld.del<Position>(prefab);

	CHECK(hits == 1);
	CHECK(observed == instance);
	CHECK_FALSE(wld.has<Position>(instance));

	(void)observer;
}

TEST_CASE("Observer - inherited prefab removal typed OnDel payload") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
	wld.add<Position>(prefab, {11.0f, 12.0f, 13.0f});
	const auto instance = wld.instantiate(prefab);

	uint32_t hits = 0;
	Position observedPos{};
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnDel)
														.all<const Position>()
														.on_each([&](const Position& pos) {
															++hits;
															observedPos = pos;
														})
														.entity();

	wld.del<Position>(prefab);

	CHECK(hits == 1);
	CHECK(observedPos.x == doctest::Approx(11.0f));
	CHECK(observedPos.y == doctest::Approx(12.0f));
	CHECK(observedPos.z == doctest::Approx(13.0f));
	CHECK_FALSE(wld.has<Position>(instance));

	(void)observer;
}

TEST_CASE("Observer - prefab sync spawned child matches Parent pair") {
	TestWorld twld;

	const auto rootPrefab = wld.prefab();
	const auto rootInstance = wld.instantiate(rootPrefab);
	const auto childPrefab = wld.prefab();

	uint32_t hits = 0;
	ecs::Entity observedChild = ecs::EntityBad;

	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnAdd)
														.all(ecs::Pair(ecs::Parent, rootInstance))
														.on_each([&](ecs::Iter& it) {
															++hits;
															auto entityView = it.view<ecs::Entity>();
															observedChild = entityView[0];
														})
														.entity();

	auto childBuilder = wld.build(childPrefab);
	childBuilder.add(ecs::Pair(ecs::Parent, rootPrefab));
	childBuilder.commit();

	hits = 0;
	observedChild = ecs::EntityBad;

	CHECK(wld.sync(rootPrefab) == 1);
	CHECK(hits == 1);
	CHECK(observedChild != ecs::EntityBad);
	CHECK(wld.has_direct(observedChild, ecs::Pair(ecs::Parent, rootInstance)));
	CHECK(wld.has_direct(observedChild, ecs::Pair(ecs::Is, childPrefab)));

	(void)observer;
}

TEST_CASE("Observer - prefab sync spawned child matches ChildOf pair") {
	TestWorld twld;

	const auto rootPrefab = wld.prefab();
	const auto rootInstance = wld.instantiate(rootPrefab);
	const auto childPrefab = wld.prefab();

	uint32_t hits = 0;
	ecs::Entity observedChild = ecs::EntityBad;

	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnAdd)
														.all(ecs::Pair(ecs::ChildOf, rootInstance))
														.on_each([&](ecs::Iter& it) {
															++hits;
															auto entityView = it.view<ecs::Entity>();
															observedChild = entityView[0];
														})
														.entity();

	wld.child(childPrefab, rootPrefab);

	hits = 0;
	observedChild = ecs::EntityBad;

	CHECK(wld.sync(rootPrefab) == 1);
	CHECK(hits == 1);
	CHECK(observedChild != ecs::EntityBad);
	CHECK(wld.has_direct(observedChild, ecs::Pair(ecs::ChildOf, rootInstance)));
	CHECK(wld.has_direct(observedChild, ecs::Pair(ecs::Is, childPrefab)));

	(void)observer;
}

TEST_CASE("Observer - instantiate_n parented ChildOf prefab subtree emits hierarchy pairs") {
	TestWorld twld;

	const auto scene = wld.add();
	const auto rootPrefab = wld.prefab();
	const auto childPrefab = wld.prefab();
	const auto leafPrefab = wld.prefab();

	wld.child(childPrefab, rootPrefab);
	wld.child(leafPrefab, childPrefab);
	wld.add<Position>(rootPrefab, {1.0f, 0.0f, 0.0f});
	wld.add<Position>(childPrefab, {2.0f, 0.0f, 0.0f});
	wld.add<Position>(leafPrefab, {3.0f, 0.0f, 0.0f});

	uint32_t rootPairHits = 0;
	uint32_t childOfPairHits = 0;
	cnt::darr<ecs::Entity> roots;
	cnt::darr<ecs::Entity> childOfEntities;

	const auto rootObserver = wld.observer()
																.event(ecs::ObserverEvent::OnAdd)
																.all(ecs::Pair(ecs::Parent, scene))
																.on_each([&](ecs::Iter& it) {
																	rootPairHits += it.size();
																	auto entityView = it.view<ecs::Entity>();
																	GAIA_EACH(it) {
																		roots.push_back(entityView[i]);
																	}
																})
																.entity();
	const auto childOfObserver = wld.observer()
																	 .event(ecs::ObserverEvent::OnAdd)
																	 .all(ecs::Pair(ecs::ChildOf, ecs::All))
																	 .on_each([&](ecs::Iter& it) {
																		 childOfPairHits += it.size();
																		 auto entityView = it.view<ecs::Entity>();
																		 GAIA_EACH(it) {
																			 childOfEntities.push_back(entityView[i]);
																		 }
																	 })
																	 .entity();
	(void)rootObserver;
	(void)childOfObserver;

	wld.instantiate_n(rootPrefab, scene, 3, [&](ecs::Entity root) {
		CHECK(wld.has(root, ecs::Pair(ecs::Parent, scene)));
	});

	CHECK(rootPairHits == 3);
	CHECK(roots.size() == 3);
	CHECK(childOfPairHits == 6);
	CHECK(childOfEntities.size() == 6);

	for (const auto root: roots) {
		const auto child = wld.find_prefab_instance(root, childPrefab);
		const auto leaf = wld.find_prefab_instance(root, leafPrefab);
		CHECK(child != ecs::EntityBad);
		CHECK(leaf != ecs::EntityBad);
		if (child == ecs::EntityBad || leaf == ecs::EntityBad)
			continue;

		CHECK(wld.has(child, ecs::Pair(ecs::ChildOf, root)));
		CHECK(wld.has(leaf, ecs::Pair(ecs::ChildOf, child)));

		bool observedChild = false;
		bool observedLeaf = false;
		for (const auto observed: childOfEntities) {
			observedChild |= observed == child;
			observedLeaf |= observed == leaf;
		}
		CHECK(observedChild);
		CHECK(observedLeaf);
	}
}

TEST_CASE("Observer - prefab sync child removal emits no OnDel") {
	TestWorld twld;

	const auto rootPrefab = wld.prefab();
	const auto childPrefab = wld.prefab();
	wld.parent(childPrefab, rootPrefab);
	const auto rootInstance = wld.instantiate(rootPrefab);

	ecs::Entity childInstance = ecs::EntityBad;
	wld.sources(ecs::Parent, rootInstance, [&](ecs::Entity child) {
		if (wld.has_direct(child, ecs::Pair(ecs::Is, childPrefab)))
			childInstance = child;
	});

	CHECK(childInstance != ecs::EntityBad);
	if (childInstance == ecs::EntityBad)
		return;

	uint32_t hits = 0;
	ecs::Entity observed = ecs::EntityBad;
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnDel)
														.all(ecs::Pair(ecs::Parent, rootInstance))
														.on_each([&](ecs::Iter& it) {
															++hits;
															auto entityView = it.view<ecs::Entity>();
															observed = entityView[0];
														})
														.entity();

	wld.del(childPrefab, ecs::Pair(ecs::Parent, rootPrefab));
	CHECK(wld.sync(rootPrefab) == 0);

	CHECK(hits == 0);
	CHECK(observed == ecs::EntityBad);
	CHECK(wld.has_direct(childInstance, ecs::Pair(ecs::Parent, rootInstance)));

	(void)observer;
}

TEST_CASE("Observer - prefab sync removed copied override emits no OnDel") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	wld.add<Position>(prefab, {4.0f, 5.0f, 6.0f});
	const auto instance = wld.instantiate(prefab);

	uint32_t hits = 0;
	ecs::Entity observed = ecs::EntityBad;
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnDel)
														.all<Position>()
														.on_each([&](ecs::Iter& it) {
															++hits;
															auto entityView = it.view<ecs::Entity>();
															observed = entityView[0];
														})
														.entity();

	wld.del<Position>(prefab);
	CHECK(wld.sync(prefab) == 0);

	CHECK(hits == 0);
	CHECK(observed == ecs::EntityBad);
	CHECK(wld.has<Position>(instance));
	const auto& pos = wld.get<Position>(instance);
	CHECK(pos.x == doctest::Approx(4.0f));
	CHECK(pos.y == doctest::Approx(5.0f));
	CHECK(pos.z == doctest::Approx(6.0f));

	(void)observer;
}

TEST_CASE("Observer - prefab sync removed sparse copied override emits no OnDel") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	wld.add<PositionSparse>(prefab, {4.0f, 5.0f, 6.0f});
	const auto instance = wld.instantiate(prefab);

	uint32_t hits = 0;
	ecs::Entity observed = ecs::EntityBad;
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnDel)
														.all<PositionSparse>()
														.on_each([&](ecs::Iter& it) {
															++hits;
															auto entityView = it.view<ecs::Entity>();
															observed = entityView[0];
														})
														.entity();

	wld.del<PositionSparse>(prefab);
	CHECK(wld.sync(prefab) == 0);

	CHECK(hits == 0);
	CHECK(observed == ecs::EntityBad);
	CHECK(wld.has<PositionSparse>(instance));
	const auto& pos = wld.get<PositionSparse>(instance);
	CHECK(pos.x == doctest::Approx(4.0f));
	CHECK(pos.y == doctest::Approx(5.0f));
	CHECK(pos.z == doctest::Approx(6.0f));

	(void)observer;
}

TEST_CASE("Observer - del runtime sparse id payload") {
	TestWorld twld;

	const auto sparseId = wld.add<PositionSparse>().entity;

	uint32_t hits = 0;
	ecs::Entity observedEntity = ecs::EntityBad;
	PositionSparse pos{};

	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnDel)
														.all(sparseId)
														.on_each([&](ecs::Iter& it) {
															++hits;
															auto entityView = it.view<ecs::Entity>();
															auto posView = it.view_any<PositionSparse>(0);
															observedEntity = entityView[0];
															pos = posView[0];
														})
														.entity();

	const auto e = wld.add();
	wld.add<PositionSparse>(e, {21.0f, 22.0f, 23.0f});

	hits = 0;
	observedEntity = ecs::EntityBad;
	pos = {};

	wld.del(e, sparseId);

	CHECK(hits == 1);
	CHECK(observedEntity == e);
	CHECK(pos.x == doctest::Approx(21.0f));
	CHECK(pos.y == doctest::Approx(22.0f));
	CHECK(pos.z == doctest::Approx(23.0f));

	(void)observer;
}

TEST_CASE("Observer - prefab sparse override data matches on instantiate") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	wld.add<PositionSparse>(prefab, {4, 0, 0});

	uint32_t hits = 0;
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnAdd)
														.all<PositionSparse>()
														.on_each([&](ecs::Iter& it) {
															hits += it.size();
														})
														.entity();

	const auto instance = wld.instantiate(prefab);

	CHECK(hits == 1);
	CHECK(wld.has<PositionSparse>(instance));
	(void)observer;
}

TEST_CASE("Observer - prefab sparse override data matches on instantiate_n") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	wld.add<PositionSparse>(prefab, {4, 0, 0});

	uint32_t hits = 0;
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnAdd)
														.all<PositionSparse>()
														.on_each([&](ecs::Iter& it) {
															hits += it.size();
														})
														.entity();

	cnt::darray<ecs::Entity> instances;
	wld.instantiate_n(prefab, 5, [&](ecs::Entity instance) {
		instances.push_back(instance);
	});

	CHECK(hits == 5);
	CHECK(instances.size() == 5);
	for (const auto instance: instances)
		CHECK(wld.has<PositionSparse>(instance));

	(void)observer;
}

TEST_CASE("Observer - add(QueryInput) registration and fast path") {
	TestWorld twld;

	const auto positionTerm = wld.add<Position>().entity;

	ecs::QueryInput simpleItem{};
	simpleItem.op = ecs::QueryOpKind::All;
	simpleItem.access = ecs::QueryAccess::Read;
	simpleItem.id = positionTerm;

	uint32_t hits = 0;
	const auto observerSimple = wld.observer()
																	.event(ecs::ObserverEvent::OnAdd)
																	.add(simpleItem)
																	.on_each([&hits](ecs::Iter&) {
																		++hits;
																	})
																	.entity();

	const auto& dataSimple = wld.observers().data(observerSimple);
	CHECK(dataSimple.plan.fastPath == ecs::ObserverPlan::FastPath::SinglePositiveTerm);

	const auto e = wld.add();
	wld.add<Position>(e, {});
	CHECK(hits == 1);

	ecs::QueryInput srcItem{};
	srcItem.op = ecs::QueryOpKind::All;
	srcItem.access = ecs::QueryAccess::Read;
	srcItem.id = positionTerm;
	srcItem.entSrc = wld.add();

	const auto observerSrc =
			wld.observer().event(ecs::ObserverEvent::OnAdd).add(srcItem).on_each([](ecs::Iter&) {}).entity();

	const auto& dataSrc = wld.observers().data(observerSrc);
	CHECK(dataSrc.plan.fastPath == ecs::ObserverPlan::FastPath::Disabled);
}

TEST_CASE("Observer - single negative fast path runtime") {
	TestWorld twld;

	uint32_t onAddHits = 0;
	uint32_t onDelHits = 0;

	const auto observerOnAddNoPos = wld.observer()
																			.event(ecs::ObserverEvent::OnAdd)
																			.no<Position>()
																			.on_each([&onAddHits](ecs::Iter&) {
																				++onAddHits;
																			})
																			.entity();
	const auto observerOnDelNoPos = wld.observer()
																			.event(ecs::ObserverEvent::OnDel)
																			.no<Position>()
																			.on_each([&onDelHits](ecs::Iter&) {
																				++onDelHits;
																			})
																			.entity();

	const auto& dataOnAddNoPos = wld.observers().data(observerOnAddNoPos);
	const auto& dataOnDelNoPos = wld.observers().data(observerOnDelNoPos);
	CHECK(dataOnAddNoPos.plan.fastPath == ecs::ObserverPlan::FastPath::SingleNegativeTerm);
	CHECK(dataOnDelNoPos.plan.fastPath == ecs::ObserverPlan::FastPath::SingleNegativeTerm);

	const auto e = wld.add();
	wld.add<Position>(e, {});
	CHECK(onAddHits == 0);
	CHECK(onDelHits == 0);

	wld.del<Position>(e);
	CHECK(onAddHits == 0);
	CHECK(onDelHits == 1);
}

TEST_CASE("Observer - single positive OnDel fast path runtime") {
	TestWorld twld;

	uint32_t onDelHits = 0;
	ecs::Entity observed = ecs::EntityBad;

	const auto observerOnDelPos = wld.observer()
																		.event(ecs::ObserverEvent::OnDel)
																		.all<Position>()
																		.on_each([&](ecs::Iter& it) {
																			++onDelHits;
																			auto entityView = it.view<ecs::Entity>();
																			observed = entityView[0];
																		})
																		.entity();

	const auto& dataOnDelPos = wld.observers().data(observerOnDelPos);
	CHECK(dataOnDelPos.plan.fastPath == ecs::ObserverPlan::FastPath::SinglePositiveTerm);

	const auto e = wld.add();
	wld.add<Position>(e, {});
	onDelHits = 0;
	observed = ecs::EntityBad;

	wld.del<Position>(e);
	CHECK(onDelHits == 1);
	CHECK(observed == e);
}

TEST_CASE("Observer - fixed pair OnDel fast path runtime") {
	TestWorld twld;

	const auto relation = wld.add();
	const auto target = wld.add();
	const auto pair = ecs::Pair(relation, target);

	uint32_t hits = 0;
	ecs::Entity observed = ecs::EntityBad;

	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnDel)
														.all(pair)
														.on_each([&](ecs::Iter& it) {
															++hits;
															auto entityView = it.view<ecs::Entity>();
															observed = entityView[0];
														})
														.entity();

	const auto& data = wld.observers().data(observer);
	CHECK(data.plan.fastPath == ecs::ObserverPlan::FastPath::SinglePositiveTerm);

	const auto e = wld.add();
	wld.add(e, pair);
	hits = 0;
	observed = ecs::EntityBad;

	wld.del(e, pair);
	CHECK(hits == 1);
	CHECK(observed == e);

	(void)observer;
}

TEST_CASE("Observer - deleting prefab entity leaves instances and emits no standalone inherited OnDel") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
	wld.add<Position>(prefab, {31.0f, 32.0f, 33.0f});

	const auto a = wld.instantiate(prefab);
	const auto b = wld.instantiate(prefab);

	uint32_t hits = 0;
	cnt::darray<ecs::Entity> observed;
	observed.reserve(2);

	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnDel)
														.all<Position>()
														.on_each([&](ecs::Iter& it) {
															auto entityView = it.view<ecs::Entity>();
															GAIA_EACH(it) {
																++hits;
																observed.push_back(entityView[i]);
															}
														})
														.entity();

	wld.del(prefab);
	wld.update();

	CHECK(hits == 0);
	CHECK(observed.empty());
	CHECK_FALSE(wld.valid(prefab));
	CHECK(wld.valid(a));
	CHECK(wld.valid(b));

	(void)observer;
}

#endif
