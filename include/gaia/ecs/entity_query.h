#pragma once
#include <tuple>
#include <type_traits>

#include "../config/profiler.h"
#include "../containers/darray.h"
#include "../containers/sarray_ext.h"
#include "../utils/hashing_policy.h"
#include "../utils/utility.h"
#include "archetype.h"
#include "chunk.h"
#include "common.h"
#include "component.h"
#include "component_cache.h"
#include "component_utils.h"
#include "data_buffer.h"
#include "entity_query_cache.h"
#include "entity_query_common.h"
#include "entity_query_info.h"

namespace gaia {
	namespace ecs {
		class EntityQuery final {
			static constexpr uint32_t ChunkBatchSize = 16;
			using CChunkSpan = std::span<const Chunk*>;
			using ChunkBatchedList = containers::sarray_ext<Chunk*, ChunkBatchSize>;
			using CmdBufferCmdFunc = void (*)(DataBuffer& buffer, query::LookupCtx& ctx);

		public:
			//! Query constraints
			enum class Constraints : uint8_t { EnabledOnly, DisabledOnly, AcceptAll };

		private:
			//! Command buffer command type
			enum CommandBufferCmd : uint8_t { ADD_COMPONENT, ADD_FILTER };

			struct Command_AddComponent {
				static constexpr CommandBufferCmd Id = CommandBufferCmd::ADD_COMPONENT;

				component::ComponentId componentId;
				component::ComponentType componentType;
				query::ListType listType;
				bool isReadWrite;

				void Save(DataBuffer& buffer) {
					buffer.Save(componentId);
					buffer.Save(componentType);
					buffer.Save(listType);
					buffer.Save(isReadWrite);
				}
				void Load(DataBuffer& buffer) {
					buffer.Load(componentId);
					buffer.Load(componentType);
					buffer.Load(listType);
					buffer.Load(isReadWrite);
				}

				void Exec(query::LookupCtx& ctx) {
					auto& list = ctx.list[componentType];
					auto& componentIds = list.componentIds[listType];

					// Unique component ids only
					GAIA_ASSERT(!utils::has(componentIds, componentId));

#if GAIA_DEBUG
					// There's a limit to the amount of components which we can store
					if (componentIds.size() >= query::MAX_COMPONENTS_IN_QUERY) {
						GAIA_ASSERT(false && "Trying to create an ECS query with too many components!");

						const auto& cc = ComponentCache::Get();
						auto componentName = cc.GetComponentDesc(componentId).name;
						GAIA_LOG_E(
								"Trying to add ECS component '%.*s' to an already full ECS query!", (uint32_t)componentName.size(),
								componentName.data());

						return;
					}
#endif

					ctx.rw[componentType] |= (uint8_t)isReadWrite << (uint8_t)componentIds.size();
					componentIds.push_back(componentId);
				}
			};

			struct Command_Filter {
				static constexpr CommandBufferCmd Id = CommandBufferCmd::ADD_FILTER;

				component::ComponentId componentId;
				component::ComponentType componentType;

				void Save(DataBuffer& buffer) {
					buffer.Save(componentId);
					buffer.Save(componentType);
				}
				void Load(DataBuffer& buffer) {
					buffer.Load(componentId);
					buffer.Load(componentType);
				}

				void Exec(query::LookupCtx& ctx) {
					auto& list = ctx.list[componentType];
					auto& arrFilter = ctx.listChangeFiltered[componentType];

					// Unique component ids only
					GAIA_ASSERT(!utils::has(arrFilter, componentId));

#if GAIA_DEBUG
					// There's a limit to the amount of components which we can store
					if (arrFilter.size() >= query::MAX_COMPONENTS_IN_QUERY) {
						GAIA_ASSERT(false && "Trying to create an ECS filter query with too many components!");

						const auto& cc = ComponentCache::Get();
						auto componentName = cc.GetComponentDesc(componentId).name;
						GAIA_LOG_E(
								"Trying to add ECS component %.*s to an already full filter query!", (uint32_t)componentName.size(),
								componentName.data());
						return;
					}
#endif

					// Component has to be present in anyList or allList.
					// NoneList makes no sense because we skip those in query processing anyway.
					if (utils::has(list.componentIds[query::ListType::LT_Any], componentId)) {
						arrFilter.push_back(componentId);
						return;
					}
					if (utils::has(list.componentIds[query::ListType::LT_All], componentId)) {
						arrFilter.push_back(componentId);
						return;
					}

					GAIA_ASSERT(false && "SetChangeFilter trying to filter ECS component which is not a part of the query");
#if GAIA_DEBUG
					const auto& cc = ComponentCache::Get();
					auto componentName = cc.GetComponentDesc(componentId).name;
					GAIA_LOG_E(
							"SetChangeFilter trying to filter ECS component %.*s but "
							"it's not a part of the query!",
							(uint32_t)componentName.size(), componentName.data());
#endif
				}
			};

			static constexpr CmdBufferCmdFunc CommandBufferRead[] = {
					// Add component
					[](DataBuffer& buffer, query::LookupCtx& ctx) {
						Command_AddComponent cmd;
						cmd.Load(buffer);
						cmd.Exec(ctx);
					},
					// Add filter
					[](DataBuffer& buffer, query::LookupCtx& ctx) {
						Command_Filter cmd;
						cmd.Load(buffer);
						cmd.Exec(ctx);
					}};

			DataBuffer m_cmdBuffer;
			//! Query cache id
			query::QueryId m_queryId = query::QueryIdBad;
			//! Tell what kinds of chunks are going to be accepted by the query
			EntityQuery::Constraints m_constraints = EntityQuery::Constraints::EnabledOnly;

			//! Entity query cache (stable pointer to parent world's query cache)
			EntityQueryCache* m_entityQueryCache{};
			//! World version (stable pointer to parent world's world version)
			uint32_t* m_worldVersion{};
			//! List of achetypes (stable pointer to parent world's archetype array)
			containers::darray<archetype::Archetype*>* m_archetypes{};

			//--------------------------------------------------------------------------------
		public:
			query::EntityQueryInfo& FetchQueryInfo() {
				// Lookup hash is present which means EntityQueryInfo was already found
				if GAIA_LIKELY (m_queryId != query::QueryIdBad) {
					auto& queryInfo = m_entityQueryCache->Get(m_queryId);
					queryInfo.Match(*m_archetypes);
					return queryInfo;
				}

				// No lookup hash is present which means EntityQueryInfo needes to fetched or created
				query::LookupCtx ctx;
				Commit(ctx);
				m_queryId = m_entityQueryCache->GetOrCreate(std::move(ctx));
				auto& queryInfo = m_entityQueryCache->Get(m_queryId);
				queryInfo.Match(*m_archetypes);
				return queryInfo;
			}

		private:
			//--------------------------------------------------------------------------------

			template <typename T>
			void AddComponent_Internal(query::ListType listType) {
				using U = typename component::DeduceComponent<T>::Type;
				using UOriginal = typename component::DeduceComponent<T>::TypeOriginal;
				using UOriginalPR = std::remove_reference_t<std::remove_pointer_t<UOriginal>>;

				const auto componentId = component::GetComponentId<T>();
				constexpr auto componentType = component::IsGenericComponent<T> ? component::ComponentType::CT_Generic
																																				: component::ComponentType::CT_Chunk;
				constexpr bool isReadWrite =
						std::is_same_v<U, UOriginal> || (!std::is_const_v<UOriginalPR> && !std::is_empty_v<U>);

				// Make sure the component is always registered
				auto& cc = ComponentCache::Get();
				(void)cc.GetOrCreateComponentInfo<T>();

				m_cmdBuffer.Save(Command_AddComponent::Id);
				Command_AddComponent{componentId, componentType, listType, isReadWrite}.Save(m_cmdBuffer);
			}

			template <typename T>
			void WithChanged_Internal() {
				using U = typename component::DeduceComponent<T>::Type;
				using UOriginal = typename component::DeduceComponent<T>::TypeOriginal;
				using UOriginalPR = std::remove_reference_t<std::remove_pointer_t<UOriginal>>;

				const auto componentId = component::GetComponentId<T>();
				constexpr auto componentType = component::IsGenericComponent<T> ? component::ComponentType::CT_Generic
																																				: component::ComponentType::CT_Chunk;

				m_cmdBuffer.Save(Command_Filter::Id);
				Command_Filter{componentId, componentType}.Save(m_cmdBuffer);
			}

			//--------------------------------------------------------------------------------

			void Commit(query::LookupCtx& ctx) {
				GAIA_ASSERT(m_queryId == query::QueryIdBad);

				// Read data from buffer and execute the command stored in it
				m_cmdBuffer.Seek(0);
				while (m_cmdBuffer.GetPos() < m_cmdBuffer.Size()) {
					CommandBufferCmd id{};
					m_cmdBuffer.Load(id);
					CommandBufferRead[id](m_cmdBuffer, ctx);
				}

				// Calculate the lookup hash from the provided context
				query::CalculateLookupHash(ctx);

				// We can free all temporary data now
				m_cmdBuffer.Reset();
			}

			//--------------------------------------------------------------------------------

			//! Unpacks the parameter list \param types into query \param query and performs All for each of them
			template <typename... T>
			void UnpackArgsIntoQuery_All(EntityQuery& query, [[maybe_unused]] utils::func_type_list<T...> types) const {
				static_assert(sizeof...(T) > 0, "Inputs-less functors can not be unpacked to query");
				query.All<T...>();
			}

			//! Unpacks the parameter list \param types into query \param query and performs HasAll for each of them
			template <typename... T>
			GAIA_NODISCARD bool UnpackArgsIntoQuery_HasAll(
					const query::EntityQueryInfo& queryInfo, [[maybe_unused]] utils::func_type_list<T...> types) const {
				if constexpr (sizeof...(T) > 0)
					return queryInfo.HasAll<T...>();
				else
					return true;
			}

			//--------------------------------------------------------------------------------

			GAIA_NODISCARD static bool CheckFilters(const Chunk& chunk, const query::EntityQueryInfo& queryInfo) {
				GAIA_ASSERT(chunk.HasEntities() && "CheckFilters called on an empty chunk");

				const auto queryVersion = queryInfo.GetWorldVersion();

				// See if any generic component has changed
				{
					const auto& filtered = queryInfo.GetFiltered(component::ComponentType::CT_Generic);
					for (const auto componentId: filtered) {
						const auto componentIdx = chunk.GetComponentIdx(component::ComponentType::CT_Generic, componentId);
						if (chunk.DidChange(component::ComponentType::CT_Generic, queryVersion, componentIdx))
							return true;
					}
				}

				// See if any chunk component has changed
				{
					const auto& filtered = queryInfo.GetFiltered(component::ComponentType::CT_Chunk);
					for (const auto componentId: filtered) {
						const uint32_t componentIdx = chunk.GetComponentIdx(component::ComponentType::CT_Chunk, componentId);
						if (chunk.DidChange(component::ComponentType::CT_Chunk, queryVersion, componentIdx))
							return true;
					}
				}

				// Skip unchanged chunks.
				return false;
			}

			template <bool HasFilters>
			GAIA_NODISCARD bool
			CanAcceptChunkForProcessing(const Chunk& chunk, const query::EntityQueryInfo& queryInfo) const {
				if GAIA_UNLIKELY (!chunk.HasEntities())
					return false;

				if constexpr (HasFilters) {
					if (!CheckFilters(chunk, queryInfo))
						return false;
				}

				return true;
			}

			template <bool HasFilters>
			void
			ChunkBatch_Prepare(CChunkSpan chunkSpan, const query::EntityQueryInfo& queryInfo, ChunkBatchedList& chunkBatch) {
				GAIA_PROF_SCOPE(ChunkBatch_Prepare);

				for (const auto* pChunk: chunkSpan) {
					if (!CanAcceptChunkForProcessing<HasFilters>(*pChunk, queryInfo))
						continue;

					chunkBatch.push_back(const_cast<Chunk*>(pChunk));
				}
			}

			//! Execute functors in batches
			template <typename Func>
			static void ChunkBatch_Perform(Func func, ChunkBatchedList& chunks) {
				GAIA_ASSERT(!chunks.empty());

				// This is what the function is doing:
				// for (auto *pChunk: chunks)
				//	func(*pChunk);
				// chunks.clear();

				GAIA_PROF_SCOPE(ChunkBatch_Perform);

				// We only have one chunk to process
				if GAIA_UNLIKELY (chunks.size() == 1) {
					chunks[0]->SetStructuralChanges(true);
					func(*chunks[0]);
					chunks[0]->SetStructuralChanges(false);
					chunks.clear();
					return;
				}

				// We have many chunks to process.
				// Chunks might be located at different memory locations. Not even in the same memory page.
				// Therefore, to make it easier for the CPU we give it a hint that we want to prefetch data
				// for the next chunk explictely so we do not end up stalling later.
				// Note, this is a micro optimization and on average it bring no performance benefit. It only
				// helps with edge cases.
				// Let us be conservatine for now and go with T2. That means we will try to keep our data at
				// least in L3 cache or higher.
				gaia::prefetch(&chunks[1], PrefetchHint::PREFETCH_HINT_T2);
				chunks[0]->SetStructuralChanges(true);
				func(*chunks[0]);
				chunks[0]->SetStructuralChanges(false);

				size_t chunkIdx = 1;
				for (; chunkIdx < chunks.size() - 1; ++chunkIdx) {
					gaia::prefetch(&chunks[chunkIdx + 1], PrefetchHint::PREFETCH_HINT_T2);
					chunks[chunkIdx]->SetStructuralChanges(true);
					func(*chunks[chunkIdx]);
					chunks[chunkIdx]->SetStructuralChanges(false);
				}

				chunks[chunkIdx]->SetStructuralChanges(true);
				func(*chunks[chunkIdx]);
				chunks[chunkIdx]->SetStructuralChanges(false);

				chunks.clear();
			}

			template <bool HasFilters, typename Func>
			void ProcessQueryOnChunks(
					Func func, ChunkBatchedList& chunkBatch, const containers::darray<Chunk*>& chunksList,
					const query::EntityQueryInfo& queryInfo) {
				size_t chunkOffset = 0;

				size_t itemsLeft = chunksList.size();
				while (itemsLeft > 0) {
					const size_t maxBatchSize = chunkBatch.max_size() - chunkBatch.size();
					const size_t batchSize = itemsLeft > maxBatchSize ? maxBatchSize : itemsLeft;
					ChunkBatch_Prepare<HasFilters>(
							CChunkSpan((const Chunk**)&chunksList[chunkOffset], batchSize), queryInfo, chunkBatch);

					if GAIA_UNLIKELY (chunkBatch.size() == chunkBatch.max_size())
						ChunkBatch_Perform(func, chunkBatch);

					itemsLeft -= batchSize;
					chunkOffset += batchSize;
				}
			}

			template <typename Func>
			void RunQueryOnChunks_Internal(query::EntityQueryInfo& queryInfo, Func func) {
				// Update the world version
				UpdateVersion(*m_worldVersion);

				ChunkBatchedList chunkBatch;

				const bool hasFilters = queryInfo.HasFilters();
				if (hasFilters) {
					for (auto* pArchetype: queryInfo) {
						// Evaluate ordinary chunks
						if (CheckConstraints<true>())
							ProcessQueryOnChunks<true>(func, chunkBatch, pArchetype->GetChunks(), queryInfo);

						// Evaluate disabled chunks
						if (CheckConstraints<false>())
							ProcessQueryOnChunks<true>(func, chunkBatch, pArchetype->GetChunksDisabled(), queryInfo);
					}
				} else {
					for (auto* pArchetype: queryInfo) {
						// Evaluate ordinary chunks
						if (CheckConstraints<true>())
							ProcessQueryOnChunks<false>(func, chunkBatch, pArchetype->GetChunks(), queryInfo);

						// Evaluate disabled chunks
						if (CheckConstraints<false>())
							ProcessQueryOnChunks<false>(func, chunkBatch, pArchetype->GetChunksDisabled(), queryInfo);
					}
				}

				if (!chunkBatch.empty())
					ChunkBatch_Perform(func, chunkBatch);

				// Update the query version with the current world's version
				queryInfo.SetWorldVersion(*m_worldVersion);
			}

			template <typename Func>
			void ForEachChunk_Internal(Func func) {
				using InputArgs = decltype(utils::func_args(&Func::operator()));

				auto& queryInfo = FetchQueryInfo();

#if GAIA_DEBUG
				if (!std::is_invocable<Func, Chunk&>::value) {
					// Make sure we only use components specified in the query
					GAIA_ASSERT(UnpackArgsIntoQuery_HasAll(queryInfo, InputArgs{}));
				}
#endif

				RunQueryOnChunks_Internal(queryInfo, [&](Chunk& chunk) {
					func(chunk);
				});
			}

			template <typename Func>
			void ForEach_Internal(Func func) {
				using InputArgs = decltype(utils::func_args(&Func::operator()));

				auto& queryInfo = FetchQueryInfo();

#if GAIA_DEBUG
				// Make sure we only use components specified in the query
				GAIA_ASSERT(UnpackArgsIntoQuery_HasAll(queryInfo, InputArgs{}));
#endif

				RunQueryOnChunks_Internal(queryInfo, [&](Chunk& chunk) {
					chunk.ForEach(InputArgs{}, func);
				});
			}

			void InvalidateQuery() {
				m_queryId = query::QueryIdBad;
			}

		public:
			EntityQuery() = default;
			EntityQuery(
					EntityQueryCache& queryCache, uint32_t& worldVersion, containers::darray<archetype::Archetype*>& archetypes):
					m_entityQueryCache(&queryCache),
					m_worldVersion(&worldVersion), m_archetypes(&archetypes) {}

			GAIA_NODISCARD uint32_t GetQueryId() const {
				return m_queryId;
			}

			template <typename... T>
			EntityQuery& All() {
				// Adding new rules invalides the query
				InvalidateQuery();
				// Add commands to the command buffer
				(AddComponent_Internal<T>(query::ListType::LT_All), ...);
				return *this;
			}

			template <typename... T>
			EntityQuery& Any() {
				// Adding new rules invalides the query
				InvalidateQuery();
				// Add commands to the command buffer
				(AddComponent_Internal<T>(query::ListType::LT_Any), ...);
				return *this;
			}

			template <typename... T>
			EntityQuery& None() {
				// Adding new rules invalides the query
				InvalidateQuery();
				// Add commands to the command buffer
				(AddComponent_Internal<T>(query::ListType::LT_None), ...);
				return *this;
			}

			template <typename... T>
			EntityQuery& WithChanged() {
				// Adding new rules invalides the query
				InvalidateQuery();
				// Add commands to the command buffer
				(WithChanged_Internal<T>(), ...);
				return *this;
			}

			EntityQuery& SetConstraints(EntityQuery::Constraints value) {
				m_constraints = value;
				return *this;
			}

			GAIA_NODISCARD EntityQuery::Constraints GetConstraints() const {
				return m_constraints;
			}

			template <bool Enabled>
			GAIA_NODISCARD bool CheckConstraints() const {
				// By default we only evaluate EnabledOnly changes. AcceptAll is something that has to be asked for
				// explicitely.
				if GAIA_UNLIKELY (m_constraints == EntityQuery::Constraints::AcceptAll)
					return true;

				if constexpr (Enabled)
					return m_constraints == EntityQuery::Constraints::EnabledOnly;
				else
					return m_constraints == EntityQuery::Constraints::DisabledOnly;
			}

			template <typename Func>
			void ForEach(Func func) {
				// Make sure the query was created by World.CreateQuery()
				GAIA_ASSERT(m_entityQueryCache != nullptr);

				if constexpr (std::is_invocable<Func, Chunk&>::value)
					ForEachChunk_Internal(func);
				else
					ForEach_Internal(func);
			}

			template <typename Func>
			void ForEach(query::QueryId queryId, Func func) {
				using InputArgs = decltype(utils::func_args(&Func::operator()));

				// Make sure the query was created by World.CreateQuery()
				GAIA_ASSERT(m_entityQueryCache != nullptr);

				auto& queryInfo = m_entityQueryCache->Get(queryId);

#if GAIA_DEBUG
				if (!std::is_invocable<Func, Chunk&>::value) {
					// Make sure we only use components specified in the query
					GAIA_ASSERT(UnpackArgsIntoQuery_HasAll(queryInfo, InputArgs{}));
				}
#endif

				RunQueryOnChunks_Internal(queryInfo, [&](Chunk& chunk) {
					chunk.ForEach(InputArgs{}, func);
				});
			}

			/*!
				Returns true or false depending on whether there are entities matching the query.
				\warning Only use if you only care if there are any entities matching the query.
								 The result is not cached and repeated calls to the function might be slow.
								 If you already called ToArray, checking if it is empty is preferred.
				\return True if there are any entites matchine the query. False otherwise.
				*/
			bool HasItems() {
				// Make sure the query was created by World.CreateQuery()
				GAIA_ASSERT(m_entityQueryCache != nullptr);

				bool hasItems = false;

				auto& queryInfo = FetchQueryInfo();

				const bool hasFilters = queryInfo.HasFilters();

				auto execWithFiltersON = [&](const auto& chunksList) {
					for (auto* pChunk: chunksList) {
						if (!CanAcceptChunkForProcessing<true>(*pChunk, queryInfo))
							continue;
						hasItems = true;
						break;
					}
				};

				auto execWithFiltersOFF = [&](const auto& chunksList) {
					for (auto* pChunk: chunksList) {
						if (!CanAcceptChunkForProcessing<false>(*pChunk, queryInfo))
							continue;
						hasItems = true;
						break;
					}
				};

				if (hasItems) {
					if (hasFilters) {
						for (auto* pArchetype: queryInfo) {
							if (CheckConstraints<true>()) {
								execWithFiltersON(pArchetype->GetChunks());
								break;
							}
							if (CheckConstraints<false>()) {
								execWithFiltersON(pArchetype->GetChunksDisabled());
								break;
							}
						}
					} else {
						for (auto* pArchetype: queryInfo) {
							if (CheckConstraints<true>()) {
								execWithFiltersOFF(pArchetype->GetChunks());
								break;
							}
							if (CheckConstraints<false>()) {
								execWithFiltersOFF(pArchetype->GetChunksDisabled());
								break;
							}
						}
					}

				} else {
					if (hasFilters) {
						for (auto* pArchetype: queryInfo) {
							if (CheckConstraints<true>())
								execWithFiltersON(pArchetype->GetChunks());
							if (CheckConstraints<false>())
								execWithFiltersON(pArchetype->GetChunksDisabled());
						}
					} else {
						for (auto* pArchetype: queryInfo) {
							if (CheckConstraints<true>())
								execWithFiltersOFF(pArchetype->GetChunks());
							if (CheckConstraints<false>())
								execWithFiltersOFF(pArchetype->GetChunksDisabled());
						}
					}
				}

				return hasItems;
			}

			/*!
			Returns the number of entities matching the query
			\warning Only use if you only care about the number of entities matching the query.
							 The result is not cached and repeated calls to the function might be slow.
							 If you already called ToArray, use the size provided by the array.
			\return The number of matching entities
			*/
			size_t CalculateItemCount() {
				// Make sure the query was created by World.CreateQuery()
				GAIA_ASSERT(m_entityQueryCache != nullptr);

				size_t itemCount = 0;

				auto& queryInfo = FetchQueryInfo();

				const bool hasFilters = queryInfo.HasFilters();

				auto execWithFiltersON = [&](const auto& chunksList) {
					for (auto* pChunk: chunksList) {
						if (CanAcceptChunkForProcessing<true>(*pChunk, queryInfo))
							itemCount += pChunk->GetEntityCount();
					}
				};

				auto execWithFiltersOFF = [&](const auto& chunksList) {
					for (auto* pChunk: chunksList) {
						if (CanAcceptChunkForProcessing<false>(*pChunk, queryInfo))
							itemCount += pChunk->GetEntityCount();
					}
				};

				if (hasFilters) {
					for (auto* pArchetype: queryInfo) {
						if (CheckConstraints<true>())
							execWithFiltersON(pArchetype->GetChunks());
						if (CheckConstraints<false>())
							execWithFiltersON(pArchetype->GetChunksDisabled());
					}
				} else {
					for (auto* pArchetype: queryInfo) {
						if (CheckConstraints<true>())
							execWithFiltersOFF(pArchetype->GetChunks());
						if (CheckConstraints<false>())
							execWithFiltersOFF(pArchetype->GetChunksDisabled());
					}
				}

				return itemCount;
			}

			/*!
			Returns an array of components or entities matching the query
			\tparam Container Container storing entities or components
			\return Array with entities or components
			*/
			template <typename Container>
			void ToArray(Container& outArray) {
				using ContainerItemType = typename Container::value_type;

				// Make sure the query was created by World.CreateQuery()
				GAIA_ASSERT(m_entityQueryCache != nullptr);

				const size_t itemCount = CalculateItemCount();
				outArray.reserve(itemCount);

				auto& queryInfo = FetchQueryInfo();

				const bool hasFilters = queryInfo.HasFilters();

				auto addChunk = [&](Chunk* pChunk) {
					const auto componentView = pChunk->template View<ContainerItemType>();
					for (size_t i = 0; i < pChunk->GetEntityCount(); ++i)
						outArray.push_back(componentView[i]);
				};

				auto execWithFiltersON = [&](const auto& chunksList) {
					for (auto* pChunk: chunksList) {
						if (CanAcceptChunkForProcessing<true>(*pChunk, queryInfo))
							addChunk(pChunk);
					}
				};

				auto execWithFiltersOFF = [&](const auto& chunksList) {
					for (auto* pChunk: chunksList) {
						if (CanAcceptChunkForProcessing<false>(*pChunk, queryInfo))
							addChunk(pChunk);
					}
				};

				if (hasFilters) {
					for (auto* pArchetype: *m_archetypes) {
						if (CheckConstraints<true>())
							execWithFiltersON(pArchetype->GetChunks());
						if (CheckConstraints<false>())
							execWithFiltersON(pArchetype->GetChunksDisabled());
					}
				} else {
					for (auto* pArchetype: *m_archetypes) {
						if (CheckConstraints<true>())
							execWithFiltersOFF(pArchetype->GetChunks());
						if (CheckConstraints<false>())
							execWithFiltersOFF(pArchetype->GetChunksDisabled());
					}
				}
			}

			/*!
			Returns an array of chunks matching the query
			\return Array of chunks
			*/
			template <typename Container>
			void ToChunkArray(Container& outArray) {
				static_assert(std::is_same_v<typename Container::value_type, Chunk*>);

				// Make sure the query was created by World.CreateQuery()
				GAIA_ASSERT(m_entityQueryCache != nullptr);

				const size_t itemCount = CalculateItemCount();
				outArray.reserve(itemCount);

				auto& queryInfo = FetchQueryInfo();

				const bool hasFilters = queryInfo.HasFilters();

				auto execWithFiltersON = [&](const auto& chunksList) {
					for (auto* pChunk: chunksList) {
						if (CanAcceptChunkForProcessing<true>(*pChunk, queryInfo))
							outArray.push_back(pChunk);
					}
				};

				auto execWithFiltersOFF = [&](const auto& chunksList) {
					for (auto* pChunk: chunksList) {
						if (CanAcceptChunkForProcessing<false>(*pChunk, queryInfo))
							outArray.push_back(pChunk);
					}
				};

				if (hasFilters) {
					for (auto* pArchetype: queryInfo) {
						if (CheckConstraints<true>())
							execWithFiltersON(pArchetype->GetChunks());
						if (CheckConstraints<false>())
							execWithFiltersON(pArchetype->GetChunksDisabled());
					}
				} else {
					for (auto* pArchetype: queryInfo) {
						if (CheckConstraints<true>())
							execWithFiltersOFF(pArchetype->GetChunks());
						if (CheckConstraints<false>())
							execWithFiltersOFF(pArchetype->GetChunksDisabled());
					}
				}
			}
		};
	} // namespace ecs
} // namespace gaia
