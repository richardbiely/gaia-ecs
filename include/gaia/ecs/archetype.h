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
				const auto s0 = comps.size();
				const auto s1 = compsOther.size();

				// Size has to match
				if (s0 != s1)
					return false;

				// Elements have to match
				GAIA_FOR(s0) {
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
			};

		private:
			using AsPairsIndexBuffer = cnt::sarray<uint8_t, ChunkHeader::MAX_COMPONENTS>;

			ArchetypeIdLookupKey::LookupHash m_archetypeIdHash;
			//! Hash of components within this archetype - used for lookups
			LookupHash m_hashLookup = {0};

			Properties m_properties{};
			//! Pointer to the parent world
			const World& m_world;
			//! Component cache reference
			const ComponentCache& m_cc;
			//! Stable reference to parent world's world version
			uint32_t& m_worldVersion;

			//! Array of chunks allocated by this archetype
			cnt::darray<Chunk*> m_chunks;
			//! Mask of chunks with disabled entities
			// cnt::dbitset m_disabledMask;
			//! Graph of archetypes linked with this one
			ArchetypeGraph m_graph;

			//! Offsets to various parts of data inside chunk
			ChunkDataOffsets m_dataOffsets;
			//! Array of entities used to identify the archetype
			Entity m_ids[ChunkHeader::MAX_COMPONENTS];
			//! Array of indices to Is relationship pairs in m_ids
			uint8_t m_pairs_as_index_buffer[ChunkHeader::MAX_COMPONENTS];
			//! Array of component ids
			Component m_comps[ChunkHeader::MAX_COMPONENTS];
			//! Array of components offset indices
			ChunkDataOffset m_compOffs[ChunkHeader::MAX_COMPONENTS];

			//! Index of the first chunk with enough space to add at least one entity
			uint32_t m_firstFreeChunkIdx = 0;
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
			// uint32_t m_unused : 6;

			//! Constructor is hidden. Create archetypes via Archetype::Create
			Archetype(const World& world, const ComponentCache& cc, uint32_t& worldVersion):
					m_world(world), m_cc(cc), m_worldVersion(worldVersion), m_listIdx(BadIndex), //
					m_deleteReq(0), m_dead(0), //
					m_lifespanCountdownMax(1), m_lifespanCountdown(0), //
					m_pairCnt(0), m_pairCnt_is(0) {}

			~Archetype() {
				// Delete all archetype chunks
				for (auto* pChunk: m_chunks)
					Chunk::free(pChunk);
			}

			//! Calculates offsets in memory at which important chunk data is going to be stored.
			//! These offsets are use to setup the chunk data area layout.
			//! \param memoryAddress Memory address used to calculate offsets
			void update_data_offsets(uintptr_t memoryAddress) {
				uintptr_t offset = 0;

				// Versions
				// We expect versions to fit in the first 256 bytes.
				// With 32 components per archetype this gives us some headroom.
				{
					offset += mem::padding<alignof(ComponentVersion)>(memoryAddress);

					const auto cnt = comps_view().size();
					if (cnt != 0) {
						GAIA_ASSERT(offset < 256);
						m_dataOffsets.firstByte_Versions = (ChunkDataVersionOffset)offset;
						offset += sizeof(ComponentVersion) * cnt;
					}
				}

				// Entity ids
				{
					offset += mem::padding<alignof(Entity)>(offset);

					const auto cnt = comps_view().size();
					if (cnt != 0) {
						m_dataOffsets.firstByte_CompEntities = (ChunkDataOffset)offset;

						// Storage-wise, treat the component array as it it were MAX_COMPONENTS long.
						offset += sizeof(Entity) * ChunkHeader::MAX_COMPONENTS;
					}
				}

				// Component records
				{
					offset += mem::padding<alignof(ComponentRecord)>(offset);

					const auto cnt = comps_view().size();
					if (cnt != 0) {

						m_dataOffsets.firstByte_Records = (ChunkDataOffset)offset;

						// Storage-wise, treat the component array as it it were MAX_COMPONENTS long.
						offset += sizeof(ComponentRecord) * cnt;
					}
				}

				// First entity offset
				{
					offset += mem::padding<alignof(Entity)>(offset);
					m_dataOffsets.firstByte_EntityData = (ChunkDataOffset)offset;
				}
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
					Archetype& arch, EntitySpan ids, ComponentSpan comps, uint8_t from, uint8_t to, uint32_t& currOff,
					uint32_t count) {
				auto& ofs = arch.m_compOffs;

				// Set component ids
				GAIA_FOR2(from, to) arch.m_ids[i] = ids[i];

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

			GAIA_NODISCARD static Archetype*
			create(const World& world, ArchetypeId archetypeId, uint32_t& worldVersion, EntitySpan ids) {
				const auto& cc = comp_cache(world);

				auto* newArch = mem::AllocHelper::alloc<Archetype>("Archetype");
				(void)new (newArch) Archetype(world, cc, worldVersion);

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
				GAIA_FOR(cnt) {
					if (ids[i].pair()) {
						// When using pairs we need to decode the storage type from them.
						// This is what pair<Rel, Tgt>::type actually does to determine what type to use at compile-time.
						Entity pairEntities[] = {entity_from_id(world, ids[i].id()), entity_from_id(world, ids[i].gen())};
						Component pairComponents[] = {as_comp(pairEntities[0]), as_comp(pairEntities[1])};
						const uint32_t idx = (pairComponents[0].size() != 0U || pairComponents[1].size() == 0U) ? 0 : 1;
						comps[i] = pairComponents[idx];
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
				GAIA_FOR(cnt) {
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
				reg_components(*newArch, ids, comps, (uint8_t)0, (uint8_t)entsGeneric, currOff, maxGenItemsInArchetype);
				reg_components(*newArch, ids, comps, (uint8_t)entsGeneric, (uint8_t)ids.size(), currOff, 1);

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

			ArchetypeIdLookupKey::LookupHash id_hash() const {
				return m_archetypeIdHash;
			}

			//! Sets hashes for each component type and lookup.
			//! \param hashLookup Hash used for archetype lookup purposes
			void set_hashes(LookupHash hashLookup) {
				m_hashLookup = hashLookup;
			}

			//! Enables or disables the entity on a given row in the chunk.
			//! \param pChunk Chunk the entity belongs to
			//! \param row Row of the entity
			//! \param enableEntity Enables the entity
			void enable_entity(Chunk* pChunk, uint16_t row, bool enableEntity, EntityContainers& recs) {
				pChunk->enable_entity(row, enableEntity, recs);
				// m_disabledMask.set(pChunk->idx(), enableEntity ? true : pChunk->has_disabled_entities());
			}

			//! Removes a chunk from the list of chunks managed by their archetype and deletes its memory.
			//! \param pChunk Chunk to remove from the list of managed archetypes
			void del(Chunk* pChunk) {
				// Make sure there are any chunks to delete
				GAIA_ASSERT(!m_chunks.empty());

				const auto chunkIndex = pChunk->idx();

				// Make sure the chunk is a part of the chunk array
				GAIA_ASSERT(chunkIndex == core::get_index(m_chunks, pChunk));

				// Remove the chunk from the chunk array. We are swapping this chunk's entry
				// with the last one in the array. Therefore, we first update the last item's
				// index with the current chunk's index and then do the swapping.
				m_chunks.back()->set_idx(chunkIndex);
				core::swap_erase(m_chunks, chunkIndex);

				// Delete the chunk now. Otherwise, if the chunk happened to be the last
				// one we would end up overriding released memory.
				Chunk::free(pChunk);
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
						if (entityCnt < pChunk->capacity()) {
							m_firstFreeChunkIdx = i;
							return pChunk;
						}
					}
				}

				// Make sure not too many chunks are allocated
				GAIA_ASSERT(chunkCnt < UINT32_MAX);

				// No free space found anywhere. Let's create a new chunk.
				auto* pChunk = Chunk::create(
						m_world, m_cc, chunkCnt, //
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
				if (pChunk->size() >= pChunk->capacity())
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
				if (pChunk->size() >= pChunk->capacity())
					++m_firstFreeChunkIdx;
			}

			void remove_entity_raw(Chunk& chunk, uint16_t row, EntityContainers& recs) {
				chunk.remove_entity(row, recs);
				try_update_free_chunk_idx(chunk);
			}

			void remove_entity(Chunk& chunk, uint16_t row, EntityContainers& recs) {
				remove_entity_raw(chunk, row, recs);
				chunk.update_versions();
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

			GAIA_NODISCARD auto& right_edges() {
				return m_graph.right_edges();
			}

			GAIA_NODISCARD const auto& right_edges() const {
				return m_graph.right_edges();
			}

			GAIA_NODISCARD auto& left_edges() {
				return m_graph.left_edges();
			}

			GAIA_NODISCARD const auto& left_edges() const {
				return m_graph.left_edges();
			}

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
				GAIA_ASSERT(lifespan > 0);
				GAIA_ASSERT(lifespan <= MAX_ARCHETYPE_LIFESPAN);

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
			GAIA_NODISCARD bool progress_death() {
				GAIA_ASSERT(dying());
				--m_lifespanCountdown;
				return dying();
			}

			//! Tells whether archetype is ready to be deleted
			GAIA_NODISCARD bool ready_to_die() const {
				return m_lifespanCountdownMax > 0 && !dying() && empty();
			}

			static void diag_entity(const World& world, Entity entity) {
				if (entity.entity()) {
					GAIA_LOG_N(
							"    ent [%u:%u] %s [%s]", entity.id(), entity.gen(), entity_name(world, entity),
							EntityKindString[entity.kind()]);
				} else if (entity.pair()) {
					GAIA_LOG_N(
							"    pair [%u:%u] %s -> %s", entity.id(), entity.gen(), entity_name(world, entity.id()),
							entity_name(world, entity.gen()));
				} else {
					const auto& cc = comp_cache(world);
					const auto& desc = cc.get(entity);
					GAIA_LOG_N(
							"    hash:%016" PRIx64 ", size:%3u B, align:%3u B, [%u:%u] %s [%s]", desc.hashLookup.hash,
							desc.comp.size(), desc.comp.alig(), desc.entity.id(), desc.entity.gen(), desc.name.str(),
							EntityKindString[entity.kind()]);
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
