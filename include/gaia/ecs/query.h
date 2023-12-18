#pragma once
#include "../config/config.h"

#include <cstdarg>
#include <type_traits>

#include "../cnt/darray.h"
#include "../cnt/map.h"
#include "../cnt/sarray_ext.h"
#include "../config/profiler.h"
#include "../core/hashing_policy.h"
#include "../core/utility.h"
#include "../ser/serialization.h"
#include "archetype.h"
#include "archetype_common.h"
#include "chunk.h"
#include "chunk_iterator.h"
#include "common.h"
#include "component.h"
#include "component_cache.h"
#include "component_utils.h"
#include "data_buffer.h"
#include "id.h"
#include "query_cache.h"
#include "query_common.h"
#include "query_info.h"

namespace gaia {
	namespace ecs {
		class World;
		inline const ComponentCache& comp_cache(const World& world);
		inline ComponentCache& comp_cache_mut(World& world);
		template <typename T>
		inline const ComponentCacheItem& comp_cache_add(World& world);

		namespace detail {
			template <bool Cached>
			struct QueryImplStorage {
				//! QueryImpl cache id
				QueryId m_queryId = QueryIdBad;
				//! QueryImpl cache (stable pointer to parent world's query cache)
				QueryCache* m_queryCache{};
			};

			template <>
			struct QueryImplStorage<false> {
				QueryInfo m_queryInfo;
			};

			template <bool UseCaching = true>
			class QueryImpl final {
				static constexpr uint32_t ChunkBatchSize = 16;
				using ChunkSpan = std::span<const Chunk*>;
				using ChunkSpanMut = std::span<Chunk*>;
				using ChunkBatchedList = cnt::sarray_ext<Chunk*, ChunkBatchSize>;
				using CmdBufferCmdFunc = void (*)(SerializationBuffer& buffer, QueryCtx& ctx);

			private:
				//! Command buffer command type
				enum CommandBufferCmd : uint8_t { ADD_ITEM, ADD_FILTER };

				struct Command_AddItem {
					static constexpr CommandBufferCmd Id = CommandBufferCmd::ADD_ITEM;

					QueryItem item;

					void exec(QueryCtx& ctx) const {
						auto& data = ctx.data;
						auto& ids = data.ids;
						auto& pairs = data.pairs;
						auto& lastMatchedArchetypeIdx = data.lastMatchedArchetypeIdx;

						// Unique component ids only
						GAIA_ASSERT(!core::has(ids, item.id));

#if GAIA_DEBUG
						// There's a limit to the amount of query items which we can store
						if (ids.size() >= MAX_ITEMS_IN_QUERY) {
							GAIA_ASSERT2(false, "Trying to create a query with too many components!");

							const auto* name = ctx.cc->get(item.id).name.str();
							GAIA_LOG_E("Trying to add component '%s' to an already full ECS query!", name);
							return;
						}
#endif

						const uint8_t isReadWrite = item.access == QueryAccess::Write;
						data.readWriteMask |= (isReadWrite << (uint8_t)ids.size());

						ids.push_back(item.id);
						pairs.push_back({item.op, item.id});
						lastMatchedArchetypeIdx.push_back(0);
					}
				};

				struct Command_Filter {
					static constexpr CommandBufferCmd Id = CommandBufferCmd::ADD_FILTER;

					Entity comp;

					void exec(QueryCtx& ctx) const {
						auto& data = ctx.data;
						auto& ids = data.ids;
						auto& withChanged = data.withChanged;
						const auto& pair = data.pairs;

						GAIA_ASSERT(core::has(ids, comp));
						GAIA_ASSERT(!core::has(withChanged, comp));

#if GAIA_DEBUG
						// There's a limit to the amount of components which we can store
						if (withChanged.size() >= MAX_ITEMS_IN_QUERY) {
							GAIA_ASSERT2(false, "Trying to create an filter query with too many components!");

							auto compName = ctx.cc->get(comp).name.str();
							GAIA_LOG_E("Trying to add component %s to an already full filter query!", compName);
							return;
						}
#endif

						uint32_t compIdx = 0;
						for (; compIdx < ids.size(); ++compIdx)
							if (ids[compIdx] == comp)
								break;
						// NOTE: This code bellow does technically the same as above.
						//       However, compilers can't quite optimize it as well because it does some more
						//       calculations. This is used often so go with the custom code.
						// const auto compIdx = core::get_index_unsafe(ids, comp);

						// Component has to be present in anyList or allList.
						// NoneList makes no sense because we skip those in query processing anyway.
						if (pair[compIdx].op != QueryOp::Not) {
							withChanged.push_back(comp);
							return;
						}

						GAIA_ASSERT2(false, "SetChangeFilter trying to filter component which is not a part of the query");
#if GAIA_DEBUG
						auto compName = ctx.cc->get(comp).name.str();
						GAIA_LOG_E("SetChangeFilter trying to filter component %s but it's not a part of the query!", compName);
#endif
					}
				};

				static constexpr CmdBufferCmdFunc CommandBufferRead[] = {
						// Add component
						[](SerializationBuffer& buffer, QueryCtx& ctx) {
							Command_AddItem cmd;
							ser::load(buffer, cmd);
							cmd.exec(ctx);
						},
						// Add filter
						[](SerializationBuffer& buffer, QueryCtx& ctx) {
							Command_Filter cmd;
							ser::load(buffer, cmd);
							cmd.exec(ctx);
						}};

				World* m_world{};
				//! Storage for data based on whether Caching is used or not
				QueryImplStorage<UseCaching> m_storage;
				//! Buffer with commands used to fetch the QueryInfo
				SerializationBuffer m_serBuffer;
				//! World version (stable pointer to parent world's m_nextArchetypeId)
				ArchetypeId* m_nextArchetypeId{};
				//! World version (stable pointer to parent world's world version)
				uint32_t* m_worldVersion{};
				//! List of archetypes (stable pointer to parent world's archetype array)
				const cnt::map<ArchetypeId, Archetype*>* m_archetypes{};
				//! Map of component ids to archetypes (stable pointer to parent world's archetype component-to-archetype map)
				const EntityToArchetypeMap* m_entityToArchetypeMap{};

				//--------------------------------------------------------------------------------
			public:
				//! Fetches the QueryInfo object.
				//! \return QueryInfo object
				QueryInfo& fetch() {
					GAIA_PROF_SCOPE(query::fetch);

					if constexpr (UseCaching) {
						// Make sure the query was created by World.query()
						GAIA_ASSERT(m_storage.m_queryCache != nullptr);

						// If queryId is set it means QueryInfo was already created.
						// Because caching is used, we expect this to be the common case.
						if GAIA_LIKELY (m_storage.m_queryId != QueryIdBad) {
							auto& queryInfo = m_storage.m_queryCache->get(m_storage.m_queryId);
							queryInfo.match(*m_entityToArchetypeMap, last_archetype_id());
							return queryInfo;
						}

						// No queryId is set which means QueryInfo needs to be created
						QueryCtx ctx;
						ctx.cc = &comp_cache_mut(*m_world);
						commit(ctx);
						auto& queryInfo = m_storage.m_queryCache->add(GAIA_MOV(ctx));
						m_storage.m_queryId = queryInfo.id();
						queryInfo.match(*m_entityToArchetypeMap, last_archetype_id());
						return queryInfo;
					} else {
						if GAIA_UNLIKELY (m_storage.m_queryInfo.id() == QueryIdBad) {
							QueryCtx ctx;
							ctx.cc = &comp_cache_mut(*m_world);
							commit(ctx);
							m_storage.m_queryInfo = QueryInfo::create(QueryId{}, GAIA_MOV(ctx));
						}
						m_storage.m_queryInfo.match(*m_entityToArchetypeMap, last_archetype_id());
						return m_storage.m_queryInfo;
					}
				}

				//--------------------------------------------------------------------------------
			private:
				ArchetypeId last_archetype_id() const {
					return *m_nextArchetypeId - 1;
				}

				void add_inter(QueryItem item) {
					// Adding new query items invalidates the query
					invalidate();

					// When excluding make sure the access type is None.
					GAIA_ASSERT(item.op != QueryOp::Not || item.access == QueryAccess::None);

					Command_AddItem cmd{item};
					ser::save(m_serBuffer, Command_AddItem::Id);
					ser::save(m_serBuffer, cmd);
				}

				template <typename T>
				void add_inter(QueryOp op) {
					Entity e;

					if constexpr (is_pair<T>::value) {
						// Make sure the components are always registered
						const auto& desc_rel = comp_cache_add<typename T::rel_type>(*m_world);
						const auto& desc_tgt = comp_cache_add<typename T::tgt_type>(*m_world);

						e = Pair(desc_rel.entity, desc_tgt.entity);
					} else {
						// Make sure the component is always registered
						const auto& desc = comp_cache_add<T>(*m_world);
						e = desc.entity;
					}

					// Determine the access type
					QueryAccess access = QueryAccess::None;
					if (op != QueryOp::Not) {
						constexpr auto isReadWrite = core::is_mut_v<T>;
						access = isReadWrite ? QueryAccess::Write : QueryAccess::Read;
					}

					add_inter({e, op, access});
				}

				//--------------------------------------------------------------------------------

				void changed_inter(Entity entity) {
					// Adding new changed items invalidates the query
					invalidate();

					Command_Filter cmd{entity};
					ser::save(m_serBuffer, Command_Filter::Id);
					ser::save(m_serBuffer, cmd);
				}

				template <typename T>
				void changed_inter() {
					using UO = typename component_type_t<T>::TypeOriginal;
					static_assert(core::is_raw_v<UO>, "Use changed() with raw types only");

					// Make sure the component is always registered
					const auto& desc = comp_cache_add<T>(*m_world);
					changed_inter(desc.entity);
				}

				//--------------------------------------------------------------------------------

				void commit(QueryCtx& ctx) {
#if GAIA_ASSERT_ENABLED
					if constexpr (UseCaching) {
						GAIA_ASSERT(m_storage.m_queryId == QueryIdBad);
					} else {
						GAIA_ASSERT(m_storage.m_queryInfo.id() == QueryIdBad);
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
					if constexpr (UseCaching)
						calc_lookup_hash(ctx);

					// We can free all temporary data now
					m_serBuffer.reset();
				}

				//--------------------------------------------------------------------------------

				//! Unpacks the parameter list \param types into query \param query and performs All for each of them
				template <typename... T>
				void unpack_args_into_query_all(QueryImpl& query, [[maybe_unused]] core::func_type_list<T...> types) const {
					static_assert(sizeof...(T) > 0, "Inputs-less functors can not be unpacked to query");
					query.all<T...>();
				}

				//! Unpacks the parameter list \param types into query \param query and performs has_all for each of them
				template <typename... T>
				GAIA_NODISCARD bool unpack_args_into_query_has_all(
						const QueryInfo& queryInfo, [[maybe_unused]] core::func_type_list<T...> types) const {
					if constexpr (sizeof...(T) > 0)
						return queryInfo.has_all<T...>();
					else
						return true;
				}

				//--------------------------------------------------------------------------------

				GAIA_NODISCARD static bool match_filters(const Chunk& chunk, const QueryInfo& queryInfo) {
					GAIA_ASSERT(!chunk.empty() && "match_filters called on an empty chunk");

					const auto queryVersion = queryInfo.world_version();

					// See if any component has changed
					const auto& filtered = queryInfo.filters();
					for (const auto comp: filtered) {
						// TODO: Components are sorted. Therefore, we don't need to search from 0
						//       all the time. We can search from the last found index.
						const auto compIdx = chunk.comp_idx(comp);
						if (chunk.changed(queryVersion, compIdx))
							return true;
					}

					// Skip unchanged chunks.
					return false;
				}

				//! Execute functors in batches
				template <typename Func>
				static void run_func_batched(Func func, ChunkBatchedList& chunks) {
					const auto chunkCnt = chunks.size();
					GAIA_ASSERT(chunkCnt > 0);

					// This is what the function is doing:
					// for (auto *pChunk: chunks) {
					//  pChunk->lock(true);
					//	func(*pChunk);
					//  pChunk->lock(false);
					// }
					// chunks.clear();

					GAIA_PROF_SCOPE(query::run_func_batched);

					// We only have one chunk to process
					if GAIA_UNLIKELY (chunkCnt == 1) {
						chunks[0]->lock(true);
						func(*chunks[0]);
						chunks[0]->lock(false);
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
					chunks[0]->lock(true);
					func(*chunks[0]);
					chunks[0]->lock(false);

					uint32_t chunkIdx = 1;
					for (; chunkIdx < chunkCnt - 1; ++chunkIdx) {
						gaia::prefetch(&chunks[chunkIdx + 1], PrefetchHint::PREFETCH_HINT_T2);
						chunks[chunkIdx]->lock(true);
						func(*chunks[chunkIdx]);
						chunks[chunkIdx]->lock(false);
					}

					chunks[chunkIdx]->lock(true);
					func(*chunks[chunkIdx]);
					chunks[chunkIdx]->lock(false);

					chunks.clear();
				}

				template <bool HasFilters, typename Iter, typename Func>
				void run_query(
						const QueryInfo& queryInfo, Func func, ChunkBatchedList& chunkBatch, const cnt::darray<Chunk*>& chunks) {
					GAIA_PROF_SCOPE(query::run_query); // batch preparation + chunk processing

					uint32_t chunkOffset = 0;
					uint32_t itemsLeft = chunks.size();
					while (itemsLeft > 0) {
						const auto maxBatchSize = chunkBatch.max_size() - chunkBatch.size();
						const auto batchSize = itemsLeft > maxBatchSize ? maxBatchSize : itemsLeft;

						ChunkSpanMut chunkSpan((Chunk**)&chunks[chunkOffset], batchSize);
						for (auto* pChunk: chunkSpan) {
							Iter iter(*pChunk);
							if (iter.size() == 0)
								continue;

							if constexpr (HasFilters) {
								if (!match_filters(*pChunk, queryInfo))
									continue;
							}

							chunkBatch.push_back(pChunk);
						}

						if GAIA_UNLIKELY (chunkBatch.size() == chunkBatch.max_size())
							run_func_batched(func, chunkBatch);

						itemsLeft -= batchSize;
						chunkOffset += batchSize;
					}
				}

				template <typename Iter, typename Func>
				void run_query_on_chunks(QueryInfo& queryInfo, Func func) {
					// Update the world version
					update_version(*m_worldVersion);

					ChunkBatchedList chunkBatch;

					const bool hasFilters = queryInfo.has_filters();
					if (hasFilters) {
						for (auto* pArchetype: queryInfo)
							run_query<true, Iter>(queryInfo, func, chunkBatch, pArchetype->chunks());
					} else {
						for (auto* pArchetype: queryInfo)
							run_query<false, Iter>(queryInfo, func, chunkBatch, pArchetype->chunks());
					}

					if (!chunkBatch.empty())
						run_func_batched(func, chunkBatch);

					// Update the query version with the current world's version
					queryInfo.set_world_version(*m_worldVersion);
				}

				template <typename Iter, typename Func, typename... T>
				GAIA_FORCEINLINE void
				run_query_on_chunk(Chunk& chunk, Func func, [[maybe_unused]] core::func_type_list<T...> types) {
					if constexpr (sizeof...(T) > 0) {
						Iter iter(chunk);

						// Pointers to the respective component types in the chunk, e.g
						// 		q.each([&](Position& p, const Velocity& v) {...}
						// Translates to:
						//  	auto p = iter.view_mut_inter<Position, true>();
						//		auto v = iter.view_inter<Velocity>();
						auto dataPointerTuple = std::make_tuple(iter.template view_auto<T>()...);

						// Iterate over each entity in the chunk.
						// Translates to:
						//		GAIA_EACH(iter) func(p[i], v[i]);

						GAIA_EACH(iter) func(std::get<decltype(iter.template view_auto<T>())>(dataPointerTuple)[i]...);
					} else {
						// No functor parameters. Do an empty loop.
						Iter iter(chunk);
						GAIA_EACH(iter) func();
					}
				}

				void invalidate() {
					if constexpr (UseCaching)
						m_storage.m_queryId = QueryIdBad;
				}

				template <bool UseFilters, typename Iter>
				GAIA_NODISCARD bool empty_inter(const QueryInfo& queryInfo, const cnt::darray<Chunk*>& chunks) {
					return core::has_if(chunks, [&](Chunk* pChunk) {
						Iter iter(*pChunk);
						if constexpr (UseFilters)
							return iter.size() > 0 && match_filters(*pChunk, queryInfo);
						else
							return iter.size() > 0;
					});
				}

				template <bool UseFilters, typename Iter>
				GAIA_NODISCARD uint32_t count_inter(const QueryInfo& queryInfo, const cnt::darray<Chunk*>& chunks) {
					uint32_t cnt = 0;

					for (auto* pChunk: chunks) {
						Iter iter(*pChunk);
						const auto entityCnt = iter.size();
						if (entityCnt == 0)
							continue;

						// Filters
						if constexpr (UseFilters) {
							if (!match_filters(*pChunk, queryInfo))
								continue;
						}

						// Entity count
						cnt += entityCnt;
					}

					return cnt;
				}

				template <bool UseFilters, typename Iter, typename ContainerOut>
				void arr_inter(const QueryInfo& queryInfo, const cnt::darray<Chunk*>& chunks, ContainerOut& outArray) {
					using ContainerItemType = typename ContainerOut::value_type;

					for (auto* pChunk: chunks) {
						Iter iter(*pChunk);
						if (iter.size() == 0)
							continue;

						// Filters
						if constexpr (UseFilters) {
							if (!match_filters(*pChunk, queryInfo))
								continue;
						}

						const auto dataView = iter.template view<ContainerItemType>();
						GAIA_EACH(iter) outArray.push_back(dataView[i]);
					}
				}

				static auto trim(std::span<const char> expr) {
					uint32_t beg = 0;
					while (expr[beg] == ' ')
						++beg;
					uint32_t end = (uint32_t)expr.size() - 1;
					while (end > beg && expr[end] == ' ')
						--end;
					return expr.subspan(beg, end - beg + 1);
				};

				Entity expr_to_entity(va_list& args, std::span<const char> exprRaw) {
					auto expr = trim(exprRaw);

					if (expr[0] == '%') {
						if (expr[1] != 'e') {
							GAIA_ASSERT2(false, "Expression '%' not terminated");
							return EntityBad;
						}

						auto id = (Identifier)va_arg(args, unsigned long long);
						return Entity(id);
					}

					if (expr[0] == '(') {
						if (expr.back() != ')') {
							GAIA_ASSERT2(false, "Expression '(' not terminated");
							return EntityBad;
						}

						auto idStr = expr.subspan(1, expr.size() - 2);
						const auto commaIdx = core::get_index(idStr, ',');

						const auto first = expr_to_entity(args, idStr.subspan(0, commaIdx));
						if (first == EntityBad)
							return EntityBad;
						const auto second = expr_to_entity(args, idStr.subspan(commaIdx + 1));
						if (second == EntityBad)
							return EntityBad;

						return ecs::Pair(first, second);
					}

					{
						auto idStr = trim(expr);

						// Wildcard character
						if (idStr.size() == 1 && idStr[0] == '*')
							return All;

						// Anything else is a component name
						const auto& cc = comp_cache(*m_world);
						const auto* pItem = cc.find(idStr.data(), (uint32_t)idStr.size());
						if (pItem == nullptr) {
							GAIA_ASSERT2(false, "Type not found");
							GAIA_LOG_W("Type '%.*s' not found", (uint32_t)idStr.size(), idStr.data());
							return EntityBad;
						}

						return pItem->entity;
					}
				};

			public:
				QueryImpl() = default;

				template <bool FuncEnabled = UseCaching>
				QueryImpl(
						World& world, QueryCache& queryCache, ArchetypeId& nextArchetypeId, uint32_t& worldVersion,
						const cnt::map<ArchetypeId, Archetype*>& archetypes, const EntityToArchetypeMap& entityToArchetypeMap):
						m_world(&world),
						m_serBuffer(&comp_cache_mut(world)), m_nextArchetypeId(&nextArchetypeId), m_worldVersion(&worldVersion),
						m_archetypes(&archetypes), m_entityToArchetypeMap(&entityToArchetypeMap) {
					m_storage.m_queryCache = &queryCache;
				}

				template <bool FuncEnabled = !UseCaching>
				QueryImpl(
						World& world, ArchetypeId& nextArchetypeId, uint32_t& worldVersion,
						const cnt::map<ArchetypeId, Archetype*>& archetypes, const EntityToArchetypeMap& entityToArchetypeMap):
						m_world(&world),
						m_serBuffer(&comp_cache_mut(world)), m_nextArchetypeId(&nextArchetypeId), m_worldVersion(&worldVersion),
						m_archetypes(&archetypes), m_entityToArchetypeMap(&entityToArchetypeMap) {}

				GAIA_NODISCARD uint32_t id() const {
					static_assert(UseCaching, "id() can be used only with cached queries");
					return m_storage.m_queryId;
				}

				//! Creates a query from a null-terminated expression string.
				//!
				//! Expresion is a string between two semicolons.
				//! Spaces are trimmed automatically.
				//!
				//! Supported modifiers:
				//!   ";" - separates expressions
				//!   "?" - query::any
				//!   "!" - query::none
				//!   "&" - read-write access
				//!   "%e" - entity value
				//!   "(rel,tgt)" - relationship pair, a wildcard character in either rel or tgt is translated into All
				//!
				//! Example:
				//!   struct Position {...};
				//!   struct Velocity {...};
				//!   struct RigidBody {...};
				//!   struct Fuel {...};
				//!   auto player = w.add();
				//!   w.query().add("&Position; !Velocity; ?RigidBody; (Fuel,*); %e", player.value());
				//! Translates into:
				//!   w.query()
				//!      .all<Position&>()
				//!      .none<Velocity>()
				//!      .any<RigidBody>()
				//!      .all(Pair(w.add<Fuel>().entity, All)>()
				//!      .all(Player);
				//!
				//! \param str Null-terminated string with the query expression
				QueryImpl& add(const char* str, ...) {

					GAIA_ASSERT(str != nullptr);
					if (str == nullptr)
						return *this;

					va_list args{};
					va_start(args, str);

					uint32_t pos = 0;
					uint32_t exp0 = 0;

					auto process = [&]() {
						std::span<const char> exprRaw(&str[exp0], pos - exp0);
						exp0 = ++pos;

						auto expr = trim(exprRaw);
						if (expr.empty())
							return true;

						if (expr[0] == '+') {
							uint32_t off = 1;
							if (expr[1] == '&')
								off = 2;

							auto var = expr.subspan(off);
							auto entity = expr_to_entity(args, var);
							if (entity == EntityBad)
								return false;

							any(entity, off != 0);
						} else if (expr[0] == '!') {
							auto var = expr.subspan(1);
							auto entity = expr_to_entity(args, var);
							if (entity == EntityBad)
								return false;

							none(entity);
						} else {
							auto entity = expr_to_entity(args, expr);
							if (entity == EntityBad)
								return false;

							all(entity);
						}

						return true;
					};

					for (; str[pos] != 0; ++pos) {
						if (str[pos] == ';' && !process())
							goto add_end;
					}
					process();

				add_end:
					va_end(args);
					return *this;
				}

				QueryImpl& add(QueryItem item) {
					// Add commands to the command buffer
					add_inter(item);
					return *this;
				}

				QueryImpl& all(Entity entity, bool isReadWrite = false) {
					if (entity.pair())
						add({entity, QueryOp::All, QueryAccess::None});
					else
						add({entity, QueryOp::All, isReadWrite ? QueryAccess::Write : QueryAccess::Read});
					return *this;
				}

				template <typename... T>
				QueryImpl& all() {
					// Add commands to the command buffer
					(add_inter<T>(QueryOp::All), ...);
					return *this;
				}

				QueryImpl& any(Entity entity, bool isReadWrite = false) {
					if (entity.pair())
						add({entity, QueryOp::Any, QueryAccess::None});
					else
						add({entity, QueryOp::Any, isReadWrite ? QueryAccess::Write : QueryAccess::Read});
					return *this;
				}

				template <typename... T>
				QueryImpl& any() {
					// Add commands to the command buffer
					(add_inter<T>(QueryOp::Any), ...);
					return *this;
				}

				QueryImpl& none(Entity entity) {
					add({entity, QueryOp::Not, QueryAccess::None});
					return *this;
				}

				template <typename... T>
				QueryImpl& none() {
					// Add commands to the command buffer
					(add_inter<T>(QueryOp::Not), ...);
					return *this;
				}

				template <typename... T>
				QueryImpl& changed() {
					// Add commands to the command buffer
					(changed_inter<T>(), ...);
					return *this;
				}

				template <typename Func>
				void each(QueryInfo& queryInfo, Func func) {
					using InputArgs = decltype(core::func_args(&Func::operator()));

#if GAIA_DEBUG
					// Make sure we only use components specified in the query.
					// Constness is respected. Therefore, if a type is const when registered to query,
					// it has to be const (or immutable) also in each().
					// in query.
					// Example 1:
					//   auto q = w.query().all<MyType>(); // immutable access requested
					//   q.each([](MyType val)) {}); // okay
					//   q.each([](const MyType& val)) {}); // okay
					//   q.each([](MyType& val)) {}); // error
					// Example 2:
					//   auto q = w.query().all<MyType&>(); // mutable access requested
					//   q.each([](MyType val)) {}); // error
					//   q.each([](const MyType& val)) {}); // error
					//   q.each([](MyType& val)) {}); // okay
					GAIA_ASSERT(unpack_args_into_query_has_all(queryInfo, InputArgs{}));
#endif

					run_query_on_chunks<Iter>(queryInfo, [&](Chunk& chunk) {
						run_query_on_chunk<Iter>(chunk, func, InputArgs{});
					});
				}

				template <typename Func>
				void each(Func func) {
					auto& queryInfo = fetch();

					if constexpr (std::is_invocable_v<Func, IterAll>)
						run_query_on_chunks<IterAll>(queryInfo, [&](Chunk& chunk) {
							func(IterAll(chunk));
						});
					else if constexpr (std::is_invocable_v<Func, Iter>)
						run_query_on_chunks<Iter>(queryInfo, [&](Chunk& chunk) {
							func(Iter(chunk));
						});
					else if constexpr (std::is_invocable_v<Func, IterDisabled>)
						run_query_on_chunks<IterDisabled>(queryInfo, [&](Chunk& chunk) {
							func(IterDisabled(chunk));
						});
					else
						each(queryInfo, func);
				}

				template <typename Func, bool FuncEnabled = UseCaching, typename std::enable_if<FuncEnabled>::type* = nullptr>
				void each(QueryId queryId, Func func) {
					// Make sure the query was created by World.query()
					GAIA_ASSERT(m_storage.m_queryCache != nullptr);
					GAIA_ASSERT(queryId != QueryIdBad);

					auto& queryInfo = m_storage.m_queryCache->get(queryId);
					each(queryInfo, func);
				}

				/*!
					Returns true or false depending on whether there are any entities matching the query.
					\warning Only use if you only care if there are any entities matching the query.
									 The result is not cached and repeated calls to the function might be slow.
									 If you already called arr(), checking if it is empty is preferred.
									 Use empty() instead of calling count()==0.
					\return True if there are any entites matchine the query. False otherwise.
					*/
				bool empty(Constraints constraints = Constraints::EnabledOnly) {
					auto& queryInfo = fetch();
					const bool hasFilters = queryInfo.has_filters();

					if (hasFilters) {
						switch (constraints) {
							case Constraints::EnabledOnly: {
								for (auto* pArchetype: queryInfo)
									if (empty_inter<true, Iter>(queryInfo, pArchetype->chunks()))
										return false;
							} break;
							case Constraints::DisabledOnly: {
								for (auto* pArchetype: queryInfo)
									if (empty_inter<true, IterDisabled>(queryInfo, pArchetype->chunks()))
										return false;
							} break;
							case Constraints::AcceptAll: {
								for (auto* pArchetype: queryInfo)
									if (empty_inter<true, IterAll>(queryInfo, pArchetype->chunks()))
										return false;
							} break;
						}
					} else {
						switch (constraints) {
							case Constraints::EnabledOnly: {
								for (auto* pArchetype: queryInfo)
									if (empty_inter<false, Iter>(queryInfo, pArchetype->chunks()))
										return false;
							} break;
							case Constraints::DisabledOnly: {
								for (auto* pArchetype: queryInfo)
									if (empty_inter<false, IterDisabled>(queryInfo, pArchetype->chunks()))
										return false;
							} break;
							case Constraints::AcceptAll: {
								for (auto* pArchetype: queryInfo)
									if (empty_inter<false, IterAll>(queryInfo, pArchetype->chunks()))
										return false;
							} break;
						}
					}

					return true;
				}

				/*!
				Calculates the number of entities matching the query
				\warning Only use if you only care about the number of entities matching the query.
								 The result is not cached and repeated calls to the function might be slow.
								 If you already called arr(), use the size provided by the array.
								 Use empty() instead of calling count()==0.
				\return The number of matching entities
				*/
				uint32_t count(Constraints constraints = Constraints::EnabledOnly) {
					auto& queryInfo = fetch();
					uint32_t entCnt = 0;

					const bool hasFilters = queryInfo.has_filters();
					if (hasFilters) {
						switch (constraints) {
							case Constraints::EnabledOnly: {
								for (auto* pArchetype: queryInfo)
									entCnt += count_inter<true, Iter>(queryInfo, pArchetype->chunks());
							} break;
							case Constraints::DisabledOnly: {
								for (auto* pArchetype: queryInfo)
									entCnt += count_inter<true, IterDisabled>(queryInfo, pArchetype->chunks());
							} break;
							case Constraints::AcceptAll: {
								for (auto* pArchetype: queryInfo)
									entCnt += count_inter<true, IterAll>(queryInfo, pArchetype->chunks());
							} break;
						}
					} else {
						switch (constraints) {
							case Constraints::EnabledOnly: {
								for (auto* pArchetype: queryInfo)
									entCnt += count_inter<false, Iter>(queryInfo, pArchetype->chunks());
							} break;
							case Constraints::DisabledOnly: {
								for (auto* pArchetype: queryInfo)
									entCnt += count_inter<false, IterDisabled>(queryInfo, pArchetype->chunks());
							} break;
							case Constraints::AcceptAll: {
								for (auto* pArchetype: queryInfo)
									entCnt += count_inter<false, IterAll>(queryInfo, pArchetype->chunks());
							} break;
						}
					}

					return entCnt;
				}

				/*!
				Appends all components or entities matching the query to the output array
				\tparam outArray Container storing entities or components
				\param constraints QueryImpl constraints
				\return Array with entities or components
				*/
				template <typename Container>
				void arr(Container& outArray, Constraints constraints = Constraints::EnabledOnly) {
					const auto entCnt = count();
					if (entCnt == 0)
						return;

					outArray.reserve(entCnt);
					auto& queryInfo = fetch();

					const bool hasFilters = queryInfo.has_filters();
					if (hasFilters) {
						switch (constraints) {
							case Constraints::EnabledOnly:
								for (auto* pArchetype: queryInfo)
									arr_inter<true, Iter>(queryInfo, pArchetype->chunks(), outArray);
								break;
							case Constraints::DisabledOnly:
								for (auto* pArchetype: queryInfo)
									arr_inter<true, IterDisabled>(queryInfo, pArchetype->chunks(), outArray);
								break;
							case Constraints::AcceptAll:
								for (auto* pArchetype: queryInfo)
									arr_inter<true, IterAll>(queryInfo, pArchetype->chunks(), outArray);
								break;
						}
					} else {
						switch (constraints) {
							case Constraints::EnabledOnly:
								for (auto* pArchetype: queryInfo)
									arr_inter<false, Iter>(queryInfo, pArchetype->chunks(), outArray);
								break;
							case Constraints::DisabledOnly:
								for (auto* pArchetype: queryInfo)
									arr_inter<false, IterDisabled>(queryInfo, pArchetype->chunks(), outArray);
								break;
							case Constraints::AcceptAll:
								for (auto* pArchetype: queryInfo)
									arr_inter<false, IterAll>(queryInfo, pArchetype->chunks(), outArray);
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
