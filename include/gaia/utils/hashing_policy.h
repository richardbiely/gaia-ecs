#pragma once
#include "../config/config.h"
#include <ctype.h>
#include <functional>
#include <inttypes.h>
#include <type_traits>

namespace gaia {
	namespace utils {
		template <class TKey>
		struct StdHashingPolicy {
			typedef uint32_t HashValueType;

			static HashValueType GetHash(const TKey& val) {
				return std::hash<TKey>()(val);
			}
		};

		template <class TKey>
		struct PrehashedKeyPolicy {
			typedef uint32_t HashValueType;

			static HashValueType GetHash(const TKey& val) {
				return (uint32_t)val;
			}
		};

		template <class TKey>
		struct HashMethodPolicy {
			typedef uint32_t HashValueType;

			static HashValueType GetHash(const TKey& val) {
				return val.GetHash();
			}
		};

		template <class TKey>
		struct StringHashingPolicyCS {
			typedef uint32_t HashValueType;

			static HashValueType GetHash(const TKey& val) {
				return val.GetHashValue();
			}
		};
		template <>
		struct StringHashingPolicyCS<const char*> {
			typedef uint32_t HashValueType;

			static HashValueType GetHash(const char* str) {
				int c;
				uint32_t hashval = 0;

				while ((c = *str++) != 0)
					hashval = hashval * 37 + c;

				return hashval;
			}
		};

		template <class TKey>
		struct StringHashingPolicyCI {
			typedef uint32_t HashValueType;

			static HashValueType GetHash(const TKey& val) {
				return val.GetHashValueCI();
			}
		};
		template <>
		struct StringHashingPolicyCI<const char*> {
			typedef uint32_t HashValueType;

			static HashValueType GetHash(const char* str) {
				int c;
				uint32_t hashval = 0;

				while ((c = *str++) != 0)
					hashval = hashval * 37 + tolower(c);

				return hashval;
			}
		};

		template <class TKey>
		struct SequentialKeyHash {
			typedef uint32_t HashValueType;

			static HashValueType GetHash(const TKey& val) {
				// Relies on implementation of GenericHashSet that uses all but 7 least
				// significant bits for initial probe position. 128 sequential keys
				// would thus fall into same bucket. Avoid this by rotating bits.
				HashValueType bottom = uintptr_t(val) << 7;
				HashValueType top = (uintptr_t(val) >> (sizeof(TKey) * 8 - 7)) & 0x7F;
				return top | bottom;
			}
		};

		//! Hash bytes of the TKey type
		template <class TKey>
		struct MemoryHashingPolicy {
			typedef uint32_t HashValueType;

			static uint32_t GetHash(const TKey& desc) {
				constexpr uint32_t count = sizeof(TKey);
				auto bytes = reinterpret_cast<const char*>(&desc);
				uint32_t val = 1806131521;
					for (uint32_t i = 0; i < count; ++i) {
						val ^= static_cast<uint32_t>(bytes[i]);
						val *= 1674261049;
					}

				return val;
			}
		};

#pragma region "Fowler-Noll-Vo hash"
		// Fowler-Noll-Vo hash is a fast public-domain hash function good for
		// checksums

		constexpr uint32_t val_32_const = 0x811c9dc5;
		constexpr uint32_t prime_32_const = 0x1000193;

		constexpr uint32_t hash_fnv1a_32(
				const char* const str, const uint32_t value = val_32_const) noexcept {
			return (str[0] == '\0')
								 ? value
								 : hash_fnv1a_32(
											 &str[1], (value ^ (uint32_t)(str[0])) * prime_32_const);
		}

		constexpr uint64_t val_64_const = 0xcbf29ce484222325;
		constexpr uint64_t prime_64_const = 0x100000001b3;

		constexpr uint64_t hash_fnv1a_64(
				const char* const str, const uint64_t value = val_64_const) noexcept {
			return (str[0] == '\0')
								 ? value
								 : hash_fnv1a_64(
											 &str[1], (value ^ (uint64_t)(str[0])) * prime_64_const);
		}

		template <class TKey>
		struct Fnv1a32HashingPolicy {
			using HashValueType = uint32_t;

			static HashValueType GetHash(const TKey& val) {
				return val.GetHashValue();
			}
		};
		template <>
		struct Fnv1a32HashingPolicy<const char*> {
			using HashValueType = uint32_t;

			static HashValueType GetHash(const char* str) {
				return hash_fnv1a_32(str);
			}
		};

		template <class TKey>
		struct Fnv1a64HashingPolicy {
			using HashValueType = uint64_t;

			static HashValueType GetHash(const TKey& val) {
				return val.GetHashValue();
			}
		};
		template <>
		struct Fnv1a64HashingPolicy<const char*> {
			using HashValueType = uint64_t;

			static HashValueType GetHash(const char* str) {
				return hash_fnv1a_64(str);
			}
		};

#pragma endregion
	} // namespace utils
} // namespace gaia
