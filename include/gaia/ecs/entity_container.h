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
#if GAIA_USE_WEAK_ENTITY
		struct WeakEntityTracker;
#endif

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
			RefDecreased = 1 << 12, // GAIA_USE_SAFE_ENTITY
			Load = 1 << 13, // EntityContainer is being loaded from a file
		};

		struct EntityContainer: cnt::ilist_item_base {
			//! Allocated items: Index in the list.
			//! Deleted items: Index of the next deleted item in the list.
			uint32_t idx;

			///////////////////////////////////////////////////////////////////
			// Bits in this section need to be 1:1 with Entity internal data
			///////////////////////////////////////////////////////////////////

			struct EntityData {
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
			};

			union {
				EntityData data;
				uint32_t dataRaw;
			};

			///////////////////////////////////////////////////////////////////

			//! Row at which the entity is stored in the chunk
			uint16_t row;
			//! Flags
			uint16_t flags = 0;

#if GAIA_USE_SAFE_ENTITY
			//! Number of references held to this entity.
			//! The entity won't be deleted unless the reference count is zero.
			uint32_t refCnt = 1;
#else
			//! Currently unused area
			uint32_t unused = 0;
#endif

#if GAIA_USE_WEAK_ENTITY
			WeakEntityTracker* pWeakTracker = nullptr;
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
				ec.data.gen = generation;
				ec.data.ent = (uint32_t)ctx->isEntity;
				ec.data.pair = (uint32_t)ctx->isPair;
				ec.data.kind = (uint32_t)ctx->kind;
				return ec;
			}

			GAIA_NODISCARD static Entity handle(const EntityContainer& ec) {
				return Entity(ec.idx, ec.data.gen, (bool)ec.data.ent, (bool)ec.data.pair, (EntityKind)ec.data.kind);
			}

			void req_del() {
				flags |= DeleteRequested;
			}
		};

#if GAIA_USE_PAGED_ENTITY_CONTAINER
		struct EntityContainer_paged_ilist_storage: public cnt::page_storage<EntityContainer> {
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

#if GAIA_USE_SAFE_ENTITY
		//! SafeEntity is a wrapper over Entity that makes sure that so long it is around
		//! a given entity will not be deleted. When the last SafeEntity referencing this
		//! entity goes out of scope, only then can the entity be deleted.
		class SafeEntity {
			World* m_w = nullptr;
			Entity m_entity;

		public:
			SafeEntity() = default;
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

			SafeEntity(SafeEntity&& other) noexcept: m_w(other.m_w), m_entity(other.m_entity) {
				other.m_w = nullptr;
				other.m_entity = EntityBad;
			}
			SafeEntity& operator=(SafeEntity&& other) noexcept {
				GAIA_ASSERT(core::addressof(other) != this);

				m_w = other.m_w;
				m_entity = other.m_entity;

				other.m_w = nullptr;
				other.m_entity = EntityBad;
				return *this;
			}

			void save(SerializationBufferDyn& s) const {
				s.save(m_entity.val);
			}
			void load(SerializationBufferDyn& s) {
				Identifier id{};
				s.load(id);
				m_entity = Entity(id);
			}

			GAIA_NODISCARD Entity entity() const noexcept {
				return m_entity;
			}
			GAIA_NODISCARD operator Entity() const noexcept {
				return m_entity;
			}

			bool operator==(const SafeEntity& other) const noexcept {
				return m_entity == other.entity();
			}
		};

		inline bool operator==(const SafeEntity& e1, Entity e2) noexcept {
			return e1.entity() == e2;
		}
		inline bool operator==(Entity e1, const SafeEntity& e2) noexcept {
			return e1 == e2.entity();
		}

		inline bool operator!=(const SafeEntity& e1, Entity e2) noexcept {
			return e1.entity() != e2;
		}
		inline bool operator!=(Entity e1, const SafeEntity& e2) noexcept {
			return e1 != e2.entity();
		}
#endif

#if GAIA_USE_WEAK_ENTITY
		class WeakEntity;

		struct WeakEntityTracker {
			WeakEntityTracker* next = nullptr;
			WeakEntityTracker* prev = nullptr;
			WeakEntity* pWeakEntity = nullptr;
		};

		//! WeakEntity is a wrapper over Entity that makes sure that when the entity is deleted,
		//! this wrapper will start acting as EntityBad. Use when you are afraid that the linked
		//! entity might be recycled multiple times. In that case, the linked entity would suddenly
		//! become something different from would you would expect.
		//! The chance of this happening is rather slim. However, e.g. if you decided to tweak
		//! the value of Entity::gen to a lower number (28 bits for generation by default), WeakEntity
		//! could come handy.
		//! A more useful use case, however, would be if you need an entity identifier that gets automatically
		//! reset when the entity gets deleted without any setup necessary from your end. Certain situations
		//! can be complex and using WeakEntity just might be the one way for you to address them.
		class WeakEntity {
			friend class World;
			friend struct WeakEntityTracker;

			World* m_w = nullptr;
			WeakEntityTracker* m_pTracker = nullptr;
			Entity m_entity;

		public:
			WeakEntity() = default;
			WeakEntity(World& w, Entity entity): m_w(&w), m_pTracker(new WeakEntityTracker()), m_entity(entity) {
				set_tracker();
			}
			~WeakEntity() {
				del_tracker();
			}

			WeakEntity(const WeakEntity& other): m_w(other.m_w), m_entity(other.m_entity) {
				if (other.m_entity == EntityBad)
					return;

				m_pTracker = new WeakEntityTracker();
				m_pTracker->pWeakEntity = this;

				auto& ec = fetch_mut(*other.m_w, other.m_entity);
				if (ec.pWeakTracker != nullptr) {
					ec.pWeakTracker->prev = m_pTracker;
					m_pTracker->next = ec.pWeakTracker;
				} else
					ec.pWeakTracker = m_pTracker;
			}
			WeakEntity& operator=(const WeakEntity& other) {
				GAIA_ASSERT(core::addressof(other) != this);

				m_w = other.m_w;
				m_entity = other.m_entity;

				if (other.m_entity != EntityBad) {
					m_pTracker = new WeakEntityTracker();
					m_pTracker->pWeakEntity = this;

					auto& ec = fetch_mut(*other.m_w, other.m_entity);
					if (ec.pWeakTracker != nullptr) {
						ec.pWeakTracker->prev = m_pTracker;
						m_pTracker->next = ec.pWeakTracker;
					} else
						ec.pWeakTracker = m_pTracker;
				}

				return *this;
			}

			WeakEntity(WeakEntity&& other) noexcept: m_w(other.m_w), m_pTracker(other.m_pTracker), m_entity(other.m_entity) {
				other.m_w = nullptr;
				other.m_pTracker->pWeakEntity = this;
				other.m_pTracker = nullptr;
				other.m_entity = EntityBad;
			}
			WeakEntity& operator=(WeakEntity&& other) noexcept {
				GAIA_ASSERT(core::addressof(other) != this);

				m_w = other.m_w;
				m_pTracker = other.m_pTracker;
				m_entity = other.m_entity;

				other.m_w = nullptr;
				other.m_pTracker->pWeakEntity = this;
				other.m_pTracker = nullptr;
				other.m_entity = EntityBad;
				return *this;
			}

			void set_tracker() {
				GAIA_ASSERT(m_pTracker != nullptr);
				m_pTracker->pWeakEntity = this;

				auto& ec = fetch_mut(*m_w, m_entity);
				if (ec.pWeakTracker != nullptr) {
					ec.pWeakTracker->prev = m_pTracker;
					m_pTracker->next = ec.pWeakTracker;
				} else
					ec.pWeakTracker = m_pTracker;
			}

			void del_tracker() {
				if (m_pTracker == nullptr)
					return;

				if (m_pTracker->next != nullptr)
					m_pTracker->next->prev = m_pTracker->prev;
				if (m_pTracker->prev != nullptr)
					m_pTracker->prev->next = m_pTracker->next;

				auto& ec = fetch_mut(*m_w, m_entity);
				if (ec.pWeakTracker == m_pTracker)
					ec.pWeakTracker = nullptr;

				delete m_pTracker;
				m_pTracker = nullptr;
			}

			void save(SerializationBufferDyn& s) const {
				s.save(m_entity.val);
			}
			void load(SerializationBufferDyn& s) {
				del_tracker();
				Identifier id{};
				s.load(id);
				m_entity = Entity(id);
				set_tracker();
			}

			GAIA_NODISCARD Entity entity() const noexcept {
				return m_entity;
			}
			GAIA_NODISCARD operator Entity() const noexcept {
				return entity();
			}

			bool operator==(const WeakEntity& other) const noexcept {
				return m_entity == other.entity();
			}
		};

		inline bool operator==(const WeakEntity& e1, Entity e2) noexcept {
			return e1.entity() == e2;
		}
		inline bool operator==(Entity e1, const WeakEntity& e2) noexcept {
			return e1 == e2.entity();
		}

		inline bool operator!=(const WeakEntity& e1, Entity e2) noexcept {
			return e1.entity() != e2;
		}
		inline bool operator!=(Entity e1, const WeakEntity& e2) noexcept {
			return e1 != e2.entity();
		}
#endif
	} // namespace ecs

	namespace cnt {
		inline page_storage_id to_page_storage_id<ecs::EntityContainer>::get(const ecs::EntityContainer& item) noexcept {
			return item.idx;
		}
	} // namespace cnt
} // namespace gaia
