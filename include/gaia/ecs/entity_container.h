#pragma once
#include "../config/config.h"

#include <cstdint>
#include <type_traits>

#include "../cnt/ilist.h"
#include "id.h"

namespace gaia {
	namespace ecs {
		class Chunk;
		class Archetype;

		struct EntityContainerCtx {
			EntityKind kind;
		};

		struct EntityContainer: cnt::ilist_item_base {
			//! Allocated items: Index in the list.
			//! Deleted items: Index of the next deleted item in the list.
			uint32_t idx;

			///////////////////////////////////////////////////////////////////
			// Bits in this section need to be 1:1 with Entity internal data
			///////////////////////////////////////////////////////////////////

			//! Generation ID of the record
			uint32_t gen : 30;
			//! Component kind
			uint32_t kind : 1;
			//! Disabled
			uint32_t dis : 1;

			///////////////////////////////////////////////////////////////////

			//! Index within the chunk
			uint32_t row;
			//! Archetype
			Archetype* pArchetype;
			//! Chunk the entity currently resides in
			Chunk* pChunk;

			EntityContainer() = default;
			EntityContainer(uint32_t index, uint32_t generation):
					idx(index), gen(generation), dis(0), row(0), pArchetype(nullptr), pChunk(nullptr) {}

			static Entity create(uint32_t index, uint32_t generation, void* pCtx) {
				auto* ctx = (EntityContainerCtx*)pCtx;
				return Entity(index, generation, ctx->kind);
			}
		};
	} // namespace ecs
} // namespace gaia
