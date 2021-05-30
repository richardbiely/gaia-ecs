#pragma once
#include <array>
#include <tuple>
#include <type_traits>

namespace gaia {
	namespace utils {

#pragma region Checking if a template arg is unique among the rest
		template <typename...>
		inline constexpr auto is_unique = std::true_type{};

		template <typename T, typename... Rest>
		inline constexpr auto is_unique<T, Rest...> = std::bool_constant<
				(!std::is_same_v<T, Rest> && ...) && is_unique<Rest...>>{};

		namespace detail {
			template <class T>
			struct type_identity {
				using type = T;
			};
		} // namespace detail

		template <typename T, typename... Ts>
		struct unique:
				detail::type_identity<T> {
		}; // TODO: In C++20 we could use std::type_identity

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

#pragma region Function helpers

		template <typename... Type>
		struct func_type_list {};

		template <typename Class, typename Ret, typename... Args>
		func_type_list<Args...> func_args(Ret (Class::*)(Args...) const);

#pragma endregion

#pragma region Type helpers

		template <typename... Type>
		struct type_list {
			using types = type_list;
			static constexpr auto size = sizeof...(Type);
		};

		template <typename ta, typename tb>
		struct type_list_concat;

		template <typename... a, typename... b>
		struct type_list_concat<type_list<a...>, type_list<b...>> {
			using type = type_list<a..., b...>;
		};

#pragma endregion

#pragma region Compile - time for each

		namespace detail {
			template <class Func, auto... Is>
			constexpr void for_each_impl(Func&& func, std::index_sequence<Is...>) {
				(func(std::integral_constant<decltype(Is), Is>{}), ...);
			}

			template <class Tuple, class Func, auto... Is>
			void for_each_tuple_impl(
					Tuple&& tuple, Func&& func, std::index_sequence<Is...>) {
				(func(std::get<Is>(tuple)), ...);
			}
		} // namespace detail

		//! Compile-time for loop. Performs \tparam Iters iterations.
		//!
		//! Example:
		//! std::array<int, 10> arr = { ... };
		//! for_each<arr.size()>([&arr][auto i]) {
		//!    std::cout << arr[i] << std::endl;
		//! }
		template <auto Iters, class Func>
		constexpr void for_each(Func&& func) {
			detail::for_each_impl(
					std::forward<Func>(func), std::make_index_sequence<Iters>());
		}

		//! Compile-time for loop over containers.
		//! Iteration starts at \tparam FirstIdx and end at \tparam LastIdx
		//! (excluding) in increments of \tparam Inc.
		//!
		//! Example:
		//! std::array<int, 10> arr;
		//! for_each_ext<0, 10, 1>([&arr][auto i]) {
		//!    std::cout << arr[i] << std::endl;
		//! }
		//! print(69, "likes", 420.0f);
		template <auto FirstIdx, auto LastIdx, auto Inc, typename Func>
		constexpr void for_each_ext(Func&& func) {
			if constexpr (FirstIdx < LastIdx) {
				func(std::integral_constant<decltype(FirstIdx), FirstIdx>());
				for_each_ext<FirstIdx + Inc, LastIdx, Inc>(func);
			}
		}

		//! Compile-time for loop over parameter packs.
		//!
		//! Example:
		//! template<typename... Args>
		//! void print(const Args&... args) {
		//!  for_each_pack([][const auto& value]) {
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
		template <typename Tuple, typename Func>
		constexpr void for_each_tuple(Tuple&& tuple, Func&& func) {
			detail::for_each_tuple_impl(
					std::forward<Tuple>(tuple), std::forward<Func>(func),
					std::make_index_sequence<
							std::tuple_size<std::remove_reference_t<Tuple>>::value>{});
		}

#pragma endregion

#pragma region Compile - time sort

		namespace detail {
			template <typename T>
			constexpr void swap_if_less(T& lhs, T& rhs) noexcept {
				T t = lhs < rhs ? std::move(lhs) : std::move(rhs);
				rhs = lhs < rhs ? std::move(rhs) : std::move(lhs);
				lhs = std::move(t);
			}

			template <typename Array>
			constexpr void comb_sort_impl(Array& array_) noexcept {
				using size_type = typename Array::size_type;
				size_type gap = array_.size();
				bool swapped = false;
				while ((gap > size_type{1}) || swapped) {
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

		//! Compile-time sort.
		//! Implements a sorting network for \tparam N up to 8
		template <typename T, std::size_t N>
		constexpr void sort(std::array<T, N>& arr) noexcept {
			using detail::swap_if_less;
			if constexpr (N == 1) {
				return;
			} else if constexpr (N == 2) {
				swap_if_less(arr[0], arr[1]);
			} else if constexpr (N == 3) {
				swap_if_less(arr[1], arr[2]);
				swap_if_less(arr[0], arr[2]);
				swap_if_less(arr[0], arr[1]);
			} else if constexpr (N == 4) {
				swap_if_less(arr[0], arr[1]);
				swap_if_less(arr[2], arr[3]);

				swap_if_less(arr[0], arr[2]);
				swap_if_less(arr[1], arr[3]);

				swap_if_less(arr[1], arr[2]);
			} else if constexpr (N == 5) {
				swap_if_less(arr[0], arr[1]);
				swap_if_less(arr[3], arr[4]);

				swap_if_less(arr[2], arr[4]);

				swap_if_less(arr[2], arr[3]);
				swap_if_less(arr[1], arr[4]);

				swap_if_less(arr[0], arr[3]);

				swap_if_less(arr[0], arr[2]);
				swap_if_less(arr[1], arr[3]);

				swap_if_less(arr[1], arr[2]);
			} else if constexpr (N == 6) {
				swap_if_less(arr[1], arr[2]);
				swap_if_less(arr[4], arr[5]);

				swap_if_less(arr[0], arr[2]);
				swap_if_less(arr[3], arr[5]);

				swap_if_less(arr[0], arr[1]);
				swap_if_less(arr[3], arr[4]);
				swap_if_less(arr[2], arr[5]);

				swap_if_less(arr[0], arr[3]);
				swap_if_less(arr[1], arr[4]);

				swap_if_less(arr[2], arr[4]);
				swap_if_less(arr[1], arr[3]);

				swap_if_less(arr[2], arr[3]);
			} else if constexpr (N == 7) {
				swap_if_less(arr[1], arr[2]);
				swap_if_less(arr[3], arr[4]);
				swap_if_less(arr[5], arr[6]);

				swap_if_less(arr[0], arr[2]);
				swap_if_less(arr[3], arr[5]);
				swap_if_less(arr[4], arr[6]);

				swap_if_less(arr[0], arr[1]);
				swap_if_less(arr[4], arr[5]);
				swap_if_less(arr[2], arr[6]);

				swap_if_less(arr[0], arr[4]);
				swap_if_less(arr[1], arr[5]);

				swap_if_less(arr[0], arr[3]);
				swap_if_less(arr[2], arr[5]);

				swap_if_less(arr[1], arr[3]);
				swap_if_less(arr[2], arr[4]);

				swap_if_less(arr[2], arr[3]);
			} else if constexpr (N == 8) {
				swap_if_less(arr[0], arr[1]);
				swap_if_less(arr[2], arr[3]);
				swap_if_less(arr[4], arr[5]);
				swap_if_less(arr[6], arr[7]);

				swap_if_less(arr[0], arr[2]);
				swap_if_less(arr[1], arr[3]);
				swap_if_less(arr[4], arr[6]);
				swap_if_less(arr[5], arr[7]);

				swap_if_less(arr[1], arr[2]);
				swap_if_less(arr[5], arr[6]);
				swap_if_less(arr[0], arr[4]);
				swap_if_less(arr[3], arr[7]);

				swap_if_less(arr[1], arr[5]);
				swap_if_less(arr[2], arr[6]);

				swap_if_less(arr[1], arr[4]);
				swap_if_less(arr[3], arr[6]);

				swap_if_less(arr[2], arr[4]);
				swap_if_less(arr[3], arr[5]);

				swap_if_less(arr[3], arr[4]);
			} else {
				//! TODO: replace with std::sort in C++20
				detail::comb_sort_impl(arr);
			}
		}

#pragma endregion

	} // namespace utils
} // namespace gaia