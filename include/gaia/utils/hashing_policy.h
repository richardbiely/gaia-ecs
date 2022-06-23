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

			size_t i = 0;
			while (str[i] != '\0') {
				hash = (hash ^ uint64_t(str[i])) * detail::fnv1a::prime_64_const;
				++i;
			}

			return hash;
		}

		constexpr uint64_t hash_fnv1a_64(const char* const str, const size_t length) noexcept {
			uint64_t hash = detail::fnv1a::val_64_const;

			for (size_t i = 0; i < length; i++)
				hash = (hash ^ uint64_t(str[i])) * detail::fnv1a::prime_64_const;

			return hash;
		}

		//----------------------------------------------------------------------
		// Murmur2A 64-bit hash
		//----------------------------------------------------------------------

		namespace detail {
			namespace murmur2a {
				constexpr uint64_t seed_64_const = 0xffffffffffffffc5ull;

				constexpr uint64_t hash_murmur2a_64_ct(const char* const str, const uint32_t size, uint64_t seed) {
					constexpr uint64_t prime = 0xc6a4a7935bd1e995ull;
					constexpr uint32_t shift1 = 19;
					constexpr uint32_t shift2 = 37;

					const uint64_t* data = (const uint64_t*)str;
					const uint64_t* end = data + (size / 8);
					uint64_t hash = seed ^ (size * prime);

					while (data != end) {
						uint64_t word = *data++;
						word *= prime;
						word ^= word >> shift1;
						word *= prime;
						hash ^= word;
						hash *= prime;
					}

					const uint8_t* byte_data = (const uint8_t*)data;
					switch (size & 7) {
						case 7:
							hash ^= ((uint64_t)byte_data[6]) << 48;
							[[fallthrough]];
						case 6:
							hash ^= ((uint64_t)byte_data[5]) << 40;
							[[fallthrough]];
						case 5:
							hash ^= ((uint64_t)byte_data[4]) << 32;
							[[fallthrough]];
						case 4:
							hash ^= ((uint64_t)byte_data[3]) << 24;
							[[fallthrough]];
						case 3:
							hash ^= ((uint64_t)byte_data[2]) << 16;
							[[fallthrough]];
						case 2:
							hash ^= ((uint64_t)byte_data[1]) << 8;
							[[fallthrough]];
						case 1:
							hash ^= ((uint64_t)byte_data[0]);
							break;
						default:
							break;
					}

					hash ^= hash >> shift1;
					hash *= prime;
					hash ^= hash >> shift2;

					return hash;
				}
			} // namespace murmur2a
		} // namespace detail

		constexpr uint64_t hash_murmur2a_64(const char* str) {
			uint32_t size = 0;
			while (str[size] != '\0')
				++size;

			return detail::murmur2a::hash_murmur2a_64_ct(str, size, detail::murmur2a::seed_64_const);
		}

		constexpr uint64_t hash_murmur2a_64(const char* str, uint32_t length) {
			return detail::murmur2a::hash_murmur2a_64_ct(str, length, detail::murmur2a::seed_64_const);
		}

	} // namespace utils
} // namespace gaia
