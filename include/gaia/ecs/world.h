#pragma once
#include <cassert>
#include <inttypes.h>
#include <unordered_map>
#include <vector>

#include "../config/config.h"
#include "../utils/span.h"
#include "../utils/type_info.h"
#include "../utils/utility.h"
#include "chunk_allocator.h"
#include "creation_query.h"
#include "entity.h"
#include "entity_query.h"
#include "fwd.h"


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
			std::unordered_map<uint64_t, std::vector<Archetype*>> m_archetypeMap;
			//! List of archetypes - used for iteration
			std::vector<Archetype*> m_archetypeList;
			//! List of unique archetype hashes - used for iteration
			std::vector<uint64_t> m_archetypeHashList;

			//! List of entities. Used for look-ups only when searching for entities
			//! in chunks + data validation
			std::vector<EntityContainer> m_entities;
			//! Identified of the next entity to recycle
			uint32_t m_nextFreeEntity = Entity::IdMask;
			//! Number of entites to recycle
			uint32_t m_freeEntities = 0;

			//! List of chunks to delete
			std::vector<Chunk*> m_chunksToRemove;

			//! With every structural change world version changes
			uint32_t m_worldVersion = 0;

			void UpdateWorldVersion() {
				UpdateVersion(m_worldVersion);
			}

			void* AllocateChunkMemory() {
				return m_chunkAllocator.Allocate();
			}

			void ReleaseChunkMemory(void* mem) {
				m_chunkAllocator.Release(mem);
			}

		public:
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
				if (entityContainer.chunk &&
						entityContainer.chunk->GetEntity(entityContainer.idx) != entity)
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
					m_archetypeHashList = {};
					m_archetypeMap = {};
				}
			}

		private:
			//! Remove entity from a chunk. If the chunk is empty it's queued for
			//! removal as well.
			void RemoveEntity(Chunk* pChunk, uint16_t entityChunkIndex) {
				pChunk->RemoveEntity(entityChunkIndex, m_entities);

				if (
						// Skip non-empty chunks
						pChunk->HasEntities() ||
						// Skip chunks which already requested removal
						pChunk->header.remove)
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
				pChunk->header.remove = true;

				m_chunksToRemove.push_back(pChunk);
			}

			[[nodiscard]] Archetype* FindArchetype(
					std::span<const ComponentMetaData*> genericTypes,
					std::span<const ComponentMetaData*> chunkTypes,
					const uint64_t lookupHash) {
				// Search for the archetype in the map
				const auto it = m_archetypeMap.find(lookupHash);
				if (it == m_archetypeMap.end())
					return nullptr;

				const auto& archetypeArray = it->second;

				auto checkTypes =
						[&](const ChunkComponentList& list,
								const std::span<const ComponentMetaData*>& types) {
							for (uint32_t j = 0; j < types.size(); j++) {
								// Different components. We need to search further
								if (list[j].type != types[j])
									return false;
							}
						return true;
						};

					// Iterate over the list of archetypes and find the exact match
					for (const auto archetype: archetypeArray) {
						const auto& genericComponentList =
								archetype->componentList[ComponentType::CT_Generic];
						if (genericComponentList.size() != genericTypes.size())
							continue;
						const auto& chunkComponentList =
								archetype->componentList[ComponentType::CT_Chunk];
						if (chunkComponentList.size() != chunkTypes.size())
							continue;

						if (checkTypes(genericComponentList, genericTypes) &&
								checkTypes(chunkComponentList, chunkTypes))
							return archetype;
					}

				return nullptr;
			}

			[[nodiscard]] Archetype* CreateArchetype(
					std::span<const ComponentMetaData*> genericTypes,
					std::span<const ComponentMetaData*> chunkTypes,
					const uint64_t lookupHash) {
				auto newArch = Archetype::Create(*this, genericTypes, chunkTypes);
				newArch->lookupHash = lookupHash;

				m_archetypeList.push_back(newArch);
				m_archetypeHashList.push_back(lookupHash);

				auto it = m_archetypeMap.find(lookupHash);
					if (it == m_archetypeMap.end()) {
						m_archetypeMap.insert({lookupHash, {newArch}});
					} else {
						auto& archetypes = it->second;
						if (!utils::has(archetypes, newArch))
							archetypes.push_back(newArch);
					}

				return newArch;
			}

			[[nodiscard]] Archetype* FindOrCreateArchetype(
					std::span<const ComponentMetaData*> genericTypes,
					std::span<const ComponentMetaData*> chunkTypes) {
				// Make sure to sort the meta-types so we receive the same hash no
				// matter the order in which components are provided Bubble sort is
				// okay. We're dealing with at most MAX_COMPONENTS_PER_ARCHETYPE
				// items.
				// TODO: Replace with a sorting network
				std::sort(
						genericTypes.begin(), genericTypes.end(),
						std::less<const ComponentMetaData*>());
				std::sort(
						chunkTypes.begin(), chunkTypes.end(),
						std::less<const ComponentMetaData*>());

				// Calculate hash for our combination of components
				const auto lookupHash = CalculateLookupHash(std::array<uint64_t, 2>{
						CalculateLookupHash(genericTypes),
						CalculateLookupHash(chunkTypes)});

				if (auto archetype =
								FindArchetype(genericTypes, chunkTypes, lookupHash))
					return archetype;

				// Archetype wasn't found so we have to create a new one
				return CreateArchetype(genericTypes, chunkTypes, lookupHash);
			}

			[[nodiscard]] Archetype* FindOrCreateArchetype(CreationQuery& query) {
				// TODO: Verify if return FindOrCreateArchetype(std::span<const
				// ComponentMetaData*>(types)) works the same way
				return FindOrCreateArchetype(
						std::span(query.list[ComponentType::CT_Generic]),
						std::span(query.list[ComponentType::CT_Chunk]));
			}

			[[nodiscard]] Archetype* GetArchetype(Entity entity) const {
				GAIA_ASSERT(IsEntityValid(entity));

				auto& entityContainer = m_entities[entity.id()];
				auto chunk = entityContainer.chunk;
				return chunk ? (Archetype*)&chunk->header.owner : nullptr;
			}

			void RemoveArchetype(Archetype* pArchetype) {
				const auto idx = utils::get_index(m_archetypeList, pArchetype);
				if (idx == utils::BadIndex)
					return;

				utils::erase_fast(m_archetypeList, idx);
				utils::erase_fast(m_archetypeHashList, idx);

				const auto it = m_archetypeMap.find(pArchetype->lookupHash);
				if (it != m_archetypeMap.end())
					utils::erase_fast(
							it->second, utils::get_index(it->second, pArchetype));

				delete pArchetype;
			}

			[[nodiscard]] Entity AllocateEntity() {
					if (!m_freeEntities) {
						// We don't want to go out of range for new entities
						GAIA_ASSERT(
								m_entities.size() < Entity::IdMask &&
								"Trying to allocate too many entities!");
						m_entities.push_back({nullptr, 0, 0});

						return {(EntityId)m_entities.size() - 1, 0};
					} else {
						--m_freeEntities;
						GAIA_ASSERT(
								m_nextFreeEntity < m_entities.size() &&
								"ECS recycle list broken!");

						const auto index = m_nextFreeEntity;
						m_nextFreeEntity = m_entities[m_nextFreeEntity].idx;
						return {index, m_entities[index].gen};
					}
			}

			void DeallocateEntity(Entity entityToDelete) {
				auto& entityContainer = m_entities[entityToDelete.id()];
				entityContainer.chunk = nullptr;

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

			void StoreEntity(Entity entity, Chunk* chunk) {
				auto& entityContainer = m_entities[entity.id()];
				entityContainer.chunk = chunk;

				const auto idx = chunk->AddEntity(entity);
				const auto gen = entityContainer.gen;
				entityContainer.idx = idx;
				entityContainer.gen = gen;
			}

			EntityContainer* AddComponent_Internal(
					ComponentType TYPE, Entity entity,
					std::span<const ComponentMetaData*> typesToAdd) {
				const auto newTypesCount = (uint32_t)typesToAdd.size();
				auto& entityContainer = m_entities[entity.id()];

					// Adding a component to an entity which already is a part of some
					// chunk
					if (auto chunk = entityContainer.chunk) {
						const auto& archetype = chunk->header.owner;
						const auto& componentList = archetype.componentList[TYPE];

						const auto metatypesCount =
								(uint32_t)componentList.size() + (uint32_t)typesToAdd.size();
							if (!VerityArchetypeComponentCount(metatypesCount)) {
								GAIA_ASSERT(
										false &&
										"Trying to add too many ECS components to ECS entity!");
#if GAIA_DEBUG
								LOG_W(
										"Trying to add %u ECS %s components to ECS entity [%u.%u] "
										"but "
										"there's only enough room for %u more!",
										newTypesCount, ComponentTypeString[TYPE], entity.id(),
										entity.gen(),
										MAX_COMPONENTS_PER_ARCHETYPE -
												(uint32_t)archetype.componentList[TYPE].size());
								LOG_W("Already present:");
								for (uint32_t i = 0u; i < componentList.size(); i++)
									LOG_W("> [%u] %s", i, componentList[i].type->name.data());
								LOG_W("Trying to add:");
								for (uint32_t i = 0u; i < newTypesCount; i++)
									LOG_W("> [%u] %s", i, typesToAdd[i]->name.data());
#endif
								return nullptr;
						}

						auto newMetatypes = (const ComponentMetaData**)alloca(
								sizeof(ComponentMetaData) * metatypesCount);

						uint32_t j = 0;
							for (uint32_t i = 0; i < componentList.size(); i++) {
								const auto& info = componentList[i];
									// Don't add the same component twice
									for (uint32_t k = 0; k < newTypesCount; k++) {
											if (info.type == typesToAdd[k]) {
												GAIA_ASSERT(
														false &&
														"Trying to add a duplicate ECS component to "
														"ECS entity");
#if GAIA_DEBUG
												LOG_W(
														"Trying to add a duplicate ECS %s component to ECS "
														"entity "
														"[%u.%u]",
														ComponentTypeString[TYPE], entity.id(),
														entity.gen());
												LOG_W("> %s", info.type->name.data());
#endif
												return nullptr;
										}
									}
								// Keep the types offset by the number of new input types
								newMetatypes[newTypesCount + j++] = info.type;
							}
						// Fill in the gap with new input types
						for (uint32_t i = 0; i < newTypesCount; i++)
							newMetatypes[i] = typesToAdd[i];

						const auto& secondList = archetype.componentList[(TYPE + 1) & 1];
						auto secondMetaTypes = (const ComponentMetaData**)alloca(
								sizeof(ComponentMetaData) * secondList.size());
						for (uint32_t i = 0; i < secondList.size(); i++)
							secondMetaTypes[i] = secondList[i].type;

						auto newArchetype =
								TYPE == ComponentType::CT_Generic
										? FindOrCreateArchetype(
													std::span(newMetatypes, metatypesCount),
													std::span(secondMetaTypes, secondList.size()))
										: FindOrCreateArchetype(
													std::span(secondMetaTypes, secondList.size()),
													std::span(newMetatypes, metatypesCount));

						MoveEntity(entity, *newArchetype);
					}
					// Adding a component to an empty entity
					else {
							if (!VerityArchetypeComponentCount(newTypesCount)) {
								GAIA_ASSERT(
										false &&
										"Trying to add too many ECS components to ECS entity!");
#if GAIA_DEBUG
								LOG_W(
										"Trying to add %u ECS %s components to ECS entity [%u.%u] "
										"but maximum of only %u is supported!",
										newTypesCount, ComponentTypeString[TYPE], entity.id(),
										entity.gen(), MAX_COMPONENTS_PER_ARCHETYPE);
								for (uint32_t i = 0; i < newTypesCount; i++)
									LOG_W("> [%u] %s", i, typesToAdd[i]->name.data());
#endif
								return nullptr;
						}

						auto newArchetype = TYPE == ComponentType::CT_Generic
																		? FindOrCreateArchetype(typesToAdd, {})
																		: FindOrCreateArchetype({}, typesToAdd);

						StoreEntity(entity, newArchetype->FindOrCreateChunk());
					}

				return &entityContainer;
			}

			template <typename... TComponent>
			EntityContainer*
			AddComponent_Internal(ComponentType TYPE, Entity entity) {
				constexpr auto newTypesCount = (uint32_t)sizeof...(TComponent);
				auto& entityContainer = m_entities[entity.id()];

					// Adding a component to an entity which already is a part of some
					// chunk
					if (auto chunk = entityContainer.chunk) {
						const auto& archetype = chunk->header.owner;
						const auto& componentList = archetype.componentList[TYPE];

						const auto metatypesCount =
								(uint32_t)componentList.size() + newTypesCount;
							if (!VerityArchetypeComponentCount(metatypesCount)) {
								GAIA_ASSERT(
										false &&
										"Trying to add too many ECS components to ECS entity!");
#if GAIA_DEBUG
								LOG_W(
										"Trying to add %u ECS %s components to ECS entity [%u.%u] "
										"but there's only enough room for %u more!",
										newTypesCount, ComponentTypeString[TYPE], entity.id(),
										entity.gen(),
										MAX_COMPONENTS_PER_ARCHETYPE -
												(uint32_t)archetype.componentList[TYPE].size());
								LOG_W("Already present:");
								for (uint32_t i = 0; i < componentList.size(); i++)
									LOG_W("> [%u] %s", i, componentList[i].type->name.data());
								std::string_view newNames[] = {
										utils::type_info::name<TComponent>()...};
								LOG_W("Trying to add:");
								for (uint32_t i = 0; i < newTypesCount; i++)
									LOG_W(
											"> [%u] %.*s", i, (uint32_t)newNames[i].length(),
											newNames[i].data());
#endif
								return nullptr;
						}

						const ComponentMetaData* typesToAdd[] = {
								g_ComponentCache.GetOrCreateComponentMetaType<TComponent>()...};

						auto newMetatypes = (const ComponentMetaData**)alloca(
								sizeof(ComponentMetaData) * metatypesCount);

						uint32_t j = 0;
							for (uint32_t i = 0; i < componentList.size(); i++) {
								const auto& info = componentList[i];

									// Don't add the same component twice
									for (uint32_t k = 0; k < newTypesCount; k++) {
											if (info.type == typesToAdd[k]) {
												GAIA_ASSERT(
														false &&
														"Trying to add a duplicate ECS component to "
														"ECS entity");
#if GAIA_DEBUG
												LOG_W(
														"Trying to add a duplicate ECS %s component to ECS "
														"entity "
														"[%u.%u]",
														ComponentTypeString[TYPE], entity.id(),
														entity.gen());
												LOG_W("> %s", info.type->name.data());
#endif
												return nullptr;
										}
									}

								// Keep the types offset by the number of new input types
								newMetatypes[newTypesCount + j++] = info.type;
							}
						// Fill in the gap with new input types
						for (uint32_t i = 0; i < newTypesCount; i++)
							newMetatypes[i] = typesToAdd[i];

						const auto& secondList = archetype.componentList[(TYPE + 1) & 1];
						auto secondMetaTypes = (const ComponentMetaData**)alloca(
								sizeof(ComponentMetaData) * secondList.size());
						for (uint32_t i = 0; i < secondList.size(); i++)
							secondMetaTypes[i] = secondList[i].type;

						auto newArchetype =
								TYPE == ComponentType::CT_Generic
										? FindOrCreateArchetype(
													std::span(newMetatypes, metatypesCount),
													std::span(secondMetaTypes, secondList.size()))
										: FindOrCreateArchetype(
													std::span(secondMetaTypes, secondList.size()),
													std::span(newMetatypes, metatypesCount));

						MoveEntity(entity, *newArchetype);
					}
					// Adding a component to an empty entity
					else {
							if (!VerityArchetypeComponentCount(newTypesCount)) {
								GAIA_ASSERT(
										false &&
										"Trying to add too many ECS components to ECS entity!");
#if GAIA_DEBUG
								LOG_W(
										"Trying to add %u ECS %s components to ECS entity [%u.%u] "
										"but maximum of only %u is supported!",
										newTypesCount, ComponentTypeString[TYPE], entity.id(),
										entity.gen(), MAX_COMPONENTS_PER_ARCHETYPE);
								std::string_view newNames[] = {
										utils::type_info::name<TComponent>()...};
								for (auto i = 0U; i < newTypesCount; i++)
									LOG_W(
											"> [%u] %.*s", i, (uint32_t)newNames[i].length(),
											newNames[i].data());
#endif
								return nullptr;
						}

						const ComponentMetaData* newMetatypes[] = {
								g_ComponentCache.GetOrCreateComponentMetaType<TComponent>()...};

						auto newArchetype =
								TYPE == ComponentType::CT_Generic
										? FindOrCreateArchetype(
													std::span(newMetatypes, newTypesCount), {})
										: FindOrCreateArchetype(
													{}, std::span(newMetatypes, newTypesCount));

						StoreEntity(entity, newArchetype->FindOrCreateChunk());
					}

				return &entityContainer;
			}

			void RemoveComponent_Internal(
					ComponentType TYPE, Entity entity,
					std::span<const ComponentMetaData*> typesToRemove) {
				auto& entityContainer = m_entities[entity.id()];
				auto chunk = entityContainer.chunk;

				const auto& archetype = chunk->header.owner;
				const auto& componentList = archetype.componentList[TYPE];

				// find intersection
				const auto metatypesCount = componentList.size();
				auto newMetatypes = (const ComponentMetaData**)alloca(
						sizeof(ComponentMetaData) * metatypesCount);

				size_t typesAfter = 0;
					// TODO: Arrays are sorted so we can make this in O(n+m) instead of
					// O(N^2)
					for (auto i = 0U; i < componentList.size(); i++) {
						const auto& info = componentList[i];

							for (auto k = 0U; k < typesToRemove.size(); k++) {
								if (info.type == typesToRemove[k])
									goto nextIter;
							}

						newMetatypes[typesAfter++] = info.type;

					nextIter:
						continue;
					}

				// Nothing has changed. Return
				if (typesAfter == metatypesCount)
					return;

				const auto& secondList = archetype.componentList[(TYPE + 1) & 1];
				auto secondMetaTypes = (const ComponentMetaData**)alloca(
						sizeof(ComponentMetaData) * secondList.size());
				for (auto i = 0U; i < secondList.size(); i++)
					secondMetaTypes[i] = secondList[i].type;

				auto newArchetype =
						TYPE == ComponentType::CT_Generic
								? FindOrCreateArchetype(
											std::span(newMetatypes, typesAfter),
											std::span(secondMetaTypes, secondList.size()))
								: FindOrCreateArchetype(
											std::span(secondMetaTypes, secondList.size()),
											std::span(newMetatypes, typesAfter));

				MoveEntity(entity, *newArchetype);
			}

			template <typename... TComponent>
			void RemoveComponent_Internal(ComponentType TYPE, Entity entity) {
				auto& entityContainer = m_entities[entity.id()];
				auto chunk = entityContainer.chunk;

				const ComponentMetaData* typesToRemove[] = {
						g_ComponentCache.GetOrCreateComponentMetaType<TComponent>()...};

				const auto& archetype = chunk->header.owner;
				const auto& componentList = archetype.componentList[TYPE];

				// find intersection
				const auto metatypesCount = componentList.size();
				auto newMetatypes = (const ComponentMetaData**)alloca(
						sizeof(ComponentMetaData) * metatypesCount);

				size_t typesAfter = 0;
					// TODO: Arrays are sorted so we can make this in O(n+m) instead of
					// O(N^2)
					for (auto i = 0U; i < componentList.size(); i++) {
						const auto& info = componentList[i];

							for (auto k = 0U; k < sizeof...(TComponent); k++) {
								if (info.type == typesToRemove[k])
									goto nextIter;
							}

						newMetatypes[typesAfter++] = info.type;

					nextIter:
						continue;
					}

				// Nothing has changed. Return
				if (typesAfter == metatypesCount)
					return;

				// TODO: Remove the archetype after the last component is removed?
				/*if (typesAfter == 0)
				{
				...
				}*/

				const auto& secondList = archetype.componentList[(TYPE + 1) & 1];
				auto secondMetaTypes = (const ComponentMetaData**)alloca(
						sizeof(ComponentMetaData) * secondList.size());
				for (uint32_t i = 0; i < secondList.size(); i++)
					secondMetaTypes[i] = secondList[i].type;

				auto newArchetype =
						TYPE == ComponentType::CT_Generic
								? FindOrCreateArchetype(
											std::span(newMetatypes, typesAfter),
											std::span(secondMetaTypes, secondList.size()))
								: FindOrCreateArchetype(
											std::span(secondMetaTypes, secondList.size()),
											std::span(newMetatypes, typesAfter));
				MoveEntity(entity, *newArchetype);
			}

			void MoveEntity(Entity oldEntity, Archetype& newArchetype) {
				auto& entityContainer = m_entities[oldEntity.id()];
				auto oldChunk = entityContainer.chunk;
				const auto oldIndex = entityContainer.idx;
				const auto& oldArchetype = oldChunk->header.owner;

				// find a new chunk for the entity and move it inside.
				// Old entity ID needs to remain valid or lookups would break.
				auto newChunk = newArchetype.FindOrCreateChunk();
				const auto newIndex = newChunk->AddEntity(oldEntity);

				// find intersection of the two component lists
				const auto& oldInfo =
						oldArchetype.componentList[ComponentType::CT_Generic];
				const auto& newInfo =
						newArchetype.componentList[ComponentType::CT_Generic];

				// TODO: Handle chunk components!

				struct Intersection {
					uint32_t size;
					uint32_t oldIndex;
					uint32_t newIndex;
				};

				const auto maxIntersectionCount =
						oldInfo.size() > newInfo.size() ? newInfo.size() : oldInfo.size();
				auto intersections =
						(Intersection*)alloca(maxIntersectionCount * sizeof(Intersection));
				uint32_t intersectionCount = 0;

					// TODO: Arrays are sorted so we can do this in O(n+_) instead of
					// O(N^2)
					for (uint32_t i = 0; i < oldInfo.size(); i++) {
						const auto typeOld = oldInfo[i].type;
						if (!typeOld->size)
							continue;

							for (uint32_t j = 0; j < newInfo.size(); j++) {
								const auto typeNew = newInfo[j].type;
									if (typeNew == typeOld) {
										intersections[intersectionCount] = {typeOld->size, i, j};
										++intersectionCount;
								}
							}
					}

					// Let's move all data from oldEntity to newEntity
					for (uint32_t i = 0; i < intersectionCount; i++) {
						const uint32_t idxFrom = newInfo[intersections[i].newIndex].offset +
																		 intersections[i].size * newIndex;
						const uint32_t idxTo = oldInfo[intersections[i].oldIndex].offset +
																	 intersections[i].size * oldIndex;

						GAIA_ASSERT(idxFrom < Chunk::DATA_SIZE);
						GAIA_ASSERT(idxTo < Chunk::DATA_SIZE);

						memcpy(
								&newChunk->data[idxFrom], &oldChunk->data[idxTo],
								intersections[i].size);
					}

				// Remove entity from the previous chunk
				RemoveEntity(oldChunk, oldIndex);

				// Update entity's chunk and index so look-ups can find it
				entityContainer.chunk = newChunk;
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
						GAIA_ASSERT(
								nextFreeEntity < m_entities.size() &&
								"ECS recycle list broken!");

						nextFreeEntity = m_entities[nextFreeEntity].idx;
						--freeEntities;
					}

				// At this point the index of the last index in list should
				// point to -1 because that's the tail of our implicit list.
				GAIA_ASSERT(nextFreeEntity == Entity::IdMask);
#endif
			}

			//! Verifies than the chunk is valid
			void ValidateChunk([[maybe_unused]] Chunk* chunk) const {
#if GAIA_ECS_VALIDATE_CHUNKS
					// Make sure no entites reference the deleted chunk
					for ([[maybe_unused]] const auto& e: m_entities) {
						GAIA_ASSERT(chunk->HasEntities() || e.chunk != chunk);
					}
#endif
			}

			void Init() {}

			void Done() {
				Cleanup();
				m_chunkAllocator.Flush();

#if GAIA_DEBUG
				// Make sure there are no leaks
				ChunkAllocatorStats memstats;
				m_chunkAllocator.GetStats(memstats);
					if (memstats.NumAllocations != 0) {
						LOG_W(
								"ECS leaking memory: %u of unreleased allocations "
								"(blocks:%u, "
								"allocated:%" PRIu64 "B, overhead:%" PRIu64 "B)",
								(uint32_t)memstats.NumAllocations, (uint32_t)memstats.NumBlocks,
								memstats.Allocated, memstats.Overhead);
				}

#endif
			}

			/*!
			Creates a new entity from archetype
			\return Entity
			*/
			Entity CreateEntity(Archetype& archetype) {
				Entity entity = AllocateEntity();

				auto chunk = m_entities[entity.id()].chunk;
				if (chunk == nullptr)
					chunk = archetype.FindOrCreateChunk();

				StoreEntity(entity, chunk);

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
			Creates a new entity by cloning an already existing one
			\return Entity
			*/
			Entity CreateEntity(Entity entity) {
				auto& entityContainer = m_entities[entity.id()];
					if (auto chunk = entityContainer.chunk) {
						auto& archetype = const_cast<Archetype&>(chunk->header.owner);

						Entity newEntity = CreateEntity(archetype);
						auto& newEntityContainer = m_entities[newEntity.id()];
						auto newChunk = newEntityContainer.chunk;

						// By adding a new entity m_entities array might have been
						// reallocated. We need to get the new address.
						auto& oldEntityContainer = m_entities[entity.id()];
						auto oldChunk = oldEntityContainer.chunk;

						// Copy generic component data from reference entity to our new
						// entity
						const auto& info =
								archetype.componentList[ComponentType::CT_Generic];
							for (uint32_t i = 0; i < info.size(); i++) {
								const auto type = info[i].type;
								if (!type->size)
									continue;

								const uint32_t idxFrom =
										info[i].offset + type->size * oldEntityContainer.idx;
								const uint32_t idxTo =
										info[i].offset + type->size * newEntityContainer.idx;

								GAIA_ASSERT(idxFrom < Chunk::DATA_SIZE);
								GAIA_ASSERT(idxTo < Chunk::DATA_SIZE);

								memcpy(
										&newChunk->data[idxTo], &oldChunk->data[idxFrom],
										type->size);
							}

						return newEntity;
				} else
					return CreateEntity();
			}

			/*!
			Removes an entity
			*/
			void DeleteEntity(Entity entity) {
				if (m_entities.empty() || entity == EntityNull)
					return;

				GAIA_ASSERT(IsEntityValid(entity));

				auto& entityContainer = m_entities[entity.id()];

					// Remove entity from chunk
					if (auto chunk = entityContainer.chunk) {
						RemoveEntity(chunk, entityContainer.idx);

						// Return entity to pool
						DeallocateEntity(entity);

						ValidateChunk(chunk);
						ValidateEntityList();
					} else {
						// Return entity to pool
						DeallocateEntity(entity);
					}
			}

			/*!
			Returns the number of active entities
			\return Entity
			*/
			[[nodiscard]] uint32_t GetEntityCount() const {
				return m_entities.size() - m_freeEntities;
			}

			/*!
			Returns a entity at a given position
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
				return entityContainer.chunk;
			}

			/*!
			Returns a chunk containing the given entity.
			Index of the entity is stored in \param indexInChunk
			\return Chunk or nullptr if not found
			*/
			[[nodiscard]] Chunk*
			GetEntityChunk(Entity entity, uint32_t& indexInChunk) const {
				GAIA_ASSERT(entity.id() < m_entities.size());
				auto& entityContainer = m_entities[entity.id()];
				indexInChunk = entityContainer.idx;
				return entityContainer.chunk;
			}

			template <typename... TComponent>
			void AddComponent(Entity entity) {
				VerifyComponents<TComponent...>();
				GAIA_ASSERT(IsEntityValid(entity));

				AddComponent_Internal<TComponent...>(ComponentType::CT_Generic, entity);
			}

			template <typename... TComponent>
			void AddComponent(Entity entity, TComponent&&... data) {
				VerifyComponents<TComponent...>();
				GAIA_ASSERT(IsEntityValid(entity));

				if (auto entityContainer = AddComponent_Internal<TComponent...>(
								ComponentType::CT_Generic, entity))
					SetComponents_Internal<TComponent...>(
							ComponentType::CT_Generic, entityContainer->chunk,
							entityContainer->idx, std::forward<TComponent>(data)...);
			}

			template <typename... TComponent>
			void AddChunkComponent(Entity entity) {
				VerifyComponents<TComponent...>();
				GAIA_ASSERT(IsEntityValid(entity));

				AddComponent_Internal<TComponent...>(ComponentType::CT_Chunk, entity);
			}

			template <typename... TComponent>
			void AddChunkComponent(Entity entity, TComponent&&... data) {
				VerifyComponents<TComponent...>();
				GAIA_ASSERT(IsEntityValid(entity));

				if (auto entityContainer = AddComponent_Internal<TComponent...>(
								ComponentType::CT_Chunk, entity))
					SetComponents_Internal<TComponent...>(
							ComponentType::CT_Chunk, entityContainer->chunk, 0,
							std::forward<TComponent>(data)...);
			}

			template <typename... TComponent>
			void RemoveComponent(Entity entity) {
				VerifyComponents<TComponent...>();
				GAIA_ASSERT(IsEntityValid(entity));

				RemoveComponent_Internal<TComponent...>(
						ComponentType::CT_Generic, entity);
			}

			template <typename... TComponent>
			void RemoveChunkComponent(Entity entity) {
				VerifyComponents<TComponent...>();
				GAIA_ASSERT(IsEntityValid(entity));

				RemoveComponent_Internal<TComponent...>(
						ComponentType::CT_Chunk, entity);
			}

			template <typename... TComponent>
			void SetComponent(Entity entity, TComponent&&... data) {
				VerifyComponents<TComponent...>();
				GAIA_ASSERT(IsEntityValid(entity));

				auto& entityContainer = m_entities[entity.id()];
				auto chunk = entityContainer.chunk;

				SetComponents_Internal<TComponent...>(
						ComponentType::CT_Generic, chunk, entityContainer.idx,
						std::forward<TComponent>(data)...);
			}

			template <typename... TComponent>
			void SetChunkComponent(Entity entity, TComponent&&... data) {
				VerifyComponents<TComponent...>();
				GAIA_ASSERT(IsEntityValid(entity));

				auto& entityContainer = m_entities[entity.id()];
				auto chunk = entityContainer.chunk;

				SetComponents_Internal<TComponent...>(
						ComponentType::CT_Chunk, chunk, 0,
						std::forward<TComponent>(data)...);
			}

			template <typename... TComponent>
			void GetComponent(Entity entity, TComponent&... data) {
				VerifyComponents<TComponent...>();
				GAIA_ASSERT(IsEntityValid(entity));

				auto& entityContainer = m_entities[entity.id()];
				auto chunk = entityContainer.chunk;

				GetComponents_Internal<TComponent...>(
						ComponentType::CT_Generic, chunk, entityContainer.idx, data...);
			}

			template <typename... TComponent>
			void GetChunkComponent(Entity entity, TComponent&... data) {
				VerifyComponents<TComponent...>();
				GAIA_ASSERT(IsEntityValid(entity));

				auto& entityContainer = m_entities[entity.id()];
				auto chunk = entityContainer.chunk;

				GetComponents_Internal<TComponent...>(
						ComponentType::CT_Chunk, chunk, 0, data...);
			}

			template <typename... TComponent>
			[[nodiscard]] bool HasComponents(Entity entity) {
				VerifyComponents<TComponent...>();
				GAIA_ASSERT(IsEntityValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
					if (const auto* chunk = entityContainer.chunk) {
						const auto& archetype = chunk->header.owner;
						return archetype.HasComponents<TComponent...>();
				}

				return false;
			}

			template <typename... TComponent>
			[[nodiscard]] bool HasAnyComponents(Entity entity) {
				VerifyComponents<TComponent...>();
				GAIA_ASSERT(IsEntityValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
					if (const auto* chunk = entityContainer.chunk) {
						const auto& archetype = chunk->header.owner;
						return archetype.HasAnyComponents<TComponent...>();
				}

				return false;
			}

			template <typename... TComponent>
			[[nodiscard]] bool HasNoneComponents(Entity entity) {
				VerifyComponents<TComponent...>();
				GAIA_ASSERT(IsEntityValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
					if (const auto* chunk = entityContainer.chunk) {
						const auto& archetype = chunk->header.owner;
						return archetype.HasNoneComponents<TComponent...>();
				}

				return false;
			}

			template <typename... TComponent>
			[[nodiscard]] bool HasChunkComponents(Entity entity) {
				VerifyComponents<TComponent...>();
				GAIA_ASSERT(IsEntityValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
					if (const auto* chunk = entityContainer.chunk) {
						const auto& archetype = chunk->header.owner;
						return archetype.HasChunkComponents<TComponent...>();
				}

				return false;
			}

			template <typename... TComponent>
			[[nodiscard]] bool HasAnyChunkComponents(Entity entity) {
				VerifyComponents<TComponent...>();
				GAIA_ASSERT(IsEntityValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
					if (const auto* chunk = entityContainer.chunk) {
						const auto& archetype = chunk->header.owner;
						return archetype.HasAnyChunkComponents<TComponent...>();
				}

				return false;
			}

			template <typename... TComponent>
			[[nodiscard]] bool HasNoneChunkComponents(Entity entity) {
				VerifyComponents<TComponent...>();
				GAIA_ASSERT(IsEntityValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
					if (const auto* chunk = entityContainer.chunk) {
						const auto& archetype = chunk->header.owner;
						return archetype.HasNoneChunkComponents<TComponent...>();
				}

				return false;
			}

		private:
			template <typename TComponent>
			void SetComponent_Internal(
					ComponentType TYPE, Chunk* chunk, uint32_t index, TComponent&& data) {
					if constexpr (!std::is_empty<TComponent>::value) {
						chunk->SetComponent_Internal<TComponent>(TYPE, index) =
								std::forward<TComponent>(data);
				}
			}

			template <typename... TComponent>
			void SetComponents_Internal(
					ComponentType TYPE, Chunk* chunk, uint32_t index,
					TComponent&&... data) {
				(SetComponent_Internal<TComponent>(
						 TYPE, chunk, index, std::forward<TComponent>(data)),
				 ...);
			}

			template <typename TComponent>
			void GetComponent_Internal(
					ComponentType TYPE, Chunk* chunk, uint32_t index,
					TComponent& data) const {
				static_assert(
						!std::is_empty<TComponent>::value,
						"Attempting to get the value of an empty component");
				data = chunk->GetComponent_Internal<TComponent>(TYPE, index);
			}

			template <typename... TComponent>
			void GetComponents_Internal(
					ComponentType TYPE, Chunk* chunk, uint32_t index,
					TComponent&... data) const {
				(GetComponent_Internal<TComponent>(TYPE, chunk, index, data), ...);
			}

			template <class T>
			struct IsReadOnlyType:
					std::bool_constant<
							std::is_const<
									std::remove_reference_t<std::remove_pointer_t<T>>>::value ||
							(!std::is_pointer<T>::value && !std::is_reference<T>::value)> {};

			template <class T, std::enable_if_t<IsReadOnlyType<T>::value>* = nullptr>
			constexpr const std::decay_t<T>* expandTuple(Chunk& chunk) const {
				return chunk.View<T>().data();
			}

			template <class T, std::enable_if_t<!IsReadOnlyType<T>::value>* = nullptr>
			constexpr std::decay_t<T>* expandTuple(Chunk& chunk) {
				return chunk.ViewRW<T>().data();
			}

			template <typename... TFuncArgs, typename TFunc>
			void ForEachEntityInChunk(Chunk& chunk, TFunc&& func) {
				auto tup = std::make_tuple(expandTuple<TFuncArgs>(chunk)...);
				for (uint16_t i = 0; i < chunk.GetItemCount(); i++)
					func(std::get<decltype(expandTuple<TFuncArgs>(chunk))>(tup)[i]...);
			}

			enum class MatchArchetypeQueryRet { Fail, Ok, Skip };

			template <ComponentType TYPE>
			[[nodiscard]] MatchArchetypeQueryRet MatchArchetypeQuery(
					const Archetype& archetype, const EntityQuery& query) {
				const uint64_t withNoneTest =
						archetype.matcherHash[TYPE] & query.list[TYPE].hashNone;
				const uint64_t withAnyTest =
						archetype.matcherHash[TYPE] & query.list[TYPE].hashAny;
				const uint64_t withAllTest =
						archetype.matcherHash[TYPE] & query.list[TYPE].hashAll;

				const auto& componentList = archetype.componentList[TYPE];

				// If withAllTest is empty but we wanted something
					if (!withAllTest && query.list[TYPE].hashAll != 0) {
						DiagNotMatching("all", componentList, query.list[TYPE].listAll);
						return MatchArchetypeQueryRet::Fail;
				}
				// If withAnyTest is empty but we wanted something
					if (!withAnyTest && query.list[TYPE].hashAny != 0) {
						DiagNotMatching("any", componentList, query.list[TYPE].listAny);
						return MatchArchetypeQueryRet::Fail;
				}

					// If there is any match with the withNoneList we quit
					if (withNoneTest != 0) {
							// withNoneList first because we usually request for less
							// components that there are components in archetype
							for (const auto type: query.list[TYPE].listNone) {
									for (const auto& component: componentList) {
											if (component.type == type) {
												DiagNotMatching(
														"none", componentList, query.list[TYPE].listNone);
												return MatchArchetypeQueryRet::Fail;
										}
									}
							}
				}

					// We need to have at least one match inside withAnyList
					if (withAnyTest != 0) {
						uint32_t matches = 0;
							// withAnyList first because we usually request for less
							// components that there are components in archetype
							for (const auto type: query.list[TYPE].listAny) {
									for (const auto& component: componentList) {
											if (component.type == type) {
												++matches;
												goto checkIncludeAnyMatches;
										}
									}
							}

					checkIncludeAnyMatches:
							// At least one match has been found to continue with
							// evaluation
							if (!matches) {
								DiagNotMatching("any", componentList, query.list[TYPE].listAny);
								return MatchArchetypeQueryRet::Fail;
						}
				}

					// If withAllList is not empty there has to be an exact match
					if (withAllTest != 0) {
							// If the number of queried components is greater than the
							// number of components in archetype there's no need to search
							if (query.list[TYPE].listAll.size() <= componentList.size()) {
								uint32_t matches = 0;

									// withAllList first because we usually request for less
									// components than there are components in archetype
									for (const auto type: query.list[TYPE].listAll) {
											for (const auto& component: componentList) {
												if (component.type != type)
													continue;

												// All requirements are fulfilled. Let's iterate
												// over all chunks in archetype
												if (++matches == query.list[TYPE].listAll.size())
													return MatchArchetypeQueryRet::Ok;

												break;
											}
									}
						}

						// No match found. We're done
						DiagNotMatching("all", componentList, query.list[TYPE].listAll);
						return MatchArchetypeQueryRet::Fail;
				}

				return (withAnyTest != 0) ? MatchArchetypeQueryRet::Ok
																	: MatchArchetypeQueryRet::Skip;
			}

			template <typename TFunc>
			void ForEachArchetype(const EntityQuery& query, TFunc&& func) {
					for (auto a: m_archetypeList) {
						const auto& archetype = *a;

						// Early exit if generic query doesn't match
						const auto retGeneric =
								MatchArchetypeQuery<ComponentType::CT_Generic>(
										archetype, query);
						if (retGeneric == MatchArchetypeQueryRet::Fail)
							continue;
						// Early exit if chunk query doesn't match
						const auto retChunk =
								MatchArchetypeQuery<ComponentType::CT_Chunk>(archetype, query);
						if (retChunk == MatchArchetypeQueryRet::Fail)
							continue;

						// If at least one query succeeded run our logic
						if (retGeneric == MatchArchetypeQueryRet::Ok ||
								retChunk == MatchArchetypeQueryRet::Ok)
							func(archetype);
					}
			}

			template <typename... TComponents, typename TFunc>
			void Unpack_ForEachEntityInChunk(
					[[maybe_unused]] utils::func_type_list<TComponents...> types,
					Chunk& chunk, TFunc&& func) {
				ForEachEntityInChunk<TComponents...>(chunk, func);
			}

			template <typename... TComponents>
			[[nodiscard]] EntityQuery& Unpack_ForEachQuery(
					[[maybe_unused]] utils::func_type_list<TComponents...> types,
					EntityQuery& query) {
				if constexpr (!sizeof...(TComponents))
					return query;
				else
					return query.All<TComponents...>();
			}

			[[nodiscard]] static bool CanProcessChunk(
					EntityQuery& query, Chunk& chunk, uint32_t lastSystemVersion) {
				// Skip empty chunks
				if (!chunk.HasEntities())
					return false;

				// Skip unchanged chunks
				const bool checkGenericComponents =
						!query.listChangeFiltered[ComponentType::CT_Generic].empty();
				const bool checkChunkComponents =
						!query.listChangeFiltered[ComponentType::CT_Chunk].empty();
					if (checkGenericComponents || checkChunkComponents) {
						bool genericChanged = false;
						bool chunkChanged = false;

							for (auto typeIdx:
									 query.listChangeFiltered[ComponentType::CT_Generic]) {
								const uint32_t componentIdx = chunk.GetComponentIdx(typeIdx);
								GAIA_ASSERT(
										componentIdx !=
										(uint32_t)-1); // the component must exist at this point!

								if (!chunk.DidChange(
												ComponentType::CT_Generic, lastSystemVersion,
												componentIdx))
									continue;

								genericChanged = true;
								break;
							}

							for (auto typeIdx:
									 query.listChangeFiltered[ComponentType::CT_Chunk]) {
								const uint32_t componentIdx =
										chunk.GetChunkComponentIdx(typeIdx);
								GAIA_ASSERT(
										componentIdx !=
										(uint32_t)-1); // the component must exist at this point!

								if (!chunk.DidChange(
												ComponentType::CT_Chunk, lastSystemVersion,
												componentIdx))
									continue;

								chunkChanged = true;
								break;
							}

						if (!genericChanged && !chunkChanged)
							return false;
				}

				return true;
			}

			template <typename TFunc>
			static void RunQueryOnChunks_Direct(
					World& world, EntityQuery& query, TFunc& func,
					uint32_t lastSystemVersion) {
				// Add an All filter for components listed as input arguments of func
				query.Commit(world.GetWorldVersion());

				// Iterate over all archetypes
				world.ForEachArchetype(query, [&](const Archetype& archetype) {
					for (auto chunk: archetype.chunks) {
						if (!CanProcessChunk(query, *chunk, lastSystemVersion))
							continue;

						func(*chunk);
					}
				});
			}

			template <typename TFunc>
			static void RunQueryOnChunks_Indirect(
					World& world, EntityQuery& query, TFunc& func,
					uint32_t lastSystemVersion) {
				using InputArgs = decltype(utils::func_args(&TFunc::operator()));

				// Add an All filter for components listed as input arguments of func
				world.Unpack_ForEachQuery(InputArgs{}, query)
						.Commit(world.GetWorldVersion());

				// Iterate over all archetypes
				world.ForEachArchetype(query, [&](const Archetype& archetype) {
					for (auto chunk: archetype.chunks) {
						if (!CanProcessChunk(query, *chunk, lastSystemVersion))
							continue;

						world.Unpack_ForEachEntityInChunk(
								InputArgs{}, *chunk, func); // Don't move func here. We need it
																						// for every further function calls
					}
				});
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
				void Run(uint32_t lastSystemVersion) {
					World::RunQueryOnChunks_Direct(
							this->world, this->query, this->func, lastSystemVersion);
				}
			};

			//--------------------------------------------------------------------------------

			template <typename TFunc, bool InternalQuery>
			struct ForEachExecutionContext {
				using TQuery =
						std::conditional_t<InternalQuery, EntityQuery, EntityQuery&>;

				World& world;
				TQuery query;
				TFunc func;

			public:
				ForEachExecutionContext(World& w, EntityQuery& q, TFunc&& f):
						world(w), query(q), func(std::forward<TFunc>(f)) {
					static_assert(
							!InternalQuery,
							"lvalue/prvalue can be used only with external queries");
				}
				ForEachExecutionContext(World& w, EntityQuery&& q, TFunc&& f):
						world(w), query(std::move(q)), func(std::forward<TFunc>(f)) {
					static_assert(
							InternalQuery, "rvalue can be used only with internal queries");
				}
				void Run(uint32_t lastSystemVersion) {
					World::RunQueryOnChunks_Indirect(
							this->world, this->query, this->func, lastSystemVersion);
				}
			};

			//--------------------------------------------------------------------------------

		public:
			template <typename TFunc>
			[[nodiscard]] ForEachChunkExecutionContext<TFunc>
			ForEachChunk(EntityQuery& query, TFunc&& func) {
				return {(World&)*this, query, std::forward<TFunc>(func)};
			}

			template <typename TFunc>
			[[nodiscard]] ForEachExecutionContext<TFunc, false>
			ForEach(EntityQuery& query, TFunc&& func) {
				return {(World&)*this, query, std::forward<TFunc>(func)};
			}

			template <typename TFunc>
			[[nodiscard]] ForEachExecutionContext<TFunc, true>
			ForEach(EntityQuery&& query, TFunc&& func) {
				return {(World&)*this, std::move(query), std::forward<TFunc>(func)};
			}

			template <typename TFunc>
			[[nodiscard]] ForEachExecutionContext<TFunc, true> ForEach(TFunc&& func) {
				return {(World&)*this, EntityQuery(), std::forward<TFunc>(func)};
			}

		private:
			//! Collect garbage
			void GC() {
					// Handle memory released by chunks and archetypes
					for (auto chunk: m_chunksToRemove) {
							// Skip chunks which were reused in the meantime
							if (chunk->HasEntities()) {
								// Reset the flag
								chunk->header.remove = false;
								continue;
						}

						GAIA_ASSERT(chunk->header.remove);
						auto& archetype = const_cast<Archetype&>(chunk->header.owner);

						// Remove the chunk if it's no longer needed
						archetype.RemoveChunk(chunk);

						// Remove the archetype if it's no longer used
						if (archetype.chunks.empty())
							RemoveArchetype(&archetype);
					}

				m_chunksToRemove.clear();
			}

			void DiagArchetypes() const {
				static bool DiagArchetypes = GAIA_ECS_DIAG_ARCHETYPES;
					if (DiagArchetypes) {
						DiagArchetypes = false;
						std::unordered_map<uint64_t, uint32_t> archetypeEntityCountMap;
						for (const auto* archetype: m_archetypeList)
							archetypeEntityCountMap.insert({archetype->lookupHash, 0});

							// Calculate the number of entities using a given archetype
							for (const auto& e: m_entities) {
								if (!e.chunk)
									continue;

								auto it = archetypeEntityCountMap.find(
										e.chunk->header.owner.lookupHash);
								if (it != archetypeEntityCountMap.end())
									++it->second;
							}

						// Print archetype info
						LOG_N("Archetypes:%u", (uint32_t)m_archetypeList.size());
							for (const auto* archetype: m_archetypeList) {
								const auto& genericComponents =
										archetype->componentList[ComponentType::CT_Generic];
								const auto& chunkComponents =
										archetype->componentList[ComponentType::CT_Chunk];
								uint32_t genericComponentsSize = 0;
								uint32_t chunkComponentsSize = 0;
								for (const auto& component: genericComponents)
									genericComponentsSize += component.type->size;
								for (const auto& component: chunkComponents)
									chunkComponentsSize += component.type->size;

								const auto it =
										archetypeEntityCountMap.find(archetype->lookupHash);
								LOG_N(
										"Archetype ID:%016llx, "
										"matcherGeneric:%016llx, "
										"matcherChunk:%016llx, "
										"chunks:%u, data size:%u B (%u + %u), "
										"entities:%u/%u",
										archetype->lookupHash,
										archetype->matcherHash[ComponentType::CT_Generic],
										archetype->matcherHash[ComponentType::CT_Chunk],
										(uint32_t)archetype->chunks.size(),
										genericComponentsSize + chunkComponentsSize,
										genericComponentsSize, chunkComponentsSize, it->second,
										archetype->capacity);

								auto logInfo = [](const ChunkComponentList& components) {
									for (const auto& component: components) {
										const auto type = component.type;
										LOG_N(
												"--> (%p) lookupHash:%016llx, matcherHash:%016llx, "
												"size:%3u "
												"B, "
												"align:%3u B, %.*s",
												(void*)type, type->lookupHash, type->matcherHash,
												type->size, type->alig, (uint32_t)type->name.length(),
												type->name.data());
									}
								};

									if (!genericComponents.empty()) {
										LOG_N(
												"Generic components - count:%u",
												(uint32_t)genericComponents.size());
										logInfo(genericComponents);
								}
									if (!chunkComponents.empty()) {
										LOG_N(
												"Chunk components - count:%u",
												(uint32_t)chunkComponents.size());
										logInfo(chunkComponents);
								}
							}
				}
			}

			void DiagRegisteredTypes() const {
				static bool DiagRegisteredTypes = GAIA_ECS_DIAG_REGISTERED_TYPES;
					if (DiagRegisteredTypes) {
						DiagRegisteredTypes = false;

						g_ComponentCache.Diag();
				}
			}

			void DiagNotMatching(
					const char* text, const ChunkComponentList& componentList,
					const EntityQuery::ComponentMetaDataArray& listToCompare) const {
				static bool DiagTypeMatching = GAIA_ECS_DIAG_TYPEMATCHING;
					if (DiagTypeMatching) {
						DiagTypeMatching = false;

						LOG_N(
								"MatchArchetypeQuery - %s, components to compare: %u", text,
								(uint32_t)componentList.size());
							for (const auto& component: componentList) {
								const auto type = component.type;
								LOG_N(
										"--> (%p) lookupHash:%016llx, matcherHash:%016llx, %.*s",
										(void*)type, type->lookupHash, type->matcherHash,
										(uint32_t)type->name.length(), type->name.data());
							}

						LOG_N("types to match: %u", (uint32_t)listToCompare.size());
							for (const auto type: listToCompare) {
								LOG_N(
										"--> (%p) lookupHash:%016llx, matcherHash:%016llx, %.*s",
										(void*)type, type->lookupHash, type->matcherHash,
										(uint32_t)type->name.length(), type->name.data());
							}
				}
			}

			void DiagEntities() const {
				static bool DiagDeletedEntities = GAIA_ECS_DIAG_DELETED_ENTITIES;
					if (DiagDeletedEntities) {
						DiagDeletedEntities = false;

						ValidateEntityList();

						LOG_N("Deleted entities: %u", m_freeEntities);
							if (m_freeEntities) {
								LOG_N("--> %u", m_nextFreeEntity);

								uint32_t iters = 0;
								auto fe = m_entities[m_nextFreeEntity].idx;
									while (fe != Entity::IdMask) {
										LOG_N("--> %u", m_entities[fe].idx);
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

			void DiagMemory() const {
				ChunkAllocatorStats memstats;
				m_chunkAllocator.GetStats(memstats);
					if (memstats.NumAllocations != 0) {
						LOG_N("ChunkAllocator stats");
						LOG_N("  Allocations: %llu", memstats.NumAllocations);
						LOG_N("  Blocks: %llu", memstats.NumBlocks);
						LOG_N("  Allocated: %llu B", memstats.Allocated);
						LOG_N("  Overhead: %llu B", memstats.Overhead);
				}
			}

		public:
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