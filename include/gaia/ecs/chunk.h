#pragma once
#include <cinttypes>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "../config/config.h"
#include "../containers/sarray_ext.h"
#include "../ecs/component.h"
#include "../utils/containers.h"
#include "../utils/mem.h"
#include "archetype.h"
#include "chunk_allocator.h"
#include "chunk_header.h"
#include "common.h"
#include "entity.h"
#include "fwd.h"

namespace gaia {
	namespace ecs {
		const ComponentInfoList& GetArchetypeComponentInfoList(const Archetype& archetype, ComponentType type);
		const ComponentLookupList& GetArchetypeComponentLookupList(const Archetype& archetype, ComponentType type);

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
			uint8_t data[DATA_SIZE_NORESERVE];

			Chunk(const Archetype& archetype): header(archetype) {}

			/*!
			Checks if a component is present in the archetype based on the provided \param infoIndex.
			\param componentType Component type
			\return True if found. False otherwise.
			*/
			[[nodiscard]] bool HasComponent_Internal(ComponentType componentType, uint32_t infoIndex) const {
				const auto& infos = GetArchetypeComponentLookupList(header.owner, componentType);
				return utils::has_if(infos, [&](const auto& info) {
					return info.infoIndex == infoIndex;
				});
			}

			/*!
			Checks if a component is present in the archetype based on the provided \param infoIndex.
			Component if deduced from \tparam T.
			\param componentType Component type
			\return True if found. False otherwise.
			*/
			template <typename T>
			[[nodiscard]] bool HasComponent_Internal(ComponentType componentType) const {
				using U = typename DeduceComponent<T>::Type;
				const auto infoIndex = utils::type_info::index<U>();
				return HasComponent_Internal(componentType, infoIndex);
			}

			template <typename T>
			[[nodiscard]] bool HasComponent_Internal() const {
				if constexpr (IsGenericComponent<T>::value)
					return HasComponent_Internal<typename detail::ExtractComponentType_Generic<T>::Type>(
							ComponentType::CT_Generic);
				else
					return HasComponent_Internal<typename detail::ExtractComponentType_NonGeneric<T>::Type>(
							ComponentType::CT_Chunk);
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

					const auto& componentInfos = GetArchetypeComponentInfoList(header.owner, ComponentType::CT_Generic);
					const auto& lookupList = GetArchetypeComponentLookupList(header.owner, ComponentType::CT_Generic);

					for (uint32_t i = 0U; i < componentInfos.size(); i++) {
						const auto& info = componentInfos[i];
						const auto& look = lookupList[i];

						// Skip tag components
						if (!info.info->properties.size)
							continue;

						const uint32_t idxFrom = look.offset + (uint32_t)index * info.info->properties.size;
						const uint32_t idxTo = look.offset + (uint32_t)(header.items.count - 1) * info.info->properties.size;

						GAIA_ASSERT(idxFrom < Chunk::DATA_SIZE_NORESERVE);
						GAIA_ASSERT(idxTo < Chunk::DATA_SIZE_NORESERVE);
						GAIA_ASSERT(idxFrom != idxTo);

						memcpy(&data[idxFrom], &data[idxTo], info.info->properties.size);
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
			[[nodiscard]] GAIA_FORCEINLINE auto View_Internal() const {
				using U = typename DeduceComponent<T>::Type;

				if constexpr (std::is_same<U, Entity>::value) {
					return std::span<const Entity>{(const Entity*)&data[0], GetItemCount()};
				} else {
					static_assert(!std::is_empty<U>::value, "Attempting to get value of an empty component");

					const auto infoIndex = utils::type_info::index<U>();

					if constexpr (IsGenericComponent<U>::value)
						return std::span<const U>{(const U*)get_data_ptr(ComponentType::CT_Generic, infoIndex), GetItemCount()};
					else
						return std::span<const U>{(const U*)get_data_ptr(ComponentType::CT_Chunk, infoIndex), 1};
				}
			}

			template <typename T>
			[[nodiscard]] GAIA_FORCEINLINE auto ViewRW_Internal() {
				using U = typename DeduceComponent<T>::Type;
#if GAIA_COMPILER_MSVC && _MSC_VER <= 1916
				// Workaround for MSVC 2017 bug where it incorrectly evaluates the static assert
				// even in context where it shouldn't.
				// Unfortunatelly, even runtime assert can't be used...
				// GAIA_ASSERT(!std::is_same<U, Entity>::value);
#else
				static_assert(!std::is_same<U, Entity>::value);
#endif
				static_assert(!std::is_empty<U>::value, "Attempting to set value of an empty component");

				const auto infoIndex = utils::type_info::index<U>();

				if constexpr (IsGenericComponent<U>::value)
					return std::span<U>{(U*)get_data_rw_ptr(ComponentType::CT_Generic, infoIndex), GetItemCount()};
				else
					return std::span<U>{(U*)get_data_rw_ptr(ComponentType::CT_Chunk, infoIndex), 1};
			}

			[[nodiscard]] GAIA_FORCEINLINE uint8_t*
			ViewRW_Internal(const ComponentInfo* info, ComponentType componentType = ComponentType::CT_Generic) {
				GAIA_ASSERT(info != nullptr);
				// Empty components shouldn't be used for writing!
				GAIA_ASSERT(info->properties.size != 0);

				return get_data_rw_ptr(componentType, info->infoIndex);
			}

			[[nodiscard]] GAIA_FORCEINLINE const uint8_t*
			get_data_ptr(ComponentType componentType, uint32_t infoIndex) const {
				// Searching for a component that's not there! Programmer mistake.
				GAIA_ASSERT(HasComponent_Internal(componentType, infoIndex));

				const auto& infos = GetArchetypeComponentLookupList(header.owner, componentType);
				const auto componentIdx = utils::get_index_if_unsafe(infos, [&](const auto& info) {
					return info.infoIndex == infoIndex;
				});

				return (const uint8_t*)&data[infos[componentIdx].offset];
			}

			[[nodiscard]] GAIA_FORCEINLINE uint8_t* get_data_rw_ptr(ComponentType componentType, uint32_t infoIndex) {
				// Searching for a component that's not there! Programmer mistake.
				GAIA_ASSERT(HasComponent_Internal(componentType, infoIndex));

				const auto& infos = GetArchetypeComponentLookupList(header.owner, componentType);
				const auto componentIdx = utils::get_index_if_unsafe(infos, [&](const auto& info) {
					return info.infoIndex == infoIndex;
				});

				// Update version number so we know RW access was used on chunk
				header.UpdateWorldVersion(componentType, componentIdx);

				return (uint8_t*)&data[infos[componentIdx].offset];
			}

		public:
			/*!
			Returns the parent archetype.
			\return Parent archetype
			*/
			const Archetype& GetArchetype() const {
				return header.owner;
			}

			/*!
			Returns a read-only entity or component view.
			\return Component view
			*/
			template <typename T>
			[[nodiscard]] auto View() const {
				using U = typename DeduceComponent<T>::Type;
				using UOriginal = typename DeduceComponent<T>::TypeOriginal;
				static_assert(IsReadOnlyType<UOriginal>::value);

				return utils::auto_view_policy_get<const U>{View_Internal<U>()};
			}

			/*!
			Returns a mutable component view.
			\return Component view
			*/
			template <typename T>
			[[nodiscard]] auto ViewRW() {
				using U = typename DeduceComponent<T>::Type;
				static_assert(!std::is_same<U, Entity>::value);

				return utils::auto_view_policy_set<U>{ViewRW_Internal<U>()};
			}

			/*!
			Returns the internal index of a component based on the provided \param infoIndex.
			\param componentType Component type
			\return Component index if the component was found. -1 otherwise.
			*/
			[[nodiscard]] uint32_t GetComponentIdx(ComponentType componentType, uint32_t infoIndex) const {
				const auto& list = GetArchetypeComponentLookupList(header.owner, componentType);
				return utils::get_index_if_unsafe(list, [&](const auto& info) {
					return info.infoIndex == infoIndex;
				});
			}

			/*!
			Checks if all provided components are present on chunk.
			\return True if components are present. False otherwise.
			*/
			template <typename... T>
			[[nodiscard]] bool HasComponent() const {
				return (HasComponent_Internal<T>() && ...);
			}

			/*!
			Checks if any of the provided components is present on chunk.
			\return True if any of the components is present. False otherwise.
			*/
			template <typename... T>
			[[nodiscard]] bool HasAnyComponent() const {
				static_assert(sizeof...(T) > 1, "Use at least 2 component types when using HasAny");
				return (HasComponent_Internal<T>() || ...);
			}

			/*!
			Checks if none of the provided components are present on chunk.
			\return True if none of the components are present. False otherwise.
			*/
			template <typename... T>
			[[nodiscard]] bool HasNoneComponent() const {
				return (!HasComponent_Internal<T>() && ...);
			}

			//----------------------------------------------------------------------
			// Setting component value
			//----------------------------------------------------------------------

			template <typename T>
			void SetComponent(uint32_t index, typename DeduceComponent<T>::Type&& value) {
				using U = typename DeduceComponent<T>::Type;

				static_assert(
						IsGenericComponent<T>::value,
						"SetComponent providing an index in chunk is only available for generic components");

				ViewRW<U>()[index] = std::forward<U>(value);
			}

			template <typename T>
			void SetComponent(typename DeduceComponent<T>::Type&& value) {
				using U = typename DeduceComponent<T>::Type;

				static_assert(
						!IsGenericComponent<T>::value,
						"SetComponent not providing an index in chunk is only available for non-generic components");

				ViewRW<U>()[0] = std::forward<U>(value);
			}

			//----------------------------------------------------------------------
			// Component data
			//----------------------------------------------------------------------

			template <typename T>
			const auto& GetComponent(uint32_t index) const {
				static_assert(
						IsGenericComponent<T>::value, "GetComponent providing an index is only available for generic components");
				return View<T>()[index];
			}

			template <typename T>
			const auto& GetComponent() const {
				static_assert(
						!IsGenericComponent<T>::value,
						"GetComponent not providing an index is only available for non-generic components");
				return View<T>()[0];
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
