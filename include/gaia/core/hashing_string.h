#pragma once
#include "../config/config.h"

#include <cstdint>

#include "hashing_policy.h"
#include "utility.h"

namespace gaia {
	namespace core {
		template <uint32_t MaxLen>
		struct StringLookupKey {
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
			static constexpr bool IsDirectHashKey = true;

			StringLookupKey(): m_pStr(nullptr), m_len(0), m_owned(0), m_hash({0}) {}
			//! Constructor calculating hash and length from the provided string \param pStr
			//! \warning String has to be null-terminated and up to MaxLen characters long.
			//!          Undefined behavior otherwise.
			explicit StringLookupKey(const char* pStr):
					m_pStr(pStr), m_len(len(pStr)), m_owned(0), m_hash(calc(pStr, m_len)) {}
			//! Constructor calculating hash from the provided string \param pStr and \param length
			//! \warning String has to be null-terminated and up to MaxLen characters long.
			//!          Undefined behavior otherwise.
			explicit StringLookupKey(const char* pStr, uint32_t len):
					m_pStr(pStr), m_len(len), m_owned(0), m_hash(calc(pStr, len)) {}
			//! Constructor just for setting values
			explicit StringLookupKey(const char* pStr, uint32_t len, uint32_t owned, LookupHash hash):
					m_pStr(pStr), m_len(len), m_owned(owned), m_hash(hash) {}

			const char* str() const {
				return m_pStr;
			}

			uint32_t len() const {
				return m_len;
			}

			bool owned() const {
				return m_owned == 1;
			}

			uint32_t hash() const {
				return m_hash.hash;
			}

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

			bool operator!=(const StringLookupKey& other) const {
				return !operator==(other);
			}
		};
	} // namespace core
} // namespace gaia