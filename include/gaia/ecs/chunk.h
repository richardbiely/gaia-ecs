#pragma once
#include "../config/config.h"

#include <cinttypes>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "../containers/sarray_ext.h"
#include "../utils/mem.h"
#include "../utils/utility.h"
#include "archetype_common.h"
#include "chunk_allocator.h"
#include "chunk_header.h"
#include "common.h"
#include "component.h"
#include "component_cache.h"
#include "entity.h"

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
			//! Chunk header
			ChunkHeader m_header;
			//! Chunk data (entites + components)
			uint8_t m_data[DATA_SIZE_NORESERVE];

			GAIA_MSVC_WARNING_PUSH()
			GAIA_MSVC_WARNING_DISABLE(26495)

			Chunk(
					uint32_t archetypeId, uint16_t chunkIndex, uint16_t capacity, uint32_t& worldVersion,
					const containers::sarray<archetype::ComponentIdArray, component::ComponentType::CT_Count>& componentIds,
					const containers::sarray<archetype::ComponentOffsetArray, component::ComponentType::CT_Count>&
							componentOffsets):
					m_header(worldVersion) {
				m_header.archetypeId = archetypeId;
				m_header.index = chunkIndex;
				m_header.capacity = capacity;
				m_header.componentIds = componentIds;
				m_header.componentOffsets = componentOffsets;

				const auto& cc = ComponentCache::Get();

				// Size of the entity + all of its generic components
				const auto& componentIdsGeneric = componentIds[component::ComponentType::CT_Generic];
				for (const auto componentId: componentIdsGeneric) {
					const auto& desc = cc.GetComponentDesc(componentId);
					m_header.has_custom_generic_ctor |= (desc.properties.has_custom_ctor != 0);
					m_header.has_custom_generic_dtor |= (desc.properties.has_custom_dtor != 0);
				}

				// Size of chunk components
				const auto& componentIdsChunk = componentIds[component::ComponentType::CT_Chunk];
				for (const auto componentId: componentIdsChunk) {
					const auto& desc = cc.GetComponentDesc(componentId);
					m_header.has_custom_chunk_ctor |= (desc.properties.has_custom_ctor != 0);
					m_header.has_custom_chunk_dtor |= (desc.properties.has_custom_dtor != 0);
				}
			}

			GAIA_MSVC_WARNING_POP()

			/*!
			Returns a read-only span of the component data.
			\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component
			\return Span of read-only component data.
			*/
			template <typename T>
			GAIA_NODISCARD GAIA_FORCEINLINE auto View_Internal() const {
				using U = typename component::DeduceComponent<T>::Type;
				using UConst = typename std::add_const_t<U>;

				if constexpr (std::is_same_v<U, Entity>) {
					return std::span<const Entity>{(const Entity*)&m_data[0], GetEntityCount()};
				} else {
					static_assert(!std::is_empty_v<U>, "Attempting to get value of an empty component");

					const auto componentId = component::GetComponentId<T>();

					if constexpr (component::IsGenericComponent<T>) {
						const uint32_t count = GetEntityCount();
						return std::span<UConst>{(UConst*)GetDataPtr(component::ComponentType::CT_Generic, componentId), count};
					} else
						return std::span<UConst>{(UConst*)GetDataPtr(component::ComponentType::CT_Chunk, componentId), 1};
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
				using U = typename component::DeduceComponent<T>::Type;
#if GAIA_COMPILER_MSVC && _MSC_VER <= 1916
				// Workaround for MSVC 2017 bug where it incorrectly evaluates the static assert
				// even in context where it shouldn't.
				// Unfortunatelly, even runtime assert can't be used...
				// GAIA_ASSERT(!std::is_same_v<U, Entity>::value);
#else
				static_assert(!std::is_same_v<U, Entity>);
#endif
				static_assert(!std::is_empty_v<U>, "Attempting to set value of an empty component");

				const auto componentId = component::GetComponentId<T>();

				if constexpr (component::IsGenericComponent<T>) {
					const uint32_t count = GetEntityCount();
					return std::span<U>{
							(U*)GetDataPtrRW<UpdateWorldVersion>(component::ComponentType::CT_Generic, componentId), count};
				} else
					return std::span<U>{(U*)GetDataPtrRW<UpdateWorldVersion>(component::ComponentType::CT_Chunk, componentId), 1};
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
				using U = typename component::DeduceComponent<T>::Type;
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
					const containers::sarray<archetype::ComponentIdArray, component::ComponentType::CT_Count>& componentIds,
					const containers::sarray<archetype::ComponentOffsetArray, component::ComponentType::CT_Count>&
							componentOffsets) {
#if GAIA_ECS_CHUNK_ALLOCATOR
				auto* pChunk = (Chunk*)ChunkAllocator::Get().Allocate();
				new (pChunk) Chunk(archetypeId, chunkIndex, capacity, worldVersion, componentIds, componentOffsets);
#else
				auto pChunk = new Chunk(archetypeId, chunkIndex, capacity, worldVersion, componentIds, componentOffsets);
#endif

				return pChunk;
			}

			/*!
			Releases all memory allocated by \param pChunk.
			\param pChunk Chunk which we want to destroy
			*/
			static void Release(Chunk* pChunk) {
				GAIA_ASSERT(pChunk != nullptr);

				// Call destructors for components that need it
				if (pChunk->HasAnyCustomGenericDestructor())
					pChunk->CallDestructors(component::ComponentType::CT_Generic, 0, pChunk->GetEntityCount());
				if (pChunk->HasAnyCustomChunkDestructor())
					pChunk->CallDestructors(component::ComponentType::CT_Chunk, 0, 1);

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
				using U = typename component::DeduceComponent<T>::Type;

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
				using U = typename component::DeduceComponent<T>::Type;
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
				using U = typename component::DeduceComponent<T>::Type;
				static_assert(!std::is_same_v<U, Entity>);

				return utils::auto_view_policy_set<U>{{ViewRW_Internal<T, false>()}};
			}

			/*!
			Make the \param entity entity a part of the chunk at the version of the world
			\return Index of the entity within the chunk.
			*/
			GAIA_NODISCARD uint32_t AddEntity(Entity entity) {
				const auto index = m_header.count++;
				SetEntity(index, entity);

				UpdateVersion(m_header.worldVersion);
				m_header.UpdateWorldVersion(component::ComponentType::CT_Generic);
				m_header.UpdateWorldVersion(component::ComponentType::CT_Chunk);

				return index;
			}

			/*!
			Remove the entity at \param index from the \param entities array.
			*/
			void RemoveEntity(uint32_t index, containers::darray<EntityContainer>& entities) {
				// Ignore requests on empty chunks
				if (!HasEntities())
					return;

				// We can't be removing from an index which is no longer there
				GAIA_ASSERT(index < m_header.count);

				// If there are at least two entities inside and it's not already the
				// last one let's swap our entity with the last one in the chunk.
				if GAIA_LIKELY (m_header.count > 1 && m_header.count != index + 1) {
					// Swap data at index with the last one
					const auto entity = GetEntity(m_header.count - 1);
					SetEntity(index, entity);

					const auto& cc = ComponentCache::Get();
					const auto& componentIds = m_header.componentIds[component::ComponentType::CT_Generic];
					const auto& componentOffsets = m_header.componentOffsets[component::ComponentType::CT_Generic];

					for (size_t i = 0; i < componentIds.size(); i++) {
						const auto& desc = cc.GetComponentDesc(componentIds[i]);
						if (desc.properties.size == 0U)
							continue;

						const auto offset = componentOffsets[i];
						const auto idxSrc = offset + index * desc.properties.size;
						const auto idxDst = offset + (m_header.count - 1U) * desc.properties.size;

						GAIA_ASSERT(idxSrc < Chunk::DATA_SIZE_NORESERVE);
						GAIA_ASSERT(idxDst < Chunk::DATA_SIZE_NORESERVE);
						GAIA_ASSERT(idxSrc != idxDst);

						auto* pSrc = (void*)&m_data[idxSrc];
						auto* pDst = (void*)&m_data[idxDst];

						if (desc.properties.has_custom_move == 1) {
							desc.move(pSrc, pDst);
						} else if (desc.properties.has_custom_copy == 1) {
							desc.copy(pSrc, pDst);
						} else
							memmove(pDst, (const void*)pSrc, desc.properties.size);

						if (desc.properties.has_custom_dtor == 1)
							desc.dtor(pSrc, 1);
					}

					// Entity has been replaced with the last one in chunk.
					// Update its index so look ups can find it.
					entities[entity.id()].idx = index;
					entities[entity.id()].gen = entity.gen();
				}

				UpdateVersion(m_header.worldVersion);
				m_header.UpdateWorldVersion(component::ComponentType::CT_Generic);
				m_header.UpdateWorldVersion(component::ComponentType::CT_Chunk);

				--m_header.count;
			}

			/*!
			Makes the entity a part of a chunk on a given index.
			\param index Index of the entity
			\param entity Entity to store in the chunk
			*/
			void SetEntity(uint32_t index, Entity entity) {
				GAIA_ASSERT(index < m_header.count && "Entity index in chunk out of bounds!");

				const auto offset = sizeof(Entity) * index;
				utils::unaligned_ref<Entity> mem((void*)&m_data[offset]);
				mem = entity;
			}

			/*!
			Returns the entity on a given index in the chunk.
			\param index Index of the entity
			\return Entity on a given index within the chunk.
			*/
			GAIA_NODISCARD Entity GetEntity(uint32_t index) const {
				GAIA_ASSERT(index < m_header.count && "Entity index in chunk out of bounds!");

				const auto offset = sizeof(Entity) * index;
				utils::unaligned_ref<Entity> mem((void*)&m_data[offset]);
				return mem;
			}

			/*!
			Returns a pointer to chunk data.
			\param offset Offset into chunk data
			\return Pointer to chunk data.
			*/
			uint8_t& GetData(uint32_t offset) {
				return m_data[offset];
			}

			/*!
			Returns a pointer to component data with read-only access.
			\param componentType Component type
			\param componentId Component id
			\return Const pointer to component data.
			*/
			GAIA_NODISCARD GAIA_FORCEINLINE const uint8_t*
			GetDataPtr(component::ComponentType componentType, component::ComponentId componentId) const {
				// Searching for a component that's not there! Programmer mistake.
				GAIA_ASSERT(HasComponent(componentType, componentId));

				const auto& componentIds = m_header.componentIds[componentType];
				const auto& componentOffsets = m_header.componentOffsets[componentType];
				const auto componentIdx = (uint32_t)utils::get_index_unsafe(componentIds, componentId);
				const auto componentOffset = componentOffsets[componentIdx];

				return (const uint8_t*)&m_data[componentOffset];
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
			GAIA_NODISCARD GAIA_FORCEINLINE uint8_t*
			GetDataPtrRW(component::ComponentType componentType, component::ComponentId componentId) {
				// Searching for a component that's not there! Programmer mistake.
				GAIA_ASSERT(HasComponent(componentType, componentId));
				// Don't use this with empty components. It's impossible to write to them anyway.
				GAIA_ASSERT(ComponentCache::Get().GetComponentDesc(componentId).properties.size != 0);

				const auto& componentIds = m_header.componentIds[componentType];
				const auto& componentOffsets = m_header.componentOffsets[componentType];
				const auto componentIdx = (uint32_t)utils::get_index_unsafe(componentIds, componentId);
				const auto componentOffset = componentOffsets[componentIdx];

				if constexpr (UpdateWorldVersion) {
					UpdateVersion(m_header.worldVersion);
					// Update version number so we know RW access was used on chunk
					m_header.UpdateWorldVersion(componentType, componentIdx);
				}

				return (uint8_t*)&m_data[componentOffset];
			}

			//----------------------------------------------------------------------
			// Component handling
			//----------------------------------------------------------------------

			bool HasAnyCustomGenericConstructor() const {
				return m_header.has_custom_generic_ctor;
			}

			bool HasAnyCustomChunkConstructor() const {
				return m_header.has_custom_chunk_ctor;
			}

			bool HasAnyCustomGenericDestructor() const {
				return m_header.has_custom_generic_dtor;
			}

			bool HasAnyCustomChunkDestructor() const {
				return m_header.has_custom_chunk_dtor;
			}

			void CallConstructor(
					component::ComponentType componentType, component::ComponentId componentId, uint32_t entityIndex) {
				// Make sure only generic types are used with indices
				GAIA_ASSERT(componentType == component::ComponentType::CT_Generic || entityIndex == 0);

				const auto& cc = ComponentCache::Get();
				const auto& desc = cc.GetComponentDesc(componentId);
				if (desc.properties.has_custom_ctor == 0)
					return;

				const auto& componentOffsets = m_header.componentOffsets[componentType];
				const auto& componentIds = m_header.componentIds[componentType];

				const auto idx = utils::get_index_unsafe(componentIds, componentId);
				const auto offset = componentOffsets[idx];
				const auto idxSrc = offset + entityIndex * desc.properties.size;
				GAIA_ASSERT(idxSrc < Chunk::DATA_SIZE_NORESERVE);

				auto* pSrc = (void*)&m_data[idxSrc];
				desc.ctor(pSrc, 1);
			}

			void CallConstructors(component::ComponentType componentType, uint32_t entityIndex, uint32_t entityCount) {
				GAIA_ASSERT(
						componentType == component::ComponentType::CT_Generic && HasAnyCustomGenericConstructor() ||
						componentType == component::ComponentType::CT_Chunk && HasAnyCustomChunkConstructor());

				// Make sure only generic types are used with indices
				GAIA_ASSERT(componentType == component::ComponentType::CT_Generic || (entityIndex == 0 && entityCount == 1));

				const auto& cc = ComponentCache::Get();
				const auto& componentIds = m_header.componentIds[componentType];
				const auto& componentOffsets = m_header.componentOffsets[componentType];

				for (size_t i = 0; i < componentIds.size(); i++) {
					const auto& desc = cc.GetComponentDesc(componentIds[i]);
					if (desc.properties.has_custom_ctor == 0)
						continue;

					const auto offset = componentOffsets[i];
					const auto idxSrc = offset + entityIndex * desc.properties.size;
					GAIA_ASSERT(idxSrc < Chunk::DATA_SIZE_NORESERVE);

					auto* pSrc = (void*)&m_data[idxSrc];
					desc.ctor(pSrc, entityCount);
				}
			}

			void CallDestructors(component::ComponentType componentType, uint32_t entityIndex, uint32_t entityCount) {
				GAIA_ASSERT(
						componentType == component::ComponentType::CT_Generic && HasAnyCustomGenericDestructor() ||
						componentType == component::ComponentType::CT_Chunk && HasAnyCustomChunkDestructor());

				// Make sure only generic types are used with indices
				GAIA_ASSERT(componentType == component::ComponentType::CT_Generic || (entityIndex == 0 && entityCount == 1));

				const auto& cc = ComponentCache::Get();
				const auto& componentIds = m_header.componentIds[componentType];
				const auto& componentOffsets = m_header.componentOffsets[componentType];

				for (size_t i = 0; i < componentIds.size(); ++i) {
					const auto& desc = cc.GetComponentDesc(componentIds[i]);
					if (desc.properties.has_custom_dtor == 0)
						continue;

					const auto offset = componentOffsets[i];
					const auto idxSrc = offset + entityIndex * desc.properties.size;
					GAIA_ASSERT(idxSrc < Chunk::DATA_SIZE_NORESERVE);

					auto* pSrc = (void*)&m_data[idxSrc];
					desc.dtor(pSrc, entityCount);
				}
			};

			//----------------------------------------------------------------------
			// Check component presence
			//----------------------------------------------------------------------

			/*!
			Checks if a component with \param componentId and type \param componentType is present in the archetype.
			\param componentId Component id
			\param componentType Component type
			\return True if found. False otherwise.
			*/
			GAIA_NODISCARD bool
			HasComponent(component::ComponentType componentType, component::ComponentId componentId) const {
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
				const auto componentId = component::GetComponentId<T>();

				if constexpr (component::IsGenericComponent<T>)
					return HasComponent(component::ComponentType::CT_Generic, componentId);
				else
					return HasComponent(component::ComponentType::CT_Chunk, componentId);
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
			template <typename T, typename U = typename component::DeduceComponent<T>::Type>
			void SetComponent(uint32_t index, U&& value) {
				static_assert(
						component::IsGenericComponent<T>,
						"SetComponent providing an index can only be used with generic components");

				GAIA_ASSERT(index < m_header.capacity);
				ViewRW<T>()[index] = std::forward<U>(value);
			}

			/*!
			Sets the value of the chunk component \tparam T in the chunk.
			\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component
			\param value Value to set for the component
			*/
			template <typename T, typename U = typename component::DeduceComponent<T>::Type>
			void SetComponent(U&& value) {
				static_assert(
						!component::IsGenericComponent<T>,
						"SetComponent not providing an index can only be used with chunk components");

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
			template <typename T, typename U = typename component::DeduceComponent<T>::Type>
			void SetComponentSilent(uint32_t index, U&& value) {
				static_assert(
						component::IsGenericComponent<T>,
						"SetComponentSilent providing an index can only be used with generic components");

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
			template <typename T, typename U = typename component::DeduceComponent<T>::Type>
			void SetComponentSilent(U&& value) {
				static_assert(
						!component::IsGenericComponent<T>,
						"SetComponentSilent not providing an index can only be used with chunk components");

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
						component::IsGenericComponent<T>,
						"GetComponent providing an index can only be used with generic components");
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
						!component::IsGenericComponent<T>,
						"GetComponent not providing an index can only be used with chunk components");
				return GetComponent_Internal<T>(0);
			}

			/*!
			Returns the internal index of a component based on the provided \param componentId.
			\param componentType Component type
			\return Component index if the component was found. -1 otherwise.
			*/
			GAIA_NODISCARD uint32_t
			GetComponentIdx(component::ComponentType componentType, component::ComponentId componentId) const {
				const auto idx = utils::get_index_unsafe(m_header.componentIds[componentType], componentId);
				GAIA_ASSERT(idx != BadIndex);
				return (uint32_t)idx;
			}

			//----------------------------------------------------------------------
			// Iteration
			//----------------------------------------------------------------------

			template <typename T>
			constexpr GAIA_NODISCARD GAIA_FORCEINLINE auto GetComponentView() {
				using U = typename component::DeduceComponent<T>::Type;
				using UOriginal = typename component::DeduceComponent<T>::TypeOriginal;
				if constexpr (component::IsReadOnlyType<UOriginal>::value)
					return View_Internal<U>();
				else
					return ViewRW_Internal<U, true>();
			}

			template <typename... T, typename Func>
			GAIA_FORCEINLINE void ForEach([[maybe_unused]] utils::func_type_list<T...> types, Func func) {
				const size_t size = GetEntityCount();
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
					//		for (size_t i = 0; i < chunk.GetEntityCount(); ++i)
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
				return m_header.lifespanCountdown > 0;
			}

			void PrepareToDie() {
				m_header.lifespanCountdown = MAX_CHUNK_LIFESPAN;
			}

			bool ProgressDeath() {
				GAIA_ASSERT(IsDying());
				--m_header.lifespanCountdown;
				return IsDying();
			}

			void SetStructuralChanges(bool value) {
				if (value) {
					GAIA_ASSERT(m_header.structuralChangesLocked < 16);
					++m_header.structuralChangesLocked;
				} else {
					GAIA_ASSERT(m_header.structuralChangesLocked > 0);
					--m_header.structuralChangesLocked;
				}
			}

			bool IsStructuralChangesLocked() const {
				return m_header.structuralChangesLocked != 0;
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
			GAIA_NODISCARD uint32_t GetEntityCount() const {
				return m_header.count;
			}

			GAIA_NODISCARD const archetype::ComponentIdArray&
			GetComponentIdArray(component::ComponentType componentType) const {
				return m_header.componentIds[componentType];
			}

			GAIA_NODISCARD const archetype::ComponentOffsetArray&
			GetComponentOffsetArray(component::ComponentType componentType) const {
				return m_header.componentOffsets[componentType];
			}

			//! Returns true if the provided version is newer than the one stored internally
			GAIA_NODISCARD bool
			DidChange(component::ComponentType componentType, uint32_t version, uint32_t componentIdx) const {
				return DidVersionChange(m_header.versions[componentType][componentIdx], version);
			}

			void Diag(uint32_t index) const {
				GAIA_LOG_N(
						"  Chunk #%04u, entities:%u/%u, lifespanCountdown:%u", index, m_header.count, m_header.capacity,
						m_header.lifespanCountdown);
			}
		};
		static_assert(sizeof(Chunk) <= ChunkMemorySize, "Chunk size must match ChunkMemorySize!");
	} // namespace ecs
} // namespace gaia
