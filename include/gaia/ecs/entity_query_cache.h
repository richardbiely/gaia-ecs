#pragma once
#include "entity_query.h"
#include "gaia/config/config_core_end.h"

namespace gaia {
	namespace ecs {
		class EntityQueryCache {
			containers::map<EntityQueryInfo::LookupHash, containers::darray<EntityQueryInfo>> m_cachedQueries;

		public:
			EntityQueryCache() = default;

			EntityQueryCache(EntityQueryCache&&) = delete;
			EntityQueryCache(const EntityQueryCache&) = delete;
			EntityQueryCache& operator=(EntityQueryCache&&) = delete;
			EntityQueryCache& operator=(const EntityQueryCache&) = delete;

			//! Searches for an entity query info from the provided \param query.
			//! \param query Query used to search for query info
			//! \return Entity query info or nullptr if not found
			EntityQueryInfo* Find(const EntityQuery& query) const {
				const auto hash = query.GetLookupHash();
				const auto id = query.GetCacheId();

				auto it = m_cachedQueries.find(hash);
				GAIA_ASSERT(it != m_cachedQueries.end());

				const auto& queries = it->second;
				for (auto& q: queries) {
					if (q.GetCacheId() != id)
						continue;
					return &q;
				}

				return nullptr;
			};

			//! Returns an already existing entity query info from the provided \param query.
			//! \warning It is expected that the query has already been registered. Undefined behavior otherwise.
			//! \param query Query used to search for query info
			//! \return Entity query info
			EntityQueryInfo& Get(const EntityQuery& query) const {
				auto* pInfo = Find(query);
				GAIA_ASSERT(pInfo != nullptr);
				return *pInfo;
			};

			//! Registers the provided entity query lookup context \param ctx. If it already exists it is returned.
			//! \return Entity query info
			EntityQueryInfo& GetOrCreate(EntityQuery& query, EntityQueryInfo::LookupCtx&& ctx) {
				GAIA_ASSERT(ctx.hashLookup.hash != 0);

				// Check if the query info exists first
				auto it = m_cachedQueries.find(ctx.hashLookup);
				if GAIA_UNLIKELY (it == m_cachedQueries.end()) {
					// Query info does not exist so we need to create it and update the orignal query accordingly.
					const auto hash = ctx.hashLookup;

					auto info = EntityQueryInfo::Create(std::move(ctx));
					info.Init(0);
					query.Init(hash, 0);

					m_cachedQueries[hash] = {std::move(info)};
					return m_cachedQueries[hash].back();
				}

				auto& queries = it->second;

				// Record with the query info lookup hash exists but we need to check if the query itself is a part of it.
				if GAIA_LIKELY (query.GetCacheId() != (int32_t)-1) {
					// Make sure the same hash gets us to the proper query
					for (auto& q: queries) {
						if (q != ctx)
							continue;
						return q;
					}

					GAIA_ASSERT(false && "EntityQueryInfo not found despite having its lookupHash and cacheId set!");
				}

				// This query has not been added anywhere yet. Let's change that.
				auto info = EntityQueryInfo::Create(std::move(ctx));
				info.Init((uint32_t)queries.size());
				query.Init(ctx.hashLookup, (uint32_t)queries.size());

				queries.push_back(std::move(info));
				return queries.back();
			};
		};
	} // namespace ecs
} // namespace gaia
