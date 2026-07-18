#pragma once
#include "gaia/config/config.h"

#include <cstdint>

#include "gaia/core/hashing_policy.h"
#include "gaia/core/utility.h"

namespace gaia {
	namespace core {
		//! Fixed-limit string lookup key carrying a precomputed 32-bit hash.
		//! \tparam MaxLen Maximum supported string length used by bounded comparisons.
		template <uint32_t MaxLen>
		struct StringLookupKey {
			//! Direct hash wrapper used by lookup containers.
			using LookupHash = core::direct_hash_key<uint32_t>;

		private:
			//! Pointer to the string
			const char* m_pStr;
			//! Length of the string
			uint32_t m_len : 31;
			//! 1 - owned (lifetime managed by the framework), 0 - non-owned (lifetime user-managed)
			uint32_t m_owned : 1;
			//! String hash
			LookupHash m_hash;

			static uint32_t len(const char* pStr) {
				GAIA_FOR(MaxLen) {
					if (pStr[i] == 0)
						return i;
				}
				GAIA_ASSERT2(false, "Only null-terminated strings up to MaxLen characters are supported");
				return BadIndex;
			}

			static LookupHash calc(const char* pStr, uint32_t len) {
				return {static_cast<uint32_t>(core::calculate_hash64(pStr, len))};
			}

		public:
			//! Marker indicating that this key provides its hash directly.
			static constexpr bool IsDirectHashKey = true;

			//! Constructs an empty lookup key.
			StringLookupKey(): m_pStr(nullptr), m_len(0), m_owned(0), m_hash({0}) {}

			//! Constructor calculating hash from the provided string \a pStr and \a len.
			//! \param pStr Pointer to the string
			//! \param len Number of characters
			//! \param owned True if the string is owned
			//! \warning String has to be null-terminated and up to MaxLen characters long.
			//!          Undefined behavior otherwise.
			explicit StringLookupKey(const char* pStr, uint32_t len, uint32_t owned):
					m_pStr(pStr), m_len(len), m_owned(owned), m_hash(calc(pStr, len)) {}

			//! Constructor just for setting values
			//! \param pStr Pointer to the string
			//! \param len Number of characters
			//! \param owned True if the string is owned
			//! \param hash String hash
			explicit StringLookupKey(const char* pStr, uint32_t len, uint32_t owned, LookupHash hash):
					m_pStr(pStr), m_len(len), m_owned(owned), m_hash(hash) {}

			//! Returns the referenced string.
			//! \return Pointer to the string, or nullptr for an empty key.
			const char* str() const {
				return m_pStr;
			}

			//! Returns the string length.
			//! \return Number of characters in the string.
			uint32_t len() const {
				return m_len;
			}

			//! Reports whether Gaia-ECS manages the string lifetime.
			//! \return True when the referenced string is owned by the framework.
			bool owned() const {
				return m_owned == 1;
			}

			//! Returns the precomputed string hash.
			//! \return The 32-bit hash value.
			uint32_t hash() const {
				return m_hash.hash;
			}

			//! Compares keys by hash, length, and string contents.
			//! \param other Key to compare with.
			//! \return True when both keys identify equal strings.
			bool operator==(const StringLookupKey& other) const {
				// Hash doesn't match we don't have a match.
				// Hash collisions are expected to be very unlikely so optimize for this case.
				if GAIA_LIKELY (m_hash != other.m_hash)
					return false;

				// Lengths have to match
				if (m_len != other.m_len)
					return false;

				// Contents have to match
				const auto l = m_len;
				GAIA_ASSUME(l < MaxLen);
				GAIA_FOR(l) {
					if (m_pStr[i] != other.m_pStr[i])
						return false;
				}

				return true;
			}

			//! Compares keys for inequality.
			//! \param other Key to compare with.
			//! \return True when the keys identify different strings.
			bool operator!=(const StringLookupKey& other) const {
				return !operator==(other);
			}
		};
	} // namespace core
} // namespace gaia
