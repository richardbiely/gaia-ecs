#pragma once
#include "../config/config.h"

#include <type_traits>

#include "../cnt/darray.h"
#include "../cnt/map.h"
#include "../cnt/sarray_ext.h"
#include "../config/profiler.h"
#include "../core/hashing_policy.h"
#include "../core/utility.h"
#include "../mt/jobhandle.h"
#include "../ser/serialization.h"
#include "archetype.h"
#include "archetype_common.h"
#include "chunk.h"
#include "common.h"
#include "component.h"
#include "component_cache.h"
#include "component_utils.h"
#include "data_buffer.h"
#include "iterators.h"
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
				using ChunkBatchedList = cnt::sarray_ext<archetype::Chunk*, ChunkBatchSize>;
				using CmdBufferCmdFunc = void (*)(SerializationBuffer& buffer, query::LookupCtx& ctx);

			public:
				//! QueryImpl constraints
				enum class Constraints : uint8_t { EnabledOnly, DisabledOnly, AcceptAll };

			private:
				//! Command buffer command type
				enum CommandBufferCmd : uint8_t { ADD_COMPONENT, ADD_FILTER };

				struct Command_AddComponent {
					static constexpr CommandBufferCmd Id = CommandBufferCmd::ADD_COMPONENT;

					component::ComponentId componentId;
					component::ComponentType compType;
					query::ListType listType;
					bool isReadWrite;

					void exec(query::LookupCtx& ctx) const {
						auto& data = ctx.data[compType];
						auto& compIds = data.compIds;
						auto& lastMatchedArchetypeIndex = data.lastMatchedArchetypeIndex;
						auto& rules = data.rules;

						// Unique component ids only
						GAIA_ASSERT(!core::has(compIds, componentId));

#if GAIA_DEBUG
						// There's a limit to the amount of components which we can store
						if (compIds.size() >= query::MAX_COMPONENTS_IN_QUERY) {
							GAIA_ASSERT(false && "Trying to create an ECS query with too many components!");

							const auto& cc = ComponentCache::get();
							auto componentName = cc.comp_desc(componentId).name;
							GAIA_LOG_E(
									"Trying to add ECS component '%.*s' to an already full ECS query!", (uint32_t)componentName.size(),
									componentName.data());

							return;
						}
#endif

						data.readWriteMask |= (uint8_t)isReadWrite << (uint8_t)compIds.size();
						compIds.push_back(componentId);
						lastMatchedArchetypeIndex.push_back(0);
						rules.push_back(listType);

						if (listType == query::ListType::LT_All)
							++data.rulesAllCount;
					}
				};

				struct Command_Filter {
					static constexpr CommandBufferCmd Id = CommandBufferCmd::ADD_FILTER;

					component::ComponentId componentId;
					component::ComponentType compType;

					void exec(query::LookupCtx& ctx) const {
						auto& data = ctx.data[compType];
						auto& compIds = data.compIds;
						auto& withChanged = data.withChanged;
						const auto& rules = data.rules;

						GAIA_ASSERT(core::has(compIds, componentId));
						GAIA_ASSERT(!core::has(withChanged, componentId));

#if GAIA_DEBUG
						// There's a limit to the amount of components which we can store
						if (withChanged.size() >= query::MAX_COMPONENTS_IN_QUERY) {
							GAIA_ASSERT(false && "Trying to create an ECS filter query with too many components!");

							const auto& cc = ComponentCache::get();
							auto componentName = cc.comp_desc(componentId).name;
							GAIA_LOG_E(
									"Trying to add ECS component %.*s to an already full filter query!", (uint32_t)componentName.size(),
									componentName.data());
							return;
						}
#endif

						const auto componentIdx = core::get_index_unsafe(compIds, componentId);

						// Component has to be present in anyList or allList.
						// NoneList makes no sense because we skip those in query processing anyway.
						if (rules[componentIdx] != query::ListType::LT_None) {
							withChanged.push_back(componentId);
							return;
						}

						GAIA_ASSERT(false && "SetChangeFilter trying to filter ECS component which is not a part of the query");
#if GAIA_DEBUG
						const auto& cc = ComponentCache::get();
						auto componentName = cc.comp_desc(componentId).name;
						GAIA_LOG_E(
								"SetChangeFilter trying to filter ECS component %.*s but "
								"it's not a part of the query!",
								(uint32_t)componentName.size(), componentName.data());
#endif
					}
				};

				static constexpr CmdBufferCmdFunc CommandBufferRead[] = {
						// Add component
						[](SerializationBuffer& buffer, query::LookupCtx& ctx) {
							Command_AddComponent cmd;
							ser::load(buffer, cmd);
							cmd.exec(ctx);
						},
						// Add filter
						[](SerializationBuffer& buffer, query::LookupCtx& ctx) {
							Command_Filter cmd;
							ser::load(buffer, cmd);
							cmd.exec(ctx);
						}};

				//! Storage for data based on whether Caching is used or not
				QueryImplStorage<UseCaching> m_storage;
				//! Buffer with commands used to fetch the QueryInfo
				SerializationBuffer m_serBuffer;
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
				query::QueryInfo& fetch_query_info() {
					if constexpr (UseCaching) {
						// Make sure the query was created by World.create_query()
						GAIA_ASSERT(m_storage.m_entityQueryCache != nullptr);

						// Lookup hash is present which means QueryInfo was already found
						if GAIA_LIKELY (m_storage.m_queryId != query::QueryIdBad) {
							auto& queryInfo = m_storage.m_entityQueryCache->get(m_storage.m_queryId);
							queryInfo.match(*m_componentToArchetypeMap, (uint32_t)m_archetypes->size());
							return queryInfo;
						}

						// No lookup hash is present which means QueryInfo needs to fetched or created
						query::LookupCtx ctx;
						commit(ctx);
						m_storage.m_queryId = m_storage.m_entityQueryCache->goc(std::move(ctx));
						auto& queryInfo = m_storage.m_entityQueryCache->get(m_storage.m_queryId);
						queryInfo.match(*m_componentToArchetypeMap, (uint32_t)m_archetypes->size());
						return queryInfo;
					} else {
						if GAIA_UNLIKELY (m_storage.m_queryInfo.id() == query::QueryIdBad) {
							query::LookupCtx ctx;
							commit(ctx);
							m_storage.m_queryInfo = query::QueryInfo::create(query::QueryId{}, std::move(ctx));
						}
						m_storage.m_queryInfo.match(*m_componentToArchetypeMap, (uint32_t)m_archetypes->size());
						return m_storage.m_queryInfo;
					}
				}

				//--------------------------------------------------------------------------------
			private:
				template <typename T>
				void add_inter(query::ListType listType) {
					using U = typename component::component_type_t<T>::Type;
					using UOriginal = typename component::component_type_t<T>::TypeOriginal;
					using UOriginalPR = std::remove_reference_t<std::remove_pointer_t<UOriginal>>;

					const auto componentId = component::comp_id<T>();
					constexpr auto compType = component::component_type_v<T>;
					constexpr bool isReadWrite =
							std::is_same_v<U, UOriginal> || (!std::is_const_v<UOriginalPR> && !std::is_empty_v<U>);

					// Make sure the component is always registered
					auto& cc = ComponentCache::get();
					(void)cc.goc_comp_info<T>();

					Command_AddComponent cmd{componentId, compType, listType, isReadWrite};
					ser::save(m_serBuffer, Command_AddComponent::Id);
					ser::save(m_serBuffer, cmd);
				}

				template <typename T>
				void WithChanged_inter() {
					const auto componentId = component::comp_id<T>();
					constexpr auto compType = component::component_type_v<T>;

					Command_Filter cmd{componentId, compType};
					ser::save(m_serBuffer, Command_Filter::Id);
					ser::save(m_serBuffer, cmd);
				}

				//--------------------------------------------------------------------------------

				void commit(query::LookupCtx& ctx) {
#if GAIA_ASSERT_ENABLED
					if constexpr (UseCaching) {
						GAIA_ASSERT(m_storage.m_queryId == query::QueryIdBad);
					} else {
						GAIA_ASSERT(m_storage.m_queryInfo.id() == query::QueryIdBad);
					}
#endif

					// Read data from buffer and execute the command stored in it
					m_serBuffer.seek(0);
					while (m_serBuffer.tell() < m_serBuffer.bytes()) {
						CommandBufferCmd id{};
						ser::load(m_serBuffer, id);
						CommandBufferRead[id](m_serBuffer, ctx);
					}

					// Calculate the lookup hash from the provided context
					query::calc_lookup_hash(ctx);

					// We can free all temporary data now
					m_serBuffer.reset();
				}

				//--------------------------------------------------------------------------------

				//! Unpacks the parameter list \param types into query \param query and performs All for each of them
				template <typename... T>
				void unpack_args_into_query_All(QueryImpl& query, [[maybe_unused]] core::func_type_list<T...> types) const {
					static_assert(sizeof...(T) > 0, "Inputs-less functors can not be unpacked to query");
					query.all<T...>();
				}

				//! Unpacks the parameter list \param types into query \param query and performs has_all for each of them
				template <typename... T>
				GAIA_NODISCARD bool unpack_args_into_query_has_all(
						const query::QueryInfo& queryInfo, [[maybe_unused]] core::func_type_list<T...> types) const {
					if constexpr (sizeof...(T) > 0)
						return queryInfo.has_all<T...>();
					else
						return true;
				}

				//--------------------------------------------------------------------------------

				GAIA_NODISCARD static bool match_filters(const archetype::Chunk& chunk, const query::QueryInfo& queryInfo) {
					GAIA_ASSERT(chunk.has_entities() && "match_filters called on an empty chunk");

					const auto queryVersion = queryInfo.world_version();

					// See if any generic component has changed
					{
						const auto& filtered = queryInfo.filters(component::ComponentType::CT_Generic);
						for (const auto componentId: filtered) {
							const auto componentIdx = chunk.comp_idx(component::ComponentType::CT_Generic, componentId);
							if (chunk.changed(component::ComponentType::CT_Generic, queryVersion, componentIdx))
								return true;
						}
					}

					// See if any chunk component has changed
					{
						const auto& filtered = queryInfo.filters(component::ComponentType::CT_Chunk);
						for (const auto componentId: filtered) {
							const uint32_t componentIdx = chunk.comp_idx(component::ComponentType::CT_Chunk, componentId);
							if (chunk.changed(component::ComponentType::CT_Chunk, queryVersion, componentIdx))
								return true;
						}
					}

					// Skip unchanged chunks.
					return false;
				}

				//! Execute functors in batches
				template <typename Func>
				static void run_func_batched(Func func, ChunkBatchedList& chunks) {
					GAIA_ASSERT(!chunks.empty());

					// This is what the function is doing:
					// for (auto *pChunk: chunks)
					//	func(*pChunk);
					// chunks.clear();

					GAIA_PROF_SCOPE(run_func_batched);

					// We only have one chunk to process
					if GAIA_UNLIKELY (chunks.size() == 1) {
						chunks[0]->set_structural_changes(true);
						func(*chunks[0]);
						chunks[0]->set_structural_changes(false);
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
					chunks[0]->set_structural_changes(true);
					func(*chunks[0]);
					chunks[0]->set_structural_changes(false);

					uint32_t chunkIdx = 1;
					for (; chunkIdx < chunks.size() - 1; ++chunkIdx) {
						gaia::prefetch(&chunks[chunkIdx + 1], PrefetchHint::PREFETCH_HINT_T2);
						chunks[chunkIdx]->set_structural_changes(true);
						func(*chunks[chunkIdx]);
						chunks[chunkIdx]->set_structural_changes(false);
					}

					chunks[chunkIdx]->set_structural_changes(true);
					func(*chunks[chunkIdx]);
					chunks[chunkIdx]->set_structural_changes(false);

					chunks.clear();
				}

				template <bool has_filters, typename Func>
				void run_query_unconstrained(
						Func func, ChunkBatchedList& chunkBatch, const cnt::darray<archetype::Chunk*>& chunks,
						const query::QueryInfo& queryInfo) {
					uint32_t chunkOffset = 0;
					uint32_t itemsLeft = chunks.size();
					while (itemsLeft > 0) {
						const auto maxBatchSize = chunkBatch.max_size() - chunkBatch.size();
						const auto batchSize = itemsLeft > maxBatchSize ? maxBatchSize : itemsLeft;

						CChunkSpan chunkSpan((const archetype::Chunk**)&chunks[chunkOffset], batchSize);
						for (const auto* pChunk: chunkSpan) {
							if (!pChunk->has_entities())
								continue;
							if constexpr (has_filters) {
								if (!match_filters(*pChunk, queryInfo))
									continue;
							}

							chunkBatch.push_back(const_cast<archetype::Chunk*>(pChunk));
						}

						if GAIA_UNLIKELY (chunkBatch.size() == chunkBatch.max_size())
							run_func_batched(func, chunkBatch);

						itemsLeft -= batchSize;
						chunkOffset += batchSize;
					}
				}

				template <bool has_filters, typename Func>
				void run_query_constrained(
						Func func, ChunkBatchedList& chunkBatch, const cnt::darray<archetype::Chunk*>& chunks,
						const query::QueryInfo& queryInfo, bool enabledOnly) {
					uint32_t chunkOffset = 0;
					uint32_t itemsLeft = chunks.size();
					while (itemsLeft > 0) {
						const auto maxBatchSize = chunkBatch.max_size() - chunkBatch.size();
						const auto batchSize = itemsLeft > maxBatchSize ? maxBatchSize : itemsLeft;

						CChunkSpan chunkSpan((const archetype::Chunk**)&chunks[chunkOffset], batchSize);
						for (const auto* pChunk: chunkSpan) {
							if (!pChunk->has_entities())
								continue;

							if (enabledOnly && !pChunk->has_enabled_entities())
								continue;
							if (!enabledOnly && !pChunk->has_disabled_entities())
								continue;

							if constexpr (has_filters) {
								if (!match_filters(*pChunk, queryInfo))
									continue;
							}

							chunkBatch.push_back(const_cast<archetype::Chunk*>(pChunk));
						}

						if GAIA_UNLIKELY (chunkBatch.size() == chunkBatch.max_size())
							run_func_batched(func, chunkBatch);

						itemsLeft -= batchSize;
						chunkOffset += batchSize;
					}
				}

				template <typename Func>
				void run_query_on_chunks(query::QueryInfo& queryInfo, Constraints constraints, Func func) {
					// Update the world version
					update_version(*m_worldVersion);

					ChunkBatchedList chunkBatch;

					const bool hasFilters = queryInfo.has_filters();
					if (hasFilters) {
						// Evaluation defaults to EnabledOnly changes. AcceptAll is something that has to be asked for explicitely
						if GAIA_UNLIKELY (constraints == Constraints::AcceptAll) {
							for (auto* pArchetype: queryInfo)
								run_query_unconstrained<true>(func, chunkBatch, pArchetype->chunks(), queryInfo);
						} else {
							const bool enabledOnly = constraints == Constraints::EnabledOnly;
							for (auto* pArchetype: queryInfo)
								run_query_constrained<true>(func, chunkBatch, pArchetype->chunks(), queryInfo, enabledOnly);
						}
					} else {
						if GAIA_UNLIKELY (constraints == Constraints::AcceptAll) {
							for (auto* pArchetype: queryInfo)
								run_query_unconstrained<false>(func, chunkBatch, pArchetype->chunks(), queryInfo);
						} else {
							const bool enabledOnly = constraints == Constraints::EnabledOnly;
							for (auto* pArchetype: queryInfo)
								run_query_constrained<false>(func, chunkBatch, pArchetype->chunks(), queryInfo, enabledOnly);
						}
					}

					if (!chunkBatch.empty())
						run_func_batched(func, chunkBatch);

					// Update the query version with the current world's version
					queryInfo.set_world_version(*m_worldVersion);
				}

				template <typename Func>
				void each_inter(query::QueryInfo& queryInfo, Func func) {
					using InputArgs = decltype(core::func_args(&Func::operator()));

#if GAIA_DEBUG
					// Make sure we only use components specified in the query
					GAIA_ASSERT(unpack_args_into_query_has_all(queryInfo, InputArgs{}));
#endif

					run_query_on_chunks(queryInfo, Constraints::EnabledOnly, [&](archetype::Chunk& chunk) {
						chunk.each(InputArgs{}, func);
					});
				}

				void invalidate() {
					if constexpr (UseCaching)
						m_storage.m_queryId = query::QueryIdBad;
				}

			public:
				QueryImpl() = default;

				template <bool FuncEnabled = UseCaching>
				QueryImpl(
						QueryCache& queryCache, uint32_t& worldVersion, const cnt::darray<archetype::Archetype*>& archetypes,
						const query::ComponentToArchetypeMap& componentToArchetypeMap):
						m_worldVersion(&worldVersion),
						m_archetypes(&archetypes), m_componentToArchetypeMap(&componentToArchetypeMap) {
					m_storage.m_entityQueryCache = &queryCache;
				}

				template <bool FuncEnabled = !UseCaching>
				QueryImpl(
						uint32_t& worldVersion, const cnt::darray<archetype::Archetype*>& archetypes,
						const query::ComponentToArchetypeMap& componentToArchetypeMap):
						m_worldVersion(&worldVersion),
						m_archetypes(&archetypes), m_componentToArchetypeMap(&componentToArchetypeMap) {}

				GAIA_NODISCARD uint32_t id() const {
					static_assert(UseCaching, "id() can be used only with cached queries");
					return m_storage.m_queryId;
				}

				template <typename... T>
				QueryImpl& all() {
					// Adding new rules invalidates the query
					invalidate();
					// Add commands to the command buffer
					(add_inter<T>(query::ListType::LT_All), ...);
					return *this;
				}

				template <typename... T>
				QueryImpl& any() {
					// Adding new rules invalidates the query
					invalidate();
					// Add commands to the command buffer
					(add_inter<T>(query::ListType::LT_Any), ...);
					return *this;
				}

				template <typename... T>
				QueryImpl& none() {
					// Adding new rules invalidates the query
					invalidate();
					// Add commands to the command buffer
					(add_inter<T>(query::ListType::LT_None), ...);
					return *this;
				}

				template <typename... T>
				QueryImpl& changed() {
					// Adding new rules invalidates the query
					invalidate();
					// Add commands to the command buffer
					(WithChanged_inter<T>(), ...);
					return *this;
				}

				QueryImpl& sched() {
					m_executionMode = query::ExecutionMode::Single;
					return *this;
				}

				QueryImpl& sched_par() {
					m_executionMode = query::ExecutionMode::Parallel;
					return *this;
				}

				template <typename Func>
				void each(Func func) {
					auto& queryInfo = fetch_query_info();

					if constexpr (std::is_invocable<Func, IteratorAll>::value)
						run_query_on_chunks(queryInfo, Constraints::AcceptAll, [&](archetype::Chunk& chunk) {
							func(IteratorAll(chunk));
						});
					else if constexpr (std::is_invocable<Func, Iterator>::value)
						run_query_on_chunks(queryInfo, Constraints::EnabledOnly, [&](archetype::Chunk& chunk) {
							func(Iterator(chunk));
						});
					else if constexpr (std::is_invocable<Func, IteratorDisabled>::value)
						run_query_on_chunks(queryInfo, Constraints::DisabledOnly, [&](archetype::Chunk& chunk) {
							func(IteratorDisabled(chunk));
						});
					else
						each_inter(queryInfo, func);
				}

				template <typename Func, bool FuncEnabled = UseCaching, typename std::enable_if<FuncEnabled>::type* = nullptr>
				void each(query::QueryId queryId, Func func) {
					// Make sure the query was created by World.create_query()
					GAIA_ASSERT(m_storage.m_entityQueryCache != nullptr);
					GAIA_ASSERT(queryId != query::QueryIdBad);

					auto& queryInfo = m_storage.m_entityQueryCache->get(queryId);
					each_inter(queryInfo, func);
				}

				template <bool UseFilters, Constraints c, typename ChunksContainer>
				GAIA_NODISCARD bool HasEntities_Helper(query::QueryInfo& queryInfo, const ChunksContainer& chunks) {
					return core::has_if(chunks, [&](archetype::Chunk* pChunk) {
						if constexpr (UseFilters) {
							if constexpr (c == Constraints::AcceptAll)
								return pChunk->has_entities() && match_filters(*pChunk, queryInfo);
							else if constexpr (c == Constraints::EnabledOnly)
								return pChunk->size_disabled() != pChunk->size() && match_filters(*pChunk, queryInfo);
							else // if constexpr (c == Constraints::DisabledOnly)
								return pChunk->size_disabled() > 0 && match_filters(*pChunk, queryInfo);
						} else {
							if constexpr (c == Constraints::AcceptAll)
								return pChunk->has_entities();
							else if constexpr (c == Constraints::EnabledOnly)
								return pChunk->size_disabled() != pChunk->size();
							else // if constexpr (c == Constraints::DisabledOnly)
								return pChunk->size_disabled() > 0;
						}
					});
				}

				template <bool UseFilters, Constraints c, typename ChunksContainer>
				GAIA_NODISCARD uint32_t calc_entity_cnt_inter(query::QueryInfo& queryInfo, const ChunksContainer& chunks) {
					uint32_t cnt = 0;

					for (auto* pChunk: chunks) {
						if (!pChunk->has_entities())
							continue;

						// Filters
						if constexpr (UseFilters) {
							if (!match_filters(*pChunk, queryInfo))
								continue;
						}

						// Entity count
						if constexpr (c == Constraints::EnabledOnly)
							cnt += pChunk->size_enabled();
						else if constexpr (c == Constraints::DisabledOnly)
							cnt += pChunk->size_disabled();
						else
							cnt += pChunk->size();
					}

					return cnt;
				}

				template <bool UseFilters, Constraints c, typename ChunksContainerIn, typename ChunksContainerOut>
				void as_arr_inter(query::QueryInfo& queryInfo, const ChunksContainerIn& chunks, ChunksContainerOut& outArray) {
					using ContainerItemType = typename ChunksContainerOut::value_type;

					for (auto* pChunk: chunks) {
						if (!pChunk->has_entities())
							continue;

						if constexpr (c == Constraints::EnabledOnly) {
							if (pChunk->has_disabled_entities())
								continue;
						} else if constexpr (c == Constraints::DisabledOnly) {
							if (!pChunk->has_disabled_entities())
								continue;
						}

						// Filters
						if constexpr (UseFilters) {
							if (!match_filters(*pChunk, queryInfo))
								continue;
						}

						const auto componentView = pChunk->template view<ContainerItemType>();
						for (uint32_t i = 0; i < pChunk->size(); ++i)
							outArray.push_back(componentView[i]);
					}
				}

				/*!
					Returns true or false depending on whether there are entities matching the query.
					\warning Only use if you only care if there are any entities matching the query.
									 The result is not cached and repeated calls to the function might be slow.
									 If you already called as_arr, checking if it is empty is preferred.
					\return True if there are any entites matchine the query. False otherwise.
					*/
				bool has_entities(Constraints constraints = Constraints::EnabledOnly) {
					auto& queryInfo = fetch_query_info();
					const bool hasFilters = queryInfo.has_filters();

					if (hasFilters) {
						switch (constraints) {
							case Constraints::EnabledOnly: {
								for (auto* pArchetype: queryInfo)
									if (HasEntities_Helper<true, Constraints::EnabledOnly>(queryInfo, pArchetype->chunks()))
										return true;
							} break;
							case Constraints::DisabledOnly: {
								for (auto* pArchetype: queryInfo)
									if (HasEntities_Helper<true, Constraints::DisabledOnly>(queryInfo, pArchetype->chunks()))
										return true;
							} break;
							case Constraints::AcceptAll: {
								for (auto* pArchetype: queryInfo)
									if (HasEntities_Helper<true, Constraints::AcceptAll>(queryInfo, pArchetype->chunks()))
										return true;
							} break;
						}
					} else {
						switch (constraints) {
							case Constraints::EnabledOnly: {
								for (auto* pArchetype: queryInfo)
									if (HasEntities_Helper<false, Constraints::EnabledOnly>(queryInfo, pArchetype->chunks()))
										return true;
							} break;
							case Constraints::DisabledOnly: {
								for (auto* pArchetype: queryInfo)
									if (HasEntities_Helper<false, Constraints::DisabledOnly>(queryInfo, pArchetype->chunks()))
										return true;
							} break;
							case Constraints::AcceptAll: {
								for (auto* pArchetype: queryInfo)
									if (HasEntities_Helper<false, Constraints::AcceptAll>(queryInfo, pArchetype->chunks()))
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
								 If you already called as_arr, use the size provided by the array.
				\return The number of matching entities
				*/
				uint32_t calc_entity_cnt(Constraints constraints = Constraints::EnabledOnly) {
					auto& queryInfo = fetch_query_info();
					uint32_t entCnt = 0;

					const bool hasFilters = queryInfo.has_filters();
					if (hasFilters) {
						switch (constraints) {
							case Constraints::EnabledOnly: {
								for (auto* pArchetype: queryInfo)
									entCnt += calc_entity_cnt_inter<true, Constraints::EnabledOnly>(queryInfo, pArchetype->chunks());
							} break;
							case Constraints::DisabledOnly: {
								for (auto* pArchetype: queryInfo)
									entCnt += calc_entity_cnt_inter<true, Constraints::DisabledOnly>(queryInfo, pArchetype->chunks());
							} break;
							case Constraints::AcceptAll: {
								for (auto* pArchetype: queryInfo)
									entCnt += calc_entity_cnt_inter<true, Constraints::AcceptAll>(queryInfo, pArchetype->chunks());
							} break;
						}
					} else {
						switch (constraints) {
							case Constraints::EnabledOnly: {
								for (auto* pArchetype: queryInfo)
									entCnt += calc_entity_cnt_inter<false, Constraints::EnabledOnly>(queryInfo, pArchetype->chunks());
							} break;
							case Constraints::DisabledOnly: {
								for (auto* pArchetype: queryInfo)
									entCnt += calc_entity_cnt_inter<false, Constraints::DisabledOnly>(queryInfo, pArchetype->chunks());
							} break;
							case Constraints::AcceptAll: {
								for (auto* pArchetype: queryInfo)
									entCnt += calc_entity_cnt_inter<false, Constraints::AcceptAll>(queryInfo, pArchetype->chunks());
							} break;
						}
					}

					return entCnt;
				}

				/*!
				Appends all components or entities matching the query to the array
				\tparam outArray Container storing entities or components
				\param constraints QueryImpl constraints
				\return Array with entities or components
				*/
				template <typename Container>
				void as_arr(Container& outArray, Constraints constraints = Constraints::EnabledOnly) {
					const auto entCnt = calc_entity_cnt();
					if (entCnt == 0)
						return;

					outArray.reserve(entCnt);
					auto& queryInfo = fetch_query_info();

					const bool hasFilters = queryInfo.has_filters();
					if (hasFilters) {
						switch (constraints) {
							case Constraints::EnabledOnly:
								for (auto* pArchetype: queryInfo)
									as_arr_inter<true, Constraints::EnabledOnly>(queryInfo, pArchetype->chunks(), outArray);
								break;
							case Constraints::DisabledOnly:
								for (auto* pArchetype: queryInfo)
									as_arr_inter<true, Constraints::DisabledOnly>(queryInfo, pArchetype->chunks(), outArray);
								break;
							case Constraints::AcceptAll:
								for (auto* pArchetype: queryInfo)
									as_arr_inter<true, Constraints::AcceptAll>(queryInfo, pArchetype->chunks(), outArray);
								break;
						}
					} else {
						switch (constraints) {
							case Constraints::EnabledOnly:
								for (auto* pArchetype: queryInfo)
									as_arr_inter<false, Constraints::EnabledOnly>(queryInfo, pArchetype->chunks(), outArray);
								break;
							case Constraints::DisabledOnly:
								for (auto* pArchetype: queryInfo)
									as_arr_inter<false, Constraints::DisabledOnly>(queryInfo, pArchetype->chunks(), outArray);
								break;
							case Constraints::AcceptAll:
								for (auto* pArchetype: queryInfo)
									as_arr_inter<false, Constraints::AcceptAll>(queryInfo, pArchetype->chunks(), outArray);
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
