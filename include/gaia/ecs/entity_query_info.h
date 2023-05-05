#pragma once
#include "../containers/darray.h"
#include "../containers/sarray_ext.h"
#include "../utils/hashing_policy.h"
#include "../utils/utility.h"
#include "archetype.h"
#include "archetype_common.h"
#include "component.h"
#include "component_utils.h"
#include "entity_query_common.h"

namespace gaia {
	namespace ecs {
		struct Entity;

		namespace query {
			class EntityQueryInfo {
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
				bool HasComponent_Internal(
						[[maybe_unused]] query::ListType listType, [[maybe_unused]] component::ComponentType componentType,
						bool isReadWrite) const {
					if constexpr (std::is_same_v<T, Entity>) {
						// Skip Entity input args
						return true;
					} else {
						const auto& componentIds = m_lookupCtx.list[componentType].componentIds[listType];

						// Component id has to be present
						const auto componentId = component::GetComponentId<T>();
						const auto idx = utils::get_index(componentIds, componentId);
						if (idx == BadIndex)
							return false;

						// Read-write mask must match
						const uint8_t maskRW = (uint32_t)m_lookupCtx.rw[componentType] & (1U << (uint32_t)idx);
						const uint8_t maskXX = (uint32_t)isReadWrite << idx;
						return maskRW == maskXX;
					}
				}

				template <typename T>
				bool HasComponent_Internal(query::ListType listType) const {
					using U = typename component::DeduceComponent<T>::Type;
					using UOriginal = typename component::DeduceComponent<T>::TypeOriginal;
					using UOriginalPR = std::remove_reference_t<std::remove_pointer_t<UOriginal>>;
					constexpr bool isReadWrite =
							std::is_same_v<U, UOriginal> || (!std::is_const_v<UOriginalPR> && !std::is_empty_v<U>);

					if constexpr (component::IsGenericComponent<T>)
						return HasComponent_Internal<U>(listType, component::ComponentType::CT_Generic, isReadWrite);
					else
						return HasComponent_Internal<U>(listType, component::ComponentType::CT_Chunk, isReadWrite);
				}

				//! Tries to match component ids in \param componentIdsQuery with those in \param componentIds.
				//! \return True if there is a match, false otherwise.
				static GAIA_NODISCARD bool CheckMatchOne(
						const archetype::ComponentIdArray& componentIds, const query::ComponentIdArray& componentIdsQuery) {
					// Arrays are sorted so we can do linear intersection lookup
					size_t i = 0;
					size_t j = 0;
					while (i < componentIds.size() && j < componentIdsQuery.size()) {
						const auto componentId = componentIds[i];
						const auto componentIdQuery = componentIdsQuery[j];

						if (componentId == componentIdQuery)
							return true;

						if (component::SortComponentCond{}.operator()(componentId, componentIdQuery))
							++i;
						else
							++j;
					}

					return false;
				}

				//! Tries to match all component ids in \param componentIdsQuery with those in \param componentIds.
				//! \return True if there is a match, false otherwise.
				static GAIA_NODISCARD bool CheckMatchMany(
						const archetype::ComponentIdArray& componentIds, const query::ComponentIdArray& componentIdsQuery) {
					size_t matches = 0;

					// Arrays are sorted so we can do linear intersection lookup
					size_t i = 0;
					size_t j = 0;
					while (i < componentIds.size() && j < componentIdsQuery.size()) {
						const auto componentId = componentIds[i];
						const auto componentIdQuery = componentIdsQuery[j];

						if (componentId == componentIdQuery) {
							if (++matches == componentIdsQuery.size())
								return true;
						}

						if (component::SortComponentCond{}.operator()(componentId, componentIdQuery))
							++i;
						else
							++j;
					}

					return false;
				}

				//! Tries to match component with component type \param componentType from the archetype \param archetype with
				//! the query. \return MatchArchetypeQueryRet::Fail if there is no match, MatchArchetypeQueryRet::Ok for match
				//! or MatchArchetypeQueryRet::Skip is not relevant.
				GAIA_NODISCARD MatchArchetypeQueryRet
				Match(const archetype::Archetype& archetype, component::ComponentType componentType) const {
					const auto& matcherHash = archetype.GetMatcherHash(componentType);
					const auto& queryList = GetData(componentType);

					const auto withNoneTest = matcherHash.hash & queryList.hash[query::ListType::LT_None].hash;
					const auto withAnyTest = matcherHash.hash & queryList.hash[query::ListType::LT_Any].hash;
					const auto withAllTest = matcherHash.hash & queryList.hash[query::ListType::LT_All].hash;

					// If withAllTest is empty but we wanted something
					if (withAllTest == 0 && queryList.hash[query::ListType::LT_All].hash != 0)
						return MatchArchetypeQueryRet::Fail;

					// If withAnyTest is empty but we wanted something
					if (withAnyTest == 0 && queryList.hash[query::ListType::LT_Any].hash != 0)
						return MatchArchetypeQueryRet::Fail;

					const auto& componentIds = archetype.GetComponentIdArray(componentType);

					// If there is any match with withNoneList we quit
					if (withNoneTest != 0) {
						if (CheckMatchOne(componentIds, queryList.componentIds[query::ListType::LT_None]))
							return MatchArchetypeQueryRet::Fail;
					}

					// If there is any match with withAnyTest
					if (withAnyTest != 0) {
						if (CheckMatchOne(componentIds, queryList.componentIds[query::ListType::LT_Any]))
							goto checkWithAllMatches;

						// At least one match necessary to continue
						return MatchArchetypeQueryRet::Fail;
					}

				checkWithAllMatches:
					// If withAllList is not empty there has to be an exact match
					if (withAllTest != 0) {
						// If the number of queried components is greater than the
						// number of components in archetype there's no need to search
						if (queryList.componentIds[query::ListType::LT_All].size() <= componentIds.size()) {
							// m_list[ListType::LT_All] first because we usually request for less
							// components than there are components in archetype
							if (CheckMatchMany(componentIds, queryList.componentIds[query::ListType::LT_All]))
								return MatchArchetypeQueryRet::Ok;
						}

						// No match found. We're done
						return MatchArchetypeQueryRet::Fail;
					}

					return (withAnyTest != 0) ? MatchArchetypeQueryRet::Ok : MatchArchetypeQueryRet::Skip;
				}

			public:
				static GAIA_NODISCARD EntityQueryInfo Create(QueryId id, query::LookupCtx&& ctx) {
					EntityQueryInfo info;
					query::CalculateMatcherHashes(ctx);
					info.m_lookupCtx = std::move(ctx);
					info.m_lookupCtx.queryId = id;
					return info;
				}

				void SetWorldVersion(uint32_t version) {
					m_worldVersion = version;
				}

				GAIA_NODISCARD uint32_t GetWorldVersion() const {
					return m_worldVersion;
				}

				query::LookupHash GetLookupHash() const {
					return m_lookupCtx.hashLookup;
				}

				GAIA_NODISCARD bool operator==(const query::LookupCtx& other) const {
					return m_lookupCtx == other;
				}

				GAIA_NODISCARD bool operator!=(const query::LookupCtx& other) const {
					return !operator==(other);
				}

				//! Tries to match the query against \param archetypes. For each matched archetype the archetype is cached.
				//! This is necessary so we do not iterate all chunks over and over again when running queries.
				void Match(const archetype::ArchetypeList& archetypes) {
					for (size_t i = m_lastArchetypeIdx; i < archetypes.size(); i++) {
						auto* pArchetype = archetypes[i];

						// Early exit if generic query doesn't match
						const auto retGeneric = Match(*pArchetype, component::ComponentType::CT_Generic);
						if (retGeneric == MatchArchetypeQueryRet::Fail)
							continue;

						// Early exit if chunk query doesn't match
						const auto retChunk = Match(*pArchetype, component::ComponentType::CT_Chunk);
						if (retChunk == MatchArchetypeQueryRet::Fail)
							continue;

						// If at least one query succeeded run our logic
						if (retGeneric == MatchArchetypeQueryRet::Ok || retChunk == MatchArchetypeQueryRet::Ok)
							m_archetypeCache.push_back(pArchetype);
					}

					m_lastArchetypeIdx = (uint32_t)archetypes.size();
				}

				GAIA_NODISCARD const query::ComponentListData& GetData(component::ComponentType componentType) const {
					return m_lookupCtx.list[componentType];
				}

				GAIA_NODISCARD const query::ChangeFilterArray& GetFiltered(component::ComponentType componentType) const {
					return m_lookupCtx.listChangeFiltered[componentType];
				}

				GAIA_NODISCARD bool HasFilters() const {
					return !m_lookupCtx.listChangeFiltered[component::ComponentType::CT_Generic].empty() ||
								 !m_lookupCtx.listChangeFiltered[component::ComponentType::CT_Chunk].empty();
				}

				template <typename... T>
				bool HasAny() const {
					return (HasComponent_Internal<T>(query::ListType::LT_Any) || ...);
				}

				template <typename... T>
				bool HasAll() const {
					return (HasComponent_Internal<T>(query::ListType::LT_All) && ...);
				}

				template <typename... T>
				bool HasNone() const {
					return (!HasComponent_Internal<T>(query::ListType::LT_None) && ...);
				}

				GAIA_NODISCARD archetype::ArchetypeList::iterator begin() {
					return m_archetypeCache.begin();
				}

				GAIA_NODISCARD archetype::ArchetypeList::const_iterator begin() const {
					return m_archetypeCache.cbegin();
				}

				GAIA_NODISCARD archetype::ArchetypeList::iterator end() {
					return m_archetypeCache.end();
				}

				GAIA_NODISCARD archetype::ArchetypeList::const_iterator end() const {
					return m_archetypeCache.cend();
				}
			};
		} // namespace query
	} // namespace ecs
} // namespace gaia
