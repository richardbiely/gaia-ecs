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
