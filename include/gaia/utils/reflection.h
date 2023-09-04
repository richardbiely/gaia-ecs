#pragma once

#include "../config/config_core.h"

#include <tuple>
#include <type_traits>
#include <utility>

namespace gaia {
	namespace utils {

		namespace detail {
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
			using is_braces_constructible_t = decltype(detail::is_braces_constructible<T, TArgs...>(0));
		} // namespace detail

		//----------------------------------------------------------------------
		// Tuple to struct conversion
		//----------------------------------------------------------------------

		template <typename S, size_t... Is, typename Tuple>
		GAIA_NODISCARD S tuple_to_struct(std::index_sequence<Is...> /*no_name*/, Tuple&& tup) {
			return {std::get<Is>(std::forward<Tuple>(tup))...};
		}

		template <typename S, typename Tuple>
		GAIA_NODISCARD S tuple_to_struct(Tuple&& tup) {
			using T = std::remove_reference_t<Tuple>;

			return tuple_to_struct<S>(std::make_index_sequence<std::tuple_size<T>{}>{}, std::forward<Tuple>(tup));
		}

		//----------------------------------------------------------------------
		// Struct to tuple conversion
		//----------------------------------------------------------------------

		//! The number of bits necessary to fit the maximum supported number of members in a struct
		static constexpr uint32_t StructToTupleMaxTypesBits = 4;
		// static constexpr uint32_t StructToTupleMaxTypes = 1 << StructToTupleMaxTypesBits;

		//! Converts a struct to a tuple (struct must support initialization via:
		//! Struct{x,y,...,z})
		template <typename T>
		auto struct_to_tuple(T&& object) noexcept {
			using type = typename std::decay_t<typename std::remove_pointer_t<T>>;

			if constexpr (std::is_empty_v<type>) {
				return std::make_tuple();
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15] = object;
				return std::make_tuple(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14] = object;
				return std::make_tuple(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13] = object;
				return std::make_tuple(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12] = object;
				return std::make_tuple(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11] = object;
				return std::make_tuple(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10] = object;
				return std::make_tuple(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9] = object;
				return std::make_tuple(p1, p2, p3, p4, p5, p6, p7, p8, p9);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8] = object;
				return std::make_tuple(p1, p2, p3, p4, p5, p6, p7, p8);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7] = object;
				return std::make_tuple(p1, p2, p3, p4, p5, p6, p7);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6] = object;
				return std::make_tuple(p1, p2, p3, p4, p5, p6);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5] = object;
				return std::make_tuple(p1, p2, p3, p4, p5);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3, p4] = object;
				return std::make_tuple(p1, p2, p3, p4);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3] = object;
				return std::make_tuple(p1, p2, p3);
			} else if constexpr (detail::is_braces_constructible_t<type, detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2] = object;
				return std::make_tuple(p1, p2);
			} else if constexpr (detail::is_braces_constructible_t<type, detail::any_type>{}) {
				auto&& [p1] = object;
				return std::make_tuple(p1);
			}

			static_assert("Unsupported number of members");
		}

		//----------------------------------------------------------------------
		// Struct to tuple conversion
		//----------------------------------------------------------------------

		template <typename T>
		auto struct_member_count() {
			using type = std::decay_t<T>;

			if constexpr (std::is_empty_v<type>) {
				return 0;
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type>{}) {
				return 15;
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type>{}) {
				return 14;
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type>{}) {
				return 13;
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type>{}) {
				return 12;
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type>{}) {
				return 11;
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type>{}) {
				return 10;
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type>{}) {
				return 9;
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type>{}) {
				return 8;
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type>{}) {
				return 7;
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type>{}) {
				return 6;
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type>{}) {
				return 5;
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type>{}) {
				return 4;
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type>{}) {
				return 3;
			} else if constexpr (detail::is_braces_constructible_t<type, detail::any_type, detail::any_type>{}) {
				return 2;
			} else if constexpr (detail::is_braces_constructible_t<type, detail::any_type>{}) {
				return 1;
			}

			static_assert("Unsupported number of members");
		}

		template <typename T, typename Func>
		auto for_each_member(T&& object, Func&& visitor) {
			using type = std::decay_t<T>;

			if constexpr (std::is_empty_v<type>) {
				visitor();
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15] = object;
				return visitor(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14] = object;
				return visitor(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13] = object;
				return visitor(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12] = object;
				return visitor(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11] = object;
				return visitor(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10] = object;
				return visitor(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9] = object;
				return visitor(p1, p2, p3, p4, p5, p6, p7, p8, p9);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8] = object;
				return visitor(p1, p2, p3, p4, p5, p6, p7, p8);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7] = object;
				return visitor(p1, p2, p3, p4, p5, p6, p7);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6] = object;
				return visitor(p1, p2, p3, p4, p5, p6);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5] = object;
				return visitor(p1, p2, p3, p4, p5);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3, p4] = object;
				return visitor(p1, p2, p3, p4);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3] = object;
				return visitor(p1, p2, p3);
			} else if constexpr (detail::is_braces_constructible_t<type, detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2] = object;
				return visitor(p1, p2);
			} else if constexpr (detail::is_braces_constructible_t<type, detail::any_type>{}) {
				auto&& [p1] = object;
				return visitor(p1);
			}

			static_assert("Unsupported number of members");
		}

	} // namespace utils
} // namespace gaia
