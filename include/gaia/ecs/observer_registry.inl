#if GAIA_OBSERVERS_ENABLED
namespace gaia {
	namespace ecs {
		inline void ObserverRegistry::DiffDispatcher::collect_query_matches(
				World& world, ObserverRuntimeData& obs, cnt::darray<Entity>& out) {
			out.clear();
			if (!world.valid(obs.entity))
				return;

			const auto& ec = world.fetch(obs.entity);
			if (!world.enabled(ec))
				return;

			// Reset the query iterator so this snapshot always starts from the first match.
			obs.query.reset();
			obs.query.collect_entities_enabled(out);

			// The later comparison walks two ordered lists instead of searching one list for every entity.
			core::sort(out, [](Entity left, Entity right) {
				return left.value() < right.value();
			});
		}

		inline void ObserverRegistry::DiffDispatcher::collect_query_target_matches(
				World& world, ObserverRuntimeData& obs, EntitySpan targets, cnt::darray<Entity>& out) {
			out.clear();
			if (!world.valid(obs.entity))
				return;

			const auto& ec = world.fetch(obs.entity);
			if (!world.enabled(ec))
				return;

			auto& queryInfo = obs.query.fetch();
			for (auto entity: targets) {
				if (!world.valid(entity))
					continue;

				const auto& ecTarget = world.fetch(entity);
				if (ecTarget.pArchetype == nullptr)
					continue;

				if (obs.query.matches_any(queryInfo, *ecTarget.pArchetype, EntitySpan{&entity, 1}))
					out.push_back(entity);
			}
		}

		inline void
		ObserverRegistry::DiffDispatcher::add_valid_targets(World& world, cnt::darray<Entity>& out, EntitySpan targets) {
			for (auto entity: targets) {
				if (world.valid(entity))
					out.push_back(entity);
			}
		}

		inline void ObserverRegistry::DiffDispatcher::copy_target_narrow_plan(
				const ObserverRuntimeData& obs, TargetNarrowCacheEntry& entry) {
			entry.kind = obs.plan.diff.dispatchKind;
			entry.bindingRelation = obs.plan.diff.bindingRelation;
			entry.traversalRelation = obs.plan.diff.traversalRelation;
			entry.travKind = obs.plan.diff.travKind;
			entry.travDepth = obs.plan.diff.travDepth;
			entry.triggerTermCount = obs.plan.diff.traversalTriggerTermCount;
			GAIA_FOR(obs.plan.diff.traversalTriggerTermCount) {
				entry.triggerTerms[i] = obs.plan.diff.traversalTriggerTerms[i];
			}
		}

		inline bool ObserverRegistry::DiffDispatcher::same_target_narrow_plan(
				const ObserverRuntimeData& obs, const TargetNarrowCacheEntry& entry) {
			if (obs.plan.diff.dispatchKind != entry.kind || obs.plan.diff.bindingRelation != entry.bindingRelation ||
					obs.plan.diff.traversalRelation != entry.traversalRelation || obs.plan.diff.travKind != entry.travKind ||
					obs.plan.diff.travDepth != entry.travDepth ||
					obs.plan.diff.traversalTriggerTermCount != entry.triggerTermCount)
				return false;

			GAIA_FOR(obs.plan.diff.traversalTriggerTermCount) {
				if (obs.plan.diff.traversalTriggerTerms[i] != entry.triggerTerms[i])
					return false;
			}

			return true;
		}

		inline void ObserverRegistry::DiffDispatcher::normalize_targets(cnt::darray<Entity>& targets) {
			if (targets.empty())
				return;

			// Sorting places duplicates next to each other and gives the diff pass a deterministic order.
			core::sort(targets, [](Entity left, Entity right) {
				return left.value() < right.value();
			});

			uint32_t outIdx = 0;
			for (uint32_t i = 0; i < targets.size(); ++i) {
				if (outIdx != 0 && targets[i] == targets[outIdx - 1])
					continue;
				targets[outIdx++] = targets[i];
			}
			targets.resize(outIdx);
		}

		inline uint64_t ObserverRegistry::DiffDispatcher::query_hash(ObserverRuntimeData& obs) {
			auto& queryInfo = obs.query.fetch();
			return queryInfo.ctx().hashLookup.hash;
		}

		inline bool ObserverRegistry::DiffDispatcher::same_query_ctx(const QueryCtx& left, const QueryCtx& right) {
			// The hash rejects different queries quickly. The field comparisons below make
			// cache sharing safe even if two different queries have the same hash.
			if (left.hashLookup != right.hashLookup)
				return false;

			const auto& leftData = left.data;
			const auto& rightData = right.data;
			if (leftData.idsCnt != rightData.idsCnt || leftData.changedCnt != rightData.changedCnt ||
					leftData.readWriteMask != rightData.readWriteMask || leftData.cacheSrcTrav != rightData.cacheSrcTrav ||
					leftData.sortBy != rightData.sortBy || leftData.sortByFunc != rightData.sortByFunc ||
					leftData.groupBy != rightData.groupBy || leftData.groupByFunc != rightData.groupByFunc)
				return false;

			GAIA_FOR(leftData.idsCnt) {
				if (leftData.terms[i] != rightData.terms[i])
					return false;
			}

			GAIA_FOR(leftData.changedCnt) {
				if (leftData.changed[i] != rightData.changed[i])
					return false;
			}

			return true;
		}

		inline int32_t ObserverRegistry::DiffDispatcher::find_match_cache_entry(
				cnt::darray<MatchCacheEntry>& cache, ObserverRuntimeData& obs) {
			auto& queryInfo = obs.query.fetch();
			auto& queryCtx = queryInfo.ctx();
			const auto queryHash = queryCtx.hashLookup.hash;

			GAIA_FOR((uint32_t)cache.size()) {
				auto& entry = cache[i];

				// The same query object is an immediate match. Other observers may still use
				// an equivalent query, which is checked after the hash comparison.
				if (entry.pQueryInfoRepresentative == &queryInfo)
					return (int32_t)i;
				if (entry.queryHash != queryHash || entry.pObsRepresentative == nullptr)
					continue;

				auto& repQueryInfo = entry.pObsRepresentative->query.fetch();
				if (same_query_ctx(queryCtx, repQueryInfo.ctx()))
					return (int32_t)i;
			}

			return -1;
		}

		inline ObserverRegistry::DiffDispatcher::Context ObserverRegistry::DiffDispatcher::prepare(
				ObserverRegistry& registry, World& world, ObserverEvent event, EntitySpan terms, EntitySpan targetEntities) {
			Context ctx{};
			ctx.event = event;
			const auto& index = registry.diff_index(event);
			if (index.empty())
				return ctx;

			// Creating or deleting an entity also changes whether that entity itself can be
			// used as a query term. Such changes may affect entities beyond the supplied targets.
			bool hasEntityLifecycleTerm = false;
			if (!targetEntities.empty() && !terms.empty()) {
				for (auto term: terms) {
					if (term.pair())
						continue;

					for (auto target: targetEntities) {
						if (term == target) {
							hasEntityLifecycleTerm = true;
							break;
						}
					}

					if (hasEntityLifecycleTerm)
						break;
				}
			}

			// Use the supplied targets only when no observer depends on a source or relation
			// that can carry the change to other entities.
			if (!hasEntityLifecycleTerm && !targetEntities.empty() && !terms.empty() &&
					!SharedDispatch::has_terms(index.sourceTerm, terms) &&
					!SharedDispatch::has_pair_relations(world, index.traversalRelation, terms)) {
				ctx.targeted = true;
				ctx.targets.reserve((uint32_t)targetEntities.size());
				add_valid_targets(world, ctx.targets, targetEntities);
				normalize_targets(ctx.targets);
			}

			// Look up observers through the narrowest available indexes. The match stamp
			// prevents one observer from being added more than once through different terms.
			CandidateScope candidateScope(registry);
			auto& relevantObservers = candidateScope.candidates();
			const auto matchStamp = ++registry.m_current_match_stamp;
			if (terms.empty()) {
				SharedDispatch::collect_diff_from_list(registry, world, index.all, matchStamp, relevantObservers);
			} else {
				for (auto term: terms) {
					SharedDispatch::collect_from_map<true>(registry, world, index.direct, term, matchStamp, relevantObservers);

					if (!term.pair())
						continue;

					if (!is_wildcard(term.id()) && world.valid_entity_id((EntityId)term.id())) {
						const auto relation = entity_from_id(world, term.id());
						if (world.valid(relation)) {
							SharedDispatch::collect_from_map<true>(
									registry, world, index.traversalRelation, relation, matchStamp, relevantObservers);
							SharedDispatch::collect_from_map<true>(
									registry, world, index.pairRelation, relation, matchStamp, relevantObservers);
						}
					}

					if (!is_wildcard(term.gen()) && world.valid_entity_id((EntityId)term.gen())) {
						const auto target = world.get(term.gen());
						if (world.valid(target))
							SharedDispatch::collect_from_map<true>(
									registry, world, index.pairTarget, target, matchStamp, relevantObservers);
					}
				}
			}

			// Entity lifetime changes and globally dynamic terms cannot be represented by
			// a single exact index lookup, so include their broader observer lists.
			if (hasEntityLifecycleTerm)
				SharedDispatch::collect_diff_from_list(registry, world, index.all, matchStamp, relevantObservers);
			if (!terms.empty() && !hasEntityLifecycleTerm)
				SharedDispatch::collect_diff_from_list(registry, world, index.global, matchStamp, relevantObservers);

			// A propagated observer may still turn a broad dispatch into a known target set.
			// Observers with the same propagation plan share the result of that work.
			if (!ctx.targeted && !targetEntities.empty() && !relevantObservers.empty()) {
				cnt::darray<Entity> narrowedTargets;
				cnt::darray<TargetNarrowCacheEntry> narrowCache;
				bool canNarrow = true;

				for (auto* pObs: relevantObservers) {
					if (pObs == nullptr)
						continue;

					const TargetNarrowCacheEntry* pEntry = nullptr;
					for (const auto& entry: narrowCache) {
						if (same_target_narrow_plan(*pObs, entry)) {
							pEntry = &entry;
							break;
						}
					}

					if (pEntry == nullptr) {
						narrowCache.push_back({});
						auto& entry = narrowCache.back();
						copy_target_narrow_plan(*pObs, entry);

						if (!collect_diff_targets_for_observer(registry, world, *pObs, terms, targetEntities, entry.targets)) {
							canNarrow = false;
							break;
						}

						pEntry = &entry;
					}

					for (auto entity: pEntry->targets)
						narrowedTargets.push_back(entity);
				}

				if (canNarrow) {
					normalize_targets(narrowedTargets);
					ctx.targeted = true;
					ctx.targets = GAIA_MOV(narrowedTargets);
				}
			}

			if (relevantObservers.empty())
				return ctx;

			// Capture each distinct query once. Several observers may use the same query,
			// while keeping separate callbacks and event settings.
			ctx.active = true;
			for (auto* pObs: relevantObservers) {
				if (pObs == nullptr)
					continue;

				ctx.observers.push_back({});
				auto& snapshot = ctx.observers.back();
				snapshot.observer = pObs->entity;
				if (!ctx.resetTraversalCaches && observer_uses_changed_traversal_relation(world, *pObs, terms))
					ctx.resetTraversalCaches = true;

				auto cacheIdx = find_match_cache_entry(ctx.matchesBeforeCache, *pObs);
				if (cacheIdx == -1) {
					ctx.matchesBeforeCache.push_back({});
					auto& entry = ctx.matchesBeforeCache.back();
					entry.pObsRepresentative = pObs;
					entry.pQueryInfoRepresentative = &pObs->query.fetch();
					entry.queryHash = query_hash(*pObs);
					if (ctx.targeted)
						collect_query_target_matches(
								world, *pObs, EntitySpan{ctx.targets.data(), ctx.targets.size()}, entry.matches);
					else
						collect_query_matches(world, *pObs, entry.matches);
					cacheIdx = (int32_t)ctx.matchesBeforeCache.size() - 1;
				}

				snapshot.matchesBeforeIdx = (uint32_t)cacheIdx;
			}

			return ctx;
		}

		inline ObserverRegistry::DiffDispatcher::Context
		ObserverRegistry::DiffDispatcher::prepare_add_new(ObserverRegistry& registry, World& world, EntitySpan terms) {
			Context ctx{};
			ctx.event = ObserverEvent::OnAdd;
			const auto& index = registry.diff_index(ObserverEvent::OnAdd);
			if (index.empty())
				return ctx;

			// Source and traversal observers can affect existing entities. They require the
			// normal before snapshot even though the directly created entities are new.
			if (terms.empty() || SharedDispatch::has_terms(index.sourceTerm, terms) ||
					SharedDispatch::has_pair_relations(world, index.traversalRelation, terms)) {
				return prepare(registry, world, ObserverEvent::OnAdd, terms);
			}

			// Only observers reachable from the new entity's terms need to be considered.
			CandidateScope candidateScope(registry);
			auto& relevantObservers = candidateScope.candidates();
			const auto matchStamp = ++registry.m_current_match_stamp;
			for (auto term: terms) {
				SharedDispatch::collect_from_map<true>(registry, world, index.direct, term, matchStamp, relevantObservers);

				if (!term.pair())
					continue;

				if (!is_wildcard(term.id()) && world.valid_entity_id((EntityId)term.id())) {
					const auto relation = entity_from_id(world, term.id());
					if (world.valid(relation))
						SharedDispatch::collect_from_map<true>(
								registry, world, index.pairRelation, relation, matchStamp, relevantObservers);
				}

				if (!is_wildcard(term.gen()) && world.valid_entity_id((EntityId)term.gen())) {
					const auto target = world.get(term.gen());
					if (world.valid(target))
						SharedDispatch::collect_from_map<true>(
								registry, world, index.pairTarget, target, matchStamp, relevantObservers);
				}
			}
			SharedDispatch::collect_diff_from_list(registry, world, index.global, matchStamp, relevantObservers);

			if (relevantObservers.empty())
				return ctx;

			ctx.active = true;
			ctx.targeted = true;
			ctx.targetsAddedAfterPrepare = true;
			for (auto* pObs: relevantObservers) {
				if (pObs == nullptr)
					continue;

				if (!ctx.resetTraversalCaches && observer_uses_changed_traversal_relation(world, *pObs, terms))
					ctx.resetTraversalCaches = true;
				ctx.observers.push_back({});
				ctx.observers.back().observer = pObs->entity;
			}

			return ctx;
		}

		inline void ObserverRegistry::DiffDispatcher::add_targets(World& world, Context& ctx, EntitySpan targets) {
			if (!ctx.active || !ctx.targeted || targets.empty())
				return;

			add_valid_targets(world, ctx.targets, targets);
		}

		inline void ObserverRegistry::DiffDispatcher::finish(World& world, Context&& ctx) {
			if (!ctx.active)
				return;

			// A changed traversal relation can invalidate query paths even when no local
			// component changed on the entities returned by the query.
			if (ctx.resetTraversalCaches) {
				world.m_targetsTravCache = {};
				world.m_srcBfsTravCache = {};
				world.m_depthOrderCache = {};
				world.m_sourcesAllCache = {};
				world.m_targetsAllCache = {};
				world.m_entityToAsTargetsTravCache = {};
				world.m_entityToAsRelationsTravCache = {};
			}

			if (ctx.targeted)
				normalize_targets(ctx.targets);

			// As with the before snapshot, equivalent observer queries share one result.
			cnt::darray<MatchCacheEntry> matchesAfterCache;
			cnt::darray<Entity> delta;

			for (auto& snapshot: ctx.observers) {
				auto* pObs = world.m_observers.data_try(snapshot.observer);
				if (pObs == nullptr || !world.valid(snapshot.observer))
					continue;

				// Some removal paths delete the target before this function runs. Their last
				// valid matches were captured in the before snapshot and are the event targets.
				if (ctx.targetsRemovedAfterPrepare && ctx.event == ObserverEvent::OnDel) {
					GAIA_ASSERT(snapshot.matchesBeforeIdx < ctx.matchesBeforeCache.size());
					const auto& matchesBefore = ctx.matchesBeforeCache[snapshot.matchesBeforeIdx].matches;
					SharedDispatch::execute_targets(world, *pObs, EntitySpan{matchesBefore});
					continue;
				}

				auto afterCacheIdx = find_match_cache_entry(matchesAfterCache, *pObs);
				if (afterCacheIdx == -1) {
					matchesAfterCache.push_back({});
					auto& entry = matchesAfterCache.back();
					entry.pObsRepresentative = pObs;
					entry.pQueryInfoRepresentative = &pObs->query.fetch();
					entry.queryHash = query_hash(*pObs);
					if (ctx.targeted)
						collect_query_target_matches(
								world, *pObs, EntitySpan{ctx.targets.data(), ctx.targets.size()}, entry.matches);
					else
						collect_query_matches(world, *pObs, entry.matches);
					afterCacheIdx = (int32_t)matchesAfterCache.size() - 1;
				}

				const auto& matchesAfter = matchesAfterCache[(uint32_t)afterCacheIdx].matches;

				// Newly created entities have no meaningful before result. Every matching
				// entity in the after snapshot is therefore an added match.
				if (ctx.targetsAddedAfterPrepare && ctx.event == ObserverEvent::OnAdd) {
					SharedDispatch::execute_targets(world, *pObs, EntitySpan{matchesAfter});
					continue;
				}

				// Both lists are sorted. Walk them together to collect only entities that
				// entered the query for OnAdd or left it for OnDel.
				GAIA_ASSERT(snapshot.matchesBeforeIdx < ctx.matchesBeforeCache.size());
				const auto& before = ctx.matchesBeforeCache[snapshot.matchesBeforeIdx].matches;
				delta.clear();
				uint32_t beforeIdx = 0;
				uint32_t afterMatchIdx = 0;
				while (beforeIdx < before.size() || afterMatchIdx < matchesAfter.size()) {
					if (beforeIdx == before.size()) {
						if (ctx.event == ObserverEvent::OnAdd)
							delta.push_back(matchesAfter[afterMatchIdx]);
						++afterMatchIdx;
						continue;
					}

					if (afterMatchIdx == matchesAfter.size()) {
						if (ctx.event == ObserverEvent::OnDel)
							delta.push_back(before[beforeIdx]);
						++beforeIdx;
						continue;
					}

					const auto beforeEntity = before[beforeIdx];
					const auto afterEntity = matchesAfter[afterMatchIdx];
					if (beforeEntity == afterEntity) {
						++beforeIdx;
						++afterMatchIdx;
						continue;
					}

					if (beforeEntity < afterEntity) {
						if (ctx.event == ObserverEvent::OnDel)
							delta.push_back(beforeEntity);
						++beforeIdx;
					} else {
						if (ctx.event == ObserverEvent::OnAdd)
							delta.push_back(afterEntity);
						++afterMatchIdx;
					}
				}

				SharedDispatch::execute_targets(world, *pObs, EntitySpan{delta.data(), delta.size()});
			}
		}

		inline void ObserverRegistry::DirectDispatcher::on_add(
				ObserverRegistry& registry, World& world, const Archetype& archetype, EntitySpan entsAdded,
				EntitySpan targets) {
			if GAIA_UNLIKELY (world.tearing_down())
				return;

			// Most mutations have no observers. Archetype and registry flags make that
			// common path return without building a candidate list.
			if (!archetype.has_observed_terms() && registry.m_observer_map_add.empty() &&
					registry.m_observer_map_add_is.empty())
				return;

			if (!archetype.has_observed_terms() && !SharedDispatch::has_terms(registry.m_observer_map_add, entsAdded) &&
					!SharedDispatch::has_semantic_is_terms(world, registry.m_observer_map_add_is, entsAdded) &&
					!SharedDispatch::has_inherited_terms(world, registry.m_observer_map_add, entsAdded))
				return;

			// Exact terms, semantic Is targets, and inherited terms can all lead to the
			// same observer. The match stamp keeps the candidate list unique.
			const bool archetypeIsPrefab = archetype.has(Prefab);
			CandidateScope candidateScope(registry);
			auto& relevantObservers = candidateScope.candidates();
			const auto matchStamp = ++registry.m_current_match_stamp;
			for (auto comp: entsAdded) {
				SharedDispatch::collect_for_event_term(
						registry, world, registry.m_observer_map_add, comp, matchStamp, relevantObservers);
				if (!is_semantic_is_term(comp))
					continue;

				const auto target = world.get(comp.gen());
				if (!world.valid(target))
					continue;

				SharedDispatch::collect_for_is_target(
						registry, world, registry.m_observer_map_add_is, target, matchStamp, relevantObservers);
				for (auto inheritedTarget: world.as_targets_trav_cache(target))
					SharedDispatch::collect_for_is_target(
							registry, world, registry.m_observer_map_add_is, inheritedTarget, matchStamp, relevantObservers);
				SharedDispatch::collect_for_inherited_terms(
						registry, world, registry.m_observer_map_add, target, matchStamp, relevantObservers);
			}

			// The index only identifies possible observers. The plan and query decide
			// whether this archetype and these entities are actual matches.
			for (auto* pObs: relevantObservers) {
				if (pObs == nullptr)
					continue;
				const auto observer = pObs->entity;
				if (!world.valid(observer) || !world.enabled(world.fetch(observer)))
					continue;

				auto& obs = *pObs;
				if (!obs.plan.uses_direct_dispatch())
					continue;
				QueryInfo* pQueryInfo = nullptr;
				if (archetypeIsPrefab) {
					pQueryInfo = &obs.query.fetch();
					if (!pQueryInfo->matches_prefab_entities())
						continue;
				}

				if (SharedDispatch::matches_direct_targets(obs, archetype, targets, pQueryInfo))
					SharedDispatch::execute_targets(world, obs, targets);
			}
		}

		inline void ObserverRegistry::DirectDispatcher::on_del(
				ObserverRegistry& registry, World& world, const Archetype& archetype, EntitySpan entsRemoved,
				EntitySpan targets) {
			if GAIA_UNLIKELY (world.tearing_down())
				return;

			if (!archetype.has_observed_terms() && registry.m_observer_map_del.empty() &&
					registry.m_observer_map_del_is.empty())
				return;

			// OnDel uses the old archetype, so direct negative plans are known to have
			// stopped matching even though their normal positive check returns false.
			const bool archetypeIsPrefab = archetype.has(Prefab);
			if (!archetype.has_observed_terms() && !SharedDispatch::has_terms(registry.m_observer_map_del, entsRemoved) &&
					!SharedDispatch::has_semantic_is_terms(world, registry.m_observer_map_del_is, entsRemoved) &&
					!SharedDispatch::has_inherited_terms(world, registry.m_observer_map_del, entsRemoved))
				return;

			CandidateScope candidateScope(registry);
			auto& relevantObservers = candidateScope.candidates();
			const auto matchStamp = ++registry.m_current_match_stamp;
			for (auto comp: entsRemoved) {
				SharedDispatch::collect_for_event_term(
						registry, world, registry.m_observer_map_del, comp, matchStamp, relevantObservers);
				if (!is_semantic_is_term(comp))
					continue;

				const auto target = world.get(comp.gen());
				if (!world.valid(target))
					continue;

				SharedDispatch::collect_for_is_target(
						registry, world, registry.m_observer_map_del_is, target, matchStamp, relevantObservers);
				for (auto inheritedTarget: world.as_targets_trav_cache(target))
					SharedDispatch::collect_for_is_target(
							registry, world, registry.m_observer_map_del_is, inheritedTarget, matchStamp, relevantObservers);
				SharedDispatch::collect_for_inherited_terms(
						registry, world, registry.m_observer_map_del, target, matchStamp, relevantObservers);
			}

			for (auto* pObs: relevantObservers) {
				if (pObs == nullptr)
					continue;
				const auto observer = pObs->entity;
				if (!world.valid(observer) || !world.enabled(world.fetch(observer)))
					continue;

				auto& obs = *pObs;
				if (!obs.plan.uses_direct_dispatch())
					continue;
				QueryInfo* pQueryInfo = nullptr;
				if (archetypeIsPrefab) {
					pQueryInfo = &obs.query.fetch();
					if (!pQueryInfo->matches_prefab_entities())
						continue;
				}

				bool matches = SharedDispatch::matches_direct_targets(obs, archetype, targets, pQueryInfo);
				if (obs.plan.exec_kind() == ObserverPlan::ExecKind::DirectFast && obs.plan.is_fast_negative())
					matches = true;

				if (matches)
					SharedDispatch::execute_targets(world, obs, targets);
			}
		}

		inline void ObserverRegistry::DirectDispatcher::on_set(
				ObserverRegistry& registry, World& world, Entity term, EntitySpan targets) {
			if GAIA_UNLIKELY (world.tearing_down())
				return;
			if (targets.empty())
				return;

			// A concrete pair can satisfy observers registered for the exact pair, either
			// wildcard endpoint, or both wildcard endpoints.
			CandidateScope candidateScope(registry);
			auto& relevantObservers = candidateScope.candidates();
			const auto matchStamp = ++registry.m_current_match_stamp;
			SharedDispatch::collect_from_map<false>(
					registry, world, registry.m_observer_map_set, term, matchStamp, relevantObservers);
			if (term.pair()) {
				Entity rel, tgt;
				pair_endpoint_entities(term, rel, tgt);
				SharedDispatch::collect_from_map<false>(
						registry, world, registry.m_observer_map_set, Pair(rel, All), matchStamp, relevantObservers);
				SharedDispatch::collect_from_map<false>(
						registry, world, registry.m_observer_map_set, Pair(All, tgt), matchStamp, relevantObservers);
				SharedDispatch::collect_from_map<false>(
						registry, world, registry.m_observer_map_set, Pair(All, All), matchStamp, relevantObservers);
			}
			if (relevantObservers.empty())
				return;

			// OnSet is value based, so every target must still satisfy the complete query.
			for (auto* pObs: relevantObservers) {
				if (pObs == nullptr)
					continue;
				const auto observer = pObs->entity;
				for (auto entity: targets) {
					if (!world.valid(observer) || !world.enabled(world.fetch(observer)))
						break;

					auto& obs = *pObs;
					if (!world.valid(entity))
						continue;

					auto& queryInfo = obs.query.fetch();
					const auto& ec = world.fetch(entity);
					if (ec.pArchetype == nullptr)
						continue;
					if (ec.pArchetype->has(Prefab) && !queryInfo.matches_prefab_entities())
						continue;
					if (!obs.query.matches_any(queryInfo, *ec.pArchetype, EntitySpan{&entity, 1}))
						continue;

					SharedDispatch::execute_targets(world, obs, EntitySpan{&entity, 1});
				}
			}
		}

		template <bool DiffOnly, typename TObserverMap, typename TObserverList>
		void ObserverRegistry::SharedDispatch::collect_from_map(
				ObserverRegistry& registry, World& world, const TObserverMap& map, Entity term, uint64_t matchStamp,
				TObserverList& out) {
			const auto it = map.find(EntityLookupKey(term));
			if (it == map.end())
				return;

			for (auto observer: it->second) {
				auto* pObs = registry.data_try(observer);
				GAIA_ASSERT(pObs != nullptr);
				if (pObs == nullptr)
					continue;
				// One mutation can find the same observer through several changed terms.
				if (pObs->lastMatchStamp == matchStamp)
					continue;

				const auto& ec = world.fetch(observer);
				if (!world.enabled(ec))
					continue;

				if constexpr (DiffOnly) {
					if (!pObs->plan.uses_diff_dispatch())
						continue;
				}

				pObs->lastMatchStamp = matchStamp;
				out.push_back(pObs);
			}
		}

		template <typename TObserverList>
		inline void ObserverRegistry::SharedDispatch::collect_diff_from_list(
				ObserverRegistry& registry, World& world, const cnt::darray<Entity>& observers, uint64_t matchStamp,
				TObserverList& out) {
			for (auto observer: observers) {
				auto* pObs = registry.data_try(observer);
				GAIA_ASSERT(pObs != nullptr);
				if (pObs == nullptr || !pObs->plan.uses_diff_dispatch())
					continue;
				if (pObs->lastMatchStamp == matchStamp)
					continue;

				const auto& ec = world.fetch(observer);
				if (!world.enabled(ec))
					continue;

				pObs->lastMatchStamp = matchStamp;
				out.push_back(pObs);
			}
		}

		template <typename TObserverMap>
		bool ObserverRegistry::SharedDispatch::has_pair_relations(World& world, const TObserverMap& map, EntitySpan terms) {
			for (auto term: terms) {
				if (!term.pair())
					continue;

				const auto relation = entity_from_id(world, term.id());
				if (!world.valid(relation))
					continue;

				const auto it = map.find(EntityLookupKey(relation));
				if (it != map.end() && !it->second.empty())
					return true;
			}

			return false;
		}

		template <typename TObserverMap, typename TObserverList>
		void ObserverRegistry::SharedDispatch::collect_for_event_term(
				ObserverRegistry& registry, World& world, const TObserverMap& map, Entity term, uint64_t matchStamp,
				TObserverList& out) {
			if (!world.valid(term))
				return;

			if (!is_semantic_is_term(term)) {
				if ((world.fetch(term).flags & EntityContainerFlags::IsObserved) == 0)
					return;
			}

			collect_from_map<false>(registry, world, map, term, matchStamp, out);
		}

		template <typename TObserverMap, typename TObserverList>
		void ObserverRegistry::SharedDispatch::collect_for_is_target(
				ObserverRegistry& registry, World& world, const TObserverMap& map, Entity target, uint64_t matchStamp,
				TObserverList& out) {
			collect_from_map<false>(registry, world, map, target, matchStamp, out);
		}

		template <typename Func>
		void ObserverRegistry::SharedDispatch::for_each_inherited_term(World& world, Entity baseEntity, Func&& func) {
			// Only plain component terms can be inherited through OnInstantiate.
			auto collectTerms = [&](Entity entity) {
				if (!world.valid(entity))
					return;

				const auto& ec = world.fetch(entity);
				if (ec.pArchetype == nullptr)
					return;

				for (const auto id: ec.pArchetype->ids_view()) {
					if (id.pair() || is_wildcard(id) || !world.valid(id))
						continue;
					if (world.target(id, OnInstantiate) != Inherit)
						continue;

					func(id);
				}
			};

			collectTerms(baseEntity);
			for (const auto inheritedBase: world.as_targets_trav_cache(baseEntity))
				collectTerms(inheritedBase);
		}

		template <typename TObserverMap>
		bool
		ObserverRegistry::SharedDispatch::has_semantic_is_terms(World& world, const TObserverMap& map, EntitySpan terms) {
			for (auto term: terms) {
				if (!is_semantic_is_term(term))
					continue;

				const auto target = world.get(term.gen());
				if (!world.valid(target))
					continue;

				if (map.find(EntityLookupKey(target)) != map.end())
					return true;

				for (auto inheritedTarget: world.as_targets_trav_cache(target)) {
					if (map.find(EntityLookupKey(inheritedTarget)) != map.end())
						return true;
				}
			}

			return false;
		}

		template <typename TObserverMap>
		bool
		ObserverRegistry::SharedDispatch::has_inherited_terms(World& world, const TObserverMap& map, EntitySpan terms) {
			for (auto term: terms) {
				if (!is_semantic_is_term(term))
					continue;

				const auto target = world.get(term.gen());
				if (!world.valid(target))
					continue;

				bool found = false;
				for_each_inherited_term(world, target, [&](Entity inheritedId) {
					if (found)
						return;
					const auto it = map.find(EntityLookupKey(inheritedId));
					found = it != map.end() && !it->second.empty();
				});

				if (found)
					return true;
			}

			return false;
		}

		template <typename TObserverMap, typename TObserverList>
		void ObserverRegistry::SharedDispatch::collect_for_inherited_terms(
				ObserverRegistry& registry, World& world, const TObserverMap& map, Entity baseEntity, uint64_t matchStamp,
				TObserverList& out) {
			for_each_inherited_term(world, baseEntity, [&](Entity inheritedId) {
				collect_for_event_term(registry, world, map, inheritedId, matchStamp, out);
			});
		}

		inline void
		ObserverRegistry::SharedDispatch::execute_targets(World& world, ObserverRuntimeData& obs, EntitySpan targets) {
			if (targets.empty())
				return;

			Iter it;
			it.set_world(&world);
			it.set_group_id(0);
			it.set_comp_indices(0);
			obs.exec(it, targets);
		}

		inline bool ObserverRegistry::SharedDispatch::matches_direct_targets(
				ObserverRuntimeData& obs, const Archetype& archetype, EntitySpan targets, QueryInfo* pQueryInfo) {
			switch (obs.plan.exec_kind()) {
				case ObserverPlan::ExecKind::DirectFast:
					if (obs.plan.is_fast_positive())
						return true;
					if (obs.plan.is_fast_negative())
						return false;
					break;
				case ObserverPlan::ExecKind::DirectQuery:
					break;
				case ObserverPlan::ExecKind::DiffLocal:
				case ObserverPlan::ExecKind::DiffPropagated:
				case ObserverPlan::ExecKind::DiffFallback:
					return false;
			}

			auto& queryInfo = pQueryInfo != nullptr ? *pQueryInfo : obs.query.fetch();
			return obs.query.matches_any(queryInfo, archetype, targets);
		}

		inline bool ObserverRegistry::can_mark_term_observed(World& world, Entity term) {
			if (!term.pair())
				return world.valid(term);

			// A wildcard pair is a search pattern, not a concrete record owned by the world.
			if (is_wildcard(term))
				return false;

			const auto* pPair = world.m_recs.pair_record_find(term);
			return pPair != nullptr && world.valid(*pPair, term);
		}

		inline bool ObserverRegistry::is_semantic_is_term(Entity term, QueryMatchKind matchKind) {
			return matchKind != QueryMatchKind::Direct && term.pair() && term.id() == Is.id() && !is_wildcard(term.gen());
		}

		inline void ObserverRegistry::mark_term_observed(World& world, Entity term, bool observed) {
			auto& ec = world.fetch(term);
			const bool wasObserved = (ec.flags & EntityContainerFlags::IsObserved) != 0;
			if (wasObserved == observed)
				return;

			if (observed)
				ec.flags |= EntityContainerFlags::IsObserved;
			else
				ec.flags &= ~EntityContainerFlags::IsObserved;

			// Archetypes keep a counter so mutation dispatch can reject unobserved changes
			// without searching the registry. Keep every archetype containing this term in sync.
			const auto it = world.m_entityToArchetypeMap.find(EntityLookupKey(term));
			if (it == world.m_entityToArchetypeMap.end())
				return;

			for (const auto& record: it->second) {
				auto* pArchetype = record.pArchetype;
				if (observed)
					pArchetype->observed_terms_inc();
				else
					pArchetype->observed_terms_dec();
			}
		}

		inline void ObserverRegistry::collect_traversal_descendants(
				World& world, Entity relation, Entity root, QueryTravKind travKind, uint8_t travDepth, uint64_t visitStamp,
				cnt::darray<Entity>& outTargets) {
			cnt::set<EntityLookupKey> visitedPairs;
			auto try_mark_visited = [&](Entity entity) {
				if (entity.pair())
					return visitedPairs.insert(EntityLookupKey(entity)).second;
				return world.try_mark_entity_visited(entity, visitStamp);
			};

			// Self is a query choice independent of walking the relation.
			if (query_trav_has(travKind, QueryTravKind::Self)) {
				if (try_mark_visited(root))
					outTargets.push_back(root);
			}

			if (!query_trav_has(travKind, QueryTravKind::Up))
				return;

			// Use the world's cached breadth-first traversal for the common unlimited path.
			if (travDepth == QueryTermOptions::TravDepthUnlimited && !query_trav_has(travKind, QueryTravKind::Down)) {
				world.sources_bfs(relation, root, [&](Entity source) {
					if (try_mark_visited(source))
						outTargets.push_back(source);
				});
				return;
			}

			if (travDepth == 1) {
				world.sources(relation, root, [&](Entity source) {
					if (try_mark_visited(source))
						outTargets.push_back(source);
				});
				return;
			}

			// Limited-depth traversal keeps the current depth beside each queued entity.
			cnt::darray_ext<Entity, 32> queue;
			cnt::darray_ext<uint8_t, 32> depths;
			queue.push_back(root);
			depths.push_back(0);

			for (uint32_t i = 0; i < queue.size(); ++i) {
				const auto curr = queue[i];
				const auto currDepth = depths[i];
				if (travDepth != QueryTermOptions::TravDepthUnlimited && currDepth >= travDepth)
					continue;

				world.sources(relation, curr, [&](Entity source) {
					if (!try_mark_visited(source))
						return;

					outTargets.push_back(source);
					queue.push_back(source);
					depths.push_back((uint8_t)(currDepth + 1));
				});
			}
		}

		inline ObserverRegistry::PropagatedTargetCacheEntry& ObserverRegistry::ensure_propagated_targets_cached(
				ObserverRegistry& registry, World& world, const ObserverRuntimeData& obs, Entity changedSource) {
			const PropagatedTargetCacheKey key{
					obs.plan.diff.bindingRelation, obs.plan.diff.traversalRelation, changedSource, obs.plan.diff.travKind,
					obs.plan.diff.travDepth};

			auto& entry = registry.m_propagated_target_cache[key];
			const auto bindingRelationVersion = world.rel_version(obs.plan.diff.bindingRelation);
			const auto traversalRelationVersion = world.rel_version(obs.plan.diff.traversalRelation);
			// Relation versions let this cache survive unrelated world mutations.
			const bool cacheValid = entry.bindingRelationVersion == bindingRelationVersion &&
															entry.traversalRelationVersion == traversalRelationVersion;

			if (!cacheValid) {
				entry.bindingRelationVersion = bindingRelationVersion;
				entry.traversalRelationVersion = traversalRelationVersion;
				entry.targets.clear();

				// First find the binding targets reachable from the changed source. Then find
				// entities whose binding relation points at any of those targets.
				const auto visitStamp = world.next_entity_visit_stamp();
				cnt::darray<Entity> bindingTargets;
				collect_traversal_descendants(
						world, obs.plan.diff.traversalRelation, changedSource, obs.plan.diff.travKind, obs.plan.diff.travDepth,
						visitStamp, bindingTargets);
				for (auto bindingTarget: bindingTargets) {
					world.sources(obs.plan.diff.bindingRelation, bindingTarget, [&](Entity source) {
						entry.targets.push_back(source);
					});
				}

				DiffDispatcher::normalize_targets(entry.targets);
			}

			return entry;
		}

		inline void ObserverRegistry::collect_propagated_targets_cached(
				ObserverRegistry& registry, World& world, const ObserverRuntimeData& obs, Entity changedSource,
				uint64_t visitStamp, cnt::set<EntityLookupKey>& visitedPairs, cnt::darray<Entity>& outTargets) {
			auto& entry = ensure_propagated_targets_cached(registry, world, obs, changedSource);

			for (auto source: entry.targets) {
				const bool isNew = source.pair() ? visitedPairs.insert(EntityLookupKey(source)).second
																				 : world.try_mark_entity_visited(source, visitStamp);
				if (isNew)
					outTargets.push_back(source);
			}
		}

		inline void ObserverRegistry::add_propagated_targets_cached(
				ObserverRegistry& registry, World& world, const ObserverRuntimeData& obs, Entity changedSource,
				cnt::darray<Entity>& outTargets) {
			auto& entry = ensure_propagated_targets_cached(registry, world, obs, changedSource);

			for (auto source: entry.targets)
				outTargets.push_back(source);
		}

		inline bool ObserverRegistry::collect_source_traversal_diff_targets(
				ObserverRegistry& registry, World& world, ObserverRuntimeData& obs, EntitySpan changedTerms,
				EntitySpan changedSources, cnt::darray<Entity>& outTargets) {
			if (changedSources.empty())
				return false;
			if (!obs.plan.uses_propagated_diff_targets())
				return false;
			if (obs.plan.diff.bindingRelation == EntityBad || obs.plan.diff.traversalRelation == EntityBad ||
					obs.plan.diff.traversalTriggerTermCount == 0)
				return false;

			// A source change matters only when it touches the source entity itself, the
			// traversal relation, or a term recorded as a traversal trigger by the plan.
			bool termTriggered = false;
			for (auto changedTerm: changedTerms) {
				for (auto changedSource: changedSources) {
					if (!changedTerm.pair() && changedTerm == changedSource) {
						termTriggered = true;
						break;
					}
				}
				if (termTriggered)
					break;

				if (changedTerm.pair() && entity_from_id(world, changedTerm.id()) == obs.plan.diff.traversalRelation) {
					termTriggered = true;
					break;
				}

				GAIA_FOR(obs.plan.diff.traversalTriggerTermCount) {
					if (obs.plan.diff.traversalTriggerTerms[i] == changedTerm) {
						termTriggered = true;
						break;
					}
				}

				if (termTriggered)
					break;
			}
			if (!termTriggered)
				return false;

			// One source needs no cross-source duplicate tracking. Multiple sources share
			// a visit stamp so the same bound entity is returned only once.
			if (changedSources.size() == 1) {
				add_propagated_targets_cached(registry, world, obs, changedSources[0], outTargets);
				return true;
			}

			const auto visitStamp = world.next_entity_visit_stamp();
			cnt::set<EntityLookupKey> visitedPairs;
			for (auto changedSource: changedSources)
				collect_propagated_targets_cached(registry, world, obs, changedSource, visitStamp, visitedPairs, outTargets);

			return true;
		}

		inline bool ObserverRegistry::collect_diff_targets_for_observer(
				ObserverRegistry& registry, World& world, ObserverRuntimeData& obs, EntitySpan changedTerms,
				EntitySpan changedTargets, cnt::darray<Entity>& outTargets) {
			switch (obs.plan.exec_kind()) {
				case ObserverPlan::ExecKind::DiffLocal:
					DiffDispatcher::add_valid_targets(world, outTargets, changedTargets);
					return true;
				case ObserverPlan::ExecKind::DiffPropagated:
					return collect_source_traversal_diff_targets(registry, world, obs, changedTerms, changedTargets, outTargets);
				case ObserverPlan::ExecKind::DiffFallback:
					return false;
				case ObserverPlan::ExecKind::DirectQuery:
				case ObserverPlan::ExecKind::DirectFast:
					return false;
			}

			return false;
		}

		inline bool ObserverRegistry::observer_uses_changed_traversal_relation(
				World& world, const ObserverRuntimeData& obs, EntitySpan changedTerms) {
			if (obs.plan.diff.traversalRelationCount == 0 || changedTerms.empty())
				return false;

			for (auto changedTerm: changedTerms) {
				if (!changedTerm.pair())
					continue;

				const auto relation = entity_from_id(world, changedTerm.id());
				if (!world.valid(relation))
					continue;

				GAIA_FOR(obs.plan.diff.traversalRelationCount) {
					if (obs.plan.diff.traversalRelations[i] == relation)
						return true;
				}
			}

			return false;
		}

		inline void ObserverRegistry::add_diff_observer_term(
				World& world, Entity observer, Entity term, const QueryTermOptions& options) {
			GAIA_ASSERT(world.valid(observer));

			const auto& ec = world.fetch(observer);
			const auto compIdx = ec.pChunk->comp_idx(Observer);
			const auto& obs = *reinterpret_cast<const Observer_*>(ec.pChunk->comp_ptr(compIdx, ec.row));

			switch (obs.event) {
				case ObserverEvent::OnAdd:
				case ObserverEvent::OnDel:
					break;
				case ObserverEvent::OnSet:
					return;
			}

			// Every diff observer remains available through the complete list for mutation
			// paths that cannot provide precise changed terms.
			auto& index = diff_index(obs.event);
			add_observer_to_list(index.all, observer);

			bool registered = false;

			// Add every index key that can identify this dependency. Registration is
			// unique because one term may describe the same dependency in several ways.
			if (term != EntityBad && term != All) {
				add_observer_to_map_unique(index.direct, term, observer);
				registered = true;
			}

			if (term != EntityBad && term != All && options.entSrc != EntityBad) {
				add_observer_to_map_unique(index.sourceTerm, term, observer);
				registered = true;
			}

			if (options.entTrav != EntityBad) {
				if (term != EntityBad && term != All)
					add_observer_to_map_unique(index.sourceTerm, term, observer);
				add_observer_to_map_unique(index.traversalRelation, options.entTrav, observer);
				registered = true;
			}

			if (term.pair()) {
				const bool relDynamic = is_dynamic_pair_endpoint(term.id());
				const bool tgtDynamic = is_dynamic_pair_endpoint(term.gen());

				if (relDynamic && !tgtDynamic) {
					add_observer_to_map_unique(index.pairTarget, world.get(term.gen()), observer);
					registered = true;
				}

				if (tgtDynamic && !relDynamic) {
					add_observer_to_map_unique(index.pairRelation, entity_from_id(world, term.id()), observer);
					registered = true;
				}
			}

			// Fully dynamic terms cannot be found from a concrete changed term alone.
			if (!registered || is_observer_term_globally_dynamic(term))
				add_observer_to_list(index.global, observer);
		}

		inline ObserverRegistry::DiffDispatchCtx
		ObserverRegistry::prepare_diff(World& world, ObserverEvent event, EntitySpan terms, EntitySpan targetEntities) {
			if GAIA_UNLIKELY (world.tearing_down())
				return {};
			return DiffDispatcher::prepare(*this, world, event, terms, targetEntities);
		}

		inline ObserverRegistry::DiffDispatchCtx ObserverRegistry::prepare_diff_add_new(World& world, EntitySpan terms) {
			if GAIA_UNLIKELY (world.tearing_down())
				return {};
			return DiffDispatcher::prepare_add_new(*this, world, terms);
		}

		inline void ObserverRegistry::add_diff_targets(World& world, DiffDispatchCtx& ctx, EntitySpan targets) {
			DiffDispatcher::add_targets(world, ctx, targets);
		}

		inline void ObserverRegistry::finish_diff(World& world, DiffDispatchCtx&& ctx) {
			if GAIA_UNLIKELY (world.tearing_down())
				return;
			DispatchScope dispatchScope(*this);
			DiffDispatcher::finish(world, GAIA_MOV(ctx));
		}

		inline void ObserverRegistry::try_mark_term_observed(World& world, Entity term) {
			if (!has_on_add_observers() && !has_on_del_observers() && !m_hasOnSetObservers)
				return;

			if (!can_mark_term_observed(world, term))
				return;
			if (!has_observers_for_term(term))
				return;
			if ((world.fetch(term).flags & EntityContainerFlags::IsObserved) != 0)
				return;

			mark_term_observed(world, term, true);
		}

		inline void ObserverRegistry::add(World& world, Entity term, Entity observer, QueryMatchKind matchKind) {
			GAIA_ASSERT(!observer.pair());
			GAIA_ASSERT(world.valid(observer));
			// A concrete pair is valid only after the world has created its pair record.
			// Observers may register pair terms before that happens, so only plain terms
			// must already be valid here.
			GAIA_ASSERT(term.pair() || world.valid(term));

			// Mark the term only when the first observer is attached. Archetypes use this
			// state to skip registry work for terms that nobody observes.
			const auto wasObserved = has_observers_for_term(term);
			const auto canMarkObserved = can_mark_term_observed(world, term);
			const auto& ec = world.fetch(observer);
			const auto compIdx = ec.pChunk->comp_idx(Observer);
			const auto& obs = *reinterpret_cast<const Observer_*>(ec.pChunk->comp_ptr(compIdx, ec.row));
			switch (obs.event) {
				case ObserverEvent::OnAdd:
					add_observer_to_map(m_observer_map_add, term, observer);
					if (is_semantic_is_term(term, matchKind))
						add_observer_to_map(m_observer_map_add_is, world.get(term.gen()), observer);
					break;
				case ObserverEvent::OnDel:
					add_observer_to_map(m_observer_map_del, term, observer);
					if (is_semantic_is_term(term, matchKind))
						add_observer_to_map(m_observer_map_del_is, world.get(term.gen()), observer);
					break;
				case ObserverEvent::OnSet:
					add_observer_to_map(m_observer_map_set, term, observer);
					m_hasOnSetObservers = true;
					break;
			}
			if (!wasObserved && canMarkObserved)
				mark_term_observed(world, term, true);
		}

		inline void ObserverRegistry::del(World& world, Entity term) {
			GAIA_ASSERT(world.valid(term));

			// First remove entries keyed directly by this entity. When term is an observer
			// entity, its runtime data tells us whether the broader indexes must be scanned.
			const auto termKey = EntityLookupKey(term);
			Entity erasedData = EntityBad;
			const auto itData = m_observer_data.find(termKey);
			if (itData != m_observer_data.end()) {
				erasedData = itData->second;
				m_observer_data.erase(itData);
			}
			const auto erasedOnAdd = m_observer_map_add.erase(termKey);
			const auto erasedOnDel = m_observer_map_del.erase(termKey);
			const auto erasedOnSet = m_observer_map_set.erase(termKey);
			if (erasedOnSet != 0)
				m_hasOnSetObservers = !m_observer_map_set.empty();
			if (is_semantic_is_term(term)) {
				const auto isKey = EntityLookupKey(world.get(term.gen()));
				m_observer_map_add_is.erase(isKey);
				m_observer_map_del_is.erase(isKey);
			}
			if ((erasedOnAdd != 0 || erasedOnDel != 0 || erasedOnSet != 0) && can_mark_term_observed(world, term))
				mark_term_observed(world, term, false);

			if (erasedData == EntityBad)
				return;

			// The observer may appear under several query dependencies. Remove every
			// occurrence and clear observed flags when the last registration disappears.
			auto remove_observer_from_map = [&](auto& map) {
				for (auto it = map.begin(); it != map.end();) {
					auto& observers = it->second;
					for (uint32_t i = 0; i < observers.size();) {
						if (observers[i] == term)
							core::swap_erase_unsafe(observers, i);
						else
							++i;
					}

					if (observers.empty()) {
						const auto mappedTerm = it->first.entity();
						auto itToErase = it++;
						map.erase(itToErase);

						if (can_mark_term_observed(world, mappedTerm) && !has_observers_for_term(mappedTerm))
							mark_term_observed(world, mappedTerm, false);
					} else
						++it;
				}
			};
			remove_observer_from_map(m_observer_map_add);
			remove_observer_from_map(m_observer_map_del);
			remove_observer_from_map(m_observer_map_set);
			m_hasOnSetObservers = !m_observer_map_set.empty();
			remove_observer_from_map(m_observer_map_add_is);
			remove_observer_from_map(m_observer_map_del_is);
			auto remove_observer_from_diff_index = [&](auto& index) {
				remove_observer_from_map(index.direct);
				remove_observer_from_map(index.sourceTerm);
				remove_observer_from_map(index.traversalRelation);
				remove_observer_from_map(index.pairRelation);
				remove_observer_from_map(index.pairTarget);
				remove_observer_from_list(index.all, term);
				remove_observer_from_list(index.global, term);
			};
			remove_observer_from_diff_index(m_diff_index_add);
			remove_observer_from_diff_index(m_diff_index_del);
			retire_observer_data(erasedData);
		}

		inline void
		ObserverRegistry::on_add(World& world, const Archetype& archetype, EntitySpan entsAdded, EntitySpan targets) {
			DispatchScope dispatchScope(*this);
			DirectDispatcher::on_add(*this, world, archetype, entsAdded, targets);
		}

		inline void
		ObserverRegistry::on_del(World& world, const Archetype& archetype, EntitySpan entsRemoved, EntitySpan targets) {
			DispatchScope dispatchScope(*this);
			DirectDispatcher::on_del(*this, world, archetype, entsRemoved, targets);
		}

		inline void ObserverRegistry::on_set(World& world, Entity term, EntitySpan targets) {
			DispatchScope dispatchScope(*this);
			DirectDispatcher::on_set(*this, world, term, targets);
		}
	} // namespace ecs
} // namespace gaia
#endif
