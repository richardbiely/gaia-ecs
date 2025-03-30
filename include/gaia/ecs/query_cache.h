#pragma once
#include "../config/config.h"

#include "../cnt/darray.h"
#include "../cnt/map.h"
#include "../core/utility.h"
#include "component.h"
#include "id.h"
#include "query_common.h"
#include "query_info.h"

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
			cnt::map<QueryLookupKey, uint32_t> m_queryCache;
			// TODO: Make m_queryArr allocate data in pages.
			//       Currently ilist always uses a darray internally which keeps growing following
			//       logic not suitable for this particular use case.
			//       QueryInfo is quite big and we do not want to copying a lot of data every time
			//       resizing is necessary.
			cnt::ilist<QueryInfo, QueryHandle> m_queryArr;

			//! entity -> query mapping
			cnt::map<EntityLookupKey, cnt::darray<QueryHandle>> m_entityToQuery;

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
				return h.idx == handle.id() && h.gen == handle.gen();
			}

			void clear() {
				m_queryCache.clear();
				m_queryArr.clear();
				m_entityToQuery.clear();
			}

			//! Returns a QueryInfo object stored at the index \param idx.
			//! \param idx Index of the QueryInfo we try to retrieve
			//! \return Query info
			QueryInfo* try_get(QueryHandle handle) {
				if (!valid(handle))
					return nullptr;

				auto& info = m_queryArr[handle.id()];
				GAIA_ASSERT(info.idx == handle.id());
				GAIA_ASSERT(info.gen == handle.gen());
				return &info;
			};

			//! Returns a QueryInfo object stored at the index \param idx.
			//! \param idx Index of the QueryInfo we try to retrieve
			//! \return Query info
			QueryInfo& get(QueryHandle handle) {
				GAIA_ASSERT(valid(handle));

				auto& info = m_queryArr[handle.id()];
				GAIA_ASSERT(info.idx == handle.id());
				GAIA_ASSERT(info.gen == handle.gen());
				return info;
			};

			//! Registers the provided query lookup context \param ctx. If it already exists it is returned.
			//! \return Reference a newly created or an already existing QueryInfo object.
			QueryInfo&
			add(QueryCtx&& ctx, //
					const EntityToArchetypeMap& entityToArchetypeMap, //
					const ArchetypeDArray& allArchetypes) {
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
				creationCtx.pAllArchetypes = &allArchetypes;
				auto handle = m_queryArr.alloc(&creationCtx);

				// We are moving the rvalue to "ctx". As a result, the pointer stored in m_queryCache.emplace above is no longer
				// going to be valid. Therefore we swap the map key with a one with a valid pointer.
				auto& info = get(handle);
				info.add_ref();
				auto new_p = robin_hood::pair(std::make_pair(QueryLookupKey(ctx.hashLookup, &info.ctx()), info.idx));
				ret.first->swap(new_p);

				// Add the entity->query pair
				add_entity_to_query_pairs({info.ids().data(), info.ids().size()}, handle);

				return info;
			}

			//! Deletes an existing QueryInfo object given the provided query lookup context \param ctx.
			bool del(QueryHandle handle) {
				auto* pInfo = try_get(handle);
				if (pInfo == nullptr)
					return false;

				pInfo->dec_ref();
				if (pInfo->refs() != 0)
					return false;

				// If this was the last reference to the query, we can safely remove it
				auto it = m_queryCache.find(QueryLookupKey(pInfo->ctx().hashLookup, &pInfo->ctx()));
				GAIA_ASSERT(it != m_queryCache.end());
				m_queryCache.erase(it);
				m_queryArr.free(handle);

				// Remove the entity->query pair
				del_entity_to_query_pairs({pInfo->ids().data(), pInfo->ids().size()}, handle);

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
			void invalidate_queries_for_entity(EntityLookupKey entityKey) {
				auto it = m_entityToQuery.find(entityKey);
				if (it == m_entityToQuery.end())
					return;

				const auto& handles = it->second;
				for (auto& handle: handles) {
					auto& info = get(handle);
					info.reset();
					info.refresh_ctx();
				}
			}

		private:
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
			void del_entity_archetype_pair(Entity entity, QueryHandle handle) {
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
					add_entity_query_pair(entity, handle);
				}
			}
		};
	} // namespace ecs
} // namespace gaia
