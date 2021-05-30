#pragma once
#include <algorithm>
#include <array>
#include <tuple>
#include <vector>

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
			static_assert(
					size < MAX_COMPONENTS_IN_QUERY,
					"Only component with size of at most MAX_COMPONENTS_IN_QUERY are "
					"allowed");
			static_assert(
					utils::is_unique<std::decay_t<Type>...>,
					"Only unique inputs are enabled");
		};

		enum class QueryTypes { Any, All, None };

		template <QueryTypes qt, typename... Type>
		struct component_query_container: component_query_container_base<Type...> {
		private:
			static constexpr uint64_t calculate_combined_hash() {
				std::array<uint64_t, sizeof...(Type)> arr = {
						utils::type_info::hash<Type>()...};
				utils::sort(arr);
				return CalculateLookupHash(arr);
			}

		public:
			static constexpr QueryTypes query_type = qt;
			//! Loopup hash - can be used to hashmap lookups
			static constexpr uint64_t lookupHash = calculate_combined_hash();
			//! Combination hash - can be used to quickly check for matches
			static constexpr uint64_t matcherHash = CalculateMatcherHash<Type...>();
		};

		template <typename... Type>
		using AnyTypes = component_query_container<QueryTypes::Any, Type...>;
		template <typename... Type>
		using AllTypes = component_query_container<QueryTypes::All, Type...>;
		template <typename... Type>
		using NoneTypes = component_query_container<QueryTypes::None, Type...>;

		template <
				typename T1 = ecs::AllTypes<>, typename T2 = ecs::AnyTypes<>,
				typename T3 = ecs::NoneTypes<>>
		struct EntityQuery2;

		template <typename T1, typename T2, typename T3>
		struct EntityQuery2 final {
			using all = std::conditional_t<
					T1::query_type == QueryTypes::All, T1,
					std::conditional_t<T2::query_type == QueryTypes::All, T2, T3>>;
			using any = std::conditional_t<
					T1::query_type == QueryTypes::Any, T1,
					std::conditional_t<T2::query_type == QueryTypes::Any, T2, T3>>;
			using none = std::conditional_t<
					T1::query_type == QueryTypes::None, T1,
					std::conditional_t<T2::query_type == QueryTypes::None, T2, T3>>;

			// TODO: Make sure there are no duplicates among types
			static_assert(true);
		};

		template <typename T1, typename T2>
		struct EntityQuery2<T1, T2, ecs::NoneTypes<>> {
			using all = std::conditional_t<
					T1::query_type == QueryTypes::All, T1,
					std::conditional_t<
							T2::query_type == QueryTypes::All, T2, ecs::NoneTypes<>>>;
			using any = std::conditional_t<
					T1::query_type == QueryTypes::Any, T1,
					std::conditional_t<
							T2::query_type == QueryTypes::Any, T2, ecs::NoneTypes<>>>;
			using none = ecs::NoneTypes<>;

			// TODO: Make sure there are no duplicates among types
			static_assert(true);
		};

		template <typename T1, typename T2>
		struct EntityQuery2<T1, T2, ecs::AnyTypes<>> {
			using all = std::conditional_t<
					T1::query_type == QueryTypes::All, T1,
					std::conditional_t<
							T2::query_type == QueryTypes::All, T2, ecs::AnyTypes<>>>;
			using any = ecs::AnyTypes<>;
			using none = std::conditional_t<
					T1::query_type == QueryTypes::None, T1,
					std::conditional_t<
							T2::query_type == QueryTypes::None, T2, ecs::AnyTypes<>>>;

			// TODO: Make sure there are no duplicates among types
			static_assert(true);
		};

		template <typename T1, typename T2>
		struct EntityQuery2<T1, T2, ecs::AllTypes<>> {
			using all = ecs::AllTypes<>;
			using any = std::conditional_t<
					T1::query_type == QueryTypes::Any, T1,
					std::conditional_t<
							T2::query_type == QueryTypes::Any, T2, ecs::AllTypes<>>>;
			using none = std::conditional_t<
					T1::query_type == QueryTypes::None, T1,
					std::conditional_t<
							T2::query_type == QueryTypes::None, T2, ecs::AllTypes<>>>;

			// TODO: Make sure there are no duplicates among types
			static_assert(true);
		};

		template <typename T1>
		struct EntityQuery2<T1, ecs::AnyTypes<>, ecs::NoneTypes<>> {
			using all = T1;
			using any = ecs::AnyTypes<>;
			using none = ecs::NoneTypes<>;
		};
		template <typename T1>
		struct EntityQuery2<T1, ecs::AllTypes<>, ecs::NoneTypes<>> {
			using all = ecs::AllTypes<>;
			using any = T1;
			using none = ecs::NoneTypes<>;
		};
		template <typename T1>
		struct EntityQuery2<T1, ecs::AllTypes<>, ecs::AnyTypes<>> {
			using all = ecs::AllTypes<>;
			using any = ecs::AnyTypes<>;
			using none = T1;
		};

		template <typename TQuery>
		inline void DiagQuery([[maybe_unused]] const TQuery& q) {
			auto print_type = [](auto const&... e) {
				(printf(
						 "%.*s\n", (uint32_t)utils::type_info::name<decltype(e)>().length(),
						 utils::type_info::name<decltype(e)>().data()),
				 ...);
			};
			LOG_N(
					"AllTypes (lookupHash: %016llx, matcherHash: %016llx):",
					TQuery::all::lookupHash, TQuery::all::matcherHash);
			std::apply(print_type, typename TQuery::all::types{});
			LOG_N(
					"AnyTypes (lookupHash: %016llx, matcherHash: %016llx):",
					TQuery::any::lookupHash, TQuery::any::matcherHash);
			std::apply(print_type, typename TQuery::any::types{});
			LOG_N(
					"NoneTypes (lookupHash: %016llx, matcherHash: %016llx):",
					TQuery::none::lookupHash, TQuery::none::matcherHash);
			std::apply(print_type, typename TQuery::none::types{});
		}

		class EntityQuery final {
		private:
			//! Number of components that can be a part of EntityQuery
			static constexpr uint32_t MAX_COMPONENTS_IN_QUERY = 8u;
			friend class World;

			// Keeps an array of ComponentMetaData copies. We can't keep pointers in
			// queries because as new components are added and removed the global type
			// map will get invalidated.
			using ComponentMetaDataArray =
					utils::sarray<const ComponentMetaData*, MAX_COMPONENTS_IN_QUERY>;

			// Keeps an array of ComponentMetaData hashes
			using ChangeFilterArray =
					utils::sarray<uint32_t, MAX_COMPONENTS_IN_QUERY>;

			struct ComponentListData {
				ComponentMetaDataArray listNone{};
				ComponentMetaDataArray listAny{};
				ComponentMetaDataArray listAll{};

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
			//! If true, query needs to be commited. Set to true on structural changes
			bool m_invalidated = false;

			template <class TComponent>
			bool CalculateHash_Internal(ComponentMetaDataArray& arr) {
				using T = std::decay_t<TComponent>;

				if constexpr (std::is_same<T, Entity>::value) {
					// Skip Entity input args
					return true;
				} else {
					const auto* metaType =
							g_ComponentCache.GetOrCreateComponentMetaType<T>();
					if (!utils::has(arr, metaType)) {
#if GAIA_DEBUG
						// There's a limit to the amount of components which we can store
						if (arr.size() >= MAX_COMPONENTS_IN_QUERY) {
							GAIA_ASSERT(
									false && "Trying to create an ECS query with too many "
													 "components!");
							LOG_E(
									"Trying to add ECS component '%.*s' to an already full ECS "
									"query!",
									(uint32_t)metaType->name.length(), metaType->name.data());
							LOG_W("Already present:");
							for (uint32_t i = 0; i < arr.size(); i++)
								LOG_W("> [%u] %s", i, arr[i]->name.data());
							LOG_W("Trying to add:");
							LOG_W(
									"> %.*s", (uint32_t)metaType->name.length(),
									metaType->name.data());

							return false;
						}
#endif

						arr.push_back(metaType);
						m_invalidated = true;
					}

					return true;
				}
			}

			template <typename... TComponent>
			void CalculateHashes(ComponentMetaDataArray& arr) {
				(CalculateHash_Internal<TComponent>(arr) && ...);
			}

			template <typename TComponent>
			bool SetChangedFilter_Internal(
					ChangeFilterArray& arr, ComponentListData& arrMeta) {
				using T = std::decay_t<TComponent>;
				static_assert(
						!std::is_same<T, Entity>::value,
						"It doesn't make sense to use ChangedFilter with Entity");

				const auto* metaType =
						g_ComponentCache.GetOrCreateComponentMetaType<T>();
				if (!utils::has(arr, metaType->typeIndex)) {
#if GAIA_DEBUG
					// There's a limit to the amount of components which we can store
					if (arr.size() >= MAX_COMPONENTS_IN_QUERY) {
						GAIA_ASSERT(
								false && "Trying to create an ECS filter query with too "
												 "many components!");
						LOG_E(
								"Trying to add ECS component %.*s to an already full "
								"filter query!",
								(uint32_t)metaType->name.length(), metaType->name.data());
						LOG_E("Already present:");
						for (auto i = 0U; i < (uint32_t)arr.size(); i++) {
							const auto meta =
									g_ComponentCache.GetComponentMetaTypeFromIdx(arr[i]);
							LOG_E(
									"> [%u] %.*s", i, (uint32_t)meta->name.length(),
									meta->name.data());
						}

						return false;
					}
#endif

					// Component has to be present in anyList or allList.
					// NoneList makes no sense because we skip those in query processing
					// anyway
					if (!utils::has_if(
									arrMeta.listAny,
									[metaType](const auto* info) {
										return info->typeIndex == metaType->typeIndex;
									}) &&
							!utils::has_if(arrMeta.listAll, [metaType](const auto* info) {
								return info->typeIndex == metaType->typeIndex;
							})) {
#if GAIA_DEBUG
						LOG_E(
								"SetChangeFilter trying to filter ECS component %.*s but "
								"it's not a part of the query!",
								(uint32_t)metaType->name.length(), metaType->name.data());
#endif
						return false;
					}

					arr.push_back(metaType->typeIndex);
					m_invalidated = true;
				}

				return true;
			}

			template <typename... TComponent>
			void
			SetChangedFilter(ChangeFilterArray& arr, ComponentListData& arrMeta) {
				(SetChangedFilter_Internal<TComponent>(arr, arrMeta) && ...);
			}

		public:
			const ComponentListData& GetData(ComponentType type) const {
				return list[type];
			}

			template <typename... TComponent>
			EntityQuery& Any() {
				CalculateHashes<TComponent...>(list[ComponentType::CT_Generic].listAny);
				return *this;
			}
			template <typename... TComponent>
			EntityQuery& All() {
				CalculateHashes<TComponent...>(list[ComponentType::CT_Generic].listAll);
				return *this;
			}
			template <typename... TComponent>
			EntityQuery& None() {
				CalculateHashes<TComponent...>(
						list[ComponentType::CT_Generic].listNone);
				return *this;
			}

			template <typename... TComponent>
			EntityQuery& AnyChunk() {
				CalculateHashes<TComponent...>(list[ComponentType::CT_Chunk].listAny);
				return *this;
			}
			template <typename... TComponent>
			EntityQuery& AllChunk() {
				CalculateHashes<TComponent...>(list[ComponentType::CT_Chunk].listAll);
				return *this;
			}
			template <typename... TComponent>
			EntityQuery& NoneChunk() {
				CalculateHashes<TComponent...>(list[ComponentType::CT_Chunk].listNone);
				return *this;
			}

			template <typename... TComponent>
			EntityQuery& WithChanged() {
				SetChangedFilter<TComponent...>(
						listChangeFiltered[ComponentType::CT_Generic],
						list[ComponentType::CT_Generic]);
				return *this;
			}
			template <typename... TComponent>
			EntityQuery& WithChangedChunk() {
				SetChangedFilter<TComponent...>(
						listChangeFiltered[ComponentType::CT_Chunk],
						list[ComponentType::CT_Chunk]);
				return *this;
			}

			void Commit() {
				// Make sure not to commit already committed queue
				if (!m_invalidated)
					return;

				m_invalidated = false;

				auto commit = [&](ComponentType componentType) {
					auto& listNone = list[componentType].listNone;
					auto& listAny = list[componentType].listAny;
					auto& listAll = list[componentType].listAll;

					list[componentType].hashNone = CalculateMatcherHash(listNone);
					list[componentType].hashAny = CalculateMatcherHash(listAny);
					list[componentType].hashAll = CalculateMatcherHash(listAll);
				};

				commit(ComponentType::CT_Generic);
				commit(ComponentType::CT_Chunk);
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