#pragma once
#include <algorithm>
#include <inttypes.h>

namespace gaia {
  namespace utils {
    constexpr uint32_t BadIndex = uint32_t(-1);

    template <template <typename...> class C, typename... Args, class Func>
    constexpr auto for_each(const C<Args...>& arr, Func func) {
      return std::for_each(arr.begin(), arr.end(), func);
    }

    template <template <typename...> class C, typename... Args>
    constexpr auto
    find(const C<Args...>& arr, typename C<Args...>::const_reference item) {
      return std::find(arr.begin(), arr.end(), item);
    }

    template <
        class UnaryPredicate, template <typename...> class C, typename... Args>
    constexpr auto find_if(const C<Args...>& arr, UnaryPredicate predicate) {
      return std::find_if(arr.begin(), arr.end(), predicate);
    }

    template <template <typename...> class C, typename... Args>
    constexpr uint32_t get_index(
        const C<Args...>& arr, typename C<Args...>::const_reference item) {
      const auto it = find(arr, item);
      if (it == arr.end())
        return (uint32_t)BadIndex;

      return (uint32_t)std::distance(arr.begin(), it);
    }

    template <
        class UnaryPredicate, template <typename...> class C, typename... Args>
    constexpr uint32_t
    get_index_if(const C<Args...>& arr, UnaryPredicate predicate) {
      const auto it = find_if(arr, predicate);
      if (it == arr.end())
        return (uint32_t)BadIndex;

      return (uint32_t)std::distance(arr.begin(), it);
    }

    template <template <typename...> class C, typename... Args>
    constexpr bool
    has(const C<Args...>& arr, typename C<Args...>::const_reference item) {
      const auto it = find(arr, item);
      return it != arr.end();
    }

    template <
        class UnaryPredicate, template <typename...> class C, typename... Args>
    constexpr bool has_if(const C<Args...>& arr, UnaryPredicate predicate) {
      const auto it = find_if(arr, predicate);
      return it != arr.end();
    }

    template <template <typename...> class C, typename... Args>
    void erase_fast(C<Args...>& arr, uint32_t idx) {
      if (idx >= arr.size())
        return;

      if (idx + 1 != arr.size())
        std::swap(arr[idx], arr.back());

      arr.pop_back();
    }

    /*!
    Fill memory with 32 bit integer value
    \param dest pointer to destination
    \param num number of items to be filled (not data size!)
    \param data 32bit data to be filled
    */
    template <class T> void fill_array(T* dest, int num, const T& data) {
      for (int n = 0; n < num; n++)
        ((T*)dest)[n] = data;
    }
  } // namespace utils
} // namespace gaia