#pragma once
#include <cassert>
#include <inttypes.h>

#include "../config/config.h"
#include "../containers/map.h"
#include "../containers/sarray.h"
#include "../containers/sarray_ext.h"
#include "../utils/span.h"
#include "../utils/type_info.h"
#include "../utils/utility.h"
#include "../utils/utils_containers.h"
#include "chunk.h"
#include "chunk_allocator.h"
#include "creation_query.h"
#include "entity.h"
#include "entity_query.h"
#include "fwd.h"
#include "gaia/ecs/component.h"

namespace gaia {
	namespace ecs {

		class GAIA_API World final {
			friend class ECSSystem;
			friend class ECSSystemManager;
			friend class CommandBuffer;
			friend void* AllocateChunkMemory(World& world);
			friend void ReleaseChunkMemory(World& world, void* mem);

			//! Allocator used to allocate chunks
			ChunkAllocator m_chunkAllocator;

			//! Map or archetypes mapping to the same hash - used for lookups
			// TODO: Replace darray with linked-list via pointers for minimum overhead or come up with something else
			containers::map<uint64_t, containers::darray<Archetype*>> m_archetypeMap;
			//! List of archetypes - used for iteration
			containers::darray<Archetype*> m_archetypeList;
			//! Root archetype
			Archetype* m_rootArchetype;

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

			void* AllocateChunkMemory() {
				return m_chunkAllocator.Allocate();
			}

			void ReleaseChunkMemory(void* mem) {
				m_chunkAllocator.Release(mem);
			}

		public:
			void UpdateWorldVersion() {
				UpdateVersion(m_worldVersion);
			}

			[[nodiscard]] bool IsEntityValid(Entity entity) const {
				// Entity ID has to fit inside entity array
				if (entity.id() >= m_entities.size())
					return false;

				auto& entityContainer = m_entities[entity.id()];
				// Generation ID has to match the one in the array
				if (entityContainer.gen != entity.gen())
					return false;
				// If chunk information is present the entity at the pointed index has
				// to match our entity
				if (entityContainer.pChunk && entityContainer.pChunk->GetEntity(entityContainer.idx) != entity)
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
					for (auto archetype: m_archetypeList)
						delete archetype;

					m_archetypeList = {};
					m_archetypeMap = {};
				}
			}

		private:
			/*!
			Remove an entity from chunk.
			\param pChunk Chunk we remove the entity from
			\param entityChunkIndex Index of entity within its chunk
			*/
			void RemoveEntity(Chunk* pChunk, uint32_t entityChunkIndex) {
				GAIA_ASSERT(
						!pChunk->header.owner.info.structuralChangesLocked &&
						"Entities can't be removed while chunk is being iterated "
						"(structural changes are forbidden during this time!)");

				pChunk->RemoveEntity(entityChunkIndex, m_entities);

				if (
						// Skip chunks which already requested removal
						pChunk->header.info.lifespan > 0 ||
						// Skip non-empty chunks
						pChunk->HasEntities())
					return;

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
				pChunk->header.info.lifespan = MAX_CHUNK_LIFESPAN;

				m_chunksToRemove.push_back(pChunk);
			}

			/*!
			Searches for archetype with a given set of components
			\param genericTypes Span of generic component types
			\param chunkTypes Span of chunk component types
			\param lookupHash Archetype lookup hash
			\return Pointer to archetype or nullptr
			*/
			[[nodiscard]] Archetype* FindArchetype(
					std::span<const ComponentMetaData*> genericTypes, std::span<const ComponentMetaData*> chunkTypes,
					const uint64_t lookupHash) {
				// Search for the archetype in the map
				const auto it = m_archetypeMap.find(lookupHash);
				if (it == m_archetypeMap.end())
					return nullptr;

				const auto& archetypeArray = it->second;

				auto checkTypes = [&](const ChunkComponentTypeList& list, const std::span<const ComponentMetaData*>& types) {
					for (uint32_t j = 0; j < types.size(); j++) {
						// Different components. We need to search further
						if (list[j].type != types[j])
							return false;
					}
					return true;
				};

				// Iterate over the list of archetypes and find the exact match
				for (const auto archetype: archetypeArray) {
					const auto& genericComponentList = archetype->componentTypeList[ComponentType::CT_Generic];
					if (genericComponentList.size() != genericTypes.size())
						continue;
					const auto& chunkComponentList = archetype->componentTypeList[ComponentType::CT_Chunk];
					if (chunkComponentList.size() != chunkTypes.size())
						continue;

					if (checkTypes(genericComponentList, genericTypes) && checkTypes(chunkComponentList, chunkTypes))
						return archetype;
				}

				return nullptr;
			}

			/*!
			Creates a new archetype from a given set of components
			\param genericTypes Span of generic component types
			\param chunkTypes Span of chunk component types
			\param lookupHash Archetype lookup hash
			\return Pointer to archetype
			*/
			[[nodiscard]] Archetype* CreateArchetype(
					std::span<const ComponentMetaData*> genericTypes, std::span<const ComponentMetaData*> chunkTypes) {
				auto newArch = Archetype::Create(*this, genericTypes, chunkTypes);

				newArch->id = (uint32_t)m_archetypeList.size();
				GAIA_ASSERT(!utils::has(m_archetypeList, newArch));
				m_archetypeList.push_back(newArch);

				newArch->genericHash = CalculateLookupHash(genericTypes);
				newArch->chunkHash = CalculateLookupHash(chunkTypes);

				// Calculate hash for our combination of components
				const auto lookupHash =
						CalculateLookupHash(containers::sarray<uint64_t, 2>{newArch->genericHash, newArch->chunkHash});

				newArch->lookupHash = lookupHash;
				auto it = m_archetypeMap.find(lookupHash);
				if (it == m_archetypeMap.end()) {
					m_archetypeMap[lookupHash] = {newArch};
				} else {
					auto& archetypes = it->second;
					GAIA_ASSERT(!utils::has(archetypes, newArch));
					archetypes.push_back(newArch);
				}

				return newArch;
			}

			/*!
			Searches for an archetype given based on a given set of components. If no archetype is found a new one is
			created. \param genericTypes Span of generic component types \param chunkTypes Span of chunk component types
			\return Pointer to archetype
			*/
			[[nodiscard]] Archetype* FindOrCreateArchetype(
					std::span<const ComponentMetaData*> genericTypes, std::span<const ComponentMetaData*> chunkTypes) {
				// Make sure to sort the meta-types so we receive the same hash no
				// matter the order in which components are provided Bubble sort is
				// okay. We're dealing with at most MAX_COMPONENTS_PER_ARCHETYPE items.
				// TODO: Replace with a sorting network
				std::sort(genericTypes.begin(), genericTypes.end(), std::less<const ComponentMetaData*>());
				std::sort(chunkTypes.begin(), chunkTypes.end(), std::less<const ComponentMetaData*>());

				// Calculate hash for our combination of components
				const auto genericHash = CalculateLookupHash(genericTypes);
				const auto chunkHash = CalculateLookupHash(chunkTypes);
				const auto lookupHash = CalculateLookupHash(containers::sarray<uint64_t, 2>{genericHash, chunkHash});

				if (auto archetype = FindArchetype(genericTypes, chunkTypes, lookupHash))
					return archetype;

				// Archetype wasn't found so we have to create a new one
				return CreateArchetype(genericTypes, chunkTypes);
			}

			/*!
			Searches for an archetype given based on a given set of components. If no archetype is found a new one is
			created. \param query Query to use to search for the archetype \return Pointer to archetype
			*/
			[[nodiscard]] Archetype* FindOrCreateArchetype(CreationQuery& query) {
				return FindOrCreateArchetype(
						std::span<const ComponentMetaData*>(
								query.list[ComponentType::CT_Generic].data(), query.list[ComponentType::CT_Generic].size()),
						std::span<const ComponentMetaData*>(
								query.list[ComponentType::CT_Chunk].data(), query.list[ComponentType::CT_Chunk].size()));
			}

#if GAIA_DEBUG
			void VerifyAddComponent(
					Archetype& archetype, Entity entity, ComponentType componentType,
					std::span<const ComponentMetaData*> typesToAdd) {
				const auto& info = archetype.componentTypeList[componentType];
				const uint32_t oldTypesCount = (uint32_t)info.size();
				const uint32_t newTypesCount = (uint32_t)typesToAdd.size();
				const uint32_t metaTypesCount = oldTypesCount + newTypesCount;

				// Make sure not to add too many types
				if (!VerityArchetypeComponentCount(metaTypesCount)) {
					GAIA_ASSERT(false && "Trying to add too many components to entity!");
					LOG_W(
							"Trying to add %u component(s) to entity [%u.%u] but there's only enough room for %u more!",
							newTypesCount, entity.id(), entity.gen(), MAX_COMPONENTS_PER_ARCHETYPE - oldTypesCount);
					LOG_W("Already present:");
					for (uint32_t i = 0U; i < oldTypesCount; i++)
						LOG_W("> [%u] %.*s", i, (uint32_t)info[i].type->name.length(), info[i].type->name.data());
					LOG_W("Trying to add:");
					for (uint32_t i = 0U; i < newTypesCount; i++)
						LOG_W("> [%u] %.*s", i, (uint32_t)typesToAdd[i]->name.length(), typesToAdd[i]->name.data());
				}

				// Don't add the same component twice
				for (const auto& inf: info) {
					for (uint32_t k = 0; k < newTypesCount; k++) {
						if (inf.type == typesToAdd[k]) {
							GAIA_ASSERT(false && "Trying to add a duplicate component");

							LOG_W(
									"Trying to add a duplicate of component %s to entity [%u.%u]", ComponentTypeString[componentType],
									entity.id(), entity.gen());
							LOG_W("> %.*s", (uint32_t)inf.type->name.length(), inf.type->name.data());
						}
					}
				}
			}

			void VerifyRemoveComponent(
					Archetype& archetype, Entity entity, ComponentType componentType,
					std::span<const ComponentMetaData*> typesToRemove) {
				auto* node = &archetype;

				// Make sure not to add too many types
				const uint32_t newTypesCount = (uint32_t)typesToRemove.size();
				for (uint32_t i = 0; i < newTypesCount; i++) {
					const auto* type = typesToRemove[i];

					const auto it = utils::find_if(node->edgesDel, [type, componentType](const auto& edge) {
						return edge.type == type && edge.componentType == componentType;
					});

					if (it == node->edgesDel.end()) {
						GAIA_ASSERT(false && "Trying to remove a component which wasn't added");
						LOG_W("Trying to remove a component from entity [%u.%u] but it was never added", entity.id(), entity.gen());
						LOG_W("Currently present:");

						const auto& info = node->componentTypeList[componentType];
						const uint32_t oldTypesCount = (uint32_t)info.size();
						for (uint32_t i = 0U; i < oldTypesCount; i++)
							LOG_W("> [%u] %.*s", i, (uint32_t)info[i].type->name.length(), info[i].type->name.data());
						LOG_W("Trying to remove:");
						LOG_W("> %s", typesToRemove[i]->name.data());
					}

					node = it->archetype;
				}
			}
#endif

			/*!
			Searches for an archetype given based on a given set of components. If no archetype is found a new one is created.
			\param genericTypes Span of generic component types
			\param chunkTypes Span of chunk component types
			\return Pointer to archetype
			*/
			[[nodiscard]] Archetype* FindOrCreateArchetype_AddComponents(
					Archetype* oldArchetype, ComponentType componentType, std::span<const ComponentMetaData*> newTypes) {

				auto* node = oldArchetype;
				for (uint32_t i = 0; i < (uint32_t)newTypes.size(); i++) {
					const auto* type = newTypes[i];

					const auto it = utils::find_if(node->edgesAdd, [type, componentType](const auto& edge) {
						return edge.type == type && edge.componentType == componentType;
					});

					// Not found among edges, create a new archetype
					if (it == node->edgesAdd.end()) {
						const auto& archetype = *node;

						const auto& componentTypeList = archetype.componentTypeList[(uint32_t)componentType];
						const auto& componentTypeList2 = archetype.componentTypeList[((uint32_t)componentType + 1) & 1];

						const auto oldTypesCount = (uint32_t)componentTypeList.size();
						const auto newTypesCount = 1;
						const auto metaTypesCount = oldTypesCount + newTypesCount;

						// Prepare a joint array of meta types of old and new types for our componentType
						containers::sarray_ext<const ComponentMetaData*, MAX_COMPONENTS_PER_ARCHETYPE> newMetaTypes;
						newMetaTypes.resize(metaTypesCount);
						{
							for (uint32_t j = 0U; j < oldTypesCount; j++)
								newMetaTypes[j] = componentTypeList[j].type;
							newMetaTypes[oldTypesCount] = newTypes[i];
						}

						// Prepare an array of old meta types for out other componentType. This is a simple copy.
						containers::sarray_ext<const ComponentMetaData*, MAX_COMPONENTS_PER_ARCHETYPE> otherMetaTypes;
						otherMetaTypes.resize(componentTypeList2.size());
						{
							for (uint32_t j = 0U; j < componentTypeList2.size(); j++)
								otherMetaTypes[j] = componentTypeList2[j].type;
						}

						auto newArchetype =
								componentType == ComponentType::CT_Generic
										? FindOrCreateArchetype(
													std::span<const ComponentMetaData*>(newMetaTypes.data(), (uint32_t)newMetaTypes.size()),
													std::span<const ComponentMetaData*>(otherMetaTypes.data(), (uint32_t)otherMetaTypes.size()))
										: FindOrCreateArchetype(
													std::span<const ComponentMetaData*>(otherMetaTypes.data(), (uint32_t)otherMetaTypes.size()),
													std::span<const ComponentMetaData*>(newMetaTypes.data(), (uint32_t)newMetaTypes.size()));
						{
							node->edgesAdd.push_back({type, newArchetype, componentType});
							newArchetype->edgesDel.push_back({type, node, componentType});
						}
						node = newArchetype;
					} else {
						node = it->archetype;
					}
				}

				return node;
			}

			/*!
			Searches for an archetype given based on a given set of components. If no archetype is found a new one is created.
			\param genericTypes Span of generic component types
			\param chunkTypes Span of chunk component types
			\return Pointer to archetype
			*/
			[[nodiscard]] Archetype* FindArchetype_RemoveComponents(
					Archetype* archetype, ComponentType componentType, std::span<const ComponentMetaData*> newTypes) {

				auto* node = archetype;
				for (uint32_t i = 0; i < (uint32_t)newTypes.size(); i++) {
					const auto* type = newTypes[i];

					const auto it = utils::find_if(node->edgesDel, [type, componentType](const auto& edge) {
						return edge.type == type && edge.componentType == componentType;
					});

					// We expect the component was found in the list. Verified in VerifyRemoveComponent.
					node = it->archetype;
				}

				return node;
			}

			/*!
			Returns an array of archetypes registered in the world
			\return Array or archetypes
			*/
			const auto& GetArchetypes() const {
				return m_archetypeList;
			}

			/*!
			Returns the archetype the entity belongs to.
			\param entity Entity
			\return Pointer to archetype
			*/
			[[nodiscard]] Archetype* GetArchetype(Entity entity) const {
				GAIA_ASSERT(IsEntityValid(entity));

				auto& entityContainer = m_entities[entity.id()];
				auto* pChunk = entityContainer.pChunk;
				return pChunk ? (Archetype*)&pChunk->header.owner : nullptr;
			}

			/*!
			Allocates a new entity.
			\return Entity
			*/
			[[nodiscard]] Entity AllocateEntity() {
				if (!m_freeEntities) {
					// We don't want to go out of range for new entities
					GAIA_ASSERT(m_entities.size() < Entity::IdMask && "Trying to allocate too many entities!");

					m_entities.push_back({});
					return {(EntityId)m_entities.size() - 1, 0};
				} else {
					// Make sure the list is not broken
					GAIA_ASSERT(m_nextFreeEntity < m_entities.size() && "ECS recycle list broken!");

					--m_freeEntities;
					const auto index = m_nextFreeEntity;
					m_nextFreeEntity = m_entities[m_nextFreeEntity].idx;
					return {index, m_entities[index].gen};
				}
			}

			/*!
			Deallocates a new entity.
			\param entityToDelete Entity to delete
			*/
			void DeallocateEntity(Entity entityToDelete) {
				auto& entityContainer = m_entities[entityToDelete.id()];
				entityContainer.pChunk = nullptr;

				// New generation
				auto gen = ++entityContainer.gen;

				// Update our implicit list
				if (!m_freeEntities) {
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
						!pChunk->header.owner.info.structuralChangesLocked &&
						"Entities can't be added while chunk is being iterated "
						"(structural changes are forbidden during this time!)");

				auto& entityContainer = m_entities[entity.id()];
				entityContainer.pChunk = pChunk;
				entityContainer.idx = pChunk->AddEntity(entity);
				entityContainer.gen = entityContainer.gen;
			}

			EntityContainer* AddComponent_Internal(
					ComponentType componentType, Entity entity, std::span<const ComponentMetaData*> typesToAdd) {
				auto& entityContainer = m_entities[entity.id()];

				// Adding a component to an entity which already is a part of some chunk
				if (auto* pChunk = entityContainer.pChunk) {
					auto& archetype = const_cast<Archetype&>(pChunk->header.owner);

					GAIA_ASSERT(
							!archetype.info.structuralChangesLocked && "New components can't be added while chunk is being iterated "
																												 "(structural changes are forbidden during this time!)");
#if GAIA_DEBUG
					VerifyAddComponent(archetype, entity, componentType, typesToAdd);
#endif

					auto newArchetype = FindOrCreateArchetype_AddComponents(&archetype, componentType, typesToAdd);
					MoveEntity(entity, *newArchetype);
				}
				// Adding a component to an empty entity
				else {
					auto& archetype = const_cast<Archetype&>(*m_rootArchetype);

					GAIA_ASSERT(
							!archetype.info.structuralChangesLocked && "New components can't be added while chunk is being iterated "
																												 "(structural changes are forbidden during this time!)");
#if GAIA_DEBUG
					VerifyAddComponent(archetype, entity, componentType, typesToAdd);
#endif

					auto newArchetype = FindOrCreateArchetype_AddComponents(&archetype, componentType, typesToAdd);
					StoreEntity(entity, newArchetype->FindOrCreateFreeChunk());
				}

				return &entityContainer;
			}

			template <typename... TComponent>
			EntityContainer* AddComponent_Internal(ComponentType componentType, Entity entity) {
				constexpr auto typesCount = sizeof...(TComponent);
				const ComponentMetaData* types[] = {g_ComponentCache.GetOrCreateComponentMetaType<TComponent>()...};

				return AddComponent_Internal(componentType, entity, {types, typesCount});
			}

			void RemoveComponent_Internal(
					ComponentType componentType, Entity entity, std::span<const ComponentMetaData*> typesToRemove) {
				auto& entityContainer = m_entities[entity.id()];
				auto* pChunk = entityContainer.pChunk;
				auto& archetype = const_cast<Archetype&>(pChunk->header.owner);

				GAIA_ASSERT(
						!archetype.info.structuralChangesLocked && "Components can't be removed while chunk is being iterated "
																											 "(structural changes are forbidden during this time!)");
#if GAIA_DEBUG
				VerifyAddComponent(archetype, entity, componentType, typesToRemove);
#endif

				auto newArchetype = FindArchetype_RemoveComponents(&archetype, componentType, typesToRemove);
				MoveEntity(entity, *newArchetype);
			}

			template <typename... TComponent>
			void RemoveComponent_Internal(ComponentType componentType, Entity entity) {
				constexpr auto typesCount = sizeof...(TComponent);
				const ComponentMetaData* types[] = {g_ComponentCache.GetOrCreateComponentMetaType<TComponent>()...};

				RemoveComponent_Internal(componentType, entity, {types, typesCount});
			}

			void MoveEntity(Entity oldEntity, Archetype& newArchetype) {
				auto& entityContainer = m_entities[oldEntity.id()];
				auto oldChunk = entityContainer.pChunk;
				const auto oldIndex = entityContainer.idx;
				const auto& oldArchetype = oldChunk->header.owner;

				// Find a new chunk for the entity and move it inside.
				// Old entity ID needs to remain valid or lookups would break.
				auto newChunk = newArchetype.FindOrCreateFreeChunk();
				const auto newIndex = newChunk->AddEntity(oldEntity);

				// Find intersection of the two component lists.
				// We ignore chunk components here because they should't be influenced
				// by entities moving around.
				const auto& oldTypes = oldArchetype.componentTypeList[ComponentType::CT_Generic];
				const auto& newTypes = newArchetype.componentTypeList[ComponentType::CT_Generic];
				const auto& oldLook = oldArchetype.componentLookupList[ComponentType::CT_Generic];
				const auto& newLook = newArchetype.componentLookupList[ComponentType::CT_Generic];

				struct Intersection {
					uint32_t size;
					uint32_t oldIndex;
					uint32_t newIndex;
				};

				containers::sarray_ext<Intersection, MAX_COMPONENTS_PER_ARCHETYPE> intersections;

				// TODO: Arrays are sorted so we can do this in O(n+_) instead of
				// O(N^2)
				for (uint32_t i = 0U; i < oldTypes.size(); i++) {
					const auto typeOld = oldTypes[i].type;
					if (!typeOld->info.size)
						continue;

					for (uint32_t j = 0U; j < newTypes.size(); j++) {
						const auto typeNew = newTypes[j].type;
						if (typeNew != typeOld)
							continue;

						intersections.push_back({typeOld->info.size, i, j});
						break;
					}
				}

				// Let's move all data from oldEntity to newEntity
				for (uint32_t i = 0U; i < intersections.size(); i++) {
					const auto newIdx = intersections[i].newIndex;
					const auto oldIdx = intersections[i].oldIndex;

					const uint32_t idxFrom = newLook[newIdx].offset + intersections[i].size * newIndex;
					const uint32_t idxTo = oldLook[oldIdx].offset + intersections[i].size * oldIndex;

					GAIA_ASSERT(idxFrom < Chunk::DATA_SIZE_NORESERVE);
					GAIA_ASSERT(idxTo < Chunk::DATA_SIZE_NORESERVE);

					memcpy(&newChunk->data[idxFrom], &oldChunk->data[idxTo], intersections[i].size);
				}

				// Remove entity from the previous chunk
				RemoveEntity(oldChunk, oldIndex);

				// Update entity's chunk and index so look-ups can find it
				entityContainer.pChunk = newChunk;
				entityContainer.idx = newIndex;
				entityContainer.gen = oldEntity.gen();

				ValidateChunk(oldChunk);
				ValidateChunk(newChunk);
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
				// Make sure no entites reference the deleted chunk
				for (const auto& e: m_entities) {
					(void)e;
					GAIA_ASSERT(pChunk->HasEntities() || e.pChunk != pChunk);
				}
#endif
			}

			void Init() {
				m_rootArchetype = CreateArchetype({}, {});
			}

			void Done() {
				Cleanup();
				m_chunkAllocator.Flush();

#if GAIA_DEBUG
				// Make sure there are no leaks
				ChunkAllocatorStats memstats;
				m_chunkAllocator.GetStats(memstats);
				if (memstats.AllocatedMemory != 0) {
					GAIA_ASSERT(false && "ECS leaking memory");
					LOG_W("ECS leaking memory!");
					DiagMemory();
				}
#endif
			}

			/*!
			Creates a new entity from archetype
			\return Entity
			*/
			Entity CreateEntity(Archetype& archetype) {
				Entity entity = AllocateEntity();

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

			/*!
			Returns the current version of the world.
			\return World version number
			*/
			[[nodiscard]] uint32_t GetWorldVersion() const {
				return m_worldVersion;
			}

			/*!
			Creates a new empty entity
			\return Entity
			*/
			[[nodiscard]] Entity CreateEntity() {
				return AllocateEntity();
			}

			/*!
			Creates a new entity from query
			\return Entity
			*/
			Entity CreateEntity(CreationQuery& query) {
				auto archetype = FindOrCreateArchetype(query);
				return CreateEntity(*archetype);
			}

			/*!
			Creates a new entity by cloning an already existing one.
			\param entity Entity
			\return Entity
			*/
			Entity CreateEntity(Entity entity) {
				auto& entityContainer = m_entities[entity.id()];
				if (auto* pChunk = entityContainer.pChunk) {
					auto& archetype = const_cast<Archetype&>(pChunk->header.owner);

					Entity newEntity = CreateEntity(archetype);
					auto& newEntityContainer = m_entities[newEntity.id()];
					auto newChunk = newEntityContainer.pChunk;

					// By adding a new entity m_entities array might have been reallocated.
					// We need to get the new address.
					auto& oldEntityContainer = m_entities[entity.id()];
					auto oldChunk = oldEntityContainer.pChunk;

					// Copy generic component data from reference entity to our new ntity
					const auto& info = archetype.componentTypeList[ComponentType::CT_Generic];
					const auto& look = archetype.componentLookupList[ComponentType::CT_Generic];

					for (uint32_t i = 0U; i < info.size(); i++) {
						const auto* metaType = info[i].type;
						if (!metaType->info.size)
							continue;

						const uint32_t idxFrom = look[i].offset + metaType->info.size * oldEntityContainer.idx;
						const uint32_t idxTo = look[i].offset + metaType->info.size * newEntityContainer.idx;

						GAIA_ASSERT(idxFrom < Chunk::DATA_SIZE_NORESERVE);
						GAIA_ASSERT(idxTo < Chunk::DATA_SIZE_NORESERVE);

						memcpy(&newChunk->data[idxTo], &oldChunk->data[idxFrom], metaType->info.size);
					}

					return newEntity;
				} else
					return CreateEntity();
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
						(!entityContainer.pChunk || !entityContainer.pChunk->header.owner.info.structuralChangesLocked) &&
						"Entities can't be enabled/disabled while chunk is being iterated "
						"(structural changes are forbidden during this time!)");

				if (enable != (bool)entityContainer.disabled)
					return;
				entityContainer.disabled = !enable;

				if (auto* pChunkFrom = entityContainer.pChunk) {
					auto& archetype = const_cast<Archetype&>(pChunkFrom->header.owner);

					// Create a spot in the new chunk
					auto* pChunkTo = enable ? archetype.FindOrCreateFreeChunk() : archetype.FindOrCreateFreeChunkDisabled();
					const auto idxNew = pChunkTo->AddEntity(entity);

					// Copy generic component data from the reference entity to our new entity
					{
						const auto& info = archetype.componentTypeList[ComponentType::CT_Generic];
						const auto& look = archetype.componentLookupList[ComponentType::CT_Generic];

						for (uint32_t i = 0U; i < info.size(); i++) {
							const auto* metaType = info[i].type;
							if (!metaType->info.size)
								continue;

							const uint32_t idxFrom = look[i].offset + metaType->info.size * entityContainer.idx;
							const uint32_t idxTo = look[i].offset + metaType->info.size * idxNew;

							GAIA_ASSERT(idxFrom < Chunk::DATA_SIZE_NORESERVE);
							GAIA_ASSERT(idxTo < Chunk::DATA_SIZE_NORESERVE);

							memcpy(&pChunkTo->data[idxTo], &pChunkFrom->data[idxFrom], metaType->info.size);
						}
					}

					// Remove the entity from the old chunk
					pChunkFrom->RemoveEntity(entityContainer.idx, m_entities);

					// Update the entity container with new info
					entityContainer.pChunk = pChunkTo;
					entityContainer.idx = idxNew;
				}
			}

			/*!
			Returns the number of active entities
			\return Entity
			*/
			[[nodiscard]] uint32_t GetEntityCount() const {
				return (uint32_t)m_entities.size() - m_freeEntities;
			}

			/*!
			Returns an entity at a given position
			\return Entity
			*/
			[[nodiscard]] Entity GetEntity(uint32_t idx) const {
				GAIA_ASSERT(idx < m_entities.size());
				auto& entityContainer = m_entities[idx];
				return {idx, entityContainer.gen};
			}

			/*!
			Returns a chunk containing the given entity.
			\return Chunk or nullptr if not found
			*/
			[[nodiscard]] Chunk* GetEntityChunk(Entity entity) const {
				GAIA_ASSERT(entity.id() < m_entities.size());
				auto& entityContainer = m_entities[entity.id()];
				return entityContainer.pChunk;
			}

			/*!
			Returns a chunk containing the given entity.
			Index of the entity is stored in \param indexInChunk
			\return Chunk or nullptr if not found
			*/
			[[nodiscard]] Chunk* GetEntityChunk(Entity entity, uint32_t& indexInChunk) const {
				GAIA_ASSERT(entity.id() < m_entities.size());
				auto& entityContainer = m_entities[entity.id()];
				indexInChunk = entityContainer.idx;
				return entityContainer.pChunk;
			}

			/*!
			Attaches a new component to \param entity.
			\warning It is expected the component is not there yet and that \param
			entity is valid. Undefined behavior otherwise.
			*/
			template <typename... TComponent>
			void AddComponent(Entity entity) {
				VerifyComponents<TComponent...>();
				GAIA_ASSERT(IsEntityValid(entity));

				AddComponent_Internal<TComponent...>(ComponentType::CT_Generic, entity);
			}

			/*!
			Attaches new components to \param entity.
			\warning It is expected the component is not there yet and that
			\param entity is valid. Undefined behavior otherwise.
			*/
			template <typename... TComponent>
			void AddComponent(Entity entity, TComponent&&... data) {
				VerifyComponents<TComponent...>();
				GAIA_ASSERT(IsEntityValid(entity));

				auto entityContainer = AddComponent_Internal<TComponent...>(ComponentType::CT_Generic, entity);
				auto* pChunk = entityContainer->pChunk;
				pChunk->template SetComponents<TComponent...>(entityContainer->idx, std::forward<TComponent>(data)...);
			}

			/*!
			Attaches new chunk components to \param entity.
			Chunk component is shared for the entire chunk.
			\warning It is expected the component is not there yet and that
			\param entity is valid. Undefined behavior otherwise.
			*/
			template <typename... TComponent>
			void AddChunkComponent(Entity entity) {
				VerifyComponents<TComponent...>();
				GAIA_ASSERT(IsEntityValid(entity));

				AddComponent_Internal<TComponent...>(ComponentType::CT_Chunk, entity);
			}

			/*!
			Attaches new chunk components to \param entity.
			Chunk component is shared for the entire chunk.
			\warning It is expected the component is not there yet and that
			\param entity is valid. Undefined behavior otherwise.
			*/
			template <typename... TComponent>
			void AddChunkComponent(Entity entity, TComponent&&... data) {
				VerifyComponents<TComponent...>();
				GAIA_ASSERT(IsEntityValid(entity));

				auto entityContainer = AddComponent_Internal<TComponent...>(ComponentType::CT_Chunk, entity);
				auto* pChunk = entityContainer->pChunk;
				pChunk->template SetChunkComponents<TComponent...>(std::forward<TComponent>(data)...);
			}

			/*!
			Removes a component from \param entity.
			\warning It is expected the component is not there yet and that
			\param entity is valid. Undefined behavior otherwise.
			*/
			template <typename... TComponent>
			void RemoveComponent(Entity entity) {
				VerifyComponents<TComponent...>();
				GAIA_ASSERT(IsEntityValid(entity));

				RemoveComponent_Internal<TComponent...>(ComponentType::CT_Generic, entity);
			}

			/*!
			Removes a chunk component from \param entity.
			\warning It is expected the component is not there yet and that
			\param entity is valid. Undefined behavior otherwise.
			*/
			template <typename... TComponent>
			void RemoveChunkComponent(Entity entity) {
				VerifyComponents<TComponent...>();
				GAIA_ASSERT(IsEntityValid(entity));

				RemoveComponent_Internal<TComponent...>(ComponentType::CT_Chunk, entity);
			}

			/*!
			Sets value for of a component of \param entity.
			\warning It is expected the component is not there yet and that
			\param entity is valid. Undefined behavior otherwise.
			*/
			template <typename... TComponent>
			void SetComponent(Entity entity, TComponent&&... data) {
				VerifyComponents<TComponent...>();
				GAIA_ASSERT(IsEntityValid(entity));

				auto& entityContainer = m_entities[entity.id()];
				auto* pChunk = entityContainer.pChunk;
				pChunk->SetComponents<TComponent...>(entityContainer.idx, std::forward<TComponent>(data)...);
			}

			/*!
			Sets values for all listed components of \param entity.
			\warning It is expected the component is not there yet and that
			\param entity is valid. Undefined behavior otherwise.
			*/
			template <typename... TComponent>
			void SetChunkComponent(Entity entity, TComponent&&... data) {
				VerifyComponents<TComponent...>();
				GAIA_ASSERT(IsEntityValid(entity));

				auto& entityContainer = m_entities[entity.id()];
				auto* pChunk = entityContainer.pChunk;
				pChunk->SetChunkComponent<TComponent...>(std::forward<TComponent>(data)...);
			}

			//----------------------------------------------------------------------
			// Component data by copy
			//----------------------------------------------------------------------

			/*!
			Returns a copy of value of a component of \param entity.
			\warning It is expected the component is not there yet and that
			\param entity is valid. Undefined behavior otherwise.
			*/
			template <typename... TComponent>
			void GetComponent(Entity entity, TComponent&... data) const {
				VerifyComponents<TComponent...>();
				GAIA_ASSERT(IsEntityValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				const auto* pChunk = entityContainer.pChunk;
				pChunk->GetComponents<TComponent...>(entityContainer.idx, data...);
			}

			/*!
			Returns a copy of value of a chunk component of \param entity.
			\warning It is expected the component is not there yet and that
			\param entity is valid. Undefined behavior otherwise.
			*/
			template <typename... TComponent>
			void GetChunkComponent(Entity entity, TComponent&... data) const {
				VerifyComponents<TComponent...>();
				GAIA_ASSERT(IsEntityValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				const auto* pChunk = entityContainer.pChunk;
				pChunk->GetChunkComponents<TComponent...>(data...);
			}

			//----------------------------------------------------------------------
			// Component data by reference
			//----------------------------------------------------------------------

			/*!
			Returns a const reference to a component of \param entity.
			\warning It is expected the component is not there yet and that
			\param entity is valid. Undefined behavior otherwise.
			*/
			template <typename... TComponent>
			void GetComponent(Entity entity, const TComponent*&... data) const {
				VerifyComponents<TComponent...>();
				GAIA_ASSERT(IsEntityValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				const auto* pChunk = entityContainer.pChunk;
				pChunk->GetComponents<TComponent...>(entityContainer.idx, data...);
			}

			/*!
			Returns a const reference to a chunk component of \param entity.
			\warning It is expected the component is not there yet and that
			\param entity is valid. Undefined behavior otherwise.
			*/
			template <typename... TComponent>
			void GetChunkComponent(Entity entity, const TComponent*&... data) const {
				VerifyComponents<TComponent...>();
				GAIA_ASSERT(IsEntityValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				const auto* pChunk = entityContainer.pChunk;
				pChunk->GetChunkComponents<TComponent...>(data...);
			}

			//----------------------------------------------------------------------

			/*!
			Tells if \param entity contains all the listed components.
			\return True if all listed components are present on entity.
			*/
			template <typename... TComponent>
			[[nodiscard]] bool HasComponents(Entity entity) {
				VerifyComponents<TComponent...>();
				GAIA_ASSERT(IsEntityValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				if (const auto* pChunk = entityContainer.pChunk) {
					const auto& archetype = pChunk->header.owner;
					return archetype.HasComponents<TComponent...>();
				}

				return false;
			}

			/*!
			Tells if \param entity contains any of the listed components.
			\return True if at least one listed components is present on entity.
			*/
			template <typename... TComponent>
			[[nodiscard]] bool HasAnyComponents(Entity entity) {
				VerifyComponents<TComponent...>();
				GAIA_ASSERT(IsEntityValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				if (const auto* pChunk = entityContainer.pChunk) {
					const auto& archetype = pChunk->header.owner;
					return archetype.HasAnyComponents<TComponent...>();
				}

				return false;
			}

			/*!
			Tells if \param entity contains none of the listed components.
			\return True if none of the listed components are present on entity.
			*/
			template <typename... TComponent>
			[[nodiscard]] bool HasNoneComponents(Entity entity) {
				VerifyComponents<TComponent...>();
				GAIA_ASSERT(IsEntityValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				if (const auto* pChunk = entityContainer.pChunk) {
					const auto& archetype = pChunk->header.owner;
					return archetype.HasNoneComponents<TComponent...>();
				}

				return false;
			}

			/*!
			Tells if the chunk \param entity is a part of contains all listed chunk components.
			\return True if all listed chunk components are present on entity's chunk.
			*/
			template <typename... TComponent>
			[[nodiscard]] bool HasChunkComponents(Entity entity) {
				VerifyComponents<TComponent...>();
				GAIA_ASSERT(IsEntityValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				if (const auto* pChunk = entityContainer.pChunk) {
					const auto& archetype = pChunk->header.owner;
					return archetype.HasChunkComponents<TComponent...>();
				}

				return false;
			}

			/*!
			Tells if the chunk \param entity is a part of contains at least one of the listed chunk components.
			\return True if at least one of the listed chunk components are present on entity's chunk.
			*/
			template <typename... TComponent>
			[[nodiscard]] bool HasAnyChunkComponents(Entity entity) {
				VerifyComponents<TComponent...>();
				GAIA_ASSERT(IsEntityValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				if (const auto* pChunk = entityContainer.pChunk) {
					const auto& archetype = pChunk->header.owner;
					return archetype.HasAnyChunkComponents<TComponent...>();
				}

				return false;
			}

			/*!
			Tells if \param entity contains all the listed components.
			\return True if none of the listed chunk components are present on entity's chunk.
			*/
			template <typename... TComponent>
			[[nodiscard]] bool HasNoneChunkComponents(Entity entity) {
				VerifyComponents<TComponent...>();
				GAIA_ASSERT(IsEntityValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				if (const auto* pChunk = entityContainer.pChunk) {
					const auto& archetype = pChunk->header.owner;
					return archetype.HasNoneChunkComponents<TComponent...>();
				}

				return false;
			}

		private:
			template <class T>
			struct IsReadOnlyType:
					std::bool_constant<
							std::is_const<std::remove_reference_t<std::remove_pointer_t<T>>>::value ||
							(!std::is_pointer<T>::value && !std::is_reference<T>::value)> {};

			template <class T, std::enable_if_t<IsReadOnlyType<T>::value>* = nullptr>
			constexpr GAIA_FORCEINLINE const std::decay_t<T>* expandTuple(Chunk& chunk) const {
				return chunk.view_internal<T>().data();
			}

			template <class T, std::enable_if_t<!IsReadOnlyType<T>::value>* = nullptr>
			constexpr GAIA_FORCEINLINE std::decay_t<T>* expandTuple(Chunk& chunk) {
				return chunk.view_rw_internal<T>().data();
			}

			template <typename... TFuncArgs, typename TFunc>
			GAIA_FORCEINLINE void ForEachEntityInChunk(Chunk& chunk, TFunc&& func) {
				auto tup = std::make_tuple(expandTuple<TFuncArgs>(chunk)...);
				const uint32_t size = chunk.GetItemCount();
				for (uint32_t i = 0U; i < size; i++)
					func(std::get<decltype(expandTuple<TFuncArgs>(chunk))>(tup)[i]...);
			}

			template <typename TFunc>
			void ForEachArchetype(EntityQuery& query, TFunc&& func) {
				query.Match(m_archetypeList);
				for (auto* pArchetype: query)
					func(*pArchetype);
			}

			template <typename... TComponents, typename TFunc>
			GAIA_FORCEINLINE void Unpack_ForEachEntityInChunk(
					[[maybe_unused]] utils::func_type_list<TComponents...> types, Chunk& chunk, TFunc&& func) {
				ForEachEntityInChunk<TComponents...>(chunk, std::forward<TFunc>(func));
			}

			template <typename... TComponents>
			GAIA_FORCEINLINE void
			Unpack_ForEachQuery([[maybe_unused]] utils::func_type_list<TComponents...> types, EntityQuery& query) {
				if constexpr (sizeof...(TComponents) > 0)
					query.All<TComponents...>();
			}

			[[nodiscard]] static bool CheckFilters(const EntityQuery& query, const Chunk& chunk) {
				GAIA_ASSERT(chunk.HasEntities() && "CheckFilters called on an empty chunk");

				const auto lastWorldVersion = query.GetWorldVersion();

				// See if any generic component has changed
				{
					const auto& filtered = query.GetFiltered(ComponentType::CT_Generic);
					for (auto typeIndex: filtered) {
						const uint32_t componentIdx = chunk.GetComponentIdx(typeIndex);
						if (chunk.DidChange(ComponentType::CT_Generic, lastWorldVersion, componentIdx))
							return true;
					}
				}

				// See if any chunk component has changed
				{
					const auto& filtered = query.GetFiltered(ComponentType::CT_Chunk);
					for (auto typeIndex: filtered) {
						const uint32_t componentIdx = chunk.GetChunkComponentIdx(typeIndex);
						if (chunk.DidChange(ComponentType::CT_Chunk, lastWorldVersion, componentIdx))
							return true;
					}
				}

				// Skip unchanged chunks.
				return false;
			}

			template <typename TFunc>
			static void RunQueryOnChunks_Direct(World& world, EntityQuery& query, TFunc& func) {
				const uint32_t BatchSize = 256U;
				containers::sarray<Chunk*, BatchSize> tmp;

				// Update the world version
				world.UpdateWorldVersion();

				const bool hasFilters = query.HasFilters();

				// Iterate over all archetypes
				world.ForEachArchetype(query, [&](Archetype& archetype) {
					archetype.info.structuralChangesLocked = true;

					auto exec = [&](const auto& chunksList) {
						uint32_t chunkOffset = 0U;
						uint32_t batchSize = 0U;
						const auto maxIters = (uint32_t)chunksList.size();
						do {
							// Prepare a buffer to iterate over
							for (; chunkOffset < maxIters; ++chunkOffset) {
								auto* pChunk = chunksList[chunkOffset];

								if (!pChunk->HasEntities())
									continue;
								if (!query.CheckConstraints(!pChunk->IsDisabled()))
									continue;
								if (hasFilters && !CheckFilters(query, *pChunk))
									continue;

								tmp[batchSize++] = pChunk;
							}

							// Execute functors in batches
							for (auto chunkIdx = 0U; chunkIdx < batchSize; ++chunkIdx)
								func(*tmp[chunkIdx]);

							// Reset the batch size
							batchSize = 0U;
						} while (chunkOffset < maxIters);
					};

					if (query.CheckConstraints(true))
						exec(archetype.chunks);
					if (query.CheckConstraints(false))
						exec(archetype.chunksDisabled);

					archetype.info.structuralChangesLocked = false;
				});

				query.SetWorldVersion(world.GetWorldVersion());
			}

			template <typename TFunc>
			static void RunQueryOnChunks_Indirect(World& world, EntityQuery& query, TFunc& func) {
				using InputArgs = decltype(utils::func_args(&TFunc::operator()));
				const uint32_t BatchSize = 256U;
				containers::sarray<Chunk*, BatchSize> tmp;

				// Update the world version
				world.UpdateWorldVersion();

				// Add an All filter for components listed as input arguments of func
				world.Unpack_ForEachQuery(InputArgs{}, query);

				const bool hasFilters = query.HasFilters();

				// Iterate over all archetypes
				world.ForEachArchetype(query, [&](Archetype& archetype) {
					archetype.info.structuralChangesLocked = true;

					auto exec = [&](const auto& chunksList) {
						uint32_t chunkOffset = 0U;
						uint32_t batchSize = 0U;
						const auto maxIters = (uint32_t)chunksList.size();
						do {
							// Prepare a buffer to iterate over
							for (; chunkOffset < maxIters; ++chunkOffset) {
								auto* pChunk = chunksList[chunkOffset];

								if (!pChunk->HasEntities())
									continue;
								if (hasFilters && !CheckFilters(query, *pChunk))
									continue;

								tmp[batchSize++] = pChunk;
							}

							// Execute functors in bulk
							const auto size = batchSize;
							for (auto chunkIdx = 0U; chunkIdx < size; ++chunkIdx)
								world.Unpack_ForEachEntityInChunk(InputArgs{}, *tmp[chunkIdx], func);

							// Reset the batch size
							batchSize = 0U;
						} while (chunkOffset < maxIters);
					};

					if (query.CheckConstraints(true))
						exec(archetype.chunks);
					if (query.CheckConstraints(false))
						exec(archetype.chunksDisabled);

					archetype.info.structuralChangesLocked = false;
				});

				query.SetWorldVersion(world.GetWorldVersion());
			}

			//--------------------------------------------------------------------------------

			template <typename TFunc>
			struct ForEachChunkExecutionContext {
				World& world;
				EntityQuery& query;
				TFunc func;

			public:
				ForEachChunkExecutionContext(World& w, EntityQuery& q, TFunc&& f):
						world(w), query(q), func(std::forward<TFunc>(f)) {}
				~ForEachChunkExecutionContext() {
					World::RunQueryOnChunks_Direct(this->world, this->query, this->func);
				}
			};

			//--------------------------------------------------------------------------------

			template <typename TFunc>
			struct ForEachExecutionContext_External {
				World& world;
				EntityQuery& query;
				TFunc func;

			public:
				ForEachExecutionContext_External(World& w, EntityQuery& q, TFunc&& f):
						world(w), query(q), func(std::forward<TFunc>(f)) {}
				~ForEachExecutionContext_External() {
					World::RunQueryOnChunks_Indirect(this->world, this->query, this->func);
				}
			};

			template <typename TFunc>
			struct ForEachExecutionContext_Internal {
				World& world;
				// TODO: Cache these in the parent world for better performance
				EntityQuery query;
				TFunc func;

			public:
				ForEachExecutionContext_Internal(World& w, EntityQuery&& q, TFunc&& f):
						world(w), query(std::move(q)), func(std::forward<TFunc>(f)) {}
				~ForEachExecutionContext_Internal() {
					World::RunQueryOnChunks_Indirect(this->world, this->query, this->func);
				}
			};

			//--------------------------------------------------------------------------------

		public:
			/*!
			Iterates over all chunks satisfying conditions set by \param query and calls \param func for all of them.
			Exposing chunks this version of iteration gives users the most control over have data is processed.
			\warning This version makes it possible to perform optimizations otherwise not possible with other methods
							 of iteration. On the other hand it is more verbose and takes more lines of code when used.
			*/
			template <typename TFunc>
			void ForEachChunk(EntityQuery& query, TFunc&& func) {
				ForEachChunkExecutionContext<TFunc>{(World&)*this, query, std::forward<TFunc>(func)};
			}

			/*!
			Iterates over all chunks satisfying conditions set by \param query and calls \param func for all of them.
			\warning Performance-wise it has less potential than ForEachChunk. However, it is easier to use and unless
							 some specific optimizations are necessary is the preffered way of iterating over data.
			*/
			template <typename TFunc>
			void ForEach(EntityQuery& query, TFunc&& func) {
				ForEachExecutionContext_External<TFunc>{(World&)*this, query, std::forward<TFunc>(func)};
			}

			/*!
			Iterates over all chunks satisfying conditions set by \param query and calls \param func for all of them.
			\warning Performance-wise it has less potential than ForEachChunk. However, it is easier to use and unless
							 some specific optimizations are necessary is the preffered way of iterating over data.
			*/
			template <typename TFunc>
			void ForEach(EntityQuery&& query, TFunc&& func) {
				ForEachExecutionContext_Internal<TFunc>{(World&)*this, std::move(query), std::forward<TFunc>(func)};
			}

			/*!
			Iterates over all chunks satisfying conditions set by \param func and calls \param func for all of them.
			EntityQuery instance is generated internally from the input arguments of \param func.
			\warning Performance-wise it has less potential than ForEachChunk. However, it is easier to use and unless
							 some specific optimizations are necessary is the preffered way of iterating over data.
			*/
			template <typename TFunc>
			void ForEach(TFunc&& func) {
				ForEachExecutionContext_Internal<TFunc>{(World&)*this, EntityQuery(), std::forward<TFunc>(func)};
			}

			/*!
			Collect garbage. Check all chunks and archetypes which are empty and have not been used for a while
			and tries to delete them and release memory allocated by them.
			*/
			void GC() {
				// Handle chunks
				for (auto i = 0U; i < (uint32_t)m_chunksToRemove.size();) {
					auto* pChunk = m_chunksToRemove[i];

					// Skip reclaimed chunks
					if (pChunk->HasEntities()) {
						pChunk->header.info.lifespan = MAX_CHUNK_LIFESPAN;
						utils::erase_fast(m_chunksToRemove, i);
						continue;
					}

					GAIA_ASSERT(pChunk->header.info.lifespan > 0);
					--pChunk->header.info.lifespan;
					if (pChunk->header.info.lifespan > 0) {
						++i;
						continue;
					}
				}

				// Remove all dead chunks
				for (auto* pChunk: m_chunksToRemove) {
					const_cast<Archetype&>(pChunk->header.owner).RemoveChunk(pChunk);
				}
				m_chunksToRemove.clear();
			}

			/*!
			Performs diagnostics on archetypes. Prints basic info about them and the chunks they contain.
			*/
			void DiagArchetypes() const {
				static bool DiagArchetypes = GAIA_ECS_DIAG_ARCHETYPES;
				if (DiagArchetypes) {
					DiagArchetypes = false;
					containers::map<uint64_t, uint32_t> archetypeEntityCountMap;
					for (const auto* archetype: m_archetypeList)
						archetypeEntityCountMap.insert({archetype->lookupHash, 0});

					// Calculate the number of entities using a given archetype
					for (const auto& e: m_entities) {
						if (!e.pChunk)
							continue;

						auto it = archetypeEntityCountMap.find(e.pChunk->header.owner.lookupHash);
						if (it != archetypeEntityCountMap.end())
							++it->second;
					}

					// Print archetype info
					LOG_N("Archetypes:%u", (uint32_t)m_archetypeList.size());
					for (const auto* archetype: m_archetypeList) {
						const auto& genericComponents = archetype->componentTypeList[ComponentType::CT_Generic];
						const auto& chunkComponents = archetype->componentTypeList[ComponentType::CT_Chunk];
						uint32_t genericComponentsSize = 0;
						uint32_t chunkComponentsSize = 0;
						for (const auto& component: genericComponents)
							genericComponentsSize += component.type->info.size;
						for (const auto& component: chunkComponents)
							chunkComponentsSize += component.type->info.size;

						const auto it = archetypeEntityCountMap.find(archetype->lookupHash);
						LOG_N(
								"Archetype ID:%u, "
								"lookupHash:%016" PRIx64 ", "
								"matcherHash Generic:%016" PRIx64 ", "
								"matcherHash Chunk:%016" PRIx64 ", "
								"chunks:%u, data size:%u B (%u + %u), "
								"entities:%u (max-per-chunk:%u)",
								archetype->id, archetype->lookupHash, archetype->matcherHash[ComponentType::CT_Generic],
								archetype->matcherHash[ComponentType::CT_Chunk], (uint32_t)archetype->chunks.size(),
								genericComponentsSize + chunkComponentsSize, genericComponentsSize, chunkComponentsSize, it->second,
								archetype->info.capacity);

						auto logComponentInfo = [](const ComponentMetaData* metaType) {
							LOG_N(
									"    (%p) lookupHash:%016" PRIx64 ", matcherHash:%016" PRIx64 ", size:%3u B, align:%3u B, %.*s",
									(void*)metaType, metaType->lookupHash, metaType->matcherHash, metaType->info.size,
									metaType->info.alig, (uint32_t)metaType->name.length(), metaType->name.data());
						};

						if (!genericComponents.empty()) {
							LOG_N("  Generic components - count:%u", (uint32_t)genericComponents.size());
							for (const auto& component: genericComponents)
								logComponentInfo(component.type);
						}
						if (!chunkComponents.empty()) {
							LOG_N("  Chunk components - count:%u", (uint32_t)chunkComponents.size());
							for (const auto& component: chunkComponents)
								logComponentInfo(component.type);
						}

						const auto& addEdges = archetype->edgesAdd;
						if (!addEdges.empty()) {
							LOG_N("  Add edges - count:%u", (uint32_t)addEdges.size());

							{
								uint32_t edgeCount = 0;
								for (const auto& edge: addEdges)
									if (edge.componentType == ComponentType::CT_Generic)
										++edgeCount;
								if (edgeCount > 0)
									LOG_N("    Generic - count:%u", edgeCount);

								for (const auto& edge: addEdges) {
									if (edge.componentType == ComponentType::CT_Generic)
										continue;
									LOG_N(
											"      %.*s (--> Archetype ID:%u)", (uint32_t)edge.type->name.length(), edge.type->name.data(),
											edge.archetype->id);
								}
							}

							{
								uint32_t edgeCount = 0;
								for (const auto& edge: addEdges)
									if (edge.componentType == ComponentType::CT_Chunk)
										++edgeCount;
								if (edgeCount > 0)
									LOG_N("    Chunk - count:%u", edgeCount);

								for (const auto& edge: addEdges) {
									if (edge.componentType == ComponentType::CT_Chunk)
										continue;
									LOG_N(
											"      %.*s (--> Archetype ID:%u)", (uint32_t)edge.type->name.length(), edge.type->name.data(),
											edge.archetype->id);
								}
							}
						}

						const auto& delEdges = archetype->edgesDel;
						if (!delEdges.empty()) {
							LOG_N("  Del edges - count:%u", (uint32_t)addEdges.size());

							{
								uint32_t edgeCount = 0;
								for (const auto& edge: delEdges)
									if (edge.componentType == ComponentType::CT_Generic)
										++edgeCount;
								if (edgeCount > 0)
									LOG_N("    Generic - count:%u", edgeCount);

								for (const auto& edge: delEdges) {
									if (edge.componentType == ComponentType::CT_Generic)
										continue;
									LOG_N(
											"      %.*s (<-- Archetype ID:%u)", (uint32_t)edge.type->name.length(), edge.type->name.data(),
											edge.archetype->id);
								}
							}

							{
								uint32_t edgeCount = 0;
								for (const auto& edge: delEdges)
									if (edge.componentType == ComponentType::CT_Chunk)
										++edgeCount;
								if (edgeCount > 0)
									LOG_N("    Chunk - count:%u", edgeCount);

								for (const auto& edge: delEdges) {
									if (edge.componentType == ComponentType::CT_Chunk)
										continue;
									LOG_N(
											"      %.*s (<-- Archetype ID:%u)", (uint32_t)edge.type->name.length(), edge.type->name.data(),
											edge.archetype->id);
								}
							}
						}

						const auto& chunks = archetype->chunks;
						for (auto i = 0U; i < (uint32_t)chunks.size(); ++i) {
							const auto* pChunk = chunks[i];
							const auto entityCount = pChunk->header.items.count;
							LOG_N(
									"  Chunk #%04u, entities:%u/%u, lifespan:%u", i, entityCount, archetype->info.capacity,
									pChunk->header.info.lifespan);
						}
					}
				}
			}

			/*!
			Performs diagnostics on registered components.
			Prints basic info about them and reports and detected issues.
			*/
			void DiagRegisteredTypes() const {
				static bool DiagRegisteredTypes = GAIA_ECS_DIAG_REGISTERED_TYPES;
				if (DiagRegisteredTypes) {
					DiagRegisteredTypes = false;

					g_ComponentCache.Diag();
				}
			}

			/*!
			Performs diagnostics on entites of the world.
			Also performs validation of internal structures which hold the entities.
			*/
			void DiagEntities() const {
				static bool DiagDeletedEntities = GAIA_ECS_DIAG_DELETED_ENTITIES;
				if (DiagDeletedEntities) {
					DiagDeletedEntities = false;

					ValidateEntityList();

					LOG_N("Deleted entities: %u", m_freeEntities);
					if (m_freeEntities) {
						LOG_N("  --> %u", m_nextFreeEntity);

						uint32_t iters = 0U;
						auto fe = m_entities[m_nextFreeEntity].idx;
						while (fe != Entity::IdMask) {
							LOG_N("  --> %u", m_entities[fe].idx);
							fe = m_entities[fe].idx;
							++iters;
							if (!iters || iters > m_freeEntities) {
								LOG_E("  Entities recycle list contains inconsistent "
											"data!");
								break;
							}
						}
					}
				}
			}

			/*!
			Performs diagnostics of the memory used by the world.
			*/
			void DiagMemory() const {
				ChunkAllocatorStats memstats;
				m_chunkAllocator.GetStats(memstats);
				LOG_N("ChunkAllocator stats");
				LOG_N("  Allocated: %016" PRIu64 " B", memstats.AllocatedMemory);
				LOG_N("  Used: %016" PRIu64 " B", memstats.AllocatedMemory - memstats.UsedMemory);
				LOG_N("  Overhead: %016" PRIu64 " B", memstats.UsedMemory);
				LOG_N("  Utilization: %.1f%%", 100.0 * ((double)memstats.UsedMemory / (double)memstats.AllocatedMemory));
				LOG_N("  Pages: %u", memstats.NumPages);
				LOG_N("  Free pages: %u", memstats.NumFreePages);
			}

			/*!
			Performs all diagnostics.
			*/
			void Diag() const {
				DiagArchetypes();
				DiagRegisteredTypes();
				DiagEntities();
				DiagMemory();
			}
		};

		inline uint32_t GetWorldVersionFromWorld(const World& world) {
			return world.GetWorldVersion();
		}
		inline void* AllocateChunkMemory(World& world) {
			return world.AllocateChunkMemory();
		}
		inline void ReleaseChunkMemory(World& world, void* mem) {
			world.ReleaseChunkMemory(mem);
		}
	} // namespace ecs
} // namespace gaia