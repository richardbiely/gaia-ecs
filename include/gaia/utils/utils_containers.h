#pragma once
#include <algorithm>
#include <inttypes.h>

namespace gaia {
	namespace utils {
		constexpr uint32_t BadIndex = uint32_t(-1);

		template <class C, class Func>
		constexpr auto for_each(const C& arr, Func func) {
			return std::for_each(arr.begin(), arr.end(), func);
		}

		template <class C>
		constexpr auto find(const C& arr, typename C::const_reference item) {
			return std::find(arr.begin(), arr.end(), item);
		}

		template <class UnaryPredicate, class C>
		constexpr auto find_if(const C& arr, UnaryPredicate predicate) {
			return std::find_if(arr.begin(), arr.end(), predicate);
		}

		template <class C>
		constexpr uint32_t get_index(const C& arr, typename C::const_reference item) {
			const auto it = find(arr, item);
			if (it == arr.end())
				return (uint32_t)BadIndex;

			return (uint32_t)std::distance(arr.begin(), it);
		}

		template <class C>
		constexpr uint32_t get_index_unsafe(const C& arr, typename C::const_reference item) {
			return (uint32_t)std::distance(arr.begin(), find(arr, item));
		}

		template <class UnaryPredicate, class C>
		constexpr uint32_t get_index_if(const C& arr, UnaryPredicate predicate) {
			const auto it = find_if(arr, predicate);
			if (it == arr.end())
				return (uint32_t)BadIndex;

			return (uint32_t)std::distance(arr.begin(), it);
		}

		template <class UnaryPredicate, class C>
		constexpr uint32_t get_index_if_unsafe(const C& arr, UnaryPredicate predicate) {
			return (uint32_t)std::distance(arr.begin(), find_if(arr, predicate));
		}

		template <class C>
		constexpr bool has(const C& arr, typename C::const_reference item) {
			const auto it = find(arr, item);
			return it != arr.end();
		}

		template <class UnaryPredicate, class C>
		constexpr bool has_if(const C& arr, UnaryPredicate predicate) {
			const auto it = find_if(arr, predicate);
			return it != arr.end();
		}

		template <class C>
		void erase_fast(C& arr, uint32_t idx) {
			if (idx >= arr.size())
				return;

			if (idx + 1 != arr.size())
				std::swap(arr[idx], arr[arr.size() - 1]);

			arr.pop_back();
		}
	} // namespace utils
} // namespace gaia