#pragma once
#include <tuple>
#include <type_traits>
#include <utility>

namespace gaia {
	namespace utils {

#pragma region "Tuple to struct conversion"

		template <class S, size_t... Is, class Tuple>
		S tuple_to_struct(std::index_sequence<Is...>, Tuple&& tup) {
			return {std::get<Is>(std::forward<Tuple>(tup))...};
		}

		template <class S, class Tuple>
		S tuple_to_struct(Tuple&& tup) {
			using T = std::remove_reference_t<Tuple>;

			return tuple_to_struct<S>(
					std::make_index_sequence<std::tuple_size<T>{}>{},
					std::forward<Tuple>(tup));
		}

#pragma endregion

#pragma region "Struct to tuple conversion"

		// Check if type T is constructible via T{Args...}
		struct any_type {
			template <class T>
			constexpr operator T(); // non explicit
		};

		template <class T, class... TArgs>
		decltype(void(T{std::declval<TArgs>()...}), std::true_type{})
		is_braces_constructible(int);

		template <class, class...>
		std::false_type is_braces_constructible(...);

		template <class T, class... TArgs>
		using is_braces_constructible_t =
				decltype(is_braces_constructible<T, TArgs...>(0));

		//! Converts a struct to a tuple (struct must support initialization via:
		//! Struct{x,y,...,z})
		template <class T>
		auto struct_to_tuple(T&& object) noexcept {
			using type = std::decay_t<T>;
			// Don't support empty structs. They have no data.
			// We also want compilation to fail for structs with many members so we
			// can handle them here That shouldn't be necessary, though for we plan
			// to support only structs with little amount of arguments.
			if constexpr (is_braces_constructible_t<
												type, any_type, any_type, any_type, any_type, any_type,
												any_type, any_type, any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8] = object;
				return std::make_tuple(p1, p2, p3, p4, p5, p6, p7, p8);
			} else if constexpr (is_braces_constructible_t<
															 type, any_type, any_type, any_type, any_type,
															 any_type, any_type, any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7] = object;
				return std::make_tuple(p1, p2, p3, p4, p5, p6, p7);
			} else if constexpr (is_braces_constructible_t<
															 type, any_type, any_type, any_type, any_type,
															 any_type, any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6] = object;
				return std::make_tuple(p1, p2, p3, p4, p5, p6);
			} else if constexpr (is_braces_constructible_t<
															 type, any_type, any_type, any_type, any_type,
															 any_type>{}) {
				auto&& [p1, p2, p3, p4, p5] = object;
				return std::make_tuple(p1, p2, p3, p4, p5);
			} else if constexpr (is_braces_constructible_t<
															 type, any_type, any_type, any_type,
															 any_type>{}) {
				auto&& [p1, p2, p3, p4] = object;
				return std::make_tuple(p1, p2, p3, p4);
			} else if constexpr (is_braces_constructible_t<
															 type, any_type, any_type, any_type>{}) {
				auto&& [p1, p2, p3] = object;
				return std::make_tuple(p1, p2, p3);
			} else if constexpr (is_braces_constructible_t<
															 type, any_type, any_type>{}) {
				auto&& [p1, p2] = object;
				return std::make_tuple(p1, p2);
			} else if constexpr (is_braces_constructible_t<type, any_type>{}) {
				auto&& [p1] = object;
				return std::make_tuple(p1);
			}
			// Let's not support defaults. We don't want to allow too many types
			// because they indicate some wrong usage of ECS.
			else {
				return std::make_tuple();
			}
		}

#pragma endregion
	} // namespace utils
} // namespace gaia
