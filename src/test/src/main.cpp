#define NOMINMAX

#include <atomic>
#include <type_traits>

#include <gaia.h>

#if GAIA_COMPILER_MSVC
	#if _MSC_VER <= 1916
// warning C4100: 'XYZ': unreferenced formal parameter
GAIA_MSVC_WARNING_DISABLE(4100)
	#endif
#endif

#define DOCTEST_CONFIG_IMPLEMENT
#define DOCTEST_CONFIG_SUPER_FAST_ASSERTS
#include <doctest/doctest.h>

#include <cstdio>
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
bool operator==(const PositionSoA& a, const PositionSoA& b) {
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
};
struct StringComponent2 {
	std::string value = StringComponent2DefaultValue;

	StringComponent2() = default;
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

TEST_CASE("StringLookupKey") {
	constexpr uint32_t MaxLen = 32;
	char tmp0[MaxLen];
	char tmp1[MaxLen];
	GAIA_STRFMT(tmp0, MaxLen, "%s", "some string");
	GAIA_STRFMT(tmp1, MaxLen, "%s", "some string");
	core::StringLookupKey<128> l0(tmp0);
	core::StringLookupKey<128> l1(tmp1);
	CHECK(l0.len() == l1.len());
	// Two different addresses in memory have to return the same hash if the string is the same
	CHECK(l0.hash() == l1.hash());
}

//------------------------------------------------------------------------------
// Utility functions
//------------------------------------------------------------------------------

struct Dummy0 {
	Dummy0* foo(const Dummy0&) const {
		return nullptr;
	}
	void foo(int dummy) {
		(void)dummy;
	}
};
inline bool operator==(const Dummy0&, const Dummy0&) {
	return true;
}
struct Dummy1 {
	bool operator==(const Dummy1&) const {
		return true;
	}
};

GAIA_DEFINE_HAS_MEMBER_FUNC(some_func_local);
struct HasFuncDummy {
	void some_func_local(int, float*, short&) {}
};

TEST_CASE("has_XYZ_check") {
	SUBCASE("member_func") {
		constexpr auto hasFunc1_0 = has_func_some_func_local<HasFuncDummy, int>::value;
		constexpr auto hasFunc1_1 = has_func_some_func_local<HasFuncDummy, int, float, short>::value;
		constexpr auto hasFunc1_2 = has_func_some_func_local<HasFuncDummy, int, float*, short>::value;
		constexpr auto hasFunc1_3 = has_func_some_func_local<HasFuncDummy, int, float*, short*>::value;
		constexpr auto hasFunc1_4 = has_func_some_func_local<HasFuncDummy, int, float*, short&>::value;
		constexpr auto hasFunc1_5 = has_func_some_func_local<HasFuncDummy, int, float&, short&>::value;
		constexpr auto hasFunc1_6 = has_func_some_func_local<HasFuncDummy, int&, float&, short&>::value;
		CHECK_FALSE(hasFunc1_0);
		static_assert(!hasFunc1_0);
		CHECK_FALSE(hasFunc1_1);
		static_assert(!hasFunc1_1);
		CHECK_FALSE(hasFunc1_2);
		static_assert(!hasFunc1_2);
		CHECK_FALSE(hasFunc1_3);
		static_assert(!hasFunc1_3);
		CHECK(hasFunc1_4);
		static_assert(hasFunc1_4);
		CHECK_FALSE(hasFunc1_5);
		static_assert(!hasFunc1_5);
		CHECK_FALSE(hasFunc1_6);
		static_assert(!hasFunc1_6);
	}
}

TEST_CASE("has_XYZ_equals_check") {
	{
		constexpr auto hasMember = core::has_func_equals<Dummy0>::value;
		constexpr auto hasGlobal = core::has_ffunc_equals<Dummy0>::value;
		constexpr auto hasFoo1 = has_func_foo<Dummy0, const Dummy0&>::value;
		constexpr auto hasFoo2 = has_func_foo<Dummy0, int>::value;
		constexpr auto hasFood = has_func_food<Dummy0>::value;
		CHECK_FALSE(hasMember);
		CHECK(hasGlobal);
		CHECK(hasFoo1);
		CHECK(hasFoo2);
		CHECK_FALSE(hasFood);
	}
	{
		constexpr auto hasMember = core::has_func_equals<Dummy1>::value;
		constexpr auto hasGlobal = core::has_ffunc_equals<Dummy1>::value;
		constexpr auto hasFoo = has_func_foo<Dummy1>::value;
		CHECK(hasMember);
		CHECK_FALSE(hasGlobal);
		CHECK_FALSE(hasFoo);
	}
}

TEST_CASE("Intrinsics") {
	SUBCASE("POPCNT") {
		const uint32_t zero32 = GAIA_POPCNT(0);
		CHECK(zero32 == 0);
		const uint64_t zero64 = GAIA_POPCNT64(0);
		CHECK(zero64 == 0);

		const uint32_t val32 = GAIA_POPCNT(0x0003002);
		CHECK(val32 == 3);
		const uint64_t val64_1 = GAIA_POPCNT64(0x0003002);
		CHECK(val64_1 == 3);
		const uint64_t val64_2 = GAIA_POPCNT64(0x00030020000000);
		CHECK(val64_2 == 3);
		const uint64_t val64_3 = GAIA_POPCNT64(0x00030020003002);
		CHECK(val64_3 == 6);
	}
	SUBCASE("CLZ") {
		const uint32_t zero32 = GAIA_CLZ(0);
		CHECK(zero32 == 32);
		const uint64_t zero64 = GAIA_CLZ64(0);
		CHECK(zero64 == 64);

		const uint32_t val32 = GAIA_CLZ(0x0003002);
		CHECK(val32 == 1);
		const uint64_t val64_1 = GAIA_CLZ64(0x0003002);
		CHECK(val64_1 == 1);
		const uint64_t val64_2 = GAIA_CLZ64(0x00030020000000);
		CHECK(val64_2 == 29);
		const uint64_t val64_3 = GAIA_CLZ64(0x00030020003002);
		CHECK(val64_3 == 1);
	}
	SUBCASE("CTZ") {
		const uint32_t zero32 = GAIA_CTZ(0);
		CHECK(zero32 == 32);
		const uint64_t zero64 = GAIA_CTZ64(0);
		CHECK(zero64 == 64);

		const uint32_t val32 = GAIA_CTZ(0x0003002);
		CHECK(val32 == 18);
		const uint64_t val64_1 = GAIA_CTZ64(0x0003002);
		CHECK(val64_1 == 50);
		const uint64_t val64_2 = GAIA_CTZ64(0x00030020000000);
		CHECK(val64_2 == 22);
		const uint64_t val64_3 = GAIA_CTZ64(0x00030020003002);
		CHECK(val64_3 == 22);
	}
	SUBCASE("FFS") {
		const uint32_t zero32 = GAIA_FFS(0);
		CHECK(zero32 == 0);
		const uint64_t zero64 = GAIA_FFS64(0);
		CHECK(zero64 == 0);

		const uint32_t val32 = GAIA_FFS(0x0003002);
		CHECK(val32 == 2);
		const uint64_t val64_1 = GAIA_FFS64(0x0003002);
		CHECK(val64_1 == 2);
		const uint64_t val64_2 = GAIA_FFS64(0x00030020000000);
		CHECK(val64_2 == 30);
		const uint64_t val64_3 = GAIA_FFS64(0x00030020003002);
		CHECK(val64_3 == 2);
	}
}

TEST_CASE("Memory allocation") {
	SUBCASE("mem_alloc") {
		void* pMem = mem::mem_alloc(311);
		CHECK(pMem != nullptr);
		mem::mem_free(pMem);
	}
	SUBCASE("mem_alloc_alig A") {
		void* pMem = mem::mem_alloc_alig(1, 16);
		CHECK(pMem != nullptr);
		mem::mem_free_alig(pMem);
	}
	SUBCASE("mem_alloc_alig B") {
		void* pMem = mem::mem_alloc_alig(311, 16);
		CHECK(pMem != nullptr);
		mem::mem_free_alig(pMem);
	}
}

TEST_CASE("pow2") {
	SUBCASE("is_pow2") {
		CHECK(core::is_pow2(0));
		CHECK(core::is_pow2(1));
		CHECK(core::is_pow2(2));
		CHECK(core::is_pow2(4));
		CHECK(core::is_pow2(8));
		CHECK(core::is_pow2(16));
		CHECK(core::is_pow2(32));
		CHECK(core::is_pow2(64));
		CHECK_FALSE(core::is_pow2(3));
		CHECK_FALSE(core::is_pow2(5));
		CHECK_FALSE(core::is_pow2(7));
		CHECK_FALSE(core::is_pow2(9));
		CHECK_FALSE(core::is_pow2(11));
		CHECK_FALSE(core::is_pow2(13));
		CHECK_FALSE(core::is_pow2(48));
		CHECK_FALSE(core::is_pow2(96));
		CHECK_FALSE(core::is_pow2(127));
	}

	SUBCASE("closest_pow2") {
		constexpr uint8_t test0 = 0;
		constexpr uint8_t test2 = 2;
		constexpr uint8_t test8 = 19;
		constexpr uint16_t test16 = 1023;
		constexpr uint32_t test32 = 123456;
		constexpr uint64_t test64 = 123456789123456789ULL;

		constexpr auto result0 = core::closest_pow2(test0);
		constexpr auto result2 = core::closest_pow2(test2);
		constexpr auto result8 = core::closest_pow2(test8);
		constexpr auto result16 = core::closest_pow2(test16);
		constexpr auto result32 = core::closest_pow2(test32);
		constexpr auto result64 = core::closest_pow2(test64);

		constexpr auto result0_isPow2 = core::is_pow2(result0);
		constexpr auto result2_isPow2 = core::is_pow2(result2);
		constexpr auto result8_isPow2 = core::is_pow2(result8);
		constexpr auto result16_isPow2 = core::is_pow2(result16);
		constexpr auto result32_isPow2 = core::is_pow2(result32);
		constexpr auto result64_isPow2 = core::is_pow2(result64);

		CHECK(result0_isPow2);
		CHECK(result2_isPow2);
		CHECK(result8_isPow2);
		CHECK(result16_isPow2);
		CHECK(result32_isPow2);
		CHECK(result64_isPow2);
	}
}

TEST_CASE("bit_view") {
	constexpr uint32_t BlockBits = 6;
	using view = core::bit_view<BlockBits>;
	// Number of BlockBits-sized elements
	constexpr uint32_t NBlocks = view::MaxValue + 1;
	// Number of bytes necessary to host "Blocks" number of BlockBits-sized elements
	constexpr uint32_t N = (BlockBits * NBlocks + 7) / 8;

	uint8_t arr[N]{};
	view bv{arr};

	GAIA_FOR(NBlocks) {
		bv.set(i * BlockBits, (uint8_t)i);

		const uint8_t val = bv.get(i * BlockBits);
		CHECK(val == i);
	}

	// Make sure nothing was overriden
	GAIA_FOR(NBlocks) {
		const uint8_t val = bv.get(i * BlockBits);
		CHECK(val == i);
	}
}

TEST_CASE("trim") {
	std::string target = "Gaia-ECS";

	{
		std::string str = "  \t\n  Gaia-ECS  \t\n  ";
		auto t = core::trim({str.c_str(), str.length()});
		CHECK(std::string(t.data(), t.size()) == target);
	}

	{
		std::string str = "Gaia-ECS";
		auto t = core::trim({str.c_str(), str.length()});
		CHECK(std::string(t.data(), t.size()) == target);
	}

	{
		std::string str = "";
		auto t = core::trim(str);
		CHECK(std::string(t.data(), t.size()) == std::string(""));
	}
}

//------------------------------------------------------------------------------
// Containers
//------------------------------------------------------------------------------

TEST_CASE("fwd_llist") {
	struct Foo: cnt::fwd_llist_base<Foo> {
		uint32_t value = 0;
	};

	constexpr uint32_t N = 16;
	cnt::darray<Foo*> foos;
	foos.resize(N);
	GAIA_FOR(N) {
		auto*& f = foos[i];
		f = new Foo();
		f->value = i;
	}

	cnt::fwd_llist<Foo> list;
	GAIA_FOR(N) list.link(foos[i]);

	// Check forward pointers
	{
		CHECK(foos[0]->get_fwd_llist_link().next == nullptr);
		GAIA_FOR(N - 1) {
			CHECK(foos[i + 1]->get_fwd_llist_link().next == foos[i]);
		}
		GAIA_FOR(N) {
			CHECK(foos[i]->get_fwd_llist_link().linked());
		}
	}

	// Check via assigned indices
	{
		uint32_t idx = 0;
		for (const auto& foo: list) {
			const auto foosIdx = foos.size() - idx - 1;
			const auto* f = foos[foosIdx];
			CHECK(foo.value == f->value);
			++idx;
		}
		CHECK(idx == N);
	}

	// Check forward pointers after a node from the middle of the list is removed
	{
		list.unlink(foos[5]);
		CHECK_FALSE(foos[5]->get_fwd_llist_link().linked());
		foos.retain([](const Foo* f) {
			return f->value != 5;
		});

		uint32_t idx = 0;
		for (const auto& foo: list) {
			const auto foosIdx = foos.size() - idx - 1;
			const auto* f = foos[foosIdx];
			CHECK(foo.value == f->value);
			++idx;
		}
		CHECK(idx == N - 1);
	}

	// Check forward pointers after a node at the end is removed
	{
		list.unlink(foos.back());
		CHECK_FALSE(foos.back()->get_fwd_llist_link().linked());
		foos.retain([&](const Foo* f) {
			return f->value != N - 1;
		});

		uint32_t idx = 0;
		for (const auto& foo: list) {
			const auto foosIdx = foos.size() - idx - 1;
			const auto* f = foos[foosIdx];
			CHECK(foo.value == f->value);
			++idx;
		}
		CHECK(idx == N - 2);
	}

	// Check forward pointers after a node at the beginning is removed
	{
		list.unlink(foos.front());
		CHECK_FALSE(foos.front()->get_fwd_llist_link().linked());
		foos.retain([&](const Foo* f) {
			return f->value != 0;
		});

		uint32_t idx = 0;
		for (const auto& foo: list) {
			const auto foosIdx = foos.size() - idx - 1;
			const auto* f = foos[foosIdx];
			CHECK(foo.value == f->value);
			++idx;
		}
		CHECK(idx == N - 3);
	}
}

template <typename Container>
void fixed_arr_test() {
	using cont_item = typename Container::value_type;

	constexpr auto N = Container::extent;
	static_assert(N > 2); // we need at least 2 items to complete this test
	Container arr;

	GAIA_FOR(N) {
		arr[i] = i;
		CHECK(arr[i] == i);
	}
	CHECK(arr.back() == N - 1);
	// Verify the values remain the same even after the internal buffer is reallocated
	GAIA_FOR(N) CHECK(arr[i] == i);
	// Copy assignment
	{
		Container arrCopy = arr;
		GAIA_FOR(N) CHECK(arrCopy[i] == i);
	}
	// Copy constructor
	{
		Container arrCopy(arr);
		GAIA_FOR(N) CHECK(arrCopy[i] == i);
	}
	// Move assignment
	{
		Container arrCopy = GAIA_MOV(arr);
		GAIA_FOR(N) CHECK(arrCopy[i] == i);
		// move back
		arr = GAIA_MOV(arrCopy);
	}
	// Move constructor
	{
		Container arrCopy(GAIA_MOV(arr));
		GAIA_FOR(N) CHECK(arrCopy[i] == i);
		// move back
		arr = GAIA_MOV(arrCopy);
	}

	// Container comparison
	{
		Container arrEmpty(core::zero);
		CHECK_FALSE(arrEmpty == arr);

		Container arr2(arr);
		CHECK(arr2 == arr);
	}

	uint32_t cnt = 0;
	for (auto val: arr) {
		CHECK(val == cnt);
		++cnt;
	}
	CHECK(cnt == N);
	CHECK(cnt == arr.size());

	std::span<const cont_item> view{arr.data(), arr.size()};

	CHECK(core::find(arr, 0U) == arr.begin());
	CHECK(core::find(arr, N) == arr.end());
	CHECK(core::find(view, 0U) == view.begin());
	CHECK(core::find(view, N) == view.end());

	CHECK(core::has(arr, 0U));
	CHECK(core::has(view, 0U));
	CHECK_FALSE(core::has(arr, N));
	CHECK_FALSE(core::has(view, N));
}

TEST_CASE("Containers - sarr") {
	constexpr uint32_t N = 100;
	using TrivialT = cnt::sarr<uint32_t, N>;
	using NonTrivialT = cnt::sarr<TypeNonTrivial<uint32_t>, N>;
	SUBCASE("trivial_types") {
		fixed_arr_test<TrivialT>();
	}
	SUBCASE("non_trivial_types") {
		fixed_arr_test<NonTrivialT>();
	}
}

template <typename Container>
void resizable_arr_test(uint32_t N) {
	using cont_item = typename Container::value_type;

	GAIA_ASSERT(N > 2); // we need at least 2 items to complete this test
	Container arr;

	GAIA_FOR(N) {
		arr.push_back(i);
		CHECK(arr[i] == i);
		CHECK(arr.back() == i);
	}
	// Verify the values remain the same even after the internal buffer is reallocated
	GAIA_FOR(N) CHECK(arr[i] == i);
	// Copy assignment
	{
		Container arrCopy = arr;
		GAIA_FOR(N) CHECK(arrCopy[i] == i);
	}
	// Copy constructor
	{
		Container arrCopy(arr);
		GAIA_FOR(N) CHECK(arrCopy[i] == i);
	}
	// Move assignment
	{
		Container arrCopy = GAIA_MOV(arr);
		GAIA_FOR(N) CHECK(arrCopy[i] == i);
		// move back
		arr = GAIA_MOV(arrCopy);
	}
	// Move constructor
	{
		Container arrCopy(GAIA_MOV(arr));
		GAIA_FOR(N) CHECK(arrCopy[i] == i);
		// move back
		arr = GAIA_MOV(arrCopy);
	}

	// Container comparison
	{
		Container arrEmpty(core::zero);
		CHECK_FALSE(arrEmpty == arr);

		Container arr2(arr);
		CHECK(arr2 == arr);
	}

	uint32_t cnt = 0;
	for (auto val: arr) {
		CHECK(val == cnt);
		++cnt;
	}
	CHECK(cnt == N);
	CHECK(cnt == arr.size());

	std::span<const cont_item> view{arr.data(), arr.size()};

	CHECK(core::find(arr, 0U) == arr.begin());
	CHECK(core::find(arr, N) == arr.end());
	CHECK(core::find(view, 0U) == view.begin());
	CHECK(core::find(view, N) == view.end());

	CHECK(core::has(arr, 0U));
	CHECK(core::has(view, 0U));
	CHECK_FALSE(core::has(arr, N));
	CHECK_FALSE(core::has(view, N));

	arr.erase(arr.begin());
	CHECK(arr.size() == (N - 1));
	CHECK(core::find(arr, 0U) == arr.end());
	GAIA_EACH(arr)
	CHECK(arr[i] == i + 1);

	arr.clear();
	CHECK(arr.empty());

	arr.push_back(11);
	arr.erase(arr.begin());
	CHECK(arr.empty());

	arr.push_back(11);
	arr.push_back(12);
	arr.push_back(13);
	arr.push_back(14);
	arr.push_back(15);
	arr.erase(arr.begin() + 1, arr.begin() + 4);
	CHECK(arr.size() == 2);
	CHECK(arr[0] == 11);
	CHECK(arr[1] == 15);

	arr.erase(arr.begin(), arr.end());
	CHECK(arr.empty());

	arr.push_back(11);
	arr.push_back(12);
	arr.push_back(13);
	arr.push_back(14);
	arr.push_back(15);
	arr.erase(arr.begin() + 1);
	CHECK(arr.size() == 4);
	CHECK(arr[0] == 11);
	CHECK(arr[1] == 13);
	CHECK(arr[2] == 14);
	CHECK(arr[3] == 15);
	arr.erase(arr.begin() + 3);
	CHECK(arr.size() == 3);
	CHECK(arr[0] == 11);
	CHECK(arr[1] == 13);
	CHECK(arr[2] == 14);
	arr.erase(arr.begin());
	arr.erase(arr.begin());
	arr.erase(arr.begin());
	CHECK(arr.size() == 0);
	CHECK(arr.empty());

	// 11, 13, 14, 15
	// 11, 13, 13, 14, 15
	// 11, [12], 13, 14, 15
	arr.push_back(11);
	arr.push_back(13);
	arr.push_back(14);
	arr.insert(arr.begin() + 1, 12);
	CHECK(arr.size() == 4);
	CHECK(arr[0] == 11);
	CHECK(arr[1] == 12);
	CHECK(arr[2] == 13);
	CHECK(arr[3] == 14);

	arr.insert(arr.begin(), 10);
	CHECK(arr.size() == 5);
	CHECK(arr[0] == 10);
	CHECK(arr[1] == 11);
	CHECK(arr[2] == 12);
	CHECK(arr[3] == 13);
	CHECK(arr[4] == 14);
}

template <typename Container>
void retainable_arr_test() {
	using cont_item = typename Container::value_type;

	constexpr cont_item original[] = {2, 5, 10, 8, 9, 21, 22, 24, 36, 7};
	constexpr cont_item expected[] = {2, 10, 8, 22, 24, 36};

	constexpr auto cntOriginal = sizeof(original) / sizeof(cont_item);
	constexpr auto cntExpected = sizeof(expected) / sizeof(cont_item);

	Container arr;
	arr.resize(cntOriginal);
	GAIA_EACH(arr) arr[i] = original[i];

	arr.retain([](const cont_item& item) {
		// Keep only even numbers
		return item % 2 == 0;
	});

	CHECK(arr.size() == cntExpected);
	GAIA_EACH(arr) {
		CHECK(arr[i] == expected[i]);
	}
}

TEST_CASE("Containers - sarr_ext") {
	constexpr uint32_t N = 100;
	using TrivialT = cnt::sarr_ext<uint32_t, 100>;
	using NonTrivialT = cnt::sarr_ext<TypeNonTrivial<uint32_t>, 100>;
	SUBCASE("trivial_types") {
		resizable_arr_test<TrivialT>(N);
	}
	SUBCASE("non_trivial_types") {
		resizable_arr_test<NonTrivialT>(N);
	}
	SUBCASE("retain") {
		retainable_arr_test<TrivialT>();
	}
}

TEST_CASE("Containers - darr") {
	constexpr uint32_t N = 100;
	constexpr uint32_t M = 10000;
	using TrivialT = cnt::darr<uint32_t>;
	using NonTrivialT = cnt::darr<TypeNonTrivial<uint32_t>>;
	SUBCASE("trivial_types") {
		resizable_arr_test<TrivialT>(N);
		resizable_arr_test<TrivialT>(M);
	}
	SUBCASE("non_trivial_types") {
		resizable_arr_test<NonTrivialT>(N);
		resizable_arr_test<NonTrivialT>(M);
	}
	SUBCASE("retain") {
		retainable_arr_test<TrivialT>();
	}
}

TEST_CASE("Containers - darr_ext") {
	constexpr uint32_t N = 100;
	constexpr uint32_t M = 10000;
	using TrivialT1 = cnt::darr_ext<uint32_t, 50>;
	using TrivialT2 = cnt::darr_ext<uint32_t, 100>;
	using NonTrivialT1 = cnt::darr_ext<TypeNonTrivial<uint32_t>, 50>;
	using NonTrivialT2 = cnt::darr_ext<TypeNonTrivial<uint32_t>, 100>;

	SUBCASE("trivial_types") {
		resizable_arr_test<TrivialT1>(N);
		resizable_arr_test<TrivialT1>(M);

		resizable_arr_test<TrivialT2>(N);
		resizable_arr_test<TrivialT2>(M);
	}
	SUBCASE("non_trivial_types") {
		resizable_arr_test<NonTrivialT1>(N);
		resizable_arr_test<NonTrivialT1>(M);

		resizable_arr_test<NonTrivialT2>(N);
		resizable_arr_test<NonTrivialT2>(M);
	}
	SUBCASE("retain") {
		retainable_arr_test<TrivialT1>();
	}
}

//------------------------------------------------------------------------------
// Signals
//------------------------------------------------------------------------------

using sig_func_t = void(ecs::Entity, ecs::Entity, uint32_t&);
void test_func([[maybe_unused]] ecs::Entity e1, [[maybe_unused]] ecs::Entity e2, uint32_t& cnt) {
	GAIA_ASSERT(e1 == e2);
	++cnt;
}

struct SigFoo {
	void on_event([[maybe_unused]] ecs::Entity e1, [[maybe_unused]] ecs::Entity e2, uint32_t& cnt) {
		GAIA_ASSERT(e1 == e2);
		++cnt;
	}
};

TEST_CASE("Delegates") {
	TestWorld twld;
	auto e1 = wld.add();
	auto e2 = wld.add();
	(void)e2;

	// free function
	{
		util::delegate<sig_func_t> d;
		CHECK_FALSE(d.operator bool());
		d.bind<test_func>();
		CHECK(d.operator bool());

		uint32_t i = 0;
		d(e1, e1, i);
		CHECK(i == 1);

		d.reset();
		CHECK_FALSE(d.operator bool());
	}

	// class
	{
		SigFoo f;
		util::delegate<sig_func_t> d;
		CHECK_FALSE(d.operator bool());
		d.bind<&SigFoo::on_event>(f);
		CHECK(d.operator bool());

		uint32_t i = 0;
		d(e1, e1, i);
		CHECK(i == 1);
	}

	// non-capturing lambda-like construct
	{
		struct dummyCtx {
			void operator()([[maybe_unused]] ecs::Entity e1, [[maybe_unused]] ecs::Entity e2, uint32_t& cnt) {
				GAIA_ASSERT(e1 == e2);
				++cnt;
			}
		} dummy;
		util::delegate<sig_func_t> d;
		d.bind<&dummyCtx::operator()>(dummy);

		uint32_t i = 0;
		d(e1, e1, i);
		CHECK(i);
	}
}

TEST_CASE("Signals") {
	TestWorld twld;
	auto e1 = wld.add();
	auto e2 = wld.add();
	auto e3 = wld.add();
	util::signal<sig_func_t> sig;

	{
		util::sink<sig_func_t> sink{sig};
		CHECK(sig.size() == 0);
		CHECK(sig.empty());

		// No bindings, zero expected
		uint32_t cnt = 0;
		sig.emit(e1, e1, cnt);
		sig.emit(e2, e2, cnt);
		sig.emit(e3, e3, cnt);
		CHECK(cnt == 0);

		// Free function bound
		cnt = 0;
		sink.bind<test_func>();
		CHECK(sig.size() == 1);
		CHECK(!sig.empty());
		sig.emit(e1, e1, cnt);
		sig.emit(e2, e2, cnt);
		sig.emit(e3, e3, cnt);
		CHECK(cnt == 3);

		// Unbind the function, zero expected
		cnt = 0;
		sink.unbind<test_func>();
		CHECK(sig.size() == 0);
		CHECK(sig.empty());
		sig.emit(e1, e1, cnt);
		sig.emit(e2, e2, cnt);
		sig.emit(e3, e3, cnt);
		CHECK(cnt == 0);

		// Bind again
		cnt = 0;
		sink.bind<test_func>();
		CHECK(sig.size() == 1);
		CHECK(!sig.empty());
		sig.emit(e1, e1, cnt);
		sig.emit(e2, e2, cnt);
		sig.emit(e3, e3, cnt);
		CHECK(cnt == 3);

		// Reset the sink object, zero expected
		cnt = 0;
		sink.reset();
		CHECK(sig.size() == 0);
		CHECK(sig.empty());
		sig.emit(e1, e1, cnt);
		sig.emit(e2, e2, cnt);
		sig.emit(e3, e3, cnt);
		CHECK(cnt == 0);
	}
	{
		SigFoo f;

		util::sink<sig_func_t> sink{sig};
		CHECK(sig.size() == 0);
		CHECK(sig.empty());

		// No bindings, zero expected
		uint32_t cnt = 0;
		sig.emit(e1, e1, cnt);
		sig.emit(e2, e2, cnt);
		sig.emit(e3, e3, cnt);
		CHECK(cnt == 0);

		// Free function bound
		cnt = 0;
		sink.bind<&SigFoo::on_event>(f);
		CHECK(sig.size() == 1);
		CHECK(!sig.empty());
		sig.emit(e1, e1, cnt);
		sig.emit(e2, e2, cnt);
		sig.emit(e3, e3, cnt);
		CHECK(cnt == 3);

		// Unbind the function, zero expected
		cnt = 0;
		sink.unbind<&SigFoo::on_event>(f);
		CHECK(sig.size() == 0);
		CHECK(sig.empty());
		sig.emit(e1, e1, cnt);
		sig.emit(e2, e2, cnt);
		sig.emit(e3, e3, cnt);
		CHECK(cnt == 0);

		// Bind again
		cnt = 0;
		sink.bind<&SigFoo::on_event>(f);
		CHECK(sig.size() == 1);
		CHECK(!sig.empty());
		sig.emit(e1, e1, cnt);
		sig.emit(e2, e2, cnt);
		sig.emit(e3, e3, cnt);
		CHECK(cnt == 3);

		// Reset the sink object, zero expected
		cnt = 0;
		sink.reset();
		CHECK(sig.size() == 0);
		CHECK(sig.empty());
		sig.emit(e1, e1, cnt);
		sig.emit(e2, e2, cnt);
		sig.emit(e3, e3, cnt);
		CHECK(cnt == 0);
	}
}

//------------------------------------------------------------------------------
// Allocators and storages
//------------------------------------------------------------------------------

TEST_CASE("StackAllocator") {
	struct TFoo {
		int a;
		bool b;
	};

	mem::StackAllocator a;
	{
		auto* pPosN = a.alloc<PositionNonTrivial>(3);
		GAIA_FOR(3) {
			CHECK(pPosN[i].x == 1);
			CHECK(pPosN[i].y == 2);
			CHECK(pPosN[i].z == 3);
		}
	}
	a.reset();
	{
		auto* pInt = a.alloc<int>(1);
		*pInt = 10;
		a.free(pInt, 1);
		pInt = a.alloc<int>(1);
		CHECK(*pInt == 10);

		auto* pPos = a.alloc<Position>(10);
		GAIA_FOR(10) {
			pPos[i].x = (float)i;
			pPos[i].y = (float)i + 1.f;
			pPos[i].z = (float)i + 2.f;
		}
		a.free(pPos, 10);
		pPos = a.alloc<Position>(10);
		GAIA_FOR(10) {
			CHECK(pPos[i].x == (float)i);
			CHECK(pPos[i].y == (float)i + 1.f);
			CHECK(pPos[i].z == (float)i + 2.f);
		}

		auto* pPosN = a.alloc<PositionNonTrivial>(3);
		GAIA_FOR(3) {
			CHECK(pPosN[i].x == 1.f);
			CHECK(pPosN[i].y == 2.f);
			CHECK(pPosN[i].z == 3.f);
		}

		// Alloc and release some more objects
		auto* pFoo = a.alloc<TFoo>(1);
		auto* pInt5 = a.alloc<int>(5);
		a.free(pInt5, 5);
		a.free(pFoo, 1);

		// Make sure the previously stored positions are still intact
		GAIA_FOR(3) {
			CHECK(pPosN[i].x == 1);
			CHECK(pPosN[i].y == 2);
			CHECK(pPosN[i].z == 3);
		}
	}
}

struct SparseTestItem {
	uint32_t id;
	uint32_t data;
};
bool operator==(const SparseTestItem& a, const SparseTestItem& b) {
	return a.id == b.id && a.data == b.data;
}

struct SparseTestItem_NonTrivial {
	uint32_t id;
	uint32_t data;

	SparseTestItem_NonTrivial(): id(0), data(0) {}
	SparseTestItem_NonTrivial(uint32_t xx, uint32_t yy): id(xx), data(yy) {}
};
bool operator==(const SparseTestItem_NonTrivial& a, const SparseTestItem_NonTrivial& b) {
	return a.id == b.id && a.data == b.data;
}

namespace gaia {
	namespace cnt {
		template <>
		struct to_sparse_id<SparseTestItem> {
			static sparse_id get(const SparseTestItem& item) noexcept {
				return item.id;
			}
		};
		template <>
		struct to_sparse_id<SparseTestItem_NonTrivial> {
			static sparse_id get(const SparseTestItem_NonTrivial& item) noexcept {
				return item.id;
			}
		};
		template <>
		struct to_page_storage_id<SparseTestItem> {
			static page_storage_id get(const SparseTestItem& item) noexcept {
				return item.id;
			}
		};
		template <>
		struct to_page_storage_id<SparseTestItem_NonTrivial> {
			static page_storage_id get(const SparseTestItem_NonTrivial& item) noexcept {
				return item.id;
			}
		};
	} // namespace cnt
} // namespace gaia

template <typename Container>
void sparse_storage_test(uint32_t N) {
	using cont_item = typename Container::value_type;

	constexpr uint32_t CONV = 100;
	auto to_sid = [&](uint32_t i) {
		return i * CONV;
	};
	auto new_item = [to_sid](uint32_t i) {
		return cont_item{to_sid(i), i};
	};

	GAIA_ASSERT(N > 2); // we need at least 2 items to complete this test
	Container arr;

	GAIA_FOR(N) {
		arr.add(new_item(i));
		CHECK(arr[to_sid(i)].data == i);
		CHECK(arr.back().data == i);
	}

	// Verify the values remain the same even after the internal buffer is reallocated
	GAIA_FOR(N) CHECK(arr[to_sid(i)].data == i);
	// Copy assignment
	{
		Container arrCopy = arr;
		GAIA_FOR(N) {
			const auto& item = arrCopy[to_sid(i)];
			CHECK(item.data == i);
		}
	}
	// Copy constructor
	{
		Container arrCopy(arr);
		GAIA_FOR(N) CHECK(arrCopy[to_sid(i)].data == i);
	}
	// Move assignment
	{
		Container arrCopy = GAIA_MOV(arr);
		GAIA_FOR(N) CHECK(arrCopy[to_sid(i)].data == i);
		// move back
		arr = GAIA_MOV(arrCopy);
	}
	// Move constructor
	{
		Container arrCopy(GAIA_MOV(arr));
		GAIA_FOR(N) CHECK(arrCopy[to_sid(i)].data == i);
		// move back
		arr = GAIA_MOV(arrCopy);
	}

	// Container comparison
	{
		Container arrEmpty;
		CHECK_FALSE(arrEmpty == arr);

		Container arr2(arr);
		CHECK(arr2 == arr);
	}

	uint32_t cnt = 0;
	for (auto val: arr) {
		CHECK(val.id == to_sid(cnt));
		CHECK(val.data == cnt);
		++cnt;
	}
	CHECK(cnt == N);
	CHECK(cnt == arr.size());

	CHECK(core::find(arr, cont_item{0U, 0U}) == arr.begin());
	CHECK(core::find(arr, cont_item{N, N}) == arr.end());
	CHECK(core::has(arr, cont_item{0U, 0U}));
	CHECK_FALSE(core::has(arr, cont_item{N, N}));

	// ------------------

	arr.clear();
	CHECK(arr.empty());
	CHECK(arr.size() == 0);

	arr.add(new_item(11));
	arr.add(new_item(12));
	arr.add(new_item(13));
	arr.add(new_item(14));
	arr.add(new_item(15));

	CHECK_FALSE(arr.empty());
	CHECK(arr.size() == 5);

	arr.del(new_item(13));
	CHECK(arr.size() == 4);
	CHECK(arr[to_sid(11)].data == 11);
	CHECK(arr[to_sid(12)].data == 12);
	CHECK(arr[to_sid(14)].data == 14);
	CHECK(arr[to_sid(15)].data == 15);

	arr.del(new_item(11));
	CHECK(arr.size() == 3);
	CHECK(arr[to_sid(12)].data == 12);
	CHECK(arr[to_sid(14)].data == 14);
	CHECK(arr[to_sid(15)].data == 15);

	arr.del(new_item(15));
	CHECK(arr.size() == 2);
	CHECK(arr[to_sid(12)].data == 12);
	CHECK(arr[to_sid(14)].data == 14);

	arr.add(new_item(9));
	CHECK(arr.size() == 3);
	CHECK(arr[to_sid(9)].data == 9);
	CHECK(arr[to_sid(12)].data == 12);
	CHECK(arr[to_sid(14)].data == 14);

	arr.add(new_item(9000));
	CHECK(arr.size() == 4);
	CHECK(arr[to_sid(9)].data == 9);
	CHECK(arr[to_sid(12)].data == 12);
	CHECK(arr[to_sid(14)].data == 14);
	CHECK(arr[to_sid(9000)].data == 9000);

	arr.add(new_item(9001));
	arr.add(new_item(9002));
	arr.add(new_item(9003));
	arr.add(new_item(9030));
	CHECK(arr.size() == 8);
	CHECK(arr[to_sid(9)].data == 9);
	CHECK(arr[to_sid(12)].data == 12);
	CHECK(arr[to_sid(14)].data == 14);
	CHECK(arr[to_sid(9000)].data == 9000);
	CHECK(arr[to_sid(9001)].data == 9001);
	CHECK(arr[to_sid(9002)].data == 9002);
	CHECK(arr[to_sid(9003)].data == 9003);
	CHECK(arr[to_sid(9030)].data == 9030);

	arr.del(new_item(9002));
	CHECK(arr.size() == 7);
	CHECK(arr[to_sid(9)].data == 9);
	CHECK(arr[to_sid(12)].data == 12);
	CHECK(arr[to_sid(14)].data == 14);
	CHECK(arr[to_sid(9000)].data == 9000);
	CHECK(arr[to_sid(9001)].data == 9001);
	CHECK(arr[to_sid(9003)].data == 9003);
	CHECK(arr[to_sid(9030)].data == 9030);

	{
		uint32_t indices[] = {14, 12, 9, 9000, 9001, 9030, 9003};
		cnt = 0;
		for (auto val: arr) {
			const auto id = indices[cnt];
			CHECK(val.id == to_sid(id));
			CHECK(val.data == id);
			++cnt;
		}
		CHECK(cnt == 7);
	}

	auto& ref = arr.set(to_sid(14));
	ref.data = 400;
	CHECK(arr.size() == 7);
	CHECK(arr[to_sid(9)].data == 9);
	CHECK(arr[to_sid(12)].data == 12);
	CHECK(arr[to_sid(14)].data == 400);
	CHECK(arr[to_sid(9000)].data == 9000);
	CHECK(arr[to_sid(9001)].data == 9001);
	CHECK(arr[to_sid(9003)].data == 9003);
	CHECK(arr[to_sid(9030)].data == 9030);

	{
		uint32_t indices[] = {14, 12, 9, 9000, 9001, 9030, 9003};
		uint32_t values[] = {400, 12, 9, 9000, 9001, 9030, 9003};
		cnt = 0;
		for (auto val: arr) {
			CHECK(val.id == to_sid(indices[cnt]));
			CHECK(val.data == values[cnt]);
			++cnt;
		}
		CHECK(cnt == 7);
	}

	ref.data = 4000;
	CHECK(arr.size() == 7);
	CHECK(arr[to_sid(9)].data == 9);
	CHECK(arr[to_sid(12)].data == 12);
	CHECK(arr[to_sid(14)].data == 4000);
	CHECK(arr[to_sid(9000)].data == 9000);
	CHECK(arr[to_sid(9001)].data == 9001);
	CHECK(arr[to_sid(9003)].data == 9003);
	CHECK(arr[to_sid(9030)].data == 9030);
}

template <typename Container>
void sparse_storage_test_tag(uint32_t N) {
	constexpr uint32_t CONV = 100;
	auto to_sid = [&](uint32_t i) {
		return i * CONV;
	};

	GAIA_ASSERT(N > 2); // we need at least 2 items to complete this test
	Container arr;

	GAIA_FOR(N) {
		arr.add(to_sid(i));
		CHECK(arr.has(to_sid(i)));
	}

	// Verify the values remain the same even after all the adds
	GAIA_FOR(N) CHECK(arr.has(to_sid(i)));

	// Copy assignment
	{
		Container arrCopy = arr;
		GAIA_FOR(N) CHECK(arrCopy.has(to_sid(i)));
	}
	// Copy constructor
	{
		Container arrCopy(arr);
		GAIA_FOR(N) CHECK(arrCopy.has(to_sid(i)));
	}
	// Move assignment
	{
		Container arrCopy = GAIA_MOV(arr);
		GAIA_FOR(N) CHECK(arrCopy.has(to_sid(i)));
		// move back
		arr = GAIA_MOV(arrCopy);
	}
	// Move constructor
	{
		Container arrCopy(GAIA_MOV(arr));
		GAIA_FOR(N) CHECK(arrCopy.has(to_sid(i)));
		// move back
		arr = GAIA_MOV(arrCopy);
	}

	// Container comparison
	{
		Container arrEmpty;
		CHECK_FALSE(arrEmpty == arr);

		Container arr2(arr);
		CHECK(arr2 == arr);
	}

	uint32_t cnt = 0;
	for (const auto& val: arr) {
		CHECK(val == to_sid(cnt));
		++cnt;
	}
	CHECK(cnt == N);
	CHECK(cnt == arr.size());

	// ------------------

	arr.clear();
	CHECK(arr.empty());
	CHECK(arr.size() == 0);

	arr.add(to_sid(11));
	arr.add(to_sid(12));
	arr.add(to_sid(13));
	arr.add(to_sid(14));
	arr.add(to_sid(15));

	CHECK_FALSE(arr.empty());
	CHECK(arr.size() == 5);

	arr.del(to_sid(13));
	CHECK(arr.size() == 4);
	CHECK(arr.has(to_sid(11)));
	CHECK(arr.has(to_sid(12)));
	CHECK(arr.has(to_sid(14)));
	CHECK(arr.has(to_sid(15)));

	arr.del(to_sid(11));
	CHECK(arr.size() == 3);
	CHECK(arr.has(to_sid(12)));
	CHECK(arr.has(to_sid(14)));
	CHECK(arr.has(to_sid(15)));

	arr.del(to_sid(15));
	CHECK(arr.size() == 2);
	CHECK(arr.has(to_sid(12)));
	CHECK(arr.has(to_sid(14)));

	arr.add(to_sid(9));
	CHECK(arr.size() == 3);
	CHECK(arr.has(to_sid(9)));
	CHECK(arr.has(to_sid(12)));
	CHECK(arr.has(to_sid(14)));

	arr.add(to_sid(9001));
	arr.add(to_sid(9002));
	arr.add(to_sid(9003));
	arr.add(to_sid(9030));
	CHECK(arr.size() == 7);
	CHECK(arr.has(to_sid(9)));
	CHECK(arr.has(to_sid(12)));
	CHECK(arr.has(to_sid(14)));
	CHECK(arr.has(to_sid(9001)));
	CHECK(arr.has(to_sid(9002)));
	CHECK(arr.has(to_sid(9003)));
	CHECK(arr.has(to_sid(9030)));

	arr.del(to_sid(9002));
	CHECK(arr.size() == 6);
	CHECK(arr.has(to_sid(9)));
	CHECK(arr.has(to_sid(12)));
	CHECK(arr.has(to_sid(14)));
	CHECK(arr.has(to_sid(9001)));
	CHECK(arr.has(to_sid(9003)));
	CHECK(arr.has(to_sid(9030)));

	{
		uint32_t indices[] = {14, 12, 9, 9001, 9030, 9003};
		cnt = 0;
		for (auto val: arr) {
			const auto id = indices[cnt];
			CHECK(val == to_sid(id));
			++cnt;
		}
		CHECK(cnt == 6);
	}
}

TEST_CASE("Containers - sparse_storage") {
	constexpr uint32_t N = 100;
	constexpr uint32_t M = 10000;
	using TagT = cnt::sparse_storage<Empty>;
	using TrivialT = cnt::sparse_storage<SparseTestItem>;
	using NonTrivialT = cnt::sparse_storage<SparseTestItem_NonTrivial>;
	SUBCASE("trivial_types") {
		sparse_storage_test_tag<TagT>(N);
	}
	SUBCASE("trivial_types") {
		sparse_storage_test<TrivialT>(N);
		sparse_storage_test<TrivialT>(M);
	}
	SUBCASE("non_trivial_types") {
		sparse_storage_test<NonTrivialT>(N);
		sparse_storage_test<NonTrivialT>(M);
	}
}

template <typename Container>
void paged_storage_test(uint32_t N) {
	using cont_item = typename Container::value_type;

	auto to_sid = [&](uint32_t i) {
		return i;
	};
	auto new_item = [&](uint32_t i) {
		return cont_item{to_sid(i), i};
	};

	GAIA_ASSERT(N > 2); // we need at least 2 items to complete this test
	Container arr;

	GAIA_FOR(N) {
		arr.add(new_item(i));
		CHECK(arr[to_sid(i)].data == i);
		CHECK(arr.back().data == i);
	}

	// Verify the values remain the same even after the internal buffer is reallocated
	GAIA_FOR(N) CHECK(arr[to_sid(i)].data == i);
	// Copy assignment
	{
		Container arrCopy = arr;
		GAIA_FOR(N) {
			const auto& item = arrCopy[to_sid(i)];
			CHECK(item.data == i);
		}
	}
	// Copy constructor
	{
		Container arrCopy(arr);
		GAIA_FOR(N) CHECK(arrCopy[to_sid(i)].data == i);
	}
	// Move assignment
	{
		Container arrCopy = GAIA_MOV(arr);
		GAIA_FOR(N) CHECK(arrCopy[to_sid(i)].data == i);
		// move back
		arr = GAIA_MOV(arrCopy);
	}
	// Move constructor
	{
		Container arrCopy(GAIA_MOV(arr));
		GAIA_FOR(N) CHECK(arrCopy[to_sid(i)].data == i);
		// move back
		arr = GAIA_MOV(arrCopy);
	}

	// Container comparison
	{
		Container arrEmpty;
		CHECK_FALSE(arrEmpty == arr);

		Container arr2(arr);
		CHECK(arr2 == arr);
	}

	uint32_t cnt = 0;
	for (auto val: arr) {
		CHECK(val.id == to_sid(cnt));
		CHECK(val.data == cnt);
		++cnt;
	}
	CHECK(cnt == N);
	CHECK(cnt == arr.size());

	CHECK(core::find(arr, cont_item{0U, 0U}) == arr.begin());
	CHECK(core::find(arr, cont_item{N, N}) == arr.end());
	CHECK(core::has(arr, cont_item{0U, 0U}));
	CHECK_FALSE(core::has(arr, cont_item{N, N}));

	// ------------------

	arr.clear();
	CHECK(arr.empty());
	CHECK(arr.size() == 0);

	arr.add(new_item(11));
	arr.add(new_item(12));
	arr.add(new_item(13));
	arr.add(new_item(14));
	arr.add(new_item(15));

	CHECK_FALSE(arr.empty());
	CHECK(arr.size() == 5);

	arr.del(new_item(13));
	CHECK(arr.size() == 4);
	CHECK(arr[to_sid(11)].data == 11);
	CHECK(arr[to_sid(12)].data == 12);
	CHECK(arr[to_sid(14)].data == 14);
	CHECK(arr[to_sid(15)].data == 15);

	arr.del(new_item(11));
	CHECK(arr.size() == 3);
	CHECK(arr[to_sid(12)].data == 12);
	CHECK(arr[to_sid(14)].data == 14);
	CHECK(arr[to_sid(15)].data == 15);

	arr.del(new_item(15));
	CHECK(arr.size() == 2);
	CHECK(arr[to_sid(12)].data == 12);
	CHECK(arr[to_sid(14)].data == 14);

	arr.add(new_item(9));
	CHECK(arr.size() == 3);
	CHECK(arr[to_sid(9)].data == 9);
	CHECK(arr[to_sid(12)].data == 12);
	CHECK(arr[to_sid(14)].data == 14);

	arr.add(new_item(4000));
	CHECK(arr.size() == 4);
	CHECK(arr[to_sid(9)].data == 9);
	CHECK(arr[to_sid(12)].data == 12);
	CHECK(arr[to_sid(14)].data == 14);
	CHECK(arr[to_sid(4000)].data == 4000);

	arr.add(new_item(4001));
	arr.add(new_item(4002));
	arr.add(new_item(4003));
	arr.add(new_item(4030));
	CHECK(arr.size() == 8);
	CHECK(arr[to_sid(9)].data == 9);
	CHECK(arr[to_sid(12)].data == 12);
	CHECK(arr[to_sid(14)].data == 14);
	CHECK(arr[to_sid(4000)].data == 4000);
	CHECK(arr[to_sid(4001)].data == 4001);
	CHECK(arr[to_sid(4002)].data == 4002);
	CHECK(arr[to_sid(4003)].data == 4003);
	CHECK(arr[to_sid(4030)].data == 4030);

	arr.del(new_item(4002));
	CHECK(arr.size() == 7);
	CHECK(arr[to_sid(9)].data == 9);
	CHECK(arr[to_sid(12)].data == 12);
	CHECK(arr[to_sid(14)].data == 14);
	CHECK(arr[to_sid(4000)].data == 4000);
	CHECK(arr[to_sid(4001)].data == 4001);
	CHECK(arr[to_sid(4003)].data == 4003);
	CHECK(arr[to_sid(4030)].data == 4030);

	{
		uint32_t indices[] = {9, 12, 14, 4000, 4001, 4003, 4030};
		cnt = 0;
		for (auto val: arr) {
			const auto id = indices[cnt];
			CHECK(val.id == to_sid(id));
			CHECK(val.data == id);
			++cnt;
		}
		CHECK(cnt == 7);
	}

	auto& ref = arr.set(to_sid(14));
	ref.data = 400;
	CHECK(arr.size() == 7);
	CHECK(arr[to_sid(9)].data == 9);
	CHECK(arr[to_sid(12)].data == 12);
	CHECK(arr[to_sid(14)].data == 400);
	CHECK(arr[to_sid(4000)].data == 4000);
	CHECK(arr[to_sid(4001)].data == 4001);
	CHECK(arr[to_sid(4003)].data == 4003);
	CHECK(arr[to_sid(4030)].data == 4030);

	{
		uint32_t indices[] = {9, 12, 14, 4000, 4001, 4003, 4030};
		uint32_t values[] = {9, 12, 400, 4000, 4001, 4003, 4030};
		cnt = 0;
		for (auto val: arr) {
			CHECK(val.id == to_sid(indices[cnt]));
			CHECK(val.data == values[cnt]);
			++cnt;
		}
		CHECK(cnt == 7);
	}

	ref.data = 200;
	CHECK(arr.size() == 7);
	CHECK(arr[to_sid(9)].data == 9);
	CHECK(arr[to_sid(12)].data == 12);
	CHECK(arr[to_sid(14)].data == 200);
	CHECK(arr[to_sid(4000)].data == 4000);
	CHECK(arr[to_sid(4001)].data == 4001);
	CHECK(arr[to_sid(4003)].data == 4003);
	CHECK(arr[to_sid(4030)].data == 4030);
}

TEST_CASE("Containers - paged storage") {
	constexpr uint32_t N = 100;
	constexpr uint32_t M = 10000;
	using TrivialT = cnt::page_storage<SparseTestItem>;
	using NonTrivialT = cnt::page_storage<SparseTestItem_NonTrivial>;
	SUBCASE("trivial_types") {
		paged_storage_test<TrivialT>(N);
		paged_storage_test<TrivialT>(M);
	}
	SUBCASE("non_trivial_types") {
		paged_storage_test<NonTrivialT>(N);
		paged_storage_test<NonTrivialT>(M);
	}

	// Only for coverage.
	// Hide logging so it does not spam the results of unit testing.
	const auto logLevelBackup = util::g_logLevelMask;
	util::g_logLevelMask = 0;
	TrivialT::Allocator::get().flush();
	TrivialT::Allocator::get().diag();
	NonTrivialT::Allocator::get().flush();
	NonTrivialT::Allocator::get().diag();
	util::g_logLevelMask = logLevelBackup;
}

TEST_CASE("Containers - alignment check") {
	using TArrInter = cnt::sarr<ecs::QueryTerm, 3>;
	struct TFoo {
		uint8_t b;
		TArrInter arr;

		bool operator==(const TFoo& other) const {
			return b == other.b && arr == other.arr;
		}
	};
	using TArr = cnt::sarr_ext<TFoo, 100>;
	TArr arr;
	arr.resize(2);

	// Make sure alignment is right
	{
		using TPolicy = mem::data_view_policy_aos<ecs::QueryTerm>;
		constexpr auto TPolicyAlign = TPolicy::Alignment;
		const auto addr = (uintptr_t)arr.data();
		CHECK(addr % TPolicyAlign == 0);
	}

	{
		auto& a = arr[0];
		a.b = 16;
		a.arr = {{ecs::Entity(1, 2), {}, {}, {}}, {ecs::Entity(2, 30), {}, {}, {}}, {ecs::Entity(3, 400), {}, {}, {}}};
	}
	{
		auto& a = arr[1];
		a.b = 214;
		a.arr = {{ecs::Entity(10, 2), {}, {}, {}}, {ecs::Entity(20, 90), {}, {}, {}}, {ecs::Entity(30, 421), {}, {}, {}}};
	}
	{
		auto& a = arr[0];
		CHECK(a.b == 16);
		CHECK(a.arr[0].id == ecs::Entity(1, 2));
		CHECK(a.arr[1].id == ecs::Entity(2, 30));
		CHECK(a.arr[2].id == ecs::Entity(3, 400));

		TArrInter test = {
				{ecs::Entity(1, 2), {}, {}, {}}, {ecs::Entity(2, 30), {}, {}, {}}, {ecs::Entity(3, 400), {}, {}, {}}};
		CHECK(test == a.arr);
	}
	{
		auto& a = arr[1];
		CHECK(a.b == 214);
		CHECK(a.arr[0].id == ecs::Entity(10, 2));
		CHECK(a.arr[1].id == ecs::Entity(20, 90));
		CHECK(a.arr[2].id == ecs::Entity(30, 421));

		TArrInter test = {
				{ecs::Entity(10, 2), {}, {}, {}}, {ecs::Entity(20, 90), {}, {}, {}}, {ecs::Entity(30, 421), {}, {}, {}}};
		CHECK(test == a.arr);
	}
}

TEST_CASE("Containers - sringbuffer") {
	{
		cnt::sarray<uint32_t, 5> comparearr = {0, 1, 2, 3, 4};
		cnt::sringbuffer<uint32_t, 5> arr = {0, 1, 2, 3, 4};
		uint32_t val{};

		// Iteration
		{
			uint32_t i = 0;
			for (auto curr: arr) {
				const auto expected = comparearr[i++];
				CHECK(curr == expected);
			}
			CHECK(i == 5);
		}

		CHECK_FALSE(arr.empty());
		CHECK(arr.front() == 0);
		CHECK(arr.back() == 4);

		arr.pop_front(val);
		CHECK(val == 0);
		CHECK_FALSE(arr.empty());
		CHECK(arr.front() == 1);
		CHECK(arr.back() == 4);

		arr.pop_front(val);
		CHECK(val == 1);
		CHECK_FALSE(arr.empty());
		CHECK(arr.front() == 2);
		CHECK(arr.back() == 4);

		arr.pop_front(val);
		CHECK(val == 2);
		CHECK_FALSE(arr.empty());
		CHECK(arr.front() == 3);
		CHECK(arr.back() == 4);

		arr.pop_back(val);
		CHECK(val == 4);
		CHECK_FALSE(arr.empty());
		CHECK(arr.front() == 3);
		CHECK(arr.back() == 3);

		arr.pop_back(val);
		CHECK(val == 3);
		CHECK(arr.empty());
	}

	{
		cnt::sringbuffer<uint32_t, 5> arr;
		uint32_t val{};

		CHECK(arr.empty());

		// Iteration
		{
			uint32_t i = 0;
			for ([[maybe_unused]] auto curr: arr) {
				++i;
			}
			CHECK(i == 0);
		}

		{
			arr.push_back(0);
			CHECK_FALSE(arr.empty());
			CHECK(arr.front() == 0);
			CHECK(arr.back() == 0);

			arr.push_back(1);
			CHECK_FALSE(arr.empty());
			CHECK(arr.front() == 0);
			CHECK(arr.back() == 1);

			arr.push_back(2);
			CHECK_FALSE(arr.empty());
			CHECK(arr.front() == 0);
			CHECK(arr.back() == 2);

			arr.push_back(3);
			CHECK_FALSE(arr.empty());
			CHECK(arr.front() == 0);
			CHECK(arr.back() == 3);

			arr.push_back(4);
			CHECK_FALSE(arr.empty());
			CHECK(arr.front() == 0);
			CHECK(arr.back() == 4);
		}
		{
			arr.pop_front(val);
			CHECK(val == 0);
			CHECK_FALSE(arr.empty());
			CHECK(arr.front() == 1);
			CHECK(arr.back() == 4);

			arr.pop_front(val);
			CHECK(val == 1);
			CHECK_FALSE(arr.empty());
			CHECK(arr.front() == 2);
			CHECK(arr.back() == 4);

			arr.pop_front(val);
			CHECK(val == 2);
			CHECK_FALSE(arr.empty());
			CHECK(arr.front() == 3);
			CHECK(arr.back() == 4);

			arr.pop_back(val);
			CHECK(val == 4);
			CHECK_FALSE(arr.empty());
			CHECK(arr.front() == 3);
			CHECK(arr.back() == 3);

			arr.pop_back(val);
			CHECK(val == 3);
			CHECK(arr.empty());
		}
	}
}

TEST_CASE("Containers - ilist") {
	struct EntityContainer: cnt::ilist_item {
		int value;

		EntityContainer() = default;
		EntityContainer(uint32_t index, uint32_t generation): cnt::ilist_item(index, generation) {}
	};
	SUBCASE("0 -> 1 -> 2") {
		cnt::ilist<EntityContainer, ecs::Entity> il;
		ecs::Entity handles[3];

		handles[0] = il.alloc();
		il[handles[0].id()].value = 100;
		CHECK(handles[0].id() == 0);
		CHECK(il[0].idx == 0);
		CHECK(handles[0].gen() == il[0].data.gen);
		CHECK(il[0].data.gen == 0);
		handles[1] = il.alloc();
		il[handles[1].id()].value = 200;
		CHECK(handles[1].id() == 1);
		CHECK(il[1].idx == 1);
		CHECK(handles[1].gen() == il[1].data.gen);
		CHECK(il[1].data.gen == 0);
		handles[2] = il.alloc();
		il[handles[2].id()].value = 300;
		CHECK(handles[2].id() == 2);
		CHECK(il[2].idx == 2);
		CHECK(handles[2].gen() == il[2].data.gen);
		CHECK(il[2].data.gen == 0);

		il.free(handles[2]);
		CHECK(il[2].idx == ecs::Entity::IdMask);
		CHECK(il[2].data.gen == 1);
		il.free(handles[1]);
		CHECK(il[1].idx == 2);
		CHECK(il[1].data.gen == 1);
		il.free(handles[0]);
		CHECK(il[0].idx == 1);
		CHECK(il[0].data.gen == 1);

		handles[0] = il.alloc();
		CHECK(handles[0].id() == 0);
		CHECK(il[0].value == 100);
		CHECK(il[0].idx == 1);
		CHECK(handles[0].gen() == il[0].data.gen);
		CHECK(il[0].data.gen == 1);
		handles[1] = il.alloc();
		CHECK(handles[1].id() == 1);
		CHECK(il[1].value == 200);
		CHECK(il[1].idx == 2);
		CHECK(handles[1].gen() == il[1].data.gen);
		CHECK(il[1].data.gen == 1);
		handles[2] = il.alloc();
		CHECK(handles[2].id() == 2);
		CHECK(il[2].value == 300);
		CHECK(il[2].idx == ecs::Entity::IdMask);
		CHECK(handles[2].gen() == il[2].data.gen);
		CHECK(il[2].data.gen == 1);
	}
	SUBCASE("2 -> 1 -> 0") {
		cnt::ilist<EntityContainer, ecs::Entity> il;
		ecs::Entity handles[3];

		handles[0] = il.alloc();
		il[handles[0].id()].value = 100;
		CHECK(handles[0].id() == 0);
		CHECK(il[0].idx == 0);
		CHECK(handles[0].gen() == il[0].data.gen);
		CHECK(il[0].data.gen == 0);
		handles[1] = il.alloc();
		il[handles[1].id()].value = 200;
		CHECK(handles[1].id() == 1);
		CHECK(il[1].idx == 1);
		CHECK(handles[1].gen() == il[1].data.gen);
		CHECK(il[1].data.gen == 0);
		handles[2] = il.alloc();
		il[handles[2].id()].value = 300;
		CHECK(handles[2].id() == 2);
		CHECK(il[2].idx == 2);
		CHECK(handles[2].gen() == il[2].data.gen);
		CHECK(il[2].data.gen == 0);

		il.free(handles[0]);
		CHECK(il[0].idx == ecs::Entity::IdMask);
		CHECK(il[0].data.gen == 1);
		il.free(handles[1]);
		CHECK(il[1].idx == 0);
		CHECK(il[1].data.gen == 1);
		il.free(handles[2]);
		CHECK(il[2].idx == 1);
		CHECK(il[2].data.gen == 1);

		handles[0] = il.alloc();
		CHECK(handles[0].id() == 2);
		const auto idx0 = handles[0].id();
		CHECK(il[idx0].value == 300);
		CHECK(il[idx0].idx == 1);
		CHECK(handles[0].gen() == il[idx0].data.gen);
		CHECK(il[idx0].data.gen == 1);
		handles[1] = il.alloc();
		CHECK(handles[1].id() == 1);
		const auto idx1 = handles[1].id();
		CHECK(il[idx1].value == 200);
		CHECK(il[idx1].idx == 0);
		CHECK(handles[1].gen() == il[idx1].data.gen);
		CHECK(il[idx1].data.gen == 1);
		handles[2] = il.alloc();
		CHECK(handles[2].id() == 0);
		const auto idx2 = handles[2].id();
		CHECK(il[idx2].value == 100);
		CHECK(il[idx2].idx == ecs::Entity::IdMask);
		CHECK(handles[2].gen() == il[idx2].data.gen);
		CHECK(il[idx2].data.gen == 1);
	}
}

template <uint32_t NBits>
void test_bitset() {
	// Following tests expect at least 5 bits of space
	static_assert(NBits >= 5);

	SUBCASE("Bit operations") {
		cnt::bitset<NBits> bs;
		CHECK(bs.count() == 0);
		CHECK(bs.size() == NBits);
		CHECK(bs.any() == false);
		CHECK(bs.all() == false);
		CHECK(bs.none() == true);

		bs.set(0, true);
		CHECK(bs.test(0) == true);
		CHECK(bs.count() == 1);
		CHECK(bs.any() == true);
		CHECK(bs.all() == false);
		CHECK(bs.none() == false);

		bs.set(1, true);
		CHECK(bs.test(1) == true);
		CHECK(bs.count() == 2);
		CHECK(bs.any() == true);
		CHECK(bs.all() == false);
		CHECK(bs.none() == false);

		bs.set(1, false);
		CHECK(bs.test(1) == false);
		CHECK(bs.count() == 1);
		CHECK(bs.any() == true);
		CHECK(bs.all() == false);
		CHECK(bs.none() == false);

		bs.flip(1);
		CHECK(bs.test(1) == true);
		CHECK(bs.count() == 2);
		CHECK(bs.any() == true);
		CHECK(bs.all() == false);
		CHECK(bs.none() == false);

		bs.flip(1);
		CHECK(bs.test(1) == false);
		CHECK(bs.count() == 1);
		CHECK(bs.any() == true);
		CHECK(bs.all() == false);
		CHECK(bs.none() == false);

		bs.reset(0);
		CHECK(bs.test(0) == false);
		CHECK(bs.count() == 0);
		CHECK(bs.any() == false);
		CHECK(bs.all() == false);
		CHECK(bs.none() == true);

		bs.set();
		CHECK(bs.count() == NBits);
		CHECK(bs.any() == true);
		CHECK(bs.all() == true);
		CHECK(bs.none() == false);

		bs.flip();
		CHECK(bs.count() == 0);
		CHECK(bs.any() == false);
		CHECK(bs.all() == false);
		CHECK(bs.none() == true);

		bs.flip();
		CHECK(bs.count() == NBits);
		CHECK(bs.any() == true);
		CHECK(bs.all() == true);
		CHECK(bs.none() == false);

		bs.reset();
		CHECK(bs.count() == 0);
		CHECK(bs.any() == false);
		CHECK(bs.all() == false);
		CHECK(bs.none() == true);
	}
	SUBCASE("Iteration") {
		{
			cnt::bitset<NBits> bs;
			uint32_t i = 0;
			for ([[maybe_unused]] auto val: bs)
				++i;
			CHECK(i == 0);
		}
		auto fwd_iterator_test = [](std::span<uint32_t> vals) {
			cnt::bitset<NBits> bs;
			for (uint32_t bit: vals)
				bs.set(bit);
			const uint32_t c = bs.count();

			uint32_t i = 0;
			for (const auto val: bs) {
				const auto valExpected = vals[i];
				CHECK(valExpected == val);
				++i;
			}
			CHECK(i == c);
			CHECK(i == (uint32_t)vals.size());
		};
		auto fwd_iterator_test2 = [](std::span<uint32_t> vals) {
			cnt::bitset<NBits> bs;
			for (uint32_t bit: vals)
				bs.set(bit);
			const uint32_t c = bs.count();

			uint32_t i = 0;
			for (auto it = bs.begin(); it != bs.end(); ++it) {
				const auto val = *it;
				const auto valExpected = vals[i];
				CHECK(valExpected == val);
				++i;
			}
			CHECK(i == c);
			CHECK(i == (uint32_t)vals.size());
		};
		auto bwd_iterator_test = [](std::span<uint32_t> vals) {
			cnt::bitset<NBits> bs;
			for (uint32_t bit: vals)
				bs.set(bit);
			const uint32_t c = bs.count();
			const uint32_t valsN = (uint32_t)vals.size();

			uint32_t i = 0;
			auto itEnd = bs.rend();
			for (auto it = bs.rbegin(); it != itEnd; ++it) {
				const auto val = *it;
				const auto valExpected = vals[valsN - i - 1U];
				CHECK(valExpected == val);
				++i;
			}
			CHECK(i == c);
			CHECK(i == valsN);
		};
		{
			uint32_t vals[]{1, 2, 3};
			fwd_iterator_test(vals);
			fwd_iterator_test2(vals);
			bwd_iterator_test(vals);
		}
		{
			uint32_t vals[]{0, 2, 3};
			fwd_iterator_test(vals);
			fwd_iterator_test2(vals);
			bwd_iterator_test(vals);
		}
		{
			uint32_t vals[]{1, 3, NBits - 1};
			fwd_iterator_test(vals);
			fwd_iterator_test2(vals);
			bwd_iterator_test(vals);
		}
		{
			uint32_t vals[]{1, NBits - 2, NBits - 1};
			fwd_iterator_test(vals);
			fwd_iterator_test2(vals);
			bwd_iterator_test(vals);
		}
		{
			uint32_t vals[]{0, 1, NBits - 1};
			fwd_iterator_test(vals);
			fwd_iterator_test2(vals);
			bwd_iterator_test(vals);
		}
		{
			uint32_t vals[]{0, 3, NBits - 1};
			fwd_iterator_test(vals);
			fwd_iterator_test2(vals);
			bwd_iterator_test(vals);
		}
	}
}

TEST_CASE("Containers - bitset") {
	SUBCASE("5 bits") {
		test_bitset<5>();
	}
	SUBCASE("11 bits") {
		test_bitset<11>();
	}
	SUBCASE("32 bits") {
		test_bitset<32>();
	}
	SUBCASE("33 bits") {
		test_bitset<33>();
	}
	SUBCASE("64 bits") {
		test_bitset<64>();
	}
	SUBCASE("90 bits") {
		test_bitset<90>();
	}
	SUBCASE("128 bits") {
		test_bitset<128>();
	}
	SUBCASE("150 bits") {
		test_bitset<150>();
	}
	SUBCASE("512 bits") {
		test_bitset<512>();
	}
	SUBCASE("Ranges 11 bits") {
		cnt::bitset<11> bs;
		bs.set(1);
		bs.set(10);
		bs.flip(2, 9);
		for (uint32_t i = 1; i <= 10; ++i)
			CHECK(bs.test(i) == true);
		bs.flip(2, 9);
		for (uint32_t i = 2; i < 10; ++i)
			CHECK(bs.test(i) == false);
		CHECK(bs.test(1));
		CHECK(bs.test(10));

		bs.reset();
		bs.flip(0, 0);
		CHECK(bs.test(0));
		CHECK(bs.count() == 1);
		CHECK(bs.any() == true);
		CHECK(bs.all() == false);

		bs.reset();
		bs.flip(10, 10);
		CHECK(bs.test(10));
		CHECK(bs.count() == 1);
		CHECK(bs.any() == true);
		CHECK(bs.all() == false);

		bs.reset();
		bs.flip(0, 10);
		CHECK(bs.count() == 11);
		CHECK(bs.all() == true);
		bs.flip(0, 10);
		CHECK(bs.count() == 0);
		CHECK(bs.none() == true);
	}
	SUBCASE("Ranges 64 bits") {
		cnt::bitset<64> bs;
		bs.set(1);
		bs.set(10);
		bs.flip(2, 9);
		for (uint32_t i = 1; i <= 10; ++i)
			CHECK(bs.test(i) == true);
		bs.flip(2, 9);
		for (uint32_t i = 2; i < 10; ++i)
			CHECK(bs.test(i) == false);
		CHECK(bs.test(1));
		CHECK(bs.test(10));

		bs.reset();
		bs.flip(0, 0);
		CHECK(bs.test(0));
		CHECK(bs.count() == 1);
		CHECK(bs.any() == true);
		CHECK(bs.all() == false);

		bs.reset();
		bs.flip(63, 63);
		CHECK(bs.test(63));
		CHECK(bs.count() == 1);
		CHECK(bs.any() == true);
		CHECK(bs.all() == false);

		bs.reset();
		bs.flip(0, 63);
		CHECK(bs.count() == 64);
		CHECK(bs.all() == true);
		bs.flip(0, 63);
		CHECK(bs.count() == 0);
		CHECK(bs.none() == true);
	}
	SUBCASE("Ranges 101 bits") {
		cnt::bitset<101> bs;
		bs.set(1);
		bs.set(100);
		bs.flip(2, 99);
		for (uint32_t i = 1; i <= 100; ++i)
			CHECK(bs.test(i) == true);
		bs.flip(2, 99);
		for (uint32_t i = 2; i < 100; ++i)
			CHECK(bs.test(i) == false);
		CHECK(bs.test(1));
		CHECK(bs.test(100));

		bs.reset();
		bs.flip(0, 0);
		CHECK(bs.test(0));
		CHECK(bs.count() == 1);
		CHECK(bs.any() == true);
		CHECK(bs.all() == false);

		bs.reset();
		bs.flip(100, 100);
		CHECK(bs.test(100));
		CHECK(bs.count() == 1);
		CHECK(bs.any() == true);
		CHECK(bs.all() == false);

		bs.reset();
		bs.flip(0, 100);
		CHECK(bs.count() == 101);
		CHECK(bs.all() == true);
		bs.flip(0, 100);
		CHECK(bs.count() == 0);
		CHECK(bs.none() == true);
	}
}

template <uint32_t NBits>
void test_dbitset() {
	// Following tests expect at least 5 bits of space
	static_assert(NBits >= 5);

	SUBCASE("Bit operations") {
		cnt::dbitset bs;
		CHECK(bs.count() == 0);
		CHECK(bs.size() == 1);
		CHECK(bs.any() == false);
		CHECK(bs.all() == false);
		CHECK(bs.none() == true);

		bs.set(0, true);
		CHECK(bs.test(0) == true);
		CHECK(bs.count() == 1);
		CHECK(bs.any() == true);
		CHECK(bs.all() == true);
		CHECK(bs.none() == false);

		bs.set(1, true);
		CHECK(bs.test(1) == true);
		CHECK(bs.count() == 2);
		CHECK(bs.any() == true);
		CHECK(bs.all() == true);
		CHECK(bs.none() == false);

		bs.set(1, false);
		CHECK(bs.test(1) == false);
		CHECK(bs.count() == 1);
		CHECK(bs.any() == true);
		CHECK(bs.all() == false);
		CHECK(bs.none() == false);

		bs.flip(1);
		CHECK(bs.test(1) == true);
		CHECK(bs.count() == 2);
		CHECK(bs.any() == true);
		CHECK(bs.all() == true);
		CHECK(bs.none() == false);

		bs.flip(1);
		CHECK(bs.test(1) == false);
		CHECK(bs.count() == 1);
		CHECK(bs.any() == true);
		CHECK(bs.all() == false);
		CHECK(bs.none() == false);

		bs.reset(0);
		CHECK(bs.test(0) == false);
		CHECK(bs.count() == 0);
		CHECK(bs.any() == false);
		CHECK(bs.all() == false);
		CHECK(bs.none() == true);

		bs.set();
		CHECK(bs.count() == bs.size());
		CHECK(bs.any() == true);
		CHECK(bs.all() == true);
		CHECK(bs.none() == false);

		bs.flip();
		CHECK(bs.count() == 0);
		CHECK(bs.any() == false);
		CHECK(bs.all() == false);
		CHECK(bs.none() == true);

		bs.flip();
		CHECK(bs.count() == bs.size());
		CHECK(bs.any() == true);
		CHECK(bs.all() == true); // 2 bits are set
		CHECK(bs.none() == false);

		const auto ss = bs.size();
		bs.reserve(256);
		CHECK(bs.size() == 2);
		CHECK(bs.count() == ss); // size doesn't change
		CHECK(bs.any() == true);
		CHECK(bs.all() == true);
		CHECK(bs.none() == false);

		bs.resize(128);
		CHECK(bs.size() == 128);
		CHECK(bs.count() == 2);
		CHECK(bs.any() == true);
		CHECK(bs.all() == false);
		CHECK(bs.none() == false);

		bs.resize(512);
		CHECK(bs.size() == 512);
		CHECK(bs.count() == 2);
		CHECK(bs.any() == true);
		CHECK(bs.all() == false);
		CHECK(bs.none() == false);

		bs.reset();
		CHECK(bs.count() == 0);
		CHECK(bs.any() == false);
		CHECK(bs.all() == false);
		CHECK(bs.none() == true);
	}
	SUBCASE("Iteration") {
		{
			cnt::dbitset bs;
			uint32_t i = 0;
			for ([[maybe_unused]] auto val: bs)
				++i;
			CHECK(i == 0);
		}
		auto fwd_iterator_test = [](std::span<uint32_t> vals) {
			cnt::dbitset bs;
			for (uint32_t bit: vals)
				bs.set(bit);
			const uint32_t c = bs.count();

			uint32_t i = 0;
			for (auto val: bs) {
				CHECK(vals[i] == val);
				++i;
			}
			CHECK(i == c);
			CHECK(i == (uint32_t)vals.size());
		};
		auto fwd_iterator_test2 = [](std::span<uint32_t> vals) {
			cnt::dbitset bs;
			for (uint32_t bit: vals)
				bs.set(bit);
			const uint32_t c = bs.count();

			uint32_t i = 0;
			for (auto it = bs.begin(); it != bs.end(); ++it) {
				const auto val = *it;
				const auto valExpected = vals[i];
				CHECK(valExpected == val);
				++i;
			}
			CHECK(i == c);
			CHECK(i == (uint32_t)vals.size());
		};
		auto bwd_iterator_test = [](std::span<uint32_t> vals) {
			cnt::dbitset bs;
			for (uint32_t bit: vals)
				bs.set(bit);
			const uint32_t c = bs.count();
			const uint32_t valsN = (uint32_t)vals.size();

			uint32_t i = 0;
			auto itEnd = bs.rend();
			for (auto it = bs.rbegin(); it != itEnd; ++it) {
				const auto val = *it;
				const auto valExpected = vals[valsN - i - 1U];
				CHECK(valExpected == val);
				++i;
			}
			CHECK(i == c);
			CHECK(i == valsN);
		};
		{
			uint32_t vals[]{1, 2, 3};
			fwd_iterator_test(vals);
			fwd_iterator_test2(vals);
			bwd_iterator_test(vals);
		}
		{
			uint32_t vals[]{0, 2, 3};
			fwd_iterator_test(vals);
			fwd_iterator_test2(vals);
			bwd_iterator_test(vals);
		}
		{
			uint32_t vals[]{1, 3, NBits - 1};
			fwd_iterator_test(vals);
			fwd_iterator_test2(vals);
			bwd_iterator_test(vals);
		}
		{
			uint32_t vals[]{1, NBits - 2, NBits - 1};
			fwd_iterator_test(vals);
			fwd_iterator_test2(vals);
			bwd_iterator_test(vals);
		}
		{
			uint32_t vals[]{0, 1, NBits - 1};
			fwd_iterator_test(vals);
			fwd_iterator_test2(vals);
			bwd_iterator_test(vals);
		}
		{
			uint32_t vals[]{0, 3, NBits - 1};
			fwd_iterator_test(vals);
			fwd_iterator_test2(vals);
			bwd_iterator_test(vals);
		}
	}
}

TEST_CASE("Containers - dbitset") {
	SUBCASE("5 bits") {
		test_dbitset<5>();
	}
	SUBCASE("11 bits") {
		test_dbitset<11>();
	}
	SUBCASE("32 bits") {
		test_dbitset<32>();
	}
	SUBCASE("33 bits") {
		test_dbitset<33>();
	}
	SUBCASE("64 bits") {
		test_dbitset<64>();
	}
	SUBCASE("90 bits") {
		test_dbitset<90>();
	}
	SUBCASE("128 bits") {
		test_dbitset<128>();
	}
	SUBCASE("150 bits") {
		test_dbitset<150>();
	}
	SUBCASE("512 bits") {
		test_dbitset<512>();
	}
	SUBCASE("Ranges 11 bits") {
		cnt::dbitset bs;
		bs.resize(11);

		bs.set(1);
		bs.set(10);
		bs.flip(2, 9);
		for (uint32_t i = 1; i <= 10; ++i)
			CHECK(bs.test(i) == true);
		bs.flip(2, 9);
		for (uint32_t i = 2; i < 10; ++i)
			CHECK(bs.test(i) == false);
		CHECK(bs.test(1));
		CHECK(bs.test(10));

		bs.reset();
		bs.flip(0, 0);
		CHECK(bs.test(0));
		CHECK(bs.count() == 1);
		CHECK(bs.any() == true);
		CHECK(bs.all() == false);

		bs.reset();
		bs.flip(10, 10);
		CHECK(bs.test(10));
		CHECK(bs.count() == 1);
		CHECK(bs.any() == true);
		CHECK(bs.all() == false);

		bs.reset();
		bs.flip(0, 10);
		CHECK(bs.count() == 11);
		CHECK(bs.all() == true);
		bs.flip(0, 10);
		CHECK(bs.count() == 0);
		CHECK(bs.none() == true);
	}
	SUBCASE("Ranges 64 bits") {
		cnt::dbitset bs;
		bs.resize(64);

		bs.set(1);
		bs.set(10);
		bs.flip(2, 9);
		for (uint32_t i = 1; i <= 10; ++i)
			CHECK(bs.test(i) == true);
		bs.flip(2, 9);
		for (uint32_t i = 2; i < 10; ++i)
			CHECK(bs.test(i) == false);
		CHECK(bs.test(1));
		CHECK(bs.test(10));

		bs.reset();
		bs.flip(0, 0);
		CHECK(bs.test(0));
		CHECK(bs.count() == 1);
		CHECK(bs.any() == true);
		CHECK(bs.all() == false);

		bs.reset();
		bs.flip(63, 63);
		CHECK(bs.test(63));
		CHECK(bs.count() == 1);
		CHECK(bs.any() == true);
		CHECK(bs.all() == false);

		bs.reset();
		bs.flip(0, 63);
		CHECK(bs.count() == 64);
		CHECK(bs.all() == true);
		bs.flip(0, 63);
		CHECK(bs.count() == 0);
		CHECK(bs.none() == true);
	}
	SUBCASE("Ranges 101 bits") {
		cnt::dbitset bs;
		bs.resize(101);

		bs.set(1);
		bs.set(100);
		bs.flip(2, 99);
		for (uint32_t i = 1; i <= 100; ++i)
			CHECK(bs.test(i) == true);
		bs.flip(2, 99);
		for (uint32_t i = 2; i < 100; ++i)
			CHECK(bs.test(i) == false);
		CHECK(bs.test(1));
		CHECK(bs.test(100));

		bs.reset();
		bs.flip(0, 0);
		CHECK(bs.test(0));
		CHECK(bs.count() == 1);
		CHECK(bs.any() == true);
		CHECK(bs.all() == false);

		bs.reset();
		bs.flip(100, 100);
		CHECK(bs.test(100));
		CHECK(bs.count() == 1);
		CHECK(bs.any() == true);
		CHECK(bs.all() == false);

		bs.reset();
		bs.flip(0, 100);
		CHECK(bs.count() == 101);
		CHECK(bs.all() == true);
		bs.flip(0, 100);
		CHECK(bs.count() == 0);
		CHECK(bs.none() == true);
	}
}

//-----------------------------------------------------------------
// Iteration
//-----------------------------------------------------------------

TEST_CASE("each") {
	constexpr uint32_t N = 10;
	SUBCASE("index argument") {
		uint32_t cnt = 0;
		core::each<N>([&cnt](auto i) {
			cnt += i;
		});
		CHECK(cnt == 0 + 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9);
	}
	SUBCASE("no index argument") {
		uint32_t cnt = 0;
		core::each<N>([&cnt]() {
			++cnt;
		});
		CHECK(cnt == N);
	}
}

TEST_CASE("each_ext") {
	constexpr uint32_t N = 10;
	SUBCASE("index argument") {
		uint32_t cnt = 0;
		core::each_ext<2, N - 1, 2>([&cnt](auto i) {
			cnt += i;
		});
		CHECK(cnt == 2 + 4 + 6 + 8);
	}
	SUBCASE("no index argument") {
		uint32_t cnt = 0;
		core::each_ext<2, N - 1, 2>([&cnt]() {
			++cnt;
		});
		CHECK(cnt == 4);
	}
}

TEST_CASE("each_tuple") {
	SUBCASE("func(Args)") {
		uint32_t val = 0;
		core::each_tuple(std::make_tuple(69, 10, 20), [&val](const auto& value) {
			val += value;
		});
		CHECK(val == 99);
	}
	SUBCASE("func(Args, iter)") {
		uint32_t val = 0;
		uint32_t iter = 0;
		core::each_tuple(std::make_tuple(69, 10, 20), [&](const auto& value, uint32_t i) {
			val += value;
			CHECK(i == iter);
			++iter;
		});
		CHECK(val == 99);
	}
}

TEST_CASE("each_tuple_ext") {
	SUBCASE("func(Args)") {
		uint32_t val = 0;
		core::each_tuple_ext<1, 3>(std::make_tuple(69, 10, 20), [&val](const auto& value) {
			val += value;
		});
		CHECK(val == 30);
	}
	SUBCASE("func(Args, iter)") {
		uint32_t val = 0;
		uint32_t iter = 1;
		core::each_tuple_ext<1, 3>(std::make_tuple(69, 10, 20), [&](const auto& value, uint32_t i) {
			val += value;
			CHECK(i == iter);
			++iter;
		});
		CHECK(val == 30);
	}
}

TEST_CASE("each_tuple2") {
	SUBCASE("func(Args)") {
		uint32_t val = 0;
		using TTuple = std::tuple<int, int, double>;
		core::each_tuple<TTuple>([&val](auto&& item) {
			val += sizeof(item);
		});
		CHECK(val == 16);
	}
	SUBCASE("func(Args, iter)") {
		uint32_t val = 0;
		uint32_t iter = 0;
		using TTuple = std::tuple<int, int, double>;
		core::each_tuple<TTuple>([&](auto&& item, uint32_t i) {
			val += sizeof(item);
			CHECK(i == iter);
			++iter;
		});
		CHECK(val == 16);
	}
}

TEST_CASE("each_tuple_ext2") {
	SUBCASE("func(Args)") {
		uint32_t val = 0;
		using TTuple = std::tuple<int, int, double>;
		core::each_tuple_ext<1, 3, TTuple>([&val](auto&& item) {
			val += sizeof(item);
		});
		CHECK(val == 12);
	}
	SUBCASE("func(Args, iter)") {
		uint32_t val = 0;
		uint32_t iter = 1;
		using TTuple = std::tuple<int, int, double>;
		core::each_tuple_ext<1, 3, TTuple>([&](auto&& item, uint32_t i) {
			val += sizeof(item);
			CHECK(i == iter);
			++iter;
		});
		CHECK(val == 12);
	}
}

TEST_CASE("each_pack") {
	uint32_t val = 0;
	core::each_pack(
			[&val](const auto& value) {
				val += value;
			},
			69, 10, 20);
	CHECK(val == 99);
}

//-----------------------------------------------------------------
// Sorting
//-----------------------------------------------------------------

template <typename C>
void sort_descending(C&& arr) {
	using TValue = typename C::value_type;

	// Default swap function

	{
		for (TValue i = 0; i < (TValue)arr.size(); ++i)
			arr[i] = i;
		core::sort(arr, core::is_greater<TValue>());
		for (uint32_t i = 1; i < arr.size(); ++i)
			CHECK(arr[i - 1] > arr[i]);
	}

	{
		for (TValue i = 0; i < (TValue)arr.size(); ++i)
			arr[i] = i;
		core::sort(arr.begin(), arr.end(), core::is_greater<TValue>());
		for (uint32_t i = 1; i < arr.size(); ++i)
			CHECK(arr[i - 1] > arr[i]);
	}

	{
		for (TValue i = 0; i < (TValue)arr.size(); ++i)
			arr[i] = i;
		core::quick_sort_stack(arr, 0, arr.size() - 1, core::is_greater<TValue>(), arr.size());
		for (uint32_t i = 1; i < arr.size(); ++i)
			CHECK(arr[i - 1] > arr[i]);
	}

	// Custom swap function

	{
		for (TValue i = 0; i < (TValue)arr.size(); ++i)
			arr[i] = i;
		core::sort(arr, core::is_greater<TValue>(), [&](uint32_t a, uint32_t b) {
			core::swap(arr[a], arr[b]);
		});
		for (uint32_t i = 1; i < arr.size(); ++i)
			CHECK(arr[i - 1] > arr[i]);
	}

	{
		for (TValue i = 0; i < (TValue)arr.size(); ++i)
			arr[i] = i;
		core::sort(arr.begin(), arr.end(), core::is_greater<TValue>(), [&](uint32_t a, uint32_t b) {
			core::swap(arr[a], arr[b]);
		});
		for (uint32_t i = 1; i < arr.size(); ++i)
			CHECK(arr[i - 1] > arr[i]);
	}

	{
		for (TValue i = 0; i < (TValue)arr.size(); ++i)
			arr[i] = i;
		core::quick_sort_stack(
				arr, 0, arr.size() - 1, core::is_greater<TValue>(),
				[&](uint32_t a, uint32_t b) {
					core::swap(arr[a], arr[b]);
				},
				arr.size());
		for (uint32_t i = 1; i < arr.size(); ++i)
			CHECK(arr[i - 1] > arr[i]);
	}
}

template <typename C>
void sort_ascending(C&& arr) {
	using TValue = typename C::value_type;

	// Default swap function

	{
		for (TValue i = 0; i < (TValue)arr.size(); ++i)
			arr[i] = i;
		core::sort(arr, core::is_smaller<TValue>());
		for (uint32_t i = 1; i < arr.size(); ++i)
			CHECK(arr[i - 1] < arr[i]);
	}

	{
		for (TValue i = 0; i < (TValue)arr.size(); ++i)
			arr[i] = i;
		core::sort(arr.begin(), arr.end(), core::is_smaller<TValue>());
		for (uint32_t i = 1; i < arr.size(); ++i)
			CHECK(arr[i - 1] < arr[i]);
	}

	{
		for (TValue i = 0; i < (TValue)arr.size(); ++i)
			arr[i] = i;
		core::quick_sort_stack(arr, 0, arr.size() - 1, core::is_smaller<TValue>(), arr.size());
		for (uint32_t i = 1; i < arr.size(); ++i)
			CHECK(arr[i - 1] < arr[i]);
	}

	// Custom swap function

	{
		for (TValue i = 0; i < (TValue)arr.size(); ++i)
			arr[i] = i;
		core::sort(arr, core::is_smaller<TValue>(), [&](uint32_t a, uint32_t b) {
			core::swap(arr[a], arr[b]);
		});
		for (uint32_t i = 1; i < arr.size(); ++i)
			CHECK(arr[i - 1] < arr[i]);
	}

	{
		for (TValue i = 0; i < (TValue)arr.size(); ++i)
			arr[i] = i;
		core::sort(arr.begin(), arr.end(), core::is_smaller<TValue>(), [&](uint32_t a, uint32_t b) {
			core::swap(arr[a], arr[b]);
		});
		for (uint32_t i = 1; i < arr.size(); ++i)
			CHECK(arr[i - 1] < arr[i]);
	}

	{
		for (TValue i = 0; i < (TValue)arr.size(); ++i)
			arr[i] = i;
		core::quick_sort_stack(
				arr, 0, arr.size() - 1, core::is_smaller<TValue>(),
				[&](uint32_t a, uint32_t b) {
					core::swap(arr[a], arr[b]);
				},
				arr.size());
		for (uint32_t i = 1; i < arr.size(); ++i)
			CHECK(arr[i - 1] < arr[i]);
	}
}

TEST_CASE("Sort descending") {
	sort_descending(cnt::sarray<uint32_t, 2>{});
	sort_descending(cnt::sarray<uint32_t, 3>{});
	sort_descending(cnt::sarray<uint32_t, 4>{});
	sort_descending(cnt::sarray<uint32_t, 5>{});
	sort_descending(cnt::sarray<uint32_t, 6>{});
	sort_descending(cnt::sarray<uint32_t, 7>{});
	sort_descending(cnt::sarray<uint32_t, 8>{});
	sort_descending(cnt::sarray<uint32_t, 9>{});
	sort_descending(cnt::sarray<uint32_t, 10>{});
	sort_descending(cnt::sarray<uint32_t, 11>{});
	sort_descending(cnt::sarray<uint32_t, 12>{});
	sort_descending(cnt::sarray<uint32_t, 13>{});
	sort_descending(cnt::sarray<uint32_t, 14>{});
	sort_descending(cnt::sarray<uint32_t, 15>{});
	sort_descending(cnt::sarray<uint32_t, 16>{});
	sort_descending(cnt::sarray<uint32_t, 17>{});
	sort_descending(cnt::sarray<uint32_t, 18>{});
	sort_descending(cnt::sarray<uint32_t, 45>{});
}

TEST_CASE("Sort ascending") {
	sort_ascending(cnt::sarray<uint32_t, 2>{});
	sort_ascending(cnt::sarray<uint32_t, 3>{});
	sort_ascending(cnt::sarray<uint32_t, 4>{});
	sort_ascending(cnt::sarray<uint32_t, 5>{});
	sort_ascending(cnt::sarray<uint32_t, 6>{});
	sort_ascending(cnt::sarray<uint32_t, 7>{});
	sort_ascending(cnt::sarray<uint32_t, 8>{});
	sort_ascending(cnt::sarray<uint32_t, 9>{});
	sort_ascending(cnt::sarray<uint32_t, 10>{});
	sort_ascending(cnt::sarray<uint32_t, 11>{});
	sort_ascending(cnt::sarray<uint32_t, 12>{});
	sort_ascending(cnt::sarray<uint32_t, 13>{});
	sort_ascending(cnt::sarray<uint32_t, 14>{});
	sort_ascending(cnt::sarray<uint32_t, 15>{});
	sort_ascending(cnt::sarray<uint32_t, 16>{});
	sort_ascending(cnt::sarray<uint32_t, 17>{});
	sort_ascending(cnt::sarray<uint32_t, 18>{});
	sort_ascending(cnt::sarray<uint32_t, 45>{});
}

//-----------------------------------------------------------------
// ECS
//-----------------------------------------------------------------

TEST_CASE("EntityKinds") {
	CHECK(ecs::entity_kind_v<uint32_t> == ecs::EntityKind::EK_Gen);
	CHECK(ecs::entity_kind_v<Position> == ecs::EntityKind::EK_Gen);
	CHECK(ecs::entity_kind_v<ecs::uni<Position>> == ecs::EntityKind::EK_Uni);
}

GAIA_GCC_WARNING_PUSH()
GAIA_GCC_WARNING_DISABLE("-Wmissing-field-initializers")
GAIA_CLANG_WARNING_PUSH()
GAIA_CLANG_WARNING_DISABLE("-Wmissing-field-initializers")

template <typename T>
void TestDataLayoutAoS() {
	constexpr uint32_t N = 100;
	cnt::sarray<T, N> data;

	GAIA_FOR(N) {
		const auto f = (float)(i + 1);
		data[i] = {f, f, f};

		auto val = data[i];
		CHECK(val.x == f);
		CHECK(val.y == f);
		CHECK(val.z == f);
	}

	SUBCASE("Make sure that all values are correct (e.g. that they were not overriden by one of the loop iteration)") {
		GAIA_FOR(N) {
			const auto f = (float)(i + 1);

			auto val = data[i];
			CHECK(val.x == f);
			CHECK(val.y == f);
			CHECK(val.z == f);
		}
	}
}

TEST_CASE("DataLayout AoS") {
	TestDataLayoutAoS<Position>();
	TestDataLayoutAoS<Rotation>();
}

template <typename T>
void TestDataLayoutSoA() {
	constexpr uint32_t N = 100;
	auto test = [](const auto& data, auto* f, uint32_t i) {
		T val = data[i];
		CHECK(val.x == f[0]);
		CHECK(val.y == f[1]);
		CHECK(val.z == f[2]);

		const float ff[] = {data.template view<0>()[i], data.template view<1>()[i], data.template view<2>()[i]};
		CHECK(ff[0] == f[0]);
		CHECK(ff[1] == f[1]);
		CHECK(ff[2] == f[2]);
	};

	{
		cnt::sarray_soa<T, N> data;

		GAIA_FOR(N) {
			const float f[] = {(float)(i + 1), (float)(i + 2), (float)(i + 3)};
			data[i] = {f[0], f[1], f[2]};
			test(data, f, i);
		}

		// Make sure that all values are correct (e.g. that they were not overriden by one of the loop iteration)
		GAIA_FOR(N) {
			const float f[] = {(float)(i + 1), (float)(i + 2), (float)(i + 3)};
			test(data, f, i);
		}
	}
	{
		cnt::darray_soa<T> data;

		GAIA_FOR(N) {
			const float f[] = {(float)(i + 1), (float)(i + 2), (float)(i + 3)};
			data.push_back({f[0], f[1], f[2]});
			test(data, f, i);
		}

		// Make sure that all values are correct (e.g. that they were not overriden by one of the loop iteration)
		GAIA_FOR(N) {
			const float f[] = {(float)(i + 1), (float)(i + 2), (float)(i + 3)};
			test(data, f, i);
		}
	}
}

template <>
void TestDataLayoutSoA<DummySoA>() {
	constexpr uint32_t N = 100;
	auto test = [](const auto& data, auto* f, uint32_t i) {
		DummySoA val = data[i];
		CHECK(val.x == f[0]);
		CHECK(val.y == f[1]);
		CHECK(val.b == true);
		CHECK(val.w == f[2]);

		const float ff[] = {data.template view<0>()[i], data.template view<1>()[i], data.template view<3>()[i]};
		const bool b = data.template view<2>()[i];
		CHECK(ff[0] == f[0]);
		CHECK(ff[1] == f[1]);
		CHECK(b == true);
		CHECK(ff[2] == f[2]);
	};

	{
		cnt::sarray_soa<DummySoA, N> data{};

		GAIA_FOR(N) {
			float f[] = {(float)(i + 1), (float)(i + 2), (float)(i + 3)};
			data[i] = {f[0], f[1], true, f[2]};
			test(data, f, i);
		}

		// Make sure that all values are correct (e.g. that they were not overriden by one of the loop iteration)
		GAIA_FOR(N) {
			const float f[] = {(float)(i + 1), (float)(i + 2), (float)(i + 3)};
			test(data, f, i);
		}
	}
	{
		cnt::darray_soa<DummySoA> data;

		GAIA_FOR(N) {
			float f[] = {(float)(i + 1), (float)(i + 2), (float)(i + 3)};
			data.push_back({f[0], f[1], true, f[2]});
			test(data, f, i);
		}

		// Make sure that all values are correct (e.g. that they were not overriden by one of the loop iteration)
		GAIA_FOR(N) {
			const float f[] = {(float)(i + 1), (float)(i + 2), (float)(i + 3)};
			test(data, f, i);
		}
	}
}

TEST_CASE("DataLayout SoA") {
	TestDataLayoutSoA<PositionSoA>();
	TestDataLayoutSoA<RotationSoA>();
	TestDataLayoutSoA<DummySoA>();
}

TEST_CASE("DataLayout SoA8") {
	TestDataLayoutSoA<PositionSoA8>();
	TestDataLayoutSoA<RotationSoA8>();
}

TEST_CASE("DataLayout SoA16") {
	TestDataLayoutSoA<PositionSoA16>();
	TestDataLayoutSoA<RotationSoA16>();
}

GAIA_CLANG_WARNING_POP()
GAIA_GCC_WARNING_POP()

TEST_CASE("Entity - valid/has") {
	TestWorld twld;

	auto e = wld.add();
	CHECK(wld.valid(e));
	CHECK(wld.has(e));

	wld.del(e);
	CHECK_FALSE(wld.valid(e));
	CHECK_FALSE(wld.has(e));
}

TEST_CASE("Entity - EntityBad") {
	CHECK(ecs::Entity{} == ecs::EntityBad);

	TestWorld twld;
	CHECK_FALSE(wld.valid(ecs::EntityBad));
	CHECK_FALSE(wld.has(ecs::EntityBad));

	auto e = wld.add();
	CHECK(e != ecs::EntityBad);
	CHECK_FALSE(e == ecs::EntityBad);
	CHECK(e.entity());
}

TEST_CASE("Entity copy") {
	TestWorld twld;

	auto e1 = wld.add();
	auto e2 = wld.add();
	wld.add(e1, e2);
	auto e3 = wld.copy(e1);

	CHECK(wld.has(e3, e2));
}

#if GAIA_USE_SAFE_ENTITY
TEST_CASE("Entity safe") {
	SUBCASE("safe, no delete") {
		TestWorld twld;

		auto e = wld.add();
		CHECK(wld.valid(e));
		CHECK(wld.has(e));

		{
			auto se = ecs::SafeEntity(wld, e);
			CHECK(wld.valid(se));
			CHECK(wld.has(se));
			CHECK(wld.valid(e));
			CHECK(wld.has(e));
		}

		// SafeEntity went out of scope but we didn't request delete.
		// The entity needs to stay alive.
		CHECK(wld.valid(e));
		CHECK(wld.has(e));
	}

	SUBCASE("safe with delete") {
		TestWorld twld;

		auto e = wld.add();
		CHECK(wld.valid(e));
		CHECK(wld.has(e));

		{
			auto se = ecs::SafeEntity(wld, e);
			CHECK(se == e);

			CHECK(wld.valid(se));
			CHECK(wld.has(se));
			CHECK(wld.valid(e));
			CHECK(wld.has(e));

			// We can call del as many times as we want. So long a SafeEntity is around,
			// we won't be able to delete the entity.
			GAIA_FOR(10) {
				wld.del(e);
				CHECK(wld.valid(e));
				CHECK(wld.has(e));
			}
		}

		// SafeEntity went out of scope and we did request delete.
		// The entity needs to be dead now.
		CHECK_FALSE(wld.valid(e));
		CHECK_FALSE(wld.has(e));
	}

	SUBCASE("many safes with delete") {
		TestWorld twld;

		auto e = wld.add();
		CHECK(wld.valid(e));
		CHECK(wld.has(e));

		{
			auto se0 = ecs::SafeEntity(wld, e);

			{
				auto se = ecs::SafeEntity(wld, e);
				CHECK(se == e);
				CHECK(se == se0);

				CHECK(wld.valid(se));
				CHECK(wld.has(se));
				CHECK(wld.valid(e));
				CHECK(wld.has(e));

				// We can call del as many times as we want. So long a SafeEntity is around,
				// we won't be able to delete the entity.
				GAIA_FOR(10) {
					wld.del(e);
					CHECK(wld.valid(e));
					CHECK(wld.has(e));
				}
			}

			// SafeEntity went out of scope and we did request delete.
			// However, se0 is still in scope, so no delete should happen no matter
			// how many times we try.
			GAIA_FOR(10) {
				wld.del(e);
				CHECK(wld.valid(e));
				CHECK(wld.has(e));
			}
		}

		// SafeEntity went out of scope and we did request delete.
		// The entity needs to be dead now.
		CHECK_FALSE(wld.valid(e));
		CHECK_FALSE(wld.has(e));
	}

	SUBCASE("component") {
		TestWorld twld;

		struct SafeComponent {
			ecs::SafeEntity entity;
		};

		auto e = wld.add();

		auto e2 = wld.add();
		wld.add<SafeComponent>(e2, {ecs::SafeEntity(wld, e)});

		const auto& sc = wld.get<SafeComponent>(e2);
		CHECK(sc.entity == e);

		CHECK(wld.valid(e));
		CHECK(wld.has(e));

		// Nothings gets deleted because the reference is held in the component
		wld.del(e);
		CHECK(wld.valid(e));
		CHECK(wld.has(e));

		// Delete the entity holding the ref
		wld.del(e2);
		CHECK_FALSE(wld.valid(e));
		CHECK_FALSE(wld.has(e));
	}
}
#endif

#if GAIA_USE_WEAK_ENTITY
TEST_CASE("Entity weak") {
	SUBCASE("weak, no delete") {
		TestWorld twld;

		auto e = wld.add();
		CHECK(wld.valid(e));
		CHECK(wld.has(e));

		auto we = ecs::WeakEntity(wld, e);
		CHECK(we == e);

		CHECK(wld.valid(we));
		CHECK(wld.has(we));
		CHECK(wld.valid(e));
		CHECK(wld.has(e));

		wld.del(e);
		CHECK_FALSE(wld.valid(e));
		CHECK_FALSE(wld.has(e));
		CHECK_FALSE(wld.valid(we));
		CHECK_FALSE(wld.has(we));
	}

	SUBCASE("weak with delete") {
		TestWorld twld;

		auto e = wld.add();
		CHECK(wld.valid(e));
		CHECK(wld.has(e));

		auto we = ecs::WeakEntity(wld, e);

		{
			auto se = ecs::SafeEntity(wld, e);
			CHECK(se == e);

			CHECK(wld.valid(we));
			CHECK(wld.has(we));
			CHECK(wld.valid(e));
			CHECK(wld.has(e));

			// We can call del as many times as we want. So long a SafeEntity is around,
			// we won't be able to delete the entity.
			GAIA_FOR(10) {
				wld.del(e);
				CHECK(wld.valid(we));
				CHECK(wld.has(we));
				CHECK(wld.valid(e));
				CHECK(wld.has(e));
			}
		}

		// SafeEntity went out of scope and we did request delete.
		// The entity needs to be dead now.
		CHECK_FALSE(wld.valid(e));
		CHECK_FALSE(wld.has(e));
		CHECK_FALSE(wld.valid(we));
		CHECK_FALSE(wld.has(we));
	}

	SUBCASE("weak, safes with delete") {
		TestWorld twld;

		auto e = wld.add();
		CHECK(wld.valid(e));
		CHECK(wld.has(e));

		auto we = ecs::WeakEntity(wld, e);

		{
			auto se0 = ecs::SafeEntity(wld, e);

			{
				auto se = ecs::SafeEntity(wld, e);
				CHECK(se == e);

				CHECK(wld.valid(we));
				CHECK(wld.has(we));
				CHECK(wld.valid(e));
				CHECK(wld.has(e));

				// We can call del as many times as we want. So long a SafeEntity is around,
				// we won't be able to delete the entity.
				GAIA_FOR(10) {
					wld.del(e);
					CHECK(wld.valid(we));
					CHECK(wld.has(we));
					CHECK(wld.valid(e));
					CHECK(wld.has(e));
				}
			}

			// SafeEntity went out of scope and we did request delete.
			// However, se0 is still in scope, so no delete should happen no matter
			// how many times we try.
			GAIA_FOR(10) {
				wld.del(e);
				CHECK(wld.valid(we));
				CHECK(wld.has(we));
				CHECK(wld.valid(e));
				CHECK(wld.has(e));
			}
		}

		// SafeEntity went out of scope and we did request delete.
		// The entity needs to be dead now.
		CHECK_FALSE(wld.valid(we));
		CHECK_FALSE(wld.has(we));
		CHECK_FALSE(wld.valid(e));
		CHECK_FALSE(wld.has(e));
	}

	SUBCASE("component") {
		TestWorld twld;

		struct WeakComponent {
			ecs::WeakEntity entity;
		};

		auto e = wld.add();

		auto e2 = wld.add();
		wld.add<WeakComponent>(e2, {ecs::WeakEntity(wld, e)});

		const auto& wc = wld.get<WeakComponent>(e2);
		CHECK(wc.entity == e);

		CHECK(wld.valid(wc.entity));
		CHECK(wld.has(wc.entity));

		wld.del(e);
		CHECK_FALSE(wld.valid(wc.entity));
		CHECK_FALSE(wld.has(wc.entity));
	}
}
#endif

TEST_CASE("Add - no components") {
	const uint32_t N = 1'500;

	TestWorld twld;
	cnt::darr<ecs::Entity> ents, arr;
	ents.reserve(N);
	arr.reserve(N);

	auto create = [&]() {
		auto e = wld.add();
		ents.push_back(e);
	};
	auto verify = [&](uint32_t i) {
		const auto a = arr[i + 2];
		const auto e = ents[i];
		CHECK(a == e);
	};

	GAIA_FOR(N) create();

	auto q = wld.query().no<ecs::Component>().no<ecs::Core_>();
	q.arr(arr);
	CHECK(arr.size() - 2 == ents.size()); // 2 for (OnDelete, Error) and (OnTargetDelete, Error)

	GAIA_FOR(N) verify(i);
}

TEST_CASE("Add - 1 component") {
	const uint32_t N = 1'500;

	TestWorld twld;
	cnt::darr<ecs::Entity> ents;
	ents.reserve(N);

	auto create = [&](uint32_t i) {
		auto e = wld.add();
		ents.push_back(e);
		wld.add<Int3>(e, {i, i, i});
	};
	auto verify = [&](uint32_t i) {
		auto e = ents[i];

		CHECK(wld.has<Int3>(e));
		auto val = wld.get<Int3>(e);
		CHECK(val.x == i);
		CHECK(val.y == i);
		CHECK(val.z == i);
	};

	GAIA_FOR(N) create(i);
	GAIA_FOR(N) verify(i);

	{
		const auto& p = wld.add<Int3>();
		auto e = wld.add();
		wld.add(e, p.entity, Int3{1, 2, 3});

		CHECK(wld.has(e, p.entity));
		CHECK(wld.has<Int3>(e));
		auto val0 = wld.get<Int3>(e);
		CHECK(val0.x == 1);
		CHECK(val0.y == 2);
		CHECK(val0.z == 3);
	}
}

namespace dummy {
	struct Position {
		float x;
		float y;
		float z;
	};
} // namespace dummy

TEST_CASE("Add - namespaces") {
	TestWorld twld;
	auto e = wld.add();
	wld.add<Position>(e, {1, 1, 1});
	wld.add<dummy::Position>(e);
	auto e2 = wld.add();
	wld.add<Position>(e2);
	auto a3 = wld.add();
	wld.add<dummy::Position>(a3, {7, 7, 7});
	auto a4 = wld.copy(a3);
	(void)a4;

	CHECK(wld.has<Position>(e));
	CHECK(wld.has<dummy::Position>(e));

	{
		auto p1 = wld.get<Position>(e);
		CHECK(p1.x == 1.f);
		CHECK(p1.y == 1.f);
		CHECK(p1.z == 1.f);
		// auto p2 = wld.get<dummy::Position>(e); commented, value added without being initialized to anything
		// CHECK(p2.x == 0.f);
		// CHECK(p2.y == 0.f);
		// CHECK(p2.z == 0.f);
	}
	{
		// auto p = wld.get<Position>(e2); commented, value added without being initialized to anything
		// CHECK(p.x == 1.f);
		// CHECK(p.y == 1.f);
		// CHECK(p.z == 1.f);
	}
	{
		auto p1 = wld.get<dummy::Position>(a3);
		CHECK(p1.x == 7.f);
		CHECK(p1.y == 7.f);
		CHECK(p1.z == 7.f);
		auto p2 = wld.get<dummy::Position>(a4);
		CHECK(p2.x == 7.f);
		CHECK(p2.y == 7.f);
		CHECK(p2.z == 7.f);
	}
	{
		auto q0 = wld.query().all<PositionSoA>();
		const auto c0 = q0.count();
		CHECK(c0 == 0); // nothing

		auto q1 = wld.query().all<Position>();
		const auto c1 = q1.count();
		CHECK(c1 == 2); // e, e2

		auto q2 = wld.query().all<dummy::Position>();
		const auto c2 = q2.count();
		CHECK(c2 == 3); // e, a3, a4

		auto q3 = wld.query().no<Position>();
		const auto c3 = q3.count();
		CHECK(c3 > 0); // It's going to be a bunch

		auto q4 = wld.query().all<dummy::Position>().no<Position>();
		const auto c4 = q4.count();
		CHECK(c4 == 2); // a3, a4
	}
}

TEST_CASE("Add - many components") {
	TestWorld twld;

	auto create = [&]() {
		auto e = wld.add();

		wld.add<Int3>(e, {3, 3, 3});
		wld.add<Position>(e, {1, 1, 1});
		wld.add<Empty>(e);
		wld.add<Else>(e, {true});
		wld.add<Rotation>(e, {2, 2, 2, 2});
		wld.add<Scale>(e, {4, 4, 4});

		CHECK(wld.has<Int3>(e));
		CHECK(wld.has<Position>(e));
		CHECK(wld.has<Empty>(e));
		CHECK(wld.has<Rotation>(e));
		CHECK(wld.has<Scale>(e));

		{
			auto val = wld.get<Int3>(e);
			CHECK(val.x == 3);
			CHECK(val.y == 3);
			CHECK(val.z == 3);
		}
		{
			auto val = wld.get<Position>(e);
			CHECK(val.x == 1.f);
			CHECK(val.y == 1.f);
			CHECK(val.z == 1.f);
		}
		{
			auto val = wld.get<Rotation>(e);
			CHECK(val.x == 2.f);
			CHECK(val.y == 2.f);
			CHECK(val.z == 2.f);
			CHECK(val.w == 2.f);
		}
		{
			auto val = wld.get<Scale>(e);
			CHECK(val.x == 4.f);
			CHECK(val.y == 4.f);
			CHECK(val.z == 4.f);
		}
	};

	const uint32_t N = 1'500;
	GAIA_FOR(N) create();
}

TEST_CASE("Add - many components, bulk") {
	TestWorld twld;

	auto create = [&]() {
		auto e = wld.add();

		auto b = wld.build(e);
		(void)b;
		wld.build(e).add<Int3>().add<Position>().add<Empty>().add<Else>().add<Rotation>().add<Scale>();

		CHECK(wld.has<Int3>(e));
		CHECK(wld.has<Position>(e));
		CHECK(wld.has<Empty>(e));
		CHECK(wld.has<Else>(e));
		CHECK(wld.has<Rotation>(e));
		CHECK(wld.has<Scale>(e));

		wld.acc_mut(e)
				.set<Int3>({3, 3, 3})
				.set<Position>({1, 1, 1})
				.set<Else>({true})
				.set<Rotation>({2, 2, 2, 2})
				.set<Scale>({4, 4, 4});

		{
			auto val = wld.get<Int3>(e);
			CHECK(val.x == 3);
			CHECK(val.y == 3);
			CHECK(val.z == 3);
		}
		{
			auto val = wld.get<Position>(e);
			CHECK(val.x == 1.f);
			CHECK(val.y == 1.f);
			CHECK(val.z == 1.f);
		}
		{
			auto val = wld.get<Rotation>(e);
			CHECK(val.x == 2.f);
			CHECK(val.y == 2.f);
			CHECK(val.z == 2.f);
			CHECK(val.w == 2.f);
		}
		{
			auto val = wld.get<Scale>(e);
			CHECK(val.x == 4.f);
			CHECK(val.y == 4.f);
			CHECK(val.z == 4.f);
		}

		{
			auto setter = wld.acc_mut(e);
			auto& pos = setter.mut<Int3>();
			pos = {30, 30, 30};

			{
				auto val = wld.get<Int3>(e);
				CHECK(val.x == 30);
				CHECK(val.y == 30);
				CHECK(val.z == 30);

				val = setter.get<Int3>();
				CHECK(val.x == 30);
				CHECK(val.y == 30);
				CHECK(val.z == 30);
			}
		}

		wld.clear(e);
		CHECK_FALSE(wld.has<Int3>(e));
		CHECK_FALSE(wld.has<Position>(e));
		CHECK_FALSE(wld.has<Empty>(e));
		CHECK_FALSE(wld.has<Else>(e));
		CHECK_FALSE(wld.has<Rotation>(e));
		CHECK_FALSE(wld.has<Scale>(e));
	};

	const uint32_t N = 1'500;
	GAIA_FOR(N) create();
}

TEST_CASE("Add - many components, bulk 2") {
	TestWorld twld;
	auto e = wld.add();

	auto checkInt3 = [&]() {
		CHECK(wld.has<Int3>(e));
		wld.acc_mut(e).set<Int3>({3, 3, 3});

		{
			auto val = wld.get<Int3>(e);
			CHECK(val.x == 3);
			CHECK(val.y == 3);
			CHECK(val.z == 3);
		}

		{
			auto setter = wld.acc_mut(e);
			auto& pos = setter.mut<Int3>();
			pos = {30, 30, 30};

			{
				auto val = wld.get<Int3>(e);
				CHECK(val.x == 30);
				CHECK(val.y == 30);
				CHECK(val.z == 30);

				val = setter.get<Int3>();
				CHECK(val.x == 30);
				CHECK(val.y == 30);
				CHECK(val.z == 30);
			}
		}
	};

	// This should result in adding just Int3
	wld.build(e).add<Position>().add<Int3>().del<Position>();
	CHECK_FALSE(wld.has<Position>(e));
	checkInt3();

	// This should do nothing
	wld.build(e).add<Position>().del<Position>();
	CHECK_FALSE(wld.has<Position>(e));
	checkInt3();
}

TEST_CASE("Pair") {
	{
		TestWorld twld;
		auto a = wld.add();
		auto b = wld.add();
		auto p = ecs::Pair(a, b);
		CHECK(p.first() == a);
		CHECK(p.second() == b);
		auto pe = (ecs::Entity)p;
		CHECK(wld.get(pe.id()) == a);
		CHECK(wld.get(pe.gen()) == b);
	}
	{
		TestWorld twld;
		auto a = wld.add<Position>().entity;
		auto b = wld.add<ecs::Requires_>().entity;
		auto p = ecs::Pair(a, b);
		CHECK(ecs::is_pair<decltype(p)>::value);
		CHECK(p.first() == a);
		CHECK(p.second() == b);
		auto pe = (ecs::Entity)p;
		CHECK_FALSE(ecs::is_pair<decltype(pe)>::value);
		CHECK(wld.get(pe.id()) == a);
		CHECK(wld.get(pe.gen()) == b);
	}
	struct Start {};
	struct Stop {};
	{
		CHECK(ecs::is_pair<ecs::pair<Start, Position>>::value);
		CHECK(ecs::is_pair<ecs::pair<Position, Start>>::value);

		using Pair1 = ecs::pair<Start, Position>;
		static_assert(std::is_same_v<Pair1::rel, Start>);
		static_assert(std::is_same_v<Pair1::tgt, Position>);
		using Pair1Actual = ecs::actual_type_t<Pair1>;
		static_assert(std::is_same_v<Pair1Actual::Type, Position>);

		using Pair2 = ecs::pair<Start, ecs::uni<Position>>;
		static_assert(std::is_same_v<Pair2::rel, Start>);
		static_assert(std::is_same_v<Pair2::tgt, ecs::uni<Position>>);
		using Pair2Actual = ecs::actual_type_t<Pair2>;
		static_assert(std::is_same_v<Pair2Actual::Type, Position>);
		static_assert(std::is_same_v<Pair2Actual::TypeFull, ecs::uni<Position>>);

		TestWorld twld;
		const auto& pci = wld.add<Position>();
		const auto& upci = wld.add<ecs::uni<Position>>();
		using TestPair = ecs::pair<Position, ecs::uni<Position>>;
		const auto& pci2 = wld.add<TestPair::rel>();
		const auto& upci2 = wld.add<TestPair::tgt>();
		CHECK(pci.entity == pci2.entity);
		CHECK(upci.entity == upci2.entity);
	}
	{
		TestWorld twld;
		auto eStart = wld.add<Start>().entity;
		auto eStop = wld.add<Stop>().entity;
		auto ePos = wld.add<Position>().entity;
		auto e = wld.add();

		wld.add<Position>(e, {5, 5, 5});
		wld.add(e, {eStart, ePos});
		wld.add(e, {eStop, ePos});
		auto p = wld.get<Position>(e);
		CHECK(p.x == 5);
		CHECK(p.y == 5);
		CHECK(p.z == 5);

		wld.add<ecs::pair<Start, ecs::uni<Position>>>(e, {50, 50, 50}); // 19, 14:19
		auto spu = wld.get<ecs::pair<Start, ecs::uni<Position>>>(e);
		CHECK(spu.x == 50);
		CHECK(spu.y == 50);
		CHECK(spu.z == 50);
		CHECK(p.x == 5);
		CHECK(p.y == 5);
		CHECK(p.z == 5);

		wld.add<ecs::pair<Start, Position>>(e, {100, 100, 100}); // 14:18
		auto sp = wld.get<ecs::pair<Start, Position>>(e);
		CHECK(sp.x == 100);
		CHECK(sp.y == 100);
		CHECK(sp.z == 100);

		p = wld.get<Position>(e);
		spu = wld.get<ecs::pair<Start, ecs::uni<Position>>>(e);
		CHECK(p.x == 5);
		CHECK(p.y == 5);
		CHECK(p.z == 5);
		CHECK(spu.x == 50);
		CHECK(spu.y == 50);
		CHECK(spu.z == 50);

		{
			uint32_t i = 0;
			auto q = wld.query().all<ecs::pair<Start, Position>>();
			q.each([&]() {
				++i;
			});
			CHECK(i == 1);
		}
	}
	{
		TestWorld twld;

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

		{
			uint32_t i = 0;
			auto q = wld.query().all(ecs::Pair{eats, ecs::All});
			q.each([&]() {
				++i;
			});
			CHECK(i == 2);
		}
		{
			uint32_t i = 0;
			auto q = wld.query().all(ecs::Pair{drinks, ecs::All});
			q.each([&]() {
				++i;
			});
			CHECK(i == 2);
		}
		{
			uint32_t i = 0;
			auto q = wld.query().all(ecs::Pair{drinks, water}).all(ecs::Pair{eats, ecs::All});
			q.each([&]() {
				++i;
			});
			CHECK(i == 1);
		}
		{
			uint32_t i = 0;
			auto q = wld.query().all(ecs::Pair{eats, ecs::All}).all(ecs::Pair{drinks, water});
			q.each([&]() {
				++i;
			});
			CHECK(i == 1);
		}
		{
			uint32_t i = 0;
			auto q = wld.query().all(ecs::Pair{eats, ecs::All}).all(ecs::Pair{drinks, ecs::All});
			q.each([&]() {
				++i;
			});
			CHECK(i == 2);
		}
	}
}

TEST_CASE("CantCombine") {
	SUBCASE("One") {
		TestWorld twld;
		auto weak = wld.add();
		auto strong = wld.add();
		wld.add(weak, {ecs::CantCombine, strong});

		auto dummy = wld.add();
		wld.add(dummy, strong);
#if !GAIA_ASSERT_ENABLED
		// Can be tested only with asserts disabled because the situation is assert-protected.
		wld.add(dummy, weak);
		CHECK(wld.has(dummy, strong));
		CHECK_FALSE(wld.has(dummy, weak));
#endif

		// Unset
		wld.del(weak, {ecs::CantCombine, strong});
		wld.add(dummy, weak);
		CHECK(wld.has(dummy, strong));
		CHECK(wld.has(dummy, weak));
	}
	SUBCASE("Two") {
		TestWorld twld;
		auto weak = wld.add();
		auto strong = wld.add();
		auto stronger = wld.add();
		wld.add(weak, {ecs::CantCombine, strong});
		wld.add(weak, {ecs::CantCombine, stronger});

		auto dummy = wld.add();
		wld.add(dummy, strong);
#if !GAIA_ASSERT_ENABLED
		// Can be tested only with asserts disabled because the situation is assert-protected.
		wld.add(dummy, weak);
		CHECK(wld.has(dummy, strong));
		CHECK_FALSE(wld.has(dummy, weak));
#endif
		wld.add(dummy, stronger);
#if !GAIA_ASSERT_ENABLED
		// Can be tested only with asserts disabled because the situation is assert-protected.
		wld.add(dummy, weak);
		CHECK(wld.has(dummy, strong));
		CHECK(wld.has(dummy, stronger));
		CHECK_FALSE(wld.has(dummy, weak));
#endif

		// Unset strong. Still should not be able to add because of stronger.
		wld.del(weak, {ecs::CantCombine, strong});
#if !GAIA_ASSERT_ENABLED
		// Can be tested only with asserts disabled because the situation is assert-protected.
		wld.add(dummy, weak);
		CHECK(wld.has(dummy, strong));
		CHECK(wld.has(dummy, stronger));
		CHECK_FALSE(wld.has(dummy, weak));
#endif

		// Unset. Finally should be able to add because there are no more restrictions
		wld.del(weak, {ecs::CantCombine, stronger});
		wld.add(dummy, weak);
		CHECK(wld.has(dummy, strong));
		CHECK(wld.has(dummy, stronger));
		CHECK(wld.has(dummy, weak));
	}
}

TEST_CASE("Requires") {
	TestWorld twld;
	auto rabbit = wld.add();
	auto animal = wld.add();
	auto herbivore = wld.add();
	auto carrot = wld.add();
	wld.add(carrot, {ecs::Requires, herbivore});
	wld.add(herbivore, {ecs::Requires, animal});

	wld.add(rabbit, carrot);
	CHECK(wld.has(rabbit, herbivore));
	CHECK(wld.has(rabbit, animal));

	wld.del(rabbit, animal);
	CHECK(wld.has(rabbit, animal));

	wld.del(rabbit, herbivore);
	CHECK(wld.has(rabbit, herbivore));

	wld.del(rabbit, carrot);
	CHECK_FALSE(wld.has(rabbit, carrot));
}

TEST_CASE("Inheritance (Is)") {
	TestWorld twld;
	ecs::Entity animal = wld.add();
	ecs::Entity herbivore = wld.add();
	ecs::Entity carnivore = wld.add();
	wld.add<Position>(herbivore, {});
	wld.add<Rotation>(herbivore, {});
	ecs::Entity rabbit = wld.add();
	ecs::Entity hare = wld.add();
	ecs::Entity wolf = wld.add();

	wld.as(carnivore, animal);
	wld.as(herbivore, animal);
	wld.as(rabbit, herbivore);
	wld.as(hare, herbivore);
	wld.as(wolf, carnivore);

	CHECK(wld.is(carnivore, animal));
	CHECK(wld.is(herbivore, animal));
	CHECK(wld.is(rabbit, animal));
	CHECK(wld.is(hare, animal));
	CHECK(wld.is(wolf, animal));
	CHECK(wld.is(rabbit, herbivore));
	CHECK(wld.is(hare, herbivore));
	CHECK(wld.is(wolf, carnivore));

	CHECK(wld.is(animal, animal));
	CHECK(wld.is(herbivore, herbivore));
	CHECK(wld.is(carnivore, carnivore));

	CHECK_FALSE(wld.is(animal, herbivore));
	CHECK_FALSE(wld.is(animal, carnivore));
	CHECK_FALSE(wld.is(wolf, herbivore));
	CHECK_FALSE(wld.is(rabbit, carnivore));
	CHECK_FALSE(wld.is(hare, carnivore));

	{
		uint32_t i = 0;
		ecs::Query q = wld.query().all(ecs::Pair(ecs::Is, animal));
		q.each([&](ecs::Entity entity) {
			const bool isOK = entity == animal || entity == hare || entity == rabbit || entity == herbivore ||
												entity == carnivore || entity == wolf;
			CHECK(isOK);

			++i;
		});
		CHECK(i == 6);
	}
	{
		uint32_t i = 0;
		ecs::Query q = wld.query().all(ecs::Pair(ecs::Is, herbivore));
		q.each([&](ecs::Entity entity) {
			const bool isOK = entity == hare || entity == rabbit || entity == herbivore;
			CHECK(isOK);

			++i;
		});
		CHECK(i == 3);
	}
	{
		uint32_t i = 0;
		ecs::Query q = wld.query().all(ecs::Pair(ecs::Is, animal)).no(herbivore);
		q.each([&](ecs::Entity entity) {
			const bool isOK = entity == animal || entity == hare || entity == rabbit || entity == wolf || entity == carnivore;
			CHECK(isOK);

			++i;
		});
		CHECK(i == 5);
	}
	{
		uint32_t i = 0;
		ecs::Query q = wld.query().all(ecs::Pair(ecs::Is, animal)).no(ecs::Pair(ecs::Is, herbivore));
		q.each([&](ecs::Entity entity) {
			const bool isOK = entity == animal || entity == wolf || entity == carnivore;
			CHECK(isOK);

			++i;
		});
		CHECK(i == 3);
	}
	{
		uint32_t i = 0;
		ecs::Query q = wld.query().any(ecs::Pair(ecs::Is, animal)).no(ecs::Pair(ecs::Is, herbivore));
		q.each([&](ecs::Entity entity) {
			const bool isOK = entity == animal || entity == wolf || entity == carnivore;
			CHECK(isOK);

			++i;
		});
		CHECK(i == 3);
	}
	{
		uint32_t i = 0;
		ecs::Query q = wld.query().any(ecs::Pair(ecs::Is, animal)).no(ecs::Pair(ecs::Is, carnivore));
		q.each([&](ecs::Entity entity) {
			const bool isOK = entity == animal || entity == hare || entity == rabbit || entity == herbivore;
			CHECK(isOK);

			++i;
		});
		CHECK(i == 4);
	}
}

TEST_CASE("Inheritance (Is) - change") {
	TestWorld twld;
	ecs::Entity animal = wld.add();
	ecs::Entity herbivore = wld.add();
	ecs::Entity carnivore = wld.add();
	ecs::Entity wolf = wld.add();

	wld.as(carnivore, animal);
	wld.as(herbivore, animal);
	wld.as(wolf, carnivore);

	CHECK(wld.is(carnivore, animal));
	CHECK(wld.is(herbivore, animal));
	CHECK(wld.is(wolf, animal));

	CHECK(wld.is(animal, animal));
	CHECK(wld.is(herbivore, herbivore));
	CHECK(wld.is(carnivore, carnivore));
	CHECK(wld.is(wolf, carnivore));

	CHECK_FALSE(wld.is(animal, herbivore));
	CHECK_FALSE(wld.is(animal, carnivore));
	CHECK_FALSE(wld.is(wolf, herbivore));

	ecs::Query q = wld.query().all(ecs::Pair(ecs::Is, animal));

	{
		uint32_t i = 0;
		q.each([&](ecs::Entity entity) {
			const bool isOK = entity == animal || entity == herbivore || entity == carnivore || entity == wolf;
			CHECK(isOK);

			++i;
		});
		CHECK(i == 4);
	}

	// Carnivore is no longer an animal
	wld.del(carnivore, {ecs::Is, animal});
	CHECK(wld.is(wolf, carnivore));
	CHECK_FALSE(wld.is(carnivore, animal));
	CHECK_FALSE(wld.is(wolf, animal));

	{
		uint32_t i = 0;
		q.each([&](ecs::Entity entity) {
			const bool isOK = entity == animal || entity == herbivore;
			CHECK(isOK);

			++i;
		});
		CHECK(i == 2);
	}

	// Make carnivore an animal again
	wld.as(carnivore, animal);

	{
		uint32_t i = 0;
		q.each([&](ecs::Entity entity) {
			const bool isOK = entity == animal || entity == herbivore || entity == carnivore || entity == wolf;
			CHECK(isOK);

			++i;
		});
		CHECK(i == 4);
	}

	// Wolf is no longer a carnivore and thus no longer an animal.
	// It should no longer match q
	wld.del(wolf, {ecs::Is, carnivore});
	CHECK_FALSE(wld.is(wolf, carnivore));
	CHECK(wld.is(carnivore, animal));
	CHECK_FALSE(wld.is(wolf, animal));

	{
		uint32_t i = 0;
		q.each([&](ecs::Entity entity) {
			const bool isOK = entity == animal || entity == herbivore || entity == carnivore;
			CHECK(isOK);

			++i;
		});
		CHECK(i == 3);
	}
}

TEST_CASE("AddAndDel_entity - no components") {
	const uint32_t N = 1'500;

	TestWorld twld;
	cnt::darr<ecs::Entity> arr;
	arr.reserve(N);

	auto create = [&]() {
		auto e = wld.add();
		arr.push_back(e);
	};
	auto remove = [&](ecs::Entity e) {
		wld.del(e);
		const bool isValid = wld.valid(e);
		const bool hasEntity = wld.has(e);
		CHECK_FALSE(isValid);
		CHECK_FALSE(hasEntity);
	};

	// Create entities
	GAIA_FOR(N) create();
	// Remove entities
	GAIA_FOR(N) remove(arr[i]);

	wld.update();
	GAIA_FOR(N) {
		const auto e = arr[i];
		const bool isValid = wld.valid(e);
		const bool hasEntity = wld.has(e);
		CHECK_FALSE(isValid);
		CHECK_FALSE(hasEntity);
	}
}

TEST_CASE("AddAndDel_entity - 1 component") {
	const uint32_t N = 1'500;

	TestWorld twld;
	cnt::darr<ecs::Entity> arr;
	arr.reserve(N);

	auto create = [&](uint32_t id) {
		auto e = wld.add();
		arr.push_back(e);

		wld.add<Int3>(e, {id, id, id});
		auto pos = wld.get<Int3>(e);
		CHECK(pos.x == id);
		CHECK(pos.y == id);
		CHECK(pos.z == id);
		return e;
	};
	auto remove = [&](ecs::Entity e) {
		wld.del(e);
		const bool isValid = wld.valid(e);
		const bool hasEntity = wld.has(e);
		CHECK_FALSE(isValid);
		CHECK_FALSE(hasEntity);
	};

	GAIA_FOR(N) create(i);
	GAIA_FOR(N) remove(arr[i]);

	wld.update();
	GAIA_FOR(N) {
		const auto e = arr[i];
		const bool isValid = wld.valid(e);
		const bool hasEntity = wld.has(e);
		CHECK_FALSE(isValid);
		CHECK_FALSE(hasEntity);
	}
}

void verify_entity_has(const ecs::ComponentCache& cc, ecs::Entity entity) {
	const auto* res = cc.find(entity);
	CHECK(res != nullptr);
}

template <typename T>
void verify_name_has(const ecs::ComponentCache& cc, const char* str) {
	auto name = cc.get<T>().name;
	CHECK(name.str() != nullptr);
	CHECK(name.len() > 1);
	const auto* res = cc.find(str);
	CHECK(res != nullptr);
}

void verify_name_has_not(const ecs::ComponentCache& cc, const char* str) {
	const auto* item = cc.find(str);
	CHECK(item == nullptr);
}

template <typename T>
void verify_name_has_not(const ecs::ComponentCache& cc) {
	const auto* item = cc.find<T>();
	CHECK(item == nullptr);
}

#define verify_name_has(name) verify_name_has<name>(cc, #name);
#define verify_name_has_not(name)                                                                                      \
	{                                                                                                                    \
		verify_name_has_not<name>(cc);                                                                                     \
		verify_name_has_not(cc, #name);                                                                                    \
	}

TEST_CASE("Component names") {
	SUBCASE("direct registration") {
		TestWorld twld;
		const auto& cc = wld.comp_cache();

		verify_name_has_not(Int3);
		verify_name_has_not(Position);
		verify_name_has_not(dummy::Position);

		auto e_pos = wld.add<Position>().entity;
		verify_entity_has(cc, e_pos);
		verify_name_has_not(Int3);
		verify_name_has(Position);
		verify_name_has_not(dummy::Position);

		auto e_int = wld.add<Int3>().entity;
		verify_entity_has(cc, e_pos);
		verify_entity_has(cc, e_int);
		verify_name_has(Int3);
		verify_name_has(Position);
		verify_name_has_not(dummy::Position);

		auto e_dpos = wld.add<dummy::Position>().entity;
		verify_entity_has(cc, e_pos);
		verify_entity_has(cc, e_int);
		verify_entity_has(cc, e_dpos);
		verify_name_has(Int3);
		verify_name_has(Position);
		verify_name_has(dummy::Position);
	}
	SUBCASE("entity+component") {
		TestWorld twld;
		const auto& cc = wld.comp_cache();
		auto e = wld.add();

		verify_name_has_not(Int3);
		verify_name_has_not(Position);
		verify_name_has_not(dummy::Position);

		wld.add<Position>(e);
		verify_name_has_not(Int3);
		verify_name_has(Position);
		verify_name_has_not(dummy::Position);

		wld.add<Int3>(e);
		verify_name_has(Int3);
		verify_name_has(Position);
		verify_name_has_not(dummy::Position);

		wld.del<Position>(e);
		verify_name_has(Int3);
		verify_name_has(Position);
		verify_name_has_not(dummy::Position);

		wld.add<dummy::Position>(e);
		verify_name_has(Int3);
		verify_name_has(Position);
		verify_name_has(dummy::Position);
	}
}

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

		// Make sure the same entities are returned by each and arr
		// and that they match out expectations.
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

	// Verify querying at a different source entity works, too
	auto game = wld.add();
	struct Level {
		uint32_t value;
	};
	wld.add<Level>(game, {2});
	auto q4 = wld.query<UseCachedQuery>().template all<Position>().all(wld.add<Level>().entity, game);

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
			// auto pos_view = it.view<Position>();
			// auto lvl_view = src.view<Level>();
			GAIA_EACH(it) {
				++cnt2;
			}
		});
		CHECK(cnt == cnt2);
	}
}

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

		auto qq1_ = wld.query<UseCachedQuery>().add("Position; Rotation");
		auto qq2_ = wld.query<UseCachedQuery>().add("Rotation; Position");
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

		auto qq1_ = wld.query<UseCachedQuery>().add("Position; Rotation; Acceleration; Something");
		auto qq2_ = wld.query<UseCachedQuery>().add("Rotation; Something; Position; Acceleration");
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
			// Because "e" was moved to a new archetype nobody ever set the value.
			// Therefore, it is garbage and won't match the original chunk.
			auto p = wld.get<ecs::uni<Position>>(e);
			CHECK_FALSE(p.x == 1.f);
			CHECK_FALSE(p.y == 2.f);
			CHECK_FALSE(p.z == 3.f);
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
			// Because "e" was moved to a new archetype nobody ever set the value.
			// Therefore, it is garbage and won't match the original chunk.
			auto p = wld.get<ecs::uni<Position>>(e);
			CHECK_FALSE(p.x == 1.f);
			CHECK_FALSE(p.y == 2.f);
			CHECK_FALSE(p.z == 3.f);
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
			auto a = wld.get<ecs::uni<Acceleration>>(e);
			CHECK_FALSE(a.x == 4.f);
			CHECK_FALSE(a.y == 5.f);
			CHECK_FALSE(a.z == 6.f);
		}
		{
			// Because "e" was moved to a new archetype nobody ever set the value.
			// Therefore, it is garbage and won't match the original chunk.
			auto p = wld.get<ecs::uni<Position>>(e);
			CHECK_FALSE(p.x == 1.f);
			CHECK_FALSE(p.y == 2.f);
			CHECK_FALSE(p.z == 3.f);
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
			// Because "e" was moved to a new archetype nobody ever set the value.
			// Therefore, it is garbage and won't match the original chunk.
			auto p = wld.get<ecs::uni<Position>>(e);
			CHECK_FALSE(p.x == 1.f);
			CHECK_FALSE(p.y == 2.f);
			CHECK_FALSE(p.z == 3.f);
		}

		// Add a unique entity. Archetype changes.
		auto f = wld.add(ecs::EntityKind::EK_Uni);
		wld.add(e, f);
		CHECK(wld.has(e, f));

		CHECK(wld.has<ecs::uni<Position>>(e));
		CHECK(wld.has<ecs::uni<Acceleration>>(e));
		CHECK_FALSE(wld.has<Position>(e));
		CHECK_FALSE(wld.has<Acceleration>(e));
		{
			// Because "e" was moved to a new archetype nobody ever set the value.
			// Therefore, it is garbage and won't match the original chunk.
			auto a = wld.get<ecs::uni<Acceleration>>(e);
			CHECK_FALSE(a.x == 4.f);
			CHECK_FALSE(a.y == 5.f);
			CHECK_FALSE(a.z == 6.f);
		}
		{
			// Because "e" was moved to a new archetype nobody ever set the value.
			// Therefore, it is garbage and won't match the original chunk.
			auto p = wld.get<ecs::uni<Position>>(e);
			CHECK_FALSE(p.x == 1.f);
			CHECK_FALSE(p.y == 2.f);
			CHECK_FALSE(p.z == 3.f);
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
}

TEST_CASE("Entity name - entity only") {
	constexpr uint32_t N = 1'500;

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
		const auto* ename = wld.name(e);

		const auto l0 = strlen(tmp);
		const auto l1 = strlen(ename);
		CHECK(l0 == l1);
		CHECK(strcmp(tmp, ename) == 0);
	};

	SUBCASE("basic") {
		ents.clear();
		create();
		verify(0);
		auto e = ents[0];

		char original[MaxLen];
		GAIA_STRFMT(original, MaxLen, "%s", wld.name(e));

		// If we change the original string we still must have a match
		{
			GAIA_STRCPY(tmp, MaxLen, "some_random_string");
			CHECK(strcmp(wld.name(e), original) == 0);
			CHECK(wld.get(original) == e);
			CHECK(wld.get(tmp) == ecs::EntityBad);

			// Change the name back
			GAIA_STRCPY(tmp, MaxLen, original);
			verify(0);
		}

#if !GAIA_ASSERT_ENABLED
		// Renaming has to fail. Can be tested only with asserts disabled
		// because the situation is assert-protected.
		{
			auto e1 = wld.add();
			wld.name(e1, original);
			CHECK(wld.name(e1) == nullptr);
			CHECK(wld.get(original) == e);
		}
#endif

		wld.name(e, nullptr);
		CHECK(wld.get(original) == ecs::EntityBad);
		CHECK(wld.name(e) == nullptr);

		wld.name(e, original);
		wld.del(e);
		CHECK(wld.get(original) == ecs::EntityBad);
	}

	SUBCASE("basic - non-owned") {
		ents.clear();
		create_raw();
		verify(0);
		auto e = ents[0];

		char original[MaxLen];
		GAIA_STRFMT(original, MaxLen, "%s", wld.name(e));

		// If we change the original string we can't have a match
		{
			GAIA_STRCPY(tmp, MaxLen, "some_random_string");
			const auto* str = wld.name(e);
			CHECK(strcmp(str, "some_random_string") == 0);
			CHECK(wld.get(original) == ecs::EntityBad);
			// Hash was calculated for [original] but we changed the string to "some_random_string".
			// Hash won't match so we shouldn't be able to find the entity still.
			CHECK(wld.get("some_random_string") == ecs::EntityBad);
		}

		{
			// Change the name back
			GAIA_STRCPY(tmp, MaxLen, original);
			verify(0);
		}

		wld.name(e, nullptr);
		CHECK(wld.get(original) == ecs::EntityBad);
		CHECK(wld.name(e) == nullptr);

		wld.name_raw(e, original);
		wld.del(e);
		CHECK(wld.get(original) == ecs::EntityBad);
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

			char original[MaxLen];
			GAIA_STRFMT(original, MaxLen, "%s", wld.name(e));

			{
				wld.enable(e, false);
				const auto* str = wld.name(e);
				CHECK(strcmp(str, original) == 0);
				CHECK(e == wld.get(original));
			}
			{
				wld.enable(e, true);
				const auto* str = wld.name(e);
				CHECK(strcmp(str, original) == 0);
				CHECK(e == wld.get(original));
			}
		}
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
		// name must match
		const char* name = wld.name(pci.entity);
		CHECK(strcmp(name, "Position") == 0);
		const auto e = wld.get("Position");
		CHECK(e == pci.entity);
	}
	// Add unique component
	const auto& upci = wld.add<ecs::uni<Position>>();
	{
		// name must match
		const char* name = wld.name(upci.entity);
		CHECK(strcmp(name, "gaia::ecs::uni<Position>") == 0);
		const auto e = wld.get("gaia::ecs::uni<Position>");
		CHECK(e == upci.entity);
	}
	{
		// generic component name must still match
		const char* name = wld.name(pci.entity);
		CHECK(strcmp(name, "Position") == 0);
		const auto e = wld.get("Position");
		CHECK(e == pci.entity);
	}

	// Change the component name
	wld.name(pci.entity, "xyz", 3);
	{
		// name must match
		const char* name = wld.name(pci.entity);
		CHECK(strcmp(name, "xyz") == 0);
		const auto e = wld.get("xyz");
		CHECK(e == pci.entity);
	}
	{
		// unique component name must still match
		const char* name = wld.name(upci.entity);
		CHECK(strcmp(name, "gaia::ecs::uni<Position>") == 0);
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

		const auto* e1name = wld.name(e1);
		CHECK(e1name == pTestStr);
		const auto* e2name = wld.name(e2);
		CHECK(e2name == nullptr);
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
			const auto* e1name = wld.name(e1);
			CHECK(e1name == pTestStr);
			const auto& p1 = wld.get<PositionNonTrivial>(e1);
			CHECK(p1.x == 1.f);
			CHECK(p1.y == 2.f);
			CHECK(p1.z == 3.f);

			for (auto ent: ents) {
				const auto* e2name = wld.name(ent);
				CHECK(e2name == nullptr);
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
			const auto* e1name = wld.name(e1);
			CHECK(e1name == pTestStr);
			const auto& p1 = wld.get<PositionNonTrivial>(e1);
			CHECK(p1.x == 1.f);
			CHECK(p1.y == 2.f);
			CHECK(p1.z == 3.f);

			for (auto ent: ents) {
				const auto* e2name = wld.name(ent);
				CHECK(e2name == nullptr);
				const auto& p2 = wld.get<PositionNonTrivial>(ent);
				CHECK(p2.x == 1.f);
				CHECK(p2.y == 2.f);
				CHECK(p2.z == 3.f);
			}
		}
	}
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
		ecs::Query q = wld.query().any<Position>().any<Acceleration>();

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
		ecs::Query q = wld.query().any<Position>().any<Acceleration>().all<Scale>();

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
		ecs::Query q = wld.query().any<Position>().any<Acceleration>().all<PositionSoA>();

		uint32_t cnt = 0;
		q.each([&]() {
			++cnt;
		});
		CHECK(cnt == 0);
	}
	{
		ecs::Query q = wld.query().any<Position>().any<Acceleration>().no<Scale>();

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
		auto q = wld.query().any<ecs::uni<Position>>().any<ecs::uni<Acceleration>>();

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
		auto q = wld.query().any<ecs::uni<Position>>().any<ecs::uni<Acceleration>>().all<ecs::uni<Scale>>();

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
		auto q = wld.query().any<ecs::uni<Position>>().any<ecs::uni<Acceleration>>().no<ecs::uni<Scale>>();

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

	auto s1 = wld.system().all<Position>().on_each([]() {}).entity();
	auto s2 = wld.system().on_each([]() {}).entity();

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

	auto q_ = wld.query().any(e1).any(e2);
	CHECK(q_.count() == 0);
	auto q = wld.query().all(e1).all(e2);
	CHECK(q.count() == 0);

	auto e3 = wld.add();
	wld.add(e3, e3);
	wld.add(e3, e1);
	wld.add(e3, e2);
	CHECK(q.count() == 1);
	CHECK(q_.count() == 1);

	auto e4 = wld.add();
	wld.add(e4, e4);
	wld.add(e4, e1);
	wld.add(e4, e2);
	CHECK(q_.count() == 2);
	CHECK(q.count() == 2);
}

TEST_CASE("add_n") {
	TestWorld twld;

	auto qa = wld.query().all<Acceleration>();
	auto qp = wld.query().all<Position>();

	auto e = wld.add();
	wld.add<Acceleration>(e, {1.f, 1.f, 1.f});
	wld.add<Position>(e, {2.f, 2.f, 2.f});

	constexpr uint32_t N = 1000;

	wld.add_n(e, N);

	{
		uint32_t cnt = 0;
		qa.each([&](ecs::Entity ent, const Acceleration& a) {
			++cnt;

			if (ent == e) {
				CHECK(a.x == 1.f);
				CHECK(a.y == 1.f);
				CHECK(a.z == 1.f);
			} else {
				// CHECK(a.x == 0.f);
				// CHECK(a.y == 0.f);
				// CHECK(a.z == 0.f);
			}
		});
		CHECK(cnt == N + 1);
	}
	{
		uint32_t cnt = 0;
		qp.each([&](ecs::Entity ent, const Position& p) {
			++cnt;

			if (ent == e) {
				CHECK(p.x == 2.f);
				CHECK(p.y == 2.f);
				CHECK(p.z == 2.f);
			} else {
				// CHECK(p.x == 0.f);
				// CHECK(p.y == 0.f);
				// CHECK(p.z == 0.f);
			}
		});
		CHECK(cnt == N + 1);
	}
}

TEST_CASE("copy_n") {
	TestWorld twld;

	auto qa = wld.query().all<Acceleration>();
	auto qp = wld.query().all<Position>();

	auto e = wld.add();
	wld.add<Acceleration>(e, {1.f, 1.f, 1.f});
	wld.add<Position>(e, {2.f, 2.f, 2.f});

	{
		const auto& a = wld.get<Acceleration>(e);
		CHECK(a.x == 1.f);
		CHECK(a.y == 1.f);
		CHECK(a.z == 1.f);
	}
	{
		const auto& p = wld.get<Position>(e);
		CHECK(p.x == 2.f);
		CHECK(p.y == 2.f);
		CHECK(p.z == 2.f);
	}

	constexpr uint32_t N = 1000;

	wld.copy_n(e, N);

	{
		uint32_t cnt = 0;
		qa.each([&](const Acceleration& a) {
			++cnt;

			CHECK(a.x == 1.f);
			CHECK(a.y == 1.f);
			CHECK(a.z == 1.f);
		});
		CHECK(cnt == N + 1);
	}
	{
		uint32_t cnt = 0;
		qp.each([&](const Position& p) {
			++cnt;

			CHECK(p.x == 2.f);
			CHECK(p.y == 2.f);
			CHECK(p.z == 2.f);
		});
		CHECK(cnt == N + 1);
	}
}

TEST_CASE("Set - generic") {
	TestWorld twld;

	constexpr uint32_t N = 100;
	constexpr uint32_t NE = 10;
	cnt::darr<ecs::Entity> arr;
	arr.reserve(N);
	cnt::darr<ecs::Entity> arre;
	arr.reserve(NE);

	GAIA_FOR(NE) {
		auto e = wld.add();
		arre.push_back(e);
	}

	GAIA_FOR(N) {
		const auto ent = wld.add();
		arr.push_back(ent);
		wld.add<Position>(ent, {});
		wld.add<Rotation>(ent, {});
		wld.add<Scale>(ent, {});
		wld.add<Acceleration>(ent, {});
		wld.add<Else>(ent, {});
		GAIA_FOR_(NE, j) {
			auto e = arre[j];
			wld.add(ent, e);
		}
	}

	// Default values
	for (const auto ent: arr) {
		auto r = wld.get<Rotation>(ent);
		CHECK(r.x == 0.f);
		CHECK(r.y == 0.f);
		CHECK(r.z == 0.f);
		CHECK(r.w == 0.f);

		auto s = wld.get<Scale>(ent);
		CHECK(s.x == 0.f);
		CHECK(s.y == 0.f);
		CHECK(s.z == 0.f);

		auto e = wld.get<Else>(ent);
		CHECK(e.value == false);
	}

	// Modify values
	{
		ecs::Query q = wld.query().all<Rotation&>().all<Scale&>().all<Else&>();

		q.each([&](ecs::Iter& it) {
			auto rotationView = it.view_mut<Rotation>();
			auto scaleView = it.view_mut<Scale>();
			auto elseView = it.view_mut<Else>();

			GAIA_EACH(it) {
				rotationView[i] = {1, 2, 3, 4};
				scaleView[i] = {11, 22, 33};
				elseView[i] = {true};
			}
		});

		q.each([&](ecs::Iter& it) {
			auto rotationView = it.view<Rotation>();
			auto scaleView = it.view<Scale>();
			auto elseView = it.view<Else>();

			GAIA_EACH(it) {
				auto r = rotationView[i];
				CHECK(r.x == 1.f);
				CHECK(r.y == 2.f);
				CHECK(r.z == 3.f);
				CHECK(r.w == 4.f);

				auto s = scaleView[i];
				CHECK(s.x == 11.f);
				CHECK(s.y == 22.f);
				CHECK(s.z == 33.f);

				auto e = elseView[i];
				CHECK(e.value == true);
			}
		});

		{
			uint32_t entIdx = 0;
			q.each([&](ecs::Entity ent) {
				CHECK(ent == arr[entIdx++]);
			});
			entIdx = 0;
			q.each([&](ecs::Iter& it) {
				auto entityView = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					CHECK(entityView[i] == arr[entIdx++]);
				}
			});
		}

		for (const auto ent: arr) {
			auto r = wld.get<Rotation>(ent);
			CHECK(r.x == 1.f);
			CHECK(r.y == 2.f);
			CHECK(r.z == 3.f);
			CHECK(r.w == 4.f);

			auto s = wld.get<Scale>(ent);
			CHECK(s.x == 11.f);
			CHECK(s.y == 22.f);
			CHECK(s.z == 33.f);

			auto e = wld.get<Else>(ent);
			CHECK(e.value == true);
		}
	}

	// Modify values + view idx
	{
		ecs::Query q = wld.query().all<Rotation&>().all<Scale&>().all<Else&>();

		q.each([&](ecs::Iter& it) {
			auto rotationView = it.view_mut<Rotation>(0);
			auto scaleView = it.view_mut<Scale>(1);
			auto elseView = it.view_mut<Else>(2);

			GAIA_EACH(it) {
				rotationView[i] = {1, 2, 3, 4};
				scaleView[i] = {11, 22, 33};
				elseView[i] = {true};
			}
		});

		q.each([&](ecs::Iter& it) {
			auto rotationView = it.view<Rotation>(0);
			auto scaleView = it.view<Scale>(1);
			auto elseView = it.view<Else>(2);

			GAIA_EACH(it) {
				auto r = rotationView[i];
				CHECK(r.x == 1.f);
				CHECK(r.y == 2.f);
				CHECK(r.z == 3.f);
				CHECK(r.w == 4.f);

				auto s = scaleView[i];
				CHECK(s.x == 11.f);
				CHECK(s.y == 22.f);
				CHECK(s.z == 33.f);

				auto e = elseView[i];
				CHECK(e.value == true);
			}
		});

		for (const auto ent: arr) {
			auto r = wld.get<Rotation>(ent);
			CHECK(r.x == 1.f);
			CHECK(r.y == 2.f);
			CHECK(r.z == 3.f);
			CHECK(r.w == 4.f);

			auto s = wld.get<Scale>(ent);
			CHECK(s.x == 11.f);
			CHECK(s.y == 22.f);
			CHECK(s.z == 33.f);

			auto e = wld.get<Else>(ent);
			CHECK(e.value == true);
		}
	}

	// Add one more component and check if the values are still fine after creating a new archetype
	{
		auto ent = wld.copy(arr[0]);
		wld.add<Position>(ent, {5, 6, 7});

		auto r = wld.get<Rotation>(ent);
		CHECK(r.x == 1.f);
		CHECK(r.y == 2.f);
		CHECK(r.z == 3.f);
		CHECK(r.w == 4.f);

		auto s = wld.get<Scale>(ent);
		CHECK(s.x == 11.f);
		CHECK(s.y == 22.f);
		CHECK(s.z == 33.f);

		auto e = wld.get<Else>(ent);
		CHECK(e.value == true);
	}
}

TEST_CASE("Set - generic & unique") {
	TestWorld twld;

	constexpr uint32_t N = 100;
	cnt::darr<ecs::Entity> arr;
	arr.reserve(N);

	GAIA_FOR(N) {
		arr.push_back(wld.add());
		wld.add<Rotation>(arr.back(), {});
		wld.add<Scale>(arr.back(), {});
		wld.add<Else>(arr.back(), {});
		wld.add<ecs::uni<Position>>(arr.back(), {});
	}

	// Default values
	for (const auto ent: arr) {
		auto r = wld.get<Rotation>(ent);
		CHECK(r.x == 0.f);
		CHECK(r.y == 0.f);
		CHECK(r.z == 0.f);
		CHECK(r.w == 0.f);

		auto s = wld.get<Scale>(ent);
		CHECK(s.x == 0.f);
		CHECK(s.y == 0.f);
		CHECK(s.z == 0.f);

		auto e = wld.get<Else>(ent);
		CHECK(e.value == false);

		auto p = wld.get<ecs::uni<Position>>(ent);
		CHECK(p.x == 0.f);
		CHECK(p.y == 0.f);
		CHECK(p.z == 0.f);
	}

	// Modify values
	{
		ecs::Query q = wld.query().all<Rotation&>().all<Scale&>().all<Else&>();

		q.each([&](ecs::Iter& it) {
			auto rotationView = it.view_mut<Rotation>();
			auto scaleView = it.view_mut<Scale>();
			auto elseView = it.view_mut<Else>();

			GAIA_EACH(it) {
				rotationView[i] = {1, 2, 3, 4};
				scaleView[i] = {11, 22, 33};
				elseView[i] = {true};
			}
		});

		wld.set<ecs::uni<Position>>(arr[0]) = {111, 222, 333};

		{
			Position p = wld.get<ecs::uni<Position>>(arr[0]);
			CHECK(p.x == 111.f);
			CHECK(p.y == 222.f);
			CHECK(p.z == 333.f);
		}
		{
			for (const auto ent: arr) {
				auto r = wld.get<Rotation>(ent);
				CHECK(r.x == 1.f);
				CHECK(r.y == 2.f);
				CHECK(r.z == 3.f);
				CHECK(r.w == 4.f);

				auto s = wld.get<Scale>(ent);
				CHECK(s.x == 11.f);
				CHECK(s.y == 22.f);
				CHECK(s.z == 33.f);

				auto e = wld.get<Else>(ent);
				CHECK(e.value == true);
			}
		}
		{
			auto p = wld.get<ecs::uni<Position>>(arr[0]);
			CHECK(p.x == 111.f);
			CHECK(p.y == 222.f);
			CHECK(p.z == 333.f);
		}
	}
}

TEST_CASE("Components - non trivial") {
	TestWorld twld;

	constexpr uint32_t N = 100;
	cnt::darr<ecs::Entity> arr;
	arr.reserve(N);

	GAIA_FOR(N) {
		arr.push_back(wld.add());
		wld.add<StringComponent>(arr.back(), {});
		wld.add<StringComponent2>(arr.back(), {});
		wld.add<PositionNonTrivial>(arr.back(), {});
	}

	// Default values
	for (const auto ent: arr) {
		const auto& s1 = wld.get<StringComponent>(ent);
		CHECK(s1.value.empty());

		{
			auto s2 = wld.get<StringComponent2>(ent);
			CHECK(s2.value == StringComponent2DefaultValue);
		}
		{
			const auto& s2 = wld.get<StringComponent2>(ent);
			CHECK(s2.value == StringComponent2DefaultValue);
		}

		const auto& p = wld.get<PositionNonTrivial>(ent);
		CHECK(p.x == 1.f);
		CHECK(p.y == 2.f);
		CHECK(p.z == 3.f);
	}

	// Modify values
	{
		ecs::Query q = wld.query().all<StringComponent&>().all<StringComponent2&>().all<PositionNonTrivial&>();

		q.each([&](ecs::Iter& it) {
			auto strView = it.view_mut<StringComponent>();
			auto str2View = it.view_mut<StringComponent2>();
			auto posView = it.view_mut<PositionNonTrivial>();

			GAIA_EACH(it) {
				strView[i] = {StringComponentDefaultValue};
				str2View[i].value = StringComponent2DefaultValue_2;
				posView[i] = {111, 222, 333};
			}
		});

		q.each([&](ecs::Iter& it) {
			auto strView = it.view_mut<StringComponent>(0);
			auto str2View = it.view_mut<StringComponent2>(1);
			auto posView = it.view_mut<PositionNonTrivial>(2);

			GAIA_EACH(it) {
				strView[i] = {StringComponentDefaultValue};
				str2View[i].value = StringComponent2DefaultValue_2;
				posView[i] = {111, 222, 333};
			}
		});

		for (const auto ent: arr) {
			const auto& s1 = wld.get<StringComponent>(ent);
			CHECK(s1.value == StringComponentDefaultValue);

			const auto& s2 = wld.get<StringComponent2>(ent);
			CHECK(s2.value == StringComponent2DefaultValue_2);

			const auto& p = wld.get<PositionNonTrivial>(ent);
			CHECK(p.x == 111.f);
			CHECK(p.y == 222.f);
			CHECK(p.z == 333.f);
		}
	}

	// Add one more component and check if the values are still fine after creating a new archetype
	{
		auto ent = wld.copy(arr[0]);
		wld.add<Position>(ent, {5, 6, 7});

		const auto& s1 = wld.get<StringComponent>(ent);
		CHECK(s1.value == StringComponentDefaultValue);

		const auto& s2 = wld.get<StringComponent2>(ent);
		CHECK(s2.value == StringComponent2DefaultValue_2);

		const auto& p = wld.get<PositionNonTrivial>(ent);
		CHECK(p.x == 111.f);
		CHECK(p.y == 222.f);
		CHECK(p.z == 333.f);
	}
}

#if GAIA_ECS_CHUNK_ALLOCATOR
TEST_CASE("ChunkAllocator") {
	// We do this mostly for code coverage
	{
		TestWorld twld;
		ecs::CommandBufferST cb(wld);
		auto mainEntity = wld.add();

		wld.add<Position>(mainEntity, {1, 2, 3});

		constexpr uint32_t M = 100000;
		(void)wld.copy_n(mainEntity, M);

		// delete all created entities
		auto q = wld.query().all<Position>();
		CHECK(q.count() == M + 1);
		q.each([&](ecs::Entity e) {
			cb.del(e);
		});

		cb.commit();
		wld.update();
		ecs::ChunkAllocator::get().flush();
	}

	// We do this just for code coverage.
	// Hide logging so it does not spam the results of unit testing.
	const auto logLevelBackup = util::g_logLevelMask;
	util::g_logLevelMask = 0;
	ecs::ChunkAllocator::get().diag();
	util::g_logLevelMask = logLevelBackup;
}
#endif

TEST_CASE("CommandBuffer") {
	SUBCASE("Entity creation") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		const uint32_t N = 100;
		GAIA_FOR(N) {
			[[maybe_unused]] auto tmp = cb.add();
		}

		cb.commit();

		CHECK(wld.size() == ecs::GAIA_ID(LastCoreComponent).id() + 1 + N);
	}

	SUBCASE("Entity creation from a query") {
		TestWorld twld;
		auto mainEntity = wld.add();
		wld.add<Position>(mainEntity, {1, 2, 3});

		uint32_t cnt = 0;
		auto q = wld.query().all<Position>();
		q.each([&](ecs::Iter& it) {
			auto& cb = it.cmd_buffer_st();
			auto e = cb.add();
			cb.add<Position>(e, {4, 5, 6});
			++cnt;
		});
		CHECK(cnt == 1);

		cnt = 0;
		q.each([&](ecs::Iter& it) {
			auto ev = it.view<ecs::Entity>();
			auto pv = it.view<Position>();
			GAIA_EACH(it) {
				const auto& p = pv[i];
				if (ev[i] == mainEntity) {
					CHECK(p.x == 1.f);
					CHECK(p.y == 2.f);
					CHECK(p.z == 3.f);
				} else {
					CHECK(p.x == 4.f);
					CHECK(p.y == 5.f);
					CHECK(p.z == 6.f);
				}
				++cnt;
			}
		});
		CHECK(cnt == 2);
	}

	SUBCASE("Entity creation from another entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		auto mainEntity = wld.add();

		const uint32_t N = 100;
		GAIA_FOR(N) {
			[[maybe_unused]] auto tmp = cb.copy(mainEntity);
		}

		cb.commit();

		CHECK(wld.size() == ecs::GAIA_ID(LastCoreComponent).id() + 1 + 1 + N); // core + mainEntity + N others
	}

	SUBCASE("Entity creation from a to-be-created entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		auto mainEntity = cb.add();

		const uint32_t N = 100;
		GAIA_FOR(N) {
			[[maybe_unused]] auto tmp = cb.copy(mainEntity);
		}

		cb.commit();

		CHECK(wld.size() == ecs::GAIA_ID(LastCoreComponent).id() + 1 + 1 + N); // core + mainEntity + N others
	}

	SUBCASE("Entity creation from another entity with a component") {
		{
			TestWorld twld;
			ecs::CommandBufferST cb(wld);
			auto mainEntity = wld.add();

			wld.add<Position>(mainEntity, {1, 2, 3});

			auto q = wld.query().all<Position>();
			CHECK(q.count() == 1);
			uint32_t i = 0;
			q.each([&](const Position& p) {
				CHECK(p.x == 1.f);
				CHECK(p.y == 2.f);
				CHECK(p.z == 3.f);
				++i;
			});
			CHECK(i == 1);

			(void)wld.copy(mainEntity);
			CHECK(q.count() == 2);
			i = 0;
			q.each([&](const Position& p) {
				CHECK(p.x == 1.f);
				CHECK(p.y == 2.f);
				CHECK(p.z == 3.f);
				++i;
			});
			CHECK(i == 2);
		}

		{
			TestWorld twld;
			ecs::CommandBufferST cb(wld);
			auto mainEntity = wld.add();

			wld.add<Position>(mainEntity, {1, 2, 3});

			[[maybe_unused]] auto tmp = cb.copy(mainEntity);
			cb.commit();

			auto q = wld.query().all<Position>();
			CHECK(q.count() == 2);
			uint32_t i = 0;
			q.each([&](const Position& p) {
				CHECK(p.x == 1.f);
				CHECK(p.y == 2.f);
				CHECK(p.z == 3.f);
				++i;
			});
			CHECK(i == 2);
		}
	}

	SUBCASE("Entity creation from another entity with a SoA component") {
		{
			TestWorld twld;
			ecs::CommandBufferST cb(wld);
			auto mainEntity = wld.add();

			wld.add<PositionSoA>(mainEntity, {1, 2, 3});

			auto q = wld.query().all<PositionSoA>();
			CHECK(q.count() == 1);
			uint32_t i = 0;
			q.each([&](const PositionSoA& p) {
				CHECK(p.x == 1.f);
				CHECK(p.y == 2.f);
				CHECK(p.z == 3.f);
				++i;
			});
			CHECK(i == 1);

			(void)wld.copy(mainEntity);
			CHECK(q.count() == 2);
			i = 0;
			q.each([&](const PositionSoA& p) {
				CHECK(p.x == 1.f);
				CHECK(p.y == 2.f);
				CHECK(p.z == 3.f);
				++i;
			});
			CHECK(i == 2);
		}

		{
			TestWorld twld;
			ecs::CommandBufferST cb(wld);
			auto mainEntity = wld.add();

			wld.add<PositionSoA>(mainEntity, {1, 2, 3});

			(void)cb.copy(mainEntity);
			cb.commit();

			auto q = wld.query().all<PositionSoA>();
			CHECK(q.count() == 2);
			uint32_t i = 0;
			q.each([&](const PositionSoA& p) {
				CHECK(p.x == 1.f);
				CHECK(p.y == 2.f);
				CHECK(p.z == 3.f);
				++i;
			});
			CHECK(i == 2);
		}
	}

	SUBCASE("Delayed component addition to an existing entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		auto e = wld.add();
		cb.add<Position>(e);
		CHECK_FALSE(wld.has<Position>(e));
		cb.commit();
		CHECK(wld.has<Position>(e));
	}

	SUBCASE("Delayed component addition (via entity) to an existing entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		auto p = wld.add<Position>().entity;

		auto e = wld.add();
		cb.add(e, p);
		CHECK_FALSE(wld.has(e, p));
		cb.commit();
		CHECK(wld.has(e, p));
	}

	SUBCASE("Delayed entity addition to an existing entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		auto e = wld.add(); // core + 1
		const auto s = wld.size();
		auto tmp = cb.add();
		CHECK(wld.size() == s); // no new added entity
		cb.add(e, tmp);
		cb.commit();
		CHECK(wld.size() == s + 1); // new entity added

		auto e2 = wld.get(s); // core + e + new entity
		CHECK(wld.has(e, e2));
	}

	SUBCASE("Delayed component addition to a to-be-created entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		(void)wld.add<Position>();

		const auto s = wld.size();

		auto tmp = cb.add(); // no new entity created yet
		CHECK(wld.size() == s);
		cb.add<Position>(tmp); // component entity created
		CHECK(wld.size() == s);
		cb.commit();
		CHECK(wld.size() == s + 1);

		auto e = wld.get(s); // + new entity
		CHECK(wld.has<Position>(e));
	}

	SUBCASE("Delayed component addition (via entity) to a to-be-created entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		(void)wld.add<Position>();

		const auto s = wld.size();
		auto p = wld.add<Position>().entity;
		CHECK(wld.size() == s);

		auto tmp = cb.add(); // no new entity created yet
		CHECK(wld.size() == s);
		cb.add(tmp, p);
		CHECK(wld.size() == s);
		cb.commit();
		CHECK(wld.size() == s + 1);

		auto e = wld.get(s); // + new entity
		CHECK(wld.has(e, p));
	}

	SUBCASE("Delayed entity addition to a to-be-created entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		const auto s = wld.size();
		auto tmpA = cb.add();
		auto tmpB = cb.add(); // core + 0 (no new entity created yet)
		CHECK(wld.size() == s);
		cb.add(tmpA, tmpB);
		CHECK(wld.size() == s);
		cb.commit();
		CHECK(wld.size() == s + 2);

		auto e1 = wld.get(s);
		auto e2 = wld.get(s + 1);
		CHECK(wld.has(e1, e2));
	}

	SUBCASE("Delayed component setting of an existing entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		auto e = wld.add();

		cb.add<Position>(e);
		cb.set<Position>(e, {1, 2, 3});
		CHECK_FALSE(wld.has<Position>(e));

		cb.commit();
		CHECK(wld.has<Position>(e));

		auto p = wld.get<Position>(e);
		CHECK(p.x == 1);
		CHECK(p.y == 2);
		CHECK(p.z == 3);
	}

	SUBCASE("Delayed 2 components setting of an existing entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		auto e = wld.add();

		cb.add<Position>(e);
		cb.set<Position>(e, {1, 2, 3});
		cb.add<Acceleration>(e);
		cb.set<Acceleration>(e, {4, 5, 6});
		CHECK_FALSE(wld.has<Position>(e));
		CHECK_FALSE(wld.has<Acceleration>(e));

		cb.commit();
		CHECK(wld.has<Position>(e));
		CHECK(wld.has<Acceleration>(e));

		auto p = wld.get<Position>(e);
		CHECK(p.x == 1.f);
		CHECK(p.y == 2.f);
		CHECK(p.z == 3.f);

		auto a = wld.get<Acceleration>(e);
		CHECK(a.x == 4.f);
		CHECK(a.y == 5.f);
		CHECK(a.z == 6.f);
	}

	SUBCASE("Delayed component setting of a to-be-created entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		(void)wld.add<Position>();

		const auto s = wld.size();
		auto tmp = cb.add();
		CHECK(wld.size() == s);

		cb.add<Position>(tmp);
		CHECK(wld.size() == s);
		cb.set<Position>(tmp, {1, 2, 3});
		cb.commit();
		CHECK(wld.size() == s + 1); // + new entity

		auto e = wld.get(s);
		CHECK(wld.has<Position>(e));

		auto p = wld.get<Position>(e);
		CHECK(p.x == 1.f);
		CHECK(p.y == 2.f);
		CHECK(p.z == 3.f);
	}

	SUBCASE("Delayed component add+delete of a temporary entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		(void)wld.add<Position>();

		const auto s = wld.size();
		auto tmp = cb.add();
		CHECK(wld.size() == s);

		cb.add<Position>(tmp);
		CHECK(wld.size() == s);
		cb.del<Position>(tmp);
		cb.commit();
		CHECK(wld.size() == s + 1); // + new entity

		auto e = wld.get(s);
		CHECK_FALSE(wld.has<Position>(e));
	}

	SUBCASE("Delayed component add+set+set") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		(void)wld.add<Position>();

		const auto s = wld.size();
		auto tmp = cb.add();
		CHECK(wld.size() == s);

		cb.add<Position>(tmp);
		cb.set<Position>(tmp, {1, 2, 3});
		CHECK(wld.size() == s);
		cb.set<Position>(tmp, {4, 5, 6});
		cb.commit();
		CHECK(wld.size() == s + 1); // + new entity

		auto e = wld.get(s);
		CHECK(wld.has<Position>(e));

		auto p = wld.get<Position>(e);
		CHECK(p.x == 4.f);
		CHECK(p.y == 5.f);
		CHECK(p.z == 6.f);
	}

	SUBCASE("Delayed component add+set+set+del") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		(void)wld.add<Position>();

		const auto s = wld.size();
		auto tmp = cb.add();
		CHECK(wld.size() == s);

		cb.add<Position>(tmp);
		cb.set<Position>(tmp, {1, 2, 3});
		CHECK(wld.size() == s);
		cb.set<Position>(tmp, {4, 5, 6});
		cb.del(tmp);
		cb.commit();
		CHECK(wld.size() == s);
	}

	SUBCASE("Delayed 2 components setting of a to-be-created entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		(void)wld.add<Position>();
		(void)wld.add<Acceleration>();

		const auto s = wld.size();
		auto tmp = cb.add();
		CHECK(wld.size() == s);

		cb.add<Position>(tmp);
		cb.add<Acceleration>(tmp);
		cb.set<Position>(tmp, {1, 2, 3});
		cb.set<Acceleration>(tmp, {4, 5, 6});
		cb.commit();
		CHECK(wld.size() == s + 1); // + new entity

		auto e = wld.get(s);
		CHECK(wld.has<Position>(e));
		CHECK(wld.has<Acceleration>(e));

		auto p = wld.get<Position>(e);
		CHECK(p.x == 1.f);
		CHECK(p.y == 2.f);
		CHECK(p.z == 3.f);

		auto a = wld.get<Acceleration>(e);
		CHECK(a.x == 4.f);
		CHECK(a.y == 5.f);
		CHECK(a.z == 6.f);
	}

	SUBCASE("Delayed component add with setting of a to-be-created entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		(void)wld.add<Position>();

		const auto s = wld.size();
		auto tmp = cb.add();
		CHECK(wld.size() == s);

		cb.add<Position>(tmp, {1, 2, 3});
		cb.commit();
		CHECK(wld.size() == s + 1); // + new entity

		auto e = wld.get(s);
		CHECK(wld.has<Position>(e));

		auto p = wld.get<Position>(e);
		CHECK(p.x == 1.f);
		CHECK(p.y == 2.f);
		CHECK(p.z == 3.f);
	}

	SUBCASE("Delayed 2 components add with setting of a to-be-created entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		(void)wld.add<Position>();
		(void)wld.add<Acceleration>();

		const auto s = wld.size();
		auto tmp = cb.add();
		CHECK(wld.size() == s);

		cb.add<Position>(tmp, {1, 2, 3});
		cb.add<Acceleration>(tmp, {4, 5, 6});
		cb.commit();
		CHECK(wld.size() == s + 1); // + new entity

		auto e = wld.get(s);
		CHECK(wld.has<Position>(e));
		CHECK(wld.has<Acceleration>(e));

		auto p = wld.get<Position>(e);
		CHECK(p.x == 1.f);
		CHECK(p.y == 2.f);
		CHECK(p.z == 3.f);

		auto a = wld.get<Acceleration>(e);
		CHECK(a.x == 4.f);
		CHECK(a.y == 5.f);
		CHECK(a.z == 6.f);
	}

	SUBCASE("Delayed component removal from an existing entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		auto e = wld.add();
		wld.add<Position>(e, {1, 2, 3});

		cb.del<Position>(e);
		CHECK(wld.has<Position>(e));
		{
			auto p = wld.get<Position>(e);
			CHECK(p.x == 1.f);
			CHECK(p.y == 2.f);
			CHECK(p.z == 3.f);
		}

		cb.commit();
		CHECK_FALSE(wld.has<Position>(e));
	}

	SUBCASE("Delayed 2 component removal from an existing entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		auto e = wld.add();
		wld.add<Position>(e, {1, 2, 3});
		wld.add<Acceleration>(e, {4, 5, 6});

		cb.del<Position>(e);
		cb.del<Acceleration>(e);
		CHECK(wld.has<Position>(e));
		CHECK(wld.has<Acceleration>(e));
		{
			auto p = wld.get<Position>(e);
			CHECK(p.x == 1.f);
			CHECK(p.y == 2.f);
			CHECK(p.z == 3.f);

			auto a = wld.get<Acceleration>(e);
			CHECK(a.x == 4.f);
			CHECK(a.y == 5.f);
			CHECK(a.z == 6.f);
		}

		cb.commit();
		CHECK_FALSE(wld.has<Position>(e));
		CHECK_FALSE(wld.has<Acceleration>(e));
	}

	SUBCASE("Delayed non-trivial component setting of an existing entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		auto e = wld.add();

		cb.add<StringComponent>(e);
		cb.set<StringComponent>(e, {StringComponentDefaultValue});
		cb.add<StringComponent2>(e);
		CHECK_FALSE(wld.has<StringComponent>(e));
		CHECK_FALSE(wld.has<StringComponent2>(e));

		cb.commit();
		CHECK(wld.has<StringComponent>(e));
		CHECK(wld.has<StringComponent2>(e));

		auto s1 = wld.get<StringComponent>(e);
		CHECK(s1.value == StringComponentDefaultValue);
		auto s2 = wld.get<StringComponent2>(e);
		CHECK(s2.value == StringComponent2DefaultValue);
	}

	SUBCASE("Delayed entity deletion") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		auto e = wld.add();

		cb.del(e);
		CHECK(wld.has(e));

		cb.commit();
		CHECK_FALSE(wld.has(e));
	}
}

TEST_CASE("Query Filter - no systems") {
	TestWorld twld;
	ecs::Query q = wld.query().all<Position>().changed<Position>();

	auto e = wld.add();
	wld.add<Position>(e);

	// System-less filters
	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		CHECK(cnt == 1); // first run always happens
	}
	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		CHECK(cnt == 0); // no change of position so this shouldn't run
	}
	{
		wld.set<Position>(e) = {};
	}
	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		CHECK(cnt == 1);
	}
	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		CHECK(cnt == 0);
	}
	{
		wld.sset<Position>(e) = {};
	}
	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		CHECK(cnt == 0);
	}
	{
		auto* ch = wld.get_chunk(e);
		auto p = ch->sview_mut<Position>();
		p[0] = {};
	}
	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		CHECK(cnt == 0);
	}
	auto e2 = wld.copy(e);
	(void)e2;
	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		CHECK(cnt == 2); // adding an entity triggers the change
	}
	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		CHECK(cnt == 0); // no new change since the last time
	}
}

TEST_CASE("Query Filter - systems") {
	TestWorld twld;

	uint32_t expectedCnt = 0;
	uint32_t actualCnt = 0;
	uint32_t wsCnt = 0;
	uint32_t wssCnt = 0;

	auto e = wld.add();
	wld.add<Position>(e);

	// WriterSystem
	auto ws = wld.system()
								.all<Position&>()
								.on_each([&](Position& a) {
									++wsCnt;
									(void)a;
								})
								.entity();
	// WriterSystemSilent
	auto wss = wld.system()
								 .all<Position&>()
								 .on_each([&](ecs::Iter& it) {
									 ++wssCnt;
									 auto posRWView = it.sview_mut<Position>();
									 (void)posRWView;
								 })
								 .entity();
	// ReaderSystem
	auto rs = wld.system()
								.all<Position>()
								.changed<Position>()
								.on_each([&](ecs::Iter& it) {
									GAIA_EACH(it)++ actualCnt;
								})
								.entity();
	(void)rs;

	// first run always happens
	{
		wld.enable(ws, false);
		wld.enable(wss, false);
		CHECK_FALSE(wld.enabled(ws));
		CHECK_FALSE(wld.enabled(wss));
		CHECK(wld.enabled(rs));

		expectedCnt = 1;
		actualCnt = 0;
		wsCnt = 0;
		wssCnt = 0;
		wld.update();

		CHECK_FALSE(wld.enabled(ws));
		CHECK_FALSE(wld.enabled(wss));
		CHECK(wld.enabled(rs));

		CHECK(actualCnt == expectedCnt);
		CHECK(wsCnt == 0);
		CHECK(wssCnt == 0);
	}
	// no change of position so ReaderSystem should't see any changes
	{
		expectedCnt = 0;
		actualCnt = 0;
		wsCnt = 0;
		wssCnt = 0;
		wld.update();

		CHECK_FALSE(wld.enabled(ws));
		CHECK_FALSE(wld.enabled(wss));
		CHECK(wld.enabled(rs));

		CHECK(actualCnt == expectedCnt);
		CHECK(wsCnt == 0);
		CHECK(wssCnt == 0);
	}
	// update position so ReaderSystem should detect a change
	{
		wld.enable(ws, true);
		CHECK(wld.enabled(ws));
		CHECK_FALSE(wld.enabled(wss));
		CHECK(wld.enabled(rs));

		expectedCnt = 1;
		actualCnt = 0;
		wsCnt = 0;
		wssCnt = 0;
		wld.update();

		CHECK(wld.enabled(ws));
		CHECK_FALSE(wld.enabled(wss));
		CHECK(wld.enabled(rs));

		CHECK(actualCnt == expectedCnt);
		CHECK(wsCnt > 0);
		CHECK(wssCnt == 0);
	}
	// no change of position so ReaderSystem shouldn't see any changes
	{
		wld.enable(ws, false);
		CHECK_FALSE(wld.enabled(ws));
		CHECK_FALSE(wld.enabled(wss));
		CHECK(wld.enabled(rs));

		expectedCnt = 0;
		actualCnt = 0;
		wsCnt = 0;
		wssCnt = 0;
		wld.update();

		CHECK_FALSE(wld.enabled(ws));
		CHECK_FALSE(wld.enabled(wss));
		CHECK(wld.enabled(rs));

		CHECK(actualCnt == expectedCnt);
		CHECK(wsCnt == 0);
		CHECK(wssCnt == 0);
	}
	// silent writer enabled again. If should not trigger an update
	{
		wld.enable(ws, false);
		wld.enable(wss, true);
		CHECK_FALSE(wld.enabled(ws));
		CHECK(wld.enabled(wss));
		CHECK(wld.enabled(rs));

		expectedCnt = 0;
		actualCnt = 0;
		wsCnt = 0;
		wssCnt = 0;
		wld.update();

		CHECK_FALSE(wld.enabled(ws));
		CHECK(wld.enabled(wss));
		CHECK(wld.enabled(rs));

		CHECK(actualCnt == expectedCnt);
		CHECK(wsCnt == 0);
		CHECK(wssCnt > 0);
	}
}

struct Eats {};
struct Healthy {};
ecs::GroupId
group_by_relation([[maybe_unused]] const ecs::World& world, const ecs::Archetype& archetype, ecs::Entity groupBy) {
	if (archetype.pairs() > 0) {
		auto ids = archetype.ids_view();
		for (auto id: ids) {
			if (!id.pair() || id.id() != groupBy.id())
				continue;

			// Consider the pair's target the groupId
			return id.gen();
		}
	}

	// No group
	return 0;
}

TEST_CASE("Query - group") {
	TestWorld twld;

	ecs::Entity eats = wld.add(); // 16
	ecs::Entity carrot = wld.add(); // 17
	ecs::Entity salad = wld.add(); // 18
	ecs::Entity apple = wld.add(); // 19

	ecs::Entity ents[6];
	GAIA_FOR(6) ents[i] = wld.add(); // 20, 21, 22, 23, 24, 25
	{
		// 26 - Position
		// 27 - Healthy
		wld.build(ents[0]).add<Position>().add({eats, salad}); // 20 <-- Pos, {Eats,Salad}
		wld.build(ents[1]).add<Position>().add({eats, carrot});
		wld.build(ents[2]).add<Position>().add({eats, apple});

		wld.build(ents[3]).add<Position>().add({eats, apple}).add<Healthy>();
		wld.build(ents[4]).add<Position>().add({eats, salad}).add<Healthy>();
		wld.build(ents[5]).add<Position>().add({eats, carrot}).add<Healthy>();
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
	ecs::Entity ents_expected[] = {ents[1], ents[5], // carrot, 21, 25
																 ents[0], ents[4], // salad, 20, 24
																 ents[2], ents[3]}; // apple, 22, 23

	auto checkQuery = [&](ecs::Query& q, //
												std::span<ecs::Entity> ents_expected_view) {
		{
			uint32_t j = 0;
			q.each([&](ecs::Iter& it) {
				auto ents = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					const auto e = ents[i];
					const auto e_wanted = ents_expected_view[j++];
					CHECK(e == e_wanted);
				}
			});
			CHECK(j == (uint32_t)ents_expected_view.size());
		}
		{
			uint32_t j = 0;
			q.each([&](ecs::Entity e) {
				const auto e_wanted = ents_expected_view[j++];
				CHECK(e == e_wanted);
			});
			CHECK(j == (uint32_t)ents_expected_view.size());
		}
	};

	{
		auto qq = wld.query().all<Position>().group_by(eats);

		// Grouping on, no group enforced
		checkQuery(qq, {&ents_expected[0], 6});
		// Grouping on, a group is enforced
		qq.group_id(carrot);
		checkQuery(qq, {&ents_expected[0], 2});
		qq.group_id(salad);
		checkQuery(qq, {&ents_expected[2], 2});
		qq.group_id(apple);
		checkQuery(qq, {&ents_expected[4], 2});
	}

	{
		auto qq = wld.query().all<Position>().group_by(eats, group_by_relation);

		// Grouping on, no group enforced
		checkQuery(qq, {&ents_expected[0], 6});
		// Grouping on, a group is enforced
		qq.group_id(carrot);
		checkQuery(qq, {&ents_expected[0], 2});
		qq.group_id(salad);
		checkQuery(qq, {&ents_expected[2], 2});
		qq.group_id(apple);
		checkQuery(qq, {&ents_expected[4], 2});
	}
}

TEST_CASE("Query - sort") {
	TestWorld twld;

	ecs::Entity e0 = wld.add();
	ecs::Entity e1 = wld.add();
	ecs::Entity e2 = wld.add();
	ecs::Entity e3 = wld.add();

	wld.add<Position>(e0, {2, 0, 0});
	wld.add<Position>(e1, {4, 0, 0});
	wld.add<Position>(e2, {1, 0, 0});
	wld.add<Position>(e3, {3, 0, 0});

	SUBCASE("By entity index") {
		auto q = wld.query().all<Position>().sort_by(
				ecs::EntityBad, []([[maybe_unused]] const ecs::World& world, const void* pData0, const void* pData1) {
					const auto& e0 = *static_cast<const ecs::Entity*>(pData0);
					const auto& e1 = *static_cast<const ecs::Entity*>(pData1);
					return (int)e0.id() - (int)e1.id();
				});
		q.each([&](ecs::Iter& it) {
			auto ents = it.view<ecs::Entity>();
			CHECK(ents[0] == e0);
			CHECK(ents[1] == e1);
			CHECK(ents[2] == e2);
			CHECK(ents[3] == e3);
		});
	}

	SUBCASE("By component value (1)") {
		auto q = wld.query().all<Position>().sort_by<Position>(
				[]([[maybe_unused]] const ecs::World& world, const void* pData0, const void* pData1) {
					const auto& p0 = *static_cast<const Position*>(pData0);
					const auto& p1 = *static_cast<const Position*>(pData1);
					const float diff = p0.x - p1.x;
					if (diff < 0.f)
						return -1;
					if (diff > 0.f)
						return 1;
					return 0;
				});
		q.each([&](ecs::Iter& it) {
			auto ents = it.view<ecs::Entity>();
			CHECK(ents[0] == e2);
			CHECK(ents[1] == e0);
			CHECK(ents[2] == e3);
			CHECK(ents[3] == e1);
		});
	}

	SUBCASE("By component value (2)") {
		auto q = wld.query().all<Position>().sort_by(
				wld.get<Position>(), //
				[]([[maybe_unused]] const ecs::World& world, const void* pData0, const void* pData1) {
					const auto& p0 = *static_cast<const Position*>(pData0);
					const auto& p1 = *static_cast<const Position*>(pData1);
					const float diff = p0.x - p1.x;
					if (diff < 0.f)
						return -1;
					if (diff > 0.f)
						return 1;
					return 0;
				});
		q.each([&](ecs::Iter& it) {
			auto ents = it.view<ecs::Entity>();
			CHECK(ents[0] == e2);
			CHECK(ents[1] == e0);
			CHECK(ents[2] == e3);
			CHECK(ents[3] == e1);
		});

		cnt::darr<ecs::Entity> tmp;

		// Change some archetype
		{
			wld.add<Something>(e0, {false});
			q.each([&tmp](ecs::Iter& it) {
				auto ents = it.view<ecs::Entity>();
				GAIA_EACH(ents) tmp.push_back(ents[i]);
			});

			CHECK(tmp[0] == e2);
			CHECK(tmp[1] == e0);
			CHECK(tmp[2] == e3);
			CHECK(tmp[3] == e1);
		}

		// Add new entity
		auto e4 = wld.add();
		{
			wld.add<Position>(e4, {0, 0, 0});
			tmp.clear();
			q.each([&tmp](ecs::Iter& it) {
				auto ents = it.view<ecs::Entity>();
				GAIA_EACH(ents) tmp.push_back(ents[i]);
			});

			CHECK(tmp[0] == e4);
			CHECK(tmp[1] == e2);
			CHECK(tmp[2] == e0);
			CHECK(tmp[3] == e3);
			CHECK(tmp[4] == e1);
		}

		// Delete entity
		{
			wld.del(e0);
			tmp.clear();
			q.each([&tmp](ecs::Iter& it) {
				auto ents = it.view<ecs::Entity>();
				GAIA_EACH(ents) tmp.push_back(ents[i]);
			});
			CHECK(tmp[0] == e4);
			CHECK(tmp[1] == e2);
			CHECK(tmp[2] == e3);
			CHECK(tmp[3] == e1);
		}
	}
}

#if GAIA_SYSTEMS_ENABLED

TEST_CASE("System - simple") {
	TestWorld twld;

	constexpr uint32_t N = 10;
	{
		auto e = wld.add();
		wld.add<Position>(e, {0, 100, 0});
		wld.add<Acceleration>(e, {1, 0, 0});
		GAIA_FOR(N - 1) {
			[[maybe_unused]] auto newEntity = wld.copy(e);
		}
	}

	uint32_t sys1_cnt = 0;
	uint32_t sys2_cnt = 0;
	uint32_t sys3_cnt = 0;
	bool sys3_run_before_sys1 = false;
	bool sys3_run_before_sys2 = false;

	auto testRun = [&]() {
		GAIA_FOR(100) {
			sys3_run_before_sys1 = false;
			sys3_run_before_sys2 = false;
			wld.update();
			CHECK(sys1_cnt == N);
			CHECK(sys2_cnt == N);
			CHECK(sys3_cnt == N);
			sys1_cnt = 0;
			sys2_cnt = 0;
			sys3_cnt = 0;
		}
	};

	// Our systems
	auto sys1 = wld.system()
									.all<Position>()
									.all<Acceleration>() //
									.on_each([&](Position, Acceleration) {
										if (sys1_cnt == 0 && sys3_cnt > 0)
											sys3_run_before_sys1 = true;
										++sys1_cnt;
									});
	auto sys2 = wld.system()
									.all<Position>() //
									.on_each([&](ecs::Iter& it) {
										if (sys2_cnt == 0 && sys3_cnt > 0)
											sys3_run_before_sys2 = true;
										GAIA_EACH(it)++ sys2_cnt;
									});
	auto sys3 = wld.system()
									.all<Acceleration>() //
									.on_each([&](ecs::Iter& it) {
										GAIA_EACH(it)++ sys3_cnt;
									});

	testRun();

	// Make sure to execute sys2 before sys1
	wld.add(sys1.entity(), {ecs::DependsOn, sys3.entity()});
	wld.add(sys2.entity(), {ecs::DependsOn, sys3.entity()});

	testRun();

	CHECK(sys3_run_before_sys1);
	CHECK(sys3_run_before_sys2);
}

#endif

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

		// Make sure disabling works
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

	// TODO: Finish this part
	// SUBCASE("each") {
	// 	ecs::Query q = wld.query().all<T>();

	// 	{
	// 		const auto cnt = q.count();
	// 		CHECK(cnt == N);

	// 		uint32_t j = 0;
	// 		// TODO: Add SoA support for q.each([](T& t){})
	// 		q.each([&](const T& t) {
	// 			auto f = (float)j;
	// 			CHECK(t.x == f);
	// 			CHECK(t.y == f);
	// 			CHECK(t.z == f);

	// 			++j;
	// 		});
	// 		CHECK(j == cnt);
	// 	}

	// 	// Make sure disabling works
	// 	{
	// 		auto e = ents[0];
	// 		wld.enable(e, false);
	// 		const auto cnt = q.count();
	// 		CHECK(cnt == N - 1);
	// 		uint32_t cntCalculated = 0;
	// 		q.each([&]([[maybe_unused]] const T& t) {
	// 			++cntCalculated;
	// 		});
	// 		CHECK(cnt == cntCalculated);
	// 	}
	// }
}

TEST_CASE("DataLayout SoA - ECS") {
	TestDataLayoutSoA_ECS<PositionSoA>();
	TestDataLayoutSoA_ECS<RotationSoA>();
}

TEST_CASE("DataLayout SoA8 - ECS") {
	TestDataLayoutSoA_ECS<PositionSoA8>();
	TestDataLayoutSoA_ECS<RotationSoA8>();
}

TEST_CASE("DataLayout SoA16 - ECS") {
	TestDataLayoutSoA_ECS<PositionSoA16>();
	TestDataLayoutSoA_ECS<RotationSoA16>();
}

//------------------------------------------------------------------------------
// Component cache
//------------------------------------------------------------------------------

template <typename T>
void comp_cache_verify(ecs::World& w, const ecs::ComponentCacheItem& other) {
	const auto& d = w.add<const Position>();
	CHECK(other.entity == d.entity);
	CHECK(other.comp == d.comp);
}

TEST_CASE("Component cache") {
	{
		TestWorld twld;
		const auto& item = wld.add<Position>();
		auto ent = item.entity;
		CHECK_FALSE(ent.entity());

		comp_cache_verify<const Position>(wld, item);
		comp_cache_verify<Position&>(wld, item);
		comp_cache_verify<const Position&>(wld, item);
		comp_cache_verify<Position*>(wld, item);
		comp_cache_verify<const Position*>(wld, item);
	}
	{
		TestWorld twld;
		const auto& item = wld.add<const Position>();
		auto ent = item.entity;
		CHECK_FALSE(ent.entity());

		comp_cache_verify<Position>(wld, item);
		comp_cache_verify<Position&>(wld, item);
		comp_cache_verify<const Position&>(wld, item);
		comp_cache_verify<Position*>(wld, item);
		comp_cache_verify<const Position*>(wld, item);
	}
	{
		TestWorld twld;
		const auto& item = wld.add<Position&>();
		auto ent = item.entity;
		CHECK_FALSE(ent.entity());

		comp_cache_verify<Position>(wld, item);
		comp_cache_verify<const Position>(wld, item);
		comp_cache_verify<const Position&>(wld, item);
		comp_cache_verify<Position*>(wld, item);
		comp_cache_verify<const Position*>(wld, item);
	}
	{
		TestWorld twld;
		const auto& item = wld.add<const Position&>();
		auto ent = item.entity;
		CHECK_FALSE(ent.entity());

		comp_cache_verify<Position>(wld, item);
		comp_cache_verify<const Position>(wld, item);
		comp_cache_verify<Position&>(wld, item);
		comp_cache_verify<Position*>(wld, item);
		comp_cache_verify<const Position*>(wld, item);
	}
	{
		TestWorld twld;
		const auto& item = wld.add<Position*>();
		auto ent = item.entity;
		CHECK_FALSE(ent.entity());

		comp_cache_verify<Position>(wld, item);
		comp_cache_verify<const Position>(wld, item);
		comp_cache_verify<Position&>(wld, item);
		comp_cache_verify<const Position&>(wld, item);
		comp_cache_verify<const Position*>(wld, item);
	}
	{
		TestWorld twld;
		const auto& item = wld.add<const Position*>();
		auto ent = item.entity;
		CHECK_FALSE(ent.entity());

		comp_cache_verify<Position>(wld, item);
		comp_cache_verify<const Position>(wld, item);
		comp_cache_verify<Position&>(wld, item);
		comp_cache_verify<const Position&>(wld, item);
		comp_cache_verify<Position*>(wld, item);
	}
}

//------------------------------------------------------------------------------
// Hooks
//------------------------------------------------------------------------------

#if GAIA_ENABLE_HOOKS

static thread_local int hook_trigger_cnt = 0;

TEST_CASE("Hooks") {
	#if GAIA_ENABLE_ADD_DEL_HOOKS
	SUBCASE("add") {
		TestWorld twld;
		const auto& pitem = wld.add<Position>();
		hook_trigger_cnt = 0;

		auto e = wld.add();
		wld.add<Position>(e);
		CHECK(hook_trigger_cnt == 0);
		wld.add<Rotation>(e);
		CHECK(hook_trigger_cnt == 0);

		// Set up a hook for adding a Position component
		// ecs::ComponentCache::hooks(pitem).func_add = position_hook;
		ecs::ComponentCache::hooks(pitem).func_add = //
				[](const ecs::World& w, const ecs::ComponentCacheItem& cci, ecs::Entity src) {
					(void)w;
					(void)cci;
					(void)src;
					++hook_trigger_cnt;
				};

		// Components were added already so we don't expect the hook to trigger yet
		wld.add<Position>(e);
		CHECK(hook_trigger_cnt == 0);
		wld.add<Rotation>(e);
		CHECK(hook_trigger_cnt == 0);

		// The component has been removed, and added again. Triggering the hook is expected
		wld.del<Position>(e);
		wld.add<Position>(e);
		CHECK(hook_trigger_cnt == 1);

		// No trigger on Rotation expected because no such trigger has been registered
		wld.del<Rotation>(e);
		wld.add<Rotation>(e);
		CHECK(hook_trigger_cnt == 1);

		// Trigger for new additions
		wld.add<Position>(wld.add());
		wld.add<Position>(wld.add());
		CHECK(hook_trigger_cnt == 3);
	}

	SUBCASE("del") {
		TestWorld twld;
		const auto& pitem = wld.add<Position>();
		hook_trigger_cnt = 0;

		auto e = wld.add();

		// Set up a hook for deleting a Position component
		ecs::ComponentCache::hooks(pitem).func_del = //
				[](const ecs::World& w, const ecs::ComponentCacheItem& cci, ecs::Entity src) {
					(void)w;
					(void)cci;
					(void)src;
					++hook_trigger_cnt;
				};

		#if !GAIA_ASSERT_ENABLED
		// No components were added yet so we don't expect the hook to trigger
		wld.del<Position>(e);
		CHECK(hook_trigger_cnt == 0);
		wld.del<Rotation>(e);
		CHECK(hook_trigger_cnt == 0);
		#endif

		// The component has been added. No triggering yet
		wld.add<Rotation>(e);
		wld.add<Position>(e);
		CHECK(hook_trigger_cnt == 0);

		// Don't trigger for components without a hook
		wld.del<Rotation>(e);
		CHECK(hook_trigger_cnt == 0);

		// Trigger now
		wld.del<Position>(e);
		CHECK(hook_trigger_cnt == 1);

		#if !GAIA_ASSERT_ENABLED
		// Don't trigger again
		wld.del<Position>(e);
		CHECK(hook_trigger_cnt == 1);
		#endif

		// Component added and removed. Trigger again.
		wld.add<Position>(e);
		wld.del<Position>(e);
		CHECK(hook_trigger_cnt == 2);

		#if !GAIA_ASSERT_ENABLED
		// Don't trigger again
		wld.del<Position>(e);
		CHECK(hook_trigger_cnt == 2);
		#endif
	}
	#endif

	#if GAIA_ENABLE_SET_HOOKS
	SUBCASE("set") {
		TestWorld twld;
		const auto& pitem = wld.add<Position>();
		hook_trigger_cnt = 0;

		auto e = wld.add();
		wld.add<Position>(e);
		CHECK(hook_trigger_cnt == 0);
		wld.add<Rotation>(e);
		CHECK(hook_trigger_cnt == 0);

		// Set up a hook for setting a Position component
		ecs::ComponentCache::hooks(pitem).func_set = //
				[](const ecs::World& w, const ecs::ComponentRecord& rec, ecs::Chunk& chunk) {
					(void)w;
					(void)rec;
					(void)chunk;
					++hook_trigger_cnt;
				};

		// Adding shouldn't trigger the set trigger
		{
			hook_trigger_cnt = 0;
			wld.add<Position>(e);
			CHECK(hook_trigger_cnt == 0);
			wld.add<Rotation>(e);
			CHECK(hook_trigger_cnt == 0);

			wld.del<Position>(e);
			wld.add<Position>(e);
			CHECK(hook_trigger_cnt == 0);

			wld.del<Rotation>(e);
			wld.add<Rotation>(e);
			CHECK(hook_trigger_cnt == 0);
		}

		// Don't trigger when setting a different component
		{
			hook_trigger_cnt = 0;
			wld.set<Rotation>(e) = {};
			CHECK(hook_trigger_cnt == 0);
			(void)wld.acc(e).get<Rotation>();
			CHECK(hook_trigger_cnt == 0);
			(void)wld.acc_mut(e).get<Rotation>();
			CHECK(hook_trigger_cnt == 0);
			wld.acc_mut(e).set<Rotation>({});
			CHECK(hook_trigger_cnt == 0);
			wld.acc_mut(e).sset<Rotation>({});
			CHECK(hook_trigger_cnt == 0);
			// Modify + no hooks doesn't trigger
			wld.modify<Rotation, false>(e);
			CHECK(hook_trigger_cnt == 0);
			// Modify + hooks would trigger but we don't have any trigger set for Rotation
			wld.modify<Rotation, true>(e);
			CHECK(hook_trigger_cnt == 0);
		}

		// Trigger for mutable access
		{
			hook_trigger_cnt = 0;
			wld.set<Position>(e) = {};
			CHECK(hook_trigger_cnt == 1);
			(void)wld.acc(e).get<Position>();
			CHECK(hook_trigger_cnt == 1);
			(void)wld.acc_mut(e).get<Position>();
			CHECK(hook_trigger_cnt == 1);
			wld.acc_mut(e).set<Position>({});
			CHECK(hook_trigger_cnt == 2);
			// Silent set doesn't trigger
			wld.acc_mut(e).sset<Position>({});
			CHECK(hook_trigger_cnt == 2);
			// Modify + no hooks doesn't trigger
			wld.modify<Position, false>(e);
			CHECK(hook_trigger_cnt == 2);
			// Modify + hooks does trigger
			wld.modify<Position, true>(e);
			CHECK(hook_trigger_cnt == 3);
		}
	}
	#endif
}

#endif

//------------------------------------------------------------------------------
// Multiple worlds
//------------------------------------------------------------------------------

TEST_CASE("Multiple worlds") {
	ecs::World w1, w2;
	{
		auto e = w1.add();
		w1.add<Position>(e, {1, 1, 1});
		(void)w1.copy(e);
		(void)w1.copy(e);
	}
	{
		auto e = w2.add();
		w2.add<Scale>(e, {2, 0, 0});
		w2.add<Position>(e, {2, 2, 2});
	}

	uint32_t c = 0;
	auto q1 = w1.query().all<Position>();
	q1.each([&c](const Position& p) {
		CHECK(p.x == 1.f);
		CHECK(p.y == 1.f);
		CHECK(p.z == 1.f);
		++c;
	});
	CHECK(c == 3);

	c = 0;
	auto q2 = w2.query().all<Position>();
	q2.each([&c](const Position& p) {
		CHECK(p.x == 2.f);
		CHECK(p.y == 2.f);
		CHECK(p.z == 2.f);
		++c;
	});
	CHECK(c == 1);
}

//------------------------------------------------------------------------------
// Serialization
//------------------------------------------------------------------------------

template <typename T>
bool CompareSerializableType(const T& a, const T& b) {
	if constexpr (
			!std::is_trivially_copyable_v<T> || core::has_func_equals<T>::value || core::has_ffunc_equals<T>::value) {
		return a == b;
	} else {
		return !std::memcmp((const void*)&a, (const void*)&b, sizeof(a));
	}
}

template <typename T>
bool CompareSerializableTypes(const T& a, const T& b) {
	if constexpr (std::is_fundamental<T>::value || core::has_func_equals<T>::value || core::has_ffunc_equals<T>::value) {
		return a == b;
	} else {
		// Convert inputs into tuples where each struct member is an element of the tuple
		auto ta = meta::struct_to_tuple(a);
		auto tb = meta::struct_to_tuple(b);

		// Do element-wise comparison
		bool ret = true;
		core::each<std::tuple_size<decltype(ta)>::value>([&](auto i) {
			const auto& aa = std::get<i>(ta);
			const auto& bb = std::get<i>(tb);
			ret = ret && CompareSerializableType(aa, bb);
		});
		return ret;
	}
}

struct FooNonTrivial {
	uint32_t a = 0;

	explicit FooNonTrivial(uint32_t value): a(value) {};
	FooNonTrivial() noexcept = default;
	~FooNonTrivial() = default;
	FooNonTrivial(const FooNonTrivial&) = default;
	FooNonTrivial(FooNonTrivial&&) noexcept = default;
	FooNonTrivial& operator=(const FooNonTrivial&) = default;
	FooNonTrivial& operator=(FooNonTrivial&&) noexcept = default;

	bool operator==(const FooNonTrivial& other) const {
		return a == other.a;
	}
};

struct SerializeStruct1 {
	int a1;
	int a2;
	bool b;
	float c;
};

struct SerializeStruct2 {
	FooNonTrivial d;
	int a1;
	int a2;
	bool b;
	float c;
};

struct CustomStruct {
	char* ptr;
	uint32_t size;
};

bool operator==(const CustomStruct& a, const CustomStruct& b) {
	return a.size == b.size && 0 == memcmp(a.ptr, b.ptr, a.size);
}
namespace gaia::ser {
	template <typename Serializer>
	void tag_invoke(save_tag, Serializer& s, const CustomStruct& data) {
		s.save(data.size);
		s.save(data.ptr, data.size);
	}

	template <typename Serializer>
	void tag_invoke(load_tag, Serializer& s, CustomStruct& data) {
		s.load(data.size);
		data.ptr = new char[data.size];
		s.load(data.ptr, data.size);
	}
} // namespace gaia::ser

struct CustomStructInternal {
	char* ptr;
	uint32_t size;

	bool operator==(const CustomStructInternal& other) const {
		return size == other.size && !memcmp(ptr, other.ptr, other.size);
	}

	template <typename Serializer>
	void save(Serializer& s) const {
		s.save(size);
		s.save(ptr, size);
	}

	template <typename Serializer>
	void load(Serializer& s) {
		s.load(size);
		ptr = new char[size];
		s.load(ptr, size);
	}
};

TEST_CASE("Serialization - custom") {
	SUBCASE("external") {
		CustomStruct in, out;
		in.ptr = new char[5];
		in.ptr[0] = 'g';
		in.ptr[1] = 'a';
		in.ptr[2] = 'i';
		in.ptr[3] = 'a';
		in.ptr[4] = '\0';
		in.size = 5;

		ecs::SerializationBuffer s;

		static_assert(!ser::has_func_save<CustomStruct, ecs::SerializationBuffer&>::value);
		static_assert(ser::has_tag_save<ecs::SerializationBuffer, CustomStruct>::value);

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(CompareSerializableTypes(in, out));
		delete[] in.ptr;
		delete[] out.ptr;
	}
	SUBCASE("internal") {
		CustomStructInternal in, out;
		in.ptr = new char[5];
		in.ptr[0] = 'g';
		in.ptr[1] = 'a';
		in.ptr[2] = 'i';
		in.ptr[3] = 'a';
		in.ptr[4] = '\0';
		in.size = 5;

		ecs::SerializationBuffer s;

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(CompareSerializableTypes(in, out));
		delete[] in.ptr;
		delete[] out.ptr;
	}
}

TEST_CASE("Serialization - simple") {
	{
		Int3 in{1, 2, 3}, out{};

		ecs::SerializationBuffer s;

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(CompareSerializableTypes(in, out));
	}

	{
		Position in{1, 2, 3}, out{};

		ecs::SerializationBuffer s;

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(CompareSerializableTypes(in, out));
	}

	{
		PositionSoA in{1, 2, 3}, out{};

		ecs::SerializationBuffer s;

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(CompareSerializableTypes(in, out));
	}

	{
		SerializeStruct1 in{1, 2, true, 3.12345f}, out{};

		ecs::SerializationBuffer s;

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(CompareSerializableTypes(in, out));
	}

	{
		SerializeStruct2 in{FooNonTrivial(111), 1, 2, true, 3.12345f}, out{};

		ecs::SerializationBuffer s;

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(CompareSerializableTypes(in, out));
	}
}

struct SerializeStructSArray {
	cnt::sarray<uint32_t, 8> arr;
	float f;
};

struct SerializeStructSArray_PositionSoA {
	cnt::sarray_soa<PositionSoA, 8> arr;
	float f;

	bool operator==(const SerializeStructSArray_PositionSoA& other) const {
		return f == other.f && arr == other.arr;
	}
};

struct SerializeStructSArrayNonTrivial {
	cnt::sarray<FooNonTrivial, 8> arr;
	float f;

	bool operator==(const SerializeStructSArrayNonTrivial& other) const {
		return f == other.f && arr == other.arr;
	}
};

struct SerializeStructDArray_PositionSoA {
	cnt::darr_soa<PositionSoA> arr;
	float f;

	bool operator==(const SerializeStructDArray_PositionSoA& other) const {
		return f == other.f && arr == other.arr;
	}
};

struct SerializeStructDArray {
	cnt::darr<uint32_t> arr;
	float f;
};

struct SerializeStructDArrayNonTrivial {
	cnt::darr<uint32_t> arr;
	float f;

	bool operator==(const SerializeStructDArrayNonTrivial& other) const {
		return f == other.f && arr == other.arr;
	}
};

struct SerializeCustomStructInternalDArray {
	cnt::darr<CustomStructInternal> arr;
	float f;

	bool operator==(const SerializeCustomStructInternalDArray& other) const {
		return f == other.f && arr == other.arr;
	}
};

TEST_CASE("Serialization - arrays") {
	{
		SerializeStructSArray in{}, out{};
		GAIA_EACH(in.arr) in.arr[i] = i;
		in.f = 3.12345f;

		ecs::SerializationBuffer s;

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(CompareSerializableTypes(in, out));
	}

	{
		SerializeStructSArray_PositionSoA in{}, out{};
		GAIA_EACH(in.arr) {
			in.arr.view_mut<0>()[i] = (float)i;
			in.arr.view_mut<1>()[i] = (float)i;
			in.arr.view_mut<2>()[i] = (float)i;
		}
		in.f = 3.12345f;

		ecs::SerializationBuffer s;

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(CompareSerializableTypes(in, out));
	}

	{
		SerializeStructSArrayNonTrivial in{}, out{};
		GAIA_EACH(in.arr) in.arr[i] = FooNonTrivial(i);
		in.f = 3.12345f;

		ecs::SerializationBuffer s;

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(CompareSerializableTypes(in, out));
	}

	{
		SerializeStructDArray in{}, out{};
		in.arr.resize(10);
		GAIA_EACH(in.arr) in.arr[i] = i;
		in.f = 3.12345f;

		ecs::SerializationBuffer s;

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(CompareSerializableTypes(in, out));
	}

	{
		SerializeStructDArray_PositionSoA in{}, out{};
		GAIA_EACH(in.arr) {
			in.arr.push_back({(float)i, (float)i, (float)i});
		}
		in.f = 3.12345f;

		ecs::SerializationBuffer s;

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(CompareSerializableTypes(in, out));
	}

	{
		SerializeCustomStructInternalDArray in{}, out{};
		in.arr.resize(10);
		for (auto& a: in.arr) {
			a.ptr = new char[5];
			a.ptr[0] = 'g';
			a.ptr[1] = 'a';
			a.ptr[2] = 'i';
			a.ptr[3] = 'a';
			a.ptr[4] = '\0';
			a.size = 5;
		}
		in.f = 3.12345f;

		ecs::SerializationBuffer s;

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(CompareSerializableTypes(in, out));

		for (auto& a: in.arr)
			delete[] a.ptr;
		for (auto& a: out.arr)
			delete[] a.ptr;
	}
}

TEST_CASE("Serialization - hashmap") {
	{
		gaia::cnt::map<int, CustomStruct> in{}, out{};
		GAIA_FOR(5) {
			CustomStruct str;
			str.ptr = new char[6];
			str.ptr[0] = 'g';
			str.ptr[1] = 'a';
			str.ptr[2] = 'i';
			str.ptr[3] = 'a';
			str.ptr[4] = '0' + (char)i;
			str.ptr[5] = '\0';
			str.size = 6;
			in.insert({i, str});
		}

		ecs::SerializationBuffer s;

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(in.size() == out.size());
		if (in.size() == out.size()) {
			auto it0 = in.begin();
			auto it1 = out.begin();
			while (it0 != in.end()) {
				CHECK(CompareSerializableTypes(it0->first, it1->first));
				CHECK(CompareSerializableTypes(it0->second, it1->second));
				++it0;
				++it1;
			}
		}

		for (auto& it: in)
			delete[] it.second.ptr;
		for (auto& it: out)
			delete[] it.second.ptr;
	}
}

TEST_CASE("Serialization - hashset") {
	{
		gaia::cnt::set<CustomStruct> in{}, out{};
		GAIA_FOR(5) {
			CustomStruct str;
			str.ptr = new char[6];
			str.ptr[0] = 'g';
			str.ptr[1] = 'a';
			str.ptr[2] = 'i';
			str.ptr[3] = 'a';
			str.ptr[4] = '0' + (char)i;
			str.ptr[5] = '\0';
			str.size = 6;
			in.insert({str});
		}

		ecs::SerializationBuffer s;

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(in.size() == out.size());
		// TODO: Hashset does not necessasrily return iterated items in the same order
		//.      in which we inserted them. Sort before comparing.
		// if (in.size() == out.size()) {
		// 	auto it0 = in.begin();
		// 	auto it1 = out.begin();
		// 	while (it0 != in.end()) {
		// 		CHECK(CompareSerializableTypes(*it0, *it1));
		// 		++it0;
		// 		++it1;
		// 	}
		// }

		for (auto& it: in)
			delete[] it.ptr;
		for (auto& it: out)
			delete[] it.ptr;
	}
}

#if GAIA_USE_SERIALIZATION

TEST_CASE("Serialization - world self") {
	auto initComponents = [](ecs::World& w) {
		(void)w.add<Position>();
		(void)w.add<PositionSoA>();
	};

	ecs::World in;
	initComponents(in);

	ecs::Entity eats = in.add();
	ecs::Entity carrot = in.add();
	in.del(carrot);
	in.update();
	carrot = in.add();
	ecs::Entity salad = in.add();
	ecs::Entity apple = in.add();

	in.add<Position>(eats, {1, 2, 3});
	in.add<PositionSoA>(eats, {10, 20, 30});
	in.name(eats, "Eats");
	in.name(carrot, "Carrot");
	in.name(salad, "Salad");
	in.name(apple, "Apple");

	ecs::SerializationBufferDyn buffer;
	in.save(buffer);

	//--------
	in.cleanup();
	initComponents(in);
	in.load(buffer);

	Position pos = in.get<Position>(eats);
	CHECK(pos.x == 1.f);
	CHECK(pos.y == 2.f);
	CHECK(pos.z == 3.f);

	PositionSoA pos_soa = in.get<PositionSoA>(eats);
	CHECK(pos_soa.x == 10.f);
	CHECK(pos_soa.y == 20.f);
	CHECK(pos_soa.z == 30.f);

	auto eats2 = in.get("Eats");
	auto carrot2 = in.get("Carrot");
	auto salad2 = in.get("Salad");
	auto apple2 = in.get("Apple");
	CHECK(eats2 == eats);
	CHECK(carrot2 == carrot);
	CHECK(salad2 == salad);
	CHECK(apple2 == apple);
}

TEST_CASE("Serialization - world other") {
	ecs::World in;
	auto initComponents = [](ecs::World& w) {
		(void)w.add<Position>();
		(void)w.add<PositionSoA>();
	};

	initComponents(in);

	ecs::Entity eats = in.add();
	ecs::Entity carrot = in.add();
	ecs::Entity salad = in.add();
	ecs::Entity apple = in.add();

	in.add<Position>(eats, {1, 2, 3});
	in.add<PositionSoA>(eats, {10, 20, 30});
	in.name(eats, "Eats");
	in.name(carrot, "Carrot");
	in.name(salad, "Salad");
	in.name(apple, "Apple");

	ecs::SerializationBufferDyn buffer;
	in.save(buffer);

	TestWorld twld;
	initComponents(wld);
	wld.load(buffer);

	Position pos = wld.get<Position>(eats);
	CHECK(pos.x == 1.f);
	CHECK(pos.y == 2.f);
	CHECK(pos.z == 3.f);

	PositionSoA pos_soa = in.get<PositionSoA>(eats);
	CHECK(pos_soa.x == 10.f);
	CHECK(pos_soa.y == 20.f);
	CHECK(pos_soa.z == 30.f);

	auto eats2 = wld.get("Eats");
	auto carrot2 = wld.get("Carrot");
	auto salad2 = wld.get("Salad");
	auto apple2 = wld.get("Apple");
	CHECK(eats2 == eats);
	CHECK(carrot2 == carrot);
	CHECK(salad2 == salad);
	CHECK(apple2 == apple);
}

TEST_CASE("Serialization - world + query") {
	auto initComponents = [](ecs::World& w) {
		(void)w.add<Position>();
		(void)w.add<PositionSoA>();
		(void)w.add<CustomStruct>();
	};

	char testStr[5] = {'g', 'a', 'i', 'a', '\0'};

	TestWorld in;
	initComponents(in.m_w);

	const uint32_t N = 2;

	cnt::darr<ecs::Entity> ents;
	ents.reserve(N);

	auto create = [&](uint32_t i) {
		auto e = in.m_w.add();
		ents.push_back(e);
		in.m_w.add<Position>(e, {(float)i, (float)i, (float)i});
		in.m_w.add<PositionSoA>(e, {(float)(i * 10), (float)(i * 10), (float)(i * 10)});
		in.m_w.add<CustomStruct>(e, {testStr, 5});
	};
	GAIA_FOR(N) create(i);

	ecs::SerializationBufferDyn buffer;
	in.m_w.save(buffer);

	// Load

	TestWorld twld;
	initComponents(wld);
	wld.load(buffer);

	auto q1 = wld.query().template all<Position>();
	auto q2 = wld.query().template all<PositionSoA>();
	auto q3 = wld.query().template all<CustomStruct>();

	{
		const auto cnt = q1.count();
		CHECK(cnt == N);
	}
	{
		const auto cnt = q2.count();
		CHECK(cnt == N);
	}
	{
		const auto cnt = q3.count();
		CHECK(cnt == N);
	}

	{
		cnt::darr<ecs::Entity> arr;
		q1.arr(arr);

		// Make sure the same entities are returned by each and arr
		// and that they match out expectations.
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
		cnt::darr<PositionSoA> arr;
		q2.arr(arr);
		CHECK(arr.size() == N);
		GAIA_EACH(arr) {
			const auto& pos = arr[i];
			CHECK(pos.x == (float)(i * 10));
			CHECK(pos.y == (float)(i * 10));
			CHECK(pos.z == (float)(i * 10));
		}
	}
	{
		cnt::darr_soa<PositionSoA> arr;
		q2.arr(arr);
		CHECK(arr.size() == N);
		auto ppx = arr.view<0>();
		auto ppy = arr.view<1>();
		auto ppz = arr.view<2>();
		GAIA_EACH(arr) {
			const auto x = ppx[i];
			const auto y = ppy[i];
			const auto z = ppz[i];
			CHECK(x == (float)(i * 10));
			CHECK(y == (float)(i * 10));
			CHECK(z == (float)(i * 10));
		}
	}
	{
		cnt::darr<CustomStruct> arr;
		q3.arr(arr);
		CHECK(arr.size() == N);
		GAIA_EACH(arr) {
			const auto& val = arr[i];
			CHECK(val.size == 5);
			const int cmp_res = strncmp(val.ptr, testStr, 5);
			CHECK(cmp_res == 0);
		}
	}
}

#endif

//------------------------------------------------------------------------------
// Multithreading
//------------------------------------------------------------------------------

template <typename TQueue>
void TestJobQueue_PushPopSteal(bool reverse) {
	mt::JobHandle handle;
	TQueue q;
	bool res;

	for (uint32_t i = 0; i < 5; ++i) {
		CHECK(q.empty());
		res = q.try_push(mt::JobHandle(1, 0, 0));
		CHECK(res);
		CHECK_FALSE(q.empty());
		res = q.try_push(mt::JobHandle(2, 0, 0));
		CHECK(res);
		CHECK_FALSE(q.empty());
		res = q.try_push(mt::JobHandle(3, 0, 0));
		CHECK(res);
		CHECK_FALSE(q.empty());

		res = q.try_pop(handle);
		CHECK(res);
		CHECK_FALSE(q.empty());
		res = q.try_pop(handle);
		CHECK(res);
		CHECK_FALSE(q.empty());
		res = q.try_pop(handle);
		CHECK(res);
		CHECK(q.empty());
		res = q.try_pop(handle);
		CHECK_FALSE(res);
		CHECK(q.empty());
		res = q.try_pop(handle);
		CHECK_FALSE(res);
		CHECK(q.empty());
		res = q.try_pop(handle);
		CHECK_FALSE(res);
		CHECK(q.empty());
	}

	{
		mt::JobHandle handles[3] = {mt::JobHandle(1, 0, 0), mt::JobHandle(2, 0, 0), mt::JobHandle(3, 0, 0)};
		res = q.try_push(std::span(handles, 3));
		CHECK(res);
		CHECK_FALSE(q.empty());

		res = q.try_pop(handle);
		CHECK(res);
		CHECK_FALSE(q.empty());
		res = q.try_pop(handle);
		CHECK(res);
		CHECK_FALSE(q.empty());
		res = q.try_pop(handle);
		CHECK(res);
		CHECK(q.empty());
		res = q.try_pop(handle);
		CHECK_FALSE(res);
		CHECK(q.empty());
		res = q.try_pop(handle);
		CHECK_FALSE(res);
		CHECK(q.empty());
		res = q.try_pop(handle);
		CHECK_FALSE(res);
		CHECK(q.empty());
	}

	{
		CHECK(q.empty());
		(void)q.try_push(mt::JobHandle(1, 0, 0));
		CHECK_FALSE(q.empty());
		(void)q.try_push(mt::JobHandle(2, 0, 0));
		CHECK_FALSE(q.empty());

		res = q.try_pop(handle);
		CHECK(res);
		CHECK_FALSE(q.empty());
		if (reverse)
			CHECK(handle.id() == 1);
		else
			CHECK(handle.id() == 2);

		res = q.try_pop(handle);
		CHECK(res);
		CHECK(q.empty());
		if (reverse)
			CHECK(handle.id() == 2);
		else
			CHECK(handle.id() == 1);

		res = q.try_pop(handle);
		CHECK_FALSE(res);
		CHECK(q.empty());
	}

	{
		(void)q.try_push(mt::JobHandle(1, 0, 0));
		CHECK_FALSE(q.empty());
		(void)q.try_push(mt::JobHandle(2, 0, 0));
		CHECK_FALSE(q.empty());

		res = q.try_steal(handle);
		CHECK(res);
		CHECK_FALSE(q.empty());
		if (reverse)
			CHECK(handle.id() == 2);
		else
			CHECK(handle.id() == 1);

		res = q.try_steal(handle);
		CHECK(res);
		CHECK(q.empty());
		if (reverse)
			CHECK(handle.id() == 1);
		else
			CHECK(handle.id() == 2);

		res = q.try_steal(handle);
		CHECK(res);
		CHECK(handle == (mt::JobHandle)mt::JobNull_t{});
		CHECK(q.empty());
	}
}

template <typename TQueue>
void TestJobQueue_PushPop(bool reverse) {
	mt::JobHandle handle;
	TQueue q;

	{
		q.try_push(mt::JobHandle(1, 0, 0));
		q.try_push(mt::JobHandle(2, 0, 0));

		auto res = q.try_pop(handle);
		CHECK(res);
		if (reverse)
			CHECK(handle.id() == 1);
		else
			CHECK(handle.id() == 2);

		res = q.try_pop(handle);
		CHECK(res);
		if (reverse)
			CHECK(handle.id() == 2);
		else
			CHECK(handle.id() == 1);

		res = q.try_pop(handle);
		CHECK_FALSE(res);
	}
}

constexpr uint32_t JobQueueMTTesterItems = 500;

template <typename TQueue>
struct JobQueueMTTester_PushPopSteal {
	using QueueType = TQueue;

	TQueue* q;
	bool* terminate;
	uint32_t thread_idx;

	std::thread t;
	uint32_t processed = 0;

	JobQueueMTTester_PushPopSteal(TQueue* q_, bool* terminate_, uint32_t idx):
			q(q_), terminate(terminate_), thread_idx(idx) {}
	JobQueueMTTester_PushPopSteal() = default;

	void init() {
		processed = 0;
		t = std::thread([this]() {
			run();
		});
	}

	void fini() {
		if (t.joinable())
			t.join();
	}

	void run() {
		mt::JobHandle handle;

		if (0 == thread_idx) {
			rnd::pseudo_random rng{};
			uint32_t itemsToInsert = JobQueueMTTesterItems;

			// This thread generates 2 items per tick and consumes one
			while (itemsToInsert > 0) {
				// Keep inserting items
				if (!q->try_push(mt::JobHandle(itemsToInsert, 0, 0))) {
					std::this_thread::yield();
					continue;
				}

				--itemsToInsert;

				if (rng.range(0, 1) == 0)
					std::this_thread::yield();

				if (!q->try_pop(handle))
					std::this_thread::yield();
				else
					++processed;
			}

			// Make sure all items are consumed
			while (!q->empty())
				std::this_thread::yield();
			*terminate = true;
		} else {
			while (!*terminate) {
				// Other threads steal work
				const bool res = q->try_steal(handle);

				// Failed race, try again
				if (!res)
					continue;

				// Empty queue, wait a bit and try again
				if (res && handle == (mt::JobHandle)mt::JobNull_t{}) {
					std::this_thread::yield();
					continue;
				}

				// Processed
				++processed;
			}
		}
	}
};

template <typename TQueue>
struct JobQueueMTTester_PushPop {
	using QueueType = TQueue;

	TQueue* q;
	bool* terminate;
	uint32_t thread_idx;

	std::thread t;
	uint32_t processed = 0;

	JobQueueMTTester_PushPop(TQueue* q_, bool* terminate_, uint32_t idx): q(q_), terminate(terminate_), thread_idx(idx) {}
	JobQueueMTTester_PushPop() = default;

	void init() {
		processed = 0;
		t = std::thread([this]() {
			run();
		});
	}

	void fini() {
		if (t.joinable())
			t.join();
	}

	void run() {
		mt::JobHandle handle;

		if (0 == thread_idx) {
			rnd::pseudo_random rng{};
			uint32_t itemsToInsert = JobQueueMTTesterItems;

			// This thread generates 2 items per tick and consumes one
			while (itemsToInsert > 0) {
				// Keep inserting items
				if (!q->try_push(mt::JobHandle(itemsToInsert, 0, 0))) {
					std::this_thread::yield();
					continue;
				}

				--itemsToInsert;

				if (rng.range(0, 1) == 0)
					std::this_thread::yield();

				if (!q->try_pop(handle))
					std::this_thread::yield();
				else
					++processed;
			}

			// Make sure all items are consumed
			while (!q->empty())
				std::this_thread::yield();
			*terminate = true;
		} else {
			while (!*terminate) {
				// Other threads steal work
				if (!q->try_pop(handle)) {
					std::this_thread::yield();
					continue;
				}

				++processed;
			}
		}
	}
};

template <typename TTestType>
void TestJobQueueMT(uint32_t threadCnt) {
	CHECK(threadCnt > 1);

	typename TTestType::QueueType q;
	bool terminate = false;
	cnt::darray<TTestType> testers;
	GAIA_FOR(threadCnt) testers.push_back(TTestType(&q, &terminate, i));

	GAIA_FOR_(100, j) {
		GAIA_FOR(threadCnt) testers[i].init();
		GAIA_FOR(threadCnt) testers[i].fini();
		uint32_t total = 0;
		GAIA_FOR(threadCnt) {
			total += testers[i].processed;
		}
		CHECK(total == JobQueueMTTesterItems);
		terminate = false;
	}
}

TEST_CASE("JobQueue") {
	using jc = mt::JobQueue<1024>;
	using mpmc = mt::MpmcQueue<mt::JobHandle, 1024>;

	SUBCASE("Basic") {
		TestJobQueue_PushPopSteal<jc>(false);
		TestJobQueue_PushPop<mpmc>(true);
	}

	using mt_tester_jc = JobQueueMTTester_PushPopSteal<jc>;
	using mt_tester_mpmc = JobQueueMTTester_PushPop<mpmc>;

	SUBCASE("MT - 2 threads") {
		TestJobQueueMT<mt_tester_jc>(2);
		TestJobQueueMT<mt_tester_mpmc>(2);
	}

	SUBCASE("MT - 4 threads") {
		TestJobQueueMT<mt_tester_jc>(4);
		TestJobQueueMT<mt_tester_mpmc>(4);
	}

	SUBCASE("MT - 16 threads") {
		TestJobQueueMT<mt_tester_jc>(16);
		TestJobQueueMT<mt_tester_mpmc>(16);
	}
}

static uint32_t JobSystemFunc(std::span<const uint32_t> arr) {
	uint32_t sum = 0;
	GAIA_EACH(arr) sum += arr[i];
	return sum;
}

template <typename Func>
void Run_Schedule_Simple(
		const uint32_t* pArr, uint32_t* pRes, uint32_t jobCnt, uint32_t ItemsPerJob, Func func, uint32_t depCnt) {
	auto& tp = mt::ThreadPool::get();
	std::atomic_uint32_t cnt = 0;

	auto* pHandles = (mt::JobHandle*)alloca(sizeof(mt::JobHandle) * jobCnt);
	GAIA_FOR(jobCnt) {
		mt::Job job;
		job.func = [&, i, func]() {
			const auto idxStart = i * ItemsPerJob;
			const auto idxEnd = (i + 1) * ItemsPerJob;
			pRes[i] += func({pArr + idxStart, idxEnd - idxStart});
			cnt.fetch_add(1, std::memory_order_relaxed);
		};
		pHandles[i] = tp.add(job);
	}

	mt::Job dependencyJob;
	dependencyJob.func = [&]() {
		const bool isLast = cnt.load(std::memory_order_acquire) == jobCnt;
		CHECK(isLast);
	};
	auto* pDepHandles = (mt::JobHandle*)alloca(sizeof(mt::JobHandle) * depCnt);
	GAIA_FOR(depCnt) {
		pDepHandles[i] = tp.add(dependencyJob);
		tp.dep(std::span(pHandles, jobCnt), pDepHandles[i]);
		tp.submit(pDepHandles[i]);
	}

	tp.submit(std::span(pHandles, jobCnt));

	GAIA_FOR(depCnt) {
		tp.wait(pDepHandles[i]);
	}

	GAIA_FOR(jobCnt) CHECK(pRes[i] == ItemsPerJob);
	CHECK(cnt == jobCnt);
}

TEST_CASE("Multithreading - Schedule") {
	auto& tp = mt::ThreadPool::get();

	constexpr uint32_t JobCount = 64;
	constexpr uint32_t ItemsPerJob = 5000;
	constexpr uint32_t N = JobCount * ItemsPerJob;

	cnt::sarray<uint32_t, JobCount> res;

	cnt::darr<uint32_t> arr;
	arr.resize(N);
	GAIA_EACH(arr) arr[i] = 1;

	SUBCASE("Max workers") {
		const auto threads = tp.hw_thread_cnt();
		tp.set_max_workers(threads, threads);

		GAIA_FOR(res.max_size()) res[i] = 0;
		Run_Schedule_Simple(arr.data(), res.data(), JobCount, ItemsPerJob, JobSystemFunc, 1);
	}
	SUBCASE("Max workers Deps2") {
		const auto threads = tp.hw_thread_cnt();
		tp.set_max_workers(threads, threads);

		GAIA_FOR(res.max_size()) res[i] = 0;
		Run_Schedule_Simple(arr.data(), res.data(), JobCount, ItemsPerJob, JobSystemFunc, 2);
	}
	SUBCASE("Max workers Deps3") {
		const auto threads = tp.hw_thread_cnt();
		tp.set_max_workers(threads, threads);

		GAIA_FOR(res.max_size()) res[i] = 0;
		Run_Schedule_Simple(arr.data(), res.data(), JobCount, ItemsPerJob, JobSystemFunc, 3);
	}
	SUBCASE("Max workers Deps4") {
		const auto threads = tp.hw_thread_cnt();
		tp.set_max_workers(threads, threads);

		GAIA_FOR(res.max_size()) res[i] = 0;
		Run_Schedule_Simple(arr.data(), res.data(), JobCount, ItemsPerJob, JobSystemFunc, 4);
	}
	SUBCASE("0 workers") {
		tp.set_max_workers(0, 0);

		GAIA_FOR(res.max_size()) res[i] = 0;
		Run_Schedule_Simple(arr.data(), res.data(), JobCount, ItemsPerJob, JobSystemFunc, 1);
	}
	SUBCASE("0 workers Deps2") {
		tp.set_max_workers(0, 0);

		GAIA_FOR(res.max_size()) res[i] = 0;
		Run_Schedule_Simple(arr.data(), res.data(), JobCount, ItemsPerJob, JobSystemFunc, 2);
	}
	SUBCASE("0 workers Deps3") {
		tp.set_max_workers(0, 0);

		GAIA_FOR(res.max_size()) res[i] = 0;
		Run_Schedule_Simple(arr.data(), res.data(), JobCount, ItemsPerJob, JobSystemFunc, 3);
	}
	SUBCASE("0 workers Deps4") {
		tp.set_max_workers(0, 0);

		GAIA_FOR(res.max_size()) res[i] = 0;
		Run_Schedule_Simple(arr.data(), res.data(), JobCount, ItemsPerJob, JobSystemFunc, 4);
	}
}

TEST_CASE("Multithreading - ScheduleParallel") {
	auto& tp = mt::ThreadPool::get();

	constexpr uint32_t JobCount = 64;
	constexpr uint32_t ItemsPerJob = 5000;
	constexpr uint32_t N = JobCount * ItemsPerJob;

	cnt::darr<uint32_t> arr;
	arr.resize(N);
	GAIA_EACH(arr) arr[i] = 1;

	std::atomic_uint32_t sum1 = 0;
	mt::JobParallel j1;
	j1.func = [&arr, &sum1](const mt::JobArgs& args) {
		sum1 += JobSystemFunc({arr.data() + args.idxStart, args.idxEnd - args.idxStart});
	};

	auto work = [&]() {
		auto jobHandle = tp.sched_par(j1, N, ItemsPerJob);
		tp.wait(jobHandle);
		CHECK(sum1 == N);
	};

	SUBCASE("Max workers") {
		const auto threads = tp.hw_thread_cnt();
		tp.set_max_workers(threads, threads);

		work();
	}
	SUBCASE("0 workers") {
		tp.set_max_workers(0, 0);

		work();
	}
}

TEST_CASE("Multithreading - complete") {
	auto& tp = mt::ThreadPool::get();

	constexpr uint32_t Jobs = 15000;

	cnt::darr<mt::JobHandle> handles;
	cnt::darr<uint32_t> res;

	handles.resize(Jobs);
	res.resize(Jobs);

	std::atomic_uint32_t cnt = 0;

	auto work = [&]() {
		GAIA_EACH(res) res[i] = BadIndex;

		GAIA_FOR(Jobs) {
			mt::Job job;
			job.flags = mt::JobCreationFlags::ManualDelete;
			job.func = [&, i]() {
				res[i] = i;
				++cnt;
			};
			handles[i] = tp.add(job);
		}

		tp.submit(std::span(handles.data(), handles.size()));

		GAIA_FOR(Jobs) {
			tp.wait(handles[i]);
			CHECK(res[i] == i);
			tp.del(handles[i]);
		}

		CHECK(cnt == Jobs);
	};

	SUBCASE("Max workers") {
		const auto threads = tp.hw_thread_cnt();
		tp.set_max_workers(threads, threads);

		work();
	}
	SUBCASE("0 workers") {
		tp.set_max_workers(0, 0);

		work();
	}
}

TEST_CASE("Multithreading - CompleteMany") {
	auto& tp = mt::ThreadPool::get();

	constexpr uint32_t Iters = 15000;

	auto work = [&]() {
		srand(0);
		uint32_t res = BadIndex;

		GAIA_FOR(Iters) {
			mt::Job job0{[&res, i]() {
				res = (i + 1);
			}};
			mt::Job job1{[&res, i]() {
				res *= (i + 1);
			}};
			mt::Job job2{[&res, i]() {
				res /= (i + 1); // we add +1 everywhere to avoid division by zero at i==0
			}};

			const mt::JobHandle jobHandle[] = {tp.add(job0), tp.add(job1), tp.add(job2)};

			tp.dep(jobHandle[0], jobHandle[1]);
			tp.dep(jobHandle[1], jobHandle[2]);

			// 2, 0, 1 -> wrong sum
			// Submit jobs in random order to make sure this doesn't work just by accident
			const uint32_t startIdx0 = (uint32_t)rand() % 3;
			const uint32_t startIdx1 = (startIdx0 + 1) % 3;
			const uint32_t startIdx2 = (startIdx0 + 2) % 3;
			tp.submit(jobHandle[startIdx0]);
			tp.submit(jobHandle[startIdx1]);
			tp.submit(jobHandle[startIdx2]);

			tp.wait(jobHandle[2]);

			CHECK(res == (i + 1));
		}
	};

	SUBCASE("Max workers") {
		const auto threads = tp.hw_thread_cnt();
		tp.set_max_workers(threads, threads);

		work();
	}
	SUBCASE("0 workers") {
		tp.set_max_workers(0, 0);

		work();
	}
}

#if GAIA_SYSTEMS_ENABLED

struct SomeData1 {
	uint32_t val;
};
struct SomeData2 {
	uint32_t val;
};

TEST_CASE("Multithreading - Systems") {
	auto& tp = mt::ThreadPool::get();
	TestWorld twld;

	constexpr uint32_t Iters = 15000;

	{
		// 10k of SomeData1
		auto e = wld.add();
		wld.add<SomeData1>(e, {0});
		wld.copy_n(e, 9999);

		// 10k of SomeData1, SomeData2
		auto e2 = wld.add();
		wld.add<SomeData1>(e2, {0});
		wld.add<SomeData2>(e2, {0});
		wld.copy_n(e2, 9999);
	}

	uint32_t data1 = 0;
	uint32_t data2 = 0;

	auto sys1 = wld.system()
									.all<SomeData1>()
									.all<SomeData2>() //
									.on_each([&](SomeData1, SomeData2) {
										++data1;
										++data2;
									});
	auto sys2 = wld.system()
									.all<SomeData1>() //
									.on_each([&](SomeData1) {
										++data1;
									});

	auto handle1 = sys1.job_handle();
	auto handle2 = sys2.job_handle();

	tp.dep(handle1, handle2);
	GAIA_FOR(Iters - 1) {
		tp.submit(handle2);
		tp.submit(handle1);
		tp.wait(handle2);
		GAIA_ASSERT(data1 == 30000);
		GAIA_ASSERT(data2 == 10000);

		data1 = 0;
		data2 = 0;
		tp.reset_state(handle1);
		tp.reset_state(handle2);
		tp.dep_refresh(handle1, handle2);
	}
	{
		tp.submit(handle2);
		tp.submit(handle1);
		tp.wait(handle2);
		GAIA_ASSERT(data1 == 30000);
		GAIA_ASSERT(data2 == 10000);
	}
}

#endif

int main(int argc, char** argv) {
	// Use custom logging. Just for code coverage.
	util::set_log_func(util::detail::log_cached);
	util::set_log_flush_func(util::detail::log_flush_cached);

	return doctest::Context(argc, argv).run();
}