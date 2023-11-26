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
					const auto compIdx = ecs::comp_idx<MAX_COMPONENTS_IN_QUERY>(ids.data(), comp);

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

			//! Tries to match query component ids with those in \param ids given the rule \param func.
			//! \return True if there is a match, false otherwise.
			template <typename Func>
			GAIA_NODISCARD bool match_inter(const Chunk::EntityArray& ids, QueryOp op, Func func) const {
				const auto& data = m_lookupCtx.data;

				// Arrays are sorted so we can do linear intersection lookup
				uint32_t i = 0;
				uint32_t j = 0;
				while (i < ids.size() && j < data.ids.size()) {
					if (data.ops[j] == op) {
						const auto compArchetype = ids[i];
						const auto compQuery = data.ids[j];

						if (compArchetype == compQuery && func(compArchetype, compQuery))
							return true;

						if (SortComponentCond{}.operator()(compArchetype, compQuery)) {
							++i;
							continue;
						}
					}
					++j;
				}

				return false;
			}

			//! Tries to match all query component ids with those in \param ids.
			//! \return True on the first match, false otherwise.
			GAIA_NODISCARD bool match_one(const Chunk::EntityArray& ids, QueryOp op) const {
				return match_inter(ids, op, [](Entity comp, Entity compQuery) {
					return comp == compQuery;
				});
			}

			//! Tries to match all query component ids with those in \param ids.
			//! \return True if all ids match, false otherwise.
			GAIA_NODISCARD bool match_all(const Chunk::EntityArray& ids) const {
				uint32_t matches = 0;
				const auto& data = m_lookupCtx.data;
				return match_inter(ids, QueryOp::All, [&](Entity comp, Entity compQuery) {
					return comp == compQuery && (++matches == data.opsAllCount);
				});
			}

			//! Tries to match component with component type \param kind from the archetype \param archetype with
			//! the query. \return MatchArchetypeQueryRet::Fail if there is no match, MatchArchetypeQueryRet::Ok for match
			//! or MatchArchetypeQueryRet::Skip is not relevant.
			GAIA_NODISCARD MatchArchetypeQueryRet match(const Archetype& archetype) const {
				GAIA_PROF_SCOPE(queryinfo::match);

				const auto& matcherHash = archetype.matcher_hash();
				const auto& ids = archetype.entities();
				const auto& compData = data();

				const auto withNoneTest = matcherHash.hash & compData.matcherHash[(uint32_t)QueryOp::Not].hash;
				const auto withAnyTest = matcherHash.hash & compData.matcherHash[(uint32_t)QueryOp::Any].hash;
				const auto withAllTest = matcherHash.hash & compData.matcherHash[(uint32_t)QueryOp::All].hash;

				// If withAnyTest is empty but we wanted something
				if (withAnyTest == 0 && compData.matcherHash[(uint32_t)QueryOp::Any].hash != 0)
					return MatchArchetypeQueryRet::Fail;

				// If withAllTest is empty but we wanted something
				if (withAllTest == 0 && compData.matcherHash[(uint32_t)QueryOp::All].hash != 0)
					return MatchArchetypeQueryRet::Fail;

				// If there is any match with withNoneList we quit
				if (withNoneTest != 0) {
					if (match_one(ids, QueryOp::Not))
						return MatchArchetypeQueryRet::Fail;
				}

				// If there is any match with withAnyTest
				if (withAnyTest != 0) {
					if (match_one(ids, QueryOp::Any))
						goto checkWithAllMatches;

					// At least one match necessary to continue
					return MatchArchetypeQueryRet::Fail;
				}

			checkWithAllMatches:
				// If withAllList is not empty there has to be an exact match
				if (withAllTest != 0) {
					if (
							// We skip archetypes with less components than the query requires
							compData.opsAllCount <= ids.size() &&
							// Match everything with LT_ALL
							match_all(ids))
						return MatchArchetypeQueryRet::Ok;

					// No match found. We're done
					return MatchArchetypeQueryRet::Fail;
				}

				return (withAnyTest != 0) ? MatchArchetypeQueryRet::Ok : MatchArchetypeQueryRet::Skip;
			}

		public:
			GAIA_NODISCARD static QueryInfo create(QueryId id, QueryCtx&& ctx) {
				QueryInfo info;
				matcher_hashes(ctx);
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
				static cnt::set<Archetype*> s_tmpArchetypeMatches;

				// Skip if no new archetype appeared
				GAIA_ASSERT(archetypeLastId >= m_lastArchetypeId);
				if (m_lastArchetypeId == archetypeLastId)
					return;
				m_lastArchetypeId = archetypeLastId;

				GAIA_PROF_SCOPE(queryinfo::match);

				auto& data = m_lookupCtx.data;
				GAIA_EACH(data.ids) {
					const auto comp = data.ids[i];

					// Check if any archetype is associated with the component id
					const auto it = componentToArchetypeMap.find(EntityLookupKey(comp));
					if (it == componentToArchetypeMap.end())
						continue;

					const auto& archetypes = it->second;
					const auto lastMatchedIdx = data.lastMatchedArchetypeIdx[i];
					GAIA_FOR2_(lastMatchedIdx, archetypes.size(), j) {
						auto* pArchetype = archetypes[j];

						// Early exit if query doesn't match
						const auto ret = match(*pArchetype);
						if (ret == MatchArchetypeQueryRet::Fail)
							continue;

						(void)s_tmpArchetypeMatches.emplace(pArchetype);
					}
					data.lastMatchedArchetypeIdx[i] = archetypes.size();
				}

				for (auto* pArchetype: s_tmpArchetypeMatches)
					m_archetypeCache.push_back(pArchetype);
				s_tmpArchetypeMatches.clear();
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
				for (auto& lastMatchedArchetypeIdx: m_lookupCtx.data.lastMatchedArchetypeIdx) {
					if (lastMatchedArchetypeIdx > 0)
						--lastMatchedArchetypeIdx;
				}
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
