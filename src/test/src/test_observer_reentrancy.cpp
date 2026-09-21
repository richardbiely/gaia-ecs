#include "test_common.h"

#define TestWorld SparseTestWorld

#if GAIA_OBSERVERS_ENABLED

namespace {

enum class ObserverTrace : uint32_t {
	TriggerEnter = 1,
	PairDelete = 2,
	PairAdd = 3,
	TriggerExit = 4,
};

void print_trace(const char* label, const cnt::darr<ObserverTrace>& trace) {
	std::printf("%s:", label);
	for (const auto item: trace)
		std::printf(" %u", static_cast<uint32_t>(item));
	std::printf("\n");
}

struct BucketMoveResult {
	cnt::darr<ObserverTrace> trace;
	bool hasTrueBucket = false;
	bool hasFalseBucket = false;
	uint32_t deleteHits = 0;
	uint32_t addHits = 0;
};

BucketMoveResult run_bucket_move(bool nonFragmenting) {
	TestWorld twld;

	const auto relationTrue = wld.add();
	const auto relationFalse = wld.add();
	const auto row = wld.add();
	const auto trigger = ecs::Pair(relationTrue, row);
	const auto result = ecs::Pair(relationFalse, row);

	if (nonFragmenting) {
		wld.add(relationTrue, ecs::Exclusive);
		wld.add(relationTrue, ecs::DontFragment);
		wld.add(relationFalse, ecs::Exclusive);
		wld.add(relationFalse, ecs::DontFragment);
	}

	wld.add(row, trigger);

	BucketMoveResult observed;
	observed.trace.reserve(4);
	bool inTrigger = false;
	bool moveRequested = false;
	uint32_t triggerHits = 0;
	uint32_t deleteHits = 0;
	uint32_t addHits = 0;
	const auto triggerObserver = wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.all<Position>()
			.on_each([&](ecs::Entity entity) {
				++triggerHits;
				if (moveRequested)
					return;
				moveRequested = true;
				observed.trace.push_back(ObserverTrace::TriggerEnter);
				inTrigger = true;
				wld.del(entity, trigger);
				wld.add(entity, result);
				inTrigger = false;
				observed.trace.push_back(ObserverTrace::TriggerExit);
			})
			.entity();
	const auto deleteObserver = wld.observer()
			.event(ecs::ObserverEvent::OnDel)
			.all(trigger)
			.on_each([&](ecs::Iter& it) {
				deleteHits += it.size();
				if (inTrigger)
					observed.trace.push_back(ObserverTrace::PairDelete);
			})
			.entity();
	const auto addObserver = wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.all(result)
			.on_each([&](ecs::Iter& it) {
				addHits += it.size();
				if (inTrigger)
					observed.trace.push_back(ObserverTrace::PairAdd);
			})
			.entity();
	(void)triggerObserver;
	(void)deleteObserver;
	(void)addObserver;

	wld.add<Position>(row);

	observed.hasTrueBucket = wld.has(row, trigger);
	observed.hasFalseBucket = wld.has(row, result);
	observed.deleteHits = deleteHits;
	observed.addHits = addHits;
	return observed;
}


//! Runs an UNGUARDED repeated bucket move under a hard cap. Every OnAdd callback for the trigger
//! component moves the row between two relation buckets, and each move re-enters observer dispatch
//! from inside the enclosing dispatch's own iteration.
//!
//! Extra OnDel/OnAdd pair observers are registered so the outer dispatch collects a multi-element
//! relevant-observer region. A nested dispatch appending to and truncating the shared scratch buffer
//! is what used to invalidate that region mid-iteration
//! (ObserverRegistry::m_relevant_observers_tmp, observer_registry.inl).
struct NestedDispatchResult {
	uint32_t moves = 0;
	uint32_t maxDepth = 0;
	uint32_t delHits = 0;
	uint32_t addHits = 0;
	uint32_t spectatorHits = 0;
	bool hasTrueBucket = false;
	bool hasFalseBucket = false;
	bool depthBalanced = false;
};

NestedDispatchResult run_nested_dispatch_storm(uint32_t moveCap, uint32_t spectatorCount) {
	TestWorld twld;

	const auto relationTrue = wld.add();
	const auto relationFalse = wld.add();
	const auto row = wld.add();
	const auto trigger = ecs::Pair(relationTrue, row);
	const auto result = ecs::Pair(relationFalse, row);

	wld.add(row, trigger);

	NestedDispatchResult observed;
	uint32_t depth = 0;

	// Spectator observers widen the outer dispatch's collected region so a nested collect_* has to
	// grow the shared scratch buffer past its current capacity.
	cnt::darr<ecs::Entity> spectators;
	for (uint32_t i = 0; i < spectatorCount; ++i) {
		spectators.push_back(wld.observer()
				.event(ecs::ObserverEvent::OnDel)
				.all(trigger)
				.on_each([&](ecs::Iter& it) { observed.spectatorHits += it.size(); })
				.entity());
		spectators.push_back(wld.observer()
				.event(ecs::ObserverEvent::OnAdd)
				.all(result)
				.on_each([&](ecs::Iter& it) { observed.spectatorHits += it.size(); })
				.entity());
	}

	const auto moverObserver = wld.observer()
			.event(ecs::ObserverEvent::OnDel)
			.all(trigger)
			.on_each([&](ecs::Iter& it) {
				observed.delHits += it.size();
				++depth;
				if (depth > observed.maxDepth)
					observed.maxDepth = depth;
				if (observed.moves < moveCap) {
					++observed.moves;
					// Re-enter dispatch from inside the enclosing dispatch's iteration.
					wld.add(row, result);
					wld.del(row, result);
					wld.add(row, trigger);
					wld.del(row, trigger);
				}
				--depth;
			})
			.entity();
	const auto addObserver = wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.all(result)
			.on_each([&](ecs::Iter& it) { observed.addHits += it.size(); })
			.entity();
	(void)moverObserver;
	(void)addObserver;

	wld.del(row, trigger);

	observed.hasTrueBucket = wld.has(row, trigger);
	observed.hasFalseBucket = wld.has(row, result);
	observed.depthBalanced = depth == 0;
	return observed;
}

} // namespace

TEST_CASE("Observer - nested component callback fires after the outer callback") {
	TestWorld twld;

	const auto entity = wld.add();
	bool outerActive = false;
	bool nestedWhileOuterActive = false;
	uint32_t outerHits = 0;
	uint32_t nestedHits = 0;

	const auto outerObserver = wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.all<Position>()
			.no<Acceleration>()
			.on_each([&](ecs::Entity observed) {
				++outerHits;
				outerActive = true;
				wld.add<Acceleration>(observed);
				outerActive = false;
			})
			.entity();
	const auto nestedObserver = wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.all<Acceleration>()
			.on_each([&](ecs::Entity) {
				++nestedHits;
				nestedWhileOuterActive |= outerActive;
			})
			.entity();
	(void)outerObserver;
	(void)nestedObserver;

	wld.add<Position>(entity);

	std::printf(
		"nested-component: outer=%u nested=%u synchronous=%u\n", outerHits, nestedHits,
		nestedWhileOuterActive ? 1U : 0U);
	// Amended from 3 by commit 3f1f9f3a ("fix(observers): isolate nested dispatch from the shared
	// scratch buffer"). The 3rd firing was never a firing: every dispatch iterated one registry-wide
	// scratch container, so the nested dispatch inherited this dispatch's leftover entries and read
	// the same ObserverRuntimeData* slot twice. Measured on the unfixed engine, the nested on_add
	// reported the identical pObs at two consecutive indices. That corruption is now pinned directly
	// by "Observer - nested dispatch does not invalidate the enclosing dispatch"; 3 must not be
	// restored here.
	//
	// 2 is the CURRENT value, not the correct one. This observer declares no<Acceleration>, and the
	// remaining 2nd firing is dispatched after the callback has added Acceleration - so it fires on
	// an entity its own filter excludes. That is a SEPARATE pre-existing defect in no<T> filtering on
	// the OnAdd direct-dispatch path, unrelated to the scratch buffer and present before 3f1f9f3a. It
	// reproduces with no reentrancy at all: add<Acceleration> then add<Position> at top level fires a
	// no<Acceleration> observer once. When that is fixed this expectation should become 1.
	CHECK(outerHits == 2);
	CHECK(nestedHits == 1);
	CHECK_FALSE(nestedWhileOuterActive);
	CHECK(wld.has<Acceleration>(entity));
}

//! Characterization only: this deliberately exercises the observed recursion; it is not an endorsement
//! of using an observer to mutate the term that triggers that same observer.
TEST_CASE("Observer - self-trigger reenters after one guarded mutation") {
	TestWorld twld;

	const auto entity = wld.add();
	constexpr uint32_t invocationCap = 1;
	uint32_t depth = 0;
	uint32_t maxDepth = 0;
	uint32_t hits = 0;
	bool actionTaken = false;

	const auto observer = wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.all<Position>()
			.on_each([&](ecs::Entity observed) {
				++hits;
				++depth;
				if (depth > maxDepth)
					maxDepth = depth;
				if (!actionTaken) {
					actionTaken = true;
					wld.del<Position>(observed);
					wld.add<Position>(observed);
				}
				--depth;
			})
			.entity();
	(void)observer;

	wld.add<Position>(entity);

	std::printf("self-trigger: invocations=%u max-depth=%u action-cap=%u\n", hits, maxDepth, invocationCap);
	// Amended from 3 by commit 3f1f9f3a ("fix(observers): isolate nested dispatch from the shared
	// scratch buffer"). 2 is what this setup can actually produce: one observer on OnAdd/Position and
	// exactly two add<Position> events - the initial one, plus the single in-callback re-add guarded
	// by actionTaken. The 3rd count came from the shared dispatch scratch container: the nested
	// dispatch inherited this dispatch's leftover entry and re-ran an observer that had already
	// fired. Measured on the unfixed engine, the 3rd invocation arrived at depth 2, the SAME depth as
	// the 2nd - a duplicated read of a stale buffer slot, not a re-entry. That corruption is now
	// pinned directly by "Observer - nested dispatch does not invalidate the enclosing dispatch"; 3
	// must not be restored here.
	CHECK(hits == 2);
	CHECK(maxDepth == 2);
	CHECK(actionTaken);
	CHECK(depth == 0);
	CHECK(wld.has<Position>(entity));
}

TEST_CASE("Observer - same-event callbacks follow registration order") {
	TestWorld twld;

	const auto entity = wld.add();
	cnt::darr<uint32_t> order;
	order.reserve(3);

	const auto observer1 = wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.all<Position>()
			.on_each([&](ecs::Entity) { order.push_back(1); })
			.entity();
	const auto observer2 = wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.all<Position>()
			.on_each([&](ecs::Entity) { order.push_back(2); })
			.entity();
	const auto observer3 = wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.all<Position>()
			.on_each([&](ecs::Entity) { order.push_back(3); })
			.entity();
	(void)observer1;
	(void)observer2;
	(void)observer3;

	wld.add<Position>(entity);

	std::printf("ordering:");
	for (const auto item: order)
		std::printf(" %u", item);
	std::printf("\n");
	CHECK(order.size() == 3);
	if (order.size() == 3) {
		CHECK(order[0] == 1);
		CHECK(order[1] == 2);
		CHECK(order[2] == 3);
	}
}

TEST_CASE("Observer - pair OnAdd and OnDel observe pair mutations") {
	TestWorld twld;

	const auto relation = wld.add();
	const auto target = wld.add();
	const auto source = wld.add();
	const auto pair = ecs::Pair(relation, target);
	uint32_t addHits = 0;
	uint32_t delHits = 0;

	const auto addObserver = wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.all(pair)
			.on_each([&](ecs::Iter& it) { addHits += it.size(); })
			.entity();
	const auto delObserver = wld.observer()
			.event(ecs::ObserverEvent::OnDel)
			.all(pair)
			.on_each([&](ecs::Iter& it) { delHits += it.size(); })
			.entity();
	(void)addObserver;
	(void)delObserver;

	wld.add(source, pair);
	wld.del(source, pair);

	std::printf("pair-observer: add=%u del=%u\n", addHits, delHits);
	CHECK(addHits == 1);
	CHECK(delHits == 1);
}

TEST_CASE("Observer - bucket move fires pair observers for fragmenting relation") {
	const auto observed = run_bucket_move(false);

	print_trace("bucket-move-fragmenting", observed.trace);
	CHECK(observed.trace.size() == 4);
	if (observed.trace.size() == 4) {
		CHECK(observed.trace[0] == ObserverTrace::TriggerEnter);
		CHECK(observed.trace[1] == ObserverTrace::PairDelete);
		CHECK(observed.trace[2] == ObserverTrace::PairAdd);
		CHECK(observed.trace[3] == ObserverTrace::TriggerExit);
	}
	CHECK(observed.deleteHits == 1);
	CHECK(observed.addHits == 1);
	CHECK_FALSE(observed.hasTrueBucket);
	CHECK(observed.hasFalseBucket);
}

TEST_CASE("Observer - bucket move fires pair observers for non-fragmenting relation") {
	const auto observed = run_bucket_move(true);

	print_trace("bucket-move-non-fragmenting", observed.trace);
	CHECK(observed.trace.size() == 4);
	if (observed.trace.size() == 4) {
		CHECK(observed.trace[0] == ObserverTrace::TriggerEnter);
		CHECK(observed.trace[1] == ObserverTrace::PairDelete);
		CHECK(observed.trace[2] == ObserverTrace::PairAdd);
		CHECK(observed.trace[3] == ObserverTrace::TriggerExit);
	}
	CHECK(observed.deleteHits == 1);
	CHECK(observed.addHits == 1);
	CHECK_FALSE(observed.hasTrueBucket);
	CHECK(observed.hasFalseBucket);
}

//! Regression: nested observer dispatch must not invalidate an enclosing dispatch's iteration.
//!
//! Before the fix, every dispatch iterated one registry-wide scratch container
//! (ObserverRegistry::m_relevant_observers_tmp). A callback that re-entered dispatch made the nested
//! collect_* push into - and the nested dispatch clear() - that same container while the outer
//! range-for still held pointers into its buffer. ASan reported this at observer_registry.inl:523 as
//! container-overflow and, with that check disabled, heap-use-after-free.
//!
//! This test drives deep nested dispatch with a multi-element outer region so that a nested collect
//! is forced to grow the shared buffer. It asserts termination and a balanced depth counter; the real
//! assertion is that it runs clean under -fsanitize=address,undefined.
TEST_CASE("Observer - nested dispatch does not invalidate the enclosing dispatch") {
	constexpr uint32_t moveCap = 32;

	// Several spectator counts: each changes how many entries the outer region holds, and therefore
	// where the nested push_back lands relative to the buffer's capacity.
	for (uint32_t spectatorCount: {0U, 1U, 4U, 16U}) {
		const auto observed = run_nested_dispatch_storm(moveCap, spectatorCount);

		std::printf(
			"nested-dispatch-storm: spectators=%u moves=%u max-depth=%u del=%u add=%u spectator=%u\n",
			spectatorCount, observed.moves, observed.maxDepth, observed.delHits, observed.addHits,
			observed.spectatorHits);

		// Terminated with a defined result rather than crashing or running away.
		CHECK(observed.moves == moveCap);
		CHECK(observed.depthBalanced);
		// Nesting actually happened - otherwise this test would not be exercising the bug at all.
		CHECK(observed.maxDepth == moveCap + 1);

		// The discriminating assertions. Each dispatch must run each matching observer EXACTLY once.
		// There is one initial del plus one del per move, so the mover sees exactly moveCap + 1 events.
		// Before the fix a nested dispatch inherited the enclosing dispatch's leftover scratch entries
		// and re-ran observers that had already fired, which inflated these counts
		// (measured on the unfixed engine: delHits == 65 and, at spectatorCount == 16,
		// spectatorHits == 1552, against the 33 / 1040 asserted here).
		CHECK(observed.delHits == moveCap + 1);
		CHECK(observed.addHits == moveCap);
		// Each spectator pair sees every del event and every add event exactly once.
		CHECK(observed.spectatorHits == spectatorCount * (2 * moveCap + 1));

		// Both buckets were released by the final unwinding.
		CHECK_FALSE(observed.hasTrueBucket);
		CHECK_FALSE(observed.hasFalseBucket);
	}
}

//! Regression: the reproducer from docs/observer-reentrancy-diagnosis.md.
//!
//! An UNGUARDED repeated bucket move under a hard cap, with exact-pair OnDel/OnAdd observers
//! registered so the outer dispatch collects a multi-element relevant-observer region.
//!
//! Measured on the unfixed engine, this aborts under ASan with
//!   container-overflow ... observer_registry.inl:523
//!   #0 ObserverRegistry::DirectDispatcher::on_del  observer_registry.inl:523
//!   #1 ObserverRegistry::on_del                    observer_registry.inl:1231
//!   #2 World::EntityBuilder::trigger_del_hooks     world.h:2041
//!   #3 World::EntityBuilder::commit                world.h:1663
//!   #5 World::del(Entity, Pair)                    world.h:5441
//! and exits 139 (SIGSEGV) in Release. With the dispatch scope in place it reaches the cap and
//! terminates with a defined result.
TEST_CASE("Observer - unguarded bucket move has a hard diagnostic cap") {
	TestWorld twld;

	const auto relationTrue = wld.add();
	const auto relationFalse = wld.add();
	const auto row = wld.add();
	const auto trigger = ecs::Pair(relationTrue, row);
	const auto result = ecs::Pair(relationFalse, row);

	wld.add(row, trigger);

	constexpr uint32_t cap = 64;
	uint32_t moves = 0;
	uint32_t delHits = 0;
	uint32_t addHits = 0;

	const auto triggerObserver = wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.all<Position>()
			.on_each([&](ecs::Entity entity) {
				// UNGUARDED: every invocation moves buckets again, up to the hard cap.
				if (moves >= cap)
					return;
				++moves;
				// The diagnosis' shape: move the row between buckets and move it back, so the OnAdd
				// Position observer keeps re-firing rather than settling after one move.
				wld.del(entity, trigger);
				wld.add(entity, result);
				wld.del<Position>(entity);
				wld.add<Position>(entity);
				wld.del(entity, result);
				wld.add(entity, trigger);
			})
			.entity();
	const auto deleteObserver = wld.observer()
			.event(ecs::ObserverEvent::OnDel)
			.all(trigger)
			.on_each([&](ecs::Iter& it) { delHits += it.size(); })
			.entity();
	const auto addObserver = wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.all(result)
			.on_each([&](ecs::Iter& it) { addHits += it.size(); })
			.entity();
	(void)triggerObserver;
	(void)deleteObserver;
	(void)addObserver;

	wld.add<Position>(row);

	std::printf("phase1-repro: moves=%u cap=%u del=%u add=%u trueBucket=%d\n", moves, cap, delHits, addHits, (int)wld.has(row, trigger));
	CHECK(moves <= cap);
}

#endif
