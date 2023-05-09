#pragma once
#include "../config/config.h"

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
						const auto& data = m_lookupCtx.data[componentType];
						const auto& componentIds = data.componentIds;

						// Component id has to be present
						const auto componentId = component::GetComponentId<T>();
						GAIA_ASSERT(utils::has(componentIds, componentId));
						const auto idx = utils::get_index_unsafe(componentIds, componentId);
						if (listType != query::ListType::LT_Count && listType != data.rules[idx])
							return false;

						// Read-write mask must match
						const uint8_t maskRW = (uint32_t)data.readWriteMask & (1U << (uint32_t)idx);
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

				template <typename Func>
				GAIA_NODISCARD bool CheckMatch_Internal(
						component::ComponentType componentType, const archetype::ComponentIdArray& archetypeComponentIds,
						ListType listType, Func func) const {
					const auto& data = m_lookupCtx.data[componentType];

					// Arrays are sorted so we can do linear intersection lookup
					size_t i = 0;
					size_t j = 0;
					while (i < archetypeComponentIds.size() && j < data.componentIds.size()) {
						if (data.rules[j] == listType) {
							const auto componentIdArchetype = archetypeComponentIds[i];
							const auto componentIdQuery = data.componentIds[j];

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

				//! Tries to match component ids in \param componentIdsQuery with those in \param componentIds.
				//! \return True if there is a match, false otherwise.
				GAIA_NODISCARD bool CheckMatchOne(
						component::ComponentType componentType, const archetype::ComponentIdArray& archetypeComponentIds,
						ListType listType) const {
					return CheckMatch_Internal(
							componentType, archetypeComponentIds, listType,
							[](component::ComponentId componentId, component::ComponentId componentIdQuery) {
								return componentId == componentIdQuery;
							});
				}

				//! Tries to match all component ids in \param componentIdsQuery with those in \param componentIds.
				//! \return True if there is a match, false otherwise.
				GAIA_NODISCARD bool CheckMatchAll(
						component::ComponentType componentType, const archetype::ComponentIdArray& archetypeComponentIds) const {
					size_t matches = 0;
					const auto& data = m_lookupCtx.data[componentType];
					return CheckMatch_Internal(
							componentType, archetypeComponentIds, ListType::LT_All,
							[&](component::ComponentId componentId, component::ComponentId componentIdQuery) {
								return componentId == componentIdQuery && (++matches == data.rulesAllCount);
							});
				}

				//! Tries to match component with component type \param componentType from the archetype \param archetype with
				//! the query. \return MatchArchetypeQueryRet::Fail if there is no match, MatchArchetypeQueryRet::Ok for match
				//! or MatchArchetypeQueryRet::Skip is not relevant.
				GAIA_NODISCARD MatchArchetypeQueryRet
				Match(const archetype::Archetype& archetype, component::ComponentType componentType) const {
					const auto& matcherHash = archetype.GetMatcherHash(componentType);
					const auto& data = GetData(componentType);

					const auto withNoneTest = matcherHash.hash & data.hash[query::ListType::LT_None].hash;
					const auto withAnyTest = matcherHash.hash & data.hash[query::ListType::LT_Any].hash;
					const auto withAllTest = matcherHash.hash & data.hash[query::ListType::LT_All].hash;

					// If withAnyTest is empty but we wanted something
					if (withAnyTest == 0 && data.hash[query::ListType::LT_Any].hash != 0)
						return MatchArchetypeQueryRet::Fail;

					// If withAllTest is empty but we wanted something
					if (withAllTest == 0 && data.hash[query::ListType::LT_All].hash != 0)
						return MatchArchetypeQueryRet::Fail;

					const auto& archetypeComponentIds = archetype.GetComponentIdArray(componentType);

					// If there is any match with withNoneList we quit
					if (withNoneTest != 0) {
						if (CheckMatchOne(componentType, archetypeComponentIds, query::ListType::LT_None))
							return MatchArchetypeQueryRet::Fail;
					}

					// If there is any match with withAnyTest
					if (withAnyTest != 0) {
						if (CheckMatchOne(componentType, archetypeComponentIds, query::ListType::LT_Any))
							goto checkWithAllMatches;

						// At least one match necessary to continue
						return MatchArchetypeQueryRet::Fail;
					}

				checkWithAllMatches:
					// If withAllList is not empty there has to be an exact match
					if (withAllTest != 0) {
						if (
								// We can't search for more components than there are no the archetype itself
								data.rulesAllCount <= archetypeComponentIds.size() &&
								// Match everything with LT_ALL
								CheckMatchAll(componentType, archetypeComponentIds))
							return MatchArchetypeQueryRet::Ok;

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

				GAIA_NODISCARD const query::LookupCtx::Data& GetData(component::ComponentType componentType) const {
					return m_lookupCtx.data[componentType];
				}

				GAIA_NODISCARD const query::ComponentIdArray& GetComponentIds(component::ComponentType componentType) const {
					return m_lookupCtx.data[componentType].componentIds;
				}

				GAIA_NODISCARD const query::ChangeFilterArray& GetFiltered(component::ComponentType componentType) const {
					return m_lookupCtx.data[componentType].withChanged;
				}

				GAIA_NODISCARD bool HasFilters() const {
					return !m_lookupCtx.data[component::ComponentType::CT_Generic].withChanged.empty() ||
								 !m_lookupCtx.data[component::ComponentType::CT_Chunk].withChanged.empty();
				}

				template <typename... T>
				bool Has() const {
					return (HasComponent_Internal<T>(query::ListType::LT_Count) || ...);
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
