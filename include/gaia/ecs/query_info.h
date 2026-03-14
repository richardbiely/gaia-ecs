#pragma once
#include "gaia/config/config.h"

#include "gaia/cnt/darray.h"
#include "gaia/cnt/ilist.h"
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
#include "gaia/ecs/query_match_stamps.h"
#include "gaia/ecs/vm.h"
#include "gaia/mem/mem_utils.h"

namespace gaia {
	namespace ecs {
		struct Entity;
		class World;
		uint32_t world_version(const World& world);

		using EntityToArchetypeMap = cnt::map<EntityLookupKey, ComponentIndexEntryArray>;
		struct ArchetypeCacheData {
			GroupId groupId = 0;
			uint8_t indices[ChunkHeader::MAX_COMPONENTS];
		};

		struct QueryMatchScratch {
			//! Ordered list of matched archetypes emitted by the VM for the current run.
			cnt::darr<const Archetype*> matchesArr;
			//! Paged O(1) dedup table keyed by world-local archetype ids.
			//! Pages stay allocated on the scratch frame so repeated matches do not churn
			//! heap memory when archetype ids revisit the same ranges.
			ArchetypeMatchStamps matchStamps;
			//! Monotonic dedup stamp used when the same scratch frame is reused by later
			//! full match() calls without clearing stamp pages.
			uint32_t matchVersion = 0;

			void clear_temporary_matches() {
				matchesArr.clear();
				matchStamps.clear();
				matchVersion = 0;
			}

			void clear_temporary_matches_keep_stamps() {
				//! Full match() can reuse prior stamp pages and advance matchVersion instead
				//! of zeroing the whole table again.
				matchesArr.clear();
			}

			void reset_stamps() {
				matchStamps.clear();
				matchVersion = 0;
			}

			GAIA_NODISCARD uint32_t next_match_version() {
				++matchVersion;
				if (matchVersion != 0)
					return matchVersion;

				reset_stamps();
				matchVersion = 1;
				return matchVersion;
			}
		};

		GAIA_NODISCARD QueryMatchScratch& query_match_scratch_acquire(World& world);
		void query_match_scratch_release(World& world, bool keepStamps);

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

			struct SrcTravSnapshotItem {
				Entity entity = EntityBad;
				//! Version of the source entity's archetype membership captured in the snapshot.
				uint32_t sourceVersion = 0;

				GAIA_NODISCARD bool operator==(const SrcTravSnapshotItem& other) const {
					return entity == other.entity && sourceVersion == other.sourceVersion;
				}

				GAIA_NODISCARD bool operator!=(const SrcTravSnapshotItem& other) const {
					return !operator==(other);
				}
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

				//! Used to make sure only unique archetypes are inserted into the cache.
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
				//! Cached range for the currently selected group id.
				mutable GroupData selectedGroupData{};
				//! True when selectedGroupData matches the active group filter.
				mutable bool selectedGroupDataValid = false;
				//! World version at which the sorted cache slices were last rebuilt.
				//! Unlike worldVersion, this is only updated after a real sort refresh.
				uint32_t sortVersion{};

				//! Id of the last archetype in the world we checked
				ArchetypeId lastArchetypeId{};
				//! Version of the world for which the query has been called most recently
				uint32_t worldVersion{};
				//! Last seen versions for tracked dynamic relation dependencies.
				cnt::sarray<uint32_t, MAX_ITEMS_IN_QUERY> relationVersions;
				//! Last seen archetype versions for tracked direct concrete source entities.
				cnt::sarray<uint32_t, MAX_ITEMS_IN_QUERY> directSrcEntityVersions;
				//! Last seen traversed-source closure for reusable source queries.
				cnt::darray<SrcTravSnapshotItem> srcTravSnapshot;
				//! True when the traversed source closure exceeded the configured snapshot cap.
				bool srcTravSnapshotOverflowed = false;
				//! Snapshot of runtime variable bindings used to build the current dynamic cache.
				cnt::sarray<Entity, MaxVarCnt> varBindings;
				//! Bitmask of the variable bindings captured in varBindings.
				uint8_t varBindingMask = 0;
				//! Bumped when the result archetype membership changes.
				//! QueryCache uses this to skip reverse-index resync on warm cached reads.
				uint32_t resultCacheRevision = 1;
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
					selectedGroupData = {};
					selectedGroupDataValid = false;
					sortVersion = 0;
				}

				void clear_cache() {
					clear_seed_cache();
					clear_result_cache();
				}

				void reset() {
					clear_cache();
					srcTravSnapshot.clear();
					srcTravSnapshotOverflowed = false;
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
				if (!m_state.archetypeCache.empty())
					mark_result_cache_membership_changed();

				m_state.reset();

				auto& ctxData = m_plan.ctx.data;
				ctxData.lastMatchedArchetypeIdx_All = {};
				ctxData.lastMatchedArchetypeIdx_Or = {};
				ctxData.lastMatchedArchetypeIdx_Not = {};
			}

			void clear_result_cache() {
				m_state.clear_result_cache();
			}

			void mark_result_cache_membership_changed() {
				++m_state.resultCacheRevision;
				if (m_state.resultCacheRevision != 0)
					return;

				// Reserve 0 as the "never synced" value in QueryCache.
				m_state.resultCacheRevision = 1;
			}

			//! Returns true when the query uses the dynamic cache layer.
			GAIA_NODISCARD bool has_dyn_terms() const {
				return m_plan.ctx.data.cachePolicy == QueryCtx::CachePolicy::Dynamic;
			}

			//! Returns true when the dynamic cache can be reused by checking tracked runtime inputs.
			GAIA_NODISCARD bool can_reuse_dyn_cache() const {
				if (!has_dyn_terms())
					return false;

				const auto& deps = m_plan.ctx.data.deps;
				if (!deps.has(QueryCtx::DependencyHasSourceTerms))
					return true;

				if (!deps.can_reuse_src_cache())
					return false;

				// Direct concrete-source reuse is automatic. Traversed source reuse still needs explicit snapshots.
				if (!deps.has(QueryCtx::DependencyHasTraversalTerms))
					return true;

				return m_plan.ctx.data.cacheSrcTrav != 0;
			}

			//! Direct concrete-source queries reuse per-source archetype versions without rebuilding a traversal closure.
			GAIA_NODISCARD bool uses_direct_src_version_tracking() const {
				const auto& deps = m_plan.ctx.data.deps;
				return !deps.has(QueryCtx::DependencyHasTraversalTerms) && //
							 can_reuse_dyn_cache();
			}

			//! Traversed-source queries reuse an explicit source-closure snapshot when opted in.
			GAIA_NODISCARD bool uses_src_trav_snapshot() const {
				const auto& deps = m_plan.ctx.data.deps;
				return deps.has(QueryCtx::DependencyHasTraversalTerms) && //
							 can_reuse_dyn_cache();
			}

			//! Checks tracked relation versions used by the dynamic cache.
			GAIA_NODISCARD bool dyn_rel_versions_changed() const {
				const auto relations = m_plan.ctx.data.deps.relations_view();
				const auto cnt = (uint32_t)relations.size();
				GAIA_FOR(cnt) {
					if (m_state.relationVersions[i] != world_rel_version(*world(), relations[i]))
						return true;
				}

				return false;
			}

			//! Checks runtime variable bindings used by the dynamic cache.
			GAIA_NODISCARD bool dyn_var_bindings_changed() const {
				const auto& ctxData = m_plan.ctx.data;
				if (m_state.varBindingMask != ctxData.varBindingMask)
					return true;

				GAIA_FOR(MaxVarCnt) {
					const auto bit = (uint8_t(1) << i);
					if ((ctxData.varBindingMask & bit) == 0)
						continue;

					if (m_state.varBindings[i] != ctxData.varBindings[i])
						return true;
				}

				return false;
			}

			//! Checks direct concrete source entities tracked through per-entity archetype versions.
			GAIA_NODISCARD bool direct_src_versions_changed() const {
				if (!uses_direct_src_version_tracking())
					return false;

				const auto& deps = m_plan.ctx.data.deps;
				const auto sourceEntities = deps.src_entities_view();
				const auto cnt = (uint32_t)sourceEntities.size();
				GAIA_FOR(cnt) {
					if (m_state.directSrcEntityVersions[i] != world_entity_archetype_version(*world(), sourceEntities[i]))
						return true;
				}

				return false;
			}

			//! Iterates reusable source entities that participate in direct-source or traversed-source reuse.
			template <typename Func>
			void each_reusable_src_entity(Func&& func) const {
				const auto terms = m_plan.ctx.data.terms_view();
				const auto cnt = (uint32_t)terms.size();
				GAIA_FOR(cnt) {
					const auto& term = terms[i];
					if (term.src == EntityBad || is_variable(term.src))
						continue;

					(void)vm::detail::each_lookup_src(*world(), term, term.src, [&](Entity source) {
						return func(source);
					});
				}
			}

			//! Returns thread-local scratch reused when a traversed-source closure must be rebuilt for comparison.
			GAIA_NODISCARD static cnt::darray<SrcTravSnapshotItem>& src_trav_snapshot_scratch() {
				static thread_local cnt::darray<SrcTravSnapshotItem> scratch;
				return scratch;
			}

			//! Builds the traversed-source closure snapshot.
			//! Returns false if the configured snapshot cap was exceeded.
			GAIA_NODISCARD bool build_src_trav_snapshot(cnt::darray<SrcTravSnapshotItem>& items) const {
				const auto maxItems = (uint32_t)m_plan.ctx.data.cacheSrcTrav;
				items.clear();
				if (maxItems == 0)
					return false;

				bool overflowed = false;
				each_reusable_src_entity([&](Entity source) {
					if (items.size() >= maxItems) {
						overflowed = true;
						return true;
					}
					items.push_back({source, world_entity_archetype_version(*world(), source)});
					return false;
				});

				return !overflowed;
			}

			//! Checks whether the previously snapshotted traversed source entities changed archetype membership.
			//! This avoids rebuilding the full traversal closure while relation topology is unchanged.
			GAIA_NODISCARD bool traversed_src_versions_changed() const {
				const auto cnt = (uint32_t)m_state.srcTravSnapshot.size();
				GAIA_FOR(cnt) {
					const auto& item = m_state.srcTravSnapshot[i];
					if (item.sourceVersion != world_entity_archetype_version(*world(), item.entity))
						return true;
				}

				return false;
			}

			//! Checks source-driven dynamic inputs, using direct-source versions or traversed-source snapshots as needed.
			GAIA_NODISCARD bool dyn_src_inputs_changed(bool relationVersionsChanged) {
				const auto& deps = m_plan.ctx.data.deps;
				if (!deps.has(QueryCtx::DependencyHasSourceTerms))
					return false;

				if (!deps.has(QueryCtx::DependencyHasTraversalTerms))
					return direct_src_versions_changed();

				if (m_state.srcTravSnapshotOverflowed)
					return true;

				if (!relationVersionsChanged)
					return traversed_src_versions_changed();

				auto& scratch = src_trav_snapshot_scratch();
				if (!build_src_trav_snapshot(scratch))
					return true;

				if (scratch.size() != m_state.srcTravSnapshot.size())
					return true;

				const auto cnt = (uint32_t)m_state.srcTravSnapshot.size();
				GAIA_FOR(cnt) {
					if (scratch[i] != m_state.srcTravSnapshot[i])
						return true;
				}

				return false;
			}

			//! Checks whether any tracked runtime input invalidates the reusable dynamic cache.
			GAIA_NODISCARD bool dyn_inputs_changed() {
				if (!can_reuse_dyn_cache())
					return false;

				if (dyn_var_bindings_changed())
					return true;

				const bool relationVersionsChanged = dyn_rel_versions_changed();
				if (relationVersionsChanged && !uses_src_trav_snapshot())
					return true;

				const auto& deps = m_plan.ctx.data.deps;
				if (!deps.has(QueryCtx::DependencyHasSourceTerms))
					return relationVersionsChanged;

				if (m_state.srcTravSnapshotOverflowed)
					return true;

				return dyn_src_inputs_changed(relationVersionsChanged);
			}

			//! Captures the tracked runtime inputs used by the reusable dynamic cache.
			void snapshot_dyn_inputs() {
				if (!can_reuse_dyn_cache())
					return;

				const auto relations = m_plan.ctx.data.deps.relations_view();
				const auto cnt = (uint32_t)relations.size();
				GAIA_FOR(cnt)
				m_state.relationVersions[i] = world_rel_version(*world(), relations[i]);

				const auto& deps = m_plan.ctx.data.deps;
				if (uses_direct_src_version_tracking()) {
					const auto sourceEntities = deps.src_entities_view();
					const auto sourceCnt = (uint32_t)sourceEntities.size();
					GAIA_FOR(sourceCnt)
					m_state.directSrcEntityVersions[i] = world_entity_archetype_version(*world(), sourceEntities[i]);
				}

				if (uses_src_trav_snapshot()) {
					auto& scratch = src_trav_snapshot_scratch();
					if (build_src_trav_snapshot(scratch)) {
						m_state.srcTravSnapshot = scratch;
						m_state.srcTravSnapshotOverflowed = false;
					} else {
						m_state.srcTravSnapshot.clear();
						m_state.srcTravSnapshotOverflowed = true;
					}
				} else {
					m_state.srcTravSnapshot.clear();
					m_state.srcTravSnapshotOverflowed = false;
				}

				m_state.varBindings = m_plan.ctx.data.varBindings;
				m_state.varBindingMask = m_plan.ctx.data.varBindingMask;
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

			//! Marks the cached sorted slices dirty without invalidating query membership.
			void invalidate_sort() {
				if (m_plan.ctx.data.sortByFunc != nullptr)
					m_plan.ctx.data.flags |= QueryCtx::QueryFlags::SortEntities;
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

			GAIA_NODISCARD QueryCtx::CachePolicy cache_policy() const {
				return m_plan.ctx.data.cachePolicy;
			}

			GAIA_NODISCARD uint32_t reverse_index_revision() const {
				return m_state.resultCacheRevision;
			}

			GAIA_NODISCARD bool can_update_with_new_archetype() const {
				// Only immediate structural queries participate in archetype-create propagation.
				return m_plan.vm.is_compiled() && cache_policy() == QueryCtx::CachePolicy::Immediate &&
							 !m_state.needs_refresh();
			}

			//! Returns whether create-time matching should bypass the temporary one-archetype VM path.
			GAIA_NODISCARD bool can_use_direct_create_archetype_match() const {
				return m_plan.ctx.data.createArchetypeMatchKind == QueryCtx::CreateArchetypeMatchKind::DirectAllTerms;
			}

			//! Returns whether direct create-time matching needs Is-aware id checks.
			GAIA_NODISCARD bool direct_create_archetype_match_uses_is() const {
				const auto& ctxData = m_plan.ctx.data;
				return (ctxData.as_mask_0 + ctxData.as_mask_1) != 0;
			}

			GAIA_NODISCARD bool operator==(const QueryCtx& other) const {
				return m_plan.ctx == other;
			}

			GAIA_NODISCARD bool operator!=(const QueryCtx& other) const {
				return m_plan.ctx != other;
			}

			struct CleanUpTmpArchetypeMatches {
				World& world;
				bool keepStamps;

				explicit CleanUpTmpArchetypeMatches(World& world, bool keepStamps): world(world), keepStamps(keepStamps) {}
				CleanUpTmpArchetypeMatches(const CleanUpTmpArchetypeMatches&) = delete;
				CleanUpTmpArchetypeMatches(CleanUpTmpArchetypeMatches&&) = delete;
				CleanUpTmpArchetypeMatches& operator=(const CleanUpTmpArchetypeMatches&) = delete;
				CleanUpTmpArchetypeMatches& operator=(CleanUpTmpArchetypeMatches&&) = delete;

				~CleanUpTmpArchetypeMatches() {
					query_match_scratch_release(world, keepStamps);
				}
			};

			//! Tries to match the query against archetypes in @a entityToArchetypeMap.
			//! This is necessary so we do not iterate all chunks over and over again when running queries.
			//! \param entityToArchetypeMap Lookup of archetypes by queried id
			//! \param allArchetypes List of all archetypes
			//! \param archetypeLastId Last recorded archetype id
			//! \warning Not thread safe. No two threads can call this at the same time.
			template <typename ArchetypeLookup>
			void match(
					// entity -> archetypes mapping
					const ArchetypeLookup& entityToArchetypeMap,
					// all archetypes in the world
					std::span<const Archetype*> allArchetypes,
					// last matched archetype id
					ArchetypeId archetypeLastId) {
				auto& w = *world();
				auto& matchScratch = query_match_scratch_acquire(w);
				CleanUpTmpArchetypeMatches autoCleanup(w, true);

				auto& ctxData = m_plan.ctx.data;

				// Recompile if necessary
				if ((ctxData.flags & QueryCtx::QueryFlags::Recompile) != 0)
					recompile();

				// Skip if nothing has been compiled.
				if (!m_plan.vm.is_compiled())
					return;

				const bool hasDynamicTerms = has_dyn_terms();
				if (hasDynamicTerms && (!can_reuse_dyn_cache() || m_state.needs_refresh() || dyn_inputs_changed())) {
					// Dynamic queries keep their cached result as long as tracked runtime inputs stay stable.
					// Source-based queries still take the conservative rebuild path until their inputs
					// are tracked with finer-grained version metadata.
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
				ctx.archetypeLookup = vm::make_archetype_lookup_view(entityToArchetypeMap);
				ctx.pMatchesArr = &matchScratch.matchesArr;
				ctx.pMatchesStampByArchetypeId = &matchScratch.matchStamps;
				ctx.matchesVersion = matchScratch.next_match_version();
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
						add_archetype_to_cache(pArchetype, true);
					} else {
						add_archetype_to_seed_cache(pArchetype);
						add_archetype_to_cache(pArchetype, true);
					}
				}

				// Sort entities if necessary
				sort_entities();
				// Sort cache groups if necessary
				sort_cache_groups();
				snapshot_dyn_inputs();
				m_state.clear_dirty();
			}

			//! Tries to match the query against the provided archetype.
			//! This is necessary so we do not iterate all chunks over and over again when running queries.
			//! \param archetype Archtype to match
			//! \param targetEntities Entities related to the matched archetype
			//! \warning Not thread safe. No two threads can call this at the same time.
			void match_one(const Archetype& archetype, EntitySpan targetEntities) {
				auto& w = *world();
				auto& matchScratch = query_match_scratch_acquire(w);
				CleanUpTmpArchetypeMatches autoCleanup(w, true);

				auto& ctxData = m_plan.ctx.data;

				// Recompile if necessary
				if ((ctxData.flags & QueryCtx::QueryFlags::Recompile) != 0)
					recompile();

				// Skip if nothing has been compiled.
				if (!m_plan.vm.is_compiled())
					return;

				const bool hasDynamicTerms = has_dyn_terms();
				if ((hasDynamicTerms && (!can_reuse_dyn_cache() || m_state.needs_refresh() || dyn_inputs_changed())) ||
						m_state.seed_dirty()) {
					// Dynamic queries keep their cached result as long as tracked runtime inputs stay stable.
					// Source-based queries still take the conservative rebuild path until their inputs
					// are tracked with finer-grained version metadata.
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
				ctx.archetypeLookup = {};
				ctx.pMatchesArr = &matchScratch.matchesArr;
				ctx.pMatchesStampByArchetypeId = &matchScratch.matchStamps;
				ctx.matchesVersion = matchScratch.next_match_version();
				ctx.pLastMatchedArchetypeIdx_All = nullptr;
				ctx.pLastMatchedArchetypeIdx_Or = nullptr;
				ctx.pLastMatchedArchetypeIdx_Not = nullptr;
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
						add_archetype_to_cache(pArch, true);
					} else {
						add_archetype_to_seed_cache(pArch);
						add_archetype_to_cache(pArch, true);
					}
				}
				snapshot_dyn_inputs();
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

			bool register_archetype(const Archetype& archetype) {
				auto& ctxData = m_plan.ctx.data;

				// Recompile if necessary.
				if ((ctxData.flags & QueryCtx::QueryFlags::Recompile) != 0)
					recompile();

				if (!can_update_with_new_archetype())
					return false;

				const bool hadMatchBefore = m_state.archetypeSet.contains(&archetype);
				if (can_use_direct_create_archetype_match()) {
					const bool usesIs = direct_create_archetype_match_uses_is();
					for (const auto& term: ctxData.terms_view()) {
						const bool present = usesIs ? vm::detail::match_single_id_on_archetype(*world(), archetype, term.id)
																				: world_component_index_match_count(*world(), archetype, term.id) != 0;
						const bool matched = term.op == QueryOpKind::Not ? !present : present;
						if (!matched)
							return false;
					}
					if (hadMatchBefore)
						return false;

					add_archetype_to_seed_cache(&archetype);
					add_archetype_to_cache(&archetype, true);
					return true;
				}

				SingleArchetypeLookup singleArchetypeLookup;
				auto addLookupUnique = [&](Entity key, uint16_t compIdx, uint16_t matchCount) {
					const auto keyLookup = EntityLookupKey(key);
					const auto itLookup =
							core::find_if(singleArchetypeLookup.begin(), singleArchetypeLookup.end(), [&](const auto& item) {
								return item.matches(keyLookup);
							});
					if (itLookup != singleArchetypeLookup.end()) {
						auto& entry = itLookup->entry;
						entry.matchCount = (uint16_t)(entry.matchCount + matchCount);
						if (compIdx != ComponentIndexBad)
							entry.compIdx = compIdx;
						return;
					}
					singleArchetypeLookup.push_back(
							SingleArchetypeLookupItem{
									keyLookup, ComponentIndexEntry{const_cast<Archetype*>(&archetype), compIdx, matchCount}});
				};
				auto archetypeIds = archetype.ids_view();
				const auto cntIds = (uint32_t)archetypeIds.size();
				GAIA_FOR(cntIds) {
					const auto entity = archetypeIds[i];
					singleArchetypeLookup.push_back(
							SingleArchetypeLookupItem{
									EntityLookupKey(entity), ComponentIndexEntry{const_cast<Archetype*>(&archetype), (uint16_t)i, 1}});

					if (!entity.pair())
						continue;

					// Wildcard pair lookups use the same special records as the world-level
					// entity-to-archetype map, so incremental matching can reuse the normal VM path.
					const auto relKind = entity.entity() ? EntityKind::EK_Uni : EntityKind::EK_Gen;
					const auto rel = Entity((EntityId)entity.id(), 0, false, false, relKind);
					const auto tgt = Entity((EntityId)entity.gen(), 0, false, false, entity.kind());
					addLookupUnique(Pair(All, tgt), ComponentIndexBad, 1);
					addLookupUnique(Pair(rel, All), ComponentIndexBad, 1);
					addLookupUnique(Pair(All, All), ComponentIndexBad, 1);
				}

				auto lastMatchedArchetypeIdx_All = GAIA_MOV(ctxData.lastMatchedArchetypeIdx_All);
				auto lastMatchedArchetypeIdx_Or = GAIA_MOV(ctxData.lastMatchedArchetypeIdx_Or);
				auto lastMatchedArchetypeIdx_Not = GAIA_MOV(ctxData.lastMatchedArchetypeIdx_Not);
				GAIA_ASSERT(ctxData.lastMatchedArchetypeIdx_All.empty());
				GAIA_ASSERT(ctxData.lastMatchedArchetypeIdx_Or.empty());
				GAIA_ASSERT(ctxData.lastMatchedArchetypeIdx_Not.empty());

				const auto* pArchetype = &archetype;
				match(singleArchetypeLookup, std::span((const Archetype**)&pArchetype, 1), archetype.id());
				ctxData.lastMatchedArchetypeIdx_All = GAIA_MOV(lastMatchedArchetypeIdx_All);
				ctxData.lastMatchedArchetypeIdx_Or = GAIA_MOV(lastMatchedArchetypeIdx_Or);
				ctxData.lastMatchedArchetypeIdx_Not = GAIA_MOV(lastMatchedArchetypeIdx_Not);
				const bool matched = !hadMatchBefore && m_state.archetypeSet.contains(&archetype);

				if (!matched)
					return false;

				sort_entities();
				sort_cache_groups();
				return true;
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
				// 	adv_cursor_for_that_chunk();
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
							auto compIdx = world_component_index_comp_idx(*m_plan.ctx.w, *pArchetype, m_plan.ctx.data.sortBy);
							if (compIdx == BadIndex)
								compIdx = pChunk->comp_idx(m_plan.ctx.data.sortBy);
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

				if ((m_plan.ctx.data.flags & QueryCtx::QueryFlags::SortEntities) == 0 && m_state.sortVersion != 0)
					return;
				m_plan.ctx.data.flags &= ~QueryCtx::QueryFlags::SortEntities;

				// First, sort entities in archetypes
				for (const auto* pArchetype: m_state.archetypeCache)
					const_cast<Archetype*>(pArchetype)->sort_entities(m_plan.ctx.data.sortBy, m_plan.ctx.data.sortByFunc);

				// Now that entites are sorted, we can start creating slices
				calculate_sort_data();
				m_state.sortVersion = ::gaia::ecs::world_version(*world());
			}

			void sort_cache_groups() {
				if ((m_plan.ctx.data.flags & QueryCtx::QueryFlags::SortGroups) == 0)
					return;
				m_plan.ctx.data.flags &= ~QueryCtx::QueryFlags::SortGroups;

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
				m_state.selectedGroupDataValid = false;
			}

			ArchetypeCacheData create_cache_data(const Archetype* pArchetype) {
				ArchetypeCacheData cacheData;
				auto queryIds = ctx().data.ids_view();
				const auto cnt = (uint32_t)queryIds.size();
				GAIA_FOR(cnt) {
					const auto idxBeforeRemapping = m_plan.ctx.data._remapping[i];
					const auto queryId = queryIds[idxBeforeRemapping];
					auto compIdx = world_component_index_comp_idx(*world(), *pArchetype, queryId);
					if (compIdx == BadIndex) {
						// Wildcard/semantic terms are not represented by an exact component index entry.
						// Fall back to the archetype-local scan in those cases.
						compIdx = core::get_index_unsafe(pArchetype->ids_view(), queryId);
					}
					GAIA_ASSERT(compIdx != BadIndex);

					cacheData.indices[i] = (uint8_t)compIdx;
				}
				return cacheData;
			}

			void add_archetype_to_cache_no_grouping(const Archetype* pArchetype, bool trackMembershipChange) {
				GAIA_PROF_SCOPE(queryinfo::add_cache_ng);

				if (m_state.archetypeSet.contains(pArchetype))
					return;

				m_state.archetypeSet.emplace(pArchetype);
				m_state.archetypeCache.push_back(pArchetype);
				m_state.archetypeCacheData.push_back(create_cache_data(pArchetype));
				if (trackMembershipChange)
					mark_result_cache_membership_changed();
			}

			void add_archetype_to_seed_cache(const Archetype* pArchetype) {
				if (m_state.seedArchetypeSet.contains(pArchetype))
					return;

				m_state.seedArchetypeSet.emplace(pArchetype);
				m_state.seedArchetypeCache.push_back(pArchetype);
				m_state.seedArchetypeCacheData.push_back(create_cache_data(pArchetype));
			}

			void add_archetype_to_cache_w_grouping(const Archetype* pArchetype, bool trackMembershipChange) {
				GAIA_PROF_SCOPE(queryinfo::add_cache_wg);

				if (m_state.archetypeSet.contains(pArchetype))
					return;

				m_state.selectedGroupDataValid = false;

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
				if (trackMembershipChange)
					mark_result_cache_membership_changed();
			}

			void add_archetype_to_cache(const Archetype* pArchetype, bool trackMembershipChange) {
				if (m_plan.ctx.data.sortByFunc != nullptr)
					m_plan.ctx.data.flags |= QueryCtx::QueryFlags::SortEntities;

				if (m_plan.ctx.data.groupBy != EntityBad)
					add_archetype_to_cache_w_grouping(pArchetype, trackMembershipChange);
				else
					add_archetype_to_cache_no_grouping(pArchetype, trackMembershipChange);
			}

			//! Returns cached group bounds for the currently selected group filter.
			//! The cached range is invalidated whenever group layout changes or the selected group id changes.
			GAIA_NODISCARD const GroupData* selected_group_data() const {
				if (m_plan.ctx.data.groupBy == EntityBad || m_plan.ctx.data.groupIdSet == 0)
					return nullptr;

				if (!m_state.selectedGroupDataValid || m_state.selectedGroupData.groupId != m_plan.ctx.data.groupIdSet) {
					const auto cnt = m_state.archetypeGroupData.size();
					GAIA_FOR(cnt) {
						if (m_state.archetypeGroupData[i].groupId != m_plan.ctx.data.groupIdSet)
							continue;

						m_state.selectedGroupData = m_state.archetypeGroupData[i];
						m_state.selectedGroupDataValid = true;
						return &m_state.selectedGroupData;
					}

					m_state.selectedGroupData = {};
					m_state.selectedGroupDataValid = false;
					return nullptr;
				}

				return &m_state.selectedGroupData;
			}

			GAIA_NODISCARD bool has_same_result_membership_as_seed_cache() const {
				if (m_state.archetypeSet.size() != m_state.seedArchetypeSet.size())
					return false;

				for (const auto* pArchetype: m_state.seedArchetypeCache) {
					if (!m_state.archetypeSet.contains(pArchetype))
						return false;
				}

				return true;
			}

			void sync_result_cache_from_seed_cache() {
				const bool membershipChanged = !has_same_result_membership_as_seed_cache();
				clear_result_cache();
				const auto cnt = (uint32_t)m_state.seedArchetypeCache.size();
				GAIA_FOR(cnt) {
					add_archetype_to_cache(m_state.seedArchetypeCache[i], false);
				}
				if (membershipChanged)
					mark_result_cache_membership_changed();
			}

			bool del_archetype_from_cache(const Archetype* pArchetype) {
				const auto it = m_state.archetypeSet.find(pArchetype);
				if (it == m_state.archetypeSet.end())
					return false;

				m_state.archetypeSet.erase(it);

				const auto archetypeIdx = core::get_index(m_state.archetypeCache, pArchetype);
				GAIA_ASSERT(archetypeIdx != BadIndex);
				if (archetypeIdx == BadIndex)
					return true;

				if (m_plan.ctx.data.sortByFunc != nullptr)
					m_plan.ctx.data.flags |= QueryCtx::QueryFlags::SortEntities;

				core::swap_erase(m_state.archetypeCache, archetypeIdx);
				core::swap_erase(m_state.archetypeCacheData, archetypeIdx);

				// Update the group data if possible
				if (m_plan.ctx.data.groupBy != EntityBad) {
					m_state.selectedGroupDataValid = false;

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

				mark_result_cache_membership_changed();
				return true;
			}

			bool del_archetype_from_seed_cache(const Archetype* pArchetype) {
				const auto it = m_state.seedArchetypeSet.find(pArchetype);
				if (it == m_state.seedArchetypeSet.end())
					return false;

				m_state.seedArchetypeSet.erase(it);

				const auto archetypeIdx = core::get_index(m_state.seedArchetypeCache, pArchetype);
				GAIA_ASSERT(archetypeIdx != BadIndex);
				if (archetypeIdx == BadIndex)
					return true;

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
				const auto& ctxData = m_plan.ctx.data;
				return ctxData.changedCnt > 0;
			}

			//! Returns true when direct non-fragmenting terms must be rechecked per entity.
			GAIA_NODISCARD bool has_entity_filter_terms() const {
				const auto& ctxData = m_plan.ctx.data;
				return ctxData.deps.has(QueryCtx::DependencyHasAdjunctTerms);
			}

			//! Returns true when prefab-tagged entities should participate in query results.
			GAIA_NODISCARD bool matches_prefab_entities() const {
				const auto& ctxData = m_plan.ctx.data;
				return (ctxData.flags & QueryCtx::QueryFlags::MatchPrefab) != 0 ||
							 (ctxData.flags & QueryCtx::QueryFlags::HasPrefabTerms) != 0;
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
