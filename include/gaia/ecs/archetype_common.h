#pragma once
#include <cinttypes>

#include "../cnt/darray.h"
#include "../cnt/sarray_ext.h"
#include "../core/hashing_policy.h"
#include "comp/component.h"

namespace gaia {
	namespace ecs {
		namespace archetype {
			constexpr uint32_t MAX_COMPONENTS_PER_ARCHETYPE_BITS = 5U;
			//! Maximum number of components on archetype
			constexpr uint32_t MAX_COMPONENTS_PER_ARCHETYPE = 1U << MAX_COMPONENTS_PER_ARCHETYPE_BITS;

			class Archetype;

			using ArchetypeId = uint32_t;
			using ArchetypeList = cnt::darray<Archetype*>;
			using ComponentIdArray = cnt::sarray_ext<comp::ComponentId, MAX_COMPONENTS_PER_ARCHETYPE>;
			using ChunkVersionOffset = uint8_t;
			using ChunkComponentOffset = uint16_t;
			using ComponentOffsetArray = cnt::sarray_ext<ChunkComponentOffset, MAX_COMPONENTS_PER_ARCHETYPE>;

			static constexpr ArchetypeId ArchetypeIdBad = (ArchetypeId)-1;

			GAIA_NODISCARD inline constexpr bool verify_archetype_comp_cnt(uint32_t count) {
				return count <= MAX_COMPONENTS_PER_ARCHETYPE;
			}
		} // namespace archetype
	} // namespace ecs
} // namespace gaia