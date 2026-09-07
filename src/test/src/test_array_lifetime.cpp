#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#include <doctest/doctest.h>

#include "gaia/cnt/darray.h"
#include "gaia/cnt/darray_ext.h"
#include "gaia/cnt/sparse_storage.h"

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
