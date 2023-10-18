#pragma once
#include "../config/config.h"

#include <cinttypes>
#include <cstdint>

#include "../containers/bitset.h"
#include "../utils/utility.h"
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
			public:
				static constexpr uint16_t CHUNK_LIFESPAN_BITS = 4;
				//! Number of ticks before empty chunks are removed
				static constexpr uint16_t MAX_CHUNK_LIFESPAN = (1 << CHUNK_LIFESPAN_BITS) - 1;

				//! Maxiumum number of entities per chunk
				static constexpr uint16_t MAX_CHUNK_ENTITIES = 512;
				static constexpr uint16_t MAX_CHUNK_ENTITIES_BITS = (uint16_t)utils::count_bits(MAX_CHUNK_ENTITIES);
				using DisabledEntityMask = containers::bitset<MAX_CHUNK_ENTITIES>;

				//! Archetype the chunk belongs to
				ArchetypeId archetypeId;

				//! Number of items enabled in the chunk.
				uint32_t count: MAX_CHUNK_ENTITIES_BITS;
				//! Capacity (copied from the owner archetype).
				uint32_t capacity: MAX_CHUNK_ENTITIES_BITS;
				//! Once removal is requested and it hits 0 the chunk is removed.
				uint32_t lifespanCountdown: CHUNK_LIFESPAN_BITS;
				//! If true this chunk stores disabled entities
				uint32_t hasDisabledEntities : 1;
				//! Updated when chunks are being iterated. Used to inform of structural changes when they shouldn't happen.
				uint32_t structuralChangesLocked : 3;
				//! True if there's a component that requires custom construction
				uint32_t hasAnyCustomGenericCtor : 1;
				//! True if there's a component that requires custom construction
				uint32_t hasAnyCustomChunkCtor : 1;
				//! True if there's a component that requires custom destruction
				uint32_t hasAnyCustomGenericDtor : 1;
				//! True if there's a component that requires custom destruction
				uint32_t hasAnyCustomChunkDtor : 1;

				//! Offsets to various parts of data inside chunk
				ChunkHeaderOffsets offsets;
				//! Chunk index in its archetype list
				uint16_t index : 15;
				//! Chunk size
				uint16_t sizeType : 1;

				//! Number of components on the archetype
				uint8_t componentCount[component::ComponentType::CT_Count]{};
				//! Version of the world (stable pointer to parent world's world version)
				uint32_t& worldVersion;
				//! Mask of disabled entities
				DisabledEntityMask disabledEntityMask;

				ChunkHeader(
						uint32_t aid, uint16_t chunkIndex, uint16_t cap, uint16_t st, const ChunkHeaderOffsets& offs,
						uint32_t& version):
						archetypeId(aid),
						count(0), capacity(cap), lifespanCountdown(0), hasDisabledEntities(0), structuralChangesLocked(0),
						hasAnyCustomGenericCtor(0), hasAnyCustomChunkCtor(0), hasAnyCustomGenericDtor(0), hasAnyCustomChunkDtor(0),
						offsets(offs), index(chunkIndex), sizeType(st), worldVersion(version) {
					// Make sure the alignment is right
					GAIA_ASSERT(uintptr_t(this) % (sizeof(size_t)) == 0);
				}
			};
		} // namespace archetype
	} // namespace ecs
} // namespace gaia
