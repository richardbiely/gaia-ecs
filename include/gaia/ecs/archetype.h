#pragma once
#include "../config/config.h"

#include <cinttypes>

#include "../containers/darray.h"
#include "../containers/dbitset.h"
#include "../containers/sarray.h"
#include "../containers/sarray_ext.h"
#include "../utils/hashing_policy.h"
#include "archetype_common.h"
#include "archetype_graph.h"
#include "chunk.h"
#include "chunk_allocator.h"
#include "chunk_header.h"
#include "component.h"
#include "component_cache.h"
#include "component_utils.h"
#include "entity.h"

namespace gaia {
	namespace ecs {
		namespace archetype {
			class Archetype;

			class ArchetypeBase {
			protected:
				//! Archetype ID - used to address the archetype directly in the world's list or archetypes
				ArchetypeId m_archetypeId = ArchetypeIdBad;

			public:
				GAIA_NODISCARD ArchetypeId GetArchetypeId() const {
					return m_archetypeId;
				}
			};

			class ArchetypeLookupChecker: public ArchetypeBase {
				friend class Archetype;

				//! List of component indices
				component::ComponentIdSpan m_componentIds[component::ComponentType::CT_Count];

			public:
				ArchetypeLookupChecker(
						component::ComponentIdSpan componentIdsGeneric, component::ComponentIdSpan componentIdsChunk) {
					m_componentIds[component::ComponentType::CT_Generic] = componentIdsGeneric;
					m_componentIds[component::ComponentType::CT_Chunk] = componentIdsChunk;
				}

				bool CompareComponentIds(const ArchetypeLookupChecker& other) const {
					// Size has to match
					if (m_componentIds[component::ComponentType::CT_Generic].size() !=
							other.m_componentIds[component::ComponentType::CT_Generic].size())
						return false;
					if (m_componentIds[component::ComponentType::CT_Chunk].size() !=
							other.m_componentIds[component::ComponentType::CT_Chunk].size())
						return false;

					// Elements have to match
					for (size_t i = 0; i < m_componentIds[component::ComponentType::CT_Generic].size(); ++i) {
						if (m_componentIds[component::ComponentType::CT_Generic][i] !=
								other.m_componentIds[component::ComponentType::CT_Generic][i])
							return false;
					}
					for (size_t i = 0; i < m_componentIds[component::ComponentType::CT_Chunk].size(); ++i) {
						if (m_componentIds[component::ComponentType::CT_Chunk][i] !=
								other.m_componentIds[component::ComponentType::CT_Chunk][i])
							return false;
					}

					return true;
				}
			};

			class Archetype final: public ArchetypeBase {
			public:
				using LookupHash = utils::direct_hash_key<uint64_t>;
				using GenericComponentHash = utils::direct_hash_key<uint64_t>;
				using ChunkComponentHash = utils::direct_hash_key<uint64_t>;

			private:
				struct {
					//! The number of entities this archetype can take (e.g 5 = 5 entities with all their components)
					uint32_t capacity: ChunkHeader::MAX_CHUNK_ENTITES_BITS;
					//! How many bytes of data is needed for a fully utilized chunk
					uint32_t chunkDataBytes : 16;
				} m_properties{};
				static_assert(sizeof(m_properties) <= sizeof(uint32_t));
				//! Stable reference to parent world's world version
				uint32_t& m_worldVersion;

				//! List of chunks allocated by this archetype
				containers::darray<Chunk*> m_chunks;
				//! Mask of chunks with disabled entities
				// containers::dbitset m_disabledMask;
				//! Graph of archetypes linked with this one
				ArchetypeGraph m_graph;

				//! Offsets to various parts of data inside chunk
				ChunkHeaderOffsets m_dataOffsets;
				//! List of component indices
				containers::sarray<ComponentIdArray, component::ComponentType::CT_Count> m_componentIds;
				//! List of components offset indices
				containers::sarray<ComponentOffsetArray, component::ComponentType::CT_Count> m_componentOffsets;

				//! Hash of generic components
				GenericComponentHash m_genericHash = {0};
				//! Hash of chunk components
				ChunkComponentHash m_chunkHash = {0};
				//! Hash of components within this archetype - used for lookups
				LookupHash m_lookupHash = {0};
				//! Hash of components within this archetype - used for matching
				component::ComponentMatcherHash m_matcherHash[component::ComponentType::CT_Count]{};

				// Constructor is hidden. Create archetypes via Create
				Archetype(uint32_t& worldVersion): m_worldVersion(worldVersion) {}

				void UpdateDataOffsets(uintptr_t memoryAddress) {
					uint32_t offset = 0;

					// Versions
					// We expect versions to fit in the first 256 bytes.
					// With 64 components per archetype (32 generic + 32 chunk) this gives us some headroom.
					{
						const auto padding = utils::padding<alignof(uint32_t)>(memoryAddress);
						offset += padding;

						if (!m_componentIds[component::ComponentType::CT_Generic].empty()) {
							GAIA_ASSERT(offset < 256);
							m_dataOffsets.firstByte_Versions[component::ComponentType::CT_Generic] = (ChunkVersionOffset)offset;
							offset += sizeof(uint32_t) * m_componentIds[component::ComponentType::CT_Generic].size();
						}
						if (!m_componentIds[component::ComponentType::CT_Chunk].empty()) {
							GAIA_ASSERT(offset < 256);
							m_dataOffsets.firstByte_Versions[component::ComponentType::CT_Chunk] = (ChunkVersionOffset)offset;
							offset += sizeof(uint32_t) * m_componentIds[component::ComponentType::CT_Chunk].size();
						}
					}

					// Component ids
					{
						const auto padding = utils::padding<alignof(component::ComponentId)>(offset);
						offset += padding;

						if (!m_componentIds[component::ComponentType::CT_Generic].empty()) {
							m_dataOffsets.firstByte_ComponentIds[component::ComponentType::CT_Generic] = (ChunkComponentOffset)offset;
							offset += sizeof(component::ComponentId) * m_componentIds[component::ComponentType::CT_Generic].size();
						}
						if (!m_componentIds[component::ComponentType::CT_Chunk].empty()) {
							m_dataOffsets.firstByte_ComponentIds[component::ComponentType::CT_Chunk] = (ChunkComponentOffset)offset;
							offset += sizeof(component::ComponentId) * m_componentIds[component::ComponentType::CT_Chunk].size();
						}
					}

					// Component offsets
					{
						const auto padding = utils::padding<alignof(ChunkComponentOffset)>(offset);
						offset += padding;

						if (!m_componentIds[component::ComponentType::CT_Generic].empty()) {
							m_dataOffsets.firstByte_ComponentOffsets[component::ComponentType::CT_Generic] =
									(ChunkComponentOffset)offset;
							offset += sizeof(ChunkComponentOffset) * m_componentIds[component::ComponentType::CT_Generic].size();
						}
						if (!m_componentIds[component::ComponentType::CT_Chunk].empty()) {
							m_dataOffsets.firstByte_ComponentOffsets[component::ComponentType::CT_Chunk] =
									(ChunkComponentOffset)offset;
							offset += sizeof(ChunkComponentOffset) * m_componentIds[component::ComponentType::CT_Chunk].size();
						}
					}

					// First entity offset
					{
						const auto padding = utils::padding<alignof(Entity)>(offset);
						offset += padding;
						m_dataOffsets.firstByte_EntityData = (ChunkComponentOffset)offset;
					}
				}

#if GAIA_AVOID_CHUNK_FRAGMENTATION
				static void VerifyChunksFragmentation_Internal(std::span<Chunk*> chunkArray) {
					if (chunkArray.size() <= 1)
						return;

					uint32_t i = 1;

					if (chunkArray[0]->IsFull()) {
						// Make sure all chunks before the first non-full one are full
						for (; i < chunkArray.size(); ++i) {
							auto* pChunk = chunkArray[i];
							if (pChunk->IsFull())
								GAIA_ASSERT(chunkArray[i - 1]->IsFull());
							else
								break;
						}
					}

					// Make sure all chunks after the full non-full are empty
					++i;
					for (; i < chunkArray.size(); ++i) {
						GAIA_ASSERT(!chunkArray[i]->HasEntities());
					}
				}

				//! Returns the first non-full chunk or nullptr if there is no such chunk.
				GAIA_NODISCARD static Chunk* FindFirstNonFullChunk_Internal(std::span<Chunk*> chunkArray) {
					if GAIA_UNLIKELY (chunkArray.empty())
						return nullptr;

					Chunk* pFirstNonFullChunk = nullptr;
					uint32_t i = (uint32_t)chunkArray.size() - 1;
					do {
						auto* pChunk = chunkArray[i];
						if (pChunk->IsFull())
							break;

						pFirstNonFullChunk = pChunk;
					} while (i-- > 0);

					return pFirstNonFullChunk;
				}

				//! Returns the first non-empty chunk or nullptr if there are no chunks.
				GAIA_NODISCARD static Chunk* FindFirstNonEmptyChunk_Internal(std::span<Chunk*> chunkArray) {
					if GAIA_UNLIKELY (chunkArray.empty())
						return nullptr;
					if (chunkArray.size() == 1)
						return chunkArray[0];

					Chunk* pFirstNonEmptyChunk = nullptr;
					uint32_t i = (uint32_t)chunkArray.size() - 1;
					do {
						auto* pChunk = chunkArray[i];
						if (pChunk->IsFull()) {
							if (pFirstNonEmptyChunk == nullptr)
								return pChunk;
							break;
						}

						if (pChunk->HasEntities())
							pFirstNonEmptyChunk = pChunk;
					} while (i-- > 0);

					return pFirstNonEmptyChunk;
				}
#endif

				GAIA_NODISCARD Chunk* FindOrCreateFreeChunk_Internal(containers::darray<Chunk*>& chunkArray) const {
					const auto chunkCnt = chunkArray.size();

					if (chunkCnt > 0) {
#if GAIA_AVOID_CHUNK_FRAGMENTATION
						// In order to avoid memory fragmentation we always take from the back.
						// This means all previous chunks are always going to be fully utilized
						// and it is safe for as to peek at the last one to make descisions.
						auto* pChunk = FindFirstNonFullChunk_Internal(chunkArray);
						if (pChunk != nullptr)
							return pChunk;
#else
						// Find first semi-empty chunk.
						// Picking the first non-full would only support fragmentation.
						Chunk* pEmptyChunk = nullptr;
						for (auto* pChunk: chunkArray) {
							GAIA_ASSERT(pChunk != nullptr);
							const auto entityCnt = pChunk->GetEntityCount();
							if GAIA_UNLIKELY (entityCnt == 0)
								pEmptyChunk = pChunk;
							else if (entityCnt < pChunk->GetEntityCapacity())
								return pChunk;
						}
						if (pEmptyChunk != nullptr)
							return pEmptyChunk;
#endif
					}

					// Make sure not too many chunks are allocated
					GAIA_ASSERT(chunkCnt < UINT16_MAX);

					// No free space found anywhere. Let's create a new chunk.
					auto* pChunk = Chunk::Create(
							m_archetypeId, (uint16_t)chunkCnt, m_properties.capacity, m_properties.chunkDataBytes, m_worldVersion,
							m_dataOffsets, m_componentIds, m_componentOffsets);

					chunkArray.push_back(pChunk);
					return pChunk;
				}

				/*!
				Checks if a component with \param componentId and type \param componentType is present in the archetype.
				\param componentId Component id
				\param componentType Component type
				\return True if found. False otherwise.
				*/
				GAIA_NODISCARD bool
				HasComponent_Internal(component::ComponentType componentType, component::ComponentId componentId) const {
					const auto& componentIds = GetComponentIdArray(componentType);
					return utils::has(componentIds, componentId);
				}

				/*!
				Checks if a component of type \tparam T is present in the archetype.
				\return True if found. False otherwise.
				*/
				template <typename T>
				GAIA_NODISCARD bool HasComponent_Internal() const {
					const auto componentId = component::GetComponentId<T>();

					constexpr auto componentType = component::component_type_v<T>;
					return HasComponent_Internal(componentType, componentId);
				}

				static bool EstimateMaxEntitiesInAchetype(
						uint32_t& dataOffset, uint32_t& maxItems, component::ComponentIdSpan componentIds, uint32_t size,
						uint32_t maxDataOffset) {
					const auto& cc = ComponentCache::Get();

					for (const auto componentId: componentIds) {
						const auto& desc = cc.GetComponentDesc(componentId);
						const auto alignment = desc.properties.alig;
						if (alignment == 0)
							continue;

						// If we're beyond what the chunk could take, subtract one entity
						const auto nextOffset = desc.CalculateNewMemoryOffset(dataOffset, size);
						if (nextOffset >= maxDataOffset) {
							const auto subtractItems = (nextOffset - maxDataOffset + desc.properties.size) / desc.properties.size;
							GAIA_ASSERT(subtractItems > 0);
							GAIA_ASSERT(maxItems > subtractItems);
							maxItems -= subtractItems;
							return false;
						}

						dataOffset = nextOffset;
					}

					return true;
				};

			public:
				/*!
				Checks if the archetype id is valid.
				\return True if the id is valid, false otherwise.
				*/
				static bool IsIdValid(ArchetypeId id) {
					return id != ArchetypeIdBad;
				}

				Archetype(Archetype&& world) = delete;
				Archetype(const Archetype& world) = delete;
				Archetype& operator=(Archetype&&) = delete;
				Archetype& operator=(const Archetype&) = delete;

				~Archetype() {
					// Delete all archetype chunks
					for (auto* pChunk: m_chunks)
						Chunk::Release(pChunk);
				}

				bool CompareComponentIds(const ArchetypeLookupChecker& other) const {
					// Size has to match
					if (m_componentIds[component::ComponentType::CT_Generic].size() !=
							other.m_componentIds[component::ComponentType::CT_Generic].size())
						return false;
					if (m_componentIds[component::ComponentType::CT_Chunk].size() !=
							other.m_componentIds[component::ComponentType::CT_Chunk].size())
						return false;

					// Elements have to match
					for (uint32_t i = 0; i < m_componentIds[component::ComponentType::CT_Generic].size(); ++i) {
						if (m_componentIds[component::ComponentType::CT_Generic][i] !=
								other.m_componentIds[component::ComponentType::CT_Generic][i])
							return false;
					}
					for (uint32_t i = 0; i < m_componentIds[component::ComponentType::CT_Chunk].size(); ++i) {
						if (m_componentIds[component::ComponentType::CT_Chunk][i] !=
								other.m_componentIds[component::ComponentType::CT_Chunk][i])
							return false;
					}

					return true;
				}

				GAIA_NODISCARD static LookupHash
				CalculateLookupHash(GenericComponentHash genericHash, ChunkComponentHash chunkHash) noexcept {
					return {utils::hash_combine(genericHash.hash, chunkHash.hash)};
				}

				GAIA_NODISCARD static Archetype* Create(
						ArchetypeId archetypeId, uint32_t& worldVersion, component::ComponentIdSpan componentIdsGeneric,
						component::ComponentIdSpan componentIdsChunk) {
					auto* newArch = new Archetype(worldVersion);
					newArch->m_archetypeId = archetypeId;

					newArch->m_componentIds[component::ComponentType::CT_Generic].resize((uint32_t)componentIdsGeneric.size());
					newArch->m_componentIds[component::ComponentType::CT_Chunk].resize((uint32_t)componentIdsChunk.size());
					newArch->m_componentOffsets[component::ComponentType::CT_Generic].resize(
							(uint32_t)componentIdsGeneric.size());
					newArch->m_componentOffsets[component::ComponentType::CT_Chunk].resize((uint32_t)componentIdsChunk.size());
					newArch->UpdateDataOffsets(sizeof(ChunkHeader) + MemoryBlockUsableOffset);

					const auto& cc = ComponentCache::Get();
					const auto& dataOffset = newArch->m_dataOffsets;

					// Calculate the number of entities per chunks precisely so we can
					// fit as many of them into chunk as possible.

					// Total size of generic components
					uint32_t genericComponentListSize = 0;
					for (const auto componentId: componentIdsGeneric) {
						const auto& desc = cc.GetComponentDesc(componentId);
						genericComponentListSize += desc.properties.size;
					}

					// Total size of chunk components
					uint32_t chunkComponentListSize = 0;
					for (const auto componentId: componentIdsChunk) {
						const auto& desc = cc.GetComponentDesc(componentId);
						chunkComponentListSize += desc.properties.size;
					}

					const uint32_t size0 = Chunk::GetChunkDataSize(detail::ChunkAllocatorImpl::GetMemoryBlockSize(0));
					const uint32_t size1 = Chunk::GetChunkDataSize(detail::ChunkAllocatorImpl::GetMemoryBlockSize(1));
					const auto sizeM = (size0 + size1) / 2;

					uint32_t maxDataOffsetTarget = size1;
					// Theoretical maximum number of components we can fit into one chunk.
					// This can be further reduced due alignment and padding.
					auto maxGenericItemsInArchetype =
							(maxDataOffsetTarget - dataOffset.firstByte_EntityData - chunkComponentListSize - 1) /
							(genericComponentListSize + (uint32_t)sizeof(Entity));

					bool finalCheck = false;
				recalculate:
					auto currentOffset = dataOffset.firstByte_EntityData + (uint32_t)sizeof(Entity) * maxGenericItemsInArchetype;

					// Adjust the maximum number of entities. Recalculation happens at most once when the original guess
					// for entity count is not right (most likely because of padding or usage of SoA components).
					if (!EstimateMaxEntitiesInAchetype(
									currentOffset, maxGenericItemsInArchetype, componentIdsGeneric, maxGenericItemsInArchetype,
									maxDataOffsetTarget))
						goto recalculate;
					if (!EstimateMaxEntitiesInAchetype(
									currentOffset, maxGenericItemsInArchetype, componentIdsChunk, 1, maxDataOffsetTarget))
						goto recalculate;

					// TODO: Make it possible for chunks to be not restricted by ChunkHeader::DisabledEntityMask::BitCount.
					// TODO: Consider having chunks of different sizes as this would minimize the memory footprint.
					if (maxGenericItemsInArchetype > ChunkHeader::MAX_CHUNK_ENTITES) {
						maxGenericItemsInArchetype = ChunkHeader::MAX_CHUNK_ENTITES;
						goto recalculate;
					}

					// We create chunks of either 8K or 16K but might end up with requested capacity 8.1K. Allocating a 16K chunk
					// in this case would be wasteful. Therefore, let's find the middle ground. Anything 12K or smaller we'll
					// allocate into 8K chunks so we avoid wasting too much memory.
					if (!finalCheck && currentOffset < sizeM) {
						finalCheck = true;
						maxDataOffsetTarget = size0;

						maxGenericItemsInArchetype =
								(maxDataOffsetTarget - dataOffset.firstByte_EntityData - chunkComponentListSize - 1) /
								(genericComponentListSize + (uint32_t)sizeof(Entity));
						goto recalculate;
					}

					// Update the offsets according to the recalculated maxGenericItemsInArchetype
					currentOffset = dataOffset.firstByte_EntityData + (uint32_t)sizeof(Entity) * maxGenericItemsInArchetype;

					auto registerComponents = [&](component::ComponentIdSpan componentIds, component::ComponentType componentType,
																				const uint32_t count) {
						auto& ids = newArch->m_componentIds[componentType];
						auto& ofs = newArch->m_componentOffsets[componentType];

						for (uint32_t i = 0; i < componentIds.size(); ++i) {
							const auto componentId = componentIds[i];
							const auto& desc = cc.GetComponentDesc(componentId);
							const auto alignment = desc.properties.alig;
							if (alignment == 0) {
								GAIA_ASSERT(desc.properties.size == 0);

								// Register the component info
								ids[i] = componentId;
								ofs[i] = {};
							} else {
								const auto padding = utils::padding(currentOffset, alignment);
								currentOffset += padding;

								// Register the component info
								ids[i] = componentId;
								ofs[i] = (ChunkComponentOffset)currentOffset;

								// Make sure the following component list is properly aligned
								currentOffset += desc.properties.size * count;
							}
						}
					};
					registerComponents(componentIdsGeneric, component::ComponentType::CT_Generic, maxGenericItemsInArchetype);
					registerComponents(componentIdsChunk, component::ComponentType::CT_Chunk, 1);

					newArch->m_properties.capacity = (uint32_t)maxGenericItemsInArchetype;
					newArch->m_properties.chunkDataBytes = (uint16_t)currentOffset;
					GAIA_ASSERT(
							Chunk::GetTotalChunkSize((uint16_t)currentOffset) <
							detail::ChunkAllocatorImpl::GetMemoryBlockSize(currentOffset));

					newArch->m_matcherHash[component::ComponentType::CT_Generic] =
							component::CalculateMatcherHash(componentIdsGeneric);
					newArch->m_matcherHash[component::ComponentType::CT_Chunk] =
							component::CalculateMatcherHash(componentIdsChunk);

					return newArch;
				}

				/*!
				Sets hashes for each component type and lookup.
				\param hashGeneric Generic components hash
				\param hashChunk Chunk components hash
				\param hashLookup Hash used for archetype lookup purposes
				*/
				void SetHashes(GenericComponentHash hashGeneric, ChunkComponentHash hashChunk, LookupHash hashLookup) {
					m_genericHash = hashGeneric;
					m_chunkHash = hashChunk;
					m_lookupHash = hashLookup;
				}

				/*!
				Enables or disables the entity on a given index in the chunk.
				\param pChunk Chunk the entity belongs to
				\param index Index of the entity
				\param enableEntity Enables the entity
				*/
				void EnableEntity(Chunk* pChunk, uint32_t entityIdx, bool enableEntity) {
					pChunk->EnableEntity(entityIdx, enableEntity);
					// m_disabledMask.set(pChunk->GetChunkIndex(), enableEntity ? true : pChunk->HasDisabledEntities());
				}

				/*!
				Removes a chunk from the list of chunks managed by their achetype.
				\param pChunk Chunk to remove from the list of managed archetypes
				*/
				void RemoveChunk(Chunk* pChunk) {
					const auto chunkIndex = pChunk->GetChunkIndex();

					Chunk::Release(pChunk);

					auto remove = [&](auto& chunkArray) {
						if (chunkArray.size() > 1)
							chunkArray.back()->SetChunkIndex(chunkIndex);
						GAIA_ASSERT(chunkIndex == utils::get_index(chunkArray, pChunk));
						utils::erase_fast(chunkArray, chunkIndex);
					};

					remove(m_chunks);
				}

#if GAIA_AVOID_CHUNK_FRAGMENTATION
				void VerifyChunksFramentation() const {
					VerifyChunksFragmentation_Internal(m_chunks);
				}

				//! Returns the first non-empty chunk or nullptr if none is found.
				GAIA_NODISCARD Chunk* FindFirstNonEmptyChunk() const {
					auto* pChunk = FindFirstNonEmptyChunk_Internal(m_chunks);
					GAIA_ASSERT(pChunk == nullptr || !pChunk->HasDisabledEntities());
					return pChunk;
				}
#else
				//! Defragments the chunk.
				//! \param maxEntites Maximum number of entities moved per call
				//! \param chunksToRemove Container of chunks ready for removal
				//! \param entities Container with entities
				void Defragment(
						uint32_t& maxEntities, containers::darray<Chunk*>& chunksToRemove, std::span<EntityContainer> entities) {
					// Assuming the following chunk layout:
					//   Chunk_1: 10/10
					//   Chunk_2:  1/10
					//   Chunk_3:  7/10
					//   Chunk_4: 10/10
					//   Chunk_5:  9/10
					// After full defragmentation we end up with:
					//   Chunk_1: 10/10
					//   Chunk_2: 10/10 (7 entities from Chunk_3 + 2 entities from Chunk_5)
					//   Chunk_3:  0/10 (empty, ready for removal)
					//   Chunk_4: 10/10
					//   Chunk_5:  7/10
					// TODO:
					// Implement mask of semi-full chunks so we can pick one easily when searching
					// for a chunk to fill with a new entity and when defragmenting.
					// NOTE:
					// Even though entity movement might be present during defragmentation, we do
					// not update the world version here because no real structural changes happen.
					// All entites and components remain intact, they just move to a different place.

					if (m_chunks.empty())
						return;

					uint32_t front = 0;
					uint32_t back = m_chunks.size() - 1;

					// Find the first semi-empty chunk in the front
					while (front < back && !m_chunks[front++]->IsSemiFull())
						;

					auto* pDstChunk = m_chunks[front];
					uint32_t firstFreeIdxInDstChunk = pDstChunk->GetEntityCount();

					// Find the first semi-empty chunk in the back
					while (front < back && m_chunks[--back]->IsSemiFull()) {
						auto* pSrcChunk = m_chunks[back];

						const uint32_t entitiesInChunk = pSrcChunk->GetEntityCount();
						const uint32_t entitiesToMove = entitiesInChunk > maxEntities ? maxEntities : entitiesInChunk;
						for (uint32_t i = 0; i < entitiesToMove; ++i) {
							const auto lastEntityIdx = entitiesInChunk - i - 1;
							auto entity = pSrcChunk->GetEntity(lastEntityIdx);
							pDstChunk->SetEntity(firstFreeIdxInDstChunk, entity);
							pDstChunk->MoveEntityData(entity, firstFreeIdxInDstChunk++, entities);
							pSrcChunk->RemoveLastEntity(chunksToRemove);

							auto& lastEntityContainer = entities[entity.id()];
							lastEntityContainer.pChunk = pDstChunk;
							lastEntityContainer.idx = firstFreeIdxInDstChunk;
							lastEntityContainer.gen = entity.gen();

							// The destination chunk is full, we need to move to the next one
							if (firstFreeIdxInDstChunk == m_properties.capacity) {
								++front;

								// We reached the source chunk which means this archetype has been deframented
								if (front >= back) {
									maxEntities -= i + 1;
									return;
								}
							}
						}

						maxEntities -= entitiesToMove;
					}
				}
#endif

				//! Tries to locate a chunk that has some space left for a new entity.
				//! If not found a new chunk is created.
				GAIA_NODISCARD Chunk* FindOrCreateFreeChunk() {
					auto* pChunk = FindOrCreateFreeChunk_Internal(m_chunks);
					GAIA_ASSERT(!pChunk->HasDisabledEntities());
					return pChunk;
				}

				GAIA_NODISCARD uint32_t GetCapacity() const {
					return m_properties.capacity;
				}

				GAIA_NODISCARD const containers::darray<Chunk*>& GetChunks() const {
					return m_chunks;
				}

				GAIA_NODISCARD GenericComponentHash GetGenericHash() const {
					return m_genericHash;
				}

				GAIA_NODISCARD ChunkComponentHash GetChunkHash() const {
					return m_chunkHash;
				}

				GAIA_NODISCARD LookupHash GetLookupHash() const {
					return m_lookupHash;
				}

				GAIA_NODISCARD component::ComponentMatcherHash GetMatcherHash(component::ComponentType componentType) const {
					return m_matcherHash[componentType];
				}

				GAIA_NODISCARD const ComponentIdArray& GetComponentIdArray(component::ComponentType componentType) const {
					return m_componentIds[componentType];
				}

				GAIA_NODISCARD const ComponentOffsetArray&
				GetComponentOffsetArray(component::ComponentType componentType) const {
					return m_componentOffsets[componentType];
				}

				/*!
				Checks if a given component is present on the archetype.
				\return True if the component is present. False otherwise.
				*/
				template <typename T>
				GAIA_NODISCARD bool HasComponent() const {
					return HasComponent_Internal<T>();
				}

				/*!
				Returns the internal index of a component based on the provided \param componentId.
				\param componentType Component type
				\return Component index if the component was found. -1 otherwise.
				*/
				GAIA_NODISCARD uint32_t
				GetComponentIdx(component::ComponentType componentType, component::ComponentId componentId) const {
					const auto idx = utils::get_index_unsafe(m_componentIds[componentType], componentId);
					GAIA_ASSERT(idx != BadIndex);
					return (uint32_t)idx;
				}

				void BuildGraphEdges(
						Archetype* pArchetypeRight, component::ComponentType componentType, component::ComponentId componentId) {
					GAIA_ASSERT(pArchetypeRight != this);
					m_graph.AddGraphEdgeRight(componentType, componentId, pArchetypeRight->GetArchetypeId());
					pArchetypeRight->BuildGraphEdgesLeft(this, componentType, componentId);
				}

				void BuildGraphEdgesLeft(
						Archetype* pArchetypeLeft, component::ComponentType componentType, component::ComponentId componentId) {
					GAIA_ASSERT(pArchetypeLeft != this);
					m_graph.AddGraphEdgeLeft(componentType, componentId, pArchetypeLeft->GetArchetypeId());
				}

				//! Checks if the graph edge for component type \param componentType contains the component \param componentId.
				//! \return Archetype id of the target archetype if the edge is found. ArchetypeIdBad otherwise.
				GAIA_NODISCARD ArchetypeId
				FindGraphEdgeRight(component::ComponentType componentType, const component::ComponentId componentId) const {
					return m_graph.FindGraphEdgeRight(componentType, componentId);
				}

				//! Checks if the graph edge for component type \param componentType contains the component \param componentId.
				//! \return Archetype id of the target archetype if the edge is found. ArchetypeIdBad otherwise.
				GAIA_NODISCARD ArchetypeId
				FindGraphEdgeLeft(component::ComponentType componentType, const component::ComponentId componentId) const {
					return m_graph.FindGraphEdgeLeft(componentType, componentId);
				}

				static void DiagArchetype_PrintBasicInfo(const Archetype& archetype) {
					const auto& cc = ComponentCache::Get();
					const auto& genericComponents = archetype.m_componentIds[component::ComponentType::CT_Generic];
					const auto& chunkComponents = archetype.m_componentIds[component::ComponentType::CT_Chunk];

					// Caclulate the number of entites in archetype
					uint32_t entityCount = 0;
					uint32_t entityCountDisabled = 0;
					for (const auto* chunk: archetype.m_chunks) {
						entityCount += chunk->GetEntityCount();
						entityCountDisabled += chunk->GetDisabledEntityMask().count();
					}

					// Calculate the number of components
					uint32_t genericComponentsSize = 0;
					uint32_t chunkComponentsSize = 0;
					for (const auto componentId: genericComponents) {
						const auto& desc = cc.GetComponentDesc(componentId);
						genericComponentsSize += desc.properties.size;
					}
					for (const auto componentId: chunkComponents) {
						const auto& desc = cc.GetComponentDesc(componentId);
						chunkComponentsSize += desc.properties.size;
					}

					GAIA_LOG_N(
							"Archetype ID:%u, "
							"lookupHash:%016" PRIx64 ", "
							"mask:%016" PRIx64 "/%016" PRIx64 ", "
							"chunks:%u (%uK), data:%u/%u/%u B, "
							"entities:%u/%u/%u",
							archetype.m_archetypeId, archetype.m_lookupHash.hash,
							archetype.m_matcherHash[component::ComponentType::CT_Generic].hash,
							archetype.m_matcherHash[component::ComponentType::CT_Chunk].hash, (uint32_t)archetype.m_chunks.size(),
							Chunk::GetTotalChunkSize(archetype.m_properties.chunkDataBytes) <= 8192 ? 8 : 16, genericComponentsSize,
							chunkComponentsSize, archetype.m_properties.chunkDataBytes, entityCount, entityCountDisabled,
							archetype.m_properties.capacity);

					auto logComponentInfo = [](const component::ComponentInfo& info, const component::ComponentDesc& desc) {
						GAIA_LOG_N(
								"    lookupHash:%016" PRIx64 ", mask:%016" PRIx64 ", size:%3u B, align:%3u B, %.*s",
								info.lookupHash.hash, info.matcherHash.hash, desc.properties.size, desc.properties.alig,
								(uint32_t)desc.name.size(), desc.name.data());
					};

					if (!genericComponents.empty()) {
						GAIA_LOG_N("  Generic components - count:%u", (uint32_t)genericComponents.size());
						for (const auto componentId: genericComponents) {
							const auto& info = cc.GetComponentInfo(componentId);
							logComponentInfo(info, cc.GetComponentDesc(componentId));
						}
						if (!chunkComponents.empty()) {
							GAIA_LOG_N("  Chunk components - count:%u", (uint32_t)chunkComponents.size());
							for (const auto componentId: chunkComponents) {
								const auto& info = cc.GetComponentInfo(componentId);
								logComponentInfo(info, cc.GetComponentDesc(componentId));
							}
						}
					}
				}

				static void DiagArchetype_PrintGraphInfo(const Archetype& archetype) {
					archetype.m_graph.Diag();
				}

				static void DiagArchetype_PrintChunkInfo(const Archetype& archetype) {
					auto logChunks = [](const auto& chunks) {
						for (uint32_t i = 0; i < chunks.size(); ++i) {
							const auto* pChunk = chunks[i];
							pChunk->Diag((uint32_t)i);
						}
					};

					const auto& chunks = archetype.m_chunks;
					if (!chunks.empty())
						GAIA_LOG_N("  Chunks");

					logChunks(chunks);
				}

				/*!
				Performs diagnostics on a specific archetype. Prints basic info about it and the chunks it contains.
				\param archetype Archetype to run diagnostics on
				*/
				static void DiagArchetype(const Archetype& archetype) {
					DiagArchetype_PrintBasicInfo(archetype);
					DiagArchetype_PrintGraphInfo(archetype);
					DiagArchetype_PrintChunkInfo(archetype);
				}
			};

			class ArchetypeLookupKey final {
				Archetype::LookupHash m_hash;
				ArchetypeBase* m_pArchetypeBase;

			public:
				static constexpr bool IsDirectHashKey = true;

				ArchetypeLookupKey(): m_hash({0}), m_pArchetypeBase(nullptr) {}
				ArchetypeLookupKey(Archetype::LookupHash hash, ArchetypeBase* pArchetypeBase):
						m_hash(hash), m_pArchetypeBase(pArchetypeBase) {}

				size_t hash() const {
					return (size_t)m_hash.hash;
				}

				bool operator==(const ArchetypeLookupKey& other) const {
					// Hash doesn't match we don't have a match.
					// Hash collisions are expected to be very unlikely so optimize for this case.
					if GAIA_LIKELY (m_hash != other.m_hash)
						return false;

					const auto id = m_pArchetypeBase->GetArchetypeId();
					if (id == ArchetypeIdBad) {
						const auto* pArchetype = (const Archetype*)other.m_pArchetypeBase;
						const auto* pArchetypeLookupChecker = (const ArchetypeLookupChecker*)m_pArchetypeBase;
						return pArchetype->CompareComponentIds(*pArchetypeLookupChecker);
					}

					// Real ArchetypeID is given. Compare the pointers.
					// Normally we'd compare archetype IDs but because we do not allow archetype copies and all archetypes are
					// unique it's guaranteed that if pointers are the same we have a match.
					return m_pArchetypeBase == other.m_pArchetypeBase;
				}
			};
		} // namespace archetype
	} // namespace ecs
} // namespace gaia
