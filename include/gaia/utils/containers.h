#pragma once
#include <cstddef>
#include <cstdint>

#include "iterator.h"
#include "utility.h"

namespace gaia {
	namespace utils {
		template <typename C, typename Func>
		constexpr auto for_each(const C& arr, Func func) {
			return utils::for_each(arr.begin(), arr.end(), func);
		}

		template <typename C>
		constexpr auto get_index(const C& arr, typename C::const_reference item) {
			const auto it = find(arr, item);
			if (it == arr.end())
				return (std::ptrdiff_t)BadIndex;

			return GAIA_UTIL::distance(arr.begin(), it);
		}

		template <typename C>
		constexpr auto get_index_unsafe(const C& arr, typename C::const_reference item) {
			return GAIA_UTIL::distance(arr.begin(), find(arr, item));
		}

		template <typename UnaryPredicate, typename C>
		constexpr auto get_index_if(const C& arr, UnaryPredicate predicate) {
			const auto it = find_if(arr, predicate);
			if (it == arr.end())
				return BadIndex;

			return GAIA_UTIL::distance(arr.begin(), it);
		}

		template <typename UnaryPredicate, typename C>
		constexpr auto get_index_if_unsafe(const C& arr, UnaryPredicate predicate) {
			return GAIA_UTIL::distance(arr.begin(), find_if(arr, predicate));
		}

		template <typename C>
		void erase_fast(C& arr, size_t idx) {
			if (idx >= arr.size())
				return;

			if (idx + 1 != arr.size())
				utils::swap(arr[idx], arr[arr.size() - 1]);

			arr.pop_back();
		}
	} // namespace utils
} // namespace gaia
