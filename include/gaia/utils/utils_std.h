#pragma once
#include <algorithm>
#include <inttypes.h>

namespace gaia {
namespace utils {
constexpr uint32_t BadIndex = uint32_t(-1);

template <template <typename...> class C, typename... Args, class Func>
constexpr auto ForEach(const C<Args...> &arr, Func func) {
  return std::for_each(arr.begin(), arr.end(), func);
}

template <template <typename...> class C, typename... Args>
constexpr auto Find(const C<Args...> &arr,
                    typename C<Args...>::const_reference item) {
  return std::find(arr.begin(), arr.end(), item);
}

template <class UnaryPredicate, template <typename...> class C, typename... Args>
constexpr auto FindIf(const C<Args...> &arr, UnaryPredicate predicate) {
  return std::find_if(arr.begin(), arr.end(), predicate);
}

template <template <typename...> class C, typename... Args>
constexpr uint32_t GetIndexOf(const C<Args...> &arr,
                              typename C<Args...>::const_reference item) {
  const auto it = Find(arr, item);
  if (it == arr.end())
    return (uint32_t)BadIndex;

  return (uint32_t)std::distance(arr.begin(), it);
}

template <class UnaryPredicate, template <typename...> class C, typename... Args>
constexpr uint32_t GetIndexOfIf(const C<Args...> &arr,
                                UnaryPredicate predicate) {
  const auto it = FindIf(arr, predicate);
  if (it == arr.end())
    return (uint32_t)BadIndex;

  return (uint32_t)std::distance(arr.begin(), it);
}

template <template <typename...> class C, typename... Args>
constexpr bool Contains(const C<Args...> &arr,
                        typename C<Args...>::const_reference item) {
  const auto it = Find(arr, item);
  return it != arr.end();
}

template <class UnaryPredicate, template <typename...> class C, typename... Args>
constexpr bool ContainsIf(const C<Args...> &arr, UnaryPredicate predicate) {
  const auto it = FindIf(arr, predicate);
  return it != arr.end();
}

template <template <typename...> class C, typename... Args>
void FastErase(C<Args...> &arr, uint32_t idx) {
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
template <class T> void FillArray(T *dest, int num, const T &data) {
  for (int n = 0; n < num; n++)
    ((T *)dest)[n] = data;
}
} // namespace utils
} // namespace gaia