#pragma once
#include "../config/config.h"

#include <cstdint>
#include <type_traits>

#include "../cnt/darray.h"
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

			using EntityNameLookupKey = core::StringLookupKey<256>;
			using PairMap = cnt::map<EntityLookupKey, cnt::set<EntityLookupKey>>;

			//! Cache of components
			ComponentCache m_compCache;
			//! Cache of queries
			QueryCache m_queryCache;

			//! Map of entity -> archetypes
			EntityToArchetypeMap m_entityToArchetypeMap;
			//! Map of relation -> targets
			PairMap m_relationsToTargets;
			//! Map of target -> relations
			PairMap m_targetsToRelations;

			//! Map of archetypes identified by their component hash code
			cnt::map<ArchetypeLookupKey, Archetype*> m_archetypesByHash;
			//! Map of archetypes identified by their ID
			cnt::map<ArchetypeId, Archetype*> m_archetypesById;
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

			//! List of chunks to delete
			cnt::darray<Chunk*> m_chunksToDel;
			//! List of archetypes to delete
			cnt::darray<Archetype*> m_archetypesToDel;
			//! ID of the last defragmented archetype
			uint32_t m_defragLastArchetypeID = 0;
			//! Maximum number of entities to defragment per world tick
			uint32_t m_defragEntitesPerTick = 100;

			//! With every structural change world version changes
			uint32_t m_worldVersion = 0;

		public:
			EntityContainer& fetch(Entity entity) {
				// Valid entity
				GAIA_ASSERT(valid(entity));
				// Wildcard pairs are not a real entity so we can't accept them
				GAIA_ASSERT(!entity.pair() || !is_wildcard(entity));
				return m_recs[entity];
			}

			const EntityContainer& fetch(Entity entity) const {
				GAIA_ASSERT(valid(entity));
				return m_recs[entity];
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

				template <typename... T>
				EntityBuilder& add() {
					(verify_comp<T>(), ...);
					(add(m_world.add<T>().entity), ...);
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
					(del(m_world.add<T>().entity), ...);
					return *this;
				}

			private:
				void handle_add_deps(Entity entity) {
					cnt::sarray_ext<Entity, Chunk::MAX_COMPONENTS> targets;

					// Handle entity combination that can't be together
					{
						const auto& ec = m_world.fetch(entity);
						if ((ec.flags & EntityContainerFlags::HasCantCombine) != 0) {
							m_world.targets(entity, CantCombine, targets);
							for (auto e: targets) {
								if (m_pArchetype->has(e)) {
#if GAIA_ASSERT_ENABLED
									GAIA_ASSERT2(false, "Trying to add an entity which can't be combined with the source");
									print_archetype_entities(m_world, *m_pArchetype, entity, true);
#endif
									return;
								}
							}
						}
					}

					// Handle dependencies
					{
						targets.clear();
						m_world.targets(entity, DependsOn, targets);

						for (auto e: targets) {
							auto* pArchetype = m_pArchetype;
							handle_add(e);
							if (m_pArchetype != pArchetype)
								handle_add_deps(e);
						}
					}
				}

				bool has_DependsOn_deps(Entity entity) const {
					// Don't allow to delete entity if something in the archetype depends on it
					const auto& ids = m_pArchetype->ids();
					for (auto e: ids) {
						if (m_world.has(e, Pair(DependsOn, entity)))
							return true;
					}

					return false;
				}

				void updateFlag(Entity entity, EntityContainerFlags flag, bool enable) {
					auto& ec = m_world.fetch(entity);
					if (enable)
						ec.flags |= flag;
					else
						ec.flags &= ~flag;
				};

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

				void handle_add(Entity entity) {
#if GAIA_DEBUG
					World::verify_add(m_world, *m_pArchetype, m_entity, entity);
#endif

					// Don't add the same entity twice
					if (m_pArchetype->has(entity))
						return;

					try_set_CantCombine(entity, true);
					try_set_OnDeleteTarget(entity, true);
					try_set_OnDelete(entity, true);

					m_pArchetype = m_world.foc_archetype_add(m_pArchetype, entity);
				}

				void handle_del(Entity entity) {
#if GAIA_DEBUG
					World::verify_del(m_world, *m_pArchetype, m_entity, entity);
#endif

					try_set_CantCombine(entity, false);
					try_set_OnDeleteTarget(entity, false);
					try_set_OnDelete(entity, false);

					m_pArchetype = m_world.foc_archetype_del(m_pArchetype, entity);
				}

				void add_inter(Entity entity) {
					GAIA_ASSERT(!is_wildcard(entity));

					if (entity.pair()) {
						// Make sure the entity container record exists if it is a pair
						m_world.assign(entity, *m_world.m_pEntityArchetype);
					}

					handle_add_deps(entity);
					handle_add(entity);
				}

				void del_inter(Entity entity) {
					if (has_DependsOn_deps(entity))
						return;
					handle_del(entity);
				}
			};

		private:
			//! Remove an entity from its chunk.
			//! \param pChunk Chunk we remove the entity from
			//! \param row Index of entity within its chunk
			void remove_entity(Chunk* pChunk, uint16_t row) {
				GAIA_PROF_SCOPE(World::remove_entity);

				pChunk->remove_entity(row, m_recs, m_chunksToDel);
				pChunk->update_versions();
			}

			//! Delete an empty chunk from its archetype
			void del_empty_chunk(Chunk* pChunk) {
				GAIA_PROF_SCOPE(World::del_empty_chunk);

				GAIA_ASSERT(pChunk != nullptr);
				GAIA_ASSERT(pChunk->empty());
				GAIA_ASSERT(!pChunk->dying());

				cnt::sarr_ext<Entity, Chunk::MAX_COMPONENTS> ids;
				cnt::sarr_ext<Component, Chunk::MAX_COMPONENTS> comps;
				{
					auto recs = pChunk->comp_rec_view();
					ids.resize((uint32_t)recs.size());
					comps.resize((uint32_t)recs.size());
					GAIA_EACH_(recs, j) ids[j] = recs[j].entity;
					GAIA_EACH_(recs, j) comps[j] = recs[j].comp;
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

					// Skip reclaimed chunks
					if (!pChunk->empty()) {
						pChunk->revive();
						core::erase_fast(m_chunksToDel, i);
						continue;
					}

					// Skip chunks which still has some lifetime left
					if (pChunk->progress_death()) {
						++i;
						continue;
					}

					// Remove unused chunks
					del_empty_chunk(pChunk);
					core::erase_fast(m_chunksToDel, i);
				}
			}

			//! Delete an archetype from the world
			void del_empty_archetype(Archetype* pArchetype) {
				GAIA_PROF_SCOPE(World::del_empty_archetype);

				GAIA_ASSERT(pArchetype != nullptr);
				GAIA_ASSERT(pArchetype->empty());
				GAIA_ASSERT(!pArchetype->dying());

				// If the deleted archetype is the last one we defragmented
				// make sure to point to the next one.
				if (m_defragLastArchetypeID == pArchetype->id()) {
					auto it = m_archetypesById.find(pArchetype->id());
					++it;

					// Handle the wrap-around
					if (it == m_archetypesById.end())
						m_defragLastArchetypeID = m_archetypesById.begin()->second->id();
					else
						m_defragLastArchetypeID = it->second->id();
				}

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
						pArchetype->revive();
						core::erase_fast(m_archetypesToDel, i);
						continue;
					}

					// Skip archetypes which still has some lifetime left
					if (pArchetype->progress_death()) {
						++i;
						continue;
					}

					tmp.push_back(pArchetype);

					// Remove the unused archetypes
					del_empty_archetype(pArchetype);
					core::erase_fast(m_archetypesToDel, i);
				}

				// Remove all dead archetypes from query caches.
				// Because the number of cached queries is way higher than the number of archetypes
				// we want to remove, we flip the logic around and iterate over all query caches
				// and match against our lists.
				if (!tmp.empty()) {
					// TODO: How to speed this up? If there are 1k cached queries is it still going to
					//       be fast enough or do we get spikes?
					for (auto& info: m_queryCache) {
						for (auto* pArchetype: tmp)
							info.remove(pArchetype);
					}
				}
			}

			//! Defragments chunks.
			//! \param maxEntites Maximum number of entities moved per call
			void defrag_chunks(uint32_t maxEntities) {
				GAIA_PROF_SCOPE(World::defrag_chunks);

				const auto maxIters = (uint32_t)m_archetypesById.size();
				// There has to be at least the root archetype present
				GAIA_ASSERT(maxIters > 0);

				auto it = m_archetypesById.find(m_defragLastArchetypeID);
				// Every time we delete an archetype we mamke sure the defrag ID is updated.
				// Therefore, it should always be valid.
				GAIA_ASSERT(it != m_archetypesById.end());

				GAIA_FOR(maxIters) {
					auto* pArchetype = m_archetypesById[m_defragLastArchetypeID];
					pArchetype->defrag(maxEntities, m_chunksToDel, m_recs);
					if (maxEntities == 0)
						return;

					++it;
					if (it == m_archetypesById.end())
						m_defragLastArchetypeID = m_archetypesById.begin()->second->id();
					else
						m_defragLastArchetypeID = it->second->id();
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
			void add_entity_archetype_pair(Entity entity, Archetype* pArchetype) {
				const auto it = m_entityToArchetypeMap.find(EntityLookupKey(entity));
				if (it == m_entityToArchetypeMap.end())
					m_entityToArchetypeMap.try_emplace(EntityLookupKey(entity), ArchetypeList{pArchetype});
				else if (!core::has(it->second, pArchetype))
					it->second.push_back(pArchetype);
			}

			//! Creates a new archetype from a given set of entities
			//! \param entities Archetype entities/components
			//! \return Pointer to the new archetype.
			GAIA_NODISCARD Archetype* create_archetype(EntitySpan entities) {
				auto* pArchetype = Archetype::create(comp_cache(), m_nextArchetypeId++, m_worldVersion, entities);

				for (auto entity: entities) {
					add_entity_archetype_pair(entity, pArchetype);

					// If the entity is a pair, make sure to create special wildcard records for it
					// as well so wildcard queries can find the archetype.
					if (entity.pair()) {
						// Fake entities instantiated for both ids.
						// We don't care though because to construct the archetype pair we only need
						// the IDs anyway.
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
				GAIA_ASSERT(!m_archetypesById.contains(pArchetype->id()));

				// Register the archetype
				m_archetypesById.emplace(pArchetype->id(), pArchetype);
				m_archetypesByHash.emplace(ArchetypeLookupKey(pArchetype->lookup_hash(), pArchetype), pArchetype);
			}

			//! Unregisters the archetype in the world.
			//! \param pArchetype Archetype to register.
			void unreg_archetype(Archetype* pArchetype) {
				GAIA_ASSERT(pArchetype != nullptr);

				// Make sure hashes were set already
				GAIA_ASSERT(
						(m_archetypesById.empty() || pArchetype == m_pRootArchetype) || (pArchetype->lookup_hash().hash != 0));

				// Make sure the archetype was registered already
				GAIA_ASSERT(!m_archetypesById.contains(pArchetype->id()));

				const auto& ids = pArchetype->ids();
				auto tmpArchetype = ArchetypeLookupChecker({ids.data(), ids.size()});
				ArchetypeLookupKey key(pArchetype->lookup_hash(), &tmpArchetype);
				m_archetypesByHash.erase(key);
				m_archetypesById.erase(pArchetype->id());
			}

#if GAIA_DEBUG
			static void print_archetype_entities(const World& world, const Archetype& archetype, Entity entity, bool adding) {
				const auto& ids = archetype.ids();

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
				const auto& ids = archetype.ids();
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

				// 	const auto hashLookup = calc_lookup_hash(EntitySpan{&entity, 1}).hash;
				// 	pArchetypeRight = find_archetype({hashLookup}, EntitySpan{&entity, 1});
				// 	if (pArchetypeRight == nullptr) {
				// 		pArchetypeRight = create_archetype(EntitySpan{&entity, 1});
				// 		pArchetypeRight->set_hashes({hashLookup});
				// 		pArchetypeRight->build_graph_edges_left(pArchetypeLeft, entity);
				// 		reg_archetype(pArchetypeRight);
				// 	}

				// 	return pArchetypeRight;
				// }

				// Check if the component is found when following the "add" edges
				{
					const auto archetypeId = pArchetypeLeft->find_edge_right(entity);
					if (archetypeId != ArchetypeIdBad)
						return m_archetypesById[archetypeId];
				}

				// Prepare a joint array of components of old + the newly added component
				cnt::sarray_ext<Entity, Chunk::MAX_COMPONENTS> entsNew;
				{
					const auto& entsOld = pArchetypeLeft->ids();
					entsNew.resize(entsOld.size() + 1);
					GAIA_EACH(entsOld) entsNew[i] = entsOld[i];
					entsNew[entsOld.size()] = entity;
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
					const auto archetypeId = pArchetypeRight->find_edge_left(entity);
					if (archetypeId != ArchetypeIdBad)
						return m_archetypesById[archetypeId];
				}

				cnt::sarray_ext<Entity, Chunk::MAX_COMPONENTS> entsNew;
				const auto& entsOld = pArchetypeRight->ids();

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
				auto& ec = fetch(entity);
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
			void del_entity(Entity entity) {
				// Wildcard entites are a virtual concept, there is nothing to delete
				if (is_wildcard(entity))
					return;

				const auto& ec = fetch(entity);
				GAIA_ASSERT(entity.id() > GAIA_ID(LastCoreComponent).id());

				if (m_recs.entities.item_count() == 0 || entity == EntityBad)
					return;

				auto* pChunk = ec.pChunk;
				GAIA_ASSERT(pChunk != nullptr);

				// Remove the entity from its chunk.
				// We call del_name first because remove_entity calls component destructors.
				// If the call was made inside invalidate_entity we would access a memory location
				// which has already been destructed which is not nice.
				del_name(entity);
				remove_entity(pChunk, ec.row);
				invalidate_entity(entity);

				// End-state validation
				validate_chunk(pChunk);
				validate_entities();
			}

			void del_entities(Archetype& archetype) {
				for (auto* pChunk: archetype.chunks()) {
					auto ids = pChunk->entity_view();
					for (auto e: ids) {
						del_name(e);
						invalidate_entity(e);
					}

					// If the chunk was already dying we need to remove it
					// from the delete list.
					// TODO: Instead of searching for it we could store a delete index in the chunk
					//       header. This was the lookup is O(1) instead of O(N) and it would help
					//       with edge-cases (tons of chunks removed at the same time).
					if (pChunk->dying()) {
						const auto idx = core::get_index(m_chunksToDel, pChunk);
						if (idx != BadIndex)
							core::erase_fast(m_chunksToDel, idx);
					}

					archetype.del(pChunk, m_archetypesToDel);
				}
			}

			bool archetype_cond_match(Archetype& archetype, Pair cond, Entity target) {
				// E.g.:
				//   target = (All, entity)
				//   cond = (OnDeleteTarget, delete)
				// Delete the entity if it matches the cond
				const auto& ids = archetype.ids();

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

			//! Delete anything referencing the entity \param target. No questions asked.
			void del_entities_with(Entity target, Pair cond = {EntityBad, EntityBad}) {
				GAIA_PROF_SCOPE(World::del_entities_with);

				const auto it = m_entityToArchetypeMap.find(EntityLookupKey(target));
				if (it != m_entityToArchetypeMap.end()) {
					const auto& archetypes = it->second;
					if (cond.first() != EntityBad) {
						for (auto* pArchetype: archetypes) {
							// Evaluate the conditon if a valid pair is given
							if (!archetype_cond_match(*pArchetype, cond, target))
								continue;

							del_entities(*pArchetype);
						}
					} else {
						for (auto* pArchetype: archetypes) {
							del_entities(*pArchetype);
						}
					}
				}
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
					//       with a pointer to another one. The some goes for archetypes. Component data
					//       would not have to move at all internal chunk header pointers would remain unchanged.

					auto* pDstChunk = dstArchetype.foc_free_chunk();
					int i = (int)(srcEnts.size() - 1);
					while (i >= 0) {
						if (pDstChunk->full())
							pDstChunk = dstArchetype.foc_free_chunk();
						move_entity(srcEnts[(uint32_t)i], dstArchetype, *pDstChunk);

						--i;
					}
				}
			}

			//! Find the destination archetype \param pArchetype as if removing \param entity.
			//! \return Destination archetype of nullptr if no match is found
			Archetype* calc_dst_archetype_ent(Archetype* pArchetype, Entity entity) {
				GAIA_ASSERT(!is_wildcard(entity));

				const auto& ids = pArchetype->ids();
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
			Archetype* calc_dst_archetype_all_ent(Archetype* pArchetype, Entity entity) {
				GAIA_ASSERT(is_wildcard(entity));

				Archetype* pDstArchetype = pArchetype;
				bool found = false;

				const auto& ids = pArchetype->ids();
				for (auto id: ids) {
					if (!id.pair() || id.gen() != entity.gen())
						continue;

					pDstArchetype = foc_archetype_del(pDstArchetype, id);
					found = true;
				}

				return found ? pDstArchetype : nullptr;
			}

			//! Find the destination archetype \param pArchetype as if removing all entities
			//! matching (entity, All) from the archetype.
			//! \return Destination archetype of nullptr if no match is found
			Archetype* calc_dst_archetype_ent_all(Archetype* pArchetype, Entity entity) {
				GAIA_ASSERT(is_wildcard(entity));

				Archetype* pDstArchetype = pArchetype;
				bool found = false;

				const auto& ids = pArchetype->ids();
				for (auto id: ids) {
					if (!id.pair() || id.id() != entity.id())
						continue;

					pDstArchetype = foc_archetype_del(pDstArchetype, id);
					found = true;
				}

				return found ? pDstArchetype : nullptr;
			}

			//! Find the destination archetype \param pArchetype as if removing all entities
			//! matching (All, All) from the archetype.
			//! \return Destination archetype of nullptr if no match is found
			Archetype* calc_dst_archetype_all_all(Archetype* pArchetype, [[maybe_unused]] Entity entity) {
				GAIA_ASSERT(is_wildcard(entity));

				Archetype* pDstArchetype = pArchetype;
				bool found = false;

				const auto& ids = pArchetype->ids();
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
			Archetype* calc_dst_archetype(Archetype* pArchetype, Entity entity) {
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

			//! Removes the entity \param target from anything referencing it.
			void rem_from_entities(Entity target, Pair cond = {EntityBad, EntityBad}) {
				GAIA_PROF_SCOPE(World::rem_from_entities);

				const auto it = m_entityToArchetypeMap.find(EntityLookupKey(target));
				if (it != m_entityToArchetypeMap.end()) {
					if (cond.first() != EntityBad) {
						for (auto* pArchetype: it->second) {
							// Evaluate the conditon if a valid pair is given
							if (!archetype_cond_match(*pArchetype, cond, target))
								continue;

							auto* pDstArchetype = calc_dst_archetype(pArchetype, target);
							if (pDstArchetype != nullptr)
								move_to_archetype(*pArchetype, *pDstArchetype);
						}
					} else {
						for (auto* pArchetype: it->second) {
							auto* pDstArchetype = calc_dst_archetype(pArchetype, target);
							if (pDstArchetype != nullptr)
								move_to_archetype(*pArchetype, *pDstArchetype);
						}
					}
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
						del_entities_with(Pair(All, tgt), Pair(OnDeleteTarget, Delete));
					} else {
						// Remove from all entities referencing this one as a relationship pair's target
						rem_from_entities(Pair(All, tgt), Pair(OnDeleteTarget, Delete));
					}

					if ((ec.flags & EntityContainerFlags::OnDelete_Delete) != 0) {
						// Delete all references to the entity
						del_entities_with(entity);
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
						del_entities_with(Pair(All, entity), Pair(OnDeleteTarget, Delete));
					} else {
						// Remove from all entities referencing this one as a relationship pair's target
						rem_from_entities(Pair(All, entity), Pair(OnDeleteTarget, Delete));
					}

					if ((ec.flags & EntityContainerFlags::OnDelete_Delete) != 0) {
						// Delete all references to the entity
						del_entities_with(entity);
					} else {
						// Entities are only removed by default
						rem_from_entities(entity);
					}
				}
			}

			//! Invalidates the entity record, effectivelly deleting it.
			//! \param entity Entity to delete
			void invalidate_entity(Entity entity) {
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
					ec.pArchetype = nullptr;
					ec.pChunk = nullptr;

					// Update pairs
					delPair(m_relationsToTargets, All, entity);
					delPair(m_targetsToRelations, All, entity);
					m_relationsToTargets.erase(EntityLookupKey(entity));
					m_targetsToRelations.erase(EntityLookupKey(entity));
				}
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
				if (newArchetype.id() == oldArchetype.id())
					pNewChunk->move_entity_data(entity, newRow, m_recs);
				else
					pNewChunk->move_foreign_entity_data(entity, newRow, m_recs);

				// Remove the entity record from the old chunk
				remove_entity(pOldChunk, oldRow);

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
				GAIA_ASSERT(valid(entity));

				if (name == nullptr) {
					GAIA_ASSERT(len == 0);
					del_name(entity);
					return;
				}

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
					sset<EntityDesc>(entity, {entityStr, key.len()});
				} else {
					// We tell the map the string is non-owned.
					auto p = robin_hood::pair(std::make_pair(EntityNameLookupKey(key.str(), key.len(), 0, {key.hash()}), entity));
					res.first->swap(p);

					// Update the entity string pointer
					sset<EntityDesc>(entity, {name, key.len()});
				}
			}

			void init() {
				// Register the root archetype
				{
					m_pRootArchetype = create_archetype({});
					m_pRootArchetype->set_hashes({calc_lookup_hash({})});
					reg_archetype(m_pRootArchetype);
				}

				// Register the core component
				{
					const auto& id = Core;
					auto comp = add(*m_pRootArchetype, id.entity(), id.pair(), id.kind());
					const auto& desc = comp_cache_mut().add<Core_>(id);
					GAIA_ASSERT(desc.entity == id);
					(void)comp;
					(void)desc;
				}

				// Register the entity archetype (entity + EntityDesc component)
				{
					const auto& id = GAIA_ID(EntityDesc);
					auto comp = add(*m_pRootArchetype, id.entity(), id.pair(), id.kind());
					const auto& desc = comp_cache_mut().add<EntityDesc>(comp);
					GAIA_ASSERT(desc.entity == id);
					EntityBuilder(*this, comp).add(desc.entity);
					sset<EntityDesc>(comp, {desc.name.str(), desc.name.len()});
					m_pEntityArchetype = m_recs.entities[comp.id()].pArchetype;
				}

				// Register the component archetype (entity + EntityDesc + Component)
				{
					const auto& id = GAIA_ID(Component);
					auto comp = add(*m_pEntityArchetype, id.entity(), id.pair(), id.kind());
					const auto& desc = comp_cache_mut().add<Component>(comp);
					GAIA_ASSERT(desc.entity == id);
					EntityBuilder(*this, comp).add(desc.entity);
					set(comp)
							// Entity descriptor
							.sset<EntityDesc>({desc.name.str(), desc.name.len()})
							// Component
							.sset<Component>(desc.comp);
					m_pCompArchetype = m_recs.entities[comp.id()].pArchetype;
				}

				// Register OnDelete/OnDeleteTarget/Remove/Delete components.
				// Used to set up how deleting an entity/pair is handled.
				{
					const auto& id = OnDelete;
					auto comp = add(*m_pRootArchetype, id.entity(), id.pair(), id.kind());
					const auto& desc = comp_cache_mut().add<OnDelete_>(id);
					GAIA_ASSERT(desc.entity == id);
					(void)comp;
					(void)desc;
				}
				{
					const auto& id = OnDeleteTarget;
					auto comp = add(*m_pRootArchetype, id.entity(), id.pair(), id.kind());
					const auto& desc = comp_cache_mut().add<OnDeleteTarget_>(id);
					GAIA_ASSERT(desc.entity == id);
					(void)comp;
					(void)desc;
				}
				{
					const auto& id = Remove;
					auto comp = add(*m_pRootArchetype, id.entity(), id.pair(), id.kind());
					const auto& desc = comp_cache_mut().add<Remove_>(id);
					GAIA_ASSERT(desc.entity == id);
					(void)comp;
					(void)desc;
				}
				{
					const auto& id = Delete;
					auto comp = add(*m_pRootArchetype, id.entity(), id.pair(), id.kind());
					const auto& desc = comp_cache_mut().add<Delete_>(id);
					GAIA_ASSERT(desc.entity == id);
					(void)comp;
					(void)desc;
				}
				{
					const auto& id = Error;
					auto comp = add(*m_pRootArchetype, id.entity(), id.pair(), id.kind());
					const auto& desc = comp_cache_mut().add<Error_>(id);
					GAIA_ASSERT(desc.entity == id);
					(void)comp;
					(void)desc;
				}

				// Dependencies
				{
					const auto& id = DependsOn;
					auto comp = add(*m_pRootArchetype, id.entity(), id.pair(), id.kind());
					const auto& desc = comp_cache_mut().add<DependsOn_>(id);
					GAIA_ASSERT(desc.entity == id);
					(void)comp;
					(void)desc;
				}
				{
					const auto& id = CantCombine;
					auto comp = add(*m_pRootArchetype, id.entity(), id.pair(), id.kind());
					const auto& desc = comp_cache_mut().add<CantCombine_>(id);
					GAIA_ASSERT(desc.entity == id);
					(void)comp;
					(void)desc;
				}

				// Graph restrictions
				{
					const auto& id = Acyclic;
					auto comp = add(*m_pRootArchetype, id.entity(), id.pair(), id.kind());
					const auto& desc = comp_cache_mut().add<Acyclic_>(id);
					GAIA_ASSERT(desc.entity == id);
					(void)comp;
					(void)desc;
				}

				// Register All component. Used with relationship queries.
				{
					const auto& id = All;
					auto comp = add(*m_pRootArchetype, id.entity(), id.pair(), id.kind());
					const auto& desc = comp_cache_mut().add<All_>(id);
					GAIA_ASSERT(desc.entity == id);
					(void)comp;
					(void)desc;
				}

				// Register ChildOf component. Used with relationship queries.
				// When the relationship target is deleted all children are deleted as well.
				{
					const auto& id = ChildOf;
					auto comp = add(*m_pRootArchetype, id.entity(), id.pair(), id.kind());
					const auto& desc = comp_cache_mut().add<ChildOf_>(id);
					GAIA_ASSERT(desc.entity == id);
					(void)comp;
					(void)desc;
				}

				// Special properites for core components
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
						.add(Pair(OnDelete, Error))
						.add(Pair(OnDeleteTarget, Delete));
			}

			void done() {
				cleanup();

#if GAIA_ECS_CHUNK_ALLOCATOR
				ChunkAllocator::get().flush();
#endif
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
				assign(entity, archetype);
				return entity;
			}

			//! Assigns an entity to a given archetype
			//! \param archetype Archetype the entity should inherit
			//! \param entity Entity
			void assign(Entity entity, Archetype& archetype) {
				if (entity.pair()) {
					// Pairs are always added to m_pEntityArchetype initially and this can't change.
					GAIA_ASSERT(&archetype == m_pEntityArchetype);
					const auto it = m_recs.pairs.find(EntityLookupKey(entity));
					if (it == m_recs.pairs.end()) {
						// Update the container record
						EntityContainer ec{};
						ec.idx = entity.id();
						ec.gen = entity.gen();
						ec.pair = 1;
						ec.ent = 1;
						ec.kind = EntityKind::EK_Gen;

						auto* pChunk = archetype.foc_free_chunk();
						store_entity(ec, entity, &archetype, pChunk);
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
				} else {
					auto* pChunk = archetype.foc_free_chunk();
					store_entity(m_recs.entities[entity.id()], entity, &archetype, pChunk);

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
			}

			//! Garbage collection
			void gc() {
				GAIA_PROF_SCOPE(World::gc);

				del_empty_chunks();
				defrag_chunks(m_defragEntitesPerTick);
				del_empty_archetypes();
			}

		public:
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
			//! \return True is the entity is valid. False otherwise.
			GAIA_NODISCARD bool valid(Entity entity) const {
				if (entity == EntityBad)
					return false;

				// Pair
				if (entity.pair()) {
					// Ignore wildcards because they can't be attached to entities
					if (is_wildcard(entity))
						return true;

					const auto it = m_recs.pairs.find(EntityLookupKey(entity));
					if (it == m_recs.pairs.end())
						return false;

					const auto& ec = it->second;

					// The entity in the chunk must match the index in the entity container
					auto* pChunk = ec.pChunk;
					return pChunk != nullptr && pChunk->entity_view()[ec.row] == entity;
				}

				// Regular entity
				{
					// Entity ID has to fit inside the entity array
					if (entity.id() >= m_recs.entities.size())
						return false;

					const auto& ec = m_recs.entities[entity.id()];

					// Generation ID has to match the one in the array
					if (ec.gen != entity.gen())
						return false;

					// The entity in the chunk must match the index in the entity container
					auto* pChunk = ec.pChunk;
					return pChunk != nullptr && pChunk->entity_view()[ec.row] == entity;
				}
			}

			//----------------------------------------------------------------------

			//! Returns the entity located at the index \param id
			//! \return Entity
			GAIA_NODISCARD Entity get(EntityId id) const {
				GAIA_ASSERT(id < m_recs.entities.size());
				const auto& ec = m_recs.entities[id];
#if GAIA_ASSERT_ENABLED
				if (ec.pChunk != nullptr) {
					auto entityExpected = ec.pChunk->entity_view()[ec.row];
					GAIA_ASSERT(entityExpected == Entity(id, ec.gen, (bool)ec.ent, (bool)ec.pair, (EntityKind)ec.kind));
				}
#endif
				return Entity(id, ec.gen, (bool)ec.ent, (bool)ec.pair, (EntityKind)ec.kind);
			}

			//----------------------------------------------------------------------

			//! Starts a bulk add/remove operation on \param entity.
			//! \param entity Entity
			//! \return EntityBuilder
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			EntityBuilder bulk(Entity entity) {
				return EntityBuilder(*this, entity);
			}

			//! Creates a new empty entity
			//! \param kind Entity kind
			//! \return New entity
			GAIA_NODISCARD Entity add(EntityKind kind = EntityKind::EK_Gen) {
				return add(*m_pEntityArchetype, true, false, kind);
			}

			//! Creates a new component if not found already.
			//! \param kind Component kind
			//! \return Component cache item of the component
			template <typename T>
			GAIA_NODISCARD const ComponentCacheItem& add() {
				using CT = component_type_t<T>;
				using FT = typename CT::TypeFull;
				constexpr auto kind = CT::Kind;

				const auto* pItem = comp_cache().find<FT>();
				if (pItem != nullptr)
					return *pItem;

				const auto entity = add(*m_pCompArchetype, false, false, kind);
				// Don't allow components to be deleted
				EntityBuilder(*this, entity).add(Pair(OnDelete, Error));

				const auto& desc = comp_cache_mut().add<FT>(entity);

				// Following lines do the following but a bit faster:
				// sset<Component>(comp, desc.comp);
				auto& ec = m_recs.entities[entity.id()];
				auto* pComps = (Component*)ec.pChunk->comp_ptr_mut(EntityKind::EK_Gen, 1);
				auto& comp = pComps[ec.row];
				comp = desc.comp;
				// TODO: Implement entity locking. A locked entity can't change archtypes.
				//       This way we can prevent anybody messing with the internal state of the component
				//       entities created at compile-time data.

				return desc;
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
				using CT = component_type_t<T>;
				using FT = typename CT::TypeFull;
				EntityBuilder(*this, entity).add<FT>();
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
				ComponentSetter{ec.pChunk, idx}.set(object, GAIA_FWD(value));
			}

			//! Attaches a new component \tparam T to \param entity. Also sets its value.
			//! \tparam T Component
			//! \param entity Entity
			//! \param value Value to set for the component
			//! \warning It is expected the component is not present on \param entity yet. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename T, typename U = typename component_type_t<T>::Type>
			void add(Entity entity, U&& value) {
				using CT = component_type_t<T>;
				using FT = typename CT::TypeFull;
				constexpr auto kind = (uint32_t)CT::Kind;

				GAIA_ASSERT(!entity.pair());

				EntityBuilder(*this, entity).add<FT>();

				const auto& ec = m_recs.entities[entity.id()];
				// Make sure the idx is 0 for unique entities
				const auto idx = uint16_t(ec.row * (1U - (uint32_t)kind));
				ComponentSetter{ec.pChunk, idx}.set<FT>(GAIA_FWD(value));
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

			//----------------------------------------------------------------------

			void del_inter(Entity entity) {
				auto handle_del = [this](Entity entityToDel) {
					handle_del_entity(entityToDel);
					del_entity(entityToDel);
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
							for (auto key: *pTargets)
								handle_del(Pair(key.entity(), tgt));
						}
					}
					// (X,*)
					else if (tgt == All) {
						if (const auto* pRelations = targets(rel)) {
							for (auto key: *pRelations)
								handle_del(Pair(rel, key.entity()));
						}
					}
				} else {
					handle_del(entity);
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

			//! Starts a bulk set operation on \param entity.
			//! \param entity Entity
			//! \return ComponentSetter
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			//! \warning Undefined behavior if \param entity changes archetype after ComponentSetter is created.
			ComponentSetter set(Entity entity) {
				GAIA_ASSERT(valid(entity));

				const auto& ec = m_recs.entities[entity.id()];
				return ComponentSetter{ec.pChunk, ec.row};
			}

			//! Sets the value of the component \tparam T on \param entity.
			//! \tparam T Component
			//! \param entity Entity
			//! \param value Value to set for the component
			//! \warning It is expected the component is present on \param entity. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			//! \warning Undefined behavior if \param entity changes archetype after ComponentSetter is created.
			template <typename T, typename U = typename component_type_t<T>::Type>
			void set(Entity entity, U&& value) {
				using CT = component_type_t<T>;
				using FT = typename CT::TypeFull;

				GAIA_ASSERT(valid(entity));

				const auto& ec = m_recs.entities[entity.id()];
				ComponentSetter{ec.pChunk, ec.row}.set<FT>(GAIA_FWD(value));
			}

			//! Sets the value of the component \tparam T on \param entity without triggering a world version update.
			//! \tparam T Component
			//! \param entity Entity
			//! \param value Value to set for the component
			//! \warning It is expected the component is present on \param entity. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			//! \warning Undefined behavior if \param entity changes archetype after ComponentSetter is created.
			template <typename T, typename U = typename component_type_t<T>::Type>
			void sset(Entity entity, U&& value) {
				using CT = component_type_t<T>;
				using FT = typename CT::TypeFull;

				GAIA_ASSERT(valid(entity));

				const auto& ec = m_recs.entities[entity.id()];
				ComponentSetter{ec.pChunk, ec.row}.sset<FT>(GAIA_FWD(value));
			}

			//----------------------------------------------------------------------

			//! Returns the value stored in the component \tparam T on \param entity.
			//! \tparam T Component
			//! \param entity Entity
			//! \return Value stored in the component.
			//! \warning It is expected the component is present on \param entity. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			//! \warning Undefined behavior if \param entity changes archetype after ComponentSetter is created.
			template <typename T>
			GAIA_NODISCARD decltype(auto) get(Entity entity) const {
				GAIA_ASSERT(valid(entity));

				const auto& ec = m_recs.entities[entity.id()];
				return ComponentGetter{ec.pChunk, ec.row}.get<T>();
			}

			//----------------------------------------------------------------------

			//! Checks if \param entity is currently used by the world.
			//! \return True is the entity is used. False otherwise.
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

#if GAIA_ASSERT_ENABLED
					// If the pair is found, both entities forming it need to be found as well
					GAIA_ASSERT(has(get(entity.id())) && has(get(entity.gen())));

					// Index of the entity must fit inside the chunk
					const auto& ec = it->second;
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
					auto* pChunk = ec.pChunk;
					return pChunk != nullptr && ec.row < pChunk->size();
				}
			}

			//! Tells if \param entity contains the entity \param object.
			//! \param entity Entity
			//! \param object Tested entity
			//! \return True if object is present on entity. False otherwise or if any of the entites is not valid.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			//! \warning Undefined behavior if \param entity changes archetype after ComponentSetter is created.
			GAIA_NODISCARD bool has(Entity entity, Entity object) const {
				const auto& ec = fetch(entity);

				if (object.pair()) {
					EntityId rel = object.id();
					EntityId tgt = object.gen();

					// (*,*)
					if (rel == All.id() && tgt == All.id()) {
						const auto& ids = ec.pArchetype->ids();
						for (auto id: ids) {
							if (!id.pair())
								continue;
							return true;
						}

						return false;
					}

					// (X,*)
					if (rel != All.id() && tgt == All.id()) {
						const auto& ids = ec.pArchetype->ids();
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
						const auto& ids = ec.pArchetype->ids();
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
				return ComponentGetter{ec.pChunk, ec.row}.has<T>();
			}

			//----------------------------------------------------------------------

			//! Assigns a \param name to \param entity. The string is copied and kept internally.
			//! \param entity Entity
			//! \param name A null-terminated string
			//! \param len String length. If zero, the length is calculated
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			//! \warning Name is expected to be unique. If it is not this function does nothing.
			void name(Entity entity, const char* name, uint32_t len = 0) {
				name_inter<true>(entity, name, len);
			}

			//! Assigns a \param name to \param entity.
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
				return get<EntityDesc>(entity).name;
			}

			//! Returns the name assigned to \param entityId.
			//! \param entityId EntityId
			//! \return Name assigned to entity.
			//! \warning It is expected \param entityId is valid. Undefined behavior otherwise.
			GAIA_NODISCARD const char* name(EntityId entityId) const {
				const auto& ec = m_recs.entities[entityId];
				auto entity = ec.pChunk->entity_view()[ec.row];
				return get<EntityDesc>(entity).name;
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

				const auto& ids = pArchetype->ids();
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
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename C>
			void relations(Entity entity, Entity target, C& out) const {
				GAIA_ASSERT(valid(entity));
				if (!valid(target))
					return;

				const auto& ec = m_recs.entities[entity.id()];
				const auto* pArchetype = ec.pArchetype;

				const auto& ids = pArchetype->ids();
				for (auto e: ids) {
					if (!e.pair())
						continue;
					if (e.gen() != target.id())
						continue;

					const auto& ecRel = m_recs.entities[e.id()];
					auto relation = ecRel.pChunk->entity_view()[ecRel.row];
					out.push_back(relation);
				}
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

				const auto& ids = pArchetype->ids();
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
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename C>
			void targets(Entity entity, Entity relation, C& out) const {
				GAIA_ASSERT(valid(entity));
				if (!valid(relation))
					return;

				const auto& ec = m_recs.entities[entity.id()];
				const auto* pArchetype = ec.pArchetype;

				const auto& ids = pArchetype->ids();
				for (auto e: ids) {
					if (!e.pair())
						continue;
					if (e.id() != relation.id())
						continue;

					const auto& ecTarget = m_recs.entities[e.gen()];
					auto target = ecTarget.pChunk->entity_view()[ecTarget.row];
					out.push_back(target);
				}
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
							m_nextArchetypeId, m_worldVersion, m_archetypesById, m_entityToArchetypeMap);
					q.none(Core);
					return q;
				} else {
					QueryUncached q(
							*const_cast<World*>(this),
							//
							m_nextArchetypeId, m_worldVersion, m_archetypesById, m_entityToArchetypeMap);
					q.none(Core);
					return q;
				}
			}

			//----------------------------------------------------------------------

			//! Enables or disables an entire entity.
			//! \param entity Entity
			//! \param enable Enable or disable the entity
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			void enable(Entity entity, bool enable) {
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

			//----------------------------------------------------------------------

			//! Performs various internal operations related to the end of the frame such as
			//! memory cleanup and other managment operations which keep the system healthy.
			void update() {
				gc();

				// Signal the end of the frame
				GAIA_PROF_FRAME();
			}

			//! Clears the world so that all its entities and components are released
			void cleanup() {
				// Clear entities
				m_recs.entities = {};
				m_recs.pairs = {};

				// Clear archetypes
				{
					// Delete all allocated chunks and their parent archetypes
					for (auto pair: m_archetypesById)
						delete pair.second;

					m_targetsToRelations.clear();
					m_relationsToTargets.clear();

					m_archetypesById = {};
					m_archetypesByHash = {};
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

			//! Sets the maximum number of entites defragmented per world tick
			//! \param value Number of entities to defragment
			void defrag_entities_per_tick(uint32_t value) {
				m_defragEntitesPerTick = value;
			}

			//--------------------------------------------------------------------------------

			//! Performs diagnostics on archetypes. Prints basic info about them and the chunks they contain.
			void diag_archetypes() const {
				GAIA_LOG_N("Archetypes:%u", (uint32_t)m_archetypesById.size());
				for (auto pair: m_archetypesById)
					Archetype::diag(*this, *pair.second);
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
		};

		inline const ComponentCache& comp_cache(const World& world) {
			return world.comp_cache();
		}

		inline ComponentCache& comp_cache_mut(World& world) {
			return world.comp_cache_mut();
		}

		template <typename T>
		inline const ComponentCacheItem& comp_cache_add(World& world) {
			return world.add<T>();
		}

		inline const char* entity_name(const World& world, Entity entity) {
			return world.name(entity);
		}

		inline const char* entity_name(const World& world, EntityId entityId) {
			return world.name(entityId);
		}
	} // namespace ecs
} // namespace gaia
