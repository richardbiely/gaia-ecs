#pragma once
#include "../containers/darray.h"
#include "../containers/sarray.h"
#include "../containers/sarray_ext.h"
#include "../utils/hashing_policy.h"
#include "../utils/mem.h"
#include "chunk.h"
#include "chunk_allocator.h"
#include "chunk_header.h"
#include "component.h"
#include "component_cache.h"
#include "component_utils.h"

namespace gaia {
	namespace ecs {
		class World;

		uint32_t& GetWorldVersionFromWorld(World& world);

		class Archetype final {
		public:
			using LookupHash = utils::direct_hash_key<uint64_t>;
			using GenericComponentHash = utils::direct_hash_key<uint64_t>;
			using ChunkComponentHash = utils::direct_hash_key<uint64_t>;

		private:
			friend class World;
			friend class CommandBuffer;
			friend class Chunk;
			friend struct ChunkHeader;

			static constexpr uint32_t BadIndex = (uint32_t)-1;

#if GAIA_ARCHETYPE_GRAPH
			struct ArchetypeGraphEdge {
				uint32_t archetypeId;
			};
#endif

			//! World to which this chunk belongs to
			const World* m_pParentWorld = nullptr;

			//! List of active chunks allocated by this archetype
			containers::darray<Chunk*> m_chunks;
			//! List of disabled chunks allocated by this archetype
			containers::darray<Chunk*> m_chunksDisabled;

#if GAIA_ARCHETYPE_GRAPH
			//! Map of edges in the archetype graph when adding components
			containers::map<ComponentLookupHash, ArchetypeGraphEdge> m_edgesAdd[ComponentType::CT_Count];
			//! Map of edges in the archetype graph when removing components
			containers::map<ComponentLookupHash, ArchetypeGraphEdge> m_edgesDel[ComponentType::CT_Count];
#endif

			//! Description of components within this archetype
			containers::sarray<ComponentIdList, ComponentType::CT_Count> m_componentIds;
			//! Lookup hashes of components within this archetype
			containers::sarray<ComponentOffsetList, ComponentType::CT_Count> m_componentOffsets;

			GenericComponentHash m_genericHash = {0};
			ChunkComponentHash m_chunkHash = {0};

			//! Hash of components within this archetype - used for lookups
			ComponentLookupHash m_lookupHash = {0};
			//! Hash of components within this archetype - used for matching
			ComponentMatcherHash m_matcherHash[ComponentType::CT_Count] = {0};
			//! Archetype ID - used to address the archetype directly in the world's list or archetypes
			uint32_t m_archetypeId = 0;
			//! Stable reference to parent world's world version
			uint32_t& m_worldVersion;
			struct {
				//! The number of entities this archetype can take (e.g 5 = 5 entities with all their components)
				uint32_t capacity : 16;
				//! True if there's a component that requires custom destruction
				uint32_t hasGenericComponentWithCustomDestruction : 1;
				//! True if there's a component that requires custom destruction
				uint32_t hasChunkComponentWithCustomDestruction : 1;
				//! Updated when chunks are being iterated. Used to inform of structural changes when they shouldn't happen.
				uint32_t structuralChangesLocked : 4;
			} m_properties{};

			// Constructor is hidden. Create archetypes via Create
			Archetype(uint32_t& worldVersion): m_worldVersion(worldVersion){};

			Archetype(Archetype&& world) = delete;
			Archetype(const Archetype& world) = delete;
			Archetype& operator=(Archetype&&) = delete;
			Archetype& operator=(const Archetype&) = delete;

			GAIA_NODISCARD static LookupHash
			CalculateLookupHash(GenericComponentHash genericHash, ChunkComponentHash chunkHash) noexcept {
				return {utils::hash_combine(genericHash.hash, chunkHash.hash)};
			}

			/*!
			Releases all memory allocated by \param pChunk.
			\param pChunk Chunk which we want to destroy
			*/
			void ReleaseChunk(Chunk* pChunk) const {
				const auto& cc = ComponentCache::Get();

				auto callDestructors = [&](ComponentType componentType) {
					const auto& componentIds = m_componentIds[componentType];
					const auto& offsets = m_componentOffsets[componentType];
					const auto itemCount = componentType == ComponentType::CT_Generic ? pChunk->GetItemCount() : 1U;
					for (size_t i = 0; i < componentIds.size(); ++i) {
						const auto componentId = componentIds[i];
						const auto& infoCreate = cc.GetComponentDesc(componentId);
						if (infoCreate.destructor == nullptr)
							continue;
						auto* pSrc = (void*)((uint8_t*)pChunk + offsets[i]);
						infoCreate.destructor(pSrc, itemCount);
					}
				};

				// Call destructors for components that need it
				if (m_properties.hasGenericComponentWithCustomDestruction == 1)
					callDestructors(ComponentType::CT_Generic);
				if (m_properties.hasChunkComponentWithCustomDestruction == 1)
					callDestructors(ComponentType::CT_Chunk);

				Chunk::Release(pChunk);
			}

			GAIA_NODISCARD static Archetype*
			Create(World& world, ComponentIdSpan componentIdsGeneric, ComponentIdSpan componentIdsChunk) {
				auto* newArch = new Archetype(GetWorldVersionFromWorld(world));
				newArch->m_pParentWorld = &world;

#if GAIA_ARCHETYPE_GRAPH
				// Preallocate arrays for graph edges
				// Generic components are going to be more common so we prepare bigger arrays for them.
				// Chunk components are expected to be very rare so only a small buffer is preallocated.
				newArch->m_edgesAdd[ComponentType::CT_Generic].reserve(8);
				newArch->m_edgesAdd[ComponentType::CT_Chunk].reserve(1);
				newArch->m_edgesDel[ComponentType::CT_Generic].reserve(8);
				newArch->m_edgesDel[ComponentType::CT_Chunk].reserve(1);
#endif

				const auto& cc = ComponentCache::Get();

				// Size of the entity + all of its generic components
				size_t genericComponentListSize = sizeof(Entity);
				for (const uint32_t componentId: componentIdsGeneric) {
					const auto& desc = cc.GetComponentDesc(componentId);
					genericComponentListSize += desc.properties.size;
					newArch->m_properties.hasGenericComponentWithCustomDestruction |= (desc.properties.destructible != 0);
				}

				// Size of chunk components
				size_t chunkComponentListSize = 0;
				for (const uint32_t componentId: componentIdsChunk) {
					const auto& desc = cc.GetComponentDesc(componentId);
					chunkComponentListSize += desc.properties.size;
					newArch->m_properties.hasChunkComponentWithCustomDestruction |= (desc.properties.destructible != 0);
				}

				// TODO: Calculate the number of entities per chunks precisely so we can
				// fit more of them into chunk on average. Currently, DATA_SIZE_RESERVED
				// is substracted but that's not optimal...

				// Number of components we can fit into one chunk
				auto maxGenericItemsInArchetype = (Chunk::DATA_SIZE - chunkComponentListSize) / genericComponentListSize;

				// Calculate component offsets now. Skip the header and entity IDs
				auto componentOffsets = sizeof(Entity) * maxGenericItemsInArchetype;
				auto alignedOffset = sizeof(ChunkHeader) + componentOffsets;

				// Add generic infos
				for (const uint32_t componentId: componentIdsGeneric) {
					const auto& desc = cc.GetComponentDesc(componentId);
					const auto alignment = desc.properties.alig;
					if (alignment != 0) {
						const size_t padding = utils::align(alignedOffset, alignment) - alignedOffset;
						componentOffsets += padding;
						alignedOffset += padding;

						// Make sure we didn't exceed the chunk size
						GAIA_ASSERT(componentOffsets <= Chunk::DATA_SIZE_NORESERVE);

						// Register the component info
						newArch->m_componentIds[ComponentType::CT_Generic].push_back(componentId);
						newArch->m_componentOffsets[ComponentType::CT_Generic].push_back((uint32_t)componentOffsets);

						// Make sure the following component list is properly aligned
						componentOffsets += desc.properties.size * maxGenericItemsInArchetype;
						alignedOffset += desc.properties.size * maxGenericItemsInArchetype;

						// Make sure we didn't exceed the chunk size
						GAIA_ASSERT(componentOffsets <= Chunk::DATA_SIZE_NORESERVE);
					} else {
						// Register the component info
						newArch->m_componentIds[ComponentType::CT_Generic].push_back(componentId);
						newArch->m_componentOffsets[ComponentType::CT_Generic].push_back((uint32_t)componentOffsets);
					}
				}

				// Add chunk infos
				for (const uint32_t componentId: componentIdsChunk) {
					const auto& desc = cc.GetComponentDesc(componentId);
					const auto alignment = desc.properties.alig;
					if (alignment != 0) {
						const size_t padding = utils::align(alignedOffset, alignment) - alignedOffset;
						componentOffsets += padding;
						alignedOffset += padding;

						// Make sure we didn't exceed the chunk size
						GAIA_ASSERT(componentOffsets <= Chunk::DATA_SIZE_NORESERVE);

						// Register the component info
						newArch->m_componentIds[ComponentType::CT_Chunk].push_back(componentId);
						newArch->m_componentOffsets[ComponentType::CT_Chunk].push_back((uint32_t)componentOffsets);

						// Make sure the following component list is properly aligned
						componentOffsets += desc.properties.size;
						alignedOffset += desc.properties.size;

						// Make sure we didn't exceed the chunk size
						GAIA_ASSERT(componentOffsets <= Chunk::DATA_SIZE_NORESERVE);
					} else {
						// Register the component info
						newArch->m_componentIds[ComponentType::CT_Chunk].push_back(componentId);
						newArch->m_componentOffsets[ComponentType::CT_Chunk].push_back((uint32_t)componentOffsets);
					}
				}

				newArch->m_properties.capacity = (uint32_t)maxGenericItemsInArchetype;
				newArch->m_matcherHash[ComponentType::CT_Generic] = CalculateMatcherHash(componentIdsGeneric);
				newArch->m_matcherHash[ComponentType::CT_Chunk] = CalculateMatcherHash(componentIdsChunk);

				return newArch;
			}

			GAIA_NODISCARD Chunk* FindOrCreateFreeChunk_Internal(containers::darray<Chunk*>& chunkArray) const {
				const auto chunkCnt = chunkArray.size();

				if (chunkCnt > 0) {
					// Look for chunks with free space back-to-front.
					// We do it this way because we always try to keep fully utilized and
					// thus only the one in the back should be free.
					auto i = chunkCnt - 1;
					do {
						auto* pChunk = chunkArray[i];
						GAIA_ASSERT(pChunk != nullptr);
						if (!pChunk->IsFull())
							return pChunk;
					} while (i-- > 0);
				}

				GAIA_ASSERT(chunkCnt < UINT16_MAX);

				// No free space found anywhere. Let's create a new chunk.
				auto* pChunk = Chunk::Create(
						m_archetypeId, (uint16_t)chunkCnt, m_properties.capacity, m_worldVersion, m_componentIds,
						m_componentOffsets);

				chunkArray.push_back(pChunk);
				return pChunk;
			}

			//! Tries to locate a chunk that has some space left for a new entity.
			//! If not found a new chunk is created
			GAIA_NODISCARD Chunk* FindOrCreateFreeChunk() {
				return FindOrCreateFreeChunk_Internal(m_chunks);
			}

			//! Tries to locate a chunk for disabled entities that has some space left for a new one.
			//! If not found a new chunk is created
			GAIA_NODISCARD Chunk* FindOrCreateFreeChunkDisabled() {
				auto* pChunk = FindOrCreateFreeChunk_Internal(m_chunksDisabled);
				pChunk->SetDisabled(true);
				return pChunk;
			}

			/*!
			Removes a chunk from the list of chunks managed by their achetype.
			\param pChunk Chunk to remove from the list of managed archetypes
			*/
			void RemoveChunk(Chunk* pChunk) {
				const bool isDisabled = pChunk->IsDisabled();
				const auto chunkIndex = pChunk->GetChunkIndex();

				ReleaseChunk(pChunk);

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

#if GAIA_ARCHETYPE_GRAPH
			//! Create an edge in the graph leading from this archetype to \param archetypeId via component \param info.
			void AddEdgeArchetypeRight(ComponentType componentType, const ComponentInfo& info, uint32_t archetypeId) {
				[[maybe_unused]] const auto ret =
						m_edgesAdd[componentType].try_emplace({info.lookupHash}, ArchetypeGraphEdge{archetypeId});
				GAIA_ASSERT(ret.second);
			}

			//! Create an edge in the graph leading from this archetype to \param archetypeId via component \param info.
			void AddEdgeArchetypeLeft(ComponentType componentType, const ComponentInfo& info, uint32_t archetypeId) {
				[[maybe_unused]] const auto ret =
						m_edgesDel[componentType].try_emplace({info.lookupHash}, ArchetypeGraphEdge{archetypeId});
				GAIA_ASSERT(ret.second);
			}

			GAIA_NODISCARD uint32_t FindAddEdgeArchetypeId(ComponentType componentType, const ComponentInfo& info) const {
				const auto& edges = m_edgesAdd[componentType];
				const auto it = edges.find({info.lookupHash});
				return it != edges.end() ? it->second.archetypeId : BadIndex;
			}

			GAIA_NODISCARD uint32_t FindDelEdgeArchetypeId(ComponentType componentType, const ComponentInfo& info) const {
				const auto& edges = m_edgesDel[componentType];
				const auto it = edges.find({info.lookupHash});
				return it != edges.end() ? it->second.archetypeId : BadIndex;
			}
#endif

			/*!
			Checks if a component with \param componentId and type \param componentType is present in the archetype.
			\param componentId Component id
			\param componentType Component type
			\return True if found. False otherwise.
			*/
			GAIA_NODISCARD bool HasComponent_Internal(ComponentType componentType, uint32_t componentId) const {
				const auto& componentIds = GetComponentIdList(componentType);
				return utils::has(componentIds, componentId);
			}

			/*!
			Checks if a component of type \tparam T is present in the archetype.
			\return True if found. False otherwise.
			*/
			template <typename T>
			GAIA_NODISCARD bool HasComponent_Internal() const {
				const auto componentId = GetComponentId<T>();

				if constexpr (IsGenericComponent<T>) {
					return HasComponent_Internal(ComponentType::CT_Generic, componentId);
				} else {
					return HasComponent_Internal(ComponentType::CT_Chunk, componentId);
				}
			}

		public:
			/*!
			Checks if the archetype id is valid.
			\return True if the id is valid, false otherwise.
			*/
			static bool IsIdValid(uint32_t id) {
				return id != BadIndex;
			}

			~Archetype() {
				// Delete all archetype chunks
				for (auto* pChunk: m_chunks)
					ReleaseChunk(pChunk);
				for (auto* pChunk: m_chunksDisabled)
					ReleaseChunk(pChunk);
			}

			/*!
			Initializes the archetype with hash values for each kind of component types.
			\param hashGeneric Generic components hash
			\param hashChunk Chunk components hash
			\param hashLookup Hash used for archetype lookup purposes
			*/
			void Init(GenericComponentHash hashGeneric, ChunkComponentHash hashChunk, ComponentLookupHash hashLookup) {
				m_genericHash = hashGeneric;
				m_chunkHash = hashChunk;
				m_lookupHash = hashLookup;
			}

			void SetStructuralChanges(bool value) {
				if (value) {
					GAIA_ASSERT(m_properties.structuralChangesLocked < 16);
					++m_properties.structuralChangesLocked;
				} else {
					GAIA_ASSERT(m_properties.structuralChangesLocked > 0);
					--m_properties.structuralChangesLocked;
				}
			}

			bool IsStructuralChangesLock() const {
				return m_properties.structuralChangesLocked != 0;
			}

			GAIA_NODISCARD const World& GetWorld() const {
				return *m_pParentWorld;
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

			GAIA_NODISCARD ComponentMatcherHash GetMatcherHash(ComponentType componentType) const {
				return m_matcherHash[componentType];
			}

			GAIA_NODISCARD const ComponentIdList& GetComponentIdList(ComponentType componentType) const {
				return m_componentIds[componentType];
			}

			GAIA_NODISCARD const ComponentOffsetList& GetComponentOffsetList(ComponentType componentType) const {
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
			GAIA_NODISCARD uint32_t GetComponentIdx(ComponentType componentType, ComponentId componentId) const {
				const auto idx = utils::get_index_unsafe(m_componentIds[componentType], componentId);
				GAIA_ASSERT(idx != BadIndex);
				return (uint32_t)idx;
			}
		};

		GAIA_NODISCARD inline ComponentMatcherHash
		GetArchetypeMatcherHash(const Archetype& archetype, ComponentType componentType) {
			return archetype.GetMatcherHash(componentType);
		}

		GAIA_NODISCARD inline const ComponentIdList&
		GetArchetypeComponentInfoList(const Archetype& archetype, ComponentType componentType) {
			return archetype.GetComponentIdList(componentType);
		}

		GAIA_NODISCARD inline const ComponentOffsetList&
		GetArchetypeComponentOffsetList(const Archetype& archetype, ComponentType componentType) {
			return archetype.GetComponentOffsetList(componentType);
		}
	} // namespace ecs
} // namespace gaia

REGISTER_HASH_TYPE(gaia::ecs::Archetype::LookupHash)
REGISTER_HASH_TYPE(gaia::ecs::Archetype::GenericComponentHash)
REGISTER_HASH_TYPE(gaia::ecs::Archetype::ChunkComponentHash)