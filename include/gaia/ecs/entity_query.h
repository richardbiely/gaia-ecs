#pragma once
#include "../utils/array.h"
#include "../utils/vector.h"
#include <algorithm>
#include <tuple>

#include "../utils/sarray.h"
#include "../utils/utility.h"
#include "../utils/utils_containers.h"
#include "common.h"
#include "component.h"
#include "fwd.h"

namespace gaia {
	namespace ecs {
		template <typename... Type>
		struct component_query_container_base {
			using types = std::tuple<Type...>;
			static constexpr auto size = sizeof...(Type);
			template <uint32_t N>
			using item = typename std::tuple_element<N, types>::type;

			static constexpr uint32_t MAX_COMPONENTS_IN_QUERY = 8u;
			static_assert(size < MAX_COMPONENTS_IN_QUERY, "Max MAX_COMPONENTS_IN_QUERY allowed to be in component query.");
			static_assert(utils::is_unique<std::decay_t<Type>...>, "Only unique inputs are enabled for component query.");
		};

		enum class QueryTypes { All, Any, None, Empty };

		template <QueryTypes qt, typename... Type>
		struct component_query_container: component_query_container_base<Type...> {
		private:
			static constexpr uint64_t calculate_combined_hash() {
				utils::array<uint64_t, sizeof...(Type)> arr = {utils::type_info::hash<Type>()...};
				utils::sort(arr);
				return CalculateLookupHash(arr);
			}

		public:
			using types = utils::type_list<Type...>;

			static constexpr QueryTypes query_type = qt;
			//! Loopup hash - can be used to hashmap lookups
			static constexpr uint64_t lookupHash = calculate_combined_hash();
			//! Combination hash - can be used to quickly check for matches
			static constexpr uint64_t matcherHash = CalculateMatcherHash<Type...>();
		};

		template <>
		struct component_query_container<QueryTypes::Empty> {
			using types = utils::type_list<>;

			static constexpr QueryTypes query_type = QueryTypes::Empty;
			//! Loopup hash - can be used to hashmap lookups
			static constexpr uint64_t lookupHash = 0;
			//! Combination hash - can be used to quickly check for matches
			static constexpr uint64_t matcherHash = 0;
		};

		using EmptyTypes = component_query_container<QueryTypes::Empty>;
		template <typename... Type>
		using AnyTypes = component_query_container<QueryTypes::Any, Type...>;
		template <typename... Type>
		using AllTypes = component_query_container<QueryTypes::All, Type...>;
		template <typename... Type>
		using NoneTypes = component_query_container<QueryTypes::None, Type...>;

		template <typename T1 = ecs::EmptyTypes, typename T2 = ecs::EmptyTypes, typename T3 = ecs::EmptyTypes>
		struct EntityQuery2;

		template <typename T1, typename T2, typename T3>
		struct EntityQuery2 final {
			// Deduce which template arg is supposed to be used as 'all'
			using all = std::conditional_t<
					T1::query_type == QueryTypes::All, T1, std::conditional_t<T2::query_type == QueryTypes::All, T2, T3>>;
			// Deduce which template arg is supposed to be used as 'any'
			using any = std::conditional_t<
					T1::query_type == QueryTypes::Any, T1, std::conditional_t<T2::query_type == QueryTypes::Any, T2, T3>>;
			// Deduce which template arg is supposed to be used as 'none'
			using none = std::conditional_t<
					T1::query_type == QueryTypes::None, T1, std::conditional_t<T2::query_type == QueryTypes::None, T2, T3>>;

			// Make sure there are no duplicates among types
			static_assert(
					utils::is_unique<utils::type_list_concat<typename T1::types, typename T2::types>> &&
							utils::is_unique<utils::type_list_concat<typename T1::types, typename T3::types>> &&
							utils::is_unique<utils::type_list_concat<typename T2::types, typename T3::types>>,
					"Unique types need to be provided to EntityQuery2");
		};

		template <typename TQuery>
		inline void DiagQuery([[maybe_unused]] const TQuery& q) {
			auto print_type = [](auto const&... e) {
				(printf(
						 "%.*s\n", (uint32_t)utils::type_info::name<decltype(e)>().length(),
						 utils::type_info::name<decltype(e)>().data()),
				 ...);
			};
			LOG_N("AllTypes (lookupHash: %016llx, matcherHash: %016llx):", TQuery::all::lookupHash, TQuery::all::matcherHash);
			std::apply(print_type, typename TQuery::all::types{});
			LOG_N("AnyTypes (lookupHash: %016llx, matcherHash: %016llx):", TQuery::any::lookupHash, TQuery::any::matcherHash);
			std::apply(print_type, typename TQuery::any::types{});
			LOG_N(
					"NoneTypes (lookupHash: %016llx, matcherHash: %016llx):", TQuery::none::lookupHash,
					TQuery::none::matcherHash);
			std::apply(print_type, typename TQuery::none::types{});
		}

		class EntityQuery final {
		private:
			//! Number of components that can be a part of EntityQuery
			static constexpr uint32_t MAX_COMPONENTS_IN_QUERY = 8u;
			friend class World;

			// Keeps an array of Component type indices
			using ComponentIndexArray = utils::sarray<uint32_t, MAX_COMPONENTS_IN_QUERY>;
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