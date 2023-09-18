#pragma once
#include "../config/config.h"

#include <cinttypes>
#include <cstdint>

#include "../containers/bitset.h"
#include "archetype_common.h"
#include "component.h"

namespace gaia {
	namespace ecs {
		namespace archetype {
			struct ChunkHeaderOffsets {
				//! Byte at which the first component id is located
				ChunkComponentOffset firstByte_Versions[component::ComponentType::CT_Count]{};
				//! Byte at which the first component id is located
				ChunkComponentOffset firstByte_ComponentIds[component::ComponentType::CT_Count]{};
				//! Byte at which the first component offset is located
				ChunkComponentOffset firstByte_ComponentOffsets[component::ComponentType::CT_Count]{};
				//! Byte at which the first entity is located
				ChunkComponentOffset firstByte_EntityData{};
			};

			struct ChunkHeader final {
			public:
				static constexpr uint16_t CHUNK_LIFESPAN_BITS = 4;
				//! Number of ticks before empty chunks are removed
				static constexpr uint16_t MAX_CHUNK_LIFESPAN = (1 << CHUNK_LIFESPAN_BITS) - 1;
				using DisabledEntityMask = containers::bitset<512>;

				//! Archetype the chunk belongs to
				ArchetypeId archetypeId{};
				//! Number of items enabled in the chunk.
				uint16_t count{};
				//! Capacity (copied from the owner archetype).
				uint16_t capacity{};
				//! Chunk index in its archetype list
				uint16_t index{};
				//! Once removal is requested and it hits 0 the chunk is removed.
				uint16_t lifespanCountdown: CHUNK_LIFESPAN_BITS;
				//! If true this chunk stores disabled entities
				uint16_t hasDisabledEntities : 1;
				//! Updated when chunks are being iterated. Used to inform of structural changes when they shouldn't happen.
				uint16_t structuralChangesLocked : 4;
				//! True if there's a component that requires custom construction
				uint16_t hasAnyCustomGenericCtor : 1;
				//! True if there's a component that requires custom construction
				uint16_t hasAnyCustomChunkCtor : 1;
				//! True if there's a component that requires custom destruction
				uint16_t hasAnyCustomGenericDtor : 1;
				//! True if there's a component that requires custom destruction
				uint16_t hasAnyCustomChunkDtor : 1;
				//! Version of the world (stable pointer to parent world's world version)
				uint32_t& worldVersion;
				//! Number of components on the archetype
				uint8_t componentCount[component::ComponentType::CT_Count]{};
				//! Offsets to various parts of data inside chunk
				ChunkHeaderOffsets offsets;
				//! Mask of disabled entities
				DisabledEntityMask disabledEntityMask;

				ChunkHeader(const ChunkHeaderOffsets& offs, uint32_t& version):
						lifespanCountdown(0), hasDisabledEntities(0), structuralChangesLocked(0), hasAnyCustomGenericCtor(0),
						hasAnyCustomChunkCtor(0), hasAnyCustomGenericDtor(0), hasAnyCustomChunkDtor(0),
						worldVersion(version), offsets(offs) {
					// Make sure the alignment is right
					GAIA_ASSERT(uintptr_t(this) % (sizeof(size_t)) == 0);
				}
			};
		} // namespace archetype
	} // namespace ecs
} // namespace gaia
