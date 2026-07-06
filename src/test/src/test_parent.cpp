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
	CHECK(cwld.parent(child, root));
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
