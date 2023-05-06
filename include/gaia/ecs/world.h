#pragma once
#include <cinttypes>
#include <type_traits>

#include "../config/config.h"
#include "../config/profiler.h"
#include "../containers/darray.h"
#include "../containers/map.h"
#include "../containers/sarray.h"
#include "../containers/sarray_ext.h"
#include "../containers/set.h"
#include "../utils/hashing_policy.h"
#include "../utils/span.h"
#include "../utils/type_info.h"
#include "../utils/utility.h"
#include "archetype.h"
#include "archetype_common.h"
#include "chunk.h"
#include "chunk_allocator.h"
#include "common.h"
#include "component.h"
#include "component_cache.h"
#include "component_utils.h"
#include "entity.h"
#include "entity_query.h"
#include "entity_query_cache.h"
#include "entity_query_common.h"

namespace gaia {
	namespace ecs {

		//----------------------------------------------------------------------

		struct ComponentSetter {
			Chunk* m_pChunk;
			uint32_t m_idx;

			template <typename T>
			ComponentSetter& SetComponent(typename component::DeduceComponent<T>::Type&& data) {
				using U = typename component::DeduceComponent<T>::Type;
				if constexpr (component::IsGenericComponent<T>)
					m_pChunk->template SetComponent<T>(m_idx, std::forward<U>(data));
				else
					m_pChunk->template SetComponent<T>(std::forward<U>(data));
				return *this;
			}

			template <typename T>
			ComponentSetter& SetComponentSilent(typename component::DeduceComponent<T>::Type&& data) {
				using U = typename component::DeduceComponent<T>::Type;
				if constexpr (component::IsGenericComponent<T>)
					m_pChunk->template SetComponentSilent<T>(m_idx, std::forward<U>(data));
				else
					m_pChunk->template SetComponentSilent<T>(std::forward<U>(data));
				return *this;
			}
		};

		//----------------------------------------------------------------------

		class GAIA_API World final {
			friend class ECSSystem;
			friend class ECSSystemManager;
			friend class CommandBuffer;
			friend void* AllocateChunkMemory(World& world);
			friend void ReleaseChunkMemory(World& world, void* mem);

			//! Cache of entity queries
			EntityQueryCache m_queryCache;
			//! Cache of query ids to speed up ForEach
			containers::map<component::ComponentLookupHash, query::QueryId> m_uniqueFuncQueryPairs;
			//! Map or archetypes mapping to the same hash - used for lookups
			containers::map<archetype::LookupHash, archetype::ArchetypeList> m_archetypeMap;
			//! List of archetypes - used for iteration
			archetype::ArchetypeList m_archetypes;
			//! Root archetype
			archetype::Archetype* m_pRootArchetype = nullptr;

			//! Implicit list of entities. Used for look-ups only when searching for
			//! entities in chunks + data validation
			containers::darray<EntityContainer> m_entities;
			//! Index of the next entity to recycle
			uint32_t m_nextFreeEntity = Entity::IdMask;
			//! Number of entites to recycle
			uint32_t m_freeEntities = 0;

			//! List of chunks to delete
			containers::darray<Chunk*> m_chunksToRemove;

			//! With every structural change world version changes
			uint32_t m_worldVersion = 0;

		private:
			/*!
			Remove an entity from chunk.
			\param pChunk Chunk we remove the entity from
			\param entityChunkIndex Index of entity within its chunk
			*/
			void RemoveEntity(Chunk* pChunk, uint32_t entityChunkIndex) {
				GAIA_ASSERT(
						!pChunk->IsStructuralChangesLocked() && "Entities can't be removed while their chunk is being iterated "
																										"(structural changes are forbidden during this time!)");

				pChunk->RemoveEntity(
						entityChunkIndex, m_entities, pChunk->GetComponentIdArray(component::ComponentType::CT_Generic),
						pChunk->GetComponentOffsetArray(component::ComponentType::CT_Generic));

				if (!pChunk->IsDying() && !pChunk->HasEntities()) {
					// When the chunk is emptied we want it to be removed. We can't do it
					// right away and need to wait for world's GC to be called.
					//
					// However, we need to prevent the following:
					//    1) chunk is emptied + add to some removal list
					//    2) chunk is reclaimed
					//    3) chunk is emptied again + add to some removal list again
					//
					// Therefore, we have a flag telling us the chunk is already waiting to
					// be removed. The chunk might be reclaimed before GC happens but it
					// simply ignores such requests. This way GC always has at most one
					// record for removal for any given chunk.
					pChunk->PrepareToDie();

					m_chunksToRemove.push_back(pChunk);
				}
			}

			/*!
			Searches for archetype with a given set of components
			\param lookupHash Archetype lookup hash
			\param componentIdsGeneric Span of generic component ids
			\param componentIdsChunk Span of chunk component ids
			\return Pointer to archetype or nullptr
			*/
			GAIA_NODISCARD archetype::Archetype* FindArchetype(
					archetype::LookupHash lookupHash, component::ComponentIdSpan componentIdsGeneric,
					component::ComponentIdSpan componentIdsChunk) {
				// Search for the archetype in the map
				const auto it = m_archetypeMap.find(lookupHash);
				if (it == m_archetypeMap.end())
					return nullptr;

				const auto& archetypeArray = it->second;
				GAIA_ASSERT(!archetypeArray.empty());

				// More than one archetype can have the same lookup key. However, this should be extermely
				// rare (basically it should never happen). For this reason, only search for the exact match
				// if we happen to have more archetypes under the same hash.
				if GAIA_LIKELY (archetypeArray.size() == 1)
					return archetypeArray[0];

				auto checkComponentIds = [&](const archetype::ComponentIdArray& componentIdsArchetype,
																		 component::ComponentIdSpan componentIds) {
					for (uint32_t j = 0; j < componentIds.size(); j++) {
						// Different components. We need to search further
						if (componentIdsArchetype[j] != componentIds[j])
							return false;
					}
					return true;
				};

				// Iterate over the list of archetypes and find the exact match
				for (auto* pArchetype: archetypeArray) {
					const auto& genericComponentList = pArchetype->GetComponentIdArray(component::ComponentType::CT_Generic);
					if (genericComponentList.size() != componentIdsGeneric.size())
						continue;
					const auto& chunkComponentList = pArchetype->GetComponentIdArray(component::ComponentType::CT_Chunk);
					if (chunkComponentList.size() != componentIdsChunk.size())
						continue;

					if (checkComponentIds(genericComponentList, componentIdsGeneric) &&
							checkComponentIds(chunkComponentList, componentIdsChunk))
						return pArchetype;
				}

				return nullptr;
			}

			/*!
			Creates a new archetype from a given set of components
			\param componentIdsGeneric Span of generic component infos
			\param componentIdsChunk Span of chunk component infos
			\return Pointer to the new archetype
			*/
			GAIA_NODISCARD archetype::Archetype*
			CreateArchetype(component::ComponentIdSpan componentIdsGeneric, component::ComponentIdSpan componentIdsChunk) {
				return archetype::Archetype::Create(
						(archetype::ArchetypeId)m_archetypes.size(), m_worldVersion, componentIdsGeneric, componentIdsChunk);
			}

			/*!
			Registers the archetype in the world.
			\param pArchetype Archetype to register
			*/
			void RegisterArchetype(archetype::Archetype* pArchetype) {
				// Make sure hashes were set already
				GAIA_ASSERT(
						pArchetype == m_pRootArchetype ||
						(pArchetype->GetGenericHash().hash != 0 || pArchetype->GetChunkHash().hash != 0));
				GAIA_ASSERT(pArchetype == m_pRootArchetype || pArchetype->GetLookupHash().hash != 0);

				// Make sure the archetype is not registered yet
				GAIA_ASSERT(!utils::has(m_archetypes, pArchetype));

				// Register the archetype
				m_archetypes.push_back(pArchetype);

				auto it = m_archetypeMap.find(pArchetype->GetLookupHash());
				if (it == m_archetypeMap.end()) {
					m_archetypeMap[pArchetype->GetLookupHash()] = {pArchetype};
				} else {
					auto& archetypes = it->second;
					GAIA_ASSERT(!utils::has(archetypes, pArchetype));
					archetypes.push_back(pArchetype);
				}
			}

#if GAIA_DEBUG
			static void VerifyAddComponent(
					archetype::Archetype& archetype, Entity entity, component::ComponentType componentType,
					const component::ComponentInfo& infoToAdd) {
				const auto& componentIds = archetype.GetComponentIdArray(componentType);
				const auto& cc = ComponentCache::Get();

				// Make sure not to add too many infos
				if GAIA_UNLIKELY (!archetype::VerifyArchetypeComponentCount(1)) {
					GAIA_ASSERT(false && "Trying to add too many components to entity!");
					GAIA_LOG_W(
							"Trying to add a component to entity [%u.%u] but there's no space left!", entity.id(), entity.gen());
					GAIA_LOG_W("Already present:");
					const size_t oldInfosCount = componentIds.size();
					for (size_t i = 0; i < oldInfosCount; i++) {
						const auto& info = cc.GetComponentDesc(componentIds[i]);
						GAIA_LOG_W("> [%u] %.*s", (uint32_t)i, (uint32_t)info.name.size(), info.name.data());
					}
					GAIA_LOG_W("Trying to add:");
					{
						const auto& info = cc.GetComponentDesc(infoToAdd.componentId);
						GAIA_LOG_W("> %.*s", (uint32_t)info.name.size(), info.name.data());
					}
				}

				// Don't add the same component twice
				for (size_t i = 0; i < componentIds.size(); ++i) {
					const auto& info = cc.GetComponentDesc(componentIds[i]);
					if (info.componentId == infoToAdd.componentId) {
						GAIA_ASSERT(false && "Trying to add a duplicate component");

						GAIA_LOG_W(
								"Trying to add a duplicate of component %s to entity [%u.%u]",
								component::ComponentTypeString[componentType], entity.id(), entity.gen());
						GAIA_LOG_W("> %.*s", (uint32_t)info.name.size(), info.name.data());
					}
				}
			}

			static void VerifyRemoveComponent(
					archetype::Archetype& archetype, Entity entity, component::ComponentType componentType,
					const component::ComponentInfo& infoToRemove) {
				const auto& componentIds = archetype.GetComponentIdArray(componentType);
				if GAIA_UNLIKELY (!utils::has(componentIds, infoToRemove.componentId)) {
					GAIA_ASSERT(false && "Trying to remove a component which wasn't added");
					GAIA_LOG_W(
							"Trying to remove a component from entity [%u.%u] but it was never added", entity.id(), entity.gen());
					GAIA_LOG_W("Currently present:");

					const auto& cc = ComponentCache::Get();

					for (size_t k = 0; k < componentIds.size(); k++) {
						const auto& info = cc.GetComponentDesc(componentIds[k]);
						GAIA_LOG_W("> [%u] %.*s", (uint32_t)k, (uint32_t)info.name.size(), info.name.data());
					}

					{
						GAIA_LOG_W("Trying to remove:");
						const auto& info = cc.GetComponentDesc(infoToRemove.componentId);
						GAIA_LOG_W("> %.*s", (uint32_t)info.name.size(), info.name.data());
					}
				}
			}
#endif

#if GAIA_ARCHETYPE_GRAPH
			static GAIA_FORCEINLINE void BuildGraphEdges(
					component::ComponentType componentType, archetype::Archetype* left, archetype::Archetype* right,
					component::ComponentId componentId) {
				left->AddGraphEdgeRight(componentType, componentId, right->GetArchetypeId());
				right->AddGraphEdgeLeft(componentType, componentId, left->GetArchetypeId());
			}
#endif

			/*!
			Searches for an archetype which is formed by adding \param componentType to \param pArchetypeLeft.
			If no such archetype is found a new one is created.
			\param pArchetypeLeft Archetype we originate from.
			\param componentType Component infos.
			\param infoToAdd Component we want to add.
			\return Pointer to archetype
			*/
			GAIA_NODISCARD archetype::Archetype* FindOrCreateArchetype_AddComponent(
					archetype::Archetype* pArchetypeLeft, component::ComponentType componentType,
					const component::ComponentInfo& infoToAdd) {
#if GAIA_ARCHETYPE_GRAPH
				// We don't want to store edges for the root archetype because the more components there are the longer
				// it would take to find anything. Therefore, for the root archetype we always make a lookup.
				// However, compared to an oridnary lookup, this path is stripped as much as possible.
				if (pArchetypeLeft == m_pRootArchetype) {
					archetype::Archetype* pArchetypeRight = nullptr;
					if (componentType == component::ComponentType::CT_Generic) {
						const auto genericHash = infoToAdd.lookupHash;
						const auto lookupHash = archetype::Archetype::CalculateLookupHash(genericHash, {0});
						pArchetypeRight = FindArchetype(lookupHash, component::ComponentIdSpan(&infoToAdd.componentId, 1), {});
						if (pArchetypeRight == nullptr) {
							pArchetypeRight = CreateArchetype(component::ComponentIdSpan(&infoToAdd.componentId, 1), {});
							pArchetypeRight->Init({genericHash}, {0}, lookupHash);
							RegisterArchetype(pArchetypeRight);
							BuildGraphEdges(componentType, pArchetypeLeft, pArchetypeRight, infoToAdd.componentId);
						}
					} else {
						const auto chunkHash = infoToAdd.lookupHash;
						const auto lookupHash = archetype::Archetype::CalculateLookupHash({0}, chunkHash);
						pArchetypeRight = FindArchetype(lookupHash, {}, component::ComponentIdSpan(&infoToAdd.componentId, 1));
						if (pArchetypeRight == nullptr) {
							pArchetypeRight = CreateArchetype({}, component::ComponentIdSpan(&infoToAdd.componentId, 1));
							pArchetypeRight->Init({0}, {chunkHash}, lookupHash);
							RegisterArchetype(pArchetypeRight);
							BuildGraphEdges(componentType, pArchetypeLeft, pArchetypeRight, infoToAdd.componentId);
						}
					}

					return pArchetypeRight;
				}

				// Check if the component is found when following the "add" edges
				{
					const uint32_t archetypeId = pArchetypeLeft->FindGraphEdgeRight(componentType, infoToAdd.componentId);
					if (archetypeId != archetype::ArchetypeIdBad)
						return m_archetypes[archetypeId];
				}
#endif

				const uint32_t a = componentType;
				const uint32_t b = (componentType + 1) & 1;
				const containers::sarray_ext<uint32_t, archetype::MAX_COMPONENTS_PER_ARCHETYPE>* infos[2];

				containers::sarray_ext<uint32_t, archetype::MAX_COMPONENTS_PER_ARCHETYPE> infosNew;
				infos[a] = &infosNew;
				infos[b] = &pArchetypeLeft->GetComponentIdArray((component::ComponentType)b);

				// Prepare a joint array of component infos of old + the newly added component
				{
					const auto& componentIds = pArchetypeLeft->GetComponentIdArray((component::ComponentType)a);
					const size_t componentInfosSize = componentIds.size();
					infosNew.resize(componentInfosSize + 1);

					for (size_t j = 0; j < componentInfosSize; ++j)
						infosNew[j] = componentIds[j];
					infosNew[componentInfosSize] = infoToAdd.componentId;
				}

				// Make sure to sort the component infos so we receive the same hash no matter the order in which components
				// are provided Bubble sort is okay. We're dealing with at most MAX_COMPONENTS_PER_ARCHETYPE items.
				component::SortComponents(infosNew);

				// Once sorted we can calculate the hashes
				const archetype::Archetype::GenericComponentHash genericHash = {
						component::CalculateLookupHash({*infos[0]}).hash};
				const archetype::Archetype::ChunkComponentHash chunkHash = {component::CalculateLookupHash({*infos[1]}).hash};
				const auto lookupHash = archetype::Archetype::CalculateLookupHash(genericHash, chunkHash);

				auto* pArchetypeRight =
						FindArchetype(lookupHash, {infos[0]->data(), infos[0]->size()}, {infos[1]->data(), infos[1]->size()});
				if (pArchetypeRight == nullptr) {
					pArchetypeRight = CreateArchetype({infos[0]->data(), infos[0]->size()}, {infos[1]->data(), infos[1]->size()});
					pArchetypeRight->Init(genericHash, chunkHash, lookupHash);
					RegisterArchetype(pArchetypeRight);

#if GAIA_ARCHETYPE_GRAPH
					// Build the graph edges so that the next time we want to add this component we can do it the quick way
					BuildGraphEdges(componentType, pArchetypeLeft, pArchetypeRight, infoToAdd.componentId);
#endif
				}

				return pArchetypeRight;
			}

			/*!
			Searches for an archetype which is formed by removing \param componentType from \param pArchetypeRight.
			If no such archetype is found a new one is created.
			\param pArchetypeRight Archetype we originate from.
			\param componentType Component infos.
			\param infoToRemove Component we want to remove.
			\return Pointer to archetype
			*/
			GAIA_NODISCARD archetype::Archetype* FindOrCreateArchetype_RemoveComponent(
					archetype::Archetype* pArchetypeRight, component::ComponentType componentType,
					const component::ComponentInfo& infoToRemove) {
#if GAIA_ARCHETYPE_GRAPH
				// Check if the component is found when following the "del" edges
				{
					const auto archetypeId = pArchetypeRight->FindGraphEdgeLeft(componentType, infoToRemove.componentId);
					if (archetypeId != archetype::ArchetypeIdBad)
						return m_archetypes[archetypeId];
				}
#endif

				const uint32_t a = componentType;
				const uint32_t b = (componentType + 1) & 1;
				const containers::sarray_ext<uint32_t, archetype::MAX_COMPONENTS_PER_ARCHETYPE>* infos[2];

				containers::sarray_ext<uint32_t, archetype::MAX_COMPONENTS_PER_ARCHETYPE> infosNew;
				infos[a] = &infosNew;
				infos[b] = &pArchetypeRight->GetComponentIdArray((component::ComponentType)b);

				// Find the intersection
				for (const auto componentId: pArchetypeRight->GetComponentIdArray((component::ComponentType)a)) {
					if (componentId == infoToRemove.componentId)
						goto nextIter;

					infosNew.push_back(componentId);

				nextIter:
					continue;
				}

				// Return if there's no change
				if (infosNew.size() == pArchetypeRight->GetComponentIdArray((component::ComponentType)a).size())
					return nullptr;

				// Calculate the hashes
				const archetype::Archetype::GenericComponentHash genericHash = {
						component::CalculateLookupHash({*infos[0]}).hash};
				const archetype::Archetype::ChunkComponentHash chunkHash = {component::CalculateLookupHash({*infos[1]}).hash};
				const auto lookupHash = archetype::Archetype::CalculateLookupHash(genericHash, chunkHash);

				auto* pArchetype = FindArchetype(lookupHash, {*infos[0]}, {*infos[1]});
				if (pArchetype == nullptr) {
					pArchetype = CreateArchetype({infos[0]->data(), infos[0]->size()}, {infos[1]->data(), infos[1]->size()});
					pArchetype->Init(genericHash, lookupHash, lookupHash);
					RegisterArchetype(pArchetype);

#if GAIA_ARCHETYPE_GRAPH
					// Build the graph edges so that the next time we want to remove this component we can do it the quick way
					BuildGraphEdges(componentType, pArchetype, pArchetypeRight, infoToRemove.componentId);
#endif
				}

				return pArchetype;
			}

			/*!
			Returns an array of archetypes registered in the world
			\return Array or archetypes
			*/
			const auto& GetArchetypes() const {
				return m_archetypes;
			}

			/*!
			Returns the archetype the entity belongs to.
			\param entity Entity
			\return Reference to the archetype
			*/
			GAIA_NODISCARD archetype::Archetype& GetArchetype(Entity entity) const {
				GAIA_ASSERT(IsEntityValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				auto* pChunk = entityContainer.pChunk;
				return pChunk == nullptr ? *m_pRootArchetype : *m_archetypes[pChunk->GetArchetypeId()];
			}

			/*!
			Allocates a new entity.
			\return Entity
			*/
			GAIA_NODISCARD Entity AllocateEntity() {
				if GAIA_UNLIKELY (!m_freeEntities) {
					const auto entityCnt = m_entities.size();
					// We don't want to go out of range for new entities
					GAIA_ASSERT(entityCnt < Entity::IdMask && "Trying to allocate too many entities!");

					m_entities.push_back({});
					return {(EntityId)entityCnt, 0};
				}

				// Make sure the list is not broken
				GAIA_ASSERT(m_nextFreeEntity < m_entities.size() && "ECS recycle list broken!");

				--m_freeEntities;
				const auto index = m_nextFreeEntity;
				m_nextFreeEntity = m_entities[m_nextFreeEntity].idx;
				return {index, m_entities[index].gen};
			}

			/*!
			Deallocates a new entity.
			\param entityToDelete Entity to delete
			*/
			void DeallocateEntity(Entity entityToDelete) {
				auto& entityContainer = m_entities[entityToDelete.id()];
				entityContainer.pChunk = nullptr;

				// New generation
				const auto gen = ++entityContainer.gen;

				// Update our implicit list
				if GAIA_UNLIKELY (!m_freeEntities) {
					m_nextFreeEntity = entityToDelete.id();
					entityContainer.idx = Entity::IdMask;
					entityContainer.gen = gen;
				} else {
					entityContainer.idx = m_nextFreeEntity;
					entityContainer.gen = gen;
					m_nextFreeEntity = entityToDelete.id();
				}
				++m_freeEntities;
			}

			/*!
			Associates an entity with a chunk.
			\param entity Entity to associate with a chunk
			\param pChunk Chunk the entity is to become a part of
			*/
			void StoreEntity(Entity entity, Chunk* pChunk) {
				GAIA_ASSERT(pChunk != nullptr);
				GAIA_ASSERT(
						!pChunk->IsStructuralChangesLocked() && "Entities can't be added while their chunk is being iterated "
																										"(structural changes are forbidden during this time!)");

				auto& entityContainer = m_entities[entity.id()];
				entityContainer.pChunk = pChunk;
				entityContainer.idx = pChunk->AddEntity(entity);
				entityContainer.gen = entity.gen();
			}

			/*!
			Moves an entity along with all its generic components from its current to another
			chunk in a new archetype.
			\param oldEntity Entity to move
			\param newArchetype Target archetype
			*/
			void MoveEntity(Entity oldEntity, archetype::Archetype& newArchetype) {
				const auto& cc = ComponentCache::Get();

				auto& entityContainer = m_entities[oldEntity.id()];
				auto* pOldChunk = entityContainer.pChunk;
				const auto oldIndex = entityContainer.idx;
				const auto& oldArchetype = *m_archetypes[pOldChunk->GetArchetypeId()];

				// Find a new chunk for the entity and move it inside.
				// Old entity ID needs to remain valid or lookups would break.
				auto* pNewChunk = newArchetype.FindOrCreateFreeChunk();
				const auto newIndex = pNewChunk->AddEntity(oldEntity);

				// Find intersection of the two component lists.
				// We ignore chunk components here because they should't be influenced
				// by entities moving around.
				const auto& oldInfos = oldArchetype.GetComponentIdArray(component::ComponentType::CT_Generic);
				const auto& newInfos = newArchetype.GetComponentIdArray(component::ComponentType::CT_Generic);
				const auto& oldOffs = oldArchetype.GetComponentOffsetArray(component::ComponentType::CT_Generic);
				const auto& newOffs = newArchetype.GetComponentOffsetArray(component::ComponentType::CT_Generic);

				// Arrays are sorted so we can do linear intersection lookup
				{
					size_t i = 0;
					size_t j = 0;

					auto moveData = [&](const component::ComponentDesc& descOld, const component::ComponentDesc& descNew) {
						// Let's move all type data from oldEntity to newEntity
						const auto idxSrc = oldOffs[i++] + descOld.properties.size * oldIndex;
						const auto idxDst = newOffs[j++] + descOld.properties.size * newIndex;

						GAIA_ASSERT(idxSrc < Chunk::DATA_SIZE_NORESERVE);
						GAIA_ASSERT(idxDst < Chunk::DATA_SIZE_NORESERVE);

						auto* pSrc = (void*)&pOldChunk->GetData(idxSrc);
						auto* pDst = (void*)&pNewChunk->GetData(idxDst);

						if (descNew.properties.movable == 1) {
							const auto& desc = cc.GetComponentDesc(descNew.componentId);
							desc.move(pSrc, pDst);
						} else if (descNew.properties.copyable == 1) {
							const auto& desc = cc.GetComponentDesc(descNew.componentId);
							desc.copy(pSrc, pDst);
						} else
							memmove(pDst, (const void*)pSrc, descOld.properties.size);
					};

					while (i < oldInfos.size() && j < newInfos.size()) {
						const auto& descOld = cc.GetComponentDesc(oldInfos[i]);
						const auto& descNew = cc.GetComponentDesc(newInfos[j]);

						if (&descOld == &descNew)
							moveData(descOld, descNew);
						else if (component::SortComponentCond{}.operator()(descOld.componentId, descNew.componentId))
							++i;
						else
							++j;
					}
				}

				// Remove entity from the previous chunk
				RemoveEntity(pOldChunk, oldIndex);

				// Update entity's chunk and index so look-ups can find it
				entityContainer.pChunk = pNewChunk;
				entityContainer.idx = newIndex;
				entityContainer.gen = oldEntity.gen();

				ValidateChunk(pOldChunk);
				ValidateChunk(pNewChunk);
				ValidateEntityList();
			}

			//! Verifies than the implicit linked list of entities is valid
			void ValidateEntityList() const {
#if GAIA_ECS_VALIDATE_ENTITY_LIST
				bool hasThingsToRemove = m_freeEntities > 0;
				if (!hasThingsToRemove)
					return;

				// If there's something to remove there has to be at least one
				// entity left
				GAIA_ASSERT(!m_entities.empty());

				auto freeEntities = m_freeEntities;
				auto nextFreeEntity = m_nextFreeEntity;
				while (freeEntities > 0) {
					GAIA_ASSERT(nextFreeEntity < m_entities.size() && "ECS recycle list broken!");

					nextFreeEntity = m_entities[nextFreeEntity].idx;
					--freeEntities;
				}

				// At this point the index of the last index in list should
				// point to -1 because that's the tail of our implicit list.
				GAIA_ASSERT(nextFreeEntity == Entity::IdMask);
#endif
			}

			//! Verifies than the chunk is valid
			void ValidateChunk([[maybe_unused]] Chunk* pChunk) const {
#if GAIA_ECS_VALIDATE_CHUNKS
				// Note: Normally we'd go [[maybe_unused]] instead of "(void)" but MSVC
				// 2017 suffers an internal compiler error in that case...
				(void)pChunk;
				GAIA_ASSERT(pChunk != nullptr);

				if (pChunk->HasEntities()) {
					// Make sure a proper amount of entities reference the chunk
					size_t cnt = 0;
					for (const auto& e: m_entities) {
						if (e.pChunk != pChunk)
							continue;
						++cnt;
					}
					GAIA_ASSERT(cnt == pChunk->GetItemCount());
				} else {
					// Make sure no entites reference the chunk
					for (const auto& e: m_entities) {
						(void)e;
						GAIA_ASSERT(e.pChunk != pChunk);
					}
				}
#endif
			}

			EntityContainer& AddComponent_Internal(
					component::ComponentType componentType, Entity entity, const component::ComponentInfo& infoToAdd) {
				auto& entityContainer = m_entities[entity.id()];

				auto* pChunk = entityContainer.pChunk;

				// Adding a component to an entity which already is a part of some chunk
				if (pChunk != nullptr) {
					auto& archetype = *m_archetypes[pChunk->GetArchetypeId()];

					GAIA_ASSERT(
							!pChunk->IsStructuralChangesLocked() &&
							"New components can't be added while their chunk is being iterated "
							"(structural changes are forbidden during this time!)");
#if GAIA_DEBUG
					VerifyAddComponent(archetype, entity, componentType, infoToAdd);
#endif

					auto* pTargetArchetype = FindOrCreateArchetype_AddComponent(&archetype, componentType, infoToAdd);
					MoveEntity(entity, *pTargetArchetype);
				}
				// Adding a component to an empty entity
				else {
					auto& archetype = const_cast<archetype::Archetype&>(*m_pRootArchetype);

#if GAIA_DEBUG
					VerifyAddComponent(archetype, entity, componentType, infoToAdd);
#endif

					auto* pTargetArchetype = FindOrCreateArchetype_AddComponent(&archetype, componentType, infoToAdd);
					pChunk = pTargetArchetype->FindOrCreateFreeChunk();
					StoreEntity(entity, pChunk);
				}

				return entityContainer;
			}

			ComponentSetter RemoveComponent_Internal(
					component::ComponentType componentType, Entity entity, const component::ComponentInfo& infoToRemove) {
				auto& entityContainer = m_entities[entity.id()];
				auto* pChunk = entityContainer.pChunk;
				auto& archetype = *m_archetypes[pChunk->GetArchetypeId()];

				GAIA_ASSERT(
						!pChunk->IsStructuralChangesLocked() && "Components can't be removed while their chunk is being iterated "
																										"(structural changes are forbidden during this time!)");
#if GAIA_DEBUG
				VerifyRemoveComponent(archetype, entity, componentType, infoToRemove);
#endif

				auto* pNewArchetype = FindOrCreateArchetype_RemoveComponent(&archetype, componentType, infoToRemove);
				GAIA_ASSERT(pNewArchetype != nullptr);
				MoveEntity(entity, *pNewArchetype);

				return ComponentSetter{pChunk, entityContainer.idx};
			}

			void Init() {
				m_pRootArchetype = CreateArchetype({}, {});
				m_pRootArchetype->Init({0}, {0}, archetype::Archetype::CalculateLookupHash({0}, {0}));
				RegisterArchetype(m_pRootArchetype);
			}

			void Done() {
				Cleanup();

				ChunkAllocator::Get().Flush();

#if GAIA_DEBUG && GAIA_ECS_CHUNK_ALLOCATOR
				// Make sure there are no leaks
				ChunkAllocatorStats memstats = ChunkAllocator::Get().GetStats();
				if (memstats.AllocatedMemory != 0) {
					GAIA_ASSERT(false && "ECS leaking memory");
					GAIA_LOG_W("ECS leaking memory!");
					ChunkAllocator::Get().Diag();
				}
#endif
			}

			/*!
			Creates a new entity from archetype
			\return Entity
			*/
			GAIA_NODISCARD Entity CreateEntity(archetype::Archetype& archetype) {
				const auto entity = CreateEntity();

				auto* pChunk = m_entities[entity.id()].pChunk;
				if (pChunk == nullptr)
					pChunk = archetype.FindOrCreateFreeChunk();

				StoreEntity(entity, pChunk);

				return entity;
			}

		public:
			World() {
				Init();
			}

			~World() {
				Done();
			}

			World(World&&) = delete;
			World(const World&) = delete;
			World& operator=(World&&) = delete;
			World& operator=(const World&) = delete;

			GAIA_NODISCARD bool IsEntityValid(Entity entity) const {
				// Entity ID has to fit inside the entity array
				if (entity.id() >= m_entities.size())
					return false;

				const auto& entityContainer = m_entities[entity.id()];
				// Generation ID has to match the one in the array
				if (entityContainer.gen != entity.gen())
					return false;
				// If chunk information is present the entity at the pointed index has to match our entity
				if ((entityContainer.pChunk != nullptr) && entityContainer.pChunk->GetEntity(entityContainer.idx) != entity)
					return false;

				return true;
			}

			void Cleanup() {
				// Clear entities
				{
					m_entities = {};
					m_nextFreeEntity = 0;
					m_freeEntities = 0;
				}

				// Clear archetypes
				{
					m_chunksToRemove = {};

					// Delete all allocated chunks and their parent archetypes
					for (auto* pArchetype: m_archetypes)
						delete pArchetype;

					m_archetypes = {};
					m_archetypeMap = {};
				}
			}

			//----------------------------------------------------------------------

			/*!
			Returns the current version of the world.
			\return World version number
			*/
			GAIA_NODISCARD uint32_t& GetWorldVersion() {
				return m_worldVersion;
			}

			//----------------------------------------------------------------------

			/*!
			Creates a new empty entity
			\return Entity
			*/
			GAIA_NODISCARD Entity CreateEntity() {
				return AllocateEntity();
			}

			/*!
			Creates a new entity by cloning an already existing one.
			\param entity Entity
			\return Entity
			*/
			GAIA_NODISCARD Entity CreateEntity(Entity entity) {
				auto& entityContainer = m_entities[entity.id()];

				auto* pChunk = entityContainer.pChunk;

				// If the reference entity doesn't have any chunk assigned create a chunkless one
				if GAIA_UNLIKELY (pChunk == nullptr)
					return CreateEntity();

				auto& archetype = *m_archetypes[pChunk->GetArchetypeId()];

				const auto newEntity = CreateEntity(archetype);
				auto& newEntityContainer = m_entities[newEntity.id()];
				auto* pNewChunk = newEntityContainer.pChunk;

				// By adding a new entity m_entities array might have been reallocated.
				// We need to get the new address.
				auto& oldEntityContainer = m_entities[entity.id()];
				auto* pOldChunk = oldEntityContainer.pChunk;

				// Copy generic component data from reference entity to our new entity
				const auto& componentIds = archetype.GetComponentIdArray(component::ComponentType::CT_Generic);
				const auto& offsets = archetype.GetComponentOffsetArray(component::ComponentType::CT_Generic);

				const auto& cc = ComponentCache::Get();

				for (size_t i = 0; i < componentIds.size(); i++) {
					const auto& desc = cc.GetComponentDesc(componentIds[i]);
					if (desc.properties.size == 0U)
						continue;

					const auto offset = offsets[i];
					const auto idxSrc = offset + desc.properties.size * (uint32_t)oldEntityContainer.idx;
					const auto idxDst = offset + desc.properties.size * (uint32_t)newEntityContainer.idx;

					GAIA_ASSERT(idxSrc < Chunk::DATA_SIZE_NORESERVE);
					GAIA_ASSERT(idxDst < Chunk::DATA_SIZE_NORESERVE);

					auto* pSrc = (void*)&pOldChunk->GetData(idxSrc);
					auto* pDst = (void*)&pNewChunk->GetData(idxDst);

					if (desc.properties.copyable == 1)
						desc.copy(pSrc, pDst);
					else
						memmove(pDst, (const void*)pSrc, desc.properties.size);
				}

				return newEntity;
			}

			/*!
			Removes an entity along with all data associated with it.
			\param entity Entity
			*/
			void DeleteEntity(Entity entity) {
				if (m_entities.empty() || entity == EntityNull)
					return;

				GAIA_ASSERT(IsEntityValid(entity));

				auto& entityContainer = m_entities[entity.id()];

				// Remove entity from chunk
				if (auto* pChunk = entityContainer.pChunk) {
					RemoveEntity(pChunk, entityContainer.idx);

					// Return entity to pool
					DeallocateEntity(entity);

					ValidateChunk(pChunk);
					ValidateEntityList();
				} else {
					// Return entity to pool
					DeallocateEntity(entity);
				}
			}

			/*!
			Enables or disables an entire entity.
			\param entity Entity
			\param enable Enable or disable the entity
			*/
			void EnableEntity(Entity entity, bool enable) {
				auto& entityContainer = m_entities[entity.id()];

				GAIA_ASSERT(
						(!entityContainer.pChunk || !entityContainer.pChunk->IsStructuralChangesLocked()) &&
						"Entities can't be enabled/disabled while their chunk is being iterated "
						"(structural changes are forbidden during this time!)");

				if (enable != (bool)entityContainer.disabled)
					return;
				entityContainer.disabled = !enable;

				if (auto* pChunkFrom = entityContainer.pChunk) {
					auto& archetype = *m_archetypes[pChunkFrom->GetArchetypeId()];

					// Create a spot in the new chunk
					auto* pChunkTo = enable ? archetype.FindOrCreateFreeChunk() : archetype.FindOrCreateFreeChunkDisabled();
					const auto idxNew = pChunkTo->AddEntity(entity);

					// Copy generic component data from the reference entity to our new entity
					{
						const auto& componentIds = archetype.GetComponentIdArray(component::ComponentType::CT_Generic);
						const auto& offsets = archetype.GetComponentOffsetArray(component::ComponentType::CT_Generic);

						const auto& cc = ComponentCache::Get();

						for (size_t i = 0; i < componentIds.size(); i++) {
							const auto& desc = cc.GetComponentDesc(componentIds[i]);
							if (desc.properties.size == 0U)
								continue;

							const auto offset = offsets[i];
							const auto idxSrc = offset + desc.properties.size * (uint32_t)entityContainer.idx;
							const auto idxDst = offset + desc.properties.size * idxNew;

							GAIA_ASSERT(idxSrc < Chunk::DATA_SIZE_NORESERVE);
							GAIA_ASSERT(idxDst < Chunk::DATA_SIZE_NORESERVE);

							auto* pSrc = (void*)&pChunkFrom->GetData(idxSrc);
							auto* pDst = (void*)&pChunkTo->GetData(idxDst);

							if (desc.properties.copyable == 1)
								desc.copy(pSrc, pDst);
							else
								memmove(pDst, (const void*)pSrc, desc.properties.size);
						}
					}

					// Remove the entity from the old chunk
					pChunkFrom->RemoveEntity(
							entityContainer.idx, m_entities, archetype.GetComponentIdArray(component::ComponentType::CT_Generic),
							archetype.GetComponentOffsetArray(component::ComponentType::CT_Generic));

					// Update the entity container with new info
					entityContainer.pChunk = pChunkTo;
					entityContainer.idx = idxNew;
				}
			}

			/*!
			Returns the number of active entities
			\return Entity
			*/
			GAIA_NODISCARD uint32_t GetEntityCount() const {
				return (uint32_t)m_entities.size() - m_freeEntities;
			}

			/*!
			Returns an entity at a given position
			\return Entity
			*/
			GAIA_NODISCARD Entity GetEntity(uint32_t idx) const {
				GAIA_ASSERT(idx < m_entities.size());
				const auto& entityContainer = m_entities[idx];
				return {idx, entityContainer.gen};
			}

			/*!
			Returns a chunk containing the given entity.
			\return Chunk or nullptr if not found
			*/
			GAIA_NODISCARD Chunk* GetChunk(Entity entity) const {
				GAIA_ASSERT(entity.id() < m_entities.size());
				const auto& entityContainer = m_entities[entity.id()];
				return entityContainer.pChunk;
			}

			/*!
			Returns a chunk containing the given entity.
			Index of the entity is stored in \param indexInChunk
			\return Chunk or nullptr if not found
			*/
			GAIA_NODISCARD Chunk* GetChunk(Entity entity, uint32_t& indexInChunk) const {
				GAIA_ASSERT(entity.id() < m_entities.size());
				const auto& entityContainer = m_entities[entity.id()];
				indexInChunk = entityContainer.idx;
				return entityContainer.pChunk;
			}

			//----------------------------------------------------------------------

			/*!
			Attaches a new component \tparam T to \param entity.
			\warning It is expected the component is not present on \param entity yet. Undefined behavior otherwise.
			\warning It is expected \param entity is valid. Undefined behavior otherwise.
			\tparam T Component
			\param entity Entity
			\return ComponentSetter object.
			*/
			template <typename T>
			ComponentSetter AddComponent(Entity entity) {
				component::VerifyComponent<T>();
				GAIA_ASSERT(IsEntityValid(entity));

				using U = typename component::DeduceComponent<T>::Type;
				const auto& info = ComponentCache::Get().GetOrCreateComponentInfo<U>();

				if constexpr (component::IsGenericComponent<T>) {
					auto& entityContainer = AddComponent_Internal(component::ComponentType::CT_Generic, entity, info);
					return ComponentSetter{entityContainer.pChunk, entityContainer.idx};
				} else {
					auto& entityContainer = AddComponent_Internal(component::ComponentType::CT_Chunk, entity, info);
					return ComponentSetter{entityContainer.pChunk, entityContainer.idx};
				}
			}

			/*!
			Attaches a new component \tparam T to \param entity. Also sets its value.
			\warning It is expected the component is not present on \param entity yet. Undefined behavior otherwise.
			\warning It is expected \param entity is valid. Undefined behavior otherwise.
			\tparam T Component
			\param entity Entity
			\param value Value to set for the component
			\return ComponentSetter object.
			*/
			template <typename T>
			ComponentSetter AddComponent(Entity entity, typename component::DeduceComponent<T>::Type&& value) {
				component::VerifyComponent<T>();
				GAIA_ASSERT(IsEntityValid(entity));

				using U = typename component::DeduceComponent<T>::Type;
				const auto& info = ComponentCache::Get().GetOrCreateComponentInfo<U>();

				if constexpr (component::IsGenericComponent<T>) {
					auto& entityContainer = AddComponent_Internal(component::ComponentType::CT_Generic, entity, info);
					auto* pChunk = entityContainer.pChunk;
					pChunk->template SetComponent<T>(entityContainer.idx, std::forward<U>(value));
					return ComponentSetter{entityContainer.pChunk, entityContainer.idx};
				} else {
					auto& entityContainer = AddComponent_Internal(component::ComponentType::CT_Chunk, entity, info);
					auto* pChunk = entityContainer.pChunk;
					pChunk->template SetComponent<T>(std::forward<U>(value));
					return ComponentSetter{entityContainer.pChunk, entityContainer.idx};
				}
			}

			/*!
			Removes a component \tparam T from \param entity.
			\warning It is expected the component is present on \param entity. Undefined behavior otherwise.
			\warning It is expected \param entity is valid. Undefined behavior otherwise.
			\tparam T Component
			\param entity Entity
			\return ComponentSetter object.
			*/
			template <typename T>
			ComponentSetter RemoveComponent(Entity entity) {
				component::VerifyComponent<T>();
				GAIA_ASSERT(IsEntityValid(entity));

				using U = typename component::DeduceComponent<T>::Type;
				const auto& info = ComponentCache::Get().GetOrCreateComponentInfo<U>();

				if constexpr (component::IsGenericComponent<T>)
					return RemoveComponent_Internal(component::ComponentType::CT_Generic, entity, info);
				else
					return RemoveComponent_Internal(component::ComponentType::CT_Chunk, entity, info);
			}

			/*!
			Sets the value of the component \tparam T on \param entity.
			\warning It is expected the component is present on \param entity. Undefined behavior otherwise.
			\warning It is expected \param entity is valid. Undefined behavior otherwise.
			\tparam T Component
			\param entity Entity
			\param value Value to set for the component
			\return ComponentSetter object.
			*/
			template <typename T>
			ComponentSetter SetComponent(Entity entity, typename component::DeduceComponent<T>::Type&& value) {
				component::VerifyComponent<T>();
				GAIA_ASSERT(IsEntityValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				return ComponentSetter{entityContainer.pChunk, entityContainer.idx}.SetComponent<T>(
						std::forward<typename component::DeduceComponent<T>::Type>(value));
			}

			/*!
			Sets the value of the component \tparam T on \param entity.
			\warning It is expected the component is present on \param entity. Undefined behavior otherwise.
			\warning It is expected \param entity is valid. Undefined behavior otherwise.
			\tparam T Component
			\param entity Entity
			\param value Value to set for the component
			\return ComponentSetter object.
			*/
			template <typename T>
			ComponentSetter SetComponentSilent(Entity entity, typename component::DeduceComponent<T>::Type&& value) {
				component::VerifyComponent<T>();
				GAIA_ASSERT(IsEntityValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				return ComponentSetter{entityContainer.pChunk, entityContainer.idx}.SetComponentSilent<T>(
						std::forward<typename component::DeduceComponent<T>::Type>(value));
			}

			/*!
			Returns the value stored in the component \tparam T on \param entity.
			\warning It is expected the component is present on \param entity. Undefined behavior otherwise.
			\warning It is expected \param entity is valid. Undefined behavior otherwise.
			\tparam T Component
			\param entity Entity
			\return Value stored in the component.
			*/
			template <typename T>
			GAIA_NODISCARD auto GetComponent(Entity entity) const {
				component::VerifyComponent<T>();
				GAIA_ASSERT(IsEntityValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				const auto* pChunk = entityContainer.pChunk;

				if constexpr (component::IsGenericComponent<T>)
					return pChunk->GetComponent<T>(entityContainer.idx);
				else
					return pChunk->GetComponent<T>();
			}

			//----------------------------------------------------------------------

			/*!
			Tells if \param entity contains the component \tparam T.
			\warning It is expected \param entity is valid. Undefined behavior otherwise.
			\tparam T Component
			\param entity Entity
			\return True if the component is present on entity.
			*/
			template <typename T>
			GAIA_NODISCARD bool HasComponent(Entity entity) const {
				component::VerifyComponent<T>();
				GAIA_ASSERT(IsEntityValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				if (const auto* pChunk = entityContainer.pChunk)
					return pChunk->HasComponent<T>();

				return false;
			}

			//----------------------------------------------------------------------

		private:
			template <typename T>
			constexpr GAIA_NODISCARD GAIA_FORCEINLINE auto GetComponentView(Chunk& chunk) const {
				using U = typename component::DeduceComponent<T>::Type;
				using UOriginal = typename component::DeduceComponent<T>::TypeOriginal;
				if constexpr (component::IsReadOnlyType<UOriginal>::value)
					return chunk.View_Internal<U>();
				else
					return chunk.ViewRW_Internal<U, true>();
			}

			//--------------------------------------------------------------------------------

			template <typename... T>
			void UnpackArgsIntoQuery(EntityQuery& query, [[maybe_unused]] utils::func_type_list<T...> types) const {
				static_assert(sizeof...(T) > 0, "Inputs-less functors can not be unpacked to query");
				query.All<T...>();
			}

			template <typename... T>
			static void RegisterComponents_Internal(utils::func_type_list<T...> /*no_name*/) {
				static_assert(sizeof...(T) > 0, "Empty EntityQuery is not supported in this context");
				auto& cc = ComponentCache::Get();
				((void)cc.GetOrCreateComponentInfo<T>(), ...);
			}

			template <typename Func>
			static void RegisterComponents() {
				using InputArgs = decltype(utils::func_args(&Func::operator()));
				RegisterComponents_Internal(InputArgs{});
			}

			template <typename... T>
			static constexpr component::ComponentLookupHash
			CalculateQueryIdLookupHash([[maybe_unused]] utils::func_type_list<T...> types) {
				return component::CalculateLookupHash<T...>();
			}

		public:
			EntityQuery CreateQuery() {
				return EntityQuery(m_queryCache, m_worldVersion, m_archetypes);
			}

			/*!
			Iterates over all chunks satisfying conditions set by \param func and calls \param func for all of them.
			EntityQuery instance is generated internally from the input arguments of \param func.
			\warning Performance-wise it has less potential than iterating using ecs::Chunk or a comparable ForEach
			passing in a query because it needs to do cached query lookups on each invocation. However, it is easier to
			use and for non-critical code paths it is the most elegant way of iterating your data.
			*/
			template <typename Func>
			void ForEach(Func func) {
				static_assert(
						!std::is_invocable<Func, Chunk&>::value, "Calling query-less ForEach is not supported for chunk iteration");

				using InputArgs = decltype(utils::func_args(&Func::operator()));

				RegisterComponents<Func>();

				constexpr auto lookupHash = CalculateQueryIdLookupHash(InputArgs{});
				if (m_uniqueFuncQueryPairs.count(lookupHash) == 0) {
					EntityQuery query = CreateQuery();
					UnpackArgsIntoQuery(query, InputArgs{});
					(void)query.FetchQueryInfo();
					m_uniqueFuncQueryPairs.try_emplace(lookupHash, query.GetQueryId());
					CreateQuery().ForEach(query.GetQueryId(), func);
				} else {
					const auto queryId = m_uniqueFuncQueryPairs[lookupHash];
					CreateQuery().ForEach(queryId, func);
				}
			}

			/*!
			Garbage collection. Checks all chunks and archetypes which are empty and have not been
			used for a while and tries to delete them and release memory allocated by them.
			*/
			void GC() {
				// Handle chunks
				for (size_t i = 0; i < m_chunksToRemove.size();) {
					auto* pChunk = m_chunksToRemove[i];

					// Skip reclaimed chunks
					if (pChunk->HasEntities()) {
						pChunk->PrepareToDie();
						utils::erase_fast(m_chunksToRemove, i);
						continue;
					}

					if (pChunk->ProgressDeath()) {
						++i;
						continue;
					}
				}

				// Remove all dead chunks
				for (auto* pChunk: m_chunksToRemove) {
					auto& archetype = *m_archetypes[pChunk->GetArchetypeId()];
					archetype.RemoveChunk(pChunk);
				}
				m_chunksToRemove.clear();
			}

			//--------------------------------------------------------------------------------

			/*!
			Performs diagnostics on archetypes. Prints basic info about them and the chunks they contain.
			*/
			void DiagArchetypes() const {
				GAIA_LOG_N("Archetypes:%u", (uint32_t)m_archetypes.size());
				for (const auto* archetype: m_archetypes)
					archetype::Archetype::DiagArchetype(*archetype);
			}

			/*!
			Performs diagnostics on registered components.
			Prints basic info about them and reports and detected issues.
			*/
			static void DiagRegisteredTypes() {
				ComponentCache::Get().Diag();
			}

			/*!
			Performs diagnostics on entites of the world.
			Also performs validation of internal structures which hold the entities.
			*/
			void DiagEntities() const {
				ValidateEntityList();

				GAIA_LOG_N("Deleted entities: %u", m_freeEntities);
				if (m_freeEntities != 0U) {
					GAIA_LOG_N("  --> %u", m_nextFreeEntity);

					uint32_t iters = 0;
					auto fe = m_entities[m_nextFreeEntity].idx;
					while (fe != Entity::IdMask) {
						GAIA_LOG_N("  --> %u", m_entities[fe].idx);
						fe = m_entities[fe].idx;
						++iters;
						if ((iters == 0U) || iters > m_freeEntities) {
							GAIA_LOG_E("  Entities recycle list contains inconsistent "
												 "data!");
							break;
						}
					}
				}
			}

			/*!
			Performs all diagnostics.
			*/
			void Diag() const {
				DiagArchetypes();
				DiagRegisteredTypes();
				DiagEntities();
			}
		}; // namespace gaia

	} // namespace ecs
} // namespace gaia
