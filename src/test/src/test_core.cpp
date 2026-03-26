#include "test_common.h"

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

TEST_CASE("Containers - sparse_storage empty page reuse and cross-page delete remap") {
	SUBCASE("trivial payload") {
		cnt::sparse_storage<SparseTestItem> arr;

		arr.add(SparseTestItem{1, 11});
		arr.del(1);
		CHECK_FALSE(arr.has(1));

		// Reusing an id on a page that just became empty must not touch freed page storage.
		arr.add(SparseTestItem{1, 22});
		CHECK(arr.has(1));
		CHECK(arr[1].data == 22);

		arr.add(SparseTestItem{5000, 33});
		arr.del(1);
		CHECK(arr.has(5000));
		CHECK(arr[5000].data == 33);
	}

	SUBCASE("tag payload") {
		cnt::sparse_storage<Empty> arr;

		arr.add(1);
		arr.del(1);
		CHECK_FALSE(arr.has(1));

		arr.add(1);
		CHECK(arr.has(1));

		arr.add(5000);
		arr.del(1);
		CHECK(arr.has(5000));
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

		GAIA_NODISCARD static EntityContainer create(uint32_t index, uint32_t generation, void*) {
			return EntityContainer(index, generation);
		}

		GAIA_NODISCARD static ecs::Entity handle(const EntityContainer& item) {
			return ecs::Entity(item.idx, item.data.gen);
		}
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

namespace {
	struct ListHandle {
		static constexpr uint32_t IdMask = uint32_t(-1);

		uint32_t m_id = IdMask;
		uint32_t m_gen = 0;

		ListHandle() = default;
		ListHandle(uint32_t id, uint32_t gen): m_id(id), m_gen(gen) {}

		GAIA_NODISCARD uint32_t id() const noexcept {
			return m_id;
		}
		GAIA_NODISCARD uint32_t gen() const noexcept {
			return m_gen;
		}

		GAIA_NODISCARD bool operator==(const ListHandle& other) const noexcept {
			return m_id == other.m_id && m_gen == other.m_gen;
		}
		GAIA_NODISCARD bool operator!=(const ListHandle& other) const noexcept {
			return !(*this == other);
		}
	};

	struct ListItemCtx {
		uint32_t value = 0;
	};

	struct ListItem {
		uint32_t idx = 0;
		struct ItemData {
			uint32_t gen = 0;
		} data;
		uint32_t value = 0;

		ListItem() = default;
		ListItem(uint32_t index, uint32_t generation): idx(index) {
			data.gen = generation;
		}

		GAIA_NODISCARD static ListItem create(uint32_t index, uint32_t generation, void* pCtx) {
			ListItem item(index, generation);
			if (pCtx != nullptr)
				item.value = ((ListItemCtx*)pCtx)->value;
			return item;
		}

		GAIA_NODISCARD static ListHandle handle(const ListItem& item) {
			return ListHandle(item.idx, item.data.gen);
		}
	};
} // namespace

TEST_CASE("ilist - slot reuse bumps generation") {
	cnt::ilist<ListItem, ListHandle> list;
	list.reserve(8);

	ListItemCtx ctx0{10};
	ListItemCtx ctx1{20};
	ListItemCtx ctx2{30};
	const auto h0 = list.alloc(&ctx0);
	const auto h1 = list.alloc(&ctx1);
	const auto h2 = list.alloc(&ctx2);

	CHECK(h0 == ListHandle(0, 0));
	CHECK(h1 == ListHandle(1, 0));
	CHECK(h2 == ListHandle(2, 0));
	CHECK(list.item_count() == 3);

	const auto& dead1 = list.free(h1);
	CHECK(dead1.data.gen == 1);
	CHECK(dead1.idx == ListHandle::IdMask);
	CHECK(list.get_free_items() == 1);
	list.validate();

	const auto& dead2 = list.free(h2);
	CHECK(dead2.data.gen == 1);
	CHECK(dead2.idx == 1);
	CHECK(list.get_next_free_item() == 2);
	CHECK(list.get_free_items() == 2);
	list.validate();

	ListItemCtx ctx3{40};
	const auto h3 = list.alloc(&ctx3);
	CHECK(h3 == ListHandle(2, 1));
	CHECK(list[h3.id()].value == 40);

	ListItemCtx ctx4{50};
	const auto h4 = list.alloc(&ctx4);
	CHECK(h4 == ListHandle(1, 1));
	CHECK(list[h4.id()].value == 50);
	CHECK(list.get_free_items() == 0);
	CHECK(list.item_count() == 3);
	list.validate();
}

TEST_CASE("paged_ilist - restore preserves free list metadata") {
	cnt::paged_ilist<ListItem, ListHandle> list;

	list.add_free(ListHandle(9, 5), ListHandle::IdMask);
	list.add_free(ListHandle(4, 2), 9);
	list.m_nextFreeIdx = 4;
	list.m_freeItems = 2;

	auto live = ListItem::create(11, 3, nullptr);
	live.value = 77;
	list.add_live(GAIA_MOV(live));

	CHECK(list.size() == 12);
	CHECK_FALSE(list.has(4));
	CHECK_FALSE(list.has(9));
	CHECK(list.has(11));
	CHECK(list.handle(4) == ListHandle(4, 2));
	CHECK(list.handle(9) == ListHandle(9, 5));
	CHECK(list.next_free(4) == 9);
	CHECK(list.next_free(9) == ListHandle::IdMask);
	CHECK(list[11].value == 77);
	list.validate();

	ListItemCtx ctx0{100};
	const auto h0 = list.alloc(&ctx0);
	CHECK(h0 == ListHandle(4, 2));
	CHECK(list[4].value == 100);

	ListItemCtx ctx1{200};
	const auto h1 = list.alloc(&ctx1);
	CHECK(h1 == ListHandle(9, 5));
	CHECK(list[9].value == 200);
	CHECK(list.get_free_items() == 0);
	list.validate();
}

TEST_CASE("paged_ilist - iterates only live items across pages") {
	cnt::paged_ilist<ListItem, ListHandle> list;
	cnt::darray<ListHandle> handles;
	constexpr uint32_t ItemCount = 600;
	handles.reserve(ItemCount);

	GAIA_FOR(ItemCount) {
		ListItemCtx ctx{i + 1};
		handles.push_back(list.alloc(&ctx));
	}

	uint32_t expectedLive = ItemCount;
	uint64_t expectedSum = (uint64_t)ItemCount * (ItemCount + 1) / 2;

	GAIA_FOR(ItemCount) {
		if ((i % 3) != 0)
			continue;

		list.free(handles[i]);
		--expectedLive;
		expectedSum -= (uint64_t)(i + 1);
		CHECK_FALSE(list.has(handles[i]));
		CHECK(list.try_get(handles[i].id()) == nullptr);
	}
	list.validate();

	uint32_t actualLive = 0;
	uint64_t actualSum = 0;
	for (const auto& item: list) {
		++actualLive;
		actualSum += item.value;
	}

	CHECK(list.item_count() == expectedLive);
	CHECK(actualLive == expectedLive);
	CHECK(actualSum == expectedSum);
	CHECK(list.has(handles[1]));
	CHECK(list.try_get(handles[1].id()) != nullptr);
}
