#pragma once
#include <algorithm>
#include <tuple>

#include "../containers/darray.h"
#include "../containers/sarray_ext.h"
#include "../utils/containers.h"
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
			//! List type
			enum ListType : uint8_t { LT_None, LT_Any, LT_All, LT_Count };
			//! Query constraints
			enum class Constraints { EnabledOnly, DisabledOnly, AcceptAll };
			//! Query matching result
			enum class MatchArchetypeQueryRet { Fail, Ok, Skip };
			//! Number of components that can be a part of EntityQuery
			static constexpr uint32_t MAX_COMPONENTS_IN_QUERY = 8u;

		private:
			//! Array of component type indices
			using ComponentIndexArray = containers::sarray_ext<uint32_t, MAX_COMPONENTS_IN_QUERY>;
			//! Array of component type indices reserved for filtering
			using ChangeFilterArray = ComponentIndexArray;

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
			uint32_t m_lastArchetypeId = 0;
			//! Lookup hash for this query
			uint64_t m_hashLookup = 0;
			//! List of cached archetypes
			containers::darray<Archetype*> m_archetypeCache;
			//! Tell what kinds of chunks are going to be accepted by the query
			Constraints m_constraints = Constraints::EnabledOnly;
			//! If true, we need to recalculate hashes
			bool m_recalculate = true;
			//! If true, sorting types is necessary
			bool m_sort = true;

			template <class TComponent>
			void AddComponent_Internal([[maybe_unused]] ComponentIndexArray& arr) {
				using T = std::decay_t<TComponent>;

				if constexpr (std::is_same<T, Entity>::value) {
					// Skip Entity input args
					return;
				} else {
					const auto typeIndex = utils::type_info::index<T>();

					// Unique types only
					if (utils::has(arr, typeIndex))
						return;

#if GAIA_DEBUG
					// There's a limit to the amount of components which we can store
					if (arr.size() >= MAX_COMPONENTS_IN_QUERY) {
						GAIA_ASSERT(
								false && "Trying to create an ECS query with too many "
												 "components!");

						constexpr auto typeName = utils::type_info::name<T>();
						LOG_E(
								"Trying to add ECS component '%.*s' to an already full ECS query!", (uint32_t)typeName.length(),
								typeName.data());

						return;
					}
#endif

					arr.push_back(typeIndex);
					m_recalculate = true;
					m_sort = true;
				}
			}

			template <typename TComponent>
			void SetChangedFilter_Internal(ChangeFilterArray& arrFilter, ComponentListData& arrMeta) {
				using T = std::decay_t<TComponent>;
				static_assert(!std::is_same<T, Entity>::value, "It doesn't make sense to use ChangedFilter with Entity");

				const auto typeIndex = utils::type_info::index<T>();

				// Unique types only
				GAIA_ASSERT(!utils::has(arrFilter, typeIndex) && "Filter doesn't contain unique types");

#if GAIA_DEBUG
				// There's a limit to the amount of components which we can store
				if (arrFilter.size() >= MAX_COMPONENTS_IN_QUERY) {
					GAIA_ASSERT(false && "Trying to create an ECS filter query with too many components!");

					constexpr auto typeName = utils::type_info::name<T>();
					LOG_E(
							"Trying to add ECS component %.*s to an already full filter query!", (uint32_t)typeName.length(),
							typeName.data());

					return;
				}
#endif

				// Component has to be present in anyList or allList.
				// NoneList makes no sense because we skip those in query processing anyway.
				if (utils::has_if(arrMeta.list[ListType::LT_Any], [typeIndex](auto idx) {
							return idx == typeIndex;
						})) {
					arrFilter.push_back(typeIndex);
					return;
				}
				if (utils::has_if(arrMeta.list[ListType::LT_All], [typeIndex](auto idx) {
							return idx == typeIndex;
						})) {
					arrFilter.push_back(typeIndex);
					return;
				}

				GAIA_ASSERT("SetChangeFilter trying to filter ECS component which is not a part of the query");
#if GAIA_DEBUG
				constexpr auto typeName = utils::type_info::name<T>();
				LOG_E(
						"SetChangeFilter trying to filter ECS component %.*s but "
						"it's not a part of the query!",
						(uint32_t)typeName.length(), typeName.data());
#endif
			}

			template <typename... TComponent>
			void SetChangedFilter(ChangeFilterArray& arr, ComponentListData& arrMeta) {
				(SetChangedFilter_Internal<TComponent>(arr, arrMeta), ...);
			}

			//! Sorts internal component arrays by their type indices
			void SortComponentArrays() {
				for (auto& l: m_list) {
					for (auto& arr: l.list) {
						utils::sort(arr, [](uint32_t left, uint32_t right) {
							return left < right;
						});
					}
				}
			}

			void CalculateLookupHash(const World& world) {
				// Sort the arrays if necessary
				if (m_sort) {
					SortComponentArrays();
					m_sort = false;
				}

				// Make sure we don't calculate the hash twice
				GAIA_ASSERT(m_hashLookup == 0);

				const auto& cc = GetComponentCache(world);

				// Contraints
				m_hashLookup = utils::hash_combine(m_hashLookup, (uint64_t)m_constraints);

				// Filters
				{
					uint64_t hash = 0;
					for (auto typeIndex: m_listChangeFiltered[ComponentType::CT_Generic])
						hash = utils::hash_combine(hash, (uint64_t)typeIndex);
					hash = utils::hash_combine(hash, (uint64_t)m_listChangeFiltered[ComponentType::CT_Generic].size());
					for (auto typeIndex: m_listChangeFiltered[ComponentType::CT_Chunk])
						hash = utils::hash_combine(hash, (uint64_t)typeIndex);
					hash = utils::hash_combine(hash, (uint64_t)m_listChangeFiltered[ComponentType::CT_Chunk].size());
				}

				// Generic components lookup hash
				{
					uint64_t hash = 0;
					const auto& l = m_list[ComponentType::CT_Generic];
					for (uint32_t i = 0; i < ListType::LT_Count; ++i) {
						const auto& arr = l.list[i];
						hash = utils::hash_combine(hash, (uint64_t)i);
						for (uint32_t j = 0; j < arr.size(); ++j) {
							const auto* type = cc.GetComponentMetaTypeFromIdx(arr[j]);
							GAIA_ASSERT(type != nullptr);
							hash = utils::hash_combine(hash, type->lookupHash);
						}
					}
					m_hashLookup = utils::hash_combine(m_hashLookup, hash);
				}

				// Chunk components lookup hash
				{
					uint64_t hash = 0;
					const auto& l = m_list[ComponentType::CT_Chunk];
					for (uint32_t i = 0; i < ListType::LT_Count; ++i) {
						const auto& arr = l.list[i];
						hash = utils::hash_combine(hash, (uint64_t)i);
						for (uint32_t j = 0; j < arr.size(); ++j) {
							const auto* type = cc.GetComponentMetaTypeFromIdx(arr[j]);
							GAIA_ASSERT(type != nullptr);
							hash = utils::hash_combine(hash, type->lookupHash);
						}
					}
					m_hashLookup = utils::hash_combine(m_hashLookup, hash);
				}
			}

			void CalculateMatcherHashes(const World& world) {
				if (!m_recalculate)
					return;
				m_recalculate = false;

				// Sort the arrays if necessary
				if (m_sort) {
					m_sort = false;
					SortComponentArrays();
				}

				// Calculate the matcher hash
				{
					const auto& cc = GetComponentCache(world);

					for (auto& l: m_list) {
						for (uint32_t i = 0; i < ListType::LT_Count; ++i) {
							auto& arr = l.list[i];

							if (!arr.empty()) {
								const auto* type = cc.GetComponentMetaTypeFromIdx(arr[0]);
								GAIA_ASSERT(type != nullptr);
								l.hash[i] = type->matcherHash;
							}
							for (uint32_t j = 1; j < arr.size(); ++j) {
								const auto* type = cc.GetComponentMetaTypeFromIdx(arr[j]);
								GAIA_ASSERT(type != nullptr);
								l.hash[i] = utils::combine_or(l.hash[i], type->matcherHash);
							}
						}
					}
				}
			}

			/*!
				Tries to match \param componentTypeList with a given \param matcherHash.
				\return MatchArchetypeQueryRet::Fail if there is no match, MatchArchetypeQueryRet::Ok for match or
								MatchArchetypeQueryRet::Skip is not relevant.
				*/
			template <ComponentType TComponentType>
			[[nodiscard]] MatchArchetypeQueryRet
			Match(const ChunkComponentTypeList& componentTypeList, uint64_t matcherHash) const {
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

				// If there is any match with the withNoneList we quit
				if (withNoneTest != 0) {
					for (const auto typeIndex: queryList.list[ListType::LT_None]) {
						for (const auto& component: componentTypeList) {
							if (component.type->typeIndex == typeIndex) {
								return MatchArchetypeQueryRet::Fail;
							}
						}
					}
				}

				// If there is any match with the withAnyTest
				if (withAnyTest != 0) {
					for (const auto typeIndex: queryList.list[ListType::LT_Any]) {
						for (const auto& component: componentTypeList) {
							if (component.type->typeIndex == typeIndex)
								goto checkWithAllMatches;
						}
					}

					// At least one match necessary to continue
					return MatchArchetypeQueryRet::Fail;
				}

			checkWithAllMatches:
				// If withAllList is not empty there has to be an exact match
				if (withAllTest != 0) {
					// If the number of queried components is greater than the
					// number of components in archetype there's no need to search
					if (queryList.list[ListType::LT_All].size() <= componentTypeList.size()) {
						uint32_t matches = 0;

						// m_list[ListType::LT_All] first because we usually request for less
						// components than there are components in archetype
						for (const auto typeIndex: queryList.list[ListType::LT_All]) {
							for (const auto& component: componentTypeList) {
								if (component.type->typeIndex != typeIndex)
									continue;

								// All requirements are fulfilled. Let's iterate
								// over all chunks in archetype
								if (++matches == queryList.list[ListType::LT_All].size())
									return MatchArchetypeQueryRet::Ok;

								break;
							}
						}
					}

					// No match found. We're done
					return MatchArchetypeQueryRet::Fail;
				}

				return (withAnyTest != 0) ? MatchArchetypeQueryRet::Ok : MatchArchetypeQueryRet::Skip;
			}

			[[nodiscard]] bool operator==(const EntityQuery& other) const {
				// Lookup hash must match
				if (m_hashLookup != other.m_hashLookup)
					return false;

				if (m_constraints != other.m_constraints)
					return false;

				for (uint32_t j = 0; j < ComponentType::CT_Count; ++j) {
					const auto& queryList = m_list[j];
					const auto& otherList = other.m_list[j];

					// Component count needes to be the same
					for (uint32_t i = 0; i < ListType::LT_Count; ++i) {
						if (queryList.list[i].size() != otherList.list[i].size())
							return false;
					}

					// Matches hashes need to be the same
					for (uint32_t i = 0; i < ListType::LT_Count; ++i) {
						if (queryList.hash[i] != otherList.hash[i])
							return false;
					}

					// Filter count needs to be the same
					if (m_listChangeFiltered[j].size() != other.m_listChangeFiltered[j].size())
						return false;

					// Components need to be the same
					for (uint32_t i = 0; i < ListType::LT_Count; ++i) {
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

				return false;
			}

			[[nodiscard]] bool operator!=(const EntityQuery& other) const {
				return !operator==(other);
			}

			/*!
			Tries to match the query against \param archetypeList. For each matched archetype the archetype is cached.
			This is necessary so we do not iterate all chunks over and over again when running queries.
			*/
			void Match(const containers::darray<Archetype*>& archetypeList) {
				if (m_recalculate && !archetypeList.empty()) {
					const auto& world = archetypeList[0]->GetWorld();
					CalculateMatcherHashes(world);
				}

				for (uint32_t i = m_lastArchetypeId; i < archetypeList.size(); i++) {
					auto* pArchetype = archetypeList[i];
#if GAIA_DEBUG
					auto& archetype = *pArchetype;
#else
					const auto& archetype = *pArchetype;
#endif

					// Early exit if generic query doesn't match
					const auto retGeneric = Match<ComponentType::CT_Generic>(
							GetArchetypeComponentTypeList(archetype, ComponentType::CT_Generic),
							GetArchetypeMatcherHash(archetype, ComponentType::CT_Generic));
					if (retGeneric == EntityQuery::MatchArchetypeQueryRet::Fail)
						continue;

					// Early exit if chunk query doesn't match
					const auto retChunk = Match<ComponentType::CT_Chunk>(
							GetArchetypeComponentTypeList(archetype, ComponentType::CT_Chunk),
							GetArchetypeMatcherHash(archetype, ComponentType::CT_Chunk));
					if (retChunk == EntityQuery::MatchArchetypeQueryRet::Fail)
						continue;

					// If at least one query succeeded run our logic
					if (retGeneric == EntityQuery::MatchArchetypeQueryRet::Ok ||
							retChunk == EntityQuery::MatchArchetypeQueryRet::Ok)
						m_archetypeCache.push_back(pArchetype);
				}

				m_lastArchetypeId = (uint32_t)archetypeList.size();
			}

			void SetWorldVersion(uint32_t worldVersion) {
				m_worldVersion = worldVersion;
			}

		public:
			[[nodiscard]] const ComponentListData& GetData(ComponentType type) const {
				return m_list[type];
			}

			[[nodiscard]] const ChangeFilterArray& GetFiltered(ComponentType type) const {
				return m_listChangeFiltered[type];
			}

			EntityQuery& SetConstraints(Constraints value) {
				m_constraints = value;
				return *this;
			}

			[[nodiscard]] bool CheckConstraints(bool enabled) const {
				return m_constraints == Constraints::AcceptAll || (enabled && m_constraints == Constraints::EnabledOnly) ||
							 (!enabled && m_constraints == Constraints::DisabledOnly);
			}

			[[nodiscard]] bool HasFilters() const {
				return !m_listChangeFiltered[ComponentType::CT_Generic].empty() ||
							 !m_listChangeFiltered[ComponentType::CT_Chunk].empty();
			}

			template <typename... TComponent>
			EntityQuery& Any() {
				(AddComponent_Internal<TComponent>(m_list[ComponentType::CT_Generic].list[ListType::LT_Any]), ...);
				return *this;
			}

			template <typename... TComponent>
			EntityQuery& All() {
				(AddComponent_Internal<TComponent>(m_list[ComponentType::CT_Generic].list[ListType::LT_All]), ...);
				return *this;
			}

			template <typename... TComponent>
			EntityQuery& None() {
				(AddComponent_Internal<TComponent>(m_list[ComponentType::CT_Generic].list[ListType::LT_None]), ...);
				return *this;
			}

			template <typename... TComponent>
			EntityQuery& AnyChunk() {
				(AddComponent_Internal<TComponent>(m_list[ComponentType::CT_Chunk].list[ListType::LT_Any]), ...);
				return *this;
			}

			template <typename... TComponent>
			EntityQuery& AllChunk() {
				(AddComponent_Internal<TComponent>(m_list[ComponentType::CT_Chunk].list[ListType::LT_All]), ...);
				return *this;
			}

			template <typename... TComponent>
			EntityQuery& NoneChunk() {
				(AddComponent_Internal<TComponent>(m_list[ComponentType::CT_Chunk].list[ListType::LT_None]), ...);
				return *this;
			}

			template <typename... TComponent>
			EntityQuery& WithChanged() {
				SetChangedFilter<TComponent...>(
						m_listChangeFiltered[ComponentType::CT_Generic], m_list[ComponentType::CT_Generic]);
				return *this;
			}

			template <typename... TComponent>
			EntityQuery& WithChangedChunk() {
				SetChangedFilter<TComponent...>(m_listChangeFiltered[ComponentType::CT_Chunk], m_list[ComponentType::CT_Chunk]);
				return *this;
			}

			[[nodiscard]] uint32_t GetWorldVersion() const {
				return m_worldVersion;
			}

			[[nodiscard]] containers::darray<Archetype*>::iterator begin() {
				return m_archetypeCache.begin();
			}

			[[nodiscard]] containers::darray<Archetype*>::iterator end() {
				return m_archetypeCache.end();
			}
		};
	} // namespace ecs
} // namespace gaia
