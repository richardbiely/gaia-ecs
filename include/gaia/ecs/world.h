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

			//! Cache of components
			ComponentCache m_compCache;
			//! Cache of queries
			QueryCache m_queryCache;
			//! Map of components -> archetype matches
			ComponentIdToArchetypeMap m_componentToArchetypeMap;

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

			//! Implicit list of entities. Used for look-ups only when searching for
			//! entities in chunks + data validation
			cnt::ilist<EntityContainer, Entity> m_entities;
			//! Name to entity mapping
			cnt::map<EntityNameLookupKey, Entity> m_nameToEntity;

			//! List of chunks to delete
			cnt::darray<Chunk*> m_chunksToRemove;
			//! List of archetypes to delete
			cnt::darray<Archetype*> m_archetypesToRemove;
			//! ID of the last defragmented archetype
			uint32_t m_defragLastArchetypeID = 0;
			//! Maximum number of entities to defragment per world tick
			uint32_t m_defragEntitesPerTick = 100;

			//! With every structural change world version changes
			uint32_t m_worldVersion = 0;

		private:
			//! Remove an entity from its chunk.
			//! \param pChunk Chunk we remove the entity from
			//! \param row Index of entity within its chunk
			void remove_entity(Chunk* pChunk, uint32_t row) {
				GAIA_PROF_SCOPE(world::remove_entity);

				pChunk->remove_entity(
						row,
						// entities
						{m_entities.data(), m_entities.size()}, m_chunksToRemove);

				pChunk->update_versions();
			}

			//! Delete an empty chunk from its archetype
			void remove_empty_chunk(Chunk* pChunk) {
				GAIA_PROF_SCOPE(world::remove_empty_chunk);

				GAIA_ASSERT(pChunk != nullptr);
				GAIA_ASSERT(pChunk->empty());
				GAIA_ASSERT(!pChunk->dying());

				cnt::sarr_ext<Entity, Chunk::MAX_COMPONENTS> ents[EntityKind::EK_Count];
				cnt::sarr_ext<Component, Chunk::MAX_COMPONENTS> comps[EntityKind::EK_Count];
				GAIA_FOR(EntityKind::EK_Count) {
					auto recs = pChunk->comp_rec_view((EntityKind)i);
					{
						auto& dst = ents[i];
						dst.resize((uint32_t)recs.size());
						GAIA_EACH_(recs, j) dst[j] = recs[j].entity;
					}
					{
						auto& dst = comps[i];
						dst.resize((uint32_t)recs.size());
						GAIA_EACH_(recs, j) dst[j] = recs[j].comp;
					}
				}

				const Archetype::GenComponentHash hashGen = {calc_lookup_hash({ents[0].data(), ents[0].size()}).hash};
				const Archetype::UniComponentHash hashUni = {calc_lookup_hash({ents[1].data(), ents[1].size()}).hash};
				const auto hashLookup = Archetype::calc_lookup_hash(hashGen, hashUni);

				auto* pArchetype =
						find_archetype(hashLookup, {ents[0].data(), ents[0].size()}, {ents[1].data(), ents[1].size()});
				GAIA_ASSERT(pArchetype != nullptr);

				pArchetype->remove_chunk(pChunk, m_archetypesToRemove);
			}

			//! Delete all chunks which are empty (have no entities) and have not been used in a while
			void remove_empty_chunks() {
				GAIA_PROF_SCOPE(world::remove_empty_chunks);

				for (uint32_t i = 0; i < m_chunksToRemove.size();) {
					auto* pChunk = m_chunksToRemove[i];

					// Skip reclaimed chunks
					if (!pChunk->empty()) {
						pChunk->revive();
						core::erase_fast(m_chunksToRemove, i);
						continue;
					}

					// Skip chunks which still has some lifetime left
					if (pChunk->progress_death()) {
						++i;
						continue;
					}

					// Remove unused chunks
					remove_empty_chunk(pChunk);
					core::erase_fast(m_chunksToRemove, i);
				}
			}

			//! Delete an empty archetype from the world
			void remove_empty_archetype(Archetype* pArchetype) {
				GAIA_PROF_SCOPE(world::remove_empty_archetype);

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

				const auto& compsGen = pArchetype->entities(EntityKind::EK_Gen);
				const auto& compsUni = pArchetype->entities(EntityKind::EK_Uni);
				auto tmpArchetype =
						ArchetypeLookupChecker({compsGen.data(), compsGen.size()}, {compsUni.data(), compsUni.size()});
				ArchetypeLookupKey key(pArchetype->lookup_hash(), &tmpArchetype);
				m_archetypesByHash.erase(key);
				m_archetypesById.erase(pArchetype->id());
			}

			//! Delete all archetypes which are empty (have no used chunks) and have not been used in a while
			void remove_empty_archetypes() {
				GAIA_PROF_SCOPE(world::remove_empty_archetypes);

				cnt::sarr_ext<Archetype*, 512> tmp;

				for (uint32_t i = 0; i < m_archetypesToRemove.size();) {
					auto* pArchetype = m_archetypesToRemove[i];

					// Skip reclaimed archetypes
					if (!pArchetype->empty()) {
						pArchetype->revive();
						core::erase_fast(m_archetypesToRemove, i);
						continue;
					}

					// Skip chunks which still has some lifetime left
					if (pArchetype->progress_death()) {
						++i;
						continue;
					}

					tmp.push_back(pArchetype);

					// Remove the unused archetype
					remove_empty_archetype(pArchetype);
					core::erase_fast(m_archetypesToRemove, i);
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
				GAIA_PROF_SCOPE(world::defrag_chunks);

				const auto maxIters = (uint32_t)m_archetypesById.size();
				// There has to be at least the root archetype present
				GAIA_ASSERT(maxIters > 0);

				auto it = m_archetypesById.find(m_defragLastArchetypeID);
				// Every time we delete an archetype we mamke sure the defrag ID is updated.
				// Therefore, it should always be valid.
				GAIA_ASSERT(it != m_archetypesById.end());

				GAIA_FOR(maxIters) {
					auto* pArchetype = m_archetypesById[m_defragLastArchetypeID];
					pArchetype->defrag(
							maxEntities, m_chunksToRemove,
							// entities
							{m_entities.data(), m_entities.size()});
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
			//! \param compsGen Span of generic component ids
			//! \param compsUni Span of unique component ids
			//! \return Pointer to archetype or nullptr.
			GAIA_NODISCARD Archetype*
			find_archetype(Archetype::LookupHash hashLookup, EntitySpan compsGen, EntitySpan compsUni) {
				auto tmpArchetype = ArchetypeLookupChecker(compsGen, compsUni);
				ArchetypeLookupKey key(hashLookup, &tmpArchetype);

				// Search for the archetype in the map
				const auto it = m_archetypesByHash.find(key);
				if (it == m_archetypesByHash.end())
					return nullptr;

				auto* pArchetype = it->second;
				return pArchetype;
			}

			//! Creates a new archetype from a given set of components
			//! \param compsGen Span of generic components
			//! \param compsUni Span of unique components
			//! \return Pointer to the new archetype.
			GAIA_NODISCARD Archetype* create_archetype(EntitySpan compsGen, EntitySpan compsUni) {
				auto* pArchetype = Archetype::create(comp_cache(), m_nextArchetypeId++, m_worldVersion, compsGen, compsUni);

				auto registerComponentToArchetypePair = [&](Entity entity) {
					const auto it = m_componentToArchetypeMap.find(EntityLookupKey(entity));
					if (it == m_componentToArchetypeMap.end())
						m_componentToArchetypeMap.try_emplace(entity, ArchetypeList{pArchetype});
					else if (!core::has(it->second, pArchetype))
						it->second.push_back(pArchetype);
				};

				for (auto comp: compsGen)
					registerComponentToArchetypePair(comp);
				for (auto comp: compsUni)
					registerComponentToArchetypePair(comp);

				return pArchetype;
			}

			//! Registers the archetype in the world.
			//! \param pArchetype Archetype to register.
			void reg_archetype(Archetype* pArchetype) {
				// Make sure hashes were set already
				GAIA_ASSERT(
						(m_archetypesById.empty() || pArchetype == m_pRootArchetype) ||
						(pArchetype->generic_hash().hash != 0 || pArchetype->chunk_hash().hash != 0));
				GAIA_ASSERT(
						(m_archetypesById.empty() || pArchetype == m_pRootArchetype) || pArchetype->lookup_hash().hash != 0);

				// Make sure the archetype is not registered yet
				GAIA_ASSERT(!m_archetypesById.contains(pArchetype->id()));

				// Register the archetype
				m_archetypesById.emplace(pArchetype->id(), pArchetype);
				m_archetypesByHash.emplace(ArchetypeLookupKey(pArchetype->lookup_hash(), pArchetype), pArchetype);
			}

#if GAIA_DEBUG
			static void verify_add(const World& world, Archetype& archetype, Entity entity, Entity addEntity) {
				const auto kind = entity.kind();
				const auto& comps = archetype.entities(kind);

				// Make sure not to add too many comps
				if GAIA_UNLIKELY (comps.size() + 1 >= Chunk::MAX_COMPONENTS) {
					GAIA_ASSERT2(false, "Trying to add too many entities to entity!");
					GAIA_LOG_W("Trying to add an entity to entity [%u.%u] but there's no space left!", entity.id(), entity.gen());
					GAIA_LOG_W("Already present:");
					GAIA_EACH(comps) GAIA_LOG_W("> [%u] %s", (uint32_t)i, entity_name(world, comps[i]));
					GAIA_LOG_W("Trying to add:");
					GAIA_LOG_W("> %s", entity_name(world, addEntity));
				}

				// Don't add the same entity twice
				for (auto comp: comps) {
					if (comp == addEntity) {
						GAIA_ASSERT2(false, "Trying to add a duplicate entity");
						GAIA_LOG_W(
								"Trying to add a duplicate of entity %s (%s) to entity [%u.%u]", entity_name(world, comp),
								EntityKindString[kind], entity.id(), entity.gen());
					}
				}
			}

			static void verify_del(const World& world, Archetype& archetype, Entity entity, Entity delEntity) {
				const auto kind = delEntity.kind();
				const auto& comps = archetype.entities(kind);

				if GAIA_UNLIKELY (!archetype.has(delEntity)) {
					GAIA_ASSERT2(false, "Trying to remove an entity which wasn't added");
					GAIA_LOG_W("Trying to del an entity from entity [%u.%u] but it was never added", entity.id(), entity.gen());
					GAIA_LOG_W("Currently present:");
					GAIA_EACH(comps) GAIA_LOG_W("> [%u] %s", i, entity_name(world, comps[i]));
					GAIA_LOG_W("Trying to remove:");
					GAIA_LOG_W("> %s (%s)", entity_name(world, delEntity), EntityKindString[kind]);
				}
			}
#endif

			//! Searches for an archetype which is formed by adding entity \param entity of entity kind \param kind to
			//! \param pArchetypeLeft. If no such archetype is found a new one is created.
			//! \param pArchetypeLeft Archetype we originate from.
			//! \param entity Enity we want to add.
			//! \return Archetype pointer.
			GAIA_NODISCARD Archetype* foc_archetype_add(Archetype* pArchetypeLeft, Entity entity) {
				const auto kind = entity.kind();

				// TODO: Add specialization for m_pCompArchetype and m_pEntityArchetype to make this faster
				//       E.g. when adding a new entity we only have to sort from position 2 because the first 2
				//			 indices are taken by the core component. Also hash calculation can be sped up this way.
				// // We don't want to store edges for the root archetype because the more components there are the longer
				// // it would take to find anything. Therefore, for the root archetype we always make a lookup.
				// // Compared to an ordinary lookup this path is stripped as much as possible.
				// if (pArchetypeLeft == m_pRootArchetype) {
				// 	Archetype* pArchetypeRight = nullptr;
				//
				// 	if (kind == EntityKind::EK_Gen) {
				// 		const auto hashGen = descToAdd.hashLookup;
				// 		const auto hashLookup = Archetype::calc_lookup_hash(hashGen, {0});
				// 		pArchetypeRight = find_archetype(hashLookup, ComponentSpan(&descToAdd.comp, 1), {});
				// 		if (pArchetypeRight == nullptr) {
				// 			pArchetypeRight = create_archetype(ComponentSpan(&descToAdd.comp, 1), {});
				// 			pArchetypeRight->set_hashes({hashGen}, {0}, hashLookup);
				// 			pArchetypeRight->build_graph_edges_left(pArchetypeLeft, kind, descToAdd.entity);
				// 			reg_archetype(pArchetypeRight);
				// 		}
				// 	} else {
				// 		const auto hashUni = descToAdd.hashLookup;
				// 		const auto hashLookup = Archetype::calc_lookup_hash({0}, hashUni);
				// 		pArchetypeRight = find_archetype(hashLookup, {}, ComponentSpan(&descToAdd.comp, 1));
				// 		if (pArchetypeRight == nullptr) {
				// 			pArchetypeRight = create_archetype({}, ComponentSpan(&descToAdd.comp, 1));
				// 			pArchetypeRight->set_hashes({0}, {hashUni}, hashLookup);
				// 			pArchetypeRight->build_graph_edges_left(pArchetypeLeft, kind, descToAdd.entity);
				// 			reg_archetype(pArchetypeRight);
				// 		}
				// 	}
				//
				// 	return pArchetypeRight;
				// }

				// Check if the component is found when following the "add" edges
				{
					const auto archetypeId = pArchetypeLeft->find_edge_right(entity);
					if (archetypeId != ArchetypeIdBad)
						return m_archetypesById[archetypeId];
				}

				const uint32_t a = kind;
				const uint32_t b = (kind + 1) & 1;
				const cnt::sarray_ext<Entity, Chunk::MAX_COMPONENTS>* comps[2];

				cnt::sarray_ext<Entity, Chunk::MAX_COMPONENTS> compsNew;
				comps[a] = &compsNew;
				comps[b] = &pArchetypeLeft->entities((EntityKind)b);

				// Prepare a joint array of components of old + the newly added component
				{
					const auto& compsOld = pArchetypeLeft->entities((EntityKind)a);
					const auto componentDescSize = compsOld.size();
					compsNew.resize(componentDescSize + 1);

					GAIA_FOR_(componentDescSize, j) {
						// NOTE: GCC 13 with optimizations enabled has a bug causing stringop-overflow warning to trigger
						//       on the following assignment:
						//       compsNew[j] = compsOld[j];
						//       Therefore, we split the assignment into two parts.
						auto comp = compsOld[j];
						compsNew[j] = comp;
					}
					compsNew[componentDescSize] = entity;
				}

				// Make sure to sort the components so we receive the same hash no matter the order in which components
				// are provided Bubble sort is okay. We're dealing with at most Chunk::MAX_COMPONENTS items.
				sort(compsNew, SortComponentCond{});

				// Once sorted we can calculate the hashes
				const Archetype::GenComponentHash hashGen = {calc_lookup_hash({comps[0]->data(), comps[0]->size()}).hash};
				const Archetype::UniComponentHash hashUni = {calc_lookup_hash({comps[1]->data(), comps[1]->size()}).hash};
				const auto hashLookup = Archetype::calc_lookup_hash(hashGen, hashUni);

				auto* pArchetypeRight =
						find_archetype(hashLookup, {comps[0]->data(), comps[0]->size()}, {comps[1]->data(), comps[1]->size()});
				if (pArchetypeRight == nullptr) {
					pArchetypeRight =
							create_archetype({comps[0]->data(), comps[0]->size()}, {comps[1]->data(), comps[1]->size()});
					pArchetypeRight->set_hashes(hashGen, hashUni, hashLookup);
					pArchetypeLeft->build_graph_edges(pArchetypeRight, entity);
					reg_archetype(pArchetypeRight);
				}

				return pArchetypeRight;
			}

			//! Searches for an archetype which is formed by removing \param kind from \param pArchetypeRight.
			//! If no such archetype is found a new one is created.
			//! \param pArchetypeRight Archetype we originate from.
			//! \param entity Component we want to remove.
			//! \return Pointer to archetype.
			GAIA_NODISCARD Archetype* foc_archetype_del_comp(Archetype* pArchetypeRight, Entity entity) {
				const auto kind = entity.kind();

				// Check if the component is found when following the "del" edges
				{
					const auto archetypeId = pArchetypeRight->find_edge_left(entity);
					if (archetypeId != ArchetypeIdBad)
						return m_archetypesById[archetypeId];
				}

				const uint32_t a = kind;
				const uint32_t b = (kind + 1) & 1;
				const cnt::sarray_ext<Entity, Chunk::MAX_COMPONENTS>* comps[2];

				cnt::sarray_ext<Entity, Chunk::MAX_COMPONENTS> compsNew;
				comps[a] = &compsNew;
				comps[b] = &pArchetypeRight->entities((EntityKind)b);

				// Find the intersection
				for (const auto comp: pArchetypeRight->entities((EntityKind)a)) {
					if (comp == entity)
						continue;

					compsNew.push_back(comp);
				}

				// Return if there's no change
				if (compsNew.size() == pArchetypeRight->comps((EntityKind)a).size())
					return nullptr;

				// Calculate the hashes
				const Archetype::GenComponentHash hashGen = {calc_lookup_hash({comps[0]->data(), comps[0]->size()}).hash};
				const Archetype::UniComponentHash hashUni = {calc_lookup_hash({comps[1]->data(), comps[1]->size()}).hash};
				const auto hashLookup = Archetype::calc_lookup_hash(hashGen, hashUni);

				auto* pArchetype =
						find_archetype(hashLookup, {comps[0]->data(), comps[0]->size()}, {comps[1]->data(), comps[1]->size()});
				if (pArchetype == nullptr) {
					pArchetype = create_archetype({comps[0]->data(), comps[0]->size()}, {comps[1]->data(), comps[1]->size()});
					pArchetype->set_hashes(hashGen, hashLookup, hashLookup);
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
			GAIA_NODISCARD Archetype& archetype(Entity entity) const {
				GAIA_ASSERT(valid(entity));

				return *m_entities[entity.id()].pArchetype;
			}

			//! Removes any name associated with the entity
			//! \param entity Entity the name of which we want to delete
			void del_name(Entity entity) {
				auto& ec = m_entities[entity.id()];
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

			//! Invalidates the entity record, effectivelly deleting it
			//! \param entity Entity to delete
			void del_entity(Entity entity) {
				auto& ec = m_entities.free(entity);
				ec.pArchetype = nullptr;
				ec.pChunk = nullptr;
			}

			//! Associates an entity with a chunk.
			//! \param entity Entity to associate with a chunk
			//! \param pChunk Chunk the entity is to become a part of
			void store_entity(Entity entity, Archetype* pArchetype, Chunk* pChunk) {
				GAIA_ASSERT(pArchetype != nullptr);
				GAIA_ASSERT(pChunk != nullptr);
				GAIA_ASSERT(
						!pChunk->locked() && "Entities can't be added while their chunk is being iterated "
																 "(structural changes are forbidden during this time!)");

				auto& ec = m_entities[entity.id()];
				ec.pArchetype = pArchetype;
				ec.pChunk = pChunk;
				ec.row = pChunk->add_entity(entity);
				GAIA_ASSERT(ec.gen == entity.gen());
				ec.dis = 0;
			}

			/*!
			Moves an entity along with all its generic components from its current chunk to another one.
			\param entity Entity to move
			\param targetArchetype Target archetype
			\param targetChunk Target chunk
			*/
			void move_entity(Entity entity, Archetype& targetArchetype, Chunk& targetChunk) {
				GAIA_PROF_SCOPE(world::move_entity);

				auto* pNewChunk = &targetChunk;

				auto& ec = m_entities[entity.id()];
				auto* pOldChunk = ec.pChunk;

				const auto oldRow0 = ec.row;
				const auto newRow = pNewChunk->add_entity(entity);
				const bool wasEnabled = !ec.dis;

				auto& oldArchetype = *ec.pArchetype;
				auto& newArchetype = targetArchetype;

				// Make sure the old entity becomes enabled now
				oldArchetype.enable_entity(
						pOldChunk, oldRow0, true,
						// entities
						{m_entities.data(), m_entities.size()});
				// Enabling the entity might have changed its chunk index so fetch it again
				const auto oldRow = ec.row;

				// Move data from the old chunk to the new one
				if (newArchetype.id() == oldArchetype.id())
					pNewChunk->move_entity_data(
							entity, newRow,
							// entities
							{m_entities.data(), m_entities.size()});
				else
					pNewChunk->move_foreign_entity_data(
							entity, newRow,
							// entities
							{m_entities.data(), m_entities.size()});

				// Remove the entity record from the old chunk
				remove_entity(pOldChunk, oldRow);

				// Bring the entity container record up-to-date
				ec.pArchetype = &newArchetype;
				ec.pChunk = pNewChunk;
				ec.row = newRow;

				// Make the enabled state in the new chunk match the original state
				newArchetype.enable_entity(
						pNewChunk, newRow, wasEnabled,
						// entities
						{m_entities.data(), m_entities.size()});

				// End-state validation
				GAIA_ASSERT(valid(entity));
				validate_chunk(pOldChunk);
				validate_chunk(pNewChunk);
				validate_entities();
			}

			/*!
			Moves an entity along with all its generic components from its current chunk to another one in a new archetype.
			\param entity Entity to move
			\param newArchetype Target archetype
			*/
			void move_entity(Entity entity, Archetype& newArchetype) {
				auto* pNewChunk = newArchetype.foc_free_chunk();
				move_entity(entity, newArchetype, *pNewChunk);
			}

			//! Verifies than the implicit linked list of entities is valid
			void validate_entities() const {
#if GAIA_ECS_VALIDATE_ENTITY_LIST
				m_entities.validate();
#endif
			}

			//! Verifies that the chunk is valid
			void validate_chunk([[maybe_unused]] Chunk* pChunk) const {
#if GAIA_ECS_VALIDATE_CHUNKS
				// Note: Normally we'd go [[maybe_unused]] instead of "(void)" but MSVC
				// 2017 suffers an internal compiler error in that case...
				(void)pChunk;
				GAIA_ASSERT(pChunk != nullptr);

				if (!pChunk->empty()) {
					// Make sure a proper amount of entities reference the chunk
					uint32_t cnt = 0;
					for (const auto& e: m_entities) {
						if (e.pChunk != pChunk)
							continue;
						++cnt;
					}
					GAIA_ASSERT(cnt == pChunk->size());
				} else {
					// Make sure no entites reference the chunk
					for (const auto& e: m_entities) {
						(void)e;
						GAIA_ASSERT(e.pChunk != pChunk);
					}
				}
#endif
			}

			struct CompMoveHelper {
				World& m_world;
				Entity m_entity;
				Archetype* m_pArchetype;

				CompMoveHelper(World& world, Entity entity): m_world(world), m_entity(entity) {
					GAIA_ASSERT(world.valid(entity));
					auto& ec = world.m_entities[entity.id()];
					m_pArchetype = ec.pArchetype;
				}

				CompMoveHelper(const CompMoveHelper&) = default;
				CompMoveHelper(CompMoveHelper&&) = delete;
				CompMoveHelper& operator=(const CompMoveHelper&) = delete;
				CompMoveHelper& operator=(CompMoveHelper&&) = delete;

				~CompMoveHelper() {
					commit();
				}

				//! Commits all gathered changes and performs an archetype movement
				//! \warning Once called, the object is returned to its default state (as if no add/remove was ever called).
				void commit() {
					if (m_pArchetype == nullptr)
						return;

					// Now that we have the final archetype move the entity to it
					m_world.move_entity(m_entity, *m_pArchetype);

					// Finalize the helper by reseting the archetype pointer
					m_pArchetype = nullptr;
				}

				//! Prepares an archetype movement by following the "add" edge of the current archetype
				//! \param object Added entity
				CompMoveHelper& add(Entity object) {
					GAIA_PROF_SCOPE(CompMoveHelper::add);

#if GAIA_DEBUG
					verify_add(m_world, *m_pArchetype, m_entity, object);
#endif

					m_pArchetype = m_world.foc_archetype_add(m_pArchetype, object);

					return *this;
				}

				template <typename... T>
				CompMoveHelper& add() {
					(verify_comp<T>(entity_kind_v<T>), ...);
					(add(m_world.add<T>().entity), ...);
					return *this;
				}

				//! Prepares an archetype movement by following the "del" edge of the current archetype
				//! \param object Removed entity
				CompMoveHelper& del(Entity object) {
					GAIA_PROF_SCOPE(CompMoveHelper::del);

#if GAIA_DEBUG
					verify_del(m_world, *m_pArchetype, m_entity, object);
#endif

					m_pArchetype = m_world.foc_archetype_del_comp(m_pArchetype, object);

					return *this;
				}

				template <typename... T>
				CompMoveHelper& del() {
					(verify_comp<T>(), ...);
					(del(m_world.add<T>().entity), ...);
					return *this;
				}
			};

			void add_inter(Entity entity, Entity object) {
				CompMoveHelper(*this, entity).add(object);
			}

			void del_inter(Entity entity, Entity object) {
				CompMoveHelper(*this, entity).del(object);
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
					m_pRootArchetype = create_archetype({}, {});
					m_pRootArchetype->set_hashes({0}, {0}, Archetype::calc_lookup_hash({0}, {0}));
					reg_archetype(m_pRootArchetype);
				}

				// Register the core component
				{
					auto comp = add(*m_pRootArchetype, GAIA_ID(Core).kind(), GAIA_ID(Core).entity());
					const auto& desc = comp_cache_mut().add<Core>(GAIA_ID(Core));
					GAIA_ASSERT(desc.entity == GAIA_ID(Core));
					(void)comp;
					(void)desc;
					// add_inter(comp, desc.entity);
				}

				// Register the entity archetype (entity + EntityDesc component)
				{
					auto comp = add(*m_pRootArchetype, GAIA_ID(EntityDesc).kind(), GAIA_ID(EntityDesc).entity());
					const auto& desc = comp_cache_mut().add<EntityDesc>(comp);
					GAIA_ASSERT(desc.entity == GAIA_ID(EntityDesc));
					add_inter(comp, desc.entity);
					sset<EntityDesc>(comp, {desc.name.str(), desc.name.len()});
					m_pEntityArchetype = m_entities[comp.id()].pArchetype;
				}

				// Register the component archetype (entity + EntityDesc + Component)
				{
					auto comp = add(*m_pEntityArchetype, GAIA_ID(Component).kind(), GAIA_ID(Component).entity());
					const auto& desc = comp_cache_mut().add<Component>(comp);
					GAIA_ASSERT(desc.entity == GAIA_ID(Component));
					add_inter(comp, desc.entity);
					set(comp)
							// Entity descriptor
							.sset<EntityDesc>({desc.name.str(), desc.name.len()})
							// Component
							.sset<Component>(desc.comp);
					m_pCompArchetype = m_entities[comp.id()].pArchetype;
				}
			}

			void done() {
				cleanup();

#if GAIA_ECS_CHUNK_ALLOCATOR
				ChunkAllocator::get().flush();
#endif
			}

			//! Creates a new entity of a given archetype
			//! \param archetype Archetype the entity should inherit
			//! \param kind Component kind
			//! \return New entity
			GAIA_NODISCARD Entity add(Archetype& archetype, EntityKind kind, bool isEntity) {
				EntityContainerCtx ctx{kind, isEntity};
				const auto entity = m_entities.alloc(&ctx);

				auto* pChunk = archetype.foc_free_chunk();
				store_entity(entity, &archetype, pChunk);

				// Call constructors for the generic components on the newly added entity if necessary
				if (pChunk->has_custom_gen_ctor())
					pChunk->call_ctors(EntityKind::EK_Gen, pChunk->size() - 1, 1);

#if GAIA_ASSERT_ENABLED
				const auto& ec = m_entities[entity.id()];
				GAIA_ASSERT(ec.pChunk == pChunk);
				auto entityExpected = pChunk->entity_view()[ec.row];
				GAIA_ASSERT(entityExpected == entity);
#endif

				return entity;
			}

			//! Garbage collection
			void gc() {
				GAIA_PROF_SCOPE(world::gc);

				remove_empty_chunks();
				defrag_chunks(m_defragEntitesPerTick);
				remove_empty_archetypes();
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

			GAIA_NODISCARD ComponentCache& comp_cache_mut() {
				return m_compCache;
			}
			GAIA_NODISCARD const ComponentCache& comp_cache() const {
				return m_compCache;
			}

			//! Checks if \param entity is valid.
			//! \return True is the entity is valid. False otherwise.
			GAIA_NODISCARD bool valid(Entity entity) const {
				// Entity ID has to fit inside the entity array
				if (entity.id() >= m_entities.size())
					return false;

				const auto& ec = m_entities[entity.id()];

				// Generation ID has to match the one in the array
				if (ec.gen != entity.gen())
					return false;

				// The entity in the chunk must match the index in the entity container
				auto* pChunk = ec.pChunk;
				return pChunk != nullptr && pChunk->entity_view()[ec.row] == entity;
			}

			//! Checks if \param entity is currently used by the world.
			//! \return True is the entity is used. False otherwise.
			GAIA_NODISCARD bool has(Entity entity) const {
				// Entity ID has to fit inside the entity array
				if (entity.id() >= m_entities.size())
					return false;

				// Index of the entity must fit inside the chunk
				const auto& ec = m_entities[entity.id()];
				auto* pChunk = ec.pChunk;
				return pChunk != nullptr && ec.row < pChunk->size();
			}

			//! Clears the world so that all its entities and components are released
			void cleanup() {
				// Clear entities
				m_entities = {};

				// Clear archetypes
				{
					// Delete all allocated chunks and their parent archetypes
					for (auto pair: m_archetypesById)
						delete pair.second;

					m_archetypesById = {};
					m_archetypesByHash = {};
					m_chunksToRemove = {};
					m_archetypesToRemove = {};
				}

				// Clear caches
				{
					m_componentToArchetypeMap = {};
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

			//----------------------------------------------------------------------

			//! Returns the current version of the world.
			//! \return World version number.
			GAIA_NODISCARD uint32_t& world_version() {
				return m_worldVersion;
			}

			//----------------------------------------------------------------------

			//! Creates a new empty entity
			//! \return New entity
			GAIA_NODISCARD Entity add() {
				return add(*m_pEntityArchetype, EntityKind::EK_Gen, true);
			}

			//! Creates a new entity by cloning an already existing one.
			//! \param entity Entity to clone
			//! \return New entity
			GAIA_NODISCARD Entity copy(Entity entity) {
				auto& ec = m_entities[entity.id()];

				GAIA_ASSERT(ec.pChunk != nullptr);
				GAIA_ASSERT(ec.pArchetype != nullptr);

				auto& archetype = *ec.pArchetype;
				const auto newEntity = add(archetype, entity.kind(), entity.entity());

				Chunk::copy_entity_data(
						entity, newEntity,
						// entities
						{m_entities.data(), m_entities.size()});

				return newEntity;
			}

			//! Removes an entity along with all data associated with it.
			//! \param entity Entity to delete
			void del(Entity entity) {
				if (m_entities.item_count() == 0 || entity == IdentifierBad)
					return;

				GAIA_ASSERT(valid(entity));

				const auto& ec = m_entities[entity.id()];
				auto* pChunk = ec.pChunk;
				GAIA_ASSERT(pChunk != nullptr);

				// Remove the entity from its chunk.
				// We call del_name first because remove_entity calls component destructors.
				// If the call was made inside del_entity we would access a memory location
				// which has already been destructed which is not nice.
				del_name(entity);
				remove_entity(pChunk, ec.row);
				del_entity(entity);

				// End-state validation
				validate_chunk(pChunk);
				validate_entities();
			}

			//! Returns the entity located at the index \param id
			//! \return Entity
			GAIA_NODISCARD Entity get(EntityId id) const {
				GAIA_ASSERT(id < m_entities.size());
				const auto& ec = m_entities[id];
#if GAIA_ASSERT_ENABLED
				if (ec.pChunk != nullptr) {
					auto entityExpected = ec.pChunk->entity_view()[ec.row];
					GAIA_ASSERT(entityExpected == Entity(id, ec.gen, (bool)ec.ent, (EntityKind)ec.kind));
				}
#endif
				return Entity(id, ec.gen, (bool)ec.ent, (EntityKind)ec.kind);
			}

			//! Enables or disables an entire entity.
			//! \param entity Entity
			//! \param enable Enable or disable the entity
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			void enable(Entity entity, bool enable) {
				auto& ec = m_entities[entity.id()];

				GAIA_ASSERT(
						(ec.pChunk && !ec.pChunk->locked()) &&
						"Entities can't be enabled/disabled while their chunk is being iterated "
						"(structural changes are forbidden during this time!)");

				auto& archetype = *ec.pArchetype;
				archetype.enable_entity(
						ec.pChunk, ec.row, enable,
						// entities
						{m_entities.data(), m_entities.size()});
			}

			//! Checks if an entity is enabled.
			//! \param entity Entity
			//! \return True it the entity is enabled. False otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			bool enabled(Entity entity) const {
				GAIA_ASSERT(valid(entity));

				const auto& ec = m_entities[entity.id()];
				const bool entityStateInContainer = !ec.dis;
#if GAIA_ASSERT_ENABLED
				const bool entityStateInChunk = ec.pChunk->enabled(ec.row);
				GAIA_ASSERT(entityStateInChunk == entityStateInContainer);
#endif
				return entityStateInContainer;
			}

			//! Returns the number of active entities
			//! \return Entity
			GAIA_NODISCARD GAIA_FORCEINLINE uint32_t size() const {
				return m_entities.item_count();
			}

			//----------------------------------------------------------------------

			//! Returns a chunk containing the \param entity.
			//! \return Chunk or nullptr if not found.
			GAIA_NODISCARD Chunk* get_chunk(Entity entity) const {
				GAIA_ASSERT(entity.id() < m_entities.size());
				const auto& ec = m_entities[entity.id()];
				return ec.pChunk;
			}

			//! Returns a chunk containing the \param entity.
			//! Index of the entity is stored in \param indexInChunk
			//! \return Chunk or nullptr if not found
			GAIA_NODISCARD Chunk* get_chunk(Entity entity, uint32_t& indexInChunk) const {
				GAIA_ASSERT(entity.id() < m_entities.size());
				const auto& ec = m_entities[entity.id()];
				indexInChunk = ec.row;
				return ec.pChunk;
			}

			//----------------------------------------------------------------------

			//! Starts a bulk add/remove operation on \param entity.
			//! \param entity Entity
			//! \return CompMoveHelper
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			CompMoveHelper bulk(Entity entity) {
				return CompMoveHelper(*this, entity);
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

				const auto entity = add(*m_pCompArchetype, kind, false);
				const auto& desc = comp_cache_mut().add<FT>(entity);

				// Following lines do the following but a bit faster:
				// sset<Component>(comp, desc.comp);
				auto& ec = m_entities[entity.id()];
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
				GAIA_ASSERT(valid(entity));
				GAIA_ASSERT(valid(object));
				add_inter(entity, object);
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
				constexpr auto kind = CT::Kind;

				verify_comp<T>(kind);
				GAIA_ASSERT(valid(entity));

				const auto& desc = add<FT>();
				add_inter(entity, desc.entity);
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
				constexpr auto kind = CT::Kind;

				verify_comp<T>(kind);
				GAIA_ASSERT(valid(entity));

				const auto& desc = add<FT>();
				add_inter(entity, desc.entity);

				const auto& ec = m_entities[entity.id()];

				if constexpr (kind == EntityKind::EK_Gen) {
					ec.pChunk->template set<FT>(ec.row, GAIA_FWD(value));
				} else {
					ec.pChunk->template set<FT>(GAIA_FWD(value));
				}
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

				verify_comp<T>();
				GAIA_ASSERT(valid(entity));

				const auto& desc = add<FT>();
				del_inter(entity, desc.entity);
			}

			//----------------------------------------------------------------------

			//! Starts a bulk set operation on \param entity.
			//! \param entity Entity
			//! \return ComponentSetter
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			//! \warning Undefined behavior if \param entity changes archetype after ComponentSetter is created.
			ComponentSetter set(Entity entity) {
				GAIA_ASSERT(valid(entity));

				const auto& ec = m_entities[entity.id()];
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

				const auto& ec = m_entities[entity.id()];
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

				const auto& ec = m_entities[entity.id()];
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

				const auto& ec = m_entities[entity.id()];
				return ComponentGetter{ec.pChunk, ec.row}.get<T>();
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

				const auto& ec = m_entities[entity.id()];
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

			//! Returns the entity that is assigned with the \param name.
			//! \param name Name
			//! \return Entity assigned the given name.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			GAIA_NODISCARD Entity get(const char* name) const {
				if (name == nullptr)
					return IdentifierBad;

				const auto it = m_nameToEntity.find(EntityNameLookupKey(name));
				if (it == m_nameToEntity.end())
					return IdentifierBad;

				return it->second;
			}

			//----------------------------------------------------------------------

			//! Provides a query set up to work with the parent world.
			//! \tparam UseCache If true, results of the query are cached
			//! \return Valid query object
			template <bool UseCache = true>
			auto query() {
				if constexpr (UseCache)
					return Query(
							*const_cast<World*>(this), m_queryCache,
							//
							m_nextArchetypeId, m_worldVersion, m_archetypesById, m_componentToArchetypeMap);
				else
					return QueryUncached(
							*const_cast<World*>(this),
							//
							m_nextArchetypeId, m_worldVersion, m_archetypesById, m_componentToArchetypeMap);
			}

			//----------------------------------------------------------------------

			//! Performs various internal operations related to the end of the frame such as
			//! memory cleanup and other managment operations which keep the system healthy.
			void update() {
				gc();

				// Signal the end of the frame
				GAIA_PROF_FRAME();
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

				GAIA_LOG_N("Deleted entities: %u", (uint32_t)m_entities.get_free_items());
				if (m_entities.get_free_items() != 0U) {
					GAIA_LOG_N("  --> %u", (uint32_t)m_entities.get_next_free_item());

					uint32_t iters = 0;
					auto fe = m_entities[m_entities.get_next_free_item()].idx;
					while (fe != IdentifierIdBad) {
						GAIA_LOG_N("  --> %u", m_entities[fe].idx);
						fe = m_entities[fe].idx;
						++iters;
						if (iters > m_entities.get_free_items())
							break;
					}

					if ((iters == 0U) || iters > m_entities.get_free_items())
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
	} // namespace ecs
} // namespace gaia
