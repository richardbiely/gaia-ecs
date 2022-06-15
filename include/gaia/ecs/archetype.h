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
				const ComponentMetaData* type;
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
			containers::sarray<ChunkComponentTypeList, ComponentType::CT_Count> componentTypeList;
			//! Lookup hashes of components within this archetype
			containers::sarray<ChunkComponentLookupList, ComponentType::CT_Count> componentLookupList;

			uint64_t genericHash = 0;
			uint64_t chunkHash = 0;

			//! Hash of components within this archetype - used for lookups
			uint64_t lookupHash = 0;
			//! Hash of components within this archetype - used for matching
			uint64_t matcherHash[ComponentType::CT_Count] = {0};
			//! Archetype ID - used to address the archetype directly in the world's list or archetypes
			uint32_t id = 0;
			struct {
				//! The number of entities this archetype can take (e.g 5 = 5 entities
				//! with all their components)
				uint32_t capacity : 16;

				//! True if there's a component that requires custom construction
				uint32_t hasComponentWithCustomConstruction : 1;
				//! True if there's a chunk component that requires custom construction
				uint32_t hasChunkComponentTypesWithCustomConstruction : 1;
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
#if GAIA_ECS_CHUNK_ALLOCATOR
				auto& world = const_cast<World&>(*archetype.parentWorld);
				auto pChunk = (Chunk*)AllocateChunkMemory(world);
				new (pChunk) Chunk(archetype);
#else
				auto pChunk = new Chunk(archetype);
#endif

				// Call default constructors for components that need it
				if (archetype.info.hasComponentWithCustomConstruction) {
					const auto& comp = archetype.componentTypeList[ComponentType::CT_Generic];
					const auto& look = archetype.componentLookupList[ComponentType::CT_Generic];
					for (uint32_t i = 0U; i < comp.size(); ++i) {
						if (comp[i].type->constructor == nullptr)
							continue;
						comp[i].type->constructor((void*)((char*)pChunk + look[i].offset));
					}
				}
				// Call default constructors for chunk components that need it
				if (archetype.info.hasChunkComponentTypesWithCustomConstruction) {
					const auto& comp = archetype.componentTypeList[ComponentType::CT_Chunk];
					const auto& look = archetype.componentLookupList[ComponentType::CT_Chunk];
					for (uint32_t i = 0U; i < comp.size(); ++i) {
						if (comp[i].type->constructor == nullptr)
							continue;
						comp[i].type->constructor((void*)((char*)pChunk + look[i].offset));
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
				// Call destructors for types that need it
				const auto& archetype = pChunk->header.owner;
				if (archetype.info.hasComponentWithCustomConstruction) {
					const auto& comp = archetype.componentTypeList[ComponentType::CT_Generic];
					const auto& look = archetype.componentLookupList[ComponentType::CT_Generic];
					for (uint32_t i = 0U; i < comp.size(); ++i) {
						if (comp[i].type->destructor == nullptr)
							continue;
						comp[i].type->destructor((void*)((char*)pChunk + look[i].offset));
					}
				}
				// Call destructors for chunk components which need it
				if (archetype.info.hasChunkComponentTypesWithCustomConstruction) {
					const auto& comp = archetype.componentTypeList[ComponentType::CT_Chunk];
					const auto& look = archetype.componentLookupList[ComponentType::CT_Chunk];
					for (uint32_t i = 0U; i < comp.size(); ++i) {
						if (comp[i].type->destructor == nullptr)
							continue;
						comp[i].type->destructor((void*)((char*)pChunk + look[i].offset));
					}
				}

#if GAIA_ECS_CHUNK_ALLOCATOR
				auto* world = const_cast<World*>(pChunk->header.owner.parentWorld);
				pChunk->~Chunk();
				ReleaseChunkMemory(*world, pChunk);
#else
				delete pChunk;
#endif
			}

			[[nodiscard]] static Archetype* Create(
					World& pWorld, std::span<const ComponentMetaData*> genericTypes,
					std::span<const ComponentMetaData*> chunkTypes) {
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
				uint32_t genericComponentListSize = (uint32_t)sizeof(Entity);
				for (const auto type: genericTypes) {
					genericComponentListSize += type->info.size;
					newArch->info.hasComponentWithCustomConstruction |= type->constructor != nullptr;
				}

				// Size of chunk components
				uint32_t chunkComponentListSize = 0;
				for (const auto type: chunkTypes) {
					chunkComponentListSize += type->info.size;
					newArch->info.hasChunkComponentTypesWithCustomConstruction |= type->constructor != nullptr;
				}

				// Number of components we can fit into one chunk
				auto maxGenericItemsInArchetype = (Chunk::DATA_SIZE - chunkComponentListSize) / genericComponentListSize;

				// Calculate component offsets now. Skip the header and entity IDs
				auto componentOffset = (uint32_t)sizeof(Entity) * maxGenericItemsInArchetype;
				auto alignedOffset = (uint32_t)sizeof(ChunkHeader) + componentOffset;

				// Add generic types
				for (uint32_t i = 0U; i < (uint32_t)genericTypes.size(); i++) {
					const auto* type = genericTypes[i];
					const auto alignment = type->info.alig;
					if (alignment != 0U) {
						const uint32_t padding = utils::align(alignedOffset, alignment) - alignedOffset;
						componentOffset += padding;
						alignedOffset += padding;

						// Make sure we didn't exceed the chunk size
						GAIA_ASSERT(componentOffset <= Chunk::DATA_SIZE_NORESERVE);

						// Register the type
						newArch->componentTypeList[ComponentType::CT_Generic].push_back({type});
						newArch->componentLookupList[ComponentType::CT_Generic].push_back({type->typeIndex, componentOffset});

						// Make sure the following component list is properly aligned
						componentOffset += type->info.size * maxGenericItemsInArchetype;
						alignedOffset += type->info.size * maxGenericItemsInArchetype;

						// Make sure we didn't exceed the chunk size
						GAIA_ASSERT(componentOffset <= Chunk::DATA_SIZE_NORESERVE);
					} else {
						// Register the type
						newArch->componentTypeList[ComponentType::CT_Generic].push_back({type});
						newArch->componentLookupList[ComponentType::CT_Generic].push_back({type->typeIndex, componentOffset});
					}
				}

				// Add chunk types
				for (uint32_t i = 0U; i < (uint32_t)chunkTypes.size(); i++) {
					const auto type = chunkTypes[i];
					const auto alignment = type->info.alig;
					if (alignment != 0U) {
						const uint32_t padding = utils::align(alignedOffset, alignment) - alignedOffset;
						componentOffset += padding;
						alignedOffset += padding;

						// Make sure we didn't exceed the chunk size
						GAIA_ASSERT(componentOffset <= Chunk::DATA_SIZE_NORESERVE);

						// Register the type
						newArch->componentTypeList[ComponentType::CT_Chunk].push_back({type});
						newArch->componentLookupList[ComponentType::CT_Chunk].push_back({type->typeIndex, componentOffset});

						// Make sure the following component list is properly aligned
						componentOffset += type->info.size;
						alignedOffset += type->info.size;

						// Make sure we didn't exceed the chunk size
						GAIA_ASSERT(componentOffset <= Chunk::DATA_SIZE_NORESERVE);
					} else {
						// Register the type
						newArch->componentTypeList[ComponentType::CT_Chunk].push_back({type});
						newArch->componentLookupList[ComponentType::CT_Chunk].push_back({type->typeIndex, componentOffset});
					}
				}

				newArch->info.capacity = maxGenericItemsInArchetype;
				newArch->matcherHash[ComponentType::CT_Generic] = CalculateMatcherHash(genericTypes);
				newArch->matcherHash[ComponentType::CT_Chunk] = CalculateMatcherHash(chunkTypes);

				return newArch;
			}

			[[nodiscard]] Chunk* FindOrCreateFreeChunk_Internal(containers::darray<Chunk*>& chunkArray) {
				if (!chunkArray.empty()) {
					// Look for chunks with free space back-to-front.
					// We do it this way because we always try to keep fully utilized and
					// thus only the one in the back should be free.
					auto i = (uint32_t)chunkArray.size() - 1;
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
			Archetype* FindDelEdgeArchetype(ComponentType componentType, const ComponentMetaData* type) {
				// Breath-first lookup.
				// Go through all edges first. If nothing is found check each leaf and repeat until there is a match.
				const auto& edges = edgesDel[componentType];
				const auto it = utils::find_if(edges, [type](const auto& edge) {
					return edge.type == type;
				});
				if (it != edges.end())
					return it->archetype;

				// Not found right away, search deeper
				for (const auto& edge: edges) {
					auto ret = edge.archetype->FindDelEdgeArchetype(componentType, type);
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

			[[nodiscard]] const ChunkComponentTypeList& GetComponentTypeList(ComponentType type) const {
				return componentTypeList[type];
			}

			[[nodiscard]] const ChunkComponentLookupList& GetComponentLookupList(ComponentType type) const {
				return componentLookupList[type];
			}

			/*!
			Checks if a given generic component is present on archetype.
			\return True if the generic component is present. False otherwise.
			*/
			template <typename T>
			[[nodiscard]] bool HasComponent() const {
				return HasComponent_Internal<ComponentType::CT_Generic, T>();
			}

			/*!
			Checks if all provided generic components are present on archetype.
			\return True if generic components are present. False otherwise.
			*/
			template <typename... T>
			[[nodiscard]] bool HasComponents() const {
				return HasComponents_Internal<ComponentType::CT_Generic, T...>();
			}

			/*!
			Checks if any of the provided generic components is present on archetype.
			\return True if any of the generic components is present. False otherwise.
			*/
			template <typename... T>
			[[nodiscard]] bool HasAnyComponents() const {
				return HasAnyComponents_Internal<ComponentType::CT_Generic, T...>();
			}

			/*!
			Checks if none of the provided generic components are present on archetype.
			\return True if none of the generic components are present. False otherwise.
			*/
			template <typename... T>
			[[nodiscard]] bool HasNoneComponents() const {
				return HasNoneComponents_Internal<ComponentType::CT_Generic, T...>();
			}

			/*!
			Checks if a given chunk component is present on archetype.
			\return True if the chunk component is present. False otherwise.
			*/
			template <typename T>
			[[nodiscard]] bool HasChunkComponent() const {
				return HasComponent_Internal<ComponentType::CT_Chunk, T>();
			}

			/*!
			Checks if all provided chunk components are present on archetype.
			\return True if chunk components are present. False otherwise.
			*/
			template <typename... T>
			[[nodiscard]] bool HasChunkComponents() const {
				return HasComponents_Internal<ComponentType::CT_Chunk, T...>();
			}

			/*!
			Checks if any of the provided chunk components is present on archetype.
			\return True if any of the chunk components is present. False otherwise.
			*/
			template <typename... T>
			[[nodiscard]] bool HasAnyChunkComponents() const {
				return HasAnyComponents_Internal<ComponentType::CT_Chunk, T...>();
			}

			/*!
			Checks if none of the provided chunk components are present on archetype.
			\return True if none of the chunk components are present. False otherwise.
			*/
			template <typename... T>
			[[nodiscard]] bool HasNoneChunkComponents() const {
				return HasNoneComponents_Internal<ComponentType::CT_Chunk, T...>();
			}

		private:
			template <ComponentType TComponentType, typename T>
			[[nodiscard]] bool HasComponent_Internal() const {
				using TComponent = std::decay_t<T>;
				const auto typeIndex = utils::type_info::index<TComponent>();
				return utils::has_if(GetComponentLookupList(TComponentType), [&](const auto& info) {
					return info.typeIndex == typeIndex;
				});
			}

			template <ComponentType TComponentType, typename... T>
			[[nodiscard]] bool HasComponents_Internal() const {
				return (HasComponent_Internal<TComponentType, T>() && ...);
			}

			template <ComponentType TComponentType, typename... T>
			[[nodiscard]] bool HasAnyComponents_Internal() const {
				return (HasComponent_Internal<TComponentType, T>() || ...);
			}

			template <ComponentType TComponentType, typename... T>
			[[nodiscard]] bool HasNoneComponents_Internal() const {
				return (!HasComponent_Internal<TComponentType, T>() && ...);
			}
		};

		[[nodiscard]] inline uint32_t GetWorldVersionFromArchetype(const Archetype& archetype) {
			return archetype.GetWorldVersion();
		}
		[[nodiscard]] inline uint64_t GetArchetypeMatcherHash(const Archetype& archetype, ComponentType type) {
			return archetype.GetMatcherHash(type);
		}
		[[nodiscard]] inline const ChunkComponentTypeList&
		GetArchetypeComponentTypeList(const Archetype& archetype, ComponentType type) {
			return archetype.GetComponentTypeList(type);
		}
		[[nodiscard]] inline const ChunkComponentLookupList&
		GetArchetypeComponentLookupList(const Archetype& archetype, ComponentType type) {
			return archetype.GetComponentLookupList(type);
		}
	} // namespace ecs
} // namespace gaia
