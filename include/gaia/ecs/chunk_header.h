#pragma once
#include <cinttypes>
#include <cstdint>

#include "archetype_common.h"
#include "common.h"
#include "component.h"

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
			uint16_t lifespanCountdown : 11;
			//! If true this chunk stores disabled entities
			uint16_t disabled : 1;
			//! Updated when chunks are being iterated. Used to inform of structural changes when they shouldn't happen.
			uint16_t structuralChangesLocked : 4;
			//! Description of components within this archetype (copied from the owner archetype)
			containers::sarray<archetype::ComponentIdArray, component::ComponentType::CT_Count> componentIds;
			//! Lookup hashes of components within this archetype (copied from the owner archetype)
			containers::sarray<archetype::ComponentOffsetArray, component::ComponentType::CT_Count> componentOffsets;
			//! Version of the world (stable pointer to parent world's world version)
			uint32_t& worldVersion;
			//! Versions of individual components on chunk.
			uint32_t versions[component::ComponentType::CT_Count][archetype::MAX_COMPONENTS_PER_ARCHETYPE]{};

			ChunkHeader(uint32_t& version): worldVersion(version) {
				lifespanCountdown = 0;
				disabled = 0;
				structuralChangesLocked = 0;

				// Make sure the alignment is right
				GAIA_ASSERT(uintptr_t(this) % 8 == 0);
			}

			GAIA_FORCEINLINE void UpdateWorldVersion(component::ComponentType componentType, uint32_t componentIdx) {
				// Make sure only proper input is provided
				GAIA_ASSERT(componentIdx != UINT32_MAX && componentIdx < archetype::MAX_COMPONENTS_PER_ARCHETYPE);

				// Update all components' version
				versions[componentType][componentIdx] = worldVersion;
			}

			GAIA_FORCEINLINE void UpdateWorldVersion(component::ComponentType componentType) {
				// Update all components' version
				for (size_t i = 0; i < archetype::MAX_COMPONENTS_PER_ARCHETYPE; i++)
					versions[componentType][i] = worldVersion;
			}
		};
	} // namespace ecs
} // namespace gaia
