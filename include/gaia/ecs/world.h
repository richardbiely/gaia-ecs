#pragma once
#include "../config/config.h"

#include <cstdint>
#include <type_traits>

#include "../cnt/darray.h"
#include "../cnt/darray_ext.h"
#include "../cnt/ilist.h"
#include "../cnt/map.h"
#include "../cnt/sarray.h"
#include "../cnt/sarray_ext.h"
#include "../cnt/set.h"
#include "../config/profiler.h"
#include "../core/hashing_policy.h"
#include "../core/hashing_string.h"
#include "../core/span.h"
#include "../core/utility.h"
#include "../mem/mem_alloc.h"
#include "../meta/type_info.h"
#include "archetype.h"
#include "archetype_common.h"
#include "archetype_graph.h"
#include "chunk.h"
#include "chunk_allocator.h"
#include "common.h"
#include "component.h"
#include "component_cache.h"
#include "component_cache_item.h"
#include "component_getter.h"
#include "component_setter.h"
#include "component_utils.h"
#include "entity_container.h"
#include "id.h"
#include "query.h"
#include "query_cache.h"
#include "query_common.h"
#include "query_info.h"

namespace gaia {
	namespace ecs {
		class GAIA_API World final {
			friend class ECSSystem;
			friend class ECSSystemManager;
			friend class CommandBuffer;
			friend void* AllocateChunkMemory(World& world);
			friend void ReleaseChunkMemory(World& world, void* mem);

			using TFunc_Void_With_Entity = void(Entity);
			static void func_void_with_entity([[maybe_unused]] Entity entity) {}

			using EntityNameLookupKey = core::StringLookupKey<256>;
			using PairMap = cnt::map<EntityLookupKey, cnt::set<EntityLookupKey>>;

			//! Cache of components
			ComponentCache m_compCache;
			//! Cache of queries
			QueryCache m_queryCache;

			//! Map of entity -> archetypes
			EntityToArchetypeMap m_entityToArchetypeMap;
			//! Map of [entity; Is relationship targets].
			//!   w.as(herbivore, animal);
			//!   w.as(rabbit, herbivore);
			//!   w.as(hare, herbivore);
			//! -->
			//!   herbivore -> {animal}
			//!   rabbit -> {herbivore}
			//!   hare -> {herbivore}
			PairMap m_entityToAsTargets;
			//! Map of [entity; Is relationship relations]
			//!   w.as(herbivore, animal);
			//!   w.as(rabbit, herbivore);
			//!   w.as(hare, herbivore);
			//!-->
			//!   animal -> {herbivore}
			//!   herbivore -> {rabbit, hare}
			PairMap m_entityToAsRelations;
			//! Map of relation -> targets
			PairMap m_relationsToTargets;
			//! Map of target -> relations
			PairMap m_targetsToRelations;

			//! Array of all archetypes
			ArchetypeDArray m_archetypes;
			//! Map of archetypes identified by their component hash code
			cnt::map<ArchetypeLookupKey, Archetype*> m_archetypesByHash;
			//! Map of archetypes identified by their ID
			cnt::map<ArchetypeIdLookupKey, Archetype*> m_archetypesById;

			//! Pointer to the root archetype
			Archetype* m_pRootArchetype = nullptr;
			//! Entity archetype
			Archetype* m_pEntityArchetype = nullptr;
			//! Pointer to the component archetype
			Archetype* m_pCompArchetype = nullptr;
			//! Id assigned to the next created archetype
			ArchetypeId m_nextArchetypeId = 0;

			//! Entity records
			EntityContainers m_recs;

			//! Name to entity mapping
			cnt::map<EntityNameLookupKey, Entity> m_nameToEntity;

			cnt::set<ArchetypeLookupKey> m_reqArchetypesToDel;
			cnt::set<EntityLookupKey> m_reqEntitiesToDel;

			//! Local set of entites to delete
			cnt::set<EntityLookupKey> m_entitiesToDel;
			//! Array of chunks to delete
			cnt::darray<Chunk*> m_chunksToDel;
			//! Array of archetypes to delete
			ArchetypeDArray m_archetypesToDel;
			//! Index of the last defragmented archetype in the archetype list
			uint32_t m_defragLastArchetypeIdx = 0;
			//! Maximum number of entities to defragment per world tick
			uint32_t m_defragEntitesPerTick = 100;

			//! With every structural change world version changes
			uint32_t m_worldVersion = 0;

		public:
			GAIA_NODISCARD EntityContainer& fetch(Entity entity) {
				// Valid entity
				GAIA_ASSERT(valid(entity));
				// Wildcard pairs are not a real entity so we can't accept them
				GAIA_ASSERT(!entity.pair() || !is_wildcard(entity));
				return m_recs[entity];
			}

			GAIA_NODISCARD const EntityContainer& fetch(Entity entity) const {
				// Valid entity
				GAIA_ASSERT(valid(entity));
				// Wildcard pairs are not a real entity so we can't accept them
				GAIA_ASSERT(!entity.pair() || !is_wildcard(entity));
				return m_recs[entity];
			}

			GAIA_NODISCARD static bool is_req_del(const EntityContainer& ec) {
				if ((ec.flags & EntityContainerFlags::DeleteRequested) != 0)
					return true;
				if (ec.pArchetype != nullptr && ec.pArchetype->is_req_del())
					return true;

				return false;
			}

			struct EntityBuilder final {
				friend class World;

				World& m_world;
				Entity m_entity;
				Archetype* m_pArchetype;

				EntityBuilder(World& world, Entity entity): m_world(world), m_entity(entity) {
					auto& ec = world.fetch(entity);
					m_pArchetype = ec.pArchetype;
				}

				EntityBuilder(const EntityBuilder&) = default;
				EntityBuilder(EntityBuilder&&) = delete;
				EntityBuilder& operator=(const EntityBuilder&) = delete;
				EntityBuilder& operator=(EntityBuilder&&) = delete;

				~EntityBuilder() {
					commit();
				}

				//! Commits all gathered changes and performs an archetype movement.
				//! \warning Once called, the object is returned to its default state (as if no add/remove was ever called).
				void commit() {
					if (m_pArchetype == nullptr)
						return;

					// Now that we have the final archetype move the entity to it
					m_world.move_entity(m_entity, *m_pArchetype);

					// Finalize the builder by reseting the archetype pointer
					m_pArchetype = nullptr;
				}

				//! Prepares an archetype movement by following the "add" edge of the current archetype.
				//! \param entity Added entity
				EntityBuilder& add(Entity entity) {
					GAIA_PROF_SCOPE(EntityBuilder::add);
					GAIA_ASSERT(m_world.valid(m_entity));
					GAIA_ASSERT(m_world.valid(entity));

					add_inter(entity);
					return *this;
				}

				//! Prepares an archetype movement by following the "add" edge of the current archetype.
				//! \param pair Relationship pair
				EntityBuilder& add(Pair pair) {
					GAIA_PROF_SCOPE(EntityBuilder::add);
					GAIA_ASSERT(m_world.valid(m_entity));
					GAIA_ASSERT(m_world.valid(pair.first()));
					GAIA_ASSERT(m_world.valid(pair.second()));

					add_inter(pair);
					return *this;
				}

				//! Shortcut for add(Pair(Is, entityBase)).
				//! Effectively makes an entity inherit from \param entityBase
				EntityBuilder& as(Entity entityBase) {
					return add(Pair(Is, entityBase));
				}

				//! Check if \param entity inherits from \param entityBase
				//! \return True if entity inherits from entityBase
				GAIA_NODISCARD bool as(Entity entity, Entity entityBase) const {
					return static_cast<const World&>(m_world).is(entity, entityBase);
				}

				//! Shortcut for add(Pair(ChildOf, parent))
				EntityBuilder& child(Entity parent) {
					return add(Pair(ChildOf, parent));
				}

				//! Takes care of registering the component \tparam T
				template <typename T>
				Entity register_component() {
					if constexpr (is_pair<T>::value) {
						const auto rel = m_world.add<typename T::rel>().entity;
						const auto tgt = m_world.add<typename T::tgt>().entity;
						const Entity ent = Pair(rel, tgt);
						add_inter(ent);
						return ent;
					} else {
						return m_world.add<T>().entity;
					}
				}

				template <typename... T>
				EntityBuilder& add() {
					(verify_comp<T>(), ...);
					(add(register_component<T>()), ...);
					return *this;
				}

				//! Prepares an archetype movement by following the "del" edge of the current archetype.
				//! \param entity Removed entity
				EntityBuilder& del(Entity entity) {
					GAIA_PROF_SCOPE(EntityBuilder::del);
					GAIA_ASSERT(m_world.valid(m_entity));
					GAIA_ASSERT(m_world.valid(entity));
					del_inter(entity);
					return *this;
				}

				//! Prepares an archetype movement by following the "del" edge of the current archetype.
				//! \param pair Relationship pair
				EntityBuilder& del(Pair pair) {
					GAIA_PROF_SCOPE(EntityBuilder::add);
					GAIA_ASSERT(m_world.valid(m_entity));
					GAIA_ASSERT(m_world.valid(pair.first()));
					GAIA_ASSERT(m_world.valid(pair.second()));
					del_inter(pair);
					return *this;
				}

				template <typename... T>
				EntityBuilder& del() {
					(verify_comp<T>(), ...);
					(del(register_component<T>()), ...);
					return *this;
				}

			private:
				bool handle_add_deps(Entity entity) {
					cnt::sarray_ext<Entity, Chunk::MAX_COMPONENTS> targets;

					// Handle entity combination that can't be together
					{
						const auto& ec = m_world.fetch(entity);
						if ((ec.flags & EntityContainerFlags::HasCantCombine) != 0) {
							m_world.targets(entity, CantCombine, [&targets](Entity target) {
								targets.push_back(target);
							});
							for (auto e: targets) {
								if (m_pArchetype->has(e)) {
#if GAIA_ASSERT_ENABLED
									GAIA_ASSERT2(false, "Trying to add an entity which can't be combined with the source");
									print_archetype_entities(m_world, *m_pArchetype, entity, true);
#endif
									return false;
								}
							}
						}
					}

					// Handle dependencies
					{
						targets.clear();
						m_world.targets(entity, DependsOn, [&targets](Entity target) {
							targets.push_back(target);
						});

						for (auto e: targets) {
							auto* pArchetype = m_pArchetype;
							handle_add(e);
							if (m_pArchetype != pArchetype)
								handle_add_deps(e);
						}
					}

					return true;
				}

				bool has_DependsOn_deps(Entity entity) const {
					// Don't allow to delete entity if something in the archetype depends on it
					auto ids = m_pArchetype->ids_view();
					for (auto e: ids) {
						if (m_world.has(e, Pair(DependsOn, entity)))
							return true;
					}

					return false;
				}

				static void updateFlag(EntityContainerFlagsType& flags, EntityContainerFlags flag, bool enable) {
					if (enable)
						flags |= flag;
					else
						flags &= ~flag;
				};

				void updateFlag(Entity entity, EntityContainerFlags flag, bool enable) {
					auto& ec = m_world.fetch(entity);
					updateFlag(ec.flags, flag, enable);
				};

				void try_set_flags(Entity entity, bool enable) {
					try_set_Is(entity, enable);
					try_set_CantCombine(entity, enable);
					try_set_OnDeleteTarget(entity, enable);
					try_set_OnDelete(entity, enable);
					try_set_IsSingleton(entity, enable);
				}

				void try_set_Is(Entity entity, bool enable) {
					if (!entity.pair() || entity.id() != Is.id())
						return;

					updateFlag(m_entity, EntityContainerFlags::HasAliasOf, enable);
				}

				void try_set_CantCombine(Entity entity, bool enable) {
					if (!entity.pair() || entity.id() != CantCombine.id())
						return;

					updateFlag(m_entity, EntityContainerFlags::HasCantCombine, enable);
				}

				void try_set_OnDeleteTarget(Entity entity, bool enable) {
					if (!entity.pair())
						return;

					const auto rel = m_world.get(entity.id());
					const auto tgt = m_world.get(entity.gen());

					// Adding a pair to an entity with OnDeleteTarget relationship.
					// We need to update the target entity's flags.
					if (m_world.has(rel, Pair(OnDeleteTarget, Delete)))
						updateFlag(tgt, EntityContainerFlags::OnDeleteTarget_Delete, enable);
					else if (m_world.has(rel, Pair(OnDeleteTarget, Remove)))
						updateFlag(tgt, EntityContainerFlags::OnDeleteTarget_Remove, enable);
					else if (m_world.has(rel, Pair(OnDeleteTarget, Error)))
						updateFlag(tgt, EntityContainerFlags::OnDeleteTarget_Error, enable);
				}

				void try_set_OnDelete(Entity entity, bool enable) {
					if (entity == Pair(OnDelete, Delete))
						updateFlag(m_entity, EntityContainerFlags::OnDelete_Delete, enable);
					else if (entity == Pair(OnDelete, Remove))
						updateFlag(m_entity, EntityContainerFlags::OnDelete_Remove, enable);
					else if (entity == Pair(OnDelete, Error))
						updateFlag(m_entity, EntityContainerFlags::OnDelete_Error, enable);
				}

				void try_set_IsSingleton(Entity entity, bool enable) {
					const bool isSingleton = m_entity == entity;
					updateFlag(m_entity, EntityContainerFlags::IsSingleton, isSingleton && enable);
				}

				void handle_add(Entity entity) {
#if GAIA_DEBUG
					World::verify_add(m_world, *m_pArchetype, m_entity, entity);
#endif

					// Don't add the same entity twice
					if (m_pArchetype->has(entity))
						return;

					try_set_flags(entity, true);

					// Update the Is relationship base counter if necessary
					if (entity.pair() && entity.id() == Is.id()) {
						auto tgt = m_world.get(entity.gen());

						// m_entity -> {..., e}
						auto& entity_to_e = m_world.m_entityToAsTargets[EntityLookupKey(m_entity)];
						entity_to_e.insert(EntityLookupKey{tgt});
						// e -> {..., m_entity}
						auto& e_to_entity = m_world.m_entityToAsRelations[EntityLookupKey(tgt)];
						e_to_entity.insert(EntityLookupKey{m_entity});

						// Make sure the relation entity is registered as archetype so queries can find it
						// auto& ec = m_world.fetch(tgt);
						// m_world.add_entity_archetype_pair(m_entity, ec.pArchetype);
					}

					m_pArchetype = m_world.foc_archetype_add(m_pArchetype, entity);
				}

				void handle_del(Entity entity) {
#if GAIA_DEBUG
					World::verify_del(m_world, *m_pArchetype, m_entity, entity);
#endif

					// Don't delete what has not beed added
					if (!m_pArchetype->has(entity))
						return;

					try_set_flags(entity, false);

					// Update the Is relationship base counter if necessary
					if (entity.pair() && entity.id() == Is.id()) {
						auto e = m_world.get(entity.gen());

						{
							auto& set = m_world.m_entityToAsTargets;
							const auto it = set.find(EntityLookupKey(entity));
							GAIA_ASSERT(it != set.end());
							GAIA_ASSERT(!it->second.empty());
							it->second.erase(EntityLookupKey(e));

							// Remove the record if it is not referenced anymore
							if (it->second.empty())
								set.erase(it);
						}
						{
							auto& set = m_world.m_entityToAsRelations;
							const auto it = set.find(EntityLookupKey(e));
							GAIA_ASSERT(it != set.end());
							GAIA_ASSERT(!it->second.empty());
							it->second.erase(EntityLookupKey(entity));

							// Remove the record if it is not referenced anymore
							if (it->second.empty())
								set.erase(it);
						}
					}

					m_pArchetype = m_world.foc_archetype_del(m_pArchetype, entity);
				}

				void add_inter(Entity entity) {
					GAIA_ASSERT(!is_wildcard(entity));

					if (entity.pair()) {
						// Make sure the entity container record exists if it is a pair
						m_world.assign_pair(entity, *m_world.m_pEntityArchetype);
					}

					if (!handle_add_deps(entity))
						return;

					handle_add(entity);
				}

				void del_inter(Entity entity) {
					if (has_DependsOn_deps(entity))
						return;

					handle_del(entity);
				}
			};

			World() {
				init();
			}

			~World() {
				done();
			}

			World(World&&) = delete;
			World(const World&) = delete;
			World& operator=(World&&) = delete;
			World& operator=(const World&) = delete;

			//----------------------------------------------------------------------

			GAIA_NODISCARD ComponentCache& comp_cache_mut() {
				return m_compCache;
			}
			GAIA_NODISCARD const ComponentCache& comp_cache() const {
				return m_compCache;
			}

			//----------------------------------------------------------------------

			//! Checks if \param entity is valid.
			//! \return True if the entity is valid. False otherwise.
			GAIA_NODISCARD bool valid(Entity entity) const {
				return entity.pair() //
									 ? valid_pair(entity)
									 : valid_entity(entity);
			}

			//----------------------------------------------------------------------

			//! Returns the entity located at the index \param id
			//! \return Entity
			GAIA_NODISCARD Entity get(EntityId id) const {
				GAIA_ASSERT(valid_entity_id(id));

				const auto& ec = m_recs.entities[id];
				return Entity(id, ec.gen, (bool)ec.ent, (bool)ec.pair, (EntityKind)ec.kind);
			}

			//----------------------------------------------------------------------

			//! Starts a bulk add/remove operation on \param entity.
			//! \param entity Entity
			//! \return EntityBuilder
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			EntityBuilder build(Entity entity) {
				return EntityBuilder(*this, entity);
			}

			//! Creates a new empty entity
			//! \param kind Entity kind
			//! \return New entity
			GAIA_NODISCARD Entity add(EntityKind kind = EntityKind::EK_Gen) {
				return add(*m_pEntityArchetype, true, false, kind);
			}

			//! Creates \param count new empty entities
			//! \param func Functor to execute every time an entity is added
			template <typename Func = TFunc_Void_With_Entity>
			void add_n(uint32_t count, Func func = func_void_with_entity) {
				add_many_entities(*m_pEntityArchetype, count, func);
			}

			//! Creates \param count of entities of the same archetype as \param entity.
			//! \param func Functor to execute every time an entity is added
			//! \note Similar to copy_n, but keeps component values uninitialized or default-intialized
			//!       if they provide a constructor
			template <typename Func = TFunc_Void_With_Entity>
			void add_n(Entity entity, uint32_t count, Func func = func_void_with_entity) {
				auto& ec = m_recs.entities[entity.id()];

				GAIA_ASSERT(ec.pChunk != nullptr);
				GAIA_ASSERT(ec.pArchetype != nullptr);

				add_many_entities(*ec.pArchetype, count, func);
			}

			//! Creates a new component if not found already.
			//! \param kind Component kind
			//! \return Component cache item of the component
			template <typename T>
			GAIA_NODISCARD const ComponentCacheItem& add() {
				static_assert(!is_pair<T>::value, "Pairs can't be registered as components");

				using CT = component_type_t<T>;
				using FT = typename CT::TypeFull;
				constexpr auto kind = CT::Kind;

				const auto* pItem = comp_cache().find<FT>();
				if (pItem != nullptr)
					return *pItem;

				const auto entity = add(*m_pCompArchetype, false, false, kind);
				// Don't allow components to be deleted
				EntityBuilder(*this, entity).add(Pair(OnDelete, Error));

				const auto& item = comp_cache_mut().add<FT>(entity);
				auto& ec = m_recs.entities[entity.id()];

				// Following lines do the following but a bit faster:
				// sset<Component>(item.entity, item.comp);
				auto* pComps = (Component*)ec.pChunk->comp_ptr_mut(EntityKind::EK_Gen, 1);
				auto& comp = pComps[ec.row];
				comp = item.comp;

				// Make sure the default component entity name points to the cache item name
				name_raw(item.entity, item.name.str(), item.name.len());

				// TODO: Implement entity locking. A locked entity can't change archtypes.
				//       This way we can prevent anybody messing with the internal state of the component
				//       entities created at compile-time data.

				return item;
			}

			//! Attaches entity \param object to entity \param entity.
			//! \param entity Source entity
			//! \param object Added entity
			//! \warning It is expected both \param entity and \param object are valid. Undefined behavior otherwise.
			void add(Entity entity, Entity object) {
				EntityBuilder(*this, entity).add(object);
			}

			//! Creates a new entity relationship pair
			//! \param entity Source entity
			//! \param pair Pair
			//! \warning It is expected both \param entity and the entities forming the relationship are valid.
			//!          Undefined behavior otherwise.
			void add(Entity entity, Pair pair) {
				EntityBuilder(*this, entity).add(pair);
			}

			//! Attaches a new component \tparam T to \param entity.
			//! \tparam T Component
			//! \param entity Entity
			//! \warning It is expected the component is not present on \param entity yet. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename T>
			void add(Entity entity) {
				EntityBuilder(*this, entity).add<T>();
			}

			//! Attaches \param object to \param entity. Also sets its value.
			//! \param object Object
			//! \param entity Entity
			//! \param value Value to set for the object
			//! \warning It is expected the component is not present on \param entity yet. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			//! \warning It is expected \param object is valid. Undefined behavior otherwise.
			template <typename T>
			void add(Entity entity, Entity object, T&& value) {
				static_assert(core::is_raw_v<T>);

				EntityBuilder(*this, entity).add(object);

				const auto& ec = fetch(entity);
				// Make sure the idx is 0 for unique entities
				const auto idx = uint16_t(ec.row * (1U - (uint32_t)object.kind()));
				ComponentSetter{{ec.pChunk, idx}}.set(object, GAIA_FWD(value));
			}

			//! Attaches a new component \tparam T to \param entity. Also sets its value.
			//! \tparam T Component
			//! \param entity Entity
			//! \param value Value to set for the component
			//! \warning It is expected the component is not present on \param entity yet. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename T, typename U = typename actual_type_t<T>::Type>
			void add(Entity entity, U&& value) {
				EntityBuilder(*this, entity).add<T>();

				const auto& ec = m_recs.entities[entity.id()];
				// Make sure the idx is 0 for unique entities
				constexpr auto kind = (uint32_t)actual_type_t<T>::Kind;
				const auto idx = uint16_t(ec.row * (1U - (uint32_t)kind));
				ComponentSetter{{ec.pChunk, idx}}.set<T>(GAIA_FWD(value));
			}

			//----------------------------------------------------------------------

			//! Creates a new entity by cloning an already existing one.
			//! \param entity Entity to clone
			//! \return New entity
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			GAIA_NODISCARD Entity copy(Entity entity) {
				GAIA_ASSERT(!entity.pair());
				GAIA_ASSERT(valid(entity));

				auto& ec = m_recs.entities[entity.id()];

				GAIA_ASSERT(ec.pChunk != nullptr);
				GAIA_ASSERT(ec.pArchetype != nullptr);

				auto& archetype = *ec.pArchetype;
				const auto newEntity = add(archetype, entity.entity(), entity.pair(), entity.kind());
				Chunk::copy_entity_data(entity, newEntity, m_recs);

				return newEntity;
			}

			//! Creates \param count new entities by cloning an already existing one.
			//! \param entity Entity to clone
			//! \param count Number of clones to make
			//! \param func void(Entity copy) functor executed every time a copy is created
			//! \warning It is expected \param entity is valid generic entity. Undefined behavior otherwise.
			template <typename Func = TFunc_Void_With_Entity>
			void copy_n(Entity entity, uint32_t count, Func func = func_void_with_entity) {
				GAIA_ASSERT(!entity.pair());
				GAIA_ASSERT(valid(entity));

				auto& ec = m_recs.entities[entity.id()];

				GAIA_ASSERT(ec.pChunk != nullptr);
				GAIA_ASSERT(ec.pArchetype != nullptr);

				auto& archetype = *ec.pArchetype;
				auto* pOldChunk = ec.pChunk;

				// Entities array might get reallocated after m_recs.entities.alloc
				// so insted of fetching the container again we simply cache the row
				// of our source entity.
				const auto oldRow = ec.row;

				EntityContainerCtx ctx{true, false, EntityKind::EK_Gen};

				uint32_t left = count;
				do {
					auto* pChunk = archetype.foc_free_chunk();
					const uint32_t originalChunkSize = pChunk->size();
					const uint32_t freeSlotsInChunk = pChunk->capacity() - originalChunkSize;
					const uint32_t toCreate = core::get_min(freeSlotsInChunk, left);

					GAIA_FOR(toCreate) {
						const auto entityNew = m_recs.entities.alloc(&ctx);
						auto& ecNew = m_recs.entities[entityNew.id()];
						store_entity(ecNew, entityNew, &archetype, pChunk);

#if GAIA_ASSERT_ENABLED
						GAIA_ASSERT(ecNew.pChunk == pChunk);
						auto entityExpected = pChunk->entity_view()[ecNew.row];
						GAIA_ASSERT(entityExpected == entityNew);
#endif
					}

					// New entities were added, try updating the free chunk index
					archetype.try_update_free_chunk_idx();

					// Call constructors for the generic components on the newly added entity if necessary
					if (pChunk->has_custom_gen_ctor())
						pChunk->call_gen_ctors(originalChunkSize, toCreate);

					// Copy data
					{
						GAIA_PROF_SCOPE(copy_n::copy_entity_data);

						auto oldRecs = pOldChunk->comp_rec_view();

						// Copy generic component data from reference entity to our new entity
						GAIA_FOR(pOldChunk->m_header.genEntities) {
							const auto& rec = oldRecs[i];
							if (rec.comp.size() == 0U)
								continue;

							const auto* pSrc = (const void*)pOldChunk->comp_ptr(i);
							GAIA_FOR_(toCreate, rowOffset) {
								auto* pDst = (void*)pChunk->comp_ptr_mut(i);
								rec.pDesc->copy(
										pDst, pSrc, originalChunkSize + rowOffset, oldRow, pChunk->capacity(), pOldChunk->capacity());
							}
						}
					}

					// Call functors
					{
						auto entities = pChunk->entity_view();
						GAIA_FOR2(originalChunkSize, pChunk->size()) func(entities[i]);
					}

					left -= toCreate;
				} while (left > 0);
			}

			//----------------------------------------------------------------------

			void del_inter(Entity entity) {
				auto on_delete = [this](Entity entityToDel) {
					handle_del_entity(entityToDel);
					req_del(entityToDel);
				};

				if (is_wildcard(entity)) {
					const auto rel = get(entity.id());
					const auto tgt = get(entity.gen());

					// (*,*)
					if (rel == All && tgt == All) {
						GAIA_ASSERT2(false, "Not supported yet");
					}
					// (*,X)
					else if (rel == All) {
						if (const auto* pTargets = relations(tgt)) {
							// handle_del might invalidate the targets map so we need to make a copy
							// TODO: this is suboptimal at best, needs to be optimized
							cnt::darray_ext<Entity, 64> tmp;
							for (auto key: *pTargets)
								tmp.push_back(key.entity());
							for (auto e: tmp)
								on_delete(Pair(e, tgt));
						}
					}
					// (X,*)
					else if (tgt == All) {
						if (const auto* pRelations = targets(rel)) {
							// handle_del might invalidate the targets map so we need to make a copy
							// TODO: this is suboptimal at best, needs to be optimized
							cnt::darray_ext<Entity, 64> tmp;
							for (auto key: *pRelations)
								tmp.push_back(key.entity());
							for (auto e: tmp)
								on_delete(Pair(rel, e));
						}
					}
				} else {
					on_delete(entity);
				}
			}

			//! Finalize all queued delete operations
			void del_finalize() {
				// Force-delete all entities from the requested archetypes along with the archetype itself
				for (auto& key: m_reqArchetypesToDel) {
					auto* pArchetype = key.archetype();
					if (pArchetype == nullptr)
						continue;

					del_entities(*pArchetype);

					// Now that all entites are deleted, all their chunks are requestd to get deleted
					// and in turn the archetype itself as well. Therefore, it is added to the archetype
					// delete list and picked up by del_empty_archetypes. No need to call deletion from here.
					// > del_empty_archetype(pArchetype);
				}
				m_reqArchetypesToDel.clear();

				// Try to delete all requested entities
				for (auto it = m_reqEntitiesToDel.begin(); it != m_reqEntitiesToDel.end();) {
					const auto e = it->entity();

					// Entities that form archetypes need to stay until the archetype itself is gone
					if (m_entityToArchetypeMap.contains(*it)) {
						++it;
						continue;
					}

					// Requested entities are partialy deleted. We only need to invalidate them.
					invalidate_entity(e);

					it = m_reqEntitiesToDel.erase(it);
				}
			}

			//! Removes an entity along with all data associated with it.
			//! \param entity Entity to delete
			void del(Entity entity) {
				if (!entity.pair()) {
					// Delete all relationships associated with this entity (if any)
					del_inter(Pair(entity, All));
					del_inter(Pair(All, entity));
				}

				del_inter(entity);
			}

			//! Removes an \param object from \param entity if possible.
			//! \param entity Entity to delete from
			//! \param object Entity to delete
			//! \warning It is expected both \param entity and \param object are valid. Undefined behavior otherwise.
			void del(Entity entity, Entity object) {
				EntityBuilder(*this, entity).del(object);
			}

			//! Removes an existing entity relationship pair
			//! \param entity Source entity
			//! \param pair Pair
			//! \warning It is expected both \param entity and the entities forming the relationship are valid.
			//!          Undefined behavior otherwise.
			void del(Entity entity, Pair pair) {
				EntityBuilder(*this, entity).del(pair);
			}

			//! Removes a component \tparam T from \param entity.
			//! \tparam T Component
			//! \param entity Entity
			//! \warning It is expected the component is present on \param entity. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename T>
			void del(Entity entity) {
				using CT = component_type_t<T>;
				using FT = typename CT::TypeFull;
				EntityBuilder(*this, entity).del<FT>();
			}

			//----------------------------------------------------------------------

			//! Shortcut for add(entity, Pair(Is, entityBase)
			void as(Entity entity, Entity entityBase) {
				add(entity, Pair(Is, entityBase));
			}

			//! Checks if \param entity inherits from \param entityBase.
			//! True if entity inherits from entityBase. False otherwise.
			GAIA_NODISCARD bool is(Entity entity, Entity entityBase) const {
				return is_inter<true>(entity, entityBase);
			}

			//! Checks if \param entity is located in \param entityBase.
			//! This is almost the same as "is" with the exception that false is returned
			//! if \param entity matches \param entityBase
			//! True if entity is located in entityBase. False otherwise.
			GAIA_NODISCARD bool in(Entity entity, Entity entityBase) const {
				return is_inter<false>(entity, entityBase);
			}

			GAIA_NODISCARD bool is_base(Entity target) const {
				GAIA_ASSERT(valid_entity(target));

				// Pairs are not supported
				if (target.pair())
					return false;

				const auto it = m_entityToAsRelations.find(EntityLookupKey(target));
				return it != m_entityToAsRelations.end();
			}

			//----------------------------------------------------------------------

			//! Shortcut for add(entity, Pair(ChildOf, parent)
			void child(Entity entity, Entity parent) {
				add(entity, Pair(ChildOf, parent));
			}

			//! Checks if \param entity is a child of \param parent
			//! True if entity is a child of parent. False otherwise.
			GAIA_NODISCARD bool child(Entity entity, Entity parent) const {
				return has(entity, Pair(ChildOf, parent));
			}

			//----------------------------------------------------------------------

			//! Starts a bulk set operation on \param entity.
			//! \param entity Entity
			//! \return ComponentSetter
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			//! \warning Undefined behavior if \param entity changes archetype after ComponentSetter is created.
			ComponentSetter acc_mut(Entity entity) {
				GAIA_ASSERT(valid(entity));

				const auto& ec = m_recs.entities[entity.id()];
				return ComponentSetter{{ec.pChunk, ec.row}};
			}

			//! Sets the value of the component \tparam T on \param entity.
			//! \tparam T Component
			//! \param entity Entity
			//! \warning It is expected the component is present on \param entity. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			//! \warning Undefined behavior if \param entity changes archetype after ComponentSetter is created.
			template <typename T>
			decltype(auto) set(Entity entity) {
				static_assert(!is_pair<T>::value);
				return acc_mut(entity).mut<T>();
			}

			//! Sets the value of the component \tparam T on \param entity without triggering a world version update.
			//! \tparam T Component
			//! \param entity Entity
			//! \warning It is expected the component is present on \param entity. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			//! \warning Undefined behavior if \param entity changes archetype after ComponentSetter is created.
			template <typename T>
			decltype(auto) sset(Entity entity) {
				static_assert(!is_pair<T>::value);
				return acc_mut(entity).smut<T>();
			}

			//----------------------------------------------------------------------

			//! Starts a bulk get operation on \param entity.
			//! \param entity Entity
			//! \return ComponentGetter
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			//! \warning Undefined behavior if \param entity changes archetype after ComponentGetter is created.
			ComponentGetter acc(Entity entity) const {
				GAIA_ASSERT(valid(entity));

				const auto& ec = m_recs.entities[entity.id()];
				return ComponentGetter{ec.pChunk, ec.row};
			}

			//! Returns the value stored in the component \tparam T on \param entity.
			//! \tparam T Component
			//! \param entity Entity
			//! \return Value stored in the component.
			//! \warning It is expected the component is present on \param entity. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			//! \warning Undefined behavior if \param entity changes archetype after ComponentGetter is created.
			template <typename T>
			GAIA_NODISCARD decltype(auto) get(Entity entity) const {
				return acc(entity).get<T>();
			}

			//----------------------------------------------------------------------

			//! Checks if \param entity is currently used by the world.
			//! \return True if the entity is used. False otherwise.
			GAIA_NODISCARD bool has(Entity entity) const {
				// Pair
				if (entity.pair()) {
					if (is_wildcard(entity)) {
						if (!m_entityToArchetypeMap.contains(EntityLookupKey(entity)))
							return false;

						// If the pair is found, both entities forming it need to be found as well
						GAIA_ASSERT(has(get(entity.id())) && has(get(entity.gen())));

						return true;
					}

					const auto it = m_recs.pairs.find(EntityLookupKey(entity));
					if (it == m_recs.pairs.end())
						return false;

					const auto& ec = it->second;
					if (is_req_del(ec))
						return false;

#if GAIA_ASSERT_ENABLED
					// If the pair is found, both entities forming it need to be found as well
					GAIA_ASSERT(has(get(entity.id())) && has(get(entity.gen())));

					// Index of the entity must fit inside the chunk
					auto* pChunk = ec.pChunk;
					GAIA_ASSERT(pChunk != nullptr && ec.row < pChunk->size());
#endif

					return true;
				}

				// Regular entity
				{
					// Entity ID has to fit inside the entity array
					if (entity.id() >= m_recs.entities.size())
						return false;

					// Index of the entity must fit inside the chunk
					const auto& ec = m_recs.entities[entity.id()];
					if (is_req_del(ec))
						return false;

					auto* pChunk = ec.pChunk;
					return pChunk != nullptr && ec.row < pChunk->size();
				}
			}

			//! Checks if \param pair is currently used by the world.
			//! \return True if the entity is used. False otherwise.
			GAIA_NODISCARD bool has(Pair pair) const {
				return has((Entity)pair);
			}

			//! Tells if \param entity contains the entity \param object.
			//! \param entity Entity
			//! \param object Tested entity
			//! \return True if object is present on entity. False otherwise or if any of the entites is not valid.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			//! \warning Undefined behavior if \param entity changes archetype after ComponentSetter is created.
			GAIA_NODISCARD bool has(Entity entity, Entity object) const {
				const auto& ec = fetch(entity);
				if (is_req_del(ec))
					return false;

				if (object.pair()) {
					const auto* pArchetype = ec.pArchetype;

					// Early exit if there are no pairs on the archetype
					if (pArchetype->pairs() == 0)
						return false;

					EntityId rel = object.id();
					EntityId tgt = object.gen();

					// (*,*)
					if (rel == All.id() && tgt == All.id()) {
						auto ids = pArchetype->ids_view();
						for (auto id: ids) {
							if (!id.pair())
								continue;
							return true;
						}

						return false;
					}

					// (X,*)
					if (rel != All.id() && tgt == All.id()) {
						auto ids = pArchetype->ids_view();
						for (auto id: ids) {
							if (!id.pair())
								continue;
							if (id.id() == rel)
								return true;
						}

						return false;
					}

					// (*,X)
					if (rel == All.id() && tgt != All.id()) {
						auto ids = pArchetype->ids_view();
						for (auto id: ids) {
							if (!id.pair())
								continue;
							if (id.gen() == tgt)
								return true;
						}

						return false;
					}
				}

				return ComponentGetter{ec.pChunk, ec.row}.has(object);
			}

			//! Tells if \param entity contains \param pair.
			//! \param entity Entity
			//! \param pair Tested pair
			//! \return True if object is present on entity. False otherwise or if any of the entites is not valid.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			//! \warning Undefined behavior if \param entity changes archetype after ComponentSetter is created.
			GAIA_NODISCARD bool has(Entity entity, Pair pair) const {
				return has(entity, (Entity)pair);
			}

			//! Tells if \param entity contains the component \tparam T.
			//! \tparam T Component
			//! \param entity Entity
			//! \return True if the component is present on entity.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			//! \warning Undefined behavior if \param entity changes archetype after ComponentSetter is created.
			template <typename T>
			GAIA_NODISCARD bool has(Entity entity) const {
				GAIA_ASSERT(valid(entity));

				const auto& ec = m_recs.entities[entity.id()];
				if (is_req_del(ec))
					return false;

				return ComponentGetter{ec.pChunk, ec.row}.has<T>();
			}

			//----------------------------------------------------------------------

			//! Assigns a \param name to \param entity. Ignored if used with pair.
			//! The string is copied and kept internally.
			//! \param entity Entity
			//! \param name A null-terminated string
			//! \param len String length. If zero, the length is calculated
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			//! \warning Name is expected to be unique. If it is not this function does nothing.
			void name(Entity entity, const char* name, uint32_t len = 0) {
				name_inter<true>(entity, name, len);
			}

			//! Assigns a \param name to \param entity. Ignored if used with pair.
			//! The string is NOT copied. Your are responsible for its lifetime.
			//! \param entity Entity
			//! \param name Pointer to a stable null-terminated string
			//! \param len String length. If zero, the length is calculated
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			//! \warning Name is expected to be unique. If it is not this function does nothing.
			//! \warning In this case the string is NOT copied and NOT stored internally. You are responsible for its
			//!          lifetime. The pointer also needs to be stable. Otherwise, any time your storage tries to move
			//!          the string to a different place you have to unset the name before it happens and set it anew
			//!          after the move is done.
			void name_raw(Entity entity, const char* name, uint32_t len = 0) {
				name_inter<false>(entity, name, len);
			}

			//! Returns the name assigned to \param entity.
			//! \param entity Entity
			//! \return Name assigned to entity.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			GAIA_NODISCARD const char* name(Entity entity) const {
				if (entity.pair())
					return nullptr;

				if (!has<EntityDesc>(entity))
					return nullptr;

				const auto& desc = get<EntityDesc>(entity);
				return desc.name;
			}

			//! Returns the name assigned to \param entityId.
			//! \param entityId EntityId
			//! \return Name assigned to entity.
			//! \warning It is expected \param entityId is valid. Undefined behavior otherwise.
			GAIA_NODISCARD const char* name(EntityId entityId) const {
				auto entity = get(entityId);
				return name(entity);
			}

			//! Returns the entity that is assigned with the \param name.
			//! \param name Name
			//! \return Entity assigned the given name. EntityBad if there is nothing to return.
			GAIA_NODISCARD Entity get(const char* name) const {
				if (name == nullptr)
					return EntityBad;

				const auto it = m_nameToEntity.find(EntityNameLookupKey(name));
				if (it == m_nameToEntity.end())
					return EntityBad;

				return it->second;
			}

			//----------------------------------------------------------------------

			//! Returns relations for \param target.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			GAIA_NODISCARD const cnt::set<EntityLookupKey>* relations(Entity target) const {
				const auto it = m_targetsToRelations.find(EntityLookupKey(target));
				if (it == m_targetsToRelations.end())
					return nullptr;

				return &it->second;
			}

			//! Returns the first relationship relation for the \param target entity on \param entity.
			//! \return Relationship target. EntityBad if there is nothing to return.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			GAIA_NODISCARD Entity relation(Entity entity, Entity target) const {
				GAIA_ASSERT(valid(entity));
				if (!valid(target))
					return EntityBad;

				const auto& ec = fetch(entity);
				const auto* pArchetype = ec.pArchetype;

				// Early exit if there are no pairs on the archetype
				if (pArchetype->pairs() == 0)
					return EntityBad;

				auto ids = pArchetype->ids_view();
				for (auto e: ids) {
					if (!e.pair())
						continue;
					if (e.gen() != target.id())
						continue;

					const auto& ecRel = m_recs.entities[e.id()];
					auto relation = ecRel.pChunk->entity_view()[ecRel.row];
					return relation;
				}

				return EntityBad;
			}

			//! Returns the relationship relations for the \param target entity on \param entity.
			//! Appends all relationship relations if any to \param out.
			//! \param func void(Entity relation) functor executed for relationship relation found.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename Func>
			void relations(Entity entity, Entity target, Func func) const {
				GAIA_ASSERT(valid(entity));
				if (!valid(target))
					return;

				const auto& ec = fetch(entity);
				const auto* pArchetype = ec.pArchetype;

				// Early exit if there are no pairs on the archetype
				if (pArchetype->pairs() == 0)
					return;

				auto ids = pArchetype->ids_view();
				for (auto e: ids) {
					if (!e.pair())
						continue;
					if (e.gen() != target.id())
						continue;

					const auto& ecRel = m_recs.entities[e.id()];
					auto relation = ecRel.pChunk->entity_view()[ecRel.row];
					func(relation);
				}
			}

			template <typename Func>
			void as_relations_trav(Entity target, Func func) const {
				GAIA_ASSERT(valid(target));
				if (!valid(target))
					return;

				const auto it = m_entityToAsRelations.find(EntityLookupKey(target));
				if (it == m_entityToAsRelations.end())
					return;

				const auto& set = it->second;
				for (auto relation: set) {
					func(relation.entity());
					as_relations_trav(relation.entity(), func);
				}
			}

			template <typename Func>
			bool as_relations_trav_if(Entity target, Func func) const {
				GAIA_ASSERT(valid(target));
				if (!valid(target))
					return false;

				const auto it = m_entityToAsRelations.find(EntityLookupKey(target));
				if (it == m_entityToAsRelations.end())
					return false;

				const auto& set = it->second;
				for (auto relation: set) {
					if (func(relation.entity()))
						return true;
					if (as_relations_trav_if(relation.entity(), func))
						return true;
				}

				return false;
			}

			//----------------------------------------------------------------------

			//! Returns targets for \param relation.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			GAIA_NODISCARD const cnt::set<EntityLookupKey>* targets(Entity relation) const {
				const auto it = m_relationsToTargets.find(EntityLookupKey(relation));
				if (it == m_relationsToTargets.end())
					return nullptr;

				return &it->second;
			}

			//! Returns the first relationship target for the \param relation entity on \param entity.
			//! \return Relationship target. EntityBad if there is nothing to return.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			GAIA_NODISCARD Entity target(Entity entity, Entity relation) const {
				GAIA_ASSERT(valid(entity));
				if (!valid(relation))
					return EntityBad;

				const auto& ec = fetch(entity);
				const auto* pArchetype = ec.pArchetype;

				// Early exit if there are no pairs on the archetype
				if (pArchetype->pairs() == 0)
					return EntityBad;

				auto ids = pArchetype->ids_view();
				for (auto e: ids) {
					if (!e.pair())
						continue;
					if (e.id() != relation.id())
						continue;

					const auto& ecTarget = m_recs.entities[e.gen()];
					auto target = ecTarget.pChunk->entity_view()[ecTarget.row];
					return target;
				}

				return EntityBad;
			}

			//! Returns the relationship targets for the \param relation entity on \param entity.
			//! Appends all relationship targets if any to \param out.
			//! \param func void(Entity target) functor executed for relationship target found.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename Func>
			void targets(Entity entity, Entity relation, Func func) const {
				GAIA_ASSERT(valid(entity));
				if (!valid(relation))
					return;

				const auto& ec = fetch(entity);
				const auto* pArchetype = ec.pArchetype;

				// Early exit if there are no pairs on the archetype
				if (pArchetype->pairs() == 0)
					return;

				auto ids = pArchetype->ids_view();
				for (auto e: ids) {
					if (!e.pair())
						continue;
					if (e.id() != relation.id())
						continue;

					const auto& ecTarget = m_recs.entities[e.gen()];
					auto target = ecTarget.pChunk->entity_view()[ecTarget.row];
					func(target);
				}
			}

			template <typename Func>
			void as_targets_trav(Entity relation, Func func) const {
				GAIA_ASSERT(valid(relation));
				if (!valid(relation))
					return;

				const auto it = m_entityToAsTargets.find(EntityLookupKey(relation));
				if (it == m_entityToAsTargets.end())
					return;

				const auto& set = it->second;
				for (auto target: set) {
					func(target.entity());
					as_targets_trav(target.entity(), func);
				}
			}

			template <typename Func>
			bool as_targets_trav_if(Entity relation, Func func) const {
				GAIA_ASSERT(valid(relation));
				if (!valid(relation))
					return false;

				const auto it = m_entityToAsTargets.find(EntityLookupKey(relation));
				if (it == m_entityToAsTargets.end())
					return false;

				const auto& set = it->second;
				for (auto target: set) {
					if (func(target.entity()))
						return true;
					if (as_targets_trav(target.entity(), func))
						return true;
				}

				return false;
			}

			//----------------------------------------------------------------------

			//! Provides a query set up to work with the parent world.
			//! \tparam UseCache If true, results of the query are cached
			//! \return Valid query object
			template <bool UseCache = true>
			auto query() {
				if constexpr (UseCache) {
					Query q(
							*const_cast<World*>(this), m_queryCache,
							//
							m_nextArchetypeId, m_worldVersion, m_archetypesById, m_entityToArchetypeMap, m_archetypes);
					return q;
				} else {
					QueryUncached q(
							*const_cast<World*>(this),
							//
							m_nextArchetypeId, m_worldVersion, m_archetypesById, m_entityToArchetypeMap, m_archetypes);
					return q;
				}
			}

			//----------------------------------------------------------------------

			//! Enables or disables an entire entity.
			//! \param entity Entity
			//! \param enable Enable or disable the entity
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			void enable(Entity entity, bool enable) {
				GAIA_ASSERT(valid(entity));

				auto& ec = m_recs.entities[entity.id()];

				GAIA_ASSERT(
						(ec.pChunk && !ec.pChunk->locked()) &&
						"Entities can't be enabled/disabled while their chunk is being iterated "
						"(structural changes are forbidden during this time!)");

				auto& archetype = *ec.pArchetype;
				archetype.enable_entity(ec.pChunk, ec.row, enable, m_recs);
			}

			//! Checks if an entity is enabled.
			//! \param entity Entity
			//! \return True it the entity is enabled. False otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			bool enabled(Entity entity) const {
				GAIA_ASSERT(valid(entity));

				const auto& ec = m_recs.entities[entity.id()];
				const bool entityStateInContainer = !ec.dis;
#if GAIA_ASSERT_ENABLED
				const bool entityStateInChunk = ec.pChunk->enabled(ec.row);
				GAIA_ASSERT(entityStateInChunk == entityStateInContainer);
#endif
				return entityStateInContainer;
			}

			//----------------------------------------------------------------------

			//! Returns a chunk containing the \param entity.
			//! \return Chunk or nullptr if not found.
			GAIA_NODISCARD Chunk* get_chunk(Entity entity) const {
				GAIA_ASSERT(entity.id() < m_recs.entities.size());
				const auto& ec = m_recs.entities[entity.id()];
				return ec.pChunk;
			}

			//! Returns a chunk containing the \param entity.
			//! Index of the entity is stored in \param indexInChunk
			//! \return Chunk or nullptr if not found
			GAIA_NODISCARD Chunk* get_chunk(Entity entity, uint32_t& indexInChunk) const {
				GAIA_ASSERT(entity.id() < m_recs.entities.size());
				const auto& ec = m_recs.entities[entity.id()];
				indexInChunk = ec.row;
				return ec.pChunk;
			}

			//! Returns the number of active entities
			//! \return Entity
			GAIA_NODISCARD uint32_t size() const {
				return m_recs.entities.item_count();
			}

			//! Returns the current version of the world.
			//! \return World version number.
			GAIA_NODISCARD uint32_t& world_version() {
				return m_worldVersion;
			}

			//! Sets maximal lifespan of an archetype \param entity belongs to.
			//! \param lifespan How many world updates an empty archetype is kept.
			//!                 If zero, the archetype it kept indefinitely.
			void set_max_lifespan(Entity entity, uint32_t lifespan = Archetype::MAX_ARCHETYPE_LIFESPAN) {
				if (!valid(entity))
					return;

				auto& ec = fetch(entity);
				ec.pArchetype->set_max_lifespan(lifespan);
			}

			//----------------------------------------------------------------------

			//! Performs various internal operations related to the end of the frame such as
			//! memory cleanup and other managment operations which keep the system healthy.
			void update() {
				// Finish deleting entities
				del_finalize();

				// Run garbage collector
				gc();

				// Signal the end of the frame
				GAIA_PROF_FRAME();
			}

			//! Clears the world so that all its entities and components are released
			void cleanup() {
				cleanup_internal();

				// Reinit
				m_pRootArchetype = nullptr;
				m_pEntityArchetype = nullptr;
				m_pCompArchetype = nullptr;
				m_nextArchetypeId = 0;
				m_defragLastArchetypeIdx = 0;
				m_worldVersion = 0;
				init();
			}

			//! Sets the maximum number of entites defragmented per world tick
			//! \param value Number of entities to defragment
			void defrag_entities_per_tick(uint32_t value) {
				m_defragEntitesPerTick = value;
			}

			//--------------------------------------------------------------------------------

			//! Performs diagnostics on archetypes. Prints basic info about them and the chunks they contain.
			void diag_archetypes() const {
				GAIA_LOG_N("Archetypes:%u", (uint32_t)m_archetypes.size());
				for (auto* pArchetype: m_archetypes)
					Archetype::diag(*this, *pArchetype);
			}

			//! Performs diagnostics on registered components.
			//! Prints basic info about them and reports and detected issues.
			void diag_components() const {
				comp_cache().diag();
			}

			//! Performs diagnostics on entites of the world.
			//! Also performs validation of internal structures which hold the entities.
			void diag_entities() const {
				validate_entities();

				GAIA_LOG_N("Deleted entities: %u", (uint32_t)m_recs.entities.get_free_items());
				if (m_recs.entities.get_free_items() != 0U) {
					GAIA_LOG_N("  --> %u", (uint32_t)m_recs.entities.get_next_free_item());

					uint32_t iters = 0;
					auto fe = m_recs.entities[m_recs.entities.get_next_free_item()].idx;
					while (fe != IdentifierIdBad) {
						GAIA_LOG_N("  --> %u", m_recs.entities[fe].idx);
						fe = m_recs.entities[fe].idx;
						++iters;
						if (iters > m_recs.entities.get_free_items())
							break;
					}

					if ((iters == 0U) || iters > m_recs.entities.get_free_items())
						GAIA_LOG_E("  Entities recycle list contains inconsistent data!");
				}
			}

			//! Performs all diagnostics.
			void diag() const {
				diag_archetypes();
				diag_components();
				diag_entities();
			}

		private:
			//! Clears the world so that all its entities and components are released
			void cleanup_internal() {
				GAIA_PROF_SCOPE(World::cleanup_internal);

				// Clear entities
				m_recs.entities = {};
				m_recs.pairs = {};

				// Clear archetypes
				{
					// Delete all allocated chunks and their parent archetypes
					for (auto* pArchetype: m_archetypes)
						Archetype::destroy(pArchetype);

					m_entityToAsRelations = {};
					m_entityToAsTargets = {};
					m_targetsToRelations = {};
					m_relationsToTargets = {};

					m_archetypes = {};
					m_archetypesById = {};
					m_archetypesByHash = {};

					m_reqArchetypesToDel = {};
					m_reqEntitiesToDel = {};

					m_entitiesToDel = {};
					m_chunksToDel = {};
					m_archetypesToDel = {};
				}

				// Clear caches
				{
					m_entityToArchetypeMap = {};
					m_queryCache.clear();
				}

				// Clear entity names
				{
					for (auto& pair: m_nameToEntity) {
						if (!pair.first.owned())
							continue;
						// Release any memory allocated for owned names
						mem::mem_free((void*)pair.first.str());
					}
					m_nameToEntity = {};
				}

				// Clear component cache
				m_compCache.clear();
			}

			GAIA_NODISCARD bool valid(const EntityContainer& ec, [[maybe_unused]] Entity entityExpected) const {
				if (is_req_del(ec))
					return false;

				// The entity in the chunk must match the index in the entity container
				auto* pChunk = ec.pChunk;
				if (pChunk == nullptr || ec.row >= pChunk->size())
					return false;

#if GAIA_ASSERT_ENABLED
				const auto entityPresent = ec.pChunk->entity_view()[ec.row];
				GAIA_ASSERT(entityExpected == entityPresent);
				if (entityExpected != entityPresent)
					return false;
#endif

				return true;
			}

			//! Checks if the pair \param entity is valid.
			//! \return True if the entity is valid. False otherwise.
			GAIA_NODISCARD bool valid_pair(Entity entity) const {
				if (entity == EntityBad)
					return false;

				GAIA_ASSERT(entity.pair());
				if (!entity.pair())
					return false;

				// Ignore wildcards because they can't be attached to entities
				if (is_wildcard(entity))
					return true;

				const auto it = m_recs.pairs.find(EntityLookupKey(entity));
				if (it == m_recs.pairs.end())
					return false;

				const auto& ec = it->second;
				return valid(ec, entity);
			}

			//! Checks if the entity \param entity is valid.
			//! \return True if the entity is valid. False otherwise.
			GAIA_NODISCARD bool valid_entity(Entity entity) const {
				if (entity == EntityBad)
					return false;

				GAIA_ASSERT(!entity.pair());
				if (entity.pair())
					return false;

				// Entity ID has to fit inside the entity array
				if (entity.id() >= m_recs.entities.size())
					return false;

				const auto& ec = m_recs.entities[entity.id()];
				return valid(ec, entity);
			}

			//! Checks if the entity with id \param entityId is valid.
			//! Pairs are considered invalid.
			//! \return True if entityId is valid. False otherwise.
			GAIA_NODISCARD bool valid_entity_id(EntityId entityId) const {
				if (entityId == EntityBad.id())
					return false;

				// Entity ID has to fit inside the entity array
				if (entityId >= m_recs.entities.size())
					return false;

				const auto& ec = m_recs.entities[entityId];
				if (ec.pair != 0)
					return false;

				return valid(ec, Entity(entityId, ec.gen, (bool)ec.ent, (bool)ec.pair, (EntityKind)ec.kind));
			}

			//! Sorts archetypes in the archetype list with their ids in ascending order
			void sort_archetypes() {
				struct sort_cond {
					bool operator()(const Archetype* a, const Archetype* b) const {
						return a->id() < b->id();
					}
				};

				core::sort(m_archetypes, sort_cond{}, [&](uint32_t left, uint32_t right) {
					Archetype* tmp = m_archetypes[left];

					m_archetypes[right]->list_idx(left);
					m_archetypes[left]->list_idx(right);

					m_archetypes.data()[left] = (Archetype*)m_archetypes[right];
					m_archetypes.data()[right] = tmp;
				});
			}

			//! Remove an entity from its chunk.
			//! \param archetype Archetype we remove the entity from
			//! \param chunk Chunk we remove the entity from
			//! \param row Index of entity within its chunk
			void remove_entity(Archetype& archetype, Chunk& chunk, uint16_t row) {
				GAIA_PROF_SCOPE(World::remove_entity);

				archetype.remove_entity(chunk, row, m_recs, m_chunksToDel);
			}

			//! Delete an empty chunk from its archetype
			void del_empty_chunk(Chunk* pChunk) {
				GAIA_PROF_SCOPE(World::del_empty_chunk);

				GAIA_ASSERT(pChunk != nullptr);
				GAIA_ASSERT(pChunk->empty());
				GAIA_ASSERT(!pChunk->dying());

				cnt::sarr_ext<Entity, Chunk::MAX_COMPONENTS> ids;
				{
					auto recs = pChunk->comp_rec_view();
					ids.resize((uint32_t)recs.size());
					GAIA_EACH_(recs, j) ids[j] = recs[j].entity;
				}

				const auto hashLookup = calc_lookup_hash({ids.data(), ids.size()}).hash;
				auto* pArchetype = find_archetype({hashLookup}, {ids.data(), ids.size()});
				GAIA_ASSERT(pArchetype != nullptr);

				pArchetype->del(pChunk, m_archetypesToDel);
			}

			//! Delete all chunks which are empty (have no entities) and have not been used in a while
			void del_empty_chunks() {
				GAIA_PROF_SCOPE(World::del_empty_chunks);

				for (uint32_t i = 0; i < m_chunksToDel.size();) {
					auto* pChunk = m_chunksToDel[i];

					// Revive reclaimed chunks
					if (!pChunk->empty()) {
						pChunk->revive();
						core::erase_fast(m_chunksToDel, i);
						continue;
					}

					// Skip chunks which still have some lifespan left
					if (pChunk->progress_death()) {
						++i;
						continue;
					}

					// Delete unused chunks that are past their lifespan
					del_empty_chunk(pChunk);
					core::erase_fast(m_chunksToDel, i);
				}
			}

			//! Delete an archetype from the world
			void del_empty_archetype(Archetype* pArchetype) {
				GAIA_PROF_SCOPE(World::del_empty_archetype);

				GAIA_ASSERT(pArchetype != nullptr);
				GAIA_ASSERT(pArchetype->empty());
				GAIA_ASSERT(!pArchetype->dying() || pArchetype->is_req_del());

				unreg_archetype(pArchetype);
			}

			//! Delete all archetypes which are empty (have no used chunks) and have not been used in a while
			void del_empty_archetypes() {
				GAIA_PROF_SCOPE(World::del_empty_archetypes);

				cnt::sarr_ext<Archetype*, 512> tmp;

				for (uint32_t i = 0; i < m_archetypesToDel.size();) {
					auto* pArchetype = m_archetypesToDel[i];

					// Skip reclaimed archetypes
					if (!pArchetype->empty()) {
						revive_archetype(*pArchetype);
						core::erase_fast(m_archetypesToDel, i);
						continue;
					}

					// Skip archetypes which still has some lifespan left unless
					// they are force-deleted.
					if (!pArchetype->is_req_del() && pArchetype->progress_death()) {
						++i;
						continue;
					}

					tmp.push_back(pArchetype);

					// Remove the unused archetypes
					del_empty_archetype(pArchetype);
					core::erase_fast(m_archetypesToDel, i);
					Archetype::destroy(pArchetype);
				}

				// Remove all dead archetypes from query caches.
				// Because the number of cached queries is way higher than the number of archetypes
				// we want to remove, we flip the logic around and iterate over all query caches
				// and match against our lists.
				// Note, all archetype pointers in the tmp array are invalid at this point and can
				// be used only for comparison. They can't be dereferenced.
				if (!tmp.empty()) {
					// TODO: How to speed this up? If there are 1k cached queries is it still going to
					//       be fast enough or do we get spikes? Probably a linked list for query cache
					//       would be a way to go.
					for (auto& info: m_queryCache) {
						for (auto* pArchetype: tmp)
							info.remove(pArchetype);
					}
				}
			}

			void revive_archetype(Archetype& archetype) {
				archetype.revive();
				m_reqArchetypesToDel.erase(ArchetypeLookupKey(archetype.lookup_hash(), &archetype));
			}

			void revive_archetype(Entity entity) {
				auto& ec = fetch(entity);
				GAIA_ASSERT(ec.pArchetype != nullptr);
				GAIA_ASSERT(ec.pChunk != nullptr);

				ec.pChunk->revive();
				revive_archetype(*ec.pArchetype);
			}

			//! Defragments chunks.
			//! \param maxEntites Maximum number of entities moved per call
			void defrag_chunks(uint32_t maxEntities) {
				GAIA_PROF_SCOPE(World::defrag_chunks);

				const auto maxIters = m_archetypes.size();
				// There has to be at least the root archetype present
				GAIA_ASSERT(maxIters > 0);

				GAIA_FOR(maxIters) {
					const auto idx = (m_defragLastArchetypeIdx + 1) % maxIters;
					auto* pArchetype = m_archetypes[idx];
					pArchetype->defrag(maxEntities, m_chunksToDel, m_recs);
					if (maxEntities == 0)
						return;

					m_defragLastArchetypeIdx = idx;
				}
			}

			//! Searches for archetype with a given set of components
			//! \param hashLookup Archetype lookup hash
			//! \param ids Archetype entities/components
			//! \return Pointer to archetype or nullptr.
			GAIA_NODISCARD Archetype* find_archetype(Archetype::LookupHash hashLookup, EntitySpan ids) {
				auto tmpArchetype = ArchetypeLookupChecker(ids);
				ArchetypeLookupKey key(hashLookup, &tmpArchetype);

				// Search for the archetype in the map
				const auto it = m_archetypesByHash.find(key);
				if (it == m_archetypesByHash.end())
					return nullptr;

				auto* pArchetype = it->second;
				return pArchetype;
			}

			//! Adds the archetype to <entity, archetype> map for quick lookups of archetypes by comp/tag/pair
			//! \param entity Entity getting added
			//! \param pArchetype Linked archetype
			void add_entity_archetype_pair(Entity entity, Archetype* pArchetype) {
				const auto it = m_entityToArchetypeMap.find(EntityLookupKey(entity));
				if (it == m_entityToArchetypeMap.end())
					m_entityToArchetypeMap.try_emplace(EntityLookupKey(entity), ArchetypeDArray{pArchetype});
				else if (!core::has(it->second, pArchetype))
					it->second.push_back(pArchetype);
			}

			//! Deletes an archetype to <entity, archetype> record
			//! \param entity Entity getting deleted
			void del_entity_archetype_pair(Entity entity) {
				auto it = m_entityToArchetypeMap.find(EntityLookupKey(entity));
				auto& archetypes = it->second;
				for (int i = (int)archetypes.size() - 1; i >= 0; --i) {
					const auto* pArchetype = archetypes[(uint32_t)i];
					if (pArchetype->has(entity))
						continue;

					core::erase_fast_unsafe(archetypes, i);
				}
			}

			//! Deletes an archetype to <entity, archetype> record
			//! \param entity Entity getting deleted
			void del_entity_archetype_pairs(Entity entity) {
				// TODO: Optimize. Either switch to an array or add an index to the map value.
				//       Otherwise all these lookups make deleting entities slow.

				m_entityToArchetypeMap.erase(EntityLookupKey(entity));

				if (entity.pair()) {
					// Fake entities instantiated for both ids.
					// We are find with it because to build a pair all we need are valid entity ids.
					const auto first = Entity(entity.id(), 0, false, false, EntityKind::EK_Gen);
					const auto second = Entity(entity.gen(), 0, false, false, EntityKind::EK_Gen);

					// (*, tgt)
					del_entity_archetype_pair(Pair(All, second));
					// (src, *)
					del_entity_archetype_pair(Pair(first, All));
					// (*, *)
					del_entity_archetype_pair(Pair(All, All));
				}
			}

			//! Creates a new archetype from a given set of entities
			//! \param entities Archetype entities/components
			//! \return Pointer to the new archetype.
			GAIA_NODISCARD Archetype* create_archetype(EntitySpan entities) {
				auto* pArchetype = Archetype::create(*this, m_nextArchetypeId++, m_worldVersion, entities);

				for (auto entity: entities) {
					add_entity_archetype_pair(entity, pArchetype);

					// If the entity is a pair, make sure to create special wildcard records for it
					// as well so wildcard queries can find the archetype.
					if (entity.pair()) {
						// Fake entities instantiated for both ids.
						// We are find with it because to build a pair all we need are valid entity ids.
						const auto first = Entity(entity.id(), 0, false, false, EntityKind::EK_Gen);
						const auto second = Entity(entity.gen(), 0, false, false, EntityKind::EK_Gen);

						// (*, tgt)
						add_entity_archetype_pair(Pair(All, second), pArchetype);
						// (src, *)
						add_entity_archetype_pair(Pair(first, All), pArchetype);
						// (*, *)
						add_entity_archetype_pair(Pair(All, All), pArchetype);
					}
				}

				return pArchetype;
			}

			//! Registers the archetype in the world.
			//! \param pArchetype Archetype to register.
			void reg_archetype(Archetype* pArchetype) {
				GAIA_ASSERT(pArchetype != nullptr);

				// // Make sure hashes were set already
				// GAIA_ASSERT(
				// 		(m_archetypesById.empty() || pArchetype == m_pRootArchetype) || (pArchetype->lookup_hash().hash != 0));

				// Make sure the archetype is not registered yet
				GAIA_ASSERT(pArchetype->list_idx() == BadIndex);

				// Register the archetype
				m_archetypesById.emplace(ArchetypeIdLookupKey(pArchetype->id(), pArchetype->id_hash()), pArchetype);
				m_archetypesByHash.emplace(ArchetypeLookupKey(pArchetype->lookup_hash(), pArchetype), pArchetype);

				pArchetype->list_idx(m_archetypes.size());
				m_archetypes.emplace_back(pArchetype);
			}

			void unreg_archetype_raw(Archetype* pArchetype) {
				// Make sure the archetype was registered already
				GAIA_ASSERT(pArchetype->list_idx() != BadIndex);

				auto tmpArchetype = ArchetypeLookupChecker(pArchetype->ids_view());
				ArchetypeLookupKey key(pArchetype->lookup_hash(), &tmpArchetype);
				m_archetypesByHash.erase(key);
				m_archetypesById.erase(ArchetypeIdLookupKey(pArchetype->id(), pArchetype->id_hash()));

				const auto idx = pArchetype->list_idx();
				GAIA_ASSERT(idx == core::get_index(m_archetypes, pArchetype));
				core::erase_fast(m_archetypes, idx);
				if (!m_archetypes.empty() && idx != m_archetypes.size())
					m_archetypes[idx]->list_idx(idx);
			}

			//! Unregisters the archetype in the world.
			//! \param pArchetype Archetype to register.
			void unreg_archetype(Archetype* pArchetype) {
				GAIA_ASSERT(pArchetype != nullptr);

				// Make sure hashes were set already
				GAIA_ASSERT(
						(m_archetypesById.empty() || pArchetype == m_pRootArchetype) || (pArchetype->lookup_hash().hash != 0));

				unreg_archetype_raw(pArchetype);
			}

#if GAIA_ASSERT_ENABLED
			static void print_archetype_entities(const World& world, const Archetype& archetype, Entity entity, bool adding) {
				auto ids = archetype.ids_view();

				GAIA_LOG_W("Currently present:");
				GAIA_EACH(ids) {
					GAIA_LOG_W("> [%u] %s [%s]", i, entity_name(world, ids[i]), EntityKindString[(uint32_t)ids[i].kind()]);
				}

				GAIA_LOG_W("Trying to %s:", adding ? "add" : "del");
				GAIA_LOG_W("> %s [%s]", entity_name(world, entity), EntityKindString[(uint32_t)entity.kind()]);
			}

			static void verify_add(const World& world, Archetype& archetype, Entity entity, Entity addEntity) {
				// Makes sure no wildcard entities are added
				if (is_wildcard(addEntity)) {
					GAIA_ASSERT2(false, "Adding wildcard pairs is not supported");
					print_archetype_entities(world, archetype, addEntity, true);
					return;
				}
				// Make sure not to add too many entities/components
				auto ids = archetype.ids_view();
				if GAIA_UNLIKELY (ids.size() + 1 >= Chunk::MAX_COMPONENTS) {
					GAIA_ASSERT2(false, "Trying to add too many entities to entity!");
					GAIA_LOG_W("Trying to add an entity to entity [%u:%u] but there's no space left!", entity.id(), entity.gen());
					print_archetype_entities(world, archetype, addEntity, true);
					return;
				}
			}

			static void verify_del(const World& world, Archetype& archetype, Entity entity, Entity delEntity) {
				if GAIA_UNLIKELY (!archetype.has(delEntity)) {
					GAIA_ASSERT2(false, "Trying to remove an entity which wasn't added");
					GAIA_LOG_W("Trying to del an entity from entity [%u:%u] but it was never added", entity.id(), entity.gen());
					print_archetype_entities(world, archetype, delEntity, false);
				}
			}
#endif

			//! Searches for an archetype which is formed by adding entity \param entity of \param pArchetypeLeft.
			//! If no such archetype is found a new one is created.
			//! \param pArchetypeLeft Archetype we originate from.
			//! \param entity Enity we want to add.
			//! \return Archetype pointer.
			GAIA_NODISCARD Archetype* foc_archetype_add(Archetype* pArchetypeLeft, Entity entity) {
				// TODO: Add specialization for m_pCompArchetype and m_pEntityArchetype to make this faster
				//       E.g. when adding a new entity we only have to sort from position 2 because the first 2
				//			 indices are taken by the core component. Also hash calculation can be sped up this way.
				// // We don't want to store edges for the root archetype because the more components there are the longer
				// // it would take to find anything. Therefore, for the root archetype we always make a lookup.
				// // Compared to an ordinary lookup this path is stripped as much as possible.
				// if (pArchetypeLeft == m_pRootArchetype) {
				// 	Archetype* pArchetypeRight = nullptr;
				//
				// 	const auto hashLookup = calc_lookup_hash(EntitySpan{&entity, 1}).hash;
				// 	pArchetypeRight = find_archetype({hashLookup}, EntitySpan{&entity, 1});
				// 	if (pArchetypeRight == nullptr) {
				// 		pArchetypeRight = create_archetype(EntitySpan{&entity, 1});
				// 		pArchetypeRight->set_hashes({hashLookup});
				// 		pArchetypeRight->build_graph_edges_left(pArchetypeLeft, entity);
				// 		reg_archetype(pArchetypeRight);
				// 	}
				//
				// 	return pArchetypeRight;
				// }

				// Check if the component is found when following the "add" edges
				{
					const auto edge = pArchetypeLeft->find_edge_right(entity);
					if (edge != ArchetypeIdHashPairBad)
						return m_archetypesById[edge];
				}

				// Prepare a joint array of components of old + the newly added component
				cnt::sarray_ext<Entity, Chunk::MAX_COMPONENTS> entsNew;
				{
					auto entsOld = pArchetypeLeft->ids_view();
					entsNew.resize((uint32_t)entsOld.size() + 1);
					GAIA_EACH(entsOld) entsNew[i] = entsOld[i];
					entsNew[(uint32_t)entsOld.size()] = entity;
				}

				// Make sure to sort the components so we receive the same hash no matter the order in which components
				// are provided Bubble sort is okay. We're dealing with at most Chunk::MAX_COMPONENTS items.
				sort(entsNew, SortComponentCond{});

				// Once sorted we can calculate the hashes
				const auto hashLookup = calc_lookup_hash({entsNew.data(), entsNew.size()}).hash;
				auto* pArchetypeRight = find_archetype({hashLookup}, {entsNew.data(), entsNew.size()});
				if (pArchetypeRight == nullptr) {
					pArchetypeRight = create_archetype({entsNew.data(), entsNew.size()});
					pArchetypeRight->set_hashes({hashLookup});
					pArchetypeLeft->build_graph_edges(pArchetypeRight, entity);
					reg_archetype(pArchetypeRight);
				}

				return pArchetypeRight;
			}

			//! Searches for an archetype which is formed by removing entity \param entity from \param pArchetypeRight.
			//! If no such archetype is found a new one is created.
			//! \param pArchetypeRight Archetype we originate from.
			//! \param entity Component we want to remove.
			//! \return Pointer to archetype.
			GAIA_NODISCARD Archetype* foc_archetype_del(Archetype* pArchetypeRight, Entity entity) {
				// Check if the component is found when following the "del" edges
				{
					const auto edge = pArchetypeRight->find_edge_left(entity);
					if (edge != ArchetypeIdHashPairBad)
						return m_archetypesById[edge];
				}

				cnt::sarray_ext<Entity, Chunk::MAX_COMPONENTS> entsNew;
				auto entsOld = pArchetypeRight->ids_view();

				// Find the intersection
				for (const auto e: entsOld) {
					if (e == entity)
						continue;

					entsNew.push_back(e);
				}

				// Verify there was a change
				GAIA_ASSERT(entsNew.size() != entsOld.size());

				// Calculate the hashes
				const auto hashLookup = calc_lookup_hash({entsNew.data(), entsNew.size()}).hash;
				auto* pArchetype = find_archetype({hashLookup}, {entsNew.data(), entsNew.size()});
				if (pArchetype == nullptr) {
					pArchetype = create_archetype({entsNew.data(), entsNew.size()});
					pArchetype->set_hashes({hashLookup});
					pArchetype->build_graph_edges(pArchetypeRight, entity);
					reg_archetype(pArchetype);
				}

				return pArchetype;
			}

			//! Returns an array of archetypes registered in the world
			//! \return Array or archetypes.
			const auto& archetypes() const {
				return m_archetypesById;
			}

			//! Returns the archetype the entity belongs to.
			//! \param entity Entity
			//! \return Reference to the archetype.
			GAIA_NODISCARD Archetype& archetype(Entity entity) {
				const auto& ec = fetch(entity);
				return *ec.pArchetype;
			}

			//! Removes any name associated with the entity
			//! \param entity Entity the name of which we want to delete
			void del_name(Entity entity) {
				if (entity.pair())
					return;

				auto& ec = fetch(entity);
				if (!ec.pChunk->has<EntityDesc>())
					return;

				auto& entityDesc = ec.pChunk->sview_mut<EntityDesc>()[ec.row];
				if (entityDesc.name == nullptr)
					return;

				const auto it = m_nameToEntity.find(EntityNameLookupKey(entityDesc.name, entityDesc.len));
				// If the assert is hit it means the pointer to the name string was invalidated or became dangling.
				// That should not be possible for strings managed internally so the only other option is user-managed
				// strings are broken.
				GAIA_ASSERT(it != m_nameToEntity.end());
				if (it != m_nameToEntity.end()) {
					// Release memory allocated for the string if we own it
					if (it->first.owned())
						mem::mem_free((void*)entityDesc.name);

					m_nameToEntity.erase(it);
				}
				entityDesc.name = nullptr;
			}

			//! Deletes an entity along with all data associated with it.
			//! \param entity Entity to delete
			void del_entity(Entity entity, bool invalidate = true) {
				if (entity.pair() || entity == EntityBad)
					return;

				const auto& ec = fetch(entity);
				GAIA_ASSERT(entity.id() > GAIA_ID(LastCoreComponent).id());

				// if (!is_req_del(ec))
				{
					if (m_recs.entities.item_count() == 0)
						return;

#if GAIA_ASSERT_ENABLED
					auto* pChunk = ec.pChunk;
					GAIA_ASSERT(pChunk != nullptr);
#endif

					// Remove the entity from its chunk.
					// We call del_name first because remove_entity calls component destructors.
					// If the call was made inside invalidate_entity we would access a memory location
					// which has already been destructed which is not nice.
					del_name(entity);
					remove_entity(*ec.pArchetype, *ec.pChunk, ec.row);
				}

				// Invalidate on-demand.
				// We delete as a separate step in the delayed deletion.
				if (invalidate)
					invalidate_entity(entity);
			}

			//! Deletes all entites (and in turn chunks) from \param archetype.
			//! If an archetype forming entity is present, the chunk is treated as if it were empty
			//! and normal dying procedure is applied to it. At the last dying tick the entity is
			//! deleted so the chunk can be removed.
			void del_entities(Archetype& archetype) {
				for (auto* pChunk: archetype.chunks()) {
					auto ids = pChunk->entity_view();
					for (auto e: ids) {
						if (!valid(e))
							continue;

#if GAIA_ASSERT_ENABLED
						const auto& ec = fetch(e);

						// We should never end up trying to delete a forbidden-to-delete entity
						GAIA_ASSERT((ec.flags & EntityContainerFlags::OnDeleteTarget_Error) == 0);
#endif

						del_entity(e);
					}

					validate_chunk(pChunk);

					// If the chunk was already dying we need to remove it from the delete list
					// because we can delete it right away.
					// TODO: Instead of searching for it we could store a delete index in the chunk
					//       header. This way the lookup is O(1) instead of O(N) and it will help
					//       with edge-cases (tons of chunks removed at the same time).
					if (pChunk->dying()) {
						const auto idx = core::get_index(m_chunksToDel, pChunk);
						if (idx != BadIndex)
							core::erase_fast(m_chunksToDel, idx);
					}

					archetype.del(pChunk, m_archetypesToDel);
				}

				validate_entities();
			}

			GAIA_NODISCARD bool archetype_cond_match(Archetype& archetype, Pair cond, Entity target) const {
				// E.g.:
				//   target = (All, entity)
				//   cond = (OnDeleteTarget, delete)
				// Delete the entity if it matches the cond
				auto ids = archetype.ids_view();

				if (target.pair()) {
					for (auto e: ids) {
						// Find the pair which matches (All, entity)
						if (!e.pair())
							continue;
						if (e.gen() != target.gen())
							continue;

						const auto& ec = m_recs.entities[e.id()];
						const auto entity = ec.pChunk->entity_view()[ec.row];
						if (!has(entity, cond))
							continue;

						return true;
					}
				} else {
					for (auto e: ids) {
						if (e.pair())
							continue;
						if (!has(e, cond))
							continue;

						return true;
					}
				}

				return false;
			}

			//! Updates all chunks and entities of archetype \param srcArchetype so they are a part of \param dstArchetype
			void move_to_archetype(Archetype& srcArchetype, Archetype& dstArchetype) {
				GAIA_ASSERT(&srcArchetype != &dstArchetype);

				for (auto* pSrcChunk: srcArchetype.chunks()) {
					auto srcEnts = pSrcChunk->entity_view();
					if (srcEnts.empty())
						continue;

					// Copy entities back-to-front to avoid unnecessary data movements.
					// TODO: Handle disabled entities efficiently.
					//       If there are disabled entites, we still do data movements if there already
					//       are enabled entities in the chunk.
					// TODO: If the header was of some fixed size, e.g. if we always acted as if we had
					//       Chunk::MAX_COMPONENTS, certain data movements could be done pretty much instantly.
					//       E.g. when removing tags or pairs, we would simply replace the chunk pointer
					//       with a pointer to another one. The same goes for archetypes. Component data
					//       would not have to move at all internal chunk header pointers would remain unchanged.

					int i = (int)(srcEnts.size() - 1);
					while (i >= 0) {
						auto* pDstChunk = dstArchetype.foc_free_chunk();
						move_entity(srcEnts[(uint32_t)i], dstArchetype, *pDstChunk);

						--i;
					}
				}
			}

			//! Find the destination archetype \param pArchetype as if removing \param entity.
			//! \return Destination archetype of nullptr if no match is found
			GAIA_NODISCARD Archetype* calc_dst_archetype_ent(Archetype* pArchetype, Entity entity) {
				GAIA_ASSERT(!is_wildcard(entity));

				auto ids = pArchetype->ids_view();
				for (auto id: ids) {
					if (id != entity)
						continue;

					return foc_archetype_del(pArchetype, id);
				}

				return nullptr;
			}

			//! Find the destination archetype \param pArchetype as if removing all entities
			//! matching (All, entity) from the archetype.
			//! \return Destination archetype of nullptr if no match is found
			GAIA_NODISCARD Archetype* calc_dst_archetype_all_ent(Archetype* pArchetype, Entity entity) {
				GAIA_ASSERT(is_wildcard(entity));

				Archetype* pDstArchetype = pArchetype;

				auto ids = pArchetype->ids_view();
				for (auto id: ids) {
					if (!id.pair() || id.gen() != entity.gen())
						continue;

					pDstArchetype = foc_archetype_del(pDstArchetype, id);
				}

				return pArchetype != pDstArchetype ? pDstArchetype : nullptr;
			}

			//! Find the destination archetype \param pArchetype as if removing all entities
			//! matching (entity, All) from the archetype.
			//! \return Destination archetype of nullptr if no match is found
			GAIA_NODISCARD Archetype* calc_dst_archetype_ent_all(Archetype* pArchetype, Entity entity) {
				GAIA_ASSERT(is_wildcard(entity));

				Archetype* pDstArchetype = pArchetype;

				auto ids = pArchetype->ids_view();
				for (auto id: ids) {
					if (!id.pair() || id.id() != entity.id())
						continue;

					pDstArchetype = foc_archetype_del(pDstArchetype, id);
				}

				return pArchetype != pDstArchetype ? pDstArchetype : nullptr;
			}

			//! Find the destination archetype \param pArchetype as if removing all entities
			//! matching (All, All) from the archetype.
			//! \return Destination archetype of nullptr if no match is found
			GAIA_NODISCARD Archetype* calc_dst_archetype_all_all(Archetype* pArchetype, [[maybe_unused]] Entity entity) {
				GAIA_ASSERT(is_wildcard(entity));

				Archetype* pDstArchetype = pArchetype;
				bool found = false;

				auto ids = pArchetype->ids_view();
				for (auto id: ids) {
					if (!id.pair())
						continue;

					pDstArchetype = foc_archetype_del(pDstArchetype, id);
					found = true;
				}

				return found ? pDstArchetype : nullptr;
			}

			//! Find the destination archetype \param pArchetype as if removing \param entity.
			//! Wildcard pair entities are supported as well.
			//! \return Destination archetype of nullptr if no match is found
			GAIA_NODISCARD Archetype* calc_dst_archetype(Archetype* pArchetype, Entity entity) {
				if (entity.pair()) {
					auto rel = entity.id();
					auto tgt = entity.gen();

					// Removing a wildcard pair. We need to find all pairs matching it.
					if (rel == All.id() || tgt == All.id()) {
						// (first, All) means we need to match (first, A), (first, B), ...
						if (rel != All.id() && tgt == All.id())
							return calc_dst_archetype_ent_all(pArchetype, entity);

						// (All, second) means we need to match (A, second), (B, second), ...
						if (rel == All.id() && tgt != All.id())
							return calc_dst_archetype_all_ent(pArchetype, entity);

						// (All, All) means we need to match all relationships
						return calc_dst_archetype_all_all(pArchetype, EntityBad);
					}
				}

				// Non-wildcard pair or entity
				return calc_dst_archetype_ent(pArchetype, entity);
			}

			void req_del(Archetype& archetype) {
				if (archetype.is_req_del())
					return;

				archetype.req_del();
				m_reqArchetypesToDel.insert(ArchetypeLookupKey(archetype.lookup_hash(), &archetype));
			}

			void req_del(Entity entity) {
				auto& ec = fetch(entity);
				if (is_req_del(ec))
					return;

				// del_name(entity);
				del_entity(entity, false);

				ec.req_del();
				m_reqEntitiesToDel.insert(EntityLookupKey(entity));
			}

			//! Requests deleting anything that references the \param entity. No questions asked.
			void req_del_entities_with(Entity entity) {
				GAIA_PROF_SCOPE(World::req_del_entities_with);

				const auto it = m_entityToArchetypeMap.find(EntityLookupKey(entity));
				if (it == m_entityToArchetypeMap.end())
					return;

				const auto& archetypes = it->second;
				for (auto* pArchetype: archetypes)
					req_del(*pArchetype);
			}

			//! Requests deleting anything that references the \param entity. No questions asked.
			//! Takes \param cond into account.
			void req_del_entities_with(Entity entity, Pair cond) {
				GAIA_PROF_SCOPE(World::req_del_entities_with);

				const auto it = m_entityToArchetypeMap.find(EntityLookupKey(entity));
				if (it == m_entityToArchetypeMap.end())
					return;

				const auto& archetypes = it->second;
				for (auto* pArchetype: archetypes) {
					// Evaluate the conditon if a valid pair is given
					if (!archetype_cond_match(*pArchetype, cond, entity))
						continue;

					req_del(*pArchetype);
				}
			}

			//! Removes the entity \param entity from anything referencing it.
			void rem_from_entities(Entity entity) {
				GAIA_PROF_SCOPE(World::rem_from_entities);

				const auto it = m_entityToArchetypeMap.find(EntityLookupKey(entity));
				if (it == m_entityToArchetypeMap.end())
					return;

				// Invalidate the singleton status if necessary
				if (!entity.pair()) {
					auto& ec = fetch(entity);
					if ((ec.flags & EntityContainerFlags::IsSingleton) != 0) {
						auto ids = ec.pArchetype->ids_view();
						const auto idx = core::get_index(ids, entity);
						if (idx != BadIndex)
							EntityBuilder::updateFlag(ec.flags, EntityContainerFlags::IsSingleton, false);
					}
				}

				// Update archetypes of all affected entities
				const auto& archetypes = it->second;
				for (auto* pArchetype: archetypes) {
					if (pArchetype->is_req_del())
						continue;

					auto* pDstArchetype = calc_dst_archetype(pArchetype, entity);
					if (pDstArchetype != nullptr)
						move_to_archetype(*pArchetype, *pDstArchetype);
				}
			}

			//! Removes the entity \param entity from anything referencing it.
			//! Takes \param cond into account.
			void rem_from_entities(Entity entity, Pair cond) {
				GAIA_PROF_SCOPE(World::rem_from_entities);

				const auto it = m_entityToArchetypeMap.find(EntityLookupKey(entity));
				if (it == m_entityToArchetypeMap.end())
					return;

				// Invalidate the singleton status if necessary
				if (!entity.pair()) {
					auto& ec = fetch(entity);
					if ((ec.flags & EntityContainerFlags::IsSingleton) != 0) {
						auto ids = ec.pArchetype->ids_view();
						const auto idx = core::get_index(ids, entity);
						if (idx != BadIndex)
							EntityBuilder::updateFlag(ec.flags, EntityContainerFlags::IsSingleton, false);
					}
				}

				const auto& archetypes = it->second;
				for (auto* pArchetype: archetypes) {
					if (pArchetype->is_req_del())
						continue;

					// Evaluate the conditon if a valid pair is given
					if (!archetype_cond_match(*pArchetype, cond, entity))
						continue;

					auto* pDstArchetype = calc_dst_archetype(pArchetype, entity);
					if (pDstArchetype != nullptr)
						move_to_archetype(*pArchetype, *pDstArchetype);
				}
			}

			//! Handles specific conditions that might arise from deleting \param entity.
			//! Conditions are:
			//!   OnDelete - deleting an entity/pair
			//!   OnDeleteTarget - deleting a pair's target
			//! Reactions are:
			//!   Remove - removes the entity/pair from anything referencing it
			//!   Delete - delete everything referencing the entity
			//!   Error - error out when deleted
			//! These rules can be set up as:
			//!   e.add(Pair(OnDelete, Remove));
			void handle_del_entity(Entity entity) {
				GAIA_PROF_SCOPE(World::handle_del_entity);

				GAIA_ASSERT(!is_wildcard(entity));

				if (entity.pair()) {
					const auto& ec = fetch(entity);
					if ((ec.flags & EntityContainerFlags::OnDelete_Error) != 0) {
						GAIA_ASSERT2(false, "Trying to delete entity that is forbidden to be deleted");
						GAIA_LOG_E(
								"Trying to delete pair [%u.%u] %s [%s] that is forbidden to be deleted", entity.id(), entity.gen(),
								name(entity), EntityKindString[entity.kind()]);
						return;
					}

					const auto tgt = get(entity.gen());
					const auto& ecTgt = fetch(tgt);
					if ((ecTgt.flags & EntityContainerFlags::OnDeleteTarget_Error) != 0) {
						GAIA_ASSERT2(false, "Trying to delete entity that is forbidden to be deleted (target restricion)");
						GAIA_LOG_E(
								"Trying to delete pair [%u.%u] %s [%s] that is forbidden to be deleted (target restricion)",
								entity.id(), entity.gen(), name(entity), EntityKindString[entity.kind()]);
						return;
					}

					if ((ecTgt.flags & EntityContainerFlags::OnDeleteTarget_Delete) != 0) {
						// Delete all entities referencing this one as a relationship pair's target
						req_del_entities_with(Pair(All, tgt), Pair(OnDeleteTarget, Delete));
					} else {
						// Remove from all entities referencing this one as a relationship pair's target
						rem_from_entities(Pair(All, tgt));
					}

					// This entity is supposed to be deleted. Nothing more for us to do here
					if (is_req_del(ec))
						return;

					if ((ec.flags & EntityContainerFlags::OnDelete_Delete) != 0) {
						// Delete all references to the entity
						req_del_entities_with(entity);
					} else {
						// Entities are only removed by default
						rem_from_entities(entity);
					}
				} else {
					const auto& ec = fetch(entity);
					if ((ec.flags & EntityContainerFlags::OnDelete_Error) != 0) {
						GAIA_ASSERT2(false, "Trying to delete entity that is forbidden to be deleted");
						GAIA_LOG_E(
								"Trying to delete entity [%u.%u] %s [%s] that is forbidden to be deleted", entity.id(), entity.gen(),
								name(entity), EntityKindString[entity.kind()]);
						return;
					}

					if ((ec.flags & EntityContainerFlags::OnDeleteTarget_Error) != 0) {
						GAIA_ASSERT2(false, "Trying to delete entity that is forbidden to be deleted (a pair's target)");
						GAIA_LOG_E(
								"Trying to delete entity [%u.%u] %s [%s] that is forbidden to be deleted (a pair's target)",
								entity.id(), entity.gen(), name(entity), EntityKindString[entity.kind()]);
						return;
					}

					if ((ec.flags & EntityContainerFlags::OnDeleteTarget_Delete) != 0) {
						// Delete all entities referencing this one as a relationship pair's target
						req_del_entities_with(Pair(All, entity), Pair(OnDeleteTarget, Delete));
					} else {
						// Remove from all entities referencing this one as a relationship pair's target
						rem_from_entities(Pair(All, entity));
					}

					// This entity is supposed to be deleted. Nothing more for us to do here
					if (is_req_del(ec))
						return;

					if ((ec.flags & EntityContainerFlags::OnDelete_Delete) != 0) {
						// Delete all references to the entity
						req_del_entities_with(entity);
					} else {
						// Entities are only removed by default
						rem_from_entities(entity);
					}
				}
			}

			//! Deletes any edges containing the entity from the archetype graph.
			//! \param entity Entity to delete
			void del_graph_edges(Entity entity) {
				auto removeEdges = [&](Entity entityToRemove) {
					const auto it = m_entityToArchetypeMap.find(EntityLookupKey(entityToRemove));
					if (it == m_entityToArchetypeMap.end())
						return;

					const auto& archetypes = it->second;
					for (auto* pArchetype: archetypes) {
						const auto leftId = pArchetype->find_edge_left(entityToRemove);
						const auto itLeft = m_archetypesById.find(leftId);

						// The edge might have been deleted aleady
						if (itLeft == m_archetypesById.end())
							continue;

						auto* pArchetypeLeft = itLeft->second;
						GAIA_ASSERT(pArchetypeLeft != nullptr);
						pArchetypeLeft->del_graph_edges(pArchetype, entityToRemove);
					}
				};

				removeEdges(entity);

				// Make sure to remove all pairs containing the entity
				if (!entity.pair()) {
					// (X, something)
					const auto* tgts = targets(entity);
					if (tgts != nullptr) {
						for (auto target: *tgts)
							removeEdges(Pair(entity, target.entity()));
					}
					// (something, X)
					const auto* rels = relations(entity);
					if (rels != nullptr) {
						for (auto relation: *rels)
							removeEdges(Pair(relation.entity(), entity));
					}
				}
			}

			void del_reltgt_tgtrel_pairs(Entity entity) {
				auto delPair = [](PairMap& map, Entity source, Entity remove) {
					auto itTargets = map.find(EntityLookupKey(source));
					if (itTargets != map.end()) {
						auto& targets = itTargets->second;
						targets.erase(EntityLookupKey(remove));
					}
				};

				if (entity.pair()) {
					const auto it = m_recs.pairs.find(EntityLookupKey(entity));
					if (it != m_recs.pairs.end()) {
						// Delete the container record
						m_recs.pairs.erase(it);

						// Update pairs
						auto rel = get(entity.id());
						auto tgt = get(entity.gen());

						delPair(m_relationsToTargets, rel, tgt);
						delPair(m_relationsToTargets, All, tgt);
						delPair(m_targetsToRelations, tgt, rel);
						delPair(m_targetsToRelations, All, rel);
					}
				} else {
					// Update the container record
					auto& ec = m_recs.entities.free(entity);

					// If this is a singleton entity its archetype needs to be deleted
					if ((ec.flags & EntityContainerFlags::IsSingleton) != 0)
						req_del(*ec.pArchetype);

					ec.pArchetype = nullptr;
					ec.pChunk = nullptr;
					EntityBuilder::updateFlag(ec.flags, EntityContainerFlags::DeleteRequested, false);

					// Update pairs
					delPair(m_relationsToTargets, All, entity);
					delPair(m_targetsToRelations, All, entity);
					m_relationsToTargets.erase(EntityLookupKey(entity));
					m_targetsToRelations.erase(EntityLookupKey(entity));
				}
			}

			//! Invalidates the entity record, effectivelly deleting it.
			//! \param entity Entity to delete
			void invalidate_entity(Entity entity) {
				del_graph_edges(entity);
				del_reltgt_tgtrel_pairs(entity);
				del_entity_archetype_pairs(entity);
			}

			//! Associates an entity with a chunk.
			//! \param entity Entity to associate with a chunk
			//! \param pArchetype Target archetype
			//! \param pChunk Target chunk (with the archetype \param pArchetype)
			void store_entity(EntityContainer& ec, Entity entity, Archetype* pArchetype, Chunk* pChunk) {
				GAIA_ASSERT(pArchetype != nullptr);
				GAIA_ASSERT(pChunk != nullptr);
				GAIA_ASSERT(
						!pChunk->locked() && "Entities can't be added while their chunk is being iterated "
																 "(structural changes are forbidden during this time!)");

				ec.pArchetype = pArchetype;
				ec.pChunk = pChunk;
				ec.row = pChunk->add_entity(entity);
				GAIA_ASSERT(entity.pair() || ec.gen == entity.gen());
				ec.dis = 0;
			}

			//! Moves an entity along with all its generic components from its current chunk to another one.
			//! \param entity Entity to move
			//! \param targetArchetype Target archetype
			//! \param targetChunk Target chunk
			void move_entity(Entity entity, Archetype& targetArchetype, Chunk& targetChunk) {
				GAIA_PROF_SCOPE(World::move_entity);

				auto* pNewChunk = &targetChunk;

				auto& ec = fetch(entity);
				auto* pOldChunk = ec.pChunk;

				GAIA_ASSERT(pNewChunk != pOldChunk);

				const auto oldRow0 = ec.row;
				const auto newRow = pNewChunk->add_entity(entity);
				const bool wasEnabled = !ec.dis;

				auto& oldArchetype = *ec.pArchetype;
				auto& newArchetype = targetArchetype;

				// Make sure the old entity becomes enabled now
				oldArchetype.enable_entity(pOldChunk, oldRow0, true, m_recs);
				// Enabling the entity might have changed its chunk index so fetch it again
				const auto oldRow = ec.row;

				// Move data from the old chunk to the new one
				if (newArchetype.id() == oldArchetype.id()) {
					pNewChunk->move_entity_data(entity, newRow, m_recs);
				} else {
					pNewChunk->move_foreign_entity_data(pOldChunk, oldRow, pNewChunk, newRow);
				}

				// Remove the entity record from the old chunk
				remove_entity(oldArchetype, *pOldChunk, oldRow);

				// An entity might have moved, try updating the free chunk index
				newArchetype.try_update_free_chunk_idx();

				// Bring the entity container record up-to-date
				ec.pArchetype = &newArchetype;
				ec.pChunk = pNewChunk;
				ec.row = (uint16_t)newRow;

				// Make the enabled state in the new chunk match the original state
				newArchetype.enable_entity(pNewChunk, newRow, wasEnabled, m_recs);

				// End-state validation
				GAIA_ASSERT(valid(entity));
				validate_chunk(pOldChunk);
				validate_chunk(pNewChunk);
				validate_entities();
			}

			//! Moves an entity along with all its generic components from its current chunk to another one in a new
			//! archetype. \param entity Entity to move \param newArchetype Target archetype
			void move_entity(Entity entity, Archetype& newArchetype) {
				// Archetypes need to be different
				const auto& ec = fetch(entity);
				if (ec.pArchetype == &newArchetype)
					return;

				auto* pNewChunk = newArchetype.foc_free_chunk();
				move_entity(entity, newArchetype, *pNewChunk);
			}

			//! Verifies than the implicit linked list of entities is valid
			void validate_entities() const {
#if GAIA_ECS_VALIDATE_ENTITY_LIST
				m_recs.entities.validate();
#endif
			}

			//! Verifies that the chunk is valid
			void validate_chunk([[maybe_unused]] Chunk* pChunk) const {
#if GAIA_ECS_VALIDATE_CHUNKS && GAIA_ASSERT_ENABLED
				GAIA_ASSERT(pChunk != nullptr);

				if (!pChunk->empty()) {
					// Make sure a proper amount of entities reference the chunk
					uint32_t cnt = 0;
					for (const auto& ec: m_recs.entities) {
						if (ec.pChunk != pChunk)
							continue;
						++cnt;
					}
					for (const auto& pair: m_recs.pairs) {
						if (pair.second.pChunk != pChunk)
							continue;
						++cnt;
					}
					GAIA_ASSERT(cnt == pChunk->size());
				} else {
					// Make sure no entites reference the chunk
					for (const auto& ec: m_recs.entities) {
						GAIA_ASSERT(ec.pChunk != pChunk);
					}
					for (const auto& pair: m_recs.pairs) {
						GAIA_ASSERT(pair.second.pChunk != pChunk);
					}
				}
#endif
			}

			template <bool IsOwned>
			void name_inter(Entity entity, const char* name, uint32_t len) {
				GAIA_ASSERT(!entity.pair());
				if (entity.pair())
					return;

				GAIA_ASSERT(valid(entity));

				if (name == nullptr) {
					GAIA_ASSERT(len == 0);
					del_name(entity);
					return;
				}

				add<EntityDesc>(entity);

				auto res =
						m_nameToEntity.try_emplace(len == 0 ? EntityNameLookupKey(name) : EntityNameLookupKey(name, len), entity);
				// Make sure the name is unique. Ignore setting the same name twice on the same entity
				GAIA_ASSERT(res.second || res.first->second == entity);

				// Not a unique name, nothing to do
				if (!res.second)
					return;

				auto& key = res.first->first;

				if constexpr (IsOwned) {
					// Allocate enough storage for the name
					char* entityStr = (char*)mem::mem_alloc(key.len() + 1);
					memcpy((void*)entityStr, (const void*)name, key.len() + 1);
					entityStr[key.len()] = 0;

					// Update the map so it points to the newly allocated string.
					// We replace the pointer we provided in try_emplace with an internally allocated string.
					auto p = robin_hood::pair(std::make_pair(EntityNameLookupKey(entityStr, key.len(), 1, {key.hash()}), entity));
					res.first->swap(p);

					// Update the entity string pointer
					sset<EntityDesc>(entity) = {entityStr, key.len()};
				} else {
					// We tell the map the string is non-owned.
					auto p = robin_hood::pair(std::make_pair(EntityNameLookupKey(key.str(), key.len(), 0, {key.hash()}), entity));
					res.first->swap(p);

					// Update the entity string pointer
					sset<EntityDesc>(entity) = {name, key.len()};
				}
			}

			//! If \tparam CheckIn is true, checks if \param entity inherits from \param entityBase.
			//! If \tparam CheckIn is false, checks if \param entity is located in \param entityBase.
			//! True if \param entity inherits from/is located in \param entityBase. False otherwise.
			template <bool CheckIn>
			GAIA_NODISCARD bool is_inter(Entity entity, Entity entityBase) const {
				GAIA_ASSERT(valid_entity(entity));
				GAIA_ASSERT(valid_entity(entityBase));

				// Pairs are not supported
				if (entity.pair() || entityBase.pair())
					return false;

				if constexpr (!CheckIn) {
					if (entity == entityBase)
						return true;
				}

				const auto& ec = m_recs.entities[entity.id()];
				const auto* pArchetype = ec.pArchetype;

				// Early exit if there are no Is relationship pairs on the archetype
				if (pArchetype->pairs_is() == 0)
					return false;

				for (uint32_t i = 0; i < pArchetype->pairs_is(); ++i) {
					auto e = pArchetype->entity_from_pairs_as_idx(i);
					const auto& ecTarget = m_recs.entities[e.gen()];
					auto target = ecTarget.pChunk->entity_view()[ecTarget.row];
					if (target == entityBase)
						return true;

					if (is_inter<CheckIn>(target, entityBase))
						return true;
				}

				return false;
			}

			template <typename T>
			const ComponentCacheItem& reg_core_entity(Entity id, Archetype* pArchetype) {
				auto comp = add(*pArchetype, id.entity(), id.pair(), id.kind());
				const auto& ci = comp_cache_mut().add<T>(id);
				GAIA_ASSERT(ci.entity == id);
				GAIA_ASSERT(comp == id);
				(void)comp;
				return ci;
			}

			template <typename T>
			const ComponentCacheItem& reg_core_entity(Entity id) {
				return reg_core_entity<T>(id, m_pRootArchetype);
			}

			void init() {
				// Register the root archetype
				{
					m_pRootArchetype = create_archetype({});
					m_pRootArchetype->set_hashes({calc_lookup_hash({})});
					reg_archetype(m_pRootArchetype);
				}

				(void)reg_core_entity<Core_>(Core);

				// Entity archetype matches the root archetype for now
				m_pEntityArchetype = m_pRootArchetype;

				// Register the component archetype (entity + EntityDesc + Component)
				{
					Archetype* pCompArchetype{};
					{
						const auto id = GAIA_ID(EntityDesc);
						const auto& ci = reg_core_entity<EntityDesc>(id);
						EntityBuilder(*this, id).add(ci.entity);
						sset<EntityDesc>(id) = {ci.name.str(), ci.name.len()};
						pCompArchetype = m_recs.entities[id.id()].pArchetype;
					}
					{
						const auto id = GAIA_ID(Component);
						const auto& ci = reg_core_entity<Component>(id, pCompArchetype);
						EntityBuilder(*this, id).add(ci.entity);
						acc_mut(id)
								// Entity descriptor
								.sset<EntityDesc>({ci.name.str(), ci.name.len()})
								// Component
								.sset<Component>(ci.comp);
						m_pCompArchetype = m_recs.entities[id.id()].pArchetype;
					}
				}

				// Core components
				{
					(void)reg_core_entity<OnDelete_>(OnDelete);
					(void)reg_core_entity<OnDeleteTarget_>(OnDeleteTarget);
					(void)reg_core_entity<Remove_>(Remove);
					(void)reg_core_entity<Delete_>(Delete);
					(void)reg_core_entity<Error_>(Error);
					(void)reg_core_entity<DependsOn_>(DependsOn);
					(void)reg_core_entity<CantCombine_>(CantCombine);
					(void)reg_core_entity<Acyclic_>(Acyclic);
					(void)reg_core_entity<All_>(All);
					(void)reg_core_entity<ChildOf_>(ChildOf);
					(void)reg_core_entity<Is_>(Is);
				}

				// Add special properites for core components
				{
					EntityBuilder(*this, Core) //
							.add(Core)
							.add(Pair(OnDelete, Error));
					EntityBuilder(*this, GAIA_ID(EntityDesc)) //
							.add(Pair(OnDelete, Error));
					EntityBuilder(*this, GAIA_ID(Component)) //
							.add(Pair(OnDelete, Error));
					EntityBuilder(*this, OnDelete) //
							.add(Core)
							.add(Pair(OnDelete, Error));
					EntityBuilder(*this, OnDeleteTarget) //
							.add(Core)
							.add(Pair(OnDelete, Error));
					EntityBuilder(*this, Remove) //
							.add(Core)
							.add(Pair(OnDelete, Error));
					EntityBuilder(*this, Delete) //
							.add(Core)
							.add(Pair(OnDelete, Error));
					EntityBuilder(*this, Error) //
							.add(Core)
							.add(Pair(OnDelete, Error));
					EntityBuilder(*this, All) //
							.add(Core)
							.add(Pair(OnDelete, Error));
					EntityBuilder(*this, DependsOn) //
							.add(Core)
							.add(Pair(OnDelete, Error))
							.add(Acyclic);
					EntityBuilder(*this, CantCombine) //
							.add(Core)
							.add(Pair(OnDelete, Error))
							.add(Acyclic);
					EntityBuilder(*this, Acyclic) //
							.add(Core)
							.add(Pair(OnDelete, Error));
					EntityBuilder(*this, ChildOf) //
							.add(Core)
							.add(Acyclic)
							.add(Pair(OnDelete, Error))
							.add(Pair(OnDeleteTarget, Delete));
					EntityBuilder(*this, Is) //
							.add(Core)
							.add(Acyclic)
							.add(Pair(OnDelete, Error));
				}

				// Remove all archetypes with no chunks. We don't want any leftovers after
				// archetype movemements.
				{
					for (uint32_t i = 1; i < m_archetypes.size(); ++i) {
						auto* pArchetype = m_archetypes[i];
						if (!pArchetype->chunks().empty())
							continue;
						m_archetypesToDel.push_back(pArchetype);
					}

					for (auto* pArchetype: m_archetypesToDel) {
						unreg_archetype_raw(pArchetype);
						Archetype::destroy(pArchetype);
					}

					m_archetypesToDel.clear();
					sort_archetypes();
				}

				// Make sure archetype pointers are up-to-date
				m_pCompArchetype = m_recs.entities[GAIA_ID(Component).id()].pArchetype;
			}

			void done() {
				cleanup_internal();

#if GAIA_ECS_CHUNK_ALLOCATOR
				ChunkAllocator::get().flush();
#endif
			}

			//! Assigns an entity to a given archetype.
			//! \param archetype Archetype the entity should inherit
			//! \param entity Entity
			void assign_entity(Entity entity, Archetype& archetype) {
				GAIA_ASSERT(!entity.pair());

				auto* pChunk = archetype.foc_free_chunk();
				store_entity(m_recs.entities[entity.id()], entity, &archetype, pChunk);
				archetype.try_update_free_chunk_idx();

				// Call constructors for the generic components on the newly added entity if necessary
				if (pChunk->has_custom_gen_ctor())
					pChunk->call_gen_ctors(pChunk->size() - 1, 1);

#if GAIA_ASSERT_ENABLED
				const auto& ec = m_recs.entities[entity.id()];
				GAIA_ASSERT(ec.pChunk == pChunk);
				auto entityExpected = pChunk->entity_view()[ec.row];
				GAIA_ASSERT(entityExpected == entity);
#endif
			}

			//! Assigns a pair entity to a given archetype.
			//! \param archetype Archetype the pair should inherit
			//! \param entity Pair entity
			void assign_pair(Entity entity, Archetype& archetype) {
				GAIA_ASSERT(entity.pair());

				// Pairs are always added to m_pEntityArchetype initially and this can't change.
				GAIA_ASSERT(&archetype == m_pEntityArchetype);

				const auto it = m_recs.pairs.find(EntityLookupKey(entity));
				if (it != m_recs.pairs.end())
					return;

				// Update the container record
				EntityContainer ec{};
				ec.idx = entity.id();
				ec.gen = entity.gen();
				ec.pair = 1;
				ec.ent = 1;
				ec.kind = EntityKind::EK_Gen;

				auto* pChunk = archetype.foc_free_chunk();
				store_entity(ec, entity, &archetype, pChunk);
				archetype.try_update_free_chunk_idx();

				m_recs.pairs.emplace(EntityLookupKey(entity), GAIA_MOV(ec));

				// Update pair mappings
				const auto rel = get(entity.id());
				const auto tgt = get(entity.gen());

				auto addPair = [](PairMap& map, Entity source, Entity add) {
					auto& ents = map[EntityLookupKey(source)];
					ents.insert(EntityLookupKey(add));
				};

				addPair(m_relationsToTargets, rel, tgt);
				addPair(m_relationsToTargets, All, tgt);
				addPair(m_targetsToRelations, tgt, rel);
				addPair(m_targetsToRelations, All, rel);
			}

			//! Creates a new entity of a given archetype
			//! \param archetype Archetype the entity should inherit
			//! \param isEntity True if entity, false otherwise
			//! \param isPair True if pair, false otherwise
			//! \param kind Component kind
			//! \return New entity
			GAIA_NODISCARD Entity add(Archetype& archetype, bool isEntity, bool isPair, EntityKind kind) {
				EntityContainerCtx ctx{isEntity, isPair, kind};
				const auto entity = m_recs.entities.alloc(&ctx);
				assign_entity(entity, archetype);
				return entity;
			}

			//! Creates multiple entity of a given archetype at once.
			//! More efficient than creating entities individually.
			//! \param archetype Archetype the entity should inherit
			//! \param count Number of entities to create
			//! \param func void(Entity) functor executed for each added entity.
			template <typename Func>
			void add_many_entities(Archetype& archetype, uint32_t count, Func func) {
				EntityContainerCtx ctx{true, false, EntityKind::EK_Gen};

				uint32_t left = count;
				do {
					auto* pChunk = archetype.foc_free_chunk();
					const uint32_t originalChunkSize = pChunk->size();
					const uint32_t freeSlotsInChunk = pChunk->capacity() - originalChunkSize;
					const uint32_t toCreate = core::get_min(freeSlotsInChunk, left);

					GAIA_FOR(toCreate) {
						const auto entityNew = m_recs.entities.alloc(&ctx);
						auto& ecNew = m_recs.entities[entityNew.id()];
						store_entity(ecNew, entityNew, &archetype, pChunk);

#if GAIA_ASSERT_ENABLED
						GAIA_ASSERT(ecNew.pChunk == pChunk);
						auto entityExpected = pChunk->entity_view()[ecNew.row];
						GAIA_ASSERT(entityExpected == entityNew);
#endif
					}

					// New entities were added, try updating the free chunk index
					archetype.try_update_free_chunk_idx();

					// Call constructors for the generic components on the newly added entity if necessary
					if (pChunk->has_custom_gen_ctor())
						pChunk->call_gen_ctors(originalChunkSize, toCreate);

					// Call functors
					{
						auto entities = pChunk->entity_view();
						GAIA_FOR2(originalChunkSize, pChunk->size()) func(entities[i]);
					}

					left -= toCreate;
				} while (left > 0);
			}

			//! Garbage collection
			void gc() {
				GAIA_PROF_SCOPE(World::gc);

				// del_empty_chunks();
				defrag_chunks(m_defragEntitesPerTick);
				// del_empty_archetypes();
			}
		};

		GAIA_NODISCARD inline const ComponentCache& comp_cache(const World& world) {
			return world.comp_cache();
		}

		GAIA_NODISCARD inline ComponentCache& comp_cache_mut(World& world) {
			return world.comp_cache_mut();
		}

		template <typename T>
		GAIA_NODISCARD inline const ComponentCacheItem& comp_cache_add(World& world) {
			return world.add<T>();
		}

		GAIA_NODISCARD inline Entity entity_from_id(const World& world, EntityId id) {
			return world.get(id);
		}

		GAIA_NODISCARD inline bool is(const World& world, Entity entity, Entity baseEntity) {
			return world.is(entity, baseEntity);
		}

		GAIA_NODISCARD inline bool is_base(const World& world, Entity entity) {
			return world.is_base(entity);
		}

		GAIA_NODISCARD inline Archetype* archetype_from_entity(const World& world, Entity entity) {
			const auto& ec = world.fetch(entity);
			if (World::is_req_del(ec))
				return nullptr;

			return ec.pArchetype;
		}

		GAIA_NODISCARD inline const char* entity_name(const World& world, Entity entity) {
			return world.name(entity);
		}

		GAIA_NODISCARD inline const char* entity_name(const World& world, EntityId entityId) {
			return world.name(entityId);
		}

		template <typename Func>
		void as_relations_trav(const World& world, Entity target, Func func) {
			world.as_relations_trav(target, func);
		}

		template <typename Func>
		bool as_relations_trav_if(const World& world, Entity target, Func func) {
			return world.as_relations_trav_if(target, func);
		}

		template <typename Func>
		void as_targets_trav(const World& world, Entity relation, Func func) {
			world.as_targets_trav(relation, func);
		}

		template <typename Func>
		void as_targets_trav_if(const World& world, Entity relation, Func func) {
			return world.as_targets_trav_if(relation, func);
		}
	} // namespace ecs
} // namespace gaia
