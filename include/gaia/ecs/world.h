#pragma once
#include "../config/config.h"

#include <cinttypes>
#include <type_traits>

#include "../cnt/darray.h"
#include "../cnt/ilist.h"
#include "../cnt/map.h"
#include "../cnt/sarray.h"
#include "../cnt/sarray_ext.h"
#include "../cnt/set.h"
#include "../config/profiler.h"
#include "../core/hashing_policy.h"
#include "../core/span.h"
#include "../core/utility.h"
#include "../meta/type_info.h"
#include "archetype.h"
#include "archetype_common.h"
#include "chunk.h"
#include "chunk_allocator.h"
#include "common.h"
#include "component.h"
#include "component_cache.h"
#include "component_getter.h"
#include "component_setter.h"
#include "component_utils.h"
#include "entity.h"
#include "query.h"
#include "query_cache.h"
#include "query_common.h"

namespace gaia {
	namespace ecs {
		class GAIA_API World final {
			friend class ECSSystem;
			friend class ECSSystemManager;
			friend class CommandBuffer;
			friend void* AllocateChunkMemory(World& world);
			friend void ReleaseChunkMemory(World& world, void* mem);

			//! Cache of queries
			QueryCache m_queryCache;
			//! Cache of query ids to speed up each
			cnt::map<ComponentLookupHash, QueryId> m_uniqueFuncQueryPairs;
			//! Map of compId -> archetype matches.
			ComponentToArchetypeMap m_componentToArchetypeMap;

			//! Map of archetypes mapping to the same hash - used for lookups
			cnt::map<ArchetypeLookupKey, Archetype*> m_archetypeMap;
			//! List of archetypes - used for iteration
			ArchetypeList m_archetypes;

			//! Implicit list of entities. Used for look-ups only when searching for
			//! entities in chunks + data validation
			cnt::ilist<EntityContainer, Entity> m_entities;

			//! List of chunks to delete
			cnt::darray<Chunk*> m_chunksToRemove;
			//! ID of the last defragmented archetype
			uint32_t m_defragLastArchetypeID = 0;

			//! With every structural change world version changes
			uint32_t m_worldVersion = 0;

		private:
			//! Remove an entity from chunk.
			//! \param pChunk Chunk we remove the entity from
			//! \param entityChunkIndex Index of entity within its chunk
			//! \tparam IsEntityDeleteWanted True if entity is to be deleted. False otherwise.
			template <bool IsEntityDeleteWanted>
			void remove_entity(Chunk* pChunk, uint32_t entityChunkIndex) {
				GAIA_PROF_SCOPE(remove_entity);

				const auto entity = pChunk->get_entity(entityChunkIndex);
				pChunk->remove_entity(entityChunkIndex, {m_entities.data(), m_entities.size()}, m_chunksToRemove);

				pChunk->update_versions();
				if constexpr (IsEntityDeleteWanted)
					del_entity(entity);
			}

			//! defragments chunks.
			//! \param maxEntites Maximum number of entities moved per call
			void defrag_chunks(uint32_t maxEntities) {
				const auto maxIters = (uint32_t)m_archetypes.size();
				for (uint32_t i = 0; i < maxIters; ++i) {
					m_defragLastArchetypeID = (m_defragLastArchetypeID + i) % maxIters;

					auto* pArchetype = m_archetypes[m_defragLastArchetypeID];
					pArchetype->defrag(maxEntities, m_chunksToRemove, {m_entities.data(), m_entities.size()});
					if (maxEntities == 0)
						return;
				}
			}

			//! Searches for archetype with a given set of components
			//! \param lookupHash Archetype lookup hash
			//! \param compIdsGeneric Span of generic component ids
			//! \param compIdsChunk Span of chunk component ids
			//! \return Pointer to archetype or nullptr.
			GAIA_NODISCARD Archetype*
			find_archetype(Archetype::LookupHash lookupHash, ComponentIdSpan compIdsGeneric, ComponentIdSpan compIdsChunk) {
				auto tmpArchetype = ArchetypeLookupChecker(compIdsGeneric, compIdsChunk);
				ArchetypeLookupKey key(lookupHash, &tmpArchetype);

				// Search for the archetype in the map
				const auto it = m_archetypeMap.find(key);
				if (it == m_archetypeMap.end())
					return nullptr;

				auto* pArchetype = it->second;
				return pArchetype;
			}

			//! Creates a new archetype from a given set of components
			//! \param compIdsGeneric Span of generic component infos
			//! \param compIdsChunk Span of chunk component infos
			//! \return Pointer to the new archetype.
			GAIA_NODISCARD Archetype* create_archetype(ComponentIdSpan compIdsGeneric, ComponentIdSpan compIdsChunk) {
				auto* pArchetype =
						Archetype::create((ArchetypeId)m_archetypes.size(), m_worldVersion, compIdsGeneric, compIdsChunk);

				auto registerComponentToArchetypePair = [&](ComponentId compId) {
					const auto it = m_componentToArchetypeMap.find(compId);
					if (it == m_componentToArchetypeMap.end())
						m_componentToArchetypeMap.try_emplace(compId, ArchetypeList{pArchetype});
					else if (!core::has(it->second, pArchetype))
						it->second.push_back(pArchetype);
				};

				for (const auto compId: compIdsGeneric)
					registerComponentToArchetypePair(compId);
				for (const auto compId: compIdsChunk)
					registerComponentToArchetypePair(compId);

				return pArchetype;
			}

			//! Registers the archetype in the world.
			//! \param pArchetype Archetype to register.
			void reg_archetype(Archetype* pArchetype) {
				// Make sure hashes were set already
				GAIA_ASSERT(
						(m_archetypes.empty() || pArchetype == m_archetypes[0]) ||
						(pArchetype->generic_hash().hash != 0 || pArchetype->chunk_hash().hash != 0));
				GAIA_ASSERT((m_archetypes.empty() || pArchetype == m_archetypes[0]) || pArchetype->lookup_hash().hash != 0);

				// Make sure the archetype is not registered yet
				GAIA_ASSERT(!core::has(m_archetypes, pArchetype));

				// Register the archetype
				m_archetypes.push_back(pArchetype);
				m_archetypeMap.emplace(ArchetypeLookupKey(pArchetype->lookup_hash(), pArchetype), pArchetype);
			}

#if GAIA_DEBUG
			static void
			verify_add(Archetype& archetype, Entity entity, ComponentType compType, const ComponentInfo& infoToAdd) {
				const auto& compIds = archetype.comp_ids(compType);
				const auto& cc = ComponentCache::get();

				// Make sure not to add too many infos
				if GAIA_UNLIKELY (compIds.size() + 1 >= Chunk::MAX_COMPONENTS) {
					GAIA_ASSERT(false && "Trying to add too many components to entity!");
					GAIA_LOG_W(
							"Trying to add a component to entity [%u.%u] but there's no space left!", entity.id(), entity.gen());
					GAIA_LOG_W("Already present:");
					const uint32_t oldInfosCount = compIds.size();
					for (uint32_t i = 0; i < oldInfosCount; ++i) {
						const auto& info = cc.comp_desc(compIds[i]);
						GAIA_LOG_W("> [%u] %.*s", (uint32_t)i, (uint32_t)info.name.size(), info.name.data());
					}
					GAIA_LOG_W("Trying to add:");
					{
						const auto& info = cc.comp_desc(infoToAdd.compId);
						GAIA_LOG_W("> %.*s", (uint32_t)info.name.size(), info.name.data());
					}
				}

				// Don't add the same component twice
				for (uint32_t i = 0; i < compIds.size(); ++i) {
					const auto& info = cc.comp_desc(compIds[i]);
					if (info.compId == infoToAdd.compId) {
						GAIA_ASSERT(false && "Trying to add a duplicate component");

						GAIA_LOG_W(
								"Trying to add a duplicate of component %s to entity [%u.%u]", ComponentTypeString[compType],
								entity.id(), entity.gen());
						GAIA_LOG_W("> %.*s", (uint32_t)info.name.size(), info.name.data());
					}
				}
			}

			static void
			verify_del(Archetype& archetype, Entity entity, ComponentType compType, const ComponentInfo& infoToRemove) {
				const auto& compIds = archetype.comp_ids(compType);
				if GAIA_UNLIKELY (!core::has(compIds, infoToRemove.compId)) {
					GAIA_ASSERT(false && "Trying to remove a component which wasn't added");
					GAIA_LOG_W(
							"Trying to remove a component from entity [%u.%u] but it was never added", entity.id(), entity.gen());
					GAIA_LOG_W("Currently present:");

					const auto& cc = ComponentCache::get();

					for (uint32_t k = 0; k < compIds.size(); k++) {
						const auto& info = cc.comp_desc(compIds[k]);
						GAIA_LOG_W("> [%u] %.*s", (uint32_t)k, (uint32_t)info.name.size(), info.name.data());
					}

					{
						GAIA_LOG_W("Trying to remove:");
						const auto& info = cc.comp_desc(infoToRemove.compId);
						GAIA_LOG_W("> %.*s", (uint32_t)info.name.size(), info.name.data());
					}
				}
			}
#endif

			//! Searches for an archetype which is formed by adding \param compType to \param pArchetypeLeft.
			//! If no such archetype is found a new one is created.
			//! \param pArchetypeLeft Archetype we originate from.
			//! \param compType Component infos.
			//! \param infoToAdd Component we want to add.
			//! \return Pointer to archetype.
			GAIA_NODISCARD Archetype*
			foc_archetype_add_comp(Archetype* pArchetypeLeft, ComponentType compType, const ComponentInfo& infoToAdd) {
				// We don't want to store edges for the root archetype because the more components there are the longer
				// it would take to find anything. Therefore, for the root archetype we always make a lookup.
				// Compared to an ordinary lookup this path is stripped as much as possible.
				if (pArchetypeLeft == m_archetypes[0]) {
					Archetype* pArchetypeRight = nullptr;

					if (compType == ComponentType::CT_Generic) {
						const auto genericHash = infoToAdd.lookupHash;
						const auto lookupHash = Archetype::calc_lookup_hash(genericHash, {0});
						pArchetypeRight = find_archetype(lookupHash, ComponentIdSpan(&infoToAdd.compId, 1), {});
						if (pArchetypeRight == nullptr) {
							pArchetypeRight = create_archetype(ComponentIdSpan(&infoToAdd.compId, 1), {});
							pArchetypeRight->set_hashes({genericHash}, {0}, lookupHash);
							pArchetypeRight->build_graph_edges_left(pArchetypeLeft, compType, infoToAdd.compId);
							reg_archetype(pArchetypeRight);
						}
					} else {
						const auto chunkHash = infoToAdd.lookupHash;
						const auto lookupHash = Archetype::calc_lookup_hash({0}, chunkHash);
						pArchetypeRight = find_archetype(lookupHash, {}, ComponentIdSpan(&infoToAdd.compId, 1));
						if (pArchetypeRight == nullptr) {
							pArchetypeRight = create_archetype({}, ComponentIdSpan(&infoToAdd.compId, 1));
							pArchetypeRight->set_hashes({0}, {chunkHash}, lookupHash);
							pArchetypeRight->build_graph_edges_left(pArchetypeLeft, compType, infoToAdd.compId);
							reg_archetype(pArchetypeRight);
						}
					}

					return pArchetypeRight;
				}

				// Check if the component is found when following the "add" edges
				{
					const auto archetypeId = pArchetypeLeft->find_edge_right(compType, infoToAdd.compId);
					if (archetypeId != ArchetypeIdBad)
						return m_archetypes[archetypeId];
				}

				const uint32_t a = compType;
				const uint32_t b = (compType + 1) & 1;
				const cnt::sarray_ext<uint32_t, Chunk::MAX_COMPONENTS>* infos[2];

				cnt::sarray_ext<uint32_t, Chunk::MAX_COMPONENTS> infosNew;
				infos[a] = &infosNew;
				infos[b] = &pArchetypeLeft->comp_ids((ComponentType)b);

				// Prepare a joint array of component infos of old + the newly added component
				{
					const auto& compIds = pArchetypeLeft->comp_ids((ComponentType)a);
					const auto componentInfosSize = compIds.size();
					infosNew.resize(componentInfosSize + 1);

					for (uint32_t j = 0; j < componentInfosSize; ++j)
						infosNew[j] = compIds[j];
					infosNew[componentInfosSize] = infoToAdd.compId;
				}

				// Make sure to sort the component infos so we receive the same hash no matter the order in which components
				// are provided Bubble sort is okay. We're dealing with at most Chunk::MAX_COMPONENTS items.
				sort(infosNew);

				// Once sorted we can calculate the hashes
				const Archetype::GenericComponentHash genericHash = {
						calc_lookup_hash({infos[0]->data(), infos[0]->size()}).hash};
				const Archetype::ChunkComponentHash chunkHash = {calc_lookup_hash({infos[1]->data(), infos[1]->size()}).hash};
				const auto lookupHash = Archetype::calc_lookup_hash(genericHash, chunkHash);

				auto* pArchetypeRight =
						find_archetype(lookupHash, {infos[0]->data(), infos[0]->size()}, {infos[1]->data(), infos[1]->size()});
				if (pArchetypeRight == nullptr) {
					pArchetypeRight =
							create_archetype({infos[0]->data(), infos[0]->size()}, {infos[1]->data(), infos[1]->size()});
					pArchetypeRight->set_hashes(genericHash, chunkHash, lookupHash);
					pArchetypeLeft->build_graph_edges(pArchetypeRight, compType, infoToAdd.compId);
					reg_archetype(pArchetypeRight);
				}

				return pArchetypeRight;
			}

			//! Searches for an archetype which is formed by removing \param compType from \param pArchetypeRight.
			//! If no such archetype is found a new one is created.
			//! \param pArchetypeRight Archetype we originate from.
			//! \param compType Component infos.
			//! \param infoToRemove Component we want to remove.
			//! \return Pointer to archetype.
			GAIA_NODISCARD Archetype*
			foc_archetype_remove_comp(Archetype* pArchetypeRight, ComponentType compType, const ComponentInfo& infoToRemove) {
				// Check if the component is found when following the "del" edges
				{
					const auto archetypeId = pArchetypeRight->find_edge_left(compType, infoToRemove.compId);
					if (archetypeId != ArchetypeIdBad)
						return m_archetypes[archetypeId];
				}

				const uint32_t a = compType;
				const uint32_t b = (compType + 1) & 1;
				const cnt::sarray_ext<uint32_t, Chunk::MAX_COMPONENTS>* infos[2];

				cnt::sarray_ext<uint32_t, Chunk::MAX_COMPONENTS> infosNew;
				infos[a] = &infosNew;
				infos[b] = &pArchetypeRight->comp_ids((ComponentType)b);

				// Find the intersection
				for (const auto compId: pArchetypeRight->comp_ids((ComponentType)a)) {
					if (compId == infoToRemove.compId)
						continue;

					infosNew.push_back(compId);
				}

				// Return if there's no change
				if (infosNew.size() == pArchetypeRight->comp_ids((ComponentType)a).size())
					return nullptr;

				// Calculate the hashes
				const Archetype::GenericComponentHash genericHash = {
						calc_lookup_hash({infos[0]->data(), infos[0]->size()}).hash};
				const Archetype::ChunkComponentHash chunkHash = {calc_lookup_hash({infos[1]->data(), infos[1]->size()}).hash};
				const auto lookupHash = Archetype::calc_lookup_hash(genericHash, chunkHash);

				auto* pArchetype =
						find_archetype(lookupHash, {infos[0]->data(), infos[0]->size()}, {infos[1]->data(), infos[1]->size()});
				if (pArchetype == nullptr) {
					pArchetype = create_archetype({infos[0]->data(), infos[0]->size()}, {infos[1]->data(), infos[1]->size()});
					pArchetype->set_hashes(genericHash, lookupHash, lookupHash);
					pArchetype->build_graph_edges(pArchetypeRight, compType, infoToRemove.compId);
					reg_archetype(pArchetype);
				}

				return pArchetype;
			}

			//! Returns an array of archetypes registered in the world
			//! \return Array or archetypes.
			const auto& get_archetypes() const {
				return m_archetypes;
			}

			//! Returns the archetype the entity belongs to.
			//! \param entity Entity
			//! \return Reference to the archetype.
			GAIA_NODISCARD Archetype& get_archetype(Entity entity) const {
				GAIA_ASSERT(valid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				auto* pChunk = entityContainer.pChunk;
				return *m_archetypes[pChunk == nullptr ? ArchetypeId(0) : pChunk->archetype_id()];
			}

			//! Invalidates the entity record, effectivelly deleting it
			//! \param entity Entity to delete
			void del_entity(Entity entity) {
				auto& entityContainer = m_entities.free(entity);
				entityContainer.pChunk = nullptr;
			}

			//! Associates an entity with a chunk.
			//! \param entity Entity to associate with a chunk
			//! \param pChunk Chunk the entity is to become a part of
			void store_entity(Entity entity, Chunk* pChunk) {
				GAIA_ASSERT(pChunk != nullptr);
				GAIA_ASSERT(
						!pChunk->has_structural_changes() && "Entities can't be added while their chunk is being iterated "
																								 "(structural changes are forbidden during this time!)");

				auto& entityContainer = m_entities[entity.id()];
				entityContainer.pChunk = pChunk;
				entityContainer.idx = pChunk->add_entity(entity);
				entityContainer.gen = entity.gen();
				entityContainer.dis = 0;
			}

			/*!
			Moves an entity along with all its generic components from its current chunk to another one.
			\param oldEntity Entity to move
			\param targetChunk Target chunk
			*/
			void move_entity(Entity oldEntity, Chunk& targetChunk) {
				GAIA_PROF_SCOPE(move_entity);

				auto* pNewChunk = &targetChunk;

				auto& entityContainer = m_entities[oldEntity.id()];
				auto* pOldChunk = entityContainer.pChunk;

				const auto oldIndex0 = entityContainer.idx;
				const auto newIndex = pNewChunk->add_entity(oldEntity);
				const bool wasEnabled = !entityContainer.dis;

				auto& oldArchetype = *m_archetypes[pOldChunk->archetype_id()];
				auto& newArchetype = *m_archetypes[pNewChunk->archetype_id()];

				// Make sure the old entity becomes enabled now
				oldArchetype.enable_entity(pOldChunk, oldIndex0, true, {m_entities.data(), m_entities.size()});
				// Enabling the entity might have changed the index so fetch it again
				const auto oldIndex = entityContainer.idx;

				// No data movement necessary when dealing with the root archetype
				if GAIA_LIKELY (pNewChunk->archetype_id() + pOldChunk->archetype_id() != 0) {
					// Move data from the old chunk to the new one
					if (pOldChunk->archetype_id() == pNewChunk->archetype_id())
						pNewChunk->move_entity_data(oldEntity, newIndex, {m_entities.data(), m_entities.size()});
					else
						pNewChunk->move_foreign_entity_data(oldEntity, newIndex, {m_entities.data(), m_entities.size()});
				}

				// Transfer the original enabled state to the new archetype
				newArchetype.enable_entity(pNewChunk, newIndex, wasEnabled, {m_entities.data(), m_entities.size()});

				// Remove the entity record from the old chunk
				remove_entity<false>(pOldChunk, oldIndex);

				// Make the entity point to the new chunk
				entityContainer.pChunk = pNewChunk;
				entityContainer.idx = newIndex;
				entityContainer.gen = oldEntity.gen();
				GAIA_ASSERT((bool)entityContainer.dis == !wasEnabled);
				// entityContainer.dis = !wasEnabled;

				// End-state validation
				validate_chunk(pOldChunk);
				validate_chunk(pNewChunk);
				validate_entities();
			}

			/*!
			Moves an entity along with all its generic components from its current chunk to another one in a new archetype.
			\param oldEntity Entity to move
			\param newArchetype Target archetype
			*/
			void move_entity(Entity oldEntity, Archetype& newArchetype) {
				auto* pNewChunk = newArchetype.foc_free_chunk();
				return move_entity(oldEntity, *pNewChunk);
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

				if (pChunk->has_entities()) {
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

			EntityContainer& add_inter(ComponentType compType, Entity entity, const ComponentInfo& infoToAdd) {
				GAIA_PROF_SCOPE(AddComponent);

				auto& entityContainer = m_entities[entity.id()];

				auto* pChunk = entityContainer.pChunk;

				GAIA_ASSERT(pChunk != nullptr);
				GAIA_ASSERT(
						!pChunk->has_structural_changes() && "New components can't be added while their chunk is being iterated "
																								 "(structural changes are forbidden during this time!)");

				// Adding a component to an entity which already is a part of some chunk
				{
					auto& archetype = *m_archetypes[pChunk->archetype_id()];

#if GAIA_DEBUG
					verify_add(archetype, entity, compType, infoToAdd);
#endif

					auto* pTargetArchetype = foc_archetype_add_comp(&archetype, compType, infoToAdd);
					move_entity(entity, *pTargetArchetype);
					pChunk = entityContainer.pChunk;
				}

				// Call the constructor for the newly added component if necessary
				if (compType == ComponentType::CT_Generic)
					pChunk->call_ctor(compType, infoToAdd.compId, entityContainer.idx);
				else if (compType == ComponentType::CT_Chunk)
					pChunk->call_ctor(compType, infoToAdd.compId, 0);

				return entityContainer;
			}

			ComponentSetter del_inter(ComponentType compType, Entity entity, const ComponentInfo& infoToRemove) {
				GAIA_PROF_SCOPE(del);

				auto& entityContainer = m_entities[entity.id()];
				auto* pChunk = entityContainer.pChunk;

				GAIA_ASSERT(pChunk != nullptr);
				GAIA_ASSERT(
						!pChunk->has_structural_changes() && "Components can't be removed while their chunk is being iterated "
																								 "(structural changes are forbidden during this time!)");

				auto& archetype = *m_archetypes[pChunk->archetype_id()];

#if GAIA_DEBUG
				verify_del(archetype, entity, compType, infoToRemove);
#endif

				auto* pNewArchetype = foc_archetype_remove_comp(&archetype, compType, infoToRemove);
				GAIA_ASSERT(pNewArchetype != nullptr);
				move_entity(entity, *pNewArchetype);

				return ComponentSetter{pChunk, entityContainer.idx};
			}

			void init() {
				auto* pRootArchetype = create_archetype({}, {});
				pRootArchetype->set_hashes({0}, {0}, Archetype::calc_lookup_hash({0}, {0}));
				reg_archetype(pRootArchetype);
			}

			void done() {
				cleanup();

				ChunkAllocator::get().flush();

#if GAIA_DEBUG && GAIA_ECS_CHUNK_ALLOCATOR
				// Make sure there are no leaks
				ChunkAllocatorStats memStats = ChunkAllocator::get().stats();
				for (const auto& s: memStats.stats) {
					if (s.mem_total != 0) {
						GAIA_ASSERT(false && "ECS leaking memory");
						GAIA_LOG_W("ECS leaking memory!");
						ChunkAllocator::get().diag();
					}
				}
#endif
			}

			//! Creates a new entity of a given archetype
			//! \param archetype Archetype the entity should inherit
			//! \return New entity
			GAIA_NODISCARD Entity add(Archetype& archetype) {
				const auto entity = m_entities.alloc();

				auto* pChunk = archetype.foc_free_chunk();
				store_entity(entity, pChunk);

				// Call constructors for the generic components on the newly added entity if necessary
				if (pChunk->has_custom_generic_ctor())
					pChunk->call_ctors(ComponentType::CT_Generic, pChunk->size() - 1, 1);

				return entity;
			}

			//! Garbage collection. Checks all chunks and archetypes which are empty and have not been
			//! used for a while and tries to delete them and release memory allocated by them.
			void GC() {
				// Handle chunks
				for (uint32_t i = 0; i < m_chunksToRemove.size();) {
					auto* pChunk = m_chunksToRemove[i];

					// Skip reclaimed chunks
					if (pChunk->has_entities()) {
						pChunk->prepare_to_die();
						core::erase_fast(m_chunksToRemove, i);
						continue;
					}

					if (pChunk->progress_death()) {
						++i;
						continue;
					}
				}

				// Remove all dead chunks
				for (auto* pChunk: m_chunksToRemove) {
					auto& archetype = *m_archetypes[pChunk->archetype_id()];
					archetype.remove_chunk(pChunk);
				}
				m_chunksToRemove.clear();

				// Defragment chunks only now. If we did this at the begging of the function,
				// we would needlessly iterate chunks which have no way of being collected because
				// it would be their first frame dying. This way, the number of chunks to process
				// is lower.
				defrag_chunks(GAIA_DEFRAG_ENTITIES_PER_FRAME);
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

			//! Checks if \param entity is valid.
			//! \return True is the entity is valid. False otherwise.
			GAIA_NODISCARD bool valid(Entity entity) const {
				// Entity ID has to fit inside the entity array
				if (entity.id() >= m_entities.size())
					return false;

				const auto& entityContainer = m_entities[entity.id()];

				// Generation ID has to match the one in the array
				if (entityContainer.gen != entity.gen())
					return false;

				// The entity in the chunk must match the index in the entity container
				auto* pChunk = entityContainer.pChunk;
				return pChunk != nullptr && pChunk->get_entity(entityContainer.idx) == entity;
			}

			//! Checks if \param entity is currently used by the world.
			//! \return True is the entity is used. False otherwise.
			GAIA_NODISCARD bool has(Entity entity) const {
				// Entity ID has to fit inside the entity array
				if (entity.id() >= m_entities.size())
					return false;

				// Index of the entity must fit inside the chunk
				const auto& entityContainer = m_entities[entity.id()];
				auto* pChunk = entityContainer.pChunk;
				return pChunk != nullptr && entityContainer.idx < pChunk->size();
			}

			//! Clears the world so that all its entities and components are released
			void cleanup() {
				// Clear entities
				m_entities.clear();

				// Clear archetypes
				{
					// Delete all allocated chunks and their parent archetypes
					for (auto* pArchetype: m_archetypes)
						delete pArchetype;

					m_archetypes = {};
					m_archetypeMap = {};
					m_chunksToRemove = {};
				}
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
				return add(*m_archetypes[0]);
			}

			//! Creates a new entity by cloning an already existing one.
			//! \param entity Entity to clone
			//! \return New entity
			GAIA_NODISCARD Entity add(Entity entity) {
				auto& entityContainer = m_entities[entity.id()];

				auto* pChunk = entityContainer.pChunk;
				GAIA_ASSERT(pChunk != nullptr);

				auto& archetype = *m_archetypes[pChunk->archetype_id()];
				const auto newEntity = add(archetype);

				Chunk::copy_entity_data(entity, newEntity, {m_entities.data(), m_entities.size()});

				return newEntity;
			}

			//! Removes an entity along with all data associated with it.
			//! \param entity Entity to delete
			void del(Entity entity) {
				if (m_entities.item_count() == 0 || entity == EntityNull)
					return;

				GAIA_ASSERT(valid(entity));

				const auto& entityContainer = m_entities[entity.id()];

				// Remove entity from chunk
				if (auto* pChunk = entityContainer.pChunk) {
					remove_entity<true>(pChunk, entityContainer.idx);
					validate_chunk(pChunk);
					validate_entities();
				} else {
					del_entity(entity);
				}
			}

			//! Enables or disables an entire entity.
			//! \param entity Entity
			//! \param enable Enable or disable the entity
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			void enable(Entity entity, bool enable) {
				auto& entityContainer = m_entities[entity.id()];

				GAIA_ASSERT(
						(!entityContainer.pChunk || !entityContainer.pChunk->has_structural_changes()) &&
						"Entities can't be enabled/disabled while their chunk is being iterated "
						"(structural changes are forbidden during this time!)");

				if (auto* pChunk = entityContainer.pChunk) {
					auto& archetype = *m_archetypes[pChunk->archetype_id()];
					archetype.enable_entity(pChunk, entityContainer.idx, enable, {m_entities.data(), m_entities.size()});
				}
			}

			//! Checks if an entity is valid.
			//! \param entity Entity
			//! \return True it the entity is valid. False otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			bool enabled(Entity entity) const {
				GAIA_ASSERT(valid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				const bool entityStateInContainer = !entityContainer.dis;
#if GAIA_ASSERT_ENABLED
				const bool entityStateInChunk = entityContainer.pChunk->enabled(entityContainer.idx);
				GAIA_ASSERT(entityStateInChunk == entityStateInContainer);
#endif
				return entityStateInContainer;
			}

			//! Returns the number of active entities
			//! \return Entity
			GAIA_NODISCARD GAIA_FORCEINLINE uint32_t size() const {
				return m_entities.item_count();
			}

			//! Returns an entity at the index \param idx
			//! \return Entity
			GAIA_NODISCARD Entity get(uint32_t idx) const {
				GAIA_ASSERT(idx < m_entities.size());
				const auto& entityContainer = m_entities[idx];
				return {idx, entityContainer.gen};
			}

			//! Returns a chunk containing the \param entity.
			//! \return Chunk or nullptr if not found.
			GAIA_NODISCARD Chunk* get_chunk(Entity entity) const {
				GAIA_ASSERT(entity.id() < m_entities.size());
				const auto& entityContainer = m_entities[entity.id()];
				return entityContainer.pChunk;
			}

			//! Returns a chunk containing the \param entity.
			//! Index of the entity is stored in \param indexInChunk
			//! \return Chunk or nullptr if not found
			GAIA_NODISCARD Chunk* get_chunk(Entity entity, uint32_t& indexInChunk) const {
				GAIA_ASSERT(entity.id() < m_entities.size());
				const auto& entityContainer = m_entities[entity.id()];
				indexInChunk = entityContainer.idx;
				return entityContainer.pChunk;
			}

			//----------------------------------------------------------------------

			//! Attaches a new component \tparam T to \param entity.
			//! \tparam T Component
			//! \param entity Entity
			//! \return ComponentSetter
			//! \warning It is expected the component is not present on \param entity yet. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename T>
			ComponentSetter add(Entity entity) {
				verify_comp<T>();
				GAIA_ASSERT(valid(entity));

				using U = typename component_type_t<T>::Type;
				const auto& info = ComponentCache::get().goc_comp_info<U>();

				constexpr auto compType = component_type_v<T>;
				auto& entityContainer = add_inter(compType, entity, info);
				return ComponentSetter{entityContainer.pChunk, entityContainer.idx};
			}

			//! Attaches a new component \tparam T to \param entity. Also sets its value.
			//! \tparam T Component
			//! \param entity Entity
			//! \param value Value to set for the component
			//! \return ComponentSetter object.
			//! \warning It is expected the component is not present on \param entity yet. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename T, typename U = typename component_type_t<T>::Type>
			ComponentSetter add(Entity entity, U&& value) {
				verify_comp<T>();
				GAIA_ASSERT(valid(entity));

				const auto& info = ComponentCache::get().goc_comp_info<U>();

				if constexpr (component_type_v<T> == ComponentType::CT_Generic) {
					auto& entityContainer = add_inter(ComponentType::CT_Generic, entity, info);
					auto* pChunk = entityContainer.pChunk;
					pChunk->template set<T>(entityContainer.idx, std::forward<U>(value));
					return ComponentSetter{entityContainer.pChunk, entityContainer.idx};
				} else {
					auto& entityContainer = add_inter(ComponentType::CT_Chunk, entity, info);
					auto* pChunk = entityContainer.pChunk;
					pChunk->template set<T>(std::forward<U>(value));
					return ComponentSetter{entityContainer.pChunk, entityContainer.idx};
				}
			}

			//! Removes a component \tparam T from \param entity.
			//! \tparam T Component
			//! \param entity Entity
			//! \return ComponentSetter
			//! \warning It is expected the component is present on \param entity. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename T>
			ComponentSetter del(Entity entity) {
				verify_comp<T>();
				GAIA_ASSERT(valid(entity));

				using U = typename component_type_t<T>::Type;
				const auto& info = ComponentCache::get().goc_comp_info<U>();

				constexpr auto compType = component_type_v<T>;
				return del_inter(compType, entity, info);
			}

			//----------------------------------------------------------------------

			//! Sets the value of the component \tparam T on \param entity.
			//! \tparam T Component
			//! \param entity Entity
			//! \param value Value to set for the component
			//! \return ComponentSetter
			//! \warning It is expected the component is present on \param entity. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename T, typename U = typename component_type_t<T>::Type>
			ComponentSetter set(Entity entity, U&& value) {
				GAIA_ASSERT(valid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				return ComponentSetter{entityContainer.pChunk, entityContainer.idx}.set<T>(std::forward<U>(value));
			}

			//! Sets the value of the component \tparam T on \param entity without trigger a world version update.
			//! \tparam T Component
			//! \param entity Entity
			//! \param value Value to set for the component
			//! \return ComponentSetter
			//! \warning It is expected the component is present on \param entity. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename T, typename U = typename component_type_t<T>::Type>
			ComponentSetter sset(Entity entity, U&& value) {
				GAIA_ASSERT(valid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				return ComponentSetter{entityContainer.pChunk, entityContainer.idx}.sset<T>(std::forward<U>(value));
			}

			//----------------------------------------------------------------------

			//! Returns the value stored in the component \tparam T on \param entity.
			//! \tparam T Component
			//! \param entity Entity
			//! \return Value stored in the component.
			//! \warning It is expected the component is present on \param entity. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename T>
			GAIA_NODISCARD auto get(Entity entity) const {
				GAIA_ASSERT(valid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				return ComponentGetter{entityContainer.pChunk, entityContainer.idx}.get<T>();
			}

			//! Tells if \param entity contains the component \tparam T.
			//! \tparam T Component
			//! \param entity Entity
			//! \return True if the component is present on entity.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename T>
			GAIA_NODISCARD bool has(Entity entity) const {
				GAIA_ASSERT(valid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				return ComponentGetter{entityContainer.pChunk, entityContainer.idx}.has<T>();
			}

		private:
			template <typename... T>
			static void unpack_args_into_query(Query& query, [[maybe_unused]] core::func_type_list<T...> types) {
				static_assert(sizeof...(T) > 0, "Inputs-less functors can not be unpacked to query");
				query.all<T...>();
			}

			template <typename... T>
			static void reg_comps_inter([[maybe_unused]] core::func_type_list<T...> types) {
				static_assert(sizeof...(T) > 0, "Empty Query is not supported in this context");
				auto& cc = ComponentCache::get();
				((void)cc.goc_comp_info<T>(), ...);
			}

			template <typename Func>
			static void reg_comps() {
				using InputArgs = decltype(core::func_args(&Func::operator()));
				reg_comps_inter(InputArgs{});
			}

			template <typename... T>
			static constexpr ComponentLookupHash
			calc_query_id_lookup_hash([[maybe_unused]] core::func_type_list<T...> types) {
				return calc_lookup_hash<T...>();
			}

		public:
			template <bool UseCache = true>
			auto query() {
				if constexpr (UseCache)
					return Query(m_queryCache, m_worldVersion, m_archetypes, m_componentToArchetypeMap);
				else
					return QueryUncached(m_worldVersion, m_archetypes, m_componentToArchetypeMap);
			}

			//! Iterates over all chunks satisfying conditions set by \param func and calls \param func for all of them.
			//! Query instance is generated internally from the input arguments of \param func.
			//! \warning Performance-wise it has less potential than iterating using ecs::Chunk or a comparable each
			//!          passing in a query because it needs to do cached query lookups on each invocation. However, it is
			//!          easier to use and for non-critical code paths it is the most elegant way of iterating your data.
			template <typename Func>
			void each(Func func) {
				using InputArgs = decltype(core::func_args(&Func::operator()));

				reg_comps<Func>();

				constexpr auto lookupHash = calc_query_id_lookup_hash(InputArgs{});
				if (m_uniqueFuncQueryPairs.count(lookupHash) == 0) {
					Query q = query();
					unpack_args_into_query(q, InputArgs{});
					(void)q.fetch_query_info();
					m_uniqueFuncQueryPairs.try_emplace(lookupHash, q.id());
					query().each(q.id(), func);
				} else {
					const auto queryId = m_uniqueFuncQueryPairs[lookupHash];
					query().each(queryId, func);
				}
			}

			//! Performs various internal operations related to the end of the frame such as
			//! memory cleanup and other various managment operations which keep the system healthy.
			void update() {
				GC();

				// Signal the end of the frame
				GAIA_PROF_FRAME();
			}

			//--------------------------------------------------------------------------------

			//! Performs diagnostics on archetypes. Prints basic info about them and the chunks they contain.
			void diag_archetypes() const {
				GAIA_LOG_N("Archetypes:%u", (uint32_t)m_archetypes.size());
				for (const auto* archetype: m_archetypes)
					Archetype::diag(*archetype);
			}

			//! Performs diagnostics on registered components.
			//! Prints basic info about them and reports and detected issues.
			static void diag_components() {
				ComponentCache::get().diag();
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
					while (fe != Entity::IdMask) {
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

			/*!
			Performs all diagnostics.
			*/
			void diag() const {
				diag_archetypes();
				diag_components();
				diag_entities();
			}
		};
	} // namespace ecs
} // namespace gaia
