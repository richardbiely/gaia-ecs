#include <gaia.h>

using namespace gaia;

struct Position {
	float x, y, z;
};
struct PositionSoA {
	static constexpr auto Layout = utils::DataLayout::SoA;
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
	ecs::Query m_q;

public:
	void OnCreated() override {
		m_q = GetWorld().CreateQuery().All<Position, const Velocity>();
	}

	void OnUpdate() override {
		m_q.ForEach([](Position& p, const Velocity& v) {
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
		m_q = GetWorld().CreateQuery().All<Position, const Velocity>();
	}

	void OnUpdate() override {
		m_q.ForEach([](ecs::Iterator iter) {
			auto p = iter.ViewRW<Position>();
			auto v = iter.View<Velocity>();
			const float dt = 0.01f;
			for (auto i: iter) {
				p[i].x += v[i].x * dt;
				p[i].y += v[i].y * dt;
				p[i].z += v[i].z * dt;
			}

			if (iter.IsEnabled(0))
				p[0].x += 1.f;
		});
	}
};

class PositionSystem_All2 final: public ecs::System {
	ecs::Query m_q;

public:
	void OnCreated() override {
		m_q = GetWorld().CreateQuery().All<Position, const Velocity>();
	}

	void OnUpdate() override {
		m_q.ForEach([](ecs::Iterator iter) {
			auto p = iter.ViewRW<Position>();
			auto v = iter.View<Velocity>();
			const float dt = 0.01f;
			for (uint32_t i = 0; i < iter.size(); ++i) {
				p[i].x += v[i].x * dt;
				p[i].y += v[i].y * dt;
				p[i].z += v[i].z * dt;
			}

			if (iter.IsEnabled(0))
				p[0].x += 1.f;
		});
	}
};

class PositionSystem_EnabledOnly final: public ecs::System {
	ecs::Query m_q;

public:
	void OnCreated() override {
		m_q = GetWorld().CreateQuery().All<Position, const Velocity>();
	}

	void OnUpdate() override {
		m_q.ForEach([](ecs::IteratorEnabled iter) {
			auto p = iter.ViewRW<Position>();
			auto v = iter.View<Velocity>();
			const float dt = 0.01f;
			for (auto i: iter) {
				p[i].x += v[i].x * dt;
				p[i].y += v[i].y * dt;
				p[i].z += v[i].z * dt;
			}
		});
	}
};

void CreateEntities(ecs::World& w) {
	{
		auto e = w.Add();
		w.Add<Position>(e, {0, 100, 0});
		w.Add<Velocity>(e, {1, 0, 0});

		constexpr uint32_t N = 1000;
		for (uint32_t i = 0; i < N; i++) {
			[[maybe_unused]] auto newentity = w.Add(e);
		}
	}
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

struct test_0 {
	containers::darray<int8_t*> arr;

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
	containers::darray<int8_t> arr;

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
	containers::sarray<int8_t, 4> arr;

	test_000() {
		arr = {10, 10, 10, 10};
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
} g_test_000;

struct test_1 {
	containers::darray<Position> arr;

	test_1() {
		arr.reserve(2);

		Position dummy = {1, 2, 3};
		arr.push_back(dummy);
		arr.push_back(dummy);
		arr.push_back(std::move(dummy));
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
	containers::darray<PositionSoA> arr;

	test_2() {
		arr.reserve(2);

		PositionSoA dummy = {1, 2, 3};
		arr.push_back(dummy);
		arr.push_back(dummy);
		arr.push_back(std::move(dummy));
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

int main() {
	g_test_0.getters();
	g_test_0.setters();
	g_test_0.diag();

	g_test_00.getters();
	g_test_00.setters();
	g_test_00.diag();

	g_test_1.getters();
	g_test_1.setters();
	g_test_1.diag();

	g_test_2.getters();
	g_test_2.setters();
	g_test_2.diag();

	printf("aos\n");
	{
		using vp = utils::data_view_policy_aos<int*>;
		utils::raw_data_holder<int*, 10> arr;
		(void)arr;
		auto aa = arr[0];
		(void)aa;
		auto* bb = &arr[0];
		(void)bb;
		int* a = reinterpret_cast<int*>(arr[0]);
		(void)a;

		vp::set({(int**)&arr[0], 10}, 0, (int*)bb);
	}

	printf("darray<int8_t*>\n");
	{
		containers::darray<int8_t*> arr;
		arr.reserve(2);

		int8_t dummy = 10;
		arr.push_back(&dummy);
		arr.push_back(&dummy);
		arr.push_back(&dummy);
		arr.push_back(nullptr);

		const auto& valR = arr[0];
		::gaia::dont_optimize(valR);
		auto* valC = arr[0];
		(void)valC;
		arr[0] = &dummy;

		for (auto* it: arr)
			printf("%p\n", it);
	}

	printf("darray<int*>\n");
	{
		containers::darray<int*> arr;
		arr.reserve(2);

		int dummy = 10;
		arr.push_back(&dummy);
		arr.push_back(&dummy);
		arr.push_back(&dummy);
		arr.push_back(nullptr);

		const auto& valR = arr[0];
		::gaia::dont_optimize(valR);
		auto* valC = arr[0];
		(void)valC;
		arr[0] = &dummy;

		for (auto* it: arr)
			printf("%p\n", it);
	}

	printf("darray<const int*>\n");
	{
		containers::darray<const int*> arr;
		arr.reserve(2);

		const int dummy = 10;
		arr.push_back(&dummy);
		arr.push_back(&dummy);
		arr.push_back(&dummy);
		arr.push_back(nullptr);

		const auto& valR = arr[0];
		::gaia::dont_optimize(valR);
		const auto* valC = arr[0];
		(void)valC;
		arr[0] = nullptr;

		for (const auto* it: arr)
			printf("%p\n", it);
	}

	ecs::World w;
	CreateEntities(w);

	ecs::SystemManager sm(w);
	sm.CreateSystem<PositionSystem>();
	sm.CreateSystem<PositionSystem_All>();
	sm.CreateSystem<PositionSystem_All2>();
	sm.CreateSystem<PositionSystem_EnabledOnly>();
	for (uint32_t i = 0; i < 1000; ++i) {
		sm.Update();
		w.Update();
	}

	return 0;
}