#pragma once
#include "../config/config.h"

#include "../cnt/darray.h"
#include "../cnt/sarray_ext.h"
#include "../cnt/set.h"
#include "../config/profiler.h"
#include "../core/hashing_policy.h"
#include "../core/utility.h"
#include "archetype.h"
#include "archetype_common.h"
#include "component.h"
#include "component_cache.h"
#include "component_utils.h"
#include "gaia/ecs/id.h"
#include "query_common.h"

namespace gaia {
	namespace ecs {
		struct Entity;

		using ComponentIdToArchetypeMap = cnt::map<EntityLookupKey, ArchetypeList>;

		class QueryInfo {
		public:
			//! Query matching result
			enum class MatchArchetypeQueryRet : uint8_t { Fail, Ok, Skip };

		private:
			//! Lookup context
			QueryCtx m_lookupCtx;
			//! List of archetypes matching the query
			ArchetypeList m_archetypeCache;
			//! Id of the last archetype in the world we checked
			ArchetypeId m_lastArchetypeId{};
			//! Version of the world for which the query has been called most recently
			uint32_t m_worldVersion{};

			template <typename T>
			bool has_inter([[maybe_unused]] QueryOp op, bool isReadWrite) const {
				if constexpr (std::is_same_v<T, Entity>) {
					// Entities are read-only.
					GAIA_ASSERT(!isReadWrite);
					// Skip Entity input args. Entities are always there.
					return true;
				} else {
					const auto& data = m_lookupCtx.data;
					const auto& ids = data.ids;

					const auto comp = m_lookupCtx.cc->get<T>().entity;
					const auto compIdx = ecs::comp_idx<MAX_ITEMS_IN_QUERY>(ids.data(), comp);

					if (op != data.ops[compIdx])
						return false;

					// Read-write mask must match
					const uint32_t maskRW = (uint32_t)data.readWriteMask & (1U << compIdx);
					const uint32_t maskXX = (uint32_t)isReadWrite << compIdx;
					return maskRW == maskXX;
				}
			}

			template <typename T>
			bool has_inter(QueryOp op) const {
				// static_assert(is_raw_v<<T>, "has() must be used with raw types");

				using CT = component_type_t<T>;
				using FT = typename CT::TypeFull;
				constexpr bool isReadWrite = core::is_mut_v<T>;

				return has_inter<FT>(op, isReadWrite);
			}

			//! Tries to match entity ids in \param queryIds with those in \param archetypeIds given
			//! the comparison function \param func.
			//! \return True if there is a match, false otherwise.
			template <typename Func>
			GAIA_NODISCARD bool match_inter(EntitySpan queryIds, const Chunk::EntityArray& archetypeIds, Func func) const {
				const auto& data = m_lookupCtx.data;

				// Arrays are sorted so we can do linear intersection lookup
				uint32_t i = 0;
				uint32_t j = 0;
				while (i < archetypeIds.size() && j < queryIds.size()) {
					const auto idInArchetype = archetypeIds[i];
					const auto idInQuery = queryIds[j];

					if (func(idInArchetype, idInQuery))
						return true;

					if (SortComponentCond{}.operator()(idInArchetype, idInQuery)) {
						++i;
						continue;
					}

					++j;
				}

				return false;
			}

			//! Tries to match entity ids in \param queryIds with those in \param archetypeIds given
			//! the comparison function \param func.
			//! \return True on the first match, false otherwise.
			GAIA_NODISCARD bool match_one(EntitySpan queryIds, const Chunk::EntityArray& archetypeIds) const {
				return match_inter(queryIds, archetypeIds, [](Entity comp, Entity compQuery) {
					return comp == compQuery;
				});
			}

			//! Tries to match entity ids in \param queryIds with those in \param archetypeIds given
			//! the comparison function \param func.
			//! \return True if all ids match, false otherwise.
			GAIA_NODISCARD bool match_all(EntitySpan queryIds, const Chunk::EntityArray& archetypeIds) const {
				uint32_t matches = 0;
				const auto& data = m_lookupCtx.data;
				return match_inter(queryIds, archetypeIds, [&](Entity comp, Entity compQuery) {
					return comp == compQuery && (++matches == (int32_t)queryIds.size());
				});
			}

		public:
			GAIA_NODISCARD static QueryInfo create(QueryId id, QueryCtx&& ctx) {
				// Make sure query items are sorted
				sort(ctx);

				QueryInfo info;
				info.m_lookupCtx = GAIA_MOV(ctx);
				info.m_lookupCtx.queryId = id;
				return info;
			}

			void set_world_version(uint32_t version) {
				m_worldVersion = version;
			}

			GAIA_NODISCARD uint32_t world_version() const {
				return m_worldVersion;
			}

			GAIA_NODISCARD bool operator==(const QueryCtx& other) const {
				return m_lookupCtx == other;
			}

			GAIA_NODISCARD bool operator!=(const QueryCtx& other) const {
				return !operator==(other);
			}

			//! Tries to match the query against archetypes in \param componentToArchetypeMap.
			//! This is necessary so we do not iterate all chunks over and over again when running queries.
			void match(const ComponentIdToArchetypeMap& componentToArchetypeMap, ArchetypeId archetypeLastId) {
				// Skip if no new archetype appeared
				GAIA_ASSERT(archetypeLastId >= m_lastArchetypeId);
				if (m_lastArchetypeId == archetypeLastId)
					return;
				m_lastArchetypeId = archetypeLastId;

				GAIA_PROF_SCOPE(queryinfo::match);

				auto& data = m_lookupCtx.data;
				if (data.ids.empty())
					return;

				const auto& ops_ids_not = data.ops_ids[(uint32_t)QueryOp::Not];
				const auto& ops_ids_any = data.ops_ids[(uint32_t)QueryOp::Any];
				const auto& ops_ids_all = data.ops_ids[(uint32_t)QueryOp::All];

				if (!ops_ids_all.empty()) {
					// Check if any archetype is associated with the entity id
					const auto it = componentToArchetypeMap.find(EntityLookupKey(ops_ids_all[0]));
					if (it == componentToArchetypeMap.end())
						return;

					// Start from the item 1 instead of 0.
					// Item 0 is guaranteed to be found because we checked it in the component->archetype map.
					EntitySpan s{ops_ids_all.data() + 1, ops_ids_all.size() - 1};

					const auto lastMatchedIdx = data.lastMatchedArchetypeIdx;
					const auto& archetypes = it->second;
					data.lastMatchedArchetypeIdx = archetypes.size();

					for (uint32_t j = lastMatchedIdx; j < archetypes.size();) {
						auto* pArchetype = archetypes[j];

						// Eliminate all archetypes not matching the NOT rule
						if (!ops_ids_not.empty() && match_one({ops_ids_not.data(), ops_ids_not.size()}, pArchetype->entities()))
							goto nextIterationAll;

						// Eliminate all archetypes not matching the ANY rule
						if (!ops_ids_any.empty() && !match_one({ops_ids_any.data(), ops_ids_any.size()}, pArchetype->entities()))
							goto nextIterationAll;

						// Eliminate all archetypes not matching the ALL rule
						if (!s.empty() && !match_all(s, pArchetype->entities()))
							goto nextIterationAll;

						m_archetypeCache.push_back(archetypes[j]);

					nextIterationAll:
						++j;
					}
				} else if (!ops_ids_any.empty()) {
					// Check if any archetype is associated with any of the "Any" entity ids
					ComponentIdToArchetypeMap::const_iterator it;
					GAIA_EACH(ops_ids_any) {
						it = componentToArchetypeMap.find(EntityLookupKey(ops_ids_any[i]));
						if (it == componentToArchetypeMap.end())
							continue;
						break;
					}

					if (it == componentToArchetypeMap.end())
						return;

					const auto lastMatchedIdx = data.lastMatchedArchetypeIdx;
					const auto& archetypes = it->second;
					data.lastMatchedArchetypeIdx = archetypes.size();

					if (!ops_ids_not.empty()) {
						for (uint32_t j = lastMatchedIdx; j < archetypes.size(); ++j) {
							auto* pArchetype = archetypes[j];

							// Eliminate all archetypes not matching the NOT rule
							if (match_one({ops_ids_not.data(), ops_ids_not.size()}, pArchetype->entities()))
								continue;

							m_archetypeCache.push_back(pArchetype);
						}
					} else {
						for (uint32_t j = lastMatchedIdx; j < archetypes.size(); ++j)
							m_archetypeCache.push_back(archetypes[j]);
					}
				} else {
					GAIA_ASSERT2(false, "Querying exclusively for NOT items is not supported yet");
				}
			}

			GAIA_NODISCARD QueryId id() const {
				return m_lookupCtx.queryId;
			}

			GAIA_NODISCARD const QueryCtx::Data& data() const {
				return m_lookupCtx.data;
			}

			GAIA_NODISCARD const QueryEntityArray& ids() const {
				return m_lookupCtx.data.ids;
			}

			GAIA_NODISCARD const QueryEntityArray& filters() const {
				return m_lookupCtx.data.withChanged;
			}

			GAIA_NODISCARD bool has_filters() const {
				return !m_lookupCtx.data.withChanged.empty();
			}

			template <typename... T>
			bool has_any() const {
				return (has_inter<T>(QueryOp::Any) || ...);
			}

			template <typename... T>
			bool has_all() const {
				return (has_inter<T>(QueryOp::All) && ...);
			}

			template <typename... T>
			bool has_none() const {
				return (!has_inter<T>(QueryOp::Not) && ...);
			}

			//! Removes an archetype from cache
			//! \param pArchetype Archetype to remove
			void remove(Archetype* pArchetype) {
				GAIA_PROF_SCOPE(queryinfo::remove);

				const auto idx = core::get_index(m_archetypeCache, pArchetype);
				if (idx == BadIndex)
					return;
				core::erase_fast(m_archetypeCache, idx);

				// An archetype was removed from the world so the last matching archetype index needs to be
				// lowered by one for every component context.
				// for (auto& lastMatchedArchetypeIdx: m_lookupCtx.data.lastMatchedArchetypeIdx) {
				// 	if (lastMatchedArchetypeIdx > 0)
				// 		--lastMatchedArchetypeIdx;
				// }
			}

			GAIA_NODISCARD ArchetypeList::iterator begin() {
				return m_archetypeCache.begin();
			}

			GAIA_NODISCARD ArchetypeList::iterator end() {
				return m_archetypeCache.end();
			}
		};
	} // namespace ecs
} // namespace gaia
