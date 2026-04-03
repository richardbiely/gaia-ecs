#pragma once

#include "gaia/ecs/query_adapter_typed.inl"

#if GAIA_OBSERVERS_ENABLED
namespace gaia {
	namespace ecs {
		template <QueryOpKind Op, typename T>
		inline void ObserverBuilder::reg_typed_term(ObserverRuntimeData& data) {
			const auto term = m_world.template reg_comp<T>().entity;
			cache_term_id(data, term);
			data.plan.add_term_descriptor(Op, is_fast_path_eligible_term(term, QueryTermOptions{}));
			register_diff_term(data, Op, term, QueryTermOptions{});
			m_world.observers().add(m_world, term, m_entity, QueryMatchKind::Semantic);
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
			validate();
			auto& data = runtime_data();
			data.query.template all<T>();
			reg_typed_term<QueryOpKind::All, T>(data);
			return *this;
		}

		template <typename T>
		inline ObserverBuilder& ObserverBuilder::any() {
			validate();
			auto& data = runtime_data();
			data.query.template any<T>();
			reg_typed_term<QueryOpKind::Any, T>(data);
			return *this;
		}

		template <typename T>
		inline ObserverBuilder& ObserverBuilder::or_() {
			validate();
			auto& data = runtime_data();
			data.query.template or_<T>();
			reg_typed_term<QueryOpKind::Or, T>(data);
			return *this;
		}

		template <typename T>
		inline ObserverBuilder& ObserverBuilder::no() {
			validate();
			auto& data = runtime_data();
			data.query.template no<T>();
			reg_typed_term<QueryOpKind::Not, T>(data);
			return *this;
		}

		template <typename Rel>
		inline ObserverBuilder& ObserverBuilder::depth_order() {
			validate();
			runtime_data().query.template depth_order<Rel>();
			return *this;
		}

		template <typename... T>
		static bool observer_uses_inherited_arg_path(Query& query, core::func_type_list<T...>) {
			constexpr bool needsInheritedArgIds =
					(!std::is_same_v<std::remove_cv_t<std::remove_reference_t<T>>, Entity> || ... || false);
			if constexpr (!needsInheritedArgIds)
				return false;
			else
				return query.fetch().has_potential_inherited_id_terms();
		}

		template <typename Func, typename... T>
		static void observer_run_typed_on_entity(
				ObserverRuntimeData& obs, World& world, Entity entity, Iter& it, Func& func, core::func_type_list<T...>,
				bool hasInheritedTerms, const Entity* pInheritedArgIds, const bool* pWriteFlags) {
			constexpr bool needsInheritedArgIds =
					(!std::is_same_v<std::remove_cv_t<std::remove_reference_t<T>>, Entity> || ... || false);
			if constexpr (!needsInheritedArgIds)
				obs.query.run_query_on_chunk(it, func, core::func_type_list<T...>{});
			else {
				if (hasInheritedTerms) {
					invoke_typed_query_args_by_id<T...>(world, entity, pInheritedArgIds, func, std::index_sequence_for<T...>{});
					finish_typed_query_args_by_id(world, entity, pInheritedArgIds, pWriteFlags, sizeof...(T));
					return;
				}

				obs.query.run_query_on_chunk(it, func, core::func_type_list<T...>{});
			}
		}

		template <typename Func, std::enable_if_t<!std::is_invocable_v<Func, Iter&>, int>>
		inline ObserverBuilder& ObserverBuilder::on_each(Func func) {
			validate();

			auto& ctx = runtime_data();
			using InputArgs = decltype(core::func_args(&Func::operator()));

			const bool hasInheritedTerms = observer_uses_inherited_arg_path(ctx.query, InputArgs{});
			Entity inheritedArgIds[ChunkHeader::MAX_COMPONENTS] = {};
			bool inheritedArgWriteFlags[ChunkHeader::MAX_COMPONENTS] = {};
			init_typed_query_arg_descs(inheritedArgIds, inheritedArgWriteFlags, m_world, InputArgs{});

	#if GAIA_ASSERT_ENABLED
			auto& queryInfo = ctx.query.fetch();
			ctx.query.match_all(queryInfo);
			GAIA_ASSERT(typed_query_args_match_query(queryInfo, InputArgs{}));
	#endif

			ctx.on_each_func = [e = m_entity, func, hasInheritedTerms, inheritedArgIds,
													inheritedArgWriteFlags](Iter& it) mutable {
				auto& obs = it.world()->observers().data(e);
				auto& world = *it.world();
				const auto entity = it.view<Entity>()[0];
				observer_run_typed_on_entity(
						obs, world, entity, it, func, InputArgs{}, hasInheritedTerms, inheritedArgIds, inheritedArgWriteFlags);
			};

			return *this;
		}
	} // namespace ecs
} // namespace gaia
#endif
