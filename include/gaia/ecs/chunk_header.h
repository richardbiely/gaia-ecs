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
			//! [8-9] Last entity index. If -1 the chunk is empty.
			uint16_t lastEntityIndex = UINT16_MAX;
			//! [10-11] Once removal is requested and it hits 0 the chunk is removed.
			uint16_t lifespan = 0;
			//! [12-13] Capacity (copied from the owner archetype)
			uint16_t capacity = 0;
			//! [14-63] Empty space
			uint8_t dummy[50]{};
			//! [64-319] Versions of individual components on chunk. Stored on
			//! separate cache lines from the rest.
			uint32_t versions[ComponentType::CT_Count]
											 [MAX_COMPONENTS_PER_ARCHETYPE]{};

			ChunkHeader(const Archetype& archetype): owner(archetype) {
				// Make sure the alignment is right
				GAIA_ASSERT(uintptr_t(this) % 8 == 0);
			}

			void
			UpdateWorldVersion(ComponentType componentType, uint32_t componentIdx) {
				const auto gv = GetWorldVersionFromArchetype(owner);

				// Make sure only proper input is provided
				GAIA_ASSERT(
						componentIdx == UINT32_MAX ||
						componentIdx < MAX_COMPONENTS_PER_ARCHETYPE);

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
		static_assert(
				sizeof(ChunkHeader) == 320,
				"Chunk header needs to be 320 bytes in size");
	} // namespace ecs
} // namespace gaia
