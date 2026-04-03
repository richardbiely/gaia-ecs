#pragma once

#include "gaia/ecs/query_adapter_typed.inl"

#if GAIA_OBSERVERS_ENABLED
namespace gaia {
	namespace ecs {
		template <QueryOpKind Op, typename T>
		inline void ObserverBuilder::reg_typed_term(ObserverRuntimeData& data) {
			reg_typed_term<Op, T>(data, QueryTermOptions{});
		}

		template <QueryOpKind Op, typename T>
		inline void ObserverBuilder::reg_typed_term(ObserverRuntimeData& data, const QueryTermOptions& options) {
			const auto term = m_world.template reg_comp<T>().entity;
			cache_term_id(data, term);
			data.plan.add_term_descriptor(Op, is_fast_path_eligible_term(term, options));
			register_diff_term(data, Op, term, options);
			m_world.observers().add(m_world, term, m_entity, options.matchKind);
		}

		template <typename T>
		inline ObserverBuilder& ObserverBuilder::all(const QueryTermOptions& options) {
			validate();
			auto& data = runtime_data();
			data.query.template all<T>(options);
			reg_typed_term<QueryOpKind::All, T>(data, options);
			return *this;
		}

		template <typename T>
		inline ObserverBuilder& ObserverBuilder::any(const QueryTermOptions& options) {
			validate();
			auto& data = runtime_data();
			data.query.template any<T>(options);
			reg_typed_term<QueryOpKind::Any, T>(data, options);
			return *this;
		}

		template <typename T>
		inline ObserverBuilder& ObserverBuilder::or_(const QueryTermOptions& options) {
			validate();
			auto& data = runtime_data();
			data.query.template or_<T>(options);
			reg_typed_term<QueryOpKind::Or, T>(data, options);
			return *this;
		}

		template <typename T>
		inline ObserverBuilder& ObserverBuilder::no(const QueryTermOptions& options) {
			validate();
			auto& data = runtime_data();
			data.query.template no<T>(options);
			reg_typed_term<QueryOpKind::Not, T>(data, options);
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
			using UO = typename component_type_t<Rel>::TypeOriginal;
			static_assert(core::is_raw_v<UO>, "Use depth_order() with raw relation types only");

			const auto& desc = comp_cache_add<Rel>(m_world);
			return depth_order(desc.entity);
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
				const TypedQueryArgMeta* pArgMetas, uint32_t argCount,
				const Entity* pInheritedArgIds, const bool* pWriteFlags,
				void (*runDirect)(Query&, Iter&, void*, const TypedQueryExecState&),
				void (*runMapped)(Query&, const QueryInfo&, Iter&, void*, const TypedQueryExecState&),
				void (*invokeInherited)(World&, Entity, const Entity*, void*)) {
			auto& queryInfo = obs.query.fetch();
			const auto state = detail::build_typed_query_exec_state(obs.query, world, queryInfo, pArgMetas, argCount);
			if (state.hasInheritedTerms) {
				invokeInherited(world, entity, pInheritedArgIds, pFunc);
				finish_typed_query_args_by_id(world, entity, pInheritedArgIds, pWriteFlags, argCount);
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
				const TypedQueryArgMeta* pArgMetas, uint32_t argCount,
				const Entity* pInheritedArgIds, const bool* pWriteFlags, core::func_type_list<T...>) {
			observer_run_typed_on_entity_erased(
					obs, world, entity, it, pFunc, pArgMetas, argCount, pInheritedArgIds, pWriteFlags,
					&observer_run_typed_direct_cb<Func, T...>, &observer_run_typed_mapped_cb<Func, T...>,
					&invoke_typed_query_args_by_id_erased<Func, T...>);
		}

		template <typename Func, std::enable_if_t<!std::is_invocable_v<Func, Iter&>, int>>
		inline ObserverBuilder& ObserverBuilder::on_each(Func func) {
			validate();

			auto& ctx = runtime_data();
			using InputArgs = decltype(core::func_args(&Func::operator()));

			TypedQueryArgMeta argMetas[MAX_ITEMS_IN_QUERY] = {};
			const auto argCount = init_typed_query_arg_metas(argMetas, m_world, InputArgs{});
			Entity inheritedArgIds[ChunkHeader::MAX_COMPONENTS] = {};
			bool inheritedArgWriteFlags[ChunkHeader::MAX_COMPONENTS] = {};
			init_typed_query_arg_descs_from_metas(inheritedArgIds, inheritedArgWriteFlags, argMetas, argCount);

	#if GAIA_ASSERT_ENABLED
			auto& queryInfo = ctx.query.fetch();
			ctx.query.match_all(queryInfo);
			GAIA_ASSERT(typed_query_args_match_query(queryInfo, InputArgs{}));
	#endif

			ctx.on_each_func = [e = m_entity, func, argMetas, inheritedArgIds, inheritedArgWriteFlags, argCount](Iter& it) mutable {
				auto& obs = it.world()->observers().data(e);
				auto& world = *it.world();
				const auto entity = it.view<Entity>()[0];
				observer_run_typed_on_entity<Func>(
						obs, world, entity, it, &func, argMetas, argCount, inheritedArgIds, inheritedArgWriteFlags, InputArgs{});
			};

			return *this;
		}
	} // namespace ecs
} // namespace gaia
#endif
