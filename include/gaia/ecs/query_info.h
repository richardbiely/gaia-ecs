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
		Entity world_query_first_inherited_owner(const World& world, const Archetype& archetype, Entity term);

		using EntityToArchetypeMap = cnt::map<EntityLookupKey, ComponentIndexEntryArray>;
		struct ArchetypeCompIndices {
			uint8_t indices[ChunkHeader::MAX_COMPONENTS];
		};
		struct ArchetypeInheritedOwners {
			Entity owners[ChunkHeader::MAX_COMPONENTS];
		};

		struct QueryMatchScratch {
			//! Ordered list of matched archetypes emitted by the VM for the current run.
			cnt::darr<const Archetype*> matchesArr;
			//! Paged O(1) dedup table keyed by world-local archetype ids.
			//! Pages stay allocated on the scratch frame so repeated matches do not keep
			//! reallocating heap memory when archetype ids revisit the same ranges.
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

		class QueryInfo {
		public:
			//! Allocated items: index in the query slot list.
			//! Deleted items: index of the next deleted item in the slot list.
			uint32_t idx = 0;
			//! Generation ID of the query slot.
			uint32_t gen = 0;

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

				//! Used to make sure only unique archetypes are inserted into the cache.
				//! TODO: Get rid of the set by changing the way the caching works.
				cnt::set<const Archetype*> archetypeSet;
				//! Cached array of archetypes matching the query
				CArchetypeDArray archetypeCache;

				struct GroupedPayload {
					//! Group ids for grouped queries, aligned with archetypeCache.
					cnt::darray<GroupId> archetypeGroupIds;
					//! Group data used by cache.
					cnt::darray<GroupData> archetypeGroupData;
					//! Cached range for the currently selected group id.
					mutable GroupData selectedGroupData{};
					//! True when selectedGroupData matches the active group filter.
					mutable bool selectedGroupDataValid = false;
					//! True when grouped archetype order/ranges need to be rebuilt.
					bool dataPending = false;

					void clear() {
						archetypeGroupIds = {};
						archetypeGroupData = {};
						selectedGroupData = {};
						selectedGroupDataValid = false;
						dataPending = false;
					}

					void clear_transient() {
						archetypeGroupIds.clear();
						archetypeGroupData.clear();
						selectedGroupData = {};
						selectedGroupDataValid = false;
						dataPending = false;
					}
				};

				struct ExecPayload {
					//! Cached component-index mapping for each matched archetype.
					cnt::darray<ArchetypeCompIndices> archetypeCompIndices;
					//! Cached inherited owner entity per query field for exact self-source semantic terms.
					cnt::darray<ArchetypeInheritedOwners> archetypeInheritedOwners;
					//! True when archetype membership is populated but component-index metadata
					//! still needs to be built on demand.
					bool compIndicesPending = false;
					//! True when archetype membership is populated but inherited-owner metadata
					//! still needs to be built on demand.
					bool inheritedOwnersPending = false;

					void clear() {
						archetypeCompIndices = {};
						archetypeInheritedOwners = {};
						compIndicesPending = false;
						inheritedOwnersPending = false;
					}

					void clear_transient() {
						archetypeCompIndices.clear();
						archetypeInheritedOwners.clear();
						compIndicesPending = false;
						inheritedOwnersPending = false;
					}
				};

				struct NonTrivialPayload {
					//! Cached depth-order hierarchy barrier result for each archetype.
					cnt::darray<uint8_t> archetypeBarrierPasses;
					//! Sort data used by cache.
					cnt::darray<SortData> archetypeSortData;
					//! World version at which the sorted cache slices were last rebuilt.
					//! Unlike worldVersion, this is only updated after a real sort refresh.
					uint32_t sortVersion{};
					//! Relation topology version at which the cached depth-order hierarchy barrier state was last rebuilt.
					uint32_t barrierRelVersion = UINT32_MAX;
					//! Entity enable-state version at which the cached depth-order hierarchy barrier state was last rebuilt.
					uint32_t barrierEnabledVersion = UINT32_MAX;

					void clear() {
						archetypeSortData = {};
						archetypeBarrierPasses = {};
						sortVersion = 0;
						barrierRelVersion = UINT32_MAX;
						barrierEnabledVersion = UINT32_MAX;
					}

					void clear_transient() {
						archetypeSortData.clear();
						archetypeBarrierPasses.clear();
						sortVersion = 0;
						barrierRelVersion = UINT32_MAX;
						barrierEnabledVersion = UINT32_MAX;
					}
				};

				//! Execution metadata shared by plain and grouped iteration paths on demand.
				ExecPayload exec;
				//! Grouped-query-only payload.
				GroupedPayload grouped;
				//! Sort/remap/barrier payload used by nontrivial execution paths on demand.
				NonTrivialPayload nonTrivial;

				//! Id of the last archetype in the world we checked
				ArchetypeId lastArchetypeId{};
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
				//! Dirty flags
				uint8_t dirtyFlags = DirtyFlags::All;

				void clear_seed_cache() {
					seedArchetypeSet = {};
					seedArchetypeCache = {};
				}

				void clear_result_cache() {
					archetypeSet = {};
					archetypeCache = {};
					exec.clear();
					grouped.clear();
					nonTrivial.clear();
				}

				void clear_transient_result_cache() {
					archetypeCache.clear();
					exec.clear_transient();
					grouped.clear_transient();
					nonTrivial.clear_transient();
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
				m_state.nonTrivial.barrierRelVersion = UINT32_MAX;
				m_state.nonTrivial.barrierEnabledVersion = UINT32_MAX;
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
				if (!deps.has_dep_flag(QueryCtx::DependencyHasSourceTerms))
					return true;

				if (!deps.can_reuse_src_cache())
					return false;

				// Direct concrete-source reuse is automatic. Traversed source reuse still needs explicit snapshots.
				if (!deps.has_dep_flag(QueryCtx::DependencyHasTraversalTerms))
					return true;

				return m_plan.ctx.data.cacheSrcTrav != 0;
			}

			//! Direct concrete-source queries reuse per-source archetype versions without rebuilding a traversal closure.
			GAIA_NODISCARD bool uses_direct_src_version_tracking() const {
				const auto& deps = m_plan.ctx.data.deps;
				return !deps.has_dep_flag(QueryCtx::DependencyHasTraversalTerms) && //
							 can_reuse_dyn_cache();
			}

			//! Traversed-source queries reuse an explicit source-closure snapshot when opted in.
			GAIA_NODISCARD bool uses_src_trav_snapshot() const {
				const auto& deps = m_plan.ctx.data.deps;
				return deps.has_dep_flag(QueryCtx::DependencyHasTraversalTerms) && //
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
			GAIA_NODISCARD bool dyn_var_bindings_changed(
					const cnt::sarray<Entity, MaxVarCnt>& runtimeVarBindings, uint8_t runtimeVarBindingMask) const {
				if (m_state.varBindingMask != runtimeVarBindingMask)
					return true;

				GAIA_FOR(MaxVarCnt) {
					const auto bit = (uint8_t(1) << i);
					if ((runtimeVarBindingMask & bit) == 0)
						continue;

					if (m_state.varBindings[i] != runtimeVarBindings[i])
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
				if (!deps.has_dep_flag(QueryCtx::DependencyHasSourceTerms))
					return false;

				if (!deps.has_dep_flag(QueryCtx::DependencyHasTraversalTerms))
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
			GAIA_NODISCARD bool
			dyn_inputs_changed(const cnt::sarray<Entity, MaxVarCnt>& runtimeVarBindings, uint8_t runtimeVarBindingMask) {
				if (!can_reuse_dyn_cache())
					return false;

				if (dyn_var_bindings_changed(runtimeVarBindings, runtimeVarBindingMask))
					return true;

				const bool relationVersionsChanged = dyn_rel_versions_changed();
				if (relationVersionsChanged && !uses_src_trav_snapshot())
					return true;

				const auto& deps = m_plan.ctx.data.deps;
				if (!deps.has_dep_flag(QueryCtx::DependencyHasSourceTerms))
					return relationVersionsChanged;

				if (m_state.srcTravSnapshotOverflowed)
					return true;

				return dyn_src_inputs_changed(relationVersionsChanged);
			}

			//! Captures the tracked runtime inputs used by the reusable dynamic cache.
			void
			snapshot_dyn_inputs(const cnt::sarray<Entity, MaxVarCnt>& runtimeVarBindings, uint8_t runtimeVarBindingMask) {
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

				m_state.varBindings = runtimeVarBindings;
				m_state.varBindingMask = runtimeVarBindingMask;
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
					const auto compIdx = comp_idx<MAX_ITEMS_IN_QUERY>(ctxData.terms.data(), id, EntityBad);

					if (op != ctxData.terms[compIdx].op)
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
				info.gen = 0;

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
				info.gen = gen;

				info.m_plan.ctx = GAIA_MOV(queryCtx);
				info.m_plan.ctx.q.handle = {idx, gen};

				// Compile the query
				info.compile(entityToArchetypeMap, pCreationCtx->allArchetypes);

				return info;
			}

			GAIA_NODISCARD static QueryHandle handle(const QueryInfo& info) {
				return QueryHandle(info.idx, info.gen);
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

			GAIA_NODISCARD QueryCtx::CachePolicy cache_policy() const {
				return m_plan.ctx.data.cachePolicy;
			}

			GAIA_NODISCARD bool has_grouped_payload() const {
				return m_plan.ctx.data.groupBy != EntityBad;
			}

			GAIA_NODISCARD bool has_sorted_payload() const {
				return m_plan.ctx.data.sortByFunc != nullptr;
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
				return m_plan.ctx.data.createArchetypeMatchKind == QueryCtx::CreateArchetypeMatchKind::DirectStructuralTerms;
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
					ArchetypeId archetypeLastId, const cnt::sarray<Entity, MaxVarCnt>& runtimeVarBindings,
					uint8_t runtimeVarBindingMask) {
				auto& ctxData = m_plan.ctx.data;

				// Recompile if necessary
				if ((ctxData.flags & QueryCtx::QueryFlags::Recompile) != 0)
					recompile();

				// Skip if nothing has been compiled.
				if (!m_plan.vm.is_compiled())
					return;

				const bool hasDynamicTerms = has_dyn_terms();
				if (hasDynamicTerms && (!can_reuse_dyn_cache() || m_state.needs_refresh() ||
																dyn_inputs_changed(runtimeVarBindings, runtimeVarBindingMask))) {
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

				auto& w = *world();
				auto& matchScratch = query_match_scratch_acquire(w);
				CleanUpTmpArchetypeMatches autoCleanup(w, true);

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
				ctx.varBindings = runtimeVarBindings;
				ctx.varBindingMask = runtimeVarBindingMask;

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
				snapshot_dyn_inputs(runtimeVarBindings, runtimeVarBindingMask);
				m_state.clear_dirty();
			}

			//! Tries to match the query against the provided archetype.
			//! This is necessary so we do not iterate all chunks over and over again when running queries.
			//! \param archetype Archtype to match
			//! \param targetEntities Entities related to the matched archetype
			//! \warning Not thread safe. No two threads can call this at the same time.
			bool match_one(
					const Archetype& archetype, EntitySpan targetEntities,
					const cnt::sarray<Entity, MaxVarCnt>& runtimeVarBindings, uint8_t runtimeVarBindingMask) {
				auto& ctxData = m_plan.ctx.data;

				// Recompile if necessary
				if ((ctxData.flags & QueryCtx::QueryFlags::Recompile) != 0)
					recompile();

				// Skip if nothing has been compiled.
				if (!m_plan.vm.is_compiled())
					return false;

				const bool hasDynamicTerms = has_dyn_terms();
				if ((hasDynamicTerms && (!can_reuse_dyn_cache() || m_state.needs_refresh() ||
																 dyn_inputs_changed(runtimeVarBindings, runtimeVarBindingMask))) ||
						m_state.seed_dirty()) {
					// Dynamic queries keep their cached result as long as tracked runtime inputs stay stable.
					// Source-based queries still take the conservative rebuild path until their inputs
					// are tracked with finer-grained version metadata.
					reset_matching_cache();
				} else if (m_state.result_dirty()) {
					sync_result_cache_from_seed_cache();
				}

				GAIA_PROF_SCOPE(queryinfo::match1);

				auto& w = *world();
				auto& matchScratch = query_match_scratch_acquire(w);
				CleanUpTmpArchetypeMatches autoCleanup(w, true);

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
				ctx.varBindings = runtimeVarBindings;
				ctx.varBindingMask = runtimeVarBindingMask;

				// Run the virtual machine
				m_plan.vm.exec(ctx);
				const bool matched = !ctx.pMatchesArr->empty();

				// Write found matches to cache
				for (const auto* pArch: *ctx.pMatchesArr) {
					if (hasDynamicTerms) {
						add_archetype_to_cache(pArch, true);
					} else {
						add_archetype_to_seed_cache(pArch);
						add_archetype_to_cache(pArch, true);
					}
				}
				snapshot_dyn_inputs(runtimeVarBindings, runtimeVarBindingMask);
				m_state.clear_dirty();
				return matched;
			}

			void ensure_matches(
					const EntityToArchetypeMap& entityToArchetypeMap, std::span<const Archetype*> allArchetypes,
					ArchetypeId archetypeLastId, const cnt::sarray<Entity, MaxVarCnt>& runtimeVarBindings,
					uint8_t runtimeVarBindingMask) {
				match(entityToArchetypeMap, allArchetypes, archetypeLastId, runtimeVarBindings, runtimeVarBindingMask);
			}

			void ensure_matches_transient(
					const EntityToArchetypeMap& entityToArchetypeMap, std::span<const Archetype*> allArchetypes,
					const cnt::sarray<Entity, MaxVarCnt>& runtimeVarBindings, uint8_t runtimeVarBindingMask) {
				auto& ctxData = m_plan.ctx.data;

				if ((ctxData.flags & QueryCtx::QueryFlags::Recompile) != 0)
					recompile();

				if (!m_plan.vm.is_compiled())
					return;

				m_state.clear_transient_result_cache();

				auto& w = *world();
				auto& matchScratch = query_match_scratch_acquire(w);
				CleanUpTmpArchetypeMatches autoCleanup(w, true);

				vm::MatchingCtx ctx{};
				ctx.pWorld = world();
				ctx.allArchetypes = allArchetypes;
				ctx.archetypeLookup = vm::make_archetype_lookup_view(entityToArchetypeMap);
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
				ctx.varBindings = runtimeVarBindings;
				ctx.varBindingMask = runtimeVarBindingMask;

				m_plan.vm.exec(ctx);

				m_state.archetypeCache.reserve(ctx.pMatchesArr->size());
				if (ctxData.groupBy != EntityBad)
					m_state.grouped.archetypeGroupIds.reserve(ctx.pMatchesArr->size());
				for (const auto* pArchetype: *ctx.pMatchesArr)
					add_archetype_to_transient_cache(pArchetype);

				sort_entities();
				ensure_group_data();
			}

			bool ensure_matches_one(
					const Archetype& archetype, EntitySpan targetEntities,
					const cnt::sarray<Entity, MaxVarCnt>& runtimeVarBindings, uint8_t runtimeVarBindingMask) {
				return match_one(archetype, targetEntities, runtimeVarBindings, runtimeVarBindingMask);
			}

			bool ensure_matches_one_transient(
					const Archetype& archetype, EntitySpan targetEntities,
					const cnt::sarray<Entity, MaxVarCnt>& runtimeVarBindings, uint8_t runtimeVarBindingMask) {
				auto& ctxData = m_plan.ctx.data;

				if ((ctxData.flags & QueryCtx::QueryFlags::Recompile) != 0)
					recompile();

				if (!m_plan.vm.is_compiled())
					return false;

				m_state.clear_transient_result_cache();

				auto& w = *world();
				auto& matchScratch = query_match_scratch_acquire(w);
				CleanUpTmpArchetypeMatches autoCleanup(w, true);

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
				ctx.varBindings = runtimeVarBindings;
				ctx.varBindingMask = runtimeVarBindingMask;

				m_plan.vm.exec(ctx);
				const bool matched = !ctx.pMatchesArr->empty();

				m_state.archetypeCache.reserve(ctx.pMatchesArr->size());
				if (ctxData.groupBy != EntityBad)
					m_state.grouped.archetypeGroupIds.reserve(ctx.pMatchesArr->size());
				for (const auto* pArch: *ctx.pMatchesArr)
					add_archetype_to_transient_cache(pArch);

				ensure_group_data();
				return matched;
			}

			bool register_archetype(const Archetype& archetype, Entity matchedSelector = EntityBad, bool assumeNew = false) {
				auto& ctxData = m_plan.ctx.data;

				// Recompile if necessary.
				if ((ctxData.flags & QueryCtx::QueryFlags::Recompile) != 0)
					recompile();

				if (!can_update_with_new_archetype())
					return false;

				const bool hadMatchBefore = !assumeNew && m_state.archetypeSet.contains(&archetype);
				if (can_use_direct_create_archetype_match()) {
					const bool usesIs = direct_create_archetype_match_uses_is();
					bool hasOrTerms = false;
					bool matchedOrTerm = false;
					for (const auto& term: ctxData.terms_view()) {
						if (term.id == matchedSelector) {
							if (term.op == QueryOpKind::Or)
								matchedOrTerm = true;
							continue;
						}

						const bool present = usesIs ? vm::detail::match_single_id_on_archetype(*world(), archetype, term.id)
																				: world_component_index_match_count(*world(), archetype, term.id) != 0;
						if (term.op == QueryOpKind::Or) {
							hasOrTerms = true;
							matchedOrTerm |= present;
							continue;
						}
						if (term.op == QueryOpKind::Any)
							continue;

						const bool matched = term.op == QueryOpKind::Not ? !present : present;
						if (!matched)
							return false;
					}
					if (hasOrTerms && !matchedOrTerm)
						return false;
					if (hadMatchBefore)
						return false;

					if (assumeNew)
						add_new_archetype_to_immediate_caches(&archetype, true);
					else {
						add_archetype_to_seed_cache(&archetype, false);
						add_archetype_to_cache(&archetype, true, false);
					}
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
				const cnt::sarray<Entity, MaxVarCnt> noRuntimeVarBindings{};
				match(
						singleArchetypeLookup, std::span((const Archetype**)&pArchetype, 1), archetype.id(), noRuntimeVarBindings,
						0);
				ctxData.lastMatchedArchetypeIdx_All = GAIA_MOV(lastMatchedArchetypeIdx_All);
				ctxData.lastMatchedArchetypeIdx_Or = GAIA_MOV(lastMatchedArchetypeIdx_Or);
				ctxData.lastMatchedArchetypeIdx_Not = GAIA_MOV(lastMatchedArchetypeIdx_Not);
				const bool matched = assumeNew ? m_state.archetypeSet.contains(&archetype)
																			 : !hadMatchBefore && m_state.archetypeSet.contains(&archetype);

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

				m_state.nonTrivial.archetypeSortData.clear();

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
							m_state.nonTrivial.archetypeSortData.push_back(
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
					m_state.nonTrivial.archetypeSortData.push_back(
							{pCurrentChunk, currArchetypeIdx, currentStartRow, (uint16_t)(currentRow - currentStartRow)});
				}
			}

			void sort_entities() {
				if (m_plan.ctx.data.sortByFunc == nullptr)
					return;

				if ((m_plan.ctx.data.flags & QueryCtx::QueryFlags::SortEntities) == 0 && m_state.nonTrivial.sortVersion != 0)
					return;
				m_plan.ctx.data.flags &= ~QueryCtx::QueryFlags::SortEntities;

				// First, sort entities in archetypes
				for (const auto* pArchetype: m_state.archetypeCache)
					const_cast<Archetype*>(pArchetype)->sort_entities(m_plan.ctx.data.sortBy, m_plan.ctx.data.sortByFunc);

				// Now that entites are sorted, we can start creating slices
				calculate_sort_data();
				m_state.nonTrivial.sortVersion = ::gaia::ecs::world_version(*world());
			}

			void sort_cache_groups() {
				if ((m_plan.ctx.data.flags & QueryCtx::QueryFlags::SortGroups) == 0)
					return;
				m_plan.ctx.data.flags &= ~QueryCtx::QueryFlags::SortGroups;

				ensure_group_data();
			}

			void swap_archetype_cache_entry(uint32_t left, uint32_t right) {
				auto* pTmpArchetype = m_state.archetypeCache[left];
				m_state.archetypeCache[left] = m_state.archetypeCache[right];
				m_state.archetypeCache[right] = pTmpArchetype;

				if (left < m_state.grouped.archetypeGroupIds.size() && right < m_state.grouped.archetypeGroupIds.size()) {
					const auto tmp = m_state.grouped.archetypeGroupIds[left];
					m_state.grouped.archetypeGroupIds[left] = m_state.grouped.archetypeGroupIds[right];
					m_state.grouped.archetypeGroupIds[right] = tmp;
				}

				if (left < m_state.exec.archetypeCompIndices.size() && right < m_state.exec.archetypeCompIndices.size()) {
					auto tmp = m_state.exec.archetypeCompIndices[left];
					m_state.exec.archetypeCompIndices[left] = m_state.exec.archetypeCompIndices[right];
					m_state.exec.archetypeCompIndices[right] = tmp;
				}

				if (left < m_state.exec.archetypeInheritedOwners.size() &&
						right < m_state.exec.archetypeInheritedOwners.size()) {
					auto tmp = m_state.exec.archetypeInheritedOwners[left];
					m_state.exec.archetypeInheritedOwners[left] = m_state.exec.archetypeInheritedOwners[right];
					m_state.exec.archetypeInheritedOwners[right] = tmp;
				}

				if (left < m_state.nonTrivial.archetypeBarrierPasses.size() &&
						right < m_state.nonTrivial.archetypeBarrierPasses.size()) {
					const auto tmp = m_state.nonTrivial.archetypeBarrierPasses[left];
					m_state.nonTrivial.archetypeBarrierPasses[left] = m_state.nonTrivial.archetypeBarrierPasses[right];
					m_state.nonTrivial.archetypeBarrierPasses[right] = tmp;
				}
			}

			void ensure_comp_indices() {
				if (!m_state.exec.compIndicesPending)
					return;

				m_state.exec.archetypeCompIndices.clear();
				m_state.exec.archetypeCompIndices.reserve(m_state.archetypeCache.size());
				for (const auto* pArchetype: m_state.archetypeCache)
					m_state.exec.archetypeCompIndices.push_back(create_comp_indices(pArchetype));

				m_state.exec.compIndicesPending = false;
			}

			GAIA_NODISCARD bool has_inherited_owner_payload() const {
				return ctx().data.deps.has_dep_flag(QueryCtx::DependencyHasInheritedTerms);
			}

			void ensure_inherited_owners() {
				if (!m_state.exec.inheritedOwnersPending)
					return;

				if (!has_inherited_owner_payload()) {
					m_state.exec.archetypeInheritedOwners.clear();
					m_state.exec.inheritedOwnersPending = false;
					return;
				}

				m_state.exec.archetypeInheritedOwners.clear();
				m_state.exec.archetypeInheritedOwners.reserve(m_state.archetypeCache.size());
				for (const auto* pArchetype: m_state.archetypeCache)
					m_state.exec.archetypeInheritedOwners.push_back(create_inherited_owners(pArchetype));

				m_state.exec.inheritedOwnersPending = false;
			}

			void ensure_group_data() {
				if (m_plan.ctx.data.groupBy == EntityBad || !m_state.grouped.dataPending)
					return;

				struct sort_cond {
					bool operator()(GroupId a, GroupId b) const {
						return a <= b;
					}
				};

				core::sort(m_state.grouped.archetypeGroupIds, sort_cond{}, [&](uint32_t left, uint32_t right) {
					swap_archetype_cache_entry(left, right);
				});

				m_state.grouped.archetypeGroupData.clear();
				m_state.grouped.selectedGroupDataValid = false;

				if (m_state.grouped.archetypeGroupIds.empty()) {
					m_state.grouped.dataPending = false;
					return;
				}

				GroupId groupId = m_state.grouped.archetypeGroupIds[0];
				uint32_t idxFirst = 0;
				const auto cnt = (uint32_t)m_state.grouped.archetypeGroupIds.size();
				for (uint32_t i = 1; i < cnt; ++i) {
					if (m_state.grouped.archetypeGroupIds[i] == groupId)
						continue;

					m_state.grouped.archetypeGroupData.push_back({groupId, idxFirst, i - 1, false});
					groupId = m_state.grouped.archetypeGroupIds[i];
					idxFirst = i;
				}

				m_state.grouped.archetypeGroupData.push_back({groupId, idxFirst, cnt - 1, false});
				m_state.grouped.dataPending = false;
			}

			void ensure_depth_order_hierarchy_barrier_cache_inter() {
				if (!world_depth_order_prunes_disabled_subtrees(*world(), m_plan.ctx.data.groupBy))
					return;

				ensure_group_data();

				const auto currRelationVersion = world_rel_version(*world(), m_plan.ctx.data.groupBy);
				const auto currEnabledVersion = world_enabled_hierarchy_version(*world());
				if (m_state.nonTrivial.barrierRelVersion == currRelationVersion &&
						m_state.nonTrivial.barrierEnabledVersion == currEnabledVersion)
					return;

				m_state.nonTrivial.archetypeBarrierPasses.resize(m_state.archetypeCache.size(), 1);

				const auto relation = m_plan.ctx.data.groupBy;
				for (uint32_t i = 0; i < m_state.archetypeCache.size(); ++i) {
					const auto* pArchetype = m_state.archetypeCache[i];
					auto& barrierPasses = m_state.nonTrivial.archetypeBarrierPasses[i];
					barrierPasses = 1;

					auto ids = pArchetype->ids_view();
					for (auto idsIdx: pArchetype->pair_rel_indices(relation)) {
						const auto pair = ids[idsIdx];
						const auto parent = world_pair_target_if_alive(*world(), pair);
						if (parent == EntityBad || !world_entity_enabled_hierarchy(*world(), parent, relation)) {
							barrierPasses = 0;
							break;
						}
					}
				}

				m_state.nonTrivial.barrierRelVersion = currRelationVersion;
				m_state.nonTrivial.barrierEnabledVersion = currEnabledVersion;
			}

			ArchetypeCompIndices create_comp_indices(const Archetype* pArchetype) {
				ArchetypeCompIndices cacheData{};
				core::fill(cacheData.indices, cacheData.indices + ChunkHeader::MAX_COMPONENTS, (uint8_t)0xFF);
				const auto terms = ctx().data.terms_view();
				const auto cnt = (uint32_t)terms.size();
				GAIA_FOR(cnt) {
					const auto& term = terms[i];
					const auto fieldIdx = term.fieldIndex;
					const auto queryId = term.id;
					if (!queryId.pair() && world_is_out_of_line_component(*world(), queryId)) {
						const auto compIdx = core::get_index_unsafe(pArchetype->ids_view(), queryId);
						GAIA_ASSERT(compIdx != BadIndex);
						cacheData.indices[fieldIdx] = 0xFF;
						continue;
					}

					auto compIdx = world_component_index_comp_idx(*world(), *pArchetype, queryId);
					if (compIdx == BadIndex) {
						// Wildcard/semantic terms are not represented by an exact component index entry.
						// Fall back to the archetype-local scan in those cases.
						compIdx = core::get_index_unsafe(pArchetype->ids_view(), queryId);
					}
					GAIA_ASSERT(compIdx != BadIndex);

					cacheData.indices[fieldIdx] = (uint8_t)compIdx;
				}
				return cacheData;
			}

			ArchetypeInheritedOwners create_inherited_owners(const Archetype* pArchetype) {
				ArchetypeInheritedOwners cacheData{};
				core::fill(cacheData.owners, cacheData.owners + ChunkHeader::MAX_COMPONENTS, EntityBad);

				const auto terms = ctx().data.terms_view();
				const auto cnt = (uint32_t)terms.size();
				GAIA_FOR(cnt) {
					const auto& term = terms[i];
					if (term.src != EntityBad || term.entTrav != EntityBad || term_has_variables(term))
						continue;
					if (term.matchKind != QueryMatchKind::Semantic)
						continue;
					const auto queryId = term.id;
					if (queryId == EntityBad || is_wildcard(queryId) || is_variable((EntityId)queryId.id()))
						continue;
					if (world_is_out_of_line_component(*world(), queryId))
						continue;
					if (!world_term_uses_inherit_policy(*world(), queryId))
						continue;
					if (pArchetype->has(queryId))
						continue;

					const auto owner = world_query_first_inherited_owner(*world(), *pArchetype, queryId);
					GAIA_ASSERT(owner != EntityBad);
					cacheData.owners[term.fieldIndex] = owner;
				}

				return cacheData;
			}

			void add_archetype_to_cache_no_grouping(
					const Archetype* pArchetype, bool trackMembershipChange, bool assumeAbsent = false) {
				GAIA_PROF_SCOPE(queryinfo::add_cache_ng);

				if (!assumeAbsent && m_state.archetypeSet.contains(pArchetype))
					return;
				GAIA_ASSERT(assumeAbsent || !m_state.archetypeSet.contains(pArchetype));

				m_state.archetypeSet.emplace(pArchetype);
				m_state.archetypeCache.push_back(pArchetype);
				m_state.exec.compIndicesPending = true;
				m_state.exec.inheritedOwnersPending = true;
				m_state.nonTrivial.barrierRelVersion = UINT32_MAX;
				m_state.nonTrivial.barrierEnabledVersion = UINT32_MAX;
				if (trackMembershipChange)
					mark_result_cache_membership_changed();
			}

			void add_archetype_to_seed_cache(const Archetype* pArchetype, bool assumeAbsent = false) {
				if (!assumeAbsent && m_state.seedArchetypeSet.contains(pArchetype))
					return;
				GAIA_ASSERT(assumeAbsent || !m_state.seedArchetypeSet.contains(pArchetype));

				m_state.seedArchetypeSet.emplace(pArchetype);
				m_state.seedArchetypeCache.push_back(pArchetype);
			}

			//! Adds a newly matched archetype to both immediate caches while reusing one computed index mapping.
			void add_new_archetype_to_immediate_caches(const Archetype* pArchetype, bool trackMembershipChange) {
				GAIA_ASSERT(m_plan.ctx.data.groupBy == EntityBad);
				GAIA_ASSERT(m_plan.ctx.data.sortByFunc == nullptr);
				GAIA_ASSERT(!m_state.seedArchetypeSet.contains(pArchetype));
				GAIA_ASSERT(!m_state.archetypeSet.contains(pArchetype));

				m_state.seedArchetypeSet.emplace(pArchetype);
				m_state.seedArchetypeCache.push_back(pArchetype);

				m_state.archetypeSet.emplace(pArchetype);
				m_state.archetypeCache.push_back(pArchetype);
				m_state.exec.compIndicesPending = true;
				m_state.exec.inheritedOwnersPending = true;
				if (trackMembershipChange)
					mark_result_cache_membership_changed();
			}

			void add_archetype_to_cache_w_grouping(
					const Archetype* pArchetype, bool trackMembershipChange, bool assumeAbsent = false) {
				GAIA_PROF_SCOPE(queryinfo::add_cache_wg);

				if (!assumeAbsent && m_state.archetypeSet.contains(pArchetype))
					return;
				GAIA_ASSERT(assumeAbsent || !m_state.archetypeSet.contains(pArchetype));

				m_state.grouped.selectedGroupDataValid = false;

				const GroupId groupId = m_plan.ctx.data.groupByFunc(*m_plan.ctx.w, *pArchetype, m_plan.ctx.data.groupBy);

				m_state.archetypeSet.emplace(pArchetype);
				m_state.archetypeCache.push_back(pArchetype);
				m_state.grouped.archetypeGroupIds.push_back(groupId);
				m_state.grouped.dataPending = true;
				m_state.exec.compIndicesPending = true;
				m_state.exec.inheritedOwnersPending = true;
				m_state.nonTrivial.barrierRelVersion = UINT32_MAX;
				m_state.nonTrivial.barrierEnabledVersion = UINT32_MAX;
				m_plan.ctx.data.flags |= QueryCtx::QueryFlags::SortGroups;
				if (trackMembershipChange)
					mark_result_cache_membership_changed();
			}

			void add_archetype_to_cache(const Archetype* pArchetype, bool trackMembershipChange, bool assumeAbsent = false) {
				if (m_plan.ctx.data.sortByFunc != nullptr)
					m_plan.ctx.data.flags |= QueryCtx::QueryFlags::SortEntities;

				if (m_plan.ctx.data.groupBy != EntityBad)
					add_archetype_to_cache_w_grouping(pArchetype, trackMembershipChange, assumeAbsent);
				else
					add_archetype_to_cache_no_grouping(pArchetype, trackMembershipChange, assumeAbsent);
			}

			void add_archetype_to_transient_cache(const Archetype* pArchetype) {
				m_state.archetypeCache.push_back(pArchetype);
				m_state.exec.compIndicesPending = true;
				m_state.exec.inheritedOwnersPending = true;
				if (m_plan.ctx.data.groupBy != EntityBad) {
					const auto groupId = m_plan.ctx.data.groupByFunc(*m_plan.ctx.w, *pArchetype, m_plan.ctx.data.groupBy);
					m_state.grouped.archetypeGroupIds.push_back(groupId);
					m_state.grouped.dataPending = true;
				}
			}

			//! Returns cached group bounds for the currently selected group filter.
			//! The cached range is invalidated whenever group layout changes or the selected group id changes.
			GAIA_NODISCARD const GroupData* selected_group_data(GroupId runtimeGroupId) const {
				const_cast<QueryInfo*>(this)->ensure_group_data();
				if (m_plan.ctx.data.groupBy == EntityBad || runtimeGroupId == 0)
					return nullptr;

				if (!m_state.grouped.selectedGroupDataValid || m_state.grouped.selectedGroupData.groupId != runtimeGroupId) {
					uint32_t left = 0;
					uint32_t right = (uint32_t)m_state.grouped.archetypeGroupData.size();
					while (left < right) {
						const uint32_t mid = left + ((right - left) >> 1);
						const auto midGroupId = m_state.grouped.archetypeGroupData[mid].groupId;
						if (midGroupId < runtimeGroupId)
							left = mid + 1;
						else
							right = mid;
					}

					if (left < m_state.grouped.archetypeGroupData.size() &&
							m_state.grouped.archetypeGroupData[left].groupId == runtimeGroupId) {
						m_state.grouped.selectedGroupData = m_state.grouped.archetypeGroupData[left];
						m_state.grouped.selectedGroupDataValid = true;
						return &m_state.grouped.selectedGroupData;
					}

					m_state.grouped.selectedGroupData = {};
					m_state.grouped.selectedGroupDataValid = false;
					return nullptr;
				}

				return &m_state.grouped.selectedGroupData;
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
				if (archetypeIdx < m_state.exec.archetypeCompIndices.size())
					core::swap_erase(m_state.exec.archetypeCompIndices, archetypeIdx);
				if (archetypeIdx < m_state.exec.archetypeInheritedOwners.size())
					core::swap_erase(m_state.exec.archetypeInheritedOwners, archetypeIdx);
				if (archetypeIdx < m_state.grouped.archetypeGroupIds.size())
					core::swap_erase(m_state.grouped.archetypeGroupIds, archetypeIdx);
				if (archetypeIdx < m_state.nonTrivial.archetypeBarrierPasses.size())
					core::swap_erase(m_state.nonTrivial.archetypeBarrierPasses, archetypeIdx);

				if (m_plan.ctx.data.groupBy != EntityBad) {
					m_state.grouped.selectedGroupDataValid = false;
					m_state.grouped.archetypeGroupData.clear();
					m_state.grouped.dataPending = true;
					m_plan.ctx.data.flags |= QueryCtx::QueryFlags::SortGroups;
				}
				m_state.nonTrivial.barrierRelVersion = UINT32_MAX;
				m_state.nonTrivial.barrierEnabledVersion = UINT32_MAX;

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
				return ctxData.deps.has_dep_flag(QueryCtx::DependencyHasAdjunctTerms);
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
				const_cast<QueryInfo*>(this)->ensure_comp_indices();
				const auto& ctxData = m_state.exec.archetypeCompIndices[archetypeIdx];
				return {(const uint8_t*)&ctxData.indices[0], ChunkHeader::MAX_COMPONENTS};
			}

			std::span<const Entity> inherited_owner_view(uint32_t archetypeIdx) const {
				const_cast<QueryInfo*>(this)->ensure_inherited_owners();
				if (archetypeIdx >= m_state.exec.archetypeInheritedOwners.size())
					return {};
				const auto& ctxData = m_state.exec.archetypeInheritedOwners[archetypeIdx];
				return {(const Entity*)&ctxData.owners[0], ChunkHeader::MAX_COMPONENTS};
			}

			void ensure_depth_order_hierarchy_barrier_cache() {
				ensure_depth_order_hierarchy_barrier_cache_inter();
			}

			//! Returns a cached indices mapping view for an exact archetype match, or an empty span when absent.
			std::span<const uint8_t> try_indices_mapping_view(const Archetype* pArchetype) const {
				if (m_state.exec.compIndicesPending)
					return {};
				const auto archetypeIdx = core::get_index(m_state.archetypeCache, pArchetype);
				if (archetypeIdx == BadIndex)
					return {};
				return indices_mapping_view(archetypeIdx);
			}

			GAIA_NODISCARD GroupId group_id(uint32_t archetypeIdx) const {
				const_cast<QueryInfo*>(this)->ensure_group_data();
				GAIA_ASSERT(archetypeIdx < m_state.grouped.archetypeGroupIds.size());
				return m_state.grouped.archetypeGroupIds[archetypeIdx];
			}

			GAIA_NODISCARD bool barrier_passes(uint32_t archetypeIdx) const {
				const_cast<QueryInfo*>(this)->ensure_depth_order_hierarchy_barrier_cache();
				if (m_state.nonTrivial.archetypeBarrierPasses.empty())
					return true;
				GAIA_ASSERT(archetypeIdx < m_state.nonTrivial.archetypeBarrierPasses.size());
				return m_state.nonTrivial.archetypeBarrierPasses[archetypeIdx] != 0;
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

			GAIA_NODISCARD std::span<const SortData> cache_sort_view() const {
				return std::span{m_state.nonTrivial.archetypeSortData.data(), m_state.nonTrivial.archetypeSortData.size()};
			}

			GAIA_NODISCARD std::span<const GroupData> group_data_view() const {
				const_cast<QueryInfo*>(this)->ensure_group_data();
				return std::span{m_state.grouped.archetypeGroupData.data(), m_state.grouped.archetypeGroupData.size()};
			}
		};
	} // namespace ecs
} // namespace gaia
