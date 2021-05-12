#pragma once
#include <algorithm>
#include <array>
#include <vector>

#include "../external/stack_allocator.h"
#include "../utils/utils_std.h"
#include "common.h"
#include "component.h"
#include "fwd.h"

namespace gaia {
	namespace ecs {
		class EntityQuery final {
		private:
			//! Number of components that can be a part of EntityQuery
			static constexpr uint32_t MAX_COMPONENTS_IN_QUERY = 8u;
			friend class World;

			// Keeps an array of ComponentMetaData copies. We can't keep pointers in
			// queries because as new components are added and removed the global type
			// map will get invalidated.
			using ComponentMetaDataArrayAllocator =
					stack_allocator<const ComponentMetaData*, MAX_COMPONENTS_IN_QUERY>;
			using ComponentMetaDataArray = std::vector<
					const ComponentMetaData*, ComponentMetaDataArrayAllocator>;

			// Keeps an array of ComponentMetaData hashes
			using ChangeFilterArrayAllocator =
					stack_allocator<uint32_t, MAX_COMPONENTS_IN_QUERY>;
			using ChangeFilterArray =
					std::vector<uint32_t, ChangeFilterArrayAllocator>;

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
			//! Version of the world at which the querry was commited
			uint32_t m_committedWorldVersion = 0;
			//! If true, query needs to be commited. Set to true on structural changes
			bool m_invalidated = false;

			EntityQuery& Commit(uint32_t worldVersion) {
				// Make sure not to commit already committed queue
				if (!m_invalidated && m_committedWorldVersion == worldVersion)
					return *this;

				m_invalidated = false;
				m_committedWorldVersion = worldVersion;

				auto commit = [&](ComponentType componentType) {
					auto& listNone = list[componentType].listNone;
					auto& listAny = list[componentType].listAny;
					auto& listAll = list[componentType].listAll;

					// Make sure to sort the meta-types so we receive the same hash no
					// matter the order in which components are provided Bubble sort is
					// okay. We're dealing with at most MAX_COMPONENTS_PER_ARCHETYPE items
					// anyway. 99% of time with at most 3 or 4
					// TODO: Replace with a sorting network
					std::sort(
							listNone.begin(), listNone.end(),
							std::less<const ComponentMetaData*>());
					std::sort(
							listAny.begin(), listAny.end(),
							std::less<const ComponentMetaData*>());
					std::sort(
							listAll.begin(), listAll.end(),
							std::less<const ComponentMetaData*>());

					list[componentType].hashNone = CalculateComponentsHash(listNone);
					list[componentType].hashAny = CalculateComponentsHash(listAny);
					list[componentType].hashAll = CalculateComponentsHash(listAll);
				};

				commit(ComponentType::CT_Generic);
				commit(ComponentType::CT_Chunk);
				return *this;
			}

			template <class TComponent>
			bool CalculateHash_Internal(ComponentMetaDataArray& arr) {
				using T = std::decay_t<TComponent>;

					if constexpr (std::is_same<T, Entity>::value) {
						// Skip Entity input args
						return true;
					} else {
						const auto* metaType = GetOrCreateComponentMetaType<T>();
							if (!utils::has(arr, metaType)) {
									if (arr.size() >= MAX_COMPONENTS_IN_QUERY) {
										assert(
												false && "Trying to create an ECS query with too many "
																 "components!");
#if GAIA_DEBUG
										LOG_E(
												"Trying to add ECS component %s to an already full ECS "
												"query!",
												metaType->name.data());
										LOG_W("Already present:");
										for (uint32_t i = 0; i < arr.size(); i++)
											LOG_W("> [%u] %s", i, arr[i]->name.data());
										LOG_W("Trying to add:");
										LOG_W("> %s", metaType->name.data());
#endif
										return false;
								}

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

				const auto* metaType = GetOrCreateComponentMetaType<T>();
					if (!utils::has(arr, metaType->typeIndex)) {
							// There's a limit to the amount of components which we can store
							if (arr.size() >= MAX_COMPONENTS_IN_QUERY) {
								assert(
										false && "Trying to create an ECS filter query with too "
														 "many components!");
#if GAIA_DEBUG
								LOG_E(
										"Trying to add ECS component %.*s to an already full "
										"filter query!",
										(uint32_t)metaType->name.length(), metaType->name.data());
								LOG_E("Already present:");
									for (auto i = 0; i < (uint32_t)arr.size(); i++) {
										const auto meta = GetComponentMetaTypeFromIdx(arr[i]);
										LOG_E(
												"> [%u] %.*s", i, (uint32_t)meta->name.length(),
												meta->name.data());
									}
#endif
								return false;
						}

						// Component has to be present in anyList or allList.
						// NoneList makes no sense because we skip those in query processing
						// anyway
						auto checkList = [&](const ComponentMetaDataArray& arr) {
							for (const auto& c: arr) {
								if (c->typeIndex == metaType->typeIndex)
									return true;
							}
						return false;
						};
							if (!checkList(arrMeta.listAny) && !checkList(arrMeta.listAll)) {
#if GAIA_DEBUG
								LOG_E(
										"SetChangeFilter trying to filter ECS component %s but "
										"it's not "
										"a part of the query!",
										metaType->name);
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
			template <typename... TComponent> EntityQuery& WithAny() {
				CalculateHashes<TComponent...>(list[ComponentType::CT_Generic].listAny);
				return *this;
			}
			template <typename... TComponent> EntityQuery& With() {
				CalculateHashes<TComponent...>(list[ComponentType::CT_Generic].listAll);
				return *this;
			}
			template <typename... TComponent> EntityQuery& WithNone() {
				CalculateHashes<TComponent...>(
						list[ComponentType::CT_Generic].listNone);
				return *this;
			}

			template <typename... TComponent> EntityQuery& WithAnyChunkComponent() {
				CalculateHashes<TComponent...>(list[ComponentType::CT_Chunk].listAny);
				return *this;
			}
			template <typename... TComponent> EntityQuery& WithChunkComponents() {
				CalculateHashes<TComponent...>(list[ComponentType::CT_Chunk].listAll);
				return *this;
			}
			template <typename... TComponent> EntityQuery& WithNoneChunkComponents() {
				CalculateHashes<TComponent...>(list[ComponentType::CT_Chunk].listNone);
				return *this;
			}

			template <typename... TComponent> EntityQuery& WithChanged() {
				SetChangedFilter<TComponent...>(
						listChangeFiltered[ComponentType::CT_Generic],
						list[ComponentType::CT_Generic]);
				return *this;
			}
			template <typename... TComponent>
			EntityQuery& WithChangedChunkComponents() {
				SetChangedFilter<TComponent...>(
						listChangeFiltered[ComponentType::CT_Chunk],
						list[ComponentType::CT_Chunk]);
				return *this;
			}
		};
	} // namespace ecs
} // namespace gaia