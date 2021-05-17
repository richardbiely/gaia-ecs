#pragma once

#include <type_traits>

namespace gaia {
	namespace utils {

#pragma region Checking if a template arg is unique among the rest
		template <typename...>
		inline constexpr auto is_unique = std::true_type{};

		template <typename T, typename... Rest>
		inline constexpr auto is_unique<T, Rest...> = std::bool_constant<
				(!std::is_same_v<T, Rest> && ...) && is_unique<Rest...>>{};

		template <typename T, typename... Ts>
		struct unique: std::type_identity<T> {};

		template <typename... Ts, typename U, typename... Us>
		struct unique<std::tuple<Ts...>, U, Us...>:
				std::conditional_t<
						(std::is_same_v<U, Ts> || ...), unique<std::tuple<Ts...>, Us...>,
						unique<std::tuple<Ts..., U>, Us...>> {};

		template <typename... Ts>
		using unique_tuple = typename unique<std::tuple<>, Ts...>::type;
#pragma endregion

#pragma region Calculating total size of all types of tuple
		template <typename... Args>
		constexpr unsigned get_args_size(std::tuple<Args...> const&) {
			return (sizeof(Args) + ...);
		}
#pragma endregion

		template <typename... Type>
		struct func_type_list {};

		template <typename Class, typename Ret, typename... Args>
		func_type_list<Args...> func_args(Ret (Class::*)(Args...) const);

#pragma region Compile - time for each

		//! Compile-time for loop over containers
		//!
		//! Example:
		//! std::array<int, 10> arr;
		//! for_each<0, 10, 1>([&arr][auto i]) {
		//!    std::cout << arr[i] << std::endl;
		//! }
		//! print(69, "likes", 420.0f);
		template <auto FirstIdx, auto LastIdx, auto Inc, typename Func>
		constexpr void for_each(Func&& func) {
				if constexpr (FirstIdx < LastIdx) {
					func(std::integral_constant<decltype(FirstIdx), FirstIdx>());
					for_each<FirstIdx + Inc, LastIdx, Inc>(func);
			}
		}

		//! Compile-time for loop over parameter packs.
		//!
		//! Example:
		//! template<typename... Args>
		//! void print(const Args&... args) {
		//!  for_each([][const auto& value]) {
		//!    std::cout << value << std::endl;
		//!  }
		//! }
		//! print(69, "likes", 420.0f);
		template <typename Func, typename... Args>
		constexpr void for_each_pack(Func&& func, Args&&... args) {
			(func(std::forward<Args>(args)), ...);
		}

		//! Compile-time for loop over tuples and other objects implementing
		//! tuple_size (std::array, std::pair etc).
		//!
		//! Example:
		//! for_each_tuple(const auto& value) {
		//!  std::cout << value << std::endl;
		//!  }, std::make(69, "likes", 420.0f);
		template <typename Func, typename Tuple>
		constexpr void for_each_tuple(Func&& func, Tuple&& tuple) {
			constexpr size_t iters = std::tuple_size_v<std::decay_t<Tuple>>;
			for_each<size_t(0), iters, size_t(1)>(
					[&](auto i) { func(std::get<i.value>(tuple)); });
		}

#pragma endregion

#pragma region Compile - time sort

		namespace detail {
			template <typename Array>
			constexpr void comb_sort_impl(Array& array_) noexcept {
				using size_type = typename Array::size_type;
				size_type gap = array_.size();
				bool swapped = false;
					while ((gap > size_type{1}) or swapped) {
							if (gap > size_type{1}) {
								gap = static_cast<size_type>(gap / 1.247330950103979);
						}
						swapped = false;
							for (size_type i = size_type{0};
									 gap + i < static_cast<size_type>(array_.size()); ++i) {
									if (array_[i] > array_[i + gap]) {
										auto swap = array_[i];
										array_[i] = array_[i + gap];
										array_[i + gap] = swap;
										swapped = true;
								}
							}
					}
			}
		} // namespace detail

		template <typename Array>
		constexpr Array sort(Array array_) noexcept {
			auto sorted = array_;
			detail::comb_sort_impl(sorted);
			return sorted;
		}

#pragma endregion

	} // namespace utils
} // namespace gaia