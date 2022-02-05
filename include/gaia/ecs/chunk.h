#pragma once
#include <algorithm>
#include <cassert>
#include <inttypes.h>
#include <iterator>
#include <stdint.h>
#include <type_traits>
#include <utility>

#include "../config/config.h"
#include "../containers/sarray_ext.h"
#include "../ecs/component.h"
#include "../utils/utils_containers.h"
#include "../utils/utils_mem.h"
#include "archetype.h"
#include "chunk_allocator.h"
#include "chunk_header.h"
#include "common.h"
#include "entity.h"
#include "fwd.h"

namespace gaia {
	namespace ecs {
		const ChunkComponentTypeList& GetArchetypeComponentTypeList(const Archetype& archetype, ComponentType type);
		const ChunkComponentLookupList& GetArchetypeComponentLookupList(const Archetype& archetype, ComponentType type);

		class Chunk final {
		public:
			//! Size of data at the end of the chunk reserved for special purposes
			//! (alignment, allocation overhead etc.)
			static constexpr uint32_t DATA_SIZE_RESERVED = 128;
			//! Size of one chunk's data part with components
			static constexpr uint32_t DATA_SIZE = ChunkMemorySize - sizeof(ChunkHeader) - DATA_SIZE_RESERVED;
			//! Size of one chunk's data part with components without the serve part
			static constexpr uint32_t DATA_SIZE_NORESERVE = ChunkMemorySize - sizeof(ChunkHeader);

		private:
			friend class World;
			friend class Archetype;
			friend class CommandBuffer;

			//! Archetype header with info about the archetype
			ChunkHeader header;
			//! Archetype data. Entities first, followed by a lists of components.
			uint8_t data[DATA_SIZE];

			Chunk(const Archetype& archetype): header(archetype) {}

			template <typename T>
			[[nodiscard]] std::decay_t<T> GetComponentVal_Internal(ComponentType componentType, uint32_t index) const {
				using TComponent = std::decay_t<T>;
				return View<TComponent>(componentType)[index];
			}

			template <typename T>
			[[nodiscard]] const std::decay_t<T>& GetComponentRef_Internal(ComponentType componentType, uint32_t index) const {
				using TComponent = std::decay_t<T>;
				return View<TComponent>(componentType)[index];
			}

			template <typename T>
			void SetComponent_Internal(ComponentType componentType, uint32_t index, std::decay_t<T>&& value) {
				using TComponent = std::decay_t<T>;
				if constexpr (std::is_empty<TComponent>::value)
					return;
				else {
					ViewRW<TComponent>(componentType)[index] = std::forward<TComponent>(value);
				}
			}

			[[nodiscard]] uint32_t GetComponentIdx_Internal(ComponentType componentType, uint32_t typeIndex) const {
				const auto& list = GetArchetypeComponentLookupList(header.owner, componentType);
				return utils::get_index_if_unsafe(list, [&](const auto& info) {
					return info.typeIndex == typeIndex;
				});
			}

			[[nodiscard]] bool HasComponent_Internal(ComponentType componentType, uint32_t typeIndex) const {
				const auto& list = GetArchetypeComponentLookupList(header.owner, componentType);
				return utils::has_if(list, [&](const auto& info) {
					return info.typeIndex == typeIndex;
				});
			}

			template <typename T>
			[[nodiscard]] bool HasComponent_Internal(ComponentType componentType) const {
				using TComponent = std::decay_t<T>;
				const auto typeIndex = utils::type_info::index<TComponent>();
				return HasComponent_Internal(componentType, typeIndex);
			}

			[[nodiscard]] uint32_t AddEntity(Entity entity) {
				const auto index = header.items.count++;
				SetEntity(index, entity);

				header.UpdateWorldVersion(ComponentType::CT_Generic);
				header.UpdateWorldVersion(ComponentType::CT_Chunk);

				return index;
			}

			void RemoveEntity(const uint32_t index, containers::darray<EntityContainer>& entities) {
				// Ignore request on empty chunks
				if (header.items.count == 0)
					return;

				// We can't be removing from an index which is no longer there
				GAIA_ASSERT(index < header.items.count);

				// If there are at least two entities inside and it's not already the
				// last one let's swap our entity with the last one in chunk.
				if (header.items.count > 1 && header.items.count != index + 1) {
					// Swap data at index with the last one
					const auto entity = GetEntity(header.items.count - 1);
					SetEntity(index, entity);

					const auto& componentTypeList = GetArchetypeComponentTypeList(header.owner, ComponentType::CT_Generic);
					const auto& lookupList = GetArchetypeComponentLookupList(header.owner, ComponentType::CT_Generic);

					for (uint32_t i = 0U; i < componentTypeList.size(); i++) {
						const auto& info = componentTypeList[i];
						const auto& look = lookupList[i];

						// Skip tag components
						if (!info.type->info.size)
							continue;

						const uint32_t idxFrom = look.offset + (uint32_t)index * info.type->info.size;
						const uint32_t idxTo = look.offset + (uint32_t)(header.items.count - 1) * info.type->info.size;

						GAIA_ASSERT(idxFrom < Chunk::DATA_SIZE);
						GAIA_ASSERT(idxTo < Chunk::DATA_SIZE);
						GAIA_ASSERT(idxFrom != idxTo);

						memcpy(&data[idxFrom], &data[idxTo], info.type->info.size);
					}

					// Entity has been replaced with the last one in chunk.
					// Update its index so look ups can find it.
					entities[entity.id()].idx = index;
					entities[entity.id()].gen = entity.gen();
				}

				header.UpdateWorldVersion(ComponentType::CT_Generic);
				header.UpdateWorldVersion(ComponentType::CT_Chunk);

				--header.items.count;
			}

			void SetEntity(uint32_t index, Entity entity) {
				GAIA_ASSERT(index < header.items.count && "Entity index in chunk out of bounds!");

				utils::unaligned_ref<Entity> mem((void*)&data[sizeof(Entity) * index]);
				mem = entity;
			}

			[[nodiscard]] const Entity GetEntity(uint32_t index) const {
				GAIA_ASSERT(index < header.items.count && "Entity index in chunk out of bounds!");

				utils::unaligned_ref<Entity> mem((void*)&data[sizeof(Entity) * index]);
				return mem;
			}

			[[nodiscard]] bool IsFull() const {
				return header.items.count >= header.items.capacity;
			}

			template <typename T>
			[[nodiscard]] GAIA_FORCEINLINE
					typename std::enable_if_t<std::is_same<std::decay_t<T>, Entity>::value, std::span<const Entity>>
					view_internal() const {
				return {(const Entity*)&data[0], GetItemCount()};
			}

			template <typename T>
			[[nodiscard]] GAIA_FORCEINLINE
					typename std::enable_if_t<!std::is_same<std::decay_t<T>, Entity>::value, std::span<const std::decay_t<T>>>
					view_internal(ComponentType componentType = ComponentType::CT_Generic) const {
				using TComponent = std::decay_t<T>;
				static_assert(!std::is_empty<TComponent>::value, "Attempting to get value of an empty component");

				const auto typeIndex = utils::type_info::index<TComponent>();

				// Searching for a component that's not there! Programmer mistake.
				GAIA_ASSERT(HasComponent_Internal(componentType, typeIndex));

				const auto& componentLookupList = GetArchetypeComponentLookupList(header.owner, componentType);
				const auto componentIdx = utils::get_index_if_unsafe(componentLookupList, [&](const auto& info) {
					return info.typeIndex == typeIndex;
				});

				return {(const TComponent*)&data[componentLookupList[componentIdx].offset], GetItemCount()};
			}

			template <typename T>
			[[nodiscard]] GAIA_FORCEINLINE
					typename std::enable_if_t<!std::is_same<std::decay_t<T>, Entity>::value, std::span<std::decay_t<T>>>
					view_rw_internal(ComponentType componentType = ComponentType::CT_Generic) {
				using TComponent = std::decay_t<T>;
				static_assert(!std::is_empty<TComponent>::value, "Empty components shouldn't be used for writing!");

				const auto typeIndex = utils::type_info::index<TComponent>();

				// Searching for a component that's not there! Programmer mistake.
				GAIA_ASSERT(HasComponent_Internal(componentType, typeIndex));

				const auto& componentLookupList = GetArchetypeComponentLookupList(header.owner, componentType);
				const auto componentIdx = utils::get_index_if_unsafe(componentLookupList, [&](const auto& info) {
					return info.typeIndex == typeIndex;
				});

				// Update version number so we know RW access was used on chunk
				header.UpdateWorldVersion(componentType, componentIdx);

				return {(TComponent*)&data[componentLookupList[componentIdx].offset], GetItemCount()};
			}

		public:
			template <typename T>
			[[nodiscard]]
			typename std::enable_if_t<std::is_same<std::decay_t<T>, Entity>::value, utils::auto_view_policy_get<const Entity>>
			View() const {
				return {view_internal<T>()};
			}

			template <typename T>
			[[nodiscard]] typename std::enable_if_t<
					!std::is_same<std::decay_t<T>, Entity>::value, utils::auto_view_policy_get<const std::decay_t<T>>>
			View(ComponentType componentType = ComponentType::CT_Generic) const {
				using TComponent = const std::decay_t<T>;
				return {view_internal<TComponent>(componentType)};
			}

			template <typename T>
			[[nodiscard]] typename std::enable_if_t<
					!std::is_same<std::decay_t<T>, Entity>::value, utils::auto_view_policy_set<std::decay_t<T>>>
			ViewRW(ComponentType componentType = ComponentType::CT_Generic) {
				using TComponent = std::decay_t<T>;
				return {view_rw_internal<TComponent>(componentType)};
			}

			[[nodiscard]] uint32_t GetComponentIdx(uint32_t typeIndex) const {
				return GetComponentIdx_Internal(ComponentType::CT_Generic, typeIndex);
			}

			[[nodiscard]] uint32_t GetChunkComponentIdx(uint32_t typeIndex) const {
				return GetComponentIdx_Internal(ComponentType::CT_Chunk, typeIndex);
			}

			template <typename T>
			[[nodiscard]] bool HasComponent() const {
				return HasComponent_Internal<std::decay_t<T>>(ComponentType::CT_Generic);
			}

			template <typename T>
			[[nodiscard]] bool HasChunkComponent() const {
				return HasComponent_Internal<T>(ComponentType::CT_Chunk);
			}

			//----------------------------------------------------------------------
			// Setting component value
			//----------------------------------------------------------------------

			template <typename T>
			void SetComponent(uint32_t index, T&& value) {
				SetComponent_Internal<T>(ComponentType::CT_Generic, index, std::forward<T>(value));
			}

			template <typename... T>
			void SetComponents(uint32_t index, T&&... value) {
				(SetComponent_Internal<T>(ComponentType::CT_Generic, index, std::forward<T>(value)), ...);
			}

			template <typename T>
			void SetChunkComponent(T&& value) {
				SetComponent_Internal<T>(ComponentType::CT_Chunk, 0, std::forward<T>(value));
			}

			template <typename... T>
			void SetChunkComponents(T&&... value) {
				(SetComponent_Internal<T>(ComponentType::CT_Chunk, 0, std::forward<T>(value)), ...);
			}

			//----------------------------------------------------------------------
			// Component data by copy
			//----------------------------------------------------------------------

			template <typename T>
			void GetComponent(uint32_t index, std::decay_t<T>& data) const {
				data = GetComponentVal_Internal<T>(ComponentType::CT_Generic, index);
			}

			template <typename... T>
			void GetComponents(uint32_t index, std::decay_t<T>&... data) const {
				(GetComponent<T>(index, data), ...);
			}

			template <typename T>
			void GetChunkComponent(std::decay_t<T>& data) const {
				data = GetComponentVal_Internal<T>(ComponentType::CT_Chunk, 0);
			}

			template <typename... T>
			void GetChunkComponents(std::decay_t<T>&... data) const {
				(GetChunkComponent<T>(data), ...);
			}

			//----------------------------------------------------------------------
			// Component data by reference
			//----------------------------------------------------------------------

			template <typename T>
			void GetComponent(uint32_t index, const std::decay_t<T>*& data) const {
				// invalid input is a programmer's bug
				GAIA_ASSERT(data != nullptr);
				const auto& ref = GetComponentRef_Internal<T>(ComponentType::CT_Generic, index);
				data = &ref;
			}

			template <typename... T>
			void GetComponent(uint32_t index, const std::decay_t<T>*&... data) const {
				(GetComponent<T>(index, data), ...);
			}

			template <typename T>
			void GetChunkComponent(const std::decay_t<T>*& data) const {
				// invalid input is a programmer's bug
				GAIA_ASSERT(data != nullptr);
				const auto& ref = GetComponentRef_Internal<T>(ComponentType::CT_Chunk, 0);
				data = &ref;
			}

			template <typename... T>
			void GetChunkComponent(const std::decay_t<T>*&... data) const {
				(GetChunkComponent<T>(data), ...);
			}

			//----------------------------------------------------------------------

			//! Checks is this chunk is disabled
			[[nodiscard]] bool IsDisabled() const {
				return header.info.disabled;
			}

			//! Checks is there are any entities in the chunk
			[[nodiscard]] bool HasEntities() const {
				return header.items.count > 0;
			}

			//! Returns the number of entities in the chunk
			[[nodiscard]] uint32_t GetItemCount() const {
				return header.items.count;
			}

			//! Returns true if the provided version is newer than the one stored internally
			[[nodiscard]] bool DidChange(ComponentType componentType, uint32_t version, uint32_t componentIdx) const {
				return DidVersionChange(header.versions[componentType][componentIdx], version);
			}
		};
		static_assert(sizeof(Chunk) <= ChunkMemorySize, "Chunk size must match ChunkMemorySize!");
	} // namespace ecs
} // namespace gaia