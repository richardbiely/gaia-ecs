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

			obs.query.reset();
			obs.query.collect_entities_enabled(out);

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

			if (!hasEntityLifecycleTerm && !targetEntities.empty() && !terms.empty() &&
					!SharedDispatch::has_terms(index.sourceTerm, terms) &&
					!SharedDispatch::has_pair_relations(world, index.traversalRelation, terms)) {
				ctx.targeted = true;
				ctx.targets.reserve((uint32_t)targetEntities.size());
				add_valid_targets(world, ctx.targets, targetEntities);
				normalize_targets(ctx.targets);
			}

			registry.m_relevant_observers_tmp.clear();
			const auto matchStamp = ++registry.m_current_match_stamp;
			if (terms.empty()) {
				SharedDispatch::collect_diff_from_list(registry, world, index.all, matchStamp);
			} else {
				for (auto term: terms) {
					SharedDispatch::collect_from_map<true>(registry, world, index.direct, term, matchStamp);

					if (!term.pair())
						continue;

					if (!is_wildcard(term.id()) && world.valid_entity_id((EntityId)term.id())) {
						const auto relation = entity_from_id(world, term.id());
						if (world.valid(relation)) {
							SharedDispatch::collect_from_map<true>(registry, world, index.traversalRelation, relation, matchStamp);
							SharedDispatch::collect_from_map<true>(registry, world, index.pairRelation, relation, matchStamp);
						}
					}

					if (!is_wildcard(term.gen()) && world.valid_entity_id((EntityId)term.gen())) {
						const auto target = world.get(term.gen());
						if (world.valid(target))
							SharedDispatch::collect_from_map<true>(registry, world, index.pairTarget, target, matchStamp);
					}
				}
			}

			if (hasEntityLifecycleTerm)
				SharedDispatch::collect_diff_from_list(registry, world, index.all, matchStamp);
			if (!terms.empty() && !hasEntityLifecycleTerm)
				SharedDispatch::collect_diff_from_list(registry, world, index.global, matchStamp);

			if (!ctx.targeted && !targetEntities.empty() && !registry.m_relevant_observers_tmp.empty()) {
				cnt::darray<Entity> narrowedTargets;
				cnt::darray<TargetNarrowCacheEntry> narrowCache;
				bool canNarrow = true;

				for (auto* pObs: registry.m_relevant_observers_tmp) {
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

			if (registry.m_relevant_observers_tmp.empty())
				return ctx;

			ctx.active = true;
			for (auto* pObs: registry.m_relevant_observers_tmp) {
				ctx.observers.push_back({});
				auto& snapshot = ctx.observers.back();
				snapshot.pObs = pObs;
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

			if (terms.empty() || SharedDispatch::has_terms(index.sourceTerm, terms) ||
					SharedDispatch::has_pair_relations(world, index.traversalRelation, terms)) {
				return prepare(registry, world, ObserverEvent::OnAdd, terms);
			}

			registry.m_relevant_observers_tmp.clear();
			const auto matchStamp = ++registry.m_current_match_stamp;
			for (auto term: terms) {
				SharedDispatch::collect_from_map<true>(registry, world, index.direct, term, matchStamp);

				if (!term.pair())
					continue;

				if (!is_wildcard(term.id()) && world.valid_entity_id((EntityId)term.id())) {
					const auto relation = entity_from_id(world, term.id());
					if (world.valid(relation))
						SharedDispatch::collect_from_map<true>(registry, world, index.pairRelation, relation, matchStamp);
				}

				if (!is_wildcard(term.gen()) && world.valid_entity_id((EntityId)term.gen())) {
					const auto target = world.get(term.gen());
					if (world.valid(target))
						SharedDispatch::collect_from_map<true>(registry, world, index.pairTarget, target, matchStamp);
				}
			}
			SharedDispatch::collect_diff_from_list(registry, world, index.global, matchStamp);

			if (registry.m_relevant_observers_tmp.empty())
				return ctx;

			ctx.active = true;
			ctx.targeted = true;
			ctx.targetsAddedAfterPrepare = true;
			for (auto* pObs: registry.m_relevant_observers_tmp) {
				if (!ctx.resetTraversalCaches && observer_uses_changed_traversal_relation(world, *pObs, terms))
					ctx.resetTraversalCaches = true;
				ctx.observers.push_back({});
				ctx.observers.back().pObs = pObs;
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

			cnt::darray<MatchCacheEntry> matchesAfterCache;
			cnt::darray<Entity> delta;

			for (auto& snapshot: ctx.observers) {
				auto* pObs = snapshot.pObs;
				if (pObs == nullptr || !world.valid(pObs->entity))
					continue;

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

				if (ctx.targetsAddedAfterPrepare && ctx.event == ObserverEvent::OnAdd) {
					SharedDispatch::execute_targets(world, *pObs, EntitySpan{matchesAfter});
					continue;
				}

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

			if (!archetype.has_observed_terms() && registry.m_observer_map_add.empty() &&
					registry.m_observer_map_add_is.empty())
				return;

			if (!archetype.has_observed_terms() && !SharedDispatch::has_terms(registry.m_observer_map_add, entsAdded) &&
					!SharedDispatch::has_semantic_is_terms(world, registry.m_observer_map_add_is, entsAdded) &&
					!SharedDispatch::has_inherited_terms(world, registry.m_observer_map_add, entsAdded))
				return;

			const bool archetypeIsPrefab = archetype.has(Prefab);
			const auto matchStamp = ++registry.m_current_match_stamp;
			for (auto comp: entsAdded) {
				SharedDispatch::collect_for_event_term(registry, world, registry.m_observer_map_add, comp, matchStamp);
				if (!is_semantic_is_term(comp))
					continue;

				const auto target = world.get(comp.gen());
				if (!world.valid(target))
					continue;

				SharedDispatch::collect_for_is_target(registry, world, registry.m_observer_map_add_is, target, matchStamp);
				for (auto inheritedTarget: world.as_targets_trav_cache(target))
					SharedDispatch::collect_for_is_target(
							registry, world, registry.m_observer_map_add_is, inheritedTarget, matchStamp);
				SharedDispatch::collect_for_inherited_terms(registry, world, registry.m_observer_map_add, target, matchStamp);
			}

			for (auto* pObs: registry.m_relevant_observers_tmp) {
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

			registry.m_relevant_observers_tmp.clear();
		}

		inline void ObserverRegistry::DirectDispatcher::on_del(
				ObserverRegistry& registry, World& world, const Archetype& archetype, EntitySpan entsRemoved,
				EntitySpan targets) {
			if GAIA_UNLIKELY (world.tearing_down())
				return;

			if (!archetype.has_observed_terms() && registry.m_observer_map_del.empty() &&
					registry.m_observer_map_del_is.empty())
				return;

			const bool archetypeIsPrefab = archetype.has(Prefab);
			if (!archetype.has_observed_terms() && !SharedDispatch::has_terms(registry.m_observer_map_del, entsRemoved) &&
					!SharedDispatch::has_semantic_is_terms(world, registry.m_observer_map_del_is, entsRemoved) &&
					!SharedDispatch::has_inherited_terms(world, registry.m_observer_map_del, entsRemoved))
				return;

			const auto matchStamp = ++registry.m_current_match_stamp;
			for (auto comp: entsRemoved) {
				SharedDispatch::collect_for_event_term(registry, world, registry.m_observer_map_del, comp, matchStamp);
				if (!is_semantic_is_term(comp))
					continue;

				const auto target = world.get(comp.gen());
				if (!world.valid(target))
					continue;

				SharedDispatch::collect_for_is_target(registry, world, registry.m_observer_map_del_is, target, matchStamp);
				for (auto inheritedTarget: world.as_targets_trav_cache(target))
					SharedDispatch::collect_for_is_target(
							registry, world, registry.m_observer_map_del_is, inheritedTarget, matchStamp);
				SharedDispatch::collect_for_inherited_terms(registry, world, registry.m_observer_map_del, target, matchStamp);
			}

			for (auto* pObs: registry.m_relevant_observers_tmp) {
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

			registry.m_relevant_observers_tmp.clear();
		}

		inline void ObserverRegistry::DirectDispatcher::on_set(
				ObserverRegistry& registry, World& world, Entity term, EntitySpan targets) {
			if GAIA_UNLIKELY (world.tearing_down())
				return;
			if (targets.empty())
				return;

			registry.m_relevant_observers_tmp.clear();
			const auto matchStamp = ++registry.m_current_match_stamp;
			SharedDispatch::collect_from_map<false>(registry, world, registry.m_observer_map_set, term, matchStamp);
			if (term.pair()) {
				Entity rel, tgt;
				pair_endpoint_entities(term, rel, tgt);
				SharedDispatch::collect_from_map<false>(
						registry, world, registry.m_observer_map_set, Pair(rel, All), matchStamp);
				SharedDispatch::collect_from_map<false>(
						registry, world, registry.m_observer_map_set, Pair(All, tgt), matchStamp);
				SharedDispatch::collect_from_map<false>(
						registry, world, registry.m_observer_map_set, Pair(All, All), matchStamp);
			}
			if (registry.m_relevant_observers_tmp.empty())
				return;

			for (auto* pObs: registry.m_relevant_observers_tmp) {
				auto& obs = *pObs;
				for (auto entity: targets) {
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

			registry.m_relevant_observers_tmp.clear();
		}

		template <bool DiffOnly, typename TObserverMap>
		void ObserverRegistry::SharedDispatch::collect_from_map(
				ObserverRegistry& registry, World& world, const TObserverMap& map, Entity term, uint64_t matchStamp) {
			const auto it = map.find(EntityLookupKey(term));
			if (it == map.end())
				return;

			for (auto observer: it->second) {
				auto* pObs = registry.data_try(observer);
				GAIA_ASSERT(pObs != nullptr);
				if (pObs == nullptr)
					continue;
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
				registry.m_relevant_observers_tmp.push_back(pObs);
			}
		}

		inline void ObserverRegistry::SharedDispatch::collect_diff_from_list(
				ObserverRegistry& registry, World& world, const cnt::darray<Entity>& observers, uint64_t matchStamp) {
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
				registry.m_relevant_observers_tmp.push_back(pObs);
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

		template <typename TObserverMap>
		void ObserverRegistry::SharedDispatch::collect_for_event_term(
				ObserverRegistry& registry, World& world, const TObserverMap& map, Entity term, uint64_t matchStamp) {
			if (!world.valid(term))
				return;

			if (!is_semantic_is_term(term)) {
				if ((world.fetch(term).flags & EntityContainerFlags::IsObserved) == 0)
					return;
			}

			collect_from_map<false>(registry, world, map, term, matchStamp);
		}

		template <typename TObserverMap>
		void ObserverRegistry::SharedDispatch::collect_for_is_target(
				ObserverRegistry& registry, World& world, const TObserverMap& map, Entity target, uint64_t matchStamp) {
			collect_from_map<false>(registry, world, map, target, matchStamp);
		}

		template <typename Func>
		void ObserverRegistry::SharedDispatch::for_each_inherited_term(World& world, Entity baseEntity, Func&& func) {
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

		template <typename TObserverMap>
		void ObserverRegistry::SharedDispatch::collect_for_inherited_terms(
				ObserverRegistry& registry, World& world, const TObserverMap& map, Entity baseEntity, uint64_t matchStamp) {
			for_each_inherited_term(world, baseEntity, [&](Entity inheritedId) {
				collect_for_event_term(registry, world, map, inheritedId, matchStamp);
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

			if (query_trav_has(travKind, QueryTravKind::Self)) {
				if (try_mark_visited(root))
					outTargets.push_back(root);
			}

			if (!query_trav_has(travKind, QueryTravKind::Up))
				return;

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
			const bool cacheValid = entry.bindingRelationVersion == bindingRelationVersion &&
															entry.traversalRelationVersion == traversalRelationVersion;

			if (!cacheValid) {
				entry.bindingRelationVersion = bindingRelationVersion;
				entry.traversalRelationVersion = traversalRelationVersion;
				entry.targets.clear();

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

			auto& index = diff_index(obs.event);
			add_observer_to_list(index.all, observer);

			bool registered = false;

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
			// For a pair term, valid(pair) is true only if that exact pair already exists
			// in the world's pair records. Observers are allowed to register pair terms that
			// may appear later, so asserting just world.valid(term) for pairs when adding is wrong.
			GAIA_ASSERT(term.pair() || world.valid(term));

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

			const auto termKey = EntityLookupKey(term);
			const auto erasedData = m_observer_data.erase(termKey);
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

			if (erasedData == 0)
				return;

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
		}

		inline void
		ObserverRegistry::on_add(World& world, const Archetype& archetype, EntitySpan entsAdded, EntitySpan targets) {
			DirectDispatcher::on_add(*this, world, archetype, entsAdded, targets);
		}

		inline void
		ObserverRegistry::on_del(World& world, const Archetype& archetype, EntitySpan entsRemoved, EntitySpan targets) {
			DirectDispatcher::on_del(*this, world, archetype, entsRemoved, targets);
		}

		inline void ObserverRegistry::on_set(World& world, Entity term, EntitySpan targets) {
			DirectDispatcher::on_set(*this, world, term, targets);
		}
	} // namespace ecs
} // namespace gaia
#endif
