#pragma once
#include <tuple>
#include <type_traits>
#include <utility>

namespace gaia {
	namespace utils {

		//----------------------------------------------------------------------
		// Tuple to struct conversion
		//----------------------------------------------------------------------

		template <typename S, size_t... Is, typename Tuple>
		S tuple_to_struct(std::index_sequence<Is...>, Tuple&& tup) {
			return {std::get<Is>(std::forward<Tuple>(tup))...};
		}

		template <typename S, typename Tuple>
		S tuple_to_struct(Tuple&& tup) {
			using T = std::remove_reference_t<Tuple>;

			return tuple_to_struct<S>(std::make_index_sequence<std::tuple_size<T>{}>{}, std::forward<Tuple>(tup));
		}

		//----------------------------------------------------------------------
		// Struct to tuple conversion
		//----------------------------------------------------------------------

		// Check if type T is constructible via T{Args...}
		struct any_type {
			template <typename T>
			constexpr operator T(); // non explicit
		};

		template <typename T, typename... TArgs>
		decltype(void(T{std::declval<TArgs>()...}), std::true_type{}) is_braces_constructible(int);

		template <typename, typename...>
		std::false_type is_braces_constructible(...);

		template <typename T, typename... TArgs>
		using is_braces_constructible_t = decltype(is_braces_constructible<T, TArgs...>(0));

		//! Converts a struct to a tuple (struct must support initialization via:
		//! Struct{x,y,...,z})
		template <typename T>
		auto struct_to_tuple(T&& object) noexcept {
			using type = std::decay_t<T>;
			// Don't support empty structs. They have no data.
			// We also want to fail for structs with too many members because it smells with bad usage.
			// Therefore, only 1 to 8 types are supported at the moment.
			if constexpr (is_braces_constructible_t<
												type, any_type, any_type, any_type, any_type, any_type, any_type, any_type, any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8] = object;
				return std::make_tuple(p1, p2, p3, p4, p5, p6, p7, p8);
			} else if constexpr (is_braces_constructible_t<
															 type, any_type, any_type, any_type, any_type, any_type, any_type, any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7] = object;
				return std::make_tuple(p1, p2, p3, p4, p5, p6, p7);
			} else if constexpr (is_braces_constructible_t<
															 type, any_type, any_type, any_type, any_type, any_type, any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6] = object;
				return std::make_tuple(p1, p2, p3, p4, p5, p6);
			} else if constexpr (is_braces_constructible_t<type, any_type, any_type, any_type, any_type, any_type>{}) {
				auto&& [p1, p2, p3, p4, p5] = object;
				return std::make_tuple(p1, p2, p3, p4, p5);
			} else if constexpr (is_braces_constructible_t<type, any_type, any_type, any_type, any_type>{}) {
				auto&& [p1, p2, p3, p4] = object;
				return std::make_tuple(p1, p2, p3, p4);
			} else if constexpr (is_braces_constructible_t<type, any_type, any_type, any_type>{}) {
				auto&& [p1, p2, p3] = object;
				return std::make_tuple(p1, p2, p3);
			} else if constexpr (is_braces_constructible_t<type, any_type, any_type>{}) {
				auto&& [p1, p2] = object;
				return std::make_tuple(p1, p2);
			} else if constexpr (is_braces_constructible_t<type, any_type>{}) {
				auto&& [p1] = object;
				return std::make_tuple(p1);
			} else {
				return std::make_tuple();
			}
		}

	} // namespace utils
} // namespace gaia
