#pragma once

#include "gaia/ecs/query_builder_typed.inl"
#include "gaia/ecs/query_typed.inl"

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
			const auto runDirectFastChunk = detail::typed_run_direct_fast_chunk_ptr<Func>(InputArgs{});
			const auto runDirectChunk = detail::typed_run_direct_chunk_ptr<Func>(InputArgs{});
			const auto runMappedChunk = detail::typed_run_mapped_chunk_ptr<Func>(InputArgs{});
			const auto invokeInherited = typed_invoke_inherited_ptr<Func>(InputArgs{});
			const bool hasInheritedTerms = execState.hasInheritedTerms;
			if (hasInheritedTerms) {
				ctx.on_each_func = [func, execState, runDirectFastChunk, runDirectChunk, runMappedChunk, invokeInherited](
										 Query& query, QueryExecType execType) mutable {
					auto& queryInfo = query.fetch();
					query.match_all(queryInfo);
					switch (execType) {
						case QueryExecType::Parallel:
							query.each_inter<QueryExecType::Parallel>(
									queryInfo, &func, execState, runDirectFastChunk, runDirectChunk, runMappedChunk,
									execState.needsInheritedArgIds, invokeInherited);
							break;
						case QueryExecType::ParallelPerf:
							query.each_inter<QueryExecType::ParallelPerf>(
									queryInfo, &func, execState, runDirectFastChunk, runDirectChunk, runMappedChunk,
									execState.needsInheritedArgIds, invokeInherited);
							break;
						case QueryExecType::ParallelEff:
							query.each_inter<QueryExecType::ParallelEff>(
									queryInfo, &func, execState, runDirectFastChunk, runDirectChunk, runMappedChunk,
									execState.needsInheritedArgIds, invokeInherited);
							break;
						default:
							query.each_inter<QueryExecType::Default>(
									queryInfo, &func, execState, runDirectFastChunk, runDirectChunk, runMappedChunk,
									execState.needsInheritedArgIds, invokeInherited);
							break;
					}
				};
			} else {
				ctx.on_each_func = [func, execState, runDirectFastChunk, runMappedChunk](Query& query,
																												 QueryExecType execType) mutable {
					query.each_iter_erased(execType, &func, execState, runDirectFastChunk, runMappedChunk);
				};
			}

			return *this;
		}
	} // namespace ecs
} // namespace gaia
#endif
