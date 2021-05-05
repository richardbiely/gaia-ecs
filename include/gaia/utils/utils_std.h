#pragma once

namespace gaia {
namespace utils {
constexpr uint32_t BadIndex = uint32_t(-1);

template <template <typename...> class C, typename... Args>
uint32_t GetIndexOf(const C<Args...> &arr,
                    typename C<Args...>::value_type item) {
  const auto it = std::find(arr.begin(), arr.end(), item);
  if (it == arr.end())
    return (uint32_t)BadIndex;

  return (uint32_t)std::distance(arr.begin(), it);
}

template <template <typename...> class C, typename... Args>
bool Contains(const C<Args...> &arr, typename C<Args...>::value_type item) {
  const auto it = std::find(arr.begin(), arr.end(), item);
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