# Gaia-ECS correctness and design audit

This is a working review list for Gaia-ECS itself and its retained examples. It
deliberately separates reproduced defects from design decisions and missing
coverage. Performance notes appear only where an optimized A/B was measured.

Status values:

- **Reproduced**: observed with a focused executable or directly exercised behavior.
- **Confirmed by inspection**: the problematic path is unambiguous in the source.
- **Review**: behavior is internally consistent, but the intended contract needs a decision.
- **Test gap**: comparable behavior exists but is not covered at the relevant boundary.

## Design inconsistencies requiring a contract decision

### DES-003 — negative observer terms used structural event direction

- Status: addressed
- Commits: `ac144df7` aligns negative-term event routing and `bbaed5a5` adds
  explicit exact whole-query monitors.
- Resolution: `OnAdd` and `OnDel` describe observer-query membership transitions.
  A negative term inverts the structural mutation used to find the observer: removing
  an excluded term can produce `OnAdd`, while adding it can produce `OnDel`.
  Single-negative observers retain direct dispatch. Compound negative observers use
  targeted before-and-after comparison so the other terms are evaluated correctly.
  `.monitor()` is the explicit exact whole-query mode. It compares membership before
  and after every relevant add or delete, then reports the actual transition as
  `OnAdd` or `OnDel` through `Iter::event()`.
- Coverage: all-negative transitions in both directions, mixed positive/negative
  queries, semantic `Is`, strict `In`, late `.monitor()` selection, switching back
  with `.event(...)`, inherited prefab removal, exact monitor transitions for
  positive, negative, and wildcard pair terms, entity deletion, and retained
  direct/dynamic observer regressions.
- Performance: `Observer_` remains 16 bytes, `ObserverRuntimeData` remains 992 bytes,
  and `ObserverPlan` remains 248 bytes. An optimized interleaved A/B with a 10,000-cycle
  warm-up and twenty 1,000,000-cycle samples measured a median 1246.480 ns per
  add/delete cycle with `.monitor()` versus 1249.272 ns before the API split.

### DES-006 — `Is` inheritance on exact pair records

- Status: addressed
- `Is` is an `(ordinary entity, ordinary entity)` relationship. Pair records are not `Is`
  subjects or bases, matching Flecs `IsA` (entity-to-entity) and Gaia pair packing (only
  endpoint indexes are stored; nested pairs are not representable).
- `World::as`, `EntityBuilder::as`, and `EntityBuilder::add(Pair)` assert on pair-bit
  subjects/bases and nested pair endpoints. `prepare_pair_add` and `link_is_relation`
  assert the same `Is` subject rule on packed-entity / command-buffer replay.
- `is_inter`, `is_base`, and `as_up_trav` keep returning false for pair identities.
  `Pair` remains a lookup-key wrapper as well, so construction itself is not asserted.
- `child(Pair, parent)` is unchanged: that attaches `Pair(ChildOf, parent)` with ordinary
  endpoints to a pair record, which is data on an edge, not taxonomy.
- Coverage: `Inheritance (Is) - pair records are not subjects or bases` in
  `src/test/src/test_ecs_entity.cpp`.

### DES-008 — command buffers cannot instantiate prefabs

- Status: addressed
- `CommandBufferST` and `CommandBufferMT` record `instantiate` / `instantiate_n` with the same Prefab,
  `Is`, name, OnInstantiate, and recursive child rules as `World::instantiate` / `instantiate_n`.
  `instantiate` returns a temporary root handle so per-instance `add` / `set` / `del` can be queued
  before commit. `instantiate_n` invokes its callback immediately while recording, with those
  temporary handles; `CopyIter` callbacks stay World-only. Commit groups matching records and
  replays them through `World::instantiate` / `instantiate_n` once the world is unlocked.
- Coverage: CommandBufferST instantiate vs copy fallback, name skip, DontInherit, Parent and
  ChildOf child recursion, parented roots including a temporary parent, instantiate_n with
  per-instance writes, canceled instantiate, sparse payload copy, CommandBufferMT, locked
  query.each, and a later system seeing iterator-buffer instantiations.

### DES-009 — `DontFragment` does not latch on compile-time table or empty tags

- Status: addressed
- Source: `world.add(componentEntity, DontFragment)` immediately after
  `add<T>()` and before any instances exist.
- Resolution: compile-time table payload storage stays authoritative for
  data-bearing structs such as `Position`. Empty tags can latch `DontFragment`
  before instances exist, with or without `GAIA_STORAGE(DontFragment)`. That
  macro also covers non-empty AoS payloads (sparse storage plus non-fragmenting
  membership). `Exclusive`+`DontFragment` empty ids such as `Parent` stay
  relation traits. Query/observer column mapping treats `ComponentIndexBad` as
  unmapped so fragmenting Sparse neighbors do not poison table `view_mut`.
- Coverage: compile-time table authority for `Position`; compile-time
  `DontFragment` empty tag and AoS payload; runtime latch on a fresh empty
  typed tag and a runtime-registered empty tag; rejected latch after instances
  exist; copy of an empty DontFragment tag; typed and EntityBuilder delete of
  empty DontFragment tags; empty-tag query `all`/`no`/`or_` with a table neighbor;
  OnAdd/OnDel observers; CommandBufferST add/del; prefab instantiate; `clear`;
  entity delete; binary and JSON save/load of empty DontFragment membership;
  `view_mut`/`sview_mut` of a table neighbor after Sparse-without-DontFragment
  is added.

### DES-010 — entity-seed `each()` does not commit iterator command buffers

- Status: addressed
- Source: entity-seed `each()` over a DontFragment / sparse term.
  `it.cmd_buffer_st().del/add/set` is expected to become visible when
  `each()` returns, matching chunk `each()` which unlocks and then
  `commit_cmd_buffer_st/mt`.
- Entity-seed `each()` now calls `finish_unlocked_each()` after
  `each_direct_iter_inter`, `each_direct_inter`, and typed sparse entity-seed
  execution. `commit_cmd_buffer_st/mt` already no-op while the world is locked,
  so nested entity-seed `each()` inside a chunk/system `each()` still defers to
  the outer unlock. The entity-seed callback is not locked.
- Coverage: `Entity-seed each commits iterator command buffers` in
  `src/test/src/test_storage.cpp`. Follow-up matching uses another entity-seed
  query, not a chunk query.

### DES-011 — Iter `each()` ignores DontFragment entity filters

- Status: addressed
- Commit: `0099f33a`
- Source: `Query::count()` / `arr()` / typed `each()` already call
  `match_entity_filters` for DontFragment / sparse terms. Public `Iter& each()`
  did not. It pushed whole chunk `[from, to)` ranges, so
  `.all<T>().no<DontFragment>()` still visited every archetype member.
  `.all<T>().all<DontFragment>()` already uses entity-seed evaluation for
  `Iter&` and was not the broken path.
- CityZens hit this on trip-need, in-transit, and other DontFragment markers:
  narrowed retained queries matched `.count()` but the hot `Iter& each()` path
  still scanned the full archetype. A broader CityZens FetchContent patch also
  split typed `each()` batches; that doubled typed `.no<>()` cost and is not
  on `main`.
- Resolution: public `Iter&` windows are contiguous chunk rows, so
  `each_matching_iter_range` splits those windows with `match_entity_filters`
  in the runtime batch runner. Queries without entity-filter terms keep the old
  contiguous range. Typed callbacks already skip non-matching rows and keep
  whole chunk ranges; splitting them too double-filtered and added iterator
  setup. `DirectDense` stays disabled when `QueryPlanFlag_EntityFilter` is set.
- Coverage: `Sparse DontFragment component direct query terms are evaluated as
  entity filters` now checks `Iter& each()` for `all` / `no` / `or_`, including
  plan mode. Additional cases cover interleaved one-row splits with aligned
  `view<Position>()` writes, enabled-row constraints, empty results, empty-tag
  neighbors, and system `on_each(Iter&)`.
- Performance: RelWithDebInfo, AppleClang, `-O2 -g -DNDEBUG -march=native`,
  10,000 `Position` entities, 2,000-cycle warm-up, twenty 3,000-cycle samples,
  three interleaved binary pairs. Medians of those pairs: unfiltered
  `all<Position>()` `Iter&` 2077 ns before and 2041 ns after; `.no<Marker>()`
  5% `Iter&` 2073 ns before (visited 10,000) and 341370 ns after (9,500
  matches); `.all<Marker>()` 5% `Iter&` 182265 ns before and 197838 ns after;
  `.no<Marker>()` 5% typed 298135 ns before and 312682 ns after; `.no<Marker>()`
  5% `count()` 299756 ns before and 300637 ns after; `.no<Marker>()` 50% `Iter&`
  2065 ns before (visited 10,000) and 374188 ns after (5,000 matches). The
  unfiltered `Iter&` path is unchanged. The old `.no<>()` `Iter&` numbers match
  `all<Position>()` because they did the same unfiltered chunk walk. Correct
  `Iter&` `.no<>()` is now in the same band as `count()` and typed `each()`.
  Splitting typed batches as well, as in the original suggested patch, doubled
  typed `.no<>()` to about 584 us; that path was left unsplit.


### DES-012 — Entity-typed APIs indexed pair records as relation endpoints

- Status: addressed
- Commits: `8dc1c617` looks up exact pair records in `enable`, `enabled`,
  `get_chunk`, `modify`, and `add_n`. `6d6d382d` extends that lookup to `copy` /
  `copy_n` / `copy_ext` / `copy_ext_n`, `add_n` type copy, and `instantiate` /
  `instantiate_n` / `sync`.
- Source: dedicated `Pair` overloads already fetched the exact pair record.
  Query callbacks yield `Entity`, so Entity-typed overloads still indexed
  `entities[id()]` and hit the relation endpoint that shares the numeric id.
- Resolution: those Entity-typed paths look up through `cont()`. Ordinary
  entities stay on the direct array. `add(Entity, Pair)`, `get(Entity)`,
  `acc` / `acc_mut`, and `mut_im` already used the unlikely-path lookup.
  `copy` of a pair record matches Flecs `ecs_clone`: it creates a new ordinary
  entity with the pair's table components, sparse payloads, and non-fragmenting
  relation pairs. The pair identity is not duplicated. `add_n` matches
  `ecs_clone(..., false)`: table payloads stay default, DontFragment ids and
  relation pairs are copied, fragmenting sparse ids get a default payload, and
  `EntityDesc` is omitted. `instantiate` of a non-prefab pair falls back to
  `copy`. A Prefab-tagged pair drops `Prefab` and copies data but does not add
  `Pair(Is, pair)`, because nested pairs are not representable.
  `World::add(entity, pair, value)` and `cb.add(entity, pair, value)` /
  `cb.set(entity, pair, value)` write the relationship payload on the owner.
  Structural `cb.add(entity, pair)` still attaches the pair without a payload.
  `cb.add<T>(pair, value)` writes component `T` on the pair record.
- Coverage: `Exact pair records - Entity-typed enable, modify, and add_n`,
  `Exact pair records - copy produces ordinary entities` (`copy_ext_n` included),
  `Exact pair records - add_n copies type without table values`,
  `Exact pair records - instantiate falls back to copy` (`instantiate_n` with a
  parent, and command-buffer instantiate payload/identity checks),
  `Exact pair records - instantiate prefab drops Prefab without Is`,
  `add_n copies non-table type without table values`, command-buffer table
  payload replay on exact pair records, and `Delayed relationship payload add
  and set`.
- Performance: ordinary-copy A/B (RelWithDebInfo, AppleClang,
  `-O2 -g -DNDEBUG -march=native -ftree-vectorize`, 128 `Position` sources,
  4 copies then deletes per source per cycle plus `update()`, 200-cycle
  warm-up, twenty 200-cycle samples, three interleaved binary pairs): median
  93.20 ns per copy+del before and 94.45 ns after. Ordinary `add_n` A/B (same
  compiler flags, one `Position` source, 256 entities then deletes plus
  `update()` per cycle, 200-cycle warm-up, twenty 200-cycle samples, three
  interleaved binary pairs): median 86.0 ns per add_n+del before and 87.4 ns
  after.

## Open defects

Found by the independent reviews of `3c2196f3` and `4afceb8c`. Each defect also
reproduces at the commit before those changes, so neither change introduced it.
The snippets below are self-contained repros. The reviewers' original probes lived in a
temporary session scratchpad and may no longer exist.

### BUG-001 — `SafeEntity` stored in a component breaks world destruction and `cleanup()`

- Status: Reproduced (AppleClang 21, ASan+UBSan, `4afceb8c`). Fixed: `~SafeEntity` releases through `try_fetch_mut` and skips cleared records; contract in README.
- Repro:
  ```cpp
  struct SafeHolder { ecs::SafeEntity s; };
  ecs::World w;
  auto e = w.add();
  auto h = w.add();
  w.add<SafeHolder>(h, {ecs::SafeEntity(w, e)});
  w.update();
  // Leaving scope, or calling w.cleanup(), asserts in World::fetch.
  ```
  It fails the same way whether `e` is live, `del()`'d, or a `ChildOf` child whose
  parent was deleted.
- Result:
  - Debug: `Assertion failed: allowStaleExactPair || allowDeleteRequestedEntity || ...`
    in `World::fetch`.
  - `-O2`: segfault.
- Cause: `cleanup_inter` clears `m_recs.entities` and the pair records before
  `Archetype::destroy` runs the chunk component destructors. `~SafeEntity` then calls
  `fetch_mut` → `World::fetch` on a record that no longer exists, and decrements and
  deletes through it.
- Fix direction: `SafeEntity` must not touch the world once teardown has cleared
  records. One option is to destroy chunk components before clearing records. Another is
  to invalidate every `SafeEntity`, as `cleanup_inter` already does for
  `WeakEntity`, but `SafeEntity` has no tracker list. Decide whether a `SafeEntity`
  inside a component is supported at all, then document the contract in the README.
- Coverage needed: a `SafeEntity` in a component, for world destruction and for
  `cleanup()`, with a live target and with a deleted target.

### BUG-002 — bulk-deleting a prefab together with its `instantiate()` copy asserts

- Status: Reproduced (`4afceb8c`; the reviewer reports the same stack at base). Fixed: `as_up_trav` asserts the record exists instead of `valid_entity`, so it walks up from a delete-requested base.
- Repro:
  ```cpp
  struct Pos { int v; };
  ecs::World w;
  auto t = w.add();
  w.add(t, ecs::Pair(ecs::OnDelete, ecs::Delete));
  auto P = w.prefab();
  w.add<Pos>(P, {5});
  w.add(P, t);
  auto inst = w.instantiate(P);   // the copy also gets t
  w.del(t);
  ```
- Result: Debug `Assertion failed: valid_entity(entity)` in `World::as_up_trav`, reached
  from `invalidate_queries_for_entity({Is, P})` ← `unlink_live_is_relation(inst, P)`
  ← `prepare_del_archetype_rows`.
- Cause: `del(t)` marks every archetype holding `t`. The prefab's archetype is marked
  first, which makes `P` invalid. Preparing the instance's archetype then unlinks the
  instance's live `Is` edge to `P`, and query invalidation traverses up from the
  already-invalid `P`.
- Fix direction: when a base is delete-requested, the `Is`-unlink path must not
  traverse through it. For example, skip or adapt `as_up_trav` for delete-requested
  bases, or unlink `Is` edges before any archetype of the same request is marked. A
  prefab whose instances are made with `w.as(e, P)` instead of `instantiate` does not
  hit this.
- Coverage needed: a prefab and its `instantiate()` copies deleted in the same
  cascade, with and without cached `Is` queries.

### BUG-003 — O(N²) deletion-guard scan when bulk-deleting with OnDel observers

- Status: Reproduced (optimized A/B). Fixed: the deletion guard mirrors its stack in a hashed set once deeper than 16 entries; bulk with one OnDel observer is 0.52 / 4.9 / 52 ms at 1k / 10k / 100k rows (was 0.65 / 10.3 / 630 ms).
- Source: `prepare_del_archetype_rows` puts every row on `m_entitiesDeleting` before
  notifying. `entity_deletion_active` is a linear scan of that stack, and it runs for
  every row and again inside the notifications, so the cost grows quadratically with
  the number of rows.
- Measurement (-O3, 10k rows of one `(OnDelete, Delete)` tag, one OnDel observer,
  median of 31): 9.1 ms before `4afceb8c`, 9.9 ms after. Without the observer, the same
  deletion takes about 1 ms.
- Fix direction: a hashed or flagged membership test for entities being deleted, for
  example a set beside the stack or a record flag. That removes the quadratic term
  without changing semantics.
- Coverage needed: an A/B at 1k, 10k and 100k rows with one OnDel observer, to confirm
  linear scaling.

### BUG-004 — deleting a pair applied its target's OnDeleteTarget rules

- Status: Fixed in `71e487e6` (per-entity pair deletion) and in the follow-up commit for pair archetype rows, which had the same call. Found while designing DES-013.
- Repro: `w.child(child, parent); w.add(x, Pair(rel, parent)); w.del(rel);` (or `w.del(Pair(rel, parent))`)
  deletes `child`. With a target that has no delete rule, `(rel2, target)` pairs on other entities are
  stripped instead.
- Cause: the pair branch of `handle_del_entity` ran `del_entity_target_rules` for the pair's live target,
  covering every `(*, target)` pair.
- Fix: removed. Deleting the target applies its own rules.

### BUG-005 — a `Parent` cascade leaves the deleted endpoint's pairs on their sources

- Status: Fixed. Non-fragmenting cascades now delete each source through `del_cascade_entity`, which runs
  `handle_del_entity` like a plain `del()`, but even while a SafeEntity references it. Costs about 40 ns per
  cascaded entity (10k `Parent` children: 1.34 -> 1.75 ms), the same rule pass `ChildOf` rows already pay.
- Repro:
  ```cpp
  w.add(src, ecs::Pair(rel, tgt));
  w.parent(tgt, p);
  w.del(p); w.update();
  // tgt is invalid, but has(src, Pair(rel, tgt)) is true. Once tgt's id is reused as `fresh`,
  // has(src, Pair(rel, fresh)) is true.
  ```
- Cause: non-fragmenting cascades delete sources through `req_del_nonfragmenting_pair` → `req_del` →
  `req_del_inter`, which never collects or deletes the entity's pairs (the archetype-row path does,
  through `del_pair_entities`).
- Fix direction: give `req_del_inter` cascades the pair deletion the row path has. README's `SafeEntity`
  section names this as a known exception.
- Coverage needed: a `Parent` cascade deleting a relation and a target of pairs on other entities, with
  and without a `SafeEntity` on the pair.

### BUG-006 — releasing a `SafeEntity` from a component destructor during removal reenters deletion

- Status: Fixed. `World::remove_entity` and `remove_chunk` mark a `CompDtorScope`; a release dropping the
  last reference inside one, or while the world is locked for iteration, queues the entity in
  `m_safeEntityDels`. The outermost `del()` drains the queue once nothing is in use, otherwise the next
  `frame_cleanup()` does. Releasing inside `each()` asserted in `verify_move` before, and is covered too.
- Repro (base): `src` has tag `x` and a component holding `SafeEntity(w, x)`; `w.del(x); w.del(src);`.
  Destroying the component releases the last reference, `del(x)` removes `x` from `src` while `src` is
  being removed, and `Chunk::remove_last_entity` asserts. Removing just the holder component
  (`w.del<Holder>(src)`) runs its destructor twice and trips `refCnt > 0` in the release.
  With DES-013, a component holding `SafeEntity(Pair(rel, tgt))` on `src` triggers the same thing
  through a deferred `del(rel)`.
- Fix direction: defer deletions requested by releases that run inside structural changes, for example
  through the command buffer or a queue drained after the change.

### BUG-007 — saving a world with a record reserved by a `SafeEntity` dereferences a null archetype

- Status: Fixed. Also found: `refCnt` was saved including live SafeEntity handles, which do not survive a
  load, so a loaded entity that had a handle could never be deleted. Live records now save only the world's
  own reference (and drop a deletion that waited for handles); deleted records are saved without a location
  and load as pending deletions that the next `update()` finishes.
- Repro: a cleanup rule deletes an entity (or, with DES-013, a pair endpoint) held by a `SafeEntity`, then
  the world is saved. `save_to` calls `ec.pArchetype->list_idx()` on the reserved record, whose archetype
  is null.
- Fix direction: skip or specially encode reserved records when saving, and restore their `refCnt`.

### BUG-008 — deleting an entity used both as a pair relation and as an id leaks its record

- Status: Fixed. Found while fixing BUG-005. Present at `205b5f8c`.
- Repro: `w.add(x, Pair(c, t)); w.add(x, c); w.del(c);` then any number of updates: `c`'s record is never
  freed, so its id is never reused.
- Cause: `rem_from_entities` computed a destination for every archetype holding the removed id, including
  ones the deletion had already emptied. That created archetypes such as `{c}` that no chunk ever entered,
  so they were never collected, and `del_finalize_entities` keeps a record while archetypes contain it.
- Fix: `rem_from_entities` skips archetypes without rows (`archetype_has_rows`).

### BUG-009 — the binary serializer buffer grows by 128 bytes, so writing N bytes is O(N²)

- Status: Reproduced (`9b97a35f`). Fixed in `17586d67`: `reserve` grows the capacity with `ResizePolicy::Scale` (1.5×),
  rounded up to 128 bytes at minimum, and compares against `capacity()`. Found while profiling an
  application whose command buffer records ~260k component adds in one pass; ~8 s of that pass went to
  copying this buffer.
- Regression test: `Serialization - binary buffer grows geometrically` (`ser_buffer_binary`,
  `ser_buffer_binary_dyn`, `bin_stream`) writes 256 KiB in 4-byte pieces and allows at most 32 relocations.
  It failed before the fix with 2048 relocations and passes in Debug and Release.
- Measurement (arm64 macOS, Apple Clang 21, `-O3 -DNDEBUG`, one warm-up and 12 alternating ABBA rounds,
  medians): writing 10 MiB into `bin_stream` 4 bytes at a time took 5585 ms before and 16.0 ms after (348×).
  Recording 262144 `add<T>(e, value)` into `CommandBufferST` took 480 ms before and 3.35 ms after (143×).
- Repro: write 256 KiB into a `ser::ser_buffer_binary` (or `ser_buffer_binary_dyn`) 4 bytes at a time and
  count how often `data()` changes. It relocates every 128 bytes, about 2048 times.
- Problem: `ser_buffer_binary_impl::reserve(size)` reserves `bytes() + size` rounded up to the next multiple
  of 128 whenever a write runs past `bytes()`, and `darray::reserve` / `darr_ext::reserve` allocate exactly
  that. Every relocation copies the whole buffer. `bin_stream` (command buffers, world save) routes all its
  writes through this `reserve`.
- Fix: grow the capacity geometrically with the containers' own `ResizePolicy::Scale`
  (`cnt::detail::resize_capacity`, 1.5×), keeping the round-up to 128 bytes as the minimum. Compare
  against `capacity()`, not `bytes()`, so a reserve inside the current capacity does not grow it.
- Decision needed: none; `darray` defaults stay unchanged, only the serializer's own growth changes.
  Open question: `bin_stream` and `CommandBuffer` do not expose `reserve`, so callers cannot pre-size a
  command buffer. With geometric growth the remaining cost is ~20 relocations per 10 MiB.

## Design decisions pending

### DES-013 — a `SafeEntity` on a pair keeps it alive after its relation's id is recycled

- Status: Fixed (candidate after `71e487e6`). A `SafeEntity` on a pair also holds its relation and target,
  so plain `del()` of an endpoint waits for the handle and no endpoint id is reused while held. When a
  cleanup rule deletes an endpoint anyway, the held pair is removed from its sources and its record
  stays reserved until release. README documents it. `Parent` cascades are the exception (BUG-005).
- Repro:
  ```cpp
  auto rel = w.add();
  auto tgt = w.add();
  auto src = w.add();
  w.add(src, ecs::Pair(rel, tgt));
  ecs::SafeEntity sp(w, ecs::Pair(rel, tgt));
  w.del(rel);   // or bulk-delete rel through a tag
  // update(), then create entities until one reuses rel's id as `fresh`:
  // w.has(src, ecs::Pair(fresh, tgt)) == true, and query().all(Pair(fresh, tgt)).count() == 1
  ```
  The pair goes away once `sp` is released.
- Problem: the `SafeEntity` keeps the pair record, and therefore the pair on `src`,
  alive. Pair ids do not carry the endpoint generations, so a new entity that reuses
  `rel`'s id aliases the surviving pair.
- Decision needed: either deleting a pair endpoint deletes the pair regardless of
  `SafeEntity`, or an endpoint's id is not recycled while a `SafeEntity` holds a
  pair that uses it. Then update the README's `SafeEntity` section, which currently
  says the wrapper never touches a recycled entity.

### DES-014 — runtime schemas carry no descriptions

- Status: addressed by `416665ea` (descriptions on fields, constants and types; manifest unchanged
  without them). Debug and Release ctest pass (53/53).
- Problem: `RuntimeFieldInit`, `RuntimeConstantInit` and `RuntimeTypeDesc` carry names, units,
  ranges and steps, but nothing that says what a value means. A tool built on
  `save_runtime_schema_json` (an inspector, a modding reference, generated script type
  definitions) has nothing to show beside a field. An application that wants that text must keep a
  second table keyed by type and field name next to the schema, and keep the two in step by hand.
- Found by: an application that reflects its configuration structs through `World::add<T>(RuntimeTypeDesc)`
  and wants its workbench and modders to see each value's meaning, unit and bounds from the
  manifest alone.
- Candidate:
  - optional `description` on field, constant and type descriptors, interned in the cache
    symbol table and shorter than `RuntimeDescriptionMaxLength` (1024);
  - read through `ComponentCacheItem::type_description()`, `field_description()` and
    `constant_description()`;
  - written to the manifest only when non-empty, so an existing manifest and
    `runtime_schema_hash()` stay byte-identical.

  Test: `Serialization - runtime schema descriptions`.
- Decision needed: whether descriptions belong in the schema, and so in its hash, or in a separate
  presentation layer; whether enum and bitmask constants need them too.

### DES-015 — semantic JSON import cannot require every field or enforce authored ranges

- Status: addressed by `d0bd514a` (two opt-in flags; `$raw` still bypasses both checks, see below).
  Debug and Release ctest pass (53/53). Test: `Serialization - strict runtime json import`.
- Repro:
  ```cpp
  struct Limits { uint32_t a; uint32_t b; };
  ecs::RuntimeFieldInit fields[] = {{util::str_view("a"), ecs::U32, 0, 0},
                                    {util::str_view("b"), ecs::U32, 4, 0}};
  fields[1].flags = ecs::RuntimeFieldFlag_HasMaximum;
  fields[1].maximum = 100.0;
  // register with w.add<Limits>(schema), then:
  // json_to_component(item, &value, reader("{\"a\":1}"), diagnostics)            -> true, no diagnostic,
  //   b keeps whatever value held
  // json_to_component(item, &value, reader("{\"a\":1,\"b\":999}"), diagnostics) -> true, no diagnostic,
  //   b == 999
  ```
- Problem: data that must be complete, such as configuration or content definitions, cannot be
  validated by Gaia on load. A missing field silently keeps the destination's old bytes. A value
  outside the field's authored range loads unchecked. Only `patch_comp_json` checks ranges. Each
  application re-implements field coverage and range checks over the schema it already registered.
- Candidate: opt-in `ser::RuntimeJsonPolicy` flags; the defaults stay as they are.
  - `requireAllFields` reports one `JsonDiagReason::MissingField` error per absent field, nested
    structs included.
  - `enforceRanges` reads a ranged field into a staged copy, reports `RangeViolation` and leaves the
    field unchanged.
  - The patch code's numeric reader (`runtime_patch_numeric_value`) moves to the import code, so
    validated patches and strict import share it.
- Decision needed: two flags or one strict mode; whether strict import should also refuse a
  `{"$raw":[...]}` payload for a type that has runtime fields, which otherwise bypasses both checks.

### DES-016 — one value can be patched by JSON Pointer but not read by one

- Status: addressed by `6dce9475` (`component_to_json_at(item, data, pointer, writer, diagnostics,
  policy)`: item and value pointer, so values outside a world read too; one pointer resolver shared
  with `patch_comp_json`). Debug and Release ctest pass (53/53). Test: `Serialization - runtime json
  pointer read`.
- Repro:
  ```cpp
  // w.patch_comp_json(e, comp, "/inner/small", "7", diagnostics, policy) -> true, one field written
  // reading /inner/small back: no API; component_to_json(item, &value, writer, policy) writes the
  // whole component, and the caller parses that text and walks the pointer itself
  ```
- Problem: `World::patch_comp_json` resolves an RFC 6901 pointer against the registered schema
  (`runtime_patch_decode_token`, `runtime_patch_parse_index`) and writes one field. The read side
  has no counterpart. A tool that shows or exports one value, such as an inspector, a console
  command or a modding API getter, serializes the whole component and then needs a second JSON
  library to parse the output and resolve the pointer again. It also has to re-check the pointer's
  syntax first, because that library's pointer parser may throw on bad escapes. The result is a
  full write and a full parse for one value, and two pointer implementations that can disagree on
  edge cases such as `~` escapes or array indices.
- Found by: an application whose operator tool reports one configuration value by JSON Pointer
  from a reflected singleton component.
- Candidate: `World::comp_json_at(entity, component, pointer, writer, diagnostics, policy)` and a
  `component_to_json_at(item, data, pointer, writer, policy)` overload. Both resolve the pointer
  with the patch code's token decoder and write only the addressed value, with the same symbolic
  enum and bitmask policy. An unknown field or a bad pointer gives `false` and a diagnostic.
- Decision needed: whether the read takes a `World`, an entity and a component like
  `patch_comp_json`, or only an item and a data pointer so values outside any world can be read;
  whether the patch code's pointer walk should become one shared resolver for both directions.

### DES-017 — a value of the wrong JSON kind fails the import without naming its field

- Status: addressed by `ca55e2df` (diagnostic only; still a hard parse failure). Debug and Release
  ctest pass (53/53). Test: `Serialization - runtime json value of the wrong kind names its field`.
- Repro:
  ```cpp
  struct Flags { bool enabled; uint32_t count; };
  // json_to_component(item, &value, reader("{\"enabled\":1,\"count\":2}"), diagnostics)
  //   -> false, diagnostics empty
  // json_to_component(item, &value, reader("{\"enabled\":true,\"count\":\"two\"}"), diagnostics)
  //   -> false, diagnostics empty
  ```
- Problem: `read_runtime_json_scalar` returns false when `ser::detail::read_runtime_field_json`
  cannot parse the value as the field's JSON kind: a number for a bool, or a string for a number.
  The import stops as malformed without adding a diagnostic. The caller learns only that the
  document failed. A lossy value of the right kind, an unknown enum name or a range violation
  names its field path instead. An application loading hand-edited data cannot tell the author
  which field is wrong.
- Found by: an application whose strict data loader reports the failing field to modders.
- Candidate: before the scalar reader returns false, add one error,
  `JsonDiagReason::InvalidJson`, at the field's path. The return value and the abort stay the
  same. Only the diagnostic is new.
- Decision needed: whether a wrong-kind value should stay a hard parse failure or become a
  per-field error that skips the value and continues, as an unknown field does.

## Resolved during this audit

- Commit `5f7cd741` fixes the 32-ID Debug assertion off-by-one and adds a regression
  that fills an archetype to `ChunkHeader::MAX_COMPONENTS`.
- The same commit adds valid-boundary coverage for exactly `MAX_ITEMS_IN_QUERY`
  query terms and changed filters.
- Dropping the four experimental examples left four stale `add_subdirectory(...)`
  calls in `src/CMakeLists.txt`, which made every retained example build fail during
  configuration. The stale calls were removed.
- `World::clear` removed archetype ids but retained non-fragmenting sparse
  components and non-fragmenting relations. It now clears both stores; regressions
  cover sparse payload reuse and `Parent` exact/wildcard queries across child-slot
  recycling.
- Command-buffer sparse add/set replay assumed every payload lived in a chunk.
  Sparse add-with-value could therefore dereference an invalid chunk column. Replay
  now uses the selected sparse store for fragmenting and `DontFragment` components,
  with deferred add/set/delete/recycle coverage.
- Command-buffer table add-with-value dispatched `OnAdd` before loading the queued
  payload, and buffered `set` skipped `OnSet` and changed-filter state. Replay now
  initializes before `OnAdd` and finishes buffered writes through the normal Gaia
  notification path; table and sparse observer regressions cover both behaviors.
- Sparse `World::set` and query write completion advanced the world version without
  marking an owning chunk, so `changed<SparseComponent>()` did not run. Fragmenting
  sparse components now mark their archetype column; `DontFragment` sparse writes
  conservatively mark the owning chunk at chunk granularity.
- Archetype sorting used the last row as a quicksort pivot and recursively processed
  both partitions. Reverse-ordered input caused linear recursion and a deterministic
  Debug/ASan stack overflow at 20,000 rows. Sorting now chooses a middle pivot and
  recurses only into the smaller partition; the existing 20,000-row sorted-query
  regression passes under ASan.
- Added changed-query lifecycle coverage for component removal, last-row deletion,
  row recreation, and buffered table writes.
- Command-buffer pairs packed temporary relation and target ids before commit,
  discarding the temporary markers and replaying a pair made from unrelated live
  ids. Pair operations now retain and resolve both endpoints independently, with
  a deferred temporary-relation/temporary-target regression.
- Command-buffer reduction only detected entity-order changes, so operations for
  one component separated by another component were replayed independently.
  `OnAdd` could consequently observe default data before a later buffered `set`.
  Full operation keys now drive sorting and equal-key operations retain insertion
  order; an interleaved add/set observer regression covers the behavior.
- Added lifecycle regressions for changed fixed-pair queries after target recycling,
  cached `Parent` query rematching after parent recycling, and deletion/recycling of
  a non-fragmenting relation used as its own source and target.
- Wildcard pair deletion only removed archetype-backed pairs. `(Rel, All)` silently
  retained a non-fragmenting binding, while mixed `(All, Target)` deletion asserted
  in Debug before reaching the archetype path. Builders now expand wildcard matches
  into deterministic exact-pair removals across both storage paths, preserving exact
  observer and dependency bookkeeping.
- Command-buffer pair sorting reordered wildcard removal and exclusive rebinding by
  target id instead of insertion order. Pair operations now remain ordered while
  ordinary component operations are still grouped for reduction; regressions cover
  add/remove reversal and two-target exclusive rebinding.
- EntityBuilder pending add/delete queues were fixed at 32 entries, matching the
  archetype component limit. A single entity can own more than 32 non-fragmenting
  pairs because those pairs do not consume archetype columns; bulk builder adds or
  wildcard removals therefore overflowed the fixed queues. The queues now retain 32
  ids inline and grow only for the wider non-fragmenting case.
- Wildcard `OnAdd` observers missed non-fragmenting relation additions because the
  relation store changed before EntityBuilder captured its normal observer diff;
  wildcard `OnDel` also missed exclusive retargeting because membership remained
  true across the replacement. Add diffs now evaluate the builder's final state,
  while traversal/source snapshots and explicit replacement removals are preserved
  from before the non-fragmenting mutation.
- `EntityBuilder` was copy-constructible even though each copy owned the same
  destructor-driven pending commit. Committing one copy left the other with stale
  source storage and could replay the mutation. Builders are now non-copyable and
  move construction explicitly transfers the single pending commit.
- Entity copy paths cloned archetype-backed pairs but omitted equivalent
  non-fragmenting relation pairs. `copy`, `copy_ext`, `copy_n`, and `copy_ext_n`
  now collect exact stored pairs in deterministic id order, attach them to every
  destination, and preserve the silent versus observer-reporting copy contracts.
- Entity deletion removed exact pair records before entering the entity's cascade
  transaction. In a two-node non-fragmenting `OnDeleteTarget(Delete)` cycle this
  recursively invalidated the requested root before its incoming source could be
  marked, leaving the second node alive. Entity deletion now collects pair records,
  processes the entity cascade first, and keeps its reentrancy guard active even
  when observers are compiled out. Coverage includes the cycle in both observer
  configurations and a 256-level `Parent` cascade.
- Commit `2efb6682` fixes `OnSet` dispatch queuing pointers into flat-map observer
  payloads. Deleting or registering observers from a callback could invalidate
  queued or active payloads,
  while nested `OnSet` dispatch reused and cleared the outer candidate list. Runtime
  payloads now live in stable paged slots, removed slots remain alive until the
  outermost callback returns, and nested dispatches borrow separate reusable
  candidate storage. Regressions cover deleting pending and active observers,
  callback-side registration, and nested `OnSet` delivery.
- Commit `895fb7f4` fixes direct target evaluation selecting its single-`all`
  shortcut even when `not` or `or` terms followed the required term. Observer
  candidate filtering consequently
  accepted entities after checking only the first term. Mixed shapes now retain the
  required-term seed but evaluate every remaining term; regressions cover negative
  and OR-group filtering for direct observers, including semantic `Is` and strict
  `In` seeds.
- Commit `c8bb60cb` fixes observer event selection being order-dependent. Calling
  `event(OnDel)` or `event(OnSet)` after adding query terms changed the stored
  event but left those terms indexed under the previous event, so callbacks could
  run for the wrong mutation and miss the requested one. Event changes now rebuild
  only that observer's construction-time dispatch indexes; regressions cover
  `OnDel`, `OnSet`, repeated event changes, and wildcard-pair diff dispatch.
- Commit `09de6ba1` fixes whole-entity deletion skipping component `OnDel`
  observers and component delete hooks. Notifications now run while table and
  sparse payloads remain readable, multi-term observers are batched once, and a
  conservative hook-use marker keeps the ordinary deletion path from scanning
  component ids. Regressions cover table, fragmenting sparse, non-fragmenting
  sparse, multi-term observer, and delete-hook behavior.
- Commit `a7d5e729` extends those notifications to entities removed by
  `(OnDeleteTarget, Delete)` cascades, whose bulk archetype path bypasses normal
  per-entity deletion. Regressions cover table components, exact pairs,
  non-fragmenting sparse payloads, delete hooks, multiple entities in one
  archetype, and callback-side deletion without duplicate delivery. The
  no-listener path retains its original per-entity unlink loop.
- Commit `e9f1aece` fixes `copy`, `copy_ext`, `copy_n`, and `copy_ext_n` creating
  entities with copied components without running component add hooks. Hook ids
  are selected once per copy operation and run after table and sparse payloads
  are initialized, before `OnAdd` observers. Coverage includes table, sparse,
  and data-bearing pair components across all four APIs.
- Commit `ddc5ab57` fixes typed pair registration mutating the destination builder
  before the caller performed its requested add. Typed pair adds consequently
  queued the same id twice; value adds emitted `OnAdd` first with default data
  and then with the initialized payload, and add hooks ran twice. Registration
  now only returns the registered pair id. Regressions cover initialized and
  value-less builder adds, typed builder removal, observer payload visibility,
  and pair add-hook count.
- Commit `0c7d1f56` fixes parented `instantiate_n` calls on ordinary entities
  dropping their non-fragmenting relation pairs. The fallback now collects the
  same copied relations and add-hook ids as `copy_n` before applying the parent
  batch. Regressions cover exact relation retention, copied payloads, parent
  visibility in callbacks, and copied-component add hooks.
- Commit `c47e63a7` fixes prefab instantiation skipping add hooks for
  data-bearing pair components. Prefab metadata now uses the common pair-aware
  hook collector, and single and batched dispatch resolve pair payload metadata
  through the destination archetype. Regressions cover initialized payloads and
  exact hook counts for `instantiate` and `instantiate_n`.
- Commit `fc17ca82` fixes `sync(prefab)` adding newly introduced data-bearing
  pairs to existing instances with default payloads. Exact pair columns now use
  their archetype component metadata during copy, with notification deferred
  until the payload is initialized. Regressions verify stored data plus observer
  and add-hook payload visibility.
- Commit `c846aed6` adds the missing pair-record source overload for attaching a
  relationship to an exact pair id. The ordinary `Entity + Pair` overload had
  indexed the relation entity record when its source was a pair and asserted;
  the dedicated overload fetches the exact pair record without adding a branch
  to normal pair insertion. Coverage configures and resolves a per-pair
  `OnInstantiate(Inherit)` policy.
- Commit `273495d8` fixes explicit inherited overrides materializing
  data-bearing pairs with default payloads and notifying ordinary component
  observers before inherited data was copied. Override now creates storage
  silently, copies the owner payload, then dispatches hooks and observers.
  Regressions cover component and pair observer payloads, pair add hooks, direct
  ownership, and repeat override behavior.
- Commit `147743b2` closes the missing exact-pair policy matrix coverage. The
  focused test verifies that default `Override` copies a direct pair payload,
  `Inherit` keeps it indirect with readable inherited data, and `DontInherit`
  leaves it absent.
- Commit `75a29d01` fixes typed payload access on exact pair records. The
  ordinary `Entity` overloads indexed the relation endpoint record after a
  component was attached to a pair id, causing typed add/get/set/modify and
  typed `OnSet` callbacks to use the wrong storage. Dedicated `Pair` overloads
  now select the exact pair record without adding a branch to ordinary entity
  access; the observer fallback keeps the pair-only lookup on its cold path.
  Regressions cover table and sparse components, data-bearing pair payloads,
  bulk accessors, write-back setters, modify notifications, runtime-selected
  components, and mutable query iteration.
- Commit `b75ef442` fixes enable-state and chunk lookup operations on exact pair
  records. Entity-typed overloads indexed the relation endpoint, so disabling a
  pair disabled its relation entity while the pair stayed query-visible, and
  chunk/row lookup returned unrelated storage. Dedicated `Pair` overloads now
  fetch the exact pair record without changing ordinary-entity paths.
  Regression coverage verifies independent pair and relation state, enabled
  query membership, and exact chunk rows across the enabled-partition swap.
- Commit `c4f5c431` fixes hierarchy-aware enabled-state checks on exact pair
  records. The Entity overload tested the pair relation endpoint before walking
  the pair record's own hierarchy, so a disabled pair could still report an
  enabled effective state. The dedicated `Pair` overload checks the exact
  record and then traverses its parent chain. Regression coverage includes
  direct, parent, and grandparent disablement; the ordinary Entity overload's
  optimized assembly is unchanged.
- Commit `0166e2d8` fixes `add_n(pair, count)` selecting the pair relation
  endpoint's archetype instead of the exact pair record's archetype. A
  dedicated `Pair` overload now creates ordinary entities with the source pair
  record's component layout. Regression coverage verifies all created entities
  receive the pair record's `Position` component, while the optimized ordinary
  Entity overload remains byte-for-byte identical to its baseline.
- Commit `402a0e90` restores named-system coverage in the schedule rebuild test.
  The existing test had disabled all three names behind an old warning; named
  systems now complete 100 dependency rebuild/update cycles in both Debug and
  Release while preserving the expected `DependsOn` order.
- Commit `90a6212e` fixes binary world loading reconstructing exact pair record
  keys from their entity-bearing storage handles. The resulting lookup key did
  not equal the canonical relationship pair, so archetype restoration asserted
  or lost the record. Loading now rebuilds the key from the live endpoints.
  Regression coverage round-trips an exact pair record with its own table
  payload and verifies its owner, payload, and query membership.
- Commit `ac3cec60` fixes non-fragmenting relation storage aliasing an exact pair
  source with the ordinary relation endpoint that shares its numeric id. Exact
  pair sources now use a full-identity side map while ordinary sources retain
  direct array lookup. Wildcard query seeds, relation teardown, pair deletion,
  reverse source buckets, and binary serialization include both source kinds.
  Regressions cover independent edges, bucket swaps, deletion cleanup, wildcard
  queries, and round-trip loading. Optimized A/B medians measured ordinary
  target lookup at 7.775 ns before and 7.770 ns after; repeated rebind measured
  11.909 ns before and 12.304 ns after.
- Commit `1e8d5ca4` fixes typed and runtime sparse stores aliasing an exact pair
  owner with the ordinary relation endpoint sharing its numeric id. Exact pair
  payloads now use a full-identity side map, while ordinary entities retain the
  existing sparse-storage lookup. Enumeration is deterministic, deletion clears
  sparse state when the entity becomes logically dead, and typed direct seeds
  avoid repeating the identity lookup. Regressions cover independent typed and
  runtime payloads, immutable and mutable typed queries, entity enumeration, and
  pair deletion without disturbing the relation endpoint. Debug and Release
  each pass all 9 CTest targets, and the focused Address/Undefined sanitizer run
  passes. Optimized A/B medians measured ordinary sparse get at 4.72 ns both
  before and after, add/delete at 39.88 ns before and 39.98 ns after, and the
  deliberately minimal payload-only sparse query at 4.53 ns before and 4.88 ns
  after per row (the exact-pair identity dispatch adds about 0.35 ns).
- Commit `309adb54` fixes exact pair deletion leaving the pair record's own
  archetype row and table payloads alive after the pair became publicly invalid.
  Those ghost rows remained query-visible, and reattaching the same relationship
  created owner membership without reviving the exact pair record. Exact pair
  deletion now removes its row through the normal entity lifecycle, and a later
  attachment can revive the deferred record with fresh storage while cancelling
  its pending invalidation. The regression covers query visibility, stale-payload
  removal, delayed and immediate recreation, target lookup, and survival across
  subsequent world updates. Debug and Release each pass all 9 CTest targets, and
  the focused Address/Undefined sanitizer run passes. Optimized A/B medians kept
  first-time pair add at about 801 ns; correct deletion measured about 545 ns
  versus 526 ns for the broken path that skipped removing the pair's own row.
- Commit `9042a35b` fixes binary world snapshots omitting every sparse component
  payload. Fragmenting sparse membership survived loading without its backing
  value, while non-fragmenting sparse owners disappeared entirely. Version 5
  snapshots now serialize populated sparse stores in deterministic component and
  owner order, including exact pair owners, runtime components, and custom
  serializers; versions 2 through 4 remain readable. Debug and Release each
  pass all 9 CTest targets, the Address/Undefined sanitizer suite passes 691
  tests with 2,255,234 assertions, and Doxygen builds successfully. An optimized
  A/B measured empty-world save at 3.36 us before and 3.88 us after. Saving
  4,096 sparse values measured 345 us for the broken 150,336-byte snapshot and
  450 us for the correct 215,888-byte snapshot, about 25.6 ns per restored
  owner/value added to the stream.
- Commit `4858e44a` fixes semantic world JSON reading fragmenting sparse values
  from unrelated chunk memory and omitting non-fragmenting sparse values because
  their ids do not participate in archetype signatures. JSON save now resolves
  fragmenting payloads through their sparse stores and indexes non-fragmenting
  store owners once so their values are emitted in the normal entity component
  object. Semantic loading creates and addresses both typed and runtime sparse
  storage directly. Regressions cover fragmenting typed, non-fragmenting typed,
  custom serialization, and non-fragmenting runtime values. Debug and Release
  each pass all 9 CTest targets, the focused Address/Undefined sanitizer test
  passes, and Doxygen builds successfully. Optimized A/B medians for 1,024
  table-only entities were 349.4 us before and 345.8 us after; fragmenting sparse
  output was 356.8 us before and 349.5 us after. Correctly emitting 1,024
  previously omitted non-fragmenting values added about 224 ns per value.
- Commit `86f440c5` fixes semantic JSON loading treating any nested key or string
  equal to `binary` as the optional top-level binary snapshot. A normal runtime
  component named `binary` caused its raw four-byte payload to be passed to the
  world snapshot loader and rejected as an unsupported world version. The
  validated top-level parse now collects only the actual root `binary` field and
  parses its byte array once. Debug and Release each pass all 9 CTest targets,
  the focused Address/Undefined sanitizer test passes, and Doxygen builds
  successfully. Optimized A/B median load time for a 69 KB JSON document with a
  binary snapshot improved from 560 us to 364 us; semantic-only loading improved
  from 254 us to 251 us.
- Commit `b004e485` fixes command-buffer table payload replay addressing the
  ordinary relation endpoint record when the operation target was an exact pair
  record. Structural add/remove already used pair-aware lookup, but buffered
  `add<T>(pair, value)` and `set<T>(pair, value)` resolved the destination chunk
  and row through the relation's numeric id. Replay now keeps the direct-array
  fast path for ordinary targets and fetches exact pair records explicitly. The
  regression covers buffered add, set, remove, payload values, and isolation
  from the relation endpoint. Debug and Release each pass all 9 CTest targets,
  the focused Address/Undefined sanitizer test passes, and Doxygen builds
  successfully. Optimized A/B median cost for 819,200 ordinary buffered sets was
  30.08 ns before and 30.98 ns after, about 0.9 ns for the predictable pair
  dispatch.
- Commit `4e94d974` fixes command-buffer target sorting deleting an entity before
  a later-sorted source group could replay a relationship to that still-valid-at-
  recording-time target. The result depended on entity id order and could assert
  while resolving the pair endpoint. Commit now replays all component and pair
  groups first, then applies queued entity deletions in the same deterministic
  target order; cascade cleanup therefore sees every relationship consistently.
  The regression covers both relative source/target id orderings. Debug and
  Release each pass all 9 CTest targets, the focused Address/Undefined sanitizer
  test passes, and Doxygen builds successfully. Optimized A/B median cost for
  819,200 ordinary buffered sets was 31.26 ns before and 30.79 ns after, with no
  measurable component-only replay regression.
- Commit `d0dd4d92` fixes binary snapshot loading silently accepting a target
  world whose non-core components were registered in a different order. Two
  same-sized components were reproducibly assigned each other's bytes while
  `load()` returned true. Version 6 snapshots now store a deterministic component
  symbol/id/layout directory and validate it before mutating the target world;
  versions 2 through 5 remain readable. The regression reverses `Position` and
  `Acceleration` registration and verifies an early failure with the existing
  target registrations intact. Debug and Release each pass all 9 CTest targets,
  the focused Address/Undefined sanitizer test passes, the generated single
  header configurations pass, and Doxygen builds successfully. An optimized A/B
  over 1,024 entities with eight components measured median save at 161-164 us
  after versus 170-174 us before, and load at 125-129 us after versus 136-139 us
  before; the validation directory added 192 bytes to the 172 KB snapshot. The
  README now states that binary `load()` is an exact snapshot: the same user
  components, in the same order, with the same names and storage. Adding,
  removing, renaming, reordering, or resizing a component makes `load()` return
  false without changing the target world. Built-in component ids are still
  remapped. Semantic JSON is the path that skips unknown content.
- Commit `27b83ee6` fixes enabled-row removal decrementing the chunk's disabled
  partition boundary even though the number of disabled rows did not change.
  With two entities in one archetype, disabling one and structurally moving the
  only enabled neighbor left the disabled entity's record and chunk row in
  disagreement; Debug asserted, and optimized builds could no longer re-enable
  the entity. The regression verifies all/enabled/disabled query counts before
  and after re-enabling the survivor. A 100,000-step deterministic differential
  run covering table, sparse, `DontFragment`, `Parent`, enable/disable,
  deletion/recycling, and cached mixed queries also completes cleanly. Debug and
  Release each pass all 9 CTest targets, the focused Address/Undefined sanitizer
  test passes, the generated single-header configurations pass, and Doxygen
  builds successfully. An optimized A/B of one million unaffected structural
  add/remove round trips measured a 108.0 ns median after versus 108.7 ns before.
- Commit `51a2df30` fixes typed query array materialization resolving an exact
  pair result through the ordinary relation endpoint that shares its numeric id.
  Sparse payload arrays asserted while indexing the sparse store, and dense
  payload arrays could silently duplicate the relation endpoint's value. The
  direct entity accessor now reconstructs the exact pair record on its unlikely
  path while ordinary entities retain their existing access path. The regression
  covers both sparse and dense array payloads. Debug and Release each pass all 9
  CTest targets, the focused Address/Undefined sanitizer test passes, the
  generated single-header configurations pass, and Doxygen builds successfully.
  An optimized A/B over 100,000 ordinary entities and 100 array passes measured
  2.002 ns per materialized entity after versus 2.009 ns before.
- Commit `121f850c` fixes `child(pair, parent)` selecting the ordinary-entity
  insertion overload after converting the exact pair wrapper to `Entity`. Debug
  asserted because the relation endpoint record did not describe the pair row;
  optimized builds could mutate unrelated storage. A dedicated `Pair` overload
  now selects the existing exact-pair relationship path at compile time, so the
  ordinary entity path is unchanged. The existing exact-pair hierarchy test now
  uses the convenience API and verifies the relation endpoint remains untouched.
  Debug and Release each pass all 9 CTest targets, the focused
  Address/Undefined sanitizer test passes, the generated single-header
  configurations pass, and Doxygen builds successfully.
- Commit `d9278e09` adds the missing fixed-source query coverage for an exact
  pair record. The test verifies initial non-match, dynamic-cache refresh after
  adding the source payload, typed iteration of the self-source term, exact-pair
  payload identity, and cache refresh after removal in Debug, Release, and the
  Address/Undefined sanitizer configuration.
- Commit `f8981333` fixes typed pairs whose relation-owned or target-owned
  payload type declares `GAIA_STORAGE(Sparse)`. Typed access selected the sparse
  path even though pairs are table-backed, while archetype layout copied the
  endpoint's sparse descriptor and reserved no payload bytes. Initialization
  then overwrote the pair id stored in the chunk header. Pair-aware compile-time
  dispatch and an effective table descriptor now keep typed, erased, iterator,
  and lifecycle paths consistent. The regression covers both payload endpoints,
  raw access, iterator views, and a non-trivial default constructor. Debug and
  Release each pass all 9 CTest targets, the focused Address/Undefined sanitizer
  test passes, the generated single-header configurations pass, and Doxygen
  builds successfully. Optimized interleaved A/B medians for ordinary sparse
  components were 4.75 vs 4.79 ns per get, 4.92 vs 4.96 ns per queried entity,
  and 40.70 vs 41.29 ns per add/remove after versus before.
- Commit `da1d3c66` fixes the C++17 fallback `span` accepting temporary
  initializer-list storage. The span retained the temporary backing array after
  the declaration's full expression, and the first later read reproduced as an
  AddressSanitizer stack-use-after-scope. Compatible initializer-list overloads
  are now explicitly deleted, and initializer lists no longer satisfy Gaia's
  internal contiguous-container detection. The compile-time regression verifies
  rejection while retaining normal built-in array construction. Debug and
  Release each pass all 9 CTest targets, the focused Address/Undefined sanitizer
  test passes, the generated single-header configurations pass, and Doxygen
  builds successfully. The change only removes constructor candidates and emits
  no runtime code.
- Commit `844d64fc` fixes random-access subtraction in all four SoA container
  iterator families. Mutable and const `operator-(offset)` added the offset, so
  expressions such as `end() - 1` advanced beyond the container instead of
  selecting its last element. The regression covers fixed, inline-resizable,
  dynamic, and dynamic-inline SoA containers through mutable and const
  iterators. Debug and Release each pass all 9 CTest targets, the
  Address/Undefined sanitizer suite passes, the generated single-header
  configurations pass, and Doxygen builds successfully. The implementation
  still performs one index arithmetic operation and adds no branches, loads, or
  allocations to the iterator path.
- Commit `658d762f` fixes iterator-range construction from forward and input
  iterators in all eight AoS and SoA array families. The fallback copy loop
  incremented its destination index before assignment, leaving element zero
  untouched and writing the final source element at `size()`. Debug builds
  asserted and an optimized AddressSanitizer reproduction reported a heap
  buffer overflow. The regression covers fixed, inline-resizable, dynamic, and
  dynamic-inline AoS and SoA containers. Debug and Release each pass all 9
  CTest targets, the focused Address/Undefined sanitizer test passes, the
  generated single-header configurations pass, and Doxygen builds successfully.
  The constructor loop still performs one index increment per copied element;
  only whether that increment occurs before or after indexing changed.
- Commit `e42dc944` fixes the advertised random-access operations of
  `sringbuffer` iterators. Subscript and addition/subtraction tried to construct
  an iterator from only an index (or even return an element as an iterator), so
  these operations failed when instantiated. They now preserve the buffer,
  tail, size, and logical index, including across physical wraparound. Iterator
  distance now uses the 32-bit signed `sringbuffer_detail::diff_type`, matching
  the 32-bit index design while representing reverse distances correctly. The
  regression covers mutable and const wrapped access, forward and reverse
  offsets, signed distance, and `core::find`. Debug and Release each pass all 9
  CTest targets, the focused Address/Undefined sanitizer test passes, generated
  single-header configurations pass, and Doxygen builds successfully. Iterator
  object size is unchanged, and the operations add no allocation or branch.
- Commit `fd85344b` fixes both `sringbuffer::pop_back` overloads checking
  `size() < capacity()` instead of checking that the buffer is non-empty. A
  valid pop from a full buffer asserted in Debug, while an invalid pop from an
  empty buffer passed the guard and underflowed the index. The regression pops
  copied and moved values starting from a full buffer. Debug and Release each
  pass all 9 CTest targets, the focused Address/Undefined sanitizer test passes,
  generated single-header configurations pass, and Doxygen builds
  successfully. The check compiles out in optimized builds, so release code is
  unchanged.
- Commit `b56881c5` fixes `sringbuffer::operator==` returning false as soon as
  corresponding elements were equal, ignoring live size, and comparing all
  physical slots instead of the logical ring sequence. Besides inverted
  results, the old implementation could read unused fundamental-type storage.
  Equality now compares size and only live values in logical order, and
  `operator!=` mirrors it. The regression covers empty buffers, different
  physical layouts with the same wrapped sequence, value mismatches, and size
  mismatches. Debug and Release each pass all 9 CTest targets, the focused
  Address/Undefined sanitizer test passes, generated single-header
  configurations pass, and Doxygen builds successfully. Equality now visits at
  most `size()` elements rather than always visiting the full capacity.
- Commit `8abe277e` fixes `sringbuffer` copy/move construction and assignment
  copying physical slots `[0, size)` while preserving a nonzero tail. Wrapped,
  non-full buffers therefore exposed slots that were never copied, and the old
  helper calls also failed to compile when these member functions were first
  instantiated. The implementation now bulk-copies the one or two physical
  segments containing the live logical sequence. The regression covers copy
  construction, copy assignment, move construction, move assignment, source
  emptying, and logical values across a wrap. Debug and Release each pass all 9
  CTest targets, the focused Address/Undefined sanitizer test passes, generated
  single-header configurations pass, and Doxygen builds successfully. An
  optimized interleaved A/B over a 48-element `uint64_t` ring measured the
  common unwrapped assignment at 4.11 ns after versus 4.19 ns before. A wrapped
  assignment measured 7.29 ns after versus 4.60 ns for the old incorrect
  one-segment copy; the extra approximately 2.7 ns is the required second bulk
  segment.
- Commit `e5d4d5aa` fixes lvalue insertion in the dynamic AoS array families.
  Both overloads shifted through `size() + 1`, reading an unused slot and
  writing past the new logical end; plain `darr` then constructed the copied
  value at the old end instead of the requested position. Existing tests only
  passed rvalues and selected the unaffected overload. The regression inserts
  lvalues into `darr` and `darr_ext` at the spare-capacity boundary and verifies
  the returned iterator and full sequence. Debug and Release each pass all 9
  CTest targets, the focused Address/Undefined sanitizer test passes, generated
  single-header configurations pass, and Doxygen builds successfully. The fix
  removes one element from the shift range and adds no operation.
- Commit `05227d52` fixes mutation APIs in all three resizable SoA array
  families. Their insert and erase overloads compared SoA iterators with raw
  pointers and relied on pointer-only distance helpers, so the public functions
  did not compile when instantiated. The shared SoA shift wrappers also passed
  a byte where a buffer pointer was required; once instantiated, right shifts
  traversed forward and overwrote values still needed as sources. Insert and
  erase now use native iterator arithmetic, right shifts traverse backward, and
  multi-element left shifts use the logical source range. The regression covers
  lvalue and rvalue insertion, insertion at end, single erase, middle range
  erase, and tail range erase in `sarr_ext_soa`, `darr_soa`, and
  `darr_ext_soa`. Debug and Release each pass all 9 CTest targets, the focused
  Address/Undefined sanitizer test passes all 66 assertions, generated
  single-header configurations pass, and Doxygen builds successfully. The
  corrected shifts remain linear in the number of moved elements and add no
  allocation.
- Commit `b6ddfd59` fixes reverse traversal in all eight AoS and SoA array
  families. The array APIs returned ordinary forward iterators, requiring `--`
  while Gaia's other reverse iterators advance with `++`; AoS `rend()` also
  formed a pointer before the allocation, and empty dynamic `rbegin()` called
  `back()`. A Gaia random-access reverse adapter now stores the valid forward
  base iterator, so `rbegin()` is based on `end()` and `rend()` on `begin()`.
  Array iterator differences use the 32-bit signed standard name
  `difference_type`, instead of unsigned differences or an unnecessary
  `ptrdiff_t`. The regression covers mutable and const
  traversal, empty containers, indexing, arithmetic, and signed distance across
  all eight families. Debug and Release each pass all 9 CTest targets, the
  focused Address/Undefined sanitizer test passes all 150 assertions, generated
  single-header configurations pass, and Doxygen builds successfully. An
  optimized 15-pair interleaved A/B over 409.6 million visited elements per
  sample measured 0.062605 ns per element after versus 0.062599 ns before. A
  nine-pair optimized compile A/B measured medians of 1.17 s after and 1.15 s
  before, with overlapping samples.
- Commit `9a0e30a3` fixes the per-page AoS and SoA iterator inequality operators.
  They combined page and bit-position differences with `&&`, so two different
  elements in the same page compared as neither equal nor unequal. Inequality
  now directly complements equality. The regression covers mutable and const
  forward and reverse positions in normal `page_storage` use. Debug and Release
  each pass all 9 CTest targets, the focused Address/Undefined sanitizer test
  passes, generated single-header configurations pass, and Doxygen builds
  successfully. When page pointers differ, the corrected expression can
  short-circuit without comparing the inner bit iterator.
- Commit `b0d00617` fixes reverse iteration over an all-zero `bitset` or
  `dbitset`. Reverse iterator construction searched for an earlier set bit but,
  when none existed, selected the final unset index instead of the established
  `UINT32_MAX` reverse sentinel. Empty reverse ranges therefore exposed a bogus
  element, and paged storage could dereference unallocated page data while
  trying to skip an empty page. The fixed and dynamic bitset regressions verify
  both endpoint equality and zero reverse visits. Debug and Release each pass
  all 9 CTest targets, both focused Address/Undefined sanitizer suites pass,
  generated single-header configurations pass, and Doxygen builds
  successfully. Non-empty reverse search follows the unchanged branch.
- Commit `c02fb851` fixes ordinary `page_storage` traversal across empty
  intermediate pages. Iterator construction skipped leading holes, but
  advancement moved to only one adjacent page and could expose the sentinel of
  an unallocated page as an element. Reverse endpoints also formed a page
  pointer before the allocation. Forward and reverse iterators now keep the old
  single-comparison hot path and loop only inside page transitions until they
  find data or the valid page boundary. The existing non-empty assertion
  contract is unchanged. Mutable and const regressions cover both directions
  across an unallocated middle page. Debug and Release each pass all 9 CTest
  targets, the focused Address/Undefined sanitizer test passes, generated
  single-header configurations pass, and Doxygen builds successfully. An
  optimized 21-pair interleaved A/B over 32.768 million dense visits per
  direction measured 3.1807 vs 3.1910 ns per forward element and 3.7841 vs
  3.7957 ns per reverse element after versus before.
- Commit `5c318058` fixes the paged allocator's usable-block alignment and
  capacity accounting. Each block started on a 16-byte boundary but returned
  its payload immediately after an 8-byte page pointer, so objects requiring
  16-byte alignment were misaligned. The header was also counted inside the
  requested block size, leaving less usable storage than callers requested.
  The payload now starts after one aligned header area, and total block size
  includes both that header and the requested usable bytes. The regression
  checks the returned alignment and compile-time usable capacity. Debug and
  Release each pass all 9 CTest targets, and the focused Address/Undefined
  sanitizer tests pass for both the allocator and SoA paged storage.
- Commit `b4c2e5c0` makes `page_storage<T>` select its public iterator family
  from `T`'s data layout. SoA storage previously advertised SoA iterator types
  but `begin()`, `end()`, reverse traversal, `front()`, and `back()` all used
  the AoS family and did not compile when instantiated. A separate SoA iterator
  is required because each member lives in a separate array, so there is no
  addressable `T` record that an AoS iterator could return as `T&` or `T*`.
  Lvalue insertion also
  selected its return behavior from the argument's cv-qualified type instead
  of the storage type. The SoA iterators now have a valid end state, skip empty
  intermediate pages, and are selected by the public aliases. The regression
  covers const-lvalue insertion, indexed and endpoint access, and mutable and
  const traversal in both directions across an empty page. Debug and Release
  each pass all 9 CTest targets, the focused Address/Undefined sanitizer test
  passes all 27 assertions, generated single-header configurations pass, and
  Doxygen builds successfully. Dense advancement still performs one end check
  per element; the loop is entered only at a page boundary.
- Commit `2297e78c` standardizes Gaia's array, ring-buffer, and span distance
  aliases on `difference_type`. The earlier reverse-array fix introduced the
  shorter internal `diff_type` spelling while retaining `difference_type` on
  public iterators, leaving two names for the same concept. All Gaia-owned
  aliases now use the standard spelling needed for iterator compatibility;
  the array distance type remains the explicit signed 32-bit `int32_t` selected
  in `b6ddfd59`. Debug and Release each pass all 9 CTest targets, the focused
  Address/Undefined reverse-array test passes all 150 assertions, generated
  single-header configurations pass, and Doxygen builds successfully. This is
  a type-alias-only change and adds no runtime work or storage.
- Commit `442e0c53` resolves DES-004 by separating hierarchy mutations from
  predicates. `child()` and `parent()` now only mutate a mutable `World`, while
  `is_child()` and `is_parent()` query either a mutable or const `World`. No
  compatibility predicate overloads remain under the mutation names. Internal
  hierarchy checks use the explicit predicates and no longer cast the world to
  const to force overload selection. Regressions cover mutable and const checks,
  true and false results, and exact pair-record children. Debug and Release each
  pass all 9 CTest targets, the focused Address/Undefined tests pass, generated
  single-header configurations pass, and Doxygen builds successfully. The
  predicate bodies retain the same direct `has`/`has_direct` calls, adding no
  runtime work.
- Commits `ad956e8e`, `05a5f763`, and `1a682648` resolve DES-001 without
  depending on the dropped Bunny example. Query and system callbacks can
  declare direct world-component access with typed `reads<T>()` and
  `writes<T>()`. Application-owned context uses a plain Gaia entity as a
  resource key passed to `reads(Entity)` or `writes(Entity)`, so systems sharing
  that key participate in the same scheduler conflict analysis. The focused
  regression uses two parallel systems sharing `ctx()`, verifies that the
  scheduler creates the writer-to-reader dependency, and passes in Debug and
  Release. The final documentation and regression commit adds no runtime state
  or work; all 9 CTest targets pass in both configurations, generated
  single-header configurations pass, and Doxygen builds successfully.
- Commit `23faeede` resolves DES-002 by loading CTest whenever unit tests or
  examples are requested. An examples-only configuration now exposes the
  standard `BUILD_TESTING` switch: it defaults to `ON` and registers the
  Roguelike smoke test, while `OFF` registers no example tests. The two
  examples-only Windows CI jobs now run CTest after building. Fresh Debug and
  Release examples-only builds compile every retained native example and pass
  the smoke test. The existing 9-target unit-test matrices pass in both
  configurations, workflow YAML parses successfully, and Doxygen builds.
- Commit `327d4214` moves the per-dispatch observer after-cache and membership
  delta buffers to `darray_ext`. Four distinct query-cache entries and sixteen
  entities per transition now stay inline, while larger dispatches retain heap
  growth. The optimized wildcard diff median remained flat at 769.921 ns per
  cycle versus 774.293 ns, while monitor dispatch improved from 1463.962 ns to
  1337.662 ns per cycle. Debug and Release pass all 9 CTest targets, the focused
  Address and Undefined sanitizer monitor test passes, generated single-header
  configurations pass, and Doxygen builds successfully.
- Relationship `add(Entity, Pair)`, `get(Entity)`, `acc` / `acc_mut`, and
  `mut_im` look up exact pair records on the unlikely path instead of indexing
  the relation endpoint that shares the numeric id. Regressions cover
  Entity-typed pair `child` / `get` / `acc_mut`, query
  `get<PositionSparse>(entity)` for pair owners, and command-buffer sparse
  add/set/delete on exact pair destinations.
- Commits `8dc1c617` and `6d6d382d` resolve DES-012. Entity-typed `enable` /
  `enabled` / `get_chunk` / `modify` / `add_n` / `copy` / `instantiate` /
  `sync` look up exact pair records through `cont()`. Copy and instantiate of a
  pair source create ordinary destinations; `add_n` copies type without table
  values. `World::add(entity, pair, value)` and `cb.add` / `cb.set` of
  `(entity, pair, value)` write the relationship payload on the owner.
  Regressions cover Entity-typed enable/modify/add_n, copy including
  `copy_ext_n`, add_n type copy, instantiate fallback including parented
  `instantiate_n` and command-buffer instantiate payloads, Prefab-tagged pairs
  dropping `Prefab` without `Pair(Is, pair)`, and delayed relationship
  payload add/set.
