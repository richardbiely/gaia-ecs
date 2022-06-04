#pragma once
#include <cstdint>
#include <type_traits>

#include "../config/config.h"

namespace gaia {
	namespace utils {

		//! Combines values via OR.
		template <typename... T>
		constexpr auto combine_or([[maybe_unused]] T... t) {
			return (... | t);
		}

		//-----------------------------------------------------------------------------------

		// TODO: Keep it here for now. Maybe this version is helpful later
		// namespace detail {
		// 	constexpr void hash_combine2_simple_out(uint32_t& lhs, uint32_t rhs) {
		// 		lhs ^= (rhs * 1610612741);
		// 	}
		// 	constexpr void hash_combine2_simple_out(uint64_t& lhs, uint64_t rhs) {
		// 		lhs ^= (rhs * 1610612741);
		// 	}

		// 	template <typename T>
		// 	[[nodiscard]] constexpr T hash_combine2_simple(T lhs, T rhs) {
		// 		hash_combine2_simple_out(lhs, rhs);
		// 		return lhs;
		// 	}
		// } // namespace detail

		// template <typename T, typename... Rest>
		// constexpr T hash_combine_simple(T first, T next, Rest... rest) {
		// 	auto h = detail::hash_combine2_simple(first, next);
		// 	(detail::hash_combine2_simple_out(h, rest), ...);
		// 	return h;
		// }

		//-----------------------------------------------------------------------------------

		namespace detail {

			constexpr void hash_combine2_out(uint32_t& lhs, uint32_t rhs) {
				lhs ^= rhs + 0x9e3779b9 + (lhs << 6) + (lhs >> 2);
			}
			constexpr void hash_combine2_out(uint64_t& lhs, uint64_t rhs) {
				lhs ^= rhs + 0x9e3779B97f4a7c15ULL + (lhs << 6) + (lhs >> 2);
			}

			template <typename T>
			[[nodiscard]] constexpr T hash_combine2(T lhs, T rhs) {
				hash_combine2_out(lhs, rhs);
				return lhs;
			}
		} // namespace detail

		//! Combines hashes into another complex one
		template <typename T, typename... Rest>
		constexpr T hash_combine(T first, T next, Rest... rest) {
			auto h = detail::hash_combine2(first, next);
			(detail::hash_combine2_out(h, rest), ...);
			return h;
		}

		//----------------------------------------------------------------------
		// Fnv1a 64-bit hash
		//----------------------------------------------------------------------

		namespace detail {
			namespace fnv1a {
				constexpr uint64_t val_64_const = 0xcbf29ce484222325;
				constexpr uint64_t prime_64_const = 0x100000001b3;
			} // namespace fnv1a
		} // namespace detail

		constexpr uint64_t hash_fnv1a_64(const char* const str) noexcept {
			uint64_t hash = detail::fnv1a::val_64_const;

			uint32_t i = 0;
			while (str[i] != '\0') {
				hash = (hash ^ uint64_t(str[i])) * detail::fnv1a::prime_64_const;
				++i;
			}

			return hash;
		}

		constexpr uint64_t hash_fnv1a_64(const char* const str, const uint32_t length) noexcept {
			uint64_t hash = detail::fnv1a::val_64_const;

			for (size_t i = 0; i < length; i++)
				hash = (hash ^ uint64_t(str[i])) * detail::fnv1a::prime_64_const;

			return hash;
		}

	} // namespace utils
} // namespace gaia
