#pragma once

#include "gaia/ecs/query_builder_typed.inl"
#include "gaia/ecs/query_typed.inl"

#if GAIA_OBSERVERS_ENABLED
namespace gaia {
	namespace ecs {
		template <typename T>
		inline ObserverBuilder& ObserverBuilder::all(const QueryTermOptions& options) {
			validate();
			auto& data = runtime_data();
			const auto resolvedOptions = detail::typed_query_term_options<T>(QueryOpKind::All, options);
			const auto term = detail::typed_query_term_entity<T>(m_world);
			data.query.all(term, resolvedOptions);
			reg_term(data, QueryOpKind::All, term, resolvedOptions);
			return *this;
		}

		template <typename T>
		inline ObserverBuilder& ObserverBuilder::any(const QueryTermOptions& options) {
			validate();
			auto& data = runtime_data();
			const auto resolvedOptions = detail::typed_query_term_options<T>(QueryOpKind::Any, options);
			const auto term = detail::typed_query_term_entity<T>(m_world);
			data.query.any(term, resolvedOptions);
			reg_term(data, QueryOpKind::Any, term, resolvedOptions);
			return *this;
		}

		template <typename T>
		inline ObserverBuilder& ObserverBuilder::or_(const QueryTermOptions& options) {
			validate();
			auto& data = runtime_data();
			const auto resolvedOptions = detail::typed_query_term_options<T>(QueryOpKind::Or, options);
			const auto term = detail::typed_query_term_entity<T>(m_world);
			data.query.or_(term, resolvedOptions);
			reg_term(data, QueryOpKind::Or, term, resolvedOptions);
			return *this;
		}

		template <typename T>
		inline ObserverBuilder& ObserverBuilder::no(const QueryTermOptions& options) {
			validate();
			auto& data = runtime_data();
			const auto resolvedOptions = detail::typed_query_term_options<T>(QueryOpKind::Not, options);
			const auto term = detail::typed_query_term_entity<T>(m_world);
			data.query.no(term, resolvedOptions);
			reg_term(data, QueryOpKind::Not, term, resolvedOptions);
			return *this;
		}

		template <typename T>
		inline ObserverBuilder& ObserverBuilder::all() {
			return all<T>(QueryTermOptions{});
		}

		template <typename T>
		inline ObserverBuilder& ObserverBuilder::any() {
			return any<T>(QueryTermOptions{});
		}

		template <typename T>
		inline ObserverBuilder& ObserverBuilder::or_() {
			return or_<T>(QueryTermOptions{});
		}

		template <typename T>
		inline ObserverBuilder& ObserverBuilder::no() {
			return no<T>(QueryTermOptions{});
		}

		template <typename Rel>
		inline ObserverBuilder& ObserverBuilder::depth_order() {
			return depth_order(detail::typed_query_raw_entity<Rel>(m_world));
		}

		template <typename Func, std::enable_if_t<!std::is_invocable_v<Func, Iter&>, int>>
		inline ObserverBuilder& ObserverBuilder::on_each(Func func) {
			validate();

			auto& ctx = runtime_data();
			using InputArgs = decltype(core::func_args(&Func::operator()));

			auto& queryInfo = ctx.query.fetch();
			TypedQueryArgMeta metas[MAX_ITEMS_IN_QUERY]{};
			const auto argCount = init_typed_query_arg_metas(metas, m_world, InputArgs{});
			const auto execState = detail::build_typed_query_exec_state(ctx.query, m_world, queryInfo, metas, argCount);
			const auto runDirectChunk = detail::typed_run_direct_chunk_ptr<Func>(InputArgs{});
			const auto runMappedChunk = detail::typed_run_mapped_chunk_ptr<Func>(InputArgs{});
			const auto invokeInherited = typed_invoke_inherited_ptr<Func>(InputArgs{});

	#if GAIA_ASSERT_ENABLED
			ctx.query.match_all(queryInfo);
			GAIA_ASSERT(typed_query_args_match_query(queryInfo, InputArgs{}));
	#endif

			ctx.on_each_func = [e = m_entity, func, execState, runDirectChunk, runMappedChunk, invokeInherited](
									 Iter& it) mutable {
				auto& obs = it.world()->observers().data(e);
				auto& world = *it.world();
				const auto entity = it.view<Entity>()[0];
				auto& queryInfo = obs.query.fetch();
				if (execState.hasInheritedTerms) {
					invokeInherited(world, entity, execState.argIds, &func);
					finish_typed_query_args_by_id(world, entity, execState.argIds, execState.writeFlags, execState.argCount);
					return;
				}

				if (execState.canUseDirectChunkEval)
					runDirectChunk(obs.query, it, &func, execState);
				else
					runMappedChunk(obs.query, queryInfo, it, &func, execState);
			};

			return *this;
		}
	} // namespace ecs
} // namespace gaia
#endif
