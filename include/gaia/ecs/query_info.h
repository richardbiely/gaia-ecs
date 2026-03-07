#pragma once
#include "gaia/config/config.h"

#include "gaia/cnt/darray.h"
#include "gaia/cnt/ilist.h"
#include "gaia/cnt/set.h"
#include "gaia/config/profiler.h"
#include "gaia/core/hashing_policy.h"
#include "gaia/core/utility.h"
#include "gaia/ecs/api.h"
#include "gaia/ecs/archetype.h"
#include "gaia/ecs/archetype_common.h"
#include "gaia/ecs/component.h"
#include "gaia/ecs/component_cache.h"
#include "gaia/ecs/id.h"
#include "gaia/ecs/query_common.h"
#include "gaia/ecs/vm.h"
#include "gaia/mem/mem_utils.h"

namespace gaia {
	namespace ecs {
		struct Entity;
		class World;

		using EntityToArchetypeMap = cnt::map<EntityLookupKey, ArchetypeDArray>;
		struct ArchetypeCacheData {
			GroupId groupId = 0;
			uint8_t indices[ChunkHeader::MAX_COMPONENTS];
		};

		struct QueryInfoCreationCtx {
			QueryCtx* pQueryCtx;
			const EntityToArchetypeMap* pEntityToArchetypeMap;
			std::span<const Archetype*> allArchetypes;
		};

		class QueryInfo: public cnt::ilist_item {
		public:
			//! Query matching result
			enum class MatchArchetypeQueryRet : uint8_t { Fail, Ok, Skip };

		private:
			struct Instruction {
				Entity id;
				QueryOpKind op;
			};

			struct SortData {
				Chunk* pChunk;
				uint32_t archetypeIdx;
				uint16_t startRow;
				uint16_t count;
			};

			struct GroupData {
				GroupId groupId;
				uint32_t idxFirst;
				uint32_t idxLast;
				bool needsSorting;
			};

		public:
			enum class InvalidationKind : uint8_t {
				//! Only the final result cache is stale. Structural seed matches remain valid and can be reused.
				Result,
				//! Structural seed matches are stale. This also implies the final result cache must be rebuilt.
				Seed,
				//! Full invalidation of all query cache state.
				All
			};

		private:
			struct QueryPlan {
				QueryCtx ctx;
				vm::VirtualMachine vm;
			};

			struct QueryState {
				enum DirtyFlags : uint8_t { Clean = 0x00, Seed = 0x01, Result = 0x02, All = Seed | Result };

				//! Structural seed cache built without source/variable refinement.
				cnt::set<const Archetype*> seedArchetypeSet;
				CArchetypeDArray seedArchetypeCache;
				cnt::darray<ArchetypeCacheData> seedArchetypeCacheData;

				//! Used to make sure only unique archetypes are inserted into the cache
				//! TODO: Get rid of the set by changing the way the caching works.
				cnt::set<const Archetype*> archetypeSet;
				//! Cached array of archetypes matching the query
				CArchetypeDArray archetypeCache;
				//! Cached array of query-specific data
				cnt::darray<ArchetypeCacheData> archetypeCacheData;

				//! Sort data used by cache
				cnt::darray<SortData> archetypeSortData;
				//! Group data used by cache
				cnt::darray<GroupData> archetypeGroupData;

				//! Id of the last archetype in the world we checked
				ArchetypeId lastArchetypeId{};
				//! Version of the world for which the query has been called most recently
				uint32_t worldVersion{};
				uint8_t dirtyFlags = DirtyFlags::All;

				void clear_seed_cache() {
					seedArchetypeSet = {};
					seedArchetypeCache = {};
					seedArchetypeCacheData = {};
				}

				void clear_result_cache() {
					archetypeSet = {};
					archetypeCache = {};
					archetypeSortData = {};
					archetypeCacheData = {};
					archetypeGroupData = {};
				}

				void clear_cache() {
					clear_seed_cache();
					clear_result_cache();
				}

				void reset() {
					clear_cache();
					lastArchetypeId = 0;
					dirtyFlags = DirtyFlags::All;
				}

				void invalidate_seed() {
					dirtyFlags = (uint8_t)(dirtyFlags | DirtyFlags::Seed | DirtyFlags::Result);
				}

				void invalidate_result() {
					dirtyFlags = (uint8_t)(dirtyFlags | DirtyFlags::Result);
				}

				void invalidate_all() {
					dirtyFlags = DirtyFlags::All;
				}

				GAIA_NODISCARD bool seed_dirty() const {
					return (dirtyFlags & DirtyFlags::Seed) != 0;
				}

				GAIA_NODISCARD bool result_dirty() const {
					return (dirtyFlags & DirtyFlags::Result) != 0;
				}

				GAIA_NODISCARD bool needs_refresh() const {
					return seed_dirty() || result_dirty();
				}

				void clear_dirty() {
					dirtyFlags = DirtyFlags::Clean;
				}
			};

			uint32_t m_refs = 0;

			QueryPlan m_plan;
			QueryState m_state;

			enum QueryCmdType : uint8_t { ALL, OR, NOT };

			void reset_matching_cache() {
				m_state.reset();

				auto& ctxData = m_plan.ctx.data;
				ctxData.lastMatchedArchetypeIdx_All = {};
				ctxData.lastMatchedArchetypeIdx_Or = {};
				ctxData.lastMatchedArchetypeIdx_Not = {};
			}

			void clear_result_cache() {
				m_state.clear_result_cache();
			}

			GAIA_NODISCARD bool has_dynamic_terms() const {
				const auto flags = m_plan.ctx.data.flags;
				return (flags & (QueryCtx::QueryFlags::HasSourceTerms | QueryCtx::QueryFlags::HasVariableTerms)) != 0U;
			}

			template <typename TType>
			GAIA_NODISCARD bool has_inter([[maybe_unused]] QueryOpKind op, bool isReadWrite) const {
				using T = core::raw_t<TType>;

				if constexpr (std::is_same_v<T, Entity>) {
					// Entities are read-only.
					GAIA_ASSERT(!isReadWrite);
					// Skip Entity input args. Entities are always there.
					return true;
				} else {
					Entity id;

					if constexpr (is_pair<T>::value) {
						const auto rel = m_plan.ctx.cc->get<typename T::rel>().entity;
						const auto tgt = m_plan.ctx.cc->get<typename T::tgt>().entity;
						id = (Entity)Pair(rel, tgt);
					} else {
						id = m_plan.ctx.cc->get<T>().entity;
					}

					const auto& ctxData = m_plan.ctx.data;
					const auto compIdx = comp_idx<MAX_ITEMS_IN_QUERY>(ctxData._terms.data(), id, EntityBad);

					if (op != ctxData._terms[compIdx].op)
						return false;

					// Read-write mask must match
					const uint32_t maskRW = (uint32_t)ctxData.readWriteMask & (1U << compIdx);
					const uint32_t maskXX = (uint32_t)isReadWrite << compIdx;
					return maskRW == maskXX;
				}
			}

			template <typename T>
			GAIA_NODISCARD bool has_inter(QueryOpKind op) const {
				// static_assert(is_raw_v<<T>, "has() must be used with raw types");
				constexpr bool isReadWrite = core::is_mut_v<T>;
				return has_inter<T>(op, isReadWrite);
			}

		public:
			void add_ref() {
				++m_refs;
				GAIA_ASSERT(m_refs != 0);
			}

			void dec_ref() {
				GAIA_ASSERT(m_refs > 0);
				--m_refs;
			}

			uint32_t refs() const {
				return m_refs;
			}

			void init(World* world) {
				m_plan.ctx.w = world;
			}

			void reset() {
				reset_matching_cache();
			}

			void invalidate(InvalidationKind kind = InvalidationKind::All) {
				switch (kind) {
					case InvalidationKind::Result:
						m_state.invalidate_result();
						break;
					case InvalidationKind::Seed:
						m_state.invalidate_seed();
						break;
					case InvalidationKind::All:
						m_state.invalidate_all();
						break;
				}
			}

			void invalidate_seed() {
				invalidate(InvalidationKind::Seed);
			}

			void invalidate_result() {
				invalidate(InvalidationKind::Result);
			}

			GAIA_NODISCARD static QueryInfo create(
					QueryId id, QueryCtx&& ctx, const EntityToArchetypeMap& entityToArchetypeMap,
					std::span<const Archetype*> allArchetypes) {
				// Make sure query items are sorted
				sort(ctx);

				QueryInfo info;
				info.idx = id;
				info.data.gen = 0;

				info.m_plan.ctx = GAIA_MOV(ctx);
				info.m_plan.ctx.q.handle = {id, 0};

				// Compile the query
				info.compile(entityToArchetypeMap, allArchetypes);

				return info;
			}

			GAIA_NODISCARD static QueryInfo create(uint32_t idx, uint32_t gen, void* pCtx) {
				auto* pCreationCtx = (QueryInfoCreationCtx*)pCtx;
				auto& queryCtx = *pCreationCtx->pQueryCtx;
				auto& entityToArchetypeMap = (EntityToArchetypeMap&)*pCreationCtx->pEntityToArchetypeMap;

				// Make sure query items are sorted
				sort(queryCtx);

				QueryInfo info;
				info.idx = idx;
				info.data.gen = gen;

				info.m_plan.ctx = GAIA_MOV(queryCtx);
				info.m_plan.ctx.q.handle = {idx, gen};

				// Compile the query
				info.compile(entityToArchetypeMap, pCreationCtx->allArchetypes);

				return info;
			}

			GAIA_NODISCARD static QueryHandle handle(const QueryInfo& info) {
				return QueryHandle(info.idx, info.data.gen);
			}

			//! Compile the query terms into a form we can easily process
			void compile(const EntityToArchetypeMap& entityToArchetypeMap, std::span<const Archetype*> allArchetypes) {
				GAIA_PROF_SCOPE(queryinfo::compile);

				// Compile the opcodes
				m_plan.vm.compile(entityToArchetypeMap, allArchetypes, m_plan.ctx);
			}

			//! Recompile the query
			void recompile() {
				GAIA_PROF_SCOPE(queryinfo::recompile);

				// Compile the opcodes
				m_plan.vm.create_opcodes(m_plan.ctx);
			}

			void set_world_version(uint32_t version) {
				m_state.worldVersion = version;
			}

			GAIA_NODISCARD uint32_t world_version() const {
				return m_state.worldVersion;
			}

			GAIA_NODISCARD bool operator==(const QueryCtx& other) const {
				return m_plan.ctx == other;
			}

			GAIA_NODISCARD bool operator!=(const QueryCtx& other) const {
				return m_plan.ctx != other;
			}

			// Global temporary buffers for collecting archetypes that match a query.
			// TODO: This means queries have to run from the main thread.
			//       We could make these thread_local but that does not work on all platforms. Replace with per-world cachce.
			static inline cnt::set<const Archetype*> s_tmpArchetypeMatchesSet;
			static inline cnt::darr<const Archetype*> s_tmpArchetypeMatchesArr;
			static inline cnt::sparse_storage<ArchetypeMatchStamp> s_tmpArchetypeMatchStamps;
			static inline uint32_t s_tmpArchetypeMatchEpoch = 0;

			static uint32_t next_archetype_match_epoch() {
				++s_tmpArchetypeMatchEpoch;
				if (s_tmpArchetypeMatchEpoch != 0)
					return s_tmpArchetypeMatchEpoch;

				// Overflow: drop stamps so epoch value can be reused safely.
				s_tmpArchetypeMatchStamps.clear();
				s_tmpArchetypeMatchEpoch = 1;
				return s_tmpArchetypeMatchEpoch;
			}

			struct CleanUpTmpArchetypeMatches {
				CleanUpTmpArchetypeMatches() = default;
				CleanUpTmpArchetypeMatches(const CleanUpTmpArchetypeMatches&) = delete;
				CleanUpTmpArchetypeMatches(CleanUpTmpArchetypeMatches&&) = delete;
				CleanUpTmpArchetypeMatches& operator=(const CleanUpTmpArchetypeMatches&) = delete;
				CleanUpTmpArchetypeMatches& operator=(CleanUpTmpArchetypeMatches&&) = delete;

				~CleanUpTmpArchetypeMatches() {
					// When the scope ends, we clear the arrays.
					// Note, no memory is released. Allocated capacity remains unchanged
					// because we do not want to kill time with allocating memory all the time.
					s_tmpArchetypeMatchesSet.clear();
					s_tmpArchetypeMatchesArr.clear();
				}
			};

			//! Tries to match the query against archetypes in @a entityToArchetypeMap.
			//! This is necessary so we do not iterate all chunks over and over again when running queries.
			//! \param entityToArchetypeMap Map of all archetypes
			//! \param allArchetypes List of all archetypes
			//! \param archetypeLastId Last recorded archetype id
			//! \warning Not thread safe. No two threads can call this at the same time.
			void match(
					// entity -> archetypes mapping
					const EntityToArchetypeMap& entityToArchetypeMap,
					// all archetypes in the world
					std::span<const Archetype*> allArchetypes,
					// last matched archetype id
					ArchetypeId archetypeLastId) {
				CleanUpTmpArchetypeMatches autoCleanup;

				auto& ctxData = m_plan.ctx.data;

				// Recompile if necessary
				if ((ctxData.flags & QueryCtx::QueryFlags::Recompile) != 0)
					recompile();

				// Skip if nothing has been compiled.
				if (!m_plan.vm.is_compiled())
					return;

				const bool hasDynamicTerms = has_dynamic_terms();
				if (hasDynamicTerms) {
					// Source lookups can change query results without creating new archetypes.
					// Variable terms can do the same. Rebuild the cache from scratch on each match call.
					reset_matching_cache();
				} else if (m_state.seed_dirty()) {
					reset_matching_cache();
				} else if (m_state.result_dirty()) {
					sync_result_cache_from_seed_cache();
					if (m_state.lastArchetypeId == archetypeLastId) {
						sort_entities();
						sort_cache_groups();
						m_state.clear_dirty();
						return;
					}
				}

				// Skip if no new archetype appeared
				GAIA_ASSERT(archetypeLastId >= m_state.lastArchetypeId);
				if (!hasDynamicTerms && !m_state.needs_refresh() && m_state.lastArchetypeId == archetypeLastId) {
					// Sort entities if necessary
					sort_entities();
					return;
				}

				m_state.lastArchetypeId = archetypeLastId;

				GAIA_PROF_SCOPE(queryinfo::match);

				// Prepare the context
				vm::MatchingCtx ctx{};
				ctx.pWorld = world();
				// ctx.targetEntities = {};
				ctx.allArchetypes = allArchetypes;
				ctx.pEntityToArchetypeMap = &entityToArchetypeMap;
				ctx.pMatchesArr = &s_tmpArchetypeMatchesArr;
				ctx.pMatchesSet = &s_tmpArchetypeMatchesSet;
				ctx.pMatchesStampByArchetypeId = &s_tmpArchetypeMatchStamps;
				ctx.matchesEpoch = next_archetype_match_epoch();
				ctx.pLastMatchedArchetypeIdx_All = &ctxData.lastMatchedArchetypeIdx_All;
				ctx.pLastMatchedArchetypeIdx_Or = &ctxData.lastMatchedArchetypeIdx_Or;
				ctx.pLastMatchedArchetypeIdx_Not = &ctxData.lastMatchedArchetypeIdx_Not;
				ctx.queryMask = ctxData.queryMask;
				ctx.as_mask_0 = ctxData.as_mask_0;
				ctx.as_mask_1 = ctxData.as_mask_1;
				ctx.flags = ctxData.flags;
				ctx.varBindings = ctxData.varBindings;
				ctx.varBindingMask = ctxData.varBindingMask;

				// Run the virtual machine
				m_plan.vm.exec(ctx);

				// Write found matches to cache
				for (const auto* pArchetype: *ctx.pMatchesArr) {
					if (hasDynamicTerms) {
						add_archetype_to_cache(pArchetype);
					} else {
						add_archetype_to_seed_cache(pArchetype);
						add_archetype_to_cache(pArchetype);
					}
				}

				// Sort entities if necessary
				sort_entities();
				// Sort cache groups if necessary
				sort_cache_groups();
				m_state.clear_dirty();
			}

			//! Tries to match the query against the provided archetype.
			//! This is necessary so we do not iterate all chunks over and over again when running queries.
			//! \param archetype Archtype to match
			//! \param targetEntities Entities related to the matched archetype
			//! \warning Not thread safe. No two threads can call this at the same time.
			void match_one(const Archetype& archetype, EntitySpan targetEntities) {
				CleanUpTmpArchetypeMatches autoCleanup;

				auto& ctxData = m_plan.ctx.data;

				// Recompile if necessary
				if ((ctxData.flags & QueryCtx::QueryFlags::Recompile) != 0)
					recompile();

				// Skip if nothing has been compiled.
				if (!m_plan.vm.is_compiled())
					return;

				const bool hasDynamicTerms = has_dynamic_terms();
				if (hasDynamicTerms || m_state.seed_dirty()) {
					// Source lookups can invalidate previously cached archetype matches.
					// Variable terms can invalidate them as well.
					reset_matching_cache();
				} else if (m_state.result_dirty()) {
					sync_result_cache_from_seed_cache();
				}

				GAIA_PROF_SCOPE(queryinfo::match1);

				// Prepare the context
				vm::MatchingCtx ctx{};
				ctx.pWorld = world();
				ctx.targetEntities = targetEntities;
				const auto* pArchetype = &archetype;
				ctx.allArchetypes = std::span((const Archetype**)&pArchetype, 1);
				ctx.pEntityToArchetypeMap = nullptr;
				ctx.pMatchesArr = &s_tmpArchetypeMatchesArr;
				ctx.pMatchesSet = &s_tmpArchetypeMatchesSet;
				ctx.pMatchesStampByArchetypeId = &s_tmpArchetypeMatchStamps;
				ctx.matchesEpoch = next_archetype_match_epoch();
				ctx.pLastMatchedArchetypeIdx_All = &ctxData.lastMatchedArchetypeIdx_All;
				ctx.pLastMatchedArchetypeIdx_Or = &ctxData.lastMatchedArchetypeIdx_Or;
				ctx.pLastMatchedArchetypeIdx_Not = &ctxData.lastMatchedArchetypeIdx_Not;
				ctx.queryMask = ctxData.queryMask;
				ctx.as_mask_0 = ctxData.as_mask_0;
				ctx.as_mask_1 = ctxData.as_mask_1;
				ctx.flags = ctxData.flags;
				ctx.varBindings = ctxData.varBindings;
				ctx.varBindingMask = ctxData.varBindingMask;

				// Run the virtual machine
				m_plan.vm.exec(ctx);

				// Write found matches to cache
				for (const auto* pArch: *ctx.pMatchesArr) {
					if (hasDynamicTerms) {
						add_archetype_to_cache(pArch);
					} else {
						add_archetype_to_seed_cache(pArch);
						add_archetype_to_cache(pArch);
					}
				}
				m_state.clear_dirty();
			}

			void ensure_matches(
					const EntityToArchetypeMap& entityToArchetypeMap, std::span<const Archetype*> allArchetypes,
					ArchetypeId archetypeLastId) {
				match(entityToArchetypeMap, allArchetypes, archetypeLastId);
			}

			void ensure_matches_one(const Archetype& archetype, EntitySpan targetEntities) {
				match_one(archetype, targetEntities);
			}

			//! Calculates the sort data for the archetypes in the cache.
			//! This allows us to iterate entites in the order they are sorted across all archetypes.
			void calculate_sort_data() {
				GAIA_PROF_SCOPE(queryinfo::calc_sort_data);

				m_state.archetypeSortData.clear();

				// The function doesn't do any moves and expects that all chunks have their data sorted already.
				// We use a min-heap / priority queue - like structure during query iteration to merge sorted tables:
				// - we hold a cursor into each sorted chunk
				// - we compare the next entity from each chunk using your sorting function
				// - we then pick the smallest one (like k-way merge sort), and advance that cursor
				// This is esentially what this function does:
				// while (any_chunk_has_entities) {
				// 	find_chunk_with_smallest_next_element();
				// 	yield(entity_from_that_chunk);
				// 	advance_cursor_for_that_chunk();
				// }
				// This produces a globally sorted view without modifying actual data. It's a balance between
				// performance and memory usage. We could also sort the data in-place across all chunks, but that
				// would generated too many data moves (entities + all of their components).

				struct Cursor {
					uint32_t chunkIdx = 0;
					uint16_t row = 0;
				};

				auto& archetypes = m_state.archetypeCache;

				// Initialize cursors. We will need as many as there are archetypes.
				cnt::sarray_ext<Cursor, 128> cursors(archetypes.size());

				uint32_t currArchetypeIdx = (uint32_t)-1;
				Chunk* pCurrentChunk = nullptr;
				uint16_t currentStartRow = 0;
				uint16_t currentRow = 0;

				const void* pDataMin = nullptr;
				const void* pDataCurr = nullptr;

				while (true) {
					uint32_t minArchetypeIdx = (uint32_t)-1;
					Entity minEntity = EntityBad;

					// Find the next entity across all tables/chunks
					for (uint32_t t = 0; t < archetypes.size(); ++t) {
						const auto* pArchetype = archetypes[t];
						const auto& chunks = pArchetype->chunks();
						auto& cur = cursors[t];

						while (cur.chunkIdx < chunks.size() && cur.row >= chunks[cur.chunkIdx]->size()) {
							++cur.chunkIdx;
							cur.row = 0;
						}

						if (cur.chunkIdx >= chunks.size())
							continue;

						const auto* pChunk = pArchetype->chunks()[cur.chunkIdx];
						auto entity = pChunk->entity_view()[cur.row];

						if (m_plan.ctx.data.sortBy != ecs::EntityBad) {
							const auto compIdx = pChunk->comp_idx(m_plan.ctx.data.sortBy);
							pDataCurr = pChunk->comp_ptr(compIdx, cur.row);
						} else
							pDataCurr = &pChunk->entity_view()[cur.row];

						if (minEntity == EntityBad) {
							minEntity = entity;
							minArchetypeIdx = t;
							pDataMin = pDataCurr;
							continue;
						}

						if (m_plan.ctx.data.sortByFunc(*m_plan.ctx.w, pDataCurr, pDataMin) < 0) {
							minEntity = entity;
							minArchetypeIdx = t;
						}
					}

					// No more results found, we can stop
					if (minArchetypeIdx == (uint32_t)-1)
						break;

					auto& cur = cursors[minArchetypeIdx];
					const auto& chunks = archetypes[minArchetypeIdx]->chunks();
					Chunk* pChunk = chunks[cur.chunkIdx];

					if (minArchetypeIdx == currArchetypeIdx && pChunk == pCurrentChunk) {
						// Current slice
					} else {
						// End previous slice
						if (pCurrentChunk != nullptr) {
							m_state.archetypeSortData.push_back(
									{pCurrentChunk, currArchetypeIdx, currentStartRow, (uint16_t)(currentRow - currentStartRow)});
						}

						// Start a new slice
						currArchetypeIdx = minArchetypeIdx;
						pCurrentChunk = pChunk;
						currentStartRow = cur.row;
					}

					++cur.row;
					currentRow = cur.row;
				}

				if (pCurrentChunk != nullptr) {
					m_state.archetypeSortData.push_back(
							{pCurrentChunk, currArchetypeIdx, currentStartRow, (uint16_t)(currentRow - currentStartRow)});
				}
			}

			void sort_entities() {
				if (m_plan.ctx.data.sortByFunc == nullptr)
					return;

				if ((m_plan.ctx.data.flags & QueryCtx::QueryFlags::SortEntities) == 0) {
					// TODO: We need observers to implement that would listen to entity movements within chunks.
					//       thanks to that we would know right away if some movement happenend without
					//       having to check this constantly.
					bool hasChanged = false;
					for (const auto* pArchetype: m_state.archetypeCache) {
						const auto& chunks = pArchetype->chunks();
						for (const auto* pChunk: chunks) {
							if (pChunk->changed(m_state.worldVersion)) {
								hasChanged = true;
								break;
							}
						}
					}
					if (!hasChanged)
						return;
				}
				m_plan.ctx.data.flags ^= QueryCtx::QueryFlags::SortEntities;

				// First, sort entities in archetypes
				for (const auto* pArchetype: m_state.archetypeCache)
					const_cast<Archetype*>(pArchetype)->sort_entities(m_plan.ctx.data.sortBy, m_plan.ctx.data.sortByFunc);

				// Now that entites are sorted, we can start creating slices
				calculate_sort_data();
			}

			void sort_cache_groups() {
				if ((m_plan.ctx.data.flags & QueryCtx::QueryFlags::SortGroups) == 0)
					return;
				m_plan.ctx.data.flags ^= QueryCtx::QueryFlags::SortGroups;

				struct sort_cond {
					bool operator()(const ArchetypeCacheData& a, const ArchetypeCacheData& b) const {
						return a.groupId <= b.groupId;
					}
				};

				// Archetypes in cache are ordered by groupId. Adding a new archetype
				// possibly means rearranging the existing ones.
				// 2 2 3 3 3 3 4 4 4 [2]
				// -->
				// 2 2 [2] 3 3 3 3 4 4 4
				core::sort(m_state.archetypeCacheData, sort_cond{}, [&](uint32_t left, uint32_t right) {
					auto* pTmpArchetype = m_state.archetypeCache[left];
					m_state.archetypeCache[left] = m_state.archetypeCache[right];
					m_state.archetypeCache[right] = pTmpArchetype;

					auto tmp = m_state.archetypeCacheData[left];
					m_state.archetypeCacheData[left] = m_state.archetypeCacheData[right];
					m_state.archetypeCacheData[right] = tmp;
				});
			}

			ArchetypeCacheData create_cache_data(const Archetype* pArchetype) {
				ArchetypeCacheData cacheData;
				auto queryIds = ctx().data.ids_view();
				const auto cnt = (uint32_t)queryIds.size();
				GAIA_FOR(cnt) {
					const auto idxBeforeRemapping = m_plan.ctx.data._remapping[i];
					const auto queryId = queryIds[idxBeforeRemapping];
					// compIdx can be -1. We are fine with it because the user should never ask for something
					// that is not present on the archetype. If they do, they made a mistake.
					const auto compIdx = core::get_index_unsafe(pArchetype->ids_view(), queryId);
					GAIA_ASSERT(compIdx != BadIndex);

					cacheData.indices[i] = (uint8_t)compIdx;
				}
				return cacheData;
			}

			void add_archetype_to_cache_no_grouping(const Archetype* pArchetype) {
				GAIA_PROF_SCOPE(queryinfo::add_cache_ng);

				if (m_state.archetypeSet.contains(pArchetype))
					return;

				m_state.archetypeSet.emplace(pArchetype);
				m_state.archetypeCache.push_back(pArchetype);
				m_state.archetypeCacheData.push_back(create_cache_data(pArchetype));
			}

			void add_archetype_to_seed_cache(const Archetype* pArchetype) {
				if (m_state.seedArchetypeSet.contains(pArchetype))
					return;

				m_state.seedArchetypeSet.emplace(pArchetype);
				m_state.seedArchetypeCache.push_back(pArchetype);
				m_state.seedArchetypeCacheData.push_back(create_cache_data(pArchetype));
			}

			void add_archetype_to_cache_w_grouping(const Archetype* pArchetype) {
				GAIA_PROF_SCOPE(queryinfo::add_cache_wg);

				if (m_state.archetypeSet.contains(pArchetype))
					return;

				const GroupId groupId = m_plan.ctx.data.groupByFunc(*m_plan.ctx.w, *pArchetype, m_plan.ctx.data.groupBy);

				ArchetypeCacheData cacheData = create_cache_data(pArchetype);
				cacheData.groupId = groupId;

				if (m_state.archetypeGroupData.empty()) {
					m_state.archetypeGroupData.push_back({groupId, 0, 0, false});
				} else {
					const auto cnt = m_state.archetypeGroupData.size();
					GAIA_FOR(cnt) {
						if (groupId < m_state.archetypeGroupData[i].groupId) {
							// Insert the new group before one with a lower groupId.
							// 2 3 5 10 20 25 [7]<-new group
							// -->
							// 2 3 5 [7] 10 20 25
							m_state.archetypeGroupData.insert(
									m_state.archetypeGroupData.begin() + i,
									{groupId, m_state.archetypeGroupData[i].idxFirst, m_state.archetypeGroupData[i].idxFirst, false});
							const auto lastGrpIdx = m_state.archetypeGroupData.size();

							// Update ranges
							for (uint32_t j = i + 1; j < lastGrpIdx; ++j) {
								++m_state.archetypeGroupData[j].idxFirst;
								++m_state.archetypeGroupData[j].idxLast;
							}

							// Resort groups
							m_plan.ctx.data.flags |= QueryCtx::QueryFlags::SortGroups;
							goto groupWasFound;
						} else if (m_state.archetypeGroupData[i].groupId == groupId) {
							const auto lastGrpIdx = m_state.archetypeGroupData.size();
							++m_state.archetypeGroupData[i].idxLast;

							// Update ranges
							for (uint32_t j = i + 1; j < lastGrpIdx; ++j) {
								++m_state.archetypeGroupData[j].idxFirst;
								++m_state.archetypeGroupData[j].idxLast;
								m_plan.ctx.data.flags |= QueryCtx::QueryFlags::SortGroups;
							}

							goto groupWasFound;
						}
					}

					{
						// We have a new group
						const auto groupsCnt = m_state.archetypeGroupData.size();
						if (groupsCnt == 0) {
							// No groups exist yet, the range is {0 .. 0}
							m_state.archetypeGroupData.push_back( //
									{groupId, 0, 0, false});
						} else {
							const auto& groupPrev = m_state.archetypeGroupData[groupsCnt - 1];
							GAIA_ASSERT(groupPrev.idxLast + 1 == m_state.archetypeCache.size());
							// The new group starts where the old one ends
							m_state.archetypeGroupData.push_back(
									{groupId, //
									 groupPrev.idxLast + 1, //
									 groupPrev.idxLast + 1, //
									 false});
						}
					}

				groupWasFound:;
				}

				m_state.archetypeSet.emplace(pArchetype);
				m_state.archetypeCache.push_back(pArchetype);
				m_state.archetypeCacheData.push_back(GAIA_MOV(cacheData));
			}

			void add_archetype_to_cache(const Archetype* pArchetype) {
				if (m_plan.ctx.data.sortByFunc != nullptr)
					m_plan.ctx.data.flags |= QueryCtx::QueryFlags::SortEntities;

				if (m_plan.ctx.data.groupBy != EntityBad)
					add_archetype_to_cache_w_grouping(pArchetype);
				else
					add_archetype_to_cache_no_grouping(pArchetype);
			}

			void sync_result_cache_from_seed_cache() {
				clear_result_cache();
				const auto cnt = (uint32_t)m_state.seedArchetypeCache.size();
				GAIA_FOR(cnt)
				add_archetype_to_cache(m_state.seedArchetypeCache[i]);
			}

			bool del_archetype_from_cache(const Archetype* pArchetype) {
				const auto it = m_state.archetypeSet.find(pArchetype);
				if (it == m_state.archetypeSet.end())
					return false;
				m_state.archetypeSet.erase(it);

				if (m_plan.ctx.data.sortByFunc != nullptr)
					m_plan.ctx.data.flags |= QueryCtx::QueryFlags::SortEntities;

				const auto archetypeIdx = core::get_index_unsafe(m_state.archetypeCache, pArchetype);
				GAIA_ASSERT(archetypeIdx != BadIndex);

				core::swap_erase(m_state.archetypeCache, archetypeIdx);
				core::swap_erase(m_state.archetypeCacheData, archetypeIdx);

				// Update the group data if possible
				if (m_plan.ctx.data.groupBy != EntityBad) {
					const auto groupId = m_plan.ctx.data.groupByFunc(*m_plan.ctx.w, *pArchetype, m_plan.ctx.data.groupBy);
					const auto grpIdx = core::get_index_if_unsafe(m_state.archetypeGroupData, [&](const GroupData& group) {
						return group.groupId == groupId;
					});
					GAIA_ASSERT(grpIdx != BadIndex);

					auto& currGrp = m_state.archetypeGroupData[archetypeIdx];

					// Update ranges
					const auto lastGrpIdx = m_state.archetypeGroupData.size();
					for (uint32_t j = grpIdx + 1; j < lastGrpIdx; ++j) {
						--m_state.archetypeGroupData[j].idxFirst;
						--m_state.archetypeGroupData[j].idxLast;
					}

					// Handle the current group. If it's about to be left empty, delete it.
					if (currGrp.idxLast - currGrp.idxFirst > 0)
						--currGrp.idxLast;
					else
						m_state.archetypeGroupData.erase(m_state.archetypeGroupData.begin() + grpIdx);
				}

				return true;
			}

			bool del_archetype_from_seed_cache(const Archetype* pArchetype) {
				const auto it = m_state.seedArchetypeSet.find(pArchetype);
				if (it == m_state.seedArchetypeSet.end())
					return false;
				m_state.seedArchetypeSet.erase(it);

				const auto archetypeIdx = core::get_index_unsafe(m_state.seedArchetypeCache, pArchetype);
				GAIA_ASSERT(archetypeIdx != BadIndex);
				core::swap_erase(m_state.seedArchetypeCache, archetypeIdx);
				core::swap_erase(m_state.seedArchetypeCacheData, archetypeIdx);
				return true;
			}

			GAIA_NODISCARD World* world() {
				GAIA_ASSERT(m_plan.ctx.w != nullptr);
				return const_cast<World*>(m_plan.ctx.w);
			}
			GAIA_NODISCARD const World* world() const {
				GAIA_ASSERT(m_plan.ctx.w != nullptr);
				return m_plan.ctx.w;
			}

			GAIA_NODISCARD QuerySerBuffer& ser_buffer() {
				return m_plan.ctx.q.ser_buffer(world());
			}
			void ser_buffer_reset() {
				m_plan.ctx.q.ser_buffer_reset(world());
			}

			GAIA_NODISCARD QueryCtx& ctx() {
				return m_plan.ctx;
			}
			GAIA_NODISCARD const QueryCtx& ctx() const {
				return m_plan.ctx;
			}

			GAIA_NODISCARD util::str bytecode() const {
				return m_plan.vm.bytecode(*world());
			}

			GAIA_NODISCARD uint32_t op_count() const {
				return m_plan.vm.op_count();
			}

			GAIA_NODISCARD uint64_t op_signature() const {
				return m_plan.vm.op_signature();
			}

			GAIA_NODISCARD bool has_filters() const {
				return m_plan.ctx.data.changedCnt > 0;
			}

			template <typename... T>
			GAIA_NODISCARD bool has_any() const {
				return (has_inter<T>(QueryOpKind::Any) || ...);
			}

			template <typename... T>
			GAIA_NODISCARD bool has_or() const {
				return (has_inter<T>(QueryOpKind::Or) || ...);
			}

			template <typename... T>
			GAIA_NODISCARD bool has_all() const {
				return (has_inter<T>(QueryOpKind::All) && ...);
			}

			template <typename... T>
			GAIA_NODISCARD bool has_no() const {
				return (!has_inter<T>(QueryOpKind::Not) && ...);
			}

			//! Removes an archetype from cache
			//! \param pArchetype Archetype to remove
			void remove(Archetype* pArchetype) {
				GAIA_PROF_SCOPE(queryinfo::remove);

				const bool removedFromSeed = del_archetype_from_seed_cache(pArchetype);
				const bool removedFromResult = del_archetype_from_cache(pArchetype);
				if (!removedFromSeed && !removedFromResult)
					return;

				// An archetype was removed from the world so the last matching archetype index needs to be
				// lowered by one for every component context.
				auto clearMatches = [](QueryArchetypeCacheIndexMap& matches) {
					for (auto& pair: matches) {
						auto& lastMatchedArchetypeIdx = pair.second;
						if (lastMatchedArchetypeIdx > 0)
							--lastMatchedArchetypeIdx;
					}
				};
				clearMatches(m_plan.ctx.data.lastMatchedArchetypeIdx_All);
				clearMatches(m_plan.ctx.data.lastMatchedArchetypeIdx_Or);
				clearMatches(m_plan.ctx.data.lastMatchedArchetypeIdx_Not);
			}

			//! Returns a view of indices mapping for component entities in a given archetype
			std::span<const uint8_t> indices_mapping_view(uint32_t archetypeIdx) const {
				const auto& ctxData = m_state.archetypeCacheData[archetypeIdx];
				return {(const uint8_t*)&ctxData.indices[0], ChunkHeader::MAX_COMPONENTS};
			}

			GAIA_NODISCARD CArchetypeDArray::iterator begin() {
				return m_state.archetypeCache.begin();
			}

			GAIA_NODISCARD CArchetypeDArray::const_iterator begin() const {
				return m_state.archetypeCache.begin();
			}

			GAIA_NODISCARD CArchetypeDArray::const_iterator cbegin() const {
				return m_state.archetypeCache.begin();
			}

			GAIA_NODISCARD CArchetypeDArray::iterator end() {
				return m_state.archetypeCache.end();
			}

			GAIA_NODISCARD CArchetypeDArray::const_iterator end() const {
				return m_state.archetypeCache.end();
			}

			GAIA_NODISCARD CArchetypeDArray::const_iterator cend() const {
				return m_state.archetypeCache.end();
			}

			GAIA_NODISCARD std::span<const Archetype*> cache_archetype_view() const {
				return std::span{(const Archetype**)m_state.archetypeCache.data(), m_state.archetypeCache.size()};
			}

			GAIA_NODISCARD std::span<const ArchetypeCacheData> cache_data_view() const {
				return std::span{m_state.archetypeCacheData.data(), m_state.archetypeCacheData.size()};
			}

			GAIA_NODISCARD std::span<const SortData> cache_sort_view() const {
				return std::span{m_state.archetypeSortData.data(), m_state.archetypeSortData.size()};
			}

			GAIA_NODISCARD std::span<const GroupData> group_data_view() const {
				return std::span{m_state.archetypeGroupData.data(), m_state.archetypeGroupData.size()};
			}
		};
	} // namespace ecs
} // namespace gaia
