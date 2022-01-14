#pragma once
#include "../containers/sarray.h"
#include "../containers/sarray_ext.h"
#include <algorithm>
#include <tuple>

#include "../containers/sarray_ext.h"
#include "../utils/utility.h"
#include "../utils/utils_containers.h"
#include "common.h"
#include "component.h"
#include "fwd.h"

namespace gaia {
	namespace ecs {
		class Archetype;

		class EntityQuery final {
		private:
			//! Number of components that can be a part of EntityQuery
			static constexpr uint32_t MAX_COMPONENTS_IN_QUERY = 8u;
			friend class World;

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

			template <class TComponent>
			void CalculateHash_Internal(ComponentIndexArray& arr, [[maybe_unused]] uint64_t& hash) {
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
								"Trying to add ECS component '%.*s' to an already full ECS "
								"query!",
								(uint32_t)typeName.length(), typeName.data());
						LOG_E("Already present:");
						for (uint32_t i = 0U; i < arr.size(); i++) {
							const auto metaType = g_ComponentCache.GetComponentMetaTypeFromIdx(arr[i]);
							LOG_E("> [%u] %.*s", i, (uint32_t)metaType->name.length(), metaType->name.data());
						}

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
					LOG_E("Already present:");
					for (auto i = 0U; i < (uint32_t)arrFilter.size(); i++) {
						const auto metaType = g_ComponentCache.GetComponentMetaTypeFromIdx(arrFilter[i]);
						LOG_E("> [%u] %.*s", i, (uint32_t)metaType->name.length(), metaType->name.data());
					}

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

			[[nodiscard]] bool HasFilters() const {
				return !listChangeFiltered[ComponentType::CT_Generic].empty() ||
							 !listChangeFiltered[ComponentType::CT_Chunk].empty();
			}

		public:
			const ComponentListData& GetData(ComponentType type) const {
				return list[type];
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

			void SetWorldVersion(uint32_t worldVersion) {
				m_worldVersion = worldVersion;
			}
			uint32_t GetWorldVersion() const {
				return m_worldVersion;
			}
		};
	} // namespace ecs
} // namespace gaia