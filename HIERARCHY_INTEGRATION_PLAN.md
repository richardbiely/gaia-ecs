# Hierarchy integration plan

## Goal

Integrate the remaining useful hierarchy ideas described by Flecs while preserving Gaia-ECS's existing exact relationship semantics, deterministic behavior, and low-cost default paths.

## Design decisions

1. Keep `ChildOf` and `Parent` exact.
   - `Pair(ChildOf, target)` and `Pair(Parent, target)` queries retain their current meaning and cost.
   - Add semantic helpers for code that intentionally wants to treat both storage kinds as one hierarchy.

2. Keep unordered `Parent` storage as the default.
   - Existing source-indexed removal remains O(1) through swap removal.
   - Ordered children are opt-in per parent and pay the required linear erase/reorder cost only for those parents.

3. Reuse the query walk cache for non-fragmenting breadth-first ordering.
   - Add breadth-first traversal orders to `order_by(...)` rather than putting depth into archetype identity.
   - Cache the ordered entity list by relation version and query-result revision, as existing DFS traversal does.

4. Cache prefab instantiation plans persistently.
   - Cache topology, destination archetypes, copied sparse ids, and add-id metadata per prefab root.
   - Register only prefab nodes captured by a plan and advance a shared structural epoch when one of those nodes changes.
   - Validate each root in O(1) with the structural epoch, instantiation-policy epoch, and archetype-deletion version.
   - Refresh hook-dependent metadata before use so late hook registration remains correct.

5. Expose complete direct-id introspection.
   - Merge archetype ids, sparse ids, and non-fragmenting relation pairs.
   - Return ids once in deterministic order.

## Public API additions

- `World::parent(Entity child) const`: semantic parent lookup across `Parent` then `ChildOf`.
- `World::children(Entity parent, Func) const`: deterministic direct-child iteration across both hierarchy kinds.
- `Query::children_of(Entity parent)`: semantic OR term covering both hierarchy kinds.
- `World::each_direct_id(Entity, Func) const`: complete deterministic direct-id enumeration.
- `World::ordered_children(Entity parent, bool)`: enable or disable ordered `Parent` children.
- `World::set_child_order(Entity parent, EntitySpan)`: replace the stored direct `Parent` child order.
- `TravOrder::BreadthFirstDown` and `TravOrder::ReverseBreadthFirstDown`.

## Correctness coverage

- Semantic parent/children helpers with `ChildOf`, `Parent`, mixed storage, and duplicate edges.
- `children_of(...)` exact result set and cache invalidation after reparenting.
- Direct-id enumeration includes table, sparse, `DontFragment`, and non-fragmenting pair ids exactly once.
- Ordered `Parent` children survive component/archetype moves, reparenting, removal, and explicit reorder.
- Breadth-first `Parent` order is deterministic and refreshes after reparent/delete.
- Cached prefab plans refresh after prefab component, sparse-id, and hierarchy-topology changes.
- Cached prefab plans remain correct after destination archetype deletion.

## Performance validation

- Debug and optimized unit-test runs.
- Optimized A/B benchmark for repeated small prefab-tree batches against the same benchmark on the base revision.
- Treat breadth-first `Parent` ordering as a correctness/API addition; make no speedup claim without a separate traversal study.
- A/B the existing unordered `Parent` set benchmark against the base revision to guard its default hot path.

## Non-goals

- No automatic runtime choice between `ChildOf` and `Parent`.
- No redefinition of exact `ChildOf` query terms to hide `Parent` matches.
- No change to the 32 archetype-id limit.
- No single-header generation unless explicitly requested.

## Follow-on integration

The article review identified four smaller follow-ons after the initial hierarchy work. They are integrated under the
same exact-semantics and measurement rules:

1. Prefab/base reassociation
   - Add an atomic `replace_base(entity, oldBase, newBase)` structural operation.
   - Preserve entity identity, directly owned overrides, unrelated `Is` bases, and instantiated descendants.
   - Emit the normal direct `Is` removal/addition observer effects and refresh inherited-query caches.

2. Zero-allocation child batches
   - Add `each_child_batch(parent, func)` for read-only hot paths.
   - Expose contiguous `Parent` source buckets and `ChildOf` chunk spans without collecting or sorting.
   - Keep `children(...)` as the deterministic merged API; batch iteration is storage order and forbids structural
     mutation while a span is live.

3. Shared hierarchy depth cache
   - Reuse the world's relation-versioned depth cache for breadth-first ordering of exclusive hierarchy relations.
   - Define breadth-first hierarchy ordering by absolute depth, with entity id as the deterministic within-level tie-break.
   - Keep the generic induced-DAG traversal for non-hierarchy relations.
   - Retain the optimization only if an optimized A/B benchmark demonstrates a benefit.

4. Group iteration convenience
   - Add `each_group(func, sortGroups)` as a thin allocation-conscious wrapper around `groups(...)` and `group_id(...)`.
   - Save and restore the caller's selected group and expose the selected query to the callback.

### Follow-on validation

- Cover base replacement with inherited data, local overrides, multiple bases, direct-query invalidation, and observers.
- Cover mixed `Parent`/`ChildOf` batch iteration, duplicate suppression, storage order, and empty parents.
- Cover absolute hierarchy depth ordering for query subsets and invalidation after reparenting.
- Cover sorted group enumeration and selected-group restoration.
- Run Debug and Release unit tests and an optimized A/B benchmark isolating shared hierarchy-depth reuse.

### Follow-on measured results

Configuration: AppleClang 21, arm64, Release, Picobench focus settings (64 iterations), five interleaved
baseline/candidate process runs. Baseline is commit `2ff0a919`; both trees use the identical benchmark source.

- Four Parent breadth-first queries invalidated by one reparent, 10K entities:
  - baseline median: 6,662,549 ns/op
  - shared-depth median: 3,429,551 ns/op
  - result: 48.5% lower time
- One Parent breadth-first query invalidated by one reparent, 10K entities:
  - baseline median: 2,176,739 ns/op
  - shared-depth median: 1,356,522 ns/op
  - result: 37.7% lower time
- Mixed Parent/ChildOf direct-child scan, 10K children, same Release binary:
  - deterministic `children(...)` median: 162,826 ns/op
  - storage-order `each_child_batch(...)` median: 59,218 ns/op
  - result: 63.6% lower time; semantics intentionally differ in ordering
