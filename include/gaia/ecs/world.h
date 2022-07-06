#pragma once
#include <cinttypes>
#include <type_traits>

#include "../config/config.h"
#include "../containers/map.h"
#include "../containers/sarray.h"
#include "../containers/sarray_ext.h"
#include "../utils/containers.h"
#include "../utils/span.h"
#include "../utils/type_info.h"
#include "../utils/utility.h"
#include "chunk.h"
#include "chunk_allocator.h"
#include "command_buffer.h"
#include "component.h"
#include "component_cache.h"
#include "entity.h"
#include "entity_query.h"
#include "fwd.h"
#include "gaia/utils/hashing_policy.h"

namespace gaia {
	namespace ecs {

		//----------------------------------------------------------------------

		struct ComponentSetter {
			Chunk* m_pChunk;
			uint32_t m_idx;

			template <typename TComponent>
			ComponentSetter& SetComponent(typename DeduceComponent<TComponent>::Type&& data) {
				if constexpr (IsGenericComponent<TComponent>::value) {
					using U = typename detail::ExtractComponentType_Generic<TComponent>::Type;
					m_pChunk->template SetComponent<TComponent>(m_idx, std::forward<U>(data));
					return *this;
				} else {
					using U = typename detail::ExtractComponentType_Generic<TComponent>::Type;
					m_pChunk->template SetComponent<TComponent>(std::forward<U>(data));
					return *this;
				}
			}
		};

		//----------------------------------------------------------------------

		class GAIA_API World final {
			friend class ECSSystem;
			friend class ECSSystemManager;
			friend class CommandBuffer;
			friend void* AllocateChunkMemory(World& world);
			friend void ReleaseChunkMemory(World& world, void* mem);

			//! Allocator used to allocate chunks
			ChunkAllocator m_chunkAllocator;
			//! Cache of components used by the world
			ComponentCache m_componentCache;

			containers::map<utils::direct_hash_key, containers::darray<EntityQuery>> m_cachedQueries;
			//! Map or archetypes mapping to the same hash - used for lookups
			containers::map<utils::direct_hash_key, containers::darray<Archetype*>> m_archetypeMap;
			//! List of archetypes - used for iteration
			containers::darray<Archetype*> m_archetypes;
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
			ComponentCache& GetComponentCache() {
				return m_componentCache;
			}

			const ComponentCache& GetComponentCache() const {
				return m_componentCache;
			}

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
					for (auto archetype: m_archetypes)
						delete archetype;

					m_archetypes = {};
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
			\param infosGeneric Span of generic component infos
			\param infosChunk Span of chunk component infos
			\param lookupHash Archetype lookup hash
			\return Pointer to archetype or nullptr
			*/
			[[nodiscard]] Archetype* FindArchetype(
					std::span<const ComponentInfo*> infosGeneric, std::span<const ComponentInfo*> infosChunk,
					utils::direct_hash_key lookupHash) {
				// Search for the archetype in the map
				const auto it = m_archetypeMap.find(lookupHash);
				if (it == m_archetypeMap.end())
					return nullptr;

				const auto& archetypeArray = it->second;

				auto checkInfos = [&](const ComponentInfoList& list, const std::span<const ComponentInfo*>& infos) {
					for (uint32_t j = 0; j < infos.size(); j++) {
						// Different components. We need to search further
						if (list[j] != infos[j])
							return false;
					}
					return true;
				};

				// Iterate over the list of archetypes and find the exact match
				for (const auto archetype: archetypeArray) {
					const auto& genericComponentList = archetype->componentInfos[ComponentType::CT_Generic];
					if (genericComponentList.size() != infosGeneric.size())
						continue;
					const auto& chunkComponentList = archetype->componentInfos[ComponentType::CT_Chunk];
					if (chunkComponentList.size() != infosChunk.size())
						continue;

					if (checkInfos(genericComponentList, infosGeneric) && checkInfos(chunkComponentList, infosChunk))
						return archetype;
				}

				return nullptr;
			}

#if GAIA_ARCHETYPE_GRAPH
			/*!
			Creates a new archetype from a given set of components
			\param infosGeneric Span of generic component infos
			\param infosChunk Span of chunk component infos
			\return Pointer to the new archetype
			*/
			[[nodiscard]] Archetype*
			CreateArchetype(std::span<const ComponentInfo*> infosGeneric, std::span<const ComponentInfo*> infosChunk) {
				// Make sure to sort the component infos so we receive the same hash no
				// matter the order in which components are provided Bubble sort is
				// okay. We're dealing with at most MAX_COMPONENTS_PER_ARCHETYPE items.
				utils::sort(infosGeneric, [](const ComponentInfo* left, const ComponentInfo* right) {
					return left->infoIndex < right->infoIndex;
				});
				utils::sort(infosChunk, [](const ComponentInfo* left, const ComponentInfo* right) {
					return left->infoIndex < right->infoIndex;
				});

				const auto genericHash = CalculateLookupHash(infosGeneric);
				const auto chunkHash = CalculateLookupHash(infosChunk);
				const auto lookupHash = CalculateLookupHash(containers::sarray<uint64_t, 2>{genericHash, chunkHash});

				auto newArch = Archetype::Create(*this, infosGeneric, infosChunk);

				newArch->genericHash = genericHash;
				newArch->chunkHash = chunkHash;
				newArch->lookupHash = lookupHash;

				return newArch;
			}
#else
			/*!
			Creates a new archetype from a given set of components
			\param infosGeneric Span of generic component infos
			\param infosChunk Span of chunk component infos
			\return Pointer to the new archetype
			*/
			[[nodiscard]] Archetype*
			CreateArchetype(std::span<const ComponentInfo*> infosGeneric, std::span<const ComponentInfo*> infosChunk) {
				return Archetype::Create(*this, infosGeneric, infosChunk);
			}

			void
			InitArchetype(Archetype* archetype, uint64_t genericHash, uint64_t chunkHash, utils::direct_hash_key lookupHash) {
				archetype->genericHash = genericHash;
				archetype->chunkHash = chunkHash;
				archetype->lookupHash = lookupHash;
			}

			/*!
			Searches for an archetype given based on a given set of components. If no archetype is found a new one is
			created. \param infosGeneric Span of generic component infos \param infosChunk Span of chunk component infos
			\return Pointer to archetype
			*/
			[[nodiscard]] Archetype*
			FindOrCreateArchetype(std::span<const ComponentInfo*> infosGeneric, std::span<const ComponentInfo*> infosChunk) {
				// Make sure to sort the component infos so we receive the same hash no
				// matter the order in which components are provided Bubble sort is
				// okay. We're dealing with at most MAX_COMPONENTS_PER_ARCHETYPE items.
				utils::sort(infosGeneric, [](const ComponentInfo* left, const ComponentInfo* right) {
					return left->infoIndex < right->infoIndex;
				});
				utils::sort(infosChunk, [](const ComponentInfo* left, const ComponentInfo* right) {
					return left->infoIndex < right->infoIndex;
				});

				// Calculate hash for our combination of components
				const auto genericHash = CalculateLookupHash(infosGeneric);
				const auto chunkHash = CalculateLookupHash(infosChunk);
				utils::direct_hash_key lookupHash = {
						CalculateLookupHash(containers::sarray<uint64_t, 2>{genericHash, chunkHash})};

				Archetype* archetype = FindArchetype(infosGeneric, infosChunk, lookupHash);
				if (archetype == nullptr) {
					archetype = CreateArchetype(infosGeneric, infosChunk);
					InitArchetype(archetype, genericHash, chunkHash, lookupHash);
					RegisterArchetype(archetype);
				}

				return archetype;
			}
#endif

			void RegisterArchetype(Archetype* archetype) {
				// Make sure hashes were set already
				GAIA_ASSERT(newArch->genericHash != 0);
				GAIA_ASSERT(newArch->chunkHash != 0);
				GAIA_ASSERT(newArch->lookupHash != 0);

				// Make sure the archetype is not registered yet
				GAIA_ASSERT(!utils::has(m_archetypes, archetype));

				// Register the archetype
				archetype->id = (uint32_t)m_archetypes.size();
				m_archetypes.push_back(archetype);

				auto it = m_archetypeMap.find(archetype->lookupHash);
				if (it == m_archetypeMap.end()) {
					m_archetypeMap[archetype->lookupHash] = {archetype};
				} else {
					auto& archetypes = it->second;
					GAIA_ASSERT(!utils::has(archetypes, archetype));
					archetypes.push_back(archetype);
				}
			}

#if GAIA_DEBUG
			void VerifyAddComponent(Archetype& archetype, Entity entity, ComponentType type, const ComponentInfo* infoToAdd) {
				const auto& lookup = archetype.componentLookupData[type];
				const size_t oldInfosCount = lookup.size();

				// Make sure not to add too many infos
				if (!VerityArchetypeComponentCount(1)) {
					GAIA_ASSERT(false && "Trying to add too many components to entity!");
					LOG_W("Trying to add a component to entity [%u.%u] but there's no space left!", entity.id(), entity.gen());
					LOG_W("Already present:");
					for (size_t i = 0; i < oldInfosCount; i++) {
						const auto& info = m_componentCache.GetComponentCreateInfoFromIdx(lookup[i].infoIndex);
						LOG_W("> [%u] %.*s", (uint32_t)i, (uint32_t)info.name.size(), info.name.data());
					}
					LOG_W("Trying to add:");
					{
						const auto& info = m_componentCache.GetComponentCreateInfoFromIdx(infoToAdd->infoIndex);
						LOG_W("> %.*s", (uint32_t)info.name.size(), info.name.data());
					}
				}

				// Don't add the same component twice
				for (size_t i = 0; i < lookup.size(); ++i) {
					const auto& info = m_componentCache.GetComponentCreateInfoFromIdx(lookup[i].infoIndex);
					if (info.infoIndex == infoToAdd->infoIndex) {
						GAIA_ASSERT(false && "Trying to add a duplicate component");

						LOG_W(
								"Trying to add a duplicate of component %s to entity [%u.%u]", ComponentTypeString[type], entity.id(),
								entity.gen());
						LOG_W("> %.*s", (uint32_t)info.name.size(), info.name.data());
					}
				}
			}

			void VerifyRemoveComponent(
					Archetype& archetype, Entity entity, ComponentType type, const ComponentInfo* infoToRemove) {

	#if GAIA_ARCHETYPE_GRAPH
				auto ret = archetype.FindDelEdgeArchetype(type, infoToRemove);
				if (ret == nullptr) {
					GAIA_ASSERT(false && "Trying to remove a component which wasn't added");
					LOG_W("Trying to remove a component from entity [%u.%u] but it was never added", entity.id(), entity.gen());
					LOG_W("Currently present:");

					const auto& infos = archetype.componentInfos[type];
					for (size_t k = 0; k < infos.size(); k++) {
						const auto& info = m_componentCache.GetComponentCreateInfoFromIdx(infos[k]->infoIndex);
						LOG_W("> [%u] %.*s", (uint32_t)k, (uint32_t)info.name.size(), info.name.data());
					}

					{
						LOG_W("Trying to remove:");
						const auto& info = m_componentCache.GetComponentCreateInfoFromIdx(infoToRemove->infoIndex);
						LOG_W("> %.*s", (uint32_t)info.name.size(), info.name.data());
					}
				}
	#else
				const auto& infos = archetype.componentInfos[type];
				if (!utils::has_if(infos, [&](const auto* info) {
							return info == infoToRemove;
						})) {
					GAIA_ASSERT(false && "Trying to remove a component which wasn't added");
					LOG_W("Trying to remove a component from entity [%u.%u] but it was never added", entity.id(), entity.gen());
					LOG_W("Currently present:");

					for (size_t k = 0; k < infos.size(); k++) {
						const auto& info = m_componentCache.GetComponentCreateInfoFromIdx(infos[k]->infoIndex);
						LOG_W("> [%u] %.*s", (uint32_t)k, (uint32_t)info.name.size(), info.name.data());
					}

					{
						LOG_W("Trying to remove:");
						const auto& info = m_componentCache.GetComponentCreateInfoFromIdx(infoToRemove->infoIndex);
						LOG_W("> %.*s", (uint32_t)info.name.size(), info.name.data());
					}
				}
	#endif
			}
#endif

#if GAIA_ARCHETYPE_GRAPH
			GAIA_FORCEINLINE void
			BuildGraphEdges(ComponentType type, Archetype* left, Archetype* right, const ComponentInfo* info) {
				left->edgesAdd[type].push_back({info, right});
				right->edgesDel[type].push_back({info, left});
			}

			/*!
			Checks if archetype \param left is a superset of archetype \param right (contains all its infos)
			\param left Entity to delete
			\param right Entity to delete
			\return Returns true if left is a superset of right
			*/
			bool IsSuperSet(ComponentType type, Archetype& left, Archetype& right) {
				size_t i = 0;
				size_t j = 0;

				const auto& infosLeft = left.componentInfos[type];
				const auto& infosRight = right.componentInfos[type];
				if (infosLeft.size() < infosRight.size())
					return false;

				// Arrays are sorted so we can do linear intersection lookup
				while (i < infosLeft.size() && j < infosRight.size()) {
					const auto* infoLeft = infosLeft[i];
					const auto* infoRight = infosRight[j];

					if (infoLeft == infoRight) {
						++i;
						++j;
					} else if (infoLeft->infoIndex < infoRight->infoIndex)
						++i;
					else
						return false;
				}

				return j == infosRight.size();
			}
#endif

			/*!
			Searches for an archetype based on the given set of components. If no archetype is found a new one is created.
			\param oldArchetype Original archetype
			\param type Component infos
			\param infoToAdd Span of chunk components
			\return Pointer to archetype
			*/
			[[nodiscard]] Archetype*
			FindOrCreateArchetype(Archetype* oldArchetype, ComponentType type, const ComponentInfo* infoToAdd) {
				auto* node = oldArchetype;

#if GAIA_ARCHETYPE_GRAPH
				// We don't want to store edges for the root archetype because the more components there are the longer
				// it would take to find anything. Therefore, for the root archetype we simply make a lookup.
				if (node == m_rootArchetype) {
					if (type == ComponentType::CT_Generic) {
						const auto genericHash = infoToAdd->lookupHash;
						const auto lookupHash = CalculateLookupHash(containers::sarray<uint64_t, 2>{genericHash, 0});
						node = FindArchetype(std::span<const ComponentInfo*>(&infoToAdd, 1), {}, lookupHash);
						if (node == nullptr) {
							node = CreateArchetype(std::span<const ComponentInfo*>(&infoToAdd, 1), {});
							RegisterArchetype(node);
							node->edgesDel[type].push_back({infoToAdd, m_rootArchetype});
						}
					} else {
						const auto chunkHash = infoToAdd->lookupHash;
						const auto lookupHash = CalculateLookupHash(containers::sarray<uint64_t, 2>{0, chunkHash});
						node = FindArchetype({}, std::span<const ComponentInfo*>(&infoToAdd, 1), lookupHash);
						if (node == nullptr) {
							node = CreateArchetype({}, std::span<const ComponentInfo*>(&infoToAdd, 1));
							RegisterArchetype(node);
							node->edgesDel[type].push_back({infoToAdd, m_rootArchetype});
						}
					}
				}

				const auto it = utils::find_if(node->edgesAdd[type], [infoToAdd](const auto& edge) {
					return edge.info == infoToAdd;
				});

				// Not found among edges, create a new archetype
				if (it == node->edgesAdd[type].end())
#endif
				{
					const auto& archetype = *node;

					const auto& componentInfos = archetype.componentInfos[type];
					const auto& componentInfos2 = archetype.componentInfos[(type + 1) & 1];

					// Prepare a joint array of component infos of old + the newly added component
					containers::sarray_ext<const ComponentInfo*, MAX_COMPONENTS_PER_ARCHETYPE> newInfos;
					{
						const size_t newSize = componentInfos.size();
						newInfos.resize(newSize + 1);

						size_t j = 0;
						for (; j < newSize; ++j)
							newInfos[j] = componentInfos[j];
						newInfos[j] = infoToAdd;
					}

#if GAIA_ARCHETYPE_GRAPH
					auto newArchetype =
							type == ComponentType::CT_Generic
									? CreateArchetype(
												{newInfos.data(), newInfos.size()}, {componentInfos2.data(), componentInfos2.size()})
									: CreateArchetype(
												{componentInfos2.data(), componentInfos2.size()}, {newInfos.data(), newInfos.size()});

					RegisterArchetype(newArchetype);
					BuildGraphEdges(type, node, newArchetype, infoToAdd);
#else
					// Prepare an array of old component infos for our other type. This is a simple copy.
					containers::sarray_ext<const ComponentInfo*, MAX_COMPONENTS_PER_ARCHETYPE> otherInfos;
					{
						const size_t otherSize = componentInfos2.size();
						otherInfos.resize(otherSize);

						for (size_t j = 0; j < otherSize; ++j)
							otherInfos[j] = componentInfos2[j];
					}

					auto newArchetype =
							type == ComponentType::CT_Generic
									? FindOrCreateArchetype({newInfos.data(), newInfos.size()}, {otherInfos.data(), otherInfos.size()})
									: FindOrCreateArchetype({otherInfos.data(), otherInfos.size()}, {newInfos.data(), newInfos.size()});
#endif

					node = newArchetype;
#if GAIA_ARCHETYPE_GRAPH
				} else {
					node = it->archetype;

#endif
				}

				return node;
			}

			/*!
			Searches for a parent archetype that contains the given component of \param type.
			\param archetype Archetype to search from
			\param type Component type
			\param typesToRemove Span of component infos we want to remove
			\return Pointer to archetype
			*/
			[[nodiscard]] Archetype*
			FindArchetype_RemoveComponents(Archetype* archetype, ComponentType type, const ComponentInfo* intoToRemove) {
#if GAIA_ARCHETYPE_GRAPH
				// Follow the graph to the next archetype
				auto* node = archetype->FindDelEdgeArchetype(type, intoToRemove);
				GAIA_ASSERT(node != nullptr);
				return node;
#else
				const auto& infosFirst = archetype->componentInfos[type];
				const auto& infosOther = archetype->componentInfos[(type + 1) & 1];

				containers::sarray_ext<const ComponentInfo*, MAX_COMPONENTS_PER_ARCHETYPE> infosNew;
				{
					// Find the intersection
					for (const auto* info: infosFirst) {
						if (info == intoToRemove)
							goto nextIter;

						infosNew.push_back(info);

					nextIter:
						continue;
					}

					// Return if there's no change
					if (infosNew.size() == infosFirst.size())
						return nullptr;
				}

				auto newArchetype =
						type == ComponentType::CT_Generic
								? FindOrCreateArchetype({infosNew.data(), infosNew.size()}, {infosOther.data(), infosOther.size()})
								: FindOrCreateArchetype({infosOther.data(), infosOther.size()}, {infosNew.data(), infosNew.size()});

				return newArchetype;
#endif
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
			\return Pointer to archetype
			*/
			[[nodiscard]] Archetype* GetArchetype(Entity entity) const {
				GAIA_ASSERT(IsEntityValid(entity));

				auto& entityContainer = m_entities[entity.id()];
				auto* pChunk = entityContainer.pChunk;
				return pChunk ? const_cast<Archetype*>(&pChunk->header.owner) : nullptr;
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
				const auto gen = ++entityContainer.gen;

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
				entityContainer.gen = entity.gen();
			}

			/*!
			Moves an entity along with all its generic components from its current to another
			chunk in a new archetype.
			\param oldEntity Entity to move
			\param newArchetype Target archetype
			*/
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
				const auto& oldInfos = oldArchetype.componentInfos[ComponentType::CT_Generic];
				const auto& newInfos = newArchetype.componentInfos[ComponentType::CT_Generic];
				const auto& oldLook = oldArchetype.componentLookupData[ComponentType::CT_Generic];
				const auto& newLook = newArchetype.componentLookupData[ComponentType::CT_Generic];

				// Arrays are sorted so we can do linear intersection lookup
				{
					size_t i = 0;
					size_t j = 0;
					while (i < oldInfos.size() && j < newInfos.size()) {
						const auto* infoOld = oldInfos[i];
						const auto* infoNew = newInfos[j];

						if (infoOld == infoNew) {
							// Let's move all type data from oldEntity to newEntity
							const auto idxFrom = oldLook[i++].offset + infoOld->properties.size * oldIndex;
							const auto idxTo = newLook[j++].offset + infoOld->properties.size * newIndex;

							GAIA_ASSERT(idxFrom < Chunk::DATA_SIZE_NORESERVE);
							GAIA_ASSERT(idxTo < Chunk::DATA_SIZE_NORESERVE);

							memcpy(&newChunk->data[idxTo], &oldChunk->data[idxFrom], infoOld->properties.size);
						} else if (infoOld->infoIndex > infoNew->infoIndex)
							++j;
						else
							++i;
					}
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

			EntityContainer& AddComponent_Internal(ComponentType type, Entity entity, const ComponentInfo* infoToAdd) {
				auto& entityContainer = m_entities[entity.id()];

				// Adding a component to an entity which already is a part of some chunk
				if (auto* pChunk = entityContainer.pChunk) {
					auto& archetype = const_cast<Archetype&>(pChunk->header.owner);

					GAIA_ASSERT(
							!archetype.info.structuralChangesLocked && "New components can't be added while chunk is being iterated "
																												 "(structural changes are forbidden during this time!)");
#if GAIA_DEBUG
					VerifyAddComponent(archetype, entity, type, infoToAdd);
#endif

					auto* newArchetype = FindOrCreateArchetype(&archetype, type, infoToAdd);
					MoveEntity(entity, *newArchetype);
				}
				// Adding a component to an empty entity
				else {
					auto& archetype = const_cast<Archetype&>(*m_rootArchetype);

					GAIA_ASSERT(
							!archetype.info.structuralChangesLocked && "New components can't be added while chunk is being iterated "
																												 "(structural changes are forbidden during this time!)");
#if GAIA_DEBUG
					VerifyAddComponent(archetype, entity, type, infoToAdd);
#endif

					auto* newArchetype = FindOrCreateArchetype(&archetype, type, infoToAdd);
					StoreEntity(entity, newArchetype->FindOrCreateFreeChunk());
				}

				return entityContainer;
			}

			ComponentSetter RemoveComponent_Internal(ComponentType type, Entity entity, const ComponentInfo* infoToRemove) {
				auto& entityContainer = m_entities[entity.id()];
				auto* pChunk = entityContainer.pChunk;
				auto& archetype = const_cast<Archetype&>(pChunk->header.owner);

				GAIA_ASSERT(
						!archetype.info.structuralChangesLocked && "Components can't be removed while chunk is being iterated "
																											 "(structural changes are forbidden during this time!)");
#if GAIA_DEBUG
				VerifyRemoveComponent(archetype, entity, type, infoToRemove);
#endif

#if GAIA_ARCHETYPE_GRAPH
				auto newArchetype = FindArchetype_RemoveComponents(&archetype, type, infoToRemove);
				MoveEntity(entity, *newArchetype);
#else
				if (auto newArchetype = FindArchetype_RemoveComponents(&archetype, type, infoToRemove))
					MoveEntity(entity, *newArchetype);
#endif

				return ComponentSetter{pChunk, entityContainer.idx};
			}

			void Init() {
				m_rootArchetype = CreateArchetype({}, {});
				RegisterArchetype(m_rootArchetype);
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

			//----------------------------------------------------------------------

			/*!
			Creates a new empty entity
			\return Entity
			*/
			[[nodiscard]] Entity CreateEntity() {
				return AllocateEntity();
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

					const auto newEntity = CreateEntity(archetype);
					auto& newEntityContainer = m_entities[newEntity.id()];
					auto newChunk = newEntityContainer.pChunk;

					// By adding a new entity m_entities array might have been reallocated.
					// We need to get the new address.
					auto& oldEntityContainer = m_entities[entity.id()];
					auto oldChunk = oldEntityContainer.pChunk;

					// Copy generic component data from reference entity to our new ntity
					const auto& infos = archetype.componentInfos[ComponentType::CT_Generic];
					const auto& looks = archetype.componentLookupData[ComponentType::CT_Generic];

					for (size_t i = 0; i < infos.size(); i++) {
						const auto* info = infos[i];
						if (!info->properties.size)
							continue;

						const auto offset = looks[i].offset;
						const auto idxFrom = offset + info->properties.size * oldEntityContainer.idx;
						const auto idxTo = offset + info->properties.size * newEntityContainer.idx;

						GAIA_ASSERT(idxFrom < Chunk::DATA_SIZE_NORESERVE);
						GAIA_ASSERT(idxTo < Chunk::DATA_SIZE_NORESERVE);

						memcpy(&newChunk->data[idxTo], &oldChunk->data[idxFrom], info->properties.size);
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
						const auto& infos = archetype.componentInfos[ComponentType::CT_Generic];
						const auto& looks = archetype.componentLookupData[ComponentType::CT_Generic];

						for (size_t i = 0; i < infos.size(); i++) {
							const auto* info = infos[i];
							if (!info->properties.size)
								continue;

							const auto offset = looks[i].offset;
							const auto idxFrom = offset + info->properties.size * entityContainer.idx;
							const auto idxTo = offset + info->properties.size * idxNew;

							GAIA_ASSERT(idxFrom < Chunk::DATA_SIZE_NORESERVE);
							GAIA_ASSERT(idxTo < Chunk::DATA_SIZE_NORESERVE);

							memcpy(&pChunkTo->data[idxTo], &pChunkFrom->data[idxFrom], info->properties.size);
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
			[[nodiscard]] Chunk* GetChunk(Entity entity) const {
				GAIA_ASSERT(entity.id() < m_entities.size());
				auto& entityContainer = m_entities[entity.id()];
				return entityContainer.pChunk;
			}

			/*!
			Returns a chunk containing the given entity.
			Index of the entity is stored in \param indexInChunk
			\return Chunk or nullptr if not found
			*/
			[[nodiscard]] Chunk* GetChunk(Entity entity, uint32_t& indexInChunk) const {
				GAIA_ASSERT(entity.id() < m_entities.size());
				auto& entityContainer = m_entities[entity.id()];
				indexInChunk = entityContainer.idx;
				return entityContainer.pChunk;
			}

			//----------------------------------------------------------------------

			/*!
			Attaches a new component to \param entity.
			\warning It is expected the component is not there yet and that \param
			entity is valid. Undefined behavior otherwise.
			\return ComponentSetter object.
			*/
			template <typename TComponent>
			ComponentSetter AddComponent(Entity entity) {
				VerifyComponent<TComponent>();
				GAIA_ASSERT(IsEntityValid(entity));

				if constexpr (IsGenericComponent<TComponent>::value) {
					using U = typename detail::ExtractComponentType_Generic<TComponent>::Type;
					const auto* info = m_componentCache.GetOrCreateComponentInfo<U>();
					auto& entityContainer = AddComponent_Internal(ComponentType::CT_Generic, entity, info);
					return ComponentSetter{entityContainer.pChunk, entityContainer.idx};
				} else {
					using U = typename detail::ExtractComponentType_NonGeneric<TComponent>::Type;
					const auto* info = m_componentCache.GetOrCreateComponentInfo<U>();
					auto& entityContainer = AddComponent_Internal(ComponentType::CT_Chunk, entity, info);
					return ComponentSetter{entityContainer.pChunk, entityContainer.idx};
				}
			}

			/*!
			Attaches a component to \param entity. Also sets its value.
			\warning It is expected the component is not there yet and that
			\param entity is valid. Undefined behavior otherwise.
			\return ComponentSetter object.
			*/
			template <typename TComponent>
			ComponentSetter AddComponent(Entity entity, typename DeduceComponent<TComponent>::Type&& data) {
				VerifyComponent<TComponent>();
				GAIA_ASSERT(IsEntityValid(entity));

				if constexpr (IsGenericComponent<TComponent>::value) {
					using U = typename detail::ExtractComponentType_Generic<TComponent>::Type;
					const auto* info = m_componentCache.GetOrCreateComponentInfo<U>();
					auto& entityContainer = AddComponent_Internal(ComponentType::CT_Generic, entity, info);
					auto* pChunk = entityContainer.pChunk;
					pChunk->template SetComponent<TComponent>(entityContainer.idx, std::forward<U>(data));
					return ComponentSetter{entityContainer.pChunk, entityContainer.idx};
				} else {
					using U = typename detail::ExtractComponentType_NonGeneric<TComponent>::Type;
					const auto* info = m_componentCache.GetOrCreateComponentInfo<U>();
					auto& entityContainer = AddComponent_Internal(ComponentType::CT_Chunk, entity, info);
					auto* pChunk = entityContainer.pChunk;
					pChunk->template SetComponent<TComponent>(std::forward<U>(data));
					return ComponentSetter{entityContainer.pChunk, entityContainer.idx};
				}
			}

			/*!
			Removes a component from \param entity.
			\warning It is expected the component is not there yet and that
			\param entity is valid. Undefined behavior otherwise.
			\return ComponentSetter object.
			*/
			template <typename TComponent>
			ComponentSetter RemoveComponent(Entity entity) {
				VerifyComponent<TComponent>();
				GAIA_ASSERT(IsEntityValid(entity));

				if constexpr (IsGenericComponent<TComponent>::value) {
					using U = typename detail::ExtractComponentType_Generic<TComponent>::Type;
					const auto* info = m_componentCache.GetOrCreateComponentInfo<U>();
					return RemoveComponent_Internal(ComponentType::CT_Generic, entity, info);
				} else {
					using U = typename detail::ExtractComponentType_NonGeneric<TComponent>::Type;
					const auto* info = m_componentCache.GetOrCreateComponentInfo<U>();
					return RemoveComponent_Internal(ComponentType::CT_Chunk, entity, info);
				}
			}

			/*!
			Sets the value of component on \param entity.
			\warning It is expected the component was added to \param entity already. Undefined behavior otherwise.
			\param entity is valid. Undefined behavior otherwise.
			\return ComponentSetter object.
			*/
			template <typename TComponent>
			ComponentSetter SetComponent(Entity entity, typename DeduceComponent<TComponent>::Type&& data) {
				VerifyComponent<TComponent>();
				GAIA_ASSERT(IsEntityValid(entity));

				auto& entityContainer = m_entities[entity.id()];
				return ComponentSetter{entityContainer.pChunk, entityContainer.idx}.SetComponent<TComponent>(
						std::forward<typename DeduceComponent<TComponent>::Type>(data));
			}

			/*!
			Returns the value stored in the component on \param entity.
			\warning It is expected the component was added to \param entity already. Undefined behavior otherwise.
			\return Value stored in the component.
			*/
			template <typename TComponent>
			auto GetComponent(Entity entity) const {
				VerifyComponent<TComponent>();
				GAIA_ASSERT(IsEntityValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				const auto* pChunk = entityContainer.pChunk;

				if constexpr (IsGenericComponent<TComponent>::value)
					return pChunk->GetComponent<TComponent>(entityContainer.idx);
				else
					return pChunk->GetComponent<TComponent>();
			}

			//----------------------------------------------------------------------

			/*!
			Tells if \param entity contains the component.
			\return True if the component is present on entity.
			*/
			template <typename TComponent>
			[[nodiscard]] bool HasComponent(Entity entity) {
				VerifyComponent<TComponent>();
				GAIA_ASSERT(IsEntityValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				if (const auto* pChunk = entityContainer.pChunk)
					return pChunk->HasComponent<TComponent>();

				return false;
			}

			//----------------------------------------------------------------------

		private:
			template <typename T>
			constexpr GAIA_FORCEINLINE auto GetComponentView(Chunk& chunk) const {
				using U = typename DeduceComponent<T>::Type;
				using UOriginal = typename DeduceComponent<T>::TypeOriginal;
				if constexpr (IsReadOnlyType<UOriginal>::value)
					return chunk.View_Internal<U>();
				else
					return chunk.ViewRW_Internal<U>();
			}

			//--------------------------------------------------------------------------------

			template <typename... T, typename Func>
			GAIA_FORCEINLINE void
			ForEachEntityInChunk([[maybe_unused]] utils::func_type_list<T...> types, Chunk& chunk, Func func) {
				// Pointers to the respective component types in the chunk, e.g
				// 		w.ForEach(q, [&](Position& p, const Velocity& v) {...}
				// Translates to:
				//  	auto p = chunk.ViewRW_Internal<Position>();
				//		auto v = chunk.View_Internal<Velocity>();
				auto dataPointerTuple = std::make_tuple(GetComponentView<T>(chunk)...);

				// Iterate over each entity in the chunk.
				// Translates to:
				//		for (size_t i = 0; i < chunk.GetItemCount(); ++i)
				//			func(p[i], v[i]);
				const size_t size = chunk.GetItemCount();
				for (size_t i = 0; i < size; ++i)
					func(std::get<decltype(GetComponentView<T>(chunk))>(dataPointerTuple)[i]...);
			}

			template <typename Func>
			GAIA_FORCEINLINE void ForEachArchetype(EntityQuery& query, Func func) {
				query.Match(m_archetypes);
				for (auto* pArchetype: query)
					func(*pArchetype);
			}

			//--------------------------------------------------------------------------------

			template <typename... T>
			void UnpackArgsIntoQuery([[maybe_unused]] utils::func_type_list<T...> types, EntityQuery& query) const {
				if constexpr (sizeof...(T) > 0)
					query.All<T...>();
			}

			template <typename... T>
			bool UnpackArgsIntoQuery_Check([[maybe_unused]] utils::func_type_list<T...> types, EntityQuery& query) const {
				if constexpr (sizeof...(T) > 0)
					return query.HasAll<T...>();
			}

			template <typename Func>
			static void ResolveQuery(World& world, EntityQuery& query) {
				using InputArgs = decltype(utils::func_args(&Func::operator()));
				world.UnpackArgsIntoQuery(InputArgs{}, query);
			}

			template <typename Func>
			static bool CheckQuery(World& world, EntityQuery& query) {
				using InputArgs = decltype(utils::func_args(&Func::operator()));
				return world.UnpackArgsIntoQuery_Check(InputArgs{}, query);
			}

			//--------------------------------------------------------------------------------

			[[nodiscard]] static bool CheckFilters(const EntityQuery& query, const Chunk& chunk) {
				GAIA_ASSERT(chunk.HasEntities() && "CheckFilters called on an empty chunk");

				const auto lastWorldVersion = query.GetWorldVersion();

				// See if any generic component has changed
				{
					const auto& filtered = query.GetFiltered(ComponentType::CT_Generic);
					for (auto infoIndex: filtered) {
						const uint32_t componentIdx = chunk.GetComponentIdx(ComponentType::CT_Generic, infoIndex);
						if (chunk.DidChange(ComponentType::CT_Generic, lastWorldVersion, componentIdx))
							return true;
					}
				}

				// See if any chunk component has changed
				{
					const auto& filtered = query.GetFiltered(ComponentType::CT_Chunk);
					for (auto infoIndex: filtered) {
						const uint32_t componentIdx = chunk.GetComponentIdx(ComponentType::CT_Chunk, infoIndex);
						if (chunk.DidChange(ComponentType::CT_Chunk, lastWorldVersion, componentIdx))
							return true;
					}
				}

				// Skip unchanged chunks.
				return false;
			}

			//--------------------------------------------------------------------------------

			template <typename Func>
			static void RunQueryOnChunks_Direct(World& world, EntityQuery& query, Func func) {
				constexpr size_t BatchSize = 256U;
				containers::sarray<Chunk*, BatchSize> tmp;

				// Update the world version
				world.UpdateWorldVersion();

				const bool hasFilters = query.HasFilters();

				// Iterate over all archetypes
				world.ForEachArchetype(query, [&](Archetype& archetype) {
					GAIA_ASSERT(archetype.info.structuralChangesLocked < 8);
					++archetype.info.structuralChangesLocked;

					auto exec = [&](const auto& chunksList) {
						size_t chunkOffset = 0;
						size_t batchSize = 0;
						const size_t maxIters = chunksList.size();
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
							const size_t size = batchSize;
							for (size_t chunkIdx = 0; chunkIdx < size; ++chunkIdx)
								func(*tmp[chunkIdx]);

							// Reset the batch size
							batchSize = 0;
						} while (chunkOffset < maxIters);
					};

					if (query.CheckConstraints(true))
						exec(archetype.chunks);
					if (query.CheckConstraints(false))
						exec(archetype.chunksDisabled);

					GAIA_ASSERT(archetype.info.structuralChangesLocked > 0);
					--archetype.info.structuralChangesLocked;
				});

				query.SetWorldVersion(world.GetWorldVersion());
			}

			template <typename Func>
			static void RunQueryOnChunks_Indirect_NoResolve(World& world, EntityQuery& query, Func func) {
				using InputArgs = decltype(utils::func_args(&Func::operator()));

				constexpr size_t BatchSize = 256U;
				containers::sarray<Chunk*, BatchSize> tmp;

				// Update the world version
				world.UpdateWorldVersion();

				const bool hasFilters = query.HasFilters();

				// Iterate over all archetypes
				world.ForEachArchetype(query, [&](Archetype& archetype) {
					GAIA_ASSERT(archetype.info.structuralChangesLocked < 8);
					++archetype.info.structuralChangesLocked;

					auto exec = [&](const auto& chunksList) {
						size_t chunkOffset = 0;
						size_t batchSize = 0;
						const size_t maxIters = chunksList.size();
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
							const size_t size = batchSize;
							for (size_t chunkIdx = 0; chunkIdx < size; ++chunkIdx)
								world.ForEachEntityInChunk(InputArgs{}, *tmp[chunkIdx], func);

							// Reset the batch size
							batchSize = 0;
						} while (chunkOffset < maxIters);
					};

					if (query.CheckConstraints(true))
						exec(archetype.chunks);
					if (query.CheckConstraints(false))
						exec(archetype.chunksDisabled);

					GAIA_ASSERT(archetype.info.structuralChangesLocked > 0);
					--archetype.info.structuralChangesLocked;
				});

				query.SetWorldVersion(world.GetWorldVersion());
			}

			template <typename Func>
			static void RunQueryOnChunks_Indirect(World& world, EntityQuery& query, Func func) {
#if GAIA_DEBUG
				// Make sure we only use components specificed in the query
				GAIA_ASSERT(CheckQuery<Func>(world, query));
#endif
				RunQueryOnChunks_Indirect_NoResolve(world, query, func);
			}

			//--------------------------------------------------------------------------------

			template <typename... TComponent>
			static void
			RegisterComponents_Internal([[maybe_unused]] utils::func_type_list<TComponent...> types, World& world) {
				static_assert(sizeof...(TComponent) > 0, "Empty EntityQuery is not supported in this context");
				auto& cc = world.GetComponentCache();
				((void)cc.GetOrCreateComponentInfo<TComponent>(), ...);
			}

			template <typename Func>
			static void RegisterComponents(World& world) {
				using InputArgs = decltype(utils::func_args(&Func::operator()));
				RegisterComponents_Internal(InputArgs{}, world);
			}

			//--------------------------------------------------------------------------------

			EntityQuery& AddOrFindEntityQueryInCache(World& world, EntityQuery& queryTmp) {
				EntityQuery* query = nullptr;

				auto it = world.m_cachedQueries.find(queryTmp.m_hashLookup);
				if (it == world.m_cachedQueries.end()) {
					const auto hash = queryTmp.m_hashLookup;
					world.m_cachedQueries[hash] = {std::move(queryTmp)};
					query = &world.m_cachedQueries[hash].back();
				} else {
					auto& queries = it->second;

					// Make sure the same hash gets us to the proper query
					for (const auto& q: queries) {
						if (q != queryTmp)
							continue;
						query = &queries.back();
						return *query;
					}

					queries.push_back(std::move(queryTmp));
					query = &queries.back();
				}

				return *query;
			}

			//--------------------------------------------------------------------------------

			template <typename Func>
			void ForEachChunk_External(World& world, EntityQuery& query, Func func) {
				RunQueryOnChunks_Direct(world, query, func);
			}

			template <typename Func>
			void ForEachChunk_Internal(World& world, EntityQuery&& queryTmp, Func func) {
				RegisterComponents<Func>(world);
				queryTmp.CalculateLookupHash(world);
				RunQueryOnChunks_Direct(world, AddOrFindEntityQueryInCache(world, queryTmp), func);
			}

			//--------------------------------------------------------------------------------

			template <typename Func>
			void ForEach_External(World& world, EntityQuery& query, Func func) {
				RunQueryOnChunks_Indirect(world, query, func);
			}

			template <typename Func>
			void ForEach_Internal(World& world, EntityQuery&& queryTmp, Func func) {
				RegisterComponents<Func>(world);
				queryTmp.CalculateLookupHash(world);
				RunQueryOnChunks_Indirect_NoResolve(world, AddOrFindEntityQueryInCache(world, queryTmp), func);
			}

			//--------------------------------------------------------------------------------

		public:
			/*!
			Iterates over all chunks satisfying conditions set by \param query and calls \param func for all of them.
			\warning Iterating using ecs::Chunk makes it possible to perform optimizations otherwise not possible with
							other methods of iteration as it exposes the chunk itself. On the other hand, it is more verbose
							and takes more lines of code when used.
			*/
			template <typename Func>
			void ForEach(EntityQuery& query, Func func) {
				if constexpr (std::is_invocable<Func, Chunk&>::value)
					ForEachChunk_External((World&)*this, query, func);
				else
					ForEach_External((World&)*this, query, func);
			}

			/*!
			Iterates over all chunks satisfying conditions set by \param query and calls \param func for all of them.
			\warning Iterating using ecs::Chunk makes it possible to perform optimizations otherwise not possible with
							other methods of iteration as it exposes the chunk itself. On the other hand, it is more verbose
							and takes more lines of code when used.
			*/
			template <typename Func>
			void ForEach(EntityQuery&& query, Func func) {
				if constexpr (std::is_invocable<Func, Chunk&>::value)
					ForEachChunk_Internal((World&)*this, std::forward<EntityQuery>(query), func);
				else
					ForEach_Internal((World&)*this, std::forward<EntityQuery>(query), func);
			}

			/*!
			Iterates over all chunks satisfying conditions set by \param func and calls \param func for all of them.
			EntityQuery instance is generated internally from the input arguments of \param func.
			\warning Performance-wise it has less potential than iterating using ecs::Chunk or a comparable ForEach passing
							in a query because it needs to do cached query lookups on each invocation. However, it is easier to use
							and for non-critical code paths it is the most elegant way of iterating your data.
			*/
			template <typename Func>
			void ForEach(Func func) {
				static_assert(
						!std::is_invocable<Func, Chunk&>::value, "Calling query-less ForEach is not supported for chunk iteration");

				EntityQuery query;
				ResolveQuery<Func>((World&)*this, query);
				ForEach_Internal<Func>((World&)*this, std::move(query), func);
			}

			/*!
			Collect garbage. Check all chunks and archetypes which are empty and have not been used for a while
			and tries to delete them and release memory allocated by them.
			*/
			void GC() {
				// Handle chunks
				for (size_t i = 0; i < m_chunksToRemove.size();) {
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
				for (auto* pChunk: m_chunksToRemove)
					const_cast<Archetype&>(pChunk->header.owner).RemoveChunk(pChunk);
				m_chunksToRemove.clear();
			}

			/*!
			Performs diagnostics on a specific archetype. Prints basic info about it and the chunks it contains.
			\param archetype Archetype to run diagnostics on
			*/
			void DiagArchetype(const Archetype& archetype) const {
				static bool DiagArchetypes = GAIA_ECS_DIAG_ARCHETYPES;
				if (!DiagArchetypes)
					return;
				DiagArchetypes = false;

				// Caclulate the number of entites in archetype
				uint32_t entityCount = 0;
				uint32_t entityCountDisabled = 0;
				for (const auto* chunk: archetype.chunks)
					entityCount += chunk->GetItemCount();
				for (const auto* chunk: archetype.chunksDisabled) {
					entityCountDisabled += chunk->GetItemCount();
					entityCount += chunk->GetItemCount();
				}

				// Print archetype info
				{
					const auto& genericComponents = archetype.componentInfos[ComponentType::CT_Generic];
					const auto& chunkComponents = archetype.componentInfos[ComponentType::CT_Chunk];
					uint32_t genericComponentsSize = 0;
					uint32_t chunkComponentsSize = 0;
					for (const auto& component: genericComponents)
						genericComponentsSize += component->properties.size;
					for (const auto& component: chunkComponents)
						chunkComponentsSize += component->properties.size;

					LOG_N(
							"Archetype ID:%u, "
							"lookupHash:%016" PRIx64 ", "
							"mask:%016" PRIx64 "/%016" PRIx64 ", "
							"chunks:%u, data size:%3u B (%u/%u), "
							"entities:%u/%u (disabled:%u)",
							archetype.id, archetype.lookupHash.hash, archetype.matcherHash[ComponentType::CT_Generic],
							archetype.matcherHash[ComponentType::CT_Chunk], (uint32_t)archetype.chunks.size(),
							genericComponentsSize + chunkComponentsSize, genericComponentsSize, chunkComponentsSize, entityCount,
							archetype.info.capacity, entityCountDisabled);

					auto logComponentInfo = [](const ComponentInfo* info, const ComponentInfoCreate& infoStatic) {
						LOG_N(
								"    (%p) lookupHash:%016" PRIx64 ", mask:%016" PRIx64 ", size:%3u B, align:%3u B, %.*s", (void*)info,
								info->lookupHash, info->matcherHash, info->properties.size, info->properties.alig,
								(uint32_t)infoStatic.name.size(), infoStatic.name.data());
					};

					if (!genericComponents.empty()) {
						LOG_N("  Generic components - count:%u", (uint32_t)genericComponents.size());
						for (const auto* component: genericComponents)
							logComponentInfo(component, m_componentCache.GetComponentCreateInfoFromIdx(component->infoIndex));
					}
					if (!chunkComponents.empty()) {
						LOG_N("  Chunk components - count:%u", (uint32_t)chunkComponents.size());
						for (const auto* component: chunkComponents)
							logComponentInfo(component, m_componentCache.GetComponentCreateInfoFromIdx(component->infoIndex));
					}

#if GAIA_ARCHETYPE_GRAPH
					{
						const auto& edgesG = archetype.edgesAdd[ComponentType::CT_Generic];
						const auto& edgesC = archetype.edgesAdd[ComponentType::CT_Chunk];
						const auto edgeCount = (uint32_t)(edgesG.size() + edgesC.size());
						if (edgeCount > 0) {
							LOG_N("  Add edges - count:%u", edgeCount);

							if (!edgesG.empty()) {
								LOG_N("    Generic - count:%u", (uint32_t)edgesG.size());
								for (const auto& edge: edgesG) {
									const auto& info = m_componentCache.GetComponentCreateInfoFromIdx(edge.info->infoIndex);
									LOG_N(
											"      %.*s (--> Archetype ID:%u)", (uint32_t)info.name.size(), info.name.data(),
											edge.archetype->id);
								}
							}

							if (!edgesC.empty()) {
								LOG_N("    Chunk - count:%u", (uint32_t)edgesC.size());
								for (const auto& edge: edgesC) {
									const auto& info = m_componentCache.GetComponentCreateInfoFromIdx(edge.info->infoIndex);
									LOG_N(
											"      %.*s (--> Archetype ID:%u)", (uint32_t)info.name.size(), info.name.data(),
											edge.archetype->id);
								}
							}
						}
					}

					{
						const auto& edgesG = archetype.edgesDel[ComponentType::CT_Generic];
						const auto& edgesC = archetype.edgesDel[ComponentType::CT_Chunk];
						const auto edgeCount = (uint32_t)(edgesG.size() + edgesC.size());
						if (edgeCount > 0) {
							LOG_N("  Del edges - count:%u", edgeCount);

							if (!edgesG.empty()) {
								LOG_N("    Generic - count:%u", (uint32_t)edgesG.size());
								for (const auto& edge: edgesG) {
									const auto& info = m_componentCache.GetComponentCreateInfoFromIdx(edge.info->infoIndex);
									LOG_N(
											"      %.*s (--> Archetype ID:%u)", (uint32_t)info.name.size(), info.name.data(),
											edge.archetype->id);
								}
							}

							if (!edgesC.empty()) {
								LOG_N("    Chunk - count:%u", (uint32_t)edgesC.size());
								for (const auto& edge: edgesC) {
									const auto& info = m_componentCache.GetComponentCreateInfoFromIdx(edge.info->infoIndex);
									LOG_N(
											"      %.*s (--> Archetype ID:%u)", (uint32_t)info.name.size(), info.name.data(),
											edge.archetype->id);
								}
							}
						}
					}
#endif

					auto logChunks = [](const auto& chunks) {
						for (size_t i = 0; i < chunks.size(); ++i) {
							const auto* pChunk = chunks[i];
							const auto& header = pChunk->header;
							LOG_N(
									"  Chunk #%04u, entities:%u/%u, lifespan:%u", (uint32_t)i, header.items.count,
									header.owner.info.capacity, header.info.lifespan);
						}
					};

					{
						const auto& chunks = archetype.chunks;
						if (!chunks.empty())
							LOG_N("  Enabled chunks");

						logChunks(chunks);
					}

					{
						const auto& chunks = archetype.chunksDisabled;
						if (!chunks.empty())
							LOG_N("  Disabled chunks");

						logChunks(chunks);
					}
				}
			}

			/*!
			Performs diagnostics on archetypes. Prints basic info about them and the chunks they contain.
			*/
			void DiagArchetypes() const {
				static bool DiagArchetypes = GAIA_ECS_DIAG_ARCHETYPES;
				if (!DiagArchetypes)
					return;

				// Print archetype info
				LOG_N("Archetypes:%u", (uint32_t)m_archetypes.size());
				for (const auto* archetype: m_archetypes) {
					DiagArchetype(*archetype);
					DiagArchetypes = true;
				}

				DiagArchetypes = false;
			}

			/*!
			Performs diagnostics on registered components.
			Prints basic info about them and reports and detected issues.
			*/
			void DiagRegisteredTypes() const {
				static bool DiagRegisteredTypes = GAIA_ECS_DIAG_REGISTERED_TYPES;
				if (!DiagRegisteredTypes)
					return;
				DiagRegisteredTypes = false;

				m_componentCache.Diag();
			}

			/*!
			Performs diagnostics on entites of the world.
			Also performs validation of internal structures which hold the entities.
			*/
			void DiagEntities() const {
				static bool DiagDeletedEntities = GAIA_ECS_DIAG_DELETED_ENTITIES;
				if (!DiagDeletedEntities)
					return;
				DiagDeletedEntities = false;

				ValidateEntityList();

				LOG_N("Deleted entities: %u", m_freeEntities);
				if (m_freeEntities) {
					LOG_N("  --> %u", m_nextFreeEntity);

					uint32_t iters = 0;
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

			/*!
			Performs diagnostics of the memory used by the world.
			*/
			void DiagMemory() const {
				ChunkAllocatorStats memstats;
				m_chunkAllocator.GetStats(memstats);
				LOG_N("ChunkAllocator stats");
				LOG_N("  Allocated: %" PRIu64 " B", memstats.AllocatedMemory);
				LOG_N("  Used: %" PRIu64 " B", memstats.AllocatedMemory - memstats.UsedMemory);
				LOG_N("  Overhead: %" PRIu64 " B", memstats.UsedMemory);
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

		inline ComponentCache& GetComponentCache(World& world) {
			return world.GetComponentCache();
		}
		inline const ComponentCache& GetComponentCache(const World& world) {
			return world.GetComponentCache();
		}
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
