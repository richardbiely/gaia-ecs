#pragma once
#include "gaia/config/config.h"

#include "gaia/cnt/darray.h"
#include "gaia/cnt/map.h"
#include "gaia/core/utility.h"
#include "gaia/ecs/component.h"
#include "gaia/ecs/id.h"
#include "gaia/ecs/query_common.h"
#include "gaia/ecs/query_info.h"

namespace gaia {
	namespace ecs {
		class QueryLookupKey {
			QueryLookupHash m_hash;
			const QueryCtx* m_pCtx;

		public:
			static constexpr bool IsDirectHashKey = true;

			QueryLookupKey(): m_hash({0}), m_pCtx(nullptr) {}
			explicit QueryLookupKey(QueryLookupHash hash, const QueryCtx* pCtx): m_hash(hash), m_pCtx(pCtx) {}

			size_t hash() const {
				return (size_t)m_hash.hash;
			}

			bool operator==(const QueryLookupKey& other) const {
				// Hash doesn't match we don't have a match.
				// Hash collisions are expected to be very unlikely so optimize for this case.
				if GAIA_LIKELY (m_hash != other.m_hash)
					return false;

				const auto id = m_pCtx->q.handle.id();

				// Temporary key is given. Do full context comparison.
				if (id == QueryIdBad)
					return *m_pCtx == *other.m_pCtx;

				// Real key is given. Compare context pointer.
				// Normally we'd compare query IDs but because we do not allow query copies and all queries are
				// unique it's guaranteed that if pointers are the same we have a match.
				return m_pCtx == other.m_pCtx;
			}
		};

		class QueryCache {
		public:
			enum class ChangeKind : uint8_t {
				// Query membership may have changed due to structural world changes.
				Structural,
				// Only dynamic/source-driven results may have changed.
				DynamicResult,
				// Full query cache invalidation.
				All,
			};

		private:
			struct TrackedArchetypes {
				cnt::darray<const Archetype*> archetypes;
				uint32_t syncedRevision = 0;
			};

			cnt::map<QueryLookupKey, uint32_t> m_queryCache;
			// TODO: Make m_queryArr allocate data in pages.
			//       Currently ilist always uses a darray internally which keeps growing, making
			//       it not suitable for this particular use case.
			//       QueryInfo is quite big and we do not want to copy a lot of data every time
			//       resizing is necessary.
			cnt::ilist<QueryInfo, QueryHandle> m_queryArr;

			//! Entity -> query mapping
			cnt::map<EntityLookupKey, cnt::darray<QueryHandle>> m_entityToQuery;
			//! Relation entity -> queries with relation/traversal dependencies on it.
			cnt::map<EntityLookupKey, cnt::darray<QueryHandle>> m_relationToQuery;
			//! Sort key entity -> cached sorted queries that need their sorted slices refreshed after writes.
			cnt::map<EntityLookupKey, cnt::darray<QueryHandle>> m_sortEntityToQuery;
			//! Cached sorted queries that need their sorted slices refreshed after row-order changes.
			cnt::darray<QueryHandle> m_sortedQueries;
			//! Positive structural term -> cached queries that may match a newly created archetype containing that id.
			cnt::map<EntityLookupKey, cnt::darray<QueryHandle>> m_entityToCreateQuery;
			//! Archetype -> cached queries whose current result cache contains it
			cnt::map<ArchetypeIdLookupKey, cnt::darray<QueryHandle>> m_archetypeToQuery;
			//! Cached query -> tracked result archetypes currently registered in m_archetypeToQuery
			cnt::map<QueryHandleLookupKey, TrackedArchetypes> m_queryToArchetype;
			//! Scratch candidate list reused while routing a newly created archetype to cached queries.
			cnt::darray<QueryHandle> m_createQueryHandleScratch;
			//! Handle-id stamp table used to deduplicate create candidates in O(1) per hit.
			cnt::darray<uint32_t> m_createQueryHandleStampById;
			uint32_t m_createQueryHandleStamp = 1;

		public:
			QueryCache() {
				m_queryArr.reserve(256);
			}

			~QueryCache() = default;

			QueryCache(QueryCache&&) = delete;
			QueryCache(const QueryCache&) = delete;
			QueryCache& operator=(QueryCache&&) = delete;
			QueryCache& operator=(const QueryCache&) = delete;

			GAIA_NODISCARD bool valid(QueryHandle handle) const {
				if (handle.id() == QueryIdBad)
					return false;

				// Entity ID has to fit inside the entity array
				if (handle.id() >= m_queryArr.size())
					return false;

				const auto& h = m_queryArr[handle.id()];
				return h.idx == handle.id() && h.data.gen == handle.gen();
			}

			void clear() {
				m_queryCache.clear();
				m_queryArr.clear();
				m_entityToQuery.clear();
				m_relationToQuery.clear();
				m_sortEntityToQuery.clear();
				m_sortedQueries.clear();
				m_entityToCreateQuery.clear();
				m_archetypeToQuery.clear();
				m_queryToArchetype.clear();
				m_createQueryHandleScratch.clear();
				m_createQueryHandleStampById.clear();
				m_createQueryHandleStamp = 1;
			}

			//! Clears only the reverse indices that keep raw archetype pointers alive.
			//! Used during world shutdown before archetypes begin running chunk/component dtors.
			void clear_archetype_tracking() {
				m_archetypeToQuery.clear();
				m_queryToArchetype.clear();
			}

			//! Returns a QueryInfo object associated with @a handle.
			//! \param handle Query handle
			//! \return Query info
			QueryInfo* try_get(QueryHandle handle) {
				if (!valid(handle))
					return nullptr;

				auto& info = m_queryArr[handle.id()];
				GAIA_ASSERT(info.idx == handle.id());
				GAIA_ASSERT(info.data.gen == handle.gen());
				return &info;
			}

			//! Returns a QueryInfo object associated with @a handle.
			//! \param handle Query handle
			//! \return Query info
			QueryInfo& get(QueryHandle handle) {
				GAIA_ASSERT(valid(handle));

				auto& info = m_queryArr[handle.id()];
				GAIA_ASSERT(info.idx == handle.id());
				GAIA_ASSERT(info.data.gen == handle.gen());
				return info;
			}

			//! Registers the provided query lookup context @a ctx. If it already exists it is returned.
			//! \param ctx Query context
			//! \param entityToArchetypeMap Map of all archetypes
			//! \param allArchetypes Array of all archetypes
			//! \return Reference to the newly created or an already existing QueryInfo object.
			QueryInfo&
			add(QueryCtx&& ctx, //
					const EntityToArchetypeMap& entityToArchetypeMap, //
					std::span<const Archetype*> allArchetypes) {
				GAIA_ASSERT(ctx.hashLookup.hash != 0);

				// First check if the query cache record exists
				auto ret = m_queryCache.try_emplace(QueryLookupKey(ctx.hashLookup, &ctx));
				if (!ret.second) {
					const auto idx = ret.first->second;
					auto& info = m_queryArr[idx];
					GAIA_ASSERT(idx == info.idx);
					info.add_ref();
					return info;
				}

				// No record exists, let us create a new one
				QueryInfoCreationCtx creationCtx{};
				creationCtx.pQueryCtx = &ctx;
				creationCtx.pEntityToArchetypeMap = &entityToArchetypeMap;
				creationCtx.allArchetypes = allArchetypes;
				auto handle = m_queryArr.alloc(&creationCtx);

				// We are moving the rvalue to "ctx". As a result, the pointer stored in m_queryCache.emplace above is no longer
				// going to be valid. Therefore we swap the map key with a one with a valid pointer.
				auto& info = get(handle);
				info.add_ref();
				auto new_p = robin_hood::pair(std::make_pair(QueryLookupKey(ctx.hashLookup, &info.ctx()), info.idx));
				ret.first->swap(new_p);

				// Add the entity->query pair
				add_entity_to_query_pairs(info.ctx().data.ids_view(), handle);
				add_rel_to_query_pairs(info.ctx(), handle);
				add_sort_to_query_pairs(info.ctx(), handle);
				add_sorted_query(info.ctx(), handle);
				add_create_to_query_pairs(info.ctx(), handle);

				return info;
			}

			//! Deletes an existing QueryInfo object given the provided query handle.
			//! \param handle Query handle
			//! \return True if handle was found. False otherwise.
			bool del(QueryHandle handle) {
				auto* pInfo = try_get(handle);
				if (pInfo == nullptr)
					return false;

				pInfo->dec_ref();
				if (pInfo->refs() != 0)
					return false;

				unregister_query_archetypes(handle);

				// If this was the last reference to the query, we can safely remove it
				auto it = m_queryCache.find(QueryLookupKey(pInfo->ctx().hashLookup, &pInfo->ctx()));
				GAIA_ASSERT(it != m_queryCache.end());
				m_queryCache.erase(it);

				// Remove the entity->query pair
				del_entity_to_query_pairs(pInfo->ctx().data.ids_view(), handle);
				del_rel_to_query_pairs(pInfo->ctx(), handle);
				del_sort_to_query_pairs(pInfo->ctx(), handle);
				del_sorted_query(pInfo->ctx(), handle);
				del_create_to_query_pairs(pInfo->ctx(), handle);
				m_queryArr.free(handle);

				return true;
			}

			cnt::darray<QueryInfo>::iterator begin() {
				return m_queryArr.begin();
			}

			cnt::darray<QueryInfo>::iterator end() {
				return m_queryArr.end();
			}

			//! Invalidates all cached queries that work with the given entity
			//! This covers the following kinds of query terms:
			//! 1) X
			//! 2) (*, X)
			//! 3) (X, *)
			//! \param entityKey Entity lookup key
			void invalidate_queries_for_entity(EntityLookupKey entityKey, ChangeKind changeKind) {
				auto it = m_entityToQuery.find(entityKey);
				if (it == m_entityToQuery.end())
					return;

				const auto& handles = it->second;
				for (const auto& handle: handles) {
					auto& info = get(handle);
					// World mutations invalidate cached results, but they do not change query shape.
					// Recomputing QueryCtx metadata here only adds overhead to the invalidation path.
					info.invalidate(select_invalidation_kind(info, changeKind));
				}
			}

			void invalidate_queries_for_rel(Entity relation, ChangeKind changeKind) {
				auto it = m_relationToQuery.find(EntityLookupKey(relation));
				if (it == m_relationToQuery.end())
					return;

				for (const auto handle: it->second) {
					auto& info = get(handle);
					// Relation changes affect dynamic freshness, not the query definition itself.
					info.invalidate(select_invalidation_kind(info, changeKind));
				}
			}

			void invalidate_sorted_queries_for_entity(Entity entity) {
				auto it = m_sortEntityToQuery.find(EntityLookupKey(entity));
				if (it == m_sortEntityToQuery.end())
					return;

				for (const auto handle: it->second) {
					auto* pInfo = try_get(handle);
					if (pInfo == nullptr || pInfo->refs() == 0)
						continue;

					pInfo->invalidate_sort();
				}
			}

			//! Invalidates all cached sorted queries after chunk row order changes.
			void invalidate_sorted_queries() {
				for (const auto handle: m_sortedQueries) {
					auto* pInfo = try_get(handle);
					if (pInfo == nullptr || pInfo->refs() == 0)
						continue;

					pInfo->invalidate_sort();
				}
			}

			void sync_archetype_cache(QueryInfo& queryInfo) {
				const auto handle = QueryInfo::handle(queryInfo);
				if (!valid(handle))
					return;

				const auto archetypes = queryInfo.cache_archetype_view();
				const auto key = QueryHandleLookupKey(handle);
				auto it = m_queryToArchetype.find(key);
				if (it != m_queryToArchetype.end() && it->second.syncedRevision == queryInfo.reverse_index_revision())
					return;

				unregister_query_archetypes(handle);

				if (archetypes.empty())
					return;

				auto [trackedIt, inserted] = m_queryToArchetype.try_emplace(key);
				auto& tracked = trackedIt->second.archetypes;
				if (!inserted)
					tracked.clear();

				tracked.reserve((uint32_t)archetypes.size());
				for (const auto* pArchetype: archetypes) {
					tracked.push_back(pArchetype);
					add_archetype_query_pair(pArchetype, handle);
				}
				trackedIt->second.syncedRevision = queryInfo.reverse_index_revision();
			}

			void remove_archetype_from_queries(Archetype* pArchetype) {
				const auto archetypeKey = ArchetypeIdLookupKey(pArchetype->id(), pArchetype->id_hash());
				auto it = m_archetypeToQuery.find(archetypeKey);
				if (it == m_archetypeToQuery.end())
					return;

				const auto handles = it->second;
				for (const auto handle: handles) {
					auto* pInfo = try_get(handle);
					if (pInfo != nullptr && pInfo->refs() != 0)
						pInfo->remove(pArchetype);

					auto trackedIt = m_queryToArchetype.find(QueryHandleLookupKey(handle));
					if (trackedIt == m_queryToArchetype.end())
						continue;

					auto& tracked = trackedIt->second.archetypes;
					core::swap_erase(tracked, core::get_index(tracked, pArchetype));
					if (trackedIt->second.archetypes.empty())
						m_queryToArchetype.erase(trackedIt);
				}

				m_archetypeToQuery.erase(it);
			}

			void register_archetype_with_queries(const Archetype* pArchetype) {
				auto& handles = prepare_create_query_handles();
				bool hasAnyPair = false;
				for (const auto entity: pArchetype->ids_view()) {
					add_create_query_handles(EntityLookupKey(entity), handles);
					if (!entity.pair())
						continue;

					hasAnyPair = true;

					// Pair ids retain the relation/target ids plus their kind bits. That is enough to
					// rebuild wildcard pair lookup keys without touching the world record storage.
					const auto relKind = entity.entity() ? EntityKind::EK_Uni : EntityKind::EK_Gen;
					const auto rel = Entity((EntityId)entity.id(), 0, false, false, relKind);
					const auto tgt = Entity((EntityId)entity.gen(), 0, false, false, entity.kind());
					add_create_query_handles(EntityLookupKey(Pair(All, tgt)), handles);
					add_create_query_handles(EntityLookupKey(Pair(rel, All)), handles);
				}

				if (hasAnyPair)
					add_create_query_handles(EntityLookupKey(Pair(All, All)), handles);

				for (const auto handle: handles) {
					auto* pInfo = try_get(handle);
					if (pInfo == nullptr || pInfo->refs() == 0)
						continue;

					if (!pInfo->register_archetype(*pArchetype))
						continue;

					register_query_archetype(handle, pArchetype, pInfo->reverse_index_revision());
				}
			}

		private:
			static QueryInfo::InvalidationKind select_invalidation_kind(const QueryInfo& info, ChangeKind changeKind) {
				switch (changeKind) {
					case ChangeKind::DynamicResult:
						return QueryInfo::InvalidationKind::Result;
					case ChangeKind::All:
						return QueryInfo::InvalidationKind::All;
					case ChangeKind::Structural:
						// Structural changes invalidate seed caches for structural queries.
						// Dynamic queries reuse structural compilation state and only need their
						// final result refreshed on the next read.
						return info.ctx().data.deps.has(QueryCtx::DependencyHasSourceTerms) ||
													 info.ctx().data.deps.has(QueryCtx::DependencyHasVariableTerms)
											 ? QueryInfo::InvalidationKind::Result
											 : QueryInfo::InvalidationKind::Seed;
				}

				GAIA_ASSERT(false);
				return QueryInfo::InvalidationKind::All;
			}

			//! Adds an entity to the <entity, query> map
			//! \param entity Entity getting added
			//! \param handle Query handle
			void add_entity_query_pair(Entity entity, QueryHandle handle) {
				EntityLookupKey entityKey(entity);
				const auto it = m_entityToQuery.find(entityKey);
				if (it == m_entityToQuery.end()) {
					m_entityToQuery.try_emplace(entityKey, cnt::darray<QueryHandle>{handle});
					return;
				}

				auto& handles = it->second;
				if (!core::has(handles, handle))
					handles.push_back(handle);
			}

			//! Deletes an entity from the <entity, query> map
			//! \param entity Entity getting removed
			//! \param handle Query handle
			void del_entity_query_pair(Entity entity, QueryHandle handle) {
				auto it = m_entityToQuery.find(EntityLookupKey(entity));
				if (it == m_entityToQuery.end())
					return;

				auto& handles = it->second;
				const auto idx = core::get_index_unsafe(handles, handle);
				core::swap_erase_unsafe(handles, idx);

				// Remove the mapping if there are no more matches
				if (handles.empty())
					m_entityToQuery.erase(it);
			}

			//! Adds an entity to the <entity, query> map
			//! \param entities Entities getting added
			void add_entity_to_query_pairs(EntitySpan entities, QueryHandle handle) {
				for (auto entity: entities) {
					add_entity_query_pair(entity, handle);
				}
			}

			//! Deletes an entity from the <entity, query> map
			//! \param entities Entities getting deleted
			void del_entity_to_query_pairs(EntitySpan entities, QueryHandle handle) {
				for (auto entity: entities) {
					del_entity_query_pair(entity, handle);
				}
			}

			void add_create_to_query_pair(Entity entity, QueryHandle handle) {
				EntityLookupKey entityKey(entity);
				const auto it = m_entityToCreateQuery.find(entityKey);
				if (it == m_entityToCreateQuery.end()) {
					m_entityToCreateQuery.try_emplace(entityKey, cnt::darray<QueryHandle>{handle});
					return;
				}

				auto& handles = it->second;
				if (!core::has(handles, handle))
					handles.push_back(handle);
			}

			void add_sort_to_query_pair(Entity entity, QueryHandle handle) {
				auto it = m_sortEntityToQuery.find(EntityLookupKey(entity));
				if (it == m_sortEntityToQuery.end()) {
					m_sortEntityToQuery.try_emplace(EntityLookupKey(entity), cnt::darray<QueryHandle>{handle});
					return;
				}

				auto& handles = it->second;
				if (!core::has(handles, handle))
					handles.push_back(handle);
			}

			void del_sort_to_query_pair(Entity entity, QueryHandle handle) {
				auto it = m_sortEntityToQuery.find(EntityLookupKey(entity));
				if (it == m_sortEntityToQuery.end())
					return;

				auto& handles = it->second;
				core::swap_erase(handles, core::get_index(handles, handle));
				if (handles.empty())
					m_sortEntityToQuery.erase(it);
			}

			void add_sort_to_query_pairs(const QueryCtx& ctx, QueryHandle handle) {
				if (ctx.data.sortByFunc == nullptr || ctx.data.sortBy == EntityBad)
					return;

				add_sort_to_query_pair(ctx.data.sortBy, handle);
			}

			void del_sort_to_query_pairs(const QueryCtx& ctx, QueryHandle handle) {
				if (ctx.data.sortByFunc == nullptr || ctx.data.sortBy == EntityBad)
					return;

				del_sort_to_query_pair(ctx.data.sortBy, handle);
			}

			void add_sorted_query(const QueryCtx& ctx, QueryHandle handle) {
				if (ctx.data.sortByFunc == nullptr)
					return;

				m_sortedQueries.push_back(handle);
			}

			void del_sorted_query(const QueryCtx& ctx, QueryHandle handle) {
				if (ctx.data.sortByFunc == nullptr)
					return;

				const auto idx = core::get_index(m_sortedQueries, handle);
				GAIA_ASSERT(idx != BadIndex);
				if (idx != BadIndex)
					core::swap_erase(m_sortedQueries, idx);
			}

			void del_create_to_query_pair(Entity entity, QueryHandle handle) {
				auto it = m_entityToCreateQuery.find(EntityLookupKey(entity));
				if (it == m_entityToCreateQuery.end())
					return;

				auto& handles = it->second;
				core::swap_erase(handles, core::get_index(handles, handle));
				if (handles.empty())
					m_entityToCreateQuery.erase(it);
			}

			void add_create_to_query_pairs(const QueryCtx& ctx, QueryHandle handle) {
				if (ctx.data.cachePolicy != QueryCtx::CachePolicy::Immediate)
					return;

				// Only structural queries with positive selector dependencies are tracked here.
				// Dependency metadata is refreshed together with cache policy classification so
				// create-time propagation can consume it without re-deriving query shape here.
				for (const auto entity: ctx.data.deps.create_selectors_view())
					add_create_to_query_pair(entity, handle);
			}

			void del_create_to_query_pairs(const QueryCtx& ctx, QueryHandle handle) {
				if (ctx.data.cachePolicy != QueryCtx::CachePolicy::Immediate)
					return;

				for (const auto entity: ctx.data.deps.create_selectors_view())
					del_create_to_query_pair(entity, handle);
			}

			void add_create_query_handles(EntityLookupKey entityKey, cnt::darray<QueryHandle>& handles) {
				const auto it = m_entityToCreateQuery.find(entityKey);
				if (it == m_entityToCreateQuery.end())
					return;

				for (const auto handle: it->second) {
					if (mark_create_query_handle(handle))
						handles.push_back(handle);
				}
			}

			cnt::darray<QueryHandle>& prepare_create_query_handles() {
				m_createQueryHandleScratch.clear();

				// Archetype creation can fan out through many positive selector ids. Use a monotonic stamp table
				// keyed by query-handle id so duplicate candidates do not devolve into repeated linear scans.
				++m_createQueryHandleStamp;
				if (m_createQueryHandleStamp == 0) {
					m_createQueryHandleStampById = {};
					m_createQueryHandleStamp = 1;
				}

				return m_createQueryHandleScratch;
			}

			GAIA_NODISCARD bool mark_create_query_handle(QueryHandle handle) {
				const auto handleId = (uint32_t)handle.id();
				if (handleId >= m_createQueryHandleStampById.size())
					m_createQueryHandleStampById.resize(handleId + 1);

				auto& stamp = m_createQueryHandleStampById[handleId];
				if (stamp == m_createQueryHandleStamp)
					return false;

				stamp = m_createQueryHandleStamp;
				return true;
			}

			void add_archetype_query_pair(const Archetype* pArchetype, QueryHandle handle) {
				const auto archetypeKey = ArchetypeIdLookupKey(pArchetype->id(), pArchetype->id_hash());
				const auto it = m_archetypeToQuery.find(archetypeKey);
				if (it == m_archetypeToQuery.end()) {
					m_archetypeToQuery.try_emplace(archetypeKey, cnt::darray<QueryHandle>{handle});
					return;
				}

				auto& handles = it->second;
				// Callers only register a <query, archetype> edge after they proved that edge is new.
				GAIA_ASSERT(!core::has(handles, handle));
				handles.push_back(handle);
			}

			void del_archetype_query_pair(const Archetype* pArchetype, QueryHandle handle) {
				auto it = m_archetypeToQuery.find(ArchetypeIdLookupKey(pArchetype->id(), pArchetype->id_hash()));
				if (it == m_archetypeToQuery.end())
					return;

				auto& handles = it->second;
				const auto idx = core::get_index(handles, handle);
				GAIA_ASSERT(idx != BadIndex);
				core::swap_erase(handles, idx);
				if (handles.empty())
					m_archetypeToQuery.erase(it);
			}

			void unregister_query_archetypes(QueryHandle handle) {
				auto it = m_queryToArchetype.find(QueryHandleLookupKey(handle));
				if (it == m_queryToArchetype.end())
					return;

				const auto& tracked = it->second.archetypes;
				for (const auto* pArchetype: tracked)
					del_archetype_query_pair(pArchetype, handle);

				m_queryToArchetype.erase(it);
			}

			void register_query_archetype(QueryHandle handle, const Archetype* pArchetype, uint32_t syncedRevision) {
				auto [trackedIt, inserted] = m_queryToArchetype.try_emplace(QueryHandleLookupKey(handle));
				auto& tracked = trackedIt->second.archetypes;

				// Newly-created archetypes and sync_archetype_cache() both route through a deduplicated edge set,
				// so reverse-index registration can append directly instead of re-scanning tracked archetypes.
				GAIA_ASSERT(inserted || !core::has(tracked, pArchetype));
				tracked.push_back(pArchetype);
				trackedIt->second.syncedRevision = syncedRevision;
				add_archetype_query_pair(pArchetype, handle);
			}

			void add_rel_query_pair(Entity relation, QueryHandle handle) {
				const auto key = EntityLookupKey(relation);
				const auto it = m_relationToQuery.find(key);
				if (it == m_relationToQuery.end()) {
					m_relationToQuery.try_emplace(key, cnt::darray<QueryHandle>{handle});
					return;
				}

				auto& handles = it->second;
				if (!core::has(handles, handle))
					handles.push_back(handle);
			}

			void del_rel_query_pair(Entity relation, QueryHandle handle) {
				auto it = m_relationToQuery.find(EntityLookupKey(relation));
				if (it == m_relationToQuery.end())
					return;

				auto& handles = it->second;
				core::swap_erase(handles, core::get_index(handles, handle));
				if (handles.empty())
					m_relationToQuery.erase(it);
			}

			void add_rel_to_query_pairs(const QueryCtx& ctx, QueryHandle handle) {
				for (const auto relation: ctx.data.deps.relations_view())
					add_rel_query_pair(relation, handle);
			}

			void del_rel_to_query_pairs(const QueryCtx& ctx, QueryHandle handle) {
				for (const auto relation: ctx.data.deps.relations_view())
					del_rel_query_pair(relation, handle);
			}
		};
	} // namespace ecs
} // namespace gaia
