#pragma once
#include <cinttypes>
#include <cstdint>

#include "archetype_common.h"

namespace gaia {
	namespace ecs {
		struct ChunkHeader final {
		public:
			//! Archetype the chunk belongs to
			uint32_t archetypeId{};
			//! Number of items in the chunk.
			uint16_t count{};
			//! Capacity (copied from the owner archetype).
			uint16_t capacity{};
			//! Chunk index in its archetype list
			uint16_t index{};
			//! Once removal is requested and it hits 0 the chunk is removed.
			uint16_t lifespanCountdown : 11;
			//! If true this chunk stores disabled entities
			uint16_t disabled : 1;
			//! Updated when chunks are being iterated. Used to inform of structural changes when they shouldn't happen.
			uint16_t structuralChangesLocked : 4;
			//! True if there's a component that requires custom construction
			uint16_t hasCustomGenericCtor : 1;
			//! True if there's a component that requires custom construction
			uint16_t hasCustomChunkCtor : 1;
			//! True if there's a component that requires custom destruction
			uint16_t hasCustomGenericDtor : 1;
			//! True if there's a component that requires custom destruction
			uint16_t hasCustomChunkDtor : 1;
			//! Number of generic components on the archetype
			uint16_t componentsGeneric: archetype::MAX_COMPONENTS_PER_ARCHETYPE_BITS;
			//! Number of chunk components on the archetype
			uint16_t componentsChunks: archetype::MAX_COMPONENTS_PER_ARCHETYPE_BITS;
			//! Version of the world (stable pointer to parent world's world version)
			uint32_t& worldVersion;
			//! Byte at which the first component id is located
			archetype::ChunkComponentOffset firstByte_ComponentIds{};
			//! Byte at which the first component offset is located
			archetype::ChunkComponentOffset firstByte_ComponentOffsets{};
			//! Byte at which the first entity is located
			archetype::ChunkComponentOffset firstByte_EntityData{};
			//! Byte at which the first component is located
			archetype::ChunkComponentOffset firstByte_ComponentData{};

			ChunkHeader(uint32_t& version):
					lifespanCountdown(0), disabled(0), structuralChangesLocked(0), hasCustomGenericCtor(0), hasCustomChunkCtor(0),
					hasCustomGenericDtor(0), hasCustomChunkDtor(0), componentsGeneric(0), componentsChunks(0),
					worldVersion(version) {
				// Make sure the alignment is right
				GAIA_ASSERT(uintptr_t(this) % 8 == 0);
			}
		};
	} // namespace ecs
} // namespace gaia
