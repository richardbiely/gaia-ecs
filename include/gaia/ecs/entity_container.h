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
			bool isEntity;
			bool isPair;
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
			uint32_t gen : 28;
			//! 0-component, 1-entity
			uint32_t ent : 1;
			//! 0-ordinary, 1-pair
			uint32_t pair : 1;
			//! Component kind
			uint32_t kind : 1;
			//! Disabled
			uint32_t dis : 1;

			///////////////////////////////////////////////////////////////////

			//! Row at which the entity is stored in the chunk
			uint16_t row;
			//! Archetype
			Archetype* pArchetype;
			//! Chunk the entity currently resides in
			Chunk* pChunk;

			EntityContainer() = default;

			static EntityContainer create(uint32_t index, uint32_t generation, void* pCtx) {
				auto* ctx = (EntityContainerCtx*)pCtx;

				EntityContainer ec{};
				ec.idx = index;
				ec.gen = generation;
				ec.ent = (uint32_t)ctx->isEntity;
				ec.pair = (uint32_t)ctx->isPair;
				ec.kind = (uint32_t)ctx->kind;
				return ec;
			}

			static Entity create(const EntityContainer& ec) {
				return Entity(ec.idx, ec.gen, (bool)ec.ent, (bool)ec.pair, (EntityKind)ec.kind);
			}
		};
	} // namespace ecs
} // namespace gaia
