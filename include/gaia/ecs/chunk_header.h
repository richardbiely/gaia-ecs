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
			uint32_t has_custom_generic_ctor : 1;
			//! True if there's a component that requires custom construction
			uint32_t has_custom_chunk_ctor : 1;
			//! True if there's a component that requires custom destruction
			uint32_t has_custom_generic_dtor : 1;
			//! True if there's a component that requires custom destruction
			uint32_t has_custom_chunk_dtor : 1;
			//! Description of components within this archetype (copied from the owner archetype)
			containers::sarray<archetype::ComponentIdArray, component::ComponentType::CT_Count> componentIds;
			//! Lookup hashes of components within this archetype (copied from the owner archetype)
			containers::sarray<archetype::ComponentOffsetArray, component::ComponentType::CT_Count> componentOffsets;
			//! Version of the world (stable pointer to parent world's world version)
			uint32_t& worldVersion;

			struct Versions {
				//! Versions of individual components on chunk.
				uint32_t versions[archetype::MAX_COMPONENTS_PER_ARCHETYPE];
			} versions[component::ComponentType::CT_Count]{};
			// Make sure the mask can take all components
			static_assert(archetype::MAX_COMPONENTS_PER_ARCHETYPE <= 32);

			ChunkHeader(uint32_t& version):
					lifespanCountdown(0), disabled(0), structuralChangesLocked(0), has_custom_generic_ctor(0),
					has_custom_chunk_ctor(0), has_custom_generic_dtor(0), has_custom_chunk_dtor(0), worldVersion(version) {
				// Make sure the alignment is right
				GAIA_ASSERT(uintptr_t(this) % 8 == 0);
			}

			GAIA_FORCEINLINE void UpdateWorldVersion(component::ComponentType componentType, uint32_t componentIdx) {
				// Make sure only proper input is provided
				GAIA_ASSERT(componentIdx != UINT32_MAX && componentIdx < archetype::MAX_COMPONENTS_PER_ARCHETYPE);

				// Update the version of the component
				auto& v = versions[componentType];
				v.versions[componentIdx] = worldVersion;
			}

			GAIA_FORCEINLINE void UpdateWorldVersion(component::ComponentType componentType) {
				// Update the version of all components
				auto& v = versions[componentType];
				for (size_t i = 0; i < archetype::MAX_COMPONENTS_PER_ARCHETYPE; i++)
					v.versions[i] = worldVersion;
			}
		};
	} // namespace ecs
} // namespace gaia
