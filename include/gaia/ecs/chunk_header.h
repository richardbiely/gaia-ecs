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
			//! [8-9] Number of items in the chunk.
			uint16_t count{};
			//! [10-11] Capacity (copied from the owner archetype).
			uint16_t capacity{};
			//! [12-13] Chunk index in its archetype list
			uint16_t index{};
			//! [14-15] Once removal is requested and it hits 0 the chunk is removed.
			uint16_t lifespan : 15;
			//! [1 bit on byte 15] If true this chunk stores disabled entities
			uint16_t disabled : 1;
			//! [16-272] Versions of individual components on chunk.
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
				for (size_t i = 0; i < MAX_COMPONENTS_PER_ARCHETYPE; i++)
					versions[type][i] = gv;
			}
		};
	} // namespace ecs
} // namespace gaia
