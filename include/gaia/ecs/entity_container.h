#pragma once
#include "../config/config.h"

#include <cstdint>
#include <type_traits>

#include "../cnt/ilist.h"
#include "../cnt/map.h"
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

		using EntityContainerFlagsType = uint16_t;
		enum EntityContainerFlags : EntityContainerFlagsType {
			OnDelete_Remove = 1 << 0,
			OnDelete_Delete = 1 << 1,
			OnDelete_Error = 1 << 2,
			OnDeleteTarget_Remove = 1 << 3,
			OnDeleteTarget_Delete = 1 << 4,
			OnDeleteTarget_Error = 1 << 5,
			HasAcyclic = 1 << 6,
			HasCantCombine = 1 << 7,
			HasAliasOf = 1 << 8,
			IsSingleton = 1 << 9,
			DeleteRequested = 1 << 10,
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
			//! Flags
			uint16_t flags = 0;

			//! Archetype
			Archetype* pArchetype;
			//! Chunk the entity currently resides in
			Chunk* pChunk;

			uint8_t depthDependsOn = 0;

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

			void req_del() {
				flags |= DeleteRequested;
			}
		};

		struct EntityContainers {
			//! Implicit list of entities. Used for look-ups only when searching for
			//! entities in chunks + data validation. Entities only.
			cnt::ilist<EntityContainer, Entity> entities;
			//! Just like m_recs.entities, but stores pairs. Needs to be a map because
			//! pair ids are huge numbers.
			cnt::map<EntityLookupKey, EntityContainer> pairs;

			EntityContainer& operator[](Entity entity) {
				return entity.pair() //
									 ? pairs.find(EntityLookupKey(entity))->second
									 : entities[entity.id()];
			}

			const EntityContainer& operator[](Entity entity) const {
				return entity.pair() //
									 ? pairs.find(EntityLookupKey(entity))->second
									 : entities[entity.id()];
			}
		};
	} // namespace ecs
} // namespace gaia
