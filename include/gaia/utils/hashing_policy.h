#pragma once
#include <cstdint>
#include <functional>
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
		// Fowler-Noll-Vo hash
		//----------------------------------------------------------------------

		// Fowler-Noll-Vo hash is a fast public-domain hash function good for
		// checksums

		constexpr uint32_t val_32_const = 0x811c9dc5;
		constexpr uint32_t prime_32_const = 0x1000193;

		constexpr uint32_t hash_fnv1a_32(const char* const str, const uint32_t value = val_32_const) noexcept {
			return (str[0] == '\0') ? value : hash_fnv1a_32(&str[1], (value ^ (uint32_t)(str[0])) * prime_32_const);
		}

		constexpr uint64_t val_64_const = 0xcbf29ce484222325;
		constexpr uint64_t prime_64_const = 0x100000001b3;

		constexpr uint64_t hash_fnv1a_64(const char* const str, const uint64_t value = val_64_const) noexcept {
			return (str[0] == '\0') ? value : hash_fnv1a_64(&str[1], (value ^ (uint64_t)(str[0])) * prime_64_const);
		}

	} // namespace utils
} // namespace gaia
