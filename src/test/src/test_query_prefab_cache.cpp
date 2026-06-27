#include "test_common.h"

#define TestWorld SparseTestWorld

TEST_CASE("Query - Iter is query component access") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto mammal = wld.add();
	const auto rabbit = wld.add();

	wld.as(mammal, animal);
	wld.as(rabbit, mammal);

	wld.add<Position>(animal, {4, 0, 0});
	wld.add<Position>(mammal, {1, 0, 0});
	wld.add<Position>(rabbit, {2, 0, 0});

	float semanticX = 0.0f;
	float directX = 0.0f;

	auto qSemantic = wld.query().all<Position>().is(animal);
	qSemantic.each([&](ecs::Iter& it) {
		auto posView = it.view<Position>();
		GAIA_EACH(it) {
			semanticX += posView[i].x;
		}
	});

	auto qDirect = wld.query().all<Position>().is(animal, ecs::QueryTermOptions{}.direct());
	qDirect.each([&](ecs::Iter& it) {
		auto posView = it.view<Position>();
		GAIA_EACH(it) {
			directX += posView[i].x;
		}
	});

	CHECK(semanticX == doctest::Approx(7.0f));
	CHECK(directX == doctest::Approx(1.0f));
}

TEST_CASE("Query - cached direct Is query without transitive descendants") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto mammal = wld.add();
	wld.add(mammal, ecs::Pair(ecs::Is, animal));

	ecs::QueryTermOptions directOpts;
	directOpts.direct();

	auto q = wld.query().all(ecs::Pair(ecs::Is, animal), directOpts);
	auto& info = q.fetch();
	q.match_all(info);
	CHECK(q.count() == 1);
	CHECK(info.cache_archetype_view().size() == 1);

	const auto wolf = wld.add();
	wld.add(wolf, ecs::Pair(ecs::Is, mammal));
	CHECK(q.count() == 1);
	CHECK(info.cache_archetype_view().size() == 1);

	const auto reptile = wld.add();
	wld.add(reptile, ecs::Pair(ecs::Is, animal));
	CHECK(q.count() == 2);
	expect_exact_entities(q, {mammal, reptile});
	CHECK(info.cache_archetype_view().size() == 1);
}

TEST_CASE("Query - mixed semantic and direct Is terms") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto herbivore = wld.add();
	const auto carnivore = wld.add();
	const auto rabbit = wld.add();
	const auto hare = wld.add();
	const auto wolf = wld.add();

	wld.as(herbivore, animal);
	wld.as(carnivore, animal);
	wld.as(rabbit, herbivore);
	wld.as(hare, herbivore);
	wld.as(wolf, carnivore);

	ecs::QueryTermOptions directOpts;
	directOpts.direct();

	auto qDirectHerbivoreChildren =
			wld.uquery().all(ecs::Pair(ecs::Is, animal)).all(ecs::Pair(ecs::Is, herbivore), directOpts);
	CHECK(qDirectHerbivoreChildren.count() == 2);
	expect_exact_entities(qDirectHerbivoreChildren, {rabbit, hare});

	auto qExcludeDirectHerbivoreChildren =
			wld.uquery().all(ecs::Pair(ecs::Is, animal)).no(ecs::Pair(ecs::Is, herbivore), directOpts);
	CHECK(qExcludeDirectHerbivoreChildren.count() == 4);
	expect_exact_entities(qExcludeDirectHerbivoreChildren, {animal, herbivore, carnivore, wolf});
}

TEST_CASE("Query - direct Is QueryInput item with direct stored edges") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto mammal = wld.add();
	const auto wolf = wld.add();
	wld.add(mammal, ecs::Pair(ecs::Is, animal));
	wld.add(wolf, ecs::Pair(ecs::Is, mammal));

	ecs::QueryInput item{};
	item.op = ecs::QueryOpKind::All;
	item.access = ecs::QueryAccess::None;
	item.id = ecs::Pair(ecs::Is, animal);
	item.matchKind = ecs::QueryMatchKind::Direct;

	auto q = wld.query();
	q.add(item);
	CHECK(q.count() == 1);
	expect_exact_entities(q, {mammal});
}

TEST_CASE("Query - cached query reverse-index revision changes only on membership changes") {
	TestWorld twld;

	auto q = wld.query().all<Position&>().all<Acceleration&>();
	auto& info = q.fetch();
	q.match_all(info);
	const auto revBefore = info.result_cache_rev();

	auto e = wld.add();
	wld.add<Position>(e, {1, 2, 3});
	CHECK(info.result_cache_rev() == revBefore);

	wld.add<Acceleration>(e, {4, 5, 6});
	const auto revAfterMembershipChange = info.result_cache_rev();
	CHECK(revAfterMembershipChange != revBefore);
	CHECK(info.cache_archetype_view().size() == 1);

	info.invalidate_result();
	q.match_all(info);
	CHECK(info.result_cache_rev() == revAfterMembershipChange);
	CHECK(info.cache_archetype_view().size() == 1);
}

TEST_CASE("Query - cached direct typed query excludes prefab archetypes") {
	TestWorld twld;

	const auto entity = wld.add();
	wld.add<Position>(entity, {1, 0, 0});

	const auto prefab = wld.prefab();
	wld.add<Position>(prefab, {10, 0, 0});

	auto q = wld.query().all<Position>();
	auto& info = q.fetch();

	uint32_t count = 0;
	float sum = 0.0f;
	q.each([&](const Position& pos) {
		++count;
		sum += pos.x;
	});

	CHECK(info.result_cache_may_need_prefab_filter());
	CHECK(count == 1);
	CHECK(sum == doctest::Approx(1.0f));
}

TEST_CASE("Query - cached flattened direct typed query maps single callback arg") {
	TestWorld twld;

	constexpr uint32_t EntityCount = 1100;
	GAIA_FOR(EntityCount) {
		const auto parent = wld.add();
		const auto entity = wld.add();
		wld.add<Position>(entity, {1.0f, 0.0f, 0.0f});
		wld.add<Acceleration>(entity, {7.0f, 0.0f, 0.0f});
		wld.add(entity, ecs::Pair(ecs::ChildOf, parent));
	}

	auto q = wld.query().all<Position>().all<Acceleration>();
	uint32_t count = 0;
	float sum = 0.0f;
	q.each([&](const Acceleration& accel) {
		++count;
		sum += accel.x;
	});

	CHECK(count == EntityCount);
	CHECK(sum == doctest::Approx((float)EntityCount * 7.0f));
}

TEST_CASE("Query - cached flattened direct typed query maps multiple callback args") {
	TestWorld twld;

	constexpr uint32_t EntityCount = 1100;
	GAIA_FOR(EntityCount) {
		const auto parent = wld.add();
		const auto entity = wld.add();
		wld.add<Position>(entity, {1.0f, 0.0f, 0.0f});
		wld.add<Acceleration>(entity, {3.0f, 0.0f, 0.0f});
		wld.add<Rotation>(entity, {5.0f, 0.0f, 0.0f, 1.0f});
		wld.add(entity, ecs::Pair(ecs::ChildOf, parent));
	}

	auto q = wld.query().all<Position>().all<Acceleration>().all<Rotation>();
	uint32_t count = 0;
	float sum = 0.0f;
	q.each([&](const Rotation& rot, const Acceleration& accel) {
		++count;
		sum += rot.x + accel.x;
	});

	CHECK(count == EntityCount);
	CHECK(sum == doctest::Approx((float)EntityCount * 8.0f));
}

TEST_CASE("Query - cached dynamic query after input changes") {
	TestWorld twld;

	auto rel = wld.add();
	auto tgtA = wld.add();
	auto tgtB = wld.add();

	auto eA = wld.add();
	wld.add(eA, ecs::Pair(rel, tgtA));

	auto eB = wld.add();
	wld.add(eB, ecs::Pair(rel, tgtB));

	auto q = wld.query().all(ecs::Pair(rel, ecs::Var0));
	q.set_var(ecs::Var0, tgtA);

	auto& info = q.fetch();
	q.match_all(info);
	const auto revA = info.result_cache_rev();
	CHECK(info.cache_archetype_view().size() == 1);
	CHECK(q.count() == 1);

	q.match_all(info);
	CHECK(info.result_cache_rev() == revA);
	CHECK(q.count() == 1);

	q.set_var(ecs::Var0, tgtB);
	auto& infoB = q.fetch();
	CHECK(&infoB == &info);
	q.match_all(infoB);
	const auto revB = info.result_cache_rev();
	CHECK(revB != revA);
	CHECK(info.cache_archetype_view().size() == 1);
	CHECK(q.count() == 1);

	q.match_all(info);
	CHECK(info.result_cache_rev() == revB);
	CHECK(q.count() == 1);
}

TEST_CASE("Query - cached direct-source query after source changes") {
	TestWorld twld;

	auto source = wld.add();
	auto q = wld.query().all<Position>().all<Acceleration>(ecs::QueryTermOptions{}.src(source));
	auto& info = q.fetch();

	auto e = wld.add();
	wld.add<Position>(e, {1, 2, 3});

	q.match_all(info);
	const auto revEmpty = info.result_cache_rev();
	CHECK(q.count() == 0);

	q.match_all(info);
	CHECK(info.result_cache_rev() == revEmpty);
	CHECK(q.count() == 0);

	wld.add<Acceleration>(source, {1, 2, 3});
	q.match_all(info);
	const auto revFilled = info.result_cache_rev();
	CHECK(revFilled != revEmpty);
	CHECK(q.count() == 1);
	CHECK(info.result_cache_rev() == revFilled);

	wld.del<Acceleration>(source);
	q.match_all(info);
	CHECK(info.result_cache_rev() != revFilled);
	CHECK(q.count() == 0);
}

TEST_CASE("Query - cached direct-source query with unrelated archetype changes") {
	TestWorld twld;

	auto source = wld.add();
	wld.add<Acceleration>(source, {1, 2, 3});

	auto q = wld.query().all<Position>().all<Acceleration>(ecs::QueryTermOptions{}.src(source));
	auto& info = q.fetch();

	auto matched = wld.add();
	wld.add<Position>(matched, {1, 2, 3});

	q.match_all(info);
	const auto revA = info.result_cache_rev();
	CHECK(q.count() == 1);

	auto unrelated = wld.add();
	wld.add<Rotation>(unrelated, {4, 5, 6});
	wld.add<Scale>(unrelated, {7, 8, 9});

	q.match_all(info);
	CHECK(info.result_cache_rev() == revA);
	CHECK(q.count() == 1);
}

TEST_CASE("Query - cached direct-source query with deleted source entities") {
	TestWorld twld;

	auto source = wld.add();
	wld.add<Acceleration>(source, {1, 2, 3});

	auto q = wld.query().all<Position>().all<Acceleration>(ecs::QueryTermOptions{}.src(source));
	auto& info = q.fetch();

	auto matched = wld.add();
	wld.add<Position>(matched, {1, 2, 3});

	q.match_all(info);
	const auto revA = info.result_cache_rev();
	CHECK(q.count() == 1);

	wld.del(source);

	q.match_all(info);
	CHECK(info.result_cache_rev() != revA);
	CHECK(q.count() == 0);
}

TEST_CASE("Query - cached direct-source query ignores recycled source ids") {
	TestWorld twld;

	const auto source = wld.add();
	wld.add<Acceleration>(source, {1, 2, 3});

	auto q = wld.query().all<Position>().all<Acceleration>(ecs::QueryTermOptions{}.src(source));
	auto& info = q.fetch();

	const auto matched = wld.add();
	wld.add<Position>(matched, {1, 2, 3});

	q.match_all(info);
	const auto revA = info.result_cache_rev();
	CHECK(q.count() == 1);

	wld.del(source);
	wld.update();

	const auto recycled = wld.add();
	CHECK(recycled.id() == source.id());
	CHECK(recycled.gen() != source.gen());
	wld.add<Acceleration>(recycled, {4, 5, 6});

	q.match_all(info);
	CHECK(info.result_cache_rev() != revA);
	CHECK(q.count() == 0);

	auto freshQuery = wld.query().all<Position>().all<Acceleration>(ecs::QueryTermOptions{}.src(recycled));
	CHECK(freshQuery.count() == 1);
}

TEST_CASE("Query - direct-source query ignores recycled source ids") {
	TestWorld twld;

	const auto source = wld.add();
	wld.add<Acceleration>(source, {1, 2, 3});

	auto q = wld.query().all<Position>().all<Acceleration>(ecs::QueryTermOptions{}.src(source));

	const auto matched = wld.add();
	wld.add<Position>(matched, {1, 2, 3});

	CHECK(q.count() == 1);

	wld.del(source);
	wld.update();

	const auto recycled = wld.add();
	CHECK(recycled.id() == source.id());
	CHECK(recycled.gen() != source.gen());
	wld.add<Acceleration>(recycled, {4, 5, 6});

	CHECK(q.count() == 0);

	auto freshQuery = wld.query().all<Position>().all<Acceleration>(ecs::QueryTermOptions{}.src(recycled));
	CHECK(freshQuery.count() == 1);
}

TEST_CASE("Query - direct-source negative term tracks source changes") {
	TestWorld twld;

	struct Blocked {};

	const auto source = wld.add();
	const auto matched = wld.add();
	wld.add<Position>(matched, {1, 2, 3});

	auto q = wld.query().all<Position>().no<Blocked>(ecs::QueryTermOptions{}.src(source));
	CHECK(q.count() == 1);

	wld.add<Blocked>(source);
	CHECK(q.count() == 0);

	wld.del<Blocked>(source);
	CHECK(q.count() == 1);
}

TEST_CASE("Query - cached direct-source negative term tracks source changes") {
	TestWorld twld;

	struct Blocked {};

	const auto source = wld.add();
	const auto matched = wld.add();
	wld.add<Position>(matched, {1, 2, 3});

	auto q = wld.query().all<Position>().no<Blocked>(ecs::QueryTermOptions{}.src(source));
	auto& info = q.fetch();

	q.match_all(info);
	const auto revA = info.result_cache_rev();
	CHECK(q.count() == 1);

	wld.add<Blocked>(source);
	q.match_all(info);
	const auto revB = info.result_cache_rev();
	CHECK(revB != revA);
	CHECK(q.count() == 0);

	wld.del<Blocked>(source);
	q.match_all(info);
	CHECK(info.result_cache_rev() != revB);
	CHECK(q.count() == 1);
}

TEST_CASE("Query - direct-source or term tracks source changes") {
	TestWorld twld;

	const auto source = wld.add();
	const auto matched = wld.add();
	wld.add<Position>(matched, {1, 2, 3});

	auto q = wld.query().all<Position>().or_<Acceleration>(ecs::QueryTermOptions{}.src(source));
	CHECK(q.count() == 0);

	wld.add<Acceleration>(source, {4, 5, 6});
	CHECK(q.count() == 1);

	wld.del<Acceleration>(source);
	CHECK(q.count() == 0);
}

TEST_CASE("Query - cached direct-source or term tracks source changes") {
	TestWorld twld;

	const auto source = wld.add();
	const auto matched = wld.add();
	wld.add<Position>(matched, {1, 2, 3});

	auto q = wld.query().all<Position>().or_<Acceleration>(ecs::QueryTermOptions{}.src(source));
	auto& info = q.fetch();

	q.match_all(info);
	const auto revA = info.result_cache_rev();
	CHECK(q.count() == 0);

	wld.add<Acceleration>(source, {4, 5, 6});
	q.match_all(info);
	const auto revB = info.result_cache_rev();
	CHECK(revB != revA);
	CHECK(q.count() == 1);

	wld.del<Acceleration>(source);
	q.match_all(info);
	CHECK(info.result_cache_rev() != revB);
	CHECK(q.count() == 0);
}

TEST_CASE("Query - cached traversed-source query after traversal changes") {
	TestWorld twld;

	auto root = wld.add();
	auto parent = wld.add();
	auto scene = wld.add();
	wld.child(scene, parent);
	wld.child(parent, root);

	auto e = wld.add();
	wld.add<Position>(e, {1, 2, 3});

	auto q = wld.query()
							 .cache_src_trav(ecs::MaxCacheSrcTrav)
							 .all<Position>()
							 .all<Acceleration>(ecs::QueryTermOptions{}.src(scene).trav_parent());
	auto& info = q.fetch();

	q.match_all(info);
	const auto revEmpty = info.result_cache_rev();
	CHECK(q.count() == 0);

	q.match_all(info);
	CHECK(info.result_cache_rev() == revEmpty);
	CHECK(q.count() == 0);

	wld.add<Acceleration>(parent, {1, 2, 3});
	q.match_all(info);
	const auto revParent = info.result_cache_rev();
	CHECK(revParent != revEmpty);
	CHECK(q.count() == 1);

	q.match_all(info);
	CHECK(info.result_cache_rev() == revParent);
	CHECK(q.count() == 1);

	wld.del<Acceleration>(parent);
	wld.add<Acceleration>(root, {4, 5, 6});
	q.match_all(info);
	CHECK(info.result_cache_rev() != revParent);
	CHECK(q.count() == 0);
}

TEST_CASE("Query - cached traversed-source query with unrelated archetype changes") {
	TestWorld twld;

	auto root = wld.add();
	auto parent = wld.add();
	auto scene = wld.add();
	wld.child(scene, parent);
	wld.child(parent, root);
	wld.add<Acceleration>(parent, {1, 2, 3});

	auto e = wld.add();
	wld.add<Position>(e, {1, 2, 3});

	auto q = wld.query()
							 .cache_src_trav(ecs::MaxCacheSrcTrav)
							 .all<Position>()
							 .all<Acceleration>(ecs::QueryTermOptions{}.src(scene).trav_parent());
	auto& info = q.fetch();

	q.match_all(info);
	const auto revA = info.result_cache_rev();
	CHECK(q.count() == 1);

	auto unrelated = wld.add();
	wld.add<Rotation>(unrelated, {4, 5, 6});
	wld.add<Scale>(unrelated, {7, 8, 9});

	q.match_all(info);
	CHECK(info.result_cache_rev() == revA);
	CHECK(q.count() == 1);
}

TEST_CASE("Query - source traversal snapshot caching is opt-in for cached traversed-source queries") {
	TestWorld twld;

	auto source = wld.add();
	auto rel = wld.add();

	auto qDefault = wld.query().all<Position>().all<Acceleration>(ecs::QueryTermOptions{}.src(source).trav(rel));
	auto qOptIn = wld.query()
										.cache_src_trav(ecs::MaxCacheSrcTrav)
										.all<Position>()
										.all<Acceleration>(ecs::QueryTermOptions{}.src(source).trav(rel));

	CHECK(qDefault.cache_policy() == ecs::QueryCachePolicy::Dynamic);
	CHECK(qOptIn.cache_policy() == ecs::QueryCachePolicy::Dynamic);
	CHECK(qDefault.cache_src_trav() == 0);
	CHECK(qOptIn.cache_src_trav() > 0);
}

TEST_CASE("Query - capped traversed-source snapshots fall back to lazy rebuild") {
	TestWorld twld;

	auto root = wld.add();
	auto parent = wld.add();
	auto scene = wld.add();
	wld.child(scene, parent);
	wld.child(parent, root);

	auto e = wld.add();
	wld.add<Position>(e, {1, 2, 3});
	wld.add<Acceleration>(root, {4, 5, 6});

	auto q = wld.query().cache_src_trav(2).all<Position>().all<Acceleration>(ecs::QueryTermOptions{}.src(scene).trav());
	auto& info = q.fetch();

	q.match_all(info);
	CHECK(q.count() == 1);
	const auto revA = info.result_cache_rev();
	const auto passA = info.test_match_pass_count();

	q.match_all(info);
	const auto revB = info.result_cache_rev();
	CHECK(revB == revA);
	CHECK(info.test_match_pass_count() == passA + 1);
	CHECK(q.count() == 1);
}

TEST_CASE("Query - dependency metadata") {
	TestWorld twld;

	auto rel = wld.add();
	auto source = wld.add();

	auto qStructural = wld.query().all<Position>().or_<Scale>().no<Acceleration>().any<Rotation>();
	const auto& depsStructural = qStructural.fetch().ctx().data.deps;
	CHECK(depsStructural.has_dep_flag(ecs::QueryCtx::DependencyHasPositiveTerms));
	CHECK(depsStructural.has_dep_flag(ecs::QueryCtx::DependencyHasNegativeTerms));
	CHECK(depsStructural.has_dep_flag(ecs::QueryCtx::DependencyHasAnyTerms));
	CHECK(!depsStructural.has_dep_flag(ecs::QueryCtx::DependencyHasSourceTerms));
	CHECK(!depsStructural.has_dep_flag(ecs::QueryCtx::DependencyHasVariableTerms));
	CHECK(depsStructural.create_selectors_view().size() == 1);
	CHECK(depsStructural.exclusions_view().size() == 1);
	CHECK(core::has(depsStructural.create_selectors_view(), wld.get<Position>()));
	CHECK(core::has(depsStructural.exclusions_view(), wld.get<Acceleration>()));

	auto qOrOnly = wld.query().or_<Position>().or_<Scale>();
	const auto& depsOrOnly = qOrOnly.fetch().ctx().data.deps;
	CHECK(depsOrOnly.has_dep_flag(ecs::QueryCtx::DependencyHasPositiveTerms));
	CHECK(depsOrOnly.create_selectors_view().size() == 2);
	CHECK(core::has(depsOrOnly.create_selectors_view(), wld.get<Position>()));
	CHECK(core::has(depsOrOnly.create_selectors_view(), wld.get<Scale>()));

	auto qSource = wld.query().all<Position>(ecs::QueryTermOptions{}.src(source));
	const auto& depsSource = qSource.fetch().ctx().data.deps;
	CHECK(depsSource.has_dep_flag(ecs::QueryCtx::DependencyHasSourceTerms));
	CHECK(!depsSource.has_dep_flag(ecs::QueryCtx::DependencyHasPositiveTerms));
	CHECK(depsSource.create_selectors_view().empty());
	CHECK(depsSource.src_entities_view().size() == 1);
	CHECK(core::has(depsSource.src_entities_view(), source));

	auto qVar = wld.query().all(ecs::Pair(rel, ecs::Var0));
	const auto& depsVar = qVar.fetch().ctx().data.deps;
	CHECK(depsVar.has_dep_flag(ecs::QueryCtx::DependencyHasVariableTerms));
	CHECK(!depsVar.has_dep_flag(ecs::QueryCtx::DependencyHasPositiveTerms));
	CHECK(depsVar.create_selectors_view().empty());
	CHECK(depsVar.relations_view().size() == 1);
	CHECK(core::has(depsVar.relations_view(), rel));

	auto qTrav = wld.query().all<Position>(ecs::QueryTermOptions{}.src(source).trav(rel));
	const auto& depsTrav = qTrav.fetch().ctx().data.deps;
	CHECK(depsTrav.has_dep_flag(ecs::QueryCtx::DependencyHasSourceTerms));
	CHECK(depsTrav.has_dep_flag(ecs::QueryCtx::DependencyHasTraversalTerms));
	CHECK(depsTrav.relations_view().size() == 1);
	CHECK(core::has(depsTrav.relations_view(), rel));

	auto qSorted = wld.query().all<Position>().sort_by<Position>(
			[]([[maybe_unused]] const ecs::World& world, const void* pData0, const void* pData1) {
				const auto& p0 = *static_cast<const Position*>(pData0);
				const auto& p1 = *static_cast<const Position*>(pData1);
				if (p0.x < p1.x)
					return -1;
				if (p0.x > p1.x)
					return 1;
				return 0;
			});
	CHECK(qSorted.fetch().ctx().data.deps.has_dep_flag(ecs::QueryCtx::DependencyHasSort));

	auto eats = wld.add();
	auto qGrouped = wld.query().all<Position>().group_by(eats);
	const auto& depsGrouped = qGrouped.fetch().ctx().data.deps;
	CHECK(depsGrouped.has_dep_flag(ecs::QueryCtx::DependencyHasGroup));
	CHECK(depsGrouped.relations_view().size() == 1);
	CHECK(core::has(depsGrouped.relations_view(), eats));

	auto custom_group_by = []([[maybe_unused]] const ecs::World& world, const ecs::Archetype& archetype,
														ecs::Entity groupBy) {
		if (archetype.pairs() > 0) {
			auto ids = archetype.ids_view();
			for (auto id: ids) {
				if (!id.pair() || id.id() != groupBy.id())
					continue;

				return id.gen();
			}
		}

		return ecs::GroupId(0);
	};

	auto qGroupedCustom = wld.query().all<Position>().group_by(eats, custom_group_by);
	const auto& depsGroupedCustom = qGroupedCustom.fetch().ctx().data.deps;
	CHECK(depsGroupedCustom.has_dep_flag(ecs::QueryCtx::DependencyHasGroup));
	CHECK(depsGroupedCustom.relations_view().empty());

	auto qGroupedCustomDep = wld.query().all<Position>().group_by(eats, custom_group_by).group_dep(eats);
	const auto& depsGroupedCustomDep = qGroupedCustomDep.fetch().ctx().data.deps;
	CHECK(depsGroupedCustomDep.has_dep_flag(ecs::QueryCtx::DependencyHasGroup));
	CHECK(depsGroupedCustomDep.relations_view().size() == 1);
	CHECK(core::has(depsGroupedCustomDep.relations_view(), eats));

	auto parent = wld.add();
	auto qGroupedCustomDeps =
			wld.query().all<Position>().group_by(eats, custom_group_by).group_dep(eats).group_dep(parent);
	const auto& depsGroupedCustomDeps = qGroupedCustomDeps.fetch().ctx().data.deps;
	CHECK(depsGroupedCustomDeps.has_dep_flag(ecs::QueryCtx::DependencyHasGroup));
	CHECK(depsGroupedCustomDeps.relations_view().size() == 2);
	CHECK(core::has(depsGroupedCustomDeps.relations_view(), eats));
	CHECK(core::has(depsGroupedCustomDeps.relations_view(), parent));
}

TEST_CASE("Query - create selectors with narrowest ALL term") {
	TestWorld twld;

	GAIA_FOR(8) {
		auto e = wld.add();
		wld.add(e, wld.add());
		wld.add<Position>(e, {(float)i, 0, 0});
	}

	auto selective = wld.add();
	wld.add(selective, wld.add());
	wld.add<Position>(selective, {1, 2, 3});
	wld.add<Acceleration>(selective, {1, 2, 3});

	auto q = wld.query().all<Position>().all<Acceleration>();
	const auto& deps = q.fetch().ctx().data.deps;
	CHECK(deps.create_selectors_view().size() == 1);
	CHECK(deps.create_selectors_view()[0] == wld.get<Acceleration>());
}

TEST_CASE("Query - create selectors with wildcard pair over broad exact term on empty world") {
	TestWorld twld;

	auto rel = wld.add();
	auto q = wld.query().all<Position>().all(ecs::Pair(rel, ecs::All));
	const auto& deps = q.fetch().ctx().data.deps;
	CHECK(deps.create_selectors_view().size() == 1);
	CHECK(deps.create_selectors_view()[0] == ecs::Pair(rel, ecs::All));
}

TEST_CASE("Query - kind and policy") {
	TestWorld twld;

	auto source = wld.add();
	auto rel = wld.add();

	auto qCachedImmediate = wld.query().all<Position>();
	auto qCachedLazy = wld.query().no<Position>();
	auto qCachedDynamic = wld.query().all<Position>(ecs::QueryTermOptions{}.src(source));
	auto qCachedDynamicOptIn =
			wld.query().cache_src_trav(ecs::MaxCacheSrcTrav).all<Position>(ecs::QueryTermOptions{}.src(source).trav(rel));
	auto qCachedVar = wld.query().all(ecs::Pair(rel, ecs::Var0));
	auto qSharedImmediate = wld.query().scope(ecs::QueryCacheScope::Shared).all<Position>();
	auto qNoneImmediate = wld.query().kind(ecs::QueryCacheKind::None).all<Position>();
	auto qUncachedImmediate = wld.uquery().all<Position>();

	CHECK(qCachedImmediate.kind() == ecs::QueryCacheKind::Default);
	CHECK(qCachedImmediate.scope() == ecs::QueryCacheScope::Local);
	CHECK(qCachedImmediate.cache_policy() == ecs::QueryCachePolicy::Immediate);

	CHECK(qCachedLazy.kind() == ecs::QueryCacheKind::Default);
	CHECK(qCachedLazy.scope() == ecs::QueryCacheScope::Local);
	CHECK(qCachedLazy.cache_policy() == ecs::QueryCachePolicy::Lazy);

	CHECK(qCachedDynamic.kind() == ecs::QueryCacheKind::Default);
	CHECK(qCachedDynamic.scope() == ecs::QueryCacheScope::Local);
	CHECK(qCachedDynamic.cache_policy() == ecs::QueryCachePolicy::Dynamic);
	CHECK(qCachedDynamic.cache_src_trav() == 0);

	CHECK(qCachedDynamicOptIn.kind() == ecs::QueryCacheKind::Default);
	CHECK(qCachedDynamicOptIn.scope() == ecs::QueryCacheScope::Local);
	CHECK(qCachedDynamicOptIn.cache_policy() == ecs::QueryCachePolicy::Dynamic);
	CHECK(qCachedDynamicOptIn.cache_src_trav() == 32);

	CHECK(qCachedVar.kind() == ecs::QueryCacheKind::Default);
	CHECK(qCachedVar.scope() == ecs::QueryCacheScope::Local);
	CHECK(qCachedVar.cache_policy() == ecs::QueryCachePolicy::Dynamic);

	CHECK(qSharedImmediate.kind() == ecs::QueryCacheKind::Default);
	CHECK(qSharedImmediate.scope() == ecs::QueryCacheScope::Shared);
	CHECK(qSharedImmediate.cache_policy() == ecs::QueryCachePolicy::Immediate);

	CHECK(qNoneImmediate.kind() == ecs::QueryCacheKind::None);
	CHECK(qNoneImmediate.scope() == ecs::QueryCacheScope::Local);
	CHECK(qNoneImmediate.cache_policy() == ecs::QueryCachePolicy::Immediate);

	CHECK(qUncachedImmediate.kind() == ecs::QueryCacheKind::None);
	CHECK(qUncachedImmediate.scope() == ecs::QueryCacheScope::Local);
	CHECK(qUncachedImmediate.cache_policy() == ecs::QueryCachePolicy::Immediate);
}

TEST_CASE("Query - kind") {
	TestWorld twld;

	ecs::Query::SilenceInvalidCacheKindAssertions = true;

	auto source = wld.add();
	auto rel = wld.add();

	auto qDefault = wld.query().kind(ecs::QueryCacheKind::Default).all<Position>();
	auto qDefaultSrcTrav = wld.query()
														 .kind(ecs::QueryCacheKind::Default)
														 .cache_src_trav(ecs::MaxCacheSrcTrav)
														 .all<Position>(ecs::QueryTermOptions{}.src(source).trav(rel));
	auto qAuto = wld.query().kind(ecs::QueryCacheKind::Auto).no<Position>();
	auto qAutoDirectSrc = wld.query().kind(ecs::QueryCacheKind::Auto).all<Position>(ecs::QueryTermOptions{}.src(source));
	auto qAutoTraversedSrc =
			wld.query().kind(ecs::QueryCacheKind::Auto).all<Position>(ecs::QueryTermOptions{}.src(source).trav(rel));
	auto qAutoSrcTrav = wld.query()
													.kind(ecs::QueryCacheKind::Auto)
													.cache_src_trav(ecs::MaxCacheSrcTrav)
													.all<Position>(ecs::QueryTermOptions{}.src(source).trav(rel));
	auto qNone = wld.query().kind(ecs::QueryCacheKind::None).all<Position>();
	auto qAll = wld.query().kind(ecs::QueryCacheKind::All).all<Position>();
	auto qAllFail = wld.query().kind(ecs::QueryCacheKind::All).no<Position>();
	auto qAllDynamic = wld.query().kind(ecs::QueryCacheKind::All).all<Position>(ecs::QueryTermOptions{}.src(source));
	auto qAllSrcTrav = wld.query()
												 .kind(ecs::QueryCacheKind::All)
												 .cache_src_trav(ecs::MaxCacheSrcTrav)
												 .all<Position>(ecs::QueryTermOptions{}.src(source).trav(rel));
	auto qDynamic = wld.query().kind(ecs::QueryCacheKind::Default).all<Position>(ecs::QueryTermOptions{}.src(source));

	CHECK(qDefault.kind() == ecs::QueryCacheKind::Default);
	CHECK(qDefault.valid());
	CHECK(qDefault.kind_error() == ecs::QueryKindRes::OK);
	CHECK(qDefault.cache_policy() == ecs::QueryCachePolicy::Immediate);

	CHECK(qDefaultSrcTrav.kind() == ecs::QueryCacheKind::Default);
	CHECK(qDefaultSrcTrav.valid());
	CHECK(qDefaultSrcTrav.kind_error() == ecs::QueryKindRes::OK);
	CHECK(qDefaultSrcTrav.cache_src_trav() > 0);

	CHECK(qAuto.kind() == ecs::QueryCacheKind::Auto);
	CHECK(qAuto.valid());
	CHECK(qAuto.kind_error() == ecs::QueryKindRes::OK);
	CHECK(qAuto.cache_policy() == ecs::QueryCachePolicy::Lazy);

	CHECK(qAutoDirectSrc.kind() == ecs::QueryCacheKind::Auto);
	CHECK(qAutoDirectSrc.valid());
	CHECK(qAutoDirectSrc.kind_error() == ecs::QueryKindRes::OK);
	CHECK(qAutoDirectSrc.cache_policy() == ecs::QueryCachePolicy::Dynamic);
	CHECK(qAutoDirectSrc.cache_src_trav() == 0);

	CHECK(qAutoTraversedSrc.kind() == ecs::QueryCacheKind::Auto);
	CHECK(qAutoTraversedSrc.valid());
	CHECK(qAutoTraversedSrc.kind_error() == ecs::QueryKindRes::OK);
	CHECK(qAutoTraversedSrc.cache_policy() == ecs::QueryCachePolicy::Dynamic);
	CHECK(qAutoTraversedSrc.cache_src_trav() == 0);

	CHECK(qAutoSrcTrav.kind() == ecs::QueryCacheKind::Auto);
	CHECK(!qAutoSrcTrav.valid());
	CHECK(qAutoSrcTrav.kind_error() == ecs::QueryKindRes::AutoSrcTrav);
	CHECK(std::string(qAutoSrcTrav.kind_error_str()).find("Auto") != std::string::npos);
	CHECK(qAutoSrcTrav.count() == 0);

	CHECK(qNone.kind() == ecs::QueryCacheKind::None);
	CHECK(qNone.valid());
	CHECK(qNone.kind_error() == ecs::QueryKindRes::OK);
	CHECK(qNone.cache_policy() == ecs::QueryCachePolicy::Immediate);

	CHECK(qAll.kind() == ecs::QueryCacheKind::All);
	CHECK(qAll.valid());
	CHECK(qAll.kind_error() == ecs::QueryKindRes::OK);
	CHECK(qAll.cache_policy() == ecs::QueryCachePolicy::Immediate);

	CHECK(qAllFail.kind() == ecs::QueryCacheKind::All);
	CHECK(!qAllFail.valid());
	CHECK(qAllFail.kind_error() == ecs::QueryKindRes::AllNotIm);
	CHECK(std::string(qAllFail.kind_error_str()).find("immediate") != std::string::npos);
	CHECK(qAllFail.count() == 0);

	CHECK(qAllDynamic.kind() == ecs::QueryCacheKind::All);
	CHECK(!qAllDynamic.valid());
	CHECK(qAllDynamic.kind_error() == ecs::QueryKindRes::AllNotIm);
	CHECK(qAllDynamic.count() == 0);

	CHECK(qAllSrcTrav.kind() == ecs::QueryCacheKind::All);
	CHECK(!qAllSrcTrav.valid());
	CHECK(qAllSrcTrav.kind_error() == ecs::QueryKindRes::AllSrcTrav);
	CHECK(std::string(qAllSrcTrav.kind_error_str()).find("snapshot") != std::string::npos);
	CHECK(qAllSrcTrav.count() == 0);

	CHECK(qDynamic.valid());
	CHECK(qDynamic.kind_error() == ecs::QueryKindRes::OK);
	CHECK(qDynamic.cache_policy() == ecs::QueryCachePolicy::Dynamic);

	ecs::Query::SilenceInvalidCacheKindAssertions = false;
}

TEST_CASE("Query - cache_src_trav and traversed source cache keys") {
	TestWorld twld;

	auto root = wld.add();
	auto leaf = wld.add();
	wld.child(leaf, root);
	auto qNoSource = wld.query().scope(ecs::QueryCacheScope::Shared).all<Position>();
	auto qNoSourceSrcTrav =
			wld.query().scope(ecs::QueryCacheScope::Shared).cache_src_trav(ecs::MaxCacheSrcTrav).all<Position>();
	auto qDirectSource = wld.query().scope(ecs::QueryCacheScope::Shared).all<Position>(ecs::QueryTermOptions{}.src(root));
	auto qDirectSourceSrcTrav = wld.query()
																	.scope(ecs::QueryCacheScope::Shared)
																	.cache_src_trav(ecs::MaxCacheSrcTrav)
																	.all<Position>(ecs::QueryTermOptions{}.src(root));
	auto qTraversedSource =
			wld.query().scope(ecs::QueryCacheScope::Shared).all<Position>(ecs::QueryTermOptions{}.src(leaf).trav());
	auto qTraversedSourceSrcTrav = wld.query()
																		 .scope(ecs::QueryCacheScope::Shared)
																		 .cache_src_trav(ecs::MaxCacheSrcTrav)
																		 .all<Position>(ecs::QueryTermOptions{}.src(leaf).trav());

	const auto& ctxNoSource = qNoSource.fetch().ctx();
	const auto& ctxNoSourceSrcTrav = qNoSourceSrcTrav.fetch().ctx();
	const auto& ctxDirectSource = qDirectSource.fetch().ctx();
	const auto& ctxDirectSourceSrcTrav = qDirectSourceSrcTrav.fetch().ctx();
	const auto& ctxTraversedSource = qTraversedSource.fetch().ctx();
	const auto& ctxTraversedSourceSrcTrav = qTraversedSourceSrcTrav.fetch().ctx();

	CHECK(ctxNoSource.hashLookup.hash == ctxNoSourceSrcTrav.hashLookup.hash);
	CHECK(ctxDirectSource.hashLookup.hash == ctxDirectSourceSrcTrav.hashLookup.hash);
	CHECK(ctxTraversedSource.hashLookup.hash != ctxTraversedSourceSrcTrav.hashLookup.hash);
	CHECK(ctxNoSourceSrcTrav.data.cacheSrcTrav == 0);
	CHECK(ctxDirectSourceSrcTrav.data.cacheSrcTrav == 0);
	CHECK(ctxTraversedSourceSrcTrav.data.cacheSrcTrav == ecs::MaxCacheSrcTrav);
}

TEST_CASE("Query - shared cache for identical traversed queries") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.child(child, root);
	wld.add<Acceleration>(root);

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, child));

	const auto buildQuery = [&twld, connectedTo] {
		return wld.query()
				.scope(ecs::QueryCacheScope::Shared)
				.template all<Position>()
				.all(ecs::Pair(connectedTo, ecs::Var0))
				.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav());
	};

	cnt::darray<ecs::Query> queries;
	queries.reserve(200);

	auto q0 = buildQuery();
	const auto expectedHandle = ecs::QueryInfo::handle(q0.fetch());
	CHECK(q0.count() == 1);
	queries.push_back(GAIA_MOV(q0));

	for (uint32_t i = 1; i < 200; ++i) {
		auto q = buildQuery();
		CHECK(ecs::QueryInfo::handle(q.fetch()) == expectedHandle);
		queries.push_back(GAIA_MOV(q));
	}

	CHECK(queries.size() == 200);
}

TEST_CASE("Query - cached broad NOT query after archetype creation") {
	TestWorld twld;

	auto excluded = wld.add();
	auto q = wld.query().no(excluded);
	auto qUncached = wld.uquery().no(excluded);
	auto& info = q.fetch();
	q.match_all(info);
	const auto archetypeCntBefore = info.cache_archetype_view().size();

	auto e = wld.add();
	wld.add<Position>(e, {1, 2, 3});

	// Selector-less structural queries use the lazy cached path.
	CHECK(info.cache_archetype_view().size() == archetypeCntBefore);
	CHECK(q.count() == qUncached.count());
	CHECK(info.cache_archetype_view().size() == archetypeCntBefore + 1);
}

TEST_CASE("Query - cached broad NOT query with non-matching archetypes") {
	TestWorld twld;

	auto excluded = wld.add();
	auto q = wld.query().no(excluded);
	auto qUncached = wld.uquery().no(excluded);
	auto& info = q.fetch();
	q.match_all(info);
	const auto archetypeCntBefore = info.cache_archetype_view().size();
	const auto entityCntBefore = qUncached.count();

	auto e = wld.add();
	wld.add(e, excluded);

	CHECK(info.cache_archetype_view().size() == archetypeCntBefore);
	CHECK(q.count() == entityCntBefore);
	CHECK(q.count() == qUncached.count());
}

TEST_CASE("Query - cached broad NOT wildcard pair query after archetype creation") {
	TestWorld twld;

	auto relExcluded = wld.add();
	auto relOther = wld.add();
	auto tgt = wld.add();
	auto q = wld.query().no(ecs::Pair(relExcluded, ecs::All));
	auto qUncached = wld.uquery().no(ecs::Pair(relExcluded, ecs::All));
	auto& info = q.fetch();
	q.match_all(info);
	const auto archetypeCntBefore = info.cache_archetype_view().size();

	auto e = wld.add();
	wld.add(e, ecs::Pair(relOther, tgt));

	CHECK(info.cache_archetype_view().size() == archetypeCntBefore);
	CHECK(q.count() == qUncached.count());
	CHECK(info.cache_archetype_view().size() == archetypeCntBefore + 1);
}

TEST_CASE("Query - cached broad NOT wildcard pair query with excluded archetypes") {
	TestWorld twld;

	auto relExcluded = wld.add();
	auto tgt = wld.add();
	auto q = wld.query().no(ecs::Pair(relExcluded, ecs::All));
	auto qUncached = wld.uquery().no(ecs::Pair(relExcluded, ecs::All));
	auto& info = q.fetch();
	q.match_all(info);
	const auto archetypeCntBefore = info.cache_archetype_view().size();

	auto e = wld.add();
	wld.add(e, ecs::Pair(relExcluded, tgt));

	CHECK(info.cache_archetype_view().size() == archetypeCntBefore);
	CHECK(q.count() == qUncached.count());
}

TEST_CASE("Query - cached multi-NOT query after archetype creation") {
	TestWorld twld;

	auto excludedA = wld.add();
	auto excludedB = wld.add();
	auto included = wld.add();
	auto q = wld.query().no(excludedA).no(excludedB);
	auto qUncached = wld.uquery().no(excludedA).no(excludedB);
	auto& info = q.fetch();
	q.match_all(info);
	const auto archetypeCntBefore = info.cache_archetype_view().size();

	auto e = wld.add();
	wld.add(e, included);

	CHECK(info.cache_archetype_view().size() == archetypeCntBefore);
	CHECK(q.count() == qUncached.count());
	CHECK(info.cache_archetype_view().size() == archetypeCntBefore + 1);
}

TEST_CASE("Query - cached multi-NOT query with excluded ids") {
	TestWorld twld;

	auto excludedA = wld.add();
	auto excludedB = wld.add();
	auto q = wld.query().no(excludedA).no(excludedB);
	auto qUncached = wld.uquery().no(excludedA).no(excludedB);
	auto& info = q.fetch();
	q.match_all(info);
	const auto archetypeCntBefore = info.cache_archetype_view().size();

	auto e = wld.add();
	wld.add(e, excludedB);

	CHECK(info.cache_archetype_view().size() == archetypeCntBefore);
	CHECK(q.count() == qUncached.count());
}

TEST_CASE("Query - cached structural query after warm match and new archetype") {
	TestWorld twld;

	auto e0 = wld.add();
	wld.add<Position>(e0, {1, 0, 0});

	auto q = wld.query().all<Position>();
	CHECK(q.count() == 1);
	CHECK(q.count() == 1);

	auto& info = q.fetch();
	CHECK(info.cache_archetype_view().size() == 1);

	auto e1 = wld.add();
	wld.add<Position>(e1, {2, 0, 0});
	wld.add<Something>(e1, {false});

	CHECK(info.cache_archetype_view().size() == 2);
	CHECK(q.count() == 2);
}

TEST_CASE("Query - cached wildcard pair query after matching archetype creation") {
	TestWorld twld;

	auto rel = wld.add();
	auto tgt = wld.add();

	auto qRel = wld.query().all(ecs::Pair{rel, ecs::All});
	auto qTgt = wld.query().all(ecs::Pair{ecs::All, tgt});

	auto& infoRel = qRel.fetch();
	auto& infoTgt = qTgt.fetch();
	qRel.match_all(infoRel);
	qTgt.match_all(infoTgt);

	CHECK(infoRel.cache_archetype_view().empty());
	CHECK(infoTgt.cache_archetype_view().empty());

	auto e = wld.add();
	wld.add(e, ecs::Pair(rel, tgt));

	CHECK(infoRel.cache_archetype_view().size() == 1);
	CHECK(infoTgt.cache_archetype_view().size() == 1);
	CHECK(qRel.count() == 1);
	CHECK(qTgt.count() == 1);
}

TEST_CASE("Query - cached exact and wildcard pair query after matching archetype creation") {
	TestWorld twld;

	auto rel = wld.add();
	auto tgt = wld.add();

	auto q = wld.query().all<Position>().all(ecs::Pair{rel, ecs::All});
	auto& info = q.fetch();
	q.match_all(info);

	CHECK(info.cache_archetype_view().empty());
	CHECK(q.count() == 0);

	auto e = wld.add();
	wld.add(e, ecs::Pair(rel, tgt));
	wld.add<Position>(e, {1, 0, 0});

	CHECK(info.cache_archetype_view().size() == 1);
	CHECK(q.count() == 1);
}

TEST_CASE("Query - cached exact and NOT query after matching archetype creation") {
	TestWorld twld;

	auto q = wld.query().all<Position>().no<Scale>();
	auto& info = q.fetch();
	q.match_all(info);

	CHECK(info.cache_archetype_view().empty());
	CHECK(q.count() == 0);

	auto e = wld.add();
	wld.add<Position>(e, {1, 0, 0});

	CHECK(info.cache_archetype_view().size() == 1);
	CHECK(q.count() == 1);
}

TEST_CASE("Query - cached exact and NOT query with excluded archetype creation") {
	TestWorld twld;

	auto q = wld.query().all<Position>().no<Scale>();
	auto& info = q.fetch();
	q.match_all(info);

	CHECK(info.cache_archetype_view().empty());
	CHECK(q.count() == 0);

	auto e = wld.add();
	wld.build(e).add<Scale>().add<Position>();

	CHECK(info.cache_archetype_view().empty());
	CHECK(q.count() == 0);
}

TEST_CASE("Query - cached any-pair query after matching archetype creation") {
	TestWorld twld;

	auto rel = wld.add();
	auto tgt = wld.add();

	auto q = wld.query().all(ecs::Pair{ecs::All, ecs::All});
	auto& info = q.fetch();
	q.match_all(info);
	const auto archetypeCntBefore = info.cache_archetype_view().size();
	const auto entityCntBefore = q.count();

	auto e = wld.add();
	wld.build(e).add({rel, tgt});

	CHECK(info.cache_archetype_view().size() == archetypeCntBefore + 1);
	CHECK(q.count() == entityCntBefore + 1);
}

TEST_CASE("Query - cached any-pair query after pair removal") {
	TestWorld twld;

	auto rel = wld.add();
	auto tgt = wld.add();
	auto e = wld.add();
	wld.add(e, ecs::Pair(rel, tgt));

	auto q = wld.query().all(ecs::Pair{ecs::All, ecs::All}).no<ecs::Core_>().no<ecs::System_>();
	auto& info = q.fetch();
	q.match_all(info);
	CHECK(q.count() == 1);
	CHECK(info.cache_archetype_view().size() == 1);

	wld.del(e, ecs::Pair(rel, tgt));

	CHECK(q.count() == 0);
}

TEST_CASE("Query - cached wildcard pair query on pair-heavy archetypes") {
	TestWorld twld;

	auto rel = wld.add();
	auto tgt0 = wld.add();
	auto tgt1 = wld.add();

	auto q = wld.query().all(ecs::Pair{rel, ecs::All});
	auto& info = q.fetch();
	q.match_all(info);
	CHECK(info.cache_archetype_view().empty());

	auto e = wld.add();
	wld.build(e).add({rel, tgt0}).add({rel, tgt1});

	const auto archetypeCntAfterBuild = info.cache_archetype_view().size();
	CHECK(archetypeCntAfterBuild >= 1);
	CHECK(q.count() == 1);
	CHECK(info.cache_archetype_view().size() == archetypeCntAfterBuild);
}

TEST_CASE("Query - cached sorted query after archetype creation") {
	TestWorld twld;

	auto q = wld.query().all<Position>().sort_by(
			wld.get<Position>(), []([[maybe_unused]] const ecs::World& world, const void* pData0, const void* pData1) {
				const auto& p0 = *static_cast<const Position*>(pData0);
				const auto& p1 = *static_cast<const Position*>(pData1);
				if (p0.x < p1.x)
					return -1;
				if (p0.x > p1.x)
					return 1;
				return 0;
			});

	auto& info = q.fetch();
	q.match_all(info);
	CHECK(info.cache_archetype_view().empty());

	auto e = wld.add();
	wld.add<Position>(e, {1, 0, 0});

	// Sorted queries keep using the normal read-time refresh path.
	CHECK(info.cache_archetype_view().empty());
	CHECK(q.count() == 1);
	CHECK(info.cache_archetype_view().size() == 1);
}

TEST_CASE("Query - sorted query identity hash includes sort payload") {
	TestWorld twld;

	auto pos = wld.add<Position>().entity;
	auto e = wld.add();
	wld.add<Position>(e, {1, 0, 0});

	auto sort_ascending = []([[maybe_unused]] const ecs::World& world, const void* pData0, const void* pData1) {
		const auto& p0 = *static_cast<const Position*>(pData0);
		const auto& p1 = *static_cast<const Position*>(pData1);
		if (p0.x < p1.x)
			return -1;
		if (p0.x > p1.x)
			return 1;
		return 0;
	};
	auto sort_descending = []([[maybe_unused]] const ecs::World& world, const void* pData0, const void* pData1) {
		const auto& p0 = *static_cast<const Position*>(pData0);
		const auto& p1 = *static_cast<const Position*>(pData1);
		if (p0.x > p1.x)
			return -1;
		if (p0.x < p1.x)
			return 1;
		return 0;
	};

	auto qAscending = wld.query().scope(ecs::QueryCacheScope::Shared).all<Position>().sort_by(pos, sort_ascending);
	auto qDescending = wld.query().scope(ecs::QueryCacheScope::Shared).all<Position>().sort_by(pos, sort_descending);
	auto qUnsorted = wld.query().scope(ecs::QueryCacheScope::Shared).all<Position>();

	CHECK(qAscending.count() == 1);
	CHECK(qDescending.count() == 1);
	CHECK(qUnsorted.count() == 1);

	const auto hashAscending = qAscending.fetch().ctx().hashLookup.hash;
	const auto hashDescending = qDescending.fetch().ctx().hashLookup.hash;
	const auto hashUnsorted = qUnsorted.fetch().ctx().hashLookup.hash;

	CHECK(hashAscending != 0);
	CHECK(hashDescending != 0);
	CHECK(hashUnsorted != 0);
	CHECK(hashAscending != hashDescending);
	CHECK(hashAscending != hashUnsorted);
	CHECK(hashDescending != hashUnsorted);
}

TEST_CASE("Query - cached sorted query exact sort term lookup across archetypes") {
	TestWorld twld;

	auto e0 = wld.add();
	auto e1 = wld.add();
	auto e2 = wld.add();
	auto e3 = wld.add();

	wld.add<Position>(e0, {4, 0, 0});
	wld.add<Position>(e1, {2, 0, 0});
	wld.add<Position>(e2, {3, 0, 0});
	wld.add<Position>(e3, {1, 0, 0});

	wld.add<Something>(e1, {true});
	wld.add<Else>(e2, {true});
	wld.add<Something>(e3, {true});
	wld.add<Else>(e3, {true});

	for (auto entity: {e0, e1, e2, e3}) {
		const auto& ec = wld.fetch(entity);
		const auto compIdxIndex = ecs::world_component_index_comp_idx(wld, *ec.pArchetype, wld.get<Position>());
		const auto compIdxChunk = ec.pChunk->comp_idx(wld.get<Position>());
		CHECK(compIdxIndex == compIdxChunk);
	}

	auto q = wld.query().all<Position>().sort_by(
			wld.get<Position>(), []([[maybe_unused]] const ecs::World& world, const void* pData0, const void* pData1) {
				const auto& p0 = *static_cast<const Position*>(pData0);
				const auto& p1 = *static_cast<const Position*>(pData1);
				if (p0.x < p1.x)
					return -1;
				if (p0.x > p1.x)
					return 1;
				return 0;
			});

	auto& info = q.fetch();
	q.match_all(info);
	CHECK(!info.cache_archetype_view().empty());

	cnt::darray<ecs::Entity> ordered;
	q.each([&](ecs::Iter& it) {
		auto ents = it.view<ecs::Entity>();
		GAIA_EACH(ents) ordered.push_back(ents[i]);
	});

	CHECK(ordered.size() == 4);
	CHECK(wld.get<Position>(ordered[0]).x == doctest::Approx(1.0f));
	CHECK(wld.get<Position>(ordered[3]).x == doctest::Approx(4.0f));
	CHECK(info.cache_archetype_view().size() >= 4);
	{
		cnt::darray<ecs::Entity> orderedArr;
		q.arr(orderedArr);
		CHECK(orderedArr.size() == 4);
		CHECK(orderedArr[0] == ordered[0]);
		CHECK(orderedArr[1] == ordered[1]);
		CHECK(orderedArr[2] == ordered[2]);
		CHECK(orderedArr[3] == ordered[3]);
	}

	auto e4 = wld.add();
	wld.add<Position>(e4, {0, 0, 0});
	wld.add<Empty>(e4);

	ordered.clear();
	q.each([&](ecs::Iter& it) {
		auto ents = it.view<ecs::Entity>();
		GAIA_EACH(ents) ordered.push_back(ents[i]);
	});

	CHECK(ordered.size() == 5);
	CHECK(wld.get<Position>(ordered[0]).x == doctest::Approx(0.0f));
	CHECK(wld.get<Position>(ordered[1]).x == doctest::Approx(1.0f));
	CHECK(wld.get<Position>(ordered[4]).x == doctest::Approx(4.0f));
	CHECK(info.cache_archetype_view().size() >= 5);

	wld.del(e2);
	for (auto entity: {e0, e1, e3, e4}) {
		const auto& ec = wld.fetch(entity);
		const auto compIdxIndex = ecs::world_component_index_comp_idx(wld, *ec.pArchetype, wld.get<Position>());
		const auto compIdxChunk = ec.pChunk->comp_idx(wld.get<Position>());
		CHECK(compIdxIndex == compIdxChunk);
	}
}

TEST_CASE("Query - cached sorted query external exact sort term lookup across archetypes") {
	TestWorld twld;

	auto e0 = wld.add();
	auto e1 = wld.add();
	auto e2 = wld.add();
	auto e3 = wld.add();

	wld.add<Position>(e0, {4, 0, 0});
	wld.add<Position>(e1, {2, 0, 0});
	wld.add<Position>(e2, {3, 0, 0});
	wld.add<Position>(e3, {1, 0, 0});

	wld.add<Scale>(e0, {1, 1, 1});
	wld.add<Scale>(e1, {1, 1, 1});
	wld.add<Scale>(e2, {1, 1, 1});
	wld.add<Scale>(e3, {1, 1, 1});

	wld.add<Something>(e1, {true});
	wld.add<Else>(e2, {true});
	wld.add<Something>(e3, {true});
	wld.add<Else>(e3, {true});

	for (auto entity: {e0, e1, e2, e3}) {
		const auto& ec = wld.fetch(entity);
		const auto compIdxIndex = ecs::world_component_index_comp_idx(wld, *ec.pArchetype, wld.get<Position>());
		const auto compIdxChunk = ec.pChunk->comp_idx(wld.get<Position>());
		CHECK(compIdxIndex == compIdxChunk);
	}

	auto q = wld.query().all<Scale>().sort_by(
			wld.get<Position>(), []([[maybe_unused]] const ecs::World& world, const void* pData0, const void* pData1) {
				const auto& p0 = *static_cast<const Position*>(pData0);
				const auto& p1 = *static_cast<const Position*>(pData1);
				if (p0.x < p1.x)
					return -1;
				if (p0.x > p1.x)
					return 1;
				return 0;
			});

	auto& info = q.fetch();
	q.match_all(info);
	CHECK(!info.cache_archetype_view().empty());

	cnt::darray<ecs::Entity> ordered;
	q.each([&](ecs::Iter& it) {
		auto ents = it.view<ecs::Entity>();
		GAIA_EACH(ents) ordered.push_back(ents[i]);
	});

	CHECK(ordered.size() == 4);
	CHECK(wld.get<Position>(ordered[0]).x == doctest::Approx(1.0f));
	CHECK(wld.get<Position>(ordered[3]).x == doctest::Approx(4.0f));
	CHECK(info.cache_archetype_view().size() >= 4);

	auto e4 = wld.add();
	wld.add<Position>(e4, {0, 0, 0});
	wld.add<Scale>(e4, {1, 1, 1});
	wld.add<Empty>(e4);

	ordered.clear();
	q.each([&](ecs::Iter& it) {
		auto ents = it.view<ecs::Entity>();
		GAIA_EACH(ents) ordered.push_back(ents[i]);
	});

	CHECK(ordered.size() == 5);
	CHECK(wld.get<Position>(ordered[0]).x == doctest::Approx(0.0f));
	CHECK(wld.get<Position>(ordered[1]).x == doctest::Approx(1.0f));
	CHECK(wld.get<Position>(ordered[4]).x == doctest::Approx(4.0f));
	CHECK(info.cache_archetype_view().size() >= 5);
}

TEST_CASE("Query - cached grouped query after archetype creation") {
	TestWorld twld;

	auto eats = wld.add();
	auto carrot = wld.add();

	auto q = wld.query().all<Position>().group_by(eats);

	auto& info = q.fetch();
	q.match_all(info);
	CHECK(info.cache_archetype_view().empty());

	auto e = wld.add();
	wld.add<Position>(e, {1, 0, 0});
	wld.add(e, ecs::Pair(eats, carrot));

	// Grouped queries keep using the normal read-time refresh path.
	CHECK(info.cache_archetype_view().empty());
	CHECK(q.count() == 1);
	CHECK(!info.cache_archetype_view().empty());
	q.group_id(carrot);
	CHECK(q.count() == 1);
}

TEST_CASE("Query - grouped query identity hash includes group payload") {
	TestWorld twld;

	auto eats = wld.add();
	auto drinks = wld.add();
	auto carrot = wld.add();
	auto water = wld.add();

	auto e = wld.add();
	wld.add<Position>(e, {1, 0, 0});
	wld.add(e, ecs::Pair(eats, carrot));
	wld.add(e, ecs::Pair(drinks, water));

	auto qUnGrouped = wld.query().scope(ecs::QueryCacheScope::Shared).all<Position>();
	auto qGroupedEats = wld.query().scope(ecs::QueryCacheScope::Shared).all<Position>().group_by(eats);
	auto qGroupedDrinks = wld.query().scope(ecs::QueryCacheScope::Shared).all<Position>().group_by(drinks);

	CHECK(qUnGrouped.count() == 1);
	CHECK(qGroupedEats.count() == 1);
	CHECK(qGroupedDrinks.count() == 1);

	const auto hashUnGrouped = qUnGrouped.fetch().ctx().hashLookup.hash;
	const auto hashGroupedEats = qGroupedEats.fetch().ctx().hashLookup.hash;
	const auto hashGroupedDrinks = qGroupedDrinks.fetch().ctx().hashLookup.hash;

	CHECK(hashUnGrouped != 0);
	CHECK(hashGroupedEats != 0);
	CHECK(hashGroupedDrinks != 0);
	CHECK(hashUnGrouped != hashGroupedEats);
	CHECK(hashUnGrouped != hashGroupedDrinks);
	CHECK(hashGroupedEats != hashGroupedDrinks);
}

TEST_CASE("Query - custom grouped query refreshes on multiple group deps") {
	TestWorld twld;

	struct Marker {};

	auto root = wld.add();
	auto mid = wld.add();
	auto parentA = wld.add();
	auto parentB = wld.add();
	auto leafA = wld.add();
	auto leafB = wld.add();

	wld.add(parentA, ecs::Pair(ecs::Parent, mid));
	wld.add(mid, ecs::Pair(ecs::Parent, root));
	wld.add(parentB, ecs::Pair(ecs::Parent, root));

	wld.add(leafA, ecs::Pair(ecs::ChildOf, parentA));
	wld.add(leafB, ecs::Pair(ecs::ChildOf, parentB));

	wld.add<Position>(leafA, {0, 0, 0});
	wld.add<Position>(leafB, {0, 0, 0});
	wld.add<Marker>(leafA);
	wld.add<Marker>(leafB);

	auto group_by_parent_depth = []([[maybe_unused]] const ecs::World& world, const ecs::Archetype& archetype,
																	ecs::Entity groupBy) {
		if (archetype.pairs() == 0)
			return ecs::GroupId(0);

		auto ids = archetype.ids_view();
		for (auto id: ids) {
			if (!id.pair() || id.id() != groupBy.id())
				continue;

			auto curr = world.try_get(id.gen());
			ecs::GroupId depth = 1;
			constexpr uint32_t MaxTraversalDepth = 2048;
			GAIA_FOR(MaxTraversalDepth) {
				const auto next = world.target(curr, ecs::Parent);
				if (next == ecs::EntityBad || next == curr)
					break;
				++depth;
				curr = next;
			}

			return depth;
		}

		return ecs::GroupId(0);
	};

	auto q = wld.query()
							 .all<Position>()
							 .all<Marker>()
							 .group_by(ecs::ChildOf, group_by_parent_depth)
							 .group_dep(ecs::ChildOf)
							 .group_dep(ecs::Parent);

	auto collect_group = [&](ecs::GroupId groupId) {
		cnt::darr<ecs::Entity> ents;
		q.group_id(groupId).each([&](ecs::Entity e) {
			ents.push_back(e);
		});
		return ents;
	};

	{
		auto ents = collect_group(2);
		CHECK(ents.size() == 1);
		CHECK(ents[0] == leafB);
	}
	{
		auto ents = collect_group(3);
		CHECK(ents.size() == 1);
		CHECK(ents[0] == leafA);
	}

	wld.del(parentA, ecs::Pair(ecs::Parent, mid));

	{
		auto ents = collect_group(1);
		CHECK(ents.size() == 1);
		CHECK(ents[0] == leafA);
	}
	{
		auto ents = collect_group(2);
		CHECK(ents.size() == 1);
		CHECK(ents[0] == leafB);
	}
	CHECK(collect_group(3).empty());
}

TEST_CASE("Query - cached grouped queries with instance-local group filters") {
	TestWorld twld;

	auto eats = wld.add();
	auto carrot = wld.add();
	auto salad = wld.add();

	auto eCarrotA = wld.add();
	wld.add<Position>(eCarrotA, {1, 0, 0});
	wld.add(eCarrotA, ecs::Pair(eats, carrot));

	auto eCarrotB = wld.add();
	wld.add<Position>(eCarrotB, {2, 0, 0});
	wld.add(eCarrotB, ecs::Pair(eats, carrot));

	auto eSaladA = wld.add();
	wld.add<Position>(eSaladA, {3, 0, 0});
	wld.add(eSaladA, ecs::Pair(eats, salad));

	auto eSaladB = wld.add();
	wld.add<Position>(eSaladB, {4, 0, 0});
	wld.add(eSaladB, ecs::Pair(eats, salad));

	auto qCarrot = wld.query().all<Position>().group_by(eats);
	auto qSalad = wld.query().all<Position>().group_by(eats);

	qCarrot.group_id(carrot);
	qSalad.group_id(salad);

	CHECK(qCarrot.count() == 2);
	CHECK(qSalad.count() == 2);
	expect_exact_entities(qCarrot, {eCarrotA, eCarrotB});
	expect_exact_entities(qSalad, {eSaladA, eSaladB});
	{
		cnt::darray<ecs::Entity> carrotArr;
		cnt::darray<ecs::Entity> saladArr;
		qCarrot.arr(carrotArr);
		qSalad.arr(saladArr);
		CHECK(carrotArr.size() == 2);
		CHECK(saladArr.size() == 2);
		CHECK(carrotArr[0] == eCarrotA);
		CHECK(carrotArr[1] == eCarrotB);
		CHECK(saladArr[0] == eSaladA);
		CHECK(saladArr[1] == eSaladB);
	}
	CHECK(qCarrot.count() == 2);
	expect_exact_entities(qCarrot, {eCarrotA, eCarrotB});
}

TEST_CASE("QueryInfo - uncached transient single-archetype grouping") {
	TestWorld twld;

	const auto eats = wld.add();
	const auto carrot = wld.add();
	const auto salad = wld.add();

	const auto eCarrot = wld.add();
	wld.add<Position>(eCarrot, {1, 0, 0});
	wld.add(eCarrot, ecs::Pair(eats, carrot));

	const auto eSalad = wld.add();
	wld.add<Position>(eSalad, {2, 0, 0});
	wld.add<Something>(eSalad, {true});
	wld.add(eSalad, ecs::Pair(eats, salad));

	auto q = wld.uquery().all<Position>().group_by(eats);
	auto& info = q.fetch();

	const auto* pCarrotArchetype = wld.fetch(eCarrot).pArchetype;
	const auto* pSaladArchetype = wld.fetch(eSalad).pArchetype;
	CHECK(pCarrotArchetype != nullptr);
	CHECK(pSaladArchetype != nullptr);
	if (pCarrotArchetype == nullptr || pSaladArchetype == nullptr)
		return;

	info.test_add_transient_archetype(pCarrotArchetype);
	CHECK(info.cache_archetype_view().size() == 1);
	CHECK(info.group_data_view().size() == 1);
	CHECK(info.group_id(0) == ecs::GroupId(carrot.id()));
	{
		const auto* pCarrotGroup = info.selected_group_data(ecs::GroupId(carrot.id()));
		CHECK(pCarrotGroup != nullptr);
		if (pCarrotGroup == nullptr)
			return;
		CHECK(pCarrotGroup->groupId == ecs::GroupId(carrot.id()));
		CHECK(pCarrotGroup->idxFirst == 0);
		CHECK(pCarrotGroup->idxLast == 0);
	}
	CHECK(info.selected_group_data(ecs::GroupId(salad.id())) == nullptr);

	info.test_add_transient_archetype(pSaladArchetype);
	CHECK(info.cache_archetype_view().size() == 1);
	CHECK(info.group_data_view().size() == 1);
	CHECK(info.group_id(0) == ecs::GroupId(salad.id()));

	q.match_all(info);
	CHECK(q.count() == 2);
	CHECK(!info.cache_archetype_view().empty());
	CHECK(info.group_data_view().size() >= 2);
	{
		const auto* pCarrotGroup = info.selected_group_data(ecs::GroupId(carrot.id()));
		CHECK(pCarrotGroup != nullptr);
		if (pCarrotGroup != nullptr)
			CHECK(pCarrotGroup->groupId == ecs::GroupId(carrot.id()));
	}
	{
		const auto* pSaladGroup = info.selected_group_data(ecs::GroupId(salad.id()));
		CHECK(pSaladGroup != nullptr);
		if (pSaladGroup != nullptr)
			CHECK(pSaladGroup->groupId == ecs::GroupId(salad.id()));
	}
}

TEST_CASE("QueryInfo - inherited data view exposes inherited prefab payload") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(prefabAnimal, {5, 6, 7});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto instance = wld.instantiate(prefabAnimal);
	const auto* pInstanceArchetype = wld.fetch(instance).pArchetype;
	CHECK(pInstanceArchetype != nullptr);
	if (pInstanceArchetype == nullptr)
		return;

	auto q = wld.uquery().all<Position>();
	auto& info = q.fetch();
	CHECK(q.count() == 1);
	info.test_add_transient_archetype(pInstanceArchetype);

	const auto archetypes = info.cache_archetype_view();
	CHECK(archetypes.size() == 1);
	if (archetypes.size() != 1)
		return;
	const auto* pArchetype = archetypes[0];
	CHECK(pArchetype != nullptr);
	if (pArchetype == nullptr)
		return;

	CHECK(info.try_inherited_data_view(pArchetype).empty());

	const auto inheritedData = info.inherited_data_view(pArchetype);
	const auto terms = info.ctx().data.terms_view();
	CHECK(terms.size() == 1);
	if (terms.size() != 1)
		return;
	const auto fieldIdx = terms[0].fieldIndex;
	CHECK(fieldIdx < inheritedData.size());
	if (fieldIdx >= inheritedData.size())
		return;

	const auto* pInheritedPos = static_cast<const Position*>(inheritedData[fieldIdx]);
	CHECK(pInheritedPos != nullptr);
	if (pInheritedPos == nullptr)
		return;
	CHECK(pInheritedPos->x == doctest::Approx(5.0f));
	CHECK(pInheritedPos->y == doctest::Approx(6.0f));
	CHECK(pInheritedPos->z == doctest::Approx(7.0f));

	const auto inheritedDataCached = info.try_inherited_data_view(pArchetype);
	CHECK(fieldIdx < inheritedDataCached.size());
	if (fieldIdx >= inheritedDataCached.size())
		return;
	CHECK(inheritedDataCached[fieldIdx] == inheritedData[fieldIdx]);

	CHECK(wld.has(instance));
}

TEST_CASE("QueryInfo - diagnostics and explicit invalidation stay stable") {
	TestWorld twld;

	const auto e0 = wld.add();
	const auto e1 = wld.add();
	wld.add<Position>(e0, {1, 0, 0});
	wld.add<Position>(e1, {2, 0, 0});
	wld.add<Rotation>(e1, {3, 0, 0, 1});

	auto qA = wld.query().all<Position>();
	auto qB = wld.query().all<Position>();
	auto qC = wld.query().all<Position>().no<Rotation>();

	auto& infoA = qA.fetch();
	auto& infoB = qB.fetch();
	auto& infoC = qC.fetch();

	CHECK(infoA.op_count() > 0);
	CHECK(infoA.op_signature() != 0);
	CHECK(infoA.op_count() == infoB.op_count());
	CHECK(infoA.op_signature() == infoB.op_signature());
	CHECK(infoA.op_signature() != infoC.op_signature());

	qA.match_all(infoA);
	CHECK(qA.count() == 2);
	const auto archetypeCntBefore = infoA.cache_archetype_view().size();

	infoA.invalidate_seed();
	qA.match_all(infoA);
	CHECK(qA.count() == 2);
	CHECK(infoA.cache_archetype_view().size() == archetypeCntBefore);

	infoA.invalidate_result();
	qA.match_all(infoA);
	CHECK(qA.count() == 2);
	CHECK(infoA.cache_archetype_view().size() == archetypeCntBefore);
}

TEST_CASE("Query - cached relation wildcard query after repeated pair additions") {
	TestWorld twld;
	static constexpr uint32_t PairCount = 30;

	auto rel = wld.add();
	cnt::darray<ecs::Entity> targets;
	targets.reserve(PairCount);
	GAIA_FOR(PairCount) {
		targets.push_back(wld.add());
	}

	auto q = wld.query().all(ecs::Pair{rel, ecs::All}).no<ecs::Core_>().no<ecs::System_>();
	CHECK(q.count() == 0);

	auto e = wld.add();
	GAIA_FOR(PairCount) {
		wld.add(e, ecs::Pair(rel, targets[i]));
	}

	CHECK(q.count() == 1);
}

TEST_CASE("Component index exact and wildcard pair match counts") {
	TestWorld twld;

	const auto rel0 = wld.add();
	const auto rel1 = wld.add();
	const auto tgt0 = wld.add();
	const auto tgt1 = wld.add();

	const auto e = wld.add();
	wld.add(e, ecs::Pair(rel0, tgt0));
	wld.add(e, ecs::Pair(rel0, tgt1));
	wld.add(e, ecs::Pair(rel1, tgt1));

	const auto* pArchetype = wld.fetch(e).pArchetype;
	CHECK(pArchetype != nullptr);
	if (pArchetype == nullptr)
		return;

	CHECK(ecs::world_component_index_match_count(wld, *pArchetype, ecs::Pair(rel0, tgt0)) == 1);
	CHECK(ecs::world_component_index_match_count(wld, *pArchetype, ecs::Pair(rel0, tgt1)) == 1);
	CHECK(ecs::world_component_index_match_count(wld, *pArchetype, ecs::Pair(rel1, tgt1)) == 1);
	CHECK(ecs::world_component_index_match_count(wld, *pArchetype, ecs::Pair(rel0, ecs::All)) == 2);
	CHECK(ecs::world_component_index_match_count(wld, *pArchetype, ecs::Pair(ecs::All, tgt1)) == 2);
	CHECK(ecs::world_component_index_match_count(wld, *pArchetype, ecs::Pair(ecs::All, ecs::All)) == 3);
}

TEST_CASE("Component index exact term entries across add and delete archetype moves") {
	TestWorld twld;

	const auto pos = wld.add<Position>().entity;
	const auto game = wld.add();

	const auto* pRootArchetype = wld.fetch(game).pArchetype;
	CHECK(pRootArchetype != nullptr);
	if (pRootArchetype == nullptr)
		return;
	CHECK(ecs::world_component_index_comp_idx(wld, *pRootArchetype, pos) == BadIndex);
	CHECK(ecs::world_component_index_match_count(wld, *pRootArchetype, pos) == 0);

	wld.add<Position>(game, {1, 2, 3});
	const auto* pValueArchetype = wld.fetch(game).pArchetype;
	CHECK(pValueArchetype != nullptr);
	if (pValueArchetype == nullptr)
		return;
	CHECK(ecs::world_component_index_comp_idx(wld, *pValueArchetype, pos) != BadIndex);
	CHECK(ecs::world_component_index_match_count(wld, *pValueArchetype, pos) == 1);

	wld.del<Position>(game);
	const auto* pRootAgain = wld.fetch(game).pArchetype;
	CHECK(pRootAgain != nullptr);
	if (pRootAgain == nullptr)
		return;
	CHECK(ecs::world_component_index_comp_idx(wld, *pRootAgain, pos) == BadIndex);
	CHECK(ecs::world_component_index_match_count(wld, *pRootAgain, pos) == 0);
}

TEST_CASE("Query - exact owned term matcher with inherited fallback") {
	TestWorld twld;

	const auto owned = wld.add();
	const auto ownedEntity = wld.add();
	wld.add(ownedEntity, owned);

	const auto prefab = wld.prefab();
	wld.add(prefab, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
	wld.add<Position>(prefab, {1, 2, 3});

	const auto instance = wld.instantiate(prefab);

	auto qOwned = wld.uquery().all(owned);
	CHECK(qOwned.count() == 1);
	expect_exact_entities(qOwned, {ownedEntity});

	auto qInherited = wld.uquery().all<Position>();
	CHECK(qInherited.count() == 1);
	expect_exact_entities(qInherited, {instance});
}

TEST_CASE("Query - cached local and shared query state is immediately updated by cache propagation") {
	TestWorld twld;

	auto qShared = wld.query().scope(ecs::QueryCacheScope::Shared).all<Position>();
	auto qLocal = wld.query().all<Position>();
	auto qNone = wld.query().kind(ecs::QueryCacheKind::None).all<Position>();
	auto qUncached = wld.uquery().all<Position>();

	auto& sharedInfo = qShared.fetch();
	auto& localInfo = qLocal.fetch();
	auto& noneInfo = qNone.fetch();
	auto& uncachedInfo = qUncached.fetch();
	qShared.match_all(sharedInfo);
	qLocal.match_all(localInfo);
	qNone.match_all(noneInfo);
	qUncached.match_all(uncachedInfo);

	CHECK(sharedInfo.cache_archetype_view().empty());
	CHECK(localInfo.cache_archetype_view().empty());
	CHECK(noneInfo.cache_archetype_view().empty());
	CHECK(uncachedInfo.cache_archetype_view().empty());

	auto e = wld.add();
	wld.add<Position>(e, {1, 0, 0});

	CHECK(sharedInfo.cache_archetype_view().size() == 1);
	CHECK(localInfo.cache_archetype_view().size() == 1);
	CHECK(noneInfo.cache_archetype_view().empty());
	CHECK(uncachedInfo.cache_archetype_view().empty());
	CHECK(qShared.count() == 1);
	CHECK(qLocal.count() == 1);
	CHECK(qNone.count() == 1);
	CHECK(qUncached.count() == 1);
}

TEST_CASE("Query - cached cursor resets after lookup bucket revision changes") {
	ecs::QueryArchetypeCacheIndexMap cursors;
	const auto key = ecs::EntityLookupKey(ecs::Entity(42, 0));

	CHECK(ecs::vm::detail::handle_last_archetype_match(&cursors, key, 5, 1) == 0);
	CHECK(cursors[key].index == 5);
	CHECK(cursors[key].revision == 1);

	CHECK(ecs::vm::detail::handle_last_archetype_match(&cursors, key, 7, 1) == 5);
	CHECK(cursors[key].index == 7);
	CHECK(cursors[key].revision == 1);

	CHECK(ecs::vm::detail::handle_last_archetype_match(&cursors, key, 6, 2) == 0);
	CHECK(cursors[key].index == 6);
	CHECK(cursors[key].revision == 2);

	CHECK(ecs::vm::detail::handle_last_archetype_match(&cursors, ecs::EntityBadLookupKey, 4, 1) == 0);
	CHECK(cursors[ecs::EntityBadLookupKey].index == 4);
	CHECK(cursors[ecs::EntityBadLookupKey].revision == 1);

	CHECK(ecs::vm::detail::handle_last_archetype_match(&cursors, ecs::EntityBadLookupKey, 3, 2) == 0);
	CHECK(cursors[ecs::EntityBadLookupKey].index == 3);
	CHECK(cursors[ecs::EntityBadLookupKey].revision == 2);
}

TEST_CASE("Query - cached structural query drops deleted archetypes after gc") {
	TestWorld twld;

	auto e = wld.add();
	wld.add<Position>(e, {1, 2, 3});
	wld.add<Acceleration>(e, {4, 5, 6});
	wld.set_max_lifespan(e, 1);

	auto q = wld.query().all<Position&>().all<Acceleration&>();
	CHECK(q.count() == 1);

	auto& info = q.fetch();
	CHECK(info.cache_archetype_view().size() == 1);

	wld.del<Acceleration>(e);
	twld.update();

	CHECK(q.count() == 0);
	CHECK(info.cache_archetype_view().empty());
}

TEST_CASE("Query - cached structural query matches uncached count across archetype reuse") {
	struct CTransform {
		float x = 0.0f;
		float y = 0.0f;
	};
	struct CProjectile {
		float vx = 0.0f;
		float vy = 0.0f;
	};

	enum class Mode : uint8_t { PersistentCached, LocalCachedEachFrame };

	auto runScenario = [](Mode mode, int& failCycle, uint32_t& cachedCountOut, uint32_t& referenceCountOut) {
		ecs::World world;
		auto qReference = world.uquery().all<CTransform&>().all<CProjectile&>();
		ecs::Query qPersistent;
		if (mode == Mode::PersistentCached)
			qPersistent = world.query().all<CTransform&>().all<CProjectile&>();

		for (int cycle = 0; cycle < 100; ++cycle) {
			{
				auto& cb = world.cmd_buffer_st();
				const auto e = cb.add();
				cb.add<CTransform>(e, {1.0f, 2.0f});
				cb.add<CProjectile>(e, {3.0f, 4.0f});
				cb.commit();
			}

			uint32_t cachedCount = 0;
			if (mode == Mode::PersistentCached) {
				cachedCount = qPersistent.count();
			} else {
				auto qLocal = world.query().all<CTransform&>().all<CProjectile&>();
				cachedCount = qLocal.count();
			}

			const uint32_t referenceCount = qReference.count();
			if (cachedCount != referenceCount) {
				failCycle = cycle;
				cachedCountOut = cachedCount;
				referenceCountOut = referenceCount;
				return false;
			}

			cnt::darr<ecs::Entity> toDelete;
			qReference.each([&](ecs::Iter& it) {
				const auto entities = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					toDelete.push_back(entities[i]);
				}
			});

			auto& cb = world.cmd_buffer_st();
			for (const auto e: toDelete)
				cb.del(e);
			cb.commit();

			world.update();
			if ((cycle % 7) == 0)
				world.update();
		}

		return true;
	};

	int failCycle = -1;
	uint32_t cachedCount = 0;
	uint32_t referenceCount = 0;
	CHECK(runScenario(Mode::PersistentCached, failCycle, cachedCount, referenceCount));
	CHECK(failCycle == -1);
	CHECK(cachedCount == referenceCount);

	failCycle = -1;
	cachedCount = 0;
	referenceCount = 0;
	CHECK(runScenario(Mode::LocalCachedEachFrame, failCycle, cachedCount, referenceCount));
	CHECK(failCycle == -1);
	CHECK(cachedCount == referenceCount);
}

TEST_CASE("Query - cached structural queries stay coherent across deterministic archetype changes") {
	struct CacheStressA {
		uint32_t v = 0;
	};
	struct CacheStressB {
		uint32_t v = 0;
	};
	struct CacheStressC {
		uint32_t v = 0;
	};
	struct CacheStressD {
		uint32_t v = 0;
	};

	TestWorld twld;

	const auto compA = wld.add<CacheStressA>().entity;
	const auto compB = wld.add<CacheStressB>().entity;
	const auto compC = wld.add<CacheStressC>().entity;
	const auto compD = wld.add<CacheStressD>().entity;
	const auto rel0 = wld.add();
	const auto rel1 = wld.add();
	const auto tgt0 = wld.add();
	const auto tgt1 = wld.add();

	auto qA = wld.query().all(compA);
	auto qAB = wld.query().all(compA).all(compB);
	auto qOrBC = wld.query().or_(compB).or_(compC);
	auto qNoD = wld.query().all(compA).no(compD);
	auto qPairExact = wld.query().all(ecs::Pair(rel0, tgt0));
	auto qPairRelAny = wld.query().all(ecs::Pair(rel0, ecs::All));
	auto qPairTgtAny = wld.query().all(ecs::Pair(ecs::All, tgt0));

	cnt::darr<ecs::Entity> live;
	cnt::darr<ecs::Entity> cachedEntities;
	cnt::darr<ecs::Entity> referenceEntities;

	uint32_t rng = 0x12345678;
	auto nextRand = [&]() {
		rng = rng * 1664525U + 1013904223U;
		return rng;
	};

	auto collect = [](ecs::Query& query, cnt::darr<ecs::Entity>& out) {
		out.clear();
		query.each([&](ecs::Entity entity) {
			out.push_back(entity);
		});
	};

	auto sameEntitySet = [](const cnt::darr<ecs::Entity>& left, const cnt::darr<ecs::Entity>& right) {
		if (left.size() != right.size())
			return false;

		for (const auto entity: left) {
			bool found = false;
			for (const auto candidate: right) {
				if (candidate != entity)
					continue;

				found = true;
				break;
			}

			if (!found)
				return false;
		}

		return true;
	};

	auto addEntity = [&](uint32_t seed, uint32_t cycle) {
		const auto entity = wld.add();
		if ((seed & 0x01U) != 0)
			wld.add<CacheStressA>(entity, {cycle});
		if ((seed & 0x02U) != 0)
			wld.add<CacheStressB>(entity, {cycle + 1});
		if ((seed & 0x04U) != 0)
			wld.add<CacheStressC>(entity, {cycle + 2});
		if ((seed & 0x08U) != 0)
			wld.add<CacheStressD>(entity, {cycle + 3});
		if ((seed & 0x10U) != 0)
			wld.add(entity, ecs::Pair(rel0, tgt0));
		if ((seed & 0x20U) != 0)
			wld.add(entity, ecs::Pair(rel0, tgt1));
		if ((seed & 0x40U) != 0)
			wld.add(entity, ecs::Pair(rel1, tgt0));

		live.push_back(entity);
	};

	auto pickLive = [&]() {
		return nextRand() % live.size();
	};

	auto mutateLiveEntity = [&](uint32_t cycle) {
		if (live.empty())
			return;

		const auto liveIdx = pickLive();
		const auto entity = live[liveIdx];
		if (!wld.has(entity)) {
			core::swap_erase(live, liveIdx);
			return;
		}

		switch (nextRand() % 8U) {
			case 0:
				if (wld.has<CacheStressA>(entity))
					wld.del<CacheStressA>(entity);
				else
					wld.add<CacheStressA>(entity, {cycle});
				break;
			case 1:
				if (wld.has<CacheStressB>(entity))
					wld.del<CacheStressB>(entity);
				else
					wld.add<CacheStressB>(entity, {cycle});
				break;
			case 2:
				if (wld.has<CacheStressC>(entity))
					wld.del<CacheStressC>(entity);
				else
					wld.add<CacheStressC>(entity, {cycle});
				break;
			case 3:
				if (wld.has<CacheStressD>(entity))
					wld.del<CacheStressD>(entity);
				else
					wld.add<CacheStressD>(entity, {cycle});
				break;
			case 4:
				if (wld.has(entity, ecs::Pair(rel0, tgt0)))
					wld.del(entity, ecs::Pair(rel0, tgt0));
				else
					wld.add(entity, ecs::Pair(rel0, tgt0));
				break;
			case 5:
				if (wld.has(entity, ecs::Pair(rel0, tgt1)))
					wld.del(entity, ecs::Pair(rel0, tgt1));
				else
					wld.add(entity, ecs::Pair(rel0, tgt1));
				break;
			case 6:
				if (wld.has(entity, ecs::Pair(rel1, tgt0)))
					wld.del(entity, ecs::Pair(rel1, tgt0));
				else
					wld.add(entity, ecs::Pair(rel1, tgt0));
				break;
			default:
				wld.del(entity);
				core::swap_erase(live, liveIdx);
				break;
		}
	};

	auto compare = [&](const char* label, uint32_t cycle, ecs::Query& cached, ecs::Query reference) {
		CAPTURE(label);
		CAPTURE(cycle);

		collect(cached, cachedEntities);
		collect(reference, referenceEntities);

		CHECK(cachedEntities.size() == referenceEntities.size());
		CHECK(sameEntitySet(cachedEntities, referenceEntities));
		CHECK(wld.verify_query_cache());
	};

	for (uint32_t cycle = 0; cycle < 160; ++cycle) {
		const auto addCount = 1 + (nextRand() % 3U);
		for (uint32_t i = 0; i < addCount; ++i)
			addEntity(nextRand(), cycle);

		const auto mutationCount = 1 + (nextRand() % 4U);
		for (uint32_t i = 0; i < mutationCount; ++i)
			mutateLiveEntity(cycle);

		if ((cycle % 5U) == 0)
			wld.update();
		if ((cycle % 17U) == 0)
			twld.update();

		compare("all A", cycle, qA, wld.uquery().all(compA));
		compare("all A B", cycle, qAB, wld.uquery().all(compA).all(compB));
		compare("or B C", cycle, qOrBC, wld.uquery().or_(compB).or_(compC));
		compare("all A no D", cycle, qNoD, wld.uquery().all(compA).no(compD));
		compare("pair exact", cycle, qPairExact, wld.uquery().all(ecs::Pair(rel0, tgt0)));
		compare("pair rel wildcard", cycle, qPairRelAny, wld.uquery().all(ecs::Pair(rel0, ecs::All)));
		compare("pair tgt wildcard", cycle, qPairTgtAny, wld.uquery().all(ecs::Pair(ecs::All, tgt0)));

		auto qLocal = wld.query().all(compA).all(ecs::Pair(rel0, ecs::All));
		compare("local all A pair rel wildcard", cycle, qLocal, wld.uquery().all(compA).all(ecs::Pair(rel0, ecs::All)));
	}

	twld.update();
	CHECK(wld.verify_query_cache());
}

TEST_CASE("Query - cached structural query across immediate add and gc") {
	TestWorld twld;

	auto e0 = wld.add();
	wld.add<Position>(e0, {1, 0, 0});
	wld.add<Something>(e0, {false});

	auto q = wld.query().all<Position>().all<Something>();
	auto& info = q.fetch();

	CHECK(q.count() == 1);
	CHECK(q.count() == 1);
	CHECK(info.cache_archetype_view().size() == 1);

	auto e1 = wld.add();
	wld.add<Position>(e1, {2, 0, 0});
	wld.add<Something>(e1, {true});
	wld.add<Acceleration>(e1, {1, 0, 0});
	CHECK(info.cache_archetype_view().size() == 2);
	CHECK(q.count() == 2);
	CHECK(info.cache_archetype_view().size() == 2);

	wld.del<Something>(e1);
	twld.update();

	CHECK(q.count() == 1);
	CHECK(info.cache_archetype_view().size() == 1);
}

TEST_CASE("Query - cached query destruction unregisters archetype reverse index") {
	TestWorld twld;

	auto e = wld.add();
	wld.add<Position>(e, {1, 2, 3});
	wld.add<Acceleration>(e, {4, 5, 6});
	wld.set_max_lifespan(e, 1);

	{
		auto q = wld.query().all<Position&>().all<Acceleration&>();
		CHECK(q.count() == 1);
	}

	wld.del<Acceleration>(e);
	twld.update();

	CHECK(wld.query().all<Position&>().all<Acceleration&>().count() == 0);
}

TEST_CASE("Query - cached reverse index across repeated immediate add gc and destruction") {
	TestWorld twld;
	for (int i = 0; i < 256; ++i) {
		auto e0 = wld.add();
		wld.add<Position>(e0, {1, 0, 0});
		wld.add<Something>(e0, {false});

		{
			auto q = wld.query().all<Position>().all<Something>();
			auto& info = q.fetch();

			CHECK(q.count() == 1);
			CHECK(info.cache_archetype_view().size() == 1);

			auto e1 = wld.add();
			wld.add<Position>(e1, {2, 0, 0});
			wld.add<Something>(e1, {true});
			wld.add<Acceleration>(e1, {1, 0, 0});

			CHECK(info.cache_archetype_view().size() == 2);

			wld.del<Something>(e1);
			twld.update();

			CHECK(q.count() == 1);
			CHECK(info.cache_archetype_view().size() == 1);
		}

		CHECK(wld.query().all<Position>().all<Something>().count() == 1);

		wld.del(e0);
		twld.update();

		CHECK(wld.query().all<Position>().all<Something>().count() == 0);
	}
}
