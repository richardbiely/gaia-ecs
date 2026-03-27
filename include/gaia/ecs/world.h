#pragma once
#include "gaia/config/config.h"

#include <cctype>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>

#include "gaia/cnt/darray.h"
#include "gaia/cnt/darray_ext.h"
#include "gaia/cnt/map.h"
#include "gaia/cnt/sarray_ext.h"
#include "gaia/cnt/set.h"
#include "gaia/config/profiler.h"
#include "gaia/core/hashing_policy.h"
#include "gaia/core/hashing_string.h"
#include "gaia/core/span.h"
#include "gaia/core/utility.h"
#include "gaia/ecs/api.h"
#include "gaia/ecs/archetype.h"
#include "gaia/ecs/archetype_common.h"
#include "gaia/ecs/archetype_graph.h"
#include "gaia/ecs/chunk.h"
#include "gaia/ecs/chunk_allocator.h"
#include "gaia/ecs/chunk_header.h"
#include "gaia/ecs/command_buffer_fwd.h"
#include "gaia/ecs/common.h"
#include "gaia/ecs/component.h"
#include "gaia/ecs/component_cache.h"
#include "gaia/ecs/component_cache_item.h"
#include "gaia/ecs/component_getter.h"
#include "gaia/ecs/component_setter.h"
#include "gaia/ecs/entity_container.h"
#include "gaia/ecs/id.h"
#include "gaia/ecs/observer.h"
#include "gaia/ecs/query.h"
#include "gaia/ecs/query_cache.h"
#include "gaia/ecs/query_common.h"
#include "gaia/ecs/query_info.h"
#include "gaia/mem/mem_alloc.h"
#include "gaia/ser/ser_binary.h"
#include "gaia/ser/ser_json.h"
#include "gaia/ser/ser_rt.h"
#include "gaia/util/logging.h"
#include "gaia/util/str.h"

namespace gaia {
	namespace ecs {
		template <typename T>
		struct SparseComponentRecord;
	}

	namespace cnt {
		template <typename T>
		struct to_sparse_id<ecs::SparseComponentRecord<T>> {
			static sparse_id get(const ecs::SparseComponentRecord<T>& item) noexcept {
				return (sparse_id)item.entity.id();
			}
		};
	} // namespace cnt

	namespace ecs {
#if GAIA_SYSTEMS_ENABLED
		class SystemBuilder;
#endif
#if GAIA_OBSERVERS_ENABLED
		class ObserverBuilder;
#endif
		class World;

		template <typename T>
		struct SparseComponentRecord {
			Entity entity;
			T value{};
		};

		class GAIA_API World final {
		public:
			//! Allows asserts on duplicate name/alias assignment
			inline static bool s_enableUniqueNameDuplicateAssert = true;

		private:
			friend CommandBufferST;
			friend CommandBufferMT;
			friend void lock(World&);
			friend void unlock(World&);
			friend QueryMatchScratch& query_match_scratch_acquire(World&);
			friend void query_match_scratch_release(World&, bool);
			friend uint32_t world_component_index_bucket_size(const World&, Entity);
			friend uint32_t world_component_index_comp_idx(const World&, const Archetype&, Entity);
			friend uint32_t world_component_index_match_count(const World&, const Archetype&, Entity);

			ser::bin_stream m_stream;
			ser::serializer m_serializer{};

			using TFunc_Void_With_Entity = void(Entity);
			static void func_void_with_entity([[maybe_unused]] Entity entity) {}

			using EntityNameLookupKey = core::StringLookupKey<256>;
			using PairMap = cnt::map<EntityLookupKey, cnt::set<EntityLookupKey>>;
			using EntityArrayMap = cnt::map<EntityLookupKey, cnt::darray<Entity>>;

			struct ExclusiveAdjunctStore {
				//! Direct source-entity-id indexed target lookup. EntityBad means no binding for that source.
				cnt::darray<Entity> srcToTgt;
				//! Direct source-entity-id indexed position in the current target's source list.
				cnt::darray<uint32_t> srcToTgtIdx;
				//! Number of active source bindings in srcToTgt.
				uint32_t srcToTgtCnt = 0;
				//! Direct target-entity-id indexed source buckets used for traversal and wildcard operations.
				cnt::darray<cnt::darray<Entity>> tgtToSrc;
			};

			struct SparseComponentStoreErased {
				void* pStore = nullptr;
				void (*func_del)(void*, Entity) = nullptr;
				bool (*func_has)(const void*, Entity) = nullptr;
				bool (*func_copy_entity)(void*, Entity, Entity) = nullptr;
				uint32_t (*func_count)(const void*) = nullptr;
				void (*func_collect_entities)(const void*, cnt::darray<Entity>&) = nullptr;
				bool (*func_for_each_entity)(const void*, void*, bool (*)(void*, Entity)) = nullptr;
				void (*func_clear_store)(void*) = nullptr;
				void (*func_del_store)(void*) = nullptr;
			};

			template <typename T>
			struct SparseComponentStore final {
				cnt::sparse_storage<SparseComponentRecord<T>> data;

				static cnt::sparse_id sid(Entity entity) {
					return (cnt::sparse_id)entity.id();
				}

				T& add(Entity entity) {
					const auto sparseId = sid(entity);
					if (data.has(sparseId))
						return data[sparseId].value;

					auto& item = data.add(SparseComponentRecord<T>{entity});
					return item.value;
				}

				T& mut(Entity entity) {
					GAIA_ASSERT(data.has(sid(entity)));
					return data[sid(entity)].value;
				}

				const T& get(Entity entity) const {
					GAIA_ASSERT(data.has(sid(entity)));
					return data[sid(entity)].value;
				}

				void del_entity(Entity entity) {
					const auto sparseId = sid(entity);
					if (!data.empty() && data.has(sparseId))
						data.del(sparseId);
				}

				bool has(Entity entity) const {
					if (data.empty())
						return false;
					return data.has(sid(entity));
				}

				uint32_t count() const {
					return (uint32_t)data.size();
				}

				void collect_entities(cnt::darray<Entity>& out) const {
					out.reserve(out.size() + (uint32_t)data.size());
					for (const auto& item: data)
						out.push_back(item.entity);
				}

				void clear_store() {
					data.clear();
				}
			};

			template <typename T>
			static SparseComponentStoreErased make_sparse_component_store_erased(SparseComponentStore<T>* pStore) {
				SparseComponentStoreErased store{};
				store.pStore = pStore;
				store.func_del = [](void* pStoreRaw, Entity entity) {
					static_cast<SparseComponentStore<T>*>(pStoreRaw)->del_entity(entity);
				};
				store.func_has = [](const void* pStoreRaw, Entity entity) {
					return static_cast<const SparseComponentStore<T>*>(pStoreRaw)->has(entity);
				};
				store.func_copy_entity = [](void* pStoreRaw, Entity dstEntity, Entity srcEntity) {
					auto* pStore = static_cast<SparseComponentStore<T>*>(pStoreRaw);
					if (!pStore->has(srcEntity))
						return false;

					auto& dst = pStore->add(dstEntity);
					dst = pStore->get(srcEntity);
					return true;
				};
				store.func_count = [](const void* pStoreRaw) {
					return static_cast<const SparseComponentStore<T>*>(pStoreRaw)->count();
				};
				store.func_collect_entities = [](const void* pStoreRaw, cnt::darray<Entity>& out) {
					static_cast<const SparseComponentStore<T>*>(pStoreRaw)->collect_entities(out);
				};
				store.func_for_each_entity = [](const void* pStoreRaw, void* pCtx, bool (*func)(void*, Entity)) {
					for (const auto& item: static_cast<const SparseComponentStore<T>*>(pStoreRaw)->data) {
						if (!func(pCtx, item.entity))
							return false;
					}
					return true;
				};
				store.func_clear_store = [](void* pStoreRaw) {
					static_cast<SparseComponentStore<T>*>(pStoreRaw)->clear_store();
				};
				store.func_del_store = [](void* pStoreRaw) {
					delete static_cast<SparseComponentStore<T>*>(pStoreRaw);
				};
				return store;
			}

			//----------------------------------------------------------------------

#if GAIA_OBSERVERS_ENABLED
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
				};

				struct PropagatedTargetCacheKey {
					Entity bindingRelation = EntityBad;
					Entity traversalRelation = EntityBad;
					Entity rootTarget = EntityBad;
					QueryTravKind travKind = QueryTravKind::None;
					uint8_t travDepth = QueryTermOptions::TravDepthUnlimited;

					size_t hash() const {
						size_t seed = EntityLookupKey(bindingRelation).hash();
						seed ^= EntityLookupKey(traversalRelation).hash() + 0x9e3779b9u + (seed << 6u) + (seed >> 2u);
						seed ^= EntityLookupKey(rootTarget).hash() + 0x9e3779b9u + (seed << 6u) + (seed >> 2u);
						seed ^= size_t(travKind) + 0x9e3779b9u + (seed << 6u) + (seed >> 2u);
						seed ^= size_t(travDepth) + 0x9e3779b9u + (seed << 6u) + (seed >> 2u);
						return seed;
					}

					bool operator==(const PropagatedTargetCacheKey& other) const {
						return bindingRelation == other.bindingRelation && traversalRelation == other.traversalRelation &&
									 rootTarget == other.rootTarget && travKind == other.travKind && travDepth == other.travDepth;
					}
				};

				struct PropagatedTargetCacheEntry {
					uint32_t bindingRelationVersion = 0;
					uint32_t traversalRelationVersion = 0;
					cnt::darray<Entity> targets;
				};

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

					static void collect_query_matches(World& world, ObserverRuntimeData& obs, cnt::darray<Entity>& out) {
						out.clear();
						if (!world.valid(obs.entity))
							return;

						const auto& ec = world.fetch(obs.entity);
						if (!world.enabled(ec))
							return;

						obs.query.reset();
						obs.query.each([&](Entity entity) {
							out.push_back(entity);
						});

						core::sort(out, [](Entity left, Entity right) {
							return left.value() < right.value();
						});
					}

					static void collect_query_target_matches(
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

					static void append_valid_targets(World& world, cnt::darray<Entity>& out, EntitySpan targets) {
						for (auto entity: targets) {
							if (world.valid(entity))
								out.push_back(entity);
						}
					}

					static void copy_target_narrow_plan(const ObserverRuntimeData& obs, TargetNarrowCacheEntry& entry) {
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

					static bool same_target_narrow_plan(const ObserverRuntimeData& obs, const TargetNarrowCacheEntry& entry) {
						if (obs.plan.diff.dispatchKind != entry.kind || obs.plan.diff.bindingRelation != entry.bindingRelation ||
								obs.plan.diff.traversalRelation != entry.traversalRelation ||
								obs.plan.diff.travKind != entry.travKind || obs.plan.diff.travDepth != entry.travDepth ||
								obs.plan.diff.traversalTriggerTermCount != entry.triggerTermCount)
							return false;

						GAIA_FOR(obs.plan.diff.traversalTriggerTermCount) {
							if (obs.plan.diff.traversalTriggerTerms[i] != entry.triggerTerms[i])
								return false;
						}

						return true;
					}

					static void normalize_targets(cnt::darray<Entity>& targets) {
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

					static uint64_t query_hash(ObserverRuntimeData& obs) {
						auto& queryInfo = obs.query.fetch();
						return queryInfo.ctx().hashLookup.hash;
					}

					static bool same_query_ctx(const QueryCtx& left, const QueryCtx& right) {
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
							if (leftData._terms[i] != rightData._terms[i])
								return false;
						}

						GAIA_FOR(leftData.changedCnt) {
							if (leftData._changed[i] != rightData._changed[i])
								return false;
						}

						return true;
					}

					static int32_t find_match_cache_entry(cnt::darray<MatchCacheEntry>& cache, ObserverRuntimeData& obs) {
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

					static Context prepare(
							ObserverRegistry& registry, World& world, ObserverEvent event, EntitySpan terms,
							EntitySpan targetEntities = {}) {
						Context ctx{};
						ctx.event = event;
						const auto& index = registry.diff_index(event);

						if (terms.empty() && index.all.empty() && index.global.empty())
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
							append_valid_targets(world, ctx.targets, targetEntities);
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
										SharedDispatch::collect_from_map<true>(
												registry, world, index.traversalRelation, relation, matchStamp);
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

									if (!collect_diff_targets_for_observer(
													registry, world, *pObs, terms, targetEntities, entry.targets)) {
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

					static Context prepare_add_new(ObserverRegistry& registry, World& world, EntitySpan terms) {
						Context ctx{};
						ctx.event = ObserverEvent::OnAdd;
						const auto& index = registry.diff_index(ObserverEvent::OnAdd);

						if (terms.empty() && index.all.empty() && index.global.empty())
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

					static void append_targets(World& world, Context& ctx, EntitySpan targets) {
						if (!ctx.active || !ctx.targeted || targets.empty())
							return;

						append_valid_targets(world, ctx.targets, targets);
					}

					static void finish(World& world, Context&& ctx) {
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
				};

				struct DirectDispatcher {
					static void on_add(
							ObserverRegistry& registry, World& world, const Archetype& archetype, EntitySpan entsAdded,
							EntitySpan targets) {
						if GAIA_UNLIKELY (world.tearing_down())
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

							SharedDispatch::collect_for_is_target(
									registry, world, registry.m_observer_map_add_is, target, matchStamp);
							for (auto inheritedTarget: world.as_targets_trav_cache(target))
								SharedDispatch::collect_for_is_target(
										registry, world, registry.m_observer_map_add_is, inheritedTarget, matchStamp);
							SharedDispatch::collect_for_inherited_terms(
									registry, world, registry.m_observer_map_add, target, matchStamp);
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

							if (SharedDispatch::matches_direct_targets(obs, archetype, targets, pQueryInfo)) {
								SharedDispatch::execute_targets(world, obs, targets);
							}
						}

						registry.m_relevant_observers_tmp.clear();
					}

					static void on_del(
							ObserverRegistry& registry, World& world, const Archetype& archetype, EntitySpan entsRemoved,
							EntitySpan targets) {
						if GAIA_UNLIKELY (world.tearing_down())
							return;

						const bool archetypeIsPrefab = archetype.has(Prefab);
						if (!archetype.has_observed_terms() &&
								!SharedDispatch::has_terms(registry.m_observer_map_del, entsRemoved) &&
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

							SharedDispatch::collect_for_is_target(
									registry, world, registry.m_observer_map_del_is, target, matchStamp);
							for (auto inheritedTarget: world.as_targets_trav_cache(target))
								SharedDispatch::collect_for_is_target(
										registry, world, registry.m_observer_map_del_is, inheritedTarget, matchStamp);
							SharedDispatch::collect_for_inherited_terms(
									registry, world, registry.m_observer_map_del, target, matchStamp);
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

							if (matches) {
								SharedDispatch::execute_targets(world, obs, targets);
							}
						}

						registry.m_relevant_observers_tmp.clear();
					}
				};

				struct SharedDispatch {
					template <bool DiffOnly, typename TObserverMap>
					static void collect_from_map(
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

					static void collect_diff_from_list(
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
					GAIA_NODISCARD static bool has_terms(const TObserverMap& map, EntitySpan terms) {
						for (auto term: terms) {
							const auto it = map.find(EntityLookupKey(term));
							if (it != map.end() && !it->second.empty())
								return true;
						}

						return false;
					}

					template <typename TObserverMap>
					GAIA_NODISCARD static bool has_pair_relations(World& world, const TObserverMap& map, EntitySpan terms) {
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
					static void collect_for_event_term(
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
					static void collect_for_is_target(
							ObserverRegistry& registry, World& world, const TObserverMap& map, Entity target, uint64_t matchStamp) {
						collect_from_map<false>(registry, world, map, target, matchStamp);
					}

					template <typename Func>
					static void for_each_inherited_term(World& world, Entity baseEntity, Func&& func) {
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
					GAIA_NODISCARD static bool has_semantic_is_terms(World& world, const TObserverMap& map, EntitySpan terms) {
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
					GAIA_NODISCARD static bool has_inherited_terms(World& world, const TObserverMap& map, EntitySpan terms) {
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
					static void collect_for_inherited_terms(
							ObserverRegistry& registry, World& world, const TObserverMap& map, Entity baseEntity,
							uint64_t matchStamp) {
						for_each_inherited_term(world, baseEntity, [&](Entity inheritedId) {
							collect_for_event_term(registry, world, map, inheritedId, matchStamp);
						});
					}

					static void execute_targets(World& world, ObserverRuntimeData& obs, EntitySpan targets) {
						if (targets.empty())
							return;

						Iter it;
						it.set_world(&world);
						it.set_group_id(0);
						it.set_remapping_indices(0);
						obs.exec(it, targets);
					}

					static bool matches_direct_targets(
							ObserverRuntimeData& obs, const Archetype& archetype, EntitySpan targets,
							QueryInfo* pQueryInfo = nullptr) {
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
				};

			public:
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
					return m_observer_map_add.find(termKey) != m_observer_map_add.end() ||
								 m_observer_map_del.find(termKey) != m_observer_map_del.end();
				}

				GAIA_NODISCARD static bool can_mark_term_observed(World& world, Entity term) {
					if (!term.pair())
						return world.valid(term);

					if (is_wildcard(term))
						return false;

					const auto it = world.m_recs.pairs.find(EntityLookupKey(term));
					return it != world.m_recs.pairs.end() && world.valid(it->second, term);
				}

				GAIA_NODISCARD static bool
				is_semantic_is_term(Entity term, QueryMatchKind matchKind = QueryMatchKind::Semantic) {
					return matchKind != QueryMatchKind::Direct && term.pair() && term.id() == Is.id() && !is_wildcard(term.gen());
				}

				void mark_term_observed(World& world, Entity term, bool observed) {
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

				static void collect_traversal_descendants(
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

				static PropagatedTargetCacheEntry& ensure_propagated_targets_cached(
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

				static void collect_propagated_targets_cached(
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

				static void append_propagated_targets_cached(
						ObserverRegistry& registry, World& world, const ObserverRuntimeData& obs, Entity changedSource,
						cnt::darray<Entity>& outTargets) {
					auto& entry = ensure_propagated_targets_cached(registry, world, obs, changedSource);

					for (auto source: entry.targets)
						outTargets.push_back(source);
				}

				static bool collect_source_traversal_diff_targets(
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
						append_propagated_targets_cached(registry, world, obs, changedSources[0], outTargets);
						return true;
					}

					const auto visitStamp = world.next_entity_visit_stamp();
					cnt::set<EntityLookupKey> visitedPairs;
					for (auto changedSource: changedSources)
						collect_propagated_targets_cached(
								registry, world, obs, changedSource, visitStamp, visitedPairs, outTargets);

					return true;
				}

				static bool collect_diff_targets_for_observer(
						ObserverRegistry& registry, World& world, ObserverRuntimeData& obs, EntitySpan changedTerms,
						EntitySpan changedTargets, cnt::darray<Entity>& outTargets) {
					switch (obs.plan.exec_kind()) {
						case ObserverPlan::ExecKind::DiffLocal:
							DiffDispatcher::append_valid_targets(world, outTargets, changedTargets);
							return true;
						case ObserverPlan::ExecKind::DiffPropagated:
							return collect_source_traversal_diff_targets(
									registry, world, obs, changedTerms, changedTargets, outTargets);
						case ObserverPlan::ExecKind::DiffFallback:
							return false;
						case ObserverPlan::ExecKind::DirectQuery:
						case ObserverPlan::ExecKind::DirectFast:
							return false;
					}

					return false;
				}

				static bool observer_uses_changed_traversal_relation(
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

				static bool is_dynamic_pair_endpoint(EntityId endpoint) {
					return is_wildcard(endpoint) || is_variable(endpoint);
				}

				static bool is_observer_term_globally_dynamic(Entity term) {
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

				void add_diff_observer_term(World& world, Entity observer, Entity term, const QueryTermOptions& options) {
					GAIA_ASSERT(world.valid(observer));

					const auto& ec = world.fetch(observer);
					const auto compIdx = ec.pChunk->comp_idx(Observer);
					const auto& obs = *reinterpret_cast<const Observer_*>(ec.pChunk->comp_ptr(compIdx, ec.row));
					auto& index = diff_index(obs.event);

					switch (obs.event) {
						case ObserverEvent::OnAdd:
						case ObserverEvent::OnDel:
							break;
						case ObserverEvent::OnSet:
							return;
					}

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

				GAIA_NODISCARD DiffDispatchCtx
				prepare_diff(World& world, ObserverEvent event, EntitySpan terms, EntitySpan targetEntities = {}) {
					if GAIA_UNLIKELY (world.tearing_down())
						return {};
					return DiffDispatcher::prepare(*this, world, event, terms, targetEntities);
				}

				GAIA_NODISCARD DiffDispatchCtx prepare_diff_add_new(World& world, EntitySpan terms) {
					if GAIA_UNLIKELY (world.tearing_down())
						return {};
					return DiffDispatcher::prepare_add_new(*this, world, terms);
				}

				void append_diff_targets(World& world, DiffDispatchCtx& ctx, EntitySpan targets) {
					DiffDispatcher::append_targets(world, ctx, targets);
				}

				void finish_diff(World& world, DiffDispatchCtx&& ctx) {
					if GAIA_UNLIKELY (world.tearing_down())
						return;
					DiffDispatcher::finish(world, GAIA_MOV(ctx));
				}

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

				void try_mark_term_observed(World& world, Entity term) {
					if (!can_mark_term_observed(world, term))
						return;
					if (!has_observers_for_term(term))
						return;
					if ((world.fetch(term).flags & EntityContainerFlags::IsObserved) != 0)
						return;

					mark_term_observed(world, term, true);
				}

				//! Registers a new term to the observer registry and links an observer with it.
				//! \param world World the observer is triggered for
				//! \param term Term to add to @a observer
				//! \param observer Observer entity
				void add(World& world, Entity term, Entity observer, QueryMatchKind matchKind = QueryMatchKind::Semantic) {
					GAIA_ASSERT(!observer.pair());
					GAIA_ASSERT(world.valid(observer));
					// For a pair term, valid(pair) is true only if that exact pair is already materialized
					// in m_recs.pairs (exists in-world). Observers are allowed to register pair terms that
					// may appear later, so asserting just world.valid(term) for pairs when adding is wrong.
					GAIA_ASSERT(term.pair() || world.valid(term));

					const auto wasObserved = has_observers_for_term(term);
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
							// OnSet observers are not triggered via OnAdd/OnDel hooks.
							break;
					}
					if (!wasObserved && has_observers_for_term(term) && can_mark_term_observed(world, term))
						mark_term_observed(world, term, true);
				}

				//! Removes a term from the observer registry.
				//! \param world World the observer is triggered for
				//! \param tern Term to remove from @a observer
				void del(World& w, Entity term) {
					GAIA_ASSERT(w.valid(term));

					const auto termKey = EntityLookupKey(term);
					const auto erasedData = m_observer_data.erase(termKey);
					const auto erasedOnAdd = m_observer_map_add.erase(termKey);
					const auto erasedOnDel = m_observer_map_del.erase(termKey);
					if (is_semantic_is_term(term)) {
						const auto isKey = EntityLookupKey(w.get(term.gen()));
						m_observer_map_add_is.erase(isKey);
						m_observer_map_del_is.erase(isKey);
					}
					if ((erasedOnAdd != 0 || erasedOnDel != 0) && can_mark_term_observed(w, term))
						mark_term_observed(w, term, false);

					// A regular term deletion has nothing else to clean up.
					// If an observer gets deleted, remove it from all term mappings.
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

								if (can_mark_term_observed(w, mappedTerm) && !has_observers_for_term(mappedTerm))
									mark_term_observed(w, mappedTerm, false);
							} else
								++it;
						}
					};
					remove_observer_from_map(m_observer_map_add);
					remove_observer_from_map(m_observer_map_del);
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

				//! Called when components are added to an entity.
				//! \param world World the observer is triggered for
				//! \param archetype Archetype we try to match with the observer
				//! \param ents_added Span of entities added to the @a archetype
				//! \param targets Span on entities for which the observers triggers
				void on_add(World& world, const Archetype& archetype, EntitySpan ents_added, EntitySpan targets) {
					DirectDispatcher::on_add(*this, world, archetype, ents_added, targets);
				}

				//! Called when components are removed from an entity
				//! \param world World the observer is triggered for
				//! \param archetype Archetype we try to match with the observer
				//! \param ents_added Span of entities added to the @a archetype
				//! \param targets Span on entities for which the observers triggers
				void on_del(World& world, const Archetype& archetype, EntitySpan ents_removed, EntitySpan targets) {
					DirectDispatcher::on_del(*this, world, archetype, ents_removed, targets);
				}
			};
#endif

			//----------------------------------------------------------------------

			//! Cache of components
			ComponentCache m_compCache;
			//! Cache of queries
			QueryCache m_queryCache;
			//! Reentrant per-world scratch frames reused by QueryInfo while matching archetypes.
			//! Each active match() acquires one frame so nested queries don't clobber outer scratch.
			cnt::darray<QueryMatchScratch*> m_queryMatchScratchStack;
			//! Number of currently acquired scratch frames.
			uint32_t m_queryMatchScratchDepth = 0;
			//! A map of [Query*, Buffer].
			//! Contains serialization buffers used by queries during their initialization.
			//! Kept here because it's only necessary for query initialization and would just
			//! take space on a query almost 100% of the time with no purpose at all.
			//! Records removed as soon as the query is compiled.
			QuerySerMap m_querySerMap;
			uint32_t m_nextQuerySerId = 0;

			//! TODO: Use this empty space for something
			uint32_t m_emptySpace0 = 0;

			//! Map of entity -> archetypes
			EntityToArchetypeMap m_entityToArchetypeMap;
			//! Map of [entity; Is relationship targets].
			//!   w.as(herbivore, animal);
			//!   w.as(rabbit, herbivore);
			//!   w.as(hare, herbivore);
			//! -->
			//!   herbivore -> {animal}
			//!   rabbit -> {herbivore}
			//!   hare -> {herbivore}
			PairMap m_entityToAsTargets;
			//! Lazily built transitive closure for m_entityToAsTargets.
			//! Cleared whenever an `Is` edge changes.
			mutable cnt::map<EntityLookupKey, cnt::darray<Entity>> m_entityToAsTargetsTravCache;
			//! Map of [entity; Is relationship relations]
			//!   w.as(herbivore, animal);
			//!   w.as(rabbit, herbivore);
			//!   w.as(hare, herbivore);
			//!-->
			//!   animal -> {herbivore}
			//!   herbivore -> {rabbit, hare}
			PairMap m_entityToAsRelations;
			//! Lazily built transitive closure for m_entityToAsRelations.
			//! Cleared whenever an `Is` edge changes.
			mutable cnt::map<EntityLookupKey, cnt::darray<Entity>> m_entityToAsRelationsTravCache;
			//! Lazily built ancestor chains for unlimited source traversal.
			//! Keyed by `(relation, source)` and cleared whenever a pair edge changes.
			mutable cnt::map<EntityLookupKey, cnt::darray<Entity>> m_targetsTravCache;
			//! Lazily built breadth-first descendant closures for unlimited source traversal.
			//! Keyed by `(relation, source)` and cleared whenever a pair edge changes.
			mutable cnt::map<EntityLookupKey, cnt::darray<Entity>> m_srcBfsTravCache;
			//! Lazily built hierarchy levels for cached depth-ordered iteration.
			//! Keyed by `(relation, target)` and cleared whenever a pair edge changes.
			mutable cnt::map<EntityLookupKey, uint32_t> m_depthOrderCache;
			//! Lazily built deduped sources for wildcard source traversal on a target entity.
			//! Keyed by `target` and cleared whenever a pair edge changes.
			mutable cnt::map<EntityLookupKey, cnt::darray<Entity>> m_sourcesAllCache;
			//! Lazily built deduped targets for wildcard target traversal on a source entity.
			//! Keyed by `source` and cleared whenever a pair edge changes.
			mutable cnt::map<EntityLookupKey, cnt::darray<Entity>> m_targetsAllCache;
			//! Map of relation -> targets
			PairMap m_relToTgt;
			//! Map of target -> relations
			PairMap m_tgtToRel;
			//! Non-fragmenting exclusive relation stores keyed by the relation entity.
			cnt::map<EntityLookupKey, ExclusiveAdjunctStore> m_exclusiveAdjunctByRel;
			//! Source entity -> non-fragmenting exclusive relations present on that source.
			EntityArrayMap m_srcToExclusiveAdjunctRel;
			//! Non-fragmenting sparse component stores keyed by the component entity.
			cnt::map<EntityLookupKey, SparseComponentStoreErased> m_sparseComponentsByComp;
			//! Relation-local structural version used for dependency ordering caches.
			cnt::map<EntityLookupKey, uint32_t> m_relationVersions;
			//! Sparse archetype-membership versions tracked only for entities used by source-cached queries.
			//! Entries are created lazily on first snapshot to avoid any global per-entity tax.
			mutable cnt::map<EntityLookupKey, uint32_t> m_srcEntityVersions;

			//! Array of all archetypes
			ArchetypeDArray m_archetypes;
			//! Map of archetypes identified by their component hash code
			ArchetypeMapByHash m_archetypesByHash;
			//! Map of archetypes identified by their ID
			ArchetypeMapById m_archetypesById;

			//! Pointer to the root archetype
			Archetype* m_pRootArchetype = nullptr;
			//! Entity archetype
			Archetype* m_pEntityArchetype = nullptr;
			//! Pointer to the component archetype
			Archetype* m_pCompArchetype = nullptr;
			//! Id assigned to the next created archetype
			ArchetypeId m_nextArchetypeId = 0;

			//! TODO: Use this empty space for something
			uint32_t m_emptySpace1 = 0;

			//! Entity records
			EntityContainers m_recs;

			//! Name to entity mapping
			cnt::map<EntityNameLookupKey, Entity> m_nameToEntity;
			//! Alias to entity mapping. Ambiguous aliases are stored with EntityBad and treated as lookup misses.
			cnt::map<EntityNameLookupKey, Entity> m_aliasToEntity;
			//! Current scope used for component registration and relative component lookup.
			Entity m_componentScope = EntityBad;
			//! Ordered lookup scopes used for unqualified component lookup when component scope is not enough.
			cnt::darray<Entity> m_componentLookupPath;
			//! Cached dotted path for the current component scope.
			mutable util::str m_componentScopePathCache;
			//! Scope entity associated with the cached dotted path.
			mutable Entity m_componentScopePathCacheEntity = EntityBad;
			//! Whether the cached dotted path is up-to-date.
			mutable bool m_componentScopePathCacheValid = false;

			//! Archetypes requested to be deleted
			cnt::set<ArchetypeLookupKey> m_reqArchetypesToDel;
			//! Entities requested to be deleted
			cnt::set<EntityLookupKey> m_reqEntitiesToDel;

#if GAIA_OBSERVERS_ENABLED
			//! Observers
			ObserverRegistry m_observers;
#endif

			//! Command buffer for commands executed from a locked world. Not thread-safe
			CommandBufferST* m_pCmdBufferST;
			//! Command buffer for commands executed from a locked world. Thread-safe
			CommandBufferMT* m_pCmdBufferMT;
			//! Runtime callbacks have been shut down and must not execute anymore.
			bool m_teardownActive = false;
			//! Query used to iterate systems
			ecs::Query m_systemsQuery;
			//! Scratch entity-visit stamps reused by wildcard relationship traversal helpers.
			mutable cnt::darray<uint64_t> m_entityVisitStamps;
			//! Monotonic stamp used with m_entityVisitStamps for O(1) per-call dedup.
			mutable uint64_t m_entityVisitStamp = 0;

			//! Local set of entities to delete
			cnt::set<EntityLookupKey> m_entitiesToDel;
			//! Array of chunks to delete
			cnt::darray<ArchetypeChunkPair> m_chunksToDel;
			//! Array of archetypes to delete
			ArchetypeDArray m_archetypesToDel;
			//! Index of the last defragmented archetype in the archetype list
			uint32_t m_defragLastArchetypeIdx = 0;
			//! Maximum number of entities to defragment per world tick
			uint32_t m_defragEntitiesPerTick = 100;

			//! With every structural change world version changes
			uint32_t m_worldVersion = 0;
			//! Increments whenever an entity enable bit changes.
			uint32_t m_enabledHierarchyVersion = 0;

			uint32_t m_structuralChangesLocked = 0;

		public:
			World():
					// Command buffer for the main thread
					m_pCmdBufferST(cmd_buffer_st_create(*this)),
					// Command buffer safe for concurrent access
					m_pCmdBufferMT(cmd_buffer_mt_create(*this)) {
				init();
			}

			~World() {
				teardown();
				done();
				cmd_buffer_destroy(*m_pCmdBufferST);
				cmd_buffer_destroy(*m_pCmdBufferMT);
			}

			World(World&&) = delete;
			World(const World&) = delete;
			World& operator=(World&&) = delete;
			World& operator=(const World&) = delete;

			//----------------------------------------------------------------------

			//! Provides a query set up to work with the parent world.
			//! \tparam UseCache If true, results of the query are cached
			//! \return Valid query object
			template <bool UseCache = true>
			auto query() {
				if constexpr (UseCache) {
					return Query(
							*const_cast<World*>(this), m_queryCache,
							//
							m_nextArchetypeId, m_worldVersion, m_archetypesById, m_entityToArchetypeMap, m_archetypes);
				} else {
					return QueryUncached(
							*const_cast<World*>(this),
							//
							m_nextArchetypeId, m_worldVersion, m_archetypesById, m_entityToArchetypeMap, m_archetypes);
				}
			}

			//----------------------------------------------------------------------

			GAIA_NODISCARD EntityContainer& fetch(Entity entity) {
				// Wildcard pairs are not a real entity so we can't accept them
				GAIA_ASSERT(!entity.pair() || !is_wildcard(entity));
				if (!valid(entity)) {
					// Delete-time cleanup can still reference an exact pair record after one endpoint
					// has already become invalid. As long as the pair record itself still exists,
					// allow fetch() so cleanup can finish removing it.
					if (!(entity.pair() && !is_wildcard(entity) && m_recs.pairs.contains(EntityLookupKey(entity))))
						GAIA_ASSERT(false);
				}
				return m_recs[entity];
			}

			GAIA_NODISCARD const EntityContainer& fetch(Entity entity) const {
				// Wildcard pairs are not a real entity so we can't accept them
				GAIA_ASSERT(!entity.pair() || !is_wildcard(entity));
				if (!valid(entity)) {
					if (!(entity.pair() && !is_wildcard(entity) && m_recs.pairs.contains(EntityLookupKey(entity))))
						GAIA_ASSERT(false);
				}
				return m_recs[entity];
			}

			GAIA_NODISCARD static bool is_req_del(const EntityContainer& ec) {
				if ((ec.flags & EntityContainerFlags::DeleteRequested) != 0)
					return true;
				GAIA_ASSERT((ec.flags & EntityContainerFlags::Load) == 0);
				return ec.pArchetype != nullptr && ec.pArchetype->is_req_del();
			}

			GAIA_NODISCARD bool is_dont_fragment(Entity entity) const {
				return (fetch(entity).flags & EntityContainerFlags::IsDontFragment) != 0;
			}

			GAIA_NODISCARD bool is_dont_fragment_relation(Entity relation) const {
				return valid(relation) && !relation.pair() && is_dont_fragment(relation);
			}

			GAIA_NODISCARD bool is_exclusive_dont_fragment_relation(Entity relation) const {
				if (!valid(relation) || relation.pair())
					return false;

				const auto& ec = fetch(relation);
				return (ec.flags & EntityContainerFlags::IsExclusive) != 0 &&
							 (ec.flags & EntityContainerFlags::IsDontFragment) != 0;
			}

			//! Returns true for hierarchy-like relations whose targets form an exclusive traversable parent chain.
			//! ChildOf and Parent satisfy this today. DependsOn intentionally does not.
			GAIA_NODISCARD bool is_hierarchy_relation(Entity relation) const {
				if (!valid(relation) || relation.pair())
					return false;

				return has(relation, Exclusive) && has(relation, Traversable);
			}

			//! Returns true when the relation still participates in archetype identity.
			//! Non-fragmenting relations such as Parent are excluded.
			GAIA_NODISCARD bool is_fragmenting_relation(Entity relation) const {
				return valid(relation) && !relation.pair() && !is_dont_fragment(relation);
			}

			//! Returns true for hierarchy relations that still fragment archetypes.
			//! ChildOf satisfies this today, while Parent intentionally does not.
			GAIA_NODISCARD bool is_fragmenting_hierarchy_relation(Entity relation) const {
				return is_hierarchy_relation(relation) && is_fragmenting_relation(relation);
			}

			//! Returns true when the relation can drive cached depth-ordered iteration.
			//! This requires a fragmenting relation whose target participates in archetype identity,
			//! such as ChildOf, DependsOn, or a custom fragmenting relation. Cycles are still invalid
			//! and are diagnosed by the depth cache itself.
			GAIA_NODISCARD bool supports_depth_order(Entity relation) const {
				return is_fragmenting_relation(relation);
			}

			//! Returns true when depth-ordered iteration may safely prune disabled subtrees at archetype level.
			//! Only fragmenting hierarchy relations qualify because all rows in the archetype then share
			//! the same direct parent and therefore the same ancestor chain.
			GAIA_NODISCARD bool depth_order_prunes_disabled_subtrees(Entity relation) const {
				return is_fragmenting_hierarchy_relation(relation);
			}

			GAIA_NODISCARD bool is_out_of_line_component(Entity component) const {
				if (!valid(component) || component.pair() || component.entity())
					return false;

				const auto* pItem = comp_cache().find(component);
				if (pItem == nullptr || component.kind() != EntityKind::EK_Gen || pItem->comp.soa() != 0)
					return false;

				const auto& ec = fetch(component);
				// Sparse AoS payloads live out-of-line even if the component still fragments.
				// DontFragment uses that same storage model but also removes the id from archetype identity.
				return component_has_out_of_line_data(pItem->comp) || (ec.flags & EntityContainerFlags::IsDontFragment) != 0;
			}

			GAIA_NODISCARD bool is_non_fragmenting_out_of_line_component(Entity component) const {
				if (!is_out_of_line_component(component))
					return false;

				return (fetch(component).flags & EntityContainerFlags::IsDontFragment) != 0;
			}

			//! Out-of-line non-fragmenting storage currently supports only plain generic components.
			//! Pairs, unique components and SoA layouts stay on the normal archetype path.
			template <typename T>
			GAIA_NODISCARD static constexpr bool supports_out_of_line_component() {
				using U = typename actual_type_t<T>::Type;
				return !is_pair<T>::value && entity_kind_v<T> == EntityKind::EK_Gen && !mem::is_soa_layout_v<U>;
			}

			template <typename T>
			GAIA_NODISCARD bool can_use_out_of_line_component(Entity object) const {
				if constexpr (!supports_out_of_line_component<T>())
					return false;
				else {
					if (object.pair())
						return false;

					const auto* pItem = comp_cache().find(object);
					if (pItem == nullptr || pItem->entity != object || !is_out_of_line_component(object))
						return false;

					using U = typename actual_type_t<T>::Type;
					return pItem->comp.size() == (uint32_t)sizeof(U) && pItem->comp.alig() == (uint32_t)alignof(U) &&
								 pItem->comp.soa() == 0 && object.kind() == entity_kind_v<T>;
				}
			}

			template <typename T>
			GAIA_NODISCARD SparseComponentStore<T>* sparse_component_store(Entity component) {
				const auto it = m_sparseComponentsByComp.find(EntityLookupKey(component));
				if (it == m_sparseComponentsByComp.end())
					return nullptr;

				return static_cast<SparseComponentStore<T>*>(it->second.pStore);
			}

			template <typename T>
			GAIA_NODISCARD const SparseComponentStore<T>* sparse_component_store(Entity component) const {
				const auto it = m_sparseComponentsByComp.find(EntityLookupKey(component));
				if (it == m_sparseComponentsByComp.end())
					return nullptr;

				return static_cast<const SparseComponentStore<T>*>(it->second.pStore);
			}

			template <typename T>
			GAIA_NODISCARD SparseComponentStore<T>& sparse_component_store_mut(Entity component) {
				const auto key = EntityLookupKey(component);
				const auto it = m_sparseComponentsByComp.find(key);
				if (it != m_sparseComponentsByComp.end())
					return *static_cast<SparseComponentStore<T>*>(it->second.pStore);

				auto* pStore = new SparseComponentStore<T>();
				m_sparseComponentsByComp.emplace(key, make_sparse_component_store_erased(pStore));
				return *pStore;
			}

			void del_sparse_components(Entity entity) {
				for (auto& [compKey, store]: m_sparseComponentsByComp) {
					(void)compKey;
					store.func_del(store.pStore, entity);
				}
			}

			void del_sparse_component_store(Entity component) {
				const auto it = m_sparseComponentsByComp.find(EntityLookupKey(component));
				if (it == m_sparseComponentsByComp.end())
					return;

				it->second.func_clear_store(it->second.pStore);
				it->second.func_del_store(it->second.pStore);
				m_sparseComponentsByComp.erase(it);
			}

			GAIA_NODISCARD const ExclusiveAdjunctStore* exclusive_adjunct_store(Entity relation) const {
				const auto it = m_exclusiveAdjunctByRel.find(EntityLookupKey(relation));
				if (it == m_exclusiveAdjunctByRel.end())
					return nullptr;

				return &it->second;
			}

			GAIA_NODISCARD ExclusiveAdjunctStore& exclusive_adjunct_store_mut(Entity relation) {
				return m_exclusiveAdjunctByRel[EntityLookupKey(relation)];
			}

			static void ensure_exclusive_adjunct_src_capacity(ExclusiveAdjunctStore& store, Entity source) {
				const auto required = (uint32_t)source.id() + 1;
				if (store.srcToTgt.size() >= required)
					return;

				const auto oldSize = (uint32_t)store.srcToTgt.size();
				auto newSize = oldSize == 0 ? 16U : oldSize;
				while (newSize < required)
					newSize *= 2U;

				store.srcToTgt.resize(newSize, EntityBad);
				store.srcToTgtIdx.resize(newSize, BadIndex);
			}

			static void ensure_exclusive_adjunct_tgt_capacity(ExclusiveAdjunctStore& store, Entity target) {
				const auto required = target.id() + 1;
				if (store.tgtToSrc.size() >= required)
					return;

				const auto oldSize = (uint32_t)store.tgtToSrc.size();
				auto newSize = oldSize == 0 ? 16U : oldSize;
				while (newSize < required)
					newSize *= 2U;

				store.tgtToSrc.resize(newSize);
			}

			GAIA_NODISCARD static Entity exclusive_adjunct_target(const ExclusiveAdjunctStore& store, Entity source) {
				if (source.id() >= store.srcToTgt.size())
					return EntityBad;

				return store.srcToTgt[source.id()];
			}

			GAIA_NODISCARD static const cnt::darray<Entity>*
			exclusive_adjunct_sources(const ExclusiveAdjunctStore& store, Entity target) {
				if (target.id() >= store.tgtToSrc.size())
					return nullptr;

				const auto& sources = store.tgtToSrc[target.id()];
				return sources.empty() ? nullptr : &sources;
			}

			static void del_exclusive_adjunct_target_source(ExclusiveAdjunctStore& store, Entity target, Entity source) {
				GAIA_ASSERT(target.id() < store.tgtToSrc.size());
				if (target.id() >= store.tgtToSrc.size())
					return;

				auto& sources = store.tgtToSrc[target.id()];
				const auto idx = source.id() < store.srcToTgtIdx.size() ? store.srcToTgtIdx[source.id()] : BadIndex;
				GAIA_ASSERT(idx != BadIndex && idx < sources.size());
				if (idx == BadIndex || idx >= sources.size())
					return;

				const auto lastIdx = (uint32_t)sources.size() - 1;
				if (idx != lastIdx) {
					const auto movedSource = sources[lastIdx];
					sources[idx] = movedSource;
					GAIA_ASSERT(movedSource.id() < store.srcToTgtIdx.size());
					store.srcToTgtIdx[movedSource.id()] = idx;
				}

				sources.pop_back();
			}

			void exclusive_adjunct_track_src_relation(Entity source, Entity relation) {
				auto& rels = m_srcToExclusiveAdjunctRel[EntityLookupKey(source)];
				if (!core::has(rels, relation))
					rels.push_back(relation);
			}

			void exclusive_adjunct_untrack_src_relation(Entity source, Entity relation) {
				const auto it = m_srcToExclusiveAdjunctRel.find(EntityLookupKey(source));
				if (it == m_srcToExclusiveAdjunctRel.end())
					return;

				auto& rels = it->second;
				const auto idx = core::get_index(rels, relation);
				if (idx != BadIndex)
					core::swap_erase_unsafe(rels, idx);
				if (rels.empty())
					m_srcToExclusiveAdjunctRel.erase(it);
			}

			void exclusive_adjunct_set(Entity source, Entity relation, Entity target) {
				GAIA_ASSERT(is_exclusive_dont_fragment_relation(relation));

				auto& store = exclusive_adjunct_store_mut(relation);
				ensure_exclusive_adjunct_src_capacity(store, source);
				const auto oldTarget = store.srcToTgt[source.id()];
				if (oldTarget != EntityBad) {
					if (oldTarget == target)
						return;

					del_exclusive_adjunct_target_source(store, oldTarget, source);
				} else {
					++store.srcToTgtCnt;
					exclusive_adjunct_track_src_relation(source, relation);
				}

				ensure_exclusive_adjunct_tgt_capacity(store, target);
				auto& sources = store.tgtToSrc[target.id()];
				store.srcToTgt[source.id()] = target;
				store.srcToTgtIdx[source.id()] = (uint32_t)sources.size();
				sources.push_back(source);

				touch_rel_version(relation);
				invalidate_queries_for_rel(relation);
				m_targetsTravCache = {};
				m_srcBfsTravCache = {};
				m_depthOrderCache = {};
				m_sourcesAllCache = {};
				m_targetsAllCache = {};
			}

			bool exclusive_adjunct_del(Entity source, Entity relation, Entity target) {
				const auto itStore = m_exclusiveAdjunctByRel.find(EntityLookupKey(relation));
				if (itStore == m_exclusiveAdjunctByRel.end())
					return false;

				auto& store = itStore->second;
				const auto oldTarget = exclusive_adjunct_target(store, source);
				if (oldTarget == EntityBad)
					return false;
				if (target != EntityBad && oldTarget != target)
					return false;

				del_exclusive_adjunct_target_source(store, oldTarget, source);
				store.srcToTgt[source.id()] = EntityBad;
				store.srcToTgtIdx[source.id()] = BadIndex;
				GAIA_ASSERT(store.srcToTgtCnt > 0);
				--store.srcToTgtCnt;

				exclusive_adjunct_untrack_src_relation(source, relation);
				if (store.srcToTgtCnt == 0)
					m_exclusiveAdjunctByRel.erase(itStore);

				touch_rel_version(relation);
				invalidate_queries_for_rel(relation);
				m_targetsTravCache = {};
				m_srcBfsTravCache = {};
				m_depthOrderCache = {};
				m_sourcesAllCache = {};
				m_targetsAllCache = {};

				return true;
			}

			GAIA_NODISCARD bool has_exclusive_adjunct_pair(Entity source, Entity object) const {
				if (!object.pair())
					return false;

				const auto srcRelsIt = m_srcToExclusiveAdjunctRel.find(EntityLookupKey(source));
				if (srcRelsIt == m_srcToExclusiveAdjunctRel.end())
					return false;

				const auto relId = object.id();
				const auto tgtId = object.gen();

				if (relId != All.id()) {
					const auto relation = get(relId);
					if (!is_exclusive_dont_fragment_relation(relation))
						return false;

					const auto* pStore = exclusive_adjunct_store(relation);
					if (pStore == nullptr)
						return false;

					const auto target = exclusive_adjunct_target(*pStore, source);
					if (target == EntityBad)
						return false;

					return tgtId == All.id() || target.id() == tgtId;
				}

				if (tgtId == All.id())
					return !srcRelsIt->second.empty();

				const auto target = get(tgtId);
				for (auto relKey: srcRelsIt->second) {
					const auto relation = relKey;
					const auto* pStore = exclusive_adjunct_store(relation);
					if (pStore == nullptr)
						continue;

					if (exclusive_adjunct_target(*pStore, source) == target)
						return true;
				}

				return false;
			}

			void del_exclusive_adjunct_source(Entity source) {
				const auto it = m_srcToExclusiveAdjunctRel.find(EntityLookupKey(source));
				if (it == m_srcToExclusiveAdjunctRel.end())
					return;

				cnt::darray<Entity> relations;
				for (auto relKey: it->second)
					relations.push_back(relKey);

				for (auto relation: relations) {
					touch_rel_version(relation);
					invalidate_queries_for_rel(relation);
					(void)exclusive_adjunct_del(source, relation, EntityBad);
				}
				m_targetsTravCache = {};
				m_srcBfsTravCache = {};
				m_depthOrderCache = {};
				m_sourcesAllCache = {};
				m_targetsAllCache = {};
			}

			void del_exclusive_adjunct_relation(Entity relation) {
				const auto itStore = m_exclusiveAdjunctByRel.find(EntityLookupKey(relation));
				if (itStore == m_exclusiveAdjunctByRel.end())
					return;

				cnt::darray<Entity> sources;
				const auto& srcToTgt = itStore->second.srcToTgt;
				GAIA_FOR((uint32_t)srcToTgt.size()) {
					if (srcToTgt[i] == EntityBad)
						continue;

					sources.push_back(get((EntityId)i));
				}

				touch_rel_version(relation);
				invalidate_queries_for_rel(relation);
				for (auto source: sources)
					(void)exclusive_adjunct_del(source, relation, EntityBad);
				m_targetsTravCache = {};
				m_srcBfsTravCache = {};
				m_depthOrderCache = {};
				m_sourcesAllCache = {};
				m_targetsAllCache = {};
			}

			//! Checks whether any non-fragmenting exclusive relation targeting @a target uses the given OnDeleteTarget rule.
			GAIA_NODISCARD bool has_exclusive_adjunct_target_cond(Entity target, Pair cond) const {
				for (const auto& [relKey, store]: m_exclusiveAdjunctByRel) {
					if (exclusive_adjunct_sources(store, target) == nullptr)
						continue;

					if (has(relKey.entity(), cond))
						return true;
				}

				return false;
			}

			//----------------------------------------------------------------------

			//! Resets runtime serializer binding to the default internal bin_stream backend.
			void set_serializer(std::nullptr_t) {
				// Always use the binary serializer as the default.
				m_serializer = ser::make_serializer(m_stream);
			}

			//! Binds a pre-built runtime serializer handle.
			void set_serializer(ser::serializer serializer) {
				GAIA_ASSERT(serializer.valid());
				m_serializer = serializer;
			}

			//! Binds a concrete serializer object through ser::make_serializer().
			template <typename TSerializer>
			void set_serializer(TSerializer& serializer) {
				set_serializer(ser::make_serializer(serializer));
			}

			//! Returns the currently bound runtime serializer handle.
			ser::serializer get_serializer() const {
				return m_serializer;
			}

			//----------------------------------------------------------------------

			struct EntityBuilder final {
				friend class World;

				World& m_world;
				//! Original archetype m_entity belongs to
				Archetype* m_pArchetypeSrc = nullptr;
				//! Original chunk m_entity belonged to
				Chunk* m_pChunkSrc = nullptr;
				//! Original row
				uint32_t m_rowSrc = 0;
				//! Target archetype we want to move to
				Archetype* m_pArchetype = nullptr;
				//! Target name
				EntityNameLookupKey m_targetNameKey;
				//! Target alias string pointer.
				EntityNameLookupKey m_targetAliasKey;
				//! Source entity
				Entity m_entity;
				//! Entity describing a single-step graph transition recorded during builder use.
				Entity m_graphEdgeEntity = EntityBad;
				//! Number of archetype-changing builder operations since the last commit.
				uint8_t m_graphEdgeOpCount = 0;
				//! Whether the recorded single-step transition is an add or delete move.
				bool m_graphEdgeIsAdd = false;

#if GAIA_ENABLE_ADD_DEL_HOOKS || GAIA_OBSERVERS_ENABLED
				static constexpr uint32_t MAX_TERMS = 32;
				static_assert(MAX_TERMS <= ChunkHeader::MAX_COMPONENTS);

				cnt::sarray_ext<Entity, MAX_TERMS> tl_new_comps;
				cnt::sarray_ext<Entity, MAX_TERMS> tl_del_comps;
#endif

				EntityBuilder(World& world, Entity entity, EntityContainer& ec):
						m_world(world), m_pArchetypeSrc(ec.pArchetype), m_pChunkSrc(ec.pChunk), m_rowSrc(ec.row),
						m_pArchetype(ec.pArchetype), m_entity(entity) {
					// Make sure entity matches the provided entity container record
					GAIA_ASSERT(ec.pChunk->entity_view()[ec.row] == entity);
				}

				EntityBuilder(World& world, Entity entity): m_world(world), m_entity(entity) {
					const auto& ec = world.fetch(entity);
					m_pArchetypeSrc = ec.pArchetype;
					m_pChunkSrc = ec.pChunk;
					m_rowSrc = ec.row;

					m_pArchetype = ec.pArchetype;
				}

				EntityBuilder(const EntityBuilder&) = default;
				EntityBuilder(EntityBuilder&&) = delete;
				EntityBuilder& operator=(const EntityBuilder&) = delete;
				EntityBuilder& operator=(EntityBuilder&&) = delete;

				~EntityBuilder() {
					commit();
				}

				//! Commits all gathered changes and performs an archetype movement.
				//! \warning Once called, the object is returned to its default state (as if no add/remove was ever called).
				void commit() {
					// No requests to change the archetype were made
					if (m_pArchetype == nullptr) {
						reset_graph_edge_tracking();
						return;
					}

					// Change in archetype detected
					if (m_pArchetypeSrc != m_pArchetype) {
						auto& ec = m_world.fetch(m_entity);
						GAIA_ASSERT(ec.pArchetype == m_pArchetypeSrc);
#if GAIA_OBSERVERS_ENABLED
						auto delDiffCtx = tl_del_comps.empty() ? ObserverRegistry::DiffDispatchCtx{}
																									 : m_world.m_observers.prepare_diff(
																												 m_world, ObserverEvent::OnDel, EntitySpan{tl_del_comps},
																												 EntitySpan{&m_entity, 1});
						auto addDiffCtx = tl_new_comps.empty() ? ObserverRegistry::DiffDispatchCtx{}
																									 : m_world.m_observers.prepare_diff(
																												 m_world, ObserverEvent::OnAdd, EntitySpan{tl_new_comps},
																												 EntitySpan{&m_entity, 1});
#endif

						// Trigger remove hooks if there are any
						trigger_del_hooks(*m_pArchetype);

						// Now that we have the final archetype move the entity to it
						m_world.move_entity_raw(m_entity, ec, *m_pArchetype);

						// Batched builder operations resolve intermediate archetypes without touching the
						// graph. Recreate the cached edge only for the single-step case.
						if (m_graphEdgeOpCount == 1 && m_graphEdgeEntity != EntityBad) {
							if (m_graphEdgeIsAdd)
								rebuild_graph_edge(m_pArchetypeSrc, m_pArchetype, m_graphEdgeEntity);
							else
								rebuild_graph_edge(m_pArchetype, m_pArchetypeSrc, m_graphEdgeEntity);
						}

						if (m_targetNameKey.str() != nullptr || m_targetAliasKey.str() != nullptr) {
							const auto compIdx = ec.pChunk->comp_idx(GAIA_ID(EntityDesc));
							// No need to update version, entity move did it already.
							auto* pDesc = reinterpret_cast<EntityDesc*>(ec.pChunk->comp_ptr_mut_gen<false>(compIdx, ec.row));
							GAIA_ASSERT(core::check_alignment(pDesc));

							// Update the entity name string pointers if necessary
							if (m_targetNameKey.str() != nullptr) {
								pDesc->name = m_targetNameKey.str();
								pDesc->name_len = m_targetNameKey.len();
							}

							// Update the entity alias string pointers if necessary
							if (m_targetAliasKey.str() != nullptr) {
								pDesc->alias = m_targetAliasKey.str();
								pDesc->alias_len = m_targetAliasKey.len();
							}
						}

						// Trigger add hooks if there are any
						trigger_add_hooks(*m_pArchetype);
#if GAIA_OBSERVERS_ENABLED
						m_world.m_observers.finish_diff(m_world, GAIA_MOV(delDiffCtx));
						m_world.m_observers.finish_diff(m_world, GAIA_MOV(addDiffCtx));
#endif
						cleanup_deleted_out_of_line_components();

						m_pArchetypeSrc = ec.pArchetype;
						m_pChunkSrc = ec.pChunk;
						m_rowSrc = ec.row;
					}
					// Archetype is still the same. Make sure no chunk movement has happened.
					else {
#if GAIA_ASSERT_ENABLED
						auto& ec = m_world.fetch(m_entity);
						GAIA_ASSERT(ec.pChunk == m_pChunkSrc);
#endif

#if GAIA_OBSERVERS_ENABLED
						auto delDiffCtx = tl_del_comps.empty() ? ObserverRegistry::DiffDispatchCtx{}
																									 : m_world.m_observers.prepare_diff(
																												 m_world, ObserverEvent::OnDel, EntitySpan{tl_del_comps},
																												 EntitySpan{&m_entity, 1});
						auto addDiffCtx = tl_new_comps.empty() ? ObserverRegistry::DiffDispatchCtx{}
																									 : m_world.m_observers.prepare_diff(
																												 m_world, ObserverEvent::OnAdd, EntitySpan{tl_new_comps},
																												 EntitySpan{&m_entity, 1});
#endif

						if (m_targetNameKey.str() != nullptr || m_targetAliasKey.str() != nullptr) {
							const auto compIdx = m_pChunkSrc->comp_idx(GAIA_ID(EntityDesc));
							auto* pDesc = reinterpret_cast<EntityDesc*>(m_pChunkSrc->comp_ptr_mut_gen<true>(compIdx, m_rowSrc));
							GAIA_ASSERT(core::check_alignment(pDesc));

							// Update the entity name string pointers if necessary
							if (m_targetNameKey.str() != nullptr) {
								pDesc->name = m_targetNameKey.str();
								pDesc->name_len = m_targetNameKey.len();
							}

							// Update the entity alias string pointers if necessary
							if (m_targetAliasKey.str() != nullptr) {
								pDesc->alias = m_targetAliasKey.str();
								pDesc->alias_len = m_targetAliasKey.len();
							}
						}

#if GAIA_OBSERVERS_ENABLED
						m_world.m_observers.finish_diff(m_world, GAIA_MOV(delDiffCtx));
						m_world.m_observers.finish_diff(m_world, GAIA_MOV(addDiffCtx));
#endif
						cleanup_deleted_out_of_line_components();
					}

					// Finalize the builder by reseting the archetype pointer
					m_pArchetype = nullptr;
					m_targetNameKey = {};
					m_targetAliasKey = {};
					reset_graph_edge_tracking();
				}

				//! Assigns a @a name to entity. Ignored if used with pair.
				//! The string is copied and kept internally.
				//! \param name A null-terminated string.
				//! \param len String length. If zero, the length is calculated.
				//! \warning Name is expected to be unique. If it is not this function does nothing.
				//! \warning The name can't contain the character '.'. This character is reserved for hierarchical lookups
				//!          such as "parent.child.subchild".
				void name(const char* name, uint32_t len = 0) {
					name_inter<true>(name, len);
				}

				//! Assigns a @a name to entity. Ignored if used with pair.
				//! The string is NOT copied. Your are responsible for its lifetime.
				//! \param name Pointer to a stable null-terminated string.
				//! \param len String length. If zero, the length is calculated.
				//! \warning The name is expected to be unique. If it is not this function does nothing.
				//! \warning The name can't contain the character '.'. This character is reserved for hierarchical lookups
				//!          such as "parent.child.subchild".
				//! \warning In this case the string is NOT copied and NOT stored internally. You are responsible for its
				//!          lifetime. The pointer also needs to be stable. Otherwise, any time your storage tries to move
				//!          the string to a different place you have to unset the name before it happens and set it anew
				//!          after the move is done.
				void name_raw(const char* name, uint32_t len = 0) {
					name_inter<false>(name, len);
				}

				//! Assigns an alias to entity. Ignored if used with pair.
				//! The string is copied and kept internally.
				//! \param alias A null-terminated string.
				//! \param len String length. If zero, the length is calculated.
				void alias(const char* alias, uint32_t len = 0) {
					alias_inter<true>(alias, len);
				}

				//! Assigns an alias to entity. Ignored if used with pair.
				//! The string is NOT copied. You are responsible for its lifetime.
				//! \param alias Pointer to a stable null-terminated string.
				//! \param len String length. If zero, the length is calculated.
				void alias_raw(const char* alias, uint32_t len = 0) {
					alias_inter<false>(alias, len);
				}

				//! Removes any name associated with the entity
				void del_name() {
					if (m_entity.pair())
						return;

					// The following block is essentially the same as this but without the archetype pointer access:
					// const auto compIdx = core::get_index(m_pArchetypeSrc->ids_view(), GAIA_ID(EntityDesc));
					const auto compIdx = core::get_index(m_pChunkSrc->ids_view(), GAIA_ID(EntityDesc));
					if (compIdx == BadIndex)
						return;

					{
						const auto* pDesc = reinterpret_cast<const EntityDesc*>(m_pChunkSrc->comp_ptr(compIdx, m_rowSrc));
						GAIA_ASSERT(core::check_alignment(pDesc));
						if (pDesc->name == nullptr)
							return;
					}

					// TODO: Trigger the hooks/observers manually here? I do not like essentially calling comp_ptr twice here.
					//       The second one could be replaced with const cast + emit.
					//  No need to update version, commit() will do it
					auto* pDesc = reinterpret_cast<EntityDesc*>(m_pChunkSrc->comp_ptr_mut_gen<false>(compIdx, m_rowSrc));
					del_name_inter(EntityNameLookupKey(pDesc->name, pDesc->name_len, 0));
					m_world.invalidate_scope_path_cache();

					pDesc->name = nullptr;
					pDesc->name_len = 0;
					if (pDesc->alias == nullptr)
						del_inter(GAIA_ID(EntityDesc));

					m_targetNameKey = {};
				}

				//! Removes any alias associated with the entity
				void del_alias() {
					if (m_entity.pair())
						return;

					// The following block is essentially the same as this but without the archetype pointer access:
					// const auto compIdx = core::get_index(m_pArchetypeSrc->ids_view(), GAIA_ID(EntityDesc));
					const auto compIdx = core::get_index(m_pChunkSrc->ids_view(), GAIA_ID(EntityDesc));
					if (compIdx == BadIndex)
						return;

					{
						const auto* pDesc = reinterpret_cast<const EntityDesc*>(m_pChunkSrc->comp_ptr(compIdx, m_rowSrc));
						GAIA_ASSERT(core::check_alignment(pDesc));
						if (pDesc->alias == nullptr)
							return;
					}

					// TODO: Trigger the hooks/observers manually here? I do not like essentially calling comp_ptr twice here.
					//       The second one could be replaced with const cast + emit.
					//  No need to update version, commit() will do it
					auto* pDesc = reinterpret_cast<EntityDesc*>(m_pChunkSrc->comp_ptr_mut_gen<false>(compIdx, m_rowSrc));
					del_alias_inter(EntityNameLookupKey(pDesc->alias, pDesc->alias_len, 0));

					pDesc->alias = nullptr;
					pDesc->alias_len = 0;
					if (pDesc->name == nullptr)
						del_inter(GAIA_ID(EntityDesc));

					m_targetAliasKey = {};
				}

				//! Prepares an archetype movement by following the "add" edge of the current archetype.
				//! \param entity Added entity
				EntityBuilder& add(Entity entity) {
					GAIA_PROF_SCOPE(EntityBuilder::add);
					GAIA_ASSERT(m_world.valid(m_entity));
					GAIA_ASSERT(m_world.valid(entity));

					add_inter(entity);
					return *this;
				}

				//! Prepares an archetype movement by following the "add" edge of the current archetype.
				//! \param pair Relationship pair
				EntityBuilder& add(Pair pair) {
					GAIA_PROF_SCOPE(EntityBuilder::add);
					GAIA_ASSERT(m_world.valid(m_entity));
					GAIA_ASSERT(m_world.valid(pair.first()));
					GAIA_ASSERT(m_world.valid(pair.second()));

					add_inter(pair);
					return *this;
				}

				//! Shortcut for add(Pair(Is, entityBase)).
				//! Effectively makes an entity inherit from @a entityBase
				//! \param entityBase Entity to inherit from
				EntityBuilder& as(Entity entityBase) {
					return add(Pair(Is, entityBase));
				}

				//! Marks the entity as a prefab.
				EntityBuilder& prefab() {
					return add(Prefab);
				}

				//! Check if @a entity inherits from @a entityBase
				//! \param entity Source entity
				//! \param entityBase Base entity
				//! \return True if entity inherits from entityBase
				GAIA_NODISCARD bool as(Entity entity, Entity entityBase) const {
					return static_cast<const World&>(m_world).is(entity, entityBase);
				}

				//! Shortcut for add(Pair(ChildOf, parent))
				EntityBuilder& child(Entity parent) {
					return add(Pair(ChildOf, parent));
				}

				//! Takes care of registering the component \tparam T
				template <typename T>
				Entity register_component() {
					if constexpr (is_pair<T>::value) {
						const auto rel = m_world.add<typename T::rel>().entity;
						const auto tgt = m_world.add<typename T::tgt>().entity;
						const Entity ent = Pair(rel, tgt);
						add_inter(ent);
						return ent;
					} else {
						return m_world.add<T>().entity;
					}
				}

#if GAIA_USE_VARIADIC_API
				template <typename... T>
				EntityBuilder& add() {
					(verify_comp<T>(), ...);
					(add(register_component<T>()), ...);
					return *this;
				}
#else
				template <typename T>
				EntityBuilder& add() {
					verify_comp<T>();
					add(register_component<T>());
					return *this;
				}
#endif

				//! Prepares an archetype movement by following the "del" edge of the current archetype.
				//! \param entity Removed entity
				EntityBuilder& del(Entity entity) {
					GAIA_PROF_SCOPE(EntityBuilder::del);
					GAIA_ASSERT(m_world.valid(m_entity));
					GAIA_ASSERT(m_world.valid(entity));
					del_inter(entity);
					return *this;
				}

				//! Prepares an archetype movement by following the "del" edge of the current archetype.
				//! \param pair Relationship pair
				EntityBuilder& del(Pair pair) {
					GAIA_PROF_SCOPE(EntityBuilder::add);
					GAIA_ASSERT(m_world.valid(m_entity));
					GAIA_ASSERT(m_world.valid(pair.first()));
					GAIA_ASSERT(m_world.valid(pair.second()));
					del_inter(pair);
					return *this;
				}

#if GAIA_USE_VARIADIC_API
				template <typename... T>
				EntityBuilder& del() {
					(verify_comp<T>(), ...);
					(del(register_component<T>()), ...);
					return *this;
				}
#else
				template <typename T>
				EntityBuilder& del() {
					verify_comp<T>();
					del(register_component<T>());
					return *this;
				}
#endif

			private:
				//! Triggers add hooks for the component if there are any.
				//! \param newArchetype New archetype we belong to
				void trigger_add_hooks(const Archetype& newArchetype) {
#if GAIA_ENABLE_ADD_DEL_HOOKS || GAIA_OBSERVERS_ENABLED
					if GAIA_UNLIKELY (m_world.tearing_down()) {
						tl_new_comps.clear();
						(void)newArchetype;
						return;
					}

					m_world.lock();

	#if GAIA_ENABLE_ADD_DEL_HOOKS
					// if (hookCnt > 0)
					{
						// Trigger component hooks first
						for (auto entity: tl_new_comps) {
							if (!entity.comp())
								continue;

							const auto& item = m_world.comp_cache().get(entity);
							const auto& hooks = ComponentCache::hooks(item);
							if (hooks.func_add != nullptr)
								hooks.func_add(m_world, item, m_entity);
						}
					}
	#endif

	#if GAIA_OBSERVERS_ENABLED
					// Trigger observers second
					m_world.m_observers.on_add(m_world, newArchetype, std::span<Entity>{tl_new_comps}, {&m_entity, 1});
	#else
					(void)newArchetype;
	#endif

					tl_new_comps.clear();

					m_world.unlock();
#endif
				}

				//! Triggers del hooks for the component if there are any.
				//! \param newArchetype New archetype we belong to
				void trigger_del_hooks(const Archetype& newArchetype) {
#if GAIA_ENABLE_ADD_DEL_HOOKS || GAIA_OBSERVERS_ENABLED
					if GAIA_UNLIKELY (m_world.tearing_down()) {
						tl_del_comps.clear();
						(void)newArchetype;
						return;
					}

					m_world.notify_inherited_del_dependents(m_entity, std::span<Entity>{tl_del_comps});
					m_world.lock();

	#if GAIA_OBSERVERS_ENABLED
					// Trigger observers first
					m_world.m_observers.on_del(m_world, newArchetype, std::span<Entity>{tl_del_comps}, {&m_entity, 1});
	#else
					(void)newArchetype;
	#endif

	#if GAIA_ENABLE_ADD_DEL_HOOKS
					// if (hookCnt > 0)
					{
						// Trigger component hooks second
						for (auto entity: tl_del_comps) {
							if (!entity.comp())
								continue;

							const auto& item = m_world.comp_cache().get(entity);
							const auto& hooks = ComponentCache::hooks(item);
							if (hooks.func_del != nullptr)
								hooks.func_del(m_world, item, m_entity);
						}
					}
	#endif

					m_world.unlock();
#endif
				}

				void cleanup_deleted_out_of_line_components() {
					for (auto entity: tl_del_comps) {
						if (entity.pair() || !m_world.is_out_of_line_component(entity) ||
								m_world.is_non_fragmenting_out_of_line_component(entity))
							continue;

						const auto it = m_world.m_sparseComponentsByComp.find(EntityLookupKey(entity));
						if (it != m_world.m_sparseComponentsByComp.end())
							it->second.func_del(it->second.pStore, m_entity);
					}

					tl_del_comps.clear();
				}

				bool handle_add_entity(Entity entity) {
					cnt::sarray_ext<Entity, ChunkHeader::MAX_COMPONENTS> targets;

					const auto& ecMain = m_world.fetch(entity);
					if (entity.pair())
						m_world.invalidate_scope_path_cache();

					// Handle entity combinations that can't be together
					if ((ecMain.flags & EntityContainerFlags::HasCantCombine) != 0) {
						m_world.targets(entity, CantCombine, [&targets](Entity target) {
							targets.push_back(target);
						});
						for (auto e: targets) {
							if (m_pArchetype->has(e)) {
#if GAIA_ASSERT_ENABLED
								GAIA_ASSERT2(false, "Trying to add an entity which can't be combined with the source");
								print_archetype_entities(m_world, *m_pArchetype, entity, true);
#endif
								return false;
							}
						}
					}

					// Handle exclusivity
					if (entity.pair()) {
						// Check if (rel, tgt)'s rel part is exclusive
						const auto& ecRel = m_world.m_recs.entities[entity.id()];
						if ((ecRel.flags & EntityContainerFlags::IsExclusive) != 0) {
							auto rel = Entity(
									entity.id(), ecRel.data.gen, (bool)ecRel.data.ent, (bool)ecRel.data.pair,
									(EntityKind)ecRel.data.kind);
							auto tgt = m_world.try_get(entity.gen());
							if (tgt == EntityBad)
								return false;

							// Make sure to remove the (rel, tgt0) so only the new (rel, tgt1) remains.
							// However, before that we need to make sure there only exists one target at most.
							targets.clear();
							m_world.targets_if(m_entity, rel, [&targets](Entity target) {
								targets.push_back(target);
								// Stop the moment we have more than 1 target. This kind of scenario is not supported
								// and can happen only if Exclusive is added after multiple relationships already exist.
								return targets.size() < 2;
							});

							const auto targetsCnt = targets.size();
							if (targetsCnt > 1) {
#if GAIA_ASSERT_ENABLED
								GAIA_ASSERT2(
										false, "Trying to add a pair with exclusive relationship but there are multiple targets present. "
													 "Make sure to add the Exclusive property before any relationships with it are created.");
								print_archetype_entities(m_world, *m_pArchetype, entity, true);
#endif
								return false;
							}

							// Remove the previous relationship if possible.
							// We avoid self-removal.
							const auto tgtNew = *targets.begin();
							if (targetsCnt == 1 && tgt != tgtNew) {
								// Exclusive relationship replaces the previous one.
								// We need to check if the old one can be removed.
								// This is what del_inter does on the inside.
								// It first checks if entity can be deleted and calls handle_del afterwards.
								if (!can_del(entity)) {
#if GAIA_ASSERT_ENABLED
									GAIA_ASSERT2(
											false, "Trying to replace an exclusive relationship but the entity which is getting removed has "
														 "dependencies.");
									print_archetype_entities(m_world, *m_pArchetype, entity, true);
#endif
									return false;
								}

								handle_del(ecs::Pair(rel, tgtNew));
							}
						}
					}

					// Handle requirements
					{
						targets.clear();
						m_world.targets(entity, Requires, [&targets](Entity target) {
							targets.push_back(target);
						});

						for (auto e: targets) {
							auto* pArchetype = m_pArchetype;
							handle_add<false>(e);
							if (m_pArchetype != pArchetype)
								handle_add_entity(e);
						}
					}

					return true;
				}

				GAIA_NODISCARD bool has_Requires_tgt(Entity entity) const {
					// Don't allow to delete entity if something in the archetype requires it
					auto ids = m_pArchetype->ids_view();
					for (auto e: ids) {
						if (m_world.has(e, Pair(Requires, entity)))
							return true;
					}

					return false;
				}

				static void set_flag(EntityContainerFlagsType& flags, EntityContainerFlags flag, bool enable) {
					if (enable)
						flags |= flag;
					else
						flags &= ~flag;
				}

				void set_flag(Entity entity, EntityContainerFlags flag, bool enable) {
					auto& ec = m_world.fetch(entity);
					set_flag(ec.flags, flag, enable);
				}

				void try_set_flags(Entity entity, bool enable) {
					auto& ecMain = m_world.fetch(m_entity);
					try_set_CantCombine(ecMain, entity, enable);

					auto& ec = m_world.fetch(entity);
					try_set_Is(ec, entity, enable);
					try_set_IsExclusive(ecMain, entity, enable);
					try_set_IsDontFragment(ecMain, entity, enable);
					try_set_IsSingleton(ecMain, entity, enable);
					try_set_OnDelete(ecMain, entity, enable);
					try_set_OnDeleteTarget(entity, enable);
				}

				void try_set_Is(EntityContainer& ec, Entity entity, bool enable) {
					if (!entity.pair() || entity.id() != Is.id())
						return;

					set_flag(ec.flags, EntityContainerFlags::HasAliasOf, enable);
				}

				void try_set_CantCombine(EntityContainer& ec, Entity entity, bool enable) {
					if (!entity.pair() || entity.id() != CantCombine.id())
						return;

					GAIA_ASSERT(entity != m_entity);

					// Setting the flag can be done right away.
					// One bit can only contain information about one pair but there
					// can be any amount of CanCombine pairs formed with an entity.
					// Therefore, when resetting the flag, we first need to check if there
					// are any other targets with this flag set and only reset the flag
					// if there is only one present.
					if (enable)
						set_flag(ec.flags, EntityContainerFlags::HasCantCombine, true);
					else if ((ec.flags & EntityContainerFlags::HasCantCombine) != 0) {
						uint32_t targets = 0;
						m_world.targets(m_entity, CantCombine, [&targets]([[maybe_unused]] Entity entity) {
							++targets;
						});
						if (targets == 1)
							set_flag(ec.flags, EntityContainerFlags::HasCantCombine, false);
					}
				}

				void try_set_IsExclusive(EntityContainer& ec, Entity entity, bool enable) {
					if (entity.pair() || entity.id() != Exclusive.id())
						return;

					set_flag(ec.flags, EntityContainerFlags::IsExclusive, enable);
				}

				void try_set_IsDontFragment(EntityContainer& ec, Entity entity, bool enable) {
					if (entity.pair() || entity.id() != DontFragment.id())
						return;

					set_flag(ec.flags, EntityContainerFlags::IsDontFragment, enable);
				}

				void try_set_OnDeleteTarget(Entity entity, bool enable) {
					if (!entity.pair())
						return;

					const auto rel = m_world.try_get(entity.id());
					const auto tgt = m_world.try_get(entity.gen());
					if (rel == EntityBad || tgt == EntityBad)
						return;

					// Adding a pair to an entity with OnDeleteTarget relationship.
					// We need to update the target entity's flags.
					if (m_world.has(rel, Pair(OnDeleteTarget, Delete)))
						set_flag(tgt, EntityContainerFlags::OnDeleteTarget_Delete, enable);
					else if (m_world.has(rel, Pair(OnDeleteTarget, Remove)))
						set_flag(tgt, EntityContainerFlags::OnDeleteTarget_Remove, enable);
					else if (m_world.has(rel, Pair(OnDeleteTarget, Error)))
						set_flag(tgt, EntityContainerFlags::OnDeleteTarget_Error, enable);
				}

				void try_set_OnDelete(EntityContainer& ec, Entity entity, bool enable) {
					if (entity == Pair(OnDelete, Delete))
						set_flag(ec.flags, EntityContainerFlags::OnDelete_Delete, enable);
					else if (entity == Pair(OnDelete, Remove))
						set_flag(ec.flags, EntityContainerFlags::OnDelete_Remove, enable);
					else if (entity == Pair(OnDelete, Error))
						set_flag(ec.flags, EntityContainerFlags::OnDelete_Error, enable);
				}

				void try_set_IsSingleton(EntityContainer& ec, Entity entity, bool enable) {
					const bool isSingleton = enable && m_entity == entity;
					set_flag(ec.flags, EntityContainerFlags::IsSingleton, isSingleton);
				}

				void handle_DependsOn(Entity entity, bool enable) {
					(void)entity;
					(void)enable;
					// auto& ec = m_world.fetch(entity);
					// if (enable) {
					// 	// Calculate the depth in the dependency tree
					// 	uint32_t depth = 1;

					// 	auto e = entity;
					// 	if (m_world.valid(e)) {
					// 		while (true) {
					// 			auto tgt = m_world.target(e, DependsOn);
					// 			if (tgt == EntityBad)
					// 				break;

					// 			++depth;
					// 			e = tgt;
					// 		}
					// 	}
					// 	ec.depthDependsOn = (uint8_t)depth;

					// 	// Update depth for all entities depending on this one
					// 	auto q = m_world.query<false>();
					// 	q.all(ecs::Pair(DependsOn, m_entity)) //
					// 			.each([&](Entity dependingEntity) {
					// 				auto& ecDependingEntity = m_world.fetch(dependingEntity);
					// 				ecDependingEntity.depthDependsOn += (uint8_t)depth;
					// 			});
					// } else {
					// 	// Update depth for all entities depending on this one
					// 	auto q = m_world.query<false>();
					// 	q.all(ecs::Pair(DependsOn, m_entity)) //
					// 			.each([&](Entity dependingEntity) {
					// 				auto& ecDependingEntity = m_world.fetch(dependingEntity);
					// 				ecDependingEntity.depthDependsOn -= ec.depthDependsOn;
					// 			});

					// 	// Reset the depth
					// 	ec.depthDependsOn = 0;
					// }
				}

				template <bool IsBootstrap>
				bool handle_add(Entity entity) {
					const bool isDontFragmentPair =
							entity.pair() && m_world.is_exclusive_dont_fragment_relation(m_world.get(entity.id()));
#if GAIA_ASSERT_ENABLED
					if (!isDontFragmentPair)
						World::verify_add(m_world, *m_pArchetype, m_entity, entity);
#endif

					// Don't add the same entity twice
					if (isDontFragmentPair ? m_world.has(m_entity, entity) : m_pArchetype->has(entity))
						return false;

					if (entity.pair()) {
						auto relation = m_world.get(entity.id());
						m_world.touch_rel_version(relation);
						m_world.invalidate_queries_for_rel(relation);
						m_world.m_targetsTravCache = {};
						m_world.m_srcBfsTravCache = {};
						m_world.m_depthOrderCache = {};
						m_world.m_sourcesAllCache = {};
						m_world.m_targetsAllCache = {};
					}

					try_set_flags(entity, true);

					// Update the Is relationship base counter if necessary
					if (entity.pair() && entity.id() == Is.id()) {
						auto e = m_world.try_get(entity.gen());
						if (e == EntityBad)
							return false;

						EntityLookupKey entityKey(m_entity);
						EntityLookupKey eKey(e);

						// m_entity -> {..., e}
						auto& entity_to_e = m_world.m_entityToAsTargets[entityKey];
						entity_to_e.insert(eKey);
						m_world.m_entityToAsTargetsTravCache = {};
						// e -> {..., m_entity}
						auto& e_to_entity = m_world.m_entityToAsRelations[eKey];
						e_to_entity.insert(entityKey);
						m_world.m_entityToAsRelationsTravCache = {};

						// Make sure the relation entity is registered as archetype so queries can find it
						// auto& ec = m_world.fetch(tgt);
						// m_world.add_entity_archetype_pair(m_entity, ec.pArchetype);

						// Cached queries might need to be invalidated.
						m_world.invalidate_queries_for_entity({Is, e});
					}

					if (isDontFragmentPair) {
						const auto relation = m_world.try_get(entity.id());
						const auto target = m_world.try_get(entity.gen());
						if (relation == EntityBad || target == EntityBad)
							return false;

						m_world.exclusive_adjunct_set(m_entity, relation, target);
					} else {
						m_pArchetype = m_world.foc_archetype_add_no_graph(m_pArchetype, entity);
						note_graph_edge(entity, true);
					}

					if constexpr (!IsBootstrap) {
						handle_DependsOn(entity, true);

#if GAIA_ENABLE_ADD_DEL_HOOKS || GAIA_OBSERVERS_ENABLED
						tl_new_comps.push_back(entity);
#endif
					}

					return true;
				}

				void handle_del(Entity entity) {
					if (entity.pair() && !m_world.valid(entity)) {
						if (m_pArchetype->has(entity)) {
							const auto relation = m_world.try_get(entity.id());
							if (relation != EntityBad) {
								m_world.touch_rel_version(relation);
								m_world.invalidate_queries_for_rel(relation);
								m_world.m_targetsTravCache = {};
								m_world.m_srcBfsTravCache = {};
								m_world.m_depthOrderCache = {};
								m_world.m_sourcesAllCache = {};
								m_world.m_targetsAllCache = {};
							}

							if (entity.id() == Is.id())
								m_world.unlink_stale_is_relations_by_target_id(m_entity, entity.gen());

							m_pArchetype = m_world.foc_archetype_del_no_graph(m_pArchetype, entity);
							note_graph_edge(entity, false);
						}
						return;
					}

					const bool isDontFragmentPair =
							entity.pair() && m_world.is_exclusive_dont_fragment_relation(m_world.get(entity.id()));
					if (entity.pair())
						m_world.invalidate_scope_path_cache();

#if GAIA_ASSERT_ENABLED
					if (!isDontFragmentPair)
						World::verify_del(m_world, *m_pArchetype, m_entity, entity);
#endif

					// Don't delete what has not beed added
					if (!(isDontFragmentPair ? m_world.has(m_entity, entity) : m_pArchetype->has(entity)))
						return;

					if (entity.pair()) {
						auto relation = m_world.get(entity.id());
						m_world.touch_rel_version(relation);
						m_world.invalidate_queries_for_rel(relation);
						m_world.m_targetsTravCache = {};
						m_world.m_srcBfsTravCache = {};
						m_world.m_depthOrderCache = {};
						m_world.m_sourcesAllCache = {};
						m_world.m_targetsAllCache = {};
					}

					try_set_flags(entity, false);
					handle_DependsOn(entity, false);

					// Update the Is relationship base counter if necessary
					if (entity.pair() && entity.id() == Is.id()) {
						auto e = m_world.try_get(entity.gen());
						if (e != EntityBad) {
							m_world.unlink_live_is_relation(m_entity, e);
						}
					}

					if (isDontFragmentPair) {
						const auto relation = m_world.try_get(entity.id());
						const auto target = m_world.try_get(entity.gen());
						if (relation != EntityBad && target != EntityBad)
							(void)m_world.exclusive_adjunct_del(m_entity, relation, target);
					} else {
						m_pArchetype = m_world.foc_archetype_del_no_graph(m_pArchetype, entity);
						note_graph_edge(entity, false);
					}

#if GAIA_ENABLE_ADD_DEL_HOOKS || GAIA_OBSERVERS_ENABLED
					tl_del_comps.push_back(entity);
#endif
				}

				void add_inter(Entity entity) {
					GAIA_ASSERT(!is_wildcard(entity));

					if (entity.pair()) {
						// Make sure the entity container record exists if it is a pair
						m_world.assign_pair(entity, *m_world.m_pEntityArchetype);
					}

					if (!handle_add_entity(entity))
						return;

					handle_add<false>(entity);
				}

				void note_graph_edge(Entity entity, bool isAdd) {
					++m_graphEdgeOpCount;
					m_graphEdgeEntity = entity;
					m_graphEdgeIsAdd = isAdd;
				}

				void reset_graph_edge_tracking() {
					m_graphEdgeEntity = EntityBad;
					m_graphEdgeOpCount = 0;
					m_graphEdgeIsAdd = false;
				}

				//! Rebuilds a single cached archetype-graph edge after a builder commit.
				//! The no-graph batch path bypasses stale-edge recovery, so clear both local caches first.
				static void rebuild_graph_edge(Archetype* pArchetypeLeft, Archetype* pArchetypeRight, Entity entity) {
					pArchetypeLeft->del_graph_edge_right_local(entity);
					pArchetypeRight->del_graph_edge_left_local(entity);
					pArchetypeLeft->build_graph_edges(pArchetypeRight, entity);
				}

				void add_inter_init(Entity entity) {
					GAIA_ASSERT(!is_wildcard(entity));

					if (entity.pair()) {
						// Make sure the entity container record exists if it is a pair
						m_world.assign_pair(entity, *m_world.m_pEntityArchetype);
					}

					if (!handle_add_entity(entity))
						return;

					handle_add<true>(entity);
				}

				GAIA_NODISCARD bool can_del(Entity entity) const noexcept {
					if (has_Requires_tgt(entity))
						return false;

					return true;
				}

				bool del_inter(Entity entity) {
					if (!can_del(entity))
						return false;

					handle_del(entity);
					return true;
				}

				void del_name_inter(EntityNameLookupKey key) {
					const auto it = m_world.m_nameToEntity.find(key);
					// If the assert is hit it means the pointer to the name string was invalidated or became dangling.
					// That should not be possible for strings managed internally so the only other option is user-managed
					// strings are broken.
					GAIA_ASSERT(it != m_world.m_nameToEntity.end());
					if (it != m_world.m_nameToEntity.end()) {
						// Release memory allocated for the string if we own it
						if (it->first.owned())
							mem::mem_free((void*)key.str());

						m_world.m_nameToEntity.erase(it);
					}
				}

				void del_alias_inter(EntityNameLookupKey key) {
					const auto it = m_world.m_aliasToEntity.find(key);
					// If the assert is hit it means the pointer to the name string was invalidated or became dangling.
					// That should not be possible for strings managed internally so the only other option is user-managed
					// strings are broken.
					GAIA_ASSERT(it != m_world.m_aliasToEntity.end());
					if (it != m_world.m_aliasToEntity.end()) {
						// Release memory allocated for the string if we own it
						if (it->first.owned())
							mem::mem_free((void*)key.str());

						m_world.m_aliasToEntity.erase(it);
					}
				}

				template <bool IsOwned>
				void name_inter(const char* name, uint32_t len) {
					//! We can't name pairs
					GAIA_ASSERT(!m_entity.pair());
					if (m_entity.pair())
						return;

					// When nullptr is passed for the name it means the user wants to delete the current one
					if (name == nullptr) {
						GAIA_ASSERT(len == 0);
						del_name();
						return;
					}

					GAIA_ASSERT(len < ComponentCacheItem::MaxNameLength);

					// Make sure the name does not contain a dot because this character is reserved for
					// hierarchical lookups, e.g. "parent.child.subchild".
					GAIA_FOR(len) {
						const bool hasInvalidCharacter = name[i] == '.';
						GAIA_ASSERT(!hasInvalidCharacter && "Character '.' can't be used in entity names");
						if (hasInvalidCharacter)
							return;
					}

					EntityNameLookupKey key(
							name, len == 0 ? (uint32_t)GAIA_STRLEN(name, ComponentCacheItem::MaxNameLength) : len, IsOwned);

					// Make sure the name is unique. Ignore setting the same name twice on the same entity.
					// If it is not, there is nothing to do.
					auto it = m_world.m_nameToEntity.find(key);
					if (it == m_world.m_nameToEntity.end()) {
						// If we already had some name, remove the pair from the map first.
						if (m_targetNameKey.str() != nullptr) {
							del_name_inter(m_targetNameKey);
						} else {
							const auto compIdx = core::get_index(m_pArchetypeSrc->ids_view(), GAIA_ID(EntityDesc));
							if (compIdx != BadIndex) {
								auto* pDesc = reinterpret_cast<EntityDesc*>(m_pChunkSrc->comp_ptr_mut(compIdx, m_rowSrc));
								GAIA_ASSERT(core::check_alignment(pDesc));
								if (pDesc->name != nullptr) {
									del_name_inter(EntityNameLookupKey(pDesc->name, pDesc->name_len, 0));
									pDesc->name = nullptr;
								}
							} else {
								// Make sure EntityDesc is added to the entity.
								add_inter(GAIA_ID(EntityDesc));
							}
						}

						// Insert the new pair
						it = m_world.m_nameToEntity.emplace(key, m_entity).first;
					} else {
#if GAIA_ASSERT_ENABLED
						if (it->second != m_entity && World::s_enableUniqueNameDuplicateAssert)
							GAIA_ASSERT(false && "Trying to set non-unique name for an entity");
#endif

						// Attempts to set the same name again, or not a unique name, will be dropped.
						return;
					}

					if constexpr (IsOwned) {
						// Allocate enough storage for the name
						char* entityStr = (char*)mem::mem_alloc(key.len() + 1);
						memcpy((void*)entityStr, (const void*)name, key.len());
						entityStr[key.len()] = 0;

						m_targetNameKey = EntityNameLookupKey(entityStr, key.len(), 1, {key.hash()});

						// Update the map so it points to the newly allocated string.
						// We replace the pointer we provided in try_emplace with an internally allocated string.
						auto p = robin_hood::pair(std::make_pair(m_targetNameKey, m_entity));
						it->swap(p);
					} else {
						m_targetNameKey = key;

						// We tell the map the string is non-owned.
						auto p = robin_hood::pair(std::make_pair(key, m_entity));
						it->swap(p);
					}

					m_world.invalidate_scope_path_cache();
				}

				template <bool IsOwned>
				void alias_inter(const char* alias, uint32_t len) {
					//! We can't create an alias for pairs
					GAIA_ASSERT(!m_entity.pair());
					if (m_entity.pair())
						return;

					// When nullptr is passed for the alias it means the user wants to delete the current one
					if (alias == nullptr) {
						GAIA_ASSERT(len == 0);
						del_alias();
						return;
					}

					GAIA_ASSERT(len < ComponentCacheItem::MaxNameLength);

					// Make sure the name does not contain a dot because this character is reserved for
					// hierarchical lookups, e.g. "parent.child.subchild".
					GAIA_FOR(len) {
						const bool hasInvalidCharacter = alias[i] == '.';
						GAIA_ASSERT(!hasInvalidCharacter && "Character '.' can't be used in entity aliases");
						if (hasInvalidCharacter)
							return;
					}

					EntityNameLookupKey key(
							alias, len == 0 ? (uint32_t)GAIA_STRLEN(alias, ComponentCacheItem::MaxNameLength) : len, IsOwned);

					auto it = m_world.m_aliasToEntity.find(key);
					if (it == m_world.m_aliasToEntity.end()) {
						// If we already had some alias, remove the pair from the map first.
						if (m_targetAliasKey.str() != nullptr) {
							del_alias_inter(m_targetAliasKey);
						} else {
							const auto compIdx = core::get_index(m_pArchetypeSrc->ids_view(), GAIA_ID(EntityDesc));
							if (compIdx != BadIndex) {
								auto* pDesc = reinterpret_cast<EntityDesc*>(m_pChunkSrc->comp_ptr_mut(compIdx, m_rowSrc));
								GAIA_ASSERT(core::check_alignment(pDesc));
								if (pDesc->alias != nullptr) {
									del_alias_inter(EntityNameLookupKey(pDesc->alias, pDesc->alias_len, 0));
									pDesc->alias = nullptr;
								}
							} else {
								// Make sure EntityDesc is added to the entity.
								add_inter(GAIA_ID(EntityDesc));
							}
						}

						it = m_world.m_aliasToEntity.emplace(key, m_entity).first;
					} else {
#if GAIA_ASSERT_ENABLED
						if (it->second != m_entity && World::s_enableUniqueNameDuplicateAssert)
							GAIA_ASSERT(false && "Trying to set non-unique alias for an entity");
#endif

						// Attempts to set the same alias again, or not a unique alias, will be dropped.
						return;
					}

					if constexpr (IsOwned) {
						// Allocate enough storage for the alias
						char* aliasStr = (char*)mem::mem_alloc(key.len() + 1);
						memcpy((void*)aliasStr, (const void*)alias, key.len());
						aliasStr[key.len()] = 0;

						m_targetAliasKey = EntityNameLookupKey(aliasStr, key.len(), 1, {key.hash()});

						// Update the map so it points to the newly allocated string.
						// We replace the pointer we provided in try_emplace with an internally allocated string.
						auto p = robin_hood::pair(std::make_pair(m_targetAliasKey, m_entity));
						it->swap(p);
					} else {
						m_targetAliasKey = key;

						// We tell the map the string is non-owned.
						auto p = robin_hood::pair(std::make_pair(key, m_entity));
						it->swap(p);
					}
				}
			};

			//----------------------------------------------------------------------

			//! Returns mutable access to the world component cache.
			//! \return Component cache owned by this world.
			GAIA_NODISCARD ComponentCache& comp_cache_mut() {
				return m_compCache;
			}

			//! Returns read-only access to the world component cache.
			//! \return Component cache owned by this world.
			GAIA_NODISCARD const ComponentCache& comp_cache() const {
				return m_compCache;
			}

			//----------------------------------------------------------------------

			//! Finds a component entity by its exact registered symbol.
			//! \param symbol Registered component symbol.
			//! \param len String length. If zero, the length is calculated.
			//! \return Matching component entity. EntityBad when no exact symbol match exists.
			GAIA_NODISCARD Entity symbol(const char* symbol, uint32_t len = 0) const {
				if (symbol == nullptr || symbol[0] == 0)
					return EntityBad;

				const auto* pItem = comp_cache().symbol(symbol, len);
				return pItem != nullptr ? pItem->entity : EntityBad;
			}

			//! Returns the registered symbol name for a component entity.
			//! \param component Component entity.
			//! \return Registered component symbol. Empty view when @a component is not a cached component.
			GAIA_NODISCARD util::str_view symbol(Entity component) const {
				const auto* pItem = comp_cache().find(component);
				return pItem != nullptr ? comp_cache().symbol_name(*pItem) : util::str_view{};
			}

			//! Finds a component entity by its exact scoped path.
			//! \param path Exact component path.
			//! \param len String length. If zero, the length is calculated.
			//! \return Matching component entity. EntityBad when no exact path match exists.
			GAIA_NODISCARD Entity path(const char* path, uint32_t len = 0) const {
				if (path == nullptr || path[0] == 0)
					return EntityBad;

				const auto* pItem = comp_cache().path(path, len);
				return pItem != nullptr ? pItem->entity : EntityBad;
			}

			//! Returns the scoped path name for a component entity.
			//! \param component Component entity.
			//! \return Scoped component path. Empty view when no path is assigned or @a component is not cached.
			GAIA_NODISCARD util::str_view path(Entity component) const {
				const auto* pItem = comp_cache().find(component);
				return pItem != nullptr ? comp_cache().path_name(*pItem) : util::str_view{};
			}

			//! Assigns a scoped path name to a component entity.
			//! \param component Component entity.
			//! \param path Path to assign. Pass nullptr to clear the current path.
			//! \param len String length. If zero, the length is calculated.
			//! \return True when the path metadata changed, false otherwise.
			bool path(Entity component, const char* path, uint32_t len = 0) {
				auto* pItem = comp_cache_mut().find(component);
				return pItem != nullptr ? comp_cache_mut().path(*pItem, path, len) : false;
			}

			//! Finds an entity by its exact alias.
			//! \param alias Exact entity alias.
			//! \param len String length. If zero, the length is calculated.
			//! \return Matching entity. EntityBad when no exact alias match exists.
			GAIA_NODISCARD Entity alias(const char* alias, uint32_t len = 0) const {
				if (alias == nullptr || alias[0] == 0)
					return EntityBad;

				const auto l = len == 0 ? (uint32_t)GAIA_STRLEN(alias, ComponentCacheItem::MaxNameLength) : len;
				GAIA_ASSERT(l < ComponentCacheItem::MaxNameLength);
				const auto it = m_aliasToEntity.find(EntityNameLookupKey(alias, l, 0));
				return it != m_aliasToEntity.end() ? it->second : EntityBad;
			}

			//! Returns the alias assigned to an entity.
			//! \param entity Entity.
			//! \return Alias. Empty view when no alias is assigned.
			GAIA_NODISCARD util::str_view alias(Entity entity) const {
				if (entity.pair())
					return {};

				const auto& ec = m_recs.entities[entity.id()];
				const auto compIdx = core::get_index(ec.pChunk->ids_view(), GAIA_ID(EntityDesc));
				if (compIdx == BadIndex)
					return {};

				const auto* pDesc = reinterpret_cast<const EntityDesc*>(ec.pChunk->comp_ptr(compIdx, ec.row));
				GAIA_ASSERT(core::check_alignment(pDesc));
				return {pDesc->alias, pDesc->alias_len};
			}

			//! Assigns an alias name to an entity.
			//! \param entity Entity.
			//! \param alias Alias to assign. Pass nullptr to clear the current alias.
			//! \param len String length. If zero, the length is calculated.
			//! \return True when the alias metadata changed, false otherwise.
			bool alias(Entity entity, const char* alias, uint32_t len = 0) {
				if (!valid(entity) || entity.pair())
					return false;

				const auto before = this->alias(entity);
				EntityBuilder(*this, entity).alias(alias, len);
				const auto after = this->alias(entity);
				if (alias == nullptr)
					return !before.empty() && after.empty();

				const auto l = len == 0 ? (uint32_t)GAIA_STRLEN(alias, ComponentCacheItem::MaxNameLength) : len;
				return after == util::str_view(alias, l);
			}

			//! Assigns an alias name to an entity without copying the string.
			//! \param entity Entity.
			//! \param alias Alias to assign. Pass nullptr to clear the current alias.
			//! \param len String length. If zero, the length is calculated.
			//! \return True when the alias metadata changed, false otherwise.
			bool alias_raw(Entity entity, const char* alias, uint32_t len = 0) {
				if (!valid(entity) || entity.pair())
					return false;

				const auto before = this->alias(entity);
				EntityBuilder(*this, entity).alias_raw(alias, len);
				const auto after = this->alias(entity);
				if (alias == nullptr)
					return !before.empty() && after.empty();

				const auto l = len == 0 ? (uint32_t)GAIA_STRLEN(alias, ComponentCacheItem::MaxNameLength) : len;
				return after == util::str_view(alias, l);
			}

			//! Returns the preferred display name for a entity.
			//! This is intended for diagnostics and other pretty output, not as a stable identity key.
			//! \param entity Entity.
			//! \return Display name used for user-facing output. Empty view when @a entity is not cached.
			GAIA_NODISCARD util::str_view display_name(Entity entity) const {
				const auto* pItem = comp_cache().find(entity);
				if (pItem == nullptr)
					return {};

				const auto aliasValue = alias(entity);
				if (!aliasValue.empty())
					return aliasValue;

				const auto pathValue = path(entity);
				if (!pathValue.empty()) {
					const auto symbolEntity = symbol(pathValue.data(), pathValue.size());
					if (symbolEntity == EntityBad || symbolEntity == entity)
						return pathValue;
				}

				return symbol(entity);
			}

			//----------------------------------------------------------------------

		private:
			void invalidate_scope_path_cache() const {
				m_componentScopePathCache.clear();
				m_componentScopePathCacheEntity = EntityBad;
				m_componentScopePathCacheValid = false;
			}

			//! Builds a dotted scope path for @a scope by walking its ChildOf chain.
			//! \param scope Scope entity to inspect.
			//! \param out Receives the dotted path when successful.
			//! \return True when a full named scope path could be built, false otherwise.
			GAIA_NODISCARD bool build_scope_path(Entity scope, util::str& out) const {
				out.clear();
				if (!valid(scope) || scope.pair())
					return false;

				cnt::darray_ext<util::str_view, 16> segments;
				auto curr = scope;
				while (curr != EntityBad) {
					const auto currName = name(curr);
					if (currName.empty()) {
						out.clear();
						return false;
					}

					segments.push_back(currName);
					curr = target(curr, ChildOf);
				}

				if (segments.empty())
					return false;

				uint32_t totalLen = 0;
				for (auto segment: segments)
					totalLen += segment.size();
				totalLen += (uint32_t)segments.size() - 1;

				out.reserve(totalLen);
				for (uint32_t i = (uint32_t)segments.size(); i > 0; --i) {
					if (!out.empty())
						out.append('.');
					out.append(segments[i - 1]);
				}

				return true;
			}

			//! Builds a dotted scope path for the currently active component scope.
			//! \param out Receives the dotted path when successful.
			//! \return True when the current component scope is active and fully named, false otherwise.
			GAIA_NODISCARD bool current_scope_path(util::str& out) const {
				if (m_componentScope == EntityBad) {
					invalidate_scope_path_cache();
					out.clear();
					return false;
				}

				if (m_componentScopePathCacheValid && m_componentScopePathCacheEntity == m_componentScope) {
					out.assign(m_componentScopePathCache.view());
					return true;
				}

				if (!build_scope_path(m_componentScope, out)) {
					invalidate_scope_path_cache();
					return false;
				}

				m_componentScopePathCache.assign(out.view());
				m_componentScopePathCacheEntity = m_componentScope;
				m_componentScopePathCacheValid = true;
				return true;
			}

			template <typename Func>
			bool find_component_in_scope_chain_inter(Entity scopeEntity, const char* name, uint32_t len, Func&& func) const {
				if (scopeEntity == EntityBad)
					return false;

				util::str scopePath;
				if (!build_scope_path(scopeEntity, scopePath))
					return false;

				util::str scopedName;
				scopedName.reserve(scopePath.size() + 1 + len);

				while (!scopePath.empty()) {
					scopedName.clear();
					scopedName.append(scopePath.view());
					scopedName.append('.');
					scopedName.append(name, len);

					if (const auto* pItem = m_compCache.path(scopedName.data(), (uint32_t)scopedName.size()); pItem != nullptr) {
						if (func(*pItem))
							return true;
					}

					const auto parentSepIdx = scopePath.view().find_last_of('.');
					if (parentSepIdx == BadIndex)
						break;

					scopePath.assign(util::str_view(scopePath.data(), parentSepIdx));
				}

				return false;
			}

			//! Resolves a component name using world-aware scope rules.
			//! Exact path lookup is used for dotted names. Unqualified names first search the current
			//! component scope and its parents, then each configured lookup path scope in order, and
			//! finally fall back to exact symbol, path, unique short-symbol and alias lookup.
			//! \param name Component name to resolve.
			//! \param len Name length. If zero, the length is calculated.
			//! \return Matching component cache item, or nullptr when no match exists.
			GAIA_NODISCARD const ComponentCacheItem* resolve_component_name_inter(const char* name, uint32_t len = 0) const {
				GAIA_ASSERT(name != nullptr);

				const auto l = len == 0 ? (uint32_t)GAIA_STRLEN(name, ComponentCacheItem::MaxNameLength) : len;
				GAIA_ASSERT(l < ComponentCacheItem::MaxNameLength);
				const bool isPath = memchr(name, '.', l) != nullptr;
				const bool isSymbol = memchr(name, ':', l) != nullptr;

				if (isPath) {
					if (const auto* pItem = m_compCache.path(name, l); pItem != nullptr)
						return pItem;
				}

				if (!isPath && !isSymbol) {
					const auto* pFound = (const ComponentCacheItem*)nullptr;
					const auto findAndStore = [&](const ComponentCacheItem& item) {
						pFound = &item;
						return true;
					};
					if (find_component_in_scope_chain_inter(m_componentScope, name, l, findAndStore))
						return pFound;

					for (const auto scopeEntity: m_componentLookupPath) {
						if (scopeEntity == m_componentScope)
							continue;
						if (find_component_in_scope_chain_inter(scopeEntity, name, l, findAndStore))
							return pFound;
					}
				}

				if (const auto* pItem = m_compCache.symbol(name, l); pItem != nullptr)
					return pItem;

				if (!isPath) {
					if (const auto* pItem = m_compCache.path(name, l); pItem != nullptr)
						return pItem;
					if (!isSymbol) {
						if (const auto* pItem = m_compCache.short_symbol(name, l); pItem != nullptr)
							return pItem;
					}
				}

				const auto aliasEntity = alias(name, l);
				return aliasEntity != EntityBad ? m_compCache.find(aliasEntity) : nullptr;
			}

		public:
			//----------------------------------------------------------------------

			//! Checks if @a entity is valid.
			//! \param entity Checked entity.
			//! \return True if the entity is valid. False otherwise.
			GAIA_NODISCARD bool valid(Entity entity) const {
				return entity.pair() //
									 ? valid_pair(entity)
									 : valid_entity(entity);
			}

			//----------------------------------------------------------------------

			//! Returns the entity located at the index @a id
			//! \param id Entity id
			//! \return Entity
			GAIA_NODISCARD Entity get(EntityId id) const {
				// Cleanup, observer propagation, and wildcard expansion can briefly encounter stale ids.
				// Treat those as absent instead of crashing the world.
				if (!valid_entity_id(id))
					return EntityBad;

				const auto& ec = m_recs.entities[id];
				return Entity(id, ec.data.gen, (bool)ec.data.ent, (bool)ec.data.pair, (EntityKind)ec.data.kind);
			}

			//! Returns the entity for @a id when it is still live, or EntityBad for stale cleanup-time ids.
			GAIA_NODISCARD Entity try_get(EntityId id) const {
				return valid_entity_id(id) ? get(id) : EntityBad;
			}

			template <typename T>
			GAIA_NODISCARD Entity get() const {
				static_assert(!is_pair<T>::value, "Pairs can't be registered as components");

				using CT = component_type_t<T>;
				using FT = typename CT::TypeFull;

				const auto* pItem = comp_cache().find<FT>();
				GAIA_ASSERT(pItem != nullptr);
				return pItem->entity;
			}

			//----------------------------------------------------------------------

			//! Starts a bulk add/remove operation on @a entity.
			//! \param entity Entity
			//! \return EntityBuilder
			//! \warning It is expected @a entity is valid. Undefined behavior otherwise.
			EntityBuilder build(Entity entity) {
				return EntityBuilder(*this, entity);
			}

			//! Creates a new empty entity
			//! \param kind Entity kind. Generic entity by default.
			//! \return New entity
			GAIA_NODISCARD Entity add(EntityKind kind = EntityKind::EK_Gen) {
				return add(*m_pEntityArchetype, true, false, kind);
			}

			//! Creates a new prefab entity.
			GAIA_NODISCARD Entity prefab(EntityKind kind = EntityKind::EK_Gen) {
				const auto entity = add(kind);
				add(entity, Prefab);
				return entity;
			}

			//! Creates @a count new empty entities
			//! \param count Number of enities to create
			//! \param func Functor to execute every time an entity is added
			template <typename Func = TFunc_Void_With_Entity>
			void add_n(uint32_t count, Func func = func_void_with_entity) {
				add_entity_n(*m_pEntityArchetype, count, func);
			}

			//! Creates @a count of entities of the same archetype as @a entity.
			//! \param entity Source entity to copy
			//! \param count Number of enities to create
			//! \param func Functor to execute every time an entity is added
			//! \note Similar to copy_n, but keeps component values uninitialized or default-initialized
			//!       if they provide a constructor
			template <typename Func = TFunc_Void_With_Entity>
			void add_n(Entity entity, uint32_t count, Func func = func_void_with_entity) {
				auto& ec = m_recs.entities[entity.id()];

				GAIA_ASSERT(ec.pArchetype != nullptr);
				GAIA_ASSERT(ec.pChunk != nullptr);

				add_entity_n(*ec.pArchetype, count, func);
			}

			//! Creates a new component if not found already.
			//! \tparam T Component
			//! \return Component cache item of the component
			template <typename T>
			GAIA_NODISCARD const ComponentCacheItem& add() {
				static_assert(!is_pair<T>::value, "Pairs can't be registered as components");

				using CT = component_type_t<T>;
				using FT = typename CT::TypeFull;
				constexpr auto kind = CT::Kind;

				const auto* pItem = comp_cache().find<FT>();
				if (pItem != nullptr)
					return *pItem;

				const auto entity = add(*m_pCompArchetype, false, false, kind);
				util::str scopePath;
				(void)current_scope_path(scopePath);

				const auto& item = comp_cache_mut().add<FT>(entity, scopePath.view());
				sset<Component>(item.entity) = item.comp;
				// Register the default component symbol through the normal entity naming path.
				name_raw(item.entity, item.name.str(), item.name.len());
#if GAIA_ECS_AUTO_COMPONENT_SCHEMA
				auto_populate_component_schema<FT>(comp_cache_mut().get(item.entity));
#endif

				return item;
			}

			//! Creates a new runtime component if not found already.
			//! \param name Component name.
			//! \param size Component size in bytes.
			//! \param storageType Data storage type.
			//! \param alig Component alignment in bytes.
			//! \param soa Number of SoA items (0 for AoS).
			//! \param pSoaSizes SoA item sizes, must contain at least @a soa values when @a soa > 0.
			//! \param hashLookup Optional lookup hash. If zero, hash(name) is used.
			//! \param kind Entity kind (Gen by default, Uni supported).
			//! \return Component cache item of the component.
			GAIA_NODISCARD const ComponentCacheItem&
			add(const char* name, uint32_t size, DataStorageType storageType, uint32_t alig = 1, uint32_t soa = 0,
					const uint8_t* pSoaSizes = nullptr, ComponentLookupHash hashLookup = {},
					EntityKind kind = EntityKind::EK_Gen) {
				GAIA_ASSERT(name != nullptr);

				const auto len = (uint32_t)GAIA_STRLEN(name, ComponentCacheItem::MaxNameLength);
				GAIA_ASSERT(len > 0 && len < ComponentCacheItem::MaxNameLength);

				if (const auto* pItem = comp_cache().symbol(name, len); pItem != nullptr)
					return *pItem;

				const auto entity = add(*m_pCompArchetype, false, false, kind);
				util::str scopePath;
				(void)current_scope_path(scopePath);
				const auto& item = comp_cache_mut().add(
						entity, name, len, size, storageType, alig, soa, pSoaSizes, hashLookup, scopePath.view());
				{
					auto& ec = m_recs.entities[item.entity.id()];
					const auto compIdx = core::get_index(ec.pArchetype->ids_view(), GAIA_ID(Component));
					GAIA_ASSERT(compIdx != BadIndex);
					auto* pComp = reinterpret_cast<Component*>(ec.pChunk->comp_ptr_mut(compIdx, ec.row));
					*pComp = item.comp;
				}
				// Register the default component symbol through the normal entity naming path.
				name_raw(item.entity, item.name.str(), item.name.len());
				return item;
			}

			//! Attaches entity @a object to entity @a entity.
			//! \param entity Source entity
			//! \param object Added entity
			//! \warning It is expected both @a entity and @a object are valid. Undefined behavior otherwise.
			void add(Entity entity, Entity object) {
#if GAIA_ASSERT_ENABLED
				if (!object.pair()) {
					const auto* pItem = comp_cache().find(object);
					if (pItem != nullptr && pItem->entity == object && is_out_of_line_component(object))
						GAIA_ASSERT2(
								false, "Out-of-line runtime components require an explicit typed value when added by entity id");
				}
#endif
				EntityBuilder(*this, entity).add(object);
			}

			//! Creates a new entity relationship pair
			//! \param entity Source entity
			//! \param pair Pair
			//! \warning It is expected both @a entity and the entities forming the relationship are valid.
			//!          Undefined behavior otherwise.
			void add(Entity entity, Pair pair) {
				EntityBuilder(*this, entity).add(pair);
			}

			//! Attaches a new component @a T to @a entity.
			//! \tparam T Component
			//! \param entity Entity
			//! \warning It is expected the component is not present on @a entity yet. Undefined behavior otherwise.
			//! \warning It is expected @a entity is valid. Undefined behavior otherwise.
			template <typename T>
			void add(Entity entity) {
				using FT = typename component_type_t<T>::TypeFull;
				const auto& item = add<FT>();
				if constexpr (supports_out_of_line_component<FT>()) {
					if (is_out_of_line_component(item.entity)) {
						if (!is_non_fragmenting_out_of_line_component(item.entity)) {
							(void)sparse_component_store_mut<FT>(item.entity).add(entity);
							EntityBuilder(*this, entity).add<T>();
							return;
						}
#if GAIA_OBSERVERS_ENABLED
						auto addDiffCtx = m_observers.prepare_diff(
								*this, ObserverEvent::OnAdd, EntitySpan{&item.entity, 1}, EntitySpan{&entity, 1});
#endif
						(void)sparse_component_store_mut<FT>(item.entity).add(entity);
						notify_add_single(entity, item.entity);
#if GAIA_OBSERVERS_ENABLED
						m_observers.finish_diff(*this, GAIA_MOV(addDiffCtx));
#endif
						return;
					}
				}

				EntityBuilder(*this, entity).add<T>();
			}

			//! Attaches @a object to @a entity. Also sets its value.
			//! \param object Object
			//! \param entity Entity
			//! \param value Value to set for the object
			//! \warning It is expected the component is not present on @a entity yet. Undefined behavior otherwise.
			//! \warning It is expected @a entity is valid. Undefined behavior otherwise.
			//! \warning It is expected @a object is valid. Undefined behavior otherwise.
			template <typename T>
			void add(Entity entity, Entity object, T&& value) {
				static_assert(core::is_raw_v<T>);

				if constexpr (supports_out_of_line_component<typename component_type_t<T>::TypeFull>()) {
					using FT = typename component_type_t<T>::TypeFull;
					if (can_use_out_of_line_component<FT>(object)) {
						auto& data = sparse_component_store_mut<FT>(object).add(entity);
						data = GAIA_FWD(value);

						if (!is_non_fragmenting_out_of_line_component(object)) {
#if GAIA_OBSERVERS_ENABLED
							auto addDiffCtx =
									m_observers.prepare_diff(*this, ObserverEvent::OnAdd, EntitySpan{&object, 1}, EntitySpan{&entity, 1});
#endif
							EntityBuilder eb(*this, entity);
							eb.add_inter_init(object);
							eb.commit();
							notify_add_single(entity, object);
#if GAIA_OBSERVERS_ENABLED
							m_observers.finish_diff(*this, GAIA_MOV(addDiffCtx));
#endif
							return;
						}
#if GAIA_OBSERVERS_ENABLED
						auto addDiffCtx =
								m_observers.prepare_diff(*this, ObserverEvent::OnAdd, EntitySpan{&object, 1}, EntitySpan{&entity, 1});
#endif
						notify_add_single(entity, object);
#if GAIA_OBSERVERS_ENABLED
						m_observers.finish_diff(*this, GAIA_MOV(addDiffCtx));
#endif
						return;
					}
				}

				EntityBuilder eb(*this, entity);
				eb.add_inter_init(object);
				eb.commit();

				const auto& ec = fetch(entity);
				// Make sure the idx is 0 for unique entities
				const auto idx = uint16_t(ec.row * (1U - (uint32_t)object.kind()));
				ComponentSetter{{ec.pChunk, idx}}.set(object, GAIA_FWD(value));
				notify_add_single(entity, object);
			}

			//! Attaches a new component @a T to @a entity. Also sets its value.
			//! \tparam T Component
			//! \param entity Entity
			//! \param value Value to set for the component
			//! \warning It is expected the component is not present on @a entity yet. Undefined behavior otherwise.
			//! \warning It is expected @a entity is valid. Undefined behavior otherwise.
			template <typename T, typename U = typename actual_type_t<T>::Type>
			void add(Entity entity, U&& value) {
				using FT = typename component_type_t<T>::TypeFull;
				if constexpr (!is_pair<FT>::value && supports_out_of_line_component<FT>()) {
					const auto& item = add<FT>();
					if (is_out_of_line_component(item.entity)) {
						auto& data = sparse_component_store_mut<FT>(item.entity).add(entity);
						data = GAIA_FWD(value);

						if (!is_non_fragmenting_out_of_line_component(item.entity)) {
							EntityBuilder builder(*this, entity);
							builder.add(item.entity);
							builder.commit();
							return;
						}
#if GAIA_OBSERVERS_ENABLED
						auto addDiffCtx = m_observers.prepare_diff(
								*this, ObserverEvent::OnAdd, EntitySpan{&item.entity, 1}, EntitySpan{&entity, 1});
#endif
						notify_add_single(entity, item.entity);
#if GAIA_OBSERVERS_ENABLED
						m_observers.finish_diff(*this, GAIA_MOV(addDiffCtx));
#endif
						return;
					}
				}

				EntityBuilder builder(*this, entity);
				auto object = builder.register_component<T>();
				builder.add(object);
				builder.commit();

				const auto& ec = m_recs.entities[entity.id()];
				// Make sure the idx is 0 for unique entities
				const auto idx = uint16_t(ec.row * (1U - (uint32_t)object.kind()));
				ComponentSetter{{ec.pChunk, idx}}.set<T>(GAIA_FWD(value));
			}

			//! Materializes an inherited id as directly owned storage on @a entity.
			//! \return True when a local override was created. False if nothing changed.
			GAIA_NODISCARD bool override(Entity entity, Entity object) {
				return override_inter(entity, object);
			}

			//! Materializes an inherited pair as directly owned storage on @a entity.
			//! \return True when a local override was created. False if nothing changed.
			GAIA_NODISCARD bool override(Entity entity, Pair pair) {
				return override_inter(entity, (Entity)pair);
			}

			//! Materializes an inherited typed component as directly owned storage on @a entity.
			//! \return True when a local override was created. False if nothing changed.
			template <typename T>
			GAIA_NODISCARD bool override(Entity entity) {
				static_assert(!is_pair<T>::value);
				using FT = typename component_type_t<T>::TypeFull;
				const auto& item = add<FT>();

				if constexpr (supports_out_of_line_component<FT>()) {
					if (is_out_of_line_component(item.entity)) {
						if (has_direct(entity, item.entity))
							return false;

						const auto inheritedOwner = inherited_id_owner(entity, item.entity);
						if (inheritedOwner == EntityBad)
							return false;

						auto* pStore = sparse_component_store<FT>(item.entity);
						GAIA_ASSERT(pStore != nullptr);
						auto& data = sparse_component_store_mut<FT>(item.entity).add(entity);
						data = pStore->get(inheritedOwner);

						if (!is_non_fragmenting_out_of_line_component(item.entity)) {
							EntityBuilder eb(*this, entity);
							eb.add_inter_init(item.entity);
							eb.commit();
						}
						return true;
					}
				}

				return override_inter(entity, item.entity);
			}

			//! Materializes an inherited typed component associated with @a object on @a entity.
			//! \return True when a local override was created. False if nothing changed.
			template <typename T>
			GAIA_NODISCARD bool override(Entity entity, Entity object) {
				static_assert(!is_pair<T>::value);
				using FT = typename component_type_t<T>::TypeFull;

				if constexpr (supports_out_of_line_component<FT>()) {
					if (can_use_out_of_line_component<FT>(object)) {
						if (has_direct(entity, object))
							return false;

						const auto inheritedOwner = inherited_id_owner(entity, object);
						if (inheritedOwner == EntityBad)
							return false;

						auto* pStore = sparse_component_store<FT>(object);
						GAIA_ASSERT(pStore != nullptr);
						auto& data = sparse_component_store_mut<FT>(object).add(entity);
						data = pStore->get(inheritedOwner);

						if (!is_non_fragmenting_out_of_line_component(object)) {
							EntityBuilder eb(*this, entity);
							eb.add_inter_init(object);
							eb.commit();
						}
						return true;
					}
				}

				return override_inter(entity, object);
			}

			//----------------------------------------------------------------------

			//! Removes any component or entity attached to @a entity.
			//! \param entity Entity we want to remove any attached component or entity from
			//! \warning It is expected @a entity is not a pair. Undefined behavior otherwise.
			//! \warning It is expected @a entity is valid. Undefined behavior otherwise.
			void clear(Entity entity) {
				GAIA_ASSERT(!entity.pair());
				GAIA_ASSERT(valid(entity));

				EntityBuilder eb(*this, entity);

				// Remove back to front because it's better for the archetype graph
				auto ids = eb.m_pArchetype->ids_view();
				for (uint32_t i = (uint32_t)ids.size() - 1; i != (uint32_t)-1; --i)
					eb.del(ids[i]);

				eb.commit();
			}

			//----------------------------------------------------------------------

			//! Creates a new entity by cloning an already existing one. Does not trigger observers.
			//! \param srcEntity Entity to clone
			//! \return New entity
			//! \warning It is expected @a srcEntity is valid. Undefined behavior otherwise.
			//! \warning If EntityDesc is present on @a srcEntity, it is not copied because names are
			//!          expected to be unique. Instead, the copied entity will be a part of an archetype
			//!          without EntityDesc and any calls to World::name(copiedEntity) will return an empty view.
			GAIA_NODISCARD Entity copy(Entity srcEntity) {
				GAIA_ASSERT(!srcEntity.pair());
				GAIA_ASSERT(valid(srcEntity));

				auto& ec = m_recs.entities[srcEntity.id()];
				GAIA_ASSERT(ec.pArchetype != nullptr);
				GAIA_ASSERT(ec.pChunk != nullptr);

				auto* pDstArchetype = ec.pArchetype;
				Entity dstEntity;

				// Names have to be unique so if we see that EntityDesc is present during copy
				// we navigate towards a version of the archetype without the EntityDesc.
				if (pDstArchetype->has<EntityDesc>()) {
					pDstArchetype = foc_archetype_del(pDstArchetype, GAIA_ID(EntityDesc));

					dstEntity = add(*pDstArchetype, srcEntity.entity(), srcEntity.pair(), srcEntity.kind());
					auto& ecDst = m_recs.entities[dstEntity.id()];
					Chunk::copy_foreign_entity_data(ec.pChunk, ec.row, ecDst.pChunk, ecDst.row);
				} else {
					// No description associated with the entity, direct copy is possible
					dstEntity = add(*pDstArchetype, srcEntity.entity(), srcEntity.pair(), srcEntity.kind());
					Chunk::copy_entity_data(srcEntity, dstEntity, m_recs);
				}

				copy_sparse_entity_data(srcEntity, dstEntity, [](Entity) {
					return true;
				});

				return dstEntity;
			}

			//! Creates @a count new entities by cloning an already existing one.
			//! \param entity Entity to clone
			//! \param count Number of clones to make
			//! \param func Functor executed every time a copy is created.
			//!             It can be either void(ecs::Entity) or void(ecs::CopyIter&).
			//! \warning It is expected @a entity is valid generic entity. Undefined behavior otherwise.
			//! \warning If EntityDesc is present on @a entity, it is not copied because names are
			//!          expected to be unique. Instead, the copied entity will be a part of an archetype
			//!          without EntityDesc and any calls to World::name(copiedEntity) will return an empty view.
			template <typename Func = TFunc_Void_With_Entity>
			void copy_n(Entity entity, uint32_t count, Func func = func_void_with_entity) {
				copy_n_inter(entity, count, func, EntitySpan{});
			}

#if GAIA_OBSERVERS_ENABLED
			//! Creates a new entity by cloning an already existing one. Trigger observers if necessary.
			//! \param srcEntity Entity to clone
			//! \return New entity
			//! \warning It is expected @a srcEntity is valid. Undefined behavior otherwise.
			//! \warning If EntityDesc is present on @a srcEntity, it is not copied because names are
			//!          expected to be unique. Instead, the copied entity will be a part of an archetype
			//!          without EntityDesc and any calls to World::name(copiedEntity) will return an empty view.
			GAIA_NODISCARD Entity copy_ext(Entity srcEntity) {
				GAIA_ASSERT(!srcEntity.pair());
				GAIA_ASSERT(valid(srcEntity));

				auto& ec = m_recs.entities[srcEntity.id()];
				GAIA_ASSERT(ec.pArchetype != nullptr);
				GAIA_ASSERT(ec.pChunk != nullptr);

				auto* pDstArchetype = ec.pArchetype;
				// Names have to be unique so if we see that EntityDesc is present during copy
				// we navigate towards a version of the archetype without the EntityDesc.
				const bool hasEntityDesc = pDstArchetype->has<EntityDesc>();
				if (hasEntityDesc)
					pDstArchetype = foc_archetype_del(pDstArchetype, GAIA_ID(EntityDesc));

				const auto archetypeIdCount = (uint32_t)pDstArchetype->ids_view().size();
				const auto sparseIdCount = copied_sparse_id_count(srcEntity, [](Entity) {
					return true;
				});
				const auto addedIdCount = archetypeIdCount + sparseIdCount;
				auto* pAddedIds = addedIdCount != 0U ? (Entity*)alloca(sizeof(Entity) * addedIdCount) : nullptr;
				write_archetype_ids(*pDstArchetype, pAddedIds);
				write_copied_sparse_ids(
						srcEntity,
						[](Entity) {
							return true;
						},
						pAddedIds + archetypeIdCount);
	#if GAIA_OBSERVERS_ENABLED
				auto addDiffCtx = m_observers.prepare_diff_add_new(*this, EntitySpan{pAddedIds, addedIdCount});
	#endif

				Entity dstEntity;
				if (hasEntityDesc) {
					dstEntity = add(*pDstArchetype, srcEntity.entity(), srcEntity.pair(), srcEntity.kind());
					auto& ecDst = m_recs.entities[dstEntity.id()];
					Chunk::copy_foreign_entity_data(ec.pChunk, ec.row, ecDst.pChunk, ecDst.row);
				} else {
					// No description associated with the entity, direct copy is possible
					dstEntity = add(*pDstArchetype, srcEntity.entity(), srcEntity.pair(), srcEntity.kind());
					Chunk::copy_entity_data(srcEntity, dstEntity, m_recs);
				}

				(void)copy_sparse_entity_data(srcEntity, dstEntity, [](Entity) {
					return true;
				});
				m_observers.append_diff_targets(*this, addDiffCtx, EntitySpan{&dstEntity, 1});

				m_observers.on_add(*this, *pDstArchetype, EntitySpan{pAddedIds, addedIdCount}, EntitySpan{&dstEntity, 1});
	#if GAIA_OBSERVERS_ENABLED
				m_observers.finish_diff(*this, GAIA_MOV(addDiffCtx));
	#endif

				return dstEntity;
			}

			//! Creates @a count new entities by cloning an already existing one. Trigger observers if necessary.
			//! \param entity Entity to clone
			//! \param count Number of clones to make
			//! \param func Functor executed every time a copy is created.
			//!             It can be either void(ecs::Entity) or void(ecs::CopyIter&).
			//! \warning It is expected @a entity is valid generic entity. Undefined behavior otherwise.
			//! \warning If EntityDesc is present on @a entity, it is not copied because names are
			//!          expected to be unique. Instead, the copied entity will be a part of an archetype
			//!          without EntityDesc and any calls to World::name(copiedEntity) will return an empty view.
			template <typename Func = TFunc_Void_With_Entity>
			void copy_ext_n(Entity entity, uint32_t count, Func func = func_void_with_entity) {
				auto& ec = m_recs.entities[entity.id()];
				auto* pDstArchetype = ec.pArchetype;
				if (pDstArchetype->has<EntityDesc>())
					pDstArchetype = foc_archetype_del(pDstArchetype, GAIA_ID(EntityDesc));

				const auto archetypeIdCount = (uint32_t)pDstArchetype->ids_view().size();
				const auto sparseIdCount = copied_sparse_id_count(entity, [](Entity) {
					return true;
				});
				const auto addedIdCount = archetypeIdCount + sparseIdCount;
				auto* pAddedIds = addedIdCount != 0U ? (Entity*)alloca(sizeof(Entity) * addedIdCount) : nullptr;
				write_archetype_ids(*pDstArchetype, pAddedIds);
				write_copied_sparse_ids(
						entity,
						[](Entity) {
							return true;
						},
						pAddedIds + archetypeIdCount);
	#if GAIA_OBSERVERS_ENABLED
				auto addDiffCtx = m_observers.prepare_diff_add_new(*this, EntitySpan{pAddedIds, addedIdCount});
	#endif
				copy_n_inter(
						entity, count, func, EntitySpan{pAddedIds, addedIdCount}, EntityBad
	#if GAIA_OBSERVERS_ENABLED
						,
						&addDiffCtx
	#endif
				);
	#if GAIA_OBSERVERS_ENABLED
				m_observers.finish_diff(*this, GAIA_MOV(addDiffCtx));
	#endif
			}
#endif

		private:
			struct CopyIterGroupState {
				Archetype* pArchetype = nullptr;
				Chunk* pChunk = nullptr;
				uint16_t startRow = 0;
				uint16_t count = 0;
			};

			struct PrefabInstantiatePlanNode {
				Entity prefab = EntityBad;
				uint32_t parentIdx = BadIndex;
				Archetype* pDstArchetype = nullptr;
				cnt::darray_ext<Entity, 16> copiedSparseIds;
				cnt::darray_ext<Entity, 16> addedIds;
				cnt::darray_ext<Entity, 16> addHookIds;
			};

			template <typename Func>
			void invoke_copy_batch_callback(
					Func& func, Archetype* pDstArchetype, Chunk* pDstChunk, uint32_t originalChunkSize, uint32_t toCreate) {
				if constexpr (std::is_invocable_v<Func, CopyIter&>) {
					CopyIter it;
					it.set_world(this);
					it.set_archetype(pDstArchetype);
					it.set_chunk(pDstChunk);
					it.set_range((uint16_t)originalChunkSize, (uint16_t)toCreate);
					func(it);
				} else {
					auto entities = pDstChunk->entity_view();
					GAIA_FOR2(originalChunkSize, pDstChunk->size()) func(entities[i]);
				}
			}

			template <typename Func>
			void flush_copy_iter_group(Func& func, CopyIterGroupState& group) {
				if (group.count == 0)
					return;

				CopyIter it;
				it.set_world(this);
				it.set_archetype(group.pArchetype);
				it.set_chunk(group.pChunk);
				it.set_range(group.startRow, group.count);
				func(it);
				group.count = 0;
			}

			template <typename Func>
			void push_copy_iter_group(Func& func, CopyIterGroupState& group, Entity instance) {
				const auto& ec = fetch(instance);

				if (group.count != 0 && ec.pArchetype == group.pArchetype && ec.pChunk == group.pChunk &&
						ec.row == uint16_t(group.startRow + group.count)) {
					++group.count;
					return;
				}

				flush_copy_iter_group(func, group);
				group.pArchetype = ec.pArchetype;
				group.pChunk = ec.pChunk;
				group.startRow = ec.row;
				group.count = 1;
			}

			void prepare_parent_batch(Entity parentEntity) {
				GAIA_ASSERT(valid(parentEntity));

				const auto parentPair = Pair(Parent, parentEntity);
				assign_pair(parentPair, *m_pEntityArchetype);

				touch_rel_version(Parent);
				invalidate_queries_for_rel(Parent);
				m_targetsTravCache = {};
				m_srcBfsTravCache = {};
				m_depthOrderCache = {};
				m_sourcesAllCache = {};
				m_targetsAllCache = {};

				auto& ecParent = fetch(parentEntity);
				EntityBuilder::set_flag(ecParent.flags, EntityContainerFlags::OnDeleteTarget_Delete, true);
			}

			void parent_batch(
					Entity parentEntity, Archetype& archetype, Chunk& chunk, uint32_t originalChunkSize, uint32_t toCreate) {
				GAIA_ASSERT(valid(parentEntity));

				if (toCreate == 0)
					return;

				auto entities = chunk.entity_view();
#if GAIA_OBSERVERS_ENABLED
				const Entity parentPair = Pair(Parent, parentEntity);
				auto addDiffCtx = m_observers.prepare_diff(
						*this, ObserverEvent::OnAdd, EntitySpan{&parentPair, 1},
						EntitySpan{entities.data() + originalChunkSize, toCreate});
#endif
				GAIA_FOR2_(originalChunkSize, originalChunkSize + toCreate, rowIdx) {
					exclusive_adjunct_set(entities[rowIdx], Parent, parentEntity);
				}

#if GAIA_OBSERVERS_ENABLED
				m_observers.on_add(
						*this, archetype, EntitySpan{&parentPair, 1}, EntitySpan{entities.data() + originalChunkSize, toCreate});
				m_observers.finish_diff(*this, GAIA_MOV(addDiffCtx));
#endif
			}

			void parent_direct(Entity entity, Entity parentEntity) {
				GAIA_ASSERT(valid(entity));
				GAIA_ASSERT(valid(parentEntity));

				prepare_parent_batch(parentEntity);
#if GAIA_OBSERVERS_ENABLED
				const Entity parentPair = Pair(Parent, parentEntity);
				auto addDiffCtx =
						m_observers.prepare_diff(*this, ObserverEvent::OnAdd, EntitySpan{&parentPair, 1}, EntitySpan{&entity, 1});
#endif
				exclusive_adjunct_set(entity, Parent, parentEntity);

#if GAIA_OBSERVERS_ENABLED
				const auto& ec = fetch(entity);
				m_observers.on_add(*this, *ec.pArchetype, EntitySpan{&parentPair, 1}, EntitySpan{&entity, 1});
				m_observers.finish_diff(*this, GAIA_MOV(addDiffCtx));
#endif
			}

			void notify_add_single(Entity entity, Entity object) {
#if GAIA_ENABLE_ADD_DEL_HOOKS || GAIA_OBSERVERS_ENABLED
				if GAIA_UNLIKELY (tearing_down())
					return;

				const auto& ec = fetch(entity);

				lock();

	#if GAIA_ENABLE_ADD_DEL_HOOKS
				if (object.comp()) {
					const auto& item = comp_cache().get(object);
					const auto& hooks = ComponentCache::hooks(item);
					if (hooks.func_add != nullptr)
						hooks.func_add(*this, item, entity);
				}
	#endif

	#if GAIA_OBSERVERS_ENABLED
				m_observers.on_add(*this, *ec.pArchetype, EntitySpan{&object, 1}, EntitySpan{&entity, 1});
	#endif

				unlock();
#else
				(void)entity;
				(void)object;
#endif
			}

			void notify_del_single(Entity entity, Entity object) {
#if GAIA_ENABLE_ADD_DEL_HOOKS || GAIA_OBSERVERS_ENABLED
				if GAIA_UNLIKELY (tearing_down())
					return;

				const auto& ec = fetch(entity);

				lock();

	#if GAIA_OBSERVERS_ENABLED
				m_observers.on_del(*this, *ec.pArchetype, EntitySpan{&object, 1}, EntitySpan{&entity, 1});
	#endif

	#if GAIA_ENABLE_ADD_DEL_HOOKS
				if (object.comp()) {
					const auto& item = comp_cache().get(object);
					const auto& hooks = ComponentCache::hooks(item);
					if (hooks.func_del != nullptr)
						hooks.func_del(*this, item, entity);
				}
	#endif

				unlock();
#else
				(void)entity;
				(void)object;
#endif
			}

			GAIA_NODISCARD bool has_semantic_match_without_source(
					Entity entity, Entity object, Entity excludedSource, cnt::set<EntityLookupKey>& visited) const {
				const auto inserted = visited.insert(EntityLookupKey(entity));
				if (!inserted.second)
					return false;

				if (entity != excludedSource && has_direct(entity, object))
					return true;

				const auto it = m_entityToAsTargets.find(EntityLookupKey(entity));
				if (it == m_entityToAsTargets.end())
					return false;

				for (const auto baseKey: it->second) {
					if (has_semantic_match_without_source(baseKey.entity(), object, excludedSource, visited))
						return true;
				}

				return false;
			}

			void notify_inherited_del_dependents(Entity source, Entity object) {
#if GAIA_ENABLE_ADD_DEL_HOOKS || GAIA_OBSERVERS_ENABLED
				const auto& descendants = as_relations_trav_cache(source);
				for (const auto descendant: descendants) {
					if (descendant == source)
						continue;
					if (has_direct(descendant, object) || !has(descendant, object))
						continue;

					cnt::set<EntityLookupKey> visited;
					if (has_semantic_match_without_source(descendant, object, source, visited))
						continue;

					notify_del_single(descendant, object);
				}
#else
				(void)source;
				(void)object;
#endif
			}

			void notify_inherited_del_dependents(Entity source, EntitySpan removedObjects) {
				for (const auto object: removedObjects)
					notify_inherited_del_dependents(source, object);
			}

			template <typename Func>
			void copy_n_inter(
					Entity entity, uint32_t count, Func& func, EntitySpan addedIds, Entity parentInstance = EntityBad
#if GAIA_OBSERVERS_ENABLED
					,
					ObserverRegistry::DiffDispatchCtx* pAddDiffCtx = nullptr
#endif
			) {
				GAIA_ASSERT(!entity.pair());
				GAIA_ASSERT(valid(entity));
				GAIA_ASSERT(parentInstance == EntityBad || valid(parentInstance));

				if (count == 0U)
					return;

#if GAIA_OBSERVERS_ENABLED
				const bool useLocalAddDiff = !addedIds.empty() && pAddDiffCtx == nullptr;
				ObserverRegistry::DiffDispatchCtx addDiffCtx{};
				if (useLocalAddDiff)
					addDiffCtx = m_observers.prepare_diff_add_new(*this, EntitySpan{addedIds});
#endif

				auto& ec = m_recs.entities[entity.id()];

				GAIA_ASSERT(ec.pChunk != nullptr);
				GAIA_ASSERT(ec.pArchetype != nullptr);

				auto* pSrcChunk = ec.pChunk;
				auto* pDstArchetype = ec.pArchetype;
				const auto hasEntityDesc = pDstArchetype->has<EntityDesc>();
				if (hasEntityDesc)
					pDstArchetype = foc_archetype_del(pDstArchetype, GAIA_ID(EntityDesc));

				if (parentInstance != EntityBad)
					prepare_parent_batch(parentInstance);

				// Entities array might get reallocated after m_recs.entities.alloc
				// so instead of fetching the container again we simply cache the row
				// of our source entity.
				const auto srcRow = ec.row;

				EntityContainerCtx ctx{true, false, EntityKind::EK_Gen};

				uint32_t left = count;
				do {
					auto* pDstChunk = pDstArchetype->foc_free_chunk();
					const uint32_t originalChunkSize = pDstChunk->size();
					const uint32_t freeSlotsInChunk = pDstChunk->capacity() - originalChunkSize;
					const uint32_t toCreate = core::get_min(freeSlotsInChunk, left);

					GAIA_FOR(toCreate) {
						const auto entityNew = m_recs.entities.alloc(&ctx);
						auto& ecNew = m_recs.entities[entityNew.id()];
						store_entity(ecNew, entityNew, pDstArchetype, pDstChunk);

#if GAIA_ASSERT_ENABLED
						GAIA_ASSERT(ecNew.pChunk == pDstChunk);
						auto entityExpected = pDstChunk->entity_view()[ecNew.row];
						GAIA_ASSERT(entityExpected == entityNew);
#endif

						if (hasEntityDesc) {
							Chunk::copy_foreign_entity_data(pSrcChunk, srcRow, pDstChunk, ecNew.row);
						}

						copy_sparse_entity_data(entity, entityNew, [](Entity) {
							return true;
						});
					}

					pDstArchetype->try_update_free_chunk_idx();

					if (!hasEntityDesc) {
						pDstChunk->call_gen_ctors(originalChunkSize, toCreate);

						{
							GAIA_PROF_SCOPE(World::copy_n_entity_data);
							Chunk::copy_entity_data_n_same_chunk(pSrcChunk, srcRow, pDstChunk, originalChunkSize, toCreate);
						}
					}

					pDstChunk->update_versions();

#if GAIA_OBSERVERS_ENABLED
					if (!addedIds.empty()) {
						auto entities = pDstChunk->entity_view();
						if (pAddDiffCtx != nullptr)
							m_observers.append_diff_targets(
									*this, *pAddDiffCtx, EntitySpan{entities.data() + originalChunkSize, toCreate});
						else if (useLocalAddDiff)
							m_observers.append_diff_targets(
									*this, addDiffCtx, EntitySpan{entities.data() + originalChunkSize, toCreate});
						m_observers.on_add(
								*this, *pDstArchetype, addedIds, EntitySpan{entities.data() + originalChunkSize, toCreate});
					}
#endif

					if (parentInstance != EntityBad)
						parent_batch(parentInstance, *pDstArchetype, *pDstChunk, originalChunkSize, toCreate);

					invoke_copy_batch_callback(func, pDstArchetype, pDstChunk, originalChunkSize, toCreate);

					left -= toCreate;
				} while (left > 0);
#if GAIA_OBSERVERS_ENABLED
				if (useLocalAddDiff)
					m_observers.finish_diff(*this, GAIA_MOV(addDiffCtx));
#endif
			}

			GAIA_NODISCARD bool id_uses_inherit_policy(Entity id) const {
				return !is_wildcard(id) && valid(id) && target(id, OnInstantiate) == Inherit;
			}

			GAIA_NODISCARD Entity inherited_id_owner(Entity entity, Entity id) const {
				if (!id_uses_inherit_policy(id))
					return EntityBad;

				const auto& targets = as_targets_trav_cache(entity);
				for (const auto target: targets) {
					if (has_inter(target, id, false))
						return target;
				}

				return EntityBad;
			}

			GAIA_NODISCARD bool instantiate_copies_id(Entity id) const {
				const auto policy = target(id, OnInstantiate);
				if (policy == EntityBad || policy == Override)
					return true;
				if (policy == DontInherit || policy == Inherit)
					return false;
				return true;
			}

			template <typename T>
			void gather_sorted_prefab_children(Entity prefabEntity, T& outChildren) {
				sources(Parent, prefabEntity, [&](Entity childPrefab) {
					if (!has_direct(childPrefab, Prefab))
						return;
					outChildren.push_back(childPrefab);
				});

				core::sort(outChildren, [](Entity left, Entity right) {
					return left.id() < right.id();
				});
			}

			template <typename Func>
			uint32_t
			copy_sparse_entity_data(Entity srcEntity, Entity dstEntity, Func&& shouldCopy, Entity* pCopiedIds = nullptr) {
				uint32_t copiedCnt = 0;
				for (auto& [compKey, store]: m_sparseComponentsByComp) {
					const auto comp = compKey.entity();
					if (!store.func_has(store.pStore, srcEntity) || !shouldCopy(comp))
						continue;

					GAIA_ASSERT(store.func_copy_entity != nullptr);
					if (!store.func_copy_entity(store.pStore, dstEntity, srcEntity))
						continue;

					if (pCopiedIds != nullptr)
						pCopiedIds[copiedCnt] = comp;
					++copiedCnt;
				}

				return copiedCnt;
			}

			uint32_t copy_sparse_entity_data(
					Entity srcEntity, Entity dstEntity, EntitySpan copiedSparseIds, Entity* pCopiedIds = nullptr) {
				uint32_t copiedCnt = 0;
				for (const auto comp: copiedSparseIds) {
					auto it = m_sparseComponentsByComp.find(EntityLookupKey(comp));
					GAIA_ASSERT(it != m_sparseComponentsByComp.end());

					auto& store = it->second;
					if (!store.func_has(store.pStore, srcEntity))
						continue;

					GAIA_ASSERT(store.func_copy_entity != nullptr);
					if (!store.func_copy_entity(store.pStore, dstEntity, srcEntity))
						continue;

					if (pCopiedIds != nullptr)
						pCopiedIds[copiedCnt] = comp;
					++copiedCnt;
				}

				return copiedCnt;
			}

			void write_archetype_ids(const Archetype& dstArchetype, Entity* pDst) const {
				for (const auto id: dstArchetype.ids_view())
					*pDst++ = id;
			}

			template <typename Func>
			GAIA_NODISCARD uint32_t copied_sparse_id_count(Entity srcEntity, Func&& shouldCopySparse) const {
				uint32_t count = 0;
				for (const auto& [compKey, store]: m_sparseComponentsByComp) {
					const auto comp = compKey.entity();
					if (!store.func_has(store.pStore, srcEntity) || !shouldCopySparse(comp) ||
							!is_non_fragmenting_out_of_line_component(comp))
						continue;
					++count;
				}

				return count;
			}

			template <typename Func>
			void write_copied_sparse_ids(Entity srcEntity, Func&& shouldCopySparse, Entity* pDst) const {
				for (const auto& [compKey, store]: m_sparseComponentsByComp) {
					const auto comp = compKey.entity();
					if (!store.func_has(store.pStore, srcEntity) || !shouldCopySparse(comp) ||
							!is_non_fragmenting_out_of_line_component(comp))
						continue;
					*pDst++ = comp;
				}
			}

			GAIA_NODISCARD bool override_inter(Entity entity, Entity object) {
				GAIA_ASSERT(valid(entity));
				GAIA_ASSERT(object.pair() || valid(object));

				if (has_direct(entity, object))
					return false;

				const auto inheritedOwner = inherited_id_owner(entity, object);
				if (inheritedOwner == EntityBad)
					return false;

				if (!object.pair()) {
					const auto* pItem = comp_cache().find(object);
					if (pItem != nullptr && pItem->entity == object) {
						if (is_out_of_line_component(object)) {
							const auto itSparseStore = m_sparseComponentsByComp.find(EntityLookupKey(object));
							if (itSparseStore == m_sparseComponentsByComp.end())
								return false;

							GAIA_ASSERT(itSparseStore->second.func_copy_entity != nullptr);
							if (!itSparseStore->second.func_copy_entity(itSparseStore->second.pStore, entity, inheritedOwner))
								return false;

							if (!is_non_fragmenting_out_of_line_component(object)) {
								EntityBuilder eb(*this, entity);
								eb.add_inter_init(object);
								eb.commit();
							}
							return true;
						}

						if (pItem->comp.size() != 0U) {
							add(entity, object);

							const auto& ecDst = fetch(entity);
							const auto& ecSrc = fetch(inheritedOwner);
							const auto compIdxDst = ecDst.pChunk->comp_idx(object);
							const auto compIdxSrc = ecSrc.pChunk->comp_idx(object);
							GAIA_ASSERT(compIdxDst != BadIndex && compIdxSrc != BadIndex);

							const auto idxDst = uint16_t(ecDst.row * (1U - (uint32_t)object.kind()));
							const auto idxSrc = uint16_t(ecSrc.row * (1U - (uint32_t)object.kind()));
							void* pDst = ecDst.pChunk->comp_ptr_mut(compIdxDst);
							const void* pSrc = ecSrc.pChunk->comp_ptr(compIdxSrc);
							pItem->copy(pDst, pSrc, idxDst, idxSrc, ecDst.pChunk->capacity(), ecSrc.pChunk->capacity());
							return true;
						}
					}
				}

				add(entity, object);
				return true;
			}

			GAIA_NODISCARD bool copy_owned_id_from_entity(Entity srcEntity, Entity dstEntity, Entity object) {
				GAIA_ASSERT(valid(srcEntity));
				GAIA_ASSERT(valid(dstEntity));
				GAIA_ASSERT(object.pair() || valid(object));

				if (has_direct(dstEntity, object))
					return false;

				if (!object.pair()) {
					const auto* pItem = comp_cache().find(object);
					if (pItem != nullptr && pItem->entity == object) {
						if (is_out_of_line_component(object)) {
							const auto itSparseStore = m_sparseComponentsByComp.find(EntityLookupKey(object));
							if (itSparseStore == m_sparseComponentsByComp.end())
								return false;

							GAIA_ASSERT(itSparseStore->second.func_copy_entity != nullptr);
							if (!is_non_fragmenting_out_of_line_component(object)) {
								if (!itSparseStore->second.func_copy_entity(itSparseStore->second.pStore, dstEntity, srcEntity))
									return false;

								EntityBuilder eb(*this, dstEntity);
								eb.add_inter_init(object);
								eb.commit();
								notify_add_single(dstEntity, object);
								return true;
							}
#if GAIA_OBSERVERS_ENABLED
							auto addDiffCtx = m_observers.prepare_diff(
									*this, ObserverEvent::OnAdd, EntitySpan{&object, 1}, EntitySpan{&dstEntity, 1});
#endif
							if (!itSparseStore->second.func_copy_entity(itSparseStore->second.pStore, dstEntity, srcEntity))
								return false;
							notify_add_single(dstEntity, object);
#if GAIA_OBSERVERS_ENABLED
							m_observers.finish_diff(*this, GAIA_MOV(addDiffCtx));
#endif
							return true;
						}

						if (pItem->comp.size() != 0U) {
							EntityBuilder eb(*this, dstEntity);
							eb.add_inter_init(object);
							eb.commit();

							const auto& ecDst = fetch(dstEntity);
							const auto& ecSrc = fetch(srcEntity);
							const auto compIdxDst = ecDst.pChunk->comp_idx(object);
							const auto compIdxSrc = ecSrc.pChunk->comp_idx(object);
							GAIA_ASSERT(compIdxDst != BadIndex && compIdxSrc != BadIndex);

							const auto idxDst = uint16_t(ecDst.row * (1U - (uint32_t)object.kind()));
							const auto idxSrc = uint16_t(ecSrc.row * (1U - (uint32_t)object.kind()));
							void* pDst = ecDst.pChunk->comp_ptr_mut(compIdxDst);
							const void* pSrc = ecSrc.pChunk->comp_ptr(compIdxSrc);
							pItem->copy(pDst, pSrc, idxDst, idxSrc, ecDst.pChunk->capacity(), ecSrc.pChunk->capacity());
							notify_add_single(dstEntity, object);
							return true;
						}
					}
				}

				add(dstEntity, object);
				return true;
			}

			GAIA_NODISCARD Archetype* instantiate_prefab_dst_archetype(Entity prefabEntity) {
				GAIA_ASSERT(!prefabEntity.pair());
				GAIA_ASSERT(valid(prefabEntity));
				GAIA_ASSERT(has_direct(prefabEntity, Prefab));

				if GAIA_UNLIKELY (!has_direct(prefabEntity, Prefab))
					return fetch(prefabEntity).pArchetype;

				auto& ecSrc = m_recs.entities[prefabEntity.id()];
				GAIA_ASSERT(ecSrc.pArchetype != nullptr);

				auto* pDstArchetype = ecSrc.pArchetype;
				if (pDstArchetype->has<EntityDesc>())
					pDstArchetype = foc_archetype_del(pDstArchetype, GAIA_ID(EntityDesc));
				if (pDstArchetype->has(Prefab))
					pDstArchetype = foc_archetype_del(pDstArchetype, Prefab);

				for (const auto id: ecSrc.pArchetype->ids_view()) {
					if (id.pair() && id.id() == Is.id()) {
						pDstArchetype = foc_archetype_del(pDstArchetype, id);
						continue;
					}

					if (!instantiate_copies_id(id))
						pDstArchetype = foc_archetype_del(pDstArchetype, id);
				}

				const auto isPair = Pair(Is, prefabEntity);
				assign_pair(isPair, *m_pEntityArchetype);
				pDstArchetype = foc_archetype_add(pDstArchetype, isPair);

				return pDstArchetype;
			}

			template <typename T>
			void collect_prefab_copied_sparse_ids(Entity prefabEntity, T& outCopiedSparseIds) {
				outCopiedSparseIds.clear();
				if (m_sparseComponentsByComp.empty())
					return;

				for (const auto& [compKey, store]: m_sparseComponentsByComp) {
					const auto comp = compKey.entity();
					if (!store.func_has(store.pStore, prefabEntity) || !instantiate_copies_id(comp) ||
							!is_out_of_line_component(comp))
						continue;
					outCopiedSparseIds.push_back(comp);
				}
			}

			template <typename T>
			void collect_prefab_added_ids(Archetype* pDstArchetype, EntitySpan copiedSparseIds, T& outAddedIds) {
				outAddedIds.clear();
				for (const auto id: pDstArchetype->ids_view())
					outAddedIds.push_back(id);

				for (const auto comp: copiedSparseIds) {
					if (is_non_fragmenting_out_of_line_component(comp))
						outAddedIds.push_back(comp);
				}
			}

			template <typename T>
			void collect_prefab_add_hook_ids(EntitySpan addedIds, T& outHookIds) {
				outHookIds.clear();
				for (const auto id: addedIds) {
					if (!id.comp())
						continue;

					const auto& item = comp_cache().get(id);
					if (ComponentCache::hooks(item).func_add != nullptr)
						outHookIds.push_back(id);
				}
			}

			GAIA_NODISCARD Entity instantiate_prefab_node_inter(
					Entity prefabEntity, Archetype* pDstArchetype, Entity parentInstance, EntitySpan copiedSparseIds,
					EntitySpan addedIds, EntitySpan addHookIds) {
				GAIA_ASSERT(!prefabEntity.pair());
				GAIA_ASSERT(valid(prefabEntity));
				GAIA_ASSERT(has_direct(prefabEntity, Prefab));
				GAIA_ASSERT(pDstArchetype != nullptr);
#if GAIA_OBSERVERS_ENABLED
				auto addDiffCtx = m_observers.prepare_diff_add_new(*this, EntitySpan{addedIds});
#endif

				auto& ecSrc = m_recs.entities[prefabEntity.id()];
				GAIA_ASSERT(ecSrc.pArchetype != nullptr);
				GAIA_ASSERT(ecSrc.pChunk != nullptr);

				EntityContainerCtx ctx{true, false, prefabEntity.kind()};
				const auto instance = m_recs.entities.alloc(&ctx);
				auto& ecDst = m_recs.entities[instance.id()];
				auto* pDstChunk = pDstArchetype->foc_free_chunk();
				store_entity(ecDst, instance, pDstArchetype, pDstChunk);
				pDstArchetype->try_update_free_chunk_idx();
				Chunk::copy_foreign_entity_data(ecSrc.pChunk, ecSrc.row, pDstChunk, ecDst.row);
				pDstChunk->update_versions();

				ecDst.flags |= EntityContainerFlags::HasAliasOf;

				// Keep payload copy and observer/add-id reporting separate:
				// fragmenting sparse payloads must still be copied here even though their id is
				// already present in the destination archetype and therefore absent from addedIds tail.
				(void)copy_sparse_entity_data(prefabEntity, instance, copiedSparseIds);
#if GAIA_OBSERVERS_ENABLED
				m_observers.append_diff_targets(*this, addDiffCtx, EntitySpan{&instance, 1});
#endif

				touch_rel_version(Is);
				invalidate_queries_for_rel(Is);
				m_targetsTravCache = {};
				m_srcBfsTravCache = {};
				m_depthOrderCache = {};
				m_sourcesAllCache = {};
				m_targetsAllCache = {};

				const auto instanceKey = EntityLookupKey(instance);
				const auto prefabKey = EntityLookupKey(prefabEntity);
				m_entityToAsTargets[instanceKey].insert(prefabKey);
				m_entityToAsTargetsTravCache = {};
				m_entityToAsRelations[prefabKey].insert(instanceKey);
				m_entityToAsRelationsTravCache = {};
				invalidate_queries_for_entity({Is, prefabEntity});

#if GAIA_ENABLE_ADD_DEL_HOOKS || GAIA_OBSERVERS_ENABLED
				if GAIA_UNLIKELY (tearing_down()) {
					(void)pDstArchetype;
					(void)addedIds;
				} else {
					lock();

	#if GAIA_ENABLE_ADD_DEL_HOOKS
					for (const auto id: addHookIds) {
						const auto& item = comp_cache().get(id);
						const auto& hooks = ComponentCache::hooks(item);
						GAIA_ASSERT(hooks.func_add != nullptr);
						hooks.func_add(*this, item, instance);
					}
	#endif

	#if GAIA_OBSERVERS_ENABLED
					m_observers.on_add(*this, *pDstArchetype, addedIds, EntitySpan{&instance, 1});
	#endif

					unlock();
				}
#endif

#if GAIA_OBSERVERS_ENABLED
				m_observers.finish_diff(*this, GAIA_MOV(addDiffCtx));
#endif

				if (parentInstance != EntityBad)
					parent_direct(instance, parentInstance);

				return instance;
			}

			GAIA_NODISCARD Entity instantiate_prefab_node_inter(Entity prefabEntity, Entity parentInstance) {
				auto* pDstArchetype = instantiate_prefab_dst_archetype(prefabEntity);
				cnt::darray_ext<Entity, 16> copiedSparseIds;
				cnt::darray_ext<Entity, 16> addedIds;
				cnt::darray_ext<Entity, 16> addHookIds;
				collect_prefab_copied_sparse_ids(prefabEntity, copiedSparseIds);
				collect_prefab_added_ids(pDstArchetype, EntitySpan{copiedSparseIds}, addedIds);
				collect_prefab_add_hook_ids(EntitySpan{addedIds}, addHookIds);
				return instantiate_prefab_node_inter(
						prefabEntity, pDstArchetype, parentInstance, EntitySpan{copiedSparseIds}, EntitySpan{addedIds},
						EntitySpan{addHookIds});
			}

			template <typename Func>
			void instantiate_prefab_n_inter(
					const PrefabInstantiatePlanNode& node, Entity parentInstance, uint32_t count, Func& func) {
				GAIA_ASSERT(node.prefab != EntityBad);
				GAIA_ASSERT(node.pDstArchetype != nullptr);

				if (count == 0U)
					return;
#if GAIA_OBSERVERS_ENABLED
				auto addDiffCtx = m_observers.prepare_diff_add_new(*this, EntitySpan{node.addedIds});
#endif

				auto& ecSrc = m_recs.entities[node.prefab.id()];
				GAIA_ASSERT(ecSrc.pChunk != nullptr);

				if (parentInstance != EntityBad)
					prepare_parent_batch(parentInstance);

				const auto srcRow = ecSrc.row;
				auto* pSrcChunk = ecSrc.pChunk;
				auto* pDstArchetype = node.pDstArchetype;
				const auto prefabKey = EntityLookupKey(node.prefab);
				EntityContainerCtx ctx{true, false, node.prefab.kind()};

				uint32_t left = count;
				do {
					auto* pDstChunk = pDstArchetype->foc_free_chunk();
					const uint32_t originalChunkSize = pDstChunk->size();
					const uint32_t freeSlotsInChunk = pDstChunk->capacity() - originalChunkSize;
					const uint32_t toCreate = core::get_min(freeSlotsInChunk, left);

					GAIA_FOR_(toCreate, rowOffset) {
						const auto instance = m_recs.entities.alloc(&ctx);
						auto& ecDst = m_recs.entities[instance.id()];
						store_entity(ecDst, instance, pDstArchetype, pDstChunk);
						ecDst.flags |= EntityContainerFlags::HasAliasOf;

						(void)copy_sparse_entity_data(node.prefab, instance, EntitySpan{node.copiedSparseIds});
					}

					pDstArchetype->try_update_free_chunk_idx();
					Chunk::copy_foreign_entity_data_n(pSrcChunk, srcRow, pDstChunk, originalChunkSize, toCreate);
					pDstChunk->update_versions();

					touch_rel_version(Is);
					invalidate_queries_for_rel(Is);
					m_targetsTravCache = {};
					m_srcBfsTravCache = {};
					m_depthOrderCache = {};
					m_sourcesAllCache = {};
					m_targetsAllCache = {};

					auto entities = pDstChunk->entity_view();
					auto& asRelations = m_entityToAsRelations[prefabKey];
					GAIA_FOR2_(originalChunkSize, originalChunkSize + toCreate, rowIdx) {
						const auto instance = entities[rowIdx];
						m_entityToAsTargets[EntityLookupKey(instance)].insert(prefabKey);
						asRelations.insert(EntityLookupKey(instance));
					}
					m_entityToAsTargetsTravCache = {};
					m_entityToAsRelationsTravCache = {};
					invalidate_queries_for_entity({Is, node.prefab});

#if GAIA_ENABLE_ADD_DEL_HOOKS || GAIA_OBSERVERS_ENABLED
					if GAIA_UNLIKELY (tearing_down()) {
						(void)entities;
						(void)originalChunkSize;
						(void)toCreate;
					} else {
						lock();

	#if GAIA_ENABLE_ADD_DEL_HOOKS
						for (const auto id: node.addHookIds) {
							const auto& item = comp_cache().get(id);
							const auto& hooks = ComponentCache::hooks(item);
							GAIA_ASSERT(hooks.func_add != nullptr);

							GAIA_FOR2_(originalChunkSize, originalChunkSize + toCreate, rowIdx) {
								hooks.func_add(*this, item, entities[rowIdx]);
							}
						}
	#endif

	#if GAIA_OBSERVERS_ENABLED
						m_observers.append_diff_targets(
								*this, addDiffCtx, EntitySpan{entities.data() + originalChunkSize, toCreate});
						m_observers.on_add(
								*this, *pDstArchetype, EntitySpan{node.addedIds},
								EntitySpan{entities.data() + originalChunkSize, toCreate});
	#endif

						unlock();
					}
#endif

					if (parentInstance != EntityBad)
						parent_batch(parentInstance, *pDstArchetype, *pDstChunk, originalChunkSize, toCreate);

					invoke_copy_batch_callback(func, pDstArchetype, pDstChunk, originalChunkSize, toCreate);

					left -= toCreate;
				} while (left > 0);
#if GAIA_OBSERVERS_ENABLED
				m_observers.finish_diff(*this, GAIA_MOV(addDiffCtx));
#endif
			}

			template <typename T>
			void build_prefab_instantiate_plan(Entity prefabEntity, uint32_t parentIdx, T& plan) {
				PrefabInstantiatePlanNode node{};
				node.prefab = prefabEntity;
				node.parentIdx = parentIdx;
				node.pDstArchetype = instantiate_prefab_dst_archetype(prefabEntity);
				collect_prefab_copied_sparse_ids(prefabEntity, node.copiedSparseIds);
				collect_prefab_added_ids(node.pDstArchetype, EntitySpan{node.copiedSparseIds}, node.addedIds);
				collect_prefab_add_hook_ids(EntitySpan{node.addedIds}, node.addHookIds);

				const auto nodeIdx = (uint32_t)plan.size();
				plan.push_back(GAIA_MOV(node));

				cnt::darray_ext<Entity, 16> prefabChildren;
				gather_sorted_prefab_children(prefabEntity, prefabChildren);

				for (const auto childPrefab: prefabChildren)
					build_prefab_instantiate_plan(childPrefab, nodeIdx, plan);
			}

			GAIA_NODISCARD bool instance_has_prefab_child(Entity parentInstance, Entity childPrefab) const {
				bool found = false;
				sources(Parent, parentInstance, [&](Entity child) {
					if (found)
						return;
					if (has_direct(child, Pair(Is, childPrefab)))
						found = true;
				});
				return found;
			}

			uint32_t sync_prefab_instance(
					Entity prefabEntity, Entity instance, const PrefabInstantiatePlanNode& node, EntitySpan prefabChildren) {
				uint32_t changes = 0;

				const auto isPair = Pair(Is, prefabEntity);
				for (const auto id: node.pDstArchetype->ids_view()) {
					if (id == isPair || has_direct(instance, id))
						continue;
					if (copy_owned_id_from_entity(prefabEntity, instance, id))
						++changes;
				}

				for (const auto comp: node.copiedSparseIds) {
					if (has_direct(instance, comp))
						continue;
					if (copy_owned_id_from_entity(prefabEntity, instance, comp))
						++changes;
				}

				for (const auto childPrefab: prefabChildren) {
					if (instance_has_prefab_child(instance, childPrefab))
						continue;
					(void)instantiate(childPrefab, instance);
					++changes;
				}

				return changes;
			}

			uint32_t sync_prefab_inter(Entity prefabEntity, cnt::set<EntityLookupKey>& visited) {
				GAIA_ASSERT(!prefabEntity.pair());
				GAIA_ASSERT(valid(prefabEntity));

				if (!has_direct(prefabEntity, Prefab))
					return 0;

				const auto ins = visited.insert(EntityLookupKey(prefabEntity));
				if (!ins.second)
					return 0;

				PrefabInstantiatePlanNode node{};
				node.prefab = prefabEntity;
				node.pDstArchetype = instantiate_prefab_dst_archetype(prefabEntity);
				collect_prefab_copied_sparse_ids(prefabEntity, node.copiedSparseIds);
				collect_prefab_added_ids(node.pDstArchetype, EntitySpan{node.copiedSparseIds}, node.addedIds);
				collect_prefab_add_hook_ids(EntitySpan{node.addedIds}, node.addHookIds);

				cnt::darray_ext<Entity, 16> prefabChildren;
				gather_sorted_prefab_children(prefabEntity, prefabChildren);

				uint32_t changes = 0;
				const auto& descendants = as_relations_trav_cache(prefabEntity);
				for (const auto entity: descendants) {
					if (has_direct(entity, Prefab))
						continue;
					changes += sync_prefab_instance(prefabEntity, entity, node, EntitySpan{prefabChildren});
				}

				for (const auto childPrefab: prefabChildren)
					changes += sync_prefab_inter(childPrefab, visited);

				return changes;
			}

			//! Instantiates a prefab as a normal entity and optionally parents it under @a parentInstance.
			GAIA_NODISCARD Entity instantiate_inter(Entity prefabEntity, Entity parentInstance) {
				const auto instance = instantiate_prefab_node_inter(prefabEntity, parentInstance);

				cnt::darray_ext<Entity, 16> prefabChildren;
				gather_sorted_prefab_children(prefabEntity, prefabChildren);

				for (const auto childPrefab: prefabChildren)
					(void)instantiate_inter(childPrefab, instance);

				return instance;
			}

		public:
			//! Instantiates a prefab as a normal entity.
			//! The instance copies the prefab's direct data, drops the Prefab tag, does not copy the name,
			//! removes the prefab's direct Is edges, and adds a direct Pair(Is, prefabEntity) edge instead.
			GAIA_NODISCARD Entity instantiate(Entity prefabEntity) {
				GAIA_ASSERT(!prefabEntity.pair());
				GAIA_ASSERT(valid(prefabEntity));

				if GAIA_UNLIKELY (!has_direct(prefabEntity, Prefab))
					return copy(prefabEntity);

				return instantiate_inter(prefabEntity, EntityBad);
			}

			//! Instantiates a prefab as a normal entity parented under @a parentInstance.
			//! The instance copies the prefab's direct data, drops the Prefab tag, does not copy the name,
			//! removes the prefab's direct Is edges, adds a direct Pair(Is, prefabEntity) edge instead,
			//! and attaches Pair(Parent, parentInstance) to the new root instance.
			GAIA_NODISCARD Entity instantiate(Entity prefabEntity, Entity parentInstance) {
				GAIA_ASSERT(!prefabEntity.pair());
				GAIA_ASSERT(valid(prefabEntity));
				GAIA_ASSERT(valid(parentInstance));

				if GAIA_UNLIKELY (!has_direct(prefabEntity, Prefab)) {
					const auto instance = copy(prefabEntity);
					parent_direct(instance, parentInstance);
					return instance;
				}

				return instantiate_inter(prefabEntity, parentInstance);
			}

			//! Instantiates @a count copies of a prefab as normal entities.
			//! The instance copies the prefab's direct data, drops the Prefab tag, does not copy the name,
			//! removes the prefab's direct Is edges, adds a direct Pair(Is, prefabEntity) edge instead,
			//! and attaches Pair(Parent, parentInstance) to the new root instance.
			//! \param prefabEntity Prefab entity to clone
			//! \param count Number of clones to make
			//! \param func Functor executed every time a copy is created.
			//!             It can be either void(ecs::Entity) or void(ecs::CopyIter&).
			//! \warning It is expected @a prefabEntity is valid entity. Undefined behavior otherwise.
			//! \warning If EntityDesc is present on @a prefabEntity, it is not copied because names are
			//!          expected to be unique. Instead, the copied entity will be a part of an archetype
			//!          without EntityDesc and any calls to World::name(copiedEntity) will return an empty view.
			template <typename Func = TFunc_Void_With_Entity>
			void instantiate_n(Entity prefabEntity, uint32_t count, Func func = func_void_with_entity) {
				instantiate_n(prefabEntity, EntityBad, count, func);
			}

			//! Instantiates @a count copies of a prefab as normal entities parented under @a parentInstance.
			void instantiate_n(Entity prefabEntity, Entity parentInstance, uint32_t count) {
				instantiate_n(prefabEntity, parentInstance, count, func_void_with_entity);
			}

			//! Instantiates @a count copies of a prefab as normal entities parented under @a parentInstance.
			//! The callback is invoked for each spawned root instance.
			template <typename Func>
			void instantiate_n(Entity prefabEntity, Entity parentInstance, uint32_t count, Func func) {
				GAIA_ASSERT(!prefabEntity.pair());
				GAIA_ASSERT(valid(prefabEntity));
				GAIA_ASSERT(parentInstance == EntityBad || valid(parentInstance));

				if (count == 0U)
					return;

				if GAIA_UNLIKELY (!has_direct(prefabEntity, Prefab)) {
					if (parentInstance == EntityBad) {
						copy_n(prefabEntity, count, func);
						return;
					}

					copy_n_inter(prefabEntity, count, func, EntitySpan{}, parentInstance);
					return;
				}

				cnt::darray_ext<PrefabInstantiatePlanNode, 16> plan;
				cnt::darray_ext<Entity, 16> spawned;

				if constexpr (std::is_invocable_v<Func, CopyIter&>) {
					build_prefab_instantiate_plan(prefabEntity, BadIndex, plan);
					if (plan.size() == 1) {
						instantiate_prefab_n_inter(plan[0], parentInstance, count, func);
						return;
					}

					CopyIterGroupState group;
					cnt::darray<Entity> roots;
					roots.reserve(count);
					auto collectRoot = [&](Entity instance) {
						roots.push_back(instance);
					};
					instantiate_prefab_n_inter(plan[0], parentInstance, count, collectRoot);

					spawned.resize((uint32_t)plan.size());

					GAIA_FOR_(count, rootIdx) {
						spawned[0] = roots[rootIdx];
						GAIA_FOR2_(1, (uint32_t)plan.size(), planIdx) {
							const auto parent = spawned[plan[planIdx].parentIdx];
							spawned[planIdx] = instantiate_prefab_node_inter(
									plan[planIdx].prefab, plan[planIdx].pDstArchetype, parent, EntitySpan{plan[planIdx].copiedSparseIds},
									EntitySpan{plan[planIdx].addedIds}, EntitySpan{plan[planIdx].addHookIds});
						}

						push_copy_iter_group(func, group, roots[rootIdx]);
					}

					flush_copy_iter_group(func, group);
				} else {
					build_prefab_instantiate_plan(prefabEntity, BadIndex, plan);
					if (plan.size() == 1) {
						instantiate_prefab_n_inter(plan[0], parentInstance, count, func);
						return;
					}

					cnt::darray<Entity> roots;
					roots.reserve(count);
					auto collectRoot = [&](Entity instance) {
						roots.push_back(instance);
					};
					instantiate_prefab_n_inter(plan[0], parentInstance, count, collectRoot);

					spawned.resize((uint32_t)plan.size());

					GAIA_FOR_(count, rootIdx) {
						spawned[0] = roots[rootIdx];
						GAIA_FOR2_(1, (uint32_t)plan.size(), planIdx) {
							const auto parent = spawned[plan[planIdx].parentIdx];
							spawned[planIdx] = instantiate_prefab_node_inter(
									plan[planIdx].prefab, plan[planIdx].pDstArchetype, parent, EntitySpan{plan[planIdx].copiedSparseIds},
									EntitySpan{plan[planIdx].addedIds}, EntitySpan{plan[planIdx].addHookIds});
						}

						func(roots[rootIdx]);
					}
				}
			}

			//! Propagates additive prefab changes to existing non-prefab instances.
			//! Missing copied ids are added to existing instances and missing prefab children are spawned.
			//! Existing owned instance data is left intact.
			GAIA_NODISCARD uint32_t sync(Entity prefabEntity) {
				GAIA_ASSERT(!prefabEntity.pair());
				GAIA_ASSERT(valid(prefabEntity));

				cnt::set<EntityLookupKey> visited;
				return sync_prefab_inter(prefabEntity, visited);
			}

			//----------------------------------------------------------------------

			//! Removes an entity along with all data associated with it.
			//! \param entity Entity to delete
			void del(Entity entity) {
				if (!entity.pair()) {
					// Delete all relationships associated with this entity (if any)
					del_inter(Pair(entity, All));
					del_inter(Pair(All, entity));
				}

				del_inter(entity);
			}

			//! Removes an @a object from @a entity if possible.
			//! \param entity Entity to delete from
			//! \param object Entity to delete
			//! \warning It is expected both @a entity and @a object are valid. Undefined behavior otherwise.
			void del(Entity entity, Entity object) {
				if (!object.pair()) {
					const auto itSparseStore = m_sparseComponentsByComp.find(EntityLookupKey(object));
					if (itSparseStore != m_sparseComponentsByComp.end()) {
						if (!is_non_fragmenting_out_of_line_component(object)) {
							{
								EntityBuilder eb(*this, entity);
								eb.del(object);
							}
							itSparseStore->second.func_del(itSparseStore->second.pStore, entity);
							return;
						}
#if GAIA_OBSERVERS_ENABLED
						auto delDiffCtx =
								m_observers.prepare_diff(*this, ObserverEvent::OnDel, EntitySpan{&object, 1}, EntitySpan{&entity, 1});
#endif
						notify_inherited_del_dependents(entity, object);
						notify_del_single(entity, object);
						itSparseStore->second.func_del(itSparseStore->second.pStore, entity);
#if GAIA_OBSERVERS_ENABLED
						m_observers.finish_diff(*this, GAIA_MOV(delDiffCtx));
#endif
						return;
					}
				}
				EntityBuilder(*this, entity).del(object);
			}

			//! Removes an existing entity relationship pair
			//! \param entity Source entity
			//! \param pair Pair
			//! \warning It is expected both @a entity and the entities forming the relationship are valid.
			//!          Undefined behavior otherwise.
			void del(Entity entity, Pair pair) {
				EntityBuilder(*this, entity).del(pair);
			}

			//! Removes a component @a T from @a entity.
			//! \tparam T Component
			//! \param entity Entity
			//! \warning It is expected the component is present on @a entity. Undefined behavior otherwise.
			//! \warning It is expected @a entity is valid. Undefined behavior otherwise.
			template <typename T>
			void del(Entity entity) {
				using CT = component_type_t<T>;
				using FT = typename CT::TypeFull;

				if constexpr (supports_out_of_line_component<FT>()) {
					if (const auto* pItem = comp_cache().template find<FT>();
							pItem != nullptr && is_out_of_line_component(pItem->entity)) {
						if (sparse_component_store<FT>(pItem->entity) != nullptr)
							del(entity, pItem->entity);
						return;
					}
				}

				EntityBuilder(*this, entity).del<FT>();
			}

			//----------------------------------------------------------------------

			//! Shortcut for add(entity, Pair(Is, entityBase)
			void as(Entity entity, Entity entityBase) {
				// Form the relationship
				add(entity, Pair(Is, entityBase));
			}

			//! Checks if @a entity inherits from @a entityBase.
			//! \param entity Entity
			//! \param entityBase Base entity
			//! \return True if entity is located in entityBase. False otherwise.
			GAIA_NODISCARD bool is(Entity entity, Entity entityBase) const {
				return is_inter<false>(entity, entityBase);
			}

			//! Checks if @a entity is located in @a entityBase.
			//! This is almost the same as "is" with the exception that false is returned
			//! if @a entity matches @a entityBase
			//! \param entity Entity
			//! \param entityBase Base entity
			//! \return True if entity is located in entityBase. False otherwise.
			GAIA_NODISCARD bool in(Entity entity, Entity entityBase) const {
				return is_inter<true>(entity, entityBase);
			}

			GAIA_NODISCARD bool is_base(Entity target) const {
				GAIA_ASSERT(valid_entity(target));

				// Pairs are not supported
				if (target.pair())
					return false;

				const auto it = m_entityToAsRelations.find(EntityLookupKey(target));
				return it != m_entityToAsRelations.end();
			}

			//----------------------------------------------------------------------

			//! Shortcut for add(entity, Pair(ChildOf, parent)
			void child(Entity entity, Entity parent) {
				add(entity, Pair(ChildOf, parent));
			}

			//! Checks if \param entity is a child of \param parent
			//! True if entity is a child of parent. False otherwise.
			GAIA_NODISCARD bool child(Entity entity, Entity parent) const {
				return has(entity, Pair(ChildOf, parent));
			}

			//! Shortcut for add(entity, Pair(Parent, parent))
			void parent(Entity entity, Entity parentEntity) {
				add(entity, Pair(Parent, parentEntity));
			}

			//! Checks if \param entity has non-fragmenting Parent relationship to \param parentEntity.
			GAIA_NODISCARD bool parent(Entity entity, Entity parentEntity) const {
				return has(entity, Pair(Parent, parentEntity));
			}

			//----------------------------------------------------------------------

			//! Marks the component @a T as modified. Best used with acc_mut().sset() or set()
			//! to manually trigger an update at user's whim.
			//! If @a TriggerHooks is true, also triggers the component's set hooks.
			//! \tparam T Component type
			//! \tparam TriggerHooks Triggers hooks if true
			template <
					typename T
#if GAIA_ENABLE_HOOKS
					,
					bool TriggerHooks
#endif
					>
			void modify(Entity entity) {
				GAIA_ASSERT(valid(entity));

				if constexpr (supports_out_of_line_component<T>()) {
					const auto* pItem = comp_cache().template find<T>();
					if (pItem != nullptr && is_out_of_line_component(pItem->entity))
						return;
				}

				auto& ec = m_recs.entities[entity.id()];
				ec.pChunk->template modify<
						T
#if GAIA_ENABLE_HOOKS
						,
						TriggerHooks
#endif
						>();
			}

			//----------------------------------------------------------------------

			//! Starts a bulk set operation on @a entity.
			//! \param entity Entity
			//! \return ComponentSetter
			//! \warning It is expected @a entity is valid. Undefined behavior otherwise.
			//! \warning Undefined behavior if @a entity changes archetype after ComponentSetter is created.
			GAIA_NODISCARD ComponentSetter acc_mut(Entity entity) {
				GAIA_ASSERT(valid(entity));

				const auto& ec = m_recs.entities[entity.id()];
				return ComponentSetter{{ec.pChunk, ec.row}};
			}

			//! Sets the value of the component @a T on @a entity.
			//! \tparam T Component
			//! \param entity Entity
			//! \warning It is expected the component is present on @a entity. Undefined behavior otherwise.
			//! \warning It is expected @a entity is valid. Undefined behavior otherwise.
			//! \warning Undefined behavior if @a entity changes archetype after ComponentSetter is created.
			template <typename T>
			GAIA_NODISCARD decltype(auto) set(Entity entity) {
				static_assert(!is_pair<T>::value);
				using FT = typename component_type_t<T>::TypeFull;
				const auto& item = add<FT>();
				if constexpr (supports_out_of_line_component<FT>()) {
					if (is_out_of_line_component(item.entity))
						return sparse_component_store_mut<FT>(item.entity).mut(entity);
				}
				return acc_mut(entity).mut<T>();
			}

			//! Sets the value of the component associated with @a object on @a entity.
			template <typename T>
			GAIA_NODISCARD decltype(auto) set(Entity entity, Entity object) {
				static_assert(!is_pair<T>::value);
				using FT = typename component_type_t<T>::TypeFull;
				if constexpr (supports_out_of_line_component<FT>()) {
					if (can_use_out_of_line_component<FT>(object))
						return sparse_component_store_mut<FT>(object).mut(entity);
				}
				return acc_mut(entity).mut<T>(object);
			}

			//! Sets the value of the component @a T on @a entity without triggering a world version update.
			//! \tparam T Component
			//! \param entity Entity
			//! \warning It is expected the component is present on @a entity. Undefined behavior otherwise.
			//! \warning It is expected @a entity is valid. Undefined behavior otherwise.
			//! \warning Undefined behavior if @a entity changes archetype after ComponentSetter is created.
			template <typename T>
			GAIA_NODISCARD decltype(auto) sset(Entity entity) {
				static_assert(!is_pair<T>::value);
				using FT = typename component_type_t<T>::TypeFull;
				const auto& item = add<FT>();
				if constexpr (supports_out_of_line_component<FT>()) {
					if (is_out_of_line_component(item.entity))
						return sparse_component_store_mut<FT>(item.entity).mut(entity);
				}
				return acc_mut(entity).smut<T>();
			}

			//! Sets the value of the component associated with @a object on @a entity without updating world version.
			template <typename T>
			GAIA_NODISCARD decltype(auto) sset(Entity entity, Entity object) {
				static_assert(!is_pair<T>::value);
				using FT = typename component_type_t<T>::TypeFull;
				if constexpr (supports_out_of_line_component<FT>()) {
					if (can_use_out_of_line_component<FT>(object))
						return sparse_component_store_mut<FT>(object).mut(entity);
				}
				return acc_mut(entity).smut<T>(object);
			}

			//----------------------------------------------------------------------

			//! Sets the value of the component T on entity without triggering a world version update.
			//! \tparam T Component
			//! \param entity Entity
			//! \warning It is expected the component is present on entity. Undefined behavior otherwise.
			//! \warning It is expected entity is valid. Undefined behavior otherwise.
			//! \warning Undefined behavior if entity changes archetype after ComponentSetter is created.
			template <typename T>
			GAIA_NODISCARD decltype(auto) mut(Entity entity) {
				static_assert(!is_pair<T>::value);
				return set<T>(entity);
			}

			//! Sets the value of the component associated with @a object on @a entity.
			template <typename T>
			GAIA_NODISCARD decltype(auto) mut(Entity entity, Entity object) {
				static_assert(!is_pair<T>::value);
				return set<T>(entity, object);
			}

			//----------------------------------------------------------------------

			//! Starts a bulk get operation on an entity.
			//! \param entity Entity
			//! \return ComponentGetter
			//! \warning It is expected that entity is valid. Undefined behavior otherwise.
			//! \warning Undefined behavior if entity changes archetype after ComponentGetter is created.
			ComponentGetter acc(Entity entity) const {
				GAIA_ASSERT(valid(entity));

				const auto& ec = m_recs.entities[entity.id()];
				return ComponentGetter{ec.pChunk, ec.row};
			}

			//! Returns the value stored in the component T on entity.
			//! \tparam T Component
			//! \param entity Entity
			//! \return Value stored in the component.
			//! \warning It is expected the component is present on entity. Undefined behavior otherwise.
			//! \warning It is expected entity is valid. Undefined behavior otherwise.
			//! \warning Undefined behavior if entity changes archetype after ComponentGetter is created.
			template <typename T>
			GAIA_NODISCARD decltype(auto) get(Entity entity) const {
				using FT = typename component_type_t<T>::TypeFull;
				const auto compEntity = [&]() {
					if constexpr (is_pair<FT>::value) {
						const auto rel = comp_cache().template get<typename FT::rel>().entity;
						const auto tgt = comp_cache().template get<typename FT::tgt>().entity;
						return (Entity)Pair(rel, tgt);
					} else {
						return comp_cache().template get<FT>().entity;
					}
				}();
				if constexpr (supports_out_of_line_component<FT>()) {
					const auto* pItem = comp_cache().template find<FT>();
					if (pItem != nullptr && is_out_of_line_component(pItem->entity)) {
						const auto* pStore = sparse_component_store<FT>(pItem->entity);
						GAIA_ASSERT(pStore != nullptr);
						if (pStore->has(entity))
							return pStore->get(entity);

						const auto inheritedOwner = inherited_id_owner(entity, compEntity);
						GAIA_ASSERT(inheritedOwner != EntityBad);
						return pStore->get(inheritedOwner);
					}
				}

				const auto& ec = m_recs.entities[entity.id()];
				if (ec.pArchetype->has(compEntity))
					return acc(entity).template get<T>();

				const auto inheritedOwner = inherited_id_owner(entity, compEntity);
				GAIA_ASSERT(inheritedOwner != EntityBad);
				return acc(inheritedOwner).template get<T>();
			}

			//! Returns the value stored in the component associated with @a object on @a entity.
			template <typename T>
			GAIA_NODISCARD decltype(auto) get(Entity entity, Entity object) const {
				using FT = typename component_type_t<T>::TypeFull;
				if constexpr (supports_out_of_line_component<FT>()) {
					if (can_use_out_of_line_component<FT>(object)) {
						const auto* pStore = sparse_component_store<FT>(object);
						GAIA_ASSERT(pStore != nullptr);
						if (pStore->has(entity))
							return pStore->get(entity);

						const auto inheritedOwner = inherited_id_owner(entity, object);
						GAIA_ASSERT(inheritedOwner != EntityBad);
						return pStore->get(inheritedOwner);
					}
				}

				const auto& ec = m_recs.entities[entity.id()];
				if (ec.pArchetype->has(object))
					return acc(entity).template get<T>(object);

				const auto inheritedOwner = inherited_id_owner(entity, object);
				GAIA_ASSERT(inheritedOwner != EntityBad);
				return acc(inheritedOwner).template get<T>(object);
			}

			//----------------------------------------------------------------------

			//! Checks if entity is currently used by the world.
			//! \param entity Entity.
			//! \return True if the entity is used. False otherwise.
			GAIA_NODISCARD bool has(Entity entity) const {
				// Pair
				if (entity.pair()) {
					if (entity == Pair(All, All))
						return true;

					if (is_wildcard(entity)) {
						if (!m_entityToArchetypeMap.contains(EntityLookupKey(entity)))
							return false;

						// If the pair is found, both entities forming it need to be found as well
						GAIA_ASSERT(has(get(entity.id())) && has(get(entity.gen())));

						return true;
					}

					const auto it = m_recs.pairs.find(EntityLookupKey(entity));
					if (it == m_recs.pairs.end())
						return false;

					const auto& ec = it->second;
					if (is_req_del(ec))
						return false;

#if GAIA_ASSERT_ENABLED
					// If the pair is found, both entities forming it need to be found as well
					GAIA_ASSERT(has(get(entity.id())) && has(get(entity.gen())));

					// Index of the entity must fit inside the chunk
					auto* pChunk = ec.pChunk;
					GAIA_ASSERT(pChunk != nullptr && ec.row < pChunk->size());
#endif

					return true;
				}

				// Regular entity
				{
					// Entity ID has to fit inside the entity array
					if (entity.id() >= m_recs.entities.size() || !m_recs.entities.has(entity.id()))
						return false;

					// Index of the entity must fit inside the chunk
					const auto& ec = m_recs.entities[entity.id()];
					if (is_req_del(ec))
						return false;

					auto* pChunk = ec.pChunk;
					return pChunk != nullptr && ec.row < pChunk->size();
				}
			}

			//! Checks if @a pair is currently used by the world.
			//! \param pair Pair
			//! \return True if the entity is used. False otherwise.
			GAIA_NODISCARD bool has(Pair pair) const {
				return has((Entity)pair);
			}

			//! Checks if @a entity contains the entity @a object.
			//! \param entity Entity
			//! \param object Tested entity
			//! \return True if object is present on entity. False otherwise or if any of the entities is not valid.
			//! \warning It is expected @a entity is valid. Undefined behavior otherwise.
			//! \warning Undefined behavior if @a entity changes archetype after ComponentSetter is created.
			GAIA_NODISCARD bool has(Entity entity, Entity object) const {
				return has_inter(entity, object, true);
			}

			//! Checks if @a entity directly contains the entity @a object, without semantic inheritance expansion.
			//! \param entity Entity
			//! \param object Tested entity
			//! \return True if object is directly present on entity. False otherwise or if any of the entities is not valid.
			GAIA_NODISCARD bool has_direct(Entity entity, Entity object) const {
				return has_inter(entity, object, false);
			}

			//! Checks if @a entity directly contains @a pair, without semantic inheritance expansion.
			GAIA_NODISCARD bool has_direct(Entity entity, Pair pair) const {
				return has_inter(entity, (Entity)pair, false);
			}

		private:
			GAIA_NODISCARD bool has_inter(Entity entity, Entity object, bool allowSemanticIs) const {
				const auto& ec = fetch(entity);
				if (is_req_del(ec))
					return false;

				if (object.pair() && has_exclusive_adjunct_pair(entity, object))
					return true;
				if (!object.pair()) {
					const auto itSparseStore = m_sparseComponentsByComp.find(EntityLookupKey(object));
					if (itSparseStore != m_sparseComponentsByComp.end())
						return itSparseStore->second.func_has(itSparseStore->second.pStore, entity) ||
									 (allowSemanticIs && inherited_id_owner(entity, object) != EntityBad);
				}

				const auto* pArchetype = ec.pArchetype;

				if (object.pair()) {
					if (allowSemanticIs && object.id() == Is.id() && !is_wildcard(object.gen())) {
						const auto target = get(object.gen());
						return valid(target) && is(entity, target);
					}

					// Early exit if there are no pairs on the archetype
					if (pArchetype->pairs() == 0)
						return false;

					EntityId rel = object.id();
					EntityId tgt = object.gen();

					// (*,*)
					if (rel == All.id() && tgt == All.id())
						return true;

					// (X,*)
					if (rel != All.id() && tgt == All.id()) {
						auto ids = pArchetype->ids_view();
						for (auto id: ids) {
							if (!id.pair())
								continue;
							if (id.id() == rel)
								return true;
						}

						return false;
					}

					// (*,X)
					if (rel == All.id() && tgt != All.id()) {
						auto ids = pArchetype->ids_view();
						for (auto id: ids) {
							if (!id.pair())
								continue;
							if (id.gen() == tgt)
								return true;
						}

						return false;
					}
				}

				if (pArchetype->has(object))
					return true;

				return allowSemanticIs && inherited_id_owner(entity, object) != EntityBad;
			}

		public:
			//! Returns the ordered component lookup path used for unqualified component lookup.
			//! Each scope is searched like a temporary component scope: the scope first, then its parents.
			//! \return View of the configured lookup scopes.
			GAIA_NODISCARD std::span<const Entity> lookup_path() const {
				return {m_componentLookupPath.data(), m_componentLookupPath.size()};
			}

			//! Replaces the ordered component lookup path used for unqualified component lookup.
			//! Each scope is searched in the order provided, and each scope walk includes its parents.
			//! \param scopes Ordered lookup scopes. Pass an empty span to clear the lookup path.
			void lookup_path(std::span<const Entity> scopes) {
				m_componentLookupPath.clear();
				m_componentLookupPath.reserve((uint32_t)scopes.size());
				for (const auto scopeEntity: scopes) {
					GAIA_ASSERT(scopeEntity != EntityBad && valid(scopeEntity) && !scopeEntity.pair());
					if (scopeEntity == EntityBad || !valid(scopeEntity) || scopeEntity.pair())
						continue;

					m_componentLookupPath.push_back(scopeEntity);
				}
			}

			//! Returns the current component scope used for component registration and relative component lookup.
			//! \return Scoped entity or EntityBad when component scope is disabled.
			GAIA_NODISCARD Entity scope() const {
				return m_componentScope;
			}

			//! Sets the current component scope used for component registration and relative component lookup.
			//! The scope entity and its ChildOf ancestors are expected to have names so we can build a path from them.
			//! \param scope Scope entity. Pass EntityBad to clear the current component scope.
			//! \return Previous component scope.
			Entity scope(Entity scope) {
				GAIA_ASSERT(scope == EntityBad || (valid(scope) && !scope.pair()));
				const auto prev = m_componentScope;
				if (scope == EntityBad || (valid(scope) && !scope.pair())) {
					m_componentScope = scope;
					invalidate_scope_path_cache();
				}
				return prev;
			}

			//! Executes @a func with a temporary component scope and restores the previous scope afterwards.
			//! Relative component lookup walks up the ChildOf hierarchy of the active scope.
			//! The scope entity and its ChildOf ancestors are expected to have names so we can build a path from them.
			//! \param scope Scope entity. Pass EntityBad to temporarily disable component scope.
			//! \param func Callable executed while the scope is active.
			template <typename Func>
			void scope(Entity scopeEntity, Func&& func) {
				struct ComponentScopeRestore final {
					World& world;
					Entity prevScope;
					~ComponentScopeRestore() {
						world.scope(prevScope);
					}
				};

				ComponentScopeRestore restore{*this, scope(scopeEntity)};
				func();
			}

			//! Finds or builds a named module hierarchy and returns the deepest scope entity.
			//! Each path segment is mapped to an entity name and connected with ChildOf relationships.
			//! \param path Dotted module path such as "gameplay.render".
			//! \param len String length. If zero, the length is calculated.
			//! \return Deepest module entity, or EntityBad when the path is invalid.
			//! \warning This only builds the scope hierarchy. Use scope(...) to activate it.
			Entity module(const char* path, uint32_t len = 0) {
				if (path == nullptr || path[0] == 0)
					return EntityBad;

				const auto l = len == 0 ? (uint32_t)GAIA_STRLEN(path, ComponentCacheItem::MaxNameLength) : len;
				if (l == 0 || l >= ComponentCacheItem::MaxNameLength)
					return EntityBad;
				if (path[l - 1] == '.')
					return EntityBad;

				Entity parent = EntityBad;
				uint32_t partBeg = 0;
				while (partBeg < l) {
					uint32_t partEnd = partBeg;
					while (partEnd < l && path[partEnd] != '.')
						++partEnd;
					if (partEnd == partBeg)
						return EntityBad;

					const auto partLen = partEnd - partBeg;
					const auto key = EntityNameLookupKey(path + partBeg, partLen, 0);
					const auto it = m_nameToEntity.find(key);
					Entity curr = EntityBad;

					if (it != m_nameToEntity.end()) {
						curr = it->second;
						if (parent != EntityBad && !static_cast<const World&>(*this).child(curr, parent)) {
							GAIA_ASSERT2(false, "Module path collides with an existing entity name outside the requested scope");
							return EntityBad;
						}
					} else {
						curr = add();
						name(curr, path + partBeg, partLen);
						if (parent != EntityBad)
							child(curr, parent);
					}

					parent = curr;
					partBeg = partEnd + 1;
				}

				return parent;
			}

			//! Checks if @a entity contains @a pair.
			//! \param entity Entity
			//! \param pair Tested pair
			//! \return True if object is present on entity. False otherwise or if any of the entities is not valid.
			//! \warning It is expected @a entity is valid. Undefined behavior otherwise.
			//! \warning Undefined behavior if @a entity changes archetype after ComponentSetter is created.
			GAIA_NODISCARD bool has(Entity entity, Pair pair) const {
				return has(entity, (Entity)pair);
			}

			//! Checks if @a entity contains the component @a T.
			//! \tparam T Component
			//! \param entity Entity
			//! \return True if the component is present on entity.
			//! \warning It is expected @a entity is valid. Undefined behavior otherwise.
			//! \warning Undefined behavior if @a entity changes archetype after ComponentSetter is created.
			template <typename T>
			GAIA_NODISCARD bool has(Entity entity) const {
				GAIA_ASSERT(valid(entity));

				using FT = typename component_type_t<T>::TypeFull;
				const auto compEntity = [&]() {
					if constexpr (is_pair<FT>::value) {
						const auto* pRel = comp_cache().template find<typename FT::rel>();
						const auto* pTgt = comp_cache().template find<typename FT::tgt>();
						if (pRel == nullptr || pTgt == nullptr)
							return EntityBad;

						const auto rel = pRel->entity;
						const auto tgt = pTgt->entity;
						return (Entity)Pair(rel, tgt);
					} else {
						const auto* pItem = comp_cache().template find<FT>();
						return pItem != nullptr ? pItem->entity : EntityBad;
					}
				}();
				if (compEntity == EntityBad)
					return false;

				if constexpr (supports_out_of_line_component<FT>()) {
					const auto* pItem = comp_cache().template find<FT>();
					if (pItem != nullptr && is_out_of_line_component(pItem->entity)) {
						const auto* pStore = sparse_component_store<FT>(pItem->entity);
						return pStore != nullptr && (pStore->has(entity) || inherited_id_owner(entity, compEntity) != EntityBad);
					}
				}

				const auto& ec = m_recs.entities[entity.id()];
				if (is_req_del(ec))
					return false;

				return ec.pArchetype->has(compEntity) || inherited_id_owner(entity, compEntity) != EntityBad;
			}

			//----------------------------------------------------------------------

			//! Assigns a @a name to @a entity. Ignored if used with pair.
			//! The string is copied and kept internally.
			//! \param entity Entity
			//! \param name A null-terminated string
			//! \param len String length. If zero, the length is calculated
			//! \warning It is expected @a entity is valid. Undefined behavior otherwise.
			//! \warning Name is expected to be unique. If it is not this function does nothing.
			//! \warning The name can't contain the character '.'. This character is reserved for hierarchical lookups
			//!          such as "parent.child.subchild".
			void name(Entity entity, const char* name, uint32_t len = 0) {
				EntityBuilder(*this, entity).name(name, len);
			}

			//! Assigns a @a name to @a entity. Ignored if used with pair.
			//! The string is NOT copied. Your are responsible for its lifetime.
			//! \param entity Entity
			//! \param name Pointer to a stable null-terminated string
			//! \param len String length. If zero, the length is calculated
			//! \warning It is expected @a entity is valid. Undefined behavior otherwise.
			//! \warning The name is expected to be unique. If it is not this function does nothing.
			//! \warning The name can't contain the character '.'. This character is reserved for hierarchical lookups
			//!          such as "parent.child.subchild".
			//! \warning In this case the string is NOT copied and NOT stored internally. You are responsible for its
			//!          lifetime. The pointer, contents, and length need to stay stable. Otherwise, any time your
			//!          storage tries to move or rewrite the string you have to unset the name before it happens
			//!          and set it anew after the change is done.
			void name_raw(Entity entity, const char* name, uint32_t len = 0) {
				EntityBuilder(*this, entity).name_raw(name, len);
			}

			//! Returns the name assigned to @a entity.
			//! \param entity Entity
			//! \return Name assigned to entity. Empty view when there is no name.
			//! \warning It is expected @a entity is valid. Undefined behavior otherwise.
			GAIA_NODISCARD util::str_view name(Entity entity) const {
				if (entity.pair())
					return {};

				const auto& ec = m_recs.entities[entity.id()];
				const auto compIdx = core::get_index(ec.pChunk->ids_view(), GAIA_ID(EntityDesc));
				if (compIdx == BadIndex)
					return {};

				const auto* pDesc = reinterpret_cast<const EntityDesc*>(ec.pChunk->comp_ptr(compIdx, ec.row));
				GAIA_ASSERT(core::check_alignment(pDesc));
				return {pDesc->name, pDesc->name_len};
			}

			//! Returns the entity name assigned to @a entityId.
			//! \param entityId EntityId
			//! \return Name assigned through the entity naming API. Empty view when there is no entity name.
			//! \warning It is expected @a entityId is valid. Undefined behavior otherwise.
			GAIA_NODISCARD util::str_view name(EntityId entityId) const {
				auto entity = get(entityId);
				return name(entity);
			}

			//----------------------------------------------------------------------

			//! Resolves @a name in the world naming system.
			//! Entity names and hierarchical entity paths are attempted first. If they do not match,
			//! component lookup uses the current scope, then the lookup path, then global component
			//! symbol, path and alias lookup.
			//! \param name Pointer to a stable null-terminated string
			//! \param len String length. If zero, the length is calculated
			//! \return Matching entity. EntityBad when no entity or component matches.
			GAIA_NODISCARD Entity resolve(const char* name, uint32_t len = 0) const {
				if (name == nullptr || name[0] == 0)
					return EntityBad;

				const auto l = len == 0 ? (uint32_t)GAIA_STRLEN(name, ComponentCacheItem::MaxNameLength) : len;
				GAIA_ASSERT(l < ComponentCacheItem::MaxNameLength);

				if (memchr(name, '.', l) != nullptr) {
					const auto entity = get_entity_inter(name, l);
					if (entity != EntityBad)
						return entity;
				}

				return get_inter(name, l);
			}

			//! Collects every entity and component entity that matches @a name.
			//! This is useful for diagnostics when a short lookup could refer to multiple scoped components.
			//! \param[out] out Output array cleared and then filled with unique matching entities.
			//! \param name Pointer to a stable null-terminated string.
			//! \param len String length. If zero, the length is calculated.
			void resolve(cnt::darray<Entity>& out, const char* name, uint32_t len = 0) const {
				out.clear();
				if (name == nullptr || name[0] == 0)
					return;

				const auto l = len == 0 ? (uint32_t)GAIA_STRLEN(name, ComponentCacheItem::MaxNameLength) : len;
				GAIA_ASSERT(l < ComponentCacheItem::MaxNameLength);

				auto push_unique = [&](Entity entity) {
					if (entity == EntityBad)
						return;
					for (const auto existing: out) {
						if (existing == entity)
							return;
					}
					out.push_back(entity);
				};

				push_unique(get_entity_inter(name, l));

				if (memchr(name, '.', l) == nullptr && memchr(name, ':', l) == nullptr) {
					const auto pushLookupHit = [&](const ComponentCacheItem& item) {
						push_unique(item.entity);
						return false;
					};

					(void)find_component_in_scope_chain_inter(m_componentScope, name, l, pushLookupHit);
					for (const auto scopeEntity: m_componentLookupPath) {
						if (scopeEntity == m_componentScope)
							continue;

						(void)find_component_in_scope_chain_inter(scopeEntity, name, l, pushLookupHit);
					}
				}

				if (const auto* pItem = m_compCache.symbol(name, l); pItem != nullptr)
					push_unique(pItem->entity);

				if (const auto* pItem = m_compCache.path(name, l); pItem != nullptr) {
					push_unique(pItem->entity);
				} else {
					const auto needle = util::str_view(name, l);
					m_compCache.for_each_item([&](const ComponentCacheItem& item) {
						if (item.path.view() == needle)
							push_unique(item.entity);
					});
				}

				if (out.empty() && memchr(name, '.', l) == nullptr && memchr(name, ':', l) == nullptr) {
					if (const auto* pItem = m_compCache.short_symbol(name, l); pItem != nullptr)
						push_unique(pItem->entity);
				}

				push_unique(alias(name, l));
			}

			//! Returns the entity assigned a name @a name.
			//! This is a convenience alias for resolve(name).
			//! \param name Pointer to a stable null-terminated string
			//! \param len String length. If zero, the length is calculated
			//! \return Matching entity. EntityBad if there is nothing to return.
			GAIA_NODISCARD Entity get(const char* name, uint32_t len = 0) const {
				return resolve(name, len);
			}

		private:
			GAIA_NODISCARD Entity find_named_entity_inter(const char* name, uint32_t len = 0) const {
				if (name == nullptr || name[0] == 0)
					return EntityBad;

				const auto key = EntityNameLookupKey(name, len, 0);
				const auto it = m_nameToEntity.find(key);
				return it != m_nameToEntity.end() ? it->second : EntityBad;
			}

			GAIA_NODISCARD Entity get_entity_inter(const char* name, uint32_t len = 0) const {
				if (name == nullptr || name[0] == 0)
					return EntityBad;

				if (len == 0) {
					while (name[len] != '\0')
						++len;
				}

				Entity parent = EntityBad;
				Entity child = EntityBad;
				uint32_t posDot = 0;
				std::span<const char> str(name, len);

				posDot = core::get_index(str, '.');
				if (posDot == BadIndex)
					return find_named_entity_inter(str.data(), (uint32_t)str.size());

				if (posDot == 0)
					return EntityBad;

				parent = find_named_entity_inter(str.data(), posDot);
				if (parent == EntityBad)
					return EntityBad;

				str = str.subspan(posDot + 1);
				while (!str.empty()) {
					posDot = core::get_index(str, '.');

					if (posDot == BadIndex) {
						child = find_named_entity_inter(str.data(), (uint32_t)str.size());
						if (child == EntityBad || !this->child(child, parent))
							return EntityBad;

						return child;
					}

					if (posDot == 0)
						return EntityBad;

					child = find_named_entity_inter(str.data(), posDot);
					if (child == EntityBad || !this->child(child, parent))
						return EntityBad;

					parent = child;
					str = str.subspan(posDot + 1);
				}

				return parent;
			}

			GAIA_NODISCARD Entity get_inter(const char* name, uint32_t len = 0) const {
				if (name == nullptr || name[0] == 0)
					return EntityBad;

				auto key = EntityNameLookupKey(name, len, 0);
				const auto l = key.len();

				if ((m_componentScope != EntityBad || !m_componentLookupPath.empty()) && memchr(name, '.', l) == nullptr &&
						memchr(name, ':', l) == nullptr) {
					const auto* pScopedItem = resolve_component_name_inter(name, l);
					if (pScopedItem != nullptr) {
						const auto it = m_nameToEntity.find(key);
						if (it == m_nameToEntity.end())
							return pScopedItem->entity;

						if (m_compCache.find(it->second) != nullptr)
							return pScopedItem->entity;
					}
				}

				const auto it = m_nameToEntity.find(key);
				if (it != m_nameToEntity.end())
					return it->second;

				// Name not found. This might be a component so check the component cache
				const auto* pItem = resolve_component_name_inter(name, l);
				if (pItem != nullptr)
					return pItem->entity;

				const auto aliasEntity = alias(name, l);
				if (aliasEntity != EntityBad)
					return aliasEntity;

				// No entity with the given name exists. Return a bad entity
				return EntityBad;
			}

		public:
			//----------------------------------------------------------------------

			//! Returns relations for @a target.
			//! \param target Target entity
			//! \warning It is expected @a target is valid. Undefined behavior otherwise.
			GAIA_NODISCARD const cnt::set<EntityLookupKey>* relations(Entity target) const {
				const auto it = m_tgtToRel.find(EntityLookupKey(target));
				if (it == m_tgtToRel.end())
					return nullptr;

				return &it->second;
			}

			//! Returns the first relationship relation for the @a target entity on @a entity.
			//! \param entity Source entity
			//! \param target Target entity
			//! \return Relationship target. EntityBad if there is nothing to return.
			//! \warning It is expected @a entity is valid. Undefined behavior otherwise.
			GAIA_NODISCARD Entity relation(Entity entity, Entity target) const {
				GAIA_ASSERT(valid(entity));
				if (!valid(target))
					return EntityBad;

				const auto itAdjunctRels = m_srcToExclusiveAdjunctRel.find(EntityLookupKey(entity));
				if (itAdjunctRels != m_srcToExclusiveAdjunctRel.end()) {
					for (auto relKey: itAdjunctRels->second) {
						const auto relation = relKey;
						const auto* pStore = exclusive_adjunct_store(relation);
						if (pStore == nullptr)
							continue;

						if (exclusive_adjunct_target(*pStore, entity) == target)
							return relation;
					}
				}

				const auto& ec = fetch(entity);
				const auto* pArchetype = ec.pArchetype;

				// Early exit if there are no pairs on the archetype
				if (pArchetype->pairs() == 0)
					return EntityBad;

				const auto indices = pArchetype->pair_tgt_indices(target);
				if (indices.empty())
					return EntityBad;

				const auto ids = pArchetype->ids_view();
				const auto e = ids[indices[0]];
				const auto& ecRel = m_recs.entities[e.id()];
				return *ecRel.pEntity;
			}

			//! Returns the relationship relations for the @a target entity on @a entity.
			//! \param entity Source entity
			//! \param target Target entity
			//! \param func void(Entity relation) functor executed for relationship relation found.
			//! \warning It is expected @a entity is valid. Undefined behavior otherwise.
			template <typename Func>
			void relations(Entity entity, Entity target, Func func) const {
				GAIA_ASSERT(valid(entity));
				if (!valid(target))
					return;

				const auto itAdjunctRels = m_srcToExclusiveAdjunctRel.find(EntityLookupKey(entity));
				if (itAdjunctRels != m_srcToExclusiveAdjunctRel.end()) {
					for (auto relKey: itAdjunctRels->second) {
						const auto relation = relKey;
						const auto* pStore = exclusive_adjunct_store(relation);
						if (pStore == nullptr)
							continue;

						if (exclusive_adjunct_target(*pStore, entity) == target)
							func(relation);
					}
				}

				const auto& ec = fetch(entity);
				const auto* pArchetype = ec.pArchetype;

				// Early exit if there are no pairs on the archetype
				if (pArchetype->pairs() == 0)
					return;

				const auto ids = pArchetype->ids_view();
				for (auto idsIdx: pArchetype->pair_tgt_indices(target)) {
					const auto e = ids[idsIdx];

					const auto& ecRel = m_recs.entities[e.id()];
					auto relation = *ecRel.pEntity;
					func(relation);
				}
			}

			//! Returns the relationship relations for the @a target entity on @a entity.
			//! \param entity Source entity
			//! \param target Target entity
			//! \param func bool(Entity relation) functor executed for relationship relation found.
			//!             Stops if false is returned.
			//! \warning It is expected @a entity is valid. Undefined behavior otherwise.
			template <typename Func>
			void relations_if(Entity entity, Entity target, Func func) const {
				GAIA_ASSERT(valid(entity));
				if (!valid(target))
					return;

				const auto itAdjunctRels = m_srcToExclusiveAdjunctRel.find(EntityLookupKey(entity));
				if (itAdjunctRels != m_srcToExclusiveAdjunctRel.end()) {
					for (auto relKey: itAdjunctRels->second) {
						const auto relation = relKey;
						const auto* pStore = exclusive_adjunct_store(relation);
						if (pStore == nullptr)
							continue;

						if (exclusive_adjunct_target(*pStore, entity) == target) {
							if (!func(relation))
								return;
						}
					}
				}

				const auto& ec = fetch(entity);
				const auto* pArchetype = ec.pArchetype;

				// Early exit if there are no pairs on the archetype
				if (pArchetype->pairs() == 0)
					return;

				const auto ids = pArchetype->ids_view();
				for (auto idsIdx: pArchetype->pair_tgt_indices(target)) {
					const auto e = ids[idsIdx];

					const auto& ecRel = m_recs.entities[e.id()];
					auto relation = *ecRel.pEntity;
					if (!func(relation))
						return;
				}
			}

			//! Returns the cached transitive `Is` descendants for a target entity.
			//! The cache is rebuilt lazily and cleared whenever an `Is` edge changes.
			GAIA_NODISCARD const cnt::darray<Entity>& as_relations_trav_cache(Entity target) const {
				const auto key = EntityLookupKey(target);
				const auto itCache = m_entityToAsRelationsTravCache.find(key);
				if (itCache != m_entityToAsRelationsTravCache.end())
					return itCache->second;

				auto& cache = m_entityToAsRelationsTravCache[key];
				const auto it = m_entityToAsRelations.find(key);
				if (it == m_entityToAsRelations.end())
					return cache;

				//! Small inline traversal stack for the common shallow inheritance case.
				cnt::darray_ext<EntityLookupKey, 32> stack;
				stack.reserve((uint32_t)it->second.size());
				for (auto relation: it->second)
					stack.push_back(relation);

				while (!stack.empty()) {
					const auto relationKey = stack.back();
					stack.pop_back();

					const auto relation = relationKey.entity();
					cache.push_back(relation);

					const auto itChild = m_entityToAsRelations.find(relationKey);
					if (itChild == m_entityToAsRelations.end())
						continue;

					for (auto childRelation: itChild->second)
						stack.push_back(childRelation);
				}

				return cache;
			}

			//! Returns the cached transitive `Is` targets for a relation entity.
			//! The cache is rebuilt lazily and cleared whenever an `Is` edge changes.
			GAIA_NODISCARD const cnt::darray<Entity>& as_targets_trav_cache(Entity relation) const {
				const auto key = EntityLookupKey(relation);
				const auto itCache = m_entityToAsTargetsTravCache.find(key);
				if (itCache != m_entityToAsTargetsTravCache.end())
					return itCache->second;

				auto& cache = m_entityToAsTargetsTravCache[key];
				const auto it = m_entityToAsTargets.find(key);
				if (it == m_entityToAsTargets.end())
					return cache;

				//! Small inline traversal stack for the common shallow inheritance case.
				cnt::darray_ext<EntityLookupKey, 16> stack;
				stack.reserve((uint32_t)it->second.size());
				for (auto target: it->second)
					stack.push_back(target);

				while (!stack.empty()) {
					const auto targetKey = stack.back();
					stack.pop_back();

					const auto target = targetKey.entity();
					cache.push_back(target);

					const auto itChild = m_entityToAsTargets.find(targetKey);
					if (itChild == m_entityToAsTargets.end())
						continue;

					for (auto childTarget: itChild->second)
						stack.push_back(childTarget);
				}

				return cache;
			}

			//! Returns the cached unlimited upward traversal chain for `(relation, source)`.
			//! The cache excludes the source entity itself and is cleared whenever a pair edge changes.
			GAIA_NODISCARD const cnt::darray<Entity>& targets_trav_cache(Entity relation, Entity source) const {
				const auto key = EntityLookupKey(Pair(relation, source));
				const auto itCache = m_targetsTravCache.find(key);
				if (itCache != m_targetsTravCache.end())
					return itCache->second;

				auto& cache = m_targetsTravCache[key];
				if (!valid(relation) || !valid(source))
					return cache;

				auto curr = source;
				GAIA_FOR(MAX_TRAV_DEPTH) {
					const auto next = target(curr, relation);
					if (next == EntityBad || next == curr)
						break;

					cache.push_back(next);
					curr = next;
				}

				return cache;
			}

			//! Returns the cached deduped direct targets for wildcard target traversal on @a source.
			//! The cache is keyed by `source` and cleared whenever a pair edge changes.
			GAIA_NODISCARD const cnt::darray<Entity>& targets_all_cache(Entity source) const {
				const auto key = EntityLookupKey(source);
				const auto itCache = m_targetsAllCache.find(key);
				if (itCache != m_targetsAllCache.end())
					return itCache->second;

				auto& cache = m_targetsAllCache[key];
				if (!valid(source))
					return cache;

				const auto visitStamp = next_entity_visit_stamp();

				const auto itAdjunctRels = m_srcToExclusiveAdjunctRel.find(key);
				if (itAdjunctRels != m_srcToExclusiveAdjunctRel.end()) {
					for (auto rel: itAdjunctRels->second) {
						const auto* pStore = exclusive_adjunct_store(rel);
						if (pStore == nullptr)
							continue;

						const auto target = exclusive_adjunct_target(*pStore, source);
						if (target != EntityBad && try_mark_entity_visited(target, visitStamp))
							cache.push_back(target);
					}
				}

				const auto& ec = fetch(source);
				const auto* pArchetype = ec.pArchetype;
				if (pArchetype->pairs() == 0)
					return cache;

				const auto ids = pArchetype->ids_view();
				for (auto idsIdx: pArchetype->pair_indices()) {
					const auto id = ids[idsIdx];
					const auto target = pair_target_if_alive(id);
					if (target == EntityBad)
						continue;
					if (try_mark_entity_visited(target, visitStamp))
						cache.push_back(target);
				}

				return cache;
			}

			//! Returns the cached deduped direct sources for wildcard source traversal on @a target.
			//! The cache is keyed by `target` and cleared whenever a pair edge changes.
			GAIA_NODISCARD const cnt::darray<Entity>& sources_all_cache(Entity target) const {
				const auto key = EntityLookupKey(target);
				const auto itCache = m_sourcesAllCache.find(key);
				if (itCache != m_sourcesAllCache.end())
					return itCache->second;

				auto& cache = m_sourcesAllCache[key];
				if (!valid(target))
					return cache;

				const auto visitStamp = next_entity_visit_stamp();

				for (const auto& [relKey, store]: m_exclusiveAdjunctByRel) {
					(void)relKey;
					const auto* pSources = exclusive_adjunct_sources(store, target);
					if (pSources == nullptr)
						continue;

					for (auto source: *pSources) {
						if (valid(source) && try_mark_entity_visited(source, visitStamp))
							cache.push_back(source);
					}
				}

				const auto pair = Pair(All, target);
				const auto it = m_entityToArchetypeMap.find(EntityLookupKey(pair));
				if (it == m_entityToArchetypeMap.end())
					return cache;

				for (const auto& record: it->second) {
					const auto* pArchetype = record.pArchetype;
					if (pArchetype->is_req_del())
						continue;

					for (const auto* pChunk: pArchetype->chunks()) {
						const auto entities = pChunk->entity_view();
						GAIA_EACH(entities) {
							const auto source = entities[i];
							if (!valid(source))
								continue;
							if (try_mark_entity_visited(source, visitStamp))
								cache.push_back(source);
						}
					}
				}

				return cache;
			}

			//! Traverses relationship targets upwards starting from @a source.
			//! Disabled entities act as traversal barriers and are not yielded.
			template <typename Func>
			void targets_trav(Entity relation, Entity source, Func func) const {
				if (!valid(relation) || !valid(source))
					return;

				auto curr = source;
				GAIA_FOR(MAX_TRAV_DEPTH) {
					const auto next = target(curr, relation);
					if (next == EntityBad || next == curr)
						break;
					if (!enabled(next))
						break;

					func(next);
					curr = next;
				}
			}

			//! Traverses relationship targets upwards starting from @a source.
			//! Disabled entities act as traversal barriers and are not yielded.
			//! \return True if traversal was stopped by @a func, false otherwise.
			template <typename Func>
			GAIA_NODISCARD bool targets_trav_if(Entity relation, Entity source, Func func) const {
				if (!valid(relation) || !valid(source))
					return false;

				auto curr = source;
				GAIA_FOR(MAX_TRAV_DEPTH) {
					const auto next = target(curr, relation);
					if (next == EntityBad || next == curr)
						break;
					if (!enabled(next))
						break;

					if (!func(next))
						return true;
					curr = next;
				}

				return false;
			}

			//! Returns the cached unlimited breadth-first descendant traversal for `(relation, rootTarget)`.
			//! The cache excludes the root target itself and is cleared whenever a pair edge changes.
			GAIA_NODISCARD const cnt::darray<Entity>& sources_bfs_trav_cache(Entity relation, Entity rootTarget) const {
				const auto key = EntityLookupKey(Pair(relation, rootTarget));
				const auto itCache = m_srcBfsTravCache.find(key);
				if (itCache != m_srcBfsTravCache.end())
					return itCache->second;

				auto& cache = m_srcBfsTravCache[key];
				if (!valid(relation) || !valid(rootTarget))
					return cache;

				cnt::darray_ext<Entity, 32> queue;
				queue.push_back(rootTarget);

				cnt::set<EntityLookupKey> visited;
				visited.insert(EntityLookupKey(rootTarget));

				for (uint32_t i = 0; i < queue.size(); ++i) {
					const auto currTarget = queue[i];

					cnt::darray<Entity> children;
					sources(relation, currTarget, [&](Entity source) {
						const auto keySource = EntityLookupKey(source);
						const auto ins = visited.insert(keySource);
						if (!ins.second)
							return;

						children.push_back(source);
					});

					core::sort(children, [](Entity left, Entity right) {
						return left.id() < right.id();
					});

					for (auto child: children) {
						cache.push_back(child);
						queue.push_back(child);
					}
				}

				return cache;
			}

			//! Returns the cached fragmenting relation depth used by depth-ordered iteration for `(relation, target)`.
			//! The returned value is `1` for direct dependents of a root/source with no further relation targets and grows
			//! by one per level. For multi-target relations, the deepest target chain determines the result.
			GAIA_NODISCARD uint32_t depth_order_cache(Entity relation, Entity sourceTarget) const {
				const auto key = EntityLookupKey(Pair(relation, sourceTarget));
				const auto itCache = m_depthOrderCache.find(key);
				if (itCache != m_depthOrderCache.end()) {
					GAIA_ASSERT(itCache->second != GroupIdMax && "depth_order requires an acyclic relation graph");
					return itCache->second;
				}

				if (!valid(relation) || !valid(sourceTarget))
					return 0;

				// Mark this node as in-flight so cycles trip a debug assert instead of recursing forever.
				m_depthOrderCache[key] = GroupIdMax;

				uint32_t depth = 1;
				targets(sourceTarget, relation, [&](Entity next) {
					const auto nextDepth = depth_order_cache(relation, next);
					if (nextDepth == 0)
						return;
					const auto candidate = nextDepth + 1;
					if (candidate > depth)
						depth = candidate;
				});

				m_depthOrderCache[key] = depth;
				return depth;
			}

			template <typename Func>
			void as_relations_trav(Entity target, Func func) const {
				if (!valid(target))
					return;

				const auto& relations = as_relations_trav_cache(target);
				for (auto relation: relations)
					func(relation);
			}

			template <typename Func>
			GAIA_NODISCARD bool as_relations_trav_if(Entity target, Func func) const {
				if (!valid(target))
					return false;

				const auto& relations = as_relations_trav_cache(target);
				for (auto relation: relations) {
					if (func(relation))
						return true;
				}

				return false;
			}

			//----------------------------------------------------------------------

			//! Returns targets for @a relation.
			//! \param relation Relation entity
			//! \warning It is expected @a entity is valid. Undefined behavior otherwise.
			GAIA_NODISCARD const cnt::set<EntityLookupKey>* targets(Entity relation) const {
				const auto it = m_relToTgt.find(EntityLookupKey(relation));
				if (it == m_relToTgt.end())
					return nullptr;

				return &it->second;
			}

			//! Returns the first relationship target for the @a relation entity on @a entity.
			//! \param entity Source entity
			//! \param relation Relation entity
			//! \return Relationship target. EntityBad if there is nothing to return.
			//! \warning It is expected @a entity is valid. Undefined behavior otherwise.
			GAIA_NODISCARD Entity target(Entity entity, Entity relation) const {
				if (!valid(entity))
					return EntityBad;
				if (relation != All && !valid(relation))
					return EntityBad;

				if (relation == All) {
					const auto& targets = targets_all_cache(entity);
					return targets.empty() ? EntityBad : targets[0];
				}

				if (is_exclusive_dont_fragment_relation(relation)) {
					const auto* pStore = exclusive_adjunct_store(relation);
					if (pStore == nullptr)
						return EntityBad;

					return exclusive_adjunct_target(*pStore, entity);
				}

				const auto& ec = fetch(entity);
				const auto* pArchetype = ec.pArchetype;

				// Early exit if there are no pairs on the archetype
				if (pArchetype->pairs() == 0)
					return EntityBad;

				const auto ids = pArchetype->ids_view();
				for (auto idsIdx: pArchetype->pair_rel_indices(relation)) {
					const auto e = ids[idsIdx];
					const auto target = pair_target_if_alive(e);
					if (target == EntityBad)
						continue;
					return target;
				}

				return EntityBad;
			}

			//! Returns the relationship targets for the @a relation entity on @a entity.
			//! \param entity Source entity
			//! \param relation Relation entity
			//! \param func void(Entity target) functor executed for relationship target found.
			//! \warning It is expected @a entity is valid. Undefined behavior otherwise.
			template <typename Func>
			void targets(Entity entity, Entity relation, Func func) const {
				if (!valid(entity))
					return;
				if (relation != All && !valid(relation))
					return;

				if (relation == All) {
					for (auto target: targets_all_cache(entity))
						func(target);
					return;
				}

				if (is_exclusive_dont_fragment_relation(relation)) {
					const auto target = this->target(entity, relation);
					if (target != EntityBad)
						func(target);
					return;
				}

				const auto& ec = fetch(entity);
				const auto* pArchetype = ec.pArchetype;

				// Early exit if there are no pairs on the archetype
				if (pArchetype->pairs() == 0)
					return;

				const auto ids = pArchetype->ids_view();
				for (auto idsIdx: pArchetype->pair_rel_indices(relation)) {
					const auto e = ids[idsIdx];
					const auto target = pair_target_if_alive(e);
					if (target == EntityBad)
						continue;
					func(target);
				}
			}

			//! Returns the relationship targets for the @a relation entity on @a entity.
			//! \param entity Source entity
			//! \param relation Relation entity
			//! \param func bool(Entity target) functor executed for relationship target found.
			//!             Stops if false is returned.
			//! \warning It is expected @a entity is valid. Undefined behavior otherwise.
			template <typename Func>
			void targets_if(Entity entity, Entity relation, Func func) const {
				GAIA_ASSERT(valid(entity));
				if (relation != All && !valid(relation))
					return;

				if (relation == All) {
					for (auto target: targets_all_cache(entity)) {
						if (!func(target))
							return;
					}
					return;
				}

				if (is_exclusive_dont_fragment_relation(relation)) {
					const auto target = this->target(entity, relation);
					if (target != EntityBad)
						(void)func(target);
					return;
				}

				const auto& ec = fetch(entity);
				const auto* pArchetype = ec.pArchetype;

				// Early exit if there are no pairs on the archetype
				if (pArchetype->pairs() == 0)
					return;

				const auto ids = pArchetype->ids_view();
				for (auto idsIdx: pArchetype->pair_rel_indices(relation)) {
					const auto e = ids[idsIdx];
					const auto target = pair_target_if_alive(e);
					if (target == EntityBad)
						continue;
					if (!func(target))
						return;
				}
			}

			//! Resolves the target of an archetype-stored exact pair id, skipping stale cleanup-time targets.
			GAIA_NODISCARD Entity pair_target_if_alive(Entity pair) const {
				GAIA_ASSERT(pair.pair());
				if (!valid_entity_id((EntityId)pair.gen()))
					return EntityBad;

				const auto& ecTarget = m_recs.entities[pair.gen()];
				if (ecTarget.pEntity == nullptr)
					return EntityBad;

				const auto target = *ecTarget.pEntity;
				return valid(target) ? target : EntityBad;
			}

			//! Returns relationship sources for the @a relation and @a target.
			//! \param relation Relation entity
			//! \param target Target entity
			//! \param func void(Entity source) functor executed for each source entity found.
			//! \warning It is expected @a relation and @a target are valid. Undefined behavior otherwise.
			template <typename Func>
			void sources(Entity relation, Entity target, Func func) const {
				if ((relation != All && !valid(relation)) || !valid(target))
					return;

				if (relation == All) {
					for (auto source: sources_all_cache(target))
						func(source);
					return;
				}

				if (is_exclusive_dont_fragment_relation(relation)) {
					const auto* pStore = exclusive_adjunct_store(relation);
					if (pStore == nullptr)
						return;

					const auto* pSources = exclusive_adjunct_sources(*pStore, target);
					if (pSources == nullptr)
						return;

					for (auto source: *pSources) {
						if (!valid(source))
							continue;

						func(source);
					}
					return;
				}

				const auto pair = Pair(relation, target);
				const auto it = m_entityToArchetypeMap.find(EntityLookupKey(pair));
				if (it == m_entityToArchetypeMap.end())
					return;

				for (const auto& record: it->second) {
					const auto* pArchetype = record.pArchetype;
					if (pArchetype->is_req_del())
						continue;

					for (const auto* pChunk: pArchetype->chunks()) {
						auto entities = pChunk->entity_view();
						GAIA_EACH(entities) {
							const auto source = entities[i];
							if (!valid(source))
								continue;
							func(source);
						}
					}
				}
			}

			//! Returns relationship sources for the @a relation and @a target.
			//! \param relation Relation entity
			//! \param target Target entity
			//! \param func bool(Entity source) functor executed for each source entity found.
			//!             Stops if false is returned.
			//! \warning It is expected @a relation and @a target are valid. Undefined behavior otherwise.
			template <typename Func>
			void sources_if(Entity relation, Entity target, Func func) const {
				if ((relation != All && !valid(relation)) || !valid(target))
					return;

				if (relation == All) {
					for (auto source: sources_all_cache(target)) {
						if (!func(source))
							return;
					}
					return;
				}

				if (is_exclusive_dont_fragment_relation(relation)) {
					const auto* pStore = exclusive_adjunct_store(relation);
					if (pStore == nullptr)
						return;

					const auto* pSources = exclusive_adjunct_sources(*pStore, target);
					if (pSources == nullptr)
						return;

					for (auto source: *pSources) {
						if (!valid(source))
							continue;

						if (!func(source))
							return;
					}
					return;
				}

				const auto pair = Pair(relation, target);
				const auto it = m_entityToArchetypeMap.find(EntityLookupKey(pair));
				if (it == m_entityToArchetypeMap.end())
					return;

				for (const auto& record: it->second) {
					const auto* pArchetype = record.pArchetype;
					if (pArchetype->is_req_del())
						continue;

					for (const auto* pChunk: pArchetype->chunks()) {
						auto entities = pChunk->entity_view();
						GAIA_EACH(entities) {
							const auto source = entities[i];
							if (!valid(source))
								continue;
							if (!func(source))
								return;
						}
					}
				}
			}

		private:
			GAIA_NODISCARD uint64_t next_entity_visit_stamp() const {
				++m_entityVisitStamp;
				if (m_entityVisitStamp != 0)
					return m_entityVisitStamp;

				const auto cnt = (uint32_t)m_entityVisitStamps.size();
				GAIA_FOR(cnt) {
					m_entityVisitStamps[i] = 0;
				}

				m_entityVisitStamp = 1;
				return m_entityVisitStamp;
			}

			GAIA_NODISCARD bool try_mark_entity_visited(Entity entity, uint64_t stamp) const {
				GAIA_ASSERT(!entity.pair());
				if (entity.id() >= m_entityVisitStamps.size())
					m_entityVisitStamps.resize(m_recs.entities.size(), 0);

				auto& slot = m_entityVisitStamps[entity.id()];
				if (slot == stamp)
					return false;

				slot = stamp;
				return true;
			}

			template <typename Func>
			GAIA_NODISCARD bool for_each_inherited_term_entity(Entity term, Func&& func) const {
				cnt::set<EntityLookupKey> seen;
				const auto it = m_entityToArchetypeMap.find(EntityLookupKey(term));
				if (it == m_entityToArchetypeMap.end())
					return true;

				for (const auto& record: it->second) {
					const auto* pArchetype = record.pArchetype;
					if (pArchetype->is_req_del())
						continue;

					for (const auto* pChunk: pArchetype->chunks()) {
						const auto entities = pChunk->entity_view();
						GAIA_EACH(entities) {
							const auto entity = entities[i];
							const auto entityKey = EntityLookupKey(entity);
							if (seen.contains(entityKey))
								continue;
							seen.insert(entityKey);

							if (!func(entity))
								return false;

							const auto& descendants = as_relations_trav_cache(entity);
							for (const auto descendant: descendants) {
								const auto descendantKey = EntityLookupKey(descendant);
								if (seen.contains(descendantKey))
									continue;
								seen.insert(descendantKey);

								if (!func(descendant))
									return false;
							}
						}
					}
				}

				return true;
			}

			//! Counts entities matching a direct term using the narrowest available store/index.
			//! This is used by direct non-fragmenting query fast paths to avoid world-wide row filtering.
			GAIA_NODISCARD uint32_t count_direct_term_entities_inter(Entity term, bool allowSemanticIs) const {
				if (term == EntityBad)
					return 0;

				if (allowSemanticIs && term.pair() && term.id() == Is.id() && !is_wildcard(term.gen())) {
					const auto target = get(term.gen());
					if (!valid(target))
						return 0;

					return (uint32_t)as_relations_trav_cache(target).size() + 1;
				}

				if (allowSemanticIs && !is_wildcard(term) && valid(term) && target(term, OnInstantiate) == Inherit) {
					uint32_t cnt = 0;
					(void)for_each_inherited_term_entity(term, [&](Entity) {
						++cnt;
						return true;
					});
					return cnt;
				}

				if (term.pair() && is_exclusive_dont_fragment_relation(entity_from_id(*this, term.id()))) {
					const auto relation = entity_from_id(*this, term.id());
					const auto* pStore = exclusive_adjunct_store(relation);
					if (pStore == nullptr)
						return 0;

					if (is_wildcard(term.gen()))
						return pStore->srcToTgtCnt;

					const auto* pSources = exclusive_adjunct_sources(*pStore, entity_from_id(*this, term.gen()));
					return pSources != nullptr ? (uint32_t)pSources->size() : 0;
				}

				if (!term.pair() && is_non_fragmenting_out_of_line_component(term)) {
					const auto it = m_sparseComponentsByComp.find(EntityLookupKey(term));
					return it != m_sparseComponentsByComp.end() ? it->second.func_count(it->second.pStore) : 0;
				}

				const auto it = m_entityToArchetypeMap.find(EntityLookupKey(term));
				if (it == m_entityToArchetypeMap.end())
					return 0;

				uint32_t cnt = 0;
				for (const auto& record: it->second) {
					const auto* pArchetype = record.pArchetype;
					if (pArchetype->is_req_del())
						continue;
					for (const auto* pChunk: pArchetype->chunks())
						cnt += pChunk->size();
				}

				return cnt;
			}

			//! Appends entities matching a direct term using the narrowest available store/index.
			void collect_direct_term_entities_inter(Entity term, cnt::darray<Entity>& out, bool allowSemanticIs) const {
				if (term == EntityBad)
					return;

				if (allowSemanticIs && term.pair() && term.id() == Is.id() && !is_wildcard(term.gen())) {
					const auto target = get(term.gen());
					if (!valid(target))
						return;

					out.push_back(target);
					const auto& relations = as_relations_trav_cache(target);
					out.reserve(out.size() + (uint32_t)relations.size());
					for (auto relation: relations)
						out.push_back(relation);
					return;
				}

				if (allowSemanticIs && !is_wildcard(term) && valid(term) && target(term, OnInstantiate) == Inherit) {
					(void)for_each_inherited_term_entity(term, [&](Entity entity) {
						out.push_back(entity);
						return true;
					});
					return;
				}

				if (term.pair() && is_exclusive_dont_fragment_relation(entity_from_id(*this, term.id()))) {
					const auto relation = entity_from_id(*this, term.id());
					const auto* pStore = exclusive_adjunct_store(relation);
					if (pStore == nullptr)
						return;

					if (is_wildcard(term.gen())) {
						out.reserve(out.size() + pStore->srcToTgtCnt);
						const auto cnt = (uint32_t)pStore->srcToTgt.size();
						GAIA_FOR(cnt) {
							const auto target = pStore->srcToTgt[i];
							if (target == EntityBad)
								continue;
							if (!m_recs.entities.has(i))
								continue;
							out.push_back(EntityContainer::handle(m_recs.entities[i]));
						}
						return;
					}

					const auto* pSources = exclusive_adjunct_sources(*pStore, entity_from_id(*this, term.gen()));
					if (pSources == nullptr)
						return;

					out.reserve(out.size() + (uint32_t)pSources->size());
					for (auto source: *pSources)
						out.push_back(source);
					return;
				}

				if (!term.pair() && is_non_fragmenting_out_of_line_component(term)) {
					const auto it = m_sparseComponentsByComp.find(EntityLookupKey(term));
					if (it != m_sparseComponentsByComp.end())
						it->second.func_collect_entities(it->second.pStore, out);
					return;
				}

				const auto it = m_entityToArchetypeMap.find(EntityLookupKey(term));
				if (it == m_entityToArchetypeMap.end())
					return;

				for (const auto& record: it->second) {
					const auto* pArchetype = record.pArchetype;
					if (pArchetype->is_req_del())
						continue;

					for (const auto* pChunk: pArchetype->chunks()) {
						const auto entities = pChunk->entity_view();
						out.reserve(out.size() + (uint32_t)entities.size());
						GAIA_EACH(entities)
						out.push_back(entities[i]);
					}
				}
			}

			//! Visits entities matching a direct term without materializing a temporary entity array first.
			GAIA_NODISCARD bool for_each_direct_term_entity_inter(
					Entity term, void* ctx, bool (*func)(void*, Entity), bool allowSemanticIs) const {
				if (term == EntityBad)
					return true;

				if (allowSemanticIs && term.pair() && term.id() == Is.id() && !is_wildcard(term.gen())) {
					const auto target = get(term.gen());
					if (!valid(target))
						return true;

					if (!func(ctx, target))
						return false;

					const auto& relations = as_relations_trav_cache(target);
					for (auto relation: relations) {
						if (!func(ctx, relation))
							return false;
					}
					return true;
				}

				if (allowSemanticIs && !is_wildcard(term) && valid(term) && target(term, OnInstantiate) == Inherit) {
					return for_each_inherited_term_entity(term, [&](Entity entity) {
						return func(ctx, entity);
					});
				}

				if (term.pair() && is_exclusive_dont_fragment_relation(entity_from_id(*this, term.id()))) {
					const auto relation = entity_from_id(*this, term.id());
					const auto* pStore = exclusive_adjunct_store(relation);
					if (pStore == nullptr)
						return true;

					if (is_wildcard(term.gen())) {
						const auto cnt = (uint32_t)pStore->srcToTgt.size();
						GAIA_FOR(cnt) {
							const auto target = pStore->srcToTgt[i];
							if (target == EntityBad)
								continue;
							if (!m_recs.entities.has(i))
								continue;
							if (!func(ctx, EntityContainer::handle(m_recs.entities[i])))
								return false;
						}
						return true;
					}

					const auto* pSources = exclusive_adjunct_sources(*pStore, entity_from_id(*this, term.gen()));
					if (pSources == nullptr)
						return true;

					for (auto source: *pSources) {
						if (!func(ctx, source))
							return false;
					}
					return true;
				}

				if (!term.pair() && is_non_fragmenting_out_of_line_component(term)) {
					const auto it = m_sparseComponentsByComp.find(EntityLookupKey(term));
					if (it == m_sparseComponentsByComp.end())
						return true;
					return it->second.func_for_each_entity(it->second.pStore, ctx, func);
				}

				const auto it = m_entityToArchetypeMap.find(EntityLookupKey(term));
				if (it == m_entityToArchetypeMap.end())
					return true;

				for (const auto& record: it->second) {
					const auto* pArchetype = record.pArchetype;
					if (pArchetype->is_req_del())
						continue;

					for (const auto* pChunk: pArchetype->chunks()) {
						const auto entities = pChunk->entity_view();
						GAIA_EACH(entities) {
							if (!func(ctx, entities[i]))
								return false;
						}
					}
				}

				return true;
			}

		public:
			GAIA_NODISCARD uint32_t count_direct_term_entities(Entity term) const {
				return count_direct_term_entities_inter(term, true);
			}

			GAIA_NODISCARD uint32_t count_direct_term_entities_direct(Entity term) const {
				return count_direct_term_entities_inter(term, false);
			}

			void collect_direct_term_entities(Entity term, cnt::darray<Entity>& out) const {
				collect_direct_term_entities_inter(term, out, true);
			}

			void collect_direct_term_entities_direct(Entity term, cnt::darray<Entity>& out) const {
				collect_direct_term_entities_inter(term, out, false);
			}

			GAIA_NODISCARD bool for_each_direct_term_entity(Entity term, void* ctx, bool (*func)(void*, Entity)) const {
				return for_each_direct_term_entity_inter(term, ctx, func, true);
			}

			GAIA_NODISCARD bool
			for_each_direct_term_entity_direct(Entity term, void* ctx, bool (*func)(void*, Entity)) const {
				return for_each_direct_term_entity_inter(term, ctx, func, false);
			}

			//! Traverses relationship sources in breadth-first order.
			//! Starting at @a rootTarget, this visits all direct sources first and then deeper levels.
			//! \param relation Relation entity
			//! \param rootTarget Root target entity
			//! \param func void(Entity source) functor executed for each traversed source.
			template <typename Func>
			void sources_bfs(Entity relation, Entity rootTarget, Func func) const {
				if (!valid(relation) || !valid(rootTarget))
					return;

				cnt::darray<Entity> queue;
				queue.push_back(rootTarget);

				cnt::set<EntityLookupKey> visited;
				visited.insert(EntityLookupKey(rootTarget));

				for (uint32_t i = 0; i < queue.size(); ++i) {
					const auto currTarget = queue[i];

					cnt::darray<Entity> children;
					sources(relation, currTarget, [&](Entity source) {
						const auto key = EntityLookupKey(source);
						const auto ins = visited.insert(key);
						if (!ins.second)
							return;

						children.push_back(source);
					});

					core::sort(children, [](Entity left, Entity right) {
						return left.id() < right.id();
					});

					for (auto child: children) {
						if (!enabled(child))
							continue;
						func(child);
						queue.push_back(child);
					}
				}
			}

			//! Traverses relationship sources in breadth-first order.
			//! Starting at @a rootTarget, this visits all direct sources first and then deeper levels.
			//! \param relation Relation entity
			//! \param rootTarget Root target entity
			//! \param func bool(Entity source) functor executed for each traversed source.
			//!             Stops if true is returned.
			//! \return True if traversal was stopped by @a func, false otherwise.
			template <typename Func>
			GAIA_NODISCARD bool sources_bfs_if(Entity relation, Entity rootTarget, Func func) const {
				if (!valid(relation) || !valid(rootTarget))
					return false;

				cnt::darray<Entity> queue;
				queue.push_back(rootTarget);

				cnt::set<EntityLookupKey> visited;
				visited.insert(EntityLookupKey(rootTarget));

				for (uint32_t i = 0; i < queue.size(); ++i) {
					const auto currTarget = queue[i];

					cnt::darray<Entity> children;
					sources(relation, currTarget, [&](Entity source) {
						const auto key = EntityLookupKey(source);
						const auto ins = visited.insert(key);
						if (!ins.second)
							return;

						children.push_back(source);
					});

					core::sort(children, [](Entity left, Entity right) {
						return left.id() < right.id();
					});

					for (auto child: children) {
						if (!enabled(child))
							continue;
						if (func(child))
							return true;

						queue.push_back(child);
					}
				}

				return false;
			}

			//! Returns direct children in the ChildOf hierarchy.
			template <typename Func>
			void children(Entity parent, Func func) const {
				sources(ChildOf, parent, func);
			}

			//! Returns direct children in the ChildOf hierarchy.
			//! \param func bool(Entity child) functor executed for each child found.
			//!             Stops if false is returned.
			template <typename Func>
			void children_if(Entity parent, Func func) const {
				sources_if(ChildOf, parent, func);
			}

			//! Traverses descendants in the ChildOf hierarchy in breadth-first order.
			template <typename Func>
			void children_bfs(Entity root, Func func) const {
				sources_bfs(ChildOf, root, func);
			}

			//! Traverses descendants in the ChildOf hierarchy in breadth-first order.
			//! \param func bool(Entity child) functor executed for each child found.
			//!             Stops if true is returned.
			template <typename Func>
			GAIA_NODISCARD bool children_bfs_if(Entity root, Func func) const {
				return sources_bfs_if(ChildOf, root, func);
			}

			template <typename Func>
			void as_targets_trav(Entity relation, Func func) const {
				GAIA_ASSERT(valid(relation));
				if (!valid(relation))
					return;

				const auto& targets = as_targets_trav_cache(relation);
				for (auto target: targets)
					func(target);
			}

			template <typename Func>
			bool as_targets_trav_if(Entity relation, Func func) const {
				GAIA_ASSERT(valid(relation));
				if (!valid(relation))
					return false;

				const auto& targets = as_targets_trav_cache(relation);
				for (auto target: targets)
					if (func(target))
						return true;

				return false;
			}

			//----------------------------------------------------------------------

			CommandBufferST& cmd_buffer_st() const {
				return *m_pCmdBufferST;
			}

			CommandBufferMT& cmd_buffer_mt() const {
				return *m_pCmdBufferMT;
			}

			//----------------------------------------------------------------------

#if GAIA_SYSTEMS_ENABLED

			//! Makes sure the world can work with systems.
			void systems_init();

			//! Executes all registered systems once.
			void systems_run();

			//! Provides a system set up to work with the parent world.
			//! \return Entity holding the system.
			SystemBuilder system();

#endif

#if GAIA_OBSERVERS_ENABLED

			//! Provides a observer set up to work with the parent world.
			//! \return Entity holding the observer.
			ObserverBuilder observer();

			ObserverRegistry& observers() {
				return m_observers;
			}

			const ObserverRegistry& observers() const {
				return m_observers;
			}

#endif

			//----------------------------------------------------------------------

			//! Enables or disables an entire entity.
			//! \param entity Entity
			//! \param enable Enable or disable the entity
			//! \warning It is expected @a entity is valid. Undefined behavior otherwise.
			void enable(Entity entity, bool enable) {
				GAIA_ASSERT(valid(entity));

				auto& ec = m_recs.entities[entity.id()];
				auto& archetype = *ec.pArchetype;
				auto* pChunk = ec.pChunk;
				const bool wasEnabled = !ec.data.dis;
#if GAIA_ASSERT_ENABLED
				verify_enable(*this, archetype, entity);
#endif
				archetype.enable_entity(ec.pChunk, ec.row, enable, m_recs);

				if (wasEnabled != enable) {
					pChunk->update_world_version();
					pChunk->update_entity_order_version();
					update_version(m_enabledHierarchyVersion);
					update_version(m_worldVersion);
				}
			}

			//! Checks if an entity is enabled.
			//! \param ec Entity container of the entity
			//! \return True if the entity is enabled. False otherwise.
			GAIA_NODISCARD bool enabled(const EntityContainer& ec) const {
				const bool entityStateInContainer = !ec.data.dis;
#if GAIA_ASSERT_ENABLED
				const bool entityStateInChunk = ec.pChunk->enabled(ec.row);
				GAIA_ASSERT(entityStateInChunk == entityStateInContainer);
#endif
				return entityStateInContainer;
			}

			//! Checks if an entity is enabled.
			//! \param entity Entity
			//! \return True it the entity is enabled. False otherwise.
			//! \warning It is expected @a entity is valid. Undefined behavior otherwise.
			GAIA_NODISCARD bool enabled(Entity entity) const {
				GAIA_ASSERT(valid(entity));

				const auto& ec = m_recs.entities[entity.id()];
				return enabled(ec);
			}

			//! Checks whether an entity is enabled together with all of its ancestors reachable through @a relation.
			//! This keeps direct enabled state separate from hierarchy-aware gating.
			GAIA_NODISCARD bool enabled_hierarchy(Entity entity, Entity relation) const {
				GAIA_ASSERT(valid(entity));
				GAIA_ASSERT(valid(relation));
				if (!valid(entity) || !valid(relation))
					return false;
				if (!enabled(entity))
					return false;

				auto curr = entity;
				GAIA_FOR(MAX_TRAV_DEPTH) {
					const auto next = target(curr, relation);
					if (next == EntityBad || next == curr)
						break;
					if (!enabled(next))
						return false;
					curr = next;
				}

				return true;
			}

			//----------------------------------------------------------------------

			//! Returns a chunk containing the @a entity.
			//! \param entity Entity
			//! \return Chunk or nullptr if not found.
			GAIA_NODISCARD Chunk* get_chunk(Entity entity) const {
				GAIA_ASSERT(entity.id() < m_recs.entities.size());
				const auto& ec = m_recs.entities[entity.id()];
				return ec.pChunk;
			}

			//! Returns a chunk containing the @a entity.
			//! Index of the entity is stored in @a row
			//! \param entity Entity
			//! \param[out] row Row of @a entity within chunk
			//! \return Chunk or nullptr if not found
			GAIA_NODISCARD Chunk* get_chunk(Entity entity, uint32_t& row) const {
				GAIA_ASSERT(entity.id() < m_recs.entities.size());
				const auto& ec = m_recs.entities[entity.id()];
				row = ec.row;
				return ec.pChunk;
			}

			//! Returns the number of active entities
			//! \return Entity
			GAIA_NODISCARD uint32_t size() const {
				return m_recs.entities.item_count();
			}

			//! Returns high-level runtime counters useful for diagnostics/telemetry.
			void runtime_counters(
					uint32_t& outArchetypes, uint32_t& outChunks, uint32_t& outEntitiesTotal, uint32_t& outEntitiesActive) const {
				outArchetypes = (uint32_t)m_archetypes.size();
				outChunks = 0;
				outEntitiesTotal = 0;
				outEntitiesActive = 0;

				for (const auto* pArchetype: m_archetypes) {
					if (pArchetype == nullptr)
						continue;
					const auto& chunks = pArchetype->chunks();
					outChunks += (uint32_t)chunks.size();
					for (const auto* pChunk: chunks) {
						if (pChunk == nullptr)
							continue;
						outEntitiesTotal += pChunk->size();
						outEntitiesActive += pChunk->size_enabled();
					}
				}
			}

			//! Returns the current version of the world.
			//! \return World version number.
			GAIA_NODISCARD uint32_t& world_version() {
				return m_worldVersion;
			}

			//! Returns structural version for a given relation.
			//! Increments whenever any Pair(relation, *) is added or removed on any entity.
			GAIA_NODISCARD uint32_t rel_version(Entity relation) const {
				const auto it = m_relationVersions.find(EntityLookupKey(relation));
				return it != m_relationVersions.end() ? it->second : 0;
			}

			GAIA_NODISCARD uint32_t enabled_hierarchy_version() const {
				return m_enabledHierarchyVersion;
			}

			friend uint32_t world_rel_version(const World& world, Entity relation);
			friend uint32_t world_version(const World& world);
			friend uint32_t world_entity_archetype_version(const World& world, Entity entity);

			//! Updates a tracked source-entity version after the entity changes archetype membership.
			void update_src_entity_version(Entity entity) {
				const auto key = EntityLookupKey(entity);
				const auto it = m_srcEntityVersions.find(key);
				if (it == m_srcEntityVersions.end())
					return;

				update_version(it->second);
			}

			//! Removes sparse source-version state for an entity that is being destroyed.
			void remove_src_entity_version(Entity entity) {
				m_srcEntityVersions.erase(EntityLookupKey(entity));
			}

			//! Sets maximal lifespan of an archetype @a entity belongs to.
			//! \param entity Entity
			//! \param lifespan How many world updates an empty archetype is kept.
			//!                 If zero, the archetype it kept indefinitely.
			void set_max_lifespan(Entity entity, uint32_t lifespan = Archetype::MAX_ARCHETYPE_LIFESPAN) {
				if (!valid(entity))
					return;

				auto& ec = fetch(entity);
				const auto prevLifespan = ec.pArchetype->max_lifespan();
				ec.pArchetype->set_max_lifespan(lifespan);

				if (prevLifespan == 0) {
					// The archetype used to be immortal but not anymore
					try_enqueue_archetype_for_deletion(*ec.pArchetype);
				}
			}

			//----------------------------------------------------------------------

			//! Performs various internal operations related to the end of the frame such as
			//! memory cleanup and other management operations which keep the system healthy.
			void update() {
				systems_run();

				// Finish deleting entities
				del_finalize();

				// Run garbage collector
				gc();

				util::log_flush();

				// Signal the end of the frame
				GAIA_PROF_FRAME();
			}

			//! Performs world shutdown maintenance without running systems or observers.
			//! Runtime callbacks are shut down in-place first, then deferred entity/chunk/archetype
			//! cleanup is drained until no more pending teardown work remains.
			void teardown() {
				if GAIA_UNLIKELY (m_teardownActive)
					return;
				m_teardownActive = true;

				GAIA_PROF_SCOPE(World::teardown);

#if GAIA_SYSTEMS_ENABLED
				systems_done();
#endif

#if GAIA_OBSERVERS_ENABLED
				m_observers.teardown();
#endif

				for (;;) {
					const auto prevReqArchetypes = m_reqArchetypesToDel.size();
					const auto prevReqEntities = m_reqEntitiesToDel.size();
					const auto prevChunks = m_chunksToDel.size();
					const auto prevArchetypes = m_archetypesToDel.size();

					del_finalize();
					gc();

					if (m_reqArchetypesToDel.empty() && m_reqEntitiesToDel.empty() && m_chunksToDel.empty() &&
							m_archetypesToDel.empty())
						break;

					const bool madeProgress = m_reqArchetypesToDel.size() != prevReqArchetypes ||
																		m_reqEntitiesToDel.size() != prevReqEntities ||
																		m_chunksToDel.size() != prevChunks || m_archetypesToDel.size() != prevArchetypes;
					if (!madeProgress)
						break;
				}

				util::log_flush();
			}

			//! Clears the world so that all its entities and components are released
			void cleanup() {
				cleanup_inter();

				// Reinit
				m_pRootArchetype = nullptr;
				m_pEntityArchetype = nullptr;
				m_pCompArchetype = nullptr;
				m_nextArchetypeId = 0;
				m_defragLastArchetypeIdx = 0;
				m_worldVersion = 0;
				m_enabledHierarchyVersion = 0;
				init();
			}

			//! Sets the maximum number of entities defragmented per world tick
			//! \param value Number of entities to defragment
			void defrag_entities_per_tick(uint32_t value) {
				m_defragEntitiesPerTick = value;
			}

			//--------------------------------------------------------------------------------

			//! Performs diagnostics on archetypes. Prints basic info about them and the chunks they contain.
			void diag_archetypes() const {
				GAIA_LOG_N("Archetypes:%u", (uint32_t)m_archetypes.size());
				for (auto* pArchetype: m_archetypes)
					Archetype::diag(*this, *pArchetype);
			}

			//! Performs diagnostics on registered components.
			//! Prints basic info about them and reports and detected issues.
			void diag_components() const {
				comp_cache().diag();
			}

			//! Performs diagnostics on entities of the world.
			//! Also performs validation of internal structures which hold the entities.
			void diag_entities() const {
				validate_entities();

				GAIA_LOG_N("Deleted entities: %u", (uint32_t)m_recs.entities.get_free_items());
				if (m_recs.entities.get_free_items() != 0U) {
					GAIA_LOG_N("  --> %u", (uint32_t)m_recs.entities.get_next_free_item());

					uint32_t iters = 0;
					auto fe = m_recs.entities.next_free(m_recs.entities.get_next_free_item());
					while (fe != IdentifierIdBad) {
						GAIA_LOG_N("  --> %u", m_recs.entities.next_free(fe));
						fe = m_recs.entities.next_free(fe);
						++iters;
						if (iters > m_recs.entities.get_free_items())
							break;
					}

					if ((iters == 0U) || iters > m_recs.entities.get_free_items())
						GAIA_LOG_E("  Entities recycle list contains inconsistent data!");
				}
			}

			//! Performs all diagnostics.
			void diag() const {
				diag_archetypes();
				diag_components();
				diag_entities();
			}

		private:
			//! Clears the world so that all its entities and components are released
			void cleanup_inter() {
				GAIA_PROF_SCOPE(World::cleanup_inter);

				// Shutdown bypasses the regular GC path, so clear raw-pointer tracking first.
				// Chunk/component dtors that run while archetypes are freed can still drop cached queries,
				// but after this point they must not touch stale archetype/chunk reverse indices.
				{
					m_queryCache.clear_archetype_tracking();
					m_reqArchetypesToDel = {};
					m_reqEntitiesToDel = {};
					m_entitiesToDel = {};
					m_chunksToDel = {};
					m_archetypesToDel = {};
				}

				// Clear entities
				m_recs.entities = {};
				m_recs.pairs = {};

				// Clear archetypes
				{
					// Delete all allocated chunks and their parent archetypes
					for (auto* pArchetype: m_archetypes)
						Archetype::destroy(pArchetype);

					m_entityToAsRelations = {};
					m_entityToAsRelationsTravCache = {};
					m_entityToAsTargets = {};
					m_entityToAsTargetsTravCache = {};
					m_targetsTravCache = {};
					m_srcBfsTravCache = {};
					m_depthOrderCache = {};
					m_sourcesAllCache = {};
					m_targetsAllCache = {};
					m_tgtToRel = {};
					m_relToTgt = {};
					m_exclusiveAdjunctByRel = {};
					m_srcToExclusiveAdjunctRel = {};
					for (auto& [compKey, store]: m_sparseComponentsByComp) {
						(void)compKey;
						store.func_clear_store(store.pStore);
						store.func_del_store(store.pStore);
					}
					m_sparseComponentsByComp = {};
					m_relationVersions = {};
					m_srcEntityVersions = {};

					m_archetypes = {};
					m_archetypesById = {};
					m_archetypesByHash = {};
				}

				// Clear caches
				{
					m_entityToArchetypeMap = {};
					m_queryCache.clear();
					for (auto* pScratch: m_queryMatchScratchStack)
						delete pScratch;
					m_queryMatchScratchStack = {};
					m_queryMatchScratchDepth = 0;
					m_querySerMap = {};
					m_nextQuerySerId = 0;
				}

				// Clear entity aliases
				{
					for (auto& pair: m_aliasToEntity) {
						if (!pair.first.owned())
							continue;
						// Release any memory allocated for owned names
						mem::mem_free((void*)pair.first.str());
					}
					m_aliasToEntity = {};
				}

				// Clear entity names
				{
					for (auto& pair: m_nameToEntity) {
						if (!pair.first.owned())
							continue;
						// Release any memory allocated for owned names
						mem::mem_free((void*)pair.first.str());
					}
					m_nameToEntity = {};
				}

				// Clear component cache
				m_compCache.clear();
			}

			GAIA_NODISCARD static bool valid(const EntityContainer& ec, [[maybe_unused]] Entity entityExpected) {
				if ((ec.flags & EntityContainerFlags::Load) != 0) {
					return entityExpected.id() == ec.idx && entityExpected.gen() == ec.data.gen &&
								 entityExpected.entity() == (bool)ec.data.ent && entityExpected.pair() == (bool)ec.data.pair &&
								 entityExpected.kind() == (EntityKind)ec.data.kind;
				}

				if (is_req_del(ec))
					return false;

				// The entity in the chunk must match the index in the entity container
				const auto* pChunk = ec.pChunk;
				if (pChunk == nullptr || ec.row >= pChunk->size())
					return false;

				const auto entityPresent = pChunk->entity_view()[ec.row];
				// Public validity checks can legitimately observe a recycled slot with a different generation.
				// Treat that as stale instead of aborting.
				return entityExpected == entityPresent;
			}

			//! Checks if the pair \param entity is valid.
			//! \return True if the entity is valid. False otherwise.
			GAIA_NODISCARD bool valid_pair(Entity entity) const {
				if (entity == EntityBad)
					return false;

				GAIA_ASSERT(entity.pair());
				if (!entity.pair())
					return false;

				// Ignore wildcards because they can't be attached to entities
				if (is_wildcard(entity))
					return true;

				const auto it = m_recs.pairs.find(EntityLookupKey(entity));
				if (it == m_recs.pairs.end())
					return false;

				const auto& ec = it->second;
				return valid(ec, entity);
			}

			//! Checks if the entity \param entity is valid.
			//! \return True if the entity is valid. False otherwise.
			GAIA_NODISCARD bool valid_entity(Entity entity) const {
				if (entity == EntityBad)
					return false;

				GAIA_ASSERT(!entity.pair());
				if (entity.pair())
					return false;

				// Entity ID has to fit inside the entity array
				if (entity.id() >= m_recs.entities.size())
					return false;

				const auto* pEc = m_recs.entities.try_get(entity.id());
				if (pEc == nullptr)
					return false;

				return valid(*pEc, entity);
			}

			//! Checks if the entity with id \param entityId is valid.
			//! Pairs are considered invalid.
			//! \return True if entityId is valid. False otherwise.
			GAIA_NODISCARD bool valid_entity_id(EntityId entityId) const {
				if (entityId == EntityBad.id())
					return false;

				// Entity ID has to fit inside the entity array
				if (entityId >= m_recs.entities.size())
					return false;

				const auto* pEc = m_recs.entities.try_get(entityId);
				if (pEc == nullptr)
					return false;

				const auto& ec = *pEc;
				if (ec.data.pair != 0)
					return false;

				return valid(
						ec, Entity(entityId, ec.data.gen, (bool)ec.data.ent, (bool)ec.data.pair, (EntityKind)ec.data.kind));
			}

			//! Locks the chunk for structural changes.
			//! While locked, no new entities or components can be added or removed.
			//! While locked, no entities can be enabled or disabled.
			void lock() {
				GAIA_ASSERT(m_structuralChangesLocked != (uint32_t)-1);
				++m_structuralChangesLocked;
			}

			//! Unlocks the chunk for structural changes.
			//! While locked, no new entities or components can be added or removed.
			//! While locked, no entities can be enabled or disabled.
			void unlock() {
				GAIA_ASSERT(m_structuralChangesLocked > 0);
				--m_structuralChangesLocked;
			}

#if GAIA_SYSTEMS_ENABLED
			void systems_done();
#endif

		public:
			//! Checks if the chunk is locked for structural changes.
			GAIA_NODISCARD bool locked() const {
				return m_structuralChangesLocked != 0;
			}

			//! Returns true while the world is draining teardown work and normal runtime callbacks
			//! must not execute.
			GAIA_NODISCARD bool tearing_down() const {
				return m_teardownActive;
			}

		private:
			static constexpr uint32_t WorldSerializerVersion = 2;
			static constexpr uint32_t WorldSerializerJSONVersion = 1;

			void save_to(ser::serializer s) const {
				GAIA_ASSERT(s.valid());

				// Version number, currently unused
				s.save((uint32_t)WorldSerializerVersion);

				// Store the index of the last core component.
				// TODO: As this changes, we will have to modify entity ids accordingly.
				const auto lastCoreComponentId = GAIA_ID(LastCoreComponent).id();
				s.save(lastCoreComponentId);

				// Entities
				{
					auto saveEntityContainer = [&](const EntityContainer& ec) {
						s.save(ec.idx);
						s.save(ec.dataRaw);
						s.save(ec.row);
						GAIA_ASSERT((ec.flags & EntityContainerFlags::Load) == 0);
						s.save(ec.flags); // ignore Load

#if GAIA_USE_SAFE_ENTITY
						s.save(ec.refCnt);
#else
						s.save((uint32_t)0);
#endif

						uint32_t archetypeIdx = ec.pArchetype->list_idx();
						s.save(archetypeIdx);
						uint32_t chunkIdx = ec.pChunk->idx();
						s.save(chunkIdx);
					};

					const auto recEntities = (uint32_t)m_recs.entities.size();
					const auto newEntities = recEntities - lastCoreComponentId;
					s.save(newEntities);
					GAIA_FOR2(lastCoreComponentId, recEntities) {
						const bool isAlive = m_recs.entities.has(i);
						s.save(isAlive);
						if (isAlive)
							saveEntityContainer(m_recs.entities[i]);
						else {
							s.save(m_recs.entities.handle(i).val);
							s.save(m_recs.entities.next_free(i));
						}
					}

					{
						uint32_t pairsCnt = 0;
						for (const auto& pair: m_recs.pairs) {
							// Skip core pairs
							if (pair.first.entity().id() < lastCoreComponentId && pair.first.entity().gen() < lastCoreComponentId)
								continue;

							++pairsCnt;
						}
						s.save(pairsCnt);
					}
					{
						for (const auto& pair: m_recs.pairs) {
							// Skip core pairs
							if (pair.first.entity().id() < lastCoreComponentId && pair.first.entity().gen() < lastCoreComponentId)
								continue;

							saveEntityContainer(pair.second);
						}
					}

					s.save(m_recs.entities.m_nextFreeIdx);
					s.save(m_recs.entities.m_freeItems);
				}

				// World
				{
					s.save((uint32_t)m_archetypes.size());
					for (auto* pArchetype: m_archetypes) {
						s.save((uint32_t)pArchetype->ids_view().size());
						for (auto e: pArchetype->ids_view())
							s.save(e);

						pArchetype->save(s);
					}

					s.save(m_worldVersion);
				}

				// Entity names
				{
					s.save((uint32_t)m_nameToEntity.size());
					for (const auto& pair: m_nameToEntity) {
						s.save(pair.second);
						const bool isOwnedStr = pair.first.owned();
						s.save(isOwnedStr);

						// For owner string we copy the entire string into the buffer
						if (isOwnedStr) {
							const auto* str = pair.first.str();
							const uint32_t len = pair.first.len();
							s.save(len);
							s.save_raw(str, len, ser::serialization_type_id::c8);
						}
						// Non-owned strings will only store the pointer.
						// However, if it is a component, we do not store anything at all because we can reconstruct
						// the name from our component cache.
						else if (!pair.second.comp()) {
							const auto* str = pair.first.str();
							const uint32_t len = pair.first.len();
							s.save(len);
							const auto ptr_val = (uint64_t)str;
							s.save_raw(&ptr_val, sizeof(ptr_val), ser::serialization_type_id::u64);
						}
					}
				}

				// Entity aliases
				{
					uint32_t aliasCnt = 0;
					GAIA_FOR((uint32_t)m_recs.entities.size()) {
						const auto entity = get((EntityId)i);
						if (!valid(entity) || entity.pair())
							continue;

						const auto& ec = m_recs.entities[i];
						const auto compIdx = core::get_index(ec.pChunk->ids_view(), GAIA_ID(EntityDesc));
						if (compIdx == BadIndex)
							continue;

						const auto* pDesc = reinterpret_cast<const EntityDesc*>(ec.pChunk->comp_ptr(compIdx, ec.row));
						if (pDesc->alias != nullptr)
							++aliasCnt;
					}

					s.save(aliasCnt);
					GAIA_FOR((uint32_t)m_recs.entities.size()) {
						const auto entity = get((EntityId)i);
						if (!valid(entity) || entity.pair())
							continue;

						const auto& ec = m_recs.entities[i];
						const auto compIdx = core::get_index(ec.pChunk->ids_view(), GAIA_ID(EntityDesc));
						if (compIdx == BadIndex)
							continue;

						const auto* pDesc = reinterpret_cast<const EntityDesc*>(ec.pChunk->comp_ptr(compIdx, ec.row));
						if (pDesc->alias == nullptr)
							continue;

						s.save(entity);
						s.save(pDesc->alias_len);
						s.save_raw(pDesc->alias, pDesc->alias_len, ser::serialization_type_id::c8);
					}
				}
			}

		public:
			//! Saves contents of the world to a buffer. The buffer is reset, not appended.
			//! NOTE: In order for custom version of save to be used for a given component, it needs to have either
			//!       of the following functions defined:
			//!       1) member function: "void save(bin_stream& s)"
			//!       2) free function in gaia::ser namespace: "void tag_invoke(gaia::ser::save_v, bin_stream& s,
			//!       const YourType& data)"
			void save() {
				auto s = m_serializer;
				GAIA_ASSERT(s.valid());

				s.reset();
				save_to(s);
			}

			//! Serializes world state into a JSON document.
			//! Components with runtime schema are emitted as structured JSON objects.
			//! Components with no schema fallback to raw serialized bytes.
			//! Returns false when some schema field types are unsupported (those fields are emitted as null).
			bool save_json(ser::ser_json& writer, ser::JsonSaveFlags flags = ser::JsonSaveFlags::Default) const;

			//! Convenience overload returning JSON as a string.
			ser::json_str save_json(bool& ok, ser::JsonSaveFlags flags = ser::JsonSaveFlags::Default) const;

			//! Loads world state from JSON previously emitted by save_json().
			//! Returns true when JSON shape is valid and parsing succeeds.
			//! Non-fatal semantic issues are reported through @a diagnostics.
			bool load_json(const char* json, uint32_t len, ser::JsonDiagnostics& diagnostics);

			bool load_json(const char* json, uint32_t len);

			bool load_json(ser::json_str_view json, ser::JsonDiagnostics& diagnostics);

			bool load_json(ser::json_str_view json);

			//! Loads a world state from a buffer. The buffer is sought to 0 before any loading happens.
			//! NOTE: In order for custom version of load to be used for a given component, it needs to have either
			//!       of the following functions defined:
			//!       1) member function: "void load(bin_stream& s)"
			//!       2) free function in gaia::ser namespace: "void tag_invoke(gaia::ser::load_v, bin_stream& s,
			//!       YourType& data)"
			bool load(ser::serializer inputSerializer = {}) {
				auto s = inputSerializer.valid() ? inputSerializer : m_serializer;
				GAIA_ASSERT(s.valid());

				// Move back to the beginning of the stream
				s.seek(0);

				// Version number, currently unused
				uint32_t version = 0;
				s.load(version);
				if (version != WorldSerializerVersion) {
					GAIA_LOG_E("Unsupported world version %u. Expected %u.", version, WorldSerializerVersion);
					return false;
				}

				// Store the index of the last core component. As they change, we will have to modify entity ids accordingly.
				uint32_t lastCoreComponentId = 0;
				s.load(lastCoreComponentId);

				// Append-only core ids are handled via load-time entity remapping.
				// Snapshots from a runtime with a larger core-id boundary are not supported.
				const auto currLastCoreComponentId = GAIA_ID(LastCoreComponent).id();
				if (lastCoreComponentId > currLastCoreComponentId) {
					GAIA_LOG_E(
							"Unsupported world core boundary %u. Current runtime supports up to %u.", lastCoreComponentId,
							currLastCoreComponentId);
					return false;
				}
				// Install the append-only core-id remap for nested Entity::load() calls.
				// This keeps the serializer API unchanged, at the cost of relying on
				// scoped thread-local state instead of explicit serializer-local context.
				const detail::EntityLoadRemapGuard entityLoadRemapGuard(lastCoreComponentId, currLastCoreComponentId);
				auto remapLoadedEntityId = [&](uint32_t id) {
					return detail::remap_loaded_entity_id(id, lastCoreComponentId, currLastCoreComponentId);
				};

				// Entities
				{
					auto loadEntityContainer = [&](EntityContainer& ec) {
						s.load(ec.idx);
						s.load(ec.dataRaw);
						s.load(ec.row);
						s.load(ec.flags);
						ec.flags |= EntityContainerFlags::Load;

						ec.idx = remapLoadedEntityId(ec.idx);
						if (ec.data.pair != 0)
							ec.data.gen = remapLoadedEntityId(ec.data.gen);

#if GAIA_USE_SAFE_ENTITY
						s.load(ec.refCnt);
#else
						s.load(ec.unused);
						// if this value is different from zero, it means we are trying to load data
						// that was previously saved with GAIA_USE_SAFE_ENTITY. It's probably not a good idea
						// because if your program used reference counting it probably won't work correctly.
						GAIA_ASSERT(ec.unused == 0);
#endif
						// Store the archetype idx inside the pointer. We will decode this once archetypes are created.
						uint32_t archetypeIdx = 0;
						s.load(archetypeIdx);
						ec.pArchetype = (Archetype*)((uintptr_t)archetypeIdx);
						// Store the chunk idx inside the pointer. We will decode this once chunks are created.
						uint32_t chunkIdx = 0;
						s.load(chunkIdx);
						ec.pChunk = (Chunk*)((uintptr_t)chunkIdx);
					};

					uint32_t newEntities = 0;
					s.load(newEntities);
					GAIA_FOR(newEntities) {
						bool isAlive = false;
						s.load(isAlive);
						if (isAlive) {
							EntityContainer ec{};
							loadEntityContainer(ec);
							m_recs.entities.add_live(GAIA_MOV(ec));
						} else {
							Identifier id = IdentifierBad;
							uint32_t nextFreeIdx = Entity::IdMask;
							s.load(id);
							s.load(nextFreeIdx);
							auto entity = Entity(id);
							entity = detail::remap_loaded_entity(entity, lastCoreComponentId, currLastCoreComponentId);
							nextFreeIdx = remapLoadedEntityId(nextFreeIdx);
							GAIA_ASSERT(entity.id() == remapLoadedEntityId(lastCoreComponentId + i));
							m_recs.entities.add_free(entity, nextFreeIdx);
						}
					}

					uint32_t pairsCnt = 0;
					s.load(pairsCnt);
					GAIA_FOR(pairsCnt) {
						EntityContainer ec{};
						loadEntityContainer(ec);
						Entity pair(ec.idx, ec.data.gen);
						m_recs.pairs.emplace(EntityLookupKey(pair), GAIA_MOV(ec));
					}

					s.load(m_recs.entities.m_nextFreeIdx);
					m_recs.entities.m_nextFreeIdx = remapLoadedEntityId(m_recs.entities.m_nextFreeIdx);
					s.load(m_recs.entities.m_freeItems);
				}

				// World
				{
					uint32_t archetypesSize = 0;
					s.load(archetypesSize);
					m_archetypes.reserve(archetypesSize);
					GAIA_FOR(archetypesSize) {
						uint32_t idsSize = 0;
						s.load(idsSize);
						Entity ids[ChunkHeader::MAX_COMPONENTS];
						GAIA_FOR_(idsSize, j) {
							s.load(ids[j]);
						}

						// Calculate the lookup hash
						const auto hashLookup = calc_lookup_hash({&ids[0], idsSize}).hash;

						auto* pArchetype = find_archetype({hashLookup}, {&ids[0], idsSize});
						if (pArchetype == nullptr) {
							// Create the archetype
							pArchetype = create_archetype({&ids[0], idsSize});
							pArchetype->set_hashes({hashLookup});

							// No need to do anything with the archetype graph. It will build itself naturally.
							// pArchetype->build_graph_edges(pArchetypeRight, entity);

							// Register the archetype in the world
							reg_archetype(pArchetype);
						}

						// Load archetype data
						pArchetype->load(s);
					}

					s.load(m_worldVersion);
				}

				// Update entity records.
				// We previously encoded the archetype id into refCnt.
				// Now we need to convert it back to the pointer.
				{
					for (auto& ec: m_recs.entities) {
						if ((ec.flags & EntityContainerFlags::Load) == 0)
							continue;
						ec.flags &= ~EntityContainerFlags::Load; // Clear the load flag

						const auto archetypeIdx = (ArchetypeId)((uintptr_t)ec.pArchetype); // Decode the archetype idx
						ec.pArchetype = m_archetypes[archetypeIdx];
						const uint32_t chunkIdx = (uint32_t)((uintptr_t)ec.pChunk); // Decode the chunk idx
						ec.pChunk = ec.pArchetype->chunks()[chunkIdx];
						ec.pEntity = &ec.pChunk->entity_view()[ec.row];
					}

					for (auto& pair: m_recs.pairs) {
						auto& ec = pair.second;

						// Core pairs remain in-world during load and were not serialized into the stream.
						if ((ec.flags & EntityContainerFlags::Load) == 0)
							continue;

						GAIA_ASSERT((ec.flags & EntityContainerFlags::Load) != 0);
						ec.flags &= ~EntityContainerFlags::Load; // Clear the load flag

						const auto archetypeIdx = (ArchetypeId)((uintptr_t)ec.pArchetype); // Decode the archetype idx
						ec.pArchetype = m_archetypes[archetypeIdx];
						const uint32_t chunkIdx = (uint32_t)((uintptr_t)ec.pChunk); // Decode the chunk idx
						ec.pChunk = ec.pArchetype->chunks()[chunkIdx];
						ec.pEntity = &ec.pChunk->entity_view()[ec.row];
					}
				}

#if GAIA_ASSERT_ENABLED
				for (const auto& ec: m_recs.entities) {
					GAIA_ASSERT(ec.idx < m_recs.entities.size());
					GAIA_ASSERT(m_recs.entities.handle(ec.idx) == EntityContainer::handle(ec));
					GAIA_ASSERT(ec.pArchetype != nullptr);
					GAIA_ASSERT(ec.pChunk != nullptr);
					GAIA_ASSERT(ec.pEntity != nullptr);
				}
#endif
				// Entity names
				{
					m_nameToEntity = {};
					uint32_t cnt = 0;
					s.load(cnt);
					GAIA_FOR(cnt) {
						Entity entity;
						s.load(entity);
						// entity.data.gen = 0; // Reset generation to zero

						const auto& ec = fetch(entity);
						const auto compIdx = core::get_index(ec.pChunk->ids_view(), GAIA_ID(EntityDesc));
						auto* pDesc = reinterpret_cast<EntityDesc*>(ec.pChunk->comp_ptr_mut(compIdx, ec.row));
						GAIA_ASSERT(core::check_alignment(pDesc));

						bool isOwned = false;
						s.load(isOwned);
						if (!isOwned) {
							if (entity.comp()) {
								// Make components point back to their component cache record because if we save the world and load
								// it back in runtime, EntityDesc would still point to the old pointers to component names.
								const auto& ci = comp_cache().get(entity);
								pDesc->name = ci.name.str();
								// Length should still be the same. Only the pointer has changed.
								GAIA_ASSERT(pDesc->name_len == ci.name.len());
								m_nameToEntity.try_emplace(EntityNameLookupKey(pDesc->name, pDesc->name_len, 0), entity);
							} else {
								uint32_t len = 0;
								s.load(len);
								uint64_t ptr_val = 0;
								s.load_raw(&ptr_val, sizeof(ptr_val), ser::serialization_type_id::u64);

								// Simply point to whereever the original pointer pointed to
								pDesc->name = (const char*)ptr_val;
								pDesc->name_len = len;
								m_nameToEntity.try_emplace(EntityNameLookupKey(pDesc->name, pDesc->name_len, 0), entity);
							}

							continue;
						}

						uint32_t len = 0;
						s.load(len);

						// Get a pointer to where the string begins and seek to the end of the string
						const char* entityStr = (const char*)(s.data() + s.tell());
						s.seek(s.tell() + len);

						// Make sure EntityDesc does not point anywhere right now.
						{
							pDesc->name = nullptr;
							pDesc->name_len = 0;
						}

						// Name the entity using an owned string
						name(entity, entityStr, len);
					}
				}

				// Entity aliases
				{
					m_aliasToEntity = {};
					for (auto& ec: m_recs.entities) {
						const auto entity = EntityContainer::handle(ec);
						if (entity.pair())
							continue;

						const auto compIdx = core::get_index(ec.pChunk->ids_view(), GAIA_ID(EntityDesc));
						if (compIdx == BadIndex)
							continue;

						auto* pDesc = reinterpret_cast<EntityDesc*>(ec.pChunk->comp_ptr_mut(compIdx, ec.row));
						GAIA_ASSERT(core::check_alignment(pDesc));
						pDesc->alias = nullptr;
						pDesc->alias_len = 0;
					}

					uint32_t cnt = 0;
					s.load(cnt);
					GAIA_FOR(cnt) {
						Entity entity;
						s.load(entity);

						const auto& ec = fetch(entity);
						const auto compIdx = core::get_index(ec.pChunk->ids_view(), GAIA_ID(EntityDesc));
						auto* pDesc = reinterpret_cast<EntityDesc*>(ec.pChunk->comp_ptr_mut(compIdx, ec.row));
						GAIA_ASSERT(core::check_alignment(pDesc));

						uint32_t len = 0;
						s.load(len);

						// Get a pointer to where the string begins and seek to the end of the string
						const char* aliasStr = (const char*)(s.data() + s.tell());
						s.seek(s.tell() + len);

						pDesc->alias = nullptr;
						pDesc->alias_len = 0;
						alias(entity, aliasStr, len);
					}
				}

				return true;
			}

			template <typename TSerializer>
			bool load(TSerializer& inputSerializer) {
				return load(ser::make_serializer(inputSerializer));
			}

		private:
			//! Sorts archetypes in the archetype list with their ids in ascending order
			void sort_archetypes() {
				struct sort_cond {
					bool operator()(const Archetype* a, const Archetype* b) const {
						return a->id() < b->id();
					}
				};

				core::sort(m_archetypes, sort_cond{}, [&](uint32_t left, uint32_t right) {
					Archetype* tmp = m_archetypes[left];

					m_archetypes[right]->list_idx(left);
					m_archetypes[left]->list_idx(right);

					m_archetypes.data()[left] = (Archetype*)m_archetypes[right];
					m_archetypes.data()[right] = tmp;
				});
			}

			//! Remove a chunk from its archetype.
			//! \param archetype Archetype we remove the chunk from
			//! \param chunk Chunk we are removing
			void remove_chunk(Archetype& archetype, Chunk& chunk) {
				archetype.del(&chunk);
				try_enqueue_archetype_for_deletion(archetype);
			}

			//! Removes a chunk from the deferred delete queue and keeps the moved entry's queue index in sync.
			void remove_chunk_from_delete_queue(uint32_t idx) {
				GAIA_ASSERT(idx < m_chunksToDel.size());

				auto* pChunk = m_chunksToDel[idx].pChunk;
				pChunk->clear_delete_queue_index();

				const auto lastIdx = (uint32_t)m_chunksToDel.size() - 1;
				if (idx != lastIdx) {
					auto* pMovedChunk = m_chunksToDel[lastIdx].pChunk;
					pMovedChunk->delete_queue_index(idx);
				}

				core::swap_erase(m_chunksToDel, idx);
			}

			//! Remove an entity from its chunk.
			//! \param archetype Archetype we remove the entity from
			//! \param chunk Chunk we remove the entity from
			//! \param row Index of entity within its chunk
			void remove_entity(Archetype& archetype, Chunk& chunk, uint16_t row) {
				archetype.remove_entity(chunk, row, m_recs);
				try_enqueue_chunk_for_deletion(archetype, chunk);
			}

			//! Delete all chunks which are empty (have no entities) and have not been used in a while
			void del_empty_chunks() {
				GAIA_PROF_SCOPE(World::del_empty_chunks);

				for (uint32_t i = 0; i < m_chunksToDel.size();) {
					auto* pArchetype = m_chunksToDel[i].pArchetype;
					auto* pChunk = m_chunksToDel[i].pChunk;

					// Revive reclaimed chunks
					if (!pChunk->empty()) {
						pChunk->revive();
						revive_archetype(*pArchetype);
						remove_chunk_from_delete_queue(i);
						continue;
					}

					// Skip chunks which still have some lifespan left
					if (pChunk->progress_death()) {
						++i;
						continue;
					}

					// Delete unused chunks that are past their lifespan
					remove_chunk(*pArchetype, *pChunk);
					remove_chunk_from_delete_queue(i);
				}
			}

			//! Delete an archetype from the world
			void del_empty_archetype(Archetype* pArchetype) {
				GAIA_PROF_SCOPE(World::del_empty_archetype);

				GAIA_ASSERT(pArchetype != nullptr);
				GAIA_ASSERT(pArchetype->empty() || pArchetype->is_req_del());
				GAIA_ASSERT(!pArchetype->dying() || pArchetype->is_req_del());

				unreg_archetype(pArchetype);
				for (auto& ec: m_recs.entities) {
					if (ec.pArchetype != pArchetype)
						continue;

					ec.pArchetype = nullptr;
					ec.pChunk = nullptr;
					ec.pEntity = nullptr;
				}
				for (auto& [_, ec]: m_recs.pairs) {
					if (ec.pArchetype != pArchetype)
						continue;

					ec.pArchetype = nullptr;
					ec.pChunk = nullptr;
					ec.pEntity = nullptr;
				}
				Archetype::destroy(pArchetype);
			}

			//! Delete all archetypes which are empty (have no used chunks) and have not been used in a while
			void del_empty_archetypes() {
				GAIA_PROF_SCOPE(World::del_empty_archetypes);

				cnt::sarray_ext<Archetype*, 512> tmp;

				// Remove all dead archetypes from query caches.
				// Because the number of cached queries is way higher than the number of archetypes
				// we want to remove, we flip the logic around and iterate over all query caches
				// and match against our lists.
				// Note, all archetype pointers in the tmp array are invalid at this point and can
				// be used only for comparison. They can't be dereferenced.
				auto remove_from_queries = [&]() {
					if (tmp.empty())
						return;

					for (auto* pArchetype: tmp) {
						m_queryCache.remove_archetype_from_queries(pArchetype);
						del_empty_archetype(pArchetype);
					}
					tmp.clear();
				};

				for (uint32_t i = 0; i < m_archetypesToDel.size();) {
					auto* pArchetype = m_archetypesToDel[i];

					// Skip reclaimed archetypes or archetypes that became immortal
					if (!pArchetype->empty() || pArchetype->max_lifespan() == 0) {
						revive_archetype(*pArchetype);
						core::swap_erase(m_archetypesToDel, i);
						continue;
					}

					// Skip archetypes which still have some lifespan left unless
					// they are force-deleted.
					if (!pArchetype->is_req_del() && pArchetype->progress_death()) {
						++i;
						continue;
					}

					tmp.push_back(pArchetype);

					// Remove the unused archetypes
					core::swap_erase(m_archetypesToDel, i);

					// Clear what we have once the capacity is reached
					if (tmp.size() == tmp.max_size())
						remove_from_queries();
				}

				remove_from_queries();
			}

			void revive_archetype(Archetype& archetype) {
				archetype.revive();
				m_reqArchetypesToDel.erase(ArchetypeLookupKey(archetype.lookup_hash(), &archetype));
			}

			void try_enqueue_chunk_for_deletion(Archetype& archetype, Chunk& chunk) {
				if (chunk.dying() || !chunk.empty())
					return;

				// When the chunk is emptied we want it to be removed. We can't do it
				// rowB away and need to wait for world::gc() to be called.
				//
				// However, we need to prevent the following:
				//    1) chunk is emptied, add it to some removal list
				//    2) chunk is reclaimed
				//    3) chunk is emptied, add it to some removal list again
				//
				// Therefore, we have a flag telling us the chunk is already waiting to
				// be removed. The chunk might be reclaimed before garbage collection happens
				// but it simply ignores such requests. This way we always have at most one
				// record for removal for any given chunk.
				chunk.start_dying();

				m_chunksToDel.push_back({&archetype, &chunk});
				chunk.delete_queue_index((uint32_t)m_chunksToDel.size() - 1);
			}

			void try_enqueue_archetype_for_deletion(Archetype& archetype) {
				if (!archetype.ready_to_die())
					return;

				// When the chunk is emptied we want it to be removed. We can't do it
				// rowB away and need to wait for world::gc() to be called.
				//
				// However, we need to prevent the following:
				//    1) archetype is emptied, add it to some removal list
				//    2) archetype is reclaimed
				//    3) archetype is emptied, add it to some removal list again
				//
				// Therefore, we have a flag telling us the chunk is already waiting to
				// be removed. The archetype might be reclaimed before garbage collection happens
				// but it simply ignores such requests. This way we always have at most one
				// record for removal for any given chunk.
				archetype.start_dying();

				m_archetypesToDel.push_back(&archetype);
			}

			//! Defragments chunks.
			//! \param maxEntities Maximum number of entities moved per call
			void defrag_chunks(uint32_t maxEntities) {
				GAIA_PROF_SCOPE(World::defrag_chunks);

				const auto maxIters = m_archetypes.size();
				// There has to be at least the root archetype present
				GAIA_ASSERT(maxIters > 0);

				GAIA_FOR(maxIters) {
					const auto idx = (m_defragLastArchetypeIdx + 1) % maxIters;
					auto* pArchetype = m_archetypes[idx];
					defrag_archetype(*pArchetype, maxEntities);
					if (maxEntities == 0)
						return;

					m_defragLastArchetypeIdx = idx;
				}
			}

			//! Defragments the chunk.
			//! \param maxEntities Maximum number of entities moved per call
			//! \param chunksToDelete Container of chunks ready for removal
			//! \param entities Container with entities
			void defrag_archetype(Archetype& archetype, uint32_t& maxEntities) {
				// Assuming the following chunk layout:
				//   Chunk_1: 10/10
				//   Chunk_2:  1/10
				//   Chunk_3:  7/10
				//   Chunk_4: 10/10
				//   Chunk_5:  9/10
				// After full defragmentation we end up with:
				//   Chunk_1: 10/10
				//   Chunk_2: 10/10 (7 entities from Chunk_3 + 2 entities from Chunk_5)
				//   Chunk_3:  0/10 (empty, ready for removal)
				//   Chunk_4: 10/10
				//   Chunk_5:  7/10
				// TODO: Implement mask of semi-full chunks so we can pick one easily when searching
				//       for a chunk to fill with a new entity and when defragmenting.
				// NOTE 1:
				// Even though entity movement might be present during defragmentation, we do
				// not update the world version here because no real structural changes happen.
				// All entities and components remain intact, they just move to a different place.
				// NOTE 2:
				// Entities belonging to chunks with uni components are locked to their chunk.
				// Therefore, we won't defragment them unless their uni components contain matching
				// values.

				if (maxEntities == 0)
					return;

				const auto& chunks = archetype.chunks();
				if (chunks.size() < 2)
					return;

				uint32_t front = 0;
				uint32_t back = chunks.size() - 1;

				auto* pDstChunk = chunks[front];
				auto* pSrcChunk = chunks[back];

				// Find the first semi-full chunk in the front
				while (front < back && (pDstChunk->full() || !pDstChunk->is_semi()))
					pDstChunk = chunks[++front];
				// Find the last semi-full chunk in the back
				while (front < back && (pSrcChunk->empty() || !pSrcChunk->is_semi()))
					pSrcChunk = chunks[--back];

				const auto& props = archetype.props();
				const bool hasUniEnts =
						props.cntEntities > 0 && archetype.ids_view()[props.cntEntities - 1].kind() == EntityKind::EK_Uni;

				// Find the first semi-empty chunk in the back
				while (front < back) {
					pDstChunk = chunks[front];
					pSrcChunk = chunks[back];

					const uint32_t entitiesInSrcChunk = pSrcChunk->size();
					const uint32_t spaceInDstChunk = pDstChunk->capacity() - pDstChunk->size();
					const uint32_t entitiesToMoveSrc = core::get_min(entitiesInSrcChunk, maxEntities);
					const uint32_t entitiesToMove = core::get_min(entitiesToMoveSrc, spaceInDstChunk);

					// Make sure uni components have matching values
					if (hasUniEnts) {
						auto rec = pSrcChunk->comp_rec_view();
						bool res = true;
						GAIA_FOR2(props.genEntities, props.cntEntities) {
							const auto* pSrcVal = (const void*)pSrcChunk->comp_ptr(i, 0);
							const auto* pDstVal = (const void*)pDstChunk->comp_ptr(i, 0);
							if (rec[i].pItem->cmp(pSrcVal, pDstVal)) {
								res = false;
								break;
							}
						}

						// When there is not a match we move to the next chunk
						if (!res) {
							pDstChunk = chunks[++front];
							goto next_iteration;
						}
					}

					GAIA_FOR(entitiesToMove) {
						const auto lastSrcEntityIdx = entitiesInSrcChunk - i - 1;
						const auto entity = pSrcChunk->entity_view()[lastSrcEntityIdx];

						auto& ec = m_recs[entity];

						const auto srcRow = ec.row;
						const auto dstRow = pDstChunk->add_entity(entity);
						const bool wasEnabled = !ec.data.dis;

						// Make sure the old entity becomes enabled now
						archetype.enable_entity(pSrcChunk, srcRow, true, m_recs);
						// We go back-to-front in the chunk so enabling the entity is not expected to change its row
						GAIA_ASSERT(srcRow == ec.row);

						// Move data from the old chunk to the new one
						pDstChunk->move_entity_data(entity, dstRow, m_recs);

						// Remove the entity record from the old chunk.
						// Normally we'd call remove_entity but we don't want to trigger world
						// version updated all the time. It's enough to do it just once at the
						// end of defragmentation.
						// remove_entity(archetype, *pSrcChunk, srcRow);
						archetype.remove_entity_raw(*pSrcChunk, srcRow, m_recs);
						try_enqueue_chunk_for_deletion(archetype, *pSrcChunk);

						// Bring the entity container record up-to-date
						ec.pChunk = pDstChunk;
						ec.row = (uint16_t)dstRow;
						ec.pEntity = &pDstChunk->entity_view()[dstRow];

						// Transfer the original enabled state to the new chunk
						archetype.enable_entity(pDstChunk, dstRow, wasEnabled, m_recs);
					}

					// Update world versions
					if (entitiesToMove > 0) {
						pSrcChunk->update_world_version();
						pDstChunk->update_world_version();
						pSrcChunk->update_entity_order_version();
						pDstChunk->update_entity_order_version();
						update_version(m_worldVersion);
					}

					maxEntities -= entitiesToMove;
					if (maxEntities == 0)
						return;

					// The source is empty, find another semi-empty source
					if (pSrcChunk->empty()) {
						while (front < back) {
							if (chunks[--back]->is_semi())
								break;
						}
					}

				next_iteration:
					// The destination chunk is full, we need to move to the next one.
					// The idea is to fill the destination as much as possible.
					while (front < back && pDstChunk->full())
						pDstChunk = chunks[++front];
				}
			}

			//! Searches for archetype with a given set of components
			//! \param hashLookup Archetype lookup hash
			//! \param ids Archetype entities/components
			//! \return Pointer to archetype or nullptr.
			GAIA_NODISCARD Archetype* find_archetype(Archetype::LookupHash hashLookup, EntitySpan ids) {
				auto tmpArchetype = ArchetypeLookupChecker(ids);
				ArchetypeLookupKey key(hashLookup, &tmpArchetype);

				// Search for the archetype in the map
				const auto it = m_archetypesByHash.find(key);
				if (it == m_archetypesByHash.end())
					return nullptr;

				auto* pArchetype = it->second;
				return pArchetype;
			}

			GAIA_NODISCARD static auto
			find_component_index_record(ComponentIndexEntryArray& records, const Archetype* pArchetype) {
				return core::get_index_if(records, [&](const auto& record) {
					return record.matches(pArchetype);
				});
			}

			GAIA_NODISCARD static auto
			find_component_index_record(const ComponentIndexEntryArray& records, const Archetype* pArchetype) {
				return core::get_index_if(records, [&](const auto& record) {
					return record.matches(pArchetype);
				});
			}

			//! Adds the archetype to <entity, archetype> map for quick lookups of archetypes by comp/tag/pair.
			//! Exact ids store the owning column index, wildcard pair ids store an aggregated match count.
			void add_entity_archetype_pair(
					Entity entity, Archetype* pArchetype, uint16_t compIdx = ComponentIndexBad, uint16_t matchCount = 1) {
				GAIA_ASSERT(pArchetype != nullptr);
				GAIA_ASSERT(matchCount > 0);

				EntityLookupKey entityKey(entity);
				const auto it = m_entityToArchetypeMap.find(entityKey);
				if (it == m_entityToArchetypeMap.end()) {
					ComponentIndexEntryArray records;
					records.push_back(ComponentIndexEntry{pArchetype, compIdx, matchCount});
					m_entityToArchetypeMap.try_emplace(entityKey, GAIA_MOV(records));
					return;
				}

				auto& records = it->second;
				const auto idx = find_component_index_record(records, pArchetype);
				if (idx == BadIndex) {
					records.push_back(ComponentIndexEntry{pArchetype, compIdx, matchCount});
					return;
				}

				auto& record = records[idx];
				record.matchCount = (uint16_t)(record.matchCount + matchCount);
				if (compIdx != ComponentIndexBad)
					record.compIdx = compIdx;
			}

			void add_pair_archetype_query_pairs(Entity pair, Archetype* pArchetype, uint16_t matchCount = 1) {
				GAIA_ASSERT(pair.pair());
				GAIA_ASSERT(pArchetype != nullptr);
				GAIA_ASSERT(matchCount > 0);

				const auto first = get(pair.id());
				const auto second = get(pair.gen());

				add_entity_archetype_pair(Pair(All, second), pArchetype, ComponentIndexBad, matchCount);
				add_entity_archetype_pair(Pair(first, All), pArchetype, ComponentIndexBad, matchCount);
				add_entity_archetype_pair(Pair(All, All), pArchetype, ComponentIndexBad, matchCount);
			}

			//! Deletes an archetype from the <pairEntity, archetype> map
			//! \param pair Pair entity used as a key in the map
			//! \param entityToRemove Entity used to identify archetypes we are removing from the archetype array
			void del_entity_query_pair(Pair pair, Entity entityToRemove) {
				auto it = m_entityToArchetypeMap.find(EntityLookupKey(pair));
				if (it == m_entityToArchetypeMap.end())
					return;
				auto& records = it->second;

				// Remove any reference to the found archetype from the array.
				// We don't know the archetype so we remove/decrement any archetype record that contains our entity.
				for (uint32_t i = records.size() - 1; i != (uint32_t)-1; --i) {
					auto& record = records[i];
					const auto* pArchetype = record.pArchetype;
					if (!pArchetype->has(entityToRemove))
						continue;

					if ((!is_wildcard(pair.first()) && !is_wildcard(pair.second())) || record.matchCount <= 1)
						core::swap_erase_unsafe(records, i);
					else
						--record.matchCount;
				}

				if (records.empty())
					m_entityToArchetypeMap.erase(it);
			}

			//! Deletes a known archetype from the <pairEntity, archetype> map.
			//! Used when deleting pair entities, where the owning archetype is already known.
			void del_entity_query_pair(Pair pair, Archetype* pArchetypeToRemove) {
				GAIA_ASSERT(pArchetypeToRemove != nullptr);

				auto it = m_entityToArchetypeMap.find(EntityLookupKey(pair));
				if (it == m_entityToArchetypeMap.end())
					return;

				auto& records = it->second;
				const auto idx = find_component_index_record(records, pArchetypeToRemove);
				if (idx != BadIndex)
					core::swap_erase_unsafe(records, idx);

				if (records.empty())
					m_entityToArchetypeMap.erase(it);
			}

			void del_pair_archetype_query_pairs(Entity pair, Archetype* pArchetypeToRemove) {
				GAIA_ASSERT(pair.pair());
				GAIA_ASSERT(pArchetypeToRemove != nullptr);

				GAIA_ASSERT(pair.id() < m_recs.entities.size());
				GAIA_ASSERT(pair.gen() < m_recs.entities.size());
				const auto first = m_recs.entities.handle(pair.id());
				const auto second = m_recs.entities.handle(pair.gen());

				del_entity_query_pair(Pair(All, second), pArchetypeToRemove);
				del_entity_query_pair(Pair(first, All), pArchetypeToRemove);
				del_entity_query_pair(Pair(All, All), pArchetypeToRemove);
			}

			void del_pair_archetype_query_pairs(Entity pair, Entity entityToRemove) {
				GAIA_ASSERT(pair.pair());

				GAIA_ASSERT(pair.id() < m_recs.entities.size());
				GAIA_ASSERT(pair.gen() < m_recs.entities.size());
				const auto first = m_recs.entities.handle(pair.id());
				const auto second = m_recs.entities.handle(pair.gen());

				del_entity_query_pair(Pair(All, second), entityToRemove);
				del_entity_query_pair(Pair(first, All), entityToRemove);
				del_entity_query_pair(Pair(All, All), entityToRemove);
			}

			//! Deletes a known archetype from the <entity, archetype> map.
			//! Used when unregistering an archetype from the world.
			void del_entity_archetype_pair(Entity entity, Archetype* pArchetypeToRemove) {
				GAIA_ASSERT(entity != Pair(All, All));
				GAIA_ASSERT(pArchetypeToRemove != nullptr);

				auto it = m_entityToArchetypeMap.find(EntityLookupKey(entity));
				if (it == m_entityToArchetypeMap.end())
					return;

				auto& records = it->second;
				const auto idx = find_component_index_record(records, pArchetypeToRemove);
				if (idx != BadIndex)
					core::swap_erase_unsafe(records, idx);

				if (records.empty())
					m_entityToArchetypeMap.erase(it);
			}

			//! Deletes a known archetype from all of its entity and wildcard-pair lookup buckets.
			void del_archetype_entity_pairs(Archetype* pArchetype) {
				GAIA_ASSERT(pArchetype != nullptr);

				for (const auto entity: pArchetype->ids_view()) {
					del_entity_archetype_pair(entity, pArchetype);

					if (!entity.pair())
						continue;

					// Archetype unregistration can run while the pair's relation or target entity is already
					// invalid. Rebuild wildcard pair lookup keys from the stored entity records instead of
					// calling get(), which asserts on invalidated ids.
					GAIA_ASSERT(entity.id() < m_recs.entities.size());
					GAIA_ASSERT(entity.gen() < m_recs.entities.size());
					del_pair_archetype_query_pairs(entity, pArchetype);
				}
			}

			//! Deletes an archetype from the <entity, archetype> map
			//! \param entity Entity getting deleted
			void del_entity_archetype_pairs(Entity entity, Archetype* pArchetype) {
				GAIA_ASSERT(entity != Pair(All, All));

				m_entityToArchetypeMap.erase(EntityLookupKey(entity));

				if (entity.pair()) {
					if (pArchetype != nullptr) {
						del_pair_archetype_query_pairs(entity, pArchetype);
					} else {
						del_pair_archetype_query_pairs(entity, entity);
					}
				}
			}

			//! Creates a new archetype from a given set of entities
			//! \param entities Archetype entities/components
			//! \return Pointer to the new archetype.
			GAIA_NODISCARD Archetype* create_archetype(EntitySpan entities) {
				GAIA_ASSERT(m_nextArchetypeId < (decltype(m_nextArchetypeId))-1);
				auto* pArchetype = Archetype::create(*this, m_nextArchetypeId++, m_worldVersion, entities);

				const auto entityCnt = (uint32_t)entities.size();
				GAIA_FOR(entityCnt) {
					auto entity = entities[i];
					add_entity_archetype_pair(entity, pArchetype, (uint16_t)i);

#if GAIA_OBSERVERS_ENABLED
					auto& ec = fetch(entity);
					if ((ec.flags & EntityContainerFlags::IsObserved) != 0 || m_observers.has_observers(entity)) {
						ec.flags |= EntityContainerFlags::IsObserved;
						pArchetype->observed_terms_inc();
					}
#endif

					// If the entity is a pair, make sure to create special wildcard records for it
					// as well so wildcard queries can find the archetype.
					if (entity.pair()) {
						add_pair_archetype_query_pairs(entity, pArchetype);
					}
				}

				return pArchetype;
			}

			//! Registers the archetype in the world.
			//! \param pArchetype Archetype to register.
			void reg_archetype(Archetype* pArchetype) {
				GAIA_ASSERT(pArchetype != nullptr);

				// // Make sure hashes were set already
				// GAIA_ASSERT(
				// 		(m_archetypesById.empty() || pArchetype == m_pRootArchetype) || (pArchetype->lookup_hash().hash != 0));

				// Make sure the archetype is not registered yet
				GAIA_ASSERT(pArchetype->list_idx() == BadIndex);

				// Register the archetype
				[[maybe_unused]] const auto it0 =
						m_archetypesById.emplace(ArchetypeIdLookupKey(pArchetype->id(), pArchetype->id_hash()), pArchetype);
				[[maybe_unused]] const auto it1 =
						m_archetypesByHash.emplace(ArchetypeLookupKey(pArchetype->lookup_hash(), pArchetype), pArchetype);

				GAIA_ASSERT(it0.second);
				GAIA_ASSERT(it1.second);

				pArchetype->list_idx(m_archetypes.size());
				m_archetypes.emplace_back(pArchetype);

				m_queryCache.register_archetype_with_queries(pArchetype);
			}

			//! Unregisters the archetype from the world.
			//! \param pArchetype Archetype to unregister.
			void unreg_archetype(Archetype* pArchetype) {
				GAIA_ASSERT(pArchetype != nullptr);

				// Make sure hashes were set already
				GAIA_ASSERT(
						(m_archetypesById.empty() || pArchetype == m_pRootArchetype) || (pArchetype->lookup_hash().hash != 0));

				// Make sure the archetype was registered already
				GAIA_ASSERT(pArchetype->list_idx() != BadIndex);

				// Query rematching uses the entity -> archetype lookup map as an input. Remove this
				// archetype from all of its lookup buckets before destroying it so dead archetype
				// pointers cannot be reintroduced into cached query state during the next rematch.
				del_archetype_entity_pairs(pArchetype);

				// Break graph connections
				{
					auto& edgeLefts = pArchetype->left_edges();
					for (auto& itLeft: edgeLefts)
						remove_edge_from_archetype(pArchetype, itLeft.second, itLeft.first.entity());
				}

				auto tmpArchetype = ArchetypeLookupChecker(pArchetype->ids_view());
				[[maybe_unused]] const auto res0 =
						m_archetypesById.erase(ArchetypeIdLookupKey(pArchetype->id(), pArchetype->id_hash()));
				[[maybe_unused]] const auto res1 =
						m_archetypesByHash.erase(ArchetypeLookupKey(pArchetype->lookup_hash(), &tmpArchetype));
				GAIA_ASSERT(res0 != 0);
				GAIA_ASSERT(res1 != 0);

				const auto idx = pArchetype->list_idx();
				GAIA_ASSERT(idx == core::get_index(m_archetypes, pArchetype));
				core::swap_erase(m_archetypes, idx);
				if (!m_archetypes.empty() && idx != m_archetypes.size())
					m_archetypes[idx]->list_idx(idx);
			}

#if GAIA_ASSERT_ENABLED
			static void print_archetype_entities(const World& world, const Archetype& archetype, Entity entity, bool adding) {
				auto ids = archetype.ids_view();

				GAIA_LOG_W("Currently present:");
				GAIA_EACH(ids) {
					const auto name = entity_name(world, ids[i]);
					GAIA_LOG_W(
							"> [%u] %.*s [%s]", i, (int)name.size(), name.empty() ? "" : name.data(),
							EntityKindString[(uint32_t)ids[i].kind()]);
				}

				GAIA_LOG_W("Trying to %s:", adding ? "add" : "del");
				const auto name = entity_name(world, entity);
				GAIA_LOG_W(
						"> %.*s [%s]", (int)name.size(), name.empty() ? "" : name.data(),
						EntityKindString[(uint32_t)entity.kind()]);
			}

			static void verify_add(const World& world, Archetype& archetype, Entity entity, Entity addEntity) {
				// Make sure the world is not locked
				if (world.locked()) {
					GAIA_ASSERT2(false, "Trying to add an entity while the world is locked");
					GAIA_LOG_W("Trying to add an entity [%u:%u] while the world is locked", entity.id(), entity.gen());
					print_archetype_entities(world, archetype, entity, false);
					return;
				}

				// Makes sure no wildcard entities are added
				if (is_wildcard(addEntity)) {
					GAIA_ASSERT2(false, "Adding wildcard pairs is not supported");
					print_archetype_entities(world, archetype, addEntity, true);
					return;
				}

				// Make sure not to add too many entities/components
				auto ids = archetype.ids_view();
				if GAIA_UNLIKELY (ids.size() + 1 >= ChunkHeader::MAX_COMPONENTS) {
					GAIA_ASSERT2(false, "Trying to add too many entities to entity!");
					GAIA_LOG_W("Trying to add an entity to entity [%u:%u] but there's no space left!", entity.id(), entity.gen());
					print_archetype_entities(world, archetype, addEntity, true);
					return;
				}
			}

			static void verify_del(const World& world, Archetype& archetype, Entity entity, Entity func_del) {
				// Make sure the world is not locked
				if (world.locked()) {
					GAIA_ASSERT2(false, "Trying to delete an entity while the world is locked");
					GAIA_LOG_W("Trying to delete an entity [%u:%u] while the world is locked", entity.id(), entity.gen());
					print_archetype_entities(world, archetype, entity, false);
					return;
				}

				// Make sure the entity is present on the archetype
				if GAIA_UNLIKELY (!archetype.has(func_del)) {
					GAIA_ASSERT2(false, "Trying to remove an entity which wasn't added");
					GAIA_LOG_W("Trying to del an entity from entity [%u:%u] but it was never added", entity.id(), entity.gen());
					print_archetype_entities(world, archetype, func_del, false);
					return;
				}
			}

			static void verify_enable(const World& world, Archetype& archetype, Entity entity) {
				if (world.locked()) {
					GAIA_ASSERT2(false, "Trying to enable/disable an entity while the world is locked");
					GAIA_LOG_W("Trying to enable/disable an entity [%u:%u] while the world is locked", entity.id(), entity.gen());
					print_archetype_entities(world, archetype, entity, false);
				}
			}

			static void verify_move(const World& world, Archetype& archetype, Entity entity) {
				if (world.locked()) {
					GAIA_ASSERT2(false, "Trying to move an entity while the world is locked");
					GAIA_LOG_W("Trying to move an entity [%u:%u] while the world is locked", entity.id(), entity.gen());
					print_archetype_entities(world, archetype, entity, false);
				}
			}
#endif

			//! Searches for an archetype which is formed by adding entity \param entity of \param pArchetypeLeft.
			//! If no such archetype is found a new one is created.
			//! \param pArchetypeLeft Archetype we originate from.
			//! \param entity Entity we want to add.
			//! \return Archetype pointer.
			GAIA_NODISCARD Archetype* foc_archetype_add(Archetype* pArchetypeLeft, Entity entity) {
				// Check if the component is found when following the "add" edges
				bool edgeNeedsRebuild = false;
				{
					const auto edge = pArchetypeLeft->find_edge_right(entity);
					if (edge != ArchetypeIdHashPairBad) {
						auto it = m_archetypesById.find(ArchetypeIdLookupKey(edge.id, edge.hash));
						if (it != m_archetypesById.end() && it->second != nullptr)
							return it->second;

						// Drop stale local cache edge and rebuild it below.
						pArchetypeLeft->del_graph_edge_right_local(entity);
						edgeNeedsRebuild = true;
					}
				}

				// Prepare a joint array of components of old + the newly added component
				cnt::sarray_ext<Entity, ChunkHeader::MAX_COMPONENTS> entsNew;
				{
					auto entsOld = pArchetypeLeft->ids_view();
					const auto entsOldCnt = entsOld.size();
					entsNew.resize((uint32_t)entsOld.size() + 1);
					GAIA_FOR(entsOldCnt) entsNew[i] = entsOld[i];
					entsNew[(uint32_t)entsOld.size()] = entity;
				}

				// Make sure to sort the components so we receive the same hash no matter the order in which components
				// are provided Bubble sort is okay. We're dealing with at most ChunkHeader::MAX_COMPONENTS items.
				sort(entsNew, SortComponentCond{});

				// Once sorted we can calculate the hashes
				const auto hashLookup = calc_lookup_hash({entsNew.data(), entsNew.size()}).hash;
				auto* pArchetypeRight = find_archetype({hashLookup}, {entsNew.data(), entsNew.size()});
				if (pArchetypeRight == nullptr) {
					pArchetypeRight = create_archetype({entsNew.data(), entsNew.size()});
					pArchetypeRight->set_hashes({hashLookup});
					reg_archetype(pArchetypeRight);
					edgeNeedsRebuild = true;
				}

				if (edgeNeedsRebuild)
					pArchetypeLeft->build_graph_edges(pArchetypeRight, entity);

				return pArchetypeRight;
			}

			//! Batched builder variant of foc_archetype_add() that resolves the target archetype without
			//! consulting or mutating the archetype graph. This avoids graph churn on intermediate batch steps.
			GAIA_NODISCARD Archetype* foc_archetype_add_no_graph(Archetype* pArchetypeLeft, Entity entity) {
				cnt::sarray_ext<Entity, ChunkHeader::MAX_COMPONENTS> entsNew;
				{
					auto entsOld = pArchetypeLeft->ids_view();
					const auto entsOldCnt = entsOld.size();
					entsNew.resize((uint32_t)entsOld.size() + 1);
					GAIA_FOR(entsOldCnt) entsNew[i] = entsOld[i];
					entsNew[(uint32_t)entsOld.size()] = entity;
				}

				sort(entsNew, SortComponentCond{});

				const auto hashLookup = calc_lookup_hash({entsNew.data(), entsNew.size()}).hash;
				auto* pArchetypeRight = find_archetype({hashLookup}, {entsNew.data(), entsNew.size()});
				if (pArchetypeRight != nullptr)
					return pArchetypeRight;

				pArchetypeRight = create_archetype({entsNew.data(), entsNew.size()});
				pArchetypeRight->set_hashes({hashLookup});
				reg_archetype(pArchetypeRight);
				return pArchetypeRight;
			}

			//! Searches for an archetype which is formed by removing entity \param entity from \param pArchetypeRight.
			//! If no such archetype is found a new one is created.
			//! \param pArchetypeRight Archetype we originate from.
			//! \param entity Component we want to remove.
			//! \return Pointer to archetype.
			GAIA_NODISCARD Archetype* foc_archetype_del(Archetype* pArchetypeRight, Entity entity) {
				// Check if the component is found when following the "del" edges
				bool edgeNeedsRebuild = false;
				{
					const auto edge = pArchetypeRight->find_edge_left(entity);
					if (edge != ArchetypeIdHashPairBad) {
						const auto it = m_archetypesById.find(ArchetypeIdLookupKey(edge.id, edge.hash));
						if (it != m_archetypesById.end()) {
							auto* pArchetypeLeft = it->second;
							if (pArchetypeLeft != nullptr)
								return pArchetypeLeft;
						}

						// Drop stale local cache edge and rebuild it below.
						pArchetypeRight->del_graph_edge_left_local(entity);
						edgeNeedsRebuild = true;
					}
				}

				cnt::sarray_ext<Entity, ChunkHeader::MAX_COMPONENTS> entsNew;
				auto entsOld = pArchetypeRight->ids_view();

				// Find the intersection
				for (const auto e: entsOld) {
					if (e == entity)
						continue;

					entsNew.push_back(e);
				}

				// Verify there was a change
				GAIA_ASSERT(entsNew.size() != entsOld.size());

				// Calculate the hashes
				const auto hashLookup = calc_lookup_hash({entsNew.data(), entsNew.size()}).hash;
				auto* pArchetype = find_archetype({hashLookup}, {entsNew.data(), entsNew.size()});
				if (pArchetype == nullptr) {
					pArchetype = create_archetype({entsNew.data(), entsNew.size()});
					pArchetype->set_hashes({hashLookup});
					reg_archetype(pArchetype);
					edgeNeedsRebuild = true;
				}

				if (edgeNeedsRebuild)
					pArchetype->build_graph_edges(pArchetypeRight, entity);

				return pArchetype;
			}

			//! Batched builder variant of foc_archetype_del() that resolves the target archetype without
			//! consulting or mutating the archetype graph. This avoids graph churn on intermediate batch steps.
			GAIA_NODISCARD Archetype* foc_archetype_del_no_graph(Archetype* pArchetypeRight, Entity entity) {
				cnt::sarray_ext<Entity, ChunkHeader::MAX_COMPONENTS> entsNew;
				auto entsOld = pArchetypeRight->ids_view();

				for (const auto e: entsOld) {
					if (e == entity)
						continue;

					entsNew.push_back(e);
				}

				GAIA_ASSERT(entsNew.size() != entsOld.size());

				const auto hashLookup = calc_lookup_hash({entsNew.data(), entsNew.size()}).hash;
				auto* pArchetype = find_archetype({hashLookup}, {entsNew.data(), entsNew.size()});
				if (pArchetype != nullptr)
					return pArchetype;

				pArchetype = create_archetype({entsNew.data(), entsNew.size()});
				pArchetype->set_hashes({hashLookup});
				reg_archetype(pArchetype);
				return pArchetype;
			}

			//! Returns an array of archetypes registered in the world
			//! \return Array or archetypes.
			GAIA_NODISCARD const auto& archetypes() const {
				return m_archetypes;
			}

			//! Returns the archetype the entity belongs to.
			//! \param entity Entity
			//! \return Reference to the archetype.
			GAIA_NODISCARD Archetype& archetype(Entity entity) {
				const auto& ec = fetch(entity);
				return *ec.pArchetype;
			}

			//! Removes any name associated with the entity
			//! \param entity Entity the name of which we want to delete
			void del_name(EntityContainer& ec, Entity entity) {
				EntityBuilder(*this, entity, ec).del_name();
			}

			//! Removes any name associated with the entity
			//! \param entity Entity the name of which we want to delete
			void del_name(Entity entity) {
				EntityBuilder(*this, entity).del_name();
			}

			//! Deletes an entity along with all data associated with it.
			//! Ignored for invalid entities or pairs.
			//! \param entity Entity to delete
			//! \param invalidate If true all entity records are invalidated
			void del_entity(Entity entity, bool invalidate) {
				if (entity.pair() || entity == EntityBad)
					return;

				auto& ec = fetch(entity);
				del_entity_inter(ec, entity, invalidate);
			}

			//! Deletes an entity along with all data associated with it.
			//! Ignored for invalid entities or pairs.
			//! \param ec Entity container records
			//! \param entity Entity to delete
			//! \param invalidate If true all entity records are invalidated
			void del_entity(EntityContainer& ec, Entity entity, bool invalidate) {
				if (entity.pair() || entity == EntityBad)
					return;

				del_entity_inter(ec, entity, invalidate);
			}

			//! Deletes an entity along with all data associated with it.
			//! \param ec Entity container record
			//! \param entity Entity to delete
			//! \param invalidate If true, all entity records are deleted
			void del_entity_inter(EntityContainer& ec, Entity entity, bool invalidate) {
				GAIA_ASSERT(entity.id() > GAIA_ID(LastCoreComponent).id());

				// if (!is_req_del(ec))
				{
					if (m_recs.entities.item_count() == 0)
						return;

#if GAIA_ASSERT_ENABLED
					auto* pChunk = ec.pChunk;
					GAIA_ASSERT(pChunk != nullptr);
#endif

					// Remove the entity from its chunk.
					// We call del_name first because remove_entity calls component destructors.
					// If the call was made inside invalidate_entity we would access a memory location
					// which has already been destructed which is not nice.
					del_name(ec, entity);
					remove_entity(*ec.pArchetype, *ec.pChunk, ec.row);
					remove_src_entity_version(entity);
				}

				// Invalidate on-demand.
				// We delete as a separate step in the delayed deletion.
				if (invalidate)
					invalidate_entity(entity);
			}

			//! Deletes all entities (and in turn chunks) from \param archetype.
			//! If an archetype forming entity is present, the chunk is treated as if it were empty
			//! and normal dying procedure is applied to it. At the last dying tick the entity is
			//! deleted so the chunk can be removed.
			void del_entities(Archetype& archetype) {
				for (auto* pChunk: archetype.chunks()) {
					auto ids = pChunk->entity_view();
					for (auto e: ids) {
						if (!valid(e))
							continue;

#if GAIA_ASSERT_ENABLED
						const auto& ec = fetch(e);

						// We should never end up trying to delete a forbidden-to-delete entity
						GAIA_ASSERT((ec.flags & EntityContainerFlags::OnDeleteTarget_Error) == 0);
#endif

						del_entity(e, true);
					}

					validate_chunk(pChunk);

					// If the chunk was already dying we need to remove it from the delete list
					// because we can delete it right away.
					if (pChunk->queued_for_deletion())
						remove_chunk_from_delete_queue(pChunk->delete_queue_index());

					remove_chunk(archetype, *pChunk);
				}

				validate_entities();
			}

			//! Deletes the entity
			//! \param entity Entity to delete
			void del_inter(Entity entity) {
				auto on_delete = [this](Entity entityToDel) {
					auto& ec = fetch(entityToDel);
					handle_del_entity(ec, entityToDel);
				};

				if (is_wildcard(entity)) {
					const auto rel = get(entity.id());
					const auto tgt = get(entity.gen());

					// (*,*)
					if (rel == All && tgt == All) {
						GAIA_ASSERT2(false, "Not supported yet");
					}
					// (*,X)
					else if (rel == All) {
						if (const auto* pTargets = relations(tgt)) {
							// handle_del might invalidate the targets map so we need to make a copy
							// TODO: this is suboptimal at best, needs to be optimized
							cnt::darray_ext<Entity, 64> tmp;
							for (auto key: *pTargets)
								tmp.push_back(key.entity());
							for (auto e: tmp)
								on_delete(Pair(e, tgt));
						}
					}
					// (X,*)
					else if (tgt == All) {
						if (const auto* pRelations = targets(rel)) {
							// handle_del might invalidate the targets map so we need to make a copy
							// TODO: this is suboptimal at best, needs to be optimized
							cnt::darray_ext<Entity, 64> tmp;
							for (auto key: *pRelations)
								tmp.push_back(key.entity());
							for (auto e: tmp)
								on_delete(Pair(rel, e));
						}
					}
				} else {
					on_delete(entity);
				}
			}

			// Force-delete all entities from the requested archetypes along with the archetype itself
			void del_finalize_archetypes() {
				GAIA_PROF_SCOPE(World::del_finalize_archetypes);

				for (auto& key: m_reqArchetypesToDel) {
					auto* pArchetype = key.archetype();
					if (pArchetype == nullptr)
						continue;

					del_entities(*pArchetype);

					// Now that all entities are deleted, all their chunks are requested to get deleted
					// and in turn the archetype itself as well. Therefore, it is added to the archetype
					// delete list and picked up by del_empty_archetypes. No need to call deletion from here.
					// > del_empty_archetype(pArchetype);
				}
				m_reqArchetypesToDel.clear();
			}

			//! Try to delete all requested entities
			void del_finalize_entities() {
				GAIA_PROF_SCOPE(World::del_finalize_entities);

				for (auto it = m_reqEntitiesToDel.begin(); it != m_reqEntitiesToDel.end();) {
					const auto e = it->entity();

					// Entities that form archetypes need to stay until the archetype itself is gone
					if (m_entityToArchetypeMap.contains(*it)) {
						++it;
						continue;
					}

					// Requested entities are partially deleted. We only need to invalidate them.
					invalidate_entity(e);

					it = m_reqEntitiesToDel.erase(it);
				}
			}

			//! Finalize all queued delete operations
			void del_finalize() {
				GAIA_PROF_SCOPE(World::del_finalize);

				del_finalize_archetypes();
				del_finalize_entities();
			}

			GAIA_NODISCARD bool archetype_cond_match(Archetype& archetype, Pair cond, Entity target) const {
				// E.g.:
				//   target = (All, entity)
				//   cond = (OnDeleteTarget, delete)
				// Delete the entity if it matches the cond
				auto ids = archetype.ids_view();

				if (target.pair()) {
					for (auto e: ids) {
						// Find the pair which matches (All, entity)
						if (!e.pair())
							continue;
						if (e.gen() != target.gen())
							continue;

						const auto& ec = m_recs.entities[e.id()];
						const auto entity = ec.pChunk->entity_view()[ec.row];
						if (!has(entity, cond))
							continue;

						return true;
					}
				} else {
					for (auto e: ids) {
						if (e.pair())
							continue;
						if (!has(e, cond))
							continue;

						return true;
					}
				}

				return false;
			}

			//! Updates all chunks and entities of archetype @a srcArchetype so they are a part of @a dstArchetype
			//! \param srcArchetype Source archetype
			//! \param dstArchetype Destination archeytpe
			void move_to_archetype(Archetype& srcArchetype, Archetype& dstArchetype) {
				GAIA_ASSERT(&srcArchetype != &dstArchetype);

				bool updated = false;

				for (auto* pSrcChunk: srcArchetype.chunks()) {
					auto srcEnts = pSrcChunk->entity_view();
					if (srcEnts.empty())
						continue;

					// Copy entities back-to-front to avoid unnecessary data movements.
					// TODO: Handle disabled entities efficiently.
					//       If there are disabled entities, we still do data movements if there already
					//       are enabled entities in the chunk.
					// TODO: If the header was of some fixed size, e.g. if we always acted as if we had
					//       ChunkHeader::MAX_COMPONENTS, certain data movements could be done pretty much instantly.
					//       E.g. when removing tags or pairs, we would simply replace the chunk pointer
					//       with a pointer to another one. The same goes for archetypes. Component data
					//       would not have to move at all internal chunk header pointers would remain unchanged.

					uint32_t i = (uint32_t)srcEnts.size();
					while (i != 0) {
						auto* pDstChunk = dstArchetype.foc_free_chunk();
						const uint32_t dstSpaceLeft = pDstChunk->capacity() - pDstChunk->size();
						const uint32_t cnt = core::get_min(dstSpaceLeft, i);
						for (uint32_t j = 0; j < cnt; ++j) {
							auto e = srcEnts[i - j - 1];
							move_entity(e, fetch(e), dstArchetype, *pDstChunk);
						}

						pDstChunk->update_world_version();
						pDstChunk->update_entity_order_version();

						GAIA_ASSERT(cnt <= i);
						i -= cnt;
					}

					pSrcChunk->update_world_version();
					pSrcChunk->update_entity_order_version();
					updated = true;
				}

				if (updated)
					update_version(m_worldVersion);
			}

			//! Find the destination archetype \param pArchetype as if removing \param entity.
			//! \return Destination archetype of nullptr if no match is found
			GAIA_NODISCARD Archetype* calc_dst_archetype_ent(Archetype* pArchetype, Entity entity) {
				GAIA_ASSERT(!is_wildcard(entity));

				auto ids = pArchetype->ids_view();
				for (auto id: ids) {
					if (id != entity)
						continue;

					return foc_archetype_del(pArchetype, id);
				}

				return nullptr;
			}

			//! Find the destination archetype \param pArchetype as if removing all entities
			//! matching (All, entity) from the archetype.
			//! \return Destination archetype of nullptr if no match is found
			GAIA_NODISCARD Archetype* calc_dst_archetype_all_ent(Archetype* pArchetype, Entity entity) {
				GAIA_ASSERT(is_wildcard(entity));

				Archetype* pDstArchetype = pArchetype;

				auto ids = pArchetype->ids_view();
				for (auto id: ids) {
					if (!id.pair() || id.gen() != entity.gen())
						continue;

					pDstArchetype = foc_archetype_del(pDstArchetype, id);
				}

				return pArchetype != pDstArchetype ? pDstArchetype : nullptr;
			}

			//! Find the destination archetype \param pArchetype as if removing all entities
			//! matching (entity, All) from the archetype.
			//! \return Destination archetype of nullptr if no match is found
			GAIA_NODISCARD Archetype* calc_dst_archetype_ent_all(Archetype* pArchetype, Entity entity) {
				GAIA_ASSERT(is_wildcard(entity));

				Archetype* pDstArchetype = pArchetype;

				auto ids = pArchetype->ids_view();
				for (auto id: ids) {
					if (!id.pair() || id.id() != entity.id())
						continue;

					pDstArchetype = foc_archetype_del(pDstArchetype, id);
				}

				return pArchetype != pDstArchetype ? pDstArchetype : nullptr;
			}

			//! Find the destination archetype \param pArchetype as if removing all entities
			//! matching (All, All) from the archetype.
			//! \return Destination archetype of nullptr if no match is found
			GAIA_NODISCARD Archetype* calc_dst_archetype_all_all(Archetype* pArchetype, [[maybe_unused]] Entity entity) {
				GAIA_ASSERT(is_wildcard(entity));

				Archetype* pDstArchetype = pArchetype;
				bool found = false;

				auto ids = pArchetype->ids_view();
				for (auto id: ids) {
					if (!id.pair())
						continue;

					pDstArchetype = foc_archetype_del(pDstArchetype, id);
					found = true;
				}

				return found ? pDstArchetype : nullptr;
			}

			//! Find the destination archetype \param pArchetype as if removing \param entity.
			//! Wildcard pair entities are supported as well.
			//! \return Destination archetype of nullptr if no match is found
			GAIA_NODISCARD Archetype* calc_dst_archetype(Archetype* pArchetype, Entity entity) {
				if (entity.pair()) {
					auto rel = entity.id();
					auto tgt = entity.gen();

					// Removing a wildcard pair. We need to find all pairs matching it.
					if (rel == All.id() || tgt == All.id()) {
						// (first, All) means we need to match (first, A), (first, B), ...
						if (rel != All.id() && tgt == All.id())
							return calc_dst_archetype_ent_all(pArchetype, entity);

						// (All, second) means we need to match (A, second), (B, second), ...
						if (rel == All.id() && tgt != All.id())
							return calc_dst_archetype_all_ent(pArchetype, entity);

						// (All, All) means we need to match all relationships
						return calc_dst_archetype_all_all(pArchetype, EntityBad);
					}
				}

				// Non-wildcard pair or entity
				return calc_dst_archetype_ent(pArchetype, entity);
			}

			void req_del(Archetype& archetype) {
				if (archetype.is_req_del())
					return;

				archetype.req_del();
				m_reqArchetypesToDel.insert(ArchetypeLookupKey(archetype.lookup_hash(), &archetype));
			}

			void req_del(EntityContainer& ec, Entity entity) {
				if (is_req_del(ec))
					return;

#if GAIA_OBSERVERS_ENABLED
				auto delDiffCtx =
						m_observers.prepare_diff(*this, ObserverEvent::OnDel, EntitySpan{&entity, 1}, EntitySpan{&entity, 1});
#endif
				del_entity(ec, entity, false);
#if GAIA_OBSERVERS_ENABLED
				m_observers.finish_diff(*this, GAIA_MOV(delDiffCtx));
#endif

				ec.req_del();
				m_reqEntitiesToDel.insert(EntityLookupKey(entity));
			}

			void invalidate_pair_removal_caches(Entity entity) {
				if (!entity.pair())
					return;

				auto invalidate_relation = [this](Entity relation) {
					if (relation == EntityBad)
						return;
					touch_rel_version(relation);
					invalidate_queries_for_rel(relation);
				};

				if (entity.id() != All.id()) {
					invalidate_relation(try_get(entity.id()));
				} else if (entity.gen() != All.id()) {
					const auto target = try_get(entity.gen());
					if (target != EntityBad) {
						if (const auto* pRelations = relations(target)) {
							for (auto relationKey: *pRelations)
								invalidate_relation(relationKey.entity());
						}
					}
				} else {
					for (const auto& [relationKey, _]: m_relToTgt)
						invalidate_relation(relationKey.entity());
				}

				m_targetsTravCache = {};
				m_srcBfsTravCache = {};
				m_depthOrderCache = {};
				m_sourcesAllCache = {};
				m_targetsAllCache = {};
			}

			void unlink_live_is_relation(Entity source, Entity target) {
				const auto sourceKey = EntityLookupKey(source);
				const auto targetKey = EntityLookupKey(target);

				invalidate_queries_for_entity({Is, target});

				if (const auto itTargets = m_entityToAsTargets.find(sourceKey); itTargets != m_entityToAsTargets.end()) {
					itTargets->second.erase(targetKey);
					if (itTargets->second.empty())
						m_entityToAsTargets.erase(itTargets);
				}
				m_entityToAsTargetsTravCache = {};

				if (const auto itRelations = m_entityToAsRelations.find(targetKey);
						itRelations != m_entityToAsRelations.end()) {
					itRelations->second.erase(sourceKey);
					if (itRelations->second.empty())
						m_entityToAsRelations.erase(itRelations);
				}
				m_entityToAsRelationsTravCache = {};
			}

			void unlink_stale_is_relations_by_target_id(Entity source, EntityId targetId) {
				const auto sourceKey = EntityLookupKey(source);
				const auto itTargets = m_entityToAsTargets.find(sourceKey);
				if (itTargets == m_entityToAsTargets.end())
					return;

				cnt::darray_ext<EntityLookupKey, 4> removedTargets;
				for (auto targetKey: itTargets->second) {
					if (targetKey.entity().id() == targetId)
						removedTargets.push_back(targetKey);
				}

				for (auto targetKey: removedTargets) {
					invalidate_queries_for_structural_entity(EntityLookupKey(Pair{Is, targetKey.entity()}));
					itTargets->second.erase(targetKey);

					const auto itRelations = m_entityToAsRelations.find(targetKey);
					if (itRelations != m_entityToAsRelations.end()) {
						itRelations->second.erase(sourceKey);
						if (itRelations->second.empty())
							m_entityToAsRelations.erase(itRelations);
					}
				}

				if (itTargets->second.empty())
					m_entityToAsTargets.erase(itTargets);

				m_entityToAsTargetsTravCache = {};
				m_entityToAsRelationsTravCache = {};
			}

			template <typename Func>
			void each_delete_cascade_direct_source(Entity target, Pair cond, Func&& func) {
				GAIA_ASSERT(!target.pair());

				for (const auto& [relKey, store]: m_exclusiveAdjunctByRel) {
					const auto relation = relKey.entity();
					if (!has(relation, cond))
						continue;

					const auto* pSources = exclusive_adjunct_sources(store, target);
					if (pSources == nullptr)
						continue;

					for (auto source: *pSources)
						func(source);
				}

				const auto pairEntity = Pair(All, target);
				const auto it = m_entityToArchetypeMap.find(EntityLookupKey(pairEntity));
				if (it != m_entityToArchetypeMap.end()) {
					for (const auto& record: it->second) {
						auto* pArchetype = record.pArchetype;
						if (pArchetype == nullptr || pArchetype->is_req_del())
							continue;
						if (!archetype_cond_match(*pArchetype, cond, pairEntity))
							continue;

						for (const auto* pChunk: pArchetype->chunks()) {
							const auto entities = pChunk->entity_view();
							GAIA_EACH(entities)
							func(entities[i]);
						}
					}
				}
			}

			void collect_delete_cascade_direct_sources(Entity target, Pair cond, cnt::darray<Entity>& out) {
				GAIA_ASSERT(!target.pair());
				const auto visitStamp = next_entity_visit_stamp();
				each_delete_cascade_direct_source(target, cond, [&](Entity source) {
					if (!valid(source))
						return;
					if (!try_mark_entity_visited(source, visitStamp))
						return;
					out.push_back(source);
				});
			}

			void collect_delete_cascade_sources(Entity target, Pair cond, cnt::darray<Entity>& out) {
				GAIA_ASSERT(!target.pair());
				const auto visitStamp = next_entity_visit_stamp();
				cnt::darray_ext<Entity, 32> targetsToVisit;
				(void)try_mark_entity_visited(target, visitStamp);
				targetsToVisit.push_back(target);

				for (uint32_t i = 0; i < targetsToVisit.size(); ++i) {
					const auto currTarget = targetsToVisit[i];
					each_delete_cascade_direct_source(currTarget, cond, [&](Entity source) {
						if (!valid(source))
							return;
						if (!try_mark_entity_visited(source, visitStamp))
							return;

						out.push_back(source);
						targetsToVisit.push_back(source);
					});
				}
			}

			//! Requests deleting anything that references the \param entity. No questions asked.
			void req_del_entities_with(Entity entity) {
				GAIA_PROF_SCOPE(World::req_del_entities_with);

				GAIA_ASSERT(entity != Pair(All, All));

				auto req_del_adjunct_pair = [&](Entity relation, Entity target) {
					cnt::darray<Entity> sourcesToDel;
					sources(relation, target, [&](Entity source) {
						sourcesToDel.push_back(source);
					});

					for (auto source: sourcesToDel)
						req_del(fetch(source), source);
				};

				if (entity.pair()) {
					if (entity.id() != All.id()) {
						const auto relation = try_get(entity.id());
						const auto target = try_get(entity.gen());
						if (relation != EntityBad && target != EntityBad && is_exclusive_dont_fragment_relation(relation))
							req_del_adjunct_pair(relation, target);
					} else {
						const auto target = try_get(entity.gen());
						if (target == EntityBad)
							goto skip_req_del_all_target;
						for (const auto& [relKey, store]: m_exclusiveAdjunctByRel) {
							if (exclusive_adjunct_sources(store, target) == nullptr)
								continue;

							req_del_adjunct_pair(relKey.entity(), target);
						}
					skip_req_del_all_target:;
					}
				} else if (is_exclusive_dont_fragment_relation(entity)) {
					if (const auto* pTargets = targets(entity)) {
						for (auto targetKey: *pTargets)
							req_del_adjunct_pair(entity, targetKey.entity());
					}
				}

				const auto it = m_entityToArchetypeMap.find(EntityLookupKey(entity));
				if (it == m_entityToArchetypeMap.end())
					return;

				for (const auto& record: it->second)
					req_del(*record.pArchetype);
			}

			//! Requests deleting anything that references the \param entity. No questions asked.
			//! Takes \param cond into account.
			void req_del_entities_with(Entity entity, Pair cond) {
				cnt::set<EntityLookupKey> visited;
				req_del_entities_with(entity, cond, visited);
			}

			void req_del_entities_with(Entity entity, Pair cond, cnt::set<EntityLookupKey>& visited) {
				GAIA_PROF_SCOPE(World::req_del_entities_with);

				GAIA_ASSERT(entity != Pair(All, All));
				if (!visited.insert(EntityLookupKey(entity)).second)
					return;

				auto req_del_adjunct_pair = [&](Entity relation, Entity target) {
					if (!has(relation, cond))
						return;

					cnt::darray<Entity> sourcesToDel;
					sources(relation, target, [&](Entity source) {
						sourcesToDel.push_back(source);
					});

					for (auto source: sourcesToDel)
						req_del(fetch(source), source);
				};

				if (entity.pair()) {
					if (entity.id() != All.id()) {
						const auto relation = try_get(entity.id());
						const auto target = try_get(entity.gen());
						if (relation != EntityBad && target != EntityBad && is_exclusive_dont_fragment_relation(relation))
							req_del_adjunct_pair(relation, target);
					} else {
						const auto target = try_get(entity.gen());
						if (target == EntityBad)
							goto skip_req_del_all_target_cond;
						for (const auto& [relKey, store]: m_exclusiveAdjunctByRel) {
							if (exclusive_adjunct_sources(store, target) == nullptr)
								continue;

							req_del_adjunct_pair(relKey.entity(), target);
						}
					skip_req_del_all_target_cond:;
					}
				} else if (is_exclusive_dont_fragment_relation(entity)) {
					if (const auto* pTargets = targets(entity)) {
						for (auto targetKey: *pTargets)
							req_del_adjunct_pair(entity, targetKey.entity());
					}
				}

				const auto it = m_entityToArchetypeMap.find(EntityLookupKey(entity));
				if (it == m_entityToArchetypeMap.end())
					return;

				cnt::darray<Entity> cascadeTargets;
				if (entity.pair() && entity.id() == All.id()) {
					const auto target = try_get(entity.gen());
					if (target != EntityBad)
						collect_delete_cascade_direct_sources(target, cond, cascadeTargets);
				}

				for (auto source: cascadeTargets)
					req_del_entities_with(Pair(All, source), cond, visited);

				for (const auto& record: it->second) {
					auto* pArchetype = record.pArchetype;
					// Evaluate the condition if a valid pair is given
					if (!archetype_cond_match(*pArchetype, cond, entity))
						continue;

					req_del(*pArchetype);
				}
			}

			//! Removes the entity \param entity from anything referencing it.
			void rem_from_entities(Entity entity) {
				GAIA_PROF_SCOPE(World::rem_from_entities);

				invalidate_pair_removal_caches(entity);

				auto rem_adjunct_pair = [&](Entity relation, Entity target) {
					cnt::darray<Entity> sourcesToRem;
					sources(relation, target, [&](Entity source) {
						sourcesToRem.push_back(source);
					});

					for (auto source: sourcesToRem)
						del(source, Pair(relation, target));
				};

				if (entity.pair()) {
					if (entity.id() != All.id()) {
						const auto relation = try_get(entity.id());
						const auto target = try_get(entity.gen());
						if (relation != EntityBad && target != EntityBad && is_exclusive_dont_fragment_relation(relation))
							rem_adjunct_pair(relation, target);
					} else {
						const auto target = try_get(entity.gen());
						if (target == EntityBad)
							goto skip_rem_all_target;
						for (const auto& [relKey, store]: m_exclusiveAdjunctByRel) {
							if (exclusive_adjunct_sources(store, target) == nullptr)
								continue;

							rem_adjunct_pair(relKey.entity(), target);
						}
					skip_rem_all_target:;
					}
				} else if (is_exclusive_dont_fragment_relation(entity)) {
					if (const auto* pTargets = targets(entity)) {
						for (auto targetKey: *pTargets)
							rem_adjunct_pair(entity, targetKey.entity());
					}
				}

				const auto it = m_entityToArchetypeMap.find(EntityLookupKey(entity));
				if (it == m_entityToArchetypeMap.end())
					return;

				// Invalidate the singleton status if necessary
				if (!entity.pair()) {
					auto& ec = fetch(entity);
					if ((ec.flags & EntityContainerFlags::IsSingleton) != 0) {
						auto ids = ec.pArchetype->ids_view();
						const auto idx = core::get_index(ids, entity);
						if (idx != BadIndex)
							EntityBuilder::set_flag(ec.flags, EntityContainerFlags::IsSingleton, false);
					}
				}

#if GAIA_OBSERVERS_ENABLED
				cnt::set<EntityLookupKey> diffTermSet;
				cnt::darray<Entity> diffTerms;
				cnt::darray<Entity> diffTargets;
				for (const auto& record: it->second) {
					auto* pArchetype = record.pArchetype;
					if (pArchetype->is_req_del())
						continue;

					auto* pDstArchetype = calc_dst_archetype(pArchetype, entity);
					if (pDstArchetype == nullptr)
						continue;

					for (auto id: pArchetype->ids_view()) {
						bool matches = false;
						if (entity.pair()) {
							if (entity.id() == All.id() && entity.gen() == All.id())
								matches = id.pair();
							else if (entity.id() == All.id())
								matches = id.pair() && id.gen() == entity.gen();
							else if (entity.gen() == All.id())
								matches = id.pair() && id.id() == entity.id();
							else
								matches = id == entity;
						} else
							matches = id == entity;

						if (!matches)
							continue;

						if (diffTermSet.insert(EntityLookupKey(id)).second)
							diffTerms.push_back(id);
					}

					for (const auto* pChunk: pArchetype->chunks()) {
						const auto entities = pChunk->entity_view();
						GAIA_EACH(entities)
						diffTargets.push_back(entities[i]);
					}
				}
				auto delDiffCtx = diffTargets.empty() || diffTerms.empty()
															? ObserverRegistry::DiffDispatchCtx{}
															: m_observers.prepare_diff(
																		*this, ObserverEvent::OnDel, EntitySpan{diffTerms.data(), diffTerms.size()},
																		EntitySpan{diffTargets.data(), diffTargets.size()});
#endif

				// Update archetypes of all affected entities
				for (const auto& record: it->second) {
					auto* pArchetype = record.pArchetype;
					if (pArchetype->is_req_del())
						continue;

					if (entity.pair()) {
						cnt::darray_ext<Entity, 16> removedIsTargets;
						for (auto id: pArchetype->ids_view()) {
							bool matches = false;
							if (entity.id() == All.id() && entity.gen() == All.id())
								matches = id.pair();
							else if (entity.id() == All.id())
								matches = id.pair() && id.gen() == entity.gen();
							else if (entity.gen() == All.id())
								matches = id.pair() && id.id() == entity.id();
							else
								matches = id == entity;

							if (!matches || !id.pair() || id.id() != Is.id())
								continue;

							const auto target = try_get(id.gen());
							if (target != EntityBad)
								removedIsTargets.push_back(target);
						}

						if (!removedIsTargets.empty()) {
							for (const auto* pChunk: pArchetype->chunks()) {
								auto entities = pChunk->entity_view();
								GAIA_EACH(entities) {
									for (const auto target: removedIsTargets)
										unlink_live_is_relation(entities[i], target);
								}
							}
						}
					}

					auto* pDstArchetype = calc_dst_archetype(pArchetype, entity);
					if (pDstArchetype != nullptr)
						move_to_archetype(*pArchetype, *pDstArchetype);
				}

#if GAIA_OBSERVERS_ENABLED
				m_observers.finish_diff(*this, GAIA_MOV(delDiffCtx));
#endif
			}

			//! Removes the entity \param entity from anything referencing it.
			//! Takes \param cond into account.
			void rem_from_entities(Entity entity, Pair cond) {
				GAIA_PROF_SCOPE(World::rem_from_entities);

				invalidate_pair_removal_caches(entity);

				auto rem_adjunct_pair = [&](Entity relation, Entity target) {
					if (!has(relation, cond))
						return;

					cnt::darray<Entity> sourcesToRem;
					sources(relation, target, [&](Entity source) {
						sourcesToRem.push_back(source);
					});

					for (auto source: sourcesToRem)
						del(source, Pair(relation, target));
				};

				if (entity.pair()) {
					if (entity.id() != All.id()) {
						const auto relation = try_get(entity.id());
						const auto target = try_get(entity.gen());
						if (relation != EntityBad && target != EntityBad && is_exclusive_dont_fragment_relation(relation))
							rem_adjunct_pair(relation, target);
					} else {
						const auto target = try_get(entity.gen());
						if (target == EntityBad)
							goto skip_rem_all_target_cond;
						for (const auto& [relKey, store]: m_exclusiveAdjunctByRel) {
							if (exclusive_adjunct_sources(store, target) == nullptr)
								continue;

							rem_adjunct_pair(relKey.entity(), target);
						}
					skip_rem_all_target_cond:;
					}
				} else if (is_exclusive_dont_fragment_relation(entity)) {
					if (const auto* pTargets = targets(entity)) {
						for (auto targetKey: *pTargets)
							rem_adjunct_pair(entity, targetKey.entity());
					}
				}

				const auto it = m_entityToArchetypeMap.find(EntityLookupKey(entity));
				if (it == m_entityToArchetypeMap.end())
					return;

				// Invalidate the singleton status if necessary
				if (!entity.pair()) {
					auto& ec = fetch(entity);
					if ((ec.flags & EntityContainerFlags::IsSingleton) != 0) {
						auto ids = ec.pArchetype->ids_view();
						const auto idx = core::get_index(ids, entity);
						if (idx != BadIndex)
							EntityBuilder::set_flag(ec.flags, EntityContainerFlags::IsSingleton, false);
					}
				}

#if GAIA_OBSERVERS_ENABLED
				cnt::set<EntityLookupKey> diffTermSet;
				cnt::darray<Entity> diffTerms;
				cnt::darray<Entity> diffTargets;
				for (const auto& record: it->second) {
					auto* pArchetype = record.pArchetype;
					if (pArchetype->is_req_del())
						continue;

					if (!archetype_cond_match(*pArchetype, cond, entity))
						continue;

					auto* pDstArchetype = calc_dst_archetype(pArchetype, entity);
					if (pDstArchetype == nullptr)
						continue;

					for (auto id: pArchetype->ids_view()) {
						bool matches = false;
						if (entity.pair()) {
							if (entity.id() == All.id() && entity.gen() == All.id())
								matches = id.pair();
							else if (entity.id() == All.id())
								matches = id.pair() && id.gen() == entity.gen();
							else if (entity.gen() == All.id())
								matches = id.pair() && id.id() == entity.id();
							else
								matches = id == entity;
						} else
							matches = id == entity;

						if (!matches)
							continue;

						if (diffTermSet.insert(EntityLookupKey(id)).second)
							diffTerms.push_back(id);
					}

					for (const auto* pChunk: pArchetype->chunks()) {
						const auto entities = pChunk->entity_view();
						GAIA_EACH(entities)
						diffTargets.push_back(entities[i]);
					}
				}
				auto delDiffCtx = diffTargets.empty() || diffTerms.empty()
															? ObserverRegistry::DiffDispatchCtx{}
															: m_observers.prepare_diff(
																		*this, ObserverEvent::OnDel, EntitySpan{diffTerms.data(), diffTerms.size()},
																		EntitySpan{diffTargets.data(), diffTargets.size()});
#endif

				for (const auto& record: it->second) {
					auto* pArchetype = record.pArchetype;
					if (pArchetype->is_req_del())
						continue;

					// Evaluate the condition if a valid pair is given
					if (!archetype_cond_match(*pArchetype, cond, entity))
						continue;

					auto* pDstArchetype = calc_dst_archetype(pArchetype, entity);
					if (pDstArchetype != nullptr)
						move_to_archetype(*pArchetype, *pDstArchetype);
				}

#if GAIA_OBSERVERS_ENABLED
				m_observers.finish_diff(*this, GAIA_MOV(delDiffCtx));
#endif
			}

			//! Handles specific conditions that might arise from deleting \param entity.
			//! Conditions are:
			//!   OnDelete - deleting an entity/pair
			//!   OnDeleteTarget - deleting a pair's target
			//! Reactions are:
			//!   Remove - removes the entity/pair from anything referencing it
			//!   Delete - delete everything referencing the entity
			//!   Error - error out when deleted
			//! These rules can be set up as:
			//!   e.add(Pair(OnDelete, Remove));
			void handle_del_entity(EntityContainer& ec, Entity entity) {
				GAIA_PROF_SCOPE(World::handle_del_entity);

				GAIA_ASSERT(!is_wildcard(entity));

				if (entity.pair()) {
					if ((ec.flags & EntityContainerFlags::OnDelete_Error) != 0) {
						GAIA_ASSERT2(false, "Trying to delete an entity that is forbidden from being deleted");
						GAIA_LOG_E(
								"Trying to delete a pair [%u.%u] %s [%s] that is forbidden from being deleted", entity.id(),
								entity.gen(), name(entity), EntityKindString[entity.kind()]);
						return;
					}

					const auto tgt = try_get(entity.gen());
					const bool hasLiveTarget = tgt != EntityBad;
					if (hasLiveTarget) {
						const auto& ecTgt = fetch(tgt);
						if ((ecTgt.flags & EntityContainerFlags::OnDeleteTarget_Error) != 0 ||
								has_exclusive_adjunct_target_cond(tgt, Pair(OnDeleteTarget, Error))) {
							GAIA_ASSERT2(
									false, "Trying to delete an entity that is forbidden from being deleted (target restriction)");
							GAIA_LOG_E(
									"Trying to delete a pair [%u.%u] %s [%s] that is forbidden from being deleted (target restriction)",
									entity.id(), entity.gen(), name(entity), EntityKindString[entity.kind()]);
							return;
						}
					}

#if GAIA_USE_SAFE_ENTITY
					// Decrement the ref count at this point.
					if ((ec.flags & EntityContainerFlags::RefDecreased) == 0) {
						--ec.refCnt;
						ec.flags |= EntityContainerFlags::RefDecreased;
					}

					// Don't delete so long something still references us
					if (ec.refCnt != 0)
						return;
#endif

					if (hasLiveTarget) {
						const auto& ecTgt = fetch(tgt);
						if ((ecTgt.flags & EntityContainerFlags::OnDeleteTarget_Delete) != 0 ||
								has_exclusive_adjunct_target_cond(tgt, Pair(OnDeleteTarget, Delete))) {
#if GAIA_OBSERVERS_ENABLED
							cnt::darray<Entity> cascadeTargets;
							collect_delete_cascade_sources(tgt, Pair(OnDeleteTarget, Delete), cascadeTargets);
							auto cascadeDelDiffCtx =
									cascadeTargets.empty()
											? ObserverRegistry::DiffDispatchCtx{}
											: m_observers.prepare_diff(
														*this, ObserverEvent::OnDel, EntitySpan{cascadeTargets.data(), cascadeTargets.size()},
														EntitySpan{cascadeTargets.data(), cascadeTargets.size()});
#endif
							// Delete all entities referencing this one as a relationship pair's target
							req_del_entities_with(Pair(All, tgt), Pair(OnDeleteTarget, Delete));
#if GAIA_OBSERVERS_ENABLED
							m_observers.finish_diff(*this, GAIA_MOV(cascadeDelDiffCtx));
#endif
						} else {
							// Remove from all entities referencing this one as a relationship pair's target
							rem_from_entities(Pair(All, tgt));
						}
					}

					// This entity has been requested to be deleted already. Nothing more for us to do here
					if (is_req_del(ec))
						return;

#if GAIA_OBSERVERS_ENABLED
					observers().del(*this, entity);
#endif

					if ((ec.flags & EntityContainerFlags::OnDelete_Delete) != 0) {
						// Delete all references to the entity
						req_del_entities_with(entity);
					} else {
						// Entities are only removed by default
						rem_from_entities(entity);
					}
				} else {
					if ((ec.flags & EntityContainerFlags::OnDelete_Error) != 0) {
						GAIA_ASSERT2(false, "Trying to delete an entity that is forbidden from being deleted");
						GAIA_LOG_E(
								"Trying to delete an entity [%u.%u] %s [%s] that is forbidden from being deleted", entity.id(),
								entity.gen(), name(entity), EntityKindString[entity.kind()]);
						return;
					}

					if ((ec.flags & EntityContainerFlags::OnDeleteTarget_Error) != 0 ||
							has_exclusive_adjunct_target_cond(entity, Pair(OnDeleteTarget, Error))) {
						GAIA_ASSERT2(false, "Trying to delete an entity that is forbidden from being deleted (a pair's target)");
						GAIA_LOG_E(
								"Trying to delete an entity [%u.%u] %s [%s] that is forbidden from being deleted (a pair's target)",
								entity.id(), entity.gen(), name(entity), EntityKindString[entity.kind()]);
						return;
					}

#if GAIA_USE_SAFE_ENTITY
					// Decrement the ref count at this point.
					if ((ec.flags & EntityContainerFlags::RefDecreased) == 0) {
						--ec.refCnt;
						ec.flags |= EntityContainerFlags::RefDecreased;
					}

					// Don't delete so long something still references us
					if (ec.refCnt != 0)
						return;
#endif

					const bool deleteTargets = (ec.flags & EntityContainerFlags::OnDeleteTarget_Delete) != 0 ||
																		 has_exclusive_adjunct_target_cond(entity, Pair(OnDeleteTarget, Delete));
					cnt::darray<Entity> cascadeTargets;
					if (deleteTargets)
						collect_delete_cascade_sources(entity, Pair(OnDeleteTarget, Delete), cascadeTargets);
#if GAIA_OBSERVERS_ENABLED
					auto cascadeDelDiffCtx = ObserverRegistry::DiffDispatchCtx{};
					if (!cascadeTargets.empty()) {
						cascadeDelDiffCtx = m_observers.prepare_diff(
								*this, ObserverEvent::OnDel, EntitySpan{cascadeTargets.data(), cascadeTargets.size()},
								EntitySpan{cascadeTargets.data(), cascadeTargets.size()});
					}
#endif

					if (deleteTargets) {
						// Delete all entities referencing this one as a relationship pair's target
						req_del_entities_with(Pair(All, entity), Pair(OnDeleteTarget, Delete));
					} else {
						// Remove from all entities referencing this one as a relationship pair's target
						rem_from_entities(Pair(All, entity));
					}

#if GAIA_OBSERVERS_ENABLED
					m_observers.finish_diff(*this, GAIA_MOV(cascadeDelDiffCtx));
#endif

					// This entity is has been requested to be deleted already. Nothing more for us to do here
					if (is_req_del(ec))
						return;

#if GAIA_OBSERVERS_ENABLED
					observers().del(*this, entity);
#endif

					if ((ec.flags & EntityContainerFlags::OnDelete_Delete) != 0) {
						// Delete all references to the entity
						req_del_entities_with(entity);
					} else {
						// Entities are only removed by default
						rem_from_entities(entity);
					}
				}

				// Mark the entity with the "delete requested" flag
				req_del(ec, entity);

#if GAIA_USE_WEAK_ENTITY
				// Invalidate WeakEntities
				while (ec.pWeakTracker != nullptr) {
					auto* pTracker = ec.pWeakTracker;
					ec.pWeakTracker = pTracker->next;
					if (ec.pWeakTracker != nullptr)
						ec.pWeakTracker->prev = nullptr;

					auto* pWeakEntity = pTracker->pWeakEntity;
					GAIA_ASSERT(pWeakEntity != nullptr);
					GAIA_ASSERT(pWeakEntity->m_pTracker == pTracker);
					pWeakEntity->m_pTracker = nullptr;
					pWeakEntity->m_entity = EntityBad;
					delete pTracker;
				}
#endif
			}

			//! Removes a graph connection with the surrounding archetypes.
			//! \param pArchetype Archetype we are removing an edge from
			//! \param edgeLeft An edge pointing towards the archetype on the left
			//! \param edgeEntity An entity which when followed from the left edge we reach the archetype
			void remove_edge_from_archetype(Archetype* pArchetype, ArchetypeGraphEdge edgeLeft, Entity edgeEntity) {
				GAIA_ASSERT(pArchetype != nullptr);

				const auto edgeLeftIt = m_archetypesById.find(ArchetypeIdLookupKey(edgeLeft.id, edgeLeft.hash));
				if (edgeLeftIt == m_archetypesById.end())
					return;

				auto* pArchetypeLeft = edgeLeftIt->second;
				GAIA_ASSERT(pArchetypeLeft != nullptr);

				// Remove the connection with the current archetype
				pArchetypeLeft->del_graph_edges(pArchetype, edgeEntity);

				// Traverse all archetypes on the right
				auto& archetypesRight = pArchetype->right_edges();
				for (auto& it: archetypesRight) {
					const auto& edgeRight = it.second;
					const auto edgeRightIt = m_archetypesById.find(ArchetypeIdLookupKey(edgeRight.id, edgeRight.hash));
					if (edgeRightIt == m_archetypesById.end())
						continue;

					auto* pArchetypeRight = edgeRightIt->second;

					// Remove the connection with the current archetype
					pArchetype->del_graph_edges(pArchetypeRight, it.first.entity());
				}
			}

			void remove_edges(Entity entityToRemove) {
				const auto it = m_entityToArchetypeMap.find(EntityLookupKey(entityToRemove));
				if (it == m_entityToArchetypeMap.end())
					return;

				for (const auto& record: it->second) {
					auto* pArchetype = record.pArchetype;
					remove_edge_from_archetype(pArchetype, pArchetype->find_edge_left(entityToRemove), entityToRemove);
				}
			}

			void remove_edges_from_pairs(Entity entity) {
				if (entity.pair())
					return;

				// Make sure to remove all pairs containing the entity
				// (X, something)
				const auto* tgts = targets(entity);
				if (tgts != nullptr) {
					for (auto target: *tgts)
						remove_edges(Pair(entity, target.entity()));
				}
				// (something, X)
				const auto* rels = relations(entity);
				if (rels != nullptr) {
					for (auto relation: *rels)
						remove_edges(Pair(relation.entity(), entity));
				}
			}

			//! Deletes any edges containing the entity from the archetype graph.
			//! \param entity Entity to delete
			void del_graph_edges(Entity entity) {
				remove_edges(entity);
				remove_edges_from_pairs(entity);
			}

			void touch_rel_version(Entity relation) {
				const EntityLookupKey key(relation);
				auto it = m_relationVersions.find(key);
				if (it == m_relationVersions.end())
					m_relationVersions.emplace(key, 1);
				else {
					++it->second;
					if (it->second == 0)
						it->second = 1;
				}
			}

			void del_reltgt_tgtrel_pairs(Entity entity) {
				auto delPair = [](PairMap& map, Entity source, Entity remove) {
					auto itTargets = map.find(EntityLookupKey(source));
					if (itTargets != map.end()) {
						auto& targets = itTargets->second;
						targets.erase(EntityLookupKey(remove));
					}
				};

				Archetype* pArchetype = nullptr;

				if (entity.pair()) {
					const auto it = m_recs.pairs.find(EntityLookupKey(entity));
					if (it != m_recs.pairs.end()) {
						pArchetype = it->second.pArchetype;
						// Delete the container record
						m_recs.pairs.erase(it);

						// Update pairs
						// The relation or target entity may already be invalid at this point. Rebuild the
						// lookup keys from the stored entity records instead of calling get().
						GAIA_ASSERT(entity.id() < m_recs.entities.size());
						GAIA_ASSERT(entity.gen() < m_recs.entities.size());
						auto rel = m_recs.entities.handle(entity.id());
						auto tgt = m_recs.entities.handle(entity.gen());

						delPair(m_relToTgt, rel, tgt);
						delPair(m_relToTgt, All, tgt);
						delPair(m_tgtToRel, tgt, rel);
						delPair(m_tgtToRel, All, rel);
					}
				} else {
					// Update the container record
					auto ec = m_recs.entities[entity.id()];
					m_recs.entities.free(entity);

					// Remove all outgoing non-fragmenting sparse components from this entity.
					del_sparse_components(entity);
					// Remove all outgoing non-fragmenting exclusive relations from this source entity.
					del_exclusive_adjunct_source(entity);
					// If the deleted entity is itself a non-fragmenting exclusive relation, drop its store.
					del_exclusive_adjunct_relation(entity);
					// If the deleted entity is itself a non-fragmenting sparse component, drop its store.
					del_sparse_component_store(entity);

					// If this is a singleton entity its archetype needs to be deleted
					if ((ec.flags & EntityContainerFlags::IsSingleton) != 0)
						req_del(*ec.pArchetype);

					ec.pArchetype = nullptr;
					ec.pChunk = nullptr;
					ec.pEntity = nullptr;
					EntityBuilder::set_flag(ec.flags, EntityContainerFlags::DeleteRequested, false);

					// Update pairs
					delPair(m_relToTgt, All, entity);
					delPair(m_tgtToRel, All, entity);
					m_relToTgt.erase(EntityLookupKey(entity));
					m_tgtToRel.erase(EntityLookupKey(entity));
				}

				del_entity_archetype_pairs(entity, pArchetype);
			}

			//! Invalidates the entity record, effectively deleting it.
			//! \param entity Entity to delete
			void invalidate_entity(Entity entity) {
				del_graph_edges(entity);
				del_reltgt_tgtrel_pairs(entity);
			}

			//! Associates an entity with a chunk.
			//! \param entity Entity to associate with a chunk
			//! \param pArchetype Target archetype
			//! \param pChunk Target chunk (with the archetype \param pArchetype)
			void store_entity(EntityContainer& ec, Entity entity, Archetype* pArchetype, Chunk* pChunk) {
				GAIA_ASSERT(pArchetype != nullptr);
				GAIA_ASSERT(pChunk != nullptr);
				GAIA_ASSERT(
						!locked() && "Entities can't be stored while the world is locked "
												 "(structural changes are forbidden during this time!)");

				ec.pArchetype = pArchetype;
				ec.pChunk = pChunk;
				ec.row = pChunk->add_entity(entity);
				ec.pEntity = &pChunk->entity_view()[ec.row];
				GAIA_ASSERT(entity.pair() || ec.data.gen == entity.gen());
				ec.data.dis = 0;
			}

			//! Moves an entity along with all its generic components from its current chunk to another one.
			//! \param entity Entity to move
			//! \param ec Container containing the entity
			//! \param dstArchetype Destination archetype
			//! \param dstChunk Destination chunk
			void move_entity(Entity entity, EntityContainer& ec, Archetype& dstArchetype, Chunk& dstChunk) {
				GAIA_PROF_SCOPE(World::move_entity);

				auto* pDstChunk = &dstChunk;
				auto* pSrcChunk = ec.pChunk;

				GAIA_ASSERT(pDstChunk != pSrcChunk);

				const auto srcRow0 = ec.row;
				const auto dstRow = pDstChunk->add_entity(entity);
				const bool wasEnabled = !ec.data.dis;

				auto& srcArchetype = *ec.pArchetype;
				const bool archetypeChanged = srcArchetype.id() != dstArchetype.id();
#if GAIA_ASSERT_ENABLED
				verify_move(*this, srcArchetype, entity);
#endif

				// Make sure the old entity becomes enabled now
				srcArchetype.enable_entity(pSrcChunk, srcRow0, true, m_recs);
				// Enabling the entity might have changed its chunk index so fetch it again
				const auto srcRow = ec.row;

				// Move data from the old chunk to the new one
				if (dstArchetype.id() == srcArchetype.id()) {
					pDstChunk->move_entity_data(entity, dstRow, m_recs);
				} else {
					pDstChunk->move_foreign_entity_data(pSrcChunk, srcRow, pDstChunk, dstRow);
				}

				// Remove the entity record from the old chunk
				remove_entity(srcArchetype, *pSrcChunk, srcRow);

				// An entity might have moved, try updating the free chunk index
				dstArchetype.try_update_free_chunk_idx();

				// Bring the entity container record up-to-date
				ec.pArchetype = &dstArchetype;
				ec.pChunk = pDstChunk;
				ec.row = (uint16_t)dstRow;
				ec.pEntity = &pDstChunk->entity_view()[dstRow];
				if (archetypeChanged)
					update_src_entity_version(entity);

				// Make the enabled state in the new chunk match the original state
				dstArchetype.enable_entity(pDstChunk, dstRow, wasEnabled, m_recs);

				// End-state validation
				GAIA_ASSERT(valid(entity));
				validate_chunk(pSrcChunk);
				validate_chunk(pDstChunk);
				validate_entities();
			}

			//! Moves an entity along with all its generic components from its current chunk to another one in a new
			//! archetype. \param entity Entity to move \param dstArchetype Target archetype
			void move_entity_raw(Entity entity, EntityContainer& ec, Archetype& dstArchetype) {
				// Update the old chunk's world version first
				ec.pChunk->update_world_version();
				ec.pChunk->update_entity_order_version();

				auto* pDstChunk = dstArchetype.foc_free_chunk();
				move_entity(entity, ec, dstArchetype, *pDstChunk);

				// Update world versions
				pDstChunk->update_world_version();
				pDstChunk->update_entity_order_version();
				update_version(m_worldVersion);
			}

			//! Moves an entity along with all its generic components from its current chunk to another one in a new
			//! archetype. \param entity Entity to move \param dstArchetype Target archetype
			Chunk* move_entity(Entity entity, Archetype& dstArchetype) {
				// Archetypes need to be different
				auto& ec = fetch(entity);
				if (ec.pArchetype == &dstArchetype)
					return nullptr;

				// Update the old chunk's world version first
				ec.pChunk->update_world_version();
				ec.pChunk->update_entity_order_version();

				auto* pDstChunk = dstArchetype.foc_free_chunk();
				move_entity(entity, ec, dstArchetype, *pDstChunk);

				// Update world versions
				pDstChunk->update_world_version();
				pDstChunk->update_entity_order_version();
				update_version(m_worldVersion);

				return pDstChunk;
			}

			void validate_archetype_edges([[maybe_unused]] const Archetype* pArchetype) const {
#if GAIA_ECS_VALIDATE_ARCHETYPE_GRAPH && GAIA_ASSERT_ENABLED
				GAIA_ASSERT(pArchetype != nullptr);

				// Validate left edges
				const auto& archetypesLeft = pArchetype->left_edges();
				for (const auto& it: archetypesLeft) {
					const auto& edge = it.second;
					const auto edgeIt = m_archetypesById.find(ArchetypeIdLookupKey(edge.id, edge.hash));
					if (edgeIt == m_archetypesById.end())
						continue;

					const auto entity = it.first.entity();
					const auto* pArchetypeRight = edgeIt->second;

					// Edge must be found
					const auto edgeRight = pArchetypeRight->find_edge_right(entity);
					GAIA_ASSERT(edgeRight != ArchetypeIdHashPairBad);

					// The edge must point to pArchetype
					const auto it2 = m_archetypesById.find(ArchetypeIdLookupKey(edgeRight.id, edgeRight.hash));
					GAIA_ASSERT(it2 != m_archetypesById.end());
					const auto* pArchetype2 = it2->second;
					GAIA_ASSERT(pArchetype2 == pArchetype);
				}

				// Validate right edges
				const auto& archetypesRight = pArchetype->right_edges();
				for (const auto& it: archetypesRight) {
					const auto& edge = it.second;
					const auto edgeIt = m_archetypesById.find(ArchetypeIdLookupKey(edge.id, edge.hash));
					if (edgeIt == m_archetypesById.end())
						continue;

					const auto entity = it.first.entity();
					const auto* pArchetypeRight = edgeIt->second;

					// Edge must be found
					const auto edgeLeft = pArchetypeRight->find_edge_left(entity);
					GAIA_ASSERT(edgeLeft != ArchetypeIdHashPairBad);

					// The edge must point to pArchetype
					const auto it2 = m_archetypesById.find(ArchetypeIdLookupKey(edgeLeft.id, edgeLeft.hash));
					GAIA_ASSERT(it2 != m_archetypesById.end());
					const auto* pArchetype2 = it2->second;
					GAIA_ASSERT(pArchetype2 == pArchetype);
				}
#endif
			}

			//! Verifies than the implicit linked list of entities is valid
			void validate_entities() const {
#if GAIA_ECS_VALIDATE_ENTITY_LIST
				m_recs.entities.validate();
#endif
			}

			//! Verifies that the chunk is valid
			void validate_chunk([[maybe_unused]] Chunk* pChunk) const {
#if GAIA_ECS_VALIDATE_CHUNKS && GAIA_ASSERT_ENABLED
				GAIA_ASSERT(pChunk != nullptr);

				if (!pChunk->empty()) {
					// Make sure a proper amount of entities reference the chunk
					uint32_t cnt = 0;
					for (const auto& ec: m_recs.entities) {
						if (ec.pChunk != pChunk)
							continue;
						++cnt;
					}
					for (const auto& pair: m_recs.pairs) {
						if (pair.second.pChunk != pChunk)
							continue;
						++cnt;
					}
					GAIA_ASSERT(cnt == pChunk->size());
				} else {
					// Make sure no entities reference the chunk
					for (const auto& ec: m_recs.entities) {
						GAIA_ASSERT(ec.pChunk != pChunk);
					}
					for (const auto& pair: m_recs.pairs) {
						GAIA_ASSERT(pair.second.pChunk != pChunk);
					}
				}
#endif
			}

			//! If \tparam CheckIn is true, checks if \param entity is located in \param entityBase.
			//! If \tparam CheckIn is false, checks if \param entity inherits from \param entityBase.
			//! True if \param entity inherits from/is located in \param entityBase. False otherwise.
			template <bool CheckIn>
			GAIA_NODISCARD bool is_inter(Entity entity, Entity entityBase) const {
				GAIA_ASSERT(valid_entity(entity));
				GAIA_ASSERT(valid_entity(entityBase));

				// Pairs are not supported
				if (entity.pair() || entityBase.pair())
					return false;

				if constexpr (!CheckIn) {
					if (entity == entityBase)
						return true;
				}

				const auto& targets = as_targets_trav_cache(entity);
				for (auto target: targets) {
					if (target == entityBase)
						return true;
				}

				return false;
			}

			//! Traverse the (Is, X) relationships all the way to their source
			template <bool CheckIn, typename Func>
			void as_up_trav(Entity entity, Func func) {
				GAIA_ASSERT(valid_entity(entity));

				// Pairs are not supported
				if (entity.pair())
					return;

				if constexpr (!CheckIn) {
					func(entity);
				}

				const auto& ec = m_recs.entities[entity.id()];
				const auto* pArchetype = ec.pArchetype;

				// Early exit if there are no Is relationship pairs on the archetype
				if (pArchetype->pairs_is() == 0)
					return;

				for (uint32_t i = 0; i < pArchetype->pairs_is(); ++i) {
					auto e = pArchetype->entity_from_pairs_as_idx(i);
					const auto& ecTarget = m_recs.entities[e.gen()];
					auto target = *ecTarget.pEntity;
					func(target);

					as_up_trav<CheckIn>(target, func);
				}
			}

			template <typename T>
			const ComponentCacheItem& reg_core_entity(Entity id, Archetype* pArchetype) {
				auto comp = add(*pArchetype, id.entity(), id.pair(), id.kind());
				const auto& ci = comp_cache_mut().add<T>(id);
				GAIA_ASSERT(ci.entity == id);
				GAIA_ASSERT(comp == id);
				(void)comp;
				return ci;
			}

			template <typename T>
			const ComponentCacheItem& reg_core_entity(Entity id) {
				return reg_core_entity<T>(id, m_pRootArchetype);
			}

#if GAIA_ECS_AUTO_COMPONENT_SCHEMA
			template <typename T>
			static void auto_populate_component_schema(ComponentCacheItem& item) {
				if (!item.fields_empty())
					return;

				using U = core::raw_t<T>;
				if constexpr (std::is_empty_v<U>)
					return;
				if constexpr (mem::is_soa_layout_v<U>)
					return;

				if constexpr (!std::is_class_v<U>) {
					(void)item.set_field("value", 5, ser::type_id<U>(), 0, (uint32_t)sizeof(U));
					return;
				} else if constexpr (!std::is_aggregate_v<U>) {
					// Non-aggregate classes are not safe for offset extraction via meta::each_member.
					// Keep these components on serializer-based fallback rendering.
					return;
				} else {
					U tmp{};
					const auto* pBase = reinterpret_cast<const uint8_t*>(&tmp);
					uint32_t fieldIdx = 0;
					meta::each_member(tmp, [&](auto&... fields) {
						auto add_field = [&](auto& field) {
							using F = core::raw_t<decltype(field)>;
							char fieldName[24]{};
							(void)GAIA_STRFMT(fieldName, sizeof(fieldName), "f%u", fieldIdx++);
							const auto* pField = reinterpret_cast<const uint8_t*>(&field);
							const auto offset = (uint32_t)(pField - pBase);
							(void)item.set_field(fieldName, 0, ser::type_id<F>(), offset, (uint32_t)sizeof(F));
						};
						(add(fields), ...);
					});
				}
			}
#endif

			void init();

			void done() {
				cleanup_inter();

#if GAIA_ECS_CHUNK_ALLOCATOR
				ChunkAllocator::get().flush();
#endif
			}

			//! Assigns an entity to a given archetype.
			//! \param archetype Archetype the entity should inherit
			//! \param entity Entity
			void assign_entity(Entity entity, Archetype& archetype) {
				GAIA_ASSERT(!entity.pair());

				auto* pChunk = archetype.foc_free_chunk();
				store_entity(m_recs.entities[entity.id()], entity, &archetype, pChunk);
				pChunk->update_versions();
				archetype.try_update_free_chunk_idx();

				// Call constructors for the generic components on the newly added entity if necessary
				pChunk->call_gen_ctors(pChunk->size() - 1, 1);

#if GAIA_ASSERT_ENABLED
				const auto& ec = m_recs.entities[entity.id()];
				GAIA_ASSERT(ec.pChunk == pChunk);
				auto entityExpected = pChunk->entity_view()[ec.row];
				GAIA_ASSERT(entityExpected == entity);
#endif
			}

			//! Assigns a pair entity to a given archetype.
			//! \param archetype Archetype the pair should inherit
			//! \param entity Pair entity
			void assign_pair(Entity entity, Archetype& archetype) {
				GAIA_ASSERT(entity.pair());

				// Pairs are always added to m_pEntityArchetype initially and this can't change.
				GAIA_ASSERT(&archetype == m_pEntityArchetype);

				const auto it = m_recs.pairs.find(EntityLookupKey(entity));
				if (it != m_recs.pairs.end())
					return;

				// Update the container record
				EntityContainer ec{};
				ec.idx = entity.id();
				ec.data.gen = entity.gen();
				ec.data.pair = 1;
				ec.data.ent = 1;
				ec.data.kind = EntityKind::EK_Gen;

				auto* pChunk = archetype.foc_free_chunk();
				store_entity(ec, entity, &archetype, pChunk);
				pChunk->update_versions();
				archetype.try_update_free_chunk_idx();

				m_recs.pairs.emplace(EntityLookupKey(entity), GAIA_MOV(ec));

				// Update pair mappings
				const auto rel = get(entity.id());
				const auto tgt = get(entity.gen());

				auto addPair = [](PairMap& map, Entity source, Entity add) {
					auto& ents = map[EntityLookupKey(source)];
					ents.insert(EntityLookupKey(add));
				};

				addPair(m_relToTgt, rel, tgt);
				addPair(m_relToTgt, All, tgt);
				addPair(m_tgtToRel, tgt, rel);
				addPair(m_tgtToRel, All, rel);

#if GAIA_OBSERVERS_ENABLED
				m_observers.try_mark_term_observed(*this, entity);
#endif
			}

			//! Creates a new entity of a given archetype.
			//! \param archetype Archetype the entity should inherit.
			//! \param isEntity True if entity, false otherwise.
			//! \param isPair True if pair, false otherwise.
			//! \param kind Component kind.
			//! \return New entity.
			GAIA_NODISCARD Entity add(Archetype& archetype, bool isEntity, bool isPair, EntityKind kind) {
				EntityContainerCtx ctx{isEntity, isPair, kind};
				const auto entity = m_recs.entities.alloc(&ctx);
				assign_entity(entity, archetype);
				return entity;
			}

			//! Creates multiple entity of a given archetype at once.
			//! More efficient than creating entities individually.
			//! \param archetype Archetype the entity should inherit.
			//! \param count Number of entities to create.
			//! \param func void(Entity) functor executed for each added entity.
			template <typename Func>
			void add_entity_n(Archetype& archetype, uint32_t count, Func func) {
				EntityContainerCtx ctx{true, false, EntityKind::EK_Gen};
#if GAIA_OBSERVERS_ENABLED
				const auto addedIds = EntitySpan{archetype.ids_view()};
				ObserverRegistry::DiffDispatchCtx addDiffCtx{};
				if (!addedIds.empty())
					addDiffCtx = m_observers.prepare_diff_add_new(*this, addedIds);
#endif

				uint32_t left = count;
				do {
					auto* pChunk = archetype.foc_free_chunk();
					const uint32_t originalChunkSize = pChunk->size();
					const uint32_t freeSlotsInChunk = pChunk->capacity() - originalChunkSize;
					const uint32_t toCreate = core::get_min(freeSlotsInChunk, left);

					GAIA_FOR(toCreate) {
						const auto entityNew = m_recs.entities.alloc(&ctx);
						auto& ecNew = m_recs.entities[entityNew.id()];
						store_entity(ecNew, entityNew, &archetype, pChunk);

#if GAIA_ASSERT_ENABLED
						GAIA_ASSERT(ecNew.pChunk == pChunk);
						auto entityExpected = pChunk->entity_view()[ecNew.row];
						GAIA_ASSERT(entityExpected == entityNew);
#endif
					}

					// New entities were added, try updating the free chunk index
					archetype.try_update_free_chunk_idx();

					// Call constructors for the generic components on the newly added entity if necessary
					pChunk->call_gen_ctors(originalChunkSize, toCreate);

					// Call functors
					{
						auto entities = pChunk->entity_view();
						GAIA_FOR2(originalChunkSize, pChunk->size()) func(entities[i]);
					}

					pChunk->update_versions();

#if GAIA_OBSERVERS_ENABLED
					if (!addedIds.empty()) {
						auto entities = pChunk->entity_view();
						const auto targets = EntitySpan{entities.data() + originalChunkSize, toCreate};
						m_observers.append_diff_targets(*this, addDiffCtx, targets);
						m_observers.on_add(*this, archetype, addedIds, targets);
					}
#endif

					left -= toCreate;
				} while (left > 0);

#if GAIA_OBSERVERS_ENABLED
				if (!addedIds.empty())
					m_observers.finish_diff(*this, GAIA_MOV(addDiffCtx));
#endif
			}

			//! Garbage collection
			void gc() {
				GAIA_PROF_SCOPE(World::gc);

				del_empty_chunks();
				defrag_chunks(m_defragEntitiesPerTick);
				del_empty_archetypes();
			}

		public:
			QuerySerBuffer& query_buffer(QueryId& serId) {
				// No serialization id set on the query, try creating a new record
				if GAIA_UNLIKELY (serId == QueryIdBad) {
#if GAIA_ASSERT_ENABLED
					uint32_t safetyCounter = 0;
#endif

					while (true) {
#if GAIA_ASSERT_ENABLED
						// Make sure we don't cross some safety threshold
						++safetyCounter;
						GAIA_ASSERT(safetyCounter < 100000);
#endif

						serId = ++m_nextQuerySerId;
						// Make sure we do not overflow
						GAIA_ASSERT(serId != 0);

						// If the id is already found, try again.
						// Note, this is essentially never going to repeat. We would have to prepare millions if
						// not billions of queries for which we only added inputs but never queried them.
						auto ret = m_querySerMap.try_emplace(serId);
						if (!ret.second)
							continue;

						return ret.first->second;
					};
				}

				return m_querySerMap[serId];
			}

			void query_buffer_reset(QueryId& serId) {
				auto it = m_querySerMap.find(serId);
				if (it == m_querySerMap.end())
					return;

				m_querySerMap.erase(it);
				serId = QueryIdBad;
			}

			void invalidate_queries_for_structural_entity(EntityLookupKey entityKey) {
				m_queryCache.invalidate_queries_for_entity(entityKey, QueryCache::ChangeKind::Structural);
			}

			void invalidate_queries_for_rel(Entity relation) {
				m_queryCache.invalidate_queries_for_rel(relation, QueryCache::ChangeKind::DynamicResult);
			}

			void invalidate_sorted_queries_for_entity(Entity entity) {
				m_queryCache.invalidate_sorted_queries_for_entity(entity);
			}

			//! Invalidates all cached sorted queries after row-order changes.
			void invalidate_sorted_queries() {
				m_queryCache.invalidate_sorted_queries();
			}

			void invalidate_queries_for_entity(Pair is_pair) {
				GAIA_ASSERT(is_pair.first() == Is);

				// We still need to handle invalidation "down-the-tree".
				// E.g. following setup:
				// q = w.query().all({Is,animal});
				// w.as(wolf, carnivore);
				// w.as(carnivore, animal);
				// q.each() ...; // animal, carnivore, wolf
				// w.del(wolf, {Is,carnivore}) // wolf is no longer a carnivore and thus no longer an animal
				// After this deletion, we need to invalidate "q" because wolf is no longer an animal
				// and we don't want q to include it.
				// q.each() ...; // animal

				auto e = is_pair.second();
				as_up_trav<false>(e, [&](Entity target) {
					// Invalidate all queries that contain  everything in our path.
					invalidate_queries_for_structural_entity(EntityLookupKey(Pair{Is, target}));
				});
			}

			Entity name_to_entity(std::span<const char> exprRaw) const {
				auto expr = util::trim(exprRaw);

				if (expr[0] == '(') {
					if (expr.back() != ')') {
						GAIA_ASSERT2(false, "Expression '(' not terminated");
						return EntityBad;
					}

					const auto idStr = expr.subspan(1, expr.size() - 2);
					const auto commaIdx = core::get_index(idStr, ',');

					const auto first = name_to_entity(idStr.subspan(0, commaIdx));
					if (first == EntityBad)
						return EntityBad;
					const auto second = name_to_entity(idStr.subspan(commaIdx + 1));
					if (second == EntityBad)
						return EntityBad;

					return ecs::Pair(first, second);
				}

				{
					auto idStr = util::trim(expr);

					// Wildcard character
					if (idStr.size() == 1 && idStr[0] == '*')
						return All;

					return get_inter(idStr.data(), (uint32_t)idStr.size());
				}
			}

			Entity expr_to_entity(va_list& args, std::span<const char> exprRaw) const {
				auto expr = util::trim(exprRaw);

				if (expr[0] == '%') {
					if (expr[1] != 'e') {
						GAIA_ASSERT2(false, "Expression '%' not terminated");
						return EntityBad;
					}

					auto id = (Identifier)va_arg(args, unsigned long long);
					return Entity(id);
				}

				if (expr[0] == '(') {
					if (expr.back() != ')') {
						GAIA_ASSERT2(false, "Expression '(' not terminated");
						return EntityBad;
					}

					const auto idStr = expr.subspan(1, expr.size() - 2);
					const auto commaIdx = core::get_index(idStr, ',');

					const auto first = expr_to_entity(args, idStr.subspan(0, commaIdx));
					if (first == EntityBad)
						return EntityBad;
					const auto second = expr_to_entity(args, idStr.subspan(commaIdx + 1));
					if (second == EntityBad)
						return EntityBad;

					return ecs::Pair(first, second);
				}

				{
					auto idStr = util::trim(expr);

					// Wildcard character
					if (idStr.size() == 1 && idStr[0] == '*')
						return All;

					// Anything else is a component name
					const auto* pItem = resolve_component_name_inter(idStr.data(), (uint32_t)idStr.size());
					if (pItem == nullptr) {
						GAIA_ASSERT2(false, "Component not found");
						GAIA_LOG_W("Component '%.*s' not found", (uint32_t)idStr.size(), idStr.data());
						return EntityBad;
					}

					return pItem->entity;
				}
			}
		};

		using EntityBuilder = World::EntityBuilder;
	} // namespace ecs
} // namespace gaia

#include "api.inl"
#include "observer.inl"
#include "system.inl"

namespace gaia {
	namespace ecs {
		inline void World::init() {
			// Use the default serializer
			set_serializer(nullptr);

			// Register the root archetype
			{
				m_pRootArchetype = create_archetype({});
				m_pRootArchetype->set_hashes({calc_lookup_hash({})});
				reg_archetype(m_pRootArchetype);
			}

			(void)reg_core_entity<Core_>(Core);

			// Entity archetype matches the root archetype for now
			m_pEntityArchetype = m_pRootArchetype;

			// Register the component archetype (entity + EntityDesc + Component)
			{
				Archetype* pCompArchetype{};
				{
					const auto id = GAIA_ID(EntityDesc);
					const auto& ci = reg_core_entity<EntityDesc>(id);
					EntityBuilder(*this, id).add_inter_init(ci.entity);
					sset<EntityDesc>(id) = {ci.name.str(), ci.name.len(), nullptr, 0};
					pCompArchetype = m_recs.entities[id.id()].pArchetype;
				}
				{
					const auto id = GAIA_ID(Component);
					const auto& ci = reg_core_entity<Component>(id, pCompArchetype);
					EntityBuilder(*this, id).add_inter_init(ci.entity);
					acc_mut(id)
							// Entity descriptor
							.sset<EntityDesc>({ci.name.str(), ci.name.len(), nullptr, 0})
							// Component
							.sset<Component>(ci.comp);
					m_pCompArchetype = m_recs.entities[id.id()].pArchetype;
				}
			}

			// Core components.
			// Their order must correspond to the value sequence in id.h.
			{
				(void)reg_core_entity<OnDelete_>(OnDelete);
				(void)reg_core_entity<OnDeleteTarget_>(OnDeleteTarget);
				(void)reg_core_entity<Remove_>(Remove);
				(void)reg_core_entity<Delete_>(Delete);
				(void)reg_core_entity<Error_>(Error);
				(void)reg_core_entity<Requires_>(Requires);
				(void)reg_core_entity<CantCombine_>(CantCombine);
				(void)reg_core_entity<Exclusive_>(Exclusive);
				(void)reg_core_entity<DontFragment_>(DontFragment);
				(void)reg_core_entity<Acyclic_>(Acyclic);
				(void)reg_core_entity<Traversable_>(Traversable);
				(void)reg_core_entity<All_>(All);
				(void)reg_core_entity<ChildOf_>(ChildOf);
				(void)reg_core_entity<Parent_>(Parent);
				(void)reg_core_entity<Is_>(Is);
				(void)reg_core_entity<Prefab_>(Prefab);
				(void)reg_core_entity<OnInstantiate_>(OnInstantiate);
				(void)reg_core_entity<Override_>(Override);
				(void)reg_core_entity<Inherit_>(Inherit);
				(void)reg_core_entity<DontInherit_>(DontInherit);
				(void)reg_core_entity<System_>(System);
				(void)reg_core_entity<DependsOn_>(DependsOn);
				(void)reg_core_entity<Observer_>(Observer);

				(void)reg_core_entity<_Var0>(Var0);
				(void)reg_core_entity<_Var1>(Var1);
				(void)reg_core_entity<_Var2>(Var2);
				(void)reg_core_entity<_Var3>(Var3);
				(void)reg_core_entity<_Var4>(Var4);
				(void)reg_core_entity<_Var5>(Var5);
				(void)reg_core_entity<_Var6>(Var6);
				(void)reg_core_entity<_Var7>(Var7);
			}

			// Add special properties for core components.
			// Their order must correspond to the value sequence in id.h.
			{
				EntityBuilder(*this, Core) //
						.add(Core)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, GAIA_ID(EntityDesc)) //
						.add(Core)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, GAIA_ID(Component)) //
						.add(Core)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, OnDelete) //
						.add(Core)
						.add(Exclusive)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, OnDeleteTarget) //
						.add(Core)
						.add(Exclusive)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, Remove) //
						.add(Core)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, Delete) //
						.add(Core)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, Error) //
						.add(Core)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, All) //
						.add(Core)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, Requires) //
						.add(Core)
						.add(Acyclic)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, CantCombine) //
						.add(Core)
						.add(Acyclic)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, Exclusive) //
						.add(Core)
						.add(Pair(OnDelete, Error))
						.add(Acyclic);
				EntityBuilder(*this, DontFragment) //
						.add(Core)
						.add(Pair(OnDelete, Error))
						.add(Acyclic);
				EntityBuilder(*this, Acyclic) //
						.add(Core)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, Traversable) //
						.add(Core)
						.add(Pair(OnDelete, Error));

				EntityBuilder(*this, ChildOf) //
						.add(Core)
						.add(Acyclic)
						.add(Exclusive)
						.add(Traversable)
						.add(Pair(OnDelete, Error))
						.add(Pair(OnDeleteTarget, Delete));
				EntityBuilder(*this, Parent) //
						.add(Core)
						.add(Acyclic)
						.add(Exclusive)
						.add(DontFragment)
						.add(Traversable)
						.add(Pair(OnDelete, Error))
						.add(Pair(OnDeleteTarget, Delete));
				EntityBuilder(*this, Is) //
						.add(Core)
						.add(Acyclic)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, Prefab) //
						.add(Core)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, OnInstantiate) //
						.add(Core)
						.add(Acyclic)
						.add(Exclusive)
						.add(DontFragment)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, Override) //
						.add(Core)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, Inherit) //
						.add(Core)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, DontInherit) //
						.add(Core)
						.add(Pair(OnDelete, Error));

				EntityBuilder(*this, System) //
						.add(Core)
						.add(Acyclic)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, DependsOn) //
						.add(Core)
						.add(Acyclic)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, Observer) //
						.add(Core)
						.add(Acyclic)
						.add(Pair(OnDelete, Error));

				EntityBuilder(*this, Var0) //
						.add(Core)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, Var1) //
						.add(Core)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, Var2) //
						.add(Core)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, Var3) //
						.add(Core)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, Var4) //
						.add(Core)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, Var5) //
						.add(Core)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, Var6) //
						.add(Core)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, Var7) //
						.add(Core)
						.add(Pair(OnDelete, Error));
			}

			// Remove all archetypes with no chunks. We don't want any leftovers after
			// archetype movements.
			{
				for (uint32_t i = 1; i < m_archetypes.size(); ++i) {
					auto* pArchetype = m_archetypes[i];
					if (!pArchetype->chunks().empty())
						continue;

					// Request deletion the standard way.
					// We could simply add archetypes into m_archetypesToDel but this way
					// we can actually replicate what the system really does on the inside
					// and it will require more work at the cost of easier maintenance.
					// The amount of archetypes cleanup is very small after init and the code
					// only runs after the world is created so this is not a big deal.
					req_del(*pArchetype);
				}

				// Cleanup
				{
					del_finalize();
					while (!m_chunksToDel.empty() || !m_archetypesToDel.empty())
						gc();

					// Make sure everything has been cleared
					GAIA_ASSERT(m_reqArchetypesToDel.empty());
					GAIA_ASSERT(m_chunksToDel.empty());
					GAIA_ASSERT(m_archetypesToDel.empty());
				}

				sort_archetypes();

				// Make sure archetypes have valid graphs after the cleanup
				for (const auto* pArchetype: m_archetypes)
					validate_archetype_edges(pArchetype);
			}

			// Make sure archetype pointers are up-to-date
			m_pCompArchetype = m_recs.entities[GAIA_ID(Component).id()].pArchetype;

#if GAIA_SYSTEMS_ENABLED
			// Initialize the systems query
			systems_init();
#endif
		}

		inline GroupId
		group_by_func_default([[maybe_unused]] const World& world, const Archetype& archetype, Entity groupBy) {
			if (archetype.pairs() > 0) {
				auto ids = archetype.ids_view();
				for (auto id: ids) {
					if (!id.pair() || id.id() != groupBy.id())
						continue;

					// Consider the pair's target the groupId
					return id.gen();
				}
			}

			// No group
			return 0;
		}

		inline GroupId group_by_func_depth_order(const World& world, const Archetype& archetype, Entity relation) {
			GAIA_ASSERT(!relation.pair());

			// Depth ordering only makes sense for fragmenting relations whose target participates in archetype identity.
			// Non-fragmenting relations such as Parent must stay on walk(...), because their targets vary per entity
			// and cannot be represented by one cached archetype depth.
			// The level is derived from the cached upward traversal chain so normal query iteration can stay cheap.
			if (!world.supports_depth_order(relation) || archetype.pairs() == 0)
				return 0;

			auto ids = archetype.ids_view();
			GroupId maxDepth = 0;
			bool found = false;

			for (auto idsIdx: archetype.pair_rel_indices(relation)) {
				const auto pair = ids[idsIdx];
				const auto target = world.pair_target_if_alive(pair);
				if (target == EntityBad)
					continue;

				const GroupId depth = GroupId(world.depth_order_cache(relation, target));

				if (!found || depth > maxDepth) {
					maxDepth = depth;
					found = true;
				}
			}

			return found ? maxDepth : 0;
		}
	} // namespace ecs
} // namespace gaia

#include "gaia/ecs/impl/world_json.h"

#if GAIA_SYSTEMS_ENABLED
namespace gaia {
	namespace ecs {
		inline void World::systems_init() {
			m_systemsQuery = query().all(System).depth_order(DependsOn);
		}

		inline void World::systems_run() {
			if GAIA_UNLIKELY (tearing_down())
				return;

			m_systemsQuery.each([&](Entity systemEntity) {
				if (!valid(systemEntity) || !has(systemEntity, System))
					return;
				if (!enabled_hierarchy(systemEntity, ChildOf))
					return;

				auto ss = acc_mut(systemEntity);
				auto& sys = ss.smut<ecs::System_>();
				sys.exec();
			});
		}

		inline void World::systems_done() {
			cnt::darray<Entity> tmpEntities;
			m_systemsQuery.each([&](Entity systemEntity) {
				tmpEntities.push_back(systemEntity);
			});

			// Wait for every outstanding system job before mutating any system runtime state.
			// This keeps dependency chains intact while jobs are still live.
			for (auto entity: tmpEntities) {
				if (!valid(entity) || !has(entity, System))
					continue;

				auto ss = acc_mut(entity);
				auto& sys = ss.smut<ecs::System_>();
				if (sys.jobHandle != (mt::JobHandle)mt::JobNull_t{}) {
					auto& tp = mt::ThreadPool::get();
					tp.wait(sys.jobHandle);
				}
			}

			// With all system jobs complete we can release their runtime state safely.
			for (auto entity: tmpEntities) {
				if (!valid(entity) || !has(entity, System))
					continue;

				auto ss = acc_mut(entity);
				auto& sys = ss.smut<ecs::System_>();
				if (sys.jobHandle != (mt::JobHandle)mt::JobNull_t{}) {
					auto& tp = mt::ThreadPool::get();
					tp.del(sys.jobHandle);
					sys.jobHandle = mt::JobNull;
				}
				sys.on_each_func = {};
				sys.query = {};
			}

			m_systemsQuery = {};
			tmpEntities.clear();
		}

		inline SystemBuilder World::system() {
			// Create the system
			auto e = add();
			EntityBuilder(*this, e) //
					.add<System_>();

			auto ss = acc_mut(e);
			auto& sys = ss.smut<System_>();
			{
				sys.entity = e;
				sys.query = query<true>();
			}
			return SystemBuilder(*this, e);
		}
	} // namespace ecs
} // namespace gaia
#endif

namespace gaia {
	namespace ecs {
		inline uint32_t world_version(const World& world) {
			return world.m_worldVersion;
		}

		inline QueryMatchScratch& query_match_scratch_acquire(World& world) {
			if (world.m_queryMatchScratchDepth == world.m_queryMatchScratchStack.size())
				world.m_queryMatchScratchStack.push_back(new QueryMatchScratch());

			auto& scratch = *world.m_queryMatchScratchStack[world.m_queryMatchScratchDepth++];
			scratch.clear_temporary_matches();
			return scratch;
		}

		inline void query_match_scratch_release(World& world, bool keepStamps) {
			GAIA_ASSERT(world.m_queryMatchScratchDepth > 0);
			auto& scratch = *world.m_queryMatchScratchStack[--world.m_queryMatchScratchDepth];
			if (keepStamps)
				scratch.clear_temporary_matches_keep_stamps();
			else
				scratch.clear_temporary_matches();
		}

		inline void world_invalidate_sorted_queries_for_entity(World& world, Entity entity) {
			world.invalidate_sorted_queries_for_entity(entity);
		}

		inline void world_invalidate_sorted_queries(World& world) {
			world.invalidate_sorted_queries();
		}

		inline bool world_has_entity_term(const World& world, Entity entity, Entity term) {
			if (term.pair() && term.id() == Is.id() && !is_wildcard(term.gen())) {
				const auto target = world.get(term.gen());
				return world.valid(target) && world.is(entity, target);
			}

			return world.has(entity, term);
		}

		inline bool world_has_entity_term_in(const World& world, Entity entity, Entity term) {
			if (term.pair() && term.id() == Is.id() && !is_wildcard(term.gen())) {
				const auto target = world.get(term.gen());
				return world.valid(target) && world.in(entity, target);
			}

			return false;
		}

		inline bool world_term_uses_inherit_policy(const World& world, Entity term) {
			return !is_wildcard(term) && world.valid(term) && world.target(term, OnInstantiate) == Inherit;
		}

		inline bool world_has_entity_term_direct(const World& world, Entity entity, Entity term) {
			return world.has_direct(entity, term);
		}

		inline bool world_is_exclusive_dont_fragment_relation(const World& world, Entity relation) {
			return world.is_exclusive_dont_fragment_relation(relation);
		}

		inline bool world_is_out_of_line_component(const World& world, Entity component) {
			return world.is_out_of_line_component(component);
		}

		inline bool world_is_non_fragmenting_out_of_line_component(const World& world, Entity component) {
			return world.is_non_fragmenting_out_of_line_component(component);
		}

		inline uint32_t world_count_direct_term_entities(const World& world, Entity term) {
			return world.count_direct_term_entities(term);
		}

		inline uint32_t world_count_in_term_entities(const World& world, Entity term) {
			if (!(term.pair() && term.id() == Is.id() && !is_wildcard(term.gen())))
				return 0;

			const auto target = world.get(term.gen());
			return world.valid(target) ? (uint32_t)world.as_relations_trav_cache(target).size() : 0U;
		}

		inline uint32_t world_count_direct_term_entities_direct(const World& world, Entity term) {
			return world.count_direct_term_entities_direct(term);
		}

		inline void world_collect_direct_term_entities(const World& world, Entity term, cnt::darray<Entity>& out) {
			world.collect_direct_term_entities(term, out);
		}

		inline void world_collect_in_term_entities(const World& world, Entity term, cnt::darray<Entity>& out) {
			if (!(term.pair() && term.id() == Is.id() && !is_wildcard(term.gen())))
				return;

			const auto target = world.get(term.gen());
			if (!world.valid(target))
				return;

			const auto& relations = world.as_relations_trav_cache(target);
			out.reserve(out.size() + (uint32_t)relations.size());
			for (auto relation: relations)
				out.push_back(relation);
		}

		inline void world_collect_direct_term_entities_direct(const World& world, Entity term, cnt::darray<Entity>& out) {
			world.collect_direct_term_entities_direct(term, out);
		}

		inline bool
		world_for_each_direct_term_entity(const World& world, Entity term, void* ctx, bool (*func)(void*, Entity)) {
			return world.for_each_direct_term_entity(term, ctx, func);
		}

		inline bool world_for_each_in_term_entity(const World& world, Entity term, void* ctx, bool (*func)(void*, Entity)) {
			if (!(term.pair() && term.id() == Is.id() && !is_wildcard(term.gen())))
				return true;

			const auto target = world.get(term.gen());
			if (!world.valid(target))
				return true;

			for (auto relation: world.as_relations_trav_cache(target)) {
				if (!func(ctx, relation))
					return false;
			}

			return true;
		}

		inline bool
		world_for_each_direct_term_entity_direct(const World& world, Entity term, void* ctx, bool (*func)(void*, Entity)) {
			return world.for_each_direct_term_entity_direct(term, ctx, func);
		}

		inline bool world_entity_enabled(const World& world, Entity entity) {
			return world.enabled(entity);
		}

		inline Entity world_pair_target_if_alive(const World& world, Entity pair) {
			return world.pair_target_if_alive(pair);
		}

		inline bool world_entity_enabled_hierarchy(const World& world, Entity entity, Entity relation) {
			return world.enabled_hierarchy(entity, relation);
		}

		inline uint32_t world_enabled_hierarchy_version(const World& world) {
			return world.enabled_hierarchy_version();
		}

		inline bool world_is_hierarchy_relation(const World& world, Entity relation) {
			return world.is_hierarchy_relation(relation);
		}

		inline bool world_is_fragmenting_relation(const World& world, Entity relation) {
			return world.is_fragmenting_relation(relation);
		}

		inline bool world_is_fragmenting_hierarchy_relation(const World& world, Entity relation) {
			return world.is_fragmenting_hierarchy_relation(relation);
		}

		inline bool world_supports_depth_order(const World& world, Entity relation) {
			return world.supports_depth_order(relation);
		}

		inline bool world_depth_order_prunes_disabled_subtrees(const World& world, Entity relation) {
			return world.depth_order_prunes_disabled_subtrees(relation);
		}

		inline bool world_entity_prefab(const World& world, Entity entity) {
			const auto& ec = world.fetch(entity);
			return ec.pArchetype != nullptr && ec.pArchetype->has(Prefab);
		}

		inline const Archetype* world_entity_archetype(const World& world, Entity entity) {
			return world.fetch(entity).pArchetype;
		}

		inline uint32_t world_component_index_bucket_size(const World& world, Entity term) {
			const auto it = world.m_entityToArchetypeMap.find(EntityLookupKey(term));
			if (it == world.m_entityToArchetypeMap.end())
				return 0;

			return (uint32_t)it->second.size();
		}

		inline uint32_t world_component_index_comp_idx(const World& world, const Archetype& archetype, Entity term) {
			if (is_wildcard(term))
				return BadIndex;

			const auto it = world.m_entityToArchetypeMap.find(EntityLookupKey(term));
			if (it == world.m_entityToArchetypeMap.end())
				return BadIndex;

			const auto idx = core::get_index_if(it->second, [&](const auto& entry) {
				return entry.matches(&archetype);
			});
			if (idx == BadIndex)
				return BadIndex;

			return it->second[idx].compIdx;
		}

		inline uint32_t world_component_index_match_count(const World& world, const Archetype& archetype, Entity term) {
			const auto it = world.m_entityToArchetypeMap.find(EntityLookupKey(term));
			if (it == world.m_entityToArchetypeMap.end())
				return 0;

			const auto idx = core::get_index_if(it->second, [&](const auto& entry) {
				return entry.matches(&archetype);
			});
			if (idx == BadIndex)
				return 0;

			return it->second[idx].matchCount;
		}

		template <typename T>
		inline decltype(auto) world_direct_entity_arg(World& world, Entity entity) {
			using Arg = std::remove_cv_t<std::remove_reference_t<T>>;
			if constexpr (std::is_same_v<Arg, Entity>)
				return entity;
			else if constexpr (std::is_lvalue_reference_v<T> && !std::is_const_v<std::remove_reference_t<T>>)
				return world.template set<Arg>(entity);
			else
				return world.template get<Arg>(entity);
		}

		template <typename T>
		inline Entity world_query_arg_id(World& world) {
			using Arg = std::remove_cv_t<std::remove_reference_t<T>>;
			using FT = typename component_type_t<Arg>::TypeFull;
			if constexpr (is_pair<FT>::value) {
				const auto rel = comp_cache(world).template get<typename FT::rel>().entity;
				const auto tgt = comp_cache(world).template get<typename FT::tgt>().entity;
				return (Entity)Pair(rel, tgt);
			} else
				return comp_cache(world).template get<FT>().entity;
		}

		template <typename T>
		inline decltype(auto) world_query_entity_arg(World& world, Entity entity) {
			using Arg = std::remove_cv_t<std::remove_reference_t<T>>;
			if constexpr (std::is_same_v<Arg, Entity>)
				return entity;
			else {
				const auto id = world_query_arg_id<Arg>(world);
				return world_query_entity_arg_by_id<T>(world, entity, id);
			}
		}

		template <typename T>
		inline decltype(auto) world_query_entity_arg_by_id(World& world, Entity entity, Entity id) {
			using Arg = std::remove_cv_t<std::remove_reference_t<T>>;
			if constexpr (std::is_same_v<Arg, Entity>)
				return entity;
			const auto termId = id != EntityBad ? id : world_query_arg_id<Arg>(world);
			if constexpr (std::is_lvalue_reference_v<T> && !std::is_const_v<std::remove_reference_t<T>>) {
				if (!world.has_direct(entity, termId) && world.has(entity, termId)) {
					if constexpr (is_pair<Arg>::value)
						(void)world.override(entity, termId);
					else
						(void)world.template override<Arg>(entity, termId);
				}

				return world.template set<Arg>(entity, termId);
			} else
				return world.template get<Arg>(entity, termId);
		}

	} // namespace ecs
} // namespace gaia

#if GAIA_OBSERVERS_ENABLED
namespace gaia {
	namespace ecs {
		inline uint32_t world_rel_version(const World& world, Entity relation) {
			return world.rel_version(relation);
		}

		//! Returns the sparse archetype-membership version tracked for a source entity.
		//! Invalid source entities report version 0 so cached queries see deletions immediately.
		//! Valid entries are created on first use so non-source entities do not pay any extra storage.
		inline uint32_t world_entity_archetype_version(const World& world, Entity entity) {
			if (!world.valid(entity))
				return 0;

			const auto key = EntityLookupKey(entity);
			auto it = world.m_srcEntityVersions.find(key);
			if (it != world.m_srcEntityVersions.end())
				return it->second;

			it = world.m_srcEntityVersions.try_emplace(key, 1).first;
			return it->second;
		}

		inline ObserverBuilder World::observer() {
			// Create the observer
			auto e = add();
			EntityBuilder(*this, e) //
					.add<Observer_>();

			auto ss = acc_mut(e);
			auto& hdr = ss.smut<Observer_>();
			auto& obs = observers().data_add(e);
			{
				hdr.entity = e;
				obs.entity = e;
				obs.query = query<true>();
			}
			return ObserverBuilder(*this, e);
		}
	} // namespace ecs
} // namespace gaia
#endif
