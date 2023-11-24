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
			bool
			has_inter([[maybe_unused]] QueryListType listType, [[maybe_unused]] EntityKind kind, bool isReadWrite) const {
				if constexpr (std::is_same_v<T, Entity>) {
					// Entities are read-only.
					GAIA_ASSERT(!isReadWrite);
					// Skip Entity input args. Entities are always there.
					return true;
				} else {
					const auto& data = m_lookupCtx.data[kind];
					const auto& comps = data.comps;

					const auto comp = m_lookupCtx.cc->get<T>().entity;
					const auto compIdx = ecs::comp_idx<MAX_COMPONENTS_IN_QUERY>(comps.data(), comp);

					if (listType != data.rules[compIdx])
						return false;

					// Read-write mask must match
					const uint32_t maskRW = (uint32_t)data.readWriteMask & (1U << compIdx);
					const uint32_t maskXX = (uint32_t)isReadWrite << compIdx;
					return maskRW == maskXX;
				}
			}

			template <typename T>
			bool has_inter(QueryListType listType) const {
				// static_assert(is_raw_v<<T>, "has() must be used with raw types");

				using CT = component_type_t<T>;
				using FT = typename CT::TypeFull;
				constexpr auto kind = CT::Kind;
				constexpr bool isReadWrite = core::is_mut_v<T>;

				return has_inter<FT>(listType, kind, isReadWrite);
			}

			//! Tries to match query component ids with those in \param comps given the rule \param func.
			//! \return True if there is a match, false otherwise.
			template <typename Func>
			GAIA_NODISCARD bool
			match_inter(EntityKind kind, const Chunk::EntityArray& comps, QueryListType listType, Func func) const {
				const auto& data = m_lookupCtx.data[kind];

				// Arrays are sorted so we can do linear intersection lookup
				uint32_t i = 0;
				uint32_t j = 0;
				while (i < comps.size() && j < data.comps.size()) {
					if (data.rules[j] == listType) {
						const auto compArchetype = comps[i];
						const auto compQuery = data.comps[j];

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

			//! Tries to match all query component ids with those in \param comps.
			//! \return True on the first match, false otherwise.
			GAIA_NODISCARD bool match_one(EntityKind kind, const Chunk::EntityArray& comps, QueryListType listType) const {
				return match_inter(kind, comps, listType, [](Entity comp, Entity compQuery) {
					return comp == compQuery;
				});
			}

			//! Tries to match all query component ids with those in \param comps.
			//! \return True if all ids match, false otherwise.
			GAIA_NODISCARD bool match_all(EntityKind kind, const Chunk::EntityArray& comps) const {
				uint32_t matches = 0;
				const auto& data = m_lookupCtx.data[kind];
				return match_inter(kind, comps, QueryListType::LT_All, [&](Entity comp, Entity compQuery) {
					return comp == compQuery && (++matches == data.rulesAllCount);
				});
			}

			//! Tries to match component with component type \param kind from the archetype \param archetype with
			//! the query. \return MatchArchetypeQueryRet::Fail if there is no match, MatchArchetypeQueryRet::Ok for match
			//! or MatchArchetypeQueryRet::Skip is not relevant.
			GAIA_NODISCARD MatchArchetypeQueryRet match(const Archetype& archetype, EntityKind kind) const {
				GAIA_PROF_SCOPE(queryinfo::match);

				const auto& matcherHash = archetype.matcher_hash(kind);
				const auto& comps = archetype.entities(kind);
				const auto& compData = data(kind);

				const auto withNoneTest = matcherHash.hash & compData.hash[QueryListType::LT_None].hash;
				const auto withAnyTest = matcherHash.hash & compData.hash[QueryListType::LT_Any].hash;
				const auto withAllTest = matcherHash.hash & compData.hash[QueryListType::LT_All].hash;

				// If withAnyTest is empty but we wanted something
				if (withAnyTest == 0 && compData.hash[QueryListType::LT_Any].hash != 0)
					return MatchArchetypeQueryRet::Fail;

				// If withAllTest is empty but we wanted something
				if (withAllTest == 0 && compData.hash[QueryListType::LT_All].hash != 0)
					return MatchArchetypeQueryRet::Fail;

				// If there is any match with withNoneList we quit
				if (withNoneTest != 0) {
					if (match_one(kind, comps, QueryListType::LT_None))
						return MatchArchetypeQueryRet::Fail;
				}

				// If there is any match with withAnyTest
				if (withAnyTest != 0) {
					if (match_one(kind, comps, QueryListType::LT_Any))
						goto checkWithAllMatches;

					// At least one match necessary to continue
					return MatchArchetypeQueryRet::Fail;
				}

			checkWithAllMatches:
				// If withAllList is not empty there has to be an exact match
				if (withAllTest != 0) {
					if (
							// We skip archetypes with less components than the query requires
							compData.rulesAllCount <= comps.size() &&
							// Match everything with LT_ALL
							match_all(kind, comps))
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

				// Match against generic types
				{
					auto& data = m_lookupCtx.data[EntityKind::EK_Gen];
					GAIA_EACH(data.comps) {
						const auto comp = data.comps[i];

						// Check if any archetype is associated with the component id
						const auto it = componentToArchetypeMap.find(EntityLookupKey(comp));
						if (it == componentToArchetypeMap.end())
							continue;

						const auto& archetypes = it->second;
						const auto lastMatchedIdx = data.lastMatchedArchetypeIdx[i];
						GAIA_FOR2_(lastMatchedIdx, archetypes.size(), j) {
							auto* pArchetype = archetypes[j];

							// Early exit if generic query doesn't match
							const auto ret = match(*pArchetype, EntityKind::EK_Gen);
							if (ret == MatchArchetypeQueryRet::Fail)
								continue;

							(void)s_tmpArchetypeMatches.emplace(pArchetype);
						}
						data.lastMatchedArchetypeIdx[i] = archetypes.size();
					}
				}

				// Match against unique types
				{
					auto& data = m_lookupCtx.data[EntityKind::EK_Uni];
					GAIA_EACH(data.comps) {
						const auto comp = data.comps[i];

						const auto it = componentToArchetypeMap.find(EntityLookupKey(comp));
						if (it == componentToArchetypeMap.end())
							continue;

						GAIA_FOR2_(data.lastMatchedArchetypeIdx[i], it->second.size(), j) {
							auto* pArchetype = it->second[j];
							// Early exit if unique query doesn't match
							const auto ret = match(*pArchetype, EntityKind::EK_Uni);
							if (ret == MatchArchetypeQueryRet::Fail) {
								s_tmpArchetypeMatches.erase(pArchetype);
								continue;
							}

							(void)s_tmpArchetypeMatches.emplace(pArchetype);
						}
						data.lastMatchedArchetypeIdx[i] = (uint32_t)it->second.size();
					}
				}

				for (auto* pArchetype: s_tmpArchetypeMatches)
					m_archetypeCache.push_back(pArchetype);
				s_tmpArchetypeMatches.clear();
			}

			GAIA_NODISCARD QueryId id() const {
				return m_lookupCtx.queryId;
			}

			GAIA_NODISCARD const QueryCtx::Data& data(EntityKind kind) const {
				return m_lookupCtx.data[kind];
			}

			GAIA_NODISCARD const QueryComponentArray& comps(EntityKind kind) const {
				return m_lookupCtx.data[kind].comps;
			}

			GAIA_NODISCARD const QueryChangeArray& filters(EntityKind kind) const {
				return m_lookupCtx.data[kind].withChanged;
			}

			GAIA_NODISCARD bool has_filters() const {
				return !m_lookupCtx.data[EntityKind::EK_Gen].withChanged.empty() ||
							 !m_lookupCtx.data[EntityKind::EK_Uni].withChanged.empty();
			}

			template <typename... T>
			bool has_any() const {
				return (has_inter<T>(QueryListType::LT_Any) || ...);
			}

			template <typename... T>
			bool has_all() const {
				return (has_inter<T>(QueryListType::LT_All) && ...);
			}

			template <typename... T>
			bool has_none() const {
				return (!has_inter<T>(QueryListType::LT_None) && ...);
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
				for (auto& ctxData: m_lookupCtx.data) {
					for (auto& lastMatchedArchetypeIdx: ctxData.lastMatchedArchetypeIdx) {
						if (lastMatchedArchetypeIdx > 0)
							--lastMatchedArchetypeIdx;
					}
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
