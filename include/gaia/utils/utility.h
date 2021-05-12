#pragma once

#include <type_traits>

namespace gaia {
	namespace utils {

#pragma region Checking if a template arg is unique among the rest
		template <typename...> inline constexpr auto is_unique = std::true_type{};

		template <typename T, typename... Rest>
		inline constexpr auto is_unique<T, Rest...> = std::bool_constant<
				(!std::is_same_v<T, Rest> && ...) && is_unique<Rest...>>{};
#pragma endregion

#pragma region Calculating total size of all types of tuple
		template <typename... Args>
		constexpr unsigned get_args_size(std::tuple<Args...> const&) {
			return (sizeof(Args) + ...);
		}
#pragma endregion

	} // namespace utils
} // namespace gaia