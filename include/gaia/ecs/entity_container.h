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
			bool isEntity;
		};

		struct EntityContainer: cnt::ilist_item_base {
			//! Allocated items: Index in the list.
			//! Deleted items: Index of the next deleted item in the list.
			uint32_t idx;

			///////////////////////////////////////////////////////////////////
			// Bits in this section need to be 1:1 with Entity internal data
			///////////////////////////////////////////////////////////////////

			//! Generation ID of the record
			uint32_t gen : 29;
			//! 0-component, 1-entity
			uint32_t ent : 1;
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
			// EntityContainer(uint32_t index, uint32_t generation):
			// 		idx(index), gen(generation), dis(0), row(0), pArchetype(nullptr), pChunk(nullptr) {}

			static EntityContainer create(uint32_t index, uint32_t generation, void* pCtx) {
				auto* ctx = (EntityContainerCtx*)pCtx;

				EntityContainer ec{};
				ec.idx = index;
				ec.gen = generation;
				ec.kind = (uint32_t)ctx->kind;
				ec.ent = (uint32_t)ctx->isEntity;
				return ec;
			}

			static Entity create(const EntityContainer& ec) {
				return Entity(ec.idx, ec.gen, (bool)ec.ent, (EntityKind)ec.kind);
			}
		};
	} // namespace ecs
} // namespace gaia
