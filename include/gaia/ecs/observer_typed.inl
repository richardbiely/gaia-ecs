#pragma once

#include "gaia/ecs/query_adapter_typed.inl"

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

		template <typename Func, typename... T>
		static void observer_run_typed_direct_cb(Query& query, Iter& it, void* pFunc, const TypedQueryExecState& state) {
			auto& func = *static_cast<Func*>(pFunc);
			detail::run_typed_chunk_direct_finish(query, it, func, state, core::func_type_list<T...>{});
		}

		template <typename Func, typename... T>
		static void observer_run_typed_mapped_cb(
				Query& query, const QueryInfo& queryInfo, Iter& it, void* pFunc, const TypedQueryExecState& state) {
			auto& func = *static_cast<Func*>(pFunc);
			detail::run_typed_chunk_mapped_finish(query, queryInfo, it, func, state, core::func_type_list<T...>{});
		}

		static void observer_run_typed_on_entity_erased(
				ObserverRuntimeData& obs, World& world, Entity entity, Iter& it, void* pFunc,
				const TypedQueryBindState& bindState, const TypedQueryExecState& state,
				void (*runDirect)(Query&, Iter&, void*, const TypedQueryExecState&),
				void (*runMapped)(Query&, const QueryInfo&, Iter&, void*, const TypedQueryExecState&),
				void (*invokeInherited)(World&, Entity, const Entity*, void*)) {
			auto& queryInfo = obs.query.fetch();
			if (state.hasInheritedTerms) {
				invokeInherited(world, entity, bindState.argIds, pFunc);
				finish_typed_query_args_by_id(world, entity, bindState.argIds, bindState.writeFlags, bindState.argCount);
				return;
			}

			if (state.canUseDirectChunkEval)
				runDirect(obs.query, it, pFunc, state);
			else
				runMapped(obs.query, queryInfo, it, pFunc, state);
		}

		template <typename Func, typename... T>
		static void observer_run_typed_on_entity(
				ObserverRuntimeData& obs, World& world, Entity entity, Iter& it, void* pFunc,
				const TypedQueryBindState& bindState, const TypedQueryExecState& state, core::func_type_list<T...>) {
			observer_run_typed_on_entity_erased(
					obs, world, entity, it, pFunc, bindState, state,
					&observer_run_typed_direct_cb<Func, T...>, &observer_run_typed_mapped_cb<Func, T...>,
					&invoke_typed_query_args_by_id_erased<Func, T...>);
		}

		template <typename Func, std::enable_if_t<!std::is_invocable_v<Func, Iter&>, int>>
		inline ObserverBuilder& ObserverBuilder::on_each(Func func) {
			validate();

			auto& ctx = runtime_data();
			using InputArgs = decltype(core::func_args(&Func::operator()));

			const auto bindState = build_typed_query_bind_state(m_world, InputArgs{});
			auto& queryInfo = ctx.query.fetch();
			const auto execState = detail::build_typed_query_exec_state(ctx.query, m_world, queryInfo, bindState);

	#if GAIA_ASSERT_ENABLED
			ctx.query.match_all(queryInfo);
			GAIA_ASSERT(typed_query_args_match_query(queryInfo, InputArgs{}));
	#endif

			ctx.on_each_func = [e = m_entity, func, bindState, execState](Iter& it) mutable {
				auto& obs = it.world()->observers().data(e);
				auto& world = *it.world();
				const auto entity = it.view<Entity>()[0];
				observer_run_typed_on_entity<Func>(obs, world, entity, it, &func, bindState, execState, InputArgs{});
			};

			return *this;
		}
	} // namespace ecs
} // namespace gaia
#endif
