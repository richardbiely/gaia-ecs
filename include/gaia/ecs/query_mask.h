#pragma once
#include "../config/config.h"

#include "component.h"
#include "id.h"

namespace gaia {
	namespace ecs {
		struct Entity;

#if GAIA_USE_PARTITIONED_BLOOM_FILTER
		static constexpr uint64_t s_ct_queryMask_primes[4] = {
				11400714819323198485ull, // golden ratio
				14029467366897019727ull, //
				1609587929392839161ull, //
				9650029242287828579ull //
		};

		struct QueryMask {
			uint64_t value[4];

			bool operator==(const QueryMask& other) const {
				return value[0] == other.value[0] && //
							 value[1] == other.value[1] && //
							 value[2] == other.value[2] && //
							 value[3] == other.value[3];
			}
			bool operator!=(const QueryMask& other) const {
				return !(*this == other);
			}
		};

		//! Hash an entity id into one bit per 64-bit block
		GAIA_NODISCARD inline QueryMask hash_entity_id(Entity entity) {
			QueryMask mask{};
			for (uint32_t i = 0; i < 4; ++i) {
				const uint64_t bit = (entity.id() * s_ct_queryMask_primes[i]) >> (64 - 4); // pick 1 bit in each 16-bit block
				mask.value[i] = (1ull << bit);
			}
			return mask;
		}

		//! Builds a partitioned bloom mask from a list of entity IDs
		GAIA_NODISCARD inline QueryMask build_entity_mask(EntitySpan entities) {
			QueryMask result{};
			for (auto entity: entities) {
				QueryMask hash = hash_entity_id(entity);
				for (uint32_t i = 0; i < 4; ++i)
					result.value[i] |= hash.value[i];
			}
			return result;
		}

		//! Checks is there is a match between two masks
		GAIA_NODISCARD inline bool match_entity_mask(const QueryMask& m1, const QueryMask& m2) {
			return (m1.value[0] & m2.value[0]) != 0 && //
						 (m1.value[1] & m2.value[1]) != 0 && //
						 (m1.value[2] & m2.value[2]) != 0 && //
						 (m1.value[3] & m2.value[3]) != 0;
		}
#else
		using QueryMask = uint64_t;

		//! Hash an entity id into a mask
		GAIA_NODISCARD inline QueryMask hash_entity_id(Entity entity) {
			return (entity.id() * 11400714819323198485ull) >> (64 - 6);
		}

		//! Builds a bloom mask from a list of entity IDs
		GAIA_NODISCARD inline QueryMask build_entity_mask(EntitySpan entities) {
			QueryMask mask = 0;
			for (auto entity: entities)
				mask |= (1ull << hash_entity_id(entity));

			return mask;
		}

		//! Checks is there is a match between two masks
		GAIA_NODISCARD inline bool match_entity_mask(const QueryMask& m1, const QueryMask& m2) {
			return (m1 & m2) != 0;
		}
#endif
	} // namespace ecs
} // namespace gaia
