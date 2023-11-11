#pragma once
#include "../config/config.h"

#include <cinttypes>

#include "../cnt/darray.h"
#include "../cnt/dbitset.h"
#include "../cnt/sarray.h"
#include "../cnt/sarray_ext.h"
#include "../core/hashing_policy.h"
#include "archetype_common.h"
#include "archetype_graph.h"
#include "chunk.h"
#include "chunk_allocator.h"
#include "chunk_header.h"
#include "component.h"
#include "component_cache.h"
#include "component_utils.h"
#include "id.h"

namespace gaia {
	namespace ecs {
		class Archetype;
		struct EntityContainer;

		class ArchetypeBase {
		protected:
			//! Archetype ID - used to address the archetype directly in the world's list or archetypes
			ArchetypeId m_archetypeId = ArchetypeIdBad;

		public:
			GAIA_NODISCARD ArchetypeId id() const {
				return m_archetypeId;
			}
		};

		GAIA_NODISCARD inline bool cmp_comps(ComponentSpan* comps, ComponentSpan* compsOther) {
			// Size has to match
			GAIA_FOR(ComponentKind::CK_Count) {
				if (comps[i].size() != compsOther[i].size())
					return false;
			}

			// Elements have to match
			GAIA_FOR(ComponentKind::CK_Count) {
				const auto& c0 = comps[i];
				const auto& c1 = compsOther[i];
				GAIA_EACH_(c0, j) {
					if (c0[j] != c1[j])
						return false;
				}
			}

			return true;
		}

		class ArchetypeLookupChecker: public ArchetypeBase {
			friend class Archetype;

			//! List of component indices
			ComponentSpan m_comps[ComponentKind::CK_Count];

		public:
			ArchetypeLookupChecker(ComponentSpan compsGen, ComponentSpan compsUni) {
				m_comps[ComponentKind::CK_Gen] = compsGen;
				m_comps[ComponentKind::CK_Uni] = compsUni;
			}

			GAIA_NODISCARD bool cmp_comps(const ArchetypeLookupChecker& other) const {
				return ecs::cmp_comps((ComponentSpan*)&m_comps[0], (ComponentSpan*)&other.m_comps[0]);
			}
		};

		class Archetype final: public ArchetypeBase {
		public:
			using LookupHash = core::direct_hash_key<uint64_t>;
			using GenComponentHash = core::direct_hash_key<uint64_t>;
			using UniComponentHash = core::direct_hash_key<uint64_t>;

			struct Properties {
				//! The number of entities this archetype can take (e.g 5 = 5 entities with all their components)
				uint16_t capacity;
				//! How many bytes of data is needed for a fully utilized chunk
				ChunkDataOffset chunkDataBytes;
			};

		private:
			Properties m_properties{};
			//! Stable reference to parent world's world version
			uint32_t& m_worldVersion;

			//! List of chunks allocated by this archetype
			cnt::darray<Chunk*> m_chunks;
			//! Mask of chunks with disabled entities
			// cnt::dbitset m_disabledMask;
			//! Graph of archetypes linked with this one
			ArchetypeGraph m_graph;

			//! Offsets to various parts of data inside chunk
			ChunkDataOffsets m_dataOffsets;
			//! List of component ids
			cnt::sarray<Chunk::ComponentArray, ComponentKind::CK_Count> m_comps;
#if GAIA_COMP_ID_PROBING
			//! List of component ids -> offset index mappings
			cnt::sarray<Chunk::ComponentIdInterMap, ComponentKind::CK_Count> m_compMap;
#endif
			//! List of components offset indices
			cnt::sarray<Chunk::ComponentOffsetArray, ComponentKind::CK_Count> m_compOffs;

			//! Hash of generic components
			GenComponentHash m_hashGen = {0};
			//! Hash of unique components
			UniComponentHash m_hashUni = {0};
			//! Hash of components within this archetype - used for lookups
			LookupHash m_hashLookup = {0};
			//! Hash of components within this archetype - used for matching
			ComponentMatcherHash m_matcherHash[ComponentKind::CK_Count]{};

			//! Number of bits representing archetype lifespan
			static constexpr uint16_t ARCHETYPE_LIFESPAN_BITS = 7;
			//! Archetype lifespan must be at least as long as chunk lifespan
			static_assert(ARCHETYPE_LIFESPAN_BITS >= ChunkHeader::CHUNK_LIFESPAN_BITS);
			//! Number of ticks before empty chunks are removed
			static constexpr uint16_t MAX_ARCHETYPE_LIFESPAN = (1 << ARCHETYPE_LIFESPAN_BITS) - 1;

			uint32_t m_lifespanCountdown: ARCHETYPE_LIFESPAN_BITS;
			uint32_t m_dead : 1;

			// Constructor is hidden. Create archetypes via Create
			Archetype(uint32_t& worldVersion): m_worldVersion(worldVersion) {}

			//! Calulcates offsets in memory at which important chunk data is going to be stored.
			//! These offsets are use to setup the chunk data area layout.
			//! \param memoryAddress Memory address used to calculate offsets
			void update_data_offsets(uintptr_t memoryAddress) {
				uintptr_t offset = 0;

				// Versions
				// We expect versions to fit in the first 256 bytes.
				// With 64 components per archetype (32 generic + 32 unique) this gives us some headroom.
				{
					offset += mem::padding<alignof(ComponentVersion)>(memoryAddress);
					GAIA_FOR(ComponentKind::CK_Count) {
						const auto cnt = comps((ComponentKind)i).size();
						if (cnt == 0)
							continue;

						GAIA_ASSERT(offset < 256);
						m_dataOffsets.firstByte_Versions[i] = (ChunkDataVersionOffset)offset;
						offset += sizeof(ComponentVersion) * cnt;
					}
				}

				// Component ids
				{
					offset += mem::padding<alignof(Component)>(offset);
					GAIA_FOR(ComponentKind::CK_Count) {
						const auto cnt = comps((ComponentKind)i).size();
						if (cnt == 0)
							continue;

						m_dataOffsets.firstByte_ComponentIds[i] = (ChunkDataOffset)offset;

						// Storage-wise, treat the component array as it it were MAX_COMPONENTS long.
						offset += sizeof(Component) * Chunk::MAX_COMPONENTS;
					}
				}

				// Component records
				{
					offset += mem::padding<alignof(ComponentRecord)>(offset);
					GAIA_FOR(ComponentKind::CK_Count) {
						const auto cnt = comps((ComponentKind)i).size();
						if (cnt == 0)
							continue;

						m_dataOffsets.firstByte_Records[i] = (ChunkDataOffset)offset;

						// Storage-wise, treat the component array as it it were MAX_COMPONENTS long.
						offset += sizeof(ComponentRecord) * cnt;
					}
				}

#if GAIA_COMP_ID_PROBING
				// Component ids internal map
				{
					offset += mem::padding<alignof(Component)>(offset);
					GAIA_FOR(ComponentKind::CK_Count) {
						const auto cnt = comps((ComponentKind)i).size();
						if (cnt == 0)
							continue;

						m_dataOffsets.firstByte_CompIdMap[i] = (ChunkDataOffset)offset;
						offset += sizeof(Component) * cnt;
					}
				}
#endif

				// First entity offset
				{
					offset += mem::padding<alignof(Entity)>(offset);
					m_dataOffsets.firstByte_EntityData = (ChunkDataOffset)offset;
				}
			}

			/*!
			Estimates how many entities can fit into the chunk described by \param comps components.
			*/
			static bool est_max_entities_per_archetype(
					uint32_t& offs, uint32_t& maxItems, ComponentSpan comps, uint32_t size, uint32_t maxDataOffset) {
				const auto& cc = ComponentCache::get();

				for (const auto comp: comps) {
					if (comp.alig() == 0)
						continue;

					const auto& desc = cc.comp_desc(comp.id());

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

			static void
			reg_components(Archetype& arch, ComponentSpan comps, ComponentKind compKind, uint32_t& currOff, uint32_t count) {
				auto& ids = arch.m_comps[compKind];
#if GAIA_COMP_ID_PROBING
				auto& map = arch.m_compMap[compKind];
#endif
				auto& ofs = arch.m_compOffs[compKind];

				// Set component ids
				GAIA_EACH(comps) ids[i] = comps[i];

#if GAIA_COMP_ID_PROBING
				// Generate component id map. Initialize it with ComponentBad values.
				for (auto& compId: map)
					compId = IdentifierIdBad;
				for (auto comp: comps)
					ecs::set_comp_idx({map.data(), map.size()}, comp.id());
#endif

				// Calulate offsets and assign them indices according to our mappings
				GAIA_EACH(comps) {
					const auto comp = comps[i];
#if GAIA_COMP_ID_PROBING
					const auto compIdx = ecs::get_comp_idx({map.data(), map.size()}, comp.id());
#else
					const auto compIdx = i;
#endif
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
			Archetype(Archetype&& world) = delete;
			Archetype(const Archetype& world) = delete;
			Archetype& operator=(Archetype&&) = delete;
			Archetype& operator=(const Archetype&) = delete;

			~Archetype() {
				// Delete all archetype chunks
				for (auto* pChunk: m_chunks)
					Chunk::free(pChunk);
			}

			GAIA_NODISCARD bool cmp_comps(const ArchetypeLookupChecker& other) const {
				const auto& compsGen = comps(ComponentKind::CK_Gen);
				const auto& compsUni = comps(ComponentKind::CK_Uni);
				ComponentSpan s[ComponentKind::CK_Count] = {
						{compsGen.data(), compsGen.size()}, {compsUni.data(), compsUni.size()}};
				return ecs::cmp_comps(s, (ComponentSpan*)&other.m_comps[0]);
			}

			GAIA_NODISCARD static LookupHash calc_lookup_hash(GenComponentHash hashGen, UniComponentHash hashUni) noexcept {
				return {core::hash_combine(hashGen.hash, hashUni.hash)};
			}

			GAIA_NODISCARD static Archetype*
			create(ArchetypeId archetypeId, uint32_t& worldVersion, ComponentSpan compsGen, ComponentSpan compsUni) {
				auto* newArch = new Archetype(worldVersion);
				newArch->m_archetypeId = archetypeId;
				const uint32_t maxEntities = archetypeId == 0 ? ChunkHeader::MAX_CHUNK_ENTITIES : 512;

				newArch->m_comps[ComponentKind::CK_Gen].resize((uint32_t)compsGen.size());
				newArch->m_comps[ComponentKind::CK_Uni].resize((uint32_t)compsUni.size());
#if GAIA_COMP_ID_PROBING
				newArch->m_compMap[ComponentKind::CK_Gen].resize((uint32_t)compsGen.size());
				newArch->m_compMap[ComponentKind::CK_Uni].resize((uint32_t)compsUni.size());
#endif
				newArch->m_compOffs[ComponentKind::CK_Gen].resize((uint32_t)compsGen.size());
				newArch->m_compOffs[ComponentKind::CK_Uni].resize((uint32_t)compsUni.size());

				static auto ChunkDataAreaOffset = Chunk::chunk_data_area_offset();
				newArch->update_data_offsets(
						// This is not a real memory address.
						// Chunk memory is organized as header+data. The offsets we calculate here belong to
						// the data area.
						// Every allocated chunk is going to have the same relative offset from the header part
						// which is why providing a fictional relative offset is enough.
						ChunkDataAreaOffset);

				const auto& offs = newArch->m_dataOffsets;

				// Calculate the number of entities per chunks precisely so we can
				// fit as many of them into chunk as possible.

				uint32_t genCompsSize = 0;
				uint32_t uniCompsSize = 0;
				for (auto comp: compsGen)
					genCompsSize += comp.size();
				for (auto comp: compsUni)
					uniCompsSize += comp.size();

				const uint32_t size0 = Chunk::chunk_data_bytes(detail::ChunkAllocatorImpl::mem_block_size(0));
				const uint32_t size1 = Chunk::chunk_data_bytes(detail::ChunkAllocatorImpl::mem_block_size(1));
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
								currOff, maxGenItemsInArchetype, compsGen, maxGenItemsInArchetype, maxDataOffsetTarget))
					goto recalculate;
				if (!est_max_entities_per_archetype(currOff, maxGenItemsInArchetype, compsUni, 1, maxDataOffsetTarget))
					goto recalculate;

				// Limit the number of entities to a certain number so we can make use of smaller
				// chunks where it makes sense.
				// TODO:
				// Tweak this so the full remaining capacity is used. So if we occupy 7000 B we still
				// have 1000 B left to fill.
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
				reg_components(*newArch, compsGen, ComponentKind::CK_Gen, currOff, maxGenItemsInArchetype);
				reg_components(*newArch, compsUni, ComponentKind::CK_Uni, currOff, 1);

				GAIA_ASSERT(
						Chunk::chunk_total_bytes((ChunkDataOffset)currOff) < detail::ChunkAllocatorImpl::mem_block_size(currOff));
				newArch->m_properties.capacity = (uint16_t)maxGenItemsInArchetype;
				newArch->m_properties.chunkDataBytes = (ChunkDataOffset)currOff;

				newArch->m_matcherHash[ComponentKind::CK_Gen] = ecs::matcher_hash(compsGen);
				newArch->m_matcherHash[ComponentKind::CK_Uni] = ecs::matcher_hash(compsUni);

				return newArch;
			}

			/*!
			Sets hashes for each component type and lookup.
			\param genHash Generic components hash
			\param uniHash Unique components hash
			\param hashLookup Hash used for archetype lookup purposes
			*/
			void set_hashes(GenComponentHash genHash, UniComponentHash uniHash, LookupHash hashLookup) {
				m_hashGen = genHash;
				m_hashUni = uniHash;
				m_hashLookup = hashLookup;
			}

			/*!
			Enables or disables the entity on a given index in the chunk.
			\param pChunk Chunk the entity belongs to
			\param index Index of the entity
			\param enableEntity Enables the entity
			*/
			void enable_entity(Chunk* pChunk, uint32_t entityIdx, bool enableEntity, std::span<EntityContainer> entities) {
				pChunk->enable_entity(entityIdx, enableEntity, entities);
				// m_disabledMask.set(pChunk->idx(), enableEntity ? true : pChunk->has_disabled_entities());
			}

			/*!
			Removes a chunk from the list of chunks managed by their archetype.
			\param pChunk Chunk to remove from the list of managed archetypes
			*/
			void remove_chunk(Chunk* pChunk, cnt::darray<Archetype*>& archetypesToRemove) {
				const auto chunkIndex = pChunk->idx();

				Chunk::free(pChunk);

				auto remove = [&](auto& chunkArray) {
					if (chunkArray.size() > 1)
						chunkArray.back()->set_idx(chunkIndex);
					GAIA_ASSERT(chunkIndex == core::get_index(chunkArray, pChunk));
					core::erase_fast(chunkArray, chunkIndex);
				};

				remove(m_chunks);

				// No deleting for the root archetype
				if (m_archetypeId == 0)
					return;

				// TODO: This needs cleaning up.
				//       Chunk should have no idea of the world and also should not store
				//       any states realted to its lifetime.
				if (!dying() && empty()) {
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

					archetypesToRemove.push_back(this);
				}
			}

			//! Defragments the chunk.
			//! \param maxEntites Maximum number of entities moved per call
			//! \param chunksToRemove Container of chunks ready for removal
			//! \param entities Container with entities
			void defrag(uint32_t& maxEntities, cnt::darray<Chunk*>& chunksToRemove, std::span<EntityContainer> entities) {
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
				while (front < back && !m_chunks[front++]->is_semi())
					;

				auto* pDstChunk = m_chunks[front];

				// Find the first semi-empty chunk in the back
				while (front < back && m_chunks[--back]->is_semi()) {
					auto* pSrcChunk = m_chunks[back];

					const uint32_t entitiesInChunk = pSrcChunk->size();
					const uint32_t entitiesToMove = entitiesInChunk > maxEntities ? maxEntities : entitiesInChunk;
					GAIA_FOR(entitiesToMove) {
						const auto lastEntityIdx = entitiesInChunk - i - 1;
						const auto entity = pSrcChunk->entity_view()[lastEntityIdx];

						const auto& entityContainer = entities[entity.id()];

						const auto oldIndex = entityContainer.idx;
						const auto newIndex = pDstChunk->add_entity(entity);
						const bool wasEnabled = !entityContainer.dis;

						// Make sure the old entity becomes enabled now
						enable_entity(pSrcChunk, oldIndex, true, entities);
						// We go back-to-front in the chunk so enabling the entity is not expected to change its index
						GAIA_ASSERT(oldIndex == entityContainer.idx);

						// Transfer the original enabled state to the new chunk
						enable_entity(pDstChunk, newIndex, wasEnabled, entities);

						// Remove the entity record from the old chunk
						pSrcChunk->remove_entity(oldIndex, entities, chunksToRemove);

						// The destination chunk is full, we need to move to the next one
						if (pDstChunk->size() == m_properties.capacity) {
							++front;

							// We reached the source chunk which means this archetype has been defragmented
							if (front >= back) {
								maxEntities -= i + 1;
								return;
							}
						}
					}

					maxEntities -= entitiesToMove;
				}
			}

			//! Tries to locate a chunk that has some space left for a new entity.
			//! If not found a new chunk is created.
			GAIA_NODISCARD Chunk* foc_free_chunk() {
				const auto chunkCnt = m_chunks.size();

				if (chunkCnt > 0) {
					// Find first semi-empty chunk.
					// Picking the first non-full would only support fragmentation.
					Chunk* pEmptyChunk = nullptr;
					for (auto* pChunk: m_chunks) {
						GAIA_ASSERT(pChunk != nullptr);
						const auto entityCnt = pChunk->size();
						if GAIA_UNLIKELY (entityCnt == 0)
							pEmptyChunk = pChunk;
						else if (entityCnt < pChunk->capacity())
							return pChunk;
					}
					if (pEmptyChunk != nullptr)
						return pEmptyChunk;
				}

				// Make sure not too many chunks are allocated
				GAIA_ASSERT(chunkCnt < UINT32_MAX);

				// No free space found anywhere. Let's create a new chunk.
				auto* pChunk = Chunk::create(
						chunkCnt, props().capacity, m_properties.chunkDataBytes, m_worldVersion, m_dataOffsets, m_comps,
#if GAIA_COMP_ID_PROBING
						m_compMap,
#endif
						m_compOffs);

				m_chunks.push_back(pChunk);
				return pChunk;
			}

			GAIA_NODISCARD const Properties& props() const {
				return m_properties;
			}

			GAIA_NODISCARD const cnt::darray<Chunk*>& chunks() const {
				return m_chunks;
			}

			GAIA_NODISCARD GenComponentHash generic_hash() const {
				return m_hashGen;
			}

			GAIA_NODISCARD UniComponentHash chunk_hash() const {
				return m_hashUni;
			}

			GAIA_NODISCARD LookupHash lookup_hash() const {
				return m_hashLookup;
			}

			GAIA_NODISCARD ComponentMatcherHash matcher_hash(ComponentKind compKind) const {
				return m_matcherHash[compKind];
			}

			GAIA_NODISCARD const Chunk::ComponentArray& comps(ComponentKind compKind) const {
				return m_comps[compKind];
			}

			GAIA_NODISCARD const Chunk::ComponentOffsetArray& comp_offs(ComponentKind compKind) const {
				return m_compOffs[compKind];
			}

			/*!
			Checks if a component with \param comp and type \param compKind is present in the archetype.
			\param comp Component id
			\param compKind Component type
			\return True if found. False otherwise.
			*/
			GAIA_NODISCARD bool has(ComponentKind compKind, ComponentId compId) const {
				const auto& c = comps(compKind);
				return core::has_if(c, [&](Component comp) {
					return comp.id() == compId;
				});
			}

			/*!
			Checks if a component of type \tparam T is present in the archetype.
			\return True if found. False otherwise.
			*/
			template <typename T>
			GAIA_NODISCARD bool has() const {
				const auto compId = comp_id<T>();

				constexpr auto compKind = component_kind_v<T>;
				return has(compKind, compId);
			}

			void build_graph_edges(Archetype* pArchetypeRight, ComponentKind compKind, ComponentId compId) {
				GAIA_ASSERT(pArchetypeRight != this);
				m_graph.add_edge_right(compKind, compId, pArchetypeRight->id());
				pArchetypeRight->build_graph_edges_left(this, compKind, compId);
			}

			void build_graph_edges_left(Archetype* pArchetypeLeft, ComponentKind compKind, ComponentId compId) {
				GAIA_ASSERT(pArchetypeLeft != this);
				m_graph.add_edge_left(compKind, compId, pArchetypeLeft->id());
			}

			//! Checks if the graph edge for component type \param compKind contains the component \param compId.
			//! \return Archetype id of the target archetype if the edge is found. ArchetypeIdBad otherwise.
			GAIA_NODISCARD ArchetypeId find_edge_right(ComponentKind compKind, ComponentId compId) const {
				return m_graph.find_edge_right(compKind, compId);
			}

			//! Checks if the graph edge for component type \param compKind contains the component \param compId.
			//! \return Archetype id of the target archetype if the edge is found. ArchetypeIdBad otherwise.
			GAIA_NODISCARD ArchetypeId find_edge_left(ComponentKind compKind, ComponentId compId) const {
				return m_graph.find_edge_left(compKind, compId);
			}

			//! Checks is there are no chunk in the archetype
			GAIA_NODISCARD bool empty() const {
				return m_chunks.empty();
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
				m_lifespanCountdown = MAX_ARCHETYPE_LIFESPAN;
			}

			//! Makes the chunk alive again
			void revive() {
				GAIA_ASSERT(!dead());
				m_lifespanCountdown = 0;
			}

			//! Updates internal lifetime
			//! \return True if there is some lifespan left, false otherwise.
			bool progress_death() {
				GAIA_ASSERT(dying());
				--m_lifespanCountdown;
				return dying();
			}

			static void diag_basic_info(const Archetype& archetype) {
				const auto& cc = ComponentCache::get();
				const auto& genComps = archetype.comps(ComponentKind::CK_Gen);
				const auto& uniComps = archetype.comps(ComponentKind::CK_Uni);

				// Caclulate the number of entites in archetype
				uint32_t entCnt = 0;
				uint32_t entCntDisabled = 0;
				for (const auto* chunk: archetype.m_chunks) {
					entCnt += chunk->size();
					entCntDisabled += chunk->size_disabled();
				}

				// Calculate the number of components
				uint32_t genCompsSize = 0;
				uint32_t uniCompsSize = 0;
				for (auto comp: genComps)
					genCompsSize += comp.size();
				for (auto comp: uniComps)
					uniCompsSize += comp.size();

				GAIA_LOG_N(
						"Archetype ID:%u, "
						"hashLookup:%016" PRIx64 ", "
						"mask:%016" PRIx64 "/%016" PRIx64 ", "
						"chunks:%u (%uK), data:%u/%u/%u B, "
						"entities:%u/%u/%u",
						archetype.id(), archetype.lookup_hash().hash, archetype.matcher_hash(ComponentKind::CK_Gen).hash,
						archetype.matcher_hash(ComponentKind::CK_Uni).hash, (uint32_t)archetype.chunks().size(),
						Chunk::chunk_total_bytes(archetype.props().chunkDataBytes) <= 8192 ? 8 : 16, genCompsSize, uniCompsSize,
						archetype.props().chunkDataBytes, entCnt, entCntDisabled, archetype.props().capacity);

				auto logComponentInfo = [](const ComponentDesc& desc) {
					GAIA_LOG_N(
							"    hashLookup:%016" PRIx64 ", mask:%016" PRIx64 ", size:%3u B, align:%3u B, %.*s", desc.hashLookup.hash,
							desc.matcherHash.hash, desc.comp.size(), desc.comp.alig(), (uint32_t)desc.name.size(), desc.name.data());
				};

				if (!genComps.empty()) {
					GAIA_LOG_N("  Generic components - count:%u", (uint32_t)genComps.size());
					for (const auto comp: genComps)
						logComponentInfo(cc.comp_desc(comp.id()));
					if (!uniComps.empty()) {
						GAIA_LOG_N("  Unique components - count:%u", (uint32_t)uniComps.size());
						for (const auto comp: uniComps)
							logComponentInfo(cc.comp_desc(comp.id()));
					}
				}
			}

			static void diag_graph_info(const Archetype& archetype) {
				archetype.m_graph.diag();
			}

			static void diag_chunk_info(const Archetype& archetype) {
				auto logChunks = [](const auto& chunks) {
					GAIA_EACH(chunks) {
						const auto* pChunk = chunks[i];
						pChunk->diag((uint32_t)i);
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
			static void diag(const Archetype& archetype) {
				diag_basic_info(archetype);
				diag_graph_info(archetype);
				diag_chunk_info(archetype);
			}
		};

		class ArchetypeLookupKey final {
			Archetype::LookupHash m_hash;
			const ArchetypeBase* m_pArchetypeBase;

		public:
			static constexpr bool IsDirectHashKey = true;

			ArchetypeLookupKey(): m_hash({0}), m_pArchetypeBase(nullptr) {}
			ArchetypeLookupKey(Archetype::LookupHash hash, const ArchetypeBase* pArchetypeBase):
					m_hash(hash), m_pArchetypeBase(pArchetypeBase) {}

			size_t hash() const {
				return (size_t)m_hash.hash;
			}

			bool operator==(const ArchetypeLookupKey& other) const {
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
	} // namespace ecs
} // namespace gaia
