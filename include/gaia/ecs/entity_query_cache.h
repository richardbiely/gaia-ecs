#pragma once
#include "../config/config_core_end.h"
#include "../containers/darray.h"
#include "entity_query_common.h"
#include "entity_query_info.h"

namespace gaia {
	namespace ecs {
		class EntityQueryCache {
			using QueryCacheLookupArray = containers::darr<uint32_t>;
			containers::map<query::LookupHash, QueryCacheLookupArray> m_queryCache;
			containers::darray<EntityQueryInfo> m_queryArr;

		public:
			EntityQueryCache() {
				m_queryArr.reserve(256);
			}

			~EntityQueryCache() = default;

			EntityQueryCache(EntityQueryCache&&) = delete;
			EntityQueryCache(const EntityQueryCache&) = delete;
			EntityQueryCache& operator=(EntityQueryCache&&) = delete;
			EntityQueryCache& operator=(const EntityQueryCache&) = delete;

			//! Returns an already existing entity query info from the provided \param query.
			//! \warning It is expected that the query has already been registered. Undefined behavior otherwise.
			//! \param query Query used to search for query info
			//! \return Entity query info
			EntityQueryInfo& Get(uint32_t queryId) {
				GAIA_ASSERT(queryId != (uint32_t)-1);

				return m_queryArr[queryId];
			};

			//! Registers the provided entity query lookup context \param ctx. If it already exists it is returned.
			//! \return Query id
			uint32_t GetOrCreate(query::LookupCtx&& ctx) {
				GAIA_ASSERT(ctx.hashLookup.hash != 0);

				// Check if the query info exists first
				auto ret = m_queryCache.try_emplace(ctx.hashLookup, QueryCacheLookupArray{});
				if (!ret.second) {
					const auto& queryIds = ret.first->second;

					// Record with the query info lookup hash exists but we need to check if the query itself is a part of it.
					if GAIA_LIKELY (ctx.queryId != (int32_t)-1) {
						// Make sure the same hash gets us to the proper query
						for (const auto queryId: queryIds) {
							const auto& queryInfo = m_queryArr[queryId];
							if (queryInfo != ctx)
								continue;

							return queryId;
						}

						GAIA_ASSERT(false && "EntityQueryInfo not found despite having its lookupHash and cacheId set!");
						return (uint32_t)-1;
					}
				}

				const auto queryId = (uint32_t)m_queryArr.size();
				m_queryArr.push_back(EntityQueryInfo::Create(queryId, std::move(ctx)));
				ret.first->second.push_back(queryId);
				return queryId;
			};
		};
	} // namespace ecs
} // namespace gaia
