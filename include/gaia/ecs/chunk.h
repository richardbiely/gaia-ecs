#pragma once
#include "../config/config.h"

#include <cinttypes>
#include <cstdint>
#include <tuple>
#include <type_traits>
#include <utility>

#include "../cnt/sarray_ext.h"
#include "../config/profiler.h"
#include "../core/utility.h"
#include "archetype_common.h"
#include "chunk_allocator.h"
#include "chunk_header.h"
#include "common.h"
#include "component.h"
#include "component_cache.h"
#include "component_utils.h"
#include "entity.h"

namespace gaia {
	namespace ecs {
		namespace archetype {
			class Chunk final {
			private:
				//! Chunk header
				ChunkHeader m_header;
				//! Pointer to where the chunk data starts.
				//! Data layed out as following:
				//!			1) ComponentVersions[component::ComponentType::CT_Generic]
				//!			2) ComponentVersions[component::ComponentType::CT_Chunk]
				//!     3) ComponentIds[component::ComponentType::CT_Generic]
				//!			4) ComponentIds[component::ComponentType::CT_Chunk]
				//!			5) ComponentOffsets[component::ComponentType::CT_Generic]
				//!			6) ComponentOffsets[component::ComponentType::CT_Chunk]
				//!			7) Entities
				//!			8) Components
				//! Note, root archetypes store only entites, therefore it is fully occupied with entities.
				uint8_t m_data[1];

				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(26495)

				Chunk(
						uint32_t archetypeId, uint16_t chunkIndex, uint16_t capacity, uint16_t st, uint32_t& worldVersion,
						const ChunkHeaderOffsets& headerOffsets):
						m_header(archetypeId, chunkIndex, capacity, st, headerOffsets, worldVersion) {
					// Chunk data area consist of memory offsets + component data. Normally. we would initialize it.
					// However, the memory offsets part are all trivial types and components are initialized via their
					// constructors so we do not really need to do anything.
					// const uint32_t sizeBytes = ...;
					// auto* curr = (uint8_t*)&m_data[0] + 1;
					// for (uint32_t i = 0; i < sizeBytes; ++i, (void)++curr) {
					// 	const auto* addr = mem::addressof(*curr);
					// 	(void)new (const_cast<void*>(static_cast<const volatile void*>(addr))) uint8_t;
					// }
				}

				GAIA_MSVC_WARNING_POP()

				void Init(
						const cnt::sarray<ComponentIdArray, component::ComponentType::CT_Count>& componentIds,
						const cnt::sarray<ComponentOffsetArray, component::ComponentType::CT_Count>& componentOffsets) {
					m_header.componentCount[component::ComponentType::CT_Generic] =
							(uint8_t)componentIds[component::ComponentType::CT_Generic].size();
					m_header.componentCount[component::ComponentType::CT_Chunk] =
							(uint8_t)componentIds[component::ComponentType::CT_Chunk].size();

					const auto& cc = ComponentCache::Get();

					const auto& componentIdsGeneric = componentIds[component::ComponentType::CT_Generic];
					for (const auto componentId: componentIdsGeneric) {
						const auto& desc = cc.GetComponentDesc(componentId);
						m_header.hasAnyCustomGenericCtor |= (desc.ctor != nullptr);
						m_header.hasAnyCustomGenericDtor |= (desc.dtor != nullptr);
					}
					const auto& componentIdsChunk = componentIds[component::ComponentType::CT_Chunk];
					for (const auto componentId: componentIdsChunk) {
						const auto& desc = cc.GetComponentDesc(componentId);
						m_header.hasAnyCustomChunkCtor |= (desc.ctor != nullptr);
						m_header.hasAnyCustomChunkDtor |= (desc.dtor != nullptr);
					}

					// Copy provided component id data to this chunk's data area
					{
						for (uint32_t i = 0; i < component::ComponentType::CT_Count; ++i) {
							auto offset = m_header.offsets.firstByte_ComponentIds[i];
							for (const auto componentId: componentIds[i]) {
								// unaligned_ref not necessary because data is aligned
								*(component::ComponentId*)&m_data[offset] = componentId;
								offset += sizeof(component::ComponentId);
							}
						}
					}

					// Copy provided component offset data to this chunk's data area
					{
						for (uint32_t i = 0; i < component::ComponentType::CT_Count; ++i) {
							auto offset = m_header.offsets.firstByte_ComponentOffsets[i];
							for (const auto componentOffset: componentOffsets[i]) {
								// unaligned_ref not necessary because data is aligned
								*(archetype::ChunkComponentOffset*)&m_data[offset] = componentOffset;
								offset += sizeof(archetype::ChunkComponentOffset);
							}
						}
					}
				}

				/*!
				Returns a read-only span of the component data.
				\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
				\tparam T Component
				\return Span of read-only component data.
				*/
				template <typename T>
				GAIA_NODISCARD GAIA_FORCEINLINE auto View_Internal() const -> decltype(std::span<const uint8_t>{}) {
					using U = typename component::component_type_t<T>::Type;

					if constexpr (std::is_same_v<U, Entity>) {
						return {(const uint8_t*)&GetData(m_header.offsets.firstByte_EntityData), GetEntityCount()};
					} else {
						static_assert(!std::is_empty_v<U>, "Attempting to get value of an empty component");

						const auto componentId = component::GetComponentId<T>();
						constexpr auto componentType = component::component_type_v<T>;

						// Find at what byte offset the first component of a given type is located
						uint32_t componentIdx = 0;
						const auto offset = FindDataOffset(componentType, componentId, componentIdx);

						if constexpr (componentType == component::ComponentType::CT_Generic) {
							[[maybe_unused]] const auto capacity = GetEntityCapacity();
							[[maybe_unused]] const auto maxOffset = offset + capacity * sizeof(U);
							GAIA_ASSERT(maxOffset <= GetByteSize());

							return {(const uint8_t*)&GetData(offset), GetEntityCount()};
						} else {
							[[maybe_unused]] const auto maxOffset = offset + sizeof(U);
							GAIA_ASSERT(maxOffset <= GetByteSize());

							return {(const uint8_t*)&GetData(offset), 1};
						}
					}
				}

				/*!
				Returns a read-write span of the component data. Also updates the world version for the component.
				\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
				\tparam T Component
				\tparam WorldVersionUpdateWanted If true, the world version is updated as a result of the write access
				\return Span of read-write component data.
				*/
				template <typename T, bool WorldVersionUpdateWanted>
				GAIA_NODISCARD GAIA_FORCEINLINE auto ViewRW_Internal() -> decltype(std::span<uint8_t>{}) {
					using U = typename component::component_type_t<T>::Type;
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
					constexpr auto componentType = component::component_type_v<T>;

					// Find at what byte offset the first component of a given type is located
					uint32_t componentIdx = 0;
					const auto offset = FindDataOffset(componentType, componentId, componentIdx);

					// Update version number if necessary so we know RW access was used on the chunk
					if constexpr (WorldVersionUpdateWanted)
						this->UpdateWorldVersion(componentType, componentIdx);

					if constexpr (componentType == component::ComponentType::CT_Generic) {
						[[maybe_unused]] const auto capacity = GetEntityCapacity();
						[[maybe_unused]] const auto maxOffset = offset + capacity * sizeof(U);
						GAIA_ASSERT(maxOffset <= GetByteSize());

						return {(uint8_t*)&GetData(offset), GetEntityCount()};
					} else {
						[[maybe_unused]] const auto maxOffset = offset + sizeof(U);
						GAIA_ASSERT(maxOffset <= GetByteSize());

						return {(uint8_t*)&GetData(offset), 1};
					}
				}

				/*!
				Returns the value stored in the component \tparam T on \param index in the chunk.
				\warning It is expected the \param index is valid. Undefined behavior otherwise.
				\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
				\tparam T Component
				\param index Index of entity in the chunk
				\return Value stored in the component if smaller than 8 bytes. Const reference to the value otherwise.
				*/
				template <typename T>
				GAIA_NODISCARD auto GetComponent_Internal(uint32_t index) const {
					using U = typename component::component_type_t<T>::Type;
					using RetValueType = decltype(View<T>()[0]);

					GAIA_ASSERT(index < m_header.count);
					if constexpr (sizeof(RetValueType) > 8)
						return (const U&)View<T>()[index];
					else
						return View<T>()[index];
				}

				/*!
				Removes the entity at from the chunk and updates the world versions
				*/
				void RemoveLastEntity_Internal() {
					// Should never be called over an empty chunk
					GAIA_ASSERT(HasEntities());
					--m_header.count;
					m_header.disabledEntityMask.set(m_header.count, false);
				}

			public:
				Chunk(const Chunk& chunk) = delete;
				Chunk(Chunk&& chunk) = delete;
				Chunk& operator=(const Chunk& chunk) = delete;
				Chunk& operator=(Chunk&& chunk) = delete;
				~Chunk() = default;

				static uint16_t GetTotalChunkSize(uint16_t dataSize) {
					uint16_t header = (uint16_t)sizeof(ChunkHeader) + (uint16_t)MemoryBlockUsableOffset;
					return header + dataSize;
				}

				static uint16_t GetChunkDataSize(uint16_t totalSize) {
					uint16_t header = (uint16_t)sizeof(ChunkHeader) + (uint16_t)MemoryBlockUsableOffset;
					return totalSize - header;
				}

				/*!
				Allocates memory for a new chunk.
				\param chunkIndex Index of this chunk within the parent archetype
				\return Newly allocated chunk
				*/
				static Chunk* Create(
						uint32_t archetypeId, uint16_t chunkIndex, uint16_t capacity, uint16_t dataBytes, uint32_t& worldVersion,
						const ChunkHeaderOffsets& offsets,
						const cnt::sarray<ComponentIdArray, component::ComponentType::CT_Count>& componentIds,
						const cnt::sarray<ComponentOffsetArray, component::ComponentType::CT_Count>& componentOffsets) {
					const auto totalBytes = GetTotalChunkSize(dataBytes);
					const auto sizeType = detail::ChunkAllocatorImpl::GetMemoryBlockSizeType(totalBytes);
#if GAIA_ECS_CHUNK_ALLOCATOR
					auto* pChunk = (Chunk*)ChunkAllocator::get().Allocate(totalBytes);
					new (pChunk) Chunk(archetypeId, chunkIndex, capacity, sizeType, worldVersion, offsets);
#else
					GAIA_ASSERT(totalBytes <= MaxMemoryBlockSize);
					const auto allocSize = detail::ChunkAllocatorImpl::GetMemoryBlockSize(sizeType);
					auto* pChunkMem = new uint8_t[allocSize];
					auto* pChunk = new (pChunkMem) Chunk(archetypeId, chunkIndex, capacity, sizeType, worldVersion, offsets);
#endif

					pChunk->Init(componentIds, componentOffsets);

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
					ChunkAllocator::get().Release(pChunk);
#else
					pChunk->~Chunk();
					auto* pChunkMem = (uint8_t*)pChunk;
					delete pChunkMem;
#endif
				}

				/*!
				Remove the last entity from chunk.
				\param chunksToRemove Container of chunks ready for removal
				*/
				void RemoveLastEntity(cnt::darray<archetype::Chunk*>& chunksToRemove) {
					GAIA_ASSERT(
							!IsStructuralChangesLocked() && "Entities can't be removed while their chunk is being iterated "
																							"(structural changes are forbidden during this time!)");

					RemoveLastEntity_Internal();

					if (!IsDying() && !HasEntities()) {
						// When the chunk is emptied we want it to be removed. We can't do it
						// right away and need to wait for world's GC to be called.
						//
						// However, we need to prevent the following:
						//    1) chunk is emptied, add it to some removal list
						//    2) chunk is reclaimed
						//    3) chunk is emptied, add it to some removal list again
						//
						// Therefore, we have a flag telling us the chunk is already waiting to
						// be removed. The chunk might be reclaimed before GC happens but it
						// simply ignores such requests. This way GC always has at most one
						// record for removal for any given chunk.
						PrepareToDie();

						chunksToRemove.push_back(this);
					}
				}

				//! Updates the version numbers for this chunk.
				void UpdateVersions() {
					UpdateVersion(m_header.worldVersion);
					UpdateWorldVersion(component::ComponentType::CT_Generic);
					UpdateWorldVersion(component::ComponentType::CT_Chunk);
				}

				/*!
				Returns a read-only entity or component view.
				\warning If \tparam T is a component it is expected it is present. Undefined behavior otherwise.
				\tparam T Component or Entity
				\return Entity of component view with read-only access
				*/
				template <typename T>
				GAIA_NODISCARD auto View() const {
					using U = typename component::component_type_t<T>::Type;

					return mem::auto_view_policy_get<U>{View_Internal<T>()};
				}

				/*!
				Returns a mutable entity or component view.
				\warning If \tparam T is a component it is expected it is present. Undefined behavior otherwise.
				\tparam T Component or Entity
				\return Entity or component view with read-write access
				*/
				template <typename T>
				GAIA_NODISCARD auto ViewRW() {
					using U = typename component::component_type_t<T>::Type;
					static_assert(!std::is_same_v<U, Entity>);

					return mem::auto_view_policy_set<U>{ViewRW_Internal<T, true>()};
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
					using U = typename component::component_type_t<T>::Type;
					static_assert(!std::is_same_v<U, Entity>);

					return mem::auto_view_policy_set<U>{ViewRW_Internal<T, false>()};
				}

				/*!
				Make \param entity a part of the chunk at the version of the world
				\return Index of the entity within the chunk.
				*/
				GAIA_NODISCARD uint32_t AddEntity(Entity entity) {
					const auto index = m_header.count++;
					SetEntity(index, entity);

					UpdateVersion(m_header.worldVersion);
					UpdateWorldVersion(component::ComponentType::CT_Generic);
					UpdateWorldVersion(component::ComponentType::CT_Chunk);

					return index;
				}

				/*!
				Copies all data associated with \param oldEntity into \param newEntity.
				*/
				static void CopyEntityData(Entity oldEntity, Entity newEntity, std::span<EntityContainer> entities) {
					GAIA_PROF_SCOPE(CopyEntityData);

					auto& oldEntityContainer = entities[oldEntity.id()];
					auto* pOldChunk = oldEntityContainer.pChunk;

					auto& newEntityContainer = entities[newEntity.id()];
					auto* pNewChunk = newEntityContainer.pChunk;

					GAIA_ASSERT(pOldChunk->GetArchetypeId() == pNewChunk->GetArchetypeId());

					const auto& cc = ComponentCache::Get();
					auto oldIds = pOldChunk->GetComponentIdSpan(component::ComponentType::CT_Generic);
					auto oldOffs = pOldChunk->GetComponentOffsetSpan(component::ComponentType::CT_Generic);

					// Copy generic component data from reference entity to our new entity
					for (uint32_t i = 0; i < oldIds.size(); i++) {
						const auto& desc = cc.GetComponentDesc(oldIds[i]);
						if (desc.properties.size == 0U)
							continue;

						const auto offset = oldOffs[i];
						const auto idxSrc = offset + desc.properties.size * (uint32_t)oldEntityContainer.idx;
						const auto idxDst = offset + desc.properties.size * (uint32_t)newEntityContainer.idx;

						GAIA_ASSERT(idxSrc < pOldChunk->GetByteSize());
						GAIA_ASSERT(idxDst < pNewChunk->GetByteSize());

						auto* pSrc = (void*)&pOldChunk->GetData(idxSrc);
						auto* pDst = (void*)&pNewChunk->GetData(idxDst);
						desc.Copy(pSrc, pDst);
					}
				}

				/*!
				Moves all data associated with \param entity into the chunk so that it is stored at \param newEntityIdx.
				*/
				void MoveEntityData(Entity entity, uint32_t newEntityIdx, std::span<EntityContainer> entities) {
					GAIA_PROF_SCOPE(CopyEntityFrom);

					auto& oldEntityContainer = entities[entity.id()];
					auto* pOldChunk = oldEntityContainer.pChunk;

					GAIA_ASSERT(pOldChunk->GetArchetypeId() == GetArchetypeId());

					const auto& cc = ComponentCache::Get();
					auto oldIds = pOldChunk->GetComponentIdSpan(component::ComponentType::CT_Generic);
					auto oldOffs = pOldChunk->GetComponentOffsetSpan(component::ComponentType::CT_Generic);

					// Copy generic component data from reference entity to our new entity
					for (uint32_t i = 0; i < oldIds.size(); i++) {
						const auto& desc = cc.GetComponentDesc(oldIds[i]);
						if (desc.properties.size == 0U)
							continue;

						const auto offset = oldOffs[i];
						const auto idxSrc = offset + desc.properties.size * (uint32_t)oldEntityContainer.idx;
						const auto idxDst = offset + desc.properties.size * newEntityIdx;

						GAIA_ASSERT(idxSrc < pOldChunk->GetByteSize());
						GAIA_ASSERT(idxDst < GetByteSize());

						auto* pSrc = (void*)&pOldChunk->GetData(idxSrc);
						auto* pDst = (void*)&GetData(idxDst);
						desc.CtorFrom(pSrc, pDst);
					}
				}

				/*!
				Moves all data associated with \param entity into the chunk so that it is stored at index \param newEntityIdx.
				*/
				void MoveForeignEntityData(Entity entity, uint32_t newEntityIdx, std::span<EntityContainer> entities) {
					GAIA_PROF_SCOPE(MoveForeignEntityData);

					auto& oldEntityContainer = entities[entity.id()];
					auto* pOldChunk = oldEntityContainer.pChunk;

					const auto& cc = ComponentCache::Get();

					// Find intersection of the two component lists.
					// We ignore chunk components here because they should't be influenced
					// by entities moving around.
					auto oldIds = pOldChunk->GetComponentIdSpan(component::ComponentType::CT_Generic);
					auto oldOffs = pOldChunk->GetComponentOffsetSpan(component::ComponentType::CT_Generic);
					auto newIds = GetComponentIdSpan(component::ComponentType::CT_Generic);
					auto newOffs = GetComponentOffsetSpan(component::ComponentType::CT_Generic);

					// Arrays are sorted so we can do linear intersection lookup
					{
						uint32_t i = 0;
						uint32_t j = 0;

						auto moveData = [&](const component::ComponentDesc& desc) {
							if (desc.properties.size == 0U)
								return;

							// Let's move all type data from oldEntity to newEntity
							const auto idxSrc = oldOffs[i] + desc.properties.size * (uint32_t)oldEntityContainer.idx;
							const auto idxDst = newOffs[j] + desc.properties.size * newEntityIdx;

							GAIA_ASSERT(idxSrc < pOldChunk->GetByteSize());
							GAIA_ASSERT(idxDst < GetByteSize());

							auto* pSrc = (void*)&pOldChunk->GetData(idxSrc);
							auto* pDst = (void*)&GetData(idxDst);
							desc.CtorFrom(pSrc, pDst);
						};

						while (i < oldIds.size() && j < newIds.size()) {
							const auto& descOld = cc.GetComponentDesc(oldIds[i]);
							const auto& descNew = cc.GetComponentDesc(newIds[j]);

							if (&descOld == &descNew) {
								moveData(descOld);
								++i;
								++j;
							} else if (component::SortComponentCond{}.operator()(descOld.componentId, descNew.componentId))
								++i;
							else
								++j;
						}
					}
				}

				void SwapEntitiesInsideChunkAndDeleteOld(uint32_t index, std::span<EntityContainer> entities) {
					// If there are at least two entities inside and it's not already the
					// last one let's swap our entity with the last one in the chunk.
					if GAIA_LIKELY (m_header.count > 1 && m_header.count != index + 1) {
						GAIA_PROF_SCOPE(SwapEntitiesInsideChunkAndDeleteOld);

						// Swap data at index with the last one
						const auto entity = GetEntity(m_header.count - 1);
						SetEntity(index, entity);

						const auto& cc = ComponentCache::Get();
						auto compIds = GetComponentIdSpan(component::ComponentType::CT_Generic);
						auto compOffs = GetComponentOffsetSpan(component::ComponentType::CT_Generic);

						for (uint32_t i = 0; i < compIds.size(); i++) {
							const auto& desc = cc.GetComponentDesc(compIds[i]);
							if (desc.properties.size == 0U)
								continue;

							const auto offset = compOffs[i];
							const auto idxSrc = offset + index * desc.properties.size;
							const auto idxDst = offset + (m_header.count - 1U) * desc.properties.size;

							GAIA_ASSERT(idxSrc < GetByteSize());
							GAIA_ASSERT(idxDst < GetByteSize());
							GAIA_ASSERT(idxSrc != idxDst);

							auto* pSrc = (void*)&m_data[idxSrc];
							auto* pDst = (void*)&m_data[idxDst];
							desc.Move(pSrc, pDst);
							desc.Dtor(pSrc);
						}

						// Entity has been replaced with the last one in chunk.
						// Update its index so look ups can find it.
						auto& entityContainer = entities[entity.id()];
						entityContainer.idx = index;
						entityContainer.gen = entity.gen();
					}
				}

				/*!
				Makes the entity a part of a chunk on a given index.
				\param index Index of the entity
				\param entity Entity to store in the chunk
				*/
				void SetEntity(uint32_t index, Entity entity) {
					GAIA_ASSERT(index < m_header.count && "Entity chunk index out of bounds!");

					const auto offset = sizeof(Entity) * index + m_header.offsets.firstByte_EntityData;
					// unaligned_ref not necessary because data is aligned
					auto* pMem = (Entity*)&m_data[offset];
					*pMem = entity;
				}

				/*!
				Returns the entity on a given index in the chunk.
				\param index Index of the entity
				\return Entity on a given index within the chunk.
				*/
				GAIA_NODISCARD Entity GetEntity(uint32_t index) const {
					GAIA_ASSERT(index < m_header.count && "Entity chunk index out of bounds!");

					const auto offset = sizeof(Entity) * index + m_header.offsets.firstByte_EntityData;
					// unaligned_ref not necessary because data is aligned
					auto* pMem = (Entity*)&m_data[offset];
					return *pMem;
				}

				/*!
				Enables or disables the entity on a given index in the chunk.
				\param index Index of the entity
				\param enableEntity Enables or disabled the entity
				*/
				void EnableEntity(uint32_t index, bool enableEntity) {
					GAIA_ASSERT(index < m_header.count && "Entity chunk index out of bounds!");

					if (enableEntity) {
						m_header.disabledEntityMask.set(index, false);
						SetDisabled(m_header.disabledEntityMask.any());
					} else {
						m_header.disabledEntityMask.set(index, true);
						SetDisabled(true);
					}
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
				Returns a pointer to chunk data.
				\param offset Offset into chunk data
				\return Pointer to chunk data.
				*/
				const uint8_t& GetData(uint32_t offset) const {
					return m_data[offset];
				}

				/*!
				Returns an offset to chunk data at which the component is stored.
				\warning It is expected the component with \param componentId is present. Undefined behavior otherwise.
				\param componentType Component type
				\param componentId Component id
				\param componentIdx Index of the component in this chunk's component array
				\return Offset from the start of chunk data area.
				*/
				GAIA_NODISCARD ChunkComponentOffset FindDataOffset(
						component::ComponentType componentType, component::ComponentId componentId, uint32_t& componentIdx) const {
					// Don't use this with empty components. It's impossible to write to them anyway.
					GAIA_ASSERT(ComponentCache::Get().GetComponentDesc(componentId).properties.size != 0);

					componentIdx = GetComponentIdx(componentType, componentId);

					auto compOffs = GetComponentOffsetSpan(componentType);
					const auto offset = compOffs[componentIdx];
					GAIA_ASSERT(offset >= m_header.offsets.firstByte_EntityData);
					return offset;
				}

				/*!
				Returns an offset to chunk data at which the component is stored.
				\warning It is expected the component with \param componentId is present. Undefined behavior otherwise.
				\param componentType Component type
				\param componentId Component id
				\return Offset from the start of chunk data area.
				*/
				GAIA_NODISCARD GAIA_FORCEINLINE ChunkComponentOffset
				FindDataOffset(component::ComponentType componentType, component::ComponentId componentId) const {
					[[maybe_unused]] uint32_t componentIdx = 0;
					return FindDataOffset(componentType, componentId, componentIdx);
				}

				//----------------------------------------------------------------------
				// Component handling
				//----------------------------------------------------------------------

				bool HasAnyCustomGenericConstructor() const {
					return m_header.hasAnyCustomGenericCtor;
				}

				bool HasAnyCustomChunkConstructor() const {
					return m_header.hasAnyCustomChunkCtor;
				}

				bool HasAnyCustomGenericDestructor() const {
					return m_header.hasAnyCustomGenericDtor;
				}

				bool HasAnyCustomChunkDestructor() const {
					return m_header.hasAnyCustomChunkDtor;
				}

				void CallConstructor(
						component::ComponentType componentType, component::ComponentId componentId, uint32_t entityIndex) {
					GAIA_PROF_SCOPE(CallConstructor);

					// Make sure only generic types are used with indices
					GAIA_ASSERT(componentType == component::ComponentType::CT_Generic || entityIndex == 0);

					const auto& cc = ComponentCache::Get();
					const auto& desc = cc.GetComponentDesc(componentId);
					if (desc.ctor == nullptr)
						return;

					const auto idx = GetComponentIdx(componentType, componentId);

					auto compOffs = GetComponentOffsetSpan(componentType);
					const auto offset = compOffs[idx];
					const auto idxSrc = offset + entityIndex * desc.properties.size;
					GAIA_ASSERT(idxSrc < GetByteSize());

					auto* pSrc = (void*)&m_data[idxSrc];
					desc.ctor(pSrc, 1);
				}

				void CallConstructors(component::ComponentType componentType, uint32_t entityIndex, uint32_t entityCount) {
					GAIA_PROF_SCOPE(CallConstructors);

					GAIA_ASSERT(
							componentType == component::ComponentType::CT_Generic && HasAnyCustomGenericConstructor() ||
							componentType == component::ComponentType::CT_Chunk && HasAnyCustomChunkConstructor());

					// Make sure only generic types are used with indices
					GAIA_ASSERT(componentType == component::ComponentType::CT_Generic || (entityIndex == 0 && entityCount == 1));

					const auto& cc = ComponentCache::Get();
					auto compIds = GetComponentIdSpan(componentType);
					auto compOffs = GetComponentOffsetSpan(componentType);

					for (uint32_t i = 0; i < compIds.size(); i++) {
						const auto& desc = cc.GetComponentDesc(compIds[i]);
						if (desc.ctor == nullptr)
							continue;

						const auto offset = compOffs[i];
						const auto idxSrc = offset + entityIndex * desc.properties.size;
						GAIA_ASSERT(idxSrc < GetByteSize());

						auto* pSrc = (void*)&m_data[idxSrc];
						desc.ctor(pSrc, entityCount);
					}
				}

				void CallDestructors(component::ComponentType componentType, uint32_t entityIndex, uint32_t entityCount) {
					GAIA_PROF_SCOPE(CallDestructors);

					GAIA_ASSERT(
							componentType == component::ComponentType::CT_Generic && HasAnyCustomGenericDestructor() ||
							componentType == component::ComponentType::CT_Chunk && HasAnyCustomChunkDestructor());

					// Make sure only generic types are used with indices
					GAIA_ASSERT(componentType == component::ComponentType::CT_Generic || (entityIndex == 0 && entityCount == 1));

					const auto& cc = ComponentCache::Get();
					auto compIds = GetComponentIdSpan(componentType);
					auto compOffs = GetComponentOffsetSpan(componentType);

					for (uint32_t i = 0; i < compIds.size(); ++i) {
						const auto& desc = cc.GetComponentDesc(compIds[i]);
						if (desc.dtor == nullptr)
							continue;

						const auto offset = compOffs[i];
						const auto idxSrc = offset + entityIndex * desc.properties.size;
						GAIA_ASSERT(idxSrc < GetByteSize());

						auto* pSrc = (void*)&m_data[idxSrc];
						desc.dtor(pSrc, entityCount);
					}
				};

				//----------------------------------------------------------------------
				// Check component presence
				//----------------------------------------------------------------------

				/*!
				Checks if a component with \param componentId and type \param componentType is present in the chunk.
				\param componentId Component id
				\param componentType Component type
				\return True if found. False otherwise.
				*/
				GAIA_NODISCARD bool Has(component::ComponentType componentType, component::ComponentId componentId) const {
					auto compIds = GetComponentIdSpan(componentType);
					return core::has(compIds, componentId);
				}

				/*!
				Checks if component \tparam T is present in the chunk.
				\tparam T Component
				\return True if the component is present. False otherwise.
				*/
				template <typename T>
				GAIA_NODISCARD bool Has() const {
					const auto componentId = component::GetComponentId<T>();

					constexpr auto componentType = component::component_type_v<T>;
					return Has(componentType, componentId);
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
				template <typename T, typename U = typename component::component_type_t<T>::Type>
				U& Set(uint32_t index) {
					static_assert(
							component::component_type_v<T> == component::ComponentType::CT_Generic,
							"Set providing an index can only be used with generic components");

					// Update the world version
					UpdateVersion(m_header.worldVersion);

					GAIA_ASSERT(index < m_header.capacity);
					return ViewRW<T>()[index];
				}

				/*!
				Sets the value of the chunk component \tparam T on \param index in the chunk.
				\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
				\tparam T Component
				\param index Index of entity in the chunk
				\param value Value to set for the component
				*/
				template <typename T, typename U = typename component::component_type_t<T>::Type>
				U& Set() {
					// Update the world version
					UpdateVersion(m_header.worldVersion);

					GAIA_ASSERT(0 < m_header.capacity);
					return ViewRW<T>()[0];
				}

				/*!
				Sets the value of the chunk component \tparam T on \param index in the chunk.
				\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
				\tparam T Component
				\param index Index of entity in the chunk
				\param value Value to set for the component
				*/
				template <typename T, typename U = typename component::component_type_t<T>::Type>
				void Set(uint32_t index, U&& value) {
					static_assert(
							component::component_type_v<T> == component::ComponentType::CT_Generic,
							"Set providing an index can only be used with generic components");

					// Update the world version
					UpdateVersion(m_header.worldVersion);

					GAIA_ASSERT(index < m_header.capacity);
					ViewRW<T>()[index] = std::forward<U>(value);
				}

				/*!
				Sets the value of the chunk component \tparam T in the chunk.
				\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
				\tparam T Component
				\param value Value to set for the component
				*/
				template <typename T, typename U = typename component::component_type_t<T>::Type>
				void Set(U&& value) {
					static_assert(
							component::component_type_v<T> != component::ComponentType::CT_Generic,
							"Set not providing an index can only be used with non-generic components");

					// Update the world version
					UpdateVersion(m_header.worldVersion);

					GAIA_ASSERT(0 < m_header.capacity);
					ViewRW<T>()[0] = std::forward<U>(value);
				}

				/*!
				Sets the value of the chunk component \tparam T on \param index in the chunk.
				\warning World version is not updated so Query filters will not be able to catch this change.
				\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
				\tparam T Component
				\param index Index of entity in the chunk
				\param value Value to set for the component
				*/
				template <typename T, typename U = typename component::component_type_t<T>::Type>
				void SetSilent(uint32_t index, U&& value) {
					static_assert(
							component::component_type_v<T> == component::ComponentType::CT_Generic,
							"SetSilent providing an index can only be used with generic components");

					GAIA_ASSERT(index < m_header.capacity);
					ViewRWSilent<T>()[index] = std::forward<U>(value);
				}

				/*!
				Sets the value of the chunk component \tparam T in the chunk.
				\warning World version is not updated so Query filters will not be able to catch this change.
				\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
				\tparam T Component
				\param value Value to set for the component
				*/
				template <typename T, typename U = typename component::component_type_t<T>::Type>
				void SetSilent(U&& value) {
					static_assert(
							component::component_type_v<T> != component::ComponentType::CT_Generic,
							"SetSilent not providing an index can only be used with non-generic components");

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
				GAIA_NODISCARD auto Get(uint32_t index) const {
					static_assert(
							component::component_type_v<T> == component::ComponentType::CT_Generic,
							"Get providing an index can only be used with generic components");

					return GetComponent_Internal<T>(index);
				}

				/*!
				Returns the value stored in the chunk component \tparam T.
				\warning It is expected the chunk component \tparam T is present. Undefined behavior otherwise.
				\tparam T Component
				\return Value stored in the component.
				*/
				template <typename T>
				GAIA_NODISCARD auto Get() const {
					static_assert(
							component::component_type_v<T> != component::ComponentType::CT_Generic,
							"Get not providing an index can only be used with non-generic components");

					return GetComponent_Internal<T>(0);
				}

				/*!
				Returns the internal index of a component based on the provided \param componentId.
				\param componentType Component type
				\return Component index if the component was found. -1 otherwise.
				*/
				GAIA_NODISCARD uint32_t
				GetComponentIdx(component::ComponentType componentType, component::ComponentId componentId) const {
					auto compIds = GetComponentIdSpan(componentType);
					const auto idx = core::get_index_unsafe(compIds, componentId);
					GAIA_ASSERT(idx != BadIndex);
					return idx;
				}

				//----------------------------------------------------------------------
				// Iteration
				//----------------------------------------------------------------------

				template <typename T>
				GAIA_NODISCARD constexpr GAIA_FORCEINLINE auto GetComponentView() {
					using U = typename component::component_type_t<T>::Type;
					using UOriginal = typename component::component_type_t<T>::TypeOriginal;
					if constexpr (component::is_component_mut_v<UOriginal>) {
						auto s = ViewRW_Internal<U, true>();
						return std::span{(U*)s.data(), s.size()};
					} else {
						auto s = View_Internal<U>();
						return std::span{(const U*)s.data(), s.size()};
					}
				}

				template <typename... T, typename Func>
				GAIA_FORCEINLINE void ForEach([[maybe_unused]] core::func_type_list<T...> types, Func func) {
					const uint32_t size = GetEntityCount();
					GAIA_ASSERT(size > 0);

					if constexpr (sizeof...(T) > 0) {
						// Pointers to the respective component types in the chunk, e.g
						// 		q.ForEach([&](Position& p, const Velocity& v) {...}
						// Translates to:
						//  	auto p = iter.ViewRW_Internal<Position, true>();
						//		auto v = iter.View_Internal<Velocity>();
						auto dataPointerTuple = std::make_tuple(GetComponentView<T>()...);

						// Iterate over each entity in the chunk.
						// Translates to:
						//		for (uint32_t i: iter)
						//			func(p[i], v[i]);

						for (uint32_t i = 0; i < size; ++i)
							func(std::get<decltype(GetComponentView<T>())>(dataPointerTuple)[i]...);
					} else {
						// No functor parameters. Do an empty loop.
						for (uint32_t i = 0; i < size; ++i)
							func();
					}
				}

				//----------------------------------------------------------------------

				GAIA_NODISCARD ArchetypeId GetArchetypeId() const {
					return m_header.archetypeId;
				}

				void SetChunkIndex(uint16_t value) {
					m_header.index = value;
				}

				GAIA_NODISCARD uint16_t GetChunkIndex() const {
					return m_header.index;
				}

				void SetDisabled(bool value) {
					m_header.hasDisabledEntities = value;
				}

				//! Checks is this chunk is disabled
				GAIA_NODISCARD bool HasDisabledEntities() const {
					return m_header.hasDisabledEntities != 0;
				}

				//! Checks is this chunk is dying
				GAIA_NODISCARD bool IsDying() const {
					return m_header.lifespanCountdown > 0;
				}

				void PrepareToDie() {
					m_header.lifespanCountdown = ChunkHeader::MAX_CHUNK_LIFESPAN;
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

				//! Checks is the full capacity of the has has been reached
				GAIA_NODISCARD bool IsFull() const {
					return m_header.count >= m_header.capacity;
				}

				//! Checks is the chunk is semi-full.
				GAIA_NODISCARD bool IsSemiFull() const {
					constexpr float Threshold = 0.7f;
					return ((float)m_header.count / (float)m_header.capacity) < Threshold;
				}

				//! Checks is there are any entities in the chunk
				GAIA_NODISCARD bool HasEntities() const {
					return m_header.count > 0;
				}

				//! Returns the number of entities in the chunk
				GAIA_NODISCARD uint32_t GetEntityCount() const {
					return m_header.count;
				}

				//! Returns the number of entities in the chunk
				GAIA_NODISCARD uint32_t GetEntityCapacity() const {
					return m_header.capacity;
				}

				GAIA_NODISCARD uint32_t GetByteSize() const {
					return detail::ChunkAllocatorImpl::GetMemoryBlockSize(m_header.sizeType);
				}

				//! Returns the mask of disabled entities
				GAIA_NODISCARD ChunkHeader::DisabledEntityMask& GetDisabledEntityMask() {
					return m_header.disabledEntityMask;
				}

				//! Returns the mask of disabled entities
				GAIA_NODISCARD const ChunkHeader::DisabledEntityMask& GetDisabledEntityMask() const {
					return m_header.disabledEntityMask;
				}

				GAIA_NODISCARD std::span<uint32_t> GetComponentVersionSpan(component::ComponentType componentType) const {
					const auto offset = m_header.offsets.firstByte_Versions[componentType];
					return {(uint32_t*)(&m_data[offset]), m_header.componentCount[componentType]};
				}

				GAIA_NODISCARD std::span<const component::ComponentId>
				GetComponentIdSpan(component::ComponentType componentType) const {
					using RetType = std::add_pointer_t<const component::ComponentId>;
					const auto offset = m_header.offsets.firstByte_ComponentIds[componentType];
					return {(RetType)&m_data[offset], m_header.componentCount[componentType]};
				}

				GAIA_NODISCARD std::span<const ChunkComponentOffset>
				GetComponentOffsetSpan(component::ComponentType componentType) const {
					using RetType = std::add_pointer_t<const ChunkComponentOffset>;
					const auto offset = m_header.offsets.firstByte_ComponentOffsets[componentType];
					return {(RetType)&m_data[offset], m_header.componentCount[componentType]};
				}

				//! Returns true if the provided version is newer than the one stored internally
				GAIA_NODISCARD bool
				DidChange(component::ComponentType componentType, uint32_t version, uint32_t componentIdx) const {
					auto versions = GetComponentVersionSpan(componentType);
					return DidVersionChange(versions[componentIdx], version);
				}

				//! Update version of a component at the index \param componentIdx of a given \param componentType
				GAIA_FORCEINLINE void UpdateWorldVersion(component::ComponentType componentType, uint32_t componentIdx) const {
					// Make sure only proper input is provided
					GAIA_ASSERT(componentIdx >= 0 && componentIdx < archetype::MAX_COMPONENTS_PER_ARCHETYPE);

					auto versions = GetComponentVersionSpan(componentType);
					versions[componentIdx] = m_header.worldVersion;
				}

				//! Update version of all components of a given \param componentType
				GAIA_FORCEINLINE void UpdateWorldVersion(component::ComponentType componentType) const {
					auto versions = GetComponentVersionSpan(componentType);
					for (auto& v: versions)
						v = m_header.worldVersion;
				}

				void Diag(uint32_t index) const {
					GAIA_LOG_N(
							"  Chunk #%04u, entities:%u/%u, lifespanCountdown:%u", index, m_header.count, m_header.capacity,
							m_header.lifespanCountdown);
				}
			};
		} // namespace archetype
	} // namespace ecs
} // namespace gaia
