#pragma once
#include "gaia/config/config.h"

#include <cstdarg>
#include <cstdint>
#include <type_traits>

#include "gaia/cnt/darray.h"
#include "gaia/cnt/map.h"
#include "gaia/cnt/sarray_ext.h"
#include "gaia/config/profiler.h"
#include "gaia/core/hashing_policy.h"
#include "gaia/core/utility.h"
#include "gaia/ecs/api.h"
#include "gaia/ecs/archetype.h"
#include "gaia/ecs/archetype_common.h"
#include "gaia/ecs/chunk.h"
#include "gaia/ecs/chunk_iterator.h"
#include "gaia/ecs/common.h"
#include "gaia/ecs/component.h"
#include "gaia/ecs/component_cache.h"
#include "gaia/ecs/id.h"
#include "gaia/ecs/query_cache.h"
#include "gaia/ecs/query_common.h"
#include "gaia/ecs/query_info.h"
#include "gaia/mt/threadpool.h"
#include "gaia/ser/ser_buffer_binary.h"
#include "gaia/ser/ser_ct.h"
#include "gaia/util/str.h"

namespace gaia {
	namespace ecs {
		class World;

		//! Maximal cap for cached traversed-source closure snapshots.
		inline static constexpr uint16_t MaxCacheSrcTrav = 32;

		enum class QueryExecType : uint32_t {
			// Main thread
			Serial,
			// Parallel, any core
			Parallel,
			// Parallel, perf cores only
			ParallelPerf,
			// Parallel, efficiency cores only
			ParallelEff,
			// Default execution type
			Default = Serial,
		};

		enum class QueryCacheMode : uint8_t {
			// Query uses the world's shared QueryCache and shared QueryInfo state.
			Shared,
			// Query owns a private QueryInfo and does not participate in shared cache propagation.
			Private,
		};

		enum class QueryCacheKind : uint8_t {
			// Use the engine's default cache behavior.
			// Recommended for most queries.
			// Allows all automatic cache layers:
			// - immediate structural cache
			// - lazy structural cache
			// - dynamic relation/source cache
			// Explicit traversed-source snapshot opt-ins are also allowed.
			Default,
			// Restrict the query to automatically derived cache layers only.
			// Use this when the engine should decide the cache behavior on its own.
			// Allows:
			// - immediate structural cache
			// - lazy structural cache
			// - dynamic relation/source cache derived automatically by query shape
			// Rejects explicit traversed-source snapshot opt-ins.
			Auto,
			// Require a fully immediate structural cache.
			// Use this only when query creation must fail unless the query can stay
			// on the immediate structural cache layer.
			// Allows only the immediate structural cache layer.
			// Query creation fails for lazy, dynamic, or explicit traversed-source snapshot layers.
			All
		};

		using QueryCachePolicy = QueryCtx::CachePolicy;

		namespace detail {
			//! Query command types
			enum QueryCmdType : uint8_t { ADD_ITEM, ADD_FILTER, SORT_BY, GROUP_BY, SET_GROUP };

			struct QueryCmd_AddItem {
				static constexpr QueryCmdType Id = QueryCmdType::ADD_ITEM;
				static constexpr bool InvalidatesHash = true;

				QueryInput item;

				void exec(QueryCtx& ctx) const {
					auto& ctxData = ctx.data;

#if GAIA_DEBUG
					// Unique component ids only
					GAIA_ASSERT(!core::has(ctxData.ids_view(), item.id));

					// There's a limit to the amount of query items which we can store
					if (ctxData.idsCnt >= MAX_ITEMS_IN_QUERY) {
						GAIA_ASSERT2(false, "Trying to create a query with too many components!");

						const auto* name = ctx.cc->get(item.id).name.str();
						GAIA_LOG_E("Trying to add component '%s' to an already full ECS query!", name);
						return;
					}
#endif

					// Build the read-write mask.
					// This will be used to determine what kind of access the user wants for a given component.
					const uint16_t isReadWrite = uint16_t(item.access == QueryAccess::Write);
					ctxData.readWriteMask |= (isReadWrite << ctxData.idsCnt);

					// The query engine is going to reorder the query items as necessary.
					// Remapping is used so the user can still identify the items according the order in which
					// they defined them when building the query.
					ctxData._remapping[ctxData.idsCnt] = ctxData.idsCnt;

					ctxData._ids[ctxData.idsCnt] = item.id;
					ctxData._terms[ctxData.idsCnt] = {item.id,				item.entSrc, item.entTrav, item.travKind,
																						item.travDepth, nullptr,		 item.op};
					++ctxData.idsCnt;
				}
			};

			struct QueryCmd_AddFilter {
				static constexpr QueryCmdType Id = QueryCmdType::ADD_FILTER;
				static constexpr bool InvalidatesHash = true;

				Entity comp;

				void exec(QueryCtx& ctx) const {
					auto& ctxData = ctx.data;

#if GAIA_DEBUG
					GAIA_ASSERT(core::has(ctxData.ids_view(), comp));
					GAIA_ASSERT(!core::has(ctxData.changed_view(), comp));

					// There's a limit to the amount of components which we can store
					if (ctxData.changedCnt >= MAX_ITEMS_IN_QUERY) {
						GAIA_ASSERT2(false, "Trying to create an filter query with too many components!");

						const auto* compName = ctx.cc->get(comp).name.str();
						GAIA_LOG_E("Trying to add component %s to an already full filter query!", compName);
						return;
					}

					uint32_t compIdx = 0;
					for (; compIdx < ctxData.idsCnt; ++compIdx)
						if (ctxData._ids[compIdx] == comp)
							break;

					// NOTE: Code bellow does the same as this commented piece.
					//       However, compilers can't quite optimize it as well because it does some more
					//       calculations. This is used often so go with the custom code.
					// const auto compIdx = core::get_index_unsafe(ids, comp);

					// Component has to be present in all/or lists.
					// Filtering by NOT/ANY doesn't make sense because those are not hard requirements.
					GAIA_ASSERT2(
							ctxData._terms[compIdx].op != QueryOpKind::Not && ctxData._terms[compIdx].op != QueryOpKind::Any,
							"Filtering by NOT/ANY doesn't make sense!");
					if (ctxData._terms[compIdx].op != QueryOpKind::Not && ctxData._terms[compIdx].op != QueryOpKind::Any) {
						ctxData._changed[ctxData.changedCnt++] = comp;
						return;
					}

					const auto* compName = ctx.cc->get(comp).name.str();
					GAIA_LOG_E("SetChangeFilter trying to filter component %s but it's not a part of the query!", compName);
#else
					ctxData._changed[ctxData.changedCnt++] = comp;
#endif
				}
			};

			struct QueryCmd_SortBy {
				static constexpr QueryCmdType Id = QueryCmdType::SORT_BY;
				static constexpr bool InvalidatesHash = true;

				Entity sortBy;
				TSortByFunc func;

				void exec(QueryCtx& ctx) const {
					auto& ctxData = ctx.data;
					ctxData.sortBy = sortBy;
					GAIA_ASSERT(func != nullptr);
					ctxData.sortByFunc = func;
				}
			};

			struct QueryCmd_GroupBy {
				static constexpr QueryCmdType Id = QueryCmdType::GROUP_BY;
				static constexpr bool InvalidatesHash = true;

				Entity groupBy;
				TGroupByFunc func;

				void exec(QueryCtx& ctx) const {
					auto& ctxData = ctx.data;
					ctxData.groupBy = groupBy;
					GAIA_ASSERT(func != nullptr);
					ctxData.groupByFunc = func; // group_by_func_default;
				}
			};

			struct QueryCmd_SetGroupId {
				static constexpr QueryCmdType Id = QueryCmdType::SET_GROUP;
				static constexpr bool InvalidatesHash = false;

				GroupId groupId;

				void exec(QueryCtx& ctx) const {
					auto& ctxData = ctx.data;
					ctxData.groupIdSet = groupId;
				}
			};

			template <bool Cached>
			struct QueryImplStorage {
				World* m_world{};
				//! QueryImpl cache (stable pointer to parent world's query cache)
				QueryCache* m_queryCache{};
				//! Query identity
				QueryIdentity m_q{};
				bool m_destroyed = false;

			public:
				QueryImplStorage() = default;
				~QueryImplStorage() {
					if (try_del_from_cache())
						ser_buffer_reset();
				}

				QueryImplStorage(QueryImplStorage&& other) {
					m_world = other.m_world;
					m_queryCache = other.m_queryCache;
					m_q = other.m_q;
					m_destroyed = other.m_destroyed;

					// Make sure old instance is invalidated
					other.m_q = {};
					other.m_destroyed = false;
				}
				QueryImplStorage& operator=(QueryImplStorage&& other) {
					GAIA_ASSERT(core::addressof(other) != this);

					m_world = other.m_world;
					m_queryCache = other.m_queryCache;
					m_q = other.m_q;
					m_destroyed = other.m_destroyed;

					// Make sure old instance is invalidated
					other.m_q = {};
					other.m_destroyed = false;

					return *this;
				}

				QueryImplStorage(const QueryImplStorage& other) {
					m_world = other.m_world;
					m_queryCache = other.m_queryCache;
					m_q = other.m_q;
					m_destroyed = other.m_destroyed;

					// Make sure to update the ref count of the cached query so
					// it doesn't get deleted by accident.
					if (!m_destroyed) {
						auto* pInfo = m_queryCache->try_get(m_q.handle);
						if (pInfo != nullptr)
							pInfo->add_ref();
					}
				}
				QueryImplStorage& operator=(const QueryImplStorage& other) {
					GAIA_ASSERT(core::addressof(other) != this);

					m_world = other.m_world;
					m_queryCache = other.m_queryCache;
					m_q = other.m_q;
					m_destroyed = other.m_destroyed;

					// Make sure to update the ref count of the cached query so
					// it doesn't get deleted by accident.
					if (!m_destroyed) {
						auto* pInfo = m_queryCache->try_get(m_q.handle);
						if (pInfo != nullptr)
							pInfo->add_ref();
					}

					return *this;
				}

				GAIA_NODISCARD World* world() {
					return m_world;
				}

				GAIA_NODISCARD QuerySerBuffer& ser_buffer() {
					return m_q.ser_buffer(m_world);
				}
				void ser_buffer_reset() {
					return m_q.ser_buffer_reset(m_world);
				}

				void init(World* world, QueryCache* queryCache) {
					m_world = world;
					m_queryCache = queryCache;
				}

				//! Release any data allocated by the query
				void reset() {
					auto& info = m_queryCache->get(m_q.handle);
					info.reset();
				}

				void allow_to_destroy_again() {
					m_destroyed = false;
				}

				//! Try delete the query from query cache
				GAIA_NODISCARD bool try_del_from_cache() {
					if (!m_destroyed)
						m_queryCache->del(m_q.handle);

					// Don't allow multiple calls to destroy to break the reference counter.
					// One object is only allowed to destroy once.
					m_destroyed = true;
					return false;
				}

				//! Invalidates the query handle
				void invalidate() {
					m_q.handle = {};
				}

				//! Returns true if the query is found in the query cache.
				GAIA_NODISCARD bool is_cached() const {
					auto* pInfo = m_queryCache->try_get(m_q.handle);
					return pInfo != nullptr;
				}

				//! Returns true if the query is ready to be used.
				GAIA_NODISCARD bool is_initialized() const {
					return m_world != nullptr && m_queryCache != nullptr;
				}
			};

			template <>
			struct QueryImplStorage<false> {
				QueryInfo m_queryInfo;

				QueryImplStorage() {
					m_queryInfo.idx = QueryIdBad;
					m_queryInfo.data.gen = QueryIdBad;
				}

				GAIA_NODISCARD World* world() {
					return m_queryInfo.world();
				}

				GAIA_NODISCARD QuerySerBuffer& ser_buffer() {
					return m_queryInfo.ser_buffer();
				}
				void ser_buffer_reset() {
					m_queryInfo.ser_buffer_reset();
				}

				void init(World* world) {
					m_queryInfo.init(world);
				}

				//! Release any data allocated by the query
				void reset() {
					m_queryInfo.reset();
				}

				//! Does nothing for uncached queries.
				GAIA_NODISCARD bool try_del_from_cache() {
					return false;
				}

				//! Does nothing for uncached queries.
				void invalidate() {}

				//! Does nothing for uncached queries.
				GAIA_NODISCARD bool is_cached() const {
					return false;
				}

				//! Returns true. Uncached queries are always considered initialized.
				GAIA_NODISCARD bool is_initialized() const {
					return true;
				}
			};

			template <bool UseCaching = true>
			class QueryImpl {
				static constexpr uint32_t ChunkBatchSize = 32;

				struct ChunkBatch {
					const Archetype* pArchetype;
					Chunk* pChunk;
					const uint8_t* pIndicesMapping;
					GroupId groupId;
					uint16_t from;
					uint16_t to;
				};

				using ChunkSpan = std::span<const Chunk*>;
				using ChunkSpanMut = std::span<Chunk*>;
				using ChunkBatchArray = cnt::sarray_ext<ChunkBatch, ChunkBatchSize>;
				using CmdFunc = void (*)(QuerySerBuffer& buffer, QueryCtx& ctx);

				struct DirectQueryScratch {
					cnt::darray<const Archetype*> archetypes;
					cnt::darray<Entity> entities;
					cnt::darray<Entity> bucketEntities;
					cnt::darray<uint32_t> counts;
					uint32_t seenVersion = 1;
				};

			private:
				//! Returns the per-thread scratch storage used by the direct entity-seeded query fast path.
				GAIA_NODISCARD static DirectQueryScratch& direct_query_scratch() {
					static thread_local DirectQueryScratch scratch;
					return scratch;
				}

				//! Grows the direct-query seen/count array so the given entity id can be addressed directly.
				static void ensure_direct_query_count_capacity(DirectQueryScratch& scratch, uint32_t entityId) {
					if (entityId < scratch.counts.size())
						return;

					const auto doubledSize = (uint32_t)scratch.counts.size() * 2U;
					const auto minSize = doubledSize > 64U ? doubledSize : 64U;
					const auto newSize = (entityId + 1U) > minSize ? (entityId + 1U) : minSize;
					const auto oldSize = (uint32_t)scratch.counts.size();
					scratch.counts.resize(newSize);
					for (uint32_t i = oldSize; i < newSize; ++i)
						scratch.counts[i] = 0;
				}

				//! Advances the scratch version used to deduplicate direct seeded entities without clearing on every call.
				GAIA_NODISCARD static uint32_t next_direct_query_seen_version(DirectQueryScratch& scratch) {
					update_version(scratch.seenVersion);
					if (scratch.seenVersion == 0) {
						scratch.seenVersion = 1;
						core::fill(scratch.counts.begin(), scratch.counts.end(), 0);
					}

					return scratch.seenVersion;
				}

				static constexpr CmdFunc CommandBufferRead[] = {
						// Add item
						[](QuerySerBuffer& buffer, QueryCtx& ctx) {
							QueryCmd_AddItem cmd;
							ser::load(buffer, cmd);
							cmd.exec(ctx);
						},
						// Add filter
						[](QuerySerBuffer& buffer, QueryCtx& ctx) {
							QueryCmd_AddFilter cmd;
							ser::load(buffer, cmd);
							cmd.exec(ctx);
						},
						// SortBy
						[](QuerySerBuffer& buffer, QueryCtx& ctx) {
							QueryCmd_SortBy cmd;
							ser::load(buffer, cmd);
							cmd.exec(ctx);
						},
						// GroupBy
						[](QuerySerBuffer& buffer, QueryCtx& ctx) {
							QueryCmd_GroupBy cmd;
							ser::load(buffer, cmd);
							cmd.exec(ctx);
						},
						// SetGroupId
						[](QuerySerBuffer& buffer, QueryCtx& ctx) {
							QueryCmd_SetGroupId cmd;
							ser::load(buffer, cmd);
							cmd.exec(ctx);
						} //
				}; // namespace detail

				//! Storage for data based on whether Caching is used or not
				QueryImplStorage<UseCaching> m_storage;
				//! World version (stable pointer to parent world's m_nextArchetypeId)
				ArchetypeId* m_nextArchetypeId{};
				//! World version (stable pointer to parent world's world version)
				uint32_t* m_worldVersion{};
				//! Map of archetypes (stable pointer to parent world's archetype array)
				const ArchetypeMapById* m_archetypes{};
				//! Map of component ids to archetypes (stable pointer to parent world's archetype component-to-archetype map)
				const EntityToArchetypeMap* m_entityToArchetypeMap{};
				//! All world archetypes
				const ArchetypeDArray* m_allArchetypes{};
				//! Optional user-defined names for Var0..Var7.
				cnt::sarray<util::str, 8> m_varNames;
				//! Bitmask of variable names set in m_varNames.
				uint8_t m_varNamesMask = 0;
				//! Runtime variable bindings for Var0..Var7.
				cnt::sarray<Entity, 8> m_varBindings;
				//! Bitmask of variable bindings set in m_varBindings.
				uint8_t m_varBindingsMask = 0;
				//! Batches used for parallel query processing
				//! TODO: This is just temporary until a smarter system is introduced
				cnt::darray<ChunkBatch> m_batches;
				//! User-requested cache-kind restriction.
				QueryCacheKind m_cacheKind = QueryCacheKind::Default;
				//! Traversed-source closure size allowed for explicit snapshot reuse.
				uint16_t m_cacheSrcTrav = 0;

				//! BFS-specific cache and scratch storage, allocated on-demand.
				struct EachBfsData {
					//! Cached raw entity list for each_bfs.
					cnt::darray<Entity> cachedInput;
					//! Cached ordered entity list for each_bfs.
					cnt::darray<Entity> cachedOutput;
					//! Cached relation used by each_bfs.
					Entity cachedRelation = EntityBad;
					//! Cached constraints used by each_bfs.
					Constraints cachedConstraints = Constraints::EnabledOnly;
					//! Relation version when each_bfs cache was produced.
					uint32_t cachedRelationVersion = 0;
					//! World version snapshot used for chunk-structural change checks.
					uint32_t cachedEntityVersion = 0;
					//! Cached matched chunk pointers used by each_bfs fast-path.
					cnt::darray<const Chunk*> cachedChunks;
					//! True if each_bfs cache is valid.
					bool cacheValid = false;
					//! Scratch entities for each_bfs computation.
					cnt::darray<Entity> scratchEntities;
					//! Scratch matched chunk pointers for each_bfs fast-path.
					cnt::darray<const Chunk*> scratchChunks;
					//! Scratch indegree array for each_bfs computation.
					cnt::darray<uint32_t> scratchIndegree;
					//! Scratch outdegree array for each_bfs computation.
					cnt::darray<uint32_t> scratchOutdegree;
					//! Scratch adjacency offsets (CSR) for each_bfs computation.
					cnt::darray<uint32_t> scratchOffsets;
					//! Scratch adjacency write cursor (CSR fill stage).
					cnt::darray<uint32_t> scratchWriteCursor;
					//! Scratch adjacency edges (CSR) for each_bfs computation.
					cnt::darray<uint32_t> scratchEdges;
					//! Scratch level queue for current BFS layer.
					cnt::darray<uint32_t> scratchCurrLevel;
					//! Scratch level queue for next BFS layer.
					cnt::darray<uint32_t> scratchNextLevel;
				};

				//! Optional on-demand storage for BFS iteration data.
				struct EachBfsDataHolder {
					EachBfsData* pData = nullptr;

					EachBfsDataHolder() = default;

					~EachBfsDataHolder() {
						delete pData;
					}

					EachBfsDataHolder(const EachBfsDataHolder& other) {
						if (other.pData != nullptr)
							pData = new EachBfsData(*other.pData);
					}

					EachBfsDataHolder& operator=(const EachBfsDataHolder& other) {
						if (core::addressof(other) == this)
							return *this;

						if (other.pData == nullptr) {
							delete pData;
							pData = nullptr;
							return *this;
						}

						if (pData == nullptr)
							pData = new EachBfsData(*other.pData);
						else
							*pData = *other.pData;

						return *this;
					}

					EachBfsDataHolder(EachBfsDataHolder&& other) noexcept: pData(other.pData) {
						other.pData = nullptr;
					}

					EachBfsDataHolder& operator=(EachBfsDataHolder&& other) noexcept {
						if (core::addressof(other) == this)
							return *this;

						delete pData;
						pData = other.pData;
						other.pData = nullptr;
						return *this;
					}

					GAIA_NODISCARD EachBfsData* get() {
						return pData;
					}

					GAIA_NODISCARD const EachBfsData* get() const {
						return pData;
					}

					GAIA_NODISCARD EachBfsData& ensure() {
						if (pData == nullptr)
							pData = new EachBfsData();
						return *pData;
					}

					void reset() {
						delete pData;
						pData = nullptr;
					}
				};

				EachBfsDataHolder m_eachBfsData;

				//--------------------------------------------------------------------------------
			public:
				static inline bool SilenceInvalidCacheKindAssertions = false;

				//! Fetches the QueryInfo object.
				//! \return QueryInfo object
				QueryInfo& fetch() {
					if constexpr (UseCaching) {
						GAIA_PROF_SCOPE(query::fetch);

						// Make sure the query was created by World::query()
						GAIA_ASSERT(m_storage.is_initialized());

						// If queryId is set it means QueryInfo was already created.
						// This is the common case for cached queries.
						if GAIA_LIKELY (m_storage.m_q.handle.id() != QueryIdBad) {
							auto* pQueryInfo = m_storage.m_queryCache->try_get(m_storage.m_q.handle);

							// The only time when this can be nullptr is just once after Query::destroy is called.
							if GAIA_LIKELY (pQueryInfo != nullptr) {
								recommit(pQueryInfo->ctx());
								apply_runtime_var_state(pQueryInfo->ctx());
								return *pQueryInfo;
							}

							m_storage.invalidate();
						}

						// No queryId is set which means QueryInfo needs to be created
						QueryCtx ctx;
						ctx.init(m_storage.world());
						commit(ctx);
						apply_runtime_var_state(ctx);
						auto& queryInfo =
								m_storage.m_queryCache->add(GAIA_MOV(ctx), *m_entityToArchetypeMap, all_archetypes_view());
						m_storage.m_q.handle = QueryInfo::handle(queryInfo);
						m_storage.allow_to_destroy_again();
						return queryInfo;
					} else {
						GAIA_PROF_SCOPE(query::fetchu);

						if GAIA_UNLIKELY (m_storage.m_queryInfo.ctx().q.handle.id() == QueryIdBad) {
							QueryCtx ctx;
							ctx.init(m_storage.world());
							commit(ctx);
							apply_runtime_var_state(ctx);
							m_storage.m_queryInfo =
									QueryInfo::create(QueryId{}, GAIA_MOV(ctx), *m_entityToArchetypeMap, all_archetypes_view());
						} else {
							recommit(m_storage.m_queryInfo.ctx());
							apply_runtime_var_state(m_storage.m_queryInfo.ctx());
						}
						return m_storage.m_queryInfo;
					}
				}

				void match_all(QueryInfo& queryInfo) {
					if (!validate_cache_kind(queryInfo.ctx())) {
						GAIA_ASSERT(SilenceInvalidCacheKindAssertions && "Invalid cache_kind selected for a query");
						queryInfo.reset();
						return;
					}

					queryInfo.ensure_matches(*m_entityToArchetypeMap, all_archetypes_view(), last_archetype_id());
					if constexpr (UseCaching) {
						m_storage.m_queryCache->sync_archetype_cache(queryInfo);
					}
				}

				void match_one(QueryInfo& queryInfo, const Archetype& archetype, EntitySpan targetEntities) {
					queryInfo.ensure_matches_one(archetype, targetEntities);
				}

				//--------------------------------------------------------------------------------

				GAIA_NODISCARD static constexpr QueryCacheMode cache_mode() {
					return UseCaching ? QueryCacheMode::Shared : QueryCacheMode::Private;
				}

				GAIA_NODISCARD QueryCachePolicy cache_policy() {
					return fetch().cache_policy();
				}

				//! Enables traversed-source snapshot reuse and caps the cached source closure size.
				//! This only matters for queries using traversed sources (for example src(...).trav()).
				//! For queries without source traversal the value is normalized away, so it does not
				//! affect shared cache identity.
				//! Use this only when traversed source closures stay small and stable enough for
				//! snapshot reuse to be cheaper than rebuilding them on demand.
				template <bool U = UseCaching, std::enable_if_t<U, int> = 0>
				QueryImpl& cache_src_trav(uint16_t maxItems) {
					if (m_cacheSrcTrav == maxItems)
						return *this;

					if (maxItems > MaxCacheSrcTrav) {
						GAIA_ASSERT(false && "cache_src_trav should be a value smaller than MaxCacheSrcTrav");
						maxItems = MaxCacheSrcTrav;
					}

					invalidate_each_bfs_cache();
					m_cacheSrcTrav = maxItems;
					m_storage.invalidate();
					return *this;
				}

				//! Returns the traversed-source snapshot cap.
				//! 0 disables explicit traversed-source snapshot caching.
				template <bool U = UseCaching, std::enable_if_t<U, int> = 0>
				GAIA_NODISCARD uint16_t cache_src_trav() const {
					return m_cacheSrcTrav;
				}

				template <bool U = UseCaching, std::enable_if_t<U, int> = 0>
				QueryImpl& cache_kind(QueryCacheKind cacheKind) {
					if (m_cacheKind == cacheKind)
						return *this;

					invalidate_each_bfs_cache();
					m_cacheKind = cacheKind;
					if constexpr (UseCaching)
						m_storage.invalidate();
					else
						m_storage.reset();

					return *this;
				}

				template <bool U = UseCaching, std::enable_if_t<U, int> = 0>
				GAIA_NODISCARD QueryCacheKind cache_kind() const {
					return m_cacheKind;
				}

				GAIA_NODISCARD bool valid() {
					if constexpr (UseCaching) {
						return validate_cache_kind(fetch().ctx());
					} else {
						return true;
					}
				}

				//--------------------------------------------------------------------------------
			private:
				//! Returns true when the query requests traversed-source snapshots beyond the default automatic cache layers.
				GAIA_NODISCARD bool uses_manual_src_trav_cache(const QueryCtx& ctx) const {
					return m_cacheSrcTrav != 0 && ctx.data.deps.has(QueryCtx::DependencyHasSourceTerms) &&
								 ctx.data.deps.has(QueryCtx::DependencyHasTraversalTerms);
				}

				//! Returns true when the query uses the immediate structural cache layer.
				GAIA_NODISCARD static bool uses_im_cache(const QueryCtx& ctx) {
					return ctx.data.cachePolicy == QueryCachePolicy::Immediate;
				}

				//! Returns true when the query uses the lazy structural cache layer.
				GAIA_NODISCARD static bool uses_lazy_cache(const QueryCtx& ctx) {
					return ctx.data.cachePolicy == QueryCachePolicy::Lazy;
				}

				//! Returns true when the query uses the dynamic cache layer.
				GAIA_NODISCARD static bool uses_dyn_cache(const QueryCtx& ctx) {
					return ctx.data.cachePolicy == QueryCachePolicy::Dynamic;
				}

				//! Validates that the requested public cache kind can be satisfied by the current query shape.
				GAIA_NODISCARD bool validate_cache_kind(const QueryCtx& ctx) const {
					if constexpr (!UseCaching)
						return true;

					if (m_cacheKind == QueryCacheKind::Auto) {
						const bool usesImmediateLayer = uses_im_cache(ctx);
						const bool usesLazyLayer = uses_lazy_cache(ctx);
						const bool usesDynamicLayer = uses_dyn_cache(ctx);
						const bool usesExplicitSrcTravLayer = uses_manual_src_trav_cache(ctx);
						return (usesImmediateLayer || usesLazyLayer || usesDynamicLayer) && !usesExplicitSrcTravLayer;
					}

					if (m_cacheKind == QueryCacheKind::All) {
						const bool usesImmediateLayer = uses_im_cache(ctx);
						const bool usesExplicitSrcTravLayer = uses_manual_src_trav_cache(ctx);
						return usesImmediateLayer && !usesExplicitSrcTravLayer;
					}

					return true;
				}

				GAIA_NODISCARD EachBfsData* each_bfs_data() {
					return m_eachBfsData.get();
				}

				GAIA_NODISCARD const EachBfsData* each_bfs_data() const {
					return m_eachBfsData.get();
				}

				GAIA_NODISCARD EachBfsData& ensure_each_bfs_data() {
					return m_eachBfsData.ensure();
				}

				void invalidate_each_bfs_cache() {
					auto* pBfsData = each_bfs_data();
					if (pBfsData != nullptr)
						pBfsData->cacheValid = false;
				}

				ArchetypeId last_archetype_id() const {
					return *m_nextArchetypeId - 1;
				}

				GAIA_NODISCARD std::span<const Archetype*> all_archetypes_view() const {
					GAIA_ASSERT(m_allArchetypes != nullptr);
					return {(const Archetype**)m_allArchetypes->data(), m_allArchetypes->size()};
				}

				GAIA_NODISCARD static bool is_query_var_entity(Entity entity) {
					return is_variable((EntityId)entity.id());
				}

				GAIA_NODISCARD static uint32_t query_var_idx(Entity entity) {
					GAIA_ASSERT(is_query_var_entity(entity));
					return (uint32_t)(entity.id() - Var0.id());
				}

				GAIA_NODISCARD Entity query_var_entity(uint32_t idx) {
					GAIA_ASSERT(idx < 8);
					return entity_from_id((const World&)*m_storage.world(), (EntityId)(Var0.id() + idx));
				}

				GAIA_NODISCARD static util::str_view normalize_var_name(util::str_view name) {
					auto trimmed = util::trim(name);
					if (trimmed.empty())
						return {};

					if (trimmed.data()[0] == '$') {
						if (trimmed.size() == 1)
							return {};
						trimmed = util::str_view(trimmed.data() + 1, trimmed.size() - 1);
					}

					return util::trim(trimmed);
				}

				GAIA_NODISCARD static bool is_reserved_var_name(util::str_view varName) {
					return varName == "this";
				}

				GAIA_NODISCARD Entity find_var_by_name(util::str_view rawName) {
					const auto varName = normalize_var_name(rawName);
					if (varName.empty() || is_reserved_var_name(varName))
						return EntityBad;

					GAIA_FOR(8) {
						const auto bit = (uint8_t(1) << i);
						if ((m_varNamesMask & bit) == 0)
							continue;
						if (m_varNames[i] == varName)
							return query_var_entity(i);
					}

					return EntityBad;
				}

				bool set_var_name_internal(Entity varEntity, util::str_view rawName) {
					if (!is_query_var_entity(varEntity))
						return false;

					const auto varName = normalize_var_name(rawName);
					if (varName.empty() || is_reserved_var_name(varName))
						return false;

					const auto idx = query_var_idx(varEntity);
					const auto bit = (uint8_t(1) << idx);

					GAIA_FOR(8) {
						if (i == idx)
							continue;

						const auto otherBit = (uint8_t(1) << i);
						if ((m_varNamesMask & otherBit) == 0)
							continue;
						if (!(m_varNames[i] == varName))
							continue;

						GAIA_ASSERT2(false, "Variable name is already assigned to a different query variable");
						return false;
					}

					m_varNames[idx].assign(varName);
					m_varNamesMask |= bit;
					return true;
				}

				void apply_runtime_var_state(QueryCtx& ctx) {
					auto& data = ctx.data;
					data.varBindingMask = m_varBindingsMask;
					GAIA_FOR(8) {
						const auto bit = (uint8_t(1) << i);
						if ((m_varBindingsMask & bit) == 0)
							continue;
						data.varBindings[i] = m_varBindings[i];
					}
				}

				template <typename T>
				void add_cmd(T& cmd) {
					invalidate_each_bfs_cache();

					// Make sure to invalidate if necessary.
					if constexpr (T::InvalidatesHash)
						m_storage.invalidate();

					auto& serBuffer = m_storage.ser_buffer();
					ser::save(serBuffer, T::Id);
					ser::save(serBuffer, T::InvalidatesHash);
					ser::save(serBuffer, cmd);
				}

				void add_inter(QueryInput item) {
					// When excluding or using ANY terms make sure the access type is None.
					GAIA_ASSERT((item.op != QueryOpKind::Not && item.op != QueryOpKind::Any) || item.access == QueryAccess::None);

					QueryCmd_AddItem cmd{item};
					add_cmd(cmd);
				}

				GAIA_NODISCARD static QueryAccess normalize_access(QueryOpKind op, Entity entity, QueryAccess access) {
					if (op == QueryOpKind::Not || op == QueryOpKind::Any || entity.pair())
						return QueryAccess::None;

					// Non-pair ALL/OR terms default to Read access when unspecified.
					if (access == QueryAccess::None)
						return QueryAccess::Read;

					return access;
				}

				void add_entity_term(QueryOpKind op, Entity entity, const QueryTermOptions& options) {
					const auto access = normalize_access(op, entity, options.access);
					add({op, access, entity, options.entSrc, options.entTrav, options.travKind, options.travDepth});
				}

				template <typename T>
				void add_inter(QueryOpKind op) {
					Entity e;

					if constexpr (is_pair<T>::value) {
						// Make sure the components are always registered
						const auto& desc_rel = comp_cache_add<typename T::rel_type>(*m_storage.world());
						const auto& desc_tgt = comp_cache_add<typename T::tgt_type>(*m_storage.world());

						e = Pair(desc_rel.entity, desc_tgt.entity);
					} else {
						// Make sure the component is always registered
						const auto& desc = comp_cache_add<T>(*m_storage.world());
						e = desc.entity;
					}

					// Determine the access type
					QueryAccess access = QueryAccess::None;
					if (op != QueryOpKind::Not && op != QueryOpKind::Any) {
						constexpr auto isReadWrite = core::is_mut_v<T>;
						access = isReadWrite ? QueryAccess::Write : QueryAccess::Read;
					}

					add_inter({op, access, e});
				}

				template <typename T>
				void add_inter(QueryOpKind op, const QueryTermOptions& options) {
					Entity e;

					if constexpr (is_pair<T>::value) {
						// Make sure the components are always registered
						const auto& desc_rel = comp_cache_add<typename T::rel_type>(*m_storage.world());
						const auto& desc_tgt = comp_cache_add<typename T::tgt_type>(*m_storage.world());

						e = Pair(desc_rel.entity, desc_tgt.entity);
					} else {
						// Make sure the component is always registered
						const auto& desc = comp_cache_add<T>(*m_storage.world());
						e = desc.entity;
					}

					QueryAccess access = QueryAccess::None;
					if (op != QueryOpKind::Not && op != QueryOpKind::Any) {
						if (options.access != QueryAccess::None)
							access = options.access;
						else {
							constexpr auto isReadWrite = core::is_mut_v<T>;
							access = isReadWrite ? QueryAccess::Write : QueryAccess::Read;
						}
					}

					add_inter(
							{op, normalize_access(op, e, access), e, options.entSrc, options.entTrav, options.travKind,
							 options.travDepth});
				}

				template <typename Rel, typename Tgt>
				void add_inter(QueryOpKind op) {
					using UO_Rel = typename component_type_t<Rel>::TypeOriginal;
					using UO_Tgt = typename component_type_t<Tgt>::TypeOriginal;
					static_assert(core::is_raw_v<UO_Rel>, "Use add() with raw types only");
					static_assert(core::is_raw_v<UO_Tgt>, "Use add() with raw types only");

					// Make sure the component is always registered
					const auto& descRel = comp_cache_add<Rel>(*m_storage.world());
					const auto& descTgt = comp_cache_add<Tgt>(*m_storage.world());

					// Determine the access type
					QueryAccess access = QueryAccess::None;
					if (op != QueryOpKind::Not && op != QueryOpKind::Any) {
						constexpr auto isReadWrite = core::is_mut_v<UO_Rel> || core::is_mut_v<UO_Tgt>;
						access = isReadWrite ? QueryAccess::Write : QueryAccess::Read;
					}

					add_inter({op, access, {descRel.entity, descTgt.entity}});
				}

				//--------------------------------------------------------------------------------

				void changed_inter(Entity entity) {
					QueryCmd_AddFilter cmd{entity};
					add_cmd(cmd);
				}

				template <typename T>
				void changed_inter() {
					using UO = typename component_type_t<T>::TypeOriginal;
					static_assert(core::is_raw_v<UO>, "Use changed() with raw types only");

					// Make sure the component is always registered
					const auto& desc = comp_cache_add<T>(*m_storage.world());
					changed_inter(desc.entity);
				}

				template <typename Rel, typename Tgt>
				void changed_inter() {
					using UO_Rel = typename component_type_t<Rel>::TypeOriginal;
					using UO_Tgt = typename component_type_t<Tgt>::TypeOriginal;
					static_assert(core::is_raw_v<UO_Rel>, "Use changed() with raw types only");
					static_assert(core::is_raw_v<UO_Tgt>, "Use changed() with raw types only");

					// Make sure the component is always registered
					const auto& descRel = comp_cache_add<Rel>(*m_storage.world());
					const auto& descTgt = comp_cache_add<Tgt>(*m_storage.world());
					changed_inter({descRel.entity, descTgt.entity});
				}

				//--------------------------------------------------------------------------------

				void sort_by_inter(Entity entity, TSortByFunc func) {
					QueryCmd_SortBy cmd{entity, func};
					add_cmd(cmd);
				}

				template <typename T>
				void sort_by_inter(TSortByFunc func) {
					using UO = typename component_type_t<T>::TypeOriginal;
					if constexpr (std::is_same_v<UO, Entity>) {
						sort_by_inter(EntityBad, func);
					} else {
						static_assert(core::is_raw_v<UO>, "Use changed() with raw types only");

						// Make sure the component is always registered
						const auto& desc = comp_cache_add<T>(*m_storage.world());

						sort_by_inter(desc.entity, func);
					}
				}

				template <typename Rel, typename Tgt>
				void sort_by_inter(TSortByFunc func) {
					using UO_Rel = typename component_type_t<Rel>::TypeOriginal;
					using UO_Tgt = typename component_type_t<Tgt>::TypeOriginal;
					static_assert(core::is_raw_v<UO_Rel>, "Use group_by() with raw types only");
					static_assert(core::is_raw_v<UO_Tgt>, "Use group_by() with raw types only");

					// Make sure the component is always registered
					const auto& descRel = comp_cache_add<Rel>(*m_storage.world());
					const auto& descTgt = comp_cache_add<Tgt>(*m_storage.world());

					sort_by_inter({descRel.entity, descTgt.entity}, func);
				}

				//--------------------------------------------------------------------------------

				void group_by_inter(Entity entity, TGroupByFunc func) {
					QueryCmd_GroupBy cmd{entity, func};
					add_cmd(cmd);
				}

				template <typename T>
				void group_by_inter(Entity entity, TGroupByFunc func) {
					using UO = typename component_type_t<T>::TypeOriginal;
					static_assert(core::is_raw_v<UO>, "Use changed() with raw types only");

					group_by_inter(entity, func);
				}

				template <typename Rel, typename Tgt>
				void group_by_inter(TGroupByFunc func) {
					using UO_Rel = typename component_type_t<Rel>::TypeOriginal;
					using UO_Tgt = typename component_type_t<Tgt>::TypeOriginal;
					static_assert(core::is_raw_v<UO_Rel>, "Use group_by() with raw types only");
					static_assert(core::is_raw_v<UO_Tgt>, "Use group_by() with raw types only");

					// Make sure the component is always registered
					const auto& descRel = comp_cache_add<Rel>(*m_storage.world());
					const auto& descTgt = comp_cache_add<Tgt>(*m_storage.world());

					group_by_inter({descRel.entity, descTgt.entity}, func);
				}

				//--------------------------------------------------------------------------------

				void set_group_id_inter(GroupId groupId) {
					// Dummy usage of GroupIdMax to avoid warning about unused constant
					(void)GroupIdMax;

					QueryCmd_SetGroupId cmd{groupId};
					add_cmd(cmd);
				}

				void set_group_id_inter(Entity groupId) {
					set_group_id_inter(groupId.value());
				}

				template <typename T>
				void set_group_id_inter() {
					using UO = typename component_type_t<T>::TypeOriginal;
					static_assert(core::is_raw_v<UO>, "Use group_id() with raw types only");

					// Make sure the component is always registered
					const auto& desc = comp_cache_add<T>(*m_storage.world());
					set_group_inter(desc.entity);
				}

				//--------------------------------------------------------------------------------

				void commit(QueryCtx& ctx) {
					GAIA_PROF_SCOPE(query::commit);

#if GAIA_ASSERT_ENABLED
					if constexpr (UseCaching) {
						GAIA_ASSERT(m_storage.m_q.handle.id() == QueryIdBad);
					} else {
						GAIA_ASSERT(m_storage.m_queryInfo.idx == QueryIdBad);
					}
#endif

					auto& serBuffer = m_storage.ser_buffer();

					// Read data from buffer and execute the command stored in it
					serBuffer.seek(0);
					while (serBuffer.tell() < serBuffer.bytes()) {
						QueryCmdType id{};
						bool invalidatesHash = false;
						ser::load(serBuffer, id);
						ser::load(serBuffer, invalidatesHash);
						(void)invalidatesHash; // We don't care about this during commit
						CommandBufferRead[id](serBuffer, ctx);
					}

					// Calculate the lookup hash from the provided context
					if constexpr (UseCaching) {
						ctx.data.cacheSrcTrav = m_cacheSrcTrav;
						normalize_cache_src_trav(ctx);
						auto& ctxData = ctx.data;
						if (ctxData.changedCnt > 1) {
							core::sort(ctxData._changed.data(), ctxData._changed.data() + ctxData.changedCnt, SortComponentCond{});
						}
						calc_lookup_hash(ctx);
					}

					// We can free all temporary data now
					m_storage.ser_buffer_reset();

					// Refresh the context
					ctx.refresh();
				}

				void recommit(QueryCtx& ctx) {
					GAIA_PROF_SCOPE(query::recommit);

					auto& serBuffer = m_storage.ser_buffer();

					// Read data from buffer and execute the command stored in it
					serBuffer.seek(0);
					while (serBuffer.tell() < serBuffer.bytes()) {
						QueryCmdType id{};
						bool invalidatesHash = false;
						ser::load(serBuffer, id);
						ser::load(serBuffer, invalidatesHash);
						// Hash recalculation is not accepted here
						GAIA_ASSERT(!invalidatesHash);
						if (invalidatesHash)
							return;
						CommandBufferRead[id](serBuffer, ctx);
					}
					if constexpr (UseCaching)
						ctx.data.cacheSrcTrav = m_cacheSrcTrav;
					if constexpr (UseCaching)
						normalize_cache_src_trav(ctx);

					// We can free all temporary data now
					m_storage.ser_buffer_reset();
				}

				//--------------------------------------------------------------------------------
			public:
#if GAIA_ASSERT_ENABLED
				//! Unpacks the parameter list \param types into query \param query and performs has_all for each of them
				template <typename... T>
				GAIA_NODISCARD bool unpack_args_into_query_has_all(
						const QueryInfo& queryInfo, [[maybe_unused]] core::func_type_list<T...> types) const {
					if constexpr (sizeof...(T) > 0)
						return queryInfo.template has_all<T...>();
					else
						return true;
				}
#endif

				//--------------------------------------------------------------------------------

				GAIA_NODISCARD static bool match_filters(const Chunk& chunk, const QueryInfo& queryInfo) {
					GAIA_ASSERT(!chunk.empty() && "match_filters called on an empty chunk");

					const auto queryVersion = queryInfo.world_version();
					const auto& filtered = queryInfo.ctx().data.changed_view();

					// Skip unchanged chunks
					if (filtered.empty())
						return false;

					const auto filteredCnt = (uint32_t)filtered.size();
					auto ids = chunk.ids_view();

					// This is the hot path for most change-filter queries.
					if (filteredCnt == 1) {
						const auto compIdx = core::get_index(ids, filtered[0]);
						if (compIdx != BadIndex && chunk.changed(queryVersion, compIdx))
							return true;

						return chunk.changed(queryInfo.world_version());
					}

					// See if any component has changed
					uint32_t lastIdx = 0;
					for (const auto comp: filtered) {
						uint32_t compIdx = BadIndex;
						if (lastIdx < (uint32_t)ids.size()) {
							const auto suffixIdx =
									core::get_index(std::span<const Entity>(ids.data() + lastIdx, ids.size() - lastIdx), comp);
							if (suffixIdx != BadIndex)
								compIdx = lastIdx + suffixIdx;
						}

						// Fallback for queries where change-filters are not monotonic in chunk column order
						// (e.g. OR-driven layouts).
						if (compIdx == BadIndex)
							compIdx = core::get_index(ids, comp);
						if (compIdx == BadIndex)
							continue;

						if (chunk.changed(queryVersion, compIdx))
							return true;

						lastIdx = compIdx;
					}

					// If the component hasn't been modified, the entity itself still might have been moved.
					// For that reason we also need to check the entity version.
					return chunk.changed(queryInfo.world_version());
				}

				GAIA_NODISCARD bool can_process_archetype(const Archetype& archetype) const {
					// Archetypes requested for deletion are skipped for processing
					return !archetype.is_req_del();
				}

				//--------------------------------------------------------------------------------

				//! Execute the functor for a given chunk batch
				template <typename Func, typename TIter>
				static void run_query_func(World* pWorld, Func func, ChunkBatch& batch) {
					TIter it;
					it.set_world(pWorld);
					it.set_archetype(batch.pArchetype);
					it.set_chunk(batch.pChunk, batch.from, batch.to);
					it.set_group_id(batch.groupId);
					it.set_remapping_indices(batch.pIndicesMapping);
					func(it);
				}

				//! Execute the functor for a given chunk batch
				template <typename Func, typename TIter>
				static void run_query_arch_func(World* pWorld, Func func, ChunkBatch& batch) {
					TIter it;
					it.set_world(pWorld);
					it.set_archetype(batch.pArchetype);
					// it.set_chunk(nullptr, 0, 0); We do not need this, and calling it would assert
					it.set_group_id(batch.groupId);
					it.set_remapping_indices(batch.pIndicesMapping);
					func(it);
				}

				//! Execute the functor in batches
				template <typename Func, typename TIter>
				static void run_query_func(World* pWorld, Func func, std::span<ChunkBatch> batches) {
					GAIA_PROF_SCOPE(query::run_query_func);

					const auto chunkCnt = batches.size();
					GAIA_ASSERT(chunkCnt > 0);

					// We only have one chunk to process
					if GAIA_UNLIKELY (chunkCnt == 1) {
						run_query_func<Func, TIter>(pWorld, func, batches[0]);
						return;
					}

					// We have many chunks to process.
					// Chunks might be located at different memory locations. Not even in the same memory page.
					// Therefore, to make it easier for the CPU we give it a hint that we want to prefetch data
					// for the next chunk explicitly so we do not end up stalling later.
					// Note, this is a micro optimization and on average it brings no performance benefit. It only
					// helps with edge cases.
					// Let us be conservative for now and go with T2. That means we will try to keep our data at
					// least in L3 cache or higher.
					gaia::prefetch(batches[1].pChunk, PrefetchHint::PREFETCH_HINT_T2);
					run_query_func<Func, TIter>(pWorld, func, batches[0]);

					uint32_t chunkIdx = 1;
					for (; chunkIdx < chunkCnt - 1; ++chunkIdx) {
						gaia::prefetch(batches[chunkIdx + 1].pChunk, PrefetchHint::PREFETCH_HINT_T2);
						run_query_func<Func, TIter>(pWorld, func, batches[chunkIdx]);
					}

					run_query_func<Func, TIter>(pWorld, func, batches[chunkIdx]);
				}

				//------------------------------------------------

				template <bool HasFilters, typename TIter, typename Func>
				void run_query_batch_no_group_id(
						const QueryInfo& queryInfo, const uint32_t idxFrom, const uint32_t idxTo, Func func) {
					GAIA_PROF_SCOPE(query::run_query_batch_no_group_id);

					// We are batching by chunks. Some of them might contain only few items but this state is only
					// temporary because defragmentation runs constantly and keeps things clean.
					ChunkBatchArray chunkBatches;

					auto cacheView = queryInfo.cache_archetype_view();
					auto sortView = queryInfo.cache_sort_view();

					lock(*m_storage.world());

					if (!sortView.empty()) {
						for (const auto& view: sortView) {
							const auto chunkEntitiesCnt = TIter::size(view.pChunk);
							if GAIA_UNLIKELY (chunkEntitiesCnt == 0)
								continue;

							const auto viewFrom = view.startRow;
							const auto viewTo = (uint16_t)(view.startRow + view.count);

							const auto minStartRow = TIter::start_index(view.pChunk);
							const auto minEndRow = TIter::end_index(view.pChunk);
							const auto startRow = core::get_max(minStartRow, viewFrom);
							const auto endRow = core::get_min(minEndRow, viewTo);
							const auto totalRows = endRow - startRow;
							if (totalRows == 0)
								continue;

							if constexpr (HasFilters) {
								if (!match_filters(*view.pChunk, queryInfo))
									continue;
							}

							auto* pArchetype = const_cast<Archetype*>(cacheView[view.archetypeIdx]);
							auto indicesView = queryInfo.indices_mapping_view(view.archetypeIdx);

							chunkBatches.push_back(ChunkBatch{pArchetype, view.pChunk, indicesView.data(), 0U, startRow, endRow});

							if GAIA_UNLIKELY (chunkBatches.size() == chunkBatches.max_size()) {
								run_query_func<Func, TIter>(m_storage.world(), func, {chunkBatches.data(), chunkBatches.size()});
								chunkBatches.clear();
							}
						}
					} else {
						for (uint32_t i = idxFrom; i < idxTo; ++i) {
							auto* pArchetype = const_cast<Archetype*>(cacheView[i]);
							if GAIA_UNLIKELY (!can_process_archetype(*pArchetype))
								continue;

							auto indicesView = queryInfo.indices_mapping_view(i);
							const auto& chunks = pArchetype->chunks();
							uint32_t chunkOffset = 0;
							uint32_t itemsLeft = chunks.size();
							while (itemsLeft > 0) {
								const auto maxBatchSize = chunkBatches.max_size() - chunkBatches.size();
								const auto batchSize = itemsLeft > maxBatchSize ? maxBatchSize : itemsLeft;

								ChunkSpanMut chunkSpan((Chunk**)&chunks[chunkOffset], batchSize);
								for (auto* pChunk: chunkSpan) {
									if GAIA_UNLIKELY (TIter::size(pChunk) == 0)
										continue;

									if constexpr (HasFilters) {
										if (!match_filters(*pChunk, queryInfo))
											continue;
									}

									chunkBatches.push_back({pArchetype, pChunk, indicesView.data(), 0, 0, 0});
								}

								if GAIA_UNLIKELY (chunkBatches.size() == chunkBatches.max_size()) {
									run_query_func<Func, TIter>(m_storage.world(), func, {chunkBatches.data(), chunkBatches.size()});
									chunkBatches.clear();
								}

								itemsLeft -= batchSize;
								chunkOffset += batchSize;
							}
						}
					}

					// Take care of any leftovers not processed during run_query
					if (!chunkBatches.empty())
						run_query_func<Func, TIter>(m_storage.world(), func, {chunkBatches.data(), chunkBatches.size()});

					unlock(*m_storage.world());
					// Commit the command buffer.
					// TODO: Smart handling necessary
					commit_cmd_buffer_st(*m_storage.world());
					commit_cmd_buffer_mt(*m_storage.world());
				}

				template <bool HasFilters, typename TIter, typename Func, QueryExecType ExecType>
				void run_query_batch_no_group_id_par(
						const QueryInfo& queryInfo, const uint32_t idxFrom, const uint32_t idxTo, Func func) {
					static_assert(ExecType != QueryExecType::Default);
					GAIA_PROF_SCOPE(query::run_query_batch_no_group_id_par);

					auto cacheView = queryInfo.cache_archetype_view();
					auto sortView = queryInfo.cache_sort_view();

					if (!sortView.empty()) {
						for (const auto& view: sortView) {
							const auto chunkEntitiesCnt = TIter::size(view.pChunk);
							if GAIA_UNLIKELY (chunkEntitiesCnt == 0)
								continue;

							const auto viewFrom = view.startRow;
							const auto viewTo = (uint16_t)(view.startRow + view.count);

							const auto minStartRow = TIter::start_index(view.pChunk);
							const auto minEndRow = TIter::end_index(view.pChunk);
							const auto startRow = core::get_max(minStartRow, viewFrom);
							const auto endRow = core::get_min(minEndRow, viewTo);
							const auto totalRows = endRow - startRow;
							if (totalRows == 0)
								continue;

							if constexpr (HasFilters) {
								if (!match_filters(*view.pChunk, queryInfo))
									continue;
							}

							const auto* pArchetype = cacheView[view.archetypeIdx];
							auto indicesView = queryInfo.indices_mapping_view(view.archetypeIdx);

							m_batches.push_back(ChunkBatch{pArchetype, view.pChunk, indicesView.data(), 0U, startRow, endRow});
						}
					} else {
						for (uint32_t i = idxFrom; i < idxTo; ++i) {
							const auto* pArchetype = cacheView[i];
							if GAIA_UNLIKELY (!can_process_archetype(*pArchetype))
								continue;

							auto indicesView = queryInfo.indices_mapping_view(i);
							const auto& chunks = pArchetype->chunks();
							for (auto* pChunk: chunks) {
								if GAIA_UNLIKELY (TIter::size(pChunk) == 0)
									continue;

								if constexpr (HasFilters) {
									if (!match_filters(*pChunk, queryInfo))
										continue;
								}

								m_batches.push_back({pArchetype, pChunk, indicesView.data(), 0, 0, 0});
							}
						}
					}

					if (m_batches.empty())
						return;

					lock(*m_storage.world());

					mt::JobParallel j;

					// Use efficiency cores for low-level priority jobs
					if constexpr (ExecType == QueryExecType::ParallelEff)
						j.priority = mt::JobPriority::Low;

					j.func = [&](const mt::JobArgs& args) {
						run_query_func<Func, TIter>(
								m_storage.world(), func, std::span(&m_batches[args.idxStart], args.idxEnd - args.idxStart));
					};

					auto& tp = mt::ThreadPool::get();
					auto jobHandle = tp.sched_par(j, m_batches.size(), 0);
					tp.wait(jobHandle);
					m_batches.clear();

					unlock(*m_storage.world());
					// Commit the command buffer.
					// TODO: Smart handling necessary
					commit_cmd_buffer_st(*m_storage.world());
					commit_cmd_buffer_mt(*m_storage.world());
				}

				template <bool HasFilters, typename TIter, typename Func>
				void run_query_batch_with_group_id(
						const QueryInfo& queryInfo, const uint32_t idxFrom, const uint32_t idxTo, Func func) {
					GAIA_PROF_SCOPE(query::run_query_batch_with_group_id);

					ChunkBatchArray chunkBatches;

					auto cacheView = queryInfo.cache_archetype_view();
					auto dataView = queryInfo.cache_data_view();

					lock(*m_storage.world());

					for (uint32_t i = idxFrom; i < idxTo; ++i) {
						const auto* pArchetype = cacheView[i];
						if GAIA_UNLIKELY (!can_process_archetype(*pArchetype))
							continue;

						auto indicesView = queryInfo.indices_mapping_view(i);
						const auto& chunks = pArchetype->chunks();
						const auto& data = dataView[i];

#if GAIA_ASSERT_ENABLED
						GAIA_ASSERT(
								// ... or no groupId is set...
								queryInfo.ctx().data.groupIdSet == 0 ||
								// ... or the groupId must match the requested one
								data.groupId == queryInfo.ctx().data.groupIdSet);
#endif

						uint32_t chunkOffset = 0;
						uint32_t itemsLeft = chunks.size();
						while (itemsLeft > 0) {
							const auto maxBatchSize = chunkBatches.max_size() - chunkBatches.size();
							const auto batchSize = itemsLeft > maxBatchSize ? maxBatchSize : itemsLeft;

							ChunkSpanMut chunkSpan((Chunk**)&chunks[chunkOffset], batchSize);
							for (auto* pChunk: chunkSpan) {
								if GAIA_UNLIKELY (TIter::size(pChunk) == 0)
									continue;

								if constexpr (HasFilters) {
									if (!match_filters(*pChunk, queryInfo))
										continue;
								}

								chunkBatches.push_back({pArchetype, pChunk, indicesView.data(), data.groupId, 0, 0});
							}

							if GAIA_UNLIKELY (chunkBatches.size() == chunkBatches.max_size()) {
								run_query_func<Func, TIter>(m_storage.world(), func, {chunkBatches.data(), chunkBatches.size()});
								chunkBatches.clear();
							}

							itemsLeft -= batchSize;
							chunkOffset += batchSize;
						}
					}

					// Take care of any leftovers not processed during run_query
					if (!chunkBatches.empty())
						run_query_func<Func, TIter>(m_storage.world(), func, {chunkBatches.data(), chunkBatches.size()});

					unlock(*m_storage.world());
					// Commit the command buffer.
					// TODO: Smart handling necessary
					commit_cmd_buffer_st(*m_storage.world());
					commit_cmd_buffer_mt(*m_storage.world());
				}

				template <bool HasFilters, typename TIter, typename Func, QueryExecType ExecType>
				void run_query_batch_with_group_id_par(
						const QueryInfo& queryInfo, const uint32_t idxFrom, const uint32_t idxTo, Func func) {
					static_assert(ExecType != QueryExecType::Default);
					GAIA_PROF_SCOPE(query::run_query_batch_with_group_id_par);

					ChunkBatchArray chunkBatch;

					auto cacheView = queryInfo.cache_archetype_view();
					auto dataView = queryInfo.cache_data_view();

#if GAIA_ASSERT_ENABLED
					for (uint32_t i = idxFrom; i < idxTo; ++i) {
						auto* pArchetype = cacheView[i];
						if GAIA_UNLIKELY (!can_process_archetype(*pArchetype))
							continue;

						const auto& data = dataView[i];
						GAIA_ASSERT(
								// ... or no groupId is set...
								queryInfo.ctx().data.groupIdSet == 0 ||
								// ... or the groupId must match the requested one
								data.groupId == queryInfo.ctx().data.groupIdSet);
					}
#endif

					for (uint32_t i = idxFrom; i < idxTo; ++i) {
						const Archetype* pArchetype = cacheView[i];
						if GAIA_UNLIKELY (!can_process_archetype(*pArchetype))
							continue;

						auto indicesView = queryInfo.indices_mapping_view(i);
						const auto& data = dataView[i];
						const auto& chunks = pArchetype->chunks();
						for (auto* pChunk: chunks) {
							if GAIA_UNLIKELY (TIter::size(pChunk) == 0)
								continue;

							if constexpr (HasFilters) {
								if (!match_filters(*pChunk, queryInfo))
									continue;
							}

							m_batches.push_back({pArchetype, pChunk, indicesView.data(), data.groupId, 0, 0});
						}
					}

					if (m_batches.empty())
						return;

					lock(*m_storage.world());

					mt::JobParallel j;

					// Use efficiency cores for low-level priority jobs
					if constexpr (ExecType == QueryExecType::ParallelEff)
						j.priority = mt::JobPriority::Low;

					j.func = [&](const mt::JobArgs& args) {
						run_query_func<Func, TIter>(
								m_storage.world(), func, std::span(&m_batches[args.idxStart], args.idxEnd - args.idxStart));
					};

					auto& tp = mt::ThreadPool::get();
					auto jobHandle = tp.sched_par(j, m_batches.size(), 0);
					tp.wait(jobHandle);
					m_batches.clear();

					unlock(*m_storage.world());
					// Commit the command buffer.
					// TODO: Smart handling necessary
					commit_cmd_buffer_st(*m_storage.world());
					commit_cmd_buffer_mt(*m_storage.world());
				}

				//------------------------------------------------

				template <bool HasFilters, QueryExecType ExecType, typename TIter, typename Func>
				void run_query(const QueryInfo& queryInfo, Func func) {
					GAIA_PROF_SCOPE(query::run_query);

					// TODO: Have archetype cache as double-linked list with pointers only.
					//       Have chunk cache as double-linked list with pointers only.
					//       Make it so only valid pointers are linked together.
					//       This means one less indirection + we won't need to call can_process_archetype()
					//       or pChunk.size()==0 in run_query_batch functions.
					auto cache_view = queryInfo.cache_archetype_view();
					if (cache_view.empty())
						return;

					const bool isGroupBy = queryInfo.ctx().data.groupBy != EntityBad;
					const bool isGroupSet = queryInfo.ctx().data.groupIdSet != 0;
					if (!isGroupBy || !isGroupSet) {
						// No group requested or group filtering is currently turned off
						const auto idxFrom = 0;
						const auto idxTo = (uint32_t)cache_view.size();
						if constexpr (ExecType != QueryExecType::Default)
							run_query_batch_no_group_id_par<HasFilters, TIter, Func, ExecType>(queryInfo, idxFrom, idxTo, func);
						else
							run_query_batch_no_group_id<HasFilters, TIter, Func>(queryInfo, idxFrom, idxTo, func);
					} else {
						// We wish to iterate only a certain group
						const auto* pGroupData = queryInfo.selected_group_data();
						if (pGroupData == nullptr)
							return;

						const auto idxFrom = pGroupData->idxFirst;
						const auto idxTo = pGroupData->idxLast + 1;
						if constexpr (ExecType != QueryExecType::Default)
							run_query_batch_with_group_id_par<HasFilters, TIter, Func, ExecType>(queryInfo, idxFrom, idxTo, func);
						else
							run_query_batch_with_group_id<HasFilters, TIter, Func>(queryInfo, idxFrom, idxTo, func);
					}
				}

				//------------------------------------------------

				template <QueryExecType ExecType, typename TIter, typename Func>
				void run_query_on_archetypes(QueryInfo& queryInfo, Func func) {
					// Update the world version
					// We do read-only access. No need to update the version
					//::gaia::ecs::update_version(*m_worldVersion);
					lock(*m_storage.world());

					{
						GAIA_PROF_SCOPE(query::run_query_a);

						// TODO: Have archetype cache as double-linked list with pointers only.
						//       Have chunk cache as double-linked list with pointers only.
						//       Make it so only valid pointers are linked together.
						//       This means one less indirection + we won't need to call can_process_archetype().
						auto cache_view = queryInfo.cache_archetype_view();
						GAIA_EACH(cache_view) {
							const auto* pArchetype = cache_view[i];
							if GAIA_UNLIKELY (!can_process_archetype(*pArchetype))
								continue;

							auto indicesView = queryInfo.indices_mapping_view(i);
							ChunkBatch batch{pArchetype, nullptr, indicesView.data(), 0, 0, 0};
							run_query_arch_func<Func, Iter>(m_storage.world(), func, batch);
						}
					}

					unlock(*m_storage.world());
					// Update the query version with the current world's version
					// queryInfo.set_world_version(*m_worldVersion);
				}

				//------------------------------------------------

				template <QueryExecType ExecType, typename TIter, typename Func>
				void run_query_on_chunks(QueryInfo& queryInfo, Func func) {
					// Update the world version
					::gaia::ecs::update_version(*m_worldVersion);

					const bool hasFilters = queryInfo.has_filters();
					if (hasFilters)
						run_query<true, ExecType, TIter>(queryInfo, func);
					else
						run_query<false, ExecType, TIter>(queryInfo, func);

					// Update the query version with the current world's version
					queryInfo.set_world_version(*m_worldVersion);
				}

				template <typename TIter, typename Func, typename... T>
				GAIA_FORCEINLINE void
				run_query_on_chunk(TIter& it, Func func, [[maybe_unused]] core::func_type_list<T...> types) {
					auto& queryInfo = fetch();
					run_query_on_chunk(queryInfo, it, func, types);
				}

				template <typename TIter, typename Func, typename... T>
				GAIA_FORCEINLINE void run_query_on_chunk(
						const QueryInfo& queryInfo, TIter& it, Func func, [[maybe_unused]] core::func_type_list<T...> types) {
					const auto cnt = it.size();
					const bool hasEntityFilters = queryInfo.has_entity_filter_terms();

					if constexpr (sizeof...(T) > 0) {
						// Pointers to the respective component types in the chunk, e.g
						// 		q.each([&](Position& p, const Velocity& v) {...}
						// Translates to:
						//  	auto p = it.view_mut_inter<Position, true>();
						//		auto v = it.view_inter<Velocity>();
						auto dataPointerTuple = std::make_tuple(it.template view_auto<T>()...);

						// Iterate over each entity in the chunk.
						// Translates to:
						//		GAIA_FOR(0, cnt) func(p[i], v[i]);

						if (!hasEntityFilters) {
							GAIA_FOR(cnt) {
								func(std::get<decltype(it.template view_auto<T>())>(dataPointerTuple)[it.template acc_index<T>(i)]...);
							}
						} else {
							const auto entities = it.template view<Entity>();
							GAIA_FOR(cnt) {
								if (!match_entity_filters(*queryInfo.world(), entities[i], queryInfo))
									continue;
								func(std::get<decltype(it.template view_auto<T>())>(dataPointerTuple)[it.template acc_index<T>(i)]...);
							}
						}
					} else {
						// No functor parameters. Do an empty loop.
						if (!hasEntityFilters) {
							GAIA_FOR(cnt) {
								func();
							}
						} else {
							const auto entities = it.template view<Entity>();
							GAIA_FOR(cnt) {
								if (!match_entity_filters(*queryInfo.world(), entities[i], queryInfo))
									continue;
								func();
							}
						}
					}
				}

				//------------------------------------------------

				template <QueryExecType ExecType, typename Func>
				void each_inter(QueryInfo& queryInfo, Func func) {
					using InputArgs = decltype(core::func_args(&Func::operator()));

#if GAIA_ASSERT_ENABLED
					// Make sure we only use components specified in the query.
					// Constness is respected. Therefore, if a type is const when registered to query,
					// it has to be const (or immutable) also in each().
					// in query.
					// Example 1:
					//   auto q = w.query().all<MyType>(); // immutable access requested
					//   q.each([](MyType val)) {}); // okay
					//   q.each([](const MyType& val)) {}); // okay
					//   q.each([](MyType& val)) {}); // error
					// Example 2:
					//   auto q = w.query().all<MyType&>(); // mutable access requested
					//   q.each([](MyType val)) {}); // error
					//   q.each([](const MyType& val)) {}); // error
					//   q.each([](MyType& val)) {}); // okay
					GAIA_ASSERT(unpack_args_into_query_has_all(queryInfo, InputArgs{}));
#endif

					run_query_on_chunks<ExecType, Iter>(queryInfo, [&](Iter& it) {
						GAIA_PROF_SCOPE(query_func);
						run_query_on_chunk(queryInfo, it, func, InputArgs{});
					});
				}

				template <
						QueryExecType ExecType, typename Func, bool FuncEnabled = UseCaching,
						typename std::enable_if<FuncEnabled>::type* = nullptr>
				void each_inter(QueryId queryId, Func func) {
					// Make sure the query was created by World.query()
					GAIA_ASSERT(m_storage.m_queryCache != nullptr);
					GAIA_ASSERT(queryId != QueryIdBad);

					auto& queryInfo = m_storage.m_queryCache->get(queryId);
					each_inter(queryInfo, func);
				}

				template <QueryExecType ExecType, typename Func>
				void each_inter(Func func) {
					auto& queryInfo = fetch();
					match_all(queryInfo);

					if constexpr (std::is_invocable_v<Func, IterAll&>) {
						run_query_on_chunks<ExecType, IterAll>(queryInfo, [&](IterAll& it) {
							GAIA_PROF_SCOPE(query_func);
							func(it);
						});
					} else if constexpr (std::is_invocable_v<Func, Iter&>) {
						run_query_on_chunks<ExecType, Iter>(queryInfo, [&](Iter& it) {
							GAIA_PROF_SCOPE(query_func);
							func(it);
						});
					} else if constexpr (std::is_invocable_v<Func, IterDisabled&>) {
						run_query_on_chunks<ExecType, IterDisabled>(queryInfo, [&](IterDisabled& it) {
							GAIA_PROF_SCOPE(query_func);
							func(it);
						});
					} else
						each_inter<ExecType>(queryInfo, func);
				}

				//------------------------------------------------

				//! Returns true when a direct term is backed by non-fragmenting storage and must be evaluated per entity.
				GAIA_NODISCARD static bool is_adjunct_direct_term(const World& world, const QueryTerm& term) {
					if (term.src != EntityBad || term.entTrav != EntityBad || term_has_variables(term))
						return false;

					const auto id = term.id;
					return (id.pair() && world_is_exclusive_dont_fragment_relation(world, entity_from_id(world, id.id()))) ||
								 (!id.pair() && world_is_sparse_dont_fragment_component(world, id));
				}

				//! Detects queries that can skip archetype seeding and start directly from entity-backed term indices.
				GAIA_NODISCARD static bool can_use_direct_entity_seed_eval(const QueryInfo& queryInfo) {
					if (!queryInfo.has_entity_filter_terms())
						return false;

					const auto& ctxData = queryInfo.ctx().data;
					if (ctxData.sortByFunc != nullptr || ctxData.groupBy != EntityBad)
						return false;

					bool hasPositiveTerm = false;
					for (const auto& term: ctxData.terms_view()) {
						if (term.src != EntityBad || term.entTrav != EntityBad || term_has_variables(term))
							return false;

						if (term.op == QueryOpKind::Any || term.op == QueryOpKind::Count)
							return false;

						if (term.op == QueryOpKind::All || term.op == QueryOpKind::Or)
							hasPositiveTerm = true;
					}

					return hasPositiveTerm;
				}

				//! Detects the special direct OR/NOT shape that can be answered from a union of direct term entity sets.
				GAIA_NODISCARD static bool has_only_direct_or_terms(const QueryInfo& queryInfo) {
					bool hasOr = false;
					for (const auto& term: queryInfo.ctx().data.terms_view()) {
						if (term.src != EntityBad || term.entTrav != EntityBad || term_has_variables(term))
							return false;
						if (term.op == QueryOpKind::Or) {
							hasOr = true;
							continue;
						}
						if (term.op != QueryOpKind::Not)
							return false;
					}

					return hasOr;
				}

				struct DirectEntitySeedInfo {
					Entity seededAllTerm = EntityBad;
					bool seededFromAll = false;
					bool seededFromOr = false;
				};

				//! Evaluates the remaining direct terms for a single seeded entity after the seed term itself was consumed.
				GAIA_NODISCARD static bool match_direct_entity_terms(
						const World& world, Entity entity, const QueryInfo& queryInfo, const DirectEntitySeedInfo& seedInfo) {
					bool hasOrTerms = false;
					bool anyOrMatched = false;

					for (const auto& term: queryInfo.ctx().data.terms_view()) {
						if (term.src != EntityBad || term.entTrav != EntityBad || term_has_variables(term))
							continue;
						if (seedInfo.seededFromAll && term.op == QueryOpKind::All && term.id == seedInfo.seededAllTerm)
							continue;
						if (seedInfo.seededFromOr && term.op == QueryOpKind::Or)
							continue;

						const bool present = world_has_entity_term(world, entity, term.id);
						switch (term.op) {
							case QueryOpKind::All:
								if (!present)
									return false;
								break;
							case QueryOpKind::Or:
								hasOrTerms = true;
								anyOrMatched |= present;
								break;
							case QueryOpKind::Not:
								if (present)
									return false;
								break;
							case QueryOpKind::Any:
							case QueryOpKind::Count:
								break;
						}
					}

					return !hasOrTerms || anyOrMatched;
				}

				//! Applies iterator-specific entity state constraints to the direct seeded path.
				template <typename TIter>
				GAIA_NODISCARD static bool match_direct_entity_constraints(const World& world, Entity entity) {
					if constexpr (std::is_same_v<TIter, Iter>)
						return world_entity_enabled(world, entity);
					else if constexpr (std::is_same_v<TIter, IterDisabled>)
						return !world_entity_enabled(world, entity);
					else
						return true;
				}

				//! Detects when a direct ALL seed can be counted by archetype buckets instead of per entity checks.
				GAIA_NODISCARD static bool can_use_archetype_bucket_count(
						const World& world, const QueryInfo& queryInfo, const DirectEntitySeedInfo& seedInfo) {
					if (!seedInfo.seededFromAll)
						return false;

					for (const auto& term: queryInfo.ctx().data.terms_view()) {
						if (term.src != EntityBad || term.entTrav != EntityBad || term_has_variables(term))
							return false;
						if (term.id == seedInfo.seededAllTerm && term.op == QueryOpKind::All)
							continue;
						if (term.op != QueryOpKind::All && term.op != QueryOpKind::Not)
							return false;
						if (is_adjunct_direct_term(world, term))
							return false;
					}

					return true;
				}

				//! Groups seeded entities by archetype and counts whole buckets when only structural ALL/NOT terms remain.
				template <typename TIter>
				GAIA_NODISCARD static uint32_t count_direct_entity_seed_by_archetype(
						const World& world, const QueryInfo& queryInfo, const cnt::darray<Entity>& seedEntities,
						const DirectEntitySeedInfo& seedInfo) {
					auto& scratch = direct_query_scratch();

					scratch.archetypes.clear();
					scratch.bucketEntities.clear();
					scratch.counts.clear();

					for (const auto entity: seedEntities) {
						if (!match_direct_entity_constraints<TIter>(world, entity))
							continue;

						const auto* pArchetype = world_entity_archetype(world, entity);
						const auto idx = core::get_index(scratch.archetypes, pArchetype);
						if (idx == BadIndex) {
							scratch.archetypes.push_back(pArchetype);
							scratch.bucketEntities.push_back(entity);
							scratch.counts.push_back(1);
						} else {
							++scratch.counts[idx];
						}
					}

					uint32_t cnt = 0;
					const auto archetypeCnt = (uint32_t)scratch.archetypes.size();
					GAIA_FOR(archetypeCnt) {
						const auto entity = scratch.bucketEntities[i];
						bool matched = true;
						for (const auto& term: queryInfo.ctx().data.terms_view()) {
							if (term.src != EntityBad || term.entTrav != EntityBad || term_has_variables(term))
								continue;
							if (term.id == seedInfo.seededAllTerm && term.op == QueryOpKind::All)
								continue;

							const bool present = world_has_entity_term(world, entity, term.id);
							if (term.op == QueryOpKind::All && !present) {
								matched = false;
								break;
							}
							if (term.op == QueryOpKind::Not && present) {
								matched = false;
								break;
							}
						}

						if (matched)
							cnt += scratch.counts[i];
					}

					return cnt;
				}

				//! Counts the union of direct OR term entity sets while deduplicating entities across terms.
				template <typename TIter>
				GAIA_NODISCARD static uint32_t count_direct_or_union(const World& world, const QueryInfo& queryInfo) {
					auto& scratch = direct_query_scratch();
					const auto seenVersion = next_direct_query_seen_version(scratch);

					uint32_t cnt = 0;
					for (const auto& term: queryInfo.ctx().data.terms_view()) {
						if (term.op != QueryOpKind::Or)
							continue;

						scratch.entities.clear();
						world_collect_direct_term_entities(world, term.id, scratch.entities);
						for (const auto entity: scratch.entities) {
							if (!match_direct_entity_constraints<TIter>(world, entity))
								continue;

							const auto entityId = (uint32_t)entity.id();
							ensure_direct_query_count_capacity(scratch, entityId);

							if (scratch.counts[entityId] == seenVersion)
								continue;
							scratch.counts[entityId] = seenVersion;

							bool rejected = false;
							for (const auto& notTerm: queryInfo.ctx().data.terms_view()) {
								if (notTerm.op != QueryOpKind::Not)
									continue;
								if (world_has_entity_term(world, entity, notTerm.id)) {
									rejected = true;
									break;
								}
							}

							if (!rejected)
								++cnt;
						}
					}

					return cnt;
				}

				//! Returns whether any entity survives the direct OR union after applying NOT terms and iterator constraints.
				template <typename TIter>
				GAIA_NODISCARD static bool is_empty_direct_or_union(const World& world, const QueryInfo& queryInfo) {
					auto& scratch = direct_query_scratch();
					const auto seenVersion = next_direct_query_seen_version(scratch);

					for (const auto& term: queryInfo.ctx().data.terms_view()) {
						if (term.op != QueryOpKind::Or)
							continue;

						scratch.entities.clear();
						world_collect_direct_term_entities(world, term.id, scratch.entities);
						for (const auto entity: scratch.entities) {
							if (!match_direct_entity_constraints<TIter>(world, entity))
								continue;

							const auto entityId = (uint32_t)entity.id();
							ensure_direct_query_count_capacity(scratch, entityId);

							if (scratch.counts[entityId] == seenVersion)
								continue;
							scratch.counts[entityId] = seenVersion;

							bool rejected = false;
							for (const auto& notTerm: queryInfo.ctx().data.terms_view()) {
								if (notTerm.op != QueryOpKind::Not)
									continue;
								if (world_has_entity_term(world, entity, notTerm.id)) {
									rejected = true;
									break;
								}
							}

							if (!rejected)
								return true;
						}
					}

					return false;
				}

				//! Builds the best direct entity seed set from the smallest positive ALL term or the OR union fallback.
				static DirectEntitySeedInfo
				build_direct_entity_seed(const World& world, const QueryInfo& queryInfo, cnt::darray<Entity>& out) {
					auto& scratch = direct_query_scratch();
					out.clear();
					DirectEntitySeedInfo seedInfo{};

					Entity bestAllTerm = EntityBad;
					uint32_t bestAllTermCount = UINT32_MAX;
					bool hasAllTerms = false;

					for (const auto& term: queryInfo.ctx().data.terms_view()) {
						if (term.src != EntityBad || term.entTrav != EntityBad || term_has_variables(term))
							continue;
						if (term.op != QueryOpKind::All)
							continue;

						hasAllTerms = true;
						const auto cnt = world_count_direct_term_entities(world, term.id);
						if (cnt < bestAllTermCount) {
							bestAllTermCount = cnt;
							bestAllTerm = term.id;
						}
					}

					if (hasAllTerms) {
						if (bestAllTerm != EntityBad) {
							world_collect_direct_term_entities(world, bestAllTerm, out);
							seedInfo.seededFromAll = true;
							seedInfo.seededAllTerm = bestAllTerm;
						}
						return seedInfo;
					}

					const auto seenVersion = next_direct_query_seen_version(scratch);

					for (const auto& term: queryInfo.ctx().data.terms_view()) {
						if (term.src != EntityBad || term.entTrav != EntityBad || term_has_variables(term))
							continue;
						if (term.op != QueryOpKind::Or)
							continue;

						scratch.entities.clear();
						world_collect_direct_term_entities(world, term.id, scratch.entities);
						for (const auto entity: scratch.entities) {
							const auto entityId = (uint32_t)entity.id();
							ensure_direct_query_count_capacity(scratch, entityId);

							if (scratch.counts[entityId] == seenVersion)
								continue;
							scratch.counts[entityId] = seenVersion;
							out.push_back(entity);
						}
					}

					seedInfo.seededFromOr = true;
					return seedInfo;
				}

				//! Fast empty() path for direct non-fragmenting queries that can seed from entity-backed indices.
				template <bool UseFilters, typename TIter>
				GAIA_NODISCARD bool empty_inter(const QueryInfo& queryInfo) const {
					if constexpr (!UseFilters) {
						if (can_use_direct_entity_seed_eval(queryInfo)) {
							auto& scratch = direct_query_scratch();
							if (has_only_direct_or_terms(queryInfo))
								return is_empty_direct_or_union<TIter>(*queryInfo.world(), queryInfo);

							const auto seedInfo = build_direct_entity_seed(*queryInfo.world(), queryInfo, scratch.entities);

							for (const auto entity: scratch.entities) {
								if (!match_direct_entity_constraints<TIter>(*queryInfo.world(), entity))
									continue;
								if (match_direct_entity_terms(*queryInfo.world(), entity, queryInfo, seedInfo))
									return false;
							}

							return true;
						}
					}

					const bool hasEntityFilters = queryInfo.has_entity_filter_terms();
					for (const auto* pArchetype: queryInfo) {
						if GAIA_UNLIKELY (!can_process_archetype(*pArchetype))
							continue;

						GAIA_PROF_SCOPE(query::empty);

						const auto& chunks = pArchetype->chunks();
						TIter it;
						it.set_world(queryInfo.world());
						it.set_archetype(pArchetype);

						const bool isNotEmpty = core::has_if(chunks, [&](Chunk* pChunk) {
							it.set_chunk(pChunk);
							if constexpr (UseFilters)
								if (it.size() == 0 || !match_filters(*pChunk, queryInfo))
									return false;
							if (!hasEntityFilters)
								return it.size() > 0;

							const auto entities = it.template view<Entity>();
							const auto cnt = it.size();
							GAIA_FOR(cnt) {
								if (match_entity_filters(*queryInfo.world(), entities[i], queryInfo))
									return true;
							}
							return false;
						});

						if (isNotEmpty)
							return false;
					}

					return true;
				}

				//! Evaluates the entity-level adjunct terms that are not represented by archetype membership.
				GAIA_NODISCARD static bool match_entity_filters(const World& world, Entity entity, const QueryInfo& queryInfo) {
					bool hasOrTerms = false;
					bool anyOrMatched = false;
					const bool hasAdjunctTerms = queryInfo.has_entity_filter_terms();

					for (const auto& term: queryInfo.ctx().data.terms_view()) {
						if (term.src != EntityBad || term.entTrav != EntityBad || term_has_variables(term))
							continue;

						const auto id = term.id;
						const bool isAdjunctTerm =
								(id.pair() && world_is_exclusive_dont_fragment_relation(world, entity_from_id(world, id.id()))) ||
								(!id.pair() && world_is_sparse_dont_fragment_component(world, id));
						const bool needsEntityFilter = isAdjunctTerm || (hasAdjunctTerms && term.op == QueryOpKind::Or);
						if (!needsEntityFilter)
							continue;

						const bool present = world_has_entity_term(world, entity, id);
						switch (term.op) {
							case QueryOpKind::All:
								if (!present)
									return false;
								break;
							case QueryOpKind::Or:
								hasOrTerms = true;
								anyOrMatched |= present;
								break;
							case QueryOpKind::Not:
								if (present)
									return false;
								break;
							case QueryOpKind::Any:
							case QueryOpKind::Count:
								break;
						}
					}

					return !hasOrTerms || anyOrMatched;
				}

				//! Fast count() path for direct non-fragmenting queries that can seed from entity-backed indices.
				template <bool UseFilters, typename TIter>
				GAIA_NODISCARD uint32_t count_inter(const QueryInfo& queryInfo) const {
					if constexpr (!UseFilters) {
						if (can_use_direct_entity_seed_eval(queryInfo)) {
							auto& scratch = direct_query_scratch();
							if (has_only_direct_or_terms(queryInfo))
								return count_direct_or_union<TIter>(*queryInfo.world(), queryInfo);

							const auto seedInfo = build_direct_entity_seed(*queryInfo.world(), queryInfo, scratch.entities);

							if (can_use_archetype_bucket_count(*queryInfo.world(), queryInfo, seedInfo))
								return count_direct_entity_seed_by_archetype<TIter>(
										*queryInfo.world(), queryInfo, scratch.entities, seedInfo);

							uint32_t cnt = 0;
							for (const auto entity: scratch.entities) {
								if (!match_direct_entity_constraints<TIter>(*queryInfo.world(), entity))
									continue;
								if (match_direct_entity_terms(*queryInfo.world(), entity, queryInfo, seedInfo))
									++cnt;
							}

							return cnt;
						}
					}

					uint32_t cnt = 0;
					TIter it;
					it.set_world(queryInfo.world());
					const bool hasEntityFilters = queryInfo.has_entity_filter_terms();

					for (const auto* pArchetype: queryInfo) {
						if GAIA_UNLIKELY (!can_process_archetype(*pArchetype))
							continue;

						GAIA_PROF_SCOPE(query::count);

						it.set_archetype(pArchetype);

						// No mapping for count(). It doesn't need to access data cache.
						// auto indicesView = queryInfo.indices_mapping_view(aid);

						const auto& chunks = pArchetype->chunks();
						for (auto* pChunk: chunks) {
							it.set_chunk(pChunk);

							const auto entityCnt = it.size();
							if (entityCnt == 0)
								continue;

							// Filters
							if constexpr (UseFilters) {
								if (!match_filters(*pChunk, queryInfo))
									continue;
							}

							if (hasEntityFilters) {
								const auto entities = it.template view<Entity>();
								GAIA_FOR(entityCnt) {
									if (match_entity_filters(*queryInfo.world(), entities[i], queryInfo))
										++cnt;
								}
								continue;
							}

							// Entity count
							cnt += entityCnt;
						}
					}

					return cnt;
				}

				template <bool UseFilters, typename TIter, typename ContainerOut>
				void arr_inter(QueryInfo& queryInfo, ContainerOut& outArray) {
					using ContainerItemType = typename ContainerOut::value_type;
					TIter it;
					it.set_world(queryInfo.world());
					const bool hasEntityFilters = queryInfo.has_entity_filter_terms();

					for (auto* pArchetype: queryInfo) {
						if GAIA_UNLIKELY (!can_process_archetype(*pArchetype))
							continue;

						GAIA_PROF_SCOPE(query::arr);

						it.set_archetype(pArchetype);

						// No mapping for arr(). It doesn't need to access data cache.
						// auto indicesView = queryInfo.indices_mapping_view(aid);

						const auto& chunks = pArchetype->chunks();
						for (auto* pChunk: chunks) {
							it.set_chunk(pChunk);

							const auto cnt = it.size();
							if (cnt == 0)
								continue;

							// Filters
							if constexpr (UseFilters) {
								if (!match_filters(*pChunk, queryInfo))
									continue;
							}

							const auto dataView = it.template view<ContainerItemType>();
							if (!hasEntityFilters) {
								GAIA_FOR(cnt) {
									const auto idx = it.template acc_index<ContainerItemType>(i);
									auto tmp = dataView[idx];
									outArray.push_back(tmp);
								}
							} else {
								const auto entities = it.template view<Entity>();
								GAIA_FOR(cnt) {
									if (!match_entity_filters(*queryInfo.world(), entities[i], queryInfo))
										continue;
									const auto idx = it.template acc_index<ContainerItemType>(i);
									auto tmp = dataView[idx];
									outArray.push_back(tmp);
								}
							}
						}
					}
				}

			public:
				QueryImpl() = default;
				~QueryImpl() = default;

				template <bool FuncEnabled = UseCaching>
				QueryImpl(
						World& world, QueryCache& queryCache, ArchetypeId& nextArchetypeId, uint32_t& worldVersion,
						const ArchetypeMapById& archetypes, const EntityToArchetypeMap& entityToArchetypeMap,
						const ArchetypeDArray& allArchetypes):
						m_nextArchetypeId(&nextArchetypeId), m_worldVersion(&worldVersion), m_archetypes(&archetypes),
						m_entityToArchetypeMap(&entityToArchetypeMap), m_allArchetypes(&allArchetypes) {
					m_storage.init(&world, &queryCache);
				}

				template <bool FuncEnabled = !UseCaching>
				QueryImpl(
						World& world, ArchetypeId& nextArchetypeId, uint32_t& worldVersion, const ArchetypeMapById& archetypes,
						const EntityToArchetypeMap& entityToArchetypeMap, const ArchetypeDArray& allArchetypes):
						m_nextArchetypeId(&nextArchetypeId), m_worldVersion(&worldVersion), m_archetypes(&archetypes),
						m_entityToArchetypeMap(&entityToArchetypeMap), m_allArchetypes(&allArchetypes) {
					m_storage.init(&world);
				}

				GAIA_NODISCARD QueryId id() const {
					static_assert(UseCaching, "id() can be used only with cached queries");
					return m_storage.m_q.handle.id();
				}

				GAIA_NODISCARD uint32_t gen() const {
					static_assert(UseCaching, "gen() can be used only with cached queries");
					return m_storage.m_q.handle.gen();
				}

				//------------------------------------------------

				//! Release any data allocated by the query
				void reset() {
					m_storage.reset();
					m_eachBfsData.reset();
					invalidate_each_bfs_cache();
				}

				void destroy() {
					(void)m_storage.try_del_from_cache();
					m_eachBfsData.reset();
					invalidate_each_bfs_cache();
				}

				//! Returns true if the query is stored in the query cache
				GAIA_NODISCARD bool is_cached() const {
					return m_storage.is_cached();
				}

				//------------------------------------------------

				//! Creates a query from a null-terminated expression string.
				//!
				//! Expression is a string between separators.
				//! Spaces are trimmed automatically.
				//!
				//! Supported modifiers:
				//!   "," - separates expressions
				//!   "||" - query::or_(OR chain; at least two OR terms)
				//!   "?" - query::any (optional)
				//!   "!" - query::none
				//!   "&" - read-write access
				//!   "%e" - entity value
				//!   "(rel,tgt)" - relationship pair, a wildcard character in either rel or tgt is translated into All
				//!   "$name" - query variable (`$this` is reserved)
				//!   "Id(src)" - source lookup (src can be a variable, e.g. Planet($planet), or $this for default source)
				//!
				//! Example:
				//!   struct Position {...};
				//!   struct Velocity {...};
				//!   struct RigidBody {...};
				//!   struct Fuel {...};
				//!   auto player = w.add();
				//!   w.query().add("&Position, !Velocity, ?RigidBody, (Fuel,*), %e", player.value());
				//! Translates into:
				//!   w.query()
				//!      .all<Position&>()
				//!      .no<Velocity>()
				//!      .any<RigidBody>()
				//!      .all(Pair(w.add<Fuel>().entity, All)>()
				//!      .all(Player);
				//!
				//! \param str Null-terminated string with the query expression
				QueryImpl& add(const char* str, ...) {
					GAIA_ASSERT(str != nullptr);
					if (str == nullptr)
						return *this;

					va_list args{};
					va_start(args, str);

					uint32_t pos = 0;
					uint32_t exp0 = 0;
					uint32_t parentDepth = 0;

					cnt::sarray<util::str_view, 8> varNames{};
					uint32_t varNamesCnt = 0;
					auto is_this_expr = [](std::span<const char> exprRaw) {
						auto expr = util::trim(exprRaw);
						return expr.size() == 5 && expr[0] == '$' && expr[1] == 't' && expr[2] == 'h' && expr[3] == 'i' &&
									 expr[4] == 's';
					};

					auto find_or_alloc_var = [&](std::span<const char> varExpr) -> Entity {
						auto varNameSpan = util::trim(varExpr);
						if (varNameSpan.empty())
							return EntityBad;

						const util::str_view varName{varNameSpan.data(), (uint32_t)varNameSpan.size()};
						if (is_reserved_var_name(varName)) {
							GAIA_ASSERT2(false, "$this is reserved and can only be used as a source expression: Id($this)");
							return EntityBad;
						}

						const auto namedVar = find_var_by_name(varName);
						if (namedVar != EntityBad)
							return namedVar;

						GAIA_FOR(varNamesCnt) {
							if (varNames[i].size() != varName.size())
								continue;
							if (varNames[i].size() > 0 && memcmp(varNames[i].data(), varName.data(), varName.size()) != 0)
								continue;
							return query_var_entity(i);
						}

						if (varNamesCnt >= varNames.size()) {
							GAIA_ASSERT2(false, "Too many query variables in expression");
							return EntityBad;
						}

						const auto idx = varNamesCnt++;
						varNames[idx] = varName;

						const auto varEntity = query_var_entity(idx);
						(void)set_var_name_internal(varEntity, varName);
						return varEntity;
					};

					auto parse_entity_expr = [&](auto&& self, std::span<const char> exprRaw) -> Entity {
						auto expr = util::trim(exprRaw);
						if (expr.empty())
							return EntityBad;

						if (expr[0] == '$')
							return find_or_alloc_var(expr.subspan(1));

						if (expr[0] == '(') {
							if (expr.back() != ')') {
								GAIA_ASSERT2(false, "Expression '(' not terminated");
								return EntityBad;
							}

							const auto idStr = expr.subspan(1, expr.size() - 2);
							const auto commaIdx = core::get_index(idStr, ',');
							if (commaIdx == BadIndex) {
								GAIA_ASSERT2(false, "Pair expression does not contain ','");
								return EntityBad;
							}

							const auto first = self(self, idStr.subspan(0, commaIdx));
							if (first == EntityBad)
								return EntityBad;
							const auto second = self(self, idStr.subspan(commaIdx + 1));
							if (second == EntityBad)
								return EntityBad;

							return ecs::Pair(first, second);
						}

						return expr_to_entity((const World&)*m_storage.world(), args, expr);
					};

					auto parse_src_expr = [&](std::span<const char> srcExprRaw, Entity& srcOut) -> bool {
						auto srcExpr = util::trim(srcExprRaw);
						if (srcExpr.empty())
							return false;

						// `$this` explicitly means the default source for the term.
						if (is_this_expr(srcExpr)) {
							srcOut = EntityBad;
							return true;
						}

						srcOut = parse_entity_expr(parse_entity_expr, srcExpr);
						return srcOut != EntityBad;
					};

					auto parse_term_expr = [&](std::span<const char> exprRaw, Entity& id, QueryTermOptions& options) -> bool {
						auto expr = util::trim(exprRaw);
						if (expr.empty())
							return false;

						if (expr.back() == ')') {
							int32_t depth = 0;
							int32_t openIdx = -1;
							for (int32_t i = (int32_t)expr.size() - 1; i >= 0; --i) {
								if (expr[(uint32_t)i] == ')')
									++depth;
								else if (expr[(uint32_t)i] == '(') {
									--depth;
									if (depth == 0) {
										openIdx = i;
										break;
									}
								}
							}

							// `Id(src)` form. Keep `(Rel,Tgt)` intact by requiring a non-empty prefix.
							if (openIdx > 0) {
								auto idExpr = util::trim(expr.subspan(0, (uint32_t)openIdx));
								auto srcExpr = util::trim(expr.subspan((uint32_t)openIdx + 1, expr.size() - (uint32_t)openIdx - 2));
								if (!idExpr.empty() && !srcExpr.empty()) {
									id = parse_entity_expr(parse_entity_expr, idExpr);
									if (id == EntityBad)
										return false;

									Entity src = EntityBad;
									if (!parse_src_expr(srcExpr, src))
										return false;

									options.src(src);
									return true;
								}
							}
						}

						id = parse_entity_expr(parse_entity_expr, expr);
						return id != EntityBad;
					};

					auto add_term = [&](QueryOpKind op, std::span<const char> exprRaw) {
						auto expr = util::trim(exprRaw);
						if (expr.empty())
							return false;

						bool isReadWrite = false;
						if (!expr.empty() && expr[0] == '&') {
							isReadWrite = true;
							expr = util::trim(expr.subspan(1));
						}

						QueryTermOptions options{};
						if (isReadWrite)
							options.write();

						Entity entity = EntityBad;
						if (!parse_term_expr(expr, entity, options))
							return false;

						switch (op) {
							case QueryOpKind::All:
								all(entity, options);
								break;
							case QueryOpKind::Or:
								or_(entity, options);
								break;
							case QueryOpKind::Not:
								no(entity, options);
								break;
							case QueryOpKind::Any:
								any(entity, options);
								break;
							default:
								GAIA_ASSERT(false);
								return false;
						}

						return true;
					};

					auto process = [&]() {
						std::span<const char> exprRaw(&str[exp0], pos - exp0);
						exp0 = ++pos;

						auto expr = util::trim(exprRaw);
						if (expr.empty())
							return true;

						// OR-chain at top level maps to query::or_ terms.
						bool hasOrChain = false;
						{
							uint32_t depth = 0;
							const auto cnt = (uint32_t)expr.size();
							for (uint32_t i = 0; i + 1 < cnt; ++i) {
								const auto ch = expr[i];
								if (ch == '(')
									++depth;
								else if (ch == ')') {
									GAIA_ASSERT(depth > 0);
									--depth;
								} else if (depth == 0 && ch == '|' && expr[i + 1] == '|') {
									hasOrChain = true;
									break;
								}
							}
						}

						if (hasOrChain) {
							uint32_t depth = 0;
							uint32_t partBeg = 0;
							const auto cnt = (uint32_t)expr.size();
							for (uint32_t i = 0; i < cnt; ++i) {
								const auto ch = expr[i];
								if (ch == '(')
									++depth;
								else if (ch == ')') {
									GAIA_ASSERT(depth > 0);
									--depth;
								}

								const bool isOr = i + 1 < cnt && depth == 0 && ch == '|' && expr[i + 1] == '|';
								const bool isEnd = i + 1 == cnt;
								if (!isOr && !isEnd)
									continue;

								const auto partEnd = isOr ? i : (i + 1);
								auto partExpr = expr.subspan(partBeg, partEnd - partBeg);
								if (!add_term(QueryOpKind::Or, partExpr))
									return false;

								if (isOr) {
									partBeg = i + 2;
									++i;
								}
							}

							return true;
						}

						QueryOpKind op = QueryOpKind::All;
						if (expr[0] == '?') {
							op = QueryOpKind::Any;
							expr = util::trim(expr.subspan(1));
						} else if (expr[0] == '!') {
							op = QueryOpKind::Not;
							expr = util::trim(expr.subspan(1));
						}

						return add_term(op, expr);
					};

					for (; str[pos] != 0; ++pos) {
						if (str[pos] == '(')
							++parentDepth;
						else if (str[pos] == ')') {
							GAIA_ASSERT(parentDepth > 0);
							--parentDepth;
						} else if (str[pos] == ',' && parentDepth == 0) {
							if (!process())
								goto add_end;
						}
					}
					process();

				add_end:
					va_end(args);
					return *this;
				}

				QueryImpl& add(QueryInput item) {
					// Add commands to the command buffer
					add_inter(item);
					return *this;
				}

				//------------------------------------------------

				QueryImpl& all(Entity entity, const QueryTermOptions& options = QueryTermOptions{}) {
					add_entity_term(QueryOpKind::All, entity, options);
					return *this;
				}

				template <typename T>
				QueryImpl& all(const QueryTermOptions& options) {
					add_inter<T>(QueryOpKind::All, options);
					return *this;
				}

#if GAIA_USE_VARIADIC_API
				template <typename... T>
				QueryImpl& all() {
					// Add commands to the command buffer
					(add_inter<T>(QueryOpKind::All), ...);
					return *this;
				}
#else
				template <typename T>
				QueryImpl& all() {
					// Add commands to the command buffer
					add_inter<T>(QueryOpKind::All);
					return *this;
				}
#endif

				//------------------------------------------------

				QueryImpl& any(Entity entity, const QueryTermOptions& options = QueryTermOptions{}) {
					add_entity_term(QueryOpKind::Any, entity, options);
					return *this;
				}

				template <typename T>
				QueryImpl& any(const QueryTermOptions& options) {
					add_inter<T>(QueryOpKind::Any, options);
					return *this;
				}

#if GAIA_USE_VARIADIC_API
				template <typename... T>
				QueryImpl& any() {
					// Add commands to the command buffer
					(add_inter<T>(QueryOpKind::Any), ...);
					return *this;
				}
#else
				template <typename T>
				QueryImpl& any() {
					// Add commands to the command buffer
					add_inter<T>(QueryOpKind::Any);
					return *this;
				}
#endif

				//------------------------------------------------

				//! OR terms (at least one has to match).
				//! A single OR term is canonicalized to ALL during query normalization.
				QueryImpl& or_(Entity entity, const QueryTermOptions& options = QueryTermOptions{}) {
					add_entity_term(QueryOpKind::Or, entity, options);
					return *this;
				}

				template <typename T>
				QueryImpl& or_(const QueryTermOptions& options) {
					add_inter<T>(QueryOpKind::Or, options);
					return *this;
				}

#if GAIA_USE_VARIADIC_API
				template <typename... T>
				QueryImpl& or_() {
					(add_inter<T>(QueryOpKind::Or), ...);
					return *this;
				}
#else
				template <typename T>
				QueryImpl& or_() {
					add_inter<T>(QueryOpKind::Or);
					return *this;
				}
#endif

				//------------------------------------------------

				QueryImpl& no(Entity entity, const QueryTermOptions& options = QueryTermOptions{}) {
					add_entity_term(QueryOpKind::Not, entity, options);
					return *this;
				}

				template <typename T>
				QueryImpl& no(const QueryTermOptions& options) {
					add_inter<T>(QueryOpKind::Not, options);
					return *this;
				}

#if GAIA_USE_VARIADIC_API
				template <typename... T>
				QueryImpl& no() {
					// Add commands to the command buffer
					(add_inter<T>(QueryOpKind::Not), ...);
					return *this;
				}
#else
				template <typename T>
				QueryImpl& no() {
					// Add commands to the command buffer
					add_inter<T>(QueryOpKind::Not);
					return *this;
				}
#endif

				//! Assigns a human-readable name to a query variable entity (`Var0..Var7`).
				//! The name can be used later by set_var(name, value).
				//! \param varEntity Query variable entity (`Var0..Var7`)
				//! \param name Variable name (without '$')
				//! \note Empty names and reserved name "this" are rejected.
				QueryImpl& var_name(Entity varEntity, util::str_view name) {
					[[maybe_unused]] const bool ok = set_var_name_internal(varEntity, name);
					GAIA_ASSERT(ok);
					return *this;
				}
				//! Assigns a human-readable name to a query variable entity (`Var0..Var7`).
				QueryImpl& var_name(Entity varEntity, const char* name) {
					GAIA_ASSERT(name != nullptr);
					if (name == nullptr)
						return *this;
					return var_name(varEntity, util::str_view{name, (uint32_t)GAIA_STRLEN(name, 256)});
				}

				//! Binds a query variable (`Var0..Var7`) to a concrete entity value.
				//! Bound values are applied at runtime before query evaluation.
				//! \param varEntity Query variable entity (`Var0..Var7`)
				//! \param value Entity value to bind
				QueryImpl& set_var(Entity varEntity, Entity value) {
					const bool ok = is_query_var_entity(varEntity);
					GAIA_ASSERT(ok);
					if (!ok)
						return *this;

					const auto idx = query_var_idx(varEntity);
					m_varBindings[idx] = value;
					m_varBindingsMask |= (uint8_t(1) << idx);
					return *this;
				}
				//! Binds a named query variable to a concrete entity value.
				//! \param name Variable name previously assigned by var_name(...)
				//! \param value Entity value to bind
				QueryImpl& set_var(util::str_view name, Entity value) {
					const auto varEntity = find_var_by_name(name);
					GAIA_ASSERT(varEntity != EntityBad);
					if (varEntity == EntityBad)
						return *this;
					return set_var(varEntity, value);
				}
				//! Binds a named query variable to a concrete entity value.
				QueryImpl& set_var(const char* name, Entity value) {
					GAIA_ASSERT(name != nullptr);
					if (name == nullptr)
						return *this;
					return set_var(util::str_view{name, (uint32_t)GAIA_STRLEN(name, 256)}, value);
				}

				//! Clears binding for a single query variable (`Var0..Var7`).
				//! The variable becomes unbound for the next query evaluation.
				QueryImpl& clear_var(Entity varEntity) {
					const bool ok = is_query_var_entity(varEntity);
					GAIA_ASSERT(ok);
					if (!ok)
						return *this;

					const auto idx = query_var_idx(varEntity);
					m_varBindingsMask &= (uint8_t)~(uint8_t(1) << idx);
					return *this;
				}
				//! Clears all runtime variable bindings.
				QueryImpl& clear_vars() {
					m_varBindingsMask = 0;
					return *this;
				}

				//------------------------------------------------

				QueryImpl& changed(Entity entity) {
					changed_inter(entity);
					return *this;
				}

#if GAIA_USE_VARIADIC_API
				template <typename... T>
				QueryImpl& changed() {
					// Add commands to the command buffer
					(changed_inter<T>(), ...);
					return *this;
				}
#else
				template <typename T>
				QueryImpl& changed() {
					// Add commands to the command buffer
					changed_inter<T>();
					return *this;
				}
#endif

				//------------------------------------------------

				//! Sorts the query by the specified entity and function.
				//! \param entity The entity to sort by. Use ecs::EntityBad to sort by chunk entities,
				//                or anything else to sort by components.
				//! \param func The function to use for sorting. Return -1 to put the first entity before the second,
				//!             0 to keep the order, and 1 to put the first entity after the second.
				QueryImpl& sort_by(Entity entity, TSortByFunc func) {
					sort_by_inter(entity, func);
					return *this;
				}

				//! Sorts the query by the specified component and function.
				//! \tparam T The component to sort by. It is registered if it hasn't been registered yet.
				//! \param func The function to use for sorting. Return -1 to put the first entity before the second,
				//!             0 to keep the order, and 1 to put the first entity after the second.
				template <typename T>
				QueryImpl& sort_by(TSortByFunc func) {
					sort_by_inter<T>(func);
					return *this;
				}

				//! Sorts the query by the specified pair and function.
				//! \tparam Rel The relation to sort by. It is registered if it hasn't been registered yet.
				//! \tparam Tgt The target to sort by. It is registered if it hasn't been registered yet.
				//! \param func The function to use for sorting. Return -1 to put the first entity before the second,
				//!             0 to keep the order, and 1 to put the first entity after the second.
				template <typename Rel, typename Tgt>
				QueryImpl& sort_by(TSortByFunc func) {
					sort_by_inter<Rel, Tgt>(func);
					return *this;
				}

				//------------------------------------------------

				class OrderByBfsView final {
					QueryImpl* m_query = nullptr;
					Entity m_relation = EntityBad;

				public:
					OrderByBfsView(QueryImpl& query, Entity relation): m_query(&query), m_relation(relation) {}

					template <typename Func>
					void each(Func func) {
						m_query->each_bfs(func, m_relation);
					}
				};

				//------------------------------------------------

				//! Orders query iteration in dependency BFS levels for the given relation.
				//! Pair(relation, X) on entity E means E depends on X.
				GAIA_NODISCARD OrderByBfsView bfs(Entity relation) {
					return OrderByBfsView(*this, relation);
				}

				//------------------------------------------------

				//! Organizes matching archetypes into groups according to the grouping function and entity.
				//! \param entity The entity to group by.
				//! \param func The function to use for grouping. Returns a GroupId to group the entities by.
				QueryImpl& group_by(Entity entity, TGroupByFunc func = group_by_func_default) {
					group_by_inter(entity, func);
					return *this;
				}

				//! Organizes matching archetypes into groups according to the grouping function.
				//! \tparam T Component to group by. It is registered if it hasn't been registered yet.
				//! \param func The function to use for grouping. Returns a GroupId to group the entities by.
				template <typename T>
				QueryImpl& group_by(TGroupByFunc func = group_by_func_default) {
					group_by_inter<T>(func);
					return *this;
				}

				//! Organizes matching archetypes into groups according to the grouping function.
				//! \tparam Rel The relation to group by. It is registered if it hasn't been registered yet.
				//! \tparam Tgt The target to group by. It is registered if it hasn't been registered yet.
				//! \param func The function to use for grouping. Returns a GroupId to group the entities by.
				template <typename Rel, typename Tgt>
				QueryImpl& group_by(TGroupByFunc func = group_by_func_default) {
					group_by_inter<Rel, Tgt>(func);
					return *this;
				}

				//------------------------------------------------

				//! Selects the group to iterate over.
				//! \param groupId The group to iterate over.
				QueryImpl& group_id(GroupId groupId) {
					set_group_id_inter(groupId);
					return *this;
				}

				//! Selects the group to iterate over.
				//! \param entity The entity to treat as a group to iterate over.
				QueryImpl& group_id(Entity entity) {
					GAIA_ASSERT(!entity.pair());
					set_group_id_inter(entity.id());
					return *this;
				}

				//! Selects the group to iterate over.
				//! \tparam T Component to treat as a group to iterate over. It is registered if it hasn't been registered yet.
				template <typename T>
				QueryImpl& group_id() {
					set_group_id_inter<T>();
					return *this;
				}

				//------------------------------------------------

				template <typename Func>
				void each(Func func) {
					each_inter<QueryExecType::Default, Func>(func);
				}

				template <typename Func>
				void each(Func func, QueryExecType execType) {
					switch (execType) {
						case QueryExecType::Parallel:
							each_inter<QueryExecType::Parallel, Func>(func);
							break;
						case QueryExecType::ParallelPerf:
							each_inter<QueryExecType::ParallelPerf, Func>(func);
							break;
						case QueryExecType::ParallelEff:
							each_inter<QueryExecType::ParallelEff, Func>(func);
							break;
						default:
							each_inter<QueryExecType::Default, Func>(func);
							break;
					}
				}

				//------------------------------------------------

				template <typename Func>
				void each_arch(Func func) {
					auto& queryInfo = fetch();
					match_all(queryInfo);

					if constexpr (std::is_invocable_v<Func, IterAll&>) {
						run_query_on_archetypes<QueryExecType::Default, IterAll>(queryInfo, [&](IterAll& it) {
							GAIA_PROF_SCOPE(query_func_a);
							func(it);
						});
					} else if constexpr (std::is_invocable_v<Func, Iter&>) {
						run_query_on_archetypes<QueryExecType::Default, Iter>(queryInfo, [&](Iter& it) {
							GAIA_PROF_SCOPE(query_func_a);
							func(it);
						});
					} else if constexpr (std::is_invocable_v<Func, IterDisabled&>) {
						run_query_on_archetypes<QueryExecType::Default, IterDisabled>(queryInfo, [&](IterDisabled& it) {
							GAIA_PROF_SCOPE(query_func_a);
							func(it);
						});
					}
				}

				//------------------------------------------------

				//!	Returns true or false depending on whether there are any entities matching the query.
				//!	\warning Only use if you only care if there are any entities matching the query.
				//!					 The result is not cached and repeated calls to the function might be slow.
				//!					 If you already called arr(), checking if it is empty is preferred.
				//!					 Use empty() instead of calling count()==0.
				//!	\return True if there are any entities matching the query. False otherwise.
				bool empty(Constraints constraints = Constraints::EnabledOnly) {
					auto& queryInfo = fetch();
					if (!queryInfo.has_filters() && can_use_direct_entity_seed_eval(queryInfo)) {
						switch (constraints) {
							case Constraints::EnabledOnly:
								return empty_inter<false, Iter>(queryInfo);
							case Constraints::DisabledOnly:
								return empty_inter<false, IterDisabled>(queryInfo);
							case Constraints::AcceptAll:
								return empty_inter<false, IterAll>(queryInfo);
						}
					}

					match_all(queryInfo);

					const bool hasFilters = queryInfo.has_filters();
					if (hasFilters) {
						switch (constraints) {
							case Constraints::EnabledOnly:
								return empty_inter<true, Iter>(queryInfo);
							case Constraints::DisabledOnly:
								return empty_inter<true, IterDisabled>(queryInfo);
							case Constraints::AcceptAll:
								return empty_inter<true, IterAll>(queryInfo);
						}
					} else {
						switch (constraints) {
							case Constraints::EnabledOnly:
								return empty_inter<false, Iter>(queryInfo);
							case Constraints::DisabledOnly:
								return empty_inter<false, IterDisabled>(queryInfo);
							case Constraints::AcceptAll:
								return empty_inter<false, IterAll>(queryInfo);
						}
					}

					return true;
				}

				//! Calculates the number of entities matching the query
				//! \warning Only use if you only care about the number of entities matching the query.
				//!          The result is not cached and repeated calls to the function might be slow.If you already called
				//!          arr(), use the size provided by the array.Use empty() instead of calling count() == 0.
				//! \return The number of matching entities
				uint32_t count(Constraints constraints = Constraints::EnabledOnly) {
					auto& queryInfo = fetch();
					if (!queryInfo.has_filters() && can_use_direct_entity_seed_eval(queryInfo)) {
						switch (constraints) {
							case Constraints::EnabledOnly:
								return count_inter<false, Iter>(queryInfo);
							case Constraints::DisabledOnly:
								return count_inter<false, IterDisabled>(queryInfo);
							case Constraints::AcceptAll:
								return count_inter<false, IterAll>(queryInfo);
						}
					}

					match_all(queryInfo);

					uint32_t entCnt = 0;

					const bool hasFilters = queryInfo.has_filters();
					if (hasFilters) {
						switch (constraints) {
							case Constraints::EnabledOnly: {
								entCnt += count_inter<true, Iter>(queryInfo);
							} break;
							case Constraints::DisabledOnly: {
								entCnt += count_inter<true, IterDisabled>(queryInfo);
							} break;
							case Constraints::AcceptAll: {
								entCnt += count_inter<true, IterAll>(queryInfo);
							} break;
						}
					} else {
						switch (constraints) {
							case Constraints::EnabledOnly: {
								entCnt += count_inter<false, Iter>(queryInfo);
							} break;
							case Constraints::DisabledOnly: {
								entCnt += count_inter<false, IterDisabled>(queryInfo);
							} break;
							case Constraints::AcceptAll: {
								entCnt += count_inter<false, IterAll>(queryInfo);
							} break;
						}
					}

					return entCnt;
				}

				//! Appends all components or entities matching the query to the output array
				//! \tparam Container Container type
				//! \param[out] outArray Container storing entities or components
				//! \param constraints QueryImpl constraints
				template <typename Container>
				void arr(Container& outArray, Constraints constraints = Constraints::EnabledOnly) {
					const auto entCnt = count(constraints);
					if (entCnt == 0)
						return;

					outArray.reserve(entCnt);
					auto& queryInfo = fetch();
					match_all(queryInfo);

					const bool hasFilters = queryInfo.has_filters();
					if (hasFilters) {
						switch (constraints) {
							case Constraints::EnabledOnly:
								arr_inter<true, Iter>(queryInfo, outArray);
								break;
							case Constraints::DisabledOnly:
								arr_inter<true, IterDisabled>(queryInfo, outArray);
								break;
							case Constraints::AcceptAll:
								arr_inter<true, IterAll>(queryInfo, outArray);
								break;
						}
					} else {
						switch (constraints) {
							case Constraints::EnabledOnly:
								arr_inter<false, Iter>(queryInfo, outArray);
								break;
							case Constraints::DisabledOnly:
								arr_inter<false, IterDisabled>(queryInfo, outArray);
								break;
							case Constraints::AcceptAll:
								arr_inter<false, IterAll>(queryInfo, outArray);
								break;
						}
					}
				}

				//! Iterates entities matching the query ordered in dependency BFS levels.
				//! For relation R this treats Pair(R, X) on entity E as "E depends on X".
				//! Systems depending on no other matched system are first, then their dependents level-by-level.
				//! Nodes on the same level are ordered by entity id.
				//! \param func Callable invoked for each ordered entity.
				//! \param relation Dependency relation
				//! \param constraints QueryImpl constraints
				template <typename Func>
				void each_bfs(Func func, Entity relation, Constraints constraints = Constraints::EnabledOnly) {
					static_assert(
							std::is_invocable_v<Func, const Entity&>,
							"each_bfs requires a callable with signature void(const Entity&)");

					auto& bfsData = ensure_each_bfs_data();
					auto& world = *m_storage.world();
					const uint32_t relationVersion = world.rel_version(relation);
					auto& queryInfo = fetch();
					match_all(queryInfo);

					if (bfsData.cacheValid && bfsData.cachedRelation == relation && bfsData.cachedConstraints == constraints &&
							bfsData.cachedRelationVersion == relationVersion && !queryInfo.has_filters()) {
						auto& chunks = bfsData.scratchChunks;
						chunks.clear();

						bool chunkChanged = false;
						for (auto* pArchetype: queryInfo) {
							if (pArchetype == nullptr || !can_process_archetype(*pArchetype))
								continue;

							for (const auto* pChunk: pArchetype->chunks()) {
								if (pChunk == nullptr)
									continue;

								chunks.push_back(pChunk);
								if (!chunkChanged && pChunk->changed(bfsData.cachedEntityVersion))
									chunkChanged = true;
							}
						}

						bool sameChunks = chunks.size() == bfsData.cachedChunks.size();
						if (sameChunks) {
							for (uint32_t i = 0; i < (uint32_t)chunks.size(); ++i) {
								if (chunks[i] != bfsData.cachedChunks[i]) {
									sameChunks = false;
									break;
								}
							}
						}

						if (sameChunks && !chunkChanged) {
							for (auto entity: bfsData.cachedOutput)
								func(entity);
							return;
						}
					}

					auto& entities = bfsData.scratchEntities;
					entities.clear();
					arr(entities, constraints);
					if (entities.empty())
						return;

					if (bfsData.cacheValid && bfsData.cachedRelation == relation && bfsData.cachedConstraints == constraints &&
							bfsData.cachedRelationVersion == relationVersion && entities.size() == bfsData.cachedInput.size()) {
						bool sameInput = true;
						for (uint32_t i = 0; i < (uint32_t)entities.size(); ++i) {
							if (entities[i] != bfsData.cachedInput[i]) {
								sameInput = false;
								break;
							}
						}

						if (sameInput) {
							for (auto entity: bfsData.cachedOutput)
								func(entity);
							return;
						}
					}

					auto& ordered = bfsData.cachedOutput;
					bfsData.cachedInput = entities;
					ordered.clear();
					if (!world.valid(relation)) {
						core::sort(entities, [](Entity left, Entity right) {
							return left.id() < right.id();
						});
						ordered = entities;
					} else {
						// Keep execution deterministic regardless of archetype iteration order.
						core::sort(entities, [](Entity left, Entity right) {
							return left.id() < right.id();
						});

						const auto cnt = (uint32_t)entities.size();

						auto& indegree = bfsData.scratchIndegree;
						indegree.resize(cnt);
						auto& outdegree = bfsData.scratchOutdegree;
						outdegree.resize(cnt);
						for (uint32_t i = 0; i < cnt; ++i) {
							indegree[i] = 0;
							outdegree[i] = 0;
						}

						auto find_entity_idx = [&](Entity entity) {
							const auto targetId = entity.id();
							uint32_t low = 0;
							uint32_t high = cnt;
							while (low < high) {
								const uint32_t mid = low + ((high - low) >> 1);
								if (entities[mid].id() < targetId)
									low = mid + 1;
								else
									high = mid;
							}
							if (low < cnt && entities[low].id() == targetId)
								return low;
							return cnt;
						};

						uint32_t edgeCnt = 0;
						for (uint32_t dependentIdx = 0; dependentIdx < cnt; ++dependentIdx) {
							const auto dependent = entities[dependentIdx];
							world.targets(dependent, relation, [&](Entity dependency) {
								const auto dependencyIdx = find_entity_idx(dependency);
								if (dependencyIdx == cnt || dependencyIdx == dependentIdx)
									return;

								++outdegree[dependencyIdx];
								++indegree[dependentIdx];
								++edgeCnt;
							});
						}

						auto& offsets = bfsData.scratchOffsets;
						offsets.resize(cnt + 1);
						offsets[0] = 0;
						for (uint32_t i = 0; i < cnt; ++i)
							offsets[i + 1] = offsets[i] + outdegree[i];

						auto& writeCursor = bfsData.scratchWriteCursor;
						writeCursor.resize(cnt);
						for (uint32_t i = 0; i < cnt; ++i)
							writeCursor[i] = offsets[i];

						auto& edges = bfsData.scratchEdges;
						edges.resize(edgeCnt);
						for (uint32_t dependentIdx = 0; dependentIdx < cnt; ++dependentIdx) {
							const auto dependent = entities[dependentIdx];
							world.targets(dependent, relation, [&](Entity dependency) {
								const auto dependencyIdx = find_entity_idx(dependency);
								if (dependencyIdx == cnt || dependencyIdx == dependentIdx)
									return;

								edges[writeCursor[dependencyIdx]++] = dependentIdx;
							});
						}

						ordered.reserve(cnt);

						auto& currLevel = bfsData.scratchCurrLevel;
						currLevel.clear();
						auto& nextLevel = bfsData.scratchNextLevel;
						nextLevel.clear();
						for (uint32_t i = 0; i < cnt; ++i) {
							if (indegree[i] == 0)
								currLevel.push_back(i);
						}

						auto sort_level = [&](cnt::darray<uint32_t>& level) {
							core::sort(level, [&](uint32_t left, uint32_t right) {
								return entities[left].id() < entities[right].id();
							});
						};

						sort_level(currLevel);

						uint32_t executedCnt = 0;
						while (!currLevel.empty()) {
							for (auto idx: currLevel) {
								ordered.push_back(entities[idx]);
								++executedCnt;
							}

							nextLevel.clear();
							for (auto idx: currLevel) {
								for (uint32_t edgePos = offsets[idx]; edgePos < offsets[idx + 1]; ++edgePos) {
									const auto dependentIdx = edges[edgePos];
									if (indegree[dependentIdx] == 0)
										continue;

									--indegree[dependentIdx];
									if (indegree[dependentIdx] == 0)
										nextLevel.push_back(dependentIdx);
								}
							}

							if (nextLevel.empty())
								break;

							sort_level(nextLevel);
							currLevel.clear();
							currLevel.reserve(nextLevel.size());
							for (auto idx: nextLevel)
								currLevel.push_back(idx);
						}

						// A cycle can still appear in user data. Keep behavior deterministic and output the rest by id.
						if (executedCnt != cnt) {
							for (uint32_t i = 0; i < cnt; ++i) {
								if (indegree[i] > 0)
									ordered.push_back(entities[i]);
							}
						}
					}

					bfsData.cachedRelation = relation;
					bfsData.cachedConstraints = constraints;
					bfsData.cachedRelationVersion = relationVersion;
					bfsData.cachedEntityVersion = world.world_version();
					if (!queryInfo.has_filters()) {
						auto& chunks = bfsData.scratchChunks;
						chunks.clear();
						for (auto* pArchetype: queryInfo) {
							if (pArchetype == nullptr || !can_process_archetype(*pArchetype))
								continue;

							for (const auto* pChunk: pArchetype->chunks()) {
								if (pChunk == nullptr)
									continue;
								chunks.push_back(pChunk);
							}
						}
						bfsData.cachedChunks = chunks;
					} else
						bfsData.cachedChunks.clear();
					bfsData.cacheValid = true;

					for (auto entity: bfsData.cachedOutput)
						func(entity);
				}

				//------------------------------------------------

				//! Run diagnostics
				void diag() {
					// Make sure matching happened
					auto& queryInfo = fetch();
					match_all(queryInfo);
					if constexpr (UseCaching)
						GAIA_LOG_N("BEG DIAG Query %u.%u [C]", id(), gen());
					else
						GAIA_LOG_N("BEG DIAG Query [U]");
					for (const auto* pArchetype: queryInfo)
						Archetype::diag_basic_info(*m_storage.world(), *pArchetype);
					GAIA_LOG_N("END DIAG Query");
				}

				//! Returns a textual dump of the generated query VM bytecode.
				GAIA_NODISCARD util::str bytecode() {
					auto& queryInfo = fetch();
					return queryInfo.bytecode();
				}

				//! Prints a textual dump of the generated query VM bytecode.
				void diag_bytecode() {
					const auto dump = bytecode();
					if constexpr (UseCaching)
						GAIA_LOG_N("BEG DIAG Query Bytecode %u.%u [C]", id(), gen());
					else
						GAIA_LOG_N("BEG DIAG Query Bytecode [U]");
					GAIA_LOG_N("%.*s", (int)dump.size(), dump.data());
					GAIA_LOG_N("END DIAG Query");
				}
			};
		} // namespace detail

		using Query = typename detail::QueryImpl<true>;
		using QueryUncached = typename detail::QueryImpl<false>;
	} // namespace ecs
} // namespace gaia
