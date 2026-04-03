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
				if (!world_is_out_of_line_component(world, term)) {
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

		inline void ObserverRuntimeData::exec(Iter& iter, EntitySpan targets) {
			const auto& queryInfo = query.fetch();

	#if GAIA_PROFILER_CPU
			const auto name = entity_name(*queryInfo.world(), entity);
			const char* pScopeName = !name.empty() ? name.data() : sc_observer_query_func_str;
			GAIA_PROF_SCOPE2(pScopeName);
	#endif

			auto* pWorld = iter.world();
			const auto queryIdCnt = (uint32_t)plan.termCount;
			const auto& termIds = queryTermIds;

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
							const auto queryId = termIds[i];
							auto compIdx = world_component_index_comp_idx(*pWorld, *ec.pArchetype, queryId);
							if (compIdx == BadIndex)
								compIdx = core::get_index(ec.pArchetype->ids_view(), queryId);
							cachedIndices[i] = (uint8_t)compIdx;
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

			static void cache_term_id(ObserverRuntimeData& data, Entity term) {
				GAIA_ASSERT(data.plan.termCount < MAX_ITEMS_IN_QUERY);
				if (data.plan.termCount < MAX_ITEMS_IN_QUERY)
					data.queryTermIds[data.plan.termCount] = term;
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

			void register_diff_term(ObserverRuntimeData& data, QueryOpKind op, Entity term, const QueryTermOptions& options) {
				if (!requires_diff_dispatch(term, options))
					return;

				data.plan.diff.enabled = true;
				data.plan.refresh_exec_kind();
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
				m_world.observers().add_diff_observer_term(m_world, m_entity, term, options);
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

				mark_unsupported();
			}

			template <QueryOpKind Op, typename T>
			void reg_typed_term(ObserverRuntimeData& data);

			template <QueryOpKind Op, typename T>
			void reg_typed_term(ObserverRuntimeData& data, const QueryTermOptions& options);

		public:
			ObserverBuilder(World& world, Entity entity): m_world(world), m_entity(entity) {}

			//------------------------------------------------

			ObserverBuilder& event(ObserverEvent event) {
				validate();
				data().event = event;
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

				cache_term_id(data, item.id);
				data.plan.add_term_descriptor(item.op, is_fast_path_eligible_term(item.id, options));
				register_diff_term(data, item.op, item.id, options);
				m_world.observers().add(m_world, item.id, m_entity, item.matchKind);
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
				cache_term_id(data, entity);
				data.plan.add_term_descriptor(QueryOpKind::All, is_fast_path_eligible_term(entity, options));
				register_diff_term(data, QueryOpKind::All, entity, options);
				m_world.observers().add(m_world, entity, m_entity, options.matchKind);
				return *this;
			}

			ObserverBuilder& any(Entity entity, const QueryTermOptions& options = {}) {
				validate();
				auto& data = runtime_data();
				data.query.any(entity, options);
				cache_term_id(data, entity);
				data.plan.add_term_descriptor(QueryOpKind::Any, is_fast_path_eligible_term(entity, options));
				register_diff_term(data, QueryOpKind::Any, entity, options);
				m_world.observers().add(m_world, entity, m_entity, options.matchKind);
				return *this;
			}

			ObserverBuilder& or_(Entity entity, const QueryTermOptions& options = {}) {
				validate();
				auto& data = runtime_data();
				data.query.or_(entity, options);
				cache_term_id(data, entity);
				data.plan.add_term_descriptor(QueryOpKind::Or, is_fast_path_eligible_term(entity, options));
				register_diff_term(data, QueryOpKind::Or, entity, options);
				m_world.observers().add(m_world, entity, m_entity, options.matchKind);
				return *this;
			}

			ObserverBuilder& no(Entity entity, const QueryTermOptions& options = {}) {
				validate();
				auto& data = runtime_data();
				data.query.no(entity, options);
				cache_term_id(data, entity);
				data.plan.add_term_descriptor(QueryOpKind::Not, is_fast_path_eligible_term(entity, options));
				register_diff_term(data, QueryOpKind::Not, entity, options);
				m_world.observers().add(m_world, entity, m_entity, options.matchKind);
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
				ctx.exec(iter, targets);
			}
		};

	} // namespace ecs
} // namespace gaia

	#include "gaia/ecs/observer_typed.inl"

#endif
