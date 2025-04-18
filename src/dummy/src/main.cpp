#include <gaia.h>

using namespace gaia;

struct Int3 {
	uint32_t x, y, z;
};
struct Position {
	float x, y, z;
};
struct PositionSoA {
	GAIA_LAYOUT(SoA);
	float x, y, z;
};
struct Velocity {
	float x, y, z;
};
struct Healthy {};
struct Rotation {
	float x, y, z;
};
struct Scale {
	float x, y, z;
};
struct Something {
	bool value;
};
struct Data {
	Position p;
	Scale s;
};
struct Acceleration {
	float x, y, z;
};
struct Else {
	bool value;
};
struct Empty {};

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

void CreateEntities(ecs::World& w) {
	auto e = w.add();
	w.add<Position>(e, {0, 100, 0});
	w.add<Velocity>(e, {1, 0, 0});

	constexpr uint32_t N = 10;
	GAIA_FOR(N) {
		[[maybe_unused]] auto newEntity = w.copy(e);
	}
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

struct test_0 {
	cnt::darray<int8_t*> arr;

	test_0() {
		arr.reserve(2);

		int8_t dummy = 10;
		arr.push_back(&dummy);
		arr.push_back(&dummy);
		arr.push_back(&dummy);
		arr.push_back(&dummy);
	}

	void getters() const {
		const auto& valR = arr[0];
		::gaia::dont_optimize(valR);
		auto* valC = arr[0];
		(void)valC;
	}

	void setters() {
		const auto& valR = arr[0];
		::gaia::dont_optimize(valR);
		auto* valC = arr[0];
		(void)valC;
		arr[0] = nullptr;
	}

	void diag() {
		for (auto* it: arr)
			printf("%p\n", it);
	}
} g_test_0;

struct test_00 {
	cnt::darray<int8_t> arr;

	test_00() {
		arr.reserve(2);

		int8_t dummy = 10;
		arr.push_back(dummy);
		arr.push_back(dummy);
		arr.push_back(dummy);
		arr.push_back(dummy);
	}

	void getters() const {
		const auto& valR = arr[0];
		::gaia::dont_optimize(valR);
		auto valC = arr[0];
		(void)valC;

		const uint8_t lowerPart = (arr[0] >> 1);
		const uint8_t upperPart = (arr[1] & (0xFF >> (8 - 0))) << 1;
		uint8_t value = lowerPart | upperPart;
		(void)value;
	}

	void setters() {
		const auto& valR = arr[0];
		::gaia::dont_optimize(valR);
		auto valC = arr[0];
		(void)valC;
		arr[0] = 0;
	}

	void diag() {
		for (auto it: arr)
			printf("%d\n", it);
	}
} g_test_00;

struct test_000 {
	cnt::sarray<int8_t, 4> arr{10, 10, 10, 10};

	void getters() const {
		const auto& valR = arr[0];
		::gaia::dont_optimize(valR);
		auto valC = arr[0];
		(void)valC;

		const uint8_t lowerPart = (arr[0] >> 1);
		const uint8_t upperPart = (arr[1] & (0xFF >> (8 - 0))) << 1;
		uint8_t value = lowerPart | upperPart;
		(void)value;
	}

	void setters() {
		const auto& valR = arr[0];
		::gaia::dont_optimize(valR);
		auto valC = arr[0];
		(void)valC;
		arr[0] = 0;
	}

	void diag() {
		for (auto it: arr)
			printf("%d\n", it);
	}
} g_test_000;

struct test_1 {
	cnt::darray<Position> arr;

	test_1() {
		arr.reserve(2);

		Position dummy = {1, 2, 3};
		arr.push_back(dummy);
		arr.push_back(dummy);
		arr.push_back(GAIA_MOV(dummy));
		arr.push_back({1, 2, 3});
	}

	void getters() const {
		const auto& valR = arr[0];
		::gaia::dont_optimize(valR);
		auto valC = arr[0];
		(void)valC.x;
	}

	void setters() {
		const auto& valR = arr[0];
		::gaia::dont_optimize(valR);
		auto valC = arr[0];
		(void)valC.x;
		arr[0] = {6, 6, 6};
	}

	void diag() {
		for (auto it: arr)
			printf("[%.2f, %.2f, %.2f]\n", it.x, it.y, it.z);
	}
} g_test_1;

struct test_2 {
	cnt::darray<PositionSoA> arr;

	test_2() {
		arr.reserve(2);

		PositionSoA dummy = {1, 2, 3};
		arr.push_back(dummy);
		arr.push_back(dummy);
		arr.push_back(GAIA_MOV(dummy));
		arr.push_back({1, 2, 3});
	}

	void getters() const {
		const auto& valR = arr[0];
		::gaia::dont_optimize(valR);
		auto valC = arr[0];
		(void)valC;
	}

	void setters() {
		const auto& valR = arr[0];
		::gaia::dont_optimize(valR);
		PositionSoA valC = arr[0];
		(void)valC.x;
		arr[0] = {6, 6, 6};
	}

	void diag() {
		for (auto it: arr)
			printf("[%.2f, %.2f, %.2f]\n", it.x, it.y, it.z);
	}
} g_test_2;

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

namespace dummy {
	struct Position {
		float x;
		float y;
		float z;
	};
} // namespace dummy

void test0() {
	ecs::World w;
	auto e = w.add(); // 14
	w.add<Position>(e, {1, 1, 1}); // 15
	w.add<dummy::Position>(e, {2, 2, 2}); // 16
	auto e2 = w.add(); // 17
	w.add<Position>(e2);
	auto a3 = w.add(); // 18
	w.add<dummy::Position>(a3);
	auto a4 = w.copy(a3); // 19

	w.diag_archetypes();

	{
		auto q1 = w.query().all<Position>();
		const auto c1 = q1.count();
		GAIA_ASSERT(c1 == 2);
		(void)c1;

		auto q2 = w.query().all<dummy::Position>();
		const auto c2 = q2.count();
		GAIA_ASSERT(c2 == 3);
		(void)c2;

		auto q3 = w.query().no<Position>();
		const auto c3 = q3.count();
		GAIA_ASSERT(c3 > 0); // It's going to be a bunch
		(void)c3;
	}
}

void test1() {
	ecs::World w;
	auto wolf = w.add();
	auto hare = w.add();
	auto rabbit = w.add();
	auto carrot = w.add();
	auto eats = w.add();

	w.add(rabbit, {eats, carrot});
	w.add(hare, {eats, carrot});
	w.add(wolf, {eats, rabbit});

	{
		auto q = w.query().add({ecs::QueryOpKind::All, ecs::QueryAccess::None, ecs::Pair(eats, ecs::All)});
		const auto cnt = q.count();
		GAIA_ASSERT(cnt == 3);
		(void)cnt;

		uint32_t i = 0;
		q.each([&](ecs::Entity entity) {
			const bool isRabbit = entity == rabbit;
			const bool isHare = entity == hare;
			const bool isWolf = entity == wolf;
			const bool is = isRabbit || isHare || isWolf;
			GAIA_ASSERT(is);
			(void)is;
			++i;
		});
		GAIA_ASSERT(i == cnt);
	}
}

void test2() {
	ecs::World w;
	ecs::Entity animal = w.add(); // 26
	ecs::Entity herbivore = w.add(); // 27
	w.add<Position>(herbivore, {}); // 28
	w.add<Velocity>(herbivore, {}); // 29
	ecs::Entity rabbit = w.add(); // 30
	ecs::Entity hare = w.add(); // 31
	ecs::Entity wolf = w.add(); // 32
	ecs::Entity dummy1 = w.add(); // 33
	ecs::Entity dummy2 = w.add(); // 34
	(void)dummy1;
	(void)dummy2;

	w.add(wolf, wolf); // turn wolf into an archetype
	w.as(herbivore, animal);
	w.as(rabbit, herbivore);
	w.as(hare, herbivore);
	w.as(wolf, animal);

	w.diag_archetypes();

	{
		uint32_t i = 0;
		ecs::Query q = w.query().all(ecs::Pair(ecs::Is, animal));
		q.each([&](ecs::Entity entity) {
			// runs for herbivore, rabbit, hare, wolf
			const bool isOK = entity == hare || entity == rabbit || entity == herbivore || entity == wolf;
			GAIA_ASSERT(isOK);
			GAIA_LOG_N("%d, [%u:%u]", isOK, entity.id(), entity.gen());

			++i;
		});
		GAIA_ASSERT(i == 4);
	}
	{
		uint32_t i = 0;
		ecs::Query q = w.query().all(ecs::Pair(ecs::Is, animal)).no(wolf);
		q.diag();
		q.each([&](ecs::Entity entity) {
			// runs for herbivore, rabbit, hare, wolf
			const bool isOK = entity == hare || entity == rabbit || entity == herbivore;
			GAIA_LOG_N("%d, [%u:%u]", isOK, entity.id(), entity.gen());

			++i;
		});
		GAIA_LOG_N("cnt = %u", i);
	}
	{
		uint32_t i = 0;
		ecs::Query q = w.query().all(ecs::Pair(ecs::Is, herbivore));
		q.each([&](ecs::Entity entity) {
			// runs for rabbit, hare
			const bool isOK = entity == hare || entity == rabbit;
			GAIA_LOG_N("%d, [%u:%u]", isOK, entity.id(), entity.gen());

			++i;
		});
		GAIA_LOG_N("cnt = %u", i);
		GAIA_LOG_N(" ");
	}
}

void test3() {
	ecs::World w;
	auto wolf = w.add(); // 14
	auto rabbit = w.add(); // 15
	auto carrot = w.add(); // 16
	auto eats = w.add(); // 17
	auto hungry = w.add(); // 18
	w.add(wolf, hungry);
	w.add(hungry, {ecs::OnDelete, ecs::Delete});
	w.add(wolf, {eats, rabbit});
	w.add(rabbit, {eats, carrot});

	w.diag_archetypes();

	w.del(hungry);
}

void test4() {
	ecs::World w;
	ecs::Entity e = w.add();
	w.add<Position>(e, {});
}

void test5() {
	const uint32_t N = 20;

	ecs::World w;
	cnt::darr<ecs::Entity> arr;
	arr.reserve(N);

	auto create = [&](uint32_t id) {
		auto e = w.add(); // 14, 16
		arr.push_back(e);

		w.add<Int3>(e, {id, id, id}); // 15
		auto pos = w.get<Int3>(e);
		GAIA_ASSERT(pos.x == id);
		GAIA_ASSERT(pos.y == id);
		GAIA_ASSERT(pos.z == id);
		return e;
	};
	auto remove = [&](ecs::Entity e) {
		w.del(e);
		const bool isEntityValid = w.valid(e);
		GAIA_ASSERT(!isEntityValid);
		(void)isEntityValid;
	};

	GAIA_FOR(N) create(i);
	w.diag_archetypes();

	GAIA_FOR(N) {
		remove(arr[i]);
		w.diag_archetypes();
	}
}

void test6() {
	ecs::World w;

	auto e = w.add(); // 14
	w.add<Position>(e); // 15

	auto qp = w.query().all<Position>();

	auto e1 = w.copy(e); // 16
	auto e2 = w.copy(e); // 17
	auto e3 = w.copy(e); // 18

	w.diag_archetypes();

	w.del(e1);
	w.diag_archetypes();

	{
		uint32_t cnt = 0;
		qp.each([&]([[maybe_unused]] const Position&) {
			++cnt;
		});
		GAIA_ASSERT(cnt == 2);
	}
}

void test7() {
	ecs::World w;
	auto wolf = w.add(); // 14
	auto rabbit = w.add(); // 15
	auto carrot = w.add(); // 16
	auto eats = w.add(); // 17
	auto hungry = w.add(); // 18
	w.add(wolf, hungry);
	w.add(hungry, {ecs::OnDelete, ecs::Delete});
	w.add(wolf, {eats, rabbit});
	w.add(rabbit, {eats, carrot});

	w.diag_archetypes();
	w.del(hungry);
	w.diag_archetypes();
	GAIA_ASSERT(!w.has(wolf));
	GAIA_ASSERT(w.has(rabbit));
	GAIA_ASSERT(w.has(eats));
	GAIA_ASSERT(w.has(carrot));
	GAIA_ASSERT(!w.has(hungry));
	GAIA_ASSERT(w.has(ecs::Pair(eats, rabbit)));
	GAIA_ASSERT(w.has(ecs::Pair(eats, carrot)));

	GAIA_FOR(100) w.update();
}

void test7b() {
	ecs::World wld;

	auto wolf = wld.add();
	auto rabbit = wld.add();
	auto carrot = wld.add();
	auto wine = wld.add();
	auto water = wld.add();
	auto eats = wld.add();
	auto drinks = wld.add();

	wld.add(rabbit, {eats, carrot});
	wld.add(wolf, {eats, rabbit});
	wld.add(rabbit, {drinks, water});
	wld.add(wolf, {drinks, wine});
	// wld.add(wolf, {drinks, water});

	{
		uint32_t i = 0;
		auto q = wld.query().all(ecs::Pair{eats, ecs::All}).all(ecs::Pair{drinks, water});
		q.each([&]() {
			++i;
		});
		GAIA_ASSERT(i == 1);
	}

	{
		uint32_t i = 0;
		auto q = wld.query().all(ecs::Pair{eats, ecs::All}).all(ecs::Pair{drinks, ecs::All});
		q.each([&]() {
			++i;
		});
		GAIA_ASSERT(i == 2);
	}
}

void test7c() {
	ecs::World wld;
	cnt::darr<ecs::Entity> arr;
	{
		auto q = wld.query().no<ecs::Component>().no<ecs::Core_>();
		q.arr(arr);
	}
	// 3.7
	// 4.6
	// 1.0
	// 2.0
	GAIA_EACH(arr) {
		GAIA_LOG_N("%u.%u", arr[i].id(), arr[i].gen());
	}
	GAIA_ASSERT(arr.size() == 4);
}

void test7d() {
	ecs::World wld;
	auto e1 = wld.add();
	wld.add<Position>(e1);
	wld.add<Velocity>(e1);
	auto e2 = wld.add();
	wld.add<Position>(e2);
	auto e3 = wld.add();
	wld.add<Velocity>(e3);
	auto e4 = wld.copy(e3);
	(void)e4;
	{
		auto q = wld.query().all<Velocity>().no<Position>();
		const auto cnt = q.count();
		(void)cnt;
		GAIA_ASSERT(cnt == 2);
	}
}

void test7e() {
	const uint32_t N = 10;

	ecs::World wld;
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
	};

	GAIA_FOR(N) create(i);

	constexpr bool UseCachedQuery = true;
	// auto q1 = wld.query<UseCachedQuery>().all<Position>();
	// auto q2 = wld.query<UseCachedQuery>().all<Rotation>();
	// auto q3 = wld.query<UseCachedQuery>().all<Position, Rotation>();
	// auto q4 = wld.query<UseCachedQuery>().all<Position, Scale>();
	auto q5 = wld.query<UseCachedQuery>().all<Position>().all<Scale>().all<Something>();

	auto pos = wld.get<Position>();
	auto sca = wld.get<Scale>();
	auto som = wld.get<Something>();
	(void)pos;
	(void)sca;
	(void)som;

	{
		// uint32_t cnt = 0;
		// q5.each([&cnt](ecs::Iter& it) {
		// 	GAIA_EACH(it)++ cnt;
		// });
		// GAIA_ASSERT(cnt == N / 2);

		ents.clear();
		q5.arr(ents);
		GAIA_ASSERT(ents.size() == N / 2);
	}
}

void test7f() {
	ecs::World wld;

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
		ecs::Query q = wld.query().any<Position>().any<Acceleration>();

		uint32_t cnt = 0;
		q.each([&](ecs::Iter& it) {
			++cnt;

			const bool ok1 = it.has<Position>() || it.has<Acceleration>();
			GAIA_ASSERT(ok1);
			(void)ok1;
			const bool ok2 = it.has<Acceleration>() || it.has<Position>();
			GAIA_ASSERT(ok2);
			(void)ok2;
		});
		GAIA_ASSERT(cnt == 2);
		(void)cnt;
	}
}

void test8() {
	ecs::World w;
	auto parent = w.add(); // 14
	auto child = w.add(); // 15
	auto child_of = w.add(); // 16
	w.add(child_of, {ecs::OnDeleteTarget, ecs::Delete}); // 4:6
	w.add(child, {child_of, parent}); // 16:14

	w.diag_archetypes();
	w.del(parent);
	GAIA_ASSERT(!w.has(child));
	GAIA_ASSERT(!w.has(parent));

	w.update();
}

void test9() {
	ecs::World w;
	auto e = w.add(); // 14

	struct Start {};
	w.add<Position>(e); // 15
	w.add<ecs::uni<Position>>(e); // 16
	w.add<Start>(e); // 17

	GAIA_LOG_N("set --------------------");
	w.add<ecs::pair<Start, Position>>(e, {1, 1, 1}); // 17:15
	w.add<ecs::pair<Start, ecs::uni<Position>>>(e, {10, 10, 10}); // 17:16

	GAIA_LOG_N("get --------------------");
	auto p = w.get<ecs::pair<Start, Position>>(e);
	auto up = w.get<ecs::pair<Start, ecs::uni<Position>>>(e);

	w.diag_components();
	w.diag_archetypes();
	w.update();
}

void test10() {
	ecs::World w;
	CreateEntities(w);

	int sys1_cnt = 0;
	int sys2_cnt = 0;
	auto sys2 = w.system()
									.all<Position&>()
									.all<Velocity>() //
									.on_each([&sys1_cnt](Position& p, const Velocity& v) {
										const float dt = 0.01f;
										p.x += v.x * dt;
										p.y += v.y * dt;
										p.z += v.z * dt;
										GAIA_LOG_N("sys1 #%d", ++sys1_cnt);
									});
	auto sys1 = w.system()
									.all<Position>() //
									.on_each([&sys2_cnt](ecs::Iter& it) {
										auto pv = it.view<Position>();
										GAIA_EACH(it) {
											const auto& p = pv[i];
											GAIA_LOG_N("sys2 #%d [%.2f, %.2f, %.2f]", ++sys2_cnt, p.x, p.y, p.z);
										}
									});
	// Make sure to execute sys2 before sys1
	w.add(sys1.entity(), {ecs::DependsOn, sys2.entity()});
	// w.diag_archetypes();

	// Put the systems into their proper stage
	// ecs::Entity OnPreUpdate = w.add();
	// ecs::Entity OnUpdate = w.add();
	// ecs::Entity OnPostUpdate = w.add();
	// w.add(sys1.entity(), {ecs::ChildOf, OnUpdate});
	// w.add(sys2.entity(), {ecs::ChildOf, OnUpdate});

	ecs::Query sq = w.query().all<ecs::System_&>();
	// GAIA_FOR(1000)

	sq.each([](ecs::System_& sys) {
		sys.exec();
	});

	sq.each([](ecs::Iter& it) {
		auto se_view = it.sview_mut<ecs::System_>(0);
		GAIA_EACH(it) {
			auto& sys = se_view[i];
			sys.exec();
		}
	});

	// sys1.exec();
	// sys2.exec();

	GAIA_LOG_N("RES: sys1 #%d", sys1_cnt);
	GAIA_LOG_N("RES: sys2 #%d", sys1_cnt);

	w.update();
}

void test10b() {
	ecs::World wld;

	constexpr uint32_t N = 10;
	{
		auto e = wld.add();
		wld.add<Position>(e, {0, 100, 0});
		wld.add<Acceleration>(e, {1, 0, 0});
		GAIA_FOR(N - 1) {
			[[maybe_unused]] auto newEntity = wld.copy(e);
		}
	}

	int sys1_cnt = 0;
	int sys2_cnt = 0;
	int sys3_cnt = 0;
	bool sys3_run_before_sys1 = false;
	bool sys3_run_before_sys2 = false;

	auto testRun1 = [&]() {
		GAIA_FOR(1) {
			sys3_run_before_sys1 = false;
			sys3_run_before_sys2 = false;
			wld.update();
			// wld.diag_archetypes();
			GAIA_ASSERT(sys1_cnt == N);
			// GAIA_ASSERT(sys2_cnt == N);
			// GAIA_ASSERT(sys3_cnt == N);
			sys1_cnt = 0;
			sys2_cnt = 0;
			sys3_cnt = 0;
		}
	};
	auto testRun = [&]() {
		GAIA_LOG_N("-------------------------------");
		GAIA_FOR(1) {
			sys3_run_before_sys1 = false;
			sys3_run_before_sys2 = false;
			wld.update();
			// wld.diag_archetypes();
			GAIA_ASSERT(sys1_cnt == N);
			GAIA_ASSERT(sys2_cnt == N);
			GAIA_ASSERT(sys3_cnt == N);
			sys1_cnt = 0;
			sys2_cnt = 0;
			sys3_cnt = 0;
		}
		GAIA_LOG_N("================");
	};

	// Our systems
	auto sys1 = wld.system()
									.all<Position>() //
									.all<Acceleration>() //
									.on_each([&](Position, Acceleration) {
										if (sys1_cnt == 0 && sys3_cnt > 0)
											sys3_run_before_sys1 = true;
										++sys1_cnt;
									});
	wld.name(sys1.entity(), "sys1");
	auto sys2 = wld.system()
									.all<Position>() //
									.on_each([&](ecs::Iter& it) {
										if (sys2_cnt == 0 && sys3_cnt > 0)
											sys3_run_before_sys2 = true;
										GAIA_EACH(it)++ sys2_cnt;
									});
	wld.name(sys2.entity(), "sys2");
	auto sys3 = wld.system()
									.all<Acceleration>() //
									.on_each([&](ecs::Iter& it) {
										GAIA_EACH(it)++ sys3_cnt;
									});
	wld.name(sys3.entity(), "sys3");

	testRun1();

	// Make sure to execute sys2 before sys1
	wld.add(sys1.entity(), {ecs::DependsOn, sys3.entity()});
	wld.add(sys2.entity(), {ecs::DependsOn, sys3.entity()});

	testRun();

	GAIA_ASSERT(sys3_run_before_sys1);
	GAIA_ASSERT(sys3_run_before_sys2);
}

void test11() {
	ecs::World w;

	ecs::Entity eats = w.add(); // 17
	ecs::Entity carrot = w.add(); // 18
	ecs::Entity salad = w.add(); // 19
	ecs::Entity apple = w.add(); // 20

	ecs::Entity ents[6];
	GAIA_FOR(6) ents[i] = w.add(); // 21, 22, 23, 24, 25, 26
	{
		// 27 - Position
		// 28 - Healthy
		w.build(ents[0]).add<Position>().add({eats, salad}); // 21 <-- Pos, {Eats,Salad}
		w.build(ents[1]).add<Position>().add({eats, carrot});
		w.build(ents[2]).add<Position>().add({eats, apple});

		w.build(ents[3]).add<Position>().add({eats, apple}).add<Healthy>();
		w.build(ents[4]).add<Position>().add({eats, salad}).add<Healthy>();
		w.build(ents[5]).add<Position>().add({eats, carrot}).add<Healthy>();
	}
	// This query is going to group entities by what they eat.
	// The query cache is going to contain following 6 archetypes in 3 groups as follows:
	//  - Eats:carrot:
	//     - Position, (Eats, carrot)
	//     - Position, (Eats, carrot), Healthy
	//  - Eats:salad:
	//     - Position, (Eats, salad)
	//     - Position, (Eats, salad), Healthy
	//  - Eats::apple:
	//     - Position, (Eats, apple)
	//     - Position, (Eats, apple), Healthy
	ecs::Entity ents_expected[] = {ents[1], ents[5], // carrot, 22, 26
																 ents[0], ents[4], // salad, 21, 25
																 ents[2], ents[3]}; // apple, 23, 24

	auto qqNoGroup = w.query().all<Position>();
	qqNoGroup.each([&](ecs::Iter& it) {
		auto ents = it.view<ecs::Entity>();
		GAIA_EACH(it) {
			auto e = ents[i];
			GAIA_LOG_N("%u, %u.%u", it.group_id(), e.id(), e.gen());
		}
	});

	// auto qq = w.query().all<Position>().group_by(eats);
	//  All entities, iterated by groups (lowest -> hightest id)
	//  GAIA_LOG_N("1");
	//  qq.each([&](ecs::Entity e) {
	//  	GAIA_LOG_N("%u.%u", e.id(), e.gen());
	//  });
	//  All entities, iterated by groups (lowest -> hightest id), via the iterator
	//  GAIA_LOG_N("2");
	//  qq.each([&](ecs::Iter& it) {
	//  	auto ents = it.view<ecs::Entity>();

	// 	GAIA_EACH(it) {
	// 		auto e = ents[i];
	// 		GAIA_LOG_N("%u, %u.%u", it.group_id(), e.id(), e.gen());
	// 	}
	// });
	// All entities, iterated by groups (lowest -> hightest id), via the iterator, only a specific group
	// 18, 26.0
	// 18, 22.0
	// 19, 25.0
	// 19, 21.0
	// 20, 24.0
	// 20, 23.0
	GAIA_LOG_N("3");
	auto qq = w.query().all<Position>().group_by(eats);
	qq.group_id(carrot).each([&](ecs::Iter& it) {
		auto ents = it.view<ecs::Entity>();
		GAIA_EACH(it) {
			auto e = ents[i];
			GAIA_LOG_N("%u, %u.%u", it.group_id(), e.id(), e.gen());
		}
	});
}

void test12() {
	ecs::World w;

	ecs::Entity p1 = w.add(); // 25
	ecs::Entity ents[6];
	GAIA_FOR(6) ents[i] = w.add(); // 26, 27, 28, 29, 30, 31
	ecs::Entity p2 = w.add(); // 32
	{
		// 33 - Position
		// 34 - Healthy
		w.build(ents[0]).add<Position>().add({ecs::ChildOf, p1});
		w.build(ents[1]).add({ecs::ChildOf, p1});
		w.build(ents[2]).add<Position>().add({ecs::ChildOf, p1});

		w.build(ents[3]).add<Position>().add({ecs::ChildOf, p2});
		w.build(ents[4]).add<Position>().add({ecs::ChildOf, p2});
		w.build(ents[5]).add({ecs::ChildOf, p2});
	}

	// Position($0), ChildOf($1, $0), !Position($1)
	// 1) query all entities with position
	// 2) consider only children of such entities
	// 3) consider only those children that don't have position
	auto pos = w.get<Position>();
	auto q = w.query()
							 // Position($0)
							 .add({ecs::QueryOpKind::All, ecs::QueryAccess::None, pos, ecs::Var0})
							 // ChildOf($1, $0)
							 .add({ecs::QueryOpKind::All, ecs::QueryAccess::None, ecs::Pair{ecs::ChildOf, ecs::Var0}, ecs::Var1})
							 // !Position($1)
							 .add({ecs::QueryOpKind::Not, ecs::QueryAccess::None, pos, ecs::Var1});
	q.each([&](ecs::Iter& it) {
		auto ents = it.view<ecs::Entity>();
		GAIA_EACH(it) {
			auto e = ents[i];
			GAIA_LOG_N("%u.%u", e.id(), e.gen());
		}
	});
}

void test12b() {
	ecs::World w;

	auto eats = w.add(); // 25
	auto likes = w.add(); // 26
	auto tasty = w.add(); // 27

	auto food1 = w.add(); // 28
	auto food2 = w.add(); // 29
	w.add(food2, tasty);

	ecs::Entity ents[6];
	GAIA_FOR(6) ents[i] = w.add(); // 30, 31, 32, 33, 34, 35
	{
		w.build(ents[0]).add({eats, food1}).add({likes, food1});
		w.build(ents[1]).add({eats, food2}).add({likes, food2});
		w.build(ents[2]).add({eats, food1}).add({likes, food2});
		w.build(ents[3]).add({eats, food2}).add({likes, food1});
		w.build(ents[4]).add({eats, food1});
		w.build(ents[5]).add({eats, food2});
	}

	// eats(%this, %vO), whatever(%this, tasty), likes(%this, %vO)
	// {eats, food1}, {likes, food1}
	// {eats, food2}, {likes, food2}
	// {eats, food1}, {likes, food2}
	// {eats, food2}, {likes, food1}
	auto q = w.query()
							 //
							 .add({ecs::QueryOpKind::All, ecs::QueryAccess::Read, ecs::Pair{eats, ecs::Var0}})
							 //
							 .add({ecs::QueryOpKind::All, ecs::QueryAccess::Read, ecs::Pair{likes, ecs::Var0}});
	q.each([&](ecs::Iter& it) {
		auto ents = it.view<ecs::Entity>();
		GAIA_EACH(it) {
			auto e = ents[i];
			GAIA_LOG_N("%u.%u", e.id(), e.gen());
		}
	});
}

void test13() {
	ecs::World w;
	auto e = w.add();
	w.name(e, "xyz");
	const char* pName = w.name(e);
	GAIA_LOG_N("%s", pName);
}

void test14() {
	ecs::World wld;
	auto wolf = wld.add();
	auto rabbit = wld.add();
	auto carrot = wld.add();
	auto eats = wld.add();

	wld.add(rabbit, {eats, carrot});
	wld.add(wolf, {eats, rabbit});

	auto q1 = wld.query().add({ecs::QueryOpKind::All, ecs::QueryAccess::None, ecs::Pair(eats, carrot)});
	auto q2 = wld.query().add("(%e, %e)", eats.value(), carrot.value());
	(void)q1.count();
	(void)q2.count();
}

void test15() {
	ecs::World wld;
	ecs::Entity animal = wld.add();
	ecs::Entity herbivore = wld.add();
	ecs::Entity carnivore = wld.add();
	ecs::Entity wolf = wld.add();

	wld.name(animal, "animal");
	wld.name(herbivore, "herbivore");
	wld.name(carnivore, "carnivore");
	wld.name(wolf, "wolf");

	wld.as(carnivore, animal);
	wld.as(herbivore, animal);
	wld.as(wolf, carnivore);

	GAIA_ASSERT(wld.is(carnivore, animal));
	GAIA_ASSERT(wld.is(herbivore, animal));
	GAIA_ASSERT(wld.is(wolf, animal));

	GAIA_ASSERT(wld.is(animal, animal));
	GAIA_ASSERT(wld.is(herbivore, herbivore));
	GAIA_ASSERT(wld.is(carnivore, carnivore));
	GAIA_ASSERT(wld.is(wolf, carnivore));

	GAIA_ASSERT(!wld.is(animal, herbivore));
	GAIA_ASSERT(!wld.is(animal, carnivore));
	GAIA_ASSERT(!wld.is(wolf, herbivore));

	ecs::Query q = wld.query().all(ecs::Pair(ecs::Is, animal));

	{
		uint32_t i = 0;
		q.each([&](ecs::Entity entity) {
			[[maybe_unused]] const bool isOK =
					entity == animal || entity == herbivore || entity == carnivore || entity == wolf;
			GAIA_ASSERT(isOK);

			++i;
		});
		GAIA_ASSERT(i == 4);
	}

	// Carnivore is no longer an animal
	wld.del(carnivore, {ecs::Is, animal});
	GAIA_ASSERT(wld.is(wolf, carnivore));
	GAIA_ASSERT(!wld.is(carnivore, animal));
	GAIA_ASSERT(!wld.is(wolf, animal));

	wld.diag_archetypes();

	{
		uint32_t i = 0;
		q.each([&](ecs::Entity entity) {
			[[maybe_unused]] bool isOK = entity == animal || entity == herbivore;
			GAIA_ASSERT(isOK);

			++i;
		});
		GAIA_ASSERT(i == 2);
	}

	// Make carnivore an animal again
	wld.as(carnivore, animal);

	{
		uint32_t i = 0;
		q.each([&](ecs::Entity entity) {
			[[maybe_unused]] bool isOK = entity == animal || entity == herbivore || entity == carnivore || entity == wolf;
			GAIA_ASSERT(isOK);

			++i;
		});
		GAIA_ASSERT(i == 4);
	}

	// Wolf is no longer a carnivore and thus no longer an animal.
	// It should no longer match q
	wld.del(wolf, {ecs::Is, carnivore});
	GAIA_ASSERT(!wld.is(wolf, carnivore));
	GAIA_ASSERT(wld.is(carnivore, animal));
	GAIA_ASSERT(!wld.is(wolf, animal));

	{
		uint32_t i = 0;
		q.each([&](ecs::Entity entity) {
			[[maybe_unused]] const bool isOK = entity == animal || entity == herbivore || entity == carnivore;
			GAIA_ASSERT(isOK);

			++i;
		});
		GAIA_ASSERT(i == 3);
	}
}

void test16() {
	const uint32_t N = 15'000;

	ecs::World wld;

	auto create = [&](uint32_t i) {
		auto e = wld.add();
		wld.add<Int3>(e, {i, i, i});
	};
	GAIA_FOR(N) create(i);
}

void test17() {
	ecs::World wld;

	auto e0 = wld.add();
	auto e1 = wld.add();
	auto e2 = wld.add();

	wld.add<Position>(e0, {0, 0, 0});
	wld.add<Position>(e1, {1, 1, 1});

	wld.system() //
			.all<Position>()
			.on_each([](ecs::Iter& it) {
				auto& cb = it.cmd_buffer_st();
				auto pos = it.view<Position>();
				auto ent = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					cb.add<Rotation>(ent[i], {pos[i].x, pos[i].y, pos[i].z});
				}
			});

	GAIA_FOR(100) wld.update();
}

int main() {
	// test0();
	// test1();
	// test2();
	// test3();
	// test4();
	// test5();
	// test6();
	// test7();
	// test7b();
	// test7c();
	// test7d();
	// test7e();
	// test7f();
	// test8();
	// test9();
	// test10();
	// test10b();
	// test11();
	// test12();
	// test12b();
	// test13();
	// test14();
	// test15();
	// test16();
	test17();

	return 0;
}