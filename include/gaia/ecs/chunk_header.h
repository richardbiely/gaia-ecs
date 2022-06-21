#pragma once
#include <cinttypes>
#include <cstdint>

#include "common.h"
#include "component.h"
#include "fwd.h"

namespace gaia {
	namespace ecs {
		uint32_t GetWorldVersionFromArchetype(const Archetype& archetype);

		struct ChunkHeader final {
		public:
			//! [0-7] Archetype that created this chunk.
			const Archetype& owner;
#if !GAIA_64
			uint32_t owner_padding;
#endif
			// [8-11]
			struct {
				//! Number of items in the chunk.
				uint32_t count : 16;
				//! Capacity (copied from the owner archetype).
				uint32_t capacity : 16;
			} items{};

			//! [12-15] Chunk index in its archetype list
			uint32_t index{};

			// [16-19]
			struct {
				//! Once removal is requested and it hits 0 the chunk is removed.
				uint32_t lifespan : 31;
				//! If true this chunk stores disabled entities
				uint32_t disabled : 1;
			} info{};

			//! [20-275] Versions of individual components on chunk.
			uint32_t versions[ComponentType::CT_Count][MAX_COMPONENTS_PER_ARCHETYPE]{};

			ChunkHeader(const Archetype& archetype): owner(archetype) {
				// Make sure the alignment is right
				GAIA_ASSERT(uintptr_t(this) % 8 == 0);
			}

			GAIA_FORCEINLINE void UpdateWorldVersion(ComponentType type, uint32_t componentIdx) {
				const auto gv = GetWorldVersionFromArchetype(owner);

				// Make sure only proper input is provided
				GAIA_ASSERT(componentIdx != UINT32_MAX && componentIdx < MAX_COMPONENTS_PER_ARCHETYPE);

				// Update all components' version
				versions[type][componentIdx] = gv;
			}

			GAIA_FORCEINLINE void UpdateWorldVersion(ComponentType type) {
				const auto gv = GetWorldVersionFromArchetype(owner);

				// Update all components' version
				for (uint32_t i = 0; i < MAX_COMPONENTS_PER_ARCHETYPE; i++)
					versions[type][i] = gv;
			}
		};
	} // namespace ecs
} // namespace gaia
