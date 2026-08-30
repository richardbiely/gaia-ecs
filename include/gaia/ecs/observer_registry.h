#pragma once
#include "gaia/config/config.h"

#include "gaia/cnt/darray.h"
#include "gaia/cnt/ilist.h"
#include "gaia/cnt/map.h"
#include "gaia/cnt/set.h"
#include "gaia/ecs/observer.h"

#if GAIA_OBSERVERS_ENABLED
namespace gaia {
	namespace ecs {
		class World;
		class Archetype;

		//! Runtime storage for observer callbacks and dispatch indexes kept outside ECS component storage.
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
				//! \return True when every dependency index and fallback list is empty.
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

				//! Combines every field that identifies a propagated target cache entry.
				//! \return Hash used by the propagated target cache.
				GAIA_NODISCARD size_t hash() const {
					size_t seed = EntityLookupKey(bindingRelation).hash();
					seed ^= EntityLookupKey(traversalRelation).hash() + 0x9e3779b9u + (seed << 6u) + (seed >> 2u);
					seed ^= EntityLookupKey(rootTarget).hash() + 0x9e3779b9u + (seed << 6u) + (seed >> 2u);
					seed ^= size_t(travKind) + 0x9e3779b9u + (seed << 6u) + (seed >> 2u);
					seed ^= size_t(travDepth) + 0x9e3779b9u + (seed << 6u) + (seed >> 2u);
					return seed;
				}

				//! Compares two propagated target cache keys.
				//! \param other Key to compare with this key.
				//! \return True when both keys describe the same propagation request.
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
					Entity observer = EntityBad;
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
					//! Dispatches the captured pre-mutation matches as explicit removal events.
					bool targetsRemovedAfterPrepare = false;
					bool resetTraversalCaches = false;
				};

				//! Collects every enabled entity that currently matches an observer query.
				//! The result is sorted so it can be compared with a later query result.
				//! \param world World that owns the observer and matching entities.
				//! \param obs Observer whose query is evaluated.
				//! \param out Receives the sorted matching entities.
				static void collect_query_matches(World& world, ObserverRuntimeData& obs, cnt::darray<Entity>& out);

				//! Collects matching entities from a known set of possible targets.
				//! \param world World that owns the observer and target entities.
				//! \param obs Observer whose query is evaluated.
				//! \param targets Entities that may have changed their query result.
				//! \param out Receives the matching target entities.
				static void collect_query_target_matches(
						World& world, ObserverRuntimeData& obs, EntitySpan targets, cnt::darray<Entity>& out);

				//! Appends targets that still refer to valid entities in the world.
				//! \param world World used to validate the entities.
				//! \param out Destination list. Existing entries are preserved.
				//! \param targets Candidate entities to append.
				static void add_valid_targets(World& world, cnt::darray<Entity>& out, EntitySpan targets);

				//! Stores the part of an observer plan that determines target propagation.
				//! \param obs Observer that supplies the plan.
				//! \param entry Cache entry that receives the plan description.
				static void copy_target_narrow_plan(const ObserverRuntimeData& obs, TargetNarrowCacheEntry& entry);

				//! Checks whether an observer can reuse a cached target propagation result.
				//! \param obs Observer being considered.
				//! \param entry Previously evaluated propagation plan.
				//! \return True when both plans produce the same possible targets.
				GAIA_NODISCARD static bool
				same_target_narrow_plan(const ObserverRuntimeData& obs, const TargetNarrowCacheEntry& entry);

				//! Sorts targets by entity value and removes duplicate entries.
				//! \param targets Target list to normalize in place.
				static void normalize_targets(cnt::darray<Entity>& targets);

				//! Returns the lookup hash of an observer query.
				//! \param obs Observer whose query supplies the hash.
				//! \return Hash used to find queries that may share a match result.
				GAIA_NODISCARD static uint64_t query_hash(ObserverRuntimeData& obs);

				//! Compares the query state that can affect observer matching.
				//! \param left First query state.
				//! \param right Second query state.
				//! \return True when both queries have the same matching behavior.
				GAIA_NODISCARD static bool same_query_ctx(const QueryCtx& left, const QueryCtx& right);

				//! Finds a cached match result that is safe for an observer to reuse.
				//! \param cache Match results already collected during this dispatch.
				//! \param obs Observer looking for an equivalent query result.
				//! \return Cache index, or minus one when no equivalent query was found.
				GAIA_NODISCARD static int32_t
				find_match_cache_entry(cnt::darray<MatchCacheEntry>& cache, ObserverRuntimeData& obs);

				//! Captures observer query matches before a world mutation.
				//! \param registry Registry that owns the observer indexes.
				//! \param world World about to be changed.
				//! \param event Event produced by the mutation.
				//! \param terms Terms changed by the mutation.
				//! \param targetEntities Entities directly changed by the mutation, when known.
				//! \return Context containing the relevant observers and their matches before the mutation.
				GAIA_NODISCARD static Context prepare(
						ObserverRegistry& registry, World& world, ObserverEvent event, EntitySpan terms,
						EntitySpan targetEntities = {});

				//! Prepares an add event for entities that do not exist yet.
				//! \param registry Registry that owns the observer indexes.
				//! \param world World in which the entities will be created.
				//! \param terms Terms the new entities will receive.
				//! \return Context ready to receive the created entities after the mutation.
				GAIA_NODISCARD static Context prepare_add_new(ObserverRegistry& registry, World& world, EntitySpan terms);

				//! Adds entities created after prepare_add_new completed.
				//! \param world World used to reject entities that are no longer valid.
				//! \param ctx Dispatch context receiving the entities.
				//! \param targets Newly created entities.
				static void add_targets(World& world, Context& ctx, EntitySpan targets);

				//! Compares query matches after a mutation and runs callbacks for changed matches.
				//! \param world World after the mutation has completed.
				//! \param ctx Context captured before the mutation.
				static void finish(World& world, Context&& ctx);
			};

			struct DirectDispatcher {
				//! Dispatches OnAdd observers that can be checked against the changed archetype.
				//! \param registry Registry that owns the direct observer indexes.
				//! \param world World containing the changed entities.
				//! \param archetype Archetype after the terms were added.
				//! \param entsAdded Terms added by the mutation.
				//! \param targets Entities that received the terms.
				static void on_add(
						ObserverRegistry& registry, World& world, const Archetype& archetype, EntitySpan entsAdded,
						EntitySpan targets);

				//! Dispatches OnDel observers that can be checked against the previous archetype.
				//! \param registry Registry that owns the direct observer indexes.
				//! \param world World containing the changed entities.
				//! \param archetype Archetype before the terms were removed.
				//! \param entsRemoved Terms removed by the mutation.
				//! \param targets Entities that lost the terms.
				static void on_del(
						ObserverRegistry& registry, World& world, const Archetype& archetype, EntitySpan entsRemoved,
						EntitySpan targets);

				//! Dispatches OnSet observers for entities whose component value was written.
				//! \param registry Registry that owns the OnSet observer index.
				//! \param world World containing the written component.
				//! \param term Exact component or pair that was written.
				//! \param targets Entities whose value was written.
				static void on_set(ObserverRegistry& registry, World& world, Entity term, EntitySpan targets);
			};

			struct SharedDispatch {
				//! Appends enabled observers registered under one lookup term.
				//! \tparam DiffOnly When true, observers using direct dispatch are ignored.
				//! \tparam TObserverMap Observer map type used by the selected index.
				//! \tparam TObserverList Candidate observer list type.
				//! \param registry Registry receiving the matching runtime records.
				//! \param world World used to check whether observer entities are enabled.
				//! \param map Index to search.
				//! \param term Lookup term.
				//! \param matchStamp Stamp used to avoid appending an observer more than once.
				//! \param out Receives runtime records that pass the index and enabled-state checks.
				template <bool DiffOnly, typename TObserverMap, typename TObserverList>
				static void collect_from_map(
						ObserverRegistry& registry, World& world, const TObserverMap& map, Entity term, uint64_t matchStamp,
						TObserverList& out);

				//! Appends enabled diff observers from a broad fallback list.
				//! \tparam TObserverList Candidate observer list type.
				//! \param registry Registry receiving the matching runtime records.
				//! \param world World used to check whether observer entities are enabled.
				//! \param observers Observer entities to inspect.
				//! \param matchStamp Stamp used to avoid appending an observer more than once.
				//! \param out Receives enabled diff observer runtime records.
				template <typename TObserverList>
				static void collect_diff_from_list(
						ObserverRegistry& registry, World& world, const cnt::darray<Entity>& observers, uint64_t matchStamp,
						TObserverList& out);

				//! Checks whether any changed term is present in an observer index.
				//! \tparam TObserverMap Observer map type used by the selected index.
				//! \param map Term index to search.
				//! \param terms Changed terms.
				//! \return True when at least one changed term has registered observers.
				template <typename TObserverMap>
				GAIA_NODISCARD static bool has_terms(const TObserverMap& map, EntitySpan terms) {
					for (auto term: terms) {
						const auto it = map.find(EntityLookupKey(term));
						if (it != map.end() && !it->second.empty())
							return true;
					}

					return false;
				}

				//! Checks whether any changed pair uses a relation present in an observer index.
				//! \tparam TObserverMap Observer map type used by the selected index.
				//! \param world World used to resolve pair relations.
				//! \param map Relation index to search.
				//! \param terms Changed terms.
				//! \return True when at least one changed relation has registered observers.
				template <typename TObserverMap>
				GAIA_NODISCARD static bool has_pair_relations(World& world, const TObserverMap& map, EntitySpan terms);

				//! Collects observers for an exact changed term that is known to be observed.
				//! \tparam TObserverMap Observer map type used by the selected event.
				//! \tparam TObserverList Candidate observer list type.
				//! \param registry Registry receiving the matching runtime records.
				//! \param world World that owns the term.
				//! \param map Event index to search.
				//! \param term Changed term.
				//! \param matchStamp Stamp used to avoid duplicate candidates.
				//! \param out Receives matching observer runtime records.
				template <typename TObserverMap, typename TObserverList>
				static void collect_for_event_term(
						ObserverRegistry& registry, World& world, const TObserverMap& map, Entity term, uint64_t matchStamp,
						TObserverList& out);

				//! Collects observers registered for a semantic Is target.
				//! \tparam TObserverMap Observer map type used by the selected event.
				//! \tparam TObserverList Candidate observer list type.
				//! \param registry Registry receiving the matching runtime records.
				//! \param world World that owns the observer entities.
				//! \param map Semantic Is target index to search.
				//! \param target Is target used as the lookup key.
				//! \param matchStamp Stamp used to avoid duplicate candidates.
				//! \param out Receives matching observer runtime records.
				template <typename TObserverMap, typename TObserverList>
				static void collect_for_is_target(
						ObserverRegistry& registry, World& world, const TObserverMap& map, Entity target, uint64_t matchStamp,
						TObserverList& out);

				//! Visits inheritable component terms from a base entity and all of its bases.
				//! \tparam Func Callable invoked for every inheritable component term found.
				//! \param world World containing the inheritance graph.
				//! \param baseEntity First base entity to inspect.
				//! \param func Callable that receives each inheritable component term.
				template <typename Func>
				static void for_each_inherited_term(World& world, Entity baseEntity, Func&& func);

				//! Checks whether changed semantic Is pairs can reach an indexed base target.
				//! \tparam TObserverMap Observer map type used by the selected event.
				//! \param world World containing the inheritance graph.
				//! \param map Semantic Is target index to search.
				//! \param terms Changed terms.
				//! \return True when an exact or inherited base target is indexed.
				template <typename TObserverMap>
				GAIA_NODISCARD static bool has_semantic_is_terms(World& world, const TObserverMap& map, EntitySpan terms);

				//! Checks whether changed semantic Is pairs expose an indexed inherited component.
				//! \tparam TObserverMap Observer map type used by the selected event.
				//! \param world World containing the inheritance graph.
				//! \param map Component term index to search.
				//! \param terms Changed terms.
				//! \return True when a reachable base provides an observed inheritable component.
				template <typename TObserverMap>
				GAIA_NODISCARD static bool has_inherited_terms(World& world, const TObserverMap& map, EntitySpan terms);

				//! Collects observers for inheritable component terms supplied by a base entity.
				//! \tparam TObserverMap Observer map type used by the selected event.
				//! \tparam TObserverList Candidate observer list type.
				//! \param registry Registry receiving the matching runtime records.
				//! \param world World containing the inheritance graph.
				//! \param map Component term index to search.
				//! \param baseEntity First base entity to inspect.
				//! \param matchStamp Stamp used to avoid duplicate candidates.
				//! \param out Receives matching observer runtime records.
				template <typename TObserverMap, typename TObserverList>
				static void collect_for_inherited_terms(
						ObserverRegistry& registry, World& world, const TObserverMap& map, Entity baseEntity, uint64_t matchStamp,
						TObserverList& out);

				//! Runs one observer callback for a span of matching entities.
				//! \param world World supplied to the observer iterator.
				//! \param obs Observer callback and runtime state.
				//! \param targets Entities supplied to the callback.
				static void execute_targets(World& world, ObserverRuntimeData& obs, EntitySpan targets);

				//! Checks whether a direct observer plan accepts the changed entities.
				//! \param obs Observer whose execution plan is evaluated.
				//! \param archetype Archetype used for the query match.
				//! \param targets Changed entities within the archetype.
				//! \param pQueryInfo Optional query state already fetched by the caller.
				//! \return True when the observer should run for the targets.
				GAIA_NODISCARD static bool matches_direct_targets(
						ObserverRuntimeData& obs, const Archetype& archetype, EntitySpan targets, QueryInfo* pQueryInfo = nullptr);
			};

			using DiffDispatchCtx = DiffDispatcher::Context;

		private:
			//! Stable paged slot containing one observer runtime record.
			struct ObserverRuntimeSlot {
				uint32_t idx = 0;
				uint32_t gen = 0;
				ObserverRuntimeData runtime;

				//! Constructs a slot with the index and generation assigned by the paged list.
				//! \param index Slot index.
				//! \param generation Slot generation.
				ObserverRuntimeSlot(uint32_t index, uint32_t generation): idx(index), gen(generation) {}

				//! Creates a slot for paged-list allocation.
				//! \param index Slot index.
				//! \param generation Slot generation.
				//! \param pContext Unused allocation context.
				//! \return Newly initialized runtime slot.
				GAIA_NODISCARD static ObserverRuntimeSlot create(
						uint32_t index, uint32_t generation, [[maybe_unused]] void* pContext) {
					return ObserverRuntimeSlot(index, generation);
				}

				//! Returns the internal handle identifying a runtime slot.
				//! \param slot Runtime slot whose handle is requested.
				//! \return Internal generational slot handle.
				GAIA_NODISCARD static Entity handle(const ObserverRuntimeSlot& slot) {
					return Entity(slot.idx, slot.gen, false, false, EntityKind::EK_Gen);
				}
			};

			using ObserverRuntimeSlots = cnt::paged_ilist<ObserverRuntimeSlot, Entity>;

			//! Reusable candidate storage for the outermost observer dispatch.
			cnt::darray<ObserverRuntimeData*> m_candidate_observers;
			//! Reusable candidate storage allocated only for nested observer dispatch levels.
			cnt::darray<cnt::darray<ObserverRuntimeData*>*> m_nested_candidate_observers;
			//! Number of candidate buffers currently borrowed by nested dispatches.
			uint32_t m_candidate_depth = 0;
			//! Stable runtime observer slots addressed by internal generational handles.
			ObserverRuntimeSlots m_observer_runtime_slots;
			//! Observer entity to stable runtime-slot handle mapping.
			cnt::map<EntityLookupKey, Entity> m_observer_data;
			//! Runtime slots removed by a callback and released after the outermost dispatch returns.
			cnt::darray<Entity> m_retired_observer_data;
			//! Number of active, possibly nested observer dispatches.
			uint32_t m_dispatch_depth = 0;
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

			//! Borrows reusable candidate storage for one possibly nested dispatch pass.
			class CandidateScope final {
				ObserverRegistry& m_registry;
				cnt::darray<ObserverRuntimeData*>* m_pCandidates = nullptr;

			public:
				//! Borrows and clears the candidate list for the next dispatch depth.
				//! \param registry Registry that owns the reusable candidate storage.
				explicit CandidateScope(ObserverRegistry& registry): m_registry(registry) {
					const auto depth = m_registry.m_candidate_depth++;
					if (depth == 0) {
						m_pCandidates = &m_registry.m_candidate_observers;
					} else {
						const auto nestedIdx = depth - 1;
						if (nestedIdx == m_registry.m_nested_candidate_observers.size())
							m_registry.m_nested_candidate_observers.push_back(new cnt::darray<ObserverRuntimeData*>());
						m_pCandidates = m_registry.m_nested_candidate_observers[nestedIdx];
					}
					m_pCandidates->clear();
				}

				//! Returns the candidate list reserved for this dispatch depth.
				//! \return Writable reusable candidate list.
				GAIA_NODISCARD cnt::darray<ObserverRuntimeData*>& candidates() {
					return *m_pCandidates;
				}

				//! Releases the borrowed candidate list.
				~CandidateScope() {
					GAIA_ASSERT(m_registry.m_candidate_depth > 0);
					--m_registry.m_candidate_depth;
				}

				CandidateScope(CandidateScope&&) = delete;
				CandidateScope(const CandidateScope&) = delete;
				CandidateScope& operator=(CandidateScope&&) = delete;
				CandidateScope& operator=(const CandidateScope&) = delete;
			};

			//! Keeps removed runtime records alive while user callbacks can still be executing them.
			class DispatchScope final {
				ObserverRegistry& m_registry;

			public:
				//! Starts a nested observer dispatch.
				//! \param registry Registry whose runtime records must remain stable.
				explicit DispatchScope(ObserverRegistry& registry): m_registry(registry) {
					++m_registry.m_dispatch_depth;
				}

				//! Finishes a nested observer dispatch and releases deferred runtime records when possible.
				~DispatchScope() {
					GAIA_ASSERT(m_registry.m_dispatch_depth > 0);
					if (--m_registry.m_dispatch_depth == 0)
						m_registry.release_retired_observer_data();
				}

				DispatchScope(DispatchScope&&) = delete;
				DispatchScope(const DispatchScope&) = delete;
				DispatchScope& operator=(DispatchScope&&) = delete;
				DispatchScope& operator=(const DispatchScope&) = delete;
			};

			//! Releases runtime records removed during observer dispatch.
			void release_retired_observer_data() {
				for (auto handle: m_retired_observer_data)
					m_observer_runtime_slots.free(handle);
				m_retired_observer_data.clear();
			}

			//! Deletes a runtime record immediately or defers it while a callback may still use it.
			//! \param handle Runtime slot removed from the live lookup map.
			void retire_observer_data(Entity handle) {
				GAIA_ASSERT(m_observer_runtime_slots.has(handle));
				if (m_dispatch_depth == 0)
					m_observer_runtime_slots.free(handle);
				else
					m_retired_observer_data.push_back(handle);
			}

			//! Returns the mutable diff index for an add or delete event.
			//! \param event Observer event whose index is requested.
			//! \return Mutable event index.
			GAIA_NODISCARD DiffObserverIndex& diff_index(ObserverEvent event) {
				GAIA_ASSERT(event == ObserverEvent::OnAdd || event == ObserverEvent::OnDel);
				return event == ObserverEvent::OnAdd ? m_diff_index_add : m_diff_index_del;
			}

			//! Returns the read-only diff index for an add or delete event.
			//! \param event Observer event whose index is requested.
			//! \return Read-only event index.
			GAIA_NODISCARD const DiffObserverIndex& diff_index(ObserverEvent event) const {
				GAIA_ASSERT(event == ObserverEvent::OnAdd || event == ObserverEvent::OnDel);
				return event == ObserverEvent::OnAdd ? m_diff_index_add : m_diff_index_del;
			}

			//! Checks all direct event maps for live observers registered under a term.
			//! \param term Exact component or pair term.
			//! \return True when at least one live observer is registered for the term.
			GAIA_NODISCARD bool has_observers_for_term(Entity term) const {
				const auto termKey = EntityLookupKey(term);
				return observer_map_has_observers(m_observer_map_add, termKey) ||
							 observer_map_has_observers(m_observer_map_del, termKey) ||
							 observer_map_has_observers(m_observer_map_set, termKey);
			}

			//! Checks whether a term has storage that can carry the observed flag.
			//! \param world World that owns the term records.
			//! \param term Component or pair term to inspect.
			//! \return True when the term can be marked as observed.
			GAIA_NODISCARD static bool can_mark_term_observed(World& world, Entity term);

			//! Checks whether a term uses semantic Is matching rather than exact pair matching.
			//! \param term Query term to inspect.
			//! \param matchKind Matching policy selected for the query term.
			//! \return True when changes to the target's inheritance chain can satisfy the term.
			GAIA_NODISCARD static bool is_semantic_is_term(Entity term, QueryMatchKind matchKind = QueryMatchKind::Semantic);

			//! Updates the observed flag and matching archetype counters for a concrete term.
			//! \param world World that owns the term and archetypes.
			//! \param term Concrete component or pair term.
			//! \param observed New observed state.
			void mark_term_observed(World& world, Entity term, bool observed);

			//! Adds an observer to the list stored under a term.
			//! Duplicate entries are allowed by this helper.
			//! \tparam TObserverMap Observer map type.
			//! \param map Observer map to update.
			//! \param term Lookup term.
			//! \param observer Observer entity to append.
			template <typename TObserverMap>
			static void add_observer_to_map(TObserverMap& map, Entity term, Entity observer) {
				const auto entityKey = EntityLookupKey(term);
				const auto it = map.find(entityKey);
				if (it == map.end())
					map.emplace(entityKey, cnt::darray<Entity>{observer});
				else
					it->second.push_back(observer);
			}

			//! Adds an observer to the list stored under a term unless it is already present.
			//! \tparam TObserverMap Observer map type.
			//! \param map Observer map to update.
			//! \param term Lookup term.
			//! \param observer Observer entity to add.
			template <typename TObserverMap>
			static void add_observer_to_map_unique(TObserverMap& map, Entity term, Entity observer) {
				const auto entityKey = EntityLookupKey(term);
				const auto it = map.find(entityKey);
				if (it == map.end())
					map.emplace(entityKey, cnt::darray<Entity>{observer});
				else
					add_observer_to_list(it->second, observer);
			}

			//! Appends an observer entity when the list does not already contain it.
			//! \param list Observer list to update.
			//! \param observer Observer entity to add.
			static void add_observer_to_list(cnt::darray<Entity>& list, Entity observer) {
				if (core::has(list, observer))
					return;
				list.push_back(observer);
			}

			//! Removes every occurrence of an observer entity from a list.
			//! \param list Observer list to update.
			//! \param observer Observer entity to remove.
			static void remove_observer_from_list(cnt::darray<Entity>& list, Entity observer) {
				for (uint32_t i = 0; i < list.size();) {
					if (list[i] == observer)
						core::swap_erase_unsafe(list, i);
					else
						++i;
				}
			}
			//! Checks whether a map entry refers to at least one live observer.
			//! \tparam TObserverMap Observer map type.
			//! \param map Observer map to inspect.
			//! \param termKey Lookup key within the map.
			//! \return True when the entry contains a live observer runtime record.
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

			//! Collects entities reachable from a changed source under an observer traversal plan.
			//! \param world World containing the relation graph.
			//! \param relation Relation followed from the root.
			//! \param root Entity where traversal starts.
			//! \param travKind Directions and self matching allowed by the query term.
			//! \param travDepth Maximum number of relation steps.
			//! \param visitStamp World visit stamp used to reject duplicate entities.
			//! \param outTargets Receives each reachable entity once.
			static void collect_traversal_descendants(
					World& world, Entity relation, Entity root, QueryTravKind travKind, uint8_t travDepth, uint64_t visitStamp,
					cnt::darray<Entity>& outTargets);

			//! Returns propagated observer targets, rebuilding the cache when either relation changed.
			//! \param registry Registry that owns the propagation cache.
			//! \param world World containing the relation graph.
			//! \param obs Observer that supplies the binding and traversal plan.
			//! \param changedSource Source entity whose change may affect bound entities.
			//! \return Cache entry containing the normalized affected entities.
			static PropagatedTargetCacheEntry& ensure_propagated_targets_cached(
					ObserverRegistry& registry, World& world, const ObserverRuntimeData& obs, Entity changedSource);

			//! Appends cached propagated targets without duplicates across changed sources.
			//! \param registry Registry that owns the propagation cache.
			//! \param world World used for entity visit stamps.
			//! \param obs Observer that supplies the propagation plan.
			//! \param changedSource Source entity being expanded.
			//! \param visitStamp Visit stamp shared by the complete expansion.
			//! \param visitedPairs Pair targets already appended during this expansion.
			//! \param outTargets Receives targets not seen for an earlier source.
			static void collect_propagated_targets_cached(
					ObserverRegistry& registry, World& world, const ObserverRuntimeData& obs, Entity changedSource,
					uint64_t visitStamp, cnt::set<EntityLookupKey>& visitedPairs, cnt::darray<Entity>& outTargets);

			//! Appends cached propagated targets for one changed source.
			//! \param registry Registry that owns the propagation cache.
			//! \param world World containing the relation graph.
			//! \param obs Observer that supplies the propagation plan.
			//! \param changedSource Source entity being expanded.
			//! \param outTargets Receives the cached targets.
			static void add_propagated_targets_cached(
					ObserverRegistry& registry, World& world, const ObserverRuntimeData& obs, Entity changedSource,
					cnt::darray<Entity>& outTargets);

			//! Narrows a propagated diff observer to entities affected through source traversal.
			//! \param registry Registry that owns the propagation cache.
			//! \param world World containing the relation graph.
			//! \param obs Observer whose diff plan is evaluated.
			//! \param changedTerms Terms changed by the mutation.
			//! \param changedSources Source entities changed by the mutation.
			//! \param outTargets Receives entities whose query result may have changed.
			//! \return True when the plan could produce a complete target set.
			GAIA_NODISCARD static bool collect_source_traversal_diff_targets(
					ObserverRegistry& registry, World& world, ObserverRuntimeData& obs, EntitySpan changedTerms,
					EntitySpan changedSources, cnt::darray<Entity>& outTargets);

			//! Chooses the target narrowing method required by an observer diff plan.
			//! \param registry Registry that owns propagated target caches.
			//! \param world World containing the changed entities.
			//! \param obs Observer whose plan is evaluated.
			//! \param changedTerms Terms changed by the mutation.
			//! \param changedTargets Entities directly changed by the mutation.
			//! \param outTargets Receives the complete possible target set when narrowing succeeds.
			//! \return True when the caller can safely avoid a full query scan.
			GAIA_NODISCARD static bool collect_diff_targets_for_observer(
					ObserverRegistry& registry, World& world, ObserverRuntimeData& obs, EntitySpan changedTerms,
					EntitySpan changedTargets, cnt::darray<Entity>& outTargets);

			//! Checks whether changed terms invalidate traversal data used by an observer query.
			//! \param world World used to resolve pair relations.
			//! \param obs Observer whose traversal relations are inspected.
			//! \param changedTerms Terms changed by the mutation.
			//! \return True when a changed pair uses one of the observer's traversal relations.
			GAIA_NODISCARD static bool
			observer_uses_changed_traversal_relation(World& world, const ObserverRuntimeData& obs, EntitySpan changedTerms);

			//! Checks whether a pair endpoint can match more than one entity.
			//! \param endpoint Pair relation or target identifier.
			//! \return True for wildcard and variable endpoints.
			GAIA_NODISCARD static bool is_dynamic_pair_endpoint(EntityId endpoint) {
				return is_wildcard(endpoint) || is_variable(endpoint);
			}

			//! Splits a concrete pair term into normalized relation and target endpoints.
			//! \param term Pair term to split.
			//! \param rel Destination relation entity.
			//! \param tgt Destination target entity.
			static void pair_endpoint_entities(Entity term, Entity& rel, Entity& tgt) {
				GAIA_ASSERT(term.pair());
				const auto relKind = term.entity() ? EntityKind::EK_Uni : EntityKind::EK_Gen;
				rel = Entity((EntityId)term.id(), 0, false, false, relKind);
				tgt = Entity((EntityId)term.gen(), 0, false, false, term.kind());
			}

			//! Checks whether a changed concrete term cannot identify all observers for a query term.
			//! \param term Observer query term.
			//! \return True when the term requires the global diff observer list.
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
			//! Checks whether a term has at least one live direct observer.
			//! \param term Exact component or pair term.
			//! \return True when a live observer is registered for the term.
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

			//! Returns whether an OnSet observer may observe writes for the given changed term.
			//! \param term Exact changed term.
			//! \return True if an exact or wildcard-pair OnSet observer is registered.
			GAIA_NODISCARD bool has_on_set_observers(Entity term) const {
				if (!m_hasOnSetObservers)
					return false;

				if (observer_map_has_observers(m_observer_map_set, EntityLookupKey(term)))
					return true;
				if (!term.pair())
					return false;

				Entity rel, tgt;
				pair_endpoint_entities(term, rel, tgt);
				return observer_map_has_observers(m_observer_map_set, EntityLookupKey(Pair(rel, All))) ||
							 observer_map_has_observers(m_observer_map_set, EntityLookupKey(Pair(All, tgt))) ||
							 observer_map_has_observers(m_observer_map_set, EntityLookupKey(Pair(All, All)));
			}

			//! Adds one observer query term to the indexes used by before-and-after dispatch.
			//! \param world World containing the observer entity and pair records.
			//! \param observer Observer entity being indexed.
			//! \param term Query term that may be affected by a mutation.
			//! \param options Source and traversal settings for the query term.
			void add_diff_observer_term(World& world, Entity observer, Entity term, const QueryTermOptions& options);

			//! Starts before-and-after dispatch around a world mutation.
			//! \param world World about to be changed.
			//! \param event Add or delete event produced by the mutation.
			//! \param terms Terms changed by the mutation.
			//! \param targetEntities Entities directly changed by the mutation, when known.
			//! \return Context to pass to finish_diff after the mutation.
			GAIA_NODISCARD DiffDispatchCtx
			prepare_diff(World& world, ObserverEvent event, EntitySpan terms, EntitySpan targetEntities = {});

			//! Starts add dispatch for entities that will be created by the mutation.
			//! \param world World in which the entities will be created.
			//! \param terms Terms assigned to the new entities.
			//! \return Context that accepts the created entities through add_diff_targets.
			GAIA_NODISCARD DiffDispatchCtx prepare_diff_add_new(World& world, EntitySpan terms);

			//! Adds newly created entities to an active diff dispatch context.
			//! \param world World containing the new entities.
			//! \param ctx Context returned by prepare_diff_add_new.
			//! \param targets Newly created entities.
			void add_diff_targets(World& world, DiffDispatchCtx& ctx, EntitySpan targets);

			//! Completes before-and-after dispatch and runs matching observer callbacks.
			//! \param world World after the mutation.
			//! \param ctx Context returned by a prepare_diff function.
			void finish_diff(World& world, DiffDispatchCtx&& ctx);

			//! Releases observer callbacks, queries, indexes, and cached dispatch data.
			void teardown() {
				for (auto& slot: m_observer_runtime_slots) {
					auto& obs = slot.runtime;
					obs.on_each_func = {};
					obs.query = {};
					obs.plan = {};
					obs.lastMatchStamp = 0;
				}

				m_observer_data = {};
				m_observer_runtime_slots = {};
				m_retired_observer_data = {};
				m_dispatch_depth = 0;
				m_candidate_observers = {};
				for (auto* pCandidates: m_nested_candidate_observers)
					delete pCandidates;
				m_nested_candidate_observers = {};
				m_candidate_depth = 0;
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

			//! Returns writable runtime storage for an observer, creating it when needed.
			//! \param observer Observer entity used as the storage key.
			//! \return Writable observer runtime record.
			ObserverRuntimeData& data_add(Entity observer) {
				const auto key = EntityLookupKey(observer);
				const auto it = m_observer_data.find(key);
				if (it != m_observer_data.end())
					return m_observer_runtime_slots[it->second.id()].runtime;

				const auto handle = m_observer_runtime_slots.alloc(nullptr);
				m_observer_data.emplace(key, handle);
				return m_observer_runtime_slots[handle.id()].runtime;
			}

			//! Finds writable runtime storage for an observer.
			//! \param observer Observer entity used as the storage key.
			//! \return Runtime record, or null when the observer is not registered.
			GAIA_NODISCARD ObserverRuntimeData* data_try(Entity observer) {
				const auto it = m_observer_data.find(EntityLookupKey(observer));
				if (it == m_observer_data.end() || !m_observer_runtime_slots.has(it->second))
					return nullptr;
				return &m_observer_runtime_slots[it->second.id()].runtime;
			}

			//! Finds read-only runtime storage for an observer.
			//! \param observer Observer entity used as the storage key.
			//! \return Runtime record, or null when the observer is not registered.
			GAIA_NODISCARD const ObserverRuntimeData* data_try(Entity observer) const {
				const auto it = m_observer_data.find(EntityLookupKey(observer));
				if (it == m_observer_data.end() || !m_observer_runtime_slots.has(it->second))
					return nullptr;
				return &m_observer_runtime_slots[it->second.id()].runtime;
			}

			//! Returns writable runtime storage for a registered observer.
			//! \param observer Registered observer entity.
			//! \return Writable observer runtime record.
			GAIA_NODISCARD ObserverRuntimeData& data(Entity observer) {
				auto* pData = data_try(observer);
				GAIA_ASSERT(pData != nullptr);
				return *pData;
			}

			//! Returns read-only runtime storage for a registered observer.
			//! \param observer Registered observer entity.
			//! \return Read-only observer runtime record.
			GAIA_NODISCARD const ObserverRuntimeData& data(Entity observer) const {
				const auto* pData = data_try(observer);
				GAIA_ASSERT(pData != nullptr);
				return *pData;
			}

			//! Marks a concrete term when observers were registered before the term existed.
			//! \param world World that owns the new term.
			//! \param term Newly available component or concrete pair term.
			void try_mark_term_observed(World& world, Entity term);

			//! Registers an observer under one of its query terms.
			//! \param world World containing the observer entity.
			//! \param term Query term used to find the observer during mutations.
			//! \param observer Observer entity being registered.
			//! \param matchKind Observer match policy used for the registered term.
			void add(World& world, Entity term, Entity observer, QueryMatchKind matchKind = QueryMatchKind::Semantic);

			//! Removes an observer entity or all observer registrations for a term.
			//! \param world World that owns the observer indexes.
			//! \param term Observer entity or observed term being removed.
			void del(World& world, Entity term);

			//! Dispatches a direct add event.
			//! \param world World containing the changed entities.
			//! \param archetype Archetype after the terms were added.
			//! \param entsAdded Terms added by the mutation.
			//! \param targets Entities that received the terms.
			void on_add(World& world, const Archetype& archetype, EntitySpan entsAdded, EntitySpan targets);

			//! Dispatches a direct delete event.
			//! \param world World containing the changed entities.
			//! \param archetype Archetype before the terms were removed.
			//! \param entsRemoved Terms removed by the mutation.
			//! \param targets Entities that lost the terms.
			void on_del(World& world, const Archetype& archetype, EntitySpan entsRemoved, EntitySpan targets);

			//! Dispatches a component write event.
			//! \param world World containing the written component.
			//! \param term Exact component or pair that was written.
			//! \param targets Entities whose value was written.
			void on_set(World& world, Entity term, EntitySpan targets);
		};
	} // namespace ecs
} // namespace gaia
#endif
