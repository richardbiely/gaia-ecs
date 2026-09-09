#include "gaia/config/config.h"

#include <cinttypes>

#include "gaia/ecs/chunk_iterator.h"
#include "gaia/ecs/id.h"
#include "gaia/ecs/observer.h"

#if GAIA_OBSERVERS_ENABLED
namespace gaia {
	namespace ecs {
		inline void world_finish_write(World& world, Entity term, Entity entity);

		static void observer_finish_iter_writes(Iter& it) {
			auto* pChunk = const_cast<Chunk*>(it.chunk());
			if (pChunk == nullptr)
				return;

			for (auto compIdx: it.touched_comp_indices())
				pChunk->finish_write(compIdx, it.row_begin(), it.row_end());

			auto terms = it.touched_terms();
			if (terms.empty())
				return;

			auto& world = *it.world();
			const auto entities = it.entity_rows();
			GAIA_EACH(terms) {
				const auto term = terms[i];
				if (!world_component_uses_sparse_storage(world, term)) {
					const auto compIdx = core::get_index(it.chunk()->ids_view(), term);
					if (compIdx != BadIndex) {
						pChunk->finish_write(compIdx, it.row_begin(), it.row_end());
						continue;
					}
				}

				GAIA_FOR_(entities.size(), j) {
					world_finish_write(world, term, entities[j]);
				}
			}
		}

		inline void ObserverRuntimeData::exec(Iter& iter, EntitySpan targets, ObserverEvent reportedEvent) {
			const auto& queryInfo = query.fetch();
	#if GAIA_ASSERT_ENABLED
			iter.set_query_access(&queryInfo.ctx().data);
	#endif

	#if GAIA_PROFILER_CPU
			const auto name = entity_name(*queryInfo.world(), entity);
			const char* pScopeName = !name.empty() ? name.data() : sc_observer_query_func_str;
			GAIA_PROF_SCOPE2(pScopeName);
	#endif

			auto* pWorld = iter.world();
	#if GAIA_OBSERVERS_ENABLED && GAIA_ASSERT_ENABLED
			pWorld->observer_callback_enter();
	#endif
			const auto queryIdCnt = (uint32_t)plan.termCount;
			iter.event(reportedEvent);
			const auto& termIds = queryTermIds;
			const auto terms = queryInfo.ctx().data.terms_view();
			const QueryTerm* termsByField[MAX_ITEMS_IN_QUERY]{};
			for (const auto& term: terms)
				termsByField[term.fieldIndex] = &term;

			const Archetype* pCachedArchetype = nullptr;
			uint8_t cachedIndices[ChunkHeader::MAX_COMPONENTS];
			GAIA_FOR(ChunkHeader::MAX_COMPONENTS) {
				cachedIndices[i] = 0xFF;
			}

			for (auto e: targets) {
				const auto& ec = pWorld->fetch(e);
				if (pCachedArchetype != ec.pArchetype) {
					pCachedArchetype = ec.pArchetype;
					GAIA_FOR(ChunkHeader::MAX_COMPONENTS) {
						cachedIndices[i] = 0xFF;
					}

					auto indicesView = queryInfo.try_indices_mapping_view(ec.pArchetype);
					if (!indicesView.empty()) {
						GAIA_FOR(queryIdCnt) {
							cachedIndices[i] = indicesView[i];
						}
					} else {
						GAIA_FOR(queryIdCnt) {
							const auto* pTerm = termsByField[i];
							if (pTerm == nullptr || !query_term_maps_to_current_archetype(*pTerm))
								continue;

							const auto queryId = termIds[i];
							auto compIdx = world_component_index_comp_idx(*pWorld, *ec.pArchetype, queryId);
							if (compIdx == BadIndex || compIdx == ComponentIndexBad)
								compIdx = core::get_index(ec.pArchetype->ids_view(), queryId);
							cachedIndices[i] =
									(compIdx != BadIndex && compIdx != ComponentIndexBad && compIdx < ec.pArchetype->ids_view().size())
											? (uint8_t)compIdx
											: (uint8_t)0xFF;
						}
					}
				}

				iter.set_archetype(ec.pArchetype);
				iter.set_chunk(ec.pChunk, ec.row, (uint16_t)(ec.row + 1));
				iter.set_comp_indices(cachedIndices);
				iter.set_term_ids(termIds.data());
				iter.set_write_im(false);
				on_each_func(iter);
				observer_finish_iter_writes(iter);
				iter.clear_touched_writes();
			}
	#if GAIA_OBSERVERS_ENABLED && GAIA_ASSERT_ENABLED
			pWorld->observer_callback_leave();
	#endif
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

			static void
			cache_term_desc(ObserverRuntimeData& data, QueryOpKind op, Entity term, const QueryTermOptions& options) {
				GAIA_ASSERT(data.plan.termCount < MAX_ITEMS_IN_QUERY);
				if (data.plan.termCount < MAX_ITEMS_IN_QUERY) {
					data.queryTermIds[data.plan.termCount] = term;
					data.queryTermOps[data.plan.termCount] = op;
					data.queryTermMatchKinds[data.plan.termCount] = options.matchKind;
				}
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

			bool requires_diff_dispatch(Entity term, const QueryTermOptions& options) const {
				if (options.entSrc != EntityBad || options.entTrav != EntityBad)
					return true;

				if (term == EntityBad || term == All)
					return true;

				if (is_variable((EntityId)term.id()))
					return true;

				if (term.pair()) {
					if (is_wildcard(term))
						return true;
					if (is_variable((EntityId)term.id()) || is_variable((EntityId)term.gen()))
						return true;
				}

				return false;
			}

			void register_diff_term_index(
					ObserverRuntimeData& data, QueryOpKind op, Entity term, const QueryTermOptions& options) {
				if (options.entTrav != EntityBad) {
					bool hasRelation = false;
					GAIA_FOR(data.plan.diff.traversalRelationCount) {
						if (data.plan.diff.traversalRelations[i] == options.entTrav) {
							hasRelation = true;
							break;
						}
					}

					if (!hasRelation) {
						GAIA_ASSERT(data.plan.diff.traversalRelationCount < MAX_ITEMS_IN_QUERY);
						if (data.plan.diff.traversalRelationCount < MAX_ITEMS_IN_QUERY)
							data.plan.diff.traversalRelations[data.plan.diff.traversalRelationCount++] = options.entTrav;
					}
				}
				update_diff_target_narrow_plan(data, op, term, options);
				data.plan.refresh_exec_kind();
				m_world.observers().add_diff_observer_term(m_world, m_entity, op, term, options);
			}

			void register_diff_term(ObserverRuntimeData& data, QueryOpKind op, Entity term, const QueryTermOptions& options) {
				if (data.plan.monitorsQuery) {
					const bool compoundNegative = data.plan.hasNegativeTerm && data.plan.termCount > 1;
					if (requires_diff_dispatch(term, options) || compoundNegative)
						data.plan.diff.enabled = true;
					register_diff_term_index(data, op, term, options);
					return;
				}

				const bool compoundNegative = data.plan.hasNegativeTerm && data.plan.termCount > 1;
				if (data.plan.diff.enabled) {
					register_diff_term_index(data, op, term, options);
					return;
				}

				if (!requires_diff_dispatch(term, options) && !compoundNegative)
					return;

				data.plan.diff.enabled = true;
				data.plan.refresh_exec_kind();

				// A compound negative query starts using diff dispatch only after its second
				// term is known. Register every query term so either transition direction
				// can find the observer.
				GAIA_FOR(data.plan.termCount) {
					QueryTermOptions queryOptions{};
					queryOptions.matchKind = data.queryTermMatchKinds[i];
					if (i + 1 == data.plan.termCount)
						queryOptions = options;
					register_diff_term_index(data, data.queryTermOps[i], data.queryTermIds[i], queryOptions);
				}
			}

			void update_diff_target_narrow_plan(
					ObserverRuntimeData& data, QueryOpKind op, Entity term, const QueryTermOptions& options) {
				using DispatchKind = ObserverPlan::DiffPlan::DispatchKind;
				auto& diff = data.plan.diff;
				if (diff.dispatchKind == DispatchKind::GlobalFallback)
					return;

				const auto mark_unsupported = [&] {
					diff.dispatchKind = DispatchKind::GlobalFallback;
					diff.bindingVar = EntityBad;
					diff.bindingRelation = EntityBad;
					diff.traversalRelation = EntityBad;
					diff.travKind = QueryTravKind::None;
					diff.travDepth = QueryTermOptions::TravDepthUnlimited;
					diff.traversalTriggerTermCount = 0;
				};

				if (options.entSrc != EntityBad || options.entTrav != EntityBad) {
					if (options.entSrc == EntityBad || options.entTrav == EntityBad || op != QueryOpKind::All ||
							!query_trav_has(options.travKind, QueryTravKind::Up) ||
							query_trav_has(options.travKind, QueryTravKind::Down)) {
						mark_unsupported();
						return;
					}

					if (diff.bindingVar == EntityBad)
						diff.bindingVar = options.entSrc;
					else if (diff.bindingVar != options.entSrc) {
						mark_unsupported();
						return;
					}

					if (diff.traversalRelation == EntityBad) {
						diff.traversalRelation = options.entTrav;
						diff.travKind = options.travKind;
						diff.travDepth = options.travDepth;
					} else if (
							diff.traversalRelation != options.entTrav || diff.travKind != options.travKind ||
							diff.travDepth != options.travDepth) {
						mark_unsupported();
						return;
					}

					bool hasTerm = false;
					GAIA_FOR(diff.traversalTriggerTermCount) {
						if (diff.traversalTriggerTerms[i] == term) {
							hasTerm = true;
							break;
						}
					}
					if (!hasTerm) {
						if (diff.traversalTriggerTermCount >= MAX_ITEMS_IN_QUERY) {
							mark_unsupported();
							return;
						}
						diff.traversalTriggerTerms[diff.traversalTriggerTermCount++] = term;
					}

					if (diff.dispatchKind == DispatchKind::LocalTargets)
						diff.dispatchKind = DispatchKind::PropagatedTraversal;
					return;
				}

				if (term.pair() && op == QueryOpKind::All && !is_wildcard(term) && !is_variable((EntityId)term.id()) &&
						is_variable((EntityId)term.gen())) {
					const auto bindingVar = entity_from_id(m_world, term.gen());
					const auto bindingRelation = entity_from_id(m_world, term.id());
					if (!m_world.valid(bindingRelation)) {
						mark_unsupported();
						return;
					}

					if (diff.bindingVar == EntityBad)
						diff.bindingVar = bindingVar;
					else if (diff.bindingVar != bindingVar) {
						mark_unsupported();
						return;
					}

					if (diff.bindingRelation == EntityBad)
						diff.bindingRelation = bindingRelation;
					else if (diff.bindingRelation != bindingRelation) {
						mark_unsupported();
						return;
					}

					return;
				}

				// Fixed local terms can use the entities supplied by the structural mutation.
				// Semantic Is terms can affect inheriting entities, so keep those on the
				// conservative global path.
				if (!requires_diff_dispatch(term, options) &&
						(!term.pair() || term.id() != Is.id() || options.matchKind == QueryMatchKind::Direct))
					return;

				mark_unsupported();
			}

			void reg_term(ObserverRuntimeData& data, QueryOpKind op, Entity term, const QueryTermOptions& options) {
				cache_term_desc(data, op, term, options);
				data.plan.add_term_desc(op, is_fast_path_eligible_term(term, options));
				register_diff_term(data, op, term, options);
				m_world.observers().add(m_world, term, m_entity, op, options.matchKind);
			}

			void rebuild_indices(ObserverRuntimeData& data) {
				if (data.plan.termCount == 0)
					return;

				// Terms are registered as they are added to the builder. Rebuild those indexes
				// when the mode is selected later so fluent-call order does not change behavior.
				const auto terms = data.query.fetch().ctx().data.terms_view();
				for (const auto& term: terms) {
					QueryTermOptions options{};
					options.entSrc = term.src;
					options.entTrav = term.entTrav;
					options.travKind = term.travKind;
					options.travDepth = term.travDepth;
					options.matchKind = term.matchKind;

					m_world.observers().add(m_world, term.id, m_entity, term.op, term.matchKind);
					if (data.plan.uses_diff_dispatch())
						m_world.observers().add_diff_observer_term(m_world, m_entity, term.op, term.id, options);
				}
			}

		public:
			ObserverBuilder(World& world, Entity entity): m_world(world), m_entity(entity) {}

			//------------------------------------------------

			//! Selects the event reported by the observer.
			//! Calling this after monitor() switches the observer back to single-event mode.
			//! \param event Event to report.
			//! \return Self reference.
			ObserverBuilder& event(ObserverEvent event) {
				validate();
				auto& observer = data();
				if (!observer.monitorsQuery && observer.event == event)
					return *this;

				auto& runtime = runtime_data();
				if (runtime.plan.termCount != 0)
					m_world.observers().remove_observer_indices(m_world, m_entity);

				observer.event = event;
				observer.monitorsQuery = false;
				runtime.plan.monitorsQuery = false;
				runtime.plan.refresh_exec_kind();
				rebuild_indices(runtime);
				return *this;
			}

			//! Tracks exact whole-query membership transitions in both directions.
			//! The callback receives OnAdd when an entity starts matching and OnDel when it stops matching.
			//! Calling event() afterwards switches the observer back to single-event mode.
			//! \return Self reference.
			ObserverBuilder& monitor() {
				validate();
				auto& observer = data();
				if (observer.monitorsQuery)
					return *this;

				auto& runtime = runtime_data();
				if (runtime.plan.termCount != 0)
					m_world.observers().remove_observer_indices(m_world, m_entity);

				observer.monitorsQuery = true;
				runtime.plan.monitorsQuery = true;
				runtime.plan.refresh_exec_kind();
				rebuild_indices(runtime);
				return *this;
			}

			//! Sets the hard cache-kind requirement for the observer query.
			//! \param kind Requested cache-kind restriction.
			//! \return Self reference.
			ObserverBuilder& kind(QueryCacheKind kind) {
				validate();
				runtime_data().query.kind(kind);
				return *this;
			}

			//! Sets the cache scope used by the observer query.
			//! \param scope Requested scope.
			//! \return Self reference.
			ObserverBuilder& scope(QueryCacheScope scope) {
				validate();
				runtime_data().query.scope(scope);
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

				cache_term_desc(data, item.op, item.id, options);
				data.plan.add_term_desc(item.op, is_fast_path_eligible_term(item.id, options));
				register_diff_term(data, item.op, item.id, options);
				m_world.observers().add(m_world, item.id, m_entity, item.op, item.matchKind);
				return *this;
			}

			//------------------------------------------------

			ObserverBuilder& is(Entity entity, const QueryTermOptions& options = {}) {
				return all(Pair(Is, entity), options);
			}

			//------------------------------------------------

			ObserverBuilder& in(Entity entity, QueryTermOptions options = {}) {
				options.in();
				return all(Pair(Is, entity), options);
			}

			//------------------------------------------------

			ObserverBuilder& all(Entity entity, const QueryTermOptions& options = {}) {
				validate();
				auto& data = runtime_data();
				data.query.all(entity, options);
				reg_term(data, QueryOpKind::All, entity, options);
				return *this;
			}

			ObserverBuilder& any(Entity entity, const QueryTermOptions& options = {}) {
				validate();
				auto& data = runtime_data();
				data.query.any(entity, options);
				reg_term(data, QueryOpKind::Any, entity, options);
				return *this;
			}

			ObserverBuilder& or_(Entity entity, const QueryTermOptions& options = {}) {
				validate();
				auto& data = runtime_data();
				data.query.or_(entity, options);
				reg_term(data, QueryOpKind::Or, entity, options);
				return *this;
			}

			ObserverBuilder& no(Entity entity, const QueryTermOptions& options = {}) {
				validate();
				auto& data = runtime_data();
				data.query.no(entity, options);
				reg_term(data, QueryOpKind::Not, entity, options);
				return *this;
			}

			ObserverBuilder& match_prefab() {
				validate();
				runtime_data().query.match_prefab();
				return *this;
			}

			template <typename T>
			ObserverBuilder& all(const QueryTermOptions& options);

			template <typename T>
			ObserverBuilder& any(const QueryTermOptions& options);

			template <typename T>
			ObserverBuilder& or_(const QueryTermOptions& options);

			template <typename T>
			ObserverBuilder& no(const QueryTermOptions& options);

			//------------------------------------------------

			template <typename T>
			ObserverBuilder& all();

			template <typename T>
			ObserverBuilder& any();

			template <typename T>
			ObserverBuilder& or_();

			template <typename T>
			ObserverBuilder& no();

			//------------------------------------------------

			//! Orders cached query entries by fragmenting relation depth so iteration runs breadth-first top-down.
			//! \param relation Fragmenting hierarchy relation
			ObserverBuilder& depth_order(Entity relation = ChildOf) {
				validate();
				runtime_data().query.depth_order(relation);
				return *this;
			}

			//! Orders cached query entries by fragmenting relation depth so iteration runs breadth-first top-down.
			//! \tparam Rel Fragmenting hierarchy relation, typically ChildOf.
			template <typename Rel>
			ObserverBuilder& depth_order();

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

			template <typename Func, std::enable_if_t<std::is_invocable_v<Func, Iter&>, int> = 0>
			ObserverBuilder& on_each(Func func) {
				validate();

				auto& ctx = runtime_data();
				ctx.on_each_func = [func](Iter& it) {
					func(it);
				};

				return (ObserverBuilder&)*this;
			}

			template <typename Func, std::enable_if_t<!std::is_invocable_v<Func, Iter&>, int> = 0>
			ObserverBuilder& on_each(Func func);

			GAIA_NODISCARD Entity entity() const {
				return m_entity;
			}

			void exec(Iter& iter, EntitySpan targets) {
				auto& ctx = runtime_data();
				ctx.exec(iter, targets, data().event);
			}
		};

	} // namespace ecs
} // namespace gaia

	#include "gaia/ecs/observer_typed.inl"

#endif
