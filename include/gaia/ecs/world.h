#pragma once
#include <cinttypes>
#include <type_traits>

#include "../config/config.h"
#include "../config/profiler.h"
#include "../containers/map.h"
#include "../containers/sarray.h"
#include "../containers/sarray_ext.h"
#include "../containers/set.h"
#include "../utils/containers.h"
#include "../utils/hashing_policy.h"
#include "../utils/span.h"
#include "../utils/type_info.h"
#include "../utils/utility.h"
#include "archetype.h"
#include "chunk.h"
#include "chunk_allocator.h"
#include "command_buffer.h"
#include "common.h"
#include "component.h"
#include "component_cache.h"
#include "entity.h"
#include "entity_query.h"

namespace gaia {
	namespace ecs {

		//----------------------------------------------------------------------

		struct ComponentSetter {
			Chunk* m_pChunk;
			uint32_t m_idx;

			template <typename T>
			ComponentSetter& SetComponent(typename DeduceComponent<T>::Type&& data) {
				if constexpr (IsGenericComponent<T>) {
					using U = typename detail::ExtractComponentType_Generic<T>::Type;
					m_pChunk->template SetComponent<T>(m_idx, std::forward<U>(data));
					return *this;
				} else {
					using U = typename detail::ExtractComponentType_NonGeneric<T>::Type;
					m_pChunk->template SetComponent<T>(std::forward<U>(data));
					return *this;
				}
			}

			template <typename T>
			ComponentSetter& SetComponentSilent(typename DeduceComponent<T>::Type&& data) {
				if constexpr (IsGenericComponent<T>) {
					using U = typename detail::ExtractComponentType_Generic<T>::Type;
					m_pChunk->template SetComponentSilent<T>(m_idx, std::forward<U>(data));
					return *this;
				} else {
					using U = typename detail::ExtractComponentType_NonGeneric<T>::Type;
					m_pChunk->template SetComponentSilent<T>(std::forward<U>(data));
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

			containers::map<utils::direct_hash_key, containers::darray<EntityQuery>> m_cachedQueries;
			//! Map or archetypes mapping to the same hash - used for lookups
			containers::map<utils::direct_hash_key, containers::darray<Archetype*>> m_archetypeMap;
			//! List of archetypes - used for iteration
			containers::darray<Archetype*> m_archetypes;
			//! Root archetype
			Archetype* m_rootArchetype = nullptr;

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

			GAIA_NODISCARD bool IsEntityValid(Entity entity) const {
				// Entity ID has to fit inside entity array
				if (entity.id() >= m_entities.size())
					return false;

				const auto& entityContainer = m_entities[entity.id()];
				// Generation ID has to match the one in the array
				if (entityContainer.gen != entity.gen())
					return false;
				// If chunk information is present the entity at the pointed index has
				// to match our entity
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
					for (auto* archetype: m_archetypes)
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
						pChunk->header.lifespan > 0 ||
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
				pChunk->header.lifespan = MAX_CHUNK_LIFESPAN;

				m_chunksToRemove.push_back(pChunk);
			}

			/*!
			Searches for archetype with a given set of components
			\param infosGeneric Span of generic component infos
			\param infosChunk Span of chunk component infos
			\param lookupHash Archetype lookup hash
			\return Pointer to archetype or nullptr
			*/
			GAIA_NODISCARD Archetype* FindArchetype(
					std::span<const ComponentInfo*> infosGeneric, std::span<const ComponentInfo*> infosChunk,
					utils::direct_hash_key lookupHash) {
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

				auto checkInfos = [&](const ComponentInfoList& list, const std::span<const ComponentInfo*>& infos) {
					for (uint32_t j = 0; j < infos.size(); j++) {
						// Different components. We need to search further
						if (list[j] != infos[j])
							return false;
					}
					return true;
				};

				// Iterate over the list of archetypes and find the exact match
				for (auto* archetype: archetypeArray) {
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

			/*!
			Creates a new archetype from a given set of components
			\param infosGeneric Span of generic component infos
			\param infosChunk Span of chunk component infos
			\return Pointer to the new archetype
			*/
			GAIA_NODISCARD Archetype*
			CreateArchetype(std::span<const ComponentInfo*> infosGeneric, std::span<const ComponentInfo*> infosChunk) {
				return Archetype::Create(*this, infosGeneric, infosChunk);
			}

			/*!
			Initializes the archetype with hash values for each kind of component types.
			\param genericHash Generic components hash
			\param chunkHash Chunk components hash
			\param lookupHash Hash used for archetype lookup purposes
			\return Pointer to the new archetype
			*/
			static void
			InitArchetype(Archetype* archetype, uint64_t genericHash, uint64_t chunkHash, utils::direct_hash_key lookupHash) {
				archetype->genericHash = genericHash;
				archetype->chunkHash = chunkHash;
				archetype->lookupHash = lookupHash;
			}

			void RegisterArchetype(Archetype* pArchetype) {
				// Make sure hashes were set already
				GAIA_ASSERT(pArchetype == m_rootArchetype || (pArchetype->genericHash != 0 || pArchetype->chunkHash != 0));
				GAIA_ASSERT(pArchetype == m_rootArchetype || pArchetype->lookupHash.hash != 0);

				// Make sure the archetype is not registered yet
				GAIA_ASSERT(!utils::has(m_archetypes, pArchetype));

				// Register the archetype
				pArchetype->id = (uint32_t)m_archetypes.size();
				m_archetypes.push_back(pArchetype);

				auto it = m_archetypeMap.find(pArchetype->lookupHash);
				if (it == m_archetypeMap.end()) {
					m_archetypeMap[pArchetype->lookupHash] = {pArchetype};
				} else {
					auto& archetypes = it->second;
					GAIA_ASSERT(!utils::has(archetypes, pArchetype));
					archetypes.push_back(pArchetype);
				}
			}

#if GAIA_DEBUG
			static void
			VerifyAddComponent(Archetype& archetype, Entity entity, ComponentType type, const ComponentInfo* infoToAdd) {
				const auto& lookup = archetype.componentLookupData[type];
				const auto& cc = GetComponentCache();

				// Make sure not to add too many infos
				if GAIA_UNLIKELY (!VerityArchetypeComponentCount(1)) {
					GAIA_ASSERT(false && "Trying to add too many components to entity!");
					LOG_W("Trying to add a component to entity [%u.%u] but there's no space left!", entity.id(), entity.gen());
					LOG_W("Already present:");
					const size_t oldInfosCount = lookup.size();
					for (size_t i = 0; i < oldInfosCount; i++) {
						const auto& info = cc.GetComponentCreateInfoFromIdx(lookup[i].infoIndex);
						LOG_W("> [%u] %.*s", (uint32_t)i, (uint32_t)info.name.size(), info.name.data());
					}
					LOG_W("Trying to add:");
					{
						const auto& info = cc.GetComponentCreateInfoFromIdx(infoToAdd->infoIndex);
						LOG_W("> %.*s", (uint32_t)info.name.size(), info.name.data());
					}
				}

				// Don't add the same component twice
				for (size_t i = 0; i < lookup.size(); ++i) {
					const auto& info = cc.GetComponentCreateInfoFromIdx(lookup[i].infoIndex);
					if (info.infoIndex == infoToAdd->infoIndex) {
						GAIA_ASSERT(false && "Trying to add a duplicate component");

						LOG_W(
								"Trying to add a duplicate of component %s to entity [%u.%u]", ComponentTypeString[type], entity.id(),
								entity.gen());
						LOG_W("> %.*s", (uint32_t)info.name.size(), info.name.data());
					}
				}
			}

			static void VerifyRemoveComponent(
					Archetype& archetype, Entity entity, ComponentType type, const ComponentInfo* infoToRemove) {
	#if GAIA_ARCHETYPE_GRAPH
				auto* pArchetypeLeft = archetype.FindDelEdgeArchetype(type, infoToRemove);
				if (pArchetypeLeft == nullptr) {
					GAIA_ASSERT(false && "Trying to remove a component which wasn't added");
					LOG_W("Trying to remove a component from entity [%u.%u] but it was never added", entity.id(), entity.gen());
					LOG_W("Currently present:");

					const auto& cc = GetComponentCache();

					const auto& infos = archetype.componentInfos[type];
					for (size_t k = 0; k < infos.size(); k++) {
						const auto& info = cc.GetComponentCreateInfoFromIdx(infos[k]->infoIndex);
						LOG_W("> [%u] %.*s", (uint32_t)k, (uint32_t)info.name.size(), info.name.data());
					}

					{
						LOG_W("Trying to remove:");
						const auto& info = cc.GetComponentCreateInfoFromIdx(infoToRemove->infoIndex);
						LOG_W("> %.*s", (uint32_t)info.name.size(), info.name.data());
					}
				}
	#else
				const auto& infos = archetype.componentInfos[type];
				if GAIA_UNLIKELY (!utils::has_if(infos, [&](const auto* info) {
														return info == infoToRemove;
													})) {
					GAIA_ASSERT(false && "Trying to remove a component which wasn't added");
					LOG_W("Trying to remove a component from entity [%u.%u] but it was never added", entity.id(), entity.gen());
					LOG_W("Currently present:");

					const auto& cc = GetComponentCache();

					for (size_t k = 0; k < infos.size(); k++) {
						const auto& info = cc.GetComponentCreateInfoFromIdx(infos[k]->infoIndex);
						LOG_W("> [%u] %.*s", (uint32_t)k, (uint32_t)info.name.size(), info.name.data());
					}

					{
						LOG_W("Trying to remove:");
						const auto& info = cc.GetComponentCreateInfoFromIdx(infoToRemove->infoIndex);
						LOG_W("> %.*s", (uint32_t)info.name.size(), info.name.data());
					}
				}
	#endif
			}
#endif

#if GAIA_ARCHETYPE_GRAPH
			static GAIA_FORCEINLINE void
			BuildGraphEdges(ComponentType type, Archetype* left, Archetype* right, const ComponentInfo* info) {
				left->edgesAdd[type].push_back({info, right});
				right->edgesDel[type].push_back({info, left});
			}

			/*!
			Checks if archetype \param left is a superset of archetype \param right (contains all its infos).
			\param left Entity to delete
			\param right Entity to delete
			\return Returns true if left is a superset of right
			*/
			static bool IsSuperSet(ComponentType type, Archetype& left, Archetype& right) {
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

			void TryBuildArchetypeGraph_Internal(
					Archetype* pArchetypeRight, ComponentType type,
					const containers::sarray_ext<const ComponentInfo*, MAX_COMPONENTS_PER_ARCHETYPE>& infoGeneric,
					const containers::sarray_ext<const ComponentInfo*, MAX_COMPONENTS_PER_ARCHETYPE>& infoChunk,
					const ComponentInfo* info) {
				const auto newInfos1Hash = CalculateLookupHash({infoGeneric});
				const auto newInfos2Hash = CalculateLookupHash({infoChunk});

				// Calculate hash for our combination of components
				utils::direct_hash_key lookupHash = {
						CalculateLookupHash(containers::sarray<uint64_t, 2>{newInfos1Hash, newInfos2Hash})};

				Archetype* pArchetypeLeft = FindArchetype(infoGeneric, infoChunk, lookupHash);
				if (pArchetypeLeft == nullptr) {
					pArchetypeLeft =
							CreateArchetype({infoGeneric.data(), infoGeneric.size()}, {infoChunk.data(), infoChunk.size()});
					InitArchetype(pArchetypeLeft, newInfos1Hash, newInfos2Hash, lookupHash);
					RegisterArchetype(pArchetypeLeft);
					BuildGraphEdges(type, pArchetypeLeft, pArchetypeRight, info);
					TryBuildArchetypeGraph(pArchetypeLeft, infoGeneric, infoChunk);
				} else if (pArchetypeLeft == m_rootArchetype) {
					if (pArchetypeRight->FindDelEdgeArchetype(type, info) == nullptr)
						BuildGraphEdges(type, pArchetypeLeft, pArchetypeRight, info);
				} else {
					if (pArchetypeRight->FindDelEdgeArchetype(type, info) == nullptr)
						BuildGraphEdges(type, pArchetypeLeft, pArchetypeRight, info);

					TryBuildArchetypeGraph(pArchetypeLeft, infoGeneric, infoChunk);
				}
			}

			void TryBuildArchetypeGraph(
					Archetype* pArchetypeRight,
					const containers::sarray_ext<const ComponentInfo*, MAX_COMPONENTS_PER_ARCHETYPE>& infoGeneric,
					const containers::sarray_ext<const ComponentInfo*, MAX_COMPONENTS_PER_ARCHETYPE>& infoChunk) {
				GAIA_ASSERT(pArchetypeRight != m_rootArchetype);

				auto prepareNewInfos = [&](const auto* info, const auto& infos) {
					containers::sarray_ext<const ComponentInfo*, MAX_COMPONENTS_PER_ARCHETYPE> newInfos;
					for (const auto* infoToCopy: infos) {
						if (infoToCopy != info)
							newInfos.push_back(infoToCopy);
					}
					return newInfos;
				};

				// Step 1 - all original chunks components + one less generic component
				for (const auto* info: infoGeneric)
					TryBuildArchetypeGraph_Internal(
							pArchetypeRight, ComponentType::CT_Generic, prepareNewInfos(info, infoGeneric), infoChunk, info);

				// Step 2 - all original generic components + one less chunk component
				for (const auto* info: infoChunk)
					TryBuildArchetypeGraph_Internal(
							pArchetypeRight, ComponentType::CT_Chunk, infoGeneric, prepareNewInfos(info, infoChunk), info);
			}

			template <typename Func>
			EntityQuery::MatchArchetypeQueryRet ArchetypeGraphTraverse(ComponentType type, Func func) const {
				// Use a stack to store the nodes we need to visit
				// TODO: Replace with std::stack or an alternative
				containers::darr<const Archetype*> stack;
				stack.reserve(MAX_COMPONENTS_PER_ARCHETYPE + MAX_COMPONENTS_PER_ARCHETYPE);
				containers::set<const Archetype*> visited;

				// Push all children of the root archetype onto the stack
				for (const auto& edge: m_rootArchetype->edgesAdd[(uint32_t)type])
					stack.push_back(edge.pArchetype);

				while (!stack.empty()) {
					// Pop the top node from the stack
					const Archetype* pArchetype = *stack.begin();
					stack.erase(stack.begin());

					// Decide whether to accept the node or skip it
					const auto ret = func(*pArchetype);
					if (ret == EntityQuery::MatchArchetypeQueryRet::Fail)
						return ret;
					if (ret == EntityQuery::MatchArchetypeQueryRet::Skip)
						continue;

					// Push all of the children of the current node onto the stack
					for (const auto& edge: pArchetype->edgesAdd[(uint32_t)type]) {
						if (visited.contains(edge.pArchetype))
							continue;
						visited.insert(edge.pArchetype);
						stack.push_back(edge.pArchetype);
					}
				}

				return EntityQuery::MatchArchetypeQueryRet::Ok;
			}
#endif

			/*!
			Searches for an archetype based on the given set of components. If no archetype is found a new one is created.
			\param pArchetypeLeft Archetype we originate from
			\param type Component infos
			\param infoToAdd Span of chunk components
			\return Pointer to archetype
			*/
			GAIA_NODISCARD Archetype*
			FindOrCreateArchetype(Archetype* pArchetypeLeft, ComponentType type, const ComponentInfo* infoToAdd) {
#if GAIA_ARCHETYPE_GRAPH
				// We don't want to store edges for the root archetype because the more components there are the longer
				// it would take to find anything. Therefore, for the root archetype we simply make a lookup.
				if (pArchetypeLeft == m_rootArchetype) {
					Archetype* pArchetypeRight = nullptr;
					if (type == ComponentType::CT_Generic) {
						const auto genericHash = infoToAdd->lookupHash;
						utils::direct_hash_key lookupHash = {CalculateLookupHash(containers::sarray<uint64_t, 2>{genericHash, 0})};
						pArchetypeRight = FindArchetype(std::span<const ComponentInfo*>(&infoToAdd, 1), {}, lookupHash);
						if (pArchetypeRight == nullptr) {
							pArchetypeRight = CreateArchetype(std::span<const ComponentInfo*>(&infoToAdd, 1), {});
							InitArchetype(pArchetypeRight, genericHash, 0, lookupHash);
							RegisterArchetype(pArchetypeRight);
							BuildGraphEdges(type, pArchetypeLeft, pArchetypeRight, infoToAdd);
						}
					} else {
						const auto chunkHash = infoToAdd->lookupHash;
						utils::direct_hash_key lookupHash = {CalculateLookupHash(containers::sarray<uint64_t, 2>{0, chunkHash})};
						pArchetypeRight = FindArchetype({}, std::span<const ComponentInfo*>(&infoToAdd, 1), lookupHash);
						if (pArchetypeRight == nullptr) {
							pArchetypeRight = CreateArchetype({}, std::span<const ComponentInfo*>(&infoToAdd, 1));
							InitArchetype(pArchetypeRight, 0, chunkHash, lookupHash);
							RegisterArchetype(pArchetypeRight);
							BuildGraphEdges(type, pArchetypeLeft, pArchetypeRight, infoToAdd);
						}
					}

					return pArchetypeRight;
				}

				const auto it = utils::find_if(pArchetypeLeft->edgesAdd[type], [infoToAdd](const auto& edge) {
					return edge.info == infoToAdd;
				});

				if (it != pArchetypeLeft->edgesAdd[type].end())
					return it->pArchetype;
#endif

				const uint32_t a = type;
				const uint32_t b = (type + 1) & 1;
				const containers::sarray_ext<const ComponentInfo*, MAX_COMPONENTS_PER_ARCHETYPE>* infos[2];

				containers::sarray_ext<const ComponentInfo*, MAX_COMPONENTS_PER_ARCHETYPE> infosNew;
				infos[a] = &infosNew;
				infos[b] = &pArchetypeLeft->componentInfos[b];

				// Prepare a joint array of component infos of old + the newly added component
				{
					const auto& componentInfos = pArchetypeLeft->componentInfos[a];
					const size_t componentInfosSize = componentInfos.size();
					infosNew.resize(componentInfosSize + 1);

					for (size_t j = 0; j < componentInfosSize; ++j)
						infosNew[j] = componentInfos[j];
					infosNew[componentInfosSize] = infoToAdd;
				}

				// Make sure to sort the component infos so we receive the same hash no matter the order in which components
				// are provided Bubble sort is okay. We're dealing with at most MAX_COMPONENTS_PER_ARCHETYPE items.
				utils::sort(infosNew, [](const ComponentInfo* left, const ComponentInfo* right) {
					return left->infoIndex < right->infoIndex;
				});

				// Once sorted we can calculate the hashes
				const uint64_t hashes[2] = {CalculateLookupHash({*infos[0]}), CalculateLookupHash({*infos[1]})};
				utils::direct_hash_key lookupHash = {
						CalculateLookupHash(containers::sarray<uint64_t, 2>{hashes[0], hashes[1]})};

#if GAIA_ARCHETYPE_GRAPH
				auto* pArchetypeRight =
						CreateArchetype({infos[0]->data(), infos[0]->size()}, {infos[1]->data(), infos[1]->size()});
				InitArchetype(pArchetypeRight, hashes[0], hashes[1], lookupHash);
				RegisterArchetype(pArchetypeRight);

				BuildGraphEdges(type, pArchetypeLeft, pArchetypeRight, infoToAdd);
				TryBuildArchetypeGraph(pArchetypeRight, *infos[0], *infos[1]);
#else
				auto* pArchetypeRight = FindArchetype({*infos[0]}, {*infos[1]}, lookupHash);
				if (pArchetypeRight == nullptr) {
					pArchetypeRight = CreateArchetype({infos[0]->data(), infos[0]->size()}, {infos[1]->data(), infos[1]->size()});
					InitArchetype(pArchetypeRight, hashes[0], hashes[1], lookupHash);
					RegisterArchetype(pArchetypeRight);
				}
#endif

				return pArchetypeRight;
			}

#if GAIA_ARCHETYPE_GRAPH
			/*!
				Searches for a parent archetype that contains the given component of \param type.
				\param archetype Archetype to search from
				\param type Component type
				\param typesToRemove Span of component infos we want to remove
				\return Pointer to archetype
				*/
			static GAIA_NODISCARD Archetype*
			FindArchetype_RemoveComponents(Archetype* archetype, ComponentType type, const ComponentInfo* intoToRemove) {
				// Follow the graph to the next archetype
				return archetype->FindDelEdgeArchetype(type, intoToRemove);
			}
#else
			/*!
				Searches for a parent archetype that contains the given component of \param type.
				\param archetype Archetype to search from
				\param type Component type
				\param typesToRemove Span of component infos we want to remove
				\return Pointer to archetype
				*/
			GAIA_NODISCARD Archetype*
			FindArchetype_RemoveComponents(Archetype* archetype, ComponentType type, const ComponentInfo* intoToRemove) {
				const uint32_t a = type;
				const uint32_t b = (type + 1) & 1;
				const containers::sarray_ext<const ComponentInfo*, MAX_COMPONENTS_PER_ARCHETYPE>* infos[2];

				containers::sarray_ext<const ComponentInfo*, MAX_COMPONENTS_PER_ARCHETYPE> infosNew;
				infos[a] = &infosNew;
				infos[b] = &archetype->componentInfos[b];

				// Find the intersection
				for (const auto* info: archetype->componentInfos[a]) {
					if (info == intoToRemove)
						goto nextIter;

					infosNew.push_back(info);

				nextIter:
					continue;
				}

				// Return if there's no change
				if (infosNew.size() == archetype->componentInfos[a].size())
					return nullptr;

				// Calculate the hashes
				const uint64_t hashes[2] = {CalculateLookupHash({*infos[0]}), CalculateLookupHash({*infos[1]})};
				utils::direct_hash_key lookupHash = {
						CalculateLookupHash(containers::sarray<uint64_t, 2>{hashes[0], hashes[1]})};

				auto* pArchetype = FindArchetype({*infos[0]}, {*infos[1]}, lookupHash);
				if (pArchetype == nullptr) {
					pArchetype = CreateArchetype({infos[0]->data(), infos[0]->size()}, {infos[1]->data(), infos[1]->size()});
					InitArchetype(pArchetype, hashes[0], hashes[1], lookupHash);
					RegisterArchetype(pArchetype);
				}

				return pArchetype;
			}
#endif

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
			GAIA_NODISCARD Archetype* GetArchetype(Entity entity) const {
				GAIA_ASSERT(IsEntityValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				auto* pChunk = entityContainer.pChunk;
				return pChunk != nullptr ? const_cast<Archetype*>(&pChunk->header.owner) : nullptr;
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
				auto* oldChunk = entityContainer.pChunk;
				const auto oldIndex = entityContainer.idx;
				const auto& oldArchetype = oldChunk->header.owner;

				// Find a new chunk for the entity and move it inside.
				// Old entity ID needs to remain valid or lookups would break.
				auto* newChunk = newArchetype.FindOrCreateFreeChunk();
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

					auto* pTargetArchetype = FindOrCreateArchetype(&archetype, type, infoToAdd);
					MoveEntity(entity, *pTargetArchetype);
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

					auto* pTargetArchetype = FindOrCreateArchetype(&archetype, type, infoToAdd);
					StoreEntity(entity, pTargetArchetype->FindOrCreateFreeChunk());
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

				auto* newArchetype = FindArchetype_RemoveComponents(&archetype, type, infoToRemove);
				GAIA_ASSERT(newArchetype != nullptr);
				MoveEntity(entity, *newArchetype);

				return ComponentSetter{pChunk, entityContainer.idx};
			}

			void Init() {
				m_rootArchetype = CreateArchetype({}, {});
				InitArchetype(m_rootArchetype, 0, 0, {CalculateLookupHash(containers::sarray<uint64_t, 2>{0, 0})});
				RegisterArchetype(m_rootArchetype);
			}

			void Done() {
				Cleanup();
				m_chunkAllocator.Flush();

#if GAIA_DEBUG
				// Make sure there are no leaks
				ChunkAllocatorStats memstats = m_chunkAllocator.GetStats();
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
			GAIA_NODISCARD uint32_t GetWorldVersion() const {
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
			Entity CreateEntity(Entity entity) {
				auto& entityContainer = m_entities[entity.id()];

				auto* pChunk = entityContainer.pChunk;

				// If the reference entity doesn't have any chunk assigned create a chunkless one
				if (pChunk == nullptr)
					return CreateEntity();

				auto& archetype = const_cast<Archetype&>(pChunk->header.owner);

				const auto newEntity = CreateEntity(archetype);
				auto& newEntityContainer = m_entities[newEntity.id()];
				auto* newChunk = newEntityContainer.pChunk;

				// By adding a new entity m_entities array might have been reallocated.
				// We need to get the new address.
				auto& oldEntityContainer = m_entities[entity.id()];
				auto* oldChunk = oldEntityContainer.pChunk;

				// Copy generic component data from reference entity to our new ntity
				const auto& infos = archetype.componentInfos[ComponentType::CT_Generic];
				const auto& looks = archetype.componentLookupData[ComponentType::CT_Generic];

				for (size_t i = 0; i < infos.size(); i++) {
					const auto* info = infos[i];
					if (info->properties.size == 0U)
						continue;

					const auto offset = looks[i].offset;
					const auto idxFrom = offset + info->properties.size * (uint32_t)oldEntityContainer.idx;
					const auto idxTo = offset + info->properties.size * (uint32_t)newEntityContainer.idx;

					GAIA_ASSERT(idxFrom < Chunk::DATA_SIZE_NORESERVE);
					GAIA_ASSERT(idxTo < Chunk::DATA_SIZE_NORESERVE);

					memcpy(&newChunk->data[idxTo], &oldChunk->data[idxFrom], info->properties.size);
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
							if (info->properties.size == 0U)
								continue;

							const auto offset = looks[i].offset;
							const auto idxFrom = offset + info->properties.size * (uint32_t)entityContainer.idx;
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
			Attaches a new component to \param entity.
			\warning It is expected the component is not there yet and that \param
			entity is valid. Undefined behavior otherwise.
			\return ComponentSetter object.
			*/
			template <typename T>
			ComponentSetter AddComponent(Entity entity) {
				VerifyComponent<T>();
				GAIA_ASSERT(IsEntityValid(entity));

				using U = typename DeduceComponent<T>::Type;
				const auto* info = GetComponentCacheRW().GetOrCreateComponentInfo<U>();

				if constexpr (IsGenericComponent<T>) {
					auto& entityContainer = AddComponent_Internal(ComponentType::CT_Generic, entity, info);
					return ComponentSetter{entityContainer.pChunk, entityContainer.idx};
				} else {
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
			template <typename T>
			ComponentSetter AddComponent(Entity entity, typename DeduceComponent<T>::Type&& data) {
				VerifyComponent<T>();
				GAIA_ASSERT(IsEntityValid(entity));

				using U = typename DeduceComponent<T>::Type;
				const auto* info = GetComponentCacheRW().GetOrCreateComponentInfo<U>();

				if constexpr (IsGenericComponent<T>) {
					auto& entityContainer = AddComponent_Internal(ComponentType::CT_Generic, entity, info);
					auto* pChunk = entityContainer.pChunk;
					pChunk->template SetComponent<T>(entityContainer.idx, std::forward<U>(data));
					return ComponentSetter{entityContainer.pChunk, entityContainer.idx};
				} else {
					auto& entityContainer = AddComponent_Internal(ComponentType::CT_Chunk, entity, info);
					auto* pChunk = entityContainer.pChunk;
					pChunk->template SetComponent<T>(std::forward<U>(data));
					return ComponentSetter{entityContainer.pChunk, entityContainer.idx};
				}
			}

			/*!
			Removes a component from \param entity.
			\warning It is expected the component is not there yet and that
			\param entity is valid. Undefined behavior otherwise.
			\return ComponentSetter object.
			*/
			template <typename T>
			ComponentSetter RemoveComponent(Entity entity) {
				VerifyComponent<T>();
				GAIA_ASSERT(IsEntityValid(entity));

				using U = typename DeduceComponent<T>::Type;
				const auto* info = GetComponentCacheRW().GetOrCreateComponentInfo<U>();

				if constexpr (IsGenericComponent<T>) {
					return RemoveComponent_Internal(ComponentType::CT_Generic, entity, info);
				} else {
					return RemoveComponent_Internal(ComponentType::CT_Chunk, entity, info);
				}
			}

			/*!
			Sets the value of component on \param entity.
			\warning It is expected the component was added to \param entity already. Undefined behavior otherwise.
			\param entity is valid. Undefined behavior otherwise.
			\return ComponentSetter object.
			*/
			template <typename T>
			ComponentSetter SetComponent(Entity entity, typename DeduceComponent<T>::Type&& data) {
				VerifyComponent<T>();
				GAIA_ASSERT(IsEntityValid(entity));

				auto& entityContainer = m_entities[entity.id()];
				return ComponentSetter{entityContainer.pChunk, entityContainer.idx}.SetComponent<T>(
						std::forward<typename DeduceComponent<T>::Type>(data));
			}

			/*!
			Sets the value of component on \param entity.
			\warning It is expected the component was added to \param entity already. Undefined behavior otherwise.
			\param entity is valid. Undefined behavior otherwise.
			\return ComponentSetter object.
			*/
			template <typename T>
			ComponentSetter SetComponentSilent(Entity entity, typename DeduceComponent<T>::Type&& data) {
				VerifyComponent<T>();
				GAIA_ASSERT(IsEntityValid(entity));

				auto& entityContainer = m_entities[entity.id()];
				return ComponentSetter{entityContainer.pChunk, entityContainer.idx}.SetComponentSilent<T>(
						std::forward<typename DeduceComponent<T>::Type>(data));
			}

			/*!
			Returns the value stored in the component on \param entity.
			\warning It is expected the component was added to \param entity already. Undefined behavior otherwise.
			\return Value stored in the component.
			*/
			template <typename T>
			auto GetComponent(Entity entity) const {
				VerifyComponent<T>();
				GAIA_ASSERT(IsEntityValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				const auto* pChunk = entityContainer.pChunk;

				if constexpr (IsGenericComponent<T>)
					return pChunk->GetComponent<T>(entityContainer.idx);
				else
					return pChunk->GetComponent<T>();
			}

			//----------------------------------------------------------------------

			/*!
			Tells if \param entity contains the component.
			Undefined behavior if \param entity is not valid.
			\return True if the component is present on entity.
			*/
			template <typename T>
			GAIA_NODISCARD bool HasComponent(Entity entity) {
				VerifyComponent<T>();
				GAIA_ASSERT(IsEntityValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				if (const auto* pChunk = entityContainer.pChunk)
					return pChunk->HasComponent<T>();

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
					return chunk.ViewRW_Internal<U, true>();
			}

			//--------------------------------------------------------------------------------

			template <typename... T, typename Func>
			GAIA_FORCEINLINE void
			ForEachEntityInChunk([[maybe_unused]] utils::func_type_list<T...> types, Chunk& chunk, Func func) {
				const size_t size = chunk.GetItemCount();
				if (!size)
					return;

				if constexpr (sizeof...(T) > 0) {
					// Pointers to the respective component types in the chunk, e.g
					// 		w.ForEach(q, [&](Position& p, const Velocity& v) {...}
					// Translates to:
					//  	auto p = chunk.ViewRW_Internal<Position, true>();
					//		auto v = chunk.View_Internal<Velocity>();
					auto dataPointerTuple = std::make_tuple(GetComponentView<T>(chunk)...);

					// Iterate over each entity in the chunk.
					// Translates to:
					//		for (size_t i = 0; i < chunk.GetItemCount(); ++i)
					//			func(p[i], v[i]);

					for (size_t i = 0; i < size; ++i)
						func(std::get<decltype(GetComponentView<T>(chunk))>(dataPointerTuple)[i]...);
				} else {
					// No functor parameters. Do an empty loop.
					for (size_t i = 0; i < size; ++i)
						func();
				}
			}

			template <typename Func>
			GAIA_FORCEINLINE void ForEachArchetype(EntityQuery& query, Func func) {
#if GAIA_ARCHETYPE_GRAPH
				// query.Match(*this, (uint32_t)m_archetypes.size());
				ArchetypeGraphTraverse(ComponentType::CT_Generic, [&](const Archetype& archetype) {
					(void)archetype;
					return EntityQuery::MatchArchetypeQueryRet::Ok;
				});
#else
				query.Match(m_archetypes);
#endif
				for (auto* pArchetype: query)
					func(*pArchetype);
			}

			template <typename Func>
			GAIA_FORCEINLINE void ForEachArchetype_NoMatch(const EntityQuery& query, Func func) const {
				for (const auto* pArchetype: query)
					func(*pArchetype);
			}

			template <typename Func>
			GAIA_FORCEINLINE void ForEachArchetypeCond_NoMatch(const EntityQuery& query, Func func) const {
				for (const auto* pArchetype: query)
					if (!func(*pArchetype))
						break;
			}

			//--------------------------------------------------------------------------------

			template <typename... T>
			void UnpackArgsIntoQuery([[maybe_unused]] utils::func_type_list<T...> types, EntityQuery& query) const {
				static_assert(sizeof...(T) > 0, "Inputs-less functors can not be unpacked to query");
				query.All<T...>();
			}

			template <typename... T>
			GAIA_NODISCARD bool
			UnpackArgsIntoQuery_Check([[maybe_unused]] utils::func_type_list<T...> types, EntityQuery& query) const {
				if constexpr (sizeof...(T) > 0)
					return query.HasAll<T...>();
				else
					return true;
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

			GAIA_NODISCARD static bool CheckFilters(const EntityQuery& query, const Chunk& chunk) {
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

			GAIA_NODISCARD static bool
			CanAcceptChunkForProcessing(const Chunk& chunk, const EntityQuery& q, bool hasFilters) {
				if GAIA_UNLIKELY (!chunk.HasEntities())
					return false;
				if (!q.CheckConstraints(!chunk.IsDisabled()))
					return false;
				if (hasFilters && !CheckFilters(q, chunk))
					return false;

				return true;
			}

			template <bool HasFilters>
			GAIA_NODISCARD static bool CanAcceptChunkForProcessing(const Chunk& chunk, const EntityQuery& q) {
				if GAIA_UNLIKELY (!chunk.HasEntities())
					return false;
				if (!q.CheckConstraints(!chunk.IsDisabled()))
					return false;
				if constexpr (HasFilters) {
					if (!CheckFilters(q, chunk))
						return false;
				}

				return true;
			}

			//--------------------------------------------------------------------------------

			template <bool HasFilters, typename Func>
			static void ProcessQueryOnChunks(const containers::darray<Chunk*>& chunksList, EntityQuery& query, Func func) {
				constexpr size_t BatchSize = 256U;
				containers::sarray<Chunk*, BatchSize> tmp;

				size_t chunkOffset = 0;
				size_t indexInBatch = 0;

				size_t itemsLeft = chunksList.size();
				while (itemsLeft > 0) {
					GAIA_PROF_START(PrepareChunks);

					const size_t batchSize = itemsLeft > BatchSize ? BatchSize : itemsLeft;

					// Prepare a buffer to iterate over
					for (size_t j = chunkOffset; j < chunkOffset + batchSize; ++j) {
						auto* pChunk = chunksList[j];

						if (!CanAcceptChunkForProcessing<HasFilters>(*pChunk, query))
							continue;

						tmp[indexInBatch++] = pChunk;
					}

					GAIA_PROF_STOP();
					GAIA_PROF_START(ExecChunks);

					// Execute functors in batches
					// This is what we're effectively doing:
					// for (size_t chunkIdx = 0; chunkIdx < indexInBatch; ++chunkIdx)
					//	func(*tmp[chunkIdx]);

					if (GAIA_UNLIKELY(indexInBatch == 0)) {
						// Nothing to do
					} else if GAIA_UNLIKELY (indexInBatch == 1) {
						// We only have one chunk to process
						func(*tmp[0]);
					} else {
						// We have many chunks to process.
						// Chunks might be located at different memory locations. Not even in the same memory page.
						// Therefore, to make it easier for the CPU we give it a hint that we want to prefetch data
						// for the next chunk explictely so we do not end up stalling later.
						// Note, this is a micro optimization and on average it bring no performance benefit. It only
						// helps with edge cases.
						// Let us be conservatine for now and go with T2. That means we will try to keep our data at
						// least in L3 cache or higher.
						gaia::prefetch(&tmp[1], PREFETCH_HINT_T2);
						func(*tmp[0]);

						size_t chunkIdx = 1;
						for (; chunkIdx < indexInBatch - 1; ++chunkIdx) {
							gaia::prefetch(&tmp[chunkIdx + 1], PREFETCH_HINT_T2);
							func(*tmp[chunkIdx]);
						}

						func(*tmp[chunkIdx]);
					}

					GAIA_PROF_STOP();

					// Prepeare for the next loop
					indexInBatch = 0;
					itemsLeft -= batchSize;
				}
			}

			template <typename Func>
			static void RunQueryOnChunks_Internal(World& world, EntityQuery& query, Func func) {
				// Update the world version
				world.UpdateWorldVersion();

				const bool hasFilters = query.HasFilters();
				if (hasFilters) {
					// Iterate over all archetypes
					world.ForEachArchetype(query, [&](Archetype& archetype) {
						GAIA_ASSERT(archetype.info.structuralChangesLocked < 8);
						++archetype.info.structuralChangesLocked;

						if (query.CheckConstraints<true>())
							ProcessQueryOnChunks<true>(archetype.chunks, query, func);
						if (query.CheckConstraints<false>())
							ProcessQueryOnChunks<true>(archetype.chunksDisabled, query, func);

						GAIA_ASSERT(archetype.info.structuralChangesLocked > 0);
						--archetype.info.structuralChangesLocked;
					});
				} else {
					// Iterate over all archetypes
					world.ForEachArchetype(query, [&](Archetype& archetype) {
						GAIA_ASSERT(archetype.info.structuralChangesLocked < 8);
						++archetype.info.structuralChangesLocked;

						if (query.CheckConstraints<true>())
							ProcessQueryOnChunks<false>(archetype.chunks, query, func);
						if (query.CheckConstraints<false>())
							ProcessQueryOnChunks<false>(archetype.chunksDisabled, query, func);

						GAIA_ASSERT(archetype.info.structuralChangesLocked > 0);
						--archetype.info.structuralChangesLocked;
					});
				}

				query.SetWorldVersion(world.GetWorldVersion());
			}

			//--------------------------------------------------------------------------------

			template <typename... T>
			static void RegisterComponents_Internal(utils::func_type_list<T...> /*no_name*/) {
				static_assert(sizeof...(T) > 0, "Empty EntityQuery is not supported in this context");
				auto& cc = GetComponentCacheRW();
				((void)cc.GetOrCreateComponentInfo<T>(), ...);
			}

			template <typename Func>
			static void RegisterComponents() {
				using InputArgs = decltype(utils::func_args(&Func::operator()));
				RegisterComponents_Internal(InputArgs{});
			}

			//--------------------------------------------------------------------------------

			static EntityQuery& AddOrFindEntityQueryInCache(World& world, EntityQuery& queryTmp) {
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
			GAIA_FORCEINLINE void ForEachChunk_External(World& world, EntityQuery& query, Func func) {
				GAIA_PROF_SCOPE(ForEachChunk);

				RunQueryOnChunks_Internal(world, query, [&](Chunk& chunk) {
					func(chunk);
				});
			}

			template <typename Func>
			GAIA_FORCEINLINE void ForEachChunk_Internal(World& world, EntityQuery&& queryTmp, Func func) {
				RegisterComponents<Func>(world);
				queryTmp.CalculateLookupHash();
				RunQueryOnChunks_Internal(world, AddOrFindEntityQueryInCache(world, queryTmp), [&](Chunk& chunk) {
					func(chunk);
				});
			}

			//--------------------------------------------------------------------------------

			template <typename Func>
			GAIA_FORCEINLINE void ForEach_External(World& world, EntityQuery& query, Func func) {
				GAIA_PROF_SCOPE(ForEach);

#if GAIA_DEBUG
				// Make sure we only use components specificed in the query
				GAIA_ASSERT(CheckQuery<Func>(world, query));
#endif

				using InputArgs = decltype(utils::func_args(&Func::operator()));
				RunQueryOnChunks_Internal(world, query, [&](Chunk& chunk) {
					world.ForEachEntityInChunk(InputArgs{}, chunk, func);
				});
			}

			template <typename Func>
			GAIA_FORCEINLINE void ForEach_Internal(World& world, EntityQuery&& queryTmp, Func func) {
				RegisterComponents<Func>();
				queryTmp.CalculateLookupHash();

				using InputArgs = decltype(utils::func_args(&Func::operator()));
				RunQueryOnChunks_Internal(world, AddOrFindEntityQueryInCache(world, queryTmp), [&](Chunk& chunk) {
					world.ForEachEntityInChunk(InputArgs{}, chunk, func);
				});
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

			//--------------------------------------------------------------------------------

			class QueryResult {
				const World& m_w;
				const EntityQuery& m_q;

			public:
				QueryResult(const World& w, const EntityQuery& q): m_w(w), m_q(q) {}

				/*!
				Returns true or false depending on whether there are entities matching the query.
				\param ignoreFilters If true any filters which might be set on the entity query is ignored
				\warning Only use if you only care if there are any entities matching the query.
								 The result is not cached and repeated calls to the function might be slow.
								 If you already called ToArray, checking if it is empty is preferred.
				\return True if there are any entites matchine the query. False otherwise.
				*/
				bool HasItems(bool ignoreFilters = false) const {
					bool hasItems = false;

					const bool hasFilters = !ignoreFilters && m_q.HasFilters();

					auto execWithFiltersON = [&](const auto& chunksList) {
						for (auto* pChunk: chunksList) {
							if (!CanAcceptChunkForProcessing<true>(*pChunk, m_q))
								continue;
							hasItems = true;
							break;
						}
					};

					auto execWithFiltersOFF = [&](const auto& chunksList) {
						for (auto* pChunk: chunksList) {
							if (!CanAcceptChunkForProcessing<false>(*pChunk, m_q))
								continue;
							hasItems = true;
							break;
						}
					};

					if (hasItems) {
						if (hasFilters) {
							// Iterate over all archetypes
							m_w.ForEachArchetypeCond_NoMatch(m_q, [&](const Archetype& archetype) {
								if (m_q.CheckConstraints<true>()) {
									execWithFiltersON(archetype.chunks);
									return false;
								}
								if (m_q.CheckConstraints<false>()) {
									execWithFiltersON(archetype.chunksDisabled);
									return false;
								}

								return true;
							});
						} else {
							// Iterate over all archetypes
							m_w.ForEachArchetypeCond_NoMatch(m_q, [&](const Archetype& archetype) {
								if (m_q.CheckConstraints<true>()) {
									execWithFiltersOFF(archetype.chunks);
									return false;
								}
								if (m_q.CheckConstraints<false>()) {
									execWithFiltersOFF(archetype.chunksDisabled);
									return false;
								}

								return true;
							});
						}

					} else {
						if (hasFilters) {
							// Iterate over all archetypes
							m_w.ForEachArchetypeCond_NoMatch(m_q, [&](const Archetype& archetype) {
								if (m_q.CheckConstraints<true>())
									execWithFiltersON(archetype.chunks);
								if (m_q.CheckConstraints<false>())
									execWithFiltersON(archetype.chunksDisabled);

								return true;
							});
						} else {
							// Iterate over all archetypes
							m_w.ForEachArchetypeCond_NoMatch(m_q, [&](const Archetype& archetype) {
								if (m_q.CheckConstraints<true>())
									execWithFiltersOFF(archetype.chunks);
								if (m_q.CheckConstraints<false>())
									execWithFiltersOFF(archetype.chunksDisabled);

								return true;
							});
						}
					}

					return hasItems;
				}

				/*!
				Returns the number of entities matching the query
				\param ignoreFilters If true any filters which might be set on the entity query is ignored
				\warning Only use if you only care about the number of entities matching the query.
								 The result is not cached and repeated calls to the function might be slow.
								 If you already called ToArray, use the size provided by the array.
				\return The number of matching entities
				*/
				size_t CalculateItemCount(bool ignoreFilters = false) const {
					size_t itemCount = 0;

					const bool hasFilters = !ignoreFilters && m_q.HasFilters();

					auto execWithFiltersON = [&](const auto& chunksList) {
						for (auto* pChunk: chunksList) {
							if (CanAcceptChunkForProcessing<true>(*pChunk, m_q))
								itemCount += pChunk->GetItemCount();
						}
					};

					auto execWithFiltersOFF = [&](const auto& chunksList) {
						for (auto* pChunk: chunksList) {
							if (CanAcceptChunkForProcessing<false>(*pChunk, m_q))
								itemCount += pChunk->GetItemCount();
						}
					};

					if (hasFilters) {
						m_w.ForEachArchetype_NoMatch(m_q, [&](const Archetype& archetype) {
							if (m_q.CheckConstraints<true>())
								execWithFiltersON(archetype.chunks);
							if (m_q.CheckConstraints<false>())
								execWithFiltersON(archetype.chunksDisabled);
						});
					} else {
						m_w.ForEachArchetype_NoMatch(m_q, [&](const Archetype& archetype) {
							if (m_q.CheckConstraints<true>())
								execWithFiltersOFF(archetype.chunks);
							if (m_q.CheckConstraints<false>())
								execWithFiltersOFF(archetype.chunksDisabled);
						});
					}

					return itemCount;
				}

				/*!
				Returns an array of components or entities matching the query
				\tparam Container Container storing entities or components
				\return Array with entities or components
				*/
				template <typename Container>
				void ToArray(Container& outArray) const {
					using ContainerItemType = typename Container::value_type;

					const size_t itemCount = CalculateItemCount();
					outArray.reserve(itemCount);

					const bool hasFilters = m_q.HasFilters();

					auto addChunk = [&](Chunk* pChunk) {
						const auto componentView = pChunk->template View<ContainerItemType>();
						for (size_t i = 0; i < pChunk->GetItemCount(); ++i)
							outArray.push_back(componentView[i]);
					};

					auto execWithFiltersON = [&](const auto& chunksList) {
						for (auto* pChunk: chunksList) {
							if (CanAcceptChunkForProcessing<true>(*pChunk, m_q))
								addChunk(pChunk);
						}
					};

					auto execWithFiltersOFF = [&](const auto& chunksList) {
						for (auto* pChunk: chunksList) {
							if (CanAcceptChunkForProcessing<false>(*pChunk, m_q))
								addChunk(pChunk);
						}
					};

					if (hasFilters) {
						m_w.ForEachArchetype_NoMatch(m_q, [&](const Archetype& archetype) {
							if (m_q.CheckConstraints<true>())
								execWithFiltersON(archetype.chunks);
							if (m_q.CheckConstraints<false>())
								execWithFiltersON(archetype.chunksDisabled);
						});
					} else {
						m_w.ForEachArchetype_NoMatch(m_q, [&](const Archetype& archetype) {
							if (m_q.CheckConstraints<true>())
								execWithFiltersOFF(archetype.chunks);
							if (m_q.CheckConstraints<false>())
								execWithFiltersOFF(archetype.chunksDisabled);
						});
					}
				}

				/*!
				Returns an array of chunks matching the query
				\return Array of chunks
				*/
				template <typename Container>
				void ToChunkArray(Container& outArray) const {
					static_assert(std::is_same_v<typename Container::value_type, Chunk*>);

					const size_t itemCount = CalculateItemCount();
					outArray.reserve(itemCount);

					const bool hasFilters = m_q.HasFilters();

					auto execWithFiltersON = [&](const auto& chunksList) {
						for (auto* pChunk: chunksList) {
							if (CanAcceptChunkForProcessing<true>(*pChunk, m_q))
								outArray.push_back(pChunk);
						}
					};

					auto execWithFiltersOFF = [&](const auto& chunksList) {
						for (auto* pChunk: chunksList) {
							if (CanAcceptChunkForProcessing<false>(*pChunk, m_q))
								outArray.push_back(pChunk);
						}
					};

					if (hasFilters) {
						m_w.ForEachArchetype_NoMatch(m_q, [&](const Archetype& archetype) {
							if (m_q.CheckConstraints<true>())
								execWithFiltersON(archetype.chunks);
							if (m_q.CheckConstraints<false>())
								execWithFiltersON(archetype.chunksDisabled);
						});
					} else {
						m_w.ForEachArchetype_NoMatch(m_q, [&](const Archetype& archetype) {
							if (m_q.CheckConstraints<true>())
								execWithFiltersOFF(archetype.chunks);
							if (m_q.CheckConstraints<false>())
								execWithFiltersOFF(archetype.chunksDisabled);
						});
					}
				}
			};

			GAIA_NODISCARD QueryResult FromQuery(EntityQuery& query) {
				query.Match(m_archetypes);
				return QueryResult(*this, query);
			}

			//--------------------------------------------------------------------------------

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
						pChunk->header.lifespan = MAX_CHUNK_LIFESPAN;
						utils::erase_fast(m_chunksToRemove, i);
						continue;
					}

					GAIA_ASSERT(pChunk->header.lifespan > 0);
					--pChunk->header.lifespan;
					if (pChunk->header.lifespan > 0) {
						++i;
						continue;
					}
				}

				// Remove all dead chunks
				for (auto* pChunk: m_chunksToRemove)
					const_cast<Archetype&>(pChunk->header.owner).RemoveChunk(pChunk);
				m_chunksToRemove.clear();
			}

			//--------------------------------------------------------------------------------

			static void DiagArchetype_PrintBasicInfo(const Archetype& archetype) {
				const auto& cc = GetComponentCache();
				const auto& genericComponents = archetype.componentInfos[ComponentType::CT_Generic];
				const auto& chunkComponents = archetype.componentInfos[ComponentType::CT_Chunk];

				// Caclulate the number of entites in archetype
				uint32_t entityCount = 0;
				uint32_t entityCountDisabled = 0;
				for (const auto* chunk: archetype.chunks)
					entityCount += chunk->GetItemCount();
				for (const auto* chunk: archetype.chunksDisabled) {
					entityCountDisabled += chunk->GetItemCount();
					entityCount += chunk->GetItemCount();
				}

				// Calculate the number of components
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
						logComponentInfo(component, cc.GetComponentCreateInfoFromIdx(component->infoIndex));
				}
				if (!chunkComponents.empty()) {
					LOG_N("  Chunk components - count:%u", (uint32_t)chunkComponents.size());
					for (const auto* component: chunkComponents)
						logComponentInfo(component, cc.GetComponentCreateInfoFromIdx(component->infoIndex));
				}
			}

#if GAIA_ARCHETYPE_GRAPH
			static void DiagArchetype_PrintGraphInfo(const Archetype& archetype) {
				const auto& cc = GetComponentCache();

				// Add edges (movement towards the leafs)
				{
					const auto& edgesG = archetype.edgesAdd[ComponentType::CT_Generic];
					const auto& edgesC = archetype.edgesAdd[ComponentType::CT_Chunk];
					const auto edgeCount = (uint32_t)(edgesG.size() + edgesC.size());
					if (edgeCount > 0) {
						LOG_N("  Add edges - count:%u", edgeCount);

						if (!edgesG.empty()) {
							LOG_N("    Generic - count:%u", (uint32_t)edgesG.size());
							for (const auto& edge: edgesG) {
								const auto& info = cc.GetComponentCreateInfoFromIdx(edge.info->infoIndex);
								LOG_N(
										"      %.*s (--> Archetype ID:%u)", (uint32_t)info.name.size(), info.name.data(),
										edge.pArchetype->id);
							}
						}

						if (!edgesC.empty()) {
							LOG_N("    Chunk - count:%u", (uint32_t)edgesC.size());
							for (const auto& edge: edgesC) {
								const auto& info = cc.GetComponentCreateInfoFromIdx(edge.info->infoIndex);
								LOG_N(
										"      %.*s (--> Archetype ID:%u)", (uint32_t)info.name.size(), info.name.data(),
										edge.pArchetype->id);
							}
						}
					}
				}

				// Delete edges (movement towards the root)
				{
					const auto& edgesG = archetype.edgesDel[ComponentType::CT_Generic];
					const auto& edgesC = archetype.edgesDel[ComponentType::CT_Chunk];
					const auto edgeCount = (uint32_t)(edgesG.size() + edgesC.size());
					if (edgeCount > 0) {
						LOG_N("  Del edges - count:%u", edgeCount);

						if (!edgesG.empty()) {
							LOG_N("    Generic - count:%u", (uint32_t)edgesG.size());
							for (const auto& edge: edgesG) {
								const auto& info = cc.GetComponentCreateInfoFromIdx(edge.info->infoIndex);
								LOG_N(
										"      %.*s (--> Archetype ID:%u)", (uint32_t)info.name.size(), info.name.data(),
										edge.pArchetype->id);
							}
						}

						if (!edgesC.empty()) {
							LOG_N("    Chunk - count:%u", (uint32_t)edgesC.size());
							for (const auto& edge: edgesC) {
								const auto& info = cc.GetComponentCreateInfoFromIdx(edge.info->infoIndex);
								LOG_N(
										"      %.*s (--> Archetype ID:%u)", (uint32_t)info.name.size(), info.name.data(),
										edge.pArchetype->id);
							}
						}
					}
				}
			}
#endif

			static void DiagArchetype_PrintChunkInfo(const Archetype& archetype) {
				auto logChunks = [](const auto& chunks) {
					for (size_t i = 0; i < chunks.size(); ++i) {
						const auto* pChunk = chunks[i];
						const auto& header = pChunk->header;
						LOG_N(
								"  Chunk #%04u, entities:%u/%u, lifespan:%u", (uint32_t)i, header.count, header.owner.info.capacity,
								header.lifespan);
					}
				};

				// Enabled chunks
				{
					const auto& chunks = archetype.chunks;
					if (!chunks.empty())
						LOG_N("  Enabled chunks");

					logChunks(chunks);
				}

				// Disabled chunks
				{
					const auto& chunks = archetype.chunksDisabled;
					if (!chunks.empty())
						LOG_N("  Disabled chunks");

					logChunks(chunks);
				}
			}

			/*!
			Performs diagnostics on a specific archetype. Prints basic info about it and the chunks it contains.
			\param archetype Archetype to run diagnostics on
			*/
			static void DiagArchetype(const Archetype& archetype) {
				static bool DiagArchetypes = GAIA_ECS_DIAG_ARCHETYPES;
				if (!DiagArchetypes)
					return;
				DiagArchetypes = false;

				DiagArchetype_PrintBasicInfo(archetype);
#if GAIA_ARCHETYPE_GRAPH
				DiagArchetype_PrintGraphInfo(archetype);
#endif
				DiagArchetype_PrintChunkInfo(archetype);
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
			static void DiagRegisteredTypes() {
				static bool DiagRegisteredTypes = GAIA_ECS_DIAG_REGISTERED_TYPES;
				if (!DiagRegisteredTypes)
					return;
				DiagRegisteredTypes = false;

				GetComponentCache().Diag();
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
				if (m_freeEntities != 0U) {
					LOG_N("  --> %u", m_nextFreeEntity);

					uint32_t iters = 0;
					auto fe = m_entities[m_nextFreeEntity].idx;
					while (fe != Entity::IdMask) {
						LOG_N("  --> %u", m_entities[fe].idx);
						fe = m_entities[fe].idx;
						++iters;
						if ((iters == 0U) || iters > m_freeEntities) {
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
				ChunkAllocatorStats memstats = m_chunkAllocator.GetStats();
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

		GAIA_FORCEINLINE ComponentCache& GetComponentCacheRW() {
			const auto& cc = GetComponentCache();
			return const_cast<ComponentCache&>(cc);
		}
		GAIA_FORCEINLINE const ComponentCache& GetComponentCache() {
			static ComponentCache cache;
			return cache;
		}

		GAIA_FORCEINLINE uint32_t GetWorldVersionFromWorld(const World& world) {
			return world.GetWorldVersion();
		}
		GAIA_FORCEINLINE void* AllocateChunkMemory(World& world) {
			return world.AllocateChunkMemory();
		}
		GAIA_FORCEINLINE void ReleaseChunkMemory(World& world, void* mem) {
			world.ReleaseChunkMemory(mem);
		}
	} // namespace ecs
} // namespace gaia
