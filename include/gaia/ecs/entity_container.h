#pragma once
#include "../config/config.h"

#include <cinttypes>
#include <type_traits>

#include "../cnt/ilist.h"

namespace gaia {
	namespace ecs {
		class Chunk;
		class Archetype;

		struct EntityContainer: cnt::ilist_item_base {
			//! Allocated items: Index in the list.
			//! Deleted items: Index of the next deleted item in the list.
			uint32_t idx;
			//! Generation ID of the record
			uint32_t gen : 31;
			//! Disabled
			uint32_t dis : 1;
			//! Archetype
			Archetype* pArchetype;
			//! Chunk the entity currently resides in
			Chunk* pChunk;

			EntityContainer() = default;
			EntityContainer(uint32_t index, uint32_t generation):
					idx(index), gen(generation), dis(0), pArchetype(nullptr), pChunk(nullptr) {}
		};
	} // namespace ecs
} // namespace gaia
