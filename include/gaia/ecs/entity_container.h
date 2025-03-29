#pragma once
#include "../config/config.h"

#include <cstdint>
#include <type_traits>

#include "../cnt/ilist.h"
#include "../cnt/map.h"
#include "../cnt/paged_storage.h"
#include "api.h"
#include "id.h"

namespace gaia {
	namespace ecs {
		struct EntityContainer;
	}

	namespace cnt {
		template <>
		struct to_page_storage_id<ecs::EntityContainer> {
			static page_storage_id get(const ecs::EntityContainer& item) noexcept;
		};
	} // namespace cnt

	namespace ecs {
		class Chunk;
		class Archetype;
		class World;

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
			IsExclusive = 1 << 8,
			HasAliasOf = 1 << 9,
			IsSingleton = 1 << 10,
			DeleteRequested = 1 << 11,
			RefDecreased = 1 << 12, // GAIA_USE_ENTITY_REFCNT
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
			//! Entity does not use this bit (always zero) so we steal it
			//! for special purposes.
			uint32_t dis : 1;

			///////////////////////////////////////////////////////////////////

			//! Row at which the entity is stored in the chunk
			uint16_t row;
			//! Flags
			uint16_t flags = 0;

#if GAIA_USE_ENTITY_REFCNT
			//! Number of references held to this entity.
			//! The entity won't be deleted unless the reference count is zero.
			uint32_t refCnt = 1;
#else
			//! Currently unused area
			uint32_t unused = 0;
#endif

			//! Archetype (stable address)
			Archetype* pArchetype;
			//! Chunk the entity currently resides in (stable address)
			Chunk* pChunk;

			// uint8_t depthDependsOn = 0;

			EntityContainer() = default;

			GAIA_NODISCARD static EntityContainer create(uint32_t index, uint32_t generation, void* pCtx) {
				auto* ctx = (EntityContainerCtx*)pCtx;

				EntityContainer ec{};
				ec.idx = index;
				ec.gen = generation;
				ec.ent = (uint32_t)ctx->isEntity;
				ec.pair = (uint32_t)ctx->isPair;
				ec.kind = (uint32_t)ctx->kind;
				return ec;
			}

			GAIA_NODISCARD static Entity handle(const EntityContainer& ec) {
				return Entity(ec.idx, ec.gen, (bool)ec.ent, (bool)ec.pair, (EntityKind)ec.kind);
			}

			void req_del() {
				flags |= DeleteRequested;
			}
		};

#if GAIA_USE_PAGED_ENTITY_CONTAINER
		class EntityContainer_paged_ilist_storage: public cnt::page_storage<EntityContainer> {
		public:
			void add_item(EntityContainer&& container) {
				this->add(GAIA_MOV(container));
			}

			void del_item([[maybe_unused]] EntityContainer& container) {
				// TODO: This would also invalidate the ilist item itself. Don't use for now
				// this->del(container);
			}
		};
#endif

		struct EntityContainers {
			//! Implicit list of entities. Used for look-ups only when searching for
			//! entities in chunks + data validation. Entities only.
#if GAIA_USE_PAGED_ENTITY_CONTAINER
			cnt::ilist<EntityContainer, Entity, EntityContainer_paged_ilist_storage> entities;
#else
			cnt::ilist<EntityContainer, Entity> entities;
#endif
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

#if GAIA_USE_ENTITY_REFCNT
		//! SafeEntity is a wrapper over Entity that makes sure that so long it is around
		//! a given entity will not be deleted. When the last SafeEntity referencing this
		//! entity goes out of scope, only then can the entity be deleted.
		class SafeEntity {
			World* m_w;
			Entity m_entity;

		public:
			SafeEntity(World& w, Entity entity): m_w(&w), m_entity(entity) {
				auto& ec = fetch_mut(w, entity);
				++ec.refCnt;
			}

			~SafeEntity() {
				// EntityContainer can be null only from moved-from SharedEntities.
				// This is not a common occurrence.
				if GAIA_UNLIKELY (m_w == nullptr)
					return;

				auto& ec = fetch_mut(*m_w, m_entity);
				GAIA_ASSERT(ec.refCnt > 0);
				--ec.refCnt;
				if (ec.refCnt == 0)
					del(*m_w, m_entity);
			}

			SafeEntity(const SafeEntity& other): m_w(other.m_w), m_entity(other.m_entity) {
				auto& ec = fetch_mut(*m_w, m_entity);
				++ec.refCnt;
			}
			SafeEntity& operator=(const SafeEntity& other) {
				GAIA_ASSERT(core::addressof(other) != this);

				m_w = other.m_w;
				m_entity = other.m_entity;

				auto& ec = fetch_mut(*m_w, m_entity);
				++ec.refCnt;
				return *this;
			}

			SafeEntity(SafeEntity&& other): m_w(other.m_w), m_entity(other.m_entity) {
				other.m_w = nullptr;
				other.m_entity = {};
			}
			SafeEntity& operator=(SafeEntity&& other) {
				GAIA_ASSERT(core::addressof(other) != this);

				m_w = other.m_w;
				m_entity = other.m_entity;

				other.m_w = nullptr;
				other.m_entity = {};
				return *this;
			}

			GAIA_NODISCARD operator Entity() const noexcept {
				return m_entity;
			}
			GAIA_NODISCARD Entity entity() const noexcept {
				return m_entity;
			}
		};
#endif
	} // namespace ecs

	namespace cnt {
		inline page_storage_id to_page_storage_id<ecs::EntityContainer>::get(const ecs::EntityContainer& item) noexcept {
			return item.idx;
		}
	} // namespace cnt
} // namespace gaia
