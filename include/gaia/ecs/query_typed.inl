#pragma once

#include "gaia/ecs/query_adapter_typed.inl"

namespace gaia {
	namespace ecs {
		namespace detail {
			inline TypedQueryExecState build_typed_query_exec_state(
					[[maybe_unused]] QueryImpl& query, World& world, const QueryInfo& queryInfo, const TypedQueryArgMeta* pMetas,
					uint32_t argCount) {
				TypedQueryExecState state{};
				QueryImpl::DirectChunkArgEvalDesc directChunkDescs[MAX_ITEMS_IN_QUERY]{};
				state.argCount = argCount;
				GAIA_FOR(argCount) {
					state.argIds[i] = pMetas[i].termId;
					state.writeFlags[i] = pMetas[i].isWrite;
					if (pMetas[i].isWrite) {
						state.hasWriteArgs = true;
						if (state.firstWriteArg == MAX_ITEMS_IN_QUERY)
							state.firstWriteArg = (uint32_t)i;
					}
					state.needsInheritedArgIds = state.needsInheritedArgIds || !pMetas[i].isEntity;
					directChunkDescs[i] = {pMetas[i].termId, pMetas[i].isEntity, pMetas[i].isPair};
				}

				state.canUseDirectChunkEval =
						QueryImpl::can_use_direct_chunk_term_eval_descs(world, queryInfo, directChunkDescs, argCount);
				if (state.needsInheritedArgIds)
					state.hasInheritedTerms = queryInfo.has_potential_inherited_id_terms();
				return state;
			}

#if GAIA_ECS_TEST_HOOKS
			template <typename Func>
			inline QueryImpl::QueryPlan QueryImpl::test_typed_plan(Func func) {
				auto& queryInfo = fetch();
				match_all(queryInfo);

				using InputArgs = decltype(core::func_args(&Func::operator()));
	#if GAIA_ASSERT_ENABLED
				GAIA_ASSERT(typed_query_args_match_query(queryInfo, InputArgs{}));
	#endif
				auto& world = *const_cast<World*>(queryInfo.world());
				TypedQueryArgMeta metas[MAX_ITEMS_IN_QUERY]{};
				const auto argCount = init_typed_query_arg_metas(metas, world, InputArgs{});
				const auto state = build_typed_query_exec_state(*this, world, queryInfo, metas, argCount);
				(void)func;
				return prepare_query_plan(queryInfo, state);
			}

			inline QueryImpl::QueryPlan QueryImpl::test_iter_plan(Constraints constraints) {
				auto& queryInfo = fetch();
				match_all(queryInfo);
				return prepare_query_plan(queryInfo, constraints);
			}
#endif

			inline void finish_typed_chunk_state(
					QueryImpl& query, World& world, Chunk* pChunk, uint16_t from, uint16_t to, const TypedQueryExecState& state);

			inline void finish_typed_iter_state(QueryImpl& query, Iter& it, const TypedQueryExecState& state);

			inline void noop_row_done(uint32_t) {}

			template <typename Func, typename ViewsTuple, typename... T>
			inline void invoke_typed_query_row_erased(
					void* pFunc, Iter& it, ViewsTuple& dataPointerTuple, uint32_t row, bool directLocalIndex);

			template <typename OnRowDone, typename... T>
			inline void run_typed_chunk_views(
					const QueryInfo* pQueryInfo, Iter& it, void* pFunc, bool directViews, OnRowDone&& onRowDone,
					void (*invokeDirectRow)(
							void*, Iter&, std::tuple<decltype(std::declval<Iter&>().template sview_auto<T>())...>&, uint32_t, bool),
					void (*invokeMappedRow)(
							void*, Iter&, std::tuple<decltype(std::declval<Iter&>().template view_auto_any<T>())...>&, uint32_t,
							bool),
					core::func_type_list<T...>);

			template <typename Func, typename... T>
			inline void run_typed_chunk_direct_iter_fast_cb(QueryImpl&, Iter& it, void* pFunc, const TypedQueryExecState&);

			template <typename Func, typename... T>
			inline void
			run_typed_chunk_direct_iter_cb(QueryImpl& query, Iter& it, void* pFunc, const TypedQueryExecState& state);

			template <typename Func, typename... T>
			inline void run_typed_chunk_mapped_iter_cb(
					QueryImpl& query, const QueryInfo& queryInfo, Iter& it, void* pFunc, const TypedQueryExecState& state);

			template <typename Func, typename... T>
			GAIA_NODISCARD inline auto typed_run_direct_fast_chunk_ptr(core::func_type_list<T...>) {
				return &run_typed_chunk_direct_iter_fast_cb<Func, T...>;
			}

			template <typename Func, typename... T>
			GAIA_NODISCARD inline auto typed_run_direct_chunk_ptr(core::func_type_list<T...>) {
				return &run_typed_chunk_direct_iter_cb<Func, T...>;
			}

			template <typename Func, typename... T>
			GAIA_NODISCARD inline auto typed_run_mapped_chunk_ptr(core::func_type_list<T...>) {
				return &run_typed_chunk_mapped_iter_cb<Func, T...>;
			}

			template <typename Func, typename... T>
			inline void run_typed_chunk_direct_iter_fast_cb(QueryImpl&, Iter& it, void* pFunc, const TypedQueryExecState&) {
				run_typed_chunk_views(
						nullptr, it, pFunc, true, noop_row_done,
						&invoke_typed_query_row_erased<
								Func, std::tuple<decltype(std::declval<Iter&>().template sview_auto<T>())...>, T...>,
						&invoke_typed_query_row_erased<
								Func, std::tuple<decltype(std::declval<Iter&>().template view_auto_any<T>())...>, T...>,
						core::func_type_list<T...>{});
			}

			template <typename Func, typename... T>
			inline void
			run_typed_chunk_direct_iter_cb(QueryImpl& query, Iter& it, void* pFunc, const TypedQueryExecState& state) {
				auto& func = *static_cast<Func*>(pFunc);
				run_typed_chunk_direct_finish(query, it, func, state, core::func_type_list<T...>{});
			}

			template <typename Func, typename... T>
			inline void run_typed_chunk_mapped_iter_cb(
					QueryImpl& query, const QueryInfo& queryInfo, Iter& it, void* pFunc, const TypedQueryExecState& state) {
				auto& func = *static_cast<Func*>(pFunc);
				run_typed_chunk_mapped_finish(query, queryInfo, it, func, state, core::func_type_list<T...>{});
			}

			template <typename Func, typename... T>
			inline void each_iter_dispatch(
					QueryImpl& query, const QueryInfo& queryInfo, Iter& it, Func& func, const TypedQueryExecState& state,
					core::func_type_list<T...>) {
				if (state.canUseDirectChunkEval) {
					run_typed_chunk_views(
							nullptr, it, &func, true, noop_row_done,
							&invoke_typed_query_row_erased<
									Func, std::tuple<decltype(std::declval<Iter&>().template sview_auto<T>())...>, T...>,
							&invoke_typed_query_row_erased<
									Func, std::tuple<decltype(std::declval<Iter&>().template view_auto_any<T>())...>, T...>,
							core::func_type_list<T...>{});
					finish_typed_chunk_state(
							query, *it.world(), const_cast<Chunk*>(it.chunk()), it.row_begin(), it.row_end(), state);
				} else
					run_typed_chunk_unmapped(query, queryInfo, it, func, state, core::func_type_list<T...>{});
			}

			template <typename Func, typename... T>
			inline void run_typed_chunk_direct_finish(
					QueryImpl& query, Iter& it, Func& func, const TypedQueryExecState& state, core::func_type_list<T...>) {
				run_typed_chunk_views(
						nullptr, it, &func, true, noop_row_done,
						&invoke_typed_query_row_erased<
								Func, std::tuple<decltype(std::declval<Iter&>().template sview_auto<T>())...>, T...>,
						&invoke_typed_query_row_erased<
								Func, std::tuple<decltype(std::declval<Iter&>().template view_auto_any<T>())...>, T...>,
						core::func_type_list<T...>{});
				finish_typed_iter_state(query, it, state);
			}

			template <typename Func, typename... T>
			inline void run_typed_chunk_mapped_finish(
					QueryImpl& query, const QueryInfo& queryInfo, Iter& it, Func& func, const TypedQueryExecState& state,
					core::func_type_list<T...>) {
				run_typed_chunk_views(
						&queryInfo, it, &func, false, noop_row_done,
						&invoke_typed_query_row_erased<
								Func, std::tuple<decltype(std::declval<Iter&>().template sview_auto<T>())...>, T...>,
						&invoke_typed_query_row_erased<
								Func, std::tuple<decltype(std::declval<Iter&>().template view_auto_any<T>())...>, T...>,
						core::func_type_list<T...>{});
				finish_typed_iter_state(query, it, state);
			}

			template <typename Func, typename... T>
			inline void run_typed_chunk_direct_walk_cb(
					QueryImpl& query, const QueryInfo&, Iter& it, void* pFunc, const TypedQueryExecState& state) {
				run_typed_chunk_direct_finish(query, it, *static_cast<Func*>(pFunc), state, core::func_type_list<T...>{});
			}

			template <typename Func, typename... T>
			inline void run_typed_chunk_mapped_walk_cb(
					QueryImpl& query, const QueryInfo& queryInfo, Iter& it, void* pFunc, const TypedQueryExecState& state) {
				run_typed_chunk_mapped_finish(
						query, queryInfo, it, *static_cast<Func*>(pFunc), state, core::func_type_list<T...>{});
			}

			template <typename Func, typename... T>
			inline void each_walk_dispatch_direct(
					QueryImpl& query, QueryInfo& queryInfo, std::span<const Entity> entities, Constraints constraints, Func& func,
					const TypedQueryExecState& state, core::func_type_list<T...>) {
				query.each_walk_inter(
						queryInfo, entities, constraints, &func, state, &run_typed_chunk_direct_walk_cb<Func, T...>);
			}

			template <typename Func, typename... T>
			inline void each_walk_dispatch_mapped(
					QueryImpl& query, QueryInfo& queryInfo, std::span<const Entity> entities, Constraints constraints, Func& func,
					const TypedQueryExecState& state, core::func_type_list<T...>) {
				query.each_walk_inter(
						queryInfo, entities, constraints, &func, state, &run_typed_chunk_mapped_walk_cb<Func, T...>);
			}

			template <typename Func, typename... T>
			inline void run_typed_chunk_unmapped(
					QueryImpl& query, const QueryInfo& queryInfo, Iter& it, Func& func, const TypedQueryExecState& state,
					core::func_type_list<T...> types) {
				auto& world = *const_cast<World*>(queryInfo.world());
				auto* pChunk = const_cast<Chunk*>(it.chunk());
				const bool hasEntityFilters = queryInfo.has_entity_filter_terms();

				if (!hasEntityFilters) {
					run_typed_chunk_views(
							&queryInfo, it, &func, false, noop_row_done,
							&invoke_typed_query_row_erased<
									Func, std::tuple<decltype(std::declval<Iter&>().template sview_auto<T>())...>, T...>,
							&invoke_typed_query_row_erased<
									Func, std::tuple<decltype(std::declval<Iter&>().template view_auto_any<T>())...>, T...>,
							types);
					finish_typed_chunk_state(query, world, pChunk, it.row_begin(), it.row_end(), state);
				} else {
					run_typed_chunk_views(
							&queryInfo, it, &func, false,
							[&](uint32_t row) {
								finish_typed_chunk_state(
										query, world, pChunk, (uint16_t)(it.row_begin() + row), (uint16_t)(it.row_begin() + row + 1),
										state);
							},
							&invoke_typed_query_row_erased<
									Func, std::tuple<decltype(std::declval<Iter&>().template sview_auto<T>())...>, T...>,
							&invoke_typed_query_row_erased<
									Func, std::tuple<decltype(std::declval<Iter&>().template view_auto_any<T>())...>, T...>,
							types);
				}

				it.clear_touched_writes();
			}

			template <typename Func, typename... T>
			inline void run_typed_cached_seed_runs_basic(
					QueryImpl& query, World& world, Constraints constraints, Func& func, std::span<const BfsChunkRun> runs,
					const TypedQueryExecState& state) {
				Iter it;
				it.init_query_state(&world, constraints, false);
				const Archetype* pLastArchetype = nullptr;
				for (const auto& run: runs) {
					if (run.pArchetype != pLastArchetype) {
						it.set_archetype(run.pArchetype);
						pLastArchetype = run.pArchetype;
					}

					it.set_chunk(run.pChunk, run.from, run.to);
					it.set_group_id(0);
					run_typed_chunk_direct_finish(query, it, func, state, core::func_type_list<T...>{});
				}
			}

			template <typename Func, typename... T>
			inline void run_typed_cached_seed_runs_entity_init(
					QueryImpl& query, QueryInfo& queryInfo, World& world, Constraints constraints, Func& func,
					std::span<const BfsChunkRun> runs, const TypedQueryExecState& state) {
				Iter it;
				it.init_query_state(&world, constraints, false);
				const Archetype* pLastArchetype = nullptr;
				uint8_t indices[ChunkHeader::MAX_COMPONENTS];
				Entity termIds[ChunkHeader::MAX_COMPONENTS];
				for (const auto& run: runs) {
					const auto& ec = ::gaia::ecs::fetch(world, run.pChunk->entity_view()[run.from]);
					query.init_direct_entity_iter(queryInfo, world, ec, it, indices, termIds, pLastArchetype);
					it.set_chunk(run.pChunk, run.from, run.to);
					it.set_group_id(0);
					run_typed_chunk_direct_finish(query, it, func, state, core::func_type_list<T...>{});
				}
			}

			template <typename Func, typename... T>
			inline void run_typed_cached_seed_runs(
					QueryImpl& query, QueryInfo& queryInfo, World& world, Constraints constraints, Func& func,
					std::span<const BfsChunkRun> runs, bool canUseBasicInit, const TypedQueryExecState& state) {
				if (canUseBasicInit)
					run_typed_cached_seed_runs_basic<Func, T...>(query, world, constraints, func, runs, state);
				else
					run_typed_cached_seed_runs_entity_init<Func, T...>(query, queryInfo, world, constraints, func, runs, state);
			}

			inline void QueryImpl::each_walk_inter(
					QueryInfo& queryInfo, std::span<const Entity> entities, Constraints constraints, void* pFunc,
					const TypedQueryExecState& state,
					void (*runChunk)(QueryImpl&, const QueryInfo&, Iter&, void*, const TypedQueryExecState&)) {
				auto& world = *queryInfo.world();
				auto& walkData = ensure_each_walk_data();
				Iter it;
				it.init_query_state(&world, constraints, false);
				if (!walkData.cachedRuns.empty()) {
					const auto& runs = walkData.cachedRuns;
					const Archetype* pLastArchetype = nullptr;
					for (const auto& run: runs) {
						if (run.pArchetype != pLastArchetype) {
							it.set_archetype(run.pArchetype);
							pLastArchetype = run.pArchetype;
						}

						it.set_chunk(run.pChunk, run.from, run.to);
						it.set_group_id(0);
						runChunk(*this, queryInfo, it, pFunc, state);
					}
					return;
				}

				const Archetype* pLastArchetype = nullptr;
				uint8_t indices[ChunkHeader::MAX_COMPONENTS];
				Entity termIds[ChunkHeader::MAX_COMPONENTS];
				for (const auto entity: entities) {
					const auto& ec = ::gaia::ecs::fetch(world, entity);
					init_direct_entity_iter(queryInfo, world, ec, it, indices, termIds, pLastArchetype);
					runChunk(*this, queryInfo, it, pFunc, state);
				}
			}

			inline void finish_typed_iter_state(QueryImpl& query, Iter& it, const TypedQueryExecState& state) {
				(void)query;
				query.finish_typed_iter_writes_runtime(it, state.argIds, state.writeFlags, state.argCount, state.firstWriteArg);
				it.clear_touched_writes();
			}

			inline void finish_typed_chunk_state(
					QueryImpl& query, World& world, Chunk* pChunk, uint16_t from, uint16_t to, const TypedQueryExecState& state) {
				(void)query;
				query.finish_typed_chunk_writes_runtime(
						world, pChunk, from, to, state.argIds, state.writeFlags, state.argCount, state.firstWriteArg);
			}

			template <typename InvokeRow, typename OnRowDone>
			inline void run_typed_query_rows_runtime(
					const QueryInfo* pQueryInfo, Iter& it, InvokeRow&& invokeRow, OnRowDone&& onRowDone) {
				const auto cnt = it.size();
				const bool hasEntityFilters = pQueryInfo != nullptr && pQueryInfo->has_entity_filter_terms();

				if (!hasEntityFilters) {
					GAIA_FOR(cnt) {
						invokeRow((uint32_t)i);
						onRowDone((uint32_t)i);
					}
					return;
				}

				const auto entities = it.template view<Entity>();
				GAIA_FOR(cnt) {
					if (!QueryImpl::match_entity_filters(*pQueryInfo->world(), entities[i], *pQueryInfo))
						continue;
					invokeRow((uint32_t)i);
					onRowDone((uint32_t)i);
				}
			}

			template <typename Func, typename ViewsTuple, typename... T>
			inline void invoke_typed_query_row(
					Iter& it, Func& func, ViewsTuple& dataPointerTuple, uint32_t row, bool directLocalIndex,
					core::func_type_list<T...>) {
				if constexpr (sizeof...(T) > 0) {
					if (directLocalIndex) {
						std::apply(
								[&](auto&... views) {
									func(views[row]...);
								},
								dataPointerTuple);
					} else {
						std::apply(
								[&](auto&... views) {
									func(views[it.template acc_index<T>(row)]...);
								},
								dataPointerTuple);
					}
				} else
					func();
			}

			template <typename Func, typename ViewsTuple, typename... T>
			inline void invoke_typed_query_row_erased(
					void* pFunc, Iter& it, ViewsTuple& dataPointerTuple, uint32_t row, bool directLocalIndex) {
				auto& func = *static_cast<Func*>(pFunc);
				invoke_typed_query_row(it, func, dataPointerTuple, row, directLocalIndex, core::func_type_list<T...>{});
			}

			template <typename OnRowDone, typename ViewsTuple>
			inline void run_typed_tuple_rows(
					const QueryInfo* pQueryInfo, Iter& it, void* pFunc, ViewsTuple& dataPointerTuple, bool directLocalIndex,
					void (*invokeRow)(void*, Iter&, ViewsTuple&, uint32_t, bool), OnRowDone&& onRowDone) {
				run_typed_query_rows_runtime(
						pQueryInfo, it,
						[&](uint32_t row) {
							invokeRow(pFunc, it, dataPointerTuple, row, directLocalIndex);
						},
						GAIA_FWD(onRowDone));
			}

			template <typename OnRowDone, typename... T>
			inline void run_typed_chunk_views(
					const QueryInfo* pQueryInfo, Iter& it, void* pFunc, bool directViews, OnRowDone&& onRowDone,
					void (*invokeDirectRow)(
							void*, Iter&, std::tuple<decltype(std::declval<Iter&>().template sview_auto<T>())...>&, uint32_t, bool),
					void (*invokeMappedRow)(
							void*, Iter&, std::tuple<decltype(std::declval<Iter&>().template view_auto_any<T>())...>&, uint32_t,
							bool),
					core::func_type_list<T...>) {
				if constexpr (sizeof...(T) > 0) {
					if (directViews) {
						auto dataPointerTuple = std::make_tuple(it.template sview_auto<T>()...);
						run_typed_tuple_rows(pQueryInfo, it, pFunc, dataPointerTuple, true, invokeDirectRow, GAIA_FWD(onRowDone));
					} else {
						auto dataPointerTuple = std::make_tuple(it.template view_auto_any<T>()...);
						run_typed_tuple_rows(pQueryInfo, it, pFunc, dataPointerTuple, false, invokeMappedRow, GAIA_FWD(onRowDone));
					}
				} else {
					auto dataPointerTuple = std::tuple<>{};
					run_typed_tuple_rows(
							pQueryInfo, it, pFunc, dataPointerTuple, directViews, directViews ? invokeDirectRow : invokeMappedRow,
							GAIA_FWD(onRowDone));
				}
			}

			template <typename ContainerOut>
			inline void typed_arr_push(World& world, Entity entity, ContainerOut& outArray) {
				using ContainerItemType = typename ContainerOut::value_type;
				if constexpr (std::is_same_v<ContainerItemType, Entity>)
					outArray.push_back(entity);
				else {
					auto tmp = world_direct_entity_arg<ContainerItemType>(world, entity);
					outArray.push_back(tmp);
				}
			}

			template <bool UseFilters, typename ContainerOut>
			inline void run_typed_arr_rows(
					QueryImpl& query, const QueryInfo& queryInfo, Iter& it, ContainerOut& outArray, uint32_t changedWorldVersion,
					uint32_t archetypeIdx, const Archetype* pArchetype, Chunk* pChunk, uint16_t from, uint16_t to,
					bool needsBarrierCache, bool canUseDirectChunkEval) {
				using ContainerItemType = typename ContainerOut::value_type;
				const bool barrierPasses = !needsBarrierCache || queryInfo.barrier_passes(archetypeIdx);
				if GAIA_UNLIKELY (!query.can_process_archetype_inter(queryInfo, *pArchetype, it.constraints(), barrierPasses))
					return;
				if GAIA_UNLIKELY (from == to)
					return;

				GAIA_PROF_SCOPE(query::arr);

				it.set_archetype(pArchetype);
				it.set_chunk(pChunk, from, to);
				it.set_group_id(0);

				const auto cnt = it.size();
				if (cnt == 0)
					return;

				if constexpr (UseFilters) {
					if (!QueryImpl::match_filters(*pChunk, queryInfo, changedWorldVersion))
						return;
				}

				const bool hasEntityFilters = queryInfo.has_entity_filter_terms();
				if (canUseDirectChunkEval) {
					const auto dataView = it.template sview_auto<ContainerItemType>();
					GAIA_FOR(cnt) {
						auto tmp = dataView[i];
						outArray.push_back(tmp);
					}
					return;
				}

				const auto dataView = it.template view<ContainerItemType>();
				if (!hasEntityFilters) {
					GAIA_FOR(cnt) {
						const auto idx = it.template acc_index<ContainerItemType>(i);
						auto tmp = dataView[idx];
						outArray.push_back(tmp);
					}
					return;
				}

				const auto entities = it.template view<Entity>();
				GAIA_FOR(cnt) {
					if (!QueryImpl::match_entity_filters(*queryInfo.world(), entities[i], queryInfo))
						continue;
					const auto idx = it.template acc_index<ContainerItemType>(i);
					auto tmp = dataView[idx];
					outArray.push_back(tmp);
				}
			}

			inline void QueryImpl::finish_typed_chunk_writes_runtime(
					World& world, Chunk* pChunk, uint16_t from, uint16_t to, const Entity* pArgIds, const bool* pWriteFlags,
					uint32_t argCnt, uint32_t firstWriteArg) {
				if (firstWriteArg >= argCnt || from >= to)
					return;

				Entity seenTerms[ChunkHeader::MAX_COMPONENTS]{};
				uint32_t seenCnt = 0;
				const auto entities = pChunk->entity_view();
				const auto finish_term = [&](Entity term) {
					GAIA_FOR(seenCnt) {
						if (seenTerms[i] == term)
							return;
					}

					seenTerms[seenCnt++] = term;
					if (!world_is_out_of_line_component(world, term)) {
						const auto compIdx = core::get_index(pChunk->ids_view(), term);
						if (compIdx != BadIndex) {
							pChunk->finish_write(compIdx, from, to);
							return;
						}
					}

					for (uint16_t row = from; row < to; ++row)
						world_finish_write(world, term, entities[row]);
				};

				for (uint32_t i = firstWriteArg; i < argCnt; ++i) {
					if (!pWriteFlags[i])
						continue;
					const auto term = pArgIds[i];
					if (term != EntityBad)
						finish_term(term);
				}
			}

			template <typename... T>
			inline void QueryImpl::finish_typed_chunk_writes(World& world, Chunk* pChunk, uint16_t from, uint16_t to) {
				TypedQueryArgMeta metas[MAX_ITEMS_IN_QUERY]{};
				const auto argCount = init_typed_query_arg_metas(metas, world, core::func_type_list<T...>{});
				Entity argIds[MAX_ITEMS_IN_QUERY]{};
				bool writeFlags[MAX_ITEMS_IN_QUERY]{};
				uint32_t firstWriteArg = argCount;
				GAIA_FOR(argCount) {
					argIds[i] = metas[i].termId;
					writeFlags[i] = metas[i].isWrite;
					if (metas[i].isWrite && firstWriteArg == argCount)
						firstWriteArg = (uint32_t)i;
				}
				finish_typed_chunk_writes_runtime(world, pChunk, from, to, argIds, writeFlags, argCount, firstWriteArg);
			}

			inline void QueryImpl::finish_typed_iter_writes_runtime(
					Iter& it, const Entity* pArgIds, const bool* pWriteFlags, uint32_t argCnt, uint32_t firstWriteArg) {
				if (firstWriteArg >= argCnt)
					return;

				auto* pChunk = const_cast<Chunk*>(it.chunk());
				if (pChunk == nullptr || it.row_begin() >= it.row_end())
					return;

				auto& world = *it.world();
				auto entities = it.entity_rows();
				Entity seenTerms[ChunkHeader::MAX_COMPONENTS]{};
				uint32_t seenCnt = 0;
				const auto finish_term = [&](Entity term, uint32_t fieldIdx) {
					GAIA_FOR(seenCnt) {
						if (seenTerms[i] == term)
							return;
					}

					seenTerms[seenCnt++] = term;
					const bool isOutOfLine = world_is_out_of_line_component(world, term);
					auto compIdx = uint8_t(0xFF);
					if (it.comp_indices() != nullptr && fieldIdx < ChunkHeader::MAX_COMPONENTS)
						compIdx = it.comp_indices()[fieldIdx];
					else if (!isOutOfLine)
						compIdx = (uint8_t)pChunk->comp_idx(term);

					if (compIdx != 0xFF && !isOutOfLine) {
						pChunk->finish_write(compIdx, it.row_begin(), it.row_end());
						return;
					}

					GAIA_FOR(entities.size()) {
						world_finish_write(world, term, entities[i]);
					}
				};

				for (uint32_t i = firstWriteArg; i < argCnt; ++i) {
					if (!pWriteFlags[i])
						continue;

					Entity term = pArgIds[i];
					if (it.term_ids() != nullptr && i < ChunkHeader::MAX_COMPONENTS && it.term_ids()[i] != EntityBad)
						term = it.term_ids()[i];
					if (term == EntityBad)
						continue;

					finish_term(term, i);
				}
			}

			//! Returns a typed direct-chunk view element.
			//! SoA views are chunk-relative, so they need the absolute row offset. AoS views are already sliced.
			//! \tparam T Typed query argument represented by @a view.
			//! \tparam View Prepared chunk view type.
			//! \param view Direct chunk view for the argument.
			//! \param row Row relative to the currently processed chunk range.
			//! \param from Absolute row offset of the current chunk range.
			//! \return Reference or value selected from @a view for the current row.
			//! \see run_typed_direct_chunk_rows(Chunk*, uint16_t, uint16_t, Func&, const TypedQueryExecState&,
			//! core::func_type_list<T...>)
			template <typename T, typename View>
			GAIA_NODISCARD inline decltype(auto) typed_direct_chunk_arg_at(View& view, uint32_t row, uint16_t from) {
				using U = typename actual_type_t<T>::Type;
				if constexpr (mem::is_soa_layout_v<U>)
					return view[from + row];
				else
					return view[row];
			}

			//! Invokes a row callback from prepared direct chunk views.
			//! \tparam Func Callback type.
			//! \tparam ViewsTuple Tuple type containing prepared direct chunk views.
			//! \tparam T Typed query argument list.
			//! \tparam I Tuple indices matching @a T.
			//! \param func Callback invoked for the selected row.
			//! \param views Prepared direct chunk views.
			//! \param row Row relative to the current chunk range.
			//! \param from Absolute row offset of the current chunk range.
			//! \see typed_direct_chunk_arg_at(View&, uint32_t, uint16_t)
			template <typename Func, typename ViewsTuple, typename... T, size_t... I>
			inline void invoke_typed_direct_chunk_row(
					Func& func, ViewsTuple& views, uint32_t row, uint16_t from, core::func_type_list<T...>,
					std::index_sequence<I...>) {
				func(typed_direct_chunk_arg_at<T>(std::get<I>(views), row, from)...);
			}

			//! Runs a typed row callback directly on a chunk without constructing Iter.
			//! \tparam Func Callback type.
			//! \tparam T Typed query argument list.
			//! \param pChunk Chunk supplying component data.
			//! \param from First absolute row to process.
			//! \param to One-past-the-end absolute row to process.
			//! \param func Callback invoked for each matching row.
			//! \param state Typed callback metadata, including component ids for direct chunk access.
			//! \note This fast path intentionally bypasses Iter construction and therefore does not expose Iter::ctx().
			//! \note Single-argument callbacks are common in fragmented hierarchy queries. They avoid tuple/view
			//! construction and per-row index-sequence expansion here because many tiny chunks make that adapter cost
			//! visible.
			template <typename Func, typename... T>
			inline void run_typed_direct_chunk_rows(
					Chunk* pChunk, uint16_t from, uint16_t to, Func& func, const TypedQueryExecState& state,
					core::func_type_list<T...>) {
				if constexpr (sizeof...(T) == 1) {
					//! Keep the one-arg path explicit. The multi-arg path below still needs the generic view tuple because
					//! each field can have different Entity/AoS/SoA access rules.
					using T0 = std::tuple_element_t<0, std::tuple<T...>>;
					using U = typename actual_type_t<T0>::Type;
					const auto cnt = (uint32_t)(to - from);
					if constexpr (std::is_same_v<U, Entity>) {
						const auto data = pChunk->entity_view();
						GAIA_FOR(cnt)
						func(data[from + i]);
					} else if constexpr (!mem::is_soa_layout_v<U>) {
						//! Use the chunk-local component lookup. QueryInfo owns query-specific field-to-column caches;
						//! doing world/archetype-level lookup here is not equivalent and is expensive on many-archetype
						//! ChildOf-style workloads where there is usually no per-archetype-to-many-chunk amortization.
						const auto compIdx = pChunk->comp_idx(state.argIds[0]);
						const auto recs = pChunk->comp_rec_view();
						auto* data = (U*)recs[compIdx].pData + from;
						GAIA_FOR(cnt)
						func(data[i]);
					} else {
						auto view = pChunk->template sview_auto<T0>(from, to);
						GAIA_FOR(cnt)
						func(view[from + i]);
					}
				} else if constexpr (sizeof...(T) > 0) {
					auto views = std::make_tuple(pChunk->template sview_auto<T>(from, to)...);
					const auto cnt = (uint32_t)(to - from);
					GAIA_FOR(cnt)
					invoke_typed_direct_chunk_row(
							func, views, (uint32_t)i, from, core::func_type_list<T...>{}, std::index_sequence_for<T...>{});
				} else {
					const auto cnt = (uint32_t)(to - from);
					GAIA_FOR(cnt)
					func();
				}
			}

			//! Selects the prepared execution plan for typed callbacks.
			//! \param queryInfo Prepared query cache and execution metadata.
			//! \param state Typed callback execution metadata derived from callback arguments.
			//! \return Query execution plan shared by typed and public iterator adapters.
			inline QueryImpl::QueryPlan
			QueryImpl::prepare_query_plan(const QueryInfo& queryInfo, const TypedQueryExecState& state) const {
				QueryPlan plan{};
				plan.payloadKind = exec_payload_kind(queryInfo, Constraints::EnabledOnly);

				const bool hasFilters = queryInfo.has_filters();
				const bool hasSortedPayload = queryInfo.has_sorted_payload();
				const bool hasDepthOrderBarrier = has_depth_order_hierarchy_enabled_barrier(queryInfo);
				const bool depthOrderBarrierPrunes = hasDepthOrderBarrier && depth_order_hierarchy_barrier_prunes(queryInfo);
				if (hasFilters)
					plan.flags |= QueryPlanFlag_Filtered;
				if (queryInfo.has_entity_filter_terms())
					plan.flags |= QueryPlanFlag_EntityFilter;
				if (queryInfo.has_inherited_data_payload())
					plan.flags |= QueryPlanFlag_InheritedPayload;
				if (queryInfo.has_grouped_payload())
					plan.flags |= QueryPlanFlag_Grouped;
				if (hasSortedPayload || hasDepthOrderBarrier)
					plan.flags |= QueryPlanFlag_Sorted;
				if (hasDepthOrderBarrier && !depthOrderBarrierPrunes)
					plan.payloadKind = ExecPayloadKind::Grouped;
				if (depthOrderBarrierPrunes)
					plan.flags |= QueryPlanFlag_BarrierCache;

				const bool canDirectEntitySeed = !hasFilters && can_use_direct_entity_seed_eval(queryInfo);
				const bool canDirectChunks = !hasSortedPayload && (!hasDepthOrderBarrier || !depthOrderBarrierPrunes);

				auto setDenseRange = [&]() -> bool {
					const auto cacheRange = selected_query_cache_range(queryInfo);
					plan.idxFrom = cacheRange.idxFrom;
					plan.idxTo = cacheRange.idxTo;
					if (cacheRange.hasSelectedGroup) {
						plan.flags |= QueryPlanFlag_Grouped;
						plan.payloadKind = ExecPayloadKind::Grouped;
						if (!cacheRange.valid)
							return false;
					}
					return plan.idxFrom < plan.idxTo;
				};

				if (state.canUseDirectChunkEval && !canDirectEntitySeed && canDirectChunks) {
					if (!setDenseRange()) {
						plan.mode = QueryPlanMode::Empty;
						plan.idxFrom = 0;
						plan.idxTo = 0;
						return plan;
					}

					plan.mode = QueryPlanMode::DirectDense;
					return plan;
				}

				if (canDirectEntitySeed) {
					plan.mode = QueryPlanMode::EntitySeed;
					return plan;
				}

				if (!setDenseRange()) {
					plan.mode = QueryPlanMode::Empty;
					plan.idxFrom = 0;
					plan.idxTo = 0;
					return plan;
				}

				if (hasSortedPayload) {
					plan.mode = QueryPlanMode::Sorted;
					plan.payloadKind = ExecPayloadKind::NonTrivial;
					return plan;
				}

				if (hasDepthOrderBarrier && depthOrderBarrierPrunes) {
					plan.mode = QueryPlanMode::Traversal;
					plan.payloadKind = ExecPayloadKind::NonTrivial;
					return plan;
				}

				if ((plan.flags & QueryPlanFlag_InheritedPayload) != 0) {
					plan.mode = QueryPlanMode::Traversal;
					plan.payloadKind = ExecPayloadKind::NonTrivial;
					return plan;
				}

				if (hasDepthOrderBarrier) {
					plan.payloadKind = ExecPayloadKind::Grouped;
					return plan;
				}

				if (!canDirectChunks) {
					plan.mode = QueryPlanMode::Sorted;
					return plan;
				}

				if (!state.canUseDirectChunkEval) {
					plan.mode = QueryPlanMode::MappedDense;
					return plan;
				}

				return plan;
			}

			//! Runs the prepared direct typed row path for simple cached queries.
			//! \tparam HasFilters True when changed/per-chunk filters must be evaluated.
			//! \tparam Func Callback type.
			//! \tparam T Typed query argument list.
			//! \param queryInfo Prepared query cache and execution metadata.
			//! \param plan Prepared query execution plan.
			//! \param state Typed callback execution metadata.
			//! \param func Callback invoked for each matching row.
			//! \param types Type-list tag identifying callback arguments.
			//! \see prepare_query_plan(const QueryInfo&, const TypedQueryExecState&) const
			template <bool HasFilters, typename Func, typename... T>
			inline void QueryImpl::run_query_on_chunks_direct_typed(
					QueryInfo& queryInfo, const QueryPlan& plan, const TypedQueryExecState& state, Func& func,
					core::func_type_list<T...> types) {
				auto& world = *queryInfo.world();
				if constexpr (HasFilters) {
					GAIA_ASSERT(plan.mode == QueryPlanMode::DirectDense);
					GAIA_ASSERT((plan.flags & QueryPlanFlag_Filtered) != 0);
				} else {
					GAIA_ASSERT(plan.mode == QueryPlanMode::DirectDense);
				}

				auto cacheView = queryInfo.cache_archetype_view();
				if (plan.idxFrom >= plan.idxTo)
					return;

				if (state.hasWriteArgs)
					::gaia::ecs::update_version(*m_worldVersion);

				const bool canSkipProcessCheck =
						!queryInfo.result_cache_may_need_prefab_filter() && (plan.flags & QueryPlanFlag_BarrierCache) == 0;
				lock(*m_storage.world());
				for (uint32_t i = plan.idxFrom; i < plan.idxTo; ++i) {
					const auto* pArchetype = cacheView[i];
					if (canSkipProcessCheck) {
						if GAIA_UNLIKELY (pArchetype->is_req_del())
							continue;
					} else if GAIA_UNLIKELY (!can_process_archetype_inter(queryInfo, *pArchetype, Constraints::EnabledOnly))
						continue;

					std::span<const uint8_t> indicesView;
					if constexpr (HasFilters)
						indicesView = queryInfo.indices_mapping_view(i);

					const auto& chunks = pArchetype->chunks();
					for (auto* pChunk: chunks) {
						const auto from = Iter::start_index(pChunk);
						const auto to = Iter::end_index(pChunk);
						if GAIA_UNLIKELY (from == to)
							continue;

						if constexpr (HasFilters) {
							if GAIA_UNLIKELY (!match_filters(*pChunk, queryInfo, m_changedWorldVersion, indicesView))
								continue;
						}

						GAIA_PROF_SCOPE(query_func);
						run_typed_direct_chunk_rows(pChunk, from, to, func, state, types);
						if (state.hasWriteArgs)
							finish_typed_chunk_state(*this, world, pChunk, from, to, state);
					}
				}

				unlock(*m_storage.world());
				commit_cmd_buffer_st(*m_storage.world());
				commit_cmd_buffer_mt(*m_storage.world());
				m_changedWorldVersion = *m_worldVersion;
			}

			inline void QueryImpl::run_query_on_chunks_direct_iter(
					QueryInfo& queryInfo, const QueryPlan& plan, const TypedQueryExecState& state, void* pFunc,
					void (*runChunk)(QueryImpl&, Iter& it, void*, const TypedQueryExecState&)) {
				auto& world = *queryInfo.world();
				GAIA_ASSERT(plan.mode == QueryPlanMode::DirectDense);
				GAIA_ASSERT(!queryInfo.has_filters());
				auto cacheView = queryInfo.cache_archetype_view();
				if (plan.idxFrom >= plan.idxTo)
					return;

				if (state.hasWriteArgs)
					::gaia::ecs::update_version(*m_worldVersion);

				lock(*m_storage.world());
				Iter it;
				it.init_query_state(queryInfo.world(), Constraints::EnabledOnly, false);
				const Archetype* pLastArchetype = nullptr;
				for (uint32_t i = plan.idxFrom; i < plan.idxTo; ++i) {
					const auto* pArchetype = cacheView[i];
					if GAIA_UNLIKELY (!can_process_archetype_inter(queryInfo, *pArchetype, Constraints::EnabledOnly))
						continue;

					const auto& chunks = pArchetype->chunks();
					for (auto* pChunk: chunks) {
						const auto from = Iter::start_index(pChunk);
						const auto to = Iter::end_index(pChunk);
						if GAIA_UNLIKELY (from == to)
							continue;

						GAIA_PROF_SCOPE(query_func);
						if (pArchetype != pLastArchetype) {
							it.set_archetype(pArchetype);
							pLastArchetype = pArchetype;
						}
						it.set_chunk(pChunk, from, to);
						it.set_group_id(0);
						it.ctx(m_ctx);
						runChunk(*this, it, pFunc, state);
						finish_typed_chunk_state(*this, world, pChunk, from, to, state);
					}
				}

				unlock(*m_storage.world());
				commit_cmd_buffer_st(*m_storage.world());
				commit_cmd_buffer_mt(*m_storage.world());
				m_changedWorldVersion = *m_worldVersion;
			}

			inline void QueryImpl::run_query_on_chunks_direct(
					QueryInfo& queryInfo, const QueryPlan& plan, const TypedQueryExecState& state, void* pFunc,
					void (*runChunk)(QueryImpl&, Iter& it, void*, const TypedQueryExecState&)) {
				auto& world = *queryInfo.world();
				auto cacheView = queryInfo.cache_archetype_view();
				if (plan.idxFrom >= plan.idxTo)
					return;

				if (state.hasWriteArgs)
					::gaia::ecs::update_version(*m_worldVersion);

				GAIA_ASSERT(plan.mode == QueryPlanMode::DirectDense);
				const bool hasFilters = (plan.flags & QueryPlanFlag_Filtered) != 0;

				lock(*m_storage.world());
				Iter it;
				it.init_query_state(queryInfo.world(), Constraints::EnabledOnly, false);
				const Archetype* pLastArchetype = nullptr;

				for (uint32_t i = plan.idxFrom; i < plan.idxTo; ++i) {
					const auto* pArchetype = cacheView[i];
					if GAIA_UNLIKELY (!can_process_archetype_inter(queryInfo, *pArchetype, Constraints::EnabledOnly))
						continue;

					std::span<const uint8_t> indicesView;
					if (hasFilters)
						indicesView = queryInfo.indices_mapping_view(i);

					const auto& chunks = pArchetype->chunks();
					for (auto* pChunk: chunks) {
						const auto from = Iter::start_index(pChunk);
						const auto to = Iter::end_index(pChunk);
						if GAIA_UNLIKELY (from == to)
							continue;

						if (hasFilters) {
							if GAIA_UNLIKELY (!match_filters(*pChunk, queryInfo, m_changedWorldVersion, indicesView))
								continue;
						}

						GAIA_PROF_SCOPE(query_func);
						if (pArchetype != pLastArchetype) {
							it.set_archetype(pArchetype);
							pLastArchetype = pArchetype;
						}
						it.set_chunk(pChunk, from, to);
						it.set_group_id(0);
						it.ctx(m_ctx);
						runChunk(*this, it, pFunc, state);
						finish_typed_chunk_state(*this, world, pChunk, from, to, state);
					}
				}

				unlock(*m_storage.world());
				commit_cmd_buffer_st(*m_storage.world());
				commit_cmd_buffer_mt(*m_storage.world());
				m_changedWorldVersion = *m_worldVersion;
			}

			template <QueryExecType ExecType>
			inline void QueryImpl::each_inter(
					QueryInfo& queryInfo, const QueryPlan& plan, void* pFunc, const TypedQueryExecState& state,
					void (*runDirectFastChunk)(QueryImpl&, Iter&, void*, const TypedQueryExecState&),
					void (*runDirectChunk)(QueryImpl&, Iter&, void*, const TypedQueryExecState&),
					void (*runMappedChunk)(QueryImpl&, const QueryInfo&, Iter&, void*, const TypedQueryExecState&),
					bool needsInheritedArgIds, void (*invokeInherited)(World&, Entity, const Entity*, void*)) {
				if (plan.mode == QueryPlanMode::Empty)
					return;

				if (plan.mode == QueryPlanMode::EntitySeed) {
					GAIA_PROF_SCOPE(query_func);
					each_direct_inter(
							queryInfo, Constraints::EnabledOnly, pFunc, state, runDirectChunk, needsInheritedArgIds, invokeInherited);
					return;
				}

				if (state.canUseDirectChunkEval) {
					if constexpr (ExecType == QueryExecType::Default) {
						if (plan.mode == QueryPlanMode::DirectDense) {
							run_query_on_chunks_direct(queryInfo, plan, state, pFunc, runDirectFastChunk);
							return;
						}
					}
					TypedDirectChunkCallback cb{this, pFunc, &state, runDirectChunk};
					run_query_on_chunks<ExecType, IterModeEnabled>(queryInfo, cb);
				} else {
					TypedMappedChunkCallback cb{this, &queryInfo, pFunc, &state, runMappedChunk};
					run_query_on_chunks<ExecType, IterModeEnabled>(queryInfo, cb);
				}
			}

			template <QueryExecType ExecType, typename Func>
			inline void QueryImpl::each_typed_inter(QueryInfo& queryInfo, Func func) {
				using InputArgs = decltype(core::func_args(&Func::operator()));

#if GAIA_ASSERT_ENABLED
				GAIA_ASSERT(typed_query_args_match_query(queryInfo, InputArgs{}));
#endif
				auto& world = *const_cast<World*>(queryInfo.world());
				TypedQueryArgMeta metas[MAX_ITEMS_IN_QUERY]{};
				const auto argCount = init_typed_query_arg_metas(metas, world, InputArgs{});
				const auto state = build_typed_query_exec_state(*this, world, queryInfo, metas, argCount);
				const auto plan = prepare_query_plan(queryInfo, state);
				if constexpr (ExecType == QueryExecType::Default) {
					if (plan.mode == QueryPlanMode::DirectDense) {
						if ((plan.flags & QueryPlanFlag_Filtered) != 0)
							run_query_on_chunks_direct_typed<true>(queryInfo, plan, state, func, InputArgs{});
						else
							run_query_on_chunks_direct_typed<false>(queryInfo, plan, state, func, InputArgs{});
						return;
					}
				}

				const auto runDirectFastChunk = typed_run_direct_fast_chunk_ptr<Func>(InputArgs{});
				const auto runDirectChunk = typed_run_direct_chunk_ptr<Func>(InputArgs{});
				const auto runMappedChunk = typed_run_mapped_chunk_ptr<Func>(InputArgs{});
				const auto invokeInherited = typed_invoke_inherited_ptr<Func>(InputArgs{});
				each_inter<ExecType>(
						queryInfo, plan, &func, state, runDirectFastChunk, runDirectChunk, runMappedChunk,
						state.needsInheritedArgIds, invokeInherited);
			}

			inline void QueryImpl::each_typed_erased(
					QueryExecType execType, void* pFunc, const TypedQueryExecState& state,
					void (*runDirectFastChunk)(QueryImpl&, Iter&, void*, const TypedQueryExecState&),
					void (*runDirectChunk)(QueryImpl&, Iter&, void*, const TypedQueryExecState&),
					void (*runMappedChunk)(QueryImpl&, const QueryInfo&, Iter&, void*, const TypedQueryExecState&),
					bool needsInheritedArgIds, void (*invokeInherited)(World&, Entity, const Entity*, void*)) {
				auto& queryInfo = fetch();
				match_all(queryInfo);
				const auto plan = prepare_query_plan(queryInfo, state);

				switch (execType) {
					case QueryExecType::Parallel:
						each_inter<QueryExecType::Parallel>(
								queryInfo, plan, pFunc, state, runDirectFastChunk, runDirectChunk, runMappedChunk, needsInheritedArgIds,
								invokeInherited);
						break;
					case QueryExecType::ParallelPerf:
						each_inter<QueryExecType::ParallelPerf>(
								queryInfo, plan, pFunc, state, runDirectFastChunk, runDirectChunk, runMappedChunk, needsInheritedArgIds,
								invokeInherited);
						break;
					case QueryExecType::ParallelEff:
						each_inter<QueryExecType::ParallelEff>(
								queryInfo, plan, pFunc, state, runDirectFastChunk, runDirectChunk, runMappedChunk, needsInheritedArgIds,
								invokeInherited);
						break;
					default:
						each_inter<QueryExecType::Default>(
								queryInfo, plan, pFunc, state, runDirectFastChunk, runDirectChunk, runMappedChunk, needsInheritedArgIds,
								invokeInherited);
						break;
				}
			}

			template <typename Func, std::enable_if_t<!detail::is_query_iter_callback_v<Func>, int>>
			inline void QueryImpl::each(Func func) {
				each(func, QueryExecType::Default);
			}

			template <typename Func, std::enable_if_t<!detail::is_query_iter_callback_v<Func>, int>>
			inline void QueryImpl::each(Func func, QueryExecType execType) {
				auto& queryInfo = fetch();
				match_all(queryInfo);

				switch (execType) {
					case QueryExecType::Parallel:
						each_typed_inter<QueryExecType::Parallel>(queryInfo, func);
						break;
					case QueryExecType::ParallelPerf:
						each_typed_inter<QueryExecType::ParallelPerf>(queryInfo, func);
						break;
					case QueryExecType::ParallelEff:
						each_typed_inter<QueryExecType::ParallelEff>(queryInfo, func);
						break;
					default:
						each_typed_inter<QueryExecType::Default>(queryInfo, func);
						break;
				}
			}

			template <QueryExecType ExecType>
			inline void QueryImpl::each_iter_inter_erased(
					QueryInfo& queryInfo, const QueryPlan& plan, void* pFunc, const TypedQueryExecState& state,
					void (*runDirectFastChunk)(QueryImpl&, Iter&, void*, const TypedQueryExecState&),
					void (*runMappedChunk)(QueryImpl&, const QueryInfo&, Iter&, void*, const TypedQueryExecState&)) {
				TypedIterErasedCallback cb{this, pFunc, &state, runDirectFastChunk, runMappedChunk};

				if (plan.mode == QueryPlanMode::Empty)
					return;

				if (plan.mode == QueryPlanMode::EntitySeed) {
					each_direct_iter_inter(queryInfo, Constraints::EnabledOnly, cb);
					return;
				}

				if constexpr (ExecType == QueryExecType::Default) {
					if (plan.mode == QueryPlanMode::DirectDense) {
						run_query_on_chunks_direct_iter(queryInfo, plan, state, pFunc, runDirectFastChunk);
						return;
					}
				}

				run_query_on_chunks<ExecType, IterModeEnabled>(queryInfo, cb);
			}

			inline void QueryImpl::each_iter_erased(
					QueryExecType execType, void* pFunc, const TypedQueryExecState& state,
					void (*runDirectFastChunk)(QueryImpl&, Iter&, void*, const TypedQueryExecState&),
					void (*runMappedChunk)(QueryImpl&, const QueryInfo&, Iter&, void*, const TypedQueryExecState&)) {
				auto& queryInfo = fetch();
				match_all(queryInfo);
				const auto plan = prepare_query_plan(queryInfo, state);

				switch (execType) {
					case QueryExecType::Parallel:
						each_iter_inter_erased<QueryExecType::Parallel>(
								queryInfo, plan, pFunc, state, runDirectFastChunk, runMappedChunk);
						break;
					case QueryExecType::ParallelPerf:
						each_iter_inter_erased<QueryExecType::ParallelPerf>(
								queryInfo, plan, pFunc, state, runDirectFastChunk, runMappedChunk);
						break;
					case QueryExecType::ParallelEff:
						each_iter_inter_erased<QueryExecType::ParallelEff>(
								queryInfo, plan, pFunc, state, runDirectFastChunk, runMappedChunk);
						break;
					default:
						each_iter_inter_erased<QueryExecType::Default>(
								queryInfo, plan, pFunc, state, runDirectFastChunk, runMappedChunk);
						break;
				}
			}

			template <typename Func>
			inline void QueryImpl::each_iter(Iter& it, Func func) {
				using InputArgs = decltype(core::func_args(&Func::operator()));
				auto& queryInfo = fetch();

#if GAIA_ASSERT_ENABLED
				GAIA_ASSERT(typed_query_args_match_query(queryInfo, InputArgs{}));
#endif
				TypedQueryArgMeta metas[MAX_ITEMS_IN_QUERY]{};
				const auto argCount = init_typed_query_arg_metas(metas, *it.world(), InputArgs{});
				const auto state = build_typed_query_exec_state(*this, *it.world(), queryInfo, metas, argCount);
				it.ctx(m_ctx);
				each_iter_dispatch(*this, queryInfo, it, func, state, InputArgs{});
			}

			inline void QueryImpl::each_iter_erased(
					Iter& it, void* pFunc, const TypedQueryExecState& state,
					void (*runDirectFastChunk)(QueryImpl&, Iter&, void*, const TypedQueryExecState&),
					void (*runMappedChunk)(QueryImpl&, const QueryInfo&, Iter&, void*, const TypedQueryExecState&)) {
				auto& queryInfo = fetch();
				it.ctx(m_ctx);
				if (state.canUseDirectChunkEval) {
					runDirectFastChunk(*this, it, pFunc, state);
					finish_typed_chunk_state(
							*this, *it.world(), const_cast<Chunk*>(it.chunk()), it.row_begin(), it.row_end(), state);
				} else
					runMappedChunk(*this, queryInfo, it, pFunc, state);
			}

			inline void QueryImpl::each_direct_inter(
					QueryInfo& queryInfo, Constraints constraints, void* pFunc, const TypedQueryExecState& state,
					void (*runDirectChunk)(QueryImpl&, Iter&, void*, const TypedQueryExecState&), bool needsInheritedArgIds,
					void (*invokeInherited)(World&, Entity, const Entity*, void*)) {
				auto& world = *queryInfo.world();
				const auto plan = direct_entity_seed_plan(world, queryInfo);
				const bool hasWriteTerms = queryInfo.ctx().data.readWriteMask != 0;

				auto exec_direct_entity = [&](Entity entity) {
					uint8_t indices[ChunkHeader::MAX_COMPONENTS];
					Entity termIds[ChunkHeader::MAX_COMPONENTS];
					Iter it;
					it.set_constraints(constraints);
					init_direct_entity_iter(queryInfo, world, entity, it, indices, termIds);
					it.ctx(m_ctx);
					runDirectChunk(*this, it, pFunc, state);
				};

				auto exec_entity = [&](Entity entity) {
					if (needsInheritedArgIds && state.hasInheritedTerms) {
						invokeInherited(world, entity, state.argIds, pFunc);
						finish_typed_query_args_by_id(world, entity, state.argIds, state.writeFlags, state.argCount);
						return;
					}

					exec_direct_entity(entity);
				};

				if (!hasWriteTerms && !plan.preferOrSeed) {
					const auto* pSeedTerm = find_direct_all_seed_term(queryInfo, plan);
					if (pSeedTerm != nullptr && can_use_direct_seed_run_cache(world, queryInfo, *pSeedTerm)) {
						DirectEntitySeedInfo seedInfo{};
						seedInfo.seededAllTerm = pSeedTerm->id;
						seedInfo.seededAllMatchKind = pSeedTerm->matchKind;
						seedInfo.seededFromAll = true;
						if (!state.hasInheritedTerms) {
							const auto runs = cached_direct_seed_runs(queryInfo, *pSeedTerm, seedInfo, constraints);
							if (state.canUseDirectChunkEval) {
								Iter it;
								it.init_query_state(&world, constraints, false);
								const Archetype* pLastArchetype = nullptr;
								for (const auto& run: runs) {
									if (run.pArchetype != pLastArchetype) {
										it.set_archetype(run.pArchetype);
										pLastArchetype = run.pArchetype;
									}
									it.set_chunk(run.pChunk, run.from, run.to);
									it.set_group_id(0);
									it.ctx(m_ctx);
									runDirectChunk(*this, it, pFunc, state);
								}
							} else {
								Iter it;
								it.init_query_state(&world, constraints, false);
								const Archetype* pLastArchetype = nullptr;
								uint8_t indices[ChunkHeader::MAX_COMPONENTS];
								Entity termIds[ChunkHeader::MAX_COMPONENTS];
								for (const auto& run: runs) {
									const auto& ec = ::gaia::ecs::fetch(world, run.pChunk->entity_view()[run.from]);
									init_direct_entity_iter(queryInfo, world, ec, it, indices, termIds, pLastArchetype);
									it.set_chunk(run.pChunk, run.from, run.to);
									it.set_group_id(0);
									it.ctx(m_ctx);
									runDirectChunk(*this, it, pFunc, state);
								}
							}
						} else {
							const auto entities = cached_direct_seed_chunk_entities(queryInfo, *pSeedTerm, seedInfo, constraints);
							for (const auto entity: entities)
								exec_entity(entity);
						}
						return;
					}
				}

				auto walk_entities = [&](auto&& execEntity) {
					if (hasWriteTerms) {
						auto& scratch = direct_query_scratch();
						const auto seedInfo = build_direct_entity_seed(world, queryInfo, scratch.entities);
						for (const auto entity: scratch.entities) {
							if (!match_direct_entity_constraints(world, queryInfo, entity, constraints))
								continue;
							if (!match_direct_entity_terms(world, entity, queryInfo, seedInfo))
								continue;
							execEntity(entity);
						}
						return;
					}

					if (plan.preferOrSeed) {
						for_each_direct_or_union(world, queryInfo, constraints, [&](Entity entity) {
							execEntity(entity);
							return true;
						});
						return;
					}

					(void)for_each_direct_all_seed(world, queryInfo, plan, constraints, [&](Entity entity) {
						execEntity(entity);
						return true;
					});
				};

				walk_entities(exec_entity);
			}

			template <typename Func, std::enable_if_t<!detail::is_query_walk_core_callback_v<Func>, int>>
			inline void QueryImpl::each_walk(Func func, Entity relation, TravOrder order, Constraints constraints) {
				auto& queryInfo = fetch();
				match_all(queryInfo);
				const auto ordered = ordered_entities_walk(queryInfo, relation, order, constraints);

				using InputArgs = decltype(core::func_args(&Func::operator()));
				GAIA_ASSERT(typed_query_args_match_query(queryInfo, InputArgs{}));
				GAIA_ASSERT(can_use_direct_target_eval(queryInfo));
				if (!can_use_direct_target_eval(queryInfo))
					return;

				auto& world = *queryInfo.world();
				TypedQueryArgMeta metas[MAX_ITEMS_IN_QUERY]{};
				const auto argCount = init_typed_query_arg_metas(metas, world, InputArgs{});
				const auto state = build_typed_query_exec_state(*this, world, queryInfo, metas, argCount);
				if (state.canUseDirectChunkEval) {
					each_walk_dispatch_direct(*this, queryInfo, ordered, constraints, func, state, InputArgs{});
				} else {
					each_walk_dispatch_mapped(*this, queryInfo, ordered, constraints, func, state, InputArgs{});
				}
			}

			template <bool UseFilters, typename ContainerOut>
			inline void QueryImpl::arr_inter(QueryInfo& queryInfo, ContainerOut& outArray, Constraints constraints) {
				using ContainerItemType = typename ContainerOut::value_type;
				if constexpr (!UseFilters) {
					if (can_use_direct_entity_seed_eval(queryInfo)) {
						auto& world = *queryInfo.world();
						const auto plan = direct_entity_seed_plan(world, queryInfo);
						if (plan.preferOrSeed) {
							for_each_direct_or_union(world, queryInfo, constraints, [&](Entity entity) {
								typed_arr_push(world, entity, outArray);
							});
							return;
						}

						(void)for_each_direct_all_seed(world, queryInfo, plan, constraints, [&](Entity entity) {
							typed_arr_push(world, entity, outArray);
							return true;
						});

						return;
					}
				}

				auto& world = *queryInfo.world();
				const auto meta = typed_query_arg_meta<ContainerItemType>(world);
				const DirectChunkArgEvalDesc desc{meta.termId, meta.isEntity, meta.isPair};
				Iter it;
				it.init_query_state(queryInfo.world(), constraints, false);
				const bool canUseDirectChunkEval = !UseFilters && !queryInfo.has_entity_filter_terms() &&
																					 can_use_direct_chunk_term_eval_descs(world, queryInfo, &desc, 1) &&
																					 can_use_direct_chunk_iteration_fastpath(queryInfo);
				const auto cacheView = queryInfo.cache_archetype_view();
				const bool needsBarrierCache = needs_depth_order_hierarchy_barrier_cache(queryInfo, constraints);
				const bool hasSortedArrayPayload = queryInfo.has_sorted_payload() || needsBarrierCache;
				const auto sortView =
						hasSortedArrayPayload ? queryInfo.cache_sort_view() : decltype(queryInfo.cache_sort_view()){};
				if (needsBarrierCache)
					queryInfo.ensure_depth_order_hierarchy_barrier_cache();
				const auto cacheRange = selected_query_cache_range(queryInfo);
				if (!cacheRange.valid)
					return;
				const auto idxFrom = cacheRange.idxFrom;
				const auto idxTo = cacheRange.idxTo;

				if (!sortView.empty()) {
					for (const auto& view: sortView) {
						if (view.archetypeIdx < idxFrom || view.archetypeIdx >= idxTo)
							continue;

						const bool barrierPasses = !needsBarrierCache || queryInfo.barrier_passes(view.archetypeIdx);
						if GAIA_UNLIKELY (!can_process_archetype_inter(
																	queryInfo, *cacheView[view.archetypeIdx], constraints, barrierPasses))
							continue;

						const auto viewFrom = view.startRow;
						const auto viewTo = (uint16_t)(view.startRow + view.count);
						uint16_t minStartRow = 0;
						uint16_t minEndRow = 0;
						chunk_effective_range(view.pChunk, constraints, needsBarrierCache, barrierPasses, minStartRow, minEndRow);
						const auto startRow = core::get_max(minStartRow, viewFrom);
						const auto endRow = core::get_min(minEndRow, viewTo);
						if (startRow == endRow)
							continue;

						run_typed_arr_rows<UseFilters>(
								*this, queryInfo, it, outArray, m_changedWorldVersion, view.archetypeIdx, cacheView[view.archetypeIdx],
								view.pChunk, startRow, endRow, needsBarrierCache, canUseDirectChunkEval);
					}
					return;
				}

				for (uint32_t i = idxFrom; i < idxTo; ++i) {
					auto* pArchetype = cacheView[i];
					const bool barrierPasses = !needsBarrierCache || queryInfo.barrier_passes(i);
					if GAIA_UNLIKELY (!can_process_archetype_inter(queryInfo, *pArchetype, constraints, barrierPasses))
						continue;

					const auto& chunks = pArchetype->chunks();
					for (auto* pChunk: chunks) {
						uint16_t from = 0;
						uint16_t to = 0;
						chunk_effective_range(pChunk, constraints, needsBarrierCache, barrierPasses, from, to);
						run_typed_arr_rows<UseFilters>(
								*this, queryInfo, it, outArray, m_changedWorldVersion, i, pArchetype, pChunk, from, to,
								needsBarrierCache, canUseDirectChunkEval);
					}
				}
			}

			template <typename Container>
			inline void QueryImpl::arr(Container& outArray, Constraints constraints) {
				const auto entCnt = count(constraints);
				if (entCnt == 0)
					return;

				outArray.reserve(entCnt);
				auto& queryInfo = fetch();
				match_all(queryInfo);

				const bool hasFilters = queryInfo.has_filters();
				if (hasFilters) {
					arr_inter<true>(queryInfo, outArray, constraints);
				} else {
					arr_inter<false>(queryInfo, outArray, constraints);
				}
			}
		} // namespace detail
	} // namespace ecs
} // namespace gaia
