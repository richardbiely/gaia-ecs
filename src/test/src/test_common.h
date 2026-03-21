#pragma once

#define NOMINMAX

#include <atomic>
#include <cstdint>
#include <type_traits>
#if GAIA_SYSTEMS_ENABLED
	#include <mutex>
#endif

#define GAIA_ENABLE_HOOKS 1
#define GAIA_ENABLE_SET_HOOKS 1
#define GAIA_ENABLE_ADD_DEL_HOOKS 1
#define GAIA_SYSTEMS_ENABLED 1
#define GAIA_OBSERVERS_ENABLED 1
#define GAIA_USE_SAFE_ENTITY 1
#include <gaia.h>

#if GAIA_COMPILER_MSVC
	#if _MSC_VER <= 1916
// warning C4100: 'XYZ': unreferenced formal parameter
GAIA_MSVC_WARNING_DISABLE(4100)
	#endif
#endif

#define DOCTEST_CONFIG_SUPER_FAST_ASSERTS
#include <doctest/doctest.h>

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>

namespace rnd {
	struct pseudo_random {
		uint32_t state = 0;

		constexpr pseudo_random() {}
		constexpr pseudo_random(uint32_t seed): state(seed) {}

		constexpr uint32_t next() {
			state = state * 1664525 + 1013904223;
			return state;
		}

		constexpr uint32_t operator()() noexcept {
			return next();
		}

		constexpr uint32_t range(uint32_t low, uint32_t high) noexcept {
			const uint32_t r = high - low + 1;
			return (operator()() % r) + low;
		}
	};
} // namespace rnd

using namespace gaia;

#define wld twld.m_w

//! World wrapper for test purposes.
//! Upon destruction it performs world update several times to make sure there are no issues
//! with the update function no matter what the unit test does.
struct TestWorld {
	ecs::World m_w;

	TestWorld() = default;
	TestWorld(const ecs::World& world) = delete;
	TestWorld(ecs::World&& world) = delete;
	TestWorld& operator=(const ecs::World& world) = delete;
	TestWorld& operator=(ecs::World&& world) = delete;

	~TestWorld() {
		update();
	}

	void update() {
		GAIA_FOR(100) m_w.update();
	}
};

struct NonUniqueNameOpsGuard {
	bool prev;
	NonUniqueNameOpsGuard(): prev(ecs::World::s_enableUniqueNameDuplicateAssert) {
		ecs::World::s_enableUniqueNameDuplicateAssert = false;
	}
	~NonUniqueNameOpsGuard() {
		ecs::World::s_enableUniqueNameDuplicateAssert = true;
	}
};

struct Int3 {
	uint32_t x, y, z;
};
struct Position {
	float x, y, z;
};
static_assert(ecs::storage_type_v<Position> == ecs::DataStorageType::Table);
struct PositionSparse {
	GAIA_STORAGE(Sparse);
	float x, y, z;
};
static_assert(ecs::storage_type_v<PositionSparse> == ecs::DataStorageType::Sparse);
struct PositionSoA {
	GAIA_LAYOUT(SoA);
	float x, y, z;
};
inline bool operator==(const PositionSoA& a, const PositionSoA& b) {
	return a.x == b.x && a.y == b.y && a.z == b.z;
}

struct PositionSoA8 {
	GAIA_LAYOUT(SoA8);
	float x, y, z;
};
struct PositionSoA16 {
	GAIA_LAYOUT(SoA16);
	float x, y, z;
};
struct Acceleration {
	float x, y, z;
};
struct Rotation {
	float x, y, z, w;
};
struct DummySoA {
	GAIA_LAYOUT(SoA);
	float x, y;
	bool b;
	float w;
};
struct RotationSoA {
	GAIA_LAYOUT(SoA);
	float x, y, z, w;
};
struct RotationSoA8 {
	GAIA_LAYOUT(SoA8);
	float x, y, z, w;
};
struct RotationSoA16 {
	GAIA_LAYOUT(SoA16);
	float x, y, z, w;
};
struct Scale {
	float x, y, z;
};
struct Something {
	bool value;
};
struct Else {
	bool value;
};
struct Empty {};

struct PositionNonTrivial {
	float x, y, z;
	PositionNonTrivial(): x(1), y(2), z(3) {}
	PositionNonTrivial(float xx, float yy, float zz): x(xx), y(yy), z(zz) {}
};

template <typename T>
struct TypeNonTrivial {
	T x;
	TypeNonTrivial(): x(T(1)) {}
	TypeNonTrivial(T xx): x(xx) {}
	operator T() const {
		return x;
	}
};

template <typename T>
struct TypeNonTrivialC {
	T x;
	TypeNonTrivialC(): x(T(1)) {}
	TypeNonTrivialC(T xx): x(xx) {}
	TypeNonTrivialC(const TypeNonTrivialC& xx): x(xx.x) {}
	TypeNonTrivialC& operator=(const TypeNonTrivialC& xx) {
		x = xx.x;
		return *this;
	}
	operator T() const {
		return x;
	}
};

template <typename T>
struct TypeNonTrivialM {
	T x;
	TypeNonTrivialM(): x(T(1)) {}
	TypeNonTrivialM(T xx): x(xx) {}
	TypeNonTrivialM(TypeNonTrivialM&& xx): x(xx.x) {}
	TypeNonTrivialM& operator=(TypeNonTrivialM&& xx) {
		x = xx.x;
		return *this;
	}
	TypeNonTrivialM& operator=(const TypeNonTrivialM& xx) {
		x = xx.x;
		return *this;
	}
	operator T() const {
		return x;
	}
};

//------------------------------------------------------------------------------
// Strings
//------------------------------------------------------------------------------

static constexpr const char* StringComponentDefaultValue =
		"StringComponentDefaultValue_ReasonablyLongSoThatItShouldCauseAHeapAllocationOnAllStdStringImplementations";
static constexpr const char* StringComponent2DefaultValue =
		"StringComponent2DefaultValue_ReasonablyLongSoThatItShouldCauseAHeapAllocationOnAllStdStringImplementations";
static constexpr const char* StringComponent2DefaultValue_2 =
		"2_StringComponent2DefaultValue_ReasonablyLongSoThatItShouldCauseAHeapAllocationOnAllStdStringImplementations";
static constexpr const char* StringComponentEmptyValue =
		"StringComponentEmptyValue_ReasonablyLongSoThatItShouldCauseAHeapAllocationOnAllStdStringImplementations";

struct StringComponent {
	std::string value;

	template <typename Serializer>
	void save(Serializer& s) const {
		const auto len = (uint32_t)value.size();
		s.save(len);
		s.save_raw(value.data(), len, ser::serialization_type_id::c8);
	}

	template <typename Serializer>
	void load(Serializer& s) {
		uint32_t len{};
		s.load(len);
		value.assign(s.data() + s.tell(), len);
		s.seek(s.tell() + len);
	}
};
struct StringComponent2: public StringComponent {
	StringComponent2() {
		value = StringComponent2DefaultValue;
	}
	~StringComponent2() {
		value = StringComponentEmptyValue;
	}

	StringComponent2(const StringComponent2&) = default;
	StringComponent2(StringComponent2&&) noexcept = default;
	StringComponent2& operator=(const StringComponent2&) = default;
	StringComponent2& operator=(StringComponent2&&) noexcept = default;
};
GAIA_DEFINE_HAS_MEMBER_FUNC(foo);
GAIA_DEFINE_HAS_MEMBER_FUNC(food);

template <typename TQuery>
void Test_Query_QueryResult() {
	const uint32_t N = 1'500;

	TestWorld twld;
	cnt::darr<ecs::Entity> ents;
	ents.reserve(N);

	auto create = [&](uint32_t i) {
		auto e = wld.add();
		ents.push_back(e);
		wld.add<Position>(e, {(float)i, (float)i, (float)i});
	};

	GAIA_FOR(N) create(i);

	constexpr bool UseCachedQuery = std::is_same_v<TQuery, ecs::Query>;
	auto q1 = wld.query<UseCachedQuery>().template all<Position>();
	auto q2 = wld.query<UseCachedQuery>().template all<Rotation>();
	auto q3 = wld.query<UseCachedQuery>().template all<Position>().template all<Rotation>();

	{
		const auto cnt = q1.count();
		CHECK(cnt == N);
	}
	{
		cnt::darr<ecs::Entity> arr;
		q1.arr(arr);

		CHECK(arr.size() == N);
		GAIA_EACH(arr) CHECK(arr[i] == ents[i]);

		uint32_t entIdx = 0;
		q1.each([&](ecs::Entity ent) {
			CHECK(ent == arr[entIdx++]);
		});
		entIdx = 0;
		q1.each([&](ecs::Iter& it) {
			auto entView = it.view<ecs::Entity>();
			GAIA_EACH(it) CHECK(entView[i] == arr[entIdx++]);
		});
	}
	{
		cnt::darr<Position> arr;
		q1.arr(arr);
		CHECK(arr.size() == N);
		GAIA_EACH(arr) {
			const auto& pos = arr[i];
			CHECK(pos.x == (float)i);
			CHECK(pos.y == (float)i);
			CHECK(pos.z == (float)i);
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
		CHECK(cnt == cnt2);
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
		CHECK(cnt == cnt2);
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
		CHECK(cnt == cnt3);
	}

	auto game = wld.add();
	struct Level {
		uint32_t value;
	};
	wld.add<Level>(game, {2});
	auto q4 = wld.query<UseCachedQuery>() //
								.template all<Position>()
								.template all<Level>(ecs::QueryTermOptions{}.src(game));

	{
		const auto cnt = q4.count();
		CHECK(cnt == N);
	}
	{
		cnt::darr<ecs::Entity> arr;
		q4.arr(arr);
		CHECK(arr.size() == N);
		GAIA_EACH(arr) CHECK(arr[i] == ents[i]);
	}
	{
		cnt::darr<Position> arr;
		q4.arr(arr);
		CHECK(arr.size() == N);
		GAIA_EACH(arr) {
			const auto& pos = arr[i];
			CHECK(pos.x == (float)i);
			CHECK(pos.y == (float)i);
			CHECK(pos.z == (float)i);
		}
	}
	{
		const auto cnt = q4.count();
		CHECK(cnt > 0);

		const auto empty = q4.empty();
		CHECK(empty == false);

		uint32_t cnt2 = 0;
		q4.each([&]() {
			++cnt2;
		});
		CHECK(cnt == cnt2);
	}
	{
		const auto cnt = q4.count();

		uint32_t cnt2 = 0;
		q4.each([&](ecs::Iter& it /*, ecs::Src src*/) {
			GAIA_EACH(it) {
				++cnt2;
			}
		});
		CHECK(cnt == cnt2);
	}
}

template <typename TQuery>
void expect_exact_entities(TQuery& query, std::initializer_list<ecs::Entity> expected) {
	cnt::darr<ecs::Entity> actual;
	query.each([&](ecs::Entity entity) {
		actual.push_back(entity);
	});

	CHECK(actual.size() == expected.size());
	for (const auto exp: expected) {
		bool found = false;
		for (const auto got: actual) {
			if (got != exp)
				continue;
			found = true;
			break;
		}
		CHECK(found);
	}
}

template <typename TQuery>
void expect_changed_probe_state(TQuery& query, uint32_t expectedCount) {
	CHECK(query.count() == expectedCount);
	CHECK(query.count() == expectedCount);
	CHECK(query.empty() == (expectedCount == 0));
	CHECK(query.empty() == (expectedCount == 0));
}

template <typename TQuery>
void expect_changed_consume_exact(TQuery& query, std::initializer_list<ecs::Entity> expected) {
	expect_exact_entities(query, expected);
	CHECK(query.count() == 0);
	CHECK(query.empty());
}

template <typename T>
void TestDataLayoutSoA_ECS() {
	const uint32_t N = 1'500;

	TestWorld twld;
	cnt::darr<ecs::Entity> ents;
	ents.reserve(N);

	auto create = [&]() {
		auto e = wld.add();
		wld.add<T>(e, {});
		ents.push_back(e);
	};

	GAIA_FOR(N) create();

	SUBCASE("each - iterator") {
		ecs::Query q = wld.query().all<T&>();

		{
			const auto cnt = q.count();
			CHECK(cnt == N);

			uint32_t j = 0;
			q.each([&](ecs::Iter& it) {
				auto t = it.view_mut<T>();
				auto tx = t.template set<0>();
				auto ty = t.template set<1>();
				auto tz = t.template set<2>();
				GAIA_EACH(it) {
					auto f = (float)j;
					tx[i] = f;
					ty[i] = f;
					tz[i] = f;

					CHECK(tx[i] == f);
					CHECK(ty[i] == f);
					CHECK(tz[i] == f);

					++j;
				}
			});
			CHECK(j == cnt);
		}

		{
			auto e = ents[0];
			wld.enable(e, false);
			const auto cnt = q.count();
			CHECK(cnt == N - 1);
			uint32_t cntCalculated = 0;
			q.each([&](ecs::Iter& it) {
				cntCalculated += it.size();
			});
			CHECK(cnt == cntCalculated);
		}
	}
}
