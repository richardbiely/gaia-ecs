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
		const ComponentInfo* GetComponentInfoFromIdx(uint32_t componentIdx);
		const ComponentInfoList& GetArchetypeComponentInfoList(const Archetype& archetype, ComponentType type);
		const ComponentLookupList& GetArchetypeComponentLookupList(const Archetype& archetype, ComponentType type);

		class Chunk final {
		public:
			//! Size of data at the end of the chunk reserved for special purposes
			//! (alignment, allocation overhead etc.)
			static constexpr size_t DATA_SIZE_RESERVED = 128;
			//! Size of one chunk's data part with components
			static constexpr size_t DATA_SIZE = ChunkMemorySize - sizeof(ChunkHeader) - DATA_SIZE_RESERVED;
			//! Size of one chunk's data part with components without the serve part
			static constexpr size_t DATA_SIZE_NORESERVE = ChunkMemorySize - sizeof(ChunkHeader);

		private:
			friend class World;
			friend class Archetype;
			friend class CommandBuffer;

			//! Archetype header with info about the archetype
			ChunkHeader header;
			//! Archetype data. Entities first, followed by a lists of components.
			uint8_t data[DATA_SIZE_NORESERVE];

			GAIA_MSVC_WARNING_PUSH()
			GAIA_MSVC_WARNING_DISABLE(26495)

			Chunk(const Archetype& archetype): header(archetype) {}

			GAIA_MSVC_WARNING_POP()

			/*!
			Checks if a component is present in the archetype based on the provided \param infoIndex.
			\param type Component type
			\return True if found. False otherwise.
			*/
			GAIA_NODISCARD bool HasComponent_Internal(ComponentType type, uint32_t infoIndex) const {
				const auto& infos = GetArchetypeComponentLookupList(header.owner, type);
				return utils::has_if(infos, [&](const auto& info) {
					return info.infoIndex == infoIndex;
				});
			}

			/*!
			Make the \param entity entity a part of the chunk.
			\return Index of the entity within the chunk.
			*/
			GAIA_NODISCARD uint32_t AddEntity(Entity entity) {
				const auto index = header.count++;
				SetEntity(index, entity);

				header.UpdateWorldVersion(ComponentType::CT_Generic);
				header.UpdateWorldVersion(ComponentType::CT_Chunk);

				return index;
			}

			void RemoveEntity(uint32_t index, containers::darray<EntityContainer>& entities) {
				// Ignore requests on empty chunks
				if (header.count == 0)
					return;

				// We can't be removing from an index which is no longer there
				GAIA_ASSERT(index < header.count);

				// If there are at least two entities inside and it's not already the
				// last one let's swap our entity with the last one in chunk.
				if (header.count > 1 && header.count != index + 1) {
					// Swap data at index with the last one
					const auto entity = GetEntity(header.count - 1);
					SetEntity(index, entity);

					const auto& componentInfos = GetArchetypeComponentInfoList(header.owner, ComponentType::CT_Generic);
					const auto& lookupList = GetArchetypeComponentLookupList(header.owner, ComponentType::CT_Generic);

					for (size_t i = 0; i < componentInfos.size(); i++) {
						const auto& info = componentInfos[i];
						const auto& look = lookupList[i];

						// Skip tag components
						if (!info->properties.size)
							continue;

						const uint32_t idxFrom = look.offset + index * info->properties.size;
						const uint32_t idxTo = look.offset + (header.count - 1) * info->properties.size;

						GAIA_ASSERT(idxFrom < Chunk::DATA_SIZE_NORESERVE);
						GAIA_ASSERT(idxTo < Chunk::DATA_SIZE_NORESERVE);
						GAIA_ASSERT(idxFrom != idxTo);

						memcpy(&data[idxFrom], &data[idxTo], info->properties.size);
					}

					// Entity has been replaced with the last one in chunk.
					// Update its index so look ups can find it.
					entities[entity.id()].idx = index;
					entities[entity.id()].gen = entity.gen();
				}

				header.UpdateWorldVersion(ComponentType::CT_Generic);
				header.UpdateWorldVersion(ComponentType::CT_Chunk);

				--header.count;
			}

			/*!
			Makes the entity a part of a chunk on a given index.
			\param index Index of the entity
			\param entity Entity to store in the chunk
			*/
			void SetEntity(uint32_t index, Entity entity) {
				GAIA_ASSERT(index < header.count && "Entity index in chunk out of bounds!");

				utils::unaligned_ref<Entity> mem((void*)&data[sizeof(Entity) * index]);
				mem = entity;
			}

			/*!
			Returns the entity on a given index in the chunk.
			\param index Index of the entity
			\return Entity on a given index within the chunk.
			*/
			GAIA_NODISCARD const Entity GetEntity(uint32_t index) const {
				GAIA_ASSERT(index < header.count && "Entity index in chunk out of bounds!");

				utils::unaligned_ref<Entity> mem((void*)&data[sizeof(Entity) * index]);
				return mem;
			}

			/*!
			Returns a read-only span of the component data.
			\tparam T Component
			\return Const span of the component data.
			*/
			template <typename T>
			GAIA_NODISCARD GAIA_FORCEINLINE auto View_Internal() const {
				using U = typename DeduceComponent<T>::Type;
				using UConst = typename std::add_const_t<U>;

				if constexpr (std::is_same_v<U, Entity>) {
					return std::span<const Entity>{(const Entity*)&data[0], GetItemCount()};
				} else {
					static_assert(!std::is_empty_v<U>, "Attempting to get value of an empty component");

					const auto infoIndex = utils::type_info::index<U>();

					if constexpr (IsGenericComponent<T>)
						return std::span<UConst>{(UConst*)GetDataPtr(ComponentType::CT_Generic, infoIndex), GetItemCount()};
					else
						return std::span<UConst>{(UConst*)GetDataPtr(ComponentType::CT_Chunk, infoIndex), 1};
				}
			}

			/*!
			Returns a read-write span of the component data. Also updates the world version for the component.
			\tparam T Component
			\tparam UpdateWorldVersion If true, the world version is updated as a result of the write access
			\return Span of the component data.
			*/
			template <typename T, bool UpdateWorldVersion>
			GAIA_NODISCARD GAIA_FORCEINLINE auto ViewRW_Internal() {
				using U = typename DeduceComponent<T>::Type;
#if GAIA_COMPILER_MSVC && _MSC_VER <= 1916
				// Workaround for MSVC 2017 bug where it incorrectly evaluates the static assert
				// even in context where it shouldn't.
				// Unfortunatelly, even runtime assert can't be used...
				// GAIA_ASSERT(!std::is_same_v<U, Entity>::value);
#else
				static_assert(!std::is_same_v<U, Entity>);
#endif
				static_assert(!std::is_empty_v<U>, "Attempting to set value of an empty component");

				const auto infoIndex = utils::type_info::index<U>();

				constexpr bool uwv = UpdateWorldVersion;
				if constexpr (IsGenericComponent<T>)
					return std::span<U>{(U*)GetDataPtrRW<uwv>(ComponentType::CT_Generic, infoIndex), GetItemCount()};
				else
					return std::span<U>{(U*)GetDataPtrRW<uwv>(ComponentType::CT_Chunk, infoIndex), 1};
			}

			/*!
			Returns a pointer do component data with read-only access.
			\param type Component type
			\param infoIndex Index of the component in the archetype
			\return Const pointer to component data.
			*/
			GAIA_NODISCARD GAIA_FORCEINLINE const uint8_t* GetDataPtr(ComponentType type, uint32_t infoIndex) const {
				// Searching for a component that's not there! Programmer mistake.
				GAIA_ASSERT(HasComponent_Internal(type, infoIndex));

				const auto& infos = GetArchetypeComponentLookupList(header.owner, type);
				const auto componentIdx = utils::get_index_if_unsafe(infos, [&](const auto& info) {
					return info.infoIndex == infoIndex;
				});

				return (const uint8_t*)&data[infos[componentIdx].offset];
			}

			/*!
			Returns a pointer do component data with read-write access. Also updates the world version for the component.
			\tparam UpdateWorldVersion If true, the world version is updated as a result of the write access
			\param type Component type
			\param infoIndex Index of the component in the archetype
			\return Pointer to component data.
			*/
			template <bool UpdateWorldVersion>
			GAIA_NODISCARD GAIA_FORCEINLINE uint8_t* GetDataPtrRW(ComponentType type, uint32_t infoIndex) {
				// Searching for a component that's not there! Programmer mistake.
				GAIA_ASSERT(HasComponent_Internal(type, infoIndex));
				// Don't use this with empty components. It's impossible to write to them anyway.
				GAIA_ASSERT(GetComponentInfoFromIdx(infoIndex)->properties.size != 0);

				const auto& infos = GetArchetypeComponentLookupList(header.owner, type);
				const auto componentIdx = (uint32_t)utils::get_index_if_unsafe(infos, [&](const auto& info) {
					return info.infoIndex == infoIndex;
				});

				if constexpr (UpdateWorldVersion) {
					// Update version number so we know RW access was used on chunk
					header.UpdateWorldVersion(type, componentIdx);
				}

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
			\return Component view with read-only access
			*/
			template <typename T>
			GAIA_NODISCARD auto View() const {
				using U = typename DeduceComponent<T>::Type;
				using UOriginal = typename DeduceComponent<T>::TypeOriginal;
				static_assert(IsReadOnlyType<UOriginal>::value);

				return utils::auto_view_policy_get<std::add_const_t<U>>{{View_Internal<T>()}};
			}

			/*!
			Returns a mutable component view.
			\return Component view with read-write access
			*/
			template <typename T>
			GAIA_NODISCARD auto ViewRW() {
				using U = typename DeduceComponent<T>::Type;
				static_assert(!std::is_same_v<U, Entity>);

				return utils::auto_view_policy_set<U>{{ViewRW_Internal<T, true>()}};
			}

			/*!
			Returns a mutable component view. Doesn't update the world version when the access is aquired.
			\return Component view with read-write access
			*/
			template <typename T>
			GAIA_NODISCARD auto ViewRWSilent() {
				using U = typename DeduceComponent<T>::Type;
				static_assert(!std::is_same_v<U, Entity>);

				return utils::auto_view_policy_set<U>{{ViewRW_Internal<T, false>()}};
			}

			/*!
			Returns the internal index of a component based on the provided \param infoIndex.
			\param type Component type
			\return Component index if the component was found. -1 otherwise.
			*/
			GAIA_NODISCARD uint32_t GetComponentIdx(ComponentType type, uint32_t infoIndex) const {
				const auto& list = GetArchetypeComponentLookupList(header.owner, type);
				const auto idx = (uint32_t)utils::get_index_if_unsafe(list, [&](const auto& info) {
					return info.infoIndex == infoIndex;
				});
				GAIA_ASSERT(idx != (uint32_t)-1);
				return (uint32_t)idx;
			}

			/*!
			Checks if component is present on the chunk.
			\return True if the component is present. False otherwise.
			*/
			template <typename T>
			GAIA_NODISCARD bool HasComponent() const {
				if constexpr (IsGenericComponent<T>) {
					using U = typename detail::ExtractComponentType_Generic<T>::Type;
					const auto infoIndex = utils::type_info::index<U>();
					return HasComponent_Internal(ComponentType::CT_Generic, infoIndex);
				} else {
					using U = typename detail::ExtractComponentType_NonGeneric<T>::Type;
					const auto infoIndex = utils::type_info::index<U>();
					return HasComponent_Internal(ComponentType::CT_Chunk, infoIndex);
				}
			}

			//----------------------------------------------------------------------
			// Set component data
			//----------------------------------------------------------------------

			template <typename T>
			void SetComponent(uint32_t index, typename DeduceComponent<T>::Type&& value) {
				using U = typename DeduceComponent<T>::Type;

				static_assert(
						IsGenericComponent<T>, "SetComponent providing an index in chunk is only available for generic components");

				ViewRW<T>()[index] = std::forward<U>(value);
			}

			template <typename T>
			void SetComponent(typename DeduceComponent<T>::Type&& value) {
				using U = typename DeduceComponent<T>::Type;

				static_assert(
						!IsGenericComponent<T>,
						"SetComponent not providing an index in chunk is only available for non-generic components");

				ViewRW<T>()[0] = std::forward<U>(value);
			}

			template <typename T>
			void SetComponentSilent(uint32_t index, typename DeduceComponent<T>::Type&& value) {
				using U = typename DeduceComponent<T>::Type;

				static_assert(
						IsGenericComponent<T>, "SetComponent providing an index in chunk is only available for generic components");

				ViewRWSilent<T>()[index] = std::forward<U>(value);
			}

			template <typename T>
			void SetComponentSilent(typename DeduceComponent<T>::Type&& value) {
				using U = typename DeduceComponent<T>::Type;

				static_assert(
						!IsGenericComponent<T>,
						"SetComponent not providing an index in chunk is only available for non-generic components");

				ViewRWSilent<T>()[0] = std::forward<U>(value);
			}

			//----------------------------------------------------------------------
			// Read component data
			//----------------------------------------------------------------------

			template <typename T>
			auto GetComponent(uint32_t index) const {
				static_assert(
						IsGenericComponent<T>, "GetComponent providing an index is only available for generic components");
				return View<T>()[index];
			}

			template <typename T>
			auto GetComponent() const {
				static_assert(
						!IsGenericComponent<T>, "GetComponent not providing an index is only available for non-generic components");
				return View<T>()[0];
			}

			//----------------------------------------------------------------------

			//! Checks is this chunk is disabled
			GAIA_NODISCARD bool IsDisabled() const {
				return header.disabled;
			}

			//! Checks is the full capacity of the has has been reached
			GAIA_NODISCARD bool IsFull() const {
				return header.count >= header.capacity;
			}

			//! Checks is there are any entities in the chunk
			GAIA_NODISCARD bool HasEntities() const {
				return header.count > 0;
			}

			//! Returns the number of entities in the chunk
			GAIA_NODISCARD uint32_t GetItemCount() const {
				return header.count;
			}

			//! Returns true if the provided version is newer than the one stored internally
			GAIA_NODISCARD bool DidChange(ComponentType type, uint32_t version, uint32_t componentIdx) const {
				return DidVersionChange(header.versions[type][componentIdx], version);
			}
		};
		static_assert(sizeof(Chunk) <= ChunkMemorySize, "Chunk size must match ChunkMemorySize!");
	} // namespace ecs
} // namespace gaia
