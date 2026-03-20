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

TEST_CASE("StringLookupKey") {
	constexpr uint32_t MaxLen = 32;
	char tmp0[MaxLen];
	char tmp1[MaxLen];
	GAIA_STRFMT(tmp0, MaxLen, "%s", "some string");
	GAIA_STRFMT(tmp1, MaxLen, "%s", "some string");
	core::StringLookupKey<128> l0(tmp0, (uint32_t)GAIA_STRLEN(tmp0, MaxLen), 0);
	core::StringLookupKey<128> l1(tmp1, (uint32_t)GAIA_STRLEN(tmp1, MaxLen), 0);
	CHECK(l0.len() == l1.len());
	// Two different addresses in memory have to return the same hash if the string is the same
	CHECK(l0.hash() == l1.hash());
}

//------------------------------------------------------------------------------
// Logging (for code coverage only)
//------------------------------------------------------------------------------

void custom_log_line_func(gaia::util::LogLevel level, const char* msg) {
	(void)level;
	(void)msg;
}

void custom_log_flush_func() {}

TEST_CASE("logging") {
	gaia::util::log_enable(util::LogLevel::Debug, false);
	gaia::util::log_enable(util::LogLevel::Info, false);
	gaia::util::log_enable(util::LogLevel::Warning, false);
	gaia::util::log_enable(util::LogLevel::Error, false);

	GAIA_FOR(gaia::util::detail::LOG_RECORD_LIMIT) {
		GAIA_LOG_D("");
		GAIA_LOG_N("");
		GAIA_LOG_W("");
		GAIA_LOG_E("");
		GAIA_LOG_D("dummy");
		GAIA_LOG_N("dummy");
		GAIA_LOG_W("dummy");
		GAIA_LOG_E("dummy");
	}
	{
		ecs::World w;
		GAIA_FOR(1000) w.diag_archetypes();
	}
	gaia::util::log_flush();

	gaia::util::log_enable(util::LogLevel::Debug, true);
	gaia::util::log_enable(util::LogLevel::Info, true);
	gaia::util::log_enable(util::LogLevel::Warning, true);
	gaia::util::log_enable(util::LogLevel::Error, true);

	gaia::util::set_log_line_func(custom_log_line_func);

	GAIA_FOR(gaia::util::detail::LOG_RECORD_LIMIT) {
		GAIA_LOG_D("");
		GAIA_LOG_N("");
		GAIA_LOG_W("");
		GAIA_LOG_E("");
		GAIA_LOG_D("dummy");
		GAIA_LOG_N("dummy");
		GAIA_LOG_W("dummy");
		GAIA_LOG_E("dummy");
	}
	{
		ecs::World w;
		GAIA_FOR(1000) w.diag_archetypes();
	}
	gaia::util::log_flush();

	gaia::util::set_log_line_func(nullptr);

	gaia::util::set_log_flush_func(custom_log_flush_func);
	gaia::util::log_flush();
	gaia::util::set_log_flush_func(nullptr);
	gaia::util::log_flush();
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
		auto t = util::trim(std::span<const char>(str.c_str(), str.length()));
		CHECK(std::string(t.data(), t.size()) == target);
	}

	{
		std::string str = "Gaia-ECS";
		auto t = util::trim(std::span<const char>(str.c_str(), str.length()));
		CHECK(std::string(t.data(), t.size()) == target);
	}

	{
		std::string str = "";
		auto t = util::trim(str);
		CHECK(std::string(t.data(), t.size()) == std::string(""));
	}

	{
		util::str_view sv = " \t\n  Gaia-ECS  \r\n";
		const auto t = util::trim(sv);
		CHECK(t == "Gaia-ECS");
	}

	{
		util::str_view sv = " \t\r\n ";
		const auto t = util::trim(sv);
		CHECK(t.empty());
	}

	{
		util::str_view sv{};
		const auto t = util::trim(sv);
		CHECK(t.empty());
	}
}

TEST_CASE("util::str") {
	using util::str;
	using util::str_view;

	static_assert(str_view("Gaia-ECS").size() == 8);
	static_assert(!str_view("Gaia-ECS").empty());
	static_assert(str_view("Gaia-ECS").find("ECS") == 5);
	static_assert(str_view("Gaia-ECS").find('-') == 4);
	static_assert(str_view("Gaia-ECS").find_first_of("xyzE") == 5);
	static_assert(str_view("Gaia-ECS").find_last_of('a') == 3);
	static_assert(str_view("Gaia-ECS").find_first_not_of("Ga") == 2);
	static_assert(str_view("Gaia-ECS").find_last_not_of("CS") == 5);
	static_assert(str_view("Gaia-ECS") == str_view("Gaia-ECS"));
	static_assert(str_view("Gaia-ECS") != str_view("gaia-ecs"));
	static_assert(util::trim(str_view(" \tGaia-ECS\n")) == str_view("Gaia-ECS"));

	SUBCASE("basic ownership and empty semantics") {
		str s;
		CHECK(s.empty());
		CHECK(s.size() == 0);
		CHECK(s.find("x") == BadIndex);

		s.append('a');
		CHECK_FALSE(s.empty());
		CHECK(s.size() == 1);
		CHECK(s == "a");

		s.clear();
		CHECK(s.empty());
		CHECK(s.size() == 0);

		s.assign("abc", 3);
		CHECK(s == "abc");
		s.append("def", 3);
		CHECK(s == "abcdef");
		s.reserve(64);
		CHECK(s == "abcdef");
	}

	SUBCASE("find supports positional substring and character search") {
		const str s("ababa_xyz_aba");

		CHECK(s.find("aba") == 0);
		CHECK(s.find("aba", 1) == 2);
		CHECK(s.find("aba", 3) == 10);
		CHECK(s.find("aba", 11) == BadIndex);

		CHECK(s.find('x') == 6);
		CHECK(s.find('a', 1) == 2);
		CHECK(s.find('q') == BadIndex);

		CHECK(s.find("", 0) == 0);
		CHECK(s.find("", 4) == 4);
		CHECK(s.find("", (uint32_t)s.size()) == s.size());
		CHECK(s.find("", (uint32_t)s.size() + 1) == BadIndex);
	}

	SUBCASE("find_first_of and find_last_of") {
		const str s("0123abc3210");

		CHECK(s.find_first_of("abc") == 4);
		CHECK(s.find_first_of("c") == 6);
		CHECK(s.find_first_of('2') == 2);
		CHECK(s.find_first_of("xyz") == BadIndex);

		CHECK(s.find_last_of("abc") == 6);
		CHECK(s.find_last_of("abc", 5) == 5);
		CHECK(s.find_last_of('3') == 7);
		CHECK(s.find_last_of('3', 6) == 3);
		CHECK(s.find_last_of("xyz") == BadIndex);
		CHECK(s.find_last_of("") == BadIndex);
	}

	SUBCASE("find_first_not_of and find_last_not_of") {
		const str s("000abc000");

		CHECK(s.find_first_not_of('0') == 3);
		CHECK(s.find_first_not_of("0") == 3);
		CHECK(s.find_first_not_of("0abc") == BadIndex);
		CHECK(s.find_first_not_of("") == 0);

		CHECK(s.find_last_not_of('0') == 5);
		CHECK(s.find_last_not_of("0") == 5);
		CHECK(s.find_last_not_of("0abc") == BadIndex);
		CHECK(s.find_last_not_of("") == 8);
		CHECK(s.find_last_not_of("", 3) == 3);

		const str empty;
		CHECK(empty.find_first_not_of("") == BadIndex);
		CHECK(empty.find_last_not_of("") == BadIndex);
	}

	SUBCASE("str_view search parity") {
		const str_view v("cabba", 5);

		CHECK(v.find("abb") == 1);
		CHECK(v.find('a') == 1);
		CHECK(v.find('a', 2) == 4);
		CHECK(v.find("bb", 3) == BadIndex);

		CHECK(v.find_first_of("bx") == 2);
		CHECK(v.find_last_of("ab") == 4);
		CHECK(v.find_last_of('b') == 3);

		CHECK(v.find_first_not_of("ca") == 2);
		CHECK(v.find_last_not_of("ab") == 0);

		CHECK(v.find("", 5) == 5);
		CHECK(v.find("", 6) == BadIndex);
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
	cnt::darray<Foo*> foos_backup;
	cnt::darray<Foo*> foos;
	foos_backup.resize(N);
	foos.resize(N);
	GAIA_FOR(N) {
		auto*& f = foos[i];
		f = new Foo();
		f->value = i;

		foos_backup[i] = f;
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

	for (auto* f: foos_backup)
		delete f;
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

	CHECK(core::find(arr, 0U) == arr.cbegin());
	CHECK(core::find(arr, N) == arr.cend());
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

	arr = {};
	arr.resize(N);
	GAIA_FOR(N) {
		arr[i] = i;
		CHECK(arr[i] == i);
	}

	arr = {};
	arr.resize(2, cont_item(7));
	CHECK(arr.size() == 2);
	CHECK(arr[0] == cont_item(7));
	CHECK(arr[1] == cont_item(7));

	arr[0] = cont_item(0);
	arr[1] = cont_item(1);
	arr.resize(N, cont_item(9));
	CHECK(arr.size() == N);
	CHECK(arr[0] == cont_item(0));
	CHECK(arr[1] == cont_item(1));
	GAIA_FOR2(2, N) CHECK(arr[i] == cont_item(9));

	arr = {};
	arr.resize(N);
	GAIA_FOR(N) {
		arr[i] = i;
		CHECK(arr[i] == i);
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
	for (auto&& val: arr) {
		CHECK(val == cnt);
		++cnt;
	}
	CHECK(cnt == N);
	CHECK(cnt == arr.size());

	std::span<const cont_item> view{arr.data(), arr.size()};

	CHECK(core::find(arr, 0U) == arr.cbegin());
	CHECK(core::find(arr, N) == arr.cend());
	CHECK(core::find(view, 0U) == view.begin());
	CHECK(core::find(view, N) == view.end());

	CHECK(core::has(arr, 0U));
	CHECK(core::has(view, 0U));
	CHECK_FALSE(core::has(arr, N));
	CHECK_FALSE(core::has(view, N));

	arr.erase(arr.begin());
	CHECK(arr.size() == (N - 1));
	CHECK(core::find(arr, 0U) == arr.cend());
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

	// Insert before capacity changes
	{
		arr = {};
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

	// Insert with capacity change
	{
		arr = {};
		arr.push_back(11);
		arr.push_back(13);
		arr.push_back(14);
		arr.push_back(15);
		arr.insert(arr.begin() + 1, 12);
		CHECK(arr.size() == 5);
		CHECK(arr[0] == 11);
		CHECK(arr[1] == 12);
		CHECK(arr[2] == 13);
		CHECK(arr[3] == 14);
		CHECK(arr[4] == 15);

		arr.insert(arr.begin(), 10);
		CHECK(arr.size() == 6);
		CHECK(arr[0] == 10);
		CHECK(arr[1] == 11);
		CHECK(arr[2] == 12);
		CHECK(arr[3] == 13);
		CHECK(arr[4] == 14);
		CHECK(arr[5] == 15);
	}
}

template <typename Container>
void resizable_arr_soa_resize_fill_test(uint32_t count) {
	using cont_item = typename Container::value_type;
	GAIA_ASSERT(count > 2);

	const cont_item first{1, 2, 3};
	const cont_item second{4, 5, 6};
	const cont_item third{7, 8, 9};

	Container arr;
	arr.resize(2, first);
	CHECK(arr.size() == 2);
	CHECK(arr[0] == first);
	CHECK(arr[1] == first);

	arr[0] = second;
	arr[1] = third;
	arr.resize(count, first);

	CHECK(arr.size() == count);
	CHECK(arr[0] == second);
	CHECK(arr[1] == third);
	GAIA_FOR2(2, count) CHECK(arr[i] == first);
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
	using NonTrivialT3 = cnt::darr<TypeNonTrivialC<uint32_t>>;
	using NonTrivialT4 = cnt::darr<TypeNonTrivialM<uint32_t>>;
	SUBCASE("trivial_types") {
		resizable_arr_test<TrivialT>(N);
	}
	SUBCASE("non_trivial_types") {
		resizable_arr_test<NonTrivialT>(N);
		resizable_arr_test<NonTrivialT3>(N);
		resizable_arr_test<NonTrivialT4>(N);
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
	using NonTrivialT3 = cnt::darr<TypeNonTrivialC<uint32_t>>;
	using NonTrivialT4 = cnt::darr<TypeNonTrivialM<uint32_t>>;
	SUBCASE("trivial_types") {
		resizable_arr_test<TrivialT>(N);
		resizable_arr_test<TrivialT>(M);
	}
	SUBCASE("non_trivial_types") {
		resizable_arr_test<NonTrivialT>(N);
		resizable_arr_test<NonTrivialT>(M);

		resizable_arr_test<NonTrivialT3>(N);
		resizable_arr_test<NonTrivialT3>(M);

		resizable_arr_test<NonTrivialT4>(N);
		resizable_arr_test<NonTrivialT4>(M);
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
	using NonTrivialT3 = cnt::darr_ext<TypeNonTrivialC<uint32_t>, 100>;
	using NonTrivialT4 = cnt::darr_ext<TypeNonTrivialM<uint32_t>, 100>;

	SUBCASE("trivial_types") {
		resizable_arr_test<TrivialT1>(N);
		resizable_arr_test<TrivialT1>(M);
		resizable_arr_test<TrivialT1>(M);

		resizable_arr_test<TrivialT2>(N);
		resizable_arr_test<TrivialT2>(M);
	}
	SUBCASE("non_trivial_types") {
		resizable_arr_test<NonTrivialT1>(N);
		resizable_arr_test<NonTrivialT1>(M);

		resizable_arr_test<NonTrivialT2>(N);
		resizable_arr_test<NonTrivialT2>(M);

		resizable_arr_test<NonTrivialT3>(N);
		resizable_arr_test<NonTrivialT3>(M);

		resizable_arr_test<NonTrivialT4>(N);
		resizable_arr_test<NonTrivialT4>(M);
	}
	SUBCASE("retain") {
		retainable_arr_test<TrivialT1>();
	}
}

TEST_CASE("Containers - SoA resize fill") {
	resizable_arr_soa_resize_fill_test<cnt::darr_soa<PositionSoA>>(16);
	resizable_arr_soa_resize_fill_test<cnt::darr_ext_soa<PositionSoA, 8>>(16);
	resizable_arr_soa_resize_fill_test<cnt::sarr_ext_soa<PositionSoA, 16>>(16);
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

	CHECK(core::find(arr, cont_item{0U, 0U}) == arr.cbegin());
	CHECK(core::find(arr, cont_item{N, N}) == arr.cend());
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
	CHECK_FALSE(arr.has(to_sid(13)));
	CHECK(arr.has(to_sid(14)));
	CHECK(arr.has(to_sid(15)));

	arr.del(to_sid(11));
	CHECK(arr.size() == 3);
	CHECK_FALSE(arr.has(to_sid(11)));
	CHECK(arr.has(to_sid(12)));
	CHECK(arr.has(to_sid(14)));
	CHECK(arr.has(to_sid(15)));

	arr.del(to_sid(15));
	CHECK(arr.size() == 2);
	CHECK(arr.has(to_sid(12)));
	CHECK(arr.has(to_sid(14)));
	CHECK_FALSE(arr.has(to_sid(15)));

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
	CHECK_FALSE(arr.has(to_sid(9002)));
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

	CHECK(core::find(arr, cont_item{0U, 0U}) == arr.cbegin());
	CHECK(core::find(arr, cont_item{N, N}) == arr.cend());
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

		{
			uint32_t i = 0;
			for (const auto& val: std::as_const(data)) {
				const float f[] = {(float)(i + 1), (float)(i + 2), (float)(i + 3)};
				CHECK(val.x == f[0]);
				CHECK(val.y == f[1]);
				CHECK(val.z == f[2]);
				++i;
			}
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

		{
			uint32_t i = 0;
			for (const auto& val: std::as_const(data)) {
				const float f[] = {(float)(i + 1), (float)(i + 2), (float)(i + 3)};
				CHECK(val.x == f[0]);
				CHECK(val.y == f[1]);
				CHECK(val.z == f[2]);
				++i;
			}
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

		{
			uint32_t i = 0;
			for (const auto& val: std::as_const(data)) {
				const float f[] = {(float)(i + 1), (float)(i + 2), (float)(i + 3)};
				const bool b = data.template view<2>()[i];
				CHECK(val.x == f[0]);
				CHECK(val.y == f[1]);
				CHECK(val.b == b);
				CHECK(val.w == f[2]);
				++i;
			}
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

		{
			uint32_t i = 0;
			for (const auto& val: std::as_const(data)) {
				const float f[] = {(float)(i + 1), (float)(i + 2), (float)(i + 3)};
				const bool b = data.template view<2>()[i];
				CHECK(val.x == f[0]);
				CHECK(val.y == f[1]);
				CHECK(val.b == b);
				CHECK(val.w == f[2]);
				++i;
			}
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

TEST_CASE("Entity - exact has/get survive archetype moves") {
	TestWorld twld;

	auto rel = wld.add();
	auto tgt = wld.add();
	auto e = wld.add();

	wld.add<Position>(e, {1, 2, 3});
	wld.add(e, ecs::Pair(rel, tgt));

	CHECK(wld.has<Position>(e));
	CHECK(wld.has(e, ecs::Pair(rel, tgt)));
	{
		const auto& p = wld.get<Position>(e);
		CHECK(p.x == 1);
		CHECK(p.y == 2);
		CHECK(p.z == 3);
	}

	wld.add<Rotation>(e, {4, 5, 6});

	CHECK(wld.has<Position>(e));
	CHECK(wld.has<Rotation>(e));
	CHECK(wld.has(e, ecs::Pair(rel, tgt)));
	{
		const auto& p = wld.get<Position>(e);
		const auto& r = wld.get<Rotation>(e);
		CHECK(p.x == 1);
		CHECK(p.y == 2);
		CHECK(p.z == 3);
		CHECK(r.x == 4);
		CHECK(r.y == 5);
		CHECK(r.z == 6);
	}

	wld.del<Position>(e);

	CHECK_FALSE(wld.has<Position>(e));
	CHECK(wld.has<Rotation>(e));
	CHECK(wld.has(e, ecs::Pair(rel, tgt)));
	{
		const auto& r = wld.get<Rotation>(e);
		CHECK(r.x == 4);
		CHECK(r.y == 5);
		CHECK(r.z == 6);
	}
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

	SUBCASE("copy assignment replaces previous tracking") {
		TestWorld twld;

		auto e0 = wld.add();
		auto e1 = wld.add();
		auto dst = ecs::WeakEntity(wld, e0);
		auto src = ecs::WeakEntity(wld, e1);

		dst = src;
		CHECK(dst == e1);
		CHECK(src == e1);

		wld.del(e0);
		CHECK(dst == e1);
		CHECK(src == e1);

		wld.del(e1);
		CHECK(dst == ecs::EntityBad);
		CHECK(src == ecs::EntityBad);
	}

	SUBCASE("move assignment transfers tracking and releases previous one") {
		TestWorld twld;

		auto e0 = wld.add();
		auto e1 = wld.add();
		auto dst = ecs::WeakEntity(wld, e0);
		auto src = ecs::WeakEntity(wld, e1);

		dst = GAIA_MOV(src);
		CHECK(dst == e1);
		CHECK(src == ecs::EntityBad);

		wld.del(e0);
		CHECK(dst == e1);

		wld.del(e1);
		CHECK(dst == ecs::EntityBad);
	}

	SUBCASE("remaining weak handles still invalidate after another handle is reset") {
		TestWorld twld;

		auto e = wld.add();
		auto first = ecs::WeakEntity(wld, e);
		auto second = ecs::WeakEntity(wld, e);

		first = ecs::WeakEntity();
		CHECK(first == ecs::EntityBad);
		CHECK(second == e);

		wld.del(e);
		CHECK(second == ecs::EntityBad);
	}

	SUBCASE("stress random copy move delete sequences") {
		TestWorld twld;

		constexpr uint32_t Iterations = 20'000;
		constexpr uint32_t WeakSlots = 128;
		constexpr uint32_t MaxLiveEntities = 256;

		cnt::darr<ecs::WeakEntity> weaks;
		weaks.resize(WeakSlots);

		cnt::darr<ecs::Entity> live;
		live.reserve(MaxLiveEntities);

		auto verify = [&] {
			for (const auto& weak: weaks) {
				const auto e = weak.entity();
				if (e == ecs::EntityBad) {
					CHECK_FALSE(wld.valid(e));
					continue;
				}

				CHECK(wld.valid(e));
				CHECK(wld.has(e));
			}
		};

		rnd::pseudo_random rng(0xC0FFEEu);

		for (uint32_t it = 0; it < Iterations; ++it) {
			const auto op = rng.range(0, 8);
			switch (op) {
				case 0: {
					if (live.size() < MaxLiveEntities)
						live.push_back(wld.add());
					break;
				}
				case 1: {
					if (live.empty())
						break;
					const uint32_t idx = rng.range(0, (uint32_t)live.size() - 1);
					wld.del(live[idx]);
					live[idx] = live.back();
					live.pop_back();
					break;
				}
				case 2: {
					if (live.empty())
						break;
					const uint32_t wi = rng.range(0, WeakSlots - 1);
					const uint32_t ei = rng.range(0, (uint32_t)live.size() - 1);
					weaks[wi] = ecs::WeakEntity(wld, live[ei]);
					break;
				}
				case 3: {
					const uint32_t wi = rng.range(0, WeakSlots - 1);
					weaks[wi] = ecs::WeakEntity();
					break;
				}
				case 4: {
					const uint32_t a = rng.range(0, WeakSlots - 1);
					const uint32_t b = rng.range(0, WeakSlots - 1);
					weaks[a] = weaks[b];
					break;
				}
				case 5: {
					const uint32_t a = rng.range(0, WeakSlots - 1);
					const uint32_t b = rng.range(0, WeakSlots - 1);
					weaks[a] = GAIA_MOV(weaks[b]);
					break;
				}
				case 6: {
					const uint32_t a = rng.range(0, WeakSlots - 1);
					const uint32_t b = rng.range(0, WeakSlots - 1);
					auto tmp = weaks[b];
					weaks[a] = GAIA_MOV(tmp);
					break;
				}
				case 7: {
					if (!live.empty()) {
						const uint32_t idx = rng.range(0, (uint32_t)live.size() - 1);
						wld.del(live[idx]);
						live[idx] = live.back();
						live.pop_back();
					}
					break;
				}
				case 8: {
					if (!live.empty()) {
						const uint32_t wi = rng.range(0, WeakSlots - 1);
						const uint32_t ei = rng.range(0, (uint32_t)live.size() - 1);
						const auto e = live[ei];
						weaks[wi] = ecs::WeakEntity(wld, e);
						wld.del(e);
						live[ei] = live.back();
						live.pop_back();
					}
					break;
				}
				default:
					break;
			}

			if ((it % 127) == 0)
				verify();
		}

		while (!live.empty()) {
			wld.del(live.back());
			live.pop_back();
		}

		verify();
		for (const auto& weak: weaks)
			CHECK(weak == ecs::EntityBad);
	}

	SUBCASE("world teardown with live weak component") {
		struct WeakComponent {
			ecs::WeakEntity entity;
		};

		{
			ecs::World world;
			auto e = world.add();
			auto holder = world.add();
			world.add<WeakComponent>(holder, {ecs::WeakEntity(world, e)});
		}

		CHECK(true);
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

TEST_CASE("Accessor - repeated component access") {
	TestWorld twld;
	auto e = wld.add();
	const auto& runtimeInt3 = wld.add<Int3>();

	wld.add<Position>(e, {1.f, 2.f, 3.f});
	wld.add<Rotation>(e, {4.f, 5.f, 6.f, 7.f});
	wld.add(e, runtimeInt3.entity, Int3{8, 9, 10});

	{
		auto getter = wld.acc(e);
		auto pos = getter.get<Position>();
		CHECK(pos.x == 1.f);
		CHECK(pos.y == 2.f);
		CHECK(pos.z == 3.f);

		auto rot = getter.get<Rotation>();
		CHECK(rot.x == 4.f);
		CHECK(rot.y == 5.f);
		CHECK(rot.z == 6.f);
		CHECK(rot.w == 7.f);

		pos = getter.get<Position>();
		CHECK(pos.x == 1.f);
		CHECK(pos.y == 2.f);
		CHECK(pos.z == 3.f);

		auto i3 = getter.get<Int3>(runtimeInt3.entity);
		CHECK(i3.x == 8);
		CHECK(i3.y == 9);
		CHECK(i3.z == 10);
	}

	{
		auto setter = wld.acc_mut(e);
		setter.set<Position>({10.f, 20.f, 30.f});
		setter.sset<Rotation>({40.f, 50.f, 60.f, 70.f});
		setter.set<Int3>(runtimeInt3.entity, Int3{80, 90, 100});
	}

	{
		auto getter = wld.acc(e);
		auto pos = getter.get<Position>();
		CHECK(pos.x == 10.f);
		CHECK(pos.y == 20.f);
		CHECK(pos.z == 30.f);

		auto rot = getter.get<Rotation>();
		CHECK(rot.x == 40.f);
		CHECK(rot.y == 50.f);
		CHECK(rot.z == 60.f);
		CHECK(rot.w == 70.f);

		auto i3 = getter.get<Int3>(runtimeInt3.entity);
		CHECK(i3.x == 80);
		CHECK(i3.y == 90);
		CHECK(i3.z == 100);
	}
}

TEST_CASE("Add-del") {
	// This is a do-not-crash unit test.
	// This is done to properly test cases of [] -> [A] -> [A,B] -> [A,B,C].
	// If B disappears and an operation later needs that transition, the graph need to re-link correctly (
	// where a direct transition is semantically valid for that entity operation).
	// Create a one-direction cached path, delete a type/archetype in that path, then repeatedly run
	// remove-component transitions(del) plus update() cleanup.
	TestWorld twld;

	// 1) Create three entities
	auto A = wld.add();
	auto B = wld.add();
	auto C = wld.add();

	// 2) Build archetype chain using one add-order only:
	//    [] -> [A] -> [A,B] -> [A,B,C]
	auto e = wld.add();
	wld.add(e, A);
	wld.add(e, B);
	wld.add(e, C);

	// 3) Delete A
	wld.del(A);

	// 4) Keep mutating entities through remove paths so del-edge lookup is used
	for (int i = 0; i < 2000; ++i) {
		auto x = wld.add();
		wld.add(x, B);
		wld.add(x, C);
		wld.del(x, B); // hits foc_archetype_del path
		wld.del(x);
		wld.update(); // runs gc/archetype cleanup
	}
}

TEST_CASE("Chunk delete queue handles multiple queued chunks") {
	TestWorld twld;

	struct ChunkQueueTag {};

	cnt::darray<ecs::Entity> entities;
	entities.reserve(5000);

	for (uint32_t i = 0; i < 5000; ++i) {
		auto e = wld.add();
		wld.add<ChunkQueueTag>(e);
		entities.push_back(e);
	}

	for (auto e: entities)
		wld.del(e);

	wld.update();

	for (uint32_t i = 0; i < 5000; ++i) {
		auto e = wld.add();
		wld.add<ChunkQueueTag>(e);
		entities[i] = e;
	}

	for (uint32_t i = 0; i < 2500; ++i)
		wld.del(entities[i]);

	wld.update();

	auto q = wld.query().all<ChunkQueueTag>();
	CHECK(q.count() == 2500);
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

	CHECK_FALSE(wld.has(animal, animal));
	CHECK_FALSE(wld.has(herbivore, herbivore));
	CHECK_FALSE(wld.has(carnivore, carnivore));
	CHECK_FALSE(wld.has_direct(animal, ecs::Pair(ecs::Is, animal)));
	CHECK_FALSE(wld.has_direct(herbivore, ecs::Pair(ecs::Is, herbivore)));
	CHECK_FALSE(wld.has_direct(carnivore, ecs::Pair(ecs::Is, carnivore)));

	CHECK(wld.is(carnivore, animal));
	CHECK(wld.is(herbivore, animal));
	CHECK(wld.is(rabbit, animal));
	CHECK(wld.is(hare, animal));
	CHECK(wld.is(wolf, animal));
	CHECK(wld.has(carnivore, ecs::Pair(ecs::Is, animal)));
	CHECK(wld.has(herbivore, ecs::Pair(ecs::Is, animal)));
	CHECK(wld.has(rabbit, ecs::Pair(ecs::Is, animal)));
	CHECK(wld.has(hare, ecs::Pair(ecs::Is, animal)));
	CHECK(wld.has(wolf, ecs::Pair(ecs::Is, animal)));
	CHECK(wld.is(rabbit, herbivore));
	CHECK(wld.is(hare, herbivore));
	CHECK(wld.is(wolf, carnivore));

	CHECK(wld.is(animal, animal));
	CHECK(wld.is(herbivore, herbivore));
	CHECK(wld.is(carnivore, carnivore));
	CHECK(wld.has(animal, ecs::Pair(ecs::Is, animal)));
	CHECK(wld.has(herbivore, ecs::Pair(ecs::Is, herbivore)));
	CHECK(wld.has(carnivore, ecs::Pair(ecs::Is, carnivore)));
	CHECK(wld.has_direct(carnivore, ecs::Pair(ecs::Is, animal)));
	CHECK(wld.has_direct(herbivore, ecs::Pair(ecs::Is, animal)));
	CHECK(wld.has_direct(rabbit, ecs::Pair(ecs::Is, herbivore)));
	CHECK_FALSE(wld.has_direct(rabbit, ecs::Pair(ecs::Is, animal)));
	CHECK_FALSE(wld.has_direct(wolf, ecs::Pair(ecs::Is, animal)));

	CHECK_FALSE(wld.is(animal, herbivore));
	CHECK_FALSE(wld.is(animal, carnivore));
	CHECK_FALSE(wld.is(wolf, herbivore));
	CHECK_FALSE(wld.is(rabbit, carnivore));
	CHECK_FALSE(wld.is(hare, carnivore));

	ecs::QueryTermOptions directOpts;
	directOpts.direct();
	auto qDirectAnimal = wld.query<false>().all(ecs::Pair(ecs::Is, animal), directOpts);
	CHECK(qDirectAnimal.count() == 2);
	cnt::darr<ecs::Entity> directAnimalEntities;
	qDirectAnimal.each([&](ecs::Entity entity) {
		directAnimalEntities.push_back(entity);
	});
	CHECK(directAnimalEntities.size() == 2);
	CHECK(core::has(directAnimalEntities, herbivore));
	CHECK(core::has(directAnimalEntities, carnivore));

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
			const bool isOK = entity == animal || entity == hare || entity == rabbit || entity == wolf ||
												entity == carnivore || entity == herbivore;
			CHECK(isOK);

			++i;
		});
		CHECK(i == 6);
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
		ecs::Query q = wld.query().all(ecs::Pair(ecs::Is, animal)).no(ecs::Pair(ecs::Is, carnivore));
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
void verify_name_has(const ecs::World& world, const ecs::ComponentCache& cc, const char* str) {
	const auto& item = cc.get<T>();
	auto name = item.name;
	CHECK(name.str() != nullptr);
	CHECK(name.len() > 1);
	CHECK(world.symbol(str) == item.entity);
}

void verify_name_has_not(const ecs::World& world, const char* str) {
	CHECK(world.symbol(str) == ecs::EntityBad);
}

template <typename T>
void verify_name_has_not(const ecs::ComponentCache& cc) {
	const auto* item = cc.find<T>();
	CHECK(item == nullptr);
}

#define verify_name_has(name) verify_name_has<name>(wld, cc, #name);
#define verify_name_has_not(name)                                                                                      \
	{                                                                                                                    \
		verify_name_has_not<name>(cc);                                                                                     \
		verify_name_has_not(wld, #name);                                                                                   \
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

	SUBCASE("component scope controls default paths and relative lookup") {
		TestWorld twld;
		const auto& cc = wld.comp_cache();

		const auto gameplay = wld.add();
		wld.name(gameplay, "gameplay");

		const auto render = wld.add();
		wld.name(render, "render");
		wld.child(render, gameplay);

		const auto tools = wld.add();
		wld.name(tools, "tools");
		wld.child(tools, gameplay);

		ecs::Entity gameplayPos = ecs::EntityBad;
		ecs::Entity renderPos = ecs::EntityBad;
		ecs::Entity renderDevice = ecs::EntityBad;

		wld.scope(gameplay, [&] {
			gameplayPos = wld.add<Position>().entity;
			CHECK(wld.path(gameplayPos) == "gameplay.Position");
			CHECK(wld.get("Position") == gameplayPos);

			wld.scope(render, [&] {
				renderPos = wld.add<dummy::Position>().entity;
				CHECK(wld.path(renderPos) == "gameplay.render.Position");
				CHECK(wld.get("Position") == renderPos);

				renderDevice = wld.add("Device", 0, ecs::DataStorageType::Table, 1).entity;
				CHECK(wld.path(renderDevice) == "gameplay.render.Device");
				CHECK(wld.get("Device") == renderDevice);
				CHECK(wld.get("gameplay.render.Device") == renderDevice);
			});

			CHECK(wld.get("Position") == gameplayPos);
		});

		CHECK(wld.scope() == ecs::EntityBad);
		CHECK(wld.alias("Position") == ecs::EntityBad);
		CHECK(wld.get("Position") == gameplayPos);

		CHECK(wld.scope(tools) == ecs::EntityBad);
		CHECK(wld.get("Position") == gameplayPos);
		CHECK(wld.get("gameplay.render.Device") == renderDevice);

		CHECK(wld.scope(render) == tools);
		CHECK(wld.get("Position") == renderPos);
		CHECK(wld.get("Device") == renderDevice);

		CHECK(wld.scope(ecs::EntityBad) == render);
		CHECK(wld.get("Position") == gameplayPos);
	}

	SUBCASE("component scope resolves string queries at construction time") {
		TestWorld twld;

		const auto gameplay = wld.add();
		wld.name(gameplay, "gameplay");

		const auto render = wld.add();
		wld.name(render, "render");
		wld.child(render, gameplay);

		const auto gameplayComp = wld.add<Position>().entity;
		ecs::Entity renderComp = ecs::EntityBad;
		wld.scope(render, [&] {
			renderComp = wld.add<dummy::Position>().entity;
		});

		const auto eGameplay = wld.add();
		wld.add(eGameplay, gameplayComp);

		const auto eRender = wld.add();
		wld.add(eRender, renderComp);

		auto qGlobal = wld.query<false>().add("Position");
		CHECK(qGlobal.count() == 1);
		qGlobal.each([&](ecs::Entity e) {
			CHECK(e == eGameplay);
		});

		auto qScoped = wld.query<false>();
		wld.scope(render, [&] {
			qScoped.add("Position");
		});
		CHECK(qScoped.count() == 1);
		qScoped.each([&](ecs::Entity e) {
			CHECK(e == eRender);
		});

		auto qPath = wld.query<false>().add("gameplay.render.Position");
		CHECK(qPath.count() == 1);
		qPath.each([&](ecs::Entity e) {
			CHECK(e == eRender);
		});
	}

	SUBCASE("lookup path resolves unqualified component names in configured order") {
		TestWorld twld;

		const auto gameplay = wld.add();
		wld.name(gameplay, "gameplay");

		const auto render = wld.add();
		wld.name(render, "render");
		wld.child(render, gameplay);

		const auto tools = wld.add();
		wld.name(tools, "tools");
		wld.child(tools, gameplay);

		ecs::Entity gameplayDevice = ecs::EntityBad;
		ecs::Entity toolsDevice = ecs::EntityBad;

		wld.scope(gameplay, [&] {
			gameplayDevice = wld.add("Gameplay::Device", 0, ecs::DataStorageType::Table, 1).entity;
		});
		wld.scope(tools, [&] {
			toolsDevice = wld.add("Tools::Device", 0, ecs::DataStorageType::Table, 1).entity;
		});

		CHECK(wld.get("Device") == ecs::EntityBad);

		const ecs::Entity lookupRender[] = {render};
		wld.lookup_path(lookupRender);
		CHECK(wld.get("Device") == gameplayDevice);

		const ecs::Entity lookupToolsRender[] = {tools, render};
		wld.lookup_path(lookupToolsRender);
		CHECK(wld.get("Device") == toolsDevice);

		const ecs::Entity lookupRenderTools[] = {render, tools};
		wld.lookup_path(lookupRenderTools);
		CHECK(wld.get("Device") == gameplayDevice);

		wld.lookup_path(std::span<const ecs::Entity>{});
		CHECK(wld.lookup_path().empty());
		CHECK(wld.get("Device") == ecs::EntityBad);
	}

	SUBCASE("lookup path resolves string queries at construction time") {
		TestWorld twld;

		const auto gameplay = wld.add();
		wld.name(gameplay, "gameplay");

		const auto render = wld.add();
		wld.name(render, "render");
		wld.child(render, gameplay);

		const auto gameplayComp = wld.add<Position>().entity;
		ecs::Entity renderComp = ecs::EntityBad;
		wld.scope(render, [&] {
			renderComp = wld.add<dummy::Position>().entity;
		});

		const auto eGameplay = wld.add();
		wld.add(eGameplay, gameplayComp);

		const auto eRender = wld.add();
		wld.add(eRender, renderComp);

		auto qDefault = wld.query<false>().add("Position");
		CHECK(qDefault.count() == 1);
		qDefault.each([&](ecs::Entity e) {
			CHECK(e == eGameplay);
		});

		const ecs::Entity lookupRender[] = {render};
		wld.lookup_path(lookupRender);

		auto qLookup = wld.query<false>().add("Position");
		CHECK(qLookup.count() == 1);
		qLookup.each([&](ecs::Entity e) {
			CHECK(e == eRender);
		});

		wld.lookup_path(std::span<const ecs::Entity>{});
		CHECK(qLookup.count() == 1);
		qLookup.each([&](ecs::Entity e) {
			CHECK(e == eRender);
		});
	}

	SUBCASE("module creates named scope hierarchies and scopes registration") {
		TestWorld twld;

		const auto moduleEntity = wld.module("gameplay.render");
		CHECK(moduleEntity != ecs::EntityBad);
		CHECK(wld.name(moduleEntity) == "render");
		CHECK(wld.get("gameplay.render") == moduleEntity);

		const auto gameplay = wld.get("gameplay");
		CHECK(gameplay != ecs::EntityBad);
		CHECK(wld.has(moduleEntity, ecs::Pair(ecs::ChildOf, gameplay)));

		const auto physicsModule = wld.module("gameplay.render.physics");
		CHECK(physicsModule != ecs::EntityBad);
		wld.scope(physicsModule, [&] {
			const auto& comp = wld.add<dummy::Position>();
			CHECK(wld.path(comp.entity) == "gameplay.render.physics.Position");
		});

		CHECK(wld.scope() == ecs::EntityBad);
	}

	SUBCASE("world exposes unified lookup and component naming metadata") {
		TestWorld twld;

		const auto gameplay = wld.add();
		wld.name(gameplay, "gameplay");

		const auto render = wld.add();
		wld.name(render, "render");
		wld.child(render, gameplay);

		ecs::Entity gameplayPos = ecs::EntityBad;
		ecs::Entity renderPos = ecs::EntityBad;
		wld.scope(gameplay, [&] {
			gameplayPos = wld.add<Position>().entity;

			wld.scope(render, [&] {
				renderPos = wld.add<dummy::Position>().entity;
			});
		});

		CHECK(wld.get("gameplay") == gameplay);
		CHECK(wld.get("gameplay.render") == render);
		CHECK(wld.get("gameplay.Position") == gameplayPos);
		CHECK(wld.get("gameplay.render.Position") == renderPos);
		CHECK(wld.get("Position") == gameplayPos);

		CHECK(wld.symbol(renderPos) == "dummy::Position");
		CHECK(wld.path(renderPos) == "gameplay.render.Position");
		CHECK(wld.alias(renderPos).empty());
		CHECK(wld.display_name(renderPos) == "gameplay.render.Position");

		cnt::darray<ecs::Entity> out;
		wld.resolve(out, "Position");
		CHECK(out.size() == 1);
		CHECK(out[0] == gameplayPos);

		CHECK(wld.alias(renderPos, "RenderPosition"));
		CHECK(wld.alias(renderPos) == "RenderPosition");
		CHECK(wld.display_name(renderPos) == "RenderPosition");
		CHECK(wld.get("RenderPosition") == renderPos);

		CHECK(wld.path(renderPos, "gameplay.render.RenderPosition"));
		CHECK(wld.path(renderPos) == "gameplay.render.RenderPosition");
		CHECK(wld.get("gameplay.render.RenderPosition") == renderPos);

		wld.resolve(out, "gameplay.render");
		CHECK(out.size() == 1);
		CHECK(out[0] == render);
	}

	SUBCASE("world lookup prefers ordinary entity names over scoped component matches") {
		TestWorld twld;

		const auto gameplay = wld.add();
		wld.name(gameplay, "gameplay");

		const auto render = wld.add();
		wld.name(render, "render");
		wld.child(render, gameplay);

		const auto namedEntity = wld.add();
		wld.name(namedEntity, "Device");

		ecs::Entity scopedDevice = ecs::EntityBad;
		wld.scope(render, [&] {
			scopedDevice = wld.add("Render::Device", 0, ecs::DataStorageType::Table, 1).entity;
		});

		CHECK(wld.alias("Device") == ecs::EntityBad);

		wld.scope(render, [&] {
			CHECK(wld.get("Device") == namedEntity);
		});

		cnt::darray<ecs::Entity> out;
		wld.resolve(out, "Device");
		CHECK(out.size() == 1);
		CHECK(out[0] == namedEntity);
	}

	SUBCASE("world lookup prefers entity paths over component path collisions") {
		TestWorld twld;

		const auto gameplay = wld.add();
		wld.name(gameplay, "gameplay");

		const auto render = wld.add();
		wld.name(render, "render");
		wld.child(render, gameplay);

		ecs::Entity renderComp = ecs::EntityBad;
		wld.scope(gameplay, [&] {
			renderComp = wld.add("Gameplay::render", 0, ecs::DataStorageType::Table, 1).entity;
		});

		CHECK(wld.path("gameplay.render") == renderComp);
		CHECK(wld.get("gameplay.render") == render);

		cnt::darray<ecs::Entity> out;
		wld.resolve(out, "gameplay.render");
		CHECK(out.size() == 2);
		CHECK((out[0] == render || out[1] == render));
		CHECK((out[0] == renderComp || out[1] == renderComp));
	}

	SUBCASE("world lookup prefers exact component symbols over aliases") {
		TestWorld twld;

		const auto exactSymbolComp = wld.add("Gameplay::Device", 0, ecs::DataStorageType::Table, 1).entity;
		const auto aliasComp = wld.add("OtherDevice", 0, ecs::DataStorageType::Table, 1).entity;
		CHECK(wld.alias(aliasComp, "Gameplay::Device"));

		CHECK(wld.alias("Gameplay::Device") == aliasComp);
		CHECK(wld.get("Gameplay::Device") == exactSymbolComp);

		cnt::darray<ecs::Entity> out;
		wld.resolve(out, "Gameplay::Device");
		CHECK(out.size() == 2);
		CHECK((out[0] == exactSymbolComp || out[1] == exactSymbolComp));
		CHECK((out[0] == aliasComp || out[1] == aliasComp));
	}

	SUBCASE("duplicate aliases are rejected") {
		NonUniqueNameOpsGuard guard;
		TestWorld twld;

		const auto entityA = wld.add();
		const auto entityB = wld.add();

		CHECK(wld.alias(entityA, "Device"));
		CHECK_FALSE(wld.alias(entityB, "Device"));
		CHECK(wld.alias(entityA) == "Device");
		CHECK(wld.alias(entityB).empty());
		CHECK(wld.alias("Device") == entityA);
		CHECK(wld.get("Device") == entityA);

		cnt::darray<ecs::Entity> out;
		wld.resolve(out, "Device");
		CHECK(out.size() == 1);
		CHECK(out[0] == entityA);
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

TEST_CASE("Relationship wildcard source traversal cache refreshes after change") {
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

TEST_CASE("Relationship wildcard target traversal cache refreshes after change") {
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

TEST_CASE("Relationship wildcard target traversal supports many exclusive dontfragment relations") {
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

TEST_CASE("Upward traversal stops at disabled ancestors") {
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

TEST_CASE("Query entity each bfs prunes disabled ancestor barriers") {
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
			GAIA_STRCPY(tmp, MaxLen, original.data());
			verify(0);
		}

		{
			NonUniqueNameOpsGuard guard;
			auto e1 = wld.add();
			wld.name(e1, original.data());
			CHECK(wld.name(e1).empty());
			CHECK(wld.get(original.data(), original.size()) == e);
		}

		wld.name(e, nullptr);
		CHECK(wld.get(original.data(), original.size()) == ecs::EntityBad);
		CHECK(wld.name(e).empty());

		wld.name(e, original.data());
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
			GAIA_STRCPY(tmp, MaxLen, original.data());
			verify(0);
		}

		wld.name(e, nullptr);
		CHECK(wld.get(original.data(), original.size()) == ecs::EntityBad);
		CHECK(wld.name(e).empty());

		wld.name_raw(e, original.data());
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
		NonUniqueNameOpsGuard guard;

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

TEST_CASE("Copy preserves sparse component data") {
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

TEST_CASE("Query - cached component query sees entities added after creation") {
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

TEST_CASE("Query - cached OR query sees matches added after creation") {
	TestWorld twld;

	auto tagA = wld.add();
	auto tagB = wld.add();
	auto qCached = wld.query().or_(tagA).or_(tagB);

	// Compile/cache before any matching archetype exists.
	CHECK(qCached.count() == 0);
	CHECK(wld.query<false>().or_(tagA).or_(tagB).count() == 0);

	// Add matching archetype after query creation.
	auto e = wld.add();
	wld.add(e, tagA);

	CHECK(qCached.count() == 1);
	CHECK(wld.query<false>().or_(tagA).or_(tagB).count() == 1);
}

TEST_CASE("Query - cached OR query eagerly tracks secondary selector archetypes") {
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

TEST_CASE("Query - cached exact and OR query eagerly tracks matching archetypes") {
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

TEST_CASE("Query - cached exact and ANY query eagerly tracks matching archetypes") {
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

TEST_CASE("Query - cached exact and ANY query ignores archetypes without positive selectors") {
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

TEST_CASE("Query - cached structural query eagerly tracks matching archetypes") {
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

TEST_CASE("Query - broad-first exact query still tracks selective match") {
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

TEST_CASE("Query - cached single-term structural query eagerly tracks matching archetypes") {
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

TEST_CASE("Query - cached four-term structural query eagerly tracks matching archetypes") {
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

TEST_CASE("Query - cached Is query sees inherited archetypes after refresh") {
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

TEST_CASE("Query - direct Is query matches only direct stored edges") {
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
	(void)wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.all<Position>()
			.on_each([&](ecs::Iter&) {
				++positionHits;
			})
			.entity();

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

TEST_CASE("Prefab - Inherit policy resolves through the prefab until overridden locally") {
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

TEST_CASE("Prefab - explicit override materializes inherited ownership") {
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

TEST_CASE("Prefab - explicit override supports inherited tags") {
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

TEST_CASE("Is inheritance - explicit override materializes inherited ownership") {
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

TEST_CASE("Is inheritance - explicit override supports inherited tags") {
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

TEST_CASE("Prefab - inherited component queries see instances and materialize local overrides on write") {
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

TEST_CASE(
		"Is inheritance - inherited component queries see derived entities and materialize local overrides on write") {
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

TEST_CASE("Is inheritance - explicit override preserves query membership") {
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

TEST_CASE("Prefab - inherited sparse component queries see instances and materialize local overrides on write") {
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

TEST_CASE("Prefab - inherited writable query updates multiple instances from a stable snapshot") {
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

TEST_CASE("Prefab - inherited Iter query term access resolves and writes overrides locally") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(prefabAnimal, {5, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto instance = wld.instantiate(prefabAnimal);

	auto qRead = wld.query().all<Position>();
	float xRead = 0.0f;
	qRead.each([&](ecs::Iter& it) {
		auto posView = it.view<Position>(1);
		GAIA_EACH(it) {
			xRead += posView[i].x;
		}
	});
	CHECK(xRead == doctest::Approx(5.0f));

	auto qWrite = wld.query().all<Position&>();
	qWrite.each([&](ecs::Iter& it) {
		auto posView = it.view_mut<Position>(1);
		GAIA_EACH(it) {
			posView[i].x += 4.0f;
		}
	});

	CHECK(wld.has_direct(instance, position));
	CHECK(wld.get<Position>(instance).x == doctest::Approx(9.0f));
	CHECK(wld.get<Position>(prefabAnimal).x == doctest::Approx(5.0f));
}

TEST_CASE("Prefab - inherited Iter SoA query term access resolves and writes overrides locally") {
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
		auto posView = it.view<PositionSoA>(1);
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
		auto posView = it.view_mut<PositionSoA>(1);
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

TEST_CASE("Prefab - explicit override supports inherited SoA components") {
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

TEST_CASE("Prefab - explicit override supports inherited sparse components") {
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

TEST_CASE("Prefab - explicit override by id supports inherited runtime sparse components") {
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

TEST_CASE("Instantiate - non-prefab parented fallback behaves like copy plus parent") {
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

TEST_CASE("Instantiate_n - non-prefab parented fallback supports CopyIter callbacks") {
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

TEST_CASE("Instantiate_n - non-prefab parented fallback uses entity callbacks after parenting") {
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

TEST_CASE("Observer - instantiate_n non-prefab parented fallback matches Parent pair") {
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

TEST_CASE("Prefab - instantiate_n supports CopyIter callbacks for spawned roots") {
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

TEST_CASE("Prefab - instantiate_n inherited component queries materialize independent local overrides") {
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

TEST_CASE("Prefab - removing inherited prefab data stops resolving on existing instances") {
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

TEST_CASE("Prefab - instantiate ignores non-prefab Parent children") {
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

TEST_CASE("Query - Iter is query preserves component access") {
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

TEST_CASE("Query - cached direct Is query ignores transitive descendants") {
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
			wld.query<false>().all(ecs::Pair(ecs::Is, animal)).all(ecs::Pair(ecs::Is, herbivore), directOpts);
	CHECK(qDirectHerbivoreChildren.count() == 2);
	expect_exact_entities(qDirectHerbivoreChildren, {rabbit, hare});

	auto qExcludeDirectHerbivoreChildren =
			wld.query<false>().all(ecs::Pair(ecs::Is, animal)).no(ecs::Pair(ecs::Is, herbivore), directOpts);
	CHECK(qExcludeDirectHerbivoreChildren.count() == 4);
	expect_exact_entities(qExcludeDirectHerbivoreChildren, {animal, herbivore, carnivore, wolf});
}

TEST_CASE("Query - direct Is QueryInput item matches only direct stored edges") {
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

TEST_CASE("Query - cached dynamic query keeps warm reads stable until inputs change") {
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

TEST_CASE("Query - cached direct-source query keeps warm reads stable until source changes") {
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

TEST_CASE("Query - cached direct-source query ignores unrelated archetype changes") {
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

TEST_CASE("Query - cached direct-source query sees deleted source entities") {
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

TEST_CASE("Query - cached traversed-source query keeps warm reads stable until traversal state changes") {
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

TEST_CASE("Query - cached traversed-source query ignores unrelated archetype changes") {
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

TEST_CASE("Query - dependency metadata classification") {
	TestWorld twld;

	auto rel = wld.add();
	auto source = wld.add();

	auto qStructural = wld.query().all<Position>().or_<Scale>().no<Acceleration>().any<Rotation>();
	const auto& depsStructural = qStructural.fetch().ctx().data.deps;
	CHECK(depsStructural.has(ecs::QueryCtx::DependencyHasPositiveTerms));
	CHECK(depsStructural.has(ecs::QueryCtx::DependencyHasNegativeTerms));
	CHECK(depsStructural.has(ecs::QueryCtx::DependencyHasAnyTerms));
	CHECK(!depsStructural.has(ecs::QueryCtx::DependencyHasSourceTerms));
	CHECK(!depsStructural.has(ecs::QueryCtx::DependencyHasVariableTerms));
	CHECK(depsStructural.create_selectors_view().size() == 1);
	CHECK(depsStructural.exclusions_view().size() == 1);
	CHECK(core::has(depsStructural.create_selectors_view(), wld.get<Position>()));
	CHECK(core::has(depsStructural.exclusions_view(), wld.get<Acceleration>()));

	auto qOrOnly = wld.query().or_<Position>().or_<Scale>();
	const auto& depsOrOnly = qOrOnly.fetch().ctx().data.deps;
	CHECK(depsOrOnly.has(ecs::QueryCtx::DependencyHasPositiveTerms));
	CHECK(depsOrOnly.create_selectors_view().size() == 2);
	CHECK(core::has(depsOrOnly.create_selectors_view(), wld.get<Position>()));
	CHECK(core::has(depsOrOnly.create_selectors_view(), wld.get<Scale>()));

	auto qSource = wld.query().all<Position>(ecs::QueryTermOptions{}.src(source));
	const auto& depsSource = qSource.fetch().ctx().data.deps;
	CHECK(depsSource.has(ecs::QueryCtx::DependencyHasSourceTerms));
	CHECK(!depsSource.has(ecs::QueryCtx::DependencyHasPositiveTerms));
	CHECK(depsSource.create_selectors_view().empty());
	CHECK(depsSource.src_entities_view().size() == 1);
	CHECK(core::has(depsSource.src_entities_view(), source));

	auto qVar = wld.query().all(ecs::Pair(rel, ecs::Var0));
	const auto& depsVar = qVar.fetch().ctx().data.deps;
	CHECK(depsVar.has(ecs::QueryCtx::DependencyHasVariableTerms));
	CHECK(!depsVar.has(ecs::QueryCtx::DependencyHasPositiveTerms));
	CHECK(depsVar.create_selectors_view().empty());
	CHECK(depsVar.relations_view().size() == 1);
	CHECK(core::has(depsVar.relations_view(), rel));

	auto qTrav = wld.query().all<Position>(ecs::QueryTermOptions{}.src(source).trav(rel));
	const auto& depsTrav = qTrav.fetch().ctx().data.deps;
	CHECK(depsTrav.has(ecs::QueryCtx::DependencyHasSourceTerms));
	CHECK(depsTrav.has(ecs::QueryCtx::DependencyHasTraversalTerms));
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
	CHECK(qSorted.fetch().ctx().data.deps.has(ecs::QueryCtx::DependencyHasSort));

	auto eats = wld.add();
	auto qGrouped = wld.query().all<Position>().group_by(eats);
	CHECK(qGrouped.fetch().ctx().data.deps.has(ecs::QueryCtx::DependencyHasGroup));
}

TEST_CASE("Query - create selector routing prefers narrowest ALL term") {
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

TEST_CASE("Query - create selector routing prefers wildcard pair over broad exact term on empty world") {
	TestWorld twld;

	auto rel = wld.add();
	auto q = wld.query().all<Position>().all(ecs::Pair(rel, ecs::All));
	const auto& deps = q.fetch().ctx().data.deps;
	CHECK(deps.create_selectors_view().size() == 1);
	CHECK(deps.create_selectors_view()[0] == ecs::Pair(rel, ecs::All));
}

TEST_CASE("Query - public cache mode and policy classification") {
	TestWorld twld;

	auto source = wld.add();
	auto rel = wld.add();

	auto qCachedImmediate = wld.query().all<Position>();
	auto qCachedLazy = wld.query().no<Position>();
	auto qCachedDynamic = wld.query().all<Position>(ecs::QueryTermOptions{}.src(source));
	auto qCachedDynamicOptIn =
			wld.query().cache_src_trav(ecs::MaxCacheSrcTrav).all<Position>(ecs::QueryTermOptions{}.src(source).trav(rel));
	auto qCachedVar = wld.query().all(ecs::Pair(rel, ecs::Var0));
	auto qUncachedImmediate = wld.query<false>().all<Position>();

	CHECK(qCachedImmediate.cache_mode() == ecs::QueryCacheMode::Shared);
	CHECK(qCachedImmediate.cache_policy() == ecs::QueryCachePolicy::Immediate);

	CHECK(qCachedLazy.cache_mode() == ecs::QueryCacheMode::Shared);
	CHECK(qCachedLazy.cache_policy() == ecs::QueryCachePolicy::Lazy);

	CHECK(qCachedDynamic.cache_mode() == ecs::QueryCacheMode::Shared);
	CHECK(qCachedDynamic.cache_policy() == ecs::QueryCachePolicy::Dynamic);
	CHECK(qCachedDynamic.cache_src_trav() == 0);

	CHECK(qCachedDynamicOptIn.cache_mode() == ecs::QueryCacheMode::Shared);
	CHECK(qCachedDynamicOptIn.cache_policy() == ecs::QueryCachePolicy::Dynamic);
	CHECK(qCachedDynamicOptIn.cache_src_trav() == 32);

	CHECK(qCachedVar.cache_mode() == ecs::QueryCacheMode::Shared);
	CHECK(qCachedVar.cache_policy() == ecs::QueryCachePolicy::Dynamic);

	CHECK(qUncachedImmediate.cache_mode() == ecs::QueryCacheMode::Private);
	CHECK(qUncachedImmediate.cache_policy() == ecs::QueryCachePolicy::Immediate);
}

TEST_CASE("Query - public cache kind construction") {
	TestWorld twld;

	ecs::Query::SilenceInvalidCacheKindAssertions = true;

	auto source = wld.add();
	auto rel = wld.add();

	auto qDefault = wld.query().cache_kind(ecs::QueryCacheKind::Default).all<Position>();
	auto qDefaultSrcTrav = wld.query()
														 .cache_kind(ecs::QueryCacheKind::Default)
														 .cache_src_trav(ecs::MaxCacheSrcTrav)
														 .all<Position>(ecs::QueryTermOptions{}.src(source).trav(rel));
	auto qAuto = wld.query().cache_kind(ecs::QueryCacheKind::Auto).no<Position>();
	auto qAutoDirectSrc =
			wld.query().cache_kind(ecs::QueryCacheKind::Auto).all<Position>(ecs::QueryTermOptions{}.src(source));
	auto qAutoTraversedSrc =
			wld.query().cache_kind(ecs::QueryCacheKind::Auto).all<Position>(ecs::QueryTermOptions{}.src(source).trav(rel));
	auto qAutoSrcTrav = wld.query()
													.cache_kind(ecs::QueryCacheKind::Auto)
													.cache_src_trav(ecs::MaxCacheSrcTrav)
													.all<Position>(ecs::QueryTermOptions{}.src(source).trav(rel));
	auto qAll = wld.query().cache_kind(ecs::QueryCacheKind::All).all<Position>();
	auto qAllFail = wld.query().cache_kind(ecs::QueryCacheKind::All).no<Position>();
	auto qAllDynamic =
			wld.query().cache_kind(ecs::QueryCacheKind::All).all<Position>(ecs::QueryTermOptions{}.src(source));
	auto qAllSrcTrav = wld.query()
												 .cache_kind(ecs::QueryCacheKind::All)
												 .cache_src_trav(ecs::MaxCacheSrcTrav)
												 .all<Position>(ecs::QueryTermOptions{}.src(source).trav(rel));
	auto qDynamic =
			wld.query().cache_kind(ecs::QueryCacheKind::Default).all<Position>(ecs::QueryTermOptions{}.src(source));

	CHECK(qDefault.cache_kind() == ecs::QueryCacheKind::Default);
	CHECK(qDefault.valid());
	CHECK(qDefault.cache_mode() == ecs::QueryCacheMode::Shared);
	CHECK(qDefault.cache_policy() == ecs::QueryCachePolicy::Immediate);

	CHECK(qDefaultSrcTrav.cache_kind() == ecs::QueryCacheKind::Default);
	CHECK(qDefaultSrcTrav.valid());
	CHECK(qDefaultSrcTrav.cache_src_trav() > 0);

	CHECK(qAuto.cache_kind() == ecs::QueryCacheKind::Auto);
	CHECK(qAuto.valid());
	CHECK(qAuto.cache_mode() == ecs::QueryCacheMode::Shared);
	CHECK(qAuto.cache_policy() == ecs::QueryCachePolicy::Lazy);

	CHECK(qAutoDirectSrc.cache_kind() == ecs::QueryCacheKind::Auto);
	CHECK(qAutoDirectSrc.valid());
	CHECK(qAutoDirectSrc.cache_policy() == ecs::QueryCachePolicy::Dynamic);
	CHECK(qAutoDirectSrc.cache_src_trav() == 0);

	CHECK(qAutoTraversedSrc.cache_kind() == ecs::QueryCacheKind::Auto);
	CHECK(qAutoTraversedSrc.valid());
	CHECK(qAutoTraversedSrc.cache_policy() == ecs::QueryCachePolicy::Dynamic);
	CHECK(qAutoTraversedSrc.cache_src_trav() == 0);

	CHECK(qAutoSrcTrav.cache_kind() == ecs::QueryCacheKind::Auto);
	CHECK(!qAutoSrcTrav.valid());
	CHECK(qAutoSrcTrav.count() == 0);

	CHECK(qAll.cache_kind() == ecs::QueryCacheKind::All);
	CHECK(qAll.valid());
	CHECK(qAll.cache_mode() == ecs::QueryCacheMode::Shared);
	CHECK(qAll.cache_policy() == ecs::QueryCachePolicy::Immediate);

	CHECK(qAllFail.cache_kind() == ecs::QueryCacheKind::All);
	CHECK(!qAllFail.valid());
	CHECK(qAllFail.count() == 0);

	CHECK(qAllDynamic.cache_kind() == ecs::QueryCacheKind::All);
	CHECK(!qAllDynamic.valid());
	CHECK(qAllDynamic.count() == 0);

	CHECK(qAllSrcTrav.cache_kind() == ecs::QueryCacheKind::All);
	CHECK(!qAllSrcTrav.valid());
	CHECK(qAllSrcTrav.count() == 0);

	CHECK(qDynamic.valid());
	CHECK(qDynamic.cache_policy() == ecs::QueryCachePolicy::Dynamic);

	ecs::Query::SilenceInvalidCacheKindAssertions = false;
}

TEST_CASE("Query - cache_src_trav affects cache lookup hash only for traversed source queries") {
	TestWorld twld;

	auto root = wld.add();
	auto leaf = wld.add();
	wld.child(leaf, root);
	auto qNoSource = wld.query().all<Position>();
	auto qNoSourceSrcTrav = wld.query().cache_src_trav(ecs::MaxCacheSrcTrav).all<Position>();
	auto qDirectSource = wld.query().all<Position>(ecs::QueryTermOptions{}.src(root));
	auto qDirectSourceSrcTrav =
			wld.query().cache_src_trav(ecs::MaxCacheSrcTrav).all<Position>(ecs::QueryTermOptions{}.src(root));
	auto qTraversedSource = wld.query().all<Position>(ecs::QueryTermOptions{}.src(leaf).trav());
	auto qTraversedSourceSrcTrav =
			wld.query().cache_src_trav(ecs::MaxCacheSrcTrav).all<Position>(ecs::QueryTermOptions{}.src(leaf).trav());

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

TEST_CASE("Query - cached broad NOT query refreshes lazily after archetype creation") {
	TestWorld twld;

	auto excluded = wld.add();
	auto q = wld.query().no(excluded);
	auto qUncached = wld.query<false>().no(excluded);
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

TEST_CASE("Query - cached broad NOT query ignores non-matching archetypes") {
	TestWorld twld;

	auto excluded = wld.add();
	auto q = wld.query().no(excluded);
	auto qUncached = wld.query<false>().no(excluded);
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

TEST_CASE("Query - cached broad NOT wildcard pair query refreshes lazily after archetype creation") {
	TestWorld twld;

	auto relExcluded = wld.add();
	auto relOther = wld.add();
	auto tgt = wld.add();
	auto q = wld.query().no(ecs::Pair(relExcluded, ecs::All));
	auto qUncached = wld.query<false>().no(ecs::Pair(relExcluded, ecs::All));
	auto& info = q.fetch();
	q.match_all(info);
	const auto archetypeCntBefore = info.cache_archetype_view().size();

	auto e = wld.add();
	wld.add(e, ecs::Pair(relOther, tgt));

	CHECK(info.cache_archetype_view().size() == archetypeCntBefore);
	CHECK(q.count() == qUncached.count());
	CHECK(info.cache_archetype_view().size() == archetypeCntBefore + 1);
}

TEST_CASE("Query - cached broad NOT wildcard pair query ignores excluded archetypes") {
	TestWorld twld;

	auto relExcluded = wld.add();
	auto tgt = wld.add();
	auto q = wld.query().no(ecs::Pair(relExcluded, ecs::All));
	auto qUncached = wld.query<false>().no(ecs::Pair(relExcluded, ecs::All));
	auto& info = q.fetch();
	q.match_all(info);
	const auto archetypeCntBefore = info.cache_archetype_view().size();

	auto e = wld.add();
	wld.add(e, ecs::Pair(relExcluded, tgt));

	CHECK(info.cache_archetype_view().size() == archetypeCntBefore);
	CHECK(q.count() == qUncached.count());
}

TEST_CASE("Query - cached multi-NOT query refreshes lazily after archetype creation") {
	TestWorld twld;

	auto excludedA = wld.add();
	auto excludedB = wld.add();
	auto included = wld.add();
	auto q = wld.query().no(excludedA).no(excludedB);
	auto qUncached = wld.query<false>().no(excludedA).no(excludedB);
	auto& info = q.fetch();
	q.match_all(info);
	const auto archetypeCntBefore = info.cache_archetype_view().size();

	auto e = wld.add();
	wld.add(e, included);

	CHECK(info.cache_archetype_view().size() == archetypeCntBefore);
	CHECK(q.count() == qUncached.count());
	CHECK(info.cache_archetype_view().size() == archetypeCntBefore + 1);
}

TEST_CASE("Query - cached multi-NOT query ignores archetypes with excluded ids") {
	TestWorld twld;

	auto excludedA = wld.add();
	auto excludedB = wld.add();
	auto q = wld.query().no(excludedA).no(excludedB);
	auto qUncached = wld.query<false>().no(excludedA).no(excludedB);
	auto& info = q.fetch();
	q.match_all(info);
	const auto archetypeCntBefore = info.cache_archetype_view().size();

	auto e = wld.add();
	wld.add(e, excludedB);

	CHECK(info.cache_archetype_view().size() == archetypeCntBefore);
	CHECK(q.count() == qUncached.count());
}

TEST_CASE("Query - cached structural query picks up new matching archetype after warm match") {
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

TEST_CASE("Query - cached wildcard pair queries eagerly track matching archetypes") {
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

TEST_CASE("Query - cached exact and wildcard pair query eagerly tracks matching archetypes") {
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

TEST_CASE("Query - cached exact and NOT query eagerly tracks matching archetypes") {
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

TEST_CASE("Query - cached exact and NOT query ignores excluded archetype creation") {
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

TEST_CASE("Query - cached any-pair query eagerly tracks matching archetypes") {
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

TEST_CASE("Query - cached any-pair query count refreshes after pair removal") {
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

TEST_CASE("Query - cached wildcard pair query stays stable for pair-heavy archetypes") {
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

TEST_CASE("Query - cached sorted query refreshes lazily after archetype creation") {
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

TEST_CASE("Query - cached sorted query exact sort term lookup matches chunk columns across archetypes") {
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

TEST_CASE("Query - cached sorted query exact external sort term lookup matches chunk columns across archetypes") {
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

TEST_CASE("Query - cached grouped query refreshes lazily after archetype creation") {
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

TEST_CASE("Query - cached relation wildcard query survives repeated pair additions") {
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

TEST_CASE("Component index tracks exact and wildcard pair match counts") {
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

TEST_CASE("Component index exact term entries follow add and delete archetype moves") {
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

TEST_CASE("Query - exact owned term matcher fast path preserves inherited fallback") {
	TestWorld twld;

	const auto owned = wld.add();
	const auto ownedEntity = wld.add();
	wld.add(ownedEntity, owned);

	const auto prefab = wld.prefab();
	wld.add(prefab, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
	wld.add<Position>(prefab, {1, 2, 3});

	const auto instance = wld.instantiate(prefab);

	auto qOwned = wld.query<false>().all(owned);
	CHECK(qOwned.count() == 1);
	expect_exact_entities(qOwned, {ownedEntity});

	auto qInherited = wld.query<false>().all<Position>();
	CHECK(qInherited.count() == 1);
	expect_exact_entities(qInherited, {instance});
}

TEST_CASE("Query - uncached query state is not immediately updated by shared cache propagation") {
	TestWorld twld;

	auto qCached = wld.query().all<Position>();
	auto qUncached = wld.query<false>().all<Position>();

	auto& cachedInfo = qCached.fetch();
	auto& uncachedInfo = qUncached.fetch();
	qCached.match_all(cachedInfo);
	qUncached.match_all(uncachedInfo);

	CHECK(cachedInfo.cache_archetype_view().empty());
	CHECK(uncachedInfo.cache_archetype_view().empty());

	auto e = wld.add();
	wld.add<Position>(e, {1, 0, 0});

	CHECK(cachedInfo.cache_archetype_view().size() == 1);
	CHECK(uncachedInfo.cache_archetype_view().empty());
	CHECK(qCached.count() == 1);
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

TEST_CASE("Query - cached structural query stays stable across eager add and gc") {
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

TEST_CASE("Query - cached reverse index stays stable across repeated eager add gc and destruction") {
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
	SUBCASE("size class thresholds") {
		CHECK(ecs::mem_block_size_type(1) == 0);
		CHECK(ecs::mem_block_size_type(ecs::MinMemoryBlockSize) == 0);
		CHECK(ecs::mem_block_size_type(ecs::MinMemoryBlockSize + 1) == 1);
		CHECK(ecs::mem_block_size_type(ecs::MinMemoryBlockSize * 2) == 1);
		CHECK(ecs::mem_block_size_type(ecs::MinMemoryBlockSize * 2 + 1) == 2);
		CHECK(ecs::mem_block_size_type(ecs::MaxMemoryBlockSize) == 2);
	}

	SUBCASE("stats report used memory per size class") {
		auto& alloc = ecs::ChunkAllocator::get();
		alloc.flush();

		constexpr auto NBlocks = ecs::detail::MemoryPage::NBlocks;

		void* p8k = alloc.alloc(ecs::MinMemoryBlockSize);
		void* p16k = alloc.alloc(ecs::MinMemoryBlockSize * 2);
		void* p32k = alloc.alloc(ecs::MaxMemoryBlockSize);

		{
			const auto stats = alloc.stats();
			CHECK(stats.stats[0].num_pages == 1);
			CHECK(stats.stats[1].num_pages == 1);
			CHECK(stats.stats[2].num_pages == 1);
			CHECK(stats.stats[0].num_pages_free == 1);
			CHECK(stats.stats[1].num_pages_free == 1);
			CHECK(stats.stats[2].num_pages_free == 1);

			CHECK(stats.stats[0].mem_total == (uint64_t)ecs::mem_block_size(0) * NBlocks);
			CHECK(stats.stats[1].mem_total == (uint64_t)ecs::mem_block_size(1) * NBlocks);
			CHECK(stats.stats[2].mem_total == (uint64_t)ecs::mem_block_size(2) * NBlocks);

			CHECK(stats.stats[0].mem_used == ecs::mem_block_size(0));
			CHECK(stats.stats[1].mem_used == ecs::mem_block_size(1));
			CHECK(stats.stats[2].mem_used == ecs::mem_block_size(2));
		}

		alloc.free(p8k);
		alloc.free(p16k);
		alloc.free(p32k);
		alloc.flush();

		{
			const auto stats = alloc.stats();
			CHECK(stats.stats[0].num_pages == 1);
			CHECK(stats.stats[1].num_pages == 1);
			CHECK(stats.stats[2].num_pages == 1);
			CHECK(stats.stats[0].num_pages_free == 1);
			CHECK(stats.stats[1].num_pages_free == 1);
			CHECK(stats.stats[2].num_pages_free == 1);
			CHECK(stats.stats[0].mem_total == (uint64_t)ecs::mem_block_size(0) * NBlocks);
			CHECK(stats.stats[1].mem_total == (uint64_t)ecs::mem_block_size(1) * NBlocks);
			CHECK(stats.stats[2].mem_total == (uint64_t)ecs::mem_block_size(2) * NBlocks);
			CHECK(stats.stats[0].mem_used == 0);
			CHECK(stats.stats[1].mem_used == 0);
			CHECK(stats.stats[2].mem_used == 0);
		}

		alloc.flush(true);
		{
			const auto stats = alloc.stats();
			CHECK(stats.stats[0].num_pages == 0);
			CHECK(stats.stats[1].num_pages == 0);
			CHECK(stats.stats[2].num_pages == 0);
			CHECK(stats.stats[0].num_pages_free == 0);
			CHECK(stats.stats[1].num_pages_free == 0);
			CHECK(stats.stats[2].num_pages_free == 0);
			CHECK(stats.stats[0].mem_total == 0);
			CHECK(stats.stats[1].mem_total == 0);
			CHECK(stats.stats[2].mem_total == 0);
			CHECK(stats.stats[0].mem_used == 0);
			CHECK(stats.stats[1].mem_used == 0);
			CHECK(stats.stats[2].mem_used == 0);
		}
	}

	SUBCASE("stats track full and spill pages") {
		auto& alloc = ecs::ChunkAllocator::get();
		alloc.flush();

		constexpr auto NBlocks = ecs::detail::MemoryPage::NBlocks;

		void* blocks[NBlocks + 1]{};
		GAIA_FOR(NBlocks) {
			blocks[i] = alloc.alloc(ecs::MaxMemoryBlockSize);
		}

		{
			const auto stats = alloc.stats();
			CHECK(stats.stats[2].num_pages == 1);
			CHECK(stats.stats[2].num_pages_free == 0);
			CHECK(stats.stats[2].mem_total == (uint64_t)ecs::mem_block_size(2) * NBlocks);
			CHECK(stats.stats[2].mem_used == (uint64_t)ecs::mem_block_size(2) * NBlocks);
		}

		blocks[NBlocks] = alloc.alloc(ecs::MaxMemoryBlockSize);
		{
			const auto stats = alloc.stats();
			CHECK(stats.stats[2].num_pages == 2);
			CHECK(stats.stats[2].num_pages_free == 1);
			CHECK(stats.stats[2].mem_total == (uint64_t)ecs::mem_block_size(2) * NBlocks * 2);
			CHECK(stats.stats[2].mem_used == (uint64_t)ecs::mem_block_size(2) * (NBlocks + 1));
		}

		// Freeing a block from a full page should move it back to the free list.
		alloc.free(blocks[0]);
		blocks[0] = nullptr;
		{
			const auto stats = alloc.stats();
			CHECK(stats.stats[2].num_pages == 2);
			CHECK(stats.stats[2].num_pages_free == 2);
			CHECK(stats.stats[2].mem_used == (uint64_t)ecs::mem_block_size(2) * NBlocks);
		}

		for (void* p: blocks) {
			if (p != nullptr)
				alloc.free(p);
		}
		alloc.flush();

		{
			const auto stats = alloc.stats();
			CHECK(stats.stats[2].num_pages == 1);
			CHECK(stats.stats[2].num_pages_free == 1);
			CHECK(stats.stats[2].mem_total == (uint64_t)ecs::mem_block_size(2) * NBlocks);
			CHECK(stats.stats[2].mem_used == 0);
		}

		alloc.flush(true);
		{
			const auto stats = alloc.stats();
			CHECK(stats.stats[2].num_pages == 0);
			CHECK(stats.stats[2].num_pages_free == 0);
			CHECK(stats.stats[2].mem_total == 0);
			CHECK(stats.stats[2].mem_used == 0);
		}
	}

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

template <typename TQuery>
void Test_Query_Filter_Changed_Order_NoSystems() {
	constexpr bool UseCachedQuery = std::is_same_v<TQuery, ecs::Query>;

	TestWorld twld;
	struct Marker {};
	struct A {
		int value;
	};
	struct B {
		int value;
	};

	const auto e = wld.add();
	wld.add<Marker>(e);
	wld.add<A>(e, {1});
	wld.add<B>(e, {2});

	// Intentionally reversed relative to canonical component order.
	auto q = wld.query<UseCachedQuery>() //
							 .template all<Marker>()
							 .template all<A>()
							 .template all<B>()
							 .template changed<B>()
							 .template changed<A>();

	CHECK(q.count() == 1);
	expect_exact_entities(q, {e});

	// No writes between runs.
	CHECK(q.count() == 0);
	expect_exact_entities(q, {});

	wld.set<A>(e) = {3};
	CHECK(q.count() == 1);
	expect_exact_entities(q, {e});

	CHECK(q.count() == 0);
	expect_exact_entities(q, {});

	wld.set<B>(e) = {4};
	CHECK(q.count() == 1);
	expect_exact_entities(q, {e});
}

TEST_CASE("Query Filter - changed order no systems") {
	SUBCASE("Cached query") {
		Test_Query_Filter_Changed_Order_NoSystems<ecs::Query>();
	}
	SUBCASE("Non-cached query") {
		Test_Query_Filter_Changed_Order_NoSystems<ecs::QueryUncached>();
	}
}

template <typename TQuery>
void Test_Query_Filter_Changed_Or_Missing_Component() {
	constexpr bool UseCachedQuery = std::is_same_v<TQuery, ecs::Query>;

	TestWorld twld;
	struct Marker {};
	struct A {
		int value;
	};
	struct B {
		int value;
	};

	const auto eA = wld.add();
	wld.add<Marker>(eA);
	wld.add<A>(eA, {1});

	const auto eB = wld.add();
	wld.add<Marker>(eB);
	wld.add<B>(eB, {2});

	// Archetypes can match with only one of OR terms present.
	// Both changed filters must remain safe.
	auto q = wld.query<UseCachedQuery>()
							 .template all<Marker>()
							 .template or_<A>()
							 .template or_<B>()
							 .template changed<B>()
							 .template changed<A>();

	CHECK(q.count() == 2);
	expect_exact_entities(q, {eA, eB});

	CHECK(q.count() == 0);
	expect_exact_entities(q, {});

	wld.set<A>(eA) = {3};
	CHECK(q.count() == 1);
	expect_exact_entities(q, {eA});

	wld.set<B>(eB) = {4};
	CHECK(q.count() == 1);
	expect_exact_entities(q, {eB});
}

TEST_CASE("Query Filter - changed OR terms") {
	SUBCASE("Cached query") {
		Test_Query_Filter_Changed_Or_Missing_Component<ecs::Query>();
	}
	SUBCASE("Non-cached query") {
		Test_Query_Filter_Changed_Or_Missing_Component<ecs::QueryUncached>();
	}
}

TEST_CASE("Query Filter - changed order cache key canonicalization") {
	TestWorld twld;
	struct Marker {};
	struct A {
		int value;
	};
	struct B {
		int value;
	};

	const auto e = wld.add();
	wld.add<Marker>(e);
	wld.add<A>(e, {1});
	wld.add<B>(e, {2});

	ecs::Query qAB = wld.query() //
											 .template all<Marker>()
											 .template all<A>()
											 .template all<B>()
											 .template changed<A>()
											 .template changed<B>();
	ecs::Query qBA = wld.query() //
											 .template all<Marker>()
											 .template all<A>()
											 .template all<B>()
											 .template changed<B>()
											 .template changed<A>();

	CHECK(qAB.count() == 1);
	CHECK(qBA.count() == 1);
	CHECK(qAB.id() == qBA.id());
	CHECK(qAB.gen() == qBA.gen());
}

TEST_CASE("Query Filter - systems") {
	uint32_t expectedCnt = 0;
	uint32_t actualCnt = 0;
	uint32_t wsCnt = 0;
	uint32_t wssCnt = 0;
	TestWorld twld;

	auto e = wld.add();
	wld.add<Position>(e);

	// WriterSystem
	auto ws = wld.system()
								.name("WriterSystem")
								.no<ecs::System_>()
								.all<Position&>()
								.on_each([&](Position& a) {
									++wsCnt;
									(void)a;
								})
								.entity();
	// WriterSystemSilent
	auto wss = wld.system()
								 .name("WriterSystemSilent")
								 .no<ecs::System_>()
								 .all<Position&>()
								 .on_each([&](ecs::Iter& it) {
									 ++wssCnt;
									 auto posRWView = it.sview_mut<Position>();
									 (void)posRWView;
								 })
								 .entity();
	// ReaderSystem
	auto rs = wld.system()
								.name("ReaderSystem")
								.no<ecs::System_>()
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
static uint32_t g_query_sort_cmp_cnt = 0;
static int compare_position_counted([[maybe_unused]] const ecs::World& world, const void* pData0, const void* pData1) {
	++g_query_sort_cmp_cnt;

	const auto& p0 = *static_cast<const Position*>(pData0);
	const auto& p1 = *static_cast<const Position*>(pData1);
	if (p0.x < p1.x)
		return -1;
	if (p0.x > p1.x)
		return 1;
	return 0;
}
ecs::GroupId
group_by_rel([[maybe_unused]] const ecs::World& world, const ecs::Archetype& archetype, ecs::Entity groupBy) {
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
		auto qq = wld.query().all<Position>().group_by(eats, group_by_rel);

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

	SUBCASE("Doesn't resort after unrelated component write") {
		wld.add<Something>(e0, {false});
		wld.add<Something>(e1, {false});
		wld.add<Something>(e2, {false});
		wld.add<Something>(e3, {false});

		g_query_sort_cmp_cnt = 0;
		auto q = wld.query().all<Position>().all<Something>().sort_by<Position>(compare_position_counted);

		q.each([](ecs::Iter&) {});
		CHECK(g_query_sort_cmp_cnt > 0);

		g_query_sort_cmp_cnt = 0;
		wld.set<Something>(e0).value = true;
		q.each([](ecs::Iter&) {});
		CHECK(g_query_sort_cmp_cnt == 0);
	}

	SUBCASE("Resorts after entity order changes") {
		wld.add<Something>(e0, {false});
		wld.add<Something>(e1, {false});
		wld.add<Something>(e2, {false});
		wld.add<Something>(e3, {false});

		g_query_sort_cmp_cnt = 0;
		auto q = wld.query().all<Position>().all<Something>().sort_by<Position>(compare_position_counted);

		q.each([](ecs::Iter&) {});
		g_query_sort_cmp_cnt = 0;

		auto e4 = wld.add();
		wld.add<Position>(e4, {0, 0, 0});
		wld.add<Something>(e4, {true});
		q.each([](ecs::Iter&) {});
		CHECK(g_query_sort_cmp_cnt > 0);
	}
}

#if GAIA_SYSTEMS_ENABLED

TEST_CASE("System - simple") {
	uint32_t sys1_cnt = 0;
	uint32_t sys2_cnt = 0;
	uint32_t sys3_cnt = 0;
	bool sys3_run_before_sys1 = false;
	bool sys3_run_before_sys2 = false;
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
									//.name("sys1")
									.all<Position>()
									.all<Acceleration>() //
									.on_each([&](Position, Acceleration) {
										if (sys1_cnt == 0 && sys3_cnt > 0)
											sys3_run_before_sys1 = true;
										++sys1_cnt;
									});
	auto sys2 = wld.system()
									//.name("sys2")
									.all<Position>() //
									.on_each([&](ecs::Iter& it) {
										if (sys2_cnt == 0 && sys3_cnt > 0)
											sys3_run_before_sys2 = true;
										GAIA_EACH(it)++ sys2_cnt;
									});
	auto sys3 = wld.system()
									// TODO: Using names for the systems can break ordering after rebulid.
									//       Most likely an undefined behavior somewhere (maybe partial sort on systems?).
									//       Find out what is wrong.
									//.name("sys3")
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

TEST_CASE("System - dependency BFS order") {
	cnt::darr<char> order;
	TestWorld twld;

	auto e = wld.add();
	wld.add<Position>(e, {0, 0, 0});

	auto make_sys = [&](char id) {
		return wld.system().all<Position>().on_each([&order, id](Position) {
			order.push_back(id);
		});
	};

	auto sysRoot = make_sys('R');
	auto sysA = make_sys('A');
	auto sysC = make_sys('C');
	auto sysB = make_sys('B');

	// Dependency graph:
	//   R -> A -> C
	//   R -> B
	// BFS levels should execute: R, A+B, C.
	wld.add(sysA.entity(), {ecs::DependsOn, sysRoot.entity()});
	wld.add(sysB.entity(), {ecs::DependsOn, sysRoot.entity()});
	wld.add(sysC.entity(), {ecs::DependsOn, sysA.entity()});

	wld.update();

	CHECK(order.size() == 4);
	if (order.size() == 4) {
		CHECK(order[0] == 'R');
		CHECK(order[1] == 'A');
		CHECK(order[2] == 'B');
		CHECK(order[3] == 'C');
	}
}

TEST_CASE("System - world teardown drops cached query tracking before chunk destruction") {
	uint32_t runs = 0;

	{
		ecs::World world;

		auto entity = world.add();
		world.add<Position>(entity, {1, 2, 3});

		auto sysA = world.system().all<Position>().on_each([&](const Position&) {
			++runs;
		});
		auto sysB = world.system().all<Position>().on_each([](const Position&) {});

		world.add(sysB.entity(), {ecs::DependsOn, sysA.entity()});
		world.update();

		CHECK(runs == 1);
	}

	CHECK(runs == 1);
}

TEST_CASE("System - exec mode") {
	const uint32_t N = 10'000;

	std::mutex mtx;
	const ecs::Chunk* pPrevChunk = nullptr;
	uint32_t cnt0{};
	uint32_t cntChunks0{};
	uint32_t cnt1{};
	uint32_t cntChunks1{};

	TestWorld twld;

	auto create = [&]() {
		wld.build(wld.add()).add<Position>();
	};
	GAIA_FOR(N) create();

	auto s0 = wld.system() //
								.all<Position>() //
								.on_each([&](ecs::Iter& iter) {
									cnt0 += iter.size();
									cntChunks0 += (iter.chunk() != pPrevChunk);
									pPrevChunk = iter.chunk();
								})
								.mode(ecs::QueryExecType::Default);
	auto s1 = wld.system() //
								.all<Position>() //
								.on_each([&](ecs::Iter& iter) {
									std::scoped_lock lock(mtx);
									cnt1 += iter.size();
									cntChunks1 += (iter.chunk() != pPrevChunk);
									pPrevChunk = iter.chunk();
								});

	s0.exec();
	CHECK(cnt0 == N);
	CHECK(cntChunks0 > 1);

	pPrevChunk = nullptr;
	s1.mode(ecs::QueryExecType::Parallel).exec();
	CHECK(cnt0 == cnt1);
	CHECK(cntChunks0 == cntChunks1);

	pPrevChunk = nullptr;
	cnt1 = 0;
	cntChunks1 = 0;
	s1.mode(ecs::QueryExecType::ParallelEff).exec();
	CHECK(cnt0 == cnt1);
	CHECK(cntChunks0 == cntChunks1);

	pPrevChunk = nullptr;
	cnt1 = 0;
	cntChunks1 = 0;
	s1.mode(ecs::QueryExecType::ParallelPerf).exec();
	CHECK(cnt0 == cnt1);
	CHECK(cntChunks0 == cntChunks1);
}

TEST_CASE("System - is sugar matches semantic and direct Is terms") {
	TestWorld twld;

	auto animal = wld.add();
	auto mammal = wld.add();
	auto rabbit = wld.add();

	wld.as(mammal, animal);
	wld.as(rabbit, mammal);

	uint32_t semanticHits = 0;
	uint32_t directHits = 0;

	auto sysSemantic = wld.system().is(animal).on_each([&](ecs::Iter& it) {
		semanticHits += it.size();
	});

	auto sysDirect = wld.system().is(animal, ecs::QueryTermOptions{}.direct()).on_each([&](ecs::Iter& it) {
		directHits += it.size();
	});

	sysSemantic.exec();
	sysDirect.exec();

	CHECK(semanticHits == 3);
	CHECK(directHits == 1);
}

TEST_CASE("System - in sugar matches descendants but excludes the base entity") {
	TestWorld twld;

	auto animal = wld.add();
	auto mammal = wld.add();
	auto rabbit = wld.add();

	wld.as(mammal, animal);
	wld.as(rabbit, mammal);

	uint32_t hits = 0;

	auto sys = wld.system().in(animal).on_each([&](ecs::Iter& it) {
		hits += it.size();
	});

	sys.exec();

	CHECK(hits == 2);
}

TEST_CASE("System - prefabs are excluded by default and can be matched explicitly") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto entity = wld.add();

	wld.add<Position>(prefab, {1, 0, 0});
	wld.add<Position>(entity, {2, 0, 0});

	uint32_t defaultHits = 0;
	uint32_t prefabHits = 0;

	auto sysDefault = wld.system().all<Position>().on_each([&](ecs::Iter& it) {
		defaultHits += it.size();
	});
	auto sysMatchPrefab = wld.system().all<Position>().match_prefab().on_each([&](ecs::Iter& it) {
		prefabHits += it.size();
	});

	sysDefault.exec();
	sysMatchPrefab.exec();

	CHECK(defaultHits == 1);
	CHECK(prefabHits == 2);
}

TEST_CASE("System - inherited prefab component query sees instances and writes override locally") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(prefab, {4, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto instance = wld.instantiate(prefab);

	uint32_t hits = 0;
	auto sys = wld.system().all<Position&>().on_each([&](Position& pos) {
		++hits;
		pos.x += 2.0f;
	});

	sys.exec();

	CHECK(hits == 1);
	CHECK(wld.has_direct(instance, position));
	CHECK(wld.get<Position>(instance).x == doctest::Approx(6.0f));
	CHECK(wld.get<Position>(prefab).x == doctest::Approx(4.0f));
}

TEST_CASE("System - inherited Is component query sees derived entities and writes override locally") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto rabbit = wld.add();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(animal, {4, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
	wld.as(rabbit, animal);

	uint32_t hits = 0;
	auto sys = wld.system().all<Position&>().on_each([&](Position& pos) {
		++hits;
		pos.x += 2.0f;
	});

	sys.exec();

	CHECK(hits == 2);
	CHECK(wld.has_direct(rabbit, position));
	CHECK(wld.get<Position>(animal).x == doctest::Approx(6.0f));
	CHECK(wld.get<Position>(rabbit).x == doctest::Approx(8.0f));
}

TEST_CASE("System - inherited prefab sparse component query sees instances and writes override locally") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto position = wld.add<PositionSparse>().entity;
	wld.add<PositionSparse>(prefab, {4, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto instance = wld.instantiate(prefab);

	uint32_t hits = 0;
	auto sys = wld.system().all<PositionSparse&>().on_each([&](PositionSparse& pos) {
		++hits;
		pos.x += 2.0f;
	});

	sys.exec();

	CHECK(hits == 1);
	CHECK(wld.has_direct(instance, position));
	CHECK(wld.get<PositionSparse>(instance).x == doctest::Approx(6.0f));
	CHECK(wld.get<PositionSparse>(prefab).x == doctest::Approx(4.0f));
}

TEST_CASE("System - inherited prefab Iter query preserves term-indexed access") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(prefab, {4, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto instance = wld.instantiate(prefab);

	float xRead = 0.0f;
	auto sysRead = wld.system().all<Position>().on_each([&](ecs::Iter& it) {
		auto posView = it.view<Position>(1);
		GAIA_EACH(it) {
			xRead += posView[i].x;
		}
	});
	sysRead.exec();

	auto sysWrite = wld.system().all<Position&>().on_each([&](ecs::Iter& it) {
		auto posView = it.view_mut<Position>(1);
		GAIA_EACH(it) {
			posView[i].x += 3.0f;
		}
	});
	sysWrite.exec();

	CHECK(xRead == doctest::Approx(4.0f));
	CHECK(wld.has_direct(instance, position));
	CHECK(wld.get<Position>(instance).x == doctest::Approx(7.0f));
	CHECK(wld.get<Position>(prefab).x == doctest::Approx(4.0f));
}

TEST_CASE("System - inherited prefab Iter SoA query preserves term-indexed access") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto position = wld.add<PositionSoA>().entity;
	wld.add<PositionSoA>(prefab, {4, 5, 6});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto instance = wld.instantiate(prefab);

	float xRead = 0.0f;
	float zRead = 0.0f;
	auto sysRead = wld.system().all<PositionSoA>().on_each([&](ecs::Iter& it) {
		auto posView = it.view<PositionSoA>(1);
		auto xs = posView.template get<0>();
		auto zs = posView.template get<2>();
		GAIA_EACH(it) {
			xRead += xs[i];
			zRead += zs[i];
		}
	});
	sysRead.exec();

	auto sysWrite = wld.system().all<PositionSoA&>().on_each([&](ecs::Iter& it) {
		auto posView = it.view_mut<PositionSoA>(1);
		auto xs = posView.template set<0>();
		auto zs = posView.template set<2>();
		GAIA_EACH(it) {
			xs[i] = xs[i] + 2.0f;
			zs[i] = zs[i] + 3.0f;
		}
	});
	sysWrite.exec();

	CHECK(xRead == doctest::Approx(4.0f));
	CHECK(zRead == doctest::Approx(6.0f));

	CHECK(wld.has_direct(instance, position));
	const auto pos = wld.get<PositionSoA>(instance);
	CHECK(pos.x == doctest::Approx(6.0f));
	CHECK(pos.y == doctest::Approx(5.0f));
	CHECK(pos.z == doctest::Approx(9.0f));

	const auto prefabPos = wld.get<PositionSoA>(prefab);
	CHECK(prefabPos.x == doctest::Approx(4.0f));
	CHECK(prefabPos.y == doctest::Approx(5.0f));
	CHECK(prefabPos.z == doctest::Approx(6.0f));
}

TEST_CASE("System - typed is query uses semantic direct-seeded execution") {
	TestWorld twld;

	auto animal = wld.add();
	auto mammal = wld.add();
	auto rabbit = wld.add();

	wld.as(mammal, animal);
	wld.as(rabbit, mammal);

	wld.add<Position>(animal, {4, 0, 0});
	wld.add<Position>(mammal, {1, 0, 0});
	wld.add<Position>(rabbit, {2, 0, 0});

	uint32_t semanticHits = 0;
	float semanticX = 0.0f;
	uint32_t directHits = 0;
	float directX = 0.0f;

	auto sysSemantic = wld.system().is(animal).all<Position>().on_each([&](const Position& pos) {
		++semanticHits;
		semanticX += pos.x;
	});

	auto sysDirect =
			wld.system().is(animal, ecs::QueryTermOptions{}.direct()).all<Position>().on_each([&](const Position& pos) {
				++directHits;
				directX += pos.x;
			});

	sysSemantic.exec();
	sysDirect.exec();

	CHECK(semanticHits == 3);
	CHECK(semanticX == doctest::Approx(7.0f));
	CHECK(directHits == 1);
	CHECK(directX == doctest::Approx(1.0f));
}

TEST_CASE("System - Iter is query preserves term-indexed component access") {
	TestWorld twld;

	auto animal = wld.add();
	auto mammal = wld.add();
	auto rabbit = wld.add();

	wld.as(mammal, animal);
	wld.as(rabbit, mammal);

	wld.add<Position>(animal, {4, 0, 0});
	wld.add<Position>(mammal, {1, 0, 0});
	wld.add<Position>(rabbit, {2, 0, 0});

	float semanticX = 0.0f;
	float directX = 0.0f;

	auto sysSemantic = wld.system().is(animal).all<Position>().on_each([&](ecs::Iter& it) {
		auto posView = it.view<Position>(1);
		GAIA_EACH(it) {
			semanticX += posView[i].x;
		}
	});

	auto sysDirect =
			wld.system().is(animal, ecs::QueryTermOptions{}.direct()).all<Position>().on_each([&](ecs::Iter& it) {
				auto posView = it.view<Position>(1);
				GAIA_EACH(it) {
					directX += posView[i].x;
				}
			});

	sysSemantic.exec();
	sysDirect.exec();

	CHECK(semanticX == doctest::Approx(7.0f));
	CHECK(directX == doctest::Approx(1.0f));
}

TEST_CASE("System - DependsOn is respected for semantic Is queries") {
	TestWorld twld;

	auto animal = wld.add();
	auto mammal = wld.add();
	auto rabbit = wld.add();

	wld.as(mammal, animal);
	wld.as(rabbit, mammal);

	wld.add<Position>(animal, {4, 0, 0});
	wld.add<Position>(mammal, {1, 0, 0});
	wld.add<Position>(rabbit, {2, 0, 0});

	uint32_t semanticHits = 0;
	bool directRanTooEarly = false;
	float directObservedX = 0.0f;

	auto sysSemantic = wld.system().is(animal).all<Position&>().on_each([&](Position& pos) {
		++semanticHits;
		pos.x += 10.0f;
	});

	auto sysDirect =
			wld.system().is(animal, ecs::QueryTermOptions{}.direct()).all<Position>().on_each([&](const Position& pos) {
				if (semanticHits != 3)
					directRanTooEarly = true;
				directObservedX += pos.x;
			});

	wld.add(sysDirect.entity(), {ecs::DependsOn, sysSemantic.entity()});
	wld.update();

	CHECK_FALSE(directRanTooEarly);
	CHECK(semanticHits == 3);
	CHECK(directObservedX == doctest::Approx(11.0f));
}

TEST_CASE("System - nested same-world Is query does not corrupt matcher scratch") {
	TestWorld twld;

	auto animal = wld.add();
	auto mammal = wld.add();
	auto rabbit = wld.add();

	wld.as(mammal, animal);
	wld.as(rabbit, mammal);

	wld.add<Position>(animal, {1, 0, 0});
	wld.add<Position>(mammal, {2, 0, 0});
	wld.add<Position>(rabbit, {3, 0, 0});

	uint32_t outerHits = 0;
	uint32_t nestedHits = 0;
	float nestedSum = 0.0f;

	wld.system().all<Position>().on_each([&](const Position&) {
		++outerHits;

		auto qNested = wld.query().all<Position>().is(animal);
		qNested.each([&](const Position& pos) {
			++nestedHits;
			nestedSum += pos.x;
		});
	});

	wld.update();

	CHECK(outerHits == 3);
	CHECK(nestedHits == 9);
	CHECK(nestedSum == doctest::Approx(18.0f));
}

TEST_CASE("System - deep semantic Is direct exec") {
	TestWorld twld;

	auto make_is_fanout = [](ecs::World& world, uint32_t branches, uint32_t depth) {
		auto root = world.add();
		for (uint32_t i = 0; i < branches; ++i) {
			auto curr = root;
			for (uint32_t j = 0; j < depth; ++j) {
				auto next = world.add();
				world.add(next, ecs::Pair(ecs::Is, curr));
				world.add<Position>(next, {(float)i, (float)j, (float)(i + j)});
				curr = next;
			}
		}
		return root;
	};

	auto root = make_is_fanout(wld, 1024, 8);
	uint64_t sum = 0;

	auto sys = wld.system().all<Position>().is(root).on_each([&](const Position& pos) {
		sum += (uint64_t)(pos.x + pos.y + pos.z);
	});

	sys.exec();

	CHECK(sum == 8437760ULL);
}

TEST_CASE("System - deep semantic Is direct exec survives prior systems query each") {
	TestWorld twld;

	auto make_is_fanout = [](ecs::World& world, uint32_t branches, uint32_t depth) {
		auto root = world.add();
		for (uint32_t i = 0; i < branches; ++i) {
			auto curr = root;
			for (uint32_t j = 0; j < depth; ++j) {
				auto next = world.add();
				world.add(next, ecs::Pair(ecs::Is, curr));
				world.add<Position>(next, {(float)i, (float)j, (float)(i + j)});
				curr = next;
			}
		}
		return root;
	};

	auto root = make_is_fanout(wld, 1024, 8);
	uint64_t sum = 0;

	auto sys = wld.system().all<Position>().is(root).on_each([&](const Position& pos) {
		sum += (uint64_t)(pos.x + pos.y + pos.z);
	});

	wld.query().all(ecs::System).each([&](ecs::Entity) {});
	sys.exec();

	CHECK(sum == 8437760ULL);
}

TEST_CASE("Query - deep semantic Is survives prior systems query each") {
	TestWorld twld;

	auto make_is_fanout = [](ecs::World& world, uint32_t branches, uint32_t depth) {
		auto root = world.add();
		for (uint32_t i = 0; i < branches; ++i) {
			auto curr = root;
			for (uint32_t j = 0; j < depth; ++j) {
				auto next = world.add();
				world.add(next, ecs::Pair(ecs::Is, curr));
				world.add<Position>(next, {(float)i, (float)j, (float)(i + j)});
				curr = next;
			}
		}
		return root;
	};

	auto root = make_is_fanout(wld, 1024, 8);
	uint64_t sum = 0;

	auto sys = wld.system().all<Position>().is(root).on_each([&](const Position&) {});
	(void)sys;

	wld.query().all(ecs::System).each([&](ecs::Entity) {});

	auto q = wld.query().all<Position>().is(root);
	q.each([&](const Position& pos) {
		sum += (uint64_t)(pos.x + pos.y + pos.z);
	});

	CHECK(sum == 8437760ULL);
}

TEST_CASE("System - deep semantic Is survives prior direct Is rematch in another world") {
	auto make_is_fanout = [](ecs::World& world, uint32_t branches, uint32_t depth) {
		auto root = world.add();
		for (uint32_t i = 0; i < branches; ++i) {
			auto curr = root;
			for (uint32_t j = 0; j < depth; ++j) {
				auto next = world.add();
				world.add(next, ecs::Pair(ecs::Is, curr));
				world.add<Position>(next, {(float)i, (float)j, (float)(i + j)});
				curr = next;
			}
		}
		return root;
	};

	{
		TestWorld twld;
		auto root = make_is_fanout(wld, 1024, 8);
		auto q = wld.query().all<Position>().is(root, ecs::QueryTermOptions{}.direct());
		auto& qi = q.fetch();
		q.match_all(qi);
		CHECK(qi.cache_archetype_view().size() == 1);
	}

	{
		TestWorld twld;
		auto root = make_is_fanout(wld, 1024, 8);
		uint64_t sum = 0;

		wld.system().all<Position>().is(root).on_each([&](const Position& pos) {
			sum += (uint64_t)(pos.x + pos.y + pos.z);
		});

		wld.update();

		CHECK(sum == 8437760ULL);
	}
}

TEST_CASE("Observer - deep semantic Is matches_any survives prior systems query each") {
	TestWorld twld;

	auto make_is_fanout = [](ecs::World& world, uint32_t branches, uint32_t depth, cnt::darr<ecs::Entity>& leaves) {
		auto root = world.add();
		for (uint32_t i = 0; i < branches; ++i) {
			auto curr = root;
			for (uint32_t j = 0; j < depth; ++j) {
				auto next = world.add();
				world.add(next, ecs::Pair(ecs::Is, curr));
				world.add<Position>(next, {(float)i, (float)j, (float)(i + j)});
				curr = next;
			}
			leaves.push_back(curr);
		}
		return root;
	};

	cnt::darr<ecs::Entity> leaves;
	auto root = make_is_fanout(wld, 512, 8, leaves);
	auto observerEntity = wld.observer().event(ecs::ObserverEvent::OnAdd).is(root).on_each([](ecs::Iter&) {}).entity();

	wld.query().all(ecs::System).each([&](ecs::Entity) {});

	auto& observerData = wld.observers().data(observerEntity);
	auto& observerQueryInfo = observerData.query.fetch();

	uint32_t matches = 0;
	for (const auto leaf: leaves) {
		const auto& ec = wld.fetch(leaf);
		matches += (uint32_t)observerData.query.matches_any(observerQueryInfo, *ec.pArchetype, ecs::EntitySpan{&leaf, 1});
	}

	CHECK(matches == leaves.size());
}

TEST_CASE("System - deep hierarchy skips disabled subtrees while preserving local enabled bits") {
	TestWorld twld;

	auto e = wld.add();
	wld.add<Position>(e, {0, 0, 0});

	const auto grpA = wld.add();
	const auto subGrpA1 = wld.add();
	const auto subGrpA2 = wld.add();
	const auto grpB = wld.add();
	const auto subGrpB = wld.add();
	const auto grpC = wld.add();
	const auto subGrpC = wld.add();

	wld.child(subGrpA1, grpA);
	wld.child(subGrpA2, grpA);
	wld.child(subGrpB, grpB);
	wld.child(subGrpC, grpC);

	uint32_t hits[9] = {};

	auto make_sys = [&](uint32_t idx, ecs::Entity parent) {
		auto sys = wld.system().all<Position>().on_each([&, idx](Position) {
			++hits[idx];
		});
		wld.child(sys.entity(), parent);
		return sys.entity();
	};

	const auto sys1 = make_sys(0, subGrpA1);
	const auto sys2 = make_sys(1, subGrpA1);
	const auto sys3 = make_sys(2, subGrpA1);
	const auto sys4 = make_sys(3, subGrpA2);
	const auto sys5A = make_sys(4, subGrpA2);
	const auto sys5B = make_sys(5, subGrpB);
	const auto sys6 = make_sys(6, subGrpB);
	const auto sys10 = make_sys(7, subGrpC);
	const auto sys11 = make_sys(8, subGrpC);

	auto clear_hits = [&]() {
		GAIA_FOR(9) {
			hits[i] = 0;
		}
	};

	auto check_hits = [&](const bool expected[9]) {
		GAIA_FOR(9) {
			CHECK(hits[i] == (expected[i] ? 1u : 0u));
		}
	};

	const bool allEnabled[9] = {true, true, true, true, true, true, true, true, true};
	const bool subGrpA1Disabled[9] = {false, false, false, true, true, true, true, true, true};
	const bool subGrpCDisabled[9] = {true, true, true, true, true, true, true, false, false};
	const bool grpCDisabled[9] = {true, true, true, true, true, true, true, false, false};
	const bool sys10Disabled[9] = {true, true, true, true, true, true, true, false, true};
	const bool grpADisabled[9] = {false, false, false, false, false, true, true, true, true};

	clear_hits();
	wld.update();
	check_hits(allEnabled);

	wld.enable(subGrpA1, false);
	CHECK_FALSE(wld.enabled(subGrpA1));
	CHECK(wld.enabled(sys1));
	CHECK(wld.enabled(sys2));
	CHECK(wld.enabled(sys3));
	clear_hits();
	wld.update();
	check_hits(subGrpA1Disabled);

	wld.enable(subGrpA1, true);
	wld.enable(subGrpC, false);
	CHECK_FALSE(wld.enabled(subGrpC));
	CHECK(wld.enabled(sys10));
	CHECK(wld.enabled(sys11));
	clear_hits();
	wld.update();
	check_hits(subGrpCDisabled);

	wld.enable(subGrpC, true);
	wld.enable(grpC, false);
	CHECK_FALSE(wld.enabled(grpC));
	CHECK(wld.enabled(subGrpC));
	CHECK(wld.enabled(sys10));
	CHECK(wld.enabled(sys11));
	clear_hits();
	wld.update();
	check_hits(grpCDisabled);

	wld.enable(grpC, true);
	wld.enable(sys10, false);
	CHECK_FALSE(wld.enabled(sys10));
	CHECK(wld.enabled(sys11));
	clear_hits();
	wld.update();
	check_hits(sys10Disabled);

	wld.enable(sys10, true);
	wld.enable(grpA, false);
	CHECK_FALSE(wld.enabled(grpA));
	CHECK(wld.enabled(subGrpA1));
	CHECK(wld.enabled(subGrpA2));
	CHECK(wld.enabled(sys1));
	CHECK(wld.enabled(sys4));
	clear_hits();
	wld.update();
	check_hits(grpADisabled);

	(void)sys1;
	(void)sys2;
	(void)sys3;
	(void)sys4;
	(void)sys5A;
	(void)sys5B;
	(void)sys6;
	(void)sys10;
	(void)sys11;
}

#endif

#if GAIA_OBSERVERS_ENABLED

TEST_CASE("Observer - simple") {
	TestWorld twld;
	uint32_t cnt = 0;
	bool isDel = false;

	const auto on_add = wld.observer() //
													.event(ecs::ObserverEvent::OnAdd)
													.all<Position>()
													.all<Acceleration>()
													.on_each([&cnt, &isDel]() {
														++cnt;
														isDel = false;
													})
													.entity();
	(void)on_add;
	const auto on_del = wld.observer() //
													.event(ecs::ObserverEvent::OnDel)
													.no<Position>()
													.no<Acceleration>()
													.on_each([&cnt, &isDel]() {
														++cnt;
														isDel = true;
													})
													.entity();
	(void)on_del;

	ecs::Entity e, e1, e2;

	// OnAdd
	{
		cnt = 0;
		e = wld.add();
		{
			// Observer will not trigger yet
			wld.add<Position>(e);
			CHECK(cnt == 0);
			CHECK_FALSE(isDel);
			// Both components were added, trigger the observer now.
			wld.add<Acceleration>(e);
			CHECK(cnt == 1);
			CHECK_FALSE(isDel);
		}

		{
			cnt = 0;
			e1 = wld.copy(e);
			CHECK(cnt == 0);
			CHECK_FALSE(isDel);
		}

		{
			cnt = 0;
			e2 = wld.copy_ext(e);
			CHECK(cnt == 1);
			CHECK_FALSE(isDel);
		}

		{
			cnt = 0;
			ecs::Entity e3 = wld.add();
			CHECK(cnt == 0);
			CHECK_FALSE(isDel);
			ecs::EntityBuilder builder = wld.build(e3);
			CHECK(cnt == 0);
			CHECK_FALSE(isDel);
			builder.add<Acceleration>().add<Position>();
			CHECK(cnt == 0);
			CHECK_FALSE(isDel);
			// Commit changes. The observer will triggers now.
			builder.commit();
			CHECK(cnt == 1);
			CHECK_FALSE(isDel);
		}
	}

	// OnAdd disabled
	{
		wld.enable(on_add, false);

		cnt = 0;
		e = wld.add();
		{
			// Observer will not trigger yet
			wld.add<Position>(e);
			CHECK(cnt == 0);
			CHECK_FALSE(isDel);
			// Both components were added, trigger the observer now.
			wld.add<Acceleration>(e);
			CHECK(cnt == 0);
			CHECK_FALSE(isDel);
		}

		{
			cnt = 0;
			e1 = wld.copy(e);
			CHECK(cnt == 0);
			CHECK_FALSE(isDel);
		}

		{
			cnt = 0;
			e2 = wld.copy_ext(e);
			CHECK(cnt == 0);
			CHECK_FALSE(isDel);
		}

		{
			cnt = 0;
			ecs::Entity e3 = wld.add();
			CHECK(cnt == 0);
			CHECK_FALSE(isDel);
			ecs::EntityBuilder builder = wld.build(e3);
			CHECK(cnt == 0);
			CHECK_FALSE(isDel);
			builder.add<Acceleration>().add<Position>();
			CHECK(cnt == 0);
			CHECK_FALSE(isDel);
			// Commit changes. The observer will triggers now.
			builder.commit();
			CHECK(cnt == 0);
			CHECK_FALSE(isDel);
		}

		wld.enable(on_add, true);
	}

	// OnDel
	{
		cnt = 0;
		{
			// Observer will not trigger yet
			wld.del<Position>(e);
			CHECK(cnt == 0);
			CHECK_FALSE(isDel);
			// Both components were added, trigger the observer now.
			wld.del<Acceleration>(e);
			CHECK(cnt == 1);
			CHECK(isDel);
		}

		{
			cnt = 0;
			isDel = false;
			ecs::EntityBuilder builder = wld.build(e1);
			CHECK(cnt == 0);
			CHECK_FALSE(isDel);
			builder.del<Acceleration>().del<Position>();
			CHECK(cnt == 0);
			CHECK_FALSE(isDel);
			// Commit changes. The observer will triggers now.
			builder.commit();
			CHECK(cnt == 1);
			CHECK(isDel);
		}
	}
}

TEST_CASE("EntityBuilder single-step graph rebuild tolerates stale del edges") {
	TestWorld twld;

	auto A = wld.add();
	auto B = wld.add();
	auto C = wld.add();

	auto e = wld.add();
	wld.add(e, A);
	wld.add(e, B);
	wld.add(e, C);
	wld.del(A);

	for (int i = 0; i < 2000; ++i) {
		auto x = wld.add();
		wld.add(x, B);
		wld.add(x, C);
		wld.del(x, B);
		wld.del(x);
		wld.update();
	}
}

TEST_CASE("EntityBuilder batching keeps later single-step archetype moves valid") {
	TestWorld twld;

	const auto tagA = wld.add();
	const auto tagB = wld.add();
	const auto eBatch = wld.add();

	{
		auto builder = wld.build(eBatch);
		builder.add(tagA).add(tagB);
		builder.commit();
	}

	CHECK(wld.has(eBatch, tagA));
	CHECK(wld.has(eBatch, tagB));

	const auto eSingle = wld.add();
	wld.add(eSingle, tagA);
	wld.add(eSingle, tagB);
	CHECK(wld.has(eSingle, tagA));
	CHECK(wld.has(eSingle, tagB));
}

TEST_CASE("Observer - copy_ext payload") {
	TestWorld twld;

	uint32_t hits = 0;
	uint32_t iterSize = 0;
	uint32_t entityViewSize = 0;
	ecs::Entity observedEntity = ecs::EntityBad;
	Position pos{};
	Acceleration acc{};

	const auto obs = wld.observer()
											 .event(ecs::ObserverEvent::OnAdd)
											 .all<Position>()
											 .all<Acceleration>()
											 .on_each([&](ecs::Iter& it) {
												 ++hits;
												 iterSize = it.size();
												 auto entityView = it.view<ecs::Entity>();
												 entityViewSize = (uint32_t)entityView.size();
												 observedEntity = entityView[0];
												 auto posView = it.view<Position>();
												 auto accView = it.view<Acceleration>();
												 pos = posView[0];
												 acc = accView[0];
											 })
											 .entity();
	(void)obs;

	const auto src = wld.add();
	wld.add<Position>(src, {11.0f, 22.0f, 33.0f});
	wld.add<Acceleration>(src, {44.0f, 55.0f, 66.0f});

	hits = 0;
	iterSize = 0;
	entityViewSize = 0;
	observedEntity = ecs::EntityBad;
	pos = {};
	acc = {};

	const auto dst = wld.copy_ext(src);

	CHECK(hits == 1);
	CHECK(iterSize == 1);
	CHECK(entityViewSize == 1);
	CHECK(observedEntity == dst);
	CHECK(pos.x == 11.0f);
	CHECK(pos.y == 22.0f);
	CHECK(pos.z == 33.0f);
	CHECK(acc.x == 44.0f);
	CHECK(acc.y == 55.0f);
	CHECK(acc.z == 66.0f);
}

TEST_CASE("Observer - add with value payload") {
	TestWorld twld;

	uint32_t hits = 0;
	ecs::Entity observedEntity = ecs::EntityBad;
	Position pos{};

	const auto obs = wld.observer()
											 .event(ecs::ObserverEvent::OnAdd)
											 .all<Position>()
											 .on_each([&](ecs::Iter& it) {
												 ++hits;
												 auto entityView = it.view<ecs::Entity>();
												 auto posView = it.view<Position>();
												 observedEntity = entityView[0];
												 pos = posView[0];
											 })
											 .entity();
	(void)obs;

	const auto e = wld.add();
	wld.add(e, wld.add<Position>().entity, Position{3.0f, 4.0f, 5.0f});

	CHECK(hits == 1);
	CHECK(observedEntity == e);
	CHECK(pos.x == doctest::Approx(3.0f));
	CHECK(pos.y == doctest::Approx(4.0f));
	CHECK(pos.z == doctest::Approx(5.0f));
}

TEST_CASE("Observer - add sparse with value payload") {
	TestWorld twld;

	uint32_t hits = 0;
	ecs::Entity observedEntity = ecs::EntityBad;
	PositionSparse pos{};

	const auto obs = wld.observer()
											 .event(ecs::ObserverEvent::OnAdd)
											 .all<PositionSparse>()
											 .on_each([&](ecs::Iter& it) {
												 ++hits;
												 auto entityView = it.view<ecs::Entity>();
												 auto posView = it.view<PositionSparse>();
												 observedEntity = entityView[0];
												 pos = posView[0];
											 })
											 .entity();
	(void)obs;

	const auto e = wld.add();
	wld.add(e, wld.add<PositionSparse>().entity, PositionSparse{6.0f, 7.0f, 8.0f});

	CHECK(hits == 1);
	CHECK(observedEntity == e);
	CHECK(pos.x == doctest::Approx(6.0f));
	CHECK(pos.y == doctest::Approx(7.0f));
	CHECK(pos.z == doctest::Approx(8.0f));
}

TEST_CASE("Observer - del sparse with value payload") {
	TestWorld twld;

	uint32_t hits = 0;
	ecs::Entity observedEntity = ecs::EntityBad;
	PositionSparse pos{};

	const auto obs = wld.observer()
											 .event(ecs::ObserverEvent::OnDel)
											 .all<PositionSparse>()
											 .on_each([&](ecs::Iter& it) {
												 ++hits;
												 auto entityView = it.view<ecs::Entity>();
												 auto posView = it.view<PositionSparse>();
												 observedEntity = entityView[0];
												 pos = posView[0];
											 })
											 .entity();
	(void)obs;

	const auto e = wld.add();
	wld.add<PositionSparse>(e, {6.0f, 7.0f, 8.0f});

	hits = 0;
	observedEntity = ecs::EntityBad;
	pos = {};

	wld.del<PositionSparse>(e);

	CHECK(hits == 1);
	CHECK(observedEntity == e);
	CHECK(pos.x == doctest::Approx(6.0f));
	CHECK(pos.y == doctest::Approx(7.0f));
	CHECK(pos.z == doctest::Approx(8.0f));
}

TEST_CASE("Observer - copy_ext sparse payload") {
	TestWorld twld;

	uint32_t hits = 0;
	uint32_t iterSize = 0;
	ecs::Entity observedEntity = ecs::EntityBad;
	PositionSparse pos{};

	const auto obs = wld.observer()
											 .event(ecs::ObserverEvent::OnAdd)
											 .all<PositionSparse>()
											 .on_each([&](ecs::Iter& it) {
												 ++hits;
												 iterSize = it.size();
												 auto entityView = it.view<ecs::Entity>();
												 observedEntity = entityView[0];
												 auto posView = it.view<PositionSparse>();
												 pos = posView[0];
											 })
											 .entity();
	(void)obs;

	const auto src = wld.add();
	wld.add<PositionSparse>(src, {7.0f, 8.0f, 9.0f});

	hits = 0;
	iterSize = 0;
	observedEntity = ecs::EntityBad;
	pos = {};

	const auto dst = wld.copy_ext(src);

	CHECK(hits == 1);
	CHECK(iterSize == 1);
	CHECK(observedEntity == dst);
	CHECK(pos.x == 7.0f);
	CHECK(pos.y == 8.0f);
	CHECK(pos.z == 9.0f);
}

TEST_CASE("Observer - copy_ext_n payload") {
	TestWorld twld;

	constexpr uint32_t N = 1536;
	const auto src = wld.add();
	wld.add<Position>(src, {1.0f, 2.0f, 3.0f});
	wld.add<Acceleration>(src, {4.0f, 5.0f, 6.0f});

	uint32_t hits = 0;
	uint32_t seen = 0;
	cnt::darr<ecs::Entity> observedEntities;
	observedEntities.reserve(N);
	Position pos{};
	Acceleration acc{};

	const auto obs = wld.observer()
											 .event(ecs::ObserverEvent::OnAdd)
											 .all<Position>()
											 .all<Acceleration>()
											 .on_each([&](ecs::Iter& it) {
												 ++hits;
												 seen += it.size();
												 auto entityView = it.view<ecs::Entity>();
												 auto posView = it.view<Position>();
												 auto accView = it.view<Acceleration>();
												 GAIA_EACH(it) {
													 observedEntities.push_back(entityView[i]);
													 pos = posView[i];
													 acc = accView[i];
													 CHECK(pos.x == 1.0f);
													 CHECK(pos.y == 2.0f);
													 CHECK(pos.z == 3.0f);
													 CHECK(acc.x == 4.0f);
													 CHECK(acc.y == 5.0f);
													 CHECK(acc.z == 6.0f);
												 }
											 })
											 .entity();
	(void)obs;

	uint32_t callbackSeen = 0;
	wld.copy_ext_n(src, N, [&](ecs::CopyIter& it) {
		callbackSeen += it.size();
	});

	CHECK(hits >= 1);
	CHECK(seen == N);
	CHECK(callbackSeen == N);
	CHECK(observedEntities.size() == N);
}

TEST_CASE("Observer - copy_ext_n sparse payload") {
	TestWorld twld;

	constexpr uint32_t N = 1536;
	const auto src = wld.add();
	wld.add<PositionSparse>(src, {7.0f, 8.0f, 9.0f});

	uint32_t hits = 0;
	uint32_t seen = 0;
	cnt::darr<ecs::Entity> observedEntities;
	observedEntities.reserve(N);

	const auto obs = wld.observer()
											 .event(ecs::ObserverEvent::OnAdd)
											 .all<PositionSparse>()
											 .on_each([&](ecs::Iter& it) {
												 ++hits;
												 seen += it.size();
												 auto entityView = it.view<ecs::Entity>();
												 auto posView = it.view<PositionSparse>();
												 GAIA_EACH(it) {
													 observedEntities.push_back(entityView[i]);
													 const auto pos = posView[i];
													 CHECK(pos.x == 7.0f);
													 CHECK(pos.y == 8.0f);
													 CHECK(pos.z == 9.0f);
												 }
											 })
											 .entity();
	(void)obs;

	uint32_t callbackSeen = 0;
	wld.copy_ext_n(src, N, [&](ecs::Entity entity) {
		++callbackSeen;
		CHECK(wld.has<PositionSparse>(entity));
		const auto& pos = wld.get<PositionSparse>(entity);
		CHECK(pos.x == 7.0f);
		CHECK(pos.y == 8.0f);
		CHECK(pos.z == 9.0f);
	});

	CHECK(hits >= 1);
	CHECK(seen == N);
	CHECK(callbackSeen == N);
	CHECK(observedEntities.size() == N);
}

TEST_CASE("Observer - copy_ext ignores unrelated traversed source observers") {
	TestWorld twld;

	const auto connectedToA = wld.add();
	const auto connectedToB = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.child(child, root);
	wld.add<Acceleration>(root);

	const auto src = wld.add();
	wld.add<Position>(src);
	wld.add(src, ecs::Pair(connectedToA, child));

	int hitsA = 0;
	int hitsB = 0;

	(void)wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.template all<Position>()
			.all(ecs::Pair(connectedToA, ecs::Var0))
			.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
			.on_each([&](ecs::Iter&) {
				++hitsA;
			})
			.entity();

	(void)wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.template all<Position>()
			.all(ecs::Pair(connectedToB, ecs::Var0))
			.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
			.on_each([&](ecs::Iter&) {
				++hitsB;
			})
			.entity();

	(void)wld.copy_ext(src);
	CHECK(hitsA == 1);
	CHECK(hitsB == 0);
}

TEST_CASE("copy_ext_n strips names and preserves sparse data") {
	TestWorld twld;

	const auto src = wld.add();
	wld.name(src, "copy-ext-n-source");
	wld.add<PositionSparse>(src, {3.0f, 4.0f, 5.0f});

	cnt::darr<ecs::Entity> copied;
	wld.copy_ext_n(src, 8, [&](ecs::Entity entity) {
		copied.push_back(entity);
	});

	CHECK(copied.size() == 8);
	CHECK(wld.get("copy-ext-n-source") == src);
	CHECK(wld.name(src) == "copy-ext-n-source");

	for (const auto entity: copied) {
		CHECK(wld.name(entity).empty());
		CHECK(wld.has<PositionSparse>(entity));
		const auto& pos = wld.get<PositionSparse>(entity);
		CHECK(pos.x == doctest::Approx(3.0f));
		CHECK(pos.y == doctest::Approx(4.0f));
		CHECK(pos.z == doctest::Approx(5.0f));
	}
}

TEST_CASE("Observer - fast path") {
	TestWorld twld;

	const auto relation = wld.add();
	const auto target = wld.add();
	const auto fixedPair = (ecs::Entity)ecs::Pair(relation, target);

	const auto baseEntity = wld.add();
	const auto inheritingEntity = wld.add();
	wld.as(inheritingEntity, baseEntity);
	const auto isPair = (ecs::Entity)ecs::Pair(ecs::Is, baseEntity);
	const auto wildcardPair = (ecs::Entity)ecs::Pair(relation, ecs::All);

	const auto observerAllPos =
			wld.observer().event(ecs::ObserverEvent::OnAdd).all<Position>().on_each([](ecs::Iter&) {}).entity();
	const auto observerNoPos =
			wld.observer().event(ecs::ObserverEvent::OnDel).no<Position>().on_each([](ecs::Iter&) {}).entity();
	const auto observerPairFixed =
			wld.observer().event(ecs::ObserverEvent::OnAdd).all(fixedPair).on_each([](ecs::Iter&) {}).entity();
	const auto observerPairIs =
			wld.observer().event(ecs::ObserverEvent::OnAdd).all(isPair).on_each([](ecs::Iter&) {}).entity();
	const auto observerPairWildcard =
			wld.observer().event(ecs::ObserverEvent::OnAdd).all(wildcardPair).on_each([](ecs::Iter&) {}).entity();
	ecs::QueryTermOptions readOpts;
	readOpts.read();
	const auto observerAllPosRead =
			wld.observer().event(ecs::ObserverEvent::OnAdd).all<Position>(readOpts).on_each([](ecs::Iter&) {}).entity();
	ecs::QueryTermOptions writeOpts;
	writeOpts.write();
	const auto observerAllPosWrite =
			wld.observer().event(ecs::ObserverEvent::OnAdd).all<Position>(writeOpts).on_each([](ecs::Iter&) {}).entity();
	ecs::QueryTermOptions travKindOnlyOpts;
	travKindOnlyOpts.trav_kind(ecs::QueryTravKind::Down);
	const auto observerAllPosTravKindOnly = wld.observer()
																							.event(ecs::ObserverEvent::OnAdd)
																							.all<Position>(travKindOnlyOpts)
																							.on_each([](ecs::Iter&) {})
																							.entity();
	ecs::QueryTermOptions travDepthOnlyOpts;
	travDepthOnlyOpts.trav_depth(2);
	const auto observerAllPosTravDepthOnly = wld.observer()
																							 .event(ecs::ObserverEvent::OnAdd)
																							 .all<Position>(travDepthOnlyOpts)
																							 .on_each([](ecs::Iter&) {})
																							 .entity();
	ecs::QueryTermOptions srcOpts;
	srcOpts.src(wld.add());
	const auto observerAllPosSrc =
			wld.observer().event(ecs::ObserverEvent::OnAdd).all<Position>(srcOpts).on_each([](ecs::Iter&) {}).entity();
	ecs::QueryTermOptions travOpts;
	travOpts.trav(ecs::ChildOf);
	const auto observerAllPosTrav =
			wld.observer().event(ecs::ObserverEvent::OnAdd).all<Position>(travOpts).on_each([](ecs::Iter&) {}).entity();

	const auto& dataAllPos = wld.observers().data(observerAllPos);
	const auto& dataNoPos = wld.observers().data(observerNoPos);
	const auto& dataPairFixed = wld.observers().data(observerPairFixed);
	const auto& dataPairIs = wld.observers().data(observerPairIs);
	const auto& dataPairWildcard = wld.observers().data(observerPairWildcard);
	const auto& dataAllPosRead = wld.observers().data(observerAllPosRead);
	const auto& dataAllPosWrite = wld.observers().data(observerAllPosWrite);
	const auto& dataAllPosTravKindOnly = wld.observers().data(observerAllPosTravKindOnly);
	const auto& dataAllPosTravDepthOnly = wld.observers().data(observerAllPosTravDepthOnly);
	const auto& dataAllPosSrc = wld.observers().data(observerAllPosSrc);
	const auto& dataAllPosTrav = wld.observers().data(observerAllPosTrav);

	CHECK(dataAllPos.fastPath == ecs::ObserverRuntimeData::MatchFastPath::SinglePositiveTerm);
	CHECK(dataNoPos.fastPath == ecs::ObserverRuntimeData::MatchFastPath::SingleNegativeTerm);
	CHECK(dataPairFixed.fastPath == ecs::ObserverRuntimeData::MatchFastPath::SinglePositiveTerm);
	CHECK(dataPairIs.fastPath == ecs::ObserverRuntimeData::MatchFastPath::Disabled);
	CHECK(dataPairWildcard.fastPath == ecs::ObserverRuntimeData::MatchFastPath::Disabled);
	CHECK(dataAllPosRead.fastPath == ecs::ObserverRuntimeData::MatchFastPath::SinglePositiveTerm);
	CHECK(dataAllPosWrite.fastPath == ecs::ObserverRuntimeData::MatchFastPath::SinglePositiveTerm);
	CHECK(dataAllPosTravKindOnly.fastPath == ecs::ObserverRuntimeData::MatchFastPath::SinglePositiveTerm);
	CHECK(dataAllPosTravDepthOnly.fastPath == ecs::ObserverRuntimeData::MatchFastPath::SinglePositiveTerm);
	CHECK(dataAllPosSrc.fastPath == ecs::ObserverRuntimeData::MatchFastPath::Disabled);
	CHECK(dataAllPosTrav.fastPath == ecs::ObserverRuntimeData::MatchFastPath::Disabled);

	int pairHits = 0;
	const auto observerPairRuntime = wld.observer()
																			 .event(ecs::ObserverEvent::OnAdd)
																			 .all(fixedPair)
																			 .on_each([&pairHits](ecs::Iter&) {
																				 ++pairHits;
																			 })
																			 .entity();
	const auto e = wld.add();
	wld.add(e, ecs::Pair(relation, target));
	CHECK(pairHits == 1);
	(void)observerPairRuntime;
}

TEST_CASE("Observer - traversed source propagation on ancestor term changes") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.child(child, root);

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, child));

	int addHits = 0;
	int delHits = 0;
	cnt::darr<ecs::Entity> added;
	cnt::darr<ecs::Entity> removed;

	const auto makeObserver = [&](ecs::ObserverEvent event, int& hits, cnt::darr<ecs::Entity>& out) {
		return wld.observer()
				.event(event)
				.template all<Position>()
				.all(ecs::Pair(connectedTo, ecs::Var0))
				.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
				.on_each([&](ecs::Iter& it) {
					auto entities = it.view<ecs::Entity>();
					GAIA_EACH(it) {
						++hits;
						out.push_back(entities[i]);
					}
				})
				.entity();
	};

	(void)makeObserver(ecs::ObserverEvent::OnAdd, addHits, added);
	(void)makeObserver(ecs::ObserverEvent::OnDel, delHits, removed);

	wld.add<Acceleration>(root);
	CHECK(addHits == 1);
	CHECK(added.size() == 1);
	CHECK(added[0] == cable);

	wld.del<Acceleration>(root);
	CHECK(delHits == 1);
	CHECK(removed.size() == 1);
	CHECK(removed[0] == cable);
}

TEST_CASE("Observer - traversed source propagation on source binding pair changes") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.child(child, root);
	wld.add<Acceleration>(root);

	const auto cable = wld.add();
	wld.add<Position>(cable);

	int addHits = 0;
	int delHits = 0;
	cnt::darr<ecs::Entity> added;
	cnt::darr<ecs::Entity> removed;

	const auto makeObserver = [&](ecs::ObserverEvent event, int& hits, cnt::darr<ecs::Entity>& out) {
		return wld.observer()
				.event(event)
				.template all<Position>()
				.all(ecs::Pair(connectedTo, ecs::Var0))
				.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
				.on_each([&](ecs::Iter& it) {
					auto entities = it.view<ecs::Entity>();
					GAIA_EACH(it) {
						++hits;
						out.push_back(entities[i]);
					}
				})
				.entity();
	};

	(void)makeObserver(ecs::ObserverEvent::OnAdd, addHits, added);
	(void)makeObserver(ecs::ObserverEvent::OnDel, delHits, removed);

	wld.add(cable, ecs::Pair(connectedTo, child));
	CHECK(addHits == 1);
	CHECK(added.size() == 1);
	CHECK(added[0] == cable);

	wld.del(cable, ecs::Pair(connectedTo, child));
	CHECK(delHits == 1);
	CHECK(removed.size() == 1);
	CHECK(removed[0] == cable);
}

TEST_CASE("Observer - traversed source local pair change only diffs touched entity") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.child(child, root);
	wld.add<Acceleration>(root);

	const auto cableExisting = wld.add();
	wld.add<Position>(cableExisting);
	wld.add(cableExisting, ecs::Pair(connectedTo, child));

	const auto cableAdded = wld.add();
	wld.add<Position>(cableAdded);

	int addHits = 0;
	cnt::darr<ecs::Entity> added;

	(void)wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.template all<Position>()
			.all(ecs::Pair(connectedTo, ecs::Var0))
			.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
			.on_each([&](ecs::Iter& it) {
				auto entities = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					++addHits;
					added.push_back(entities[i]);
				}
			})
			.entity();

	wld.add(cableAdded, ecs::Pair(connectedTo, child));
	CHECK(addHits == 1);
	CHECK(added.size() == 1);
	CHECK(added[0] == cableAdded);

	int delHits = 0;
	cnt::darr<ecs::Entity> removed;
	(void)wld.observer()
			.event(ecs::ObserverEvent::OnDel)
			.template all<Position>()
			.all(ecs::Pair(connectedTo, ecs::Var0))
			.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
			.on_each([&](ecs::Iter& it) {
				auto entities = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					++delHits;
					removed.push_back(entities[i]);
				}
			})
			.entity();

	wld.del(cableAdded, ecs::Pair(connectedTo, child));
	CHECK(delHits == 1);
	CHECK(removed.size() == 1);
	CHECK(removed[0] == cableAdded);
}

TEST_CASE("Observer - traversed source propagation on relation edge changes") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.add<Acceleration>(root);

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, child));
	auto q = wld.query()
							 .template all<Position>()
							 .all(ecs::Pair(connectedTo, ecs::Var0))
							 .template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav());
	CHECK(q.count() == 0);

	int addHits = 0;
	int delHits = 0;
	cnt::darr<ecs::Entity> added;
	cnt::darr<ecs::Entity> removed;

	const auto makeObserver = [&](ecs::ObserverEvent event, int& hits, cnt::darr<ecs::Entity>& out) {
		return wld.observer()
				.event(event)
				.template all<Position>()
				.all(ecs::Pair(connectedTo, ecs::Var0))
				.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
				.on_each([&](ecs::Iter& it) {
					auto entities = it.view<ecs::Entity>();
					GAIA_EACH(it) {
						++hits;
						out.push_back(entities[i]);
					}
				})
				.entity();
	};
	const auto buildQuery = [&] {
		return wld.query<false>()
				.template all<Position>()
				.all(ecs::Pair(connectedTo, ecs::Var0))
				.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav());
	};

	(void)makeObserver(ecs::ObserverEvent::OnAdd, addHits, added);
	(void)makeObserver(ecs::ObserverEvent::OnDel, delHits, removed);

	wld.child(child, root);
	CHECK(wld.target(child, ecs::ChildOf) == root);
	CHECK(buildQuery().count() == 1);
	CHECK(addHits == 1);
	CHECK(added.size() == 1);
	if (!added.empty())
		CHECK(added[0] == cable);

	wld.del(child, ecs::Pair(ecs::ChildOf, root));
	CHECK(wld.target(child, ecs::ChildOf) == ecs::EntityBad);
	CHECK(buildQuery().count() == 0);
	CHECK(delHits == 1);
	CHECK(removed.size() == 1);
	if (!removed.empty())
		CHECK(removed[0] == cable);
}

TEST_CASE("Observer - traversed source propagation ignores unrelated pair relation changes") {
	TestWorld twld;

	const auto connectedToA = wld.add();
	const auto connectedToB = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.child(child, root);
	wld.add<Acceleration>(root);

	const auto cable = wld.add();
	wld.add<Position>(cable);

	int hitsA = 0;
	int hitsB = 0;

	(void)wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.template all<Position>()
			.all(ecs::Pair(connectedToA, ecs::Var0))
			.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
			.on_each([&](ecs::Iter&) {
				++hitsA;
			})
			.entity();

	(void)wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.template all<Position>()
			.all(ecs::Pair(connectedToB, ecs::Var0))
			.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
			.on_each([&](ecs::Iter&) {
				++hitsB;
			})
			.entity();

	wld.add(cable, ecs::Pair(connectedToA, child));
	CHECK(hitsA == 1);
	CHECK(hitsB == 0);
}

TEST_CASE("Observer - Is pair uses semantic inheritance matching") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto mammal = wld.add();
	const auto wolf = wld.add();

	int hits = 0;
	cnt::darr<ecs::Entity> observed;
	const auto observerIs = wld.observer()
															.event(ecs::ObserverEvent::OnAdd)
															.all(ecs::Pair(ecs::Is, animal))
															.on_each([&](ecs::Iter& it) {
																auto entities = it.view<ecs::Entity>();
																GAIA_EACH(it) {
																	++hits;
																	observed.push_back(entities[i]);
																}
															})
															.entity();
	(void)observerIs;

	wld.as(mammal, animal);
	CHECK(hits == 1);
	CHECK(observed.size() == 1);
	if (!observed.empty())
		CHECK(observed[0] == mammal);

	wld.as(wolf, mammal);
	auto& observerIsData = wld.observers().data(observerIs);
	auto& observerQueryInfo = observerIsData.query.fetch();
	const auto& wolfEc = wld.fetch(wolf);
	CHECK(observerIsData.query.matches_any(observerQueryInfo, *wolfEc.pArchetype, ecs::EntitySpan{&wolf, 1}));
	CHECK(hits == 2);
	CHECK(observed.size() == 2);
	if (observed.size() >= 2)
		CHECK(observed[1] == wolf);
}

TEST_CASE("Observer - direct Is pair matches only direct stored edges") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto mammal = wld.add();
	const auto wolf = wld.add();

	ecs::QueryTermOptions directOpts;
	directOpts.direct();

	int hits = 0;
	cnt::darr<ecs::Entity> observed;
	const auto observerIs = wld.observer()
															.event(ecs::ObserverEvent::OnAdd)
															.all(ecs::Pair(ecs::Is, animal), directOpts)
															.on_each([&](ecs::Iter& it) {
																auto entities = it.view<ecs::Entity>();
																GAIA_EACH(it) {
																	++hits;
																	observed.push_back(entities[i]);
																}
															})
															.entity();
	(void)observerIs;

	wld.as(mammal, animal);
	CHECK(hits == 1);
	CHECK(observed.size() == 1);
	if (!observed.empty())
		CHECK(observed[0] == mammal);

	wld.as(wolf, mammal);
	CHECK(hits == 1);
	CHECK(observed.size() == 1);
}

TEST_CASE("Observer - direct Is pair via QueryInput matches only direct stored edges") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto mammal = wld.add();
	const auto wolf = wld.add();

	ecs::QueryInput item{};
	item.op = ecs::QueryOpKind::All;
	item.access = ecs::QueryAccess::None;
	item.id = ecs::Pair(ecs::Is, animal);
	item.matchKind = ecs::QueryMatchKind::Direct;

	int hits = 0;
	cnt::darr<ecs::Entity> observed;
	const auto observerIs = wld.observer()
															.event(ecs::ObserverEvent::OnAdd)
															.add(item)
															.on_each([&](ecs::Iter& it) {
																auto entities = it.view<ecs::Entity>();
																GAIA_EACH(it) {
																	++hits;
																	observed.push_back(entities[i]);
																}
															})
															.entity();
	(void)observerIs;

	wld.as(mammal, animal);
	CHECK(hits == 1);
	CHECK(observed.size() == 1);
	if (!observed.empty())
		CHECK(observed[0] == mammal);

	wld.as(wolf, mammal);
	CHECK(hits == 1);
	CHECK(observed.size() == 1);
}

TEST_CASE("Observer - is sugar matches semantic and direct Is terms") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto mammal = wld.add();
	const auto wolf = wld.add();

	int semanticHits = 0;
	const auto obsSemantic = wld.observer()
															 .event(ecs::ObserverEvent::OnAdd)
															 .is(animal)
															 .on_each([&](ecs::Iter&) {
																 ++semanticHits;
															 })
															 .entity();

	int directHits = 0;
	const auto obsDirect = wld.observer()
														 .event(ecs::ObserverEvent::OnAdd)
														 .is(animal, ecs::QueryTermOptions{}.direct())
														 .on_each([&](ecs::Iter&) {
															 ++directHits;
														 })
														 .entity();

	wld.as(mammal, animal);
	CHECK(semanticHits == 1);
	CHECK(directHits == 1);

	wld.as(wolf, mammal);
	CHECK(semanticHits == 2);
	CHECK(directHits == 1);

	(void)obsSemantic;
	(void)obsDirect;
}

TEST_CASE("Observer - in sugar matches descendants but excludes the base entity") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto mammal = wld.add();
	const auto wolf = wld.add();

	int hits = 0;
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnAdd)
														.in(animal)
														.on_each([&](ecs::Iter&) {
															++hits;
														})
														.entity();

	wld.as(mammal, animal);
	CHECK(hits == 1);

	wld.as(wolf, mammal);
	CHECK(hits == 2);

	(void)observer;
}

TEST_CASE("Observer - prefabs are excluded by default and can be matched explicitly") {
	TestWorld twld;

	uint32_t defaultHits = 0;
	const auto obsDefault = wld.observer()
															.event(ecs::ObserverEvent::OnAdd)
															.all<Position>()
															.on_each([&](ecs::Iter&) {
																++defaultHits;
															})
															.entity();

	uint32_t prefabHits = 0;
	const auto obsMatchPrefab = wld.observer()
																	.event(ecs::ObserverEvent::OnAdd)
																	.all<Position>()
																	.match_prefab()
																	.on_each([&](ecs::Iter&) {
																		++prefabHits;
																	})
																	.entity();

	const auto prefab = wld.prefab();
	const auto entity = wld.add();

	wld.add<Position>(prefab, {1, 0, 0});
	wld.add<Position>(entity, {2, 0, 0});

	CHECK(defaultHits == 1);
	CHECK(prefabHits == 2);

	(void)obsDefault;
	(void)obsMatchPrefab;
}

TEST_CASE("Observer - inherited prefab data matches on instantiate") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(prefab, {4, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	uint32_t hits = 0;
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnAdd)
														.all<Position>()
														.on_each([&](ecs::Iter& it) {
															hits += it.size();
														})
														.entity();

	const auto instance = wld.instantiate(prefab);

	CHECK(hits == 1);
	CHECK(wld.has(instance, position));
	CHECK_FALSE(wld.has_direct(instance, position));

	(void)observer;
}

TEST_CASE("Observer - inherited Is data matches when derived entity is linked to a base") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto rabbit = wld.add();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(animal, {4, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	uint32_t hits = 0;
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnAdd)
														.all<Position>()
														.on_each([&](ecs::Iter& it) {
															hits += it.size();
														})
														.entity();

	wld.as(rabbit, animal);

	CHECK(hits == 1);
	CHECK(wld.has(rabbit, position));
	CHECK_FALSE(wld.has_direct(rabbit, position));

	(void)observer;
}

TEST_CASE("Observer - inherited Is override refires OnAdd when local ownership is materialized") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto rabbit = wld.add();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(animal, {4, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
	wld.as(rabbit, animal);

	uint32_t hits = 0;
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnAdd)
														.all<Position>()
														.on_each([&](ecs::Iter& it) {
															hits += it.size();
														})
														.entity();

	CHECK(wld.override<Position>(rabbit));
	CHECK(hits == 1);

	(void)observer;
}

TEST_CASE("Observer - inherited prefab data matches on instantiate_n") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(prefab, {4, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	uint32_t hits = 0;
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnAdd)
														.all<Position>()
														.on_each([&](ecs::Iter& it) {
															hits += it.size();
														})
														.entity();

	cnt::darray<ecs::Entity> instances;
	wld.instantiate_n(prefab, 5, [&](ecs::Entity instance) {
		instances.push_back(instance);
	});

	CHECK(hits == 5);
	CHECK(instances.size() == 5);
	for (const auto instance: instances) {
		CHECK(wld.has(instance, position));
		CHECK_FALSE(wld.has_direct(instance, position));
	}

	(void)observer;
}

TEST_CASE("Observer - prefab sync copied data matches existing instances") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto instance = wld.instantiate(prefab);

	uint32_t hits = 0;
	ecs::Entity observed = ecs::EntityBad;
	Position pos{};

	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnAdd)
														.all<Position>()
														.on_each([&](ecs::Iter& it) {
															++hits;
															auto entityView = it.view<ecs::Entity>();
															auto posView = it.view<Position>();
															observed = entityView[0];
															pos = posView[0];
														})
														.entity();

	auto prefabBuilder = wld.build(prefab);
	prefabBuilder.add<Position>();
	prefabBuilder.commit();
	wld.set<Position>(prefab) = {7.0f, 8.0f, 9.0f};

	hits = 0;
	observed = ecs::EntityBad;
	pos = {};

	CHECK(wld.sync(prefab) == 1);
	CHECK(hits == 1);
	CHECK(observed == instance);
	CHECK(pos.x == doctest::Approx(7.0f));
	CHECK(pos.y == doctest::Approx(8.0f));
	CHECK(pos.z == doctest::Approx(9.0f));

	(void)observer;
}

TEST_CASE("Observer - prefab sync sparse copied data matches existing instances") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto instance = wld.instantiate(prefab);

	uint32_t hits = 0;
	ecs::Entity observed = ecs::EntityBad;
	PositionSparse pos{};

	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnAdd)
														.all<PositionSparse>()
														.on_each([&](ecs::Iter& it) {
															++hits;
															auto entityView = it.view<ecs::Entity>();
															auto posView = it.view<PositionSparse>();
															observed = entityView[0];
															pos = posView[0];
														})
														.entity();

	wld.add<PositionSparse>(prefab, {2.0f, 3.0f, 4.0f});

	hits = 0;
	observed = ecs::EntityBad;
	pos = {};

	CHECK(wld.sync(prefab) == 1);
	CHECK(hits == 1);
	CHECK(observed == instance);
	CHECK(pos.x == doctest::Approx(2.0f));
	CHECK(pos.y == doctest::Approx(3.0f));
	CHECK(pos.z == doctest::Approx(4.0f));

	(void)observer;
}

TEST_CASE("Observer - inherited prefab data matches on delete from prefab") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
	wld.add<Position>(prefab, {4.0f, 5.0f, 6.0f});
	const auto instance = wld.instantiate(prefab);

	uint32_t hits = 0;
	ecs::Entity observed = ecs::EntityBad;
	Position pos{};

	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnDel)
														.all<Position>()
														.on_each([&](ecs::Iter& it) {
															++hits;
															auto entityView = it.view<ecs::Entity>();
															auto posView = it.view<Position>(0);
															observed = entityView[0];
															pos = posView[0];
														})
														.entity();

	wld.del<Position>(prefab);

	CHECK(hits == 1);
	CHECK(observed == instance);
	CHECK(pos.x == doctest::Approx(4.0f));
	CHECK(pos.y == doctest::Approx(5.0f));
	CHECK(pos.z == doctest::Approx(6.0f));
	CHECK_FALSE(wld.has<Position>(instance));
	CHECK(wld.sync(prefab) == 0);
	CHECK(hits == 1);

	(void)observer;
}

TEST_CASE("Observer - inherited sparse prefab data matches on delete from prefab") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto positionSparse = wld.add<PositionSparse>().entity;
	wld.add(positionSparse, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
	wld.add<PositionSparse>(prefab, {2.0f, 3.0f, 4.0f});
	const auto instance = wld.instantiate(prefab);

	uint32_t hits = 0;
	ecs::Entity observed = ecs::EntityBad;
	PositionSparse pos{};

	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnDel)
														.all<PositionSparse>()
														.on_each([&](ecs::Iter& it) {
															++hits;
															auto entityView = it.view<ecs::Entity>();
															auto posView = it.view<PositionSparse>(0);
															observed = entityView[0];
															pos = posView[0];
														})
														.entity();

	wld.del<PositionSparse>(prefab);

	CHECK(hits == 1);
	CHECK(observed == instance);
	CHECK(pos.x == doctest::Approx(2.0f));
	CHECK(pos.y == doctest::Approx(3.0f));
	CHECK(pos.z == doctest::Approx(4.0f));
	CHECK_FALSE(wld.has<PositionSparse>(instance));
	CHECK(wld.sync(prefab) == 0);
	CHECK(hits == 1);

	(void)observer;
}

TEST_CASE("Observer - inherited prefab removal does not fire when another source still provides the term") {
	TestWorld twld;

	const auto root = wld.prefab();
	const auto branchA = wld.prefab();
	const auto branchB = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	wld.as(branchA, root);
	wld.as(branchB, root);
	wld.add<Position>(branchA, {1.0f, 2.0f, 3.0f});
	wld.add<Position>(branchB, {4.0f, 5.0f, 6.0f});

	const auto instance = wld.instantiate(branchA);
	wld.as(instance, branchB);

	uint32_t hits = 0;
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnDel)
														.all<Position>()
														.on_each([&](ecs::Iter&) {
															++hits;
														})
														.entity();

	wld.del<Position>(branchA);

	CHECK(hits == 0);
	CHECK(wld.has<Position>(instance));
	const auto& pos = wld.get<Position>(instance);
	CHECK(pos.x == doctest::Approx(4.0f));
	CHECK(pos.y == doctest::Approx(5.0f));
	CHECK(pos.z == doctest::Approx(6.0f));

	(void)observer;
}

TEST_CASE("Observer - inherited prefab removal triggers no<T> OnDel") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
	wld.add<Position>(prefab, {9.0f, 8.0f, 7.0f});
	const auto instance = wld.instantiate(prefab);

	uint32_t hits = 0;
	ecs::Entity observed = ecs::EntityBad;

	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnDel)
														.no<Position>()
														.on_each([&](ecs::Iter& it) {
															++hits;
															auto entityView = it.view<ecs::Entity>();
															observed = entityView[0];
														})
														.entity();

	wld.del<Position>(prefab);

	CHECK(hits == 1);
	CHECK(observed == instance);
	CHECK_FALSE(wld.has<Position>(instance));

	(void)observer;
}

TEST_CASE("Observer - inherited prefab removal typed OnDel callback sees payload") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
	wld.add<Position>(prefab, {11.0f, 12.0f, 13.0f});
	const auto instance = wld.instantiate(prefab);

	uint32_t hits = 0;
	Position observedPos{};
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnDel)
														.all<const Position>()
														.on_each([&](const Position& pos) {
															++hits;
															observedPos = pos;
														})
														.entity();

	wld.del<Position>(prefab);

	CHECK(hits == 1);
	CHECK(observedPos.x == doctest::Approx(11.0f));
	CHECK(observedPos.y == doctest::Approx(12.0f));
	CHECK(observedPos.z == doctest::Approx(13.0f));
	CHECK_FALSE(wld.has<Position>(instance));

	(void)observer;
}

TEST_CASE("Observer - prefab sync spawned child matches Parent pair") {
	TestWorld twld;

	const auto rootPrefab = wld.prefab();
	const auto rootInstance = wld.instantiate(rootPrefab);
	const auto childPrefab = wld.prefab();

	uint32_t hits = 0;
	ecs::Entity observedChild = ecs::EntityBad;

	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnAdd)
														.all(ecs::Pair(ecs::Parent, rootInstance))
														.on_each([&](ecs::Iter& it) {
															++hits;
															auto entityView = it.view<ecs::Entity>();
															observedChild = entityView[0];
														})
														.entity();

	auto childBuilder = wld.build(childPrefab);
	childBuilder.add(ecs::Pair(ecs::Parent, rootPrefab));
	childBuilder.commit();

	hits = 0;
	observedChild = ecs::EntityBad;

	CHECK(wld.sync(rootPrefab) == 1);
	CHECK(hits == 1);
	CHECK(observedChild != ecs::EntityBad);
	CHECK(wld.has_direct(observedChild, ecs::Pair(ecs::Parent, rootInstance)));
	CHECK(wld.has_direct(observedChild, ecs::Pair(ecs::Is, childPrefab)));

	(void)observer;
}

TEST_CASE("Observer - prefab sync child removal stays non-destructive and emits no OnDel") {
	TestWorld twld;

	const auto rootPrefab = wld.prefab();
	const auto childPrefab = wld.prefab();
	wld.parent(childPrefab, rootPrefab);
	const auto rootInstance = wld.instantiate(rootPrefab);

	ecs::Entity childInstance = ecs::EntityBad;
	wld.sources(ecs::Parent, rootInstance, [&](ecs::Entity child) {
		if (wld.has_direct(child, ecs::Pair(ecs::Is, childPrefab)))
			childInstance = child;
	});

	CHECK(childInstance != ecs::EntityBad);
	if (childInstance == ecs::EntityBad)
		return;

	uint32_t hits = 0;
	ecs::Entity observed = ecs::EntityBad;
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnDel)
														.all(ecs::Pair(ecs::Parent, rootInstance))
														.on_each([&](ecs::Iter& it) {
															++hits;
															auto entityView = it.view<ecs::Entity>();
															observed = entityView[0];
														})
														.entity();

	wld.del(childPrefab, ecs::Pair(ecs::Parent, rootPrefab));
	CHECK(wld.sync(rootPrefab) == 0);

	CHECK(hits == 0);
	CHECK(observed == ecs::EntityBad);
	CHECK(wld.has_direct(childInstance, ecs::Pair(ecs::Parent, rootInstance)));

	(void)observer;
}

TEST_CASE("Observer - del runtime sparse id payload") {
	TestWorld twld;

	const auto sparseId = wld.add<PositionSparse>().entity;

	uint32_t hits = 0;
	ecs::Entity observedEntity = ecs::EntityBad;
	PositionSparse pos{};

	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnDel)
														.all(sparseId)
														.on_each([&](ecs::Iter& it) {
															++hits;
															auto entityView = it.view<ecs::Entity>();
															auto posView = it.view<PositionSparse>(0);
															observedEntity = entityView[0];
															pos = posView[0];
														})
														.entity();

	const auto e = wld.add();
	wld.add<PositionSparse>(e, {21.0f, 22.0f, 23.0f});

	hits = 0;
	observedEntity = ecs::EntityBad;
	pos = {};

	wld.del(e, sparseId);

	CHECK(hits == 1);
	CHECK(observedEntity == e);
	CHECK(pos.x == doctest::Approx(21.0f));
	CHECK(pos.y == doctest::Approx(22.0f));
	CHECK(pos.z == doctest::Approx(23.0f));

	(void)observer;
}

TEST_CASE("Observer - prefab sparse override data matches on instantiate") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	wld.add<PositionSparse>(prefab, {4, 0, 0});

	uint32_t hits = 0;
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnAdd)
														.all<PositionSparse>()
														.on_each([&](ecs::Iter& it) {
															hits += it.size();
														})
														.entity();

	const auto instance = wld.instantiate(prefab);

	CHECK(hits == 1);
	CHECK(wld.has<PositionSparse>(instance));
	(void)observer;
}

TEST_CASE("Observer - prefab sparse override data matches on instantiate_n") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	wld.add<PositionSparse>(prefab, {4, 0, 0});

	uint32_t hits = 0;
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnAdd)
														.all<PositionSparse>()
														.on_each([&](ecs::Iter& it) {
															hits += it.size();
														})
														.entity();

	cnt::darray<ecs::Entity> instances;
	wld.instantiate_n(prefab, 5, [&](ecs::Entity instance) {
		instances.push_back(instance);
	});

	CHECK(hits == 5);
	CHECK(instances.size() == 5);
	for (const auto instance: instances)
		CHECK(wld.has<PositionSparse>(instance));

	(void)observer;
}

TEST_CASE("Observer - add(QueryInput) registration and fast path") {
	TestWorld twld;

	const auto positionTerm = wld.add<Position>().entity;

	ecs::QueryInput simpleItem{};
	simpleItem.op = ecs::QueryOpKind::All;
	simpleItem.access = ecs::QueryAccess::Read;
	simpleItem.id = positionTerm;

	uint32_t hits = 0;
	const auto observerSimple = wld.observer()
																	.event(ecs::ObserverEvent::OnAdd)
																	.add(simpleItem)
																	.on_each([&hits](ecs::Iter&) {
																		++hits;
																	})
																	.entity();

	const auto& dataSimple = wld.observers().data(observerSimple);
	CHECK(dataSimple.fastPath == ecs::ObserverRuntimeData::MatchFastPath::SinglePositiveTerm);

	const auto e = wld.add();
	wld.add<Position>(e, {});
	CHECK(hits == 1);

	ecs::QueryInput srcItem{};
	srcItem.op = ecs::QueryOpKind::All;
	srcItem.access = ecs::QueryAccess::Read;
	srcItem.id = positionTerm;
	srcItem.entSrc = wld.add();

	const auto observerSrc =
			wld.observer().event(ecs::ObserverEvent::OnAdd).add(srcItem).on_each([](ecs::Iter&) {}).entity();

	const auto& dataSrc = wld.observers().data(observerSrc);
	CHECK(dataSrc.fastPath == ecs::ObserverRuntimeData::MatchFastPath::Disabled);
}

TEST_CASE("Observer - single negative fast path runtime") {
	TestWorld twld;

	uint32_t onAddHits = 0;
	uint32_t onDelHits = 0;

	const auto observerOnAddNoPos = wld.observer()
																			.event(ecs::ObserverEvent::OnAdd)
																			.no<Position>()
																			.on_each([&onAddHits](ecs::Iter&) {
																				++onAddHits;
																			})
																			.entity();
	const auto observerOnDelNoPos = wld.observer()
																			.event(ecs::ObserverEvent::OnDel)
																			.no<Position>()
																			.on_each([&onDelHits](ecs::Iter&) {
																				++onDelHits;
																			})
																			.entity();

	const auto& dataOnAddNoPos = wld.observers().data(observerOnAddNoPos);
	const auto& dataOnDelNoPos = wld.observers().data(observerOnDelNoPos);
	CHECK(dataOnAddNoPos.fastPath == ecs::ObserverRuntimeData::MatchFastPath::SingleNegativeTerm);
	CHECK(dataOnDelNoPos.fastPath == ecs::ObserverRuntimeData::MatchFastPath::SingleNegativeTerm);

	const auto e = wld.add();
	wld.add<Position>(e, {});
	CHECK(onAddHits == 0);
	CHECK(onDelHits == 0);

	wld.del<Position>(e);
	CHECK(onAddHits == 0);
	CHECK(onDelHits == 1);
}

TEST_CASE("Observer - single positive OnDel fast path runtime") {
	TestWorld twld;

	uint32_t onDelHits = 0;
	ecs::Entity observed = ecs::EntityBad;

	const auto observerOnDelPos = wld.observer()
																		.event(ecs::ObserverEvent::OnDel)
																		.all<Position>()
																		.on_each([&](ecs::Iter& it) {
																			++onDelHits;
																			auto entityView = it.view<ecs::Entity>();
																			observed = entityView[0];
																		})
																		.entity();

	const auto& dataOnDelPos = wld.observers().data(observerOnDelPos);
	CHECK(dataOnDelPos.fastPath == ecs::ObserverRuntimeData::MatchFastPath::SinglePositiveTerm);

	const auto e = wld.add();
	wld.add<Position>(e, {});
	onDelHits = 0;
	observed = ecs::EntityBad;

	wld.del<Position>(e);
	CHECK(onDelHits == 1);
	CHECK(observed == e);
}

TEST_CASE("Observer - fixed pair OnDel fast path runtime") {
	TestWorld twld;

	const auto relation = wld.add();
	const auto target = wld.add();
	const auto pair = ecs::Pair(relation, target);

	uint32_t hits = 0;
	ecs::Entity observed = ecs::EntityBad;

	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnDel)
														.all(pair)
														.on_each([&](ecs::Iter& it) {
															++hits;
															auto entityView = it.view<ecs::Entity>();
															observed = entityView[0];
														})
														.entity();

	const auto& data = wld.observers().data(observer);
	CHECK(data.fastPath == ecs::ObserverRuntimeData::MatchFastPath::SinglePositiveTerm);

	const auto e = wld.add();
	wld.add(e, pair);
	hits = 0;
	observed = ecs::EntityBad;

	wld.del(e, pair);
	CHECK(hits == 1);
	CHECK(observed == e);

	(void)observer;
}

TEST_CASE("Observer - deleting prefab entity keeps instances and emits no standalone inherited OnDel") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
	wld.add<Position>(prefab, {31.0f, 32.0f, 33.0f});

	const auto a = wld.instantiate(prefab);
	const auto b = wld.instantiate(prefab);

	uint32_t hits = 0;
	cnt::darray<ecs::Entity> observed;
	observed.reserve(2);

	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnDel)
														.all<Position>()
														.on_each([&](ecs::Iter& it) {
															auto entityView = it.view<ecs::Entity>();
															GAIA_EACH(it) {
																++hits;
																observed.push_back(entityView[i]);
															}
														})
														.entity();

	wld.del(prefab);
	wld.update();

	CHECK(hits == 0);
	CHECK(observed.empty());
	CHECK_FALSE(wld.valid(prefab));
	CHECK(wld.valid(a));
	CHECK(wld.valid(b));

	(void)observer;
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

	{
		TestWorld twld;
		const auto& dense = wld.add<Position>();
		const auto& sparse = wld.add<PositionSparse>();
		CHECK(dense.comp.storage_type() == ecs::DataStorageType::Table);
		CHECK(sparse.comp.storage_type() == ecs::DataStorageType::Sparse);
	}
}

TEST_CASE("Component cache - runtime registration") {
	SUBCASE("basic registration populates runtime metadata and lookups") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();

		constexpr const char* RuntimeCompName = "Runtime_Component_Basic";
		const auto entity = wld.add();
		const auto& item = cc.add(
				entity, RuntimeCompName, 0, //
				(uint32_t)sizeof(Position), //
				ecs::DataStorageType::Table, //
				(uint32_t)alignof(Position) //
		);

		const auto nameLen = (uint32_t)GAIA_STRLEN(RuntimeCompName, ecs::ComponentCacheItem::MaxNameLength);
		CHECK(item.entity == entity);
		CHECK(item.comp.id() >= 0x80000000u);
		CHECK(item.comp.size() == (uint32_t)sizeof(Position));
		CHECK(item.comp.alig() == (uint32_t)alignof(Position));
		CHECK(item.comp.soa() == 0);
		CHECK(item.name.len() == nameLen);
		CHECK(strcmp(item.name.str(), RuntimeCompName) == 0);
		CHECK(item.hashLookup.hash == core::calculate_hash64(RuntimeCompName, nameLen));

		CHECK(cc.find(item.comp.id()) == &item);
		CHECK(cc.find(item.entity) == &item);
		CHECK(wld.symbol(RuntimeCompName) == item.entity);
		CHECK(wld.symbol(RuntimeCompName, nameLen) == item.entity);
		CHECK(wld.symbol(RuntimeCompName, nameLen - 1) == ecs::EntityBad);
		CHECK(wld.get(RuntimeCompName) == item.entity);
	}

	SUBCASE("duplicate runtime registration is idempotent") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		constexpr const char* RuntimeCompName = "Runtime_Component_Duplicate";
		const auto entityA = wld.add();
		const auto& original = cc.add(entityA, RuntimeCompName, 0, 16, ecs::DataStorageType::Table, 4);
		const auto originalHash = original.hashLookup.hash;

		constexpr uint8_t SoaSizes[] = {1, 2, 4};
		const ecs::ComponentLookupHash customHash{0x123456789abcdef0ull};
		const auto entityB = wld.add(ecs::EntityKind::EK_Uni);
		const auto& duplicate =
				cc.add(entityB, RuntimeCompName, 0, 64, ecs::DataStorageType::Table, 8, 3, SoaSizes, customHash);

		CHECK(&duplicate == &original);
		CHECK(duplicate.entity == original.entity);
		CHECK(duplicate.entity == entityA);
		CHECK(duplicate.entity != entityB);
		CHECK(duplicate.comp.id() == original.comp.id());
		CHECK(duplicate.comp.size() == 16);
		CHECK(duplicate.comp.alig() == 4);
		CHECK(duplicate.comp.soa() == 0);
		CHECK(duplicate.hashLookup.hash == originalHash);
		CHECK(duplicate.entity.kind() == ecs::EntityKind::EK_Gen);
	}

	SUBCASE("custom hash and soa metadata are preserved") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();

		constexpr const char* RuntimeCompName = "Runtime_Component_SoA";
		constexpr uint8_t SoaSizes[] = {4, 1, 2};
		const ecs::ComponentLookupHash customHash{0x00f00d00baadf00dull};
		const auto entity = wld.add(ecs::EntityKind::EK_Uni);

		const auto& item = cc.add(entity, RuntimeCompName, 0, 32, ecs::DataStorageType::Table, 8, 3, SoaSizes, customHash);

		CHECK(item.entity == entity);
		CHECK(item.entity.kind() == ecs::EntityKind::EK_Uni);
		CHECK(item.comp.soa() == 3);
		CHECK(item.comp.size() == 32);
		CHECK(item.comp.alig() == 8);
		CHECK(item.hashLookup.hash == customHash.hash);
		CHECK(item.soaSizes[0] == SoaSizes[0]);
		CHECK(item.soaSizes[1] == SoaSizes[1]);
		CHECK(item.soaSizes[2] == SoaSizes[2]);

		CHECK(item.func_ctor == nullptr);
		CHECK(item.func_move_ctor == nullptr);
		CHECK(item.func_copy_ctor == nullptr);
		CHECK(item.func_dtor == nullptr);
		CHECK(item.func_copy == nullptr);
		CHECK(item.func_move == nullptr);
		CHECK(item.func_swap == nullptr);
		CHECK(item.func_cmp == nullptr);
		CHECK(item.func_save == nullptr);
		CHECK(item.func_load == nullptr);

		const auto nameLen = (uint32_t)GAIA_STRLEN(RuntimeCompName, ecs::ComponentCacheItem::MaxNameLength);
		CHECK(wld.symbol(RuntimeCompName, nameLen) == item.entity);
		CHECK(cc.get(item.comp.id()).entity == item.entity);
	}

	SUBCASE("symbol path alias and display name each have explicit behavior") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();

		auto entityA = wld.add();
		(void)cc.add(entityA, "Gameplay::Device", 0, 0, ecs::DataStorageType::Table, 1);
		auto& itemA = cc.get(entityA);

		auto entityB = wld.add();
		(void)cc.add(entityB, "Debug::Device", 0, 0, ecs::DataStorageType::Table, 1);
		auto& itemB = cc.get(entityB);

		CHECK(wld.symbol(itemA.entity) == "Gameplay::Device");
		CHECK(wld.path(itemA.entity) == "Gameplay.Device");
		CHECK(wld.alias(itemA.entity).empty());
		CHECK(wld.symbol("Gameplay::Device") == itemA.entity);
		CHECK(wld.path("Gameplay.Device") == itemA.entity);
		CHECK(wld.alias("Device") == ecs::EntityBad);
		CHECK(wld.get("Gameplay::Device") == itemA.entity);
		CHECK(wld.get("Gameplay.Device") == itemA.entity);
		CHECK(wld.get("Device") == ecs::EntityBad);
		CHECK(wld.display_name(itemA.entity) == "Gameplay.Device");
		CHECK(wld.display_name(itemB.entity) == "Debug.Device");

		CHECK(wld.alias(itemA.entity, "GameplayDevice"));
		CHECK(wld.alias("GameplayDevice") == itemA.entity);
		CHECK(wld.get("GameplayDevice") == itemA.entity);
		CHECK(wld.display_name(itemA.entity) == "GameplayDevice");

		CHECK(wld.path(itemA.entity, "gameplay.render.Device"));
		CHECK(wld.path("gameplay.render.Device") == itemA.entity);
		CHECK(wld.get("gameplay.render.Device") == itemA.entity);

		CHECK(wld.alias(itemA.entity, nullptr));
		CHECK(wld.display_name(itemA.entity) == "gameplay.render.Device");

		CHECK(wld.path(itemA.entity, nullptr));
		CHECK(wld.display_name(itemA.entity) == "Gameplay::Device");
	}

	SUBCASE("world resolve collects ambiguous matches for diagnostics") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();

		const auto entityA = wld.add();
		(void)cc.add(entityA, "Gameplay::Device", 0, 0, ecs::DataStorageType::Table, 1);

		const auto entityB = wld.add();
		(void)cc.add(entityB, "Debug::Device", 0, 0, ecs::DataStorageType::Table, 1);

		cnt::darray<ecs::Entity> out;
		wld.resolve(out, "Device");
		CHECK(out.empty());

		wld.resolve(out, "Gameplay.Device");
		CHECK(out.size() == 1);
		CHECK(out[0] == entityA);

		wld.resolve(out, "Gameplay::Device");
		CHECK(out.size() == 1);
		CHECK(out[0] == entityA);
	}

	SUBCASE("schema field registration supports add update and clear") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();

		const auto entity = wld.add();
		(void)cc.add(entity, "Runtime_Component_Schema", 0, 24, ecs::DataStorageType::Table, 8);
		auto& item = cc.get(entity);

		CHECK_FALSE(item.has_fields());
		CHECK(item.schema.size() == 0);

		CHECK(item.set_field("x", 0, ser::serialization_type_id::f32, 0, 4));
		CHECK(item.has_fields());
		CHECK(item.schema.size() == 1);
		CHECK(strcmp(item.schema[0].name, "x") == 0);
		CHECK(item.schema[0].type == ser::serialization_type_id::f32);
		CHECK(item.schema[0].offset == 0);
		CHECK(item.schema[0].size == 4);

		// Re-registering an existing field updates metadata instead of adding a duplicate.
		CHECK(item.set_field("x", 0, ser::serialization_type_id::u32, 4, 8));
		CHECK(item.schema.size() == 1);
		CHECK(strcmp(item.schema[0].name, "x") == 0);
		CHECK(item.schema[0].type == ser::serialization_type_id::u32);
		CHECK(item.schema[0].offset == 4);
		CHECK(item.schema[0].size == 8);

		// Explicit length should be honored and copied as the canonical field name.
		CHECK(item.set_field("velocity_data", 8, ser::serialization_type_id::f64, 12, 8));
		CHECK(item.schema.size() == 2);
		CHECK(strcmp(item.schema[1].name, "velocity") == 0);
		CHECK(item.schema[1].type == ser::serialization_type_id::f64);
		CHECK(item.schema[1].offset == 12);
		CHECK(item.schema[1].size == 8);

		const auto schemaSizeBeforeInvalid = item.schema.size();
		CHECK_FALSE(item.set_field(nullptr, 0, ser::serialization_type_id::u8, 0, 1));
		CHECK_FALSE(item.set_field("", 0, ser::serialization_type_id::u8, 0, 1));

		char oversizedFieldName[ecs::ComponentCacheItem::MaxNameLength + 1] = {};
		GAIA_FOR(ecs::ComponentCacheItem::MaxNameLength) oversizedFieldName[i] = 'a';
		oversizedFieldName[ecs::ComponentCacheItem::MaxNameLength] = 0;
		CHECK_FALSE(item.set_field(
				oversizedFieldName, ecs::ComponentCacheItem::MaxNameLength, ser::serialization_type_id::u8, 0, 1));

		CHECK(item.schema.size() == schemaSizeBeforeInvalid);

		item.clear_fields();
		CHECK_FALSE(item.has_fields());
		CHECK(item.schema.size() == 0);
	}

	SUBCASE("runtime descriptor ids are monotonic") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();

		const auto& a = cc.add(wld.add(), "Runtime_Component_A", 0, 1, ecs::DataStorageType::Table, 1);
		const auto& b = cc.add(wld.add(), "Runtime_Component_B", 0, 2, ecs::DataStorageType::Table, 1);
		const auto& c = cc.add(wld.add(), "Runtime_Component_C", 0, 3, ecs::DataStorageType::Table, 1);

		CHECK(a.comp.id() >= 0x80000000u);
		CHECK(b.comp.id() == a.comp.id() + 1);
		CHECK(c.comp.id() == b.comp.id() + 1);
	}
}

//------------------------------------------------------------------------------
// Hooks
//------------------------------------------------------------------------------

#if GAIA_ENABLE_HOOKS

static thread_local uint32_t hook_trigger_cnt = 0;

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

	explicit FooNonTrivial(uint32_t value): a(value) {}
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
		s.save((uintptr_t)data.ptr); // expect that the memory is alive somewhere
	}

	template <typename Serializer>
	void tag_invoke(load_tag, Serializer& s, CustomStruct& data) {
		s.load(data.size);
		uintptr_t mem;
		s.load(mem);
		data.ptr = (char*)mem;
	}
} // namespace gaia::ser

struct CustomStructInternal {
	char* ptr;
	uint32_t size;

	bool operator==(const CustomStructInternal& other) const {
		return size == other.size && 0 == memcmp(ptr, other.ptr, other.size);
	}

	template <typename Serializer>
	void save(Serializer& s) const {
		s.save(size);
		s.save((uintptr_t)ptr); // expect that the memory is alive somewhere
	}

	template <typename Serializer>
	void load(Serializer& s) {
		s.load(size);
		uintptr_t mem;
		s.load(mem);
		ptr = (char*)mem;
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

		ser::ser_buffer_binary s;

		static_assert(!ser::has_func_save<CustomStruct, ser::ser_buffer_binary&>::value);
		static_assert(ser::has_tag_save<ser::ser_buffer_binary, CustomStruct>::value);

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(CompareSerializableTypes(in, out));
		delete[] in.ptr;
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

		ser::ser_buffer_binary s;

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(CompareSerializableTypes(in, out));
		delete[] in.ptr;
	}
}

TEST_CASE("Serialization - bytes") {
	{
		SerializeStruct2 in{FooNonTrivial(111), 1, 2, true, 3.12345f};

		ser::ser_buffer_binary s;
		ser::save(s, in);

		const auto expectedBytes = ser::bytes(in);
		CHECK(expectedBytes == s.bytes());
	}
	{
		cnt::darr<uint32_t> in{};
		in.resize(10);
		GAIA_EACH(in) in[i] = i;

		ser::ser_buffer_binary s;
		ser::save(s, in);

		const auto expectedBytes = ser::bytes(in);
		CHECK(expectedBytes == s.bytes());
	}
	{
		CustomStructInternal in{};
		char data[] = {'g', 'a', 'i', 'a', '\0'};
		in.ptr = data;
		in.size = 5;

		ser::ser_buffer_binary s;
		ser::save(s, in);

		const auto expectedBytes = ser::bytes(in);
		CHECK(expectedBytes == s.bytes());
	}
}

TEST_CASE("Serialization - ser_json") {
	SUBCASE("supports nested objects and arrays") {
		ser::ser_json writer;
		writer.begin_object();
		writer.key("meta");
		writer.begin_object();
		writer.key("name");
		writer.value_string("gaia");
		writer.key("version");
		writer.value_int(1);
		writer.key("tags");
		writer.begin_array();
		writer.value_string("ecs");
		writer.value_string("runtime");
		writer.end_array();
		writer.end_object();
		writer.key("values");
		writer.begin_array();
		writer.value_int(3);
		writer.value_int(5);
		writer.value_int(8);
		writer.end_array();
		writer.end_object();

		CHECK(
				writer.str() ==
				"{\"meta\":{\"name\":\"gaia\",\"version\":1,\"tags\":[\"ecs\",\"runtime\"]},\"values\":[3,5,8]}");
	}

	SUBCASE("escapes string control characters") {
		ser::ser_json writer;
		writer.begin_object();
		writer.key("text");
		writer.value_string("a\"b\\c\n\r\t");
		writer.end_object();

		CHECK(writer.str() == "{\"text\":\"a\\\"b\\\\c\\n\\r\\t\"}");
	}

	SUBCASE("parses values and skips nested payloads") {
		ser::ser_json reader(
				"{\"name\":\"gaia\",\"payload\":[1,{\"x\":true}],\"enabled\":false,\"count\":12.5,\"nothing\":null}");
		ser::json_str key;
		ser::json_str text;
		double number = 0.0;
		bool b = true;

		CHECK(reader.expect('{'));
		CHECK(reader.parse_string(key));
		CHECK(key == "name");
		CHECK(reader.expect(':'));
		CHECK(reader.parse_string(text));
		CHECK(text == "gaia");
		CHECK(reader.consume(','));

		CHECK(reader.parse_string(key));
		CHECK(key == "payload");
		CHECK(reader.expect(':'));
		CHECK(reader.skip_value());
		CHECK(reader.consume(','));

		CHECK(reader.parse_string(key));
		CHECK(key == "enabled");
		CHECK(reader.expect(':'));
		CHECK(reader.parse_bool(b));
		CHECK(b == false);
		CHECK(reader.consume(','));

		CHECK(reader.parse_string(key));
		CHECK(key == "count");
		CHECK(reader.expect(':'));
		CHECK(reader.parse_number(number));
		CHECK(number == 12.5);
		CHECK(reader.consume(','));

		CHECK(reader.parse_string(key));
		CHECK(key == "nothing");
		CHECK(reader.expect(':'));
		CHECK(reader.parse_null());
		CHECK(reader.expect('}'));
		reader.ws();
		CHECK(reader.eof());
	}
}

TEST_CASE("Serialization - json runtime schema") {
	struct JsonRuntimeComp {
		int32_t a;
		float b;
		bool c;
		char name[8];
	};

	SUBCASE("schema fields are emitted as json object") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		const auto entity = wld.add();
		(void)cc.add(
				entity, "Runtime_Component_Json", 0, (uint32_t)sizeof(JsonRuntimeComp), ecs::DataStorageType::Table,
				(uint32_t)alignof(JsonRuntimeComp));
		auto& item = cc.get(entity);

		JsonRuntimeComp layout{};
		const auto* pBase = reinterpret_cast<const uint8_t*>(&layout);
		const auto offA = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.a) - pBase);
		const auto offB = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.b) - pBase);
		const auto offC = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.c) - pBase);
		const auto offName = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.name) - pBase);

		CHECK(item.set_field("a", 0, ser::serialization_type_id::s32, offA, (uint32_t)sizeof(layout.a)));
		CHECK(item.set_field("b", 0, ser::serialization_type_id::f32, offB, (uint32_t)sizeof(layout.b)));
		CHECK(item.set_field("c", 0, ser::serialization_type_id::b, offC, (uint32_t)sizeof(layout.c)));
		CHECK(item.set_field("name", 0, ser::serialization_type_id::c8, offName, (uint32_t)sizeof(layout.name)));

		JsonRuntimeComp value{};
		value.a = 42;
		value.b = 1.5f;
		value.c = true;
		GAIA_STRCPY(value.name, sizeof(value.name), "gaia");

		ser::ser_json writer;
		const bool ok = ecs::component_to_json(item, &value, writer);
		CHECK(ok);
		CHECK(writer.str() == "{\"a\":42,\"b\":1.5,\"c\":true,\"name\":\"gaia\"}");

		bool okStr = false;
		const auto json = ecs::component_to_json(item, &value, okStr);
		CHECK(okStr);
		CHECK(json == writer.str());
	}

	SUBCASE("nested struct fields are emitted using schema names and offsets") {
		struct Vec3 {
			float x, y, z;
		};
		struct TransformLike {
			int32_t id;
			Vec3 position;
			Vec3 velocity;
		};

		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		const auto entity = wld.add();
		(void)cc.add(
				entity, "Runtime_Component_Json_Nested", 0, (uint32_t)sizeof(TransformLike), ecs::DataStorageType::Table,
				(uint32_t)alignof(TransformLike));
		auto& item = cc.get(entity);

		TransformLike layout{};
		const auto* pBase = reinterpret_cast<const uint8_t*>(&layout);
		const auto offId = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.id) - pBase);
		const auto offPosX = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.position.x) - pBase);
		const auto offPosY = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.position.y) - pBase);
		const auto offPosZ = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.position.z) - pBase);
		const auto offVelX = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.velocity.x) - pBase);
		const auto offVelY = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.velocity.y) - pBase);
		const auto offVelZ = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.velocity.z) - pBase);

		CHECK(item.set_field("id", 0, ser::serialization_type_id::s32, offId, (uint32_t)sizeof(layout.id)));
		CHECK(
				item.set_field("position.x", 0, ser::serialization_type_id::f32, offPosX, (uint32_t)sizeof(layout.position.x)));
		CHECK(
				item.set_field("position.y", 0, ser::serialization_type_id::f32, offPosY, (uint32_t)sizeof(layout.position.y)));
		CHECK(
				item.set_field("position.z", 0, ser::serialization_type_id::f32, offPosZ, (uint32_t)sizeof(layout.position.z)));
		CHECK(
				item.set_field("velocity.x", 0, ser::serialization_type_id::f32, offVelX, (uint32_t)sizeof(layout.velocity.x)));
		CHECK(
				item.set_field("velocity.y", 0, ser::serialization_type_id::f32, offVelY, (uint32_t)sizeof(layout.velocity.y)));
		CHECK(
				item.set_field("velocity.z", 0, ser::serialization_type_id::f32, offVelZ, (uint32_t)sizeof(layout.velocity.z)));

		TransformLike value{};
		value.id = 7;
		value.position = {1.25f, -2.5f, 0.0f};
		value.velocity = {10.0f, 11.5f, -12.0f};

		ser::ser_json writer;
		const bool ok = ecs::component_to_json(item, &value, writer);
		CHECK(ok);
		CHECK(
				writer.str() == "{\"id\":7,\"position.x\":1.25,\"position.y\":-2.5,\"position.z\":0,\"velocity.x\":10,"
												"\"velocity.y\":11.5,\"velocity.z\":-12}");
	}

	SUBCASE("array and array-of-struct fields are emitted using schema names and offsets") {
		struct InventoryEntry {
			uint16_t id;
			uint8_t count;
		};
		struct RuntimeArraysComp {
			float weights[3];
			InventoryEntry inventory[2];
			bool active[2];
		};

		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		const auto entity = wld.add();
		(void)cc.add(
				entity, "Runtime_Component_Json_Arrays", 0, (uint32_t)sizeof(RuntimeArraysComp), ecs::DataStorageType::Table,
				(uint32_t)alignof(RuntimeArraysComp));
		auto& item = cc.get(entity);

		RuntimeArraysComp layout{};
		const auto* pBase = reinterpret_cast<const uint8_t*>(&layout);
		const auto offW0 = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.weights[0]) - pBase);
		const auto offW1 = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.weights[1]) - pBase);
		const auto offW2 = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.weights[2]) - pBase);
		const auto offInv0Id = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.inventory[0].id) - pBase);
		const auto offInv0Count = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.inventory[0].count) - pBase);
		const auto offInv1Id = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.inventory[1].id) - pBase);
		const auto offInv1Count = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.inventory[1].count) - pBase);
		const auto offActive0 = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.active[0]) - pBase);
		const auto offActive1 = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.active[1]) - pBase);

		CHECK(item.set_field("weights[0]", 0, ser::serialization_type_id::f32, offW0, (uint32_t)sizeof(layout.weights[0])));
		CHECK(item.set_field("weights[1]", 0, ser::serialization_type_id::f32, offW1, (uint32_t)sizeof(layout.weights[1])));
		CHECK(item.set_field("weights[2]", 0, ser::serialization_type_id::f32, offW2, (uint32_t)sizeof(layout.weights[2])));
		CHECK(item.set_field(
				"inventory[0].id", 0, ser::serialization_type_id::u16, offInv0Id, (uint32_t)sizeof(layout.inventory[0].id)));
		CHECK(item.set_field(
				"inventory[0].count", 0, ser::serialization_type_id::u8, offInv0Count,
				(uint32_t)sizeof(layout.inventory[0].count)));
		CHECK(item.set_field(
				"inventory[1].id", 0, ser::serialization_type_id::u16, offInv1Id, (uint32_t)sizeof(layout.inventory[1].id)));
		CHECK(item.set_field(
				"inventory[1].count", 0, ser::serialization_type_id::u8, offInv1Count,
				(uint32_t)sizeof(layout.inventory[1].count)));
		CHECK(
				item.set_field("active[0]", 0, ser::serialization_type_id::b, offActive0, (uint32_t)sizeof(layout.active[0])));
		CHECK(
				item.set_field("active[1]", 0, ser::serialization_type_id::b, offActive1, (uint32_t)sizeof(layout.active[1])));

		RuntimeArraysComp value{};
		value.weights[0] = 0.25f;
		value.weights[1] = 0.5f;
		value.weights[2] = 1.0f;
		value.inventory[0].id = 101;
		value.inventory[0].count = 3;
		value.inventory[1].id = 202;
		value.inventory[1].count = 9;
		value.active[0] = true;
		value.active[1] = false;

		ser::ser_json writer;
		const bool ok = ecs::component_to_json(item, &value, writer);
		CHECK(ok);
		CHECK(
				writer.str() ==
				"{\"weights[0]\":0.25,\"weights[1]\":0.5,\"weights[2]\":1,\"inventory[0].id\":101,\"inventory[0].count\":3,"
				"\"inventory[1].id\":202,\"inventory[1].count\":9,\"active[0]\":true,\"active[1]\":false}");
	}

	SUBCASE("unsupported field types are emitted as null and reported") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		const auto entity = wld.add();
		(void)cc.add(entity, "Runtime_Component_Json_Unsupported", 0, 4, ecs::DataStorageType::Table, 4);
		auto& item = cc.get(entity);

		CHECK(item.set_field("blob", 0, ser::serialization_type_id::trivial_wrapper, 0, 4));
		uint32_t value = 123;

		ser::ser_json writer;
		const bool ok = ecs::component_to_json(item, &value, writer);
		CHECK_FALSE(ok);
		CHECK(writer.str() == "{\"blob\":null}");
	}

	SUBCASE("out of bounds schema fields are emitted as null and reported") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		const auto entity = wld.add();
		(void)cc.add(entity, "Runtime_Component_Json_Oob", 0, 4, ecs::DataStorageType::Table, 4);
		auto& item = cc.get(entity);

		CHECK(item.set_field("too_far", 0, ser::serialization_type_id::u32, 8, 4));
		uint32_t value = 123;

		ser::ser_json writer;
		const bool ok = ecs::component_to_json(item, &value, writer);
		CHECK_FALSE(ok);
		CHECK(writer.str() == "{\"too_far\":null}");
	}

	SUBCASE("json_to_component loads schema payload") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		const auto entity = wld.add();
		(void)cc.add(
				entity, "Runtime_Component_Json_Read", 0, (uint32_t)sizeof(JsonRuntimeComp), ecs::DataStorageType::Table,
				(uint32_t)alignof(JsonRuntimeComp));
		auto& item = cc.get(entity);

		JsonRuntimeComp layout{};
		const auto* pBase = reinterpret_cast<const uint8_t*>(&layout);
		const auto offA = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.a) - pBase);
		const auto offB = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.b) - pBase);
		const auto offC = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.c) - pBase);
		const auto offName = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.name) - pBase);
		CHECK(item.set_field("a", 0, ser::serialization_type_id::s32, offA, (uint32_t)sizeof(layout.a)));
		CHECK(item.set_field("b", 0, ser::serialization_type_id::f32, offB, (uint32_t)sizeof(layout.b)));
		CHECK(item.set_field("c", 0, ser::serialization_type_id::b, offC, (uint32_t)sizeof(layout.c)));
		CHECK(item.set_field("name", 0, ser::serialization_type_id::c8, offName, (uint32_t)sizeof(layout.name)));

		JsonRuntimeComp in{};
		in.a = -123;
		in.b = 9.25f;
		in.c = true;
		GAIA_STRCPY(in.name, sizeof(in.name), "reader");

		bool okWrite = false;
		const auto json = ecs::component_to_json(item, &in, okWrite);
		CHECK(okWrite);

		JsonRuntimeComp out{};
		ser::ser_json reader(json.data(), (uint32_t)json.size());
		bool okRead = true;
		CHECK(ecs::json_to_component(item, &out, reader, okRead));
		CHECK(okRead);
		reader.ws();
		CHECK(reader.eof());

		CHECK(out.a == in.a);
		CHECK(out.b == in.b);
		CHECK(out.c == in.c);
		CHECK(strcmp(out.name, in.name) == 0);
	}

	SUBCASE("json_to_component loads raw payload") {
		struct JsonRawComp {
			uint32_t a;
			float b;
		};

		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		const auto entity = wld.add();
		(void)cc.add(
				entity, "Runtime_Component_Json_Raw_Read", 0, (uint32_t)sizeof(JsonRawComp), ecs::DataStorageType::Table,
				(uint32_t)alignof(JsonRawComp));
		auto& item = cc.get(entity);
		item.clear_fields();

		JsonRawComp in{77u, 1.75f};

		ser::ser_buffer_binary raw;
		{
			auto s = ser::make_serializer(raw);
			item.save(s, &in, 0, 1, 1);
		}

		ser::ser_json writer;
		writer.begin_object();
		writer.key("$raw");
		writer.begin_array();
		{
			const auto* pData = raw.data();
			GAIA_FOR(raw.bytes()) writer.value_int(pData[i]);
		}
		writer.end_array();
		writer.end_object();

		JsonRawComp out{};
		ser::ser_json reader(writer.str().data(), (uint32_t)writer.str().size());
		bool okRead = true;
		CHECK(ecs::json_to_component(item, &out, reader, okRead));
		CHECK(okRead);
		reader.ws();
		CHECK(reader.eof());

		CHECK(out.a == in.a);
		CHECK(out.b == in.b);
	}

	SUBCASE("json_to_component marks unknown fields as best-effort failures but keeps parsing") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		const auto entity = wld.add();
		(void)cc.add(
				entity, "Runtime_Component_Json_Unknown_Fields", 0, (uint32_t)sizeof(JsonRuntimeComp),
				ecs::DataStorageType::Table, (uint32_t)alignof(JsonRuntimeComp));
		auto& item = cc.get(entity);

		JsonRuntimeComp layout{};
		const auto* pBase = reinterpret_cast<const uint8_t*>(&layout);
		const auto offA = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.a) - pBase);
		const auto offB = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.b) - pBase);
		const auto offC = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.c) - pBase);
		const auto offName = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.name) - pBase);
		CHECK(item.set_field("a", 0, ser::serialization_type_id::s32, offA, (uint32_t)sizeof(layout.a)));
		CHECK(item.set_field("b", 0, ser::serialization_type_id::f32, offB, (uint32_t)sizeof(layout.b)));
		CHECK(item.set_field("c", 0, ser::serialization_type_id::b, offC, (uint32_t)sizeof(layout.c)));
		CHECK(item.set_field("name", 0, ser::serialization_type_id::c8, offName, (uint32_t)sizeof(layout.name)));

		JsonRuntimeComp out{};
		ser::ser_json reader("{\"a\":11,\"unknown\":{\"nested\":[1,true,\"x\"]},\"b\":2.25,\"c\":false,\"name\":\"ok\"}");
		bool okRead = true;
		CHECK(ecs::json_to_component(item, &out, reader, okRead));
		CHECK_FALSE(okRead);
		reader.ws();
		CHECK(reader.eof());

		CHECK(out.a == 11);
		CHECK(out.b == 2.25f);
		CHECK(out.c == false);
		CHECK(strcmp(out.name, "ok") == 0);
	}

	SUBCASE("json_to_component fails on malformed schema field payload type") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		const auto entity = wld.add();
		(void)cc.add(
				entity, "Runtime_Component_Json_Bad_Type", 0, (uint32_t)sizeof(JsonRuntimeComp), ecs::DataStorageType::Table,
				(uint32_t)alignof(JsonRuntimeComp));
		auto& item = cc.get(entity);

		JsonRuntimeComp layout{};
		const auto* pBase = reinterpret_cast<const uint8_t*>(&layout);
		const auto offA = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.a) - pBase);
		CHECK(item.set_field("a", 0, ser::serialization_type_id::s32, offA, (uint32_t)sizeof(layout.a)));

		JsonRuntimeComp out{};
		ser::ser_json reader("{\"a\":\"bad\"}");
		bool okRead = true;
		CHECK_FALSE(ecs::json_to_component(item, &out, reader, okRead));
	}

	SUBCASE("json_to_component treats null field values as best-effort failures") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		const auto entity = wld.add();
		(void)cc.add(
				entity, "Runtime_Component_Json_Null_Field", 0, (uint32_t)sizeof(JsonRuntimeComp), ecs::DataStorageType::Table,
				(uint32_t)alignof(JsonRuntimeComp));
		auto& item = cc.get(entity);

		JsonRuntimeComp layout{};
		const auto* pBase = reinterpret_cast<const uint8_t*>(&layout);
		const auto offA = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.a) - pBase);
		CHECK(item.set_field("a", 0, ser::serialization_type_id::s32, offA, (uint32_t)sizeof(layout.a)));

		JsonRuntimeComp out{};
		out.a = 123;
		ser::ser_json reader("{\"a\":null}");
		bool okRead = true;
		CHECK(ecs::json_to_component(item, &out, reader, okRead));
		CHECK_FALSE(okRead);
		CHECK(out.a == 123);
	}

	SUBCASE("json_to_component clamps out-of-range integers and marks best-effort failure") {
		struct JsonIntsComp {
			uint8_t u8;
			uint16_t u16;
			int8_t s8;
		};

		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		const auto entity = wld.add();
		(void)cc.add(
				entity, "Runtime_Component_Json_Ints", 0, (uint32_t)sizeof(JsonIntsComp), ecs::DataStorageType::Table,
				(uint32_t)alignof(JsonIntsComp));
		auto& item = cc.get(entity);

		JsonIntsComp layout{};
		const auto* pBase = reinterpret_cast<const uint8_t*>(&layout);
		const auto offU8 = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.u8) - pBase);
		const auto offU16 = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.u16) - pBase);
		const auto offS8 = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.s8) - pBase);
		CHECK(item.set_field("u8", 0, ser::serialization_type_id::u8, offU8, (uint32_t)sizeof(layout.u8)));
		CHECK(item.set_field("u16", 0, ser::serialization_type_id::u16, offU16, (uint32_t)sizeof(layout.u16)));
		CHECK(item.set_field("s8", 0, ser::serialization_type_id::s8, offS8, (uint32_t)sizeof(layout.s8)));

		JsonIntsComp out{};
		ser::ser_json reader("{\"u8\":-3,\"u16\":70000,\"s8\":300.9}");
		bool okRead = true;
		CHECK(ecs::json_to_component(item, &out, reader, okRead));
		CHECK_FALSE(okRead);
		CHECK(out.u8 == (uint8_t)0);
		CHECK(out.u16 == (uint16_t)65535);
		CHECK(out.s8 == (int8_t)127);
	}

	SUBCASE("json_to_component reports string truncation as best-effort failure") {
		struct JsonNameComp {
			char name[4];
		};

		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		const auto entity = wld.add();
		(void)cc.add(
				entity, "Runtime_Component_Json_String_Truncate", 0, (uint32_t)sizeof(JsonNameComp),
				ecs::DataStorageType::Table, (uint32_t)alignof(JsonNameComp));
		auto& item = cc.get(entity);

		JsonNameComp layout{};
		const auto* pBase = reinterpret_cast<const uint8_t*>(&layout);
		const auto offName = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.name) - pBase);
		CHECK(item.set_field("name", 0, ser::serialization_type_id::c8, offName, (uint32_t)sizeof(layout.name)));

		JsonNameComp out{};
		ser::ser_json reader("{\"name\":\"abcdef\"}");
		bool okRead = true;
		CHECK(ecs::json_to_component(item, &out, reader, okRead));
		CHECK_FALSE(okRead);
		CHECK(strcmp(out.name, "abc") == 0);
	}

	SUBCASE("json_to_component fails on malformed raw byte arrays") {
		struct JsonRawComp {
			uint32_t a;
		};

		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		const auto entity = wld.add();
		(void)cc.add(
				entity, "Runtime_Component_Json_Raw_Malformed", 0, (uint32_t)sizeof(JsonRawComp), ecs::DataStorageType::Table,
				(uint32_t)alignof(JsonRawComp));
		auto& item = cc.get(entity);
		item.clear_fields();

		{
			JsonRawComp out{};
			ser::ser_json reader("{\"$raw\":[1,2.5]}");
			bool okRead = true;
			CHECK_FALSE(ecs::json_to_component(item, &out, reader, okRead));
		}

		{
			JsonRawComp out{};
			ser::ser_json reader("{\"$raw\":[1,256]}");
			bool okRead = true;
			CHECK_FALSE(ecs::json_to_component(item, &out, reader, okRead));
		}
	}

	SUBCASE("json_to_component marks payloads without schema or raw data as best-effort failures") {
		struct JsonRawComp {
			uint32_t a;
		};

		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		const auto entity = wld.add();
		(void)cc.add(
				entity, "Runtime_Component_Json_No_Data", 0, (uint32_t)sizeof(JsonRawComp), ecs::DataStorageType::Table,
				(uint32_t)alignof(JsonRawComp));
		auto& item = cc.get(entity);
		item.clear_fields();

		JsonRawComp out{};
		ser::ser_json reader("{\"foo\":1}");
		bool okRead = true;
		CHECK(ecs::json_to_component(item, &out, reader, okRead));
		CHECK_FALSE(okRead);
	}

	SUBCASE("json_to_component reports structured diagnostics") {
		struct JsonDiagComp {
			uint8_t value;
			char name[4];
		};

		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		const auto entity = wld.add();
		(void)cc.add(
				entity, "Runtime_Component_Json_Diagnostics", 0, (uint32_t)sizeof(JsonDiagComp), ecs::DataStorageType::Table,
				(uint32_t)alignof(JsonDiagComp));
		auto& item = cc.get(entity);

		JsonDiagComp layout{};
		const auto* pBase = reinterpret_cast<const uint8_t*>(&layout);
		const auto offValue = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.value) - pBase);
		const auto offName = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.name) - pBase);
		CHECK(item.set_field("value", 0, ser::serialization_type_id::u8, offValue, (uint32_t)sizeof(layout.value)));
		CHECK(item.set_field("name", 0, ser::serialization_type_id::c8, offName, (uint32_t)sizeof(layout.name)));

		JsonDiagComp out{};
		ser::ser_json reader("{\"value\":-2,\"name\":\"abcdef\",\"unknown\":1}");
		ser::JsonDiagnostics diagnostics;
		CHECK(ecs::json_to_component(item, &out, reader, diagnostics, "Runtime_Component_Json_Diagnostics"));
		CHECK(diagnostics.hasWarnings);

		bool hasUnknownField = false;
		bool hasAdjustedValue = false;
		bool hasAdjustedName = false;
		for (const auto& diag: diagnostics.items) {
			if (diag.reason == ser::JsonDiagReason::UnknownField && diag.path == "Runtime_Component_Json_Diagnostics.unknown")
				hasUnknownField = true;
			if (diag.reason == ser::JsonDiagReason::FieldValueAdjusted &&
					diag.path == "Runtime_Component_Json_Diagnostics.value")
				hasAdjustedValue = true;
			if (diag.reason == ser::JsonDiagReason::FieldValueAdjusted &&
					diag.path == "Runtime_Component_Json_Diagnostics.name")
				hasAdjustedName = true;
		}
		CHECK(hasUnknownField);
		CHECK(hasAdjustedValue);
		CHECK(hasAdjustedName);
	}
}

TEST_CASE("Serialization - runtime context init") {
	ser::bin_stream stream;

	const auto ctx = ser::serializer::bind_ctx(stream);
	ser::serializer serializer{ctx};
	CHECK(serializer.valid());

	uint32_t in = 0xDEADBEEF;
	uint32_t out = 0;
	serializer.save(in);
	serializer.seek(0);
	serializer.load(out);
	CHECK(in == out);
}

TEST_CASE("Serialization - simple") {
	{
		Int3 in{1, 2, 3}, out{};

		ser::ser_buffer_binary s;

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(CompareSerializableTypes(in, out));
	}

	{
		Position in{1, 2, 3}, out{};

		ser::ser_buffer_binary s;

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(CompareSerializableTypes(in, out));
	}

	{
		PositionSoA in{1, 2, 3}, out{};

		ser::ser_buffer_binary s;

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(CompareSerializableTypes(in, out));
	}

	{
		SerializeStruct1 in{1, 2, true, 3.12345f}, out{};

		ser::ser_buffer_binary s;

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(CompareSerializableTypes(in, out));
	}

	{
		SerializeStruct2 in{FooNonTrivial(111), 1, 2, true, 3.12345f}, out{};

		ser::ser_buffer_binary s;

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

GAIA_MSVC_WARNING_PUSH()
GAIA_MSVC_WARNING_DISABLE(4324)

struct SerializeStructSArray_PositionSoA {
	cnt::sarray_soa<PositionSoA, 8> arr;
	float f;

	bool operator==(const SerializeStructSArray_PositionSoA& other) const {
		return f == other.f && arr == other.arr;
	}
};

GAIA_MSVC_WARNING_POP()

struct SerializeStructSArrayNonTrivial {
	cnt::sarray<FooNonTrivial, 8> arr;
	float f;

	bool operator==(const SerializeStructSArrayNonTrivial& other) const {
		return f == other.f && arr == other.arr;
	}
};

GAIA_MSVC_WARNING_PUSH()
GAIA_MSVC_WARNING_DISABLE(4324)

struct SerializeStructDArray_PositionSoA {
	cnt::darr_soa<PositionSoA> arr;
	float f;

	bool operator==(const SerializeStructDArray_PositionSoA& other) const {
		return f == other.f && arr == other.arr;
	}
};

struct SerializeStructDArray_PositionSoA_AOS {
	cnt::darr<PositionSoA> arr;
	float f;

	bool operator==(const SerializeStructDArray_PositionSoA_AOS& other) const {
		return f == other.f && arr == other.arr;
	}
};

GAIA_MSVC_WARNING_POP()

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

		ser::ser_buffer_binary s;

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

		ser::ser_buffer_binary s;

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(CompareSerializableTypes(in, out));
	}

	{
		SerializeStructSArrayNonTrivial in{}, out{};
		GAIA_EACH(in.arr) in.arr[i] = FooNonTrivial(i);
		in.f = 3.12345f;

		ser::ser_buffer_binary s;

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

		ser::ser_buffer_binary s;

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(CompareSerializableTypes(in, out));
	}

	{
		SerializeStructDArray_PositionSoA_AOS in{}, out{};
		GAIA_FOR(10) {
			in.arr.push_back({(float)i, (float)i, (float)i});
		}
		in.f = 3.12345f;

		ser::ser_buffer_binary s;

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(CompareSerializableTypes(in, out));
	}

	{
		SerializeStructDArray_PositionSoA in{}, out{};
		GAIA_FOR(10) {
			in.arr.push_back({(float)i, (float)i, (float)i});
		}
		in.f = 3.12345f;

		ser::ser_buffer_binary s;

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

		ser::ser_buffer_binary s;

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(CompareSerializableTypes(in, out));

		for (auto& a: in.arr)
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

		ser::ser_buffer_binary s;

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

		ser::ser_buffer_binary s;

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
	}
}

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

	in.save();

	//--------
	in.cleanup();
	initComponents(in);
	CHECK(in.load());

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

	ser::bin_stream buffer;
	in.set_serializer(buffer);
	in.save();

	TestWorld twld;
	initComponents(wld);
	CHECK(wld.load(buffer));

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

TEST_CASE("Serialization - world json") {
	TestWorld twld;
	(void)wld.add<Position>();
	(void)wld.add<PositionSoA>();

	const auto positionEntity = wld.comp_cache().get<Position>().entity;
	auto& posItem = wld.comp_cache_mut().get(positionEntity);
	posItem.clear_fields();

	Position layout{};
	const auto* pBase = reinterpret_cast<const uint8_t*>(&layout);
	const auto offX = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.x) - pBase);
	const auto offY = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.y) - pBase);
	const auto offZ = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.z) - pBase);
	CHECK(posItem.set_field("x", 0, ser::serialization_type_id::f32, offX, (uint32_t)sizeof(layout.x)));
	CHECK(posItem.set_field("y", 0, ser::serialization_type_id::f32, offY, (uint32_t)sizeof(layout.y)));
	CHECK(posItem.set_field("z", 0, ser::serialization_type_id::f32, offZ, (uint32_t)sizeof(layout.z)));

	const auto e = wld.add();
	wld.add<Position>(e, {1.0f, 2.0f, 3.0f});
	wld.add<PositionSoA>(e, {10.0f, 20.0f, 30.0f});
	wld.name(e, "Player");

	ser::ser_json writer;
	const bool ok = wld.save_json(writer);
	CHECK(ok);

	const auto& json = writer.str();
	CHECK(json.find("\"format\":1") != BadIndex);
	CHECK(json.find("\"name\":\"Player\"") != BadIndex);
	CHECK(json.find("\"Position\":{\"x\":1,\"y\":2,\"z\":3}") != BadIndex);
	CHECK(json.find("\"PositionSoA\":{\"$raw\":[") != BadIndex);

	bool okStr = false;
	const auto jsonStr = wld.save_json(okStr);
	CHECK(okStr);
	CHECK(jsonStr.find("\"format\":1") != BadIndex);
	CHECK(jsonStr.find("\"binary\":[") != BadIndex);

	TestWorld twldOut;
	(void)twldOut.m_w.add<Position>();
	(void)twldOut.m_w.add<PositionSoA>();
	CHECK(twldOut.m_w.load_json(jsonStr));

	const auto loaded = twldOut.m_w.get("Player");
	CHECK(loaded != ecs::EntityBad);

	const auto posLoaded = twldOut.m_w.get<Position>(loaded);
	CHECK(posLoaded.x == 1.0f);
	CHECK(posLoaded.y == 2.0f);
	CHECK(posLoaded.z == 3.0f);

	const auto posSoaLoaded = twldOut.m_w.get<PositionSoA>(loaded);
	CHECK(posSoaLoaded.x == 10.0f);
	CHECK(posSoaLoaded.y == 20.0f);
	CHECK(posSoaLoaded.z == 30.0f);

	CHECK_FALSE(twldOut.m_w.load_json("{\"format\":1}"));

	ser::ser_json noBinaryWriter;
	const bool okNoBinary = wld.save_json(noBinaryWriter, ser::JsonSaveFlags::RawFallback);
	CHECK(okNoBinary);
	CHECK(noBinaryWriter.str().find("\"binary\":[") == BadIndex);
	TestWorld twldNoBinary;
	(void)twldNoBinary.m_w.add<Position>();
	(void)twldNoBinary.m_w.add<PositionSoA>();
	CHECK_FALSE(twldNoBinary.m_w.load_json(noBinaryWriter.str()));

	ser::ser_json strictWriter;
	const bool okStrict = wld.save_json(strictWriter, ser::JsonSaveFlags::BinarySnapshot /*no raw fallback on purpose*/);
	CHECK_FALSE(okStrict);
	CHECK(strictWriter.str().find("\"PositionSoA\":null") != BadIndex);
	ser::ser_json strictNoBinaryWriter;
	const bool okStrictNoBinary = wld.save_json(strictNoBinaryWriter, ser::JsonSaveFlags::None);
	CHECK_FALSE(okStrictNoBinary);
	TestWorld twldStrictNoBinary;
	(void)twldStrictNoBinary.m_w.add<Position>();
	(void)twldStrictNoBinary.m_w.add<PositionSoA>();
	CHECK_FALSE(twldStrictNoBinary.m_w.load_json(strictNoBinaryWriter.str()));

	TestWorld twldNoBinaryDiag;
	(void)twldNoBinaryDiag.m_w.add<Position>();
	(void)twldNoBinaryDiag.m_w.add<PositionSoA>();
	ser::JsonDiagnostics noBinaryDiagnostics;
	CHECK(twldNoBinaryDiag.m_w.load_json(noBinaryWriter.str(), noBinaryDiagnostics));
	CHECK(noBinaryDiagnostics.hasWarnings);
	bool hasSoaRawWarning = false;
	for (const auto& diag: noBinaryDiagnostics.items) {
		if (diag.reason == ser::JsonDiagReason::SoaRawUnsupported) {
			hasSoaRawWarning = true;
			break;
		}
	}
	CHECK(hasSoaRawWarning);

	ser::JsonDiagnostics missingArchetypesDiagnostics;
	CHECK_FALSE(twldNoBinaryDiag.m_w.load_json("{\"format\":1}", missingArchetypesDiagnostics));
	CHECK(missingArchetypesDiagnostics.hasErrors);
	bool hasMissingArchetypesError = false;
	for (const auto& diag: missingArchetypesDiagnostics.items) {
		if (diag.reason == ser::JsonDiagReason::MissingArchetypesSection) {
			hasMissingArchetypesError = true;
			break;
		}
	}
	CHECK(hasMissingArchetypesError);

	ser::JsonDiagnostics missingFormatDiagnostics;
	CHECK_FALSE(twldNoBinaryDiag.m_w.load_json("{\"archetypes\":[]}", missingFormatDiagnostics));
	CHECK(missingFormatDiagnostics.hasErrors);
	bool hasMissingFormatError = false;
	for (const auto& diag: missingFormatDiagnostics.items) {
		if (diag.reason == ser::JsonDiagReason::MissingFormatField) {
			hasMissingFormatError = true;
			break;
		}
	}
	CHECK(hasMissingFormatError);

	ser::JsonDiagnostics unsupportedFormatDiagnostics;
	CHECK_FALSE(twldNoBinaryDiag.m_w.load_json("{\"format\":2,\"archetypes\":[]}", unsupportedFormatDiagnostics));
	CHECK(unsupportedFormatDiagnostics.hasErrors);
	bool hasUnsupportedFormatError = false;
	for (const auto& diag: unsupportedFormatDiagnostics.items) {
		if (diag.reason == ser::JsonDiagReason::UnsupportedFormatVersion) {
			hasUnsupportedFormatError = true;
			break;
		}
	}
	CHECK(hasUnsupportedFormatError);
}

TEST_CASE("Serialization - world json schema nested arrays") {
	struct Vec3 {
		float x, y, z;
	};
	struct JsonComplexComp {
		Vec3 transform[2];
		uint16_t itemCounts[3];
		bool active[2];
		char label[12];
	};

	auto register_schema = [](ecs::World& world) {
		(void)world.add<JsonComplexComp>();
		const auto compEntity = world.comp_cache().get<JsonComplexComp>().entity;
		auto& item = world.comp_cache_mut().get(compEntity);
		item.clear_fields();

		JsonComplexComp layout{};
		const auto* pBase = reinterpret_cast<const uint8_t*>(&layout);
		const auto offT0X = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.transform[0].x) - pBase);
		const auto offT0Y = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.transform[0].y) - pBase);
		const auto offT0Z = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.transform[0].z) - pBase);
		const auto offT1X = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.transform[1].x) - pBase);
		const auto offT1Y = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.transform[1].y) - pBase);
		const auto offT1Z = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.transform[1].z) - pBase);
		const auto offCount0 = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.itemCounts[0]) - pBase);
		const auto offCount1 = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.itemCounts[1]) - pBase);
		const auto offCount2 = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.itemCounts[2]) - pBase);
		const auto offActive0 = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.active[0]) - pBase);
		const auto offActive1 = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.active[1]) - pBase);
		const auto offLabel = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.label[0]) - pBase);

		CHECK(item.set_field(
				"transform[0].x", 0, ser::serialization_type_id::f32, offT0X, (uint32_t)sizeof(layout.transform[0].x)));
		CHECK(item.set_field(
				"transform[0].y", 0, ser::serialization_type_id::f32, offT0Y, (uint32_t)sizeof(layout.transform[0].y)));
		CHECK(item.set_field(
				"transform[0].z", 0, ser::serialization_type_id::f32, offT0Z, (uint32_t)sizeof(layout.transform[0].z)));
		CHECK(item.set_field(
				"transform[1].x", 0, ser::serialization_type_id::f32, offT1X, (uint32_t)sizeof(layout.transform[1].x)));
		CHECK(item.set_field(
				"transform[1].y", 0, ser::serialization_type_id::f32, offT1Y, (uint32_t)sizeof(layout.transform[1].y)));
		CHECK(item.set_field(
				"transform[1].z", 0, ser::serialization_type_id::f32, offT1Z, (uint32_t)sizeof(layout.transform[1].z)));
		CHECK(item.set_field(
				"itemCounts[0]", 0, ser::serialization_type_id::u16, offCount0, (uint32_t)sizeof(layout.itemCounts[0])));
		CHECK(item.set_field(
				"itemCounts[1]", 0, ser::serialization_type_id::u16, offCount1, (uint32_t)sizeof(layout.itemCounts[1])));
		CHECK(item.set_field(
				"itemCounts[2]", 0, ser::serialization_type_id::u16, offCount2, (uint32_t)sizeof(layout.itemCounts[2])));
		CHECK(
				item.set_field("active[0]", 0, ser::serialization_type_id::b, offActive0, (uint32_t)sizeof(layout.active[0])));
		CHECK(
				item.set_field("active[1]", 0, ser::serialization_type_id::b, offActive1, (uint32_t)sizeof(layout.active[1])));
		CHECK(item.set_field("label", 0, ser::serialization_type_id::c8, offLabel, (uint32_t)sizeof(layout.label)));
	};

	TestWorld twld;
	register_schema(wld);

	const auto e = wld.add();
	JsonComplexComp value{};
	value.transform[0] = {1.25f, 2.5f, 3.75f};
	value.transform[1] = {4.0f, 5.25f, 6.5f};
	value.itemCounts[0] = 11;
	value.itemCounts[1] = 22;
	value.itemCounts[2] = 33;
	value.active[0] = true;
	value.active[1] = false;
	GAIA_STRCPY(value.label, sizeof(value.label), "crate");
	wld.add<JsonComplexComp>(e, value);
	wld.name(e, "ComplexEntity");

	ser::ser_json writer;
	CHECK(wld.save_json(writer));
	const auto& json = writer.str();
	const auto compSymbol = wld.symbol(wld.comp_cache().get<JsonComplexComp>().entity);
	util::str jsonCompPrefix;
	jsonCompPrefix.append('"');
	jsonCompPrefix.append(compSymbol);
	jsonCompPrefix.append("\":{\"transform[0].x\":1.25");
	CHECK(json.find(jsonCompPrefix.view()) != BadIndex);
	CHECK(json.find("\"transform[1].z\":6.5") != BadIndex);
	CHECK(json.find("\"itemCounts[2]\":33") != BadIndex);
	CHECK(json.find("\"active[0]\":true") != BadIndex);
	CHECK(json.find("\"label\":\"crate\"") != BadIndex);

	TestWorld twldOut;
	register_schema(twldOut.m_w);
	CHECK(twldOut.m_w.load_json(json));

	const auto loaded = twldOut.m_w.get("ComplexEntity");
	CHECK(loaded != ecs::EntityBad);
	const auto loadedComp = twldOut.m_w.get<JsonComplexComp>(loaded);
	CHECK(loadedComp.transform[0].x == 1.25f);
	CHECK(loadedComp.transform[0].y == 2.5f);
	CHECK(loadedComp.transform[0].z == 3.75f);
	CHECK(loadedComp.transform[1].x == 4.0f);
	CHECK(loadedComp.transform[1].y == 5.25f);
	CHECK(loadedComp.transform[1].z == 6.5f);
	CHECK(loadedComp.itemCounts[0] == 11);
	CHECK(loadedComp.itemCounts[1] == 22);
	CHECK(loadedComp.itemCounts[2] == 33);
	CHECK(loadedComp.active[0] == true);
	CHECK(loadedComp.active[1] == false);
	CHECK(std::string(loadedComp.label) == "crate");

	ser::ser_json noBinaryWriter;
	CHECK(wld.save_json(noBinaryWriter, ser::JsonSaveFlags::RawFallback));
	CHECK(noBinaryWriter.str().find("\"binary\":[") == BadIndex);

	TestWorld twldOutNoBinary;
	register_schema(twldOutNoBinary.m_w);
	CHECK(twldOutNoBinary.m_w.load_json(noBinaryWriter.str()));

	const auto loadedNoBinary = twldOutNoBinary.m_w.get("ComplexEntity");
	CHECK(loadedNoBinary != ecs::EntityBad);
	const auto loadedCompNoBinary = twldOutNoBinary.m_w.get<JsonComplexComp>(loadedNoBinary);
	CHECK(loadedCompNoBinary.transform[0].x == 1.25f);
	CHECK(loadedCompNoBinary.transform[0].y == 2.5f);
	CHECK(loadedCompNoBinary.transform[0].z == 3.75f);
	CHECK(loadedCompNoBinary.transform[1].x == 4.0f);
	CHECK(loadedCompNoBinary.transform[1].y == 5.25f);
	CHECK(loadedCompNoBinary.transform[1].z == 6.5f);
	CHECK(loadedCompNoBinary.itemCounts[0] == 11);
	CHECK(loadedCompNoBinary.itemCounts[1] == 22);
	CHECK(loadedCompNoBinary.itemCounts[2] == 33);
	CHECK(loadedCompNoBinary.active[0] == true);
	CHECK(loadedCompNoBinary.active[1] == false);
	CHECK(std::string(loadedCompNoBinary.label) == "crate");
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

	ser::bin_stream buffer;
	in.m_w.set_serializer(buffer);
	in.m_w.save();

	// Load

	TestWorld twld;
	initComponents(wld);
	CHECK(wld.load(buffer));

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

TEST_CASE("Multithreading - Reset reusable handles") {
	auto& tp = mt::ThreadPool::get();

	constexpr uint32_t Iters = 1024;

	std::atomic_uint32_t stage = 0;
	std::atomic_uint32_t firstCnt = 0;
	std::atomic_uint32_t secondCnt = 0;
	std::atomic_bool ordered = true;

	mt::Job job1;
	job1.flags = mt::JobCreationFlags::ManualDelete;
	job1.func = [&]() {
		firstCnt.fetch_add(1, std::memory_order_relaxed);
		stage.store(1, std::memory_order_release);
	};

	mt::Job job2;
	job2.flags = mt::JobCreationFlags::ManualDelete;
	job2.func = [&]() {
		if (stage.load(std::memory_order_acquire) != 1)
			ordered.store(false, std::memory_order_relaxed);
		secondCnt.fetch_add(1, std::memory_order_relaxed);
	};

	auto handle1 = tp.add(job1);
	auto handle2 = tp.add(job2);
	mt::JobHandle handles[] = {handle1, handle2};

	tp.dep(handle1, handle2);
	GAIA_FOR(Iters) {
		stage.store(0, std::memory_order_relaxed);

		tp.submit(handle2);
		tp.submit(handle1);
		tp.reset(std::span(handles, 2));

		CHECK(ordered.load(std::memory_order_relaxed));
		tp.dep_refresh(handle1, handle2);
	}

	// Final run leaves handles in Done state so they can be deleted.
	stage.store(0, std::memory_order_relaxed);
	tp.submit(handle2);
	tp.submit(handle1);
	tp.wait(handle2);

	CHECK(ordered.load(std::memory_order_relaxed));
	CHECK(firstCnt.load(std::memory_order_relaxed) == Iters + 1);
	CHECK(secondCnt.load(std::memory_order_relaxed) == Iters + 1);

	tp.del(handle1);
	tp.del(handle2);
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

	uint32_t data1 = 0;
	uint32_t data2 = 0;
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

	auto sys1 = wld.system()
									.name("sys1")
									.all<SomeData1>()
									.all<SomeData2>() //
									.on_each([&](SomeData1, SomeData2) {
										++data1;
										++data2;
									});
	auto sys2 = wld.system()
									.name("sys2")
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

TEST_CASE("Multithreading - Reset handles missing TLS worker context") {
	auto& tp = mt::ThreadPool::get();

	// Keep this deterministic regardless of previous tests.
	tp.set_max_workers(2, 2);
	CHECK(tp.workers() == 1);

	// Simulate shutdown from a thread that has no TLS worker context bound.
	mt::detail::tl_workerCtx = nullptr;

	// set_max_workers() calls reset() internally, which used to dereference null TLS context.
	tp.set_max_workers(2, 2);
	CHECK(tp.workers() == 1);
}

TEST_CASE("Multithreading - Update auto-delete job") {
	auto& tp = mt::ThreadPool::get();
	tp.set_max_workers(0, 0);
	CHECK(tp.workers() == 0);

	std::atomic_uint32_t executed = 0;

	// update() drives main_thread_tick() and must not double-delete auto jobs.
	constexpr uint32_t Iters = 1024;
	GAIA_FOR(Iters) {
		mt::Job job;
		job.func = [&]() {
			executed.fetch_add(1, std::memory_order_relaxed);
		};

		const auto handle = tp.add(job);
		tp.submit(handle);
		tp.update();
	}

	CHECK(executed.load(std::memory_order_relaxed) == Iters);
}

TEST_CASE("Multithreading - Update auto-delete with workers") {
	auto& tp = mt::ThreadPool::get();
	tp.set_max_workers(2, 2);
	CHECK(tp.workers() == 1);

	constexpr uint32_t Jobs = 2048;
	std::atomic_uint32_t executed = 0;

	cnt::darr<mt::JobHandle> handles;
	handles.resize(Jobs);

	GAIA_FOR(Jobs) {
		mt::Job job;
		job.func = [&]() {
			executed.fetch_add(1, std::memory_order_relaxed);
		};
		handles[i] = tp.add(job);
	}

	mt::Job sync;
	sync.flags = mt::JobCreationFlags::ManualDelete;
	const auto syncHandle = tp.add(sync);
	tp.dep(std::span(handles.data(), handles.size()), syncHandle);

	tp.submit(syncHandle);
	tp.submit(std::span(handles.data(), handles.size()));

	// Keep ticking from the main thread while worker threads are active.
	// This exercises the same path that previously double-deleted auto jobs.
	while (executed.load(std::memory_order_relaxed) < Jobs) {
		tp.update();
	}

	tp.wait(syncHandle);
	tp.del(syncHandle);
	CHECK(executed.load(std::memory_order_relaxed) == Jobs);
}

TEST_CASE("Multithreading - Handle reuse mixed delete modes") {
	auto& tp = mt::ThreadPool::get();
	tp.set_max_workers(2, 2);
	CHECK(tp.workers() == 1);

	std::atomic_uint32_t autoCnt = 0;
	std::atomic_uint32_t manualCnt = 0;

	constexpr uint32_t Iters = 4096;
	GAIA_FOR(Iters) {
		mt::Job autoJob;
		autoJob.func = [&]() {
			autoCnt.fetch_add(1, std::memory_order_relaxed);
		};

		mt::Job manualJob;
		manualJob.flags = mt::JobCreationFlags::ManualDelete;
		manualJob.func = [&]() {
			manualCnt.fetch_add(1, std::memory_order_relaxed);
		};

		const auto autoHandle = tp.add(autoJob);
		const auto manualHandle = tp.add(manualJob);

		tp.submit(autoHandle);
		tp.submit(manualHandle);
		tp.wait(manualHandle);
		tp.del(manualHandle);

		if ((i & 7) == 0)
			tp.update();
	}

	// Drain any remaining auto jobs.
	while (autoCnt.load(std::memory_order_relaxed) < Iters)
		tp.update();

	CHECK(autoCnt.load(std::memory_order_relaxed) == Iters);
	CHECK(manualCnt.load(std::memory_order_relaxed) == Iters);
}

TEST_CASE("Parent - non-fragmenting relation keeps structural archetype cache stable") {
	TestWorld twld;

	const auto rootA = wld.add();
	const auto rootB = wld.add();
	const auto e1 = wld.add();
	const auto e2 = wld.add();

	wld.add<Position>(e1);
	wld.add<Position>(e2);

	auto q = wld.query().all<Position>();
	(void)q.count();
	auto& info = q.fetch();
	CHECK(info.cache_archetype_view().size() == 1);

	wld.parent(e1, rootA);
	wld.parent(e2, rootB);

	CHECK(wld.has(e1, ecs::Pair(ecs::Parent, rootA)));
	CHECK(wld.has(e2, ecs::Pair(ecs::Parent, rootB)));
	CHECK(q.count() == 2);
	CHECK(info.cache_archetype_view().size() == 1);
}

TEST_CASE("Parent - targets and sources use adjunct storage") {
	TestWorld twld;

	const auto root = wld.add();
	const auto child = wld.add();

	wld.parent(child, root);

	CHECK(wld.has(child, ecs::Pair(ecs::Parent, root)));
	CHECK(wld.target(child, ecs::Parent) == root);

	cnt::darray<ecs::Entity> targets;
	wld.targets(child, ecs::Parent, [&](ecs::Entity target) {
		targets.push_back(target);
	});
	CHECK(targets.size() == 1);
	CHECK(targets[0] == root);

	cnt::darray<ecs::Entity> sources;
	wld.sources(ecs::Parent, root, [&](ecs::Entity source) {
		sources.push_back(source);
	});
	CHECK(sources.size() == 1);
	CHECK(sources[0] == child);
}

TEST_CASE("Parent - deleting target deletes children through adjunct relation") {
	TestWorld twld;

	const auto root = wld.add();
	const auto child = wld.add();

	wld.add<Position>(child);
	wld.parent(child, root);

	wld.del(root);
	wld.update();

	CHECK(!wld.has(root));
	CHECK(!wld.has(child));
}

TEST_CASE("Parent - breadth-first traversal uses adjunct relation") {
	TestWorld twld;

	const auto root = wld.add();
	const auto child = wld.add();
	const auto leaf = wld.add();

	wld.parent(child, root);
	wld.parent(leaf, child);

	cnt::darray<ecs::Entity> traversed;
	wld.sources_bfs(ecs::Parent, root, [&](ecs::Entity entity) {
		traversed.push_back(entity);
	});

	CHECK(traversed.size() == 2);
	CHECK(traversed[0] == child);
	CHECK(traversed[1] == leaf);
}

TEST_CASE("Parent - direct query terms are evaluated as entity filters") {
	TestWorld twld;

	const auto rootA = wld.add();
	const auto rootB = wld.add();
	const auto eA = wld.add();
	const auto eB = wld.add();
	const auto eC = wld.add();

	wld.add<Position>(eA);
	wld.add<Position>(eB);
	wld.add<Position>(eC);
	wld.add<Scale>(eC);

	wld.parent(eA, rootA);
	wld.parent(eB, rootB);

	auto qAll = wld.query().all<Position>().all(ecs::Pair(ecs::Parent, rootA));
	CHECK(qAll.count() == 1);
	expect_exact_entities(qAll, {eA});
	CHECK(!qAll.empty());

	auto qNo = wld.query().all<Position>().no(ecs::Pair(ecs::Parent, rootA));
	CHECK(qNo.count() == 2);
	expect_exact_entities(qNo, {eB, eC});

	auto qOr = wld.query().or_(ecs::Pair(ecs::Parent, rootA)).or_<Scale>();
	CHECK(qOr.count() == 2);
	expect_exact_entities(qOr, {eA, eC});
}

TEST_CASE("Sparse DontFragment component uses adjunct storage") {
	TestWorld twld;

	const auto& compItem = wld.add<PositionSparse>();
	wld.add(compItem.entity, ecs::DontFragment);

	const auto e = wld.add();
	const auto* pArchetypeBefore = wld.fetch(e).pArchetype;

	wld.add<PositionSparse>(e);
	CHECK(wld.has<PositionSparse>(e));
	CHECK(wld.has(e, compItem.entity));
	CHECK(wld.fetch(e).pArchetype == pArchetypeBefore);

	auto& pos = wld.set<PositionSparse>(e);
	pos = {1.0f, 2.0f, 3.0f};

	const auto& posConst = wld.get<PositionSparse>(e);
	CHECK(posConst.x == doctest::Approx(1.0f));
	CHECK(posConst.y == doctest::Approx(2.0f));
	CHECK(posConst.z == doctest::Approx(3.0f));

	wld.del<PositionSparse>(e);
	CHECK_FALSE(wld.has<PositionSparse>(e));
	CHECK_FALSE(wld.has(e, compItem.entity));
	CHECK(wld.fetch(e).pArchetype == pArchetypeBefore);
}

TEST_CASE("Sparse DontFragment component supports runtime object add with value") {
	TestWorld twld;

	const auto& compItem = wld.add<PositionSparse>();
	wld.add(compItem.entity, ecs::DontFragment);

	const auto e = wld.add();
	wld.add(e, compItem.entity, PositionSparse{4.0f, 5.0f, 6.0f});

	CHECK(wld.has<PositionSparse>(e));
	CHECK(wld.has(e, compItem.entity));

	const auto& pos = wld.get<PositionSparse>(e);
	CHECK(pos.x == doctest::Approx(4.0f));
	CHECK(pos.y == doctest::Approx(5.0f));
	CHECK(pos.z == doctest::Approx(6.0f));
}

TEST_CASE("Sparse DontFragment component direct query terms are evaluated as entity filters") {
	TestWorld twld;

	const auto& compItem = wld.add<PositionSparse>();
	wld.add(compItem.entity, ecs::DontFragment);

	const auto eA = wld.add();
	const auto eB = wld.add();
	const auto eC = wld.add();

	wld.add<Position>(eA);
	wld.add<Position>(eB);
	wld.add<Position>(eC);
	wld.add<Scale>(eC);

	wld.add<PositionSparse>(eA);

	auto qAll = wld.query().all<Position>().all<PositionSparse>();
	CHECK(qAll.count() == 1);
	expect_exact_entities(qAll, {eA});

	auto qNo = wld.query().all<Position>().no<PositionSparse>();
	CHECK(qNo.count() == 2);
	expect_exact_entities(qNo, {eB, eC});

	auto qOr = wld.query().or_<PositionSparse>().or_<Scale>();
	CHECK(qOr.count() == 2);
	expect_exact_entities(qOr, {eA, eC});
}

TEST_CASE("Sparse DontFragment runtime-registered component supports typed object access") {
	TestWorld twld;

	const auto& runtimeComp = wld.add(
			"Runtime_Sparse_Position", (uint32_t)sizeof(Position), ecs::DataStorageType::Sparse, (uint32_t)alignof(Position));
	wld.add(runtimeComp.entity, ecs::DontFragment);

	const auto e = wld.add();
	const auto* pArchetypeBefore = wld.fetch(e).pArchetype;

	wld.add(e, runtimeComp.entity, Position{7.0f, 8.0f, 9.0f});
	CHECK(wld.has(e, runtimeComp.entity));
	CHECK(wld.fetch(e).pArchetype == pArchetypeBefore);

	auto& posMut = wld.set<Position>(e, runtimeComp.entity);
	posMut = {10.0f, 11.0f, 12.0f};

	const auto& pos = wld.get<Position>(e, runtimeComp.entity);
	CHECK(pos.x == doctest::Approx(10.0f));
	CHECK(pos.y == doctest::Approx(11.0f));
	CHECK(pos.z == doctest::Approx(12.0f));

	wld.del(e, runtimeComp.entity);
	CHECK_FALSE(wld.has(e, runtimeComp.entity));
	CHECK(wld.fetch(e).pArchetype == pArchetypeBefore);
}

TEST_CASE("Sparse DontFragment runtime-registered component is removed on entity delete") {
	TestWorld twld;

	const auto& runtimeComp = wld.add(
			"Runtime_Sparse_Position_Delete", (uint32_t)sizeof(Position), ecs::DataStorageType::Sparse,
			(uint32_t)alignof(Position));
	wld.add(runtimeComp.entity, ecs::DontFragment);

	const auto e = wld.add();
	wld.add(e, runtimeComp.entity, Position{1.0f, 2.0f, 3.0f});
	CHECK(wld.has(e, runtimeComp.entity));

	wld.del(e);
	wld.update();

	CHECK_FALSE(wld.has(e));
}

TEST_CASE("EntityContainer cached entity slot stays valid across row swap and archetype move") {
	TestWorld twld;

	const auto targetA = wld.add();
	const auto targetB = wld.add();
	const auto sourceA = wld.add();
	const auto sourceB = wld.add();

	wld.child(sourceA, targetA);
	wld.child(sourceB, targetB);

	const auto* pTargetBBefore = wld.fetch(targetB).pEntity;
	CHECK(pTargetBBefore != nullptr);
	if (pTargetBBefore != nullptr)
		CHECK(*pTargetBBefore == targetB);
	CHECK(wld.target(sourceB, ecs::ChildOf) == targetB);

	wld.del(targetA);

	const auto& targetBAfterDelete = wld.fetch(targetB);
	CHECK(targetBAfterDelete.pEntity != nullptr);
	if (targetBAfterDelete.pEntity != nullptr)
		CHECK(*targetBAfterDelete.pEntity == targetB);
	CHECK(wld.target(sourceB, ecs::ChildOf) == targetB);

	wld.add<Position>(targetB, {1.0f, 2.0f, 3.0f});

	const auto& targetBAfterMove = wld.fetch(targetB);
	CHECK(targetBAfterMove.pEntity != nullptr);
	if (targetBAfterMove.pEntity != nullptr)
		CHECK(*targetBAfterMove.pEntity == targetB);
	CHECK(wld.target(sourceB, ecs::ChildOf) == targetB);
}

int main(int argc, char** argv) {
	// Use custom logging. Just for code coverage.
	util::set_log_func(util::detail::log_cached);
	util::set_log_flush_func(util::detail::log_flush_cached);

	doctest::Context ctx(argc, argv);
	ctx.setOption("success", false); // suppress successful checks
	return ctx.run();
}
