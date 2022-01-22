#pragma once
#include "../containers/sarray.h"
#include "../containers/sarray_ext.h"
#include <inttypes.h>

#include "../utils/utils_mem.h"
#include "chunk.h"
#include "chunk_allocator.h"
#include "chunk_header.h"
#include "component.h"

namespace gaia {
	namespace ecs {
		class World;
		uint32_t GetWorldVersionFromWorld(const World& world);
		void* AllocateChunkMemory(World& world);
		void ReleaseChunkMemory(World& world, void* mem);

		class Archetype final {
		private:
			friend class World;
			friend class CommandBuffer;
			friend class Chunk;
			friend struct ChunkHeader;

			//! World to which this chunk belongs to
			const World* parentWorld = nullptr;

			//! List of active chunks allocated by this archetype
			containers::darray<Chunk*> chunks;
			//! List of disabled chunks allocated by this archetype
			containers::darray<Chunk*> chunksDisabled;

			//! Description of components within this archetype
			containers::sarray<ChunkComponentTypeList, ComponentType::CT_Count> componentTypeList;
			//! Lookup hashes of components within this archetype
			containers::sarray<ChunkComponentLookupList, ComponentType::CT_Count> componentLookupList;

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
#if GAIA_DEBUG
				//! Set to true when chunks are being iterated. Used to inform of structural changes when they shouldn't happen.
				uint32_t structuralChangesLocked : 1;
#endif
			} info{};

			// Constructor is hidden. Create archetypes via Create
			Archetype() = default;

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

				// TODO: Calculate the number of entities per chunks precisely so we can
				// fit more of them into chunk on average. Currently, DATA_SIZE_RESERVED
				// is substracted but that's not optimal...

				// Size of the entity + all of its generic components
				uint32_t genericComponentListSize = (uint32_t)sizeof(Entity);
				for (const auto type: genericTypes) {
					genericComponentListSize += type->size;
					newArch->info.hasComponentWithCustomConstruction |= type->constructor != nullptr;
				}

				// Size of chunk components
				uint32_t chunkComponentListSize = 0;
				for (const auto type: chunkTypes) {
					chunkComponentListSize += type->size;
					newArch->info.hasChunkComponentTypesWithCustomConstruction |= type->constructor != nullptr;
				}

				// Number of components we can fit into one chunk
				auto maxGenericItemsInArchetype = (Chunk::DATA_SIZE - chunkComponentListSize) / genericComponentListSize;

				// If there's any component with SoA layout make sure to make the size a multiple of PackSize
				// because of SIMD requirements.
				// This means we won't be able to fit as many entities inside the chunk but in exchange we'll
				// get the ability to optimize performance via proper vectorization
				bool isAnySoA = false;
				for (const auto type: genericTypes)
					isAnySoA |= type->soa;
				if (isAnySoA)
					maxGenericItemsInArchetype -=
							(maxGenericItemsInArchetype % utils::data_layout_properties<utils::DataLayout::SoA>::PackSize);

				// Calculate component offsets now. Skip the header and entity IDs
				auto componentOffset = (uint32_t)sizeof(Entity) * maxGenericItemsInArchetype;
				auto alignedOffset = (uint32_t)sizeof(ChunkHeader) + componentOffset;

				// Add generic types
				for (uint32_t i = 0U; i < (uint32_t)genericTypes.size(); i++) {
					const auto a = genericTypes[i]->alig;
					if (a != 0) {
						const uint32_t padding = utils::align(alignedOffset, a) - alignedOffset;
						componentOffset += padding;
						alignedOffset += padding;

						// Make sure we didn't exceed the chunk size
						GAIA_ASSERT(componentOffset <= Chunk::DATA_SIZE);

						// Register the type
						newArch->componentTypeList[ComponentType::CT_Generic].push_back({genericTypes[i]});
						newArch->componentLookupList[ComponentType::CT_Generic].push_back(
								{genericTypes[i]->typeIndex, componentOffset});

						// Make sure the following component list is properly aligned
						componentOffset += genericTypes[i]->size * maxGenericItemsInArchetype;
						alignedOffset += genericTypes[i]->size * maxGenericItemsInArchetype;

						// Make sure we didn't exceed the chunk size
						GAIA_ASSERT(componentOffset <= Chunk::DATA_SIZE);
					} else {
						// Register the type
						newArch->componentTypeList[ComponentType::CT_Generic].push_back({genericTypes[i]});
						newArch->componentLookupList[ComponentType::CT_Generic].push_back(
								{genericTypes[i]->typeIndex, componentOffset});
					}
				}

				// Add chunk types
				for (uint32_t i = 0U; i < (uint32_t)chunkTypes.size(); i++) {
					const auto a = chunkTypes[i]->alig;
					if (a != 0U) {
						const uint32_t padding = utils::align(alignedOffset, a) - alignedOffset;
						componentOffset += padding;
						alignedOffset += padding;

						// Make sure we didn't exceed the chunk size
						GAIA_ASSERT(componentOffset <= Chunk::DATA_SIZE);

						// Register the type
						newArch->componentTypeList[ComponentType::CT_Chunk].push_back({chunkTypes[i]});
						newArch->componentLookupList[ComponentType::CT_Chunk].push_back(
								{chunkTypes[i]->typeIndex, componentOffset});

						// Make sure the following component list is properly aligned
						componentOffset += chunkTypes[i]->size;
						alignedOffset += chunkTypes[i]->size;

						// Make sure we didn't exceed the chunk size
						GAIA_ASSERT(componentOffset <= Chunk::DATA_SIZE);
					} else {
						// Register the type
						newArch->componentTypeList[ComponentType::CT_Chunk].push_back({chunkTypes[i]});
						newArch->componentLookupList[ComponentType::CT_Chunk].push_back(
								{chunkTypes[i]->typeIndex, componentOffset});
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

			void RemoveChunk(Chunk* pChunk) {
				ReleaseChunk(pChunk);
				utils::erase_fast(chunks, utils::get_index(pChunk->IsDisabled() ? chunksDisabled : chunks, pChunk));
			}

		public:
			~Archetype() {
				// Delete all archetype chunks
				for (auto* pChunk: chunks)
					ReleaseChunk(pChunk);
				for (auto* pChunk: chunksDisabled)
					ReleaseChunk(pChunk);
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

			template <typename... T>
			[[nodiscard]] bool HasComponents() const {
				return HasComponents_Internal<ComponentType::CT_Generic, T...>();
			}
			template <typename... T>
			[[nodiscard]] bool HasAnyComponents() const {
				return HasAnyComponents_Internal<ComponentType::CT_Generic, T...>();
			}
			template <typename... T>
			[[nodiscard]] bool HasNoneComponents() const {
				return HasNoneComponents_Internal<ComponentType::CT_Generic, T...>();
			}

			template <typename... T>
			[[nodiscard]] bool HasChunkComponents() const {
				return HasComponents_Internal<ComponentType::CT_Chunk, T...>();
			}
			template <typename... T>
			[[nodiscard]] bool HasAnyChunkComponents() const {
				return HasAnyComponents_Internal<ComponentType::CT_Chunk, T...>();
			}
			template <typename... T>
			[[nodiscard]] bool HasNoneChunkComponents() const {
				return HasNoneComponents_Internal<ComponentType::CT_Chunk, T...>();
			}

		private:
			template <ComponentType TComponentType, typename T>
			[[nodiscard]] bool HasComponent_Internal() const {
				const ComponentMetaData* type = g_ComponentCache.GetOrCreateComponentMetaType<T>();
				return utils::has_if(componentTypeList[TComponentType], [type](const auto& info) {
					return info.type == type;
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