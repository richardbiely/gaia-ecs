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
#include "chunk_allocator.h"
#include "chunk_header.h"
#include "common.h"
#include "component_cache.h"
#include "entity.h"
#include "fwd.h"

namespace gaia {
	namespace ecs {
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
			friend class CommandBuffer;

			//! Chunk header
			ChunkHeader m_header;
			//! Chunk data (entites + components)
			uint8_t m_data[DATA_SIZE_NORESERVE];

			GAIA_MSVC_WARNING_PUSH()
			GAIA_MSVC_WARNING_DISABLE(26495)

			Chunk(
					uint32_t archetypeId, uint16_t chunkIndex, uint16_t capacity, uint32_t& worldVersion,
					const containers::sarray<ComponentIdList, ComponentType::CT_Count>& componentIds,
					const containers::sarray<ComponentOffsetList, ComponentType::CT_Count>& componentOffsets):
					m_header(worldVersion) {
				m_header.archetypeId = archetypeId;
				m_header.index = chunkIndex;
				m_header.capacity = capacity;
				m_header.componentIds = componentIds;
				m_header.componentOffsets = componentOffsets;
			}

			GAIA_MSVC_WARNING_POP()

			/*!
			Make the \param entity entity a part of the chunk at the version of the world
			\return Index of the entity within the chunk.
			*/
			GAIA_NODISCARD uint32_t AddEntity(Entity entity) {
				const auto index = m_header.count++;
				SetEntity(index, entity);

				UpdateVersion(m_header.worldVersion);
				m_header.UpdateWorldVersion(ComponentType::CT_Generic);
				m_header.UpdateWorldVersion(ComponentType::CT_Chunk);

				return index;
			}

			/*!
			Remove the entity at \param index from the \param entities array.
			*/
			void RemoveEntity(
					uint32_t index, containers::darray<EntityContainer>& entities, const ComponentIdList& componentIds,
					const ComponentOffsetList& offsets) {
				// Ignore requests on empty chunks
				if (m_header.count == 0)
					return;

				// We can't be removing from an index which is no longer there
				GAIA_ASSERT(index < m_header.count);

				// If there are at least two entities inside and it's not already the
				// last one let's swap our entity with the last one in the chunk.
				if (m_header.count > 1 && m_header.count != index + 1) {
					// Swap data at index with the last one
					const auto entity = GetEntity(m_header.count - 1);
					SetEntity(index, entity);

					for (size_t i = 0; i < componentIds.size(); i++) {
						const auto& desc = ComponentCache::Get().GetComponentDesc(componentIds[i]);
						if (desc.properties.size == 0U)
							continue;

						const auto offset = offsets[i];
						const auto idxSrc = offset + index * desc.properties.size;
						const auto idxDst = offset + (m_header.count - 1U) * desc.properties.size;

						GAIA_ASSERT(idxSrc < Chunk::DATA_SIZE_NORESERVE);
						GAIA_ASSERT(idxDst < Chunk::DATA_SIZE_NORESERVE);
						GAIA_ASSERT(idxSrc != idxDst);

						auto* pSrc = (void*)&m_data[idxSrc];
						auto* pDst = (void*)&m_data[idxDst];

						if (desc.properties.movable == 1) {
							desc.move(pSrc, pDst);
						} else if (desc.properties.copyable == 1) {
							desc.copy(pSrc, pDst);
						} else
							memmove(pDst, (const void*)pSrc, desc.properties.size);

						if (desc.properties.destructible == 1)
							desc.destructor(pSrc, 1);
					}

					// Entity has been replaced with the last one in chunk.
					// Update its index so look ups can find it.
					entities[entity.id()].idx = index;
					entities[entity.id()].gen = entity.gen();
				}

				UpdateVersion(m_header.worldVersion);
				m_header.UpdateWorldVersion(ComponentType::CT_Generic);
				m_header.UpdateWorldVersion(ComponentType::CT_Chunk);

				--m_header.count;
			}

			/*!
			Makes the entity a part of a chunk on a given index.
			\param index Index of the entity
			\param entity Entity to store in the chunk
			*/
			void SetEntity(uint32_t index, Entity entity) {
				GAIA_ASSERT(index < m_header.count && "Entity index in chunk out of bounds!");

				utils::unaligned_ref<Entity> mem((void*)&m_data[sizeof(Entity) * index]);
				mem = entity;
			}

			/*!
			Returns the entity on a given index in the chunk.
			\param index Index of the entity
			\return Entity on a given index within the chunk.
			*/
			GAIA_NODISCARD Entity GetEntity(uint32_t index) const {
				GAIA_ASSERT(index < m_header.count && "Entity index in chunk out of bounds!");

				utils::unaligned_ref<Entity> mem((void*)&m_data[sizeof(Entity) * index]);
				return mem;
			}

			/*!
			Returns a pointer to component data with read-only access.
			\param componentType Component type
			\param componentId Component id
			\return Const pointer to component data.
			*/
			GAIA_NODISCARD GAIA_FORCEINLINE const uint8_t*
			GetDataPtr(ComponentType componentType, uint32_t componentId) const {
				// Searching for a component that's not there! Programmer mistake.
				GAIA_ASSERT(HasComponent(componentType, componentId));

				const auto& componentIds = m_header.componentIds[componentType];
				const auto& offsets = m_header.componentOffsets[componentType];
				const auto componentIdx = (uint32_t)utils::get_index_unsafe(componentIds, componentId);
				const auto offset = offsets[componentIdx];

				return (const uint8_t*)&m_data[offset];
			}

			/*!
			Returns a pointer to component data within chunk with read-write access.
			Also updates the world version for the component.
			\warning It is expected the component with \param componentId is present. Undefined behavior otherwise.
			\tparam UpdateWorldVersion If true, the world version is updated as a result of the write access
			\param componentType Component type
			\param componentId Component id
			\return Pointer to component data.
			*/
			template <bool UpdateWorldVersion>
			GAIA_NODISCARD GAIA_FORCEINLINE uint8_t* GetDataPtrRW(ComponentType componentType, uint32_t componentId) {
				// Searching for a component that's not there! Programmer mistake.
				GAIA_ASSERT(HasComponent(componentType, componentId));
				// Don't use this with empty components. It's impossible to write to them anyway.
				GAIA_ASSERT(ComponentCache::Get().GetComponentDesc(componentId).properties.size != 0);

				const auto& componentIds = m_header.componentIds[componentType];
				const auto& offsets = m_header.componentOffsets[componentType];
				const auto componentIdx = (uint32_t)utils::get_index_unsafe(componentIds, componentId);
				const auto offset = offsets[componentIdx];

				if constexpr (UpdateWorldVersion) {
					UpdateVersion(m_header.worldVersion);
					// Update version number so we know RW access was used on chunk
					m_header.UpdateWorldVersion(componentType, componentIdx);
				}

				return (uint8_t*)&m_data[offset];
			}

			/*!
			Returns a read-only span of the component data.
			\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component
			\return Span of read-only component data.
			*/
			template <typename T>
			GAIA_NODISCARD GAIA_FORCEINLINE auto View_Internal() const {
				using U = typename DeduceComponent<T>::Type;
				using UConst = typename std::add_const_t<U>;

				if constexpr (std::is_same_v<U, Entity>) {
					return std::span<const Entity>{(const Entity*)&m_data[0], GetItemCount()};
				} else {
					static_assert(!std::is_empty_v<U>, "Attempting to get value of an empty component");

					const auto componentId = GetComponentIdUnsafe<U>();

					if constexpr (IsGenericComponent<T>) {
						const uint32_t count = GetItemCount();
						return std::span<UConst>{(UConst*)GetDataPtr(ComponentType::CT_Generic, componentId), count};
					} else
						return std::span<UConst>{(UConst*)GetDataPtr(ComponentType::CT_Chunk, componentId), 1};
				}
			}

			/*!
			Returns a read-write span of the component data. Also updates the world version for the component.
			\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component
			\tparam UpdateWorldVersion If true, the world version is updated as a result of the write access
			\return Span of read-write component data.
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

				if constexpr (IsGenericComponent<T>) {
					const uint32_t count = GetItemCount();
					return std::span<U>{(U*)GetDataPtrRW<UpdateWorldVersion>(ComponentType::CT_Generic, componentId), count};
				} else
					return std::span<U>{(U*)GetDataPtrRW<UpdateWorldVersion>(ComponentType::CT_Chunk, componentId), 1};
			}

			/*!
			Returns the value stored in the component \tparam T on \param index in the chunk.
			\warning It is expected the \param index is valid. Undefined behavior otherwise.
			\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component
			\param index Index of entity in the chunk
			\return Value stored in the component.
			*/
			template <typename T>
			GAIA_NODISCARD auto GetComponent_Internal(uint32_t index) const {
				using U = typename DeduceComponent<T>::Type;
				using RetValue = decltype(View<T>()[0]);

				GAIA_ASSERT(index < m_header.count);
				if constexpr (sizeof(RetValue) > 8)
					return (const U&)View<T>()[index];
				else
					return View<T>()[index];
			}

		public:
			/*!
			Allocates memory for a new chunk.
			\param chunkIndex Index of this chunk within the parent archetype
			\return Newly allocated chunk
			*/
			static Chunk* Create(
					uint32_t archetypeId, uint16_t chunkIndex, uint16_t capacity, uint32_t& worldVersion,
					const containers::sarray<ComponentIdList, ComponentType::CT_Count>& componentIds,
					const containers::sarray<ComponentOffsetList, ComponentType::CT_Count>& componentOffsets) {
#if GAIA_ECS_CHUNK_ALLOCATOR
				auto* pChunk = (Chunk*)ChunkAllocator::Get().Allocate();
				new (pChunk) Chunk(archetypeId, chunkIndex, capacity, worldVersion, componentIds, componentOffsets);
#else
				auto pChunk = new Chunk(archetypeId, chunkIndex, capacity, worldVersion, componentIds, componentOffsets);
#endif

				return pChunk;
			}

			static void Release(Chunk* pChunk) {
#if GAIA_ECS_CHUNK_ALLOCATOR
				pChunk->~Chunk();
				ChunkAllocator::Get().Release(pChunk);
#else
				delete pChunk;
#endif
			}

			/*!
			Returns a read-only entity or component view.
			\warning If \tparam T is a component it is expected it is present. Undefined behavior otherwise.
			\tparam T Component or Entity
			\return Entity of component view with read-only access
			*/
			template <typename T>
			GAIA_NODISCARD auto View() const {
				using U = typename DeduceComponent<T>::Type;

				return utils::auto_view_policy_get<std::add_const_t<U>>{{View_Internal<T>()}};
			}

			/*!
			Returns a mutable entity or component view.
			\warning If \tparam T is a component it is expected it is present. Undefined behavior otherwise.
			\tparam T Component or Entity
			\return Entity or component view with read-write access
			*/
			template <typename T>
			GAIA_NODISCARD auto ViewRW() {
				using U = typename DeduceComponent<T>::Type;
				static_assert(!std::is_same_v<U, Entity>);

				return utils::auto_view_policy_set<U>{{ViewRW_Internal<T, true>()}};
			}

			/*!
			Returns a mutable component view.
			Doesn't update the world version when the access is aquired.
			\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component
			\return Component view with read-write access
			*/
			template <typename T>
			GAIA_NODISCARD auto ViewRWSilent() {
				using U = typename DeduceComponent<T>::Type;
				static_assert(!std::is_same_v<U, Entity>);

				return utils::auto_view_policy_set<U>{{ViewRW_Internal<T, false>()}};
			}

			//----------------------------------------------------------------------
			// Check component presence
			//----------------------------------------------------------------------

			/*!
			Checks if a component with \param componentId and type \param componentType is present in the archetype.
			\param componentId Component id
			\param componentType Component type
			\return True if found. False otherwise.
			*/
			GAIA_NODISCARD bool HasComponent(ComponentType componentType, uint32_t componentId) const {
				const auto& componentIds = m_header.componentIds[componentType];
				return utils::has(componentIds, componentId);
			}

			/*!
			Checks if component \tparam T is present in the chunk.
			\tparam T Component
			\return True if the component is present. False otherwise.
			*/
			template <typename T>
			GAIA_NODISCARD bool HasComponent() const {
				if constexpr (IsGenericComponent<T>) {
					using U = typename detail::ExtractComponentType_Generic<T>::Type;
					const auto componentId = GetComponentIdUnsafe<U>();
					return HasComponent(ComponentType::CT_Generic, componentId);
				} else {
					using U = typename detail::ExtractComponentType_NonGeneric<T>::Type;
					const auto componentId = GetComponentIdUnsafe<U>();
					return HasComponent(ComponentType::CT_Chunk, componentId);
				}
			}

			//----------------------------------------------------------------------
			// Set component data
			//----------------------------------------------------------------------

			/*!
			Sets the value of the chunk component \tparam T on \param index in the chunk.
			\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component
			\param index Index of entity in the chunk
			\param value Value to set for the component
			*/
			template <typename T>
			void SetComponent(uint32_t index, typename DeduceComponent<T>::Type&& value) {
				using U = typename DeduceComponent<T>::Type;

				static_assert(
						IsGenericComponent<T>, "SetComponent providing an index can only be used with generic components");

				GAIA_ASSERT(index < m_header.capacity);
				ViewRW<T>()[index] = std::forward<U>(value);
			}

			/*!
			Sets the value of the chunk component \tparam T in the chunk.
			\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component
			\param value Value to set for the component
			*/
			template <typename T>
			void SetComponent(typename DeduceComponent<T>::Type&& value) {
				using U = typename DeduceComponent<T>::Type;

				static_assert(
						!IsGenericComponent<T>, "SetComponent not providing an index can only be used with chunk components");

				GAIA_ASSERT(0 < m_header.capacity);
				ViewRW<T>()[0] = std::forward<U>(value);
			}

			/*!
			Sets the value of the chunk component \tparam T on \param index in the chunk.
			\warning World version is not updated so EntityQuery filters will not be able to catch this change.
			\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component
			\param index Index of entity in the chunk
			\param value Value to set for the component
			*/
			template <typename T>
			void SetComponentSilent(uint32_t index, typename DeduceComponent<T>::Type&& value) {
				using U = typename DeduceComponent<T>::Type;

				static_assert(
						IsGenericComponent<T>, "SetComponentSilent providing an index can only be used with generic components");

				GAIA_ASSERT(index < m_header.capacity);
				ViewRWSilent<T>()[index] = std::forward<U>(value);
			}

			/*!
			Sets the value of the chunk component \tparam T in the chunk.
			\warning World version is not updated so EntityQuery filters will not be able to catch this change.
			\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component
			\param value Value to set for the component
			*/
			template <typename T>
			void SetComponentSilent(typename DeduceComponent<T>::Type&& value) {
				using U = typename DeduceComponent<T>::Type;

				static_assert(
						!IsGenericComponent<T>, "SetComponentSilent not providing an index can only be used with chunk components");

				GAIA_ASSERT(0 < m_header.capacity);
				ViewRWSilent<T>()[0] = std::forward<U>(value);
			}

			//----------------------------------------------------------------------
			// Read component data
			//----------------------------------------------------------------------

			/*!
			Returns the value stored in the component \tparam T on \param index in the chunk.
			\warning It is expected the \param index is valid. Undefined behavior otherwise.
			\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component
			\param index Index of entity in the chunk
			\return Value stored in the component.
			*/
			template <typename T>
			GAIA_NODISCARD auto GetComponent(uint32_t index) const {
				static_assert(
						IsGenericComponent<T>, "GetComponent providing an index can only be used with generic components");
				return GetComponent_Internal<T>(index);
			}

			/*!
			Returns the value stored in the chunk component \tparam T.
			\warning It is expected the chunk component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component
			\return Value stored in the component.
			*/
			template <typename T>
			GAIA_NODISCARD auto GetComponent() const {
				static_assert(
						!IsGenericComponent<T>, "GetComponent not providing an index can only be used with chunk components");
				return GetComponent_Internal<T>(0);
			}

			/*!
			Returns the internal index of a component based on the provided \param componentId.
			\param componentType Component type
			\return Component index if the component was found. -1 otherwise.
			*/
			GAIA_NODISCARD uint32_t GetComponentIdx(ComponentType componentType, ComponentId componentId) const {
				const auto idx = utils::get_index_unsafe(m_header.componentIds[componentType], componentId);
				GAIA_ASSERT(idx != BadIndex);
				return (uint32_t)idx;
			}

			//----------------------------------------------------------------------
			// Iteration
			//----------------------------------------------------------------------

			template <typename T>
			constexpr GAIA_NODISCARD GAIA_FORCEINLINE auto GetComponentView() {
				using U = typename DeduceComponent<T>::Type;
				using UOriginal = typename DeduceComponent<T>::TypeOriginal;
				if constexpr (IsReadOnlyType<UOriginal>::value)
					return View_Internal<U>();
				else
					return ViewRW_Internal<U, true>();
			}

			template <typename... T, typename Func>
			GAIA_FORCEINLINE void ForEach([[maybe_unused]] utils::func_type_list<T...> types, Func func) {
				const size_t size = GetItemCount();
				GAIA_ASSERT(size > 0);

				if constexpr (sizeof...(T) > 0) {
					// Pointers to the respective component types in the chunk, e.g
					// 		q.ForEach([&](Position& p, const Velocity& v) {...}
					// Translates to:
					//  	auto p = chunk.ViewRW_Internal<Position, true>();
					//		auto v = chunk.View_Internal<Velocity>();
					auto dataPointerTuple = std::make_tuple(GetComponentView<T>()...);

					// Iterate over each entity in the chunk.
					// Translates to:
					//		for (size_t i = 0; i < chunk.GetItemCount(); ++i)
					//			func(p[i], v[i]);

					for (size_t i = 0; i < size; ++i)
						func(std::get<decltype(GetComponentView<T>())>(dataPointerTuple)[i]...);
				} else {
					// No functor parameters. Do an empty loop.
					for (size_t i = 0; i < size; ++i)
						func();
				}
			}

			//----------------------------------------------------------------------

			GAIA_NODISCARD uint32_t GetArchetypeId() const {
				return m_header.archetypeId;
			}

			void SetChunkIndex(uint16_t value) {
				m_header.index = value;
			}

			GAIA_NODISCARD uint16_t GetChunkIndex() const {
				return m_header.index;
			}

			void SetDisabled(bool value) {
				m_header.disabled = value;
			}

			GAIA_NODISCARD bool GetDisabled() const {
				return m_header.disabled;
			}

			//! Checks is this chunk is dying
			GAIA_NODISCARD bool IsDying() const {
				return m_header.lifespan > 0;
			}

			//! Checks is this chunk is disabled
			GAIA_NODISCARD bool IsDisabled() const {
				return m_header.disabled;
			}

			//! Checks is the full capacity of the has has been reached
			GAIA_NODISCARD bool IsFull() const {
				return m_header.count >= m_header.capacity;
			}

			//! Checks is there are any entities in the chunk
			GAIA_NODISCARD bool HasEntities() const {
				return m_header.count > 0;
			}

			//! Returns the number of entities in the chunk
			GAIA_NODISCARD uint32_t GetItemCount() const {
				return m_header.count;
			}

			//! Returns true if the provided version is newer than the one stored internally
			GAIA_NODISCARD bool DidChange(ComponentType componentType, uint32_t version, uint32_t componentIdx) const {
				return DidVersionChange(m_header.versions[componentType][componentIdx], version);
			}
		};
		static_assert(sizeof(Chunk) <= ChunkMemorySize, "Chunk size must match ChunkMemorySize!");
	} // namespace ecs
} // namespace gaia
