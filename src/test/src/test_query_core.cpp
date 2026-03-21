#include "test_common.h"

TEST_CASE("Query - QueryResult") {
	SUBCASE("Cached query") {
		Test_Query_QueryResult<ecs::Query>();
	}
	SUBCASE("Non-cached query") {
		Test_Query_QueryResult<ecs::QueryUncached>();
	}
	SUBCASE("Caching") {
		struct Player {};
		struct Health {
			uint32_t value;
		};

		TestWorld twld;

		const auto player = wld.add();
		wld.build(player).add<Player>().add<Health>();

		uint32_t matches = 0;
		auto qp = wld.query().all<Health>().all<Player>();
		qp.each([&matches]() {
			++matches;
		});
		CHECK(matches == 1);

		// Add new entity with some new component. Creates a new archetype.
		const auto something = wld.add();
		wld.add<Something>(something);

		// We still need to match the player entity once
		matches = 0;
		qp.each([&matches]() {
			++matches;
		});
		CHECK(matches == 1);

		// New the new item also has the health component
		wld.add<Health>(something);

		// We still need to match the player entity once
		matches = 0;
		qp.each([&matches]() {
			++matches;
		});
		CHECK(matches == 1);
	}
}

template <typename TQuery>
void Test_Query_SourceLookup() {
	constexpr bool UseCachedQuery = std::is_same_v<TQuery, ecs::Query>;

	TestWorld twld;

	struct Level {
		uint32_t value;
	};

	constexpr uint32_t N = 64;
	cnt::darr<ecs::Entity> positionEntities;
	positionEntities.reserve(N);
	GAIA_FOR(N) {
		auto e = wld.add();
		wld.add<Position>(e, {(float)i, (float)i, (float)i});
		positionEntities.push_back(e);
	}
	auto expect_positions = [&](auto& query, bool expectMatch) {
		cnt::darr<ecs::Entity> actual;
		query.each([&](ecs::Entity entity) {
			actual.push_back(entity);
		});

		if (!expectMatch) {
			CHECK(actual.empty());
			return;
		}

		CHECK(actual.size() == positionEntities.size());
		for (const auto exp: positionEntities) {
			bool found = false;
			for (const auto got: actual) {
				if (got != exp)
					continue;
				found = true;
				break;
			}
			CHECK(found);
		}
	};

	(void)wld.add<Level>();
	const auto game = wld.add();

	(void)wld.add<Level>(game, {1});
	auto qSrc = wld.query<UseCachedQuery>() //
									.template all<Position>()
									.template all<Level>(ecs::QueryTermOptions{}.src(game));
	CHECK(qSrc.count() == N);
	expect_positions(qSrc, true);

	wld.del<Level>(game);
	CHECK(qSrc.count() == 0);
	expect_positions(qSrc, false);

	wld.add<Level>(game, {2});
	CHECK(qSrc.count() == N);
	expect_positions(qSrc, true);

	const auto root = wld.add();
	const auto parent = wld.add();
	const auto scene = wld.add();
	wld.child(scene, parent);
	wld.child(parent, root);

	auto qSelfUp = wld.query<UseCachedQuery>() //
										 .template all<Position>()
										 .template all<Level>(ecs::QueryTermOptions{}.src(scene).trav());
	auto qUpOnly = wld.query<UseCachedQuery>() //
										 .template all<Position>()
										 .template all<Level>(ecs::QueryTermOptions{}.src(scene).trav_up());
	auto qUpDepth0 = wld.query<UseCachedQuery>() //
											 .template all<Position>()
											 .template all<Level>(ecs::QueryTermOptions{}.src(scene).trav_up().trav_depth(0));
	auto qParentOnly = wld.query<UseCachedQuery>() //
												 .template all<Position>()
												 .template all<Level>(ecs::QueryTermOptions{}.src(scene).trav_parent());
	auto qSelfParent = wld.query<UseCachedQuery>() //
												 .template all<Position>()
												 .template all<Level>(ecs::QueryTermOptions{}.src(scene).trav_self_parent());
	auto qSelfDepth1 = wld.query<UseCachedQuery>() //
												 .template all<Position>()
												 .template all<Level>(ecs::QueryTermOptions{}.src(scene).trav().trav_depth(1));
	auto qDownOnly = wld.query<UseCachedQuery>() //
											 .template all<Position>()
											 .template all<Level>(ecs::QueryTermOptions{}.src(root).trav_down());
	auto qDownDepth0 = wld.query<UseCachedQuery>() //
												 .template all<Position>()
												 .template all<Level>(ecs::QueryTermOptions{}.src(root).trav_down().trav_depth(0));
	auto qSelfDown = wld.query<UseCachedQuery>() //
											 .template all<Position>()
											 .template all<Level>(ecs::QueryTermOptions{}.src(root).trav_self_down());
	auto qChildOnly = wld.query<UseCachedQuery>() //
												.template all<Position>()
												.template all<Level>(ecs::QueryTermOptions{}.src(root).trav_child());
	auto qSelfChild = wld.query<UseCachedQuery>() //
												.template all<Position>()
												.template all<Level>(ecs::QueryTermOptions{}.src(root).trav_self_child());
	auto qSelfDownDepth1 = wld.query<UseCachedQuery>() //
														 .template all<Position>()
														 .template all<Level>(ecs::QueryTermOptions{}.src(root).trav_self_down().trav_depth(1));

	CHECK(qSelfUp.count() == 0);
	expect_positions(qSelfUp, false);
	CHECK(qUpOnly.count() == 0);
	expect_positions(qUpOnly, false);
	CHECK(qUpDepth0.count() == qUpOnly.count());
	CHECK(qParentOnly.count() == 0);
	expect_positions(qParentOnly, false);
	CHECK(qSelfParent.count() == 0);
	expect_positions(qSelfParent, false);
	CHECK(qSelfDepth1.count() == 0);
	expect_positions(qSelfDepth1, false);
	CHECK(qDownOnly.count() == 0);
	expect_positions(qDownOnly, false);
	CHECK(qDownDepth0.count() == qDownOnly.count());
	CHECK(qSelfDown.count() == 0);
	expect_positions(qSelfDown, false);
	CHECK(qChildOnly.count() == 0);
	expect_positions(qChildOnly, false);
	CHECK(qSelfChild.count() == 0);
	expect_positions(qSelfChild, false);
	CHECK(qSelfDownDepth1.count() == 0);
	expect_positions(qSelfDownDepth1, false);

	wld.add<Level>(scene, {3});
	CHECK(qSelfUp.count() == N);
	expect_positions(qSelfUp, true);
	CHECK(qUpOnly.count() == 0);
	expect_positions(qUpOnly, false);
	CHECK(qUpDepth0.count() == qUpOnly.count());
	CHECK(qParentOnly.count() == 0);
	expect_positions(qParentOnly, false);
	CHECK(qSelfParent.count() == N);
	expect_positions(qSelfParent, true);
	CHECK(qSelfDepth1.count() == N);
	expect_positions(qSelfDepth1, true);
	CHECK(qDownOnly.count() == N);
	expect_positions(qDownOnly, true);
	CHECK(qDownDepth0.count() == qDownOnly.count());
	CHECK(qSelfDown.count() == N);
	expect_positions(qSelfDown, true);
	CHECK(qChildOnly.count() == 0);
	expect_positions(qChildOnly, false);
	CHECK(qSelfChild.count() == 0);
	expect_positions(qSelfChild, false);
	CHECK(qSelfDownDepth1.count() == 0);
	expect_positions(qSelfDownDepth1, false);

	wld.del<Level>(scene);
	wld.add<Level>(parent, {4});
	CHECK(qSelfUp.count() == N);
	expect_positions(qSelfUp, true);
	CHECK(qUpOnly.count() == N);
	expect_positions(qUpOnly, true);
	CHECK(qUpDepth0.count() == qUpOnly.count());
	CHECK(qParentOnly.count() == N);
	expect_positions(qParentOnly, true);
	CHECK(qSelfParent.count() == N);
	expect_positions(qSelfParent, true);
	CHECK(qSelfDepth1.count() == N);
	expect_positions(qSelfDepth1, true);
	CHECK(qDownOnly.count() == N);
	expect_positions(qDownOnly, true);
	CHECK(qDownDepth0.count() == qDownOnly.count());
	CHECK(qSelfDown.count() == N);
	expect_positions(qSelfDown, true);
	CHECK(qChildOnly.count() == N);
	expect_positions(qChildOnly, true);
	CHECK(qSelfChild.count() == N);
	expect_positions(qSelfChild, true);
	CHECK(qSelfDownDepth1.count() == N);
	expect_positions(qSelfDownDepth1, true);

	wld.del<Level>(parent);
	wld.add<Level>(root, {5});
	CHECK(qSelfUp.count() == N);
	expect_positions(qSelfUp, true);
	CHECK(qUpOnly.count() == N);
	expect_positions(qUpOnly, true);
	CHECK(qUpDepth0.count() == qUpOnly.count());
	CHECK(qParentOnly.count() == 0);
	expect_positions(qParentOnly, false);
	CHECK(qSelfParent.count() == 0);
	expect_positions(qSelfParent, false);
	CHECK(qSelfDepth1.count() == 0);
	expect_positions(qSelfDepth1, false);
	CHECK(qDownOnly.count() == 0);
	expect_positions(qDownOnly, false);
	CHECK(qDownDepth0.count() == qDownOnly.count());
	CHECK(qSelfDown.count() == N);
	expect_positions(qSelfDown, true);
	CHECK(qChildOnly.count() == 0);
	expect_positions(qChildOnly, false);
	CHECK(qSelfChild.count() == N);
	expect_positions(qSelfChild, true);
	CHECK(qSelfDownDepth1.count() == N);
	expect_positions(qSelfDownDepth1, true);
}

TEST_CASE("Query - source lookup") {
	SUBCASE("Cached query") {
		Test_Query_SourceLookup<ecs::Query>();
	}
	SUBCASE("Non-cached query") {
		Test_Query_SourceLookup<ecs::QueryUncached>();
	}
}

template <typename TQuery>
void Test_Query_All_Any_Or_Semantics() {
	constexpr bool UseCachedQuery = std::is_same_v<TQuery, ecs::Query>;

	TestWorld twld;
	struct Cable {};
	struct Device {};
	struct Powered {};
	struct ConnectedTo {};

	const auto connectedTo = wld.add<ConnectedTo>().entity;

	const auto devA = wld.add();
	const auto devB = wld.add();
	wld.add<Device>(devA);

	const auto cablePlain = wld.add();
	wld.add<Cable>(cablePlain);

	const auto cableDevice = wld.add();
	wld.add<Cable>(cableDevice);
	wld.add<Device>(cableDevice);

	const auto cablePowered = wld.add();
	wld.add<Cable>(cablePowered);
	wld.add<Powered>(cablePowered);
	wld.add(cablePowered, {connectedTo, devB});

	const auto cableBoth = wld.add();
	wld.add<Cable>(cableBoth);
	wld.add<Device>(cableBoth);
	wld.add<Powered>(cableBoth);
	wld.add(cableBoth, {connectedTo, devA});

	// Not a cable - should never match queries that require Cable.
	const auto deviceOnly = wld.add();
	wld.add<Device>(deviceOnly);
	(void)deviceOnly;

	auto qAll = wld.query<UseCachedQuery>().template all<Cable>().template all<Device>();
	CHECK(qAll.count() == 2);
	expect_exact_entities(qAll, {cableDevice, cableBoth});

	// `any<Device>()` is optional: it does not filter out cables without Device.
	auto qAny = wld.query<UseCachedQuery>().template all<Cable>().template any<Device>();
	CHECK(qAny.count() == 4);
	expect_exact_entities(qAny, {cablePlain, cableDevice, cablePowered, cableBoth});

	// OR-chain: cable must satisfy at least one OR term.
	auto qOr = wld.query<UseCachedQuery>().template all<Cable>().template or_<Device>().template or_<Powered>();
	CHECK(qOr.count() == 3);
	expect_exact_entities(qOr, {cableDevice, cablePowered, cableBoth});

	auto qAnyExpr = wld.query<UseCachedQuery>().add("Cable, ?Device");
	CHECK(qAnyExpr.count() == 4);
	expect_exact_entities(qAnyExpr, {cablePlain, cableDevice, cablePowered, cableBoth});

	auto qOrExpr = wld.query<UseCachedQuery>().add("Cable, Device || Powered");
	CHECK(qOrExpr.count() == 3);
	expect_exact_entities(qOrExpr, {cableDevice, cablePowered, cableBoth});

	// Optional dependent term: if ConnectedTo exists, the target must be a Device.
	auto qOptionalDependent = wld.query<UseCachedQuery>().add("Cable, ?(ConnectedTo, $device), Device($device)");
	CHECK(qOptionalDependent.count() == 3);
	expect_exact_entities(qOptionalDependent, {cablePlain, cableDevice, cableBoth});
}

TEST_CASE("Query - all/any/or semantics") {
	SUBCASE("Cached query") {
		Test_Query_All_Any_Or_Semantics<ecs::Query>();
	}
	SUBCASE("Non-cached query") {
		Test_Query_All_Any_Or_Semantics<ecs::QueryUncached>();
	}
}

template <typename TQuery>
void Test_Query_Variables() {
	constexpr bool UseCachedQuery = std::is_same_v<TQuery, ecs::Query>;

	TestWorld twld;

	struct Cable {};
	struct Device {};
	struct ConnectedTo {};

	const auto connectedTo = wld.add<ConnectedTo>().entity;
	const auto deviceA = wld.add();
	const auto deviceB = wld.add();
	wld.add<Device>(deviceA);

	const auto cableA = wld.add();
	wld.add<Cable>(cableA);
	wld.add(cableA, {connectedTo, deviceA});

	const auto cableB = wld.add();
	wld.add<Cable>(cableB);
	wld.add(cableB, {connectedTo, deviceB});

	const auto cableC = wld.add();
	wld.add<Cable>(cableC);

	auto qApi = wld.query<UseCachedQuery>() //
									.template all<Cable>()
									.all(ecs::Pair(connectedTo, ecs::Var0))
									.template all<Device>(ecs::QueryTermOptions{}.src(ecs::Var0));
	CHECK(qApi.count() == 1);
	expect_exact_entities(qApi, {cableA});

	auto qExpr = wld.query<UseCachedQuery>().add("Cable, (ConnectedTo, $device), Device($device)");
	CHECK(qExpr.count() == 1);
	expect_exact_entities(qExpr, {cableA});
}

TEST_CASE("Query - variables") {
	SUBCASE("Cached query") {
		Test_Query_Variables<ecs::Query>();
	}
	SUBCASE("Non-cached query") {
		Test_Query_Variables<ecs::QueryUncached>();
	}
}

template <typename TQuery>
void Test_Query_SourceOr_VariableOr_Interaction() {
	constexpr bool UseCachedQuery = std::is_same_v<TQuery, ecs::Query>;

	struct Cable {};
	struct Device {};
	struct ConnectedTo {};

	// Unmatched source OR must not bypass variable OR terms.
	{
		TestWorld twld;
		const auto connectedTo = wld.add<ConnectedTo>().entity;

		const auto source = wld.add();
		const auto deviceA = wld.add();

		const auto cableA = wld.add();
		wld.add<Cable>(cableA);
		wld.add(cableA, {connectedTo, deviceA});

		const auto cableB = wld.add();
		wld.add<Cable>(cableB);

		auto q = wld.query<UseCachedQuery>()
								 .template all<Cable>()
								 .template or_<Device>(ecs::QueryTermOptions{}.src(source))
								 .or_(ecs::Pair(connectedTo, ecs::Var0));
		CHECK(q.count() == 1);
		expect_exact_entities(q, {cableA});
	}

	// Matched source OR can satisfy OR-group globally, so variable OR may be skipped.
	{
		TestWorld twld;
		const auto connectedTo = wld.add<ConnectedTo>().entity;

		const auto source = wld.add();
		wld.add<Device>(source);
		const auto deviceA = wld.add();

		const auto cableA = wld.add();
		wld.add<Cable>(cableA);
		wld.add(cableA, {connectedTo, deviceA});

		const auto cableB = wld.add();
		wld.add<Cable>(cableB);

		auto q = wld.query<UseCachedQuery>()
								 .template all<Cable>()
								 .template or_<Device>(ecs::QueryTermOptions{}.src(source))
								 .or_(ecs::Pair(connectedTo, ecs::Var0));
		CHECK(q.count() == 2);
		expect_exact_entities(q, {cableA, cableB});
	}
}

TEST_CASE("Query - source or and variable or interaction") {
	SUBCASE("Cached query") {
		Test_Query_SourceOr_VariableOr_Interaction<ecs::Query>();
	}
	SUBCASE("Non-cached query") {
		Test_Query_SourceOr_VariableOr_Interaction<ecs::QueryUncached>();
	}
}

template <typename TQuery>
void Test_Query_VariableOr_Backtracking_SkipBranch() {
	constexpr bool UseCachedQuery = std::is_same_v<TQuery, ecs::Query>;

	TestWorld twld;
	struct Cable {};
	struct Device {};
	struct ConnectedTo {};
	struct LinkedTo {};

	const auto connectedTo = wld.add<ConnectedTo>().entity;
	const auto linkedTo = wld.add<LinkedTo>().entity;

	const auto deviceA = wld.add();
	const auto deviceB = wld.add();
	wld.add<Device>(deviceB);

	const auto cable = wld.add();
	wld.add<Cable>(cable);
	wld.add(cable, {connectedTo, deviceA});
	wld.add(cable, {linkedTo, deviceB});

	// First OR term can bind $d to a non-device. The solver must be able to skip it
	// and backtrack into the second OR term.
	auto qApi = wld.query<UseCachedQuery>()
									.template all<Cable>()
									.template all<Device>(ecs::QueryTermOptions{}.src(ecs::Var0))
									.or_(ecs::Pair(connectedTo, ecs::Var0))
									.or_(ecs::Pair(linkedTo, ecs::Var0));
	CHECK(qApi.count() == 1);
	expect_exact_entities(qApi, {cable});

	auto qExpr = wld.query<UseCachedQuery>().add("Cable, Device($d), (ConnectedTo, $d) || (LinkedTo, $d)");
	CHECK(qExpr.count() == 1);
	expect_exact_entities(qExpr, {cable});
}

TEST_CASE("Query - variable or backtracking skip branch") {
	SUBCASE("Cached query") {
		Test_Query_VariableOr_Backtracking_SkipBranch<ecs::Query>();
	}
	SUBCASE("Non-cached query") {
		Test_Query_VariableOr_Backtracking_SkipBranch<ecs::QueryUncached>();
	}
}

template <typename TQuery>
void Test_Query_Variables_Advanced() {
	constexpr bool UseCachedQuery = std::is_same_v<TQuery, ecs::Query>;

	// Variable target reused by multiple terms + source lookup.
	{
		TestWorld twld;
		struct Cable {};
		struct ActiveDevice {};
		struct ConnectedTo {};
		struct PoweredBy {};

		const auto connectedTo = wld.add<ConnectedTo>().entity;
		const auto poweredBy = wld.add<PoweredBy>().entity;
		const auto deviceA = wld.add();
		const auto deviceB = wld.add();
		wld.add<ActiveDevice>(deviceA);

		const auto cableA = wld.add();
		wld.add<Cable>(cableA);
		wld.add(cableA, {connectedTo, deviceA});
		wld.add(cableA, {poweredBy, deviceA});

		const auto cableB = wld.add();
		wld.add<Cable>(cableB);
		wld.add(cableB, {connectedTo, deviceA});
		wld.add(cableB, {poweredBy, deviceB});

		auto qApi = wld.query<UseCachedQuery>() //
										.template all<Cable>()
										.all(ecs::Pair(connectedTo, ecs::Var0))
										.all(ecs::Pair(poweredBy, ecs::Var0))
										.template all<ActiveDevice>(ecs::QueryTermOptions{}.src(ecs::Var0));
		CHECK(qApi.count() == 1);
		expect_exact_entities(qApi, {cableA});

		auto qExpr = wld.query<UseCachedQuery>().add("Cable, (ConnectedTo, $d), (PoweredBy, $d), ActiveDevice($d)");
		CHECK(qExpr.count() == 1);
		expect_exact_entities(qExpr, {cableA});
	}

	// NOT term with variable source.
	{
		TestWorld twld;
		struct Cable {};
		struct ConnectedTo {};
		struct Blocked {};

		const auto connectedTo = wld.add<ConnectedTo>().entity;
		const auto deviceA = wld.add();
		const auto deviceB = wld.add();
		wld.add<Blocked>(deviceA);

		const auto cableA = wld.add();
		wld.add<Cable>(cableA);
		wld.add(cableA, {connectedTo, deviceA});

		const auto cableB = wld.add();
		wld.add<Cable>(cableB);
		wld.add(cableB, {connectedTo, deviceB});

		auto qApi = wld.query<UseCachedQuery>() //
										.template all<Cable>()
										.all(ecs::Pair(connectedTo, ecs::Var0))
										.template no<Blocked>(ecs::QueryTermOptions{}.src(ecs::Var0));
		CHECK(qApi.count() == 1);
		expect_exact_entities(qApi, {cableB});

		auto qExpr = wld.query<UseCachedQuery>().add("Cable, (ConnectedTo, $d), !Blocked($d)");
		CHECK(qExpr.count() == 1);
		expect_exact_entities(qExpr, {cableB});
	}

	// Variable in the relation side of a pair.
	{
		TestWorld twld;
		struct Cable {};
		struct ConnectedTo {};
		struct LinkedTo {};

		const auto connectedTo = wld.add<ConnectedTo>().entity;
		const auto linkedTo = wld.add<LinkedTo>().entity;
		const auto nodeA = wld.add();
		const auto nodeB = wld.add();

		const auto cableA = wld.add();
		wld.add<Cable>(cableA);
		wld.add(cableA, {connectedTo, nodeA});
		wld.add(cableA, {connectedTo, nodeB});

		const auto cableB = wld.add();
		wld.add<Cable>(cableB);
		wld.add(cableB, {connectedTo, nodeA});
		wld.add(cableB, {linkedTo, nodeB});

		auto qApi = wld.query<UseCachedQuery>()
										.template all<Cable>()
										.all(ecs::Pair(ecs::Var0, nodeA))
										.all(ecs::Pair(ecs::Var0, nodeB));
		CHECK(qApi.count() == 1);
		expect_exact_entities(qApi, {cableA});

		auto qExpr = wld.query<UseCachedQuery>().add("Cable, ($rel, %e), ($rel, %e)", nodeA.value(), nodeB.value());
		CHECK(qExpr.count() == 1);
		expect_exact_entities(qExpr, {cableA});
	}

	// Multiple independent variables in the same query.
	{
		TestWorld twld;
		struct Cable {};
		struct Device {};
		struct PowerNode {};
		struct ConnectedTo {};
		struct PoweredBy {};

		const auto connectedTo = wld.add<ConnectedTo>().entity;
		const auto poweredBy = wld.add<PoweredBy>().entity;
		const auto deviceA = wld.add();
		const auto deviceB = wld.add();
		wld.add<Device>(deviceA);
		wld.add<PowerNode>(deviceB);

		const auto cableAB = wld.add();
		wld.add<Cable>(cableAB);
		wld.add(cableAB, {connectedTo, deviceA});
		wld.add(cableAB, {poweredBy, deviceB});

		const auto cableAA = wld.add();
		wld.add<Cable>(cableAA);
		wld.add(cableAA, {connectedTo, deviceA});
		wld.add(cableAA, {poweredBy, deviceA});

		auto qApi = wld.query<UseCachedQuery>() //
										.template all<Cable>()
										.all(ecs::Pair(connectedTo, ecs::Var0))
										.all(ecs::Pair(poweredBy, ecs::Var1))
										.template all<Device>(ecs::QueryTermOptions{}.src(ecs::Var0))
										.template all<PowerNode>(ecs::QueryTermOptions{}.src(ecs::Var1));
		CHECK(qApi.count() == 1);
		expect_exact_entities(qApi, {cableAB});

		auto qExpr =
				wld.query<UseCachedQuery>().add("Cable, (ConnectedTo, $src), (PoweredBy, $dst), Device($src), PowerNode($dst)");
		CHECK(qExpr.count() == 1);
		expect_exact_entities(qExpr, {cableAB});
	}

	// Variable source lookup with traversal filters and depth limits.
	{
		TestWorld twld;
		struct Cable {};
		struct ConnectedTo {};
		struct Level {
			uint32_t value;
		};

		const auto connectedTo = wld.add<ConnectedTo>().entity;
		const auto root = wld.add();
		const auto planet = wld.add();
		const auto dock = wld.add();
		wld.child(planet, root);
		wld.child(dock, planet);

		const auto cableDock = wld.add();
		wld.add<Cable>(cableDock);
		wld.add(cableDock, {connectedTo, dock});

		const auto cablePlanet = wld.add();
		wld.add<Cable>(cablePlanet);
		wld.add(cablePlanet, {connectedTo, planet});

		const auto cableRoot = wld.add();
		wld.add<Cable>(cableRoot);
		wld.add(cableRoot, {connectedTo, root});

		auto qSelfUp = wld.query<UseCachedQuery>()
											 .template all<Cable>()
											 .all(ecs::Pair(connectedTo, ecs::Var0))
											 .template all<Level>(ecs::QueryTermOptions{}.src(ecs::Var0).trav());
		auto qUpOnly = wld.query<UseCachedQuery>()
											 .template all<Cable>()
											 .all(ecs::Pair(connectedTo, ecs::Var0))
											 .template all<Level>(ecs::QueryTermOptions{}.src(ecs::Var0).trav_up());
		auto qParentOnly = wld.query<UseCachedQuery>()
													 .template all<Cable>()
													 .all(ecs::Pair(connectedTo, ecs::Var0))
													 .template all<Level>(ecs::QueryTermOptions{}.src(ecs::Var0).trav_parent());
		auto qSelfDepth1 = wld.query<UseCachedQuery>()
													 .template all<Cable>()
													 .all(ecs::Pair(connectedTo, ecs::Var0))
													 .template all<Level>(ecs::QueryTermOptions{}.src(ecs::Var0).trav().trav_depth(1));

		CHECK(qSelfUp.count() == 0);
		CHECK(qUpOnly.count() == 0);
		CHECK(qParentOnly.count() == 0);
		CHECK(qSelfDepth1.count() == 0);
		expect_exact_entities(qSelfUp, {});
		expect_exact_entities(qUpOnly, {});
		expect_exact_entities(qParentOnly, {});
		expect_exact_entities(qSelfDepth1, {});

		wld.add<Level>(dock, {1});
		CHECK(qSelfUp.count() == 1);
		CHECK(qUpOnly.count() == 0);
		CHECK(qParentOnly.count() == 0);
		CHECK(qSelfDepth1.count() == 1);
		expect_exact_entities(qSelfUp, {cableDock});
		expect_exact_entities(qUpOnly, {});
		expect_exact_entities(qParentOnly, {});
		expect_exact_entities(qSelfDepth1, {cableDock});

		wld.del<Level>(dock);
		wld.add<Level>(planet, {2});
		CHECK(qSelfUp.count() == 2);
		CHECK(qUpOnly.count() == 1);
		CHECK(qParentOnly.count() == 1);
		CHECK(qSelfDepth1.count() == 2);
		expect_exact_entities(qSelfUp, {cableDock, cablePlanet});
		expect_exact_entities(qUpOnly, {cableDock});
		expect_exact_entities(qParentOnly, {cableDock});
		expect_exact_entities(qSelfDepth1, {cableDock, cablePlanet});

		wld.del<Level>(planet);
		wld.add<Level>(root, {3});
		CHECK(qSelfUp.count() == 3);
		CHECK(qUpOnly.count() == 2);
		CHECK(qParentOnly.count() == 1);
		CHECK(qSelfDepth1.count() == 2);
		expect_exact_entities(qSelfUp, {cableDock, cablePlanet, cableRoot});
		expect_exact_entities(qUpOnly, {cableDock, cablePlanet});
		expect_exact_entities(qParentOnly, {cablePlanet});
		expect_exact_entities(qSelfDepth1, {cablePlanet, cableRoot});
	}

	// `$this` explicitly targets the default source.
	{
		TestWorld twld;
		struct Cable {};
		struct Device {};

		const auto cableA = wld.add();
		wld.add<Cable>(cableA);
		wld.add<Device>(cableA);

		const auto cableB = wld.add();
		wld.add<Cable>(cableB);

		auto qImplicit = wld.query<UseCachedQuery>().add("Cable, Device");
		auto qExplicit = wld.query<UseCachedQuery>().add("Cable, Device($this)");

		CHECK(qImplicit.count() == 1);
		CHECK(qExplicit.count() == 1);
		expect_exact_entities(qImplicit, {cableA});
		expect_exact_entities(qExplicit, {cableA});
	}
}

TEST_CASE("Query - variables advanced") {
	SUBCASE("Cached query") {
		Test_Query_Variables_Advanced<ecs::Query>();
	}
	SUBCASE("Non-cached query") {
		Test_Query_Variables_Advanced<ecs::QueryUncached>();
	}
}

TEST_CASE("Query - variable source ignores stale recycled handles") {
	TestWorld twld;

	struct Ship {};
	struct PlanetTag {};

	const auto earth = wld.add();
	wld.add<PlanetTag>(earth);

	const auto ship = wld.add();
	wld.add<Ship>(ship);

	auto q = wld.query().all<Ship>().all<PlanetTag>(ecs::QueryTermOptions{}.src(ecs::Var0));

	q.set_var(ecs::Var0, earth);
	CHECK(q.count() == 1);
	expect_exact_entities(q, {ship});

	wld.del(earth);
	wld.update();

	ecs::Entity recycled = ecs::EntityBad;
	cnt::darr<ecs::Entity> extras;
	for (uint32_t i = 0; i < 64 && recycled == ecs::EntityBad; ++i) {
		const auto candidate = wld.add();
		if (candidate.id() == earth.id())
			recycled = candidate;
		else
			extras.push_back(candidate);
	}

	CHECK(recycled != ecs::EntityBad);
	CHECK(recycled.gen() != earth.gen());

	q.set_var(ecs::Var0, earth);
	CHECK(q.count() == 0);
	expect_exact_entities(q, {});

	wld.add<PlanetTag>(recycled);
	q.set_var(ecs::Var0, recycled);
	CHECK(q.count() == 1);
	expect_exact_entities(q, {ship});
}

TEST_CASE("Query - variable pair target ignores stale recycled handles") {
	TestWorld twld;

	struct Ship {};
	struct LocatedIn {};

	const auto locatedIn = wld.add<LocatedIn>().entity;
	const auto earth = wld.add();
	const auto ship = wld.add();
	wld.add<Ship>(ship);
	wld.add(ship, ecs::Pair(locatedIn, earth));

	auto q = wld.query().all<Ship>().all(ecs::Pair(locatedIn, ecs::Var0));

	q.set_var(ecs::Var0, earth);
	CHECK(q.count() == 1);
	expect_exact_entities(q, {ship});

	wld.del(earth);
	wld.update();

	ecs::Entity recycled = ecs::EntityBad;
	for (uint32_t i = 0; i < 64 && recycled == ecs::EntityBad; ++i) {
		const auto candidate = wld.add();
		if (candidate.id() == earth.id())
			recycled = candidate;
	}

	CHECK(recycled != ecs::EntityBad);
	CHECK(recycled.gen() != earth.gen());

	q.set_var(ecs::Var0, earth);
	CHECK(q.count() == 0);
	expect_exact_entities(q, {});

	wld.add(ship, ecs::Pair(locatedIn, recycled));
	q.set_var(ecs::Var0, recycled);
	CHECK(q.count() == 1);
	expect_exact_entities(q, {ship});
}

template <typename TQuery>
void Test_Query_Variables_MultiVar() {
	constexpr bool UseCachedQuery = std::is_same_v<TQuery, ecs::Query>;

	// 3 independent vars with type checks on all variable sources.
	{
		TestWorld twld;
		struct Cable {};
		struct Device {};
		struct PowerNode {};
		struct Planet {};
		struct ConnectedTo {};
		struct PoweredBy {};
		struct DockedTo {};

		const auto connectedTo = wld.add<ConnectedTo>().entity;
		const auto poweredBy = wld.add<PoweredBy>().entity;
		const auto dockedTo = wld.add<DockedTo>().entity;

		const auto devA = wld.add();
		const auto devB = wld.add();
		const auto powerA = wld.add();
		const auto powerB = wld.add();
		const auto planetA = wld.add();
		const auto asteroid = wld.add();

		wld.add<Device>(devA);
		wld.add<PowerNode>(powerA);
		wld.add<Planet>(planetA);

		const auto cableGood = wld.add();
		wld.add<Cable>(cableGood);
		wld.add(cableGood, {connectedTo, devA});
		wld.add(cableGood, {poweredBy, powerA});
		wld.add(cableGood, {dockedTo, planetA});

		const auto cableWrongPower = wld.add();
		wld.add<Cable>(cableWrongPower);
		wld.add(cableWrongPower, {connectedTo, devA});
		wld.add(cableWrongPower, {poweredBy, powerB});
		wld.add(cableWrongPower, {dockedTo, planetA});

		const auto cableWrongDevice = wld.add();
		wld.add<Cable>(cableWrongDevice);
		wld.add(cableWrongDevice, {connectedTo, devB});
		wld.add(cableWrongDevice, {poweredBy, powerA});
		wld.add(cableWrongDevice, {dockedTo, planetA});

		const auto cableWrongDock = wld.add();
		wld.add<Cable>(cableWrongDock);
		wld.add(cableWrongDock, {connectedTo, devA});
		wld.add(cableWrongDock, {poweredBy, powerA});
		wld.add(cableWrongDock, {dockedTo, asteroid});

		const auto cableMulti = wld.add();
		wld.add<Cable>(cableMulti);
		wld.add(cableMulti, {connectedTo, devA});
		wld.add(cableMulti, {connectedTo, devB});
		wld.add(cableMulti, {poweredBy, powerA});
		wld.add(cableMulti, {poweredBy, powerB});
		wld.add(cableMulti, {dockedTo, planetA});

		auto qApi = wld.query<UseCachedQuery>() //
										.template all<Cable>()
										.all(ecs::Pair(connectedTo, ecs::Var0))
										.all(ecs::Pair(poweredBy, ecs::Var1))
										.all(ecs::Pair(dockedTo, ecs::Var2))
										.template all<Device>(ecs::QueryTermOptions{}.src(ecs::Var0))
										.template all<PowerNode>(ecs::QueryTermOptions{}.src(ecs::Var1))
										.template all<Planet>(ecs::QueryTermOptions{}.src(ecs::Var2));
		CHECK(qApi.count() == 2);
		expect_exact_entities(qApi, {cableGood, cableMulti});

		auto qExpr = wld.query<UseCachedQuery>().add(
				"Cable, (ConnectedTo, $dev), (PoweredBy, $pwr), (DockedTo, $pl), Device($dev), PowerNode($pwr), Planet($pl)");
		CHECK(qExpr.count() == 2);
		expect_exact_entities(qExpr, {cableGood, cableMulti});

		// Same query semantics with shuffled term order.
		auto qExprShuffled = wld.query<UseCachedQuery>().add(
				"Cable, Device($dev), Planet($pl), PowerNode($pwr), (DockedTo, $pl), (PoweredBy, $pwr), (ConnectedTo, $dev)");
		CHECK(qExprShuffled.count() == 2);
		expect_exact_entities(qExprShuffled, {cableGood, cableMulti});
	}

	// Source-dependent variable binding: bind Var2 from a term sourced by Var0.
	{
		TestWorld twld;
		struct Relay {};
		struct PrimaryOnline {};
		struct SecondaryOnline {};
		struct RegionTag {};
		struct Primary {};
		struct Secondary {};
		struct LocatedIn {};

		const auto primary = wld.add<Primary>().entity;
		const auto secondary = wld.add<Secondary>().entity;
		const auto locatedIn = wld.add<LocatedIn>().entity;

		const auto devA = wld.add();
		const auto devB = wld.add();
		const auto backupA = wld.add();
		const auto backupB = wld.add();
		const auto regionA = wld.add();
		const auto regionB = wld.add();

		wld.add<PrimaryOnline>(devA);
		wld.add<PrimaryOnline>(devB);
		wld.add<SecondaryOnline>(backupA);
		wld.add<RegionTag>(regionA);

		wld.add(devA, {locatedIn, regionA});
		wld.add(devB, {locatedIn, regionB});

		const auto relayGood = wld.add();
		wld.add<Relay>(relayGood);
		wld.add(relayGood, {primary, devA});
		wld.add(relayGood, {secondary, backupA});

		const auto relayBadRegion = wld.add();
		wld.add<Relay>(relayBadRegion);
		wld.add(relayBadRegion, {primary, devB});
		wld.add(relayBadRegion, {secondary, backupA});

		const auto relayBadSecondary = wld.add();
		wld.add<Relay>(relayBadSecondary);
		wld.add(relayBadSecondary, {primary, devA});
		wld.add(relayBadSecondary, {secondary, backupB});

		auto qApi = wld.query<UseCachedQuery>() //
										.template all<Relay>()
										.all(ecs::Pair(primary, ecs::Var0))
										.all(ecs::Pair(secondary, ecs::Var1))
										.all(ecs::Pair(locatedIn, ecs::Var2), ecs::QueryTermOptions{}.src(ecs::Var0))
										.template all<PrimaryOnline>(ecs::QueryTermOptions{}.src(ecs::Var0))
										.template all<SecondaryOnline>(ecs::QueryTermOptions{}.src(ecs::Var1))
										.template all<RegionTag>(ecs::QueryTermOptions{}.src(ecs::Var2));
		CHECK(qApi.count() == 1);
		expect_exact_entities(qApi, {relayGood});
	}
}

TEST_CASE("Query - variables multivar") {
	SUBCASE("Cached query") {
		Test_Query_Variables_MultiVar<ecs::Query>();
	}
	SUBCASE("Non-cached query") {
		Test_Query_Variables_MultiVar<ecs::QueryUncached>();
	}
}

template <typename TQuery>
void Test_Query_RuntimeParams() {
	constexpr bool UseCachedQuery = std::is_same_v<TQuery, ecs::Query>;

	TestWorld twld;
	struct SpaceShip {};
	struct Planet {};
	struct DockedTo {};
	struct Military {};
	struct Civilian {};

	const auto dockedTo = wld.add<DockedTo>().entity;

	const auto earth = wld.add();
	const auto mars = wld.add();
	const auto moon = wld.add();
	wld.add<Planet>(earth);
	wld.add<Planet>(mars);

	const auto shipEarth = wld.add();
	wld.add<SpaceShip>(shipEarth);
	wld.add<Military>(shipEarth);
	wld.add(shipEarth, {dockedTo, earth});

	const auto shipMars = wld.add();
	wld.add<SpaceShip>(shipMars);
	wld.add(shipMars, {dockedTo, mars});

	const auto shipMoon = wld.add();
	wld.add<SpaceShip>(shipMoon);
	wld.add(shipMoon, {dockedTo, moon});

	const auto shipFree = wld.add();
	wld.add<SpaceShip>(shipFree);
	wld.add<Civilian>(shipFree);

	// Baseline expression parsing sanity checks.
	auto qSpaceShip = wld.query<UseCachedQuery>().add("SpaceShip");
	CHECK(qSpaceShip.count() == 4);
	expect_exact_entities(qSpaceShip, {shipEarth, shipMars, shipMoon, shipFree});

	auto qOrBaseline = wld.query<UseCachedQuery>().add("Military || Civilian");
	CHECK(qOrBaseline.count() == 2);
	expect_exact_entities(qOrBaseline, {shipEarth, shipFree});

	// Optional term + dependent term:
	// match all spaceships, and when DockedTo exists require that target to be a Planet.
	auto qOptional = wld.query<UseCachedQuery>().add("SpaceShip, ?(DockedTo, $planet), Planet($planet)");
	CHECK(qOptional.count() == 3);
	expect_exact_entities(qOptional, {shipEarth, shipMars, shipFree});

	// OR-chain syntax maps to query::or_.
	auto qOr = wld.query<UseCachedQuery>().add("SpaceShip, Military || Civilian");
	CHECK(qOr.count() == 2);
	expect_exact_entities(qOr, {shipEarth, shipFree});

	// Non-string OR API alias.
	auto qOrApi = wld.query<UseCachedQuery>().template all<SpaceShip>().template or_<Military>().template or_<Civilian>();
	CHECK(qOrApi.count() == 2);
	expect_exact_entities(qOrApi, {shipEarth, shipFree});

	auto qApi = wld.query<UseCachedQuery>() //
									.template all<SpaceShip>()
									.all(ecs::Pair(dockedTo, ecs::Var0))
									.template all<Planet>(ecs::QueryTermOptions{}.src(ecs::Var0))
									.var_name(ecs::Var0, "planet");

	CHECK(qApi.count() == 2);
	expect_exact_entities(qApi, {shipEarth, shipMars});

	qApi.set_var("planet", earth);
	CHECK(qApi.count() == 1);
	expect_exact_entities(qApi, {shipEarth});

	qApi.set_var(ecs::Var0, mars);
	CHECK(qApi.count() == 1);
	expect_exact_entities(qApi, {shipMars});

	qApi.clear_vars();
	CHECK(qApi.count() == 2);
	expect_exact_entities(qApi, {shipEarth, shipMars});

	if constexpr (UseCachedQuery) {
		auto qInterleavedA = wld.query()
														 .template all<SpaceShip>()
														 .all(ecs::Pair(dockedTo, ecs::Var0))
														 .template all<Planet>(ecs::QueryTermOptions{}.src(ecs::Var0));
		auto qInterleavedB = wld.query()
														 .template all<SpaceShip>()
														 .all(ecs::Pair(dockedTo, ecs::Var0))
														 .template all<Planet>(ecs::QueryTermOptions{}.src(ecs::Var0));

		qInterleavedA.set_var(ecs::Var0, earth);
		qInterleavedB.set_var(ecs::Var0, mars);
		CHECK(qInterleavedA.count() == 1);
		expect_exact_entities(qInterleavedA, {shipEarth});
		CHECK(qInterleavedB.count() == 1);
		expect_exact_entities(qInterleavedB, {shipMars});
		CHECK(qInterleavedA.count() == 1);
		expect_exact_entities(qInterleavedA, {shipEarth});
	}

	// Named var provided explicitly before parsing the expression.
	auto qExprNamed =
			wld.query<UseCachedQuery>().var_name(ecs::Var0, "planet").add("SpaceShip, (DockedTo, $planet), Planet($planet)");
	qExprNamed.set_var("planet", mars);
	CHECK(qExprNamed.count() == 1);
	expect_exact_entities(qExprNamed, {shipMars});

	// Expression-created variable names can be bound at runtime too.
	auto qExprAuto = wld.query<UseCachedQuery>().add("SpaceShip, (DockedTo, $p), Planet($p)");
	qExprAuto.set_var("p", earth);
	CHECK(qExprAuto.count() == 1);
	expect_exact_entities(qExprAuto, {shipEarth});
}

TEST_CASE("Query - runtime params") {
	SUBCASE("Cached query") {
		Test_Query_RuntimeParams<ecs::Query>();
	}
	SUBCASE("Non-cached query") {
		Test_Query_RuntimeParams<ecs::QueryUncached>();
	}
}

template <typename TQuery>
void Test_Query_Bytecode_Dump() {
	constexpr bool UseCachedQuery = std::is_same_v<TQuery, ecs::Query>;

	TestWorld twld;

	struct Level {
		uint32_t value;
	};
	struct ConnectedTo {};
	struct PlanetTag {};

	const auto level = wld.add<Level>().entity;
	const auto connectedTo = wld.add<ConnectedTo>().entity;

	const auto root = wld.add();
	const auto scene = wld.add();
	wld.child(scene, root);

	const auto entity = wld.add();
	wld.add<Position>(entity, {1.0f, 2.0f, 3.0f});
	wld.add(entity, {connectedTo, root});
	wld.add<PlanetTag>(root);

	auto q = wld.query<UseCachedQuery>() //
							 .template all<Position>()
							 .all(level, ecs::QueryTermOptions{}.src(scene).trav())
							 .all(ecs::Pair(connectedTo, ecs::Var0))
							 .template all<PlanetTag>(ecs::QueryTermOptions{}.src(ecs::Var0).trav_parent());

	const auto bytecode = q.bytecode();
	CHECK(!bytecode.empty());
	CHECK(bytecode.find("main_ops:") != BadIndex);
	CHECK(bytecode.find("ids_all: 1") != BadIndex);
	CHECK(bytecode.find("src_all: 1") != BadIndex);
	CHECK(bytecode.find("var_all: 2") != BadIndex);
	CHECK(bytecode.find("] up id=") != BadIndex);
	CHECK(bytecode.find("depth=1") != BadIndex);

	auto qDown = wld.query<UseCachedQuery>() //
									 .template all<Position>()
									 .all(level, ecs::QueryTermOptions{}.src(root).trav_self_down());
	const auto bytecodeDown = qDown.bytecode();
	CHECK(bytecodeDown.find("] down id=") != BadIndex);

	// Single OR term is canonicalized to AND.
	auto qOrOnlySingle = wld.query<UseCachedQuery>().template or_<PlanetTag>();
	const auto bytecodeOrOnlySingle = qOrOnlySingle.bytecode();
	CHECK(bytecodeOrOnlySingle.find("ids_all: 1") != BadIndex);
	CHECK(bytecodeOrOnlySingle.find("ids_or:") == BadIndex);

	auto qOrWithAllSingle = wld.query<UseCachedQuery>().template all<Position>().template or_<PlanetTag>();
	const auto bytecodeOrWithAllSingle = qOrWithAllSingle.bytecode();
	CHECK(bytecodeOrWithAllSingle.find("ids_all: 2") != BadIndex);
	CHECK(bytecodeOrWithAllSingle.find("ids_or:") == BadIndex);

	auto qOrOnlyMulti = wld.query<UseCachedQuery>().template or_<PlanetTag>().template or_<Position>();
	const auto bytecodeOrOnlyMulti = qOrOnlyMulti.bytecode();
	CHECK(bytecodeOrOnlyMulti.find("] or ") != BadIndex);

	auto qOrWithAllMulti =
			wld.query<UseCachedQuery>().template all<Position>().template or_<PlanetTag>().template or_<ConnectedTo>();
	const auto bytecodeOrWithAllMulti = qOrWithAllMulti.bytecode();
	CHECK(bytecodeOrWithAllMulti.find("] ora ") != BadIndex);

	// We do this just for code coverage.
	// Hide logging so it does not spam the results of unit testing.
	const auto logLevelBackup = util::g_logLevelMask;
	util::g_logLevelMask = 0;
	q.diag_bytecode();
	util::g_logLevelMask = logLevelBackup;
}

TEST_CASE("Query - bytecode dump") {
	SUBCASE("Cached query") {
		Test_Query_Bytecode_Dump<ecs::Query>();
	}
	SUBCASE("Non-cached query") {
		Test_Query_Bytecode_Dump<ecs::QueryUncached>();
	}
}

template <typename TQuery>
void Test_Query_Variable_Opcode_Paths() {
	constexpr bool UseCachedQuery = std::is_same_v<TQuery, ecs::Query>;

	struct Cable {};
	struct Device {};
	struct PowerNode {};
	struct ConnectedTo {};
	struct PoweredBy {};
	struct LinkedTo {};
	struct RoutedVia {};
	struct BlockedBy {};
	struct Marker {};

	// Single-variable ALL-only source-gated query should use the grouped ALL-only opcode.
	{
		TestWorld twld;
		const auto connectedTo = wld.add<ConnectedTo>().entity;
		const auto poweredBy = wld.add<PoweredBy>().entity;

		const auto devA = wld.add();
		const auto devB = wld.add();
		wld.add<Device>(devA);

		const auto cableGood = wld.add();
		wld.add<Cable>(cableGood);
		wld.add(cableGood, {connectedTo, devA});
		wld.add(cableGood, {poweredBy, devA});

		const auto cableBad = wld.add();
		wld.add<Cable>(cableBad);
		wld.add(cableBad, {connectedTo, devA});
		wld.add(cableBad, {poweredBy, devB});

		auto q = wld.query<UseCachedQuery>() //
								 .template all<Cable>()
								 .all(ecs::Pair(connectedTo, ecs::Var0))
								 .all(ecs::Pair(poweredBy, ecs::Var0))
								 .template all<Device>(ecs::QueryTermOptions{}.src(ecs::Var0));

		const auto bytecode = q.bytecode();
		CHECK(bytecode.find("] varf ") != BadIndex);
		CHECK(bytecode.find("var_exec: 1") != BadIndex);
		CHECK(bytecode.find("] search") != BadIndex);
		CHECK(bytecode.find("varp0:") != BadIndex);
		CHECK(bytecode.find("term_all_check") != BadIndex);
		CHECK(bytecode.find("term_all_bind") != BadIndex);
		CHECK(bytecode.find("search_begin_any") != BadIndex);
		CHECK(bytecode.find("search_maybe_finalize") != BadIndex);
		CHECK(q.count() == 1);
		expect_exact_entities(q, {cableGood});

		q.set_var(ecs::Var0, devA);
		CHECK(q.count() == 1);
		expect_exact_entities(q, {cableGood});

		q.set_var(ecs::Var0, devB);
		CHECK(q.count() == 0);
		expect_exact_entities(q, {});

		q.clear_vars();
		CHECK(q.count() == 1);
		expect_exact_entities(q, {cableGood});
	}

	// Single-variable ALL pair-intersection query should use the shared variable program opcode.
	{
		TestWorld twld;
		const auto connectedTo = wld.add<ConnectedTo>().entity;
		const auto linkedTo = wld.add<LinkedTo>().entity;
		const auto routedVia = wld.add<RoutedVia>().entity;

		const auto devA = wld.add();
		const auto devB = wld.add();
		const auto devC = wld.add();

		const auto cableGood = wld.add();
		wld.add<Cable>(cableGood);
		wld.add(cableGood, {connectedTo, devA});
		wld.add(cableGood, {linkedTo, devA});
		wld.add(cableGood, {routedVia, devA});

		const auto cableWrongA = wld.add();
		wld.add<Cable>(cableWrongA);
		wld.add(cableWrongA, {connectedTo, devA});
		wld.add(cableWrongA, {linkedTo, devB});
		wld.add(cableWrongA, {routedVia, devA});

		const auto cableWrongB = wld.add();
		wld.add<Cable>(cableWrongB);
		wld.add(cableWrongB, {connectedTo, devA});
		wld.add(cableWrongB, {linkedTo, devA});
		wld.add(cableWrongB, {routedVia, devC});

		auto q = wld.query<UseCachedQuery>() //
								 .template all<Cable>()
								 .all(ecs::Pair(connectedTo, ecs::Var0))
								 .all(ecs::Pair(linkedTo, ecs::Var0))
								 .all(ecs::Pair(routedVia, ecs::Var0));

		const auto bytecode = q.bytecode();
		CHECK(bytecode.find("] varf ") != BadIndex);
		CHECK(bytecode.find("var_exec: 1") != BadIndex);
		CHECK(bytecode.find("] search") != BadIndex);
		CHECK(bytecode.find("varp0:") != BadIndex);
		CHECK(bytecode.find("term_all_check") != BadIndex);
		CHECK(bytecode.find("term_all_bind") != BadIndex);
		CHECK(bytecode.find("search_begin_any") != BadIndex);
		CHECK(bytecode.find("search_maybe_finalize") != BadIndex);
		CHECK(q.count() == 1);
		expect_exact_entities(q, {cableGood});

		q.set_var(ecs::Var0, devA);
		CHECK(q.count() == 1);
		expect_exact_entities(q, {cableGood});

		q.set_var(ecs::Var0, devB);
		CHECK(q.count() == 0);
		expect_exact_entities(q, {});

		q.clear_vars();
		CHECK(q.count() == 1);
		expect_exact_entities(q, {cableGood});
	}

	// Two-variable ALL pair-intersection query should use the shared variable program opcode.
	{
		TestWorld twld;
		const auto connectedTo = wld.add<ConnectedTo>().entity;
		const auto poweredBy = wld.add<PoweredBy>().entity;
		const auto linkedTo = wld.add<LinkedTo>().entity;
		const auto routedVia = wld.add<RoutedVia>().entity;

		const auto devA = wld.add();
		const auto devB = wld.add();
		const auto pwrA = wld.add();
		const auto pwrB = wld.add();

		const auto cableGood = wld.add();
		wld.add<Cable>(cableGood);
		wld.add(cableGood, {connectedTo, devA});
		wld.add(cableGood, {linkedTo, devA});
		wld.add(cableGood, {poweredBy, pwrA});
		wld.add(cableGood, {routedVia, pwrA});

		const auto cableWrongVar0 = wld.add();
		wld.add<Cable>(cableWrongVar0);
		wld.add(cableWrongVar0, {connectedTo, devA});
		wld.add(cableWrongVar0, {linkedTo, devB});
		wld.add(cableWrongVar0, {poweredBy, pwrA});
		wld.add(cableWrongVar0, {routedVia, pwrA});

		const auto cableWrongVar1 = wld.add();
		wld.add<Cable>(cableWrongVar1);
		wld.add(cableWrongVar1, {connectedTo, devA});
		wld.add(cableWrongVar1, {linkedTo, devA});
		wld.add(cableWrongVar1, {poweredBy, pwrA});
		wld.add(cableWrongVar1, {routedVia, pwrB});

		auto q = wld.query<UseCachedQuery>() //
								 .template all<Cable>()
								 .all(ecs::Pair(connectedTo, ecs::Var0))
								 .all(ecs::Pair(poweredBy, ecs::Var1))
								 .all(ecs::Pair(linkedTo, ecs::Var0))
								 .all(ecs::Pair(routedVia, ecs::Var1));

		const auto bytecode = q.bytecode();
		CHECK(bytecode.find("] varf ") != BadIndex);
		CHECK(bytecode.find("var_exec: 1") != BadIndex);
		CHECK(bytecode.find("] search") != BadIndex);
		CHECK(bytecode.find("varp0:") != BadIndex);
		CHECK(bytecode.find("term_all_check") != BadIndex);
		CHECK(bytecode.find("term_all_bind") != BadIndex);
		CHECK(bytecode.find("search_begin_any") != BadIndex);
		CHECK(bytecode.find("search_maybe_finalize") != BadIndex);
		CHECK(q.count() == 1);
		expect_exact_entities(q, {cableGood});

		q.set_var(ecs::Var0, devA);
		q.set_var(ecs::Var1, pwrA);
		CHECK(q.count() == 1);
		expect_exact_entities(q, {cableGood});

		q.set_var(ecs::Var1, pwrB);
		CHECK(q.count() == 0);
		expect_exact_entities(q, {});

		q.clear_vars();
		CHECK(q.count() == 1);
		expect_exact_entities(q, {cableGood});
	}

	// Two-variable ALL-only source-gated groups should use the shared grouped program opcode.
	{
		TestWorld twld;
		const auto connectedTo = wld.add<ConnectedTo>().entity;
		const auto poweredBy = wld.add<PoweredBy>().entity;
		const auto linkedTo = wld.add<LinkedTo>().entity;

		const auto devA = wld.add();
		const auto devB = wld.add();
		const auto pwrA = wld.add();
		const auto pwrB = wld.add();
		wld.add<Device>(devA);
		wld.add<PowerNode>(pwrA);

		const auto cableGood = wld.add();
		wld.add<Cable>(cableGood);
		wld.add(cableGood, {connectedTo, devA});
		wld.add(cableGood, {linkedTo, devA});
		wld.add(cableGood, {poweredBy, pwrA});

		const auto cableWrongVar0 = wld.add();
		wld.add<Cable>(cableWrongVar0);
		wld.add(cableWrongVar0, {connectedTo, devA});
		wld.add(cableWrongVar0, {linkedTo, devB});
		wld.add(cableWrongVar0, {poweredBy, pwrA});

		const auto cableWrongVar1 = wld.add();
		wld.add<Cable>(cableWrongVar1);
		wld.add(cableWrongVar1, {connectedTo, devA});
		wld.add(cableWrongVar1, {linkedTo, devA});
		wld.add(cableWrongVar1, {poweredBy, pwrB});

		auto q = wld.query<UseCachedQuery>() //
								 .template all<Cable>()
								 .all(ecs::Pair(connectedTo, ecs::Var0))
								 .all(ecs::Pair(linkedTo, ecs::Var0))
								 .all(ecs::Pair(poweredBy, ecs::Var1))
								 .template all<Device>(ecs::QueryTermOptions{}.src(ecs::Var0))
								 .template all<PowerNode>(ecs::QueryTermOptions{}.src(ecs::Var1));

		const auto bytecode = q.bytecode();
		CHECK(bytecode.find("] varf ") != BadIndex);
		CHECK(bytecode.find("var_exec: 1") != BadIndex);
		CHECK(bytecode.find("] search") != BadIndex);
		CHECK(bytecode.find("varp0:") != BadIndex);
		CHECK(bytecode.find("term_all_check") != BadIndex);
		CHECK(bytecode.find("term_all_bind") != BadIndex);
		CHECK(bytecode.find("search_begin_any") != BadIndex);
		CHECK(bytecode.find("search_maybe_finalize") != BadIndex);
		CHECK(q.count() == 1);
		expect_exact_entities(q, {cableGood});

		q.set_var(ecs::Var0, devA);
		q.set_var(ecs::Var1, pwrA);
		CHECK(q.count() == 1);
		expect_exact_entities(q, {cableGood});

		q.set_var(ecs::Var1, pwrB);
		CHECK(q.count() == 0);
		expect_exact_entities(q, {});

		q.clear_vars();
		CHECK(q.count() == 1);
		expect_exact_entities(q, {cableGood});
	}

	// Coupled ALL-only multi-variable query should use the shared search program path.
	{
		TestWorld twld;
		const auto connectedTo = wld.add<ConnectedTo>().entity;
		const auto poweredBy = wld.add<PoweredBy>().entity;

		const auto devA = wld.add();
		const auto devB = wld.add();
		const auto pwrA = wld.add();
		const auto pwrB = wld.add();

		const auto cableGood = wld.add();
		wld.add<Cable>(cableGood);
		wld.add(cableGood, {connectedTo, devA});
		wld.add(cableGood, {poweredBy, pwrA});
		wld.add(cableGood, {devA, pwrA});

		const auto cableWrongCoupling = wld.add();
		wld.add<Cable>(cableWrongCoupling);
		wld.add(cableWrongCoupling, {connectedTo, devA});
		wld.add(cableWrongCoupling, {poweredBy, pwrA});
		wld.add(cableWrongCoupling, {devB, pwrA});

		const auto cableWrongTarget = wld.add();
		wld.add<Cable>(cableWrongTarget);
		wld.add(cableWrongTarget, {connectedTo, devA});
		wld.add(cableWrongTarget, {poweredBy, pwrB});
		wld.add(cableWrongTarget, {devA, pwrA});

		auto q = wld.query<UseCachedQuery>() //
								 .template all<Cable>()
								 .all(ecs::Pair(connectedTo, ecs::Var0))
								 .all(ecs::Pair(poweredBy, ecs::Var1))
								 .all(ecs::Pair(ecs::Var0, ecs::Var1));

		const auto bytecode = q.bytecode();
		CHECK(bytecode.find("] varf ") != BadIndex);
		CHECK(bytecode.find("var_exec: 1") != BadIndex);
		CHECK(bytecode.find("] search") != BadIndex);
		CHECK(bytecode.find("varp0:") != BadIndex);
		CHECK(bytecode.find("term_all_check") != BadIndex);
		CHECK(bytecode.find("term_all_bind") != BadIndex);
		CHECK(bytecode.find("search_begin_any") != BadIndex);
		CHECK(bytecode.find("search_maybe_finalize") != BadIndex);
		CHECK(q.count() == 1);
		expect_exact_entities(q, {cableGood});

		q.set_var(ecs::Var0, devA);
		q.set_var(ecs::Var1, pwrA);
		CHECK(q.count() == 1);
		expect_exact_entities(q, {cableGood});

		q.set_var(ecs::Var1, pwrB);
		CHECK(q.count() == 0);
		expect_exact_entities(q, {});

		q.clear_vars();
		CHECK(q.count() == 1);
		expect_exact_entities(q, {cableGood});
	}

	// Mixed multi-variable query with source traversal should use the shared search program path.
	{
		TestWorld twld;
		const auto connectedTo = wld.add<ConnectedTo>().entity;
		const auto linkedTo = wld.add<LinkedTo>().entity;
		const auto routedVia = wld.add<RoutedVia>().entity;

		const auto parentMarked = wld.add();
		const auto parentPlain = wld.add();
		wld.add<Marker>(parentMarked);

		const auto devMarked = wld.add();
		const auto devPlain = wld.add();
		wld.add(devMarked, ecs::Pair(ecs::ChildOf, parentMarked));
		wld.add(devPlain, ecs::Pair(ecs::ChildOf, parentPlain));

		const auto routeA = wld.add();
		const auto routeB = wld.add();

		const auto cableGood = wld.add();
		wld.add<Cable>(cableGood);
		wld.add(cableGood, {connectedTo, devMarked});
		wld.add(cableGood, {routedVia, routeA});
		wld.add(cableGood, {routeA, devMarked});

		const auto cableBadSource = wld.add();
		wld.add<Cable>(cableBadSource);
		wld.add(cableBadSource, {connectedTo, devPlain});
		wld.add(cableBadSource, {linkedTo, routeA});
		wld.add(cableBadSource, {routeA, devPlain});

		const auto cableBadCoupling = wld.add();
		wld.add<Cable>(cableBadCoupling);
		wld.add(cableBadCoupling, {connectedTo, devMarked});
		wld.add(cableBadCoupling, {linkedTo, routeB});
		wld.add(cableBadCoupling, {routeA, devMarked});

		auto q = wld.query<UseCachedQuery>() //
								 .template all<Cable>()
								 .all(ecs::Pair(connectedTo, ecs::Var0))
								 .all(ecs::Pair(ecs::Var1, ecs::Var0))
								 .template all<Marker>(ecs::QueryTermOptions{}.src(ecs::Var0).trav_parent())
								 .or_(ecs::Pair(routedVia, ecs::Var1))
								 .or_(ecs::Pair(linkedTo, ecs::Var1));

		const auto bytecode = q.bytecode();
		CHECK(bytecode.find("] varf ") != BadIndex);
		CHECK(bytecode.find("var_exec: 1") != BadIndex);
		CHECK(bytecode.find("] search") != BadIndex);
		CHECK(bytecode.find("varp0:") != BadIndex);
		CHECK(bytecode.find("term_all_check") != BadIndex);
		CHECK(bytecode.find("term_all_src_bind") != BadIndex);
		CHECK(bytecode.find("term_all_bind") != BadIndex);
		CHECK(bytecode.find("term_or_check") != BadIndex);
		CHECK(bytecode.find("term_or_bind") != BadIndex);
		CHECK(bytecode.find("search_begin_any") != BadIndex);
		CHECK(bytecode.find("search_all") != BadIndex);
		CHECK(bytecode.find("search_or") != BadIndex);
		CHECK(bytecode.find("search_other_or_bind") != BadIndex);
		CHECK(bytecode.find("search_maybe_finalize") != BadIndex);
		CHECK(q.count() == 1);
		expect_exact_entities(q, {cableGood});

		q.set_var(ecs::Var0, devMarked);
		q.set_var(ecs::Var1, routeA);
		CHECK(q.count() == 1);
		expect_exact_entities(q, {cableGood});

		q.set_var(ecs::Var0, devPlain);
		CHECK(q.count() == 0);
		expect_exact_entities(q, {});

		q.clear_vars();
		CHECK(q.count() == 1);
		expect_exact_entities(q, {cableGood});
	}

	// Single-variable ALL query with a down-only source anchor should use explicit down source binding.
	{
		TestWorld twld;
		const auto connectedTo = wld.add<ConnectedTo>().entity;

		const auto root = wld.add();
		const auto parent = wld.add();
		const auto leaf = wld.add();
		wld.child(parent, root);
		wld.child(leaf, parent);
		wld.add<Marker>(parent);

		const auto cableRoot = wld.add();
		wld.add<Cable>(cableRoot);
		wld.add(cableRoot, {connectedTo, root});

		const auto cableParent = wld.add();
		wld.add<Cable>(cableParent);
		wld.add(cableParent, {connectedTo, parent});

		const auto cableLeaf = wld.add();
		wld.add<Cable>(cableLeaf);
		wld.add(cableLeaf, {connectedTo, leaf});

		auto q = wld.query<UseCachedQuery>() //
								 .template all<Cable>()
								 .all(ecs::Pair(connectedTo, ecs::Var0))
								 .template all<Marker>(ecs::QueryTermOptions{}.src(ecs::Var0).trav_down());

		const auto bytecode = q.bytecode();
		CHECK(bytecode.find("] varf ") != BadIndex);
		CHECK(bytecode.find("var_exec: 1") != BadIndex);
		CHECK(bytecode.find("] search") != BadIndex);
		CHECK(bytecode.find("varp0:") != BadIndex);
		CHECK(bytecode.find("term_all_check") != BadIndex);
		CHECK(bytecode.find("term_all_src_bind") != BadIndex);
		CHECK(q.count() == 1);
		expect_exact_entities(q, {cableRoot});

		q.set_var(ecs::Var0, root);
		CHECK(q.count() == 1);
		expect_exact_entities(q, {cableRoot});

		q.set_var(ecs::Var0, parent);
		CHECK(q.count() == 0);
		expect_exact_entities(q, {});

		q.set_var(ecs::Var0, leaf);
		CHECK(q.count() == 0);
		expect_exact_entities(q, {});

		q.clear_vars();
		CHECK(q.count() == 1);
		expect_exact_entities(q, {cableRoot});
	}

	// Single-variable ALL query with an up+down source anchor should use explicit updown source binding.
	{
		TestWorld twld;
		const auto connectedTo = wld.add<ConnectedTo>().entity;

		const auto root = wld.add();
		const auto parent = wld.add();
		const auto leaf = wld.add();
		wld.child(parent, root);
		wld.child(leaf, parent);
		wld.add<Marker>(parent);

		const auto cableRoot = wld.add();
		wld.add<Cable>(cableRoot);
		wld.add(cableRoot, {connectedTo, root});

		const auto cableParent = wld.add();
		wld.add<Cable>(cableParent);
		wld.add(cableParent, {connectedTo, parent});

		const auto cableLeaf = wld.add();
		wld.add<Cable>(cableLeaf);
		wld.add(cableLeaf, {connectedTo, leaf});

		auto q = wld.query<UseCachedQuery>() //
								 .template all<Cable>()
								 .all(ecs::Pair(connectedTo, ecs::Var0))
								 .template all<Marker>(ecs::QueryTermOptions{}.src(ecs::Var0).trav_down().trav_kind(
										 ecs::QueryTravKind::Up | ecs::QueryTravKind::Down));

		const auto bytecode = q.bytecode();
		CHECK(bytecode.find("] varf ") != BadIndex);
		CHECK(bytecode.find("var_exec: 1") != BadIndex);
		CHECK(bytecode.find("] search") != BadIndex);
		CHECK(bytecode.find("varp0:") != BadIndex);
		CHECK(bytecode.find("term_all_check") != BadIndex);
		CHECK(bytecode.find("term_all_src_bind") != BadIndex);
		CHECK(q.count() == 2);
		expect_exact_entities(q, {cableRoot, cableLeaf});

		q.set_var(ecs::Var0, root);
		CHECK(q.count() == 1);
		expect_exact_entities(q, {cableRoot});

		q.set_var(ecs::Var0, parent);
		CHECK(q.count() == 0);
		expect_exact_entities(q, {});

		q.set_var(ecs::Var0, leaf);
		CHECK(q.count() == 1);
		expect_exact_entities(q, {cableLeaf});

		q.clear_vars();
		CHECK(q.count() == 2);
		expect_exact_entities(q, {cableRoot, cableLeaf});
	}

	// Single-variable OR query with a source-gated ALL anchor should use the shared variable program opcode.
	{
		TestWorld twld;
		const auto connectedTo = wld.add<ConnectedTo>().entity;
		const auto linkedTo = wld.add<LinkedTo>().entity;
		const auto routedVia = wld.add<RoutedVia>().entity;

		const auto devA = wld.add();
		const auto devB = wld.add();
		wld.add<Marker>(devA);

		const auto cableA = wld.add();
		wld.add<Cable>(cableA);
		wld.add(cableA, {connectedTo, devA});
		wld.add(cableA, {linkedTo, devB});
		wld.add(cableA, {routedVia, devB});

		const auto cableB = wld.add();
		wld.add<Cable>(cableB);
		wld.add(cableB, {connectedTo, devB});
		wld.add(cableB, {linkedTo, devA});
		wld.add(cableB, {routedVia, devB});

		auto q = wld.query<UseCachedQuery>() //
								 .template all<Cable>()
								 .template all<Marker>(ecs::QueryTermOptions{}.src(ecs::Var0))
								 .or_(ecs::Pair(connectedTo, ecs::Var0))
								 .or_(ecs::Pair(linkedTo, ecs::Var0))
								 .or_(ecs::Pair(routedVia, ecs::Var0));

		const auto bytecode = q.bytecode();
		CHECK(bytecode.find("] varf ") != BadIndex);
		CHECK(bytecode.find("var_exec: 1") != BadIndex);
		CHECK(bytecode.find("] search") != BadIndex);
		CHECK(bytecode.find("varp0:") != BadIndex);
		CHECK(bytecode.find("term_all_check") != BadIndex);
		CHECK(bytecode.find("term_all_src_bind") != BadIndex);
		CHECK(bytecode.find("term_all_bind") == BadIndex);
		CHECK(bytecode.find("term_or_check") != BadIndex);
		CHECK(bytecode.find("term_or_bind") != BadIndex);
		CHECK(bytecode.find("search_begin_any") != BadIndex);
		CHECK(bytecode.find("search_maybe_finalize") != BadIndex);
		CHECK(q.count() == 2);
		expect_exact_entities(q, {cableA, cableB});

		q.set_var(ecs::Var0, devA);
		CHECK(q.count() == 2);
		expect_exact_entities(q, {cableA, cableB});

		q.set_var(ecs::Var0, devB);
		CHECK(q.count() == 0);
		expect_exact_entities(q, {});

		q.clear_vars();
		CHECK(q.count() == 2);
		expect_exact_entities(q, {cableA, cableB});
	}

	// Single-variable OR query without a source-gated ALL anchor should stay on the shared single-variable program path.
	{
		TestWorld twld;
		const auto connectedTo = wld.add<ConnectedTo>().entity;
		const auto linkedTo = wld.add<LinkedTo>().entity;

		const auto devA = wld.add();
		const auto devB = wld.add();

		const auto cableA = wld.add();
		wld.add<Cable>(cableA);
		wld.add(cableA, {connectedTo, devA});

		const auto cableB = wld.add();
		wld.add<Cable>(cableB);
		wld.add(cableB, {linkedTo, devB});

		auto q = wld.query<UseCachedQuery>() //
								 .template all<Cable>()
								 .or_(ecs::Pair(connectedTo, ecs::Var0))
								 .or_(ecs::Pair(linkedTo, ecs::Var0));

		const auto bytecode = q.bytecode();
		CHECK(bytecode.find("] varf ") != BadIndex);
		CHECK(bytecode.find("var_exec: 1") != BadIndex);
		CHECK(bytecode.find("] search") != BadIndex);
		CHECK(bytecode.find("varp0:") != BadIndex);
		CHECK(bytecode.find("term_or_check") != BadIndex);
		CHECK(bytecode.find("term_or_bind") != BadIndex);
		CHECK(bytecode.find("search_or") != BadIndex);
		CHECK(bytecode.find("search_other_or_bind") != BadIndex);
		CHECK(bytecode.find("term_all_check") == BadIndex);
		CHECK(bytecode.find("term_all_bind") == BadIndex);
		CHECK(bytecode.find("final_require_or") != BadIndex);
		CHECK(bytecode.find("final_or_check") != BadIndex);
		CHECK(q.count() == 2);
		expect_exact_entities(q, {cableA, cableB});
	}

	// Single-variable pair mixed ALL/OR/NOT query should use the shared variable program opcode.
	{
		TestWorld twld;
		const auto connectedTo = wld.add<ConnectedTo>().entity;
		const auto linkedTo = wld.add<LinkedTo>().entity;
		const auto routedVia = wld.add<RoutedVia>().entity;
		const auto blockedBy = wld.add<BlockedBy>().entity;

		const auto devA = wld.add();
		const auto devB = wld.add();

		const auto cableGoodA = wld.add();
		wld.add<Cable>(cableGoodA);
		wld.add(cableGoodA, {connectedTo, devA});
		wld.add(cableGoodA, {linkedTo, devA});

		const auto cableGoodB = wld.add();
		wld.add<Cable>(cableGoodB);
		wld.add(cableGoodB, {connectedTo, devA});
		wld.add(cableGoodB, {routedVia, devA});

		const auto cableBadOr = wld.add();
		wld.add<Cable>(cableBadOr);
		wld.add(cableBadOr, {connectedTo, devA});
		wld.add(cableBadOr, {linkedTo, devB});
		wld.add(cableBadOr, {routedVia, devB});

		const auto cableBadNot = wld.add();
		wld.add<Cable>(cableBadNot);
		wld.add(cableBadNot, {connectedTo, devA});
		wld.add(cableBadNot, {linkedTo, devA});
		wld.add(cableBadNot, {blockedBy, devA});

		auto q = wld.query<UseCachedQuery>() //
								 .template all<Cable>()
								 .all(ecs::Pair(connectedTo, ecs::Var0))
								 .or_(ecs::Pair(linkedTo, ecs::Var0))
								 .or_(ecs::Pair(routedVia, ecs::Var0))
								 .no(ecs::Pair(blockedBy, ecs::Var0));

		const auto bytecode = q.bytecode();
		CHECK(bytecode.find("] varf ") != BadIndex);
		CHECK(bytecode.find("var_exec: 1") != BadIndex);
		CHECK(bytecode.find("] search") != BadIndex);
		CHECK(bytecode.find("varp0:") != BadIndex);
		CHECK(bytecode.find("term_all_check") != BadIndex);
		CHECK(bytecode.find("term_all_bind") != BadIndex);
		CHECK(bytecode.find("term_or_check") != BadIndex);
		CHECK(bytecode.find("term_or_bind") != BadIndex);
		CHECK(bytecode.find("final_not_check") != BadIndex);
		CHECK(bytecode.find("final_require_or") != BadIndex);
		CHECK(bytecode.find("final_or_check") != BadIndex);
		CHECK(bytecode.find("search_begin_any") != BadIndex);
		CHECK(bytecode.find("search_all") != BadIndex);
		CHECK(bytecode.find("search_or") != BadIndex);
		CHECK(bytecode.find("search_other_or_bind") != BadIndex);
		CHECK(bytecode.find("search_maybe_finalize") != BadIndex);
		CHECK(bytecode.find("final_success") != BadIndex);
		CHECK(q.count() == 2);
		expect_exact_entities(q, {cableGoodA, cableGoodB});

		q.set_var(ecs::Var0, devA);
		CHECK(q.count() == 2);
		expect_exact_entities(q, {cableGoodA, cableGoodB});

		q.set_var(ecs::Var0, devB);
		CHECK(q.count() == 0);
		expect_exact_entities(q, {});

		q.clear_vars();
		CHECK(q.count() == 2);
		expect_exact_entities(q, {cableGoodA, cableGoodB});
	}

	// Single-variable ANY query should emit split any-check / any-bind micro-ops.
	{
		TestWorld twld;
		const auto connectedTo = wld.add<ConnectedTo>().entity;
		const auto linkedTo = wld.add<LinkedTo>().entity;

		const auto devA = wld.add();
		const auto devB = wld.add();

		const auto cableA = wld.add();
		wld.add<Cable>(cableA);
		wld.add(cableA, {connectedTo, devA});

		const auto cableB = wld.add();
		wld.add<Cable>(cableB);
		wld.add(cableB, {linkedTo, devB});

		auto q = wld.query<UseCachedQuery>() //
								 .template all<Cable>()
								 .any(ecs::Pair(connectedTo, ecs::Var0))
								 .any(ecs::Pair(linkedTo, ecs::Var0));

		const auto bytecode = q.bytecode();
		CHECK(bytecode.find("] varf ") != BadIndex);
		CHECK(bytecode.find("var_exec: 1") != BadIndex);
		CHECK(bytecode.find("] search") != BadIndex);
		CHECK(bytecode.find("varp0:") != BadIndex);
		CHECK(bytecode.find("term_any_check") != BadIndex);
		CHECK(bytecode.find("term_any_bind") != BadIndex);
		CHECK(bytecode.find("term_all_check") == BadIndex);
		CHECK(bytecode.find("term_all_bind") == BadIndex);
		CHECK(q.count() == 2);
		expect_exact_entities(q, {cableA, cableB});
	}
}

TEST_CASE("Query - variable opcode paths") {
	SUBCASE("Cached query") {
		Test_Query_Variable_Opcode_Paths<ecs::Query>();
	}
	SUBCASE("Non-cached query") {
		Test_Query_Variable_Opcode_Paths<ecs::QueryUncached>();
	}
}

template <typename TQuery>
void Test_Query_Variable_Opcode_Selection_IsStructural() {
	constexpr bool UseCachedQuery = std::is_same_v<TQuery, ecs::Query>;

	struct Cable {};
	struct ConnectedTo {};
	struct LinkedTo {};
	struct RoutedVia {};
	struct Marker {};

	auto build_bytecode = [&](uint32_t extraMarkerCnt) {
		TestWorld twld;
		const auto connectedTo = wld.add<ConnectedTo>().entity;
		const auto linkedTo = wld.add<LinkedTo>().entity;
		const auto routedVia = wld.add<RoutedVia>().entity;

		const auto devA = wld.add();
		const auto devB = wld.add();
		wld.add<Marker>(devA);
		for (uint32_t i = 0; i < extraMarkerCnt; ++i) {
			const auto extra = wld.add();
			wld.add<Marker>(extra);
		}

		const auto cableA = wld.add();
		wld.add<Cable>(cableA);
		wld.add(cableA, {connectedTo, devA});
		wld.add(cableA, {linkedTo, devB});
		wld.add(cableA, {routedVia, devB});

		const auto cableB = wld.add();
		wld.add<Cable>(cableB);
		wld.add(cableB, {connectedTo, devB});
		wld.add(cableB, {linkedTo, devA});
		wld.add(cableB, {routedVia, devB});

		auto q = wld.query<UseCachedQuery>() //
								 .template all<Cable>()
								 .template all<Marker>(ecs::QueryTermOptions{}.src(ecs::Var0))
								 .or_(ecs::Pair(connectedTo, ecs::Var0))
								 .or_(ecs::Pair(linkedTo, ecs::Var0))
								 .or_(ecs::Pair(routedVia, ecs::Var0));

		auto bytecode = q.bytecode();
		CHECK(bytecode.find("] varf ") != BadIndex);
		CHECK(bytecode.find("] search") != BadIndex);
		CHECK(bytecode.find("term_all_check") != BadIndex);
		CHECK(bytecode.find("term_all_src_bind") != BadIndex);
		CHECK(bytecode.find("term_all_bind") == BadIndex);
		CHECK(bytecode.find("term_or_check") != BadIndex);
		CHECK(bytecode.find("term_or_bind") != BadIndex);
		CHECK(q.count() == 2);
		return bytecode;
	};

	const auto bytecodeFew = build_bytecode(0);
	const auto bytecodeMany = build_bytecode(1024);

	CHECK(bytecodeFew == bytecodeMany);
}

TEST_CASE("Query - variable opcode selection is structural") {
	SUBCASE("Cached query") {
		Test_Query_Variable_Opcode_Selection_IsStructural<ecs::Query>();
	}
	SUBCASE("Non-cached query") {
		Test_Query_Variable_Opcode_Selection_IsStructural<ecs::QueryUncached>();
	}
}

template <typename TQuery>
void Test_Query_Variable_Program_Recompile() {
	constexpr bool UseCachedQuery = std::is_same_v<TQuery, ecs::Query>;

	struct Cable {};
	struct ConnectedTo {};
	struct LinkedTo {};
	struct RoutedVia {};
	struct Marker {};

	TestWorld twld;
	const auto connectedTo = wld.add<ConnectedTo>().entity;
	const auto linkedTo = wld.add<LinkedTo>().entity;
	const auto routedVia = wld.add<RoutedVia>().entity;

	const auto parentMarked = wld.add();
	const auto parentPlain = wld.add();
	wld.add<Marker>(parentMarked);

	const auto devMarked = wld.add();
	const auto devPlain = wld.add();
	wld.add(devMarked, ecs::Pair(ecs::ChildOf, parentMarked));
	wld.add(devPlain, ecs::Pair(ecs::ChildOf, parentPlain));

	const auto routeA = wld.add();
	const auto routeB = wld.add();

	const auto cableGood = wld.add();
	wld.add<Cable>(cableGood);
	wld.add(cableGood, {connectedTo, devMarked});
	wld.add(cableGood, {routedVia, routeA});
	wld.add(cableGood, {routeA, devMarked});

	const auto cableBadSource = wld.add();
	wld.add<Cable>(cableBadSource);
	wld.add(cableBadSource, {connectedTo, devPlain});
	wld.add(cableBadSource, {linkedTo, routeA});
	wld.add(cableBadSource, {routeA, devPlain});

	const auto cableBadCoupling = wld.add();
	wld.add<Cable>(cableBadCoupling);
	wld.add(cableBadCoupling, {connectedTo, devMarked});
	wld.add(cableBadCoupling, {linkedTo, routeB});
	wld.add(cableBadCoupling, {routeA, devMarked});

	auto q = wld.query<UseCachedQuery>() //
							 .template all<Cable>()
							 .all(ecs::Pair(connectedTo, ecs::Var0))
							 .all(ecs::Pair(ecs::Var1, ecs::Var0))
							 .template all<Marker>(ecs::QueryTermOptions{}.src(ecs::Var0).trav_parent())
							 .or_(ecs::Pair(routedVia, ecs::Var1))
							 .or_(ecs::Pair(linkedTo, ecs::Var1));

	const auto bytecodeBefore = q.bytecode();
	CHECK(bytecodeBefore.find("var_exec: 1") != BadIndex);
	CHECK(bytecodeBefore.find("term_all_src_bind") != BadIndex);
	CHECK(bytecodeBefore.find("search_begin_any") != BadIndex);
	CHECK(bytecodeBefore.find("search_other_or_bind") != BadIndex);
	CHECK(bytecodeBefore.find("search_maybe_finalize") != BadIndex);
	CHECK(q.count() == 1);
	expect_exact_entities(q, {cableGood});

	q.fetch().ctx().data.flags |= ecs::QueryCtx::QueryFlags::Recompile;
	CHECK(q.count() == 1);
	expect_exact_entities(q, {cableGood});
	const auto bytecodeAfter = q.bytecode();
	CHECK(bytecodeAfter == bytecodeBefore);

	q.set_var(ecs::Var0, devMarked);
	q.set_var(ecs::Var1, routeA);
	q.fetch().ctx().data.flags |= ecs::QueryCtx::QueryFlags::Recompile;
	CHECK(q.count() == 1);
	expect_exact_entities(q, {cableGood});

	q.set_var(ecs::Var0, devPlain);
	q.fetch().ctx().data.flags |= ecs::QueryCtx::QueryFlags::Recompile;
	CHECK(q.count() == 0);
	expect_exact_entities(q, {});
}

TEST_CASE("Query - variable program recompile") {
	SUBCASE("Cached query") {
		Test_Query_Variable_Program_Recompile<ecs::Query>();
	}
	SUBCASE("Non-cached query") {
		Test_Query_Variable_Program_Recompile<ecs::QueryUncached>();
	}
}

template <typename TQuery>
void Test_Query_SingleOr_CanonicalizedToAll() {
	constexpr bool UseCachedQuery = std::is_same_v<TQuery, ecs::Query>;

	auto collect_sorted = [](auto& query) {
		cnt::darr<ecs::Entity> out;
		query.each([&](ecs::Entity entity) {
			out.push_back(entity);
		});
		core::sort(out, [](ecs::Entity left, ecs::Entity right) {
			return left.id() < right.id();
		});
		return out;
	};

	TestWorld twld;
	struct A {};
	struct B {};

	const auto source = wld.add();
	wld.add<A>(source);

	const auto eA = wld.add();
	wld.add<A>(eA);

	const auto eB = wld.add();
	wld.add<B>(eB);
	(void)eB;

	// Single OR has required semantics and matches ALL.
	auto qOr = wld.query<UseCachedQuery>().template or_<A>();
	auto qAll = wld.query<UseCachedQuery>().template all<A>();
	CHECK(qOr.count() == qAll.count());
	const auto qOrEntities = collect_sorted(qOr);
	const auto qAllEntities = collect_sorted(qAll);
	CHECK(qOrEntities.size() == qAllEntities.size());
	GAIA_FOR((uint32_t)qOrEntities.size()) {
		CHECK(qOrEntities[i] == qAllEntities[i]);
	}

	// Source-based single OR is canonicalized in the same way.
	auto qOrSrc = wld.query<UseCachedQuery>().template or_<A>(ecs::QueryTermOptions{}.src(source));
	auto qAllSrc = wld.query<UseCachedQuery>().template all<A>(ecs::QueryTermOptions{}.src(source));
	CHECK(qOrSrc.count() == qAllSrc.count());
	const auto qOrSrcEntities = collect_sorted(qOrSrc);
	const auto qAllSrcEntities = collect_sorted(qAllSrc);
	CHECK(qOrSrcEntities.size() == qAllSrcEntities.size());
	GAIA_FOR((uint32_t)qOrSrcEntities.size()) {
		CHECK(qOrSrcEntities[i] == qAllSrcEntities[i]);
	}
}

TEST_CASE("Query - single or canonicalized to all") {
	SUBCASE("Cached query") {
		Test_Query_SingleOr_CanonicalizedToAll<ecs::Query>();
	}
	SUBCASE("Non-cached query") {
		Test_Query_SingleOr_CanonicalizedToAll<ecs::QueryUncached>();
	}
}

template <typename TQuery>
void Test_Query_String_Optional_Regression() {
	constexpr bool UseCachedQuery = std::is_same_v<TQuery, ecs::Query>;

	TestWorld twld;
	struct Fuel {};
	struct Velocity {};
	struct RigidBody {};

	const auto fuel = wld.add<Fuel>().entity;
	const auto player = wld.add();
	const auto other = wld.add();
	const auto noBody = wld.add();

	wld.add<Position>(player, {1.0f, 2.0f, 3.0f});
	wld.add<RigidBody>(player);
	wld.add(player, {fuel, player});

	wld.add<Position>(other, {4.0f, 5.0f, 6.0f});
	wld.add<RigidBody>(other);
	wld.add<Velocity>(other);
	wld.add(other, {fuel, player});

	wld.add<Position>(noBody, {7.0f, 8.0f, 9.0f});
	wld.add(noBody, {fuel, player});

	auto qExpr = wld.query<UseCachedQuery>().add("&Position, !Velocity, ?RigidBody, (Fuel,*)");
	auto qApi =
			wld.query<UseCachedQuery>().template all<Position&>().template no<Velocity>().template any<RigidBody>().all(
					ecs::Pair(fuel, ecs::All));

	CHECK(qExpr.count() == 2);
	CHECK(qApi.count() == 2);
	expect_exact_entities(qExpr, {player, noBody});
	expect_exact_entities(qApi, {player, noBody});

	// "?RigidBody" in string syntax must remain optional.
	const auto bytecode = qExpr.bytecode();
	CHECK(bytecode.find("ids_or:") == BadIndex);
	CHECK(bytecode.find("ids_all: 2") != BadIndex);
	CHECK(bytecode.find("ids_not: 1") != BadIndex);
}

TEST_CASE("Query - string optional regression") {
	SUBCASE("Cached query") {
		Test_Query_String_Optional_Regression<ecs::Query>();
	}
	SUBCASE("Non-cached query") {
		Test_Query_String_Optional_Regression<ecs::QueryUncached>();
	}
}

template <typename TQuery>
void Test_Query_Or_Dedup() {
	constexpr bool UseCachedQuery = std::is_same_v<TQuery, ecs::Query>;

	TestWorld twld;
	struct Marker {};
	struct A {};
	struct B {};

	const auto eBoth = wld.add();
	wld.add<Marker>(eBoth);
	wld.add<A>(eBoth);
	wld.add<B>(eBoth);

	auto qOrOnly = wld.query<UseCachedQuery>().template or_<A>().template or_<B>(); //
	CHECK(qOrOnly.count() == 1);
	expect_exact_entities(qOrOnly, {eBoth});

	auto qAllOr = wld.query<UseCachedQuery>().template all<Marker>().template or_<A>().template or_<B>();
	CHECK(qAllOr.count() == 1);
	expect_exact_entities(qAllOr, {eBoth});
}

TEST_CASE("Query - or dedup") {
	SUBCASE("Cached query") {
		Test_Query_Or_Dedup<ecs::Query>();
	}
	SUBCASE("Non-cached query") {
		Test_Query_Or_Dedup<ecs::QueryUncached>();
	}
}

template <typename TQuery>
void Test_Query_QueryResult_Complex() {
	const uint32_t N = 1'500;

	TestWorld twld;
	struct Data {
		Position p;
		Scale s;
	};
	cnt::map<ecs::Entity, Data> cmp;
	cnt::darr<ecs::Entity> ents;
	ents.reserve(N);

	auto create = [&](uint32_t i) {
		auto e = wld.add();

		auto b = wld.build(e);
		b.add<Position>().add<Scale>();
		if (i % 2 == 0)
			b.add<Something>();
		b.commit();

		auto p1 = Position{(float)i, (float)i, (float)i};
		auto s1 = Scale{(float)i * 2, (float)i * 2, (float)i * 2};

		auto s = wld.acc_mut(e);
		s.set<Position>(p1).set<Scale>(s1);
		if (i % 2 == 0)
			s.set<Something>({true});

		auto p0 = wld.get<Position>(e);
		CHECK(memcmp((void*)&p0, (void*)&p1, sizeof(p0)) == 0);
		cmp.try_emplace(e, Data{p1, s1});
	};

	GAIA_FOR(N) create(i);

	constexpr bool UseCachedQuery = std::is_same_v<TQuery, ecs::Query>;
	auto q1 = wld.query<UseCachedQuery>().template all<Position>();
	auto q2 = wld.query<UseCachedQuery>().template all<Rotation>();
	auto q3 = wld.query<UseCachedQuery>().template all<Position>().template all<Rotation>();
	auto q4 = wld.query<UseCachedQuery>().template all<Position>().template all<Scale>();
	auto q5 = wld.query<UseCachedQuery>().template all<Position>().template all<Scale>().template all<Something>();

	{
		ents.clear();
		q1.arr(ents);
		CHECK(ents.size() == N);
	}
	{
		cnt::darr<Position> arr;
		q1.arr(arr);
		CHECK(arr.size() == N);
		GAIA_EACH(arr) {
			const auto e = ents[i];
			const auto& v0 = arr[i];
			const auto& v1 = wld.get<Position>(e);
			CHECK(memcmp((const void*)&v0, (const void*)&v1, sizeof(v0)) == 0);
			CHECK(memcmp((const void*)&v0, (const void*)&cmp[e].p, sizeof(v0)) == 0);
		}
	}
	{
		const auto cnt = q1.count();
		CHECK(cnt > 0);

		const auto empty = q1.empty();
		CHECK(empty == false);

		uint32_t cnt2 = 0;
		q1.each([&]() {
			++cnt2;
		});
		CHECK(cnt2 == cnt);
	}

	{
		const auto cnt = q2.count();
		CHECK(cnt == 0);

		const auto empty = q2.empty();
		CHECK(empty == true);

		uint32_t cnt2 = 0;
		q2.each([&]() {
			++cnt2;
		});
		CHECK(cnt2 == cnt);
	}

	{
		const auto cnt = q3.count();
		CHECK(cnt == 0);

		const auto empty = q3.empty();
		CHECK(empty == true);

		uint32_t cnt3 = 0;
		q3.each([&]() {
			++cnt3;
		});
		CHECK(cnt3 == cnt);
	}

	{
		ents.clear();
		q4.arr(ents);
		CHECK(ents.size() == N);
	}
	{
		cnt::darr<Position> arr;
		q4.arr(arr);
		CHECK(arr.size() == N);
		GAIA_EACH(arr) {
			const auto e = ents[i];
			const auto& v0 = arr[i];
			const auto& v1 = wld.get<Position>(e);
			CHECK(memcmp((const void*)&v0, (const void*)&v1, sizeof(v0)) == 0);
			CHECK(memcmp((const void*)&v0, (const void*)&cmp[e].p, sizeof(v0)) == 0);
		}
	}
	{
		cnt::darr<Scale> arr;
		q4.arr(arr);
		CHECK(arr.size() == N);
		GAIA_EACH(arr) {
			const auto e = ents[i];
			const auto& v0 = arr[i];
			const auto& v1 = wld.get<Scale>(e);
			CHECK(memcmp((const void*)&v0, (const void*)&v1, sizeof(v0)) == 0);
			CHECK(memcmp((const void*)&v0, (const void*)&cmp[e].s, sizeof(v0)) == 0);
		}
	}
	{
		const auto cnt = q4.count();
		CHECK(cnt > 0);

		const auto empty = q4.empty();
		CHECK(empty == false);

		uint32_t cnt4 = 0;
		q4.each([&]() {
			++cnt4;
		});
		CHECK(cnt4 == cnt);
	}

	{
		ents.clear();
		q5.arr(ents);
		CHECK(ents.size() == N / 2);
	}
	{
		cnt::darr<Position> arr;
		q5.arr(arr);
		CHECK(arr.size() == ents.size());
		CHECK(arr.size() == N / 2);

		GAIA_EACH(arr) {
			const auto e = ents[i];
			const auto& v0 = arr[i];
			const auto& v1 = wld.get<Position>(e);
			CHECK(memcmp((const void*)&v0, (const void*)&v1, sizeof(v0)) == 0);
			CHECK(memcmp((const void*)&v0, (const void*)&cmp[e].p, sizeof(v0)) == 0);
		}
	}
	{
		cnt::darr<Scale> arr;
		q5.arr(arr);
		CHECK(arr.size() == N / 2);
		GAIA_EACH(arr) {
			const auto e = ents[i];
			const auto& v0 = arr[i];
			const auto& v1 = wld.get<Scale>(e);
			CHECK(memcmp((const void*)&v0, (const void*)&v1, sizeof(v0)) == 0);
			CHECK(memcmp((const void*)&v0, (const void*)&cmp[e].s, sizeof(v0)) == 0);
		}
	}
	{
		const auto cnt = q5.count();
		CHECK(cnt > 0);

		const auto empty = q5.empty();
		CHECK(empty == false);

		uint32_t cnt5 = 0;
		q5.each([&]() {
			++cnt5;
		});
		CHECK(cnt5 == cnt);
	}
}

TEST_CASE("Query - QueryResult complex") {
	SUBCASE("Cached query") {
		Test_Query_QueryResult_Complex<ecs::Query>();
	}
	SUBCASE("Non-cached query") {
		Test_Query_QueryResult_Complex<ecs::QueryUncached>();
	}
}

TEST_CASE("Relationship") {
	SUBCASE("Simple") {
		TestWorld twld;
		auto wolf = wld.add();
		auto rabbit = wld.add();
		auto carrot = wld.add();
		auto eats = wld.add();

		wld.add(rabbit, {eats, carrot});
		wld.add(wolf, {eats, rabbit});

		CHECK(wld.has(rabbit, {eats, carrot}));
		CHECK(wld.has(wolf, {eats, rabbit}));
		CHECK_FALSE(wld.has(wolf, {eats, carrot}));
		CHECK_FALSE(wld.has(rabbit, {eats, wolf}));

		{
			auto q = wld.query().add({ecs::QueryOpKind::All, ecs::QueryAccess::None, ecs::Pair(eats, carrot)});
			// auto q = wld.query().all<ecs::Pair(eats, carrot)>();
			const auto cnt = q.count();
			CHECK(cnt == 1);

			uint32_t i = 0;
			q.each([&](ecs::Entity entity) {
				CHECK(entity == rabbit);
				++i;
			});
			CHECK(i == cnt);
		}
		{
			auto q = wld.query().add("(%e, %e)", eats.value(), carrot.value());
			const auto cnt = q.count();
			CHECK(cnt == 1);

			uint32_t i = 0;
			q.each([&](ecs::Entity entity) {
				CHECK(entity == rabbit);
				++i;
			});
			CHECK(i == cnt);
		}

		{
			auto q = wld.query().add({ecs::QueryOpKind::All, ecs::QueryAccess::None, ecs::Pair(eats, rabbit)});
			// auto q = wld.query().all<ecs::Pair(eats, rabbit)>();
			const auto cnt = q.count();
			CHECK(cnt == 1);

			uint32_t i = 0;
			q.each([&](ecs::Entity entity) {
				CHECK(entity == wolf);
				++i;
			});
			CHECK(i == cnt);
		}
	}

	SUBCASE("Simple - bulk") {
		TestWorld twld;
		auto wolf = wld.add();
		auto rabbit = wld.add();
		auto carrot = wld.add();
		auto eats = wld.add();

		wld.build(rabbit).add({eats, carrot});
		wld.build(wolf).add({eats, rabbit});

		CHECK(wld.has(rabbit, {eats, carrot}));
		CHECK(wld.has(wolf, {eats, rabbit}));
		CHECK_FALSE(wld.has(wolf, {eats, carrot}));
		CHECK_FALSE(wld.has(rabbit, {eats, wolf}));

		{
			auto q = wld.query().add({ecs::QueryOpKind::All, ecs::QueryAccess::None, ecs::Pair(eats, carrot)});
			// auto q = wld.query().all<ecs::Pair(eats, carrot)>();
			const auto cnt = q.count();
			CHECK(cnt == 1);

			uint32_t i = 0;
			q.each([&](ecs::Entity entity) {
				CHECK(entity == rabbit);
				++i;
			});
			CHECK(i == cnt);
		}

		{
			auto q = wld.query().add({ecs::QueryOpKind::All, ecs::QueryAccess::None, ecs::Pair(eats, rabbit)});
			// auto q = wld.query().all<ecs::Pair(eats, rabbit)>();
			const auto cnt = q.count();
			CHECK(cnt == 1);

			uint32_t i = 0;
			q.each([&](ecs::Entity entity) {
				CHECK(entity == wolf);
				++i;
			});
			CHECK(i == cnt);
		}
	}

	SUBCASE("Intermediate") {
		TestWorld twld;
		auto wolf = wld.add();
		auto hare = wld.add();
		auto rabbit = wld.add();
		auto carrot = wld.add();
		auto eats = wld.add();

		wld.add(rabbit, {eats, carrot});
		wld.add(hare, {eats, carrot});
		wld.add(wolf, {eats, rabbit});

		{
			auto q = wld.query().add({ecs::QueryOpKind::All, ecs::QueryAccess::None, ecs::Pair(eats, carrot)});
			const auto cnt = q.count();
			CHECK(cnt == 2);

			uint32_t i = 0;
			q.each([&](ecs::Entity entity) {
				const bool isRabbit = entity == rabbit;
				const bool isHare = entity == hare;
				const bool is = isRabbit || isHare;
				CHECK(is);
				++i;
			});
			CHECK(i == cnt);
		}

		{
			auto q = wld.query().add({ecs::QueryOpKind::All, ecs::QueryAccess::None, ecs::Pair(eats, rabbit)});
			const auto cnt = q.count();
			CHECK(cnt == 1);

			uint32_t i = 0;
			q.each([&](ecs::Entity entity) {
				CHECK(entity == wolf);
				++i;
			});
			CHECK(i == cnt);
		}

		{
			auto q = wld.query().add({ecs::QueryOpKind::All, ecs::QueryAccess::None, ecs::Pair(eats, ecs::All)});
			const auto cnt = q.count();
			CHECK(cnt == 3);

			uint32_t i = 0;
			q.each([&](ecs::Entity entity) {
				const bool isRabbit = entity == rabbit;
				const bool isHare = entity == hare;
				const bool isWolf = entity == wolf;
				const bool is = isRabbit || isHare || isWolf;
				CHECK(is);
				++i;
			});
			CHECK(i == cnt);
		}

		{
			auto q = wld.query().add({ecs::QueryOpKind::All, ecs::QueryAccess::None, ecs::Pair(ecs::All, carrot)});
			const auto cnt = q.count();
			CHECK(cnt == 2);

			uint32_t i = 0;
			q.each([&](ecs::Entity entity) {
				const bool isRabbit = entity == rabbit;
				const bool isHare = entity == hare;
				const bool is = isRabbit || isHare;
				CHECK(is);
				++i;
			});
			CHECK(i == cnt);
		}

		{
			auto q = wld.query()
									 .add({ecs::QueryOpKind::All, ecs::QueryAccess::None, ecs::Pair(ecs::All, ecs::All)})
									 .no<ecs::Core_>()
									 .no<ecs::System_>();
			const auto cnt = q.count();
			CHECK(cnt == 3);

			uint32_t i = 0;
			q.each([&](ecs::Entity entity) {
				if (entity <= ecs::GAIA_ID(LastCoreComponent))
					return;
				const bool isRabbit = entity == rabbit;
				const bool isHare = entity == hare;
				const bool isWolf = entity == wolf;
				const bool is = isRabbit || isHare || isWolf;
				CHECK(is);
				++i;
			});
			CHECK(i == 3);
		}
	}

	SUBCASE("Exclusive") {
		TestWorld twld;

		auto on = wld.add();
		auto off = wld.add();

		auto toggled = wld.add();
		wld.add(toggled, ecs::Exclusive);

		auto wallSwitch = wld.add();
		wld.add(wallSwitch, {toggled, on});
		CHECK(wld.has(wallSwitch, {toggled, on}));
		CHECK_FALSE(wld.has(wallSwitch, {toggled, off}));
		wld.add(wallSwitch, {toggled, off});
		CHECK_FALSE(wld.has(wallSwitch, {toggled, on}));
		CHECK(wld.has(wallSwitch, {toggled, off}));

		{
			auto q = wld.query().all(ecs::Pair(toggled, on));
			const auto cnt = q.count();
			CHECK(cnt == 0);

			uint32_t i = 0;
			q.each([&]([[maybe_unused]] ecs::Iter& it) {
				++i;
			});
			CHECK(i == 0);
		}
		{
			auto q = wld.query().all(ecs::Pair(toggled, off));
			const auto cnt = q.count();
			CHECK(cnt == 1);

			uint32_t i = 0;
			q.each([&]([[maybe_unused]] ecs::Iter& it) {
				++i;
			});
			CHECK(i == 1);
		}
		{
			auto q = wld.query().all(ecs::Pair(toggled, ecs::All));
			const auto cnt = q.count();
			CHECK(cnt == 1);

			uint32_t i = 0;
			q.each([&](ecs::Iter& it) {
				const bool hasOn = it.has({toggled, on});
				const bool hasOff = it.has({toggled, off});
				CHECK_FALSE(hasOn);
				CHECK(hasOff);
				++i;
			});
			CHECK(i == 1);
		}
	}
}

TEST_CASE("Relationship target/relation") {
	TestWorld twld;

	auto wolf = wld.add();
	auto hates = wld.add();
	auto rabbit = wld.add();
	auto carrot = wld.add();
	auto salad = wld.add();
	auto eats = wld.add();

	wld.add(rabbit, {eats, carrot});
	wld.add(rabbit, {eats, salad});
	wld.add(rabbit, {hates, wolf});

	auto e = wld.target(rabbit, eats);
	const bool ret_e = e == carrot || e == salad;
	CHECK(ret_e);

	cnt::sarr_ext<ecs::Entity, 32> out;
	wld.targets(rabbit, eats, [&out](ecs::Entity target) {
		out.push_back(target);
	});
	CHECK(out.size() == 2);
	CHECK(core::has(out, carrot));
	CHECK(core::has(out, salad));
	CHECK(wld.target(rabbit, eats) == carrot);

	out.clear();
	wld.relations(rabbit, salad, [&out](ecs::Entity relation) {
		out.push_back(relation);
	});
	CHECK(out.size() == 1);
	CHECK(core::has(out, eats));
	CHECK(wld.relation(rabbit, salad) == eats);
}

TEST_CASE("Relationship source traversal") {
	TestWorld twld;

	auto rel = wld.add();

	auto root = wld.add();
	auto a = wld.add();
	auto b = wld.add();
	auto c = wld.add();
	auto d = wld.add();
	auto e = wld.add();

	wld.add(a, {rel, root});
	wld.add(b, {rel, root});
	wld.add(c, {rel, a});
	wld.add(d, {rel, b});
	wld.add(e, {rel, b});
	wld.add(e, {rel, c});

	{
		cnt::darr<ecs::Entity> direct;
		wld.sources(rel, root, [&direct](ecs::Entity source) {
			direct.push_back(source);
		});

		CHECK(direct.size() == 2);
		CHECK(core::has(direct, a));
		CHECK(core::has(direct, b));
	}

	{
		cnt::darr<ecs::Entity> bfs;
		wld.sources_bfs(rel, root, [&bfs](ecs::Entity source) {
			bfs.push_back(source);
		});

		CHECK(bfs.size() == 5);
		CHECK(bfs[0] == a);
		CHECK(bfs[1] == b);
		CHECK(bfs[2] == c);
		CHECK(bfs[3] == d);
		CHECK(bfs[4] == e);
	}

	{
		uint32_t visited = 0;
		const bool stopped = wld.sources_bfs_if(rel, root, [&](ecs::Entity source) {
			++visited;
			return source == d;
		});

		CHECK(stopped);
		CHECK(visited == 4);
	}

	{
		auto f = wld.add();
		wld.add(f, {rel, c});

		cnt::darr<ecs::Entity> bfs;
		wld.sources_bfs(rel, root, [&bfs](ecs::Entity source) {
			bfs.push_back(source);
		});

		CHECK(bfs.size() == 6);
		CHECK(bfs[0] == a);
		CHECK(bfs[1] == b);
		CHECK(bfs[2] == c);
		CHECK(bfs[3] == d);
		CHECK(bfs[4] == e);
		CHECK(bfs[5] == f);
	}
}

TEST_CASE("Relationship traversal APIs ignore stale recycled targets") {
	TestWorld twld;

	const auto rel = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.add(child, {rel, root});

	wld.del(root);
	wld.update();

	ecs::Entity recycled = ecs::EntityBad;
	for (uint32_t i = 0; i < 64 && recycled == ecs::EntityBad; ++i) {
		const auto candidate = wld.add();
		if (candidate.id() == root.id())
			recycled = candidate;
	}
	if (recycled != ecs::EntityBad)
		CHECK(recycled.gen() != root.gen());

	cnt::darr<ecs::Entity> direct;
	wld.sources(rel, root, [&](ecs::Entity source) {
		direct.push_back(source);
	});
	CHECK(direct.empty());

	uint32_t directIfHits = 0;
	wld.sources_if(rel, root, [&](ecs::Entity) {
		++directIfHits;
		return false;
	});
	CHECK(directIfHits == 0);

	cnt::darr<ecs::Entity> bfs;
	wld.sources_bfs(rel, root, [&](ecs::Entity source) {
		bfs.push_back(source);
	});
	CHECK(bfs.empty());

	uint32_t bfsIfHits = 0;
	const bool stopped = wld.sources_bfs_if(rel, root, [&](ecs::Entity) {
		++bfsIfHits;
		return true;
	});
	CHECK_FALSE(stopped);
	CHECK(bfsIfHits == 0);
}

TEST_CASE("Relationship traversal APIs ignore stale recycled relations") {
	TestWorld twld;

	const auto rel = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.add(child, {rel, root});

	wld.del(rel);
	wld.update();

	ecs::Entity recycled = ecs::EntityBad;
	for (uint32_t i = 0; i < 64 && recycled == ecs::EntityBad; ++i) {
		const auto candidate = wld.add();
		if (candidate.id() == rel.id())
			recycled = candidate;
	}
	if (recycled != ecs::EntityBad)
		CHECK(recycled.gen() != rel.gen());

	CHECK(wld.target(child, rel) == ecs::EntityBad);

	cnt::darr<ecs::Entity> targets;
	wld.targets(child, rel, [&](ecs::Entity target) {
		targets.push_back(target);
	});
	CHECK(targets.empty());

	cnt::darr<ecs::Entity> direct;
	wld.sources(rel, root, [&](ecs::Entity source) {
		direct.push_back(source);
	});
	CHECK(direct.empty());

	cnt::darr<ecs::Entity> trav;
	wld.targets_trav(rel, child, [&](ecs::Entity target) {
		trav.push_back(target);
	});
	CHECK(trav.empty());

	uint32_t travIfHits = 0;
	const bool stopped = wld.targets_trav_if(rel, child, [&](ecs::Entity) {
		++travIfHits;
		return false;
	});
	CHECK_FALSE(stopped);
	CHECK(travIfHits == 0);
}

TEST_CASE("Relationship wildcard source traversal") {
	TestWorld twld;

	auto rel0 = wld.add();
	auto rel1 = wld.add();

	auto root = wld.add();
	auto a = wld.add();
	auto b = wld.add();
	auto c = wld.add();
	auto d = wld.add();

	wld.add(a, {rel0, root});
	wld.add(b, {rel1, root});
	wld.add(c, {rel0, root});
	wld.parent(d, root);

	cnt::darr<ecs::Entity> direct;
	wld.sources(ecs::All, root, [&direct](ecs::Entity source) {
		direct.push_back(source);
	});

	CHECK(direct.size() == 4);
	CHECK(core::has(direct, a));
	CHECK(core::has(direct, b));
	CHECK(core::has(direct, c));
	CHECK(core::has(direct, d));

	uint32_t visited = 0;
	wld.sources_if(ecs::All, root, [&](ecs::Entity) {
		++visited;
		return false;
	});
	CHECK(visited == 1);
}

TEST_CASE("Relationship wildcard source traversal after change") {
	TestWorld twld;

	auto rel0 = wld.add();
	auto rel1 = wld.add();

	auto target = wld.add();
	auto a = wld.add();
	auto b = wld.add();
	auto c = wld.add();

	wld.add(a, {rel0, target});
	wld.add(b, {rel1, target});

	cnt::darr<ecs::Entity> sources;
	wld.sources(ecs::All, target, [&sources](ecs::Entity source) {
		sources.push_back(source);
	});

	CHECK(sources.size() == 2);
	CHECK(core::has(sources, a));
	CHECK(core::has(sources, b));

	wld.add(c, {rel1, target});

	sources.clear();
	wld.sources(ecs::All, target, [&sources](ecs::Entity source) {
		sources.push_back(source);
	});

	CHECK(sources.size() == 3);
	CHECK(core::has(sources, a));
	CHECK(core::has(sources, b));
	CHECK(core::has(sources, c));
}

TEST_CASE("Relationship wildcard target traversal") {
	TestWorld twld;

	auto rel0 = wld.add();
	auto rel1 = wld.add();

	auto root = wld.add();
	auto a = wld.add();
	auto b = wld.add();
	auto c = wld.add();

	wld.add(a, {rel0, root});
	wld.add(a, {rel1, b});
	wld.add(a, {rel0, c});
	wld.parent(a, root);

	cnt::darr<ecs::Entity> direct;
	wld.targets(a, ecs::All, [&direct](ecs::Entity target) {
		direct.push_back(target);
	});

	CHECK(direct.size() == 3);
	CHECK(core::has(direct, root));
	CHECK(core::has(direct, b));
	CHECK(core::has(direct, c));

	const auto first = wld.target(a, ecs::All);
	CHECK((first == root || first == b || first == c));

	uint32_t visited = 0;
	wld.targets_if(a, ecs::All, [&](ecs::Entity) {
		++visited;
		return false;
	});
	CHECK(visited == 1);
}

TEST_CASE("Relationship wildcard target traversal after change") {
	TestWorld twld;

	auto rel0 = wld.add();
	auto rel1 = wld.add();

	auto source = wld.add();
	auto a = wld.add();
	auto b = wld.add();
	auto c = wld.add();

	wld.add(source, {rel0, a});
	wld.add(source, {rel1, b});

	cnt::darr<ecs::Entity> targets;
	wld.targets(source, ecs::All, [&targets](ecs::Entity target) {
		targets.push_back(target);
	});

	CHECK(targets.size() == 2);
	CHECK(core::has(targets, a));
	CHECK(core::has(targets, b));

	wld.add(source, {rel1, c});

	targets.clear();
	wld.targets(source, ecs::All, [&targets](ecs::Entity target) {
		targets.push_back(target);
	});

	CHECK(targets.size() == 3);
	CHECK(core::has(targets, a));
	CHECK(core::has(targets, b));
	CHECK(core::has(targets, c));
}

TEST_CASE("Relationship wildcard target traversal with many exclusive dontfragment relations") {
	TestWorld twld;

	constexpr uint32_t N = 128;
	auto source = wld.add();

	cnt::darr<ecs::Entity> expected;
	expected.reserve(N);

	GAIA_FOR(N) {
		auto rel = wld.add();
		wld.add(rel, ecs::Exclusive);
		wld.add(rel, ecs::DontFragment);

		auto target = wld.add();
		expected.push_back(target);
		wld.add(source, {rel, target});
	}

	cnt::darr<ecs::Entity> targets;
	wld.targets(source, ecs::All, [&targets](ecs::Entity target) {
		targets.push_back(target);
	});

	CHECK(targets.size() == N);
	for (auto target: expected)
		CHECK(core::has(targets, target));
	CHECK(wld.target(source, ecs::All) != ecs::EntityBad);
}

TEST_CASE("Child hierarchy traversal") {
	TestWorld twld;

	auto root = wld.add();
	auto c0 = wld.add();
	auto c1 = wld.add();
	auto c00 = wld.add();
	auto c10 = wld.add();

	wld.child(c0, root);
	wld.child(c1, root);
	wld.child(c00, c0);
	wld.child(c10, c1);

	{
		cnt::darr<ecs::Entity> direct;
		wld.children(root, [&direct](ecs::Entity child) {
			direct.push_back(child);
		});

		CHECK(direct.size() == 2);
		CHECK(core::has(direct, c0));
		CHECK(core::has(direct, c1));
	}

	{
		cnt::darr<ecs::Entity> bfs;
		wld.children_bfs(root, [&bfs](ecs::Entity child) {
			bfs.push_back(child);
		});

		CHECK(bfs.size() == 4);
		CHECK(bfs[0] == c0);
		CHECK(bfs[1] == c1);
		CHECK(bfs[2] == c00);
		CHECK(bfs[3] == c10);
	}

	{
		wld.enable(c0, false);

		cnt::darr<ecs::Entity> bfs;
		wld.children_bfs(root, [&bfs](ecs::Entity child) {
			bfs.push_back(child);
		});

		CHECK(bfs.size() == 2);
		CHECK(bfs[0] == c1);
		CHECK(bfs[1] == c10);
	}
}

TEST_CASE("Upward traversal with disabled ancestors") {
	TestWorld twld;

	auto root = wld.add();
	auto parent = wld.add();
	auto child = wld.add();

	wld.child(parent, root);
	wld.child(child, parent);

	{
		cnt::darr<ecs::Entity> up;
		wld.targets_trav(ecs::ChildOf, child, [&up](ecs::Entity entity) {
			up.push_back(entity);
		});

		CHECK(up.size() == 2);
		CHECK(up[0] == parent);
		CHECK(up[1] == root);
	}

	wld.enable(parent, false);

	{
		cnt::darr<ecs::Entity> up;
		wld.targets_trav(ecs::ChildOf, child, [&up](ecs::Entity entity) {
			up.push_back(entity);
		});

		CHECK(up.empty());
	}
}

TEST_CASE("Query entity each bfs") {
	TestWorld twld;

	auto rel = wld.add();

	auto root = wld.add();
	auto a = wld.add();
	auto b = wld.add();
	auto c = wld.add();

	wld.add<Position>(root, {0, 0, 0});
	wld.add<Position>(a, {0, 0, 0});
	wld.add<Position>(b, {0, 0, 0});
	wld.add<Position>(c, {0, 0, 0});

	// Dependency graph:
	//   root -> a -> c
	//   root -> b
	wld.add(a, {rel, root});
	wld.add(b, {rel, root});
	wld.add(c, {rel, a});

	auto q = wld.query().all<Position>();
	cnt::darray<ecs::Entity> ents;
	q.bfs(rel).each([&](ecs::Entity e) {
		ents.push_back(e);
	});

	CHECK(ents.size() == 4);
	CHECK(ents[0] == root);
	CHECK(ents[1] == a);
	CHECK(ents[2] == b);
	CHECK(ents[3] == c);

	// Re-run without changes (cache hit expected).
	ents.clear();
	q.bfs(rel).each([&](ecs::Entity e) {
		ents.push_back(e);
	});
	CHECK(ents.size() == 4);
	CHECK(ents[0] == root);
	CHECK(ents[1] == a);
	CHECK(ents[2] == b);
	CHECK(ents[3] == c);

	// Change dependency topology but keep the same entity set.
	wld.del(b, {rel, root});
	wld.add(b, {rel, c});

	ents.clear();
	q.bfs(rel).each([&](ecs::Entity e) {
		ents.push_back(e);
	});
	CHECK(ents.size() == 4);
	CHECK(ents[0] == root);
	CHECK(ents[1] == a);
	CHECK(ents[2] == c);
	CHECK(ents[3] == b);
}

TEST_CASE("Query typed each bfs") {
	TestWorld twld;

	auto rel = wld.add();

	auto root = wld.add();
	auto a = wld.add();
	auto b = wld.add();
	auto c = wld.add();

	wld.add<Position>(root, {0, 0, 0});
	wld.add<Position>(a, {1, 0, 0});
	wld.add<Position>(b, {2, 0, 0});
	wld.add<Position>(c, {3, 0, 0});

	wld.add(a, {rel, root});
	wld.add(b, {rel, root});
	wld.add(c, {rel, a});

	auto q = wld.query().all<Position>();
	cnt::darray<ecs::Entity> ents;
	cnt::darray<float> xs;
	q.bfs(rel).each([&](ecs::Entity e, const Position& pos) {
		ents.push_back(e);
		xs.push_back(pos.x);
	});

	CHECK(ents.size() == 4);
	CHECK(xs.size() == 4);
	CHECK(ents[0] == root);
	CHECK(ents[1] == a);
	CHECK(ents[2] == b);
	CHECK(ents[3] == c);
	CHECK(xs[0] == doctest::Approx(0.0f));
	CHECK(xs[1] == doctest::Approx(1.0f));
	CHECK(xs[2] == doctest::Approx(2.0f));
	CHECK(xs[3] == doctest::Approx(3.0f));
}

TEST_CASE("Query iterator each bfs") {
	TestWorld twld;

	auto rel = wld.add();

	auto root = wld.add();
	auto a = wld.add();
	auto b = wld.add();
	auto c = wld.add();

	wld.add<Position>(root, {0, 0, 0});
	wld.add<Position>(a, {1, 0, 0});
	wld.add<Position>(b, {2, 0, 0});
	wld.add<Position>(c, {3, 0, 0});

	wld.add(a, {rel, root});
	wld.add(b, {rel, root});
	wld.add(c, {rel, a});

	auto q = wld.query().all<Position>();
	cnt::darray<ecs::Entity> ents;
	cnt::darray<float> xs;
	q.bfs(rel).each([&](ecs::Iter& it) {
		auto entView = it.view<ecs::Entity>();
		auto posView = it.view<Position>();
		GAIA_EACH(it) {
			ents.push_back(entView[i]);
			xs.push_back(posView[i].x);
		}
	});

	CHECK(ents.size() == 4);
	CHECK(xs.size() == 4);
	CHECK(ents[0] == root);
	CHECK(ents[1] == a);
	CHECK(ents[2] == b);
	CHECK(ents[3] == c);
	CHECK(xs[0] == doctest::Approx(0.0f));
	CHECK(xs[1] == doctest::Approx(1.0f));
	CHECK(xs[2] == doctest::Approx(2.0f));
	CHECK(xs[3] == doctest::Approx(3.0f));

	wld.del(b, {rel, root});
	wld.add(b, {rel, c});

	ents.clear();
	xs.clear();
	q.bfs(rel).each([&](ecs::Iter& it) {
		auto entView = it.view<ecs::Entity>();
		auto posView = it.view<Position>();
		GAIA_EACH(it) {
			ents.push_back(entView[i]);
			xs.push_back(posView[i].x);
		}
	});

	CHECK(ents.size() == 4);
	CHECK(xs.size() == 4);
	CHECK(ents[0] == root);
	CHECK(ents[1] == a);
	CHECK(ents[2] == c);
	CHECK(ents[3] == b);
	CHECK(xs[0] == doctest::Approx(0.0f));
	CHECK(xs[1] == doctest::Approx(1.0f));
	CHECK(xs[2] == doctest::Approx(3.0f));
	CHECK(xs[3] == doctest::Approx(2.0f));
}

TEST_CASE("Query typed each bfs multiple components") {
	TestWorld twld;

	auto rel = wld.add();

	auto root = wld.add();
	auto a = wld.add();
	auto b = wld.add();
	auto c = wld.add();

	wld.add<Position>(root, {0, 0, 0});
	wld.add<Position>(a, {1, 0, 0});
	wld.add<Position>(b, {2, 0, 0});
	wld.add<Position>(c, {3, 0, 0});

	wld.add<Scale>(root, {10, 0, 0});
	wld.add<Scale>(a, {11, 0, 0});
	wld.add<Scale>(b, {12, 0, 0});
	wld.add<Scale>(c, {13, 0, 0});

	wld.add(a, {rel, root});
	wld.add(b, {rel, root});
	wld.add(c, {rel, a});

	auto q = wld.query().all<Position>().all<Scale>();
	cnt::darray<ecs::Entity> ents;
	cnt::darray<float> px;
	cnt::darray<float> sx;
	q.bfs(rel).each([&](ecs::Entity e, const Position& pos, const Scale& scale) {
		ents.push_back(e);
		px.push_back(pos.x);
		sx.push_back(scale.x);
	});

	CHECK(ents.size() == 4);
	CHECK(px.size() == 4);
	CHECK(sx.size() == 4);
	CHECK(ents[0] == root);
	CHECK(ents[1] == a);
	CHECK(ents[2] == b);
	CHECK(ents[3] == c);
	CHECK(px[0] == doctest::Approx(0.0f));
	CHECK(px[1] == doctest::Approx(1.0f));
	CHECK(px[2] == doctest::Approx(2.0f));
	CHECK(px[3] == doctest::Approx(3.0f));
	CHECK(sx[0] == doctest::Approx(10.0f));
	CHECK(sx[1] == doctest::Approx(11.0f));
	CHECK(sx[2] == doctest::Approx(12.0f));
	CHECK(sx[3] == doctest::Approx(13.0f));
}

TEST_CASE("Query typed each bfs mutable components") {
	TestWorld twld;

	auto rel = wld.add();

	auto root = wld.add();
	auto a = wld.add();
	auto b = wld.add();
	auto c = wld.add();

	wld.add<Position>(root, {0, 0, 0});
	wld.add<Position>(a, {1, 0, 0});
	wld.add<Position>(b, {2, 0, 0});
	wld.add<Position>(c, {3, 0, 0});

	wld.add<Scale>(root, {10, 0, 0});
	wld.add<Scale>(a, {11, 0, 0});
	wld.add<Scale>(b, {12, 0, 0});
	wld.add<Scale>(c, {13, 0, 0});

	wld.add(a, {rel, root});
	wld.add(b, {rel, root});
	wld.add(c, {rel, a});

	auto q = wld.query().all<Position&>().all<Scale>();
	cnt::darray<ecs::Entity> ents;
	q.bfs(rel).each([&](ecs::Entity e, Position& pos, const Scale& scale) {
		ents.push_back(e);
		pos.x += scale.x;
	});

	CHECK(ents.size() == 4);
	CHECK(ents[0] == root);
	CHECK(ents[1] == a);
	CHECK(ents[2] == b);
	CHECK(ents[3] == c);

	CHECK(wld.get<Position>(root).x == doctest::Approx(10.0f));
	CHECK(wld.get<Position>(a).x == doctest::Approx(12.0f));
	CHECK(wld.get<Position>(b).x == doctest::Approx(14.0f));
	CHECK(wld.get<Position>(c).x == doctest::Approx(16.0f));
}

TEST_CASE("Query entity each bfs with disabled ancestor barriers") {
	TestWorld twld;

	auto root = wld.add();
	auto child = wld.add();
	auto grandChild = wld.add();

	wld.child(child, root);
	wld.child(grandChild, child);

	wld.add<Position>(child, {0, 0, 0});
	wld.add<Position>(grandChild, {0, 0, 0});

	auto q = wld.query().all<Position>();

	{
		cnt::darr<ecs::Entity> ents;
		q.bfs(ecs::ChildOf).each([&](ecs::Entity e) {
			ents.push_back(e);
		});

		CHECK(ents.size() == 2);
		CHECK(ents[0] == child);
		CHECK(ents[1] == grandChild);
	}

	// Disable a non-matching parent/root. The subtree should be pruned even though the
	// query input set of Position entities would otherwise stay the same.
	wld.enable(root, false);
	{
		cnt::darr<ecs::Entity> ents;
		q.bfs(ecs::ChildOf).each([&](ecs::Entity e) {
			ents.push_back(e);
		});

		CHECK(ents.empty());
	}

	wld.enable(root, true);
	wld.enable(child, false);
	{
		cnt::darr<ecs::Entity> ents;
		q.bfs(ecs::ChildOf).each([&](ecs::Entity e) {
			ents.push_back(e);
		});

		CHECK(ents.empty());
	}
}

TEST_CASE("Query cascade ChildOf orders entities by depth") {
	TestWorld twld;

	auto root = wld.add();
	auto childA = wld.add();
	auto childB = wld.add();
	auto grandChild = wld.add();

	wld.child(childA, root);
	wld.child(childB, root);
	wld.child(grandChild, childA);

	wld.add<Position>(root, {0, 0, 0});
	wld.add<Position>(childA, {0, 0, 0});
	wld.add<Position>(childB, {0, 0, 0});
	wld.add<Position>(grandChild, {0, 0, 0});

	auto depth_of = [&](ecs::Entity entity) {
		uint32_t depth = 0;
		auto curr = entity;
		while (true) {
			const auto parent = wld.target(curr, ecs::ChildOf);
			if (parent == ecs::EntityBad || parent == curr)
				break;
			++depth;
			curr = parent;
		}
		return depth;
	};

	auto q = wld.query().all<Position>().cascade(ecs::ChildOf);
	cnt::darr<ecs::Entity> ents;
	q.each([&](ecs::Entity e) {
		ents.push_back(e);
	});

	CHECK(ents.size() == 4);
	CHECK(depth_of(ents[0]) == 0);
	CHECK(depth_of(ents[1]) == 1);
	CHECK(depth_of(ents[2]) == 1);
	CHECK(depth_of(ents[3]) == 2);
	CHECK(ents[0] == root);
	CHECK(ents[3] == grandChild);
}

TEST_CASE("Query cascade ChildOf updates after reparent") {
	TestWorld twld;

	auto root = wld.add();
	auto child = wld.add();
	auto grandChild = wld.add();
	auto leaf = wld.add();

	wld.child(child, root);
	wld.child(grandChild, child);
	wld.child(leaf, root);

	wld.add<Position>(root, {0, 0, 0});
	wld.add<Position>(child, {0, 0, 0});
	wld.add<Position>(grandChild, {0, 0, 0});
	wld.add<Position>(leaf, {0, 0, 0});

	auto q = wld.query().all<Position>().cascade(ecs::ChildOf);
	cnt::darr<ecs::Entity> ents;
	q.each([&](ecs::Entity e) {
		ents.push_back(e);
	});

	CHECK(ents.size() == 4);
	CHECK(ents[0] == root);
	CHECK(ents[3] == grandChild);

	wld.del(grandChild, ecs::Pair(ecs::ChildOf, child));
	wld.add(grandChild, ecs::Pair(ecs::ChildOf, leaf));

	ents.clear();
	q.each([&](ecs::Entity e) {
		ents.push_back(e);
	});

	CHECK(ents.size() == 4);
	CHECK(ents[0] == root);
	CHECK(ents[1] == child);
	CHECK(ents[2] == leaf);
	CHECK(ents[3] == grandChild);
}

TEST_CASE("Query cascade ChildOf updates when ancestor depth changes") {
	TestWorld twld;

	auto root = wld.add();
	auto parent = wld.add();
	auto sibling = wld.add();
	auto leaf = wld.add();
	auto cousinGrandChild = wld.add();

	wld.child(parent, root);
	wld.child(sibling, root);
	wld.child(leaf, parent);
	wld.child(cousinGrandChild, sibling);

	wld.add<Position>(root, {0, 0, 0});
	wld.add<Position>(parent, {0, 0, 0});
	wld.add<Position>(sibling, {0, 0, 0});
	wld.add<Position>(leaf, {0, 0, 0});
	wld.add<Position>(cousinGrandChild, {0, 0, 0});

	auto q = wld.query().all<Position>().cascade(ecs::ChildOf);
	cnt::darr<ecs::Entity> ents;
	q.each([&](ecs::Entity e) {
		ents.push_back(e);
	});

	CHECK(ents.size() == 5);
	const bool lastIsDeepDescendant = ents[4] == leaf || ents[4] == cousinGrandChild;
	CHECK(lastIsDeepDescendant);

	wld.del(parent, ecs::Pair(ecs::ChildOf, root));

	ents.clear();
	q.each([&](ecs::Entity e) {
		ents.push_back(e);
	});

	CHECK(ents.size() == 5);
	const bool firstIsRootLevel = ents[0] == root || ents[0] == parent;
	CHECK(firstIsRootLevel);
	CHECK(core::has(ents, leaf));
	CHECK(core::has(ents, cousinGrandChild));

	const auto leafIdx = core::get_index(ents, leaf);
	const auto cousinIdx = core::get_index(ents, cousinGrandChild);
	CHECK(leafIdx != BadIndex);
	CHECK(cousinIdx != BadIndex);
	CHECK(leafIdx < cousinIdx);
}

TEST_CASE("Query cascade custom relation orders entities by depth") {
	TestWorld twld;

	auto rel = wld.add();

	auto root = wld.add();
	auto childA = wld.add();
	auto childB = wld.add();
	auto grandChild = wld.add();

	wld.add(childA, ecs::Pair(rel, root));
	wld.add(childB, ecs::Pair(rel, root));
	wld.add(grandChild, ecs::Pair(rel, childA));

	wld.add<Position>(root, {0, 0, 0});
	wld.add<Position>(childA, {0, 0, 0});
	wld.add<Position>(childB, {0, 0, 0});
	wld.add<Position>(grandChild, {0, 0, 0});

	auto depth_of = [&](ecs::Entity entity) {
		uint32_t depth = 0;
		auto curr = entity;
		while (true) {
			const auto parent = wld.target(curr, rel);
			if (parent == ecs::EntityBad || parent == curr)
				break;
			++depth;
			curr = parent;
		}
		return depth;
	};

	auto q = wld.query().all<Position>().cascade(rel);
	cnt::darr<ecs::Entity> ents;
	q.each([&](ecs::Entity e) {
		ents.push_back(e);
	});

	CHECK(ents.size() == 4);
	CHECK(depth_of(ents[0]) == 0);
	CHECK(depth_of(ents[1]) == 1);
	CHECK(depth_of(ents[2]) == 1);
	CHECK(depth_of(ents[3]) == 2);
	CHECK(ents[0] == root);
	CHECK(ents[3] == grandChild);
}

template <typename TQuery>
void Test_Query_Equality() {
	constexpr bool UseCachedQuery = std::is_same_v<TQuery, ecs::Query>;
	constexpr uint32_t N = 100;

	auto verify = [&](TQuery& q1, TQuery& q2, TQuery& q3, TQuery& q4) {
		CHECK(q1.count() == q2.count());
		CHECK(q1.count() == q3.count());
		CHECK(q1.count() == q4.count());

		cnt::darr<ecs::Entity> ents1, ents2, ents3, ents4;
		q1.arr(ents1);
		q2.arr(ents2);
		q3.arr(ents3);
		q4.arr(ents4);
		CHECK(ents1.size() == ents2.size());
		CHECK(ents1.size() == ents3.size());
		CHECK(ents1.size() == ents4.size());

		uint32_t i = 0;
		for (auto e: ents1) {
			CHECK(e == ents2[i]);
			++i;
		}
		i = 0;
		for (auto e: ents1) {
			CHECK(e == ents3[i]);
			++i;
		}
		i = 0;
		for (auto e: ents1) {
			CHECK(e == ents4[i]);
			++i;
		}
	};

	SUBCASE("2 components") {
		TestWorld twld;
		GAIA_FOR(N) {
			auto e = wld.add();
			wld.add<Position>(e);
			wld.add<Rotation>(e);
		}

		auto p = wld.add<Position>().entity;
		auto r = wld.add<Rotation>().entity;

		auto qq1 = wld.query<UseCachedQuery>().template all<Position>().template all<Rotation>();
		auto qq2 = wld.query<UseCachedQuery>().template all<Rotation>().template all<Position>();
		auto qq3 = wld.query<UseCachedQuery>().all(p).all(r);
		auto qq4 = wld.query<UseCachedQuery>().all(r).all(p);
		verify(qq1, qq2, qq3, qq4);

		auto qq1_ = wld.query<UseCachedQuery>().add("Position, Rotation");
		auto qq2_ = wld.query<UseCachedQuery>().add("Rotation, Position");
		auto qq3_ = wld.query<UseCachedQuery>().add("Position").add("Rotation");
		auto qq4_ = wld.query<UseCachedQuery>().add("Rotation").add("Position");
		verify(qq1_, qq2_, qq3_, qq4_);
	}
	SUBCASE("4 components") {
		TestWorld twld;
		GAIA_FOR(N) {
			auto e = wld.add();
			wld.add<Position>(e);
			wld.add<Rotation>(e);
			wld.add<Acceleration>(e);
			wld.add<Something>(e);
		}

		auto p = wld.add<Position>().entity;
		auto r = wld.add<Rotation>().entity;
		auto a = wld.add<Acceleration>().entity;
		auto s = wld.add<Something>().entity;

		auto qq1 = wld.query<UseCachedQuery>()
									 .template all<Position>()
									 .template all<Rotation>()
									 .template all<Acceleration>()
									 .template all<Something>();
		auto qq2 = wld.query<UseCachedQuery>()
									 .template all<Rotation>()
									 .template all<Something>()
									 .template all<Position>()
									 .template all<Acceleration>();
		auto qq3 = wld.query<UseCachedQuery>().all(p).all(r).all(a).all(s);
		auto qq4 = wld.query<UseCachedQuery>().all(r).all(p).all(s).all(a);
		verify(qq1, qq2, qq3, qq4);

		auto qq1_ = wld.query<UseCachedQuery>().add("Position, Rotation, Acceleration, Something");
		auto qq2_ = wld.query<UseCachedQuery>().add("Rotation, Something, Position, Acceleration");
		auto qq3_ = wld.query<UseCachedQuery>().add("Position").add("Rotation").add("Acceleration").add("Something");
		auto qq4_ = wld.query<UseCachedQuery>().add("Rotation").add("Position").add("Something").add("Acceleration");
		verify(qq1_, qq2_, qq3_, qq4_);
	}
}

TEST_CASE("Query - equality") {
	SUBCASE("Cached query") {
		Test_Query_Equality<ecs::Query>();
	}
	SUBCASE("Non-cached query") {
		Test_Query_Equality<ecs::QueryUncached>();
	}
}

template <typename TQuery>
void Test_Query_Reset() {
	constexpr bool UseCachedQuery = std::is_same_v<TQuery, ecs::Query>;

	TestWorld twld;
	auto wolf = wld.add();
	auto rabbit = wld.add();
	auto carrot = wld.add();
	auto eats = wld.add();

	wld.add(rabbit, {eats, carrot});
	wld.add(wolf, {eats, rabbit});

	auto q1 = wld.query<UseCachedQuery>().add({ecs::QueryOpKind::All, ecs::QueryAccess::None, ecs::Pair(eats, carrot)});
	auto q2 = wld.query<UseCachedQuery>().add("(%e, %e)", eats.value(), carrot.value());

	auto check_queries = [&]() {
		const auto cnt1 = q1.count();
		CHECK(cnt1 == 1);
		const auto cnt2 = q2.count();
		CHECK(cnt2 == 1);
	};

	check_queries();

	q1.reset();
	check_queries();

	q2.reset();
	check_queries();

	q1.reset();
	q2.reset();
	check_queries();
}

TEST_CASE("Query - delete") {
	SUBCASE("Cached query") {
		Test_Query_Reset<ecs::Query>();
	}
	SUBCASE("Cached query") {
		Test_Query_Reset<ecs::QueryUncached>();
	}
}

TEST_CASE("Query - delete from cache") {
	TestWorld twld;
	auto wolf = wld.add();
	auto rabbit = wld.add();
	auto carrot = wld.add();
	auto eats = wld.add();

	wld.add(rabbit, {eats, carrot});
	wld.add(wolf, {eats, rabbit});

	auto q1 = wld.query().add({ecs::QueryOpKind::All, ecs::QueryAccess::None, ecs::Pair(eats, carrot)});
	auto q2 = wld.query().add("(%e, %e)", eats.value(), carrot.value());

	{
		const auto cnt1 = q1.count();
		CHECK(cnt1 == 1);
		const auto cnt2 = q2.count();
		CHECK(cnt2 == 1);
	};

	q1.destroy();
	{
		const auto cnt1 = q1.count();
		CHECK(cnt1 == 1);
		const auto cnt2 = q2.count();
		CHECK(cnt2 == 1);
	}
	q2.destroy();
	{
		const auto cnt1 = q1.count();
		CHECK(cnt1 == 0);
		const auto cnt2 = q2.count();
		CHECK(cnt2 == 0);
	}
}
