#pragma once
#include <cinttypes>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "../config/config.h"
#include "../containers/sarray_ext.h"
#include "../ecs/component.h"
#include "../utils/mem.h"
#include "../utils/utility.h"
#include "archetype.h"
#include "chunk_allocator.h"
#include "chunk_header.h"
#include "common.h"
#include "entity.h"
#include "fwd.h"

namespace gaia {
	namespace ecs {
		extern const ComponentInfo& GetComponentInfo(ComponentId componentId);
		extern const ComponentInfoCreate& GetComponentCreateInfo(ComponentId componentId);
		extern const ComponentIdList&
		GetArchetypeComponentInfoList(const Archetype& archetype, ComponentType componentType);
		extern const ComponentOffsetList&
		GetArchetypeComponentOffsetList(const Archetype& archetype, ComponentType componentType);

		class Chunk final {
		public:
			//! Size of data at the end of the chunk reserved for special purposes
			//! (alignment, allocation overhead etc.)
			static constexpr size_t DATA_SIZE_RESERVED = 128;
			//! Size of one chunk's data part with components
			static constexpr size_t DATA_SIZE = ChunkMemorySize - sizeof(ChunkHeader) - DATA_SIZE_RESERVED;
			//! Size of one chunk's data part with components without the reserved part
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
			Checks if a component is present in the archetype based on the provided \param componentId.
			\param componentType Component type
			\return True if found. False otherwise.
			*/
			GAIA_NODISCARD bool HasComponent_Internal(ComponentType componentType, uint32_t componentId) const {
				const auto& componentIds = GetArchetypeComponentInfoList(header.owner, componentType);
				return utils::has(componentIds, componentId);
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
				// last one let's swap our entity with the last one in the chunk.
				if (header.count > 1 && header.count != index + 1) {
					// Swap data at index with the last one
					const auto entity = GetEntity(header.count - 1);
					SetEntity(index, entity);

					const auto& componentIds = GetArchetypeComponentInfoList(header.owner, ComponentType::CT_Generic);
					const auto& offsets = GetArchetypeComponentOffsetList(header.owner, ComponentType::CT_Generic);

					for (size_t i = 0; i < componentIds.size(); i++) {
						const auto& info = GetComponentInfo(componentIds[i]);
						if (info.properties.size == 0U)
							continue;

						const auto offset = offsets[i];
						const auto idxSrc = offset + index * info.properties.size;
						const auto idxDst = offset + (header.count - 1U) * info.properties.size;

						GAIA_ASSERT(idxSrc < Chunk::DATA_SIZE_NORESERVE);
						GAIA_ASSERT(idxDst < Chunk::DATA_SIZE_NORESERVE);
						GAIA_ASSERT(idxSrc != idxDst);

						auto* pSrc = (void*)&data[idxSrc];
						auto* pDst = (void*)&data[idxDst];

						const auto& infoCreate = GetComponentCreateInfo(info.componentId);

						if (info.properties.movable == 1) {
							infoCreate.move(pSrc, pDst);
						} else if (info.properties.copyable == 1) {
							infoCreate.copy(pSrc, pDst);
						} else
							memmove(pDst, (const void*)pSrc, info.properties.size);

						if (info.properties.destructible == 1)
							infoCreate.destructor(pSrc, 1);
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
			GAIA_NODISCARD Entity GetEntity(uint32_t index) const {
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

					const auto componentId = GetComponentIdUnsafe<U>();

					if constexpr (IsGenericComponent<T>)
						return std::span<UConst>{(UConst*)GetDataPtr(ComponentType::CT_Generic, componentId), GetItemCount()};
					else
						return std::span<UConst>{(UConst*)GetDataPtr(ComponentType::CT_Chunk, componentId), 1};
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

				const auto componentId = GetComponentIdUnsafe<U>();

				constexpr bool uwv = UpdateWorldVersion;
				if constexpr (IsGenericComponent<T>)
					return std::span<U>{(U*)GetDataPtrRW<uwv>(ComponentType::CT_Generic, componentId), GetItemCount()};
				else
					return std::span<U>{(U*)GetDataPtrRW<uwv>(ComponentType::CT_Chunk, componentId), 1};
			}

			/*!
			Returns a pointer do component data with read-only access.
			\param componentType Component type
			\param componentId Component info index
			\return Const pointer to component data.
			*/
			GAIA_NODISCARD GAIA_FORCEINLINE const uint8_t*
			GetDataPtr(ComponentType componentType, uint32_t componentId) const {
				// Searching for a component that's not there! Programmer mistake.
				GAIA_ASSERT(HasComponent_Internal(componentType, componentId));

				const auto& componentIds = GetArchetypeComponentInfoList(header.owner, componentType);
				const auto& offsets = GetArchetypeComponentOffsetList(header.owner, componentType);
				const auto idx = utils::get_index_unsafe(componentIds, componentId);

				return (const uint8_t*)&data[offsets[idx]];
			}

			/*!
			Returns a pointer do component data with read-write access. Also updates the world version for the component.
			\tparam UpdateWorldVersion If true, the world version is updated as a result of the write access
			\param componentType Component type
			\param componentId Index of the component in the archetype
			\return Pointer to component data.
			*/
			template <bool UpdateWorldVersion>
			GAIA_NODISCARD GAIA_FORCEINLINE uint8_t* GetDataPtrRW(ComponentType componentType, uint32_t componentId) {
				// Searching for a component that's not there! Programmer mistake.
				GAIA_ASSERT(HasComponent_Internal(componentType, componentId));
				// Don't use this with empty components. It's impossible to write to them anyway.
				GAIA_ASSERT(GetComponentInfo(componentId).properties.size != 0);

				const auto& componentIds = GetArchetypeComponentInfoList(header.owner, componentType);
				const auto& offsets = GetArchetypeComponentOffsetList(header.owner, componentType);
				const auto idx = utils::get_index_unsafe(componentIds, componentId);

				if constexpr (UpdateWorldVersion) {
					// Update version number so we know RW access was used on chunk
					header.UpdateWorldVersion(componentType, idx);
				}

				return (uint8_t*)&data[offsets[idx]];
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
			Checks if component is present on the chunk.
			\return True if the component is present. False otherwise.
			*/
			template <typename T>
			GAIA_NODISCARD bool HasComponent() const {
				if constexpr (IsGenericComponent<T>) {
					using U = typename detail::ExtractComponentType_Generic<T>::Type;
					const auto componentId = GetComponentIdUnsafe<U>();
					return HasComponent_Internal(ComponentType::CT_Generic, componentId);
				} else {
					using U = typename detail::ExtractComponentType_NonGeneric<T>::Type;
					const auto componentId = GetComponentIdUnsafe<U>();
					return HasComponent_Internal(ComponentType::CT_Chunk, componentId);
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
			GAIA_NODISCARD auto GetComponent_Internal(uint32_t index) const {
				using U = typename DeduceComponent<T>::Type;
				using RetValue = decltype(View<T>()[0]);

				if constexpr (sizeof(RetValue) > 8)
					return (const U&)View<T>()[index];
				else
					return View<T>()[index];
			}

			template <typename T>
			GAIA_NODISCARD auto GetComponent(uint32_t index) const {
				static_assert(
						IsGenericComponent<T>, "GetComponent providing an index is only available for generic components");
				return GetComponent_Internal<T>(index);
			}

			template <typename T>
			GAIA_NODISCARD auto GetComponent() const {
				static_assert(
						!IsGenericComponent<T>, "GetComponent not providing an index is only available for non-generic components");
				return GetComponent_Internal<T>(0);
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
			GAIA_NODISCARD bool DidChange(ComponentType componentType, uint32_t version, uint32_t componentIdx) const {
				return DidVersionChange(header.versions[componentType][componentIdx], version);
			}
		};
		static_assert(sizeof(Chunk) <= ChunkMemorySize, "Chunk size must match ChunkMemorySize!");
	} // namespace ecs
} // namespace gaia
