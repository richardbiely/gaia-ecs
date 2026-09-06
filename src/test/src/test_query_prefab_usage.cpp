#include "test_common.h"

#define TestWorld SparseTestWorld

TEST_CASE("Usage 1 - simple query, 0 component") {
	TestWorld twld;

	auto e = wld.add();

	auto qa = wld.query().all<Acceleration>();
	auto qp = wld.query().all<Position>();

	{
		uint32_t cnt = 0;
		qa.each([&]([[maybe_unused]] const Acceleration&) {
			++cnt;
		});
		CHECK(cnt == 0);
	}
	{
		uint32_t cnt = 0;
		qp.each([&]([[maybe_unused]] const Position&) {
			++cnt;
		});
		CHECK(cnt == 0);
	}

	auto e1 = wld.copy(e);
	auto e2 = wld.copy(e);
	auto e3 = wld.copy(e);

	{
		uint32_t cnt = 0;
		qp.each([&]([[maybe_unused]] const Position&) {
			++cnt;
		});
		CHECK(cnt == 0);
	}

	wld.del(e1);

	{
		uint32_t cnt = 0;
		qp.each([&]([[maybe_unused]] const Position&) {
			++cnt;
		});
		CHECK(cnt == 0);
	}

	wld.del(e2);
	wld.del(e3);
	wld.del(e);

	{
		uint32_t cnt = 0;
		qp.each([&]([[maybe_unused]] const Position&) {
			++cnt;
		});
		CHECK(cnt == 0);
	}
}

TEST_CASE("Usage 1 - simple query, 1 component") {
	TestWorld twld;

	auto e = wld.add();
	wld.add<Position>(e);

	auto qa = wld.query().all<Acceleration>();
	auto qp = wld.query().all<Position>();

	{
		uint32_t cnt = 0;
		qa.each([&]([[maybe_unused]] const Acceleration&) {
			++cnt;
		});
		CHECK(cnt == 0);
	}
	{
		uint32_t cnt = 0;
		qp.each([&]([[maybe_unused]] const Position&) {
			++cnt;
		});
		CHECK(cnt == 1);
	}

	auto e1 = wld.copy(e);
	auto e2 = wld.copy(e);
	auto e3 = wld.copy(e);

	{
		uint32_t cnt = 0;
		qp.each([&]([[maybe_unused]] const Position&) {
			++cnt;
		});
		CHECK(cnt == 4);
	}

	wld.del(e1);

	{
		uint32_t cnt = 0;
		qp.each([&]([[maybe_unused]] const Position&) {
			++cnt;
		});
		CHECK(cnt == 3);
	}

	wld.del(e2);
	wld.del(e3);
	wld.del(e);

	{
		uint32_t cnt = 0;
		qp.each([&]([[maybe_unused]] const Position&) {
			++cnt;
		});
		CHECK(cnt == 0);
	}
}


TEST_CASE("Usage 2 - simple query, many components") {
	TestWorld twld;

	auto e1 = wld.add();
	wld.add<Position>(e1, {});
	wld.add<Acceleration>(e1, {});
	wld.add<Else>(e1, {});
	auto e2 = wld.add();
	wld.add<Rotation>(e2, {});
	wld.add<Scale>(e2, {});
	wld.add<Else>(e2, {});
	auto e3 = wld.add();
	wld.add<Position>(e3, {});
	wld.add<Acceleration>(e3, {});
	wld.add<Scale>(e3, {});

	{
		uint32_t cnt = 0;
		auto q = wld.query().all<Position>();
		q.each([&]([[maybe_unused]] const Position&) {
			++cnt;
		});
		CHECK(cnt == 2);
	}
	{
		uint32_t cnt = 0;
		auto q = wld.query().all<Acceleration>();
		q.each([&]([[maybe_unused]] const Acceleration&) {
			++cnt;
		});
		CHECK(cnt == 2);
	}
	{
		uint32_t cnt = 0;
		auto q = wld.query().all<Rotation>();
		q.each([&]([[maybe_unused]] const Rotation&) {
			++cnt;
		});
		CHECK(cnt == 1);
	}
	{
		uint32_t cnt = 0;
		auto q = wld.query().all<Scale>();
		q.each([&]([[maybe_unused]] const Scale&) {
			++cnt;
		});
		CHECK(cnt == 2);
	}
	{
		uint32_t cnt = 0;
		auto q = wld.query().all<Position>().all<Acceleration>();
		q.each([&]([[maybe_unused]] const Position&, [[maybe_unused]] const Acceleration&) {
			++cnt;
		});
		CHECK(cnt == 2);
	}
	{
		uint32_t cnt = 0;
		auto q = wld.query().all<Position>().all<Scale>();
		q.each([&]([[maybe_unused]] const Position&, [[maybe_unused]] const Scale&) {
			++cnt;
		});
		CHECK(cnt == 1);
	}
	{
		auto q = wld.query().all<Position&>().all<Acceleration&>();

		uint32_t cnt = 0;
		q.each([&](ecs::Entity entity, Position& position, Acceleration& acceleration) {
			++cnt;

			const bool isExpectedEntity = entity == e1 || entity == e3;
			CHECK(isExpectedEntity);
			position.x += 1.0f;
			acceleration.y += 2.0f;
		});
		CHECK(cnt == 2);
	}
	{
		auto q = wld.query().all<Position&>().all<Acceleration&>();

		uint32_t cnt = 0;
		q.each([&](Position& position, ecs::Entity entity, Acceleration& acceleration) {
			++cnt;

			const bool isExpectedEntity = entity == e1 || entity == e3;
			CHECK(isExpectedEntity);
			position.x += 1.0f;
			acceleration.y += 2.0f;
		});
		CHECK(cnt == 2);
	}
	{
		auto q = wld.query().all<Position&>().all<Acceleration&>();

		uint32_t cnt = 0;
		q.each([&](Position& position, Acceleration& acceleration, ecs::Entity entity) {
			++cnt;

			const bool isExpectedEntity = entity == e1 || entity == e3;
			CHECK(isExpectedEntity);
			position.x += 1.0f;
			acceleration.y += 2.0f;
		});
		CHECK(cnt == 2);
	}
	{
		ecs::Query q = wld.query().or_<Position>().or_<Acceleration>();

		uint32_t cnt = 0;
		q.each([&](ecs::Iter& it) {
			++cnt;

			const bool ok1 = it.has<Position>() || it.has<Acceleration>();
			CHECK(ok1);
			const bool ok2 = it.has<Acceleration>() || it.has<Position>();
			CHECK(ok2);
		});
		CHECK(cnt == 2);
	}
	{
		ecs::Query q = wld.query().or_<Position>().or_<Acceleration>().all<Scale>();

		uint32_t cnt = 0;
		q.each([&](ecs::Iter& it) {
			++cnt;

			CHECK(it.size() == 1);

			const bool ok1 = it.has<Position>() || it.has<Acceleration>();
			CHECK(ok1);
			const bool ok2 = it.has<Acceleration>() || it.has<Position>();
			CHECK(ok2);
		});
		CHECK(cnt == 1);
	}
	{
		ecs::Query q = wld.query().or_<Position>().or_<Acceleration>().all<PositionSoA>();

		uint32_t cnt = 0;
		q.each([&]() {
			++cnt;
		});
		CHECK(cnt == 0);
	}
	{
		ecs::Query q = wld.query().or_<Position>().or_<Acceleration>().no<Scale>();

		uint32_t cnt = 0;
		q.each([&](ecs::Iter& it) {
			++cnt;

			CHECK(it.size() == 1);
		});
		CHECK(cnt == 1);
	}
}


TEST_CASE("Usage 3 - simple query, no") {
	TestWorld twld;

	auto e1 = wld.add();
	wld.add<Position>(e1, {});
	wld.add<Acceleration>(e1, {});
	wld.add<Else>(e1, {});
	auto e2 = wld.add();
	wld.add<Rotation>(e2, {});
	wld.add<Scale>(e2, {});
	wld.add<Else>(e2, {});
	auto e3 = wld.add();
	wld.add<Position>(e3, {});
	wld.add<Acceleration>(e3, {});
	wld.add<Scale>(e3, {});

	auto s1 = wld.system().name("s1").all<Position>().on_each([]() {}).entity();
	auto s2 = wld.system().name("s2").on_each([]() {}).entity();

	// More complex NO query, 2 operators.
	SUBCASE("NO") {
		uint32_t cnt = 0;
		auto q = wld.query();
		q.no(gaia::ecs::System).no(gaia::ecs::Core);
		q.each([&](ecs::Entity e) {
			++cnt;

			const bool ok = e > ecs::GAIA_ID_LastCoreComponent && e != s1 && e != s2;
			CHECK(ok);
		});
		// +2 for (OnDelete, Error) and (OnTargetDelete, Error)
		// +3 for e1, e2, e3
		CHECK(cnt == 5);
	}

	// More complex NO query, 3 operators = 2x NO, 1x ALL.
	SUBCASE("ALL+NO") {
		uint32_t cnt = 0;
		auto q = wld.query();
		q.all<Position>().no(gaia::ecs::System).no(gaia::ecs::Core);
		q.each([&](ecs::Entity e) {
			++cnt;

			const bool ok = e == e1 || e == e3;
			CHECK(ok);
		});
		// e1, e3
		CHECK(cnt == 2);
	}
}

TEST_CASE("Query - all/any eval after new archetypes are created") {
	TestWorld twld;

	auto e1 = wld.add();
	auto e2 = wld.add();
	wld.add(e1, e1);
	wld.add(e2, e2);

	auto qAll = wld.query().all(e1).all(e2);
	CHECK(qAll.count() == 0);
	expect_exact_entities(qAll, {});

	// any(e2) is optional and should not filter entities that already satisfy all(e1).
	auto qAny = wld.query().all(e1).any(e2);
	CHECK(qAny.count() == 1);
	expect_exact_entities(qAny, {e1});

	auto qOr = wld.query().or_(e1).or_(e2);
	CHECK(qOr.count() == 2);
	expect_exact_entities(qOr, {e1, e2});

	auto e3 = wld.add();
	wld.add(e3, e3);
	wld.add(e3, e1);
	wld.add(e3, e2);
	CHECK(qAll.count() == 1);
	expect_exact_entities(qAll, {e3});
	CHECK(qAny.count() == 2);
	expect_exact_entities(qAny, {e1, e3});
	CHECK(qOr.count() == 3);
	expect_exact_entities(qOr, {e1, e2, e3});

	auto e4 = wld.add();
	wld.add(e4, e4);
	wld.add(e4, e1);
	wld.add(e4, e2);
	CHECK(qAll.count() == 2);
	expect_exact_entities(qAll, {e3, e4});
	CHECK(qAny.count() == 3);
	expect_exact_entities(qAny, {e1, e3, e4});
	CHECK(qOr.count() == 4);
	expect_exact_entities(qOr, {e1, e2, e3, e4});
}

TEST_CASE("Query - cached component query after entity creation") {
	TestWorld twld;

	auto qCached = wld.query().all<Position&>().all<Acceleration&>();

	// Compile/cache the query before any matching archetype exists.
	CHECK(qCached.count() == 0);

	{
		// No matching archetype yet.
		auto e = wld.add();
		wld.add<Position>(e, {1, 2, 3});

		CHECK(qCached.count() == 0);
		CHECK(wld.query().all<Position&>().all<Acceleration&>().count() == 0);
	}

	{
		// Matching archetype appears after cached query creation.
		auto e = wld.add();
		wld.add<Position>(e, {1, 2, 3});
		wld.add<Acceleration>(e, {4, 5, 6});

		CHECK(qCached.count() == 1);
		CHECK(wld.query().all<Position&>().all<Acceleration&>().count() == 1);
	}

	{
		// Additional matching entity should also be visible.
		auto e = wld.add();
		wld.add<Position>(e, {7, 8, 9});
		wld.add<Acceleration>(e, {10, 11, 12});

		CHECK(qCached.count() == 2);
		CHECK(wld.query().all<Position&>().all<Acceleration&>().count() == 2);
	}
}

TEST_CASE("Query - cached OR query after entity creation") {
	TestWorld twld;

	auto tagA = wld.add();
	auto tagB = wld.add();
	auto qCached = wld.query().or_(tagA).or_(tagB);

	// Compile/cache before any matching archetype exists.
	CHECK(qCached.count() == 0);
	CHECK(wld.uquery().or_(tagA).or_(tagB).count() == 0);

	// Add matching archetype after query creation.
	auto e = wld.add();
	wld.add(e, tagA);

	CHECK(qCached.count() == 1);
	CHECK(wld.uquery().or_(tagA).or_(tagB).count() == 1);
}

TEST_CASE("Query - cached OR query with secondary selector archetypes") {
	TestWorld twld;

	auto tagA = wld.add();
	auto tagB = wld.add();
	auto q = wld.query().or_(tagA).or_(tagB);
	auto& info = q.fetch();
	q.match_all(info);
	CHECK(info.cache_archetype_view().empty());

	auto e = wld.add();
	wld.add(e, tagB);
	CHECK(info.cache_archetype_view().size() == 1);
	CHECK(q.count() == 1);
}

TEST_CASE("Query - cached exact and OR query after matching archetype creation") {
	TestWorld twld;

	auto q = wld.query().all<Position>().or_<Scale>().or_<Acceleration>();
	auto& info = q.fetch();
	q.match_all(info);
	CHECK(info.cache_archetype_view().empty());
	CHECK(q.count() == 0);

	auto e = wld.add();
	wld.add<Position>(e, {1, 0, 0});
	wld.add<Acceleration>(e, {0, 1, 0});

	CHECK(info.cache_archetype_view().size() == 1);
	CHECK(q.count() == 1);
}

TEST_CASE("Query - cached exact and ANY query after matching archetype creation") {
	TestWorld twld;

	auto q = wld.query().all<Position>().any<Rotation>();
	auto& info = q.fetch();
	q.match_all(info);
	CHECK(info.cache_archetype_view().empty());
	CHECK(q.count() == 0);

	auto e = wld.add();
	wld.add<Position>(e, {1, 0, 0});

	CHECK(info.cache_archetype_view().size() == 1);
	CHECK(q.count() == 1);
}

TEST_CASE("Query - cached exact and ANY query without positive selectors") {
	TestWorld twld;

	auto q = wld.query().all<Position>().any<Rotation>();
	auto& info = q.fetch();
	q.match_all(info);
	CHECK(info.cache_archetype_view().empty());
	CHECK(q.count() == 0);

	auto e = wld.add();
	wld.add<Rotation>(e, {1, 0, 0, 0});

	CHECK(info.cache_archetype_view().empty());
	CHECK(q.count() == 0);
}

TEST_CASE("Query - cached structural query after matching archetype creation") {
	TestWorld twld;

	auto q = wld.query().all<Position&>().all<Acceleration&>();
	auto& info = q.fetch();
	q.match_all(info);
	CHECK(info.cache_archetype_view().empty());

	auto e = wld.add();
	wld.add<Position>(e, {1, 2, 3});
	CHECK(info.cache_archetype_view().empty());

	wld.add<Acceleration>(e, {4, 5, 6});
	CHECK(info.cache_archetype_view().size() == 1);
	CHECK(q.count() == 1);
}

TEST_CASE("Query - broad-first exact query with selective match") {
	TestWorld twld;

	auto q = wld.query().all<Position>().all<Rotation>();
	auto& info = q.fetch();
	q.match_all(info);
	CHECK(info.cache_archetype_view().empty());
	CHECK(q.count() == 0);

	auto eBroad = wld.add();
	wld.add<Position>(eBroad, {1, 2, 3});
	CHECK(info.cache_archetype_view().empty());
	CHECK(q.count() == 0);

	auto eMatch = wld.add();
	wld.add<Position>(eMatch, {4, 5, 6});
	wld.add<Rotation>(eMatch, {7, 8, 9, 0});

	CHECK(info.cache_archetype_view().size() == 1);
	CHECK(q.count() == 1);
}

TEST_CASE("Query - cached single-term structural query after matching archetype creation") {
	TestWorld twld;

	const auto tag = wld.add();
	auto q = wld.query().all(tag);
	auto& info = q.fetch();
	q.match_all(info);
	CHECK(info.cache_archetype_view().empty());

	auto e = wld.add();
	wld.add(e, tag);
	CHECK(info.cache_archetype_view().size() == 1);
	CHECK(q.count() == 1);
}

TEST_CASE("Query - cached four-term structural query after matching archetype creation") {
	TestWorld twld;

	const auto tagA = wld.add();
	const auto tagB = wld.add();
	const auto tagC = wld.add();
	const auto tagD = wld.add();

	auto q = wld.query().all(tagA).all(tagB).all(tagC).all(tagD);
	auto& info = q.fetch();
	q.match_all(info);
	CHECK(info.cache_archetype_view().empty());

	auto e = wld.add();
	wld.add(e, tagA);
	wld.add(e, tagB);
	wld.add(e, tagC);
	CHECK(info.cache_archetype_view().empty());

	wld.add(e, tagD);
	CHECK(info.cache_archetype_view().size() == 1);
	CHECK(q.count() == 1);
}

TEST_CASE("Query - cached Is query after inherited archetype creation") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto mammal = wld.add();
	wld.add(mammal, ecs::Pair(ecs::Is, animal));

	auto q = wld.query().all(ecs::Pair(ecs::Is, animal));
	auto& info = q.fetch();
	q.match_all(info);
	CHECK(info.cache_archetype_view().empty());
	CHECK(q.count() == 2);

	const auto wolf = wld.add();
	wld.add(wolf, ecs::Pair(ecs::Is, mammal));
	CHECK(q.count() == 3);
	CHECK(info.cache_archetype_view().empty());
}

TEST_CASE("Query - direct Is query with direct stored edges") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto mammal = wld.add();
	const auto wolf = wld.add();
	wld.add(mammal, ecs::Pair(ecs::Is, animal));
	wld.add(wolf, ecs::Pair(ecs::Is, mammal));

	auto q = wld.query().all(ecs::Pair(ecs::Is, animal), ecs::QueryTermOptions{}.direct());
	auto& info = q.fetch();
	q.match_all(info);
	CHECK(q.count() == 1);
	expect_exact_entities(q, {mammal});
	CHECK(info.cache_archetype_view().size() == 1);
}

TEST_CASE("Query - is sugar matches semantic and direct Is terms") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto mammal = wld.add();
	const auto wolf = wld.add();
	wld.add(mammal, ecs::Pair(ecs::Is, animal));
	wld.add(wolf, ecs::Pair(ecs::Is, mammal));

	auto qSemantic = wld.query().is(animal);
	CHECK(qSemantic.count() == 3);
	expect_exact_entities(qSemantic, {animal, mammal, wolf});

	auto qDirect = wld.query().is(animal, ecs::QueryTermOptions{}.direct());
	CHECK(qDirect.count() == 1);
	expect_exact_entities(qDirect, {mammal});
}

TEST_CASE("Query - in sugar matches descendants but excludes the base entity") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto mammal = wld.add();
	const auto wolf = wld.add();
	wld.add(mammal, ecs::Pair(ecs::Is, animal));
	wld.add(wolf, ecs::Pair(ecs::Is, mammal));

	auto q = wld.query().in(animal);
	CHECK(q.count() == 2);
	expect_exact_entities(q, {mammal, wolf});
}

TEST_CASE("Query - prefabs are excluded by default and can be matched explicitly") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	const auto prefabRabbit = wld.add();
	wld.build(prefabRabbit).prefab();
	const auto rabbit = wld.add();

	wld.as(prefabRabbit, prefabAnimal);
	wld.as(rabbit, prefabAnimal);

	wld.add<Position>(prefabAnimal, {1, 0, 0});
	wld.add<Position>(prefabRabbit, {2, 0, 0});
	wld.add<Position>(rabbit, {3, 0, 0});

	CHECK(wld.has_direct(prefabAnimal, ecs::Prefab));
	CHECK(wld.has_direct(prefabRabbit, ecs::Prefab));
	CHECK_FALSE(wld.has_direct(rabbit, ecs::Prefab));

	auto qDefault = wld.query().all<Position>().is(prefabAnimal);
	CHECK(qDefault.count() == 1);
	expect_exact_entities(qDefault, {rabbit});

	auto qMatchPrefab = wld.query().all<Position>().is(prefabAnimal).match_prefab();
	CHECK(qMatchPrefab.count() == 3);
	expect_exact_entities(qMatchPrefab, {prefabAnimal, prefabRabbit, rabbit});

	auto qPrefabOnly = wld.query().all(ecs::Prefab);
	CHECK(qPrefabOnly.count() == 2);
	expect_exact_entities(qPrefabOnly, {prefabAnimal, prefabRabbit});
}
