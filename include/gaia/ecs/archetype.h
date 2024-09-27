#pragma once
#include "../config/config.h"

#include <cinttypes>
#include <cstdint>

#include "../cnt/darray.h"
#include "../cnt/dbitset.h"
#include "../cnt/sarray.h"
#include "../cnt/sarray_ext.h"
#include "../core/hashing_policy.h"
#include "../mem/mem_alloc.h"
#include "archetype_common.h"
#include "archetype_graph.h"
#include "chunk.h"
#include "chunk_allocator.h"
#include "chunk_header.h"
#include "component.h"
#include "component_cache.h"
#include "component_cache_item.h"
#include "component_desc.h"
#include "id.h"

namespace gaia {
	namespace ecs {
		class World;
		class Archetype;
		struct EntityContainer;

		const ComponentCache& comp_cache(const World& world);
		Entity entity_from_id(const World& world, EntityId id);
		const char* entity_name(const World& world, Entity entity);
		const char* entity_name(const World& world, EntityId entityId);

		namespace detail {
			GAIA_NODISCARD inline bool cmp_comps(EntitySpan comps, EntitySpan compsOther) {
				// Size has to match
				GAIA_FOR(EntityKind::EK_Count) {
					if (comps.size() != compsOther.size())
						return false;
				}

				// Elements have to match
				GAIA_EACH(comps) {
					if (comps[i] != compsOther[i])
						return false;
				}

				return true;
			}
		} // namespace detail

		struct ArchetypeChunkPair {
			Archetype* pArchetype;
			Chunk* pChunk;
			GAIA_NODISCARD bool operator==(const ArchetypeChunkPair& other) const {
				return pArchetype == other.pArchetype && pChunk == other.pChunk;
			}
		};

		class ArchetypeBase {
		protected:
			//! Archetype ID - used to address the archetype directly in the world's list or archetypes
			ArchetypeId m_archetypeId = ArchetypeIdBad;

		public:
			GAIA_NODISCARD ArchetypeId id() const {
				return m_archetypeId;
			}
		};

		class ArchetypeLookupChecker: public ArchetypeBase {
			friend class Archetype;

			//! Span of component indices
			EntitySpan m_comps;

		public:
			ArchetypeLookupChecker(EntitySpan comps): m_comps(comps) {}

			GAIA_NODISCARD bool cmp_comps(const ArchetypeLookupChecker& other) const {
				return detail::cmp_comps(m_comps, other.m_comps);
			}
		};

		class Archetype final: public ArchetypeBase {
		public:
			using LookupHash = core::direct_hash_key<uint64_t>;

			//! Number of bits representing archetype lifespan
			static constexpr uint16_t ARCHETYPE_LIFESPAN_BITS = 7;
			//! Archetype lifespan must be at least as long as chunk lifespan
			static_assert(ARCHETYPE_LIFESPAN_BITS >= ChunkHeader::CHUNK_LIFESPAN_BITS);
			//! Number of ticks before empty chunks are removed
			static constexpr uint16_t MAX_ARCHETYPE_LIFESPAN = (1 << ARCHETYPE_LIFESPAN_BITS) - 1;

			struct Properties {
				//! The number of data entities this archetype can take (e.g 5 = 5 entities with all their components)
				uint16_t capacity;
				//! How many bytes of data is needed for a fully utilized chunk
				ChunkDataOffset chunkDataBytes;
				//! The number of generic entities/components
				uint8_t genEntities;
				//! Total number of entities/components
				uint8_t cntEntities;

				//! True if there's any generic component that requires custom construction
				uint8_t hasAnyCustomGenCtor : 1;
				//! True if there's any unique component that requires custom construction
				uint8_t hasAnyCustomUniCtor : 1;
				//! True if there's any generic component that requires custom destruction
				uint8_t hasAnyCustomGenDtor : 1;
				//! True if there's any unique component that requires custom destruction
				uint8_t hasAnyCustomUniDtor : 1;
				//!
				uint8_t unused : 4;
			};

		private:
			using AsPairsIndexBuffer = cnt::sarr<uint8_t, ChunkHeader::MAX_COMPONENTS>;

			ArchetypeIdLookupKey::LookupHash m_archetypeIdHash;
			//! Hash of components within this archetype - used for lookups
			LookupHash m_hashLookup = {0};

			Properties m_properties{};
			//! Component cache reference
			const ComponentCache& m_cc;
			//! Stable reference to parent world's world version
			uint32_t& m_worldVersion;

			//! Index of the first chunk with enough space to add at least one entity
			uint32_t m_firstFreeChunkIdx = 0;
			//! Array of chunks allocated by this archetype
			cnt::darray<Chunk*> m_chunks;
			//! Mask of chunks with disabled entities
			// cnt::dbitset m_disabledMask;
			//! Graph of archetypes linked with this one
			ArchetypeGraph m_graph;

			//! Offsets to various parts of data inside the chunk
			ChunkDataOffsets m_dataOffsets;
			//! Array of entities used to identify the archetype
			Entity m_ids[ChunkHeader::MAX_COMPONENTS];
			//! Array of indices to Is relationship pairs in m_ids
			uint8_t m_pairs_as_index_buffer[ChunkHeader::MAX_COMPONENTS];
			//! Array of component ids
			Component m_comps[ChunkHeader::MAX_COMPONENTS];
			//! Array of components offset indices
			ChunkDataOffset m_compOffs[ChunkHeader::MAX_COMPONENTS];
			//! Array of pointers to component cache items
			const ComponentCacheItem* m_pItems[ChunkHeader::MAX_COMPONENTS];

			//! Archetype list index
			uint32_t m_listIdx;

			//! Delete requested
			uint32_t m_deleteReq : 1;
			//! If set the archetype is to be deleted
			uint32_t m_dead : 1;
			//! Max lifespan of the archetype
			uint32_t m_lifespanCountdownMax: ARCHETYPE_LIFESPAN_BITS;
			//! Remaining lifespan of the archetype
			uint32_t m_lifespanCountdown: ARCHETYPE_LIFESPAN_BITS;
			//! Number of relationship pairs on the archetype
			uint32_t m_pairCnt: ChunkHeader::MAX_COMPONENTS_BITS;
			//! Number of Is relationship pairs on the archetype
			uint32_t m_pairCnt_is: ChunkHeader::MAX_COMPONENTS_BITS;
			//! Unused bits
			// uint32_t m_unused : 2;

			//! Constructor is hidden. Create archetypes via Archetype::Create
			Archetype(const ComponentCache& cc, uint32_t& worldVersion):
					m_cc(cc), m_worldVersion(worldVersion), m_listIdx(BadIndex), //
					m_deleteReq(0), m_dead(0), //
					m_lifespanCountdownMax(0), m_lifespanCountdown(0), //
					m_pairCnt(0), m_pairCnt_is(0) {}

			~Archetype() {
				// Delete all archetype chunks
				for (auto* pChunk: m_chunks)
					Chunk::free(pChunk);
			}

			//! Calculates offsets in memory at which important chunk data is going to be stored.
			//! These offsets are used to setup the chunk data area layout.
			//! \param memoryAddress Memory address used to calculate offsets
			void update_data_offsets(uintptr_t memoryAddress) {
				uintptr_t offset = mem::padding<alignof(Entity)>(memoryAddress);
				// The number must fit into firstByte_EntityData
				GAIA_ASSERT(offset < 256);
				m_dataOffsets.firstByte_EntityData = (uint8_t)offset;
			}

			//! Estimates how many entities can fit into the chunk described by \param comps components.
			static bool est_max_entities_per_archetype(
					const ComponentCache& cc, uint32_t& offs, uint32_t& maxItems, ComponentSpan comps, uint32_t size,
					uint32_t maxDataOffset) {
				for (const auto comp: comps) {
					if (comp.alig() == 0)
						continue;

					const auto& desc = cc.get(comp.id());

					// If we're beyond what the chunk could take, subtract one entity
					const auto nextOffset = desc.calc_new_mem_offset(offs, size);
					if (nextOffset >= maxDataOffset) {
						const auto subtractItems = (nextOffset - maxDataOffset + comp.size()) / comp.size();
						GAIA_ASSERT(subtractItems > 0);
						GAIA_ASSERT(maxItems > subtractItems);
						maxItems -= subtractItems;
						return false;
					}

					offs = nextOffset;
				}

				return true;
			};

			static void reg_components(
					const ComponentCache& cc, Archetype& arch, EntitySpan ids, ComponentSpan comps, uint8_t from, uint8_t to,
					uint32_t& currOff, uint32_t count) {
				auto& ofs = arch.m_compOffs;

				// Set component ids
				GAIA_FOR2(from, to) arch.m_ids[i] = ids[i];

				// Set item pointers
				GAIA_FOR2(from, to) {
					const auto* pItem = cc.find(comps[i].id());
					arch.m_pItems[i] = pItem;
					if (pItem == nullptr || pItem->comp.size() == 0U)
						continue;

					const auto e = ids[i];
					if (e.kind() == EntityKind::EK_Gen) {
						arch.m_properties.hasAnyCustomGenCtor |= (pItem->func_ctor != nullptr);
						arch.m_properties.hasAnyCustomGenDtor |= (pItem->func_dtor != nullptr);
					} else {
						arch.m_properties.hasAnyCustomUniCtor |= (pItem->func_ctor != nullptr);
						arch.m_properties.hasAnyCustomUniDtor |= (pItem->func_dtor != nullptr);
					}
				}

				// Calculate offsets and assign them indices according to our mappings
				GAIA_FOR2(from, to) {
					const auto comp = comps[i];
					const auto compIdx = i;

					const auto alig = comp.alig();
					if (alig == 0) {
						ofs[compIdx] = {};
					} else {
						currOff = mem::align(currOff, alig);
						ofs[compIdx] = (ChunkDataOffset)currOff;

						// Make sure the following component list is properly aligned
						currOff += comp.size() * count;
					}
				}
			}

			//! Returns the value stored in the component \tparam T on \param row in the chunk.
			//! \warning It is expected the \param row is valid. Undefined behavior otherwise.
			//! \warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			//! \tparam T Component
			//! \param row Row of entity in the chunk
			//! \return Value stored in the component if smaller than 8 bytes. Const reference to the value otherwise.
			template <typename T>
			GAIA_NODISCARD decltype(auto) comp_inter(const ChunkHeader& header, uint16_t row) const {
				using U = typename actual_type_t<T>::Type;
				using RetValueType = decltype(view<T>(header)[0]);

				GAIA_ASSERT(row < header.count);
				if constexpr (mem::is_soa_layout_v<U>)
					return view<T>(header, 0, header.capacity)[row];
				else if constexpr (sizeof(RetValueType) <= 8)
					return view<T>(header)[row];
				else
					return (const U&)view<T>(header)[row];
			}

			//! Returns a read-only span of the component data.
			//! \warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			//! \tparam T Component
			//! \return Span of read-only component data.
			template <typename T>
			GAIA_NODISCARD GAIA_FORCEINLINE auto view_inter(const ChunkHeader& header, uint32_t from, uint32_t to) const //
					-> decltype(std::span<const uint8_t>{}) {

				if constexpr (std::is_same_v<core::raw_t<T>, Entity>) {
					GAIA_ASSERT(to <= header.count);
					return {(const uint8_t*)&header.pEntities[from], to - from};
				} else if constexpr (is_pair<T>::value) {
					using TT = typename T::type;
					using U = typename component_type_t<TT>::Type;
					static_assert(!std::is_empty_v<U>, "Attempting to get value of an empty component");

					constexpr auto kind = entity_kind_v<TT>;
					const auto rel = header.cc->get<typename T::rel>().entity;
					const auto tgt = header.cc->get<typename T::tgt>().entity;
					const auto compIdx = comp_idx((Entity)Pair(rel, tgt));

					if constexpr (mem::is_soa_layout_v<U>) {
						GAIA_ASSERT(from == 0);
						GAIA_ASSERT(to == header.capacity);
						return {comp_ptr(header, compIdx), to};
					} else if constexpr (kind == EntityKind::EK_Gen) {
						GAIA_ASSERT(to <= header.count);
						return {comp_ptr(header, compIdx, from), to - from};
					} else {
						GAIA_ASSERT(to <= header.count);
						// GAIA_ASSERT(count == 1); we don't really care and always consider 1 for unique components
						return {comp_ptr(header, compIdx), 1};
					}
				} else {
					using U = typename component_type_t<T>::Type;
					static_assert(!std::is_empty_v<U>, "Attempting to get value of an empty component");

					constexpr auto kind = entity_kind_v<T>;
					const auto comp = header.cc->get<T>().entity;
					GAIA_ASSERT(comp.kind() == kind);
					const auto compIdx = comp_idx(comp);

					if constexpr (mem::is_soa_layout_v<U>) {
						GAIA_ASSERT(from == 0);
						GAIA_ASSERT(to == header.capacity);
						return {comp_ptr(header, compIdx), to};
					} else if constexpr (kind == EntityKind::EK_Gen) {
						GAIA_ASSERT(to <= header.count);
						return {comp_ptr(header, compIdx, from), to - from};
					} else {
						GAIA_ASSERT(to <= header.count);
						// GAIA_ASSERT(count == 1); we don't really care and always consider 1 for unique components
						return {comp_ptr(header, compIdx), 1};
					}
				}
			}

			//! Returns a read-write span of the component data. Also updates the world version for the component.
			//! \warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			//! \tparam T Component
			//! \tparam WorldVersionUpdateWanted If true, the world version is updated as a result of the write access
			//! \return Span of read-write component data.
			template <typename T, bool WorldVersionUpdateWanted>
			GAIA_NODISCARD GAIA_FORCEINLINE auto view_mut_inter(const ChunkHeader& header, uint32_t from, uint32_t to) //
					-> decltype(std::span<uint8_t>{}) {
				static_assert(!std::is_same_v<core::raw_t<T>, Entity>, "view_mut can't be used to modify Entity");

				if constexpr (is_pair<T>::value) {
					using TT = typename T::type;
					using U = typename component_type_t<TT>::Type;
					static_assert(!std::is_empty_v<U>, "view_mut can't be used to modify tag components");

					constexpr auto kind = entity_kind_v<TT>;
					const auto rel = header.cc->get<typename T::rel>().entity;
					const auto tgt = header.cc->get<typename T::tgt>().entity;
					const auto compIdx = comp_idx((Entity)Pair(rel, tgt));

					// Update version number if necessary so we know RW access was used on the chunk
					if constexpr (WorldVersionUpdateWanted)
						update_world_version(header, compIdx);

					if constexpr (mem::is_soa_layout_v<U>) {
						GAIA_ASSERT(from == 0);
						GAIA_ASSERT(to == header.capacity);
						return {comp_ptr_mut(header, compIdx), to};
					} else if constexpr (kind == EntityKind::EK_Gen) {
						GAIA_ASSERT(to <= header.count);
						return {comp_ptr_mut(header, compIdx, from), to - from};
					} else {
						GAIA_ASSERT(to <= header.count);
						// GAIA_ASSERT(count == 1); we don't really care and always consider 1 for unique components
						return {comp_ptr_mut(header, compIdx), 1};
					}
				} else {
					using U = typename component_type_t<T>::Type;
					static_assert(!std::is_empty_v<U>, "view_mut can't be used to modify tag components");

					constexpr auto kind = entity_kind_v<T>;
					const auto comp = header.cc->get<T>().entity;
					GAIA_ASSERT(comp.kind() == kind);
					const auto compIdx = comp_idx(comp);

					// Update version number if necessary so we know RW access was used on the chunk
					if constexpr (WorldVersionUpdateWanted)
						update_world_version(header, compIdx);

					if constexpr (mem::is_soa_layout_v<U>) {
						GAIA_ASSERT(from == 0);
						GAIA_ASSERT(to == header.capacity);
						return {comp_ptr_mut(header, compIdx), to};
					} else if constexpr (kind == EntityKind::EK_Gen) {
						GAIA_ASSERT(to <= header.count);
						return {comp_ptr_mut(header, compIdx, from), to - from};
					} else {
						GAIA_ASSERT(to <= header.count);
						// GAIA_ASSERT(count == 1); we don't really care and always consider 1 for unique components
						return {comp_ptr_mut(header, compIdx), 1};
					}
				}
			}

		public:
			Archetype(Archetype&&) = delete;
			Archetype(const Archetype&) = delete;
			Archetype& operator=(Archetype&&) = delete;
			Archetype& operator=(const Archetype&) = delete;

			void list_idx(uint32_t idx) {
				m_listIdx = idx;
			}

			uint32_t list_idx() const {
				return m_listIdx;
			}

			GAIA_NODISCARD bool cmp_comps(const ArchetypeLookupChecker& other) const {
				return detail::cmp_comps(ids_view(), other.m_comps);
			}

			ArchetypeIdLookupKey::LookupHash id_hash() const {
				return m_archetypeIdHash;
			}

			//! Sets hashes for each component type and lookup.
			//! \param hashLookup Hash used for archetype lookup purposes
			void set_hashes(LookupHash hashLookup) {
				m_hashLookup = hashLookup;
			}

			//! Defragments the chunk.
			//! \param maxEntities Maximum number of entities moved per call
			//! \param chunksToDelete Container of chunks ready for removal
			//! \param entities Container with entities
			void defrag(uint32_t& maxEntities, cnt::darray<ArchetypeChunkPair>& chunksToDelete, EntityContainers& recs) {
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
				// TODO: Implement mask of semi-full chunks so we can pick one easily when searching
				//       for a chunk to fill with a new entity and when defragmenting.
				// NOTE 1:
				// Even though entity movement might be present during defragmentation, we do
				// not update the world version here because no real structural changes happen.
				// All entities and components remain intact, they just move to a different place.
				// NOTE 2:
				// Entities belonging to chunks with uni components are locked to their chunk.
				// Therefore, we won't defragment them unless their uni components contain matching
				// values.

				if (maxEntities == 0)
					return;
				if (m_chunks.size() < 2)
					return;

				uint32_t front = 0;
				uint32_t back = m_chunks.size() - 1;

				auto* pDstChunk = m_chunks[front];
				auto* pSrcChunk = m_chunks[back];

				// Find the first semi-full chunk in the front
				while (front < back && (pDstChunk->full() || !pDstChunk->is_semi()))
					pDstChunk = m_chunks[++front];
				// Find the last semi-full chunk in the back
				while (front < back && (pSrcChunk->empty() || !pSrcChunk->is_semi()))
					pSrcChunk = m_chunks[--back];

				const bool hasUniEnts =
						m_properties.cntEntities > 0 && m_ids[m_properties.cntEntities - 1].kind() == EntityKind::EK_Uni;

				// Find the first semi-empty chunk in the back
				while (front < back) {
					pDstChunk = m_chunks[front];
					pSrcChunk = m_chunks[back];

					const uint32_t entitiesInSrcChunk = pSrcChunk->size();
					const uint32_t spaceInDstChunk = pDstChunk->m_header.capacity - pDstChunk->size();
					const uint32_t entitiesToMoveSrc = core::get_min(entitiesInSrcChunk, maxEntities);
					const uint32_t entitiesToMove = core::get_min(entitiesToMoveSrc, spaceInDstChunk);

					// Make sure uni components have matching values
					if (hasUniEnts) {
						auto rec = pSrcChunk->comp_rec_view();
						bool res = true;
						GAIA_FOR2(m_properties.genEntities, m_properties.cntEntities) {
							const auto* pSrcVal = (const void*)comp_ptr(pSrcChunk->m_header, i, 0);
							const auto* pDstVal = (const void*)comp_ptr(pDstChunk->m_header, i, 0);
							if (m_pItems[i]->cmp(pSrcVal, pDstVal)) {
								res = false;
								break;
							}
						}

						// When there is not a match we move to the next chunk
						if (!res) {
							pDstChunk = m_chunks[++front];
							goto next_iteration;
						}
					}

					GAIA_FOR(entitiesToMove) {
						const auto lastSrcEntityIdx = entitiesInSrcChunk - i - 1;
						const auto entity = pSrcChunk->entity_view()[lastSrcEntityIdx];

						auto& ec = recs[entity];

						const auto oldRow = ec.row;
						const auto newRow = pDstChunk->add_entity(entity);
						const bool wasEnabled = !ec.dis;

						// Make sure the old entity becomes enabled now
						enable_entity(*pSrcChunk, recs, oldRow, true);
						// We go back-to-front in the chunk so enabling the entity is not expected to change its row
						GAIA_ASSERT(oldRow == ec.row);

						// Move data from the old chunk to the new one
						Archetype::move_entity_data(recs, entity, pDstChunk, newRow);

						// Remove the entity record from the old chunk
						remove_entity(*pSrcChunk, oldRow, recs, chunksToDelete);

						// Bring the entity container record up-to-date
						ec.pChunk = pDstChunk;
						ec.row = (uint16_t)newRow;

						// Transfer the original enabled state to the new chunk
						enable_entity(*pDstChunk, recs, newRow, wasEnabled);
					}

					maxEntities -= entitiesToMove;
					if (maxEntities == 0)
						return;

					// The source is empty, find another semi-empty source
					if (pSrcChunk->empty()) {
						while (front < back && !m_chunks[--back]->is_semi())
							;
					}

				next_iteration:
					// The destination chunk is full, we need to move to the next one.
					// The idea is to fill the destination as much as possible.
					while (front < back && pDstChunk->full())
						pDstChunk = m_chunks[++front];
				}
			}

			//! Tries to locate a chunk that has some space left for a new entity.
			//! If not found a new chunk is created.
			//! \warning Always used in tandem with try_update_free_chunk_idx() or remove_entity()
			GAIA_NODISCARD Chunk* foc_free_chunk() {
				const auto chunkCnt = m_chunks.size();

				if (chunkCnt > 0) {
					for (uint32_t i = m_firstFreeChunkIdx; i < m_chunks.size(); ++i) {
						auto* pChunk = m_chunks[i];
						GAIA_ASSERT(pChunk != nullptr);
						const auto entityCnt = pChunk->size();
						if (entityCnt < pChunk->m_header.capacity) {
							m_firstFreeChunkIdx = i;
							return pChunk;
						}
					}
				}

				// Make sure not too many chunks are allocated
				GAIA_ASSERT(chunkCnt < UINT32_MAX);

				// No free space found anywhere. Let's create a new chunk.
				auto* pChunk = Archetype::create_chunk(
						m_cc, chunkCnt, //
						m_properties.capacity, m_properties.cntEntities, //
						m_properties.genEntities, m_properties.chunkDataBytes, //
						m_worldVersion, m_dataOffsets, m_ids, m_comps, m_compOffs);

				m_firstFreeChunkIdx = m_chunks.size();
				m_chunks.push_back(pChunk);
				return pChunk;
			}

			//! Tries to update the index of the first chunk that has space left
			//! for at least one entity.
			//! \warning Always use in tandem with foc_free_chunk()
			void try_update_free_chunk_idx() {
				// This is expected to be called only if there are any chunks
				GAIA_ASSERT(!m_chunks.empty());

				auto* pChunk = m_chunks[m_firstFreeChunkIdx];
				if (pChunk->size() >= pChunk->m_header.capacity)
					++m_firstFreeChunkIdx;
			}

			//! Tries to update the index of the first chunk that has space left
			//! for at least one entity.
			//! \warning Always use in tandem with foc_free_chunk() and remove_entity()
			void try_update_free_chunk_idx(Chunk& chunkThatRemovedEntity) {
				// This is expected to be called only if there are any chunks
				GAIA_ASSERT(!m_chunks.empty());

				if (chunkThatRemovedEntity.idx() == m_firstFreeChunkIdx)
					return;

				if (chunkThatRemovedEntity.idx() < m_firstFreeChunkIdx) {
					m_firstFreeChunkIdx = chunkThatRemovedEntity.idx();
					return;
				}

				auto* pChunk = m_chunks[m_firstFreeChunkIdx];
				if (pChunk->size() >= pChunk->m_header.capacity)
					++m_firstFreeChunkIdx;
			}

			GAIA_NODISCARD const Properties& props() const {
				return m_properties;
			}

			GAIA_NODISCARD const cnt::darray<Chunk*>& chunks() const {
				return m_chunks;
			}

			GAIA_NODISCARD LookupHash lookup_hash() const {
				return m_hashLookup;
			}

			GAIA_NODISCARD EntitySpan ids_view() const {
				return {&m_ids[0], m_properties.cntEntities};
			}

			GAIA_NODISCARD ComponentSpan comps_view() const {
				return {&m_comps[0], m_properties.cntEntities};
			}

			GAIA_NODISCARD ChunkDataOffsetSpan comp_offs_view() const {
				return {&m_compOffs[0], m_properties.cntEntities};
			}

			GAIA_NODISCARD std::span<const ComponentCacheItem*> items() const {
				return {(const ComponentCacheItem**)m_pItems, m_properties.cntEntities};
			}

			GAIA_NODISCARD uint32_t pairs() const {
				return m_pairCnt;
			}

			GAIA_NODISCARD uint32_t pairs_is() const {
				return m_pairCnt_is;
			}

			GAIA_NODISCARD Entity entity_from_pairs_as_idx(uint32_t idx) const {
				const auto ids_idx = m_pairs_as_index_buffer[idx];
				return m_ids[ids_idx];
			}

			//----------------------------------------------------------------------
			// Archetype lifetime
			//----------------------------------------------------------------------

			GAIA_NODISCARD static Archetype*
			create(const World& world, ArchetypeId archetypeId, uint32_t& worldVersion, EntitySpan ids) {
				const auto& cc = comp_cache(world);

				auto* newArch = mem::AllocHelper::alloc<Archetype>("Archetype");
				(void)new (newArch) Archetype(cc, worldVersion);

				newArch->m_archetypeId = archetypeId;
				newArch->m_archetypeIdHash = ArchetypeIdLookupKey::calc(archetypeId);
				const uint32_t maxEntities = archetypeId == 0 ? ChunkHeader::MAX_CHUNK_ENTITIES : 512;

				const auto cnt = (uint32_t)ids.size();
				newArch->m_properties.cntEntities = (uint8_t)ids.size();

				auto as_comp = [&](Entity entity) {
					const auto* pDesc = cc.find(entity);
					return pDesc == nullptr //
										 ? Component(IdentifierIdBad, 0, 0, 0) //
										 : pDesc->comp;
				};

				// Prepare m_comps array
				auto comps = std::span(&newArch->m_comps[0], cnt);
				GAIA_EACH(ids) {
					if (ids[i].pair()) {
						// When using pairs we need to decode the storage type from them.
						// This is what pair<Rel, Tgt>::type actually does to determine what type to use at compile-time.
						const Entity pairEntities[] = {entity_from_id(world, ids[i].id()), entity_from_id(world, ids[i].gen())};
						const uint32_t idx = (!pairEntities[0].tag() || pairEntities[1].tag()) ? 0 : 1;
#if !GAIA_DISABLE_ASSERTS
						Component pairComponents[] = {as_comp(pairEntities[0]), as_comp(pairEntities[1])};
						const uint32_t idx_check = (pairComponents[0].size() != 0U || pairComponents[1].size() == 0U) ? 0 : 1;
						GAIA_ASSERT(idx_check == idx);
#endif
						comps[i] = as_comp(pairEntities[idx]);
					} else {
						comps[i] = as_comp(ids[i]);
					}
				}

				// Calculate offsets
				static auto ChunkDataAreaOffset = Chunk::chunk_data_area_offset();
				newArch->update_data_offsets(
						// This is not a real memory address.
						// Chunk memory is organized as header+data. The offsets we calculate here belong to
						// the data area.
						// Every allocated chunk is going to have the same relative offset from the header part
						// which is why providing a fictional relative offset is enough.
						ChunkDataAreaOffset);
				const auto& offs = newArch->m_dataOffsets;

				// Calculate the number of pairs
				GAIA_EACH(ids) {
					if (!ids[i].pair())
						continue;

					++newArch->m_pairCnt;

					// If it is an Is relationship, count it separately
					if (ids[i].id() == Is.id())
						newArch->m_pairs_as_index_buffer[newArch->m_pairCnt_is++] = (uint8_t)i;
				}

				// Find the index of the last generic component in both arrays
				uint32_t entsGeneric = (uint32_t)ids.size();
				if (!ids.empty()) {
					for (int i = (int)ids.size() - 1; i >= 0; --i) {
						if (ids[(uint32_t)i].kind() != EntityKind::EK_Uni)
							break;
						--entsGeneric;
					}
				}

				// Calculate the number of entities per chunks precisely so we can
				// fit as many of them into chunk as possible.

				uint32_t genCompsSize = 0;
				uint32_t uniCompsSize = 0;
				GAIA_FOR(entsGeneric) genCompsSize += newArch->m_comps[i].size();
				GAIA_FOR2(entsGeneric, cnt) uniCompsSize += newArch->m_comps[i].size();

				const uint32_t size0 = Chunk::chunk_data_bytes(mem_block_size(0));
				const uint32_t size1 = Chunk::chunk_data_bytes(mem_block_size(1));
				const auto sizeM = (size0 + size1) / 2;

				uint32_t maxDataOffsetTarget = size1;
				// Theoretical maximum number of components we can fit into one chunk.
				// This can be further reduced due alignment and padding.
				auto maxGenItemsInArchetype = (maxDataOffsetTarget - offs.firstByte_EntityData - uniCompsSize - 1) /
																			(genCompsSize + (uint32_t)sizeof(Entity));

				bool finalCheck = false;
			recalculate:
				auto currOff = offs.firstByte_EntityData + (uint32_t)sizeof(Entity) * maxGenItemsInArchetype;

				// Adjust the maximum number of entities. Recalculation happens at most once when the original guess
				// for entity count is not right (most likely because of padding or usage of SoA components).
				if (!est_max_entities_per_archetype(
								cc, currOff, maxGenItemsInArchetype, comps.subspan(0, entsGeneric), maxGenItemsInArchetype,
								maxDataOffsetTarget))
					goto recalculate;
				if (!est_max_entities_per_archetype(
								cc, currOff, maxGenItemsInArchetype, comps.subspan(entsGeneric), 1, maxDataOffsetTarget))
					goto recalculate;

				// Limit the number of entities to a certain number so we can make use of smaller
				// chunks where it makes sense.
				// TODO: Tweak this so the full remaining capacity is used. So if we occupy 7000 B we still
				//       have 1000 B left to fill.
				if (maxGenItemsInArchetype > maxEntities) {
					maxGenItemsInArchetype = maxEntities;
					goto recalculate;
				}

				// We create chunks of either 8K or 16K but might end up with requested capacity 8.1K. Allocating a 16K chunk
				// in this case would be wasteful. Therefore, let's find the middle ground. Anything 12K or smaller we'll
				// allocate into 8K chunks so we avoid wasting too much memory.
				if (!finalCheck && currOff < sizeM) {
					finalCheck = true;
					maxDataOffsetTarget = size0;

					maxGenItemsInArchetype = (maxDataOffsetTarget - offs.firstByte_EntityData - uniCompsSize - 1) /
																	 (genCompsSize + (uint32_t)sizeof(Entity));
					goto recalculate;
				}

				// Update the offsets according to the recalculated maxGenItemsInArchetype
				currOff = offs.firstByte_EntityData + (uint32_t)sizeof(Entity) * maxGenItemsInArchetype;
				reg_components(cc, *newArch, ids, comps, (uint8_t)0, (uint8_t)entsGeneric, currOff, maxGenItemsInArchetype);
				reg_components(cc, *newArch, ids, comps, (uint8_t)entsGeneric, (uint8_t)ids.size(), currOff, 1);

				GAIA_ASSERT(Chunk::chunk_total_bytes((ChunkDataOffset)currOff) < mem_block_size(currOff));
				newArch->m_properties.capacity = (uint16_t)maxGenItemsInArchetype;
				newArch->m_properties.chunkDataBytes = (ChunkDataOffset)currOff;
				newArch->m_properties.genEntities = (uint8_t)entsGeneric;

				return newArch;
			}

			void static destroy(Archetype* pArchetype) {
				GAIA_ASSERT(pArchetype != nullptr);
				pArchetype->~Archetype();
				mem::AllocHelper::free("Archetype", pArchetype);
			}

			//----------------------------------------------------------------------
			// Chunk lifetime
			//----------------------------------------------------------------------

			//! Allocates memory for a new chunk.
			//! \param chunkIndex Index of this chunk within the parent archetype
			//! \return Newly allocated chunk
			Chunk* create_chunk(
					const ComponentCache& cc, uint32_t chunkIndex, uint16_t capacity, uint8_t cntEntities, uint8_t genEntities,
					uint16_t dataBytes, uint32_t& worldVersion,
					// data offsets
					const ChunkDataOffsets& offsets,
					// component entities
					const Entity* ids,
					// component
					const Component* comps,
					// component offsets
					const ChunkDataOffset* compOffs) {
				auto* pChunk = Chunk::create(
						cc, chunkIndex, capacity, cntEntities, genEntities, dataBytes, worldVersion, offsets, ids, comps, compOffs);

				// Construct unique components
				GAIA_FOR2(0, cntEntities) {
					const auto* pItem = m_pItems[i];
					if (pItem == nullptr)
						continue;

					// We construct unique components right away if possible
					call_ctor(pChunk->m_header, 0, *pItem);
				}

				return pChunk;
			}

			//! Releases all memory allocated by \param pChunk.
			//! \param pChunk Chunk which we want to destroy
			void free_chunk(Chunk* pChunk) {
				GAIA_ASSERT(pChunk != nullptr);
				GAIA_ASSERT(!pChunk->dead());

				// Mark as dead
				pChunk->die();

				// Call destructors for components that need it
				call_all_dtors(pChunk->m_header);

				Chunk::free(pChunk);
			}

			//! Removes a chunk from the list of chunks managed by their archetype and deletes its memory.
			//! \param pChunk Chunk to remove from the list of managed archetypes
			void del(Chunk* pChunk, ArchetypeDArray& archetypesToDelete) {
				// Make sure there are any chunks to delete
				GAIA_ASSERT(!m_chunks.empty());

				const auto chunkIndex = pChunk->idx();

				// Make sure the chunk is a part of the chunk array
				GAIA_ASSERT(chunkIndex == core::get_index(m_chunks, pChunk));

				// Remove the chunk from the chunk array. We are swapping this chunk's entry
				// with the last one in the array. Therefore, we first update the last item's
				// index with the current chunk's index and then do the swapping.
				m_chunks.back()->set_idx(chunkIndex);
				core::erase_fast(m_chunks, chunkIndex);

				// Delete the chunk now. Otherwise, if the chunk happened to be the last
				// one we would end up overriding released memory.
				Chunk::free(pChunk);

				// TODO: This needs cleaning up.
				//       Chunk should have no idea of the world and also should not store
				//       any states related to its lifespan.
				if (m_lifespanCountdownMax > 0 && !dying() && empty()) {
					// When the chunk is emptied we want it to be removed. We can't do it
					// right away and need to wait for world::gc() to be called.
					//
					// However, we need to prevent the following:
					//    1) chunk is emptied, add it to some removal list
					//    2) chunk is reclaimed
					//    3) chunk is emptied, add it to some removal list again
					//
					// Therefore, we have a flag telling us the chunk is already waiting to
					// be removed. The chunk might be reclaimed before garbage collection happens
					// but it simply ignores such requests. This way we always have at most one
					// record for removal for any given chunk.
					start_dying();

					archetypesToDelete.push_back(this);
				}
			}

			//----------------------------------------------------------------------
			// Data viewing
			//----------------------------------------------------------------------

			GAIA_NODISCARD static uint8_t* comp_ptr_mut(const ChunkHeader& header, uint32_t compIdx) {
				const auto& rec = header.recs[compIdx];
				return rec.pData;
			}

			GAIA_NODISCARD static uint8_t* comp_ptr_mut(const ChunkHeader& header, uint32_t compIdx, uint32_t offset) {
				const auto& rec = header.recs[compIdx];
				return rec.pData + (uintptr_t)rec.comp.size() * offset;
			}

			GAIA_NODISCARD static const uint8_t* comp_ptr(const ChunkHeader& header, uint32_t compIdx) {
				const auto& rec = header.recs[compIdx];
				return rec.pData;
			}

			GAIA_NODISCARD static const uint8_t* comp_ptr(const ChunkHeader& header, uint32_t compIdx, uint32_t offset) {
				const auto& rec = header.recs[compIdx];
				return rec.pData + (uintptr_t)rec.comp.size() * offset;
			}

			//! Returns a read-only entity or component view.
			//! \warning If \tparam T is a component it is expected it is present. Undefined behavior otherwise.
			//! \tparam T Component or Entity
			//! \param from First valid entity row
			//! \param to Last valid entity row
			//! \return Entity of component view with read-only access
			template <typename T>
			GAIA_NODISCARD decltype(auto) view(const ChunkHeader& header, uint16_t from, uint16_t to) const {
				using U = typename actual_type_t<T>::Type;

				// Always consider full range for SoA
				if constexpr (mem::is_soa_layout_v<U>)
					return mem::auto_view_policy_get<U>{view_inter<T>(header, 0, header.capacity)};
				else
					return mem::auto_view_policy_get<U>{view_inter<T>(header, from, to)};
			}

			template <typename T>
			GAIA_NODISCARD decltype(auto) view(const ChunkHeader& header) const {
				return view<T>(header, 0, header.count);
			}

			template <typename T>
			GAIA_NODISCARD decltype(auto) view_raw(const void* ptr, uint32_t size) const {
				using U = typename actual_type_t<T>::Type;
				return mem::auto_view_policy_get<U>{std::span{(const uint8_t*)ptr, size}};
			}

			//! Returns a mutable entity or component view.
			//! \warning If \tparam T is a component it is expected it is present. Undefined behavior otherwise.
			//! \tparam T Component or Entity
			//! \param from First valid entity row
			//! \param to Last valid entity row
			//! \return Entity or component view with read-write access
			template <typename T>
			GAIA_NODISCARD decltype(auto) view_mut(const ChunkHeader& header, uint16_t from, uint16_t to) {
				using U = typename actual_type_t<T>::Type;
				static_assert(!std::is_same_v<U, Entity>, "Modifying chunk entities via view_mut is forbidden");

				// Always consider full range for SoA
				if constexpr (mem::is_soa_layout_v<U>) {
					(void)to;
					return mem::auto_view_policy_set<U>{view_mut_inter<T, true>(header, 0, header.capacity)};
				} else
					return mem::auto_view_policy_set<U>{view_mut_inter<T, true>(header, from, to)};
			}

			template <typename T>
			GAIA_NODISCARD decltype(auto) view_mut(const ChunkHeader& header) {
				return view_mut<T>(header, 0, header.count);
			}

			template <typename T>
			GAIA_NODISCARD decltype(auto) view_mut_raw(void* ptr, uint32_t size) const {
				using U = typename actual_type_t<T>::Type;
				static_assert(!std::is_same_v<U, Entity>, "Modifying chunk entities via view_mut is forbidden");

				return mem::auto_view_policy_set<U>{std::span{(uint8_t*)ptr, size}};
			}

			//! Returns a mutable component view.
			//! Doesn't update the world version when the access is acquired.
			//! \warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			//! \tparam T Component
			//! \param from First valid entity row
			//! \param to Last valid entity row
			//! \return Component view with read-write access
			template <typename T>
			GAIA_NODISCARD decltype(auto) sview_mut(const ChunkHeader& header, uint16_t from, uint16_t to) {
				using U = typename actual_type_t<T>::Type;
				static_assert(!std::is_same_v<U, Entity>, "Modifying chunk entities via view_mut is forbidden");

				// Always consider full range for SoA
				if constexpr (mem::is_soa_layout_v<U>) {
					(void)to;
					return mem::auto_view_policy_set<U>{view_mut_inter<T, false>(header, 0, header.capacity)};
				} else
					return mem::auto_view_policy_set<U>{view_mut_inter<T, false>(header, from, to)};
			}

			template <typename T>
			GAIA_NODISCARD decltype(auto) sview_mut_raw(void* ptr, uint32_t size) const {
				using U = typename actual_type_t<T>::Type;
				static_assert(!std::is_same_v<U, Entity>, "Modifying chunk entities via view_mut is forbidden");

				return mem::auto_view_policy_set<U>{std::span{(uint8_t*)ptr, size}};
			}

			template <typename T>
			GAIA_NODISCARD decltype(auto) sview_mut(const ChunkHeader& header) {
				return sview_mut<T>(header, 0, header.count);
			}

			//! Returns either a mutable or immutable entity/component view based on the requested type.
			//! Value and const types are considered immutable. Anything else is mutable.
			//! \warning If \tparam T is a component it is expected to be present. Undefined behavior otherwise.
			//! \tparam T Component or Entity
			//! \param from First valid entity row
			//! \param to Last valid entity row
			//! \return Entity or component view
			template <typename T>
			GAIA_NODISCARD decltype(auto) view_auto(const ChunkHeader& header, uint16_t from, uint16_t to) {
				using UOriginal = typename actual_type_t<T>::TypeOriginal;
				if constexpr (core::is_mut_v<UOriginal>)
					return view_mut<T>(header, from, to);
				else
					return view<T>(header, from, to);
			}

			template <typename T>
			GAIA_NODISCARD decltype(auto) view_auto(const ChunkHeader& header) {
				return view_auto<T>(header, 0, header.count);
			}

			//! Returns either a mutable or immutable entity/component view based on the requested type.
			//! Value and const types are considered immutable. Anything else is mutable.
			//! Doesn't update the world version when read-write access is acquired.
			//! \warning If \tparam T is a component it is expected to be present. Undefined behavior otherwise.
			//! \tparam T Component or Entity
			//! \param from First valid entity row
			//! \param to Last valid entity row
			//! \return Entity or component view
			template <typename T>
			GAIA_NODISCARD decltype(auto) sview_auto(uint16_t from, uint16_t to) {
				using UOriginal = typename actual_type_t<T>::TypeOriginal;
				if constexpr (core::is_mut_v<UOriginal>)
					return sview_mut<T>(from, to);
				else
					return view<T>(from, to);
			}

			template <typename T>
			GAIA_NODISCARD decltype(auto) sview_auto(const ChunkHeader& header) {
				return sview_auto<T>(0, header.count);
			}

			//----------------------------------------------------------------------
			// Check component presence
			//----------------------------------------------------------------------

			//! Checks if an entity is a part of the archetype.
			//! \param entity Entity
			//! \return True if found. False otherwise.
			GAIA_NODISCARD bool has(Entity entity) const {
				return core::has_if(ids_view(), [&](Entity e) {
					return e == entity;
				});
			}

			//! Checks if component \tparam T is present in the chunk.
			//! \tparam T Component or pair
			//! \return True if the component is present. False otherwise.
			template <typename T>
			GAIA_NODISCARD bool has() const {
				if constexpr (is_pair<T>::value) {
					const auto rel = m_cc.get<typename T::rel>().entity;
					const auto tgt = m_cc.get<typename T::tgt>().entity;
					return has((Entity)Pair(rel, tgt));
				} else {
					const auto* pComp = m_cc.find<T>();
					return pComp != nullptr && has(pComp->entity);
				}
			}

			//----------------------------------------------------------------------
			// Version detection
			//----------------------------------------------------------------------

			//! Returns true if the provided version is newer than the one stored internally
			GAIA_NODISCARD bool changed(const ChunkHeader& header, uint32_t version, uint32_t compIdx) const {
				auto versions = Chunk::comp_version_view(header);
				return version_changed(versions[compIdx], version);
			}

			//! Update the version of a component at the index \param compIdx
			GAIA_FORCEINLINE void update_world_version(const ChunkHeader& header, uint32_t compIdx) {
				auto versions = Chunk::comp_version_view_mut(header);
				versions[compIdx] = header.worldVersion;
			}

			//! Update the version of all components
			GAIA_FORCEINLINE void update_world_version(const ChunkHeader& header) {
				auto versions = Chunk::comp_version_view_mut(header);
				for (auto& v: versions)
					v = header.worldVersion;
			}

			//----------------------------------------------------------------------
			// Set component data
			//----------------------------------------------------------------------

			//! Sets the value of the unique component \tparam T on \param row in the chunk.
			//! \tparam T Component or pair
			//! \param row Row of entity in the chunk
			//! \param value Value to set for the component
			//! \warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			template <typename T>
			decltype(auto) set(const ChunkHeader& header, uint16_t row) {
				verify_comp<T>();

				GAIA_ASSERT2(
						actual_type_t<T>::Kind == EntityKind::EK_Gen || row == 0,
						"Set providing a row can only be used with generic components");

				// Update the world version
				update_version(header.worldVersion);

				GAIA_ASSERT(row < header.capacity);
				return view_mut<T>(header)[row];
			}

			//! Sets the value of a generic entity \param type at the position \param row in the chunk.
			//! \param row Row of entity in the chunk
			//! \param type Component/entity/pair
			//! \param value New component value
			//! \warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			template <typename T>
			decltype(auto) set(const ChunkHeader& header, uint16_t row, Entity type) {
				verify_comp<T>();

				GAIA_ASSERT2(
						type.kind() == EntityKind::EK_Gen || row == 0,
						"Set providing a row can only be used with generic components");
				GAIA_ASSERT(type.kind() == entity_kind_v<T>);

				// Update the world version
				update_version(header.worldVersion);

				GAIA_ASSERT(row < header.capacity);

				// TODO: This function works but is useless because it does the same job as
				//       set(uint16_t row, U&& value).
				//       This is because T needs to match U anyway so the component lookup can succeed.
				(void)type;
				// const uint32_t col = comp_idx(type);
				//(void)col;

				return view_mut<T>(header)[row];
			}

			//! Sets the value of the unique component \tparam T on \param row in the chunk.
			//! \tparam T Component or pair
			//! \param row Row of entity in the chunk
			//! \param value Value to set for the component
			//! \warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			//! \warning World version is not updated so Query filters will not be able to catch this change.
			template <typename T>
			decltype(auto) sset(const ChunkHeader& header, uint16_t row) {
				GAIA_ASSERT2(
						actual_type_t<T>::Kind == EntityKind::EK_Gen || row == 0,
						"Set providing a row can only be used with generic components");

				GAIA_ASSERT(row < header.capacity);
				return view_mut<T>(header)[row];
			}

			//! Sets the value of a generic entity \param type at the position \param row in the chunk.
			//! \param row Row of entity in the chunk
			//! \param type Component/entity/pair
			//! \param value New component value
			//! \warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			//! \warning World version is not updated so Query filters will not be able to catch this change.
			template <typename T>
			decltype(auto) sset(const ChunkHeader& header, uint16_t row, Entity type) {
				static_assert(core::is_raw_v<T>);

				GAIA_ASSERT2(
						type.kind() == EntityKind::EK_Gen || row == 0,
						"Set providing a row can only be used with generic components");

				GAIA_ASSERT(row < header.capacity);

				// TODO: This function works but is useless because it does the same job as
				//       sset(uint16_t row, U&& value).
				//       This is because T needs to match U anyway so the component lookup can succeed.
				(void)type;
				// const uint32_t col = comp_idx(type);
				//(void)col;

				return view_mut<T>(header)[row];
			}

			//----------------------------------------------------------------------
			// Read component data
			//----------------------------------------------------------------------

			//! Returns the value stored in the generic component \tparam T on \param row in the chunk.
			//! \warning It is expected the \param row is valid. Undefined behavior otherwise.
			//! \warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			//! \tparam T Component or pair
			//! \param row Row of entity in the chunk
			//! \return Value stored in the component.
			template <typename T>
			GAIA_NODISCARD decltype(auto) get(const ChunkHeader& header, uint16_t row) const {
				static_assert(
						entity_kind_v<T> == EntityKind::EK_Gen, "Get providing a row can only be used with generic components");

				return comp_inter<T>(header, row);
			}

			//! Returns the value stored in the unique component \tparam T.
			//! \warning It is expected the unique component \tparam T is present. Undefined behavior otherwise.
			//! \tparam T Component or pair
			//! \return Value stored in the component.
			template <typename T>
			GAIA_NODISCARD decltype(auto) get(const ChunkHeader& header) const {
				static_assert(
						entity_kind_v<T> != EntityKind::EK_Gen,
						"Get not providing a row can only be used with non-generic components");

				return comp_inter<T>(header, 0);
			}

			//! Returns the internal index of a component based on the provided \param entity.
			//! \param entity Component
			//! \return Component index if the component was found. -1 otherwise.
			GAIA_NODISCARD uint32_t comp_idx(Entity entity) const {
				return ecs::comp_idx<ChunkHeader::MAX_COMPONENTS>(m_ids, entity);
			}

			//----------------------------------------------------------------------
			// Component construction
			//----------------------------------------------------------------------

			void call_ctor(const ChunkHeader& header, uint32_t entIdx, const ComponentCacheItem& item) {
				if (item.func_ctor == nullptr)
					return;

				GAIA_PROF_SCOPE(Chunk::call_ctor);

				const auto compIdx = comp_idx(item.entity);
				auto* pSrc = (void*)comp_ptr_mut(header, compIdx, entIdx);
				item.func_ctor(pSrc, 1);
			}

			void call_gen_ctors(const ChunkHeader& header, uint32_t entIdx, uint32_t entCnt) {
				if (!m_properties.hasAnyCustomGenCtor)
					return;

				GAIA_PROF_SCOPE(Chunk::call_gen_ctors);

				GAIA_FOR2(0, header.genEntities) {
					const auto* pItem = m_pItems[i];
					if (pItem == nullptr || pItem->func_ctor == nullptr)
						continue;

					auto* pSrc = (void*)comp_ptr_mut(header, i, entIdx);
					pItem->func_ctor(pSrc, entCnt);
				}
			}

			void call_all_dtors(const ChunkHeader& header) {
				if (!m_properties.hasAnyCustomGenDtor && !m_properties.hasAnyCustomUniCtor)
					return;

				GAIA_PROF_SCOPE(Chunk::call_all_dtors);

				GAIA_FOR2(0, header.cntEntities) {
					const auto* pItem = m_pItems[i];
					if (pItem == nullptr || pItem->func_dtor == nullptr)
						continue;

					auto* pSrc = (void*)comp_ptr_mut(header, i, 0);
					const auto e = m_ids[i];
					const auto cnt = (e.kind() == EntityKind::EK_Gen) ? header.count : 1;
					pItem->func_dtor(pSrc, cnt);
				}
			};

			//----------------------------------------------------------------------
			// Data movement
			//----------------------------------------------------------------------

			//! Copies all data associated with \param oldEntity into \param newEntity.
			static void copy_entity_data(EntityContainers& recs, Entity oldEntity, Entity newEntity) {
				GAIA_PROF_SCOPE(Chunk::copy_entity_data);

				auto& oldEntityContainer = recs[oldEntity];
				auto* pOldArchetype = oldEntityContainer.pArchetype;
				auto* pOldChunk = oldEntityContainer.pChunk;

				auto& newEntityContainer = recs[newEntity];
				auto* pNewChunk = newEntityContainer.pChunk;

				GAIA_ASSERT(oldEntityContainer.pArchetype == newEntityContainer.pArchetype);

				// Copy generic component data from reference entity to our new entity.
				// Unique components do not change place in the chunk so there is no need to move them.
				const auto* pItems = pOldArchetype->m_pItems;
				GAIA_FOR(pOldChunk->m_header.genEntities) {
					const auto* pItem = pItems[i];
					if (pItem == nullptr || pItem->comp.size() == 0U)
						continue;

					const auto* pSrc = (const void*)comp_ptr_mut(pOldChunk->m_header, i);
					auto* pDst = (void*)comp_ptr_mut(pNewChunk->m_header, i);
					pItem->copy(
							pDst, pSrc, //
							newEntityContainer.row, oldEntityContainer.row, //
							pNewChunk->capacity(), pOldChunk->capacity());
				}
			}

			//! Moves all data associated with \param entity into the chunk so that it is stored at the row \param row.
			static void move_entity_data(EntityContainers& recs, Entity oldEntity, Chunk* pNewChunk, uint16_t newRow) {
				GAIA_PROF_SCOPE(Chunk::move_entity_data);

				auto& oldEntityContainer = recs[oldEntity];
				auto* pOldArchetype = oldEntityContainer.pArchetype;
				auto* pOldChunk = oldEntityContainer.pChunk;

				// Copy generic component data from reference entity to our new entity.
				// Unique components do not change place in the chunk so there is no need to move them.
				const auto* pItems = pOldArchetype->m_pItems;
				GAIA_FOR(pOldChunk->m_header.genEntities) {
					const auto* pItem = pItems[i];
					if (pItem == nullptr || pItem->comp.size() == 0U)
						continue;

					auto* pSrc = (void*)comp_ptr_mut(pOldChunk->m_header, i);
					auto* pDst = (void*)comp_ptr_mut(pNewChunk->m_header, i);
					pItem->ctor_from(
							pDst, pSrc, //
							newRow, oldEntityContainer.row, //
							pNewChunk->capacity(), pOldChunk->capacity());
				}
			}

			//! Moves all data associated with \param entity into the chunk so that it is stored at the row \param row.
			static void move_foreign_entity_data(
					Archetype& oldArchetype, Chunk& oldChunk, uint32_t oldRow, //
					Archetype& newArchetype, Chunk& newChunk, uint32_t newRow //
			) {
				GAIA_PROF_SCOPE(Chunk::move_foreign_entity_data);

				GAIA_ASSERT(oldRow < oldChunk.size());
				GAIA_ASSERT(newRow < newChunk.size());

				auto oldIds = oldArchetype.ids_view();
				auto newIds = newArchetype.ids_view();

				// Find intersection of the two component lists.
				// Arrays are sorted so we can do linear intersection lookup.
				// Call constructor on each match.
				// Unique components do not change place in the chunk so there is no need to move them.
				{
					uint32_t i = 0;
					uint32_t j = 0;
					while (i < oldChunk.m_header.genEntities && j < newChunk.m_header.genEntities) {
						const auto oldId = oldIds[i];
						const auto newId = newIds[j];

						if (oldId == newId) {
							const auto* pItem = newArchetype.m_pItems[j];
							if (pItem != nullptr && pItem->comp.size() != 0U) {
								auto* pSrc = (void*)comp_ptr_mut(oldChunk.m_header, i);
								auto* pDst = (void*)comp_ptr_mut(newChunk.m_header, j);
								pItem->ctor_from(pDst, pSrc, newRow, oldRow, newChunk.capacity(), oldChunk.capacity());
							}

							++i;
							++j;
						} else if (SortComponentCond{}.operator()(oldId, newId)) {
							++i;
						} else {
							// No match with the old chunk. Construct the component
							const auto* pItem = newArchetype.m_pItems[j];
							if (pItem != nullptr && pItem->func_ctor != nullptr) {
								auto* pDst = (void*)comp_ptr_mut(newChunk.m_header, j, newRow);
								pItem->func_ctor(pDst, 1);
							}

							++j;
						}
					}

					// Initialize the rest of the components if they are generic.
					for (; j < newChunk.m_header.genEntities; ++j) {
						const auto* pItem = newArchetype.m_pItems[j];
						if (pItem != nullptr && pItem->func_ctor != nullptr) {
							auto* pDst = (void*)comp_ptr_mut(newChunk.m_header, j, newRow);
							pItem->func_ctor(pDst, 1);
						}
					}
				}
			}

			//! Tries to remove the entity at \param row.
			//! Removal is done via swapping with last entity in chunk.
			//! Upon removal, all associated data is also removed.
			//! If the entity at the given row already is the last chunk entity, it is removed directly.
			void remove_entity_inter(Chunk& chunk, uint16_t row, EntityContainers& recs) {
				GAIA_PROF_SCOPE(Chunk::remove_entity_inter);

				const uint16_t rowA = row;
				const uint16_t rowB = chunk.m_header.count - 1;
				// The "rowA" entity is the one we are going to destroy so it needs to precede the "rowB"
				GAIA_ASSERT(rowA <= rowB);

				// To move anything, we need at least 2 entities
				if GAIA_LIKELY (rowA < rowB) {
					GAIA_ASSERT(chunk.m_header.count > 1);

					auto ev = chunk.entity_view_mut();

					// Update entity data
					const auto entityB = ev[rowB];
					auto& ecB = recs[entityB];
#if GAIA_ASSERT_ENABLED
					const auto entityA = ev[rowA];
					auto& ecA = recs[entityA];

					GAIA_ASSERT(ecA.pArchetype == ecB.pArchetype);
					GAIA_ASSERT(ecA.pChunk == ecB.pChunk);
#endif

					ev[rowA] = entityB;

					// Move component data from entityB to entityA
					GAIA_FOR(chunk.m_header.genEntities) {
						const auto* pItem = m_pItems[i];
						if (pItem == nullptr || pItem->comp.size() == 0U)
							continue;

						auto* pSrc = (void*)comp_ptr_mut(chunk.m_header, i);
						pItem->move(pSrc, pSrc, rowA, rowB, m_properties.capacity, m_properties.capacity);

						pSrc = (void*)comp_ptr_mut(chunk.m_header, i, rowB);
						pItem->dtor(pSrc);
					}

					// Entity has been replaced with the last one in our chunk. Update its container record.
					ecB.row = rowA;
				} else if (m_properties.hasAnyCustomGenDtor != 0) {
					// This is the last entity in the chunk so simply destroy its data
					GAIA_FOR(chunk.m_header.genEntities) {
						const auto* pItem = m_pItems[i];
						if (pItem == nullptr || pItem->comp.size() == 0U)
							continue;

						auto* pSrc = (void*)comp_ptr_mut(chunk.m_header, i, rowA);
						pItem->dtor(pSrc);
					}
				}
			}

			//! Tries to remove the entity at row \param row.
			//! Removal is done via swapping with last entity in chunk.
			//! Upon removal, all associated data is also removed.
			//! If the entity at the given row already is the last chunk entity, it is removed directly.
			void remove_entity(
					Chunk& chunk, uint16_t row, EntityContainers& recs, cnt::darray<ArchetypeChunkPair>& chunksToDelete) {
				GAIA_ASSERT(
						!chunk.locked() && "Entities can't be removed while their chunk is being iterated "
															 "(structural changes are forbidden during this time!)");

				if GAIA_UNLIKELY (chunk.m_header.count == 0)
					return;

				GAIA_PROF_SCOPE(Chunk::remove_entity);

				if (chunk.enabled(row)) {
					// Entity was previously enabled. Swap with the last entity
					remove_entity_inter(chunk, row, recs);
					// If this was the first enabled entity make sure to update the row
					if (chunk.m_header.rowFirstEnabledEntity > 0 && row == chunk.m_header.rowFirstEnabledEntity)
						--chunk.m_header.rowFirstEnabledEntity;
				} else {
					// Entity was previously disabled. Swap with the last disabled entity
					const uint16_t pivot = chunk.size_disabled() - 1;
					swap_chunk_entities(chunk, recs, row, pivot);
					// Once swapped, try to swap with the last (enabled) entity in the chunk.
					remove_entity_inter(chunk, pivot, recs);
					--chunk.m_header.rowFirstEnabledEntity;
				}

				// Make sure update the world version
				chunk.update_versions();

				// At this point the last entity is no longer valid so remove it
				chunk.remove_last_entity();

				// TODO: This needs cleaning up.
				//       Chunk should have no idea of the world and also should not store
				//       any states related to its lifespan.
				if (!chunk.dying() && chunk.empty()) {
					// When the chunk is emptied we want it to be removed. We can't do it
					// rowB away and need to wait for world::gc() to be called.
					//
					// However, we need to prevent the following:
					//    1) chunk is emptied, add it to some removal list
					//    2) chunk is reclaimed
					//    3) chunk is emptied, add it to some removal list again
					//
					// Therefore, we have a flag telling us the chunk is already waiting to
					// be removed. The chunk might be reclaimed before garbage collection happens
					// but it simply ignores such requests. This way we always have at most one
					// record for removal for any given chunk.
					chunk.start_dying();

					chunksToDelete.push_back({this, &chunk});
				}

				// The chunk might have been emptied
				try_update_free_chunk_idx(chunk);
			}

			//! Tries to swap the entity at row \param rowA with the one at the row \param rowB.
			//! When swapping, all data associated with the two entities is swapped as well.
			//! If \param rowA equals \param rowB no swapping is performed.
			//! \warning "rowA" must he smaller or equal to "rowB"
			void swap_chunk_entities(Chunk& chunk, EntityContainers& recs, uint16_t rowA, uint16_t rowB) {
				// The "rowA" entity is the one we are going to destroy so it needs to precede the "rowB".
				// Unlike remove_entity_inter, it is not technically necessary but we do it
				// anyway for the sake of consistency.
				GAIA_ASSERT(rowA <= rowB);

				// If there are at least two different entities inside to swap
				if GAIA_UNLIKELY (chunk.m_header.count <= 1 || rowA == rowB)
					return;

				GAIA_PROF_SCOPE(Chunk::swap_chunk_entities);

				// Update entity data
				auto ev = chunk.entity_view_mut();
				const auto entityA = ev[rowA];
				const auto entityB = ev[rowB];

				auto& ecA = recs[entityA];
				auto& ecB = recs[entityB];
				GAIA_ASSERT(ecA.pArchetype == ecB.pArchetype);
				GAIA_ASSERT(ecA.pChunk == ecB.pChunk);

				ev[rowA] = entityB;
				ev[rowB] = entityA;

				// Swap component data
				auto recView = chunk.comp_rec_view();
				GAIA_FOR2(0, chunk.m_header.genEntities) {
					const auto* pItem = m_pItems[i];
					if (pItem == nullptr || pItem->comp.size() == 0U)
						continue;

					auto& rec = recView[i];
					GAIA_ASSERT(rec.pData == comp_ptr_mut(chunk.m_header, i));
					pItem->swap(rec.pData, rec.pData, rowA, rowB, m_properties.capacity, m_properties.capacity);
				}

				// Update indices in entity container.
				ecA.row = (uint16_t)rowB;
				ecB.row = (uint16_t)rowA;
			}

			//! Enables or disables the entity on a given row in the chunk.
			//! \param chunk Chunk the entity belongs to
			//! \param recs Entity records container
			//! \param row Row of the entity within chunk.
			//! \param enableEntity Enables or disables the entity.
			void enable_entity(Chunk& chunk, EntityContainers& recs, uint16_t row, bool enableEntity) {
				GAIA_ASSERT(
						!chunk.locked() && "Entities can't be enable while their chunk is being iterated "
															 "(structural changes are forbidden during this time!)");
				GAIA_ASSERT(row < chunk.m_header.count && "Entity chunk row out of bounds!");

				if (enableEntity) {
					// Nothing to enable if there are no disabled entities
					if (!chunk.m_header.has_disabled_entities())
						return;
					// Trying to enable an already enabled entity
					if (chunk.enabled(row))
						return;
					// Try swapping our entity with the last disabled one
					const auto entity = chunk.entity_view()[row];
					swap_chunk_entities(chunk, recs, --chunk.m_header.rowFirstEnabledEntity, row);
					recs[entity].dis = 0;
					++chunk.m_header.countEnabled;
				} else {
					// Nothing to disable if there are no enabled entities
					if (!chunk.m_header.has_enabled_entities())
						return;
					// Trying to disable an already disabled entity
					if (!chunk.enabled(row))
						return;
					// Try swapping our entity with the last one in our chunk
					const auto entity = chunk.entity_view()[row];
					swap_chunk_entities(chunk, recs, chunk.m_header.rowFirstEnabledEntity++, row);
					recs[entity].dis = 1;
					--chunk.m_header.countEnabled;
				}

				// m_disabledMask.set(pChunk->idx(), enableEntity ? true : pChunk->has_disabled_entities());
			}

			//----------------------------------------------------------------------
			// Graph
			//----------------------------------------------------------------------

			void build_graph_edges(Archetype* pArchetypeRight, Entity entity) {
				// Loops can't happen
				GAIA_ASSERT(pArchetypeRight != this);

				m_graph.add_edge_right(entity, pArchetypeRight->id(), pArchetypeRight->id_hash());
				pArchetypeRight->build_graph_edges_left(this, entity);
			}

			void build_graph_edges_left(Archetype* pArchetypeLeft, Entity entity) {
				// Loops can't happen
				GAIA_ASSERT(pArchetypeLeft != this);

				m_graph.add_edge_left(entity, pArchetypeLeft->id(), pArchetypeLeft->id_hash());
			}

			void del_graph_edges(Archetype* pArchetypeRight, Entity entity) {
				// Loops can't happen
				GAIA_ASSERT(pArchetypeRight != this);

				m_graph.del_edge_right(entity);
				pArchetypeRight->del_graph_edges_left(this, entity);
			}

			void del_graph_edges_left([[maybe_unused]] Archetype* pArchetypeLeft, Entity entity) {
				// Loops can't happen
				GAIA_ASSERT(pArchetypeLeft != this);

				m_graph.del_edge_left(entity);
			}

			//! Checks if an archetype graph "add" edge with entity \param entity exists.
			//! \return Archetype id of the target archetype if the edge is found. ArchetypeIdBad otherwise.
			GAIA_NODISCARD ArchetypeGraphEdge find_edge_right(Entity entity) const {
				return m_graph.find_edge_right(entity);
			}

			//! Checks if an archetype graph "del" edge with entity \param entity exists.
			//! \return Archetype id of the target archetype if the edge is found. ArchetypeIdBad otherwise.
			GAIA_NODISCARD ArchetypeGraphEdge find_edge_left(Entity entity) const {
				return m_graph.find_edge_left(entity);
			}

			//----------------------------------------------------------------------
			// Misc
			//----------------------------------------------------------------------

			//! Checks is there are no chunk in the archetype.
			GAIA_NODISCARD bool empty() const {
				return m_chunks.empty();
			}

			//! Request deleting the archetype.
			void req_del() {
				m_deleteReq = 1;
			}

			//! Returns true if this archetype is requested to be deleted.
			GAIA_NODISCARD bool is_req_del() const {
				return m_deleteReq;
			}

			//! Sets maximal lifespan of an archetype
			//! \param lifespan How many world updates an empty archetype is kept.
			//!                 If zero, the archetype it kept indefinitely.
			void set_max_lifespan(uint32_t lifespan) {
				GAIA_ASSERT(lifespan < MAX_ARCHETYPE_LIFESPAN);
				m_lifespanCountdownMax = lifespan;
			}

			//! Checks is this chunk is dying
			GAIA_NODISCARD bool dying() const {
				return m_lifespanCountdown > 0;
			}

			//! Marks the chunk as dead
			void die() {
				m_dead = 1;
			}

			//! Checks is this chunk is dying
			GAIA_NODISCARD bool dead() const {
				return m_dead == 1;
			}

			//! Starts the process of dying
			void start_dying() {
				GAIA_ASSERT(!dead());
				m_lifespanCountdown = m_lifespanCountdownMax;
			}

			//! Makes the archetype alive again
			void revive() {
				GAIA_ASSERT(!dead());
				m_lifespanCountdown = 0;
				m_deleteReq = 0;
			}

			//! Updates internal lifespan
			//! \return True if there is some lifespan left, false otherwise.
			bool progress_death() {
				GAIA_ASSERT(dying());
				--m_lifespanCountdown;
				return dying();
			}

			static void diag_entity(const World& world, Entity entity) {
				static constexpr const char* TagString[2] = {"", "[Tag]"};
				if (entity.entity()) {
					GAIA_LOG_N(
							"    ent [%u:%u] %s [%s] %s", entity.id(), entity.gen(), entity_name(world, entity),
							EntityKindString[entity.kind()], TagString[entity.tag()]);

				} else if (entity.pair()) {
					GAIA_LOG_N(
							"    pair [%u:%u] %s -> %s %s", entity.id(), entity.gen(), entity_name(world, entity.id()),
							entity_name(world, entity.gen()), TagString[entity.tag()]);
				} else {
					const auto& cc = comp_cache(world);
					const auto& desc = cc.get(entity);
					GAIA_LOG_N(
							"    hash:%016" PRIx64 ", size:%3u B, align:%3u B, [%u:%u] %s [%s] %s", desc.hashLookup.hash,
							desc.comp.size(), desc.comp.alig(), desc.entity.id(), desc.entity.gen(), desc.name.str(),
							EntityKindString[entity.kind()], TagString[entity.tag()]);
				}
			}

			static void diag_basic_info(const World& world, const Archetype& archetype) {
				auto ids = archetype.ids_view();
				auto comps = archetype.comps_view();

				// Calculate the number of entities in archetype
				uint32_t entCnt = 0;
				uint32_t entCntDisabled = 0;
				for (const auto* chunk: archetype.m_chunks) {
					entCnt += chunk->size();
					entCntDisabled += chunk->size_disabled();
				}

				// Calculate the number of components
				uint32_t genCompsSize = 0;
				uint32_t uniCompsSize = 0;
				{
					const auto& p = archetype.props();
					GAIA_FOR(p.genEntities) genCompsSize += comps[i].size();
					GAIA_FOR2(p.genEntities, comps.size()) uniCompsSize += comps[i].size();
				}

				GAIA_LOG_N(
						"aid:%u, "
						"hash:%016" PRIx64 ", "
						"chunks:%u (%uK), data:%u/%u/%u B, "
						"entities:%u/%u/%u",
						archetype.id(), archetype.lookup_hash().hash, (uint32_t)archetype.chunks().size(),
						Chunk::chunk_total_bytes(archetype.props().chunkDataBytes) <= 8192 ? 8 : 16, genCompsSize, uniCompsSize,
						archetype.props().chunkDataBytes, entCnt, entCntDisabled, archetype.props().capacity);

				if (!ids.empty()) {
					GAIA_LOG_N("  Components - count:%u", (uint32_t)ids.size());
					for (const auto ent: ids)
						diag_entity(world, ent);
				}
			}

			static void diag_graph_info(const World& world, const Archetype& archetype) {
				archetype.m_graph.diag(world);
			}

			static void diag_chunk_info(const Archetype& archetype) {
				const auto& chunks = archetype.m_chunks;
				if (chunks.empty())
					return;

				GAIA_LOG_N("  Chunks");
				for (const auto* pChunk: chunks)
					pChunk->diag();
			}

			static void diag_entity_info(const World& world, const Archetype& archetype) {
				const auto& chunks = archetype.m_chunks;
				if (chunks.empty())
					return;

				GAIA_LOG_N("  Entities");
				bool noEntities = true;
				for (const auto* pChunk: chunks) {
					if (pChunk->empty())
						continue;
					noEntities = false;

					auto ev = pChunk->entity_view();
					for (auto entity: ev)
						diag_entity(world, entity);
				}
				if (noEntities)
					GAIA_LOG_N("    N/A");
			}

			//! Performs diagnostics on a specific archetype. Prints basic info about it and the chunks it contains.
			//! \param archetype Archetype to run diagnostics on
			static void diag(const World& world, const Archetype& archetype) {
				diag_basic_info(world, archetype);
				diag_graph_info(world, archetype);
				diag_chunk_info(archetype);
				diag_entity_info(world, archetype);
			}
		};

		class ArchetypeLookupKey final {
			Archetype::LookupHash m_hash;
			const ArchetypeBase* m_pArchetypeBase;

		public:
			static constexpr bool IsDirectHashKey = true;

			ArchetypeLookupKey(): m_hash({0}), m_pArchetypeBase(nullptr) {}
			explicit ArchetypeLookupKey(Archetype::LookupHash hash, const ArchetypeBase* pArchetypeBase):
					m_hash(hash), m_pArchetypeBase(pArchetypeBase) {}

			GAIA_NODISCARD size_t hash() const {
				return (size_t)m_hash.hash;
			}

			GAIA_NODISCARD Archetype* archetype() const {
				return (Archetype*)m_pArchetypeBase;
			}

			GAIA_NODISCARD bool operator==(const ArchetypeLookupKey& other) const {
				// Hash doesn't match we don't have a match.
				// Hash collisions are expected to be very unlikely so optimize for this case.
				if GAIA_LIKELY (m_hash != other.m_hash)
					return false;

				const auto id = m_pArchetypeBase->id();
				if (id == ArchetypeIdBad) {
					const auto* pArchetype = (const Archetype*)other.m_pArchetypeBase;
					const auto* pArchetypeLookupChecker = (const ArchetypeLookupChecker*)m_pArchetypeBase;
					return pArchetype->cmp_comps(*pArchetypeLookupChecker);
				}

				// Real ArchetypeID is given. Compare the pointers.
				// Normally we'd compare archetype IDs but because we do not allow archetype copies and all archetypes are
				// unique it's guaranteed that if pointers are the same we have a match.
				// This also saves a pointer indirection because we do not access the memory the pointer points to.
				return m_pArchetypeBase == other.m_pArchetypeBase;
			}
		};

		using ArchetypeMapByHash = cnt::map<ArchetypeLookupKey, Archetype*>;
	} // namespace ecs
} // namespace gaia
