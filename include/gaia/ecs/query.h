#pragma once
#include "../config/config.h"

#include <tuple>
#include <type_traits>

#include "../config/profiler.h"
#include "../containers/darray.h"
#include "../containers/map.h"
#include "../containers/sarray_ext.h"
#include "../mt/jobhandle.h"
#include "../utils/hashing_policy.h"
#include "../utils/serialization.h"
#include "../utils/utility.h"
#include "archetype.h"
#include "archetype_common.h"
#include "chunk.h"
#include "common.h"
#include "component.h"
#include "component_cache.h"
#include "component_utils.h"
#include "data_buffer.h"
#include "iterator.h"
#include "iterator_disabled.h"
#include "iterator_enabled.h"
#include "query_cache.h"
#include "query_common.h"
#include "query_info.h"

namespace gaia {
	namespace ecs {
		enum class ExecutionMode : uint8_t {
			//! Run on the main thread
			Run,
			//! Run on a single worker thread
			Single,
			//! Run on as many worker threads as possible
			Parallel
		};

		class Query final {
			static constexpr uint32_t ChunkBatchSize = 16;
			using CChunkSpan = std::span<const archetype::Chunk*>;
			using ChunkBatchedList = containers::sarray_ext<archetype::Chunk*, ChunkBatchSize>;
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

				void Exec(query::LookupCtx& ctx) const {
					auto& data = ctx.data[componentType];
					auto& componentIds = data.componentIds;
					auto& lastMatchedArchetypeIndex = data.lastMatchedArchetypeIndex;
					auto& rules = data.rules;

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

					data.readWriteMask |= (uint8_t)isReadWrite << (uint8_t)componentIds.size();
					componentIds.push_back(componentId);
					lastMatchedArchetypeIndex.push_back(0);
					rules.push_back(listType);

					if (listType == query::ListType::LT_All)
						++data.rulesAllCount;
				}
			};

			struct Command_Filter {
				static constexpr CommandBufferCmd Id = CommandBufferCmd::ADD_FILTER;

				component::ComponentId componentId;
				component::ComponentType componentType;

				void Exec(query::LookupCtx& ctx) const {
					auto& data = ctx.data[componentType];
					auto& componentIds = data.componentIds;
					auto& withChanged = data.withChanged;
					const auto& rules = data.rules;

					GAIA_ASSERT(utils::has(componentIds, componentId));
					GAIA_ASSERT(!utils::has(withChanged, componentId));

#if GAIA_DEBUG
					// There's a limit to the amount of components which we can store
					if (withChanged.size() >= query::MAX_COMPONENTS_IN_QUERY) {
						GAIA_ASSERT(false && "Trying to create an ECS filter query with too many components!");

						const auto& cc = ComponentCache::Get();
						auto componentName = cc.GetComponentDesc(componentId).name;
						GAIA_LOG_E(
								"Trying to add ECS component %.*s to an already full filter query!", (uint32_t)componentName.size(),
								componentName.data());
						return;
					}
#endif

					const auto componentIdx = utils::get_index_unsafe(componentIds, componentId);

					// Component has to be present in anyList or allList.
					// NoneList makes no sense because we skip those in query processing anyway.
					if (rules[componentIdx] != query::ListType::LT_None) {
						withChanged.push_back(componentId);
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
						DataBuffer_SerializationWrapper s(buffer);
						serialization::load(s, cmd);
						cmd.Exec(ctx);
					},
					// Add filter
					[](DataBuffer& buffer, query::LookupCtx& ctx) {
						Command_Filter cmd;
						DataBuffer_SerializationWrapper s(buffer);
						serialization::load(s, cmd);
						cmd.Exec(ctx);
					}};

			DataBuffer m_cmdBuffer;
			//! Query cache id
			query::QueryId m_queryId = query::QueryIdBad;

			//! Query cache (stable pointer to parent world's query cache)
			QueryCache* m_entityQueryCache{};
			//! World version (stable pointer to parent world's world version)
			uint32_t* m_worldVersion{};
			//! List of achetypes (stable pointer to parent world's archetype array)
			const archetype::ArchetypeList* m_archetypes{};
			//! Map of component ids to archetypes (stable pointer to parent world's archetype component-to-archetype map)
			const query::ComponentToArchetypeMap* m_componentToArchetypeMap{};
			//! Execution mode
			ExecutionMode m_executionMode = ExecutionMode::Run;

			//--------------------------------------------------------------------------------
		public:
			query::QueryInfo& FetchQueryInfo() {
				// Lookup hash is present which means QueryInfo was already found
				if GAIA_LIKELY (m_queryId != query::QueryIdBad) {
					auto& queryInfo = m_entityQueryCache->Get(m_queryId);
					queryInfo.Match(*m_componentToArchetypeMap, (uint32_t)m_archetypes->size());
					return queryInfo;
				}

				// No lookup hash is present which means QueryInfo needes to fetched or created
				query::LookupCtx ctx;
				Commit(ctx);
				m_queryId = m_entityQueryCache->GetOrCreate(std::move(ctx));
				auto& queryInfo = m_entityQueryCache->Get(m_queryId);
				queryInfo.Match(*m_componentToArchetypeMap, (uint32_t)m_archetypes->size());
				return queryInfo;
			}

			//--------------------------------------------------------------------------------
		private:
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

				Command_AddComponent cmd{componentId, componentType, listType, isReadWrite};
				DataBuffer_SerializationWrapper s(m_cmdBuffer);
				serialization::save(s, Command_AddComponent::Id);
				serialization::save(s, cmd);
			}

			template <typename T>
			void WithChanged_Internal() {
				const auto componentId = component::GetComponentId<T>();
				constexpr auto componentType = component::IsGenericComponent<T> ? component::ComponentType::CT_Generic
																																				: component::ComponentType::CT_Chunk;

				Command_Filter cmd{componentId, componentType};
				DataBuffer_SerializationWrapper s(m_cmdBuffer);
				serialization::save(s, Command_Filter::Id);
				serialization::save(s, cmd);
			}

			//--------------------------------------------------------------------------------

			void Commit(query::LookupCtx& ctx) {
				GAIA_ASSERT(m_queryId == query::QueryIdBad);
				DataBuffer_SerializationWrapper s(m_cmdBuffer);

				// Read data from buffer and execute the command stored in it
				m_cmdBuffer.Seek(0);
				while (m_cmdBuffer.GetPos() < m_cmdBuffer.Size()) {
					CommandBufferCmd id{};
					serialization::load(s, id);
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
			void UnpackArgsIntoQuery_All(Query& query, [[maybe_unused]] utils::func_type_list<T...> types) const {
				static_assert(sizeof...(T) > 0, "Inputs-less functors can not be unpacked to query");
				query.All<T...>();
			}

			//! Unpacks the parameter list \param types into query \param query and performs HasAll for each of them
			template <typename... T>
			GAIA_NODISCARD bool UnpackArgsIntoQuery_HasAll(
					const query::QueryInfo& queryInfo, [[maybe_unused]] utils::func_type_list<T...> types) const {
				if constexpr (sizeof...(T) > 0)
					return queryInfo.HasAll<T...>();
				else
					return true;
			}

			//--------------------------------------------------------------------------------

			GAIA_NODISCARD static bool CheckFilters(const archetype::Chunk& chunk, const query::QueryInfo& queryInfo) {
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
				// Let us be conservative for now and go with T2. That means we will try to keep our data at
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
			void ProcessQueryOnChunks_NoConstraints(
					Func func, ChunkBatchedList& chunkBatch, const containers::darray<archetype::Chunk*>& chunks,
					const query::QueryInfo& queryInfo) {
				size_t chunkOffset = 0;
				size_t itemsLeft = chunks.size();
				while (itemsLeft > 0) {
					const size_t maxBatchSize = chunkBatch.max_size() - chunkBatch.size();
					const size_t batchSize = itemsLeft > maxBatchSize ? maxBatchSize : itemsLeft;

					CChunkSpan chunkSpan((const archetype::Chunk**)&chunks[chunkOffset], batchSize);
					for (const auto* pChunk: chunkSpan) {
						if (!pChunk->HasEntities())
							continue;
						if constexpr (HasFilters) {
							if (!CheckFilters(*pChunk, queryInfo))
								continue;
						}

						chunkBatch.push_back(const_cast<archetype::Chunk*>(pChunk));
					}

					if GAIA_UNLIKELY (chunkBatch.size() == chunkBatch.max_size())
						ChunkBatch_Perform(func, chunkBatch);

					itemsLeft -= batchSize;
					chunkOffset += batchSize;
				}
			}

			template <bool HasFilters, typename Func>
			void ProcessQueryOnChunks(
					Func func, ChunkBatchedList& chunkBatch, const containers::darray<archetype::Chunk*>& chunks,
					const query::QueryInfo& queryInfo, bool enabledOnly) {
				size_t chunkOffset = 0;
				size_t itemsLeft = chunks.size();
				while (itemsLeft > 0) {
					const size_t maxBatchSize = chunkBatch.max_size() - chunkBatch.size();
					const size_t batchSize = itemsLeft > maxBatchSize ? maxBatchSize : itemsLeft;

					CChunkSpan chunkSpan((const archetype::Chunk**)&chunks[chunkOffset], batchSize);
					for (const auto* pChunk: chunkSpan) {
						if (!pChunk->HasEntities())
							continue;
						if (enabledOnly == pChunk->HasDisabledEntities()) {
							const auto maskCnt = pChunk->GetDisabledEntityMask().count();

							// We want enabled entities but there are only disables ones on the chunk
							if (enabledOnly && maskCnt == pChunk->GetEntityCount())
								continue;
							// We want disabled entities but there are only enabled ones on the chunk
							if (!enabledOnly && maskCnt == 0)
								continue;
						}
						if constexpr (HasFilters) {
							if (!CheckFilters(*pChunk, queryInfo))
								continue;
						}

						chunkBatch.push_back(const_cast<archetype::Chunk*>(pChunk));
					}

					if GAIA_UNLIKELY (chunkBatch.size() == chunkBatch.max_size())
						ChunkBatch_Perform(func, chunkBatch);

					itemsLeft -= batchSize;
					chunkOffset += batchSize;
				}
			}

			template <typename Func>
			void ForEach_RunQueryOnChunks(query::QueryInfo& queryInfo, Query::Constraints constraints, Func func) {
				// Update the world version
				UpdateVersion(*m_worldVersion);

				ChunkBatchedList chunkBatch;

				const bool hasFilters = queryInfo.HasFilters();
				if (hasFilters) {
					// Evaluation defaults to EnabledOnly changes. AcceptAll is something that has to be asked for explicitely
					if GAIA_UNLIKELY (constraints == Query::Constraints::AcceptAll) {
						for (auto* pArchetype: queryInfo)
							ProcessQueryOnChunks_NoConstraints<true>(func, chunkBatch, pArchetype->GetChunks(), queryInfo);
					} else {
						const bool enabledOnly = constraints == Query::Constraints::EnabledOnly;
						for (auto* pArchetype: queryInfo)
							ProcessQueryOnChunks<true>(func, chunkBatch, pArchetype->GetChunks(), queryInfo, enabledOnly);
					}
				} else {
					if GAIA_UNLIKELY (constraints == Query::Constraints::AcceptAll) {
						for (auto* pArchetype: queryInfo)
							ProcessQueryOnChunks_NoConstraints<false>(func, chunkBatch, pArchetype->GetChunks(), queryInfo);
					} else {
						const bool enabledOnly = constraints == Query::Constraints::EnabledOnly;
						for (auto* pArchetype: queryInfo)
							ProcessQueryOnChunks<false>(func, chunkBatch, pArchetype->GetChunks(), queryInfo, enabledOnly);
					}
				}

				if (!chunkBatch.empty())
					ChunkBatch_Perform(func, chunkBatch);

				// Update the query version with the current world's version
				queryInfo.SetWorldVersion(*m_worldVersion);
			}

			template <typename Func>
			void ForEach_Internal(query::QueryInfo& queryInfo, Func func) {
				using InputArgs = decltype(utils::func_args(&Func::operator()));

#if GAIA_DEBUG
				// Make sure we only use components specified in the query
				GAIA_ASSERT(UnpackArgsIntoQuery_HasAll(queryInfo, InputArgs{}));
#endif

				ForEach_RunQueryOnChunks(queryInfo, Query::Constraints::EnabledOnly, [&](archetype::Chunk& chunk) {
					chunk.ForEach(InputArgs{}, func);
				});
			}

			void InvalidateQuery() {
				m_queryId = query::QueryIdBad;
			}

		public:
			Query() = default;
			Query(
					QueryCache& queryCache, uint32_t& worldVersion, const containers::darray<archetype::Archetype*>& archetypes,
					const query::ComponentToArchetypeMap& componentToArchetypeMap):
					m_entityQueryCache(&queryCache),
					m_worldVersion(&worldVersion), m_archetypes(&archetypes),
					m_componentToArchetypeMap(&componentToArchetypeMap) {}

			GAIA_NODISCARD uint32_t GetQueryId() const {
				return m_queryId;
			}

			template <typename... T>
			Query& All() {
				// Adding new rules invalidates the query
				InvalidateQuery();
				// Add commands to the command buffer
				(AddComponent_Internal<T>(query::ListType::LT_All), ...);
				return *this;
			}

			template <typename... T>
			Query& Any() {
				// Adding new rules invalidates the query
				InvalidateQuery();
				// Add commands to the command buffer
				(AddComponent_Internal<T>(query::ListType::LT_Any), ...);
				return *this;
			}

			template <typename... T>
			Query& None() {
				// Adding new rules invalidates the query
				InvalidateQuery();
				// Add commands to the command buffer
				(AddComponent_Internal<T>(query::ListType::LT_None), ...);
				return *this;
			}

			template <typename... T>
			Query& WithChanged() {
				// Adding new rules invalidates the query
				InvalidateQuery();
				// Add commands to the command buffer
				(WithChanged_Internal<T>(), ...);
				return *this;
			}

			GAIA_NODISCARD static bool
			CheckConstraintsEnabledDisabledOnly(ecs::Query::Constraints constraints, bool filterEnabledOnly) {
				const bool arr[2] = {// EnabledOnly
														 true,
														 // DisabledOnly
														 false};
				return filterEnabledOnly == arr[(int)constraints];
			}

			GAIA_NODISCARD static bool CheckConstraints(ecs::Query::Constraints constraints, bool filterEnabledOnly) {
				const bool arr[2] = {// EnabledOnly
														 true,
														 // DisabledOnly
														 false};
				return constraints == Query::Constraints::AcceptAll || filterEnabledOnly == arr[(int)constraints];
			}

			Query& Schedule() {
				m_executionMode = ExecutionMode::Single;
				return *this;
			}

			Query& ScheduleParallel() {
				m_executionMode = ExecutionMode::Parallel;
				return *this;
			}

			template <typename Func>
			void ForEach(Func func) {
				// Make sure the query was created by World.CreateQuery()
				GAIA_ASSERT(m_entityQueryCache != nullptr);

				auto& queryInfo = FetchQueryInfo();

				if constexpr (std::is_invocable<Func, Iterator>::value)
					ForEach_RunQueryOnChunks(queryInfo, Query::Constraints::AcceptAll, [&](archetype::Chunk& chunk) {
						func(Iterator(queryInfo, chunk));
					});
				else if constexpr (std::is_invocable<Func, IteratorEnabled>::value)
					ForEach_RunQueryOnChunks(queryInfo, Query::Constraints::EnabledOnly, [&](archetype::Chunk& chunk) {
						func(IteratorEnabled(queryInfo, chunk));
					});
				else if constexpr (std::is_invocable<Func, IteratorDisabled>::value)
					ForEach_RunQueryOnChunks(queryInfo, Query::Constraints::DisabledOnly, [&](archetype::Chunk& chunk) {
						func(IteratorDisabled(queryInfo, chunk));
					});
				else
					ForEach_Internal(queryInfo, func);
			}

			template <typename Func>
			void ForEach(query::QueryId queryId, Func func) {
				// Make sure the query was created by World.CreateQuery()
				GAIA_ASSERT(m_entityQueryCache != nullptr);

				auto& queryInfo = m_entityQueryCache->Get(queryId);
				ForEach_Internal(queryInfo, func);
			}

			/*!
				Returns true or false depending on whether there are entities matching the query.
				\warning Only use if you only care if there are any entities matching the query.
								 The result is not cached and repeated calls to the function might be slow.
								 If you already called ToArray, checking if it is empty is preferred.
				\return True if there are any entites matchine the query. False otherwise.
				*/
			bool HasEntities(ecs::Query::Constraints constraints = ecs::Query::Constraints::EnabledOnly) {
				// Make sure the query was created by World.CreateQuery()
				GAIA_ASSERT(m_entityQueryCache != nullptr);

				auto& queryInfo = FetchQueryInfo();
				const bool hasFilters = queryInfo.HasFilters();

				if (hasFilters) {
					auto execWithFiltersON = [&](const auto& chunks) {
						return utils::has_if(chunks, [&](archetype::Chunk* pChunk) {
							if (!pChunk->HasEntities())
								return false;
							return CheckFilters(*pChunk, queryInfo);
						});
					};

					auto execWithFiltersON_EnabledDisabled = [&](const auto& chunks, bool enabledOnly) {
						return utils::has_if(chunks, [&](archetype::Chunk* pChunk) {
							const auto hasEntities = enabledOnly ? pChunk->GetEntityCount() != pChunk->GetDisabledEntityMask().count()
																									 : pChunk->GetDisabledEntityMask().count() > 0;
							if (!hasEntities)
								return false;

							return CheckFilters(*pChunk, queryInfo);
						});
					};

					if (constraints == Query::Constraints::AcceptAll) {
						for (auto* pArchetype: queryInfo) {
							if (execWithFiltersON(pArchetype->GetChunks()))
								return true;
						}
					} else {
						for (auto* pArchetype: queryInfo) {
							if (execWithFiltersON_EnabledDisabled(
											pArchetype->GetChunks(), constraints == Query::Constraints::EnabledOnly))
								return true;
						}
					}
				} else {
					auto execWithFiltersOFF = [&](const auto& chunks) {
						return utils::has_if(chunks, [&](archetype::Chunk* pChunk) {
							return pChunk->HasEntities();
						});
					};

					auto execWithFiltersOFF_EnabledDisabled = [&](const auto& chunks, bool enabledOnly) {
						return utils::has_if(chunks, [&](archetype::Chunk* pChunk) {
							return enabledOnly ? pChunk->GetEntityCount() != pChunk->GetDisabledEntityMask().count()
																 : pChunk->GetDisabledEntityMask().count() > 0;
						});
					};

					if (constraints == Query::Constraints::AcceptAll) {
						for (auto* pArchetype: queryInfo) {
							if (execWithFiltersOFF(pArchetype->GetChunks()))
								return true;
						}
					} else {
						for (auto* pArchetype: queryInfo) {
							if (execWithFiltersOFF_EnabledDisabled(
											pArchetype->GetChunks(), constraints == Query::Constraints::EnabledOnly))
								return true;
						}
					}
				}

				return false;
			}

			/*!
			Returns the number of entities matching the query
			\warning Only use if you only care about the number of entities matching the query.
							 The result is not cached and repeated calls to the function might be slow.
							 If you already called ToArray, use the size provided by the array.
			\return The number of matching entities
			*/
			size_t CalculateEntityCount(ecs::Query::Constraints constraints = ecs::Query::Constraints::EnabledOnly) {
				// Make sure the query was created by World.CreateQuery()
				GAIA_ASSERT(m_entityQueryCache != nullptr);

				auto& queryInfo = FetchQueryInfo();
				const bool hasFilters = queryInfo.HasFilters();
				size_t entityCount = 0;

				if (hasFilters) {
					auto execWithFiltersON = [&](const auto& chunks) {
						size_t cnt = 0;
						for (auto* pChunk: chunks) {
							if (!pChunk->HasEntities())
								continue;
							if (!CheckFilters(*pChunk, queryInfo))
								continue;
							cnt += pChunk->GetEntityCount();
						};
						return cnt;
					};

					auto execWithFiltersON_EnabledDisabled = [&](const auto& chunks, bool enabledOnly) {
						size_t cnt = 0;
						for (auto* pChunk: chunks) {
							if (!pChunk->HasEntities())
								continue;
							if (!CheckFilters(*pChunk, queryInfo))
								continue;
							if (enabledOnly)
								cnt += pChunk->GetEntityCount() - pChunk->GetDisabledEntityMask().count();
							else
								cnt += pChunk->GetDisabledEntityMask().count();
						};
						return cnt;
					};

					if (constraints == Query::Constraints::AcceptAll) {
						for (auto* pArchetype: queryInfo)
							entityCount += execWithFiltersON(pArchetype->GetChunks());
					} else {
						for (auto* pArchetype: queryInfo)
							entityCount += execWithFiltersON_EnabledDisabled(
									pArchetype->GetChunks(), constraints == Query::Constraints::EnabledOnly);
					}
				} else {
					auto execWithFiltersOFF = [&](const auto& chunks) {
						size_t cnt = 0;
						for (auto* pChunk: chunks) {
							if (!pChunk->HasEntities())
								continue;
							cnt += pChunk->GetEntityCount();
						};
						return cnt;
					};

					auto execWithFiltersOFF_EnabledDisabled = [&](const auto& chunks, bool enabledOnly) {
						size_t cnt = 0;
						for (auto* pChunk: chunks) {
							if (!pChunk->HasEntities())
								continue;
							if (enabledOnly)
								cnt += pChunk->GetEntityCount() - pChunk->GetDisabledEntityMask().count();
							else
								cnt += pChunk->GetDisabledEntityMask().count();
						};
						return cnt;
					};

					if (constraints == Query::Constraints::AcceptAll) {
						for (auto* pArchetype: queryInfo)
							entityCount += execWithFiltersOFF(pArchetype->GetChunks());
					} else {
						for (auto* pArchetype: queryInfo)
							entityCount += execWithFiltersOFF_EnabledDisabled(
									pArchetype->GetChunks(), constraints == Query::Constraints::EnabledOnly);
					}
				}

				return entityCount;
			}

			/*!
			Appends all components or entities matching the query to the array
			\tparam outArray Container storing entities or components
			\param constraints Query constraints
			\return Array with entities or components
			*/
			template <typename Container>
			void ToArray(Container& outArray, ecs::Query::Constraints constraints = ecs::Query::Constraints::EnabledOnly) {
				using ContainerItemType = typename Container::value_type;

				// Make sure the query was created by World.CreateQuery()
				GAIA_ASSERT(m_entityQueryCache != nullptr);

				const auto entityCount = CalculateEntityCount();
				if (entityCount == 0)
					return;

				outArray.reserve(entityCount);
				auto& queryInfo = FetchQueryInfo();
				const bool hasFilters = queryInfo.HasFilters();

				auto addChunk = [&](archetype::Chunk* pChunk) {
					const auto componentView = pChunk->template View<ContainerItemType>();
					for (size_t i = 0; i < pChunk->GetEntityCount(); ++i)
						outArray.push_back(componentView[i]);
				};

				if (hasFilters) {
					auto execWithFiltersON = [&](const auto& chunks) {
						for (auto* pChunk: chunks) {
							if (!pChunk->HasEntities())
								continue;
							if (!CheckFilters(*pChunk, queryInfo))
								continue;
							addChunk(pChunk);
						};
					};

					auto execWithFiltersON_EnabledDisabled = [&](const auto& chunks, bool enabledOnly) {
						for (auto* pChunk: chunks) {
							if (!pChunk->HasEntities())
								continue;
							if (CheckConstraintsEnabledDisabledOnly(constraints, enabledOnly) == pChunk->HasDisabledEntities())
								continue;
							if (!CheckFilters(*pChunk, queryInfo))
								continue;
							addChunk(pChunk);
						};
					};

					if (constraints == Query::Constraints::AcceptAll) {
						for (auto* pArchetype: queryInfo)
							execWithFiltersON(pArchetype->GetChunks());
					} else {
						for (auto* pArchetype: queryInfo)
							execWithFiltersON_EnabledDisabled(
									pArchetype->GetChunks(), constraints == Query::Constraints::EnabledOnly);
					}
				} else {
					auto execWithFiltersOFF = [&](const auto& chunks) {
						for (auto* pChunk: chunks) {
							if (!pChunk->HasEntities())
								continue;
							addChunk(pChunk);
						};
					};

					auto execWithFiltersOFF_EnabledDisabled = [&](const auto& chunks, bool enabledOnly) {
						for (auto* pChunk: chunks) {
							if (!pChunk->HasEntities())
								continue;
							if (CheckConstraintsEnabledDisabledOnly(constraints, enabledOnly) == pChunk->HasDisabledEntities())
								continue;
							addChunk(pChunk);
						};
					};

					if (constraints == Query::Constraints::AcceptAll) {
						for (auto* pArchetype: queryInfo)
							execWithFiltersOFF(pArchetype->GetChunks());
					} else {
						for (auto* pArchetype: queryInfo)
							execWithFiltersOFF_EnabledDisabled(
									pArchetype->GetChunks(), constraints == Query::Constraints::EnabledOnly);
					}
				}
			}
		};
	} // namespace ecs
} // namespace gaia
