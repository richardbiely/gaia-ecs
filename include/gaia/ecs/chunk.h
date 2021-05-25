#pragma once
#include <algorithm>
#include <cassert>
#include <inttypes.h>
#include <iterator>
#include <stdint.h>
#include <type_traits>
#include <vector>

#include "../utils/utils_std.h"
#include "archetype.h"
#include "chunk_header.h"
#include "common.h"
#include "entity.h"
#include "fwd.h"

namespace gaia {
	namespace ecs {
		uint16_t GetArchetypeCapacity(const Archetype& archetype);
		const ChunkComponentList&
		GetArchetypeComponentList(const Archetype& archetype, ComponentType type);

		class Chunk final {
		public:
			//! Size of data at the end of the chunk reserved for special purposes
			//! (alignment, allocation overhead etc.)
			static constexpr uint32_t DATA_SIZE_RESERVED = 128;
			//! Size of one chunk's data part with components
			static constexpr uint32_t DATA_SIZE =
					CHUNK_SIZE - sizeof(ChunkHeader) - DATA_SIZE_RESERVED;

		private:
			friend class World;
			friend class Archetype;
			friend class CommandBuffer;

			//! Archetype header with info about the archetype
			ChunkHeader header;
			//! Archetype data. Entities first, followed by a lists of components.
			uint8_t data[DATA_SIZE - sizeof(ChunkHeader)];

			Chunk(const Archetype& archetype): header(archetype) {}

			[[nodiscard]] void* GetComponent_Internal(
					ComponentType TYPE, uint32_t index,
					const ComponentMetaData* type) const {
				GAIA_ASSERT(index <= header.lastEntityIndex || index != (uint32_t)-1);
				// invalid component requests are a programmer's bug
				GAIA_ASSERT(type != nullptr);

				const auto& componentList =
						GetArchetypeComponentList(header.owner, TYPE);
				const auto it = utils::find_if(componentList, [type](const auto& info) {
					return info.type == type;
				});

				// Searching for a component that's not there! Programmer mistake.
				GAIA_ASSERT(it != componentList.end());

				const uint32_t idxData = it->offset + type->size * index;
				GAIA_ASSERT(idxData <= Chunk::DATA_SIZE);
				return (void*)&data[idxData];
			}

			[[nodiscard]] void* SetComponent_Internal(
					ComponentType TYPE, uint32_t index, const ComponentMetaData* type) {
				GAIA_ASSERT(index <= header.lastEntityIndex || index != (uint32_t)-1);
				// invalid component requests are a programmer's bug
				GAIA_ASSERT(type != nullptr);

				const auto& componentList =
						GetArchetypeComponentList(header.owner, TYPE);
				const auto it = utils::find_if(componentList, [type](const auto& info) {
					return info.type == type;
				});

				// Searching for a component that's not there! Programmer mistake.
				const auto componentIdx =
						(uint32_t)std::distance(componentList.begin(), it);
				GAIA_ASSERT(componentIdx != utils::BadIndex);

				// Update version number so we know RW access was used on chunk
				header.UpdateLastWorldVersion(TYPE, componentIdx);

				const uint32_t idxData = it->offset + type->size * index;
				GAIA_ASSERT(idxData <= Chunk::DATA_SIZE);
				return (void*)&data[idxData];
			}

			template <typename T>
			[[nodiscard]] std::decay_t<T>&
			GetComponent_Internal(ComponentType TYPE, uint32_t index) const {
				using TComponent = std::decay_t<T>;
				const ComponentMetaData* type =
						g_ComponentCache.GetComponentMetaType<TComponent>();
				return *(TComponent*)GetComponent_Internal(TYPE, index, type);
			}

			template <typename T>
			[[nodiscard]] std::decay_t<T>&
			SetComponent_Internal(ComponentType TYPE, uint32_t index) {
				using TComponent = std::decay_t<T>;
				const ComponentMetaData* type =
						g_ComponentCache.GetComponentMetaType<TComponent>();
				return *(TComponent*)SetComponent_Internal(TYPE, index, type);
			}

			[[nodiscard]] uint32_t
			GetComponentIdx_Internal(ComponentType TYPE, uint32_t typeIdx) const {
				const auto& componentList =
						GetArchetypeComponentList(header.owner, TYPE);
				return utils::get_index_if(componentList, [typeIdx](const auto& info) {
					return info.type->typeIndex == typeIdx;
				});
			}

			template <typename T>
			[[nodiscard]] bool HasComponent_Internal(ComponentType TYPE) const {
				using TComponent = std::decay_t<T>;

				if (!g_ComponentCache.HasComponentMetaType<TComponent>())
					return false;

				const ComponentMetaData* type =
						g_ComponentCache.GetComponentMetaType<TComponent>();
				const auto& componentList =
						GetArchetypeComponentList(header.owner, TYPE);
				return utils::has_if(componentList, [type](const auto& info) {
					return info.type == type;
				});
			}

			[[nodiscard]] uint32_t AddEntity(Entity entity) {
				uint16_t index = ++header.lastEntityIndex;
				if (index == UINT16_MAX)
					index = 0;
				SetEntity(index, entity);

				header.UpdateLastWorldVersion(ComponentType::CT_Generic, UINT32_MAX);
				header.UpdateLastWorldVersion(ComponentType::CT_Chunk, UINT32_MAX);

				return header.lastEntityIndex;
			}

			void RemoveEntity(
					const uint16_t index, std::vector<EntityContainer>& entities) {
				// Ignore request on empty chunks
				if (header.lastEntityIndex == UINT16_MAX)
					return;

				// We can't be removing from an index which is no longer there
				GAIA_ASSERT(index <= header.lastEntityIndex);

					// If there are at least two entities inside and it's not already the
					// last one let's swap our entity with the last one in chunk.
					if (header.lastEntityIndex != 0 && header.lastEntityIndex != index) {
						// Swap data at index with the last one
						const auto entity = GetEntity(header.lastEntityIndex);
						SetEntity(index, entity);

						const auto& componentList = GetArchetypeComponentList(
								header.owner, ComponentType::CT_Generic);
							for (const auto& info: componentList) {
								// Skip tag components
								if (!info.type->size)
									continue;

								const uint32_t idxFrom =
										info.offset + (uint32_t)index * info.type->size;
								const uint32_t idxTo =
										info.offset +
										(uint32_t)header.lastEntityIndex * info.type->size;

								GAIA_ASSERT(idxFrom < Chunk::DATA_SIZE);
								GAIA_ASSERT(idxTo < Chunk::DATA_SIZE);
								GAIA_ASSERT(idxFrom != idxTo);

								memcpy(&data[idxFrom], &data[idxTo], info.type->size);
							}

						// Entity has been replaced with the last one in chunk.
						// Update its index so look ups can find it.
						entities[entity.id()].idx = index;
						entities[entity.id()].gen = entity.gen();
				}

				header.UpdateLastWorldVersion(ComponentType::CT_Generic, UINT32_MAX);
				header.UpdateLastWorldVersion(ComponentType::CT_Chunk, UINT32_MAX);

				--header.lastEntityIndex;
			}

			void SetEntity(uint16_t index, Entity entity) {
				GAIA_ASSERT(
						index <= header.lastEntityIndex && index != UINT16_MAX &&
						"Entity index in chunk out of bounds!");

				(Entity&)data[sizeof(Entity) * index] = entity;
			}

			[[nodiscard]] const Entity GetEntity(uint16_t index) const {
				GAIA_ASSERT(
						index <= header.lastEntityIndex && index != UINT16_MAX &&
						"Entity index in chunk out of bounds!");

				return (const Entity&)data[sizeof(Entity) * index];
			}

			[[nodiscard]] bool IsFull() const {
				return header.lastEntityIndex != UINT16_MAX &&
							 header.lastEntityIndex + 1 >= GetArchetypeCapacity(header.owner);
			}

		public:
			template <typename T>
			[[nodiscard]] typename std::enable_if_t<
					std::is_same<std::decay_t<T>, Entity>::value, std::span<const Entity>>
			View() const {
				using TEntity = std::decay_t<T>;
				return {(const TEntity*)&data[0], GetItemCount()};
			}

			template <typename T>
			[[nodiscard]] typename std::enable_if_t<
					!std::is_same<std::decay_t<T>, Entity>::value,
					std::span<std::decay_t<T>>>
			ViewRW(ComponentType TYPE = ComponentType::CT_Generic) {
				using TComponent = std::decay_t<T>;

				const ComponentMetaData* type =
						g_ComponentCache.GetOrCreateComponentMetaType<TComponent>();

				const auto& componentList =
						GetArchetypeComponentList(header.owner, TYPE);
				const auto it = utils::find_if(componentList, [type](const auto& info) {
					return info.type == type;
				});

				// Searching for a component that's not there! Programmer mistake.
				const auto componentIdx =
						(uint32_t)std::distance(componentList.begin(), it);
				GAIA_ASSERT(componentIdx != utils::BadIndex);

				// Update version number so we know RW access was used on chunk
				header.UpdateLastWorldVersion(TYPE, componentIdx);
				const auto& info = componentList[componentIdx];
				return {(TComponent*)&data[info.offset], GetItemCount()};
			}

			template <typename T>
			[[nodiscard]] typename std::enable_if_t<
					!std::is_same<std::decay_t<T>, Entity>::value,
					std::span<const std::decay_t<T>>>
			View(ComponentType TYPE = ComponentType::CT_Generic) const {
				using TComponent = std::decay_t<T>;

				const ComponentMetaData* type =
						g_ComponentCache.GetOrCreateComponentMetaType<TComponent>();

				const auto& componentList =
						GetArchetypeComponentList(header.owner, TYPE);
				const auto it = utils::find_if(componentList, [type](const auto& info) {
					return info.type == type;
				});

				// Searching for a component that's not there! Programmer mistake.
				GAIA_ASSERT(it != componentList.end());

				return {(const TComponent*)&data[it->offset], GetItemCount()};
			}

			[[nodiscard]] uint32_t GetComponentIdx(uint32_t typeIdx) const {
				return GetComponentIdx_Internal(ComponentType::CT_Generic, typeIdx);
			}
			[[nodiscard]] uint32_t GetChunkComponentIdx(uint32_t typeIdx) const {
				return GetComponentIdx_Internal(ComponentType::CT_Chunk, typeIdx);
			}

			template <typename T>
			[[nodiscard]] bool HasComponent() const {
				return HasComponent_Internal<std::decay_t<T>>(
						ComponentType::CT_Generic);
			}
			template <typename T>
			[[nodiscard]] bool HasChunkComponent() const {
				return HasComponent_Internal<std::decay_t<T>>(ComponentType::CT_Chunk);
			}

			template <typename T>
			void SetChunkComponent(T&& value) {
				SetComponent_Internal<std::decay_t<T>>(ComponentType::CT_Chunk, 0) =
						std::forward<T>(value);
			}

			template <typename T>
			[[nodiscard]] const std::decay_t<T>&
			GetChunkComponent(uint32_t index) const {
				return GetComponent_Internal<std::decay_t<T>>(
						ComponentType::CT_Chunk, index);
			}

			//! Checks is there are any entities in the chunk
			[[nodiscard]] bool HasEntities() const {
				return header.lastEntityIndex != UINT16_MAX;
			}

			//! Returns the number of entities in the chunk
			[[nodiscard]] uint16_t GetItemCount() const {
				return HasEntities() ? header.lastEntityIndex + 1 : 0;
			}

			//! Returns true if the passed version of a component is newer than the
			//! one stored internally
			[[nodiscard]] bool DidChange(
					ComponentType TYPE, uint32_t version, uint32_t componentIdx) const {
				return DidVersionChange(header.versions[TYPE][componentIdx], version);
			}
		};
		static_assert(
				sizeof(Chunk) == Chunk::DATA_SIZE, "Chunk size must match DATA_SIZE!");
	} // namespace ecs
} // namespace gaia