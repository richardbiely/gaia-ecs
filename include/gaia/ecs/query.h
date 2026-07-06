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
#include "gaia/ecs/sched.h"
#include "gaia/mem/smallblock_allocator.h"
#include "gaia/ser/ser_buffer_binary.h"
#include "gaia/ser/ser_ct.h"
#include "gaia/util/str.h"

namespace gaia {
	namespace ecs {
		class World;
		void world_finish_write(World& world, Entity term, Entity entity);
		const Sched& world_sched(const World& world);

		//! Maximal cap for cached traversed-source closure snapshots.
		inline static constexpr uint16_t MaxCacheSrcTrav = 32;

		//! Query execution type.
		enum class QueryExecType : uint32_t {
			//! Main thread
			Serial,
			//! Parallel, any core
			Parallel,
			//! Parallel, perf cores only
			ParallelPerf,
			//! Parallel, efficiency cores only
			ParallelEff,
			//! Default execution type
			Default = Serial,
		};

		//! Hard cache-kind requirement for a query.
		enum class QueryCacheKind : uint8_t {
			//! Disable result caching. The query keeps its immutable compiled plan locally
			//! and rebuilds transient results on demand.
			None,
			//! Use the engine's default cache behavior.
			//! Recommended for most queries.
			//! Allows all automatic cache layers:
			//! - immediate structural cache
			//! - lazy structural cache
			//! - dynamic relation/source cache
			//! Explicit traversed-source snapshot opt-ins are also allowed.
			Default,
			//! Restrict the query to automatically derived cache layers only.
			//! Use this when the engine should decide the cache behavior on its own.
			//! Allows:
			//! - immediate structural cache
			//! - lazy structural cache
			//! - dynamic relation/source cache derived automatically by query shape
			//! Rejects explicit traversed-source snapshot opt-ins.
			Auto,
			//! Require a fully immediate structural cache.
			//! Use this only when query creation must fail unless the query can stay
			//! on the immediate structural cache layer.
			//! Allows only the immediate structural cache layer.
			//! Query creation fails for lazy, dynamic, or explicit traversed-source snapshot layers.
			All
		};

		//! Scope of cache-backed query state.
		enum class QueryCacheScope : uint8_t {
			//! Keep cached query state private to this query instance and its copies.
			Local,
			//! Allow identical query shapes to share one cached query state through the world cache.
			Shared
		};

		//! Result of validating a query shape against the requested QueryCacheKind.
		enum class QueryKindRes : uint8_t {
			//! The requested kind is satisfied.
			OK,
			//! QueryCacheKind::Auto rejected an explicit traversed-source snapshot opt-in.
			AutoSrcTrav,
			//! QueryCacheKind::All requires a fully immediate structural cache, but the query shape does not allow it.
			AllNotIm,
			//! QueryCacheKind::All rejected an explicit traversed-source snapshot opt-in.
			AllSrcTrav
		};

		//! Entity traversal order for Query::order_by(Entity, TravOrder).
		//!
		//! If entity E has Pair(relation, X), E is the source and X is the target.
		//! Down visits targets before sources. Up visits sources before their targets.
		enum class TravOrder : uint8_t {
			//! Visit each source before the target it points at.
			Up,
			//! Alias for TravOrder::Up.
			Postorder = Up,
			//! Visit each target before the sources that point at it.
			Down,
			//! Alias for TravOrder::Down.
			Preorder = Down,
			//! Exact reverse of TravOrder::Up.
			ReverseUp,
			//! Alias for TravOrder::ReverseUp.
			ReversePostorder = ReverseUp,
			//! Exact reverse of TravOrder::Down.
			ReverseDown,
			//! Alias for TravOrder::ReverseDown.
			ReversePreorder = ReverseDown
		};

		using QueryCachePolicy = QueryCtx::CachePolicy;
		struct TypedQueryExecState;

		namespace detail {
			template <typename Func>
			inline constexpr bool is_query_iter_callback_v = std::is_invocable_v<Func, Iter&>;

			template <typename Func>
			inline constexpr bool is_query_walk_core_callback_v =
					is_query_iter_callback_v<Func> || std::is_invocable_v<Func, const Entity&> ||
					std::is_invocable_v<Func, Entity>;

			//! Query command types
			enum QueryCmdType : uint8_t { ADD_ITEM, ADD_FILTER, SORT_BY, GROUP_BY, GROUP_DEP, MATCH_PREFAB };

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

					ctxData.ids[ctxData.idsCnt] = item.id;
					ctxData.terms[ctxData.idsCnt] = {item.id,				item.entSrc,		item.entTrav,
																					 item.travKind, item.travDepth, item.matchKind,
																					 nullptr,				item.op,				(uint8_t)ctxData.idsCnt};
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
						if (ctxData.ids[compIdx] == comp)
							break;

					// NOTE: Code bellow does the same as this commented piece.
					//       However, compilers can't quite optimize it as well because it does some more
					//       calculations. This is used often so go with the custom code.
					// const auto compIdx = core::get_index_unsafe(ids, comp);

					// Component has to be present in all/or lists.
					// Filtering by NOT/ANY doesn't make sense because those are not hard requirements.
					GAIA_ASSERT2(
							ctxData.terms[compIdx].op != QueryOpKind::Not && ctxData.terms[compIdx].op != QueryOpKind::Any,
							"Filtering by NOT/ANY doesn't make sense!");
					if (ctxData.terms[compIdx].op != QueryOpKind::Not && ctxData.terms[compIdx].op != QueryOpKind::Any) {
						ctxData.changed[ctxData.changedCnt++] = comp;
						return;
					}

					const auto* compName = ctx.cc->get(comp).name.str();
					GAIA_LOG_E("SetChangeFilter trying to filter component %s but it's not a part of the query!", compName);
#else
					ctxData.changed[ctxData.changedCnt++] = comp;
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
				uint16_t flags;

				void exec(QueryCtx& ctx) const {
					auto& ctxData = ctx.data;
					ctxData.groupBy = groupBy;
					GAIA_ASSERT(func != nullptr);
					ctxData.groupByFunc = func; // group_by_func_default;
					if ((flags & QueryCtx::QueryFlags::OrderGroups) != 0)
						ctxData.flags |= QueryCtx::QueryFlags::OrderGroups;
					else
						ctxData.flags &= ~QueryCtx::QueryFlags::OrderGroups;
				}
			};

			struct QueryCmd_GroupDep {
				static constexpr QueryCmdType Id = QueryCmdType::GROUP_DEP;
				static constexpr bool InvalidatesHash = true;

				Entity relation;

				void exec(QueryCtx& ctx) const {
					auto& ctxData = ctx.data;
					GAIA_ASSERT(!relation.pair());
					ctxData.add_group_dep(relation);
				}
			};

			struct QueryCmd_MatchPrefab {
				static constexpr QueryCmdType Id = QueryCmdType::MATCH_PREFAB;
				static constexpr bool InvalidatesHash = true;

				void exec(QueryCtx& ctx) const {
					ctx.data.flags |= QueryCtx::QueryFlags::MatchPrefab;
				}
			};

			struct QueryImplStorage {
				World* m_world = nullptr;
				//! QueryImpl cache (stable pointer to parent world's query cache)
				QueryCache* m_pCache = nullptr;
				//! Hot cached query pointer. Validated against m_identity.handle before use.
				QueryInfo* m_pInfo = nullptr;
				//! Locally-owned query plan used when the query does not use cache-backed storage.
				QueryInfo* m_pOwnedInfo = nullptr;
				//! Query identity
				QueryIdentity m_identity{};
				bool m_destroyed = false;

			public:
				QueryImplStorage() = default;
				~QueryImplStorage() {
					(void)try_del_from_cache();
					delete m_pOwnedInfo;
				}

				QueryImplStorage(QueryImplStorage&& other) {
					m_world = other.m_world;
					m_pCache = other.m_pCache;
					m_pInfo = other.m_pInfo;
					m_pOwnedInfo = other.m_pOwnedInfo;
					m_identity = other.m_identity;
					m_destroyed = other.m_destroyed;

					// Make sure old instance is invalidated
					other.m_pInfo = nullptr;
					other.m_pOwnedInfo = nullptr;
					other.m_identity = {};
					other.m_destroyed = false;
				}
				QueryImplStorage& operator=(QueryImplStorage&& other) {
					GAIA_ASSERT(core::addressof(other) != this);

					(void)try_del_from_cache();
					delete m_pOwnedInfo;

					m_world = other.m_world;
					m_pCache = other.m_pCache;
					m_pInfo = other.m_pInfo;
					m_pOwnedInfo = other.m_pOwnedInfo;
					m_identity = other.m_identity;
					m_destroyed = other.m_destroyed;

					// Make sure old instance is invalidated
					other.m_pInfo = nullptr;
					other.m_pOwnedInfo = nullptr;
					other.m_identity = {};
					other.m_destroyed = false;

					return *this;
				}

				QueryImplStorage(const QueryImplStorage& other) {
					m_world = other.m_world;
					m_pCache = other.m_pCache;
					m_pInfo = other.m_pInfo;
					if (other.m_pOwnedInfo != nullptr)
						m_pOwnedInfo = new QueryInfo(*other.m_pOwnedInfo);
					m_identity = other.m_identity;
					m_destroyed = other.m_destroyed;

					// Make sure to update the ref count of the cached query so
					// it doesn't get deleted by accident.
					if (!m_destroyed) {
						auto* pInfo = try_query_info_fast();
						if (pInfo == nullptr)
							pInfo = m_pCache->try_get(m_identity.handle);
						if (pInfo != nullptr)
							pInfo->add_ref();
					}
				}
				QueryImplStorage& operator=(const QueryImplStorage& other) {
					GAIA_ASSERT(core::addressof(other) != this);

					(void)try_del_from_cache();
					delete m_pOwnedInfo;
					m_pOwnedInfo = nullptr;

					m_world = other.m_world;
					m_pCache = other.m_pCache;
					m_pInfo = other.m_pInfo;
					if (other.m_pOwnedInfo != nullptr)
						m_pOwnedInfo = new QueryInfo(*other.m_pOwnedInfo);
					m_identity = other.m_identity;
					m_destroyed = other.m_destroyed;

					// Make sure to update the ref count of the cached query so
					// it doesn't get deleted by accident.
					if (!m_destroyed) {
						auto* pInfo = try_query_info_fast();
						if (pInfo == nullptr)
							pInfo = m_pCache->try_get(m_identity.handle);
						if (pInfo != nullptr)
							pInfo->add_ref();
					}

					return *this;
				}

				//! Returns the world associated with this query storage.
				//! \return World associated with this query storage.
				GAIA_NODISCARD World* world() {
					return m_world;
				}

				//! Returns the serialized command buffer for this query.
				//! \return Serialized command buffer.
				GAIA_NODISCARD QuerySerBuffer& ser_buffer() {
					return m_identity.ser_buffer(m_world);
				}

				//! Releases the serialized command buffer for this query.
				void ser_buffer_reset() {
					return m_identity.ser_buffer_reset(m_world);
				}

				//! Initializes storage against a world and query cache.
				//! \param world World owning the query.
				//! \param queryCache Query cache owned by the world.
				void init(World* world, QueryCache* queryCache) {
					m_world = world;
					m_pCache = queryCache;
					m_pInfo = nullptr;
				}

				//! Releases any data allocated by the query.
				void reset() {
					if (auto* pInfo = try_query_info_fast(); pInfo != nullptr)
						pInfo->reset();
					if (m_pOwnedInfo != nullptr)
						m_pOwnedInfo->reset();
				}

				//! Allows this storage object to destroy cache-backed state again after a move/copy handoff.
				void allow_to_destroy_again() {
					m_destroyed = false;
				}

				//! Tries to delete the query from the query cache.
				//! \return False.
				GAIA_NODISCARD bool try_del_from_cache() {
					if (!m_destroyed && m_identity.handle.id() != QueryIdBad)
						m_pCache->del(m_identity.handle);

					// Don't allow multiple calls to destroy to break the reference counter.
					// One object is only allowed to destroy once.
					m_pInfo = nullptr;
					m_destroyed = true;
					return false;
				}

				//! Invalidates the query handle.
				void invalidate() {
					m_pInfo = nullptr;
					m_identity.handle = {};
					delete m_pOwnedInfo;
					m_pOwnedInfo = nullptr;
				}

				//! Returns the cached QueryInfo pointer when the fast-path cache is still valid.
				//! \return Cached QueryInfo pointer or nullptr.
				GAIA_NODISCARD QueryInfo* try_query_info_fast() const {
					if (m_pInfo == nullptr || m_identity.handle.id() == QueryIdBad || m_pCache == nullptr)
						return nullptr;

					auto* pInfo = m_pCache->try_get(m_identity.handle);
					return pInfo == m_pInfo ? pInfo : nullptr;
				}

				//! Caches the hot QueryInfo pointer locally.
				//! \param queryInfo Query info
				void cache_query_info(QueryInfo& queryInfo) {
					m_pInfo = &queryInfo;
				}

				//! Returns whether storage owns a local QueryInfo instance.
				//! \return True if a local QueryInfo exists. False otherwise.
				GAIA_NODISCARD bool has_owned_query_info() const {
					return m_pOwnedInfo != nullptr;
				}

				//! Returns the locally-owned QueryInfo.
				//! \return Locally-owned QueryInfo.
				GAIA_NODISCARD QueryInfo& owned_query_info() {
					GAIA_ASSERT(m_pOwnedInfo != nullptr);
					return *m_pOwnedInfo;
				}

				//! Stores a locally-owned QueryInfo, replacing the old one if present.
				//! \param queryInfo Query info to store.
				void init_owned_query_info(QueryInfo&& queryInfo) {
					if (m_pOwnedInfo == nullptr)
						m_pOwnedInfo = new QueryInfo(GAIA_MOV(queryInfo));
					else
						*m_pOwnedInfo = GAIA_MOV(queryInfo);
				}

				//! Returns whether the query is found in the query cache.
				//! \return True if the query is found in the query cache. False otherwise.
				GAIA_NODISCARD bool is_cached() const {
					auto* pInfo = try_query_info_fast();
					if (pInfo == nullptr)
						pInfo = m_pCache->try_get(m_identity.handle);
					return pInfo != nullptr;
				}

				//! Returns whether the query is ready to be used.
				//! \return True if the query is ready to be used. False otherwise.
				GAIA_NODISCARD bool is_initialized() const {
					return m_world != nullptr && m_pCache != nullptr;
				}
			};
			class QueryImpl {
				static constexpr uint32_t ChunkBatchSize = 32;
				friend class SystemBuilder;

				struct ChunkBatch {
					const Archetype* pArchetype;
					Chunk* pChunk;
					const uint8_t* pCompIndices;
					InheritedTermDataView inheritedData;
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
					cnt::darray<Entity> termEntities;
					cnt::darray<Entity> bucketEntities;
					cnt::darray<uint32_t> counts;
					uint32_t seenVersion = 1;
				};

			private:
				GAIA_NODISCARD bool uses_query_cache_storage() const {
					return m_cacheKind != QueryCacheKind::None;
				}

				GAIA_NODISCARD bool uses_shared_cache_layer() const {
					return uses_query_cache_storage() && m_cacheScope == QueryCacheScope::Shared;
				}

				void invalidate_query_storage() {
					if (uses_query_cache_storage())
						(void)m_storage.try_del_from_cache();
					m_storage.invalidate();
				}

				//! Returns the per-thread scratch storage used by the direct entity-seeded query fast path.
				//! \return Per-thread scratch storage.
				GAIA_NODISCARD static DirectQueryScratch& direct_query_scratch() {
					static thread_local DirectQueryScratch scratch;
					return scratch;
				}

				//! Grows the direct-query seen/count array so the given entity id can be addressed directly.
				//! \param scratch Scratch storage
				//! \param entityId Entity id that must fit into the array.
				static void ensure_direct_query_count_capacity(DirectQueryScratch& scratch, uint32_t entityId) {
					if (entityId < scratch.counts.size())
						return;

					const auto doubledSize = (uint32_t)scratch.counts.size() * 2U;
					const auto minSize = doubledSize > 64U ? doubledSize : 64U;
					const auto newSize = (entityId + 1U) > minSize ? (entityId + 1U) : minSize;
					scratch.counts.resize(newSize, 0);
				}

				//! Advances the scratch version used to deduplicate direct seeded entities without clearing on every call.
				//! \param scratch Scratch storage
				//! \return Next seen-version value.
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
						// GroupDep
						[](QuerySerBuffer& buffer, QueryCtx& ctx) {
							QueryCmd_GroupDep cmd;
							ser::load(buffer, cmd);
							cmd.exec(ctx);
						},
						// MatchPrefab
						[](QuerySerBuffer& buffer, QueryCtx& ctx) {
							QueryCmd_MatchPrefab cmd;
							ser::load(buffer, cmd);
							cmd.exec(ctx);
						} //
				}; // namespace detail

				//! Storage for cache-backed and locally-owned query state.
				QueryImplStorage m_storage;
				//! World version (stable pointer to parent world's m_nextArchetypeId)
				ArchetypeId* m_nextArchetypeId{};
				//! World version (stable pointer to parent world's world version)
				uint32_t* m_worldVersion{};
				//! Map of component ids to archetypes (stable pointer to parent world's archetype component-to-archetype map)
				const EntityToArchetypeMap* m_entityToArchetypeMap{};
				//! Revision table for component-id lookup buckets that can reorder after removal.
				const EntityToArchetypeVersionMap* m_entityToArchetypeMapVersions{};
				//! All world archetypes
				const ArchetypeDArray* m_allArchetypes{};
				//! Optional user-defined names for Var0..Var7.
				cnt::sarray<util::str, MaxVarCnt> m_varNames;
				//! Bitmask of variable names set in m_varNames.
				uint8_t m_varNamesMask = 0;
				//! Runtime variable bindings for Var0..Var7.
				cnt::sarray<Entity, MaxVarCnt> m_varBindings;
				//! Bitmask of variable bindings set in m_varBindings.
				uint8_t m_varBindingsMask = 0;
				//! Runtime-selected group id for grouped iteration.
				GroupId m_groupIdSet = 0;
				//! World version seen by this query instance for changed() filters.
				uint32_t m_changedWorldVersion = 0;
				//! Batches used for parallel query processing
				//! TODO: This is just temporary until a smarter system is introduced
				cnt::darray<ChunkBatch> m_batches;
				//! User-requested cache-kind restriction.
				QueryCacheKind m_cacheKind = QueryCacheKind::Default;
				//! User-requested cache-scope selection.
				QueryCacheScope m_cacheScope = QueryCacheScope::Local;
				//! Traversed-source closure size allowed for explicit snapshot reuse.
				uint16_t m_cacheSrcTrav = 0;
				//! User-owned data pointer exposed to iterator callbacks.
				void* m_ctx = nullptr;
				//! True when this query must run on the main thread/serial path.
				bool m_mainThread = false;
				//! User-declared accesses that are not part of the query term shape.
				QueryAccessSet m_access;

				//! Walk-specific cache and scratch storage, allocated on-demand.
				struct EachWalkData {
					//! Cached raw entity list for each_walk.
					cnt::darray<Entity> cachedInput;
					//! Cached ordered entity list for each_walk.
					cnt::darray<Entity> cachedOutput;
					//! Cached contiguous chunk-row runs for typed each_walk fast-paths.
					cnt::darray<detail::BfsChunkRun> cachedRuns;
					//! Cached relation used by each_walk.
					Entity cachedRelation = EntityBad;
					//! Cached traversal order used by each_walk.
					TravOrder cachedOrder = TravOrder::Down;
					//! Cached constraints used by each_walk.
					Constraints cachedConstraints = Constraints::EnabledOnly;
					//! Relation version when each_walk cache was produced.
					uint32_t cachedRelationVersion = 0;
					//! World version snapshot used for chunk-structural change checks.
					uint32_t cachedEntityVersion = 0;
					//! Query result-cache revision when each_walk cache was produced.
					uint32_t cachedResultCacheRevision = 0;
					//! Cached matched chunk pointers used by each_walk fast-path.
					cnt::darray<const Chunk*> cachedChunks;
					//! True if each_walk cache is valid.
					bool cacheValid = false;
					//! Scratch entities for each_walk computation.
					cnt::darray<Entity> scratchEntities;
					//! Scratch matched chunk pointers for each_walk fast-path.
					cnt::darray<const Chunk*> scratchChunks;
					//! Scratch indegree array for each_walk computation.
					cnt::darray<uint32_t> scratchIndegree;
					//! Scratch outdegree array for each_walk computation.
					cnt::darray<uint32_t> scratchOutdegree;
					//! Scratch adjacency offsets (CSR) for each_walk computation.
					cnt::darray<uint32_t> scratchOffsets;
					//! Scratch adjacency write cursor (CSR fill stage).
					cnt::darray<uint32_t> scratchWriteCursor;
					//! Scratch adjacency edges (CSR) for each_walk computation.
					cnt::darray<uint32_t> scratchEdges;
					//! Scratch level queue for current walk layer.
					cnt::darray<uint32_t> scratchCurrLevel;
					//! Scratch level queue for next walk layer.
					cnt::darray<uint32_t> scratchNextLevel;
				};

				template <typename T>
				struct OnDemandDataHolder {
					T* pData = nullptr;

					OnDemandDataHolder() = default;

					~OnDemandDataHolder() {
						delete pData;
					}

					OnDemandDataHolder(const OnDemandDataHolder& other) {
						if (other.pData != nullptr)
							pData = new T(*other.pData);
					}

					OnDemandDataHolder& operator=(const OnDemandDataHolder& other) {
						if (core::addressof(other) == this)
							return *this;

						if (other.pData == nullptr) {
							delete pData;
							pData = nullptr;
							return *this;
						}

						if (pData == nullptr)
							pData = new T(*other.pData);
						else
							*pData = *other.pData;

						return *this;
					}

					OnDemandDataHolder(OnDemandDataHolder&& other) noexcept: pData(other.pData) {
						other.pData = nullptr;
					}

					OnDemandDataHolder& operator=(OnDemandDataHolder&& other) noexcept {
						if (core::addressof(other) == this)
							return *this;

						delete pData;
						pData = other.pData;
						other.pData = nullptr;
						return *this;
					}

					GAIA_NODISCARD T* get() {
						return pData;
					}

					GAIA_NODISCARD const T* get() const {
						return pData;
					}

					GAIA_NODISCARD T& ensure() {
						if (pData == nullptr)
							pData = new T();
						return *pData;
					}

					void reset() {
						delete pData;
						pData = nullptr;
					}
				};

				//! Optional on-demand storage for walk iteration data.
				OnDemandDataHolder<EachWalkData> m_eachWalkData;

				//! Cached run data for repeated direct-seeded semantic/inherited entity walks.
				struct DirectSeedRunData {
					cnt::darray<Entity> cachedEntities;
					cnt::darray<Entity> cachedChunkOrderedEntities;
					cnt::darray<detail::BfsChunkRun> cachedRuns;
					Entity cachedSeedTerm = EntityBad;
					QueryMatchKind cachedSeedMatchKind = QueryMatchKind::Semantic;
					Constraints cachedConstraints = Constraints::EnabledOnly;
					uint32_t cachedRelVersion = 0;
					uint32_t cachedWorldVersion = 0;
					bool cacheValid = false;
				};

				OnDemandDataHolder<DirectSeedRunData> m_directSeedRunData;

				//! Merges two access modes, preserving write access as the strongest mode.
				//! \param lhs First access mode.
				//! \param rhs Second access mode.
				//! \return Strongest access mode represented by either argument.
				GAIA_NODISCARD static QueryAccess merge_access(QueryAccess lhs, QueryAccess rhs) {
					if (lhs == QueryAccess::Write || rhs == QueryAccess::Write)
						return QueryAccess::Write;
					if (lhs == QueryAccess::Read || rhs == QueryAccess::Read)
						return QueryAccess::Read;
					return QueryAccess::None;
				}

				//! Returns access requested by positive non-pair query terms for an id.
				//! \param data Compiled query data.
				//! \param entity Component/entity id to inspect.
				//! \return Access inferred from query terms, or None when no data access is declared by the query shape.
				GAIA_NODISCARD static QueryAccess term_access(const QueryCtx::Data& data, Entity entity) {
					if (entity == EntityBad || entity.pair())
						return QueryAccess::None;

					QueryAccess access = QueryAccess::None;
					const auto terms = data.terms_view();
					GAIA_FOR((uint32_t)terms.size()) {
						const auto& term = terms[i];
						if (term.id != entity || (term.op != QueryOpKind::All && term.op != QueryOpKind::Or))
							continue;

						if ((data.readWriteMask & (uint16_t(1) << i)) != 0)
							return QueryAccess::Write;
						access = QueryAccess::Read;
					}

					return access;
				}

				//! Returns effective access from query terms plus explicit user declarations.
				//! \param data Compiled query data.
				//! \param accessSet Explicit user access declarations.
				//! \param entity Component/entity id to inspect.
				//! \return Effective access mode.
				GAIA_NODISCARD static QueryAccess
				effective_access(const QueryCtx::Data& data, const QueryAccessSet& accessSet, Entity entity) {
					return merge_access(term_access(data, entity), accessSet.access(entity));
				}

				//! Returns whether two access modes conflict.
				//! \param lhs First access mode.
				//! \param rhs Second access mode.
				//! \return True when at least one side writes and the other side accesses the same id.
				GAIA_NODISCARD static bool access_conflicts(QueryAccess lhs, QueryAccess rhs) {
					return (lhs == QueryAccess::Write && rhs != QueryAccess::None) ||
								 (rhs == QueryAccess::Write && lhs != QueryAccess::None);
				}

				//! Checks one access set against another query's effective access declarations.
				//! \param leftData Compiled query data for the left query.
				//! \param leftAccess Explicit access set for the left query.
				//! \param rightData Compiled query data for the right query.
				//! \param rightAccess Explicit access set for the right query.
				//! \return True when any access on the left conflicts with the right query.
				GAIA_NODISCARD static bool conflicts_one_way(
						const QueryCtx::Data& leftData, const QueryAccessSet& leftAccess, const QueryCtx::Data& rightData,
						const QueryAccessSet& rightAccess) {
					const auto terms = leftData.terms_view();
					GAIA_FOR((uint32_t)terms.size()) {
						const auto id = terms[i].id;
						const auto access = term_access(leftData, id);
						if (access_conflicts(access, effective_access(rightData, rightAccess, id)))
							return true;
					}

					for (const auto id: leftAccess.reads_view()) {
						if (access_conflicts(QueryAccess::Read, effective_access(rightData, rightAccess, id)))
							return true;
					}
					for (const auto id: leftAccess.writes_view()) {
						if (access_conflicts(QueryAccess::Write, effective_access(rightData, rightAccess, id)))
							return true;
					}

					return false;
				}

				//! Resolves a component, entity type, or pair type to a Gaia entity id for explicit access declarations.
				//! \tparam T Component, entity type, or pair type.
				//! \return Registered entity or pair id.
				template <typename T>
				GAIA_NODISCARD Entity access_entity_inter() {
					if constexpr (is_pair<T>::value) {
						const auto& descRel = comp_cache_add<typename T::rel_type>(*m_storage.world());
						const auto& descTgt = comp_cache_add<typename T::tgt_type>(*m_storage.world());
						return Pair(descRel.entity, descTgt.entity);
					} else {
						using UO = typename component_type_t<T>::TypeOriginal;
						static_assert(core::is_raw_v<UO>, "Use reads()/writes() with raw types only");
						const auto& desc = comp_cache_add<T>(*m_storage.world());
						return desc.entity;
					}
				}

				//--------------------------------------------------------------------------------
			public:
				static inline bool SilenceInvalidCacheKindAssertions = false;

				//! Fetches the QueryInfo object.
				//! Creates or refreshes the backing QueryInfo if needed.
				//! \return QueryInfo object.
				QueryInfo& fetch() {
					GAIA_PROF_SCOPE(query::fetch);

					// Make sure the query was created by World::query()
					GAIA_ASSERT(m_storage.is_initialized());

					if (!uses_query_cache_storage()) {
						if GAIA_UNLIKELY (!m_storage.has_owned_query_info()) {
							QueryCtx ctx;
							ctx.init(m_storage.world());
							commit(ctx);
							m_storage.init_owned_query_info(
									QueryInfo::create(QueryId{}, GAIA_MOV(ctx), *m_entityToArchetypeMap, all_archetypes_view()));
						} else if GAIA_UNLIKELY (m_storage.m_identity.serId != QueryIdBad) {
							recommit(m_storage.owned_query_info().ctx());
						}

						return m_storage.owned_query_info();
					}

					// If queryId is set it means QueryInfo was already created.
					// This is the common case for cached queries.
					if GAIA_LIKELY (m_storage.m_identity.handle.id() != QueryIdBad) {
						auto* pQueryInfo = m_storage.try_query_info_fast();
						if GAIA_UNLIKELY (pQueryInfo == nullptr)
							pQueryInfo = m_storage.m_pCache->try_get(m_storage.m_identity.handle);

						// The only time when this can be nullptr is just once after Query::destroy is called.
						if GAIA_LIKELY (pQueryInfo != nullptr) {
							m_storage.cache_query_info(*pQueryInfo);
							if GAIA_UNLIKELY (m_storage.m_identity.serId != QueryIdBad)
								recommit(pQueryInfo->ctx());
							return *pQueryInfo;
						}

						m_storage.invalidate();
					}

					// No queryId is set which means QueryInfo needs to be created
					QueryCtx ctx;
					ctx.init(m_storage.world());
					commit(ctx);
					auto& queryInfo =
							uses_shared_cache_layer()
									? m_storage.m_pCache->add(GAIA_MOV(ctx), *m_entityToArchetypeMap, all_archetypes_view())
									: m_storage.m_pCache->add_local(GAIA_MOV(ctx), *m_entityToArchetypeMap, all_archetypes_view());
					m_storage.m_identity.handle = QueryInfo::handle(queryInfo);
					m_storage.cache_query_info(queryInfo);
					m_storage.allow_to_destroy_again();
					return queryInfo;
				}

				//! Matches the query against all relevant archetypes.
				//! \param queryInfo Query info
				void match_all(QueryInfo& queryInfo) {
					const auto kindError = validate_kind(queryInfo.ctx());
					if (kindError != QueryKindRes::OK) {
						GAIA_ASSERT2(SilenceInvalidCacheKindAssertions, kind_error_str(kindError));
						queryInfo.reset();
						return;
					}

					if (!uses_query_cache_storage()) {
						queryInfo.ensure_matches_transient(
								*m_entityToArchetypeMap, all_archetypes_view(), *m_entityToArchetypeMapVersions, m_varBindings,
								m_varBindingsMask);
						return;
					}

					queryInfo.ensure_matches(
							*m_entityToArchetypeMap, all_archetypes_view(), *m_entityToArchetypeMapVersions, last_archetype_id(),
							m_varBindings, m_varBindingsMask);
					m_storage.m_pCache->sync_archetype_cache(queryInfo);
				}

				//! Matches the query against a single archetype.
				//! \param queryInfo Query info
				//! \param archetype Archetype
				//! \param targetEntities Optional target-entity filter
				//! \return True if the archetype matches. False otherwise.
				GAIA_NODISCARD bool match_one(QueryInfo& queryInfo, const Archetype& archetype, EntitySpan targetEntities) {
					if (!uses_query_cache_storage()) {
						return queryInfo.ensure_matches_one_transient(archetype, targetEntities, m_varBindings, m_varBindingsMask);
					}

					return queryInfo.ensure_matches_one(archetype, targetEntities, m_varBindings, m_varBindingsMask);
				}

				//! Returns whether any supplied target entity matches the query on @a archetype.
				//! \param queryInfo Query info
				//! \param archetype Archetype
				//! \param targetEntities Candidate target entities
				//! \return True if any target entity matches. False otherwise.
				GAIA_NODISCARD bool matches_any(QueryInfo& queryInfo, const Archetype& archetype, EntitySpan targetEntities) {
					const auto kindError = validate_kind(queryInfo.ctx());
					if (kindError != QueryKindRes::OK) {
						GAIA_ASSERT2(SilenceInvalidCacheKindAssertions, kind_error_str(kindError));
						queryInfo.reset();
						return false;
					}

					return matches_target_entities(queryInfo, archetype, targetEntities);
				}

				//--------------------------------------------------------------------------------

				//! Returns the effective cache policy chosen for the query.
				//! \return Effective cache policy.
				GAIA_NODISCARD QueryCachePolicy cache_policy() {
					return fetch().cache_policy();
				}

				//! Enables traversed-source snapshot reuse and caps the cached source closure size.
				//! This only matters for queries using traversed sources (for example src(...).trav()).
				//! For queries without source traversal the value is normalized away, so it does not
				//! affect shared cache identity.
				//! Use this only when traversed source closures stay small and stable enough for
				//! snapshot reuse to be cheaper than rebuilding them on demand.
				QueryImpl& cache_src_trav(uint16_t maxItems) {
					if (m_cacheSrcTrav == maxItems)
						return *this;

					if (maxItems > MaxCacheSrcTrav) {
						GAIA_ASSERT(false && "cache_src_trav should be a value smaller than MaxCacheSrcTrav");
						maxItems = MaxCacheSrcTrav;
					}

					invalidate_each_walk_cache();
					invalidate_direct_seed_run_cache();
					invalidate_query_storage();
					m_cacheSrcTrav = maxItems;
					return *this;
				}

				//! Returns the traversed-source snapshot cap.
				//! 0 disables explicit traversed-source snapshot caching.
				//! \return Traversed-source snapshot cap.
				GAIA_NODISCARD uint16_t cache_src_trav() const {
					return m_cacheSrcTrav;
				}

				//! \name Query context
				//! \{
				//! Stores raw, user-owned data on this query and exposes it to iterator-style callbacks.
				//! \see Iter::ctx() const
				//! \see SystemBuilder::ctx(void*)

				//! Sets the user-owned context pointer visible through Iter::ctx() during iterator callbacks.
				//! Changing the pointer does not affect query identity, query matching, shared-cache lookup, or cached query
				//! storage. Identical shared-cache query shapes may therefore keep different context pointers.
				//! \note Gaia-ECS stores only the pointer. The caller owns allocation, lifetime, destruction, and any
				//! synchronization needed when a parallel query callback reads or writes the pointed-to data.
				//! \param pCtx Context pointer. May be null.
				//! \return Self reference.
				QueryImpl& ctx(void* pCtx) {
					m_ctx = pCtx;
					return *this;
				}

				//! Returns the user-owned context pointer attached to this query.
				//! \return Context pointer, or null when none is attached.
				GAIA_NODISCARD void* ctx() const {
					return m_ctx;
				}
				//! \}

				//! \name Scheduling declarations
				//! \{
				//! Marks whether this query must run on the main thread/serial path.
				//!
				//! Use this for callbacks with side effects that are not captured by component read/write declarations, such as
				//! external APIs that are main-thread-only. This flag is scheduling metadata only and does not affect query
				//! matching, hashing, shared-cache identity, or cache invalidation.
				//! \param required True when the query requires the main thread/serial path.
				//! \return Self reference.
				QueryImpl& main_thread(bool required = true) {
					m_mainThread = required;
					return *this;
				}

				//! Returns whether this query must run on the main thread/serial path.
				//! \return True when main_thread(true) was set.
				GAIA_NODISCARD bool main_thread_required() const {
					return m_mainThread;
				}
				//! \}

				//! \name Query access declarations
				//! \{
				//! Declares an additional id read by this query callback.
				//!
				//! Use this for data accessed inside the query kernel but not present as a positive query term. The declaration
				//! is scheduling metadata only; it does not change query matching or cache identity.
				//! \param entity Component/entity id read by user code.
				//! \return Self reference.
				//! \see writes(Entity)
				QueryImpl& reads(Entity entity) {
					m_access.add_read(entity);
					return *this;
				}

				//! Declares an additional component or pair type read by this query callback.
				//! \tparam T Component, entity type, or pair type to mark as read.
				//! \return Self reference.
				//! \see reads(Entity)
				template <typename T>
				QueryImpl& reads() {
					return reads(access_entity_inter<T>());
				}

				//! Declares an additional id written by this query callback.
				//!
				//! A write conflicts with any read or write of the same id when comparing two queries with conflicts_with().
				//! The declaration is scheduling metadata only; it does not change query matching or cache identity.
				//! \param entity Component/entity id written by user code.
				//! \return Self reference.
				//! \see reads(Entity)
				QueryImpl& writes(Entity entity) {
					m_access.add_write(entity);
					return *this;
				}

				//! Declares an additional component or pair type written by this query callback.
				//! \tparam T Component, entity type, or pair type to mark as written.
				//! \return Self reference.
				//! \see writes(Entity)
				template <typename T>
				QueryImpl& writes() {
					return writes(access_entity_inter<T>());
				}

				//! Returns explicitly declared read ids that are not query terms.
				//! \return Read-only span over custom read declarations.
				GAIA_NODISCARD std::span<const Entity> custom_reads() const {
					return m_access.reads_view();
				}

				//! Returns explicitly declared write ids that are not query terms.
				//! \return Read-only span over custom write declarations.
				GAIA_NODISCARD std::span<const Entity> custom_writes() const {
					return m_access.writes_view();
				}

				//! Returns the effective read/write access for an id.
				//!
				//! Effective access combines positive query terms and explicit reads()/writes() declarations. Pair query terms
				//! do not imply data access; use explicit reads(Entity) or writes(Entity) when a pair id is a custom scheduling
				//! key.
				//! \param entity Component/entity id to inspect.
				//! \return Effective access mode for the id.
				GAIA_NODISCARD QueryAccess access(Entity entity) {
					return effective_access(fetch().ctx().data, m_access, entity);
				}

				//! Returns whether this query conflicts with another query's effective access declarations.
				//!
				//! Two queries conflict when both access the same id and at least one side writes it. This check uses component
				//! access declared by positive query terms plus custom reads()/writes() declarations. It does not account for
				//! world structural mutation or arbitrary side effects.
				//! \param other Query to compare against.
				//! \return True if the two queries should not run concurrently based on declared access.
				GAIA_NODISCARD bool conflicts_with(QueryImpl& other) {
					const auto& leftData = fetch().ctx().data;
					const auto& rightData = other.fetch().ctx().data;
					return conflicts_one_way(leftData, m_access, rightData, other.m_access) ||
								 conflicts_one_way(rightData, other.m_access, leftData, m_access);
				}

				//! Returns whether this query can run concurrently with another query based on declared scheduling metadata.
				//! \param other Query to compare against.
				//! \return True when neither query requires the main thread and conflicts_with(other) is false.
				GAIA_NODISCARD bool can_run_parallel(QueryImpl& other) {
					return !m_mainThread && !other.m_mainThread && !conflicts_with(other);
				}
				//! \}

				//! Sets the hard cache-kind requirement for the query.
				//! \param cacheKind Requested cache-kind restriction.
				//! \return Self reference.
				QueryImpl& kind(QueryCacheKind cacheKind) {
					if (m_cacheKind == cacheKind)
						return *this;

					invalidate_each_walk_cache();
					invalidate_direct_seed_run_cache();
					invalidate_query_storage();
					m_cacheKind = cacheKind;

					return *this;
				}

				//! Sets the cache scope used by cached queries.
				//! \param cacheScope Whether cache-backed state stays local or can be shared across identical query shapes.
				//! \return Self reference.
				QueryImpl& scope(QueryCacheScope cacheScope) {
					if (m_cacheScope == cacheScope)
						return *this;

					invalidate_each_walk_cache();
					invalidate_direct_seed_run_cache();
					invalidate_query_storage();
					m_cacheScope = cacheScope;

					return *this;
				}

				//! Makes the query include prefab entities in matches.
				//! \return Self reference.
				QueryImpl& match_prefab() {
					QueryCmd_MatchPrefab cmd{};
					add_cmd(cmd);
					return *this;
				}

				//! Returns the currently requested cache scope.
				//! \return Cache scope used by this query.
				GAIA_NODISCARD QueryCacheScope scope() const {
					return m_cacheScope;
				}

				//! Returns the currently requested cache kind.
				//! \return Cache-kind restriction used by this query.
				GAIA_NODISCARD QueryCacheKind kind() const {
					return m_cacheKind;
				}

				//! Returns the validation result for the current query shape and requested kind.
				//! \return Validation result for the query's kind requirement.
				GAIA_NODISCARD QueryKindRes kind_error() {
					return validate_kind(fetch().ctx());
				}

				//! Returns a human-readable description of the current kind validation result.
				//! \return Validation message for the current kind requirement.
				GAIA_NODISCARD const char* kind_error_str() {
					return kind_error_str(kind_error());
				}

				//! Returns whether the current query shape satisfies the requested kind.
				//! \return True when the current kind requirement is satisfied.
				GAIA_NODISCARD bool valid() {
					return kind_error() == QueryKindRes::OK;
				}

				//--------------------------------------------------------------------------------
			private:
				//! Returns whether the query requests traversed-source snapshots beyond the default automatic cache layers.
				//! \param ctx Query context
				//! \return True if manual traversed-source snapshot caching is requested. False otherwise.
				GAIA_NODISCARD bool uses_manual_src_trav_cache(const QueryCtx& ctx) const {
					return m_cacheSrcTrav != 0 && //
								 ctx.data.deps.has_dep_flag(QueryCtx::DependencyHasSourceTerms) && //
								 ctx.data.deps.has_dep_flag(QueryCtx::DependencyHasTraversalTerms);
				}

				//! Returns whether the query uses the immediate structural cache layer.
				//! \param ctx Query context
				//! \return True if the immediate structural cache layer is used. False otherwise.
				GAIA_NODISCARD static bool uses_im_cache(const QueryCtx& ctx) {
					return ctx.data.cachePolicy == QueryCachePolicy::Immediate;
				}

				//! Returns whether the query uses the lazy structural cache layer.
				//! \param ctx Query context
				//! \return True if the lazy structural cache layer is used. False otherwise.
				GAIA_NODISCARD static bool uses_lazy_cache(const QueryCtx& ctx) {
					return ctx.data.cachePolicy == QueryCachePolicy::Lazy;
				}

				//! Returns whether the query uses the dynamic cache layer.
				//! \param ctx Query context
				//! \return True if the dynamic cache layer is used. False otherwise.
				GAIA_NODISCARD static bool uses_dyn_cache(const QueryCtx& ctx) {
					return ctx.data.cachePolicy == QueryCachePolicy::Dynamic;
				}

				//! Validates that the requested public kind can be satisfied by the current query shape.
				//! \param ctx Query context
				//! \return Validation result.
				GAIA_NODISCARD QueryKindRes validate_kind(const QueryCtx& ctx) const {
					if (m_cacheKind == QueryCacheKind::Auto) {
						if (uses_manual_src_trav_cache(ctx))
							return QueryKindRes::AutoSrcTrav;
					}

					if (m_cacheKind == QueryCacheKind::All) {
						if (uses_manual_src_trav_cache(ctx))
							return QueryKindRes::AllSrcTrav;
						if (!uses_im_cache(ctx))
							return QueryKindRes::AllNotIm;
					}

					return QueryKindRes::OK;
				}

				//! Returns a human-readable message for a query kind validation result.
				//! \param error Validation result
				//! \return Text description for @a error.
				GAIA_NODISCARD static const char* kind_error_str(QueryKindRes error) {
					switch (error) {
						case QueryKindRes::OK:
							return "OK";
						case QueryKindRes::AutoSrcTrav:
							return "QueryCacheKind::Auto rejects explicit traversed-source snapshot caching";
						case QueryKindRes::AllNotIm:
							return "QueryCacheKind::All requires a fully immediate structural cache";
						case QueryKindRes::AllSrcTrav:
							return "QueryCacheKind::All rejects explicit traversed-source snapshot caching";
						default:
							return "Unknown query kind validation error";
					}
				}

				//! Returns cached walk-iteration data if present.
				//! \return Walk data pointer or nullptr.
				GAIA_NODISCARD EachWalkData* each_walk_data() {
					return m_eachWalkData.get();
				}

				//! Returns cached walk-iteration data if present.
				//! \return Walk data pointer or nullptr.
				GAIA_NODISCARD const EachWalkData* each_walk_data() const {
					return m_eachWalkData.get();
				}

				//! Returns walk-iteration data, creating it if needed.
				//! \return Walk data reference.
				GAIA_NODISCARD EachWalkData& ensure_each_walk_data() {
					return m_eachWalkData.ensure();
				}

				//! Invalidates cached each_walk state.
				void invalidate_each_walk_cache() {
					auto* pWalkData = each_walk_data();
					if (pWalkData != nullptr)
						pWalkData->cacheValid = false;
				}

				//! Returns cached direct-seed run data if present.
				//! \return Direct-seed run data pointer or nullptr.
				GAIA_NODISCARD DirectSeedRunData* direct_seed_run_data() {
					return m_directSeedRunData.get();
				}

				//! Returns cached direct-seed run data if present.
				//! \return Direct-seed run data pointer or nullptr.
				GAIA_NODISCARD const DirectSeedRunData* direct_seed_run_data() const {
					return m_directSeedRunData.get();
				}

				//! Returns direct-seed run data, creating it if needed.
				//! \return Direct-seed run data reference.
				GAIA_NODISCARD DirectSeedRunData& ensure_direct_seed_run_data() {
					return m_directSeedRunData.ensure();
				}

				//! Invalidates cached direct-seed run state.
				void invalidate_direct_seed_run_cache() {
					auto* pRunData = direct_seed_run_data();
					if (pRunData != nullptr)
						pRunData->cacheValid = false;
				}

				//! Resets changed-filter bookkeeping for this query instance.
				void reset_changed_filter_state() {
					m_changedWorldVersion = 0;
				}

				//! Returns the last allocated archetype id in the world.
				//! \return Last allocated archetype id.
				ArchetypeId last_archetype_id() const {
					return *m_nextArchetypeId - 1;
				}

				//! Returns a view over all archetypes in the parent world.
				//! \return Span over all archetypes.
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

				template <typename T>
				void add_cmd(T& cmd) {
					invalidate_each_walk_cache();

					// Make sure to invalidate if necessary.
					if constexpr (T::InvalidatesHash) {
						reset_changed_filter_state();
						m_storage.invalidate();
					}

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
					add(
							{op, access, entity, options.entSrc, options.entTrav, options.travKind, options.travDepth,
							 options.matchKind});
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
							 options.travDepth, options.matchKind});
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

				void group_by_inter(Entity entity, TGroupByFunc func, bool orderGroups = false) {
					QueryCmd_GroupBy cmd{entity, func, orderGroups ? (uint16_t)QueryCtx::QueryFlags::OrderGroups : (uint16_t)0};
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

				void group_dep_inter(Entity relation) {
					GAIA_ASSERT(!relation.pair());
					QueryCmd_GroupDep cmd{relation};
					add_cmd(cmd);
				}

				template <typename T>
				void group_dep_inter() {
					using UO = typename component_type_t<T>::TypeOriginal;
					static_assert(core::is_raw_v<UO>, "Use group_dep() with raw types only");

					const auto& desc = comp_cache_add<T>(*m_storage.world());
					group_dep_inter(desc.entity);
				}

				//--------------------------------------------------------------------------------

				void set_group_id_inter(GroupId groupId) {
					// Dummy usage of GroupIdMax to avoid warning about unused constant
					(void)GroupIdMax;

					invalidate_each_walk_cache();
					m_groupIdSet = groupId;
				}

				void set_group_id_inter(Entity groupId) {
					set_group_id_inter(groupId.id());
				}

				template <typename T>
				void set_group_id_inter() {
					using UO = typename component_type_t<T>::TypeOriginal;
					static_assert(core::is_raw_v<UO>, "Use group_id() with raw types only");

					// Make sure the component is always registered
					const auto& desc = comp_cache_add<T>(*m_storage.world());
					set_group_id_inter(desc.entity);
				}

				//--------------------------------------------------------------------------------

				void commit(QueryCtx& ctx) {
					GAIA_PROF_SCOPE(query::commit);

#if GAIA_ASSERT_ENABLED
					GAIA_ASSERT(m_storage.m_identity.handle.id() == QueryIdBad);
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
					if (uses_query_cache_storage()) {
						ctx.data.cacheSrcTrav = m_cacheSrcTrav;
						normalize_cache_src_trav(ctx);
					}
					if (uses_shared_cache_layer()) {
						auto& ctxData = ctx.data;
						if (ctxData.changedCnt > 1) {
							core::sort(ctxData.changed.data(), ctxData.changed.data() + ctxData.changedCnt, SortComponentCond{});
						}
					}

					// We can free all temporary data now
					m_storage.ser_buffer_reset();

					// Refresh the context
					ctx.refresh();
					if (uses_shared_cache_layer())
						calc_lookup_hash(ctx);
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
					if (uses_query_cache_storage()) {
						ctx.data.cacheSrcTrav = m_cacheSrcTrav;
						normalize_cache_src_trav(ctx);
					}

					// We can free all temporary data now
					m_storage.ser_buffer_reset();
				}

				//--------------------------------------------------------------------------------
			public:
				//! Returns whether a chunk passes the query's changed filters.
				//! \param chunk Chunk being evaluated.
				//! \param queryInfo Query metadata containing changed-filter terms.
				//! \param changedWorldVersion World version captured by the previous query pass.
				//! \param compIndices Per-archetype mapping from query term index to chunk component column.
				GAIA_NODISCARD static bool match_filters(
						const Chunk& chunk, const QueryInfo& queryInfo, uint32_t changedWorldVersion,
						std::span<const uint8_t> compIndices) {
					GAIA_ASSERT(!chunk.empty() && "match_filters called on an empty chunk");

					const auto queryVersion = changedWorldVersion;
					const auto& data = queryInfo.ctx().data;
					if ((data.flags & (QueryCtx::QueryFlags::HasVariableTerms | QueryCtx::QueryFlags::HasSourceTerms)) != 0)
						return match_filters(chunk, queryInfo, changedWorldVersion);

					const auto changedFields = data.changed_fields_view();

					if (changedFields.empty())
						return false;

					const auto changedCnt = (uint32_t)changedFields.size();
					if (changedCnt == 1) {
						const auto fieldIdx = changedFields[0];
						const auto compIdx = fieldIdx < compIndices.size() ? compIndices[fieldIdx] : (uint8_t)0xFF;
						if (compIdx == (uint8_t)0xFF)
							return match_filters(chunk, queryInfo, changedWorldVersion);
						if (chunk.changed(queryVersion, compIdx))
							return true;

						return chunk.entity_order_changed(changedWorldVersion);
					}

					GAIA_FOR(changedCnt) {
						const auto fieldIdx = changedFields[i];
						const auto compIdx = fieldIdx < compIndices.size() ? compIndices[fieldIdx] : (uint8_t)0xFF;
						if (compIdx == (uint8_t)0xFF)
							return match_filters(chunk, queryInfo, changedWorldVersion);
						if (chunk.changed(queryVersion, compIdx))
							return true;
					}

					return chunk.entity_order_changed(changedWorldVersion);
				}

				//! Returns whether a chunk passes the query's changed filters.
				//! \param chunk Chunk being evaluated.
				//! \param queryInfo Query metadata containing changed-filter terms.
				//! \param changedWorldVersion World version captured by the previous query pass.
				GAIA_NODISCARD static bool
				match_filters(const Chunk& chunk, const QueryInfo& queryInfo, uint32_t changedWorldVersion) {
					GAIA_ASSERT(!chunk.empty() && "match_filters called on an empty chunk");

					const auto queryVersion = changedWorldVersion;
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

						return chunk.entity_order_changed(changedWorldVersion);
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

					// If none of the tracked components changed, row movement can still make the
					// filtered query observable because newly added or moved entities must be seen.
					return chunk.entity_order_changed(changedWorldVersion);
				}

				GAIA_NODISCARD bool can_process_archetype(const QueryInfo& queryInfo, const Archetype& archetype) const {
					// Archetypes requested for deletion are skipped for processing.
					if (archetype.is_req_del())
						return false;

					// Prefabs are excluded from query results by default unless the query opted in
					// explicitly or it mentions Prefab directly.
					if (!queryInfo.matches_prefab_entities() && archetype.has(Prefab))
						return false;

					return true;
				}

				//! Checks whether depth-order grouping can prune disabled hierarchy subtrees.
				//! \param queryInfo Prepared query cache and execution metadata.
				//! \return True when the query groups by depth order on a relation whose hierarchy can prune disabled subtrees.
				GAIA_NODISCARD static bool has_depth_order_hierarchy_enabled_barrier(const QueryInfo& queryInfo) {
					const auto& data = queryInfo.ctx().data;
					return data.groupByFunc == group_by_func_depth_order &&
								 world_relation_depth_order_prunes_disabled_subtrees(*queryInfo.world(), data.groupBy);
				}

				//! Checks whether the current row constraints require the depth-order hierarchy barrier cache.
				//! \param queryInfo Prepared query cache and execution metadata.
				//! \param constraints Entity-row subset exposed to the callback.
				//! \return True when enabled/disabled row iteration needs cached barrier results.
				GAIA_NODISCARD static bool
				needs_depth_order_hierarchy_barrier_cache(const QueryInfo& queryInfo, Constraints constraints) {
					return constraints != Constraints::AcceptAll && has_depth_order_hierarchy_enabled_barrier(queryInfo);
				}

				//! Calculates the row range of a chunk after applying row constraints and depth-order barrier state.
				//! \param pChunk Chunk to inspect.
				//! \param constraints Entity-row subset exposed to the callback.
				//! \param needsBarrierCache True when barrier state participates in row selection.
				//! \param barrierPasses Cached barrier result for the archetype containing the chunk.
				//! \param from Receives the first row to process.
				//! \param to Receives the one-past-the-end row to process.
				static void chunk_effective_range(
						Chunk* pChunk, Constraints constraints, bool needsBarrierCache, bool barrierPasses, uint16_t& from,
						uint16_t& to) noexcept {
					if (needsBarrierCache && constraints == Constraints::DisabledOnly && !barrierPasses) {
						from = 0;
						to = pChunk->size();
						return;
					}

					from = detail::ChunkIterImpl::start_index(pChunk, constraints);
					to = detail::ChunkIterImpl::end_index(pChunk, constraints);
				}

				//! Checks whether cached depth-order barrier results can prune any matched archetype.
				//! \param queryInfo Prepared query cache and execution metadata.
				//! \return True when depth-order hierarchy barriers are active and the cache can prune rows.
				GAIA_NODISCARD static bool depth_order_hierarchy_barrier_prunes(const QueryInfo& queryInfo) {
					return has_depth_order_hierarchy_enabled_barrier(queryInfo) && queryInfo.barrier_may_prune();
				}

				//! Fast enabled-subtree gate for cached depth_order(...) queries over fragmenting hierarchy relations.
				//! ChildOf is the native built-in example, but the rule is semantic: the relation must support
				//! depth_order(...) and also form a fragmenting hierarchy chain. For such relations, all rows in
				//! the archetype share the same direct parent target. That lets us prune the entire archetype when
				//! its parent chain crosses a disabled entity. Non-fragmenting hierarchy relations such as Parent
				//! cannot use this archetype-level check and must stay on the per-entity traversal path instead.
				//! \param queryInfo Prepared query cache and execution metadata.
				//! \param archetype Matched archetype to test.
				//! \return True when the archetype can be processed under the enabled hierarchy barrier.
				GAIA_NODISCARD static bool
				survives_cascade_hierarchy_enabled_barrier(const QueryInfo& queryInfo, const Archetype& archetype) {
					if (!has_depth_order_hierarchy_enabled_barrier(queryInfo))
						return true;

					const auto& world = *queryInfo.world();
					const auto relation = queryInfo.ctx().data.groupBy;
					auto ids = archetype.ids_view();

					for (auto idsIdx: archetype.pair_rel_indices(relation)) {
						const auto pair = ids[idsIdx];
						const auto parent = world_pair_target_if_alive(world, pair);
						if (parent == EntityBad)
							return false;
						if (!world_entity_enabled_hierarchy(world, parent, relation))
							return false;
					}

					return true;
				}

				//! Checks whether a matched archetype can be processed for the current row constraints.
				//! \param queryInfo Prepared query cache and execution metadata.
				//! \param archetype Matched archetype to test.
				//! \param constraints Entity-row subset exposed to the callback.
				//! \param barrierPasses Cached barrier result, or -1 to compute the enabled hierarchy gate directly.
				//! \return True when the archetype is eligible for callback execution.
				GAIA_NODISCARD bool can_process_archetype_inter(
						const QueryInfo& queryInfo, const Archetype& archetype, Constraints constraints,
						int8_t barrierPasses = -1) const {
					if (!can_process_archetype(queryInfo, archetype))
						return false;
					if (constraints == Constraints::EnabledOnly) {
						if (has_depth_order_hierarchy_enabled_barrier(queryInfo)) {
							if (barrierPasses >= 0)
								return barrierPasses != 0;
							if (!survives_cascade_hierarchy_enabled_barrier(queryInfo, archetype))
								return false;
						}
					}
					return true;
				}

				template <typename TIter>
				static void finish_iter_writes(TIter& it) {
					if (it.chunk() == nullptr)
						return;

					auto compIndices = it.touched_comp_indices();
					for (auto compIdx: compIndices)
						const_cast<Chunk*>(it.chunk())->finish_write(compIdx, it.row_begin(), it.row_end());

					auto terms = it.touched_terms();
					if (terms.empty())
						return;

					auto entities = it.entity_rows();
					auto& world = *it.world();
					GAIA_EACH(terms) {
						const auto term = terms[i];
						if (!world_component_uses_sparse_storage(world, term)) {
							const auto compIdx = core::get_index(it.chunk()->ids_view(), term);
							if (compIdx != BadIndex) {
								const_cast<Chunk*>(it.chunk())->finish_write(compIdx, it.row_begin(), it.row_end());
								continue;
							}
						}

						GAIA_FOR_(entities.size(), j) {
							world_finish_write(world, term, entities[j]);
						}
					}
				}

				static void finish_typed_chunk_writes_runtime(
						World& world, Chunk* pChunk, uint16_t from, uint16_t to, const Entity* pArgIds, const bool* pWriteFlags,
						uint32_t argCnt, uint32_t firstWriteArg);

				template <typename... T>
				static void finish_typed_chunk_writes(World& world, Chunk* pChunk, uint16_t from, uint16_t to);

				static void finish_typed_iter_writes_runtime(
						Iter& it, const Entity* pArgIds, const bool* pWriteFlags, uint32_t argCnt, uint32_t firstWriteArg);

				//! Runtime payload layout required by generic chunk-batch execution.
				enum class ExecPayloadKind : uint8_t {
					//! Plain batches without group ids, inherited data, sorted slices, or barrier metadata.
					Plain,
					//! Batches carry group ids but do not require sorted slices or inherited/barrier metadata.
					Grouped,
					//! Batches require non-trivial side payload such as sorted slices, inherited data, or barriers.
					NonTrivial
				};

				//! Classifies the generic batch payload needed for a matched query under row constraints.
				//! \param queryInfo Prepared query cache and execution metadata.
				//! \param constraints Entity-row subset exposed to the callback.
				//! \return Payload layout required by generic chunk-batch runners.
				GAIA_NODISCARD static ExecPayloadKind exec_payload_kind(const QueryInfo& queryInfo, Constraints constraints) {
					if (queryInfo.has_sorted_payload())
						return ExecPayloadKind::NonTrivial;
					if (!queryInfo.has_grouped_payload())
						return ExecPayloadKind::Plain;
					if (needs_depth_order_hierarchy_barrier_cache(queryInfo, constraints))
						return ExecPayloadKind::NonTrivial;
					return ExecPayloadKind::Grouped;
				}

				//! Prepared query runner mode shared by typed callbacks and public Iter callbacks.
				//!
				//! The mode is selected after the query cache has been matched. It describes the runner family
				//! independently from orthogonal query properties such as filters. Public C++ templates are
				//! expected to remain thin adapters on top of this prepared execution metadata.
				enum class QueryPlanMode : uint8_t {
					//! The selected group/range has no matching archetypes, so execution can return immediately.
					Empty,
					//! Use the generic query execution path because no specialized runner is valid.
					General,
					//! Direct entity-seed evaluation over explicitly selected entities.
					EntitySeed,
					//! Direct cached archetype/chunk iteration with query-term indices matching storage layout.
					DirectDense,
					//! Typed dense cached archetype/chunk iteration using mapped component access.
					//! Public Iter callbacks use DirectDense with iterator component-index mapping instead.
					MappedDense,
					//! Sorted payload execution that must preserve cache-provided chunk order.
					Sorted,
					//! Traversal or inherited payload execution that requires the mapped generic path.
					Traversal
				};

				//! Orthogonal flags attached to a prepared query plan.
				//!
				//! Flags describe properties that can apply to multiple runner modes. Keep them separate from
				//! QueryPlanMode so filtered and unfiltered variants do not multiply execution-mode values.
				enum QueryPlanFlags : uint8_t {
					//! No additional query-plan properties are present.
					QueryPlanFlag_None = 0,
					//! The query has per-chunk filters such as changed terms.
					QueryPlanFlag_Filtered = 1 << 0,
					//! The query has entity-filter terms that require per-entity rechecks.
					QueryPlanFlag_EntityFilter = 1 << 1,
					//! The query carries inherited component data into iterator payloads.
					QueryPlanFlag_InheritedPayload = 1 << 2,
					//! The query uses grouped payload/ranges or grouped cache ordering.
					QueryPlanFlag_Grouped = 1 << 3,
					//! The plan may need sorted cache slices; runners use them only with non-trivial payload.
					QueryPlanFlag_Sorted = 1 << 4,
					//! The plan must use the depth-order hierarchy barrier cache when checking archetype/row ranges.
					QueryPlanFlag_BarrierCache = 1 << 5
				};

				//! Prepared query execution metadata shared by typed callbacks and public Iter callbacks.
				struct QueryPlan final {
					//! Runner family selected for the current matched query cache.
					QueryPlanMode mode = QueryPlanMode::General;
					//! Orthogonal plan properties such as filtering, entity filters, grouping, or payload requirements.
					uint8_t flags = QueryPlanFlag_None;
					//! Payload layout required by generic chunk-batch runners independent of sorted-cache availability.
					ExecPayloadKind payloadKind = ExecPayloadKind::Plain;
					//! First cached archetype index to process.
					uint32_t idxFrom = 0;
					//! One-past-the-end cached archetype index to process.
					uint32_t idxTo = 0;
				};

				//! Cache range selected by the query's optional group id filter.
				struct QueryCacheRange final {
					//! First cached archetype index to process.
					uint32_t idxFrom = 0;
					//! One-past-the-end cached archetype index to process.
					uint32_t idxTo = 0;
					//! True when `m_groupIdSet` narrowed the range to one cache group.
					bool hasSelectedGroup = false;
					//! False when the selected group id is absent from the matched cache.
					bool valid = true;
				};

				//! Tag selecting enabled-only row constraints for prepared query iteration.
				struct IterModeEnabled final {};
				//! Tag selecting disabled-only row constraints for prepared query iteration.
				struct IterModeDisabledOnly final {};
				//! Tag selecting unconstrained row iteration for prepared query iteration.
				struct IterModeAcceptAll final {};

				//! Maps an iteration-mode tag to the runtime row constraints used by iterators.
				//! \tparam TMode One of the IterMode* tag types.
				//! \return Entity-row constraints represented by the tag.
				template <typename TMode>
				GAIA_NODISCARD static constexpr Constraints iter_mode_constraints() {
					if constexpr (std::is_same_v<TMode, IterModeDisabledOnly>)
						return Constraints::DisabledOnly;
					else if constexpr (std::is_same_v<TMode, IterModeAcceptAll>)
						return Constraints::AcceptAll;
					else
						return Constraints::EnabledOnly;
				}

				//--------------------------------------------------------------------------------

				//! Executes an iterator callback for one prepared chunk batch.
				//! \tparam Func Callback type invocable with `Iter&`.
				//! \tparam TMode Iteration-mode tag controlling enabled/disabled row constraints.
				//! \param pWorld World owning the chunk batch.
				//! \param func Callback invoked for the initialized iterator.
				//! \param batch Prepared chunk batch to expose through the iterator.
				//! \see run_query_func(World*, Func, std::span<ChunkBatch>)
				template <typename Func, typename TMode>
				static void run_query_func(World* pWorld, Func func, ChunkBatch& batch) {
					Iter it;
					it.init_query_state(pWorld, iter_mode_constraints<TMode>(), false);
					it.set_archetype(batch.pArchetype);
					it.set_chunk(batch.pChunk, batch.from, batch.to);
					it.set_group_id(batch.groupId);
					it.set_comp_indices(batch.pCompIndices);
					it.set_inherited_data(batch.inheritedData);
					func(it);
					finish_iter_writes(it);
					it.clear_touched_writes();
				}

				//! Executes an archetype-level iterator callback for one prepared chunk batch.
				//! The iterator is initialized with archetype metadata but no chunk rows.
				//! \tparam Func Callback type invocable with `Iter&`.
				//! \param pWorld World owning the archetype batch.
				//! \param func Callback invoked for the initialized iterator.
				//! \param batch Prepared batch carrying the archetype and inherited metadata.
				//! \param constraints Entity-row constraints associated with this execution.
				//! \see each_arch(Func, Constraints)
				template <typename Func>
				static void run_query_arch_func(World* pWorld, Func func, ChunkBatch& batch, Constraints constraints) {
					Iter it;
					it.init_query_state(pWorld, constraints, false);
					it.set_archetype(batch.pArchetype);
					// it.set_chunk(nullptr, 0, 0); We do not need this, and calling it would assert
					it.set_group_id(batch.groupId);
					it.set_comp_indices(batch.pCompIndices);
					it.set_inherited_data(batch.inheritedData);
					func(it);
					it.clear_touched_writes();
				}

				//! Executes an iterator callback over a contiguous list of prepared chunk batches.
				//! \tparam Func Callback type invocable with `Iter&`.
				//! \tparam TMode Iteration-mode tag controlling enabled/disabled row constraints.
				//! \param pWorld World owning the chunk batches.
				//! \param func Callback invoked once per initialized chunk iterator.
				//! \param batches Prepared chunk batches to iterate.
				//! \see run_query_func(World*, Func, ChunkBatch&)
				template <typename Func, typename TMode>
				static void run_query_func(World* pWorld, Func func, std::span<ChunkBatch> batches) {
					GAIA_PROF_SCOPE(query::run_query_func);

					const auto chunkCnt = batches.size();
					GAIA_ASSERT(chunkCnt > 0);

					Iter it;
					it.init_query_state(pWorld, iter_mode_constraints<TMode>(), false);

					const Archetype* pLastArchetype = nullptr;
					const uint8_t* pLastIndices = nullptr;
					InheritedTermDataView lastInheritedData{};
					GroupId lastGroupId = GroupIdMax;

					const auto apply_batch = [&](const ChunkBatch& batch) {
						if (batch.pArchetype != pLastArchetype) {
							it.set_archetype(batch.pArchetype);
							pLastArchetype = batch.pArchetype;
						}

						if (batch.pCompIndices != pLastIndices) {
							it.set_comp_indices(batch.pCompIndices);
							pLastIndices = batch.pCompIndices;
						}

						if (batch.inheritedData.data() != lastInheritedData.data()) {
							it.set_inherited_data(batch.inheritedData);
							lastInheritedData = batch.inheritedData;
						}

						if (batch.groupId != lastGroupId) {
							it.set_group_id(batch.groupId);
							lastGroupId = batch.groupId;
						}

						it.set_chunk(batch.pChunk, batch.from, batch.to);
						func(it);
						finish_iter_writes(it);
						it.clear_touched_writes();
					};

					// We only have one chunk to process.
					if GAIA_UNLIKELY (chunkCnt == 1) {
						apply_batch(batches[0]);
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
					apply_batch(batches[0]);

					uint32_t chunkIdx = 1;
					for (; chunkIdx < chunkCnt - 1; ++chunkIdx) {
						gaia::prefetch(batches[chunkIdx + 1].pChunk, PrefetchHint::PREFETCH_HINT_T2);
						apply_batch(batches[chunkIdx]);
					}

					apply_batch(batches[chunkIdx]);
				}

				//------------------------------------------------

				template <typename Func, typename TMode>
				struct QueryJobCtx {
					QueryImpl* pSelf = nullptr;
					World* pWorld = nullptr;
					cnt::darray<ChunkBatch> batches;
					Func func;

					GAIA_USE_SMALLBLOCK(QueryJobCtx)
				};

				template <typename Func>
				struct QueryTaskJobCtx {
					QueryImpl* pSelf = nullptr;
					Func func;
					QueryExecType execType = QueryExecType::Default;

					GAIA_USE_SMALLBLOCK(QueryTaskJobCtx)
				};

				//! Callback adapter that sets iterator context before invoking an `Iter&` user callback.
				//! \tparam Func User callback type invocable with `Iter&`.
				template <typename Func>
				struct IterJobCallback {
					//! Query owning the runtime context pointer.
					QueryImpl* pSelf = nullptr;
					//! User callback copied into the deferred job context.
					Func func;

					//! Invokes the stored callback for @a it.
					//! \param it Iterator prepared for the current query batch.
					void operator()(Iter& it) {
						it.ctx(pSelf->ctx());
						func(it);
					}
				};

				//! Callback adapter that materializes typed callback arguments on top of a prepared iterator.
				//! \tparam Func Typed query callback type accepted by each().
				template <typename Func>
				struct TypedJobCallback {
					//! Query that owns typed execution metadata and runtime context.
					QueryImpl* pSelf = nullptr;
					//! User callback copied into the deferred job context.
					Func func;

					//! Runs the typed callback for @a it.
					//! \param it Iterator prepared for the current query batch.
					void operator()(Iter& it) {
						pSelf->each_iter(it, func);
					}
				};

				template <typename Func>
				static void invoke_query_task_job(void* pCtx) {
					auto& ctx = *reinterpret_cast<QueryTaskJobCtx<Func>*>(pCtx);
					ctx.pSelf->each(ctx.func, ctx.execType);
				}

				template <typename Func>
				static void cleanup_query_task_job(void* pCtx) {
					auto* pJobCtx = reinterpret_cast<QueryTaskJobCtx<Func>*>(pCtx);
					if (pJobCtx == nullptr)
						return;
					delete pJobCtx;
				}

				template <typename Func, typename TMode>
				static void cleanup_query_job(void* pCtx) {
					auto* pJobCtx = reinterpret_cast<QueryJobCtx<Func, TMode>*>(pCtx);
					if (pJobCtx == nullptr)
						return;

					auto* pWorld = pJobCtx->pWorld;
					if (pWorld != nullptr) {
						unlock(*pWorld);
						commit_cmd_buffer_st(*pWorld);
						commit_cmd_buffer_mt(*pWorld);
						if (pJobCtx->pSelf != nullptr)
							pJobCtx->pSelf->m_changedWorldVersion = *pJobCtx->pSelf->m_worldVersion;
					}

					delete pJobCtx;
				}

				template <typename Func, typename TMode, QueryExecType ExecType>
				GAIA_NODISCARD SchedJob add_parallel_query_job(Func func) {
					static_assert(ExecType != QueryExecType::Default);
					if (m_batches.empty()) {
						m_changedWorldVersion = *m_worldVersion;
						return {};
					}

					auto* pWorld = m_storage.world();
					lock(*pWorld);

					auto* pCtx = new QueryJobCtx<Func, TMode>{this, pWorld, {}, GAIA_MOV(func)};
					pCtx->batches.resize(m_batches.size());
					GAIA_EACH(m_batches) pCtx->batches[i] = m_batches[i];
					m_batches.clear();

					SchedParDesc desc{};
					desc.pCtx = pCtx;
					desc.itemCount = (uint32_t)pCtx->batches.size();
					desc.groupSize = 0;
					desc.execType = ExecType;
					desc.invoke = [](void* pInvokeCtx, uint32_t idxStart, uint32_t idxEnd) {
						auto& ctx = *reinterpret_cast<QueryJobCtx<Func, TMode>*>(pInvokeCtx);
						run_query_func<Func, TMode>(ctx.pWorld, ctx.func, std::span(&ctx.batches[idxStart], idxEnd - idxStart));
					};

					return sched_add_par(world_sched(*pWorld), desc, pCtx, &cleanup_query_job<Func, TMode>);
				}

				template <bool HasFilters>
				void
				collect_runtime_parallel_batches(const QueryInfo& queryInfo, const QueryPlan& plan, Constraints constraints) {
					auto cacheView = queryInfo.cache_archetype_view();
					const bool hasSortedPlanPayload =
							plan.payloadKind == ExecPayloadKind::NonTrivial && (plan.flags & QueryPlanFlag_Sorted) != 0;
					const auto sortView =
							hasSortedPlanPayload ? queryInfo.cache_sort_view() : decltype(queryInfo.cache_sort_view()){};
					const bool hasInheritedData = (plan.flags & QueryPlanFlag_InheritedPayload) != 0;
					const bool needsBarrierCache = (plan.flags & QueryPlanFlag_BarrierCache) != 0;
					if (needsBarrierCache)
						const_cast<QueryInfo&>(queryInfo).ensure_depth_order_hierarchy_barrier_cache();

					if (!sortView.empty()) {
						for (const auto& view: sortView) {
							const auto* pArchetype = cacheView[view.archetypeIdx];
							const bool barrierPasses = !needsBarrierCache || queryInfo.barrier_passes(view.archetypeIdx);
							if GAIA_UNLIKELY (!can_process_archetype_inter(queryInfo, *pArchetype, constraints, barrierPasses))
								continue;

							const auto viewFrom = view.startRow;
							const auto viewTo = (uint16_t)(view.startRow + view.count);
							uint16_t minStartRow = 0;
							uint16_t minEndRow = 0;
							chunk_effective_range(view.pChunk, constraints, needsBarrierCache, barrierPasses, minStartRow, minEndRow);
							const auto startRow = core::get_max(minStartRow, viewFrom);
							const auto endRow = core::get_min(minEndRow, viewTo);
							if (endRow == startRow)
								continue;

							if constexpr (HasFilters) {
								if (!match_filters(*view.pChunk, queryInfo, m_changedWorldVersion))
									continue;
							}

							auto indicesView = queryInfo.indices_mapping_view(view.archetypeIdx);
							const auto inheritedDataView =
									hasInheritedData ? queryInfo.inherited_data_view(view.archetypeIdx) : InheritedTermDataView{};
							m_batches.push_back(
									{pArchetype, view.pChunk, indicesView.data(), inheritedDataView, 0U, startRow, endRow});
						}
						return;
					}

					for (uint32_t i = plan.idxFrom; i < plan.idxTo; ++i) {
						const auto* pArchetype = cacheView[i];
						const bool barrierPasses = !needsBarrierCache || queryInfo.barrier_passes(i);
						if GAIA_UNLIKELY (!can_process_archetype_inter(queryInfo, *pArchetype, constraints, barrierPasses))
							continue;

						auto indicesView = queryInfo.indices_mapping_view(i);
						const auto inheritedDataView =
								hasInheritedData ? queryInfo.inherited_data_view(i) : InheritedTermDataView{};
						const auto& chunks = pArchetype->chunks();
						for (auto* pChunk: chunks) {
							uint16_t from = 0;
							uint16_t to = 0;
							chunk_effective_range(pChunk, constraints, needsBarrierCache, barrierPasses, from, to);
							if GAIA_UNLIKELY (from == to)
								continue;

							if constexpr (HasFilters) {
								if (!match_filters(*pChunk, queryInfo, m_changedWorldVersion))
									continue;
							}

							m_batches.push_back({pArchetype, pChunk, indicesView.data(), inheritedDataView, 0, from, to});
						}
					}
				}

				template <typename Func>
				GAIA_NODISCARD SchedJob add_query_task_job(Func func, QueryExecType execType) {
					auto* pCtx = new QueryTaskJobCtx<Func>{this, GAIA_MOV(func), execType};

					SchedTaskDesc desc{};
					desc.pCtx = pCtx;
					desc.invoke = &invoke_query_task_job<Func>;
					desc.execType = execType;

					return sched_add(world_sched(*m_storage.world()), desc, pCtx, &cleanup_query_task_job<Func>);
				}

				template <typename Func, QueryExecType ExecType>
				GAIA_NODISCARD SchedJob add_iter_parallel_job(Func func) {
					static_assert(ExecType != QueryExecType::Default);

					auto& queryInfo = fetch();
					match_all(queryInfo);
					const auto constraints = Constraints::EnabledOnly;
					const auto plan = prepare_query_plan(queryInfo, constraints);
					if (plan.mode == QueryPlanMode::Empty || plan.idxFrom >= plan.idxTo)
						return {};
					if (plan.mode == QueryPlanMode::EntitySeed)
						return add_query_task_job(GAIA_MOV(func), ExecType);

					const auto cacheRange = selected_query_cache_range(queryInfo);
					if (cacheRange.hasSelectedGroup)
						return add_query_task_job(GAIA_MOV(func), ExecType);

					::gaia::ecs::update_version(*m_worldVersion);
					m_batches.clear();
					if ((plan.flags & QueryPlanFlag_Filtered) != 0)
						collect_runtime_parallel_batches<true>(queryInfo, plan, constraints);
					else
						collect_runtime_parallel_batches<false>(queryInfo, plan, constraints);

					using JobFunc = IterJobCallback<Func>;
					return add_parallel_query_job<JobFunc, IterModeEnabled, ExecType>(JobFunc{this, GAIA_MOV(func)});
				}

				//------------------------------------------------

				template <bool HasFilters, typename TMode, typename Func>
				void run_query_batch_no_group_id(
						const QueryInfo& queryInfo, const uint32_t idxFrom, const uint32_t idxTo, Func func) {
					GAIA_PROF_SCOPE(query::run_query_batch_no_group_id);

					auto cacheView = queryInfo.cache_archetype_view();
					constexpr auto constraints = iter_mode_constraints<TMode>();
					const auto payloadKind = exec_payload_kind(queryInfo, constraints);
					const bool hasInheritedData = queryInfo.has_inherited_data_payload();
					const bool needsBarrierCache = payloadKind == ExecPayloadKind::NonTrivial &&
																				 needs_depth_order_hierarchy_barrier_cache(queryInfo, constraints);
					const bool hasSortedBatchPayload =
							payloadKind == ExecPayloadKind::NonTrivial && (queryInfo.has_sorted_payload() || needsBarrierCache);
					const auto sortView =
							hasSortedBatchPayload ? queryInfo.cache_sort_view() : decltype(queryInfo.cache_sort_view()){};
					if (needsBarrierCache)
						const_cast<QueryInfo&>(queryInfo).ensure_depth_order_hierarchy_barrier_cache();

					lock(*m_storage.world());

					// We are batching by chunks. Some of them might contain only few items but this state is only
					// temporary because defragmentation runs constantly and keeps things clean.
					ChunkBatchArray chunkBatches;

					if (!sortView.empty()) {
						for (const auto& view: sortView) {
							auto* pArchetype = const_cast<Archetype*>(cacheView[view.archetypeIdx]);
							const bool barrierPasses = !needsBarrierCache || queryInfo.barrier_passes(view.archetypeIdx);
							if GAIA_UNLIKELY (!can_process_archetype_inter(queryInfo, *pArchetype, constraints, barrierPasses))
								continue;

							const auto viewFrom = view.startRow;
							const auto viewTo = (uint16_t)(view.startRow + view.count);
							uint16_t minStartRow = 0;
							uint16_t minEndRow = 0;
							chunk_effective_range(view.pChunk, constraints, needsBarrierCache, barrierPasses, minStartRow, minEndRow);
							const auto startRow = core::get_max(minStartRow, viewFrom);
							const auto endRow = core::get_min(minEndRow, viewTo);
							const auto totalRows = endRow - startRow;
							if (totalRows == 0)
								continue;

							if constexpr (HasFilters) {
								if (!match_filters(*view.pChunk, queryInfo, m_changedWorldVersion))
									continue;
							}

							auto indicesView = queryInfo.indices_mapping_view(view.archetypeIdx);
							const auto inheritedDataView =
									hasInheritedData ? queryInfo.inherited_data_view(view.archetypeIdx) : InheritedTermDataView{};

							chunkBatches.push_back(
									{pArchetype, view.pChunk, indicesView.data(), inheritedDataView, 0U, startRow, endRow});

							if GAIA_UNLIKELY (chunkBatches.size() == chunkBatches.max_size()) {
								run_query_func<Func, TMode>(m_storage.world(), func, {chunkBatches.data(), chunkBatches.size()});
								chunkBatches.clear();
							}
						}
					} else {
						for (uint32_t i = idxFrom; i < idxTo; ++i) {
							auto* pArchetype = const_cast<Archetype*>(cacheView[i]);
							const bool barrierPasses = !needsBarrierCache || queryInfo.barrier_passes(i);
							if GAIA_UNLIKELY (!can_process_archetype_inter(queryInfo, *pArchetype, constraints, barrierPasses))
								continue;

							auto indicesView = queryInfo.indices_mapping_view(i);
							const auto inheritedDataView =
									hasInheritedData ? queryInfo.inherited_data_view(i) : InheritedTermDataView{};
							const auto& chunks = pArchetype->chunks();
							uint32_t chunkOffset = 0;
							uint32_t itemsLeft = chunks.size();
							while (itemsLeft > 0) {
								const auto maxBatchSize = chunkBatches.max_size() - chunkBatches.size();
								const auto batchSize = itemsLeft > maxBatchSize ? maxBatchSize : itemsLeft;

								ChunkSpanMut chunkSpan((Chunk**)&chunks[chunkOffset], batchSize);
								for (auto* pChunk: chunkSpan) {
									uint16_t from = 0;
									uint16_t to = 0;
									chunk_effective_range(pChunk, constraints, needsBarrierCache, barrierPasses, from, to);
									if GAIA_UNLIKELY (from == to)
										continue;

									if constexpr (HasFilters) {
										if (!match_filters(*pChunk, queryInfo, m_changedWorldVersion))
											continue;
									}

									chunkBatches.push_back({pArchetype, pChunk, indicesView.data(), inheritedDataView, 0, from, to});
								}

								if GAIA_UNLIKELY (chunkBatches.size() == chunkBatches.max_size()) {
									run_query_func<Func, TMode>(m_storage.world(), func, {chunkBatches.data(), chunkBatches.size()});
									chunkBatches.clear();
								}

								itemsLeft -= batchSize;
								chunkOffset += batchSize;
							}
						}
					}

					// Take care of any leftovers not processed during run_query
					if (!chunkBatches.empty())
						run_query_func<Func, TMode>(m_storage.world(), func, {chunkBatches.data(), chunkBatches.size()});

					unlock(*m_storage.world());
					// Commit the command buffer.
					// TODO: Smart handling necessary
					commit_cmd_buffer_st(*m_storage.world());
					commit_cmd_buffer_mt(*m_storage.world());
				}

				template <bool HasFilters, typename TMode, typename Func, QueryExecType ExecType>
				void run_query_batch_no_group_id_par(
						const QueryInfo& queryInfo, const uint32_t idxFrom, const uint32_t idxTo, Func func) {
					static_assert(ExecType != QueryExecType::Default);
					GAIA_PROF_SCOPE(query::run_query_batch_no_group_id_par);

					auto cacheView = queryInfo.cache_archetype_view();
					constexpr auto constraints = iter_mode_constraints<TMode>();
					const auto payloadKind = exec_payload_kind(queryInfo, constraints);
					const bool hasInheritedData = queryInfo.has_inherited_data_payload();
					const bool needsBarrierCache = payloadKind == ExecPayloadKind::NonTrivial &&
																				 needs_depth_order_hierarchy_barrier_cache(queryInfo, constraints);
					const bool hasSortedBatchPayload =
							payloadKind == ExecPayloadKind::NonTrivial && (queryInfo.has_sorted_payload() || needsBarrierCache);
					const auto sortView =
							hasSortedBatchPayload ? queryInfo.cache_sort_view() : decltype(queryInfo.cache_sort_view()){};
					if (needsBarrierCache)
						const_cast<QueryInfo&>(queryInfo).ensure_depth_order_hierarchy_barrier_cache();

					if (!sortView.empty()) {
						for (const auto& view: sortView) {
							const auto* pArchetype = cacheView[view.archetypeIdx];
							const bool barrierPasses = !needsBarrierCache || queryInfo.barrier_passes(view.archetypeIdx);
							if GAIA_UNLIKELY (!can_process_archetype_inter(queryInfo, *pArchetype, constraints, barrierPasses))
								continue;

							const auto viewFrom = view.startRow;
							const auto viewTo = (uint16_t)(view.startRow + view.count);
							uint16_t minStartRow = 0;
							uint16_t minEndRow = 0;
							chunk_effective_range(view.pChunk, constraints, needsBarrierCache, barrierPasses, minStartRow, minEndRow);
							const auto startRow = core::get_max(minStartRow, viewFrom);
							const auto endRow = core::get_min(minEndRow, viewTo);
							const auto totalRows = endRow - startRow;
							if (totalRows == 0)
								continue;

							if constexpr (HasFilters) {
								if (!match_filters(*view.pChunk, queryInfo, m_changedWorldVersion))
									continue;
							}

							auto indicesView = queryInfo.indices_mapping_view(view.archetypeIdx);
							const auto inheritedDataView =
									hasInheritedData ? queryInfo.inherited_data_view(view.archetypeIdx) : InheritedTermDataView{};

							m_batches.push_back(
									{pArchetype, view.pChunk, indicesView.data(), inheritedDataView, 0U, startRow, endRow});
						}
					} else {
						for (uint32_t i = idxFrom; i < idxTo; ++i) {
							const auto* pArchetype = cacheView[i];
							const bool barrierPasses = !needsBarrierCache || queryInfo.barrier_passes(i);
							if GAIA_UNLIKELY (!can_process_archetype_inter(queryInfo, *pArchetype, constraints, barrierPasses))
								continue;

							auto indicesView = queryInfo.indices_mapping_view(i);
							const auto inheritedDataView =
									hasInheritedData ? queryInfo.inherited_data_view(i) : InheritedTermDataView{};
							const auto& chunks = pArchetype->chunks();
							for (auto* pChunk: chunks) {
								uint16_t from = 0;
								uint16_t to = 0;
								chunk_effective_range(pChunk, constraints, needsBarrierCache, barrierPasses, from, to);
								if GAIA_UNLIKELY (from == to)
									continue;

								if constexpr (HasFilters) {
									if (!match_filters(*pChunk, queryInfo, m_changedWorldVersion))
										continue;
								}

								m_batches.push_back({pArchetype, pChunk, indicesView.data(), inheritedDataView, 0, from, to});
							}
						}
					}

					if (m_batches.empty())
						return;

					lock(*m_storage.world());

					struct ParallelQueryBatchCtx {
						QueryImpl* pSelf;
						Func* pFunc;
					};
					ParallelQueryBatchCtx ctx{this, &func};
					SchedParDesc desc{};
					desc.pCtx = &ctx;
					desc.itemCount = (uint32_t)m_batches.size();
					desc.groupSize = 0;
					desc.execType = ExecType;
					desc.invoke = [](void* pCtx, uint32_t idxStart, uint32_t idxEnd) {
						auto& ctx = *reinterpret_cast<ParallelQueryBatchCtx*>(pCtx);
						run_query_func<Func, TMode>(
								ctx.pSelf->m_storage.world(), *ctx.pFunc,
								std::span(&ctx.pSelf->m_batches[idxStart], idxEnd - idxStart));
					};

					const auto& sched = world_sched(*m_storage.world());
					const auto token = sched_par(sched, desc);
					sched_wait(sched, token);
					sched_del(sched, token);
					m_batches.clear();

					unlock(*m_storage.world());
					// Commit the command buffer.
					// TODO: Smart handling necessary
					commit_cmd_buffer_st(*m_storage.world());
					commit_cmd_buffer_mt(*m_storage.world());
				}

				template <bool HasFilters, typename TMode, typename Func>
				void run_query_batch_with_group_id(
						const QueryInfo& queryInfo, const uint32_t idxFrom, const uint32_t idxTo, Func func) {
					GAIA_PROF_SCOPE(query::run_query_batch_with_group_id);

					ChunkBatchArray chunkBatches;

					auto cacheView = queryInfo.cache_archetype_view();
					const bool hasInheritedData = queryInfo.has_inherited_data_payload();
					constexpr auto constraints = iter_mode_constraints<TMode>();
					const auto payloadKind = exec_payload_kind(queryInfo, constraints);
					const bool needsBarrierCache = payloadKind == ExecPayloadKind::NonTrivial &&
																				 needs_depth_order_hierarchy_barrier_cache(queryInfo, constraints);
					if (needsBarrierCache)
						const_cast<QueryInfo&>(queryInfo).ensure_depth_order_hierarchy_barrier_cache();

					lock(*m_storage.world());

					for (uint32_t i = idxFrom; i < idxTo; ++i) {
						const auto* pArchetype = cacheView[i];
						const bool barrierPasses = !needsBarrierCache || queryInfo.barrier_passes(i);
						if GAIA_UNLIKELY (!can_process_archetype_inter(queryInfo, *pArchetype, constraints, barrierPasses))
							continue;

						auto indicesView = queryInfo.indices_mapping_view(i);
						const auto inheritedDataView =
								hasInheritedData ? queryInfo.inherited_data_view(i) : InheritedTermDataView{};
						const auto& chunks = pArchetype->chunks();
						const auto groupId = queryInfo.group_id(i);

#if GAIA_ASSERT_ENABLED
						GAIA_ASSERT(
								// ... or no groupId is set...
								m_groupIdSet == 0 ||
								// ... or the groupId must match the requested one
								groupId == m_groupIdSet);
#endif

						uint32_t chunkOffset = 0;
						uint32_t itemsLeft = chunks.size();
						while (itemsLeft > 0) {
							const auto maxBatchSize = chunkBatches.max_size() - chunkBatches.size();
							const auto batchSize = itemsLeft > maxBatchSize ? maxBatchSize : itemsLeft;

							ChunkSpanMut chunkSpan((Chunk**)&chunks[chunkOffset], batchSize);
							for (auto* pChunk: chunkSpan) {
								uint16_t from = 0;
								uint16_t to = 0;
								chunk_effective_range(pChunk, constraints, needsBarrierCache, barrierPasses, from, to);
								if GAIA_UNLIKELY (from == to)
									continue;

								if constexpr (HasFilters) {
									if (!match_filters(*pChunk, queryInfo, m_changedWorldVersion))
										continue;
								}

								chunkBatches.push_back({pArchetype, pChunk, indicesView.data(), inheritedDataView, groupId, from, to});
							}

							if GAIA_UNLIKELY (chunkBatches.size() == chunkBatches.max_size()) {
								run_query_func<Func, TMode>(m_storage.world(), func, {chunkBatches.data(), chunkBatches.size()});
								chunkBatches.clear();
							}

							itemsLeft -= batchSize;
							chunkOffset += batchSize;
						}
					}

					// Take care of any leftovers not processed during run_query
					if (!chunkBatches.empty())
						run_query_func<Func, TMode>(m_storage.world(), func, {chunkBatches.data(), chunkBatches.size()});

					unlock(*m_storage.world());
					// Commit the command buffer.
					// TODO: Smart handling necessary
					commit_cmd_buffer_st(*m_storage.world());
					commit_cmd_buffer_mt(*m_storage.world());
				}

				template <bool HasFilters, typename TMode, typename Func, QueryExecType ExecType>
				void run_query_batch_with_group_id_par(
						const QueryInfo& queryInfo, const uint32_t idxFrom, const uint32_t idxTo, Func func) {
					static_assert(ExecType != QueryExecType::Default);
					GAIA_PROF_SCOPE(query::run_query_batch_with_group_id_par);

					ChunkBatchArray chunkBatch;

					auto cacheView = queryInfo.cache_archetype_view();
					const bool hasInheritedData = queryInfo.has_inherited_data_payload();
					constexpr auto constraints = iter_mode_constraints<TMode>();
					const auto payloadKind = exec_payload_kind(queryInfo, constraints);
					const bool needsBarrierCache = payloadKind == ExecPayloadKind::NonTrivial &&
																				 needs_depth_order_hierarchy_barrier_cache(queryInfo, constraints);
					if (needsBarrierCache)
						const_cast<QueryInfo&>(queryInfo).ensure_depth_order_hierarchy_barrier_cache();

#if GAIA_ASSERT_ENABLED
					for (uint32_t i = idxFrom; i < idxTo; ++i) {
						const auto* pArchetype = cacheView[i];
						const bool barrierPasses = !needsBarrierCache || queryInfo.barrier_passes(i);
						if GAIA_UNLIKELY (!can_process_archetype_inter(queryInfo, *pArchetype, constraints, barrierPasses))
							continue;

						const auto groupId = queryInfo.group_id(i);
						GAIA_ASSERT(
								// ... or no groupId is set...
								m_groupIdSet == 0 ||
								// ... or the groupId must match the requested one
								groupId == m_groupIdSet);
					}
#endif

					for (uint32_t i = idxFrom; i < idxTo; ++i) {
						const Archetype* pArchetype = cacheView[i];
						const bool barrierPasses = !needsBarrierCache || queryInfo.barrier_passes(i);
						if GAIA_UNLIKELY (!can_process_archetype_inter(queryInfo, *pArchetype, constraints, barrierPasses))
							continue;

						auto indicesView = queryInfo.indices_mapping_view(i);
						const auto inheritedDataView =
								hasInheritedData ? queryInfo.inherited_data_view(i) : InheritedTermDataView{};
						const auto groupId = queryInfo.group_id(i);
						const auto& chunks = pArchetype->chunks();
						for (auto* pChunk: chunks) {
							uint16_t from = 0;
							uint16_t to = 0;
							chunk_effective_range(pChunk, constraints, needsBarrierCache, barrierPasses, from, to);
							if GAIA_UNLIKELY (from == to)
								continue;

							if constexpr (HasFilters) {
								if (!match_filters(*pChunk, queryInfo, m_changedWorldVersion))
									continue;
							}

							m_batches.push_back({pArchetype, pChunk, indicesView.data(), inheritedDataView, groupId, from, to});
						}
					}

					if (m_batches.empty())
						return;

					lock(*m_storage.world());

					struct ParallelQueryBatchCtx {
						QueryImpl* pSelf;
						Func* pFunc;
					};
					ParallelQueryBatchCtx ctx{this, &func};
					SchedParDesc desc{};
					desc.pCtx = &ctx;
					desc.itemCount = (uint32_t)m_batches.size();
					desc.groupSize = 0;
					desc.execType = ExecType;
					desc.invoke = [](void* pCtx, uint32_t idxStart, uint32_t idxEnd) {
						auto& ctx = *reinterpret_cast<ParallelQueryBatchCtx*>(pCtx);
						run_query_func<Func, TMode>(
								ctx.pSelf->m_storage.world(), *ctx.pFunc,
								std::span(&ctx.pSelf->m_batches[idxStart], idxEnd - idxStart));
					};

					const auto& sched = world_sched(*m_storage.world());
					const auto token = sched_par(sched, desc);
					sched_wait(sched, token);
					sched_del(sched, token);
					m_batches.clear();

					unlock(*m_storage.world());
					// Commit the command buffer.
					// TODO: Smart handling necessary
					commit_cmd_buffer_st(*m_storage.world());
					commit_cmd_buffer_mt(*m_storage.world());
				}

				//------------------------------------------------

				template <bool HasFilters, QueryExecType ExecType, typename TMode, typename Func>
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

					const auto cacheRange = selected_query_cache_range(queryInfo);
					if (!cacheRange.hasSelectedGroup) {
						// No group requested or group filtering is currently turned off
						if constexpr (ExecType != QueryExecType::Default)
							run_query_batch_no_group_id_par<HasFilters, TMode, Func, ExecType>(
									queryInfo, cacheRange.idxFrom, cacheRange.idxTo, func);
						else
							run_query_batch_no_group_id<HasFilters, TMode, Func>(
									queryInfo, cacheRange.idxFrom, cacheRange.idxTo, func);
					} else {
						// We wish to iterate only a certain group
						if (!cacheRange.valid)
							return;

						if constexpr (ExecType != QueryExecType::Default)
							run_query_batch_with_group_id_par<HasFilters, TMode, Func, ExecType>(
									queryInfo, cacheRange.idxFrom, cacheRange.idxTo, func);
						else
							run_query_batch_with_group_id<HasFilters, TMode, Func>(
									queryInfo, cacheRange.idxFrom, cacheRange.idxTo, func);
					}
				}

				//------------------------------------------------

				template <QueryExecType ExecType, typename Func>
				void run_query_on_archetypes(QueryInfo& queryInfo, Func func, Constraints constraints) {
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
						const auto payloadKind = exec_payload_kind(queryInfo, constraints);
						const bool needsBarrierCache = payloadKind == ExecPayloadKind::NonTrivial &&
																					 needs_depth_order_hierarchy_barrier_cache(queryInfo, constraints);
						const bool hasInheritedData = queryInfo.has_inherited_data_payload();
						if (needsBarrierCache)
							queryInfo.ensure_depth_order_hierarchy_barrier_cache();
						GAIA_EACH(cache_view) {
							const auto* pArchetype = cache_view[i];
							const bool barrierPasses = !needsBarrierCache || queryInfo.barrier_passes(i);
							if GAIA_UNLIKELY (!can_process_archetype_inter(queryInfo, *pArchetype, constraints, barrierPasses))
								continue;

							auto indicesView = queryInfo.indices_mapping_view(i);
							const auto inheritedDataView =
									hasInheritedData ? queryInfo.inherited_data_view(i) : InheritedTermDataView{};
							ChunkBatch batch{pArchetype, nullptr, indicesView.data(), inheritedDataView, 0, 0, 0};
							run_query_arch_func(m_storage.world(), func, batch, constraints);
						}
					}

					unlock(*m_storage.world());
					// Changed-filter state is instance-local for cached queries.
				}

				//------------------------------------------------

				template <QueryExecType ExecType, typename TMode, typename Func>
				void run_query_on_chunks(QueryInfo& queryInfo, Func func) {
					// Update the world version
					::gaia::ecs::update_version(*m_worldVersion);

					const bool hasFilters = queryInfo.has_filters();
					if (hasFilters)
						run_query<true, ExecType, TMode>(queryInfo, func);
					else
						run_query<false, ExecType, TMode>(queryInfo, func);

					// Changed-filter state is instance-local for cached queries.
					m_changedWorldVersion = *m_worldVersion;
				}

				template <typename Func>
				static void
				run_query_func_runtime(World* pWorld, Func func, std::span<ChunkBatch> batches, Constraints constraints) {
					GAIA_PROF_SCOPE(query::run_query_func);

					const auto chunkCnt = batches.size();
					GAIA_ASSERT(chunkCnt > 0);

					Iter it;
					it.init_query_state(pWorld, constraints, false);

					const Archetype* pLastArchetype = nullptr;
					const uint8_t* pLastIndices = nullptr;
					InheritedTermDataView lastInheritedData{};
					GroupId lastGroupId = GroupIdMax;

					const auto apply_batch = [&](const ChunkBatch& batch) {
						if (batch.pArchetype != pLastArchetype) {
							it.set_archetype(batch.pArchetype);
							pLastArchetype = batch.pArchetype;
						}

						if (batch.pCompIndices != pLastIndices) {
							it.set_comp_indices(batch.pCompIndices);
							pLastIndices = batch.pCompIndices;
						}

						if (batch.inheritedData.data() != lastInheritedData.data()) {
							it.set_inherited_data(batch.inheritedData);
							lastInheritedData = batch.inheritedData;
						}

						if (batch.groupId != lastGroupId) {
							it.set_group_id(batch.groupId);
							lastGroupId = batch.groupId;
						}

						it.set_chunk(batch.pChunk, batch.from, batch.to);
						func(it);
						finish_iter_writes(it);
						it.clear_touched_writes();
					};

					if GAIA_UNLIKELY (chunkCnt == 1) {
						apply_batch(batches[0]);
						return;
					}

					gaia::prefetch(batches[1].pChunk, PrefetchHint::PREFETCH_HINT_T2);
					apply_batch(batches[0]);

					uint32_t chunkIdx = 1;
					for (; chunkIdx < chunkCnt - 1; ++chunkIdx) {
						gaia::prefetch(batches[chunkIdx + 1].pChunk, PrefetchHint::PREFETCH_HINT_T2);
						apply_batch(batches[chunkIdx]);
					}

					apply_batch(batches[chunkIdx]);
				}

				template <bool HasFilters, typename Func>
				void run_query_batch_no_group_id_runtime(
						const QueryInfo& queryInfo, const QueryPlan& plan, Constraints constraints, Func func) {
					GAIA_PROF_SCOPE(query::run_query_batch_no_group_id);

					auto cacheView = queryInfo.cache_archetype_view();
					const bool hasSortedPlanPayload =
							plan.payloadKind == ExecPayloadKind::NonTrivial && (plan.flags & QueryPlanFlag_Sorted) != 0;
					const auto sortView =
							hasSortedPlanPayload ? queryInfo.cache_sort_view() : decltype(queryInfo.cache_sort_view()){};
					const bool hasInheritedData = (plan.flags & QueryPlanFlag_InheritedPayload) != 0;
					const bool needsBarrierCache = (plan.flags & QueryPlanFlag_BarrierCache) != 0;
					if (needsBarrierCache)
						const_cast<QueryInfo&>(queryInfo).ensure_depth_order_hierarchy_barrier_cache();

					lock(*m_storage.world());
					ChunkBatchArray chunkBatches;

					if (!sortView.empty()) {
						for (const auto& view: sortView) {
							auto* pArchetype = const_cast<Archetype*>(cacheView[view.archetypeIdx]);
							const bool barrierPasses = !needsBarrierCache || queryInfo.barrier_passes(view.archetypeIdx);
							if GAIA_UNLIKELY (!can_process_archetype_inter(queryInfo, *pArchetype, constraints, barrierPasses))
								continue;

							const auto viewFrom = view.startRow;
							const auto viewTo = (uint16_t)(view.startRow + view.count);
							uint16_t minStartRow = 0;
							uint16_t minEndRow = 0;
							chunk_effective_range(view.pChunk, constraints, needsBarrierCache, barrierPasses, minStartRow, minEndRow);
							const auto startRow = core::get_max(minStartRow, viewFrom);
							const auto endRow = core::get_min(minEndRow, viewTo);
							const auto totalRows = endRow - startRow;
							if (totalRows == 0)
								continue;

							if constexpr (HasFilters) {
								if (!match_filters(*view.pChunk, queryInfo, m_changedWorldVersion))
									continue;
							}

							auto indicesView = queryInfo.indices_mapping_view(view.archetypeIdx);
							const auto inheritedDataView =
									hasInheritedData ? queryInfo.inherited_data_view(view.archetypeIdx) : InheritedTermDataView{};

							chunkBatches.push_back(
									{pArchetype, view.pChunk, indicesView.data(), inheritedDataView, 0U, startRow, endRow});

							if GAIA_UNLIKELY (chunkBatches.size() == chunkBatches.max_size()) {
								run_query_func_runtime(
										m_storage.world(), func, {chunkBatches.data(), chunkBatches.size()}, constraints);
								chunkBatches.clear();
							}
						}
					} else {
						for (uint32_t i = plan.idxFrom; i < plan.idxTo; ++i) {
							auto* pArchetype = const_cast<Archetype*>(cacheView[i]);
							const bool barrierPasses = !needsBarrierCache || queryInfo.barrier_passes(i);
							if GAIA_UNLIKELY (!can_process_archetype_inter(queryInfo, *pArchetype, constraints, barrierPasses))
								continue;

							auto indicesView = queryInfo.indices_mapping_view(i);
							const auto inheritedDataView =
									hasInheritedData ? queryInfo.inherited_data_view(i) : InheritedTermDataView{};
							const auto& chunks = pArchetype->chunks();
							uint32_t chunkOffset = 0;
							uint32_t itemsLeft = chunks.size();
							while (itemsLeft > 0) {
								const auto maxBatchSize = chunkBatches.max_size() - chunkBatches.size();
								const auto batchSize = itemsLeft > maxBatchSize ? maxBatchSize : itemsLeft;

								ChunkSpanMut chunkSpan((Chunk**)&chunks[chunkOffset], batchSize);
								for (auto* pChunk: chunkSpan) {
									uint16_t from = 0;
									uint16_t to = 0;
									chunk_effective_range(pChunk, constraints, needsBarrierCache, barrierPasses, from, to);
									if GAIA_UNLIKELY (from == to)
										continue;

									if constexpr (HasFilters) {
										if (!match_filters(*pChunk, queryInfo, m_changedWorldVersion))
											continue;
									}

									chunkBatches.push_back({pArchetype, pChunk, indicesView.data(), inheritedDataView, 0, from, to});
								}

								if GAIA_UNLIKELY (chunkBatches.size() == chunkBatches.max_size()) {
									run_query_func_runtime(
											m_storage.world(), func, {chunkBatches.data(), chunkBatches.size()}, constraints);
									chunkBatches.clear();
								}

								itemsLeft -= batchSize;
								chunkOffset += batchSize;
							}
						}
					}

					if (!chunkBatches.empty())
						run_query_func_runtime(m_storage.world(), func, {chunkBatches.data(), chunkBatches.size()}, constraints);

					unlock(*m_storage.world());
					commit_cmd_buffer_st(*m_storage.world());
					commit_cmd_buffer_mt(*m_storage.world());
				}

				template <bool HasFilters, typename Func, QueryExecType ExecType>
				void run_query_batch_no_group_id_runtime_par(
						const QueryInfo& queryInfo, const QueryPlan& plan, Constraints constraints, Func func) {
					static_assert(ExecType != QueryExecType::Default);
					GAIA_PROF_SCOPE(query::run_query_batch_no_group_id_par);

					auto cacheView = queryInfo.cache_archetype_view();
					const bool hasSortedPlanPayload =
							plan.payloadKind == ExecPayloadKind::NonTrivial && (plan.flags & QueryPlanFlag_Sorted) != 0;
					const auto sortView =
							hasSortedPlanPayload ? queryInfo.cache_sort_view() : decltype(queryInfo.cache_sort_view()){};
					const bool hasInheritedData = (plan.flags & QueryPlanFlag_InheritedPayload) != 0;
					const bool needsBarrierCache = (plan.flags & QueryPlanFlag_BarrierCache) != 0;
					if (needsBarrierCache)
						const_cast<QueryInfo&>(queryInfo).ensure_depth_order_hierarchy_barrier_cache();

					if (!sortView.empty()) {
						for (const auto& view: sortView) {
							const auto* pArchetype = cacheView[view.archetypeIdx];
							const bool barrierPasses = !needsBarrierCache || queryInfo.barrier_passes(view.archetypeIdx);
							if GAIA_UNLIKELY (!can_process_archetype_inter(queryInfo, *pArchetype, constraints, barrierPasses))
								continue;

							const auto viewFrom = view.startRow;
							const auto viewTo = (uint16_t)(view.startRow + view.count);
							uint16_t minStartRow = 0;
							uint16_t minEndRow = 0;
							chunk_effective_range(view.pChunk, constraints, needsBarrierCache, barrierPasses, minStartRow, minEndRow);
							const auto startRow = core::get_max(minStartRow, viewFrom);
							const auto endRow = core::get_min(minEndRow, viewTo);
							const auto totalRows = endRow - startRow;
							if (totalRows == 0)
								continue;

							if constexpr (HasFilters) {
								if (!match_filters(*view.pChunk, queryInfo, m_changedWorldVersion))
									continue;
							}

							auto indicesView = queryInfo.indices_mapping_view(view.archetypeIdx);
							const auto inheritedDataView =
									hasInheritedData ? queryInfo.inherited_data_view(view.archetypeIdx) : InheritedTermDataView{};

							m_batches.push_back(
									{pArchetype, view.pChunk, indicesView.data(), inheritedDataView, 0U, startRow, endRow});
						}
					} else {
						for (uint32_t i = plan.idxFrom; i < plan.idxTo; ++i) {
							const auto* pArchetype = cacheView[i];
							const bool barrierPasses = !needsBarrierCache || queryInfo.barrier_passes(i);
							if GAIA_UNLIKELY (!can_process_archetype_inter(queryInfo, *pArchetype, constraints, barrierPasses))
								continue;

							auto indicesView = queryInfo.indices_mapping_view(i);
							const auto inheritedDataView =
									hasInheritedData ? queryInfo.inherited_data_view(i) : InheritedTermDataView{};
							const auto& chunks = pArchetype->chunks();
							for (auto* pChunk: chunks) {
								uint16_t from = 0;
								uint16_t to = 0;
								chunk_effective_range(pChunk, constraints, needsBarrierCache, barrierPasses, from, to);
								if GAIA_UNLIKELY (from == to)
									continue;

								if constexpr (HasFilters) {
									if (!match_filters(*pChunk, queryInfo, m_changedWorldVersion))
										continue;
								}

								m_batches.push_back({pArchetype, pChunk, indicesView.data(), inheritedDataView, 0, from, to});
							}
						}
					}

					if (m_batches.empty())
						return;

					lock(*m_storage.world());

					struct ParallelQueryBatchCtx {
						QueryImpl* pSelf;
						Func* pFunc;
						Constraints constraints;
					};
					ParallelQueryBatchCtx ctx{this, &func, constraints};
					SchedParDesc desc{};
					desc.pCtx = &ctx;
					desc.itemCount = (uint32_t)m_batches.size();
					desc.groupSize = 0;
					desc.execType = ExecType;
					desc.invoke = [](void* pCtx, uint32_t idxStart, uint32_t idxEnd) {
						auto& ctx = *reinterpret_cast<ParallelQueryBatchCtx*>(pCtx);
						run_query_func_runtime(
								ctx.pSelf->m_storage.world(), *ctx.pFunc, std::span(&ctx.pSelf->m_batches[idxStart], idxEnd - idxStart),
								ctx.constraints);
					};

					const auto& sched = world_sched(*m_storage.world());
					const auto token = sched_par(sched, desc);
					sched_wait(sched, token);
					sched_del(sched, token);
					m_batches.clear();

					unlock(*m_storage.world());
					commit_cmd_buffer_st(*m_storage.world());
					commit_cmd_buffer_mt(*m_storage.world());
				}

				template <bool HasFilters, typename Func>
				void run_query_batch_with_group_id_runtime(
						const QueryInfo& queryInfo, const QueryPlan& plan, Constraints constraints, Func func) {
					GAIA_PROF_SCOPE(query::run_query_batch_with_group_id);

					ChunkBatchArray chunkBatches;
					auto cacheView = queryInfo.cache_archetype_view();
					const bool hasInheritedData = (plan.flags & QueryPlanFlag_InheritedPayload) != 0;
					const bool needsBarrierCache = (plan.flags & QueryPlanFlag_BarrierCache) != 0;
					if (needsBarrierCache)
						const_cast<QueryInfo&>(queryInfo).ensure_depth_order_hierarchy_barrier_cache();

					lock(*m_storage.world());

					for (uint32_t i = plan.idxFrom; i < plan.idxTo; ++i) {
						const auto* pArchetype = cacheView[i];
						const bool barrierPasses = !needsBarrierCache || queryInfo.barrier_passes(i);
						if GAIA_UNLIKELY (!can_process_archetype_inter(queryInfo, *pArchetype, constraints, barrierPasses))
							continue;

						auto indicesView = queryInfo.indices_mapping_view(i);
						const auto inheritedDataView =
								hasInheritedData ? queryInfo.inherited_data_view(i) : InheritedTermDataView{};
						const auto& chunks = pArchetype->chunks();
						const auto groupId = queryInfo.group_id(i);

						uint32_t chunkOffset = 0;
						uint32_t itemsLeft = chunks.size();
						while (itemsLeft > 0) {
							const auto maxBatchSize = chunkBatches.max_size() - chunkBatches.size();
							const auto batchSize = itemsLeft > maxBatchSize ? maxBatchSize : itemsLeft;

							ChunkSpanMut chunkSpan((Chunk**)&chunks[chunkOffset], batchSize);
							for (auto* pChunk: chunkSpan) {
								uint16_t from = 0;
								uint16_t to = 0;
								chunk_effective_range(pChunk, constraints, needsBarrierCache, barrierPasses, from, to);
								if GAIA_UNLIKELY (from == to)
									continue;

								if constexpr (HasFilters) {
									if (!match_filters(*pChunk, queryInfo, m_changedWorldVersion))
										continue;
								}

								chunkBatches.push_back({pArchetype, pChunk, indicesView.data(), inheritedDataView, groupId, from, to});
							}

							if GAIA_UNLIKELY (chunkBatches.size() == chunkBatches.max_size()) {
								run_query_func_runtime(
										m_storage.world(), func, {chunkBatches.data(), chunkBatches.size()}, constraints);
								chunkBatches.clear();
							}

							itemsLeft -= batchSize;
							chunkOffset += batchSize;
						}
					}

					if (!chunkBatches.empty())
						run_query_func_runtime(m_storage.world(), func, {chunkBatches.data(), chunkBatches.size()}, constraints);

					unlock(*m_storage.world());
					commit_cmd_buffer_st(*m_storage.world());
					commit_cmd_buffer_mt(*m_storage.world());
				}

				template <bool HasFilters, typename Func, QueryExecType ExecType>
				void run_query_batch_with_group_id_runtime_par(
						const QueryInfo& queryInfo, const QueryPlan& plan, Constraints constraints, Func func) {
					static_assert(ExecType != QueryExecType::Default);
					GAIA_PROF_SCOPE(query::run_query_batch_with_group_id_par);

					ChunkBatchArray chunkBatch;
					auto cacheView = queryInfo.cache_archetype_view();
					const bool hasInheritedData = (plan.flags & QueryPlanFlag_InheritedPayload) != 0;
					const bool needsBarrierCache = (plan.flags & QueryPlanFlag_BarrierCache) != 0;
					if (needsBarrierCache)
						const_cast<QueryInfo&>(queryInfo).ensure_depth_order_hierarchy_barrier_cache();

					for (uint32_t i = plan.idxFrom; i < plan.idxTo; ++i) {
						const auto* pArchetype = cacheView[i];
						const bool barrierPasses = !needsBarrierCache || queryInfo.barrier_passes(i);
						if GAIA_UNLIKELY (!can_process_archetype_inter(queryInfo, *pArchetype, constraints, barrierPasses))
							continue;

						auto indicesView = queryInfo.indices_mapping_view(i);
						const auto inheritedDataView =
								hasInheritedData ? queryInfo.inherited_data_view(i) : InheritedTermDataView{};
						const auto groupId = queryInfo.group_id(i);
						const auto& chunks = pArchetype->chunks();
						for (auto* pChunk: chunks) {
							uint16_t from = 0;
							uint16_t to = 0;
							chunk_effective_range(pChunk, constraints, needsBarrierCache, barrierPasses, from, to);
							if GAIA_UNLIKELY (from == to)
								continue;

							if constexpr (HasFilters) {
								if (!match_filters(*pChunk, queryInfo, m_changedWorldVersion))
									continue;
							}

							m_batches.push_back({pArchetype, pChunk, indicesView.data(), inheritedDataView, groupId, from, to});
						}
					}

					if (m_batches.empty())
						return;

					lock(*m_storage.world());

					struct ParallelQueryBatchCtx {
						QueryImpl* pSelf;
						Func* pFunc;
						Constraints constraints;
					};
					ParallelQueryBatchCtx ctx{this, &func, constraints};
					SchedParDesc desc{};
					desc.pCtx = &ctx;
					desc.itemCount = (uint32_t)m_batches.size();
					desc.groupSize = 0;
					desc.execType = ExecType;
					desc.invoke = [](void* pCtx, uint32_t idxStart, uint32_t idxEnd) {
						auto& ctx = *reinterpret_cast<ParallelQueryBatchCtx*>(pCtx);
						run_query_func_runtime(
								ctx.pSelf->m_storage.world(), *ctx.pFunc, std::span(&ctx.pSelf->m_batches[idxStart], idxEnd - idxStart),
								ctx.constraints);
					};

					const auto& sched = world_sched(*m_storage.world());
					const auto token = sched_par(sched, desc);
					sched_wait(sched, token);
					sched_del(sched, token);
					m_batches.clear();

					unlock(*m_storage.world());
					commit_cmd_buffer_st(*m_storage.world());
					commit_cmd_buffer_mt(*m_storage.world());
				}

				template <bool HasFilters, QueryExecType ExecType, typename Func>
				void run_query_runtime_planned(
						const QueryInfo& queryInfo, const QueryPlan& plan, Constraints constraints, Func func) {
					GAIA_PROF_SCOPE(query::run_query);
					if (plan.mode == QueryPlanMode::Empty || plan.idxFrom >= plan.idxTo)
						return;

					const auto cacheRange = selected_query_cache_range(queryInfo);
					if (!cacheRange.hasSelectedGroup) {
						if constexpr (ExecType != QueryExecType::Default)
							run_query_batch_no_group_id_runtime_par<HasFilters, Func, ExecType>(queryInfo, plan, constraints, func);
						else
							run_query_batch_no_group_id_runtime<HasFilters>(queryInfo, plan, constraints, func);
					} else {
						if constexpr (ExecType != QueryExecType::Default)
							run_query_batch_with_group_id_runtime_par<HasFilters, Func, ExecType>(queryInfo, plan, constraints, func);
						else
							run_query_batch_with_group_id_runtime<HasFilters>(queryInfo, plan, constraints, func);
					}
				}

				//! Runs public iterator chunk execution from an already prepared plan.
				//! \tparam ExecType Query execution mode requested by the public API.
				//! \tparam Func Public iterator callback wrapper type.
				//! \param queryInfo Prepared query cache and execution metadata.
				//! \param plan Prepared query execution metadata.
				//! \param constraints Entity-row subset exposed to the callback.
				//! \param func Callback wrapper invoked for each iterator step.
				template <QueryExecType ExecType, typename Func>
				void run_query_on_chunks_runtime_planned(
						QueryInfo& queryInfo, const QueryPlan& plan, Constraints constraints, Func func) {
					if (plan.mode == QueryPlanMode::Empty)
						return;

					::gaia::ecs::update_version(*m_worldVersion);
					if ((plan.flags & QueryPlanFlag_Filtered) != 0)
						run_query_runtime_planned<true, ExecType>(queryInfo, plan, constraints, func);
					else
						run_query_runtime_planned<false, ExecType>(queryInfo, plan, constraints, func);

					m_changedWorldVersion = *m_worldVersion;
				}

				//! Checks whether typed callbacks can use dense chunk iteration while preserving required cache ordering.
				//! \param queryInfo Prepared query cache and execution metadata.
				//! \return True when direct chunk iteration preserves required ordering and hierarchy-barrier pruning
				//! semantics.
				GAIA_NODISCARD bool can_use_direct_chunk_iteration_fastpath(const QueryInfo& queryInfo) const {
					const auto& data = queryInfo.ctx().data;
					return data.sortByFunc == nullptr &&
								 (!has_depth_order_hierarchy_enabled_barrier(queryInfo) || !queryInfo.barrier_may_prune());
				}

				//! Selects the cache range visible to this query, applying a selected group id when present.
				//! \param queryInfo Prepared query cache and execution metadata.
				//! \return Cache range metadata; `valid == false` means the selected group is absent.
				GAIA_NODISCARD QueryCacheRange selected_query_cache_range(const QueryInfo& queryInfo) const {
					QueryCacheRange range{};
					range.idxTo = (uint32_t)queryInfo.cache_archetype_view().size();

					const auto& data = queryInfo.ctx().data;
					if (data.groupBy == EntityBad || m_groupIdSet == 0)
						return range;

					range.hasSelectedGroup = true;
					const auto* pGroupData = queryInfo.selected_group_data(m_groupIdSet);
					if (pGroupData == nullptr) {
						range.idxFrom = 0;
						range.idxTo = 0;
						range.valid = false;
						return range;
					}

					range.idxFrom = pGroupData->idxFirst;
					range.idxTo = pGroupData->idxLast + 1;
					return range;
				}

				//! Selects the prepared execution plan for typed callbacks.
				//! \param queryInfo Prepared query cache and execution metadata.
				//! \param state Typed callback execution metadata derived from callback arguments.
				//! \return Query execution plan shared by typed and public iterator adapters.
				GAIA_NODISCARD QueryPlan prepare_query_plan(const QueryInfo& queryInfo, const TypedQueryExecState& state) const;

				template <bool HasFilters, typename Func, typename... T>
				void run_query_on_chunks_direct_typed(
						QueryInfo& queryInfo, const QueryPlan& plan, const TypedQueryExecState& state, Func& func,
						core::func_type_list<T...>);

				void run_query_on_chunks_direct(
						QueryInfo& queryInfo, const QueryPlan& plan, const TypedQueryExecState& state, void* pFunc,
						void (*runChunk)(QueryImpl&, Iter&, void*, const TypedQueryExecState&));

				void run_query_on_chunks_direct_iter(
						QueryInfo& queryInfo, const QueryPlan& plan, const TypedQueryExecState& state, void* pFunc,
						void (*runChunk)(QueryImpl&, Iter&, void*, const TypedQueryExecState&));

				template <QueryExecType ExecType>
				void each_inter(
						QueryInfo& queryInfo, const QueryPlan& plan, void* pFunc, const TypedQueryExecState& state,
						void (*runDirectFastChunk)(QueryImpl&, Iter&, void*, const TypedQueryExecState&),
						void (*runDirectChunk)(QueryImpl&, Iter&, void*, const TypedQueryExecState&),
						void (*runMappedChunk)(QueryImpl&, const QueryInfo&, Iter&, void*, const TypedQueryExecState&),
						bool needsInheritedArgIds, void (*invokeInherited)(World&, Entity, const Entity*, void*));

				void each_typed_erased(
						QueryExecType execType, void* pFunc, const TypedQueryExecState& state,
						void (*runDirectFastChunk)(QueryImpl&, Iter&, void*, const TypedQueryExecState&),
						void (*runDirectChunk)(QueryImpl&, Iter&, void*, const TypedQueryExecState&),
						void (*runMappedChunk)(QueryImpl&, const QueryInfo&, Iter&, void*, const TypedQueryExecState&),
						bool needsInheritedArgIds, void (*invokeInherited)(World&, Entity, const Entity*, void*));

				template <QueryExecType ExecType, typename Func>
				void each_typed_inter(QueryInfo& queryInfo, Func func);

				template <QueryExecType ExecType>
				void each_iter_inter_erased(
						QueryInfo& queryInfo, const QueryPlan& plan, void* pFunc, const TypedQueryExecState& state,
						void (*runDirectFastChunk)(QueryImpl&, Iter&, void*, const TypedQueryExecState&),
						void (*runMappedChunk)(QueryImpl&, const QueryInfo&, Iter&, void*, const TypedQueryExecState&));

				void each_walk_inter(
						QueryInfo& queryInfo, std::span<const Entity> entities, Constraints constraints, void* pFunc,
						const TypedQueryExecState& state,
						void (*runChunk)(QueryImpl&, const QueryInfo&, Iter&, void*, const TypedQueryExecState&));

				//! Selects the prepared execution plan for public iterator callbacks.
				//! \param queryInfo Prepared query cache and execution metadata.
				//! \param constraints Entity-row subset exposed to the callback.
				//! \return Query execution plan shared by typed and public iterator adapters.
				GAIA_NODISCARD QueryPlan prepare_query_plan(const QueryInfo& queryInfo, Constraints constraints) const {
					QueryPlan plan{};
					const auto cacheRange = selected_query_cache_range(queryInfo);
					plan.idxFrom = cacheRange.idxFrom;
					plan.idxTo = cacheRange.idxTo;
					const bool hasFilters = queryInfo.has_filters();
					const bool hasSortedPayload = queryInfo.has_sorted_payload();
					const bool hasDepthOrderBarrier = has_depth_order_hierarchy_enabled_barrier(queryInfo);
					bool hasConstrainedDepthOrderBarrier = constraints != Constraints::AcceptAll && hasDepthOrderBarrier;
					if (hasFilters)
						plan.flags |= QueryPlanFlag_Filtered;
					if (queryInfo.has_entity_filter_terms())
						plan.flags |= QueryPlanFlag_EntityFilter;
					if (queryInfo.has_inherited_data_payload())
						plan.flags |= QueryPlanFlag_InheritedPayload;
					if (queryInfo.has_grouped_payload())
						plan.flags |= QueryPlanFlag_Grouped;
					if (hasSortedPayload || hasDepthOrderBarrier)
						plan.flags |= QueryPlanFlag_Sorted;
					plan.payloadKind = exec_payload_kind(queryInfo, constraints);

					if (cacheRange.hasSelectedGroup) {
						plan.flags |= QueryPlanFlag_Grouped;
						plan.payloadKind = ExecPayloadKind::Grouped;
						if (!cacheRange.valid) {
							plan.mode = QueryPlanMode::Empty;
							return plan;
						}
					}

					if ((plan.flags & QueryPlanFlag_Filtered) == 0 && can_use_direct_entity_seed_eval(queryInfo)) {
						plan.mode = QueryPlanMode::EntitySeed;
						return plan;
					}

					if (plan.idxFrom >= plan.idxTo) {
						plan.mode = QueryPlanMode::Empty;
						return plan;
					}

					if (hasConstrainedDepthOrderBarrier && !depth_order_hierarchy_barrier_prunes(queryInfo)) {
						hasConstrainedDepthOrderBarrier = false;
						if (!hasSortedPayload && (plan.flags & QueryPlanFlag_InheritedPayload) == 0)
							plan.payloadKind = queryInfo.has_grouped_payload() ? ExecPayloadKind::Grouped : ExecPayloadKind::Plain;
					}
					if (hasConstrainedDepthOrderBarrier)
						plan.flags |= QueryPlanFlag_BarrierCache;

					if (hasSortedPayload) {
						plan.mode = QueryPlanMode::Sorted;
						return plan;
					}

					if (hasConstrainedDepthOrderBarrier || (plan.flags & QueryPlanFlag_InheritedPayload) != 0) {
						plan.mode = QueryPlanMode::Traversal;
						return plan;
					}

					if ((plan.flags & QueryPlanFlag_EntityFilter) != 0)
						return plan;

					if (plan.payloadKind != ExecPayloadKind::Plain) {
						if (plan.payloadKind != ExecPayloadKind::Grouped || !hasDepthOrderBarrier)
							return plan;
						if (hasConstrainedDepthOrderBarrier)
							return plan;
						if (!can_use_direct_chunk_iteration_fastpath(queryInfo))
							return plan;

						plan.mode = QueryPlanMode::DirectDense;
						return plan;
					}

					if (!can_use_direct_chunk_iteration_fastpath(queryInfo))
						return plan;

					plan.mode = QueryPlanMode::DirectDense;
					return plan;
				}

				//! Runs a public Iter callback over cached chunks without creating chunk batches.
				//! \tparam HasFilters When true, chunks must pass changed-filter checks before callback invocation.
				//! \tparam HasGroups When true, the prepared cache range carries per-archetype group ids.
				//! \tparam Func Public iterator callback type.
				//! \param queryInfo Prepared query cache and execution metadata.
				//! \param plan Prepared query execution metadata.
				//! \param constraints Entity-row subset exposed to the callback.
				//! \param func Callback invoked once for each matching chunk.
				template <bool HasFilters, bool HasGroups, typename Func>
				void run_query_on_chunks_runtime_direct_plain_impl(
						QueryInfo& queryInfo, const QueryPlan& plan, Constraints constraints, Func& func) {
					::gaia::ecs::update_version(*m_worldVersion);

					auto cacheView = queryInfo.cache_archetype_view();
					lock(*m_storage.world());

					Iter it;
					it.init_query_state(queryInfo.world(), constraints, false);

					//! Direct dense plans already encode expensive process gates. When no cached archetype can require
					//! prefab filtering and no depth-order barrier cache can prune, only deletion state remains dynamic.
					const bool canSkipProcessCheck =
							!queryInfo.result_cache_may_need_prefab_filter() && (plan.flags & QueryPlanFlag_BarrierCache) == 0;

					for (uint32_t i = plan.idxFrom; i < plan.idxTo; ++i) {
						auto* pArchetype = const_cast<Archetype*>(cacheView[i]);
						if (canSkipProcessCheck) {
							if GAIA_UNLIKELY (pArchetype->is_req_del())
								continue;
						} else if GAIA_UNLIKELY (!can_process_archetype_inter(queryInfo, *pArchetype, constraints))
							continue;

						auto indicesView = queryInfo.indices_mapping_view(i);
						const auto* pIndices = indicesView.data();
						const auto groupId = HasGroups ? queryInfo.group_id(i) : GroupId(0);
						const auto& chunks = pArchetype->chunks();
						for (auto* pChunk: chunks) {
							const auto from = detail::ChunkIterImpl::start_index(pChunk, constraints);
							const auto to = detail::ChunkIterImpl::end_index(pChunk, constraints);
							if GAIA_UNLIKELY (from == to)
								continue;
							if constexpr (HasFilters) {
								if (!match_filters(*pChunk, queryInfo, m_changedWorldVersion, indicesView))
									continue;
							}

							it.set_query_chunk(pArchetype, pIndices, pChunk, from, to);
							if constexpr (HasGroups)
								it.set_group_id(groupId);
							it.ctx(m_ctx);
							{
								GAIA_PROF_SCOPE(query_func);
								func(it);
							}
							finish_iter_writes(it);
							it.clear_touched_writes();
						}
					}

					unlock(*m_storage.world());
					commit_cmd_buffer_st(*m_storage.world());
					commit_cmd_buffer_mt(*m_storage.world());
					m_changedWorldVersion = *m_worldVersion;
				}

				//! Runs a public iterator callback through the fastest supported runtime path.
				//! \tparam ExecType Query execution mode requested by the public API.
				//! \tparam Func Public iterator callback type.
				//! \param func Callback invoked for each iterator step.
				//! \param constraints Entity-row subset exposed to the callback.
				template <QueryExecType ExecType, typename Func>
				void each_runtime_inter(Func func, Constraints constraints = Constraints::EnabledOnly) {
					if constexpr (ExecType == QueryExecType::Default) {
						auto& queryInfo = fetch();
						match_all(queryInfo);
						const auto plan = prepare_query_plan(queryInfo, constraints);
						if (plan.mode == QueryPlanMode::DirectDense) {
							const bool hasGroups = (plan.flags & QueryPlanFlag_Grouped) != 0;
							if ((plan.flags & QueryPlanFlag_Filtered) != 0) {
								if (hasGroups)
									run_query_on_chunks_runtime_direct_plain_impl<true, true>(queryInfo, plan, constraints, func);
								else
									run_query_on_chunks_runtime_direct_plain_impl<true, false>(queryInfo, plan, constraints, func);
							} else {
								if (hasGroups)
									run_query_on_chunks_runtime_direct_plain_impl<false, true>(queryInfo, plan, constraints, func);
								else
									run_query_on_chunks_runtime_direct_plain_impl<false, false>(queryInfo, plan, constraints, func);
							}
							return;
						}

						each_runtime_erased(
								queryInfo, plan, ExecType, static_cast<void*>(&func), &invoke_runtime_iter<Func, Iter>, constraints);
						return;
					}

					each_runtime_erased(ExecType, static_cast<void*>(&func), &invoke_runtime_iter<Func, Iter>, constraints);
				}

				//! Invokes a type-erased public iterator callback.
				//! \tparam Func Stored callback type.
				//! \tparam TIter Iterator type passed to the callback.
				//! \param pFunc Pointer to the stored callback object.
				//! \param it Iterator passed to the callback.
				template <typename Func, typename TIter>
				static void invoke_runtime_iter(void* pFunc, TIter& it) {
					auto& func = *static_cast<Func*>(pFunc);
					func(it);
				}

				struct RuntimeIterCallback {
					void* pFunc;
					void* pCtx;
					void (*invoke)(void*, Iter&);

					void operator()(Iter& it) const {
						GAIA_PROF_SCOPE(query_func);
						it.ctx(pCtx);
						invoke(pFunc, it);
					}
				};

				struct TypedDirectChunkCallback {
					QueryImpl* pSelf;
					void* pFunc;
					const TypedQueryExecState* pState;
					void (*runChunk)(QueryImpl&, Iter&, void*, const TypedQueryExecState&);

					void operator()(Iter& it) const {
						GAIA_PROF_SCOPE(query_func);
						it.ctx(pSelf->ctx());
						runChunk(*pSelf, it, pFunc, *pState);
					}
				};

				struct TypedMappedChunkCallback {
					QueryImpl* pSelf;
					const QueryInfo* pQueryInfo;
					void* pFunc;
					const TypedQueryExecState* pState;
					void (*runChunk)(QueryImpl&, const QueryInfo&, Iter&, void*, const TypedQueryExecState&);

					void operator()(Iter& it) const {
						GAIA_PROF_SCOPE(query_func);
						it.ctx(pSelf->ctx());
						runChunk(*pSelf, *pQueryInfo, it, pFunc, *pState);
					}
				};

				struct TypedIterErasedCallback {
					QueryImpl* pSelf;
					void* pFunc;
					const TypedQueryExecState* pState;
					void (*runDirect)(QueryImpl&, Iter&, void*, const TypedQueryExecState&);
					void (*runChunk)(QueryImpl&, const QueryInfo&, Iter&, void*, const TypedQueryExecState&);

					void operator()(Iter& it) const {
						GAIA_PROF_SCOPE(query_func);
						it.ctx(pSelf->ctx());
						pSelf->each_iter_erased(it, pFunc, *pState, runDirect, runChunk);
					}
				};

				//! Runs a type-erased public iterator callback through generic query execution.
				//! \param execType Query execution mode requested by the public API.
				//! \param pFunc Pointer to the stored callback object.
				//! \param invoke Type-erased callback invoker.
				//! \param constraints Entity-row subset exposed to the callback.
				void each_runtime_erased(
						QueryExecType execType, void* pFunc, void (*invoke)(void*, Iter&), Constraints constraints) {
					auto& queryInfo = fetch();
					match_all(queryInfo);
					const auto plan = prepare_query_plan(queryInfo, constraints);
					each_runtime_erased(queryInfo, plan, execType, pFunc, invoke, constraints);
				}

				//! Runs a type-erased public iterator callback using an already prepared query cache and plan.
				//! \param queryInfo Prepared query cache and execution metadata.
				//! \param plan Prepared query execution metadata.
				//! \param execType Query execution mode requested by the public API.
				//! \param pFunc Pointer to the stored callback object.
				//! \param invoke Type-erased callback invoker.
				//! \param constraints Entity-row subset exposed to the callback.
				void each_runtime_erased(
						QueryInfo& queryInfo, const QueryPlan& plan, QueryExecType execType, void* pFunc,
						void (*invoke)(void*, Iter&), Constraints constraints) {
					RuntimeIterCallback cb{pFunc, m_ctx, invoke};

					if (plan.mode == QueryPlanMode::EntitySeed) {
						each_direct_iter_inter(queryInfo, constraints, cb);
						return;
					}

					switch (execType) {
						case QueryExecType::Parallel:
							run_query_on_chunks_runtime_planned<QueryExecType::Parallel>(queryInfo, plan, constraints, cb);
							break;
						case QueryExecType::ParallelPerf:
							run_query_on_chunks_runtime_planned<QueryExecType::ParallelPerf>(queryInfo, plan, constraints, cb);
							break;
						case QueryExecType::ParallelEff:
							run_query_on_chunks_runtime_planned<QueryExecType::ParallelEff>(queryInfo, plan, constraints, cb);
							break;
						default:
							run_query_on_chunks_runtime_planned<QueryExecType::Default>(queryInfo, plan, constraints, cb);
							break;
					}
				}

				//------------------------------------------------

				//! Returns whether a direct term is backed by non-fragmenting storage and must be evaluated per entity.
				//! \param world World
				//! \param term Query term
				//! \return True if the term is backed by non-fragmenting storage. False otherwise.
				GAIA_NODISCARD static bool is_non_fragmenting_direct_term(const World& world, const QueryTerm& term) {
					if (term.src != EntityBad || term.entTrav != EntityBad || term_has_variables(term))
						return false;

					const auto id = term.id;
					return (id.pair() && world_relation_uses_non_fragmenting_storage(world, pair_rel(world, id))) ||
								 (!id.pair() && world_component_is_non_fragmenting(world, id));
				}

				//! Returns whether a term uses semantic `Is` matching rather than direct storage matching.
				//! \param term Query term
				//! \return True if semantic `Is` matching is used. False otherwise.
				GAIA_NODISCARD static bool uses_semantic_is_matching(const QueryTerm& term) {
					const auto id = term.id;
					return term.matchKind == QueryMatchKind::Semantic && term.src == EntityBad && term.entTrav == EntityBad &&
								 !term_has_variables(term) && id.pair() && id.id() == Is.id() && !is_wildcard(id.gen()) &&
								 !is_variable((EntityId)id.gen());
				}

				//! Returns whether a term uses strict semantic `Is` matching that excludes the base entity itself.
				//! \param term Query term
				//! \return True if strict semantic `Is` matching is used. False otherwise.
				GAIA_NODISCARD static bool uses_in_is_matching(const QueryTerm& term) {
					const auto id = term.id;
					return term.matchKind == QueryMatchKind::In && term.src == EntityBad && term.entTrav == EntityBad &&
								 !term_has_variables(term) && id.pair() && id.id() == Is.id() && !is_wildcard(id.gen()) &&
								 !is_variable((EntityId)id.gen());
				}

				//! Returns whether a term uses any semantic `Is` matching rather than direct storage matching.
				//! \param term Query term
				//! \return True if any semantic `Is` matching is used. False otherwise.
				GAIA_NODISCARD static bool uses_non_direct_is_matching(const QueryTerm& term) {
					return uses_semantic_is_matching(term) || uses_in_is_matching(term);
				}

				//! Returns whether a term could use inherited-id matching based on query shape alone.
				//! This intentionally ignores mutable world metadata such as current OnInstantiate policy.
				//! \param term Query term
				//! \return True if the term shape can resolve through inherited-id matching.
				GAIA_NODISCARD static bool uses_potential_inherited_id_matching(const QueryTerm& term) {
					return query_term_uses_potential_inherited_id_matching(term);
				}

				//! Returns whether a term uses semantic inherited-id matching rather than direct storage matching.
				//! \param world World
				//! \param term Query term
				//! \return True if inherited-id matching is used. False otherwise.
				GAIA_NODISCARD static bool uses_inherited_id_matching(const World& world, const QueryTerm& term) {
					return uses_potential_inherited_id_matching(term) && world_term_uses_inherit_policy(world, term.id);
				}

				//! Evaluates term presence for a concrete entity using either direct or semantic semantics.
				GAIA_NODISCARD static bool match_entity_term(const World& world, Entity entity, const QueryTerm& term) {
					if (uses_semantic_is_matching(term) || uses_inherited_id_matching(world, term))
						return world_has_entity_term(world, entity, term.id);
					if (uses_in_is_matching(term))
						return world_has_entity_term_in(world, entity, term.id);

					return world_has_entity_term_direct(world, entity, term.id);
				}

				//! Evaluates a compiled single-term direct-target query without re-walking the generic term loop.
				GAIA_NODISCARD static bool match_single_direct_target_term(
						const World& world, Entity entity, Entity termId, QueryCtx::DirectTargetEvalKind kind) {
					switch (kind) {
						case QueryCtx::DirectTargetEvalKind::SingleAllSemanticIs:
						case QueryCtx::DirectTargetEvalKind::SingleAllInherited:
							return world_has_entity_term(world, entity, termId);
						case QueryCtx::DirectTargetEvalKind::SingleAllInIs:
							return world_has_entity_term_in(world, entity, termId);
						case QueryCtx::DirectTargetEvalKind::SingleAllDirect:
							return world_has_entity_term_direct(world, entity, termId);
						case QueryCtx::DirectTargetEvalKind::Generic:
							break;
					}

					return false;
				}

				GAIA_NODISCARD static uint32_t count_direct_term_entities(const World& world, const QueryTerm& term) {
					if (uses_semantic_is_matching(term) || uses_inherited_id_matching(world, term))
						return world_count_direct_term_entities(world, term.id);
					if (uses_in_is_matching(term))
						return world_count_in_term_entities(world, term.id);

					return world_count_direct_term_entities_direct(world, term.id);
				}

				static void collect_direct_term_entities(const World& world, const QueryTerm& term, cnt::darray<Entity>& out) {
					if (uses_semantic_is_matching(term) || uses_inherited_id_matching(world, term)) {
						world_collect_direct_term_entities(world, term.id, out);
						return;
					}
					if (uses_in_is_matching(term)) {
						world_collect_in_term_entities(world, term.id, out);
						return;
					}

					world_collect_direct_term_entities_direct(world, term.id, out);
				}

				template <typename Func>
				GAIA_NODISCARD static bool for_each_direct_term_entity(const World& world, const QueryTerm& term, Func&& func) {
					struct Visitor {
						Func& func;
						static bool thunk(void* ctx, Entity entity) {
							return static_cast<Visitor*>(ctx)->func(entity);
						}
					};

					Visitor visitor{func};
					if (uses_semantic_is_matching(term) || uses_inherited_id_matching(world, term))
						return world_for_each_direct_term_entity(world, term.id, &visitor, &Visitor::thunk);
					if (uses_in_is_matching(term))
						return world_for_each_in_term_entity(world, term.id, &visitor, &Visitor::thunk);

					return world_for_each_direct_term_entity_direct(world, term.id, &visitor, &Visitor::thunk);
				}

				//! Detects queries that can skip archetype seeding and start directly from non-fragmenting term indices.
				GAIA_NODISCARD static bool can_use_direct_entity_seed_eval(const QueryInfo& queryInfo) {
					if (!queryInfo.can_direct_entity_seed_eval_shape())
						return false;

					const auto& world = *queryInfo.world();
					bool hasSeedableTerm = false;
					for (const auto& term: queryInfo.ctx().data.terms_view()) {
						if (term.op != QueryOpKind::All && term.op != QueryOpKind::Or)
							continue;
						if (uses_non_direct_is_matching(term) || uses_inherited_id_matching(world, term) ||
								uses_in_is_matching(term) || is_non_fragmenting_direct_term(world, term))
							hasSeedableTerm = true;
					}

					return hasSeedableTerm;
				}

				//! Detects queries whose terms can be evaluated directly against concrete target entities.
				GAIA_NODISCARD static bool can_use_direct_target_eval(const QueryInfo& queryInfo) {
					return queryInfo.can_direct_target_eval();
				}

				//! Detects the special direct OR/NOT shape that can be answered from a union of direct term entity sets.
				GAIA_NODISCARD static bool has_only_direct_or_terms(const QueryInfo& queryInfo) {
					return queryInfo.has_only_direct_or_terms();
				}

				static void
				add_chunk_run(cnt::darray<detail::BfsChunkRun>& runs, const EntityContainer& ec, uint32_t entityOffset) {
					if (runs.empty()) {
						runs.push_back({ec.pArchetype, ec.pChunk, ec.row, (uint16_t)(ec.row + 1), entityOffset});
						return;
					}

					auto& run = runs.back();
					if (ec.pChunk == run.pChunk && ec.row == run.to) {
						run.to = (uint16_t)(run.to + 1);
						return;
					}

					runs.push_back({ec.pArchetype, ec.pChunk, ec.row, (uint16_t)(ec.row + 1), entityOffset});
				}

				struct DirectEntitySeedInfo {
					Entity seededAllTerm = EntityBad;
					QueryMatchKind seededAllMatchKind = QueryMatchKind::Semantic;
					bool seededFromAll = false;
					bool seededFromOr = false;
				};

				//! Describes which direct term should seed direct non-fragmenting evaluation.
				struct DirectEntitySeedPlan {
					Entity bestAllTerm = EntityBad;
					uint32_t bestAllTermCount = UINT32_MAX;
					QueryMatchKind bestAllTermMatchKind = QueryMatchKind::Semantic;
					bool hasAllTerms = false;
					bool hasOrTerms = false;
					bool preferOrSeed = false;
				};

				struct DirectEntitySeedEvalPlan {
					//! Fast path for the common `ALL + direct/semantic Is` shape:
					//! after consuming the seed term, only one more `ALL` term remains.
					const QueryTerm* pSingleAllTerm = nullptr;
					//! No remaining terms after consuming the seed.
					bool alwaysMatch = false;
				};

				//! Chooses the narrowest available seed for direct non-fragmenting evaluation.
				GAIA_NODISCARD static bool should_prefer_direct_seed_term(
						const World& world, const QueryTerm& candidate, uint32_t candidateCount, const DirectEntitySeedPlan& plan) {
					const bool candidateIsSemanticIs = uses_non_direct_is_matching(candidate);
					const bool bestIsSemanticIs = plan.bestAllTermMatchKind != QueryMatchKind::Direct &&
																				plan.bestAllTerm.pair() && plan.bestAllTerm.id() == Is.id() &&
																				!is_wildcard(plan.bestAllTerm.gen()) &&
																				!is_variable((EntityId)plan.bestAllTerm.gen());
					const auto adjustedCandidateCount = candidateCount - (candidateIsSemanticIs && candidateCount > 0 ? 1U : 0U);
					const auto adjustedBestCount =
							plan.bestAllTermCount - (bestIsSemanticIs && plan.bestAllTermCount > 0 ? 1U : 0U);
					if (adjustedCandidateCount < adjustedBestCount)
						return true;
					if (adjustedCandidateCount > adjustedBestCount)
						return false;
					if (plan.bestAllTerm == EntityBad)
						return true;

					if (candidateIsSemanticIs != bestIsSemanticIs)
						return candidateIsSemanticIs;

					const bool candidateUsesInherited = uses_inherited_id_matching(world, candidate);
					const bool bestUsesInherited = plan.bestAllTermMatchKind == QueryMatchKind::Semantic &&
																				 !is_wildcard(plan.bestAllTerm) &&
																				 !is_variable((EntityId)plan.bestAllTerm.id()) &&
																				 (!plan.bestAllTerm.pair() || !is_variable((EntityId)plan.bestAllTerm.gen())) &&
																				 world_term_uses_inherit_policy(world, plan.bestAllTerm);
					if (candidateUsesInherited != bestUsesInherited)
						return !candidateUsesInherited;

					return false;
				}

				GAIA_NODISCARD static DirectEntitySeedPlan
				direct_entity_seed_plan(const World& world, const QueryInfo& queryInfo) {
					DirectEntitySeedPlan plan;
					uint32_t totalOrTermCount = 0;

					for (const auto& term: queryInfo.ctx().data.terms_view()) {
						if (term.src != EntityBad || term.entTrav != EntityBad || term_has_variables(term))
							continue;
						if (term.op == QueryOpKind::All) {
							plan.hasAllTerms = true;
							const auto cnt = count_direct_term_entities(world, term);
							if (should_prefer_direct_seed_term(world, term, cnt, plan)) {
								plan.bestAllTermCount = cnt;
								plan.bestAllTerm = term.id;
								plan.bestAllTermMatchKind = term.matchKind;
							}
						} else if (term.op == QueryOpKind::Or) {
							plan.hasOrTerms = true;
							totalOrTermCount += count_direct_term_entities(world, term);
						}
					}

					plan.preferOrSeed = plan.hasOrTerms && (!plan.hasAllTerms || totalOrTermCount < plan.bestAllTermCount);
					return plan;
				}

				//! Evaluates the remaining direct terms for a single seeded entity after the seed term itself was consumed.
				GAIA_NODISCARD static bool match_direct_entity_terms(
						const World& world, Entity entity, const QueryInfo& queryInfo, const DirectEntitySeedInfo& seedInfo) {
					bool hasOrTerms = false;
					bool anyOrMatched = false;

					for (const auto& term: queryInfo.ctx().data.terms_view()) {
						if (term.src != EntityBad || term.entTrav != EntityBad || term_has_variables(term))
							continue;
						if (seedInfo.seededFromAll && term.op == QueryOpKind::All && term.id == seedInfo.seededAllTerm &&
								term.matchKind == seedInfo.seededAllMatchKind)
							continue;
						if (seedInfo.seededFromOr && term.op == QueryOpKind::Or)
							continue;

						const bool present = match_entity_term(world, entity, term);
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

				GAIA_NODISCARD static const QueryTerm*
				find_direct_all_seed_term(const QueryInfo& queryInfo, const DirectEntitySeedPlan& plan) {
					for (const auto& term: queryInfo.ctx().data.terms_view()) {
						if (term.src != EntityBad || term.entTrav != EntityBad || term_has_variables(term))
							continue;
						if (term.op != QueryOpKind::All || term.id != plan.bestAllTerm ||
								term.matchKind != plan.bestAllTermMatchKind)
							continue;
						return &term;
					}

					return nullptr;
				}

				GAIA_NODISCARD static DirectEntitySeedEvalPlan
				direct_all_seed_eval_plan(const QueryInfo& queryInfo, const DirectEntitySeedInfo& seedInfo) {
					DirectEntitySeedEvalPlan plan{};

					for (const auto& term: queryInfo.ctx().data.terms_view()) {
						if (term.src != EntityBad || term.entTrav != EntityBad || term_has_variables(term))
							return {};
						if (seedInfo.seededFromAll && term.op == QueryOpKind::All && term.id == seedInfo.seededAllTerm &&
								term.matchKind == seedInfo.seededAllMatchKind)
							continue;

						if (term.op == QueryOpKind::All) {
							if (plan.pSingleAllTerm != nullptr)
								return {};
							plan.pSingleAllTerm = &term;
							continue;
						}

						return {};
					}

					plan.alwaysMatch = plan.pSingleAllTerm == nullptr;
					return plan;
				}

				//! Returns whether a repeated semantic or inherited seed can be cached as chunk runs.
				//! \param world World
				//! \param queryInfo Query info
				//! \param seedTerm Seed term
				//! \return True if direct-seed runs can be cached. False otherwise.
				GAIA_NODISCARD static bool
				can_use_direct_seed_run_cache(const World& world, const QueryInfo& queryInfo, const QueryTerm& seedTerm) {
					if (!(uses_non_direct_is_matching(seedTerm) || uses_inherited_id_matching(world, seedTerm)))
						return false;

					for (const auto& term: queryInfo.ctx().data.terms_view()) {
						if (term.src != EntityBad || term.entTrav != EntityBad || term_has_variables(term))
							return false;
						if (term.op == QueryOpKind::Any || term.op == QueryOpKind::Count || term.op == QueryOpKind::Or)
							return false;
						if (term.op == QueryOpKind::All && term.id == seedTerm.id && term.matchKind == seedTerm.matchKind)
							continue;
						if (is_non_fragmenting_direct_term(world, term))
							return false;
					}

					return true;
				}

				GAIA_NODISCARD std::span<const detail::BfsChunkRun> cached_direct_seed_runs(
						QueryInfo& queryInfo, const QueryTerm& seedTerm, const DirectEntitySeedInfo& seedInfo,
						Constraints constraints) {
					auto& runData = ensure_direct_seed_run_data();
					auto& world = *queryInfo.world();
					const auto cachedConstraints = constraints;
					const auto relVersion = world_rel_version(world, Is);
					const auto worldVersion = ::gaia::ecs::world_version(world);

					if (runData.cacheValid && runData.cachedSeedTerm == seedTerm.id &&
							runData.cachedSeedMatchKind == seedTerm.matchKind && runData.cachedConstraints == cachedConstraints &&
							runData.cachedRelVersion == relVersion && runData.cachedWorldVersion == worldVersion) {
						return {runData.cachedRuns.data(), runData.cachedRuns.size()};
					}

					auto& runs = runData.cachedRuns;
					auto& entities = runData.cachedEntities;
					auto& chunkOrderedEntities = runData.cachedChunkOrderedEntities;
					runs.clear();
					entities.clear();
					chunkOrderedEntities.clear();

					(void)for_each_direct_term_entity(world, seedTerm, [&](Entity entity) {
						if (!match_direct_entity_constraints(world, queryInfo, entity, constraints))
							return true;

						if (!match_direct_entity_terms(world, entity, queryInfo, seedInfo))
							return true;

						entities.push_back(entity);
						return true;
					});

					chunkOrderedEntities = entities;
					core::sort(chunkOrderedEntities, [&](Entity left, Entity right) {
						const auto& ecLeft = ::gaia::ecs::fetch(world, left);
						const auto& ecRight = ::gaia::ecs::fetch(world, right);
						if (ecLeft.pArchetype != ecRight.pArchetype)
							return ecLeft.pArchetype->id() < ecRight.pArchetype->id();
						if (ecLeft.pChunk != ecRight.pChunk)
							return ecLeft.pChunk < ecRight.pChunk;
						return ecLeft.row < ecRight.row;
					});

					uint32_t entityOffset = 0;
					for (const auto entity: chunkOrderedEntities) {
						const auto& ec = ::gaia::ecs::fetch(world, entity);
						add_chunk_run(runs, ec, entityOffset++);
					}

					runData.cachedSeedTerm = seedTerm.id;
					runData.cachedSeedMatchKind = seedTerm.matchKind;
					runData.cachedConstraints = cachedConstraints;
					runData.cachedRelVersion = relVersion;
					runData.cachedWorldVersion = worldVersion;
					runData.cacheValid = true;
					return {runs.data(), runs.size()};
				}

				GAIA_NODISCARD std::span<const Entity> cached_direct_seed_chunk_entities(
						QueryInfo& queryInfo, const QueryTerm& seedTerm, const DirectEntitySeedInfo& seedInfo,
						Constraints constraints) {
					(void)cached_direct_seed_runs(queryInfo, seedTerm, seedInfo, constraints);
					auto& runData = ensure_direct_seed_run_data();
					return {runData.cachedChunkOrderedEntities.data(), runData.cachedChunkOrderedEntities.size()};
				}

				template <typename Func>
				GAIA_NODISCARD static bool for_each_direct_all_seed(
						const World& world, const QueryInfo& queryInfo, const DirectEntitySeedPlan& plan, Constraints constraints,
						Func&& func) {
					const auto* pSeedTerm = find_direct_all_seed_term(queryInfo, plan);
					GAIA_ASSERT(pSeedTerm != nullptr);
					if (pSeedTerm == nullptr)
						return true;

					DirectEntitySeedInfo seedInfo{};
					seedInfo.seededAllTerm = pSeedTerm->id;
					seedInfo.seededAllMatchKind = pSeedTerm->matchKind;
					seedInfo.seededFromAll = true;
					const auto evalPlan = direct_all_seed_eval_plan(queryInfo, seedInfo);
					const Archetype* pLastSingleAllArchetype = nullptr;
					bool lastSingleAllMatch = false;
					bool seedImpliesSingleAllTerm = false;
					if (evalPlan.pSingleAllTerm != nullptr && uses_non_direct_is_matching(*pSeedTerm) &&
							(uses_non_direct_is_matching(*evalPlan.pSingleAllTerm) ||
							 uses_inherited_id_matching(world, *evalPlan.pSingleAllTerm))) {
						const auto seedTarget = pair_tgt(world, pSeedTerm->id);
						if (seedTarget != EntityBad)
							seedImpliesSingleAllTerm = match_entity_term(world, seedTarget, *evalPlan.pSingleAllTerm);
					}

					// Stream the chosen ALL seed term directly. This avoids materializing a temporary
					// entity array for the common `all<T>().is(base)` shape.
					return for_each_direct_term_entity(world, *pSeedTerm, [&](Entity entity) {
						if (!match_direct_entity_constraints(world, queryInfo, entity, constraints))
							return true;

						if (evalPlan.alwaysMatch)
							return func(entity);
						if (evalPlan.pSingleAllTerm != nullptr) {
							if (seedImpliesSingleAllTerm)
								return func(entity);
							if (uses_non_direct_is_matching(*evalPlan.pSingleAllTerm) ||
									uses_inherited_id_matching(world, *evalPlan.pSingleAllTerm)) {
								const auto* pArchetype = world_entity_archetype(world, entity);
								if (pArchetype != pLastSingleAllArchetype) {
									lastSingleAllMatch = match_entity_term(world, entity, *evalPlan.pSingleAllTerm);
									pLastSingleAllArchetype = pArchetype;
								}
								if (!lastSingleAllMatch)
									return true;
							} else if (!match_entity_term(world, entity, *evalPlan.pSingleAllTerm)) {
								return true;
							}
							return func(entity);
						}
						if (!match_direct_entity_terms(world, entity, queryInfo, seedInfo))
							return true;

						return func(entity);
					});
				}

				//! Applies iterator-specific entity state constraints to the direct seeded path.
				GAIA_NODISCARD static bool match_direct_entity_constraints(
						const World& world, const QueryInfo& queryInfo, Entity entity, Constraints constraints) {
					if (!queryInfo.matches_prefab_entities() && world_entity_prefab(world, entity))
						return false;

					if (constraints == Constraints::EnabledOnly)
						return world_entity_enabled(world, entity);
					if (constraints == Constraints::DisabledOnly)
						return !world_entity_enabled(world, entity);
					return true;
				}

				//! Detects when a direct seed can be counted by archetype buckets instead of per entity checks.
				GAIA_NODISCARD static bool can_use_archetype_bucket_count(
						const World& world, const QueryInfo& queryInfo, const DirectEntitySeedInfo& seedInfo) {
					if (!seedInfo.seededFromAll && !seedInfo.seededFromOr)
						return false;

					for (const auto& term: queryInfo.ctx().data.terms_view()) {
						if (term.src != EntityBad || term.entTrav != EntityBad || term_has_variables(term))
							return false;
						if (seedInfo.seededFromAll && term.id == seedInfo.seededAllTerm && term.op == QueryOpKind::All)
							continue;
						if (seedInfo.seededFromOr && term.op == QueryOpKind::Or)
							continue;
						if (term.op != QueryOpKind::All && term.op != QueryOpKind::Not)
							return false;
						if (is_non_fragmenting_direct_term(world, term))
							return false;
						if (uses_inherited_id_matching(world, term))
							return false;
					}

					return true;
				}

				//! Groups seeded entities by archetype and counts whole buckets when only structural ALL/NOT terms remain.
				GAIA_NODISCARD static uint32_t count_direct_entity_seed_by_archetype(
						const World& world, const QueryInfo& queryInfo, const cnt::darray<Entity>& seedEntities,
						const DirectEntitySeedInfo& seedInfo, Constraints constraints) {
					auto& scratch = direct_query_scratch();

					scratch.archetypes.clear();
					scratch.bucketEntities.clear();
					scratch.counts.clear();

					for (const auto entity: seedEntities) {
						if (!match_direct_entity_constraints(world, queryInfo, entity, constraints))
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
						if (match_direct_entity_terms(world, scratch.bucketEntities[i], queryInfo, seedInfo))
							cnt += scratch.counts[i];
					}

					return cnt;
				}

				//! Counts the union of direct OR term entity sets while deduplicating entities across terms.
				GAIA_NODISCARD static uint32_t
				count_direct_or_union(const World& world, const QueryInfo& queryInfo, Constraints constraints) {
					auto& scratch = direct_query_scratch();
					const auto seenVersion = next_direct_query_seen_version(scratch);
					const bool hasDirectNotTerms = has_direct_not_terms(queryInfo);

					uint32_t cnt = 0;
					for (const auto& term: queryInfo.ctx().data.terms_view()) {
						if (term.op != QueryOpKind::Or)
							continue;

						(void)for_each_direct_term_entity(world, term, [&](Entity entity) {
							if (!match_direct_entity_constraints(world, queryInfo, entity, constraints))
								return true;

							const auto entityId = (uint32_t)entity.id();
							ensure_direct_query_count_capacity(scratch, entityId);

							if (scratch.counts[entityId] == seenVersion)
								return true;
							scratch.counts[entityId] = seenVersion;

							bool rejected = false;
							if (hasDirectNotTerms) {
								for (const auto& notTerm: queryInfo.ctx().data.terms_view()) {
									if (notTerm.op != QueryOpKind::Not)
										continue;
									if (match_entity_term(world, entity, notTerm)) {
										rejected = true;
										break;
									}
								}
							}

							if (!rejected)
								++cnt;
							return true;
						});
					}

					return cnt;
				}

				//! Returns whether the direct OR union becomes empty after applying NOT terms and iterator constraints.
				//! \param world World
				//! \param queryInfo Query info
				//! \param constraints Iterator constraints applied to the candidate entities.
				//! \return True if no surviving entity exists. False otherwise.
				GAIA_NODISCARD static bool
				is_empty_direct_or_union(const World& world, const QueryInfo& queryInfo, Constraints constraints) {
					auto& scratch = direct_query_scratch();
					const auto seenVersion = next_direct_query_seen_version(scratch);
					const bool hasDirectNotTerms = has_direct_not_terms(queryInfo);

					for (const auto& term: queryInfo.ctx().data.terms_view()) {
						if (term.op != QueryOpKind::Or)
							continue;

						const bool completed = for_each_direct_term_entity(world, term, [&](Entity entity) {
							if (!match_direct_entity_constraints(world, queryInfo, entity, constraints))
								return true;

							const auto entityId = (uint32_t)entity.id();
							ensure_direct_query_count_capacity(scratch, entityId);

							if (scratch.counts[entityId] == seenVersion)
								return true;
							scratch.counts[entityId] = seenVersion;

							bool rejected = false;
							if (hasDirectNotTerms) {
								for (const auto& notTerm: queryInfo.ctx().data.terms_view()) {
									if (notTerm.op != QueryOpKind::Not)
										continue;
									if (match_entity_term(world, entity, notTerm)) {
										rejected = true;
										break;
									}
								}
							}

							if (!rejected)
								return false;
							return true;
						});

						if (!completed)
							return true;
					}

					return false;
				}

				//! Builds the best direct entity seed set from the smallest positive ALL term or the OR union fallback.
				static DirectEntitySeedInfo
				build_direct_entity_seed(const World& world, const QueryInfo& queryInfo, cnt::darray<Entity>& out) {
					auto& scratch = direct_query_scratch();
					out.clear();
					DirectEntitySeedInfo seedInfo{};
					const auto plan = direct_entity_seed_plan(world, queryInfo);

					if (plan.hasAllTerms && !plan.preferOrSeed) {
						if (plan.bestAllTerm != EntityBad) {
							for (const auto& term: queryInfo.ctx().data.terms_view()) {
								if (term.src != EntityBad || term.entTrav != EntityBad || term_has_variables(term))
									continue;
								if (term.op != QueryOpKind::All || term.id != plan.bestAllTerm ||
										term.matchKind != plan.bestAllTermMatchKind)
									continue;
								collect_direct_term_entities(world, term, out);
								seedInfo.seededAllMatchKind = term.matchKind;
								break;
							}
							seedInfo.seededFromAll = true;
							seedInfo.seededAllTerm = plan.bestAllTerm;
						}
						return seedInfo;
					}

					const auto seenVersion = next_direct_query_seen_version(scratch);

					for (const auto& term: queryInfo.ctx().data.terms_view()) {
						if (term.src != EntityBad || term.entTrav != EntityBad || term_has_variables(term))
							continue;
						if (term.op != QueryOpKind::Or)
							continue;

						scratch.termEntities.clear();
						collect_direct_term_entities(world, term, scratch.termEntities);
						for (const auto entity: scratch.termEntities) {
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

				//! Returns whether direct OR evaluation still has direct NOT terms that must be checked per entity.
				//! \param queryInfo Query info
				//! \return True if direct NOT terms remain. False otherwise.
				GAIA_NODISCARD static bool has_direct_not_terms(const QueryInfo& queryInfo) {
					for (const auto& term: queryInfo.ctx().data.terms_view()) {
						if (term.src != EntityBad || term.entTrav != EntityBad || term_has_variables(term))
							continue;
						if (term.op == QueryOpKind::Not)
							return true;
					}

					return false;
				}

				//! Visits the deduplicated OR union for direct-seeded queries without materializing an entity seed array first.
				//! \tparam Func Callback type
				//! \param world World
				//! \param queryInfo Query info
				//! \param constraints Iterator constraints applied to the candidate entities.
				//! \param func Callback executed for each surviving entity.
				template <typename Func>
				void for_each_direct_or_union(World& world, const QueryInfo& queryInfo, Constraints constraints, Func&& func) {
					auto& scratch = direct_query_scratch();
					const auto seenVersion = next_direct_query_seen_version(scratch);
					DirectEntitySeedInfo seedInfo{};
					seedInfo.seededFromOr = true;

					for (const auto& term: queryInfo.ctx().data.terms_view()) {
						if (term.op != QueryOpKind::Or)
							continue;

						(void)for_each_direct_term_entity(world, term, [&](Entity entity) {
							if (!match_direct_entity_constraints(world, queryInfo, entity, constraints))
								return true;

							const auto entityId = (uint32_t)entity.id();
							ensure_direct_query_count_capacity(scratch, entityId);

							if (scratch.counts[entityId] == seenVersion)
								return true;
							scratch.counts[entityId] = seenVersion;

							if (!match_direct_entity_terms(world, entity, queryInfo, seedInfo))
								return true;

							func(entity);
							return true;
						});
					}
				}

				//! Fast empty() path for direct non-fragmenting queries that can seed from non-fragmenting term indices.
				//! \tparam UseFilters True when changed/per-chunk filters must be evaluated.
				//! \param queryInfo Prepared query cache and execution metadata.
				//! \param constraints Entity-row constraints to apply.
				//! \return True if no entity matches the query under @a constraints.
				//! \see empty(Constraints)
				template <bool UseFilters>
				GAIA_NODISCARD bool empty_inter(const QueryInfo& queryInfo, Constraints constraints) const {
					const auto cacheRange = selected_query_cache_range(queryInfo);

					if constexpr (!UseFilters) {
						if (!cacheRange.hasSelectedGroup && can_use_direct_entity_seed_eval(queryInfo)) {
							if (has_only_direct_or_terms(queryInfo))
								return is_empty_direct_or_union(*queryInfo.world(), queryInfo, constraints);

							const auto plan = direct_entity_seed_plan(*queryInfo.world(), queryInfo);
							bool empty = true;
							(void)for_each_direct_all_seed(*queryInfo.world(), queryInfo, plan, constraints, [&](Entity) {
								empty = false;
								return false;
							});
							return empty;
						}
					}

					const bool hasEntityFilters = queryInfo.has_entity_filter_terms();
					const auto cacheView = queryInfo.cache_archetype_view();
					const bool needsBarrierCache = needs_depth_order_hierarchy_barrier_cache(queryInfo, constraints);
					if (needsBarrierCache)
						const_cast<QueryInfo&>(queryInfo).ensure_depth_order_hierarchy_barrier_cache();
					if (!cacheRange.valid)
						return true;
					const auto idxFrom = cacheRange.idxFrom;
					const auto idxTo = cacheRange.idxTo;

					for (uint32_t qi = idxFrom; qi < idxTo; ++qi) {
						const auto* pArchetype = cacheView[qi];
						const bool barrierPasses = !needsBarrierCache || queryInfo.barrier_passes(qi);
						if GAIA_UNLIKELY (!can_process_archetype_inter(queryInfo, *pArchetype, constraints, barrierPasses))
							continue;

						GAIA_PROF_SCOPE(query::empty);

						const auto& chunks = pArchetype->chunks();
						Iter it;
						it.init_query_state(queryInfo.world(), constraints, false);
						it.set_archetype(pArchetype);

						if (!hasEntityFilters) {
							for (auto* pChunk: chunks) {
								uint16_t from = 0;
								uint16_t to = 0;
								chunk_effective_range(pChunk, constraints, needsBarrierCache, barrierPasses, from, to);
								if (from == to)
									continue;
								it.set_chunk(pChunk, from, to);
								if constexpr (UseFilters) {
									if (!match_filters(*pChunk, queryInfo, m_changedWorldVersion))
										continue;
								}
								return false;
							}
							continue;
						}

						const bool isNotEmpty = core::has_if(chunks, [&](Chunk* pChunk) {
							uint16_t from = 0;
							uint16_t to = 0;
							chunk_effective_range(pChunk, constraints, needsBarrierCache, barrierPasses, from, to);
							if (from == to)
								return false;
							it.set_chunk(pChunk, from, to);
							if constexpr (UseFilters)
								if (it.size() == 0 || !match_filters(*pChunk, queryInfo, m_changedWorldVersion))
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

				//! Evaluates the entity-level terms that are not fully represented by archetype membership.
				GAIA_NODISCARD static bool match_entity_filters(const World& world, Entity entity, const QueryInfo& queryInfo) {
					bool hasOrTerms = false;
					bool anyOrMatched = false;
					const bool hasEntityFilterTerms = queryInfo.has_entity_filter_terms();

					for (const auto& term: queryInfo.ctx().data.terms_view()) {
						if (term.src != EntityBad || term.entTrav != EntityBad || term_has_variables(term))
							continue;

						const auto id = term.id;
						const bool isDirectIsTerm = uses_non_direct_is_matching(term);
						const bool isInheritedTerm = uses_inherited_id_matching(world, term);
						const bool isNonFragmentingTerm =
								(id.pair() && world_relation_uses_non_fragmenting_storage(world, pair_rel(world, id))) ||
								(!id.pair() && world_component_is_non_fragmenting(world, id));
						const bool needsEntityFilter = isNonFragmentingTerm || isDirectIsTerm || isInheritedTerm ||
																					 (hasEntityFilterTerms && term.op == QueryOpKind::Or);
						if (!needsEntityFilter)
							continue;

						const bool present = match_entity_term(world, entity, term);
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

				//! Returns whether any of the provided target entities matches the query semantics.
				//! \param queryInfo Query info
				//! \param archetype Archetype
				//! \param targetEntities Candidate target entities
				//! \return True if any target entity matches. False otherwise.
				GAIA_NODISCARD bool
				matches_target_entities(QueryInfo& queryInfo, const Archetype& archetype, EntitySpan targetEntities) {
					if (targetEntities.empty())
						return false;

					const auto& world = *queryInfo.world();

					if (can_use_direct_target_eval(queryInfo)) {
						const auto directTargetEvalKind = queryInfo.direct_target_eval_kind();
						if (directTargetEvalKind != QueryCtx::DirectTargetEvalKind::Generic) {
							const auto termId = queryInfo.direct_target_eval_id();
							if (targetEntities.size() == 1) {
								const auto entity = targetEntities[0];
								if (!match_direct_entity_constraints(world, queryInfo, entity, Constraints::EnabledOnly))
									return false;
								return match_single_direct_target_term(world, entity, termId, directTargetEvalKind);
							}

							for (const auto entity: targetEntities) {
								if (!match_direct_entity_constraints(world, queryInfo, entity, Constraints::EnabledOnly))
									continue;
								if (match_single_direct_target_term(world, entity, termId, directTargetEvalKind))
									return true;
							}

							return false;
						}

						const DirectEntitySeedInfo seedInfo{};
						if (targetEntities.size() == 1) {
							const auto entity = targetEntities[0];
							if (!match_direct_entity_constraints(world, queryInfo, entity, Constraints::EnabledOnly))
								return false;
							return match_direct_entity_terms(world, entity, queryInfo, seedInfo);
						}

						for (const auto entity: targetEntities) {
							if (!match_direct_entity_constraints(world, queryInfo, entity, Constraints::EnabledOnly))
								continue;
							if (match_direct_entity_terms(world, entity, queryInfo, seedInfo))
								return true;
						}

						return false;
					}

					if (!match_one(queryInfo, archetype, targetEntities))
						return false;

					if (!queryInfo.has_entity_filter_terms())
						return true;

					for (const auto entity: targetEntities) {
						if (!match_direct_entity_constraints(world, queryInfo, entity, Constraints::EnabledOnly))
							continue;
						if (match_entity_filters(world, entity, queryInfo))
							return true;
					}

					return false;
				}

				//! Fast count() path for direct non-fragmenting queries that can seed from non-fragmenting term indices.
				//! \tparam UseFilters True when changed/per-chunk filters must be evaluated.
				//! \param queryInfo Prepared query cache and execution metadata.
				//! \param constraints Entity-row constraints to apply.
				//! \return Number of entities matching the query under @a constraints.
				//! \see count(Constraints)
				template <bool UseFilters>
				GAIA_NODISCARD uint32_t count_inter(const QueryInfo& queryInfo, Constraints constraints) const {
					const auto cacheRange = selected_query_cache_range(queryInfo);

					if constexpr (!UseFilters) {
						if (!cacheRange.hasSelectedGroup && can_use_direct_entity_seed_eval(queryInfo)) {
							auto& scratch = direct_query_scratch();
							if (has_only_direct_or_terms(queryInfo))
								return count_direct_or_union(*queryInfo.world(), queryInfo, constraints);

							const auto plan = direct_entity_seed_plan(*queryInfo.world(), queryInfo);
							const auto seedInfo = build_direct_entity_seed(*queryInfo.world(), queryInfo, scratch.entities);

							if (can_use_archetype_bucket_count(*queryInfo.world(), queryInfo, seedInfo))
								return count_direct_entity_seed_by_archetype(
										*queryInfo.world(), queryInfo, scratch.entities, seedInfo, constraints);

							uint32_t cnt = 0;
							(void)for_each_direct_all_seed(*queryInfo.world(), queryInfo, plan, constraints, [&](Entity) {
								++cnt;
								return true;
							});

							return cnt;
						}
					}

					uint32_t cnt = 0;
					const bool hasEntityFilters = queryInfo.has_entity_filter_terms();
					const auto cacheView = queryInfo.cache_archetype_view();
					const bool needsBarrierCache = needs_depth_order_hierarchy_barrier_cache(queryInfo, constraints);
					if (needsBarrierCache)
						const_cast<QueryInfo&>(queryInfo).ensure_depth_order_hierarchy_barrier_cache();

					if (!cacheRange.valid)
						return 0;
					const auto idxFrom = cacheRange.idxFrom;
					const auto idxTo = cacheRange.idxTo;

					for (uint32_t qi = idxFrom; qi < idxTo; ++qi) {
						const auto* pArchetype = cacheView[qi];
						const bool barrierPasses = !needsBarrierCache || queryInfo.barrier_passes(qi);
						if GAIA_UNLIKELY (!can_process_archetype_inter(queryInfo, *pArchetype, constraints, barrierPasses))
							continue;

						GAIA_PROF_SCOPE(query::count);

						const auto& chunks = pArchetype->chunks();
						Iter it;
						it.init_query_state(queryInfo.world(), constraints, false);
						it.set_archetype(pArchetype);

						if (!hasEntityFilters) {
							for (auto* pChunk: chunks) {
								uint16_t from = 0;
								uint16_t to = 0;
								chunk_effective_range(pChunk, constraints, needsBarrierCache, barrierPasses, from, to);
								const uint16_t entityCnt = to - from;
								if (entityCnt == 0)
									continue;
								it.set_chunk(pChunk, from, to);

								if constexpr (UseFilters) {
									if (!match_filters(*pChunk, queryInfo, m_changedWorldVersion))
										continue;
								}

								cnt += entityCnt;
							}
							continue;
						}
						for (auto* pChunk: chunks) {
							uint16_t from = 0;
							uint16_t to = 0;
							chunk_effective_range(pChunk, constraints, needsBarrierCache, barrierPasses, from, to);
							const uint16_t entityCnt = to - from;
							if (entityCnt == 0)
								continue;
							it.set_chunk(pChunk, from, to);

							// Filters
							if constexpr (UseFilters) {
								if (!match_filters(*pChunk, queryInfo, m_changedWorldVersion))
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

				static void init_direct_entity_iter(
						const QueryInfo& queryInfo, const World& world, const EntityContainer& ec, Iter& it, uint8_t* pIndices,
						Entity* pTermIds, const Archetype*& pLastArchetype) {
					GAIA_ASSERT(ec.pArchetype != nullptr);
					GAIA_ASSERT(ec.pChunk != nullptr);
					GAIA_ASSERT(ec.row < ec.pChunk->size());

					if (ec.pArchetype != pLastArchetype) {
						GAIA_FOR(ChunkHeader::MAX_COMPONENTS) {
							pIndices[i] = 0xFF;
							pTermIds[i] = EntityBad;
						}

						const auto terms = queryInfo.ctx().data.terms_view();
						const auto queryIdCnt = (uint32_t)terms.size();
						auto indicesView = queryInfo.try_indices_mapping_view(ec.pArchetype);
						GAIA_FOR(queryIdCnt) {
							const auto& term = terms[i];
							const auto fieldIdx = term.fieldIndex;
							const auto queryId = term.id;
							pTermIds[fieldIdx] = queryId;
							if (!indicesView.empty()) {
								pIndices[fieldIdx] = indicesView[fieldIdx];
								continue;
							}
							if (!queryId.pair() && world_component_uses_sparse_storage(world, queryId)) {
#if GAIA_ASSERT_ENABLED
								const auto compIdx = core::get_index_unsafe(ec.pArchetype->ids_view(), queryId);
								GAIA_ASSERT(compIdx != BadIndex);
#endif
								pIndices[fieldIdx] = 0xFF;
								continue;
							}

							auto compIdx = world_component_index_comp_idx(world, *ec.pArchetype, queryId);
							if (compIdx == BadIndex)
								compIdx = core::get_index(ec.pArchetype->ids_view(), queryId);
							pIndices[fieldIdx] = (uint8_t)compIdx;
						}

						it.set_archetype(ec.pArchetype);
						it.set_comp_indices(pIndices);
						const auto inheritedDataView = queryInfo.inherited_data_view(ec.pArchetype);
						it.set_inherited_data(inheritedDataView);
						it.set_term_ids(pTermIds);
						pLastArchetype = ec.pArchetype;
					}

					it.set_chunk(ec.pChunk, ec.row, (uint16_t)(ec.row + 1));
					it.set_group_id(0);
				}

				static void init_direct_entity_iter(
						const QueryInfo& queryInfo, const World& world, Entity entity, Iter& it, uint8_t* pIndices,
						Entity* pTermIds) {
					const auto& ec = ::gaia::ecs::fetch(world, entity);
					const Archetype* pLastArchetype = nullptr;
					it.set_world(&world);
					init_direct_entity_iter(queryInfo, world, ec, it, pIndices, pTermIds, pLastArchetype);
				}

				template <typename Func>
				void each_chunk_runs_iter(
						QueryInfo& queryInfo, std::span<const detail::BfsChunkRun> runs, Constraints constraints, Func func) {
					auto& world = *queryInfo.world();
					Iter it;
					it.init_query_state(&world, constraints, false);
					const Archetype* pLastArchetype = nullptr;
					uint8_t indices[ChunkHeader::MAX_COMPONENTS];
					Entity termIds[ChunkHeader::MAX_COMPONENTS];

					for (const auto& run: runs) {
						const auto& ec = ::gaia::ecs::fetch(world, run.pChunk->entity_view()[run.from]);
						init_direct_entity_iter(queryInfo, world, ec, it, indices, termIds, pLastArchetype);
						it.set_chunk(run.pChunk, run.from, run.to);
						it.set_group_id(0);
						it.ctx(m_ctx);
						func(it);
						finish_iter_writes(it);
						it.clear_touched_writes();
					}
				}

				struct DirectChunkArgEvalDesc {
					Entity id = EntityBad;
					bool isEntity = false;
					bool isPair = false;
				};

				//! Returns whether a runtime query argument descriptor can use direct chunk access.
				//! \param world World
				//! \param queryInfo Query info
				//! \param desc Typed argument descriptor
				//! \return True if the argument can use direct chunk access. False otherwise.
				GAIA_NODISCARD static bool can_use_direct_chunk_term_eval_arg(
						World& world, const QueryInfo& queryInfo, const DirectChunkArgEvalDesc& desc) {
					if (desc.isEntity)
						return true;
					if (desc.isPair)
						return false;
					if (world_component_uses_sparse_storage(world, desc.id))
						return false;

					for (const auto& term: queryInfo.ctx().data.terms_view()) {
						if (term.id != desc.id)
							continue;
						if (term.src != EntityBad || term.entTrav != EntityBad || term_has_variables(term))
							return false;
						if (uses_non_direct_is_matching(term) || uses_inherited_id_matching(world, term) ||
								is_non_fragmenting_direct_term(world, term))
							return false;
						return true;
					}

					return false;
				}

				GAIA_NODISCARD static bool can_use_direct_chunk_term_eval_descs(
						World& world, const QueryInfo& queryInfo, const DirectChunkArgEvalDesc* pDescs, uint32_t descCnt) {
					if (queryInfo.has_entity_filter_terms())
						return false;

					GAIA_FOR(descCnt) {
						if (!can_use_direct_chunk_term_eval_arg(world, queryInfo, pDescs[i]))
							return false;
					}

					return true;
				}

				template <typename Func>
				void each_direct_entities_iter(
						QueryInfo& queryInfo, std::span<const Entity> entities, Constraints constraints, Func func) {
					auto& world = *queryInfo.world();
					auto& walkData = ensure_each_walk_data();
					Iter it;
					it.init_query_state(&world, constraints, false);
					if (!walkData.cachedRuns.empty()) {
						each_chunk_runs_iter(queryInfo, walkData.cachedRuns, constraints, func);
						return;
					}

					const Archetype* pLastArchetype = nullptr;
					uint8_t indices[ChunkHeader::MAX_COMPONENTS];
					Entity termIds[ChunkHeader::MAX_COMPONENTS];
					for (const auto entity: entities) {
						const auto& ec = ::gaia::ecs::fetch(world, entity);
						init_direct_entity_iter(queryInfo, world, ec, it, indices, termIds, pLastArchetype);
						it.ctx(m_ctx);
						func(it);
						finish_iter_writes(it);
						it.clear_touched_writes();
					}
				}

				//! Runs an iterator-based each() callback over directly seeded entities using one-row chunk views.
				//! \tparam Func Callback type invocable with `Iter&`.
				//! \param queryInfo Prepared query cache and execution metadata.
				//! \param constraints Entity-row constraints to apply.
				//! \param func Callback invoked for each direct-entity iterator.
				//! \see each(Func)
				template <typename Func>
				void each_direct_iter_inter(QueryInfo& queryInfo, Constraints constraints, Func func) {
					auto& world = *queryInfo.world();
					const bool hasWriteTerms = queryInfo.ctx().data.readWriteMask != 0;
					const auto plan = direct_entity_seed_plan(world, queryInfo);

					auto exec_entity = [&](Entity entity) {
						uint8_t indices[ChunkHeader::MAX_COMPONENTS];
						Entity termIds[ChunkHeader::MAX_COMPONENTS];
						Iter it;
						it.set_constraints(constraints);
						init_direct_entity_iter(queryInfo, world, entity, it, indices, termIds);
						it.set_write_im(false);
						it.ctx(m_ctx);
						func(it);
						finish_iter_writes(it);
					};

					if (hasWriteTerms) {
						auto& scratch = direct_query_scratch();
						// Writable callbacks may add local overrides and reshuffle direct-term indices,
						// so direct-seeded execution must iterate a stable snapshot.
						const auto seedInfo = build_direct_entity_seed(world, queryInfo, scratch.entities);
						for (const auto entity: scratch.entities) {
							if (!match_direct_entity_constraints(world, queryInfo, entity, constraints))
								continue;
							if (!match_direct_entity_terms(world, entity, queryInfo, seedInfo))
								continue;
							exec_entity(entity);
						}
						return;
					}

					if (!plan.preferOrSeed) {
						const auto* pSeedTerm = find_direct_all_seed_term(queryInfo, plan);
						if (pSeedTerm != nullptr && can_use_direct_seed_run_cache(world, queryInfo, *pSeedTerm)) {
							DirectEntitySeedInfo seedInfo{};
							seedInfo.seededAllTerm = pSeedTerm->id;
							seedInfo.seededAllMatchKind = pSeedTerm->matchKind;
							seedInfo.seededFromAll = true;
							each_chunk_runs_iter(
									queryInfo, cached_direct_seed_runs(queryInfo, *pSeedTerm, seedInfo, constraints), constraints, func);
							return;
						}
					}

					if (plan.preferOrSeed) {
						for_each_direct_or_union(world, queryInfo, constraints, exec_entity);
						return;
					}

					(void)for_each_direct_all_seed(world, queryInfo, plan, constraints, [&](Entity entity) {
						exec_entity(entity);
						return true;
					});
				}

				//! Runs a typed each() callback over directly seeded entities.
				void each_direct_inter(
						QueryInfo& queryInfo, Constraints constraints, void* pFunc, const TypedQueryExecState& state,
						void (*runDirectChunk)(QueryImpl&, Iter&, void*, const TypedQueryExecState&), bool needsInheritedArgIds,
						void (*invokeInherited)(World&, Entity, const Entity*, void*));

				template <bool UseFilters, typename ContainerOut>
				void arr_inter(QueryInfo& queryInfo, ContainerOut& outArray, Constraints constraints);

			public:
				QueryImpl() = default;
				~QueryImpl() = default;

				QueryImpl(
						World& world, QueryCache& queryCache, ArchetypeId& nextArchetypeId, uint32_t& worldVersion,
						const EntityToArchetypeMap& entityToArchetypeMap,
						const EntityToArchetypeVersionMap& entityToArchetypeMapVersions, const ArchetypeDArray& allArchetypes):
						m_nextArchetypeId(&nextArchetypeId), m_worldVersion(&worldVersion),
						m_entityToArchetypeMap(&entityToArchetypeMap),
						m_entityToArchetypeMapVersions(&entityToArchetypeMapVersions), m_allArchetypes(&allArchetypes) {
					m_storage.init(&world, &queryCache);
				}

#if GAIA_ECS_TEST_HOOKS
				template <typename Func>
				GAIA_NODISCARD QueryPlan test_typed_plan(Func func);

				//! Returns the prepared public iterator plan for test diagnostics.
				//! \param constraints Entity-row subset exposed by the iterator plan.
				//! \return Query execution plan selected for public iterator callbacks.
				GAIA_NODISCARD QueryPlan test_iter_plan(Constraints constraints = Constraints::EnabledOnly);
#endif

				//! Returns the cache handle id of this query.
				//! \return Query id, or QueryIdBad for uncached queries.
				GAIA_NODISCARD QueryId id() const {
					if (!uses_query_cache_storage())
						return QueryIdBad;
					return m_storage.m_identity.handle.id();
				}

				//! Returns the cache handle generation of this query.
				//! \return Query generation, or QueryIdBad for uncached queries.
				GAIA_NODISCARD uint32_t gen() const {
					if (!uses_query_cache_storage())
						return QueryIdBad;
					return m_storage.m_identity.handle.gen();
				}

				//------------------------------------------------

				//! Release any data allocated by the query
				void reset() {
					m_storage.reset();
					m_eachWalkData.reset();
					m_directSeedRunData.reset();
					reset_changed_filter_state();
					invalidate_each_walk_cache();
					invalidate_direct_seed_run_cache();
				}

				//! Destroys the current cached query state and local scratch data.
				void destroy() {
					(void)m_storage.try_del_from_cache();
					m_eachWalkData.reset();
					m_directSeedRunData.reset();
					reset_changed_filter_state();
					invalidate_each_walk_cache();
					invalidate_direct_seed_run_cache();
				}

				//! Returns whether the query is stored in the query cache.
				//! \return True if the query is stored in the query cache. False otherwise.
				GAIA_NODISCARD bool is_cached() const {
					return uses_query_cache_storage() && m_storage.is_cached();
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
				//! Adds one or more query terms described by a string expression.
				//! Component names are resolved immediately while the expression is parsed and the resulting
				//! component ids are baked into the query terms. Later scope, path or alias changes do not
				//! rewrite an already parsed query.
				//! Names in @a str are resolved while add(...) parses the expression and the resulting ids
				//! are baked into the query. Active component scope and lookup-path state affect parsing
				//! only at that moment and do not rewrite the query later.
				//! \param str Null-terminated string with the query expression.
				//! \param ... Optional varargs consumed by `%e` substitutions inside @a str.
				//! \return Reference to this query.
				QueryImpl& add(const char* str, ...) {
					GAIA_ASSERT(str != nullptr);
					if (str == nullptr)
						return *this;

					va_list args{};
					va_start(args, str);

					uint32_t pos = 0;
					uint32_t exp0 = 0;
					uint32_t parentDepth = 0;

					cnt::sarray<util::str_view, MaxVarCnt> varNames{};
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

				//! Adds a prebuilt query input item.
				//! \param item Query term or filter description.
				//! \return Self reference.
				QueryImpl& add(QueryInput item) {
					// Add commands to the command buffer
					add_inter(item);
					return *this;
				}

				//------------------------------------------------

				//! Adds a semantic `Is(entity)` requirement.
				//! \param entity Target entity matched through the Is relation.
				//! \param options Term options.
				//! \return Self reference.
				QueryImpl& is(Entity entity, const QueryTermOptions& options = QueryTermOptions{}) {
					return all(Pair(Is, entity), options);
				}

				//------------------------------------------------

				//! Adds an inherited `in(entity)` requirement.
				//! \param entity Target entity matched through inherited Is traversal.
				//! \param options Term options.
				//! \return Self reference.
				QueryImpl& in(Entity entity, QueryTermOptions options = QueryTermOptions{}) {
					options.in();
					return all(Pair(Is, entity), options);
				}

				//------------------------------------------------

				//! Adds a required entity or pair term.
				//! \param entity Required entity or pair id.
				//! \param options Term options.
				//! \return Self reference.
				QueryImpl& all(Entity entity, const QueryTermOptions& options = QueryTermOptions{}) {
					add_entity_term(QueryOpKind::All, entity, options);
					return *this;
				}

				//! Adds a required typed term.
				//! \tparam T Component or pair type.
				//! \param options Term options.
				//! \return Self reference.
				template <typename T>
				QueryImpl& all(const QueryTermOptions& options);

				//! Adds a required typed term.
				//! \tparam T Component or pair type.
				//! \return Self reference.
				template <typename T>
				QueryImpl& all();

				//------------------------------------------------

				//! Adds an optional entity or pair term.
				//! \param entity Optional entity or pair id.
				//! \param options Term options.
				//! \return Self reference.
				QueryImpl& any(Entity entity, const QueryTermOptions& options = QueryTermOptions{}) {
					add_entity_term(QueryOpKind::Any, entity, options);
					return *this;
				}

				//! Adds an optional typed term.
				//! \tparam T Component or pair type.
				//! \param options Term options.
				//! \return Self reference.
				template <typename T>
				QueryImpl& any(const QueryTermOptions& options);

				//! Adds an optional typed term.
				//! \tparam T Component or pair type.
				//! \return Self reference.
				template <typename T>
				QueryImpl& any();

				//------------------------------------------------

				//! OR terms (at least one has to match).
				//! A single OR term is canonicalized to ALL during query normalization.
				//! \param entity Entity or pair id.
				//! \param options Term options.
				//! \return Self reference.
				QueryImpl& or_(Entity entity, const QueryTermOptions& options = QueryTermOptions{}) {
					add_entity_term(QueryOpKind::Or, entity, options);
					return *this;
				}

				//! Adds an OR typed term.
				//! \tparam T Component or pair type.
				//! \param options Term options.
				//! \return Self reference.
				template <typename T>
				QueryImpl& or_(const QueryTermOptions& options);

				//! Adds an OR typed term.
				//! \tparam T Component or pair type.
				//! \return Self reference.
				template <typename T>
				QueryImpl& or_();

				//------------------------------------------------

				//! Adds an excluded entity or pair term.
				//! \param entity Entity or pair id that must not match.
				//! \param options Term options.
				//! \return Self reference.
				QueryImpl& no(Entity entity, const QueryTermOptions& options = QueryTermOptions{}) {
					add_entity_term(QueryOpKind::Not, entity, options);
					return *this;
				}

				//! Adds an excluded typed term.
				//! \tparam T Component or pair type.
				//! \param options Term options.
				//! \return Self reference.
				template <typename T>
				QueryImpl& no(const QueryTermOptions& options);

				//! Adds an excluded typed term.
				//! \tparam T Component or pair type.
				//! \return Self reference.
				template <typename T>
				QueryImpl& no();

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

				//! Marks a runtime component or pair for changed() filtering.
				//! \param entity Component or pair id to monitor.
				//! \return Self reference.
				QueryImpl& changed(Entity entity) {
					changed_inter(entity);
					return *this;
				}

				//! Marks a typed term for changed() filtering.
				//! \tparam T Component or pair type.
				//! \return Self reference.
				template <typename T>
				QueryImpl& changed();

				//------------------------------------------------

				//! Sorts the query by the specified entity and function.
				//! \param entity The entity to sort by. Use ecs::EntityBad to sort by chunk entities,
				//!               or anything else to sort by components.
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
				QueryImpl& sort_by(TSortByFunc func);

				//! Sorts the query by the specified pair and function.
				//! \tparam Rel The relation to sort by. It is registered if it hasn't been registered yet.
				//! \tparam Tgt The target to sort by. It is registered if it hasn't been registered yet.
				//! \param func The function to use for sorting. Return -1 to put the first entity before the second,
				//!             0 to keep the order, and 1 to put the first entity after the second.
				template <typename Rel, typename Tgt>
				QueryImpl& sort_by(TSortByFunc func);

				//------------------------------------------------

				class OrderByTravView final {
					QueryImpl* m_query = nullptr;
					Entity m_relation = EntityBad;
					TravOrder m_order = TravOrder::Down;

				public:
					//! Creates a query traversal view.
					//! \param query Query used to collect matching entities.
					//! \param relation Relation used to order the matched entities.
					//! \param order Traversal order to apply.
					OrderByTravView(QueryImpl& query, Entity relation, TravOrder order):
							m_query(&query), m_relation(relation), m_order(order) {}

					//! Iterates the query result through the requested relation order.
					//! \tparam Func Callback accepted by Query::each().
					//! \param func Callback invoked for each matched entity or iterator run.
					template <typename Func>
					void each(Func func) {
						m_query->each_walk(func, m_relation, m_order);
					}
				};

				//------------------------------------------------

				//! Iterates matching entities in an explicit relation traversal order.
				//!
				//! Pair(@a relation, X) on entity E means E points at X. Down visits X before E.
				//! Up visits E before X. Reverse variants produce the exact reverse of their base order.
				//! Siblings are tie-broken by entity id, so the order is deterministic. The traversal changes
				//! only visit order; it does not change which entities match the query. This path resolves order
				//! per entity and works for fragmenting relations such as ChildOf and non-fragmenting relations
				//! such as Parent. It may be slower than normal chunk iteration.
				//! \param relation Relation used to order the matched entities.
				//! \param order Traversal order to apply.
				//! \return Lightweight view exposing each(...).
				//! \see depth_order(Entity)
				//! \see group_by(Entity, TGroupByFunc)
				//! \see sort_by(Entity, TSortByFunc)
				GAIA_NODISCARD OrderByTravView order_by(Entity relation, TravOrder order) {
					return OrderByTravView(*this, relation, order);
				}

				//! Iterates matching entities in an explicit typed relation traversal order.
				//! \tparam Rel Relation type. It is registered if it hasn't been registered yet.
				//! \param order Traversal order to apply.
				//! \return Lightweight view exposing each(...).
				//! \see order_by(Entity, TravOrder)
				template <typename Rel>
				GAIA_NODISCARD OrderByTravView order_by(TravOrder order);

				//------------------------------------------------

				//! Orders cached query entries by fragmenting relation depth so iteration runs breadth-first top-down.
				//! Intended only for fragmenting relations such as ChildOf or DependsOn where the target
				//! participates in archetype identity. Unlike order_by(relation, TravOrder), this affects
				//! the cached query iteration order itself and can therefore prune fragmenting disabled subtrees at the
				//! archetype level. For non-fragmenting relations such as Parent, use order_by(...) instead.
				//! \param relation Fragmenting hierarchy relation.
				//! \return Reference to this query builder.
				QueryImpl& depth_order(Entity relation = ChildOf) {
					GAIA_ASSERT(!relation.pair());
					GAIA_ASSERT(world_relation_supports_depth_order(*m_storage.world(), relation));
					group_by_inter(relation, group_by_func_depth_order, true);
					return *this;
				}

				//! Orders cached query entries by fragmenting relation depth so iteration runs breadth-first top-down.
				//! \tparam Rel Fragmenting hierarchy relation, typically ChildOf.
				//! \return Reference to this query builder.
				template <typename Rel>
				QueryImpl& depth_order();

				//------------------------------------------------

				//! Organizes matching archetypes into groups according to the grouping function and entity.
				//! Does not order iteration by group id. Use group_id(...) to filter one group.
				//! Use depth_order(...) or sort_by(...) when iteration order matters.
				//! \param entity The entity to group by.
				//! \param func The function to use for grouping. Returns a GroupId to group the entities by.
				//! \return Reference to this query builder.
				QueryImpl& group_by(Entity entity, TGroupByFunc func = group_by_func_default) {
					group_by_inter(entity, func);
					return *this;
				}

				//! Organizes matching archetypes into groups according to the grouping function.
				//! Does not order iteration by group id. Use group_id(...) to filter one group.
				//! Use depth_order(...) or sort_by(...) when iteration order matters.
				//! \tparam T Component to group by. It is registered if it hasn't been registered yet.
				//! \param func The function to use for grouping. Returns a GroupId to group the entities by.
				//! \return Reference to this query builder.
				template <typename T>
				QueryImpl& group_by(TGroupByFunc func = group_by_func_default);

				//! Organizes matching archetypes into groups according to the grouping function.
				//! Does not order iteration by group id. Use group_id(...) to filter one group.
				//! Use depth_order(...) or sort_by(...) when iteration order matters.
				//! \tparam Rel The relation to group by. It is registered if it hasn't been registered yet.
				//! \tparam Tgt The target to group by. It is registered if it hasn't been registered yet.
				//! \param func The function to use for grouping. Returns a GroupId to group the entities by.
				//! \return Reference to this query builder.
				template <typename Rel, typename Tgt>
				QueryImpl& group_by(TGroupByFunc func = group_by_func_default);

				//------------------------------------------------

				//! Declares an explicit relation dependency for grouped cache invalidation.
				//! Useful for custom group_by callbacks that depend on hierarchy or relation topology.
				//! \param relation Relation the group depends on.
				QueryImpl& group_dep(Entity relation) {
					group_dep_inter(relation);
					return *this;
				}

				//! Declares an explicit relation dependency for grouped cache invalidation.
				//! Useful for custom group_by callbacks that depend on hierarchy or relation topology.
				//! \tparam Rel Relation the group depends on.
				template <typename Rel>
				QueryImpl& group_dep();

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
				QueryImpl& group_id();

				//! Collects active non-zero group ids for a grouped query.
				//! The ids can be fed back to group_id(...) to process each group without knowing group ids ahead of time.
				//! \tparam Container Container with GroupId elements.
				//! \param out Output array overwritten with unique group ids.
				//! \param sortGroups True to sort grouped cache ranges by group id before collecting ids.
				template <typename Container>
				void groups(Container& out, bool sortGroups) {
					auto& queryInfo = fetch();
					match_all(queryInfo);
					queryInfo.group_ids(out, sortGroups);
				}

				//------------------------------------------------

				//! Adds a query execution job without submitting it.
				//!
				//! The returned SchedJob is backed by the world's ECS scheduler descriptor rather than a
				//! gaia::mt::JobHandle, so external schedulers can provide their own token, submission,
				//! dependency, wait, and cleanup behavior. The job owns only the callback copy and scheduler
				//! token; the query and world must outlive the job.
				//! \tparam Func Query callback type accepted by each().
				//! \param func Callback invoked when the added job runs.
				//! \param execType Query execution mode used inside the job.
				//! \return Move-only scheduler job wrapper for the added query execution.
				//! \warning Structural mutations that invalidate this query while the job is pending are not safe.
				//! \see SchedJob
				//! \see each(Func, QueryExecType)
				template <typename Func>
				GAIA_NODISCARD SchedJob job(Func func, QueryExecType execType) {
					if constexpr (detail::is_query_iter_callback_v<Func>) {
						switch (execType) {
							case QueryExecType::Parallel:
								return add_iter_parallel_job<Func, QueryExecType::Parallel>(GAIA_MOV(func));
							case QueryExecType::ParallelPerf:
								return add_iter_parallel_job<Func, QueryExecType::ParallelPerf>(GAIA_MOV(func));
							case QueryExecType::ParallelEff:
								return add_iter_parallel_job<Func, QueryExecType::ParallelEff>(GAIA_MOV(func));
							default:
								break;
						}
					} else {
						switch (execType) {
							case QueryExecType::Parallel:
								return add_iter_parallel_job<TypedJobCallback<Func>, QueryExecType::Parallel>(
										TypedJobCallback<Func>{this, GAIA_MOV(func)});
							case QueryExecType::ParallelPerf:
								return add_iter_parallel_job<TypedJobCallback<Func>, QueryExecType::ParallelPerf>(
										TypedJobCallback<Func>{this, GAIA_MOV(func)});
							case QueryExecType::ParallelEff:
								return add_iter_parallel_job<TypedJobCallback<Func>, QueryExecType::ParallelEff>(
										TypedJobCallback<Func>{this, GAIA_MOV(func)});
							default:
								break;
						}
					}

					return add_query_task_job(GAIA_MOV(func), execType);
				}

				//! Iterates query matches using the default execution mode.
				//! \tparam Func Iterator callback type invocable with `Iter&`.
				//! \param func Callable invoked for each match.
				//! \see Iter::ctx() const
				template <typename Func, std::enable_if_t<detail::is_query_iter_callback_v<Func>, int> = 0>
				void each(Func func) {
					each_runtime_inter<QueryExecType::Default, Func>(func, Constraints::EnabledOnly);
				}

				template <typename Func, std::enable_if_t<!detail::is_query_iter_callback_v<Func>, int> = 0>
				void each(Func func);

				//! Iterates query matches using the selected execution mode.
				//! \tparam Func Iterator callback type invocable with `Iter&`.
				//! \param func Callable invoked for each match.
				//! \param execType Execution mode.
				//! \see Iter::ctx() const
				template <typename Func, std::enable_if_t<detail::is_query_iter_callback_v<Func>, int> = 0>
				void each(Func func, QueryExecType execType) {
					each(func, execType, Constraints::EnabledOnly);
				}

				template <typename Func, std::enable_if_t<detail::is_query_iter_callback_v<Func>, int> = 0>
				void each(Func func, Constraints constraints) {
					each(func, QueryExecType::Default, constraints);
				}

				template <typename Func, std::enable_if_t<detail::is_query_iter_callback_v<Func>, int> = 0>
				void each(Func func, QueryExecType execType, Constraints constraints) {
					switch (execType) {
						case QueryExecType::Parallel:
							each_runtime_inter<QueryExecType::Parallel, Func>(func, constraints);
							break;
						case QueryExecType::ParallelPerf:
							each_runtime_inter<QueryExecType::ParallelPerf, Func>(func, constraints);
							break;
						case QueryExecType::ParallelEff:
							each_runtime_inter<QueryExecType::ParallelEff, Func>(func, constraints);
							break;
						default:
							each_runtime_inter<QueryExecType::Default, Func>(func, constraints);
							break;
					}
				}

				template <typename Func, std::enable_if_t<!detail::is_query_iter_callback_v<Func>, int> = 0>
				void each(Func func, QueryExecType execType);

				//! Runs a typed callback against an already prepared iterator.
				//! This is used by higher-level adapters that normalize execution to `Iter&` first and
				//! then materialize typed arguments on top of the iterator.
				//! \tparam Func Callback type.
				//! \param it Iterator positioned on the current chunk range.
				//! \param func Callback to invoke.
				template <typename Func>
				void each_iter(Iter& it, Func func);

				void each_iter_erased(
						QueryExecType execType, void* pFunc, const TypedQueryExecState& state,
						void (*runDirectFastChunk)(QueryImpl&, Iter&, void*, const TypedQueryExecState&),
						void (*runMappedChunk)(QueryImpl&, const QueryInfo&, Iter&, void*, const TypedQueryExecState&));

				void each_iter_erased(
						Iter& it, void* pFunc, const TypedQueryExecState& state,
						void (*runDirectFastChunk)(QueryImpl&, Iter&, void*, const TypedQueryExecState&),
						void (*runMappedChunk)(QueryImpl&, const QueryInfo&, Iter&, void*, const TypedQueryExecState&));

				//------------------------------------------------

				//! Iterates matching archetypes instead of individual entities.
				//! \tparam Func Iterator callback type invocable with `Iter&`.
				//! \param func Callable invoked for each matching archetype iterator.
				//! \param constraints Iteration constraints applied before invoking @a func.
				//! \see Iter::ctx() const
				template <typename Func>
				void each_arch(Func func, Constraints constraints = Constraints::EnabledOnly) {
					auto& queryInfo = fetch();
					match_all(queryInfo);
					run_query_on_archetypes<QueryExecType::Default>(
							queryInfo,
							[&](Iter& it) {
								GAIA_PROF_SCOPE(query_func_a);
								it.ctx(m_ctx);
								func(it);
							},
							constraints);
				}

				//------------------------------------------------

				//!	Returns true or false depending on whether there are any entities matching the query.
				//!	\warning Only use if you only care if there are any entities matching the query.
				//!					 The result is not cached and repeated calls to the function might be slow.
				//!					 If you already called arr(), checking if it is empty is preferred.
				//!					 Use empty() instead of calling count()==0.
				//! \note For changed() queries this is a non-consuming probe. It does not advance the
				//!       query's changed-reporting state. Iteration APIs such as each()/arr() do consume it.
				//!	\return True if there are any entities matching the query. False otherwise.
				bool empty(Constraints constraints = Constraints::EnabledOnly) {
					auto& queryInfo = fetch();
					if (!queryInfo.has_filters() && m_groupIdSet == 0 && can_use_direct_entity_seed_eval(queryInfo)) {
						return empty_inter<false>(queryInfo, constraints);
					}

					match_all(queryInfo);

					const bool hasFilters = queryInfo.has_filters();
					if (hasFilters) {
						return empty_inter<true>(queryInfo, constraints);
					} else {
						return empty_inter<false>(queryInfo, constraints);
					}
				}

				//! Calculates the number of entities matching the query
				//! \warning Only use if you only care about the number of entities matching the query.
				//!          The result is not cached and repeated calls to the function might be slow.If you already called
				//!          arr(), use the size provided by the array.Use empty() instead of calling count() == 0.
				//! \note For changed() queries this is a non-consuming probe. It does not advance the
				//!       query's changed-reporting state. Iteration APIs such as each()/arr() do consume it.
				//! \return The number of matching entities
				uint32_t count(Constraints constraints = Constraints::EnabledOnly) {
					auto& queryInfo = fetch();
					if (!queryInfo.has_filters() && m_groupIdSet == 0 && can_use_direct_entity_seed_eval(queryInfo)) {
						return count_inter<false>(queryInfo, constraints);
					}

					match_all(queryInfo);

					const bool hasFilters = queryInfo.has_filters();
					return hasFilters ? count_inter<true>(queryInfo, constraints) : count_inter<false>(queryInfo, constraints);
				}

				//! Iterates matching enabled entities through a non-template erased callback.
				//! \param pCtx User callback context.
				//! \param func Callback invoked for each matching entity.
				void each_entity_enabled(void* pCtx, void (*func)(void*, Entity)) {
					auto& queryInfo = fetch();
					match_all(queryInfo);
					::gaia::ecs::update_version(*m_worldVersion);

					if (!queryInfo.has_filters() && m_groupIdSet == 0 && can_use_direct_entity_seed_eval(queryInfo)) {
						auto& world = *queryInfo.world();
						if (has_only_direct_or_terms(queryInfo)) {
							for_each_direct_or_union(world, queryInfo, Constraints::EnabledOnly, [&](Entity entity) {
								func(pCtx, entity);
								return true;
							});
						} else {
							const auto plan = direct_entity_seed_plan(world, queryInfo);
							(void)for_each_direct_all_seed(world, queryInfo, plan, Constraints::EnabledOnly, [&](Entity entity) {
								func(pCtx, entity);
								return true;
							});
						}

						m_changedWorldVersion = *m_worldVersion;
						return;
					}

					const bool hasFilters = queryInfo.has_filters();
					const bool hasEntityFilters = queryInfo.has_entity_filter_terms();
					const auto cacheView = queryInfo.cache_archetype_view();
					const bool needsBarrierCache = needs_depth_order_hierarchy_barrier_cache(queryInfo, Constraints::EnabledOnly);
					if (needsBarrierCache)
						queryInfo.ensure_depth_order_hierarchy_barrier_cache();

					const auto cacheRange = selected_query_cache_range(queryInfo);
					if (!cacheRange.valid) {
						m_changedWorldVersion = *m_worldVersion;
						return;
					}
					const auto idxFrom = cacheRange.idxFrom;
					const auto idxTo = cacheRange.idxTo;

					Iter it;
					it.init_query_state(queryInfo.world(), Constraints::EnabledOnly, false);
					for (uint32_t qi = idxFrom; qi < idxTo; ++qi) {
						const auto* pArchetype = cacheView[qi];
						const bool barrierPasses = !needsBarrierCache || queryInfo.barrier_passes(qi);
						if GAIA_UNLIKELY (!can_process_archetype_inter(
																	queryInfo, *pArchetype, Constraints::EnabledOnly, barrierPasses))
							continue;

						const auto& chunks = pArchetype->chunks();
						if (!hasEntityFilters) {
							for (auto* pChunk: chunks) {
								const auto from = Iter::start_index(pChunk);
								const auto to = Iter::end_index(pChunk);
								if (from == to)
									continue;
								if (hasFilters && !match_filters(*pChunk, queryInfo, m_changedWorldVersion))
									continue;

								const auto entityCnt = (uint32_t)(to - from);
								const auto entities = pChunk->entity_view();
								GAIA_FOR(entityCnt) {
									func(pCtx, entities[from + i]);
								}
							}
							continue;
						}

						it.set_archetype(pArchetype);
						for (auto* pChunk: chunks) {
							it.set_chunk(pChunk);
							const auto entityCnt = it.size();
							if (entityCnt == 0)
								continue;
							if (hasFilters && !match_filters(*pChunk, queryInfo, m_changedWorldVersion))
								continue;

							const auto entities = it.view<Entity>();
							GAIA_FOR(entityCnt) {
								if (match_entity_filters(*queryInfo.world(), entities[i], queryInfo))
									func(pCtx, entities[i]);
							}
						}
					}

					m_changedWorldVersion = *m_worldVersion;
				}

				void collect_entities_enabled(cnt::darray<Entity>& out) {
					auto& queryInfo = fetch();
					match_all(queryInfo);
					::gaia::ecs::update_version(*m_worldVersion);

					if (!queryInfo.has_filters() && m_groupIdSet == 0 && can_use_direct_entity_seed_eval(queryInfo)) {
						auto& world = *queryInfo.world();
						if (has_only_direct_or_terms(queryInfo)) {
							for_each_direct_or_union(world, queryInfo, Constraints::EnabledOnly, [&](Entity entity) {
								out.push_back(entity);
								return true;
							});
						} else {
							const auto plan = direct_entity_seed_plan(world, queryInfo);
							(void)for_each_direct_all_seed(world, queryInfo, plan, Constraints::EnabledOnly, [&](Entity entity) {
								out.push_back(entity);
								return true;
							});
						}

						m_changedWorldVersion = *m_worldVersion;
						return;
					}

					const bool hasFilters = queryInfo.has_filters();
					const bool hasEntityFilters = queryInfo.has_entity_filter_terms();
					const auto cacheView = queryInfo.cache_archetype_view();
					const bool needsBarrierCache = needs_depth_order_hierarchy_barrier_cache(queryInfo, Constraints::EnabledOnly);
					if (needsBarrierCache)
						queryInfo.ensure_depth_order_hierarchy_barrier_cache();

					const auto cacheRange = selected_query_cache_range(queryInfo);
					if (!cacheRange.valid) {
						m_changedWorldVersion = *m_worldVersion;
						return;
					}
					const auto idxFrom = cacheRange.idxFrom;
					const auto idxTo = cacheRange.idxTo;

					Iter it;
					it.init_query_state(queryInfo.world(), Constraints::EnabledOnly, false);
					for (uint32_t qi = idxFrom; qi < idxTo; ++qi) {
						const auto* pArchetype = cacheView[qi];
						const bool barrierPasses = !needsBarrierCache || queryInfo.barrier_passes(qi);
						if GAIA_UNLIKELY (!can_process_archetype_inter(
																	queryInfo, *pArchetype, Constraints::EnabledOnly, barrierPasses))
							continue;

						const auto& chunks = pArchetype->chunks();
						if (!hasEntityFilters) {
							for (auto* pChunk: chunks) {
								const auto from = Iter::start_index(pChunk);
								const auto to = Iter::end_index(pChunk);
								if (from == to)
									continue;
								if (hasFilters && !match_filters(*pChunk, queryInfo, m_changedWorldVersion))
									continue;

								const auto oldSize = out.size();
								const auto entityCnt = (uint32_t)(to - from);
								const auto entities = pChunk->entity_view();
								out.resize(oldSize + entityCnt);
								GAIA_FOR(entityCnt) {
									out[oldSize + i] = entities[from + i];
								}
							}
							continue;
						}

						it.set_archetype(pArchetype);
						for (auto* pChunk: chunks) {
							it.set_chunk(pChunk);
							const auto entityCnt = it.size();
							if (entityCnt == 0)
								continue;
							if (hasFilters && !match_filters(*pChunk, queryInfo, m_changedWorldVersion))
								continue;

							const auto entities = it.view<Entity>();
							GAIA_FOR(entityCnt) {
								if (match_entity_filters(*queryInfo.world(), entities[i], queryInfo))
									out.push_back(entities[i]);
							}
						}
					}

					m_changedWorldVersion = *m_worldVersion;
				}

				//! Appends all components or entities matching the query to the output array
				//! \tparam Container Container type
				//! \param[out] outArray Container storing entities or components
				//! \param constraints QueryImpl constraints
				template <typename Container>
				void arr(Container& outArray, Constraints constraints = Constraints::EnabledOnly);

				//! Builds and caches relation traversal order for the current query result.
				//! \param queryInfo Query info
				//! \param relation Dependency relation used for traversal.
				//! \param order Traversal order to apply.
				//! \param constraints Match constraints applied before ordering.
				//! \return Ordered view of matched entities.
				GAIA_NODISCARD std::span<const Entity> ordered_entities_walk(
						QueryInfo& queryInfo, Entity relation, TravOrder order,
						Constraints constraints = Constraints::EnabledOnly) {
					struct OrderedWalkTargetCtx {
						const cnt::darray<Entity>* pEntities = nullptr;
						uint32_t cnt = 0;
						uint32_t dependentIdx = 0;
						cnt::darray<uint32_t>* pIndegree = nullptr;
						cnt::darray<uint32_t>* pOutdegree = nullptr;
						cnt::darray<uint32_t>* pWriteCursor = nullptr;
						cnt::darray<uint32_t>* pEdges = nullptr;
						uint32_t* pEdgeCnt = nullptr;

						GAIA_NODISCARD static uint32_t
						find_entity_idx(const cnt::darray<Entity>& entities, uint32_t cnt, Entity entity) {
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
						}

						static void count_edge(void* rawCtx, Entity dependency) {
							auto& ctx = *static_cast<OrderedWalkTargetCtx*>(rawCtx);
							const auto dependencyIdx = find_entity_idx(*ctx.pEntities, ctx.cnt, dependency);
							if (dependencyIdx == ctx.cnt || dependencyIdx == ctx.dependentIdx)
								return;

							++(*ctx.pOutdegree)[dependencyIdx];
							++(*ctx.pIndegree)[ctx.dependentIdx];
							++*ctx.pEdgeCnt;
						}

						static void write_edge(void* rawCtx, Entity dependency) {
							auto& ctx = *static_cast<OrderedWalkTargetCtx*>(rawCtx);
							const auto dependencyIdx = find_entity_idx(*ctx.pEntities, ctx.cnt, dependency);
							if (dependencyIdx == ctx.cnt || dependencyIdx == ctx.dependentIdx)
								return;

							(*ctx.pEdges)[(*ctx.pWriteCursor)[dependencyIdx]++] = ctx.dependentIdx;
						}
					};

					auto& walkData = ensure_each_walk_data();
					auto& world = *m_storage.world();
					const uint32_t relationVersion = world_rel_version(world, relation);
					const uint32_t worldVersion = ::gaia::ecs::world_version(world);
					const uint32_t resultCacheRevision = queryInfo.result_cache_rev();

					const bool needsTraversalBarrierState =
							constraints == Constraints::EnabledOnly && ::gaia::ecs::valid(world, relation);
					auto survives_disabled_barrier = [&](Entity entity) {
						if (!needsTraversalBarrierState)
							return true;

						auto curr = entity;
						GAIA_FOR(MAX_TRAV_DEPTH) {
							const auto next = target(world, curr, relation);
							if (next == EntityBad || next == curr)
								break;
							if (!world_entity_enabled(world, next))
								return false;
							curr = next;
						}

						return true;
					};

					if (walkData.cacheValid && walkData.cachedRelation == relation && walkData.cachedOrder == order &&
							walkData.cachedConstraints == constraints && walkData.cachedRelationVersion == relationVersion &&
							walkData.cachedEntityVersion == worldVersion &&
							walkData.cachedResultCacheRevision == resultCacheRevision && !queryInfo.has_filters()) {
						return std::span<const Entity>(walkData.cachedOutput.data(), walkData.cachedOutput.size());
					}

					if (walkData.cacheValid && walkData.cachedRelation == relation && walkData.cachedOrder == order &&
							walkData.cachedConstraints == constraints && walkData.cachedRelationVersion == relationVersion &&
							(!needsTraversalBarrierState || walkData.cachedEntityVersion == worldVersion) &&
							!queryInfo.has_filters()) {
						auto& chunks = walkData.scratchChunks;
						chunks.clear();

						bool chunkChanged = false;
						for (auto* pArchetype: queryInfo) {
							if (pArchetype == nullptr || !can_process_archetype(queryInfo, *pArchetype))
								continue;

							for (const auto* pChunk: pArchetype->chunks()) {
								if (pChunk == nullptr)
									continue;

								chunks.push_back(pChunk);
								if (!chunkChanged && pChunk->changed(walkData.cachedEntityVersion))
									chunkChanged = true;
							}
						}

						bool sameChunks = chunks.size() == walkData.cachedChunks.size();
						if (sameChunks) {
							for (uint32_t i = 0; i < (uint32_t)chunks.size(); ++i) {
								if (chunks[i] != walkData.cachedChunks[i]) {
									sameChunks = false;
									break;
								}
							}
						}

						if (sameChunks && !chunkChanged) {
							return std::span<const Entity>(walkData.cachedOutput.data(), walkData.cachedOutput.size());
						}
					}

					auto& entities = walkData.scratchEntities;
					entities.clear();
					arr(entities, constraints);
					if (entities.empty())
						return {};

					if (needsTraversalBarrierState) {
						uint32_t writeIdx = 0;
						const auto cnt = (uint32_t)entities.size();
						GAIA_FOR(cnt) {
							const auto entity = entities[i];
							if (!survives_disabled_barrier(entity))
								continue;
							entities[writeIdx++] = entity;
						}
						entities.resize(writeIdx);
						if (entities.empty())
							return {};
					}

					if (walkData.cacheValid && walkData.cachedRelation == relation && walkData.cachedOrder == order &&
							walkData.cachedConstraints == constraints && walkData.cachedRelationVersion == relationVersion &&
							(!needsTraversalBarrierState || walkData.cachedEntityVersion == worldVersion) &&
							entities.size() == walkData.cachedInput.size()) {
						bool sameInput = true;
						for (uint32_t i = 0; i < (uint32_t)entities.size(); ++i) {
							if (entities[i] != walkData.cachedInput[i]) {
								sameInput = false;
								break;
							}
						}

						if (sameInput) {
							return std::span<const Entity>(walkData.cachedOutput.data(), walkData.cachedOutput.size());
						}
					}

					auto& ordered = walkData.cachedOutput;
					walkData.cachedInput = entities;
					ordered.clear();
					if (!::gaia::ecs::valid(world, relation)) {
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

						auto& indegree = walkData.scratchIndegree;
						indegree.resize(cnt);
						auto& outdegree = walkData.scratchOutdegree;
						outdegree.resize(cnt);
						for (uint32_t i = 0; i < cnt; ++i) {
							indegree[i] = 0;
							outdegree[i] = 0;
						}

						uint32_t edgeCnt = 0;
						OrderedWalkTargetCtx edgeCtx;
						edgeCtx.pEntities = &entities;
						edgeCtx.cnt = cnt;
						edgeCtx.pIndegree = &indegree;
						edgeCtx.pOutdegree = &outdegree;
						edgeCtx.pEdgeCnt = &edgeCnt;
						for (uint32_t dependentIdx = 0; dependentIdx < cnt; ++dependentIdx) {
							const auto dependent = entities[dependentIdx];
							edgeCtx.dependentIdx = dependentIdx;
							world_for_each_target(world, dependent, relation, &edgeCtx, &OrderedWalkTargetCtx::count_edge);
						}

						auto& offsets = walkData.scratchOffsets;
						offsets.resize(cnt + 1);
						offsets[0] = 0;
						for (uint32_t i = 0; i < cnt; ++i)
							offsets[i + 1] = offsets[i] + outdegree[i];

						auto& writeCursor = walkData.scratchWriteCursor;
						writeCursor.resize(cnt);
						for (uint32_t i = 0; i < cnt; ++i)
							writeCursor[i] = offsets[i];

						auto& edges = walkData.scratchEdges;
						edges.resize(edgeCnt);
						edgeCtx.pWriteCursor = &writeCursor;
						edgeCtx.pEdges = &edges;
						for (uint32_t dependentIdx = 0; dependentIdx < cnt; ++dependentIdx) {
							const auto dependent = entities[dependentIdx];
							edgeCtx.dependentIdx = dependentIdx;
							world_for_each_target(world, dependent, relation, &edgeCtx, &OrderedWalkTargetCtx::write_edge);
						}

						ordered.reserve(cnt);

						const bool isUp = order == TravOrder::Up || order == TravOrder::ReverseUp;
						const bool needsReverse = order == TravOrder::ReverseUp || order == TravOrder::ReverseDown;

						auto& visited = walkData.scratchWriteCursor;
						visited.resize(cnt);
						for (uint32_t i = 0; i < cnt; ++i)
							visited[i] = 0;

						auto& stack = walkData.scratchCurrLevel;
						stack.clear();
						auto& cursorStack = walkData.scratchNextLevel;
						cursorStack.clear();

						auto append_from_root = [&](uint32_t rootIdx) {
							stack.push_back(rootIdx);
							cursorStack.push_back(offsets[rootIdx]);

							while (!stack.empty()) {
								const auto idx = stack.back();
								if (visited[idx] == 0) {
									visited[idx] = 1;
									if (!isUp)
										ordered.push_back(entities[idx]);
								}

								bool pushedChild = false;
								auto& cursor = cursorStack.back();
								while (cursor < offsets[idx + 1]) {
									const auto childIdx = edges[cursor++];
									if (visited[childIdx] != 0)
										continue;

									stack.push_back(childIdx);
									cursorStack.push_back(offsets[childIdx]);
									pushedChild = true;
									break;
								}

								if (pushedChild)
									continue;

								if (isUp)
									ordered.push_back(entities[idx]);
								visited[idx] = 2;
								stack.pop_back();
								cursorStack.pop_back();
							}
						};

						for (uint32_t i = 0; i < cnt; ++i) {
							if (indegree[i] == 0 && visited[i] == 0)
								append_from_root(i);
						}

						// Cycles are invalid relationship data, but keep traversal deterministic by visiting leftovers by id.
						for (uint32_t i = 0; i < cnt; ++i) {
							if (visited[i] == 0)
								append_from_root(i);
						}

						if (needsReverse) {
							const auto orderedCnt = (uint32_t)ordered.size();
							for (uint32_t i = 0; i < orderedCnt / 2; ++i)
								core::swap(ordered[i], ordered[orderedCnt - i - 1]);
						}
					}

					walkData.cachedRelation = relation;
					walkData.cachedOrder = order;
					walkData.cachedConstraints = constraints;
					walkData.cachedRelationVersion = relationVersion;
					walkData.cachedEntityVersion = ::gaia::ecs::world_version(world);
					walkData.cachedResultCacheRevision = resultCacheRevision;
					walkData.cachedRuns.clear();

					{
						const auto orderedCnt = (uint32_t)ordered.size();
						if (orderedCnt != 0) {
							for (uint32_t i = 0; i < orderedCnt; ++i) {
								const auto& ec = ::gaia::ecs::fetch(world, ordered[i]);
								if (walkData.cachedRuns.empty()) {
									walkData.cachedRuns.push_back({ec.pArchetype, ec.pChunk, ec.row, (uint16_t)(ec.row + 1), i});
									continue;
								}

								auto& run = walkData.cachedRuns.back();
								if (ec.pChunk == run.pChunk && ec.row == run.to) {
									run.to = (uint16_t)(run.to + 1);
								} else {
									walkData.cachedRuns.push_back({ec.pArchetype, ec.pChunk, ec.row, (uint16_t)(ec.row + 1), i});
								}
							}
						}
					}

					if (!queryInfo.has_filters()) {
						auto& chunks = walkData.scratchChunks;
						chunks.clear();
						for (auto* pArchetype: queryInfo) {
							if (pArchetype == nullptr || !can_process_archetype(queryInfo, *pArchetype))
								continue;

							for (const auto* pChunk: pArchetype->chunks()) {
								if (pChunk == nullptr)
									continue;
								chunks.push_back(pChunk);
							}
						}
						walkData.cachedChunks = chunks;
					} else
						walkData.cachedChunks.clear();
					walkData.cacheValid = true;

					return std::span<const Entity>(walkData.cachedOutput.data(), walkData.cachedOutput.size());
				}

				//! Iterates entities matching the query in a requested relation traversal order.
				//! For relation R this treats Pair(R, X) on entity E as "E points at X".
				//! Traversal changes only visit order. Nodes with the same parent are ordered by entity id.
				//! \tparam Func Callback type. May accept `Iter&` or an entity value.
				//! \param func Callable invoked for each ordered entity.
				//! \param relation Relation used to order matched entities.
				//! \param order Traversal order to apply.
				//! \param constraints QueryImpl constraints
				//! \see ordered_entities_walk(QueryInfo&, Entity, TravOrder, Constraints)
				template <typename Func, std::enable_if_t<detail::is_query_walk_core_callback_v<Func>, int> = 0>
				void each_walk(
						Func func, Entity relation, TravOrder order = TravOrder::Down,
						Constraints constraints = Constraints::EnabledOnly) {
					auto& queryInfo = fetch();
					match_all(queryInfo);
					const auto ordered = ordered_entities_walk(queryInfo, relation, order, constraints);

					if constexpr (std::is_invocable_v<Func, Iter&>) {
						each_direct_entities_iter(queryInfo, ordered, constraints, func);
					} else if constexpr (std::is_invocable_v<Func, const Entity&> || std::is_invocable_v<Func, Entity>) {
						for (const auto entity: ordered)
							func(entity);
					}
				}

				template <typename Func, std::enable_if_t<!detail::is_query_walk_core_callback_v<Func>, int> = 0>
				void each_walk(
						Func func, Entity relation, TravOrder order = TravOrder::Down,
						Constraints constraints = Constraints::EnabledOnly);

				//------------------------------------------------

				//! Run diagnostics
				void diag() {
					// Make sure matching happened
					auto& queryInfo = fetch();
					match_all(queryInfo);
					if (uses_shared_cache_layer())
						GAIA_LOG_N("BEG DIAG Query %u.%u [S]", id(), gen());
					else if (uses_query_cache_storage())
						GAIA_LOG_N("BEG DIAG Query %u.%u [L]", id(), gen());
					else
						GAIA_LOG_N("BEG DIAG Query [U]");
					for (const auto* pArchetype: queryInfo)
						Archetype::diag_basic_info(*m_storage.world(), *pArchetype);
					GAIA_LOG_N("END DIAG Query");
				}

				//! Returns a textual dump of the generated query VM bytecode.
				//! \return Bytecode dump.
				GAIA_NODISCARD util::str bytecode() {
					auto& queryInfo = fetch();
					return queryInfo.bytecode();
				}

				//! Prints a textual dump of the generated query VM bytecode.
				void diag_bytecode() {
					const auto dump = bytecode();
					if (uses_shared_cache_layer())
						GAIA_LOG_N("BEG DIAG Query Bytecode %u.%u [S]", id(), gen());
					else if (uses_query_cache_storage())
						GAIA_LOG_N("BEG DIAG Query Bytecode %u.%u [L]", id(), gen());
					else
						GAIA_LOG_N("BEG DIAG Query Bytecode [U]");
					GAIA_LOG_N("%.*s", (int)dump.size(), dump.data());
					GAIA_LOG_N("END DIAG Query");
				}
			};
		} // namespace detail

		using Query = detail::QueryImpl;
	} // namespace ecs
} // namespace gaia

#include "gaia/ecs/query_builder_typed.inl"
#include "gaia/ecs/query_typed.inl"
