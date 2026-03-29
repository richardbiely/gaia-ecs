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
		void world_finish_write(World& world, Entity term, Entity entity);

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

		enum class QueryCacheKind : uint8_t {
			// Disable result caching. The query keeps its immutable compiled plan locally
			// and rebuilds transient results on demand.
			None,
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

		enum class QueryCacheScope : uint8_t {
			// Keep cached query state private to this query instance and its copies.
			Local,
			// Allow identical query shapes to share one cached query state through the world cache.
			Shared
		};

		using QueryCachePolicy = QueryCtx::CachePolicy;

		namespace detail {
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

				void exec(QueryCtx& ctx) const {
					auto& ctxData = ctx.data;
					ctxData.groupBy = groupBy;
					GAIA_ASSERT(func != nullptr);
					ctxData.groupByFunc = func; // group_by_func_default;
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

				GAIA_NODISCARD World* world() {
					return m_world;
				}

				GAIA_NODISCARD QuerySerBuffer& ser_buffer() {
					return m_identity.ser_buffer(m_world);
				}
				void ser_buffer_reset() {
					return m_identity.ser_buffer_reset(m_world);
				}

				void init(World* world, QueryCache* queryCache) {
					m_world = world;
					m_pCache = queryCache;
					m_pInfo = nullptr;
				}

				//! Release any data allocated by the query
				void reset() {
					if (auto* pInfo = try_query_info_fast(); pInfo != nullptr)
						pInfo->reset();
					if (m_pOwnedInfo != nullptr)
						m_pOwnedInfo->reset();
				}

				void allow_to_destroy_again() {
					m_destroyed = false;
				}

				//! Try delete the query from query cache
				GAIA_NODISCARD bool try_del_from_cache() {
					if (!m_destroyed && m_identity.handle.id() != QueryIdBad)
						m_pCache->del(m_identity.handle);

					// Don't allow multiple calls to destroy to break the reference counter.
					// One object is only allowed to destroy once.
					m_pInfo = nullptr;
					m_destroyed = true;
					return false;
				}

				//! Invalidates the query handle
				void invalidate() {
					m_pInfo = nullptr;
					m_identity.handle = {};
					delete m_pOwnedInfo;
					m_pOwnedInfo = nullptr;
				}

				GAIA_NODISCARD QueryInfo* try_query_info_fast() const {
					if (m_pInfo == nullptr || m_identity.handle.id() == QueryIdBad || m_pCache == nullptr)
						return nullptr;

					auto* pInfo = m_pCache->try_get(m_identity.handle);
					return pInfo == m_pInfo ? pInfo : nullptr;
				}

				void cache_query_info(QueryInfo& queryInfo) {
					m_pInfo = &queryInfo;
				}

				GAIA_NODISCARD bool has_owned_query_info() const {
					return m_pOwnedInfo != nullptr;
				}

				GAIA_NODISCARD QueryInfo& owned_query_info() {
					GAIA_ASSERT(m_pOwnedInfo != nullptr);
					return *m_pOwnedInfo;
				}

				void init_owned_query_info(QueryInfo&& queryInfo) {
					if (m_pOwnedInfo == nullptr)
						m_pOwnedInfo = new QueryInfo(GAIA_MOV(queryInfo));
					else
						*m_pOwnedInfo = GAIA_MOV(queryInfo);
				}

				//! Returns true if the query is found in the query cache.
				GAIA_NODISCARD bool is_cached() const {
					auto* pInfo = try_query_info_fast();
					if (pInfo == nullptr)
						pInfo = m_pCache->try_get(m_identity.handle);
					return pInfo != nullptr;
				}

				//! Returns true if the query is ready to be used.
				GAIA_NODISCARD bool is_initialized() const {
					return m_world != nullptr && m_pCache != nullptr;
				}
			};
			class QueryImpl {
				static constexpr uint32_t ChunkBatchSize = 32;

				struct ChunkBatch {
					const Archetype* pArchetype;
					Chunk* pChunk;
					const uint8_t* pCompIndices;
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
					scratch.counts.resize(newSize, 0);
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
					//! Cached constraints used by each_walk.
					Constraints cachedConstraints = Constraints::EnabledOnly;
					//! Relation version when each_walk cache was produced.
					uint32_t cachedRelationVersion = 0;
					//! World version snapshot used for chunk-structural change checks.
					uint32_t cachedEntityVersion = 0;
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

				//! Optional on-demand storage for walk iteration data.
				struct EachWalkDataHolder {
					EachWalkData* pData = nullptr;

					EachWalkDataHolder() = default;

					~EachWalkDataHolder() {
						delete pData;
					}

					EachWalkDataHolder(const EachWalkDataHolder& other) {
						if (other.pData != nullptr)
							pData = new EachWalkData(*other.pData);
					}

					EachWalkDataHolder& operator=(const EachWalkDataHolder& other) {
						if (core::addressof(other) == this)
							return *this;

						if (other.pData == nullptr) {
							delete pData;
							pData = nullptr;
							return *this;
						}

						if (pData == nullptr)
							pData = new EachWalkData(*other.pData);
						else
							*pData = *other.pData;

						return *this;
					}

					EachWalkDataHolder(EachWalkDataHolder&& other) noexcept: pData(other.pData) {
						other.pData = nullptr;
					}

					EachWalkDataHolder& operator=(EachWalkDataHolder&& other) noexcept {
						if (core::addressof(other) == this)
							return *this;

						delete pData;
						pData = other.pData;
						other.pData = nullptr;
						return *this;
					}

					GAIA_NODISCARD EachWalkData* get() {
						return pData;
					}

					GAIA_NODISCARD const EachWalkData* get() const {
						return pData;
					}

					GAIA_NODISCARD EachWalkData& ensure() {
						if (pData == nullptr)
							pData = new EachWalkData();
						return *pData;
					}

					void reset() {
						delete pData;
						pData = nullptr;
					}
				};

				EachWalkDataHolder m_eachWalkData;

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

				struct DirectSeedRunDataHolder {
					DirectSeedRunData* pData = nullptr;

					DirectSeedRunDataHolder() = default;

					~DirectSeedRunDataHolder() {
						delete pData;
					}

					DirectSeedRunDataHolder(const DirectSeedRunDataHolder& other) {
						if (other.pData != nullptr)
							pData = new DirectSeedRunData(*other.pData);
					}

					DirectSeedRunDataHolder& operator=(const DirectSeedRunDataHolder& other) {
						if (core::addressof(other) == this)
							return *this;

						if (other.pData == nullptr) {
							delete pData;
							pData = nullptr;
							return *this;
						}

						if (pData == nullptr)
							pData = new DirectSeedRunData(*other.pData);
						else
							*pData = *other.pData;

						return *this;
					}

					DirectSeedRunDataHolder(DirectSeedRunDataHolder&& other) noexcept: pData(other.pData) {
						other.pData = nullptr;
					}

					DirectSeedRunDataHolder& operator=(DirectSeedRunDataHolder&& other) noexcept {
						if (core::addressof(other) == this)
							return *this;

						delete pData;
						pData = other.pData;
						other.pData = nullptr;
						return *this;
					}

					GAIA_NODISCARD DirectSeedRunData* get() {
						return pData;
					}

					GAIA_NODISCARD const DirectSeedRunData* get() const {
						return pData;
					}

					GAIA_NODISCARD DirectSeedRunData& ensure() {
						if (pData == nullptr)
							pData = new DirectSeedRunData();
						return *pData;
					}

					void reset() {
						delete pData;
						pData = nullptr;
					}
				};

				DirectSeedRunDataHolder m_directSeedRunData;

				//--------------------------------------------------------------------------------
			public:
				static inline bool SilenceInvalidCacheKindAssertions = false;

				//! Fetches the QueryInfo object.
				//! \return QueryInfo object
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

				void match_all(QueryInfo& queryInfo) {
					if (!validate_kind(queryInfo.ctx())) {
						GAIA_ASSERT(SilenceInvalidCacheKindAssertions && "Invalid kind selected for a query");
						queryInfo.reset();
						return;
					}

					if (!uses_query_cache_storage()) {
						queryInfo.ensure_matches_transient(
								*m_entityToArchetypeMap, all_archetypes_view(), m_varBindings, m_varBindingsMask);
						return;
					}

					queryInfo.ensure_matches(
							*m_entityToArchetypeMap, all_archetypes_view(), last_archetype_id(), m_varBindings, m_varBindingsMask);
					m_storage.m_pCache->sync_archetype_cache(queryInfo);
				}

				GAIA_NODISCARD bool match_one(QueryInfo& queryInfo, const Archetype& archetype, EntitySpan targetEntities) {
					if (!uses_query_cache_storage()) {
						return queryInfo.ensure_matches_one_transient(archetype, targetEntities, m_varBindings, m_varBindingsMask);
					}

					return queryInfo.ensure_matches_one(archetype, targetEntities, m_varBindings, m_varBindingsMask);
				}

				GAIA_NODISCARD bool matches_any(QueryInfo& queryInfo, const Archetype& archetype, EntitySpan targetEntities) {
					if (!validate_kind(queryInfo.ctx())) {
						GAIA_ASSERT(SilenceInvalidCacheKindAssertions && "Invalid kind selected for a query");
						queryInfo.reset();
						return false;
					}

					return matches_target_entities(queryInfo, archetype, targetEntities);
				}

				//--------------------------------------------------------------------------------

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
				GAIA_NODISCARD uint16_t cache_src_trav() const {
					return m_cacheSrcTrav;
				}

				QueryImpl& kind(QueryCacheKind cacheKind) {
					if (m_cacheKind == cacheKind)
						return *this;

					invalidate_each_walk_cache();
					invalidate_direct_seed_run_cache();
					invalidate_query_storage();
					m_cacheKind = cacheKind;

					return *this;
				}

				QueryImpl& scope(QueryCacheScope cacheScope) {
					if (m_cacheScope == cacheScope)
						return *this;

					invalidate_each_walk_cache();
					invalidate_direct_seed_run_cache();
					invalidate_query_storage();
					m_cacheScope = cacheScope;

					return *this;
				}

				QueryImpl& match_prefab() {
					QueryCmd_MatchPrefab cmd{};
					add_cmd(cmd);
					return *this;
				}

				GAIA_NODISCARD QueryCacheKind kind() const {
					return m_cacheKind;
				}

				GAIA_NODISCARD QueryCacheScope scope() const {
					return m_cacheScope;
				}

				GAIA_NODISCARD bool valid() {
					return validate_kind(fetch().ctx());
				}

				//--------------------------------------------------------------------------------
			private:
				//! Returns true when the query requests traversed-source snapshots beyond the default automatic cache layers.
				GAIA_NODISCARD bool uses_manual_src_trav_cache(const QueryCtx& ctx) const {
					return m_cacheSrcTrav != 0 && //
								 ctx.data.deps.has_dep_flag(QueryCtx::DependencyHasSourceTerms) && //
								 ctx.data.deps.has_dep_flag(QueryCtx::DependencyHasTraversalTerms);
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

				//! Validates that the requested public kind can be satisfied by the current query shape.
				GAIA_NODISCARD bool validate_kind(const QueryCtx& ctx) const {
					if (m_cacheKind == QueryCacheKind::None)
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

				GAIA_NODISCARD EachWalkData* each_walk_data() {
					return m_eachWalkData.get();
				}

				GAIA_NODISCARD const EachWalkData* each_walk_data() const {
					return m_eachWalkData.get();
				}

				GAIA_NODISCARD EachWalkData& ensure_each_walk_data() {
					return m_eachWalkData.ensure();
				}

				void invalidate_each_walk_cache() {
					auto* pWalkData = each_walk_data();
					if (pWalkData != nullptr)
						pWalkData->cacheValid = false;
				}

				GAIA_NODISCARD DirectSeedRunData* direct_seed_run_data() {
					return m_directSeedRunData.get();
				}

				GAIA_NODISCARD const DirectSeedRunData* direct_seed_run_data() const {
					return m_directSeedRunData.get();
				}

				GAIA_NODISCARD DirectSeedRunData& ensure_direct_seed_run_data() {
					return m_directSeedRunData.ensure();
				}

				void invalidate_direct_seed_run_cache() {
					auto* pRunData = direct_seed_run_data();
					if (pRunData != nullptr)
						pRunData->cacheValid = false;
				}

				void reset_changed_filter_state() {
					m_changedWorldVersion = 0;
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
					set_group_id_inter(groupId.value());
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

						return chunk.changed(changedWorldVersion);
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
					return chunk.changed(changedWorldVersion);
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

				GAIA_NODISCARD static bool has_depth_order_hierarchy_enabled_barrier(const QueryInfo& queryInfo) {
					const auto& data = queryInfo.ctx().data;
					return data.groupByFunc == group_by_func_depth_order &&
								 world_depth_order_prunes_disabled_subtrees(*queryInfo.world(), data.groupBy);
				}

				//! Fast enabled-subtree gate for cached depth_order(...) queries over fragmenting hierarchy relations.
				//! ChildOf is the native built-in example, but the rule is semantic: the relation must support
				//! depth_order(...) and also form a fragmenting hierarchy chain. For such relations, all rows in
				//! the archetype share the same direct parent target. That lets us prune the entire archetype when
				//! its parent chain crosses a disabled entity. Non-fragmenting hierarchy relations such as Parent
				//! cannot use this archetype-level check and must stay on the per-entity walk(...) path instead.
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

				template <typename TIter>
				GAIA_NODISCARD bool can_process_archetype_inter(
						const QueryInfo& queryInfo, const Archetype& archetype, int8_t barrierPasses = -1) const {
					if (!can_process_archetype(queryInfo, archetype))
						return false;
					if constexpr (std::is_same_v<TIter, Iter>) {
						if (has_depth_order_hierarchy_enabled_barrier(queryInfo)) {
							if (barrierPasses >= 0)
								return barrierPasses != 0;
							if (!survives_cascade_hierarchy_enabled_barrier(queryInfo, archetype))
								return false;
						}
					}
					return true;
				}

				template <typename T>
				GAIA_NODISCARD static constexpr bool is_write_query_arg() {
					using Arg = std::remove_cv_t<std::remove_reference_t<T>>;
					return std::is_lvalue_reference_v<T> && !std::is_const_v<std::remove_reference_t<T>> &&
								 !std::is_same_v<Arg, Entity>;
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
						if (!world_is_out_of_line_component(world, term)) {
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

				template <typename... T>
				static void finish_typed_chunk_writes(World& world, Chunk* pChunk, uint16_t from, uint16_t to) {
					if (from >= to)
						return;

					Entity seenTerms[sizeof...(T) > 0 ? sizeof...(T) : 1]{};
					uint32_t seenCnt = 0;
					const auto entities = pChunk->entity_view();
					const auto finish_term = [&](Entity term) {
						GAIA_FOR(seenCnt) {
							if (seenTerms[i] == term)
								return;
						}

						seenTerms[seenCnt++] = term;
						if (!world_is_out_of_line_component(world, term)) {
							const auto compIdx = core::get_index(pChunk->ids_view(), term);
							if (compIdx != BadIndex) {
								pChunk->finish_write(compIdx, from, to);
								return;
							}
						}

						for (uint16_t row = from; row < to; ++row)
							world_finish_write(world, term, entities[row]);
					};

					(
							[&] {
								if constexpr (is_write_query_arg<T>()) {
									using Arg = std::remove_cv_t<std::remove_reference_t<T>>;
									finish_term(world_query_arg_id<Arg>(world));
								}
							}(),
							...);
				}

				template <typename T, typename TIter>
				static void finish_typed_iter_write_arg(TIter& it, uint32_t fieldIdx) {
					if constexpr (!is_write_query_arg<T>())
						return;
					else {
						using Arg = std::remove_cv_t<std::remove_reference_t<T>>;
						auto* pChunk = const_cast<Chunk*>(it.chunk());
						auto& world = *it.world();
						Entity term = it.term_ids() != nullptr ? it.term_ids()[fieldIdx] : world_query_arg_id<Arg>(world);
						if (term == EntityBad)
							term = world_query_arg_id<Arg>(world);

						if (it.row_begin() >= it.row_end())
							return;

						auto compIdx = uint8_t(0xFF);
						if (it.comp_indices() != nullptr && fieldIdx < ChunkHeader::MAX_COMPONENTS)
							compIdx = it.comp_indices()[fieldIdx];
						else if (!world_is_out_of_line_component(world, term))
							compIdx = (uint8_t)pChunk->comp_idx(term);
						if (compIdx != 0xFF && !world_is_out_of_line_component(world, term)) {
							pChunk->finish_write(compIdx, it.row_begin(), it.row_end());
							return;
						}

						auto entities = it.entity_rows();
						GAIA_FOR(entities.size()) {
							world_finish_write(world, term, entities[i]);
						}
					}
				}

				template <typename TIter, typename... T, size_t... I>
				static void finish_typed_iter_writes(TIter& it, std::index_sequence<I...>) {
					(finish_typed_iter_write_arg<T>(it, (uint32_t)I), ...);
				}

				enum class ExecPayloadKind : uint8_t { Plain, Grouped, NonTrivial };

				template <typename TIter>
				GAIA_NODISCARD static ExecPayloadKind exec_payload_kind(const QueryInfo& queryInfo) {
					if (queryInfo.has_sorted_payload())
						return ExecPayloadKind::NonTrivial;
					if (!queryInfo.has_grouped_payload())
						return ExecPayloadKind::Plain;
					if constexpr (std::is_same_v<TIter, Iter>) {
						if (has_depth_order_hierarchy_enabled_barrier(queryInfo))
							return ExecPayloadKind::NonTrivial;
					}
					return ExecPayloadKind::Grouped;
				}

				template <typename TIter>
				GAIA_NODISCARD static bool needs_nontrivial_payload(const QueryInfo& queryInfo) {
					return exec_payload_kind<TIter>(queryInfo) == ExecPayloadKind::NonTrivial;
				}

				//--------------------------------------------------------------------------------

				//! Execute the functor for a given chunk batch
				template <typename Func, typename TIter>
				static void run_query_func(World* pWorld, Func func, ChunkBatch& batch) {
					TIter it;
					it.set_world(pWorld);
					it.set_write_im(false);
					it.set_archetype(batch.pArchetype);
					it.set_chunk(batch.pChunk, batch.from, batch.to);
					it.set_group_id(batch.groupId);
					it.set_comp_indices(batch.pCompIndices);
					func(it);
					finish_iter_writes(it);
					it.clear_touched_writes();
				}

				//! Execute the functor for a given chunk batch
				template <typename Func, typename TIter>
				static void run_query_arch_func(World* pWorld, Func func, ChunkBatch& batch) {
					TIter it;
					it.set_world(pWorld);
					it.set_write_im(false);
					it.set_archetype(batch.pArchetype);
					// it.set_chunk(nullptr, 0, 0); We do not need this, and calling it would assert
					it.set_group_id(batch.groupId);
					it.set_comp_indices(batch.pCompIndices);
					func(it);
					it.clear_touched_writes();
				}

				//! Execute the functor in batches
				template <typename Func, typename TIter>
				static void run_query_func(World* pWorld, Func func, std::span<ChunkBatch> batches) {
					GAIA_PROF_SCOPE(query::run_query_func);

					const auto chunkCnt = batches.size();
					GAIA_ASSERT(chunkCnt > 0);

					TIter it;
					it.set_world(pWorld);
					it.set_write_im(false);

					const Archetype* pLastArchetype = nullptr;
					const uint8_t* pLastIndices = nullptr;
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

				template <bool HasFilters, typename TIter, typename Func>
				void run_query_batch_no_group_id(
						const QueryInfo& queryInfo, const uint32_t idxFrom, const uint32_t idxTo, Func func) {
					GAIA_PROF_SCOPE(query::run_query_batch_no_group_id);

					auto cacheView = queryInfo.cache_archetype_view();
					const auto payloadKind = exec_payload_kind<TIter>(queryInfo);
					auto sortView = queryInfo.cache_sort_view();
					if (payloadKind != ExecPayloadKind::NonTrivial)
						sortView = {};
					const bool needsBarrierCache = payloadKind == ExecPayloadKind::NonTrivial && std::is_same_v<TIter, Iter> &&
																				 has_depth_order_hierarchy_enabled_barrier(queryInfo);
					if (needsBarrierCache)
						const_cast<QueryInfo&>(queryInfo).ensure_depth_order_hierarchy_barrier_cache();

					lock(*m_storage.world());

					// We are batching by chunks. Some of them might contain only few items but this state is only
					// temporary because defragmentation runs constantly and keeps things clean.
					ChunkBatchArray chunkBatches;

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
								if (!match_filters(*view.pChunk, queryInfo, m_changedWorldVersion))
									continue;
							}

							auto* pArchetype = const_cast<Archetype*>(cacheView[view.archetypeIdx]);
							const bool barrierPasses = !needsBarrierCache || queryInfo.barrier_passes(view.archetypeIdx);
							if GAIA_UNLIKELY (!can_process_archetype_inter<TIter>(queryInfo, *pArchetype, barrierPasses))
								continue;
							auto indicesView = queryInfo.indices_mapping_view(view.archetypeIdx);

							chunkBatches.push_back({pArchetype, view.pChunk, indicesView.data(), 0U, startRow, endRow});

							if GAIA_UNLIKELY (chunkBatches.size() == chunkBatches.max_size()) {
								run_query_func<Func, TIter>(m_storage.world(), func, {chunkBatches.data(), chunkBatches.size()});
								chunkBatches.clear();
							}
						}
					} else {
						for (uint32_t i = idxFrom; i < idxTo; ++i) {
							auto* pArchetype = const_cast<Archetype*>(cacheView[i]);
							const bool barrierPasses = !needsBarrierCache || queryInfo.barrier_passes(i);
							if GAIA_UNLIKELY (!can_process_archetype_inter<TIter>(queryInfo, *pArchetype, barrierPasses))
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
										if (!match_filters(*pChunk, queryInfo, m_changedWorldVersion))
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
					const auto payloadKind = exec_payload_kind<TIter>(queryInfo);
					auto sortView = queryInfo.cache_sort_view();
					if (payloadKind != ExecPayloadKind::NonTrivial)
						sortView = {};
					const bool needsBarrierCache = payloadKind == ExecPayloadKind::NonTrivial && std::is_same_v<TIter, Iter> &&
																				 has_depth_order_hierarchy_enabled_barrier(queryInfo);
					if (needsBarrierCache)
						const_cast<QueryInfo&>(queryInfo).ensure_depth_order_hierarchy_barrier_cache();

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
								if (!match_filters(*view.pChunk, queryInfo, m_changedWorldVersion))
									continue;
							}

							const auto* pArchetype = cacheView[view.archetypeIdx];
							const bool barrierPasses = !needsBarrierCache || queryInfo.barrier_passes(view.archetypeIdx);
							if GAIA_UNLIKELY (!can_process_archetype_inter<TIter>(queryInfo, *pArchetype, barrierPasses))
								continue;
							auto indicesView = queryInfo.indices_mapping_view(view.archetypeIdx);

							m_batches.push_back({pArchetype, view.pChunk, indicesView.data(), 0U, startRow, endRow});
						}
					} else {
						for (uint32_t i = idxFrom; i < idxTo; ++i) {
							const auto* pArchetype = cacheView[i];
							const bool barrierPasses = !needsBarrierCache || queryInfo.barrier_passes(i);
							if GAIA_UNLIKELY (!can_process_archetype_inter<TIter>(queryInfo, *pArchetype, barrierPasses))
								continue;

							auto indicesView = queryInfo.indices_mapping_view(i);
							const auto& chunks = pArchetype->chunks();
							for (auto* pChunk: chunks) {
								if GAIA_UNLIKELY (TIter::size(pChunk) == 0)
									continue;

								if constexpr (HasFilters) {
									if (!match_filters(*pChunk, queryInfo, m_changedWorldVersion))
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
					const bool needsBarrierCache = needs_nontrivial_payload<TIter>(queryInfo) && std::is_same_v<TIter, Iter> &&
																				 has_depth_order_hierarchy_enabled_barrier(queryInfo);
					if (needsBarrierCache)
						const_cast<QueryInfo&>(queryInfo).ensure_depth_order_hierarchy_barrier_cache();

					lock(*m_storage.world());

					for (uint32_t i = idxFrom; i < idxTo; ++i) {
						const auto* pArchetype = cacheView[i];
						const bool barrierPasses = !needsBarrierCache || queryInfo.barrier_passes(i);
						if GAIA_UNLIKELY (!can_process_archetype_inter<TIter>(queryInfo, *pArchetype, barrierPasses))
							continue;

						auto indicesView = queryInfo.indices_mapping_view(i);
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
								if GAIA_UNLIKELY (TIter::size(pChunk) == 0)
									continue;

								if constexpr (HasFilters) {
									if (!match_filters(*pChunk, queryInfo, m_changedWorldVersion))
										continue;
								}

								chunkBatches.push_back({pArchetype, pChunk, indicesView.data(), groupId, 0, 0});
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
					const bool needsBarrierCache = needs_nontrivial_payload<TIter>(queryInfo) && std::is_same_v<TIter, Iter> &&
																				 has_depth_order_hierarchy_enabled_barrier(queryInfo);
					if (needsBarrierCache)
						const_cast<QueryInfo&>(queryInfo).ensure_depth_order_hierarchy_barrier_cache();

#if GAIA_ASSERT_ENABLED
					for (uint32_t i = idxFrom; i < idxTo; ++i) {
						auto* pArchetype = cacheView[i];
						const bool barrierPasses = !needsBarrierCache || queryInfo.barrier_passes(i);
						if GAIA_UNLIKELY (!can_process_archetype_inter<TIter>(queryInfo, *pArchetype, barrierPasses))
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
						if GAIA_UNLIKELY (!can_process_archetype_inter<TIter>(queryInfo, *pArchetype, barrierPasses))
							continue;

						auto indicesView = queryInfo.indices_mapping_view(i);
						const auto groupId = queryInfo.group_id(i);
						const auto& chunks = pArchetype->chunks();
						for (auto* pChunk: chunks) {
							if GAIA_UNLIKELY (TIter::size(pChunk) == 0)
								continue;

							if constexpr (HasFilters) {
								if (!match_filters(*pChunk, queryInfo, m_changedWorldVersion))
									continue;
							}

							m_batches.push_back({pArchetype, pChunk, indicesView.data(), groupId, 0, 0});
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
					const bool isGroupSet = m_groupIdSet != 0;
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
						const auto* pGroupData = queryInfo.selected_group_data(m_groupIdSet);
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
						const bool needsBarrierCache = has_depth_order_hierarchy_enabled_barrier(queryInfo);
						if (needsBarrierCache)
							const_cast<QueryInfo&>(queryInfo).ensure_depth_order_hierarchy_barrier_cache();
						GAIA_EACH(cache_view) {
							const auto* pArchetype = cache_view[i];
							const bool barrierPasses = !needsBarrierCache || queryInfo.barrier_passes(i);
							if GAIA_UNLIKELY (!can_process_archetype_inter<Iter>(queryInfo, *pArchetype, barrierPasses))
								continue;

							auto indicesView = queryInfo.indices_mapping_view(i);
							ChunkBatch batch{pArchetype, nullptr, indicesView.data(), 0, 0, 0};
							run_query_arch_func<Func, Iter>(m_storage.world(), func, batch);
						}
					}

					unlock(*m_storage.world());
					// Changed-filter state is instance-local for cached queries.
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

					// Changed-filter state is instance-local for cached queries.
					m_changedWorldVersion = *m_worldVersion;
				}

				GAIA_NODISCARD bool can_use_direct_chunk_iteration_fastpath(const QueryInfo& queryInfo) const {
					const auto& data = queryInfo.ctx().data;
					return data.sortByFunc == nullptr && !has_depth_order_hierarchy_enabled_barrier(queryInfo);
				}

				template <typename Func, typename... T>
				void run_query_on_chunks_direct(QueryInfo& queryInfo, Func func, core::func_type_list<T...>) {
					::gaia::ecs::update_version(*m_worldVersion);

					const bool hasFilters = queryInfo.has_filters();
					auto cacheView = queryInfo.cache_archetype_view();
					if (cacheView.empty())
						return;

					uint32_t idxFrom = 0;
					uint32_t idxTo = (uint32_t)cacheView.size();
					if (queryInfo.ctx().data.groupBy != EntityBad && m_groupIdSet != 0) {
						const auto* pGroupData = queryInfo.selected_group_data(m_groupIdSet);
						if (pGroupData == nullptr)
							return;
						idxFrom = pGroupData->idxFirst;
						idxTo = pGroupData->idxLast + 1;
					}

					lock(*m_storage.world());

					for (uint32_t i = idxFrom; i < idxTo; ++i) {
						const auto* pArchetype = cacheView[i];
						if GAIA_UNLIKELY (!can_process_archetype_inter<Iter>(queryInfo, *pArchetype))
							continue;

						const auto& chunks = pArchetype->chunks();
						for (auto* pChunk: chunks) {
							const auto from = Iter::start_index(pChunk);
							const auto to = Iter::end_index(pChunk);
							if GAIA_UNLIKELY (from == to)
								continue;

							if (hasFilters) {
								if GAIA_UNLIKELY (!match_filters(*pChunk, queryInfo, m_changedWorldVersion))
									continue;
							}

							GAIA_PROF_SCOPE(query_func);
							run_query_on_chunk_rows_direct(pChunk, from, to, func, core::func_type_list<T...>{});
							finish_typed_chunk_writes<T...>(*queryInfo.world(), pChunk, from, to);
						}
					}

					unlock(*m_storage.world());
					commit_cmd_buffer_st(*m_storage.world());
					commit_cmd_buffer_mt(*m_storage.world());
					m_changedWorldVersion = *m_worldVersion;
				}

				template <typename TIter, typename Func, typename... T>
				GAIA_FORCEINLINE void
				run_query_on_chunk(TIter& it, Func func, [[maybe_unused]] core::func_type_list<T...> types) {
					auto& queryInfo = fetch();
					auto& world = *const_cast<World*>(queryInfo.world());
					if (can_use_direct_chunk_term_eval<T...>(world, queryInfo))
						run_query_on_chunk_direct(it, func, types);
					else
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
						auto dataPointerTuple = std::make_tuple(it.template view_auto_any<T>()...);

						// Iterate over each entity in the chunk.
						// Translates to:
						//		GAIA_FOR(0, cnt) func(p[i], v[i]);

						if (!hasEntityFilters) {
							GAIA_FOR(cnt) {
								func(
										std::get<decltype(it.template view_auto_any<T>())>(
												dataPointerTuple)[it.template acc_index<T>(i)]...);
							}
						} else {
							const auto entities = it.template view<Entity>();
							GAIA_FOR(cnt) {
								if (!match_entity_filters(*queryInfo.world(), entities[i], queryInfo))
									continue;
								func(
										std::get<decltype(it.template view_auto_any<T>())>(
												dataPointerTuple)[it.template acc_index<T>(i)]...);
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

					finish_typed_iter_writes<TIter, T...>(it, std::index_sequence_for<T...>{});
					it.clear_touched_writes();
				}

				template <typename TIter, typename Func, typename... T>
				GAIA_FORCEINLINE void
				run_query_on_chunk_direct(TIter& it, Func func, [[maybe_unused]] core::func_type_list<T...> types) {
					run_query_on_chunk_rows_direct(const_cast<Chunk*>(it.chunk()), it.row_begin(), it.row_end(), func, types);
					finish_typed_iter_writes<TIter, T...>(it, std::index_sequence_for<T...>{});
					it.clear_touched_writes();
				}

				template <typename T>
				GAIA_FORCEINLINE static decltype(auto) chunk_view_auto(Chunk* pChunk) {
					using Arg = std::remove_cv_t<std::remove_reference_t<T>>;
					if constexpr (std::is_same_v<Arg, Entity>)
						return pChunk->entity_view();
					else {
						using FT = typename component_type_t<Arg>::TypeFull;
						if constexpr (std::is_lvalue_reference_v<T> && !std::is_const_v<std::remove_reference_t<T>>)
							return pChunk->template sview_mut<FT>();
						else
							return pChunk->template view<FT>();
					}
				}

				template <typename Func, typename... T>
				GAIA_FORCEINLINE static void run_query_on_chunk_rows_direct(
						Chunk* pChunk, uint16_t from, uint16_t to, Func& func, core::func_type_list<T...>) {
					if constexpr (sizeof...(T) > 0) {
						auto dataPointerTuple = std::make_tuple(chunk_view_auto<T>(pChunk)...);
						for (uint16_t row = from; row < to; ++row)
							func(std::get<decltype(chunk_view_auto<T>(pChunk))>(dataPointerTuple)[row]...);
					} else {
						for (uint16_t row = from; row < to; ++row)
							func();
					}
				}

				template <typename TIter, typename Func, typename... T>
				GAIA_FORCEINLINE void
				run_query_on_direct_entity(TIter& it, Func func, [[maybe_unused]] core::func_type_list<T...> types) {
					if constexpr (sizeof...(T) > 0) {
						auto dataPointerTuple = std::make_tuple(it.template view_auto_any<T>()...);
						func(std::get<decltype(it.template view_auto_any<T>())>(dataPointerTuple)[it.template acc_index<T>(0)]...);
					} else {
						func();
					}

					finish_typed_iter_writes<TIter, T...>(it, std::index_sequence_for<T...>{});
					it.clear_touched_writes();
				}

				template <typename TIter, typename Func, typename... T>
				GAIA_FORCEINLINE void
				run_query_on_direct_entity_direct(TIter& it, Func func, [[maybe_unused]] core::func_type_list<T...> types) {
					run_query_on_chunk_rows_direct(const_cast<Chunk*>(it.chunk()), it.row_begin(), it.row_end(), func, types);
					finish_typed_iter_writes<TIter, T...>(it, std::index_sequence_for<T...>{});
					it.clear_touched_writes();
				}

				//------------------------------------------------

				template <QueryExecType ExecType, typename Func, typename... T>
				void each_inter(QueryInfo& queryInfo, Func func, core::func_type_list<T...>) {
					if (!queryInfo.has_filters() && can_use_direct_entity_seed_eval(queryInfo)) {
						GAIA_PROF_SCOPE(query_func);
						each_direct_inter<Iter>(queryInfo, func, core::func_type_list<T...>{});
						return;
					}

					auto& world = *const_cast<World*>(queryInfo.world());
					if (can_use_direct_chunk_term_eval<T...>(world, queryInfo)) {
						if constexpr (ExecType == QueryExecType::Default) {
							if (can_use_direct_chunk_iteration_fastpath(queryInfo)) {
								run_query_on_chunks_direct(queryInfo, func, core::func_type_list<T...>{});
								return;
							}
						}
						run_query_on_chunks<ExecType, Iter>(queryInfo, [&](Iter& it) {
							GAIA_PROF_SCOPE(query_func);
							run_query_on_chunk_direct(it, func, core::func_type_list<T...>{});
						});
					} else {
						run_query_on_chunks<ExecType, Iter>(queryInfo, [&](Iter& it) {
							GAIA_PROF_SCOPE(query_func);
							run_query_on_chunk(queryInfo, it, func, core::func_type_list<T...>{});
						});
					}
				}

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

					each_inter<ExecType>(queryInfo, func, InputArgs{});
				}

				template <QueryExecType ExecType, typename Func>
				void each_inter(Func func) {
					auto& queryInfo = fetch();
					match_all(queryInfo);

					if constexpr (std::is_invocable_v<Func, IterAll&>) {
						if (!queryInfo.has_filters() && can_use_direct_entity_seed_eval(queryInfo)) {
							GAIA_PROF_SCOPE(query_func);
							each_direct_iter_inter<IterAll>(queryInfo, func);
							return;
						}
						run_query_on_chunks<ExecType, IterAll>(queryInfo, [&](IterAll& it) {
							GAIA_PROF_SCOPE(query_func);
							func(it);
						});
					} else if constexpr (std::is_invocable_v<Func, Iter&>) {
						if (!queryInfo.has_filters() && can_use_direct_entity_seed_eval(queryInfo)) {
							GAIA_PROF_SCOPE(query_func);
							each_direct_iter_inter<Iter>(queryInfo, func);
							return;
						}
						run_query_on_chunks<ExecType, Iter>(queryInfo, [&](Iter& it) {
							GAIA_PROF_SCOPE(query_func);
							func(it);
						});
					} else if constexpr (std::is_invocable_v<Func, IterDisabled&>) {
						if (!queryInfo.has_filters() && can_use_direct_entity_seed_eval(queryInfo)) {
							GAIA_PROF_SCOPE(query_func);
							each_direct_iter_inter<IterDisabled>(queryInfo, func);
							return;
						}
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
								 (!id.pair() && world_is_non_fragmenting_out_of_line_component(world, id));
				}

				//! Returns true when a term uses semantic `Is` matching rather than direct storage matching.
				GAIA_NODISCARD static bool uses_semantic_is_matching(const QueryTerm& term) {
					const auto id = term.id;
					return term.matchKind == QueryMatchKind::Semantic && term.src == EntityBad && term.entTrav == EntityBad &&
								 !term_has_variables(term) && id.pair() && id.id() == Is.id() && !is_wildcard(id.gen()) &&
								 !is_variable((EntityId)id.gen());
				}

				//! Returns true when a term uses strict semantic `Is` matching that excludes the base entity itself.
				GAIA_NODISCARD static bool uses_in_is_matching(const QueryTerm& term) {
					const auto id = term.id;
					return term.matchKind == QueryMatchKind::In && term.src == EntityBad && term.entTrav == EntityBad &&
								 !term_has_variables(term) && id.pair() && id.id() == Is.id() && !is_wildcard(id.gen()) &&
								 !is_variable((EntityId)id.gen());
				}

				//! Returns true when a term uses any semantic `Is` matching rather than direct storage matching.
				GAIA_NODISCARD static bool uses_non_direct_is_matching(const QueryTerm& term) {
					return uses_semantic_is_matching(term) || uses_in_is_matching(term);
				}

				//! Returns true when a term uses semantic inherited-id matching rather than direct storage matching.
				GAIA_NODISCARD static bool uses_inherited_id_matching(const World& world, const QueryTerm& term) {
					const auto id = term.id;
					return term.matchKind == QueryMatchKind::Semantic && term.src == EntityBad && term.entTrav == EntityBad &&
								 !term_has_variables(term) && !is_wildcard(id) && !is_variable((EntityId)id.id()) &&
								 (!id.pair() || !is_variable((EntityId)id.gen())) && world_term_uses_inherit_policy(world, id);
				}

				//! Evaluates term presence for a concrete entity using either direct or semantic semantics.
				GAIA_NODISCARD static bool match_entity_term(const World& world, Entity entity, const QueryTerm& term) {
					if (uses_semantic_is_matching(term) || uses_inherited_id_matching(world, term))
						return world_has_entity_term(world, entity, term.id);
					if (uses_in_is_matching(term))
						return world_has_entity_term_in(world, entity, term.id);

					return world_has_entity_term_direct(world, entity, term.id);
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

				//! Detects queries that can skip archetype seeding and start directly from entity-backed term indices.
				GAIA_NODISCARD static bool can_use_direct_entity_seed_eval(const QueryInfo& queryInfo) {
					const auto& ctxData = queryInfo.ctx().data;
					if (ctxData.sortByFunc != nullptr || ctxData.groupBy != EntityBad)
						return false;

					const auto& world = *queryInfo.world();
					bool hasPositiveTerm = false;
					bool hasSeedableTerm = false;
					for (const auto& term: ctxData.terms_view()) {
						if (term.src != EntityBad || term.entTrav != EntityBad || term_has_variables(term))
							return false;

						if (term.op == QueryOpKind::Any || term.op == QueryOpKind::Count)
							return false;

						if (term.op == QueryOpKind::All || term.op == QueryOpKind::Or) {
							hasPositiveTerm = true;
							if (uses_non_direct_is_matching(term) || uses_inherited_id_matching(world, term) ||
									uses_in_is_matching(term) || is_adjunct_direct_term(world, term)) {
								hasSeedableTerm = true;
							}
						}
					}

					return hasPositiveTerm && hasSeedableTerm;
				}

				//! Detects queries whose terms can be evaluated directly against concrete target entities.
				GAIA_NODISCARD static bool can_use_direct_target_eval(const QueryInfo& queryInfo) {
					bool hasPositiveTerm = false;
					for (const auto& term: queryInfo.ctx().data.terms_view()) {
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

				template <typename TIter>
				GAIA_NODISCARD static constexpr Constraints direct_seed_constraints() {
					if constexpr (std::is_same_v<TIter, Iter>)
						return Constraints::EnabledOnly;
					else if constexpr (std::is_same_v<TIter, IterDisabled>)
						return Constraints::DisabledOnly;
					else
						return Constraints::AcceptAll;
				}

				static void
				append_chunk_run(cnt::darray<detail::BfsChunkRun>& runs, const EntityContainer& ec, uint32_t entityOffset) {
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

				//! Returns true when a repeated semantic/inherited seed can be cached as chunk runs.
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
						if (is_adjunct_direct_term(world, term))
							return false;
					}

					return true;
				}

				template <typename TIter>
				GAIA_NODISCARD std::span<const detail::BfsChunkRun>
				cached_direct_seed_runs(QueryInfo& queryInfo, const QueryTerm& seedTerm, const DirectEntitySeedInfo& seedInfo) {
					auto& runData = ensure_direct_seed_run_data();
					auto& world = *queryInfo.world();
					const auto constraints = direct_seed_constraints<TIter>();
					const auto relVersion = world_rel_version(world, Is);
					const auto worldVersion = ::gaia::ecs::world_version(world);

					if (runData.cacheValid && runData.cachedSeedTerm == seedTerm.id &&
							runData.cachedSeedMatchKind == seedTerm.matchKind && runData.cachedConstraints == constraints &&
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
						if (!match_direct_entity_constraints<TIter>(world, queryInfo, entity))
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
						append_chunk_run(runs, ec, entityOffset++);
					}

					runData.cachedSeedTerm = seedTerm.id;
					runData.cachedSeedMatchKind = seedTerm.matchKind;
					runData.cachedConstraints = constraints;
					runData.cachedRelVersion = relVersion;
					runData.cachedWorldVersion = worldVersion;
					runData.cacheValid = true;
					return {runs.data(), runs.size()};
				}

				template <typename TIter>
				GAIA_NODISCARD std::span<const Entity> cached_direct_seed_entities(
						QueryInfo& queryInfo, const QueryTerm& seedTerm, const DirectEntitySeedInfo& seedInfo) {
					(void)cached_direct_seed_runs<TIter>(queryInfo, seedTerm, seedInfo);
					auto& runData = ensure_direct_seed_run_data();
					return {runData.cachedEntities.data(), runData.cachedEntities.size()};
				}

				template <typename TIter, typename Func>
				GAIA_NODISCARD static bool for_each_direct_all_seed(
						const World& world, const QueryInfo& queryInfo, const DirectEntitySeedPlan& plan, Func&& func) {
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
						const auto seedTarget = entity_from_id(world, (EntityId)pSeedTerm->id.gen());
						if (seedTarget != EntityBad)
							seedImpliesSingleAllTerm = match_entity_term(world, seedTarget, *evalPlan.pSingleAllTerm);
					}

					// Stream the chosen ALL seed term directly. This avoids materializing a temporary
					// entity array for the common `all<T>().is(base)` shape.
					return for_each_direct_term_entity(world, *pSeedTerm, [&](Entity entity) {
						if (!match_direct_entity_constraints<TIter>(world, queryInfo, entity))
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
				template <typename TIter>
				GAIA_NODISCARD static bool
				match_direct_entity_constraints(const World& world, const QueryInfo& queryInfo, Entity entity) {
					if (!queryInfo.matches_prefab_entities() && world_entity_prefab(world, entity))
						return false;

					if constexpr (std::is_same_v<TIter, Iter>)
						return world_entity_enabled(world, entity);
					else if constexpr (std::is_same_v<TIter, IterDisabled>)
						return !world_entity_enabled(world, entity);
					else
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
						if (is_adjunct_direct_term(world, term))
							return false;
						if (uses_inherited_id_matching(world, term))
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
						if (!match_direct_entity_constraints<TIter>(world, queryInfo, entity))
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
				template <typename TIter>
				GAIA_NODISCARD static uint32_t count_direct_or_union(const World& world, const QueryInfo& queryInfo) {
					auto& scratch = direct_query_scratch();
					const auto seenVersion = next_direct_query_seen_version(scratch);
					const bool hasDirectNotTerms = has_direct_not_terms(queryInfo);

					uint32_t cnt = 0;
					for (const auto& term: queryInfo.ctx().data.terms_view()) {
						if (term.op != QueryOpKind::Or)
							continue;

						(void)for_each_direct_term_entity(world, term, [&](Entity entity) {
							if (!match_direct_entity_constraints<TIter>(world, queryInfo, entity))
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

				//! Returns whether any entity survives the direct OR union after applying NOT terms and iterator constraints.
				template <typename TIter>
				GAIA_NODISCARD static bool is_empty_direct_or_union(const World& world, const QueryInfo& queryInfo) {
					auto& scratch = direct_query_scratch();
					const auto seenVersion = next_direct_query_seen_version(scratch);
					const bool hasDirectNotTerms = has_direct_not_terms(queryInfo);

					for (const auto& term: queryInfo.ctx().data.terms_view()) {
						if (term.op != QueryOpKind::Or)
							continue;

						const bool completed = for_each_direct_term_entity(world, term, [&](Entity entity) {
							if (!match_direct_entity_constraints<TIter>(world, queryInfo, entity))
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

				//! Returns true when direct OR evaluation still has direct NOT terms that must be checked per entity.
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
				template <typename TIter, typename Func>
				void for_each_direct_or_union(World& world, const QueryInfo& queryInfo, Func&& func) {
					auto& scratch = direct_query_scratch();
					const auto seenVersion = next_direct_query_seen_version(scratch);
					DirectEntitySeedInfo seedInfo{};
					seedInfo.seededFromOr = true;

					for (const auto& term: queryInfo.ctx().data.terms_view()) {
						if (term.op != QueryOpKind::Or)
							continue;

						(void)for_each_direct_term_entity(world, term, [&](Entity entity) {
							if (!match_direct_entity_constraints<TIter>(world, queryInfo, entity))
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

				//! Fast empty() path for direct non-fragmenting queries that can seed from entity-backed indices.
				template <bool UseFilters, typename TIter>
				GAIA_NODISCARD bool empty_inter(const QueryInfo& queryInfo) const {
					const bool hasRuntimeGroupFilter = queryInfo.ctx().data.groupBy != EntityBad && m_groupIdSet != 0;

					if constexpr (!UseFilters) {
						if (!hasRuntimeGroupFilter && can_use_direct_entity_seed_eval(queryInfo)) {
							if (has_only_direct_or_terms(queryInfo))
								return is_empty_direct_or_union<TIter>(*queryInfo.world(), queryInfo);

							const auto plan = direct_entity_seed_plan(*queryInfo.world(), queryInfo);
							bool empty = true;
							(void)for_each_direct_all_seed<TIter>(*queryInfo.world(), queryInfo, plan, [&](Entity) {
								empty = false;
								return false;
							});
							return empty;
						}
					}

					const bool hasEntityFilters = queryInfo.has_entity_filter_terms();
					const auto cacheView = queryInfo.cache_archetype_view();
					const bool needsBarrierCache = has_depth_order_hierarchy_enabled_barrier(queryInfo);
					if (needsBarrierCache)
						const_cast<QueryInfo&>(queryInfo).ensure_depth_order_hierarchy_barrier_cache();
					uint32_t idxFrom = 0;
					uint32_t idxTo = (uint32_t)cacheView.size();
					if (hasRuntimeGroupFilter) {
						const auto* pGroupData = queryInfo.selected_group_data(m_groupIdSet);
						if (pGroupData == nullptr)
							return true;
						idxFrom = pGroupData->idxFirst;
						idxTo = pGroupData->idxLast + 1;
					}

					for (uint32_t qi = idxFrom; qi < idxTo; ++qi) {
						const auto* pArchetype = cacheView[qi];
						const bool barrierPasses = !needsBarrierCache || queryInfo.barrier_passes(qi);
						if GAIA_UNLIKELY (!can_process_archetype_inter<TIter>(queryInfo, *pArchetype, barrierPasses))
							continue;

						GAIA_PROF_SCOPE(query::empty);

						const auto& chunks = pArchetype->chunks();
						if (!hasEntityFilters) {
							for (auto* pChunk: chunks) {
								if (TIter::size(pChunk) == 0)
									continue;
								if constexpr (UseFilters) {
									if (!match_filters(*pChunk, queryInfo, m_changedWorldVersion))
										continue;
								}
								return false;
							}
							continue;
						}

						TIter it;
						it.set_world(queryInfo.world());
						it.set_archetype(pArchetype);

						const bool isNotEmpty = core::has_if(chunks, [&](Chunk* pChunk) {
							it.set_chunk(pChunk);
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

				//! Evaluates the entity-level adjunct terms that are not represented by archetype membership.
				GAIA_NODISCARD static bool match_entity_filters(const World& world, Entity entity, const QueryInfo& queryInfo) {
					bool hasOrTerms = false;
					bool anyOrMatched = false;
					const bool hasAdjunctTerms = queryInfo.has_entity_filter_terms();

					for (const auto& term: queryInfo.ctx().data.terms_view()) {
						if (term.src != EntityBad || term.entTrav != EntityBad || term_has_variables(term))
							continue;

						const auto id = term.id;
						const bool isDirectIsTerm = uses_non_direct_is_matching(term);
						const bool isInheritedTerm = uses_inherited_id_matching(world, term);
						const bool isAdjunctTerm =
								(id.pair() && world_is_exclusive_dont_fragment_relation(world, entity_from_id(world, id.id()))) ||
								(!id.pair() && world_is_non_fragmenting_out_of_line_component(world, id));
						const bool needsEntityFilter =
								isAdjunctTerm || isDirectIsTerm || isInheritedTerm || (hasAdjunctTerms && term.op == QueryOpKind::Or);
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

				//! Checks whether any of the provided target entities matches the query semantics.
				GAIA_NODISCARD bool
				matches_target_entities(QueryInfo& queryInfo, const Archetype& archetype, EntitySpan targetEntities) {
					if (targetEntities.empty())
						return false;

					const auto& world = *queryInfo.world();

					if (can_use_direct_target_eval(queryInfo)) {
						const DirectEntitySeedInfo seedInfo{};
						for (const auto entity: targetEntities) {
							if (!match_direct_entity_constraints<Iter>(world, queryInfo, entity))
								continue;
							if (match_direct_entity_terms(world, entity, queryInfo, seedInfo))
								return true;
						}

						return false;
					}

					const bool archetypeMatched = match_one(queryInfo, archetype, targetEntities);

					if (!archetypeMatched)
						return false;

					if (!queryInfo.has_entity_filter_terms())
						return true;

					for (const auto entity: targetEntities) {
						if (!match_direct_entity_constraints<Iter>(world, queryInfo, entity))
							continue;
						if (match_entity_filters(world, entity, queryInfo))
							return true;
					}

					return false;
				}

				//! Fast count() path for direct non-fragmenting queries that can seed from entity-backed indices.
				template <bool UseFilters, typename TIter>
				GAIA_NODISCARD uint32_t count_inter(const QueryInfo& queryInfo) const {
					const bool hasRuntimeGroupFilter = queryInfo.ctx().data.groupBy != EntityBad && m_groupIdSet != 0;

					if constexpr (!UseFilters) {
						if (!hasRuntimeGroupFilter && can_use_direct_entity_seed_eval(queryInfo)) {
							auto& scratch = direct_query_scratch();
							if (has_only_direct_or_terms(queryInfo))
								return count_direct_or_union<TIter>(*queryInfo.world(), queryInfo);

							const auto plan = direct_entity_seed_plan(*queryInfo.world(), queryInfo);
							const auto seedInfo = build_direct_entity_seed(*queryInfo.world(), queryInfo, scratch.entities);

							if (can_use_archetype_bucket_count(*queryInfo.world(), queryInfo, seedInfo))
								return count_direct_entity_seed_by_archetype<TIter>(
										*queryInfo.world(), queryInfo, scratch.entities, seedInfo);

							uint32_t cnt = 0;
							(void)for_each_direct_all_seed<TIter>(*queryInfo.world(), queryInfo, plan, [&](Entity) {
								++cnt;
								return true;
							});

							return cnt;
						}
					}

					uint32_t cnt = 0;
					const bool hasEntityFilters = queryInfo.has_entity_filter_terms();
					const auto cacheView = queryInfo.cache_archetype_view();
					const bool needsBarrierCache = has_depth_order_hierarchy_enabled_barrier(queryInfo);
					if (needsBarrierCache)
						const_cast<QueryInfo&>(queryInfo).ensure_depth_order_hierarchy_barrier_cache();
					uint32_t idxFrom = 0;
					uint32_t idxTo = (uint32_t)cacheView.size();
					if (hasRuntimeGroupFilter) {
						const auto* pGroupData = queryInfo.selected_group_data(m_groupIdSet);
						if (pGroupData == nullptr)
							return 0;
						idxFrom = pGroupData->idxFirst;
						idxTo = pGroupData->idxLast + 1;
					}

					for (uint32_t qi = idxFrom; qi < idxTo; ++qi) {
						const auto* pArchetype = cacheView[qi];
						const bool barrierPasses = !needsBarrierCache || queryInfo.barrier_passes(qi);
						if GAIA_UNLIKELY (!can_process_archetype_inter<TIter>(queryInfo, *pArchetype, barrierPasses))
							continue;

						GAIA_PROF_SCOPE(query::count);

						const auto& chunks = pArchetype->chunks();
						if (!hasEntityFilters) {
							for (auto* pChunk: chunks) {
								const auto entityCnt = TIter::size(pChunk);
								if (entityCnt == 0)
									continue;

								if constexpr (UseFilters) {
									if (!match_filters(*pChunk, queryInfo, m_changedWorldVersion))
										continue;
								}

								cnt += entityCnt;
							}
							continue;
						}

						TIter it;
						it.set_world(queryInfo.world());
						it.set_archetype(pArchetype);
						for (auto* pChunk: chunks) {
							it.set_chunk(pChunk);

							const auto entityCnt = it.size();
							if (entityCnt == 0)
								continue;

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

				template <typename TIter>
				static void init_direct_entity_iter(
						const QueryInfo& queryInfo, const World& world, const EntityContainer& ec, TIter& it, uint8_t* pIndices,
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
							if (!queryId.pair() && world_is_out_of_line_component(world, queryId)) {
								const auto compIdx = core::get_index_unsafe(ec.pArchetype->ids_view(), queryId);
								GAIA_ASSERT(compIdx != BadIndex);
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
						it.set_term_ids(pTermIds);
						pLastArchetype = ec.pArchetype;
					}

					it.set_chunk(ec.pChunk, ec.row, (uint16_t)(ec.row + 1));
					it.set_group_id(0);
				}

				template <typename TIter>
				static void init_direct_entity_iter(
						const QueryInfo& queryInfo, const World& world, Entity entity, TIter& it, uint8_t* pIndices,
						Entity* pTermIds) {
					const auto& ec = ::gaia::ecs::fetch(world, entity);
					const Archetype* pLastArchetype = nullptr;
					it.set_world(&world);
					init_direct_entity_iter(queryInfo, world, ec, it, pIndices, pTermIds, pLastArchetype);
				}

				template <typename TIter>
				static void
				init_direct_entity_iter_basic(const EntityContainer& ec, TIter& it, const Archetype*& pLastArchetype) {
					GAIA_ASSERT(ec.pArchetype != nullptr);
					GAIA_ASSERT(ec.pChunk != nullptr);
					GAIA_ASSERT(ec.row < ec.pChunk->size());

					if (ec.pArchetype != pLastArchetype) {
						it.set_archetype(ec.pArchetype);
						pLastArchetype = ec.pArchetype;
					}
					it.set_chunk(ec.pChunk, ec.row, (uint16_t)(ec.row + 1));
					it.set_group_id(0);
				}

				template <typename TIter, typename Func>
				void each_chunk_runs_iter(QueryInfo& queryInfo, std::span<const detail::BfsChunkRun> runs, Func func) {
					auto& world = *queryInfo.world();
					TIter it;
					it.set_world(&world);
					it.set_write_im(false);
					const Archetype* pLastArchetype = nullptr;
					uint8_t indices[ChunkHeader::MAX_COMPONENTS];
					Entity termIds[ChunkHeader::MAX_COMPONENTS];

					for (const auto& run: runs) {
						const auto& ec = ::gaia::ecs::fetch(world, run.pChunk->entity_view()[run.from]);
						init_direct_entity_iter(queryInfo, world, ec, it, indices, termIds, pLastArchetype);
						it.set_chunk(run.pChunk, run.from, run.to);
						it.set_group_id(0);
						func(it);
						finish_iter_writes(it);
						it.clear_touched_writes();
					}
				}

				template <typename TIter, typename Func, typename... T>
				void each_chunk_runs(
						QueryInfo& queryInfo, std::span<const detail::BfsChunkRun> runs, Func func,
						[[maybe_unused]] core::func_type_list<T...>) {
					auto& world = *queryInfo.world();
					const bool canUseBasicInit = (can_use_direct_bfs_chunk_term_eval<T>(world, queryInfo) && ...);
					if constexpr ((can_use_raw_chunk_row_arg<T>() && ...)) {
						if (canUseBasicInit) {
							for (const auto& run: runs)
								run_query_on_chunk_rows_direct(run.pChunk, run.from, run.to, func, core::func_type_list<T...>{});
							return;
						}
					}

					if (canUseBasicInit) {
						TIter it;
						it.set_world(&world);
						const Archetype* pLastArchetype = nullptr;
						for (const auto& run: runs) {
							if (run.pArchetype != pLastArchetype) {
								it.set_archetype(run.pArchetype);
								pLastArchetype = run.pArchetype;
							}

							it.set_chunk(run.pChunk, run.from, run.to);
							it.set_group_id(0);
							run_query_on_chunk_direct(it, func, core::func_type_list<T...>{});
						}
						return;
					}

					TIter it;
					it.set_world(&world);
					const Archetype* pLastArchetype = nullptr;
					uint8_t indices[ChunkHeader::MAX_COMPONENTS];
					Entity termIds[ChunkHeader::MAX_COMPONENTS];
					for (const auto& run: runs) {
						const auto& ec = ::gaia::ecs::fetch(world, run.pChunk->entity_view()[run.from]);
						init_direct_entity_iter(queryInfo, world, ec, it, indices, termIds, pLastArchetype);
						it.set_chunk(run.pChunk, run.from, run.to);
						it.set_group_id(0);
						run_query_on_chunk_direct(it, func, core::func_type_list<T...>{});
					}
				}

				template <typename TIter, typename Func>
				void each_direct_entities_iter(QueryInfo& queryInfo, std::span<const Entity> entities, Func func) {
					auto& world = *queryInfo.world();
					auto& walkData = ensure_each_walk_data();
					TIter it;
					it.set_world(&world);
					it.set_write_im(false);
					if (!walkData.cachedRuns.empty()) {
						each_chunk_runs_iter<TIter>(queryInfo, walkData.cachedRuns, func);
						return;
					}

					const Archetype* pLastArchetype = nullptr;
					uint8_t indices[ChunkHeader::MAX_COMPONENTS];
					Entity termIds[ChunkHeader::MAX_COMPONENTS];
					for (const auto entity: entities) {
						const auto& ec = ::gaia::ecs::fetch(world, entity);
						init_direct_entity_iter(queryInfo, world, ec, it, indices, termIds, pLastArchetype);
						func(it);
						finish_iter_writes(it);
						it.clear_touched_writes();
					}
				}

				template <typename T>
				GAIA_NODISCARD static bool can_use_direct_bfs_chunk_term_eval(World& world, const QueryInfo& queryInfo) {
					using Arg = std::remove_cv_t<std::remove_reference_t<T>>;
					if constexpr (std::is_same_v<Arg, Entity>)
						return true;
					else {
						using FT = typename component_type_t<Arg>::TypeFull;
						if constexpr (is_pair<FT>::value)
							return false;
						const auto id = comp_cache(world).template get<FT>().entity;
						if (world_is_out_of_line_component(world, id))
							return false;
						for (const auto& term: queryInfo.ctx().data.terms_view()) {
							if (term.id != id)
								continue;
							if (term.src != EntityBad || term.entTrav != EntityBad || term_has_variables(term))
								return false;
							if (uses_non_direct_is_matching(term) || uses_inherited_id_matching(world, term) ||
									is_adjunct_direct_term(world, term))
								return false;
							return true;
						}

						return false;
					}
				}

				template <typename... T>
				GAIA_NODISCARD static bool can_use_direct_chunk_term_eval(World& world, const QueryInfo& queryInfo) {
					if (queryInfo.has_entity_filter_terms())
						return false;

					if constexpr (sizeof...(T) == 0)
						return true;
					else
						return (can_use_direct_bfs_chunk_term_eval<T>(world, queryInfo) && ...);
				}

				template <typename T>
				GAIA_NODISCARD static constexpr bool can_use_raw_chunk_row_arg() {
					using Arg = std::remove_cv_t<std::remove_reference_t<T>>;
					if constexpr (std::is_same_v<Arg, Entity>)
						return true;
					else
						return !std::is_lvalue_reference_v<T> || std::is_const_v<std::remove_reference_t<T>>;
				}

				template <typename TIter, typename Func, typename... T>
				void each_direct_entities(
						QueryInfo& queryInfo, std::span<const Entity> entities, Func func,
						[[maybe_unused]] core::func_type_list<T...>) {
					auto& world = *queryInfo.world();
					const bool canUseBasicInit = (can_use_direct_bfs_chunk_term_eval<T>(world, queryInfo) && ...);
					auto& walkData = ensure_each_walk_data();
					if (!walkData.cachedRuns.empty()) {
						each_chunk_runs<TIter>(queryInfo, walkData.cachedRuns, func, core::func_type_list<T...>{});
						return;
					}

					TIter it;
					it.set_world(&world);
					const Archetype* pLastArchetype = nullptr;
					uint8_t indices[ChunkHeader::MAX_COMPONENTS];
					Entity termIds[ChunkHeader::MAX_COMPONENTS];
					for (const auto entity: entities) {
						const auto& ec = ::gaia::ecs::fetch(world, entity);
						if (canUseBasicInit)
							init_direct_entity_iter_basic(ec, it, pLastArchetype);
						else
							init_direct_entity_iter(queryInfo, world, ec, it, indices, termIds, pLastArchetype);

						if (canUseBasicInit)
							run_query_on_direct_entity_direct(it, func, core::func_type_list<T...>{});
						else
							run_query_on_direct_entity(it, func, core::func_type_list<T...>{});
					}
				}

				template <typename T>
				static Entity inherited_query_arg_id(World& world) {
					using Arg = std::remove_cv_t<std::remove_reference_t<T>>;
					if constexpr (std::is_same_v<Arg, Entity>)
						return EntityBad;
					else {
						using FT = typename component_type_t<Arg>::TypeFull;
						if constexpr (is_pair<FT>::value) {
							const auto rel = comp_cache(world).template get<typename FT::rel>().entity;
							const auto tgt = comp_cache(world).template get<typename FT::tgt>().entity;
							return (Entity)Pair(rel, tgt);
						} else
							return comp_cache(world).template get<FT>().entity;
					}
				}

				template <typename T>
				static decltype(auto) inherited_query_entity_arg_by_id(World& world, Entity entity, Entity termId) {
					using Arg = std::remove_cv_t<std::remove_reference_t<T>>;
					if constexpr (std::is_same_v<Arg, Entity>)
						return entity;
					else if constexpr (std::is_lvalue_reference_v<T> && !std::is_const_v<std::remove_reference_t<T>>)
						return world_query_entity_arg_by_id_raw<T>(world, entity, termId);
					else
						return world_query_entity_arg_by_id<T>(world, entity, termId);
				}

				template <typename T>
				static decltype(auto) inherited_query_entity_arg_by_id_cached(
						World& world, Entity entity, Entity termId, const Archetype*& pLastArchetype, Entity& cachedOwner,
						bool& cachedDirect) {
					using Arg = std::remove_cv_t<std::remove_reference_t<T>>;
					if constexpr (std::is_same_v<Arg, Entity>)
						return entity;
					else if constexpr (std::is_lvalue_reference_v<T> && !std::is_const_v<std::remove_reference_t<T>>)
						return inherited_query_entity_arg_by_id<T>(world, entity, termId);
					else
						return world_query_entity_arg_by_id_cached_const<T>(
								world, entity, termId, pLastArchetype, cachedOwner, cachedDirect);
				}

				template <typename... T, typename Func, size_t... I>
				static void invoke_inherited_query_args_by_id(
						World& world, Entity entity, const Entity* pArgIds, Func& func, std::index_sequence<I...>) {
					func(inherited_query_entity_arg_by_id<T>(world, entity, pArgIds[I])...);
				}

				template <typename... T, typename Func, size_t... I>
				static void invoke_inherited_query_args_by_id_cached(
						World& world, Entity entity, const Entity* pArgIds, const Archetype** pLastArchetypes,
						Entity* pCachedOwners, bool* pCachedDirect, Func& func, std::index_sequence<I...>) {
					func(
							inherited_query_entity_arg_by_id_cached<T>(
									world, entity, pArgIds[I], pLastArchetypes[I], pCachedOwners[I], pCachedDirect[I])...);
				}

				template <typename... T, typename Func, size_t... I>
				static void invoke_query_args_by_id(
						World& world, Entity entity, const Entity* pArgIds, Func& func, std::index_sequence<I...>) {
					func(world_query_entity_arg_by_id<T>(world, entity, pArgIds[I])...);
				}

				template <typename... T, size_t... I>
				static void
				finish_query_args_by_id(World& world, Entity entity, const Entity* pArgIds, std::index_sequence<I...>) {
					Entity seenTerms[sizeof...(T) > 0 ? sizeof...(T) : 1]{};
					uint32_t seenCnt = 0;
					const auto finish_term = [&](Entity term) {
						GAIA_FOR(seenCnt) {
							if (seenTerms[i] == term)
								return;
						}
						seenTerms[seenCnt++] = term;
						world_finish_write(world, term, entity);
					};

					(
							[&] {
								if constexpr (is_write_query_arg<T>())
									finish_term(pArgIds[I]);
							}(),
							...);
				}

				//! Runs an iterator-based each() callback over directly seeded entities using one-row chunk views.
				template <typename TIter, typename Func>
				void each_direct_iter_inter(QueryInfo& queryInfo, Func func) {
					auto& world = *queryInfo.world();
					const bool hasWriteTerms = queryInfo.ctx().data.readWriteMask != 0;
					const auto plan = direct_entity_seed_plan(world, queryInfo);

					auto exec_entity = [&](Entity entity) {
						uint8_t indices[ChunkHeader::MAX_COMPONENTS];
						Entity termIds[ChunkHeader::MAX_COMPONENTS];
						TIter it;
						init_direct_entity_iter(queryInfo, world, entity, it, indices, termIds);
						it.set_write_im(false);
						func(it);
						finish_iter_writes(it);
					};

					if (hasWriteTerms) {
						auto& scratch = direct_query_scratch();
						// Writable callbacks may add local overrides and reshuffle direct-term indices,
						// so direct-seeded execution must iterate a stable snapshot.
						const auto seedInfo = build_direct_entity_seed(world, queryInfo, scratch.entities);
						for (const auto entity: scratch.entities) {
							if (!match_direct_entity_constraints<TIter>(world, queryInfo, entity))
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
							each_chunk_runs_iter<TIter>(
									queryInfo, cached_direct_seed_runs<TIter>(queryInfo, *pSeedTerm, seedInfo), func);
							return;
						}
					}

					if (plan.preferOrSeed) {
						for_each_direct_or_union<TIter>(world, queryInfo, exec_entity);
						return;
					}

					(void)for_each_direct_all_seed<TIter>(world, queryInfo, plan, [&](Entity entity) {
						exec_entity(entity);
						return true;
					});
				}

				//! Runs a typed each() callback over directly seeded entities.
				template <typename TIter, typename Func, typename... T>
				void each_direct_inter(QueryInfo& queryInfo, Func func, [[maybe_unused]] core::func_type_list<T...>) {
					constexpr bool needsInheritedArgIds =
							(!std::is_same_v<std::remove_cv_t<std::remove_reference_t<T>>, Entity> || ... || false);

					auto& world = *queryInfo.world();
					const auto plan = direct_entity_seed_plan(world, queryInfo);
					const bool hasWriteTerms = queryInfo.ctx().data.readWriteMask != 0;
					bool hasInheritedTerms = false;
					for (const auto& term: queryInfo.ctx().data.terms_view()) {
						if (uses_inherited_id_matching(world, term)) {
							hasInheritedTerms = true;
							break;
						}
					}

					if (!hasWriteTerms && !plan.preferOrSeed) {
						const auto* pSeedTerm = find_direct_all_seed_term(queryInfo, plan);
						if (pSeedTerm != nullptr && can_use_direct_seed_run_cache(world, queryInfo, *pSeedTerm)) {
							DirectEntitySeedInfo seedInfo{};
							seedInfo.seededAllTerm = pSeedTerm->id;
							seedInfo.seededAllMatchKind = pSeedTerm->matchKind;
							seedInfo.seededFromAll = true;
							if (!hasInheritedTerms) {
								each_chunk_runs<TIter>(
										queryInfo, cached_direct_seed_runs<TIter>(queryInfo, *pSeedTerm, seedInfo), func,
										core::func_type_list<T...>{});
								return;
							}

							const auto entities = cached_direct_seed_entities<TIter>(queryInfo, *pSeedTerm, seedInfo);
							if constexpr (sizeof...(T) > 0) {
								Entity argIds[sizeof...(T)] = {inherited_query_arg_id<T>(world)...};
								const Archetype* lastArchetypes[sizeof...(T)]{};
								Entity cachedOwners[sizeof...(T)]{};
								bool cachedDirect[sizeof...(T)]{};
								for (const auto entity: entities) {
									invoke_inherited_query_args_by_id_cached<T...>(
											world, entity, argIds, lastArchetypes, cachedOwners, cachedDirect, func,
											std::index_sequence_for<T...>{});
								}
							} else {
								for (const auto entity: entities) {
									(void)entity;
									func();
								}
							}
							return;
						}
					}

					auto exec_direct_entity = [&](Entity entity) {
						uint8_t indices[ChunkHeader::MAX_COMPONENTS];
						Entity termIds[ChunkHeader::MAX_COMPONENTS];
						TIter it;
						init_direct_entity_iter(queryInfo, world, entity, it, indices, termIds);
						// Entity filters already ran in the seed walker, so invoke the inner loop directly.
						run_query_on_chunk_direct(it, func, core::func_type_list<T...>{});
					};
					auto walk_entities = [&](auto&& exec_entity) {
						if (hasWriteTerms) {
							auto& scratch = direct_query_scratch();
							// Writable callbacks may add local overrides and reshuffle direct-term indices,
							// so direct-seeded execution must iterate a stable snapshot.
							const auto seedInfo = build_direct_entity_seed(world, queryInfo, scratch.entities);
							for (const auto entity: scratch.entities) {
								if (!match_direct_entity_constraints<TIter>(world, queryInfo, entity))
									continue;
								if (!match_direct_entity_terms(world, entity, queryInfo, seedInfo))
									continue;
								exec_entity(entity);
							}
							return;
						}

						if (plan.preferOrSeed) {
							for_each_direct_or_union<TIter>(world, queryInfo, [&](Entity entity) {
								exec_entity(entity);
								return true;
							});
							return;
						}

						(void)for_each_direct_all_seed<TIter>(world, queryInfo, plan, [&](Entity entity) {
							exec_entity(entity);
							return true;
						});
					};

					if constexpr (!needsInheritedArgIds)
						walk_entities(exec_direct_entity);
					else {
						Entity inheritedArgIds[sizeof...(T) > 0 ? sizeof...(T) : 1] = {inherited_query_arg_id<T>(world)...};
						auto exec_entity = [&](Entity entity) {
							if (hasInheritedTerms) {
								invoke_inherited_query_args_by_id<T...>(
										world, entity, inheritedArgIds, func, std::index_sequence_for<T...>{});
								finish_query_args_by_id<T...>(world, entity, inheritedArgIds, std::index_sequence_for<T...>{});
								return;
							}

							exec_direct_entity(entity);
						};

						walk_entities(exec_entity);
					}
				}

				template <bool UseFilters, typename TIter, typename ContainerOut>
				void arr_inter(QueryInfo& queryInfo, ContainerOut& outArray) {
					using ContainerItemType = typename ContainerOut::value_type;
					if constexpr (!UseFilters) {
						if (can_use_direct_entity_seed_eval(queryInfo)) {
							auto& world = *queryInfo.world();
							const auto plan = direct_entity_seed_plan(world, queryInfo);
							if (plan.preferOrSeed) {
								for_each_direct_or_union<TIter>(world, queryInfo, [&](Entity entity) {
									if constexpr (std::is_same_v<ContainerItemType, Entity>)
										outArray.push_back(entity);
									else {
										auto tmp = world_direct_entity_arg<ContainerItemType>(world, entity);
										outArray.push_back(tmp);
									}
								});
								return;
							}

							(void)for_each_direct_all_seed<TIter>(world, queryInfo, plan, [&](Entity entity) {
								if constexpr (std::is_same_v<ContainerItemType, Entity>)
									outArray.push_back(entity);
								else {
									auto tmp = world_direct_entity_arg<ContainerItemType>(world, entity);
									outArray.push_back(tmp);
								}
								return true;
							});

							return;
						}
					}

					auto& world = *const_cast<World*>(queryInfo.world());
					if constexpr (!UseFilters) {
						if (!queryInfo.has_entity_filter_terms() &&
								can_use_direct_chunk_term_eval<ContainerItemType>(world, queryInfo) &&
								can_use_direct_chunk_iteration_fastpath(queryInfo)) {
							const auto cacheView = queryInfo.cache_archetype_view();
							uint32_t idxFrom = 0;
							uint32_t idxTo = (uint32_t)cacheView.size();
							if (queryInfo.ctx().data.groupBy != EntityBad && m_groupIdSet != 0) {
								const auto* pGroupData = queryInfo.selected_group_data(m_groupIdSet);
								if (pGroupData == nullptr)
									return;
								idxFrom = pGroupData->idxFirst;
								idxTo = pGroupData->idxLast + 1;
							}

							for (uint32_t i = idxFrom; i < idxTo; ++i) {
								auto* pArchetype = cacheView[i];
								if GAIA_UNLIKELY (!can_process_archetype_inter<TIter>(queryInfo, *pArchetype))
									continue;

								GAIA_PROF_SCOPE(query::arr);

								const auto& chunks = pArchetype->chunks();
								for (auto* pChunk: chunks) {
									const auto from = TIter::start_index(pChunk);
									const auto to = TIter::end_index(pChunk);
									if GAIA_UNLIKELY (from == to)
										continue;

									const auto dataView = chunk_view_auto<ContainerItemType>(pChunk);
									for (uint16_t row = from; row < to; ++row)
										outArray.push_back(dataView[row]);
								}
							}

							return;
						}
					}

					TIter it;
					it.set_world(queryInfo.world());
					const bool hasEntityFilters = queryInfo.has_entity_filter_terms();
					const auto cacheView = queryInfo.cache_archetype_view();
					const auto sortView = queryInfo.cache_sort_view();
					const bool needsBarrierCache = has_depth_order_hierarchy_enabled_barrier(queryInfo);
					if (needsBarrierCache)
						const_cast<QueryInfo&>(queryInfo).ensure_depth_order_hierarchy_barrier_cache();
					uint32_t idxFrom = 0;
					uint32_t idxTo = (uint32_t)cacheView.size();
					if (queryInfo.ctx().data.groupBy != EntityBad && m_groupIdSet != 0) {
						const auto* pGroupData = queryInfo.selected_group_data(m_groupIdSet);
						if (pGroupData == nullptr)
							return;
						idxFrom = pGroupData->idxFirst;
						idxTo = pGroupData->idxLast + 1;
					}

					const auto append_rows = [&](const uint32_t archetypeIdx, auto* pArchetype, auto* pChunk, uint16_t from,
																			 uint16_t to) {
						const bool barrierPasses = !needsBarrierCache || queryInfo.barrier_passes(archetypeIdx);
						if GAIA_UNLIKELY (!can_process_archetype_inter<TIter>(queryInfo, *pArchetype, barrierPasses))
							return;

						GAIA_PROF_SCOPE(query::arr);

						it.set_archetype(pArchetype);
						it.set_chunk(pChunk, from, to);

						const auto cnt = it.size();
						if (cnt == 0)
							return;

						if constexpr (UseFilters) {
							if (!match_filters(*pChunk, queryInfo, m_changedWorldVersion))
								return;
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
					};

					if (!sortView.empty()) {
						for (const auto& view: sortView) {
							if (view.archetypeIdx < idxFrom || view.archetypeIdx >= idxTo)
								continue;

							const auto minStartRow = TIter::start_index(view.pChunk);
							const auto minEndRow = TIter::end_index(view.pChunk);
							const auto viewFrom = view.startRow;
							const auto viewTo = (uint16_t)(view.startRow + view.count);
							const auto startRow = core::get_max(minStartRow, viewFrom);
							const auto endRow = core::get_min(minEndRow, viewTo);
							if (startRow == endRow)
								continue;

							append_rows(
									view.archetypeIdx, const_cast<Archetype*>(cacheView[view.archetypeIdx]), view.pChunk, startRow,
									endRow);
						}
						return;
					}

					for (uint32_t i = idxFrom; i < idxTo; ++i) {
						auto* pArchetype = cacheView[i];
						const auto& chunks = pArchetype->chunks();
						for (auto* pChunk: chunks)
							append_rows(i, pArchetype, pChunk, 0, 0);
					}
				}

			public:
				QueryImpl() = default;
				~QueryImpl() = default;

				QueryImpl(
						World& world, QueryCache& queryCache, ArchetypeId& nextArchetypeId, uint32_t& worldVersion,
						const EntityToArchetypeMap& entityToArchetypeMap, const ArchetypeDArray& allArchetypes):
						m_nextArchetypeId(&nextArchetypeId), m_worldVersion(&worldVersion),
						m_entityToArchetypeMap(&entityToArchetypeMap), m_allArchetypes(&allArchetypes) {
					m_storage.init(&world, &queryCache);
				}

				GAIA_NODISCARD QueryId id() const {
					if (!uses_query_cache_storage())
						return QueryIdBad;
					return m_storage.m_identity.handle.id();
				}

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

				void destroy() {
					(void)m_storage.try_del_from_cache();
					m_eachWalkData.reset();
					m_directSeedRunData.reset();
					reset_changed_filter_state();
					invalidate_each_walk_cache();
					invalidate_direct_seed_run_cache();
				}

				//! Returns true if the query is stored in the query cache
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

				QueryImpl& add(QueryInput item) {
					// Add commands to the command buffer
					add_inter(item);
					return *this;
				}

				//------------------------------------------------

				QueryImpl& is(Entity entity, const QueryTermOptions& options = QueryTermOptions{}) {
					return all(Pair(Is, entity), options);
				}

				//------------------------------------------------

				QueryImpl& in(Entity entity, QueryTermOptions options = QueryTermOptions{}) {
					options.in();
					return all(Pair(Is, entity), options);
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

				class OrderByWalkView final {
					QueryImpl* m_query = nullptr;
					Entity m_relation = EntityBad;

				public:
					OrderByWalkView(QueryImpl& query, Entity relation): m_query(&query), m_relation(relation) {}

					template <typename Func>
					void each(Func func) {
						m_query->each_walk(func, m_relation);
					}
				};

				//------------------------------------------------

				//! Walks the relation graph in breadth-first levels for the given relation.
				//! Pair(relation, X) on entity E means E depends on X.
				//! This path evaluates traversal per entity, so it works for both fragmenting relations
				//! such as ChildOf and non-fragmenting relations such as Parent.
				GAIA_NODISCARD OrderByWalkView walk(Entity relation) {
					return OrderByWalkView(*this, relation);
				}

				//------------------------------------------------

				//! Orders cached query entries by fragmenting relation depth so iteration runs breadth-first top-down.
				//! Intended only for fragmenting relations such as ChildOf or DependsOn where the target
				//! participates in archetype identity. Unlike walk(...), this affects the cached query
				//! iteration order itself and can therefore prune fragmenting disabled subtrees at the
				//! archetype level. For non-fragmenting relations such as Parent, use walk(...) instead.
				//! \param relation Fragmenting hierarchy relation
				QueryImpl& depth_order(Entity relation = ChildOf) {
					GAIA_ASSERT(!relation.pair());
					GAIA_ASSERT(world_supports_depth_order(*m_storage.world(), relation));
					group_by_inter(relation, group_by_func_depth_order);
					return *this;
				}

				//! Orders cached query entries by fragmenting relation depth so iteration runs breadth-first top-down.
				//! \tparam Rel Fragmenting hierarchy relation, typically ChildOf.
				template <typename Rel>
				QueryImpl& depth_order() {
					using UO = typename component_type_t<Rel>::TypeOriginal;
					static_assert(core::is_raw_v<UO>, "Use depth_order() with raw relation types only");

					const auto& desc = comp_cache_add<Rel>(*m_storage.world());
					return depth_order(desc.entity);
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
				QueryImpl& group_dep() {
					group_dep_inter<Rel>();
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
				//! \note For changed() queries this is a non-consuming probe. It does not advance the
				//!       query's changed-reporting state. Iteration APIs such as each()/arr() do consume it.
				//!	\return True if there are any entities matching the query. False otherwise.
				bool empty(Constraints constraints = Constraints::EnabledOnly) {
					auto& queryInfo = fetch();
					if (!queryInfo.has_filters() && m_groupIdSet == 0 && can_use_direct_entity_seed_eval(queryInfo)) {
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
				//! \note For changed() queries this is a non-consuming probe. It does not advance the
				//!       query's changed-reporting state. Iteration APIs such as each()/arr() do consume it.
				//! \return The number of matching entities
				uint32_t count(Constraints constraints = Constraints::EnabledOnly) {
					auto& queryInfo = fetch();
					if (!queryInfo.has_filters() && m_groupIdSet == 0 && can_use_direct_entity_seed_eval(queryInfo)) {
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

				GAIA_NODISCARD std::span<const Entity> ordered_entities_walk(
						QueryInfo& queryInfo, Entity relation, Constraints constraints = Constraints::EnabledOnly) {
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

					if (walkData.cacheValid && walkData.cachedRelation == relation && walkData.cachedConstraints == constraints &&
							walkData.cachedRelationVersion == relationVersion &&
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

					if (walkData.cacheValid && walkData.cachedRelation == relation && walkData.cachedConstraints == constraints &&
							walkData.cachedRelationVersion == relationVersion &&
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

						auto& currLevel = walkData.scratchCurrLevel;
						currLevel.clear();
						auto& nextLevel = walkData.scratchNextLevel;
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

					walkData.cachedRelation = relation;
					walkData.cachedConstraints = constraints;
					walkData.cachedRelationVersion = relationVersion;
					walkData.cachedEntityVersion = ::gaia::ecs::world_version(world);
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

				//! Iterates entities matching the query ordered in dependency BFS levels.
				//! For relation R this treats Pair(R, X) on entity E as "E depends on X".
				//! Systems depending on no other matched system are first, then their dependents level-by-level.
				//! Nodes on the same level are ordered by entity id.
				//! \param func Callable invoked for each ordered entity.
				//! \param relation Dependency relation
				//! \param constraints QueryImpl constraints
				template <typename Func>
				void each_walk(Func func, Entity relation, Constraints constraints = Constraints::EnabledOnly) {
					auto& queryInfo = fetch();
					match_all(queryInfo);
					const auto ordered = ordered_entities_walk(queryInfo, relation, constraints);

					if constexpr (std::is_invocable_v<Func, IterAll&>) {
						each_direct_entities_iter<IterAll>(queryInfo, ordered, func);
					} else if constexpr (std::is_invocable_v<Func, Iter&>) {
						each_direct_entities_iter<Iter>(queryInfo, ordered, func);
					} else if constexpr (std::is_invocable_v<Func, IterDisabled&>) {
						each_direct_entities_iter<IterDisabled>(queryInfo, ordered, func);
					} else if constexpr (std::is_invocable_v<Func, const Entity&> || std::is_invocable_v<Func, Entity>) {
						for (const auto entity: ordered)
							func(entity);
					} else {
						using InputArgs = decltype(core::func_args(&Func::operator()));
						GAIA_ASSERT(unpack_args_into_query_has_all(queryInfo, InputArgs{}));
						GAIA_ASSERT(can_use_direct_target_eval(queryInfo));
						if (!can_use_direct_target_eval(queryInfo))
							return;

						switch (constraints) {
							case Constraints::EnabledOnly:
								each_direct_entities<Iter>(queryInfo, ordered, func, InputArgs{});
								break;
							case Constraints::DisabledOnly:
								each_direct_entities<IterDisabled>(queryInfo, ordered, func, InputArgs{});
								break;
							case Constraints::AcceptAll:
								each_direct_entities<IterAll>(queryInfo, ordered, func, InputArgs{});
								break;
						}
					}
				}

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
		//! Marker type used by tests to request World::uquery().
		struct QueryUncached {};
	} // namespace ecs
} // namespace gaia
