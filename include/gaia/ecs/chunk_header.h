#pragma once
#include <cassert>
#include <inttypes.h>
#include <stdint.h>

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
			//! [8-11] Number of items in the chunk.
			uint32_t items = 0;
			//! [12-15] Once removal is requested and it hits 0 the chunk is removed.
			uint32_t lifespan = 0;
			//! [16-19] Capacity (copied from the owner archetype).
			uint32_t capacity = 0;
			//! [20-275] Versions of individual components on chunk.
			uint32_t versions[ComponentType::CT_Count][MAX_COMPONENTS_PER_ARCHETYPE]{};

			ChunkHeader(const Archetype& archetype): owner(archetype) {
				// Make sure the alignment is right
				GAIA_ASSERT(uintptr_t(this) % 8 == 0);
			}

			void UpdateWorldVersion(ComponentType componentType, uint32_t componentIdx) {
				const auto gv = GetWorldVersionFromArchetype(owner);

				// Make sure only proper input is provided
				GAIA_ASSERT(componentIdx == UINT32_MAX || componentIdx < MAX_COMPONENTS_PER_ARCHETYPE);

				if (componentIdx != UINT32_MAX) {
					// Update the specific component's version
					versions[componentType][componentIdx] = gv;
				} else {
					// Update all components' version
					for (uint32_t i = 0U; i < MAX_COMPONENTS_PER_ARCHETYPE; i++)
						versions[componentType][i] = gv;
				};
			}
		};
	} // namespace ecs
} // namespace gaia
