#include <gaia.h>
#include <type_traits>

using namespace gaia;

struct Position {
	float x, y, z;
};
struct PositionSoA {
	static constexpr auto Layout = mem::DataLayout::SoA;
	float x, y, z;
};
struct Velocity {
	float x, y, z;
};

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

class PositionSystem final: public ecs::System {
	ecs::QueryUncached m_q;

public:
	void OnCreated() override {
		m_q = world().query<false>().all<Position&, Velocity>();
	}

	void OnUpdate() override {
		m_q.each([](Position& p, const Velocity& v) {
			const float dt = 0.01f;
			p.x += v.x * dt;
			p.y += v.y * dt;
			p.z += v.z * dt;
		});
	}
};

class PositionSystem_All final: public ecs::System {
	ecs::Query m_q;

public:
	void OnCreated() override {
		m_q = world().query().all<Position&, Velocity>();
	}

	void OnUpdate() override {
		m_q.each([](ecs::IterAll iter) {
			auto p = iter.view_mut<Position>();
			auto v = iter.view<Velocity>();
			const float dt = 0.01f;
			GAIA_EACH(iter) {
				p[i].x += v[i].x * dt;
				p[i].y += v[i].y * dt;
				p[i].z += v[i].z * dt;
			}

			if (iter.enabled(0))
				p[0].x += 1.f;
		});
	}
};

class PositionSystem_All2 final: public ecs::System {
	ecs::Query m_q;

public:
	void OnCreated() override {
		m_q = world().query().all<Position&, Velocity>();
	}

	void OnUpdate() override {
		m_q.each([](ecs::IterAll iter) {
			auto p = iter.view_mut<Position>();
			auto v = iter.view<Velocity>();
			const float dt = 0.01f;
			GAIA_EACH(iter) {
				p[i].x += v[i].x * dt;
				p[i].y += v[i].y * dt;
				p[i].z += v[i].z * dt;
			}

			if (iter.enabled(0))
				p[0].x += 1.f;
		});
	}
};

class PositionSystem_DisabledOnly final: public ecs::System {
	ecs::Query m_q;

public:
	void OnCreated() override {
		m_q = world().query().all<Position&, Velocity>();
	}

	void OnUpdate() override {
		m_q.each([](ecs::IterDisabled iter) {
			auto p = iter.view_mut<Position>();
			auto v = iter.view<Velocity>();
			const float dt = 0.01f;
			GAIA_EACH(iter) {
				p[i].x += v[i].x * dt;
				p[i].y += v[i].y * dt;
				p[i].z += v[i].z * dt;
			}
		});
	}
};

void CreateEntities(ecs::World& w) {
	auto e = w.add();
	w.add<Position>(e, {0, 100, 0});
	w.add<Velocity>(e, {1, 0, 0});

	constexpr uint32_t N = 1000;
	GAIA_FOR(N) {
		[[maybe_unused]] auto newentity = w.copy(e);
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

void test1() {
	ecs::World w;
	auto wolf = w.add();
	auto hare = w.add();
	auto rabbit = w.add();
	auto carrot = w.add();
	auto eats = w.add();

	w.add(rabbit, ecs::Pair(eats, carrot));
	w.add(hare, ecs::Pair(eats, carrot));
	w.add(wolf, ecs::Pair(eats, rabbit));

	{
		auto q = w.query().add({ecs::Pair(eats, ecs::All), ecs::QueryOp::All, ecs::QueryAccess::None});
		const auto cnt = q.count();
		GAIA_ASSERT(cnt == 3);

		uint32_t i = 0;
		q.each([&](ecs::Entity entity) {
			const bool isRabbit = entity == rabbit;
			const bool isHare = entity == hare;
			const bool isWolf = entity == wolf;
			const bool is = isRabbit || isHare || isWolf;
			GAIA_ASSERT(is);
			++i;
		});
		GAIA_ASSERT(i == cnt);
	}
}

void test2() {
	ecs::World w;
	ecs::Entity animal = w.add(); // 14
	ecs::Entity herbivore = w.add(); // 15
	w.add<Position>(herbivore, {}); // 16
	w.add<Velocity>(herbivore, {}); // 17
	ecs::Entity rabbit = w.add(); // 18
	ecs::Entity hare = w.add(); // 19
	ecs::Entity wolf = w.add(); // 20

	w.as(herbivore, animal);
	w.as(rabbit, herbivore);
	w.as(hare, herbivore);
	w.as(wolf, animal);

	{
		uint32_t i = 0;
		ecs::Query q = w.query().all(ecs::Pair(ecs::Is, animal));
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
		GAIA_LOG_N("");
	}
}

int main() {
	// test1();
	test2();

	// g_test_0.getters();
	// g_test_0.setters();
	// g_test_0.diag();

	// g_test_00.getters();
	// g_test_00.setters();
	// g_test_00.diag();

	// g_test_1.getters();
	// g_test_1.setters();
	// g_test_1.diag();

	// g_test_2.getters();
	// g_test_2.setters();
	// g_test_2.diag();

	// printf("aos\n");
	// {
	// 	using vp = mem::data_view_policy_aos<int*>;
	// 	mem::raw_data_holder<int*, 10> arr;
	// 	(void)arr;
	// 	auto aa = arr[0];
	// 	(void)aa;
	// 	auto* bb = &arr[0];
	// 	(void)bb;
	// 	int* a = reinterpret_cast<int*>(arr[0]);
	// 	(void)a;

	// 	vp::set({(int**)&arr[0], 10}, 0, (int*)bb);
	// }

	// printf("darray<int8_t*>\n");
	// {
	// 	cnt::darray<int8_t*> arr;
	// 	arr.reserve(2);

	// 	int8_t dummy = 10;
	// 	arr.push_back(&dummy);
	// 	arr.push_back(&dummy);
	// 	arr.push_back(&dummy);
	// 	arr.push_back(nullptr);

	// 	const auto& valR = arr[0];
	// 	::gaia::dont_optimize(valR);
	// 	auto* valC = arr[0];
	// 	(void)valC;
	// 	arr[0] = &dummy;

	// 	for (auto* it: arr)
	// 		printf("%p\n", it);
	// }

	// printf("darray<int*>\n");
	// {
	// 	cnt::darray<int*> arr;
	// 	arr.reserve(2);

	// 	int dummy = 10;
	// 	arr.push_back(&dummy);
	// 	arr.push_back(&dummy);
	// 	arr.push_back(&dummy);
	// 	arr.push_back(nullptr);

	// 	const auto& valR = arr[0];
	// 	::gaia::dont_optimize(valR);
	// 	auto* valC = arr[0];
	// 	(void)valC;
	// 	arr[0] = &dummy;

	// 	for (auto* it: arr)
	// 		printf("%p\n", it);
	// }

	// printf("darray<const int*>\n");
	// {
	// 	cnt::darray<const int*> arr;
	// 	arr.reserve(2);

	// 	const int dummy = 10;
	// 	arr.push_back(&dummy);
	// 	arr.push_back(&dummy);
	// 	arr.push_back(&dummy);
	// 	arr.push_back(nullptr);

	// 	const auto& valR = arr[0];
	// 	::gaia::dont_optimize(valR);
	// 	const auto* valC = arr[0];
	// 	(void)valC;
	// 	arr[0] = nullptr;

	// 	for (const auto* it: arr)
	// 		printf("%p\n", it);
	// }

	// ecs::World w;
	// CreateEntities(w);

	// ecs::SystemManager sm(w);
	// sm.add<PositionSystem>();
	// sm.add<PositionSystem_All>();
	// sm.add<PositionSystem_All2>();
	// sm.add<PositionSystem_DisabledOnly>();
	// GAIA_FOR(1000) {
	// 	sm.update();
	// 	w.update();
	// }

	return 0;
}