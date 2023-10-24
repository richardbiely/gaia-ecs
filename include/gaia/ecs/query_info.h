#pragma once
#include "../config/config.h"

#include "../cnt/darray.h"
#include "../cnt/sarray_ext.h"
#include "../cnt/set.h"
#include "../core/hashing_policy.h"
#include "../core/utility.h"
#include "archetype.h"
#include "archetype_common.h"
#include "component.h"
#include "component_utils.h"
#include "query_common.h"

namespace gaia {
	namespace ecs {
		struct Entity;

		namespace query {
			using ComponentToArchetypeMap = cnt::map<component::ComponentId, archetype::ArchetypeList>;

			class QueryInfo {
			public:
				//! Query matching result
				enum class MatchArchetypeQueryRet : uint8_t { Fail, Ok, Skip };

			private:
				//! Lookup context
				query::LookupCtx m_lookupCtx;
				//! List of archetypes matching the query
				archetype::ArchetypeList m_archetypeCache;
				//! Index of the last archetype in the world we checked
				uint32_t m_lastArchetypeIdx = 1; // skip the root archetype
				//! Version of the world for which the query has been called most recently
				uint32_t m_worldVersion = 0;

				template <typename T>
				bool has_inter(
						[[maybe_unused]] query::ListType listType, [[maybe_unused]] component::ComponentType compType,
						bool isReadWrite) const {
					if constexpr (std::is_same_v<T, Entity>) {
						// Skip Entity input args
						return true;
					} else {
						const auto& data = m_lookupCtx.data[compType];
						const auto& compIds = data.compIds;

						// Component id has to be present
						const auto componentId = component::comp_id<T>();
						GAIA_ASSERT(core::has(compIds, componentId));
						const auto idx = core::get_index_unsafe(compIds, componentId);
						if (listType != query::ListType::LT_Count && listType != data.rules[idx])
							return false;

						// Read-write mask must match
						const uint32_t maskRW = (uint32_t)data.readWriteMask & (1U << (uint32_t)idx);
						const uint32_t maskXX = (uint32_t)isReadWrite << idx;
						return maskRW == maskXX;
					}
				}

				template <typename T>
				bool has_inter(query::ListType listType) const {
					using U = typename component::component_type_t<T>::Type;
					using UOriginal = typename component::component_type_t<T>::TypeOriginal;
					using UOriginalPR = std::remove_reference_t<std::remove_pointer_t<UOriginal>>;
					constexpr bool isReadWrite =
							std::is_same_v<U, UOriginal> || (!std::is_const_v<UOriginalPR> && !std::is_empty_v<U>);

					constexpr auto compType = component::component_type_v<T>;
					return has_inter<U>(listType, compType, isReadWrite);
				}

				//! Tries to match query component ids with those in \param compIds given the rule \param func.
				//! \return True if there is a match, false otherwise.
				template <typename Func>
				GAIA_NODISCARD bool match_inter(
						component::ComponentType compType, const archetype::ComponentIdArray& compIds, ListType listType,
						Func func) const {
					const auto& data = m_lookupCtx.data[compType];

					// Arrays are sorted so we can do linear intersection lookup
					uint32_t i = 0;
					uint32_t j = 0;
					while (i < compIds.size() && j < data.compIds.size()) {
						if (data.rules[j] == listType) {
							const auto componentIdArchetype = compIds[i];
							const auto componentIdQuery = data.compIds[j];

							if (componentIdArchetype == componentIdQuery && func(componentIdArchetype, componentIdQuery))
								return true;

							if (component::SortComponentCond{}.operator()(componentIdArchetype, componentIdQuery)) {
								++i;
								continue;
							}
						}
						++j;
					}

					return false;
				}

				//! Tries to match all query component ids with those in \param compIds.
				//! \return True on the first match, false otherwise.
				GAIA_NODISCARD bool match_one(
						component::ComponentType compType, const archetype::ComponentIdArray& compIds, ListType listType) const {
					return match_inter(
							compType, compIds, listType,
							[](component::ComponentId componentId, component::ComponentId componentIdQuery) {
								return componentId == componentIdQuery;
							});
				}

				//! Tries to match all query component ids with those in \param compIds.
				//! \return True if all ids match, false otherwise.
				GAIA_NODISCARD bool
				match_all(component::ComponentType compType, const archetype::ComponentIdArray& compIds) const {
					uint32_t matches = 0;
					const auto& data = m_lookupCtx.data[compType];
					return match_inter(
							compType, compIds, ListType::LT_All,
							[&](component::ComponentId componentId, component::ComponentId componentIdQuery) {
								return componentId == componentIdQuery && (++matches == data.rulesAllCount);
							});
				}

				//! Tries to match component with component type \param compType from the archetype \param archetype with
				//! the query. \return MatchArchetypeQueryRet::Fail if there is no match, MatchArchetypeQueryRet::Ok for match
				//! or MatchArchetypeQueryRet::Skip is not relevant.
				GAIA_NODISCARD MatchArchetypeQueryRet
				match(const archetype::Archetype& archetype, component::ComponentType compType) const {
					const auto& matcherHash = archetype.matcher_hash(compType);
					const auto& compIds = archetype.comp_ids(compType);
					const auto& compData = data(compType);

					const auto withNoneTest = matcherHash.hash & compData.hash[query::ListType::LT_None].hash;
					const auto withAnyTest = matcherHash.hash & compData.hash[query::ListType::LT_Any].hash;
					const auto withAllTest = matcherHash.hash & compData.hash[query::ListType::LT_All].hash;

					// If withAnyTest is empty but we wanted something
					if (withAnyTest == 0 && compData.hash[query::ListType::LT_Any].hash != 0)
						return MatchArchetypeQueryRet::Fail;

					// If withAllTest is empty but we wanted something
					if (withAllTest == 0 && compData.hash[query::ListType::LT_All].hash != 0)
						return MatchArchetypeQueryRet::Fail;

					// If there is any match with withNoneList we quit
					if (withNoneTest != 0) {
						if (match_one(compType, compIds, query::ListType::LT_None))
							return MatchArchetypeQueryRet::Fail;
					}

					// If there is any match with withAnyTest
					if (withAnyTest != 0) {
						if (match_one(compType, compIds, query::ListType::LT_Any))
							goto checkWithAllMatches;

						// At least one match necessary to continue
						return MatchArchetypeQueryRet::Fail;
					}

				checkWithAllMatches:
					// If withAllList is not empty there has to be an exact match
					if (withAllTest != 0) {
						if (
								// We skip archetypes with less components than the query requires
								compData.rulesAllCount <= compIds.size() &&
								// Match everything with LT_ALL
								match_all(compType, compIds))
							return MatchArchetypeQueryRet::Ok;

						// No match found. We're done
						return MatchArchetypeQueryRet::Fail;
					}

					return (withAnyTest != 0) ? MatchArchetypeQueryRet::Ok : MatchArchetypeQueryRet::Skip;
				}

			public:
				GAIA_NODISCARD static QueryInfo create(QueryId id, query::LookupCtx&& ctx) {
					QueryInfo info;
					query::matcher_hashes(ctx);
					info.m_lookupCtx = std::move(ctx);
					info.m_lookupCtx.queryId = id;
					return info;
				}

				void set_world_version(uint32_t version) {
					m_worldVersion = version;
				}

				GAIA_NODISCARD uint32_t world_version() const {
					return m_worldVersion;
				}

				query::LookupHash lookup_hash() const {
					return m_lookupCtx.hashLookup;
				}

				GAIA_NODISCARD bool operator==(const query::LookupCtx& other) const {
					return m_lookupCtx == other;
				}

				GAIA_NODISCARD bool operator!=(const query::LookupCtx& other) const {
					return !operator==(other);
				}

				//! Tries to match the query against archetypes in \param componentToArchetypeMap. For each matched archetype
				//! the archetype is cached. This is necessary so we do not iterate all chunks over and over again when running
				//! queries.
				void match(const ComponentToArchetypeMap& componentToArchetypeMap, uint32_t archetypeCount) {
					static cnt::set<archetype::Archetype*> s_tmpArchetypeMatches;

					// Skip if no new archetype appeared
					if (m_lastArchetypeIdx == archetypeCount)
						return;
					m_lastArchetypeIdx = archetypeCount;

					// Match against generic types
					{
						auto& data = m_lookupCtx.data[component::ComponentType::CT_Generic];
						for (uint32_t i = 0; i < data.compIds.size(); ++i) {
							const auto componentId = data.compIds[i];

							const auto it = componentToArchetypeMap.find(componentId);
							if (it == componentToArchetypeMap.end())
								continue;

							for (uint32_t j = data.lastMatchedArchetypeIndex[i]; j < it->second.size(); ++j) {
								auto* pArchetype = it->second[j];
								// Early exit if generic query doesn't match
								const auto retGeneric = match(*pArchetype, component::ComponentType::CT_Generic);
								if (retGeneric == MatchArchetypeQueryRet::Fail)
									continue;

								(void)s_tmpArchetypeMatches.emplace(pArchetype);
							}
							data.lastMatchedArchetypeIndex[i] = (uint32_t)it->second.size();
						}
					}

					// Match against chunk types
					{
						auto& data = m_lookupCtx.data[component::ComponentType::CT_Chunk];
						for (uint32_t i = 0; i < data.compIds.size(); ++i) {
							const auto componentId = data.compIds[i];

							const auto it = componentToArchetypeMap.find(componentId);
							if (it == componentToArchetypeMap.end())
								continue;

							for (uint32_t j = data.lastMatchedArchetypeIndex[i]; j < it->second.size(); ++j) {
								auto* pArchetype = it->second[j];
								// Early exit if generic query doesn't match
								const auto retGeneric = match(*pArchetype, component::ComponentType::CT_Chunk);
								if (retGeneric == MatchArchetypeQueryRet::Fail) {
									s_tmpArchetypeMatches.erase(pArchetype);
									continue;
								}

								(void)s_tmpArchetypeMatches.emplace(pArchetype);
							}
							data.lastMatchedArchetypeIndex[i] = (uint32_t)it->second.size();
						}
					}

					for (auto* pArchetype: s_tmpArchetypeMatches)
						m_archetypeCache.push_back(pArchetype);
					s_tmpArchetypeMatches.clear();
				}

				GAIA_NODISCARD QueryId id() const {
					return m_lookupCtx.queryId;
				}

				GAIA_NODISCARD const query::LookupCtx::Data& data(component::ComponentType compType) const {
					return m_lookupCtx.data[compType];
				}

				GAIA_NODISCARD const query::ComponentIdArray& comp_ids(component::ComponentType compType) const {
					return m_lookupCtx.data[compType].compIds;
				}

				GAIA_NODISCARD const query::ChangeFilterArray& filters(component::ComponentType compType) const {
					return m_lookupCtx.data[compType].withChanged;
				}

				GAIA_NODISCARD bool has_filters() const {
					return !m_lookupCtx.data[component::ComponentType::CT_Generic].withChanged.empty() ||
								 !m_lookupCtx.data[component::ComponentType::CT_Chunk].withChanged.empty();
				}

				template <typename... T>
				bool has() const {
					return (has_inter<T>(query::ListType::LT_Count) || ...);
				}

				template <typename... T>
				bool has_any() const {
					return (has_inter<T>(query::ListType::LT_Any) || ...);
				}

				template <typename... T>
				bool has_all() const {
					return (has_inter<T>(query::ListType::LT_All) && ...);
				}

				template <typename... T>
				bool has_none() const {
					return (!has_inter<T>(query::ListType::LT_None) && ...);
				}

				GAIA_NODISCARD archetype::ArchetypeList::iterator begin() {
					return m_archetypeCache.begin();
				}

				GAIA_NODISCARD archetype::ArchetypeList::iterator end() {
					return m_archetypeCache.end();
				}
			};
		} // namespace query
	} // namespace ecs
} // namespace gaia
