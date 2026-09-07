#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#include <doctest/doctest.h>

#if defined(GAIA_TEST_SINGLE_HEADER) && GAIA_TEST_SINGLE_HEADER
	#include <gaia.h>
#else
	#include "gaia/cnt/darray.h"
	#include "gaia/cnt/darray_ext.h"
	#include "gaia/cnt/darray_ext_soa.h"
	#include "gaia/cnt/darray_soa.h"
	#include "gaia/cnt/sarray_ext.h"
	#include "gaia/cnt/sarray_ext_soa.h"
	#include "gaia/cnt/sparse_storage.h"
#endif

using namespace gaia;

namespace {
	//! Owning value that counts every live object, including moved-from objects.
	struct ArrayOwner {
		inline static int live = 0;
		int* value;

		ArrayOwner(int v = 42): value(new int(v)) {
			++live;
		}
		ArrayOwner(const ArrayOwner& other): ArrayOwner(*other.value) {}
		ArrayOwner(ArrayOwner&& other) noexcept: value(other.value) {
			other.value = nullptr;
			++live;
		}
		ArrayOwner& operator=(const ArrayOwner& other) {
			if (this != &other) {
				delete value;
				value = new int(*other.value);
			}
			return *this;
		}
		ArrayOwner& operator=(ArrayOwner&& other) noexcept {
			if (this != &other) {
				delete value;
				value = other.value;
				other.value = nullptr;
			}
			return *this;
		}
		~ArrayOwner() {
			delete value;
			--live;
		}
	};

	//! Exercises capacity changes without constructing spare objects.
	template <typename Array>
	void array_capacity_lifetime() {
		REQUIRE(ArrayOwner::live == 0);
		{
			Array values;
			values.reserve(8);
			CHECK(values.empty());
			CHECK(ArrayOwner::live == 0);
			values.resize(2);
			CHECK(ArrayOwner::live == 2);
			CHECK(*values[0].value == 42);
			values[0] = ArrayOwner(7);
			values.reserve(16);
			CHECK(ArrayOwner::live == 2);
			CHECK(*values[0].value == 7);
			values.resize(20);
			CHECK(ArrayOwner::live == 20);
			CHECK(*values.back().value == 42);
			values.resize(1);
			CHECK(ArrayOwner::live == 1);
			values.clear();
			CHECK(ArrayOwner::live == 0);
		}
		CHECK(ArrayOwner::live == 0);
		{
			Array values(6);
			CHECK(ArrayOwner::live == 6);
			CHECK(*values.back().value == 42);
		}
		CHECK(ArrayOwner::live == 0);
		{
			Array values;
			for (int i = 0; i < 12; ++i)
				values.emplace_back(i);
			CHECK(ArrayOwner::live == 12);
			for (int i = 0; i < 12; ++i)
				CHECK(*values[(uint32_t)i].value == i);
			values.insert(values.begin() + 1, ArrayOwner(50));
			CHECK(ArrayOwner::live == 13);
			CHECK(*values[1].value == 50);
			CHECK(*values[2].value == 1);
			Array copy(values);
			CHECK(ArrayOwner::live == 26);
			copy = values;
			CHECK(ArrayOwner::live == 26);
		}
		CHECK(ArrayOwner::live == 0);
	}
} // namespace

TEST_CASE("Array lifetime - capacity contains only live size") {
	SUBCASE("heap") {
		array_capacity_lifetime<cnt::darray<ArrayOwner>>();
	}
	SUBCASE("inline and heap") {
		array_capacity_lifetime<cnt::darray_ext<ArrayOwner, 4>>();
	}
}

TEST_CASE("Array lifetime - inline relocation and destruction") {
	using Array = cnt::darray_ext<ArrayOwner, 4>;
	REQUIRE(ArrayOwner::live == 0);
	{
		Array source(2);
		Array moved(GAIA_MOV(source));
		CHECK(source.empty());
		CHECK(ArrayOwner::live == 2);
		source.emplace_back(8);
		Array dest(3);
		dest = GAIA_MOV(moved);
		CHECK(ArrayOwner::live == 3);
		CHECK(moved.empty());
		moved.resize(2);
		CHECK(ArrayOwner::live == 5);
		moved.resize(7);
		CHECK(ArrayOwner::live == 10);
		CHECK(*moved.back().value == 42);
		dest = GAIA_MOV(moved);
		CHECK(ArrayOwner::live == 8);
		CHECK(*dest.back().value == 42);
	}
	CHECK(ArrayOwner::live == 0);
}

TEST_CASE("Array lifetime - reserve supports non-default-constructible values") {
	struct Value {
		int value;
		Value() = delete;
		explicit Value(int v): value(v) {}
		Value(Value&& other) noexcept: value(other.value) {}
		Value& operator=(Value&&) = default;
	};
	cnt::darray<Value> values;
	values.reserve(1);
	values.emplace_back(17);
	values.reserve(4);
	CHECK(values[0].value == 17);
	while (values.size() < values.capacity())
		values.emplace_back(17);
	values.emplace_back(values[0].value);
	CHECK(values.back().value == 17);
}

TEST_CASE("Array lifetime - sparse pages destroy only occupied slots") {
	REQUIRE(ArrayOwner::live == 0);
	{
		cnt::detail::sparse_page<ArrayOwner, 16, mem::DefaultAllocatorAdaptor> page;
		page.add();
		page.set_id(9) = 0;
		page.add_data(9, ArrayOwner(19));
		CHECK(ArrayOwner::live == 1);
		CHECK(*page.get_data(9).value == 19);
	}
	CHECK(ArrayOwner::live == 0);
}

namespace {
	//! Forces retention to use ownership-transferring moves.
	struct ArrayMoveOnly: ArrayOwner {
		using ArrayOwner::ArrayOwner;
		ArrayMoveOnly(ArrayMoveOnly&&) = default;
		ArrayMoveOnly& operator=(ArrayMoveOnly&&) = default;
	};

	//! Checks compaction into rejected slots, tail destruction, and predicate order.
	template <typename Array>
	void array_retain_lifetime() {
		REQUIRE(ArrayOwner::live == 0);
		{
			Array values;
			CHECK(values.retain([](const auto&) {
				return false;
			}) == 0);
			for (int i = 0; i < 6; ++i)
				values.emplace_back(i);
			int visited = 0;
			CHECK(values.retain([&](const auto& value) {
				CHECK(*value.value == visited++);
				return *value.value % 2 != 0;
			}) == 3);
			CHECK(visited == 6);
			CHECK(ArrayOwner::live == 3);
			for (int i = 0; i < 3; ++i)
				CHECK(*values[(uint32_t)i].value == 2 * i + 1);
			CHECK(values.retain([](const auto&) {
				return true;
			}) == 3);
			CHECK(ArrayOwner::live == 3);
			CHECK(values.retain([](const auto&) {
				return false;
			}) == 0);
			CHECK(ArrayOwner::live == 0);
			values.emplace_back(19);
		}
		CHECK(ArrayOwner::live == 0);
	}

	//! Trivial fields used to check SoA storage variants.
	struct ArraySoA {
		GAIA_LAYOUT(SoA);
		int x;
		float y;
	};

	template <typename Array>
	void array_retain_soa() {
		Array values;
		for (int i = 0; i < 6; ++i)
			values.push_back({i, (float)i});
		CHECK(values.retain([](ArraySoA v) {
			return v.x % 2 != 0;
		}) == 3);
		for (int i = 0; i < 3; ++i) {
			const ArraySoA value = values[(uint32_t)i];
			CHECK(value.x == 2 * i + 1);
			CHECK(value.y == (float)(2 * i + 1));
		}
		CHECK(values.retain([](ArraySoA) {
			return false;
		}) == 0);
	}
} // namespace

TEST_CASE("Array lifetime - retain compacts live objects") {
	SUBCASE("heap") {
		array_retain_lifetime<cnt::darray<ArrayMoveOnly>>();
	}
	SUBCASE("extended inline") {
		array_retain_lifetime<cnt::darray_ext<ArrayMoveOnly, 8>>();
	}
	SUBCASE("extended heap") {
		array_retain_lifetime<cnt::darray_ext<ArrayMoveOnly, 2>>();
	}
	SUBCASE("fixed capacity") {
		array_retain_lifetime<cnt::sarray_ext<ArrayMoveOnly, 8>>();
	}
	SUBCASE("SoA heap") {
		array_retain_soa<cnt::darray_soa<ArraySoA>>();
	}
	SUBCASE("SoA extended inline") {
		array_retain_soa<cnt::darray_ext_soa<ArraySoA, 8>>();
	}
	SUBCASE("SoA extended heap") {
		array_retain_soa<cnt::darray_ext_soa<ArraySoA, 2>>();
	}
	SUBCASE("SoA fixed capacity") {
		array_retain_soa<cnt::sarray_ext_soa<ArraySoA, 8>>();
	}
}

namespace {
	//! Runs aliasing paths with and without heap relocation.
	template <typename Array>
	void array_alias_inputs() {
		{
			Array values;
			values.reserve(4);
			while (values.size() < values.capacity())
				values.emplace_back(17);
			values.push_back(values[0]);
			CHECK(*values.back().value == 17);
			values.resize(values.capacity() + 3, values[0]);
			CHECK(*values.back().value == 17);
			values.resize(1, values.back());
			CHECK(*values[0].value == 17);
			values.clear();
			while (values.size() < values.capacity())
				values.emplace_back(23);
			values.emplace_back(*values[0].value);
			CHECK(*values.back().value == 23);
			values.clear();
			while (values.size() < values.capacity())
				values.emplace_back(29);
			values.push_back(GAIA_MOV(values[0]));
			CHECK(*values.back().value == 29);
			CHECK(values[0].value == nullptr);
		}
		CHECK(ArrayOwner::live == 0);
	}

	template <typename Array>
	void array_alias_insert(bool fillCapacity) {
		{
			Array values;
			values.emplace_back(10);
			values.emplace_back(20);
			values.emplace_back(30);
			if (fillCapacity)
				while (values.size() < values.capacity())
					values.emplace_back(40);
			values.insert(values.begin(), values[2]);
			CHECK(*values[0].value == 30);
			CHECK(*values[1].value == 10);
			CHECK(*values[2].value == 20);
			CHECK(*values[3].value == 30);
			values.insert(values.begin() + 1, GAIA_MOV(values[3]));
			CHECK(*values[1].value == 30);
			CHECK(*values[2].value == 10);
			CHECK(*values[3].value == 20);
			CHECK(values[4].value == nullptr);
		}
		CHECK(ArrayOwner::live == 0);
	}
} // namespace

TEST_CASE("Array lifetime - aliased append and resize") {
	SUBCASE("heap") {
		array_alias_inputs<cnt::darray<ArrayOwner>>();
	}
	SUBCASE("extended") {
		array_alias_inputs<cnt::darray_ext<ArrayOwner, 4>>();
	}
}

TEST_CASE("Array lifetime - aliased insertion") {
	SUBCASE("heap growth") {
		array_alias_insert<cnt::darray<ArrayOwner>>(true);
	}
	SUBCASE("heap spare capacity") {
		array_alias_insert<cnt::darray<ArrayOwner>>(false);
	}
	SUBCASE("inline to heap") {
		array_alias_insert<cnt::darray_ext<ArrayOwner, 4>>(true);
	}
	SUBCASE("inline spare capacity") {
		array_alias_insert<cnt::darray_ext<ArrayOwner, 8>>(false);
	}
	SUBCASE("fixed capacity") {
		array_alias_insert<cnt::sarray_ext<ArrayOwner, 8>>(false);
	}
}

TEST_CASE("Array lifetime - SoA emplacement consumes aliased proxies before growth") {
	auto run = [](auto& values) {
		const ArraySoA source{37, 4.0f};
		values.push_back(source);
		while (values.size() < values.capacity())
			values.push_back(source);
		values.emplace_back(values[0]);
		const ArraySoA result = values[values.size() - 1];
		CHECK(result.x == 37);
		CHECK(result.y == 4.0f);
	};
	SUBCASE("heap") {
		cnt::darray_soa<ArraySoA> values;
		run(values);
	}
	SUBCASE("inline to heap") {
		cnt::darray_ext_soa<ArraySoA, 4> values;
		run(values);
	}
}

namespace {
	template <typename Array>
	void array_erase_overlap() {
		const uint32_t ranges[][2] = {{1, 2}, {0, 3}, {3, 8}, {0, 8}};
		for (const auto& range: ranges) {
			Array values;
			for (int i = 0; i < 8; ++i)
				values.push_back(i);
			const auto first = range[0];
			const auto last = range[1];
			const auto it = values.erase(values.begin() + first, values.begin() + last);
			CHECK(values.size() == 8 - (last - first));
			CHECK(it == values.begin() + first);
			for (uint32_t i = 0; i < values.size(); ++i)
				CHECK(values[i] == (int)(i < first ? i : i + last - first));
		}
	}
} // namespace

TEST_CASE("Array lifetime - overlapping range erase") {
	SUBCASE("heap") {
		array_erase_overlap<cnt::darray<int>>();
	}
	SUBCASE("inline") {
		array_erase_overlap<cnt::darray_ext<int, 16>>();
	}
	SUBCASE("extended heap") {
		array_erase_overlap<cnt::darray_ext<int, 2>>();
	}
	SUBCASE("fixed capacity") {
		array_erase_overlap<cnt::sarray_ext<int, 16>>();
	}
	SUBCASE("owning values") {
		REQUIRE(ArrayOwner::live == 0);
		{
			cnt::darray<ArrayOwner> values;
			for (int i = 0; i < 8; ++i)
				values.emplace_back(i);
			values.erase(values.begin() + 1, values.begin() + 2);
			CHECK(ArrayOwner::live == 7);
			for (uint32_t i = 0; i < values.size(); ++i)
				CHECK(*values[i].value == (int)(i == 0 ? 0 : i + 1));
		}
		CHECK(ArrayOwner::live == 0);
	}
}

namespace {
	//! Checks compaction, heap-to-inline ownership, empty release, and reuse.
	template <typename Array>
	void array_shrink_storage(uint32_t inlineCapacity) {
		using T = typename Array::value_type;
		Array values;
		auto* initialData = values.data();
		auto append = [](Array& arr, int n) {
			if constexpr (std::is_same_v<T, ArrayOwner> || std::is_same_v<T, int>)
				arr.emplace_back(n);
			else
				arr.push_back({n, (float)n});
		};
		auto verify = [](const Array& arr) {
			for (uint32_t i = 0; i < arr.size(); ++i) {
				if constexpr (std::is_same_v<T, ArrayOwner>)
					CHECK(*arr[i].value == (int)i);
				else if constexpr (std::is_same_v<T, int>)
					CHECK(arr[i] == (int)i);
				else {
					const T value = arr[i];
					CHECK(value.x == (int)i);
					CHECK(value.y == (float)i);
				}
			}
		};
		values.shrink_to_fit();
		CHECK(values.capacity() == inlineCapacity);
		values.reserve(20);
		for (int i = 0; i < 8; ++i)
			append(values, i);
		values.shrink_to_fit();
		CHECK(values.capacity() == 8);
		verify(values);
		auto* compactData = values.data();
		values.shrink_to_fit();
		CHECK(values.data() == compactData);
		values.resize(4);
		values.shrink_to_fit();
		CHECK(values.capacity() == 4);
		verify(values);
		if (inlineCapacity != 0)
			CHECK(values.data() == initialData);
		append(values, 4);
		verify(values);
		values.reserve(24);
		values.resize(2);
		values.shrink_to_fit();
		CHECK(values.capacity() == (inlineCapacity != 0 ? inlineCapacity : 2));
		verify(values);
		Array moved(GAIA_MOV(values));
		verify(moved);
		CHECK(values.empty());
		append(values, 0);
		values = GAIA_MOV(moved);
		verify(values);
		values.clear();
		values.shrink_to_fit();
		CHECK(values.capacity() == inlineCapacity);
		if (inlineCapacity == 0)
			CHECK(values.data() == nullptr);
		append(values, 0);
		verify(values);
		values.reserve(16);
		values.clear();
		values.shrink_to_fit();
		CHECK(values.capacity() == inlineCapacity);
	}
} // namespace

TEST_CASE("Array lifetime - shrink releases capacity and preserves ownership") {
	REQUIRE(ArrayOwner::live == 0);
	SUBCASE("AoS heap") {
		array_shrink_storage<cnt::darray<int>>(0);
	}
	SUBCASE("AoS extended") {
		array_shrink_storage<cnt::darray_ext<int, 4>>(4);
	}
	SUBCASE("owning AoS heap") {
		array_shrink_storage<cnt::darray<ArrayOwner>>(0);
	}
	SUBCASE("owning AoS extended") {
		array_shrink_storage<cnt::darray_ext<ArrayOwner, 4>>(4);
	}
	SUBCASE("SoA heap") {
		array_shrink_storage<cnt::darray_soa<ArraySoA>>(0);
	}
	SUBCASE("SoA extended") {
		array_shrink_storage<cnt::darray_ext_soa<ArraySoA, 4>>(4);
	}
	CHECK(ArrayOwner::live == 0);
}
