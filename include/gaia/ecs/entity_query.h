#pragma once
#include <tuple>

#include "../containers/darray.h"
#include "../containers/sarray_ext.h"
#include "../utils/hashing_policy.h"
#include "../utils/utility.h"
#include "archetype.h"
#include "common.h"
#include "component.h"
#include "fwd.h"

namespace gaia {
	namespace ecs {
		class EntityQuery final {
			friend class World;

		public:
			using LookupHash = utils::direct_hash_key<uint64_t>;
			//! List type
			enum ListType : uint8_t { LT_None, LT_Any, LT_All, LT_Count };
			//! Query constraints
			enum class Constraints { EnabledOnly, DisabledOnly, AcceptAll };
			//! Query matching result
			enum class MatchArchetypeQueryRet { Fail, Ok, Skip };
			//! Number of components that can be a part of EntityQuery
			static constexpr uint32_t MAX_COMPONENTS_IN_QUERY = 8U;

		private:
			struct ComponentIndexData {
				//! Component index
				uint32_t index : 31;
				//! True for read/write, false for read-only
				uint32_t rw : 1;

				bool operator<(ComponentIndexData other) const {
					return (uint32_t)index < (uint32_t)other.index;
				}
				bool operator>(ComponentIndexData other) const {
					return (uint32_t)index > (uint32_t)other.index;
				}
			};
			//! Make sure the struct remains small and we can pass it by copy rather than const ref
			static_assert(sizeof(ComponentIndexData) == sizeof(uint32_t));

			//! Array of component type indices
			using ComponentIndexArray = containers::sarray_ext<ComponentIndexData, MAX_COMPONENTS_IN_QUERY>;
			//! Array of component type indices reserved for filtering
			using ChangeFilterArray = containers::sarray_ext<uint32_t, MAX_COMPONENTS_IN_QUERY>;

			struct ComponentListData {
				ComponentIndexArray list[ListType::LT_Count]{};
				uint64_t hash[ListType::LT_Count]{};
			};
			//! List of querried components
			ComponentListData m_list[ComponentType::CT_Count]{};
			//! List of filtered components
			ChangeFilterArray m_listChangeFiltered[ComponentType::CT_Count]{};
			//! Version of the world for which the query has been called most recently
			uint32_t m_worldVersion = 0;
			//! Entity of the last added archetype in the world this query remembers
			uint32_t m_lastArchetypeId = 1; // skip the root archetype
			//! Lookup hash for this query
			LookupHash m_hashLookup{};
			//! List of cached archetypes
			containers::darray<Archetype*> m_archetypeCache;
			//! Tell what kinds of chunks are going to be accepted by the query
			Constraints m_constraints = Constraints::EnabledOnly;
			//! If true, we need to recalculate hashes
			bool m_recalculate = true;
			//! If true, sorting infos is necessary
			bool m_sort = true;
			//! If true, all components have requested read-only access
			bool m_readOnly = true;

			template <typename T>
			bool HasComponent_Internal([[maybe_unused]] const ComponentIndexArray& arr) const {
				if constexpr (std::is_same_v<T, Entity>) {
					// Skip Entity input args
					return true;
				} else {
					const auto infoIndex = GetComponentIndex<T>();
					return utils::has_if(arr, [&](ComponentIndexData info) {
						return info.index == infoIndex;
					});
				}
			}

			template <typename T>
			void AddComponent_Internal([[maybe_unused]] ComponentIndexArray& arr) {
				if constexpr (std::is_same_v<T, Entity>) {
					// Skip Entity input args
					return;
				} else {
					const auto infoIndex = GetComponentIndex<T>();

					// Unique infos only
					const bool ret = utils::has_if(arr, [&](ComponentIndexData info) {
						return info.index == infoIndex;
					});
					if GAIA_UNLIKELY (ret)
						return;

					// Make sure the component is always registered
					(void)GetComponentCacheRW().GetOrCreateComponentInfo<T>();

#if GAIA_DEBUG
					// There's a limit to the amount of components which we can store
					if (arr.size() >= MAX_COMPONENTS_IN_QUERY) {
						GAIA_ASSERT(
								false && "Trying to create an ECS query with too many "
												 "components!");

						constexpr auto typeName = utils::type_info::name<T>();
						LOG_E(
								"Trying to add ECS component '%.*s' to an already full ECS query!", (uint32_t)typeName.size(),
								typeName.data());

						return;
					}
#endif

					constexpr bool rw = std::is_const_v<T>;
					arr.push_back({infoIndex, rw});
					if constexpr (rw)
						m_readOnly = false;

					m_recalculate = true;
					m_sort = true;

					// Any updates to components in the query invalidate the cache.
					// If possible, we should first prepare the query and not touch if afterwards.
					GAIA_ASSERT(m_archetypeCache.empty());
					m_archetypeCache.clear();
				}
			}

			template <typename T>
			void SetChangedFilter_Internal(ChangeFilterArray& arrFilter, ComponentListData& componentListData) {
				static_assert(!std::is_same_v<T, Entity>, "It doesn't make sense to use ChangedFilter with Entity");

				const auto infoIndex = utils::type_info::index<T>();

				// Unique infos only
				GAIA_ASSERT(!utils::has(arrFilter, infoIndex) && "Filter doesn't contain unique infos");

#if GAIA_DEBUG
				// There's a limit to the amount of components which we can store
				if (arrFilter.size() >= MAX_COMPONENTS_IN_QUERY) {
					GAIA_ASSERT(false && "Trying to create an ECS filter query with too many components!");

					constexpr auto typeName = utils::type_info::name<T>();
					LOG_E(
							"Trying to add ECS component %.*s to an already full filter query!", (uint32_t)typeName.size(),
							typeName.data());

					return;
				}
#endif

				// Component has to be present in anyList or allList.
				// NoneList makes no sense because we skip those in query processing anyway.
				if (utils::has_if(componentListData.list[ListType::LT_Any], [infoIndex](ComponentIndexData data) {
							return data.index == infoIndex;
						})) {
					arrFilter.push_back(infoIndex);
					return;
				}
				if (utils::has_if(componentListData.list[ListType::LT_All], [infoIndex](ComponentIndexData data) {
							return data.index == infoIndex;
						})) {
					arrFilter.push_back(infoIndex);
					return;
				}

				GAIA_ASSERT(false && "SetChangeFilter trying to filter ECS component which is not a part of the query");
#if GAIA_DEBUG
				constexpr auto typeName = utils::type_info::name<T>();
				LOG_E(
						"SetChangeFilter trying to filter ECS component %.*s but "
						"it's not a part of the query!",
						(uint32_t)typeName.size(), typeName.data());
#endif
			}

			template <typename... T>
			void SetChangedFilter(ChangeFilterArray& arr, ComponentListData& componentListData) {
				(SetChangedFilter_Internal<T>(arr, componentListData), ...);
			}

			//! Sorts internal component arrays by their type indices
			void SortComponentArrays() {
				for (auto& l: m_list) {
					for (auto& arr: l.list) {
						utils::sort(arr, [](ComponentIndexData left, ComponentIndexData right) {
							return left < right;
						});
					}
				}
			}

			void CalculateLookupHash() {
				// Sort the arrays if necessary
				if GAIA_UNLIKELY (m_sort) {
					SortComponentArrays();
					m_sort = false;
				}

				// Make sure we don't calculate the hash twice
				GAIA_ASSERT(m_hashLookup.hash == 0);

				const auto& cc = GetComponentCache();

				// Contraints
				uint64_t hashLookup = utils::hash_combine(m_hashLookup.hash, (uint64_t)m_constraints);

				// Filters
				for (size_t i = 0; i < ComponentType::CT_Count; ++i) {
					uint64_t hash = 0;

					const auto& l = m_listChangeFiltered[i];
					for (auto index: l)
						hash = utils::hash_combine(hash, (uint64_t)index);
					hash = utils::hash_combine(hash, (uint64_t)l.size());

					hashLookup = utils::hash_combine(hashLookup, hash);
				}

				// Components
				for (size_t i = 0; i < ComponentType::CT_Count; ++i) {
					uint64_t hash = 0;

					const auto& l = m_list[i];
					for (const auto& components: l.list) {
						for (auto data: components) {
							const auto* info = cc.GetComponentInfoFromIdx(data.index);
							hash = utils::hash_combine(hash, info->lookupHash);
						}
						hash = utils::hash_combine(hash, (uint64_t)components.size());
					}

					hashLookup = utils::hash_combine(hashLookup, hash);
				}

				m_hashLookup = {utils::calculate_hash64(hashLookup)};
			}

			void CalculateMatcherHashes() {
				if GAIA_LIKELY (!m_recalculate)
					return;
				m_recalculate = false;

				// Sort the arrays if necessary
				if GAIA_UNLIKELY (m_sort) {
					m_sort = false;
					SortComponentArrays();
				}

				// Calculate the matcher hash
				{
					const auto& cc = GetComponentCache();

					for (auto& l: m_list) {
						for (size_t i = 0; i < ListType::LT_Count; ++i) {
							auto& arr = l.list[i];

							if (!arr.empty()) {
								const auto* info = cc.GetComponentInfoFromIdx(arr[0].index);
								l.hash[i] = info->matcherHash;
							}
							for (size_t j = 1; j < arr.size(); ++j) {
								const auto* info = cc.GetComponentInfoFromIdx(arr[j].index);
								l.hash[i] = utils::combine_or(l.hash[i], info->matcherHash);
							}
						}
					}
				}
			}

			/*!
				Tries to match a component query index from \param queryList against those in \param componentInfos.
				\return True if there is a match, false otherwise.
				*/
			static GAIA_NODISCARD bool
			CheckMatchOne(const ComponentInfoList& componentInfos, const ComponentIndexArray& queryList) {
				for (const auto infoQuery: queryList) {
					for (const auto& info: componentInfos) {
						if (info->infoIndex == infoQuery.index) {
							return true;
						}
					}
				}

				return false;
			}

			/*!
				Tries to match all component query indices from \param queryList against those in \param componentInfos.
				\return True if there is a match, false otherwise.
				*/
			static GAIA_NODISCARD bool
			CheckMatchMany(const ComponentInfoList& componentInfos, const ComponentIndexArray& queryList) {
				size_t matches = 0;

				for (const auto infoQuery: queryList) {
					for (const auto& info: componentInfos) {
						if (info->infoIndex == infoQuery.index) {
							if (++matches == queryList.size())
								return true;

							break;
						}
					}
				}

				return false;
			}

			/*!
				Tries to match \param componentInfos with a given \param matcherHash.
				\return MatchArchetypeQueryRet::Fail if there is no match, MatchArchetypeQueryRet::Ok for match or
								MatchArchetypeQueryRet::Skip is not relevant.
				*/
			template <ComponentType TComponentType>
			GAIA_NODISCARD MatchArchetypeQueryRet Match(const ComponentInfoList& componentInfos, uint64_t matcherHash) const {
				const auto& queryList = GetData(TComponentType);
				const uint64_t withNoneTest = matcherHash & queryList.hash[ListType::LT_None];
				const uint64_t withAnyTest = matcherHash & queryList.hash[ListType::LT_Any];
				const uint64_t withAllTest = matcherHash & queryList.hash[ListType::LT_All];

				// If withAllTest is empty but we wanted something
				if (!withAllTest && queryList.hash[ListType::LT_All] != 0)
					return MatchArchetypeQueryRet::Fail;

				// If withAnyTest is empty but we wanted something
				if (!withAnyTest && queryList.hash[ListType::LT_Any] != 0)
					return MatchArchetypeQueryRet::Fail;

				// If there is any match with withNoneList we quit
				if (withNoneTest != 0) {
					if (CheckMatchOne(componentInfos, queryList.list[ListType::LT_None]))
						return MatchArchetypeQueryRet::Fail;
				}

				// If there is any match with withAnyTest
				if (withAnyTest != 0) {
					if (CheckMatchOne(componentInfos, queryList.list[ListType::LT_Any]))
						goto checkWithAllMatches;

					// At least one match necessary to continue
					return MatchArchetypeQueryRet::Fail;
				}

			checkWithAllMatches:
				// If withAllList is not empty there has to be an exact match
				if (withAllTest != 0) {
					// If the number of queried components is greater than the
					// number of components in archetype there's no need to search
					if (queryList.list[ListType::LT_All].size() <= componentInfos.size()) {
						// m_list[ListType::LT_All] first because we usually request for less
						// components than there are components in archetype
						if (CheckMatchMany(componentInfos, queryList.list[ListType::LT_All]))
							return MatchArchetypeQueryRet::Ok;
					}

					// No match found. We're done
					return MatchArchetypeQueryRet::Fail;
				}

				return (withAnyTest != 0) ? MatchArchetypeQueryRet::Ok : MatchArchetypeQueryRet::Skip;
			}

			GAIA_NODISCARD bool operator==(const EntityQuery& other) const {
				// Lookup hash must match
				if (m_hashLookup != other.m_hashLookup)
					return false;

				if (m_constraints != other.m_constraints)
					return false;

				for (size_t j = 0; j < ComponentType::CT_Count; ++j) {
					const auto& queryList = m_list[j];
					const auto& otherList = other.m_list[j];

					// Component count needes to be the same
					for (size_t i = 0; i < ListType::LT_Count; ++i) {
						if (queryList.list[i].size() != otherList.list[i].size())
							return false;
					}

					// Matches hashes need to be the same
					for (size_t i = 0; i < ListType::LT_Count; ++i) {
						if (queryList.hash[i] != otherList.hash[i])
							return false;
					}

					// Filter count needs to be the same
					if (m_listChangeFiltered[j].size() != other.m_listChangeFiltered[j].size())
						return false;

					// Components need to be the same
					for (size_t i = 0; i < ListType::LT_Count; ++i) {
						const auto ret = std::memcmp(
								(const void*)&queryList.list[i], (const void*)&otherList.list[i],
								queryList.list[i].size() * sizeof(queryList.list[0]));
						if (ret != 0)
							return false;
					}

					// Filters need to be the same
					{
						const auto ret = std::memcmp(
								(const void*)&m_listChangeFiltered[j], (const void*)&other.m_listChangeFiltered[j],
								m_listChangeFiltered[j].size() * sizeof(m_listChangeFiltered[0]));
						if (ret != 0)
							return false;
					}
				}

				return true;
			}

			GAIA_NODISCARD bool operator!=(const EntityQuery& other) const {
				return !operator==(other);
			}

			/*!
			Tries to match the query against \param archetypes. For each matched archetype the archetype is cached.
			This is necessary so we do not iterate all chunks over and over again when running queries.
			*/
			void Match(const containers::darray<Archetype*>& archetypes) {
				if GAIA_UNLIKELY (m_recalculate && !archetypes.empty())
					CalculateMatcherHashes();

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
					if (retGeneric == EntityQuery::MatchArchetypeQueryRet::Fail)
						continue;

					// Early exit if chunk query doesn't match
					const auto retChunk = Match<ComponentType::CT_Chunk>(
							GetArchetypeComponentInfoList(archetype, ComponentType::CT_Chunk),
							GetArchetypeMatcherHash(archetype, ComponentType::CT_Chunk));
					if (retChunk == EntityQuery::MatchArchetypeQueryRet::Fail)
						continue;

					// If at least one query succeeded run our logic
					if (retGeneric == EntityQuery::MatchArchetypeQueryRet::Ok ||
							retChunk == EntityQuery::MatchArchetypeQueryRet::Ok)
						m_archetypeCache.push_back(pArchetype);
				}

				m_lastArchetypeId = (uint32_t)archetypes.size();
			}

			void SetWorldVersion(uint32_t worldVersion) {
				m_worldVersion = worldVersion;
			}

			template <typename T>
			EntityQuery& AddComponent_Internal(ListType listType) {
				using U = typename DeduceComponent<T>::Type;
				if constexpr (IsGenericComponent<T>)
					AddComponent_Internal<U>(m_list[ComponentType::CT_Generic].list[listType]);
				else
					AddComponent_Internal<U>(m_list[ComponentType::CT_Chunk].list[listType]);
				return *this;
			}

			template <typename T>
			bool HasComponent_Internal(ListType listType) const {
				using U = typename DeduceComponent<T>::Type;
				if constexpr (IsGenericComponent<T>)
					return HasComponent_Internal<U>(m_list[ComponentType::CT_Generic].list[listType]);
				else
					return HasComponent_Internal<U>(m_list[ComponentType::CT_Chunk].list[listType]);
			}

			template <typename T>
			EntityQuery& WithChanged_Internal() {
				using U = typename DeduceComponent<T>::Type;
				if constexpr (IsGenericComponent<T>)
					SetChangedFilter<U>(m_listChangeFiltered[ComponentType::CT_Generic], m_list[ComponentType::CT_Generic]);
				else
					SetChangedFilter<U>(m_listChangeFiltered[ComponentType::CT_Chunk], m_list[ComponentType::CT_Chunk]);
				return *this;
			}

		public:
			GAIA_NODISCARD const ComponentListData& GetData(ComponentType type) const {
				return m_list[type];
			}

			GAIA_NODISCARD const ChangeFilterArray& GetFiltered(ComponentType type) const {
				return m_listChangeFiltered[type];
			}

			EntityQuery& SetConstraints(Constraints value) {
				m_constraints = value;
				return *this;
			}

			GAIA_NODISCARD Constraints GetConstraints() const {
				return m_constraints;
			}

			template <bool Enabled>
			GAIA_NODISCARD bool CheckConstraints() const {
				if GAIA_LIKELY (m_constraints == Constraints::AcceptAll)
					return true;
				if constexpr (Enabled)
					if (m_constraints == Constraints::EnabledOnly)
						return true;
				if constexpr (!Enabled)
					if (m_constraints == Constraints::DisabledOnly)
						return true;
				return false;
			}

			GAIA_NODISCARD bool CheckConstraints(bool enabled) const {
				if GAIA_LIKELY (m_constraints == Constraints::AcceptAll)
					return true;
				if (enabled && m_constraints == Constraints::EnabledOnly)
					return true;
				if (!enabled && m_constraints == Constraints::DisabledOnly)
					return true;
				return false;
			}

			GAIA_NODISCARD bool HasFilters() const {
				return !m_listChangeFiltered[ComponentType::CT_Generic].empty() ||
							 !m_listChangeFiltered[ComponentType::CT_Chunk].empty();
			}

			template <typename... T>
			GAIA_FORCEINLINE EntityQuery& Any() {
				(AddComponent_Internal<T>(ListType::LT_Any), ...);
				return *this;
			}

			template <typename... T>
			GAIA_FORCEINLINE EntityQuery& All() {
				(AddComponent_Internal<T>(ListType::LT_All), ...);
				return *this;
			}

			template <typename... T>
			GAIA_FORCEINLINE EntityQuery& None() {
				(AddComponent_Internal<T>(ListType::LT_None), ...);
				return *this;
			}

			template <typename... T>
			bool HasAny() const {
				return (HasComponent_Internal<T>(ListType::LT_Any) || ...);
			}

			template <typename... T>
			bool HasAll() const {
				return (HasComponent_Internal<T>(ListType::LT_All) && ...);
			}

			template <typename... T>
			bool HasNone() const {
				return (!HasComponent_Internal<T>(ListType::LT_None) && ...);
			}

			template <typename... T>
			GAIA_FORCEINLINE EntityQuery& WithChanged() {
				(WithChanged_Internal<T>(), ...);
				return *this;
			}

			GAIA_NODISCARD uint32_t GetWorldVersion() const {
				return m_worldVersion;
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
