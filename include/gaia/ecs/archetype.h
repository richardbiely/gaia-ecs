#pragma once
#include <array>
#include <inttypes.h>
#include <vector>

#include "../utils/utils_mem.h"
#include "chunk.h"
#include "chunk_allocator.h"
#include "chunk_header.h"
#include "component.h"

namespace gaia {
	namespace ecs {
		class World;
		uint32_t GetWorldVersionFromWorld(const World& world);

		class Archetype final {
		private:
			friend class World;
			friend class EntityCommandBuffer;
			friend class Chunk;
			friend struct ChunkHeader;

			//! World to which this chunk belongs to
			const World* parentWorld = nullptr;
			//! List of chunks allocated by this archetype
			std::vector<Chunk*> chunks;
			//! Description of components within this archetype
			std::array<ChunkComponentList, ComponentType::CT_Count> componentList;

			//! Hash of components within this archetype
			uint64_t componentsHash = 0;
			//! The number of entities this archetype can take (e.g 5 = 5 entities
			//! with all their components)
			uint16_t capacity = 0;

			// Constructor is hidden. Create archetypes via Create
			Archetype() = default;

			[[nodiscard]] static Chunk* AllocateChunk(const Archetype& archetype) {
#if ECS_CHUNK_ALLOCATOR
				auto pChunk = (Chunk*)g_ChunkAllocator.Alloc();
				new (pChunk) Chunk(archetype);
#else
				auto pChunk = new Chunk(archetype);
#endif
				return pChunk;
			}

			static void ReleaseChunk(Chunk* pChunk) {
#if ECS_CHUNK_ALLOCATOR
				pChunk->~Chunk();
				g_ChunkAllocator.Free(pChunk);
#else
				delete pChunk;
#endif
			}

			[[nodiscard]] static Archetype* Create(
					World& pWorld, tcb::span<const ComponentMetaData*> genericTypes,
					tcb::span<const ComponentMetaData*> chunkTypes) {
				auto newArch = new Archetype();
				newArch->parentWorld = &pWorld;

				// Size of the entity + all of its generic components
				uint32_t genericComponentListSize = (uint32_t)sizeof(Entity);
				for (const auto type: genericTypes)
					genericComponentListSize += type->size;
				// Size of chunk components
				uint32_t chunkComponentListSize = 0;
				for (const auto type: chunkTypes)
					chunkComponentListSize += type->size;

				// Number of components we can fit into one chunk
				const auto maxGenericItemsInArchetype =
						(Chunk::DATA_SIZE - chunkComponentListSize) /
						genericComponentListSize;

				// Calculate component offsets now. Skip the header and entity IDs
				auto componentOffset =
						(uint32_t)sizeof(Entity) * maxGenericItemsInArchetype;
				auto alignedOffset = (uint32_t)sizeof(ChunkHeader) + componentOffset;

					// Add generic types
					for (uint32_t i = 0u; i < (uint32_t)genericTypes.size(); i++) {
						const auto a = genericTypes[i]->alig;
							if (a != 0) {
								const uint32_t padding =
										utils::align(alignedOffset, a) - alignedOffset;
								componentOffset += padding;
								alignedOffset += padding;

								// Make sure we didn't exceed the size of chunk
								assert(componentOffset <= Chunk::DATA_SIZE);
						}

						newArch->componentList[ComponentType::CT_Generic].push_back(
								{genericTypes[i], componentOffset});

							// Make sure the following component list is properly aligned
							if (a != 0) {
								componentOffset +=
										genericTypes[i]->size * maxGenericItemsInArchetype;
								alignedOffset +=
										genericTypes[i]->size * maxGenericItemsInArchetype;

								// Make sure we didn't exceed the size of chunk
								assert(componentOffset <= Chunk::DATA_SIZE);
						}
					}

					// Add chunk types
					for (uint32_t i = 0; i < (uint32_t)chunkTypes.size(); i++) {
						const auto a = genericTypes[i]->alig;
							if (a != 0) {
								const uint32_t padding =
										utils::align(alignedOffset, a) - alignedOffset;
								componentOffset += padding;
								alignedOffset += padding;

								// Make sure we didn't exceed the size of chunk
								assert(componentOffset <= Chunk::DATA_SIZE);
						}

						newArch->componentList[ComponentType::CT_Chunk].push_back(
								{chunkTypes[i], componentOffset});

							// Make sure the following component list is properly aligned
							if (a != 0) {
								componentOffset += chunkTypes[i]->size;
								alignedOffset += chunkTypes[i]->size;

								// Make sure we didn't exceed the size of chunk
								assert(componentOffset <= Chunk::DATA_SIZE);
						}
					}

				// Create a chunk in the new archetype
				auto chunk = AllocateChunk(*newArch);
				newArch->capacity = maxGenericItemsInArchetype;
				newArch->chunks.push_back(chunk);

				return newArch;
			}

			[[nodiscard]] Chunk* FindOrCreateChunk() {
				assert(!chunks.empty());

				// So long we have enough space in the last chunk we use it
				auto lastChunk = chunks.back();
				if (lastChunk && !lastChunk->IsFull())
					return lastChunk;

					// Try previous chunks to fill empty spaces
					for (uint32_t i = 0; i < (uint32_t)chunks.size() - 1; i++) {
						auto pChunk = chunks[i];
						if (pChunk && !pChunk->IsFull())
							return pChunk;
					}

				// No free space found anywhere. Let's create a new one
				auto* pChunk = AllocateChunk(*this);
				chunks.push_back(pChunk);
				return pChunk;
			}

			void RemoveChunk(Chunk* pChunk) {
				ReleaseChunk(pChunk);
				auto it = utils::find(chunks, pChunk);
				assert(it != chunks.end());
				chunks.erase(it);
			}

		public:
			~Archetype() {
				// Delete all archetype chunks
				for (auto* pChunk: chunks)
					ReleaseChunk(pChunk);
			}

			uint32_t GetWorldVersion() const {
				return GetWorldVersionFromWorld(*parentWorld);
			}

			uint16_t GetCapacity() const { return capacity; }

			const ChunkComponentList& GetComponentList(ComponentType type) const {
				return componentList[type];
			}

			template <typename... T> [[nodiscard]] bool HasComponents() const {
				return HasComponents_Internal<ComponentType::CT_Generic, T...>();
			}
			template <typename... T> [[nodiscard]] bool HasAnyComponents() const {
				return HasAnyComponents_Internal<ComponentType::CT_Generic, T...>();
			}
			template <typename... T> [[nodiscard]] bool HasNoneComponents() const {
				return HasNoneComponents_Internal<ComponentType::CT_Generic, T...>();
			}

			template <typename... T> [[nodiscard]] bool HasChunkComponents() const {
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
			template <ComponentType TYPE, typename T>
			[[nodiscard]] bool HasComponent_Internal() const {
				const ComponentMetaData* type = GetOrCreateComponentMetaType<T>();
				return utils::has_if(componentList[TYPE], [type](const auto& info) {
					return info.type == type;
				});
			}

			template <ComponentType TYPE, typename... T>
			[[nodiscard]] bool HasComponents_Internal() const {
				return (HasComponent_Internal<TYPE, T>() && ...);
			}

			template <ComponentType TYPE, typename... T>
			[[nodiscard]] bool HasAnyComponents_Internal() const {
				return (HasComponent_Internal<TYPE, T>() || ...);
			}

			template <ComponentType TYPE, typename... T>
			[[nodiscard]] bool HasNoneComponents_Internal() const {
				return (!HasComponent_Internal<TYPE, T>() && ...);
			}
		};

		inline uint32_t GetWorldVersionFromArchetype(const Archetype& archetype) {
			return archetype.GetWorldVersion();
		}
		inline uint16_t GetArchetypeCapacity(const Archetype& archetype) {
			return archetype.GetCapacity();
		}
		inline const ChunkComponentList&
		GetArchetypeComponentList(const Archetype& archetype, ComponentType type) {
			return archetype.GetComponentList(type);
		}
	} // namespace ecs
} // namespace gaia