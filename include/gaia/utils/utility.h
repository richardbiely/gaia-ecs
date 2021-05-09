#pragma once

#include <type_traits>

namespace gaia {
  namespace utils {
    template <typename...> inline constexpr auto is_unique = std::true_type{};

    template <typename T, typename... Rest>
    inline constexpr auto is_unique<T, Rest...> = std::bool_constant<
        (!std::is_same_v<T, Rest> && ...) && is_unique<Rest...>>{};
  } // namespace utils
} // namespace gaia