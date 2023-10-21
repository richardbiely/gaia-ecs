#pragma once
#include "../config/config.h"

#include <cinttypes>
#include <cstdint>

#include "../cnt/bitset.h"
#include "../core/utility.h"
#include "archetype_common.h"
#include "component.h"

namespace gaia {
	namespace ecs {
		namespace archetype {
			struct ChunkHeaderOffsets {
				//! Byte at which the first version number is located
				ChunkVersionOffset firstByte_Versions[component::ComponentType::CT_Count]{};
				//! Byte at which the first component id is located
				ChunkComponentOffset firstByte_ComponentIds[component::ComponentType::CT_Count]{};
				//! Byte at which the first component offset is located
				ChunkComponentOffset firstByte_ComponentOffsets[component::ComponentType::CT_Count]{};
				//! Byte at which the first entity is located
				ChunkComponentOffset firstByte_EntityData{};
			};

			struct ChunkHeader final {
				static constexpr uint16_t CHUNK_LIFESPAN_BITS = 4;
				//! Number of ticks before empty chunks are removed
				static constexpr uint16_t MAX_CHUNK_LIFESPAN = (1 << CHUNK_LIFESPAN_BITS) - 1;

				static constexpr uint16_t MAX_CHUNK_ENTITIES_BITS = 16;
				//! Maxiumum number of entities per chunk
				static constexpr uint16_t MAX_CHUNK_ENTITIES = (1 << MAX_CHUNK_ENTITIES_BITS) - 1;

				//! Archetype the chunk belongs to
				ArchetypeId archetypeId;
				//! Chunk index in its archetype list
				uint32_t index;
				//! Number of items enabled in the chunk.
				uint16_t count;
				//! Capacity (copied from the owner archetype).
				uint16_t capacity;

				//! Index of the first enabled entity in the chunk
				uint32_t firstEnabledEntityIndex : 16;
				//! Once removal is requested and it hits 0 the chunk is removed.
				uint32_t lifespanCountdown: CHUNK_LIFESPAN_BITS;
				//! Updated when chunks are being iterated. Used to inform of structural changes when they shouldn't happen.
				uint32_t structuralChangesLocked : 3;
				//! True if there's any generic component that requires custom construction
				uint32_t hasAnyCustomGenericCtor : 1;
				//! True if there's any chunk component that requires custom construction
				uint32_t hasAnyCustomChunkCtor : 1;
				//! True if there's any generic component that requires custom destruction
				uint32_t hasAnyCustomGenericDtor : 1;
				//! True if there's any chunk component that requires custom destruction
				uint32_t hasAnyCustomChunkDtor : 1;
				//! Chunk size type. This tells whether it's 8K or 16K
				uint32_t sizeType : 1;
				//! Empty space for future use
				uint32_t unused : 4;

				//! Offsets to various parts of data inside chunk
				ChunkHeaderOffsets offsets;

				//! Number of components on the archetype
				uint8_t componentCount[component::ComponentType::CT_Count]{};
				//! Version of the world (stable pointer to parent world's world version)
				uint32_t& worldVersion;

				ChunkHeader(
						uint32_t aid, uint32_t chunkIndex, uint16_t cap, uint16_t st, const ChunkHeaderOffsets& offs,
						uint32_t& version):
						archetypeId(aid),
						index(chunkIndex), count(0), capacity(cap), firstEnabledEntityIndex(0), lifespanCountdown(0),
						structuralChangesLocked(0), hasAnyCustomGenericCtor(0), hasAnyCustomChunkCtor(0),
						hasAnyCustomGenericDtor(0), hasAnyCustomChunkDtor(0), sizeType(st), offsets(offs), worldVersion(version) {
					// Make sure the alignment is right
					GAIA_ASSERT(uintptr_t(this) % (sizeof(size_t)) == 0);
				}

				bool HasDisabledEntities() const {
					return count > 0 && firstEnabledEntityIndex > 0;
				}

				bool HasEnabledEntities() const {
					return count > 0 && firstEnabledEntityIndex != count;
				}
			};
		} // namespace archetype
	} // namespace ecs
} // namespace gaia
