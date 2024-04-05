#pragma once
#include "../config/config.h"

#include <cstdint>

#include "../cnt/darray.h"
#include "../core/hashing_policy.h"

namespace gaia {
	namespace ecs {
		class Archetype;

		using ArchetypeId = uint32_t;
		using ArchetypeDArray = cnt::darray<Archetype*>;
		using ArchetypeIdHash = core::direct_hash_key<uint32_t>;

		struct ArchetypeIdHashPair {
			ArchetypeId id;
			ArchetypeIdHash hash;

			GAIA_NODISCARD bool operator==(ArchetypeIdHashPair other) const {
				return id == other.id;
			}
			GAIA_NODISCARD bool operator!=(ArchetypeIdHashPair other) const {
				return id != other.id;
			}
		};

		static constexpr ArchetypeId ArchetypeIdBad = (ArchetypeId)-1;
		static constexpr ArchetypeIdHashPair ArchetypeIdHashPairBad = {ArchetypeIdBad, {0}};

		class ArchetypeIdLookupKey final {
		public:
			using LookupHash = core::direct_hash_key<uint32_t>;

		private:
			ArchetypeId m_id;
			ArchetypeIdHash m_hash;

		public:
			GAIA_NODISCARD static LookupHash calc(ArchetypeId id) {
				return {static_cast<uint32_t>(core::calculate_hash64(id))};
			}

			static constexpr bool IsDirectHashKey = true;

			ArchetypeIdLookupKey(): m_id(0), m_hash({0}) {}
			ArchetypeIdLookupKey(ArchetypeIdHashPair pair): m_id(pair.id), m_hash(pair.hash) {}
			explicit ArchetypeIdLookupKey(ArchetypeId id, LookupHash hash): m_id(id), m_hash(hash) {}

			GAIA_NODISCARD size_t hash() const {
				return (size_t)m_hash.hash;
			}

			GAIA_NODISCARD bool operator==(const ArchetypeIdLookupKey& other) const {
				// Hash doesn't match we don't have a match.
				// Hash collisions are expected to be very unlikely so optimize for this case.
				if GAIA_LIKELY (m_hash != other.m_hash)
					return false;

				return m_id == other.m_id;
			}
		};
	} // namespace ecs
} // namespace gaia