#include "test_common.h"

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
				q.each([&]([[maybe_unused]] ecs::IterAll& it) {
					const uint32_t cExpected = it.size();
					uint32_t c = 0;
					GAIA_EACH(it)++ c;
					CHECK(c == cExpected);
					cnt += c;
				});
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
				q.each([&]([[maybe_unused]] ecs::IterDisabled& it) {
					const uint32_t cExpected = it.size();
					uint32_t c = 0;
					GAIA_EACH(it) {
						CHECK_FALSE(it.enabled(i));
						++c;
					}
					CHECK(c == cExpected);
					cnt += c;
				});
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
			// Because "e" was moved to a new archetype nobody ever set the value.
			// Therefore, it is garbage and won't match the original chunk.
			auto p = wld.get<ecs::uni<Position>>(e);
			CHECK_FALSE(p.x == 1.f);
			CHECK_FALSE(p.y == 2.f);
			CHECK_FALSE(p.z == 3.f);
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

TEST_CASE("Usage 1 - simple query, 1 unique component") {
	TestWorld twld;

	auto e = wld.add();
	wld.add<ecs::uni<Position>>(e);

	auto q = wld.query().all<ecs::uni<Position>>();
	auto qq = wld.query().all<Position>();

	{
		uint32_t cnt = 0;
		qq.each([&]([[maybe_unused]] const Position&) {
			++cnt;
		});
		CHECK(cnt == 0);
	}
	{
		uint32_t cnt = 0;
		qq.each([&]() {
			++cnt;
		});
		CHECK(cnt == 0);
	}
	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] ecs::Iter& it) {
			++cnt;
		});
		CHECK(cnt == 1);
	}

	auto e1 = wld.copy(e);
	auto e2 = wld.copy(e);
	auto e3 = wld.copy(e);

	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] ecs::Iter& it) {
			++cnt;
		});
		CHECK(cnt == 1);
	}

	wld.del(e1);

	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] ecs::Iter& it) {
			++cnt;
		});
		CHECK(cnt == 1);
	}

	wld.del(e2);
	wld.del(e3);
	wld.del(e);

	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] ecs::Iter& it) {
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

TEST_CASE("Usage 2 - simple query, many unique components") {
	TestWorld twld;

	auto e1 = wld.add();
	wld.add<ecs::uni<Position>>(e1, {});
	wld.add<ecs::uni<Acceleration>>(e1, {});
	wld.add<ecs::uni<Else>>(e1, {});
	auto e2 = wld.add();
	wld.add<ecs::uni<Rotation>>(e2, {});
	wld.add<ecs::uni<Scale>>(e2, {});
	wld.add<ecs::uni<Else>>(e2, {});
	auto e3 = wld.add();
	wld.add<ecs::uni<Position>>(e3, {});
	wld.add<ecs::uni<Acceleration>>(e3, {});
	wld.add<ecs::uni<Scale>>(e3, {});

	{
		uint32_t cnt = 0;
		auto q = wld.query().all<ecs::uni<Position>>();
		q.each([&]([[maybe_unused]] ecs::Iter& it) {
			++cnt;
		});
		CHECK(cnt == 2);
	}
	{
		uint32_t cnt = 0;
		auto q = wld.query().all<ecs::uni<Acceleration>>();
		q.each([&]([[maybe_unused]] ecs::Iter& it) {
			++cnt;
		});
		CHECK(cnt == 2);
	}
	{
		uint32_t cnt = 0;
		auto q = wld.query().all<ecs::uni<Rotation>>();
		q.each([&]([[maybe_unused]] ecs::Iter& it) {
			++cnt;
		});
		CHECK(cnt == 1);
	}
	{
		uint32_t cnt = 0;
		auto q = wld.query().all<ecs::uni<Else>>();
		q.each([&]([[maybe_unused]] ecs::Iter& it) {
			++cnt;
		});
		CHECK(cnt == 2);
	}
	{
		uint32_t cnt = 0;
		auto q = wld.query().all<ecs::uni<Scale>>();
		q.each([&]([[maybe_unused]] ecs::Iter& it) {
			++cnt;
		});
		CHECK(cnt == 2);
	}
	{
		auto q = wld.query().or_<ecs::uni<Position>>().or_<ecs::uni<Acceleration>>();

		uint32_t cnt = 0;
		q.each([&](ecs::Iter& it) {
			++cnt;

			const bool ok1 = it.has<ecs::uni<Position>>() || it.has<ecs::uni<Acceleration>>();
			CHECK(ok1);
			const bool ok2 = it.has<ecs::uni<Acceleration>>() || it.has<ecs::uni<Position>>();
			CHECK(ok2);
		});
		CHECK(cnt == 2);
	}
	{
		auto q = wld.query().or_<ecs::uni<Position>>().or_<ecs::uni<Acceleration>>().all<ecs::uni<Scale>>();

		uint32_t cnt = 0;
		q.each([&](ecs::Iter& it) {
			++cnt;

			CHECK(it.size() == 1);

			const bool ok1 = it.has<ecs::uni<Position>>() || it.has<ecs::uni<Acceleration>>();
			CHECK(ok1);
			const bool ok2 = it.has<ecs::uni<Acceleration>>() || it.has<ecs::uni<Position>>();
			CHECK(ok2);
		});
		CHECK(cnt == 1);
	}
	{
		auto q = wld.query().or_<ecs::uni<Position>>().or_<ecs::uni<Acceleration>>().no<ecs::uni<Scale>>();

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
	wld.add<Rotation>(e, {1, 0, 0});

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
	wld.add<Rotation>(eMatch, {7, 8, 9});

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

TEST_CASE("Prefab - instantiate creates a non-prefab instance with copied data") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto prefabAnimal = wld.prefab();
	wld.as(prefabAnimal, animal);
	wld.add<Position>(prefabAnimal, {7, 0, 0});

	int positionHits = 0;
	wld.observer().event(ecs::ObserverEvent::OnAdd).all<Position>().on_each([&](ecs::Iter&) {
		++positionHits;
	});

	const auto instance = wld.instantiate(prefabAnimal);

	CHECK_FALSE(wld.has_direct(instance, ecs::Prefab));
	CHECK(wld.has_direct(instance, ecs::Pair(ecs::Is, prefabAnimal)));
	CHECK_FALSE(wld.has_direct(instance, ecs::Pair(ecs::Is, animal)));
	CHECK(wld.has(instance, ecs::Pair(ecs::Is, animal)));
	CHECK(wld.get<Position>(instance).x == 7.0f);
	CHECK(positionHits == 1);

	auto qDefault = wld.query().is(prefabAnimal);
	CHECK(qDefault.count() == 1);
	expect_exact_entities(qDefault, {instance});

	auto qPrefab = wld.query().is(prefabAnimal).match_prefab();
	CHECK(qPrefab.count() == 2);
	expect_exact_entities(qPrefab, {prefabAnimal, instance});
}

TEST_CASE("Prefab - instantiate does not copy the prefab name") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	wld.name(prefabAnimal, "prefab_animal");

	const auto instance = wld.instantiate(prefabAnimal);

	CHECK(!wld.name(prefabAnimal).empty());
	CHECK(wld.name(prefabAnimal) == "prefab_animal");
	CHECK(wld.name(instance).empty());
}

TEST_CASE("Prefab - instantiate respects DontInherit policy") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(prefabAnimal, {7, 0, 0});
	wld.add<Scale>(prefabAnimal, {3, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::DontInherit));

	const auto instance = wld.instantiate(prefabAnimal);

	CHECK_FALSE(wld.has<Position>(instance));
	CHECK(wld.has<Scale>(instance));
	CHECK(wld.get<Scale>(instance).x == 3.0f);
	CHECK(wld.has_direct(instance, ecs::Pair(ecs::Is, prefabAnimal)));
}

TEST_CASE("Prefab - explicit Override policy still copies data") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(prefabAnimal, {9, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Override));

	const auto instance = wld.instantiate(prefabAnimal);

	CHECK(wld.has<Position>(instance));
	CHECK(wld.get<Position>(instance).x == 9.0f);
}

TEST_CASE("Prefab - explicit Override policy copies sparse data") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	const auto position = wld.add<PositionSparse>().entity;
	wld.add<PositionSparse>(prefabAnimal, {9, 1, 2});

	const auto instance = wld.instantiate(prefabAnimal);

	CHECK(wld.has_direct(instance, position));
	const auto pos = wld.get<PositionSparse>(instance);
	CHECK(pos.x == doctest::Approx(9.0f));
	CHECK(pos.y == doctest::Approx(1.0f));
	CHECK(pos.z == doctest::Approx(2.0f));

	const auto prefabPos = wld.get<PositionSparse>(prefabAnimal);
	CHECK(prefabPos.x == doctest::Approx(9.0f));
	CHECK(prefabPos.y == doctest::Approx(1.0f));
	CHECK(prefabPos.z == doctest::Approx(2.0f));
}

TEST_CASE("Prefab - Inherit policy before local override") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(prefabAnimal, {5, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto instance = wld.instantiate(prefabAnimal);

	CHECK_FALSE(wld.has_direct(instance, position));
	CHECK(wld.has<Position>(instance));
	CHECK(wld.get<Position>(instance).x == 5.0f);

	wld.add<Position>(instance, {8, 0, 0});
	CHECK(wld.has_direct(instance, position));
	CHECK(wld.get<Position>(instance).x == 8.0f);
}

TEST_CASE("Prefab - explicit override and inherited ownership") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(prefabAnimal, {5, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto instance = wld.instantiate(prefabAnimal);

	CHECK_FALSE(wld.has_direct(instance, position));
	CHECK(wld.override<Position>(instance));
	CHECK(wld.has_direct(instance, position));
	CHECK(wld.get<Position>(instance).x == doctest::Approx(5.0f));
	CHECK_FALSE(wld.override<Position>(instance));
	CHECK(wld.get<Position>(prefabAnimal).x == doctest::Approx(5.0f));
}

TEST_CASE("Prefab - explicit override and inherited tags") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	const auto tag = wld.add();
	wld.add(prefabAnimal, tag);
	wld.add(tag, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto instance = wld.instantiate(prefabAnimal);

	CHECK_FALSE(wld.has_direct(instance, tag));
	CHECK(wld.has(instance, tag));
	CHECK(wld.override(instance, tag));
	CHECK(wld.has_direct(instance, tag));
	CHECK_FALSE(wld.override(instance, tag));
}

TEST_CASE("Is inheritance - explicit override and inherited ownership") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto rabbit = wld.add();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(animal, {5, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
	wld.as(rabbit, animal);

	CHECK_FALSE(wld.has_direct(rabbit, position));
	CHECK(wld.has<Position>(rabbit));
	CHECK(wld.get<Position>(rabbit).x == doctest::Approx(5.0f));
	CHECK(wld.override<Position>(rabbit));
	CHECK(wld.has_direct(rabbit, position));
	CHECK(wld.get<Position>(rabbit).x == doctest::Approx(5.0f));
	CHECK_FALSE(wld.override<Position>(rabbit));
	CHECK(wld.get<Position>(animal).x == doctest::Approx(5.0f));
}

TEST_CASE("Is inheritance - explicit override and inherited tags") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto rabbit = wld.add();
	const auto tag = wld.add();
	wld.add(animal, tag);
	wld.add(tag, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
	wld.as(rabbit, animal);

	CHECK_FALSE(wld.has_direct(rabbit, tag));
	CHECK(wld.has(rabbit, tag));
	CHECK(wld.override(rabbit, tag));
	CHECK(wld.has_direct(rabbit, tag));
	CHECK_FALSE(wld.override(rabbit, tag));
}

TEST_CASE("Prefab - inherited component query writes local overrides") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(prefabAnimal, {5, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto instance = wld.instantiate(prefabAnimal);

	auto qRead = wld.query().all<Position>();
	CHECK(qRead.count() == 1);
	expect_exact_entities(qRead, {instance});

	float xRead = 0.0f;
	qRead.each([&](const Position& pos) {
		xRead += pos.x;
	});
	CHECK(xRead == doctest::Approx(5.0f));

	auto qWrite = wld.query().all<Position&>();
	qWrite.each([&](Position& pos) {
		pos.x += 3.0f;
	});

	CHECK(wld.has_direct(instance, position));
	CHECK(wld.get<Position>(instance).x == doctest::Approx(8.0f));
	CHECK(wld.get<Position>(prefabAnimal).x == doctest::Approx(5.0f));
}

TEST_CASE("Is inheritance - inherited component queries see derived entities and create local overrides on write") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto rabbit = wld.add();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(animal, {5, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
	wld.as(rabbit, animal);

	auto qRead = wld.query().all<Position>();
	CHECK(qRead.count() == 2);
	expect_exact_entities(qRead, {animal, rabbit});

	float xRead = 0.0f;
	qRead.each([&](const Position& pos) {
		xRead += pos.x;
	});
	CHECK(xRead == doctest::Approx(10.0f));

	auto qWrite = wld.query().all<Position&>();
	qWrite.each([&](Position& pos) {
		pos.x += 3.0f;
	});

	CHECK(wld.has_direct(rabbit, position));
	CHECK(wld.get<Position>(animal).x == doctest::Approx(8.0f));
	CHECK(wld.get<Position>(rabbit).x == doctest::Approx(11.0f));
}

TEST_CASE("Is inheritance - explicit override and query membership") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto rabbit = wld.add();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(animal, {5, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
	wld.as(rabbit, animal);

	auto q = wld.query().all<Position>();
	CHECK(q.count() == 2);
	expect_exact_entities(q, {animal, rabbit});

	CHECK(wld.override<Position>(rabbit));
	CHECK(q.count() == 2);
	expect_exact_entities(q, {animal, rabbit});
}

TEST_CASE("Prefab - inherited sparse component query writes local overrides") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	const auto position = wld.add<PositionSparse>().entity;
	wld.add<PositionSparse>(prefabAnimal, {5, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto instance = wld.instantiate(prefabAnimal);

	auto qRead = wld.query().all<PositionSparse>();
	CHECK(qRead.count() == 1);
	expect_exact_entities(qRead, {instance});

	float xRead = 0.0f;
	qRead.each([&](const PositionSparse& pos) {
		xRead += pos.x;
	});
	CHECK(xRead == doctest::Approx(5.0f));

	auto qWrite = wld.query().all<PositionSparse&>();
	qWrite.each([&](PositionSparse& pos) {
		pos.x += 3.0f;
	});

	CHECK(wld.has_direct(instance, position));
	CHECK(wld.get<PositionSparse>(instance).x == doctest::Approx(8.0f));
	CHECK(wld.get<PositionSparse>(prefabAnimal).x == doctest::Approx(5.0f));
}

TEST_CASE("Prefab - inherited writable query over a stable snapshot") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(prefabAnimal, {5, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto instanceA = wld.instantiate(prefabAnimal);
	const auto instanceB = wld.instantiate(prefabAnimal);

	uint32_t hits = 0;
	auto qWrite = wld.query().all<Position&>();
	qWrite.each([&](Position& pos) {
		++hits;
		pos.x += 2.0f;
	});

	CHECK(hits == 2);
	CHECK(wld.has_direct(instanceA, position));
	CHECK(wld.has_direct(instanceB, position));
	CHECK(wld.get<Position>(instanceA).x == doctest::Approx(7.0f));
	CHECK(wld.get<Position>(instanceB).x == doctest::Approx(7.0f));
	CHECK(wld.get<Position>(prefabAnimal).x == doctest::Approx(5.0f));
}

TEST_CASE("Prefab - inherited Iter query writes local overrides") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(prefabAnimal, {5, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto instanceA = wld.instantiate(prefabAnimal);
	const auto instanceB = wld.instantiate(prefabAnimal);

	auto qRead = wld.query().all<Position>();
	float xRead = 0.0f;
	uint32_t rowsRead = 0;
	qRead.each([&](ecs::Iter& it) {
		auto posView = it.view_any<Position>(1);
		GAIA_EACH(it) {
			xRead += posView[i].x;
			++rowsRead;
		}
	});
	CHECK(rowsRead == 2);
	CHECK(xRead == doctest::Approx(10.0f));

	auto qWrite = wld.query().all<Position&>();
	qWrite.each([&](ecs::Iter& it) {
		auto posView = it.view_mut_any<Position>(1);
		GAIA_EACH(it) {
			posView[i].x += 4.0f;
		}
	});

	CHECK(wld.has_direct(instanceA, position));
	CHECK(wld.has_direct(instanceB, position));
	CHECK(wld.get<Position>(instanceA).x == doctest::Approx(9.0f));
	CHECK(wld.get<Position>(instanceB).x == doctest::Approx(9.0f));
	CHECK(wld.get<Position>(prefabAnimal).x == doctest::Approx(5.0f));
}

TEST_CASE("Prefab - inherited Iter SoA query writes local overrides") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	const auto position = wld.add<PositionSoA>().entity;
	wld.add<PositionSoA>(prefabAnimal, {5, 6, 7});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto instance = wld.instantiate(prefabAnimal);

	auto qRead = wld.query().all<PositionSoA>();
	float xRead = 0.0f;
	float yRead = 0.0f;
	qRead.each([&](ecs::Iter& it) {
		auto posView = it.view_any<PositionSoA>(1);
		auto xs = posView.template get<0>();
		auto ys = posView.template get<1>();
		GAIA_EACH(it) {
			xRead += xs[i];
			yRead += ys[i];
		}
	});
	CHECK(xRead == doctest::Approx(5.0f));
	CHECK(yRead == doctest::Approx(6.0f));

	auto qWrite = wld.query().all<PositionSoA&>();
	qWrite.each([&](ecs::Iter& it) {
		auto posView = it.view_mut_any<PositionSoA>(1);
		auto xs = posView.template set<0>();
		auto ys = posView.template set<1>();
		GAIA_EACH(it) {
			xs[i] = xs[i] + 3.0f;
			ys[i] = ys[i] + 4.0f;
		}
	});

	CHECK(wld.has_direct(instance, position));
	const auto pos = wld.get<PositionSoA>(instance);
	CHECK(pos.x == doctest::Approx(8.0f));
	CHECK(pos.y == doctest::Approx(10.0f));
	CHECK(pos.z == doctest::Approx(7.0f));

	const auto prefabPos = wld.get<PositionSoA>(prefabAnimal);
	CHECK(prefabPos.x == doctest::Approx(5.0f));
	CHECK(prefabPos.y == doctest::Approx(6.0f));
	CHECK(prefabPos.z == doctest::Approx(7.0f));
}

TEST_CASE("Prefab - explicit override and inherited SoA components") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	const auto position = wld.add<PositionSoA>().entity;
	wld.add<PositionSoA>(prefabAnimal, {5, 6, 7});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto instance = wld.instantiate(prefabAnimal);

	CHECK_FALSE(wld.has_direct(instance, position));
	CHECK(wld.override<PositionSoA>(instance));
	CHECK(wld.has_direct(instance, position));

	const auto pos = wld.get<PositionSoA>(instance);
	CHECK(pos.x == doctest::Approx(5.0f));
	CHECK(pos.y == doctest::Approx(6.0f));
	CHECK(pos.z == doctest::Approx(7.0f));
	CHECK_FALSE(wld.override<PositionSoA>(instance));
}

TEST_CASE("Prefab - explicit override and inherited sparse components") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	const auto position = wld.add<PositionSparse>().entity;
	wld.add<PositionSparse>(prefabAnimal, {5, 6, 7});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto instance = wld.instantiate(prefabAnimal);

	CHECK_FALSE(wld.has_direct(instance, position));
	CHECK(wld.override<PositionSparse>(instance));
	CHECK(wld.has_direct(instance, position));

	const auto pos = wld.get<PositionSparse>(instance);
	CHECK(pos.x == doctest::Approx(5.0f));
	CHECK(pos.y == doctest::Approx(6.0f));
	CHECK(pos.z == doctest::Approx(7.0f));
	CHECK_FALSE(wld.override<PositionSparse>(instance));
}

TEST_CASE("Prefab - explicit override by id and inherited runtime sparse components") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	const auto& runtimeComp = wld.add(
			"Runtime_Sparse_Prefab_Position", (uint32_t)sizeof(Position), ecs::DataStorageType::Sparse,
			(uint32_t)alignof(Position));
	wld.add(runtimeComp.entity, ecs::DontFragment);
	wld.add(prefabAnimal, runtimeComp.entity, Position{2, 3, 4});
	wld.add(runtimeComp.entity, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto instance = wld.instantiate(prefabAnimal);

	CHECK_FALSE(wld.has_direct(instance, runtimeComp.entity));
	CHECK(wld.has(instance, runtimeComp.entity));
	CHECK(wld.override(instance, runtimeComp.entity));
	CHECK(wld.has_direct(instance, runtimeComp.entity));
	CHECK_FALSE(wld.override(instance, runtimeComp.entity));

	const auto& pos = wld.get<Position>(instance, runtimeComp.entity);
	CHECK(pos.x == doctest::Approx(2.0f));
	CHECK(pos.y == doctest::Approx(3.0f));
	CHECK(pos.z == doctest::Approx(4.0f));
}

TEST_CASE("Prefab - instantiate recurses Parent-owned prefab children") {
	TestWorld twld;

	const auto rootPrefab = wld.prefab();
	const auto childPrefab = wld.prefab();
	const auto leafPrefab = wld.prefab();

	wld.parent(childPrefab, rootPrefab);
	wld.parent(leafPrefab, childPrefab);

	wld.add<Position>(rootPrefab, {1, 0, 0});
	wld.add<Position>(childPrefab, {2, 0, 0});
	wld.add<Position>(leafPrefab, {3, 0, 0});

	const auto rootInstance = wld.instantiate(rootPrefab);

	auto q = wld.query().all<Position>().match_prefab();
	cnt::darray<ecs::Entity> entities;
	q.arr(entities);

	ecs::Entity childInstance = ecs::EntityBad;
	ecs::Entity leafInstance = ecs::EntityBad;
	for (const auto entity: entities) {
		if (entity == rootPrefab || entity == childPrefab || entity == leafPrefab || entity == rootInstance)
			continue;
		if (wld.has_direct(entity, ecs::Pair(ecs::Is, childPrefab)))
			childInstance = entity;
		else if (wld.has_direct(entity, ecs::Pair(ecs::Is, leafPrefab)))
			leafInstance = entity;
	}

	CHECK(childInstance != ecs::EntityBad);
	CHECK(leafInstance != ecs::EntityBad);

	CHECK(wld.has_direct(rootInstance, ecs::Pair(ecs::Is, rootPrefab)));
	CHECK(wld.has_direct(childInstance, ecs::Pair(ecs::Is, childPrefab)));
	CHECK(wld.has_direct(leafInstance, ecs::Pair(ecs::Is, leafPrefab)));

	CHECK(wld.has(childInstance, ecs::Pair(ecs::Parent, rootInstance)));
	CHECK(wld.has(leafInstance, ecs::Pair(ecs::Parent, childInstance)));

	CHECK_FALSE(wld.has_direct(rootInstance, ecs::Prefab));
	CHECK_FALSE(wld.has_direct(childInstance, ecs::Prefab));
	CHECK_FALSE(wld.has_direct(leafInstance, ecs::Prefab));

	CHECK(wld.get<Position>(rootInstance).x == 1.0f);
	CHECK(wld.get<Position>(childInstance).x == 2.0f);
	CHECK(wld.get<Position>(leafInstance).x == 3.0f);
}

TEST_CASE("Prefab - instantiate can parent the spawned subtree under an existing entity") {
	TestWorld twld;

	const auto scene = wld.add();
	const auto rootPrefab = wld.prefab();
	const auto childPrefab = wld.prefab();
	const auto leafPrefab = wld.prefab();

	wld.parent(childPrefab, rootPrefab);
	wld.parent(leafPrefab, childPrefab);

	wld.add<Position>(rootPrefab, {1, 0, 0});
	wld.add<Position>(childPrefab, {2, 0, 0});
	wld.add<Position>(leafPrefab, {3, 0, 0});

	const auto rootInstance = wld.instantiate(rootPrefab, scene);

	auto q = wld.query().all<Position>().match_prefab();
	cnt::darray<ecs::Entity> entities;
	q.arr(entities);

	ecs::Entity childInstance = ecs::EntityBad;
	ecs::Entity leafInstance = ecs::EntityBad;
	for (const auto entity: entities) {
		if (entity == rootPrefab || entity == childPrefab || entity == leafPrefab || entity == rootInstance)
			continue;
		if (wld.has_direct(entity, ecs::Pair(ecs::Is, childPrefab)))
			childInstance = entity;
		else if (wld.has_direct(entity, ecs::Pair(ecs::Is, leafPrefab)))
			leafInstance = entity;
	}

	CHECK(rootInstance != ecs::EntityBad);
	CHECK(childInstance != ecs::EntityBad);
	CHECK(leafInstance != ecs::EntityBad);

	CHECK(wld.has(rootInstance, ecs::Pair(ecs::Parent, scene)));
	CHECK(wld.has(childInstance, ecs::Pair(ecs::Parent, rootInstance)));
	CHECK(wld.has(leafInstance, ecs::Pair(ecs::Parent, childInstance)));
}

TEST_CASE("Instantiate - non-prefab parented fallback as copy plus parent") {
	TestWorld twld;

	const auto scene = wld.add();
	const auto animal = wld.add();
	wld.name(animal, "animal");
	wld.add<Position>(animal, {4, 5, 6});

	const auto instance = wld.instantiate(animal, scene);

	CHECK(instance != animal);
	CHECK(wld.has(instance, ecs::Pair(ecs::Parent, scene)));
	CHECK_FALSE(wld.has_direct(instance, ecs::Prefab));
	CHECK_FALSE(wld.has_direct(instance, ecs::Pair(ecs::Is, animal)));
	CHECK(wld.name(instance).empty());
	CHECK(wld.get<Position>(instance).x == doctest::Approx(4.0f));
	CHECK(wld.get<Position>(instance).y == doctest::Approx(5.0f));
	CHECK(wld.get<Position>(instance).z == doctest::Approx(6.0f));
}

TEST_CASE("Prefab - instantiate_n spawns multiple prefab instances") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	wld.add<Position>(prefabAnimal, {1, 2, 3});

	cnt::darray<ecs::Entity> roots;
	wld.instantiate_n(prefabAnimal, 4, [&](ecs::Entity instance) {
		roots.push_back(instance);
	});

	CHECK(roots.size() == 4);
	for (const auto instance: roots) {
		CHECK_FALSE(wld.has_direct(instance, ecs::Prefab));
		CHECK(wld.has_direct(instance, ecs::Pair(ecs::Is, prefabAnimal)));
		CHECK(wld.get<Position>(instance).x == doctest::Approx(1.0f));
		CHECK(wld.get<Position>(instance).y == doctest::Approx(2.0f));
		CHECK(wld.get<Position>(instance).z == doctest::Approx(3.0f));
	}
}

TEST_CASE("Prefab - instantiate_n does not copy prefab names") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	wld.name(prefabAnimal, "prefab_animal_bulk");

	cnt::darray<ecs::Entity> roots;
	wld.instantiate_n(prefabAnimal, 5, [&](ecs::Entity instance) {
		roots.push_back(instance);
	});

	CHECK(roots.size() == 5);
	CHECK(!wld.name(prefabAnimal).empty());
	CHECK(wld.name(prefabAnimal) == "prefab_animal_bulk");
	for (const auto instance: roots)
		CHECK(wld.name(instance).empty());
}

TEST_CASE("Instantiate_n - non-prefab parented fallback with CopyIter callbacks") {
	TestWorld twld;

	const auto scene = wld.add();
	const auto animal = wld.add();
	wld.add<Position>(animal, {1, 2, 3});

	uint32_t hits = 0;
	uint32_t seen = 0;
	cnt::darray<ecs::Entity> roots;
	roots.reserve(6);
	wld.instantiate_n(animal, scene, 6, [&](ecs::CopyIter& it) {
		++hits;
		seen += it.size();

		auto entityView = it.view<ecs::Entity>();
		auto posView = it.view<Position>();
		GAIA_EACH(it) {
			roots.push_back(entityView[i]);
			CHECK(wld.has(entityView[i], ecs::Pair(ecs::Parent, scene)));
			CHECK(posView[i].x == doctest::Approx(1.0f));
			CHECK(posView[i].y == doctest::Approx(2.0f));
			CHECK(posView[i].z == doctest::Approx(3.0f));
		}
	});

	CHECK(hits >= 1);
	CHECK(seen == 6);
	CHECK(roots.size() == 6);
	for (const auto instance: roots) {
		CHECK_FALSE(wld.has_direct(instance, ecs::Prefab));
		CHECK_FALSE(wld.has_direct(instance, ecs::Pair(ecs::Is, animal)));
		CHECK(wld.has(instance, ecs::Pair(ecs::Parent, scene)));
	}
}

TEST_CASE("Instantiate_n - non-prefab parented fallback with entity callbacks after parenting") {
	TestWorld twld;

	const auto scene = wld.add();
	const auto animal = wld.add();
	wld.add<Position>(animal, {7, 8, 9});

	uint32_t seen = 0;
	cnt::darray<ecs::Entity> roots;
	roots.reserve(5);
	wld.instantiate_n(animal, scene, 5, [&](ecs::Entity instance) {
		++seen;
		roots.push_back(instance);
		CHECK(wld.has(instance, ecs::Pair(ecs::Parent, scene)));
		const auto& pos = wld.get<Position>(instance);
		CHECK(pos.x == doctest::Approx(7.0f));
		CHECK(pos.y == doctest::Approx(8.0f));
		CHECK(pos.z == doctest::Approx(9.0f));
	});

	CHECK(seen == 5);
	CHECK(roots.size() == 5);
	for (const auto instance: roots)
		CHECK(wld.has(instance, ecs::Pair(ecs::Parent, scene)));
}

TEST_CASE("Observer - instantiate_n non-prefab parented fallback and Parent pair") {
	TestWorld twld;

	const auto scene = wld.add();
	const auto animal = wld.add();
	wld.add<Position>(animal, {7, 8, 9});

	uint32_t hits = 0;
	uint32_t seen = 0;
	auto obs =
			wld.observer().event(ecs::ObserverEvent::OnAdd).all(ecs::Pair(ecs::Parent, scene)).on_each([&](ecs::Iter& it) {
				++hits;
				seen += it.size();

				auto entityView = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					CHECK(wld.has(entityView[i], ecs::Pair(ecs::Parent, scene)));
				}
			});
	(void)obs;

	wld.instantiate_n(animal, scene, 5, [&](ecs::Entity instance) {
		CHECK(wld.has(instance, ecs::Pair(ecs::Parent, scene)));
	});

	CHECK(hits >= 1);
	CHECK(seen == 5);
}

TEST_CASE("Observer - parented prefab instantiate matches Parent pair") {
	TestWorld twld;

	const auto scene = wld.add();
	const auto prefab = wld.prefab();
	wld.add<Position>(prefab, {1, 2, 3});

	uint32_t hits = 0;
	uint32_t seen = 0;
	auto obs = wld.observer() //
								 .event(ecs::ObserverEvent::OnAdd) //
								 .all(ecs::Pair(ecs::Parent, scene)) //
								 .on_each([&](ecs::Iter& it) {
									 ++hits;
									 seen += it.size();

									 auto entityView = it.view<ecs::Entity>();
									 GAIA_EACH(it) {
										 CHECK(wld.has(entityView[i], ecs::Pair(ecs::Parent, scene)));
									 }
								 });
	(void)obs;

	const auto instance = wld.instantiate(prefab, scene);
	CHECK(wld.has(instance, ecs::Pair(ecs::Parent, scene)));
	CHECK(hits == 1);
	CHECK(seen == 1);
}

TEST_CASE("Prefab - instantiate_n CopyIter callbacks for spawned roots") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	wld.add<Position>(prefabAnimal, {1, 2, 3});

	uint32_t hits = 0;
	uint32_t seen = 0;
	cnt::darray<ecs::Entity> roots;
	roots.reserve(8);
	wld.instantiate_n(prefabAnimal, 8, [&](ecs::CopyIter& it) {
		++hits;
		seen += it.size();

		auto entityView = it.view<ecs::Entity>();
		auto posView = it.view<Position>();
		GAIA_EACH(it) {
			roots.push_back(entityView[i]);
			CHECK(posView[i].x == doctest::Approx(1.0f));
			CHECK(posView[i].y == doctest::Approx(2.0f));
			CHECK(posView[i].z == doctest::Approx(3.0f));
		}
	});

	CHECK(hits >= 1);
	CHECK(seen == 8);
	CHECK(roots.size() == 8);
	for (const auto instance: roots) {
		CHECK_FALSE(wld.has_direct(instance, ecs::Prefab));
		CHECK(wld.has_direct(instance, ecs::Pair(ecs::Is, prefabAnimal)));
	}
}

TEST_CASE("Prefab - instantiate_n inherited component queries and local overrides") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(prefabAnimal, {5, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	cnt::darray<ecs::Entity> instances;
	wld.instantiate_n(prefabAnimal, 4, [&](ecs::Entity instance) {
		instances.push_back(instance);
	});

	CHECK(instances.size() == 4);

	uint32_t hits = 0;
	wld.query().all<Position&>().each([&](Position& pos) {
		++hits;
		pos.x += 2.0f;
	});

	CHECK(hits == 4);
	for (const auto instance: instances) {
		CHECK(wld.has_direct(instance, position));
		CHECK(wld.get<Position>(instance).x == doctest::Approx(7.0f));
	}
	CHECK(wld.get<Position>(prefabAnimal).x == doctest::Approx(5.0f));
}

TEST_CASE("Prefab - instantiate_n parented roots support CopyIter callbacks") {
	TestWorld twld;

	const auto scene = wld.add();
	const auto rootPrefab = wld.prefab();
	const auto childPrefab = wld.prefab();

	wld.parent(childPrefab, rootPrefab);
	wld.add<Position>(rootPrefab, {1, 0, 0});
	wld.add<Position>(childPrefab, {2, 0, 0});

	uint32_t rootCount = 0;
	cnt::darray<ecs::Entity> roots;
	roots.reserve(4);
	wld.instantiate_n(rootPrefab, scene, 4, [&](ecs::CopyIter& it) {
		rootCount += it.size();
		auto entityView = it.view<ecs::Entity>();
		auto posView = it.view<Position>();
		GAIA_EACH(it) {
			roots.push_back(entityView[i]);
			CHECK(posView[i].x == doctest::Approx(1.0f));
			CHECK(wld.has(entityView[i], ecs::Pair(ecs::Parent, scene)));
		}
	});

	CHECK(rootCount == 4);
	CHECK(roots.size() == 4);

	uint32_t childCount = 0;
	for (const auto instance: roots) {
		wld.sources(ecs::Parent, instance, [&](ecs::Entity child) {
			++childCount;
			CHECK(wld.has_direct(child, ecs::Pair(ecs::Is, childPrefab)));
			CHECK(wld.has(child, ecs::Pair(ecs::Parent, instance)));
		});
	}
	CHECK(childCount == 4);
}

TEST_CASE("Prefab - instantiate_n can parent each spawned subtree") {
	TestWorld twld;

	const auto scene = wld.add();
	const auto rootPrefab = wld.prefab();
	const auto childPrefab = wld.prefab();

	wld.parent(childPrefab, rootPrefab);
	wld.add<Position>(rootPrefab, {1, 0, 0});
	wld.add<Position>(childPrefab, {2, 0, 0});

	cnt::darray<ecs::Entity> rootInstances;
	wld.instantiate_n(rootPrefab, scene, 3, [&](ecs::Entity instance) {
		rootInstances.push_back(instance);
	});

	CHECK(rootInstances.size() == 3);

	uint32_t childCount = 0;
	for (const auto instance: rootInstances) {
		CHECK(wld.has(instance, ecs::Pair(ecs::Parent, scene)));
		wld.sources(ecs::Parent, instance, [&](ecs::Entity child) {
			++childCount;
			CHECK(wld.has_direct(child, ecs::Pair(ecs::Is, childPrefab)));
			CHECK(wld.has(child, ecs::Pair(ecs::Parent, instance)));
			CHECK(wld.get<Position>(child).x == doctest::Approx(2.0f));
		});
	}

	CHECK(childCount == 3);
}

TEST_CASE("Prefab - instantiate_n recurses nested prefab children") {
	TestWorld twld;

	const auto rootPrefab = wld.prefab();
	const auto childPrefab = wld.prefab();
	const auto leafPrefab = wld.prefab();

	wld.parent(childPrefab, rootPrefab);
	wld.parent(leafPrefab, childPrefab);

	wld.add<Position>(rootPrefab, {1, 0, 0});
	wld.add<Position>(childPrefab, {2, 0, 0});
	wld.add<Position>(leafPrefab, {3, 0, 0});

	cnt::darray<ecs::Entity> roots;
	wld.instantiate_n(rootPrefab, 2, [&](ecs::Entity instance) {
		roots.push_back(instance);
	});

	CHECK(roots.size() == 2);

	uint32_t childCount = 0;
	uint32_t leafCount = 0;
	for (const auto rootInstance: roots) {
		wld.sources(ecs::Parent, rootInstance, [&](ecs::Entity child) {
			++childCount;
			CHECK(wld.has_direct(child, ecs::Pair(ecs::Is, childPrefab)));
			CHECK(wld.get<Position>(child).x == doctest::Approx(2.0f));

			wld.sources(ecs::Parent, child, [&](ecs::Entity leaf) {
				++leafCount;
				CHECK(wld.has_direct(leaf, ecs::Pair(ecs::Is, leafPrefab)));
				CHECK(wld.get<Position>(leaf).x == doctest::Approx(3.0f));
			});
		});
	}

	CHECK(childCount == 2);
	CHECK(leafCount == 2);
}

TEST_CASE("Prefab - instantiate_n with zero count does nothing") {
	TestWorld twld;

	const auto scene = wld.add();
	const auto prefabAnimal = wld.prefab();
	wld.add<Position>(prefabAnimal, {1.0f, 2.0f, 3.0f});

	uint32_t entityHits = 0;
	wld.instantiate_n(prefabAnimal, 0, [&](ecs::Entity) {
		++entityHits;
	});
	CHECK(entityHits == 0);

	uint32_t iterHits = 0;
	wld.instantiate_n(prefabAnimal, scene, 0, [&](ecs::CopyIter& it) {
		iterHits += it.size();
	});
	CHECK(iterHits == 0);

	auto q = wld.query().all<Position>();
	CHECK(q.count() == 0);
}

TEST_CASE("Prefab - sync adds missing copied data to existing instances") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto instance = wld.instantiate(prefab);

	auto prefabBuilder = wld.build(prefab);
	prefabBuilder.add<Position>();
	prefabBuilder.commit();
	wld.set<Position>(prefab) = {1.0f, 2.0f, 3.0f};

	CHECK_FALSE(wld.has<Position>(instance));

	const auto changes = wld.sync(prefab);
	CHECK(changes == 1);
	CHECK(wld.has_direct(instance, wld.add<Position>().entity));
	CHECK(wld.get<Position>(instance).x == doctest::Approx(1.0f));
	CHECK(wld.get<Position>(instance).y == doctest::Approx(2.0f));
	CHECK(wld.get<Position>(instance).z == doctest::Approx(3.0f));
}

TEST_CASE("Prefab - sync spawns missing prefab children on existing instances") {
	TestWorld twld;

	const auto rootPrefab = wld.prefab();
	const auto rootInstance = wld.instantiate(rootPrefab);
	const auto childPrefab = wld.prefab();

	auto childBuilder = wld.build(childPrefab);
	childBuilder.add<Position>();
	childBuilder.add(ecs::Pair(ecs::Parent, rootPrefab));
	childBuilder.commit();
	wld.set<Position>(childPrefab) = {2.0f, 0.0f, 0.0f};

	CHECK_FALSE(wld.has(rootInstance, ecs::Pair(ecs::Parent, rootPrefab)));

	const auto changes = wld.sync(rootPrefab);
	CHECK(changes == 1);

	cnt::darray<ecs::Entity> childInstances;
	wld.sources(ecs::Parent, rootInstance, [&](ecs::Entity child) {
		if (wld.has_direct(child, ecs::Pair(ecs::Is, childPrefab)))
			childInstances.push_back(child);
	});

	CHECK(childInstances.size() == 1);
	CHECK(wld.get<Position>(childInstances[0]).x == doctest::Approx(2.0f));

	const auto changesAgain = wld.sync(rootPrefab);
	CHECK(changesAgain == 0);
}

TEST_CASE("Prefab - sync recurses into existing child instances") {
	TestWorld twld;

	const auto rootPrefab = wld.prefab();
	const auto childPrefab = wld.prefab();
	{
		auto childBuilder = wld.build(childPrefab);
		childBuilder.add(ecs::Pair(ecs::Parent, rootPrefab));
		childBuilder.commit();
	}

	const auto rootInstance = wld.instantiate(rootPrefab);

	ecs::Entity childInstance = ecs::EntityBad;
	wld.sources(ecs::Parent, rootInstance, [&](ecs::Entity child) {
		if (wld.has_direct(child, ecs::Pair(ecs::Is, childPrefab)))
			childInstance = child;
	});
	CHECK(childInstance != ecs::EntityBad);
	if (childInstance == ecs::EntityBad)
		return;

	{
		auto childBuilder = wld.build(childPrefab);
		childBuilder.add<Scale>();
		childBuilder.commit();
	}
	wld.set<Scale>(childPrefab) = {1.0f, 0.5f, 0.25f};

	CHECK_FALSE(wld.has<Scale>(childInstance));

	const auto changes = wld.sync(rootPrefab);
	CHECK(changes == 1);
	CHECK(wld.has_direct(childInstance, wld.add<Scale>().entity));
	CHECK(wld.get<Scale>(childInstance).x == doctest::Approx(1.0f));
	CHECK(wld.get<Scale>(childInstance).y == doctest::Approx(0.5f));
	CHECK(wld.get<Scale>(childInstance).z == doctest::Approx(0.25f));
}

TEST_CASE("Prefab - removing inherited prefab data on existing instances") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
	wld.add<Position>(prefab, {1.0f, 2.0f, 3.0f});

	const auto instance = wld.instantiate(prefab);
	CHECK(wld.has<Position>(instance));

	wld.del<Position>(prefab);

	CHECK_FALSE(wld.has<Position>(instance));
	CHECK_FALSE(wld.has_direct(instance, position));
	CHECK(wld.sync(prefab) == 0);
	CHECK_FALSE(wld.has<Position>(instance));
}

TEST_CASE("Prefab - sync does not delete copied override data removed from prefab") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	wld.add<Position>(prefab, {4.0f, 5.0f, 6.0f});

	const auto instance = wld.instantiate(prefab);
	CHECK(wld.has_direct(instance, wld.add<Position>().entity));

	wld.del<Position>(prefab);

	CHECK(wld.has<Position>(instance));
	CHECK(wld.sync(prefab) == 0);
	CHECK(wld.has<Position>(instance));
	CHECK(wld.get<Position>(instance).x == doctest::Approx(4.0f));
	CHECK(wld.get<Position>(instance).y == doctest::Approx(5.0f));
	CHECK(wld.get<Position>(instance).z == doctest::Approx(6.0f));
}

TEST_CASE("Prefab - sync does not delete existing child instances when prefab child is removed") {
	TestWorld twld;

	const auto rootPrefab = wld.prefab();
	const auto childPrefab = wld.prefab();
	wld.parent(childPrefab, rootPrefab);
	wld.add<Position>(childPrefab, {2.0f, 0.0f, 0.0f});

	const auto rootInstance = wld.instantiate(rootPrefab);

	cnt::darray<ecs::Entity> childInstances;
	wld.sources(ecs::Parent, rootInstance, [&](ecs::Entity child) {
		if (wld.has_direct(child, ecs::Pair(ecs::Is, childPrefab)))
			childInstances.push_back(child);
	});

	CHECK(childInstances.size() == 1);
	if (childInstances.size() != 1)
		return;
	const auto childInstance = childInstances[0];

	wld.del(childPrefab, ecs::Pair(ecs::Parent, rootPrefab));

	CHECK(wld.sync(rootPrefab) == 0);
	CHECK(wld.has(childInstance, ecs::Pair(ecs::Parent, rootInstance)));
	CHECK(wld.has_direct(childInstance, ecs::Pair(ecs::Is, childPrefab)));
	CHECK(wld.get<Position>(childInstance).x == doctest::Approx(2.0f));
}

TEST_CASE("Prefab - cached exact Is query ignores recycled prefab ids after delete") {
	TestWorld twld;

	const auto recycle_same_id = [&](ecs::Entity stale) {
		ecs::Entity recycled = ecs::EntityBad;
		for (uint32_t i = 0; i < 256 && recycled == ecs::EntityBad; ++i) {
			const auto candidate = wld.add();
			if (candidate.id() == stale.id())
				recycled = candidate;
		}
		return recycled;
	};

	const auto prefab = wld.prefab();
	const auto oldInstance = wld.instantiate(prefab);

	ecs::QueryTermOptions directOpts;
	directOpts.direct();

	auto q = wld.query().all(ecs::Pair(ecs::Is, prefab), directOpts);
	auto& info = q.fetch();
	q.match_all(info);
	CHECK(q.count() == 1);
	expect_exact_entities(q, {oldInstance});

	wld.del(prefab);
	twld.update();

	q.match_all(info);
	CHECK(q.count() == 0);

	const auto recycled = recycle_same_id(prefab);
	CHECK(recycled != ecs::EntityBad);
	if (recycled == ecs::EntityBad)
		return;
	CHECK(recycled.gen() != prefab.gen());

	wld.add(recycled, ecs::Prefab);

	q.match_all(info);
	CHECK(q.count() == 0);

	auto qFresh = wld.query().all(ecs::Pair(ecs::Is, recycled), directOpts);
	CHECK(qFresh.count() == 0);

	const auto newInstance = wld.instantiate(recycled);

	q.match_all(info);
	CHECK(q.count() == 1);
	expect_exact_entities(q, {newInstance});

	CHECK(qFresh.count() == 1);
	expect_exact_entities(qFresh, {newInstance});
	CHECK_FALSE(wld.has_direct(oldInstance, ecs::Pair(ecs::Is, recycled)));
}

TEST_CASE("Prefab - inherited component query ignores recycled prefab ids after delete") {
	TestWorld twld;

	const auto recycle_same_id = [&](ecs::Entity stale) {
		ecs::Entity recycled = ecs::EntityBad;
		for (uint32_t i = 0; i < 256 && recycled == ecs::EntityBad; ++i) {
			const auto candidate = wld.add();
			if (candidate.id() == stale.id())
				recycled = candidate;
		}
		return recycled;
	};

	const auto position = wld.add<Position>().entity;
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto prefab = wld.prefab();
	wld.add<Position>(prefab, {1.0f, 2.0f, 3.0f});

	const auto oldInstance = wld.instantiate(prefab);

	auto q = wld.query().all<Position>();
	auto& info = q.fetch();
	q.match_all(info);
	CHECK(q.count() == 1);
	expect_exact_entities(q, {oldInstance});

	wld.del(prefab);
	twld.update();

	q.match_all(info);
	CHECK(q.count() == 0);
	CHECK_FALSE(wld.has<Position>(oldInstance));

	const auto recycled = recycle_same_id(prefab);
	CHECK(recycled != ecs::EntityBad);
	if (recycled == ecs::EntityBad)
		return;
	CHECK(recycled.gen() != prefab.gen());

	wld.add(recycled, ecs::Prefab);
	wld.add<Position>(recycled, {7.0f, 8.0f, 9.0f});

	q.match_all(info);
	CHECK(q.count() == 0);
	CHECK_FALSE(wld.has<Position>(oldInstance));

	const auto newInstance = wld.instantiate(recycled);

	q.match_all(info);
	CHECK(q.count() == 1);
	expect_exact_entities(q, {newInstance});
	CHECK_FALSE(wld.has<Position>(oldInstance));
	CHECK(wld.get<Position>(newInstance).x == doctest::Approx(7.0f));
	CHECK(wld.get<Position>(newInstance).y == doctest::Approx(8.0f));
	CHECK(wld.get<Position>(newInstance).z == doctest::Approx(9.0f));
}

TEST_CASE("Prefab - instantiate with non-prefab Parent children excluded") {
	TestWorld twld;

	const auto rootPrefab = wld.prefab();
	const auto plainChild = wld.add();

	wld.parent(plainChild, rootPrefab);
	wld.add<Position>(rootPrefab, {1, 0, 0});
	wld.add<Position>(plainChild, {2, 0, 0});

	const auto rootInstance = wld.instantiate(rootPrefab);

	uint32_t childCount = 0;
	wld.sources(ecs::Parent, rootInstance, [&](ecs::Entity) {
		++childCount;
	});

	CHECK(childCount == 0);
	CHECK(wld.get<Position>(rootInstance).x == doctest::Approx(1.0f));
}

TEST_CASE("Prefab - child instantiation respects DontInherit policy") {
	TestWorld twld;

	const auto rootPrefab = wld.prefab();
	const auto childPrefab = wld.prefab();
	const auto position = wld.add<Position>().entity;

	wld.parent(childPrefab, rootPrefab);
	wld.add<Scale>(rootPrefab, {1, 0, 0});
	wld.add<Position>(childPrefab, {2, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::DontInherit));

	const auto rootInstance = wld.instantiate(rootPrefab);

	ecs::Entity childInstance = ecs::EntityBad;
	wld.sources(ecs::Parent, rootInstance, [&](ecs::Entity entity) {
		childInstance = entity;
	});

	CHECK(childInstance != ecs::EntityBad);
	CHECK_FALSE(wld.has<Position>(childInstance));
	CHECK(wld.has_direct(childInstance, ecs::Pair(ecs::Is, childPrefab)));
}

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
	const auto revBefore = info.reverse_index_revision();

	auto e = wld.add();
	wld.add<Position>(e, {1, 2, 3});
	CHECK(info.reverse_index_revision() == revBefore);

	wld.add<Acceleration>(e, {4, 5, 6});
	const auto revAfterMembershipChange = info.reverse_index_revision();
	CHECK(revAfterMembershipChange != revBefore);
	CHECK(info.cache_archetype_view().size() == 1);

	info.invalidate_result();
	q.match_all(info);
	CHECK(info.reverse_index_revision() == revAfterMembershipChange);
	CHECK(info.cache_archetype_view().size() == 1);
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
	const auto revA = info.reverse_index_revision();
	CHECK(info.cache_archetype_view().size() == 1);
	CHECK(q.count() == 1);

	q.match_all(info);
	CHECK(info.reverse_index_revision() == revA);
	CHECK(q.count() == 1);

	q.set_var(ecs::Var0, tgtB);
	auto& infoB = q.fetch();
	CHECK(&infoB == &info);
	q.match_all(infoB);
	const auto revB = info.reverse_index_revision();
	CHECK(revB != revA);
	CHECK(info.cache_archetype_view().size() == 1);
	CHECK(q.count() == 1);

	q.match_all(info);
	CHECK(info.reverse_index_revision() == revB);
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
	const auto revEmpty = info.reverse_index_revision();
	CHECK(q.count() == 0);

	q.match_all(info);
	CHECK(info.reverse_index_revision() == revEmpty);
	CHECK(q.count() == 0);

	wld.add<Acceleration>(source, {1, 2, 3});
	q.match_all(info);
	const auto revFilled = info.reverse_index_revision();
	CHECK(revFilled != revEmpty);
	CHECK(q.count() == 1);
	CHECK(info.reverse_index_revision() == revFilled);

	wld.del<Acceleration>(source);
	q.match_all(info);
	CHECK(info.reverse_index_revision() != revFilled);
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
	const auto revA = info.reverse_index_revision();
	CHECK(q.count() == 1);

	auto unrelated = wld.add();
	wld.add<Rotation>(unrelated, {4, 5, 6});
	wld.add<Scale>(unrelated, {7, 8, 9});

	q.match_all(info);
	CHECK(info.reverse_index_revision() == revA);
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
	const auto revA = info.reverse_index_revision();
	CHECK(q.count() == 1);

	wld.del(source);

	q.match_all(info);
	CHECK(info.reverse_index_revision() != revA);
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
	const auto revA = info.reverse_index_revision();
	CHECK(q.count() == 1);

	wld.del(source);
	wld.update();

	const auto recycled = wld.add();
	CHECK(recycled.id() == source.id());
	CHECK(recycled.gen() != source.gen());
	wld.add<Acceleration>(recycled, {4, 5, 6});

	q.match_all(info);
	CHECK(info.reverse_index_revision() != revA);
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
	const auto revA = info.reverse_index_revision();
	CHECK(q.count() == 1);

	wld.add<Blocked>(source);
	q.match_all(info);
	const auto revB = info.reverse_index_revision();
	CHECK(revB != revA);
	CHECK(q.count() == 0);

	wld.del<Blocked>(source);
	q.match_all(info);
	CHECK(info.reverse_index_revision() != revB);
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
	const auto revA = info.reverse_index_revision();
	CHECK(q.count() == 0);

	wld.add<Acceleration>(source, {4, 5, 6});
	q.match_all(info);
	const auto revB = info.reverse_index_revision();
	CHECK(revB != revA);
	CHECK(q.count() == 1);

	wld.del<Acceleration>(source);
	q.match_all(info);
	CHECK(info.reverse_index_revision() != revB);
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
	const auto revEmpty = info.reverse_index_revision();
	CHECK(q.count() == 0);

	q.match_all(info);
	CHECK(info.reverse_index_revision() == revEmpty);
	CHECK(q.count() == 0);

	wld.add<Acceleration>(parent, {1, 2, 3});
	q.match_all(info);
	const auto revParent = info.reverse_index_revision();
	CHECK(revParent != revEmpty);
	CHECK(q.count() == 1);

	q.match_all(info);
	CHECK(info.reverse_index_revision() == revParent);
	CHECK(q.count() == 1);

	wld.del<Acceleration>(parent);
	wld.add<Acceleration>(root, {4, 5, 6});
	q.match_all(info);
	CHECK(info.reverse_index_revision() != revParent);
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
	const auto revA = info.reverse_index_revision();
	CHECK(q.count() == 1);

	auto unrelated = wld.add();
	wld.add<Rotation>(unrelated, {4, 5, 6});
	wld.add<Scale>(unrelated, {7, 8, 9});

	q.match_all(info);
	CHECK(info.reverse_index_revision() == revA);
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
	const auto revA = info.reverse_index_revision();
	CHECK(q.count() == 1);

	q.match_all(info);
	const auto revB = info.reverse_index_revision();
	CHECK(revB != revA);
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
	CHECK(qDefault.cache_policy() == ecs::QueryCachePolicy::Immediate);

	CHECK(qDefaultSrcTrav.kind() == ecs::QueryCacheKind::Default);
	CHECK(qDefaultSrcTrav.valid());
	CHECK(qDefaultSrcTrav.cache_src_trav() > 0);

	CHECK(qAuto.kind() == ecs::QueryCacheKind::Auto);
	CHECK(qAuto.valid());
	CHECK(qAuto.cache_policy() == ecs::QueryCachePolicy::Lazy);

	CHECK(qAutoDirectSrc.kind() == ecs::QueryCacheKind::Auto);
	CHECK(qAutoDirectSrc.valid());
	CHECK(qAutoDirectSrc.cache_policy() == ecs::QueryCachePolicy::Dynamic);
	CHECK(qAutoDirectSrc.cache_src_trav() == 0);

	CHECK(qAutoTraversedSrc.kind() == ecs::QueryCacheKind::Auto);
	CHECK(qAutoTraversedSrc.valid());
	CHECK(qAutoTraversedSrc.cache_policy() == ecs::QueryCachePolicy::Dynamic);
	CHECK(qAutoTraversedSrc.cache_src_trav() == 0);

	CHECK(qAutoSrcTrav.kind() == ecs::QueryCacheKind::Auto);
	CHECK(!qAutoSrcTrav.valid());
	CHECK(qAutoSrcTrav.count() == 0);

	CHECK(qNone.kind() == ecs::QueryCacheKind::None);
	CHECK(qNone.valid());
	CHECK(qNone.cache_policy() == ecs::QueryCachePolicy::Immediate);

	CHECK(qAll.kind() == ecs::QueryCacheKind::All);
	CHECK(qAll.valid());
	CHECK(qAll.cache_policy() == ecs::QueryCachePolicy::Immediate);

	CHECK(qAllFail.kind() == ecs::QueryCacheKind::All);
	CHECK(!qAllFail.valid());
	CHECK(qAllFail.count() == 0);

	CHECK(qAllDynamic.kind() == ecs::QueryCacheKind::All);
	CHECK(!qAllDynamic.valid());
	CHECK(qAllDynamic.count() == 0);

	CHECK(qAllSrcTrav.kind() == ecs::QueryCacheKind::All);
	CHECK(!qAllSrcTrav.valid());
	CHECK(qAllSrcTrav.count() == 0);

	CHECK(qDynamic.valid());
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

	const auto buildQuery = [&] {
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

	cnt::darr<ecs::Entity> ents;
	q.each([&](ecs::Entity e) {
		ents.push_back(e);
	});

	CHECK(ents.size() == 2);
	CHECK(ents[0] == leafB);
	CHECK(ents[1] == leafA);

	wld.del(parentA, ecs::Pair(ecs::Parent, mid));

	ents.clear();
	q.each([&](ecs::Entity e) {
		ents.push_back(e);
	});

	CHECK(ents.size() == 2);
	CHECK(ents[0] == leafA);
	CHECK(ents[1] == leafB);
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

	(void)wld.add<Position>();
	const auto game = wld.add();

	const auto* pRootArchetype = wld.fetch(game).pArchetype;
	CHECK(pRootArchetype != nullptr);
	if (pRootArchetype == nullptr)
		return;
	CHECK(ecs::world_component_index_comp_idx(wld, *pRootArchetype, wld.get<Position>()) == BadIndex);
	CHECK(ecs::world_component_index_match_count(wld, *pRootArchetype, wld.get<Position>()) == 0);

	wld.add<Position>(game, {1, 2, 3});
	const auto* pValueArchetype = wld.fetch(game).pArchetype;
	CHECK(pValueArchetype != nullptr);
	if (pValueArchetype == nullptr)
		return;
	CHECK(ecs::world_component_index_comp_idx(wld, *pValueArchetype, wld.get<Position>()) != BadIndex);
	CHECK(ecs::world_component_index_match_count(wld, *pValueArchetype, wld.get<Position>()) == 1);

	wld.del<Position>(game);
	const auto* pRootAgain = wld.fetch(game).pArchetype;
	CHECK(pRootAgain != nullptr);
	if (pRootAgain == nullptr)
		return;
	CHECK(ecs::world_component_index_comp_idx(wld, *pRootAgain, wld.get<Position>()) == BadIndex);
	CHECK(ecs::world_component_index_match_count(wld, *pRootAgain, wld.get<Position>()) == 0);
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

TEST_CASE("Query - remove decrements ALL/OR/NOT cached cursors") {
	TestWorld twld;

	// Build a cached query and force it to populate archetype cache.
	auto q = wld.query().no<Rotation>();
	(void)q.count();

	auto& info = q.fetch();
	CHECK(info.begin() != info.end());
	if (info.begin() == info.end())
		return;

	// QueryInfo::remove early-outs if the archetype is not in cache.
	auto* pArchetype = const_cast<ecs::Archetype*>(*info.begin());

	// Seed all cursor maps with the same test key.
	auto keyEntity = wld.add();
	auto key = ecs::EntityLookupKey(keyEntity);
	info.ctx().data.lastMatchedArchetypeIdx_All[key] = 5;
	info.ctx().data.lastMatchedArchetypeIdx_Or[key] = 5;
	info.ctx().data.lastMatchedArchetypeIdx_Not[key] = 5;

	info.remove(pArchetype);

	CHECK(info.ctx().data.lastMatchedArchetypeIdx_All[key] == 4);
	CHECK(info.ctx().data.lastMatchedArchetypeIdx_Or[key] == 4);
	CHECK(info.ctx().data.lastMatchedArchetypeIdx_Not[key] == 4);
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
