#include "test_common.h"

#define TestWorld SparseTestWorld

TEST_CASE("Enable") {
	// 1,500 picked so we create enough entities that they overflow into another chunk
	const uint32_t N = 1'500;

	TestWorld twld;
	cnt::darr<ecs::Entity> arr;
	arr.reserve(N);

	auto create = [&]() {
		auto e = wld.add();
		wld.add<Position>(e);
		arr.push_back(e);
	};

	GAIA_FOR(N) create();

	SUBCASE("State validity") {
		wld.enable(arr[0], false);
		CHECK_FALSE(wld.enabled(arr[0]));

		wld.enable(arr[0], true);
		CHECK(wld.enabled(arr[0]));

		wld.enable(arr[1], false);
		CHECK(wld.enabled(arr[0]));
		CHECK_FALSE(wld.enabled(arr[1]));

		wld.enable(arr[1], true);
		CHECK(wld.enabled(arr[0]));
		CHECK(wld.enabled(arr[1]));
	}

	SUBCASE("State persistence") {
		wld.enable(arr[0], false);
		CHECK_FALSE(wld.enabled(arr[0]));
		auto e = arr[0];
		wld.del<Position>(e);
		CHECK_FALSE(wld.enabled(e));

		wld.enable(arr[0], true);
		wld.add<Position>(arr[0]);
		CHECK(wld.enabled(arr[0]));
	}

	{
		ecs::Query q = wld.query().all<Position>();

		auto checkQuery = [&q](uint32_t expectedCountAll, uint32_t expectedCountEnabled, uint32_t expectedCountDisabled) {
			{
				uint32_t cnt = 0;
				q.each(
						[&](ecs::Iter& it) {
							const uint32_t cExpected = it.size();
							uint32_t c = 0;
							GAIA_EACH(it)++ c;
							CHECK(c == cExpected);
							cnt += c;
						},
						ecs::Constraints::AcceptAll);
				CHECK(cnt == expectedCountAll);

				cnt = q.count(ecs::Constraints::AcceptAll);
				CHECK(cnt == expectedCountAll);
			}
			{
				uint32_t cnt = 0;
				q.each([&]([[maybe_unused]] ecs::Iter& it) {
					const uint32_t cExpected = it.size();
					uint32_t c = 0;
					GAIA_EACH(it) {
						CHECK(it.enabled(i));
						++c;
					}
					CHECK(c == cExpected);
					cnt += c;
				});
				CHECK(cnt == expectedCountEnabled);

				cnt = q.count();
				CHECK(cnt == expectedCountEnabled);
			}
			{
				uint32_t cnt = 0;
				q.each(
						[&]([[maybe_unused]] ecs::Iter& it) {
							const uint32_t cExpected = it.size();
							uint32_t c = 0;
							GAIA_EACH(it) {
								CHECK_FALSE(it.enabled(i));
								++c;
							}
							CHECK(c == cExpected);
							cnt += c;
						},
						ecs::Constraints::DisabledOnly);
				CHECK(cnt == expectedCountDisabled);

				cnt = q.count(ecs::Constraints::DisabledOnly);
				CHECK(cnt == expectedCountDisabled);
			}
		};

		checkQuery(N, N, 0);

		SUBCASE("Disable vs query") {
			wld.enable(arr[1000], false);
			checkQuery(N, N - 1, 1);
		}

		SUBCASE("Enable vs query") {
			wld.enable(arr[1000], true);
			checkQuery(N, N, 0);
		}

		SUBCASE("Disable vs query") {
			wld.enable(arr[1], false);
			wld.enable(arr[100], false);
			wld.enable(arr[999], false);
			wld.enable(arr[1400], false);
			checkQuery(N, N - 4, 4);
		}

		SUBCASE("Enable vs query") {
			wld.enable(arr[1], true);
			wld.enable(arr[100], true);
			wld.enable(arr[999], true);
			wld.enable(arr[1400], true);
			checkQuery(N, N, 0);
		}

		SUBCASE("Delete") {
			wld.del(arr[0]);
			CHECK_FALSE(wld.has(arr[0]));
			checkQuery(N - 1, N - 1, 0);

			wld.del(arr[10]);
			CHECK_FALSE(wld.has(arr[10]));
			checkQuery(N - 2, N - 2, 0);

			wld.enable(arr[1], false);
			CHECK_FALSE(wld.enabled(arr[1]));
			wld.del(arr[1]);
			CHECK_FALSE(wld.has(arr[1]));
			checkQuery(N - 3, N - 3, 0);

			wld.enable(arr[1000], false);
			CHECK_FALSE(wld.enabled(arr[1000]));
			wld.del(arr[1000]);
			CHECK_FALSE(wld.has(arr[1000]));
			checkQuery(N - 4, N - 4, 0);
		}
	}

	SUBCASE("AoS") {
		wld.cleanup();
		auto e0 = wld.add();
		auto e1 = wld.add();
		auto e2 = wld.add();
		float vals[3] = {1.f, 2.f, 3.f};
		wld.add<Position>(e0, {vals[0], vals[1], vals[2]});
		wld.add<Position>(e1, {vals[0] * 10.f, vals[1] * 10.f, vals[2] * 10.f});
		wld.add<Position>(e2, {vals[0] * 100.f, vals[1] * 100.f, vals[2] * 100.f});

		{
			auto p0 = wld.get<Position>(e0);
			CHECK(p0.x == vals[0]);
			CHECK(p0.y == vals[1]);
			CHECK(p0.z == vals[2]);
			auto p1 = wld.get<Position>(e1);
			CHECK(p1.x == vals[0] * 10.f);
			CHECK(p1.y == vals[1] * 10.f);
			CHECK(p1.z == vals[2] * 10.f);
			auto p2 = wld.get<Position>(e2);
			CHECK(p2.x == vals[0] * 100.f);
			CHECK(p2.y == vals[1] * 100.f);
			CHECK(p2.z == vals[2] * 100.f);

			ecs::Query q = wld.query().all<Position>();
			uint32_t cnt = 0;
			q.each([&](const Position& pos) {
				if (cnt == 0) {
					CHECK(pos.x == vals[0]);
					CHECK(pos.y == vals[1]);
					CHECK(pos.z == vals[2]);
				} else if (cnt == 1) {
					CHECK(pos.x == vals[0] * 10.f);
					CHECK(pos.y == vals[1] * 10.f);
					CHECK(pos.z == vals[2] * 10.f);
				} else if (cnt == 2) {
					CHECK(pos.x == vals[0] * 100.f);
					CHECK(pos.y == vals[1] * 100.f);
					CHECK(pos.z == vals[2] * 100.f);
				}
				++cnt;
			});
			CHECK(cnt == 3);
		}

		wld.enable(e2, false);

		{
			auto p0 = wld.get<Position>(e0);
			CHECK(p0.x == vals[0]);
			CHECK(p0.y == vals[1]);
			CHECK(p0.z == vals[2]);
			auto p1 = wld.get<Position>(e1);
			CHECK(p1.x == vals[0] * 10.f);
			CHECK(p1.y == vals[1] * 10.f);
			CHECK(p1.z == vals[2] * 10.f);
			auto p2 = wld.get<Position>(e2);
			CHECK(p2.x == vals[0] * 100.f);
			CHECK(p2.y == vals[1] * 100.f);
			CHECK(p2.z == vals[2] * 100.f);

			ecs::Query q = wld.query().all<Position>();
			uint32_t cnt = 0;
			q.each([&](const Position& pos) {
				if (cnt == 0) {
					CHECK(pos.x == vals[0] * 10.f);
					CHECK(pos.y == vals[1] * 10.f);
					CHECK(pos.z == vals[2] * 10.f);
				} else if (cnt == 1) {
					CHECK(pos.x == vals[0]);
					CHECK(pos.y == vals[1]);
					CHECK(pos.z == vals[2]);
				}
				++cnt;
			});
			CHECK(cnt == 2);
		}
	}

	SUBCASE("SoA") {
		wld.cleanup();
		auto e0 = wld.add();
		auto e1 = wld.add();
		auto e2 = wld.add();
		float vals[3] = {1.f, 2.f, 3.f};
		wld.add<PositionSoA>(e0, {vals[0], vals[1], vals[2]});
		wld.add<PositionSoA>(e1, {vals[0] * 10.f, vals[1] * 10.f, vals[2] * 10.f});
		wld.add<PositionSoA>(e2, {vals[0] * 100.f, vals[1] * 100.f, vals[2] * 100.f});

		{
			auto p0 = wld.get<PositionSoA>(e0);
			CHECK(p0.x == vals[0]);
			CHECK(p0.y == vals[1]);
			CHECK(p0.z == vals[2]);
			auto p1 = wld.get<PositionSoA>(e1);
			CHECK(p1.x == vals[0] * 10.f);
			CHECK(p1.y == vals[1] * 10.f);
			CHECK(p1.z == vals[2] * 10.f);
			auto p2 = wld.get<PositionSoA>(e2);
			CHECK(p2.x == vals[0] * 100.f);
			CHECK(p2.y == vals[1] * 100.f);
			CHECK(p2.z == vals[2] * 100.f);

			ecs::Query q = wld.query().all<PositionSoA>();
			uint32_t cnt = 0;
			q.each([&](const PositionSoA& pos) {
				if (cnt == 0) {
					CHECK(pos.x == vals[0]);
					CHECK(pos.y == vals[1]);
					CHECK(pos.z == vals[2]);
				} else if (cnt == 1) {
					CHECK(pos.x == vals[0] * 10.f);
					CHECK(pos.y == vals[1] * 10.f);
					CHECK(pos.z == vals[2] * 10.f);
				} else if (cnt == 2) {
					CHECK(pos.x == vals[0] * 100.f);
					CHECK(pos.y == vals[1] * 100.f);
					CHECK(pos.z == vals[2] * 100.f);
				}
				++cnt;
			});
			CHECK(cnt == 3);
		}

		wld.enable(e2, false);

		{
			auto p0 = wld.get<PositionSoA>(e0);
			CHECK(p0.x == vals[0]);
			CHECK(p0.y == vals[1]);
			CHECK(p0.z == vals[2]);
			auto p1 = wld.get<PositionSoA>(e1);
			CHECK(p1.x == vals[0] * 10.f);
			CHECK(p1.y == vals[1] * 10.f);
			CHECK(p1.z == vals[2] * 10.f);
			auto p2 = wld.get<PositionSoA>(e2);
			CHECK(p2.x == vals[0] * 100.f);
			CHECK(p2.y == vals[1] * 100.f);
			CHECK(p2.z == vals[2] * 100.f);

			ecs::Query q = wld.query().all<PositionSoA>();
			uint32_t cnt = 0;
			q.each([&](const PositionSoA& pos) {
				if (cnt == 0) {
					CHECK(pos.x == vals[0] * 10.f);
					CHECK(pos.y == vals[1] * 10.f);
					CHECK(pos.z == vals[2] * 10.f);
				} else if (cnt == 1) {
					CHECK(pos.x == vals[0]);
					CHECK(pos.y == vals[1]);
					CHECK(pos.z == vals[2]);
				}
				++cnt;
			});
			CHECK(cnt == 2);
		}
	}

	SUBCASE("AoS + SoA") {
		wld.cleanup();
		auto e0 = wld.add();
		auto e1 = wld.add();
		auto e2 = wld.add();
		float vals[3] = {1.f, 2.f, 3.f};
		wld.add<PositionSoA>(e0, {vals[0], vals[1], vals[2]});
		wld.add<PositionSoA>(e1, {vals[0] * 10.f, vals[1] * 10.f, vals[2] * 10.f});
		wld.add<PositionSoA>(e2, {vals[0] * 100.f, vals[1] * 100.f, vals[2] * 100.f});
		wld.add<Position>(e0, {vals[0], vals[1], vals[2]});
		wld.add<Position>(e1, {vals[0] * 10.f, vals[1] * 10.f, vals[2] * 10.f});
		wld.add<Position>(e2, {vals[0] * 100.f, vals[1] * 100.f, vals[2] * 100.f});

		{
			auto p0 = wld.get<PositionSoA>(e0);
			CHECK(p0.x == vals[0]);
			CHECK(p0.y == vals[1]);
			CHECK(p0.z == vals[2]);
			auto p1 = wld.get<PositionSoA>(e1);
			CHECK(p1.x == vals[0] * 10.f);
			CHECK(p1.y == vals[1] * 10.f);
			CHECK(p1.z == vals[2] * 10.f);
			auto p2 = wld.get<PositionSoA>(e2);
			CHECK(p2.x == vals[0] * 100.f);
			CHECK(p2.y == vals[1] * 100.f);
			CHECK(p2.z == vals[2] * 100.f);

			ecs::Query q = wld.query().all<PositionSoA>();
			uint32_t cnt = 0;
			q.each([&](const PositionSoA& pos) {
				if (cnt == 0) {
					CHECK(pos.x == vals[0]);
					CHECK(pos.y == vals[1]);
					CHECK(pos.z == vals[2]);
				} else if (cnt == 1) {
					CHECK(pos.x == vals[0] * 10.f);
					CHECK(pos.y == vals[1] * 10.f);
					CHECK(pos.z == vals[2] * 10.f);
				} else if (cnt == 2) {
					CHECK(pos.x == vals[0] * 100.f);
					CHECK(pos.y == vals[1] * 100.f);
					CHECK(pos.z == vals[2] * 100.f);
				}
				++cnt;
			});
			CHECK(cnt == 3);
		}
		{
			auto p0 = wld.get<Position>(e0);
			CHECK(p0.x == vals[0]);
			CHECK(p0.y == vals[1]);
			CHECK(p0.z == vals[2]);
			auto p1 = wld.get<Position>(e1);
			CHECK(p1.x == vals[0] * 10.f);
			CHECK(p1.y == vals[1] * 10.f);
			CHECK(p1.z == vals[2] * 10.f);
			auto p2 = wld.get<Position>(e2);
			CHECK(p2.x == vals[0] * 100.f);
			CHECK(p2.y == vals[1] * 100.f);
			CHECK(p2.z == vals[2] * 100.f);

			ecs::Query q = wld.query().all<Position>();
			uint32_t cnt = 0;
			q.each([&](const Position& pos) {
				if (cnt == 0) {
					CHECK(pos.x == vals[0]);
					CHECK(pos.y == vals[1]);
					CHECK(pos.z == vals[2]);
				} else if (cnt == 1) {
					CHECK(pos.x == vals[0] * 10.f);
					CHECK(pos.y == vals[1] * 10.f);
					CHECK(pos.z == vals[2] * 10.f);
				} else if (cnt == 2) {
					CHECK(pos.x == vals[0] * 100.f);
					CHECK(pos.y == vals[1] * 100.f);
					CHECK(pos.z == vals[2] * 100.f);
				}
				++cnt;
			});
			CHECK(cnt == 3);
		}

		wld.enable(e2, false);

		{
			auto p0 = wld.get<PositionSoA>(e0);
			CHECK(p0.x == vals[0]);
			CHECK(p0.y == vals[1]);
			CHECK(p0.z == vals[2]);
			auto p1 = wld.get<PositionSoA>(e1);
			CHECK(p1.x == vals[0] * 10.f);
			CHECK(p1.y == vals[1] * 10.f);
			CHECK(p1.z == vals[2] * 10.f);
			auto p2 = wld.get<PositionSoA>(e2);
			CHECK(p2.x == vals[0] * 100.f);
			CHECK(p2.y == vals[1] * 100.f);
			CHECK(p2.z == vals[2] * 100.f);

			ecs::Query q = wld.query().all<PositionSoA>();
			uint32_t cnt = 0;
			q.each([&](const PositionSoA& pos) {
				if (cnt == 0) {
					CHECK(pos.x == vals[0] * 10.f);
					CHECK(pos.y == vals[1] * 10.f);
					CHECK(pos.z == vals[2] * 10.f);
				} else if (cnt == 1) {
					CHECK(pos.x == vals[0]);
					CHECK(pos.y == vals[1]);
					CHECK(pos.z == vals[2]);
				}
				++cnt;
			});
			CHECK(cnt == 2);
		}
		{
			wld.enable(e2, false);

			auto p0 = wld.get<Position>(e0);
			CHECK(p0.x == vals[0]);
			CHECK(p0.y == vals[1]);
			CHECK(p0.z == vals[2]);
			auto p1 = wld.get<Position>(e1);
			CHECK(p1.x == vals[0] * 10.f);
			CHECK(p1.y == vals[1] * 10.f);
			CHECK(p1.z == vals[2] * 10.f);
			auto p2 = wld.get<Position>(e2);
			CHECK(p2.x == vals[0] * 100.f);
			CHECK(p2.y == vals[1] * 100.f);
			CHECK(p2.z == vals[2] * 100.f);

			ecs::Query q = wld.query().all<Position>();
			uint32_t cnt = 0;
			q.each([&](const Position& pos) {
				if (cnt == 0) {
					CHECK(pos.x == vals[0] * 10.f);
					CHECK(pos.y == vals[1] * 10.f);
					CHECK(pos.z == vals[2] * 10.f);
				} else if (cnt == 1) {
					CHECK(pos.x == vals[0]);
					CHECK(pos.y == vals[1]);
					CHECK(pos.z == vals[2]);
				}
				++cnt;
			});
			CHECK(cnt == 2);
		}
	}
}

TEST_CASE("Enable - moving the last enabled entity preserves the disabled partition") {
	TestWorld twld;
	const auto disabled = wld.add();
	const auto moved = wld.add();
	wld.add<Position>(disabled);
	wld.add<Position>(moved);
	wld.enable(disabled, false);

	// Move the only enabled row out of the shared archetype. The remaining row must stay in the disabled partition.
	wld.add<Acceleration>(moved);

	auto query = wld.query().all<Position>();
	CHECK(query.count(ecs::Constraints::AcceptAll) == 2);
	CHECK(query.count() == 1);
	CHECK(query.count(ecs::Constraints::DisabledOnly) == 1);
	CHECK_FALSE(wld.enabled(disabled));

	wld.enable(disabled, true);
	CHECK(wld.enabled(disabled));
	CHECK(query.count() == 2);
	CHECK(query.count(ecs::Constraints::DisabledOnly) == 0);
}

TEST_CASE("Add - generic") {
	{
		TestWorld twld;
		auto e = wld.add();

		auto f = wld.add();
		wld.add(e, f);
		CHECK(wld.has(e, f));
	}

	{
		TestWorld twld;
		auto e = wld.add();

		wld.add<Position>(e);
		wld.add<Acceleration>(e);

		CHECK(wld.has<Position>(e));
		CHECK(wld.has<Acceleration>(e));
		CHECK_FALSE(wld.has<ecs::uni<Position>>(e));
		CHECK_FALSE(wld.has<ecs::uni<Acceleration>>(e));

		auto f = wld.add();
		wld.add(e, f);
		CHECK(wld.has(e, f));

		CHECK(wld.has<Position>(e));
		CHECK(wld.has<Acceleration>(e));
		CHECK_FALSE(wld.has<ecs::uni<Position>>(e));
		CHECK_FALSE(wld.has<ecs::uni<Acceleration>>(e));
	}

	{
		TestWorld twld;
		auto e = wld.add();

		wld.add<Position>(e, {1, 2, 3});
		wld.add<Acceleration>(e, {4, 5, 6});

		CHECK(wld.has<Position>(e));
		CHECK(wld.has<Acceleration>(e));
		CHECK_FALSE(wld.has<ecs::uni<Position>>(e));
		CHECK_FALSE(wld.has<ecs::uni<Acceleration>>(e));

		auto p = wld.get<Position>(e);
		CHECK(p.x == 1.f);
		CHECK(p.y == 2.f);
		CHECK(p.z == 3.f);

		auto a = wld.get<Acceleration>(e);
		CHECK(a.x == 4.f);
		CHECK(a.y == 5.f);
		CHECK(a.z == 6.f);

		auto f = wld.add();
		wld.add(e, f);
		CHECK(wld.has(e, f));

		CHECK(wld.has<Position>(e));
		CHECK(wld.has<Acceleration>(e));
		CHECK_FALSE(wld.has<ecs::uni<Position>>(e));
		CHECK_FALSE(wld.has<ecs::uni<Acceleration>>(e));

		p = wld.get<Position>(e);
		CHECK(p.x == 1.f);
		CHECK(p.y == 2.f);
		CHECK(p.z == 3.f);

		a = wld.get<Acceleration>(e);
		CHECK(a.x == 4.f);
		CHECK(a.y == 5.f);
		CHECK(a.z == 6.f);
	}
}

// TEST_CASE("Add - from query") {
// 	TestWorld twld;
//
// 	cnt::sarray<ecs::Entity, 5> ents;
// 	for (auto& e: ents)
// 		e = wld.add();
//
// 	for (uint32_t i = 0; i < ents.size() - 1; ++i)
// 		wld.add<Position>(ents[i], {(float)i, (float)i, (float)i});
//
// 	ecs::Query q;
// 	q.all<Position>();
// 	wld.add<Acceleration>(q, {1.f, 2.f, 3.f});
//
// 	for (uint32_t i = 0; i < ents.size() - 1; ++i) {
// 		CHECK(wld.has<Position>(ents[i]));
// 		CHECK(wld.has<Acceleration>(ents[i]));
//
// 		Position p;
// 		wld.get<Position>(ents[i], p);
// 		CHECK(p.x == (float)i);
// 		CHECK(p.y == (float)i);
// 		CHECK(p.z == (float)i);
//
// 		Acceleration a;
// 		wld.get<Acceleration>(ents[i], a);
// 		CHECK(a.x == 1.f);
// 		CHECK(a.y == 2.f);
// 		CHECK(a.z == 3.f);
// 	}
//
// 	{
// 		CHECK_FALSE(wld.has<Position>(ents[4]));
// 		CHECK_FALSE(wld.has<Acceleration>(ents[4]));
// 	}
// }

TEST_CASE("Add - unique") {
	{
		TestWorld twld;
		auto e = wld.add();

		auto f = wld.add(ecs::EntityKind::EK_Uni);
		wld.add(e, f);
		CHECK(wld.has(e, f));
	}

	{
		TestWorld twld;
		auto e = wld.add();

		wld.add<ecs::uni<Position>>(e);
		wld.add<ecs::uni<Acceleration>>(e);

		CHECK_FALSE(wld.has<Position>(e));
		CHECK_FALSE(wld.has<Acceleration>(e));
		CHECK(wld.has<ecs::uni<Position>>(e));
		CHECK(wld.has<ecs::uni<Acceleration>>(e));
	}

	{
		TestWorld twld;
		auto e = wld.add();

		// Add Position unique component
		wld.add<ecs::uni<Position>>(e, {1, 2, 3});
		CHECK(wld.has<ecs::uni<Position>>(e));
		CHECK_FALSE(wld.has<Position>(e));
		{
			auto p = wld.get<ecs::uni<Position>>(e);
			CHECK(p.x == 1.f);
			CHECK(p.y == 2.f);
			CHECK(p.z == 3.f);
		}
		{
			auto setter = wld.acc_mut(e);
			auto& upos = setter.mut<ecs::uni<Position>>();
			upos = {10, 20, 30};

			auto p = wld.get<ecs::uni<Position>>(e);
			CHECK(p.x == 10.f);
			CHECK(p.y == 20.f);
			CHECK(p.z == 30.f);

			p = setter.get<ecs::uni<Position>>();
			CHECK(p.x == 10.f);
			CHECK(p.y == 20.f);
			CHECK(p.z == 30.f);
		}
		// Add Acceleration unique component.
		// This moves "e" to a new archetype.
		wld.add<ecs::uni<Acceleration>>(e, {4, 5, 6});
		CHECK(wld.has<ecs::uni<Position>>(e));
		CHECK(wld.has<ecs::uni<Acceleration>>(e));
		CHECK_FALSE(wld.has<Position>(e));
		CHECK_FALSE(wld.has<Acceleration>(e));
		{
			auto a = wld.get<ecs::uni<Acceleration>>(e);
			CHECK(a.x == 4.f);
			CHECK(a.y == 5.f);
			CHECK(a.z == 6.f);
		}
		{
			// Unique storage after archetype moves is unspecified, but it must stay writable.
			auto& p = wld.acc_mut(e).mut<ecs::uni<Position>>();
			p = {7, 8, 9};
			const auto stored = wld.get<ecs::uni<Position>>(e);
			CHECK(stored.x == 7.f);
			CHECK(stored.y == 8.f);
			CHECK(stored.z == 9.f);
		}
	}

	{
		TestWorld twld;
		auto e = wld.add();

		// Add Position unique component
		wld.add<ecs::uni<Position>>(e, {1, 2, 3});
		CHECK(wld.has<ecs::uni<Position>>(e));
		CHECK_FALSE(wld.has<Position>(e));
		{
			auto p = wld.get<ecs::uni<Position>>(e);
			CHECK(p.x == 1.f);
			CHECK(p.y == 2.f);
			CHECK(p.z == 3.f);
		}
		// Add Acceleration unique component.
		// This moves "e" to a new archetype.
		wld.add<ecs::uni<Acceleration>>(e, {4, 5, 6});
		CHECK(wld.has<ecs::uni<Position>>(e));
		CHECK(wld.has<ecs::uni<Acceleration>>(e));
		CHECK_FALSE(wld.has<Position>(e));
		CHECK_FALSE(wld.has<Acceleration>(e));
		{
			auto a = wld.get<ecs::uni<Acceleration>>(e);
			CHECK(a.x == 4.f);
			CHECK(a.y == 5.f);
			CHECK(a.z == 6.f);
		}
		{
			auto& p = wld.acc_mut(e).mut<ecs::uni<Position>>();
			p = {7, 8, 9};
			const auto stored = wld.get<ecs::uni<Position>>(e);
			CHECK(stored.x == 7.f);
			CHECK(stored.y == 8.f);
			CHECK(stored.z == 9.f);
		}

		// Add a generic entity. Archetype changes.
		auto f = wld.add();
		wld.add(e, f);
		CHECK(wld.has(e, f));

		CHECK(wld.has<ecs::uni<Position>>(e));
		CHECK(wld.has<ecs::uni<Acceleration>>(e));
		CHECK_FALSE(wld.has<Position>(e));
		CHECK_FALSE(wld.has<Acceleration>(e));
		{
			auto& a = wld.acc_mut(e).mut<ecs::uni<Acceleration>>();
			a = {40, 50, 60};
			const auto stored = wld.get<ecs::uni<Acceleration>>(e);
			CHECK(stored.x == 40.f);
			CHECK(stored.y == 50.f);
			CHECK(stored.z == 60.f);
		}
		{
			auto& p = wld.acc_mut(e).mut<ecs::uni<Position>>();
			p = {7, 8, 9};
			const auto stored = wld.get<ecs::uni<Position>>(e);
			CHECK(stored.x == 7.f);
			CHECK(stored.y == 8.f);
			CHECK(stored.z == 9.f);
		}
	}

	{
		TestWorld twld;
		auto e = wld.add();

		// Add Position unique component
		wld.add<ecs::uni<Position>>(e, {1, 2, 3});
		CHECK(wld.has<ecs::uni<Position>>(e));
		CHECK_FALSE(wld.has<Position>(e));
		{
			auto p = wld.get<ecs::uni<Position>>(e);
			CHECK(p.x == 1.f);
			CHECK(p.y == 2.f);
			CHECK(p.z == 3.f);
		}
		// Add Acceleration unique component.
		// This moves "e" to a new archetype.
		wld.add<ecs::uni<Acceleration>>(e, {4, 5, 6});
		CHECK(wld.has<ecs::uni<Position>>(e));
		CHECK(wld.has<ecs::uni<Acceleration>>(e));
		CHECK_FALSE(wld.has<Position>(e));
		CHECK_FALSE(wld.has<Acceleration>(e));
		{
			auto a = wld.get<ecs::uni<Acceleration>>(e);
			CHECK(a.x == 4.f);
			CHECK(a.y == 5.f);
			CHECK(a.z == 6.f);
		}
		// Because "e" was moved to a new archetype nobody ever set the Position value again.
		// The bytes are unspecified here, so only verify the value after writing it explicitly below.

		// Add a unique entity. Archetype changes.
		auto f = wld.add(ecs::EntityKind::EK_Uni);
		wld.add(e, f);
		CHECK(wld.has(e, f));

		CHECK(wld.has<ecs::uni<Position>>(e));
		CHECK(wld.has<ecs::uni<Acceleration>>(e));
		CHECK_FALSE(wld.has<Position>(e));
		CHECK_FALSE(wld.has<Acceleration>(e));
		{
			auto& a = wld.acc_mut(e).mut<ecs::uni<Acceleration>>();
			a = {40, 50, 60};
			const auto stored = wld.get<ecs::uni<Acceleration>>(e);
			CHECK(stored.x == 40.f);
			CHECK(stored.y == 50.f);
			CHECK(stored.z == 60.f);
		}
		{
			auto& p = wld.acc_mut(e).mut<ecs::uni<Position>>();
			p = {7, 8, 9};
			const auto stored = wld.get<ecs::uni<Position>>(e);
			CHECK(stored.x == 7.f);
			CHECK(stored.y == 8.f);
			CHECK(stored.z == 9.f);
		}
	}
}

TEST_CASE("Add - mixed") {
	{
		TestWorld twld;
		auto e = wld.add();

		auto f = wld.add(ecs::EntityKind::EK_Uni);
		wld.add(e, f);
		CHECK(wld.has(e, f));
	}

	{
		TestWorld twld;
		auto e = wld.add();

		wld.add<Position>(e);
		wld.add<ecs::uni<Position>>(e);

		CHECK(wld.has<Position>(e));
		CHECK(wld.has<ecs::uni<Position>>(e));
	}

	{
		TestWorld twld;
		auto e = wld.add();

		// Add Position unique component
		wld.add<Position>(e, {10, 20, 30});
		wld.add<ecs::uni<Position>>(e, {1, 2, 3});
		CHECK(wld.has<Position>(e));
		CHECK(wld.has<ecs::uni<Position>>(e));
		{
			auto p = wld.get<Position>(e);
			CHECK(p.x == 10.f);
			CHECK(p.y == 20.f);
			CHECK(p.z == 30.f);
		}
		{
			auto p = wld.get<ecs::uni<Position>>(e);
			CHECK(p.x == 1.f);
			CHECK(p.y == 2.f);
			CHECK(p.z == 3.f);
		}
		// Add Acceleration unique component.
		// This moves "e" to a new archetype.
		wld.add<ecs::uni<Acceleration>>(e, {4, 5, 6});
		CHECK(wld.has<Position>(e));
		CHECK(wld.has<ecs::uni<Position>>(e));
		CHECK(wld.has<ecs::uni<Acceleration>>(e));
		CHECK_FALSE(wld.has<Acceleration>(e));
		{
			auto a = wld.get<ecs::uni<Acceleration>>(e);
			CHECK(a.x == 4.f);
			CHECK(a.y == 5.f);
			CHECK(a.z == 6.f);
		}
		{
			// Position will remain the same
			auto p = wld.get<Position>(e);
			CHECK(p.x == 10.f);
			CHECK(p.y == 20.f);
			CHECK(p.z == 30.f);
		}
		{
			// Because "e" was moved to a new archetype nobody ever set the unique Position value again.
			// The bytes are unspecified here, so only verify the value after writing it explicitly below.
			// auto p = wld.get<ecs::uni<Position>>(e);
			// CHECK_FALSE(p.x == 1.f);
			// CHECK_FALSE(p.y == 2.f);
			// CHECK_FALSE(p.z == 3.f);
		}
		wld.set<ecs::uni<Position>>(e) = {100.0f, 200.0f, 300.0f};
		{
			auto p = wld.get<Position>(e);
			CHECK(p.x == 10.f);
			CHECK(p.y == 20.f);
			CHECK(p.z == 30.f);
		}
		{
			auto p = wld.get<ecs::uni<Position>>(e);
			CHECK(p.x == 100.f);
			CHECK(p.y == 200.f);
			CHECK(p.z == 300.f);
		}
	}
}

TEST_CASE("Singleton self id lifecycle and recycled slot reuse") {
	TestWorld twld;

	const auto singleton = wld.add();
	wld.add(singleton, singleton);
	CHECK(wld.has(singleton, singleton));

	auto qSingleton = wld.query().all(singleton);
	CHECK(qSingleton.count() == 1);
	expect_exact_entities(qSingleton, {singleton});

	wld.del(singleton, singleton);
	CHECK_FALSE(wld.has(singleton, singleton));
	CHECK(qSingleton.count() == 0);
	expect_exact_entities(qSingleton, {});

	wld.add(singleton, singleton);
	CHECK(wld.has(singleton, singleton));
	CHECK(qSingleton.count() == 1);
	expect_exact_entities(qSingleton, {singleton});

	wld.del(singleton);
	wld.update();

	const auto replacement = wld.add();
	CHECK_FALSE(wld.has(replacement, replacement));

	wld.add(replacement, replacement);
	CHECK(wld.has(replacement, replacement));

	auto qReplacement = wld.query().all(replacement);
	CHECK(qReplacement.count() == 1);
	expect_exact_entities(qReplacement, {replacement});
}

TEST_CASE("Del - generic") {
	{
		TestWorld twld;
		auto e1 = wld.add();

		{
			wld.add<Position>(e1);
			wld.del<Position>(e1);
			CHECK_FALSE(wld.has<Position>(e1));
		}
		{
			wld.add<Rotation>(e1);
			wld.del<Rotation>(e1);
			CHECK_FALSE(wld.has<Rotation>(e1));
		}
	}
	{
		TestWorld twld;
		auto e1 = wld.add();
		{
			wld.add<Position>(e1);
			wld.add<Rotation>(e1);

			{
				wld.del<Position>(e1);
				CHECK_FALSE(wld.has<Position>(e1));
				CHECK(wld.has<Rotation>(e1));
			}
			{
				wld.del<Rotation>(e1);
				CHECK_FALSE(wld.has<Position>(e1));
				CHECK_FALSE(wld.has<Rotation>(e1));
			}
		}
		{
			wld.add<Rotation>(e1);
			wld.add<Position>(e1);
			{
				wld.del<Position>(e1);
				CHECK_FALSE(wld.has<Position>(e1));
				CHECK(wld.has<Rotation>(e1));
			}
			{
				wld.del<Rotation>(e1);
				CHECK_FALSE(wld.has<Position>(e1));
				CHECK_FALSE(wld.has<Rotation>(e1));
			}
		}
	}
}

TEST_CASE("Del - unique") {
	TestWorld twld;
	auto e1 = wld.add();

	{
		wld.add<ecs::uni<Position>>(e1);
		wld.add<ecs::uni<Acceleration>>(e1);
		{
			wld.del<ecs::uni<Position>>(e1);
			CHECK_FALSE(wld.has<ecs::uni<Position>>(e1));
			CHECK(wld.has<ecs::uni<Acceleration>>(e1));
		}
		{
			wld.del<ecs::uni<Acceleration>>(e1);
			CHECK_FALSE(wld.has<ecs::uni<Position>>(e1));
			CHECK_FALSE(wld.has<ecs::uni<Acceleration>>(e1));
		}
	}

	{
		wld.add<ecs::uni<Acceleration>>(e1);
		wld.add<ecs::uni<Position>>(e1);
		{
			wld.del<ecs::uni<Position>>(e1);
			CHECK_FALSE(wld.has<ecs::uni<Position>>(e1));
			CHECK(wld.has<ecs::uni<Acceleration>>(e1));
		}
		{
			wld.del<ecs::uni<Acceleration>>(e1);
			CHECK_FALSE(wld.has<ecs::uni<Position>>(e1));
			CHECK_FALSE(wld.has<ecs::uni<Acceleration>>(e1));
		}
	}
}

TEST_CASE("Del - generic, unique") {
	TestWorld twld;
	auto e1 = wld.add();

	{
		wld.add<Position>(e1);
		wld.add<Acceleration>(e1);
		wld.add<ecs::uni<Position>>(e1);
		wld.add<ecs::uni<Acceleration>>(e1);
		{
			wld.del<Position>(e1);
			CHECK_FALSE(wld.has<Position>(e1));
			CHECK(wld.has<Acceleration>(e1));
			CHECK(wld.has<ecs::uni<Position>>(e1));
			CHECK(wld.has<ecs::uni<Acceleration>>(e1));
		}
		{
			wld.del<Acceleration>(e1);
			CHECK_FALSE(wld.has<Position>(e1));
			CHECK_FALSE(wld.has<Acceleration>(e1));
			CHECK(wld.has<ecs::uni<Position>>(e1));
			CHECK(wld.has<ecs::uni<Acceleration>>(e1));
		}
		{
			wld.del<ecs::uni<Acceleration>>(e1);
			CHECK_FALSE(wld.has<Position>(e1));
			CHECK_FALSE(wld.has<Acceleration>(e1));
			CHECK(wld.has<ecs::uni<Position>>(e1));
			CHECK_FALSE(wld.has<ecs::uni<Acceleration>>(e1));
		}
	}
}

TEST_CASE("Del - cleanup rules") {
	SUBCASE("default") {
		TestWorld twld;
		auto wolf = wld.add();
		auto rabbit = wld.add();
		auto carrot = wld.add();
		auto eats = wld.add();
		auto hungry = wld.add();
		wld.add(wolf, hungry);
		wld.add(wolf, {eats, rabbit});
		wld.add(rabbit, {eats, carrot});

		wld.del(wolf);
		CHECK_FALSE(wld.has(wolf));
		CHECK(wld.has(rabbit));
		CHECK(wld.has(eats));
		CHECK(wld.has(carrot));
		CHECK(wld.has(hungry));
		// global relationships
		CHECK(wld.has({eats, rabbit}));
		CHECK(wld.has({eats, carrot}));
		CHECK(wld.has({eats, ecs::All}));
		CHECK(wld.has({ecs::All, carrot}));
		CHECK(wld.has({ecs::All, rabbit}));
		CHECK(wld.has({ecs::All, ecs::All}));
		// rabbit relationships
		CHECK(wld.has(rabbit, {eats, carrot}));
		CHECK(wld.has(rabbit, {eats, ecs::All}));
		CHECK(wld.has(rabbit, {ecs::All, carrot}));
		CHECK(wld.has(rabbit, {ecs::All, ecs::All}));
	}
	SUBCASE("default, relationship source") {
		TestWorld twld;
		auto wolf = wld.add();
		auto rabbit = wld.add();
		auto carrot = wld.add();
		auto eats = wld.add();
		auto hungry = wld.add();
		wld.add(wolf, hungry);
		wld.add(wolf, {eats, rabbit});
		wld.add(rabbit, {eats, carrot});

		wld.del(eats);
		CHECK(wld.has(wolf));
		CHECK(wld.has(rabbit));
		CHECK_FALSE(wld.has(eats));
		CHECK(wld.has(carrot));
		CHECK(wld.has(hungry));
		CHECK_FALSE(wld.has(wolf, {eats, rabbit}));
		CHECK_FALSE(wld.has(rabbit, {eats, carrot}));
	}
	SUBCASE("(OnDelete,Remove)") {
		TestWorld twld;
		auto wolf = wld.add();
		auto rabbit = wld.add();
		auto carrot = wld.add();
		auto eats = wld.add();
		auto hungry = wld.add();
		wld.add(wolf, hungry);
		wld.add(wolf, {ecs::OnDelete, ecs::Remove});
		wld.add(wolf, {eats, rabbit});
		wld.add(rabbit, {eats, carrot});

		wld.del(wolf);
		CHECK_FALSE(wld.has(wolf));
		CHECK(wld.has(rabbit));
		CHECK(wld.has(eats));
		CHECK(wld.has(carrot));
		CHECK(wld.has(hungry));
		CHECK(wld.has({eats, rabbit}));
		CHECK(wld.has({eats, carrot}));
		CHECK(wld.has(rabbit, {eats, carrot}));
	}
	SUBCASE("(OnDelete,Delete)") {
		TestWorld twld;
		auto wolf = wld.add();
		auto rabbit = wld.add();
		auto carrot = wld.add();
		auto eats = wld.add();
		auto hungry = wld.add();
		wld.add(wolf, hungry);
		wld.add(hungry, {ecs::OnDelete, ecs::Delete});
		wld.add(wolf, {eats, rabbit});
		wld.add(rabbit, {eats, carrot});

		wld.del(hungry);
		CHECK_FALSE(wld.has(wolf));
		CHECK(wld.has(rabbit));
		CHECK(wld.has(eats));
		CHECK(wld.has(carrot));
		CHECK_FALSE(wld.has(hungry));
		CHECK(wld.has({eats, rabbit}));
		CHECK(wld.has({eats, carrot}));
	}
	SUBCASE("(OnDeleteTarget,Delete)") {
		TestWorld twld;
		auto parent = wld.add();
		auto child = wld.add();
		auto child_of = wld.add();
		wld.add(child_of, {ecs::OnDeleteTarget, ecs::Delete});
		wld.add(child, {child_of, parent});

		wld.del(parent);
		CHECK_FALSE(wld.has(child));
		CHECK_FALSE(wld.has(parent));
	}
	SUBCASE("exclusive OnDeleteTarget delete via Parent") {
		TestWorld twld;
		auto parent = wld.add();
		auto child = wld.add();
		wld.parent(child, parent);

		wld.del(parent);
		CHECK_FALSE(wld.has(child));
		CHECK_FALSE(wld.has(parent));
	}
}

TEST_CASE("Entity name - entity only") {
	constexpr uint32_t N = 1'500;
	NonUniqueNameOpsGuard guard;

	TestWorld twld;
	cnt::darr<ecs::Entity> ents;
	ents.reserve(N);

	constexpr auto MaxLen = 32;
	char tmp[MaxLen];

	auto create = [&]() {
		auto e = wld.add();
		GAIA_STRFMT(tmp, MaxLen, "name_%u", e.id());
		wld.name(e, tmp);
		ents.push_back(e);
	};
	auto create_raw = [&]() {
		auto e = wld.add();
		GAIA_STRFMT(tmp, MaxLen, "name_%u", e.id());
		wld.name_raw(e, tmp);
		ents.push_back(e);
	};
	auto verify = [&](uint32_t i) {
		auto e = ents[i];
		GAIA_STRFMT(tmp, MaxLen, "name_%u", e.id());
		const auto ename = wld.name(e);

		const auto l0 = (uint32_t)GAIA_STRLEN(tmp, ecs::ComponentCacheItem::MaxNameLength);
		const auto l1 = ename.size();
		CHECK(l0 == l1);
		CHECK(ename == util::str_view(tmp, l0));
	};

	SUBCASE("basic") {
		ents.clear();
		create();
		verify(0);
		auto e = ents[0];

		const util::str original(wld.name(e));

		// If we change the original string we still must have a match
		{
			GAIA_STRCPY(tmp, MaxLen, "some_random_string");
			CHECK(wld.name(e) == original.view());
			CHECK(wld.get(original.data(), original.size()) == e);
			CHECK(wld.get(tmp) == ecs::EntityBad);

			// Change the name back
			GAIA_ASSERT(original.size() < MaxLen);
			memcpy(tmp, original.data(), original.size());
			tmp[original.size()] = 0;
			verify(0);
		}

		// Add new entity and try settin the same name again.
		// The request will be ignored because names are unique.
		{
			auto e1 = wld.add();
			wld.name(e1, original.data(), original.size());
			CHECK(wld.name(e1).empty());
			CHECK(wld.get(original.data(), original.size()) == e);
		}

		wld.name(e, nullptr);
		CHECK(wld.get(original.data(), original.size()) == ecs::EntityBad);
		CHECK(wld.name(e).empty());

		wld.name(e, original.data(), original.size());
		wld.del(e);
		CHECK(wld.get(original.data(), original.size()) == ecs::EntityBad);
	}

	SUBCASE("basic - non-owned") {
		ents.clear();
		create_raw();
		verify(0);
		auto e = ents[0];

		const util::str original(wld.name(e));

		// If we rewrite the original storage without re-registering the name, lookup data becomes stale.
		{
			GAIA_STRCPY(tmp, MaxLen, "some_random_string");
			CHECK(wld.get(original.data(), original.size()) == ecs::EntityBad);
			// Hash was calculated for [original] but we changed the string to "some_random_string".
			// The registered length and hash won't match so we shouldn't be able to find the entity still.
			CHECK(wld.get("some_random_string") == ecs::EntityBad);
		}

		{
			// Change the name back
			GAIA_ASSERT(original.size() < MaxLen);
			memcpy(tmp, original.data(), original.size());
			tmp[original.size()] = 0;
			verify(0);
		}

		wld.name(e, nullptr);
		CHECK(wld.get(original.data(), original.size()) == ecs::EntityBad);
		CHECK(wld.name(e).empty());

		wld.name_raw(e, original.data(), original.size());
		wld.del(e);
		CHECK(wld.get(original.data(), original.size()) == ecs::EntityBad);
	}

	SUBCASE("two") {
		ents.clear();
		GAIA_FOR(2) create();
		GAIA_FOR(2) verify(i);
		wld.del(ents[0]);
		verify(1);
	}

	SUBCASE("many") {
		ents.clear();
		GAIA_FOR(N) create();
		GAIA_FOR(N) verify(i);
		wld.del(ents[900]);
		GAIA_FOR(900) verify(i);
		GAIA_FOR2(901, N) verify(i);

		{
			auto e = ents[1000];

			const util::str original(wld.name(e));

			{
				wld.enable(e, false);
				const auto str = wld.name(e);
				CHECK(str == original.view());
				CHECK(e == wld.get(original.data(), original.size()));
			}
			{
				wld.enable(e, true);
				const auto str = wld.name(e);
				CHECK(str == original.view());
				CHECK(e == wld.get(original.data(), original.size()));
			}
		}
	}

	SUBCASE("alias set replace and clear preserve entity name") {
		auto e = wld.add();
		wld.name(e, "entity_name");

		CHECK(wld.name(e) == "entity_name");
		CHECK(wld.get("entity_name") == e);

		CHECK(wld.alias(e, "entity_alias"));
		CHECK(wld.name(e) == "entity_name");
		CHECK(wld.get("entity_name") == e);
		CHECK(wld.alias(e) == "entity_alias");
		CHECK(wld.alias("entity_alias") == e);
		CHECK(wld.get("entity_alias") == e);

		CHECK(wld.alias(e, "entity_alias_2"));
		CHECK(wld.name(e) == "entity_name");
		CHECK(wld.get("entity_name") == e);
		CHECK(wld.alias(e) == "entity_alias_2");
		CHECK(wld.alias("entity_alias") == ecs::EntityBad);
		CHECK(wld.get("entity_alias") == ecs::EntityBad);
		CHECK(wld.alias("entity_alias_2") == e);
		CHECK(wld.get("entity_alias_2") == e);

		CHECK(wld.alias(e, nullptr));
		CHECK(wld.name(e) == "entity_name");
		CHECK(wld.get("entity_name") == e);
		CHECK(wld.alias(e).empty());
		CHECK(wld.alias("entity_alias_2") == ecs::EntityBad);
		CHECK(wld.get("entity_alias_2") == ecs::EntityBad);
	}

	SUBCASE("duplicate names are rejected") {
		const auto entityA = wld.add();
		const auto entityB = wld.add();

		wld.name(entityA, "entity_name");
		wld.name(entityB, "entity_name");
		CHECK(wld.name(entityA) == "entity_name");
		CHECK(wld.name(entityB).empty());
		CHECK(wld.get("entity_name") == entityA);
	}
}

TEST_CASE("Entity name - component") {
	constexpr uint32_t N = 1'500;

	TestWorld twld;
	cnt::darr<ecs::Entity> ents;
	ents.reserve(N);

	// Add component
	const auto& pci = wld.add<Position>();
	{
		// component entities participate in the normal entity naming path
		const auto name = wld.name(pci.entity);
		CHECK(name == "Position");
		CHECK(wld.symbol(pci.entity) == "Position");
		const auto e = wld.get("Position");
		CHECK(e == pci.entity);
	}
	// Add unique component
	const auto& upci = wld.add<ecs::uni<Position>>();
	{
		// component entities participate in the normal entity naming path
		const auto name = wld.name(upci.entity);
		CHECK(name == "gaia::ecs::uni<Position>");
		CHECK(wld.symbol(upci.entity) == "gaia::ecs::uni<Position>");
		const auto e = wld.get("gaia::ecs::uni<Position>");
		CHECK(e == upci.entity);
	}
	{
		// generic component symbol must still match
		const auto name = wld.name(pci.entity);
		CHECK(name == "Position");
		CHECK(wld.symbol(pci.entity) == "Position");
		const auto e = wld.get("Position");
		CHECK(e == pci.entity);
	}

	// Assign an entity name to the component entity
	wld.name(pci.entity, "xyz", 3);
	{
		// entity name must match
		const auto name = wld.name(pci.entity);
		CHECK(name == "xyz");
		CHECK(wld.symbol(pci.entity) == "Position");
		const auto e = wld.get("xyz");
		CHECK(e == pci.entity);
	}
	{
		// unique component symbol must still match
		const auto name = wld.name(upci.entity);
		CHECK(name == "gaia::ecs::uni<Position>");
		CHECK(wld.symbol(upci.entity) == "gaia::ecs::uni<Position>");
		const auto e = wld.get("gaia::ecs::uni<Position>");
		CHECK(e == upci.entity);
	}
}

TEST_CASE("Entity name - copy") {
	TestWorld twld;

	constexpr const char* pTestStr = "text";

	// An entity with some values. We won't be copying it.
	auto e0 = wld.add();
	wld.add<PositionNonTrivial>(e0, {10.f, 20.f, 30.f});

	// An entity we want to copy.
	auto e1 = wld.add();
	wld.add<PositionNonTrivial>(e1, {1.f, 2.f, 3.f});
	wld.name_raw(e1, pTestStr);

	// Expectations:
	// Names are unique, so the copied entity can't have the name set.

	SUBCASE("single entity") {
		auto e2 = wld.copy(e1);

		auto e = wld.get(pTestStr);
		CHECK(e == e1);

		const auto& p1 = wld.get<PositionNonTrivial>(e1);
		CHECK(p1.x == 1.f);
		CHECK(p1.y == 2.f);
		CHECK(p1.z == 3.f);
		const auto& p2 = wld.get<PositionNonTrivial>(e2);
		CHECK(p2.x == 1.f);
		CHECK(p2.y == 2.f);
		CHECK(p2.z == 3.f);

		const auto e1name = wld.name(e1);
		CHECK(e1name == util::str_view(pTestStr, (uint32_t)strlen(pTestStr)));
		const auto e2name = wld.name(e2);
		CHECK(e2name.empty());
	}

	SUBCASE("many entities") {
		constexpr uint32_t N = 1'500;

		SUBCASE("entity") {
			cnt::darr<ecs::Entity> ents;
			ents.reserve(N);
			wld.copy_n(e1, N, [&ents](ecs::Entity entity) {
				ents.push_back(entity);
			});

			auto e = wld.get(pTestStr);
			CHECK(e == e1);
			const auto e1name = wld.name(e1);
			CHECK(e1name == util::str_view(pTestStr, (uint32_t)strlen(pTestStr)));
			const auto& p1 = wld.get<PositionNonTrivial>(e1);
			CHECK(p1.x == 1.f);
			CHECK(p1.y == 2.f);
			CHECK(p1.z == 3.f);

			for (auto ent: ents) {
				const auto e2name = wld.name(ent);
				CHECK(e2name.empty());
				const auto& p2 = wld.get<PositionNonTrivial>(ent);
				CHECK(p2.x == 1.f);
				CHECK(p2.y == 2.f);
				CHECK(p2.z == 3.f);
			}
		}

		SUBCASE("iterator") {
			cnt::darr<ecs::Entity> ents;
			ents.reserve(N);
			uint32_t counter = 0;
			wld.copy_n(e1, N, [&](ecs::CopyIter& it) {
				GAIA_EACH(it) {
					auto ev = it.view<ecs::Entity>();
					auto pv = it.view<PositionNonTrivial>();

					const auto& p1 = pv[i];
					CHECK(p1.x == 1.f);
					CHECK(p1.y == 2.f);
					CHECK(p1.z == 3.f);

					ents.push_back(ev[i]);
					++counter;
				}
			});
			CHECK(counter == N);

			auto e = wld.get(pTestStr);
			CHECK(e == e1);
			const auto e1name = wld.name(e1);
			CHECK(e1name == util::str_view(pTestStr, (uint32_t)strlen(pTestStr)));
			const auto& p1 = wld.get<PositionNonTrivial>(e1);
			CHECK(p1.x == 1.f);
			CHECK(p1.y == 2.f);
			CHECK(p1.z == 3.f);

			for (auto ent: ents) {
				const auto e2name = wld.name(ent);
				CHECK(e2name.empty());
				const auto& p2 = wld.get<PositionNonTrivial>(ent);
				CHECK(p2.x == 1.f);
				CHECK(p2.y == 2.f);
				CHECK(p2.z == 3.f);
			}
		}
	}
}

TEST_CASE("Copy sparse component data") {
	TestWorld twld;

	const auto src = wld.add();
	wld.add<PositionSparse>(src, {1.0f, 2.0f, 3.0f});

	const auto dst = wld.copy(src);
	CHECK(wld.has<PositionSparse>(dst));
	{
		const auto& pos = wld.get<PositionSparse>(dst);
		CHECK(pos.x == doctest::Approx(1.0f));
		CHECK(pos.y == doctest::Approx(2.0f));
		CHECK(pos.z == doctest::Approx(3.0f));
	}

	cnt::darr<ecs::Entity> ents;
	wld.copy_n(src, 8, [&](ecs::Entity entity) {
		ents.push_back(entity);
	});
	CHECK(ents.size() == 8);
	for (const auto entity: ents) {
		CHECK(wld.has<PositionSparse>(entity));
		const auto& pos = wld.get<PositionSparse>(entity);
		CHECK(pos.x == doctest::Approx(1.0f));
		CHECK(pos.y == doctest::Approx(2.0f));
		CHECK(pos.z == doctest::Approx(3.0f));
	}
}

TEST_CASE("Copy_n with zero count does nothing") {
	TestWorld twld;

	const auto src = wld.add();
	wld.add<Position>(src, {1.0f, 2.0f, 3.0f});

	uint32_t entityHits = 0;
	wld.copy_n(src, 0, [&](ecs::Entity) {
		++entityHits;
	});
	CHECK(entityHits == 0);

	uint32_t iterHits = 0;
	wld.copy_n(src, 0, [&](ecs::CopyIter& it) {
		iterHits += it.size();
	});
	CHECK(iterHits == 0);

	auto q = wld.query().all<Position>();
	CHECK(q.count() == 1);
}

TEST_CASE("Entity name - hierarchy") {
	TestWorld twld;

	auto europe = wld.add();
	auto slovakia = wld.add();
	auto bratislava = wld.add();

	wld.child(slovakia, europe);
	wld.child(bratislava, slovakia);

	wld.name(europe, "europe");
	wld.name(slovakia, "slovakia");
	wld.name(bratislava, "bratislava");

	{
		auto e = wld.get("europe.slovakia");
		CHECK(e == slovakia);
	}
	{
		auto e = wld.get("europe.slovakia.bratislava");
		CHECK(e == bratislava);
	}
	{
		auto e = wld.get("slovakia.bratislava");
		CHECK(e == bratislava);
	}
	{
		auto e = wld.get("europe.bratislava.slovakia");
		CHECK(e == ecs::EntityBad);
	}
	{
		auto e = wld.get("bratislava.slovakia");
		CHECK(e == ecs::EntityBad);
	}
	{
		auto e = wld.get(".");
		CHECK(e == ecs::EntityBad);
	}
	{
		auto e = wld.get(".bratislava");
		CHECK(e == ecs::EntityBad);
	}
	{
		// We treat this case as "bratislava"
		auto e = wld.get("bratislava.");
		CHECK(e == bratislava);
	}
	{
		auto e = wld.get("..");
		CHECK(e == ecs::EntityBad);
	}
	{
		auto e = wld.get("..bratislava");
		CHECK(e == ecs::EntityBad);
	}
	{
		auto e = wld.get("bratislava..");
		CHECK(e == ecs::EntityBad);
	}
	{
		auto e = wld.get("slovakia..bratislava");
		CHECK(e == ecs::EntityBad);
	}
}
