#pragma once
#include <cinttypes>
#include <cstdint>

#include "common.h"
#include "component.h"
#include "fwd.h"

namespace gaia {
	namespace ecs {
		struct ChunkHeader final {
		public:
			//! Archetype the chunk belongs to
			uint32_t archetypeId;
			//! Number of items in the chunk.
			uint16_t count{};
			//! Capacity (copied from the owner archetype).
			uint16_t capacity{};
			//! Chunk index in its archetype list
			uint16_t index{};
			//! Once removal is requested and it hits 0 the chunk is removed.
			uint16_t lifespan : 15;
			//! If true this chunk stores disabled entities
			uint16_t disabled : 1;
			//! Description of components within this archetype (copied from the owner archetype)
			containers::sarray<ComponentIdList, ComponentType::CT_Count> componentIds;
			//! Lookup hashes of components within this archetype (copied from the owner archetype)
			containers::sarray<ComponentOffsetList, ComponentType::CT_Count> componentOffsets;
			//! Versions of individual components on chunk.
			const uint32_t& worldVersion;
			//! Versions of individual components on chunk.
			uint32_t versions[ComponentType::CT_Count][MAX_COMPONENTS_PER_ARCHETYPE]{};

			ChunkHeader(const uint32_t& version): worldVersion(version) {
				// Make sure the alignment is right
				GAIA_ASSERT(uintptr_t(this) % 8 == 0);
			}

			GAIA_FORCEINLINE void UpdateWorldVersion(ComponentType componentType, uint32_t componentIdx) {
				// Make sure only proper input is provided
				GAIA_ASSERT(componentIdx != UINT32_MAX && componentIdx < MAX_COMPONENTS_PER_ARCHETYPE);

				// Update all components' version
				versions[componentType][componentIdx] = worldVersion;
			}

			GAIA_FORCEINLINE void UpdateWorldVersion(ComponentType componentType) {
				// Update all components' version
				for (size_t i = 0; i < MAX_COMPONENTS_PER_ARCHETYPE; i++)
					versions[componentType][i] = worldVersion;
			}
		};
	} // namespace ecs
} // namespace gaia
