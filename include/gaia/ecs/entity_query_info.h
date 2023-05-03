#pragma once
#include "../containers/darray.h"
#include "../containers/sarray_ext.h"
#include "../utils/hashing_policy.h"
#include "../utils/utility.h"
#include "component.h"
#include "component_utils.h"
#include "entity_query_common.h"

namespace gaia {
	namespace ecs {
		struct Entity;
		class Archetype;

		const ComponentIdList& GetArchetypeComponentInfoList(const Archetype& archetype, ComponentType componentType);
		ComponentMatcherHash GetArchetypeMatcherHash(const Archetype& archetype, ComponentType componentType);

		class EntityQueryInfo {
		public:
			//! Query matching result
			enum class MatchArchetypeQueryRet : uint8_t { Fail, Ok, Skip };

		private:
			//! Lookup context
			query::LookupCtx m_lookupCtx;
			//! List of archetypes matching the query
			containers::darray<Archetype*> m_archetypeCache;
			//! Entity of the last added archetype in the world this query remembers
			uint32_t m_lastArchetypeId = 1; // skip the root archetype
			//! Version of the world for which the query has been called most recently
			uint32_t m_worldVersion = 0;

			template <typename T>
			bool HasComponent_Internal(
					[[maybe_unused]] query::ListType listType, [[maybe_unused]] ComponentType componentType,
					bool isReadWrite) const {
				if constexpr (std::is_same_v<T, Entity>) {
					// Skip Entity input args
					return true;
				} else {
					const query::ComponentIdArray& componentIds = m_lookupCtx.list[componentType].componentIds[listType];

					// Component id has to be present
					const auto componentId = GetComponentId<T>();
					const auto idx = utils::get_index(componentIds, componentId);
					if (idx == BadIndex)
						return false;

					// Read-write mask must match
					const uint8_t maskRW = (uint32_t)m_lookupCtx.rw[componentType] & (1U << (uint32_t)idx);
					const uint8_t maskXX = (uint32_t)isReadWrite << idx;
					return maskRW == maskXX;
				}
			}

			//! Tries to match component ids in \param componentIdsQuery with those in \param componentIds.
			//! \return True if there is a match, false otherwise.
			static GAIA_NODISCARD bool
			CheckMatchOne(const ComponentIdList& componentIds, const query::ComponentIdArray& componentIdsQuery) {
				for (const auto componentId: componentIdsQuery) {
					if (utils::has(componentIds, componentId))
						return true;
				}

				return false;
			}

			//! Tries to match all component ids in \param componentIdsQuery with those in \param componentIds.
			//! \return True if there is a match, false otherwise.
			static GAIA_NODISCARD bool
			CheckMatchMany(const ComponentIdList& componentIds, const query::ComponentIdArray& componentIdsQuery) {
				size_t matches = 0;

				for (const auto componentIdQuery: componentIdsQuery) {
					for (const auto componentId: componentIds) {
						if (componentId == componentIdQuery) {
							if (++matches == componentIdsQuery.size())
								return true;

							break;
						}
					}
				}

				return false;
			}

			//! Tries to match \param componentIds with a given \param matcherHash.
			//! \return MatchArchetypeQueryRet::Fail if there is no match, MatchArchetypeQueryRet::Ok for match or
			//! MatchArchetypeQueryRet::Skip is not relevant.
			template <ComponentType TComponentType>
			GAIA_NODISCARD MatchArchetypeQueryRet
			Match(const ComponentIdList& componentIds, ComponentMatcherHash matcherHash) const {
				const auto& queryList = GetData(TComponentType);
				const auto withNoneTest = matcherHash.hash & queryList.hash[query::ListType::LT_None].hash;
				const auto withAnyTest = matcherHash.hash & queryList.hash[query::ListType::LT_Any].hash;
				const auto withAllTest = matcherHash.hash & queryList.hash[query::ListType::LT_All].hash;

				// If withAllTest is empty but we wanted something
				if (!withAllTest && queryList.hash[query::ListType::LT_All].hash != 0)
					return MatchArchetypeQueryRet::Fail;

				// If withAnyTest is empty but we wanted something
				if (!withAnyTest && queryList.hash[query::ListType::LT_Any].hash != 0)
					return MatchArchetypeQueryRet::Fail;

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
			static GAIA_NODISCARD EntityQueryInfo Create(query::LookupCtx&& ctx) {
				EntityQueryInfo info;
				query::CalculateMatcherHashes(ctx);
				info.m_lookupCtx = std::move(ctx);
				return info;
			}

			void Init(uint32_t id) {
				GAIA_ASSERT(m_lookupCtx.cacheId == (uint32_t)-1);
				m_lookupCtx.cacheId = id;
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

			uint32_t GetCacheId() const {
				return m_lookupCtx.cacheId;
			}

			GAIA_NODISCARD bool operator==(const query::LookupCtx& other) const {
				return m_lookupCtx == other;
			}

			GAIA_NODISCARD bool operator!=(const query::LookupCtx& other) const {
				return !operator==(other);
			}

			//! Tries to match the query against \param archetypes. For each matched archetype the archetype is cached.
			//! This is necessary so we do not iterate all chunks over and over again when running queries.
			void Match(const containers::darray<Archetype*>& archetypes) {
				for (size_t i = m_lastArchetypeId; i < archetypes.size(); i++) {
					auto* pArchetype = archetypes[i];
#if GAIA_DEBUG
					auto& archetype = *pArchetype;
#else
					const auto& archetype = *pArchetype;
#endif

					// Early exit if generic query doesn't match
					const auto retGeneric = Match<ComponentType::CT_Generic>(
							GetArchetypeComponentInfoList(archetype, ComponentType::CT_Generic),
							GetArchetypeMatcherHash(archetype, ComponentType::CT_Generic));
					if (retGeneric == MatchArchetypeQueryRet::Fail)
						continue;

					// Early exit if chunk query doesn't match
					const auto retChunk = Match<ComponentType::CT_Chunk>(
							GetArchetypeComponentInfoList(archetype, ComponentType::CT_Chunk),
							GetArchetypeMatcherHash(archetype, ComponentType::CT_Chunk));
					if (retChunk == MatchArchetypeQueryRet::Fail)
						continue;

					// If at least one query succeeded run our logic
					if (retGeneric == MatchArchetypeQueryRet::Ok || retChunk == MatchArchetypeQueryRet::Ok)
						m_archetypeCache.push_back(pArchetype);
				}

				m_lastArchetypeId = (uint32_t)archetypes.size();
			}

			template <typename T>
			bool HasComponent_Internal(query::ListType listType) const {
				using U = typename DeduceComponent<T>::Type;
				using UOriginal = typename DeduceComponent<T>::TypeOriginal;
				using UOriginalPR = std::remove_reference_t<std::remove_pointer_t<UOriginal>>;
				constexpr bool isReadWrite =
						std::is_same_v<U, UOriginal> || !std::is_const_v<UOriginalPR> && !std::is_empty_v<U>;

				if constexpr (IsGenericComponent<T>)
					return HasComponent_Internal<U>(listType, ComponentType::CT_Generic, isReadWrite);
				else
					return HasComponent_Internal<U>(listType, ComponentType::CT_Chunk, isReadWrite);
			}

		public:
			GAIA_NODISCARD const query::ComponentListData& GetData(ComponentType componentType) const {
				return m_lookupCtx.list[componentType];
			}

			GAIA_NODISCARD const query::ChangeFilterArray& GetFiltered(ComponentType componentType) const {
				return m_lookupCtx.listChangeFiltered[componentType];
			}

			GAIA_NODISCARD bool HasFilters() const {
				return !m_lookupCtx.listChangeFiltered[ComponentType::CT_Generic].empty() ||
							 !m_lookupCtx.listChangeFiltered[ComponentType::CT_Chunk].empty();
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

			GAIA_NODISCARD containers::darray<Archetype*>::iterator begin() {
				return m_archetypeCache.begin();
			}

			GAIA_NODISCARD containers::darray<Archetype*>::const_iterator begin() const {
				return m_archetypeCache.cbegin();
			}

			GAIA_NODISCARD containers::darray<Archetype*>::iterator end() {
				return m_archetypeCache.end();
			}

			GAIA_NODISCARD containers::darray<Archetype*>::const_iterator end() const {
				return m_archetypeCache.cend();
			}
		};
	} // namespace ecs
} // namespace gaia