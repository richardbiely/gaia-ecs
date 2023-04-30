#pragma once
#include <tuple>
#include <type_traits>

#include "../config/profiler.h"
#include "../containers/darray.h"
#include "../containers/sarray_ext.h"
#include "../utils/hashing_policy.h"
#include "../utils/utility.h"
#include "archetype.h"
#include "common.h"
#include "component.h"
#include "component_utils.h"
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
			enum class Constraints : uint8_t { EnabledOnly, DisabledOnly, AcceptAll };
			//! Query matching result
			enum class MatchArchetypeQueryRet : uint8_t { Fail, Ok, Skip };
			//! Number of components that can be a part of EntityQuery
			static constexpr uint32_t MAX_COMPONENTS_IN_QUERY = 8U;

		private:
			//! Array of component ids
			using ComponentIdArray = containers::sarray_ext<ComponentId, MAX_COMPONENTS_IN_QUERY>;
			//! Array of component ids reserved for filtering
			using ChangeFilterArray = containers::sarray_ext<ComponentId, MAX_COMPONENTS_IN_QUERY>;

			struct ComponentListData {
				//! List of component ids
				ComponentIdArray componentIds[ListType::LT_Count]{};
				//! List of component matcher hashes
				ComponentMatcherHash hash[ListType::LT_Count]{};
			};
			//! List of querried components
			ComponentListData m_list[ComponentType::CT_Count]{};
			//! List of filtered components
			ChangeFilterArray m_listChangeFiltered[ComponentType::CT_Count]{};
			//! Lookup hash for this query
			LookupHash m_hashLookup{};
			//! List of cached archetypes
			containers::darray<Archetype*> m_archetypeCache;
			//! Version of the world for which the query has been called most recently
			uint32_t m_worldVersion = 0;
			//! Entity of the last added archetype in the world this query remembers
			uint32_t m_lastArchetypeId = 1; // skip the root archetype
			//! Read-write mask. Bit 0 stands for component 0 in component arrays.
			//! A set bit means write access is requested.
			uint8_t m_rw[ComponentType::CT_Count]{};
			static_assert(MAX_COMPONENTS_IN_QUERY == 8); // Make sure that MAX_COMPONENTS_IN_QUERY can fit into m_rw
			//! Tell what kinds of chunks are going to be accepted by the query
			Constraints m_constraints = Constraints::EnabledOnly;
			//! If true, we need to recalculate hashes
			bool m_recalculate = true;
			//! If true, sorting infos is necessary
			bool m_sort = true;

			template <typename T>
			bool HasComponent_Internal(
					[[maybe_unused]] ListType listType, [[maybe_unused]] ComponentType componentType, bool isReadWrite) const {
				if constexpr (std::is_same_v<T, Entity>) {
					// Skip Entity input args
					return true;
				} else {
					const ComponentIdArray& componentIds = m_list[componentType].componentIds[listType];

					// Component id has to be present
					const auto componentId = GetComponentId<T>();
					const auto idx = utils::get_index(componentIds, componentId);
					if (idx == BadIndex)
						return false;

					// Read-write mask must match
					const uint8_t maskRW = (uint32_t)m_rw[componentType] & (1U << (uint32_t)idx);
					const uint8_t maskXX = (uint32_t)isReadWrite << idx;
					return maskRW == maskXX;
				}
			}

			template <typename T>
			void AddComponent_Internal(
					[[maybe_unused]] ListType listType, [[maybe_unused]] ComponentType componentType,
					[[maybe_unused]] bool isReadWrite) {
				if constexpr (std::is_same_v<T, Entity>) {
					// Skip Entity input args
					return;
				} else {
					const auto componentId = GetComponentId<T>();
					auto& list = m_list[componentType];
					auto& componentIds = list.componentIds[listType];

					// Unique infos only
					const bool ret = utils::has(componentIds, componentId);
					if GAIA_UNLIKELY (ret)
						return;

					// Make sure the component is always registered
					(void)GetComponentCacheRW().GetOrCreateComponentInfo<T>();

#if GAIA_DEBUG
					// There's a limit to the amount of components which we can store
					if (componentIds.size() >= MAX_COMPONENTS_IN_QUERY) {
						GAIA_ASSERT(false && "Trying to create an ECS query with too many components!");

						constexpr auto typeName = utils::type_info::name<T>();
						GAIA_LOG_E(
								"Trying to add ECS component '%.*s' to an already full ECS query!", (uint32_t)typeName.size(),
								typeName.data());

						return;
					}
#endif

					m_rw[componentType] |= (uint8_t)isReadWrite << (uint8_t)componentIds.size();

					componentIds.push_back(componentId);

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

				const auto componentId = utils::type_info::id<T>();

				// Unique infos only
				GAIA_ASSERT(!utils::has(arrFilter, componentId) && "Filter doesn't contain unique infos");

#if GAIA_DEBUG
				// There's a limit to the amount of components which we can store
				if (arrFilter.size() >= MAX_COMPONENTS_IN_QUERY) {
					GAIA_ASSERT(false && "Trying to create an ECS filter query with too many components!");

					constexpr auto typeName = utils::type_info::name<T>();
					GAIA_LOG_E(
							"Trying to add ECS component %.*s to an already full filter query!", (uint32_t)typeName.size(),
							typeName.data());

					return;
				}
#endif

				// Component has to be present in anyList or allList.
				// NoneList makes no sense because we skip those in query processing anyway.
				if (utils::has(componentListData.componentIds[ListType::LT_Any], componentId)) {
					arrFilter.push_back(componentId);
					return;
				}
				if (utils::has(componentListData.componentIds[ListType::LT_All], componentId)) {
					arrFilter.push_back(componentId);
					return;
				}

				GAIA_ASSERT(false && "SetChangeFilter trying to filter ECS component which is not a part of the query");
#if GAIA_DEBUG
				constexpr auto typeName = utils::type_info::name<T>();
				GAIA_LOG_E(
						"SetChangeFilter trying to filter ECS component %.*s but "
						"it's not a part of the query!",
						(uint32_t)typeName.size(), typeName.data());
#endif
			}

			template <typename... T>
			void SetChangedFilter(ChangeFilterArray& arr, ComponentListData& componentListData) {
				(SetChangedFilter_Internal<T>(arr, componentListData), ...);
			}

			//! Sorts internal component arrays
			void SortComponentArrays() {
				for (size_t i = 0; i < ComponentType::CT_Count; ++i) {
					auto& l = m_list[i];
					for (auto& componentIds: l.componentIds) {
						// Make sure the read-write mask remains correct after sorting
						utils::sort(componentIds, SortComponentCond{}, [&](size_t left, size_t right) {
							// Swap component ids
							utils::swap(componentIds[left], componentIds[right]);

							// Swap the bits in the read-write mask
							const uint32_t b0 = m_rw[i] & (1U << left);
							const uint32_t b1 = m_rw[i] & (1U << right);
							// XOR the two bits
							const uint32_t bxor = (b0 ^ b1);
							// Put the XOR bits back to their original positions
							const uint32_t mask = (bxor << left) | (bxor << right);
							// XOR mask with the original one effectivelly swapping the bits
							m_rw[i] = m_rw[i] ^ (uint8_t)mask;
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

				// Contraints
				LookupHash::Type hashLookup = utils::hash_combine(m_hashLookup.hash, (uint64_t)m_constraints);

				// Filters
				for (size_t i = 0; i < ComponentType::CT_Count; ++i) {
					LookupHash::Type hash = 0;

					const auto& l = m_listChangeFiltered[i];
					for (auto componentId: l)
						hash = utils::hash_combine(hash, (LookupHash::Type)componentId);
					hash = utils::hash_combine(hash, (LookupHash::Type)l.size());

					hashLookup = utils::hash_combine(hashLookup, hash);
				}

				// Components
				for (size_t i = 0; i < ComponentType::CT_Count; ++i) {
					LookupHash::Type hash = 0;

					const auto& l = m_list[i];
					for (const auto& componentIds: l.componentIds) {
						for (const auto componentId: componentIds) {
							hash = utils::hash_combine(hash, (LookupHash::Type)componentId);
						}
						hash = utils::hash_combine(hash, (LookupHash::Type)componentIds.size());
					}

					hash = utils::hash_combine(hash, (LookupHash::Type)m_rw[i]);
					hashLookup = utils::hash_combine(hashLookup, hash);
				}

				m_hashLookup = {utils::calculate_hash64(hashLookup)};
			}

			void CalculateMatcherHashes() {
				if GAIA_LIKELY (!m_recalculate)
					return;

				GAIA_PROF_SCOPE(CalculateMatcherHashes);

				m_recalculate = false;

				// Sort the arrays if necessary
				if GAIA_UNLIKELY (m_sort) {
					m_sort = false;
					SortComponentArrays();
				}

				// Calculate the matcher hash
				for (auto& l: m_list) {
					for (size_t i = 0; i < ListType::LT_Count; ++i)
						l.hash[i] = CalculateMatcherHash(l.componentIds[i]);
				}
			}

			/*!
				Tries to match component ids in \param componentIdsQuery with those in \param componentIds.
				\return True if there is a match, false otherwise.
				*/
			static GAIA_NODISCARD bool
			CheckMatchOne(const ComponentIdList& componentIds, const ComponentIdArray& componentIdsQuery) {
				for (const auto componentId: componentIdsQuery) {
					if (utils::has(componentIds, componentId))
						return true;
				}

				return false;
			}

			/*!
				Tries to match all component ids in \param componentIdsQuery with those in \param componentIds.
				\return True if there is a match, false otherwise.
				*/
			static GAIA_NODISCARD bool
			CheckMatchMany(const ComponentIdList& componentIds, const ComponentIdArray& componentIdsQuery) {
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

			/*!
				Tries to match \param componentIds with a given \param matcherHash.
				\return MatchArchetypeQueryRet::Fail if there is no match, MatchArchetypeQueryRet::Ok for match or
								MatchArchetypeQueryRet::Skip is not relevant.
				*/
			template <ComponentType TComponentType>
			GAIA_NODISCARD MatchArchetypeQueryRet
			Match(const ComponentIdList& componentIds, ComponentMatcherHash matcherHash) const {
				const auto& queryList = GetData(TComponentType);
				const auto withNoneTest = matcherHash.hash & queryList.hash[ListType::LT_None].hash;
				const auto withAnyTest = matcherHash.hash & queryList.hash[ListType::LT_Any].hash;
				const auto withAllTest = matcherHash.hash & queryList.hash[ListType::LT_All].hash;

				// If withAllTest is empty but we wanted something
				if (!withAllTest && queryList.hash[ListType::LT_All].hash != 0)
					return MatchArchetypeQueryRet::Fail;

				// If withAnyTest is empty but we wanted something
				if (!withAnyTest && queryList.hash[ListType::LT_Any].hash != 0)
					return MatchArchetypeQueryRet::Fail;

				// If there is any match with withNoneList we quit
				if (withNoneTest != 0) {
					if (CheckMatchOne(componentIds, queryList.componentIds[ListType::LT_None]))
						return MatchArchetypeQueryRet::Fail;
				}

				// If there is any match with withAnyTest
				if (withAnyTest != 0) {
					if (CheckMatchOne(componentIds, queryList.componentIds[ListType::LT_Any]))
						goto checkWithAllMatches;

					// At least one match necessary to continue
					return MatchArchetypeQueryRet::Fail;
				}

			checkWithAllMatches:
				// If withAllList is not empty there has to be an exact match
				if (withAllTest != 0) {
					// If the number of queried components is greater than the
					// number of components in archetype there's no need to search
					if (queryList.componentIds[ListType::LT_All].size() <= componentIds.size()) {
						// m_list[ListType::LT_All] first because we usually request for less
						// components than there are components in archetype
						if (CheckMatchMany(componentIds, queryList.componentIds[ListType::LT_All]))
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
					// Read-write access has to be the same
					if (m_rw[j] != other.m_rw[j])
						return false;

					// Filter count needs to be the same
					if (m_listChangeFiltered[j].size() != other.m_listChangeFiltered[j].size())
						return false;

					const auto& queryList = m_list[j];
					const auto& otherList = other.m_list[j];

					// Component count needes to be the same
					for (size_t i = 0; i < ListType::LT_Count; ++i) {
						if (queryList.componentIds[i].size() != otherList.componentIds[i].size())
							return false;
					}

					// Matches hashes need to be the same
					for (size_t i = 0; i < ListType::LT_Count; ++i) {
						if (queryList.hash[i] != otherList.hash[i])
							return false;
					}

					// Components need to be the same
					for (size_t i = 0; i < ListType::LT_Count; ++i) {
						const auto ret = std::memcmp(
								(const void*)&queryList.componentIds[i], (const void*)&otherList.componentIds[i],
								queryList.componentIds[i].size() * sizeof(queryList.componentIds[0]));
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
			void AddComponent_Internal(ListType listType) {
				using U = typename DeduceComponent<T>::Type;
				using UOriginal = typename DeduceComponent<T>::TypeOriginal;
				using UOriginalPR = std::remove_reference_t<std::remove_pointer_t<UOriginal>>;
				constexpr bool isReadWrite = std::is_same_v<U, UOriginal> || !std::is_const_v<UOriginalPR>;

				if constexpr (IsGenericComponent<T>)
					AddComponent_Internal<U>(listType, ComponentType::CT_Generic, isReadWrite);
				else
					AddComponent_Internal<U>(listType, ComponentType::CT_Chunk, isReadWrite);
			}

			template <typename T>
			bool HasComponent_Internal(ListType listType) const {
				using U = typename DeduceComponent<T>::Type;
				using UOriginal = typename DeduceComponent<T>::TypeOriginal;
				using UOriginalPR = std::remove_reference_t<std::remove_pointer_t<UOriginal>>;
				constexpr bool isReadWrite = std::is_same_v<U, UOriginal> || !std::is_const_v<UOriginalPR>;

				if constexpr (IsGenericComponent<T>)
					return HasComponent_Internal<U>(listType, ComponentType::CT_Generic, isReadWrite);
				else
					return HasComponent_Internal<U>(listType, ComponentType::CT_Chunk, isReadWrite);
			}

			template <typename T>
			void WithChanged_Internal() {
				using U = typename DeduceComponent<T>::Type;
				if constexpr (IsGenericComponent<T>)
					SetChangedFilter<U>(m_listChangeFiltered[ComponentType::CT_Generic], m_list[ComponentType::CT_Generic]);
				else
					SetChangedFilter<U>(m_listChangeFiltered[ComponentType::CT_Chunk], m_list[ComponentType::CT_Chunk]);
			}

		public:
			GAIA_NODISCARD const ComponentListData& GetData(ComponentType componentType) const {
				return m_list[componentType];
			}

			GAIA_NODISCARD const ChangeFilterArray& GetFiltered(ComponentType componentType) const {
				return m_listChangeFiltered[componentType];
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
				// By default we only evaluate EnabledOnly changes. AcceptAll is something that has to be asked for
				// explicitely.
				if GAIA_UNLIKELY (m_constraints == Constraints::AcceptAll)
					return true;

				if constexpr (Enabled)
					return m_constraints == Constraints::EnabledOnly;
				else
					return m_constraints == Constraints::DisabledOnly;
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
