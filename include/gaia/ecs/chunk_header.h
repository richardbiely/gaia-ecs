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
			//! Byte offset at which the first entity is located
			uint8_t firstByte_EntityData{};
		};

		struct ComponentRecord {
			//! Component id
			Component comp;
			//! Pointer to where the first instance of the component is stored
			uint8_t* pData;
			//! Pointer to component cache record
			const ComponentCacheItem* pItem;
		};

		struct ChunkHeader final {
			static constexpr uint32_t MAX_COMPONENTS_BITS = 5U;
			//! Maximum number of components on archetype
			static constexpr uint32_t MAX_COMPONENTS = 1U << MAX_COMPONENTS_BITS;

			//! Maximum number of entities per chunk.
			//! Defined as sizeof(big_chunk) / sizeof(entity)
			static constexpr uint16_t MAX_CHUNK_ENTITIES = (mem_block_size(1) - 64) / sizeof(Entity);
			static constexpr uint16_t MAX_CHUNK_ENTITIES_BITS = (uint16_t)core::count_bits(MAX_CHUNK_ENTITIES);

			static constexpr uint16_t CHUNK_LIFESPAN_BITS = 4;
			//! Number of ticks before empty chunks are removed
			static constexpr uint16_t MAX_CHUNK_LIFESPAN = (1 << CHUNK_LIFESPAN_BITS) - 1;

			static constexpr uint16_t CHUNK_LOCKS_BITS = 3;
			//! Number of locks the chunk can acquire
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
			uint16_t rowFirstEnabledEntity: MAX_CHUNK_ENTITIES_BITS;
			//! True if there's any generic component that requires custom construction
			uint16_t hasAnyCustomGenCtor : 1;
			//! True if there's any unique component that requires custom construction
			uint16_t hasAnyCustomUniCtor : 1;
			//! True if there's any generic component that requires custom destruction
			uint16_t hasAnyCustomGenDtor : 1;
			//! True if there's any unique component that requires custom destruction
			uint16_t hasAnyCustomUniDtor : 1;
			//! Chunk size type. This tells whether it's 8K or 16K
			uint16_t sizeType : 1;
			//! When it hits 0 the chunk is scheduled for deletion
			uint16_t lifespanCountdown: CHUNK_LIFESPAN_BITS;
			//! True if deleted, false otherwise
			uint16_t dead : 1;
			//! Updated when chunks are being iterated. Used to inform of structural changes when they shouldn't happen.
			uint16_t structuralChangesLocked: CHUNK_LOCKS_BITS;
			//! Empty space for future use
			uint16_t unused : 8;

			//! Number of generic entities/components
			uint8_t genEntities;
			//! Number of components on the archetype
			uint8_t cntEntities;
			//! Version of the world (stable pointer to parent world's world version)
			uint32_t& worldVersion;

			// First entity offset.
			// It begins at offset 128 + 256 + 768 = 1152 bytes.
			// This means that the first 1.1 kiB of data in each chunk is taken by the header.
			// This is unfortunate but it allows 2 important things to happen:
			// 1) Whatever the chunk touches can fit into the L1 cache. We do not need to fetch
			//    anything from the archetype or anywhere else unless the user explicitly asks for it.
			// 2) The fixed area allows us to switch archetypes when adding/removing tags
			//    virtually for free. No chunk copies will be necessary, we simply switch
			//    the archetype pointer.

			//! Versions of components
			ComponentVersion versions[ChunkHeader::MAX_COMPONENTS];
			//! Entity ids forming the chunk/archetype
			Entity ids[ChunkHeader::MAX_COMPONENTS];
			//! Pointer to where the entity array starts
			Entity* pEntities;
			//! Pointers to where data for ids starts
			ComponentRecord recs[ChunkHeader::MAX_COMPONENTS];

			static inline uint32_t s_worldVersionDummy = 0;
			ChunkHeader(): worldVersion(s_worldVersionDummy) {}

			ChunkHeader(
					const ComponentCache& compCache, uint32_t chunkIndex, uint16_t cap, uint8_t genEntitiesCnt, uint16_t st,
					uint32_t& version):
					cc(&compCache), index(chunkIndex), count(0), countEnabled(0), capacity(cap),
					//
					rowFirstEnabledEntity(0), hasAnyCustomGenCtor(0), hasAnyCustomUniCtor(0), hasAnyCustomGenDtor(0),
					hasAnyCustomUniDtor(0), sizeType(st), lifespanCountdown(0), dead(0), structuralChangesLocked(0), unused(0),
					//
					genEntities(genEntitiesCnt), cntEntities(0), worldVersion(version) {
				// Make sure the alignment is right
				GAIA_ASSERT(uintptr_t(this) % (sizeof(size_t)) == 0);
			}

			bool has_disabled_entities() const {
				return rowFirstEnabledEntity > 0;
			}

			bool has_enabled_entities() const {
				return countEnabled > 0;
			}
		};
	} // namespace ecs
} // namespace gaia
