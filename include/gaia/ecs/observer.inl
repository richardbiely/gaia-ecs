#include "gaia/config/config.h"

#include <cinttypes>

#include "gaia/ecs/chunk_iterator.h"
#include "gaia/ecs/id.h"
#include "gaia/ecs/observer.h"

#if GAIA_OBSERVERS_ENABLED
namespace gaia {
	namespace ecs {
		template <typename T>
		inline Entity world_query_arg_id(World& world);

		template <typename T>
		inline decltype(auto) world_query_entity_arg_by_id(World& world, Entity entity, Entity id);

		template <typename T>
		static Entity observer_inherited_arg_id(World& world) {
			using Arg = std::remove_cv_t<std::remove_reference_t<T>>;
			if constexpr (std::is_same_v<Arg, Entity>)
				return EntityBad;
			else
				return world_query_arg_id<Arg>(world);
		}

		template <typename T>
		static decltype(auto) observer_inherited_entity_arg_by_id(World& world, Entity entity, Entity termId) {
			using Arg = std::remove_cv_t<std::remove_reference_t<T>>;
			if constexpr (std::is_same_v<Arg, Entity>)
				return entity;
			else
				return world_query_entity_arg_by_id<T>(world, entity, termId);
		}

		template <typename... T, typename Func, size_t... I>
		static void observer_invoke_inherited_args_by_id(
				World& world, Entity entity, const Entity* pArgIds, Func& func, std::index_sequence<I...>) {
			func(observer_inherited_entity_arg_by_id<T>(world, entity, pArgIds[I])...);
		}

		template <typename Func, typename... T>
		static void observer_run_typed_on_entity(
				ObserverRuntimeData& obs, World& world, Entity entity, Iter& it, Func& func, core::func_type_list<T...>) {
			bool hasInheritedTerms = false;
			auto& queryInfo = obs.query.fetch();
			for (const auto& term: queryInfo.ctx().data.terms_view()) {
				if (Query::uses_inherited_id_matching(world, term)) {
					hasInheritedTerms = true;
					break;
				}
			}

			if constexpr (sizeof...(T) > 0) {
				if (hasInheritedTerms) {
					Entity inheritedArgIds[sizeof...(T)] = {observer_inherited_arg_id<T>(world)...};
					observer_invoke_inherited_args_by_id<T...>(
							world, entity, inheritedArgIds, func, std::index_sequence_for<T...>{});
					return;
				}
			}

			obs.query.run_query_on_chunk(it, func, core::func_type_list<T...>{});
		}

		inline void ObserverRuntimeData::exec(Iter& iter, EntitySpan targets) {
	#if GAIA_PROFILER_CPU
			const auto& queryInfo = query.fetch();
			const char* pName = entity_name(*queryInfo.world(), entity);
			const char* pScopeName = pName != nullptr ? pName : sc_observer_query_func_str;
			GAIA_PROF_SCOPE2(pScopeName);
	#endif

			auto* pWorld = iter.world();
			auto& queryInfo = query.fetch();
			const auto queryIds = queryInfo.ctx().data.ids_view();
			const auto& remapping = queryInfo.ctx().data._remapping;
			for (auto e: targets) {
				const auto& ec = pWorld->fetch(e);
				uint8_t indices[ChunkHeader::MAX_COMPONENTS];
				Entity termIds[ChunkHeader::MAX_COMPONENTS];
				GAIA_FOR(ChunkHeader::MAX_COMPONENTS) {
					indices[i] = 0xFF;
					termIds[i] = EntityBad;
				}

				const auto queryIdCnt = (uint32_t)queryIds.size();
				GAIA_FOR(queryIdCnt) {
					const auto idxBeforeRemapping = remapping[i];
					const auto queryId = queryIds[idxBeforeRemapping];
					const auto compIdx = core::get_index(ec.pArchetype->ids_view(), queryId);
					indices[i] = (uint8_t)compIdx;
					termIds[i] = queryId;
				}

				iter.set_archetype(ec.pArchetype);
				iter.set_chunk(ec.pChunk, ec.row, (uint16_t)(ec.row + 1));
				iter.set_remapping_indices(indices);
				iter.set_term_ids(termIds);
				on_each_func(iter);
			}
		}

		class ObserverBuilder {
			World& m_world;
			Entity m_entity;

			void validate() {
				GAIA_ASSERT(m_world.valid(m_entity));
			}

			Observer_& data() {
				auto ss = m_world.acc_mut(m_entity);
				auto& sys = ss.smut<Observer_>();
				return sys;
			}

			const Observer_& data() const {
				auto ss = m_world.acc(m_entity);
				const auto& sys = ss.get<Observer_>();
				return sys;
			}

			ObserverRuntimeData& runtime_data() {
				return m_world.observers().data(m_entity);
			}

			const ObserverRuntimeData& runtime_data() const {
				return m_world.observers().data(m_entity);
			}

			bool has_default_match_options(const QueryTermOptions& options) const {
				// Access mode (read/write) does not change membership, only access semantics.
				// Source/traversal options can change membership and must stay on generic matcher.
				return options.entSrc == EntityBad && options.entTrav == EntityBad;
			}

			bool is_complex_pair_term(Entity term) const {
				GAIA_ASSERT(term.pair());

				// Wildcards, Is-relations and variable-like endpoints can have dynamic semantics.
				// Keep these on the generic matcher.
				if (is_wildcard(term))
					return true;
				if (term.id() == Is.id())
					return true;
				if (is_variable(term.id()) || is_variable(term.gen()))
					return true;

				return false;
			}

			bool is_fast_path_eligible_term(Entity term, const QueryTermOptions& options) const {
				// Pair/traversal/source terms can carry non-trivial semantics (e.g. IsA-like expressions).
				// Also exclude wildcard-style terms (All), which are not fixed direct term matches.
				// Keep these on the generic matcher for correctness.
				if (term == EntityBad)
					return false;

				if (!has_default_match_options(options))
					return false;

				if (term == All)
					return false;

				if (!term.pair())
					return true;

				// Pair fast-path only supports fixed direct pairs.
				if (is_complex_pair_term(term))
					return false;

				return true;
			}

			template <QueryOpKind Op, typename T>
			void reg_typed_term(ObserverRuntimeData& data) {
				const auto term = m_world.add<T>().entity;
				data.add_term_descriptor(Op, is_fast_path_eligible_term(term, QueryTermOptions{}));
				m_world.observers().add(m_world, term, m_entity, QueryMatchKind::Semantic);
			}

			template <QueryOpKind Op, typename T>
			void reg_typed_term(ObserverRuntimeData& data, const QueryTermOptions& options) {
				const auto term = m_world.add<T>().entity;
				data.add_term_descriptor(Op, is_fast_path_eligible_term(term, options));
				m_world.observers().add(m_world, term, m_entity, options.matchKind);
			}

		public:
			ObserverBuilder(World& world, Entity entity): m_world(world), m_entity(entity) {}

			//------------------------------------------------

			ObserverBuilder& event(ObserverEvent event) {
				validate();
				data().event = event;
				return *this;
			}

			//------------------------------------------------

			ObserverBuilder& add(QueryInput item) {
				validate();
				auto& data = runtime_data();
				data.query.add(item);

				QueryTermOptions options{};
				options.entSrc = item.entSrc;
				options.entTrav = item.entTrav;
				options.travKind = item.travKind;
				options.travDepth = item.travDepth;
				options.access = item.access;
				options.matchKind = item.matchKind;

				data.add_term_descriptor(item.op, is_fast_path_eligible_term(item.id, options));
				m_world.observers().add(m_world, item.id, m_entity, item.matchKind);
				return *this;
			}

			//------------------------------------------------

			ObserverBuilder& is(Entity entity, const QueryTermOptions& options = {}) {
				return all(Pair(Is, entity), options);
			}

			//------------------------------------------------

			ObserverBuilder& all(Entity entity, const QueryTermOptions& options = {}) {
				validate();
				auto& data = runtime_data();
				data.query.all(entity, options);
				data.add_term_descriptor(QueryOpKind::All, is_fast_path_eligible_term(entity, options));
				m_world.observers().add(m_world, entity, m_entity, options.matchKind);
				return *this;
			}

			ObserverBuilder& any(Entity entity, const QueryTermOptions& options = {}) {
				validate();
				auto& data = runtime_data();
				data.query.any(entity, options);
				data.add_term_descriptor(QueryOpKind::Any, is_fast_path_eligible_term(entity, options));
				m_world.observers().add(m_world, entity, m_entity, options.matchKind);
				return *this;
			}

			ObserverBuilder& or_(Entity entity, const QueryTermOptions& options = {}) {
				validate();
				auto& data = runtime_data();
				data.query.or_(entity, options);
				data.add_term_descriptor(QueryOpKind::Or, is_fast_path_eligible_term(entity, options));
				m_world.observers().add(m_world, entity, m_entity, options.matchKind);
				return *this;
			}

			ObserverBuilder& no(Entity entity, const QueryTermOptions& options = {}) {
				validate();
				auto& data = runtime_data();
				data.query.no(entity, options);
				data.add_term_descriptor(QueryOpKind::Not, is_fast_path_eligible_term(entity, options));
				m_world.observers().add(m_world, entity, m_entity, options.matchKind);
				return *this;
			}

			ObserverBuilder& match_prefab() {
				validate();
				runtime_data().query.match_prefab();
				return *this;
			}

			template <typename T>
			ObserverBuilder& all(const QueryTermOptions& options) {
				validate();
				auto& data = runtime_data();
				data.query.template all<T>(options);
				reg_typed_term<QueryOpKind::All, T>(data, options);
				return *this;
			}

			template <typename T>
			ObserverBuilder& any(const QueryTermOptions& options) {
				validate();
				auto& data = runtime_data();
				data.query.template any<T>(options);
				reg_typed_term<QueryOpKind::Any, T>(data, options);
				return *this;
			}

			template <typename T>
			ObserverBuilder& or_(const QueryTermOptions& options) {
				validate();
				auto& data = runtime_data();
				data.query.template or_<T>(options);
				reg_typed_term<QueryOpKind::Or, T>(data, options);
				return *this;
			}

			template <typename T>
			ObserverBuilder& no(const QueryTermOptions& options) {
				validate();
				auto& data = runtime_data();
				data.query.template no<T>(options);
				reg_typed_term<QueryOpKind::Not, T>(data, options);
				return *this;
			}

			//------------------------------------------------

	#if GAIA_USE_VARIADIC_API
			template <typename... T>
			ObserverBuilder& all() {
				validate();
				auto& data = runtime_data();
				data.query.all<T...>();
				(reg_typed_term<QueryOpKind::All, T>(data), ...);
				return *this;
			}
			template <typename... T>
			ObserverBuilder& any() {
				validate();
				auto& data = runtime_data();
				data.query.any<T...>();
				(reg_typed_term<QueryOpKind::Any, T>(data), ...);
				return *this;
			}
			template <typename... T>
			ObserverBuilder& or_() {
				validate();
				auto& data = runtime_data();
				data.query.or_<T...>();
				(reg_typed_term<QueryOpKind::Or, T>(data), ...);
				return *this;
			}
			template <typename... T>
			ObserverBuilder& no() {
				validate();
				auto& data = runtime_data();
				data.query.no<T...>();
				(reg_typed_term<QueryOpKind::Not, T>(data), ...);
				return *this;
			}
	#else
			template <typename T>
			ObserverBuilder& all() {
				validate();
				auto& data = runtime_data();
				data.query.all<T>();
				reg_typed_term<QueryOpKind::All, T>(data);
				return *this;
			}
			template <typename T>
			ObserverBuilder& any() {
				validate();
				auto& data = runtime_data();
				data.query.any<T>();
				reg_typed_term<QueryOpKind::Any, T>(data);
				return *this;
			}
			template <typename T>
			ObserverBuilder& or_() {
				validate();
				auto& data = runtime_data();
				data.query.or_<T>();
				reg_typed_term<QueryOpKind::Or, T>(data);
				return *this;
			}
			template <typename T>
			ObserverBuilder& no() {
				validate();
				auto& data = runtime_data();
				data.query.no<T>();
				reg_typed_term<QueryOpKind::Not, T>(data);
				return *this;
			}
	#endif

			//------------------------------------------------

			ObserverBuilder& name(const char* name, uint32_t len = 0) {
				m_world.name(m_entity, name, len);
				return *this;
			}

			ObserverBuilder& name_raw(const char* name, uint32_t len = 0) {
				m_world.name_raw(m_entity, name, len);
				return *this;
			}

			//------------------------------------------------

			template <typename Func>
			ObserverBuilder& on_each(Func func) {
				validate();

				auto& ctx = runtime_data();
				if constexpr (std::is_invocable_v<Func, Iter&>) {
					ctx.on_each_func = [func](Iter& it) {
						func(it);
					};
				} else {
					using InputArgs = decltype(core::func_args(&Func::operator()));

	#if GAIA_ASSERT_ENABLED
					// Make sure we only use components specified in the query.
					// Constness is respected. Therefore, if a type is const when registered to query,
					// it has to be const (or immutable) also in each().
					auto& queryInfo = ctx.query.fetch();
					ctx.query.match_all(queryInfo);
					GAIA_ASSERT(ctx.query.unpack_args_into_query_has_all(queryInfo, InputArgs{}));
	#endif

					ctx.on_each_func = [e = m_entity, func](Iter& it) mutable {
						auto& obs = it.world()->observers().data(e);
						auto& world = *it.world();
						const auto entity = it.view<Entity>()[0];
						observer_run_typed_on_entity(obs, world, entity, it, func, InputArgs{});
					};
				}

				return (ObserverBuilder&)*this;
			}

			GAIA_NODISCARD Entity entity() const {
				return m_entity;
			}

			void exec(Iter& iter, EntitySpan targets) {
				auto& ctx = runtime_data();
				ctx.exec(iter, targets);
			}
		};

	} // namespace ecs
} // namespace gaia

#endif
