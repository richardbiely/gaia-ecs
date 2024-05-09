#include <gaia.h>

#include "gaia/mem/stack_allocator.h"

#if GAIA_COMPILER_MSVC
	#if _MSC_VER <= 1916
// warning C4100: 'XYZ': unreferenced formal parameter
GAIA_MSVC_WARNING_DISABLE(4100)
	#endif
#endif

#include <catch2/catch_test_macros.hpp>

#include <string>

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
		GAIA_FOR(100) m_w.update();
	}
};

struct Int3 {
	uint32_t x, y, z;
};
struct Position {
	float x, y, z;
};
struct PositionSoA {
	float x, y, z;
	static constexpr auto Layout = mem::DataLayout::SoA;
};
struct PositionSoA8 {
	float x, y, z;
	static constexpr auto Layout = mem::DataLayout::SoA8;
};
struct PositionSoA16 {
	float x, y, z;
	static constexpr auto Layout = mem::DataLayout::SoA16;
};
struct Acceleration {
	float x, y, z;
};
struct Rotation {
	float x, y, z, w;
};
struct DummySoA {
	float x, y;
	bool b;
	float w;
	static constexpr auto Layout = mem::DataLayout::SoA;
};
struct RotationSoA {
	float x, y, z, w;
	static constexpr auto Layout = mem::DataLayout::SoA;
};
struct RotationSoA8 {
	float x, y, z, w;
	static constexpr auto Layout = mem::DataLayout::SoA8;
};
struct RotationSoA16 {
	float x, y, z, w;
	static constexpr auto Layout = mem::DataLayout::SoA16;
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

static constexpr const char* StringComponentDefaultValue =
		"StringComponentDefaultValue_ReasonablyLongSoThatItShouldCauseAHeapAllocationOnAllStdStringImplementations";
static constexpr const char* StringComponentDefaultValue_2 =
		"2_StringComponentDefaultValue_ReasonablyLongSoThatItShouldCauseAHeapAllocationOnAllStdStringImplementations";
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
GAIA_DEFINE_HAS_FUNCTION(foo)
GAIA_DEFINE_HAS_FUNCTION(food)
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

TEST_CASE("StringLookupKey") {
	constexpr uint32_t MaxLen = 32;
	char tmp0[MaxLen];
	char tmp1[MaxLen];
	GAIA_STRFMT(tmp0, MaxLen, "%s", "some string");
	GAIA_STRFMT(tmp1, MaxLen, "%s", "some string");
	core::StringLookupKey<128> l0(tmp0);
	core::StringLookupKey<128> l1(tmp1);
	REQUIRE(l0.len() == l1.len());
	// Two different addresses in memory have to return the same hash if the string is the same
	REQUIRE(l0.hash() == l1.hash());
}

TEST_CASE("has_XYZ_equals_check") {
	{
		constexpr auto hasMember = core::has_member_equals<Dummy0>::value;
		constexpr auto hasGlobal = core::has_global_equals<Dummy0>::value;
		constexpr auto hasFoo1 = has_foo<Dummy0, const Dummy0&>::value;
		constexpr auto hasFoo2 = has_foo<Dummy0, int>::value;
		constexpr auto hasFood = has_food<Dummy0>::value;
		REQUIRE_FALSE(hasMember);
		REQUIRE(hasGlobal);
		REQUIRE(hasFoo1);
		REQUIRE(hasFoo2);
		REQUIRE(!hasFood);
	}
	{
		constexpr auto hasMember = core::has_member_equals<Dummy1>::value;
		constexpr auto hasGlobal = core::has_global_equals<Dummy1>::value;
		constexpr auto hasFoo = has_foo<Dummy1>::value;
		REQUIRE(hasMember);
		REQUIRE_FALSE(hasGlobal);
		REQUIRE_FALSE(hasFoo);
	}
}

TEST_CASE("Intrinsics") {
	SECTION("POPCNT") {
		const uint32_t zero32 = GAIA_POPCNT(0);
		REQUIRE(zero32 == 0);
		const uint64_t zero64 = GAIA_POPCNT64(0);
		REQUIRE(zero64 == 0);

		const uint32_t val32 = GAIA_POPCNT(0x0003002);
		REQUIRE(val32 == 3);
		const uint64_t val64_1 = GAIA_POPCNT64(0x0003002);
		REQUIRE(val64_1 == 3);
		const uint64_t val64_2 = GAIA_POPCNT64(0x00030020000000);
		REQUIRE(val64_2 == 3);
		const uint64_t val64_3 = GAIA_POPCNT64(0x00030020003002);
		REQUIRE(val64_3 == 6);
	}
	SECTION("CLZ") {
		const uint32_t zero32 = GAIA_CLZ(0);
		REQUIRE(zero32 == 32);
		const uint64_t zero64 = GAIA_CLZ64(0);
		REQUIRE(zero64 == 64);

		const uint32_t val32 = GAIA_CLZ(0x0003002);
		REQUIRE(val32 == 1);
		const uint64_t val64_1 = GAIA_CLZ64(0x0003002);
		REQUIRE(val64_1 == 1);
		const uint64_t val64_2 = GAIA_CLZ64(0x00030020000000);
		REQUIRE(val64_2 == 29);
		const uint64_t val64_3 = GAIA_CLZ64(0x00030020003002);
		REQUIRE(val64_3 == 1);
	}
	SECTION("CTZ") {
		const uint32_t zero32 = GAIA_CTZ(0);
		REQUIRE(zero32 == 32);
		const uint64_t zero64 = GAIA_CTZ64(0);
		REQUIRE(zero64 == 64);

		const uint32_t val32 = GAIA_CTZ(0x0003002);
		REQUIRE(val32 == 18);
		const uint64_t val64_1 = GAIA_CTZ64(0x0003002);
		REQUIRE(val64_1 == 50);
		const uint64_t val64_2 = GAIA_CTZ64(0x00030020000000);
		REQUIRE(val64_2 == 22);
		const uint64_t val64_3 = GAIA_CTZ64(0x00030020003002);
		REQUIRE(val64_3 == 22);
	}
	SECTION("FFS") {
		const uint32_t zero32 = GAIA_FFS(0);
		REQUIRE(zero32 == 0);
		const uint64_t zero64 = GAIA_FFS64(0);
		REQUIRE(zero64 == 0);

		const uint32_t val32 = GAIA_FFS(0x0003002);
		REQUIRE(val32 == 2);
		const uint64_t val64_1 = GAIA_FFS64(0x0003002);
		REQUIRE(val64_1 == 2);
		const uint64_t val64_2 = GAIA_FFS64(0x00030020000000);
		REQUIRE(val64_2 == 30);
		const uint64_t val64_3 = GAIA_FFS64(0x00030020003002);
		REQUIRE(val64_3 == 2);
	}
}

TEST_CASE("EntityKinds") {
	REQUIRE(ecs::entity_kind_v<uint32_t> == ecs::EntityKind::EK_Gen);
	REQUIRE(ecs::entity_kind_v<Position> == ecs::EntityKind::EK_Gen);
	REQUIRE(ecs::entity_kind_v<ecs::uni<Position>> == ecs::EntityKind::EK_Uni);
}

TEST_CASE("Memory allocation") {
	SECTION("mem_alloc") {
		void* pMem = mem::mem_alloc(311);
		REQUIRE(pMem != nullptr);
		mem::mem_free(pMem);
	}
	SECTION("mem_alloc_alig A") {
		void* pMem = mem::mem_alloc_alig(1, 16);
		REQUIRE(pMem != nullptr);
		mem::mem_free_alig(pMem);
	}
	SECTION("mem_alloc_alig B") {
		void* pMem = mem::mem_alloc_alig(311, 16);
		REQUIRE(pMem != nullptr);
		mem::mem_free_alig(pMem);
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
		REQUIRE(val == i);
	}

	// Make sure nothing was overriden
	GAIA_FOR(NBlocks) {
		const uint8_t val = bv.get(i * BlockBits);
		REQUIRE(val == i);
	}
}

template <typename Container>
void fixed_arr_test() {
	constexpr auto N = Container::extent;
	static_assert(N > 2); // we need at least 2 items to complete this test
	Container arr;

	GAIA_FOR(N) {
		arr[i] = i;
		REQUIRE(arr[i] == i);
	}
	REQUIRE(arr.back() == N - 1);
	// Verify the values remain the same even after the internal buffer is reallocated
	GAIA_FOR(N) REQUIRE(arr[i] == i);
	// Copy assignment
	{
		Container arrCopy = arr;
		GAIA_FOR(N) REQUIRE(arrCopy[i] == i);
	}
	// Copy constructor
	{
		Container arrCopy(arr);
		GAIA_FOR(N) REQUIRE(arrCopy[i] == i);
	}
	// Move assignment
	{
		Container arrCopy = GAIA_MOV(arr);
		GAIA_FOR(N) REQUIRE(arrCopy[i] == i);
		// move back
		arr = GAIA_MOV(arrCopy);
	}
	// Move constructor
	{
		Container arrCopy(GAIA_MOV(arr));
		GAIA_FOR(N) REQUIRE(arrCopy[i] == i);
		// move back
		arr = GAIA_MOV(arrCopy);
	}

	// Container comparison
	{
		Container arrEmpty;
		REQUIRE_FALSE(arrEmpty == arr);

		Container arr2(arr);
		REQUIRE(arr2 == arr);
	}

	uint32_t cnt = 0;
	for (auto val: arr) {
		REQUIRE(val == cnt);
		++cnt;
	}
	REQUIRE(cnt == N);
	REQUIRE(cnt == arr.size());

	std::span<const typename Container::value_type> view{arr.data(), arr.size()};

	REQUIRE(core::find(arr, 0U) == arr.begin());
	REQUIRE(core::find(arr, N) == arr.end());
	REQUIRE(core::find(view, 0U) == view.begin());
	REQUIRE(core::find(view, N) == view.end());

	REQUIRE(core::has(arr, 0U));
	REQUIRE(core::has(view, 0U));
	REQUIRE_FALSE(core::has(arr, N));
	REQUIRE_FALSE(core::has(view, N));
}

TEST_CASE("Containers - sarr") {
	constexpr uint32_t N = 100;
	using TrivialT = cnt::sarr<uint32_t, N>;
	using NonTrivialT = cnt::sarr<TypeNonTrivial<uint32_t>, N>;
	SECTION("trivial_types") {
		fixed_arr_test<TrivialT>();
	}
	SECTION("non_trivial_types") {
		fixed_arr_test<NonTrivialT>();
	}
}

template <typename Container>
void resizable_arr_test(uint32_t N) {
	GAIA_ASSERT(N > 2); // we need at least 2 items to complete this test
	Container arr;

	GAIA_FOR(N) {
		arr.push_back(i);
		REQUIRE(arr[i] == i);
		REQUIRE(arr.back() == i);
	}
	// Verify the values remain the same even after the internal buffer is reallocated
	GAIA_FOR(N) REQUIRE(arr[i] == i);
	// Copy assignment
	{
		Container arrCopy = arr;
		GAIA_FOR(N) REQUIRE(arrCopy[i] == i);
	}
	// Copy constructor
	{
		Container arrCopy(arr);
		GAIA_FOR(N) REQUIRE(arrCopy[i] == i);
	}
	// Move assignment
	{
		Container arrCopy = GAIA_MOV(arr);
		GAIA_FOR(N) REQUIRE(arrCopy[i] == i);
		// move back
		arr = GAIA_MOV(arrCopy);
	}
	// Move constructor
	{
		Container arrCopy(GAIA_MOV(arr));
		GAIA_FOR(N) REQUIRE(arrCopy[i] == i);
		// move back
		arr = GAIA_MOV(arrCopy);
	}

	// Container comparison
	{
		Container arrEmpty;
		REQUIRE_FALSE(arrEmpty == arr);

		Container arr2(arr);
		REQUIRE(arr2 == arr);
	}

	uint32_t cnt = 0;
	for (auto val: arr) {
		REQUIRE(val == cnt);
		++cnt;
	}
	REQUIRE(cnt == N);
	REQUIRE(cnt == arr.size());

	std::span<const typename Container::value_type> view{arr.data(), arr.size()};

	REQUIRE(core::find(arr, 0U) == arr.begin());
	REQUIRE(core::find(arr, N) == arr.end());
	REQUIRE(core::find(view, 0U) == view.begin());
	REQUIRE(core::find(view, N) == view.end());

	REQUIRE(core::has(arr, 0U));
	REQUIRE(core::has(view, 0U));
	REQUIRE_FALSE(core::has(arr, N));
	REQUIRE_FALSE(core::has(view, N));

	arr.erase(arr.begin());
	REQUIRE(arr.size() == (N - 1));
	REQUIRE(core::find(arr, 0U) == arr.end());
	GAIA_EACH(arr)
	REQUIRE(arr[i] == i + 1);

	arr.clear();
	REQUIRE(arr.empty());

	arr.push_back(11);
	arr.erase(arr.begin());
	REQUIRE(arr.empty());

	arr.push_back(11);
	arr.push_back(12);
	arr.push_back(13);
	arr.push_back(14);
	arr.push_back(15);
	arr.erase(arr.begin() + 1, arr.begin() + 4);
	REQUIRE(arr.size() == 2);
	REQUIRE(arr[0] == 11);
	REQUIRE(arr[1] == 15);

	arr.erase(arr.begin(), arr.end());
	REQUIRE(arr.empty());

	arr.push_back(11);
	arr.push_back(12);
	arr.push_back(13);
	arr.push_back(14);
	arr.push_back(15);
	arr.erase(arr.begin() + 1);
	REQUIRE(arr.size() == 4);
	REQUIRE(arr[0] == 11);
	REQUIRE(arr[1] == 13);
	REQUIRE(arr[2] == 14);
	REQUIRE(arr[3] == 15);
	arr.erase(arr.begin() + 3);
	REQUIRE(arr.size() == 3);
	REQUIRE(arr[0] == 11);
	REQUIRE(arr[1] == 13);
	REQUIRE(arr[2] == 14);
	arr.erase(arr.begin());
	arr.erase(arr.begin());
	arr.erase(arr.begin());
	REQUIRE(arr.size() == 0);
	REQUIRE(arr.empty());

	// 11, 13, 14, 15
	// 11, 13, 13, 14, 15
	// 11, [12], 13, 14, 15
	arr.push_back(11);
	arr.push_back(13);
	arr.push_back(14);
	arr.insert(arr.begin() + 1, 12);
	REQUIRE(arr.size() == 4);
	REQUIRE(arr[0] == 11);
	REQUIRE(arr[1] == 12);
	REQUIRE(arr[2] == 13);
	REQUIRE(arr[3] == 14);

	arr.insert(arr.begin(), 10);
	REQUIRE(arr.size() == 5);
	REQUIRE(arr[0] == 10);
	REQUIRE(arr[1] == 11);
	REQUIRE(arr[2] == 12);
	REQUIRE(arr[3] == 13);
	REQUIRE(arr[4] == 14);
}

TEST_CASE("Containers - sarr_ext") {
	constexpr uint32_t N = 100;
	using TrivialT = cnt::sarr_ext<uint32_t, 100>;
	using NonTrivialT = cnt::sarr_ext<TypeNonTrivial<uint32_t>, 100>;
	SECTION("trivial_types") {
		resizable_arr_test<TrivialT>(N);
	}
	SECTION("non_trivial_types") {
		resizable_arr_test<NonTrivialT>(N);
	}
}

TEST_CASE("Containers - darr") {
	constexpr uint32_t N = 100;
	constexpr uint32_t M = 10000;
	using TrivialT = cnt::darr<uint32_t>;
	using NonTrivialT = cnt::darr<TypeNonTrivial<uint32_t>>;
	SECTION("trivial_types") {
		resizable_arr_test<TrivialT>(N);
		resizable_arr_test<TrivialT>(M);
	}
	SECTION("non_trivial_types") {
		resizable_arr_test<NonTrivialT>(N);
		resizable_arr_test<NonTrivialT>(M);
	}
}

TEST_CASE("Containers - darr_ext") {
	constexpr uint32_t N = 100;
	constexpr uint32_t M = 10000;
	using TrivialT1 = cnt::darr_ext<uint32_t, 50>;
	using TrivialT2 = cnt::darr_ext<uint32_t, 100>;
	using NonTrivialT1 = cnt::darr_ext<TypeNonTrivial<uint32_t>, 50>;
	using NonTrivialT2 = cnt::darr_ext<TypeNonTrivial<uint32_t>, 100>;

	SECTION("trivial_types") {
		resizable_arr_test<TrivialT1>(N);
		resizable_arr_test<TrivialT1>(M);

		resizable_arr_test<TrivialT2>(N);
		resizable_arr_test<TrivialT2>(M);
	}
	SECTION("non_trivial_types") {
		resizable_arr_test<NonTrivialT1>(N);
		resizable_arr_test<NonTrivialT1>(M);

		resizable_arr_test<NonTrivialT2>(N);
		resizable_arr_test<NonTrivialT2>(M);
	}
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
		REQUIRE(addr % TPolicyAlign == 0);
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
		REQUIRE(a.b == 16);
		REQUIRE(a.arr[0].id == ecs::Entity(1, 2));
		REQUIRE(a.arr[1].id == ecs::Entity(2, 30));
		REQUIRE(a.arr[2].id == ecs::Entity(3, 400));

		TArrInter test = {
				{ecs::Entity(1, 2), {}, {}, {}}, {ecs::Entity(2, 30), {}, {}, {}}, {ecs::Entity(3, 400), {}, {}, {}}};
		REQUIRE(test == a.arr);
	}
	{
		auto& a = arr[1];
		REQUIRE(a.b == 214);
		REQUIRE(a.arr[0].id == ecs::Entity(10, 2));
		REQUIRE(a.arr[1].id == ecs::Entity(20, 90));
		REQUIRE(a.arr[2].id == ecs::Entity(30, 421));

		TArrInter test = {
				{ecs::Entity(10, 2), {}, {}, {}}, {ecs::Entity(20, 90), {}, {}, {}}, {ecs::Entity(30, 421), {}, {}, {}}};
		REQUIRE(test == a.arr);
	}
}

TEST_CASE("Containers - sringbuffer") {
	{
		cnt::sringbuffer<uint32_t, 5> arr = {0, 1, 2, 3, 4};
		uint32_t val{};

		REQUIRE_FALSE(arr.empty());
		REQUIRE(arr.front() == 0);
		REQUIRE(arr.back() == 4);

		arr.pop_front(val);
		REQUIRE(val == 0);
		REQUIRE_FALSE(arr.empty());
		REQUIRE(arr.front() == 1);
		REQUIRE(arr.back() == 4);

		arr.pop_front(val);
		REQUIRE(val == 1);
		REQUIRE_FALSE(arr.empty());
		REQUIRE(arr.front() == 2);
		REQUIRE(arr.back() == 4);

		arr.pop_front(val);
		REQUIRE(val == 2);
		REQUIRE_FALSE(arr.empty());
		REQUIRE(arr.front() == 3);
		REQUIRE(arr.back() == 4);

		arr.pop_back(val);
		REQUIRE(val == 4);
		REQUIRE_FALSE(arr.empty());
		REQUIRE(arr.front() == 3);
		REQUIRE(arr.back() == 3);

		arr.pop_back(val);
		REQUIRE(val == 3);
		REQUIRE(arr.empty());
	}

	{
		cnt::sringbuffer<uint32_t, 5> arr;
		uint32_t val{};

		REQUIRE(arr.empty());
		{
			arr.push_back(0);
			REQUIRE_FALSE(arr.empty());
			REQUIRE(arr.front() == 0);
			REQUIRE(arr.back() == 0);

			arr.push_back(1);
			REQUIRE_FALSE(arr.empty());
			REQUIRE(arr.front() == 0);
			REQUIRE(arr.back() == 1);

			arr.push_back(2);
			REQUIRE_FALSE(arr.empty());
			REQUIRE(arr.front() == 0);
			REQUIRE(arr.back() == 2);

			arr.push_back(3);
			REQUIRE_FALSE(arr.empty());
			REQUIRE(arr.front() == 0);
			REQUIRE(arr.back() == 3);

			arr.push_back(4);
			REQUIRE_FALSE(arr.empty());
			REQUIRE(arr.front() == 0);
			REQUIRE(arr.back() == 4);
		}
		{
			arr.pop_front(val);
			REQUIRE(val == 0);
			REQUIRE_FALSE(arr.empty());
			REQUIRE(arr.front() == 1);
			REQUIRE(arr.back() == 4);

			arr.pop_front(val);
			REQUIRE(val == 1);
			REQUIRE_FALSE(arr.empty());
			REQUIRE(arr.front() == 2);
			REQUIRE(arr.back() == 4);

			arr.pop_front(val);
			REQUIRE(val == 2);
			REQUIRE_FALSE(arr.empty());
			REQUIRE(arr.front() == 3);
			REQUIRE(arr.back() == 4);

			arr.pop_back(val);
			REQUIRE(val == 4);
			REQUIRE_FALSE(arr.empty());
			REQUIRE(arr.front() == 3);
			REQUIRE(arr.back() == 3);

			arr.pop_back(val);
			REQUIRE(val == 3);
			REQUIRE(arr.empty());
		}
	}
}

TEST_CASE("Containers - ilist") {
	struct EntityContainer: cnt::ilist_item {
		int value;

		EntityContainer() = default;
		EntityContainer(uint32_t index, uint32_t generation): cnt::ilist_item(index, generation) {}
	};
	SECTION("0 -> 1 -> 2") {
		cnt::ilist<EntityContainer, ecs::Entity> il;
		ecs::Entity handles[3];

		handles[0] = il.alloc();
		il[handles[0].id()].value = 100;
		REQUIRE(handles[0].id() == 0);
		REQUIRE(il[0].idx == 0);
		REQUIRE(handles[0].gen() == il[0].gen);
		REQUIRE(il[0].gen == 0);
		handles[1] = il.alloc();
		il[handles[1].id()].value = 200;
		REQUIRE(handles[1].id() == 1);
		REQUIRE(il[1].idx == 1);
		REQUIRE(handles[1].gen() == il[1].gen);
		REQUIRE(il[1].gen == 0);
		handles[2] = il.alloc();
		il[handles[2].id()].value = 300;
		REQUIRE(handles[2].id() == 2);
		REQUIRE(il[2].idx == 2);
		REQUIRE(handles[2].gen() == il[2].gen);
		REQUIRE(il[2].gen == 0);

		il.free(handles[2]);
		REQUIRE(il[2].idx == ecs::Entity::IdMask);
		REQUIRE(il[2].gen == 1);
		il.free(handles[1]);
		REQUIRE(il[1].idx == 2);
		REQUIRE(il[1].gen == 1);
		il.free(handles[0]);
		REQUIRE(il[0].idx == 1);
		REQUIRE(il[0].gen == 1);

		handles[0] = il.alloc();
		REQUIRE(handles[0].id() == 0);
		REQUIRE(il[0].value == 100);
		REQUIRE(il[0].idx == 1);
		REQUIRE(handles[0].gen() == il[0].gen);
		REQUIRE(il[0].gen == 1);
		handles[1] = il.alloc();
		REQUIRE(handles[1].id() == 1);
		REQUIRE(il[1].value == 200);
		REQUIRE(il[1].idx == 2);
		REQUIRE(handles[1].gen() == il[1].gen);
		REQUIRE(il[1].gen == 1);
		handles[2] = il.alloc();
		REQUIRE(handles[2].id() == 2);
		REQUIRE(il[2].value == 300);
		REQUIRE(il[2].idx == ecs::Entity::IdMask);
		REQUIRE(handles[2].gen() == il[2].gen);
		REQUIRE(il[2].gen == 1);
	}
	SECTION("2 -> 1 -> 0") {
		cnt::ilist<EntityContainer, ecs::Entity> il;
		ecs::Entity handles[3];

		handles[0] = il.alloc();
		il[handles[0].id()].value = 100;
		REQUIRE(handles[0].id() == 0);
		REQUIRE(il[0].idx == 0);
		REQUIRE(handles[0].gen() == il[0].gen);
		REQUIRE(il[0].gen == 0);
		handles[1] = il.alloc();
		il[handles[1].id()].value = 200;
		REQUIRE(handles[1].id() == 1);
		REQUIRE(il[1].idx == 1);
		REQUIRE(handles[1].gen() == il[1].gen);
		REQUIRE(il[1].gen == 0);
		handles[2] = il.alloc();
		il[handles[2].id()].value = 300;
		REQUIRE(handles[2].id() == 2);
		REQUIRE(il[2].idx == 2);
		REQUIRE(handles[2].gen() == il[2].gen);
		REQUIRE(il[2].gen == 0);

		il.free(handles[0]);
		REQUIRE(il[0].idx == ecs::Entity::IdMask);
		REQUIRE(il[0].gen == 1);
		il.free(handles[1]);
		REQUIRE(il[1].idx == 0);
		REQUIRE(il[1].gen == 1);
		il.free(handles[2]);
		REQUIRE(il[2].idx == 1);
		REQUIRE(il[2].gen == 1);

		handles[0] = il.alloc();
		REQUIRE(handles[0].id() == 2);
		const auto idx0 = handles[0].id();
		REQUIRE(il[idx0].value == 300);
		REQUIRE(il[idx0].idx == 1);
		REQUIRE(handles[0].gen() == il[idx0].gen);
		REQUIRE(il[idx0].gen == 1);
		handles[1] = il.alloc();
		REQUIRE(handles[1].id() == 1);
		const auto idx1 = handles[1].id();
		REQUIRE(il[idx1].value == 200);
		REQUIRE(il[idx1].idx == 0);
		REQUIRE(handles[1].gen() == il[idx1].gen);
		REQUIRE(il[idx1].gen == 1);
		handles[2] = il.alloc();
		REQUIRE(handles[2].id() == 0);
		const auto idx2 = handles[2].id();
		REQUIRE(il[idx2].value == 100);
		REQUIRE(il[idx2].idx == ecs::Entity::IdMask);
		REQUIRE(handles[2].gen() == il[idx2].gen);
		REQUIRE(il[idx2].gen == 1);
	}
}

template <uint32_t NBits>
void test_bitset() {
	// Following tests expect at least 5 bits of space
	static_assert(NBits >= 5);

	SECTION("Bit operations") {
		cnt::bitset<NBits> bs;
		REQUIRE(bs.count() == 0);
		REQUIRE(bs.size() == NBits);
		REQUIRE(bs.any() == false);
		REQUIRE(bs.all() == false);
		REQUIRE(bs.none() == true);

		bs.set(0, true);
		REQUIRE(bs.test(0) == true);
		REQUIRE(bs.count() == 1);
		REQUIRE(bs.any() == true);
		REQUIRE(bs.all() == false);
		REQUIRE(bs.none() == false);

		bs.set(1, true);
		REQUIRE(bs.test(1) == true);
		REQUIRE(bs.count() == 2);
		REQUIRE(bs.any() == true);
		REQUIRE(bs.all() == false);
		REQUIRE(bs.none() == false);

		bs.set(1, false);
		REQUIRE(bs.test(1) == false);
		REQUIRE(bs.count() == 1);
		REQUIRE(bs.any() == true);
		REQUIRE(bs.all() == false);
		REQUIRE(bs.none() == false);

		bs.flip(1);
		REQUIRE(bs.test(1) == true);
		REQUIRE(bs.count() == 2);
		REQUIRE(bs.any() == true);
		REQUIRE(bs.all() == false);
		REQUIRE(bs.none() == false);

		bs.flip(1);
		REQUIRE(bs.test(1) == false);
		REQUIRE(bs.count() == 1);
		REQUIRE(bs.any() == true);
		REQUIRE(bs.all() == false);
		REQUIRE(bs.none() == false);

		bs.reset(0);
		REQUIRE(bs.test(0) == false);
		REQUIRE(bs.count() == 0);
		REQUIRE(bs.any() == false);
		REQUIRE(bs.all() == false);
		REQUIRE(bs.none() == true);

		bs.set();
		REQUIRE(bs.count() == NBits);
		REQUIRE(bs.any() == true);
		REQUIRE(bs.all() == true);
		REQUIRE(bs.none() == false);

		bs.flip();
		REQUIRE(bs.count() == 0);
		REQUIRE(bs.any() == false);
		REQUIRE(bs.all() == false);
		REQUIRE(bs.none() == true);

		bs.flip();
		REQUIRE(bs.count() == NBits);
		REQUIRE(bs.any() == true);
		REQUIRE(bs.all() == true);
		REQUIRE(bs.none() == false);

		bs.reset();
		REQUIRE(bs.count() == 0);
		REQUIRE(bs.any() == false);
		REQUIRE(bs.all() == false);
		REQUIRE(bs.none() == true);
	}
	SECTION("Iteration") {
		{
			cnt::bitset<NBits> bs;
			uint32_t i = 0;
			for ([[maybe_unused]] auto val: bs)
				++i;
			REQUIRE(i == 0);
		}
		auto fwd_iterator_test = [](std::span<uint32_t> vals) {
			cnt::bitset<NBits> bs;
			for (uint32_t bit: vals)
				bs.set(bit);
			const uint32_t c = bs.count();

			uint32_t i = 0;
			for (auto val: bs) {
				REQUIRE(vals[i] == val);
				++i;
			}
			REQUIRE(i == c);
			REQUIRE(i == (uint32_t)vals.size());
		};
		{
			uint32_t vals[]{1, 2, 3};
			fwd_iterator_test(vals);
		}
		{
			uint32_t vals[]{0, 2, 3};
			fwd_iterator_test(vals);
		}
		{
			uint32_t vals[]{1, 3, NBits - 1};
			fwd_iterator_test(vals);
		}
		{
			uint32_t vals[]{1, NBits - 2, NBits - 1};
			fwd_iterator_test(vals);
		}
		{
			uint32_t vals[]{0, 1, NBits - 1};
			fwd_iterator_test(vals);
		}
		{
			uint32_t vals[]{0, 3, NBits - 1};
			fwd_iterator_test(vals);
		}
	}
}

TEST_CASE("Containers - bitset") {
	SECTION("5 bits") {
		test_bitset<5>();
	}
	SECTION("11 bits") {
		test_bitset<11>();
	}
	SECTION("32 bits") {
		test_bitset<32>();
	}
	SECTION("33 bits") {
		test_bitset<33>();
	}
	SECTION("64 bits") {
		test_bitset<64>();
	}
	SECTION("90 bits") {
		test_bitset<90>();
	}
	SECTION("128 bits") {
		test_bitset<128>();
	}
	SECTION("150 bits") {
		test_bitset<150>();
	}
	SECTION("512 bits") {
		test_bitset<512>();
	}
	SECTION("Ranges 11 bits") {
		cnt::bitset<11> bs;
		bs.set(1);
		bs.set(10);
		bs.flip(2, 9);
		for (uint32_t i = 1; i <= 10; ++i)
			REQUIRE(bs.test(i) == true);
		bs.flip(2, 9);
		for (uint32_t i = 2; i < 10; ++i)
			REQUIRE(bs.test(i) == false);
		REQUIRE(bs.test(1));
		REQUIRE(bs.test(10));

		bs.reset();
		bs.flip(0, 0);
		REQUIRE(bs.test(0));
		REQUIRE(bs.count() == 1);
		REQUIRE(bs.any() == true);
		REQUIRE(bs.all() == false);

		bs.reset();
		bs.flip(10, 10);
		REQUIRE(bs.test(10));
		REQUIRE(bs.count() == 1);
		REQUIRE(bs.any() == true);
		REQUIRE(bs.all() == false);

		bs.reset();
		bs.flip(0, 10);
		REQUIRE(bs.count() == 11);
		REQUIRE(bs.all() == true);
		bs.flip(0, 10);
		REQUIRE(bs.count() == 0);
		REQUIRE(bs.none() == true);
	}
	SECTION("Ranges 64 bits") {
		cnt::bitset<64> bs;
		bs.set(1);
		bs.set(10);
		bs.flip(2, 9);
		for (uint32_t i = 1; i <= 10; ++i)
			REQUIRE(bs.test(i) == true);
		bs.flip(2, 9);
		for (uint32_t i = 2; i < 10; ++i)
			REQUIRE(bs.test(i) == false);
		REQUIRE(bs.test(1));
		REQUIRE(bs.test(10));

		bs.reset();
		bs.flip(0, 0);
		REQUIRE(bs.test(0));
		REQUIRE(bs.count() == 1);
		REQUIRE(bs.any() == true);
		REQUIRE(bs.all() == false);

		bs.reset();
		bs.flip(63, 63);
		REQUIRE(bs.test(63));
		REQUIRE(bs.count() == 1);
		REQUIRE(bs.any() == true);
		REQUIRE(bs.all() == false);

		bs.reset();
		bs.flip(0, 63);
		REQUIRE(bs.count() == 64);
		REQUIRE(bs.all() == true);
		bs.flip(0, 63);
		REQUIRE(bs.count() == 0);
		REQUIRE(bs.none() == true);
	}
	SECTION("Ranges 101 bits") {
		cnt::bitset<101> bs;
		bs.set(1);
		bs.set(100);
		bs.flip(2, 99);
		for (uint32_t i = 1; i <= 100; ++i)
			REQUIRE(bs.test(i) == true);
		bs.flip(2, 99);
		for (uint32_t i = 2; i < 100; ++i)
			REQUIRE(bs.test(i) == false);
		REQUIRE(bs.test(1));
		REQUIRE(bs.test(100));

		bs.reset();
		bs.flip(0, 0);
		REQUIRE(bs.test(0));
		REQUIRE(bs.count() == 1);
		REQUIRE(bs.any() == true);
		REQUIRE(bs.all() == false);

		bs.reset();
		bs.flip(100, 100);
		REQUIRE(bs.test(100));
		REQUIRE(bs.count() == 1);
		REQUIRE(bs.any() == true);
		REQUIRE(bs.all() == false);

		bs.reset();
		bs.flip(0, 100);
		REQUIRE(bs.count() == 101);
		REQUIRE(bs.all() == true);
		bs.flip(0, 100);
		REQUIRE(bs.count() == 0);
		REQUIRE(bs.none() == true);
	}
}

template <uint32_t NBits>
void test_dbitset() {
	// Following tests expect at least 5 bits of space
	static_assert(NBits >= 5);

	SECTION("Bit operations") {
		cnt::dbitset bs;
		REQUIRE(bs.count() == 0);
		REQUIRE(bs.size() == 1);
		REQUIRE(bs.any() == false);
		REQUIRE(bs.all() == false);
		REQUIRE(bs.none() == true);

		bs.set(0, true);
		REQUIRE(bs.test(0) == true);
		REQUIRE(bs.count() == 1);
		REQUIRE(bs.any() == true);
		REQUIRE(bs.all() == true);
		REQUIRE(bs.none() == false);

		bs.set(1, true);
		REQUIRE(bs.test(1) == true);
		REQUIRE(bs.count() == 2);
		REQUIRE(bs.any() == true);
		REQUIRE(bs.all() == true);
		REQUIRE(bs.none() == false);

		bs.set(1, false);
		REQUIRE(bs.test(1) == false);
		REQUIRE(bs.count() == 1);
		REQUIRE(bs.any() == true);
		REQUIRE(bs.all() == false);
		REQUIRE(bs.none() == false);

		bs.flip(1);
		REQUIRE(bs.test(1) == true);
		REQUIRE(bs.count() == 2);
		REQUIRE(bs.any() == true);
		REQUIRE(bs.all() == true);
		REQUIRE(bs.none() == false);

		bs.flip(1);
		REQUIRE(bs.test(1) == false);
		REQUIRE(bs.count() == 1);
		REQUIRE(bs.any() == true);
		REQUIRE(bs.all() == false);
		REQUIRE(bs.none() == false);

		bs.reset(0);
		REQUIRE(bs.test(0) == false);
		REQUIRE(bs.count() == 0);
		REQUIRE(bs.any() == false);
		REQUIRE(bs.all() == false);
		REQUIRE(bs.none() == true);

		bs.set();
		REQUIRE(bs.count() == bs.size());
		REQUIRE(bs.any() == true);
		REQUIRE(bs.all() == true);
		REQUIRE(bs.none() == false);

		bs.flip();
		REQUIRE(bs.count() == 0);
		REQUIRE(bs.any() == false);
		REQUIRE(bs.all() == false);
		REQUIRE(bs.none() == true);

		bs.flip();
		REQUIRE(bs.count() == bs.size());
		REQUIRE(bs.any() == true);
		REQUIRE(bs.all() == true);
		REQUIRE(bs.none() == false);

		bs.reset();
		REQUIRE(bs.count() == 0);
		REQUIRE(bs.any() == false);
		REQUIRE(bs.all() == false);
		REQUIRE(bs.none() == true);
	}
	SECTION("Iteration") {
		{
			cnt::dbitset bs;
			uint32_t i = 0;
			for ([[maybe_unused]] auto val: bs)
				++i;
			REQUIRE(i == 0);
		}
		auto fwd_iterator_test = [](std::span<uint32_t> vals) {
			cnt::dbitset bs;
			for (uint32_t bit: vals)
				bs.set(bit);
			const uint32_t c = bs.count();

			uint32_t i = 0;
			for (auto val: bs) {
				REQUIRE(vals[i] == val);
				++i;
			}
			REQUIRE(i == c);
			REQUIRE(i == (uint32_t)vals.size());
		};
		{
			uint32_t vals[]{1, 2, 3};
			fwd_iterator_test(vals);
		}
		{
			uint32_t vals[]{0, 2, 3};
			fwd_iterator_test(vals);
		}
		{
			uint32_t vals[]{1, 3, NBits - 1};
			fwd_iterator_test(vals);
		}
		{
			uint32_t vals[]{1, NBits - 2, NBits - 1};
			fwd_iterator_test(vals);
		}
		{
			uint32_t vals[]{0, 1, NBits - 1};
			fwd_iterator_test(vals);
		}
		{
			uint32_t vals[]{0, 3, NBits - 1};
			fwd_iterator_test(vals);
		}
	}
}

TEST_CASE("Containers - dbitset") {
	SECTION("5 bits") {
		test_dbitset<5>();
	}
	SECTION("11 bits") {
		test_dbitset<11>();
	}
	SECTION("32 bits") {
		test_dbitset<32>();
	}
	SECTION("33 bits") {
		test_dbitset<33>();
	}
	SECTION("64 bits") {
		test_dbitset<64>();
	}
	SECTION("90 bits") {
		test_dbitset<90>();
	}
	SECTION("128 bits") {
		test_dbitset<128>();
	}
	SECTION("150 bits") {
		test_dbitset<150>();
	}
	SECTION("512 bits") {
		test_dbitset<512>();
	}
	SECTION("Ranges 11 bits") {
		cnt::dbitset bs;
		bs.resize(11);

		bs.set(1);
		bs.set(10);
		bs.flip(2, 9);
		for (uint32_t i = 1; i <= 10; ++i)
			REQUIRE(bs.test(i) == true);
		bs.flip(2, 9);
		for (uint32_t i = 2; i < 10; ++i)
			REQUIRE(bs.test(i) == false);
		REQUIRE(bs.test(1));
		REQUIRE(bs.test(10));

		bs.reset();
		bs.flip(0, 0);
		REQUIRE(bs.test(0));
		REQUIRE(bs.count() == 1);
		REQUIRE(bs.any() == true);
		REQUIRE(bs.all() == false);

		bs.reset();
		bs.flip(10, 10);
		REQUIRE(bs.test(10));
		REQUIRE(bs.count() == 1);
		REQUIRE(bs.any() == true);
		REQUIRE(bs.all() == false);

		bs.reset();
		bs.flip(0, 10);
		REQUIRE(bs.count() == 11);
		REQUIRE(bs.all() == true);
		bs.flip(0, 10);
		REQUIRE(bs.count() == 0);
		REQUIRE(bs.none() == true);
	}
	SECTION("Ranges 64 bits") {
		cnt::dbitset bs;
		bs.resize(64);

		bs.set(1);
		bs.set(10);
		bs.flip(2, 9);
		for (uint32_t i = 1; i <= 10; ++i)
			REQUIRE(bs.test(i) == true);
		bs.flip(2, 9);
		for (uint32_t i = 2; i < 10; ++i)
			REQUIRE(bs.test(i) == false);
		REQUIRE(bs.test(1));
		REQUIRE(bs.test(10));

		bs.reset();
		bs.flip(0, 0);
		REQUIRE(bs.test(0));
		REQUIRE(bs.count() == 1);
		REQUIRE(bs.any() == true);
		REQUIRE(bs.all() == false);

		bs.reset();
		bs.flip(63, 63);
		REQUIRE(bs.test(63));
		REQUIRE(bs.count() == 1);
		REQUIRE(bs.any() == true);
		REQUIRE(bs.all() == false);

		bs.reset();
		bs.flip(0, 63);
		REQUIRE(bs.count() == 64);
		REQUIRE(bs.all() == true);
		bs.flip(0, 63);
		REQUIRE(bs.count() == 0);
		REQUIRE(bs.none() == true);
	}
	SECTION("Ranges 101 bits") {
		cnt::dbitset bs;
		bs.resize(101);

		bs.set(1);
		bs.set(100);
		bs.flip(2, 99);
		for (uint32_t i = 1; i <= 100; ++i)
			REQUIRE(bs.test(i) == true);
		bs.flip(2, 99);
		for (uint32_t i = 2; i < 100; ++i)
			REQUIRE(bs.test(i) == false);
		REQUIRE(bs.test(1));
		REQUIRE(bs.test(100));

		bs.reset();
		bs.flip(0, 0);
		REQUIRE(bs.test(0));
		REQUIRE(bs.count() == 1);
		REQUIRE(bs.any() == true);
		REQUIRE(bs.all() == false);

		bs.reset();
		bs.flip(100, 100);
		REQUIRE(bs.test(100));
		REQUIRE(bs.count() == 1);
		REQUIRE(bs.any() == true);
		REQUIRE(bs.all() == false);

		bs.reset();
		bs.flip(0, 100);
		REQUIRE(bs.count() == 101);
		REQUIRE(bs.all() == true);
		bs.flip(0, 100);
		REQUIRE(bs.count() == 0);
		REQUIRE(bs.none() == true);
	}
}

TEST_CASE("each") {
	constexpr uint32_t N = 10;
	SECTION("index agument") {
		uint32_t cnt = 0;
		core::each<N>([&cnt](auto i) {
			cnt += i;
		});
		REQUIRE(cnt == 0 + 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9);
	}
	SECTION("no index agument") {
		uint32_t cnt = 0;
		core::each<N>([&cnt]() {
			++cnt;
		});
		REQUIRE(cnt == N);
	}
}

TEST_CASE("each_ext") {
	constexpr uint32_t N = 10;
	SECTION("index agument") {
		uint32_t cnt = 0;
		core::each_ext<2, N - 1, 2>([&cnt](auto i) {
			cnt += i;
		});
		REQUIRE(cnt == 2 + 4 + 6 + 8);
	}
	SECTION("no index agument") {
		uint32_t cnt = 0;
		core::each_ext<2, N - 1, 2>([&cnt]() {
			++cnt;
		});
		REQUIRE(cnt == 4);
	}
}

TEST_CASE("each_tuple") {
	SECTION("func(Args)") {
		uint32_t val = 0;
		core::each_tuple(std::make_tuple(69, 10, 20), [&val](const auto& value) {
			val += value;
		});
		REQUIRE(val == 99);
	}
	SECTION("func(Args, iter)") {
		uint32_t val = 0;
		uint32_t iter = 0;
		core::each_tuple(std::make_tuple(69, 10, 20), [&](const auto& value, uint32_t i) {
			val += value;
			REQUIRE(i == iter);
			++iter;
		});
		REQUIRE(val == 99);
	}
}

TEST_CASE("each_tuple_ext") {
	SECTION("func(Args)") {
		uint32_t val = 0;
		core::each_tuple_ext<1, 3>(std::make_tuple(69, 10, 20), [&val](const auto& value) {
			val += value;
		});
		REQUIRE(val == 30);
	}
	SECTION("func(Args, iter)") {
		uint32_t val = 0;
		uint32_t iter = 1;
		core::each_tuple_ext<1, 3>(std::make_tuple(69, 10, 20), [&](const auto& value, uint32_t i) {
			val += value;
			REQUIRE(i == iter);
			++iter;
		});
		REQUIRE(val == 30);
	}
}

TEST_CASE("each_tuple2") {
	SECTION("func(Args)") {
		uint32_t val = 0;
		using TTuple = std::tuple<int, int, double>;
		core::each_tuple<TTuple>([&val](auto&& item) {
			val += sizeof(item);
		});
		REQUIRE(val == 16);
	}
	SECTION("func(Args, iter)") {
		uint32_t val = 0;
		uint32_t iter = 0;
		using TTuple = std::tuple<int, int, double>;
		core::each_tuple<TTuple>([&](auto&& item, uint32_t i) {
			val += sizeof(item);
			REQUIRE(i == iter);
			++iter;
		});
		REQUIRE(val == 16);
	}
}

TEST_CASE("each_tuple_ext2") {
	SECTION("func(Args)") {
		uint32_t val = 0;
		using TTuple = std::tuple<int, int, double>;
		core::each_tuple_ext<1, 3, TTuple>([&val](auto&& item) {
			val += sizeof(item);
		});
		REQUIRE(val == 12);
	}
	SECTION("func(Args, iter)") {
		uint32_t val = 0;
		uint32_t iter = 1;
		using TTuple = std::tuple<int, int, double>;
		core::each_tuple_ext<1, 3, TTuple>([&](auto&& item, uint32_t i) {
			val += sizeof(item);
			REQUIRE(i == iter);
			++iter;
		});
		REQUIRE(val == 12);
	}
}

TEST_CASE("each_pack") {
	uint32_t val = 0;
	core::each_pack(
			[&val](const auto& value) {
				val += value;
			},
			69, 10, 20);
	REQUIRE(val == 99);
}

template <bool IsRuntime, typename C>
void sort_descending(C arr) {
	using TValue = typename C::value_type;

	for (TValue i = 0; i < (TValue)arr.size(); ++i)
		arr[i] = i;
	if constexpr (IsRuntime)
		core::sort(arr, core::is_greater<TValue>());
	else
		core::sort_ct(arr, core::is_greater<TValue>());
	for (uint32_t i = 1; i < arr.size(); ++i)
		REQUIRE(arr[i - 1] > arr[i]);
}

template <bool IsRuntime, typename C>
void sort_ascending(C arr) {
	using TValue = typename C::value_type;

	for (TValue i = 0; i < (TValue)arr.size(); ++i)
		arr[i] = i;
	if constexpr (IsRuntime)
		core::sort(arr, core::is_smaller<TValue>());
	else
		core::sort_ct(arr, core::is_smaller<TValue>());
	for (uint32_t i = 1; i < arr.size(); ++i)
		REQUIRE(arr[i - 1] < arr[i]);
}

TEST_CASE("Compile-time sort descending") {
	sort_descending<false>(cnt::sarray<uint32_t, 2>{});
	sort_descending<false>(cnt::sarray<uint32_t, 3>{});
	sort_descending<false>(cnt::sarray<uint32_t, 4>{});
	sort_descending<false>(cnt::sarray<uint32_t, 5>{});
	sort_descending<false>(cnt::sarray<uint32_t, 6>{});
	sort_descending<false>(cnt::sarray<uint32_t, 7>{});
	sort_descending<false>(cnt::sarray<uint32_t, 8>{});
	sort_descending<false>(cnt::sarray<uint32_t, 9>{});
	sort_descending<false>(cnt::sarray<uint32_t, 10>{});
	sort_descending<false>(cnt::sarray<uint32_t, 11>{});
	sort_descending<false>(cnt::sarray<uint32_t, 12>{});
	sort_descending<false>(cnt::sarray<uint32_t, 13>{});
	sort_descending<false>(cnt::sarray<uint32_t, 14>{});
	sort_descending<false>(cnt::sarray<uint32_t, 15>{});
	sort_descending<false>(cnt::sarray<uint32_t, 16>{});
	sort_descending<false>(cnt::sarray<uint32_t, 17>{});
	sort_descending<false>(cnt::sarray<uint32_t, 18>{});
}

TEST_CASE("Compile-time sort ascending") {
	sort_ascending<false>(cnt::sarray<uint32_t, 2>{});
	sort_ascending<false>(cnt::sarray<uint32_t, 3>{});
	sort_ascending<false>(cnt::sarray<uint32_t, 4>{});
	sort_ascending<false>(cnt::sarray<uint32_t, 5>{});
	sort_ascending<false>(cnt::sarray<uint32_t, 6>{});
	sort_ascending<false>(cnt::sarray<uint32_t, 7>{});
	sort_ascending<false>(cnt::sarray<uint32_t, 8>{});
	sort_ascending<false>(cnt::sarray<uint32_t, 9>{});
	sort_ascending<false>(cnt::sarray<uint32_t, 10>{});
	sort_ascending<false>(cnt::sarray<uint32_t, 11>{});
	sort_ascending<false>(cnt::sarray<uint32_t, 12>{});
	sort_ascending<false>(cnt::sarray<uint32_t, 13>{});
	sort_ascending<false>(cnt::sarray<uint32_t, 14>{});
	sort_ascending<false>(cnt::sarray<uint32_t, 15>{});
	sort_ascending<false>(cnt::sarray<uint32_t, 16>{});
	sort_ascending<false>(cnt::sarray<uint32_t, 17>{});
	sort_ascending<false>(cnt::sarray<uint32_t, 18>{});
}

TEST_CASE("Run-time sort - sorting network") {
	sort_descending<true>(cnt::sarray<uint32_t, 5>{});
	sort_ascending<true>(cnt::sarray<uint32_t, 5>{});
}

TEST_CASE("Run-time sort - bubble sort") {
	sort_descending<true>(cnt::sarray<uint32_t, 15>{});
	sort_ascending<true>(cnt::sarray<uint32_t, 15>{});
}

TEST_CASE("Run-time sort - quick sort") {
	sort_descending<true>(cnt::sarray<uint32_t, 45>{});
	sort_ascending<true>(cnt::sarray<uint32_t, 45>{});
}

template <typename T>
void TestDataLayoutAoS() {
	constexpr uint32_t N = 100;
	cnt::sarray<T, N> data;

	GAIA_FOR(N) {
		const auto f = (float)(i + 1);
		data[i] = {f, f, f};

		auto val = data[i];
		REQUIRE(val.x == f);
		REQUIRE(val.y == f);
		REQUIRE(val.z == f);
	}

	SECTION("Make sure that all values are correct (e.g. that they were not overriden by one of the loop iteration)") {
		GAIA_FOR(N) {
			const auto f = (float)(i + 1);

			auto val = data[i];
			REQUIRE(val.x == f);
			REQUIRE(val.y == f);
			REQUIRE(val.z == f);
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
		REQUIRE(val.x == f[0]);
		REQUIRE(val.y == f[1]);
		REQUIRE(val.z == f[2]);

		const float ff[] = {data.template soa_view<0>()[i], data.template soa_view<1>()[i], data.template soa_view<2>()[i]};
		REQUIRE(ff[0] == f[0]);
		REQUIRE(ff[1] == f[1]);
		REQUIRE(ff[2] == f[2]);
	};

	{
		cnt::sarray<T, N> data;

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
		cnt::darray<T> data;

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
		REQUIRE(val.x == f[0]);
		REQUIRE(val.y == f[1]);
		REQUIRE(val.b == true);
		REQUIRE(val.w == f[2]);

		const float ff[] = {data.template soa_view<0>()[i], data.template soa_view<1>()[i], data.template soa_view<3>()[i]};
		const bool b = data.template soa_view<2>()[i];
		REQUIRE(ff[0] == f[0]);
		REQUIRE(ff[1] == f[1]);
		REQUIRE(b == true);
		REQUIRE(ff[2] == f[2]);
	};

	{
		cnt::sarray<DummySoA, N> data{};

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
		cnt::darray<DummySoA> data;

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

TEST_CASE("Entity - valid/has") {
	TestWorld twld;

	auto e = wld.add();
	REQUIRE(wld.valid(e));
	REQUIRE(wld.has(e));

	wld.del(e);
	REQUIRE_FALSE(wld.valid(e));
	REQUIRE_FALSE(wld.has(e));
}

TEST_CASE("Entity - EntityBad") {
	REQUIRE(ecs::Entity{} == ecs::EntityBad);

	TestWorld twld;
	REQUIRE_FALSE(wld.valid(ecs::EntityBad));
	REQUIRE_FALSE(wld.has(ecs::EntityBad));

	auto e = wld.add();
	REQUIRE(e != ecs::EntityBad);
	REQUIRE_FALSE(e == ecs::EntityBad);
	REQUIRE(e.entity());
}

TEST_CASE("Entity copy") {
	TestWorld twld;

	auto e1 = wld.add();
	auto e2 = wld.add();
	wld.add(e1, e2);
	auto e3 = wld.copy(e1);

	REQUIRE(wld.has(e3, e2));
}

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
		REQUIRE(a == e);
	};

	GAIA_FOR(N) create();

	auto q = wld.query().no<ecs::Component>().no<ecs::Core_>();
	q.arr(arr);
	REQUIRE(arr.size() - 3 == ents.size()); // 3 for core component

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

		REQUIRE(wld.has<Int3>(e));
		auto val = wld.get<Int3>(e);
		REQUIRE(val.x == i);
		REQUIRE(val.y == i);
		REQUIRE(val.z == i);
	};

	GAIA_FOR(N) create(i);
	GAIA_FOR(N) verify(i);

	{
		const auto& p = wld.add<Int3>();
		auto e = wld.add();
		wld.add(e, p.entity, Int3{1, 2, 3});

		REQUIRE(wld.has(e, p.entity));
		REQUIRE(wld.has<Int3>(e));
		auto val0 = wld.get<Int3>(e);
		REQUIRE(val0.x == 1);
		REQUIRE(val0.y == 2);
		REQUIRE(val0.z == 3);
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
	wld.add<dummy::Position>(e, {2, 2, 2});
	auto e2 = wld.add();
	wld.add<Position>(e2);
	auto a3 = wld.add();
	wld.add<dummy::Position>(a3);
	auto a4 = wld.copy(a3);
	(void)a4;

	REQUIRE(wld.has<Position>(e));
	REQUIRE(wld.has<dummy::Position>(e));

	{
		auto p1 = wld.get<Position>(e);
		REQUIRE(p1.x == 1.f);
		REQUIRE(p1.y == 1.f);
		REQUIRE(p1.z == 1.f);
	}
	{
		auto p2 = wld.get<dummy::Position>(e);
		REQUIRE(p2.x == 2.f);
		REQUIRE(p2.y == 2.f);
		REQUIRE(p2.z == 2.f);
	}
	{
		auto q1 = wld.query().all<Position>();
		const auto c1 = q1.count();
		REQUIRE(c1 == 2);

		auto q2 = wld.query().all<dummy::Position>();
		const auto c2 = q2.count();
		REQUIRE(c2 == 3);

		auto q3 = wld.query().no<Position>();
		const auto c3 = q3.count();
		REQUIRE(c3 > 0); // It's going to be a bunch

		auto q4 = wld.query().all<dummy::Position>().no<Position>();
		const auto c4 = q4.count();
		REQUIRE(c4 == 2);
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

		REQUIRE(wld.has<Int3>(e));
		REQUIRE(wld.has<Position>(e));
		REQUIRE(wld.has<Empty>(e));
		REQUIRE(wld.has<Rotation>(e));
		REQUIRE(wld.has<Scale>(e));

		{
			auto val = wld.get<Int3>(e);
			REQUIRE(val.x == 3);
			REQUIRE(val.y == 3);
			REQUIRE(val.z == 3);
		}
		{
			auto val = wld.get<Position>(e);
			REQUIRE(val.x == 1.f);
			REQUIRE(val.y == 1.f);
			REQUIRE(val.z == 1.f);
		}
		{
			auto val = wld.get<Rotation>(e);
			REQUIRE(val.x == 2.f);
			REQUIRE(val.y == 2.f);
			REQUIRE(val.z == 2.f);
			REQUIRE(val.w == 2.f);
		}
		{
			auto val = wld.get<Scale>(e);
			REQUIRE(val.x == 4.f);
			REQUIRE(val.y == 4.f);
			REQUIRE(val.z == 4.f);
		}
	};

	const uint32_t N = 1'500;
	GAIA_FOR(N) create();
}

TEST_CASE("Add - many components, bulk") {
	TestWorld twld;

	auto create = [&]() {
		auto e = wld.add();

		wld.build(e).add<Int3, Position, Empty>().add<Else>().add<Rotation>().add<Scale>();

		REQUIRE(wld.has<Int3>(e));
		REQUIRE(wld.has<Position>(e));
		REQUIRE(wld.has<Empty>(e));
		REQUIRE(wld.has<Else>(e));
		REQUIRE(wld.has<Rotation>(e));
		REQUIRE(wld.has<Scale>(e));

		wld.acc_mut(e)
				.set<Int3>({3, 3, 3})
				.set<Position>({1, 1, 1})
				.set<Else>({true})
				.set<Rotation>({2, 2, 2, 2})
				.set<Scale>({4, 4, 4});

		{
			auto val = wld.get<Int3>(e);
			REQUIRE(val.x == 3);
			REQUIRE(val.y == 3);
			REQUIRE(val.z == 3);
		}
		{
			auto val = wld.get<Position>(e);
			REQUIRE(val.x == 1.f);
			REQUIRE(val.y == 1.f);
			REQUIRE(val.z == 1.f);
		}
		{
			auto val = wld.get<Rotation>(e);
			REQUIRE(val.x == 2.f);
			REQUIRE(val.y == 2.f);
			REQUIRE(val.z == 2.f);
			REQUIRE(val.w == 2.f);
		}
		{
			auto val = wld.get<Scale>(e);
			REQUIRE(val.x == 4.f);
			REQUIRE(val.y == 4.f);
			REQUIRE(val.z == 4.f);
		}

		{
			auto setter = wld.acc_mut(e);
			auto& pos = setter.mut<Int3>();
			pos = {30, 30, 30};

			{
				auto val = wld.get<Int3>(e);
				REQUIRE(val.x == 30);
				REQUIRE(val.y == 30);
				REQUIRE(val.z == 30);

				val = setter.get<Int3>();
				REQUIRE(val.x == 30);
				REQUIRE(val.y == 30);
				REQUIRE(val.z == 30);
			}
		}
	};

	const uint32_t N = 1'500;
	GAIA_FOR(N) create();
}

TEST_CASE("Pair") {
	{
		TestWorld twld;
		auto a = wld.add();
		auto b = wld.add();
		auto p = ecs::Pair(a, b);
		REQUIRE(p.first() == a);
		REQUIRE(p.second() == b);
		auto pe = (ecs::Entity)p;
		REQUIRE(wld.get(pe.id()) == a);
		REQUIRE(wld.get(pe.gen()) == b);
	}
	{
		TestWorld twld;
		auto a = wld.add<Position>().entity;
		auto b = wld.add<ecs::Requires_>().entity;
		auto p = ecs::Pair(a, b);
		REQUIRE(ecs::is_pair<decltype(p)>::value);
		REQUIRE(p.first() == a);
		REQUIRE(p.second() == b);
		auto pe = (ecs::Entity)p;
		REQUIRE_FALSE(ecs::is_pair<decltype(pe)>::value);
		REQUIRE(wld.get(pe.id()) == a);
		REQUIRE(wld.get(pe.gen()) == b);
	}
	struct Start {};
	struct Stop {};
	{
		REQUIRE(ecs::is_pair<ecs::pair<Start, Position>>::value);
		REQUIRE(ecs::is_pair<ecs::pair<Position, Start>>::value);

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
		REQUIRE(pci.entity == pci2.entity);
		REQUIRE(upci.entity == upci2.entity);
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
		REQUIRE(p.x == 5);
		REQUIRE(p.y == 5);
		REQUIRE(p.z == 5);

		wld.add<ecs::pair<Start, ecs::uni<Position>>>(e, {50, 50, 50}); // 19, 14:19
		auto spu = wld.get<ecs::pair<Start, ecs::uni<Position>>>(e);
		REQUIRE(spu.x == 50);
		REQUIRE(spu.y == 50);
		REQUIRE(spu.z == 50);
		REQUIRE(p.x == 5);
		REQUIRE(p.y == 5);
		REQUIRE(p.z == 5);

		wld.add<ecs::pair<Start, Position>>(e, {100, 100, 100}); // 14:18
		auto sp = wld.get<ecs::pair<Start, Position>>(e);
		REQUIRE(sp.x == 100);
		REQUIRE(sp.y == 100);
		REQUIRE(sp.z == 100);

		p = wld.get<Position>(e);
		spu = wld.get<ecs::pair<Start, ecs::uni<Position>>>(e);
		REQUIRE(p.x == 5);
		REQUIRE(p.y == 5);
		REQUIRE(p.z == 5);
		REQUIRE(spu.x == 50);
		REQUIRE(spu.y == 50);
		REQUIRE(spu.z == 50);

		{
			uint32_t i = 0;
			auto q = wld.query().all<ecs::pair<Start, Position>>();
			q.each([&]() {
				++i;
			});
			REQUIRE(i == 1);
		}
	}
}

TEST_CASE("CantCombine") {
	TestWorld twld;
	auto weak = wld.add();
	auto strong = wld.add();
	wld.add(weak, {ecs::CantCombine, strong});

	auto dummy = wld.add();
	wld.add(dummy, strong);
#if !GAIA_ASSERT_ENABLED
	// Can be tested only with asserts disabled because the situation is assert-protected.
	wld.add(dummy, weak);
	REQUIRE(wld.has(dummy, strong));
	REQUIRE(!wld.has(dummy, weak));
#endif
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
	REQUIRE(wld.has(rabbit, herbivore));
	REQUIRE(wld.has(rabbit, animal));

	wld.del(rabbit, animal);
	REQUIRE(wld.has(rabbit, animal));

	wld.del(rabbit, herbivore);
	REQUIRE(wld.has(rabbit, herbivore));

	wld.del(rabbit, carrot);
	REQUIRE(!wld.has(rabbit, carrot));
}

TEST_CASE("Inheritance (Is)") {
	TestWorld twld;
	ecs::Entity animal = wld.add();
	ecs::Entity herbivore = wld.add();
	wld.add<Position>(herbivore, {});
	wld.add<Rotation>(herbivore, {});
	ecs::Entity rabbit = wld.add();
	ecs::Entity hare = wld.add();
	ecs::Entity wolf = wld.add();

	wld.add(wolf, wolf); // make wolf an archetype
	wld.as(herbivore, animal); // wld.add(herbivore, {ecs::Is, animal});
	wld.as(rabbit, herbivore); // wld.add(rabbit, {ecs::Is, herbivore});
	wld.as(hare, herbivore); // wld.add(hare, {ecs::Is, herbivore});
	wld.as(wolf, animal); // wld.add(wolf, {ecs::Is, animal})

	REQUIRE(wld.is(herbivore, animal));
	REQUIRE(wld.is(rabbit, herbivore));
	REQUIRE(wld.is(hare, herbivore));
	REQUIRE(wld.is(rabbit, animal));
	REQUIRE(wld.is(hare, animal));
	REQUIRE(wld.is(wolf, animal));

	REQUIRE_FALSE(wld.is(animal, herbivore));
	REQUIRE_FALSE(wld.is(wolf, herbivore));

	{
		uint32_t i = 0;
		ecs::Query q = wld.query().all(ecs::Pair(ecs::Is, animal));
		q.each([&](ecs::Entity entity) {
			// runs for herbivore, rabbit, hare, wolf
			const bool isOK = entity == hare || entity == rabbit || entity == herbivore || entity == wolf;
			REQUIRE(isOK);

			++i;
		});
		REQUIRE(i == 4);
	}
	{
		uint32_t i = 0;
		ecs::Query q = wld.query().all(ecs::Pair(ecs::Is, animal)).no(wolf);
		q.each([&](ecs::Entity entity) {
			// runs for herbivore, rabbit, hare, wolf
			const bool isOK = entity == hare || entity == rabbit || entity == herbivore;
			REQUIRE(isOK);

			++i;
		});
		REQUIRE(i == 3);
	}
	{
		uint32_t i = 0;
		ecs::Query q = wld.query().all(ecs::Pair(ecs::Is, herbivore));
		q.each([&](ecs::Entity entity) {
			// runs for rabbit and hare
			const bool isOK = entity == hare || entity == rabbit;
			REQUIRE(isOK);

			++i;
		});
		REQUIRE(i == 2);
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
		REQUIRE_FALSE(isValid);
		REQUIRE_FALSE(hasEntity);
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
		REQUIRE_FALSE(isValid);
		REQUIRE_FALSE(hasEntity);
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
		REQUIRE(pos.x == id);
		REQUIRE(pos.y == id);
		REQUIRE(pos.z == id);
		return e;
	};
	auto remove = [&](ecs::Entity e) {
		wld.del(e);
		const bool isValid = wld.valid(e);
		const bool hasEntity = wld.has(e);
		REQUIRE_FALSE(isValid);
		REQUIRE_FALSE(hasEntity);
	};

	GAIA_FOR(N) create(i);
	GAIA_FOR(N) remove(arr[i]);

	wld.update();
	GAIA_FOR(N) {
		const auto e = arr[i];
		const bool isValid = wld.valid(e);
		const bool hasEntity = wld.has(e);
		REQUIRE_FALSE(isValid);
		REQUIRE_FALSE(hasEntity);
	}
}

void verify_entity_has(const ecs::ComponentCache& cc, ecs::Entity entity) {
	const auto* res = cc.find(entity);
	REQUIRE(res != nullptr);
}

template <typename T>
void verify_name_has(const ecs::ComponentCache& cc, const char* str) {
	auto name = cc.get<T>().name;
	REQUIRE(name.str() != nullptr);
	REQUIRE(name.len() > 1);
	const auto* res = cc.find(str);
	REQUIRE(res != nullptr);
}

void verify_name_has_not(const ecs::ComponentCache& cc, const char* str) {
	const auto* item = cc.find(str);
	REQUIRE(item == nullptr);
}
template <typename T>
void verify_name_has_not(const ecs::ComponentCache& cc) {
	const auto* item = cc.find<T>();
	REQUIRE(item == nullptr);
}

#define verify_name_has(name) verify_name_has<name>(cc, #name);
#define verify_name_has_not(name)                                                                                      \
	{                                                                                                                    \
		verify_name_has_not<name>(cc);                                                                                     \
		verify_name_has_not(cc, #name);                                                                                    \
	}

TEST_CASE("Component names") {
	SECTION("direct registration") {
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
	SECTION("entity+component") {
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
	auto q3 = wld.query<UseCachedQuery>().template all<Position, Rotation>();

	{
		const auto cnt = q1.count();
		REQUIRE(cnt == N);
	}
	{
		cnt::darr<ecs::Entity> arr;
		q1.arr(arr);
		REQUIRE(arr.size() == N);
		GAIA_EACH(arr) REQUIRE(arr[i] == ents[i]);
	}
	{
		cnt::darr<Position> arr;
		q1.arr(arr);
		REQUIRE(arr.size() == N);
		GAIA_EACH(arr) {
			const auto& pos = arr[i];
			REQUIRE(pos.x == (float)i);
			REQUIRE(pos.y == (float)i);
			REQUIRE(pos.z == (float)i);
		}
	}
	{
		const auto cnt = q1.count();
		REQUIRE(cnt > 0);

		const auto empty = q1.empty();
		REQUIRE(empty == false);

		uint32_t cnt2 = 0;
		q1.each([&]() {
			++cnt2;
		});
		REQUIRE(cnt == cnt2);
	}

	{
		const auto cnt = q2.count();
		REQUIRE(cnt == 0);

		const auto empty = q2.empty();
		REQUIRE(empty == true);

		uint32_t cnt2 = 0;
		q2.each([&]() {
			++cnt2;
		});
		REQUIRE(cnt == cnt2);
	}

	{
		const auto cnt = q3.count();
		REQUIRE(cnt == 0);

		const auto empty = q3.empty();
		REQUIRE(empty == true);

		uint32_t cnt3 = 0;
		q3.each([&]() {
			++cnt3;
		});
		REQUIRE(cnt == cnt3);
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
		REQUIRE(cnt == N);
	}
	{
		cnt::darr<ecs::Entity> arr;
		q4.arr(arr);
		REQUIRE(arr.size() == N);
		GAIA_EACH(arr) REQUIRE(arr[i] == ents[i]);
	}
	{
		cnt::darr<Position> arr;
		q4.arr(arr);
		REQUIRE(arr.size() == N);
		GAIA_EACH(arr) {
			const auto& pos = arr[i];
			REQUIRE(pos.x == (float)i);
			REQUIRE(pos.y == (float)i);
			REQUIRE(pos.z == (float)i);
		}
	}
	{
		const auto cnt = q4.count();
		REQUIRE(cnt > 0);

		const auto empty = q4.empty();
		REQUIRE(empty == false);

		uint32_t cnt2 = 0;
		q4.each([&]() {
			++cnt2;
		});
		REQUIRE(cnt == cnt2);
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
		REQUIRE(cnt == cnt2);
	}
}

TEST_CASE("Query - QueryResult") {
	SECTION("Cached query") {
		Test_Query_QueryResult<ecs::Query>();
	}
	SECTION("Non-cached query") {
		Test_Query_QueryResult<ecs::QueryUncached>();
	}
	SECTION("Caching") {
		struct Player {};
		struct Health {
			uint32_t value;
		};

		TestWorld twld;

		const auto player = wld.add();
		wld.build(player).add<Player>().add<Health>();

		uint32_t matches = 0;
		auto qp = wld.query().all<Health, Player>();
		qp.each([&matches]() {
			++matches;
		});
		REQUIRE(matches == 1);

		// Add new entity with some new component. Creates a new archetype.
		const auto something = wld.add();
		wld.add<Something>(something);

		// We still need to match the player entity once
		matches = 0;
		qp.each([&matches]() {
			++matches;
		});
		REQUIRE(matches == 1);

		// New the new item also has the health component
		wld.add<Health>(something);

		// We still need to match the player entity once
		matches = 0;
		qp.each([&matches]() {
			++matches;
		});
		REQUIRE(matches == 1);
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
		b.add<Position, Scale>();
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
		REQUIRE(memcmp((void*)&p0, (void*)&p1, sizeof(p0)) == 0);
		cmp.try_emplace(e, Data{p1, s1});
	};

	GAIA_FOR(N) create(i);

	constexpr bool UseCachedQuery = std::is_same_v<TQuery, ecs::Query>;
	auto q1 = wld.query<UseCachedQuery>().template all<Position>();
	auto q2 = wld.query<UseCachedQuery>().template all<Rotation>();
	auto q3 = wld.query<UseCachedQuery>().template all<Position, Rotation>();
	auto q4 = wld.query<UseCachedQuery>().template all<Position, Scale>();
	auto q5 = wld.query<UseCachedQuery>().template all<Position, Scale, Something>();

	{
		ents.clear();
		q1.arr(ents);
		REQUIRE(ents.size() == N);
	}
	{
		cnt::darr<Position> arr;
		q1.arr(arr);
		REQUIRE(arr.size() == N);
		GAIA_EACH(arr) {
			const auto e = ents[i];
			const auto& v0 = arr[i];
			const auto& v1 = wld.get<Position>(e);
			REQUIRE(memcmp((const void*)&v0, (const void*)&v1, sizeof(v0)) == 0);
			REQUIRE(memcmp((const void*)&v0, (const void*)&cmp[e].p, sizeof(v0)) == 0);
		}
	}
	{
		const auto cnt = q1.count();
		REQUIRE(cnt > 0);

		const auto empty = q1.empty();
		REQUIRE(empty == false);

		uint32_t cnt2 = 0;
		q1.each([&]() {
			++cnt2;
		});
		REQUIRE(cnt2 == cnt);
	}

	{
		const auto cnt = q2.count();
		REQUIRE(cnt == 0);

		const auto empty = q2.empty();
		REQUIRE(empty == true);

		uint32_t cnt2 = 0;
		q2.each([&]() {
			++cnt2;
		});
		REQUIRE(cnt2 == cnt);
	}

	{
		const auto cnt = q3.count();
		REQUIRE(cnt == 0);

		const auto empty = q3.empty();
		REQUIRE(empty == true);

		uint32_t cnt3 = 0;
		q3.each([&]() {
			++cnt3;
		});
		REQUIRE(cnt3 == cnt);
	}

	{
		ents.clear();
		q4.arr(ents);
		REQUIRE(ents.size() == N);
	}
	{
		cnt::darr<Position> arr;
		q4.arr(arr);
		REQUIRE(arr.size() == N);
		GAIA_EACH(arr) {
			const auto e = ents[i];
			const auto& v0 = arr[i];
			const auto& v1 = wld.get<Position>(e);
			REQUIRE(memcmp((const void*)&v0, (const void*)&v1, sizeof(v0)) == 0);
			REQUIRE(memcmp((const void*)&v0, (const void*)&cmp[e].p, sizeof(v0)) == 0);
		}
	}
	{
		cnt::darr<Scale> arr;
		q4.arr(arr);
		REQUIRE(arr.size() == N);
		GAIA_EACH(arr) {
			const auto e = ents[i];
			const auto& v0 = arr[i];
			const auto& v1 = wld.get<Scale>(e);
			REQUIRE(memcmp((const void*)&v0, (const void*)&v1, sizeof(v0)) == 0);
			REQUIRE(memcmp((const void*)&v0, (const void*)&cmp[e].s, sizeof(v0)) == 0);
		}
	}
	{
		const auto cnt = q4.count();
		REQUIRE(cnt > 0);

		const auto empty = q4.empty();
		REQUIRE(empty == false);

		uint32_t cnt4 = 0;
		q4.each([&]() {
			++cnt4;
		});
		REQUIRE(cnt4 == cnt);
	}

	{
		ents.clear();
		q5.arr(ents);
		REQUIRE(ents.size() == N / 2);
	}
	{
		cnt::darr<Position> arr;
		q5.arr(arr);
		REQUIRE(arr.size() == ents.size());
		REQUIRE(arr.size() == N / 2);

		GAIA_EACH(arr) {
			const auto e = ents[i];
			const auto& v0 = arr[i];
			const auto& v1 = wld.get<Position>(e);
			REQUIRE(memcmp((const void*)&v0, (const void*)&v1, sizeof(v0)) == 0);
			REQUIRE(memcmp((const void*)&v0, (const void*)&cmp[e].p, sizeof(v0)) == 0);
		}
	}
	{
		cnt::darr<Scale> arr;
		q5.arr(arr);
		REQUIRE(arr.size() == N / 2);
		GAIA_EACH(arr) {
			const auto e = ents[i];
			const auto& v0 = arr[i];
			const auto& v1 = wld.get<Scale>(e);
			REQUIRE(memcmp((const void*)&v0, (const void*)&v1, sizeof(v0)) == 0);
			REQUIRE(memcmp((const void*)&v0, (const void*)&cmp[e].s, sizeof(v0)) == 0);
		}
	}
	{
		const auto cnt = q5.count();
		REQUIRE(cnt > 0);

		const auto empty = q5.empty();
		REQUIRE(empty == false);

		uint32_t cnt5 = 0;
		q5.each([&]() {
			++cnt5;
		});
		REQUIRE(cnt5 == cnt);
	}
}

TEST_CASE("Query - QueryResult complex") {
	SECTION("Cached query") {
		Test_Query_QueryResult_Complex<ecs::Query>();
	}
	SECTION("Non-cached query") {
		Test_Query_QueryResult_Complex<ecs::QueryUncached>();
	}
}

TEST_CASE("Relationship") {
	TestWorld twld;

	SECTION("Simple") {
		auto wolf = wld.add();
		auto rabbit = wld.add();
		auto carrot = wld.add();
		auto eats = wld.add();

		wld.add(rabbit, {eats, carrot});
		wld.add(wolf, {eats, rabbit});

		REQUIRE(wld.has(rabbit, {eats, carrot}));
		REQUIRE(wld.has(wolf, {eats, rabbit}));
		REQUIRE_FALSE(wld.has(wolf, {eats, carrot}));
		REQUIRE_FALSE(wld.has(rabbit, {eats, wolf}));

		{
			auto q = wld.query().add({ecs::QueryOp::All, ecs::QueryAccess::None, ecs::Pair(eats, carrot)});
			// auto q = wld.query().all<ecs::Pair(eats, carrot)>();
			const auto cnt = q.count();
			REQUIRE(cnt == 1);

			uint32_t i = 0;
			q.each([&](ecs::Entity entity) {
				REQUIRE(entity == rabbit);
				++i;
			});
			REQUIRE(i == cnt);
		}
		{
			auto q = wld.query().add("(%e, %e)", eats.value(), carrot.value());
			const auto cnt = q.count();
			REQUIRE(cnt == 1);

			uint32_t i = 0;
			q.each([&](ecs::Entity entity) {
				REQUIRE(entity == rabbit);
				++i;
			});
			REQUIRE(i == cnt);
		}

		{
			auto q = wld.query().add({ecs::QueryOp::All, ecs::QueryAccess::None, ecs::Pair(eats, rabbit)});
			// auto q = wld.query().all<ecs::Pair(eats, rabbit)>();
			const auto cnt = q.count();
			REQUIRE(cnt == 1);

			uint32_t i = 0;
			q.each([&](ecs::Entity entity) {
				REQUIRE(entity == wolf);
				++i;
			});
			REQUIRE(i == cnt);
		}
	}

	SECTION("Simple - bulk") {
		auto wolf = wld.add();
		auto rabbit = wld.add();
		auto carrot = wld.add();
		auto eats = wld.add();

		wld.build(rabbit).add({eats, carrot});
		wld.build(wolf).add({eats, rabbit});

		REQUIRE(wld.has(rabbit, {eats, carrot}));
		REQUIRE(wld.has(wolf, {eats, rabbit}));
		REQUIRE_FALSE(wld.has(wolf, {eats, carrot}));
		REQUIRE_FALSE(wld.has(rabbit, {eats, wolf}));

		{
			auto q = wld.query().add({ecs::QueryOp::All, ecs::QueryAccess::None, ecs::Pair(eats, carrot)});
			// auto q = wld.query().all<ecs::Pair(eats, carrot)>();
			const auto cnt = q.count();
			REQUIRE(cnt == 1);

			uint32_t i = 0;
			q.each([&](ecs::Entity entity) {
				REQUIRE(entity == rabbit);
				++i;
			});
			REQUIRE(i == cnt);
		}

		{
			auto q = wld.query().add({ecs::QueryOp::All, ecs::QueryAccess::None, ecs::Pair(eats, rabbit)});
			// auto q = wld.query().all<ecs::Pair(eats, rabbit)>();
			const auto cnt = q.count();
			REQUIRE(cnt == 1);

			uint32_t i = 0;
			q.each([&](ecs::Entity entity) {
				REQUIRE(entity == wolf);
				++i;
			});
			REQUIRE(i == cnt);
		}
	}

	SECTION("Intermediate") {
		auto wolf = wld.add();
		auto hare = wld.add();
		auto rabbit = wld.add();
		auto carrot = wld.add();
		auto eats = wld.add();

		wld.add(rabbit, {eats, carrot});
		wld.add(hare, {eats, carrot});
		wld.add(wolf, {eats, rabbit});

		{
			auto q = wld.query().add({ecs::QueryOp::All, ecs::QueryAccess::None, ecs::Pair(eats, carrot)});
			const auto cnt = q.count();
			REQUIRE(cnt == 2);

			uint32_t i = 0;
			q.each([&](ecs::Entity entity) {
				const bool isRabbit = entity == rabbit;
				const bool isHare = entity == hare;
				const bool is = isRabbit || isHare;
				REQUIRE(is);
				++i;
			});
			REQUIRE(i == cnt);
		}

		{
			auto q = wld.query().add({ecs::QueryOp::All, ecs::QueryAccess::None, ecs::Pair(eats, rabbit)});
			const auto cnt = q.count();
			REQUIRE(cnt == 1);

			uint32_t i = 0;
			q.each([&](ecs::Entity entity) {
				REQUIRE(entity == wolf);
				++i;
			});
			REQUIRE(i == cnt);
		}

		{
			auto q = wld.query().add({ecs::QueryOp::All, ecs::QueryAccess::None, ecs::Pair(eats, ecs::All)});
			const auto cnt = q.count();
			REQUIRE(cnt == 3);

			uint32_t i = 0;
			q.each([&](ecs::Entity entity) {
				const bool isRabbit = entity == rabbit;
				const bool isHare = entity == hare;
				const bool isWolf = entity == wolf;
				const bool is = isRabbit || isHare || isWolf;
				REQUIRE(is);
				++i;
			});
			REQUIRE(i == cnt);
		}

		{
			auto q = wld.query().add({ecs::QueryOp::All, ecs::QueryAccess::None, ecs::Pair(ecs::All, carrot)});
			const auto cnt = q.count();
			REQUIRE(cnt == 2);

			uint32_t i = 0;
			q.each([&](ecs::Entity entity) {
				const bool isRabbit = entity == rabbit;
				const bool isHare = entity == hare;
				const bool is = isRabbit || isHare;
				REQUIRE(is);
				++i;
			});
			REQUIRE(i == cnt);
		}

		{
			auto q = wld.query()
									 .add({ecs::QueryOp::All, ecs::QueryAccess::None, ecs::Pair(ecs::All, ecs::All)})
									 .no<ecs::Core_>()
									 .no<ecs::System2_>();
			const auto cnt = q.count();
			REQUIRE(cnt == 5); // 3 +2 for internal relationships

			uint32_t i = 0;
			q.each([&](ecs::Entity entity) {
				if (entity <= ecs::GAIA_ID(LastCoreComponent))
					return;
				const bool isRabbit = entity == rabbit;
				const bool isHare = entity == hare;
				const bool isWolf = entity == wolf;
				const bool is = isRabbit || isHare || isWolf;
				REQUIRE(is);
				++i;
			});
			REQUIRE(i == 3);
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
	REQUIRE(ret_e);

	cnt::sarr_ext<ecs::Entity, 32> out;
	wld.targets(rabbit, eats, [&out](ecs::Entity target) {
		out.push_back(target);
	});
	REQUIRE(out.size() == 2);
	REQUIRE(core::has(out, carrot));
	REQUIRE(core::has(out, salad));
	REQUIRE(wld.target(rabbit, eats) == carrot);

	out.clear();
	wld.relations(rabbit, salad, [&out](ecs::Entity relation) {
		out.push_back(relation);
	});
	REQUIRE(out.size() == 1);
	REQUIRE(core::has(out, eats));
	REQUIRE(wld.relation(rabbit, salad) == eats);
}

template <typename TQuery>
void Test_Query_Equality() {
	constexpr bool UseCachedQuery = std::is_same_v<TQuery, ecs::Query>;
	constexpr uint32_t N = 100;

	auto verify = [&](TQuery& q1, TQuery& q2, TQuery& q3, TQuery& q4) {
		REQUIRE(q1.count() == q2.count());
		REQUIRE(q1.count() == q3.count());
		REQUIRE(q1.count() == q4.count());

		cnt::darr<ecs::Entity> ents1, ents2, ents3, ents4;
		q1.arr(ents1);
		q2.arr(ents2);
		q3.arr(ents3);
		q4.arr(ents4);
		REQUIRE(ents1.size() == ents2.size());
		REQUIRE(ents1.size() == ents3.size());
		REQUIRE(ents1.size() == ents4.size());

		uint32_t i = 0;
		for (auto e: ents1) {
			REQUIRE(e == ents2[i]);
			++i;
		}
		i = 0;
		for (auto e: ents1) {
			REQUIRE(e == ents3[i]);
			++i;
		}
		i = 0;
		for (auto e: ents1) {
			REQUIRE(e == ents4[i]);
			++i;
		}
	};

	SECTION("2 components") {
		TestWorld twld;
		GAIA_FOR(N) {
			auto e = wld.add();
			wld.add<Position>(e);
			wld.add<Rotation>(e);
		}

		auto p = wld.add<Position>().entity;
		auto r = wld.add<Rotation>().entity;

		auto qq1 = wld.query<UseCachedQuery>().template all<Position, Rotation>();
		auto qq2 = wld.query<UseCachedQuery>().template all<Rotation, Position>();
		auto qq3 = wld.query<UseCachedQuery>().all(p).all(r);
		auto qq4 = wld.query<UseCachedQuery>().all(r).all(p);
		verify(qq1, qq2, qq3, qq4);

		auto qq1_ = wld.query<UseCachedQuery>().add("Position; Rotation");
		auto qq2_ = wld.query<UseCachedQuery>().add("Rotation; Position");
		auto qq3_ = wld.query<UseCachedQuery>().add("Position").add("Rotation");
		auto qq4_ = wld.query<UseCachedQuery>().add("Rotation").add("Position");
		verify(qq1_, qq2_, qq3_, qq4_);
	}
	SECTION("4 components") {
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

		auto qq1 = wld.query<UseCachedQuery>().template all<Position, Rotation, Acceleration, Something>();
		auto qq2 = wld.query<UseCachedQuery>().template all<Rotation, Something, Position, Acceleration>();
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
	SECTION("Cached query") {
		Test_Query_Equality<ecs::Query>();
	}
	SECTION("Non-cached query") {
		Test_Query_Equality<ecs::QueryUncached>();
	}
}

TEST_CASE("Enable") {
	// 1,500 picked so we create enough entites that they overflow into another chunk
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

	SECTION("State validity") {
		wld.enable(arr[0], false);
		REQUIRE_FALSE(wld.enabled(arr[0]));

		wld.enable(arr[0], true);
		REQUIRE(wld.enabled(arr[0]));

		wld.enable(arr[1], false);
		REQUIRE(wld.enabled(arr[0]));
		REQUIRE_FALSE(wld.enabled(arr[1]));

		wld.enable(arr[1], true);
		REQUIRE(wld.enabled(arr[0]));
		REQUIRE(wld.enabled(arr[1]));
	}

	SECTION("State persistance") {
		wld.enable(arr[0], false);
		REQUIRE_FALSE(wld.enabled(arr[0]));
		auto e = arr[0];
		wld.del<Position>(e);
		REQUIRE_FALSE(wld.enabled(e));

		wld.enable(arr[0], true);
		wld.add<Position>(arr[0]);
		REQUIRE(wld.enabled(arr[0]));
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
					REQUIRE(c == cExpected);
					cnt += c;
				});
				REQUIRE(cnt == expectedCountAll);

				cnt = q.count(ecs::Constraints::AcceptAll);
				REQUIRE(cnt == expectedCountAll);
			}
			{
				uint32_t cnt = 0;
				q.each([&]([[maybe_unused]] ecs::Iter& it) {
					const uint32_t cExpected = it.size();
					uint32_t c = 0;
					GAIA_EACH(it) {
						REQUIRE(it.enabled(i));
						++c;
					}
					REQUIRE(c == cExpected);
					cnt += c;
				});
				REQUIRE(cnt == expectedCountEnabled);

				cnt = q.count();
				REQUIRE(cnt == expectedCountEnabled);
			}
			{
				uint32_t cnt = 0;
				q.each([&]([[maybe_unused]] ecs::IterDisabled& it) {
					const uint32_t cExpected = it.size();
					uint32_t c = 0;
					GAIA_EACH(it) {
						REQUIRE(!it.enabled(i));
						++c;
					}
					REQUIRE(c == cExpected);
					cnt += c;
				});
				REQUIRE(cnt == expectedCountDisabled);

				cnt = q.count(ecs::Constraints::DisabledOnly);
				REQUIRE(cnt == expectedCountDisabled);
			}
		};

		checkQuery(N, N, 0);

		SECTION("Disable vs query") {
			wld.enable(arr[1000], false);
			checkQuery(N, N - 1, 1);
		}

		SECTION("Enable vs query") {
			wld.enable(arr[1000], true);
			checkQuery(N, N, 0);
		}

		SECTION("Disable vs query") {
			wld.enable(arr[1], false);
			wld.enable(arr[100], false);
			wld.enable(arr[999], false);
			wld.enable(arr[1400], false);
			checkQuery(N, N - 4, 4);
		}

		SECTION("Enable vs query") {
			wld.enable(arr[1], true);
			wld.enable(arr[100], true);
			wld.enable(arr[999], true);
			wld.enable(arr[1400], true);
			checkQuery(N, N, 0);
		}
	}

	SECTION("AoS") {
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
			REQUIRE(p0.x == vals[0]);
			REQUIRE(p0.y == vals[1]);
			REQUIRE(p0.z == vals[2]);
			auto p1 = wld.get<Position>(e1);
			REQUIRE(p1.x == vals[0] * 10.f);
			REQUIRE(p1.y == vals[1] * 10.f);
			REQUIRE(p1.z == vals[2] * 10.f);
			auto p2 = wld.get<Position>(e2);
			REQUIRE(p2.x == vals[0] * 100.f);
			REQUIRE(p2.y == vals[1] * 100.f);
			REQUIRE(p2.z == vals[2] * 100.f);

			ecs::Query q = wld.query().all<Position>();
			uint32_t cnt = 0;
			q.each([&](const Position& pos) {
				if (cnt == 0) {
					REQUIRE(pos.x == vals[0]);
					REQUIRE(pos.y == vals[1]);
					REQUIRE(pos.z == vals[2]);
				} else if (cnt == 1) {
					REQUIRE(pos.x == vals[0] * 10.f);
					REQUIRE(pos.y == vals[1] * 10.f);
					REQUIRE(pos.z == vals[2] * 10.f);
				} else if (cnt == 2) {
					REQUIRE(pos.x == vals[0] * 100.f);
					REQUIRE(pos.y == vals[1] * 100.f);
					REQUIRE(pos.z == vals[2] * 100.f);
				}
				++cnt;
			});
			REQUIRE(cnt == 3);
		}

		wld.enable(e2, false);

		{
			auto p0 = wld.get<Position>(e0);
			REQUIRE(p0.x == vals[0]);
			REQUIRE(p0.y == vals[1]);
			REQUIRE(p0.z == vals[2]);
			auto p1 = wld.get<Position>(e1);
			REQUIRE(p1.x == vals[0] * 10.f);
			REQUIRE(p1.y == vals[1] * 10.f);
			REQUIRE(p1.z == vals[2] * 10.f);
			auto p2 = wld.get<Position>(e2);
			REQUIRE(p2.x == vals[0] * 100.f);
			REQUIRE(p2.y == vals[1] * 100.f);
			REQUIRE(p2.z == vals[2] * 100.f);

			ecs::Query q = wld.query().all<Position>();
			uint32_t cnt = 0;
			q.each([&](const Position& pos) {
				if (cnt == 0) {
					REQUIRE(pos.x == vals[0] * 10.f);
					REQUIRE(pos.y == vals[1] * 10.f);
					REQUIRE(pos.z == vals[2] * 10.f);
				} else if (cnt == 1) {
					REQUIRE(pos.x == vals[0]);
					REQUIRE(pos.y == vals[1]);
					REQUIRE(pos.z == vals[2]);
				}
				++cnt;
			});
			REQUIRE(cnt == 2);
		}
	}

	SECTION("SoA") {
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
			REQUIRE(p0.x == vals[0]);
			REQUIRE(p0.y == vals[1]);
			REQUIRE(p0.z == vals[2]);
			auto p1 = wld.get<PositionSoA>(e1);
			REQUIRE(p1.x == vals[0] * 10.f);
			REQUIRE(p1.y == vals[1] * 10.f);
			REQUIRE(p1.z == vals[2] * 10.f);
			auto p2 = wld.get<PositionSoA>(e2);
			REQUIRE(p2.x == vals[0] * 100.f);
			REQUIRE(p2.y == vals[1] * 100.f);
			REQUIRE(p2.z == vals[2] * 100.f);

			ecs::Query q = wld.query().all<PositionSoA>();
			uint32_t cnt = 0;
			q.each([&](const PositionSoA& pos) {
				if (cnt == 0) {
					REQUIRE(pos.x == vals[0]);
					REQUIRE(pos.y == vals[1]);
					REQUIRE(pos.z == vals[2]);
				} else if (cnt == 1) {
					REQUIRE(pos.x == vals[0] * 10.f);
					REQUIRE(pos.y == vals[1] * 10.f);
					REQUIRE(pos.z == vals[2] * 10.f);
				} else if (cnt == 2) {
					REQUIRE(pos.x == vals[0] * 100.f);
					REQUIRE(pos.y == vals[1] * 100.f);
					REQUIRE(pos.z == vals[2] * 100.f);
				}
				++cnt;
			});
			REQUIRE(cnt == 3);
		}

		wld.enable(e2, false);

		{
			auto p0 = wld.get<PositionSoA>(e0);
			REQUIRE(p0.x == vals[0]);
			REQUIRE(p0.y == vals[1]);
			REQUIRE(p0.z == vals[2]);
			auto p1 = wld.get<PositionSoA>(e1);
			REQUIRE(p1.x == vals[0] * 10.f);
			REQUIRE(p1.y == vals[1] * 10.f);
			REQUIRE(p1.z == vals[2] * 10.f);
			auto p2 = wld.get<PositionSoA>(e2);
			REQUIRE(p2.x == vals[0] * 100.f);
			REQUIRE(p2.y == vals[1] * 100.f);
			REQUIRE(p2.z == vals[2] * 100.f);

			ecs::Query q = wld.query().all<PositionSoA>();
			uint32_t cnt = 0;
			q.each([&](const PositionSoA& pos) {
				if (cnt == 0) {
					REQUIRE(pos.x == vals[0] * 10.f);
					REQUIRE(pos.y == vals[1] * 10.f);
					REQUIRE(pos.z == vals[2] * 10.f);
				} else if (cnt == 1) {
					REQUIRE(pos.x == vals[0]);
					REQUIRE(pos.y == vals[1]);
					REQUIRE(pos.z == vals[2]);
				}
				++cnt;
			});
			REQUIRE(cnt == 2);
		}
	}

	SECTION("AoS + SoA") {
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
			REQUIRE(p0.x == vals[0]);
			REQUIRE(p0.y == vals[1]);
			REQUIRE(p0.z == vals[2]);
			auto p1 = wld.get<PositionSoA>(e1);
			REQUIRE(p1.x == vals[0] * 10.f);
			REQUIRE(p1.y == vals[1] * 10.f);
			REQUIRE(p1.z == vals[2] * 10.f);
			auto p2 = wld.get<PositionSoA>(e2);
			REQUIRE(p2.x == vals[0] * 100.f);
			REQUIRE(p2.y == vals[1] * 100.f);
			REQUIRE(p2.z == vals[2] * 100.f);

			ecs::Query q = wld.query().all<PositionSoA>();
			uint32_t cnt = 0;
			q.each([&](const PositionSoA& pos) {
				if (cnt == 0) {
					REQUIRE(pos.x == vals[0]);
					REQUIRE(pos.y == vals[1]);
					REQUIRE(pos.z == vals[2]);
				} else if (cnt == 1) {
					REQUIRE(pos.x == vals[0] * 10.f);
					REQUIRE(pos.y == vals[1] * 10.f);
					REQUIRE(pos.z == vals[2] * 10.f);
				} else if (cnt == 2) {
					REQUIRE(pos.x == vals[0] * 100.f);
					REQUIRE(pos.y == vals[1] * 100.f);
					REQUIRE(pos.z == vals[2] * 100.f);
				}
				++cnt;
			});
			REQUIRE(cnt == 3);
		}
		{
			auto p0 = wld.get<Position>(e0);
			REQUIRE(p0.x == vals[0]);
			REQUIRE(p0.y == vals[1]);
			REQUIRE(p0.z == vals[2]);
			auto p1 = wld.get<Position>(e1);
			REQUIRE(p1.x == vals[0] * 10.f);
			REQUIRE(p1.y == vals[1] * 10.f);
			REQUIRE(p1.z == vals[2] * 10.f);
			auto p2 = wld.get<Position>(e2);
			REQUIRE(p2.x == vals[0] * 100.f);
			REQUIRE(p2.y == vals[1] * 100.f);
			REQUIRE(p2.z == vals[2] * 100.f);

			ecs::Query q = wld.query().all<Position>();
			uint32_t cnt = 0;
			q.each([&](const Position& pos) {
				if (cnt == 0) {
					REQUIRE(pos.x == vals[0]);
					REQUIRE(pos.y == vals[1]);
					REQUIRE(pos.z == vals[2]);
				} else if (cnt == 1) {
					REQUIRE(pos.x == vals[0] * 10.f);
					REQUIRE(pos.y == vals[1] * 10.f);
					REQUIRE(pos.z == vals[2] * 10.f);
				} else if (cnt == 2) {
					REQUIRE(pos.x == vals[0] * 100.f);
					REQUIRE(pos.y == vals[1] * 100.f);
					REQUIRE(pos.z == vals[2] * 100.f);
				}
				++cnt;
			});
			REQUIRE(cnt == 3);
		}

		wld.enable(e2, false);

		{
			auto p0 = wld.get<PositionSoA>(e0);
			REQUIRE(p0.x == vals[0]);
			REQUIRE(p0.y == vals[1]);
			REQUIRE(p0.z == vals[2]);
			auto p1 = wld.get<PositionSoA>(e1);
			REQUIRE(p1.x == vals[0] * 10.f);
			REQUIRE(p1.y == vals[1] * 10.f);
			REQUIRE(p1.z == vals[2] * 10.f);
			auto p2 = wld.get<PositionSoA>(e2);
			REQUIRE(p2.x == vals[0] * 100.f);
			REQUIRE(p2.y == vals[1] * 100.f);
			REQUIRE(p2.z == vals[2] * 100.f);

			ecs::Query q = wld.query().all<PositionSoA>();
			uint32_t cnt = 0;
			q.each([&](const PositionSoA& pos) {
				if (cnt == 0) {
					REQUIRE(pos.x == vals[0] * 10.f);
					REQUIRE(pos.y == vals[1] * 10.f);
					REQUIRE(pos.z == vals[2] * 10.f);
				} else if (cnt == 1) {
					REQUIRE(pos.x == vals[0]);
					REQUIRE(pos.y == vals[1]);
					REQUIRE(pos.z == vals[2]);
				}
				++cnt;
			});
			REQUIRE(cnt == 2);
		}
		{
			wld.enable(e2, false);

			auto p0 = wld.get<Position>(e0);
			REQUIRE(p0.x == vals[0]);
			REQUIRE(p0.y == vals[1]);
			REQUIRE(p0.z == vals[2]);
			auto p1 = wld.get<Position>(e1);
			REQUIRE(p1.x == vals[0] * 10.f);
			REQUIRE(p1.y == vals[1] * 10.f);
			REQUIRE(p1.z == vals[2] * 10.f);
			auto p2 = wld.get<Position>(e2);
			REQUIRE(p2.x == vals[0] * 100.f);
			REQUIRE(p2.y == vals[1] * 100.f);
			REQUIRE(p2.z == vals[2] * 100.f);

			ecs::Query q = wld.query().all<Position>();
			uint32_t cnt = 0;
			q.each([&](const Position& pos) {
				if (cnt == 0) {
					REQUIRE(pos.x == vals[0] * 10.f);
					REQUIRE(pos.y == vals[1] * 10.f);
					REQUIRE(pos.z == vals[2] * 10.f);
				} else if (cnt == 1) {
					REQUIRE(pos.x == vals[0]);
					REQUIRE(pos.y == vals[1]);
					REQUIRE(pos.z == vals[2]);
				}
				++cnt;
			});
			REQUIRE(cnt == 2);
		}
	}
}

TEST_CASE("Add - generic") {
	{
		TestWorld twld;
		auto e = wld.add();

		auto f = wld.add();
		wld.add(e, f);
		REQUIRE(wld.has(e, f));
	}

	{
		TestWorld twld;
		auto e = wld.add();

		wld.add<Position>(e);
		wld.add<Acceleration>(e);

		REQUIRE(wld.has<Position>(e));
		REQUIRE(wld.has<Acceleration>(e));
		REQUIRE_FALSE(wld.has<ecs::uni<Position>>(e));
		REQUIRE_FALSE(wld.has<ecs::uni<Acceleration>>(e));

		auto f = wld.add();
		wld.add(e, f);
		REQUIRE(wld.has(e, f));

		REQUIRE(wld.has<Position>(e));
		REQUIRE(wld.has<Acceleration>(e));
		REQUIRE_FALSE(wld.has<ecs::uni<Position>>(e));
		REQUIRE_FALSE(wld.has<ecs::uni<Acceleration>>(e));
	}

	{
		TestWorld twld;
		auto e = wld.add();

		wld.add<Position>(e, {1, 2, 3});
		wld.add<Acceleration>(e, {4, 5, 6});

		REQUIRE(wld.has<Position>(e));
		REQUIRE(wld.has<Acceleration>(e));
		REQUIRE_FALSE(wld.has<ecs::uni<Position>>(e));
		REQUIRE_FALSE(wld.has<ecs::uni<Acceleration>>(e));

		auto p = wld.get<Position>(e);
		REQUIRE(p.x == 1.f);
		REQUIRE(p.y == 2.f);
		REQUIRE(p.z == 3.f);

		auto a = wld.get<Acceleration>(e);
		REQUIRE(a.x == 4.f);
		REQUIRE(a.y == 5.f);
		REQUIRE(a.z == 6.f);

		auto f = wld.add();
		wld.add(e, f);
		REQUIRE(wld.has(e, f));

		REQUIRE(wld.has<Position>(e));
		REQUIRE(wld.has<Acceleration>(e));
		REQUIRE_FALSE(wld.has<ecs::uni<Position>>(e));
		REQUIRE_FALSE(wld.has<ecs::uni<Acceleration>>(e));

		p = wld.get<Position>(e);
		REQUIRE(p.x == 1.f);
		REQUIRE(p.y == 2.f);
		REQUIRE(p.z == 3.f);

		a = wld.get<Acceleration>(e);
		REQUIRE(a.x == 4.f);
		REQUIRE(a.y == 5.f);
		REQUIRE(a.z == 6.f);
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
// 		REQUIRE(wld.has<Position>(ents[i]));
// 		REQUIRE(wld.has<Acceleration>(ents[i]));
//
// 		Position p;
// 		wld.get<Position>(ents[i], p);
// 		REQUIRE(p.x == (float)i);
// 		REQUIRE(p.y == (float)i);
// 		REQUIRE(p.z == (float)i);
//
// 		Acceleration a;
// 		wld.get<Acceleration>(ents[i], a);
// 		REQUIRE(a.x == 1.f);
// 		REQUIRE(a.y == 2.f);
// 		REQUIRE(a.z == 3.f);
// 	}
//
// 	{
// 		REQUIRE_FALSE(wld.has<Position>(ents[4]));
// 		REQUIRE_FALSE(wld.has<Acceleration>(ents[4]));
// 	}
// }

TEST_CASE("Add - unique") {
	{
		TestWorld twld;
		auto e = wld.add();

		auto f = wld.add(ecs::EntityKind::EK_Uni);
		wld.add(e, f);
		REQUIRE(wld.has(e, f));
	}

	{
		TestWorld twld;
		auto e = wld.add();

		wld.add<ecs::uni<Position>>(e);
		wld.add<ecs::uni<Acceleration>>(e);

		REQUIRE_FALSE(wld.has<Position>(e));
		REQUIRE_FALSE(wld.has<Acceleration>(e));
		REQUIRE(wld.has<ecs::uni<Position>>(e));
		REQUIRE(wld.has<ecs::uni<Acceleration>>(e));
	}

	{
		TestWorld twld;
		auto e = wld.add();

		// Add Position unique component
		wld.add<ecs::uni<Position>>(e, {1, 2, 3});
		REQUIRE(wld.has<ecs::uni<Position>>(e));
		REQUIRE_FALSE(wld.has<Position>(e));
		{
			auto p = wld.get<ecs::uni<Position>>(e);
			REQUIRE(p.x == 1.f);
			REQUIRE(p.y == 2.f);
			REQUIRE(p.z == 3.f);
		}
		{
			auto setter = wld.acc_mut(e);
			auto& upos = setter.mut<ecs::uni<Position>>();
			upos = {10, 20, 30};

			auto p = wld.get<ecs::uni<Position>>(e);
			REQUIRE(p.x == 10.f);
			REQUIRE(p.y == 20.f);
			REQUIRE(p.z == 30.f);

			p = setter.get<ecs::uni<Position>>();
			REQUIRE(p.x == 10.f);
			REQUIRE(p.y == 20.f);
			REQUIRE(p.z == 30.f);
		}
		// Add Acceleration unique component.
		// This moves "e" to a new archetype.
		wld.add<ecs::uni<Acceleration>>(e, {4, 5, 6});
		REQUIRE(wld.has<ecs::uni<Position>>(e));
		REQUIRE(wld.has<ecs::uni<Acceleration>>(e));
		REQUIRE_FALSE(wld.has<Position>(e));
		REQUIRE_FALSE(wld.has<Acceleration>(e));
		{
			auto a = wld.get<ecs::uni<Acceleration>>(e);
			REQUIRE(a.x == 4.f);
			REQUIRE(a.y == 5.f);
			REQUIRE(a.z == 6.f);
		}
		{
			// Because "e" was moved to a new archetype nobody ever set the value.
			// Therefore, it is garbage and won't match the original chunk.
			auto p = wld.get<ecs::uni<Position>>(e);
			REQUIRE_FALSE(p.x == 1.f);
			REQUIRE_FALSE(p.y == 2.f);
			REQUIRE_FALSE(p.z == 3.f);
		}
	}

	{
		TestWorld twld;
		auto e = wld.add();

		// Add Position unique component
		wld.add<ecs::uni<Position>>(e, {1, 2, 3});
		REQUIRE(wld.has<ecs::uni<Position>>(e));
		REQUIRE_FALSE(wld.has<Position>(e));
		{
			auto p = wld.get<ecs::uni<Position>>(e);
			REQUIRE(p.x == 1.f);
			REQUIRE(p.y == 2.f);
			REQUIRE(p.z == 3.f);
		}
		// Add Acceleration unique component.
		// This moves "e" to a new archetype.
		wld.add<ecs::uni<Acceleration>>(e, {4, 5, 6});
		REQUIRE(wld.has<ecs::uni<Position>>(e));
		REQUIRE(wld.has<ecs::uni<Acceleration>>(e));
		REQUIRE_FALSE(wld.has<Position>(e));
		REQUIRE_FALSE(wld.has<Acceleration>(e));
		{
			auto a = wld.get<ecs::uni<Acceleration>>(e);
			REQUIRE(a.x == 4.f);
			REQUIRE(a.y == 5.f);
			REQUIRE(a.z == 6.f);
		}
		{
			// Because "e" was moved to a new archetype nobody ever set the value.
			// Therefore, it is garbage and won't match the original chunk.
			auto p = wld.get<ecs::uni<Position>>(e);
			REQUIRE_FALSE(p.x == 1.f);
			REQUIRE_FALSE(p.y == 2.f);
			REQUIRE_FALSE(p.z == 3.f);
		}

		// Add a generic entity. Archetype changes.
		auto f = wld.add();
		wld.add(e, f);
		REQUIRE(wld.has(e, f));

		REQUIRE(wld.has<ecs::uni<Position>>(e));
		REQUIRE(wld.has<ecs::uni<Acceleration>>(e));
		REQUIRE_FALSE(wld.has<Position>(e));
		REQUIRE_FALSE(wld.has<Acceleration>(e));
		{
			auto a = wld.get<ecs::uni<Acceleration>>(e);
			REQUIRE_FALSE(a.x == 4.f);
			REQUIRE_FALSE(a.y == 5.f);
			REQUIRE_FALSE(a.z == 6.f);
		}
		{
			// Because "e" was moved to a new archetype nobody ever set the value.
			// Therefore, it is garbage and won't match the original chunk.
			auto p = wld.get<ecs::uni<Position>>(e);
			REQUIRE_FALSE(p.x == 1.f);
			REQUIRE_FALSE(p.y == 2.f);
			REQUIRE_FALSE(p.z == 3.f);
		}
	}

	{
		TestWorld twld;
		auto e = wld.add();

		// Add Position unique component
		wld.add<ecs::uni<Position>>(e, {1, 2, 3});
		REQUIRE(wld.has<ecs::uni<Position>>(e));
		REQUIRE_FALSE(wld.has<Position>(e));
		{
			auto p = wld.get<ecs::uni<Position>>(e);
			REQUIRE(p.x == 1.f);
			REQUIRE(p.y == 2.f);
			REQUIRE(p.z == 3.f);
		}
		// Add Acceleration unique component.
		// This moves "e" to a new archetype.
		wld.add<ecs::uni<Acceleration>>(e, {4, 5, 6});
		REQUIRE(wld.has<ecs::uni<Position>>(e));
		REQUIRE(wld.has<ecs::uni<Acceleration>>(e));
		REQUIRE_FALSE(wld.has<Position>(e));
		REQUIRE_FALSE(wld.has<Acceleration>(e));
		{
			auto a = wld.get<ecs::uni<Acceleration>>(e);
			REQUIRE(a.x == 4.f);
			REQUIRE(a.y == 5.f);
			REQUIRE(a.z == 6.f);
		}
		{
			// Because "e" was moved to a new archetype nobody ever set the value.
			// Therefore, it is garbage and won't match the original chunk.
			auto p = wld.get<ecs::uni<Position>>(e);
			REQUIRE_FALSE(p.x == 1.f);
			REQUIRE_FALSE(p.y == 2.f);
			REQUIRE_FALSE(p.z == 3.f);
		}

		// Add a unique entity. Archetype changes.
		auto f = wld.add(ecs::EntityKind::EK_Uni);
		wld.add(e, f);
		REQUIRE(wld.has(e, f));

		REQUIRE(wld.has<ecs::uni<Position>>(e));
		REQUIRE(wld.has<ecs::uni<Acceleration>>(e));
		REQUIRE_FALSE(wld.has<Position>(e));
		REQUIRE_FALSE(wld.has<Acceleration>(e));
		{
			// Because "e" was moved to a new archetype nobody ever set the value.
			// Therefore, it is garbage and won't match the original chunk.
			auto a = wld.get<ecs::uni<Acceleration>>(e);
			REQUIRE_FALSE(a.x == 4.f);
			REQUIRE_FALSE(a.y == 5.f);
			REQUIRE_FALSE(a.z == 6.f);
		}
		{
			// Because "e" was moved to a new archetype nobody ever set the value.
			// Therefore, it is garbage and won't match the original chunk.
			auto p = wld.get<ecs::uni<Position>>(e);
			REQUIRE_FALSE(p.x == 1.f);
			REQUIRE_FALSE(p.y == 2.f);
			REQUIRE_FALSE(p.z == 3.f);
		}
	}
}

TEST_CASE("Add - mixed") {
	{
		TestWorld twld;
		auto e = wld.add();

		auto f = wld.add(ecs::EntityKind::EK_Uni);
		wld.add(e, f);
		REQUIRE(wld.has(e, f));
	}

	{
		TestWorld twld;
		auto e = wld.add();

		wld.add<Position>(e);
		wld.add<ecs::uni<Position>>(e);

		REQUIRE(wld.has<Position>(e));
		REQUIRE(wld.has<ecs::uni<Position>>(e));
	}

	{
		TestWorld twld;
		auto e = wld.add();

		// Add Position unique component
		wld.add<Position>(e, {10, 20, 30});
		wld.add<ecs::uni<Position>>(e, {1, 2, 3});
		REQUIRE(wld.has<Position>(e));
		REQUIRE(wld.has<ecs::uni<Position>>(e));
		{
			auto p = wld.get<Position>(e);
			REQUIRE(p.x == 10.f);
			REQUIRE(p.y == 20.f);
			REQUIRE(p.z == 30.f);
		}
		{
			auto p = wld.get<ecs::uni<Position>>(e);
			REQUIRE(p.x == 1.f);
			REQUIRE(p.y == 2.f);
			REQUIRE(p.z == 3.f);
		}
		// Add Acceleration unique component.
		// This moves "e" to a new archetype.
		wld.add<ecs::uni<Acceleration>>(e, {4, 5, 6});
		REQUIRE(wld.has<Position>(e));
		REQUIRE(wld.has<ecs::uni<Position>>(e));
		REQUIRE(wld.has<ecs::uni<Acceleration>>(e));
		REQUIRE_FALSE(wld.has<Acceleration>(e));
		{
			auto a = wld.get<ecs::uni<Acceleration>>(e);
			REQUIRE(a.x == 4.f);
			REQUIRE(a.y == 5.f);
			REQUIRE(a.z == 6.f);
		}
		{
			// Position will remain the same
			auto p = wld.get<Position>(e);
			REQUIRE(p.x == 10.f);
			REQUIRE(p.y == 20.f);
			REQUIRE(p.z == 30.f);
		}
		{
			// Because "e" was moved to a new archetype nobody ever set the value.
			// Therefore, it is garbage and won't match the original chunk.
			auto p = wld.get<ecs::uni<Position>>(e);
			REQUIRE_FALSE(p.x == 1.f);
			REQUIRE_FALSE(p.y == 2.f);
			REQUIRE_FALSE(p.z == 3.f);
		}
		wld.set<ecs::uni<Position>>(e) = {100.0f, 200.0f, 300.0f};
		{
			auto p = wld.get<Position>(e);
			REQUIRE(p.x == 10.f);
			REQUIRE(p.y == 20.f);
			REQUIRE(p.z == 30.f);
		}
		{
			auto p = wld.get<ecs::uni<Position>>(e);
			REQUIRE(p.x == 100.f);
			REQUIRE(p.y == 200.f);
			REQUIRE(p.z == 300.f);
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
			REQUIRE_FALSE(wld.has<Position>(e1));
		}
		{
			wld.add<Rotation>(e1);
			wld.del<Rotation>(e1);
			REQUIRE_FALSE(wld.has<Rotation>(e1));
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
				REQUIRE_FALSE(wld.has<Position>(e1));
				REQUIRE(wld.has<Rotation>(e1));
			}
			{
				wld.del<Rotation>(e1);
				REQUIRE_FALSE(wld.has<Position>(e1));
				REQUIRE_FALSE(wld.has<Rotation>(e1));
			}
		}
		{
			wld.add<Rotation>(e1);
			wld.add<Position>(e1);
			{
				wld.del<Position>(e1);
				REQUIRE_FALSE(wld.has<Position>(e1));
				REQUIRE(wld.has<Rotation>(e1));
			}
			{
				wld.del<Rotation>(e1);
				REQUIRE_FALSE(wld.has<Position>(e1));
				REQUIRE_FALSE(wld.has<Rotation>(e1));
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
			REQUIRE_FALSE(wld.has<ecs::uni<Position>>(e1));
			REQUIRE(wld.has<ecs::uni<Acceleration>>(e1));
		}
		{
			wld.del<ecs::uni<Acceleration>>(e1);
			REQUIRE_FALSE(wld.has<ecs::uni<Position>>(e1));
			REQUIRE_FALSE(wld.has<ecs::uni<Acceleration>>(e1));
		}
	}

	{
		wld.add<ecs::uni<Acceleration>>(e1);
		wld.add<ecs::uni<Position>>(e1);
		{
			wld.del<ecs::uni<Position>>(e1);
			REQUIRE_FALSE(wld.has<ecs::uni<Position>>(e1));
			REQUIRE(wld.has<ecs::uni<Acceleration>>(e1));
		}
		{
			wld.del<ecs::uni<Acceleration>>(e1);
			REQUIRE_FALSE(wld.has<ecs::uni<Position>>(e1));
			REQUIRE_FALSE(wld.has<ecs::uni<Acceleration>>(e1));
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
			REQUIRE_FALSE(wld.has<Position>(e1));
			REQUIRE(wld.has<Acceleration>(e1));
			REQUIRE(wld.has<ecs::uni<Position>>(e1));
			REQUIRE(wld.has<ecs::uni<Acceleration>>(e1));
		}
		{
			wld.del<Acceleration>(e1);
			REQUIRE_FALSE(wld.has<Position>(e1));
			REQUIRE_FALSE(wld.has<Acceleration>(e1));
			REQUIRE(wld.has<ecs::uni<Position>>(e1));
			REQUIRE(wld.has<ecs::uni<Acceleration>>(e1));
		}
		{
			wld.del<ecs::uni<Acceleration>>(e1);
			REQUIRE_FALSE(wld.has<Position>(e1));
			REQUIRE_FALSE(wld.has<Acceleration>(e1));
			REQUIRE(wld.has<ecs::uni<Position>>(e1));
			REQUIRE_FALSE(wld.has<ecs::uni<Acceleration>>(e1));
		}
	}
}

TEST_CASE("Del - cleanup rules") {
	SECTION("default") {
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
		REQUIRE(!wld.has(wolf));
		REQUIRE(wld.has(rabbit));
		REQUIRE(wld.has(eats));
		REQUIRE(wld.has(carrot));
		REQUIRE(wld.has(hungry));
		// global relationships
		REQUIRE(wld.has({eats, rabbit}));
		REQUIRE(wld.has({eats, carrot}));
		REQUIRE(wld.has({eats, ecs::All}));
		REQUIRE(wld.has({ecs::All, carrot}));
		REQUIRE(wld.has({ecs::All, rabbit}));
		REQUIRE(wld.has({ecs::All, ecs::All}));
		// rabbit relationships
		REQUIRE(wld.has(rabbit, {eats, carrot}));
		REQUIRE(wld.has(rabbit, {eats, ecs::All}));
		REQUIRE(wld.has(rabbit, {ecs::All, carrot}));
		REQUIRE(wld.has(rabbit, {ecs::All, ecs::All}));
	}
	SECTION("default, relationship source") {
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
		REQUIRE(wld.has(wolf));
		REQUIRE(wld.has(rabbit));
		REQUIRE(!wld.has(eats));
		REQUIRE(wld.has(carrot));
		REQUIRE(wld.has(hungry));
		REQUIRE(!wld.has(wolf, {eats, rabbit}));
		REQUIRE(!wld.has(rabbit, {eats, carrot}));
	}
	SECTION("(OnDelete,Remove)") {
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
		REQUIRE(!wld.has(wolf));
		REQUIRE(wld.has(rabbit));
		REQUIRE(wld.has(eats));
		REQUIRE(wld.has(carrot));
		REQUIRE(wld.has(hungry));
		REQUIRE(wld.has({eats, rabbit}));
		REQUIRE(wld.has({eats, carrot}));
		REQUIRE(wld.has(rabbit, {eats, carrot}));
	}
	SECTION("(OnDelete,Delete)") {
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
		REQUIRE(!wld.has(wolf));
		REQUIRE(wld.has(rabbit));
		REQUIRE(wld.has(eats));
		REQUIRE(wld.has(carrot));
		REQUIRE(!wld.has(hungry));
		REQUIRE(wld.has({eats, rabbit}));
		REQUIRE(wld.has({eats, carrot}));
	}
	SECTION("(OnDeleteTarget,Delete)") {
		TestWorld twld;
		auto parent = wld.add();
		auto child = wld.add();
		auto child_of = wld.add();
		wld.add(child_of, {ecs::OnDeleteTarget, ecs::Delete});
		wld.add(child, {child_of, parent});

		wld.del(parent);
		REQUIRE(!wld.has(child));
		REQUIRE(!wld.has(parent));
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
		REQUIRE(l0 == l1);
		REQUIRE(strcmp(tmp, ename) == 0);
	};

	SECTION("basic") {
		ents.clear();
		create();
		verify(0);
		auto e = ents[0];

		char original[MaxLen];
		GAIA_STRFMT(original, MaxLen, "%s", wld.name(e));

		// If we change the original string we still must have a match
		{
			GAIA_STRCPY(tmp, MaxLen, "some_random_string");
			REQUIRE(strcmp(wld.name(e), original) == 0);
			REQUIRE(wld.get(original) == e);
			REQUIRE(wld.get(tmp) == ecs::EntityBad);

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
			REQUIRE(wld.name(e1) == nullptr);
			REQUIRE(wld.get(original) == e);
		}
#endif

		wld.name(e, nullptr);
		REQUIRE(wld.get(original) == ecs::EntityBad);
		REQUIRE(wld.name(e) == nullptr);

		wld.name(e, original);
		wld.del(e);
		REQUIRE(wld.get(original) == ecs::EntityBad);
	}

	SECTION("basic - non-owned") {
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
			REQUIRE(strcmp(str, "some_random_string") == 0);
			REQUIRE(wld.get(original) == ecs::EntityBad);
			// Hash was calculated for [original] but we changed the string to "some_random_string".
			// Hash won't match so we shouldn't be able to find the entity still.
			REQUIRE(wld.get("some_random_string") == ecs::EntityBad);
		}

		{
			// Change the name back
			GAIA_STRCPY(tmp, MaxLen, original);
			verify(0);
		}

		wld.name(e, nullptr);
		REQUIRE(wld.get(original) == ecs::EntityBad);
		REQUIRE(wld.name(e) == nullptr);

		wld.name_raw(e, original);
		wld.del(e);
		REQUIRE(wld.get(original) == ecs::EntityBad);
	}

	SECTION("two") {
		ents.clear();
		GAIA_FOR(2) create();
		GAIA_FOR(2) verify(i);
		wld.del(ents[0]);
		verify(1);
	}

	SECTION("many") {
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
				REQUIRE(strcmp(str, original) == 0);
				REQUIRE(e == wld.get(original));
			}
			{
				wld.enable(e, true);
				const auto* str = wld.name(e);
				REQUIRE(strcmp(str, original) == 0);
				REQUIRE(e == wld.get(original));
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
		REQUIRE(strcmp(name, "Position") == 0);
		const auto e = wld.get("Position");
		REQUIRE(e == pci.entity);
	}
	// Add unique component
	const auto& upci = wld.add<ecs::uni<Position>>();
	{
		// name must match
		const char* name = wld.name(upci.entity);
		REQUIRE(strcmp(name, "gaia::ecs::uni<Position>") == 0);
		const auto e = wld.get("gaia::ecs::uni<Position>");
		REQUIRE(e == upci.entity);
	}
	{
		// generic component name must still match
		const char* name = wld.name(pci.entity);
		REQUIRE(strcmp(name, "Position") == 0);
		const auto e = wld.get("Position");
		REQUIRE(e == pci.entity);
	}

	// Change the component name
	wld.name(pci.entity, "xyz", 3);
	{
		// name must match
		const char* name = wld.name(pci.entity);
		REQUIRE(strcmp(name, "xyz") == 0);
		const auto e = wld.get("xyz");
		REQUIRE(e == pci.entity);
	}
	{
		// unique component name must still match
		const char* name = wld.name(upci.entity);
		REQUIRE(strcmp(name, "gaia::ecs::uni<Position>") == 0);
		const auto e = wld.get("gaia::ecs::uni<Position>");
		REQUIRE(e == upci.entity);
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
		REQUIRE(cnt == 0);
	}
	{
		uint32_t cnt = 0;
		qp.each([&]([[maybe_unused]] const Position&) {
			++cnt;
		});
		REQUIRE(cnt == 0);
	}

	auto e1 = wld.copy(e);
	auto e2 = wld.copy(e);
	auto e3 = wld.copy(e);

	{
		uint32_t cnt = 0;
		qp.each([&]([[maybe_unused]] const Position&) {
			++cnt;
		});
		REQUIRE(cnt == 0);
	}

	wld.del(e1);

	{
		uint32_t cnt = 0;
		qp.each([&]([[maybe_unused]] const Position&) {
			++cnt;
		});
		REQUIRE(cnt == 0);
	}

	wld.del(e2);
	wld.del(e3);
	wld.del(e);

	{
		uint32_t cnt = 0;
		qp.each([&]([[maybe_unused]] const Position&) {
			++cnt;
		});
		REQUIRE(cnt == 0);
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
		REQUIRE(cnt == 0);
	}
	{
		uint32_t cnt = 0;
		qp.each([&]([[maybe_unused]] const Position&) {
			++cnt;
		});
		REQUIRE(cnt == 1);
	}

	auto e1 = wld.copy(e);
	auto e2 = wld.copy(e);
	auto e3 = wld.copy(e);

	{
		uint32_t cnt = 0;
		qp.each([&]([[maybe_unused]] const Position&) {
			++cnt;
		});
		REQUIRE(cnt == 4);
	}

	wld.del(e1);

	{
		uint32_t cnt = 0;
		qp.each([&]([[maybe_unused]] const Position&) {
			++cnt;
		});
		REQUIRE(cnt == 3);
	}

	wld.del(e2);
	wld.del(e3);
	wld.del(e);

	{
		uint32_t cnt = 0;
		qp.each([&]([[maybe_unused]] const Position&) {
			++cnt;
		});
		REQUIRE(cnt == 0);
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
		REQUIRE(cnt == 0);
	}
	{
		uint32_t cnt = 0;
		qq.each([&]() {
			++cnt;
		});
		REQUIRE(cnt == 0);
	}
	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] ecs::Iter& it) {
			++cnt;
		});
		REQUIRE(cnt == 1);
	}

	auto e1 = wld.copy(e);
	auto e2 = wld.copy(e);
	auto e3 = wld.copy(e);

	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] ecs::Iter& it) {
			++cnt;
		});
		REQUIRE(cnt == 1);
	}

	wld.del(e1);

	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] ecs::Iter& it) {
			++cnt;
		});
		REQUIRE(cnt == 1);
	}

	wld.del(e2);
	wld.del(e3);
	wld.del(e);

	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] ecs::Iter& it) {
			++cnt;
		});
		REQUIRE(cnt == 0);
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
		REQUIRE(cnt == 2);
	}
	{
		uint32_t cnt = 0;
		auto q = wld.query().all<Acceleration>();
		q.each([&]([[maybe_unused]] const Acceleration&) {
			++cnt;
		});
		REQUIRE(cnt == 2);
	}
	{
		uint32_t cnt = 0;
		auto q = wld.query().all<Rotation>();
		q.each([&]([[maybe_unused]] const Rotation&) {
			++cnt;
		});
		REQUIRE(cnt == 1);
	}
	{
		uint32_t cnt = 0;
		auto q = wld.query().all<Scale>();
		q.each([&]([[maybe_unused]] const Scale&) {
			++cnt;
		});
		REQUIRE(cnt == 2);
	}
	{
		uint32_t cnt = 0;
		auto q = wld.query().all<Position, Acceleration>();
		q.each([&]([[maybe_unused]] const Position&, [[maybe_unused]] const Acceleration&) {
			++cnt;
		});
		REQUIRE(cnt == 2);
	}
	{
		uint32_t cnt = 0;
		auto q = wld.query().all<Position, Scale>();
		q.each([&]([[maybe_unused]] const Position&, [[maybe_unused]] const Scale&) {
			++cnt;
		});
		REQUIRE(cnt == 1);
	}
	{
		ecs::Query q = wld.query().any<Position, Acceleration>();

		uint32_t cnt = 0;
		q.each([&](ecs::Iter& it) {
			++cnt;

			const bool ok1 = it.has<Position>() || it.has<Acceleration>();
			REQUIRE(ok1);
			const bool ok2 = it.has<Acceleration>() || it.has<Position>();
			REQUIRE(ok2);
		});
		REQUIRE(cnt == 2);
	}
	{
		ecs::Query q = wld.query().any<Position, Acceleration>().all<Scale>();

		uint32_t cnt = 0;
		q.each([&](ecs::Iter& it) {
			++cnt;

			REQUIRE(it.size() == 1);

			const bool ok1 = it.has<Position>() || it.has<Acceleration>();
			REQUIRE(ok1);
			const bool ok2 = it.has<Acceleration>() || it.has<Position>();
			REQUIRE(ok2);
		});
		REQUIRE(cnt == 1);
	}
	{
		ecs::Query q = wld.query().any<Position, Acceleration>().no<Scale>();

		uint32_t cnt = 0;
		q.each([&](ecs::Iter& it) {
			++cnt;

			REQUIRE(it.size() == 1);
		});
		REQUIRE(cnt == 1);
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
		REQUIRE(cnt == 2);
	}
	{
		uint32_t cnt = 0;
		auto q = wld.query().all<ecs::uni<Acceleration>>();
		q.each([&]([[maybe_unused]] ecs::Iter& it) {
			++cnt;
		});
		REQUIRE(cnt == 2);
	}
	{
		uint32_t cnt = 0;
		auto q = wld.query().all<ecs::uni<Rotation>>();
		q.each([&]([[maybe_unused]] ecs::Iter& it) {
			++cnt;
		});
		REQUIRE(cnt == 1);
	}
	{
		uint32_t cnt = 0;
		auto q = wld.query().all<ecs::uni<Else>>();
		q.each([&]([[maybe_unused]] ecs::Iter& it) {
			++cnt;
		});
		REQUIRE(cnt == 2);
	}
	{
		uint32_t cnt = 0;
		auto q = wld.query().all<ecs::uni<Scale>>();
		q.each([&]([[maybe_unused]] ecs::Iter& it) {
			++cnt;
		});
		REQUIRE(cnt == 2);
	}
	{
		auto q = wld.query().any<ecs::uni<Position>, ecs::uni<Acceleration>>();

		uint32_t cnt = 0;
		q.each([&](ecs::Iter& it) {
			++cnt;

			const bool ok1 = it.has<ecs::uni<Position>>() || it.has<ecs::uni<Acceleration>>();
			REQUIRE(ok1);
			const bool ok2 = it.has<ecs::uni<Acceleration>>() || it.has<ecs::uni<Position>>();
			REQUIRE(ok2);
		});
		REQUIRE(cnt == 2);
	}
	{
		auto q = wld.query().any<ecs::uni<Position>, ecs::uni<Acceleration>>().all<ecs::uni<Scale>>();

		uint32_t cnt = 0;
		q.each([&](ecs::Iter& it) {
			++cnt;

			REQUIRE(it.size() == 1);

			const bool ok1 = it.has<ecs::uni<Position>>() || it.has<ecs::uni<Acceleration>>();
			REQUIRE(ok1);
			const bool ok2 = it.has<ecs::uni<Acceleration>>() || it.has<ecs::uni<Position>>();
			REQUIRE(ok2);
		});
		REQUIRE(cnt == 1);
	}
	{
		auto q = wld.query().any<ecs::uni<Position>, ecs::uni<Acceleration>>().no<ecs::uni<Scale>>();

		uint32_t cnt = 0;
		q.each([&](ecs::Iter& it) {
			++cnt;

			REQUIRE(it.size() == 1);
		});
		REQUIRE(cnt == 1);
	}
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
				REQUIRE(a.x == 1.f);
				REQUIRE(a.y == 1.f);
				REQUIRE(a.z == 1.f);
			} else {
				// REQUIRE(a.x == 0.f);
				// REQUIRE(a.y == 0.f);
				// REQUIRE(a.z == 0.f);
			}
		});
		REQUIRE(cnt == N + 1);
	}
	{
		uint32_t cnt = 0;
		qp.each([&](ecs::Entity ent, const Position& p) {
			++cnt;

			if (ent == e) {
				REQUIRE(p.x == 2.f);
				REQUIRE(p.y == 2.f);
				REQUIRE(p.z == 2.f);
			} else {
				// REQUIRE(p.x == 0.f);
				// REQUIRE(p.y == 0.f);
				// REQUIRE(p.z == 0.f);
			}
		});
		REQUIRE(cnt == N + 1);
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
		REQUIRE(a.x == 1.f);
		REQUIRE(a.y == 1.f);
		REQUIRE(a.z == 1.f);
	}
	{
		const auto& p = wld.get<Position>(e);
		REQUIRE(p.x == 2.f);
		REQUIRE(p.y == 2.f);
		REQUIRE(p.z == 2.f);
	}

	constexpr uint32_t N = 1000;

	wld.copy_n(e, N);

	{
		uint32_t cnt = 0;
		qa.each([&](const Acceleration& a) {
			++cnt;

			REQUIRE(a.x == 1.f);
			REQUIRE(a.y == 1.f);
			REQUIRE(a.z == 1.f);
		});
		REQUIRE(cnt == N + 1);
	}
	{
		uint32_t cnt = 0;
		qp.each([&](const Position& p) {
			++cnt;

			REQUIRE(p.x == 2.f);
			REQUIRE(p.y == 2.f);
			REQUIRE(p.z == 2.f);
		});
		REQUIRE(cnt == N + 1);
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
		REQUIRE(r.x == 0.f);
		REQUIRE(r.y == 0.f);
		REQUIRE(r.z == 0.f);
		REQUIRE(r.w == 0.f);

		auto s = wld.get<Scale>(ent);
		REQUIRE(s.x == 0.f);
		REQUIRE(s.y == 0.f);
		REQUIRE(s.z == 0.f);

		auto e = wld.get<Else>(ent);
		REQUIRE(e.value == false);
	}

	// Modify values
	{
		ecs::Query q = wld.query().all<Rotation&, Scale&, Else&>();

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
				REQUIRE(r.x == 1.f);
				REQUIRE(r.y == 2.f);
				REQUIRE(r.z == 3.f);
				REQUIRE(r.w == 4.f);

				auto s = scaleView[i];
				REQUIRE(s.x == 11.f);
				REQUIRE(s.y == 22.f);
				REQUIRE(s.z == 33.f);

				auto e = elseView[i];
				REQUIRE(e.value == true);
			}
		});

		for (const auto ent: arr) {
			auto r = wld.get<Rotation>(ent);
			REQUIRE(r.x == 1.f);
			REQUIRE(r.y == 2.f);
			REQUIRE(r.z == 3.f);
			REQUIRE(r.w == 4.f);

			auto s = wld.get<Scale>(ent);
			REQUIRE(s.x == 11.f);
			REQUIRE(s.y == 22.f);
			REQUIRE(s.z == 33.f);

			auto e = wld.get<Else>(ent);
			REQUIRE(e.value == true);
		}
	}

	// Modify values + view idx
	{
		ecs::Query q = wld.query().all<Rotation&, Scale&, Else&>();

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
				REQUIRE(r.x == 1.f);
				REQUIRE(r.y == 2.f);
				REQUIRE(r.z == 3.f);
				REQUIRE(r.w == 4.f);

				auto s = scaleView[i];
				REQUIRE(s.x == 11.f);
				REQUIRE(s.y == 22.f);
				REQUIRE(s.z == 33.f);

				auto e = elseView[i];
				REQUIRE(e.value == true);
			}
		});

		for (const auto ent: arr) {
			auto r = wld.get<Rotation>(ent);
			REQUIRE(r.x == 1.f);
			REQUIRE(r.y == 2.f);
			REQUIRE(r.z == 3.f);
			REQUIRE(r.w == 4.f);

			auto s = wld.get<Scale>(ent);
			REQUIRE(s.x == 11.f);
			REQUIRE(s.y == 22.f);
			REQUIRE(s.z == 33.f);

			auto e = wld.get<Else>(ent);
			REQUIRE(e.value == true);
		}
	}

	// Add one more component and check if the values are still fine after creating a new archetype
	{
		auto ent = wld.copy(arr[0]);
		wld.add<Position>(ent, {5, 6, 7});

		auto r = wld.get<Rotation>(ent);
		REQUIRE(r.x == 1.f);
		REQUIRE(r.y == 2.f);
		REQUIRE(r.z == 3.f);
		REQUIRE(r.w == 4.f);

		auto s = wld.get<Scale>(ent);
		REQUIRE(s.x == 11.f);
		REQUIRE(s.y == 22.f);
		REQUIRE(s.z == 33.f);

		auto e = wld.get<Else>(ent);
		REQUIRE(e.value == true);
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
		REQUIRE(r.x == 0.f);
		REQUIRE(r.y == 0.f);
		REQUIRE(r.z == 0.f);
		REQUIRE(r.w == 0.f);

		auto s = wld.get<Scale>(ent);
		REQUIRE(s.x == 0.f);
		REQUIRE(s.y == 0.f);
		REQUIRE(s.z == 0.f);

		auto e = wld.get<Else>(ent);
		REQUIRE(e.value == false);

		auto p = wld.get<ecs::uni<Position>>(ent);
		REQUIRE(p.x == 0.f);
		REQUIRE(p.y == 0.f);
		REQUIRE(p.z == 0.f);
	}

	// Modify values
	{
		ecs::Query q = wld.query().all<Rotation&, Scale&, Else&>();

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
			REQUIRE(p.x == 111.f);
			REQUIRE(p.y == 222.f);
			REQUIRE(p.z == 333.f);
		}
		{
			for (const auto ent: arr) {
				auto r = wld.get<Rotation>(ent);
				REQUIRE(r.x == 1.f);
				REQUIRE(r.y == 2.f);
				REQUIRE(r.z == 3.f);
				REQUIRE(r.w == 4.f);

				auto s = wld.get<Scale>(ent);
				REQUIRE(s.x == 11.f);
				REQUIRE(s.y == 22.f);
				REQUIRE(s.z == 33.f);

				auto e = wld.get<Else>(ent);
				REQUIRE(e.value == true);
			}
		}
		{
			auto p = wld.get<ecs::uni<Position>>(arr[0]);
			REQUIRE(p.x == 111.f);
			REQUIRE(p.y == 222.f);
			REQUIRE(p.z == 333.f);
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
		REQUIRE(s1.value.empty());

		{
			auto s2 = wld.get<StringComponent2>(ent);
			REQUIRE(s2.value == StringComponent2DefaultValue);
		}
		{
			const auto& s2 = wld.get<StringComponent2>(ent);
			REQUIRE(s2.value == StringComponent2DefaultValue);
		}

		const auto& p = wld.get<PositionNonTrivial>(ent);
		REQUIRE(p.x == 1.f);
		REQUIRE(p.y == 2.f);
		REQUIRE(p.z == 3.f);
	}

	// Modify values
	{
		ecs::Query q = wld.query().all<StringComponent&, StringComponent2&, PositionNonTrivial&>();

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
			REQUIRE(s1.value == StringComponentDefaultValue);

			const auto& s2 = wld.get<StringComponent2>(ent);
			REQUIRE(s2.value == StringComponent2DefaultValue_2);

			const auto& p = wld.get<PositionNonTrivial>(ent);
			REQUIRE(p.x == 111.f);
			REQUIRE(p.y == 222.f);
			REQUIRE(p.z == 333.f);
		}
	}

	// Add one more component and check if the values are still fine after creating a new archetype
	{
		auto ent = wld.copy(arr[0]);
		wld.add<Position>(ent, {5, 6, 7});

		const auto& s1 = wld.get<StringComponent>(ent);
		REQUIRE(s1.value == StringComponentDefaultValue);

		const auto& s2 = wld.get<StringComponent2>(ent);
		REQUIRE(s2.value == StringComponent2DefaultValue_2);

		const auto& p = wld.get<PositionNonTrivial>(ent);
		REQUIRE(p.x == 111.f);
		REQUIRE(p.y == 222.f);
		REQUIRE(p.z == 333.f);
	}
}

TEST_CASE("CommandBuffer") {
	SECTION("Entity creation") {
		TestWorld twld;
		ecs::CommandBuffer cb(wld);

		const uint32_t N = 100;
		GAIA_FOR(N) {
			[[maybe_unused]] auto tmp = cb.add();
		}

		cb.commit();

		REQUIRE(wld.size() == ecs::GAIA_ID(LastCoreComponent).id() + 1 + N);
	}

	SECTION("Entity creation from another entity") {
		TestWorld twld;
		ecs::CommandBuffer cb(wld);

		auto mainEntity = wld.add();

		const uint32_t N = 100;
		GAIA_FOR(N) {
			[[maybe_unused]] auto tmp = cb.copy(mainEntity);
		}

		cb.commit();

		REQUIRE(wld.size() == ecs::GAIA_ID(LastCoreComponent).id() + 1 + 1 + N); // core + mainEntity + N others
	}

	SECTION("Entity creation from another entity with a component") {
		{
			TestWorld twld;
			ecs::CommandBuffer cb(wld);
			auto mainEntity = wld.add();

			wld.add<Position>(mainEntity, {1, 2, 3});

			auto q = wld.query().all<Position>();
			REQUIRE(q.count() == 1);
			uint32_t i = 0;
			q.each([&](const Position& p) {
				REQUIRE(p.x == 1.f);
				REQUIRE(p.y == 2.f);
				REQUIRE(p.z == 3.f);
				++i;
			});
			REQUIRE(i == 1);

			(void)wld.copy(mainEntity);
			REQUIRE(q.count() == 2);
			i = 0;
			q.each([&](const Position& p) {
				REQUIRE(p.x == 1.f);
				REQUIRE(p.y == 2.f);
				REQUIRE(p.z == 3.f);
				++i;
			});
			REQUIRE(i == 2);
		}

		{
			TestWorld twld;
			ecs::CommandBuffer cb(wld);
			auto mainEntity = wld.add();

			wld.add<Position>(mainEntity, {1, 2, 3});

			[[maybe_unused]] auto tmp = cb.copy(mainEntity);
			cb.commit();

			auto q = wld.query().all<Position>();
			REQUIRE(q.count() == 2);
			uint32_t i = 0;
			q.each([&](const Position& p) {
				REQUIRE(p.x == 1.f);
				REQUIRE(p.y == 2.f);
				REQUIRE(p.z == 3.f);
				++i;
			});
			REQUIRE(i == 2);
		}
	}

	SECTION("Entity creation from another entity with a SoA component") {
		{
			TestWorld twld;
			ecs::CommandBuffer cb(wld);
			auto mainEntity = wld.add();

			wld.add<PositionSoA>(mainEntity, {1, 2, 3});

			auto q = wld.query().all<PositionSoA>();
			REQUIRE(q.count() == 1);
			uint32_t i = 0;
			q.each([&](const PositionSoA& p) {
				REQUIRE(p.x == 1.f);
				REQUIRE(p.y == 2.f);
				REQUIRE(p.z == 3.f);
				++i;
			});
			REQUIRE(i == 1);

			(void)wld.copy(mainEntity);
			REQUIRE(q.count() == 2);
			i = 0;
			q.each([&](const PositionSoA& p) {
				REQUIRE(p.x == 1.f);
				REQUIRE(p.y == 2.f);
				REQUIRE(p.z == 3.f);
				++i;
			});
			REQUIRE(i == 2);
		}

		{
			TestWorld twld;
			ecs::CommandBuffer cb(wld);
			auto mainEntity = wld.add();

			wld.add<PositionSoA>(mainEntity, {1, 2, 3});

			(void)cb.copy(mainEntity);
			cb.commit();

			auto q = wld.query().all<PositionSoA>();
			REQUIRE(q.count() == 2);
			uint32_t i = 0;
			q.each([&](const PositionSoA& p) {
				REQUIRE(p.x == 1.f);
				REQUIRE(p.y == 2.f);
				REQUIRE(p.z == 3.f);
				++i;
			});
			REQUIRE(i == 2);
		}
	}

	SECTION("Delayed component addition to an existing entity") {
		TestWorld twld;
		ecs::CommandBuffer cb(wld);

		auto e = wld.add();
		cb.add<Position>(e);
		REQUIRE_FALSE(wld.has<Position>(e));
		cb.commit();
		REQUIRE(wld.has<Position>(e));
	}

	SECTION("Delayed component addition to a to-be-created entity") {
		TestWorld twld;
		ecs::CommandBuffer cb(wld);

		auto tmp = cb.add(); // core + 0 (no new entity created yet)
		REQUIRE(wld.size() == ecs::GAIA_ID(LastCoreComponent).id() + 1);
		cb.add<Position>(tmp);
		cb.commit();

		auto e = wld.get(ecs::GAIA_ID(LastCoreComponent).id() + 1 + 1); // core + position + new entity
		REQUIRE(wld.has<Position>(e));
	}

	SECTION("Delayed component setting of an existing entity") {
		TestWorld twld;
		ecs::CommandBuffer cb(wld);

		auto e = wld.add();

		cb.add<Position>(e);
		cb.set<Position>(e, {1, 2, 3});
		REQUIRE_FALSE(wld.has<Position>(e));

		cb.commit();
		REQUIRE(wld.has<Position>(e));

		auto p = wld.get<Position>(e);
		REQUIRE(p.x == 1);
		REQUIRE(p.y == 2);
		REQUIRE(p.z == 3);
	}

	SECTION("Delayed 2 components setting of an existing entity") {
		TestWorld twld;
		ecs::CommandBuffer cb(wld);

		auto e = wld.add();

		cb.add<Position>(e);
		cb.set<Position>(e, {1, 2, 3});
		cb.add<Acceleration>(e);
		cb.set<Acceleration>(e, {4, 5, 6});
		REQUIRE_FALSE(wld.has<Position>(e));
		REQUIRE_FALSE(wld.has<Acceleration>(e));

		cb.commit();
		REQUIRE(wld.has<Position>(e));
		REQUIRE(wld.has<Acceleration>(e));

		auto p = wld.get<Position>(e);
		REQUIRE(p.x == 1.f);
		REQUIRE(p.y == 2.f);
		REQUIRE(p.z == 3.f);

		auto a = wld.get<Acceleration>(e);
		REQUIRE(a.x == 4.f);
		REQUIRE(a.y == 5.f);
		REQUIRE(a.z == 6.f);
	}

	SECTION("Delayed component setting of a to-be-created entity") {
		TestWorld twld;
		ecs::CommandBuffer cb(wld);

		auto tmp = cb.add();
		REQUIRE(wld.size() == ecs::GAIA_ID(LastCoreComponent).id() + 1); // core + 0 (no new entity created yet)

		cb.add<Position>(tmp);
		cb.set<Position>(tmp, {1, 2, 3});
		cb.commit();

		auto e = wld.get(ecs::GAIA_ID(LastCoreComponent).id() + 1 + 1); // core + position + new entity
		REQUIRE(wld.has<Position>(e));

		auto p = wld.get<Position>(e);
		REQUIRE(p.x == 1.f);
		REQUIRE(p.y == 2.f);
		REQUIRE(p.z == 3.f);
	}

	SECTION("Delayed 2 components setting of a to-be-created entity") {
		TestWorld twld;
		ecs::CommandBuffer cb(wld);

		auto tmp = cb.add();
		REQUIRE(wld.size() == ecs::GAIA_ID(LastCoreComponent).id() + 1); // core + 0 (no new entity created yet)

		cb.add<Position>(tmp);
		cb.add<Acceleration>(tmp);
		cb.set<Position>(tmp, {1, 2, 3});
		cb.set<Acceleration>(tmp, {4, 5, 6});
		cb.commit();

		auto e = wld.get(ecs::GAIA_ID(LastCoreComponent).id() + 1 + 2); // core + 2 new components + new entity
		REQUIRE(wld.has<Position>(e));
		REQUIRE(wld.has<Acceleration>(e));

		auto p = wld.get<Position>(e);
		REQUIRE(p.x == 1.f);
		REQUIRE(p.y == 2.f);
		REQUIRE(p.z == 3.f);

		auto a = wld.get<Acceleration>(e);
		REQUIRE(a.x == 4.f);
		REQUIRE(a.y == 5.f);
		REQUIRE(a.z == 6.f);
	}

	SECTION("Delayed component add with setting of a to-be-created entity") {
		TestWorld twld;
		ecs::CommandBuffer cb(wld);

		auto tmp = cb.add();
		REQUIRE(wld.size() == ecs::GAIA_ID(LastCoreComponent).id() + 1); // core + 0 (no new entity created yet)

		cb.add<Position>(tmp, {1, 2, 3});
		cb.commit();

		auto e = wld.get(ecs::GAIA_ID(LastCoreComponent).id() + 1 + 1); // core + position + new entity
		REQUIRE(wld.has<Position>(e));

		auto p = wld.get<Position>(e);
		REQUIRE(p.x == 1.f);
		REQUIRE(p.y == 2.f);
		REQUIRE(p.z == 3.f);
	}

	SECTION("Delayed 2 components add with setting of a to-be-created entity") {
		TestWorld twld;
		ecs::CommandBuffer cb(wld);

		auto tmp = cb.add();
		REQUIRE(wld.size() == ecs::GAIA_ID(LastCoreComponent).id() + 1); // core + 0 (no new entity created yet)

		cb.add<Position>(tmp, {1, 2, 3});
		cb.add<Acceleration>(tmp, {4, 5, 6});
		cb.commit();

		auto e = wld.get(ecs::GAIA_ID(LastCoreComponent).id() + 1 + 2); // core + 2 new components + new entity
		REQUIRE(wld.has<Position>(e));
		REQUIRE(wld.has<Acceleration>(e));

		auto p = wld.get<Position>(e);
		REQUIRE(p.x == 1.f);
		REQUIRE(p.y == 2.f);
		REQUIRE(p.z == 3.f);

		auto a = wld.get<Acceleration>(e);
		REQUIRE(a.x == 4.f);
		REQUIRE(a.y == 5.f);
		REQUIRE(a.z == 6.f);
	}

	SECTION("Delayed component removal from an existing entity") {
		TestWorld twld;
		ecs::CommandBuffer cb(wld);

		auto e = wld.add();
		wld.add<Position>(e, {1, 2, 3});

		cb.del<Position>(e);
		REQUIRE(wld.has<Position>(e));
		{
			auto p = wld.get<Position>(e);
			REQUIRE(p.x == 1.f);
			REQUIRE(p.y == 2.f);
			REQUIRE(p.z == 3.f);
		}

		cb.commit();
		REQUIRE_FALSE(wld.has<Position>(e));
	}

	SECTION("Delayed 2 component removal from an existing entity") {
		TestWorld twld;
		ecs::CommandBuffer cb(wld);

		auto e = wld.add();
		wld.add<Position>(e, {1, 2, 3});
		wld.add<Acceleration>(e, {4, 5, 6});

		cb.del<Position>(e);
		cb.del<Acceleration>(e);
		REQUIRE(wld.has<Position>(e));
		REQUIRE(wld.has<Acceleration>(e));
		{
			auto p = wld.get<Position>(e);
			REQUIRE(p.x == 1.f);
			REQUIRE(p.y == 2.f);
			REQUIRE(p.z == 3.f);

			auto a = wld.get<Acceleration>(e);
			REQUIRE(a.x == 4.f);
			REQUIRE(a.y == 5.f);
			REQUIRE(a.z == 6.f);
		}

		cb.commit();
		REQUIRE_FALSE(wld.has<Position>(e));
		REQUIRE_FALSE(wld.has<Acceleration>(e));
	}

	SECTION("Delayed non-trivial component setting of an existing entity") {
		TestWorld twld;
		ecs::CommandBuffer cb(wld);

		auto e = wld.add();

		cb.add<StringComponent>(e);
		cb.set<StringComponent>(e, {StringComponentDefaultValue});
		cb.add<StringComponent2>(e);
		REQUIRE_FALSE(wld.has<StringComponent>(e));
		REQUIRE_FALSE(wld.has<StringComponent2>(e));

		cb.commit();
		REQUIRE(wld.has<StringComponent>(e));
		REQUIRE(wld.has<StringComponent2>(e));

		auto s1 = wld.get<StringComponent>(e);
		REQUIRE(s1.value == StringComponentDefaultValue);
		auto s2 = wld.get<StringComponent2>(e);
		REQUIRE(s2.value == StringComponent2DefaultValue);
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
		REQUIRE(cnt == 1); // first run always happens
	}
	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		REQUIRE(cnt == 0); // no change of position so this shouldn't run
	}
	{ wld.set<Position>(e) = {}; }
	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		REQUIRE(cnt == 1);
	}
	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		REQUIRE(cnt == 0);
	}
	{ wld.sset<Position>(e) = {}; }
	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		REQUIRE(cnt == 0);
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
		REQUIRE(cnt == 0);
	}
	auto e2 = wld.copy(e);
	(void)e2;
	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		REQUIRE(cnt == 2); // adding an entity triggers the change
	}
	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		REQUIRE(cnt == 0); // no new change since the last time
	}
}

TEST_CASE("Query Filter - systems") {
	TestWorld twld;

	auto e = wld.add();
	wld.add<Position>(e);

	class WriterSystem final: public ecs::System {
		ecs::Query m_q;

	public:
		void OnCreated() override {
			m_q = world().query().all<Position&>();
		}
		void OnUpdate() override {
			m_q.each([]([[maybe_unused]] Position&) {});
		}
	};
	class WriterSystemSilent final: public ecs::System {
		ecs::Query m_q;

	public:
		void OnCreated() override {
			m_q = world().query().all<Position&>();
		}
		void OnUpdate() override {
			m_q.each([&](ecs::Iter& it) {
				auto posRWView = it.sview_mut<Position>();
				(void)posRWView;
			});
		}
	};
	class ReaderSystem final: public ecs::System {
		uint32_t m_expectedCnt = 0;

		ecs::Query m_q;

	public:
		void SetExpectedCount(uint32_t cnt) {
			m_expectedCnt = cnt;
		}
		void OnCreated() override {
			m_q = world().query().all<Position>().changed<Position>();
		}
		void OnUpdate() override {
			uint32_t cnt = 0;
			m_q.each([&]([[maybe_unused]] const Position& a) {
				++cnt;
			});
			REQUIRE(cnt == m_expectedCnt);
		}
	};
	ecs::SystemManager sm(wld);
	auto* ws = sm.add<WriterSystem>();
	auto* wss = sm.add<WriterSystemSilent>();
	auto* rs = sm.add<ReaderSystem>();

	// first run always happens
	ws->enable(false);
	wss->enable(false);
	rs->SetExpectedCount(1);
	sm.update();
	// no change of position so ReaderSystem should't see changes
	rs->SetExpectedCount(0);
	sm.update();
	// update position so ReaderSystem should detect a change
	ws->enable(true);
	rs->SetExpectedCount(1);
	sm.update();
	// no change of position so ReaderSystem shouldn't see changes
	ws->enable(false);
	rs->SetExpectedCount(0);
	sm.update();
	// silent writer enabled again. If should not cause an update
	ws->enable(false);
	wss->enable(true);
	rs->SetExpectedCount(0);
	sm.update();
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
			[[maybe_unused]] auto newentity = wld.copy(e);
		}
	}

	int sys1_cnt = 0;
	int sys2_cnt = 0;
	int sys3_cnt = 0;
	bool sys3_run_before_sys1 = false;
	bool sys3_run_before_sys2 = false;

	auto testRun = [&]() {
		GAIA_FOR(100) {
			sys3_run_before_sys1 = false;
			sys3_run_before_sys2 = false;
			wld.update();
			REQUIRE(sys1_cnt == N);
			REQUIRE(sys2_cnt == N);
			REQUIRE(sys3_cnt == N);
			sys1_cnt = 0;
			sys2_cnt = 0;
			sys3_cnt = 0;
		}
	};

	// Our systems
	auto sys1 = wld.system()
									.all<Position, Acceleration>() //
									.on_each([&](Position, Acceleration) {
										if (sys1_cnt == 0 && sys3_cnt == 0)
											sys3_run_before_sys1 = true;
										++sys1_cnt;
									});
	auto sys2 = wld.system()
									.all<Position>() //
									.on_each([&](ecs::Iter& it) {
										if (sys2_cnt == 0 && sys3_cnt == 0)
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
	// REQUIRE(sys3_run_before_sys1);
	// REQUIRE(sys3_run_before_sys2);
}

struct Eats {};
struct Healthy {};
ecs::Entity group_by_relation(ecs::World& w, ecs::Entity id) {
	auto eats = w.add<Eats>().entity;
	auto tgt = w.target(id, eats);
	return (tgt != ecs::EntityBad) ? ((ecs::Entity)ecs::Pair(eats, tgt)).value() : ecs::EntityBad;
}

TEST_CASE("System - group") {
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
												std::span<ecs::Entity> ents_exected_view) {
		{
			uint32_t j = 0;
			q.each([&](ecs::Iter& it) {
				auto ents = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					const auto e = ents[i];
					const auto e_wanted = ents_exected_view[j++];
					REQUIRE(e == e_wanted);
				}
			});
			REQUIRE(j == (uint32_t)ents_exected_view.size());
		}
		{
			uint32_t j = 0;
			q.each([&](ecs::Entity e) {
				const auto e_wanted = ents_exected_view[j++];
				REQUIRE(e == e_wanted);
			});
			REQUIRE(j == (uint32_t)ents_exected_view.size());
		}
	};

	{
		auto qq = wld.query().all<Position>().group_by(eats);

		// Grouping on, no group enforced
		checkQuery(qq, {&ents_expected[0], 6});
		// Gruping on, a group is enforced
		qq.group_id(carrot);
		checkQuery(qq, {&ents_expected[0], 2});
		qq.group_id(salad);
		checkQuery(qq, {&ents_expected[2], 2});
		qq.group_id(apple);
		checkQuery(qq, {&ents_expected[4], 2});
	}
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

	SECTION("each - iterator") {
		ecs::Query q = wld.query().all<T&>();

		{
			const auto cnt = q.count();
			REQUIRE(cnt == N);

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

					REQUIRE(tx[i] == f);
					REQUIRE(ty[i] == f);
					REQUIRE(tz[i] == f);

					++j;
				}
			});
			REQUIRE(j == cnt);
		}

		// Make sure disabling works
		{
			auto e = ents[0];
			wld.enable(e, false);
			const auto cnt = q.count();
			REQUIRE(cnt == N - 1);
			uint32_t cntCalculated = 0;
			q.each([&](ecs::Iter& it) {
				cntCalculated += it.size();
			});
			REQUIRE(cnt == cntCalculated);
		}
	}

	// TODO: Finish this part
	// SECTION("each") {
	// 	ecs::Query q = wld.query().all<T>();

	// 	{
	// 		const auto cnt = q.count();
	// 		REQUIRE(cnt == N);

	// 		uint32_t j = 0;
	// 		// TODO: Add SoA support for q.each([](T& t){})
	// 		q.each([&](const T& t) {
	// 			auto f = (float)j;
	// 			REQUIRE(t.x == f);
	// 			REQUIRE(t.y == f);
	// 			REQUIRE(t.z == f);

	// 			++j;
	// 		});
	// 		REQUIRE(j == cnt);
	// 	}

	// 	// Make sure disabling works
	// 	{
	// 		auto e = ents[0];
	// 		wld.enable(e, false);
	// 		const auto cnt = q.count();
	// 		REQUIRE(cnt == N - 1);
	// 		uint32_t cntCalculated = 0;
	// 		q.each([&]([[maybe_unused]] const T& t) {
	// 			++cntCalculated;
	// 		});
	// 		REQUIRE(cnt == cntCalculated);
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
	REQUIRE(other.entity == d.entity);
	REQUIRE(other.comp == d.comp);
}

TEST_CASE("Component cache") {
	{
		TestWorld twld;
		const auto& desc = wld.add<Position>();
		auto ent = desc.entity;
		REQUIRE_FALSE(ent.entity());

		comp_cache_verify<const Position>(wld, desc);
		comp_cache_verify<Position&>(wld, desc);
		comp_cache_verify<const Position&>(wld, desc);
		comp_cache_verify<Position*>(wld, desc);
		comp_cache_verify<const Position*>(wld, desc);
	}
	{
		TestWorld twld;
		const auto& desc = wld.add<const Position>();
		auto ent = desc.entity;
		REQUIRE_FALSE(ent.entity());

		comp_cache_verify<Position>(wld, desc);
		comp_cache_verify<Position&>(wld, desc);
		comp_cache_verify<const Position&>(wld, desc);
		comp_cache_verify<Position*>(wld, desc);
		comp_cache_verify<const Position*>(wld, desc);
	}
	{
		TestWorld twld;
		const auto& desc = wld.add<Position&>();
		auto ent = desc.entity;
		REQUIRE_FALSE(ent.entity());

		comp_cache_verify<Position>(wld, desc);
		comp_cache_verify<const Position>(wld, desc);
		comp_cache_verify<const Position&>(wld, desc);
		comp_cache_verify<Position*>(wld, desc);
		comp_cache_verify<const Position*>(wld, desc);
	}
	{
		TestWorld twld;
		const auto& desc = wld.add<const Position&>();
		auto ent = desc.entity;
		REQUIRE_FALSE(ent.entity());

		comp_cache_verify<Position>(wld, desc);
		comp_cache_verify<const Position>(wld, desc);
		comp_cache_verify<Position&>(wld, desc);
		comp_cache_verify<Position*>(wld, desc);
		comp_cache_verify<const Position*>(wld, desc);
	}
	{
		TestWorld twld;
		const auto& desc = wld.add<Position*>();
		auto ent = desc.entity;
		REQUIRE_FALSE(ent.entity());

		comp_cache_verify<Position>(wld, desc);
		comp_cache_verify<const Position>(wld, desc);
		comp_cache_verify<Position&>(wld, desc);
		comp_cache_verify<const Position&>(wld, desc);
		comp_cache_verify<const Position*>(wld, desc);
	}
	{
		TestWorld twld;
		const auto& desc = wld.add<const Position*>();
		auto ent = desc.entity;
		REQUIRE_FALSE(ent.entity());

		comp_cache_verify<Position>(wld, desc);
		comp_cache_verify<const Position>(wld, desc);
		comp_cache_verify<Position&>(wld, desc);
		comp_cache_verify<const Position&>(wld, desc);
		comp_cache_verify<Position*>(wld, desc);
	}
}

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
		REQUIRE(p.x == 1.f);
		REQUIRE(p.y == 1.f);
		REQUIRE(p.z == 1.f);
		++c;
	});
	REQUIRE(c == 3);

	c = 0;
	auto q2 = w2.query().all<Position>();
	q2.each([&c](const Position& p) {
		REQUIRE(p.x == 2.f);
		REQUIRE(p.y == 2.f);
		REQUIRE(p.z == 2.f);
		++c;
	});
	REQUIRE(c == 1);
}

//------------------------------------------------------------------------------
// Serialization
//------------------------------------------------------------------------------

template <typename T>
bool CompareSerializableType(const T& a, const T& b) {
	if constexpr (
			!std::is_trivially_copyable_v<T> || core::has_member_equals<T>::value || core::has_global_equals<T>::value) {
		return a == b;
	} else {
		return !std::memcmp((const void*)&a, (const void*)&b, sizeof(a));
	}
}

template <typename T>
bool CompareSerializableTypes(const T& a, const T& b) {
	if constexpr (core::has_member_equals<T>::value || core::has_global_equals<T>::value) {
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
	return a.size == b.size && !memcmp(a.ptr, b.ptr, a.size);
}
namespace gaia::ser {
	template <>
	uint32_t bytes(const CustomStruct& data) {
		return data.size + sizeof(data.size);
	}

	template <typename Serializer>
	void save(Serializer& s, const CustomStruct& data) {
		s.save(data.size);
		s.save(data.ptr, data.size);
	}

	template <typename Serializer>
	void load(Serializer& s, CustomStruct& data) {
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

	constexpr uint32_t bytes() const noexcept {
		return size + sizeof(size);
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
	SECTION("external") {
		CustomStruct in, out;
		in.ptr = new char[5];
		in.ptr[0] = 'g';
		in.ptr[1] = 'a';
		in.ptr[2] = 'i';
		in.ptr[3] = 'a';
		in.ptr[4] = '\0';
		in.size = 5;

		ecs::SerializationBuffer s;
		s.reserve(ser::bytes(in));

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		REQUIRE(CompareSerializableTypes(in, out));
		delete in.ptr;
		delete out.ptr;
	}
	SECTION("internal") {
		CustomStructInternal in, out;
		in.ptr = new char[5];
		in.ptr[0] = 'g';
		in.ptr[1] = 'a';
		in.ptr[2] = 'i';
		in.ptr[3] = 'a';
		in.ptr[4] = '\0';
		in.size = 5;

		ecs::SerializationBuffer s;
		s.reserve(ser::bytes(in));

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		REQUIRE(CompareSerializableTypes(in, out));
		delete in.ptr;
		delete out.ptr;
	}
}

TEST_CASE("Serialization - simple") {
	{
		Int3 in{1, 2, 3}, out{};

		ecs::SerializationBuffer s;
		s.reserve(ser::bytes(in));

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		REQUIRE(CompareSerializableTypes(in, out));
	}

	{
		Position in{1, 2, 3}, out{};

		ecs::SerializationBuffer s;
		s.reserve(ser::bytes(in));

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		REQUIRE(CompareSerializableTypes(in, out));
	}

	{
		SerializeStruct1 in{1, 2, true, 3.12345f}, out{};

		ecs::SerializationBuffer s;
		s.reserve(ser::bytes(in));

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		REQUIRE(CompareSerializableTypes(in, out));
	}

	{
		SerializeStruct2 in{FooNonTrivial(111), 1, 2, true, 3.12345f}, out{};

		ecs::SerializationBuffer s;
		s.reserve(ser::bytes(in));

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		REQUIRE(CompareSerializableTypes(in, out));
	}
}

struct SerializeStructSArray {
	cnt::sarray<uint32_t, 8> arr;
	float f;
};

struct SerializeStructSArrayNonTrivial {
	cnt::sarray<FooNonTrivial, 8> arr;
	float f;

	bool operator==(const SerializeStructSArrayNonTrivial& other) const {
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
		s.reserve(ser::bytes(in));

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		REQUIRE(CompareSerializableTypes(in, out));
	}

	{
		SerializeStructSArrayNonTrivial in{}, out{};
		GAIA_EACH(in.arr) in.arr[i] = FooNonTrivial(i);
		in.f = 3.12345f;

		ecs::SerializationBuffer s;
		s.reserve(ser::bytes(in));

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		REQUIRE(CompareSerializableTypes(in, out));
	}

	{
		SerializeStructDArray in{}, out{};
		in.arr.resize(10);
		GAIA_EACH(in.arr) in.arr[i] = i;
		in.f = 3.12345f;

		ecs::SerializationBuffer s;
		s.reserve(ser::bytes(in));

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		REQUIRE(CompareSerializableTypes(in, out));
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
		s.reserve(ser::bytes(in));

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		REQUIRE(CompareSerializableTypes(in, out));

		for (auto& a: in.arr)
			delete a.ptr;
		for (auto& a: out.arr)
			delete a.ptr;
	}
}

//------------------------------------------------------------------------------
// Mutlithreading
//------------------------------------------------------------------------------

static uint32_t JobSystemFunc(std::span<const uint32_t> arr) {
	uint32_t sum = 0;
	GAIA_EACH(arr) sum += arr[i];
	return sum;
}

template <typename Func>
void Run_Schedule_Simple(const uint32_t* pArr, uint32_t* pRes, uint32_t Jobs, uint32_t ItemsPerJob, Func func) {
	auto& tp = mt::ThreadPool::get();

	GAIA_FOR(Jobs) {
		mt::Job job;
		job.func = [&pArr, &pRes, i, ItemsPerJob, func]() {
			const auto idxStart = i * ItemsPerJob;
			const auto idxEnd = (i + 1) * ItemsPerJob;
			pRes[i] += func({pArr + idxStart, idxEnd - idxStart});
		};
		tp.sched(job);
	}
	tp.wait_all();

	GAIA_FOR(Jobs) REQUIRE(pRes[i] == ItemsPerJob);
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

	SECTION("Max workers") {
		const auto threads = tp.hw_thread_cnt();
		tp.set_max_workers(threads, threads);

		GAIA_FOR(res.max_size()) res[i] = 0;
		Run_Schedule_Simple(arr.data(), res.data(), JobCount, ItemsPerJob, JobSystemFunc);
	}
	SECTION("0 workers") {
		tp.set_max_workers(0, 0);

		GAIA_FOR(res.max_size()) res[i] = 0;
		Run_Schedule_Simple(arr.data(), res.data(), JobCount, ItemsPerJob, JobSystemFunc);
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
		REQUIRE(sum1 == N);
		tp.wait_all();
	};

	SECTION("Max workers") {
		const auto threads = tp.hw_thread_cnt();
		tp.set_max_workers(threads, threads);

		work();
	}
	SECTION("0 workers") {
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

	auto work = [&]() {
		GAIA_EACH(res) res[i] = BadIndex;

		GAIA_FOR(Jobs) {
			mt::Job job;
			job.func = [&res, i]() {
				res[i] = i;
			};
			handles[i] = tp.sched(job);
		}

		GAIA_FOR(Jobs) {
			tp.wait(handles[i]);
			REQUIRE(res[i] == i);
		}
	};

	SECTION("Max workers") {
		const auto threads = tp.hw_thread_cnt();
		tp.set_max_workers(threads, threads);

		work();
	}
	SECTION("0 workers") {
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

			tp.dep(jobHandle[1], jobHandle[0]);
			tp.dep(jobHandle[2], jobHandle[1]);

			// 2, 0, 1 -> wrong sum
			// Submit jobs in random order to make sure this doesn't work just by accident
			const uint32_t startIdx0 = rand() % 3;
			const uint32_t startIdx1 = (startIdx0 + 1) % 3;
			const uint32_t startIdx2 = (startIdx0 + 2) % 3;
			tp.submit(jobHandle[startIdx0]);
			tp.submit(jobHandle[startIdx1]);
			tp.submit(jobHandle[startIdx2]);

			tp.wait(jobHandle[2]);

			REQUIRE(res == (i + 1));
		}
	};

	SECTION("Max workers") {
		const auto threads = tp.hw_thread_cnt();
		tp.set_max_workers(threads, threads);

		work();
	}
	SECTION("0 workers") {
		tp.set_max_workers(0, 0);

		work();
	}
}

//------------------------------------------------------------------------------
// Allocators
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
			REQUIRE(pPosN[i].x == 1);
			REQUIRE(pPosN[i].y == 2);
			REQUIRE(pPosN[i].z == 3);
		}
	}
	a.reset();
	{
		auto* pInt = a.alloc<int>(1);
		*pInt = 10;
		a.free(pInt, 1);
		pInt = a.alloc<int>(1);
		REQUIRE(*pInt == 10);

		auto* pPos = a.alloc<Position>(10);
		GAIA_FOR(10) {
			pPos[i].x = i;
			pPos[i].y = i + 1;
			pPos[i].z = i + 2;
		}
		a.free(pPos, 10);
		pPos = a.alloc<Position>(10);
		GAIA_FOR(10) {
			REQUIRE(pPos[i].x == i);
			REQUIRE(pPos[i].y == i + 1);
			REQUIRE(pPos[i].z == i + 2);
		}

		auto* pPosN = a.alloc<PositionNonTrivial>(3);
		GAIA_FOR(3) {
			REQUIRE(pPosN[i].x == 1);
			REQUIRE(pPosN[i].y == 2);
			REQUIRE(pPosN[i].z == 3);
		}

		// Alloc and release some more objects
		auto* pFoo = a.alloc<TFoo>(1);
		auto* pInt5 = a.alloc<int>(5);
		a.free(pInt5, 5);
		a.free(pFoo, 1);

		// Make sure the previously stored positions are still intact
		GAIA_FOR(3) {
			REQUIRE(pPosN[i].x == 1);
			REQUIRE(pPosN[i].y == 2);
			REQUIRE(pPosN[i].z == 3);
		}
	}
}

//------------------------------------------------------------------------------
