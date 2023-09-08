#pragma once
#include "../config/config.h"

#include <cinttypes>

#include "../containers/darray.h"
#include "../containers/sarray.h"
#include "../containers/sarray_ext.h"
#include "../utils/hashing_policy.h"
#include "../utils/mem.h"
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
			class Archetype final {
			public:
				using LookupHash = utils::direct_hash_key<uint64_t>;
				using GenericComponentHash = utils::direct_hash_key<uint64_t>;
				using ChunkComponentHash = utils::direct_hash_key<uint64_t>;

			private:
				//! List of active chunks allocated by this archetype
				containers::darray<Chunk*> m_chunks;
				//! List of disabled chunks allocated by this archetype
				containers::darray<Chunk*> m_chunksDisabled;

				ArchetypeGraph m_graph;

				//! Offsets to various parts of data inside chunk
				ChunkHeaderOffsets m_dataOffsets;
				//! Description of components within this archetype
				containers::sarray<ComponentIdArray, component::ComponentType::CT_Count> m_componentIds;
				//! Lookup hashes of components within this archetype
				containers::sarray<ComponentOffsetArray, component::ComponentType::CT_Count> m_componentOffsets;

				GenericComponentHash m_genericHash = {0};
				ChunkComponentHash m_chunkHash = {0};

				//! Hash of components within this archetype - used for lookups
				component::ComponentLookupHash m_lookupHash = {0};
				//! Hash of components within this archetype - used for matching
				component::ComponentMatcherHash m_matcherHash[component::ComponentType::CT_Count]{};
				//! Archetype ID - used to address the archetype directly in the world's list or archetypes
				ArchetypeId m_archetypeId = ArchetypeIdBad;
				//! Stable reference to parent world's world version
				uint32_t& m_worldVersion;
				struct {
					//! The number of entities this archetype can take (e.g 5 = 5 entities with all their components)
					uint32_t capacity : 16;
				} m_properties{};

				// Constructor is hidden. Create archetypes via Create
				Archetype(uint32_t& worldVersion): m_worldVersion(worldVersion) {}

				void UpdateDataOffsets(uintptr_t memoryAddress) {
					size_t offset = 0;

					// Versions
					{
						const size_t padding = utils::align(memoryAddress, alignof(uint32_t)) - memoryAddress;
						offset += padding;

						if (!m_componentIds[component::ComponentType::CT_Generic].empty()) {
							m_dataOffsets.firstByte_Versions[component::ComponentType::CT_Generic] = (ChunkComponentOffset)offset;
							offset += sizeof(uint32_t) * m_componentIds[component::ComponentType::CT_Generic].size();
						}
						if (!m_componentIds[component::ComponentType::CT_Chunk].empty()) {
							m_dataOffsets.firstByte_Versions[component::ComponentType::CT_Chunk] = (ChunkComponentOffset)offset;
							offset += sizeof(uint32_t) * m_componentIds[component::ComponentType::CT_Chunk].size();
						}
					}

					// Component ids
					{
						const size_t padding = utils::align(offset, alignof(component::ComponentId)) - offset;
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
						const size_t padding = utils::align(offset, alignof(ChunkComponentOffset)) - offset;
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
						const size_t padding = utils::align(offset, alignof(Entity)) - offset;
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
						// Find first non-empty chunk
						for (auto* pChunk: chunkArray) {
							GAIA_ASSERT(pChunk != nullptr);
							if (pChunk->IsFull())
								continue;
							return pChunk;
						}
#endif
					}

					// Make sure not too many chunks are allocated
					GAIA_ASSERT(chunkCnt < UINT16_MAX);

					// No free space found anywhere. Let's create a new chunk.
					auto* pChunk = Chunk::Create(
							m_archetypeId, (uint16_t)chunkCnt, m_properties.capacity, m_worldVersion, m_dataOffsets, m_componentIds,
							m_componentOffsets);

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

					if constexpr (component::IsGenericComponent<T>) {
						return HasComponent_Internal(component::ComponentType::CT_Generic, componentId);
					} else {
						return HasComponent_Internal(component::ComponentType::CT_Chunk, componentId);
					}
				}

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
					for (auto* pChunk: m_chunksDisabled)
						Chunk::Release(pChunk);
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
					newArch->m_componentIds[component::ComponentType::CT_Generic].resize(componentIdsGeneric.size());
					newArch->m_componentIds[component::ComponentType::CT_Chunk].resize(componentIdsChunk.size());
					newArch->m_componentOffsets[component::ComponentType::CT_Generic].resize(componentIdsGeneric.size());
					newArch->m_componentOffsets[component::ComponentType::CT_Chunk].resize(componentIdsChunk.size());
					newArch->UpdateDataOffsets(sizeof(ChunkHeader));

					const auto& cc = ComponentCache::Get();
					const auto& dataOffset = newArch->m_dataOffsets;

					// Calculate the number of entities per chunks precisely so we can
					// fit as many of them into chunk as possible.

					// Total size of generic components
					size_t genericComponentListSize = 0;
					for (const auto componentId: componentIdsGeneric) {
						const auto& desc = cc.GetComponentDesc(componentId);
						genericComponentListSize += desc.properties.size;
					}

					// Total size of chunk components
					size_t chunkComponentListSize = 0;
					for (const auto componentId: componentIdsChunk) {
						const auto& desc = cc.GetComponentDesc(componentId);
						chunkComponentListSize += desc.properties.size;
					}

					// Theoretical maximum number of components we can fit into one chunk.
					// This can be further reduced due alignment and padding.
					auto maxGenericItemsInArchetype =
							(Chunk::DATA_SIZE - dataOffset.firstByte_EntityData - chunkComponentListSize - 1) /
							(genericComponentListSize + sizeof(Entity));

				recalculate:
					auto componentOffsets = dataOffset.firstByte_EntityData + sizeof(Entity) * maxGenericItemsInArchetype;

					auto adjustMaxGenericItemsInAchetype = [&](component::ComponentIdSpan componentIds, size_t size) {
						for (const auto componentId: componentIds) {
							const auto& desc = cc.GetComponentDesc(componentId);
							const auto alignment = desc.properties.alig;
							if (alignment == 0)
								continue;

							const auto padding = utils::align(componentOffsets, alignment) - componentOffsets;

							// For SoA types we shall assume there is a padding of the entire size of the array.
							// Of course this is a bit wasteful but it's a bit of work to calculate how much area exactly we need.
							// We might have:
							// 	struct foo { float x; float y; bool a; float z; };
							// Each of the variables of the foo struct might need separate padding when converted to SoA.
							// TODO: Introduce a function that can calculate this.
							const auto componentDataSize =
									padding + ((uint32_t)desc.properties.soa * desc.properties.size) + desc.properties.size * size;
							const auto nextOffset = componentOffsets + componentDataSize;

							// If we're beyond what the chunk could take, subtract one entity
							if (nextOffset >= Chunk::DATA_SIZE) {
								--maxGenericItemsInArchetype;
								return false;
							}

							componentOffsets += componentDataSize;
						}

						return true;
					};

					// Adjust the maximum number of entities. Recalculation happens at most once when the original guess
					// for entity count is not right (most likely because of padding or usage of SoA components).
					if (!adjustMaxGenericItemsInAchetype(componentIdsGeneric, maxGenericItemsInArchetype))
						goto recalculate;
					if (!adjustMaxGenericItemsInAchetype(componentIdsChunk, 1))
						goto recalculate;

					// Update the offsets according to the recalculated maxGenericItemsInArchetype
					componentOffsets = dataOffset.firstByte_EntityData + sizeof(Entity) * maxGenericItemsInArchetype;

					auto registerComponents = [&](component::ComponentIdSpan componentIds, component::ComponentType componentType,
																				size_t count) {
						auto& ids = newArch->m_componentIds[componentType];
						auto& ofs = newArch->m_componentOffsets[componentType];

						for (size_t i = 0; i < componentIds.size(); ++i) {
							const auto componentId = componentIds[i];
							const auto& desc = cc.GetComponentDesc(componentId);
							const auto alignment = desc.properties.alig;
							if (alignment == 0) {
								GAIA_ASSERT(desc.properties.size == 0);

								// Register the component info
								ids[i] = componentId;
								ofs[i] = {};
							} else {
								const size_t padding = utils::align(componentOffsets, alignment) - componentOffsets;
								componentOffsets += padding;

								// Register the component info
								ids[i] = componentId;
								ofs[i] = (ChunkComponentOffset)componentOffsets;

								// Make sure the following component list is properly aligned
								componentOffsets += desc.properties.size * count;
							}
						}
					};
					registerComponents(componentIdsGeneric, component::ComponentType::CT_Generic, maxGenericItemsInArchetype);
					registerComponents(componentIdsChunk, component::ComponentType::CT_Chunk, 1);

					newArch->m_properties.capacity = (uint32_t)maxGenericItemsInArchetype;
					newArch->m_matcherHash[component::ComponentType::CT_Generic] =
							component::CalculateMatcherHash(componentIdsGeneric);
					newArch->m_matcherHash[component::ComponentType::CT_Chunk] =
							component::CalculateMatcherHash(componentIdsChunk);

					return newArch;
				}

				/*!
				Initializes the archetype with hash values for each kind of component types.
				\param hashGeneric Generic components hash
				\param hashChunk Chunk components hash
				\param hashLookup Hash used for archetype lookup purposes
				*/
				void Init(
						GenericComponentHash hashGeneric, ChunkComponentHash hashChunk, component::ComponentLookupHash hashLookup) {
					m_genericHash = hashGeneric;
					m_chunkHash = hashChunk;
					m_lookupHash = hashLookup;
				}

				/*!
				Removes a chunk from the list of chunks managed by their achetype.
				\param pChunk Chunk to remove from the list of managed archetypes
				*/
				void RemoveChunk(Chunk* pChunk) {
					const bool isDisabled = pChunk->IsDisabled();
					const auto chunkIndex = pChunk->GetChunkIndex();

					Chunk::Release(pChunk);

					auto remove = [&](auto& chunkArray) {
						if (chunkArray.size() > 1)
							chunkArray.back()->SetChunkIndex(chunkIndex);
						GAIA_ASSERT(chunkIndex == utils::get_index(chunkArray, pChunk));
						utils::erase_fast(chunkArray, chunkIndex);
					};

					if (isDisabled)
						remove(m_chunksDisabled);
					else
						remove(m_chunks);
				}

#if GAIA_AVOID_CHUNK_FRAGMENTATION
				void VerifyChunksFramentation() const {
					VerifyChunksFragmentation_Internal(m_chunks);
					VerifyChunksFragmentation_Internal(m_chunksDisabled);
				}

				//! Returns the first non-empty chunk or nullptr if none is found.
				GAIA_NODISCARD Chunk* FindFirstNonEmptyChunk() const {
					auto* pChunk = FindFirstNonEmptyChunk_Internal(m_chunks);
					GAIA_ASSERT(pChunk == nullptr || !pChunk->IsDisabled());
					return pChunk;
				}

				//! Returns the first non-empty disabled chunk or nullptr if none is found.
				GAIA_NODISCARD Chunk* FindFirstNonEmptyChunkDisabled() const {
					auto* pChunk = FindFirstNonEmptyChunk_Internal(m_chunksDisabled);
					GAIA_ASSERT(pChunk == nullptr || pChunk->IsDisabled());
					return pChunk;
				}
#endif

				//! Tries to locate a chunk that has some space left for a new entity.
				//! If not found a new chunk is created.
				GAIA_NODISCARD Chunk* FindOrCreateFreeChunk() {
					auto* pChunk = FindOrCreateFreeChunk_Internal(m_chunks);
					GAIA_ASSERT(!pChunk->IsDisabled());
					return pChunk;
				}

				//! Tries to locate a chunk for disabled entities that has some space left for a new one.
				//! If not found a new chunk is created.
				GAIA_NODISCARD Chunk* FindOrCreateFreeChunkDisabled() {
					auto* pChunk = FindOrCreateFreeChunk_Internal(m_chunksDisabled);
					pChunk->SetDisabled(true);
					return pChunk;
				}

				GAIA_NODISCARD ArchetypeId GetArchetypeId() const {
					return m_archetypeId;
				}

				GAIA_NODISCARD uint32_t GetCapacity() const {
					return m_properties.capacity;
				}

				GAIA_NODISCARD const containers::darray<Chunk*>& GetChunks() const {
					return m_chunks;
				}

				GAIA_NODISCARD const containers::darray<Chunk*>& GetChunksDisabled() const {
					return m_chunksDisabled;
				}

				GAIA_NODISCARD GenericComponentHash GetGenericHash() const {
					return m_genericHash;
				}

				GAIA_NODISCARD ChunkComponentHash GetChunkHash() const {
					return m_chunkHash;
				}

				GAIA_NODISCARD component::ComponentLookupHash GetLookupHash() const {
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
					for (const auto* chunk: archetype.m_chunks)
						entityCount += chunk->GetEntityCount();
					for (const auto* chunk: archetype.m_chunksDisabled) {
						entityCountDisabled += chunk->GetEntityCount();
						entityCount += chunk->GetEntityCount();
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
							"chunks:%u, data size:%3u B (%u/%u), "
							"entities:%u/%u (disabled:%u)",
							archetype.m_archetypeId, archetype.m_lookupHash.hash,
							archetype.m_matcherHash[component::ComponentType::CT_Generic].hash,
							archetype.m_matcherHash[component::ComponentType::CT_Chunk].hash, (uint32_t)archetype.m_chunks.size(),
							genericComponentsSize + chunkComponentsSize, genericComponentsSize, chunkComponentsSize, entityCount,
							archetype.m_properties.capacity, entityCountDisabled);

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
						for (size_t i = 0; i < chunks.size(); ++i) {
							const auto* pChunk = chunks[i];
							pChunk->Diag((uint32_t)i);
						}
					};

					// Enabled chunks
					{
						const auto& chunks = archetype.m_chunks;
						if (!chunks.empty())
							GAIA_LOG_N("  Enabled chunks");

						logChunks(chunks);
					}

					// Disabled chunks
					{
						const auto& chunks = archetype.m_chunksDisabled;
						if (!chunks.empty())
							GAIA_LOG_N("  Disabled chunks");

						logChunks(chunks);
					}
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
		} // namespace archetype
	} // namespace ecs
} // namespace gaia

REGISTER_HASH_TYPE(gaia::ecs::Archetype::LookupHash)
REGISTER_HASH_TYPE(gaia::ecs::Archetype::GenericComponentHash)
REGISTER_HASH_TYPE(gaia::ecs::Archetype::ChunkComponentHash)