#pragma once
#include "gaia/config/config.h"

#include "gaia/cnt/darray.h"
#include "gaia/cnt/map.h"
#include "gaia/cnt/set.h"
#include "gaia/ecs/observer.h"

#if GAIA_OBSERVERS_ENABLED
namespace gaia {
	namespace ecs {
		class World;
		class Archetype;

		//! Runtime storage for observer callbacks and dispatch indexes kept out-of-line from ECS component storage.
		class ObserverRegistry {
			struct DiffObserverIndex {
				//! Exact direct term to diff observer mapping.
				cnt::map<EntityLookupKey, cnt::darray<Entity>> direct;
				//! Source-evaluated term to diff observer mapping.
				cnt::map<EntityLookupKey, cnt::darray<Entity>> sourceTerm;
				//! Traversal relation to diff observer mapping.
				cnt::map<EntityLookupKey, cnt::darray<Entity>> traversalRelation;
				//! Pair relation to diff observer mapping for wildcard/variable target terms.
				cnt::map<EntityLookupKey, cnt::darray<Entity>> pairRelation;
				//! Pair target to diff observer mapping for wildcard/variable relation terms.
				cnt::map<EntityLookupKey, cnt::darray<Entity>> pairTarget;
				//! Full diff observer set for call sites without precise changed-term spans.
				cnt::darray<Entity> all;
				//! Broad fallback list for diff observers that cannot be narrowed by changed terms.
				cnt::darray<Entity> global;

				//! Returns true when no observer can be matched for this event.
				GAIA_NODISCARD bool empty() const {
					return direct.empty() && sourceTerm.empty() && traversalRelation.empty() && pairRelation.empty() &&
								 pairTarget.empty() && all.empty() && global.empty();
				}
			};

			struct PropagatedTargetCacheKey {
				Entity bindingRelation = EntityBad;
				Entity traversalRelation = EntityBad;
				Entity rootTarget = EntityBad;
				QueryTravKind travKind = QueryTravKind::None;
				uint8_t travDepth = QueryTermOptions::TravDepthUnlimited;

				GAIA_NODISCARD size_t hash() const {
					size_t seed = EntityLookupKey(bindingRelation).hash();
					seed ^= EntityLookupKey(traversalRelation).hash() + 0x9e3779b9u + (seed << 6u) + (seed >> 2u);
					seed ^= EntityLookupKey(rootTarget).hash() + 0x9e3779b9u + (seed << 6u) + (seed >> 2u);
					seed ^= size_t(travKind) + 0x9e3779b9u + (seed << 6u) + (seed >> 2u);
					seed ^= size_t(travDepth) + 0x9e3779b9u + (seed << 6u) + (seed >> 2u);
					return seed;
				}

				GAIA_NODISCARD bool operator==(const PropagatedTargetCacheKey& other) const {
					return bindingRelation == other.bindingRelation && traversalRelation == other.traversalRelation &&
								 rootTarget == other.rootTarget && travKind == other.travKind && travDepth == other.travDepth;
				}
			};

			struct PropagatedTargetCacheEntry {
				uint32_t bindingRelationVersion = 0;
				uint32_t traversalRelationVersion = 0;
				cnt::darray<Entity> targets;
			};

		public:
			struct DiffDispatcher {
				struct Snapshot {
					ObserverRuntimeData* pObs = nullptr;
					uint32_t matchesBeforeIdx = UINT32_MAX;
				};

				struct MatchCacheEntry {
					ObserverRuntimeData* pObsRepresentative = nullptr;
					QueryInfo* pQueryInfoRepresentative = nullptr;
					uint64_t queryHash = 0;
					cnt::darray<Entity> matches;
				};

				struct TargetNarrowCacheEntry {
					ObserverPlan::DiffPlan::DispatchKind kind = ObserverPlan::DiffPlan::DispatchKind::LocalTargets;
					Entity bindingRelation = EntityBad;
					Entity traversalRelation = EntityBad;
					QueryTravKind travKind = QueryTravKind::None;
					uint8_t travDepth = QueryTermOptions::TravDepthUnlimited;
					QueryEntityArray triggerTerms{};
					uint8_t triggerTermCount = 0;
					cnt::darray<Entity> targets;
				};

				struct Context {
					ObserverEvent event = ObserverEvent::OnAdd;
					cnt::darray<Snapshot> observers;
					cnt::darray<MatchCacheEntry> matchesBeforeCache;
					cnt::darray<Entity> targets;
					bool active = false;
					bool targeted = false;
					bool targetsAddedAfterPrepare = false;
					bool resetTraversalCaches = false;
				};

				static void collect_query_matches(World& world, ObserverRuntimeData& obs, cnt::darray<Entity>& out);
				static void collect_query_target_matches(
						World& world, ObserverRuntimeData& obs, EntitySpan targets, cnt::darray<Entity>& out);
				static void add_valid_targets(World& world, cnt::darray<Entity>& out, EntitySpan targets);
				static void copy_target_narrow_plan(const ObserverRuntimeData& obs, TargetNarrowCacheEntry& entry);
				GAIA_NODISCARD static bool
				same_target_narrow_plan(const ObserverRuntimeData& obs, const TargetNarrowCacheEntry& entry);
				static void normalize_targets(cnt::darray<Entity>& targets);
				GAIA_NODISCARD static uint64_t query_hash(ObserverRuntimeData& obs);
				GAIA_NODISCARD static bool same_query_ctx(const QueryCtx& left, const QueryCtx& right);
				GAIA_NODISCARD static int32_t
				find_match_cache_entry(cnt::darray<MatchCacheEntry>& cache, ObserverRuntimeData& obs);
				GAIA_NODISCARD static Context prepare(
						ObserverRegistry& registry, World& world, ObserverEvent event, EntitySpan terms,
						EntitySpan targetEntities = {});
				GAIA_NODISCARD static Context prepare_add_new(ObserverRegistry& registry, World& world, EntitySpan terms);
				static void add_targets(World& world, Context& ctx, EntitySpan targets);
				static void finish(World& world, Context&& ctx);
			};

			struct DirectDispatcher {
				static void on_add(
						ObserverRegistry& registry, World& world, const Archetype& archetype, EntitySpan entsAdded,
						EntitySpan targets);
				static void on_del(
						ObserverRegistry& registry, World& world, const Archetype& archetype, EntitySpan entsRemoved,
						EntitySpan targets);
				static void on_set(ObserverRegistry& registry, World& world, Entity term, EntitySpan targets);
			};

			struct SharedDispatch {
				template <bool DiffOnly, typename TObserverMap>
				static void collect_from_map(
						ObserverRegistry& registry, World& world, const TObserverMap& map, Entity term, uint64_t matchStamp);

				static void collect_diff_from_list(
						ObserverRegistry& registry, World& world, const cnt::darray<Entity>& observers, uint64_t matchStamp);

				template <typename TObserverMap>
				GAIA_NODISCARD static bool has_terms(const TObserverMap& map, EntitySpan terms) {
					for (auto term: terms) {
						const auto it = map.find(EntityLookupKey(term));
						if (it != map.end() && !it->second.empty())
							return true;
					}

					return false;
				}

				template <typename TObserverMap>
				GAIA_NODISCARD static bool has_pair_relations(World& world, const TObserverMap& map, EntitySpan terms);

				template <typename TObserverMap>
				static void collect_for_event_term(
						ObserverRegistry& registry, World& world, const TObserverMap& map, Entity term, uint64_t matchStamp);

				template <typename TObserverMap>
				static void collect_for_is_target(
						ObserverRegistry& registry, World& world, const TObserverMap& map, Entity target, uint64_t matchStamp);

				template <typename Func>
				static void for_each_inherited_term(World& world, Entity baseEntity, Func&& func);

				template <typename TObserverMap>
				GAIA_NODISCARD static bool has_semantic_is_terms(World& world, const TObserverMap& map, EntitySpan terms);

				template <typename TObserverMap>
				GAIA_NODISCARD static bool has_inherited_terms(World& world, const TObserverMap& map, EntitySpan terms);

				template <typename TObserverMap>
				static void collect_for_inherited_terms(
						ObserverRegistry& registry, World& world, const TObserverMap& map, Entity baseEntity, uint64_t matchStamp);

				static void execute_targets(World& world, ObserverRuntimeData& obs, EntitySpan targets);
				GAIA_NODISCARD static bool matches_direct_targets(
						ObserverRuntimeData& obs, const Archetype& archetype, EntitySpan targets, QueryInfo* pQueryInfo = nullptr);
			};

			using DiffDispatchCtx = DiffDispatcher::Context;

		private:
			//! Temporary list of observers preliminary matching the event.
			cnt::darray<ObserverRuntimeData*> m_relevant_observers_tmp;
			//! Runtime observer payload storage.
			cnt::map<EntityLookupKey, ObserverRuntimeData> m_observer_data;
			//! Component to OnAdd observer mapping.
			cnt::map<EntityLookupKey, cnt::darray<Entity>> m_observer_map_add;
			//! Component to OnDel observer mapping.
			cnt::map<EntityLookupKey, cnt::darray<Entity>> m_observer_map_del;
			//! Component to OnSet observer mapping.
			cnt::map<EntityLookupKey, cnt::darray<Entity>> m_observer_map_set;
			//! True when any OnSet observer is registered. Avoids a map probe on the common no-observer write path.
			bool m_hasOnSetObservers = false;
			//! Semantic `Is` target to OnAdd observer mapping.
			cnt::map<EntityLookupKey, cnt::darray<Entity>> m_observer_map_add_is;
			//! Semantic `Is` target to OnDel observer mapping.
			cnt::map<EntityLookupKey, cnt::darray<Entity>> m_observer_map_del_is;
			//! OnAdd diff observer dependency index.
			DiffObserverIndex m_diff_index_add;
			//! OnDel diff observer dependency index.
			DiffObserverIndex m_diff_index_del;
			//! Cached propagated observer targets keyed by supported traversal/source diff shape and changed source.
			cnt::map<PropagatedTargetCacheKey, PropagatedTargetCacheEntry> m_propagated_target_cache;
			//! Monotonically increasing stamp used for O(1) deduplication.
			uint64_t m_current_match_stamp = 0;

			GAIA_NODISCARD DiffObserverIndex& diff_index(ObserverEvent event) {
				GAIA_ASSERT(event == ObserverEvent::OnAdd || event == ObserverEvent::OnDel);
				return event == ObserverEvent::OnAdd ? m_diff_index_add : m_diff_index_del;
			}

			GAIA_NODISCARD const DiffObserverIndex& diff_index(ObserverEvent event) const {
				GAIA_ASSERT(event == ObserverEvent::OnAdd || event == ObserverEvent::OnDel);
				return event == ObserverEvent::OnAdd ? m_diff_index_add : m_diff_index_del;
			}

			GAIA_NODISCARD bool has_observers_for_term(Entity term) const {
				const auto termKey = EntityLookupKey(term);
				return observer_map_has_observers(m_observer_map_add, termKey) ||
							 observer_map_has_observers(m_observer_map_del, termKey) ||
							 observer_map_has_observers(m_observer_map_set, termKey);
			}

			GAIA_NODISCARD static bool can_mark_term_observed(World& world, Entity term);
			GAIA_NODISCARD static bool is_semantic_is_term(Entity term, QueryMatchKind matchKind = QueryMatchKind::Semantic);
			void mark_term_observed(World& world, Entity term, bool observed);

			template <typename TObserverMap>
			static void add_observer_to_map(TObserverMap& map, Entity term, Entity observer) {
				const auto entityKey = EntityLookupKey(term);
				const auto it = map.find(entityKey);
				if (it == map.end())
					map.emplace(entityKey, cnt::darray<Entity>{observer});
				else
					it->second.push_back(observer);
			}

			template <typename TObserverMap>
			static void add_observer_to_map_unique(TObserverMap& map, Entity term, Entity observer) {
				const auto entityKey = EntityLookupKey(term);
				const auto it = map.find(entityKey);
				if (it == map.end())
					map.emplace(entityKey, cnt::darray<Entity>{observer});
				else
					add_observer_to_list(it->second, observer);
			}

			static void add_observer_to_list(cnt::darray<Entity>& list, Entity observer) {
				if (core::has(list, observer))
					return;
				list.push_back(observer);
			}

			static void remove_observer_from_list(cnt::darray<Entity>& list, Entity observer) {
				for (uint32_t i = 0; i < list.size();) {
					if (list[i] == observer)
						core::swap_erase_unsafe(list, i);
					else
						++i;
				}
			}
			template <typename TObserverMap>
			GAIA_NODISCARD bool observer_map_has_observers(const TObserverMap& map, const EntityLookupKey& termKey) const {
				const auto it = map.find(termKey);
				if (it == map.end())
					return false;

				for (const auto observer: it->second) {
					if (m_observer_data.find(EntityLookupKey(observer)) != m_observer_data.end())
						return true;
				}
				return false;
			}

			static void collect_traversal_descendants(
					World& world, Entity relation, Entity root, QueryTravKind travKind, uint8_t travDepth, uint64_t visitStamp,
					cnt::darray<Entity>& outTargets);
			static PropagatedTargetCacheEntry& ensure_propagated_targets_cached(
					ObserverRegistry& registry, World& world, const ObserverRuntimeData& obs, Entity changedSource);
			static void collect_propagated_targets_cached(
					ObserverRegistry& registry, World& world, const ObserverRuntimeData& obs, Entity changedSource,
					uint64_t visitStamp, cnt::set<EntityLookupKey>& visitedPairs, cnt::darray<Entity>& outTargets);
			static void add_propagated_targets_cached(
					ObserverRegistry& registry, World& world, const ObserverRuntimeData& obs, Entity changedSource,
					cnt::darray<Entity>& outTargets);
			GAIA_NODISCARD static bool collect_source_traversal_diff_targets(
					ObserverRegistry& registry, World& world, ObserverRuntimeData& obs, EntitySpan changedTerms,
					EntitySpan changedSources, cnt::darray<Entity>& outTargets);
			GAIA_NODISCARD static bool collect_diff_targets_for_observer(
					ObserverRegistry& registry, World& world, ObserverRuntimeData& obs, EntitySpan changedTerms,
					EntitySpan changedTargets, cnt::darray<Entity>& outTargets);
			GAIA_NODISCARD static bool
			observer_uses_changed_traversal_relation(World& world, const ObserverRuntimeData& obs, EntitySpan changedTerms);

			GAIA_NODISCARD static bool is_dynamic_pair_endpoint(EntityId endpoint) {
				return is_wildcard(endpoint) || is_variable(endpoint);
			}

			GAIA_NODISCARD static bool is_observer_term_globally_dynamic(Entity term) {
				if (term == EntityBad || term == All)
					return true;

				if (!term.pair())
					return is_variable((EntityId)term.id());

				const bool relDynamic = is_dynamic_pair_endpoint(term.id());
				const bool tgtDynamic = is_dynamic_pair_endpoint(term.gen());
				return relDynamic && tgtDynamic;
			}

		public:
			GAIA_NODISCARD bool has_observers(Entity term) const {
				return has_observers_for_term(term);
			}

			//! Returns whether any OnAdd observer can be dispatched.
			//! \return True if at least one OnAdd observer is registered.
			GAIA_NODISCARD bool has_on_add_observers() const {
				return !m_observer_map_add.empty() || !m_observer_map_add_is.empty() || !m_diff_index_add.empty();
			}

			//! Returns whether any OnDel observer can be dispatched.
			//! \return True if at least one OnDel observer is registered.
			GAIA_NODISCARD bool has_on_del_observers() const {
				return !m_observer_map_del.empty() || !m_observer_map_del_is.empty() || !m_diff_index_del.empty();
			}

			GAIA_NODISCARD bool has_on_set_observers(Entity term) const {
				if (!m_hasOnSetObservers)
					return false;
				return m_observer_map_set.find(EntityLookupKey(term)) != m_observer_map_set.end();
			}

			void add_diff_observer_term(World& world, Entity observer, Entity term, const QueryTermOptions& options);
			GAIA_NODISCARD DiffDispatchCtx
			prepare_diff(World& world, ObserverEvent event, EntitySpan terms, EntitySpan targetEntities = {});
			GAIA_NODISCARD DiffDispatchCtx prepare_diff_add_new(World& world, EntitySpan terms);
			void add_diff_targets(World& world, DiffDispatchCtx& ctx, EntitySpan targets);
			void finish_diff(World& world, DiffDispatchCtx&& ctx);

			void teardown() {
				for (auto& it: m_observer_data) {
					auto& obs = it.second;
					obs.on_each_func = {};
					obs.query = {};
					obs.plan = {};
					obs.lastMatchStamp = 0;
				}

				m_relevant_observers_tmp = {};
				m_observer_data = {};
				m_observer_map_add = {};
				m_observer_map_del = {};
				m_observer_map_set = {};
				m_hasOnSetObservers = false;
				m_observer_map_add_is = {};
				m_observer_map_del_is = {};
				m_diff_index_add = {};
				m_diff_index_del = {};
				m_propagated_target_cache = {};
			}

			ObserverRuntimeData& data_add(Entity observer) {
				return m_observer_data[EntityLookupKey(observer)];
			}

			GAIA_NODISCARD ObserverRuntimeData* data_try(Entity observer) {
				const auto it = m_observer_data.find(EntityLookupKey(observer));
				if (it == m_observer_data.end())
					return nullptr;
				return &it->second;
			}

			GAIA_NODISCARD const ObserverRuntimeData* data_try(Entity observer) const {
				const auto it = m_observer_data.find(EntityLookupKey(observer));
				if (it == m_observer_data.end())
					return nullptr;
				return &it->second;
			}

			GAIA_NODISCARD ObserverRuntimeData& data(Entity observer) {
				auto* pData = data_try(observer);
				GAIA_ASSERT(pData != nullptr);
				return *pData;
			}

			GAIA_NODISCARD const ObserverRuntimeData& data(Entity observer) const {
				const auto* pData = data_try(observer);
				GAIA_ASSERT(pData != nullptr);
				return *pData;
			}

			void try_mark_term_observed(World& world, Entity term);

			//! Registers a new term to the observer registry and links an observer with it.
			//! \param world World the observer is triggered for
			//! \param term Term to add to @a observer
			//! \param observer Observer entity
			//! \param matchKind Observer match policy used for the registered term.
			void add(World& world, Entity term, Entity observer, QueryMatchKind matchKind = QueryMatchKind::Semantic);

			//! Removes a term from the observer registry.
			//! \param world World the observer is triggered for
			//! \param term Term to remove from @a observer
			void del(World& world, Entity term);

			//! Called when components are added to an entity.
			//! \param world World the observer is triggered for
			//! \param archetype Archetype we try to match with the observer
			//! \param entsAdded Span of entities added to the @a archetype
			//! \param targets Span on entities for which the observers triggers
			void on_add(World& world, const Archetype& archetype, EntitySpan entsAdded, EntitySpan targets);

			//! Called when components are removed from an entity.
			//! \param world World the observer is triggered for
			//! \param archetype Archetype we try to match with the observer
			//! \param entsRemoved Span of entities removed from the @a archetype
			//! \param targets Span on entities for which the observers triggers
			void on_del(World& world, const Archetype& archetype, EntitySpan entsRemoved, EntitySpan targets);

			//! Dispatches `OnSet` observers for @a term against @a targets.
			//! \param world World the observer is triggered for
			//! \param term Changed term
			//! \param targets Triggered entities
			void on_set(World& world, Entity term, EntitySpan targets);
		};
	} // namespace ecs
} // namespace gaia
#endif
