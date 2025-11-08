#pragma once
#include "gaia/config/config.h"

#include <cstdint>

#include "archetype_common.h"
#include "chunk_allocator.h"
#include "component.h"
#include "gaia/cnt/bitset.h"
#include "gaia/core/utility.h"
#include "id.h"

namespace gaia {
	namespace ecs {
		class World;
		class ComponentCache;
		struct ComponentCacheItem;

		struct GAIA_API ChunkDataOffsets {
			//! Byte at which the first version number is located
			ChunkDataVersionOffset firstByte_Versions{};
			//! Byte at which the first entity id is located
			ChunkDataOffset firstByte_CompEntities{};
			//! Byte at which the first component id is located
			ChunkDataOffset firstByte_Records{};
			//! Byte at which the first entity is located
			ChunkDataOffset firstByte_EntityData{};
		};

		struct GAIA_API ComponentRecord {
			//! Component id
			Component comp;
			//! Pointer to where the first instance of the component is stored
			uint8_t* pData;
			//! Pointer to component cache record
			const ComponentCacheItem* pItem;
		};

		struct GAIA_API ChunkRecords {
			//! Pointer to where component versions are stored
			ComponentVersion* pVersions{};
			//! Pointer to where (component) entities are stored
			Entity* pCompEntities{};
			//! Pointer to the array of component records
			ComponentRecord* pRecords{};
			//! Pointer to the array of entities
			Entity* pEntities{};
		};

		struct GAIA_API ChunkHeader final {
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

			//! Parent world
			const World* world;
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
			uint16_t rowFirstEnabledEntity : MAX_CHUNK_ENTITIES_BITS;
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
			uint16_t lifespanCountdown : CHUNK_LIFESPAN_BITS;
			//! True if deleted, false otherwise
			uint16_t dead : 1;
			//! Empty space for future use
			uint16_t unused : 11;

			//! Number of generic entities/components
			uint8_t genEntities;
			//! Number of components on the archetype
			uint8_t cntEntities;
			//! Version of the world (stable pointer to parent world's world version)
			uint32_t& worldVersion;

			static inline uint32_t s_worldVersionDummy = 0;
			ChunkHeader(): worldVersion(s_worldVersionDummy) {}

			ChunkHeader(
					const World& wld, const ComponentCache& compCache, uint32_t chunkIndex, uint16_t cap, uint8_t genEntitiesCnt,
					uint16_t st, uint32_t& version):
					world(&wld), cc(&compCache), index(chunkIndex), count(0), countEnabled(0), capacity(cap),
					//
					rowFirstEnabledEntity(0), hasAnyCustomGenCtor(0), hasAnyCustomUniCtor(0), hasAnyCustomGenDtor(0),
					hasAnyCustomUniDtor(0), sizeType(st), lifespanCountdown(0), dead(0), unused(0),
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
