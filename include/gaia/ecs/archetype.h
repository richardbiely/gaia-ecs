#pragma once
#include <cinttypes>

#include "../containers/darray.h"
#include "../containers/sarray.h"
#include "../containers/sarray_ext.h"
#include "../utils/mem.h"
#include "chunk.h"
#include "chunk_allocator.h"
#include "chunk_header.h"
#include "component.h"
#include "component_cache.h"

namespace gaia {
	namespace ecs {
		class World;

		ComponentCache& GetComponentCache(World& world);
		const ComponentCache& GetComponentCache(const World& world);
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
				Archetype* archetype;
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
			uint64_t lookupHash = 0;
			//! Hash of components within this archetype - used for matching
			uint64_t matcherHash[ComponentType::CT_Count] = {0};
			//! Archetype ID - used to address the archetype directly in the world's list or archetypes
			uint32_t id = 0;
			struct {
				//! The number of entities this archetype can take (e.g 5 = 5 entities with all their components)
				uint32_t capacity : 16;
				//! True if there's a component that requires custom construction or destruction
				uint32_t hasComponentWithCustomCreation : 1;
				//! Set to true when chunks are being iterated. Used to inform of structural changes when they shouldn't happen.
				uint32_t structuralChangesLocked : 1;
			} info{};

			// Constructor is hidden. Create archetypes via Create
			Archetype() = default;

			/*!
			Allocates memory for a new chunk.
			\param archetype Archetype of the chunk we want to allocate
			\return Newly allocated chunk
			*/
			[[nodiscard]] static Chunk* AllocateChunk(const Archetype& archetype) {
				auto& world = const_cast<World&>(*archetype.parentWorld);
#if GAIA_ECS_CHUNK_ALLOCATOR
				auto pChunk = (Chunk*)AllocateChunkMemory(world);
				new (pChunk) Chunk(archetype);
#else
				auto pChunk = new Chunk(archetype);
#endif

				// Call default constructors for components that need it
				if (archetype.info.hasComponentWithCustomCreation) {
					const auto& look = archetype.componentLookupData[ComponentType::CT_Generic];
					for (size_t i = 0; i < look.size(); ++i) {
						const auto* infoCreate = GetComponentCache(world).GetComponentCreateInfoFromIdx(look[i].infoIndex);
						if (infoCreate->constructor == nullptr)
							continue;
						infoCreate->constructor((void*)((uint8_t*)pChunk + look[i].offset));
					}
				}
				// Call default constructors for chunk components that need it
				if (archetype.info.hasComponentWithCustomCreation) {
					const auto& look = archetype.componentLookupData[ComponentType::CT_Chunk];
					for (size_t i = 0; i < look.size(); ++i) {
						const auto* infoCreate = GetComponentCache(world).GetComponentCreateInfoFromIdx(look[i].infoIndex);
						if (infoCreate->constructor == nullptr)
							continue;
						infoCreate->constructor((void*)((uint8_t*)pChunk + look[i].offset));
					}
				}

				pChunk->header.items.capacity = archetype.info.capacity;
				return pChunk;
			}

			/*!
			Releases all memory allocated by \param pChunk.
			\param pChunk Chunk which we want to destroy
			*/
			static void ReleaseChunk(Chunk* pChunk) {
				const auto& archetype = pChunk->header.owner;
				auto& world = const_cast<World&>(*archetype.parentWorld);

				// Call destructors for types that need it
				if (archetype.info.hasComponentWithCustomCreation) {
					const auto& look = archetype.componentLookupData[ComponentType::CT_Generic];
					for (size_t i = 0; i < look.size(); ++i) {
						const auto* infoCreate = GetComponentCache(world).GetComponentCreateInfoFromIdx(look[i].infoIndex);
						if (infoCreate->destructor == nullptr)
							continue;
						infoCreate->destructor((void*)((uint8_t*)pChunk + look[i].offset));
					}
				}
				// Call destructors for chunk components which need it
				if (archetype.info.hasComponentWithCustomCreation) {
					const auto& look = archetype.componentLookupData[ComponentType::CT_Chunk];
					for (size_t i = 0; i < look.size(); ++i) {
						const auto* infoCreate = GetComponentCache(world).GetComponentCreateInfoFromIdx(look[i].infoIndex);
						if (infoCreate->destructor == nullptr)
							continue;
						infoCreate->destructor((void*)((uint8_t*)pChunk + look[i].offset));
					}
				}

#if GAIA_ECS_CHUNK_ALLOCATOR
				pChunk->~Chunk();
				ReleaseChunkMemory(world, pChunk);
#else
				delete pChunk;
#endif
			}

			[[nodiscard]] static Archetype*
			Create(World& pWorld, std::span<const ComponentInfo*> infosGeneric, std::span<const ComponentInfo*> infosChunk) {
				auto newArch = new Archetype();
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

				// TODO: Calculate the number of entities per chunks precisely so we can
				// fit more of them into chunk on average. Currently, DATA_SIZE_RESERVED
				// is substracted but that's not optimal...

				// Size of the entity + all of its generic components
				size_t genericComponentListSize = sizeof(Entity);
				for (const auto* info: infosGeneric) {
					genericComponentListSize += info->properties.size;
					newArch->info.hasComponentWithCustomCreation |= info->properties.size != 0 && info->properties.soa != 0;
				}

				// Size of chunk components
				size_t chunkComponentListSize = 0;
				for (const auto* info: infosChunk) {
					chunkComponentListSize += info->properties.size;
					newArch->info.hasComponentWithCustomCreation |= info->properties.size != 0 && info->properties.soa != 0;
				}

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

				newArch->info.capacity = maxGenericItemsInArchetype;
				newArch->matcherHash[ComponentType::CT_Generic] = CalculateMatcherHash(infosGeneric);
				newArch->matcherHash[ComponentType::CT_Chunk] = CalculateMatcherHash(infosChunk);

				return newArch;
			}

			[[nodiscard]] Chunk* FindOrCreateFreeChunk_Internal(containers::darray<Chunk*>& chunkArray) {
				if (!chunkArray.empty()) {
					// Look for chunks with free space back-to-front.
					// We do it this way because we always try to keep fully utilized and
					// thus only the one in the back should be free.
					auto i = chunkArray.size() - 1;
					do {
						auto pChunk = chunkArray[i];
						GAIA_ASSERT(pChunk != nullptr);
						if (!pChunk->IsFull())
							return pChunk;
					} while (i-- > 0);
				}

				// No free space found anywhere. Let's create a new one.
				auto* pChunk = AllocateChunk(*this);
				pChunk->header.index = (uint32_t)chunkArray.size();
				chunkArray.push_back(pChunk);
				return pChunk;
			}

			//! Tries to locate a chunk that has some space left for a new entity.
			//! If not found a new chunk is created
			[[nodiscard]] Chunk* FindOrCreateFreeChunk() {
				return FindOrCreateFreeChunk_Internal(chunks);
			}

			//! Tries to locate a chunk for disabled entities that has some space left for a new one.
			//! If not found a new chunk is created
			[[nodiscard]] Chunk* FindOrCreateFreeChunkDisabled() {
				if (auto* pChunk = FindOrCreateFreeChunk_Internal(chunksDisabled)) {
					pChunk->header.info.disabled = true;
					return pChunk;
				}

				return nullptr;
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
			Archetype* FindDelEdgeArchetype(ComponentType type, const ComponentInfo* info) {
				// Breath-first lookup.
				// Go through all edges first. If nothing is found check each leaf and repeat until there is a match.
				const auto& edges = edgesDel[type];
				const auto it = utils::find_if(edges, [info](const auto& edge) {
					return edge.info == info;
				});
				if (it != edges.end())
					return it->archetype;

				// Not found right away, search deeper
				for (const auto& edge: edges) {
					auto ret = edge.archetype->FindDelEdgeArchetype(type, info);
					if (ret != nullptr)
						return edge.archetype;
				}

				return nullptr;
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

			[[nodiscard]] const World& GetWorld() const {
				return *parentWorld;
			}

			[[nodiscard]] uint32_t GetWorldVersion() const {
				return GetWorldVersionFromWorld(*parentWorld);
			}

			[[nodiscard]] uint32_t GetCapacity() const {
				return info.capacity;
			}

			[[nodiscard]] uint64_t GetMatcherHash(ComponentType type) const {
				return matcherHash[type];
			}

			[[nodiscard]] const ComponentInfoList& GetComponentInfoList(ComponentType type) const {
				return componentInfos[type];
			}

			[[nodiscard]] const ComponentLookupList& GetComponentLookupList(ComponentType type) const {
				return componentLookupData[type];
			}

			/*!
			Checks if a given component is present on the archetype.
			\return True if the component is present. False otherwise.
			*/
			template <typename T>
			[[nodiscard]] bool HasComponent() const {
				return HasComponent_Internal<T>();
			}

		private:
			template <typename T>
			[[nodiscard]] bool HasComponent_Internal() const {
				using U = typename DeduceComponent<T>::Type;
				const auto infoIndex = utils::type_info::index<U>();

				if constexpr (IsGenericComponent<T>::value) {
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

		[[nodiscard]] inline uint32_t GetWorldVersionFromArchetype(const Archetype& archetype) {
			return archetype.GetWorldVersion();
		}
		[[nodiscard]] inline uint64_t GetArchetypeMatcherHash(const Archetype& archetype, ComponentType type) {
			return archetype.GetMatcherHash(type);
		}
		[[nodiscard]] inline const ComponentInfoList&
		GetArchetypeComponentInfoList(const Archetype& archetype, ComponentType type) {
			return archetype.GetComponentInfoList(type);
		}
		[[nodiscard]] inline const ComponentLookupList&
		GetArchetypeComponentLookupList(const Archetype& archetype, ComponentType type) {
			return archetype.GetComponentLookupList(type);
		}
	} // namespace ecs
} // namespace gaia
