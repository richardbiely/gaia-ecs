#include <gaia.h>

#if GAIA_COMPILER_MSVC
	#if _MSC_VER <= 1916
// warning C4100: 'XYZ': unreferenced formal parameter
GAIA_MSVC_WARNING_DISABLE(4100)
	#endif
#endif

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace gaia;

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

TEST_CASE("ComponentKinds") {
	REQUIRE(ecs::component_kind_v<uint32_t> == ecs::ComponentKind::CK_Gen);
	REQUIRE(ecs::component_kind_v<Position> == ecs::ComponentKind::CK_Gen);
	REQUIRE(ecs::component_kind_v<ecs::uni<Position>> == ecs::ComponentKind::CK_Uni);
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

	for (uint32_t i = 0; i < NBlocks; ++i) {
		bv.set(i * BlockBits, (uint8_t)i);

		const uint8_t val = bv.get(i * BlockBits);
		REQUIRE(val == i);
	}

	// Make sure nothing was overriden
	for (uint32_t i = 0; i < NBlocks; ++i) {
		const uint8_t val = bv.get(i * BlockBits);
		REQUIRE(val == i);
	}
}

template <typename Container>
void fixed_arr_test() {
	constexpr auto N = Container::extent;
	static_assert(N > 2); // we need at least 2 items to complete this test
	Container arr;

	for (uint32_t i = 0; i < N; ++i) {
		arr[i] = i;
		REQUIRE(arr[i] == i);
	}
	// Verify the values remain the same even after the internal buffer is reallocated
	for (uint32_t i = 0; i < N; ++i) {
		REQUIRE(arr[i] == i);
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

	REQUIRE(core::find(arr, 0U) == arr.begin());
	REQUIRE(core::find(arr, N) == arr.end());

	REQUIRE(core::has(arr, 0U));
	REQUIRE_FALSE(core::has(arr, N));
}

TEST_CASE("Containers - sarr") {
	using arr = cnt::sarr<uint32_t, 100>;
	fixed_arr_test<arr>();
}

template <typename Container>
void resizable_arr_test(uint32_t N) {
	GAIA_ASSERT(N > 2); // we need at least 2 items to complete this test
	Container arr;

	for (uint32_t i = 0; i < N; ++i) {
		arr.push_back(i);
		REQUIRE(arr[i] == i);
	}
	// Verify the values remain the same even after the internal buffer is reallocated
	for (uint32_t i = 0; i < N; ++i) {
		REQUIRE(arr[i] == i);
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

	REQUIRE(core::find(arr, 0U) == arr.begin());
	REQUIRE(core::find(arr, N) == arr.end());

	REQUIRE(core::has(arr, 0U));
	REQUIRE_FALSE(core::has(arr, N));

	arr.erase(arr.begin());
	REQUIRE(arr.size() == (N - 1));
	REQUIRE(core::find(arr, 0U) == arr.end());
	for (uint32_t i = 0; i < arr.size(); ++i)
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
}

TEST_CASE("Containers - sarr_ext") {
	using arr = cnt::sarr_ext<uint32_t, 100>;
	resizable_arr_test<arr>(100);
}

TEST_CASE("Containers - darr") {
	using arr = cnt::darr<uint32_t>;
	resizable_arr_test<arr>(100);
	resizable_arr_test<arr>(10000);
}

TEST_CASE("Containers - darr_ext") {
	using arr0 = cnt::darr_ext<uint32_t, 50>;
	resizable_arr_test<arr0>(100);
	resizable_arr_test<arr0>(10000);

	using arr1 = cnt::darr_ext<uint32_t, 100>;
	resizable_arr_test<arr1>(100);
	resizable_arr_test<arr1>(10000);
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

	for (uint32_t i = 0; i < N; ++i) {
		const auto f = (float)(i + 1);
		data[i] = {f, f, f};

		auto val = data[i];
		REQUIRE(val.x == f);
		REQUIRE(val.y == f);
		REQUIRE(val.z == f);
	}

	SECTION("Make sure that all values are correct (e.g. that they were not overriden by one of the loop iteration)") {
		for (uint32_t i = 0; i < N; ++i) {
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
	cnt::sarray<T, N> data;

	for (uint32_t i = 0; i < N; ++i) {
		const float f[] = {(float)(i + 1), (float)(i + 2), (float)(i + 3)};
		data[i] = {f[0], f[1], f[2]};

		T val = data[i];
		REQUIRE(val.x == f[0]);
		REQUIRE(val.y == f[1]);
		REQUIRE(val.z == f[2]);

		const float ff[] = {data.template soa_view<0>()[i], data.template soa_view<1>()[i], data.template soa_view<2>()[i]};
		REQUIRE(ff[0] == f[0]);
		REQUIRE(ff[1] == f[1]);
		REQUIRE(ff[2] == f[2]);
	}

	// Make sure that all values are correct (e.g. that they were not overriden by one of the loop iteration)
	for (uint32_t i = 0; i < N; ++i) {
		const float f[] = {(float)(i + 1), (float)(i + 2), (float)(i + 3)};
		T val = data[i];
		REQUIRE(val.x == f[0]);
		REQUIRE(val.y == f[1]);
		REQUIRE(val.z == f[2]);

		const float ff[] = {data.template soa_view<0>()[i], data.template soa_view<1>()[i], data.template soa_view<2>()[i]};
		REQUIRE(ff[0] == f[0]);
		REQUIRE(ff[1] == f[1]);
		REQUIRE(ff[2] == f[2]);
	}
}

template <>
void TestDataLayoutSoA<DummySoA>() {
	constexpr uint32_t N = 100;
	cnt::sarray<DummySoA, N> data{};

	for (uint32_t i = 0; i < N; ++i) {
		float f[] = {(float)(i + 1), (float)(i + 2), (float)(i + 3)};
		data[i] = {f[0], f[1], true, f[2]};

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
	}

	// Make sure that all values are correct (e.g. that they were not overriden by one of the loop iteration)
	for (uint32_t i = 0; i < N; ++i) {
		const float f[] = {(float)(i + 1), (float)(i + 2), (float)(i + 3)};

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

#if GAIA_COMP_ID_PROBING
TEST_CASE("ComponentId internal map") {
	for (uint32_t i = 1; i < 10; ++i) {
		cnt::darray<ecs::ComponentId> data{};
		data.resize(i * 10);
		for (auto& comp: data)
			comp = ecs::ComponentIdBad;

		const ecs::ComponentId comp0 = 1000000;

		// Fill the data array
		for (uint32_t j = 0; j < data.size(); ++j) {
			ecs::set_comp_idx({data.data(), data.size()}, comp0 + j);
		}

		// Get indices
		cnt::set<ecs::ChunkDataOffset> indices;
		for (uint32_t j = 0; j < data.size(); ++j) {
			const auto idx = ecs::get_comp_idx({data.data(), data.size()}, comp0 + j);
			REQUIRE_FALSE(indices.contains(idx));
		}

		// All component ids must be found
		for (uint32_t j = 0; j < data.size(); ++j) {
			const bool has = ecs::has_comp_idx({data.data(), data.size()}, comp0 + j);
			REQUIRE(has);
		}

		// Following component ids must be not found because we didn't add them
		for (uint32_t j = 0; j < data.size(); ++j) {
			const bool has = ecs::has_comp_idx({data.data(), data.size()}, comp0 - 1 - j);
			REQUIRE_FALSE(has);
		}
	}
}
#endif

TEST_CASE("Entity - valid") {
	ecs::World w;
	{
		auto e = w.add();
		REQUIRE(w.valid(e));
		w.del(e);
		REQUIRE_FALSE(w.valid(e));
	}
	{
		auto e = w.add();
		REQUIRE(w.valid(e));
		w.del(e);
		REQUIRE_FALSE(w.valid(e));
	}
}

TEST_CASE("Entity - has") {
	ecs::World w;
	{
		auto e = w.add();
		REQUIRE(w.has(e));
		w.del(e);
		REQUIRE_FALSE(w.has(e));
	}
	{
		auto e = w.add();
		REQUIRE(w.has(e));
		w.del(e);
		REQUIRE_FALSE(w.has(e));
	}
}

TEST_CASE("EntityNull") {
	REQUIRE_FALSE(ecs::Entity{} == ecs::EntityNull);

	REQUIRE(ecs::EntityNull == ecs::EntityNull);
	REQUIRE_FALSE(ecs::EntityNull != ecs::EntityNull);

	ecs::World w;
	REQUIRE_FALSE(w.valid(ecs::EntityNull));

	auto e = w.add();
	REQUIRE(e != ecs::EntityNull);
	REQUIRE(ecs::EntityNull != e);
	REQUIRE_FALSE(e == ecs::EntityNull);
	REQUIRE_FALSE(ecs::EntityNull == e);
}

TEST_CASE("Add - no components") {
	ecs::World w;

	auto verify = [](ecs::Entity e, uint32_t id) {
		const bool ok = e.id() == id && e.gen() == 0;
		REQUIRE(ok);
	};
	auto create = [&](uint32_t id) {
		auto e = w.add();
		verify(e, id);
		return e;
	};

	const uint32_t N = 10'000;
	for (uint32_t i = 0; i < N; ++i)
		create(i);
	for (uint32_t i = 0; i < N; ++i)
		verify(w.get(i), i);
}

TEST_CASE("Add - 1 component") {
	ecs::World w;

	auto create = [&](uint32_t id) {
		auto e = w.add();
		const bool ok = e.id() == id && e.gen() == 0;
		REQUIRE(ok);

		w.add<Int3>(e, {id, id, id});
		REQUIRE(w.has<Int3>(e));
		auto val = w.get<Int3>(e);
		REQUIRE(val.x == id);
		REQUIRE(val.y == id);
		REQUIRE(val.z == id);
	};

	const uint32_t N = 10'000;
	for (uint32_t i = 0; i < N; ++i)
		create(i);
}

namespace dummy {
	struct Position {
		float x;
		float y;
		float z;
	};
} // namespace dummy

TEST_CASE("Add - namespaces") {
	ecs::World w;
	auto e = w.add();
	w.add<Position>(e, {1, 1, 1});
	w.add<dummy::Position>(e, {2, 2, 2});

	REQUIRE(w.has<Position>(e));
	REQUIRE(w.has<dummy::Position>(e));

	{
		auto p1 = w.get<Position>(e);
		REQUIRE(p1.x == 1.f);
		REQUIRE(p1.y == 1.f);
		REQUIRE(p1.z == 1.f);
	}
	{
		auto p2 = w.get<dummy::Position>(e);
		REQUIRE(p2.x == 2.f);
		REQUIRE(p2.y == 2.f);
		REQUIRE(p2.z == 2.f);
	}
}

TEST_CASE("Add - many components") {
	ecs::World w;

	auto create = [&](uint32_t id) {
		auto e = w.add();
		const bool ok = e.id() == id && e.gen() == 0;
		REQUIRE(ok);

		w.add<Int3>(e, {3, 3, 3});
		w.add<Position>(e, {1, 1, 1});
		w.add<Empty>(e);
		w.add<Else>(e, {true});
		w.add<Rotation>(e, {2, 2, 2, 2});
		w.add<Scale>(e, {4, 4, 4});

		REQUIRE(w.has<Int3>(e));
		REQUIRE(w.has<Position>(e));
		REQUIRE(w.has<Empty>(e));
		REQUIRE(w.has<Rotation>(e));
		REQUIRE(w.has<Scale>(e));

		{
			auto val = w.get<Int3>(e);
			REQUIRE(val.x == 3);
			REQUIRE(val.y == 3);
			REQUIRE(val.z == 3);
		}
		{
			auto val = w.get<Position>(e);
			REQUIRE(val.x == 1.f);
			REQUIRE(val.y == 1.f);
			REQUIRE(val.z == 1.f);
		}
		{
			auto val = w.get<Rotation>(e);
			REQUIRE(val.x == 2.f);
			REQUIRE(val.y == 2.f);
			REQUIRE(val.z == 2.f);
			REQUIRE(val.w == 2.f);
		}
		{
			auto val = w.get<Scale>(e);
			REQUIRE(val.x == 4.f);
			REQUIRE(val.y == 4.f);
			REQUIRE(val.z == 4.f);
		}
	};

	const uint32_t N = 10'000;
	for (uint32_t i = 0; i < N; ++i)
		create(i);
}

TEST_CASE("AddAndDel_entity - no components") {
	ecs::World w;

	auto verify = [](ecs::Entity e, uint32_t id, uint32_t genExpected) {
		const bool ok = e.id() == id && e.gen() == genExpected;
		REQUIRE(ok);
	};
	auto create = [&](uint32_t id) {
		auto e = w.add();
		verify(e, id, 0);
		return e;
	};
	auto remove = [&](ecs::Entity e) {
		w.del(e);
		verify(w.get(e.id()), e.id(), 1);
		const bool isEntityValid = w.valid(e);
		REQUIRE_FALSE(isEntityValid);
	};

	// 10,000 picked so we create enough entites that they overflow
	// into another chunk
	const uint32_t N = 10'000;
	cnt::darr<ecs::Entity> arr;
	arr.reserve(N);

	// Create entities
	for (uint32_t i = 0; i < N; ++i)
		arr.push_back(create(i));
	// Verify ids are still valid
	for (uint32_t i = 0; i < N; ++i)
		verify(w.get(i), i, 0);
	// Remove entities
	for (uint32_t i = 0; i < N; ++i)
		remove(arr[i]);
}

TEST_CASE("AddAndDel_entity - 1 component") {
	ecs::World w;

	auto create = [&](uint32_t id) {
		auto e = w.add();
		w.add<Int3>(e, {id, id, id});
		const bool ok = e.id() == id && e.gen() == 0;
		REQUIRE(ok);
		auto pos = w.get<Int3>(e);
		REQUIRE(pos.x == id);
		REQUIRE(pos.y == id);
		REQUIRE(pos.z == id);
		return e;
	};
	auto remove = [&](ecs::Entity e) {
		w.del(e);
		auto de = w.get(e.id());
		const bool ok = de.gen() == e.gen() + 1;
		REQUIRE(ok);
		const bool isEntityValid = w.valid(e);
		REQUIRE_FALSE(isEntityValid);
	};

	// 10,000 picked so we create enough entites that they overflow
	// into another chunk
	const uint32_t N = 10'000;
	cnt::darr<ecs::Entity> arr;
	arr.reserve(N);

	for (uint32_t i = 0; i < N; ++i)
		arr.push_back(create(i));
	for (uint32_t i = 0; i < N; ++i)
		remove(arr[i]);
}

template <typename TQuery>
void Test_Query_QueryResult() {
	ecs::World w;

	auto create = [&](uint32_t i) {
		auto e = w.add();
		w.add<Position>(e, {(float)i, (float)i, (float)i});
	};

	const uint32_t N = 10'000;
	for (uint32_t i = 0; i < N; ++i)
		create(i);

	constexpr bool UseCachedQuery = std::is_same_v<TQuery, ecs::Query>;
	auto q1 = w.query<UseCachedQuery>().template all<Position>();
	auto q2 = w.query<UseCachedQuery>().template all<Rotation>();
	auto q3 = w.query<UseCachedQuery>().template all<Position, Rotation>();

	{
		cnt::darr<ecs::Entity> arr;
		q1.arr(arr);
		GAIA_ASSERT(arr.size() == N);
		for (uint32_t i = 0; i < arr.size(); ++i)
			REQUIRE(arr[i].id() == i);
	}
	{
		cnt::darr<Position> arr;
		q1.arr(arr);
		GAIA_ASSERT(arr.size() == N);
		for (uint32_t i = 0; i < arr.size(); ++i) {
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
}

TEST_CASE("Query - QueryResult") {
	SECTION("Cached query") {
		Test_Query_QueryResult<ecs::Query>();
	}
	SECTION("Non-cached query") {
		Test_Query_QueryResult<ecs::QueryUncached>();
	}
}

template <typename TQuery>
void Test_Query_QueryResult_Complex() {
	ecs::World w;

	auto create = [&](uint32_t i) {
		auto e = w.add();
		w.add<Position>(e, {(float)i, (float)i, (float)i});
		w.add<Scale>(e, {(float)i * 2, (float)i * 2, (float)i * 2});
		if (i % 2 == 0)
			w.add<Something>(e, {true});
	};

	const uint32_t N = 10'000;
	for (uint32_t i = 0; i < N; ++i)
		create(i);

	constexpr bool UseCachedQuery = std::is_same_v<TQuery, ecs::Query>;
	auto q1 = w.query<UseCachedQuery>().template all<Position>();
	auto q2 = w.query<UseCachedQuery>().template all<Rotation>();
	auto q3 = w.query<UseCachedQuery>().template all<Position, Rotation>();
	auto q4 = w.query<UseCachedQuery>().template all<Position, Scale>();
	auto q5 = w.query<UseCachedQuery>().template all<Position, Scale, Something>();

	{
		cnt::darr<ecs::Entity> ents;
		q1.arr(ents);
		cnt::darr<Position> arr;
		q1.arr(arr);

		REQUIRE(ents.size() == arr.size());
		REQUIRE(arr.size() == N);

		for (uint32_t i = 0; i < arr.size(); ++i) {
			const auto& pos = arr[i];
			const auto val = ents[i].id();
			REQUIRE(pos.x == (float)val);
			REQUIRE(pos.y == (float)val);
			REQUIRE(pos.z == (float)val);
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
		cnt::darr<ecs::Entity> ents;
		q4.arr(ents);
		cnt::darr<Position> arr;
		q4.arr(arr);
		REQUIRE(ents.size() == arr.size());
		REQUIRE(ents.size() == N);

		for (uint32_t i = 0; i < arr.size(); ++i) {
			const auto& pos = arr[i];
			const auto val = ents[i].id();
			REQUIRE(pos.x == (float)val);
			REQUIRE(pos.y == (float)val);
			REQUIRE(pos.z == (float)val);
		}
	}
	{
		cnt::darr<ecs::Entity> ents;
		q4.arr(ents);
		cnt::darr<Scale> arr;
		q4.arr(arr);
		REQUIRE(ents.size() == arr.size());
		REQUIRE(ents.size() == N);

		for (uint32_t i = 0; i < arr.size(); ++i) {
			const auto& s = arr[i];
			const auto val = ents[i].id() * 2;
			REQUIRE(s.x == (float)val);
			REQUIRE(s.y == (float)val);
			REQUIRE(s.z == (float)val);
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
		cnt::darr<ecs::Entity> ents;
		q5.arr(ents);
		cnt::darr<Position> arr;
		q5.arr(arr);
		REQUIRE(ents.size() == arr.size());
		REQUIRE(ents.size() == N / 2);

		for (uint32_t i = 0; i < arr.size(); ++i) {
			const auto& pos = arr[i];
			const auto val = ents[i].id();
			REQUIRE(pos.x == (float)val);
			REQUIRE(pos.y == (float)val);
			REQUIRE(pos.z == (float)val);
		}
	}
	{
		cnt::darr<ecs::Entity> ents;
		q5.arr(ents);
		cnt::darr<Scale> arr;
		q5.arr(arr);
		REQUIRE(ents.size() == arr.size());
		REQUIRE(ents.size() == N / 2);

		for (uint32_t i = 0; i < arr.size(); ++i) {
			const auto& s = arr[i];
			const auto val = ents[i].id() * 2;
			REQUIRE(s.x == (float)val);
			REQUIRE(s.y == (float)val);
			REQUIRE(s.z == (float)val);
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

template <typename TQuery>
void Test_Query_Equality() {
	SECTION("2 components") {
		ecs::World w;

		auto create = [&]() {
			auto e = w.add();
			w.add<Position>(e);
			w.add<Rotation>(e);
		};

		const uint32_t N = 100;
		for (uint32_t i = 0; i < N; ++i)
			create();

		constexpr bool UseCachedQuery = std::is_same_v<TQuery, ecs::Query>;
		auto qq1 = w.query<UseCachedQuery>().template all<Position, Rotation>();
		auto qq2 = w.query<UseCachedQuery>().template all<Rotation, Position>();
		REQUIRE(qq1.count() == qq2.count());

		cnt::darr<ecs::Entity> ents1, ents2;
		qq1.arr(ents1);
		qq2.arr(ents2);
		REQUIRE(ents1.size() == ents2.size());

		uint32_t i = 0;
		for (auto e: ents1) {
			REQUIRE(e == ents2[i]);
			++i;
		}
	}
	SECTION("4 components") {
		ecs::World w;

		ecs::Query qq1 = w.query().all<Position, Rotation, Acceleration, Something>();
		ecs::Query qq2 = w.query().all<Rotation, Something, Position, Acceleration>();
		REQUIRE(qq1.count() == qq2.count());

		cnt::darr<ecs::Entity> ents1, ents2;
		qq1.arr(ents1);
		qq2.arr(ents2);
		REQUIRE(ents1.size() == ents2.size());

		uint32_t i = 0;
		for (auto e: ents1) {
			REQUIRE(e == ents2[i]);
			++i;
		}
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
	ecs::World w;

	auto create = [&](uint32_t id) {
		auto e = w.add();
		const bool ok = e.id() == id && e.gen() == 0;
		REQUIRE(ok);
		w.add<Position>(e);
		return e;
	};

	// 10,000 picked so we create enough entites that they overflow into another chunk
	const uint32_t N = 10'000;
	cnt::darr<ecs::Entity> arr;
	arr.reserve(N);

	for (uint32_t i = 0; i < N; ++i)
		arr.push_back(create(i));

	SECTION("State validity") {
		w.enable(arr[0], false);
		REQUIRE_FALSE(w.enabled(arr[0]));
		w.enable(arr[0], true);
		REQUIRE(w.enabled(arr[0]));
	}

	SECTION("State persistance") {
		w.enable(arr[0], false);
		w.del<Position>(arr[0]);
		REQUIRE_FALSE(w.enabled(arr[0]));

		w.enable(arr[0], true);
		w.add<Position>(arr[0]);
		REQUIRE(w.enabled(arr[0]));
	}

	ecs::Query q = w.query().all<Position>();

	auto checkQuery = [&q](uint32_t expectedCountAll, uint32_t expectedCountEnabled, uint32_t expectedCountDisabled) {
		{
			uint32_t cnt = 0;
			q.each([&]([[maybe_unused]] ecs::IteratorAll iter) {
				const uint32_t cExpected = iter.size();
				uint32_t c = 0;
				GAIA_EACH(iter)++ c;
				REQUIRE(c == cExpected);
				cnt += c;
			});
			REQUIRE(cnt == expectedCountAll);

			cnt = q.count(ecs::Constraints::AcceptAll);
			REQUIRE(cnt == expectedCountAll);
		}
		{
			uint32_t cnt = 0;
			q.each([&]([[maybe_unused]] ecs::Iterator iter) {
				const uint32_t cExpected = iter.size();
				uint32_t c = 0;
				GAIA_EACH(iter) {
					REQUIRE(iter.enabled(i));
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
			q.each([&]([[maybe_unused]] ecs::IteratorDisabled iter) {
				const uint32_t cExpected = iter.size();
				uint32_t c = 0;
				GAIA_EACH(iter) {
					REQUIRE(!iter.enabled(i));
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
		w.enable(arr[1000], false);
		checkQuery(N, N - 1, 1);
	}

	SECTION("Enable vs query") {
		w.enable(arr[1000], true);
		checkQuery(N, N, 0);
	}

	SECTION("Disable vs query") {
		w.enable(arr[1], false);
		w.enable(arr[1000], false);
		w.enable(arr[2000], false);
		w.enable(arr[9990], false);
		checkQuery(N, N - 4, 4);
	}

	SECTION("Enable vs query") {
		w.enable(arr[1], true);
		w.enable(arr[1000], true);
		w.enable(arr[2000], true);
		w.enable(arr[9990], true);
		checkQuery(N, N, 0);
	}
}

TEST_CASE("Add - generic") {
	ecs::World w;

	{
		auto e = w.add();
		w.add<Position>(e);
		w.add<Acceleration>(e);

		REQUIRE(w.has<Position>(e));
		REQUIRE(w.has<Acceleration>(e));
		REQUIRE_FALSE(w.has<ecs::uni<Position>>(e));
		REQUIRE_FALSE(w.has<ecs::uni<Acceleration>>(e));
	}

	{
		auto e = w.add();
		w.add<Position>(e, {1, 2, 3});
		w.add<Acceleration>(e, {4, 5, 6});

		REQUIRE(w.has<Position>(e));
		REQUIRE(w.has<Acceleration>(e));
		REQUIRE_FALSE(w.has<ecs::uni<Position>>(e));
		REQUIRE_FALSE(w.has<ecs::uni<Acceleration>>(e));

		auto p = w.get<Position>(e);
		REQUIRE(p.x == 1.f);
		REQUIRE(p.y == 2.f);
		REQUIRE(p.z == 3.f);

		auto a = w.get<Acceleration>(e);
		REQUIRE(a.x == 4.f);
		REQUIRE(a.y == 5.f);
		REQUIRE(a.z == 6.f);
	}
}

// TEST_CASE("Add - from query") {
// 	ecs::World w;

// 	cnt::sarray<ecs::Entity, 5> ents;
// 	for (auto& e: ents)
// 		e = w.add();

// 	for (uint32_t i = 0; i < ents.size() - 1; ++i)
// 		w.add<Position>(ents[i], {(float)i, (float)i, (float)i});

// 	ecs::Query q;
// 	q.all<Position>();
// 	w.add<Acceleration>(q, {1.f, 2.f, 3.f});

// 	for (uint32_t i = 0; i < ents.size() - 1; ++i) {
// 		REQUIRE(w.has<Position>(ents[i]));
// 		REQUIRE(w.has<Acceleration>(ents[i]));

// 		Position p;
// 		w.get<Position>(ents[i], p);
// 		REQUIRE(p.x == (float)i);
// 		REQUIRE(p.y == (float)i);
// 		REQUIRE(p.z == (float)i);

// 		Acceleration a;
// 		w.get<Acceleration>(ents[i], a);
// 		REQUIRE(a.x == 1.f);
// 		REQUIRE(a.y == 2.f);
// 		REQUIRE(a.z == 3.f);
// 	}

// 	{
// 		REQUIRE_FALSE(w.has<Position>(ents[4]));
// 		REQUIRE_FALSE(w.has<Acceleration>(ents[4]));
// 	}
// }

TEST_CASE("Add - chunk") {
	{
		ecs::World w;
		auto e = w.add();
		w.add<ecs::uni<Position>>(e);
		w.add<ecs::uni<Acceleration>>(e);

		REQUIRE_FALSE(w.has<Position>(e));
		REQUIRE_FALSE(w.has<Acceleration>(e));
		REQUIRE(w.has<ecs::uni<Position>>(e));
		REQUIRE(w.has<ecs::uni<Acceleration>>(e));
	}

	{
		ecs::World w;
		auto e = w.add();

		// Add Position unique component
		w.add<ecs::uni<Position>>(e, {1, 2, 3});
		REQUIRE(w.has<ecs::uni<Position>>(e));
		REQUIRE_FALSE(w.has<Position>(e));
		{
			auto p = w.get<ecs::uni<Position>>(e);
			REQUIRE(p.x == 1.f);
			REQUIRE(p.y == 2.f);
			REQUIRE(p.z == 3.f);
		}
		// Add Acceleration unique component.
		// This moves "e" to a new archetype.
		w.add<ecs::uni<Acceleration>>(e, {4, 5, 6});
		REQUIRE(w.has<ecs::uni<Position>>(e));
		REQUIRE(w.has<ecs::uni<Acceleration>>(e));
		REQUIRE_FALSE(w.has<Position>(e));
		REQUIRE_FALSE(w.has<Acceleration>(e));
		{
			auto a = w.get<ecs::uni<Acceleration>>(e);
			REQUIRE(a.x == 4.f);
			REQUIRE(a.y == 5.f);
			REQUIRE(a.z == 6.f);
		}
		{
			// Because "e" was moved to a new archetype nobody ever set the value.
			// Therefore, it is garbage and won't match the original chunk.
			auto p = w.get<ecs::uni<Position>>(e);
			REQUIRE_FALSE(p.x == 1.f);
			REQUIRE_FALSE(p.y == 2.f);
			REQUIRE_FALSE(p.z == 3.f);
		}
	}
}

TEST_CASE("del - generic") {
	{
		ecs::World w;
		auto e1 = w.add();

		{
			w.add<Position>(e1);
			w.del<Position>(e1);
			REQUIRE_FALSE(w.has<Position>(e1));
		}
		{
			w.add<Rotation>(e1);
			w.del<Rotation>(e1);
			REQUIRE_FALSE(w.has<Rotation>(e1));
		}
	}
	{
		ecs::World w;
		auto e1 = w.add();
		{
			w.add<Position>(e1);
			w.add<Rotation>(e1);

			{
				w.del<Position>(e1);
				REQUIRE_FALSE(w.has<Position>(e1));
				REQUIRE(w.has<Rotation>(e1));
			}
			{
				w.del<Rotation>(e1);
				REQUIRE_FALSE(w.has<Position>(e1));
				REQUIRE_FALSE(w.has<Rotation>(e1));
			}
		}
		{
			w.add<Rotation>(e1);
			w.add<Position>(e1);
			{
				w.del<Position>(e1);
				REQUIRE_FALSE(w.has<Position>(e1));
				REQUIRE(w.has<Rotation>(e1));
			}
			{
				w.del<Rotation>(e1);
				REQUIRE_FALSE(w.has<Position>(e1));
				REQUIRE_FALSE(w.has<Rotation>(e1));
			}
		}
	}
}

TEST_CASE("del - chunk") {
	ecs::World w;
	auto e1 = w.add();

	{
		w.add<ecs::uni<Position>>(e1);
		w.add<ecs::uni<Acceleration>>(e1);
		{
			w.del<ecs::uni<Position>>(e1);
			REQUIRE_FALSE(w.has<ecs::uni<Position>>(e1));
			REQUIRE(w.has<ecs::uni<Acceleration>>(e1));
		}
		{
			w.del<ecs::uni<Acceleration>>(e1);
			REQUIRE_FALSE(w.has<ecs::uni<Position>>(e1));
			REQUIRE_FALSE(w.has<ecs::uni<Acceleration>>(e1));
		}
	}

	{
		w.add<ecs::uni<Acceleration>>(e1);
		w.add<ecs::uni<Position>>(e1);
		{
			w.del<ecs::uni<Position>>(e1);
			REQUIRE_FALSE(w.has<ecs::uni<Position>>(e1));
			REQUIRE(w.has<ecs::uni<Acceleration>>(e1));
		}
		{
			w.del<ecs::uni<Acceleration>>(e1);
			REQUIRE_FALSE(w.has<ecs::uni<Position>>(e1));
			REQUIRE_FALSE(w.has<ecs::uni<Acceleration>>(e1));
		}
	}
}

TEST_CASE("del - generic, chunk") {
	ecs::World w;
	auto e1 = w.add();

	{
		w.add<Position>(e1);
		w.add<Acceleration>(e1);
		w.add<ecs::uni<Position>>(e1);
		w.add<ecs::uni<Acceleration>>(e1);
		{
			w.del<Position>(e1);
			REQUIRE_FALSE(w.has<Position>(e1));
			REQUIRE(w.has<Acceleration>(e1));
			REQUIRE(w.has<ecs::uni<Position>>(e1));
			REQUIRE(w.has<ecs::uni<Acceleration>>(e1));
		}
		{
			w.del<Acceleration>(e1);
			REQUIRE_FALSE(w.has<Position>(e1));
			REQUIRE_FALSE(w.has<Acceleration>(e1));
			REQUIRE(w.has<ecs::uni<Position>>(e1));
			REQUIRE(w.has<ecs::uni<Acceleration>>(e1));
		}
		{
			w.del<ecs::uni<Acceleration>>(e1);
			REQUIRE_FALSE(w.has<Position>(e1));
			REQUIRE_FALSE(w.has<Acceleration>(e1));
			REQUIRE(w.has<ecs::uni<Position>>(e1));
			REQUIRE_FALSE(w.has<ecs::uni<Acceleration>>(e1));
		}
	}
}

TEST_CASE("Usage 1 - simple query, 0 component") {
	ecs::World w;

	auto e = w.add();

	auto qa = w.query().all<const Acceleration>();
	auto qp = w.query().all<const Position>();

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

	auto e1 = w.add(e);
	auto e2 = w.add(e);
	auto e3 = w.add(e);

	{
		uint32_t cnt = 0;
		qp.each([&]([[maybe_unused]] const Position&) {
			++cnt;
		});
		REQUIRE(cnt == 0);
	}

	w.del(e1);

	{
		uint32_t cnt = 0;
		qp.each([&]([[maybe_unused]] const Position&) {
			++cnt;
		});
		REQUIRE(cnt == 0);
	}

	w.del(e2);
	w.del(e3);
	w.del(e);

	{
		uint32_t cnt = 0;
		qp.each([&]([[maybe_unused]] const Position&) {
			++cnt;
		});
		REQUIRE(cnt == 0);
	}
}

TEST_CASE("Usage 1 - simple query, 1 component") {
	ecs::World w;

	auto e = w.add();
	w.add<Position>(e);

	auto qa = w.query().all<const Acceleration>();
	auto qp = w.query().all<const Position>();

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

	auto e1 = w.add(e);
	auto e2 = w.add(e);
	auto e3 = w.add(e);

	{
		uint32_t cnt = 0;
		qp.each([&]([[maybe_unused]] const Position&) {
			++cnt;
		});
		REQUIRE(cnt == 4);
	}

	w.del(e1);

	{
		uint32_t cnt = 0;
		qp.each([&]([[maybe_unused]] const Position&) {
			++cnt;
		});
		REQUIRE(cnt == 3);
	}

	w.del(e2);
	w.del(e3);
	w.del(e);

	{
		uint32_t cnt = 0;
		qp.each([&]([[maybe_unused]] const Position&) {
			++cnt;
		});
		REQUIRE(cnt == 0);
	}
}

TEST_CASE("Usage 1 - simple query, 1 unique component") {
	ecs::World w;

	auto e = w.add();
	w.add<ecs::uni<Position>>(e);

	auto q = w.query().all<ecs::uni<const Position>>();
	auto qq = w.query().all<const Position>();

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
		q.each([&]([[maybe_unused]] ecs::Iterator iter) {
			++cnt;
		});
		REQUIRE(cnt == 1);
	}

	auto e1 = w.add(e);
	auto e2 = w.add(e);
	auto e3 = w.add(e);

	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] ecs::Iterator iter) {
			++cnt;
		});
		REQUIRE(cnt == 1);
	}

	w.del(e1);

	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] ecs::Iterator iter) {
			++cnt;
		});
		REQUIRE(cnt == 1);
	}

	w.del(e2);
	w.del(e3);
	w.del(e);

	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] ecs::Iterator iter) {
			++cnt;
		});
		REQUIRE(cnt == 0);
	}
}

TEST_CASE("Usage 2 - simple query, many components") {
	ecs::World w;

	auto e1 = w.add();
	w.add<Position>(e1, {});
	w.add<Acceleration>(e1, {});
	w.add<Else>(e1, {});
	auto e2 = w.add();
	w.add<Rotation>(e2, {});
	w.add<Scale>(e2, {});
	w.add<Else>(e2, {});
	auto e3 = w.add();
	w.add<Position>(e3, {});
	w.add<Acceleration>(e3, {});
	w.add<Scale>(e3, {});

	{
		uint32_t cnt = 0;
		auto q = w.query().all<const Position>();
		q.each([&]([[maybe_unused]] const Position&) {
			++cnt;
		});
		REQUIRE(cnt == 2);
	}
	{
		uint32_t cnt = 0;
		auto q = w.query().all<const Acceleration>();
		q.each([&]([[maybe_unused]] const Acceleration&) {
			++cnt;
		});
		REQUIRE(cnt == 2);
	}
	{
		uint32_t cnt = 0;
		auto q = w.query().all<const Rotation>();
		q.each([&]([[maybe_unused]] const Rotation&) {
			++cnt;
		});
		REQUIRE(cnt == 1);
	}
	{
		uint32_t cnt = 0;
		auto q = w.query().all<const Scale>();
		q.each([&]([[maybe_unused]] const Scale&) {
			++cnt;
		});
		REQUIRE(cnt == 2);
	}
	{
		uint32_t cnt = 0;
		auto q = w.query().all<const Position, const Acceleration>();
		q.each([&]([[maybe_unused]] const Position&, [[maybe_unused]] const Acceleration&) {
			++cnt;
		});
		REQUIRE(cnt == 2);
	}
	{
		uint32_t cnt = 0;
		auto q = w.query().all<const Position, const Scale>();
		q.each([&]([[maybe_unused]] const Position&, [[maybe_unused]] const Scale&) {
			++cnt;
		});
		REQUIRE(cnt == 1);
	}
	{
		ecs::Query q = w.query().any<Position, Acceleration>();

		uint32_t cnt = 0;
		q.each([&](ecs::Iterator iter) {
			++cnt;

			const bool ok1 = iter.has<Position>() || iter.has<Acceleration>();
			REQUIRE(ok1);
			const bool ok2 = iter.has<Acceleration>() || iter.has<Position>();
			REQUIRE(ok2);
		});
		REQUIRE(cnt == 2);
	}
	{
		ecs::Query q = w.query().any<Position, Acceleration>().all<Scale>();

		uint32_t cnt = 0;
		q.each([&](ecs::Iterator iter) {
			++cnt;

			REQUIRE(iter.size() == 1);

			const bool ok1 = iter.has<Position>() || iter.has<Acceleration>();
			REQUIRE(ok1);
			const bool ok2 = iter.has<Acceleration>() || iter.has<Position>();
			REQUIRE(ok2);
		});
		REQUIRE(cnt == 1);
	}
	{
		ecs::Query q = w.query().any<Position, Acceleration>().none<Scale>();

		uint32_t cnt = 0;
		q.each([&](ecs::Iterator iter) {
			++cnt;

			REQUIRE(iter.size() == 1);
		});
		REQUIRE(cnt == 1);
	}
}

TEST_CASE("Usage 2 - simple query, many unique components") {
	ecs::World w;

	auto e1 = w.add();
	w.add<ecs::uni<Position>>(e1, {});
	w.add<ecs::uni<Acceleration>>(e1, {});
	w.add<ecs::uni<Else>>(e1, {});
	auto e2 = w.add();
	w.add<ecs::uni<Rotation>>(e2, {});
	w.add<ecs::uni<Scale>>(e2, {});
	w.add<ecs::uni<Else>>(e2, {});
	auto e3 = w.add();
	w.add<ecs::uni<Position>>(e3, {});
	w.add<ecs::uni<Acceleration>>(e3, {});
	w.add<ecs::uni<Scale>>(e3, {});

	{
		uint32_t cnt = 0;
		auto q = w.query().all<ecs::uni<Position>>();
		q.each([&]([[maybe_unused]] ecs::Iterator iter) {
			++cnt;
		});
		REQUIRE(cnt == 2);
	}
	{
		uint32_t cnt = 0;
		auto q = w.query().all<ecs::uni<Acceleration>>();
		q.each([&]([[maybe_unused]] ecs::Iterator iter) {
			++cnt;
		});
		REQUIRE(cnt == 2);
	}
	{
		uint32_t cnt = 0;
		auto q = w.query().all<ecs::uni<Rotation>>();
		q.each([&]([[maybe_unused]] ecs::Iterator iter) {
			++cnt;
		});
		REQUIRE(cnt == 1);
	}
	{
		uint32_t cnt = 0;
		auto q = w.query().all<ecs::uni<Else>>();
		q.each([&]([[maybe_unused]] ecs::Iterator iter) {
			++cnt;
		});
		REQUIRE(cnt == 2);
	}
	{
		uint32_t cnt = 0;
		auto q = w.query().all<ecs::uni<Scale>>();
		q.each([&]([[maybe_unused]] ecs::Iterator iter) {
			++cnt;
		});
		REQUIRE(cnt == 2);
	}
	{
		auto q = w.query().any<ecs::uni<Position>, ecs::uni<Acceleration>>();

		uint32_t cnt = 0;
		q.each([&](ecs::Iterator iter) {
			++cnt;

			const bool ok1 = iter.has<ecs::uni<Position>>() || iter.has<ecs::uni<Acceleration>>();
			REQUIRE(ok1);
			const bool ok2 = iter.has<ecs::uni<Acceleration>>() || iter.has<ecs::uni<Position>>();
			REQUIRE(ok2);
		});
		REQUIRE(cnt == 2);
	}
	{
		auto q = w.query().any<ecs::uni<Position>, ecs::uni<Acceleration>>().all<ecs::uni<Scale>>();

		uint32_t cnt = 0;
		q.each([&](ecs::Iterator iter) {
			++cnt;

			REQUIRE(iter.size() == 1);

			const bool ok1 = iter.has<ecs::uni<Position>>() || iter.has<ecs::uni<Acceleration>>();
			REQUIRE(ok1);
			const bool ok2 = iter.has<ecs::uni<Acceleration>>() || iter.has<ecs::uni<Position>>();
			REQUIRE(ok2);
		});
		REQUIRE(cnt == 1);
	}
	{
		auto q = w.query().any<ecs::uni<Position>, ecs::uni<Acceleration>>().none<ecs::uni<Scale>>();

		uint32_t cnt = 0;
		q.each([&](ecs::Iterator iter) {
			++cnt;

			REQUIRE(iter.size() == 1);
		});
		REQUIRE(cnt == 1);
	}
}

TEST_CASE("Set - generic") {
	ecs::World w;

	constexpr uint32_t N = 100;
	cnt::darr<ecs::Entity> arr;
	arr.reserve(N);

	for (uint32_t i = 0; i < N; ++i) {
		const auto ent = w.add();
		REQUIRE(ent.id() == i);
		arr.push_back(ent);
		w.add<Rotation>(ent, {});
		w.add<Scale>(ent, {});
		w.add<Else>(ent, {});
	}

	// Default values
	for (const auto ent: arr) {
		auto r = w.get<Rotation>(ent);
		REQUIRE(r.x == 0.f);
		REQUIRE(r.y == 0.f);
		REQUIRE(r.z == 0.f);
		REQUIRE(r.w == 0.f);

		auto s = w.get<Scale>(ent);
		REQUIRE(s.x == 0.f);
		REQUIRE(s.y == 0.f);
		REQUIRE(s.z == 0.f);

		auto e = w.get<Else>(ent);
		REQUIRE(e.value == false);
	}

	// Modify values
	{
		ecs::Query q = w.query().all<Rotation, Scale, Else>();

		q.each([&](ecs::Iterator iter) {
			auto rotationView = iter.view_mut<Rotation>();
			auto scaleView = iter.view_mut<Scale>();
			auto elseView = iter.view_mut<Else>();

			GAIA_EACH(iter) {
				rotationView[i] = {1, 2, 3, 4};
				scaleView[i] = {11, 22, 33};
				elseView[i] = {true};
			}
		});

		for (const auto ent: arr) {
			auto r = w.get<Rotation>(ent);
			REQUIRE(r.x == 1.f);
			REQUIRE(r.y == 2.f);
			REQUIRE(r.z == 3.f);
			REQUIRE(r.w == 4.f);

			auto s = w.get<Scale>(ent);
			REQUIRE(s.x == 11.f);
			REQUIRE(s.y == 22.f);
			REQUIRE(s.z == 33.f);

			auto e = w.get<Else>(ent);
			REQUIRE(e.value == true);
		}
	}

	// Add one more component and check if the values are still fine after creating a new archetype
	{
		auto ent = w.add(arr[0]);
		w.add<Position>(ent, {5, 6, 7});

		auto r = w.get<Rotation>(ent);
		REQUIRE(r.x == 1.f);
		REQUIRE(r.y == 2.f);
		REQUIRE(r.z == 3.f);
		REQUIRE(r.w == 4.f);

		auto s = w.get<Scale>(ent);
		REQUIRE(s.x == 11.f);
		REQUIRE(s.y == 22.f);
		REQUIRE(s.z == 33.f);

		auto e = w.get<Else>(ent);
		REQUIRE(e.value == true);
	}
}

TEST_CASE("Set - generic & chunk") {
	ecs::World w;

	constexpr uint32_t N = 100;
	cnt::darr<ecs::Entity> arr;
	arr.reserve(N);

	for (uint32_t i = 0; i < N; ++i) {
		arr.push_back(w.add());
		w.add<Rotation>(arr.back(), {});
		w.add<Scale>(arr.back(), {});
		w.add<Else>(arr.back(), {});
		w.add<ecs::uni<Position>>(arr.back(), {});
	}

	// Default values
	for (const auto ent: arr) {
		auto r = w.get<Rotation>(ent);
		REQUIRE(r.x == 0.f);
		REQUIRE(r.y == 0.f);
		REQUIRE(r.z == 0.f);
		REQUIRE(r.w == 0.f);

		auto s = w.get<Scale>(ent);
		REQUIRE(s.x == 0.f);
		REQUIRE(s.y == 0.f);
		REQUIRE(s.z == 0.f);

		auto e = w.get<Else>(ent);
		REQUIRE(e.value == false);

		auto p = w.get<ecs::uni<Position>>(ent);
		REQUIRE(p.x == 0.f);
		REQUIRE(p.y == 0.f);
		REQUIRE(p.z == 0.f);
	}

	// Modify values
	{
		ecs::Query q = w.query().all<Rotation, Scale, Else>();

		q.each([&](ecs::Iterator iter) {
			auto rotationView = iter.view_mut<Rotation>();
			auto scaleView = iter.view_mut<Scale>();
			auto elseView = iter.view_mut<Else>();

			GAIA_EACH(iter) {
				rotationView[i] = {1, 2, 3, 4};
				scaleView[i] = {11, 22, 33};
				elseView[i] = {true};
			}
		});

		w.set<ecs::uni<Position>>(arr[0], {111, 222, 333});

		{
			Position p = w.get<ecs::uni<Position>>(arr[0]);
			REQUIRE(p.x == 111.f);
			REQUIRE(p.y == 222.f);
			REQUIRE(p.z == 333.f);
		}
		{
			for (const auto ent: arr) {
				auto r = w.get<Rotation>(ent);
				REQUIRE(r.x == 1.f);
				REQUIRE(r.y == 2.f);
				REQUIRE(r.z == 3.f);
				REQUIRE(r.w == 4.f);

				auto s = w.get<Scale>(ent);
				REQUIRE(s.x == 11.f);
				REQUIRE(s.y == 22.f);
				REQUIRE(s.z == 33.f);

				auto e = w.get<Else>(ent);
				REQUIRE(e.value == true);
			}
		}
		{
			auto p = w.get<ecs::uni<Position>>(arr[0]);
			REQUIRE(p.x == 111.f);
			REQUIRE(p.y == 222.f);
			REQUIRE(p.z == 333.f);
		}
	}
}

TEST_CASE("Components - non trivial") {
	ecs::World w;

	constexpr uint32_t N = 100;
	cnt::darr<ecs::Entity> arr;
	arr.reserve(N);

	for (uint32_t i = 0; i < N; ++i) {
		arr.push_back(w.add());
		w.add<StringComponent>(arr.back(), {});
		w.add<StringComponent2>(arr.back(), {});
		w.add<PositionNonTrivial>(arr.back(), {});
	}

	// Default values
	for (const auto ent: arr) {
		const auto& s1 = w.get<StringComponent>(ent);
		REQUIRE(s1.value.empty());

		{
			auto s2 = w.get<StringComponent2>(ent);
			REQUIRE(s2.value == StringComponent2DefaultValue);
		}
		{
			const auto& s2 = w.get<StringComponent2>(ent);
			REQUIRE(s2.value == StringComponent2DefaultValue);
		}

		const auto& p = w.get<PositionNonTrivial>(ent);
		REQUIRE(p.x == 1.f);
		REQUIRE(p.y == 2.f);
		REQUIRE(p.z == 3.f);
	}

	// Modify values
	{
		ecs::Query q = w.query().all<StringComponent, StringComponent2, PositionNonTrivial>();

		q.each([&](ecs::Iterator iter) {
			auto strView = iter.view_mut<StringComponent>();
			auto str2View = iter.view_mut<StringComponent2>();
			auto posView = iter.view_mut<PositionNonTrivial>();

			GAIA_EACH(iter) {
				strView[i] = {StringComponentDefaultValue};
				str2View[i].value = StringComponent2DefaultValue_2;
				posView[i] = {111, 222, 333};
			}
		});

		for (const auto ent: arr) {
			const auto& s1 = w.get<StringComponent>(ent);
			REQUIRE(s1.value == StringComponentDefaultValue);

			const auto& s2 = w.get<StringComponent2>(ent);
			REQUIRE(s2.value == StringComponent2DefaultValue_2);

			const auto& p = w.get<PositionNonTrivial>(ent);
			REQUIRE(p.x == 111.f);
			REQUIRE(p.y == 222.f);
			REQUIRE(p.z == 333.f);
		}
	}

	// Add one more component and check if the values are still fine after creating a new archetype
	{
		auto ent = w.add(arr[0]);
		w.add<Position>(ent, {5, 6, 7});

		const auto& s1 = w.get<StringComponent>(ent);
		REQUIRE(s1.value == StringComponentDefaultValue);

		const auto& s2 = w.get<StringComponent2>(ent);
		REQUIRE(s2.value == StringComponent2DefaultValue_2);

		const auto& p = w.get<PositionNonTrivial>(ent);
		REQUIRE(p.x == 111.f);
		REQUIRE(p.y == 222.f);
		REQUIRE(p.z == 333.f);
	}
}

TEST_CASE("CommandBuffer") {
	SECTION("Entity creation") {
		ecs::World w;
		ecs::CommandBuffer cb(w);

		const uint32_t N = 100;
		for (uint32_t i = 0; i < N; ++i) {
			[[maybe_unused]] auto tmp = cb.add();
		}

		cb.commit();

		for (uint32_t i = 0; i < N; ++i) {
			auto e = w.get(i);
			REQUIRE(e.id() == i);
		}
	}

	SECTION("Entity creation from another entity") {
		ecs::World w;
		ecs::CommandBuffer cb(w);

		auto mainEntity = w.add();

		const uint32_t N = 100;
		for (uint32_t i = 0; i < N; ++i) {
			[[maybe_unused]] auto tmp = cb.add(mainEntity);
		}

		cb.commit();

		for (uint32_t i = 0; i < N; ++i) {
			auto e = w.get(i + 1);
			REQUIRE(e.id() == i + 1);
		}
	}

	SECTION("Entity creation from another entity with a component") {
		ecs::World w;
		ecs::CommandBuffer cb(w);

		auto mainEntity = w.add();
		w.add<Position>(mainEntity, {1, 2, 3});

		[[maybe_unused]] auto tmp = cb.add(mainEntity);
		cb.commit();
		auto e = w.get(1);
		REQUIRE(w.has<Position>(e));
		auto p = w.get<Position>(e);
		REQUIRE(p.x == 1.f);
		REQUIRE(p.y == 2.f);
		REQUIRE(p.z == 3.f);
	}

	SECTION("Delayed component addition to an existing entity") {
		ecs::World w;
		ecs::CommandBuffer cb(w);

		auto e = w.add();
		cb.add<Position>(e);
		REQUIRE_FALSE(w.has<Position>(e));
		cb.commit();
		REQUIRE(w.has<Position>(e));
	}

	SECTION("Delayed component addition to a to-be-created entity") {
		ecs::World w;
		ecs::CommandBuffer cb(w);

		auto tmp = cb.add();
		REQUIRE_FALSE(w.size());
		cb.add<Position>(tmp);
		cb.commit();

		auto e = w.get(0);
		REQUIRE(w.has<Position>(e));
	}

	SECTION("Delayed component setting of an existing entity") {
		ecs::World w;
		ecs::CommandBuffer cb(w);

		auto e = w.add();

		cb.add<Position>(e);
		cb.set<Position>(e, {1, 2, 3});
		REQUIRE_FALSE(w.has<Position>(e));

		cb.commit();
		REQUIRE(w.has<Position>(e));

		auto p = w.get<Position>(e);
		REQUIRE(p.x == 1);
		REQUIRE(p.y == 2);
		REQUIRE(p.z == 3);
	}

	SECTION("Delayed 2 components setting of an existing entity") {
		ecs::World w;
		ecs::CommandBuffer cb(w);

		auto e = w.add();

		cb.add<Position>(e);
		cb.set<Position>(e, {1, 2, 3});
		cb.add<Acceleration>(e);
		cb.set<Acceleration>(e, {4, 5, 6});
		REQUIRE_FALSE(w.has<Position>(e));
		REQUIRE_FALSE(w.has<Acceleration>(e));

		cb.commit();
		REQUIRE(w.has<Position>(e));
		REQUIRE(w.has<Acceleration>(e));

		auto p = w.get<Position>(e);
		REQUIRE(p.x == 1.f);
		REQUIRE(p.y == 2.f);
		REQUIRE(p.z == 3.f);

		auto a = w.get<Acceleration>(e);
		REQUIRE(a.x == 4.f);
		REQUIRE(a.y == 5.f);
		REQUIRE(a.z == 6.f);
	}

	SECTION("Delayed component setting of a to-be-created entity") {
		ecs::World w;
		ecs::CommandBuffer cb(w);

		auto tmp = cb.add();
		REQUIRE_FALSE(w.size());

		cb.add<Position>(tmp);
		cb.set<Position>(tmp, {1, 2, 3});
		cb.commit();

		auto e = w.get(0);
		REQUIRE(w.has<Position>(e));

		auto p = w.get<Position>(e);
		REQUIRE(p.x == 1.f);
		REQUIRE(p.y == 2.f);
		REQUIRE(p.z == 3.f);
	}

	SECTION("Delayed 2 components setting of a to-be-created entity") {
		ecs::World w;
		ecs::CommandBuffer cb(w);

		auto tmp = cb.add();
		REQUIRE_FALSE(w.size());

		cb.add<Position>(tmp);
		cb.add<Acceleration>(tmp);
		cb.set<Position>(tmp, {1, 2, 3});
		cb.set<Acceleration>(tmp, {4, 5, 6});
		cb.commit();

		auto e = w.get(0);
		REQUIRE(w.has<Position>(e));
		REQUIRE(w.has<Acceleration>(e));

		auto p = w.get<Position>(e);
		REQUIRE(p.x == 1.f);
		REQUIRE(p.y == 2.f);
		REQUIRE(p.z == 3.f);

		auto a = w.get<Acceleration>(e);
		REQUIRE(a.x == 4.f);
		REQUIRE(a.y == 5.f);
		REQUIRE(a.z == 6.f);
	}

	SECTION("Delayed component add with setting of a to-be-created entity") {
		ecs::World w;
		ecs::CommandBuffer cb(w);

		auto tmp = cb.add();
		REQUIRE_FALSE(w.size());

		cb.add<Position>(tmp, {1, 2, 3});
		cb.commit();

		auto e = w.get(0);
		REQUIRE(w.has<Position>(e));

		auto p = w.get<Position>(e);
		REQUIRE(p.x == 1.f);
		REQUIRE(p.y == 2.f);
		REQUIRE(p.z == 3.f);
	}

	SECTION("Delayed 2 components add with setting of a to-be-created entity") {
		ecs::World w;
		ecs::CommandBuffer cb(w);

		auto tmp = cb.add();
		REQUIRE_FALSE(w.size());

		cb.add<Position>(tmp, {1, 2, 3});
		cb.add<Acceleration>(tmp, {4, 5, 6});
		cb.commit();

		auto e = w.get(0);
		REQUIRE(w.has<Position>(e));
		REQUIRE(w.has<Acceleration>(e));

		auto p = w.get<Position>(e);
		REQUIRE(p.x == 1.f);
		REQUIRE(p.y == 2.f);
		REQUIRE(p.z == 3.f);

		auto a = w.get<Acceleration>(e);
		REQUIRE(a.x == 4.f);
		REQUIRE(a.y == 5.f);
		REQUIRE(a.z == 6.f);
	}

	SECTION("Delayed component removal from an existing entity") {
		ecs::World w;
		ecs::CommandBuffer cb(w);

		auto e = w.add();
		w.add<Position>(e, {1, 2, 3});

		cb.del<Position>(e);
		REQUIRE(w.has<Position>(e));
		{
			auto p = w.get<Position>(e);
			REQUIRE(p.x == 1.f);
			REQUIRE(p.y == 2.f);
			REQUIRE(p.z == 3.f);
		}

		cb.commit();
		REQUIRE_FALSE(w.has<Position>(e));
	}

	SECTION("Delayed 2 component removal from an existing entity") {
		ecs::World w;
		ecs::CommandBuffer cb(w);

		auto e = w.add();
		w.add<Position>(e, {1, 2, 3});
		w.add<Acceleration>(e, {4, 5, 6});

		cb.del<Position>(e);
		cb.del<Acceleration>(e);
		REQUIRE(w.has<Position>(e));
		REQUIRE(w.has<Acceleration>(e));
		{
			auto p = w.get<Position>(e);
			REQUIRE(p.x == 1.f);
			REQUIRE(p.y == 2.f);
			REQUIRE(p.z == 3.f);

			auto a = w.get<Acceleration>(e);
			REQUIRE(a.x == 4.f);
			REQUIRE(a.y == 5.f);
			REQUIRE(a.z == 6.f);
		}

		cb.commit();
		REQUIRE_FALSE(w.has<Position>(e));
		REQUIRE_FALSE(w.has<Acceleration>(e));
	}

	SECTION("Delayed non-trivial component setting of an existing entity") {
		ecs::World w;
		ecs::CommandBuffer cb(w);

		auto e = w.add();

		cb.add<StringComponent>(e);
		cb.set<StringComponent>(e, {StringComponentDefaultValue});
		cb.add<StringComponent2>(e);
		REQUIRE_FALSE(w.has<StringComponent>(e));
		REQUIRE_FALSE(w.has<StringComponent2>(e));

		cb.commit();
		REQUIRE(w.has<StringComponent>(e));
		REQUIRE(w.has<StringComponent2>(e));

		auto s1 = w.get<StringComponent>(e);
		REQUIRE(s1.value == StringComponentDefaultValue);
		auto s2 = w.get<StringComponent2>(e);
		REQUIRE(s2.value == StringComponent2DefaultValue);
	}
}

TEST_CASE("Query Filter - no systems") {
	ecs::World w;
	ecs::Query q = w.query().all<const Position>().changed<Position>();

	auto e = w.add();
	w.add<Position>(e);

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
	{ w.set<Position>(e, {}); }
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
	{ w.sset<Position>(e, {}); }
	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		REQUIRE(cnt == 0);
	}
	{
		auto* ch = w.get_chunk(e);
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
}

TEST_CASE("Query Filter - systems") {
	ecs::World w;

	auto e = w.add();
	w.add<Position>(e);

	class WriterSystem final: public ecs::System {
		ecs::Query m_q;

	public:
		void OnCreated() override {
			m_q = world().query().all<Position>();
		}
		void OnUpdate() override {
			m_q.each([]([[maybe_unused]] Position&) {});
		}
	};
	class WriterSystemSilent final: public ecs::System {
		ecs::Query m_q;

	public:
		void OnCreated() override {
			m_q = world().query().all<Position>();
		}
		void OnUpdate() override {
			m_q.each([&](ecs::Iterator iter) {
				auto posRWView = iter.sview_mut<Position>();
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
			m_q = world().query().all<const Position>().changed<Position>();
		}
		void OnUpdate() override {
			uint32_t cnt = 0;
			m_q.each([&]([[maybe_unused]] const Position& a) {
				++cnt;
			});
			REQUIRE(cnt == m_expectedCnt);
		}
	};
	ecs::SystemManager sm(w);
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

template <typename T>
void TestDataLayoutSoA_ECS() {
	ecs::World w;

	auto create = [&]() {
		auto e = w.add();
		w.add<T>(e, {});
	};

	const uint32_t N = 10'000;
	for (uint32_t i = 0; i < N; ++i)
		create();

	ecs::Query q = w.query().all<T>();
	uint32_t j = 0;
	q.each([&](ecs::Iterator iter) {
		auto t = iter.view_mut<T>();
		auto tx = t.template set<0>();
		auto ty = t.template set<1>();
		auto tz = t.template set<2>();
		for (uint32_t i = 0; i < iter.size(); ++i, ++j) {
			auto f = (float)j;
			tx[i] = f;
			ty[i] = f;
			tz[i] = f;

			REQUIRE(tx[i] == f);
			REQUIRE(ty[i] == f);
			REQUIRE(tz[i] == f);
		}
	});
}

TEST_CASE("DataLayout SoA - ECS") {
	TestDataLayoutSoA_ECS<PositionSoA>();
	TestDataLayoutSoA_ECS<RotationSoA>();
}

TEST_CASE("DataLayout SoA8 - ECS") {
	TestDataLayoutSoA_ECS<PositionSoA8>();
	TestDataLayoutSoA_ECS<RotationSoA8>();
}

// TODO: Fix me
// TEST_CASE("DataLayout SoA16 - ECS") {
// 	TestDataLayoutSoA_ECS<PositionSoA16>();
// 	TestDataLayoutSoA_ECS<RotationSoA16>();
// }

//------------------------------------------------------------------------------
// Serialization
//------------------------------------------------------------------------------

template <typename T>
bool CompareSerializableType(const T& a, const T& b) {
	if constexpr (std::is_trivially_copyable_v<T>)
		return !std::memcmp((const void*)&a, (const void*)&b, sizeof(b));
	else
		return a == b;
}

template <typename T>
bool CompareSerializableTypes(const T& a, const T& b) {
	// Convert inputs into tuples where each struct member is an element of the tuple
	auto ta = meta::struct_to_tuple(a);
	auto tb = meta::struct_to_tuple(b);

	// Do element-wise comparison
	bool ret = true;
	core::each<std::tuple_size<decltype(ta)>::value>([&](auto i) {
		const auto& aa = std::get<i>(ta);
		const auto& bb = std::get<i>(ta);
		ret = ret && CompareSerializableType(aa, bb);
	});
	return ret;
}

struct FooNonTrivial {
	uint32_t a = 0;

	explicit FooNonTrivial(uint32_t value): a(value){};
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
		return ptr == other.ptr && size == other.size;
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
		return arr == other.arr && f == other.f;
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
		return arr == other.arr && f == other.f;
	}
};

struct SerializeCustomStructInternalDArray {
	cnt::darr<CustomStructInternal> arr;
	float f;

	bool operator==(const SerializeCustomStructInternalDArray& other) const {
		return arr == other.arr && f == other.f;
	}
};

TEST_CASE("Serialization - arrays") {
	{
		SerializeStructSArray in{}, out{};
		for (uint32_t i = 0; i < in.arr.size(); ++i)
			in.arr[i] = i;
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
		for (uint32_t i = 0; i < in.arr.size(); ++i)
			in.arr[i] = FooNonTrivial(i);
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
		for (uint32_t i = 0; i < in.arr.size(); ++i)
			in.arr[i] = i;
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
	for (uint32_t i = 0; i < arr.size(); ++i)
		sum += arr[i];
	return sum;
}

template <typename Func>
void Run_Schedule_Simple(const uint32_t* pArr, uint32_t* pRes, uint32_t Jobs, uint32_t ItemsPerJob, Func func) {
	auto& tp = mt::ThreadPool::get();

	std::atomic_uint32_t sum = 0;

	for (uint32_t i = 0; i < Jobs; ++i) {
		mt::Job job;
		job.func = [&pArr, &pRes, i, ItemsPerJob, func]() {
			const auto idxStart = i * ItemsPerJob;
			const auto idxEnd = (i + 1) * ItemsPerJob;
			pRes[i] += func({pArr + idxStart, idxEnd - idxStart});
		};
		tp.sched(job);
	}
	tp.wait_all();

	for (uint32_t i = 0; i < Jobs; ++i) {
		REQUIRE(pRes[i] == ItemsPerJob);
	}
}

TEST_CASE("Multithreading - Schedule") {
	constexpr uint32_t JobCount = 64;
	constexpr uint32_t ItemsPerJob = 5000;
	constexpr uint32_t N = JobCount * ItemsPerJob;

	cnt::sarray<uint32_t, JobCount> res;
	for (uint32_t i = 0; i < res.max_size(); ++i)
		res[i] = 0;

	cnt::darray<uint32_t> arr;
	arr.resize(N);
	for (uint32_t i = 0; i < arr.size(); ++i)
		arr[i] = 1;

	Run_Schedule_Simple(arr.data(), res.data(), JobCount, ItemsPerJob, JobSystemFunc);
}

TEST_CASE("Multithreading - ScheduleParallel") {
	auto& tp = mt::ThreadPool::get();

	constexpr uint32_t JobCount = 64;
	constexpr uint32_t ItemsPerJob = 5000;
	constexpr uint32_t N = JobCount * ItemsPerJob;

	cnt::darray<uint32_t> arr;
	arr.resize(N);
	for (uint32_t i = 0; i < arr.size(); ++i)
		arr[i] = 1;

	std::atomic_uint32_t sum1 = 0;
	mt::JobParallel j1;
	j1.func = [&arr, &sum1](const mt::JobArgs& args) {
		sum1 += JobSystemFunc({arr.data() + args.idxStart, args.idxEnd - args.idxStart});
	};

	auto jobHandle = tp.sched_par(j1, N, ItemsPerJob);
	tp.wait(jobHandle);

	REQUIRE(sum1 == N);

	tp.wait_all();
}

TEST_CASE("Multithreading - complete") {
	auto& tp = mt::ThreadPool::get();

	constexpr uint32_t Jobs = 15000;

	cnt::darray<mt::JobHandle> handles;
	cnt::darray<uint32_t> res;

	handles.resize(Jobs);
	res.resize(Jobs);

	for (uint32_t i = 0; i < res.size(); ++i)
		res[i] = (uint32_t)-1;

	for (uint32_t i = 0; i < Jobs; ++i) {
		mt::Job job;
		job.func = [&res, i]() {
			res[i] = i;
		};
		handles[i] = tp.sched(job);
	}

	for (uint32_t i = 0; i < Jobs; ++i) {
		tp.wait(handles[i]);
		REQUIRE(res[i] == i);
	}
}

TEST_CASE("Multithreading - CompleteMany") {
	auto& tp = mt::ThreadPool::get();

	srand(0);

	constexpr uint32_t Iters = 15000;
	uint32_t res = (uint32_t)-1;

	for (uint32_t i = 0; i < Iters; ++i) {
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
}

//------------------------------------------------------------------------------
