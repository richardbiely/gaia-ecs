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
		namespace detail {
			template <bool Cached>
			struct QueryImplStorage {
				//! QueryImpl cache id
				query::QueryId m_queryId = query::QueryIdBad;
				//! QueryImpl cache (stable pointer to parent world's query cache)
				QueryCache* m_entityQueryCache{};
			};

			template <>
			struct QueryImplStorage<false> {
				query::QueryInfo m_queryInfo;
			};

			template <bool UseCaching = true>
			class QueryImpl final {
				static constexpr uint32_t ChunkBatchSize = 16;
				using CChunkSpan = std::span<const archetype::Chunk*>;
				using ChunkBatchedList = containers::sarray_ext<archetype::Chunk*, ChunkBatchSize>;
				using CmdBufferCmdFunc = void (*)(DataBuffer& buffer, query::LookupCtx& ctx);

			public:
				//! QueryImpl constraints
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

				//! Storage for data based on whether Caching is used or not
				QueryImplStorage<UseCaching> m_storage;
				//! Buffer with commands used to fetch the QueryInfo
				DataBuffer m_cmdBuffer;
				//! World version (stable pointer to parent world's world version)
				uint32_t* m_worldVersion{};
				//! List of achetypes (stable pointer to parent world's archetype array)
				const archetype::ArchetypeList* m_archetypes{};
				//! Map of component ids to archetypes (stable pointer to parent world's archetype component-to-archetype map)
				const query::ComponentToArchetypeMap* m_componentToArchetypeMap{};
				//! Execution mode
				query::ExecutionMode m_executionMode = query::ExecutionMode::Run;

				//--------------------------------------------------------------------------------
			public:
				query::QueryInfo& FetchQueryInfo() {
					if constexpr (UseCaching) {
						// Make sure the query was created by World.CreateQuery()
						GAIA_ASSERT(m_storage.m_entityQueryCache != nullptr);

						// Lookup hash is present which means QueryInfo was already found
						if GAIA_LIKELY (m_storage.m_queryId != query::QueryIdBad) {
							auto& queryInfo = m_storage.m_entityQueryCache->Get(m_storage.m_queryId);
							queryInfo.Match(*m_componentToArchetypeMap, (uint32_t)m_archetypes->size());
							return queryInfo;
						}

						// No lookup hash is present which means QueryInfo needs to fetched or created
						query::LookupCtx ctx;
						Commit(ctx);
						m_storage.m_queryId = m_storage.m_entityQueryCache->GetOrCreate(std::move(ctx));
						auto& queryInfo = m_storage.m_entityQueryCache->Get(m_storage.m_queryId);
						queryInfo.Match(*m_componentToArchetypeMap, (uint32_t)m_archetypes->size());
						return queryInfo;
					} else {
						if GAIA_UNLIKELY (m_storage.m_queryInfo.GetId() == query::QueryIdBad) {
							query::LookupCtx ctx;
							Commit(ctx);
							m_storage.m_queryInfo = query::QueryInfo::Create(query::QueryId{}, std::move(ctx));
						}
						m_storage.m_queryInfo.Match(*m_componentToArchetypeMap, (uint32_t)m_archetypes->size());
						return m_storage.m_queryInfo;
					}
				}

				//--------------------------------------------------------------------------------
			private:
				template <typename T>
				void AddComponent_Internal(query::ListType listType) {
					using U = typename component::component_type_t<T>::Type;
					using UOriginal = typename component::component_type_t<T>::TypeOriginal;
					using UOriginalPR = std::remove_reference_t<std::remove_pointer_t<UOriginal>>;

					const auto componentId = component::GetComponentId<T>();
					constexpr auto componentType = component::component_type_v<T>;
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
					constexpr auto componentType = component::component_type_v<T>;

					Command_Filter cmd{componentId, componentType};
					DataBuffer_SerializationWrapper s(m_cmdBuffer);
					serialization::save(s, Command_Filter::Id);
					serialization::save(s, cmd);
				}

				//--------------------------------------------------------------------------------

				void Commit(query::LookupCtx& ctx) {
#if GAIA_ASSERT_ENABLED
					if constexpr (UseCaching) {
						GAIA_ASSERT(m_storage.m_queryId == query::QueryIdBad);
					} else {
						GAIA_ASSERT(m_storage.m_queryInfo.GetId() == query::QueryIdBad);
					}
#endif

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
				void UnpackArgsIntoQuery_All(QueryImpl& query, [[maybe_unused]] utils::func_type_list<T...> types) const {
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

					uint32_t chunkIdx = 1;
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
					uint32_t chunkOffset = 0;
					uint32_t itemsLeft = chunks.size();
					while (itemsLeft > 0) {
						const auto maxBatchSize = chunkBatch.max_size() - chunkBatch.size();
						const auto batchSize = itemsLeft > maxBatchSize ? maxBatchSize : itemsLeft;

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
					uint32_t chunkOffset = 0;
					uint32_t itemsLeft = chunks.size();
					while (itemsLeft > 0) {
						const auto maxBatchSize = chunkBatch.max_size() - chunkBatch.size();
						const auto batchSize = itemsLeft > maxBatchSize ? maxBatchSize : itemsLeft;

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
				void ForEach_RunQueryOnChunks(query::QueryInfo& queryInfo, Constraints constraints, Func func) {
					// Update the world version
					UpdateVersion(*m_worldVersion);

					ChunkBatchedList chunkBatch;

					const bool hasFilters = queryInfo.HasFilters();
					if (hasFilters) {
						// Evaluation defaults to EnabledOnly changes. AcceptAll is something that has to be asked for explicitely
						if GAIA_UNLIKELY (constraints == Constraints::AcceptAll) {
							for (auto* pArchetype: queryInfo)
								ProcessQueryOnChunks_NoConstraints<true>(func, chunkBatch, pArchetype->GetChunks(), queryInfo);
						} else {
							const bool enabledOnly = constraints == Constraints::EnabledOnly;
							for (auto* pArchetype: queryInfo)
								ProcessQueryOnChunks<true>(func, chunkBatch, pArchetype->GetChunks(), queryInfo, enabledOnly);
						}
					} else {
						if GAIA_UNLIKELY (constraints == Constraints::AcceptAll) {
							for (auto* pArchetype: queryInfo)
								ProcessQueryOnChunks_NoConstraints<false>(func, chunkBatch, pArchetype->GetChunks(), queryInfo);
						} else {
							const bool enabledOnly = constraints == Constraints::EnabledOnly;
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

					ForEach_RunQueryOnChunks(queryInfo, Constraints::EnabledOnly, [&](archetype::Chunk& chunk) {
						chunk.ForEach(InputArgs{}, func);
					});
				}

				void InvalidateQuery() {
					if constexpr (UseCaching)
						m_storage.m_queryId = query::QueryIdBad;
				}

			public:
				QueryImpl() = default;

				template <bool FuncEnabled = UseCaching, typename std::enable_if<FuncEnabled>::type* = nullptr>
				QueryImpl(
						QueryCache& queryCache, uint32_t& worldVersion, const containers::darray<archetype::Archetype*>& archetypes,
						const query::ComponentToArchetypeMap& componentToArchetypeMap):
						m_worldVersion(&worldVersion),
						m_archetypes(&archetypes), m_componentToArchetypeMap(&componentToArchetypeMap) {
					m_storage.m_entityQueryCache = &queryCache;
				}

				template <bool FuncEnabled = !UseCaching, typename std::enable_if<FuncEnabled>::type* = nullptr>
				QueryImpl(
						uint32_t& worldVersion, const containers::darray<archetype::Archetype*>& archetypes,
						const query::ComponentToArchetypeMap& componentToArchetypeMap):
						m_worldVersion(&worldVersion),
						m_archetypes(&archetypes), m_componentToArchetypeMap(&componentToArchetypeMap) {}

				template <bool FuncEnabled = UseCaching, typename std::enable_if<FuncEnabled>::type* = nullptr>
				GAIA_NODISCARD uint32_t GetQueryId() const {
					return m_storage.m_queryId;
				}

				template <typename... T>
				QueryImpl& All() {
					// Adding new rules invalidates the query
					InvalidateQuery();
					// Add commands to the command buffer
					(AddComponent_Internal<T>(query::ListType::LT_All), ...);
					return *this;
				}

				template <typename... T>
				QueryImpl& Any() {
					// Adding new rules invalidates the query
					InvalidateQuery();
					// Add commands to the command buffer
					(AddComponent_Internal<T>(query::ListType::LT_Any), ...);
					return *this;
				}

				template <typename... T>
				QueryImpl& None() {
					// Adding new rules invalidates the query
					InvalidateQuery();
					// Add commands to the command buffer
					(AddComponent_Internal<T>(query::ListType::LT_None), ...);
					return *this;
				}

				template <typename... T>
				QueryImpl& WithChanged() {
					// Adding new rules invalidates the query
					InvalidateQuery();
					// Add commands to the command buffer
					(WithChanged_Internal<T>(), ...);
					return *this;
				}

				QueryImpl& Schedule() {
					m_executionMode = query::ExecutionMode::Single;
					return *this;
				}

				QueryImpl& ScheduleParallel() {
					m_executionMode = query::ExecutionMode::Parallel;
					return *this;
				}

				template <typename Func>
				void ForEach(Func func) {
					auto& queryInfo = FetchQueryInfo();

					if constexpr (std::is_invocable<Func, Iterator>::value)
						ForEach_RunQueryOnChunks(queryInfo, Constraints::AcceptAll, [&](archetype::Chunk& chunk) {
							func(Iterator(chunk));
						});
					else if constexpr (std::is_invocable<Func, IteratorEnabled>::value)
						ForEach_RunQueryOnChunks(queryInfo, Constraints::EnabledOnly, [&](archetype::Chunk& chunk) {
							func(IteratorEnabled(chunk));
						});
					else if constexpr (std::is_invocable<Func, IteratorDisabled>::value)
						ForEach_RunQueryOnChunks(queryInfo, Constraints::DisabledOnly, [&](archetype::Chunk& chunk) {
							func(IteratorDisabled(chunk));
						});
					else
						ForEach_Internal(queryInfo, func);
				}

				template <typename Func, bool FuncEnabled = UseCaching, typename std::enable_if<FuncEnabled>::type* = nullptr>
				void ForEach(query::QueryId queryId, Func func) {
					// Make sure the query was created by World.CreateQuery()
					GAIA_ASSERT(m_storage.m_entityQueryCache != nullptr);
					GAIA_ASSERT(queryId != query::QueryIdBad);

					auto& queryInfo = m_storage.m_entityQueryCache->Get(queryId);
					ForEach_Internal(queryInfo, func);
				}

				template <bool UseFilters, Constraints c, typename ChunksContainer>
				GAIA_NODISCARD bool HasEntities_Helper(query::QueryInfo& queryInfo, const ChunksContainer& chunks) {
					return utils::has_if(chunks, [&](archetype::Chunk* pChunk) {
						if constexpr (UseFilters) {
							if constexpr (c == Constraints::AcceptAll)
								return pChunk->HasEntities() && CheckFilters(*pChunk, queryInfo);
							else if constexpr (c == Constraints::EnabledOnly)
								return pChunk->GetDisabledEntityMask().count() != pChunk->GetEntityCount() &&
											 CheckFilters(*pChunk, queryInfo);
							else // if constexpr (c == Constraints::DisabledOnly)
								return pChunk->GetDisabledEntityMask().count() > 0 && CheckFilters(*pChunk, queryInfo);
						} else {
							if constexpr (c == Constraints::AcceptAll)
								return pChunk->HasEntities();
							else if constexpr (c == Constraints::EnabledOnly)
								return pChunk->GetDisabledEntityMask().count() != pChunk->GetEntityCount();
							else // if constexpr (c == Constraints::DisabledOnly)
								return pChunk->GetDisabledEntityMask().count() > 0;
						}
					});
				}

				template <bool UseFilters, Constraints c, typename ChunksContainer>
				GAIA_NODISCARD uint32_t
				CalculateEntityCount_Helper(query::QueryInfo& queryInfo, const ChunksContainer& chunks) {
					uint32_t cnt = 0;

					for (auto* pChunk: chunks) {
						if (!pChunk->HasEntities())
							continue;

						// Filters
						if constexpr (UseFilters) {
							if (!CheckFilters(*pChunk, queryInfo))
								continue;
						}

						// Entity count
						if constexpr (c == Constraints::EnabledOnly)
							cnt += pChunk->GetEntityCount() - pChunk->GetDisabledEntityMask().count();
						else if constexpr (c == Constraints::DisabledOnly)
							cnt += pChunk->GetDisabledEntityMask().count();
						else
							cnt += pChunk->GetEntityCount();
					}

					return cnt;
				}

				template <bool UseFilters, Constraints c, typename ChunksContainerIn, typename ChunksContainerOut>
				void ChunkDataToArray_Helper(
						query::QueryInfo& queryInfo, const ChunksContainerIn& chunks, ChunksContainerOut& outArray) {
					using ContainerItemType = typename ChunksContainerOut::value_type;

					for (auto* pChunk: chunks) {
						if (!pChunk->HasEntities())
							continue;

						if constexpr (c == Constraints::EnabledOnly) {
							if (pChunk->HasDisabledEntities())
								continue;
						} else if constexpr (c == Constraints::DisabledOnly) {
							if (!pChunk->HasDisabledEntities())
								continue;
						}

						// Filters
						if constexpr (UseFilters) {
							if (!CheckFilters(*pChunk, queryInfo))
								continue;
						}

						const auto componentView = pChunk->template View<ContainerItemType>();
						for (uint32_t i = 0; i < pChunk->GetEntityCount(); ++i)
							outArray.push_back(componentView[i]);
					}
				}

				/*!
					Returns true or false depending on whether there are entities matching the query.
					\warning Only use if you only care if there are any entities matching the query.
									 The result is not cached and repeated calls to the function might be slow.
									 If you already called ToArray, checking if it is empty is preferred.
					\return True if there are any entites matchine the query. False otherwise.
					*/
				bool HasEntities(Constraints constraints = Constraints::EnabledOnly) {
					auto& queryInfo = FetchQueryInfo();
					const bool hasFilters = queryInfo.HasFilters();

					if (hasFilters) {
						switch (constraints) {
							case Constraints::EnabledOnly: {
								for (auto* pArchetype: queryInfo)
									if (HasEntities_Helper<true, Constraints::EnabledOnly>(queryInfo, pArchetype->GetChunks()))
										return true;
							} break;
							case Constraints::DisabledOnly: {
								for (auto* pArchetype: queryInfo)
									if (HasEntities_Helper<true, Constraints::DisabledOnly>(queryInfo, pArchetype->GetChunks()))
										return true;
							} break;
							case Constraints::AcceptAll: {
								for (auto* pArchetype: queryInfo)
									if (HasEntities_Helper<true, Constraints::AcceptAll>(queryInfo, pArchetype->GetChunks()))
										return true;
							} break;
						}
					} else {
						switch (constraints) {
							case Constraints::EnabledOnly: {
								for (auto* pArchetype: queryInfo)
									if (HasEntities_Helper<false, Constraints::EnabledOnly>(queryInfo, pArchetype->GetChunks()))
										return true;
							} break;
							case Constraints::DisabledOnly: {
								for (auto* pArchetype: queryInfo)
									if (HasEntities_Helper<false, Constraints::DisabledOnly>(queryInfo, pArchetype->GetChunks()))
										return true;
							} break;
							case Constraints::AcceptAll: {
								for (auto* pArchetype: queryInfo)
									if (HasEntities_Helper<false, Constraints::AcceptAll>(queryInfo, pArchetype->GetChunks()))
										return true;
							} break;
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
				uint32_t CalculateEntityCount(Constraints constraints = Constraints::EnabledOnly) {
					auto& queryInfo = FetchQueryInfo();
					uint32_t entityCount = 0;

					const bool hasFilters = queryInfo.HasFilters();
					if (hasFilters) {
						switch (constraints) {
							case Constraints::EnabledOnly: {
								for (auto* pArchetype: queryInfo)
									entityCount +=
											CalculateEntityCount_Helper<true, Constraints::EnabledOnly>(queryInfo, pArchetype->GetChunks());
							} break;
							case Constraints::DisabledOnly: {
								for (auto* pArchetype: queryInfo)
									entityCount +=
											CalculateEntityCount_Helper<true, Constraints::DisabledOnly>(queryInfo, pArchetype->GetChunks());
							} break;
							case Constraints::AcceptAll: {
								for (auto* pArchetype: queryInfo)
									entityCount +=
											CalculateEntityCount_Helper<true, Constraints::AcceptAll>(queryInfo, pArchetype->GetChunks());
							} break;
						}
					} else {
						switch (constraints) {
							case Constraints::EnabledOnly: {
								for (auto* pArchetype: queryInfo)
									entityCount +=
											CalculateEntityCount_Helper<false, Constraints::EnabledOnly>(queryInfo, pArchetype->GetChunks());
							} break;
							case Constraints::DisabledOnly: {
								for (auto* pArchetype: queryInfo)
									entityCount +=
											CalculateEntityCount_Helper<false, Constraints::DisabledOnly>(queryInfo, pArchetype->GetChunks());
							} break;
							case Constraints::AcceptAll: {
								for (auto* pArchetype: queryInfo)
									entityCount +=
											CalculateEntityCount_Helper<false, Constraints::AcceptAll>(queryInfo, pArchetype->GetChunks());
							} break;
						}
					}

					return entityCount;
				}

				/*!
				Appends all components or entities matching the query to the array
				\tparam outArray Container storing entities or components
				\param constraints QueryImpl constraints
				\return Array with entities or components
				*/
				template <typename Container>
				void ToArray(Container& outArray, Constraints constraints = Constraints::EnabledOnly) {
					const auto entityCount = CalculateEntityCount();
					if (entityCount == 0)
						return;

					outArray.reserve(entityCount);
					auto& queryInfo = FetchQueryInfo();

					const bool hasFilters = queryInfo.HasFilters();
					if (hasFilters) {
						switch (constraints) {
							case Constraints::EnabledOnly:
								for (auto* pArchetype: queryInfo)
									ChunkDataToArray_Helper<true, Constraints::EnabledOnly>(queryInfo, pArchetype->GetChunks(), outArray);
								break;
							case Constraints::DisabledOnly:
								for (auto* pArchetype: queryInfo)
									ChunkDataToArray_Helper<true, Constraints::DisabledOnly>(
											queryInfo, pArchetype->GetChunks(), outArray);
								break;
							case Constraints::AcceptAll:
								for (auto* pArchetype: queryInfo)
									ChunkDataToArray_Helper<true, Constraints::AcceptAll>(queryInfo, pArchetype->GetChunks(), outArray);
								break;
						}
					} else {
						switch (constraints) {
							case Constraints::EnabledOnly:
								for (auto* pArchetype: queryInfo)
									ChunkDataToArray_Helper<false, Constraints::EnabledOnly>(
											queryInfo, pArchetype->GetChunks(), outArray);
								break;
							case Constraints::DisabledOnly:
								for (auto* pArchetype: queryInfo)
									ChunkDataToArray_Helper<false, Constraints::DisabledOnly>(
											queryInfo, pArchetype->GetChunks(), outArray);
								break;
							case Constraints::AcceptAll:
								for (auto* pArchetype: queryInfo)
									ChunkDataToArray_Helper<false, Constraints::AcceptAll>(queryInfo, pArchetype->GetChunks(), outArray);
								break;
						}
					}
				}
			};
		} // namespace detail

		using Query = typename detail::QueryImpl<true>;
		using QueryUncached = typename detail::QueryImpl<false>;
	} // namespace ecs
} // namespace gaia
