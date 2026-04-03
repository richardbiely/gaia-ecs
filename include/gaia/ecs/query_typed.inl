#pragma once

#include "gaia/ecs/query_adapter_typed.inl"

namespace gaia {
	namespace ecs {
		namespace detail {
			template <typename... T>
			inline TypedQueryExecState<T...>
			build_typed_query_exec_state(QueryImpl& query, World& world, const QueryInfo& queryInfo) {
				TypedQueryExecState<T...> state{};
				init_typed_query_arg_descs(state.argIds, state.writeFlags, world, core::func_type_list<T...>{});
				state.canUseDirectChunkEval = query.template can_use_direct_chunk_term_eval<T...>(world, queryInfo);
				if constexpr (typed_query_args_need_inherited_ids<T...>())
					state.hasInheritedTerms = queryInfo.has_potential_inherited_id_terms();
				return state;
			}

			template <typename... T>
			inline constexpr bool QueryImpl::has_write_query_args() {
				return (typed_query_arg_is_write<T>() || ...);
			}

			template <typename... T>
			inline bool QueryImpl::can_use_direct_chunk_term_eval(World& world, const QueryInfo& queryInfo) {
				if constexpr (sizeof...(T) == 0)
					return !queryInfo.has_entity_filter_terms();
				else {
					const TypedDirectChunkArgEvalDesc typedDescs[] = {typed_direct_chunk_arg_eval_desc<T>(world)...};
					DirectChunkArgEvalDesc descs[sizeof...(T)];
					GAIA_FOR(sizeof...(T)) {
						descs[i] = {.id = typedDescs[i].id, .isEntity = typedDescs[i].isEntity, .isPair = typedDescs[i].isPair};
					}
					return can_use_direct_chunk_term_eval_descs(world, queryInfo, descs, sizeof...(T));
				}
			}

			template <typename... T>
			inline bool QueryImpl::can_use_direct_chunk_term_eval_args(
					World& world, const QueryInfo& queryInfo, [[maybe_unused]] core::func_type_list<T...>) {
				return can_use_direct_chunk_term_eval<T...>(world, queryInfo);
			}

			inline void QueryImpl::finish_typed_chunk_writes_runtime(
					World& world, Chunk* pChunk, uint16_t from, uint16_t to, const Entity* pArgIds, const bool* pWriteFlags,
					uint32_t argCnt) {
				if (from >= to)
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

				GAIA_FOR(argCnt) {
					if (!pWriteFlags[i])
						continue;
					const auto term = pArgIds[i];
					if (term != EntityBad)
						finish_term(term);
				}
			}

			template <typename... T>
			inline void QueryImpl::finish_typed_chunk_writes(World& world, Chunk* pChunk, uint16_t from, uint16_t to) {
				Entity argIds[sizeof...(T) > 0 ? sizeof...(T) : 1]{};
				bool writeFlags[sizeof...(T) > 0 ? sizeof...(T) : 1]{};
				init_typed_query_arg_descs(argIds, writeFlags, world, core::func_type_list<T...>{});
				finish_typed_chunk_writes_runtime(world, pChunk, from, to, argIds, writeFlags, sizeof...(T));
			}

			template <typename TIter>
			inline void QueryImpl::finish_typed_iter_writes_runtime(
					TIter& it, const Entity* pArgIds, const bool* pWriteFlags, uint32_t argCnt) {
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

				GAIA_FOR(argCnt) {
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

			template <typename TIter, typename Func, typename ViewsTuple, typename OnRowDone, typename... T>
			inline void QueryImpl::run_query_tuple_rows(
					const QueryInfo* pQueryInfo, TIter& it, Func& func, ViewsTuple& dataPointerTuple, bool directLocalIndex,
					OnRowDone&& onRowDone, core::func_type_list<T...>) {
				const auto cnt = it.size();
				const bool hasEntityFilters = pQueryInfo != nullptr && pQueryInfo->has_entity_filter_terms();

				if constexpr (sizeof...(T) > 0) {
					if (!hasEntityFilters) {
						GAIA_FOR(cnt) {
							if (directLocalIndex) {
								std::apply(
										[&](auto&... views) {
											func(views[i]...);
										},
										dataPointerTuple);
							} else {
								std::apply(
										[&](auto&... views) {
											func(views[it.template acc_index<T>(i)]...);
										},
										dataPointerTuple);
							}
							onRowDone((uint32_t)i);
						}
					} else {
						const auto entities = it.template view<Entity>();
						GAIA_FOR(cnt) {
							if (!match_entity_filters(*pQueryInfo->world(), entities[i], *pQueryInfo))
								continue;
							if (directLocalIndex) {
								std::apply(
										[&](auto&... views) {
											func(views[i]...);
										},
										dataPointerTuple);
							} else {
								std::apply(
										[&](auto&... views) {
											func(views[it.template acc_index<T>(i)]...);
										},
										dataPointerTuple);
							}
							onRowDone((uint32_t)i);
						}
					}
				} else {
					if (!hasEntityFilters) {
						GAIA_FOR(cnt) {
							func();
							onRowDone((uint32_t)i);
						}
					} else {
						const auto entities = it.template view<Entity>();
						GAIA_FOR(cnt) {
							if (!match_entity_filters(*pQueryInfo->world(), entities[i], *pQueryInfo))
								continue;
							func();
							onRowDone((uint32_t)i);
						}
					}
				}
			}

			template <typename Func, typename... T>
			inline void QueryImpl::run_query_on_chunks_direct(QueryInfo& queryInfo, Func func, core::func_type_list<T...>) {
				if constexpr (has_write_query_args<T...>())
					::gaia::ecs::update_version(*m_worldVersion);

				const bool hasFilters = queryInfo.has_filters();
				auto cacheView = queryInfo.cache_archetype_view();
				if (cacheView.empty())
					return;

				uint32_t idxFrom = 0;
				uint32_t idxTo = (uint32_t)cacheView.size();
				if (queryInfo.ctx().data.groupBy != EntityBad && m_groupIdSet != 0) {
					const auto* pGroupData = queryInfo.selected_group_data(m_groupIdSet);
					if (pGroupData == nullptr)
						return;
					idxFrom = pGroupData->idxFirst;
					idxTo = pGroupData->idxLast + 1;
				}

				lock(*m_storage.world());
				Iter it;
				it.set_world(queryInfo.world());
				const Archetype* pLastArchetype = nullptr;

				for (uint32_t i = idxFrom; i < idxTo; ++i) {
					const auto* pArchetype = cacheView[i];
					if GAIA_UNLIKELY (!can_process_archetype_inter<Iter>(queryInfo, *pArchetype))
						continue;

					const auto& chunks = pArchetype->chunks();
					for (auto* pChunk: chunks) {
						const auto from = Iter::start_index(pChunk);
						const auto to = Iter::end_index(pChunk);
						if GAIA_UNLIKELY (from == to)
							continue;

						if (hasFilters) {
							if GAIA_UNLIKELY (!match_filters(*pChunk, queryInfo, m_changedWorldVersion))
								continue;
						}

						GAIA_PROF_SCOPE(query_func);
						if (pArchetype != pLastArchetype) {
							it.set_archetype(pArchetype);
							pLastArchetype = pArchetype;
						}
						it.set_chunk(pChunk, from, to);
						it.set_group_id(0);
						run_query_on_chunk_direct_views(it, func, core::func_type_list<T...>{});
						finish_typed_chunk_writes<T...>(*queryInfo.world(), pChunk, from, to);
					}
				}

				unlock(*m_storage.world());
				commit_cmd_buffer_st(*m_storage.world());
				commit_cmd_buffer_mt(*m_storage.world());
				m_changedWorldVersion = *m_worldVersion;
			}

			template <typename TIter, typename Func, typename... T>
			inline void
			QueryImpl::run_query_on_chunk(TIter& it, Func func, [[maybe_unused]] core::func_type_list<T...> types) {
				auto& queryInfo = fetch();
				auto& world = *const_cast<World*>(queryInfo.world());
				const auto state = build_typed_query_exec_state<T...>(*this, world, queryInfo);
				if (state.canUseDirectChunkEval)
					run_query_on_chunk_direct(it, func, types);
				else
					run_query_on_chunk(queryInfo, it, func, types);
			}

			template <typename TIter, typename Func, typename... T>
			inline void QueryImpl::run_query_on_chunk(
					const QueryInfo& queryInfo, TIter& it, Func func, [[maybe_unused]] core::func_type_list<T...> types) {
				const auto state = build_typed_query_exec_state<T...>(*this, *it.world(), queryInfo);
				if constexpr (sizeof...(T) > 0) {
					auto dataPointerTuple = std::make_tuple(it.template view_auto_any<T>()...);
					run_query_tuple_rows(
							&queryInfo, it, func, dataPointerTuple, false, [](uint32_t) {}, core::func_type_list<T...>{});
				} else {
					auto dataPointerTuple = std::tuple<>{};
					run_query_tuple_rows(
							&queryInfo, it, func, dataPointerTuple, false, [](uint32_t) {}, core::func_type_list<T...>{});
				}

				finish_typed_iter_writes_runtime(it, state.argIds, state.writeFlags, sizeof...(T));
				it.clear_touched_writes();
			}

			template <typename TIter, typename Func, typename... T>
			inline void
			QueryImpl::run_query_on_chunk_direct_views(TIter& it, Func& func, [[maybe_unused]] core::func_type_list<T...>) {
				if constexpr (sizeof...(T) > 0) {
					auto dataPointerTuple = std::make_tuple(it.template sview_auto<T>()...);
					run_query_tuple_rows(
							nullptr, it, func, dataPointerTuple, true, [](uint32_t) {}, core::func_type_list<T...>{});
				} else {
					auto dataPointerTuple = std::tuple<>{};
					run_query_tuple_rows(
							nullptr, it, func, dataPointerTuple, true, [](uint32_t) {}, core::func_type_list<T...>{});
				}
			}

			template <typename TIter, typename Func, typename... T>
			inline void
			QueryImpl::run_query_on_chunk_direct(TIter& it, Func func, [[maybe_unused]] core::func_type_list<T...> types) {
				const auto state = build_typed_query_exec_state<T...>(*this, *it.world(), fetch());
				run_query_on_chunk_direct_views(it, func, types);
				finish_typed_iter_writes_runtime(it, state.argIds, state.writeFlags, sizeof...(T));
				it.clear_touched_writes();
			}

			template <typename TIter, typename Func, typename... T>
			inline void
			QueryImpl::run_query_on_chunk_unmapped(TIter& it, Func func, [[maybe_unused]] core::func_type_list<T...> types) {
				auto& queryInfo = fetch();
				auto& world = *queryInfo.world();
				const auto state = build_typed_query_exec_state<T...>(*this, world, queryInfo);
				if (state.canUseDirectChunkEval) {
					run_query_on_chunk_direct_views(it, func, types);
					finish_typed_chunk_writes_runtime(
							world, const_cast<Chunk*>(it.chunk()), it.row_begin(), it.row_end(), state.argIds, state.writeFlags,
							sizeof...(T));
				} else
					run_query_on_chunk_unmapped(queryInfo, it, func, types);
			}

			template <typename TIter, typename Func, typename... T>
			inline void QueryImpl::run_query_on_chunk_unmapped(
					const QueryInfo& queryInfo, TIter& it, Func func, [[maybe_unused]] core::func_type_list<T...> types) {
				auto& world = *const_cast<World*>(queryInfo.world());
				auto* pChunk = const_cast<Chunk*>(it.chunk());
				const bool hasEntityFilters = queryInfo.has_entity_filter_terms();
				const auto state = build_typed_query_exec_state<T...>(*this, world, queryInfo);

				if constexpr (sizeof...(T) > 0) {
					auto dataPointerTuple = std::make_tuple(it.template view_auto_any<T>()...);
					if (!hasEntityFilters) {
						run_query_tuple_rows(
								&queryInfo, it, func, dataPointerTuple, false, [](uint32_t) {}, core::func_type_list<T...>{});
						finish_typed_chunk_writes_runtime(
								world, pChunk, it.row_begin(), it.row_end(), state.argIds, state.writeFlags, sizeof...(T));
					} else {
						run_query_tuple_rows(
								&queryInfo, it, func, dataPointerTuple, false,
								[&](uint32_t row) {
									finish_typed_chunk_writes_runtime(
											world, pChunk, (uint16_t)(it.row_begin() + row), (uint16_t)(it.row_begin() + row + 1),
											state.argIds, state.writeFlags, sizeof...(T));
								},
								core::func_type_list<T...>{});
					}
				} else {
					auto dataPointerTuple = std::tuple<>{};
					run_query_tuple_rows(
							&queryInfo, it, func, dataPointerTuple, false, [](uint32_t) {}, core::func_type_list<T...>{});
				}

				it.clear_touched_writes();
			}

			template <QueryExecType ExecType, typename Func, typename... T>
			inline void QueryImpl::each_inter(QueryInfo& queryInfo, Func func, core::func_type_list<T...>) {
				if (!queryInfo.has_filters() && can_use_direct_entity_seed_eval(queryInfo)) {
					GAIA_PROF_SCOPE(query_func);
					each_direct_inter<Iter>(queryInfo, func, core::func_type_list<T...>{});
					return;
				}

				auto& world = *const_cast<World*>(queryInfo.world());
				if (can_use_direct_chunk_term_eval<T...>(world, queryInfo)) {
					if constexpr (ExecType == QueryExecType::Default) {
						if (can_use_direct_chunk_iteration_fastpath(queryInfo)) {
							run_query_on_chunks_direct(queryInfo, func, core::func_type_list<T...>{});
							return;
						}
					}
					run_query_on_chunks<ExecType, Iter>(queryInfo, [&](Iter& it) {
						GAIA_PROF_SCOPE(query_func);
						run_query_on_chunk_direct(it, func, core::func_type_list<T...>{});
					});
				} else {
					run_query_on_chunks<ExecType, Iter>(queryInfo, [&](Iter& it) {
						GAIA_PROF_SCOPE(query_func);
						run_query_on_chunk(queryInfo, it, func, core::func_type_list<T...>{});
					});
				}
			}

			template <QueryExecType ExecType, typename Func>
			inline void QueryImpl::each_typed_inter(QueryInfo& queryInfo, Func func) {
				using InputArgs = decltype(core::func_args(&Func::operator()));

#if GAIA_ASSERT_ENABLED
				GAIA_ASSERT(typed_query_args_match_query(queryInfo, InputArgs{}));
#endif

				each_inter<ExecType>(queryInfo, func, InputArgs{});
			}

			template <typename Func, std::enable_if_t<!is_query_iter_callback_v<Func>, int>>
			inline void QueryImpl::each(Func func) {
				each(func, QueryExecType::Default);
			}

			template <typename Func, std::enable_if_t<!is_query_iter_callback_v<Func>, int>>
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

			template <typename TIter, typename Func>
			inline void QueryImpl::each_iter(TIter& it, Func func) {
				using InputArgs = decltype(core::func_args(&Func::operator()));

#if GAIA_ASSERT_ENABLED
				auto& queryInfo = fetch();
				GAIA_ASSERT(typed_query_args_match_query(queryInfo, InputArgs{}));
#endif

				run_query_on_chunk_unmapped(it, func, InputArgs{});
			}

			template <typename TIter, typename Func, typename... T>
			inline void
			QueryImpl::each_direct_inter(QueryInfo& queryInfo, Func func, [[maybe_unused]] core::func_type_list<T...>) {
				constexpr bool needsInheritedArgIds =
						(!std::is_same_v<std::remove_cv_t<std::remove_reference_t<T>>, Entity> || ... || false);

				auto& world = *queryInfo.world();
				const auto plan = direct_entity_seed_plan(world, queryInfo);
				const bool hasWriteTerms = queryInfo.ctx().data.readWriteMask != 0;
				const auto state = build_typed_query_exec_state<T...>(*this, world, queryInfo);

				auto exec_direct_entity = [&](Entity entity) {
					uint8_t indices[ChunkHeader::MAX_COMPONENTS];
					Entity termIds[ChunkHeader::MAX_COMPONENTS];
					TIter it;
					init_direct_entity_iter(queryInfo, world, entity, it, indices, termIds);
					run_query_on_chunk_direct(it, func, core::func_type_list<T...>{});
				};
				auto exec_entity = [&](Entity entity) {
					if constexpr (needsInheritedArgIds) {
						if (state.hasInheritedTerms) {
							invoke_typed_query_args_by_id<T...>(world, entity, state.argIds, func, std::index_sequence_for<T...>{});
							finish_typed_query_args_by_id(world, entity, state.argIds, state.writeFlags, sizeof...(T));
							return;
						}
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
							const auto runs = cached_direct_seed_runs<TIter>(queryInfo, *pSeedTerm, seedInfo);
							const bool canUseBasicInit = state.canUseDirectChunkEval;
							if (canUseBasicInit) {
								TIter it;
								it.set_world(&world);
								const Archetype* pLastArchetype = nullptr;
								for (const auto& run: runs) {
									if (run.pArchetype != pLastArchetype) {
										it.set_archetype(run.pArchetype);
										pLastArchetype = run.pArchetype;
									}

									it.set_chunk(run.pChunk, run.from, run.to);
									it.set_group_id(0);
									run_query_on_chunk_direct(it, func, core::func_type_list<T...>{});
								}
							} else {
								TIter it;
								it.set_world(&world);
								const Archetype* pLastArchetype = nullptr;
								uint8_t indices[ChunkHeader::MAX_COMPONENTS];
								Entity termIds[ChunkHeader::MAX_COMPONENTS];
								for (const auto& run: runs) {
									const auto& ec = ::gaia::ecs::fetch(world, run.pChunk->entity_view()[run.from]);
									init_direct_entity_iter(queryInfo, world, ec, it, indices, termIds, pLastArchetype);
									it.set_chunk(run.pChunk, run.from, run.to);
									it.set_group_id(0);
									run_query_on_chunk_direct(it, func, core::func_type_list<T...>{});
								}
							}
						} else {
							const auto entities = cached_direct_seed_chunk_entities<TIter>(queryInfo, *pSeedTerm, seedInfo);
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
							if (!match_direct_entity_constraints<TIter>(world, queryInfo, entity))
								continue;
							if (!match_direct_entity_terms(world, entity, queryInfo, seedInfo))
								continue;
							execEntity(entity);
						}
						return;
					}

					if (plan.preferOrSeed) {
						for_each_direct_or_union<TIter>(world, queryInfo, [&](Entity entity) {
							execEntity(entity);
							return true;
						});
						return;
					}

					(void)for_each_direct_all_seed<TIter>(world, queryInfo, plan, [&](Entity entity) {
						execEntity(entity);
						return true;
					});
				};

				walk_entities(exec_entity);
			}

			template <typename Func, std::enable_if_t<!is_query_walk_core_callback_v<Func>, int>>
			inline void QueryImpl::each_walk(Func func, Entity relation, Constraints constraints) {
				auto& queryInfo = fetch();
				match_all(queryInfo);
				const auto ordered = ordered_entities_walk(queryInfo, relation, constraints);

				using InputArgs = decltype(core::func_args(&Func::operator()));
				GAIA_ASSERT(typed_query_args_match_query(queryInfo, InputArgs{}));
				GAIA_ASSERT(can_use_direct_target_eval(queryInfo));
				if (!can_use_direct_target_eval(queryInfo))
					return;

				auto& world = *queryInfo.world();
				const bool canUseDirectChunkEval = can_use_direct_chunk_term_eval_args(world, queryInfo, InputArgs{});

				switch (constraints) {
					case Constraints::EnabledOnly:
						each_direct_entities_iter<Iter>(queryInfo, ordered, [&](Iter& it) {
							if (canUseDirectChunkEval)
								run_query_on_chunk_direct(it, func, InputArgs{});
							else
								run_query_on_chunk(queryInfo, it, func, InputArgs{});
						});
						break;
					case Constraints::DisabledOnly:
						each_direct_entities_iter<IterDisabled>(queryInfo, ordered, [&](IterDisabled& it) {
							if (canUseDirectChunkEval)
								run_query_on_chunk_direct(it, func, InputArgs{});
							else
								run_query_on_chunk(queryInfo, it, func, InputArgs{});
						});
						break;
					case Constraints::AcceptAll:
						each_direct_entities_iter<IterAll>(queryInfo, ordered, [&](IterAll& it) {
							if (canUseDirectChunkEval)
								run_query_on_chunk_direct(it, func, InputArgs{});
							else
								run_query_on_chunk(queryInfo, it, func, InputArgs{});
						});
						break;
				}
			}

			template <bool UseFilters, typename TIter, typename ContainerOut>
			inline void QueryImpl::arr_inter(QueryInfo& queryInfo, ContainerOut& outArray) {
				using ContainerItemType = typename ContainerOut::value_type;
				if constexpr (!UseFilters) {
					if (can_use_direct_entity_seed_eval(queryInfo)) {
						auto& world = *queryInfo.world();
						const auto plan = direct_entity_seed_plan(world, queryInfo);
						if (plan.preferOrSeed) {
							for_each_direct_or_union<TIter>(world, queryInfo, [&](Entity entity) {
								if constexpr (std::is_same_v<ContainerItemType, Entity>)
									outArray.push_back(entity);
								else {
									auto tmp = world_direct_entity_arg<ContainerItemType>(world, entity);
									outArray.push_back(tmp);
								}
							});
							return;
						}

						(void)for_each_direct_all_seed<TIter>(world, queryInfo, plan, [&](Entity entity) {
							if constexpr (std::is_same_v<ContainerItemType, Entity>)
								outArray.push_back(entity);
							else {
								auto tmp = world_direct_entity_arg<ContainerItemType>(world, entity);
								outArray.push_back(tmp);
							}
							return true;
						});

						return;
					}
				}

				auto& world = *queryInfo.world();
				TIter it;
				it.set_world(queryInfo.world());
				const bool hasEntityFilters = queryInfo.has_entity_filter_terms();
				const bool canUseDirectChunkEval = !UseFilters && !hasEntityFilters &&
																					 can_use_direct_chunk_term_eval<ContainerItemType>(world, queryInfo) &&
																					 can_use_direct_chunk_iteration_fastpath(queryInfo);
				const auto cacheView = queryInfo.cache_archetype_view();
				const auto sortView = queryInfo.cache_sort_view();
				const bool needsBarrierCache = has_depth_order_hierarchy_enabled_barrier(queryInfo);
				if (needsBarrierCache)
					queryInfo.ensure_depth_order_hierarchy_barrier_cache();
				uint32_t idxFrom = 0;
				uint32_t idxTo = (uint32_t)cacheView.size();
				if (queryInfo.ctx().data.groupBy != EntityBad && m_groupIdSet != 0) {
					const auto* pGroupData = queryInfo.selected_group_data(m_groupIdSet);
					if (pGroupData == nullptr)
						return;
					idxFrom = pGroupData->idxFirst;
					idxTo = pGroupData->idxLast + 1;
				}

				const auto append_rows = [&](const uint32_t archetypeIdx, auto* pArchetype, auto* pChunk, uint16_t from,
																		 uint16_t to) {
					const bool barrierPasses = !needsBarrierCache || queryInfo.barrier_passes(archetypeIdx);
					if GAIA_UNLIKELY (!can_process_archetype_inter<TIter>(queryInfo, *pArchetype, barrierPasses))
						return;

					GAIA_PROF_SCOPE(query::arr);

					it.set_archetype(pArchetype);
					it.set_chunk(pChunk, from, to);
					it.set_group_id(0);

					const auto cnt = it.size();
					if (cnt == 0)
						return;

					if constexpr (UseFilters) {
						if (!match_filters(*pChunk, queryInfo, m_changedWorldVersion))
							return;
					}

					if (canUseDirectChunkEval) {
						const auto dataView = it.template sview_auto<ContainerItemType>();
						GAIA_FOR(cnt) {
							auto tmp = dataView[i];
							outArray.push_back(tmp);
						}
					} else {
						const auto dataView = it.template view<ContainerItemType>();
						if (!hasEntityFilters) {
							GAIA_FOR(cnt) {
								const auto idx = it.template acc_index<ContainerItemType>(i);
								auto tmp = dataView[idx];
								outArray.push_back(tmp);
							}
						} else {
							const auto entities = it.template view<Entity>();
							GAIA_FOR(cnt) {
								if (!match_entity_filters(*queryInfo.world(), entities[i], queryInfo))
									continue;
								const auto idx = it.template acc_index<ContainerItemType>(i);
								auto tmp = dataView[idx];
								outArray.push_back(tmp);
							}
						}
					}
				};

				if (!sortView.empty()) {
					for (const auto& view: sortView) {
						if (view.archetypeIdx < idxFrom || view.archetypeIdx >= idxTo)
							continue;

						const auto minStartRow = TIter::start_index(view.pChunk);
						const auto minEndRow = TIter::end_index(view.pChunk);
						const auto viewFrom = view.startRow;
						const auto viewTo = (uint16_t)(view.startRow + view.count);
						const auto startRow = core::get_max(minStartRow, viewFrom);
						const auto endRow = core::get_min(minEndRow, viewTo);
						if (startRow == endRow)
							continue;

						append_rows(
								view.archetypeIdx, const_cast<Archetype*>(cacheView[view.archetypeIdx]), view.pChunk, startRow, endRow);
					}
					return;
				}

				for (uint32_t i = idxFrom; i < idxTo; ++i) {
					auto* pArchetype = cacheView[i];
					const auto& chunks = pArchetype->chunks();
					for (auto* pChunk: chunks)
						append_rows(i, pArchetype, pChunk, 0, 0);
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
					switch (constraints) {
						case Constraints::EnabledOnly:
							arr_inter<true, Iter>(queryInfo, outArray);
							break;
						case Constraints::DisabledOnly:
							arr_inter<true, IterDisabled>(queryInfo, outArray);
							break;
						case Constraints::AcceptAll:
							arr_inter<true, IterAll>(queryInfo, outArray);
							break;
					}
				} else {
					switch (constraints) {
						case Constraints::EnabledOnly:
							arr_inter<false, Iter>(queryInfo, outArray);
							break;
						case Constraints::DisabledOnly:
							arr_inter<false, IterDisabled>(queryInfo, outArray);
							break;
						case Constraints::AcceptAll:
							arr_inter<false, IterAll>(queryInfo, outArray);
							break;
					}
				}
			}
		} // namespace detail
	} // namespace ecs
} // namespace gaia
