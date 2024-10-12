#pragma once
#include "../config/config.h"

#include <cstdarg>
#include <type_traits>

#include "../cnt/darray.h"
#include "../cnt/map.h"
#include "../cnt/sarray_ext.h"
#include "../config/profiler.h"
#include "../core/hashing_policy.h"
#include "../core/utility.h"
#include "../ser/serialization.h"
#include "archetype.h"
#include "archetype_common.h"
#include "chunk.h"
#include "chunk_iterator.h"
#include "common.h"
#include "component.h"
#include "component_cache.h"
#include "data_buffer.h"
#include "id.h"
#include "query_cache.h"
#include "query_common.h"
#include "query_info.h"

namespace gaia {
	namespace ecs {
		class World;

		QuerySerBuffer& query_buffer(World& world);
		const ComponentCache& comp_cache(const World& world);
		ComponentCache& comp_cache_mut(World& world);
		template <typename T>
		const ComponentCacheItem& comp_cache_add(World& world);
		bool is_base(const World& world, Entity entity);

		namespace detail {
			//! Query command types
			enum QueryCmdType : uint8_t { ADD_ITEM, ADD_FILTER, GROUP_BY, SET_GROUP, FILTER_GROUP };

			struct QueryCmd_AddItem {
				static constexpr QueryCmdType Id = QueryCmdType::ADD_ITEM;
				static constexpr bool InvalidatesHash = true;

				QueryInput item;

				void exec(QueryCtx& ctx) const {
					auto& data = ctx.data;
					auto& ids = data.ids;
					auto& terms = data.terms;

					// Unique component ids only
					GAIA_ASSERT(!core::has(ids, item.id));

#if GAIA_DEBUG
					// There's a limit to the amount of query items which we can store
					if (ids.size() >= MAX_ITEMS_IN_QUERY) {
						GAIA_ASSERT2(false, "Trying to create a query with too many components!");

						const auto* name = ctx.cc->get(item.id).name.str();
						GAIA_LOG_E("Trying to add component '%s' to an already full ECS query!", name);
						return;
					}
#endif

					// Build the read-write mask.
					// This will be used to determine what kind of access the user wants for a given component.
					const uint8_t isReadWrite = uint8_t(item.access == QueryAccess::Write);
					data.readWriteMask |= (isReadWrite << (uint8_t)ids.size());

					// Build the Is mask.
					// We will use it to identify entities with an Is relationship quickly.
					if (!item.id.pair()) {
						const auto has_as = (uint32_t)is_base(*ctx.w, item.id);
						data.as_mask_0 |= (has_as << ids.size());
					} else {
						if (!is_wildcard(item.id.id())) {
							const auto e = entity_from_id(*ctx.w, item.id.id());
							const auto has_as = (uint32_t)is_base(*ctx.w, e);
							data.as_mask_0 |= (has_as << ids.size());
						}

						if (!is_wildcard(item.id.gen())) {
							const auto e = entity_from_id(*ctx.w, item.id.gen());
							const auto has_as = (uint32_t)is_base(*ctx.w, e);
							data.as_mask_1 |= (has_as << ids.size());
						}
					}

					// The query engine is going to reorder the query items as necessary.
					// Remapping is used so the user can still identify the items according the order in which
					// they defined them when building the query.
					data.remapping.push_back((uint8_t)data.remapping.size());

					ids.push_back(item.id);
					terms.push_back({item.id, item.src, nullptr, item.op});
				}
			};

			struct QueryCmd_AddFilter {
				static constexpr QueryCmdType Id = QueryCmdType::ADD_FILTER;
				static constexpr bool InvalidatesHash = true;

				Entity comp;

				void exec(QueryCtx& ctx) const {
					auto& data = ctx.data;
					auto& ids = data.ids;
					auto& changed = data.changed;
					const auto& terms = data.terms;

					GAIA_ASSERT(core::has(ids, comp));
					GAIA_ASSERT(!core::has(changed, comp));

#if GAIA_DEBUG
					// There's a limit to the amount of components which we can store
					if (changed.size() >= MAX_ITEMS_IN_QUERY) {
						GAIA_ASSERT2(false, "Trying to create an filter query with too many components!");

						auto compName = ctx.cc->get(comp).name.str();
						GAIA_LOG_E("Trying to add component %s to an already full filter query!", compName);
						return;
					}
#endif

					uint32_t compIdx = 0;
					for (; compIdx < ids.size(); ++compIdx)
						if (ids[compIdx] == comp)
							break;
					// NOTE: This code bellow does technically the same as above.
					//       However, compilers can't quite optimize it as well because it does some more
					//       calculations. This is used often so go with the custom code.
					// const auto compIdx = core::get_index_unsafe(ids, comp);

					// Component has to be present in anyList or allList.
					// NoneList makes no sense because we skip those in query processing anyway.
					if (terms[compIdx].op != QueryOpKind::Not) {
						changed.push_back(comp);
						return;
					}

					GAIA_ASSERT2(false, "SetChangeFilter trying to filter component which is not a part of the query");
#if GAIA_DEBUG
					auto compName = ctx.cc->get(comp).name.str();
					GAIA_LOG_E("SetChangeFilter trying to filter component %s but it's not a part of the query!", compName);
#endif
				}
			};

			struct QueryCmd_GroupBy {
				static constexpr QueryCmdType Id = QueryCmdType::GROUP_BY;
				static constexpr bool InvalidatesHash = true;

				Entity groupBy;
				TGroupByFunc func;

				void exec(QueryCtx& ctx) {
					auto& data = ctx.data;
					data.groupBy = groupBy;
					GAIA_ASSERT(func != nullptr);
					data.groupByFunc = func; // group_by_func_default;
				}
			};

			struct QueryCmd_SetGroupId {
				static constexpr QueryCmdType Id = QueryCmdType::SET_GROUP;
				static constexpr bool InvalidatesHash = false;

				GroupId groupId;

				void exec(QueryCtx& ctx) {
					auto& data = ctx.data;
					data.groupIdSet = groupId;
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
					m_queryInfo.gen = QueryIdBad;
				}

				GAIA_NODISCARD World* world() {
					return m_queryInfo.world();
				}

				GAIA_NODISCARD QuerySerBuffer& ser_buffer() {
					return m_queryInfo.ser_buffer();
				}
				void ser_buffer_reset() {
					return m_queryInfo.ser_buffer_reset();
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
					Chunk* pChunk;
					const uint8_t* pIndicesMapping;
					GroupId groupId;
				};

				using CmdBuffer = SerializationBufferDyn;
				using ChunkSpan = std::span<const Chunk*>;
				using ChunkSpanMut = std::span<Chunk*>;
				using ChunkBatchArray = cnt::sarray_ext<ChunkBatch, ChunkBatchSize>;
				using CmdFunc = void (*)(CmdBuffer& buffer, QueryCtx& ctx);

			private:
				static constexpr CmdFunc CommandBufferRead[] = {
						// Add item
						[](CmdBuffer& buffer, QueryCtx& ctx) {
							QueryCmd_AddItem cmd;
							ser::load(buffer, cmd);
							cmd.exec(ctx);
						},
						// Add filter
						[](CmdBuffer& buffer, QueryCtx& ctx) {
							QueryCmd_AddFilter cmd;
							ser::load(buffer, cmd);
							cmd.exec(ctx);
						},
						// GroupBy
						[](CmdBuffer& buffer, QueryCtx& ctx) {
							QueryCmd_GroupBy cmd;
							ser::load(buffer, cmd);
							cmd.exec(ctx);
						},
						// SetGroupId
						[](CmdBuffer& buffer, QueryCtx& ctx) {
							QueryCmd_SetGroupId cmd;
							ser::load(buffer, cmd);
							cmd.exec(ctx);
						}};

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

				//--------------------------------------------------------------------------------
			public:
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
								pQueryInfo->match(*m_entityToArchetypeMap, *m_allArchetypes, last_archetype_id());
								return *pQueryInfo;
							}

							m_storage.invalidate();
						}

						// No queryId is set which means QueryInfo needs to be created
						QueryCtx ctx;
						ctx.init(m_storage.world());
						commit(ctx);
						auto& queryInfo = m_storage.m_queryCache->add(GAIA_MOV(ctx), *m_entityToArchetypeMap);
						m_storage.m_q.handle = queryInfo.handle(queryInfo);
						m_storage.allow_to_destroy_again();
						queryInfo.match(*m_entityToArchetypeMap, *m_allArchetypes, last_archetype_id());
						return queryInfo;
					} else {
						GAIA_PROF_SCOPE(query::fetchu);

						if GAIA_UNLIKELY (m_storage.m_queryInfo.ctx().q.handle.id() == QueryIdBad) {
							QueryCtx ctx;
							ctx.init(m_storage.world());
							commit(ctx);
							m_storage.m_queryInfo =
									QueryInfo::create(QueryId{}, GAIA_MOV(ctx), *m_entityToArchetypeMap, *m_allArchetypes);
						}
						m_storage.m_queryInfo.match(*m_entityToArchetypeMap, *m_allArchetypes, last_archetype_id());
						return m_storage.m_queryInfo;
					}
				}

				//--------------------------------------------------------------------------------
			private:
				ArchetypeId last_archetype_id() const {
					return *m_nextArchetypeId - 1;
				}

				template <typename T>
				void add_cmd(T& cmd) {
					// Make sure to invalidate if necessary.
					if constexpr (T::InvalidatesHash)
						m_storage.invalidate();

					auto& serBuffer = m_storage.ser_buffer();
					ser::save(serBuffer, T::Id);
					ser::save(serBuffer, T::InvalidatesHash);
					ser::save(serBuffer, cmd);
				}

				void add_inter(QueryInput item) {
					// When excluding make sure the access type is None.
					GAIA_ASSERT(item.op != QueryOpKind::Not || item.access == QueryAccess::None);

					QueryCmd_AddItem cmd{item};
					add_cmd(cmd);
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
					if (op != QueryOpKind::Not) {
						constexpr auto isReadWrite = core::is_mut_v<T>;
						access = isReadWrite ? QueryAccess::Write : QueryAccess::Read;
					}

					add_inter({op, access, e});
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
					if (op != QueryOpKind::Not) {
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
					GAIA_PROF_SCOPE(commit);

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
						calc_lookup_hash(ctx);
					}

					// We can free all temporary data now
					m_storage.ser_buffer_reset();
				}

				void recommit(QueryCtx& ctx) {
					GAIA_PROF_SCOPE(recommit);

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

					// See if any component has changed
					const auto& filtered = queryInfo.filters();
					for (const auto comp: filtered) {
						// TODO: Components are sorted. Therefore, we don't need to search from 0
						//       all the time. We can search from the last found index.
						const auto compIdx = chunk.comp_idx(comp);
						if (chunk.changed(queryVersion, compIdx))
							return true;
					}

					// Skip unchanged chunks.
					return false;
				}

				//! Execute functors in batches
				template <typename Func, typename TIter>
				static void run_query_func(Func func, TIter& it, ChunkBatchArray& chunks) {
					GAIA_PROF_SCOPE(query::run_query_func);

					const auto chunkCnt = chunks.size();
					GAIA_ASSERT(chunkCnt > 0);

					auto runFunc = [&](ChunkBatch& batch) {
						auto* pChunk = batch.pChunk;

#if GAIA_ASSERT_ENABLED
						pChunk->lock(true);
#endif
						it.set_group_id(batch.groupId);
						it.set_remapping_indices(batch.pIndicesMapping);
						it.set_chunk(pChunk);
						func(it);
#if GAIA_ASSERT_ENABLED
						pChunk->lock(false);
#endif
					};

					// This is what the function is doing:
					// for (auto *pChunk: chunks) {
					//  pChunk->lock(true);
					//	runFunc(pChunk);
					//  pChunk->lock(false);
					// }
					// chunks.clear();

					// We only have one chunk to process
					if GAIA_UNLIKELY (chunkCnt == 1) {
						runFunc(chunks[0]);
						chunks.clear();
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
					gaia::prefetch(&chunks[1].pChunk, PrefetchHint::PREFETCH_HINT_T2);
					runFunc(chunks[0]);

					uint32_t chunkIdx = 1;
					for (; chunkIdx < chunkCnt - 1; ++chunkIdx) {
						gaia::prefetch(&chunks[chunkIdx + 1].pChunk, PrefetchHint::PREFETCH_HINT_T2);
						runFunc(chunks[chunkIdx]);
					}

					runFunc(chunks[chunkIdx]);

					chunks.clear();
				}

				GAIA_NODISCARD bool can_process_archetype(const Archetype& archetype) const {
					// Archetypes requested for deletion are skipped for processing
					return !archetype.is_req_del();
				}

				template <bool HasFilters, typename TIter, typename Func>
				void run_query(const QueryInfo& queryInfo, Func func) {
					ChunkBatchArray chunkBatch;
					TIter it;

					GAIA_PROF_SCOPE(query::run_query); // batch preparation + chunk processing

					// TODO: Have archetype cache as double-linked list with pointers only.
					//       Have chunk cache as double-linked list with pointers only.
					//       Make it so only valid pointers are linked together.
					//       This means one less indirection + we won't need to call can_process_archetype()
					//       and pChunk.size()==0 here.
					auto cache_view = queryInfo.cache_archetype_view();
					auto cache_data_view = queryInfo.cache_data_view();

					// Determine the range of archetypes we will iterate.
					// Use the entire range by default.
					uint32_t idx_from = 0;
					uint32_t idx_to = (uint32_t)cache_view.size();

					const bool isGroupBy = queryInfo.data().groupBy != EntityBad;
					const bool isGroupSet = queryInfo.data().groupIdSet != 0;
					if (isGroupBy && isGroupSet) {
						// We wish to iterate only a certain group
						auto group_data_view = queryInfo.group_data_view();
						GAIA_EACH(group_data_view) {
							if (group_data_view[i].groupId == queryInfo.data().groupIdSet) {
								idx_from = group_data_view[i].idxFirst;
								idx_to = group_data_view[i].idxLast + 1;
								goto groupSetFound;
							}
						}
						return;
					}

				groupSetFound:
					ArchetypeCacheData dummyCacheData{};

					for (uint32_t i = idx_from; i < idx_to; ++i) {
						auto* pArchetype = cache_view[i];

						if GAIA_UNLIKELY (!can_process_archetype(*pArchetype))
							continue;

						const auto& cacheData = isGroupBy ? cache_data_view[i] : dummyCacheData;
						GAIA_ASSERT(
								// Either no grouping is used...
								!isGroupBy ||
								// ... or no groupId is set...
								queryInfo.data().groupIdSet == 0 ||
								// ... or the groupId must match the requested one
								cache_data_view[i].groupId == queryInfo.data().groupIdSet);

						auto indices_view = queryInfo.indices_mapping_view(i);
						const auto& chunks = pArchetype->chunks();

						uint32_t chunkOffset = 0;
						uint32_t itemsLeft = chunks.size();
						while (itemsLeft > 0) {
							const auto maxBatchSize = chunkBatch.max_size() - chunkBatch.size();
							const auto batchSize = itemsLeft > maxBatchSize ? maxBatchSize : itemsLeft;

							ChunkSpanMut chunkSpan((Chunk**)&chunks[chunkOffset], batchSize);
							for (auto* pChunk: chunkSpan) {
								it.set_chunk(pChunk);
								if GAIA_UNLIKELY (it.size() == 0)
									continue;

								if constexpr (HasFilters) {
									if (!match_filters(*pChunk, queryInfo))
										continue;
								}

								chunkBatch.push_back({pChunk, indices_view.data(), cacheData.groupId});
							}

							if GAIA_UNLIKELY (chunkBatch.size() == chunkBatch.max_size())
								run_query_func(func, it, chunkBatch);

							itemsLeft -= batchSize;
							chunkOffset += batchSize;
						}
					}

					// Take care of any leftovers not processed during run_query
					if (!chunkBatch.empty())
						run_query_func(func, it, chunkBatch);
				}

				template <typename TIter, typename Func>
				void run_query_on_chunks(QueryInfo& queryInfo, Func func) {
					// Update the world version
					update_version(*m_worldVersion);

					const bool hasFilters = queryInfo.has_filters();
					if (hasFilters)
						run_query<true, TIter>(queryInfo, func);
					else
						run_query<false, TIter>(queryInfo, func);

					// Update the query version with the current world's version
					queryInfo.set_world_version(*m_worldVersion);
				}

				template <typename TIter, typename Func, typename... T>
				GAIA_FORCEINLINE void
				run_query_on_chunk(TIter& it, Func func, [[maybe_unused]] core::func_type_list<T...> types) {
					if constexpr (sizeof...(T) > 0) {
						// Pointers to the respective component types in the chunk, e.g
						// 		q.each([&](Position& p, const Velocity& v) {...}
						// Translates to:
						//  	auto p = it.view_mut_inter<Position, true>();
						//		auto v = it.view_inter<Velocity>();
						auto dataPointerTuple = std::make_tuple(it.template view_auto<T>()...);

						// Iterate over each entity in the chunk.
						// Translates to:
						//		GAIA_EACH(it) func(p[i], v[i]);

						GAIA_EACH(it) {
							func(std::get<decltype(it.template view_auto<T>())>(dataPointerTuple)[it.template acc_index<T>(i)]...);
						}
					} else {
						// No functor parameters. Do an empty loop.
						GAIA_EACH(it) func();
					}
				}

				template <bool UseFilters, typename TIter>
				GAIA_NODISCARD bool empty_inter(const QueryInfo& queryInfo) const {
					for (const auto* pArchetype: queryInfo) {
						if GAIA_UNLIKELY (!can_process_archetype(*pArchetype))
							continue;

						GAIA_PROF_SCOPE(query::empty);

						const auto& chunks = pArchetype->chunks();
						TIter it;

						const bool isNotEmpty = core::has_if(chunks, [&](Chunk* pChunk) {
							it.set_chunk(pChunk);
							if constexpr (UseFilters)
								return it.size() > 0 && match_filters(*pChunk, queryInfo);
							else
								return it.size() > 0;
						});

						if (isNotEmpty)
							return false;
					}

					return true;
				}

				template <bool UseFilters, typename TIter>
				GAIA_NODISCARD uint32_t count_inter(const QueryInfo& queryInfo) const {
					uint32_t cnt = 0;
					TIter it;

					for (auto* pArchetype: queryInfo) {
						if GAIA_UNLIKELY (!can_process_archetype(*pArchetype))
							continue;

						GAIA_PROF_SCOPE(query::count);

						// No mapping for count(). It doesn't need to access data cache.
						// auto indices_view = queryInfo.indices_mapping_view(aid);

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

					for (auto* pArchetype: queryInfo) {
						if GAIA_UNLIKELY (!can_process_archetype(*pArchetype))
							continue;

						GAIA_PROF_SCOPE(query::arr);

						// No mapping for arr(). It doesn't need to access data cache.
						// auto indices_view = queryInfo.indices_mapping_view(aid);

						const auto& chunks = pArchetype->chunks();
						for (auto* pChunk: chunks) {
							it.set_chunk(pChunk);
							if (it.size() == 0)
								continue;

							// Filters
							if constexpr (UseFilters) {
								if (!match_filters(*pChunk, queryInfo))
									continue;
							}

							const auto dataView = it.template view<ContainerItemType>();
							GAIA_EACH(it) {
								outArray.push_back(dataView[it.template acc_index<ContainerItemType>(i)]);
							}
						}
					}
				}

				static auto trim(std::span<const char> expr) {
					uint32_t beg = 0;
					while (expr[beg] == ' ')
						++beg;
					uint32_t end = (uint32_t)expr.size() - 1;
					while (end > beg && expr[end] == ' ')
						--end;
					return expr.subspan(beg, end - beg + 1);
				};

				Entity expr_to_entity(va_list& args, std::span<const char> exprRaw) {
					auto expr = trim(exprRaw);

					if (expr[0] == '%') {
						if (expr[1] != 'e') {
							GAIA_ASSERT2(false, "Expression '%' not terminated");
							return EntityBad;
						}

						auto id = (Identifier)va_arg(args, unsigned long long);
						return Entity(id);
					}

					if (expr[0] == '(') {
						if (expr.back() != ')') {
							GAIA_ASSERT2(false, "Expression '(' not terminated");
							return EntityBad;
						}

						auto idStr = expr.subspan(1, expr.size() - 2);
						const auto commaIdx = core::get_index(idStr, ',');

						const auto first = expr_to_entity(args, idStr.subspan(0, commaIdx));
						if (first == EntityBad)
							return EntityBad;
						const auto second = expr_to_entity(args, idStr.subspan(commaIdx + 1));
						if (second == EntityBad)
							return EntityBad;

						return ecs::Pair(first, second);
					}

					{
						auto idStr = trim(expr);

						// Wildcard character
						if (idStr.size() == 1 && idStr[0] == '*')
							return All;

						// Anything else is a component name
						const auto& cc = comp_cache(*m_storage.world());
						const auto* pItem = cc.find(idStr.data(), (uint32_t)idStr.size());
						if (pItem == nullptr) {
							GAIA_ASSERT2(false, "Type not found");
							GAIA_LOG_W("Type '%.*s' not found", (uint32_t)idStr.size(), idStr.data());
							return EntityBad;
						}

						return pItem->entity;
					}
				};

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
				}

				void destroy() {
					m_storage.try_del_from_cache();
				}

				//! Returns true if the query is stored in the query cache
				GAIA_NODISCARD bool is_cached() const {
					return m_storage.is_cached();
				}

				//------------------------------------------------

				//! Creates a query from a null-terminated expression string.
				//!
				//! Expresion is a string between two semicolons.
				//! Spaces are trimmed automatically.
				//!
				//! Supported modifiers:
				//!   ";" - separates expressions
				//!   "?" - query::any
				//!   "!" - query::none
				//!   "&" - read-write access
				//!   "%e" - entity value
				//!   "(rel,tgt)" - relationship pair, a wildcard character in either rel or tgt is translated into All
				//!
				//! Example:
				//!   struct Position {...};
				//!   struct Velocity {...};
				//!   struct RigidBody {...};
				//!   struct Fuel {...};
				//!   auto player = w.add();
				//!   w.query().add("&Position; !Velocity; ?RigidBody; (Fuel,*); %e", player.value());
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

					auto process = [&]() {
						std::span<const char> exprRaw(&str[exp0], pos - exp0);
						exp0 = ++pos;

						auto expr = trim(exprRaw);
						if (expr.empty())
							return true;

						if (expr[0] == '+') {
							uint32_t off = 1;
							if (expr[1] == '&')
								off = 2;

							auto var = expr.subspan(off);
							auto entity = expr_to_entity(args, var);
							if (entity == EntityBad)
								return false;

							any(entity, off != 0);
						} else if (expr[0] == '!') {
							auto var = expr.subspan(1);
							auto entity = expr_to_entity(args, var);
							if (entity == EntityBad)
								return false;

							no(entity);
						} else {
							auto entity = expr_to_entity(args, expr);
							if (entity == EntityBad)
								return false;

							all(entity);
						}

						return true;
					};

					for (; str[pos] != 0; ++pos) {
						if (str[pos] == ';' && !process())
							goto add_end;
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

				QueryImpl& all(Entity entity, bool isReadWrite = false) {
					if (entity.pair())
						add({QueryOpKind::All, QueryAccess::None, entity});
					else
						add({QueryOpKind::All, isReadWrite ? QueryAccess::Write : QueryAccess::Read, entity});
					return *this;
				}

				QueryImpl& all(Entity entity, Entity src, bool isReadWrite = false) {
					if (entity.pair())
						add({QueryOpKind::All, QueryAccess::None, entity, src});
					else
						add({QueryOpKind::All, isReadWrite ? QueryAccess::Write : QueryAccess::Read, entity, src});
					return *this;
				}

				template <typename... T>
				QueryImpl& all() {
					// Add commands to the command buffer
					(add_inter<T>(QueryOpKind::All), ...);
					return *this;
				}

				//------------------------------------------------

				QueryImpl& any(Entity entity, bool isReadWrite = false) {
					if (entity.pair())
						add({QueryOpKind::Any, QueryAccess::None, entity});
					else
						add({QueryOpKind::Any, isReadWrite ? QueryAccess::Write : QueryAccess::Read, entity});
					return *this;
				}

				template <typename... T>
				QueryImpl& any() {
					// Add commands to the command buffer
					(add_inter<T>(QueryOpKind::Any), ...);
					return *this;
				}

				//------------------------------------------------

				QueryImpl& no(Entity entity) {
					add({QueryOpKind::Not, QueryAccess::None, entity});
					return *this;
				}

				template <typename... T>
				QueryImpl& no() {
					// Add commands to the command buffer
					(add_inter<T>(QueryOpKind::Not), ...);
					return *this;
				}

				//------------------------------------------------

				QueryImpl& changed(Entity entity) {
					changed_inter(entity);
					return *this;
				}

				template <typename... T>
				QueryImpl& changed() {
					// Add commands to the command buffer
					(changed_inter<T>(), ...);
					return *this;
				}

				//------------------------------------------------

				QueryImpl& group_by(Entity entity, TGroupByFunc func = group_by_func_default) {
					group_by_inter(entity, func);
					return *this;
				}

				template <typename T>
				QueryImpl& group_by(TGroupByFunc func = group_by_func_default) {
					group_by_inter<T>(func);
					return *this;
				}

				template <typename Rel, typename Tgt>
				QueryImpl& group_by(TGroupByFunc func = group_by_func_default) {
					group_by_inter<Rel, Tgt>(func);
					return *this;
				}

				//------------------------------------------------

				QueryImpl& group_id(GroupId groupId) {
					set_group_id_inter(groupId);
					return *this;
				}

				QueryImpl& group_id(Entity entity) {
					GAIA_ASSERT(!entity.pair());
					set_group_id_inter(entity.id());
					return *this;
				}

				template <typename T>
				QueryImpl& group_id() {
					set_group_id_inter<T>();
					return *this;
				}

				//------------------------------------------------

				template <typename Func>
				void each(QueryInfo& queryInfo, Func func) {
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

					run_query_on_chunks<Iter>(queryInfo, [&](Iter& it) {
						GAIA_PROF_SCOPE(query_func);
						run_query_on_chunk(it, func, InputArgs{});
					});
				}

				template <typename Func>
				void each(Func func) {
					auto& queryInfo = fetch();

					if constexpr (std::is_invocable_v<Func, IterAll&>) {
						run_query_on_chunks<IterAll>(queryInfo, [&](IterAll& it) {
							GAIA_PROF_SCOPE(query_func);
							func(it);
						});
					} else if constexpr (std::is_invocable_v<Func, Iter&>) {
						run_query_on_chunks<Iter>(queryInfo, [&](Iter& it) {
							GAIA_PROF_SCOPE(query_func);
							func(it);
						});
					} else if constexpr (std::is_invocable_v<Func, IterDisabled&>) {
						run_query_on_chunks<IterDisabled>(queryInfo, [&](IterDisabled& it) {
							GAIA_PROF_SCOPE(query_func);
							func(it);
						});
					} else
						each(queryInfo, func);
				}

				template <typename Func, bool FuncEnabled = UseCaching, typename std::enable_if<FuncEnabled>::type* = nullptr>
				void each(QueryId queryId, Func func) {
					// Make sure the query was created by World.query()
					GAIA_ASSERT(m_storage.m_queryCache != nullptr);
					GAIA_ASSERT(queryId != QueryIdBad);

					auto& queryInfo = m_storage.m_queryCache->get(queryId);
					each(queryInfo, func);
				}

				//!	Returns true or false depending on whether there are any entities matching the query.
				//!	\warning Only use if you only care if there are any entities matching the query.
				//!					 The result is not cached and repeated calls to the function might be slow.
				//!					 If you already called arr(), checking if it is empty is preferred.
				//!					 Use empty() instead of calling count()==0.
				//!	\return True if there are any entities matching the query. False otherwise.
				bool empty(Constraints constraints = Constraints::EnabledOnly) {
					auto& queryInfo = fetch();
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
				//! \tparam outArray Container storing entities or components
				//! \param constraints QueryImpl constraints
				//! \return Array with entities or components
				template <typename Container>
				void arr(Container& outArray, Constraints constraints = Constraints::EnabledOnly) {
					const auto entCnt = count();
					if (entCnt == 0)
						return;

					outArray.reserve(entCnt);
					auto& queryInfo = fetch();

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

				//!
				void diag() {
					// Make sure matching happened
					auto& info = fetch();
					GAIA_LOG_N("DIAG Query %u.%u [%c]", id(), gen(), UseCaching ? 'C' : 'U');
					for (const auto* pArchetype: info)
						Archetype::diag_basic_info(*m_storage.world(), *pArchetype);
					GAIA_LOG_N("END DIAG Query");
				}
			};
		} // namespace detail

		using Query = typename detail::QueryImpl<true>;
		using QueryUncached = typename detail::QueryImpl<false>;
	} // namespace ecs
} // namespace gaia
