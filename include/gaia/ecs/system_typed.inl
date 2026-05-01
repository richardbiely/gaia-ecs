#pragma once

#include "gaia/ecs/query_builder_typed.inl"
#include "gaia/ecs/query_typed.inl"

#if GAIA_SYSTEMS_ENABLED
namespace gaia {
	namespace ecs {
		//! Adds a typed required term to the underlying system query.
		//! \tparam T Component, entity type, or pair type to require.
		//! \param options Query-term options applied after Gaia-ECS derives typed defaults.
		//! \return Self reference.
		//! \see SystemBuilder::all(Entity, const QueryTermOptions&)
		template <typename T>
		inline SystemBuilder& SystemBuilder::all(const QueryTermOptions& options) {
			validate();
			return all(
					detail::typed_query_term_entity<T>(m_world), detail::typed_query_term_options<T>(QueryOpKind::All, options));
		}

		//! Adds a typed any-of term to the underlying system query.
		//! \tparam T Component, entity type, or pair type participating in the any-of set.
		//! \param options Query-term options applied after Gaia-ECS derives typed defaults.
		//! \return Self reference.
		//! \see SystemBuilder::any(Entity, const QueryTermOptions&)
		template <typename T>
		inline SystemBuilder& SystemBuilder::any(const QueryTermOptions& options) {
			validate();
			return any(
					detail::typed_query_term_entity<T>(m_world), detail::typed_query_term_options<T>(QueryOpKind::Any, options));
		}

		//! Adds a typed OR term to the underlying system query.
		//! \tparam T Component, entity type, or pair type participating in the OR chain.
		//! \param options Query-term options applied after Gaia-ECS derives typed defaults.
		//! \return Self reference.
		//! \see SystemBuilder::or_(Entity, const QueryTermOptions&)
		template <typename T>
		inline SystemBuilder& SystemBuilder::or_(const QueryTermOptions& options) {
			validate();
			return or_(
					detail::typed_query_term_entity<T>(m_world), detail::typed_query_term_options<T>(QueryOpKind::Or, options));
		}

		//! Adds a typed negated term to the underlying system query.
		//! \tparam T Component, entity type, or pair type that must not be present.
		//! \param options Query-term options applied after Gaia-ECS derives typed defaults.
		//! \return Self reference.
		//! \see SystemBuilder::no(Entity, const QueryTermOptions&)
		template <typename T>
		inline SystemBuilder& SystemBuilder::no(const QueryTermOptions& options) {
			validate();
			return no(
					detail::typed_query_term_entity<T>(m_world), detail::typed_query_term_options<T>(QueryOpKind::Not, options));
		}

		//! Adds a typed required term with default query-term options.
		//! \tparam T Component, entity type, or pair type to require.
		//! \return Self reference.
		//! \see SystemBuilder::all(const QueryTermOptions&)
		template <typename T>
		inline SystemBuilder& SystemBuilder::all() {
			return all<T>(QueryTermOptions{});
		}

		//! Adds a typed any-of term with default query-term options.
		//! \tparam T Component, entity type, or pair type participating in the any-of set.
		//! \return Self reference.
		//! \see SystemBuilder::any(const QueryTermOptions&)
		template <typename T>
		inline SystemBuilder& SystemBuilder::any() {
			return any<T>(QueryTermOptions{});
		}

		//! Adds a typed OR term with default query-term options.
		//! \tparam T Component, entity type, or pair type participating in the OR chain.
		//! \return Self reference.
		//! \see SystemBuilder::or_(const QueryTermOptions&)
		template <typename T>
		inline SystemBuilder& SystemBuilder::or_() {
			return or_<T>(QueryTermOptions{});
		}

		//! Adds a typed negated term with default query-term options.
		//! \tparam T Component, entity type, or pair type that must not be present.
		//! \return Self reference.
		//! \see SystemBuilder::no(const QueryTermOptions&)
		template <typename T>
		inline SystemBuilder& SystemBuilder::no() {
			return no<T>(QueryTermOptions{});
		}

		//! Adds a typed changed-filter term to the underlying system query.
		//! \tparam T Component, entity type, or pair type whose changed state is tested.
		//! \return Self reference.
		//! \see SystemBuilder::changed(Entity)
		template <typename T>
		inline SystemBuilder& SystemBuilder::changed() {
			return changed(detail::typed_query_raw_entity<T>(m_world));
		}

		//! Enables typed relation depth ordering for the underlying system query.
		//! \tparam Rel Fragmenting hierarchy relation used for breadth-first top-down cache ordering.
		//! \return Self reference.
		//! \see SystemBuilder::depth_order(Entity)
		template <typename Rel>
		inline SystemBuilder& SystemBuilder::depth_order() {
			return depth_order(detail::typed_query_raw_entity<Rel>(m_world));
		}

		//! Groups matching archetypes by a typed entity id.
		//! \tparam T Component or entity type used as the grouping id.
		//! \param func Grouping callback used to calculate the GroupId for matching archetypes.
		//! \return Self reference.
		//! \see SystemBuilder::group_by(Entity, TGroupByFunc)
		template <typename T>
		inline SystemBuilder& SystemBuilder::group_by(TGroupByFunc func) {
			return group_by(detail::typed_query_raw_entity<T>(m_world), func);
		}

		//! Groups matching archetypes by a typed relation pair.
		//! \tparam Rel Relation type used as the pair relation.
		//! \tparam Tgt Target type used as the pair target.
		//! \param func Grouping callback used to calculate the GroupId for matching archetypes.
		//! \return Self reference.
		//! \see SystemBuilder::group_by(Entity, TGroupByFunc)
		template <typename Rel, typename Tgt>
		inline SystemBuilder& SystemBuilder::group_by(TGroupByFunc func) {
			return group_by(detail::typed_query_pair_entity<Rel, Tgt>(m_world), func);
		}

		//! Declares a typed relation dependency for grouped cache invalidation.
		//! \tparam Rel Relation type the grouping result depends on.
		//! \return Self reference.
		//! \see SystemBuilder::group_dep(Entity)
		template <typename Rel>
		inline SystemBuilder& SystemBuilder::group_dep() {
			return group_dep(detail::typed_query_raw_entity<Rel>(m_world));
		}

		//! Selects a typed group id for the underlying system query.
		//! \tparam T Component or entity type treated as the group to iterate.
		//! \return Self reference.
		//! \see SystemBuilder::group_id(Entity)
		template <typename T>
		inline SystemBuilder& SystemBuilder::group_id() {
			return group_id(detail::typed_query_raw_entity<T>(m_world));
		}

		//! Registers a typed component callback and prepares the typed query execution path.
		//! \tparam Func Callable type accepted by Gaia-ECS typed query dispatch.
		//! \param func Callback copied into the system runtime payload.
		//! \return Self reference.
		//! \note This path does not expose Iter directly. Use the iterator callback overload when the system needs
		//! Iter::ctx().
		template <typename Func, std::enable_if_t<!detail::is_query_iter_callback_v<Func>, int>>
		inline SystemBuilder& SystemBuilder::on_each(Func func) {
			validate();

			auto& ctx = data();
			auto& runtime = runtime_data();
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
				runtime.on_each_func = [func, execState, runDirectFastChunk, runDirectChunk, runMappedChunk, invokeInherited](
																	 Query& query, QueryExecType execType, SystemRuntimeData::RunMode mode) mutable {
					if (mode == SystemRuntimeData::RunMode::DeferredJob)
						return query.job(func, execType);

					query.each_typed_erased(
							execType, &func, execState, runDirectFastChunk, runDirectChunk, runMappedChunk,
							execState.needsInheritedArgIds, invokeInherited);
					return SchedJob{};
				};
			} else {
				runtime.on_each_func = [func, execState, runDirectFastChunk, runMappedChunk](
																	 Query& query, QueryExecType execType, SystemRuntimeData::RunMode mode) mutable {
					if (mode == SystemRuntimeData::RunMode::DeferredJob)
						return query.job(func, execType);

					query.each_iter_erased(execType, &func, execState, runDirectFastChunk, runMappedChunk);
					return SchedJob{};
				};
			}

			return *this;
		}
	} // namespace ecs
} // namespace gaia
#endif
