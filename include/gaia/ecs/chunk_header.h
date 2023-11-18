#pragma once
#include "../config/config.h"

#include <cstdint>

#include "../cnt/bitset.h"
#include "../core/utility.h"
#include "archetype_common.h"
#include "chunk_allocator.h"
#include "component.h"
#include "id.h"

namespace gaia {
	namespace ecs {
		class ComponentCache;
		struct ComponentCacheItem;

		struct ChunkDataOffsets {
			//! Byte at which the first version number is located
			ChunkDataVersionOffset firstByte_Versions[ComponentKind::CK_Count]{};
			//! Byte at which the first component id is located
			ChunkDataOffset firstByte_ComponentIds[ComponentKind::CK_Count]{};
			//! Byte at which the first component id is located
			ChunkDataOffset firstByte_Records[ComponentKind::CK_Count]{};
#if GAIA_COMP_ID_PROBING
			//! Byte at which the componentID map is located
			ChunkDataOffset firstByte_CompIdMap[ComponentKind::CK_Count]{};
#endif
			//! Byte at which the first entity is located
			ChunkDataOffset firstByte_EntityData{};
		};

		struct ComponentRecord {
			//! Component id
			Component comp;
			//! Pointer to where the first instance of the component is stored
			uint8_t* pData;
			//! Pointer to component descriptor
			const ComponentCacheItem* pDesc;
		};

		struct ChunkRecords {
			//! Pointer to where component versions are stored
			ComponentVersion* pVersions[ComponentKind::CK_Count]{};
			//! Pointer to where component ids are stored
			ComponentId* pComponentIds[ComponentKind::CK_Count]{};
			//! Pointer to the array of component records
			ComponentRecord* pRecords[ComponentKind::CK_Count]{};
#if GAIA_COMP_ID_PROBING
			//! Pointer to the component id map
			ComponentId* compMap[ComponentKind::CK_Count]{};
#endif
			//! Pointer to the array of entities
			Entity* pEntities{};
		};

		struct ChunkHeader final {
			//! Maxiumum number of entities per chunk.
			//! Defined as sizeof(big_chunk) / sizeof(entity)
			static constexpr uint16_t MAX_CHUNK_ENTITIES = (mem_block_size(1) - 64) / sizeof(Entity);
			static constexpr uint16_t MAX_CHUNK_ENTITIES_BITS = (uint16_t)core::count_bits(MAX_CHUNK_ENTITIES);

			static constexpr uint16_t CHUNK_LIFESPAN_BITS = 4;
			//! Number of ticks before empty chunks are removed
			static constexpr uint16_t MAX_CHUNK_LIFESPAN = (1 << CHUNK_LIFESPAN_BITS) - 1;

			static constexpr uint16_t CHUNK_LOCKS_BITS = 3;
			//! Number of locks the chunk can aquire
			static constexpr uint16_t MAX_CHUNK_LOCKS = (1 << CHUNK_LOCKS_BITS) - 1;

			//! Component cache reference
			const ComponentCache* cc;
			//! Chunk index in its archetype list
			uint32_t index;
			//! Total number of entities in the chunk.
			uint16_t count;
			//! Number of enabled entities in the chunk.
			uint16_t countEnabled;
			//! Capacity (copied from the owner archetype).
			uint16_t capacity;

			//! Index of the first enabled entity in the chunk
			uint32_t firstEnabledEntityIndex: MAX_CHUNK_ENTITIES_BITS;
			//! When it hits 0 the chunk is scheduled for deletion
			uint32_t lifespanCountdown: CHUNK_LIFESPAN_BITS;
			//! True if deleted, false otherwise
			uint32_t dead : 1;
			//! Updated when chunks are being iterated. Used to inform of structural changes when they shouldn't happen.
			uint32_t structuralChangesLocked: CHUNK_LOCKS_BITS;
			//! True if there's any generic component that requires custom construction
			uint32_t hasAnyCustomGenCtor : 1;
			//! True if there's any unique component that requires custom construction
			uint32_t hasAnyCustomUniCtor : 1;
			//! True if there's any generic component that requires custom destruction
			uint32_t hasAnyCustomGenDtor : 1;
			//! True if there's any unique component that requires custom destruction
			uint32_t hasAnyCustomUniDtor : 1;
			//! Chunk size type. This tells whether it's 8K or 16K
			uint32_t sizeType : 1;
			//! Empty space for future use
			uint32_t unused : 7;

			//! Number of components on the archetype
			uint8_t componentCount[ComponentKind::CK_Count]{};
			//! Version of the world (stable pointer to parent world's world version)
			uint32_t& worldVersion;

			static inline uint32_t s_worldVersionDummy = 0;
			ChunkHeader(): worldVersion(s_worldVersionDummy) {}

			ChunkHeader(const ComponentCache& compCache, uint32_t chunkIndex, uint16_t cap, uint16_t st, uint32_t& version):
					cc(&compCache), index(chunkIndex), count(0), countEnabled(0), capacity(cap), firstEnabledEntityIndex(0),
					lifespanCountdown(0), dead(0), structuralChangesLocked(0), hasAnyCustomGenCtor(0), hasAnyCustomUniCtor(0),
					hasAnyCustomGenDtor(0), hasAnyCustomUniDtor(0), sizeType(st), unused(0), worldVersion(version) {
				// Make sure the alignment is right
				GAIA_ASSERT(uintptr_t(this) % (sizeof(size_t)) == 0);
			}

			bool has_disabled_entities() const {
				return firstEnabledEntityIndex > 0;
			}

			bool has_enabled_entities() const {
				return countEnabled > 0;
			}
		};
	} // namespace ecs
} // namespace gaia
