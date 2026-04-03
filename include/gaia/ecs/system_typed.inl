#pragma once

#if GAIA_SYSTEMS_ENABLED
namespace gaia {
	namespace ecs {
		template <typename T>
		inline SystemBuilder& SystemBuilder::all(const QueryTermOptions& options) {
			validate();
			return all(
					detail::typed_query_term_entity<T>(m_world), detail::typed_query_term_options<T>(QueryOpKind::All, options));
		}

		template <typename T>
		inline SystemBuilder& SystemBuilder::any(const QueryTermOptions& options) {
			validate();
			return any(
					detail::typed_query_term_entity<T>(m_world), detail::typed_query_term_options<T>(QueryOpKind::Any, options));
		}

		template <typename T>
		inline SystemBuilder& SystemBuilder::or_(const QueryTermOptions& options) {
			validate();
			return or_(
					detail::typed_query_term_entity<T>(m_world), detail::typed_query_term_options<T>(QueryOpKind::Or, options));
		}

		template <typename T>
		inline SystemBuilder& SystemBuilder::no(const QueryTermOptions& options) {
			validate();
			return no(
					detail::typed_query_term_entity<T>(m_world), detail::typed_query_term_options<T>(QueryOpKind::Not, options));
		}

		template <typename T>
		inline SystemBuilder& SystemBuilder::all() {
			return all<T>(QueryTermOptions{});
		}

		template <typename T>
		inline SystemBuilder& SystemBuilder::any() {
			return any<T>(QueryTermOptions{});
		}

		template <typename T>
		inline SystemBuilder& SystemBuilder::or_() {
			return or_<T>(QueryTermOptions{});
		}

		template <typename T>
		inline SystemBuilder& SystemBuilder::no() {
			return no<T>(QueryTermOptions{});
		}

		template <typename T>
		inline SystemBuilder& SystemBuilder::changed() {
			return changed(detail::typed_query_raw_entity<T>(m_world));
		}

		template <typename Rel>
		inline SystemBuilder& SystemBuilder::depth_order() {
			return depth_order(detail::typed_query_raw_entity<Rel>(m_world));
		}

		template <typename T>
		inline SystemBuilder& SystemBuilder::group_by(TGroupByFunc func) {
			return group_by(detail::typed_query_raw_entity<T>(m_world), func);
		}

		template <typename Rel, typename Tgt>
		inline SystemBuilder& SystemBuilder::group_by(TGroupByFunc func) {
			return group_by(detail::typed_query_pair_entity<Rel, Tgt>(m_world), func);
		}

		template <typename Rel>
		inline SystemBuilder& SystemBuilder::group_dep() {
			return group_dep(detail::typed_query_raw_entity<Rel>(m_world));
		}

		template <typename T>
		inline SystemBuilder& SystemBuilder::group_id() {
			return group_id(detail::typed_query_raw_entity<T>(m_world));
		}

		template <typename Func, std::enable_if_t<!detail::is_query_iter_callback_v<Func>, int>>
		inline SystemBuilder& SystemBuilder::on_each(Func func) {
			validate();

			auto& ctx = data();
			using InputArgs = decltype(core::func_args(&Func::operator()));
			auto& queryInfo = ctx.query.fetch();
			TypedQueryArgMeta metas[MAX_ITEMS_IN_QUERY]{};
			const auto argCount = init_typed_query_arg_metas(metas, m_world, InputArgs{});
			const auto execState = detail::build_typed_query_exec_state(ctx.query, m_world, queryInfo, metas, argCount);
			const auto thunks = detail::build_typed_query_thunk_set<Func>(InputArgs{});
			const bool hasInheritedTerms = execState.hasInheritedTerms;
			if (hasInheritedTerms) {
				ctx.on_each_func = [func, execState, thunks](Query& query, QueryExecType execType) mutable {
					auto& queryInfo = query.fetch();
					query.match_all(queryInfo);
					switch (execType) {
						case QueryExecType::Parallel:
							query.each_typed_inter_erased<QueryExecType::Parallel>(queryInfo, &func, execState, thunks);
							break;
						case QueryExecType::ParallelPerf:
							query.each_typed_inter_erased<QueryExecType::ParallelPerf>(queryInfo, &func, execState, thunks);
							break;
						case QueryExecType::ParallelEff:
							query.each_typed_inter_erased<QueryExecType::ParallelEff>(queryInfo, &func, execState, thunks);
							break;
						default:
							query.each_typed_inter_erased<QueryExecType::Default>(queryInfo, &func, execState, thunks);
							break;
					}
				};
			} else {
				ctx.on_each_func = [func, execState, thunks](Query& query, QueryExecType execType) mutable {
					query.each(
							[&query, &func, &execState, &thunks](Iter& it) mutable {
								query.each_iter_erased(it, &func, execState, thunks);
							},
							execType);
				};
			}

			return *this;
		}
	} // namespace ecs
} // namespace gaia
#endif
