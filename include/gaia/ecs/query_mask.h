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
				return ((value[0] ^ other.value[0]) | //
								(value[1] ^ other.value[1]) | //
								(value[2] ^ other.value[2]) | //
								(value[3] ^ other.value[3])) == 0;
			}
			bool operator!=(const QueryMask& other) const {
				return !(*this == other);
			}
		};

		//! Hash an entity id into one bit per 64-bit block
		GAIA_NODISCARD inline QueryMask hash_entity_id(Entity entity) {
			QueryMask mask{};
			const uint64_t id = entity.id();

			// Pick 1 bit in each 16-bit block.
			// We are shifting right by 60 bits, leaving only the top 4 bits of the 64-bit product. This effectively
			// gives us a random-ish value in the range 0–15, which fits one bit per 16-bit block (since 2^4 = 16).
			const auto bit0 = (id * s_ct_queryMask_primes[0]) >> (64 - 4);
			const auto bit1 = (id * s_ct_queryMask_primes[1]) >> (64 - 4);
			const auto bit2 = (id * s_ct_queryMask_primes[2]) >> (64 - 4);
			const auto bit3 = (id * s_ct_queryMask_primes[3]) >> (64 - 4);

			mask.value[0] = 1ull << bit0;
			mask.value[1] = 1ull << bit1;
			mask.value[2] = 1ull << bit2;
			mask.value[3] = 1ull << bit3;

			return mask;
		}

		//! Builds a partitioned bloom mask from a list of entity IDs
		GAIA_NODISCARD inline QueryMask build_entity_mask(EntitySpan entities) {
			QueryMask result{};
			for (auto entity: entities) {
				QueryMask hash = hash_entity_id(entity);
				result.value[0] |= hash.value[0];
				result.value[1] |= hash.value[1];
				result.value[2] |= hash.value[2];
				result.value[3] |= hash.value[3];
			}
			return result;
		}

		//! Checks is there is a match between two masks
		GAIA_NODISCARD inline bool match_entity_mask(const QueryMask& m1, const QueryMask& m2) {
			const auto r0 = m1.value[0] & m2.value[0];
			const auto r1 = m1.value[1] & m2.value[1];
			const auto r2 = m1.value[2] & m2.value[2];
			const auto r3 = m1.value[3] & m2.value[3];
			return (r0 | r1 | r2 | r3) != 0;
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
