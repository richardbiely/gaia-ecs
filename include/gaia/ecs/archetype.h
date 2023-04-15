#pragma once
#include "../containers/darray.h"
#include "../containers/sarray.h"
#include "../containers/sarray_ext.h"
#include "../utils/hashing_policy.h"
#include "../utils/mem.h"
#include "chunk.h"
#include "chunk_allocator.h"
#include "chunk_header.h"
#include "component.h"
#include "component_cache.h"

namespace gaia {
	namespace ecs {
		class World;

		const ComponentCache& GetComponentCache();
		ComponentCache& GetComponentCacheRW();

		uint32_t GetWorldVersionFromWorld(const World& world);
		void* AllocateChunkMemory(World& world);
		void ReleaseChunkMemory(World& world, void* mem);

		class Archetype final {
		private:
			friend class World;
			friend class CommandBuffer;
			friend class Chunk;
			friend struct ChunkHeader;

#if GAIA_ARCHETYPE_GRAPH
			struct ArchetypeGraphEdge {
				const ComponentInfo* info;
				Archetype* pArchetype;
			};
#endif

			//! World to which this chunk belongs to
			const World* parentWorld = nullptr;

			//! List of active chunks allocated by this archetype
			containers::darray<Chunk*> chunks;
			//! List of disabled chunks allocated by this archetype
			containers::darray<Chunk*> chunksDisabled;

#if GAIA_ARCHETYPE_GRAPH
			//! List of edges in the archetype graph when adding components
			containers::darray<ArchetypeGraphEdge> edgesAdd[ComponentType::CT_Count];
			//! List of edges in the archetype graph when removing components
			containers::darray<ArchetypeGraphEdge> edgesDel[ComponentType::CT_Count];
#endif

			//! Description of components within this archetype
			containers::sarray<ComponentInfoList, ComponentType::CT_Count> componentInfos;
			//! Lookup hashes of components within this archetype
			containers::sarray<ComponentLookupList, ComponentType::CT_Count> componentLookupData;

			uint64_t genericHash = 0;
			uint64_t chunkHash = 0;

			//! Hash of components within this archetype - used for lookups
			utils::direct_hash_key lookupHash{};
			//! Hash of components within this archetype - used for matching
			uint64_t matcherHash[ComponentType::CT_Count] = {0};
			//! Archetype ID - used to address the archetype directly in the world's list or archetypes
			uint32_t id = 0;
			struct {
				//! The number of entities this archetype can take (e.g 5 = 5 entities with all their components)
				uint32_t capacity : 16;
				//! True if there's a component that requires custom construction or destruction
				uint32_t hasComponentWithCustomCreation : 1;
				//! Updated when chunks are being iterated. Used to inform of structural changes when they shouldn't happen.
				uint32_t structuralChangesLocked : 4;
			} info{};

			// Constructor is hidden. Create archetypes via Create
			Archetype() = default;

			/*!
			Allocates memory for a new chunk.
			\param archetype Archetype of the chunk we want to allocate
			\return Newly allocated chunk
			*/
			GAIA_NODISCARD static Chunk* AllocateChunk(const Archetype& archetype) {
#if GAIA_ECS_CHUNK_ALLOCATOR
				auto& world = const_cast<World&>(*archetype.parentWorld);

				auto* pChunk = (Chunk*)AllocateChunkMemory(world);
				new (pChunk) Chunk(archetype);
#else
				auto pChunk = new Chunk(archetype);
#endif

				auto callConstructors = [&](ComponentType ct) {
					const auto& cc = GetComponentCache();
					const auto& look = archetype.componentLookupData[ct];
					for (size_t i = 0; i < look.size(); ++i) {
						const auto& infoCreate = cc.GetComponentCreateInfoFromIdx(look[i].infoIndex);
						if (infoCreate.constructor == nullptr)
							continue;
						auto* first = (void*)((uint8_t*)pChunk + look[i].offset);
						infoCreate.constructor(first, archetype.info.capacity);
					}
				};

				// Call constructors for components that need it
				if (archetype.info.hasComponentWithCustomCreation) {
					callConstructors(ComponentType::CT_Generic);
					callConstructors(ComponentType::CT_Chunk);
				}

				pChunk->header.capacity = archetype.info.capacity;
				return pChunk;
			}

			/*!
			Releases all memory allocated by \param pChunk.
			\param pChunk Chunk which we want to destroy
			*/
			static void ReleaseChunk(Chunk* pChunk) {
				const auto& archetype = pChunk->header.owner;

				auto callDestructors = [&](ComponentType ct) {
					const auto& cc = GetComponentCache();
					const auto& look = archetype.componentLookupData[ct];
					for (size_t i = 0; i < look.size(); ++i) {
						const auto& infoCreate = cc.GetComponentCreateInfoFromIdx(look[i].infoIndex);
						if (infoCreate.destructor == nullptr)
							continue;
						auto* first = (void*)((uint8_t*)pChunk + look[i].offset);
						infoCreate.destructor(first, archetype.info.capacity);
					}
				};

				// Call destructors for components that need it
				if (archetype.info.hasComponentWithCustomCreation) {
					callDestructors(ComponentType::CT_Generic);
					callDestructors(ComponentType::CT_Chunk);
				}

#if GAIA_ECS_CHUNK_ALLOCATOR
				auto& world = const_cast<World&>(*archetype.parentWorld);
				pChunk->~Chunk();
				ReleaseChunkMemory(world, pChunk);
#else
				delete pChunk;
#endif
			}

			GAIA_NODISCARD static Archetype*
			Create(World& pWorld, std::span<const ComponentInfo*> infosGeneric, std::span<const ComponentInfo*> infosChunk) {
				auto* newArch = new Archetype();
				newArch->parentWorld = &pWorld;

#if GAIA_ARCHETYPE_GRAPH
				// Preallocate arrays for graph edges
				// Generic components are going to be more common so we prepare bigger arrays for them.
				// Chunk components are expected to be very rare so only a small buffer is preallocated.
				newArch->edgesAdd[ComponentType::CT_Generic].reserve(8);
				newArch->edgesAdd[ComponentType::CT_Chunk].reserve(1);
				newArch->edgesDel[ComponentType::CT_Generic].reserve(8);
				newArch->edgesDel[ComponentType::CT_Chunk].reserve(1);
#endif

				// Size of the entity + all of its generic components
				size_t genericComponentListSize = sizeof(Entity);
				for (const auto* info: infosGeneric) {
					genericComponentListSize += info->properties.size;
					newArch->info.hasComponentWithCustomCreation |= (info->properties.size != 0U) && info->properties.soa;
				}

				// Size of chunk components
				size_t chunkComponentListSize = 0;
				for (const auto* info: infosChunk) {
					chunkComponentListSize += info->properties.size;
					newArch->info.hasComponentWithCustomCreation |= (info->properties.size != 0U) && info->properties.soa;
				}

				// TODO: Calculate the number of entities per chunks precisely so we can
				// fit more of them into chunk on average. Currently, DATA_SIZE_RESERVED
				// is substracted but that's not optimal...

				// Number of components we can fit into one chunk
				auto maxGenericItemsInArchetype = (Chunk::DATA_SIZE - chunkComponentListSize) / genericComponentListSize;

				// Calculate component offsets now. Skip the header and entity IDs
				auto componentOffset = sizeof(Entity) * maxGenericItemsInArchetype;
				auto alignedOffset = sizeof(ChunkHeader) + componentOffset;

				// Add generic infos
				for (size_t i = 0; i < infosGeneric.size(); i++) {
					const auto* info = infosGeneric[i];
					const auto alignment = info->properties.alig;
					if (alignment != 0) {
						const size_t padding = utils::align(alignedOffset, alignment) - alignedOffset;
						componentOffset += padding;
						alignedOffset += padding;

						// Make sure we didn't exceed the chunk size
						GAIA_ASSERT(componentOffset <= Chunk::DATA_SIZE_NORESERVE);

						// Register the component info
						newArch->componentInfos[ComponentType::CT_Generic].push_back(info);
						newArch->componentLookupData[ComponentType::CT_Generic].push_back(
								{info->infoIndex, (uint32_t)componentOffset});

						// Make sure the following component list is properly aligned
						componentOffset += info->properties.size * maxGenericItemsInArchetype;
						alignedOffset += info->properties.size * maxGenericItemsInArchetype;

						// Make sure we didn't exceed the chunk size
						GAIA_ASSERT(componentOffset <= Chunk::DATA_SIZE_NORESERVE);
					} else {
						// Register the component info
						newArch->componentInfos[ComponentType::CT_Generic].push_back(info);
						newArch->componentLookupData[ComponentType::CT_Generic].push_back(
								{info->infoIndex, (uint32_t)componentOffset});
					}
				}

				// Add chunk infos
				for (size_t i = 0; i < infosChunk.size(); i++) {
					const auto* info = infosChunk[i];
					const auto alignment = info->properties.alig;
					if (alignment != 0) {
						const size_t padding = utils::align(alignedOffset, alignment) - alignedOffset;
						componentOffset += padding;
						alignedOffset += padding;

						// Make sure we didn't exceed the chunk size
						GAIA_ASSERT(componentOffset <= Chunk::DATA_SIZE_NORESERVE);

						// Register the component info
						newArch->componentInfos[ComponentType::CT_Chunk].push_back(info);
						newArch->componentLookupData[ComponentType::CT_Chunk].push_back(
								{info->infoIndex, (uint32_t)componentOffset});

						// Make sure the following component list is properly aligned
						componentOffset += info->properties.size;
						alignedOffset += info->properties.size;

						// Make sure we didn't exceed the chunk size
						GAIA_ASSERT(componentOffset <= Chunk::DATA_SIZE_NORESERVE);
					} else {
						// Register the component info
						newArch->componentInfos[ComponentType::CT_Chunk].push_back(info);
						newArch->componentLookupData[ComponentType::CT_Chunk].push_back(
								{info->infoIndex, (uint32_t)componentOffset});
					}
				}

				newArch->info.capacity = (uint32_t)maxGenericItemsInArchetype;
				newArch->matcherHash[ComponentType::CT_Generic] = CalculateMatcherHash(infosGeneric);
				newArch->matcherHash[ComponentType::CT_Chunk] = CalculateMatcherHash(infosChunk);

				return newArch;
			}

			GAIA_NODISCARD Chunk* FindOrCreateFreeChunk_Internal(containers::darray<Chunk*>& chunkArray) const {
				const auto chunkCnt = chunkArray.size();

				if (chunkCnt > 0) {
					// Look for chunks with free space back-to-front.
					// We do it this way because we always try to keep fully utilized and
					// thus only the one in the back should be free.
					auto i = chunkCnt - 1;
					do {
						auto* pChunk = chunkArray[i];
						GAIA_ASSERT(pChunk != nullptr);
						if (!pChunk->IsFull())
							return pChunk;
					} while (i-- > 0);
				}

				GAIA_ASSERT(chunkCnt < (uint32_t)UINT16_MAX);

				// No free space found anywhere. Let's create a new one.
				auto* pChunk = AllocateChunk(*this);
				pChunk->header.index = (uint16_t)chunkCnt;
				chunkArray.push_back(pChunk);
				return pChunk;
			}

			//! Tries to locate a chunk that has some space left for a new entity.
			//! If not found a new chunk is created
			GAIA_NODISCARD Chunk* FindOrCreateFreeChunk() {
				return FindOrCreateFreeChunk_Internal(chunks);
			}

			//! Tries to locate a chunk for disabled entities that has some space left for a new one.
			//! If not found a new chunk is created
			GAIA_NODISCARD Chunk* FindOrCreateFreeChunkDisabled() {
				auto* pChunk = FindOrCreateFreeChunk_Internal(chunksDisabled);
				pChunk->header.disabled = true;
				return pChunk;
			}

			/*!
			Removes a chunk from the list of chunks managed by their achetype.
			\param pChunk Chunk to remove from the list of managed archetypes
			*/
			void RemoveChunk(Chunk* pChunk) {
				const bool isDisabled = pChunk->IsDisabled();
				const auto chunkIndex = pChunk->header.index;

				ReleaseChunk(pChunk);

				auto remove = [&](auto& chunkArray) {
					if (chunkArray.size() > 1)
						chunkArray.back()->header.index = chunkIndex;
					GAIA_ASSERT(chunkIndex == utils::get_index(chunkArray, pChunk));
					utils::erase_fast(chunkArray, chunkIndex);
				};

				if (isDisabled)
					remove(chunksDisabled);
				else
					remove(chunks);
			}

#if GAIA_ARCHETYPE_GRAPH
			Archetype* FindDelEdgeArchetype(ComponentType type, const ComponentInfo* info) const {
				const auto& edges = edgesDel[type];
				const auto it = utils::find_if(edges, [info](const auto& edge) {
					return edge.info == info;
				});
				return it != edges.end() ? it->pArchetype : nullptr;
			}
#endif

		public:
			~Archetype() {
				// Delete all archetype chunks
				for (auto* pChunk: chunks)
					ReleaseChunk(pChunk);
				for (auto* pChunk: chunksDisabled)
					ReleaseChunk(pChunk);
			}

			GAIA_NODISCARD const World& GetWorld() const {
				return *parentWorld;
			}

			GAIA_NODISCARD uint32_t GetWorldVersion() const {
				return GetWorldVersionFromWorld(*parentWorld);
			}

			GAIA_NODISCARD uint32_t GetCapacity() const {
				return info.capacity;
			}

			GAIA_NODISCARD uint64_t GetMatcherHash(ComponentType type) const {
				return matcherHash[type];
			}

			GAIA_NODISCARD const ComponentInfoList& GetComponentInfoList(ComponentType type) const {
				return componentInfos[type];
			}

			GAIA_NODISCARD const ComponentLookupList& GetComponentLookupList(ComponentType type) const {
				return componentLookupData[type];
			}

			/*!
			Checks if a given component is present on the archetype.
			\return True if the component is present. False otherwise.
			*/
			template <typename T>
			GAIA_NODISCARD bool HasComponent() const {
				return HasComponent_Internal<T>();
			}

		private:
			template <typename T>
			GAIA_NODISCARD bool HasComponent_Internal() const {
				using U = typename DeduceComponent<T>::Type;
				const auto infoIndex = utils::type_info::index<U>();

				if constexpr (IsGenericComponent<T>) {
					return utils::has_if(GetComponentLookupList(ComponentType::CT_Generic), [&](const auto& info) {
						return info.infoIndex == infoIndex;
					});
				} else {
					return utils::has_if(GetComponentLookupList(ComponentType::CT_Chunk), [&](const auto& info) {
						return info.infoIndex == infoIndex;
					});
				}
			}
		};

		GAIA_NODISCARD GAIA_FORCEINLINE uint32_t GetWorldVersionFromArchetype(const Archetype& archetype) {
			return archetype.GetWorldVersion();
		}
		GAIA_NODISCARD GAIA_FORCEINLINE uint64_t GetArchetypeMatcherHash(const Archetype& archetype, ComponentType type) {
			return archetype.GetMatcherHash(type);
		}
		GAIA_NODISCARD GAIA_FORCEINLINE const ComponentInfo* GetComponentInfoFromIdx(uint32_t componentIdx) {
			return GetComponentCache().GetComponentInfoFromIdx(componentIdx);
		}
		GAIA_NODISCARD GAIA_FORCEINLINE const ComponentInfoList&
		GetArchetypeComponentInfoList(const Archetype& archetype, ComponentType type) {
			return archetype.GetComponentInfoList(type);
		}
		GAIA_NODISCARD GAIA_FORCEINLINE const ComponentLookupList&
		GetArchetypeComponentLookupList(const Archetype& archetype, ComponentType type) {
			return archetype.GetComponentLookupList(type);
		}
	} // namespace ecs
} // namespace gaia
