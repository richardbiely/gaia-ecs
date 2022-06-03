#pragma once
#include <algorithm>
#include <tuple>

#include "../containers/darray.h"
#include "../containers/sarray_ext.h"
#include "../utils/containers.h"
#include "../utils/utility.h"
#include "archetype.h"
#include "common.h"
#include "component.h"
#include "fwd.h"

namespace gaia {
	namespace ecs {
		class EntityQuery final {
		public:
			enum class Constraints { EnabledOnly, DisabledOnly, AcceptAll };

		private:
			//! Number of components that can be a part of EntityQuery
			static constexpr uint32_t MAX_COMPONENTS_IN_QUERY = 8u;

			// Keeps an array of Component type indices
			using ComponentIndexArray = containers::sarray_ext<uint32_t, MAX_COMPONENTS_IN_QUERY>;
			using ChangeFilterArray = ComponentIndexArray;

			struct ComponentListData {
				ComponentIndexArray listNone{};
				ComponentIndexArray listAny{};
				ComponentIndexArray listAll{};

				uint64_t hashNone = 0;
				uint64_t hashAny = 0;
				uint64_t hashAll = 0;
			};
			//! List of querried components
			ComponentListData list[ComponentType::CT_Count]{};
			//! List of filtered components
			ChangeFilterArray listChangeFiltered[ComponentType::CT_Count]{};
			//! Version of the world for which the query has been called most recently
			uint32_t m_worldVersion = 0;
			//! Entity of the last added archetype in the world this query remembers
			uint32_t m_lastArchetypeId = 0;
			//! List of cached archetypes
			containers::darray<Archetype*> m_archetypeCache;
			//! Tell what kinds of chunks are going to be accepted by the query
			Constraints constraints = Constraints::EnabledOnly;

			template <class TComponent>
			void CalculateHash_Internal([[maybe_unused]] ComponentIndexArray& arr, [[maybe_unused]] uint64_t& hash) {
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
					hash = CalculateMatcherHash(hash, CalculateMatcherHash<T>());
				}
			}

			template <typename... TComponent>
			void CalculateHashes(ComponentIndexArray& arr, uint64_t& hash) {
				(CalculateHash_Internal<TComponent>(arr, hash), ...);
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
				if (utils::has_if(arrMeta.listAny, [typeIndex](auto idx) {
							return idx == typeIndex;
						})) {
					arrFilter.push_back(typeIndex);
					return;
				}
				if (utils::has_if(arrMeta.listAll, [typeIndex](auto idx) {
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
			enum class MatchArchetypeQueryRet { Fail, Ok, Skip };

			template <ComponentType TComponentType>
			[[nodiscard]] MatchArchetypeQueryRet
			MatchList(const ChunkComponentTypeList& componentTypeList, uint64_t matcherHash) {
				const auto& queryList = GetData(TComponentType);
				const uint64_t withNoneTest = matcherHash & queryList.hashNone;
				const uint64_t withAnyTest = matcherHash & queryList.hashAny;
				const uint64_t withAllTest = matcherHash & queryList.hashAll;

				// If withAllTest is empty but we wanted something
				if (!withAllTest && queryList.hashAll != 0)
					return MatchArchetypeQueryRet::Fail;

				// If withAnyTest is empty but we wanted something
				if (!withAnyTest && queryList.hashAny != 0)
					return MatchArchetypeQueryRet::Fail;

				// If there is any match with the withNoneList we quit
				if (withNoneTest != 0) {
					for (const auto typeIndex: queryList.listNone) {
						for (const auto& component: componentTypeList) {
							if (component.type->typeIndex == typeIndex) {
								return MatchArchetypeQueryRet::Fail;
							}
						}
					}
				}

				// If there is any match with the withAnyTest
				if (withAnyTest != 0) {
					for (const auto typeIndex: queryList.listAny) {
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
					if (queryList.listAll.size() <= componentTypeList.size()) {
						uint32_t matches = 0;

						// listAll first because we usually request for less
						// components than there are components in archetype
						for (const auto typeIndex: queryList.listAll) {
							for (const auto& component: componentTypeList) {
								if (component.type->typeIndex != typeIndex)
									continue;

								// All requirements are fulfilled. Let's iterate
								// over all chunks in archetype
								if (++matches == queryList.listAll.size())
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

		public:
			[[nodiscard]] const ComponentListData& GetData(ComponentType type) const {
				return list[type];
			}

			[[nodiscard]] const ChangeFilterArray& GetFiltered(ComponentType type) const {
				return listChangeFiltered[type];
			}

			void SetConstraints(Constraints value) {
				constraints = value;
			}

			[[nodiscard]] bool CheckConstraints(bool enabled) const {
				return constraints == Constraints::AcceptAll || (enabled && constraints == Constraints::EnabledOnly) ||
							 (!enabled && constraints == Constraints::DisabledOnly);
			}

			[[nodiscard]] bool HasFilters() const {
				return !listChangeFiltered[ComponentType::CT_Generic].empty() ||
							 !listChangeFiltered[ComponentType::CT_Chunk].empty();
			}

			template <typename... TComponent>
			EntityQuery& Any() {
				CalculateHashes<TComponent...>(
						list[ComponentType::CT_Generic].listAny, list[ComponentType::CT_Generic].hashAny);
				return *this;
			}

			template <typename... TComponent>
			EntityQuery& All() {
				CalculateHashes<TComponent...>(
						list[ComponentType::CT_Generic].listAll, list[ComponentType::CT_Generic].hashAll);
				return *this;
			}

			template <typename... TComponent>
			EntityQuery& None() {
				CalculateHashes<TComponent...>(
						list[ComponentType::CT_Generic].listNone, list[ComponentType::CT_Generic].hashNone);
				return *this;
			}

			template <typename... TComponent>
			EntityQuery& AnyChunk() {
				CalculateHashes<TComponent...>(list[ComponentType::CT_Chunk].listAny, list[ComponentType::CT_Chunk].hashAny);
				return *this;
			}

			template <typename... TComponent>
			EntityQuery& AllChunk() {
				CalculateHashes<TComponent...>(list[ComponentType::CT_Chunk].listAll, list[ComponentType::CT_Chunk].hashAll);
				return *this;
			}

			template <typename... TComponent>
			EntityQuery& NoneChunk() {
				CalculateHashes<TComponent...>(list[ComponentType::CT_Chunk].listNone, list[ComponentType::CT_Chunk].hashNone);
				return *this;
			}

			template <typename... TComponent>
			EntityQuery& WithChanged() {
				SetChangedFilter<TComponent...>(listChangeFiltered[ComponentType::CT_Generic], list[ComponentType::CT_Generic]);
				return *this;
			}

			template <typename... TComponent>
			EntityQuery& WithChangedChunk() {
				SetChangedFilter<TComponent...>(listChangeFiltered[ComponentType::CT_Chunk], list[ComponentType::CT_Chunk]);
				return *this;
			}

			void Match(const containers::darray<Archetype*>& archetypeList) {
				for (uint32_t i = m_lastArchetypeId; i < archetypeList.size(); i++) {
					auto* pArchetype = archetypeList[i];
#if GAIA_DEBUG
					auto& archetype = *pArchetype;
#else
					const auto& archetype = *pArchetype;
#endif

					// Early exit if generic query doesn't match
					const auto retGeneric = MatchList<ComponentType::CT_Generic>(
							GetArchetypeComponentTypeList(archetype, ComponentType::CT_Generic),
							GetArchetypeMatcherHash(archetype, ComponentType::CT_Generic));
					if (retGeneric == EntityQuery::MatchArchetypeQueryRet::Fail)
						continue;

					// Early exit if chunk query doesn't match
					const auto retChunk = MatchList<ComponentType::CT_Chunk>(
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
